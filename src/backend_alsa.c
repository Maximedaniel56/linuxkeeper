#ifdef WITH_ALSA

#include "audio.h"
#include "config.h"
#include "log.h"

#include <alsa/asoundlib.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    snd_pcm_t *pcm;
    void *silence_buf;
    snd_pcm_uframes_t buffer_frames;
    int frame_bytes;
} AlsaData;

static int alsa_init(AudioContext *ctx, const Config *cfg) {
    (void)cfg;
    AlsaData *data = (AlsaData *)calloc(1, sizeof(AlsaData));
    if (!data) return -1;
    ctx->backend_data = data;
    LK_DEBUG("ALSA backend initialized");
    return 0;
}

static int alsa_open_stream(AudioContext *ctx, const char *device) {
    AlsaData *data = (AlsaData *)ctx->backend_data;
    const char *dev = device;

    if (!dev || strcmp(dev, "default") == 0)
        dev = "default";

    int err = snd_pcm_open(&data->pcm, dev, SND_PCM_STREAM_PLAYBACK, 0);
    if (err < 0) {
        LK_ERROR("ALSA: cannot open device '%s': %s", dev, snd_strerror(err));
        return -1;
    }

    snd_pcm_hw_params_t *hw;
    snd_pcm_hw_params_alloca(&hw);
    snd_pcm_hw_params_any(data->pcm, hw);

    /* Use RW interleaved mode (not MMAP for BT compatibility) */
    err = snd_pcm_hw_params_set_access(data->pcm, hw, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0) {
        LK_ERROR("ALSA: cannot set access type: %s", snd_strerror(err));
        goto fail;
    }

    err = snd_pcm_hw_params_set_format(data->pcm, hw, SND_PCM_FORMAT_S16_LE);
    if (err < 0) {
        LK_ERROR("ALSA: cannot set format S16_LE: %s", snd_strerror(err));
        goto fail;
    }

    unsigned int rate = ctx->sample_rate;
    err = snd_pcm_hw_params_set_rate_near(data->pcm, hw, &rate, NULL);
    if (err < 0) {
        LK_ERROR("ALSA: cannot set sample rate %u: %s", ctx->sample_rate, snd_strerror(err));
        goto fail;
    }
    if (rate != ctx->sample_rate) {
        LK_WARN("ALSA: requested rate %u, got %u", ctx->sample_rate, rate);
    }

    err = snd_pcm_hw_params_set_channels(data->pcm, hw, ctx->channels);
    if (err < 0) {
        LK_ERROR("ALSA: cannot set channels %u: %s", ctx->channels, snd_strerror(err));
        goto fail;
    }

    snd_pcm_uframes_t buffer_size = ctx->buffer_size;
    err = snd_pcm_hw_params_set_buffer_size_near(data->pcm, hw, &buffer_size);
    if (err < 0) {
        LK_WARN("ALSA: cannot set buffer size, using default");
    }

    snd_pcm_uframes_t period_size = buffer_size / 4;
    err = snd_pcm_hw_params_set_period_size_near(data->pcm, hw, &period_size, NULL);
    if (err < 0) {
        LK_WARN("ALSA: cannot set period size");
    }

    err = snd_pcm_hw_params(data->pcm, hw);
    if (err < 0) {
        LK_ERROR("ALSA: cannot set hw params: %s", snd_strerror(err));
        goto fail;
    }

    /* Get actual values */
    snd_pcm_hw_params_get_buffer_size(hw, &data->buffer_frames);
    snd_pcm_hw_params_get_period_size(hw, &period_size, NULL);

    /* Allocate silence buffer (S16_LE = 2 bytes per sample) */
    data->frame_bytes = (int)(ctx->channels * 2);
    data->silence_buf = calloc(period_size, (size_t)data->frame_bytes);
    if (!data->silence_buf) {
        LK_ERROR("ALSA: cannot allocate silence buffer");
        goto fail;
    }
    data->buffer_frames = period_size;

    err = snd_pcm_prepare(data->pcm);
    if (err < 0) {
        LK_ERROR("ALSA: cannot prepare device: %s", snd_strerror(err));
        goto fail;
    }

    ctx->running = 1;
    LK_INFO("ALSA: opened device '%s' at %u Hz, %u ch, period=%lu frames",
            dev, rate, ctx->channels, (unsigned long)period_size);
    return 0;

fail:
    snd_pcm_close(data->pcm);
    data->pcm = NULL;
    free(data->silence_buf);
    data->silence_buf = NULL;
    return -1;
}

static int alsa_write_silence(AudioContext *ctx) {
    AlsaData *data = (AlsaData *)ctx->backend_data;
    if (!data->pcm) return -1;

    snd_pcm_sframes_t written = snd_pcm_writei(data->pcm, data->silence_buf, data->buffer_frames);
    if (written < 0) {
        LK_DEBUG("ALSA: write error (%s), attempting recovery", snd_strerror((int)written));
        int err = snd_pcm_recover(data->pcm, (int)written, 1);
        if (err < 0) {
            LK_ERROR("ALSA: recovery failed: %s", snd_strerror(err));
            return -1;
        }
        /* Retry after recovery */
        written = snd_pcm_writei(data->pcm, data->silence_buf, data->buffer_frames);
        if (written < 0) {
            LK_ERROR("ALSA: write failed after recovery: %s", snd_strerror((int)written));
            return -1;
        }
    }
    return 0;
}

static void alsa_close_stream(AudioContext *ctx) {
    AlsaData *data = (AlsaData *)ctx->backend_data;
    if (data->pcm) {
        snd_pcm_drain(data->pcm);
        snd_pcm_close(data->pcm);
        data->pcm = NULL;
    }
    free(data->silence_buf);
    data->silence_buf = NULL;
    ctx->running = 0;
    LK_DEBUG("ALSA: stream closed");
}

static void alsa_destroy(AudioContext *ctx) {
    AlsaData *data = (AlsaData *)ctx->backend_data;
    if (data) {
        if (data->pcm) alsa_close_stream(ctx);
        free(data);
        ctx->backend_data = NULL;
    }
}

static int alsa_list_devices(AudioContext *ctx) {
    (void)ctx;
    printf("ALSA output devices:\n");

    void **hints = NULL;
    int err = snd_device_name_hint(-1, "pcm", &hints);
    if (err < 0) {
        printf("  (failed to enumerate: %s)\n", snd_strerror(err));
        return -1;
    }

    for (void **h = hints; *h; h++) {
        char *name = snd_device_name_get_hint(*h, "NAME");
        char *desc = snd_device_name_get_hint(*h, "DESC");
        char *ioid = snd_device_name_get_hint(*h, "IOID");

        /* Skip input-only devices */
        if (ioid && strcmp(ioid, "Input") == 0) {
            free(name); free(desc); free(ioid);
            continue;
        }

        if (name) {
            printf("  %s", name);
            if (desc) {
                /* Print first line of description */
                char *nl = strchr(desc, '\n');
                if (nl) *nl = '\0';
                printf(" - %s", desc);
            }
            printf("\n");
        }
        free(name); free(desc); free(ioid);
    }
    snd_device_name_free_hint(hints);
    return 0;
}

const AudioBackend lk_backend_alsa = {
    .name         = "alsa",
    .init         = alsa_init,
    .open_stream  = alsa_open_stream,
    .write_silence = alsa_write_silence,
    .close_stream = alsa_close_stream,
    .destroy      = alsa_destroy,
    .list_devices = alsa_list_devices,
};

#endif /* WITH_ALSA */
