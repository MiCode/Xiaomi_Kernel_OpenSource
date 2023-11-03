// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 zircons5khp3sunnymipiraw_Sensor.c
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
#include "zircons5khp3sunnymipiraw_Sensor.h"
#define LOG_TAG "[zircons5khp3sunny]"
#define ZIRCONS5KHP3SUNNY_LOG_INF(format, args...) pr_info(LOG_TAG "[%s] " format, __func__, ##args)
#include "sensor-state.h"
extern unsigned int g_sensor_state;
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static void zircons5khp3sunny_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void zircons5khp3sunny_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void zircons5khp3sunny_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void zircons5khp3sunny_set_curr_lens_data(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static void zircons5khp3sunny_sensor_init(struct subdrv_ctx *ctx);
static void zircons5khp3sunny_get_stagger_target_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int open(struct subdrv_ctx *ctx);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt);
static void zircons5khp3sunny_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len);
#define AWB_RED_GAIN_ADDR   0x0D82
#define AWB_GREEN_GAIN_ADDR 0x0D84
#define AWB_BLUE_GAIN_ADDR  0x0D86

#define DEBUG 0
#define TESTPATTERN 0

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, zircons5khp3sunny_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, zircons5khp3sunny_seamless_switch},
	{SENSOR_FEATURE_SET_TEST_PATTERN_DATA, zircons5khp3sunny_set_test_pattern_data},
	{SENSOR_FEATURE_SET_CURR_LENS_DATA, zircons5khp3sunny_set_curr_lens_data},
	{SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO, zircons5khp3sunny_get_stagger_target_scenario},
	{SENSOR_FEATURE_SET_AWB_GAIN, zircons5khp3sunny_set_awb_gain},
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		.header_id = 0x4444A514,
		.addr_header_id = 0x00000001,
		.i2c_write_id = 0xA2,

		.xtalk_support = TRUE,
		.xtalk_size = 2048,
		.addr_xtalk = 0x150F,
	},
};
static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info[] = {
	{	.i4OffsetX = 0,
		.i4OffsetY = 0,
		.i4PitchX  = 0,
		.i4PitchY  = 0,
		.i4PairNum = 0,
		.i4SubBlkW = 0,
		.i4SubBlkH = 0,
		.i4PosL    = {{0, 0} },
		.i4PosR    = {{0, 0} },
		.i4BlockNumX = 0,
		.i4BlockNumY = 0,
		.i4LeFirst   = 0,
		.i4FullRawW = 4080,
		.i4FullRawH = 3060,
		.i4ModeIndex = 3,
		.sPDMapInfo[0] = {
			.i4PDPattern = 1,
			.i4BinFacX = 2,
			.i4BinFacY = 4,
			.i4PDOrder = {1},
		},
		.i4Crop = {
			{0, 0}, {0, 0}, {0, 382}, {60, 228}, {0, 0},
			{0, 0},{60, 228},{0, 382},{0, 24},{0, 0},
			{2040, 1542},{6120, 4614},{0, 0}, {0, 382}, {0, 382}
		},
		.iMirrorFlip = IMAGE_NORMAL,
	},
	{	.i4OffsetX = 0,
		.i4OffsetY = 0,
		.i4PitchX  = 0,
		.i4PitchY  = 0,
		.i4PairNum = 0,
		.i4SubBlkW = 0,
		.i4SubBlkH = 0,
		.i4PosL    = {{0, 0} },
		.i4PosR    = {{0, 0} },
		.i4BlockNumX = 0,
		.i4BlockNumY = 0,
		.i4LeFirst   = 0,
		.i4FullRawW = 2040,
		.i4FullRawH = 1536,
		.i4ModeIndex = 3,
		.sPDMapInfo[0] = {
			.i4PDPattern = 1,
			.i4BinFacX = 2,
			.i4BinFacY = 4,
			.i4PDOrder = {1},
		},
		.i4Crop = {
			{0, 0}, {0, 0}, {0, 382}, {60, 228}, {0, 0},
			{0, 0},{60, 228},{0, 382},{0, 24},{0, 0},
			{2040, 1542},{6120, 4614},{0, 0}, {0, 382}, {0, 382}
		},
		.iMirrorFlip = IMAGE_NORMAL,
	},
	{	.i4OffsetX = 0,
		.i4OffsetY = 0,
		.i4PitchX  = 0,
		.i4PitchY  = 0,
		.i4PairNum = 0,
		.i4SubBlkW = 0,
		.i4SubBlkH = 0,
		.i4PosL    = {{0, 0} },
		.i4PosR    = {{0, 0} },
		.i4BlockNumX = 0,
		.i4BlockNumY = 0,
		.i4LeFirst   = 0,
		.i4FullRawW = 8160,
		.i4FullRawH = 6144,
		.i4ModeIndex = 3,
		.sPDMapInfo[0] = {
			.i4PDPattern = 1,
			.i4BinFacX = 4,
			.i4BinFacY = 8,
			.i4PDOrder = {1},
		},
		.i4Crop = {
			{0, 0}, {0, 0}, {0, 382}, {60, 228}, {0, 0},
			{0, 0},{60, 228},{0, 382},{0, 24},{0, 0},
			{2040, 1542},{6120, 4614},{0, 0}, {0, 382}, {0, 382}
		},
		.iMirrorFlip = IMAGE_NORMAL,
	},
	{	.i4OffsetX = 0,
		.i4OffsetY = 0,
		.i4PitchX  = 0,
		.i4PitchY  = 0,
		.i4PairNum = 0,
		.i4SubBlkW = 0,
		.i4SubBlkH = 0,
		.i4PosL    = {{0, 0} },
		.i4PosR    = {{0, 0} },
		.i4BlockNumX = 0,
		.i4BlockNumY = 0,
		.i4LeFirst   = 0,
		.i4FullRawW = 16320,
		.i4FullRawH = 12288,
		.i4ModeIndex = 3,
		.sPDMapInfo[0] = {
			.i4PDPattern = 1,
			.i4BinFacX = 8,
			.i4BinFacY = 16,
			.i4PDOrder = {1},
		},
		.i4Crop = {
			{0, 0}, {0, 0}, {0, 382}, {60, 228}, {0, 0},
			{0, 0},{60, 228},{0, 382},{0, 24},{0, 0},
			{2040, 1542},{6120, 4614},{0, 0}, {0, 382}, {0, 382}
		},
		.iMirrorFlip = IMAGE_NORMAL,
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0ff0,
			.vsize = 0x0bf4,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 4080,
			.vsize = 764,
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
			.hsize = 4080,
			.vsize = 2296,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 4080,
			.vsize = 572,
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
			.hsize = 1920,
			.vsize = 1080,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 1920,
			.vsize = 268,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4080,
			.vsize = 2296,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 4080,
			.vsize = 572,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

// 200M
static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x3FC0,
			.vsize = 0x2FD0,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

//idcg ln2
static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x0ff0,
			.vsize = 0x0bf4,
			// .user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 4080,
			.vsize = 764,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
//idcg 2x
static struct mtk_mbus_frame_desc_entry frame_desc_cus6[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x0ff0,
			.vsize = 0x0bf4,
			// .user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 2032,
			.vsize = 380,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
//idcg 4x
static struct mtk_mbus_frame_desc_entry frame_desc_cus7[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x0ff0,
			.vsize = 0x0bf4,
			// .user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 1008,
			.vsize = 188,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
//idcg 4x4 idcg
static struct mtk_mbus_frame_desc_entry frame_desc_cus8[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x0ff0,
			.vsize = 0x0bf4,
			// .user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 4080,
			.vsize = 760,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
//idcg video 30fps
static struct mtk_mbus_frame_desc_entry frame_desc_cus9[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x0ff0,
			.vsize = 0x08f8,
			// .user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 4080,
			.vsize = 572,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};
//idcg video 24fps
static struct mtk_mbus_frame_desc_entry frame_desc_cus10[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2c,
			.hsize = 0x0ff0,
			.vsize = 0x08f8,
			// .user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 4080,
			.vsize = 572,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};

//custom11 /*10bit 50M capture 15fps*/
static struct mtk_mbus_frame_desc_entry frame_desc_50mcap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1FE0,
			.vsize = 0x17E8,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 4080,
			.vsize = 764,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
};


//1000 base for dcg gain ratio
static u32 zircons5khp3sunny_dcg_ratio_table_cus8[] = {1000};

static u32 zircons5khp3sunny_dcg_ratio_table_cus9[] = {1000};

static u32 zircons5khp3sunny_dcg_ratio_table_cus10[] = {1000};

static struct mtk_sensor_saturation_info imgsensor_saturation_info = {
	.gain_ratio = 1000,
	.OB_pedestal = 64,
	.saturation_level = 1023,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus5 = {
	.gain_ratio = 1000,
	.OB_pedestal = 256,
	.saturation_level = 4092,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus6 = {
	.gain_ratio = 1000,
	.OB_pedestal = 256,
	.saturation_level = 4092,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus7 = {
	.gain_ratio = 1000,
	.OB_pedestal = 256,
	.saturation_level = 4092,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus8 = {
	.gain_ratio = 1000,
	.OB_pedestal = 256,
	.saturation_level = 4092,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus9 = {
	.gain_ratio = 1000,
	.OB_pedestal = 256,
	.saturation_level = 4092,
};

static struct mtk_sensor_saturation_info imgsensor_saturation_info_cus10 = {
	.gain_ratio = 1000,
	.OB_pedestal = 256,
	.saturation_level = 4092,
};
static struct subdrv_mode_struct mode_struct[] = {
	{// 0.preview /*10bit 12.5M 30fps*/
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = zircons5khp3sunny_preview_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 2000000000,
		.linelength = 15008,
		.framelength = 4434,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 16320,
			.h0_size = 12288,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[0],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 128,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 1.Capture /*10bit 12.5M 30fps*/
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = zircons5khp3sunny_preview_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 2000000000,
		.linelength = 15008,
		.framelength = 4434,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 16320,
			.h0_size = 12288,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[0],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 128,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 2.normal video /*10bit 4080x2296 30fps*/
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = zircons5khp3sunny_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_normal_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 2000000000,
		.linelength = 15008,
		.framelength = 4430,
		.max_framerate = 300,
		.mipi_pixel_rate = 1138176000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 16320,
			.h0_size = 9216,
			.scale_w = 4080,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 4080,
			.h1_size = 2296,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2296,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[0],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 128,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 3.hs video /*10bit 1920x1080 240fps*/
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = zircons5khp3sunny_hs_video_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_hs_video_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 2000000000,
		.linelength = 5952,
		.framelength = 1400,
		.max_framerate = 2400,
		.mipi_pixel_rate = 1586880000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 480,
			.y0_offset = 1792,
			.w0_size = 15360,
			.h0_size = 8704,
			.scale_w = 1920,
			.scale_h = 1088,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[1],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 128,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 4.slim video /*10bit 12.5M 30fps*/
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = zircons5khp3sunny_preview_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 2000000000,
		.linelength = 15008,
		.framelength = 4434,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 16320,
			.h0_size = 12288,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[0],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 128,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 5.custom1 /*10bit 12.5M 30fps*/
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = zircons5khp3sunny_preview_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_preview_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 2000000000,
		.linelength = 15008,
		.framelength = 4434,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 16320,
			.h0_size = 12288,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[0],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 128,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 6.custom2 /*1920x1080 120fps*/
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = zircons5khp3sunny_custom2_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom2_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 2000000000,
		.linelength = 5952,
		.framelength = 2802,
		.max_framerate = 1200,
		.mipi_pixel_rate = 1586880000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 480,
			.y0_offset = 1792,
			.w0_size = 15360,
			.h0_size = 8704,
			.scale_w = 1920,
			.scale_h = 1088,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[1],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 128,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 7.custom3 /*4080x2296 60fps*/
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = zircons5khp3sunny_custom3_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom3_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 2000000000,
		.linelength = 10448,
		.framelength = 3180,
		.max_framerate = 600,
		.mipi_pixel_rate = 1586880000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 16320,
			.h0_size = 9216,
			.scale_w = 4080,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 4080,
			.h1_size = 2296,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2296,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[0],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 128,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 8.custom4 /*200M full size*/
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = zircons5khp3sunny_custom4_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom4_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 2000000000,
		.linelength = 22576,
		.framelength = 12448,
		.max_framerate = 71,
		.mipi_pixel_rate = 1586880000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 24,
			.w0_size = 16320,
			.h0_size = 12240,
			.scale_w = 16320,
			.scale_h = 12240,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 16320,
			.h1_size = 12240,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 16320,
			.h2_tg_size = 12240,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = &imgsensor_pd_info[3],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_Gr,
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 16,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 9.custom5 /*12bit ln2 4080x3060 30fps*/
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = zircons5khp3sunny_custom5_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom5_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = zircons5khp3sunny_custom5_seamless_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom5_seamless_setting),
		.hdr_group = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 2000000000,
		.linelength = 16848,
		.framelength = 3952,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 16320,
			.h0_size = 12288,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[0],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_Gr,
		.saturation_info = &imgsensor_saturation_info_cus5,
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 128,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 10.custom6 /*12bit 2X 4080x3060 30fps*/
		.frame_desc = frame_desc_cus6,
		.num_entries = ARRAY_SIZE(frame_desc_cus6),
		.mode_setting_table = zircons5khp3sunny_custom6_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom6_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = zircons5khp3sunny_custom6_seamless_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom6_seamless_setting),
		.hdr_group = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 1760000000,
		.linelength = 12496,
		.framelength = 4688,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 16,
			.w0_size = 16320,
			.h0_size = 12256,
			.scale_w = 8160,
			.scale_h = 6128,
			.x1_offset = 2040,
			.y1_offset = 1534,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[2],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_Gr,
		.saturation_info = &imgsensor_saturation_info_cus6,
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 64,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 11.custom7 /*12bit 4X 4080x3060 30fps*/
		.frame_desc = frame_desc_cus7,
		.num_entries = ARRAY_SIZE(frame_desc_cus7),
		.mode_setting_table = zircons5khp3sunny_custom7_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom7_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = zircons5khp3sunny_custom7_seamless_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom7_seamless_setting),
		.hdr_group = 1,
		.hdr_mode = HDR_NONE,
		.pclk = 2000000000,
		.linelength = 20320,
		.framelength = 3279,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 6120,
			.y0_offset = 4608,
			.w0_size = 4080,
			.h0_size = 3072,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[3],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_Gr,
		.saturation_info = &imgsensor_saturation_info_cus7,
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 16,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 12.custom8 /*12bit 4X4 idcg 4080x3060 30fps*/
		.frame_desc = frame_desc_cus8,
		.num_entries = ARRAY_SIZE(frame_desc_cus8),
		.mode_setting_table = zircons5khp3sunny_custom8_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom8_setting),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = zircons5khp3sunny_custom8_seamless_setting,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom8_seamless_setting),
		.hdr_group = 1,
		.hdr_mode = HDR_RAW_DCG_COMPOSE_RAW12,
		.pclk = 2000000000,
		.linelength = 16848,
		.framelength = 3952,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 16320,
			.h0_size = 12288,
			.scale_w = 4080,
			.scale_h = 3072,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 4080,
			.h1_size = 3060,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 3060,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[0],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_Gr,
		.saturation_info = &imgsensor_saturation_info_cus8,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_base = IMGSENSOR_DCG_GAIN_LCG_BASE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 1000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = zircons5khp3sunny_dcg_ratio_table_cus8,
			.dcg_gain_table_size = sizeof(zircons5khp3sunny_dcg_ratio_table_cus8),
		},
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 16,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 13.custom9 /*iDCG video 30fps*/
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = zircons5khp3sunny_custom9_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom9_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = 2,
		.hdr_mode = HDR_RAW_DCG_COMPOSE_RAW12,
		.pclk = 2000000000,
		.linelength = 16848,
		.framelength = 3954,
		.max_framerate = 300,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 16320,
			.h0_size = 9216,
			.scale_w = 4080,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 4080,
			.h1_size = 2296,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2296,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[0],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_Gr,
		.saturation_info = &imgsensor_saturation_info_cus9,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_base = IMGSENSOR_DCG_GAIN_LCG_BASE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 1000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = zircons5khp3sunny_dcg_ratio_table_cus9,
			.dcg_gain_table_size = sizeof(zircons5khp3sunny_dcg_ratio_table_cus9),
		},
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 16,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 14.custom10 /*iDCG video 24fps*/
		.frame_desc = frame_desc_cus10,
		.num_entries = ARRAY_SIZE(frame_desc_cus10),
		.mode_setting_table = zircons5khp3sunny_custom10_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom10_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = 2,
		.hdr_mode = HDR_RAW_DCG_COMPOSE_RAW12,
		.pclk = 2000000000,
		.linelength = 16848,
		.framelength = 4944,
		.max_framerate = 240,
		.mipi_pixel_rate = 930240000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 1536,
			.w0_size = 16320,
			.h0_size = 9216,
			.scale_w = 4080,
			.scale_h = 2304,
			.x1_offset = 0,
			.y1_offset = 4,
			.w1_size = 4080,
			.h1_size = 2296,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4080,
			.h2_tg_size = 2296,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info[0],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW12_Gr,
		.saturation_info = &imgsensor_saturation_info_cus10,
		.dcg_info = {
			.dcg_mode = IMGSENSOR_DCG_COMPOSE,
			.dcg_gain_mode = IMGSENSOR_DCG_RATIO_MODE,
			.dcg_gain_base = IMGSENSOR_DCG_GAIN_LCG_BASE,
			.dcg_gain_ratio_min = 1000,
			.dcg_gain_ratio_max = 1000,
			.dcg_gain_ratio_step = 0,
			.dcg_gain_table = zircons5khp3sunny_dcg_ratio_table_cus10,
			.dcg_gain_table_size = sizeof(zircons5khp3sunny_dcg_ratio_table_cus10),
		},
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 16,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},
	{// 15.custom11 /*10bit 50M capture 15fps*/
		.frame_desc = frame_desc_50mcap,
		.num_entries = ARRAY_SIZE(frame_desc_50mcap),
		.mode_setting_table = zircons5khp3sunny_custom11_setting,
		.mode_setting_len = ARRAY_SIZE(zircons5khp3sunny_custom11_setting),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 1760000000,
		.linelength = 11648,
		.framelength = 6268,
		.max_framerate = 240,
		.mipi_pixel_rate = 1586880000,
		.readout_length = 0,
		.read_margin = 37,
		.imgsensor_winsize_info = {
			.full_w = 16320,
			.full_h = 12288,
			.x0_offset = 0,
			.y0_offset = 24,
			.w0_size = 16320,
			.h0_size = 12240,
			.scale_w = 8160,
			.scale_h = 6120,
			.x1_offset = 0,
			.y1_offset = 6,
			.w1_size = 8160,
			.h1_size = 6120,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 8160,
			.h2_tg_size = 6120,
		},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_Gr,
		//.pdaf_cap = TRUE,
		//.imgsensor_pd_info = &imgsensor_pd_info[0],
		.ae_binning_ratio = 1000,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {0},
		.ana_gain_min = BASEGAIN * 1,
		.ana_gain_max = BASEGAIN * 64,
		.dig_gain_min = BASE_DGAIN * 1,
		.dig_gain_max = BASE_DGAIN * 1,
	},

};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = ZIRCONS5KHP3SUNNY_SENSOR_ID,
	.reg_addr_sensor_id = {0x0000, 0x0001},
	.i2c_addr_table = {0x20, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_16,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {16320, 12288},
	.mirror = IMAGE_NORMAL,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_BAYER_Gr,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 16,
	.ana_gain_type = 2,
	.ana_gain_step = 1,
	.ana_gain_table = zircons5khp3sunny_ana_gain_table,
	.ana_gain_table_size = sizeof(zircons5khp3sunny_ana_gain_table),
	.min_gain_iso = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 16,
	.exposure_max = 0xFFFF * 128 - 3,
	.exposure_step = 1,
	.exposure_margin = 74,
	.dig_gain_min = BASE_DGAIN * 1,
	.dig_gain_max = BASE_DGAIN * 1,
	.dig_gain_step = 1,
	.saturation_info = &imgsensor_saturation_info,

	.frame_length_max = 0xFFFF,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 1,

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD,
	.hdr_type = HDR_SUPPORT_STAGGER_FDOL|HDR_SUPPORT_DCG|HDR_SUPPORT_LBMF,
	.seamless_switch_support = TRUE,
	.temperature_support = FALSE,
	.g_temp = PARAM_UNDEFINED,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {{0x0202, 0x0203},},
	.long_exposure_support = TRUE,
	.reg_addr_exposure_lshift = 0x0702,
	.reg_addr_ana_gain = {{0x0204, 0x0205},},
	.reg_addr_dig_gain = {{0x020e, 0x020f},},
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_en = PARAM_UNDEFINED,
	.reg_addr_temp_read = PARAM_UNDEFINED,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = 0x0005,
	.reg_addr_stream_cmd_allowed = 0x000B,
	.reg_addr_change_page_allowed = 0xFCFC,

	.init_setting_table = PARAM_UNDEFINED,
	.init_setting_len = PARAM_UNDEFINED,
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 1,
	.chk_s_off_end = 1,
	.chk_s_sta = 1,

	.checksum_value = 0x47a75476,
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
	.vsync_notify = vsync_notify,
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_AFVDD, 3100000, 1},
	{HW_ID_RST,   0,       1},
	{HW_ID_MCLK,  24,      1},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 5},
	{HW_ID_AVDD,  2200000, 5},
	{HW_ID_DVDD,  1      , 5},
	{HW_ID_DOVDD, 1800000, 5},
	{HW_ID_RST,   1,       7},
};

