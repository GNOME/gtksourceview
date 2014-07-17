ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST += README.W32

if HAVE_DOXYGEN
  DOXYDIR = docs
endif

doc_DATA = AUTHORS ChangeLog COPYING INSTALL NEWS README

bin_PROGRAMS = jupiter
jupiter_SOURCES = main.c
jupiter_CPPFLAGS = -I$(top_srcdir)/common
jupiter_LDADD = ../common/libjupcommon.a

check_SCRIPTS = greptest.sh
TESTS = $(check_SCRIPTS)

greptest.sh:
	echo './jupiter | grep "Hello from .*jupiter!"' > greptest.sh
	chmod +x greptest.sh

CLEANFILES = greptest.sh
