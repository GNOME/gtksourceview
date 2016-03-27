TESTS = check-style.sh

STYLES =			\
	classic.xml		\
	cobalt.xml		\
	kate.xml		\
	oblivion.xml		\
	solarized-dark.xml	\
	solarized-light.xml	\
	tango.xml

stylesdir = $(datadir)/gtksourceview-3.0/styles
styles_DATA = $(STYLES) styles.rng

EXTRA_DIST =		\
	$(styles_DATA)	\
	check-style.sh	\
	styles.its	\
	styles.pot	\
	update-pot.sh

-include $(top_srcdir)/git.mk
