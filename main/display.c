#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_check.h"

#include "png.h"
#include "epd_driver.h"
#include "epd_highlevel.h"
#include "epd_board.h"
#include "app.h"

#ifndef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbidi-chars"
#endif // __clang__
#include "firasans_20.h"
#ifndef __clang__
#pragma GCC diagnostic pop
#endif // __clang__


static void init_epd(void);
static void png_to_gray(uint8_t *buf, size_t buf_len, uint8_t **output, size_t *output_len);
static int app_display_vprintf(const char *fmt, va_list args);

static const char *TAG = "display";
static EpdiyHighlevelState s_hl;
static int s_temperature;
static FILE *s_log_file;
static char *s_log_str;
static size_t s_log_str_len;

esp_err_t app_display_init(void)
{
    init_epd();
    return ESP_OK;
}

void app_display_init_log(void)
{
    s_log_file = open_memstream(&s_log_str, &s_log_str_len);
    esp_log_set_vprintf(app_display_vprintf);
}

esp_err_t app_display_png(const uint8_t *png_data, size_t png_len)
{
    uint8_t *gray_buf;
    size_t gray_len;
    png_to_gray((uint8_t *) png_data, png_len, &gray_buf, &gray_len);
    if (gray_buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    int width = epd_rotated_display_width();
    int height = epd_rotated_display_height();
    uint8_t *fb = epd_hl_get_framebuffer(&s_hl);

    epd_hl_set_all_white(&s_hl);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t pixel = gray_buf[y * width + x];
            epd_draw_pixel(x, y, pixel, fb);
        }
    }
    epd_poweron();
    epd_clear();
    epd_hl_update_screen(&s_hl, MODE_GC16, s_temperature);
    epd_poweroff();
    free(gray_buf);
    return ESP_OK;
}

void app_display_poweroff(void)
{
    epd_poweroff();
}

static void init_epd(void)
{
    epd_init(EPD_LUT_1K);
    s_hl = epd_hl_init(EPD_BUILTIN_WAVEFORM);
    epd_set_rotation(EPD_ROT_LANDSCAPE);
}

static int app_display_vprintf(const char *fmt, va_list args)
{
    // print to console
    int res = vprintf(fmt, args);

    // print to log file
    vfprintf(s_log_file, fmt, args);

    return res;
}

void app_display_show_log(void)
{
    uint8_t *fb = epd_hl_get_framebuffer(&s_hl);
    int width = epd_rotated_display_width();
    int height = epd_rotated_display_height();
    fflush(s_log_file);
    epd_poweron();
    epd_clear();
    epd_hl_set_all_white(&s_hl);
    EpdRect border_rect = {
        .x = 20,
        .y = 20,
        .width = width - 40,
        .height = height - 40
    };
    epd_draw_rect(border_rect, 0, fb);

    // Print lines from s_log_str to screen, using strsep
    char *line = s_log_str;
    char *next_line = NULL;
    int last_y = 60;
    while ((next_line = strsep(&line, "\n")) != NULL) {
        if (strlen(next_line) == 0) {
            continue;
        }
        int cursor_x = 50;
        int cursor_y = last_y;
        epd_write_default(&FiraSans_20,
                          next_line,
                          &cursor_x, &cursor_y, fb);
        last_y = cursor_y;
        if (cursor_y >= height - 50) {
            break;
        }
    }


    epd_hl_update_screen(&s_hl, MODE_GC16, s_temperature);
    epd_poweroff();
}

static void png_to_gray(uint8_t *buf, size_t buf_len, uint8_t **output, size_t *output_len)
{
    png_image image;
    memset(&image, 0, (sizeof image));
    image.version = PNG_IMAGE_VERSION;

    if (png_image_begin_read_from_memory(&image, buf, buf_len)) {
        png_bytep buffer;
        image.format = PNG_FORMAT_GRAY;
        int stride = PNG_IMAGE_ROW_STRIDE(image);
        int buf_size = PNG_IMAGE_SIZE(image);

        ESP_LOGD(TAG, "PNG size: %dx%d buf_size=%d stride=%d", image.width, image.height, buf_size, stride);
        buffer = malloc(buf_size);

        if (buffer != NULL &&
                png_image_finish_read(&image, NULL, buffer, stride, NULL)) {
            *output = buffer;
            *output_len = PNG_IMAGE_SIZE(image);
        } else {
            if (buffer == NULL) {
                png_image_free(&image);
            } else {
                free(buffer);
            }
            *output = NULL;
            *output_len = 0;
        }
    }
}
