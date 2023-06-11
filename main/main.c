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

static esp_err_t set_headers(void *user_data, esp_http_client_handle_t client);
static void power_off(void);

static const char *TAG = "main";

void app_main()
{
    esp_err_t ret;

    // Initialize the screen so that we can output logs there
    app_display_init();
    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set("connect", ESP_LOG_INFO);
    esp_log_set_vprintf(app_display_vprintf);

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

    ESP_GOTO_ON_ERROR(app_wifi_connect(), end, TAG, "Failed to connect to WiFi");

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

    ESP_LOGI(TAG, "Rendering...");
    app_display_png((const uint8_t *) png_buf, png_len);

end:
    power_off();
}

static esp_err_t set_headers(void *user_data, esp_http_client_handle_t http_client)
{
    const char *client_id = getenv("CLIENT_ID");
    ESP_RETURN_ON_FALSE(client_id != NULL, ESP_ERR_NOT_FOUND, TAG, "CLIENT_ID not set in .env");
    ESP_RETURN_ON_ERROR(esp_http_client_set_header(http_client, "CF-Access-Client-Id", client_id), TAG, "Failed to set header");

    const char *client_secret = getenv("CLIENT_SECRET");
    ESP_RETURN_ON_FALSE(client_secret != NULL, ESP_ERR_NOT_FOUND, TAG, "CLIENT_SECRET not set in .env");
    ESP_RETURN_ON_ERROR(esp_http_client_set_header(http_client, "CF-Access-Client-Secret", client_secret), TAG, "Failed to set header");

    return ESP_OK;
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
