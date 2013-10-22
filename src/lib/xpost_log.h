#ifndef XPOST_LOG_H
#define XPOST_LOG_H

#include <stdarg.h>

/**
 * @file xpost_log.h
 * @brief Logging facilities functions
 */

/**
 * @def XPOST_LOG(level, fmt, ...)
 * @brief Log a message on the specified level and format.
 */
#define XPOST_LOG(l, ...) \
  xpost_log_print(l, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

/**
 * @def XPOST_LOG_ERR(fmt, ...)
 * @brief Log a message with level #XPOST_LOG_LEVEL_ERR on the default
 * domain with the specified format.
 */
#define XPOST_LOG_ERR(...) \
    XPOST_LOG(XPOST_LOG_LEVEL_ERR, __VA_ARGS__)

/**
 * @def XPOST_LOG_WARN(fmt, ...)
 * @brief Log a message with level #XPOST_LOG_LEVEL_WARN on the default
 * domain with the specified format.
 */
#define XPOST_LOG_WARN(...) \
    XPOST_LOG(XPOST_LOG_LEVEL_WARN, __VA_ARGS__)

/**
 * @def XPOST_LOG_INFO(fmt, ...)
 * @brief Log a message with level #XPOST_LOG_LEVEL_INFO on the default
 * domain with the specified format.
 */
#define XPOST_LOG_INFO(...) \
    XPOST_LOG(XPOST_LOG_LEVEL_INFO, __VA_ARGS__)

/**
 * @def XPOST_LOG_DBG(fmt, ...)
 * @brief Log a message with level #XPOST_LOG_LEVEL_DBG on the default
 * domain with the specified format.
 */
#define XPOST_LOG_DBG(...) \
    XPOST_LOG(XPOST_LOG_LEVEL_DBG, __VA_ARGS__)

/**
 * @enum Xpost_Log_Level
 * @brief List of available logging levels.
 */
typedef enum
{
    XPOST_LOG_LEVEL_ERR,
    XPOST_LOG_LEVEL_WARN,
    XPOST_LOG_LEVEL_INFO,
    XPOST_LOG_LEVEL_DBG,
    XPOST_LOG_LEVEL_LAST
} Xpost_Log_Level;

/**
 * @typedef Xpost_Log_Print_Cb
 * @brief Type for print callbacks.
 */
typedef void (*Xpost_Log_Print_Cb)(Xpost_Log_Level level,
                                   const char *file,
                                   const char *fct,
                                   int line,
                                   const char *fmt,
                                   void *data,
                                   va_list args);
void xpost_log_init(void);

/**
 * @brief Sets logging method to use.
 *
 * @param cb The callback to call when printing a log.
 * @param data The data to pass to the callback.
 *
 * This function sets the logging method to use with
 * xpost_log_print(). By default, xpost_log_print_cb_stderr() is
 * used.
 */
void xpost_log_print_cb_set(Xpost_Log_Print_Cb cb, void *data);

/**
 * @brief Default logging method, this will output to standard error stream.
 *
 * @param level The level.
 * @param file The file which is logged.
 * @param fct The function which is logged.
 * @param line The line which is logged.
 * @param fmt The ouptut format to use.
 * @param data Not used.
 * @param args The arguments needed by the format.
 *
 * This method will colorize output provided the message logging
 * @p level. The output is sent to standard error stream.
 */
void xpost_log_print_cb_stderr(Xpost_Log_Level level,
                               const char *file,
                               const char *fct,
                               int line,
                               const char *fmt,
                               void *data,
                               va_list args);

/**
 * @brief Default logging method, this will output to standard output stream.
 *
 * @param level The level.
 * @param file The file which is logged.
 * @param fct The function which is logged.
 * @param line The line which is logged.
 * @param fmt The ouptut format to use.
 * @param data Not used.
 * @param args The arguments needed by the format.
 *
 * This method will colorize output provided the message logging
 * @p level. The output is sent to standard output stream.
 */
void xpost_log_print_cb_stdout(Xpost_Log_Level level,
                               const char *file,
                               const char *fct,
                               int line,
                               const char *fmt,
                               void *data,
                               va_list args);

/**
 * @brief Print out log message using given level.
 *
 * @param level Message level.
 * @param file Filename that originated the call, must @b not be @c NULL.
 * @param fct Function that originated the call, must @b not be @c NULL.
 * @param line Originating line in @p file.
 * @param fmt printf-like format to use. Should not provide trailing
 *        '\n' as it is automatically included.
 *
 * This function prints out log message using the given
 * level. Messages with @p level greater than the environment variable
 * EINA_LOG_LEVEL will be ignored. By default,
 * xpost_log_print_cb_stderr() is used.
 */
void xpost_log_print(Xpost_Log_Level level,
                     const char *file,
                     const char *fct,
                     int line,
                     const char *fmt, ...);

#endif
