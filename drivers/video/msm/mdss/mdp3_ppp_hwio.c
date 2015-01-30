/* Copyright (c) 2007, 2012-2013, 2015 The Linux Foundation.
 * All rights reserved.
 * Copyright (C) 2007 Google Incorporated
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/file.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include "linux/proc_fs.h"

#include "mdss_fb.h"
#include "mdp3_ppp.h"
#include "mdp3_hwio.h"

/* SHIM Q Factor */
#define PHI_Q_FACTOR          29
#define PQF_PLUS_5            (PHI_Q_FACTOR + 5)	/* due to 32 phases */
#define PQF_PLUS_4            (PHI_Q_FACTOR + 4)
#define PQF_PLUS_2            (PHI_Q_FACTOR + 2)	/* to get 4.0 */
#define PQF_MINUS_2           (PHI_Q_FACTOR - 2)	/* to get 0.25 */
#define PQF_PLUS_5_PLUS_2     (PQF_PLUS_5 + 2)
#define PQF_PLUS_5_MINUS_2    (PQF_PLUS_5 - 2)

enum {
	LAYER_FG = 0,
	LAYER_BG,
	LAYER_FB,
	LAYER_MAX,
};

static long long mdp_do_div(long long num, long long den)
{
	do_div(num, den);
	return num;
}

static int mdp_calc_scale_params(uint32_t org, uint32_t dim_in,
	uint32_t dim_out, bool is_W, int32_t *phase_init_ptr,
	uint32_t *phase_step_ptr)
{
	bool rpa_on = false;
	int init_phase = 0;
	uint64_t numer = 0;
	uint64_t denom = 0;
	int64_t point5 = 1;
	int64_t one = 1;
	int64_t k1, k2, k3, k4;	/* linear equation coefficients */
	uint64_t int_mask;
	uint64_t fract_mask;
	uint64_t Os;
	int64_t Osprime;
	int64_t Od;
	int64_t Odprime;
	int64_t Oreq;
	int64_t init_phase_temp;
	int64_t delta;
	uint32_t mult;

	/*
	 * The phase accumulator should really be rational for all cases in a
	 * general purpose polyphase scaler for a tiled architecture with
	 * non-zero * origin capability because there is no way to represent
	 * certain scale factors in fixed point regardless of precision.
	 * The error incurred in attempting to use fixed point is most
	 * eggregious for SF where 1/SF is an integral multiple of 1/3.
	 *
	 * Set the RPA flag for this dimension.
	 *
	 * In order for 1/SF (dim_in/dim_out) to be an integral multiple of
	 * 1/3, dim_out must be an integral multiple of 3.
	 */
	if (!(dim_out % 3)) {
		mult = dim_out / 3;
		rpa_on = (!(dim_in % mult));
	}

	numer = dim_out;
	denom = dim_in;

	/*
	 * convert to U30.34 before division
	 *
	 * The K vectors carry 4 extra bits of precision
	 * and are rounded.
	 *
	 * We initially go 5 bits over then round by adding
	 * 1 and right shifting by 1
	 * so final result is U31.33
	 */
	numer <<= PQF_PLUS_5;

	/* now calculate the scale factor (aka k3) */
	k3 = ((mdp_do_div(numer, denom) + 1) >> 1);

	/* check scale factor for legal range [0.25 - 4.0] */
	if (((k3 >> 4) < (1LL << PQF_MINUS_2)) ||
	    ((k3 >> 4) > (1LL << PQF_PLUS_2))) {
		return -EINVAL;
	}

	/* calculate inverse scale factor (aka k1) for phase init */
	numer = dim_in;
	denom = dim_out;
	numer <<= PQF_PLUS_5;
	k1 = ((mdp_do_div(numer, denom) + 1) >> 1);

	/*
	 * calculate initial phase and ROI overfetch
	 */
	/* convert point5 & one to S39.24 (will always be positive) */
	point5 <<= (PQF_PLUS_4 - 1);
	one <<= PQF_PLUS_4;
	k2 = ((k1 - one) >> 1);
	init_phase = (int)(k2 >> 4);
	k4 = ((k3 - one) >> 1);
	if (k3 != one) {
		/* calculate the masks */
		fract_mask = one - 1;
		int_mask = ~fract_mask;

		if (!rpa_on) {
			/*
			 * FIXED POINT IMPLEMENTATION
			 */
			if (org) {
				/*
				 * The complicated case; ROI origin != 0
				 * init_phase needs to be adjusted
				 * OF is also position dependent
				 */

				/* map (org - .5) into destination space */
				Os = ((uint64_t) org << 1) - 1;
				Od = ((k3 * Os) >> 1) + k4;

				/* take the ceiling */
				Odprime = (Od & int_mask);
				if (Odprime != Od)
					Odprime += one;

				/* now map that back to source space */
				Osprime = (k1 * (Odprime >> PQF_PLUS_4)) + k2;

				/* then floor & decrement to calc the required
				   starting coordinate */
				Oreq = (Osprime & int_mask) - one;

				/* calculate initial phase */
				init_phase_temp = Osprime - Oreq;
				delta = ((int64_t) (org) << PQF_PLUS_4) - Oreq;
				init_phase_temp -= delta;

				/* limit to valid range before the left shift */
				delta = (init_phase_temp & (1LL << 63)) ?
						4 : -4;
				delta <<= PQF_PLUS_4;
				while (abs((int)(init_phase_temp >>
							PQF_PLUS_4)) > 4)
					init_phase_temp += delta;

				/*
				 * right shift to account for extra bits of
				 * precision
				 */
				init_phase = (int)(init_phase_temp >> 4);

			}
		} else {
			/*
			 * RPA IMPLEMENTATION
			 *
			 * init_phase needs to be calculated in all RPA_on cases
			 * because it's a numerator, not a fixed point value.
			 */

			/* map (org - .5) into destination space */
			Os = ((uint64_t) org << PQF_PLUS_4) - point5;
			Od = mdp_do_div((dim_out * (Os + point5)),
					dim_in);
			Od -= point5;

			/* take the ceiling */
			Odprime = (Od & int_mask);
			if (Odprime != Od)
				Odprime += one;

			/* now map that back to source space */
			Osprime =
			    mdp_do_div((dim_in * (Odprime + point5)),
				       dim_out);
			Osprime -= point5;

			/* then floor & decrement to calculate the required
			   starting coordinate */
			Oreq = (Osprime & int_mask) - one;

			/* calculate initial phase */
			init_phase_temp = Osprime - Oreq;
			delta = ((int64_t) (org) << PQF_PLUS_4) - Oreq;
			init_phase_temp -= delta;

			/* limit to valid range before the left shift */
			delta = (init_phase_temp & (1LL << 63)) ? 4 : -4;
			delta <<= PQF_PLUS_4;
			while (abs((int)(init_phase_temp >> PQF_PLUS_4)) > 4)
				init_phase_temp += delta;

			/* right shift to account for extra bits of precision */
			init_phase = (int)(init_phase_temp >> 4);
		}
	}

	/* return the scale parameters */
	*phase_init_ptr = init_phase;
	*phase_step_ptr = (uint32_t) (k1 >> 4);

	return 0;
}

