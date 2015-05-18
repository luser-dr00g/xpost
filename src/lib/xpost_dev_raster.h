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
 * @file xpost_dev_raster.h
 * @brief This file provides the Xpost raster output functions.
 *
 * This header provides the Xpost raster output functions.
 * The raster device is modelled after the BGR device,
 * but provides BGR BGRA ARGB or RGB buffers.
 * @defgroup xpost_library Library functions
 *
 * @{
 */

#ifndef XPOST_DEV_RASTER_H
#define XPOST_DEV_RASTER_H

/**
 * @brief A blue green red alpha pixel
 */
typedef
struct Xpost_Raster_BGRA_Pixel {
    unsigned char blue, green, red, alpha;
} Xpost_Raster_BGRA_Pixel;

/**
 * @brief a blue green red pixel
 */
typedef
struct Xpost_Raster_BGR_Pixel {
    unsigned char blue, green, red;
} Xpost_Raster_BGR_Pixel;

/**
 * @brief a red green blue pixel
 */
typedef
struct Xpost_Raster_RGB_Pixel {
    unsigned char red, green, blue;
} Xpost_Raster_RGB_Pixel;

/**
 * @brief a alpha red green blue pixel
 */
typedef
struct Xpost_Raster_ARGB_Pixel {
    unsigned char alpha, red, green, blue;
} Xpost_Raster_ARGB_Pixel;

/**
 * @brief a generic buffer
 */
typedef
struct Xpost_Raster_Buffer {
    int width, height, byte_stride;
    /*(Xpost_Raster_*_Pixel)*/ char *data[1];
} Xpost_Raster_Buffer;

/**
 * @brief install operator loadrasterdevice in systemdict
 *
 * When run, creates a new operator
 *
 *       width height  newrasterdevice  device
 *
 * which, when run, creates and returns the device
 * instance dictionary.
 */
int xpost_oper_init_raster_device_ops (Xpost_Context *ctx,
                Xpost_Object sd);

/**
 * @}
 */

#endif
