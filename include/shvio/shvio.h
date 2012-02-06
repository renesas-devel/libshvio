/*
 * libshvio: A library for controlling SH-Mobile VIO/VEU
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

#ifndef __SHVIO_H__
#define __SHVIO_H__

#ifdef __cplusplus
extern "C" {
#endif

/** \mainpage
 *
 * \section intro SHVIO: A library for accessing the VIO/VEU.
 *
 * This is the documentation for the SHVIO C API. Please read the associated
 * README, COPYING and TODO files.
 *
 * Features:
 *  - Simple interface to colorspace conversion, rotation, scaling
 *
 * \subsection contents Contents
 *
 * - \link shvio.h shvio.h \endlink, \link vio_colorspace.h vio_colorspace.h \endlink:
 * Documentation of the SHVIO C API
 *
 * - \link configuration Configuration \endlink:
 * Customizing libshvio
 *
 * - \link building Building \endlink:
 *
 */

/** \defgroup configuration Configuration
 * \section configure ./configure
 *
 * It is possible to customize the functionality of libshvio
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

/** \defgroup building Building against libshvio
 *
 *
 * \section autoconf Using GNU autoconf
 *
 * If you are using GNU autoconf, you do not need to call pkg-config
 * directly. Use the following macro to determine if libshvio is
 * available:
 *
 <pre>
 PKG_CHECK_MODULES(SHVIO, shvio >= 0.5.0,
                   HAVE_SHVIO="yes", HAVE_SHVIO="no")
 if test "x$HAVE_SHVIO" = "xyes" ; then
   AC_SUBST(SHVIO_CFLAGS)
   AC_SUBST(SHVIO_LIBS)
 fi
 </pre>
 *
 * If libshvio is found, HAVE_SHVIO will be set to "yes", and
 * the autoconf variables SHVIO_CFLAGS and SHVIO_LIBS will
 * be set appropriately.
 *
 * \section pkg-config Determining compiler options with pkg-config
 *
 * If you are not using GNU autoconf in your project, you can use the
 * pkg-config tool directly to determine the correct compiler options.
 *
 <pre>
 SHVIO_CFLAGS=`pkg-config --cflags shvio`

 SHVIO_LIBS=`pkg-config --libs shvio`
 </pre>
 *
 */

/** \file
 * The libshvio C API.
 *
 */

/**
 * An opaque handle to the VIO.
 */
struct SHVIO;
typedef struct SHVIO SHVIO;


/**
 * Open a VIO device.
 * \retval 0 Failure, otherwise VIO handle
 */
SHVIO *shvio_open(void);

/**
 * Open a VIO device with the specified name.
 * If more than one VIO is available on the platform, each VIO
 * has a name such as 'VIO0', 'VIO1', and so on. This API will allow
 * to open a specific VIO by shvio_open_named("VIO0") for instance.
 * \retval 0 Failure, otherwise VIO handle.
 */
SHVIO *shvio_open_named(const char *name);

/**
 * Close a VIO device.
 * \param vio VIO handle
 */
void shvio_close(SHVIO *vio);

/**
 * Query a list of VIO available on this platform.
 * Returns the references to the names of available VIO.
 * If you need to modify the returned array of strings, please copy the
 * array and strings before you do so. The result is shared by all callers
 * of this API in the same process context.
 * \param names List of VIO available. The array is terminated with NULL.
 * \param count Number of VIO.
 * \retval 0 on success; -1 on failure.
 */
int shvio_list_vio(char ***names, int *count);

#include <shvio/vio_colorspace.h>

#ifdef __cplusplus
}
#endif

#endif /* __SHVIO_H__ */
