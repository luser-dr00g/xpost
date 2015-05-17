# Introduction #

Xpost has gone through several re-writes as my knowledge and code-base have grown. This is an attempt to describe the thread the major changes have followed. Memory management issues have motivated nearly every one.

My primarily methodologies come from The Pragmatic Programmer. The DRY principle (Don't Repeat Yourself). The Tracer-Bullet strategy (Instead of Top-Down or Bottom-Up, you build a narrow pillar straight through the architecture so the basic functionality is directly testable).

# flips #

flips was the first. _**Way**_ too ambitious, doomed many times over to failure. But you have to start somewhere.

The intention was to write a forth interpreter in C. Then a lisp interpreter in forth to run on top of it. Then a Postscript interpreter in lisp to run on top of it. Then various functionality could be moved up or down language-levels to tweak performance. That was the idea. 'F'orth 'LI'sp 'PS' and because the syntax **flips** as you descend levels.

The result was a primitive postscript interpreter that just malloc'ed and forgot. A memory sinkhole.

But I was able to get it to tokenize (oh, the tangles of spaghetti!). And I was able to get it to draw lines in an Xwindow (with still more memory fumbling in the ad-hoc display-lists I was using as a "path".

# podvig #

Discarding flips. podvig, name from Russian title of the Nabokov novel, _Glory_. As Nabokov describes in the preface, podvig actually translates as 'exploit'. But it's the **noun** 'exploit' (feat of derring-do, escapade, fait-accompli) _not_ the **verb** 'exploit' (utilize, manipulate, enslave).

podvig started with a better overall design (multiple files, for one). I designed structures and then wrote code rather than the reverse. But I neglected memory again until after it got to about the same level of functionality as flips. And then I tried to add reference-counting to the existing code. And it broke. There's still a previous revision in RCS. But when that happened, I knew it was a sign that memory was going to be my continual problem until I made it primary. The design needed to _start_ with memory. Memory management needed to be the very first module, upon which all others would rest.

I got help on comp.lang.c with the "embarrassing spaghetti code" (that's the search term if you want to read about it) and the scanner has kept the same basic form ever since, changing interface as the design changes.

Also, the importance of save/restore was now apparent to me. There was no way to retrofit podvig with that capability. It, too, would need to be part of the initial design.

# xpost0 #

So xpost started with a simple allocator in an anonymous mmap.
Arrays and Strings were added (to the same file. _mea culpa_). And then the loop and the operator handler and then I broke the operators out into modules.

Then I implemented save and restore with a stack of mmaps. This had the advantage that memory would actually be returned to the operating system by a restore. But I was informed that save and restore, properly done, should be quite cheap. But little else to guide me.

One of these I posted for review at comp.lang.c and got "more comments". Which I keep hearing. Talk more, Josh. Speak up for yourself.

# xpost1 #

There never was an xpost1. I decided starting at zero was too depressing. And it really wasn't zero because I had the experience of flips and podvig already. So I just skipped one (or re-named zero as one, if you like).

# xpost2 #

So xpost2 started with a simple allocator and now save/restore would draw a line across vm and "make no changes below the line". This eventually wasn't quite true because the way I made it work was that objects when copied-on-write would fill in a "forwarding address" pointer to the new writeable version. Thus all accesses to composite object would follow these chains of pointers to get to the data. That's the best I could come up with.

The behavior of arrays and dictionaries was thus tightly bound to the virtual-memory routines and they all went into a single big module, the "core". This was something of a step backwards design-wise. Another step backwards was putting all operator functions in a single file (with a separate file for graphics operators and later another one for font operators).

Xpost2 implemented nearly all of the PLRM 1ed operators. Lacking definefont, strokepath, and a few others. But those forwarding addresses kept nagging at me. And the main loop was too clever by half. It tried to **pipe-line** the execution by checking types with if-blocks rather than a switch. So if the input was an executable name, and that name define a procedure, and that procedure began with an operator, the operator would be executed in the first iteration of the loop. Profiling showed lots of time spent in this function. lol.

# xpost3 #

This time I investigated deeper into the word **virtual** from "virtual memory", and why postscript's description under that name doesn't quite jive with what everyone else describes by that term. Well, why is that? It ain't! Virtual-addresses :: Virtual-memory. It means don't use pointers. Don't do it! Why? Don't ask why! How? That's better.

Something/anything needs to go between the **handle** and **data**. The pointer is just a straight line. What you need is a folding ruler. You gotta bend that thing at an angle and say, "now you and these other related things will move over here or be copied or have your real addresses modified in some way". It means virtual page-translation tables at the object allocation level. It means addresses are in a memory table and the postscript object contains a **virtual address**, a table index (and maybe an offset).

Xpost3 (now called xpost 0.0.1 under the new "release" plan) is also different from previous versions in attempting to conform to ISO C90 for maximum compatibility, use of Autotools (thanks, Vincent) for cross-platform compatibility between POSIX and Win32, and attempting to migrate the entire program into the library portion. The non-linear control flow using setjmp/longjmp for error handling has been replaced by returning error codes. There is some inconsistency with this, but mostly, external interfaces return 1 for success and 0 for failure; internal functions return 0 for "no error" and non-zero for a postscript error identifier.