static int scale_idx(int factor)
{
	int idx;

	if (factor > 80)
		idx = PPP_DOWNSCALE_PT8TOPT1;
	else if (factor > 60)
		idx = PPP_DOWNSCALE_PT6TOPT8;
	else if (factor > 40)
		idx = PPP_DOWNSCALE_PT4TOPT6;
	else
		idx = PPP_DOWNSCALE_PT2TOPT4;

	return idx;
}

inline int32_t comp_conv_rgb2yuv(int32_t comp, int32_t y_high,
		int32_t y_low, int32_t c_high, int32_t c_low)
{
	if (comp < 0)
		comp = 0;
	if (comp > 255)
		comp = 255;

	/* clamp */
	if (comp < y_low)
		comp = y_low;
	if (comp > y_high)
		comp = y_high;
	return comp;
}

static uint32_t conv_rgb2yuv(uint32_t input_pixel,
		uint16_t *matrix_vector,
		uint16_t *bv,
		uint16_t *clamp_vector)
{
	uint8_t input_C2, input_C0, input_C1;
	uint32_t output;
	int32_t comp_C2, comp_C1, comp_C0, temp;
	int32_t temp1, temp2, temp3;
	int32_t matrix[9];
	int32_t bias_vector[3];
	int32_t Y_low_limit, Y_high_limit, C_low_limit, C_high_limit;
	int32_t i;

	input_C2 = (input_pixel >> 16) & 0xFF;
	input_C1 = (input_pixel >> 8) & 0xFF;
	input_C0 = (input_pixel >> 0) & 0xFF;

	comp_C0 = input_C0;
	comp_C1 = input_C1;
	comp_C2 = input_C2;

	for (i = 0; i < MDP_CSC_SIZE; i++)
		matrix[i] =
		    ((int32_t) (((int32_t) matrix_vector[i]) << 20)) >> 20;

	bias_vector[0] = (int32_t) (bv[0] & 0xFF);
	bias_vector[1] = (int32_t) (bv[1] & 0xFF);
	bias_vector[2] = (int32_t) (bv[2] & 0xFF);

	Y_low_limit = (int32_t) clamp_vector[0];
	Y_high_limit = (int32_t) clamp_vector[1];
	C_low_limit = (int32_t) clamp_vector[2];
	C_high_limit = (int32_t) clamp_vector[3];

	/*
	 * Color Conversion
	 * reorder input colors
	 */
	temp = comp_C2;
	comp_C2 = comp_C1;
	comp_C1 = comp_C0;
	comp_C0 = temp;

	/* matrix multiplication */
	temp1 = comp_C0 * matrix[0] + comp_C1 * matrix[1] + comp_C2 * matrix[2];
	temp2 = comp_C0 * matrix[3] + comp_C1 * matrix[4] + comp_C2 * matrix[5];
	temp3 = comp_C0 * matrix[6] + comp_C1 * matrix[7] + comp_C2 * matrix[8];

	comp_C0 = temp1 + 0x100;
	comp_C1 = temp2 + 0x100;
	comp_C2 = temp3 + 0x100;

	/* take interger part */
	comp_C0 >>= 9;
	comp_C1 >>= 9;
	comp_C2 >>= 9;

	/* post bias (+) */
	comp_C0 += bias_vector[0];
	comp_C1 += bias_vector[1];
	comp_C2 += bias_vector[2];

	/* limit pixel to 8-bit */
	comp_C0 = comp_conv_rgb2yuv(comp_C0, Y_high_limit,
			Y_low_limit, C_high_limit, C_low_limit);
	comp_C1 = comp_conv_rgb2yuv(comp_C1, Y_high_limit,
			Y_low_limit, C_high_limit, C_low_limit);
	comp_C2 = comp_conv_rgb2yuv(comp_C2, Y_high_limit,
			Y_low_limit, C_high_limit, C_low_limit);

	output = (comp_C2 << 16) | (comp_C1 << 8) | comp_C0;
	return output;
}

inline void y_h_even_num(struct ppp_img_desc *img)
{
	img->roi.y = (img->roi.y / 2) * 2;
	img->roi.height = (img->roi.height / 2) * 2;
}

inline void x_w_even_num(struct ppp_img_desc *img)
{
	img->roi.x = (img->roi.x / 2) * 2;
	img->roi.width = (img->roi.width / 2) * 2;
}

bool check_if_rgb(int color)
{
	bool rgb = false;
	switch (color) {
	case MDP_RGB_565:
	case MDP_BGR_565:
	case MDP_RGB_888:
	case MDP_BGR_888:
	case MDP_BGRA_8888:
	case MDP_RGBA_8888:
	case MDP_ARGB_8888:
	case MDP_XRGB_8888:
	case MDP_RGBX_8888:
	case MDP_BGRX_8888:
		rgb = true;
	default:
		break;
	}
	return rgb;
}

uint8_t *mdp_adjust_rot_addr(struct ppp_blit_op *iBuf,
	uint8_t *addr, uint32_t bpp, uint32_t uv, uint32_t layer)
{
	uint32_t ystride = 0;
	uint32_t h_slice = 1;
	uint32_t roi_width = 0;
	uint32_t roi_height = 0;
	uint32_t color_fmt = 0;

	if (layer == LAYER_BG) {
		ystride = iBuf->bg.prop.width * bpp;
		roi_width =  iBuf->bg.roi.width;
		roi_height = iBuf->bg.roi.height;
		color_fmt = iBuf->bg.color_fmt;
	} else {
		ystride = iBuf->dst.prop.width * bpp;
		roi_width =  iBuf->dst.roi.width;
		roi_height = iBuf->dst.roi.height;
		color_fmt = iBuf->dst.color_fmt;
	}
	if (uv && ((color_fmt == MDP_Y_CBCR_H2V2) ||
		(color_fmt == MDP_Y_CRCB_H2V2)))
		h_slice = 2;

	if (((iBuf->mdp_op & MDPOP_ROT90) == MDPOP_ROT90) ^
		((iBuf->mdp_op & MDPOP_LR) == MDPOP_LR)) {
		addr += (roi_width - MIN(16, roi_width)) * bpp;
	}
	if ((iBuf->mdp_op & MDPOP_UD) == MDPOP_UD) {
		addr += ((roi_height - MIN(16, roi_height))/h_slice) *
			ystride;
	}

	return addr;
}

