## Process this file with automake to produce Makefile.in

DISTCHECK_CONFIGURE_FLAGS = --enable-gcc-werror

SUBDIRS = doc include src

# pkg-config
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = shvio.pc

# Extra files to distribute in the source tarball.
EXTRA_DIST = shvio.pc.in shvio-uninstalled.pc.in
