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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbidi-chars"
#include "firasans_12.h"
#include "firasans_20.h"
#pragma GCC diagnostic pop

static void init_epd(void);
static void png_to_gray_4bit(uint8_t *buf, size_t buf_len, uint8_t **output, size_t *output_len);

static const char *TAG = "display";
static EpdiyHighlevelState s_hl;
static int s_temperature;
static int s_fb_log_y = 60;
static bool s_logging_to_screen;

esp_err_t app_display_init(void)
{
    init_epd();
    return ESP_OK;
}

esp_err_t app_display_png(const uint8_t *png_data, size_t png_len)
{
    uint8_t *gray_buf;
    size_t gray_len;
    png_to_gray_4bit((uint8_t *) png_data, png_len, &gray_buf, &gray_len);
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

    epd_poweron();
    epd_clear();
    s_temperature = epd_ambient_temperature();

    uint8_t *fb = epd_hl_get_framebuffer(&s_hl);
    int width = epd_rotated_display_width();
    int height = epd_rotated_display_height();

    epd_hl_set_all_white(&s_hl);
    EpdRect border_rect = {
        .x = 20,
        .y = 20,
        .width = width - 40,
        .height = height - 40
    };
    epd_draw_rect(border_rect, 0, fb);
    epd_hl_update_screen(&s_hl, MODE_GC16, s_temperature);
    epd_poweroff();
}

int app_display_vprintf(const char *fmt, va_list args)
{
    // print to console
    int res = vprintf(fmt, args);

    if (s_logging_to_screen) {
        return res;
    }

    s_logging_to_screen = true;

    // print to screen
    char buf[128];
    vsnprintf(buf, sizeof(buf), fmt, args);

    uint8_t *fb = epd_hl_get_framebuffer(&s_hl);
    int height = epd_rotated_display_height();
    if (s_fb_log_y >= height - 50) {
        s_fb_log_y = 60;
        epd_hl_set_all_white(&s_hl);
    }

    int cursor_x = 50;
    int cursor_y = s_fb_log_y;


    epd_write_default(&FiraSans_20,
                      buf,
                      &cursor_x, &cursor_y, fb);

    s_fb_log_y = cursor_y;

    epd_poweron();
    epd_hl_update_screen(&s_hl, MODE_GC16, s_temperature);
    epd_poweroff();

    s_logging_to_screen = false;
    return res;
}

static void png_to_gray_4bit(uint8_t *buf, size_t buf_len, uint8_t **output, size_t *output_len)
{
    png_image image;
    memset(&image, 0, (sizeof image));
    image.version = PNG_IMAGE_VERSION;

    if (png_image_begin_read_from_memory(&image, buf, buf_len)) {
        png_bytep buffer;
        ESP_LOGI(TAG, "PNG size: %dx%d", image.width, image.height);
        image.format = PNG_FORMAT_GRAY;
        buffer = malloc(PNG_IMAGE_SIZE(image));

        if (buffer != NULL &&
                png_image_finish_read(&image, NULL, buffer, 0, NULL)) {
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
