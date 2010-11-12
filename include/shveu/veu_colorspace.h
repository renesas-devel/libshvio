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

/* Common information for SH video buffers */
#ifndef __SH_VIDEO_BUFFER_H__
#define __SH_VIDEO_BUFFER_H__

/* Notes on YUV/YCbCr:
 * YUV historically refers to analogue color space, and YCbCr to digital.
 * The formula used to convert to/from RGB is BT.601 or BT.709. HDTV specifies
 * BT.709, everything else BT.601.
 * MPEG standards use 'clamped' data with Y[16,235], CbCr[16,240]. JFIF file
 * format for JPEG specifies full-range data.
 * All YCbCr formats here are BT.601, Y[16,235], CbCr[16,240].
 */

/** Surface formats */
typedef enum {
	SH_UNKNOWN,
	SH_NV12,    /**< YCbCr420: Y plane, packed CbCr plane, optional alpha plane */
	SH_NV16,    /**< YCbCr422: Y plane, packed CbCr plane, optional alpha plane */
	SH_RGB565,  /**< Packed RGB565 */
	SH_RGB24,   /**< Packed RGB888 */
	SH_BGR24,   /**< Packed BGR888 */
	SH_RGB32,   /**< Packed XRGB8888 (most significant byte ignored) */
	SH_ARGB32,  /**< Packed ARGB8888 */
} sh_vid_format_t;


/** Bounding rectange */
struct sh_vid_rect {
	int x;      /**< Offset from left in pixels */
	int y;      /**< Offset from top in pixels */
	int w;      /**< Width of rectange in pixels */
	int h;      /**< Height of rectange in pixels */
};

/** Surface */
struct sh_vid_surface {
	sh_vid_format_t format; /**< Surface format */
	int w;      /**< Width of active surface in pixels */
	int h;      /**< Height of active surface in pixels */
	int pitch;  /**< Width of surface in pixels */
	void *py;   /**< Address of Y or RGB plane */
	void *pc;   /**< Address of CbCr plane (ignored for RGB) */
	void *pa;   /**< Address of Alpha plane (ignored) */
};

struct format_info {
	sh_vid_format_t fmt;    /**< surface format */
	int y_bpp;      /**< Luma numerator */
	int c_bpp;      /**< Chroma numerator */
	int c_bpp_n;    /**< Chroma numerator */
	int c_bpp_d;    /**< Chroma denominator */
	int c_ss_horz;  /**< Chroma horizontal sub-sampling */
	int c_ss_vert;  /**< Chroma vertical sub-sampling */
};

static const struct format_info fmts[] = {
	{ SH_UNKNOWN, 0, 0, 0, 1, 1, 1 },
	{ SH_NV12,    1, 2, 1, 2, 2, 2 },
	{ SH_NV16,    1, 2, 1, 1, 2, 1 },
	{ SH_RGB565,  2, 0, 0, 1, 1, 1 },
	{ SH_RGB24,   3, 0, 0, 1, 1, 1 },
	{ SH_BGR24,   3, 0, 0, 1, 1, 1 },
	{ SH_RGB32,   4, 0, 0, 1, 1, 1 },
	{ SH_ARGB32,  4, 0, 0, 1, 1, 1 },
};

static inline int is_ycbcr(sh_vid_format_t fmt)
{
	if (fmt >= SH_NV12 && fmt <= SH_NV16)
		return 1;
	return 0;
}

static inline int is_rgb(sh_vid_format_t fmt)
{
	if (fmt >= SH_RGB565 && fmt <= SH_ARGB32)
		return 1;
	return 0;
}

static inline int different_colorspace(sh_vid_format_t fmt1, sh_vid_format_t fmt2)
{
	if ((is_rgb(fmt1) && is_ycbcr(fmt2))
	    || (is_ycbcr(fmt1) && is_rgb(fmt2)))
		return 1;
	return 0;
}

static inline size_t size_y(sh_vid_format_t format, int nr_pixels)
{
	const struct format_info *fmt = &fmts[format];
	return (fmt->y_bpp * nr_pixels);
}

