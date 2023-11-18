#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_check.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "download_file.h"
#include "nvs_dotenv.h"
#include "sdkconfig.h"
#include "app.h"
#include "esp_timer.h"

static esp_err_t set_headers(void *user_data, esp_http_client_handle_t client);
static void power_off(void);

static const char *TAG = "main";

void app_main()
{
    esp_err_t ret;
    int64_t end;
    int64_t connect_start = 0;
    int64_t connect_end = 0;

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set("wifi", ESP_LOG_NONE);
    esp_log_level_set("wifi_init", ESP_LOG_NONE);
    esp_log_level_set("phy_init", ESP_LOG_NONE);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set("connect", ESP_LOG_INFO);
    app_display_init_log();

    // Initialize NVS and networking
    ESP_GOTO_ON_ERROR(esp_netif_init(), end, TAG, "Failed to init netif");
    ESP_GOTO_ON_ERROR(esp_event_loop_create_default(), end, TAG, "Failed to create event loop");

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_GOTO_ON_ERROR(nvs_flash_erase(), end, TAG, "Failed to erase NVS");
        ret = nvs_flash_init();
    }
    ESP_GOTO_ON_ERROR(ret, end, TAG, "Failed to init NVS (2)");

    ESP_GOTO_ON_ERROR(nvs_dotenv_load(), end, TAG, "Failed to init nvs-dotenv");

    connect_start = esp_timer_get_time();
    connect_end = connect_start;
    ESP_GOTO_ON_ERROR(app_wifi_connect_start(), end, TAG, "Failed to start WiFi connection");

    // Initialize the screen
    app_display_init();

    app_stats_t old_stats;
    app_get_stats(&old_stats);
    ESP_LOGI(TAG, "S: %d F: %d Act: %ds Conn: %ds",
             old_stats.success_count,
             old_stats.fail_count,
             old_stats.awake_time_ms / 1000,
             old_stats.connecting_time_ms / 1000);

    // Wait for WiFi connection
    ESP_GOTO_ON_ERROR(app_wifi_wait_for_connection(), end, TAG, "Failed to connect to WiFi");
    connect_end = esp_timer_get_time();

    // Download and display the PNG
    ESP_LOGI(TAG, "Downloading...");
    char *png_buf;
    size_t png_len;
    FILE *f = open_memstream(&png_buf, &png_len);
    download_file_config_t download_config = DOWNLOAD_FILE_CONFIG_DEFAULT();
    download_config.http_client_post_init_cb = &set_headers;
    const char *png_url = getenv("PNG_URL");
    ESP_GOTO_ON_FALSE(png_url != NULL, ESP_ERR_NOT_FOUND, end, TAG, "PNG_URL not set in .env");
    ESP_GOTO_ON_ERROR(download_file(png_url, f, &download_config), end, TAG, "Failed to download file");
    fflush(f);
    app_wifi_stop();

    ESP_LOGI(TAG, "Rendering...");
    app_display_png((const uint8_t *) png_buf, png_len);

end:
    end = esp_timer_get_time();
    app_stats_t stats = {
        .success_count = ret == ESP_OK,
        .fail_count = ret != ESP_OK,
        .connecting_time_ms = (connect_end - connect_start) / 1000,
        .awake_time_ms = end / 1000,

    };
    ESP_LOGI(TAG, "S%d/F%d A%ds C%ds D%ds\n",
             stats.success_count,
             stats.fail_count,
             stats.awake_time_ms / 1000,
             stats.connecting_time_ms / 1000,
             stats.display_on_time_ms / 1000);
    app_update_stats(&stats);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error: %s", esp_err_to_name(ret));
        app_display_show_log();
    }
    power_off();
}

static esp_err_t set_headers(void *user_data, esp_http_client_handle_t http_client)
{
    const char *headers = getenv("HTTP_HEADERS");
    if (headers == NULL || strlen(headers) == 0) {
        ESP_LOGI(TAG, "HTTP_HEADERS not set in .env");
        return ESP_OK;
    }
    esp_err_t ret = ESP_OK;

    // HTTP_HEADERS variable has the format "key1=value1;key2=value2"
    char *headers_copy = strdup(headers);
    ESP_RETURN_ON_FALSE(headers_copy != NULL, ESP_ERR_NO_MEM, TAG, "Failed to copy headers");

    char *pair_token;
    char *pair_ptr = headers_copy;
    while ((pair_token = strsep(&pair_ptr, ";")) != NULL) {
        char *equal_pos = strchr(pair_token, '=');
        ESP_GOTO_ON_FALSE(equal_pos != NULL, ESP_ERR_INVALID_ARG, out, TAG, "Invalid HTTP_HEADERS variable format, missing = sing");
        *equal_pos = '\0';
        const char *key = pair_token;
        const char *value = equal_pos + 1;
        ESP_GOTO_ON_ERROR(esp_http_client_set_header(http_client, key, value), out, TAG, "Failed to set header");
    }

out:
    free(headers_copy);
    return ret;
}

static void power_off(void)
{
    app_display_poweroff();

    int refresh_interval_min = 30;
    const char *refresh_interval_min_str = getenv("REFRESH_INTERVAL_MIN");
    if (refresh_interval_min_str != NULL) {
        refresh_interval_min = atoi(refresh_interval_min_str);
    }
    uint64_t wakeup_interval_us = refresh_interval_min * (60ULL * 1000 * 1000);
    esp_sleep_enable_timer_wakeup(wakeup_interval_us);
    esp_deep_sleep_start();
}
