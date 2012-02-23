/*
 * test for controlling Renesas VIO image processing H/W
 * Copyright (C) 2012
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

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <uiomux/uiomux.h>
#include "vio6_regs.h"

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
	uint32_t fmtid;
	uint32_t dswap;
};

static const struct vio_format_info vio_fmts[] = {
	{ REN_NV12,	FMT_YCBCR420SP,	0xf },
	{ REN_NV16,	FMT_YCBCR422SP,	0xf },
	{ REN_YV12,	FMT_YCBCR420P,	0xf },
	{ REN_YV16,	FMT_YCBCR422P,	0xf },
	{ REN_UYVY,	FMT_YCBCR422I,	0xf },
	{ REN_RGB565,	FMT_RGB565,	0xe },
	{ REN_RGB24,	FMT_RGB888,	0xf },
	{ REN_BGR24,	FMT_BGR888,	0xf },
	{ REN_RGB32,	FMT_RGBX888,	0xc },
	{ REN_ARGB32,	FMT_ARGB8888,	0xc },
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

#if (DEBUG == 2)
static const char *regname_misc[] = {
	[0x00000000] = "CMD0",
	[0x00000004] = "CMD1",
	[0x00000008] = "CMD2",
	[0x0000000c] = "CMD3",
	[0x00000018] = "SRESET",
	[0x00000020] = "STATUS",
	[0x00000030] = "WPF0_IRQ_ENB",
	[0x00000034] = "WPF0_IRQ_STA",
	[0x00000030 + 0x0c] = "WPF1_IRQ_ENB",
	[0x00000034 + 0x0c] = "WPF1_IRQ_STA",
	[0x00000030 + 0x18] = "WPF2_IRQ_ENB",
	[0x00000034 + 0x18] = "WPF2_IRQ_STA",
	[0x00000030 + 0x24] = "WPF3_IRQ_ENB",
	[0x00000034 + 0x24] = "WPF3_IRQ_STA",
};

static const char *regname_rpf[] = {
	[0x00000000] = "SRC_BSIZE",
	[0x00000004] = "SRC_ESIZE",
	[0x00000008] = "INFMT",
	[0x0000000c] = "DSWAP",
	[0x00000010] = "LOC",
	[0x00000014] = "ALPH_SEL",
	[0x00000018] = "VRTCOL_SET",
	[0x00000064] = "SRCM_PSTRIDE",
	[0x00000068] = "SRCM_ASTRIDE",
	[0x0000006c] = "SRCM_ADDR_Y",
	[0x00000070] = "SRCM_ADDR_C0",
	[0x00000074] = "SRCM_ADDR_C1",
	[0x00000080] = "CHPRI_CTRL",
};

static const char *regname_wpf[] = {
	[0x00000000] = "SRCRPFL",
	[0x00000004] = "HSZCLIP",
	[0x00000008] = "VSZCLIP",
	[0x0000000c] = "OUTFMT",
	[0x00000010] = "DSWAP",
	[0x00000014] = "RNDCTRL",
	[0x0000004c] = "DSTM_STRIDE_Y",
	[0x00000050] = "DSTM_STRIDE_C",
	[0x00000054] = "DSTM_ADDR_Y",
	[0x00000058] = "DSTM_ADDR_C0",
	[0x0000005c] = "DSTM_ADDR_C1",
	[0x00000060] = "CHPRI_CTRL",
};

static const char *regname_dpr[] = {
	[0x00000000] = "DPR_CTRL0",
	[0x00000004] = "DPR_CTRL1",
	[0x00000008] = "DPR_CTRL2",
	[0x0000000c] = "DPR_CTRL3",
	[0x00000010] = "DPR_FXA",
	[0x00000018] = "DPR_FPORCH0",
	[0x0000001c] = "DPR_FPORCH1",
	[0x00000020] = "DPR_FPORCH2",
	[0x00000024] = "DPR_FPORCH3",
};

static const char *regname_uds[] = {
	[0x00000000] = "CTRL",
	[0x00000004] = "SCALE",
	[0x00000010] = "PASS_BWIDTH",
	[0x00000024] = "CLIP_SIZE",
	[0x00000028] = "FILL_COLOR",
};

static const char *regname_bru[] = {
	[0x00000000] = "INCTRL",
	[0x00000004] = "VIRRPF_SIZE",
	[0x00000008] = "VIRRPF_LOC",
	[0x0000000c] = "VIRRPF_COL",
	[0x00000010] = "A_CTRL",
	[0x00000014] = "A_BLD",
	[0x00000018] = "B_CTRL",
	[0x0000001c] = "B_BLD",
	[0x00000020] = "C_CTRL",
	[0x00000024] = "C_BLD",
	[0x00000028] = "D_CTRL",
	[0x0000002c] = "D_BLD",
	[0x00000030] = "ROP",
};

static inline void show_regname(int reg_nr)
{
	if (reg_nr < 0x300)
		fprintf(stderr, "%-20s", regname_misc[reg_nr]);
	else if (reg_nr < 0x300)
		fprintf(stderr, "UNKNOWN (0x%04x)   ", reg_nr);
	else if (reg_nr < 0x400)
		fprintf(stderr, "RPF0_%-15s", regname_rpf[reg_nr - 0x300]);
	else if (reg_nr < 0x500)
		fprintf(stderr, "RPF1_%-15s", regname_rpf[reg_nr - 0x400]);
	else if (reg_nr < 0x600)
		fprintf(stderr, "RPF2_%-15s", regname_rpf[reg_nr - 0x500]);
	else if (reg_nr < 0x700)
		fprintf(stderr, "RPF3_%-15s", regname_rpf[reg_nr - 0x600]);
	else if (reg_nr < 0x800)
		fprintf(stderr, "RPF4_%-15s", regname_rpf[reg_nr - 0x700]);
	else if (reg_nr < 0x1000)
		fprintf(stderr, "UNKNOWN (0x%04x)   ", reg_nr);
	else if (reg_nr < 0x1100)
		fprintf(stderr, "WPF0_%-15s", regname_wpf[reg_nr - 0x1000]);
	else if (reg_nr < 0x1200)
		fprintf(stderr, "WPF1_%-15s", regname_wpf[reg_nr - 0x1100]);
	else if (reg_nr < 0x1300)
		fprintf(stderr, "WPF2_%-15s", regname_wpf[reg_nr - 0x1200]);
	else if (reg_nr < 0x1400)
		fprintf(stderr, "WPF3_%-15s", regname_wpf[reg_nr - 0x1300]);
	else if (reg_nr < 0x2000)
		fprintf(stderr, "UNKNOWN (0x%04x)   ", reg_nr);
	else if (reg_nr <= 0x2024)
		fprintf(stderr, "DPR_%-16s", regname_dpr[reg_nr - 0x2000]);
	else if (reg_nr < 0x2300)
		fprintf(stderr, "UNKNOWN (0x%04x)   ", reg_nr);
	else if (reg_nr <= 0x2328)
		fprintf(stderr, "UDS0_%-15s", regname_uds[reg_nr - 0x2300]);
	else if (reg_nr < 0x2600)
		fprintf(stderr, "UNKNOWN (0x%04x)   ", reg_nr);
	else if (reg_nr == 0x2600)
		fprintf(stderr, "LUT                ");
	else if (reg_nr < 0x2a00)
		fprintf(stderr, "UNKNOWN (0x%04x)   ", reg_nr);
	else if (reg_nr <= 0x2a30)
		fprintf(stderr, "BRU_%-16s", regname_bru[reg_nr - 0x2a00]);
	else if (reg_nr < 0x2b00)
		fprintf(stderr, "UNKNOWN (0x%04x)   ", reg_nr);
	else if (reg_nr <= 0x2b28)
		fprintf(stderr, "UDS1_%-15s", regname_uds[reg_nr - 0x2b00]);
}
#endif

/* Helper functions for reading registers. */

