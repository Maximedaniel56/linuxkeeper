#ifdef WITH_PULSEAUDIO

#include "audio.h"
#include "config.h"
#include "log.h"

#include <pulse/pulseaudio.h>
#include <pulse/simple.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    pa_simple *simple;
    void *silence_buf;
    size_t buf_bytes;
} PulseData;

static int pulse_init(AudioContext *ctx, const Config *cfg) {
    (void)cfg;
    PulseData *data = (PulseData *)calloc(1, sizeof(PulseData));
    if (!data) return -1;
    ctx->backend_data = data;
    LK_DEBUG("PulseAudio backend initialized");
    return 0;
}

static int pulse_open_stream(AudioContext *ctx, const char *device) {
    PulseData *data = (PulseData *)ctx->backend_data;
    const char *dev = device;

    if (dev && strcmp(dev, "default") == 0)
        dev = NULL; /* NULL = default sink in PulseAudio */

    pa_sample_spec ss = {
        .format   = PA_SAMPLE_S16LE,
        .rate     = ctx->sample_rate,
        .channels = (uint8_t)ctx->channels,
    };

    pa_buffer_attr ba = {
        .maxlength = (uint32_t)-1,
        .tlength   = ctx->buffer_size * ctx->channels * 2, /* S16 = 2 bytes */
        .prebuf    = (uint32_t)-1,
        .minreq    = (uint32_t)-1,
        .fragsize  = (uint32_t)-1,
    };

    int error = 0;
    data->simple = pa_simple_new(
        NULL,                /* default server */
        "linuxkeeper",       /* app name */
        PA_STREAM_PLAYBACK,
        dev,                 /* device/sink name */
        "keep-alive",        /* stream description */
        &ss,
        NULL,                /* default channel map */
        &ba,
        &error
    );

    if (!data->simple) {
        LK_ERROR("PulseAudio: cannot open stream: %s", pa_strerror(error));
        return -1;
    }

    /* Allocate silence buffer */
    size_t period_frames = ctx->buffer_size / 4;
    if (period_frames < 256) period_frames = 256;
    data->buf_bytes = period_frames * ctx->channels * 2;
    data->silence_buf = calloc(1, data->buf_bytes);
    if (!data->silence_buf) {
        LK_ERROR("PulseAudio: cannot allocate silence buffer");
        pa_simple_free(data->simple);
        data->simple = NULL;
        return -1;
    }

    ctx->running = 1;
    LK_INFO("PulseAudio: opened stream on '%s' at %u Hz, %u ch",
            device ? device : "default", ctx->sample_rate, ctx->channels);
    return 0;
}

static int pulse_write_silence(AudioContext *ctx) {
    PulseData *data = (PulseData *)ctx->backend_data;
    if (!data->simple) return -1;

    int error = 0;
    int ret = pa_simple_write(data->simple, data->silence_buf, data->buf_bytes, &error);
    if (ret < 0) {
        LK_ERROR("PulseAudio: write error: %s", pa_strerror(error));
        return -1;
    }
    return 0;
}

static void pulse_close_stream(AudioContext *ctx) {
    PulseData *data = (PulseData *)ctx->backend_data;
    if (data->simple) {
        int error = 0;
        pa_simple_drain(data->simple, &error);
        pa_simple_free(data->simple);
        data->simple = NULL;
    }
    free(data->silence_buf);
    data->silence_buf = NULL;
    ctx->running = 0;
    LK_DEBUG("PulseAudio: stream closed");
}

static void pulse_destroy(AudioContext *ctx) {
    PulseData *data = (PulseData *)ctx->backend_data;
    if (data) {
        if (data->simple) pulse_close_stream(ctx);
        free(data);
        ctx->backend_data = NULL;
    }
}

/* Device listing using the async API for enumeration */
struct list_ctx {
    pa_mainloop *ml;
    int done;
};

static void sink_info_cb(pa_context *c, const pa_sink_info *info, int eol, void *userdata) {
    (void)c;
    struct list_ctx *lc = (struct list_ctx *)userdata;
    if (eol > 0) {
        lc->done = 1;
        return;
    }
    if (info) {
        printf("  %-40s %s\n", info->name, info->description ? info->description : "");
    }
}

static void context_state_cb(pa_context *c, void *userdata) {
    struct list_ctx *lc = (struct list_ctx *)userdata;
    pa_context_state_t state = pa_context_get_state(c);
    if (state == PA_CONTEXT_READY) {
        pa_context_get_sink_info_list(c, sink_info_cb, lc);
    } else if (state == PA_CONTEXT_FAILED || state == PA_CONTEXT_TERMINATED) {
        lc->done = 1;
    }
}

static int pulse_list_devices(AudioContext *ctx) {
    (void)ctx;
    printf("PulseAudio output sinks:\n");

    struct list_ctx lc = { .ml = NULL, .done = 0 };
    lc.ml = pa_mainloop_new();
    if (!lc.ml) return -1;

    pa_mainloop_api *api = pa_mainloop_get_api(lc.ml);
    pa_context *c = pa_context_new(api, "linuxkeeper");
    if (!c) { pa_mainloop_free(lc.ml); return -1; }

    pa_context_set_state_callback(c, context_state_cb, &lc);
    pa_context_connect(c, NULL, PA_CONTEXT_NOFLAGS, NULL);

    while (!lc.done) {
        int ret = 0;
        pa_mainloop_iterate(lc.ml, 1, &ret);
    }

    pa_context_disconnect(c);
    pa_context_unref(c);
    pa_mainloop_free(lc.ml);
    return 0;
}

const AudioBackend lk_backend_pulseaudio = {
    .name         = "pulseaudio",
    .init         = pulse_init,
    .open_stream  = pulse_open_stream,
    .write_silence = pulse_write_silence,
    .close_stream = pulse_close_stream,
    .destroy      = pulse_destroy,
    .list_devices = pulse_list_devices,
};

#endif /* WITH_PULSEAUDIO */
