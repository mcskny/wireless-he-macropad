#pragma once
#include "lighting.h"
#define WIRE_MAX 64u
#define MSG_PROFILE 1u
#define MSG_ACTIVE 2u
#define MSG_ACK 3u
#define ACK_OK 0u
#define ACK_INVALID 1u
#define ACK_STORAGE 2u

typedef struct { uint8_t type, len; uint32_t session, seq; uint8_t payload[32]; } wire_message_t;
/* Magic 'HELA', v1, type, LE payload size, LE session, LE sequence, payload, LE CRC-32/IEEE. */
size_t wire_encode(const wire_message_t *msg, uint8_t out[WIRE_MAX]);
bool wire_decode(const uint8_t *in, size_t len, wire_message_t *out);
void wire_pack_profile(uint8_t dst[8], uint8_t layer, const light_profile_t *p);
bool wire_unpack_profile(const uint8_t *src, size_t len, uint8_t *layer, light_profile_t *p);
