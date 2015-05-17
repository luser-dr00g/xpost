Xpost is an interpreter for the postscript language level-one (working on level-two) as defined by the first edition (& second edition) of the Adobe postscript manual. The ultimate goal is to extend xpost into a
postscript based window-system and debugging environment, a clone of NeWS. The dead system that was so great that even 18 years after its end-of-line, nobody's willing to sell their old manuals.

Update 2011. I got the manuals! Two of them, at least. Still need the tNt one, and Usenix 86. But it's a start. I put xpost2 on hold for a few months to work up the innards of xpost3 (with garbage collection), but now I'm back trying to finish xpost2. I'll probably alternate every few months between these two efforts.

Update Dec 2011. I got the tNt manual. It should arrive in the mail today or tomorrow. I still need the OOPs stuff from Usenix Graphics Workshop 1986 or EuroGraphics Forum 1987. But I might have some of that on a CD somewhere.

Update Aug 2012. Found the CD, but couldn't upload the whole news-tape for some reason. But the OOPS paper was in it, so the Genie's out! But I'm too scattered at the moment to catch him. With all those dictionaries flying around, xpost really needs a garbage collector. I've got one drafted for xpost3, but I fizzled out trying to find the Ultimate Inner Exec Loop because gprof says xpost2 always spends the most time in 'opexec()'.

Update Dec 2012. Old computer died. Files safe. Putting xpost on hold while playing with new computer. Possible windows port in future.

Update Apr 2013. Rebuild on Cygwin using the Hg repository instead of RCS. Now all the source is browsable! http://code.google.com/p/xpost/source/browse/

Update Oct 2013. Collaborator Vincent Torri has incorporated the Autotools build system, with support for Doxygen, Check, Splint, and much, much more. The application code is being carefully converted to function as a callable library, with improved naming conventions.

Update Nov 2013. Added support for magic dictionaries, the first NeWS extension!

> `wc` reports 19756 lines of code. 12078 in the application (`src/bin/*.[ch]`), 3702 in the library (`src/lib/*.[ch]`), 3988 in postscript (`data/*.ps`). We're working to move `/bin` files to `/lib`. At some point, some of the postscript might be translated to `/bin`.

Updata: Created a discussion group, https://groups.google.com/forum/#!forum/xpost-discuss
for more dynamic updates.

Update Jan 2014. `make count` reports 30559 lines of code (includes 1000-line glob.c file which we did not write; everything else I (or Vincent) wrote). `make count |grep '\.[ch]' | awk 'BEGIN{j=0} {print; $0 = $1; gsub(" ",""); j += $0} END{print j}'` reports 23473 lines in just the C files, and `make count |grep \\.ps | awk 'BEGIN{j=0} {print; $0 = $1; gsub(" ",""); j += $0} END{print j}'` reports 7086 lines of code in postscript, some of which is not integral to the interpreter but is the ephemera of testing (eg. 104 ./data/dancingmen.ps). So it's a "big" program now, somewhat stable, not-quite-complete.

Xpost can now execute the ps program of its own logo: http://code.google.com/p/xpost/downloads/detail?name=xpostlogo.eps
and it displays the logo in its window under Ms Windows (thanks, Vincent!).

Update Oct 2014. Subjected (submitted?) the xpost\_client example program to http://codereview.stackexchange.com/questions/65061/rfc-ps-rasterizer-library-api under the title RFC PS Rasterizer Library API.