// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 ESP32-S3 Dongle. All rights reserved.

/**
 * @file config_ap.c
 * @brief Wi-Fi AP + WebSocket server (Bölüm 2 CONFIG_AP_MODE).
 *
 * WebSocket handler (/ws) komutlari (text frame):
 *   - "exit"    → NORMAL_MODE'a donus
 *   - "get_adc"  → 100Hz ADC akisi baslat
 *   - "stop_adc" → ADC akisi durdur
 *   - "get_calib"→ kalibrasyon JSON (TODO)
 *   - "set_calib"→ NVS'e yaz (TODO, JSON parse karmaşık)
 *
 * ADC akisi: main.c config_ap_mode_step'te 100Hz'de config_ap_send_adc cagirir.
 */

#include "config_ap.h"

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "cJSON.h"
#include "nvs_store.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"

static const char *TAG = "config_ap";

static httpd_handle_t s_server = NULL;
static esp_netif_t *s_ap_netif = NULL;
static bool s_active = false;
static bool s_exit_requested = false;
static int s_ws_client_fd = -1;
static bool s_adc_stream = false;
static calibration_config_t *s_calibration = NULL;
#define WS_MAX_JSON_SIZE 4096

/* CMake EMBED_FILES "web/index.html" ile eklenen binary pointer'lar */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

/**
 * @brief 192.168.4.1/ (kök dizin) isteği geldiğinde gömülü HTML sayfasını sunar.
 */

void config_ap_bind_calibration(calibration_config_t *calibration)
{
    s_calibration = calibration;
}

/* value_type: 0=float, 1=uint16, 2=uint8, 3=enum/int */
static cJSON *number_array(const void *values, size_t count, int value_type)
{
    cJSON *array = cJSON_CreateArray();
    if (array == NULL) return NULL;
    for (size_t i = 0; i < count; ++i) {
        double value = value_type == 0 ? ((const float *)values)[i] :
            value_type == 1 ? ((const uint16_t *)values)[i] :
            value_type == 2 ? ((const uint8_t *)values)[i] : ((const int *)values)[i];
        cJSON_AddItemToArray(array, cJSON_CreateNumber(value));
    }
    return array;
}

static bool set_array(cJSON *root, const char *name, void *out, size_t count,
                      int value_type, double min_value, double max_value)
{
    cJSON *array = cJSON_GetObjectItemCaseSensitive(root, name);
    if (array == NULL) return true;
    if (!cJSON_IsArray(array) || cJSON_GetArraySize(array) != (int)count) return false;
    for (size_t i = 0; i < count; ++i) {
        cJSON *item = cJSON_GetArrayItem(array, (int)i);
        if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) || item->valuedouble < min_value || item->valuedouble > max_value ||
            (value_type != 0 && floor(item->valuedouble) != item->valuedouble)) return false;
        if (value_type == 0) ((float *)out)[i] = (float)item->valuedouble;
        else if (value_type == 1) ((uint16_t *)out)[i] = (uint16_t)item->valuedouble;
        else if (value_type == 2) ((uint8_t *)out)[i] = (uint8_t)item->valuedouble;
        else ((int *)out)[i] = (int)item->valuedouble;
    }
    return true;
}

