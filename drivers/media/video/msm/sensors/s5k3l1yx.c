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

#include "msm_sensor.h"
#define SENSOR_NAME "s5k3l1yx"
#define PLATFORM_DRIVER_NAME "msm_camera_s5k3l1yx"

DEFINE_MUTEX(s5k3l1yx_mut);
static struct msm_sensor_ctrl_t s5k3l1yx_s_ctrl;

static struct msm_camera_i2c_reg_conf s5k3l1yx_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf s5k3l1yx_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf s5k3l1yx_groupon_settings[] = {
	{0x104, 0x01},
};

static struct msm_camera_i2c_reg_conf s5k3l1yx_groupoff_settings[] = {
	{0x104, 0x00},
};

static struct msm_camera_i2c_reg_conf s5k3l1yx_snap_settings[] = {
	{0x0501, 0x00}, /* compression_algorithim_L(1d) */
	{0x0112, 0x0A}, /* CCP_data_format_H */
	{0x0113, 0x0A}, /* CCP_data_format_L raw8=0808 ,DCPM10 -->8= 0A08 */
	{0x0306, 0x00}, /* pll_multiplier */
	{0x0307, 0xA5}, /* pll_multiplier */
	{0x0202, 0x09}, /* coarse_integration_time */
	{0x0203, 0x32}, /* coarse_integration_time */
	{0x0340, 0x0B}, /* frame_length_lines */
	{0x0341, 0xEC}, /* frame_length_lines */
	{0x0342, 0x14}, /* line_length_pck */
	{0x0343, 0xD8}, /* line_length_pck */
	{0x0344, 0x00}, /* x_addr_start */
	{0x0345, 0x08}, /* x_addr_start */
	{0x0346, 0x00}, /* y_addr_start */
	{0x0347, 0x00}, /* y_addr_start */
	{0x0348, 0x0F}, /* x_addr_end */
	{0x0349, 0xA7}, /* x_addr_end */
	{0x034A, 0x0B}, /* y_addr_end */
	{0x034B, 0xC7}, /* y_addr_end */
	{0x034C, 0x0F}, /* x_output_size */
	{0x034D, 0xA0}, /* x_output_size */
	{0x034E, 0x0B}, /* y_output_size */
	{0x034F, 0xC8}, /* y_output_size */
	{0x0380, 0x00}, /* x_even_inc */
	{0x0381, 0x01}, /* x_even_inc */
	{0x0382, 0x00}, /* x_odd_inc */
	{0x0383, 0x01}, /* x_odd_inc */
	{0x0384, 0x00}, /* y_even_inc */
	{0x0385, 0x01}, /* y_even_inc */
	{0x0386, 0x00}, /* y_odd_inc */
	{0x0387, 0x01}, /* y_odd_inc */
	{0x0900, 0x00}, /* binning_mode */
	{0x0901, 0x22}, /* binning_type */
	{0x0902, 0x01}, /* binning_weighting */
};

static struct msm_camera_i2c_reg_conf s5k3l1yx_prev_settings[] = {
	{0x0501, 0x00}, /* compression_algorithim_L(1d) */
	{0x0112, 0x0A}, /* CCP_data_format_H */
	{0x0113, 0x0A}, /* CCP_data_format_L raw8=0808 ,DCPM10 -->8= 0A08 */
	{0x0306, 0x00}, /* pll_multiplier */
	{0x0307, 0xA5}, /* pll_multiplier */
	{0x0202, 0x06}, /* coarse_integration_time */
	{0x0203, 0x00}, /* coarse_integration_time */
	{0x0340, 0x09}, /* frame_length_lines */
	{0x0341, 0x6C}, /* frame_length_lines */
	{0x0342, 0x11}, /* line_length_pck */
	{0x0343, 0x80}, /* line_length_pck */
	{0x0344, 0x00}, /* x_addr_start */
	{0x0345, 0x18}, /* x_addr_start */
	{0x0346, 0x00}, /* y_addr_start */
	{0x0347, 0x00}, /* y_addr_start */
	{0x0348, 0x0F}, /* x_addr_end */
	{0x0349, 0x97}, /* x_addr_end */
	{0x034A, 0x0B}, /* y_addr_end */
	{0x034B, 0xC7}, /* y_addr_end */
	{0x034C, 0x07}, /* x_output_size */
	{0x034D, 0xC0}, /* x_output_size */
	{0x034E, 0x05}, /* y_output_size */
	{0x034F, 0xE4}, /* y_output_size */
	{0x0380, 0x00}, /* x_even_inc */
	{0x0381, 0x01}, /* x_even_inc */
	{0x0382, 0x00}, /* x_odd_inc */
	{0x0383, 0x03}, /* x_odd_inc */
	{0x0384, 0x00}, /* y_even_inc */
	{0x0385, 0x01}, /* y_even_inc */
	{0x0386, 0x00}, /* y_odd_inc */
	{0x0387, 0x03}, /* y_odd_inc */
	{0x0900, 0x01}, /* binning_mode */
	{0x0901, 0x22}, /* binning_type */
	{0x0902, 0x01}, /* binning_weighting */
};

