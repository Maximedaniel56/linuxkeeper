#ifdef WITH_PIPEWIRE

#include "audio.h"
#include "config.h"
#include "log.h"

/*
 * PipeWire backend - uses the pipewire-pulse (libpulse) compatibility layer
 * if available, since it provides the widest compatibility. If PipeWire is
 * running but the app links against libpulse, PipeWire transparently handles
 * it. This backend is for native PipeWire API usage.
 */

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    struct pw_main_loop *loop;
    struct pw_stream *stream;
    void *silence_buf;
    size_t buf_bytes;
    size_t stride;
    uint32_t n_frames;
    int stream_ready;
} PwData;

static void on_process(void *userdata) {
    AudioContext *ctx = (AudioContext *)userdata;
    PwData *data = (PwData *)ctx->backend_data;

    struct pw_buffer *b = pw_stream_dequeue_buffer(data->stream);
    if (!b) {
        LK_DEBUG("PipeWire: no buffer available");
        return;
    }

    struct spa_buffer *buf = b->buffer;
    void *dst = buf->datas[0].data;
    if (!dst) {
        pw_stream_queue_buffer(data->stream, b);
        return;
    }

    uint32_t n_frames = buf->datas[0].maxsize / (uint32_t)data->stride;
    if (n_frames > data->n_frames) n_frames = data->n_frames;

    /* Write silence (zeros) */
    memset(dst, 0, n_frames * data->stride);

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = (int32_t)data->stride;
    buf->datas[0].chunk->size = n_frames * (uint32_t)data->stride;

    pw_stream_queue_buffer(data->stream, b);
}

static void on_state_changed(void *userdata, enum pw_stream_state old,
                              enum pw_stream_state state, const char *error) {
    AudioContext *ctx = (AudioContext *)userdata;
    PwData *data = (PwData *)ctx->backend_data;
    (void)old;

    LK_DEBUG("PipeWire: stream state: %s%s%s",
             pw_stream_state_as_string(state),
             error ? " - " : "", error ? error : "");

    if (state == PW_STREAM_STATE_STREAMING)
        data->stream_ready = 1;
    else if (state == PW_STREAM_STATE_ERROR)
        data->stream_ready = 0;
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .process = on_process,
    .state_changed = on_state_changed,
};

static int pw_backend_init(AudioContext *ctx, const Config *cfg) {
    (void)cfg;
    pw_init(NULL, NULL);

    PwData *data = (PwData *)calloc(1, sizeof(PwData));
    if (!data) return -1;
    ctx->backend_data = data;
    LK_DEBUG("PipeWire backend initialized");
    return 0;
}

static int pw_open_stream(AudioContext *ctx, const char *device) {
    PwData *data = (PwData *)ctx->backend_data;

    data->loop = pw_main_loop_new(NULL);
    if (!data->loop) {
        LK_ERROR("PipeWire: cannot create main loop");
        return -1;
    }

    struct pw_properties *props = pw_properties_new(
        PW_KEY_MEDIA_TYPE, "Audio",
        PW_KEY_MEDIA_CATEGORY, "Playback",
        PW_KEY_MEDIA_ROLE, "Music",
        PW_KEY_APP_NAME, "linuxkeeper",
        PW_KEY_NODE_NAME, "linuxkeeper-keepalive",
        NULL
    );

    if (device && strcmp(device, "default") != 0) {
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, device);
    }

    data->stream = pw_stream_new_simple(
        pw_main_loop_get_loop(data->loop),
        "linuxkeeper",
        props,
        &stream_events,
        ctx
    );

    if (!data->stream) {
        LK_ERROR("PipeWire: cannot create stream");
        pw_main_loop_destroy(data->loop);
        data->loop = NULL;
        return -1;
    }

    data->stride = ctx->channels * sizeof(int16_t);
    data->n_frames = ctx->buffer_size;

    uint8_t buffer[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    const struct spa_pod *params[1];

    struct spa_audio_info_raw info = SPA_AUDIO_INFO_RAW_INIT(
        .format = SPA_AUDIO_FORMAT_S16_LE,
        .channels = ctx->channels,
        .rate = ctx->sample_rate
    );
    params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);

    int ret = pw_stream_connect(data->stream,
        PW_DIRECTION_OUTPUT,
        PW_ID_ANY,
        PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS,
        params, 1);

    if (ret < 0) {
        LK_ERROR("PipeWire: stream connect failed: %s", strerror(-ret));
        pw_stream_destroy(data->stream);
        pw_main_loop_destroy(data->loop);
        data->stream = NULL;
        data->loop = NULL;
        return -1;
    }

    ctx->running = 1;
    LK_INFO("PipeWire: stream connected on '%s' at %u Hz, %u ch",
            device ? device : "default", ctx->sample_rate, ctx->channels);
    return 0;
}

static int pw_write_silence(AudioContext *ctx) {
    PwData *data = (PwData *)ctx->backend_data;
    if (!data->loop) return -1;

    /*
     * Run a single iteration of the PipeWire main loop.
     * The on_process callback fills buffers with silence.
     * We use a short timeout (10ms) to pace the loop without busy-spinning.
     */
    struct pw_loop *l = pw_main_loop_get_loop(data->loop);
    int ret = pw_loop_iterate(l, 10); /* 10ms timeout */
    if (ret < 0) {
        LK_DEBUG("PipeWire: loop iterate error: %d", ret);
    }

    return data->stream_ready ? 0 : -1;
}

static void pw_close_stream_impl(AudioContext *ctx) {
    PwData *data = (PwData *)ctx->backend_data;
    if (data->stream) {
        pw_stream_destroy(data->stream);
        data->stream = NULL;
    }
    if (data->loop) {
        pw_main_loop_destroy(data->loop);
        data->loop = NULL;
    }
    data->stream_ready = 0;
    ctx->running = 0;
    LK_DEBUG("PipeWire: stream closed");
}

static void pw_backend_destroy(AudioContext *ctx) {
    PwData *data = (PwData *)ctx->backend_data;
    if (data) {
        pw_close_stream_impl(ctx);
        free(data);
        ctx->backend_data = NULL;
    }
    pw_deinit();
}

static int pw_list_devices(AudioContext *ctx) {
    (void)ctx;
    printf("PipeWire output devices:\n");
    printf("  (Use 'pw-cli list-objects Node' or 'wpctl status' to list devices)\n");
    printf("  Tip: set target_devices to the node name from 'wpctl status'\n");
    return 0;
}

const AudioBackend lk_backend_pipewire = {
    .name         = "pipewire",
    .init         = pw_backend_init,
    .open_stream  = pw_open_stream,
    .write_silence = pw_write_silence,
    .close_stream = pw_close_stream_impl,
    .destroy      = pw_backend_destroy,
    .list_devices = pw_list_devices,
};

#endif /* WITH_PIPEWIRE */
