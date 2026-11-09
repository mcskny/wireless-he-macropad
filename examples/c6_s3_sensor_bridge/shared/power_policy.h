// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdbool.h>
#include <stdint.h>
#define WHE_POWER_CHANNELS 12u
/* Raw ADC deltas. This is a test heuristic, not a calibrated switch position. */
typedef struct {
    uint16_t baseline[WHE_POWER_CHANNELS];
    bool initialized, active, release_timing;
    uint16_t wake_delta, release_delta;
    int64_t released_since_us;
} whe_power_policy_t;
void whe_power_policy_init(whe_power_policy_t *policy);
/* Raw ADC thresholds, with release strictly below wake. */
bool whe_power_policy_configure(whe_power_policy_t *policy,uint16_t wake_delta,uint16_t release_delta);
/* Start with all keys released. Returns true while 1 kHz active mode is wanted. */
bool whe_power_policy_update(whe_power_policy_t *policy,const uint16_t values[WHE_POWER_CHANNELS],int64_t now_us);

typedef struct {bool initialized;int64_t last_attempt_us;} whe_tx_policy_t;
/* A transition immediately emits the current sample; otherwise idle sends at most 10 Hz or 100 Hz while the app is open. */
bool whe_tx_policy_should_send(whe_tx_policy_t *policy,bool was_active,bool is_active,bool app_open,int64_t now_us);
