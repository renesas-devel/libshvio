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

/*
 * SuperH VIO color space conversion and stretching
 * Based on MPlayer Vidix driver by Magnus Damm
 * Modified by Takanari Hayama to support NV12->RGB565 conversion
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <uiomux/uiomux.h>
#include "veu_regs.h"

/* #define DEBUG 2 */
#include "common.h"

#include <endian.h>

#if !defined(__LITTLE_ENDIAN__) && !defined(__BIG_ENDIAN__)

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define __LITTLE_ENDIAN__
#elif __BYTE_ORDER == __BIG_ENDIAN
#define __BIG_ENDIAN__
#endif

#endif

struct vio_format_info {
	ren_vid_format_t fmt;
	uint32_t vtrcr_src;
	uint32_t vtrcr_dst;
	uint32_t vswpr;
};

static const struct vio_format_info vio_fmts[] = {
	{ REN_NV12,   VTRCR_SRC_FMT_YCBCR420, VTRCR_DST_FMT_YCBCR420, 7 },
	{ REN_NV16,   VTRCR_SRC_FMT_YCBCR422, VTRCR_DST_FMT_YCBCR422, 7 },
	{ REN_RGB565, VTRCR_SRC_FMT_RGB565,   VTRCR_DST_FMT_RGB565,   6 },
	{ REN_RGB24,  VTRCR_SRC_FMT_RGB888,   VTRCR_DST_FMT_RGB888,   7 },
	{ REN_BGR24,  VTRCR_SRC_FMT_BGR888,   VTRCR_DST_FMT_BGR888,   7 },
	{ REN_RGB32,  VTRCR_SRC_FMT_RGBX888,  VTRCR_DST_FMT_RGBX888,  4 },
};

static const struct vio_format_info *fmt_info(ren_vid_format_t format)
{
	int i, nr_fmts;

	nr_fmts = sizeof(vio_fmts) / sizeof(vio_fmts[0]);
	for (i=0; i<nr_fmts; i++) {
		if (vio_fmts[i].fmt == format)
			return &vio_fmts[i];
	}
	return NULL;
}

/* Helper functions for reading registers. */

static uint32_t read_reg(void *base_addr, int reg_nr)
{
	volatile uint32_t *reg = base_addr + reg_nr;
	uint32_t value = *reg;

#if (DEBUG == 2)
	fprintf(stderr, " read_reg[0x%08x] returned 0x%08x\n", reg_nr, value);
#endif

	return value;
}

static void write_reg(void *base_addr, uint32_t value, int reg_nr)
{
	volatile uint32_t *reg = base_addr + reg_nr;

#if (DEBUG == 2)
	fprintf(stderr, " write_reg[0x%08x] = 0x%08x\n", reg_nr, value);
#endif

	*reg = value;
}

static int vio_is_veu2h(SHVIO *vio)
{
	/* Is this a VEU2H on SH7723? */
	return vio->uio_mmio.size == 0x27c;
}

static int vio_is_veu3f(SHVIO *vio)
{
	return vio->uio_mmio.size == 0xcc;
}

static void set_scale(SHVIO *vio, void *base_addr, int vertical,
		      int size_in, int size_out, int zoom)
{
	uint32_t fixpoint, mant, frac, value, vb;

	/* calculate FRAC and MANT */

	fixpoint = (4096 * (size_in - 1)) / (size_out - 1);
	mant = fixpoint / 4096;
	frac = fixpoint - (mant * 4096);

	if (vio_is_veu2h(vio)) {
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
		mant = 1;
		frac = 0;
	}

	/* set scale */
	value = read_reg(base_addr, VRFCR);
	if (vertical) {
		value &= ~0xffff0000;
		value |= ((mant << 12) | frac) << 16;
	} else {
		value &= ~0xffff;
		value |= (mant << 12) | frac;
	}
	write_reg(base_addr, value, VRFCR);

	/* Assumption that anything newer than VEU2H has VRPBR */
	if (!vio_is_veu2h(vio)) {
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
		value = read_reg(base_addr, VRPBR);
		if (vertical) {
			value &= ~0xffff0000;
		value |= vb << 16;
		} else {
			value &= ~0xffff;
			value |= vb;
		}
		write_reg(base_addr, value, VRPBR);
	}
}

static void
set_clip(void *base_addr, int vertical, int clip_out)
{
	uint32_t value;

	value = read_reg(base_addr, VRFSR);

	if (vertical) {
		value &= ~0xffff0000;
		value |= clip_out << 16;
	} else {
		value &= ~0xffff;
		value |= clip_out;
	}

	write_reg(base_addr, value, VRFSR);
}

static int format_supported(ren_vid_format_t fmt)
{
	const struct vio_format_info *info = fmt_info(fmt);
	if (info)
		return 1;
	return 0;
}

