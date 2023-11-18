#include "esp_err.h"
#include "esp_check.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "app.h"


void app_update_stats(const app_stats_t *stats)
{
    app_stats_t old_stats;
    app_get_stats(&old_stats);

    old_stats.success_count += stats->success_count;
    old_stats.fail_count += stats->fail_count;
    old_stats.awake_time_ms += stats->awake_time_ms;
    old_stats.connecting_time_ms += stats->connecting_time_ms;
    old_stats.display_on_time_ms += stats->display_on_time_ms;

    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("app_stats", NVS_READWRITE, &nvs_handle));

    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "success", old_stats.success_count));
    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "fail", old_stats.fail_count));
    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "awake", old_stats.awake_time_ms));
    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "connecting", old_stats.connecting_time_ms));
    ESP_ERROR_CHECK(nvs_set_u32(nvs_handle, "display", old_stats.display_on_time_ms));

    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}

void app_get_stats(app_stats_t *stats)
{
    nvs_handle_t nvs_handle;
    *stats = (app_stats_t) {
        0
    };
    ESP_ERROR_CHECK(nvs_open("app_stats", NVS_READWRITE, &nvs_handle));
    esp_err_t err;

    err = nvs_get_u32(nvs_handle, "success", (uint32_t *) &stats->success_count);
    if (err != ESP_OK) {
        printf("read success_count failed: 0x%x\n", err);
    }

    err = nvs_get_u32(nvs_handle, "fail", (uint32_t *) &stats->fail_count);
    if (err != ESP_OK) {
        printf("read fail_count failed: 0x%x\n", err);
    }

    err = nvs_get_u32(nvs_handle, "awake", (uint32_t *) &stats->awake_time_ms);
    if (err != ESP_OK) {
        printf("read awake_time_ms failed: 0x%x\n", err);
    }

    err = nvs_get_u32(nvs_handle, "connecting", (uint32_t *) &stats->connecting_time_ms);
    if (err != ESP_OK) {
        printf("read connecting_time_ms failed: 0x%x\n", err);
    }

    err = nvs_get_u32(nvs_handle, "display", (uint32_t *) &stats->display_on_time_ms);
    if (err != ESP_OK) {
        printf("read display_on_time_ms failed: 0x%x\n", err);
    }

    nvs_close(nvs_handle);
}
