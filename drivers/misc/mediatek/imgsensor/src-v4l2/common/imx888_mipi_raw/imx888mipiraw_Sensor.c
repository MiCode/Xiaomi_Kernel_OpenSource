// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 imx888mipiraw_Sensor.c
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
#include "imx888mipiraw_Sensor.h"

// static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static void imx888_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void imx888_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt);

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, imx888_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, imx888_seamless_switch},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	.i4OffsetX = 0,
	.i4OffsetY = 0,
	.i4PitchX = 0,
	.i4PitchY = 0,
	.i4PairNum = 0,
	.i4SubBlkW = 0,
	.i4SubBlkH = 0,
	.i4PosL = {{0, 0} },
	.i4PosR = {{0, 0} },
	.i4BlockNumX = 0,
	.i4BlockNumY = 0,
	.i4LeFirst = 0,
	.i4Crop = {
		{0, 0}, {0, 0}, {0, 384}, {0, 384}, {0, 0},
		{0, 0}, {0, 384}, {0, 0}, {0, 384}, {0, 384}
	},
	.iMirrorFlip = 3,
};

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x0fa0,
			.vsize = 0x02ee,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x0fa0,
			.vsize = 0x02ee,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0240,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x0fa0,
			.vsize = 0x02ee,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x0fa0,
			.vsize = 0x02ee,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
//	{
//		.bus.csi2 = {
//			.channel = 0,
//			.data_type = 0x31,
//			.hsize = 0x1000,
//			.vsize = 0x0240,
//			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW12,
//			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
//			.valid_bit = 10,
//		},
//	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2d,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
//	{
//		.bus.csi2 = {
//			.channel = 0,
//			.data_type = 0x32,
//			.hsize = 0x1000,
//			.vsize = 0x0240,
//			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW14,
//			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
//			.valid_bit = 10,
//		},
//	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0240,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0240,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1000,
			.vsize = 0x0900,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 0x1000,
			.vsize = 0x0240,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x31,
			.hsize = 0x0fa0,
			.vsize = 0x02ee,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW12,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.valid_bit = 10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus6[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x31,
			.hsize = 0x0fa0,
			.vsize = 0x02ee,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW12,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.valid_bit = 10,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus7[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x31,
			.hsize = 0x0fa0,
			.vsize = 0x02ee,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW12,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
			.valid_bit = 10,
		},
	},
};

//1000 base for dcg gain ratio
static u32 imx888_dcg_ratio_table_cus1[] = {4000};

static u32 imx888_dcg_ratio_table_cus2[] = {16000};

