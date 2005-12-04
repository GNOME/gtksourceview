if BUILD_EGG_REGEX

SUBDIRS = pcre

INCLUDES = \
  $(DEPRECATED_FLAGS) \
  $(DEP_CFLAGS)
#  $(WARN_CFLAGS) was removed because blocked compilation

noinst_LTLIBRARIES = libeggregex.la

libeggregex_la_LIBADD =       \
	$(DEP_LIBS)           \
	pcre/libpcre.la

libeggregex_la_SOURCES = \
  eggregex.c

#noinst_PROGRAMS = testregex

#testregex_SOURCES = testregex.c
#testregex_LDFLAGS = \
#	libeggregex.la \
#	$(DEP_LIBS)
#testregex_DEPENDENCIES = libeggregex.la 

endif

noinst_HEADERS = \
  eggregex.h


