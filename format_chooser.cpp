/*
 * Copyright (C) 2014 ARM Limited. All rights reserved.
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <hardware/hardware.h>
#include <cutils/log.h>
#include <hardware/gralloc.h>
#include "format_chooser.h"

#ifndef GRALLOC_DISP_W
#define GRALLOC_DISP_W 0
#endif
#ifndef GRALLOC_DISP_H
#define GRALLOC_DISP_H 0
#endif

/* Minimum size of buffer for AFBC to be enabled. Defined as percentage of total
 * display size */
#define GRALLOC_AFBC_MIN_SIZE 75

static inline int find_format_index(int format)
{
	int index=-1;

	switch ( format )
	{
		case HAL_PIXEL_FORMAT_RGBA_8888:
			index = GRALLOC_ARM_HAL_FORMAT_INDEXED_RGBA_8888;
			break;
		case HAL_PIXEL_FORMAT_RGBX_8888:
			index = GRALLOC_ARM_HAL_FORMAT_INDEXED_RGBX_8888;
			break;
		case HAL_PIXEL_FORMAT_RGB_888:
			index = GRALLOC_ARM_HAL_FORMAT_INDEXED_RGB_888;
			break;
		case HAL_PIXEL_FORMAT_RGB_565:
			index = GRALLOC_ARM_HAL_FORMAT_INDEXED_RGB_565;
			break;
		case HAL_PIXEL_FORMAT_BGRA_8888:
			index = GRALLOC_ARM_HAL_FORMAT_INDEXED_BGRA_8888;
			break;
#if (PLATFORM_SDK_VERSION >= 19) && (PLATFORM_SDK_VERSION <= 22)
		case HAL_PIXEL_FORMAT_sRGB_A_8888:
			index = GRALLOC_ARM_HAL_FORMAT_INDEXED_sRGB_A_8888;
			break;
		case HAL_PIXEL_FORMAT_sRGB_X_8888:
			index = GRALLOC_ARM_HAL_FORMAT_INDEXED_sRGB_X_8888;
			break;
#endif
		case HAL_PIXEL_FORMAT_YV12:
			index = GRALLOC_ARM_HAL_FORMAT_INDEXED_YV12;
			break;
#if PLATFORM_SDK_VERSION >= 18
		case HAL_PIXEL_FORMAT_Y8:
			index = GRALLOC_ARM_HAL_FORMAT_INDEXED_Y8;
			break;
		case HAL_PIXEL_FORMAT_Y16:
			index = GRALLOC_ARM_HAL_FORMAT_INDEXED_Y16;
			break;
#endif
		case HAL_PIXEL_FORMAT_YCbCr_420_888:
			index = GRALLOC_ARM_HAL_FORMAT_INDEXED_YCbCr_420_888;
			break;
	}

	return index;
}

static bool is_afbc_allowed(int buffer_size)
{
	bool afbc_allowed = false;

#if !defined(GRALLOC_ARM_NO_EXTERNAL_AFBC)
	if ((GRALLOC_DISP_W*GRALLOC_DISP_H) != 0)
	{
		afbc_allowed = ((buffer_size*100) / (GRALLOC_DISP_W*GRALLOC_DISP_H)) >= GRALLOC_AFBC_MIN_SIZE;
	}
	/* If display size is not valid then always allow AFBC */
	else
	{
		afbc_allowed = true;
	}
#endif /* defined(GRALLOC_ARM_NO_EXTERNAL_AFBC) */

	return afbc_allowed;
}

/*
 * Define GRALLOC_ARM_FORMAT_SELECTION_DISABLE to disable the format selection completely
 * or GRALLOC_ARM_NO_EXTERNAL_AFBC to disable selection of AFBC formats for external buffers.
 */