static struct mtk_sensor_saturation_info imgsensor_saturation_info = {
	.gain_ratio = 1000,
	.OB_pedestal = 64,
	.saturation_level = 1023,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus1 = {
	.gain_ratio = 4000,
	.OB_pedestal = 64,
	.saturation_level = 3900,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus2 = {
	.gain_ratio = 16000,
	.OB_pedestal = 64,
	.saturation_level = 15408,
};

static struct subdrv_mode_struct mode_struct[] = {
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = imx888_preview_setting,
		.mode_setting_len = ARRAY_SIZE(imx888_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 1824000000,
		.linelength = 13984,
		.framelength = 4344,
		.max_framerate = 300,
		.mipi_pixel_rate = 2139430000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 12,
		.imgsensor_winsize_info = {
			.full_w = 8704,
			.full_h = 6016,
			.x0_offset = 352,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6016,
			.scale_w = 4000,
			.scale_h = 3008,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 755,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 63,
		},
	},
	{
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = imx888_capture_setting,
		.mode_setting_len = ARRAY_SIZE(imx888_capture_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 1824000000,
		.linelength = 13984,
		.framelength = 4344,
		.max_framerate = 300,
		.mipi_pixel_rate = 2139430000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 12,
		.imgsensor_winsize_info = {
			.full_w = 8704,
			.full_h = 6016,
			.x0_offset = 352,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6016,
			.scale_w = 4000,
			.scale_h = 3008,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 755,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 63,
		},
	},
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = imx888_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(imx888_normal_video_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx888_seamless_normal_video,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx888_seamless_normal_video),
		.hdr_group = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 1968000000,
		.linelength = 15104,
		.framelength = 4340,
		.max_framerate = 300,
		.mipi_pixel_rate = 2443890000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 12,
		.imgsensor_winsize_info = {
			.full_w = 8704,
			.full_h = 6016,
			.x0_offset = 0,
			.y0_offset = 704,
			.w0_size = 8704,
			.h0_size = 4608,
			.scale_w = 4352,
			.scale_h = 2304,
			.x1_offset = 128,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 699,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 63,
		},
	},
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = imx888_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(imx888_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 1824000000,
		.linelength = 13984,
		.framelength = 4344,
		.max_framerate = 300,
		.mipi_pixel_rate = 2139430000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 12,
		.imgsensor_winsize_info = {
			.full_w = 8704,
			.full_h = 6016,
			.x0_offset = 352,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6016,
			.scale_w = 4000,
			.scale_h = 3008,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 755,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 63,
		},
	},
	{
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = imx888_slim_video_setting,
		.mode_setting_len = ARRAY_SIZE(imx888_slim_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 1824000000,
		.linelength = 13984,
		.framelength = 4344,
		.max_framerate = 300,
		.mipi_pixel_rate = 2139430000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 12,
		.imgsensor_winsize_info = {
			.full_w = 8704,
			.full_h = 6016,
			.x0_offset = 352,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6016,
			.scale_w = 4000,
			.scale_h = 3008,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 755,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 63,
		},
	},
	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = imx888_custom1_setting,
		.mode_setting_len = ARRAY_SIZE(imx888_custom1_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx888_seamless_custom1,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx888_seamless_custom1),
		.hdr_group = 1,
		.hdr_mode = HDR_RAW_DCG_COMPOSE_RAW12,
		.pclk = 1968000000,
		.linelength = 22752,
		.framelength = 2882,
		.max_framerate = 300,
		.mipi_pixel_rate = 2036570000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w = 8704,
			.full_h = 6016,
			.x0_offset = 0,
			.y0_offset = 704,
			.w0_size = 8704,
			.h0_size = 4608,
			.scale_w = 4352,
			.scale_h = 2304,
			.x1_offset = 128,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1041,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 63,
		},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_B,
		.saturation_info = &imgsensor_saturation_info_cus1,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx888_dcg_ratio_table_cus1,
			.dcg_gain_table_size = sizeof(imx888_dcg_ratio_table_cus1),
		},
	},
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = imx888_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(imx888_custom2_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = imx888_seamless_custom2,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx888_seamless_custom2),
		.hdr_group = 1,
		.hdr_mode = HDR_RAW_DCG_COMPOSE_RAW14,
		.pclk = 1968000000,
		.linelength = 22752,
		.framelength = 2882,
		.max_framerate = 300,
		.mipi_pixel_rate = 1745630000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w = 8704,
			.full_h = 6016,
			.x0_offset = 0,
			.y0_offset = 704,
			.w0_size = 8704,
			.h0_size = 4608,
			.scale_w = 4352,
			.scale_h = 2304,
			.x1_offset = 128,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1041,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 63,
		},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW14_B,
		.saturation_info = &imgsensor_saturation_info_cus2,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 16000,
			.dcg_gain_ratio_max = 16000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx888_dcg_ratio_table_cus2,
			.dcg_gain_table_size = sizeof(imx888_dcg_ratio_table_cus2),
		},
	},
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = imx888_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(imx888_custom3_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = imx888_seamless_custom3,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx888_seamless_custom3),
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE, // LBMF Auto mode
		.pclk = 1968000000,
		.linelength = 7552,
		.framelength = 8684,
		.max_framerate = 300,
		.mipi_pixel_rate = 2443890000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 12,
		.imgsensor_winsize_info = {
			.full_w = 8704,
			.full_h = 6016,
			.x0_offset = 0,
			.y0_offset = 704,
			.w0_size = 8704,
			.h0_size = 4608,
			.scale_w = 4352,
			.scale_h = 2304,
			.x1_offset = 128,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1398,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 63,
		},
	},
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = imx888_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(imx888_custom4_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = imx888_seamless_custom4,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx888_seamless_custom4),
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE, // LBMF Manual mode
		.pclk = 1968000000,
		.linelength = 7552,
		.framelength = 8684,
		.max_framerate = 300,
		.mipi_pixel_rate = 2443890000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 4,
		.coarse_integ_step = 4,
		.min_exposure_line = 12,
		.imgsensor_winsize_info = {
			.full_w = 8704,
			.full_h = 6016,
			.x0_offset = 0,
			.y0_offset = 704,
			.w0_size = 8704,
			.h0_size = 4608,
			.scale_w = 4352,
			.scale_h = 2304,
			.x1_offset = 128,
			.y1_offset = 0,
			.w1_size = 4096,
			.h1_size = 2304,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4096,
			.h2_tg_size = 2304,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1398,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 63,
		},
	},
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = imx888_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(imx888_custom5_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx888_seamless_custom5,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx888_seamless_custom5),
		.hdr_group = 2,
		.hdr_mode = PARAM_UNDEFINED,
		.pclk = 2128000000,
		.linelength = 15392,
		.framelength = 4608,
		.max_framerate = 300,
		.mipi_pixel_rate = 1782860000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w = 8704,
			.full_h = 6016,
			.x0_offset = 352,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6016,
			.scale_w = 4000,
			.scale_h = 3008,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1041,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 63,
		},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_B,
	},
	{
		.frame_desc = frame_desc_cus6,
		.num_entries = ARRAY_SIZE(frame_desc_cus6),
		.mode_setting_table = imx888_custom6_setting,
		.mode_setting_len = ARRAY_SIZE(imx888_custom6_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx888_seamless_custom6,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx888_seamless_custom6),
		.hdr_group = 2,
		.hdr_mode = PARAM_UNDEFINED,
		.pclk = 2128000000,
		.linelength = 15072,
		.framelength = 4704,
		.max_framerate = 300,
		.mipi_pixel_rate = 1782860000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w = 8704,
			.full_h = 6016,
			.x0_offset = 2352,
			.y0_offset = 1508,
			.w0_size = 4000,
			.h0_size = 3000,
			.scale_w = 4000,
			.scale_h = 3000,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1041,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 63,
		},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
	},
	{
		.frame_desc = frame_desc_cus7,
		.num_entries = ARRAY_SIZE(frame_desc_cus7),
		.mode_setting_table = imx888_custom7_setting,
		.mode_setting_len = ARRAY_SIZE(imx888_custom7_setting),
		.seamless_switch_group = 2,
		.seamless_switch_mode_setting_table = imx888_seamless_custom7,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(imx888_seamless_custom7),
		.hdr_group = 2,
		.hdr_mode = HDR_RAW_DCG_COMPOSE_RAW12,
		.pclk = 2128000000,
		.linelength = 22752,
		.framelength = 3116,
		.max_framerate = 300,
		.mipi_pixel_rate = 1782860000,
		.readout_length = 0,
		.read_margin = 24,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w = 8704,
			.full_h = 6016,
			.x0_offset = 352,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6016,
			.scale_w = 4000,
			.scale_h = 3008,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 4000,
			.h1_size = 3000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 3000,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1000,
		.fine_integ_line = 1041,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 63,
		},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_B,
		.saturation_info = &imgsensor_saturation_info_cus1,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_ratio_min = 4000,
			.dcg_gain_ratio_max = 4000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = imx888_dcg_ratio_table_cus1,
			.dcg_gain_table_size = sizeof(imx888_dcg_ratio_table_cus1),
		},
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = IMX888_SENSOR_ID,
	.reg_addr_sensor_id = {0x0016, 0x0017},
	.i2c_addr_table = {0x34, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	// .eeprom_info = eeprom_info,
	// .eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {8704, 6016},
	.mirror = IMAGE_HV_MIRROR,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 128,
	.ana_gain_type = 0,
	.ana_gain_step = 1,
	.ana_gain_table = imx888_ana_gain_table,
	.ana_gain_table_size = sizeof(imx888_ana_gain_table),
	.min_gain_iso = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 12,
	.exposure_max = 128 * (0xFFFC - 76),
	.exposure_step = 4,
	.exposure_margin = 76,
	.dig_gain_min = BASE_DGAIN * 1,
	.dig_gain_max = BASE_DGAIN * 16,
	.dig_gain_step = 4,
	.saturation_info = &imgsensor_saturation_info,

	.frame_length_max = 0xFFFC,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 3,
	.start_exposure_offset = 3000000, // ?

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD,
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL|HDR_SUPPORT_DCG|HDR_SUPPORT_LBMF,
	.seamless_switch_support = TRUE,
	.temperature_support = TRUE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	// .s_cali = set_sensor_cali,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {
			{0x0202, 0x0203},
			{0x3162, 0x3163},
			{0x0224, 0x0225},
	},
	.long_exposure_support = TRUE,
	.reg_addr_exposure_lshift = 0x3150,
	.reg_addr_ana_gain = {
			{0x0204, 0x0205},
			{0x3164, 0x3165},
			{0x0216, 0x0217},
	},
	.reg_addr_dig_gain = {
			{0x020E, 0x020F},
			{0x3166, 0x3167},
			{0x0218, 0x0219},
	},
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_en = 0x0138,
	.reg_addr_temp_read = 0x013A,
	.reg_addr_auto_extend = 0x0350,
	.reg_addr_frame_count = 0x0005,
	.reg_addr_fast_mode = 0x3010,
	.reg_addr_dcg_ratio = 0x3172,

	.init_setting_table = imx888_init_setting,
	.init_setting_len = ARRAY_SIZE(imx888_init_setting),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 1, // ?
	.chk_s_off_end = 0, // ?

	.checksum_value = 0xAF3E324F, //?
};

