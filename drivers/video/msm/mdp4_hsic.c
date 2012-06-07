/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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

#include <linux/module.h>
#include <linux/types.h>
#include <linux/msm_mdp.h>
#include "mdp.h"
#include "mdp4.h"

/* Definitions */
#define MDP4_CSC_MV_OFF		0x4400
#define MDP4_CSC_PRE_BV_OFF	0x4500
#define MDP4_CSC_POST_BV_OFF	0x4580
#define MDP4_CSC_PRE_LV_OFF	0x4600
#define MDP4_CSC_POST_LV_OFF	0x4680
#define MDP_VG1_BASE	(MDP_BASE + MDP4_VIDEO_BASE)

#define MDP_VG1_CSC_MVn(n)	(MDP_VG1_BASE + MDP4_CSC_MV_OFF + 4 * (n))
#define MDP_VG1_CSC_PRE_LVn(n)	(MDP_VG1_BASE + MDP4_CSC_PRE_LV_OFF + 4 * (n))
#define MDP_VG1_CSC_POST_LVn(n)	(MDP_VG1_BASE + MDP4_CSC_POST_LV_OFF + 4 * (n))
#define MDP_VG1_CSC_PRE_BVn(n)	(MDP_VG1_BASE + MDP4_CSC_PRE_BV_OFF + 4 * (n))
#define MDP_VG1_CSC_POST_BVn(n)	(MDP_VG1_BASE + MDP4_CSC_POST_BV_OFF + 4 * (n))

#define Q16	(16)
#define Q16_ONE	(1 << Q16)

#define Q16_VALUE(x)	((int32_t)((uint32_t)x << Q16))
#define Q16_PERCENT_VALUE(x, n)	((int32_t)( \
				div_s64(((int64_t)x * (int64_t)Q16_ONE), n)))

#define Q16_WHOLE(x)	((int32_t)(x >> 16))
#define Q16_FRAC(x)	((int32_t)(x & 0xFFFF))
#define Q16_S1Q16_MUL(x, y)	(((x >> 1) * (y >> 1)) >> 14)

#define Q16_MUL(x, y)	((int32_t)((((int64_t)x) * ((int64_t)y)) >> Q16))
#define Q16_NEGATE(x)	(0 - (x))

/*
 * HSIC Control min/max values
 *    These settings are based on the maximum/minimum allowed modifications to
 *    HSIC controls for layer and display color.  Allowing too much variation in
 *    the CSC block will result in color clipping resulting in unwanted color
 *    shifts.
 */
#define TRIG_MAX	Q16_VALUE(128)
#define CON_SAT_MAX	Q16_VALUE(128)
#define INTENSITY_MAX	(Q16_VALUE(2047) >> 12)

#define HUE_MAX	Q16_VALUE(100)
#define HUE_MIN	Q16_VALUE(-100)
#define HUE_DEF	Q16_VALUE(0)

#define SAT_MAX	Q16_VALUE(100)
#define SAT_MIN	Q16_VALUE(-100)
#define SAT_DEF	CON_SAT_MAX

#define CON_MAX	Q16_VALUE(100)
#define CON_MIN	Q16_VALUE(-100)
#define CON_DEF	CON_SAT_MAX

#define INTEN_MAX	Q16_VALUE(100)
#define INTEN_MIN	Q16_VALUE(-100)
#define INTEN_DEF	Q16_VALUE(0)

enum {
	DIRTY,
	GENERATED,
	CLEAN
};

/* local vars*/
static int32_t csc_matrix_tab[3][3] = {
	{0x00012a00, 0x00000000, 0x00019880},
	{0x00012a00, 0xffff9b80, 0xffff3000},
	{0x00012a00, 0x00020480, 0x00000000}
};

static int32_t csc_yuv2rgb_conv_tab[3][3] = {
	{0x00010000, 0x00000000, 0x000123cb},
	{0x00010000, 0xffff9af9, 0xffff6b5e},
	{0x00010000, 0x00020838, 0x00000000}
};

