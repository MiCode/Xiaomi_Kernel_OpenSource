/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2012, 2015-2019, The Linux Foundation. All rights reserved.
 */

#ifndef SDE_ROTATOR_FORMATS_H
#define SDE_ROTATOR_FORMATS_H

#include <linux/types.h>
#include <media/msm_sde_rotator.h>

/* Internal rotator pixel formats */
#define SDE_PIX_FMT_RGBA_8888_TILE	v4l2_fourcc('Q', 'T', '0', '0')
#define SDE_PIX_FMT_RGBX_8888_TILE	v4l2_fourcc('Q', 'T', '0', '1')
#define SDE_PIX_FMT_BGRA_8888_TILE	v4l2_fourcc('Q', 'T', '0', '2')
#define SDE_PIX_FMT_BGRX_8888_TILE	v4l2_fourcc('Q', 'T', '0', '3')
#define SDE_PIX_FMT_ARGB_8888_TILE	v4l2_fourcc('Q', 'T', '0', '4')
#define SDE_PIX_FMT_XRGB_8888_TILE	v4l2_fourcc('Q', 'T', '0', '5')
#define SDE_PIX_FMT_ABGR_8888_TILE	v4l2_fourcc('Q', 'T', '0', '6')
#define SDE_PIX_FMT_XBGR_8888_TILE	v4l2_fourcc('Q', 'T', '0', '7')
#define SDE_PIX_FMT_Y_CBCR_H2V2_TILE	v4l2_fourcc('Q', 'T', '0', '8')
#define SDE_PIX_FMT_Y_CRCB_H2V2_TILE	v4l2_fourcc('Q', 'T', '0', '9')
#define SDE_PIX_FMT_ARGB_2101010_TILE	v4l2_fourcc('Q', 'T', '0', 'A')
#define SDE_PIX_FMT_XRGB_2101010_TILE	v4l2_fourcc('Q', 'T', '0', 'B')
#define SDE_PIX_FMT_ABGR_2101010_TILE	v4l2_fourcc('Q', 'T', '0', 'C')
#define SDE_PIX_FMT_XBGR_2101010_TILE	v4l2_fourcc('Q', 'T', '0', 'D')
#define SDE_PIX_FMT_BGRA_1010102_TILE	v4l2_fourcc('Q', 'T', '0', 'E')
#define SDE_PIX_FMT_BGRX_1010102_TILE	v4l2_fourcc('Q', 'T', '0', 'F')
#define SDE_PIX_FMT_RGBA_1010102_TILE	v4l2_fourcc('Q', 'T', '1', '0')
#define SDE_PIX_FMT_RGBX_1010102_TILE	v4l2_fourcc('Q', 'T', '1', '1')
#define SDE_PIX_FMT_Y_CBCR_H2V2_P010_TILE	v4l2_fourcc('Q', 'T', '1', '2')
#define SDE_PIX_FMT_RGB_565_TILE	v4l2_fourcc('Q', 'T', '1', '3')

#define SDE_ROT_MAX_PLANES		4

#define UBWC_META_MACRO_W_H		16
#define UBWC_META_BLOCK_SIZE		256

/*
 * Value of enum chosen to fit the number of bits
 * expected by the HW programming.
 */
enum {
	SDE_COLOR_4BIT,
	SDE_COLOR_5BIT,
	SDE_COLOR_6BIT,
	SDE_COLOR_8BIT,
	SDE_COLOR_ALPHA_1BIT = 0,
	SDE_COLOR_ALPHA_4BIT = 1,
};

#define C3_ALPHA	3	/* alpha */
#define C2_R_Cr		2	/* R/Cr */
#define C1_B_Cb		1	/* B/Cb */
#define C0_G_Y		0	/* G/luma */

enum sde_mdp_compress_type {
	SDE_MDP_COMPRESS_NONE,
	SDE_MDP_COMPRESS_UBWC,
};

enum sde_mdp_frame_format_type {
	SDE_MDP_FMT_LINEAR,
	SDE_MDP_FMT_TILE_A4X,
	SDE_MDP_FMT_TILE_A5X,
};

enum sde_mdp_pixel_type {
	SDE_MDP_PIXEL_NORMAL,
	SDE_MDP_PIXEL_10BIT,
};

enum sde_mdp_sspp_fetch_type {
	SDE_MDP_PLANE_INTERLEAVED,
	SDE_MDP_PLANE_PLANAR,
	SDE_MDP_PLANE_PSEUDO_PLANAR,
};

