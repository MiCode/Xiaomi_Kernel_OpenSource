/* drivers/video/msm/mdp_ppp31.c
 *
 * Copyright (C) 2009 Code Aurora Forum. All rights reserved.
 * Copyright (C) 2009 Google Incorporated
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

#include <linux/errno.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <linux/msm_mdp.h>

#include "mdp_hw.h"
#include "mdp_ppp.h"

#define NUM_COEFFS			32

struct mdp_scale_coeffs {
	uint16_t	c[4][NUM_COEFFS];
};

struct mdp_scale_tbl_info {
	uint16_t			offset;
	uint32_t			set:2;
	int				use_pr;
	struct mdp_scale_coeffs		coeffs;
};

enum {
	MDP_SCALE_PT2TOPT4,
	MDP_SCALE_PT4TOPT6,
	MDP_SCALE_PT6TOPT8,
	MDP_SCALE_PT8TO8,
	MDP_SCALE_MAX,
};

static struct mdp_scale_coeffs mdp_scale_pr_coeffs = {
	.c = {
		[0] = {
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
		},
		[1] = {
			511, 511, 511, 511, 511, 511, 511, 511,
			511, 511, 511, 511, 511, 511, 511, 511,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
		},
		[2] = {
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			511, 511, 511, 511, 511, 511, 511, 511,
			511, 511, 511, 511, 511, 511, 511, 511,
		},
		[3] = {
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
		},
	},
};

static struct mdp_scale_tbl_info mdp_scale_tbl[MDP_SCALE_MAX] = {
	[ MDP_SCALE_PT2TOPT4 ]	= {
		.offset		= 0,
		.set		= MDP_PPP_SCALE_COEFF_D0_SET,
		.use_pr		= -1,
		.coeffs.c	= {
			[0] = {
				131, 131, 130, 129, 128, 127, 127, 126,
				125, 125, 124, 123, 123, 121, 120, 119,
				119, 118, 117, 117, 116, 115, 115, 114,
				113, 112, 111, 110, 109, 109, 108, 107,
			},
			[1] = {
				141, 140, 140, 140, 140, 139, 138, 138,
				138, 137, 137, 137, 136, 137, 137, 137,
				136, 136, 136, 135, 135, 135, 134, 134,
				134, 134, 134, 133, 133, 132, 132, 132,
			},
			[2] = {
				132, 132, 132, 133, 133, 134, 134, 134,
				134, 134, 135, 135, 135, 136, 136, 136,
				137, 137, 137, 136, 137, 137, 137, 138,
				138, 138, 139, 140, 140, 140, 140, 141,
			},
			[3] = {
				107, 108, 109, 109, 110, 111, 112, 113,
				114, 115, 115, 116, 117, 117, 118, 119,
				119, 120, 121, 123, 123, 124, 125, 125,
				126, 127, 127, 128, 129, 130, 131, 131,
			}
		},
	},
	[ MDP_SCALE_PT4TOPT6 ] = {
		.offset		= 32,
		.set		= MDP_PPP_SCALE_COEFF_D1_SET,
		.use_pr		= -1,
		.coeffs.c	= {
			[0] = {
				136, 132, 128, 123, 119, 115, 111, 107,
				103, 98, 95, 91, 87, 84, 80, 76,
				73, 69, 66, 62, 59, 57, 54, 50,
				47, 44, 41, 39, 36, 33, 32, 29,
			},
			[1] = {
				206, 205, 204, 204, 201, 200, 199, 197,
				196, 194, 191, 191, 189, 185, 184, 182,
				180, 178, 176, 173, 170, 168, 165, 162,
				160, 157, 155, 152, 148, 146, 142, 140,
			},
			[2] = {
				140, 142, 146, 148, 152, 155, 157, 160,
				162, 165, 168, 170, 173, 176, 178, 180,
				182, 184, 185, 189, 191, 191, 194, 196,
				197, 199, 200, 201, 204, 204, 205, 206,
			},
			[3] = {
				29, 32, 33, 36, 39, 41, 44, 47,
				50, 54, 57, 59, 62, 66, 69, 73,
				76, 80, 84, 87, 91, 95, 98, 103,
				107, 111, 115, 119, 123, 128, 132, 136,
			},
		},
	},
	[ MDP_SCALE_PT6TOPT8 ] = {
		.offset		= 64,
		.set		= MDP_PPP_SCALE_COEFF_D2_SET,
		.use_pr		= -1,
		.coeffs.c	= {
			[0] = {
				104, 96, 89, 82, 75, 68, 61, 55,
				49, 43, 38, 33, 28, 24, 20, 16,
				12, 9, 6, 4, 2, 0, -2, -4,
				-5, -6, -7, -7, -8, -8, -8, -8,
			},
			[1] = {
				303, 303, 302, 300, 298, 296, 293, 289,
				286, 281, 276, 270, 265, 258, 252, 245,
				238, 230, 223, 214, 206, 197, 189, 180,
				172, 163, 154, 145, 137, 128, 120, 112,
			},
			[2] = {
				112, 120, 128, 137, 145, 154, 163, 172,
				180, 189, 197, 206, 214, 223, 230, 238,
				245, 252, 258, 265, 270, 276, 281, 286,
				289, 293, 296, 298, 300, 302, 303, 303,
			},
			[3] = {
				-8, -8, -8, -8, -7, -7, -6, -5,
				-4, -2, 0, 2, 4, 6, 9, 12,
				16, 20, 24, 28, 33, 38, 43, 49,
				55, 61, 68, 75, 82, 89, 96, 104,
			},
		},
	},
	[ MDP_SCALE_PT8TO8 ] = {
		.offset		= 96,
		.set		= MDP_PPP_SCALE_COEFF_U1_SET,
		.use_pr		= -1,
		.coeffs.c	= {
			[0] = {
				0, -7, -13, -19, -24, -28, -32, -34,
				-37, -39, -40, -41, -41, -41, -40, -40,
				-38, -37, -35, -33, -31, -29, -26, -24,
				-21, -18, -15, -13, -10, -7, -5, -2,
			},
			[1] = {
				511, 507, 501, 494, 485, 475, 463, 450,
				436, 422, 405, 388, 370, 352, 333, 314,
				293, 274, 253, 233, 213, 193, 172, 152,
				133, 113, 95, 77, 60, 43, 28, 13,
			},
			[2] = {
				0, 13, 28, 43, 60, 77, 95, 113,
				133, 152, 172, 193, 213, 233, 253, 274,
				294, 314, 333, 352, 370, 388, 405, 422,
				436, 450, 463, 475, 485, 494, 501, 507,
			},
			[3] = {
				0, -2, -5, -7, -10, -13, -15, -18,
				-21, -24, -26, -29, -31, -33, -35, -37,
				-38, -40, -40, -41, -41, -41, -40, -39,
				-37, -34, -32, -28, -24, -19, -13, -7,
			},
		},
	},
};

static void load_table(const struct mdp_info *mdp, int scale, int use_pr)
{
	int i;
	uint32_t val;
	struct mdp_scale_coeffs *coeffs;
	struct mdp_scale_tbl_info *tbl = &mdp_scale_tbl[scale];

	if (use_pr == tbl->use_pr)
		return;

	tbl->use_pr = use_pr;
	if (!use_pr)
		coeffs = &tbl->coeffs;
	else
		coeffs = &mdp_scale_pr_coeffs;

	for (i = 0; i < NUM_COEFFS; ++i) {
		val = ((coeffs->c[1][i] & 0x3ff) << 16) |
			(coeffs->c[0][i] & 0x3ff);
		mdp_writel(mdp, val, MDP_PPP_SCALE_COEFF_LSBn(tbl->offset + i));

		val = ((coeffs->c[3][i] & 0x3ff) << 16) |
			(coeffs->c[2][i] & 0x3ff);
		mdp_writel(mdp, val, MDP_PPP_SCALE_COEFF_MSBn(tbl->offset + i));
	}
}

#define SCALER_PHASE_BITS		29
static void scale_params(uint32_t dim_in, uint32_t dim_out, uint32_t scaler,
			 uint32_t *phase_init, uint32_t *phase_step)
{
	uint64_t src = dim_in;
	uint64_t dst = dim_out;
	uint64_t numer;
	uint64_t denom;

	*phase_init = 0;

	if (dst == 1) {
		/* if destination is 1 pixel wide, the value of phase_step
		 * is unimportant. */
		*phase_step = (uint32_t) (src << SCALER_PHASE_BITS);
		if (scaler == MDP_PPP_SCALER_FIR)
			*phase_init =
				(uint32_t) ((src - 1) << SCALER_PHASE_BITS);
		return;
	}

	if (scaler == MDP_PPP_SCALER_FIR) {
		numer = (src - 1) << SCALER_PHASE_BITS;
		denom = dst - 1;
		/* we want to round up the result*/
		numer += denom - 1;
	} else {
		numer = src << SCALER_PHASE_BITS;
		denom = dst;
	}

	do_div(numer, denom);
	*phase_step = (uint32_t) numer;
}