static int32_t csc_rgb2yuv_conv_tab[3][3] = {
	{0x00004c8b, 0x00009645, 0x00001d2f},
	{0xffffda56, 0xffffb60e, 0x00006f9d},
	{0x00009d70, 0xffff7c2a, 0xffffe666}
};

static uint32_t csc_pre_bv_tab[3]  = {0xfffff800, 0xffffc000, 0xffffc000};
static uint32_t csc_post_bv_tab[3] = {0x00000000, 0x00000000, 0x00000000};

static uint32_t csc_pre_lv_tab[6] =  {0x00000000, 0x00007f80, 0x00000000,
					0x00007f80, 0x00000000, 0x00007f80};
static uint32_t csc_post_lv_tab[6] = {0x00000000, 0x00007f80, 0x00000000,
					0x00007f80, 0x00000000, 0x00007f80};

/* Lookup table for Sin/Cos lookup - Q16*/
static const int32_t  trig_lut[65] = {
	0x00000000, /* sin((2*M_PI/256) * 0x00);*/
	0x00000648, /* sin((2*M_PI/256) * 0x01);*/
	0x00000C90, /* sin((2*M_PI/256) * 0x02);*/
	0x000012D5,
	0x00001918,
	0x00001F56,
	0x00002590,
	0x00002BC4,
	0x000031F1,
	0x00003817,
	0x00003E34,
	0x00004447,
	0x00004A50,
	0x0000504D,
	0x0000563E,
	0x00005C22,
	0x000061F8,
	0x000067BE,
	0x00006D74,
	0x0000731A,
	0x000078AD,
	0x00007E2F,
	0x0000839C,
	0x000088F6,
	0x00008E3A,
	0x00009368,
	0x00009880,
	0x00009D80,
	0x0000A268,
	0x0000A736,
	0x0000ABEB,
	0x0000B086,
	0x0000B505,
	0x0000B968,
	0x0000BDAF,
	0x0000C1D8,
	0x0000C5E4,
	0x0000C9D1,
	0x0000CD9F,
	0x0000D14D,
	0x0000D4DB,
	0x0000D848,
	0x0000DB94,
	0x0000DEBE,
	0x0000E1C6,
	0x0000E4AA,
	0x0000E768,
	0x0000EA0A,
	0x0000EC83,
	0x0000EED9,
	0x0000F109,
	0x0000F314,
	0x0000F4FA,
	0x0000F6BA,
	0x0000F854,
	0x0000F9C8,
	0x0000FB15,
	0x0000FC3B,
	0x0000FD3B,
	0x0000FE13,
	0x0000FEC4,
	0x0000FF4E,
	0x0000FFB1,
	0x0000FFEC,
	0x00010000, /* sin((2*M_PI/256) * 0x40);*/
};

void trig_values_q16(int32_t deg, int32_t *cos, int32_t *sin)
{
	int32_t   angle;
	int32_t   quad, anglei, anglef;
	int32_t   v0 = 0, v1 = 0;
	int32_t   t1, t2;

	/*
	 * Scale the angle so that 256 is one complete revolution and mask it
	 * to this domain
	 * NOTE: 0xB60B == 256/360
	 */
	angle = Q16_MUL(deg, 0xB60B) & 0x00FFFFFF;

	/* Obtain a quadrant number, integer, and fractional part */
	quad   =  angle >> 22;
	anglei = (angle >> 16) & 0x3F;
	anglef =  angle & 0xFFFF;

	/*
	 * Using the integer part, obtain the lookup table entry and its
	 * complement. Using the quadrant, swap and negate these as
	 * necessary.
	 * (The values and all derivatives of sine and cosine functions
	 * can be derived from these values)
	 */
	switch (quad) {
	case 0x0:
		v0 += trig_lut[anglei];
		v1 += trig_lut[0x40-anglei];
		break;

	case 0x1:
		v0 += trig_lut[0x40-anglei];
		v1 -= trig_lut[anglei];
		break;

	case 0x2:
		v0 -= trig_lut[anglei];
		v1 -= trig_lut[0x40-anglei];
		break;

	case 0x3:
		v0 -= trig_lut[0x40-anglei];
		v1 += trig_lut[anglei];
		break;
	}

	/*
	 * Multiply the fractional part by 2*PI/256 to move it from lookup
	 *  table units to radians, giving us the coefficient for first
	 *  derivatives.
	 */
	t1 = Q16_S1Q16_MUL(anglef, 0x0648);

	/*
	 * Square this and divide by 2 to get the coefficient for second
	 *   derivatives
	 */
	t2 = Q16_S1Q16_MUL(t1, t1) >> 1;

	*sin = v0 + Q16_S1Q16_MUL(v1, t1) - Q16_S1Q16_MUL(v0, t2);

	*cos = v1 - Q16_S1Q16_MUL(v0, t1) - Q16_S1Q16_MUL(v1, t2);
}

