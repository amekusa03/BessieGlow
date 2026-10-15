#pragma once

#include "esp_err.h"

#define AP_WIFI_SSID "Bessie-Glow"
#define AP_WIFI_PASS ""
#define AP_MAX_CONN  4
#define AP_CHANNEL   1

esp_err_t wifi_init_softap(void);
