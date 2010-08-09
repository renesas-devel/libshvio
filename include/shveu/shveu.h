/*
 * libshveu: A library for controlling SH-Mobile VEU
 * Copyright (C) 2009 Renesas Technology Corp.
 * Copyright (C) 2010 Renesas Electronics Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA  02110-1301 USA
 */

#ifndef __SHVEU_H__
#define __SHVEU_H__

#ifdef __cplusplus
extern "C" {
#endif

/** \mainpage
 *
 * \section intro SHVEU: A library for accessing the VEU.
 *
 * This is the documentation for the SHVEU C API. Please read the associated
 * README, COPYING and TODO files.
 *
 * Features:
 *  - Simple interface to colorspace conversion, rotation, scaling
 * 
 * \subsection contents Contents
 * 
 * - \link shveu.h shveu.h \endlink, \link veu_colorspace.h veu_colorspace.h \endlink:
 * Documentation of the SHVEU C API
 *
 * - \link configuration Configuration \endlink:
 * Customizing libshveu
 *
 * - \link building Building \endlink:
 * 
 */

/** \defgroup configuration Configuration
 * \section configure ./configure
 *
 * It is possible to customize the functionality of libshveu
 * by using various ./configure flags when building it from source.
 *
 * For general information about using ./configure, see the file
 * \link install INSTALL \endlink
 *
 */

/** \defgroup install Installation
 * \section install INSTALL
 *
 * \include INSTALL
 */

/** \defgroup building Building against libshveu
 *
 *
 * \section autoconf Using GNU autoconf
 *
 * If you are using GNU autoconf, you do not need to call pkg-config
 * directly. Use the following macro to determine if libshveu is
 * available:
 *
 <pre>
 PKG_CHECK_MODULES(SHVEU, shveu >= 0.5.0,
                   HAVE_SHVEU="yes", HAVE_SHVEU="no")
 if test "x$HAVE_SHVEU" = "xyes" ; then
   AC_SUBST(SHVEU_CFLAGS)
   AC_SUBST(SHVEU_LIBS)
 fi
 </pre>
 *
 * If libshveu is found, HAVE_SHVEU will be set to "yes", and
 * the autoconf variables SHVEU_CFLAGS and SHVEU_LIBS will
 * be set appropriately.
 *
 * \section pkg-config Determining compiler options with pkg-config
 *
 * If you are not using GNU autoconf in your project, you can use the
 * pkg-config tool directly to determine the correct compiler options.
 *
 <pre>
 SHVEU_CFLAGS=`pkg-config --cflags shveu`

 SHVEU_LIBS=`pkg-config --libs shveu`
 </pre>
 *
 */

/** \file
 * The libshveu C API.
 *
 */

/**
 * An opaque handle to the VEU.
 */
struct SHVEU;
typedef struct SHVEU SHVEU;


/**
 * Open a VEU device.
 * \retval 0 Failure, otherwise VEU handle
 */
SHVEU *shveu_open(void);

/**
 * Close a VEU device.
 * \param veu VEU handle
 */
void shveu_close(SHVEU *veu);

#include <shveu/veu_colorspace.h>

#ifdef __cplusplus
}
#endif

#endif /* __SHVEU_H__ */

