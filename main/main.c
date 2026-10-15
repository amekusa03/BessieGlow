#include <stdio.h>
#include "esp_log.h"
#include "nvs_store.h"
#include "led_controller.h"
#include "wifi_ap.h"
#include "dns_server.h"
#include "web_server.h"

static const char *TAG = "main";

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  BessieGlow Studio (ESP32-C6 LED App)  ");
    ESP_LOGI(TAG, "========================================");

    // 1. Initialize NVS Storage
    ESP_ERROR_CHECK(nvs_store_init());

    // 2. Initialize Hardware LED Controller (GPIO 0, 1, 2, 3)
    ESP_ERROR_CHECK(led_controller_init());

    // 3. Restore last saved state from NVS if available
    led_state_t saved_state;
    if (nvs_store_load_state(&saved_state) == ESP_OK) {
        led_controller_set_state(&saved_state);
        ESP_LOGI(TAG, "Restored previous lighting pattern from NVS!");
    } else {
        ESP_LOGI(TAG, "No previous state found. Using default pattern.");
    }

    // 4. Initialize Wi-Fi SoftAP
    ESP_ERROR_CHECK(wifi_init_softap());

    // 5. Start DNS Server for Captive Portal (Auto-popup UI on mobile)
    ESP_ERROR_CHECK(dns_server_start());

    // 6. Start Web Server
    ESP_ERROR_CHECK(web_server_start());

    ESP_LOGI(TAG, "Initialization complete!");
    ESP_LOGI(TAG, "Connect your phone to Wi-Fi: '%s'", AP_WIFI_SSID);
    ESP_LOGI(TAG, "Web UI will open automatically, or browse to http://192.168.4.1");
}
