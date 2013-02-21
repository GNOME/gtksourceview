TESTS = check-style.sh

STYLES =		\
	classic.xml	\
	cobalt.xml	\
	kate.xml	\
	oblivion.xml	\
	tango.xml

stylesdir = $(datadir)/gtksourceview-3.0/styles
styles_DATA = $(STYLES) styles.rng

EXTRA_DIST = $(styles_DATA) check-style.sh

-include $(top_srcdir)/git.mk
