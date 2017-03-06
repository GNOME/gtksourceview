SUBDIRS = \
	vs9	\
	vs10	\
	vs11	\
	vs12	\
	vs14	\
	vs15

GENERATED_ITEMS = gen-enums.bat

if HAVE_INTROSPECTION
GENERATED_ITEMS += \
	introspection.body.mak	\
	GtkSource_3_0_gir_list

introspection.body.mak: GtkSource-3.0.gir.msvc.introspect
	-$(RM) introspection.body.mak
	for F in `ls *.msvc.introspect`; do \
		case $$F in \
			*)	cat $(top_builddir)/win32/$$F >>$@ \
			;; \
		esac; \
	done
	$(RM) GtkSource-3.0.gir.msvc.introspect
endif

EXTRA_DIST = \
	replace.py	\
	gtksource_version_h.py	\
	detectenv-msvc.mak	\
	gtksourceview-introspection-msvc.mak	\
	introspection-msvc.mak	\
	gen-enums.batin	\
	pc_base.py	\
	gtksourceviewpc.py	\
	$(GENERATED_ITEMS)

gen-enums.bat: $(top_srcdir)/win32/gen-enums.batin enum.headers
	-$(RM) $@
	$(CPP) -P - -traditional-cpp <$< >$@
	$(RM) enum.headers

DISTCLEANFILES = $(GENERATED_ITEMS)

-include $(top_srcdir)/git.mk
