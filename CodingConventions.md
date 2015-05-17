# Introduction #

This page lists the Coding Conventions that Xpost source code should follow.

# Details #

Static functions:

<pre> _xpost_modulename_name_action </pre>

> eg.

<pre> _xpost_memory_size_set() </pre>

> The leading `_` is to recognize immediatly a local variable/function


Global functions :

same, without the leading `_`


Function declaration :

<pre>
returned_type function_name(param1,<br>
param2);<br>
</pre>

Function definition :

<pre>
returned_type<br>
function_name(param1<br>
param2)<br>
{<br>
}<br>
</pre>


Variables: same but without the 'action' part

Enum :

<pre>
typedef enum<br>
{<br>
XPOST_MY_ENUM_FOO,<br>
XPOST_MY_ENUM_BAR<br>
} Xpost_My_Enum;<br>
</pre>

Struct :

<pre>
typedef struct<br>
{<br>
type member;<br>
} Xpost_My_Struct;<br>
</pre>

or if it's self-referential:

<pre>
typedef struct _Xpost_My_Struct Xpost_My_Struct;<br>
struct _Xpost_My_Struct<br>
{<br>
type member;<br>
struct _Xpost_My_Struct *ptr;<br>
};<br>
</pre>

Union :

<pre>
typedef union<br>
{<br>
type member;<br>
} Xpost_My_Union;<br>
</pre>