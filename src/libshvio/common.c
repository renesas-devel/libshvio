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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include <uiomux/uiomux.h>
#include "common.h"

struct shvio_operations veu_ops;
struct shvio_operations vio6_ops;

SHVIO *shvio_open_named(const char *name)
{
	SHVIO *vio;
	int ret;

	vio = calloc(1, sizeof(*vio));
	if (!vio)
		goto err;

	if (!name) {
		vio->uiomux = uiomux_open();
		vio->uiores = UIOMUX_SH_VEU;
		vio->ops = veu_ops;
	} else {
		const char *blocks[2] = { name, NULL };
		vio->uiomux = uiomux_open_named(blocks);
		vio->uiores = (1 << 0);
		if (strncmp(name, "VEU", 3) == 0)
			vio->ops = veu_ops;
		else if (strncmp(name, "VIO", 3) == 0)
			vio->ops = vio6_ops;
		else
			goto err;
	}
	if (!vio->uiomux)
		goto err;

	ret = uiomux_get_mmio (vio->uiomux, vio->uiores,
		&vio->uio_mmio.address,
		&vio->uio_mmio.size,
		&vio->uio_mmio.iomem);
	if (!ret)
		goto err;

	return vio;

err:
	debug_info("ERR: error detected");
	shvio_close(vio);
	return 0;
}

SHVIO *shvio_open(void)
{
	return shvio_open_named("VEU");
}

void shvio_close(SHVIO *vio)
{
	if (vio) {
		if (vio->uiomux)
			uiomux_close(vio->uiomux);
		free(vio);
	}
}

#define SHVIO_UIO_VIO_MAX	(8)
#define SHVIO_UIO_PREFIX	"VEU"
#define SHVIO_UIO_PREFIX_LEN	(3)

int
shvio_list_vio(char ***names, int *count)
{
	static char *cache[SHVIO_UIO_VIO_MAX];
	static int cache_count = -1;

	char **result;
	int i, n;;

	if (cache_count != -1)
		goto done;

	if (uiomux_list_device(&result, &n) < 0) {
		goto err;
	}


	/*
	 * XXX: We can return up to (SHVIO_UIO_VIO_MAX) VIO count.
	 * If there's more than (SHVIO_UIO_VIO_MAX) VIOs available
	 * in the future. It has to be extended.
	 */
	cache_count = 0;
	memset(cache, 0, sizeof(cache));
	for (i = 0; i < n && cache_count < SHVIO_UIO_VIO_MAX; i++) {
		if (!strncmp(SHVIO_UIO_PREFIX, result[i], SHVIO_UIO_PREFIX_LEN))
			cache[cache_count++] = result[i];
	}
done:
	*names = cache;
	*count = cache_count;
	return 0;

err:
	debug_info("ERR: error detected");
	return -1;
}

static void copy_plane(void *dst, void *src, int bpp, int h, int len, int dst_bpitch, int src_bpitch)
{
	int y;
	if (src && dst != src) {
		printf("MEMCPY a surface\n");
		for (y=0; y<h; y++) {
			memcpy(dst, src, len * bpp);
			src += src_bpitch;
			dst += dst_bpitch;
		}
	}
}

/* Copy active surface contents - assumes output is big enough */
static void copy_surface(
	struct ren_vid_surface *out,
	const struct ren_vid_surface *in)
{
	const struct format_info *fmt = &fmts[in->format];
	int src_bpitch, dst_bpitch;

	src_bpitch = (in->bpitchy != 0) ? in->bpitchy : in->pitch * fmt->y_bpp;
	dst_bpitch = (out->bpitchy != 0) ? out->bpitchy : out->pitch * fmt->y_bpp;
	copy_plane(out->py, in->py, fmt->y_bpp, in->h, in->w, dst_bpitch, src_bpitch);

	src_bpitch = (in->bpitchc != 0) ? in->bpitchc :
		in->pitch / fmt->c_ss_horz * fmt->c_bpp;
	dst_bpitch = (out->bpitchc != 0) ? out->bpitchc :
		out->pitch / fmt->c_ss_horz * fmt->c_bpp;
	copy_plane(out->pc, in->pc, fmt->c_bpp,
		in->h/fmt->c_ss_vert,
		in->w/fmt->c_ss_horz,
		src_bpitch,
		dst_bpitch);

	src_bpitch = (in->bpitcha != 0) ? in->bpitcha : in->pitch;
	dst_bpitch = (out->bpitcha != 0) ? out->bpitcha : out->pitch;
	copy_plane(out->pa, in->pa, 1, in->h, in->w, dst_bpitch, src_bpitch);
}

