// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5k3m5sxmipiraw_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/
#include "s5k3m5sxmipiraw_Sensor.h"

static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static void s5k3m5sx_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void s5k3m5sx_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static void s5k3m5sx_sensor_init(struct subdrv_ctx *ctx);
static int open(struct subdrv_ctx *ctx);

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, s5k3m5sx_set_test_pattern},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, s5k3m5sx_set_test_pattern_data},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 24,
	.i4OffsetY = 24,
	.i4PitchX = 32,
	.i4PitchY = 32,
	.i4PairNum = 16,
	.i4SubBlkW = 8,
	.i4SubBlkH = 8,
	.i4PosL = {
		{26, 25}, {34, 25}, {42, 25}, {50, 25},
		{30, 37}, {38, 37}, {46, 37}, {54, 37},
		{26, 45}, {34, 45}, {42, 45}, {50, 45},
		{30, 49}, {38, 49}, {46, 49}, {54, 49}
	},
	.i4PosR = {
		{26, 29}, {34, 29}, {42, 29}, {50, 29},
		{30, 33}, {38, 33}, {46, 33}, {54, 33},
		{26, 41}, {34, 41}, {42, 41}, {50, 41},
		{30, 53}, {38, 53}, {46, 53}, {54, 53}
	},
	.i4BlockNumX = 130,
	.i4BlockNumY = 96,
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_binning = {
	.i4OffsetX = 12,
	.i4OffsetY = 12,
	.i4PitchX = 16,
	.i4PitchY = 16,
	.i4PairNum = 16,
	.i4SubBlkW = 4,
	.i4SubBlkH = 4,
	.i4PosL = {
		{12, 13}, {16, 13}, {20, 13}, {24, 13},
		{14, 19}, {18, 19}, {22, 19}, {26, 19},
		{12, 23}, {16, 23}, {20, 23}, {24, 23},
		{14, 25}, {18, 25}, {22, 25}, {26, 25}
	},
	.i4PosR = {
		{12, 15}, {16, 15}, {20, 15}, {24, 15},
		{14, 17}, {18, 17}, {22, 17}, {26, 17},
		{12, 21}, {16, 21}, {20, 21}, {24, 21},
		{14, 27}, {18, 27}, {22, 27}, {26, 27}
	},
	.i4BlockNumX = 130,
	.i4BlockNumY = 96,
};

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0838,
			.vsize = 0x0618,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x0208,
			.vsize = 0x0300,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1070,
			.vsize = 0x0c30,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x0208,
			.vsize = 0x0300,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0838,
			.vsize = 0x0618,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x0208,
			.vsize = 0x0300,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0838,
			.vsize = 0x0618,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0838,
			.vsize = 0x0618,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1070,
			.vsize = 0x0c30,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0838,
			.vsize = 0x0618,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0838,
			.vsize = 0x0618,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0838,
			.vsize = 0x0618,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0838,
			.vsize = 0x0618,
		},
	},
};