static struct msm_camera_i2c_reg_conf s5k3l1yx_video_60fps_settings[] = {
	{0x0501, 0x00}, /* compression_algorithim_L(1d) */
	{0x0112, 0x0A}, /* CCP_data_format_H */
	{0x0113, 0x0A}, /* CCP_data_format_L raw8=0808 ,DCPM10 -->8= 0A08 */
	{0x0306, 0x00}, /* pll_multiplier */
	{0x0307, 0xA5}, /* pll_multiplier */
	{0x0202, 0x03}, /* coarse_integration_time */
	{0x0203, 0xD8}, /* coarse_integration_time */
	{0x0340, 0x03}, /* frame_length_lines */
	{0x0341, 0xE0}, /* frame_length_lines */
	{0x0342, 0x14}, /* line_length_pck */
	{0x0343, 0xD8}, /* line_length_pck */
	{0x0344, 0x01}, /* x_addr_start */
	{0x0345, 0x20}, /* x_addr_start */
	{0x0346, 0x02}, /* y_addr_start */
	{0x0347, 0x24}, /* y_addr_start */
	{0x0348, 0x0E}, /* x_addr_end */
	{0x0349, 0xA0}, /* x_addr_end */
	{0x034A, 0x09}, /* y_addr_end */
	{0x034B, 0xA4}, /* y_addr_end */
	{0x034C, 0x03}, /* x_output_size */
	{0x034D, 0x60}, /* x_output_size */
	{0x034E, 0x01}, /* y_output_size */
	{0x034F, 0xE0}, /* y_output_size */
	{0x0380, 0x00}, /* x_even_inc */
	{0x0381, 0x01}, /* x_even_inc */
	{0x0382, 0x00}, /* x_odd_inc */
	{0x0383, 0x07}, /* x_odd_inc */
	{0x0384, 0x00}, /* y_even_inc */
	{0x0385, 0x01}, /* y_even_inc */
	{0x0386, 0x00}, /* y_odd_inc */
	{0x0387, 0x07}, /* y_odd_inc */
	{0x0900, 0x01}, /* binning_mode */
	{0x0901, 0x44}, /* binning_type */
	{0x0902, 0x01}, /* binning_weighting */
};

