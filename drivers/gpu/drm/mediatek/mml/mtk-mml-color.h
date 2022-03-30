/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Dennis YC Hsieh <dennis-yc.hsieh@mediatek.com>
 */

#ifndef __MTK_MML_COLOR_H__
#define __MTK_MML_COLOR_H__

#define MML_FMT(COMPRESS, PACKED, LOOSE, VIDEO, PLANE, HFACTOR, VFACTOR, \
		BITS, GROUP, SWAP_ENABLE, UNIQUEID) \
	(((COMPRESS)	<< 29) | \
	 ((PACKED)	<< 28) | \
	 ((LOOSE)	<< 27) | \
	 ((VIDEO)	<< 23) | \
	 ((PLANE)	<< 21) | \
	 ((HFACTOR)	<< 19) | \
	 ((VFACTOR)	<< 18) | \
	 ((BITS)	<< 8)  | \
	 ((GROUP)	<< 6)  | \
	 ((SWAP_ENABLE)	<< 5)  | \
	 ((UNIQUEID)	<< 0))

#define MML_FMT_COMPRESS(c)		((0x20000000 & (c)) >> 29)
#define MML_FMT_10BIT_PACKED(c)		((0x10000000 & (c)) >> 28)
#define MML_FMT_10BIT(c)		((0x18000000 & (c)) >> 27)
#define MML_FMT_10BIT_LOOSE(c)		(MML_FMT_10BIT(c) == 1)
#define MML_FMT_10BIT_TILE(c)		(MML_FMT_10BIT(c) == 3)
#define MML_FMT_10BIT_JUMP(c)		((0x04000000 & (c)) >> 26)
#define MML_FMT_UFO(c)			((0x02000000 & (c)) >> 25)
#define MML_FMT_INTERLACED(c)		((0x01000000 & (c)) >> 24)
#define MML_FMT_BLOCK(c)		((0x00800000 & (c)) >> 23)
#define MML_FMT_PLANE(c)		((0x00600000 & (c)) >> 21)/* 1-3 */
#define MML_FMT_H_SUBSAMPLE(c)		((0x00180000 & (c)) >> 19)/* 0-2 */
#define MML_FMT_V_SUBSAMPLE(c)		((0x00040000 & (c)) >> 18)/* 0-1 */
#define MML_FMT_BITS_PER_PIXEL(c)	((0x0003ff00 & (c)) >>  8)
#define MML_FMT_GROUP(c)		((0x000000c0 & (c)) >>  6)
#define MML_FMT_SWAP(c)			((0x00000020 & (c)) >>  5)
#define MML_FMT_HW_FORMAT(c)		(0x0000001f & (c))
#define MML_FMT_IS_RGB(c)		(MML_FMT_GROUP(c) == 0)
#define MML_FMT_IS_YUV(c)		(MML_FMT_GROUP(c) == 1)
#define MML_FMT_UV_COPLANE(c)		(MML_FMT_PLANE(c) == 2 && \
					 MML_FMT_IS_YUV(c))
#define MML_FMT_YUV422(c)		(MML_FMT_H_SUBSAMPLE(c) && \
					 !MML_FMT_V_SUBSAMPLE(c))
#define MML_FMT_YUV420(c)		(MML_FMT_H_SUBSAMPLE(c) && \
					 MML_FMT_V_SUBSAMPLE(c))
#define MML_FMT_AUO(c)			(MML_FMT_10BIT_JUMP(c))
#define MML_FMT_IS_ARGB(c)		(MML_FMT_HW_FORMAT(c) == 2 || \
					 MML_FMT_HW_FORMAT(c) == 3)

#define MML_FMT_ARGB_COMPRESS(c)	(MML_FMT_COMPRESS(c) && \
					 MML_FMT_IS_ARGB(c))
#define MML_FMT_YUV_COMPRESS(c)		(MML_FMT_COMPRESS(c) && \
					 MML_FMT_IS_YUV(c))

enum mml_color {
	MML_FMT_UNKNOWN		= 0,