static char *calibration_json(const calibration_config_t *cal)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) return NULL;
    cJSON_AddItemToObject(root, "min_adc", number_array(cal->min_adc, S3_KEY_COUNT, 1));
    cJSON_AddItemToObject(root, "max_adc", number_array(cal->max_adc, S3_KEY_COUNT, 1));
    cJSON_AddItemToObject(root, "actuation_point", number_array(cal->actuation_point, S3_KEY_COUNT, 0));
    cJSON_AddItemToObject(root, "rt_press_sensitivity", number_array(cal->rt_press_sensitivity, S3_KEY_COUNT, 0));
    cJSON_AddItemToObject(root, "rt_release_sensitivity", number_array(cal->rt_release_sensitivity, S3_KEY_COUNT, 0));
    cJSON_AddItemToObject(root, "trigger_mode", number_array(cal->trigger_mode, S3_KEY_COUNT, 3));
    cJSON_AddItemToObject(root, "key_mappings", number_array(&cal->key_mappings[0][0], S3_LAYER_COUNT * S3_KEY_COUNT, 1));
    cJSON_AddItemToObject(root, "snap_tap_pairs", number_array(cal->snap_tap_pairs, S3_SNAP_TAP_PAIR_MAX, 2));
    cJSON_AddItemToObject(root, "joystick_axes", number_array(cal->joystick_axes, S3_JOYSTICK_AXIS_COUNT, 2));
    cJSON_AddNumberToObject(root, "macro_trigger_key", cal->macro_trigger_key);
    cJSON *steps = cJSON_AddArrayToObject(root, "macro_steps");
    for (uint8_t i = 0; steps && i < cal->macro_step_count && i < S3_MACRO_MAX_STEPS; ++i) {
        cJSON *pair = cJSON_CreateArray();
        cJSON_AddItemToArray(pair, cJSON_CreateNumber(cal->macro_steps[i].keycode));
        cJSON_AddItemToArray(pair, cJSON_CreateNumber(cal->macro_steps[i].delay_ms));
        cJSON_AddItemToArray(steps, pair);
    }
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

static bool apply_calibration_json(cJSON *data)
{
    if (s_calibration == NULL || !cJSON_IsObject(data)) return false;
    calibration_config_t candidate = *s_calibration;
    if (!set_array(data, "min_adc", candidate.min_adc, S3_KEY_COUNT, 1, 0, 4095) ||
        !set_array(data, "max_adc", candidate.max_adc, S3_KEY_COUNT, 1, 0, 4095) ||
        !set_array(data, "actuation_point", candidate.actuation_point, S3_KEY_COUNT, 0, 0.01, 1.0) ||
        !set_array(data, "rt_press_sensitivity", candidate.rt_press_sensitivity, S3_KEY_COUNT, 0, 0.001, 1.0) ||
        !set_array(data, "rt_release_sensitivity", candidate.rt_release_sensitivity, S3_KEY_COUNT, 0, 0.001, 1.0) ||
        !set_array(data, "trigger_mode", candidate.trigger_mode, S3_KEY_COUNT, 3, 0, 1) ||
        !set_array(data, "key_mappings", &candidate.key_mappings[0][0], S3_LAYER_COUNT * S3_KEY_COUNT, 1, 0, 0x1FF) ||
        !set_array(data, "snap_tap_pairs", candidate.snap_tap_pairs, S3_SNAP_TAP_PAIR_MAX, 2, 0, 255) ||
        !set_array(data, "joystick_axes", candidate.joystick_axes, S3_JOYSTICK_AXIS_COUNT, 2, 0, 255)) return false;
    for (int i = 0; i < S3_KEY_COUNT; ++i) if (candidate.max_adc[i] <= candidate.min_adc[i]) return false;
    cJSON *trigger = cJSON_GetObjectItemCaseSensitive(data, "macro_trigger_key");
    if (trigger) {
        if (!cJSON_IsNumber(trigger) || floor(trigger->valuedouble) != trigger->valuedouble ||
            !((trigger->valueint >= 0 && trigger->valueint < S3_KEY_COUNT) || trigger->valueint == 255)) return false;
        candidate.macro_trigger_key = (uint8_t)trigger->valueint;
    }
    cJSON *steps = cJSON_GetObjectItemCaseSensitive(data, "macro_steps");
    if (steps) {
        if (!cJSON_IsArray(steps) || cJSON_GetArraySize(steps) > S3_MACRO_MAX_STEPS) return false;
        uint8_t count = (uint8_t)cJSON_GetArraySize(steps);
        for (uint8_t i = 0; i < count; ++i) {
            cJSON *pair = cJSON_GetArrayItem(steps, i);
            cJSON *key = cJSON_GetArrayItem(pair, 0);
            cJSON *delay = cJSON_GetArrayItem(pair, 1);
            if (!cJSON_IsArray(pair) || cJSON_GetArraySize(pair) != 2 || !cJSON_IsNumber(key) || !cJSON_IsNumber(delay) ||
                key->valueint < 0 || key->valueint > 0x1FF || floor(key->valuedouble) != key->valuedouble ||
                delay->valueint < 0 || delay->valueint > 5000 || floor(delay->valuedouble) != delay->valuedouble) return false;
            candidate.macro_steps[i].keycode = (uint16_t)key->valueint;
            candidate.macro_steps[i].delay_ms = (uint16_t)delay->valueint;
        }
        candidate.macro_step_count = count;
    }
    if (nvs_store_save_calibration(&candidate) != ESP_OK) return false;
    *s_calibration = candidate;
    return true;
}

