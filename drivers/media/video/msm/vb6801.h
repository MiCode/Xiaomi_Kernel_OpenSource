/* Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
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

#ifndef VB6801_H
#define VB6801_H

#include <mach/board.h>

extern struct vb6801_reg_t vb6801_regs;	/* from vb6801_reg.c */

struct reg_struct {
	uint16_t vt_pix_clk_div;	/*  0x0300 */
	uint16_t vt_sys_clk_div;	/*  0x0302 */
	uint16_t pre_pll_clk_div;	/*  0x0304 */
	uint16_t pll_multiplier;	/*  0x0306 */
	uint16_t op_pix_clk_div;	/*  0x0308 */
	uint16_t op_sys_clk_div;	/*  0x030A */
	uint16_t scale_m;	/*  0x0404 */
	uint16_t row_speed;	/*  0x3016 */
	uint16_t x_addr_start;	/*  0x3004 */
	uint16_t x_addr_end;	/*  0x3008 */
	uint16_t y_addr_start;	/*  0x3002 */
	uint16_t y_addr_end;	/*  0x3006 */
	uint16_t read_mode;	/*  0x3040 */
	uint16_t x_output_size;	/*  0x034C */
	uint16_t y_output_size;	/*  0x034E */
	uint16_t line_length_pck;	/*  0x300C */
	uint16_t frame_length_lines;	/*  0x300A */
	uint16_t coarse_int_time;	/*  0x3012 */
	uint16_t fine_int_time;	/*  0x3014 */
};

enum i2c_data_len {
	D_LEN_BYTE,
	D_LEN_WORD
};

struct vb6801_i2c_reg_conf_t {
	unsigned short waddr;
	unsigned short wdata;
	uint8_t bdata;
	enum i2c_data_len dlen;
};

struct vb6801_reg_t {
	struct reg_struct const *reg_pat;
	uint16_t reg_pat_size;
	struct vb6801_i2c_reg_conf_t const *ttbl;
	uint16_t ttbl_size;
	struct vb6801_i2c_reg_conf_t const *lctbl;
	uint16_t lctbl_size;
	struct vb6801_i2c_reg_conf_t const *rftbl;
	uint16_t rftbl_size;
};

#endif /* VB6801_H */