	/* Unified formats */
	MML_FMT_GREY		= MML_FMT(0, 0, 0, 0, 1, 0, 0, 8, 1, 0, 7),

	MML_FMT_RGB565		= MML_FMT(0, 0, 0, 0, 1, 0, 0, 16, 0, 0, 0),
	MML_FMT_BGR565		= MML_FMT(0, 0, 0, 0, 1, 0, 0, 16, 0, 1, 0),
	MML_FMT_RGB888		= MML_FMT(0, 0, 0, 0, 1, 0, 0, 24, 0, 1, 1),
	MML_FMT_BGR888		= MML_FMT(0, 0, 0, 0, 1, 0, 0, 24, 0, 0, 1),
	MML_FMT_RGBA8888	= MML_FMT(0, 0, 0, 0, 1, 0, 0, 32, 0, 1, 2),
	MML_FMT_BGRA8888	= MML_FMT(0, 0, 0, 0, 1, 0, 0, 32, 0, 0, 2),
	MML_FMT_ARGB8888	= MML_FMT(0, 0, 0, 0, 1, 0, 0, 32, 0, 1, 3),
	MML_FMT_ABGR8888	= MML_FMT(0, 0, 0, 0, 1, 0, 0, 32, 0, 0, 3),

	MML_FMT_UYVY		= MML_FMT(0, 0, 0, 0, 1, 1, 0, 16, 1, 0, 4),
	MML_FMT_VYUY		= MML_FMT(0, 0, 0, 0, 1, 1, 0, 16, 1, 1, 4),
	MML_FMT_YUYV		= MML_FMT(0, 0, 0, 0, 1, 1, 0, 16, 1, 0, 5),
	MML_FMT_YVYU		= MML_FMT(0, 0, 0, 0, 1, 1, 0, 16, 1, 1, 5),

	MML_FMT_I420		= MML_FMT(0, 0, 0, 0, 3, 1, 1, 8, 1, 0, 8),
	MML_FMT_YV12		= MML_FMT(0, 0, 0, 0, 3, 1, 1, 8, 1, 1, 8),
	MML_FMT_I422		= MML_FMT(0, 0, 0, 0, 3, 1, 0, 8, 1, 0, 9),
	MML_FMT_YV16		= MML_FMT(0, 0, 0, 0, 3, 1, 0, 8, 1, 1, 9),
	MML_FMT_I444		= MML_FMT(0, 0, 0, 0, 3, 0, 0, 8, 1, 0, 10),
	MML_FMT_YV24		= MML_FMT(0, 0, 0, 0, 3, 0, 0, 8, 1, 1, 10),

	MML_FMT_NV12		= MML_FMT(0, 0, 0, 0, 2, 1, 1, 8, 1, 0, 12),
	MML_FMT_NV21		= MML_FMT(0, 0, 0, 0, 2, 1, 1, 8, 1, 1, 12),
	MML_FMT_NV16		= MML_FMT(0, 0, 0, 0, 2, 1, 0, 8, 1, 0, 13),
	MML_FMT_NV61		= MML_FMT(0, 0, 0, 0, 2, 1, 0, 8, 1, 1, 13),
	MML_FMT_NV24		= MML_FMT(0, 0, 0, 0, 2, 0, 0, 8, 1, 0, 14),
	MML_FMT_NV42		= MML_FMT(0, 0, 0, 0, 2, 0, 0, 8, 1, 1, 14),

	/* Mediatek proprietary formats */

	/* Frame mode + Block mode + UFO 420 block packed */
	MML_FMT_BLK_UFO		= MML_FMT(0, 0, 0, 5, 2, 1, 1, 256, 1, 0, 12),
	/* Frame mode + Block mode + UFO AUO 420 block packed */
	MML_FMT_BLK_UFO_AUO	= MML_FMT(0, 0, 0, 13, 2, 1, 1, 256, 1, 0, 12),
	/* Frame mode + Block mode 420 block packed */
	MML_FMT_BLK		= MML_FMT(0, 0, 0, 1, 2, 1, 1, 256, 1, 0, 12),

