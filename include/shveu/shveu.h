/*
 * libshveu: A library for controlling SH-Mobile VEU
 * Copyright (C) 2009 Renesas Technology Corp.
 * Copyright (C) 2010 Renesas Electronics Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
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
 * Open a VEU device with the specified name.
 * If more than one VEU is available on the platform, each VEU
 * has a name such as 'VEU0', 'VEU1', and so on. This API will allow
 * to open a specific VEU by shveu_open_named("VEU0") for instance.
 * \retval 0 Failure, otherwise VEU handle.
 */
SHVEU *shveu_open_named(const char *name);

/**
 * Close a VEU device.
 * \param veu VEU handle
 */
void shveu_close(SHVEU *veu);

/**
 * Query a list of VEU available on this platform.
 * Returns the references to the names of available VEU.
 * If you need to modify the returned array of strings, please copy the
 * array and strings before you do so. The result is shared by all callers
 * of this API in the same process context.
 * \param names List of VEU available. The array is terminated with NULL.
 * \param count Number of VEU.
 * \retval 0 on success; -1 on failure.
 */
int shveu_list_veu(char ***names, int *count);

#include <shveu/veu_colorspace.h>

#ifdef __cplusplus
}
#endif

#endif /* __SHVEU_H__ */