static uint32_t read_reg(void *base_addr, int reg_nr)
{
	volatile uint32_t *reg = base_addr + reg_nr;
	uint32_t value = *reg;

#if (DEBUG == 2)
	fprintf(stderr, "  read_reg[");
	show_regname(reg_nr);
	fprintf(stderr, "] returned 0x%08x\n", value);
	fflush(stderr);
#endif

	return value;
}

static void write_reg(void *base_addr, uint32_t value, int reg_nr)
{
	volatile uint32_t *reg = base_addr + reg_nr;

#if (DEBUG == 2)
	fprintf(stderr, " write_reg[");
	show_regname(reg_nr);
	fprintf(stderr, "] = 0x%08x\n", value);
	fflush(stderr);
#endif

	*reg = value;
}

static void set_scale(SHVIO *vio, void *base_addr, int vertical,
		      int size_in, int size_out)
{
	uint32_t fixpoint, mant, frac, value, vb;

	/* calculate FRAC and MANT */

	fixpoint = (4096 * (size_in - 1)) / (size_out - 1);
	mant = fixpoint / 4096;
	frac = fixpoint - (mant * 4096);

	if (frac & 0x07) {
		frac &= ~0x07;

		if (size_out > size_in)
			frac -= 8;	/* round down if scaling up */
		else
			frac += 8;	/* round up if scaling down */
	}

	/* Fix calculation for 1 to 1 scaling */
	if (size_in == size_out){
		mant = 1;
		frac = 0;
	}

	/* set scale */
	value = read_reg(base_addr, UDS_SCALE(0));
	if (vertical) {
		value &= ~0xffff;
		value |= (mant << 12) | frac;
	} else {
		value &= ~0xffff0000;
		value |= ((mant << 12) | frac) << 16;
	}
	write_reg(base_addr, value, UDS_SCALE(0));

	/* Assumption that anything newer than VIO2H has VRPBR */
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
	value = read_reg(base_addr, UDS_PASS_BWIDTH(0));
	if (vertical) {
		value &= ~0xffff;
		value |= vb;
	} else {
		value &= ~0xffff0000;
		value |= vb << 16;
	}
	write_reg(base_addr, value, UDS_PASS_BWIDTH(0));
}