/* Convert input Q16 value to s4.9 */
int16_t convert_q16_s49(int32_t q16Value)
{	/* Top half is the whole number, Bottom half is fractional portion*/
	int16_t whole = Q16_WHOLE(q16Value);
	int32_t fraction  = Q16_FRAC(q16Value);

	/* Clamp whole to 3 bits */
	if (whole > 7)
		whole = 7;
	else if (whole < -7)
		whole = -7;

	/* Reduce fraction to 9 bits. */
	fraction = (fraction<<9)>>Q16;

	return (int16_t) ((int16_t)whole<<9) | ((int16_t)fraction);
}

/* Convert input Q16 value to uint16 */
int16_t convert_q16_int16(int32_t val)
{
	int32_t rounded;

	if (val >= 0) {
		/* Add 0.5 */
		rounded = val + (Q16_ONE>>1);
	} else {
		/* Subtract 0.5 */
		rounded = val - (Q16_ONE>>1);
	}

	/* Truncate rounded value */
	return (int16_t)(rounded>>Q16);
}

/*
 * norm_q16
 *              Return a Q16 value represeting a normalized value
 *
 * value       -100%                 0%               +100%
 *                 |-----------------|----------------|
 *                 ^                 ^                ^
 *             q16MinValue     q16DefaultValue       q16MaxValue
 *
 */
int32_t norm_q16(int32_t value, int32_t min, int32_t default_val, int32_t max,
								int32_t range)
{
	int32_t diff, perc, mul, result;

	if (0 == value) {
		result = default_val;
	} else if (value > 0) {
		/* value is between 0% and +100% represent 1.0 -> QRange Max */
		diff = range;
		perc = Q16_PERCENT_VALUE(value, max);
		mul = Q16_MUL(perc, diff);
		result = default_val + mul;
	} else {
		/* if (value <= 0) */
		diff = -range;
		perc = Q16_PERCENT_VALUE(-value, -min);
		mul = Q16_MUL(perc, diff);
		result = default_val + mul;
	}
	return result;
}

void matrix_mul_3x3(int32_t dest[][3], int32_t a[][3], int32_t b[][3])
{
	int32_t i, j, k;
	int32_t tmp[3][3];

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			tmp[i][j] = 0;
			for (k = 0; k < 3; k++)
				tmp[i][j] += Q16_MUL(a[i][k], b[k][j]);
		}
	}

	/* in case dest = a or b*/
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++)
			dest[i][j] = tmp[i][j];
	}
}

#define CONVERT(x)	(x)/*convert_q16_s49((x))*/
void pr_params(struct mdp4_hsic_regs *regs)
{
	int i;
	if (regs) {
		for (i = 0; i < NUM_HSIC_PARAM; i++) {
			pr_info("\t: hsic->params[%d] =	0x%08x [raw = 0x%08x]\n",
			i, CONVERT(regs->params[i]), regs->params[i]);
		}
	}
}

void pr_3x3_matrix(int32_t in[][3])
{
	pr_info("\t[0x%08x\t0x%08x\t0x%08x]\n", CONVERT(in[0][0]),
	CONVERT(in[0][1]), CONVERT(in[0][2]));
	pr_info("\t[0x%08x\t0x%08x\t0x%08x]\n", CONVERT(in[1][0]),
	CONVERT(in[1][1]), CONVERT(in[1][2]));
	pr_info("\t[0x%08x\t0x%08x\t0x%08x]\n", CONVERT(in[2][0]),
	CONVERT(in[2][1]), CONVERT(in[2][2]));
}