const struct subdrv_entry zircons5khp3sunny_mipi_raw_entry = {
	.name = "zircons5khp3sunny_mipi_raw",
	.id = ZIRCONS5KHP3SUNNY_SENSOR_ID,
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

static void zircons5khp3sunny_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	enum SENSOR_SCENARIO_ID_ENUM scenario_id;
	u32 *ae_ctrl = NULL;
	u64 *feature_data = (u64 *)para;

#ifdef TRUE
	SHOW_SENSOE_STATE(g_sensor_state, __func__, __LINE__);
#endif
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
	DRV_LOG(ctx, "E: set seamless switch %u to %u size:%u\n", ctx->current_scenario_id, scenario_id, ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);
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

	i2c_table_write(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);

	if (ae_ctrl) {
		set_shutter(ctx, *ae_ctrl);
		set_gain(ctx, *(ae_ctrl + 5));
	}
#ifdef TRUE
	SET_RES_STATE(g_sensor_state, scenario_id, __func__, __LINE__);
	SET_FRAME_STATE(g_sensor_state, 0, __func__, __LINE__);
#endif
	ctx->sensor_mode = scenario_id;
	ctx->fast_mode_on = TRUE;
	ctx->ref_sof_cnt = ctx->sof_cnt;
	ctx->is_seamless = FALSE;
	DRV_LOG(ctx, "X: set seamless switch done\n");
}

