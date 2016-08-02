SUBDIRS = gtksourceview po data docs tests testsuite build

@CODE_COVERAGE_RULES@

AM_DISTCHECK_CONFIGURE_FLAGS =		\
	--enable-gtk-doc		\
	--enable-introspection		\
	--enable-vala			\
	--enable-glade-catalog

pkgconfig_DATA = gtksourceview-3.0.pc

EXTRA_DIST = \
	gtksourceview.doap \
	gtksourceview-3.0.pc.in	\
	HACKING \
	MAINTAINERS \
	config.h.win32.in \
	config.h.win32

MAINTAINERCLEANFILES = 		\
	ABOUT-NLS		\
	aclocal.m4		\
	compile			\
	config.guess		\
	config.h.in		\
	config.rpath		\
	config.sub		\
	depcomp			\
	gtk-doc.make		\
	INSTALL			\
	install-sh		\
	ltmain.sh		\
	m4/ax_append_compile_flags.m4 \
	m4/ax_append_flag.m4	\
	m4/ax_append_link_flags.m4 \
	m4/ax_check_compile_flag.m4 \
	m4/ax_check_link_flag.m4 \
	m4/ax_code_coverage.m4	\
	m4/ax_compiler_flags_cflags.m4 \
	m4/ax_compiler_flags_gir.m4 \
	m4/ax_compiler_flags_ldflags.m4 \
	m4/ax_compiler_flags.m4	\
	m4/ax_is_release.m4	\
	m4/ax_pkg_check_modules.m4 \
	m4/ax_require_defined.m4 \
	m4/ax_valgrind_check.m4 \
	m4/codeset.m4		\
	m4/extern-inline.m4	\
	m4/fcntl-o.m4		\
	m4/gettext.m4		\
	m4/glibc21.m4		\
	m4/glibc2.m4		\
	m4/gtk-3.0.m4		\
	m4/gtk-doc.m4		\
	m4/iconv.m4		\
	m4/intdiv0.m4		\
	m4/intldir.m4		\
	m4/intl.m4		\
	m4/intlmacosx.m4	\
	m4/intmax.m4		\
	m4/introspection.m4	\
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
	m4/pkg.m4		\
	m4/po.m4		\
	m4/printf-posix.m4	\
	m4/progtest.m4		\
	m4/size_max.m4		\
	m4/stdint_h.m4		\
	m4/threadlib.m4		\
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
	po/remove-potcdate.sed	\
	po/remove-potcdate.sin	\
	po/stamp-po		\
	test-driver		\
	`find "$(srcdir)" -type f -name Makefile.in -print`

-include $(top_srcdir)/git.mk
