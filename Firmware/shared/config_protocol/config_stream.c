// SPDX-License-Identifier: MIT
#include "config_stream.h"
#include <string.h>

void whe_cfg_stream_init(whe_cfg_stream_t *s) {
    if (s) memset(s, 0, sizeof(*s));
}
void whe_cfg_stream_reset(whe_cfg_stream_t *s) {
    if (s) s->used = 0;
}
static void discard_first(whe_cfg_stream_t *s) {
    if (!s->used) return;
    --s->used;
    memmove(s->buffer, s->buffer + 1, s->used);
}
static size_t process(whe_cfg_stream_t *s, whe_cfg_stream_handler_t handler, void *context) {
    size_t delivered = 0;
    for (;;) {
        /* Retain a partial magic prefix when the next chunk has not arrived. */
        while (s->used && memcmp(s->buffer, "WHEC", s->used < 4 ? s->used : 4)) {
            discard_first(s);
        }
        if (s->used < WHE_CFG_HEADER_SIZE) break;
        size_t payload = (size_t)s->buffer[10] | ((size_t)s->buffer[11] << 8);
        uint8_t type = s->buffer[5];
        bool type_ok = (type >= WHE_CFG_GET_CAPABILITIES && type <= WHE_CFG_ABORT) || type == WHE_CFG_REPLY;
        bool id_ok = s->buffer[6] || s->buffer[7] || s->buffer[8] || s->buffer[9];
        if (s->buffer[4] != 1 || !type_ok || !id_ok || payload > WHE_CFG_MAX_PAYLOAD) {
            ++s->rejected;
            discard_first(s);
            continue;
        }
        size_t need = WHE_CFG_HEADER_SIZE + payload + WHE_CFG_TRAILER_SIZE;
        if (s->used < need) break;
        whe_cfg_frame_t frame;
        if (!whe_cfg_decode(s->buffer, need, &frame)) {
            ++s->rejected;
            discard_first(s);
            continue;
        }
        if (handler) handler(&frame, context);
        ++delivered;
        s->used -= need;
        memmove(s->buffer, s->buffer + need, s->used);
    }
    return delivered;
}
size_t whe_cfg_stream_feed(whe_cfg_stream_t *s, const uint8_t *data, size_t length,
                           whe_cfg_stream_handler_t handler, void *context) {
    if (!s || (!data && length)) return 0;
    size_t delivered = 0;
    for (size_t i=0; i<length; ++i) {
        if (s->used >= sizeof(s->buffer)) { ++s->rejected; discard_first(s); }
        s->buffer[s->used++] = data[i];
        delivered += process(s, handler, context);
    }
    return delivered;
}