static void zircons5khp3sunny_set_curr_lens_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	unsigned short *data = (unsigned short*)para;
	unsigned short lp = *data;
#if DEBUG
	unsigned short addr = 0x0340;
	subdrv_i2c_wr_u16(ctx, 0x602C, 0x4000);
	subdrv_i2c_wr_u16(ctx, 0x602E, addr);//to read addr register
	pr_err("%s read 0x%x val:0x%x", __func__, addr, subdrv_i2c_rd_u16(ctx, 0x6F12));
#endif
	if(ctx->is_streaming == 1){
		subdrv_i2c_wr_u16(ctx, 0xFCFC, 0x2001);
		subdrv_i2c_wr_u16(ctx, 0x7CB4, lp);
		subdrv_i2c_wr_u16(ctx, 0xFCFC, 0x4000);
		DRV_LOG(ctx, "%s lp:0x%x\n", __func__, lp);
	}
}

static void zircons5khp3sunny_set_awb_gain(struct subdrv_ctx *ctx, u8 *para, u32 *len){
	struct SET_SENSOR_AWB_GAIN *awb_gain = (struct SET_SENSOR_AWB_GAIN *)para;
	if(ctx->sensor_mode == 10 || ctx->sensor_mode == 11 || ctx->sensor_mode == 15){
		subdrv_i2c_wr_u16(ctx, 0xFCFC, 0x4000);
		subdrv_i2c_wr_u16(ctx, AWB_RED_GAIN_ADDR, awb_gain->ABS_GAIN_R * 2);
		subdrv_i2c_wr_u16(ctx, AWB_GREEN_GAIN_ADDR, awb_gain->ABS_GAIN_GR * 2);
		subdrv_i2c_wr_u16(ctx, AWB_BLUE_GAIN_ADDR, awb_gain->ABS_GAIN_B * 2);
		DRV_LOG(ctx, "%s awb_r:0x%x awb_g:0x%x awb_b:0x%x\n", __func__, awb_gain->ABS_GAIN_R * 2, awb_gain->ABS_GAIN_GR * 2, awb_gain->ABS_GAIN_B * 2);
	}
}