static esp_err_t root_html_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    const size_t index_html_len = index_html_end - index_html_start;
    httpd_resp_send(req, (const char *)index_html_start, index_html_len);
    return ESP_OK;
}

/**
 * @brief WebSocket handler: gelen frame'leri isle.
 */
static esp_err_t ws_handler(httpd_req_t *req)
{
    /* WebSocket handshake (ilk GET) */
    if (req->method == HTTP_GET) {
        s_ws_client_fd = httpd_req_to_sockfd(req);
        ESP_LOGI(TAG, "WebSocket client baglandi (fd=%d)", s_ws_client_fd);
        return ESP_OK;
    }

    /* 1. adim: once frame uzunlugunu al (payload=NULL, max_len=0) */
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ws_recv_frame (len) hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    if (ws_pkt.len == 0) {
        /* Bos frame (ornegin PING/PONG/CLOSE control frame) */
        return ESP_OK;
    }

    if (ws_pkt.len > WS_MAX_JSON_SIZE) {
        ESP_LOGW(TAG, "Frame cok buyuk (%u byte), yoksayiliyor", (unsigned)ws_pkt.len);
        return ESP_ERR_INVALID_SIZE;
    }
    uint8_t *buf = calloc(1, ws_pkt.len + 1);
    if (buf == NULL) return ESP_ERR_NO_MEM;
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) { free(buf); return ret; }

    ESP_LOGI(TAG, "WS recv (%u byte): %.*s", (unsigned)ws_pkt.len,
             (int)ws_pkt.len, (char *)ws_pkt.payload);

    char *resp = NULL;
    cJSON *request = cJSON_Parse((const char *)buf);
    cJSON *cmd_item = request ? cJSON_GetObjectItemCaseSensitive(request, "cmd") : NULL;
    const char *cmd = cJSON_IsString(cmd_item) ? cmd_item->valuestring : (const char *)buf;
    if (strcmp(cmd, "exit") == 0) {
        s_exit_requested = true; resp = strdup("{\"cmd\":\"exit_ack\"}");
    } else if (strcmp(cmd, "get_adc") == 0) {
        s_adc_stream = true; resp = strdup("{\"cmd\":\"adc_start\"}");
    } else if (strcmp(cmd, "stop_adc") == 0) {
        s_adc_stream = false; resp = strdup("{\"cmd\":\"adc_stop\"}");
    } else if (strcmp(cmd, "get_calib") == 0) {
        char *data = s_calibration ? calibration_json(s_calibration) : NULL;
        cJSON *reply = cJSON_CreateObject();
        if (reply && data) {
            cJSON_AddStringToObject(reply, "cmd", "calib");
            cJSON_AddRawToObject(reply, "data", data);
            resp = cJSON_PrintUnformatted(reply);
        }
        free(data); cJSON_Delete(reply);
    } else if (strcmp(cmd, "set_calib") == 0) {
        cJSON *data = cJSON_GetObjectItemCaseSensitive(request, "data");
        bool ok = apply_calibration_json(data);
        resp = strdup(ok ? "{\"cmd\":\"set_calib_ack\"}" : "{\"cmd\":\"error\",\"message\":\"invalid_calibration\"}");
    } else {
        resp = strdup("{\"cmd\":\"unknown_cmd\"}");
    }
    cJSON_Delete(request);
    free(buf);
    if (resp == NULL) return ESP_ERR_NO_MEM;

    /* Response gonder */
    httpd_ws_frame_t resp_pkt;
    memset(&resp_pkt, 0, sizeof(resp_pkt));
    resp_pkt.type = HTTPD_WS_TYPE_TEXT;
    resp_pkt.payload = (uint8_t *)resp;
    resp_pkt.len = strlen(resp);
    ret = httpd_ws_send_frame(req, &resp_pkt);
    free(resp);
    return ret;
}

