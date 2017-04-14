@CODE_COVERAGE_RULES@

AM_CPPFLAGS = 				\
	-DDATADIR=\""$(datadir)"\" 	\
	-DG_LOG_DOMAIN=\"GtkSourceView\"\
	-DGTK_SOURCE_COMPILATION	\
	-I$(top_srcdir) -I$(srcdir) 	\
	$(DISABLE_DEPRECATED_CFLAGS)	\
	$(WARN_CFLAGS) 			\
	$(CODE_COVERAGE_CPPFLAGS)	\
	$(DEP_CFLAGS)

noinst_LTLIBRARIES = libgtksourcecompletionwords.la

NOINST_H_FILES =				\
	gtksourcecompletionwordsbuffer.h	\
	gtksourcecompletionwordslibrary.h	\
	gtksourcecompletionwordsproposal.h	\
	gtksourcecompletionwordsutils.h

libgtksourcecompletionwords_headers = 		\
	gtksourcecompletionwords.h

libgtksourcecompletionwords_la_SOURCES =	\
	gtksourcecompletionwords.c		\
	gtksourcecompletionwordsbuffer.c	\
	gtksourcecompletionwordslibrary.c	\
	gtksourcecompletionwordsproposal.c	\
	gtksourcecompletionwordsutils.c		\
	$(libgtksourcecompletionwords_headers)	\
	$(NOINST_H_FILES)

libgtksourcecompletionwords_la_CFLAGS =		\
	$(HIDDEN_VISIBILITY_CFLAGS)		\
	$(CODE_COVERAGE_CFLAGS)

libgtksourcecompletionwords_la_LDFLAGS =	\
	$(WARN_LDFLAGS)

libgtksourcecompletionwords_la_LIBADD =		\
	$(CODE_COVERAGE_LIBS)

libgtksourcecompletionwords_includedir = \
	$(includedir)/gtksourceview-@GSV_API_VERSION@/gtksourceview/completion-providers/words

libgtksourcecompletionwords_include_HEADERS =		\
	$(libgtksourcecompletionwords_headers)

-include $(top_srcdir)/git.mk
