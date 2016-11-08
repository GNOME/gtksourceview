SUBDIRS = \
	vs9	\
	vs10	\
	vs11	\
	vs12	\
	vs14

GENERATED_ITEMS = gen-enums.bat

if HAVE_INTROSPECTION
GENERATED_ITEMS += \
	introspection.body.mak	\
	GtkSource_@GSV_API_VERSION@_gir_list

introspection.body.mak: GtkSource-@GSV_API_VERSION@.gir.msvc.introspect
	-$(RM) introspection.body.mak
	for F in `ls *.msvc.introspect`; do \
		case $$F in \
			*)	cat $(top_builddir)/build/win32/$$F >>$@ \
			;; \
		esac; \
	done
	$(RM) GtkSource-@GSV_API_VERSION@.gir.msvc.introspect
endif

EXTRA_DIST = \
	replace.py	\
	detectenv-msvc.mak	\
	gtksourceview4-introspection-msvc.mak	\
	introspection-msvc.mak	\
	gen-enums.batin	\
	$(GENERATED_ITEMS)

gen-enums.bat: $(top_srcdir)/build/win32/gen-enums.batin enum.headers
	-$(RM) $@
	$(CPP) -P - -traditional-cpp <$< >$@
	$(RM) enum.headers

DISTCLEANFILES = $(GENERATED_ITEMS)

-include $(top_srcdir)/git.mk
