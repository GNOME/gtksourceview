## Process this file with automake to produce Makefile.in

INCLUDES =            \
  $(DEPRECATED_FLAGS) \
  $(DEP_CFLAGS)
#  $(WARN_CFLAGS) was removed because blocked compilation

noinst_LTLIBRARIES = libpcre.la

libpcre_la_SOURCES = 	\
	dftables.c \
	pcre_chartables.c \
	pcre_compile.c \
	pcre_config.c \
	pcre_dfa_exec.c \
	pcre_exec.c \
	pcre_fullinfo.c \
	pcre_get.c \
	pcre_globals.c \
	pcre_info.c \
	pcre_ord2utf8.c \
	pcre_printint.c \
	pcre_refcount.c \
	pcre_study.c \
	pcre_tables.c \
	pcre_try_flipped.c \
	pcre_ucp_findchar.c \
	pcre_valid_utf8.c \
	pcre_version.c \
	pcre_xclass.c \
	ucp_findchar.c \
	ucp.h \
	config.h \
	pcre.h \
	pcre_internal.h \
	pcre_scanner.h \
	ucpinternal.h

EXTRA_DIST =	\
	pcre_maketables.c \
	ucptable.c
