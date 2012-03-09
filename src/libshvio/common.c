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
		printf("COPT A PLANE!!\n");
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

void
shvio_start_bundle(
	SHVIO *vio,
	int bundle_lines)
{
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