static void zircons5khp3sunny_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

#if TESTPATTERN
	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
		switch (mode) {
		case 0: // OFF
		case 1: // Solid Color
		case 2: // Color bar
		case 3: // Color bar Fade To Gray
		case 4: // PN9
			subdrv_i2c_wr_u16(ctx, 0x0600, mode);
			break;
		default:
			subdrv_i2c_wr_u16(ctx, 0x0600, 0x0000); /*No pattern*/
			break;
	}
#endif

	ctx->test_pattern = mode;
}

static void zircons5khp3sunny_set_test_pattern_data(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
#if TESTPATTERN
	struct mtk_test_pattern_data *data = (struct mtk_test_pattern_data *)para;
	u16 R = (data->Channel_R >> 22) & 0x3ff;
	u16 Gr = (data->Channel_Gr >> 22) & 0x3ff;
	u16 Gb = (data->Channel_Gb >> 22) & 0x3ff;
	u16 B = (data->Channel_B >> 22) & 0x3ff;

	// subdrv_i2c_wr_u16(ctx, 0x0602, R);
	// subdrv_i2c_wr_u16(ctx, 0x0604, Gr);
	// subdrv_i2c_wr_u16(ctx, 0x0606, B);
	// subdrv_i2c_wr_u16(ctx, 0x0608, Gb);

	subdrv_i2c_wr_u16(ctx, 0x0602, Gr);
	subdrv_i2c_wr_u16(ctx, 0x0604, R);
	subdrv_i2c_wr_u16(ctx, 0x0606, B);
	subdrv_i2c_wr_u16(ctx, 0x0608, Gb);
	
	DRV_LOG_MUST(ctx, "mode(%u) R/Gr/Gb/B = 0x%04x/0x%04x/0x%04x/0x%04x\n",
		ctx->test_pattern, R, Gr, Gb, B);
#endif
}

