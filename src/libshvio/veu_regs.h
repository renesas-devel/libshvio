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

#ifndef __SHVEU_REGS_H__
#define __SHVEU_REGS_H__

#define VESTR 0x00		/* start register */
#define VESWR 0x10		/* src: line length */
#define VESSR 0x14		/* src: image size */
#define VSAYR 0x18		/* src: y/rgb plane address */
#define VSACR 0x1c		/* src: c plane address */
#define VBSSR 0x20		/* bundle mode register */
#define VEDWR 0x30		/* dst: line length */
#define VDAYR 0x34		/* dst: y/rgb plane address */
#define VDACR 0x38		/* dst: c plane address */
#define VTRCR 0x50		/* transform control */
#define VRFCR 0x54		/* resize scale */
#define VRFSR 0x58		/* resize clip */
#define VENHR 0x5c		/* enhance */
#define VFMCR 0x70		/* filter mode */
#define VVTCR 0x74		/* lowpass vertical */
#define VHTCR 0x78		/* lowpass horizontal */
#define VAPCR 0x80		/* color match */
#define VECCR 0x84		/* color replace */
#define VAFXR 0x90		/* fixed mode */
#define VSWPR 0x94		/* swap */
#define VEIER 0xa0		/* interrupt mask */
#define VEVTR 0xa4		/* interrupt event */
#define VSTAR 0xb0		/* status */
#define VBSRR 0xb4		/* reset */
#define VRPBR 0xc8		/* resize passband */

#define VMCR00 0x200		/* color conversion matrix coefficient 00 */
#define VMCR01 0x204		/* color conversion matrix coefficient 01 */
#define VMCR02 0x208		/* color conversion matrix coefficient 02 */
#define VMCR10 0x20c		/* color conversion matrix coefficient 10 */
#define VMCR11 0x210		/* color conversion matrix coefficient 11 */
#define VMCR12 0x214		/* color conversion matrix coefficient 12 */
#define VMCR20 0x218		/* color conversion matrix coefficient 20 */
#define VMCR21 0x21c		/* color conversion matrix coefficient 21 */
#define VMCR22 0x220		/* color conversion matrix coefficient 22 */
#define VCOFFR 0x224		/* color conversion offset */
#define VCBR   0x228		/* color conversion clip */

#define VTRCR_DST_FMT_YCBCR420 (0 << 22)
#define VTRCR_DST_FMT_YCBCR422 (1 << 22)
#define VTRCR_DST_FMT_YCBCR444 (2 << 22)
#define VTRCR_DST_FMT_RGB565   (6 << 16)
#define VTRCR_DST_FMT_RGBX888  (19 << 16)
#define VTRCR_DST_FMT_RGB888   (21 << 16)	/* Packed 24-bit RGB */
#define VTRCR_DST_FMT_BGR888   (53 << 16)	/* Packed 24-bit BGR */
#define VTRCR_SRC_FMT_YCBCR420 (0 << 14)
#define VTRCR_SRC_FMT_YCBCR422 (1 << 14)
#define VTRCR_SRC_FMT_YCBCR444 (2 << 14)
#define VTRCR_SRC_FMT_RGB565   (3 << 8)
#define VTRCR_SRC_FMT_RGBX888  (0 << 8)
#define VTRCR_SRC_FMT_RGB888   (2 << 8)		/* Packed 24-bit RGB */
#define VTRCR_SRC_FMT_BGR888   (11 << 8)	/* Packed 24-bit BGR */
#define VTRCR_DITH             (1 << 4)
#define VTRCR_BT601            (0 << 3)
#define VTRCR_BT709            (1 << 3)
#define VTRCR_FULL_COLOR_CONV  (1 << 2)
#define VTRCR_TE_BIT_SET       (1 << 1)
#define VTRCR_RY_SRC_YCBCR     0
#define VTRCR_RY_SRC_RGB       1

#endif /* __SHVEU_REGS_H__ */