void mdp_adjust_start_addr(struct ppp_blit_op *blit_op,
	struct ppp_img_desc *img, int v_slice,
	int h_slice, uint32_t layer)
{
	uint32_t bpp = ppp_bpp(img->color_fmt);
	int x = img->roi.x;
	int y = img->roi.y;
	uint32_t width = img->prop.width;

	if (img->color_fmt == MDP_Y_CBCR_H2V2_ADRENO && layer == 0)
		img->p0 += (x + y * ALIGN(width, 32)) * bpp;
	else if (img->color_fmt == MDP_Y_CBCR_H2V2_VENUS && layer == 0)
		img->p0 += (x + y * ALIGN(width, 128)) * bpp;
	else
		img->p0 += (x + y * width) * bpp;
	if (layer != LAYER_FG)
		img->p0 = mdp_adjust_rot_addr(blit_op, img->p0, bpp, 0, layer);

	if (img->p1) {
		/*
		 * MDP_Y_CBCR_H2V2/MDP_Y_CRCB_H2V2 cosite for now
		 * we need to shift x direction same as y dir for offsite
		 */
		if ((img->color_fmt == MDP_Y_CBCR_H2V2_ADRENO ||
				img->color_fmt == MDP_Y_CBCR_H2V2_VENUS)
							&& layer == 0)
			img->p1 += ((x / h_slice) * h_slice + ((y == 0) ? 0 :
			(((y + 1) / v_slice - 1) * (ALIGN(width/2, 32) * 2))))
									* bpp;
		else
			img->p1 += ((x / h_slice) * h_slice +
			((y == 0) ? 0 : ((y + 1) / v_slice - 1) * width)) * bpp;

		if (layer != LAYER_FG)
			img->p0 = mdp_adjust_rot_addr(blit_op,
					img->p0, bpp, 0, layer);
	}
}

int load_ppp_lut(int tableType, uint32_t *lut)
{
	int i;
	uint32_t base_addr;

	base_addr = tableType ? MDP3_PPP_POST_LUT : MDP3_PPP_PRE_LUT;
	for (i = 0; i < PPP_LUT_MAX; i++)
		PPP_WRITEL(lut[i], base_addr + MDP3_PPP_LUTn(i));

	return 0;
}

/* Configure Primary CSC Matrix */
int load_primary_matrix(struct ppp_csc_table *csc)
{
	int i;

	for (i = 0; i < MDP_CSC_SIZE; i++)
		PPP_WRITEL(csc->fwd_matrix[i], MDP3_PPP_CSC_PFMVn(i));

	for (i = 0; i < MDP_CSC_SIZE; i++)
		PPP_WRITEL(csc->rev_matrix[i], MDP3_PPP_CSC_PRMVn(i));

	for (i = 0; i < MDP_BV_SIZE; i++)
		PPP_WRITEL(csc->bv[i], MDP3_PPP_CSC_PBVn(i));

	for (i = 0; i < MDP_LV_SIZE; i++)
		PPP_WRITEL(csc->lv[i], MDP3_PPP_CSC_PLVn(i));

	return 0;
}

/* Load Secondary CSC Matrix */
int load_secondary_matrix(struct ppp_csc_table *csc)
{
	int i;

	for (i = 0; i < MDP_CSC_SIZE; i++)
		PPP_WRITEL(csc->fwd_matrix[i], MDP3_PPP_CSC_SFMVn(i));

	for (i = 0; i < MDP_CSC_SIZE; i++)
		PPP_WRITEL(csc->rev_matrix[i], MDP3_PPP_CSC_SRMVn(i));

	for (i = 0; i < MDP_BV_SIZE; i++)
		PPP_WRITEL(csc->bv[i], MDP3_PPP_CSC_SBVn(i));

	for (i = 0; i < MDP_LV_SIZE; i++)
		PPP_WRITEL(csc->lv[i], MDP3_PPP_CSC_SLVn(i));
	return 0;
}

int load_csc_matrix(int matrix_type, struct ppp_csc_table *csc)
{
	if (matrix_type == CSC_PRIMARY_MATRIX)
		return load_primary_matrix(csc);

	return load_secondary_matrix(csc);
}

int config_ppp_src(struct ppp_img_desc *src, uint32_t yuv2rgb)
{
	uint32_t val;

	val = ((src->roi.height & MDP3_PPP_XY_MASK) << MDP3_PPP_XY_OFFSET) |
		   (src->roi.width & MDP3_PPP_XY_MASK);
	PPP_WRITEL(val, MDP3_PPP_SRC_SIZE);

	PPP_WRITEL(src->p0, MDP3_PPP_SRCP0_ADDR);
	PPP_WRITEL(src->p1, MDP3_PPP_SRCP1_ADDR);
	PPP_WRITEL(src->p3, MDP3_PPP_SRCP3_ADDR);

	val = (src->stride0 & MDP3_PPP_STRIDE_MASK) |
			((src->stride1 & MDP3_PPP_STRIDE_MASK) <<
			MDP3_PPP_STRIDE1_OFFSET);
	PPP_WRITEL(val, MDP3_PPP_SRC_YSTRIDE1_ADDR);
	val = ((src->stride2 & MDP3_PPP_STRIDE_MASK) <<
			MDP3_PPP_STRIDE1_OFFSET);
	PPP_WRITEL(val, MDP3_PPP_SRC_YSTRIDE2_ADDR);

	val = ppp_src_config(src->color_fmt);
	val |= (src->roi.x % 2) ? PPP_SRC_BPP_ROI_ODD_X : 0;
	val |= (src->roi.y % 2) ? PPP_SRC_BPP_ROI_ODD_Y : 0;
	PPP_WRITEL(val, MDP3_PPP_SRC_FORMAT);
	PPP_WRITEL(ppp_pack_pattern(src->color_fmt, yuv2rgb),
		MDP3_PPP_SRC_UNPACK_PATTERN1);
	return 0;
}