static int
veu_setup(
	SHVIO *vio,
	const struct ren_vid_surface *src,
	const struct ren_vid_surface *dst,
	shvio_rotation_t filter_control)
{
	float scale_x, scale_y;
	uint32_t temp;
	uint32_t Y, C;
	const struct vio_format_info *src_info;
	const struct vio_format_info *dst_info;
	void *base_addr;

	src_info = fmt_info(src->format);
	dst_info = fmt_info(dst->format);

	/* scale factors */
	scale_x = (float)dst->w / src->w;
	scale_y = (float)dst->h / src->h;

	if (!format_supported(src->format) || !format_supported(dst->format)) {
		debug_info("ERR: Invalid surface format!");
		return -1;
	}

	/* Scaling limits */
	if (vio_is_veu2h(vio)) {
		if ((scale_x > 8.0) || (scale_y > 8.0)) {
			debug_info("ERR: Outside scaling limits!");
			return -1;
		}
	} else {
		if ((scale_x > 16.0) || (scale_y > 16.0)) {
			debug_info("ERR: Outside scaling limits!");
			return -1;
		}
	}
	if ((scale_x < 1.0/16.0) || (scale_y < 1.0/16.0)) {
		debug_info("ERR: Outside scaling limits!");
		return -1;
	}

	base_addr = vio->uio_mmio.iomem;

	/* Software reset */
	if (read_reg(base_addr, VESTR) & 0x1)
		write_reg(base_addr, 0, VESTR);
	while (read_reg(base_addr, VESTR) & 1)
		;

	/* Clear VEU end interrupt flag */
	write_reg(base_addr, 0, VEVTR);

	/* VEU Module reset */
	write_reg(base_addr, 0x100, VBSRR);

	/* default to not using bundle mode */
	write_reg(base_addr, 0, VBSSR);

	/* source */
	Y = uiomux_all_virt_to_phys(src->py);
	C = uiomux_all_virt_to_phys(src->pc);
	write_reg(base_addr, Y, VSAYR);
	write_reg(base_addr, C, VSACR);
	write_reg(base_addr, (src->h << 16) | src->w, VESSR);
	write_reg(base_addr, size_y(src->format, src->pitch, src->bpitchy), VESWR);

	/* destination */
	Y = uiomux_all_virt_to_phys(dst->py);
	C = uiomux_all_virt_to_phys(dst->pc);

	if (filter_control & 0xFF) {
		if ((filter_control & 0xFF) == 0x10) {
			/* Horizontal Mirror (A) */
			Y += size_y(dst->format, src->w, 0);
			C += size_y(dst->format, src->w, 0);
		} else if ((filter_control & 0xFF) == 0x20) {
			/* Vertical Mirror (B) */
			Y += size_y(dst->format, (src->h-1) * dst->pitch, dst->bpitchy);
			C += size_c(dst->format, (src->h-2) * dst->pitch, dst->bpitchc);
		} else if ((filter_control & 0xFF) == 0x30) {
			/* Rotate 180 (C) */
			Y += size_y(dst->format, src->w, 0);
			C += size_y(dst->format, src->w, 0);
			Y += size_y(dst->format, src->h * dst->pitch, dst->bpitchy);
			C += size_c(dst->format, src->h * dst->pitch, dst->bpitchc);
		} else if ((filter_control & 0xFF) == 1) {
			/* Rotate 90 (D) */
			Y += size_y(dst->format, src->h-16, dst->bpitchy);
			C += size_y(dst->format, src->h-16, dst->bpitchy);
		} else if ((filter_control & 0xFF) == 2) {
			/* Rotate 270 (E) */
			Y += size_y(dst->format, (src->w-16) * dst->pitch, dst->bpitchy);
			C += size_c(dst->format, (src->w-16) * dst->pitch, dst->bpitchc);
		} else if ((filter_control & 0xFF) == 0x11) {
			/* Rotate 90 & Mirror Horizontal (F) */
			/* Nothing to do */
		} else if ((filter_control & 0xFF) == 0x21) {
			/* Rotate 90 & Mirror Vertical (G) */
			Y += size_y(dst->format, src->h-16, 0);
			C += size_y(dst->format, src->h-16, 0);
			Y += size_y(dst->format, (src->w-16) * dst->pitch, dst->bpitchy);
			C += size_c(dst->format, (src->w-16) * dst->pitch, dst->bpitchc);
		}
	}
	write_reg(base_addr, Y, VDAYR);
	write_reg(base_addr, C, VDACR);
	write_reg(base_addr, size_y(dst->format, dst->pitch, dst->bpitchy), VEDWR);

	/* byte/word swapping */
	temp = 0;
#ifdef __LITTLE_ENDIAN__
	temp |= src_info->vswpr;
	temp |= dst_info->vswpr << 4;
#endif
	write_reg(base_addr, temp, VSWPR);

	/* transform control */
	temp = src_info->vtrcr_src;
	temp |= dst_info->vtrcr_dst;
	if (is_rgb(src->format))
		temp |= VTRCR_RY_SRC_RGB;
	if (different_colorspace(src->format, dst->format))
		temp |= VTRCR_TE_BIT_SET;
	if (vio->bt709)
		temp |= VTRCR_BT709;
	if (vio->full_range)
		temp |= VTRCR_FULL_COLOR_CONV;
	write_reg(base_addr, temp, VTRCR);

	if (vio_is_veu2h(vio)) {
		/* color conversion matrix */
		write_reg(base_addr, 0x0cc5, VMCR00);
		write_reg(base_addr, 0x0950, VMCR01);
		write_reg(base_addr, 0x0000, VMCR02);
		write_reg(base_addr, 0x397f, VMCR10);
		write_reg(base_addr, 0x0950, VMCR11);
		write_reg(base_addr, 0x3cdd, VMCR12);
		write_reg(base_addr, 0x0000, VMCR20);
		write_reg(base_addr, 0x0950, VMCR21);
		write_reg(base_addr, 0x1023, VMCR22);
		write_reg(base_addr, 0x00800010, VCOFFR);
	}

	/* Clipping */
	write_reg(base_addr, 0, VRFSR);
	set_clip(base_addr, 0, dst->w);
	set_clip(base_addr, 1, dst->h);

	/* Scaling */
	write_reg(base_addr, 0, VRFCR);
	if (!(filter_control & 0x3)) {
		/* Not a rotate operation */
		set_scale(vio, base_addr, 0, src->w, dst->w, 0);
		set_scale(vio, base_addr, 1, src->h, dst->h, 0);
	}

	/* Filter control - directly pass user arg to register */
	write_reg(base_addr, filter_control, VFMCR);

	return 0;

fail:
	return -1;
}

