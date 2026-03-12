#include "audio.h"
#include "config.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>

/* Table of all compiled-in backends, in preference order */
static const AudioBackend *backends[] = {
#ifdef WITH_PIPEWIRE
    &lk_backend_pipewire,
#endif
#ifdef WITH_PULSEAUDIO
    &lk_backend_pulseaudio,
#endif
#ifdef WITH_ALSA
    &lk_backend_alsa,
#endif
#ifdef WITH_OSS
    &lk_backend_oss,
#endif
    NULL
};

const AudioBackend *lk_audio_get_backend(const char *name) {
    for (int i = 0; backends[i]; i++) {
        if (strcasecmp(backends[i]->name, name) == 0)
            return backends[i];
    }
    return NULL;
}

const AudioBackend *lk_audio_detect_backend(const char *preferred) {
    if (preferred && strcasecmp(preferred, "auto") != 0) {
        const AudioBackend *b = lk_audio_get_backend(preferred);
        if (b) {
            LK_INFO("Using requested backend: %s", b->name);
            return b;
        }
        LK_WARN("Requested backend '%s' not available, falling back to auto-detect", preferred);
    }

    /* Auto-detect: try each in order */
    for (int i = 0; backends[i]; i++) {
        LK_DEBUG("Trying backend: %s", backends[i]->name);
        /* We'll try init in the create function; here just return first available */
        return backends[i];
    }

    LK_ERROR("No audio backends available!");
    return NULL;
}

void lk_audio_list_backends(void) {
    printf("Compiled-in audio backends:\n");
    for (int i = 0; backends[i]; i++) {
        printf("  - %s%s\n", backends[i]->name, i == 0 ? " (preferred)" : "");
    }
    if (!backends[0]) {
        printf("  (none)\n");
    }
}

AudioContext *lk_audio_create(const Config *cfg) {
    const AudioBackend *b = lk_audio_detect_backend(cfg->backend);
    if (!b) return NULL;

    AudioContext *ctx = (AudioContext *)calloc(1, sizeof(AudioContext));
    if (!ctx) return NULL;

    ctx->backend = b;
    ctx->sample_rate = cfg->sample_rate;
    ctx->channels = cfg->channels;
    ctx->buffer_size = cfg->buffer_size;
    ctx->running = 0;

    /* Try init; if fails, try next backends */
    if (b->init(ctx, cfg) != 0) {
        LK_WARN("Backend '%s' init failed, trying alternatives", b->name);
        for (int i = 0; backends[i]; i++) {
            if (backends[i] == b) continue;
            ctx->backend = backends[i];
            LK_DEBUG("Trying backend: %s", backends[i]->name);
            if (backends[i]->init(ctx, cfg) == 0) {
                LK_INFO("Using fallback backend: %s", backends[i]->name);
                return ctx;
            }
        }
        LK_ERROR("All backends failed to initialize");
        free(ctx);
        return NULL;
    }

    LK_INFO("Audio context created with backend: %s", b->name);
    return ctx;
}

void lk_audio_free(AudioContext *ctx) {
    if (!ctx) return;
    if (ctx->backend && ctx->backend->destroy)
        ctx->backend->destroy(ctx);
    free(ctx);
}
