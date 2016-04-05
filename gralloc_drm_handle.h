/*
 * Copyright (C) 2010-2011 Chia-I Wu <olvaffe@gmail.com>
 * Copyright (C) 2010-2011 LunarG Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _GRALLOC_DRM_HANDLE_H_
#define _GRALLOC_DRM_HANDLE_H_

#include <system/window.h>
#include <cutils/native_handle.h>
#include <system/graphics.h>
#include "format_chooser.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
	MALI_YUV_NO_INFO,
	MALI_YUV_BT601_NARROW,
	MALI_YUV_BT601_WIDE,
	MALI_YUV_BT709_NARROW,
	MALI_YUV_BT709_WIDE
} mali_gralloc_yuv_info;

typedef enum
{
	MALI_DPY_TYPE_UNKNOWN = 0,
	MALI_DPY_TYPE_CLCD,
	MALI_DPY_TYPE_HDLCD
} mali_dpy_type;

struct gralloc_drm_bo_t;

struct gralloc_drm_handle_t {
	native_handle_t base;

	/* file descriptors */
	int prime_fd;

        mali_dpy_type dpy_type;
#if MALI_AFBC_GRALLOC == 1
	int     share_attr_fd;
#endif

        uint64_t   internal_format;
        int        internalWidth;
        int        internalHeight;
        int        byte_stride;
        int        size;

        union {
                off_t    offset;
                uint64_t padding4;
        };

#if MALI_AFBC_GRALLOC == 1
	// locally mapped shared attribute area
	union {
		void*    attr_base;
		uint64_t padding3;
	};
#endif

	mali_gralloc_yuv_info yuv_info;

	/* integers */
	int magic;

	int width;
	int height;
	int format;
	int usage;

	int name;   /* the name of the bo */
	int stride; /* the stride in bytes */

	struct gralloc_drm_bo_t *data; /* pointer to struct gralloc_drm_bo_t */

	// FIXME: the attributes below should be out-of-line
	uint64_t unknown __attribute__((aligned(8)));
	int data_owner; /* owner of data (for validation) */
};
#define GRALLOC_DRM_HANDLE_MAGIC 0x12345678
#define GRALLOC_DRM_HANDLE_NUM_FDS 2
#define GRALLOC_DRM_HANDLE_NUM_INTS (						\
	((sizeof(struct gralloc_drm_handle_t) - sizeof(native_handle_t))/sizeof(int))	\
	 - GRALLOC_DRM_HANDLE_NUM_FDS)

static inline struct gralloc_drm_handle_t *gralloc_drm_handle(buffer_handle_t _handle)
{
	struct gralloc_drm_handle_t *handle =
		(struct gralloc_drm_handle_t *) _handle;

	if (handle && (handle->base.version != sizeof(handle->base) ||
	               handle->base.numInts != GRALLOC_DRM_HANDLE_NUM_INTS ||
	               handle->base.numFds != GRALLOC_DRM_HANDLE_NUM_FDS ||
	               handle->magic != GRALLOC_DRM_HANDLE_MAGIC)) {
		ALOGE("invalid handle: version=%d, numInts=%d, numFds=%d, magic=%x",
			handle->base.version, handle->base.numInts,
			handle->base.numFds, handle->magic);
		handle = NULL;
	}

	return handle;
}

#ifdef __cplusplus
}
#endif
#endif /* _GRALLOC_DRM_HANDLE_H_ */