static struct subdrv_mode_struct mode_struct[] = {
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = preview_setting_array,
		.mode_setting_len = ARRAY_SIZE(preview_setting_array),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.max_framerate = 300,
		.mipi_pixel_rate = 316800000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4208,
			.h0_size = 3120,
			.scale_w = 2104,
			.scale_h = 1560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2104,
			.h1_size = 1560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2104,
			.h2_tg_size = 1560,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_binning,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = capture_setting_array,
		.mode_setting_len = ARRAY_SIZE(capture_setting_array),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 482000000,
		.linelength = 4848,
		.framelength = 3314,
		.max_framerate = 300,
		.mipi_pixel_rate = 576000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4208,
			.h0_size = 3120,
			.scale_w = 4208,
			.scale_h = 3120,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4208,
			.h1_size = 3120,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4208,
			.h2_tg_size = 3120,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = normal_video_setting_array,
		.mode_setting_len = ARRAY_SIZE(normal_video_setting_array),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.max_framerate = 300,
		.mipi_pixel_rate = 316800000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4208,
			.h0_size = 3120,
			.scale_w = 2104,
			.scale_h = 1560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2104,
			.h1_size = 1560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2104,
			.h2_tg_size = 1560,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_binning,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = hs_video_setting_array,
		.mode_setting_len = ARRAY_SIZE(hs_video_setting_array),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.max_framerate = 300,
		.mipi_pixel_rate = 316800000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4208,
			.h0_size = 3120,
			.scale_w = 2104,
			.scale_h = 1560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2104,
			.h1_size = 1560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2104,
			.h2_tg_size = 1560,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = slim_video_setting_array,
		.mode_setting_len = ARRAY_SIZE(slim_video_setting_array),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.max_framerate = 300,
		.mipi_pixel_rate = 316800000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4208,
			.h0_size = 3120,
			.scale_w = 2104,
			.scale_h = 1560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2104,
			.h1_size = 1560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2104,
			.h2_tg_size = 1560,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = custom1_setting_array,
		.mode_setting_len = ARRAY_SIZE(custom1_setting_array),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 482000000,
		.linelength = 5904,
		.framelength = 3400,
		.max_framerate = 300,
		.mipi_pixel_rate = 473600000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4208,
			.h0_size = 3120,
			.scale_w = 4208,
			.scale_h = 3120,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4208,
			.h1_size = 3120,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4208,
			.h2_tg_size = 3120,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = custom2_setting_array,
		.mode_setting_len = ARRAY_SIZE(custom2_setting_array),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 482000000,
		.linelength = 4848,
		.framelength = 1656,
		.max_framerate = 600,
		.mipi_pixel_rate = 576000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4208,
			.h0_size = 3120,
			.scale_w = 2104,
			.scale_h = 1560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2104,
			.h1_size = 1560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2104,
			.h2_tg_size = 1560,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = custom3_setting_array,
		.mode_setting_len = ARRAY_SIZE(custom3_setting_array),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.max_framerate = 300,
		.mipi_pixel_rate = 316800000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4208,
			.h0_size = 3120,
			.scale_w = 2104,
			.scale_h = 1560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2104,
			.h1_size = 1560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2104,
			.h2_tg_size = 1560,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = custom4_setting_array,
		.mode_setting_len = ARRAY_SIZE(custom4_setting_array),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.max_framerate = 300,
		.mipi_pixel_rate = 316800000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4208,
			.h0_size = 3120,
			.scale_w = 2104,
			.scale_h = 1560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2104,
			.h1_size = 1560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2104,
			.h2_tg_size = 1560,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = custom5_setting_array,
		.mode_setting_len = ARRAY_SIZE(custom5_setting_array),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 482000000,
		.linelength = 8816,
		.framelength = 1816,
		.max_framerate = 300,
		.mipi_pixel_rate = 316800000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 4208,
			.h0_size = 3120,
			.scale_w = 2104,
			.scale_h = 1560,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 2104,
			.h1_size = 1560,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2104,
			.h2_tg_size = 1560,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = S5K3M5SX_SENSOR_ID,
	.reg_addr_sensor_id = {0x0000, 0x0001},
	.i2c_addr_table = {0x20, 0x5A, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_16,
	.eeprom_info = PARAM_UNDEFINED,
	.eeprom_num = PARAM_UNDEFINED,
	.resolution = {4208, 3120},
	.mirror = IMAGE_HV_MIRROR,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 16,
	.ana_gain_type = 2,
	.ana_gain_step = 32,
	.ana_gain_table = s5k3m5sx_ana_gain_table,
	.ana_gain_table_size = sizeof(s5k3m5sx_ana_gain_table),
	.min_gain_iso = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max = 0xFFFF - 4,
	.exposure_step = 1,
	.exposure_margin = 4,

	.frame_length_max = 0xFFFF,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 3000000,

	.pdaf_type = PDAF_SUPPORT_CAMSV,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = FALSE,
	.temperature_support = FALSE,
	.g_temp = PARAM_UNDEFINED,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {{0x0202, 0x0203},},
	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {{0x0204, 0x0205},},
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = PARAM_UNDEFINED,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = 0x0005,

	.init_setting_table = PARAM_UNDEFINED,
	.init_setting_len = PARAM_UNDEFINED,
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 1,
	.chk_s_off_end = 0,

	.checksum_value = 0xE4087030,
};

static struct subdrv_ops ops = {
	.get_id = common_get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = common_close,
	.get_frame_desc = common_get_frame_desc,
	.get_csi_param = common_get_csi_param,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK, 24, 0},
	{HW_ID_RST, 0, 1},
	{HW_ID_DVDD, 1100000, 1},
	{HW_ID_AVDD, 2800000, 1},
	{HW_ID_DOVDD, 1800000, 2},
	{HW_ID_RST, 1, 2},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 1},
};

const struct subdrv_entry s5k3m5sx_mipi_raw_entry = {
	.name = "s5k3m5sx_mipi_raw",
	.id = S5K3M5SX_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION */

static void set_group_hold(void *arg, u8 en)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	if (en)
		set_i2c_buffer(ctx, 0x0104, 0x01);
	else
		set_i2c_buffer(ctx, 0x0104, 0x00);
}

