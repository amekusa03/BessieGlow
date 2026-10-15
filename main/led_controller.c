#include "led_controller.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "led_ctrl";

static const int LED_PINS[LED_NUM_CHANNELS] = {
    LED_PIN_CH0,
    LED_PIN_CH1,
    LED_PIN_CH2,
    LED_PIN_CH3
};

static const ledc_channel_t LEDC_CHANNELS[LED_NUM_CHANNELS] = {
    LEDC_CHANNEL_0,
    LEDC_CHANNEL_1,
    LEDC_CHANNEL_2,
    LEDC_CHANNEL_3
};

static led_state_t s_state = {
    .pattern_idx = 0,
    .master_brightness = 100,
    .speed = 1.0f,
    .manual_ch = {100, 100, 100, 100},
    .power_on = true,
};

static uint8_t s_live_levels[LED_NUM_CHANNELS] = {0};
static SemaphoreHandle_t s_mutex = NULL;
static TaskHandle_t s_anim_task_handle = NULL;

static void anim_task(void *pvParameters) {
    uint32_t frame = 0;
    uint16_t raw_duty[LED_NUM_CHANNELS];
    led_state_t local_state;

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(20); // 50 Hz update rate

    while (1) {
        // Fetch current state under lock
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        local_state = s_state;
        xSemaphoreGive(s_mutex);

        if (!local_state.power_on) {
            for (int i = 0; i < LED_NUM_CHANNELS; i++) {
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNELS[i], 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNELS[i]);
                s_live_levels[i] = 0;
            }
            vTaskDelayUntil(&last_wake, period);
            continue;
        }

        // Determine if manual mode
        const pattern_def_t *cur_pattern = &PATTERN_LIST[local_state.pattern_idx];
        bool is_manual = (strcmp(cur_pattern->id, "manual") == 0);

        if (is_manual) {
            for (int i = 0; i < LED_NUM_CHANNELS; i++) {
                raw_duty[i] = (uint16_t)(((uint32_t)local_state.manual_ch[i] * PWM_MAX_DUTY) / 100);
            }
        } else {
            cur_pattern->func(frame, local_state.speed, raw_duty);
        }

        // Apply Master Brightness & Update Hardware
        float master_scale = (float)local_state.master_brightness / 100.0f;
        for (int i = 0; i < LED_NUM_CHANNELS; i++) {
            uint32_t final_duty = (uint32_t)((float)raw_duty[i] * master_scale);
            if (final_duty > PWM_MAX_DUTY) final_duty = PWM_MAX_DUTY;

            ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNELS[i], final_duty);
            ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNELS[i]);

            s_live_levels[i] = (uint8_t)((final_duty * 100) / PWM_MAX_DUTY);
        }

        frame++;
        vTaskDelayUntil(&last_wake, period);
    }
}

esp_err_t led_controller_init(void) {
    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;

    // LEDC Timer configuration
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_12_BIT, // 0 ~ 4095
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,                      // 5 kHz flicker-free
        .clk_cfg = LEDC_AUTO_CLK
    };
    esp_err_t err = ledc_timer_config(&timer_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(err));
        return err;
    }

    // Configure each channel
    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        ledc_channel_config_t ch_conf = {
            .gpio_num = LED_PINS[i],
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = LEDC_CHANNELS[i],
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0
        };
        err = ledc_channel_config(&ch_conf);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LEDC channel %d config failed: %s", i, esp_err_to_name(err));
            return err;
        }
    }

    // Start background task
    BaseType_t ret = xTaskCreate(anim_task, "led_anim", 4096, NULL, 5, &s_anim_task_handle);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create animation task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "LED Controller initialized successfully (GPIO 0, 1, 2, 3)");
    return ESP_OK;
}

void led_controller_get_state(led_state_t *state) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *state = s_state;
    xSemaphoreGive(s_mutex);
}

void led_controller_set_state(const led_state_t *state) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state = *state;
    if (s_state.pattern_idx < 0) s_state.pattern_idx = 0;
    if (s_state.pattern_idx >= PATTERN_COUNT) s_state.pattern_idx = PATTERN_COUNT - 1;
    if (s_state.master_brightness > 100) s_state.master_brightness = 100;
    if (s_state.speed < 0.2f) s_state.speed = 0.2f;
    if (s_state.speed > 3.0f) s_state.speed = 3.0f;
    xSemaphoreGive(s_mutex);
}

void led_controller_set_pattern_by_id(const char *id) {
    for (int i = 0; i < PATTERN_COUNT; i++) {
        if (strcmp(PATTERN_LIST[i].id, id) == 0) {
            led_controller_set_pattern_idx(i);
            return;
        }
    }
}

void led_controller_set_pattern_idx(int idx) {
    if (idx >= 0 && idx < PATTERN_COUNT) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_state.pattern_idx = idx;
        xSemaphoreGive(s_mutex);
    }
}

void led_controller_set_brightness(uint8_t brightness) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.master_brightness = brightness > 100 ? 100 : brightness;
    xSemaphoreGive(s_mutex);
}

void led_controller_set_speed(float speed) {
    if (speed < 0.2f) speed = 0.2f;
    if (speed > 3.0f) speed = 3.0f;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.speed = speed;
    xSemaphoreGive(s_mutex);
}

void led_controller_set_manual_ch(int ch, uint8_t val) {
    if (ch >= 0 && ch < LED_NUM_CHANNELS) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_state.manual_ch[ch] = val > 100 ? 100 : val;
        xSemaphoreGive(s_mutex);
    }
}

void led_controller_set_power(bool power) {
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.power_on = power;
    xSemaphoreGive(s_mutex);
}

void led_controller_get_live_levels(uint8_t levels[LED_NUM_CHANNELS]) {
    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        levels[i] = s_live_levels[i];
    }
}
