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
#include <pthread.h>
#include <time.h>

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

#define VIO6_NUM_ENTITIES	(5 + 4 + 2 + 1 + 1)

static struct shvio_entity vio6_ent[] = {
	/* RPF */
	{
		.idx	=	0,
		.dpr_target	=	-1,
		.dpr_ctrl	=	0,
		.dpr_shift	=	24,
		.funcs	=	SHVIO_FUNC_SRC | SHVIO_FUNC_CSC,
		.lock	=	PTHREAD_MUTEX_INITIALIZER,
	},
	{
		.idx	=	1,
		.dpr_target	=	-1,
		.dpr_ctrl	=	0,
		.dpr_shift	=	16,
		.funcs	=	SHVIO_FUNC_SRC | SHVIO_FUNC_CSC,
		.lock	=	PTHREAD_MUTEX_INITIALIZER,
	},
	{
		.idx	=	2,
		.dpr_target	=	-1,
		.dpr_ctrl	=	0,
		.dpr_shift	=	8,
		.funcs	=	SHVIO_FUNC_SRC | SHVIO_FUNC_CSC,
		.lock	=	PTHREAD_MUTEX_INITIALIZER,
	},
	{
		.idx	=	3,
		.dpr_target	=	-1,
		.dpr_ctrl	=	0,
		.dpr_shift	=	0,
		.funcs	=	SHVIO_FUNC_SRC | SHVIO_FUNC_CSC,
		.lock	=	PTHREAD_MUTEX_INITIALIZER,
	},
	{
		.idx	=	4,
		.dpr_target	=	-1,
		.dpr_ctrl	=	1,
		.dpr_shift	=	24,
		.funcs	=	SHVIO_FUNC_SRC | SHVIO_FUNC_CSC,
		.lock	=	PTHREAD_MUTEX_INITIALIZER,
	},
	/* WPF */
	{
		.idx	=	0,
		.dpr_target	=	26,
		.dpr_ctrl	=	-1,
		.dpr_shift	=	-1,
		.funcs	=	SHVIO_FUNC_SINK | SHVIO_FUNC_CSC,
		.lock	=	PTHREAD_MUTEX_INITIALIZER,
	},
	{
		.idx	=	1,
		.dpr_target	=	27,
		.dpr_ctrl	=	-1,
		.dpr_shift	=	-1,
		.funcs	=	SHVIO_FUNC_SINK | SHVIO_FUNC_CSC,
		.lock	=	PTHREAD_MUTEX_INITIALIZER,
	},
	{
		.idx	=	2,
		.dpr_target	=	28,
		.dpr_ctrl	=	-1,
		.dpr_shift	=	-1,
		.funcs	=	SHVIO_FUNC_SINK | SHVIO_FUNC_CSC,
		.lock	=	PTHREAD_MUTEX_INITIALIZER,
	},
	{
		.idx	=	3,
		.dpr_target	=	29,
		.dpr_ctrl	=	-1,
		.dpr_shift	=	-1,
		.funcs	=	SHVIO_FUNC_SINK | SHVIO_FUNC_CSC,
		.lock	=	PTHREAD_MUTEX_INITIALIZER,
	},
	/* UDS */
	{
		.idx	=	0,
		.dpr_target	=	9,
		.dpr_ctrl	=	1,
		.dpr_shift	=	8,
		.funcs	=	SHVIO_FUNC_SCALE | SHVIO_FUNC_CROP,
		.lock	=	PTHREAD_MUTEX_INITIALIZER,
	},
	{
		.idx	=	1,
		.dpr_target	=	22,
		.dpr_ctrl	=	3,
		.dpr_shift	=	8,
		.funcs	=	SHVIO_FUNC_SCALE | SHVIO_FUNC_CROP,
		.lock	=	PTHREAD_MUTEX_INITIALIZER,
	},
	/* LUT */
	{
		.idx	=	0,
		.dpr_target	=	12,
		.dpr_ctrl	=	2,
		.dpr_shift	=	16,
		.funcs	=	SHVIO_FUNC_EFFECT,
		.lock	=	PTHREAD_MUTEX_INITIALIZER,
	},
	/* BRU */
	{
		.idx	=	0,
		.dpr_target	=	13,
		.dpr_ctrl	=	3,
		.dpr_shift	=	16,
		.funcs	=	SHVIO_FUNC_BLEND,
		.lock	=	PTHREAD_MUTEX_INITIALIZER,
	},
};

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

