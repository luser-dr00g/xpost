#ifndef EXAMINE_LOG_H
#define EXAMINE_LOG_H

#include <stdarg.h>

#define XPOST_LOG(l, ...) \
  xpost_log_print(l, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

#define XPOST_LOG_ERR(...) \
    XPOST_LOG(XPOST_LOG_LEVEL_ERR, __VA_ARGS__)

#define XPOST_LOG_WARN(...) \
    XPOST_LOG(XPOST_LOG_LEVEL_WARN, __VA_ARGS__)

#define XPOST_LOG_DBG(...) \
    XPOST_LOG(XPOST_LOG_LEVEL_DBG, __VA_ARGS__)

#define XPOST_LOG_INFO(...) \
    XPOST_LOG(XPOST_LOG_LEVEL_INFO, __VA_ARGS__)

typedef enum
{
    XPOST_LOG_LEVEL_ERR,
    XPOST_LOG_LEVEL_WARN,
    XPOST_LOG_LEVEL_INFO,
    XPOST_LOG_LEVEL_DBG,
    XPOST_LOG_LEVEL_LAST
} Xpost_Log_Level;

typedef void (*Xpost_Log_Print_Cb)(Xpost_Log_Level level,
                                   const char *file,
                                   const char *fct,
                                   int line,
                                   const char *fmt,
                                   void *data,
                                   va_list args);
void xpost_log_init(void);

void xpost_log_print_cb_set(Xpost_Log_Print_Cb cb, void *data);

void xpost_log_print_cb_stderr(Xpost_Log_Level level,
                               const char *file,
                               const char *fct,
                               int line,
                               const char *fmt,
                               void *data,
                               va_list args);

void xpost_log_print_cb_stdout(Xpost_Log_Level level,
                               const char *file,
                               const char *fct,
                               int line,
                               const char *fmt,
                               void *data,
                               va_list args);

void xpost_log_print(Xpost_Log_Level level,
                     const char *file,
                     const char *fct,
                     int line,
                     const char *fmt, ...);

#endif /* EXAMINE_LOG_H */
