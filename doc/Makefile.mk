
.PHONY: doc

PACKAGE_DOCNAME = $(PACKAGE_TARNAME)-$(PACKAGE_VERSION)-doc

if XPOST_BUILD_DOC

XPOST_DOC_CLEANFILES = doc/html/ doc/latex/ $(PACKAGE_DOCNAME).tar*

XPOST_CLEANFILES += $(XPOST_DOC_CLEANFILES)

doc-clean:
	rm -rf $(XPOST_DOC_CLEANFILES)

doc: all doc-clean
	$(DOXYGEN) doc/Doxyfile
	mkdir -p $(PACKAGE_DOCNAME)/doc
	cp -rf doc/html/ doc/latex/ $(PACKAGE_DOCNAME)/doc
	tar cf $(PACKAGE_DOCNAME).tar $(PACKAGE_DOCNAME)/
	bzip2 -9 $(PACKAGE_DOCNAME).tar
	rm -rf $(PACKAGE_DOCNAME)/

else

doc: all
	@echo "Documentation not built. Run ./configure --help"

endif

EXTRA_DIST += \
doc/INTERNALS \
doc/MANUAL \
doc/Doxyfile