static void set_scale(void *base_addr, int id, int vertical,
		      int size_in, int size_out)
{
	uint32_t fixpoint, mant, frac, value, vb;

	/* calculate FRAC and MANT */
	if (size_in != size_out) {
		fixpoint = (4096 * size_in) / size_out;
		mant = fixpoint / 4096;
		frac = fixpoint - (mant * 4096);
	} else {
		mant = 1;
		frac = 0;
	}

	/* set scale */
	value = read_reg(base_addr, UDS_SCALE(id));
	if (vertical) {
		value &= ~0xffff;
		value |= (mant << 12) | frac;
	} else {
		value &= ~0xffff0000;
		value |= ((mant << 12) | frac) << 16;
	}
	write_reg(base_addr, value, UDS_SCALE(id));

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
	value = read_reg(base_addr, UDS_PASS_BWIDTH(id));
	if (vertical) {
		value &= ~0xffff;
		value |= vb;
	} else {
		value &= ~0xffff0000;
		value |= vb << 16;
	}
	write_reg(base_addr, value, UDS_PASS_BWIDTH(id));
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

static void
vio6_reset(SHVIO *vio)
{
	struct shvio_entity *entity;
	void *base_addr = vio->uio_mmio.iomem;
	const struct timespec timeout = {
		.tv_sec = 0,
		.tv_nsec = 1000 * 1000,
	};
	uint32_t val;
	int i;

	/* look for the sink entity */
	entity = vio->locked_entities;
	while (entity != NULL &&
	       ((entity->funcs & SHVIO_FUNC_SINK) == 0))
		entity = entity->list_next;

	if (entity == NULL) {
		debug_info("ERR: no sink entity");
		return;
	}

	base_addr = vio->uio_mmio.iomem;

	/* WPF: disable interrupt */
	write_reg(base_addr, 0, WPF_IRQ_ENB(entity->idx));

	/* WPF: software reset */
	if (read_reg(base_addr, STATUS) & (1 << entity->idx)) {
		write_reg(base_addr, 1 << entity->idx, SRESET);
		for (i=0; i<10; i++) {
			if (read_reg(base_addr, WPF_IRQ_STA(entity->idx)) != 0)
				break;
			nanosleep(&timeout, NULL);	/* wait 1ms */
		}
		write_reg(base_addr, 0, WPF_IRQ_STA(entity->idx));
	}

	/* DPR: set the termination for routing registers */
	for (i=VIO6_NUM_ENTITIES-1; i>=0; i--) {
		if (vio6_ent[i].dpr_ctrl < 0)
			continue;
		val = read_reg(base_addr, DPR_CTRL(vio6_ent[i].dpr_ctrl));
		if ((val & (0x1f << vio6_ent[i].dpr_shift)) != 0)
			continue;
		val |= 0x1f << vio6_ent[i].dpr_shift;
		write_reg(base_addr, val, DPR_CTRL(vio6_ent[i].dpr_ctrl));
	}

	write_reg(base_addr, 0, DPR_FXA);
	write_reg(base_addr, 0, DPR_FPORCH(0));
	write_reg(base_addr, 0, DPR_FPORCH(1));
	write_reg(base_addr, (5 << 16) | (5 << 8) | 5, DPR_FPORCH(2));
	write_reg(base_addr, 5 << 24, DPR_FPORCH(3));
}

static void
vio6_rpf_setup(SHVIO *vio, struct shvio_entity *entity,
	       const struct ren_vid_surface *src,
	       const struct ren_vid_surface *dst)
{
	void *base_addr = vio->uio_mmio.iomem;
	const struct vio_format_info *viofmt;
	uint32_t val;
	uint32_t Y, Cb;

	viofmt = fmt_info(src->format);
	val = viofmt->fmtid;
	if (is_ycbcr(src->format) == is_rgb(dst->format))
		val |= FMT_DO_CSC;
	write_reg(base_addr, val, RPF_INFMT(entity->idx));
#if defined(__LITTLE_ENDIAN__)
	write_reg(base_addr, viofmt->dswap, RPF_DSWAP(entity->idx));
#else
	write_reg(base_addr, 0, RPF_DSWAP(entity->idx));
#endif

	/* RPF: source setting */
	Y = uiomux_all_virt_to_phys(src->py);
	write_reg(base_addr, Y, RPF_SRCM_ADDR_Y(entity->idx));
	Cb = uiomux_all_virt_to_phys(src->pc);
	write_reg(base_addr, Cb, RPF_SRCM_ADDR_C0(entity->idx));
	if (is_ycbcr_planar(src->format)) {
		uint32_t Cr;
		Cr = uiomux_all_virt_to_phys(src->pc2);
		write_reg(base_addr, Cr, RPF_SRCM_ADDR_C1(entity->idx));
	}
	write_reg(base_addr, 0, RPF_LOC(entity->idx));
	if (src->format == REN_ARGB32)
		write_reg(base_addr, 0, RPF_ALPH_SEL(entity->idx));
	else
		write_reg(base_addr, 4 << 28, RPF_ALPH_SEL(entity->idx));
	write_reg(base_addr, 0xff << 24, RPF_VRTCOL_SET(entity->idx));
	/* RPF_MSKCTRL, RPF_MSKSET0, RPF_MSKSET1,
	   RPF_CKEY_CTRL, RPF_CKEY_SET0, RPF_CKEY_SET1 */
	write_reg(base_addr, (src->w << 16) | src->h, RPF_SRC_BSIZE(entity->idx));
	write_reg(base_addr, (src->w << 16) | src->h, RPF_SRC_ESIZE(entity->idx));
	val = size_y(src->format, src->pitch, src->bpitchy);
	val = val << 16;
	if (is_ycbcr_planar(src->format))
		val |= size_c(src->format, src->pitch, src->bpitchc);
	else
		val |= size_y(src->format, src->pitch, src->bpitchc);
	write_reg(base_addr, val, RPF_SRCM_PSTRIDE(entity->idx));
	val = size_a(src->format, src->pitch, src->bpitcha);
	write_reg(base_addr, val, RPF_SRCM_ASTRIDE(entity->idx));
	write_reg(base_addr, PRIO_ICB, RPF_CHPRI_CTRL(entity->idx));
}

static inline void rpfact(struct shvio_entity *entity, uint32_t *val)
{
	int i;

	for (i=0; i<N_INPADS; i++)
		if (entity->pad_in[i] != NULL)
			rpfact(entity->pad_in[i], val);
	if (entity->funcs & SHVIO_FUNC_SRC)
		*val |= 1 << (entity->idx * 2);

}

static void
vio6_wpf_setup(SHVIO *vio, struct shvio_entity *entity,
	       const struct ren_vid_surface *src,
	       const struct ren_vid_surface *dst)
{
	void *base_addr = vio->uio_mmio.iomem;
	const struct vio_format_info *viofmt;
	uint32_t val;
	uint32_t Y, Cb;

	/* WPF: destination setting */
	Y = uiomux_all_virt_to_phys(dst->py);
	write_reg(base_addr, Y, WPF_DSTM_ADDR_Y(entity->idx));
	Cb = uiomux_all_virt_to_phys(dst->pc);
	write_reg(base_addr, Cb, WPF_DSTM_ADDR_C0(entity->idx));
	if (is_ycbcr_planar(dst->format)) {
		uint32_t Cr;
		Cr = uiomux_all_virt_to_phys(dst->pc2);
		write_reg(base_addr, Cr, WPF_DSTM_ADDR_C1(entity->idx));
	}

	val = 0;
	rpfact(entity, &val);
	write_reg(base_addr, val, WPF_SRCRPF(entity->idx));
	write_reg(base_addr, 0, WPF_HSZCLIP(entity->idx));
	write_reg(base_addr, 0, WPF_VSZCLIP(entity->idx));
	write_reg(base_addr, RND_CBRM_ROUND|RND_ABRM_ROUND, WPF_RNDCTRL(entity->idx));
	val = size_y(dst->format, dst->pitch, dst->bpitchy);
	write_reg(base_addr, val, WPF_DSTM_STRIDE_Y(entity->idx));
	if (is_ycbcr_planar(dst->format))
		val = size_c(dst->format, dst->pitch, dst->bpitchc);
	else
		val = size_y(dst->format, dst->pitch, dst->bpitchc);
	write_reg(base_addr, val, WPF_DSTM_STRIDE_C(entity->idx));
	write_reg(base_addr, PRIO_ICB, WPF_CHPRI_CTRL(entity->idx));

	viofmt = fmt_info(dst->format);
	val = viofmt->fmtid;
	if (is_ycbcr(src->format) == is_rgb(dst->format))
		val |= FMT_DO_CSC;
	val |= FMT_PXA_DPR;	/* fill PAD with alpha value
				   passed through DPR */
	write_reg(base_addr, val, WPF_OUTFMT(entity->idx));
#if defined(__LITTLE_ENDIAN__)
	write_reg(base_addr, viofmt->dswap, WPF_DSWAP(entity->idx));
#else
	write_reg(base_addr, 0, WPF_DSWAP(entity->idx));
#endif
}

static void
vio6_uds_setup(SHVIO *vio, struct shvio_entity *entity,
	       const struct ren_vid_surface *src,
	       const struct ren_vid_surface *dst)
{
	void *base_addr = vio->uio_mmio.iomem;

	/* UDF: scale setting */
	write_reg(base_addr, UDS_AMD | UDS_FMD | UDS_AON, UDS_CTRL(entity->idx));
	write_reg(base_addr, 0, UDS_SCALE(entity->idx));
	write_reg(base_addr, 0, UDS_PASS_BWIDTH(entity->idx));
	set_scale(base_addr, entity->idx, 0, src->w, dst->w);
	set_scale(base_addr, entity->idx, 1, src->h, dst->h);
	write_reg(base_addr, dst->w << 16 | dst->h, UDS_CLIP_SIZE(entity->idx));
	write_reg(base_addr, 0, UDS_FILL_COLOR(entity->idx));

}

static void
vio6_unlink(SHVIO *vio, struct shvio_entity *entity)
{
	void *base_addr = vio->uio_mmio.iomem;
	struct shvio_entity *prev_entity;
	uint32_t val;
	int i;

	if (entity->pad_out != NULL) {
		for (i=0; i<N_INPADS; i++) {
			if (entity->pad_out->pad_in[i] == entity) {
				entity->pad_out->pad_in[i] = NULL;
				break;
			}
		}

		if (entity->dpr_ctrl >= 0) {
			val = read_reg(base_addr, DPR_CTRL(entity->dpr_ctrl));
			val |= 0x1f << entity->dpr_shift;
			write_reg(base_addr, val, DPR_CTRL(entity->dpr_ctrl));
		}
	}

	for (i=0; i<N_INPADS; i++) {
		prev_entity = entity->pad_in[i];
		if (prev_entity != NULL) {
			prev_entity->pad_out = NULL;
			val = read_reg(base_addr, DPR_CTRL(prev_entity->dpr_ctrl));
			val |= 0x1f << prev_entity->dpr_shift;
			write_reg(base_addr, val, DPR_CTRL(prev_entity->dpr_ctrl));
		}
	}
}

static int
vio6_link(SHVIO *vio, struct shvio_entity *src, struct shvio_entity *sink, int sinkpad)
{
	void *base_addr = vio->uio_mmio.iomem;
	uint32_t val;

	if ((src->pad_out != NULL) ||
	    (sinkpad < 0) || (sinkpad >= N_INPADS) ||
	    (sink->pad_in[sinkpad] != NULL)) {
		debug_info("ERR: a pad already linked");
		return -1;
	}

	val = read_reg(base_addr, DPR_CTRL(src->dpr_ctrl));
	val &= ~(0x1f << src->dpr_shift);
	val |= sink->dpr_target << src->dpr_shift;
	write_reg(base_addr, val, DPR_CTRL(src->dpr_ctrl));

	sink->pad_in[sinkpad] = src;
	src->pad_out = sink;

	return 0;
}

static void
vio6_unlock(SHVIO *vio, struct shvio_entity *entity)
{
	/* confirm 'unlinked' */
	if ((entity->pad_in[0] != NULL) ||
	    (entity->pad_out != NULL))
		vio6_unlink(vio, entity);

	if (entity->list_prev)
		entity->list_prev->list_next = entity->list_next;
	if (entity->list_next)
		entity->list_next->list_prev = entity->list_prev;
	if (vio->locked_entities == entity)
		vio->locked_entities = entity->list_next;

	pthread_mutex_unlock(&entity->lock);
}

static struct shvio_entity *
vio6_lock(SHVIO *vio, int func)
{
	struct shvio_entity *ent;
	int i;
	int ret;

	for (i=0; i<VIO6_NUM_ENTITIES; i++) {
		if (vio6_ent[i].funcs & func) {
			ret = pthread_mutex_trylock(&vio6_ent[i].lock);
			if (ret != 0)
				continue;
			memset(vio6_ent[i].pad_in, 0, sizeof(struct shvio_entity *) * N_INPADS);
			vio6_ent[i].pad_out = NULL;
			vio6_ent[i].list_prev = NULL;
			vio6_ent[i].list_next = vio->locked_entities;
			if (vio->locked_entities)
				vio->locked_entities->list_prev = &vio6_ent[i];
			vio->locked_entities = &vio6_ent[i];
			return &vio6_ent[i];
		}
	}

	debug_info("LOG: no entity found");
	return NULL;
}

static int
vio6_setup(
	SHVIO *vio,
	const struct ren_vid_surface *src,
	const struct ren_vid_surface *dst,
	shvio_rotation_t rotate)
{
	float scale_x, scale_y;
	uint32_t Y, C;
	const struct vio_format_info *src_info;
	const struct vio_format_info *dst_info;
	uint32_t val;
	void *base_addr;
	struct shvio_entity *ent_src, *ent_scale, *ent_sink;
	int ret;

	src_info = fmt_info(src->format);
	dst_info = fmt_info(dst->format);

	if (!format_supported(src->format) ||
	    !format_supported(dst->format)) {
		debug_info("ERR: Invalid surface format!");
		return -1;
	}

	ent_src = vio6_lock(vio, SHVIO_FUNC_SRC);
	ent_scale = vio6_lock(vio, SHVIO_FUNC_SCALE);
	ent_sink = vio6_lock(vio, SHVIO_FUNC_SINK);

	if ((ent_src == NULL) ||
	    (ent_scale == NULL) || (ent_sink == NULL)) {
		debug_info("ERR: No entity unavailable!");
		goto fail_lock_entities;
	}

	vio6_reset(vio);
	ret = vio6_link(vio, ent_src, ent_scale, 0);	/* make a link from src to scale */
	if (ret < 0) {
		debug_info("ERR: cannot make a link from src to scale");
		goto fail_link_entities;
	}
	vio6_rpf_setup(vio, ent_src, src, src);	/* color */
	vio6_uds_setup(vio, ent_scale, src, dst);	/* width, height */

	ret = vio6_link(vio, ent_scale, ent_sink, 0);	/* make a link from scale to sink */
	if (ret < 0) {
		debug_info("ERR: cannot make a link from scale to sink");
		goto fail_link_entities;
	}
	vio6_wpf_setup(vio, ent_sink, src, dst);	/* color */

	return 0;
fail_link_entities:
fail_lock_entities:
	while (vio->locked_entities != NULL)
		vio6_unlock(vio, vio->locked_entities);
	return -1;
}

static void
vio6_start(SHVIO *vio)
{
	void *base_addr = vio->uio_mmio.iomem;
	struct shvio_entity *entity;

	entity = vio->locked_entities;
	while (entity != NULL &&
	       ((entity->funcs & SHVIO_FUNC_SINK) == 0))
		entity = entity->list_next;

	if (entity == NULL)
		return;

	/* enable interrupt in VIO */
	write_reg(base_addr, 1, WPF_IRQ_ENB(entity->idx));

	/* start operation */
	write_reg(base_addr, 1, CMD(entity->idx));
}

static int
vio6_wait(SHVIO *vio)
{
	void *base_addr = vio->uio_mmio.iomem;
	uint32_t vevtr;
	uint32_t vstar;
	int complete;
	struct shvio_entity *entity;

	/* look for a sink entity */
	entity = vio->locked_entities;
	while (entity != NULL &&
	       ((entity->funcs & SHVIO_FUNC_SINK) == 0))
		entity = entity->list_next;

	if (entity == NULL)
		return -1;

	do {
		/* wait for an interrupt */
		uiomux_sleep(vio->uiomux, vio->uiores);

		/* confirm the status */
		vevtr = read_reg(base_addr, WPF_IRQ_STA(entity->idx));
		complete = vevtr & 1;
	} while (complete == 0);	/* End of VIO operation? */

	write_reg(base_addr, 0, WPF_IRQ_STA(entity->idx));   /* ack interrupts */

	/* unlock all entities */
	while (vio->locked_entities != NULL)
		vio6_unlock(vio, vio->locked_entities);

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