static void zircons5khp3sunny_get_stagger_target_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len){
	u64 *feature_data = (u64 *)para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = *feature_data;
	enum IMGSENSOR_HDR_MODE_ENUM hdr_mode = *(feature_data + 1);
	u32 *pScenarios = (u32 *)(feature_data + 2);

	if (ctx->s_ctx.hdr_type == HDR_SUPPORT_NA)
		return;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	if ((SENSOR_SCENARIO_ID_NORMAL_PREVIEW == scenario_id) && (hdr_mode == HDR_NONE))
		*pScenarios = SENSOR_SCENARIO_ID_CUSTOM5;
	else if((SENSOR_SCENARIO_ID_NORMAL_PREVIEW == scenario_id) && (hdr_mode == HDR_RAW_DCG_COMPOSE_RAW12))
		*pScenarios = SENSOR_SCENARIO_ID_CUSTOM8;
	else if((SENSOR_SCENARIO_ID_CUSTOM5 == scenario_id) && (hdr_mode == HDR_RAW_DCG_COMPOSE_RAW12))
		*pScenarios = SENSOR_SCENARIO_ID_CUSTOM8;
	else if((SENSOR_SCENARIO_ID_CUSTOM8 == scenario_id) && (hdr_mode == HDR_NONE))
		*pScenarios = SENSOR_SCENARIO_ID_CUSTOM5;
	else
		*pScenarios = 0xff;

	DRV_LOG(ctx, "sid(input/output):%u/%u, hdr_mode:%u\n",
				scenario_id, *pScenarios, hdr_mode);
	/*
        int i = 0;
	u32 group = 0;
        group = ctx->s_ctx.mode[scenario_id].hdr_group;
	for (i = 0; i < ctx->s_ctx.sensor_mode_num; i++) {
		if (group != 0 && i != scenario_id &&
		(ctx->s_ctx.mode[i].hdr_group == group) &&
		(ctx->s_ctx.mode[i].hdr_mode == hdr_mode)) {
			*pScenarios = i;
			DRV_LOG_MUST(ctx, "sid(input/output):%u/%u, hdr_mode:%u\n",
				scenario_id, *pScenarios, hdr_mode);
			break;
		}
	}
	*/
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
	DRV_LOG(ctx, "sof_cnt(%u) ctx->ref_sof_cnt(%u) ctx->fast_mode_on(%d)",
		sof_cnt, ctx->ref_sof_cnt, ctx->fast_mode_on);
#ifdef TRUE
	SET_FRAME_STATE(g_sensor_state, sof_cnt, __func__, __LINE__);
#endif
	if (ctx->fast_mode_on && (sof_cnt > ctx->ref_sof_cnt)) {
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
	}
	return 0;
}