static struct subdrv_ops ops = {
	.get_id = common_get_imgsensor_id,
	.init_ctx = init_ctx,
	.open = common_open,
	.get_info = common_get_info,
	.get_resolution = common_get_resolution,
	.control = common_control,
	.feature_control = common_feature_control,
	.close = common_close,
	.get_frame_desc = common_get_frame_desc,
	.get_temp = common_get_temp,
	.get_csi_param = common_get_csi_param,
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK, 24, 0},
	{HW_ID_PDN, 0, 0},
	{HW_ID_RST, 0, 1},
	{HW_ID_AVDD2, 1800000, 0}, // power 1.8V to enable 2.8V ldo
	{HW_ID_AVDD, 2800000, 3},
	{HW_ID_AVDD1, 2000000, 3},
	{HW_ID_AFVDD1, 1800000, 0}, // power 1.8V to enable 2.8V ldo
	{HW_ID_AFVDD, 2800000, 3},
	{HW_ID_DVDD1, 810000, 4},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 6},
	{HW_ID_PDN, 1, 0},
	{HW_ID_RST, 1, 5}
};

const struct subdrv_entry imx888_mipi_raw_entry = {
	.name = "imx888_mipi_raw",
	.id = IMX888_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION */

static int get_sensor_temperature(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	u8 temperature = 0;
	int temperature_convert = 0;

	temperature = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_temp_read);

	if (temperature <= 0x54)
		temperature_convert = temperature;
	else if (temperature >= 0x55 && temperature <= 0x7F)
		temperature_convert = 85;
	else if (temperature >= 0x81 && temperature <= 0xEC)
		temperature_convert = -20;
	else
		temperature_convert = (char)temperature | 0xFFFFFF00;

	DRV_LOG(ctx, "temperature: %d degrees\n", temperature_convert);
	return temperature_convert;
}

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
	return (16384 - (16384 * BASEGAIN) / gain);
}

