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

#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <uiomux/uiomux.h>
#include "shveu/shveu.h"
#include "shveu_regs.h"

struct veu_format_info {
	ren_vid_format_t fmt;
	unsigned long vtrcr_src;
	unsigned long vtrcr_dst;
	unsigned long vswpr;
};

static const struct veu_format_info veu_fmts[] = {
	{ REN_NV12,   VTRCR_SRC_FMT_YCBCR420, VTRCR_DST_FMT_YCBCR420, 7 },
	{ REN_NV16,   VTRCR_SRC_FMT_YCBCR422, VTRCR_DST_FMT_YCBCR422, 7 },
	{ REN_RGB565, VTRCR_SRC_FMT_RGB565,   VTRCR_DST_FMT_RGB565,   6 },
	{ REN_RGB24,  VTRCR_SRC_FMT_RGB888,   VTRCR_DST_FMT_RGB888,   7 },
	{ REN_RGB32,  VTRCR_SRC_FMT_RGBX888,  VTRCR_DST_FMT_RGBX888,  4 },
};

struct uio_map {
	unsigned long address;
	unsigned long size;
	void *iomem;
};

struct SHVEU {
	UIOMux *uiomux;
	struct uio_map uio_mmio;
	struct ren_vid_surface src_user;
	struct ren_vid_surface src_hw;
	struct ren_vid_surface dst_user;
	struct ren_vid_surface dst_hw;
};


static const struct veu_format_info *fmt_info(ren_vid_format_t format)
{
	int i, nr_fmts;

	nr_fmts = sizeof(veu_fmts) / sizeof(veu_fmts[0]);
	for (i=0; i<nr_fmts; i++) {
		if (veu_fmts[i].fmt == format)
			return &veu_fmts[i];
	}
	return NULL;
}

static void dbg(const char *str1, int l, const char *str2, const struct ren_vid_surface *s)
{
#if DEBUG
	fprintf(stderr, "%s:%d: %s: (%dx%d) pitch=%d py=%p, pc=%p, pa=%p\n", str1, l, str2, s->w, s->h, s->pitch, s->py, s->pc, s->pa);
#endif
}

static void copy_plane(void *dst, void *src, int bpp, int h, int len, int dst_pitch, int src_pitch)
{
	int y;
	if (src && dst != src) {
		for (y=0; y<h; y++) {
			memcpy(dst, src, len * bpp);
			src += src_pitch * bpp;
			dst += dst_pitch * bpp;
		}
	}
}

/* Copy active surface contents - assumes output is big enough */
static void copy_surface(
	struct ren_vid_surface *out,
	const struct ren_vid_surface *in)
{
	const struct format_info *fmt = &fmts[in->format];

	copy_plane(out->py, in->py, fmt->y_bpp, in->h, in->w, out->pitch, in->pitch);

	copy_plane(out->pc, in->pc, fmt->c_bpp,
		in->h/fmt->c_ss_vert,
		in->w/fmt->c_ss_horz,
		out->pitch/fmt->c_ss_horz,
		in->pitch/fmt->c_ss_horz);

	copy_plane(out->pa, in->pa, 1, in->h, in->w, out->pitch, in->pitch);
}

