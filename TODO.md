## short term

implement size parameters to xpost_create().

implement auto-kerning control parameter.
(in each font? implement level2 user-params?)

collapse object flags: extended_int extended_real and opargsinhold.

implement gc controls in xpost_op_param.c:vmreclaim().

add more unit tests

add the infra for coverage reports

add doc to the web site

add Visual Studio installer

## longer term

re-examine the split between xpost_matrix.c and xpost_op_matrix.c.
there's a lot of converting back and forth between matrix formats.

remove optab from vm, thus removing all pointers.
- remove xpost_free_realloc();

extensible ps-init-files search ability.
or resource-compile the files into the executable.

anti-aliasing. Porter/Duff compositing? include alpha-channel?
can we specify rgba in ps with /DeviceN colorspace? 
implement /DeviceN colorspace.

expose Type 1 font data to ps (export in dict).
accept modified Type 1 font dict in `definefont`.

Implement Vatti or Weiler/Atherton clipping for eoclip and clip.
Thus also implementing eofill and fill.