static void zircons5khp3sunny_sensor_init(struct subdrv_ctx *ctx)
{
	DRV_LOG(ctx, "E\n");
	subdrv_i2c_wr_u16(ctx, 0xFCFC,0x4000);
	subdrv_i2c_wr_u16(ctx, 0x0000,0x0009);
	subdrv_i2c_wr_u16(ctx, 0x0000,0x1B73);
	subdrv_i2c_wr_u16(ctx, 0x6012,0x0001);
	subdrv_i2c_wr_u16(ctx, 0x7002,0x0008);
	subdrv_i2c_wr_u16(ctx, 0x6014,0x0001);
	mdelay(10);
	subdrv_i2c_wr_u16(ctx, 0x6214,0xFF7D);
	subdrv_i2c_wr_u16(ctx, 0x6218,0x0000);
	subdrv_i2c_wr_u16(ctx, 0xA100,0x2F06);
	subdrv_i2c_wr_u16(ctx, 0xA102,0x0000);

	i2c_table_write(ctx, zircons5khp3sunny_init_setting, sizeof(zircons5khp3sunny_init_setting)/sizeof(u16));
	subdrv_i2c_wr_regs_u16_burst(ctx, zircons5khp3sunny_init_setting2, sizeof(zircons5khp3sunny_init_setting2)/sizeof(u16));

	subdrv_i2c_wr_u16(ctx, 0xFCFC, 0x4000);
	subdrv_i2c_wr_u16(ctx, 0x6226, 0x0000);
#ifdef TRUE
	SET_INIT_STATE(g_sensor_state, 1, __func__, __LINE__);
#endif
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
	zircons5khp3sunny_sensor_init(ctx);

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
