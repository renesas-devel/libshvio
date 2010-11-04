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

/*
 * SuperH VEU color space conversion and stretching
 * Based on MPlayer Vidix driver by Magnus Damm
 * Modified by Takanari Hayama to support NV12->RGB565 conversion
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <errno.h>

#include <uiomux/uiomux.h>
#include "shveu/shveu.h"
#include "shveu_regs.h"

struct veu_format_info {
	sh_vid_format_t fmt;
	unsigned long vtrcr_src;
	unsigned long vtrcr_dst;
	unsigned long vswpr;
};

static const struct veu_format_info veu_fmts[] = {
	{ SH_NV12,   VTRCR_SRC_FMT_YCBCR420, VTRCR_DST_FMT_YCBCR420, 7 },
	{ SH_NV16,   VTRCR_SRC_FMT_YCBCR422, VTRCR_DST_FMT_YCBCR422, 7 },
	{ SH_RGB565, VTRCR_SRC_FMT_RGB565,   VTRCR_DST_FMT_RGB565,   6 },
	{ SH_RGB24,  VTRCR_SRC_FMT_RGB888,   VTRCR_DST_FMT_RGB888,   7 },
	{ SH_RGB32,  VTRCR_SRC_FMT_RGBX888,  VTRCR_DST_FMT_RGBX888,  4 },
};

struct uio_map {
	unsigned long address;
	unsigned long size;
	void *iomem;
};

struct SHVEU {
	UIOMux *uiomux;
	struct uio_map uio_mmio;
};


static const struct veu_format_info *fmt_info(sh_vid_format_t format)
{
	int i, nr_fmts;

	nr_fmts = sizeof(veu_fmts) / sizeof(veu_fmts[0]);
	for (i=0; i<nr_fmts; i++) {
		if (veu_fmts[i].fmt == format)
			return &veu_fmts[i];
	}
	return NULL;
}

/* Helper functions for reading registers. */

static unsigned long read_reg(struct uio_map *ump, int reg_nr)
{
	volatile unsigned long *reg = ump->iomem + reg_nr;

	return *reg;
}

static void write_reg(struct uio_map *ump, unsigned long value, int reg_nr)
{
	volatile unsigned long *reg = ump->iomem + reg_nr;

	*reg = value;
}

static int sh_veu_is_veu2h(struct uio_map *ump)
{
	/* Is this a VEU2H on SH7723? */
	return ump->size == 0x27c;
}

static int sh_veu_is_veu3f(struct uio_map *ump)
{
	return ump->size == 0xcc;
}

static void set_scale(struct uio_map *ump, int vertical,
		      int size_in, int size_out, int zoom)
{
	unsigned long fixpoint, mant, frac, value, vb;

	/* calculate FRAC and MANT */

	fixpoint = (4096 * (size_in - 1)) / (size_out - 1);
	mant = fixpoint / 4096;
	frac = fixpoint - (mant * 4096);

	if (sh_veu_is_veu2h(ump)) {
		if (frac & 0x07) {
			frac &= ~0x07;

			if (size_out > size_in)
				frac -= 8;	/* round down if scaling up */
			else
				frac += 8;	/* round up if scaling down */
		}
	}

	/* Fix calculation for 1 to 1 scaling */
	if (size_in == size_out){
		mant = 0;
		frac = 0;
	}

	/* set scale */
	value = read_reg(ump, VRFCR);
	if (vertical) {
		value &= ~0xffff0000;
		value |= ((mant << 12) | frac) << 16;
	} else {
		value &= ~0xffff;
		value |= (mant << 12) | frac;
	}
	write_reg(ump, value, VRFCR);

	/* VEU3F needs additional VRPBR register handling */
	if (sh_veu_is_veu3f(ump)) {
		if (size_out >= size_in)
			vb = 64;
		else {
			if ((mant >= 8) && (mant < 16))
				value = 4;
			else if ((mant >= 4) && (mant < 8))
				value = 2;
			else
				value = 1;

			vb = 64 * 4096 * value;
			vb /= 4096 * mant + frac;
		}

		/* set resize passband register */
		value = read_reg(ump, VRPBR);
		if (vertical) {
			value &= ~0xffff0000;
		value |= vb << 16;
		} else {
			value &= ~0xffff;
			value |= vb;
		}
		write_reg(ump, value, VRPBR);
	}
}

static void
set_clip(struct uio_map *ump, int vertical, int clip_out)
{
	unsigned long value;

	value = read_reg(ump, VRFSR);

	if (vertical) {
		value &= ~0xffff0000;
		value |= clip_out << 16;
	} else {
		value &= ~0xffff;
		value |= clip_out;
	}

	write_reg(ump, value, VRFSR);
}

static void
limit_selection(
	const struct sh_vid_surface *surface,
	const struct sh_vid_rect *sel,
	struct sh_vid_rect *new_sel)
{
	new_sel->x = sel->x;
	new_sel->y = sel->y;
	new_sel->w = sel->w;
	new_sel->h = sel->h;

