#pragma once
typedef struct audio_bridge audio_bridge;
static inline audio_bridge* audio_create(void) { return 0; }
static inline void audio_destroy(audio_bridge* a) {}
static inline void audio_set_ctx(audio_bridge* a, void* ctx) {}
static inline void audio_start(audio_bridge* a) {}
static inline void audio_stop(audio_bridge* a) {}
static inline void audio_set_mic_enabled(audio_bridge* a, int enabled) {}
static inline void audio_set_latency(audio_bridge* a, int spk, int mic) {}
static inline void audio_set_keepalive(audio_bridge* a, int enabled) {}
