#include "web_server.h"
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include "led_controller.h"
#include "patterns.h"
#include "nvs_store.h"

static const char *TAG = "web_server";

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

// Handler for Root GET /
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    size_t len = index_html_end - index_html_start;
    return httpd_resp_send(req, (const char *)index_html_start, len);
}

// Captive Portal Redirect Handler
static esp_err_t captive_redirect_handler(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

// GET /api/status Handler
static esp_err_t status_handler(httpd_req_t *req) {
    led_state_t state;
    led_controller_get_state(&state);

    uint8_t live[LED_NUM_CHANNELS];
    led_controller_get_live_levels(live);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "pattern_idx", state.pattern_idx);
    cJSON_AddNumberToObject(root, "master_brightness", state.master_brightness);
    cJSON_AddNumberToObject(root, "speed", state.speed);
    cJSON_AddBoolToObject(root, "power_on", state.power_on);

    cJSON *live_arr = cJSON_CreateArray();
    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        cJSON_AddItemToArray(live_arr, cJSON_CreateNumber(live[i]));
    }
    cJSON_AddItemToObject(root, "live", live_arr);

    cJSON *m_arr = cJSON_CreateArray();
    for (int i = 0; i < LED_NUM_CHANNELS; i++) {
        cJSON_AddItemToArray(m_arr, cJSON_CreateNumber(state.manual_ch[i]));
    }
    cJSON_AddItemToObject(root, "manual_ch", m_arr);

    cJSON *pat_arr = cJSON_CreateArray();
    for (int i = 0; i < PATTERN_COUNT; i++) {
        cJSON *p = cJSON_CreateObject();
        cJSON_AddStringToObject(p, "id", PATTERN_LIST[i].id);
        cJSON_AddStringToObject(p, "name", PATTERN_LIST[i].name);
        cJSON_AddStringToObject(p, "category", PATTERN_LIST[i].category);
        cJSON_AddStringToObject(p, "desc", PATTERN_LIST[i].desc);
        cJSON_AddItemToArray(pat_arr, p);
    }
    cJSON_AddItemToObject(root, "patterns", pat_arr);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);
    return ret;
}

// POST /api/control Handler
static esp_err_t control_handler(httpd_req_t *req) {
    char buf[256];
    int total_len = req->content_len;
    if (total_len >= sizeof(buf)) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int cur_len = 0;
    while (cur_len < total_len) {
        int received = httpd_req_recv(req, buf + cur_len, total_len - cur_len);
        if (received <= 0) {
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        cur_len += received;
    }
    buf[total_len] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *item = cJSON_GetObjectItem(json, "pattern");
    if (item && cJSON_IsNumber(item)) {
        led_controller_set_pattern_idx(item->valueint);
    }

    item = cJSON_GetObjectItem(json, "brightness");
    if (item && cJSON_IsNumber(item)) {
        led_controller_set_brightness((uint8_t)item->valueint);
    }

    item = cJSON_GetObjectItem(json, "speed");
    if (item && cJSON_IsNumber(item)) {
        led_controller_set_speed((float)item->valuedouble);
    }

    item = cJSON_GetObjectItem(json, "power");
    if (item && cJSON_IsBool(item)) {
        led_controller_set_power(cJSON_IsTrue(item));
    }

    item = cJSON_GetObjectItem(json, "manual");
    if (item && cJSON_IsArray(item)) {
        int sz = cJSON_GetArraySize(item);
        for (int i = 0; i < sz && i < LED_NUM_CHANNELS; i++) {
            cJSON *sub = cJSON_GetArrayItem(item, i);
            if (sub && cJSON_IsNumber(sub)) {
                led_controller_set_manual_ch(i, (uint8_t)sub->valueint);
            }
        }
    }

    cJSON_Delete(json);

    // Schedule persistent write to NVS
    nvs_store_schedule_save();

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"status\":\"ok\"}", 15);
}

esp_err_t web_server_start(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    config.stack_size = 8192;
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    // URI Handlers
    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_root);

    httpd_uri_t uri_status = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_status);

    httpd_uri_t uri_control = {
        .uri = "/api/control",
        .method = HTTP_POST,
        .handler = control_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &uri_control);

    // Captive Portal probes
    const char *captive_uris[] = {
        "/generate_204",
        "/gen_204",
        "/hotspot-detect.html",
        "/canonical.html",
        "/connecttest.txt",
        "/ncsi.txt"
    };

    for (int i = 0; i < sizeof(captive_uris)/sizeof(captive_uris[0]); i++) {
        httpd_uri_t uri_cap = {
            .uri = captive_uris[i],
            .method = HTTP_GET,
            .handler = captive_redirect_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &uri_cap);
    }

    ESP_LOGI(TAG, "Web server started on port 80");
    return ESP_OK;
}
