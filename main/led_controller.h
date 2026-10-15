#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "patterns.h"

// GPIO pins for ESP32-C6
#define LED_PIN_CH0  0
#define LED_PIN_CH1  1
#define LED_PIN_CH2  2
#define LED_PIN_CH3  3

typedef struct {
    int pattern_idx;
    uint8_t master_brightness; // 0 ~ 100 (%)
    float speed;               // 0.2 ~ 3.0
    uint8_t manual_ch[LED_NUM_CHANNELS]; // 0 ~ 100 (%)
    bool power_on;
} led_state_t;

// Initialize LEDC PWM and background animation task
esp_err_t led_controller_init(void);

// State accessors
void led_controller_get_state(led_state_t *state);
void led_controller_set_state(const led_state_t *state);

// Convenience setters
void led_controller_set_pattern_by_id(const char *id);
void led_controller_set_pattern_idx(int idx);
void led_controller_set_brightness(uint8_t brightness);
void led_controller_set_speed(float speed);
void led_controller_set_manual_ch(int ch, uint8_t val);
void led_controller_set_power(bool power);

// Get current live normalized brightness (0~100) for preview
void led_controller_get_live_levels(uint8_t levels[LED_NUM_CHANNELS]);
