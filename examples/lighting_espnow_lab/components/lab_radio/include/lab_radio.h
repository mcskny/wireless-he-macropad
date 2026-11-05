#pragma once
#include "esp_err.h"
#include "protocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
typedef struct { uint8_t mac[6]; size_t len; uint8_t bytes[WIRE_MAX]; } radio_packet_t;
esp_err_t radio_init(const uint8_t peer[6], uint8_t channel, QueueHandle_t rx_queue);
esp_err_t radio_send(const wire_message_t *message);
bool radio_parse_mac(const char *text, uint8_t out[6]);