static void imx888_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	u32 *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;

	if (feature_data == NULL) {
		DRV_LOGE(ctx, "input scenario is null!");
		return;
	}
	scenario_id = *feature_data;
	if ((feature_data + 1) != NULL)
		ae_ctrl = (u32 *)((uintptr_t)(*(feature_data + 1)));
	else
		DRV_LOGE(ctx, "no ae_ctrl input");

	check_current_scenario_id_bound(ctx);
	DRV_LOG(ctx, "E: set seamless switch %u %u\n", ctx->current_scenario_id, scenario_id);
	if (!ctx->extend_frame_length_en)
		DRV_LOGE(ctx, "please extend_frame_length before seamless_switch!\n");
	ctx->extend_frame_length_en = FALSE;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOGE(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	if (ctx->s_ctx.mode[scenario_id].seamless_switch_group == 0 ||
		ctx->s_ctx.mode[scenario_id].seamless_switch_group !=
			ctx->s_ctx.mode[ctx->current_scenario_id].seamless_switch_group) {
		DRV_LOGE(ctx, "seamless_switch not supported\n");
		return;
	}
	if (ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table == NULL) {
		DRV_LOGE(ctx, "Please implement seamless_switch setting\n");
		return;
	}

	ctx->is_seamless = TRUE;
	update_mode_info(ctx, scenario_id);

	subdrv_i2c_wr_u8(ctx, 0x0104, 0x01);
	subdrv_i2c_wr_u8(ctx, 0x3010, 0x02);
	subdrv_i2c_wr_u8(ctx, 0x3247, 0x04);// enable lbmf fast mode
	i2c_table_write(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);

	if (ae_ctrl) {
		switch (ctx->s_ctx.mode[scenario_id].hdr_mode) {
		case HDR_RAW_STAGGER_2EXP:
			set_multi_shutter_frame_length(ctx, ae_ctrl, 2, 0);
			set_multi_gain(ctx, ae_ctrl + 5, 2);
			break;
		case HDR_RAW_STAGGER_3EXP:
			set_multi_shutter_frame_length(ctx, ae_ctrl, 3, 0);
			set_multi_gain(ctx, ae_ctrl + 5, 3);
			break;
		default:
			set_shutter(ctx, *ae_ctrl);
			set_gain(ctx, *(ae_ctrl + 5));
			break;
		}
	}
	subdrv_i2c_wr_u8(ctx, 0x0104, 0x00);

	ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->is_seamless = FALSE;
	DRV_LOG(ctx, "X: set seamless switch done\n");
}

