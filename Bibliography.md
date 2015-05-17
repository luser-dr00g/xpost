# Introduction #

My first exposure to postscript was as a graphics format, a **black box**. You could view them on unix workstations at school with 'gs' or 'ghostview' (which on some documents could actually go backwards to previous pages). This was about 1997, as I was switching from English/Music to ComputerScience/FineArt. Most of the school labs had 486 IBMs running Windows 3.11. So the solaris boxes were miles ahead. Macs were confined to special purposes: the newspaper, and clubs with newsletters.

It wasn't until I took the Computer Graphics course that I discovered Postscript was a language (in the appendix of F.S. Hill, _Computer Graphics using OpenGL._).

I bought _Postscript by Example_ and devoted most of my time to writing all my papers and reports in Postscript (and debugging the horrible code that resulted). I suffered a major setback when one night I was mugged and lost this book and my Apple Newton and my HP48.

# NeWS #

Some time later, I stumbled upon PSIBER (http://www.donhopkins.com/drupal/node/97) and my interest was reawakened. While Don Hopkins found his inspiration in William Gibson's _Neuromancer,_ mine is Herman Hesse's _The Glass Bead Game._

Now, to run PSIBER, requires NeWS. So I tracked down these books which contain substantial information about NeWS.

  * Stephen G. Kochan and Patrick H. Wood. _UNIX Networking,_ 1989. A chapter is devoted to NeWS programming, showing examples of creating a clickable button and running a server procedure to monitor an input stream.
  * Allan Davison, et al. _Distributed Window Systems,_ 1992. Roughly 1/3 of the book is devoted to NeWS including classes and the toolkit.
  * James Gosling, et al. _The NeWS Book,_ 1989. A vision of what 1994 might have looked like, had September 1993 ended properly :)

# Postscript #

For the Postscript interpreter, I've made use of these books.

  * Rae A. Earnshaw, _Workstations and Publication Systems,_ 1987. Particularly the paper, C.A.A. Goswell, "An Implementation of POSTSCRIPT," which describes the RALpage ps interpreter, widely known on unix systems simply as 'ps' until ghostscript really took hold around version 3. At that point the community stopped contributing patches to RALpage. But it exists out there in archives and the source is much easier to navigate than the gs sources.
  * Adobe, _Postscript Language Reference Manual, 2nd Edition,_ 1990. I started with this manual as my primary reference, expecting it to be simpler than the 3rd edition, and expecting the Display Postscript sections to be relevant. While I have referred to the DPS parts on occasion, I found the 1st edition to be vastly simpler and easier to attack systematically. The 1st edition is roughly the same size as K&R.
  * Frank Merritt Braswell, _Inside Postscript,_ 1989. Very useful description of the parts of Postscript which are implemented in Postscript itself; "=", "==", error-handling, interaction with printer menu controls, the server loop, organization of internal dictionaries. Essential reading for implementing a correctly-functioning interactive executive.

# 'C'-level programming. Systems, libraries, runtimes #

  * Kernighan and Ritchie, _The C Programming Language, 2nd Edition,_ 1988. While I've made use of some C99 constructs, this is the book in my lap when writing C code.
  * P.J. Plauger, _The Standard C Library,_ 1992. Too much information. But sometimes that's a good thing.
  * Maurice Bach, The Design of the Unix Operating System.
  * Lion's Commentary on Unix.
  * Marvin Minsky, Finite and Infinite Machines.
  * Numerous books on Programming Language implementation from Automata Theory, Parsers, AST-generation, Evolution of Lisp, etc.

# X11 #

  * Adrian Nye, _X Window System, vol. 0: X Protocol Reference Manual, X11R5,_ 1992. Very useful to understand how the underlying protocol works. Includes reference tables not in the other volumes.
  * Adrian Nye, _X Window System, vol. 1: Xlib Programming Manual, X11,_ 1995. Thorough introduction and lots of example code.
  * Adrian Nye, _X Window System, vol. 2: Xlib Reference Manual, X11,_ 1993. Full details on every Xlib function.
  * Eric F. Johnson and Kevin Reichard, _X Window Applications Programming, 2ed,_ 1992. A much gentler introduction than the Prog. Manual. Focuses on building application modules to abstract away the underlying complexity of the window interaction, so your program logic can remain cleaner.

# Graphics #

  * Newman and Sproull, _Principles of Interactive Computer Graphics,_ 1979. The Classic in the field.
  * Foley and Van Dam, _Fundamentals of Interactive Computer Graphics,_ 1984. Review of Newman and Sproull, and info on color (which Newman and Sproull lacks).
  * Bill Casselman, _Mathematical Illustrations: A Manual of Geometry and Postscript,_ 2005. Includes explanations of Cohen-Sutherland, Weiler-Atherton, DeCasteljau, Triangulation, and 3D graphics techniques.
  * SIGGRAPH 16(3) 1982: Warnock & Wyatt, "A Device Independent Graphics Imaging Model for Use with Raster Devices".