	if (new_sel->x < 0) new_sel->x = 0;
	if (new_sel->y < 0) new_sel->y = 0;
	if (new_sel->x > surface->w) new_sel->x = surface->w;
	if (new_sel->y > surface->h) new_sel->y = surface->h;

	if ((new_sel->x + new_sel->w) > surface->w)
		new_sel->w = surface->w - new_sel->x;
	if ((new_sel->y + new_sel->h) > surface->h)
		new_sel->h = surface->h - new_sel->y;
}

static unsigned long
offset_py(
	const struct sh_vid_surface *surface,
	const struct sh_vid_rect *sel)
{
	int offset = (sel->y * surface->w) + sel->x;
	return surface->py + size_py(surface->format, offset);
}

static unsigned long
offset_pc(
	const struct sh_vid_surface *surface,
	const struct sh_vid_rect *sel)
{
	int offset = (sel->y * surface->w) + sel->x;
	return surface->pc + size_pc(surface->format, offset);
}

static int format_supported(sh_vid_format_t fmt)
{
	const struct veu_format_info *info = fmt_info(fmt);
	if (info)
		return 1;
	return 0;
}

SHVEU *shveu_open(void)
{
	SHVEU *veu;
	int ret;

	veu = calloc(1, sizeof(*veu));
	if (!veu)
		goto err;

	veu->uiomux = uiomux_open();
	if (!veu->uiomux)
		goto err;

	ret = uiomux_get_mmio (veu->uiomux, UIOMUX_SH_VEU,
		&veu->uio_mmio.address,
		&veu->uio_mmio.size,
		&veu->uio_mmio.iomem);
	if (!ret)
		goto err;

	return veu;

err:
	shveu_close(veu);
	return 0;
}

void shveu_close(SHVEU *veu)
{
	if (veu) {
		if (veu->uiomux)
			uiomux_close(veu->uiomux);
		free(veu);
	}
}

int
shveu_setup(
	SHVEU *veu,
	const struct sh_vid_surface *src_surface,
	const struct sh_vid_surface *dst_surface,
	const struct sh_vid_rect *src_selection,
	const struct sh_vid_rect *dst_selection,
	shveu_rotation_t rotate)
{
	struct uio_map *ump = &veu->uio_mmio;
	float scale_x, scale_y;
	unsigned long temp;
	struct sh_vid_rect default_src_selection = {
		.x = 0,
		.y = 0,
		.w = src_surface->w,
		.h = src_surface->h,
	};
	struct sh_vid_rect default_dst_selection = {
		.x = 0,
		.y = 0,
		.w = dst_surface->w,
		.h = dst_surface->h,
	};
	int src_w, src_h;
	int dst_w, dst_h;
	unsigned long py, pc;
	struct sh_vid_rect new_src_selection;
	struct sh_vid_rect new_dst_selection;
	const struct veu_format_info *src_info = fmt_info(src_surface->format);
	const struct veu_format_info *dst_info = fmt_info(dst_surface->format);

	/* Use default selections if not provided */
	if (!src_selection)
		src_selection = &default_src_selection;
	if (!dst_selection)
		dst_selection = &default_dst_selection;

	/* Save selection sizes for scaling calculation */
	src_w = src_selection->w;
	src_h = src_selection->h;
	dst_w = dst_selection->w;
	dst_h = dst_selection->h;

	/* scale factors */
	scale_x = (float)dst_w / src_w;
	scale_y = (float)dst_h / src_h;

	if (!format_supported(src_surface->format) || !format_supported(dst_surface->format))
		return -1;

	/* Scaling limits */
	if (sh_veu_is_veu2h(ump)) {
		if ((scale_x > 8.0) || (scale_y > 8.0))
			return -1;
	} else {
		if ((scale_x > 16.0) || (scale_y > 16.0))
			return -1;
	}
	if ((scale_x < 1.0/16.0) || (scale_y < 1.0/16.0))
		return -1;
	temp = 0;


	uiomux_lock (veu->uiomux, UIOMUX_SH_VEU);

	/* reset */
	write_reg(ump, 0x100, VBSRR);

	/* default to not using bundle mode */
	write_reg(ump, 0, VBSSR);

	/* source */
	limit_selection(src_surface, src_selection, &new_src_selection);
	py = offset_py(src_surface, &new_src_selection);
	pc = offset_pc(src_surface, &new_src_selection);
	write_reg(ump, py, VSAYR);
	write_reg(ump, pc, VSACR);

	write_reg(ump, (new_src_selection.h << 16) | new_src_selection.w, VESSR);

	/* memory pitch in bytes */
	temp = size_py(src_surface->format, src_surface->w);
	write_reg(ump, temp, VESWR);


	/* destination */
	limit_selection(dst_surface, dst_selection, &new_dst_selection);
	py = offset_py(dst_surface, &new_dst_selection);
	pc = offset_pc(dst_surface, &new_dst_selection);

	if (rotate) {
		int src_vblk  = (new_src_selection.h+15)/16;
		int src_sidev = (new_src_selection.h+15)%16 + 1;
		int offset;

		offset = size_py(dst_surface->format, ((src_vblk-2)*16 + src_sidev));
		py += offset;
		pc += offset;
	}
	write_reg(ump, py, VDAYR);
	write_reg(ump, pc, VDACR);

	/* memory pitch in bytes */
	temp = size_py(dst_surface->format, dst_surface->w);
	write_reg(ump, temp, VEDWR);

	/* byte/word swapping */
	temp = 0;
#ifdef __LITTLE_ENDIAN__
	temp |= src_info->vswpr;
	temp |= dst_info->vswpr << 4;
#endif
	write_reg(ump, temp, VSWPR);

	/* transform control */
	temp = src_info->vtrcr_src;
	temp |= dst_info->vtrcr_dst;
	if (is_rgb(src_surface->format))
		temp |= VTRCR_RY_SRC_RGB;
	if (different_colorspace(src_surface->format, dst_surface->format))
		temp |= VTRCR_TE_BIT_SET;
	write_reg(ump, temp, VTRCR);

	if (sh_veu_is_veu2h(ump)) {
		/* color conversion matrix */
		write_reg(ump, 0x0cc5, VMCR00);
		write_reg(ump, 0x0950, VMCR01);
		write_reg(ump, 0x0000, VMCR02);
		write_reg(ump, 0x397f, VMCR10);
		write_reg(ump, 0x0950, VMCR11);
		write_reg(ump, 0x3cdd, VMCR12);
		write_reg(ump, 0x0000, VMCR20);
		write_reg(ump, 0x0950, VMCR21);
		write_reg(ump, 0x1023, VMCR22);
		write_reg(ump, 0x00800010, VCOFFR);
	}

	/* Clipping */
	write_reg(ump, 0, VRFSR);
	set_clip(ump, 0, new_dst_selection.w);
	set_clip(ump, 1, new_dst_selection.h);

	/* Scaling - based on selections before cropping */
	write_reg(ump, 0, VRFCR);
	if (!rotate) {
		set_scale(ump, 0, src_w, dst_w, 0);
		set_scale(ump, 1, src_h, dst_h, 0);
	}

	/* Rotate */
	if (rotate) {
		write_reg(ump, 1, VFMCR);
	} else {
		write_reg(ump, 0, VFMCR);
	}

	return 0;
}

