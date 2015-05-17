# Introduction #

**XPOST** is a nascent postscript interpreter. The goal is
to recreate the functionality of the Sun NeWS Window System
(http://en.wikipedia.org/wiki/NeWS)
by extending the interpreter to use the NeWS operators
and then looting the archive of nntp:comp.windows.news (https://groups.google.com/d/topic/comp.windows.news/Bk4ukVJYlKU/discussion).

**Xpost** is written in C.

Xpost2 made extensive use of C99 features:
variable-length arrays to avoid using malloc for quick temporary
strings; designated initializers and complex literals to avoid
a lot of one-use variables; the inline keyword for small filter
functions to reduce use of macros and improve readability.
But it also uses anonymous structures to access flags individually
or collectively; there were portability issues with this area by at least 2 parties who contacted me.

So, Xpost3 is being re-written to be totally-clean ANSI-C, using POSIX for OS support. My development environment is now Cygwin, and I intend to try stripping it down for use with Mingw as well.

Xpost3 is also be written to support Level-2 features: growing stacks, growing dictionaries. Please see http://code.google.com/p/xpost/source/browse/README for details.

# Details #

The **xpost2** interpreter executes all the level-1 postscript
operators for
  * stack manipulation
  * math
  * arrays
  * strings
  * dictionaries
  * control structures
  * files
  * relational and boolean
  * type conversion
  * virtual memory
  * and matrices

It implements nearly all of the graphics state operators (except halftone
screens), path construction
(except _reversepath_, _strokepath_, and _pathforall_),
and painting. Of these,
_pathforall_ shouldn't be too tricky,
so it should follow along soon. _reversepath_ shouldn't be too hard.
But _strokepath_ might be tricky; Cairo doesn't appear
to offer an api function for this (although it appears that they're
working on it). I don't know how to make
cairo do halftones.

Font support is minimal and pretty shaky.

It can select among a few output devices supported by Cairo: X11 window, PDF file, null-device (paints to an internal bitmap which simply isn't displayed anywhere). It's also possible to run with no device at all, but many graphics operators will return wrong results.

> # The Story #

Inspired by Herman Hesse's The Glass Bead Game, Don Hopkins' PSIBERspace [(Google this now! That is an order. Seriously, if you're here reading this, you need to be there reading that. Then you'll understand.)], InterlispD, SmallTalk, Lego Building Blocks. The goal is a fully-GUI-interactive Postscript Graphical Programming Development System. PSalter meets Illustrator. Where you can sketch a polygon with some mouse-clicks, open the edit-source-code window, attach gizmos to various numbers so each tweak causes instantaneous changes in the graphical window, etc, etc.

> # Phase 2 is the document processor #

After polishing off this above trivial task, I intend to reimplement TeX in postscript. I have gone through a great many drafts of typesetting programs.

> # Phase 3 is the GUI word processor #

More Lyx, less Word. As much homage to WordPerfect as can be managed. A WP-style "reveal-codes" button will expose the document language for live editing (WYSIWY(M/G))

> # Phase 4 is the cross product: A Literate Self-Hosting Quine of the entire above system #

The program will contain its own source and have the ability to reproduce (and elucidate) itself.

And that's where I'll make my millions. The program, of course, will be free. It's the book that I'll sell.

In a hundred years or so.

:)