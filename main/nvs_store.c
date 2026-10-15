#include "nvs_store.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"

static const char *TAG = "nvs_store";
static const char *NVS_NAMESPACE = "bessie_cfg";
static const char *NVS_KEY_STATE = "led_state";

static TimerHandle_t s_save_timer = NULL;

static void save_timer_cb(TimerHandle_t xTimer) {
    led_state_t state;
    led_controller_get_state(&state);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, NVS_KEY_STATE, &state, sizeof(led_state_t));
        if (err == ESP_OK) {
            nvs_commit(handle);
            ESP_LOGI(TAG, "Saved state to NVS (pattern=%d, brightness=%d, speed=%.2f, pwr=%d)",
                     state.pattern_idx, state.master_brightness, state.speed, state.power_on);
        } else {
            ESP_LOGE(TAG, "Failed to write state blob: %s", esp_err_to_name(err));
        }
        nvs_close(handle);
    } else {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(err));
    }
}

esp_err_t nvs_store_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "Erasing and re-initializing NVS flash");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_save_timer = xTimerCreate("nvs_save", pdMS_TO_TICKS(1500), pdFALSE, NULL, save_timer_cb);
    if (!s_save_timer) {
        ESP_LOGE(TAG, "Failed to create save timer");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t nvs_store_load_state(led_state_t *state) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return err;

    size_t required_size = sizeof(led_state_t);
    err = nvs_get_blob(handle, NVS_KEY_STATE, state, &required_size);
    nvs_close(handle);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Loaded saved state from NVS (pattern=%d, brightness=%d, speed=%.2f)",
                 state->pattern_idx, state->master_brightness, state->speed);
    }
    return err;
}

void nvs_store_schedule_save(void) {
    if (s_save_timer) {
        // Reset 1.5s countdown on every adjustment (debounce)
        xTimerReset(s_save_timer, 0);
    }
}