	/* Packed 10-bit formats */
	MML_FMT_RGBA1010102	= MML_FMT(0, 1, 0, 0, 1, 0, 0, 32, 0, 1, 2),
	MML_FMT_BGRA1010102	= MML_FMT(0, 1, 0, 0, 1, 0, 0, 32, 0, 0, 2),
	MML_FMT_ARGB1010102	= MML_FMT(0, 1, 0, 0, 1, 0, 0, 32, 0, 1, 3),
	MML_FMT_ABGR1010102	= MML_FMT(0, 1, 0, 0, 1, 0, 0, 32, 0, 0, 3),
	/* Packed 10-bit NV21 */
	MML_FMT_NV12_10P	= MML_FMT(0, 1, 0, 0, 2, 1, 1, 10, 1, 0, 12),
	MML_FMT_NV21_10P	= MML_FMT(0, 1, 0, 0, 2, 1, 1, 10, 1, 1, 12),
	/* Frame mode + Block mode 420 block packed */
	MML_FMT_BLK_10H		= MML_FMT(0, 1, 0, 1, 2, 1, 1, 320, 1, 0, 12),
	/* Frame mode + HEVC tile mode 420 block packed */
	MML_FMT_BLK_10V		= MML_FMT(0, 1, 1, 1, 2, 1, 1, 320, 1, 0, 12),
	/* Frame mode + Block mode + Jump 420 block packed 10 H Jump */
	MML_FMT_BLK_10HJ	= MML_FMT(0, 1, 0, 9, 2, 1, 1, 320, 1, 0, 12),
	/* Frame mode + HEVC tile mode + Jump 420 block packed 10 V Jump */
	MML_FMT_BLK_10VJ	= MML_FMT(0, 1, 1, 9, 2, 1, 1, 320, 1, 0, 12),
	/* Frame mode + Block mode 420 block packed */
	MML_FMT_BLK_UFO_10H	= MML_FMT(0, 1, 0, 5, 2, 1, 1, 320, 1, 0, 12),
	/* Frame mode + HEVC tile mode 420 block packed */
	MML_FMT_BLK_UFO_10V	= MML_FMT(0, 1, 1, 5, 2, 1, 1, 320, 1, 0, 12),
	/* Frame mode + Block mode + Jump 420 block packed UFO 10 H Jump */
	MML_FMT_BLK_UFO_10HJ	= MML_FMT(0, 1, 0, 13, 2, 1, 1, 320, 1, 0, 12),
	/* Frame mode + HEVC tile mode + Jump 420 block packed UFO 10 V Jump */
	MML_FMT_BLK_UFO_10VJ	= MML_FMT(0, 1, 1, 13, 2, 1, 1, 320, 1, 0, 12),

	/* Loose 10-bit formats */
	MML_FMT_NV12_10L	= MML_FMT(0, 0, 1, 0, 2, 1, 1, 16, 1, 0, 12),
	MML_FMT_NV21_10L	= MML_FMT(0, 0, 1, 0, 2, 1, 1, 16, 1, 1, 12),
	MML_FMT_YUV4441010102	= MML_FMT(0, 0, 1, 0, 1, 0, 0, 32, 1, 0, 14),

	MML_FMT_YV12_10P	= MML_FMT(0, 1, 0, 0, 3, 1, 1, 10, 1, 1, 8),
	MML_FMT_I422_10P	= MML_FMT(0, 1, 0, 0, 3, 1, 0, 10, 1, 0, 9),
	MML_FMT_NV16_10P	= MML_FMT(0, 1, 0, 0, 2, 1, 0, 10, 1, 0, 13),
	MML_FMT_NV61_10P	= MML_FMT(0, 1, 0, 0, 2, 1, 0, 10, 1, 1, 13),

