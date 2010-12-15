## Process this file with automake to produce Makefile.in

INCLUDES = $(INCLTDL) \
           -I$(top_builddir) \
           -I$(top_srcdir)/include

EXTRA_DIST = Version_script.in

AM_CFLAGS =

# Libraries to build
lib_LTLIBRARIES = libshveu.la

noinst_HEADERS = shveu_regs.h

libshveu_la_SOURCES = \
	veu.c

libshveu_la_CFLAGS = $(UIOMUX_CFLAGS)
libshveu_la_LDFLAGS = -version-info @SHARED_VERSION_INFO@ @SHLIB_VERSION_ARG@
libshveu_la_LIBADD = $(UIOMUX_LIBS)
