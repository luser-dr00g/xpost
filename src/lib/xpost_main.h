/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * Copyright (C) 2013, Vincent Torri
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

#ifndef XPOST_MAIN_H
#define XPOST_MAIN_H

/**
 * @file xpost_main.h
 * @brief Initializing and quitting functions
 */


/**
 * @brief Initialize the xpost library.
 *
 * @return The new init count. Will be 0 if initialization failed.
 *
 * The first time this function is called, it will perform all the internal
 * initialization required for the library to function properly and increment
 * the initialization counter. Any subsequent call only increment this counter
 * and return its new value, so it's safe to call this function more than once.
 *
 * @see xpost_quit();
 */
int xpost_init(void);

/**
 * @brief Quit the xpost library.
 *
 * @return The new init count.
 *
 * If xpost_init() was called more than once for the running application,
 * xpost_quit() will decrement the initialization counter and return its
 * new value, without doing anything else. When the counter reaches 0, all
 * of the internal elements will be shutdown and any memory used freed.
 */
int xpost_quit(void);

/**
 * @brief Retrieve the version of the library.
 *
 * @param[out] maj The major version.
 * @param[out] min The minor version.
 * @param[out] mic The micro version.
 *
 * This function stores the major, minor and micro version of the library
 * respectively in the buffers @p maj, @p min and @p mic. @p maj, @p min
 * and @p mic can be @c NULL.
 */
void xpost_version_get(int *maj, int *min, int *mic);

/**
 * @brief Return the start time when library is initialized.
 *
 * @return The start time.
 *
 * This function returns a time set in xpost_init(). It is used to
 * measure the time spent in the Postscript interpreter (in the
 * usertime operator).
 */
double xpost_start_time_get(void);

#endif