static int format_supported(ren_vid_format_t fmt)
{
	const struct vio_format_info *info = fmt_info(fmt);
	if (info)
		return 1;
	return 0;
}

static void
vio6_set_src(
	SHVIO *vio,
	void *src_py,
	void *src_pc)
{
	void *base_addr = vio->uio_mmio.iomem;
	uint32_t Y, C;

	Y = uiomux_all_virt_to_phys(src_py);
	C = uiomux_all_virt_to_phys(src_pc);
	write_reg(base_addr, Y, RPF_SRCM_ADDR_Y(0));
	write_reg(base_addr, C, RPF_SRCM_ADDR_C0(0));
}

static void
vio6_set_src2(
	SHVIO *vio,
	void *src_py,
	void *src_pcb,
	void *src_pcr)
{
	void *base_addr = vio->uio_mmio.iomem;
	uint32_t Y, Cb, Cr;

	Y = uiomux_all_virt_to_phys(src_py);
	Cb = uiomux_all_virt_to_phys(src_pcb);
	Cr = uiomux_all_virt_to_phys(src_pcr);
	write_reg(base_addr, Y, RPF_SRCM_ADDR_Y(0));
	write_reg(base_addr, Cb, RPF_SRCM_ADDR_C0(0));
	write_reg(base_addr, Cr, RPF_SRCM_ADDR_C1(0));
}

static void
vio6_set_src_phys(
	SHVIO *vio,
	uint32_t src_py,
	uint32_t src_pc)
{
	void *base_addr = vio->uio_mmio.iomem;

	write_reg(base_addr, src_py, RPF_SRCM_ADDR_Y(0));
	write_reg(base_addr, src_pc, RPF_SRCM_ADDR_C0(0));
}

static void
vio6_set_dst(
	SHVIO *vio,
	void *dst_py,
	void *dst_pc)
{
	void *base_addr = vio->uio_mmio.iomem;
	uint32_t Y, C;

	Y = uiomux_all_virt_to_phys(dst_py);
	C = uiomux_all_virt_to_phys(dst_pc);
	write_reg(base_addr, Y, WPF_DSTM_ADDR_Y(0));
	write_reg(base_addr, C, WPF_DSTM_ADDR_C0(0));
}