int config_ppp_out(struct ppp_img_desc *dst, uint32_t yuv2rgb)
{
	uint32_t val;
	bool pseudoplanr_output = false;

	switch (dst->color_fmt) {
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CRCB_H2V2:
	case MDP_Y_CBCR_H2V1:
	case MDP_Y_CRCB_H2V1:
		pseudoplanr_output = true;
		break;
	default:
		break;
	}
	val = ppp_out_config(dst->color_fmt);
	if (pseudoplanr_output)
		val |= PPP_DST_PLANE_PSEUDOPLN;
	PPP_WRITEL(val, MDP3_PPP_OUT_FORMAT);
	PPP_WRITEL(ppp_pack_pattern(dst->color_fmt, yuv2rgb),
		MDP3_PPP_OUT_PACK_PATTERN1);

	val = ((dst->roi.height & MDP3_PPP_XY_MASK) << MDP3_PPP_XY_OFFSET) |
		   (dst->roi.width & MDP3_PPP_XY_MASK);
	PPP_WRITEL(val, MDP3_PPP_OUT_SIZE);

	PPP_WRITEL(dst->p0, MDP3_PPP_OUTP0_ADDR);
	PPP_WRITEL(dst->p1, MDP3_PPP_OUTP1_ADDR);
	PPP_WRITEL(dst->p3, MDP3_PPP_OUTP3_ADDR);

	val = (dst->stride0 & MDP3_PPP_STRIDE_MASK) |
			((dst->stride1 & MDP3_PPP_STRIDE_MASK) <<
			MDP3_PPP_STRIDE1_OFFSET);
	PPP_WRITEL(val, MDP3_PPP_OUT_YSTRIDE1_ADDR);
	val = ((dst->stride2 & MDP3_PPP_STRIDE_MASK) <<
			MDP3_PPP_STRIDE1_OFFSET);
	PPP_WRITEL(val, MDP3_PPP_OUT_YSTRIDE2_ADDR);
	return 0;
}

int config_ppp_background(struct ppp_img_desc *bg)
{
	uint32_t val;

	PPP_WRITEL(bg->p0, MDP3_PPP_BGP0_ADDR);
	PPP_WRITEL(bg->p1, MDP3_PPP_BGP1_ADDR);
	PPP_WRITEL(bg->p3, MDP3_PPP_BGP3_ADDR);

	val = (bg->stride0 & MDP3_PPP_STRIDE_MASK) |
			((bg->stride1 & MDP3_PPP_STRIDE_MASK) <<
			MDP3_PPP_STRIDE1_OFFSET);
	PPP_WRITEL(val, MDP3_PPP_BG_YSTRIDE1_ADDR);
	val = ((bg->stride2 & MDP3_PPP_STRIDE_MASK) <<
			MDP3_PPP_STRIDE1_OFFSET);
	PPP_WRITEL(val, MDP3_PPP_BG_YSTRIDE2_ADDR);

	PPP_WRITEL(ppp_src_config(bg->color_fmt),
		MDP3_PPP_BG_FORMAT);
	PPP_WRITEL(ppp_pack_pattern(bg->color_fmt, 0),
		MDP3_PPP_BG_UNPACK_PATTERN1);
	return 0;
}

void ppp_edge_rep_luma_pixel(struct ppp_blit_op *blit_op,
	struct ppp_edge_rep *er)
{
	if (blit_op->mdp_op & MDPOP_ASCALE) {

		er->is_scale_enabled = 1;

		if (blit_op->mdp_op & MDPOP_ROT90) {
			er->dst_roi_width = blit_op->dst.roi.height;
			er->dst_roi_height = blit_op->dst.roi.width;
		} else {
			er->dst_roi_width = blit_op->dst.roi.width;
			er->dst_roi_height = blit_op->dst.roi.height;
		}

		/*
		 * Find out the luma pixels needed for scaling in the
		 * x direction (LEFT and RIGHT).  Locations of pixels are
		 * relative to the ROI. Upper-left corner of ROI corresponds
		 * to coordinates (0,0). Also set the number of luma pixel
		 * to repeat.
		 */
		if (blit_op->src.roi.width > 3 * er->dst_roi_width) {
			/* scale factor < 1/3 */
			er->luma_interp_point_right =
				(blit_op->src.roi.width - 1);
		} else if (blit_op->src.roi.width == 3 * er->dst_roi_width) {
			/* scale factor == 1/3 */
			er->luma_interp_point_right =
				(blit_op->src.roi.width - 1) + 1;
			er->luma_repeat_right = 1;
		} else if ((blit_op->src.roi.width > er->dst_roi_width) &&
			   (blit_op->src.roi.width < 3 * er->dst_roi_width)) {
			/* 1/3 < scale factor < 1 */
			er->luma_interp_point_left = -1;
			er->luma_interp_point_right =
				(blit_op->src.roi.width - 1) + 1;
			er->luma_repeat_left = 1;
			er->luma_repeat_right = 1;
		} else if (blit_op->src.roi.width == er->dst_roi_width) {
			/* scale factor == 1 */
			er->luma_interp_point_left = -1;
			er->luma_interp_point_right =
				(blit_op->src.roi.width - 1) + 2;
			er->luma_repeat_left = 1;
			er->luma_repeat_right = 2;
		} else {
			  /* scale factor > 1 */
			er->luma_interp_point_left = -2;
			er->luma_interp_point_right =
				(blit_op->src.roi.width - 1) + 2;
			er->luma_repeat_left = 2;
			er->luma_repeat_right = 2;
		}

		/*
		 * Find out the number of pixels needed for scaling in the
		 * y direction (TOP and BOTTOM).  Locations of pixels are
		 * relative to the ROI. Upper-left corner of ROI corresponds
		 * to coordinates (0,0). Also set the number of luma pixel
		 * to repeat.
		 */
		if (blit_op->src.roi.height > 3 * er->dst_roi_height) {
			er->luma_interp_point_bottom =
				(blit_op->src.roi.height - 1);
		} else if (blit_op->src.roi.height == 3 * er->dst_roi_height) {
			er->luma_interp_point_bottom =
				(blit_op->src.roi.height - 1) + 1;
			er->luma_repeat_bottom = 1;
		} else if ((blit_op->src.roi.height > er->dst_roi_height) &&
			   (blit_op->src.roi.height < 3 * er->dst_roi_height)) {
			er->luma_interp_point_top = -1;
			er->luma_interp_point_bottom =
				(blit_op->src.roi.height - 1) + 1;
			er->luma_repeat_top = 1;
			er->luma_repeat_bottom = 1;
		} else if (blit_op->src.roi.height == er->dst_roi_height) {
			er->luma_interp_point_top = -1;
			er->luma_interp_point_bottom =
				(blit_op->src.roi.height - 1) + 2;
			er->luma_repeat_top = 1;
			er->luma_repeat_bottom = 2;
		} else {
			er->luma_interp_point_top = -2;
			er->luma_interp_point_bottom =
				(blit_op->src.roi.height - 1) + 2;
			er->luma_repeat_top = 2;
			er->luma_repeat_bottom = 2;
		}
	} else {
		/*
		 * Since no scaling needed, Tile Fetch does not require any
		 * more luma pixel than what the ROI contains.
		 */
		er->luma_interp_point_right =
			(int32_t) (blit_op->src.roi.width - 1);
		er->luma_interp_point_bottom =
			(int32_t) (blit_op->src.roi.height - 1);
	}
	/* After adding the ROI offsets, we have locations of
	 * luma_interp_points relative to the image.
	 */
	er->luma_interp_point_left += (int32_t) (blit_op->src.roi.x);
	er->luma_interp_point_right += (int32_t) (blit_op->src.roi.x);
	er->luma_interp_point_top += (int32_t) (blit_op->src.roi.y);
	er->luma_interp_point_bottom += (int32_t) (blit_op->src.roi.y);
}

