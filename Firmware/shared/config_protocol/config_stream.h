// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "config_protocol.h"

/* Bounded byte-stream decoder. It does not authenticate or execute commands.
   Callback frame and payload point into this parser and are valid only until
   the callback returns. Feed/reset the same parser from one task only; the
   callback must not call feed/reset on it. A transport must apply an idle
   timeout and reset: an incomplete but plausible header can otherwise wait
   for its remaining bytes indefinitely. */
typedef struct {
    uint8_t buffer[WHE_CFG_MAX_FRAME];
    size_t used;
    uint32_t rejected;
} whe_cfg_stream_t;
typedef void (*whe_cfg_stream_handler_t)(const whe_cfg_frame_t *frame, void *context);

void whe_cfg_stream_init(whe_cfg_stream_t *stream);
/* Reset pending bytes on disconnect or a transport-defined idle timeout. */
void whe_cfg_stream_reset(whe_cfg_stream_t *stream);
/* Number of verified frames delivered. NULL handler is allowed. */
size_t whe_cfg_stream_feed(whe_cfg_stream_t *stream, const uint8_t *data, size_t length,
                           whe_cfg_stream_handler_t handler, void *context);