/* Check/create surface that can be accessed by the hardware */
static int get_hw_surface(
	UIOMux * uiomux,
	struct ren_vid_surface *out,
	const struct ren_vid_surface *in)
{
	unsigned long phys;
	int y;

	*out = *in;

	if (in->py) {
		phys = uiomux_all_virt_to_phys(in->py);
		if (!phys) {
			size_t len = size_y(in->format, in->h * in->w);
			/* Supplied buffer is not usable by the hardware! */
			out->py = uiomux_malloc(uiomux, UIOMUX_SH_VEU, len, 32);
			if (!out->py)
				return -1;
		}
	}

	if (in->pc) {
		phys = uiomux_all_virt_to_phys(in->pc);
		if (!phys) {
			size_t len = size_c(in->format, in->h * in->w);
			/* Supplied buffer is not usable by the hardware! */
			out->pc = uiomux_malloc(uiomux, UIOMUX_SH_VEU, len, 32);
			if (!out->pc)
				return -1;
		}
	}

	if (in->pa) {
		phys = uiomux_all_virt_to_phys(in->pa);
		if (!phys) {
			size_t len = size_a(in->format, in->h * in->w);
			/* Supplied buffer is not usable by the hardware! */
			out->pa = uiomux_malloc(uiomux, UIOMUX_SH_VEU, len, 32);
			if (!out->pa)
				return -1;
		}
	}

	return 0;
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

static int veu_is_veu2h(struct uio_map *ump)
{
	/* Is this a VEU2H on SH7723? */
	return ump->size == 0x27c;
}

static int veu_is_veu3f(struct uio_map *ump)
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

	if (veu_is_veu2h(ump)) {
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
	if (veu_is_veu3f(ump)) {
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

static int format_supported(ren_vid_format_t fmt)
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
	const struct ren_vid_surface *src_surface,
	const struct ren_vid_surface *dst_surface,
	shveu_rotation_t rotate)
{
	struct uio_map *ump = &veu->uio_mmio;
	float scale_x, scale_y;
	unsigned long temp;
	unsigned long Y, C;
	const struct veu_format_info *src_info = fmt_info(src_surface->format);
	const struct veu_format_info *dst_info = fmt_info(dst_surface->format);
	struct ren_vid_surface *src = &veu->src_hw;
	struct ren_vid_surface *dst = &veu->dst_hw;

	/* scale factors */
	scale_x = (float)dst_surface->w / src_surface->w;
	scale_y = (float)dst_surface->h / src_surface->h;

	if (!format_supported(src_surface->format) || !format_supported(dst_surface->format))
		return -1;

	/* Scaling limits */
	if (veu_is_veu2h(ump)) {
		if ((scale_x > 8.0) || (scale_y > 8.0))
			return -1;
	} else {
		if ((scale_x > 16.0) || (scale_y > 16.0))
			return -1;
	}
	if ((scale_x < 1.0/16.0) || (scale_y < 1.0/16.0))
		return -1;

	/* source - use a buffer the hardware can access */
	if (get_hw_surface(veu->uiomux, src, src_surface) < 0)
		return -1;
	copy_surface(src, src_surface);

	/* destination - use a buffer the hardware can access */
	if (get_hw_surface(veu->uiomux, dst, dst_surface) < 0)
		return -1;

	/* Keep track of the requsted output surface */
	veu->src_user = *src_surface;
	veu->dst_user = *dst_surface;

	uiomux_lock (veu->uiomux, UIOMUX_SH_VEU);

	/* reset */
	write_reg(ump, 0x100, VBSRR);

	/* default to not using bundle mode */
	write_reg(ump, 0, VBSSR);

	/* source */
	Y = uiomux_all_virt_to_phys(src->py);
	C = uiomux_all_virt_to_phys(src->pc);
	write_reg(ump, Y, VSAYR);
	write_reg(ump, C, VSACR);
	write_reg(ump, (src->h << 16) | src->w, VESSR);
	write_reg(ump, size_y(src->format, src->pitch), VESWR);

	/* destination */
	Y = uiomux_all_virt_to_phys(dst->py);
	C = uiomux_all_virt_to_phys(dst->pc);
	if (rotate) {
		int src_vblk  = (src->h+15)/16;
		int src_sidev = (src->h+15)%16 + 1;
		int offset;

		offset = size_y(dst->format, ((src_vblk-2)*16 + src_sidev));
		Y += offset;
		C += offset;
	}
	write_reg(ump, Y, VDAYR);
	write_reg(ump, C, VDACR);
	write_reg(ump, size_y(dst->format, dst->pitch), VEDWR);

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

	if (veu_is_veu2h(ump)) {
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
	set_clip(ump, 0, dst->w);
	set_clip(ump, 1, dst->h);

	/* Scaling */
	write_reg(ump, 0, VRFCR);
	if (!rotate) {
		set_scale(ump, 0, src->w, dst->w, 0);
		set_scale(ump, 1, src->h, dst->h, 0);
	}

	/* Rotate */
	if (rotate) {
		write_reg(ump, 1, VFMCR);
	} else {
		write_reg(ump, 0, VFMCR);
	}

	return 0;

fail:
	uiomux_unlock(veu->uiomux, UIOMUX_SH_VEU);
	return -1;
}

void
shveu_set_src(
	SHVEU *veu,
	void *src_py,
	void *src_pc)
{
	struct uio_map *ump = &veu->uio_mmio;
	unsigned long Y, C;

	Y = uiomux_all_virt_to_phys(src_py);
	C = uiomux_all_virt_to_phys(src_pc);
	write_reg(ump, Y, VSAYR);
	write_reg(ump, C, VSACR);
}

void
shveu_set_dst(
	SHVEU *veu,
	void *dst_py,
	void *dst_pc)
{
	struct uio_map *ump = &veu->uio_mmio;
	unsigned long Y, C;

	Y = uiomux_all_virt_to_phys(dst_py);
	C = uiomux_all_virt_to_phys(dst_pc);
	write_reg(ump, Y, VDAYR);
	write_reg(ump, C, VDACR);
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

		dbg(__func__, __LINE__, "src_user", &veu->src_user);
		dbg(__func__, __LINE__, "dst_user", &veu->dst_user);
		dbg(__func__, __LINE__, "src_hw", &veu->src_hw);
		dbg(__func__, __LINE__, "dst_hw", &veu->dst_hw);
		copy_surface(&veu->dst_user, &veu->dst_hw);

		/* free locally alloacted surfaces */
		if (veu->src_hw.py != veu->src_user.py) {
			size_t len = size_y(veu->src_hw.format, veu->src_hw.h * veu->src_hw.w);
			uiomux_free(veu->uiomux, UIOMUX_SH_VEU, veu->src_hw.py, len);
		}
		if (veu->src_hw.pc != veu->src_user.pc) {
			size_t len = size_c(veu->src_hw.format, veu->src_hw.h * veu->src_hw.w);
			uiomux_free(veu->uiomux, UIOMUX_SH_VEU, veu->src_hw.pc, len);
		}
		if (veu->dst_hw.py != veu->dst_user.py) {
			size_t len = size_y(veu->dst_hw.format, veu->dst_hw.h * veu->dst_hw.w);
			uiomux_free(veu->uiomux, UIOMUX_SH_VEU, veu->dst_hw.py, len);
		}
		if (veu->dst_hw.pc != veu->dst_user.pc) {
			size_t len = size_c(veu->dst_hw.format, veu->dst_hw.h * veu->dst_hw.w);
			uiomux_free(veu->uiomux, UIOMUX_SH_VEU, veu->dst_hw.pc, len);
		}
	}

	return complete;
}

int
shveu_resize(
	SHVEU *veu,
	const struct ren_vid_surface *src_surface,
	const struct ren_vid_surface *dst_surface)
{
	int ret;

	ret = shveu_setup(veu, src_surface, dst_surface, SHVEU_NO_ROT);

	if (ret == 0) {
		shveu_start(veu);
		shveu_wait(veu);
	}

	return ret;
}

int
shveu_rotate(
	SHVEU *veu,
	const struct ren_vid_surface *src_surface,
	const struct ren_vid_surface *dst_surface,
	shveu_rotation_t rotate)
{
	int ret;

	ret = shveu_setup(veu, src_surface, dst_surface, rotate);

	if (ret == 0) {
		shveu_start(veu);
		shveu_wait(veu);
	}

	return ret;
}

