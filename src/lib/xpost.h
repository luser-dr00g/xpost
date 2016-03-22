/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
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

#ifndef XPOST_H
#define XPOST_H

#ifdef XPAPI
# undef XPAPI
#endif

#ifdef _WIN32
# ifdef XPOST_BUILD
#  ifdef DLL_EXPORT
#   define XPAPI __declspec(dllexport)
#  else
#   define XPAPI
#  endif
# else
#  define XPAPI __declspec(dllimport)
# endif
#else
# ifdef __GNUC__
#  if __GNUC__ >= 4
#   define XPAPI __attribute__ ((visibility("default")))
#  else
#   define XPAPI
#  endif
# else
#  define XPAPI
# endif
#endif

#ifdef __cplusplus
extern "C" {
#endif /* ifdef __cplusplus */


/**
 * @file xpost.h
 * @brief This file provides the Xpost API functions.
 *
 * This is the master "include" file which includes
 * all headers in the proper order needed to control
 * xpost features at the top level.
 * @defgroup xpost_library Library functions
 *
 * @{
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
XPAPI int xpost_init(void);

/**
 * @brief Quit the xpost library.
 *
 * @return The new init count.
 *
 * If xpost_init() was called more than once for the running application,
 * xpost_quit() will decrement the initialization counter and return its
 * new value, without doing anything else. When the counter reaches 0, all
 * of the internal elements will be shutdown and any memory used freed.
 *
 * @see xpost_init()
 */
XPAPI int xpost_quit(void);

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
XPAPI void xpost_version_get(int *maj, int *min, int *mic);

/**
 * @brief Return the path of the shared library.
 *
 * @return The path of the shared library.
 *
 * This function returns the path of the shared library.
 */
XPAPI const char *xpost_lib_dir_get(void);

/**
 * @brief Return the path of the data directory, based on the path of the
 * shared library.
 *
 * @return The path of the data directory.
 *
 * This function returns the path of the data directory, based on the shared library. More precisely, it is xpost_lib_path_get()../share/xpost.
 */
XPAPI const char *xpost_data_dir_get(void);

/**
 * @typedef Xpost_Context
 * @brief The context abstract structure for a thread of execution of ps code.
 */
typedef struct _Xpost_Context Xpost_Context;

/**
 * @typedef Xpost_Showpage_Semantics
 * @brief Specify the behavior the interpreter should take when executing `showpage`.
 */
typedef enum {
    XPOST_SHOWPAGE_DEFAULT, /**< Print "----showpage----\n" to stdout
                                 and read and discard a line of text
                                 from stdin (ie. wait for return). */
    XPOST_SHOWPAGE_NOPAUSE, /**< Bypasses this action but still
                                 performs a "flush" of the graphics
                                 device. */
    XPOST_SHOWPAGE_RETURN /**< Causes the interpreter to return
                               control to its caller; the suspended
                               context may be resumed by calling
                               xpost_run with the #XPOST_INPUT_RESUME
                               input type. */
} Xpost_Showpage_Semantics;

/**
 * @typedef Xpost_Output_Type
 * @brief Specify the interpretation of the outputptr parameter to xpost_create().
 */
typedef enum {
    XPOST_OUTPUT_DEFAULT, /**< Ignores outputptr. */
    XPOST_OUTPUT_FILENAME, /**< Treats outputptr as a char* to a
                                zero-terminated OS path string
                                (implemented in pgm and ppm devices). */
    XPOST_OUTPUT_BUFFERIN, /**< Treats outputptr as an unsigned char *
                                and render directly into this memory
                                (not currently implemented). */
    XPOST_OUTPUT_BUFFEROUT /**< Treats outputptr as an unsigned char **
                                and malloc()s a new buffer and assigns
                                it to the unsigned char * which
                                outputptr points to. */
} Xpost_Output_Type;

/**
 * @typedef Xpost_Input_Type
 * @brief Specify the interpretation of the inputptr parameter to xpost_run().
 */
typedef enum {
    XPOST_INPUT_STRING, /**< Treats inputptr as a char * to an
                             zero-terminated ascii string, writes the
                             whole string into a temporary file and
                             falls through to the #XPOST_INPUT_FILEPTR
                             case. */
    XPOST_INPUT_FILENAME, /**< Treats inputptr as a char * to a
                              zero-terminated OS path string, and
                              pushes the path string itself,
                              scheduling a procedure to execute it. */
    XPOST_INPUT_FILEPTR, /**< Treats inputptr as a FILE *, creates a
                               postscript file object and pushes it on
                               the execution stack (scheduling it to
                               execute). */
    XPOST_INPUT_RESUME /**< Bypasses any execution scheduling. */
} Xpost_Input_Type;

/**
 * @typedef Xpost_Set_Size
 * @brief FIXME: to fill...
 *
 * Currently, only "ignore size" is implemented.
 */
typedef enum {
    XPOST_IGNORE_SIZE,
    XPOST_USE_SIZE
} Xpost_Set_Size;

/*
   The is_installed parameter controls whether the interpreter should look
   to the standard locations for its postscript initialization files or
   it should look for these files in "$CWD/data/".
 */

/**
 * @brief Create a newly allocated context.
 *
 * @param device
 * @param output_type
 * @param outputptr
 * @param semantics
 * @param quiet
 * @param is_installed
 * @param set_size
 * @param width The height of the context page.
 * @param height The height of the context page.
 *
 * This function creates a #Xpost_Context with the given
 * parameters. FIXME: give a more detailed explanation...
 *
 * When not needed the context must be freed with xpost_destroy().
 *
 * @see xpost_destroy()
 */
XPAPI Xpost_Context *xpost_create(const char *device,
                                  Xpost_Output_Type output_type,
                                  const void *outputptr,
                                  Xpost_Showpage_Semantics semantics,
                                  int quiet,
                                  int is_installed,
                                  Xpost_Set_Size set_size,
                                  int width,
                                  int height);

/**
 * @brief Execute ps program.
 *
 * @param ctx The context to run.
 * @param input_type The input type to use.
 * @param inputptr
 * @return
 *
 * This function executes a ps program until quit, fall-through to quit,
 * #XPOST_SHOWPAGE_RETURN semantic, or error (default action: message,
 * purge and quit).
 *
 * Depending upon the input type, this function will package the input
 * into an appropriate postscript object and schedule it for execution
 * by marking it executable and pushing to the exec stack, or by
 * pushing to the operand stack and pushing to the exec stack a small
 * program to execute it.
 *
 * For a filename, push a proc to open and execute it.
 *
 * For a string, write to a temp file and fall-through to FILE * case.
 *
 * For a FILE *, mark executable and push to exec stack.
 *
 * As a special-case, if executing a FILE *, and that file is a
 * console or tty, it pushes a proc which launches the postscript
 * `executive` which offers PS> prompts.
 *
 * If an output device (such as a window) has been specified in the
 * call to xpost_create(), it is here in the startup code, where
 * the device is initialized. The device is specified in xpost_create,
 * not because it is needed at that point, but because it is considered
 * a constant for the context, whereas it is intended that a context
 * may be re-used by calling xpost_run upon it again, presumably with
 * differing arguments.
 */
XPAPI int xpost_run(Xpost_Context *ctx,
                    Xpost_Input_Type input_type,
                    const void *inputptr);

/**
 * @brief Destroy the given context.
 *
 * @param ctx The context to destroy.
 *
 * This function destroy the context @p ctx which has been created
 * with xpost_create(). No test is done on @p ctx, so it must be non
 * @c NULL.
 *
 * @see xpost_create()
 */
XPAPI void xpost_destroy(Xpost_Context *ctx);

/**
 * @}
 */


#ifdef __cplusplus
}
#endif /* ifdef __cplusplus */

#endif
