#include "device_monitor.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>

struct DeviceMonitor {
    AudioContext *ctx;
    const Config *cfg;
    DeviceCallback callback;
    void *userdata;
};

DeviceMonitor *lk_devmon_create(AudioContext *ctx, const Config *cfg) {
    DeviceMonitor *mon = (DeviceMonitor *)calloc(1, sizeof(DeviceMonitor));
    if (!mon) return NULL;
    mon->ctx = ctx;
    mon->cfg = cfg;
    return mon;
}

void lk_devmon_set_callback(DeviceMonitor *mon, DeviceCallback cb, void *userdata) {
    if (mon) {
        mon->callback = cb;
        mon->userdata = userdata;
    }
}

int lk_devmon_poll(DeviceMonitor *mon) {
    (void)mon;
    /*
     * Device monitoring is backend-specific:
     * - PulseAudio/PipeWire: subscribe to sink events via the mainloop
     * - ALSA: poll /proc/asound or use inotify on /dev/snd
     * - OSS: poll /dev/dsp existence
     *
     * For the initial implementation, monitoring is handled within
     * the daemon loop via stream error detection and reconnection.
     * A full implementation would use backend-specific event APIs.
     */
    return 0;
}

void lk_devmon_destroy(DeviceMonitor *mon) {
    free(mon);
}