static struct msm_camera_i2c_reg_conf s5k3l1yx_video_90fps_settings[] = {
	{0x0501, 0x00}, /* compression_algorithim_L(1d) */
	{0x0112, 0x0A}, /* CCP_data_format_H */
	{0x0113, 0x0A}, /* CCP_data_format_L raw8=0808 ,DCPM10 -->8= 0A08 */
	{0x0306, 0x00}, /* pll_multiplier */
	{0x0307, 0xA5}, /* pll_multiplier */
	{0x0202, 0x02}, /* coarse_integration_time */
	{0x0203, 0x90}, /* coarse_integration_time */
	{0x0340, 0x02}, /* frame_length_lines */
	{0x0341, 0x98}, /* frame_length_lines */
	{0x0342, 0x14}, /* line_length_pck */
	{0x0343, 0xD8}, /* line_length_pck */
	{0x0344, 0x01}, /* x_addr_start */
	{0x0345, 0x20}, /* x_addr_start */
	{0x0346, 0x02}, /* y_addr_start */
	{0x0347, 0x24}, /* y_addr_start */
	{0x0348, 0x0E}, /* x_addr_end */
	{0x0349, 0xA0}, /* x_addr_end */
	{0x034A, 0x09}, /* y_addr_end */
	{0x034B, 0xA4}, /* y_addr_end */
	{0x034C, 0x03}, /* x_output_size */
	{0x034D, 0x60}, /* x_output_size */
	{0x034E, 0x01}, /* y_output_size */
	{0x034F, 0xE0}, /* y_output_size */
	{0x0380, 0x00}, /* x_even_inc */
	{0x0381, 0x01}, /* x_even_inc */
	{0x0382, 0x00}, /* x_odd_inc */
	{0x0383, 0x07}, /* x_odd_inc */
	{0x0384, 0x00}, /* y_even_inc */
	{0x0385, 0x01}, /* y_even_inc */
	{0x0386, 0x00}, /* y_odd_inc */
	{0x0387, 0x07}, /* y_odd_inc */
	{0x0900, 0x01}, /* binning_mode */
	{0x0901, 0x44}, /* binning_type */
	{0x0902, 0x01}, /* binning_weighting */
};

static struct msm_camera_i2c_reg_conf s5k3l1yx_video_120fps_settings[] = {
	{0x0501, 0x00}, /* compression_algorithim_L(1d) */
	{0x0112, 0x0A}, /* CCP_data_format_H */
	{0x0113, 0x0A}, /* CCP_data_format_L raw8=0808 ,DCPM10 -->8= 0A08 */
	{0x0306, 0x00}, /* pll_multiplier */
	{0x0307, 0xA5}, /* pll_multiplier */
	{0x0202, 0x01}, /* coarse_integration_time */
	{0x0203, 0xFA}, /* coarse_integration_time */
	{0x0340, 0x02}, /* frame_length_lines */
	{0x0341, 0x02}, /* frame_length_lines */
	{0x0342, 0x14}, /* line_length_pck */
	{0x0343, 0xD8}, /* line_length_pck */
	{0x0344, 0x01}, /* x_addr_start */
	{0x0345, 0x20}, /* x_addr_start */
	{0x0346, 0x02}, /* y_addr_start */
	{0x0347, 0x24}, /* y_addr_start */
	{0x0348, 0x0E}, /* x_addr_end */
	{0x0349, 0xA0}, /* x_addr_end */
	{0x034A, 0x09}, /* y_addr_end */
	{0x034B, 0xA4}, /* y_addr_end */
	{0x034C, 0x03}, /* x_output_size */
	{0x034D, 0x60}, /* x_output_size */
	{0x034E, 0x01}, /* y_output_size */
	{0x034F, 0xE0}, /* y_output_size */
	{0x0380, 0x00}, /* x_even_inc */
	{0x0381, 0x01}, /* x_even_inc */
	{0x0382, 0x00}, /* x_odd_inc */
	{0x0383, 0x07}, /* x_odd_inc */
	{0x0384, 0x00}, /* y_even_inc */
	{0x0385, 0x01}, /* y_even_inc */
	{0x0386, 0x00}, /* y_odd_inc */
	{0x0387, 0x07}, /* y_odd_inc */
	{0x0900, 0x01}, /* binning_mode */
	{0x0901, 0x44}, /* binning_type */
	{0x0902, 0x01}, /* binning_weighting */
};