void ppp_edge_rep_chroma_pixel(struct ppp_blit_op *blit_op,
	struct ppp_edge_rep *er)
{
	bool chroma_edge_enable = true;
	uint32_t is_yuv_offsite_vertical = 0;

	/* find out which chroma pixels are needed for chroma upsampling. */
	switch (blit_op->src.color_fmt) {
	case MDP_Y_CBCR_H2V1:
	case MDP_Y_CRCB_H2V1:
	case MDP_YCRYCB_H2V1:
		er->chroma_interp_point_left = er->luma_interp_point_left >> 1;
		er->chroma_interp_point_right =
			(er->luma_interp_point_right + 1) >> 1;
		er->chroma_interp_point_top = er->luma_interp_point_top;
		er->chroma_interp_point_bottom = er->luma_interp_point_bottom;
		break;

	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CBCR_H2V2_ADRENO:
	case MDP_Y_CBCR_H2V2_VENUS:
	case MDP_Y_CRCB_H2V2:
		er->chroma_interp_point_left = er->luma_interp_point_left >> 1;
		er->chroma_interp_point_right =
			(er->luma_interp_point_right + 1) >> 1;
		er->chroma_interp_point_top =
			(er->luma_interp_point_top - 1) >> 1;
		er->chroma_interp_point_bottom =
		    (er->luma_interp_point_bottom + 1) >> 1;
		is_yuv_offsite_vertical = 1;
		break;

	default:
		chroma_edge_enable = false;
		er->chroma_interp_point_left = er->luma_interp_point_left;
		er->chroma_interp_point_right = er->luma_interp_point_right;
		er->chroma_interp_point_top = er->luma_interp_point_top;
		er->chroma_interp_point_bottom = er->luma_interp_point_bottom;

		break;
	}

	if (chroma_edge_enable) {
		/* Defines which chroma pixels belongs to the roi */
		switch (blit_op->src.color_fmt) {
		case MDP_Y_CBCR_H2V1:
		case MDP_Y_CRCB_H2V1:
		case MDP_YCRYCB_H2V1:
			er->chroma_bound_left = blit_op->src.roi.x / 2;
			/* there are half as many chroma pixel as luma pixels */
			er->chroma_bound_right =
			    (blit_op->src.roi.width +
				blit_op->src.roi.x - 1) / 2;
			er->chroma_bound_top = blit_op->src.roi.y;
			er->chroma_bound_bottom =
			    (blit_op->src.roi.height + blit_op->src.roi.y - 1);
			break;
		case MDP_Y_CBCR_H2V2:
		case MDP_Y_CBCR_H2V2_ADRENO:
		case MDP_Y_CBCR_H2V2_VENUS:
		case MDP_Y_CRCB_H2V2:
			/*
			 * cosite in horizontal dir, and offsite in vertical dir
			 * width of chroma ROI is 1/2 of size of luma ROI
			 * height of chroma ROI is 1/2 of size of luma ROI
			 */
			er->chroma_bound_left = blit_op->src.roi.x / 2;
			er->chroma_bound_right =
			    (blit_op->src.roi.width +
				blit_op->src.roi.x - 1) / 2;
			er->chroma_bound_top = blit_op->src.roi.y / 2;
			er->chroma_bound_bottom =
			    (blit_op->src.roi.height +
				blit_op->src.roi.y - 1) / 2;
			break;

		default:
			/*
			 * If no valid chroma sub-sampling format specified,
			 * assume 4:4:4 ( i.e. fully sampled).
			 */
			er->chroma_bound_left = blit_op->src.roi.x;
			er->chroma_bound_right = blit_op->src.roi.width +
				blit_op->src.roi.x - 1;
			er->chroma_bound_top = blit_op->src.roi.y;
			er->chroma_bound_bottom =
			    (blit_op->src.roi.height + blit_op->src.roi.y - 1);
			break;
		}

		/*
		 * Knowing which chroma pixels are needed, and which chroma
		 * pixels belong to the ROI (i.e. available for fetching ),
		 * calculate how many chroma pixels Tile Fetch needs to
		 * duplicate.  If any required chroma pixels falls outside
		 * of the ROI, Tile Fetch must obtain them by replicating
		 * pixels.
		 */
		if (er->chroma_bound_left > er->chroma_interp_point_left)
			er->chroma_repeat_left =
			    er->chroma_bound_left -
				er->chroma_interp_point_left;
		else
			er->chroma_repeat_left = 0;

		if (er->chroma_interp_point_right > er->chroma_bound_right)
			er->chroma_repeat_right =
			    er->chroma_interp_point_right -
				er->chroma_bound_right;
		else
			er->chroma_repeat_right = 0;

		if (er->chroma_bound_top > er->chroma_interp_point_top)
			er->chroma_repeat_top =
			    er->chroma_bound_top -
				er->chroma_interp_point_top;
		else
			er->chroma_repeat_top = 0;

		if (er->chroma_interp_point_bottom > er->chroma_bound_bottom)
			er->chroma_repeat_bottom =
			    er->chroma_interp_point_bottom -
				er->chroma_bound_bottom;
		else
			er->chroma_repeat_bottom = 0;

		if (er->is_scale_enabled && (blit_op->src.roi.height == 1)
		    && is_yuv_offsite_vertical) {
			er->chroma_repeat_bottom = 3;
			er->chroma_repeat_top = 0;
		}
	}
}