static void
vio6_set_dst2(
	SHVIO *vio,
	void *dst_py,
	void *dst_pcb,
	void *dst_pcr)
{
	void *base_addr = vio->uio_mmio.iomem;
	uint32_t Y, Cb, Cr;

	Y = uiomux_all_virt_to_phys(dst_py);
	Cb = uiomux_all_virt_to_phys(dst_pcb);
	Cr = uiomux_all_virt_to_phys(dst_pcr);
	write_reg(base_addr, Y, WPF_DSTM_ADDR_Y(0));
	write_reg(base_addr, Cb, WPF_DSTM_ADDR_C0(0));
	write_reg(base_addr, Cr, WPF_DSTM_ADDR_C1(0));
}

static void
vio6_set_dst_phys(
	SHVIO *vio,
	uint32_t dst_py,
	uint32_t dst_pc)
{
	void *base_addr = vio->uio_mmio.iomem;

	write_reg(base_addr, dst_py, WPF_DSTM_ADDR_Y(0));
	write_reg(base_addr, dst_pc, WPF_DSTM_ADDR_C0(0));
}

static int
vio6_setup(
	SHVIO *vio,
	const struct ren_vid_surface *src,
	const struct ren_vid_surface *dst)
{
	float scale_x, scale_y;
	uint32_t Y, C;
	const struct vio_format_info *src_info;
	const struct vio_format_info *dst_info;
	const struct vio_format_info *viofmt;
	uint32_t val;
	void *base_addr;

	src_info = fmt_info(src->format);
	dst_info = fmt_info(dst->format);

	if (!format_supported(src->format) ||
	    !format_supported(dst->format)) {
		debug_info("ERR: Invalid surface format!");
		return -1;
	}

	uiomux_lock (vio->uiomux, vio->uiores);

	base_addr = vio->uio_mmio.iomem;

	/* WPF: disable interrupt */
	write_reg(base_addr, 0, WPF_IRQ_ENB(0));

	/* WPF: software reset */
	if (read_reg(base_addr, STATUS) & 0x01) {
		write_reg(base_addr, 1, SRESET);
		while (read_reg(base_addr, WPF_IRQ_STA(0)) == 0);
		write_reg(base_addr, 0, WPF_IRQ_STA(0));
	}

	/* WPF: destination setting */
	if (is_ycbcr_planar(dst->format))
		vio6_set_dst2(vio, dst->py, dst->pc, dst->pc2);
	else
		vio6_set_dst(vio, dst->py, dst->pc);
	write_reg(base_addr, SRC_RPF0_MAIN, WPF_SRCRPF(0));
	write_reg(base_addr, 0, WPF_HSZCLIP(0));
	write_reg(base_addr, 0, WPF_VSZCLIP(0));
	write_reg(base_addr, RND_CBRM_ROUND|RND_ABRM_ROUND, WPF_RNDCTRL(0));
	val = size_y(dst->format, dst->pitch, dst->bpitchy);
	write_reg(base_addr, val, WPF_DSTM_STRIDE_Y(0));
	if (is_ycbcr_planar(dst->format))
		val = size_c(dst->format, dst->pitch, dst->bpitchc);
	else
		val = size_y(dst->format, dst->pitch, dst->bpitchc);
	write_reg(base_addr, val, WPF_DSTM_STRIDE_C(0));
	write_reg(base_addr, PRIO_ICB, WPF_CHPRI_CTRL(0));

	/* RPF/WPF: color conversion setting */
	viofmt = fmt_info(src->format);
	write_reg(base_addr, viofmt->fmtid, RPF_INFMT(0));
#if defined(__LITTLE_ENDIAN__)
	write_reg(base_addr, viofmt->dswap, RPF_DSWAP(0));
#else
	write_reg(base_addr, 0, RPF_DSWAP(0));
#endif
	viofmt = fmt_info(dst->format);
	val = viofmt->fmtid;
	if (is_ycbcr(src->format) == is_rgb(dst->format))
		val |= FMT_DO_CSC;
	val |= FMT_PXA_DPR;	/* fill PAD with alpha value
				   passed through DPR */
	write_reg(base_addr, val, WPF_OUTFMT(0));
#if defined(__LITTLE_ENDIAN__)
	write_reg(base_addr, viofmt->dswap, WPF_DSWAP(0));
#else
	write_reg(base_addr, 0, WPF_DSWAP(0));
#endif

	/* RPF: source setting */
	if (is_ycbcr_planar(src->format))
		vio6_set_src2(vio, src->py, src->pc, src->pc2);
	else
		vio6_set_src(vio, src->py, src->pc);
	write_reg(base_addr, 0, RPF_LOC(0));
	if (src->format == REN_ARGB32)
		write_reg(base_addr, 0, RPF_ALPH_SEL(0));
	else
		write_reg(base_addr, 4 << 28, RPF_ALPH_SEL(0));
	write_reg(base_addr, 0, RPF_VRTCOL_SET(0));
	/* RPF_MSKCTRL, RPF_MSKSET0, RPF_MSKSET1,
	   RPF_CKEY_CTRL, RPF_CKEY_SET0, RPF_CKEY_SET1 */
	write_reg(base_addr, (src->w << 16) | src->h, RPF_SRC_BSIZE(0));
	write_reg(base_addr, (src->w << 16) | src->h, RPF_SRC_ESIZE(0));
	val = size_y(src->format, src->pitch, src->bpitchy);
	val = val << 16;
	if (is_ycbcr_planar(src->format))
		val |= size_c(src->format, src->pitch, src->bpitchc);
	else
		val |= size_y(src->format, src->pitch, src->bpitchc);
	write_reg(base_addr, val, RPF_SRCM_PSTRIDE(0));
	val = size_a(src->format, src->pitch, src->bpitcha);
	write_reg(base_addr, val, RPF_SRCM_ASTRIDE(0));
	write_reg(base_addr, PRIO_ICB, RPF_CHPRI_CTRL(0));

	/* UDF: scale setting */
	write_reg(base_addr, UDS_FMD | UDS_AON, UDS_CTRL(0));
	write_reg(base_addr, 0, UDS_SCALE(0));
	write_reg(base_addr, 0, UDS_PASS_BWIDTH(0));
	set_scale(vio, base_addr, 0, src->w, dst->w);
	set_scale(vio, base_addr, 1, src->h, dst->h);
	write_reg(base_addr, dst->w << 16 | dst->h, UDS_CLIP_SIZE(0));
	write_reg(base_addr, 0, UDS_FILL_COLOR(0));

	/* DPR: node link setting */
	write_reg(base_addr, (9 << 24) | (31 << 16) |
		  (31 << 8) | 31, DPR_CTRL(0));	/* RPF0 -> UDS0 */
	write_reg(base_addr, (31 << 24) |
		  (26 << 8), DPR_CTRL(1));	/* UDS0 -> WPF0 */
	write_reg(base_addr, 31 << 16, DPR_CTRL(2));
	write_reg(base_addr, (31 << 8) | (31 << 16), DPR_CTRL(3));
	write_reg(base_addr, 0, DPR_FXA);
	write_reg(base_addr, 0, DPR_FPORCH(0));
	write_reg(base_addr, 0, DPR_FPORCH(1));
	write_reg(base_addr, (5 << 16) | (5 << 8) | 5, DPR_FPORCH(2));
	write_reg(base_addr, 5 << 24, DPR_FPORCH(3));

	return 0;
fail:
	uiomux_unlock(vio->uiomux, vio->uiores);
	return -1;
}

static void
vio6_start(SHVIO *vio)
{
	void *base_addr = vio->uio_mmio.iomem;

	/* enable interrupt in VIO */
	write_reg(base_addr, 1, WPF_IRQ_ENB(0));

	/* start operation */
	write_reg(base_addr, 1, CMD0);
}

static int
vio6_wait(SHVIO *vio)
{
	void *base_addr = vio->uio_mmio.iomem;
	uint32_t vevtr;
	uint32_t vstar;
	int complete = 0;

	uiomux_sleep(vio->uiomux, vio->uiores);

	vevtr = read_reg(base_addr, WPF_IRQ_STA(0));
	write_reg(base_addr, 0, WPF_IRQ_STA(0));   /* ack interrupts */

	/* End of VIO operation? */
	complete = vevtr & 1;

	return complete;
}

const struct shvio_operations vio6_ops = {
	.setup = vio6_setup,
	.set_src = vio6_set_src,
	.set_src_phys = vio6_set_src_phys,
	.set_dst = vio6_set_dst,
	.set_dst_phys = vio6_set_dst_phys,
	.start = vio6_start,
	.wait = vio6_wait,
};