static struct msm_camera_i2c_reg_conf s5k3l1yx_dpcm_settings[] = {
	{0x0501, 0x01}, /* compression_algorithim_L(1d) */
	{0x0112, 0x0A}, /* CCP_data_format_H */
	{0x0113, 0x08}, /* CCP_data_format_L raw8=0808 ,DCPM10 -->8= 0A08 */
	{0x0306, 0x00}, /* pll_multiplier */
	{0x0307, 0xA0}, /* pll_multiplier */
	{0x0202, 0x09}, /* coarse_integration_time */
	{0x0203, 0x32}, /* coarse_integration_time */
	{0x0340, 0x0B}, /* frame_length_lines */
	{0x0341, 0xEC}, /* frame_length_lines */
	{0x0342, 0x11}, /* line_length_pck */
	{0x0343, 0x80}, /* line_length_pck */
	{0x0344, 0x00}, /* x_addr_start */
	{0x0345, 0x08}, /* x_addr_start */
	{0x0346, 0x00}, /* y_addr_start */
	{0x0347, 0x00}, /* y_addr_start */
	{0x0348, 0x0F}, /* x_addr_end */
	{0x0349, 0xA7}, /* x_addr_end */
	{0x034A, 0x0B}, /* y_addr_end */
	{0x034B, 0xC7}, /* y_addr_end */
	{0x034C, 0x0F}, /* x_output_size */
	{0x034D, 0xA0}, /* x_output_size */
	{0x034E, 0x0B}, /* y_output_size */
	{0x034F, 0xC8}, /* y_output_size */
	{0x0380, 0x00}, /* x_even_inc */
	{0x0381, 0x01}, /* x_even_inc */
	{0x0382, 0x00}, /* x_odd_inc */
	{0x0383, 0x01}, /* x_odd_inc */
	{0x0384, 0x00}, /* y_even_inc */
	{0x0385, 0x01}, /* y_even_inc */
	{0x0386, 0x00}, /* y_odd_inc */
	{0x0387, 0x01}, /* y_odd_inc */
	{0x0900, 0x00}, /* binning_mode */
	{0x0901, 0x22}, /* binning_type */
	{0x0902, 0x01}, /* binning_weighting */
};

