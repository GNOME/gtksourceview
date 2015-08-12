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
	styles.pot

$(srcdir)/styles.pot: $(STYLES) styles.its
	$(ITSTOOL) -i styles.its -o $@ $(STYLES)

-include $(top_srcdir)/git.mk