int config_ppp_edge_rep(struct ppp_blit_op *blit_op)
{
	uint32_t reg = 0;
	struct ppp_edge_rep er;

	memset(&er, 0, sizeof(er));

	ppp_edge_rep_luma_pixel(blit_op, &er);

	/*
	 * After adding the ROI offsets, we have locations of
	 * chroma_interp_points relative to the image.
	 */
	er.chroma_interp_point_left = er.luma_interp_point_left;
	er.chroma_interp_point_right = er.luma_interp_point_right;
	er.chroma_interp_point_top = er.luma_interp_point_top;
	er.chroma_interp_point_bottom = er.luma_interp_point_bottom;

	ppp_edge_rep_chroma_pixel(blit_op, &er);
	/* ensure repeats are >=0 and no larger than 3 pixels */
	if ((er.chroma_repeat_left < 0) || (er.chroma_repeat_right < 0) ||
	    (er.chroma_repeat_top < 0) || (er.chroma_repeat_bottom < 0))
		return -EINVAL;
	if ((er.chroma_repeat_left > 3) || (er.chroma_repeat_right > 3) ||
	    (er.chroma_repeat_top > 3) || (er.chroma_repeat_bottom > 3))
		return -EINVAL;
	if ((er.luma_repeat_left < 0) || (er.luma_repeat_right < 0) ||
	    (er.luma_repeat_top < 0) || (er.luma_repeat_bottom < 0))
		return -EINVAL;
	if ((er.luma_repeat_left > 3) || (er.luma_repeat_right > 3) ||
	    (er.luma_repeat_top > 3) || (er.luma_repeat_bottom > 3))
		return -EINVAL;

	reg |= (er.chroma_repeat_left & 3) << MDP_LEFT_CHROMA;
	reg |= (er.chroma_repeat_right & 3) << MDP_RIGHT_CHROMA;
	reg |= (er.chroma_repeat_top & 3) << MDP_TOP_CHROMA;
	reg |= (er.chroma_repeat_bottom & 3) << MDP_BOTTOM_CHROMA;
	reg |= (er.luma_repeat_left & 3) << MDP_LEFT_LUMA;
	reg |= (er.luma_repeat_right & 3) << MDP_RIGHT_LUMA;
	reg |= (er.luma_repeat_top & 3) << MDP_TOP_LUMA;
	reg |= (er.luma_repeat_bottom & 3) << MDP_BOTTOM_LUMA;
	PPP_WRITEL(reg, MDP3_PPP_SRC_EDGE_REP);
	return 0;
}

int config_ppp_bg_edge_rep(struct ppp_blit_op *blit_op)
{
	uint32_t reg = 0;

	switch (blit_op->dst.color_fmt) {
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CRCB_H2V2:
		if (blit_op->dst.roi.y == 0)
			reg |= BIT(MDP_TOP_CHROMA);

		if ((blit_op->dst.roi.y + blit_op->dst.roi.height) ==
		    blit_op->dst.prop.height) {
			reg |= BIT(MDP_BOTTOM_CHROMA);
		}

		if (((blit_op->dst.roi.x + blit_op->dst.roi.width) ==
				blit_op->dst.prop.width) &&
				((blit_op->dst.roi.width % 2) == 0))
			reg |= BIT(MDP_RIGHT_CHROMA);
		break;
	case MDP_Y_CBCR_H2V1:
	case MDP_Y_CRCB_H2V1:
	case MDP_YCRYCB_H2V1:
		if (((blit_op->dst.roi.x + blit_op->dst.roi.width) ==
				blit_op->dst.prop.width) &&
				((blit_op->dst.roi.width % 2) == 0))
			reg |= BIT(MDP_RIGHT_CHROMA);
		break;
	default:
		break;
	}
	PPP_WRITEL(reg, MDP3_PPP_BG_EDGE_REP);
	return 0;
}

int config_ppp_lut(uint32_t *pppop_reg_ptr, int lut_c0_en,
	int lut_c1_en, int lut_c2_en)
{
	if (lut_c0_en)
		*pppop_reg_ptr |= MDP_LUT_C0_EN;
	if (lut_c1_en)
		*pppop_reg_ptr |= MDP_LUT_C1_EN;
	if (lut_c2_en)
		*pppop_reg_ptr |= MDP_LUT_C2_EN;
	return 0;
}

int config_ppp_scale(struct ppp_blit_op *blit_op, uint32_t *pppop_reg_ptr)
{
	struct ppp_img_desc *src = &blit_op->src;
	struct ppp_img_desc *dst = &blit_op->dst;
	uint32_t dstW, dstH;
	uint32_t x_fac, y_fac;
	uint32_t mdp_blur = 0;
	uint32_t phase_init_x, phase_init_y, phase_step_x, phase_step_y;
	int x_idx, y_idx;

	if (blit_op->mdp_op & MDPOP_ASCALE) {
		if (blit_op->mdp_op & MDPOP_ROT90) {
			dstW = dst->roi.height;
			dstH = dst->roi.width;
		} else {
			dstW = dst->roi.width;
			dstH = dst->roi.height;
		}
		*pppop_reg_ptr |=
			(PPP_OP_SCALE_Y_ON | PPP_OP_SCALE_X_ON);

		mdp_blur = blit_op->mdp_op & MDPOP_BLUR;

		if ((dstW != src->roi.width) ||
		    (dstH != src->roi.height) || mdp_blur) {

				mdp_calc_scale_params(blit_op->src.roi.x,
					blit_op->src.roi.width,
					dstW, 1, &phase_init_x,
					&phase_step_x);
				mdp_calc_scale_params(blit_op->src.roi.y,
					blit_op->src.roi.height,
					dstH, 0, &phase_init_y,
					&phase_step_y);

			PPP_WRITEL(phase_init_x, MDP3_PPP_SCALE_PHASEX_INIT);
			PPP_WRITEL(phase_init_y, MDP3_PPP_SCALE_PHASEY_INIT);
			PPP_WRITEL(phase_step_x, MDP3_PPP_SCALE_PHASEX_STEP);
			PPP_WRITEL(phase_step_y, MDP3_PPP_SCALE_PHASEY_STEP);


			if (dstW > src->roi.width || dstH > src->roi.height)
				ppp_load_up_lut();

			if (mdp_blur)
				ppp_load_gaussian_lut();

			if (dstW <= src->roi.width) {
				x_fac = (dstW * 100) / src->roi.width;
				x_idx = scale_idx(x_fac);
				ppp_load_x_scale_table(x_idx);
			}
			if (dstH <= src->roi.height) {
				y_fac = (dstH * 100) / src->roi.height;
				y_idx = scale_idx(y_fac);
				ppp_load_y_scale_table(y_idx);
			}

		} else {
			blit_op->mdp_op &= ~(MDPOP_ASCALE);
		}
	}
	config_ppp_edge_rep(blit_op);
	config_ppp_bg_edge_rep(blit_op);
	return 0;
}

