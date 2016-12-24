/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013-2016, Michael Joshua Ryan
 * Copyright (C) 2013-2016, Vincent Torri
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the Xpost software product nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
    xpost_log_print(l, __FILE__, __func__, __LINE__, __VA_ARGS__)

/**
 * @def XPOST_LOG_ERR(...)
 * @brief Log a message with level #XPOST_LOG_LEVEL_ERR on the default
 * domain with the specified format.
 */
#define XPOST_LOG_ERR(...) \
    XPOST_LOG(XPOST_LOG_LEVEL_ERR, __VA_ARGS__)

/**
 * @def XPOST_LOG_WARN(...)
 * @brief Log a message with level #XPOST_LOG_LEVEL_WARN on the default
 * domain with the specified format.
 */
#define XPOST_LOG_WARN(...) \
    XPOST_LOG(XPOST_LOG_LEVEL_WARN, __VA_ARGS__)

/**
 * @def XPOST_LOG_INFO(...)
 * @brief Log a message with level #XPOST_LOG_LEVEL_INFO on the default
 * domain with the specified format.
 */
#define XPOST_LOG_INFO(...) \
    XPOST_LOG(XPOST_LOG_LEVEL_INFO, __VA_ARGS__)

/**
 * @def XPOST_LOG_DBG(...)
 * @brief Log a message with level #XPOST_LOG_LEVEL_DBG on the default
 * domain with the specified format.
 */
#define XPOST_LOG_DBG(...) \
    XPOST_LOG(XPOST_LOG_LEVEL_DBG, __VA_ARGS__)

/**
 * @def XPOST_LOG_DUMP(...)
 * @brief Dump in file a message on the specified level and format.
 */
#define XPOST_LOG_DUMP(...)                                           \
    xpost_log_print_dump(XPOST_LOG_LEVEL_INFO, __func__, __VA_ARGS__)

/**
 * @enum Xpost_Log_Level
 * @brief List of available logging levels.
 */
typedef enum
{
    XPOST_LOG_LEVEL_ERR, /**< Error log level */
    XPOST_LOG_LEVEL_WARN, /**< Warning log level */
    XPOST_LOG_LEVEL_INFO, /**< Information log level */
    XPOST_LOG_LEVEL_DBG, /**< Debug log level */
    XPOST_LOG_LEVEL_LAST  /**< Count of default log levels */
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
XPAPI void xpost_log_print_cb_set(Xpost_Log_Print_Cb cb, void *data);

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
XPAPI void xpost_log_print_cb_stderr(Xpost_Log_Level level,
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
XPAPI void xpost_log_print_cb_stdout(Xpost_Log_Level level,
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
XPAPI void xpost_log_print(Xpost_Log_Level level,
                           const char *file,
                           const char *fct,
                           int line,
                           const char *fmt, ...);

XPAPI void xpost_log_print_dump(Xpost_Log_Level level,
                                const char *fct,
                                const char *fmt, ...);

#endif
