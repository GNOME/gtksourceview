if BUILD_EGG_REGEX

if !USE_SYSTEM_PCRE
SUBDIRS = pcre
pcre_lib = pcre/libpcre.la
pcre_incl = -I$(srcdir)/pcre
else
pcre_lib = $(PCRE_LIBS)
pcre_incl = $(PCRE_CFLAGS)
endif

INCLUDES =			\
	$(DEPRECATED_FLAGS)	\
	$(DEP_CFLAGS)		\
	$(pcre_incl)

noinst_LTLIBRARIES = libeggregex.la

libeggregex_la_LIBADD =		\
	$(DEP_LIBS)		\
	$(pcre_lib)

libeggregex_la_SOURCES =	\
	eggregex.c		\
	eggregex.h

endif
