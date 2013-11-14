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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif



#  if 1
/* This section includes only the files needed by main. */

#include <stdio.h> /* fprintf printf */
#include <stdlib.h> /* EXIT_FAILURE */
#include <string.h> /* free */

#include "xpost_pathname.h" /* xpost_is_installed exedir */
#include "xpost_memory.h" /* Xpost_Memory_File */
#include "xpost_object.h" /* Xpost_Object */
#include "xpost_context.h" /* Xpost_Context */
#include "xpost_interpreter.h" /* xpost_create */
#include "xpost_log.h" /* XPOST_LOG_ERR */


#  else
/* This section demonstrates a "full" header inclusion,
   illustrating the prescribed order of files. */

/* standard headers */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h> /* fprintf printf */
#include <stdlib.h> /* EXIT_FAILURE */
#include <string.h> /* free */

/* /lib headers */
#include "xpost_log.h"
#include "xpost_memory.h"  
#include "xpost_object.h" 
#include "xpost_stack.h" 
#include "xpost_free.h"

/* /bin headers */
#include "xpost_context.h"
#include "xpost_interpreter.h"
#include "xpost_error.h"  
#include "xpost_string.h"
#include "xpost_array.h"
#include "xpost_garbage.h"
#include "xpost_save.h"  
#include "xpost_name.h" 
#include "xpost_dict.h"
#include "xpost_file.h"  
#include "xpost_operator.h"
#include "xpost_op_token.h"  
#include "xpost_op_dict.h"  
#include "xpost_pathname.h"


#  endif 

