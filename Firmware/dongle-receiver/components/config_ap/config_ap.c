// SPDX-License-Identifier: MIT-only
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

/* CMake EMBED_FILES "web/index.html" ile eklenen binary pointer'lar */
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");

/**
 * @brief 192.168.4.1/ (kök dizin) isteği geldiğinde gömülü HTML sayfasını sunar.
 */
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

    if (ws_pkt.len >= 128) {
        ESP_LOGW(TAG, "Frame cok buyuk (%u byte), yoksayiliyor", (unsigned)ws_pkt.len);
        return ESP_OK;
    }

    /* 2. adim: gercek payload'i oku */
    uint8_t buf[128] = {0};
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ws_recv_frame (payload) hatasi: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WS recv (%u byte): %.*s", (unsigned)ws_pkt.len,
             (int)ws_pkt.len, (char *)ws_pkt.payload);

    /* Komut parse (basit string arama) */
    const char *resp = "OK";
    if (strstr((char *)ws_pkt.payload, "exit") != NULL) {
        s_exit_requested = true;
        resp = "exit_ack";
        ESP_LOGI(TAG, "Cikis komutu alindi");
    } else if (strstr((char *)ws_pkt.payload, "get_adc") != NULL) {
        s_adc_stream = true;
        resp = "adc_start";
    } else if (strstr((char *)ws_pkt.payload, "stop_adc") != NULL) {
        s_adc_stream = false;
        resp = "adc_stop";
    } else if (strstr((char *)ws_pkt.payload, "get_calib") != NULL) {
        resp = "calib_todo";  /* TODO: JSON kalibrasyon */
    } else {
        resp = "unknown_cmd";
    }

    /* Response gonder */
    httpd_ws_frame_t resp_pkt;
    memset(&resp_pkt, 0, sizeof(resp_pkt));
    resp_pkt.type = HTTPD_WS_TYPE_TEXT;
    resp_pkt.payload = (uint8_t *)resp;
    resp_pkt.len = strlen(resp);
    httpd_ws_send_frame(req, &resp_pkt);

    return ESP_OK;
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