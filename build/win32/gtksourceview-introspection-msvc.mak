# NMake Makefile to build Introspection Files for GtkSourceView

!include detectenv-msvc.mak

APIVERSION = 3.0

CHECK_PACKAGE = gtk+-3.0

built_install_girs = GtkSource-$(APIVERSION).gir
built_install_typelibs = GtkSource-$(APIVERSION).typelib

!include introspection-msvc.mak

!if "$(BUILD_INTROSPECTION)" == "TRUE"
all: setgirbuildenv $(built_install_girs) $(built_install_typelibs)

setgirbuildenv:
	@set PYTHONPATH=$(PREFIX)\lib\gobject-introspection
	@set PATH=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(PREFIX)\bin;$(PATH)
	@set PKG_CONFIG_PATH=$(PKG_CONFIG_PATH)
	@set LIB=vs$(VSVER)\$(CFG)\$(PLAT)\bin;$(LIB)

!include introspection.body.mak

install-introspection: setgirbuildenv $(built_install_girs) $(built_install_typelibs)
	@-copy *.gir $(G_IR_INCLUDEDIR)
	@-copy /b *.typelib $(G_IR_TYPELIBDIR)

!else
all:
	@-echo $(ERROR_MSG)
!endif

clean:
	@-del /f/q *.typelib
	@-del /f/q *.gir