/* Check/create surface that can be accessed by the hardware */
static int get_hw_surface(
	UIOMux * uiomux,
	uiomux_resource_t resource,
	struct ren_vid_surface *out,
	const struct ren_vid_surface *in)
{
	int alloc = 0;

	if (in == NULL || out == NULL)
		return 0;

	*out = *in;
	if (in->py) alloc |= !uiomux_all_virt_to_phys(in->py);
	if (in->pc) alloc |= !uiomux_all_virt_to_phys(in->pc);

	if (alloc) {
		/* One of the supplied buffers is not usable by the hardware! */
		size_t len = size_y(in->format, in->h * in->w, 0);
		if (in->pc) len += size_c(in->format, in->h * in->w, 0);

		out->py = uiomux_malloc(uiomux, resource, len, 32);
		if (!out->py)
			return -1;

		if (in->pc) {
			out->pc = out->py + size_y(in->format, in->h * in->w, 0);
		}
	}

	return 0;
}

static void dbg(const char *str1, int l, const char *str2, const struct ren_vid_surface *s)
{
#ifdef DEBUG
	fprintf(stderr, "%s:%d: %s: (%dx%d) pitch=%d py=%p, pc=%p, pa=%p, bpitchy=%d, bpitchc=%d, bpitcha=%d\n", str1, l, str2, s->w, s->h, s->pitch, s->py, s->pc, s->pa, s->bpitchy, s->bpitchc, s->bpitcha);
#endif
}

int
shvio_setup(
	SHVIO *vio,
	const struct ren_vid_surface *src_surface,
	const struct ren_vid_surface *dst_surface,
	shvio_rotation_t filter_control)
{
	struct ren_vid_surface local_src;
	struct ren_vid_surface local_dst;
	struct ren_vid_surface *src = &local_src;
	struct ren_vid_surface *dst = &local_dst;

	dbg(__func__, __LINE__, "src_user", src_surface);
	dbg(__func__, __LINE__, "dst_user", dst_surface);

	if (!vio || !src_surface || !dst_surface) {
		debug_info("ERR: Invalid input - need src and dest");
		return -1;
	}

	/* source - use a buffer the hardware can access */
	if (get_hw_surface(vio->uiomux, vio->uiores, src, src_surface) < 0) {
		debug_info("ERR: src is not accessible by hardware");
		return -1;
	}

	copy_surface(src, src_surface);

	/* destination - use a buffer the hardware can access */
	if (get_hw_surface(vio->uiomux, vio->uiores, dst, dst_surface) < 0) {
		debug_info("ERR: dest is not accessible by hardware");
		goto fail_get_hw_surface_dst;
	}

	/* Keep track of the requested surfaces */
	vio->src_user = *src_surface;
	vio->dst_user = *dst_surface;

	/* Keep track of the actual surfaces used */
	vio->src_hw = local_src;
	vio->dst_hw = local_dst;

	uiomux_lock (vio->uiomux, vio->uiores);

	if (vio->ops.setup(vio, src, dst, filter_control) < 0)
		goto fail_setup;

	return 0;

fail_setup:
	uiomux_unlock(vio->uiomux, vio->uiores);

	if (vio->dst_hw.py != dst_surface->py) {
		size_t len = size_y(vio->dst_hw.format, vio->dst_hw.h * vio->dst_hw.w, 0);
		len += size_c(vio->dst_hw.format, vio->dst_hw.h * vio->dst_hw.w, 0);
		uiomux_free(vio->uiomux, vio->uiores, vio->dst_hw.py, len);
	}
fail_get_hw_surface_dst:
	if (vio->src_hw.py != src_surface->py) {
		size_t len = size_y(vio->src_hw.format, vio->src_hw.h * vio->src_hw.w, 0);
		len += size_c(vio->src_hw.format, vio->src_hw.h * vio->src_hw.w, 0);
		uiomux_free(vio->uiomux, vio->uiores, vio->src_hw.py, len);
	}

	return -1;
}

void
shvio_set_src(
	SHVIO *vio,
	void *src_py,
	void *src_pc)
{
	vio->ops.set_src(vio, src_py, src_pc);
}

void
shvio_set_src_phys(
	SHVIO *vio,
	uint32_t src_py,
	uint32_t src_pc)
{
	vio->ops.set_src_phys(vio, src_py, src_pc);
}

