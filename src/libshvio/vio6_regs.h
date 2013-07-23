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

#ifndef __SHVIO6_REGS_H__
#define __SHVIO6_REGS_H__

/* common control */
#define CMD(_n)			\
	(0x0000	+ ((_n) * 0x0004))	/* start/stop */
#define SRESET			0x0018	/* software reset */
#define STATUS			0x0020	/* status */
#define WPF_IRQ_ENB(_n)		\
	(0x0030 + ((_n) * 0x000c))	/* interrupt enable/disable */
#define WPF_IRQ_STA(_n)		\
	(0x0034 + ((_n) * 0x000c))	/* interrupt status */

/* RPF control */
#define RPF_SRC_BSIZE(_n)	\
	(0x0300 + ((_n) * 0x0100))	/* basic input size */
#define RPF_SRC_ESIZE(_n)	\
	(0x0304 + ((_n) * 0x0100))	/* extended input size */
#define RPF_INFMT(_n)		\
	(0x0308 + ((_n) * 0x0100))	/* input color format */
#define FMT_YCBCR420SP		0x42
#define FMT_YCBCR422SP		0x41
#define FMT_YCBCR420P		0x4c
#define FMT_YCBCR422P		0x4b
#define FMT_YCBCR422I		0x47
#define FMT_XRGB1555		0x1b
#define FMT_RGB565		0x06
#define FMT_RGB888		0x15
#define FMT_BGR888		0x18
#define FMT_ARGB8888		0x13
#define FMT_RGBX888		0x14
#define FMT_DO_CSC		(1 << 8)
#define FMT_WRTM_FULL_RANGE	(1 << 9)
#define FMT_WRTM_BT709		(1 << 10)
#define FMT_PXA_DPR		(1 << 23)
#define FMT_VIR			(1 << 28)

#define RPF_DSWAP(_n)		\
	(0x030c + ((_n) * 0x0100))	/* data swap setting */
#define RPF_LOC(_n)		\
	(0x0310 + ((_n) * 0x0100))	/* position for display */
#define RPF_ALPH_SEL(_n)	\
	(0x0314 + ((_n) * 0x0100))	/* alpha plane select */
#define RPF_VRTCOL_SET(_n)	\
	(0x0318 + ((_n) * 0x0100))	/* color for pseudo plane */
#define RPF_MSKCTRL(_n)		\
	(0x031c + ((_n) * 0x0100))	/* mask setting */
#define RPF_MSKSET0(_n)		\
	(0x0320 + ((_n) * 0x0100))	/* IROP-SRC input setting 0 */
#define RPF_MSKSET1(_n)		\
	(0x0324 + ((_n) * 0x0100))	/* IROP-SRC input setting 1 */
#define RPF_CKEY_CTRL(_n)	\
	(0x0328 + ((_n) * 0x0100))	/* color key control */
#define RPF_CKEY_SET0(_n)	\
	(0x032c + ((_n) * 0x0100))	/* color key value 0 */
#define RPF_CKEY_SET1(_n)	\
	(0x0330 + ((_n) * 0x0100))	/*  color key value 1 */
#define RPF_SRCM_PSTRIDE(_n)	\
	(0x0364 + ((_n) * 0x0100))	/* src picture memory slide */
#define RPF_SRCM_ASTRIDE(_n)	\
	(0x0368 + ((_n) * 0x0100))	/* src alpha memory slide */
#define RPF_SRCM_ADDR_Y(_n)	\
	(0x036c + ((_n) * 0x0100))	/* src Y address */
#define RPF_SRCM_ADDR_C0(_n)	\
	(0x0370 + ((_n) * 0x0100))	/* src C address 0 */
#define RPF_SRCM_ADDR_C1(_n)	\
	(0x0374 + ((_n) * 0x0100))	/* src C address 1 */
#define RPF_SRCM_ADDR_AI(_n)	\
	(0x0378 + ((_n) * 0x0100))	/* src alpha address */
#define RPF_CHPRI_CTRL(_n)	\
	(0x0380 + ((_n) * 0x0100))	/* bus access control */

/* WPF control */
#define WPF_SRCRPF(_n)		\
	(0x1000 + ((_n) * 0x0100))	/* */
