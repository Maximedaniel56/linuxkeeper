#ifdef WITH_OSS

#include "audio.h"
#include "config.h"
#include "log.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __linux__
#include <linux/soundcard.h>
#else
#include <sys/soundcard.h>
#endif

typedef struct {
    int fd;
    void *silence_buf;
    size_t buf_bytes;
} OssData;

static int oss_init(AudioContext *ctx, const Config *cfg) {
    (void)cfg;
    OssData *data = (OssData *)calloc(1, sizeof(OssData));
    if (!data) return -1;
    data->fd = -1;
    ctx->backend_data = data;
    LK_DEBUG("OSS backend initialized");
    return 0;
}

static int oss_open_stream(AudioContext *ctx, const char *device) {
    OssData *data = (OssData *)ctx->backend_data;
    const char *dev = device;

    if (!dev || strcmp(dev, "default") == 0)
        dev = "/dev/dsp";

    data->fd = open(dev, O_WRONLY);
    if (data->fd < 0) {
        LK_ERROR("OSS: cannot open '%s': %s", dev, strerror(errno));
        return -1;
    }

    /* Set format: S16_LE */
    int fmt = AFMT_S16_LE;
    if (ioctl(data->fd, SNDCTL_DSP_SETFMT, &fmt) < 0 || fmt != AFMT_S16_LE) {
        LK_ERROR("OSS: cannot set format S16_LE");
        goto fail;
    }

    /* Set channels */
    int channels = (int)ctx->channels;
    if (ioctl(data->fd, SNDCTL_DSP_CHANNELS, &channels) < 0) {
        LK_ERROR("OSS: cannot set channels %d", channels);
        goto fail;
    }

    /* Set sample rate */
    int rate = (int)ctx->sample_rate;
    if (ioctl(data->fd, SNDCTL_DSP_SPEED, &rate) < 0) {
        LK_ERROR("OSS: cannot set rate %d", rate);
        goto fail;
    }
    if ((unsigned int)rate != ctx->sample_rate) {
        LK_WARN("OSS: requested rate %u, got %d", ctx->sample_rate, rate);
    }

    /* Allocate silence buffer */
    size_t period_frames = ctx->buffer_size / 4;
    if (period_frames < 256) period_frames = 256;
    data->buf_bytes = period_frames * ctx->channels * 2; /* S16 = 2 bytes */
    data->silence_buf = calloc(1, data->buf_bytes);
    if (!data->silence_buf) {
        LK_ERROR("OSS: cannot allocate silence buffer");
        goto fail;
    }

    ctx->running = 1;
    LK_INFO("OSS: opened '%s' at %d Hz, %d ch", dev, rate, channels);
    return 0;

fail:
    close(data->fd);
    data->fd = -1;
    return -1;
}

static int oss_write_silence(AudioContext *ctx) {
    OssData *data = (OssData *)ctx->backend_data;
    if (data->fd < 0) return -1;

    ssize_t written = write(data->fd, data->silence_buf, data->buf_bytes);
    if (written < 0) {
        LK_ERROR("OSS: write error: %s", strerror(errno));
        return -1;
    }
    return 0;
}

static void oss_close_stream(AudioContext *ctx) {
    OssData *data = (OssData *)ctx->backend_data;
    if (data->fd >= 0) {
        close(data->fd);
        data->fd = -1;
    }
    free(data->silence_buf);
    data->silence_buf = NULL;
    ctx->running = 0;
    LK_DEBUG("OSS: stream closed");
}

static void oss_destroy(AudioContext *ctx) {
    OssData *data = (OssData *)ctx->backend_data;
    if (data) {
        oss_close_stream(ctx);
        free(data);
        ctx->backend_data = NULL;
    }
}

static int oss_list_devices(AudioContext *ctx) {
    (void)ctx;
    printf("OSS output devices:\n");

    const char *devs[] = { "/dev/dsp", "/dev/dsp1", "/dev/dsp2", "/dev/audio", NULL };
    for (int i = 0; devs[i]; i++) {
        struct stat st;
        if (stat(devs[i], &st) == 0) {
            printf("  %s\n", devs[i]);
        }
    }
    return 0;
}

const AudioBackend lk_backend_oss = {
    .name         = "oss",
    .init         = oss_init,
    .open_stream  = oss_open_stream,
    .write_silence = oss_write_silence,
    .close_stream = oss_close_stream,
    .destroy      = oss_destroy,
    .list_devices = oss_list_devices,
};

#endif /* WITH_OSS */