int config_ppp_csc(int src_color, int dst_color, uint32_t *pppop_reg_ptr)
{
	bool inputRGB, outputRGB;

	inputRGB = check_if_rgb(src_color);
	outputRGB = check_if_rgb(dst_color);

	if ((!inputRGB) && (outputRGB))
		*pppop_reg_ptr |= PPP_OP_CONVERT_YCBCR2RGB |
			PPP_OP_CONVERT_ON;
	if ((inputRGB) && (!outputRGB))
		*pppop_reg_ptr |= PPP_OP_CONVERT_ON;

	return 0;
}

int config_ppp_blend(struct ppp_blit_op *blit_op,
			uint32_t *pppop_reg_ptr)
{
	struct ppp_csc_table *csc;
	uint32_t alpha, trans_color;
	uint32_t val = 0;
	int c_fmt = blit_op->src.color_fmt;
	int bg_alpha;

	csc = ppp_csc_rgb2yuv();
	alpha = blit_op->blend.const_alpha;
	trans_color = blit_op->blend.trans_color;
	if (blit_op->mdp_op & MDPOP_FG_PM_ALPHA) {
		if (ppp_per_p_alpha(c_fmt)) {
			*pppop_reg_ptr |= PPP_OP_ROT_ON |
					  PPP_OP_BLEND_ON |
					  PPP_OP_BLEND_CONSTANT_ALPHA;
		} else {
			if ((blit_op->mdp_op & MDPOP_ALPHAB)
				&& (blit_op->blend.const_alpha == 0xff)) {
				blit_op->mdp_op &= ~(MDPOP_ALPHAB);
			}

			if ((blit_op->mdp_op & MDPOP_ALPHAB)
			   || (blit_op->mdp_op & MDPOP_TRANSP)) {

				*pppop_reg_ptr |= PPP_OP_ROT_ON |
					PPP_OP_BLEND_ON |
					PPP_OP_BLEND_CONSTANT_ALPHA |
					PPP_OP_BLEND_ALPHA_BLEND_NORMAL;
			}
		}

		bg_alpha = PPP_BLEND_BG_USE_ALPHA_SEL |
			PPP_BLEND_BG_ALPHA_REVERSE;

		if ((ppp_per_p_alpha(c_fmt)) && !(blit_op->mdp_op &
						MDPOP_LAYER_IS_FG)) {
			bg_alpha |= PPP_BLEND_BG_SRCPIXEL_ALPHA;
		} else {
			bg_alpha |= PPP_BLEND_BG_CONSTANT_ALPHA;
			bg_alpha |= blit_op->blend.const_alpha << 24;
		}
		PPP_WRITEL(bg_alpha, MDP3_PPP_BLEND_BG_ALPHA_SEL);

		if (blit_op->mdp_op & MDPOP_TRANSP)
			*pppop_reg_ptr |= PPP_BLEND_CALPHA_TRNASP;
	} else if (ppp_per_p_alpha(c_fmt)) {
		if (blit_op->mdp_op & MDPOP_LAYER_IS_FG)
			*pppop_reg_ptr |= PPP_OP_ROT_ON |
				  PPP_OP_BLEND_ON |
				  PPP_OP_BLEND_CONSTANT_ALPHA;
		else
			*pppop_reg_ptr |= PPP_OP_ROT_ON |
				  PPP_OP_BLEND_ON |
				  PPP_OP_BLEND_SRCPIXEL_ALPHA;
		PPP_WRITEL(0, MDP3_PPP_BLEND_BG_ALPHA_SEL);
	} else {
		if ((blit_op->mdp_op & MDPOP_ALPHAB)
				&& (blit_op->blend.const_alpha == 0xff)) {
			blit_op->mdp_op &=
				~(MDPOP_ALPHAB);
		}

		if ((blit_op->mdp_op & MDPOP_ALPHAB)
		   || (blit_op->mdp_op & MDPOP_TRANSP)) {
			*pppop_reg_ptr |= PPP_OP_ROT_ON |
				PPP_OP_BLEND_ON |
				PPP_OP_BLEND_CONSTANT_ALPHA |
				PPP_OP_BLEND_ALPHA_BLEND_NORMAL;
		}

		if (blit_op->mdp_op & MDPOP_TRANSP)
			*pppop_reg_ptr |=
				PPP_BLEND_CALPHA_TRNASP;
		PPP_WRITEL(0, MDP3_PPP_BLEND_BG_ALPHA_SEL);
	}

	if (*pppop_reg_ptr & PPP_OP_BLEND_ON) {
		config_ppp_background(&blit_op->bg);

		if (blit_op->dst.color_fmt == MDP_YCRYCB_H2V1) {
			*pppop_reg_ptr |= PPP_OP_BG_CHROMA_H2V1;
			if (blit_op->mdp_op & MDPOP_TRANSP) {
				trans_color = conv_rgb2yuv(trans_color,
					&csc->fwd_matrix[0],
					&csc->bv[0],
					&csc->lv[0]);
			}
		}
	}
	val = (alpha << MDP_BLEND_CONST_ALPHA);
	val |= (trans_color & MDP_BLEND_TRASP_COL_MASK);
	PPP_WRITEL(val, MDP3_PPP_BLEND_PARAM);
	return 0;
}

int config_ppp_rotation(uint32_t mdp_op, uint32_t *pppop_reg_ptr)
{
	*pppop_reg_ptr |= PPP_OP_ROT_ON;

	if (mdp_op & MDPOP_ROT90)
		*pppop_reg_ptr |= PPP_OP_ROT_90;
	if (mdp_op & MDPOP_LR)
		*pppop_reg_ptr |= PPP_OP_FLIP_LR;
	if (mdp_op & MDPOP_UD)
		*pppop_reg_ptr |= PPP_OP_FLIP_UD;

	return 0;
}

