/* Copyright (c) 2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "imx074.h"
const struct reg_struct_init imx074_reg_init[1] = {
	{
		/* PLL setting */
		0x02,	/* pll_divider 0x0305 */
		0x4B,	/* plstatim 0x302b */
		0x03,	/* reg_3024 */
		0x00,	/* image_orientation 0x0101 */
		0x80,	/* vndmy_ablmgshlmt 0x300a*/
		0x08,	/* y_opbaddr_start_di 3014*/
		0x37,	/* 0x3015*/
		0x01,	/* 0x301c*/
		0x05,	/* 0x302c*/
		0x26,	/* 0x3031*/
		0x60,	/* 0x3041*/
		0x24,	/* 0x3051 CLK DIV*/
		0x34,	/* 0x3053*/
		0xc0,	/* 0x3057*/
		0x09,	/* 0x305c*/
		0x07,	/* 0x305d */
		0x30,	/* 0x3060 */
		0x00,	/* 0x3065 */
		0x08,	/* 0x30aa */
		0x1c,	/* 0x30ab */
		0x32,	/* 0x30b0 */
		0x83,	/* 0x30b2 */
		0x04,	/* 0x30d3 */
		0x78,	/* 0x3106 */
		0x82,	/* 0x310c */
		0x05,	/* 0x3304 */
		0x04,	/* 0x3305 */
		0x11,	/* 0x3306 */
		0x02,	/* 0x3307 */
		0x0c,	/* 0x3308 */
		0x06,	/* 0x3309 */
		0x08,	/* 0x330a */
		0x04,	/* 0x330b */
		0x08,	/* 0x330c */
		0x06,	/* 0x330d */
		0x01,	/* 0x330f */
		0x00,	/* 0x3381 */

	}
};

/* Preview / Snapshot register settings	*/
const struct reg_struct	imx074_reg_pat[2] = {
	/*preview*/
	{
		0x2D, /*pll_multiplier*/
		0x06, /*frame_length_lines_hi 0x0340*/
		0x2D, /* frame_length_lines_lo 0x0341*/
		0x00, /* y_addr_start 0x347 */
		0x2F, /* y_add_end 0x034b */
		0x08, /* x_output_size_msb0x034c */
		0x38, /* x_output_size_lsb0x034d */
		0x06, /*  y_output_size_msb0x034e */
		0x18, /*  y_output_size_lsb0x034f */
		0x01, /* x_even_inc 0x0381 */
		0x03, /* x_odd_inc 0x0383 */
		0x01, /* y_even_inc 0x0385 */
		0x03, /* y_odd_inc 0x0387 */
		0x80, /* hmodeadd0x3001 */
		0x16, /* vmodeadd0x3016 */
		0x24, /* vapplinepos_startox3069*/
		0x53, /* vapplinepos_end306b*/
		0x00,/*  shutter 0x3086 */
		0x80, /* haddave 0x30e8 */
		0x83, /* lanesel 0x3301 */
	},

	/*snapshot*/
	{
		0x26, /*pll_multiplier*/
		0x0C, /* frame_length_lines_hi 0x0340*/
		0x90, /* frame_length_lines_lo 0x0341*/
		0x00, /* y_addr_start 0x347 */
		0x2F, /* y_add_end 0x034b */
		0x10, /* x_output_size_msb0x034c */
		0x70, /* x_output_size_lsb0x034d */
		0x0c, /* y_output_size_msb0x034e */
		0x30, /* y_output_size_lsb0x034f */
		0x01, /* x_even_inc 0x0381 */
		0x01, /* x_odd_inc 0x0383 */
		0x01, /* y_even_inc 0x0385 */
		0x01, /* y_odd_inc 0x0387 */
		0x00, /* hmodeadd0x3001 */
		0x06, /* vmodeadd0x3016 */
		0x24, /* vapplinepos_startox3069*/
		0x53, /* vapplinepos_end306b*/
		0x00, /* shutter 0x3086 */
		0x00, /* haddave 0x30e8 */
		0x03, /* lanesel 0x3301 */
	}
};
struct imx074_reg imx074_regs = {
	.reg_pat_init = &imx074_reg_init[0],
	.reg_pat = &imx074_reg_pat[0],
};
