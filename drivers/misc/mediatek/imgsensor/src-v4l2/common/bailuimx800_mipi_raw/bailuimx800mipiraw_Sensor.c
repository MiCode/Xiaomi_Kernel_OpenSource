// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 bailuimx800mipiraw_Sensor.c
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
#include "bailuimx800mipiraw_Sensor.h"

static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static void bailuimx800_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void bailuimx800_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void bailuimx800_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void bailuimx800_get_stagger_target_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);
static int vsync_notify(struct subdrv_ctx *ctx,	unsigned int sof_cnt);

/* STRUCT  */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, bailuimx800_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, bailuimx800_seamless_switch},
	{SENSOR_FEATURE_CHECK_SENSOR_ID, bailuimx800_check_sensor_id},
	{SENSOR_FEATURE_GET_STAGGER_TARGET_SCENARIO, bailuimx800_get_stagger_target_scenario},
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		.header_id = 0x01470005,
		.addr_header_id = 0x00000006,
		.i2c_write_id = 0xA2,

		.qsc_support = TRUE,
		.qsc_size = 0x0C00,
		.addr_qsc = 0x22C0,
		.sensor_reg_addr_qsc = 0xC800,
	},
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
};

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4096,
			.vsize = 768,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4384,
			.vsize = 2464,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4384,
			.vsize = 616,
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
			.hsize = 2192,
			.vsize = 1232,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2192,
			.vsize = 308,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2192,
			.vsize = 1232,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2192,
			.vsize = 308,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4384,
			.vsize = 2464,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4384,
			.vsize = 616,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 8192,
			.vsize = 6144,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4096,
			.vsize = 3072,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 1536,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus7[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4384,
			.vsize = 2464,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4384,
			.vsize = 616,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus8[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 2048,
			.vsize = 1536,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 2048,
			.vsize = 384,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus9[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4384,
			.vsize = 2464,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 4384,
			.vsize = 2464,
			.user_data_desc = VC_STAGGER_ME,
		},
	},
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x30,
			.hsize = 4384,
			.vsize = 616,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_NE_PIX_1,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x30,
			.hsize = 4384,
			.vsize = 616,
			.dt_remap_to_type = MTK_MBUS_FRAME_DESC_REMAP_TO_RAW10,
			.user_data_desc = VC_PDAF_STATS_ME_PIX_1,
		},
	},

};