	MML_FMT_RGBA8888_AFBC	= MML_FMT(1, 0, 0, 0, 1, 0, 0, 32, 0, 1, 2),
	MML_FMT_BGRA8888_AFBC	= MML_FMT(1, 0, 0, 0, 1, 0, 0, 32, 0, 0, 2),
	MML_FMT_RGBA1010102_AFBC = MML_FMT(1, 1, 0, 0, 1, 0, 0, 32, 0, 1, 2),
	MML_FMT_BGRA1010102_AFBC = MML_FMT(1, 1, 0, 0, 1, 0, 0, 32, 0, 0, 2),

	MML_FMT_YUV420_AFBC	= MML_FMT(1, 0, 0, 0, 1, 1, 1, 12, 1, 0, 12),
	MML_FMT_YVU420_AFBC	= MML_FMT(1, 0, 0, 0, 1, 1, 1, 12, 1, 1, 12),
	MML_FMT_YUV420_10P_AFBC	= MML_FMT(1, 1, 0, 0, 1, 1, 1, 16, 1, 0, 12),
	MML_FMT_YVU420_10P_AFBC	= MML_FMT(1, 1, 0, 0, 1, 1, 1, 16, 1, 1, 12),
};

/* Combine colorspace, xfer_func, ycbcr_encoding, and quantization */
enum mml_ycbcr_profile {
	/* V4L2_YCBCR_ENC_601 and V4L2_QUANTIZATION_LIM_RANGE */
	MML_YCBCR_PROFILE_BT601,
	/* V4L2_YCBCR_ENC_709 and V4L2_QUANTIZATION_LIM_RANGE */
	MML_YCBCR_PROFILE_BT709,
	/* V4L2_YCBCR_ENC_601 and V4L2_QUANTIZATION_FULL_RANGE */
	MML_YCBCR_PROFILE_JPEG,
	MML_YCBCR_PROFILE_FULL_BT601 = MML_YCBCR_PROFILE_JPEG,

	/* Colorspaces not support for destination */
	/* V4L2_YCBCR_ENC_BT2020 and V4L2_QUANTIZATION_LIM_RANGE */
	MML_YCBCR_PROFILE_BT2020,
	/* V4L2_YCBCR_ENC_709 and V4L2_QUANTIZATION_FULL_RANGE */
	MML_YCBCR_PROFILE_FULL_BT709,
	/* V4L2_YCBCR_ENC_BT2020 and V4L2_QUANTIZATION_FULL_RANGE */
	MML_YCBCR_PROFILE_FULL_BT2020,
};

/* Minimum Y stride that is accepted by MML HW */
static inline u32 mml_color_get_min_y_stride(enum mml_color c, u32 width)
{
	return ((MML_FMT_BITS_PER_PIXEL(c) * width) + 4) >> 3;
}

/* Minimum UV stride that is accepted by MML HW */
static inline u32 mml_color_get_min_uv_stride(enum mml_color c, u32 width)
{
	u32 min_stride;

	if (MML_FMT_PLANE(c) == 1)
		return 0;
	min_stride = mml_color_get_min_y_stride(c, width) >>
		MML_FMT_H_SUBSAMPLE(c);
	if (MML_FMT_UV_COPLANE(c) && !MML_FMT_BLOCK(c))
		min_stride = min_stride * 2;
	return min_stride;
}

/* Minimum Y plane size that is necessary in buffer */
static inline u32 mml_color_get_min_y_size(enum mml_color c,
	u32 width, u32 height)
{
	if (MML_FMT_BLOCK(c))
		return ((MML_FMT_BITS_PER_PIXEL(c) * width) >> 8) * height;
	return mml_color_get_min_y_stride(c, width) * height;
}
/* Minimum UV plane size that is necessary in buffer */
static inline u32 mml_color_get_min_uv_size(enum mml_color c,
	u32 width, u32 height)
{
	height = height >> MML_FMT_V_SUBSAMPLE(c);
	if (MML_FMT_BLOCK(c) && (1 < MML_FMT_PLANE(c)))
		return ((MML_FMT_BITS_PER_PIXEL(c) * width) >> 8) * height;
	return mml_color_get_min_uv_stride(c, width) * height;
}

#endif	/* __MTK_MML_COLOR_H__ */