void
shvio_set_dst(
	SHVIO *vio,
	void *dst_py,
	void *dst_pc)
{
	vio->ops.set_dst(vio, dst_py, dst_pc);
}

void
shvio_set_dst_phys(
	SHVIO *vio,
	uint32_t dst_py,
	uint32_t dst_pc)
{
	vio->ops.set_dst_phys(vio, dst_py, dst_pc);
}

void
shvio_set_color_conversion(
	SHVIO *vio,
	int bt709,
	int full_range)
{
	vio->bt709 = bt709;
	vio->full_range = full_range;
}

void
shvio_start(SHVIO *vio)
{
	vio->ops.start(vio);
}

int
shvio_has_bundle(SHVIO *vio)
{
	return vio->ops.start_bundle ? 1 : 0;
}

void
shvio_start_bundle(
	SHVIO *vio,
	int bundle_lines)
{
	if (vio->ops.start_bundle)
		vio->ops.start_bundle(vio, bundle_lines);
}

int
shvio_wait(SHVIO *vio)
{
	int complete = 0;

	uiomux_sleep(vio->uiomux, vio->uiores);

	complete = vio->ops.wait(vio);

	if (complete) {
		dbg(__func__, __LINE__, "src_hw", &vio->src_hw);
		dbg(__func__, __LINE__, "dst_hw", &vio->dst_hw);
		copy_surface(&vio->dst_user, &vio->dst_hw);

		/* free locally allocated surfaces */
		if (vio->src_hw.py != vio->src_user.py) {
			size_t len = size_y(vio->src_hw.format, vio->src_hw.h * vio->src_hw.w, 0);
			len += size_c(vio->src_hw.format, vio->src_hw.h * vio->src_hw.w, 0);
			uiomux_free(vio->uiomux, vio->uiores, vio->src_hw.py, len);
		}
		if (vio->dst_hw.py != vio->dst_user.py) {
			size_t len = size_y(vio->dst_hw.format, vio->dst_hw.h * vio->dst_hw.w, 0);
			len += size_c(vio->dst_hw.format, vio->dst_hw.h * vio->dst_hw.w, 0);
			uiomux_free(vio->uiomux, vio->uiores, vio->dst_hw.py, len);
		}

		uiomux_unlock(vio->uiomux, vio->uiores);
	}

	return complete;
}

int
shvio_resize(
	SHVIO *vio,
	const struct ren_vid_surface *src_surface,
	const struct ren_vid_surface *dst_surface)
{
	int ret;

	ret = shvio_setup(vio, src_surface, dst_surface, SHVIO_NO_ROT);

	if (ret == 0) {
		shvio_start(vio);
		shvio_wait(vio);
	}

	return ret;
}

int
shvio_rotate(
	SHVIO *vio,
	const struct ren_vid_surface *src_surface,
	const struct ren_vid_surface *dst_surface,
	shvio_rotation_t rotate)
{
	int ret;

	ret = shvio_setup(vio, src_surface, dst_surface, rotate);

	if (ret == 0) {
		shvio_start(vio);
		shvio_wait(vio);
	}

	return ret;
}

int
shvio_fill(
	SHVIO *vio,
	const struct ren_vid_surface *dst_surface,
	uint32_t argb)
{
	struct ren_vid_surface local_dst;
	struct ren_vid_surface *dst = &local_dst;

	dbg(__func__, __LINE__, "dst_user", dst_surface);

	if (!vio || !dst_surface) {
		debug_info("ERR: Invalid input - need dest");
		return -1;
	}

	if (!vio->ops.fill) {
		debug_info("ERR: Unsupported by HW");
		return -1;
	}

	/* destination - use a buffer the hardware can access */
	if (get_hw_surface(vio->uiomux, vio->uiores, dst, dst_surface) < 0) {
		debug_info("ERR: dest is not accessible by hardware");
		goto fail_get_hw_surface_dst;
	}

	/* Keep track of the requested surfaces */
	memset(&vio->src_user, 0, sizeof(vio->src_user));
	vio->dst_user = *dst_surface;

	/* Keep track of the actual surfaces used */
	memset(&vio->src_hw, 0, sizeof(vio->src_hw));
	vio->dst_hw = local_dst;

	uiomux_lock (vio->uiomux, vio->uiores);

	if (vio->ops.fill(vio, dst, argb) < 0)
		goto fail_fill;

	shvio_start(vio);
	shvio_wait(vio);

	return 0;

fail_fill:
	uiomux_unlock(vio->uiomux, vio->uiores);

	if (vio->dst_hw.py != dst_surface->py) {
		size_t len = size_y(vio->dst_hw.format, vio->dst_hw.h * vio->dst_hw.w, 0);
		len += size_c(vio->dst_hw.format, vio->dst_hw.h * vio->dst_hw.w, 0);
		uiomux_free(vio->uiomux, vio->uiores, vio->dst_hw.py, len);
	}
fail_get_hw_surface_dst:

	return -1;
}
int
shvio_setup_blend(
	SHVIO *vio,
	const struct ren_vid_rect *virt,
	const struct ren_vid_surface *const *src_list,
	int src_count,
	const struct ren_vid_surface *dst)
{
	uiomux_lock (vio->uiomux, vio->uiores);

	if (vio->ops.setup_blend(vio, virt, src_list, src_count, dst) < 0)
		goto fail_setup_blend;

	return 0;

fail_setup_blend:
	uiomux_unlock(vio->uiomux, vio->uiores);

	return -1;
}

