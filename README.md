[![linux CI](https://github.com/luser-dr00g/xpost/actions/workflows/linux.yml/badge.svg)](https://github.com/luser-dr00g/xpost/actions/workflows/linux.yml)
[![Build Status](https://drone.io/github.com/luser-dr00g/xpost/status.png)](https://drone.io/github.com/luser-dr00g/xpost/latest)

[The original README has moved to doc/INTERNALS,
and has been superceded by doc/NEWINTERNALS.]

Xpost is a cross-platform interpreter for the PostScript Language
written in C. It has autotools for building on unix systems, and
Visual C solutions for building on Windows. Currently, a windows
build cannot run because there's no installer to coordinate the 
location for the postscript-language data files that are needed.

The core of the interpreter was written by M Joshua Ryan (luser droog).
The autotools build system, logging system, and win32 device were 
written by Vincent Torri. Individual files bear the copyright of their
respective contributors.

Xpost has rudimentary graphics support. MS Windows or X11 windows.
PGM or PPM file output. Or a nulldevice, which executes the drawing
operations and discards the output. A Raster device is in development
which will yield the image data in BGR, BGRA, ARGB, or RGB byte orders
to a calling program which uses xpost as a library component.

Xpost is currently distributed with a BSD-3-Clause licence which may
allow its use in projects where Ghostscript is not available.

The entire interpreter is implemented as a library which is used
by a relatively small application file /src/bin/xpost_main.c. A simpler
example of using the libary is in /src/bin/xpost_client.c.


Quick Installation Instructions.
----- ------------ -------------

Currently the intepreter source is in src/bin and the commands
  ./autogen.sh (or ./configure if autogen.sh has already been launched)
  make
will create the interpreter binary xpost(.exe)? in src/bin.
  make install
will install the application, so xpost can be run as a command.

Many more installation and configuration options desribed in ./INSTALL.


Support.
--------

Questions about Xpost can be addressed in the Google Group xpost-discuss
https://groups.google.com/forum/#!forum/xpost-discuss
