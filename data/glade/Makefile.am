# Note that we do not install in the detected GLADE_CATALOG_DIR
# since we may not have permissions to do it, for instance if
# compiling with a prefix while glade is installed in /usr.
# We use the variable only to find the dtd file.
# If you install gtksourceview and glade in different prefixes
# and want to use the catalog in glade, you will need to set
# the GLADE_CATALOG_SEARCH_PATH environment variable.
catalogdir = $(datadir)/glade/catalogs
catalog_in_files = gtksourceview.xml.in
catalog_DATA = $(catalog_in_files:.xml.in=.xml)

EXTRA_DIST = \
	$(catalog_in_files)

validate: $(catalog_DATA)
	xmllint --dtdvalid $(GLADE_CATALOG_DIR)/glade-catalog.dtd --noout $<

CLEANFILES = $(catalog_DATA)

@INTLTOOL_XML_NOMERGE_RULE@

-include $(top_srcdir)/git.mk
