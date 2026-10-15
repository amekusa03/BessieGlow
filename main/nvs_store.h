#pragma once

#include "esp_err.h"
#include "led_controller.h"

esp_err_t nvs_store_init(void);
esp_err_t nvs_store_load_state(led_state_t *state);
void nvs_store_schedule_save(void);