enum sde_mdp_sspp_chroma_samp_type {
	SDE_MDP_CHROMA_RGB,
	SDE_MDP_CHROMA_H2V1,
	SDE_MDP_CHROMA_H1V2,
	SDE_MDP_CHROMA_420
};

enum sde_mdp_format_flag_type {
	SDE_MDP_FORMAT_FLAG_PRIVATE = BIT(0)
};

struct sde_mdp_format_params {
	u32 format;
	const char *description;
	u32 flag;
	u8 is_yuv;
	u8 is_ubwc;

	u8 frame_format;
	u8 chroma_sample;
	u8 solid_fill;
	u8 fetch_planes;
	u8 unpack_align_msb;	/* 0 to LSB, 1 to MSB */
	u8 unpack_tight;	/* 0 for loose, 1 for tight */
	u8 unpack_count;	/* 0 = 1 component, 1 = 2 component ... */
	u8 bpp;
	u8 alpha_enable;	/*  source has alpha */
	u8 pixel_mode;		/* 0: normal, 1:10bit */
	u8 bits[SDE_ROT_MAX_PLANES];
	u8 element[SDE_ROT_MAX_PLANES];
};

struct sde_mdp_format_ubwc_tile_info {
	u16 tile_height;
	u16 tile_width;
};

struct sde_mdp_format_params_ubwc {
	struct sde_mdp_format_params mdp_format;
	struct sde_mdp_format_ubwc_tile_info micro;
};

struct sde_mdp_format_params *sde_get_format_params(u32 format);

int sde_rot_get_ubwc_micro_dim(u32 format, u16 *w, u16 *h);

int sde_rot_get_base_tilea5x_pixfmt(u32 src_pixfmt, u32 *dst_pixfmt);

static inline bool sde_mdp_is_tilea4x_format(struct sde_mdp_format_params *fmt)
{
	return fmt && (fmt->frame_format == SDE_MDP_FMT_TILE_A4X);
}

static inline bool sde_mdp_is_tilea5x_format(struct sde_mdp_format_params *fmt)
{
	return fmt && (fmt->frame_format == SDE_MDP_FMT_TILE_A5X);
}

static inline bool sde_mdp_is_ubwc_format(struct sde_mdp_format_params *fmt)
{
	return fmt && (fmt->is_ubwc == SDE_MDP_COMPRESS_UBWC);
}

static inline bool sde_mdp_is_linear_format(struct sde_mdp_format_params *fmt)
{
	return fmt && (fmt->frame_format == SDE_MDP_FMT_LINEAR);
}

static inline bool sde_mdp_is_nv12_format(struct sde_mdp_format_params *fmt)
{
	return fmt && (fmt->fetch_planes == SDE_MDP_PLANE_PSEUDO_PLANAR) &&
			(fmt->chroma_sample == SDE_MDP_CHROMA_420);
}

static inline bool sde_mdp_is_nv12_8b_format(struct sde_mdp_format_params *fmt)
{
	return fmt && sde_mdp_is_nv12_format(fmt) &&
			(fmt->pixel_mode == SDE_MDP_PIXEL_NORMAL);
}

static inline bool sde_mdp_is_nv12_10b_format(struct sde_mdp_format_params *fmt)
{
	return fmt && sde_mdp_is_nv12_format(fmt) &&
			(fmt->pixel_mode == SDE_MDP_PIXEL_10BIT);
}

static inline bool sde_mdp_is_tp10_format(struct sde_mdp_format_params *fmt)
{
	return fmt && sde_mdp_is_nv12_10b_format(fmt) &&
			fmt->unpack_tight;
}

static inline bool sde_mdp_is_p010_format(struct sde_mdp_format_params *fmt)
{
	return fmt && sde_mdp_is_nv12_10b_format(fmt) &&
			!fmt->unpack_tight;
}

static inline bool sde_mdp_is_yuv_format(struct sde_mdp_format_params *fmt)
{
	return fmt && fmt->is_yuv;
}

static inline bool sde_mdp_is_rgb_format(struct sde_mdp_format_params *fmt)
{
	return !sde_mdp_is_yuv_format(fmt);
}

static inline bool sde_mdp_is_private_format(struct sde_mdp_format_params *fmt)
{
	return fmt && (fmt->flag & SDE_MDP_FORMAT_FLAG_PRIVATE);
}

static inline int sde_mdp_format_blk_size(struct sde_mdp_format_params *fmt)
{
	return sde_mdp_is_tp10_format(fmt) ? 96 : 128;
}
#endif