void _hsic_get(struct mdp4_hsic_regs *regs, int32_t type, int8_t *val)
{
	if (type < 0 || type >= NUM_HSIC_PARAM)
		BUG_ON(-EINVAL);
	*val = regs->params[type];
	pr_info("%s: getting params[%d] = %d\n", __func__, type, *val);
}

void _hsic_set(struct mdp4_hsic_regs *regs, int32_t type, int8_t val)
{
	if (type < 0 || type >= NUM_HSIC_PARAM)
		BUG_ON(-EINVAL);

	if (regs->params[type] != Q16_VALUE(val)) {
		regs->params[type] = Q16_VALUE(val);
		regs->dirty = DIRTY;
	}
}

void _hsic_generate_csc_matrix(struct mdp4_overlay_pipe *pipe)
{
	int i, j;
	int32_t sin, cos;

	int32_t hue_matrix[3][3];
	int32_t con_sat_matrix[3][3];
	struct mdp4_hsic_regs *regs = &(pipe->hsic_regs);

	memset(con_sat_matrix, 0x0, sizeof(con_sat_matrix));
	memset(hue_matrix, 0x0, sizeof(hue_matrix));

	/*
	 * HSIC control require matrix multiplication of these two tables
	 *  [T 0 0][1 0  0]   T = Contrast       C=Cos(Hue)
	 *  [0 S 0][0 C -N]   S = Saturation     N=Sin(Hue)
	 *  [0 0 S][0 N  C]
	 */

	con_sat_matrix[0][0] = norm_q16(regs->params[HSIC_CON], CON_MIN,
						CON_DEF, CON_MAX, CON_SAT_MAX);
	con_sat_matrix[1][1] = norm_q16(regs->params[HSIC_SAT], SAT_MIN,
						SAT_DEF, SAT_MAX, CON_SAT_MAX);
	con_sat_matrix[2][2] = con_sat_matrix[1][1];

	hue_matrix[0][0] = TRIG_MAX;

	trig_values_q16(norm_q16(regs->params[HSIC_HUE], HUE_MIN, HUE_DEF,
					 HUE_MAX, TRIG_MAX), &cos, &sin);

	cos = Q16_MUL(cos, TRIG_MAX);
	sin = Q16_MUL(sin, TRIG_MAX);

	hue_matrix[1][1] = cos;
	hue_matrix[2][2] = cos;
	hue_matrix[2][1] = sin;
	hue_matrix[1][2] = Q16_NEGATE(sin);

	/* Generate YUV CSC matrix */
	matrix_mul_3x3(regs->conv_matrix, con_sat_matrix, hue_matrix);

	if (!(pipe->op_mode & MDP4_OP_SRC_DATA_YCBCR)) {
		/* Convert input RGB to YUV then apply CSC matrix */
		pr_info("Pipe %d, has RGB input\n", pipe->pipe_num);
		matrix_mul_3x3(regs->conv_matrix, regs->conv_matrix,
							csc_rgb2yuv_conv_tab);
	}

	/* Normalize the matrix */
	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++)
			regs->conv_matrix[i][j] = (regs->conv_matrix[i][j]>>14);
	}

	/* Multiply above result by current csc table */
	matrix_mul_3x3(regs->conv_matrix, regs->conv_matrix, csc_matrix_tab);

	if (!(pipe->op_mode & MDP4_OP_SRC_DATA_YCBCR)) {
		/*HACK:only "works"for src side*/
		/* Convert back to RGB */
		pr_info("Pipe %d, has RGB output\n", pipe->pipe_num);
		matrix_mul_3x3(regs->conv_matrix, csc_yuv2rgb_conv_tab,
							regs->conv_matrix);
	}

	/* Update clamps pre and post. */
	/* TODO: different tables for different color formats? */
	for (i = 0; i < 6; i++) {
		regs->pre_limit[i] = csc_pre_lv_tab[i];
		regs->post_limit[i] = csc_post_lv_tab[i];
	}

	/* update bias values, pre and post */
	for (i = 0; i < 3; i++) {
		regs->pre_bias[i] = csc_pre_bv_tab[i];
		regs->post_bias[i] = csc_post_bv_tab[i] +
				norm_q16(regs->params[HSIC_INT],
				INTEN_MIN, INTEN_DEF, INTEN_MAX, INTENSITY_MAX);
	}

	regs->dirty = GENERATED;
}