static void
veu_set_src(
	SHVIO *vio,
	void *src_py,
	void *src_pc)
{
	void *base_addr = vio->uio_mmio.iomem;
	uint32_t Y, C;

	Y = uiomux_all_virt_to_phys(src_py);
	C = uiomux_all_virt_to_phys(src_pc);
	write_reg(base_addr, Y, VSAYR);
	write_reg(base_addr, C, VSACR);
}

static void
veu_set_src_phys(
	SHVIO *vio,
	uint32_t src_py,
	uint32_t src_pc)
{
	void *base_addr = vio->uio_mmio.iomem;

	write_reg(base_addr, src_py, VSAYR);
	write_reg(base_addr, src_pc, VSACR);
}

static void
veu_set_dst(
	SHVIO *vio,
	void *dst_py,
	void *dst_pc)
{
	void *base_addr = vio->uio_mmio.iomem;
	uint32_t Y, C;

	Y = uiomux_all_virt_to_phys(dst_py);
	C = uiomux_all_virt_to_phys(dst_pc);
	write_reg(base_addr, Y, VDAYR);
	write_reg(base_addr, C, VDACR);
}

static void
veu_set_dst_phys(
	SHVIO *vio,
	uint32_t dst_py,
	uint32_t dst_pc)
{
	void *base_addr = vio->uio_mmio.iomem;

	write_reg(base_addr, dst_py, VDAYR);
	write_reg(base_addr, dst_pc, VDACR);
}

static void
veu_start(SHVIO *vio)
{
	void *base_addr = vio->uio_mmio.iomem;

	/* enable interrupt in VEU */
	write_reg(base_addr, 1, VEIER);

	/* start operation */
	write_reg(base_addr, 1, VESTR);
}

static void
veu_start_bundle(
	SHVIO *vio,
	int bundle_lines)
{
	void *base_addr = vio->uio_mmio.iomem;

	write_reg(base_addr, bundle_lines, VBSSR);

	/* enable interrupt in VEU */
	write_reg(base_addr, 0x101, VEIER);

	/* start operation */
	write_reg(base_addr, 0x101, VESTR);
}

static int
veu_wait(SHVIO *vio)
{
	void *base_addr = vio->uio_mmio.iomem;
	uint32_t vevtr;
	uint32_t vstar;
	int complete = 0;

	vevtr = read_reg(base_addr, VEVTR);
	write_reg(base_addr, 0, VEVTR);   /* ack interrupts */

	/* End of VEU operation? */
	complete = vevtr & 1;

	return complete;
}

const struct shvio_operations veu_ops = {
	.setup = veu_setup,
	.set_src = veu_set_src,
	.set_src_phys = veu_set_src_phys,
	.set_dst = veu_set_dst,
	.set_dst_phys = veu_set_dst_phys,
	.start = veu_start,
	.start_bundle = veu_start_bundle,
	.wait = veu_wait,
};
