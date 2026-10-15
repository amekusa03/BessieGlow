#pragma once

#include <stdint.h>
#include <stdbool.h>

#define LED_NUM_CHANNELS 4
#define PWM_MAX_DUTY     4095

// Function pointer for pattern animation
// frame: monotonically increasing counter (in pattern task ticks, e.g. 20ms)
// speed: speed multiplier (e.g. 0.2 ~ 3.0)
// out_duty: 0 ~ PWM_MAX_DUTY for each channel (0~3)
typedef void (*pattern_func_t)(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);

typedef struct {
    const char *id;
    const char *name;
    const char *category; // "space", "aircraft", "auto", "fx", "manual"
    const char *desc;
    pattern_func_t func;
} pattern_def_t;

extern const pattern_def_t PATTERN_LIST[];
extern const int PATTERN_COUNT;

// Pattern functions
void pattern_solid(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_breathe(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_thruster(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_scanner(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_warp_charge(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_nav_strobe(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_landing_lights(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_hazard(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_engine_start(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_welding(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_glitch(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_fireflies(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_alternate(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_manual(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
void pattern_off(uint32_t frame, float speed, uint16_t out_duty[LED_NUM_CHANNELS]);
