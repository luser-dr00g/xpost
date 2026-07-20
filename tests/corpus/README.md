Differential corpus
===================

A set of real PostScript programs, rendered through xpost and through a
reference interpreter and compared, to catch rendering regressions that
the unit suite does not reach. The programs themselves are **not** kept
in this repository: they belong to other people, or are generated, and
committing them would raise a licensing question and bloat the tree.
Instead each corpus is a directory here that holds only what is ours --
a small compatibility prelude where one is needed -- and its programs
are fetched or copied into it on demand. Every step degrades to a skip
when a corpus is absent, so none of it is a build-time dependency.

    fetch.sh [name ...]      populate the corpora from their own sources
    evaluate.sh [name ...]   render and compare whatever is present

Run them from anywhere; both locate the repository themselves. With no
arguments each acts on every corpus it knows.

The corpora
-----------

  ghostscript   Ghostscript's own examples/ -- tiger, colorcir,
                doretree, escher, snowflake, and the rest. Dense
                real-world artwork: the classic interpreter torture
                pages. Fetched from the Artifex ghostpdl repository
                (AGPL); not redistributed here.

  casselman     The PostScript chapters of Bill Casselman's
                "Mathematical Illustrations: A Manual of Geometry and
                PostScript" (Cambridge University Press), typeset in
                PostScript by their author. dvips output with embedded
                Type 1 fonts: a real-document stress test. Fetched from
                personal.math.ubc.ca; copyright the author, not
                redistributed here.

  bwipp         The variable-data examples from BWIPP (Terry Burton's
                barcode writer, MIT), driven off a local checkout. They
                repeat a compute-intensive logo and barcode across many
                pages through PostScript forms, so they exercise the
                form cache; the packaged resource loads its data through
                125 ASCII85Decode filters, so it exercises the decode
                path too. The monolithic resource is copied in as this
                corpus's prelude (large and generated, so not committed).
                Point BWIPP=/path/to/postscriptbarcode at your checkout
                (default ~/src/postscriptbarcode) and build its
                monolithic resource first.

  adobe         The sample code of Adobe's PostScript books -- the Blue
                Book (Tutorial and Cookbook) and Green Book (Program
                Design) listings -- and the DeviceN, halftone and
                masked-image technical-note examples. Each listing
                targets one named feature of a specification section, so
                a divergence points straight at the operator
                responsible; this is the compliance workhorse. Adobe
                holds the copyright and no canonical download survives,
                so this corpus is NOT fetched: place your own copy under
                adobe/ (see SOURCES) and the evaluator will pick it up.

Not included
------------

  Real World PostScript (Roth, Addison-Wesley 1988) has no surviving
  example-code distribution -- only the scanned book -- so there is
  nothing to fetch.

Preludes
--------

Where a corpus assumes something outside the language -- Ghostscript's
examples use the min/max operators and their internal dot-forms, which
Ghostscript provides but the PLRM does not; the BWIPP examples need the
barcode resource loaded first -- a file named `prelude` in the corpus
directory is prepended to every program of that corpus. It is prepended
to *both* engines, so the compared input stays identical and the shim
is not itself under test. A small prelude that is ours (ghostscript's)
is committed; a large generated one (bwipp's barcode resource) is not.

Evaluation
----------

Each program renders at 72 dpi on a letter page through xpost and
through Ghostscript, and the two rasters are compared page by page.
Colour pages compare by pixel count at a small fuzz (structural
difference, tolerant of sub-pixel edges); halftone and pattern pages
compare by tint at a coarse resize, because two correct screens differ
dot for dot but hold the same tone.

A difference is a lead, not a verdict. Ghostscript is informative;
where a deviation matters, Adobe Distiller is authoritative. Some
divergences are expected and not defects:

  - rand/srand pages (Ghostscript's snowflak and vasarely) draw
    different figures in every interpreter, the PLRM leaving the
    generator implementation-defined;
  - CMYK and DeviceN colour differs between Ghostscript's colour-managed
    render and the PLRM arithmetic xpost follows;
  - masked images under /Interpolate differ by design across
    implementations.

SOURCES
-------

  ghostscript  https://github.com/ArtifexSoftware/ghostpdl  (examples/)
  casselman    https://personal.math.ubc.ca/~cass/graphics/manual/
  bwipp        https://github.com/bwipp/postscriptbarcode  (contrib/Examples,
               build/monolithic_package/barcode.ps)
  adobe        Adobe's "PostScript Language Tutorial and Cookbook" (Blue
               Book) and "PostScript Language Program Design" (Green
               Book) sample code, and the DeviceN / halftone /
               masked-image Technical Notes. Historically on Adobe's
               developer FTP; obtain from an archive you trust and
               arrange under adobe/ as flat *.ps files.
