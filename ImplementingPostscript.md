# Introduction #

Re-reading Crispin Goswell's paper from Earnshaw, Workstations and Publication Systems, I am struck by a footnote: "token is one of many key operators in PostScript whose presence have a controlling influence on an implementation". It's true. My very first attempt (flips) foundered on this very sandbar. So this document is an attempt to set out some of the "gotchas" that may not be entirely obvious to a PostScript user attempting to construct a working interpreter. Goswell's full list of key operators is: token, clippath, strokepath, flattenpath. I'll get to these eventually, but there are a few more, IMO.

# token #

token implements the entire language scanner as a single operator (or procedure), it is responsible for determining the type of the source object (file or string) and returning the remainder of input as an additional return value if the source type is a string. For a file, the same essential result is achieved by adjusting the read-head in the file object (since this object is discarded by token, most of the file metadata must be in "shared" memory, not the object structure itself).

There are various other operators which perform the essential conversions required by token. After determining the type of object, token should (in order to be well-factored) use cvi, cvr, and cvn to produce integers, reals, and name objects.

# getinterval #

> Don't consider an implementation where scalar values require pointers
(and hence memory management).  A single PostScript object should
include its type and status (literal, etc.), a scalar value or a
composite pointer. There will need to be more.

> Before you commit to a layout, be sure you understand the full
semantics of getinterval, which must return an object that is a
pointer to a subset of the data which remains in another valid object.
This can blow an implementation completely out of the water if you
haven't thought through how it will work.

-- Aandi Inston (in comp.lang.postscript)
https://groups.google.com/d/msg/comp.lang.postscript/NYw4teLYQKg/CQH3ioxoRDEJ

getinterval is responsible for sub-arrays and sub-strings. In order for this operator to work, it must be possible for an array object to share part of another array. My personal interpretation of this is that the length and offset need to be in the object, not off in virtual memory with the array contents.

# save/restore #

save and restore create 'snapshots' of the current state of composite objects in VM (the contents of arrays and dictionaries, but not strings).

The first xpost attempted to implement this with a stack of memories. Objects were allocated linearly in the current memory. The save would make a copy of the entire memory, push the current pointer on a stack and install a new pointer as the vm pointer. So new allocations would go into a duplicate memory. Restore would pop this stack and return the mmap to the operating system. This approach was abandoned as very heavy-weight. Restore was cheap, but copying the entire vm for a save was considered inefficient.

Xpost2 implemented save/restore with a single linear allocator for vm. Objects were allocated linearly in the memory. Then save would allocate a marker in vm and take a pointer to this marker. And restore would rewind the top of memory to this marker. Arrays and dictionaries contained forwarding addresses to the latest copy. Restore had a bit of a chore to examine these entries, while scanning through memory _backwards_ and remove forwarding addresses that pointed past the new top-of-memory. There were trailers added to allocations to facilitate this backwards scan. And there were even magic numbers to recognize things.

Xpost3 implements VM with "page-tables". A composite object structure contains an "ent" (entity) number which indexes a memory table, and this table contains the address of the data itself. Having two stages means that all shared objects (eg. two duplicates of an array) contain the same indirect reference to the array contents, passing through the memory tables. Save and restore are implemented as a stack of (source,copy) pairs which relate the _active_ object with its saved copy at the current save level. By performing the "is it saved?" check in the array (and dict) setter operators (or more properly, the internal array\_put (and dict\_put) functions that all the related operators use), xpost3 achieves _copy-on-write_ semantics.

# pathforall #

For pathforall to work easily, the path structure must be easily traversed. And in order to maintain the appropriate local state inside the operator function (the working pointer running through the path list), it appears necessary to invoke the main-loop of the interpreter recursively. This in turn may create problems with the error handling mechanism, if implemented using setjmp/longjmp. The jump\_buf used by setjmp will need to be stacked so errors return to the error handler of the _correct instance_ of the interpreter. Or the setjmp call needs to be inside the main loop rather than just before it.

Scratch that recursive stuff. The way to do it is "continuations". To call ps from C, push a continuation operator on the exec stack, then the ps code on the exec stack, then return.

# file #

Even now, I'm discovering new, wild powers of the file operator. Level-2 serious complicates everything involved. A file-object doesn't necessarily correspond directly to a FILE**or whatever your implementation language may have in the way of file-handle. It can also access special input routines (%lineedit) (%statementedit), and even vaguely-defined external storage "devices" through the (%device%) special filename string. The device interaction code may be stored in the /IODevice Resource category. (Still learning this bit.)**

# clippath #

clippath appends the current clipping path to the current path.

# strokepath #

strokepath iterates through the path structure and constructs the brush outline of each segment and builds joins and caps.

# flattenpath #

flattenpath iterates through the path structure and replaces curve segments with many approximating line segments.

For these path-iterating operations I chose to implement a special-purpose version of pathforall, .devpathforall, whose only difference is not to itransform all the points. So .devpathforall operates purely in device space.

It might be interesting to readers of this page to see this mock-up of the inner interpreter loop, a few objects, and a continuation-passing non-blocking `read` operator: http://stackoverflow.com/a/20979866/733077  It doesn't reflect anything else on this page, but shows some interesting details of how simple the simple parts can be, IMO.