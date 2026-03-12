#ifndef LK_AUDIO_H
#define LK_AUDIO_H

#include <stdint.h>
#include <stddef.h>

typedef struct AudioContext AudioContext;
typedef struct Config Config;

typedef struct AudioBackend {
    const char *name;
    int  (*init)(AudioContext *ctx, const Config *cfg);
    int  (*open_stream)(AudioContext *ctx, const char *device);
    int  (*write_silence)(AudioContext *ctx);
    void (*close_stream)(AudioContext *ctx);
    void (*destroy)(AudioContext *ctx);
    int  (*list_devices)(AudioContext *ctx);
} AudioBackend;

struct AudioContext {
    const AudioBackend *backend;
    void *backend_data;
    unsigned int sample_rate;
    unsigned int channels;
    unsigned int buffer_size;
    int running;
};

/* Backend auto-detection: returns best available backend, or NULL */
const AudioBackend *lk_audio_detect_backend(const char *preferred);

/* Get a backend by name (pipewire, pulseaudio, alsa, oss) */
const AudioBackend *lk_audio_get_backend(const char *name);

/* List all compiled-in backends */
void lk_audio_list_backends(void);

/* Convenience: create + init context */
AudioContext *lk_audio_create(const Config *cfg);
void lk_audio_free(AudioContext *ctx);

/* Extern backend definitions (conditionally compiled) */
#ifdef WITH_PIPEWIRE
extern const AudioBackend lk_backend_pipewire;
#endif
#ifdef WITH_PULSEAUDIO
extern const AudioBackend lk_backend_pulseaudio;
#endif
#ifdef WITH_ALSA
extern const AudioBackend lk_backend_alsa;
#endif
#ifdef WITH_OSS
extern const AudioBackend lk_backend_oss;
#endif

#endif
