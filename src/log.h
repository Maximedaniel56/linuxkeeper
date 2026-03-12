#ifndef LK_LOG_H
#define LK_LOG_H

typedef enum {
    LK_LOG_DEBUG = 0,
    LK_LOG_INFO,
    LK_LOG_WARN,
    LK_LOG_ERROR
} LkLogLevel;

void lk_log_init(LkLogLevel level, const char *log_file);
void lk_log_set_level(LkLogLevel level);
LkLogLevel lk_log_level_from_str(const char *str);
void lk_log_cleanup(void);

void lk_log(LkLogLevel level, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#define LK_DEBUG(...) lk_log(LK_LOG_DEBUG, __VA_ARGS__)
#define LK_INFO(...)  lk_log(LK_LOG_INFO,  __VA_ARGS__)
#define LK_WARN(...)  lk_log(LK_LOG_WARN,  __VA_ARGS__)
#define LK_ERROR(...) lk_log(LK_LOG_ERROR, __VA_ARGS__)

#endif
