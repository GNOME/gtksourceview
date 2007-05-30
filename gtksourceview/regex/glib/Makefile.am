noinst_LTLIBRARIES = libgregex.la
libgregex_la_SOURCES =	\
	gregex-real.h	\
	gregex.h	\
	gregex.c

if !USE_SYSTEM_PCRE
pcre_cflags = -I$(srcdir)/..
endif

AM_CFLAGS =				\
	-I$(top_builddir)		\
	$(pcre_cflags)			\
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(DEP_CFLAGS)

EXTRA_DIST =			\
	galias.h		\
	galiasdef.c