uint64_t gralloc_select_format(int req_format, int usage, int buffer_size)
{
#if defined(GRALLOC_ARM_FORMAT_SELECTION_DISABLE)
#warning "arm_format_selection is disabled!"

	(void) usage;
	(void) buffer_size;

    if(req_format == HAL_PIXEL_FORMAT_YCrCb_NV12_10 && USAGE_CONTAIN_VALUE(GRALLOC_USAGE_TO_USE_ARM_P010,GRALLOC_USAGE_ROT_MASK))
    {
        ALOGV("rk_debug force GRALLOC_ARM_HAL_FORMAT_INDEXED_P010 usage=0x%x",usage);
        return (GRALLOC_ARM_HAL_FORMAT_INDEXED_P010 | GRALLOC_ARM_INTFMT_EXTENDED_YUV);
    }
    else
        return (uint64_t) req_format;
#else
	uint64_t new_format = req_format;
	int intformat_ind;
	int n=0;
	int largest_weight_ind=-1;
	int16_t accum_weights[GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_LAST] = {0};
	int afbc_split_mode = 0;
	bool afbc_allowed = true;

	ALOGV("gralloc_select_format: req_format=0x%x usage=0x%x\n",req_format,usage);

       if ((usage & GRALLOC_ARM_USAGE_NO_AFBC) == GRALLOC_ARM_USAGE_NO_AFBC)
       {
               afbc_allowed = false;
       }

	/* The GRALLOC_USAGE_PRIVATE_3 set in the usage field indicates the req_format is
	 * to be treated as encoded private format instead of trying to find closest match.
	 * At the time being, the flag is used for testing AFBC and 10bit YUV that are not
	 * yet supported by Android HAL */
	/* Decode the passed in private format and get the gralloc indexed formats */
	else if (usage & GRALLOC_USAGE_PRIVATE_3)
	{
		uint64_t result = 0;
	   	/* req_format is within the range for normal AFBC formats */
		if (GRALLOC_ANDROID_PRIVATE_IN_RANGE_OF_AFBC(req_format))
		{
			req_format = req_format - GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC;
			result = req_format | GRALLOC_ARM_INTFMT_AFBC;
			switch (req_format & GRALLOC_ARM_INTFMT_FMT_MASK)
			{
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV420_8BIT_AFBC:
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV422_8BIT_AFBC:
					result |= GRALLOC_ARM_INTFMT_ARM_AFBC_YUV;
					break;
			}
			return result;
		}
		else if (GRALLOC_ANDROID_PRIVATE_IN_RANGE_OF_AFBC_SPLITBLK(req_format))
		{
			req_format = req_format - GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC_SPLITBLK;
			result = req_format | GRALLOC_ARM_INTFMT_AFBC_SPLITBLK;
			/* We don't support yuv 4:2:2 afbc split mode */
			if (req_format == GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV420_8BIT_AFBC)
			{
				result |= GRALLOC_ARM_INTFMT_ARM_AFBC_YUV;
			}
			return result;
		}
		else if (GRALLOC_ANDROID_PRIVATE_IN_RANGE_OF_BASE_YUVEXT(req_format))
		{
			req_format = req_format - GRALLOC_ANDROID_PRIVATE_RANGE_BASE_YUVEXT;
			switch (req_format & GRALLOC_ARM_INTFMT_FMT_MASK)
			{
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV420_10BIT_AFBC:
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV422_10BIT_AFBC:
					result = (GRALLOC_ARM_INTFMT_AFBC | GRALLOC_ARM_INTFMT_ARM_AFBC_YUV);
					/* pass through */
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_Y0L2:
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_P010:
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_P210:
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_Y210:
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_Y410:
					/* preserve the format + possible AFBC flag, and add extended-yuv flag */
					result |= GRALLOC_ARM_INTFMT_EXTENDED_YUV;
					result |= (req_format & (GRALLOC_ARM_INTFMT_FMT_MASK | GRALLOC_ARM_INTFMT_AFBC | GRALLOC_ARM_INTFMT_ARM_AFBC_YUV));
					break;
			}
			return result;
		}
		else if (GRALLOC_ANDROID_PRIVATE_IN_RANGE_OF_AFBC_WIDEBLK(req_format))
		{
			req_format = req_format - GRALLOC_ANDROID_PRIVATE_RANGE_BASE_AFBC_WIDEBLK;
			switch (req_format & GRALLOC_ARM_INTFMT_FMT_MASK)
			{
				case HAL_PIXEL_FORMAT_RGBA_8888:
				case HAL_PIXEL_FORMAT_RGBX_8888:
				case HAL_PIXEL_FORMAT_BGRA_8888:
				case HAL_PIXEL_FORMAT_RGB_888:
					result = req_format | GRALLOC_ARM_INTFMT_AFBC_SPLITBLK | GRALLOC_ARM_INTFMT_AFBC_WIDEBLK;
					break;
				case HAL_PIXEL_FORMAT_RGB_565:
				case HAL_PIXEL_FORMAT_YV12:
					result = req_format | GRALLOC_ARM_INTFMT_AFBC_WIDEBLK;
					break;
#if 1 == MALI_USE_YUV_AFBC_WIDEBLK
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV420_8BIT_AFBC:
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV422_8BIT_AFBC:
					result = req_format | GRALLOC_ARM_INTFMT_AFBC_WIDEBLK | GRALLOC_ARM_INTFMT_ARM_AFBC_YUV;
					break;
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV420_10BIT_AFBC:
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV422_10BIT_AFBC:
					result = GRALLOC_ARM_INTFMT_EXTENDED_YUV | GRALLOC_ARM_INTFMT_ARM_AFBC_YUV;
					result |= (req_format & ( GRALLOC_ARM_INTFMT_FMT_MASK | GRALLOC_ARM_INTFMT_AFBC_WIDEBLK ));
					break;
#else
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV420_8BIT_AFBC:
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV422_8BIT_AFBC:
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV420_10BIT_AFBC:
				case GRALLOC_ARM_HAL_FORMAT_INDEXED_YUV422_10BIT_AFBC:
					/* invalid format value */
					return -EINVAL;
#endif
				default:
					ALOGV("Gralloc gets internal HAL pixel format: 0x%llX", (req_format & GRALLOC_ARM_INTFMT_FMT_MASK));
					result = req_format | GRALLOC_ARM_INTFMT_AFBC_WIDEBLK;
					break;
			}
			return result;
		}
		else
		{
			/* invalid format value */
			return -EINVAL;
		}
	}

	if( req_format == 0 )
	{
		return 0;
	}

       /* Implementation defined format set to YCbCr_420_888. */
       if(req_format == HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED)
       {
               new_format = HAL_PIXEL_FORMAT_YCbCr_420_888;
       }

	if( (usage & (GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK)) != 0 ||
             usage == 0 )
	{
		return new_format;
	}

    /* AFBC buffers are not allowed as an input to the video encoder */
    if( (usage & GRALLOC_USAGE_HW_VIDEO_ENCODER) != 0 )
	{
		return new_format;
	}

#if DISABLE_FRAMEBUFFER_HAL != 1
   /* This is currently a limitation with the display and will be removed eventually
    *  We can't allocate fbdev framebuffer buffers in AFBC format */
    if( usage & GRALLOC_USAGE_HW_FB )
	{
		return new_format;
	}
#endif
	/* if this format can't be classified in one of the groups we
	 * have pre-defined, ignore it.
	 */
	intformat_ind = find_format_index( req_format );
	if( intformat_ind < 0 )
	{
		return new_format;
	}

    if (afbc_allowed)
    {
        afbc_allowed = is_afbc_allowed(buffer_size);
    }
	while( blklist[n].blk_init != 0 )
	{
		if( (blklist[n].hwblkconf.usage & usage) != 0 )
		{
			uint32_t m;

			for(m=GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_FIRST; m<GRALLOC_ARM_FORMAT_INTERNAL_INDEXED_LAST; m++)
			{
				uint64_t internal_format = translate_internal_indexed[m].internal_extended_format;
				bool is_afbc = internal_format & (GRALLOC_ARM_INTFMT_AFBC | GRALLOC_ARM_INTFMT_AFBC_SPLITBLK |
				                                  GRALLOC_ARM_INTFMT_AFBC_WIDEBLK | GRALLOC_ARM_INTFMT_ARM_AFBC_YUV);

				if ((blklist[n].hwblkconf.weights[intformat_ind][m] != DEFAULT_WEIGHT_UNSUPPORTED) && (!is_afbc || afbc_allowed))
				{
					accum_weights[m] += blklist[n].hwblkconf.weights[intformat_ind][m];

					if( largest_weight_ind < 0 ||
						accum_weights[m] > accum_weights[largest_weight_ind])
					{
						largest_weight_ind = m;
					}
				}
				else
				{
					/* Format not supported by this hwblk */
					accum_weights[m] = DEFAULT_WEIGHT_UNSUPPORTED;
				}
			}
		}
		n++;
	}

	if( largest_weight_ind < 0 )
	{
		new_format = 0;
	}
	else
	{
		new_format = translate_internal_indexed[largest_weight_ind].internal_extended_format;
	}

	ALOGV("Returned iterated format: 0x%llX", new_format);

	return new_format;
#endif
}

extern "C"
{
void *gralloc_get_internal_info(int *blkconf_size, int *gpu_conf)
{
	void *blkinit_address = NULL;

#if !defined(GRALLOC_ARM_FORMAT_SELECTION_DISABLE)

	if(blkconf_size != NULL && gpu_conf != NULL)
	{
		blkinit_address = (void*) blklist;
		*blkconf_size = blklist_array_size;

/*
 * Tests intended to verify gralloc format selection behavior are GPU version aware in runtime.
 * They need to know what configuration we built for. For now this is simply AFBC on or off. This
 * will likely change in the future to mean something else.
 */
#if MALI_AFBC_GRALLOC == 1
		*gpu_conf = 1;
#else
		*gpu_conf = 0;
#endif /* MALI_AFBC_GRALLOC */
	}
#else
    (void) blkconf_size;
    (void) gpu_conf;
#endif /* GRALLOC_ARM_FORMAT_SELECTION_DISABLE */

	return blkinit_address;
}

int gralloc_get_internal_format(int hal_format)
{
	return find_format_index(hal_format);
}
}
