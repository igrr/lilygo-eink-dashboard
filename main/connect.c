#include <stdlib.h>
#include <string.h>
#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_netif_defaults.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app.h"


static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);


#define TASK_NOTIFY_BIT_WIFI_CONNECTED (1 << 0)
#define TASK_NOTIFY_BIT_WIFI_FAIL (1 << 1)
#define TASK_NOTIFY_BITS (TASK_NOTIFY_BIT_WIFI_CONNECTED | TASK_NOTIFY_BIT_WIFI_FAIL)
#define CONNECT_RETRY_COUNT 3

static TaskHandle_t s_connecting_task;
static const char *TAG = "connect";
static int s_retry_num = 0;
static esp_event_handler_instance_t s_instance_any_id;
static esp_event_handler_instance_t s_instance_got_ip;


esp_err_t app_wifi_connect_start(void)
{
    s_connecting_task = xTaskGetCurrentTaskHandle();
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                    ESP_EVENT_ANY_ID,
                    &event_handler,
                    NULL,
                    &s_instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                    IP_EVENT_STA_GOT_IP,
                    &event_handler,
                    NULL,
                    &s_instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    const char *wifi_ssid = getenv("WIFI_SSID");
    ESP_RETURN_ON_FALSE(wifi_ssid != NULL, ESP_ERR_NOT_FOUND, TAG, "WIFI_SSID not set in .env");
    strcpy((char *)wifi_config.sta.ssid, wifi_ssid);

    const char *wifi_password = getenv("WIFI_PASSWORD");
    ESP_RETURN_ON_FALSE(wifi_password != NULL, ESP_ERR_NOT_FOUND, TAG, "WIFI_PASSWORD not set in .env");
    strcpy((char *)wifi_config.sta.password, wifi_password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to %s...", wifi_ssid);
    return ESP_OK;
}

esp_err_t app_wifi_wait_for_connection(void)
{
    const char *connect_timeout_str = getenv("CONNECT_TIMEOUT");
    int connect_timeout_sec = 10;
    if (connect_timeout_str != NULL) {
        connect_timeout_sec = atoi(connect_timeout_str);
    }

    uint32_t notify_value;
    xTaskNotifyWait(0, TASK_NOTIFY_BITS, &notify_value, pdMS_TO_TICKS(connect_timeout_sec * 1000));

    if (notify_value & TASK_NOTIFY_BIT_WIFI_CONNECTED) {
        return ESP_OK;
    } else if (notify_value & TASK_NOTIFY_BIT_WIFI_FAIL) {
        return ESP_FAIL;
    }
    return ESP_ERR_TIMEOUT;
}

void app_wifi_stop()
{
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_instance_got_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_instance_any_id);
    esp_wifi_stop();
    esp_wifi_deinit();
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        int reason = ((wifi_event_sta_disconnected_t *)event_data)->reason;
        ESP_LOGI(TAG, "WiFi disconnected, reason: %d", reason);
        if (s_retry_num < CONNECT_RETRY_COUNT) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying to connect to the AP...");
        } else {
            xTaskNotify(s_connecting_task, TASK_NOTIFY_BIT_WIFI_FAIL, eSetBits);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        s_retry_num = 0;
        xTaskNotify(s_connecting_task, TASK_NOTIFY_BIT_WIFI_CONNECTED, eSetBits);
    }
}
