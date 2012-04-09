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

#define V4L2_BUF_TYPE_INTERLACED_IN_AFE (V4L2_BUF_TYPE_PRIVATE)
#define V4L2_BUF_TYPE_INTERLACED_IN_DECODER (V4L2_BUF_TYPE_PRIVATE + 1)

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

enum hal_vcap_vc_fmt {
	/* 1080p */
	HAL_VCAP_YUV_1080p_60_RH = 0,
	HAL_VCAP_YUV_1080p_60_FL,
	HAL_VCAP_RGB_1080p_60_FL,
	HAL_VCAP_YUV_1080p_24_FL,
	HAL_VCAP_YUV_1080p_24_RH,
	HAL_VCAP_YUV_1080p_24_RW,
	HAL_VCAP_YUV_1080p_60_RW,
	HAL_VCAP_YUV_1080p_50_FL,
	HAL_VCAP_YUV_1080p_50_RH,
	HAL_VCAP_YUV_1080p_25_FL,
	HAL_VCAP_YUV_1080p_25_RH,
	HAL_VCAP_YUV_1080p_30_RH,
	HAL_VCAP_RGB_1080p_25_FL,
	HAL_VCAP_RGB_1080p_25_RH,
	/* 1080i */
	HAL_VCAP_YUV_1080i_60_FL,
	HAL_VCAP_YUV_1080i_60_RH,
	HAL_VCAP_YUV_1080i_60_RW,
	HAL_VCAP_YUV_1080i_50_FL,
	HAL_VCAP_YUV_1080i_50_RH,
	HAL_VCAP_YUV_1080i_50_RW,
	HAL_VCAP_RGB_1080i_50_FL,
	HAL_VCAP_RGB_1080i_50_RH,
	/* 480i */
	HAL_VCAP_YUV_480i_60_RH,
	HAL_VCAP_YUV_480i_60_FL,
	HAL_VCAP_YUV_480i_60_RW,
	HAL_VCAP_YUV_2880_480i_60_FL,
	HAL_VCAP_YUV_2880_480i_60_RH,
	/* 480p */
	HAL_VCAP_YUV_480p_60_RH,
	HAL_VCAP_RGB_480p_60_RH,
	HAL_VCAP_RGB_480p_60_FL,
	HAL_VCAP_YUV_480p_60_FL,
	HAL_VCAP_YUV_480p_60_RW,
	HAL_VCAP_YUV_2880_480p_60_FL,
	HAL_VCAP_YUV_2880_480p_60_RH,
	/* 720p */
	HAL_VCAP_YUV_720p_60_FL,
	HAL_VCAP_RGB_720p_60_FL,
	HAL_VCAP_YUV_720p_60_RW,
	HAL_VCAP_YUV_720p_60_RH,
	HAL_VCAP_YUV_720p_50_FL,
	HAL_VCAP_YUV_720p_50_RW,
	HAL_VCAP_YUV_720p_50_RH,
	/* 576p */
	HAL_VCAP_YUV_576p_50_FL,
	HAL_VCAP_RGB_576p_50_FL,
	HAL_VCAP_YUV_576p_50_RW,
	HAL_VCAP_YUV_576p_50_RH,
	HAL_VCAP_YUV_1440_576p_50_RH,
	HAL_VCAP_YUV_2880_576p_50_FL,
	HAL_VCAP_YUV_2880_576p_50_RH,
	/* 576i */
	HAL_VCAP_YUV_576i_50_FL,
	HAL_VCAP_YUV_576i_50_RW,
	HAL_VCAP_YUV_576i_50_RH,
	/* XGA 1024x768 */
	HAL_VCAP_YUV_XGA_FL,
	HAL_VCAP_YUV_XGA_RH,
	HAL_VCAP_YUV_XGA_RB,
	/* SXGA 1280x1024 */
	HAL_VCAP_YUV_SXGA_FL,
	HAL_VCAP_RGB_SXGA_FL,
	HAL_VCAP_YUV_SXGA_RH,
	HAL_VCAP_YUV_SXGA_RB,
	/* UXGA 1600x1200 */
	HAL_VCAP_YUV_UXGA_FL,
	HAL_VCAP_RGB_UXGA_FL,
	HAL_VCAP_YUV_UXGA_RH,
	HAL_VCAP_YUV_UXGA_RB,
	/* test odd height */
	HAL_VCAP_ODD_HEIGHT,
	/* test odd width RGB only */
	HAL_VCAP_ODD_WIDTH,
};

struct v4l2_format_vc_ext {
	enum hal_vcap_vc_fmt   format;
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
};
#endif