void
shveu_set_src(
	SHVEU *veu,
	unsigned long src_py,
	unsigned long src_pc)
{
	struct uio_map *ump = &veu->uio_mmio;
	write_reg(ump, src_py, VSAYR);
	write_reg(ump, src_pc, VSACR);
}

void
shveu_set_dst(
	SHVEU *veu,
	unsigned long dst_py,
	unsigned long dst_pc)
{
	struct uio_map *ump = &veu->uio_mmio;
	write_reg(ump, dst_py, VDAYR);
	write_reg(ump, dst_pc, VDACR);
}

void
shveu_start(SHVEU *veu)
{
	struct uio_map *ump = &veu->uio_mmio;

	/* enable interrupt in VEU */
	write_reg(ump, 1, VEIER);

	/* start operation */
	write_reg(ump, 1, VESTR);
}

void
shveu_start_bundle(
	SHVEU *veu,
	int bundle_lines)
{
	struct uio_map *ump = &veu->uio_mmio;

	write_reg(ump, bundle_lines, VBSSR);

	/* enable interrupt in VEU */
	write_reg(ump, 0x101, VEIER);

	/* start operation */
	write_reg(ump, 0x101, VESTR);
}

int
shveu_wait(SHVEU *veu)
{
	struct uio_map *ump = &veu->uio_mmio;
	unsigned long vevtr;
	unsigned long vstar;
	int complete = 0;

	uiomux_sleep(veu->uiomux, UIOMUX_SH_VEU);

	vevtr = read_reg(ump, VEVTR);
	write_reg(ump, 0, VEVTR);   /* ack interrupts */

	/* End of VEU operation? */
	if (vevtr & 1) {
		uiomux_unlock(veu->uiomux, UIOMUX_SH_VEU);
		complete = 1;
	}

	return complete;
}

int
shveu_resize(
	SHVEU *veu,
	const struct sh_vid_surface *src_surface,
	const struct sh_vid_surface *dst_surface)
{
	int ret;

	ret = shveu_setup(veu, src_surface, dst_surface,
		NULL, NULL, SHVEU_NO_ROT);

	if (ret == 0) {
		shveu_start(veu);
		shveu_wait(veu);
	}

	return ret;
}

int
shveu_rotate(
	SHVEU *veu,
	const struct sh_vid_surface *src_surface,
	const struct sh_vid_surface *dst_surface,
	shveu_rotation_t rotate)
{
	int ret;

	ret = shveu_setup(veu, src_surface, dst_surface,
		NULL, NULL, rotate);

	if (ret == 0) {
		shveu_start(veu);
		shveu_wait(veu);
	}

	return ret;
}