static struct msm_camera_i2c_reg_conf s5k3l1yx_recommend_settings[] = {
	{0x0100, 0x00},
	{0x0103, 0x01}, /* software_reset */
	{0x0104, 0x00}, /* grouped_parameter_hold */
	{0x0114, 0x03}, /* CSI_lane_mode, 4 lane setting */
	{0x0120, 0x00}, /* gain_mode, global analogue gain*/
	{0x0121, 0x00}, /* exposure_mode, global exposure */
	{0x0136, 0x18}, /* Extclk_frequency_mhz */
	{0x0137, 0x00}, /* Extclk_frequency_mhz */
	{0x0200, 0x08}, /* fine_integration_time */
	{0x0201, 0x88}, /* fine_integration_time */
	{0x0204, 0x00}, /* analogue_gain_code_global */
	{0x0205, 0x20}, /* analogue_gain_code_global */
	{0x020E, 0x01}, /* digital_gain_greenR */
	{0x020F, 0x00}, /* digital_gain_greenR */
	{0x0210, 0x01}, /* digital_gain_red */
	{0x0211, 0x00}, /* digital_gain_red */
	{0x0212, 0x01}, /* digital_gain_blue */
	{0x0213, 0x00}, /* digital_gain_blue */
	{0x0214, 0x01}, /* digital_gain_greenB */
	{0x0215, 0x00}, /* digital_gain_greenB */
	{0x0300, 0x00}, /* vt_pix_clk_div */
	{0x0301, 0x02}, /* vt_pix_clk_div */
	{0x0302, 0x00}, /* vt_sys_clk_div */
	{0x0303, 0x01}, /* vt_sys_clk_div */
	{0x0304, 0x00}, /* pre_pll_clk_div */
	{0x0305, 0x06}, /* pre_pll_clk_div */
	{0x0308, 0x00}, /* op_pix_clk_div */
	{0x0309, 0x02}, /* op_pix_clk_div */
	{0x030A, 0x00}, /* op_sys_clk_div */
	{0x030B, 0x01}, /* op_sys_clk_div */
	{0x0800, 0x00}, /* tclk_post for D-PHY control */
	{0x0801, 0x00}, /* ths_prepare for D-PHY control */
	{0x0802, 0x00}, /* ths_zero_min for D-PHY control */
	{0x0803, 0x00}, /* ths_trail for D-PHY control */
	{0x0804, 0x00}, /* tclk_trail_min for D-PHY control */
	{0x0805, 0x00}, /* tclk_prepare for D-PHY control */
	{0x0806, 0x00}, /* tclk_zero_zero for D-PHY control */
	{0x0807, 0x00}, /* tlpx for D-PHY control */
	{0x0820, 0x02}, /* requested_link_bit_rate_mbps */
	{0x0821, 0x94}, /* requested_link_bit_rate_mbps */
	{0x0822, 0x00}, /* requested_link_bit_rate_mbps */
	{0x0823, 0x00}, /* requested_link_bit_rate_mbps */
	{0x3000, 0x0A},
	{0x3001, 0xF7},
	{0x3002, 0x0A},
	{0x3003, 0xF7},
	{0x3004, 0x08},
	{0x3005, 0xF8},
	{0x3006, 0x5B},
	{0x3007, 0x73},
	{0x3008, 0x49},
	{0x3009, 0x0C},
	{0x300A, 0xF8},
	{0x300B, 0x4E},
	{0x300C, 0x64},
	{0x300D, 0x5C},
	{0x300E, 0x71},
	{0x300F, 0x0C},
	{0x3010, 0x6A},
	{0x3011, 0x14},
	{0x3012, 0x14},
	{0x3013, 0x0C},
	{0x3014, 0x24},
	{0x3015, 0x4F},
	{0x3016, 0x86},
	{0x3017, 0x0E},
	{0x3018, 0x2C},
	{0x3019, 0x30},
	{0x301A, 0x31},
	{0x301B, 0x32},
	{0x301C, 0xFF},
	{0x301D, 0x33},
	{0x301E, 0x5C},
	{0x301F, 0xFA},
	{0x3020, 0x36},
	{0x3021, 0x46},
	{0x3022, 0x92},
	{0x3023, 0xF5},
	{0x3024, 0x6E},
	{0x3025, 0x19},
	{0x3026, 0x32},
	{0x3027, 0x4B},
	{0x3028, 0x04},
	{0x3029, 0x50},
	{0x302A, 0x0C},
	{0x302B, 0x04},
	{0x302C, 0xEF},
	{0x302D, 0xC1},
	{0x302E, 0x74},
	{0x302F, 0x40},
	{0x3030, 0x00},
	{0x3031, 0x00},
	{0x3032, 0x00},
	{0x3033, 0x00},
	{0x3034, 0x0F},
	{0x3035, 0x01},
	{0x3036, 0x00},
	{0x3037, 0x00},
	{0x3038, 0x88},
	{0x3039, 0x98},
	{0x303A, 0x1F},
	{0x303B, 0x01},
	{0x303C, 0x00},
	{0x303D, 0x03},
	{0x303E, 0x2F},
	{0x303F, 0x09},
	{0x3040, 0xFF},
	{0x3041, 0x22},
	{0x3042, 0x03},
	{0x3043, 0x03},
	{0x3044, 0x20},
	{0x3045, 0x10},
	{0x3046, 0x10},
	{0x3047, 0x08},
	{0x3048, 0x10},
	{0x3049, 0x01},
	{0x304A, 0x00},
	{0x304B, 0x80},
	{0x304C, 0x80},
	{0x304D, 0x00},
	{0x304E, 0x00},
	{0x304F, 0x00},
	{0x3051, 0x09},
	{0x3052, 0xC4},
	{0x305A, 0xE0},
	{0x323D, 0x04},
	{0x323E, 0x38},
	{0x3305, 0xDD},
	{0x3050, 0x01},
	{0x3202, 0x01},
	{0x3203, 0x01},
	{0x3204, 0x01},
	{0x3205, 0x01},
	{0x3206, 0x01},
	{0x3207, 0x01},
	{0x320A, 0x05},
	{0x320B, 0x20},
	{0x3235, 0xB7},
	{0x324C, 0x04},
	{0x324A, 0x07},
	{0x3902, 0x01},
	{0x3915, 0x70},
	{0x3916, 0x80},
	{0x3A00, 0x01},
	{0x3A06, 0x03},
	{0x3B29, 0x01},
	{0x3C11, 0x08},
	{0x3C12, 0x7B},
	{0x3C13, 0xC0},
	{0x3C14, 0x70},
	{0x3C15, 0x80},
	{0x3C20, 0x04},
	{0x3C23, 0x03},
	{0x3C24, 0x00},
	{0x3C50, 0x72},
	{0x3C51, 0x85},
	{0x3C53, 0x40},
	{0x3C55, 0xA0},
	{0x3D00, 0x00},
	{0x3D01, 0x00},
	{0x3D11, 0x01},
	{0x3486, 0x05},
	{0x3B35, 0x06},
	{0x3A05, 0x01},
	{0x3A07, 0x2B},
	{0x3A09, 0x01},
	{0x3940, 0xFF},
	{0x3300, 0x00},
	{0x3900, 0xFF},
	{0x3914, 0x08},
	{0x3A01, 0x0F},
	{0x3A02, 0xA0},
	{0x3A03, 0x0B},
	{0x3A04, 0xC8},
	{0x3701, 0x00},
	{0x3702, 0x00},
	{0x3703, 0x00},
	{0x3704, 0x00},
	{0x0101, 0x00}, /* image_orientation, mirror & flip off*/
	{0x0105, 0x01}, /* mask_corrupted_frames */
	{0x0110, 0x00}, /* CSI-2_channel_identifier */
	{0x3942, 0x01}, /* [0] 1:mipi, 0:pvi */
	{0x0B00, 0x00},
};

