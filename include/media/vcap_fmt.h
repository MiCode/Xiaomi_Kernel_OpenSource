/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#ifndef VCAP_FMT_H
#define VCAP_FMT_H

#define V4L2_BUF_TYPE_INTERLACED_IN_DECODER (V4L2_BUF_TYPE_PRIVATE)
#define V4L2_BUF_TYPE_VP_OUT (V4L2_BUF_TYPE_PRIVATE + 1)

enum hal_vcap_mode {
	HAL_VCAP_MODE_PRO = 0,
	HAL_VCAP_MODE_INT,
};

enum hal_vcap_polar {
	HAL_VCAP_POLAR_NEG = 0,
	HAL_VCAP_POLAR_POS,
};

enum hal_vcap_color {
	HAL_VCAP_YUV = 0,
	HAL_VCAP_RGB,
};

struct v4l2_format_vc_ext {
	enum hal_vcap_mode     mode;
	enum hal_vcap_polar    h_polar;
	enum hal_vcap_polar    v_polar;
	enum hal_vcap_polar    d_polar;
	enum hal_vcap_color    color_space;

	float  clk_freq;
	uint32_t vtotal;
	uint32_t htotal;
	uint32_t hactive_start;
	uint32_t hactive_end;
	uint32_t vactive_start;
	uint32_t vactive_end;
	uint32_t vsync_start;
	uint32_t vsync_end;
	uint32_t hsync_start;
	uint32_t hsync_end;
	uint32_t f2_vactive_start;
	uint32_t f2_vactive_end;
	uint32_t f2_vsync_h_start;
	uint32_t f2_vsync_h_end;
	uint32_t f2_vsync_v_start;
	uint32_t f2_vsync_v_end;
	uint32_t sizeimage;
	uint32_t bytesperline;
};

enum vcap_type {
	VC_TYPE,
	VP_IN_TYPE,
	VP_OUT_TYPE,
};

struct vcap_priv_fmt {
	enum vcap_type type;
	union {
		struct v4l2_format_vc_ext timing;
		struct v4l2_pix_format pix;
		/* Once VP is created there will be another type in here */
	} u;
};
#endif
