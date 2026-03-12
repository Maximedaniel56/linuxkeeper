#ifndef LK_DAEMON_H
#define LK_DAEMON_H

#include "config.h"
#include "audio.h"

/* Daemonize the process (double fork, setsid, etc.) */
int lk_daemonize(const Config *cfg);

/* PID file management */
int lk_pidfile_write(const char *path);
int lk_pidfile_read(const char *path);
void lk_pidfile_remove(const char *path);

/* Signal setup */
void lk_signals_setup(void);

/* Check if signals have been received */
int lk_should_quit(void);
int lk_should_reload(void);
void lk_clear_reload(void);

/* Main daemon loop */
int lk_daemon_run(AudioContext *ctx, Config *cfg);

#endif
