ACLOCAL_AMFLAGS  = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = gtksourceview po data docs tests

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = gtksourceview-3.0.pc

EXTRA_DIST = \
	intltool-extract.in \
	intltool-merge.in \
	intltool-update.in \
	gtksourceview.doap \
	gtksourceview-3.0.pc.in	\
	gtksourceview-zip.in \
	COPYING.lib \
	HACKING \
	MAINTAINERS \
	ChangeLog.20070526 \
	ChangeLog.soc-2006 \
	ChangeLog-20090418

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update

MAINTAINERCLEANFILES = 		\
	ABOUT-NLS		\
	aclocal.m4		\
	config.guess		\
	config.h.in		\
	config.rpath		\
	config.sub		\
	depcomp			\
	gtk-doc.make		\
	INSTALL			\
	install-sh		\
	ltmain.sh		\
	m4/codeset.m4		\
	m4/gettext.m4		\
	m4/glibc21.m4		\
	m4/glibc2.m4		\
	m4/gtk-doc.m4		\
	m4/iconv.m4		\
	m4/intdiv0.m4		\
	m4/intldir.m4		\
	m4/intl.m4		\
	m4/intlmacosx.m4	\
	m4/intltool.m4		\
	m4/intmax.m4		\
	m4/inttypes_h.m4	\
	m4/inttypes-pri.m4	\
	m4/lcmessage.m4		\
	m4/lib-ld.m4		\
	m4/lib-link.m4		\
	m4/lib-prefix.m4	\
	m4/libtool.m4		\
	m4/lock.m4		\
	m4/longlong.m4		\
	m4/lt~obsolete.m4	\
	m4/ltoptions.m4		\
	m4/ltsugar.m4		\
	m4/ltversion.m4		\
	m4/nls.m4		\
	m4/po.m4		\
	m4/printf-posix.m4	\
	m4/progtest.m4		\
	m4/size_max.m4		\
	m4/stdint_h.m4		\
	m4/uintmax_t.m4		\
	m4/visibility.m4	\
	m4/wchar_t.m4		\
	m4/wint_t.m4		\
	m4/xsize.m4		\
	missing			\
	mkinstalldirs		\
	po/Makevars.template	\
	po/Rules-quot		\
	po/boldquot.sed		\
	po/en@boldquot.header	\
	po/en@quot.header	\
	po/insert-header.sin	\
	po/quot.sed		\
	po/remove-potcdate.sin	\
	`find "$(srcdir)" -type f -name Makefile.in -print`

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc --enable-glade-catalog

CHANGELOG_START = GTKSOURCEVIEW_2_6_1

dist-hook:
	@if test -d "$(srcdir)/.git"; \
	then \
		echo Creating ChangeLog && \
		(GIT_DIR=$(top_srcdir)/.git \
		  ./missing --run git log $(CHANGELOG_START)^^.. --stat -M -C --name-status  --date=short --no-color) | \
		  fmt --split-only > ChangeLog.tmp \
		&& mv -f ChangeLog.tmp $(top_distdir)/ChangeLog \
		|| ( rm -f ChangeLog.tmp ; \
		  echo Failed to generate ChangeLog >&2 ); \
	else \
		echo A git clone is required to generate a ChangeLog >&2; \
	fi

@GNOME_CODE_COVERAGE_RULES@

-include $(top_srcdir)/git.mk