static u16 get_gain2reg(u32 gain)
{
	return gain * 32 / BASEGAIN;
}

static void s5k3m5sx_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	if (mode)
		subdrv_i2c_wr_u16(ctx, 0x0600, mode); /*100% Color bar*/
	else if (ctx->test_pattern)
		subdrv_i2c_wr_u16(ctx, 0x0600, 0x0000); /*No pattern*/

	ctx->test_pattern = mode;
}

static void s5k3m5sx_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	struct mtk_test_pattern_data *data = (struct mtk_test_pattern_data *)para;
	u16 R = (data->Channel_R >> 22) & 0x3ff;
	u16 Gr = (data->Channel_Gr >> 22) & 0x3ff;
	u16 Gb = (data->Channel_Gb >> 22) & 0x3ff;
	u16 B = (data->Channel_B >> 22) & 0x3ff;

	subdrv_i2c_wr_u16(ctx, 0x0602, R);
	subdrv_i2c_wr_u16(ctx, 0x0604, Gr);
	subdrv_i2c_wr_u16(ctx, 0x0606, B);
	subdrv_i2c_wr_u16(ctx, 0x0608, Gb);

	DRV_LOG(ctx, "mode(%u) R/Gr/Gb/B = 0x%04x/0x%04x/0x%04x/0x%04x\n",
		ctx->test_pattern, R, Gr, Gb, B);
}

static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;

	return 0;
}

static void s5k3m5sx_sensor_init(struct subdrv_ctx *ctx)
{
	DRV_LOG(ctx, "E\n");
	subdrv_i2c_wr_u16(ctx, 0x6028, 0x4000);
	subdrv_i2c_wr_u16(ctx, 0x0000, 0x0006);
	subdrv_i2c_wr_u16(ctx, 0x0000, 0x30D5);
	subdrv_i2c_wr_u16(ctx, 0x6214, 0x7971);
	subdrv_i2c_wr_u16(ctx, 0x6218, 0x7150);
	mdelay(3);
	subdrv_i2c_wr_u16(ctx, 0x0A02, 0x7800);
	subdrv_i2c_wr_u16(ctx, 0x6028, 0x2000);
	subdrv_i2c_wr_u16(ctx, 0x602A, 0x3EAC);

	subdrv_i2c_wr_p16(ctx, 0x6F12, sensor_init_setting_array1_burst,
		ARRAY_SIZE(sensor_init_setting_array1_burst));
	subdrv_i2c_wr_regs_u16(ctx, sensor_init_setting_array2,
		ARRAY_SIZE(sensor_init_setting_array2));

	/* set pdaf DT to 0x2B */
	subdrv_i2c_wr_u8(ctx, 0x0116, 0x2B);
	DRV_LOG(ctx, "X\n");
}

static int open(struct subdrv_ctx *ctx)
{
	u32 sensor_id = 0;
	u32 scenario_id = 0;

	/* get sensor id */
	if (common_get_imgsensor_id(ctx, &sensor_id) != ERROR_NONE)
		return ERROR_SENSOR_CONNECT_FAIL;

	/* initail setting */
	s5k3m5sx_sensor_init(ctx);

	memset(ctx->exposure, 0, sizeof(ctx->exposure));
	memset(ctx->ana_gain, 0, sizeof(ctx->gain));
	ctx->exposure[0] = ctx->s_ctx.exposure_def;
	ctx->ana_gain[0] = ctx->s_ctx.ana_gain_def;
	ctx->current_scenario_id = scenario_id;
	ctx->pclk = ctx->s_ctx.mode[scenario_id].pclk;
	ctx->line_length = ctx->s_ctx.mode[scenario_id].linelength;
	ctx->frame_length = ctx->s_ctx.mode[scenario_id].framelength;
	ctx->current_fps = 10 * ctx->pclk / ctx->line_length / ctx->frame_length;
	ctx->readout_length = ctx->s_ctx.mode[scenario_id].readout_length;
	ctx->read_margin = ctx->s_ctx.mode[scenario_id].read_margin;
	ctx->min_frame_length = ctx->frame_length;
	ctx->autoflicker_en = FALSE;
	ctx->test_pattern = 0;
	ctx->ihdr_mode = 0;
	ctx->pdaf_mode = 0;
	ctx->hdr_mode = 0;
	ctx->extend_frame_length_en = 0;
	ctx->is_seamless = 0;
	ctx->fast_mode_on = 0;
	ctx->sof_cnt = 0;
	ctx->ref_sof_cnt = 0;
	ctx->is_streaming = 0;

	return ERROR_NONE;
} /* open */
