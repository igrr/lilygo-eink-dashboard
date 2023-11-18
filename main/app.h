#pragma once

#include <stdio.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_display_init(void);
int app_display_vprintf(const char *format, va_list args);
esp_err_t app_display_png(const uint8_t *png_data, size_t png_len);
void app_display_poweroff(void);

esp_err_t app_wifi_connect_start(void);
esp_err_t app_wifi_wait_for_connection(void);
void app_wifi_stop();

typedef struct {
    unsigned success_count;
    unsigned fail_count;
    unsigned awake_time_ms;
    unsigned connecting_time_ms;
    unsigned display_on_time_ms;
} app_stats_t;

void app_update_stats(const app_stats_t *stats);
void app_get_stats(app_stats_t *stats);

#ifdef __cplusplus
}
#endif
