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

/** \file
 * Image/Video processing: Scale, rotate, crop, color conversion
 * Note: When using SH_RGB32 format, the data it treated as XRGB888,
 * i.e. the most significant byte is ignored.
 */

#ifndef __VEU_COLORSPACE_H__
#define __VEU_COLORSPACE_H__


/** Rotation */
typedef enum {
	SHVEU_NO_ROT=0,	/**< No rotation */
	SHVEU_ROT_90,	/**< Rotate 90 degrees clockwise */
} shveu_rotation_t;

/** Surface formats */
typedef enum {
	SH_NV12,
	SH_NV16,
	SH_RGB565,
	SH_RGB24,
	SH_RGB32,
	SH_UNKNOWN,
} shveu_format_t;


/** Bounding rectange */
struct shveu_rect {
	int x;           	/**< Offset from left in pixels */
	int y;           	/**< Offset from top in pixels */
	int w;           	/**< Width of surface in pixels */
	int h;           	/**< Height of surface in pixels */
};

/** Surface */
struct shveu_surface {
	shveu_format_t format; 	/**< Surface format */
	int w;           	/**< Width of surface in pixels */
	int h;           	/**< Height of surface in pixels */
	unsigned long py;	/**< Address of Y or RGB plane */
	unsigned long pc;	/**< Address of CbCr plane (ignored for RGB) */
};

/** Setup a (scale|rotate) & crop between YCbCr & RGB surfaces
 * The scaling factor is calculated from the selection sizes. Therefore, if
 * you wish to crop the output to part of the output surface, you must also
 * change the input selection.
 *
 * \param veu VEU handle
 * \param src_surface Input surface
 * \param dst_surface Output surface
 * \param src_selection Input specification (NULL indicates whole surface)
 * \param dst_selection Output specification (NULL indicates whole surface)
 * \param rotate Rotation to apply
 * \retval 0 Success
 * \retval -1 Error: Attempt to perform simultaneous scaling and rotation
 */
int
shveu_setup(
	SHVEU *veu,
	struct shveu_surface *src_surface,
	struct shveu_surface *dst_surface,
	struct shveu_rect *src_selection,
	struct shveu_rect *dst_selection,
	shveu_rotation_t rotate);


/** Set the source addresses. This is typically used for bundle mode.
 * \param veu VEU handle
 * \param src_py Address of Y or RGB plane of source image
 * \param src_pc Address of CbCr plane of source image (ignored for RGB)
 */
void
shveu_set_src(
	SHVEU *veu,
	unsigned long src_py,
	unsigned long src_pc);

/** Set the destination addresses. This is typically used for bundle mode.
 * \param veu VEU handle
 * \param dst_py Address of Y or RGB plane of destination image
 * \param dst_pc Address of CbCr plane of destination image (ignored for RGB)
 */
void
shveu_set_dst(
	SHVEU *veu,
	unsigned long dst_py,
	unsigned long dst_pc);

/** Start a VEU operation (non-bundle mode).
 * \param veu VEU handle
 */
void
shveu_start(
	SHVEU *veu);

/** Start a VEU operation (bundle mode).
 * \param veu VEU handle
 * \param bundle_lines Number of lines to process
 */
void
shveu_start_bundle(
	SHVEU *veu,
	int bundle_lines);

/** Wait for a VEU operation to complete. The operation is started by a call to shveu_start.
 * \param veu VEU handle
 */
int
shveu_wait(SHVEU *veu);


/** Perform scale between YCbCr & RGB surfaces.
 * This operates on entire surfaces and blocks until completion.
 *
 * \param veu VEU handle
 * \param src_surface Input surface
 * \param dst_surface Output surface
 * \retval 0 Success
 * \retval -1 Error: Unsupported parameters
 */
int
shveu_resize(
	SHVEU *veu,
	struct shveu_surface *src_surface,
	struct shveu_surface *dst_surface);

/** Perform rotate between YCbCr & RGB surfaces
 * This operates on entire surfaces and blocks until completion.
 *
 * \param veu VEU handle
 * \param src_surface Input surface
 * \param dst_surface Output surface
 * \param rotate Rotation to apply
 * \retval 0 Success
 * \retval -1 Error: Unsupported parameters
 */
int
shveu_rotate(
	SHVEU *veu,
	struct shveu_surface *src_surface,
	struct shveu_surface *dst_surface,
	shveu_rotation_t rotate);

#endif				/* __VEU_COLORSPACE_H__ */
