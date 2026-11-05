#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LIGHT_KEYS 12
#define LIGHT_WAVES 64
#define FLAG_COLORFUL 1u
#define FLAG_DEPTH 2u
#define FLAG_SWEEP_TURN 4u

typedef enum { EFFECT_OFF, EFFECT_STATIC, EFFECT_BREATHE, EFFECT_FLOW,
               EFFECT_RAINBOW, EFFECT_SWEEP, EFFECT_REACTIVE, EFFECT_RIPPLE, EFFECT_STARLIGHT, EFFECT_SNAKE,
               EFFECT_COUNT } light_effect_t;
typedef struct { uint8_t effect, r, g, b, brightness, speed, flags; } light_profile_t;
typedef struct { uint8_t r,g,b; } light_rgb_t;
typedef struct { uint64_t start_ms; float duration, hue, peak; uint8_t key; bool held, active; } light_wave_t;
typedef struct {
    light_profile_t profile;
    float depth[LIGHT_KEYS], hue[LIGHT_KEYS];
    bool pressed[LIGHT_KEYS], hit_valid[LIGHT_KEYS];
    uint64_t hit_ms[LIGHT_KEYS];
    uint32_t hit_count;
    light_wave_t waves[LIGHT_WAVES];
    size_t next_wave;
} light_engine_t;

bool light_profile_valid(const light_profile_t *p);
void light_engine_init(light_engine_t *s, const light_profile_t *p);
bool light_engine_set_profile(light_engine_t *s, const light_profile_t *p);
/* depth: calibrated 0.0=rest, 1.0=fully pressed; call on every sensor update. */
void light_engine_input(light_engine_t *s, uint8_t key, float depth, uint64_t now_ms);
/* Reconcile a locally detected physical hit with the Studio event sequence. */
void light_engine_sync_physical_hit(light_engine_t *s,uint8_t key,uint32_t id,uint64_t now_ms,bool awaiting_local_press);
void light_engine_virtual_hit(light_engine_t *s,uint8_t key,uint32_t id,uint64_t now_ms);
void light_engine_render(light_engine_t *s, uint64_t now_ms, light_rgb_t out[LIGHT_KEYS]);