static inline size_t size_c(sh_vid_format_t format, int nr_pixels)
{
	const struct format_info *fmt = &fmts[format];
	return (fmt->c_bpp_n * nr_pixels) / fmt->c_bpp_d;
}

static inline size_t size_a(sh_vid_format_t format, int nr_pixels)
{
	/* Assume 1 byte per alpha pixel */
	return nr_pixels;
}

static inline size_t offset_y(sh_vid_format_t format, int w, int h, int pitch)
{
	const struct format_info *fmt = &fmts[format];
	return (fmt->y_bpp * ((h * pitch) + w));
}

static inline size_t offset_c(sh_vid_format_t format, int w, int h, int pitch)
{
	const struct format_info *fmt = &fmts[format];
	return (fmt->c_bpp * (((h/fmt->c_ss_vert) * pitch/fmt->c_ss_horz) + w/fmt->c_ss_horz));
}

static inline size_t offset_a(sh_vid_format_t format, int w, int h, int pitch)
{
	/* Assume 1 byte per alpha pixel */
	return ((h * pitch) + w);
}

static int horz_increment(sh_vid_format_t format)
{
	/* Only restriction is caused by chroma sub-sampling */
	return fmts[format].c_ss_horz;
}

static int vert_increment(sh_vid_format_t format)
{
	/* Only restriction is caused by chroma sub-sampling */
	return fmts[format].c_ss_vert;
}

/* Get a new surface descriptor based on a selection */
static inline void get_sel_surface(
	struct sh_vid_surface *out,
	const struct sh_vid_surface *in,
	const struct sh_vid_rect *sel)
{
	int x = sel->x & ~horz_increment(in->format);
	int y = sel->y & ~vert_increment(in->format);

	*out = *in;
	out->w = sel->w & ~horz_increment(in->format);
	out->h = sel->h & ~vert_increment(in->format);

	if (in->py) out->py += offset_y(in->format, x, y, in->pitch);
	if (in->pc) out->pc += offset_c(in->format, x, y, in->pitch);
	if (in->pa) out->pa += offset_a(in->format, x, y, in->pitch);
}

#endif /* __SH_VIDEO_BUFFER_H__ */


/** \file
 * Image/Video processing: Scale, rotate, crop, color conversion
 */

#ifndef __VEU_COLORSPACE_H__
#define __VEU_COLORSPACE_H__


/** Rotation */
typedef enum {
	SHVEU_NO_ROT=0,	/**< No rotation */
	SHVEU_ROT_90,	/**< Rotate 90 degrees clockwise */
} shveu_rotation_t;

/** Setup a (scale|rotate) & crop between YCbCr & RGB surfaces
 * The scaling factor is calculated from the surface sizes.
 *
 * \param veu VEU handle
 * \param src_surface Input surface
 * \param dst_surface Output surface
 * \param rotate Rotation to apply
 * \retval 0 Success
 * \retval -1 Error: Attempt to perform simultaneous scaling and rotation
 */
int
shveu_setup(
	SHVEU *veu,
	const struct sh_vid_surface *src_surface,
	const struct sh_vid_surface *dst_surface,
	shveu_rotation_t rotate);


/** Set the source addresses. This is typically used for bundle mode.
 * \param veu VEU handle
 * \param src_py Address of Y or RGB plane of source image
 * \param src_pc Address of CbCr plane of source image (ignored for RGB)
 */
void
shveu_set_src(
	SHVEU *veu,
	void *src_py,
	void *src_pc);

/** Set the destination addresses. This is typically used for bundle mode.
 * \param veu VEU handle
 * \param dst_py Address of Y or RGB plane of destination image
 * \param dst_pc Address of CbCr plane of destination image (ignored for RGB)
 */
void
shveu_set_dst(
	SHVEU *veu,
	void *dst_py,
	void *dst_pc);

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
	const struct sh_vid_surface *src_surface,
	const struct sh_vid_surface *dst_surface);

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
	const struct sh_vid_surface *src_surface,
	const struct sh_vid_surface *dst_surface,
	shveu_rotation_t rotate);

#endif				/* __VEU_COLORSPACE_H__ */
