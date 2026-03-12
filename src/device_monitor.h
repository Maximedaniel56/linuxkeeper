#ifndef LK_DEVICE_MONITOR_H
#define LK_DEVICE_MONITOR_H

#include "config.h"
#include "audio.h"

typedef struct DeviceMonitor DeviceMonitor;

typedef void (*DeviceCallback)(const char *device_name, int added, void *userdata);

/* Create a device monitor (uses PA/PW sink monitoring if available) */
DeviceMonitor *lk_devmon_create(AudioContext *ctx, const Config *cfg);

/* Set callback for device changes */
void lk_devmon_set_callback(DeviceMonitor *mon, DeviceCallback cb, void *userdata);

/* Poll for device changes (non-blocking). Returns number of changes. */
int lk_devmon_poll(DeviceMonitor *mon);

/* Destroy monitor */
void lk_devmon_destroy(DeviceMonitor *mon);

#endif
