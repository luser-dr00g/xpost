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

#ifndef XPOST_DEV_BGR_H
#define XPOST_DEV_BGR_H

typedef
struct Xpost_Bgr_Pixel {
    unsigned char blue, green, red;
} Xpost_Bgr_Pixel;

typedef
struct Xpost_Bgr_Buffer {
    int width, height, byte_stride;
    Xpost_Bgr_Pixel data[1];
} Xpost_Bgr_Buffer;

/**
 * @brief install operator loadxcbdevice in systemdict
 *
 * When run, creates a new operator
 *
 *       width height  newxcbdevice  device
 *
 * which, when run, creates an X window and returns the device
 * instance dictionary.
 */
int xpost_oper_init_bgr_device_ops (Xpost_Context *ctx,
                Xpost_Object sd);

#endif
