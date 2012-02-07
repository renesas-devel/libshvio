## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_builddir) \
           -I$(top_srcdir)/include

SHVIODIR = ../libshvio
SHVIO_LIBS = $(SHVIODIR)/libshvio.la

if HAVE_NCURSES
ncurses_lib = -lncurses
endif

bin_PROGRAMS = shvio-convert shvio-display

noinst_HEADERS = display.h

shvio_convert_SOURCES = shvio-convert.c
shvio_convert_CFLAGS = $(SHVIO_CFLAGS) $(UIOMUX_CFLAGS)
shvio_convert_LDADD = $(SHVIO_LIBS) $(UIOMUX_LIBS)

if USE_MERAM
shvio_convert_CFLAGS += $(SHMERAM_CFLAGS)
shvio_convert_LDADD += $(SHMERAM_LIBS)
endif

shvio_display_SOURCES = shvio-display.c display.c
shvio_display_CFLAGS = $(SHVIO_CFLAGS) $(UIOMUX_CFLAGS)
shvio_display_LDADD = $(SHVIO_LIBS) $(UIOMUX_LIBS) $(ncurses_lib) -lrt