static struct subdrv_mode_struct mode_struct[] = {
	{/* 0 preview mode */
		.frame_desc         = frame_desc_prev,
		.num_entries        = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = bailuimx800_preview_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_preview_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 2196000000,
		.linelength        = 19552,
		.framelength       = 3740,
		.max_framerate     = 300,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 288,
			.y0_offset    = 0,
			.w0_size      = 8192,
			.h0_size      = 6144,
			.scale_w      = 4096,
			.scale_h      = 3072,
			.x1_offset    = 0,
			.y1_offset    = 0,
			.w1_size      = 4096,
			.h1_size      = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 4096,
			.h2_tg_size   = 3072,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
	{/* 1 capture mode */
		.frame_desc         = frame_desc_prev,
		.num_entries        = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = bailuimx800_preview_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_preview_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 2196000000,
		.linelength        = 19552,
		.framelength       = 3740,
		.max_framerate     = 300,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 288,
			.y0_offset    = 0,
			.w0_size      = 8192,
			.h0_size      = 6144,
			.scale_w      = 4096,
			.scale_h      = 3072,
			.x1_offset    = 0,
			.y1_offset    = 0,
			.w1_size      = 4096,
			.h1_size      = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 4096,
			.h2_tg_size   = 3072,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
	{/* 2 normal video mode */
		.frame_desc         = frame_desc_vid,
		.num_entries        = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = bailuimx800_normal_video_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_normal_video_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 1728000000,
		.linelength        = 14240,
		.framelength       = 4040,
		.max_framerate     = 300,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 0,
			.y0_offset    = 608,
			.w0_size      = 8768,
			.h0_size      = 4928,
			.scale_w      = 4384,
			.scale_h      = 2464,
			.x1_offset    = 0,
			.y1_offset    = 0,
			.w1_size      = 4384,
			.h1_size      = 2464,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 4384,
			.h2_tg_size   = 2464,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
	{/* 3 hs video mode */
		.frame_desc         = frame_desc_hs_vid,
		.num_entries        = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = bailuimx800_hs_video_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_hs_video_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 2052000000,
		.linelength        = 5520,
		.framelength       = 1548,
		.max_framerate     = 2400,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 0,
			.y0_offset    = 608,
			.w0_size      = 8768,
			.h0_size      = 4928,
			.scale_w      = 2192,
			.scale_h      = 1232,
			.x1_offset    = 0,
			.y1_offset    = 0,
			.w1_size      = 2192,
			.h1_size      = 1232,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 2192,
			.h2_tg_size   = 1232,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
	{/* 4 slim video mode */
		.frame_desc         = frame_desc_prev,
		.num_entries        = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = bailuimx800_preview_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_preview_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 2196000000,
		.linelength        = 19552,
		.framelength       = 3740,
		.max_framerate     = 300,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 288,
			.y0_offset    = 0,
			.w0_size      = 8192,
			.h0_size      = 6144,
			.scale_w      = 4096,
			.scale_h      = 3072,
			.x1_offset    = 0,
			.y1_offset    = 0,
			.w1_size      = 4096,
			.h1_size      = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 4096,
			.h2_tg_size   = 3072,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
	{/* 5 custom1 mode */
		.frame_desc         = frame_desc_prev,
		.num_entries        = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = bailuimx800_preview_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_preview_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 2196000000,
		.linelength        = 19552,
		.framelength       = 3740,
		.max_framerate     = 300,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 288,
			.y0_offset    = 0,
			.w0_size      = 8192,
			.h0_size      = 6144,
			.scale_w      = 4096,
			.scale_h      = 3072,
			.x1_offset    = 0,
			.y1_offset    = 0,
			.w1_size      = 4096,
			.h1_size      = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 4096,
			.h2_tg_size   = 3072,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
	{/* 6 custom2 mode */
		.frame_desc         = frame_desc_cus2,
		.num_entries        = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = bailuimx800_custom2_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_custom2_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 1344000000,
		.linelength        = 5520,
		.framelength       = 2028,
		.max_framerate     = 1200,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 0,
			.y0_offset    = 608,
			.w0_size      = 8768,
			.h0_size      = 4928,
			.scale_w      = 2192,
			.scale_h      = 1232,
			.x1_offset    = 0,
			.y1_offset    = 0,
			.w1_size      = 2192,
			.h1_size      = 1232,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 2192,
			.h2_tg_size   = 1232,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
	{/* 7 custom3 mode */
		.frame_desc         = frame_desc_cus3,
		.num_entries        = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = bailuimx800_custom3_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_custom3_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 1812000000,
		.linelength        = 9776,
		.framelength       = 3088,
		.max_framerate     = 600,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 0,
			.y0_offset    = 608,
			.w0_size      = 8768,
			.h0_size      = 4928,
			.scale_w      = 4384,
			.scale_h      = 2464,
			.x1_offset    = 0,
			.y1_offset    = 0,
			.w1_size      = 4384,
			.h1_size      = 2464,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 4384,
			.h2_tg_size   = 2464,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
	{/* 8 custom4 mode */
		.frame_desc         = frame_desc_cus4,
		.num_entries        = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = bailuimx800_custom4_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_custom4_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 2196000000,
		.linelength        = 23072,
		.framelength       = 6248,
		.max_framerate     = 150,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 15,
		.ana_gain_max = BASEGAIN * 16,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 288,
			.y0_offset    = 0,
			.w0_size      = 8192,
			.h0_size      = 6144,
			.scale_w      = 8192,
			.scale_h      = 6144,
			.x1_offset    = 0,
			.y1_offset    = 0,
			.w1_size      = 8192,
			.h1_size      = 6144,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 8192,
			.h2_tg_size   = 6144,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
	{/* 9 custom5 mode */
		.frame_desc         = frame_desc_cus5,
		.num_entries        = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = bailuimx800_custom5_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_custom5_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 2196000000,
		.linelength        = 11536,
		.framelength       = 6340,
		.max_framerate     = 300,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 15,
		.ana_gain_max = BASEGAIN * 16,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 2336,
			.y0_offset    = 1536,
			.w0_size      = 4096,
			.h0_size      = 3072,
			.scale_w      = 4096,
			.scale_h      = 3072,
			.x1_offset    = 0,
			.y1_offset    = 0,
			.w1_size      = 4096,
			.h1_size      = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 4096,
			.h2_tg_size   = 3072,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
	{/* 10 custom6 mode */
		.frame_desc         = frame_desc_prev,
		.num_entries        = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = bailuimx800_preview_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_preview_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 2196000000,
		.linelength        = 19552,
		.framelength       = 4674,
		.max_framerate     = 240,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 288,
			.y0_offset    = 0,
			.w0_size      = 8192,
			.h0_size      = 6144,
			.scale_w      = 4096,
			.scale_h      = 3072,
			.x1_offset    = 0,
			.y1_offset    = 0,
			.w1_size      = 4096,
			.h1_size      = 3072,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 4096,
			.h2_tg_size   = 3072,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
	{/* 11 custom7 mode */
		.frame_desc         = frame_desc_cus7,
		.num_entries        = ARRAY_SIZE(frame_desc_cus7),
		.mode_setting_table = bailuimx800_normal_video_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_normal_video_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 1728000000,
		.linelength        = 14240,
		.framelength       = 5050,
		.max_framerate     = 240,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 6,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 0,
			.y0_offset    = 608,
			.w0_size      = 8768,
			.h0_size      = 4928,
			.scale_w      = 4384,
			.scale_h      = 2464,
			.x1_offset    = 0,
			.y1_offset    = 0,
			.w1_size      = 4384,
			.h1_size      = 2464,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 4384,
			.h2_tg_size   = 2464,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
	{/* 12 custom8 mode */
		.frame_desc         = frame_desc_cus8,
		.num_entries        = ARRAY_SIZE(frame_desc_cus8),
		.mode_setting_table = bailuimx800_custom8_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_custom8_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 1440000000,
		.linelength        = 5520,
		.framelength       = 8692,
		.max_framerate     = 300,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 288,
			.y0_offset    = 0,
			.w0_size      = 8192,
			.h0_size      = 6144,
			.scale_w      = 4096,
			.scale_h      = 3072,
			.x1_offset    = 1024,
			.y1_offset    = 768,
			.w1_size      = 2048,
			.h1_size      = 1536,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 2048,
			.h2_tg_size   = 1536,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
	{/* 13 custom9 mode */
		.frame_desc         = frame_desc_cus9,
		.num_entries        = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = bailuimx800_custom9_setting,
		.mode_setting_len   = ARRAY_SIZE(bailuimx800_custom9_setting),
		.seamless_switch_group              = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len   = PARAM_UNDEFINED,
		.hdr_group         = PARAM_UNDEFINED,
		.hdr_mode          = HDR_NONE,
		.pclk              = 1728000000,
		.linelength        = 9776,
		.framelength       = 5888,
		.max_framerate     = 300,
		.mipi_pixel_rate   = 1714290000,
		.readout_length    = 0,
		.read_margin       = 10,
		.framelength_step  = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.imgsensor_winsize_info = {
			.full_w       = 8768,
			.full_h       = 6144,
			.x0_offset    = 0,
			.y0_offset    = 608,
			.w0_size      = 8768,
			.h0_size      = 4928,
			.scale_w      = 4384,
			.scale_h      = 2464,
			.x1_offset    = 0,
			.y1_offset    = 0,
			.w1_size      = 4384,
			.h1_size      = 2464,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size   = 4384,
			.h2_tg_size   = 2464,
		},
		.pdaf_cap          = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio  = 1000,
		.fine_integ_line   = 0,
		.delay_frame       = 3,
		.csi_param = {
			.cphy_settle   = 73,
		},
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id               = BAILUIMX800_SENSOR_ID,
	.reg_addr_sensor_id      = {0x0016, 0x0017},
	.i2c_addr_table          = {0x20, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type  = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num  = ARRAY_SIZE(eeprom_info),
	.resolution  = {8192, 6144},
	.mirror      = IMAGE_NORMAL,

	.mclk                  = 24,
	.isp_driving_current   = ISP_DRIVING_4MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type      = MIPI_CPHY,
	.mipi_lane_num         = SENSOR_MIPI_3_LANE,
	.ob_pedestal           = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_R,
	.ana_gain_def   = BASEGAIN * 4,
	.ana_gain_min   = BASEGAIN * 1,
	.ana_gain_max   = BASEGAIN * 64,
	.ana_gain_type  = 0,
	.ana_gain_step  = 1,
	.ana_gain_table = bailuimx800_ana_gain_table,
	.ana_gain_table_size = sizeof(bailuimx800_ana_gain_table),
	.min_gain_iso    = 50,
	.exposure_def    = 0x3D0,
	.exposure_min    = 15,
	.exposure_max    = 128 * (0xFFFC - 48),
	.exposure_step   = 4,
	.exposure_margin = 48,
	.dig_gain_min    = BASE_DGAIN * 1,
	.dig_gain_max    = BASE_DGAIN * 1,
	.dig_gain_step   = 1,

	.frame_length_max       = 0xFFFC,
	.ae_effective_frame     = 2,
	.frame_time_delay_frame = 3,
	.start_exposure_offset  = 3000000,

	.pdaf_type = PDAF_SUPPORT_CAMSV_QPD,
	.hdr_type  = HDR_SUPPORT_STAGGER_FDOL,
	.seamless_switch_support = TRUE,
	.temperature_support     = TRUE,

	.g_temp     = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph      = set_group_hold,
	.s_cali     = set_sensor_cali,

	.reg_addr_stream      = 0x0100,
	.reg_addr_mirror_flip = 0x0101,
	.reg_addr_exposure = {
			{0x0202, 0x0203}, // Long exp
			//{0x3162, 0x3163}, // Mid exp, Valid only in 3exp DOL and QDOL4
			//{0x3168, 0x3169}, // Mid2 exp, Valid only in QDOL4
			{0x0224, 0x0225}, // Short exp
	},
	.long_exposure_support    = TRUE,
	.reg_addr_exposure_lshift = 0x3150,
	.reg_addr_ana_gain = {
			{0x0204, 0x0205}, // Long ana gain
			//{0x3164, 0x3165}, // Mid ana gain, Valid only in 3exp DOL and QDOL4
			//{0x316A, 0x316B}, // Mid2 ana gain, Valid only in QDOL4
			{0x0216, 0x0217}, // Short ana gain
	},
	.reg_addr_dig_gain = {
			{0x020E, 0x020F}, // Long dig gain
			//{0x3166, 0x3167}, // Mid dig gain, Valid only in 3exp DOL and QDOL4
			//{0x316C, 0x316D}, // Mid2 dig gain, Valid only in QDOL4
			{0x0218, 0x0219}, // Short dig gain
	},
	.reg_addr_frame_length = {0x0340, 0x0341},
	.reg_addr_temp_en      = 0x0138,
	.reg_addr_temp_read    = 0x013A,
	.reg_addr_auto_extend  = 0x0350,
	.reg_addr_frame_count  = 0x0005,
	.reg_addr_fast_mode    = 0x3010,

	.init_setting_table = bailuimx800_init_setting,
	.init_setting_len   = ARRAY_SIZE(bailuimx800_init_setting),
	.mode               = mode_struct,
	.sensor_mode_num    = ARRAY_SIZE(mode_struct),
	.list               = feature_control_list,
	.list_len           = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta      = 1,
	.chk_s_off_end      = 0,

	.checksum_value = 0xD086E5A5,
};

static struct subdrv_ops ops = {
	.get_id = get_imgsensor_id,
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
	{HW_ID_AFVDD, 2800000, 3},
	{HW_ID_DVDD1, 1200000, 3}, // VCAM_LDO
	{HW_ID_MCLK,  24,      0},
	{HW_ID_RST,   0,       1},
	{HW_ID_AVDD,  2800000, 1},
	{HW_ID_AVDD1, 1800000, 1},
	{HW_ID_DVDD,  1200000, 1},
	{HW_ID_DOVDD, 1800000, 1},
	{HW_ID_MCLK_DRIVING_CURRENT, 4, 15},
	{HW_ID_RST,   1,       5}
};

const struct subdrv_entry bailuimx800_mipi_raw_entry = {
	.name = SENSOR_DRVNAME_BAILUIMX800_MIPI_RAW,
	.id = BAILUIMX800_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION  */

static void set_sensor_cali(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	u16 idx = 0;
	u8 support = FALSE;
	u8 *pbuf = NULL;
	u16 size = 0;
	u16 addr = 0;
	struct eeprom_info_struct *info = ctx->s_ctx.eeprom_info;

	if (!probe_eeprom(ctx))
		return;

	idx = ctx->eeprom_index;

	/* QSC data */
	support = info[idx].qsc_support;
	pbuf = info[idx].preload_qsc_table;
	size = info[idx].qsc_size;
	addr = info[idx].sensor_reg_addr_qsc;
	if (support) {
		if (pbuf != NULL && addr > 0 && size > 0) {
			subdrv_i2c_wr_u8(ctx, 0x86A9, 0x4E);
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			subdrv_i2c_wr_u8(ctx, 0x32D2, 0x01);
			DRV_LOG(ctx, "set QSC calibration data done.");
		} else {
			subdrv_i2c_wr_u8(ctx, 0x32D2, 0x00);
		}
	}
}

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
		temperature_convert = (char)temperature | 0xFFFFFF0;

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

static void bailuimx800_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

static void bailuimx800_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

static void bailuimx800_check_sensor_id(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	get_imgsensor_id(ctx, (u32 *)para);
}

static int get_imgsensor_id(struct subdrv_ctx *ctx, u32 *sensor_id)
{
	u8 i = 0;
	u8 retry = 2;
	u32 addr_h = ctx->s_ctx.reg_addr_sensor_id.addr[0];
	u32 addr_l = ctx->s_ctx.reg_addr_sensor_id.addr[1];
	u32 addr_ll = ctx->s_ctx.reg_addr_sensor_id.addr[2];

	while (ctx->s_ctx.i2c_addr_table[i] != 0xFF) {
		ctx->i2c_write_id = ctx->s_ctx.i2c_addr_table[i];
		do {
			*sensor_id = (subdrv_i2c_rd_u8(ctx, addr_h) << 8) |
				subdrv_i2c_rd_u8(ctx, addr_l);
			if (addr_ll)
				*sensor_id = ((*sensor_id) << 8) | subdrv_i2c_rd_u8(ctx, addr_ll);
			DRV_LOG(ctx, "i2c_write_id(0x%x) sensor_id(0x%x/0x%x)\n",
				ctx->i2c_write_id, *sensor_id, ctx->s_ctx.sensor_id);
			if (*sensor_id == BAILUIMX800_SENSOR_ID) {
				*sensor_id = ctx->s_ctx.sensor_id;
				return ERROR_NONE;
			}
			retry--;
		} while (retry > 0);
		i++;
		retry = 2;
	}
	if (*sensor_id != ctx->s_ctx.sensor_id) {
		*sensor_id = 0xFFFFFFFF;
		return ERROR_SENSOR_CONNECT_FAIL;
	}
	return ERROR_NONE;
}

static void bailuimx800_get_stagger_target_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *)para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = *feature_data;
	enum IMGSENSOR_HDR_MODE_ENUM hdr_mode = *(feature_data + 1);
	u32 *pScenarios = (u32 *)(feature_data + 2);
	int i = 0;
	u32 group = 0;

	if (ctx->s_ctx.hdr_type == HDR_SUPPORT_NA)
		return;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		return;
	}
	group = ctx->s_ctx.mode[scenario_id].hdr_group;
	if (scenario_id == SENSOR_SCENARIO_ID_NORMAL_PREVIEW)
		group = 3;
	for (i = 0; i < ctx->s_ctx.sensor_mode_num; i++) {
		if (group != 0 && i != scenario_id &&
		(ctx->s_ctx.mode[i].hdr_group == group) &&
		(ctx->s_ctx.mode[i].hdr_mode == hdr_mode)) {
			*pScenarios = i;
			DRV_LOG(ctx, "sid(input/output):%u/%u, hdr_mode:%u\n",
				scenario_id, *pScenarios, hdr_mode);
			break;
		}
	}
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
	if (ctx->fast_mode_on && (sof_cnt > ctx->ref_sof_cnt)) {
		ctx->fast_mode_on = FALSE;
		ctx->ref_sof_cnt = 0;
		DRV_LOG(ctx, "seamless_switch disabled.");
		set_i2c_buffer(ctx, 0x3010, 0x00);
		commit_i2c_buffer(ctx);
	}
	return 0;
}
