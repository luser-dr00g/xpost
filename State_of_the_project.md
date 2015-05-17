# Introduction #

I suppose the record shows that I have pretty much stalled on updating Xpost2. I've turned to codegolf.stackexchange.com for the small interpreter challenges, trying to hone some chops for when I come back to this.

# Details #

I have a pretty full set of modules for Xpost3 including a garbage collector, indexed-memory using either mmap or malloc. Xpost3 is awaiting inspiration on the ultimate PS interpreter loop, including all the polymorphic dispatch and type-checking in a super-fast, stylish (DRY), concise bit of hackery.

Xpost2 needs definefont. Which presents a dilemma. I've thought about implementing it entirely in postscript, but I fear it would be slow. At the same time, trying to figure out how Cairo expects a custom font to work is rather terrifying. It also needs strokepath and a few other. And a beefed-up set of Level-2 compatibility procs.

So humbly solicit any advice/encouragement/wild-ideas relating to any of the above hurdles. [luser.droog@gmail.com]

Xpost3, after the loop is settled, requires porting of the xpost2 operator set to use the new APIs from the modules. Then it'll need definefont, too. Then, expanding dictionaries, <<>> syntax. Then, comes the fun stuff: round-robin multiprocessing (wait and fork and friends), tuning the garbage collector, tuning the dictionary hash, adding the event queue, NeWS's crazy "magic" dictionaries, classes, the canvas class, the server-loop, uh, what else? PSIBER! Then, my friends, Full Glorious GUI Postscript Debugging!

And then, there's a whole mess of programs in the archive of comp.windows.news (pre-1993) just waiting for a platform. PS animations? PS Games?! An Illustrator with WordPerfect-style 'Reveal Codes'??!!!

This project is about fulfilling the promise of 80s, finally.

16 May 2013: have resumed efforts. Building with the xpost3 modules, the same ulgy-ass operator-handler, a cleaner exec loop. And revision control with Hg instead of the ancient RCS. So the source is browsable!

20 Sept 2013: The basic functionality seems pretty solid. Current efforts are focused on the filesystem interaction and implementing Named Resources to contain device dictionaries.

Investigating Autotools with help from Vincent Torri.

28 Oct 2013: Cross-platform portability is working nicely thanks to the Autotools support files written by Vincent Torri. Slowly turning attention to the missing graphical output.

For the latest news, visit the Xpost discussion group,
https://groups.google.com/forum/#!forum/xpost-discuss