esp_err_t config_ap_start(const char *ssid, const char *password, uint8_t channel)
{
    if (s_active) {
        return ESP_OK;
    }

    /* AP netif'i (DHCP server dahil) sadece bir kez olustur */
    if (s_ap_netif == NULL) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (s_ap_netif == NULL) {
            ESP_LOGE(TAG, "esp_netif_create_default_wifi_ap basarisiz");
            return ESP_FAIL;
        }
    }

    /* Wi-Fi AP mode */
    wifi_config_t ap_config = {0};
    strncpy((char *)ap_config.ap.ssid, ssid ? ssid : "S3-Config",
            sizeof(ap_config.ap.ssid) - 1);
    if (password && strlen(password) >= 8) {
        strncpy((char *)ap_config.ap.password, password,
                sizeof(ap_config.ap.password) - 1);
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ap_config.ap.channel = channel;
    ap_config.ap.max_connection = 1;
    ap_config.ap.ssid_hidden = 0;

    esp_wifi_stop();
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* HTTP server + WebSocket */
    httpd_config_t http_cfg = HTTPD_DEFAULT_CONFIG();
    http_cfg.server_port = 80;
    http_cfg.max_uri_handlers = 2; /* 1x Root HTML + 1x WebSocket */

    esp_err_t ret = httpd_start(&s_server, &http_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Kök dizin (Web UI) URI handler'ı */
    static const httpd_uri_t root_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = root_html_handler,
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(s_server, &root_uri);

    /* WebSocket (/ws) URI handler'ı */
    static const httpd_uri_t ws_uri = {
        .uri          = "/ws",
        .method       = HTTP_GET,
        .handler      = ws_handler,
        .is_websocket = true,
    };
    httpd_register_uri_handler(s_server, &ws_uri);

    s_active = true;
    s_exit_requested = false;
    s_adc_stream = false;
    s_ws_client_fd = -1;
    ESP_LOGI(TAG, "Config AP baslatildi: SSID='%s' ch=%u (WebSocket :80/ws)",
             ssid ? ssid : "S3-Config", channel);
    return ESP_OK;
}

esp_err_t config_ap_stop(void)
{
    if (!s_active) {
        return ESP_OK;
    }

    if (s_server != NULL) {
        httpd_stop(s_server);
        s_server = NULL;
    }

    esp_wifi_stop();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    s_active = false;
    s_ws_client_fd = -1;
    s_adc_stream = false;
    ESP_LOGI(TAG, "Config AP durduruldu, NORMAL_MODE'a donuldu");
    return ESP_OK;
}

bool config_ap_is_active(void)
{
    return s_active;
}

esp_err_t config_ap_send_adc(const uint16_t analog[12])
{
    if (!s_active || s_server == NULL || s_ws_client_fd < 0 || !s_adc_stream) {
        return ESP_ERR_NOT_FOUND;
    }

    /* Text frame: "adc:1234,2345,...,4567" */
    char buf[128];
    int pos = snprintf(buf, sizeof(buf), "adc:");
    for (int i = 0; i < 12 && pos < (int)sizeof(buf) - 6; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%u%s",
                        analog[i], (i < 11) ? "," : "");
    }

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ws_pkt.payload = (uint8_t *)buf;
    ws_pkt.len = strlen(buf);

    return httpd_ws_send_frame_async(s_server, s_ws_client_fd, &ws_pkt);
}

bool config_ap_exit_requested(void)
{
    bool req = s_exit_requested;
    s_exit_requested = false;
    return req;
}