static void imx888_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	switch (mode) {
	case 5:
		subdrv_i2c_wr_u8(ctx, 0x020E, 0x00); /* dig_gain = 0 */
		break;
	default:
		subdrv_i2c_wr_u8(ctx, 0x0601, mode);
		break;
	}

	if ((ctx->test_pattern) && (mode != ctx->test_pattern)) {
		if (ctx->test_pattern == 5)
			subdrv_i2c_wr_u8(ctx, 0x020E, 0x01);
		else if (mode == 0)
			subdrv_i2c_wr_u8(ctx, 0x0601, 0x00); /* No pattern */
	}

	ctx->test_pattern = mode;
}


static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id)
{
	memcpy(&(ctx->s_ctx), &static_ctx, sizeof(struct subdrv_static_ctx));
	subdrv_ctx_init(ctx);
	ctx->i2c_client = i2c_client;
	ctx->i2c_write_id = i2c_write_id;
	return 0;
}

static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt)
{
	// u8 lbmf_lut_ctl = 0;

	DRV_LOG(ctx, "sof_cnt(%u) ctx->ref_sof_cnt(%u) ctx->fast_mode_on(%d)",
		sof_cnt, ctx->ref_sof_cnt, ctx->fast_mode_on);
	if (ctx->fast_mode_on && (sof_cnt > ctx->ref_sof_cnt)) {
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
		DRV_LOG(ctx, "seamless_switch disabled.");
		set_i2c_buffer(ctx, 0x3010, 0x00);
		commit_i2c_buffer(ctx);
	}
	return 0;
}