static int scale_idx(int factor)
{
	int idx;

	if (factor > 80)
		idx = MDP_SCALE_PT8TO8;
	else if (factor > 60)
		idx = MDP_SCALE_PT6TOPT8;
	else if (factor > 40)
		idx = MDP_SCALE_PT4TOPT6;
	else
		idx = MDP_SCALE_PT2TOPT4;

	return idx;
}

int mdp_ppp_cfg_scale(const struct mdp_info *mdp, struct ppp_regs *regs,
		      struct mdp_rect *src_rect, struct mdp_rect *dst_rect,
		      uint32_t src_format, uint32_t dst_format)
{
	uint32_t x_fac;
	uint32_t y_fac;
	uint32_t scaler_x = MDP_PPP_SCALER_FIR;
	uint32_t scaler_y = MDP_PPP_SCALER_FIR;
	// Don't use pixel repeat mode, it looks bad
	int use_pr = 0;
	int x_idx;
	int y_idx;

	if (unlikely(src_rect->w > 2048 || src_rect->h > 2048))
		return -ENOTSUPP;

	x_fac = (dst_rect->w * 100) / src_rect->w;
	y_fac = (dst_rect->h * 100) / src_rect->h;

	/* if down-scaling by a factor smaller than 1/4, use M/N */
	scaler_x = x_fac <= 25 ? MDP_PPP_SCALER_MN : MDP_PPP_SCALER_FIR;
	scaler_y = y_fac <= 25 ? MDP_PPP_SCALER_MN : MDP_PPP_SCALER_FIR;
	scale_params(src_rect->w, dst_rect->w, scaler_x, &regs->phasex_init,
		     &regs->phasex_step);
	scale_params(src_rect->h, dst_rect->h, scaler_y, &regs->phasey_init,
		     &regs->phasey_step);

	x_idx = scale_idx(x_fac);
	y_idx = scale_idx(y_fac);
	load_table(mdp, x_idx, use_pr);
	load_table(mdp, y_idx, use_pr);

	regs->scale_cfg = 0;
	// Enable SVI when source or destination is YUV
	if (!IS_RGB(src_format) && !IS_RGB(dst_format))
		regs->scale_cfg |= (1 << 6);
	regs->scale_cfg |= (mdp_scale_tbl[x_idx].set << 2) |
		(mdp_scale_tbl[x_idx].set << 4);
	regs->scale_cfg |= (scaler_x << 0) | (scaler_y << 1);

	return 0;
}

int mdp_ppp_load_blur(const struct mdp_info *mdp)
{
	return -ENOTSUPP;
}

void mdp_ppp_init_scale(const struct mdp_info *mdp)
{
	int scale;
	for (scale = 0; scale < MDP_SCALE_MAX; ++scale)
		load_table(mdp, scale, 0);
}
