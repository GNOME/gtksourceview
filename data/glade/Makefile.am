cataloguedir = $(GLADE_CATALOGUE_DIR)
catalogue_in_files = gtksourceview.xml.in
catalogue_DATA = $(catalogue_in_files:.xml.in=.xml)

EXTRA_DIST = \
	$(catalogue_DATA)

validate: $(catalogue_DATA)
	xmllint --dtdvalid $(GLADE_CATALOGUE_DIR)/glade-catalog.dtd --noout $<

MAINTAINERCLEANFILES = $(catalogue_DATA)

@INTLTOOL_XML_NOMERGE_RULE@

-include $(top_srcdir)/git.mk
