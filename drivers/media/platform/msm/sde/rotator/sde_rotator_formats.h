/* Copyright (c) 2012, 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef SDE_ROTATOR_FORMATS_H
#define SDE_ROTATOR_FORMATS_H

#include <linux/types.h>
#include <media/msm_sde_rotator.h>

#define SDE_ROT_MAX_PLANES		4

#define UBWC_META_MACRO_W_H		16
#define UBWC_META_BLOCK_SIZE		256

/*
 * Value of enum chosen to fit the number of bits
 * expected by the HW programming.
 */
enum {
	COLOR_4BIT,
	COLOR_5BIT,
	COLOR_6BIT,
	COLOR_8BIT,
	COLOR_ALPHA_1BIT = 0,
	COLOR_ALPHA_4BIT = 1,
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
#endif
