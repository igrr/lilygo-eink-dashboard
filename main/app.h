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

esp_err_t app_wifi_connect(void);

#ifdef __cplusplus
}
#endif
