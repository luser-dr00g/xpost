#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_STDBOOL_H
# include <stdbool.h>
#else
# ifndef HAVE__BOOL
#  ifdef __cplusplus
typedef bool _Bool;
#  else
#   define _Bool signed char
#  endif
# endif
# define bool _Bool
# define false 0
# define true 1
# define __bool_true_false_are_defined 1
#endif


#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "xpost_memory.h"  // itp contexts contain mfiles and mtabs
#include "xpost_object.h"  // eval functions examine objects
#include "xpost_stack.h"  // eval functions manipulate stacks
#include "xpost_interpreter.h" // uses: context itp MAXCONTEXT MAXMFILE
#include "xpost_error.h"  // interpreter catches errors
#include "xpost_string.h"  // eval functions examine strings
#include "xpost_array.h"  // eval functions examine arrays
#include "xpost_garbage.h"  // interpreter initializes garbage collector
#include "xpost_save.h"  // interpreter initializes save/restore stacks
#include "xpost_name.h"  // eval functions examine names
#include "xpost_dict.h"  // eval functions examine dicts
#include "xpost_file.h"  // eval functions examine files
#include "xpost_operator.h"  // eval functions call operators
#include "xpost_op_token.h"  // token operator functions
#include "xpost_op_dict.h"  // dictionary operator functions
#include "xpost_pathname.h"


