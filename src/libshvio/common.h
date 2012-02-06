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

#ifndef __API_H__
#define __API_H__

#include <uiomux/uiomux.h>
#include "shvio/shvio.h"

#ifdef DEBUG
#define debug_info(s) fprintf(stderr, "%s: %s\n", __func__, s)
#else
#define debug_info(s)
#endif

struct uio_map {
	unsigned long address;
	unsigned long size;
	void *iomem;
};

struct shvio_operations {
	int (*setup)(SHVIO *vio, const struct ren_vid_surface *src_surface,
		     const struct ren_vid_surface *dst_surface,
		     shvio_rotation_t filter_control);
	void (*set_src)(SHVIO *vio, void *src_py, void *src_pc);
	void (*set_src_phys)(SHVIO *vio, uint32_t src_py, uint32_t src_pc);
	void (*set_dst)(SHVIO *vio, void *dst_py, void *dst_pc);
	void (*set_dst_phys)(SHVIO *vio, uint32_t dst_py, uint32_t dst_pc);
	void (*start)(SHVIO *vio);
	void (*start_bundle)(SHVIO *vio, int bundle_lines);
	int (*wait)(SHVIO *vio);
};

struct SHVIO {
	UIOMux *uiomux;
	uiomux_resource_t uiores;
	struct uio_map uio_mmio;
	struct ren_vid_surface src_user;
	struct ren_vid_surface src_hw;
	struct ren_vid_surface dst_user;
	struct ren_vid_surface dst_hw;
	int bt709;
	int full_range;

	struct shvio_operations ops;
};

#endif /* __API_H__ */