int config_ppp_op_mode(struct ppp_blit_op *blit_op)
{
	uint32_t yuv2rgb;
	uint32_t ppp_operation_reg = 0;
	int sv_slice, sh_slice;
	int dv_slice, dh_slice;
	static struct ppp_img_desc bg_img_param;

	sv_slice = sh_slice = dv_slice = dh_slice = 1;

	ppp_operation_reg |= ppp_dst_op_reg(blit_op->dst.color_fmt);
	switch (blit_op->dst.color_fmt) {
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CRCB_H2V2:
		y_h_even_num(&blit_op->dst);
		y_h_even_num(&blit_op->src);
		dv_slice = 2;
	case MDP_Y_CBCR_H2V1:
	case MDP_Y_CRCB_H2V1:
	case MDP_YCRYCB_H2V1:
		x_w_even_num(&blit_op->dst);
		x_w_even_num(&blit_op->src);
		dh_slice = 2;
		break;
	default:
		break;
	}

	ppp_operation_reg |= ppp_src_op_reg(blit_op->src.color_fmt);
	switch (blit_op->src.color_fmt) {
	case MDP_Y_CBCR_H2V2:
	case MDP_Y_CBCR_H2V2_ADRENO:
	case MDP_Y_CBCR_H2V2_VENUS:
	case MDP_Y_CRCB_H2V2:
		sh_slice = sv_slice = 2;
		break;
	case MDP_YCRYCB_H2V1:
		x_w_even_num(&blit_op->dst);
		x_w_even_num(&blit_op->src);
	case MDP_Y_CBCR_H2V1:
	case MDP_Y_CRCB_H2V1:
		sh_slice = 2;
		break;
	default:
		break;
	}

	config_ppp_csc(blit_op->src.color_fmt,
		blit_op->dst.color_fmt, &ppp_operation_reg);
	yuv2rgb = ppp_operation_reg & PPP_OP_CONVERT_YCBCR2RGB;

	if (blit_op->mdp_op & MDPOP_DITHER)
		ppp_operation_reg |= PPP_OP_DITHER_EN;

	if (blit_op->mdp_op & MDPOP_ROTATION)
		config_ppp_rotation(blit_op->mdp_op, &ppp_operation_reg);

	if (blit_op->src.color_fmt == MDP_Y_CBCR_H2V2_ADRENO) {
		blit_op->src.stride0 = ALIGN(blit_op->src.prop.width, 32) *
			ppp_bpp(blit_op->src.color_fmt);
		blit_op->src.stride1 = 2 * ALIGN(blit_op->src.prop.width/2, 32);
	} else if (blit_op->src.color_fmt == MDP_Y_CBCR_H2V2_VENUS) {
		blit_op->src.stride0 = ALIGN(blit_op->src.prop.width, 128)  *
			ppp_bpp(blit_op->src.color_fmt);
		blit_op->src.stride1 = blit_op->src.stride0;
	} else {
		blit_op->src.stride0 = blit_op->src.prop.width *
			ppp_bpp(blit_op->src.color_fmt);
		blit_op->src.stride1 = blit_op->src.stride0;
	}

	blit_op->dst.stride0 = blit_op->dst.prop.width *
		ppp_bpp(blit_op->dst.color_fmt);

	if (ppp_multi_plane(blit_op->dst.color_fmt)) {
		blit_op->dst.p1 = blit_op->dst.p0;
		blit_op->dst.p1 += blit_op->dst.prop.width *
			blit_op->dst.prop.height *
			ppp_bpp(blit_op->dst.color_fmt);
	} else {
		blit_op->dst.p1 = NULL;
	}

	if ((bg_img_param.p0) && (!(blit_op->mdp_op & MDPOP_SMART_BLIT))) {
		/* Use cached smart blit BG layer info in smart Blit FG request */
		blit_op->bg = bg_img_param;
		if (check_if_rgb(blit_op->bg.color_fmt)) {
			blit_op->bg.p1 = 0;
			blit_op->bg.stride1 = 0;
		}
		memset(&bg_img_param, 0, sizeof(bg_img_param));
	} else {
		blit_op->bg = blit_op->dst;
	}
        /* Cache smart blit BG layer info */
	if (blit_op->mdp_op & MDPOP_SMART_BLIT)
		bg_img_param = blit_op->src;

	/* Jumping from Y-Plane to Chroma Plane */
	/* first pixel addr calculation */
	mdp_adjust_start_addr(blit_op, &blit_op->src, sv_slice,
				sh_slice, LAYER_FG);
	mdp_adjust_start_addr(blit_op, &blit_op->bg, dv_slice,
				dh_slice, LAYER_BG);
	mdp_adjust_start_addr(blit_op, &blit_op->dst, dv_slice,
				dh_slice, LAYER_FB);

	config_ppp_scale(blit_op, &ppp_operation_reg);

	config_ppp_blend(blit_op, &ppp_operation_reg);

	config_ppp_src(&blit_op->src, yuv2rgb);
	config_ppp_out(&blit_op->dst, yuv2rgb);

	pr_debug("BLIT FG Param Fmt %d (x %d,y %d,w %d,h %d), ROI(x %d,y %d, w\
		 %d, h %d) Addr_P0 %p, Stride S0 %d Addr_P1 %p, Stride S1 %d\n",
		blit_op->src.color_fmt, blit_op->src.prop.x, blit_op->src.prop.y,
		blit_op->src.prop.width, blit_op->src.prop.height,
		blit_op->src.roi.x, blit_op->src.roi.y, blit_op->src.roi.width,
		blit_op->src.roi.height, blit_op->src.p0, blit_op->src.stride0,
                blit_op->src.p1, blit_op->src.stride1);
	if (blit_op->bg.p0 != blit_op->dst.p0)
		pr_debug("BLIT BG Param Fmt %d (x %d,y %d,w %d,h %d), ROI(x %d,y %d, w\
			 %d, h %d) Addr %p, Stride S0 %d Addr_P1 %p, Stride S1 %d\n",
			blit_op->bg.color_fmt, blit_op->bg.prop.x, blit_op->bg.prop.y,
			blit_op->bg.prop.width, blit_op->bg.prop.height,
			blit_op->bg.roi.x, blit_op->bg.roi.y, blit_op->bg.roi.width,
			blit_op->bg.roi.height, blit_op->bg.p0, blit_op->bg.stride0,
	                blit_op->bg.p1, blit_op->bg.stride1);
	pr_debug("BLIT FB Param Fmt %d (x %d,y %d,w %d,h %d), ROI(x %d,y %d, w\
		 %d, h %d) Addr %p, Stride S0 %d Addr_P1 %p, Stride S1 %d\n",
		blit_op->dst.color_fmt, blit_op->dst.prop.x, blit_op->dst.prop.y,
		blit_op->dst.prop.width, blit_op->dst.prop.height,
		blit_op->dst.roi.x, blit_op->dst.roi.y, blit_op->dst.roi.width,
		blit_op->dst.roi.height, blit_op->dst.p0, blit_op->src.stride0,
                blit_op->dst.p1, blit_op->dst.stride1);

	PPP_WRITEL(ppp_operation_reg, MDP3_PPP_OP_MODE);
	mb();
	return 0;
}

void ppp_enable(void)
{
	PPP_WRITEL(0x1000, 0x30);
	mb();
}

int mdp3_ppp_init(void)
{
	load_ppp_lut(LUT_PRE_TABLE, ppp_default_pre_lut());
	load_ppp_lut(LUT_POST_TABLE, ppp_default_post_lut());
	load_csc_matrix(CSC_PRIMARY_MATRIX, ppp_csc_rgb2yuv());
	load_csc_matrix(CSC_SECONDARY_MATRIX, ppp_csc_table2());
	return 0;
}
