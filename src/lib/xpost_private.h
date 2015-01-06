/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2015, Michael Joshua Ryan
 * Copyright (C) 2015, Vincent Torri
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

#ifndef XPOST_PRIVATE_H
#define XPOST_PRIVATE_H

/**
 * @brief Initialize the log module.
 *
 * @return 1 on success, 0 otherwise.
 *
 * This function initializes the log module. Currently, it only gets
 * the value of the environment variable XPOST_LOG_LEVEL if it
 * exists and create a file stream for dumping errors in the
 * interpreter. It is called by xpost_init().
 *
 * @see xpost_log_quit()
 * @see xpost_init()
 */
int xpost_log_init(void);

/**
 * @brief Shut down the log module.
 *
 * This function shuts down the log module. It is called by
 * xpost_quit().
 *
 * @see xpost_log_init()
 * @see xpost_quit()
 */
void xpost_log_quit(void);

#endif