void _hsic_update_mdp(struct mdp4_overlay_pipe *pipe)
{
	struct mdp4_hsic_regs *regs = &(pipe->hsic_regs);
	int i, j, k;

	uint32_t *csc_mv;
	uint32_t *pre_lv;
	uint32_t *post_lv;
	uint32_t *pre_bv;
	uint32_t *post_bv;

	switch (pipe->pipe_num) {
	case OVERLAY_PIPE_VG2:
		csc_mv = (uint32_t *) (MDP_VG1_CSC_MVn(0) +
					MDP4_VIDEO_OFF);
		pre_lv = (uint32_t *) (MDP_VG1_CSC_PRE_LVn(0) +
					MDP4_VIDEO_OFF);
		post_lv = (uint32_t *) (MDP_VG1_CSC_POST_LVn(0) +
					MDP4_VIDEO_OFF);
		pre_bv = (uint32_t *) (MDP_VG1_CSC_PRE_BVn(0) +
					MDP4_VIDEO_OFF);
		post_bv = (uint32_t *) (MDP_VG1_CSC_POST_BVn(0) +
					MDP4_VIDEO_OFF);
		break;
	case OVERLAY_PIPE_VG1:
	default:
			csc_mv = (uint32_t *) MDP_VG1_CSC_MVn(0);
			pre_lv = (uint32_t *) MDP_VG1_CSC_PRE_LVn(0);
			post_lv = (uint32_t *) MDP_VG1_CSC_POST_LVn(0);
			pre_bv = (uint32_t *) MDP_VG1_CSC_PRE_BVn(0);
			post_bv = (uint32_t *) MDP_VG1_CSC_POST_BVn(0);
		break;
	}

	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_ON, FALSE);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			k = (3*i) + j;
			MDP_OUTP(csc_mv + k, convert_q16_s49(
						regs->conv_matrix[i][j]));
		}
	}

	for (i = 0; i < 6; i++) {
		MDP_OUTP(pre_lv + i, convert_q16_s49(regs->pre_limit[i]));
		MDP_OUTP(post_lv + i, convert_q16_s49(regs->post_limit[i]));
	}

	for (i = 0; i < 3; i++) {
		MDP_OUTP(pre_bv + i, convert_q16_s49(regs->pre_bias[i]));
		MDP_OUTP(post_bv + i, convert_q16_s49(regs->post_bias[i]));
	}
	mdp_pipe_ctrl(MDP_CMD_BLOCK, MDP_BLOCK_POWER_OFF, FALSE);

	regs->dirty = CLEAN;
}

void mdp4_hsic_get(struct mdp4_overlay_pipe *pipe, struct dpp_ctrl *ctrl)
{
	int i;
	for (i = 0; i < NUM_HSIC_PARAM; i++)
		_hsic_get(&(pipe->hsic_regs), i, &(ctrl->hsic_params[i]));
}

void mdp4_hsic_set(struct mdp4_overlay_pipe *pipe, struct dpp_ctrl *ctrl)
{
	int i;
	for (i = 0; i < NUM_HSIC_PARAM; i++)
		_hsic_set(&(pipe->hsic_regs), i, ctrl->hsic_params[i]);

	if (pipe->hsic_regs.dirty == DIRTY)
		_hsic_generate_csc_matrix(pipe);
}

void mdp4_hsic_update(struct mdp4_overlay_pipe *pipe)
{
	if (pipe->hsic_regs.dirty == GENERATED)
		_hsic_update_mdp(pipe);
}