static struct v4l2_subdev_info s5k3l1yx_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array s5k3l1yx_init_conf[] = {
	{&s5k3l1yx_recommend_settings[0],
	ARRAY_SIZE(s5k3l1yx_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array s5k3l1yx_confs[] = {
	{&s5k3l1yx_snap_settings[0],
	ARRAY_SIZE(s5k3l1yx_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&s5k3l1yx_prev_settings[0],
	ARRAY_SIZE(s5k3l1yx_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&s5k3l1yx_video_60fps_settings[0],
	ARRAY_SIZE(s5k3l1yx_video_60fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&s5k3l1yx_video_90fps_settings[0],
	ARRAY_SIZE(s5k3l1yx_video_90fps_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&s5k3l1yx_video_120fps_settings[0],
	ARRAY_SIZE(s5k3l1yx_video_120fps_settings), 0,
					MSM_CAMERA_I2C_BYTE_DATA},
	{&s5k3l1yx_dpcm_settings[0],
	ARRAY_SIZE(s5k3l1yx_dpcm_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t s5k3l1yx_dimensions[] = {
	/* 20 fps snapshot */
	{
		.x_output = 4000,
		.y_output = 3016,
		.line_length_pclk = 5336,
		.frame_length_lines = 3052,
		.vt_pixel_clk = 330000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* 30 fps preview */
	{
		.x_output = 1984,
		.y_output = 1508,
		.line_length_pclk = 4480,
		.frame_length_lines = 2412,
		.vt_pixel_clk = 330000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* 60 fps video */
	{
		.x_output = 864,
		.y_output = 480,
		.line_length_pclk = 5336,
		.frame_length_lines = 992,
		.vt_pixel_clk = 330000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* 90 fps video */
	{
		.x_output = 864,
		.y_output = 480,
		.line_length_pclk = 5336,
		.frame_length_lines = 664,
		.vt_pixel_clk = 330000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* 120 fps video */
	{
		.x_output = 864,
		.y_output = 480,
		.line_length_pclk = 5336,
		.frame_length_lines = 514,
		.vt_pixel_clk = 330000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
	/* 24 fps snapshot */
	{
		.x_output = 4000,
		.y_output = 3016,
		.line_length_pclk = 4480,
		.frame_length_lines = 3052,
		.vt_pixel_clk = 330000000,
		.op_pixel_clk = 320000000,
		.binning_factor = 1,
	},
};

static struct msm_camera_csid_vc_cfg s5k3l1yx_cid_cfg[] = {
	{0, CSI_RAW10, CSI_DECODE_10BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params s5k3l1yx_csi_params = {
	.csid_params = {
		.lane_cnt = 4,
		.lut_params = {
			.num_cid = ARRAY_SIZE(s5k3l1yx_cid_cfg),
			.vc_cfg = s5k3l1yx_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 4,
		.settle_cnt = 0x1B,
	},
};

static struct msm_camera_csid_vc_cfg s5k3l1yx_cid_dpcm_cfg[] = {
	{0, CSI_RAW8, CSI_DECODE_DPCM_10_8_10},
};

static struct msm_camera_csi2_params s5k3l1yx_csi_dpcm_params = {
	.csid_params = {
		.lane_assign = 0xe4,
		.lane_cnt = 4,
		.lut_params = {
			.num_cid = ARRAY_SIZE(s5k3l1yx_cid_dpcm_cfg),
			.vc_cfg = s5k3l1yx_cid_dpcm_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 4,
		.settle_cnt = 0x1B,
	},
};

static struct msm_camera_csi2_params *s5k3l1yx_csi_params_array[] = {
	&s5k3l1yx_csi_params,
	&s5k3l1yx_csi_params,
	&s5k3l1yx_csi_params,
	&s5k3l1yx_csi_params,
	&s5k3l1yx_csi_params,
	&s5k3l1yx_csi_dpcm_params,
};

static struct msm_sensor_output_reg_addr_t s5k3l1yx_reg_addr = {
	.x_output = 0x34C,
	.y_output = 0x34E,
	.line_length_pclk = 0x342,
	.frame_length_lines = 0x340,
};

static struct msm_sensor_id_info_t s5k3l1yx_id_info = {
	.sensor_id_reg_addr = 0x0,
	.sensor_id = 0x3121,
};

static struct msm_sensor_exp_gain_info_t s5k3l1yx_exp_gain_info = {
	.coarse_int_time_addr = 0x202,
	.global_gain_addr = 0x204,
	.vert_offset = 8,
};

static const struct i2c_device_id s5k3l1yx_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&s5k3l1yx_s_ctrl},
	{ }
};

static struct i2c_driver s5k3l1yx_i2c_driver = {
	.id_table = s5k3l1yx_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k3l1yx_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static int __init msm_sensor_init_module(void)
{
	return i2c_add_driver(&s5k3l1yx_i2c_driver);
}

static struct v4l2_subdev_core_ops s5k3l1yx_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops s5k3l1yx_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops s5k3l1yx_subdev_ops = {
	.core = &s5k3l1yx_subdev_core_ops,
	.video  = &s5k3l1yx_subdev_video_ops,
};

static struct msm_sensor_fn_t s5k3l1yx_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = msm_sensor_write_exp_gain1,
	.sensor_write_snapshot_exp_gain = msm_sensor_write_exp_gain1,
	.sensor_setting = msm_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_adjust_frame_lines = msm_sensor_adjust_frame_lines1,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t s5k3l1yx_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = s5k3l1yx_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(s5k3l1yx_start_settings),
	.stop_stream_conf = s5k3l1yx_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(s5k3l1yx_stop_settings),
	.group_hold_on_conf = s5k3l1yx_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(s5k3l1yx_groupon_settings),
	.group_hold_off_conf = s5k3l1yx_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(s5k3l1yx_groupoff_settings),
	.init_settings = &s5k3l1yx_init_conf[0],
	.init_size = ARRAY_SIZE(s5k3l1yx_init_conf),
	.mode_settings = &s5k3l1yx_confs[0],
	.output_settings = &s5k3l1yx_dimensions[0],
	.num_conf = ARRAY_SIZE(s5k3l1yx_confs),
};

static struct msm_sensor_ctrl_t s5k3l1yx_s_ctrl = {
	.msm_sensor_reg = &s5k3l1yx_regs,
	.sensor_i2c_client = &s5k3l1yx_sensor_i2c_client,
	.sensor_i2c_addr = 0x6E,
	.sensor_output_reg_addr = &s5k3l1yx_reg_addr,
	.sensor_id_info = &s5k3l1yx_id_info,
	.sensor_exp_gain_info = &s5k3l1yx_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &s5k3l1yx_csi_params_array[0],
	.msm_sensor_mutex = &s5k3l1yx_mut,
	.sensor_i2c_driver = &s5k3l1yx_i2c_driver,
	.sensor_v4l2_subdev_info = s5k3l1yx_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k3l1yx_subdev_info),
	.sensor_v4l2_subdev_ops = &s5k3l1yx_subdev_ops,
	.func_tbl = &s5k3l1yx_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Samsung 12MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");
