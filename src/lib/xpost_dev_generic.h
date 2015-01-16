/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
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

/**
 * @file xpost_dev_generic.h
 * @brief This file provides utilify functions for all devices.
 *
 * This header provides utility functions for all devices.
 * It allows a device to specify a filename after creation.
 * And implements lower-level sorting and polygon filling
 * routines for speed.
 * @defgroup xpost_library Library functions
 *
 * @{
 */

#ifndef XPOST_DEV_GENERIC_H
#define XPOST_DEV_GENERIC_H

/**
 * @brief convenience function to retrieve filename associated with device
 *
 * returns malloc'ed string. caller must free.
 */
char *xpost_device_get_filename(Xpost_Context *ctx, Xpost_Object devdic);

/**
 * @brief convenience function to set the output filename associated with device
 *
 * returns a postscript error code from xpost_error.h, 0 == noerror
 */
int xpost_device_set_filename(Xpost_Context *ctx, Xpost_Object devdic, char *filename);

/**
 * @brief install operator .yxsort to improve performance of 'fill'
 *
 * also C fillpoly implementation that uses device DrawLine method.
 */
int xpost_oper_init_generic_device_ops (Xpost_Context *ctx,
                Xpost_Object sd);

/**
 * @}
 */

#endif