#if 0
int
shvio_start_blend(
	SHVIO *vio,
	const struct shvio_surface *src1_in,
	const struct shvio_surface *src2_in,
	const struct shvio_surface *src3_in,
	const struct shvio_surface *src4_in,
	const struct shvio_surface *dest_in)
{
	uint32_t start_reg;
	uint32_t control_reg;
	const struct shvio_surface *src_check;
	uint32_t bblcr1 = 0;
	uint32_t bblcr0 = 0;
	struct shvio_surface local_src1;
	struct shvio_surface local_src2;
	struct shvio_surface local_src3;
	struct shvio_surface local_dest;
	struct shvio_surface *src1 = NULL;
	struct shvio_surface *src2 = NULL;
	struct shvio_surface *src3 = NULL;
	struct shvio_surface *dest = NULL;
	void *base_addr;

	debug_info("in");

	if (src1_in) src1 = &local_src1;
	if (src2_in) src2 = &local_src2;
	if (src3_in) src3 = &local_src3;
	if (dest_in) dest = &local_dest;

	/* Check we have been passed at least an input and an output */
	if (!vio || !src1_in || !dest_in) {
		debug_info("ERR: Invalid input - need at least 1 src and dest");
		return -1;
	}

	/* Check the size of the destination surface is big enough */
	if (dest_in->s.pitch < src1_in->s.w) {
		debug_info("ERR: Size of the destination surface is not big enough");
		return -1;
	}

	/* Check the size of the destination surface matches the parent surface */
	if (dest_in->s.w != src1_in->s.w || dest_in->s.h != src1_in->s.h) {
		debug_info("ERR: Size of the destination surface does NOT match the parent surface");
		return -1;
	}

	/* surfaces - use buffers the hardware can access */
	if (get_hw_surface(vio, src1, src1_in) < 0) {
		debug_info("ERR: src1 is not accessible by hardware");
		return -1;
	}
	if (get_hw_surface(vio, src2, src2_in) < 0) {
		debug_info("ERR: src2 is not accessible by hardware");
		return -1;
	}
	if (get_hw_surface(vio, src3, src3_in) < 0) {
		debug_info("ERR: src3 is not accessible by hardware");
		return -1;
	}
	if (get_hw_surface(vio, dest, dest_in) < 0) {
		debug_info("ERR: dest is not accessible by hardware");
		return -1;
	}

	if (src1_in) copy_surface(&src1->s, &src1_in->s);
	if (src2_in) copy_surface(&src2->s, &src2_in->s);
	if (src3_in) copy_surface(&src3->s, &src3_in->s);

	/* NOTE: All register access must be inside this lock */
	uiomux_lock (vio->uiomux, vio->uiores);

	base_addr = vio->uio_mmio.iomem;

	/* Keep track of the user surfaces */
	vio->p_src1_user = (src1_in != NULL) ? &vio->src1_user : NULL;
	vio->p_src2_user = (src2_in != NULL) ? &vio->src2_user : NULL;
	vio->p_src3_user = (src3_in != NULL) ? &vio->src3_user : NULL;
	vio->p_dest_user = (dest_in != NULL) ? &vio->dest_user : NULL;

	if (src1_in) vio->src1_user = *src1_in;
	if (src2_in) vio->src2_user = *src2_in;
	if (src3_in) vio->src3_user = *src3_in;
	if (dest_in) vio->dest_user = *dest_in;

	/* Keep track of the actual surfaces used */
	vio->src1_hw = local_src1;
	vio->src2_hw = local_src2;
	vio->src3_hw = local_src3;
	vio->dest_hw = local_dest;
	src_check = src1;

	/* Ensure src2 and src3 formats are the same type (only input 1 on the
	   hardware has colorspace conversion */
	if (src2 && src3) {
		if (different_colorspace(src2->s.format, src3->s.format)) {
			if (different_colorspace(src1->s.format, src2->s.format)) {
				/* src2 is the odd one out, swap 1 and 2 */
				struct shvio_surface *tmp = src2;
				src2 = src1;
				src1 = tmp;
				bblcr1 = (1 << 24);
				bblcr0 = (2 << 24);
			} else {
				/* src3 is the odd one out, swap 1 and 3 */
				struct shvio_surface *tmp = src3;
				src3 = src1;
				src1 = tmp;
				bblcr1 = (2 << 24);
				bblcr0 = (5 << 24);
			}
		}
	}

	if (read_reg(base_addr, BSTAR)) {
		debug_info("BEU appears to be running already...");
	}

	/* Reset */
	write_reg(base_addr, 1, BBRSTR);

	/* Wait for BEU to stop */
	while (read_reg(base_addr, BSTAR) & 1)
		;

	/* Turn off register bank/plane access, access regs via Plane A */
	write_reg(base_addr, 0, BRCNTR);
	write_reg(base_addr, 0, BRCHR);

	/* Default location of surfaces is (0,0) */
	write_reg(base_addr, 0, BLOCR1);

	/* Default to no byte swapping for all surfaces (YCbCr) */
	write_reg(base_addr, 0, BSWPR);

	/* Turn off transparent color comparison */
	write_reg(base_addr, 0, BPCCR0);

	/* Turn on blending */
	write_reg(base_addr, 0, BPROCR);

	/* Not using "multi-window" capability */
	write_reg(base_addr, 0, BMWCR0);

	/* Set parent surface; output to memory */
	write_reg(base_addr, bblcr1 | BBLCR1_OUTPUT_MEM, BBLCR1);

	/* Set surface order */
	write_reg(base_addr, bblcr0, BBLCR0);

	if (setup_src_surface(base_addr, 0, src1) < 0)
		goto err;
	if (setup_src_surface(base_addr, 1, src2) < 0)
		goto err;
	if (setup_src_surface(base_addr, 2, src3) < 0)
		goto err;
	if (setup_dst_surface(base_addr, dest) < 0)
		goto err;

	if (src2) {
		if (different_colorspace(src1->s.format, src2->s.format)) {
			uint32_t bsifr = read_reg(base_addr, BSIFR + SRC1_BASE);
			debug_info("Setting BSIFR1 IN1TE bit");
			bsifr  |= (BSIFR1_IN1TE | BSIFR1_IN1TM);
			write_reg(base_addr, bsifr, BSIFR + SRC1_BASE);
		}

		src_check = src2;
	}

	/* Is input 1 colorspace (after the colorspace converter) RGB? */
	if (is_rgb(src_check->s.format)) {
		uint32_t bpkfr = read_reg(base_addr, BPKFR);
		debug_info("Setting BPKFR RY bit");
		bpkfr |= BPKFR_RY;
		write_reg(base_addr, bpkfr, BPKFR);
	}

	/* Is the output colorspace different to input? */
	if (different_colorspace(dest->s.format, src_check->s.format)) {
		uint32_t bpkfr = read_reg(base_addr, BPKFR);
		debug_info("Setting BPKFR TE bit");
		bpkfr |= (BPKFR_TM2 | BPKFR_TM | BPKFR_DITH1 | BPKFR_TE);
		write_reg(base_addr, bpkfr, BPKFR);
	}

	/* enable interrupt */
	write_reg(base_addr, 1, BEIER);

	/* start operation */
	start_reg = BESTR_BEIVK;
	if (src1) start_reg |= BESTR_CHON1;
	if (src2) start_reg |= BESTR_CHON2;
	if (src3) start_reg |= BESTR_CHON3;
	write_reg(base_addr, start_reg, BESTR);

	debug_info("out");

	return 0;

err:
	debug_info("ERR: error detected");
	uiomux_unlock(vio->uiomux, vio->uiores);
	return -1;
}

int
shvio_blend(
	SHVIO *vio,
	const struct ren_vid_surface *const *src_list,
	int src_count;
	const struct shvio_surface *dest)
{
	int ret = 0;

	ret = shvio_setup_blend(vio, src_list, src_count, dest);

	if (ret == 0) {
		shvio_start(vio)
		shvio_wait(vio);
	}

	return ret;
}
#endif