#define SRC_RPF0_MAIN		(2 << 0)
#define SRC_RPF0_SUB		(1 << 0)
#define SRC_RPF1_MAIN		(2 << 2)
#define SRC_RPF1_SUB		(1 << 2)
#define SRC_RPF2_MAIN		(2 << 4)
#define SRC_RPF2_SUB		(1 << 4)
#define SRC_RPF3_MAIN		(2 << 6)
#define SRC_RPF3_SUB		(1 << 6)
#define SRC_RPF4_MAIN		(2 << 8)
#define SRC_RPF4_SUB		(1 << 8)
#define SRC_VIRT_MAIN		(2 << 28)
#define SRC_VIRT_SUB		(1 << 29)

#define WPF_HSZCLIP(_n)		\
	(0x1004 + ((_n) * 0x0100))	/* */
#define WPF_VSZCLIP(_n)		\
	(0x1008 + ((_n) * 0x0100))	/* */
#define WPF_OUTFMT(_n)		\
	(0x100c + ((_n) * 0x0100))	/* */
#define WPF_DSWAP(_n)		\
	(0x1010 + ((_n) * 0x0100))	/* */
#define WPF_RNDCTRL(_n)		\
	(0x1014 + ((_n) * 0x0100))	/* */
#define RND_CBRM_ROUND		(1 << 28)
#define RND_ABRM_ROUND		(1 << 24)

#define WPF_DSTM_STRIDE_Y(_n)		\
	(0x104c + ((_n) * 0x0100))	/* */
#define WPF_DSTM_STRIDE_C(_n)		\
	(0x1050 + ((_n) * 0x0100))	/* */
#define WPF_DSTM_ADDR_Y(_n)		\
	(0x1054 + ((_n) * 0x0100))	/* */
#define WPF_DSTM_ADDR_C0(_n)		\
	(0x1058 + ((_n) * 0x0100))	/* */
#define WPF_DSTM_ADDR_C1(_n)		\
	(0x105c + ((_n) * 0x0100))	/* */
#define WPF_CHPRI_CTRL(_n)		\
	(0x1060 + ((_n) * 0x0100))	/* */
#define PRIO_ICB	(1 << 16)

/* DPR control */
#define DPR_CTRL(_n)		\
	(0x2000 + ((_n) * 0x0004))	/* */
#define DPR_FXA			0x2010	/* start/stop */
#define DPR_FPORCH(_n)		\
	(0x2018 + ((_n) * 0x0004))	/* */

/* UDS control */
#define UDS_CTRL(_n)		\
	(0x2300 + ((_n) * 0x0800))	/* */
#define UDS_AMD			(1 << 30)
#define UDS_FMD			(1 << 29)
#define UDS_AON			(1 << 25)
#define UDS_ATHON		(1 << 24)
#define UDS_BC			(1 << 20)
#define UDS_NE_A		(1 << 19)
#define UDS_NE_RCR		(1 << 18)
#define UDS_NE_GY		(1 << 17)
#define UDS_NE_BCB		(1 << 16)

#define UDS_SCALE(_n)		\
	(0x2304 + ((_n) * 0x0800))	/* */
#define UDS_ALPTH(_n)		\
	(0x2308 + ((_n) * 0x0800))	/* */
#define UDS_ALPVAL(_n)		\
	(0x230c + ((_n) * 0x0800))	/* */
#define UDS_PASS_BWIDTH(_n)	\
	(0x2310 + ((_n) * 0x0800))	/* */
#define UDS_CLIP_SIZE(_n)	\
	(0x2324 + ((_n) * 0x0800))	/* */
#define UDS_FILL_COLOR(_n)	\
	(0x2328 + ((_n) * 0x0800))	/* */

/* 1D-LUT control */
#define LUT			0x2600	/* start/stop */

/* BRU control */
#define BRU_INCTRL		0x2a00	/* start/stop */
#define BRU_VIRRPF_SIZE		0x2a04	/* start/stop */
#define BRU_VIRRPF_LOC		0x2a08	/* start/stop */
#define BRU_VIRRPF_COL		0x2a0c	/* start/stop */
#define BRU_CTRL(_m)	\
	(0x2a10 + ((_m) * 0x0008))	/* */
#define BRU_BLD(_m)	\
	(0x2a14 + ((_m) * 0x0008))	/* */
#define BRU_ROP			0x2a30	/* start/stop */

#endif /* __SHVIO6_REGS_H__ */
