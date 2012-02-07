## Process this file with automake to produce Makefile.in

INCLUDES = $(INCLTDL) \
           -I$(top_builddir) \
           -I$(top_srcdir)/include

EXTRA_DIST = Version_script.in

AM_CFLAGS =

# Libraries to build
lib_LTLIBRARIES = libshvio.la

noinst_HEADERS = veu_regs.h vio6_regs.h common.h

libshvio_la_SOURCES = \
	common.c veu.c vio6.c

libshvio_la_CFLAGS = $(UIOMUX_CFLAGS)
libshvio_la_LDFLAGS = -version-info @SHARED_VERSION_INFO@ @SHLIB_VERSION_ARG@
libshvio_la_LIBADD = $(UIOMUX_LIBS)
