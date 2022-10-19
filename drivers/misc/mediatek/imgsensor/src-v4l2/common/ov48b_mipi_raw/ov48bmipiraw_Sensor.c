// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 ov48bmipiraw_Sensor.c
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
#include "ov48bmipiraw_Sensor.h"

static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static void ov48b_set_dummy(struct subdrv_ctx *ctx);
static void ov48b_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static u16 get_gain2reg(u32 gain);
static void ov48b_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static void ov48b_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);

/* STRUCT */

static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, ov48b_set_test_pattern},
	{SENSOR_FEATURE_SEAMLESS_SWITCH, ov48b_seamless_switch},
	{SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO, ov48b_set_max_framerate_by_scenario},
};

static struct eeprom_info_struct eeprom_info[] = {
	{
		.header_id = 0x010B00FF,
		.addr_header_id = 0x00000001,
		.i2c_write_id = 0xA0,

		.pdc_support = TRUE,
		.pdc_size = 728,
		.addr_pdc = 0x1638,
		.sensor_reg_addr_pdc = 0x5900,

		.xtalk_support = TRUE,
		.xtalk_table = data_xtalk_ov48b2q,
		.xtalk_size = ARRAY_SIZE(data_xtalk_ov48b2q),
		.addr_xtalk = PARAM_UNDEFINED,
		.sensor_reg_addr_xtalk = 0x53C0,
	},
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info = {
	 .i4OffsetX = 16,
	 .i4OffsetY = 4,
	 .i4PitchX = 16,
	 .i4PitchY = 16,
	 .i4PairNum = 8,
	 .i4SubBlkW = 8,
	 .i4SubBlkH = 4,
	 .i4PosL = {{23, 6}, {31, 6}, {19, 10}, {27, 10},
		{23, 14}, {31, 14}, {19, 18}, {27, 18} },
	 .i4PosR = {{22, 6}, {30, 6}, {18, 10}, {26, 10},
		{22, 14}, {30, 14}, {18, 18}, {26, 18} },
	 .iMirrorFlip = 0,
	 .i4BlockNumX = 248,
	 .i4BlockNumY = 187,
	 .i4Crop = { {0, 0}, {0, 0}, {0, 200}, {0, 0}, {0, 0},
			 {0, 0}, {80, 420}, {0, 0}, {0, 0}, {0, 0} },
	 .i4VCPackNum = 2,
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_vid = {
	 .i4OffsetX = 16,
	 .i4OffsetY = 4,
	 .i4PitchX = 16,
	 .i4PitchY = 16,
	 .i4PairNum = 8,
	 .i4SubBlkW = 8,
	 .i4SubBlkH = 4,
	 .i4PosL = {{23, 6}, {31, 6}, {19, 10}, {27, 10},
		{23, 14}, {31, 14}, {19, 18}, {27, 18} },
	 .i4PosR = {{22, 6}, {30, 6}, {18, 10}, {26, 10},
		{22, 14}, {30, 14}, {18, 18}, {26, 18} },
	 .iMirrorFlip = 0,
	 .i4BlockNumX = 248,
	 .i4BlockNumY = 162,
	 .i4Crop = { {0, 0}, {0, 0}, {0, 200}, {0, 0}, {0, 0},
			 {0, 0}, {80, 420}, {0, 0}, {0, 0}, {0, 0} },
	 .i4VCPackNum = 2,
};

static struct SET_PD_BLOCK_INFO_T imgsensor_pd_info_cus2 = {
	 .i4OffsetX = 16,
	 .i4OffsetY = 4,
	 .i4PitchX = 16,
	 .i4PitchY = 16,
	 .i4PairNum = 8,
	 .i4SubBlkW = 8,
	 .i4SubBlkH = 4,
	 .i4PosL = {{23, 6}, {31, 6}, {19, 10}, {27, 10},
		{23, 14}, {31, 14}, {19, 18}, {27, 18} },
	 .i4PosR = {{22, 6}, {30, 6}, {18, 10}, {26, 10},
		{22, 14}, {30, 14}, {18, 18}, {26, 18} },
	 .iMirrorFlip = 0,
	 .i4BlockNumX = 240,
	 .i4BlockNumY = 135,
	 .i4Crop = { {0, 0}, {0, 0}, {0, 200}, {0, 0}, {0, 0},
			 {0, 0}, {80, 420}, {0, 0}, {0, 0}, {0, 0} },
	 .i4VCPackNum = 2,
};

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x01f0,
			.vsize = 0x05d8,
			.user_data_desc = VC_PDAF_STATS,
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
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x01f0,
			.vsize = 0x05d8,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0a28,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x01f0,
			.vsize = 0x0510,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0500,
			.vsize = 0x02d0,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0a28,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x08ca,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0f00,
			.vsize = 0x0870,
		},
	},
	{
		.bus.csi2 = {
			.channel = 1,
			.data_type = 0x2b,
			.hsize = 0x1e0,
			.vsize = 0x438,
			.user_data_desc = VC_PDAF_STATS,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus3[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1f40,
			.vsize = 0x1770,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus4[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x07d0,
			.vsize = 0x0468,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus5[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0500,
			.vsize = 0x02d0,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus6[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0780,
			.vsize = 0x0438,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus7[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0500,
			.vsize = 0x02d0,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus8[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0780,
			.vsize = 0x0438,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus9[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0918,
			.vsize = 0x06d0,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus10[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x1f40,
			.vsize = 0x1770,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus11[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus12[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0fa0,
			.vsize = 0x0bb8,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus13[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 0x0280,
			.vsize = 0x01e0,
		},
	},
};

static struct subdrv_mode_struct mode_struct[] = {
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = addr_data_pair_preview_ov48b2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_preview_ov48b2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 1200,
		.framelength = 3200,
		.max_framerate = 300,
		.mipi_pixel_rate = 548000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6000,
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
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = addr_data_pair_capture_ov48b2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_capture_ov48b2q),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = addr_data_pair_capture_ov48b2q,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(addr_data_pair_capture_ov48b2q),
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 1200,
		.framelength = 3200,
		.max_framerate = 300,
		.mipi_pixel_rate = 548000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6000,
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
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 3,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = addr_data_pair_video_ov48b2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_video_ov48b2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 1200,
		.framelength = 3200,
		.max_framerate = 300,
		.mipi_pixel_rate = 832000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6000,
			.scale_w = 4000,
			.scale_h = 3000,
			.x1_offset = 0,
			.y1_offset = 200,
			.w1_size = 4000,
			.h1_size = 2600,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 2600,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_vid,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = addr_data_pair_hs_video_ov48b2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_hs_video_ov48b2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 1152,
		.framelength = 833,
		.max_framerate = 1200,
		.mipi_pixel_rate = 546000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 1440,
			.y0_offset = 1560,
			.w0_size = 5120,
			.h0_size = 2880,
			.scale_w = 1280,
			.scale_h = 720,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1280,
			.h1_size = 720,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1280,
			.h2_tg_size = 720,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = addr_data_pair_slim_video_ov48b2q,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_slim_video_ov48b2q),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 1200,
		.framelength = 3200,
		.max_framerate = 300,
		.mipi_pixel_rate = 548000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 0,
			.y0_offset = 400,
			.w0_size = 8000,
			.h0_size = 5200,
			.scale_w = 4000,
			.scale_h = 2600,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4000,
			.h1_size = 2600,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 2600,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = addr_data_pair_custom1,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom1),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 576,
		.framelength = 3333,
		.max_framerate = 600,
		.mipi_pixel_rate = 956000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6000,
			.scale_w = 4000,
			.scale_h = 3000,
			.x1_offset = 0,
			.y1_offset = 375,
			.w1_size = 4000,
			.h1_size = 2250,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 2250,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = addr_data_pair_custom2,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom2),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 1200,
		.framelength = 3200,
		.max_framerate = 300,
		.mipi_pixel_rate = 548000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 170,
			.y0_offset = 840,
			.w0_size = 7680,
			.h0_size = 4320,
			.scale_w = 3840,
			.scale_h = 2160,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 3840,
			.h1_size = 2160,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 3840,
			.h2_tg_size = 2160,
		},
		.pdaf_cap = TRUE,
		.imgsensor_pd_info = &imgsensor_pd_info_cus2,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_cus3,
		.num_entries = ARRAY_SIZE(frame_desc_cus3),
		.mode_setting_table = addr_data_pair_custom3,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom3),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 1872,
		.framelength = 6152,
		.max_framerate = 100,
		.mipi_pixel_rate = 548000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6000,
			.scale_w = 8000,
			.scale_h = 6000,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 8000,
			.h1_size = 6000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 8000,
			.h2_tg_size = 6000,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 1,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_cus4,
		.num_entries = ARRAY_SIZE(frame_desc_cus4),
		.mode_setting_table = addr_data_pair_custom4,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom4),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 384,
		.framelength = 1258,
		.max_framerate = 2400,
		.mipi_pixel_rate = 832900000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6000,
			.scale_w = 2000,
			.scale_h = 1500,
			.x1_offset = 0,
			.y1_offset = 186,
			.w1_size = 2000,
			.h1_size = 1128,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2000,
			.h2_tg_size = 1128,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_cus5,
		.num_entries = ARRAY_SIZE(frame_desc_cus5),
		.mode_setting_table = addr_data_pair_custom5,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom5),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 288,
		.framelength = 833,
		.max_framerate = 4800,
		.mipi_pixel_rate = 824000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 1440,
			.y0_offset = 1560,
			.w0_size = 5120,
			.h0_size = 2880,
			.scale_w = 1280,
			.scale_h = 720,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1280,
			.h1_size = 720,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1280,
			.h2_tg_size = 720,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_cus6,
		.num_entries = ARRAY_SIZE(frame_desc_cus6),
		.mode_setting_table = addr_data_pair_custom6,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom6),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 408,
		.framelength = 1176,
		.max_framerate = 2400,
		.mipi_pixel_rate = 832900000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 160,
			.y0_offset = 840,
			.w0_size = 7680,
			.h0_size = 4320,
			.scale_w = 1920,
			.scale_h = 1080,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_cus7,
		.num_entries = ARRAY_SIZE(frame_desc_cus7),
		.mode_setting_table = addr_data_pair_custom7,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom7),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 576,
		.framelength = 833,
		.max_framerate = 2400,
		.mipi_pixel_rate = 546000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 1440,
			.y0_offset = 1560,
			.w0_size = 5120,
			.h0_size = 2880,
			.scale_w = 1280,
			.scale_h = 720,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1280,
			.h1_size = 720,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1280,
			.h2_tg_size = 720,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_cus8,
		.num_entries = ARRAY_SIZE(frame_desc_cus8),
		.mode_setting_table = addr_data_pair_custom8,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom8),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 768,
		.framelength = 1250,
		.max_framerate = 1200,
		.mipi_pixel_rate = 546000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 160,
			.y0_offset = 840,
			.w0_size = 7680,
			.h0_size = 4320,
			.scale_w = 1920,
			.scale_h = 1080,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 1920,
			.h1_size = 1080,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 1920,
			.h2_tg_size = 1080,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_cus9,
		.num_entries = ARRAY_SIZE(frame_desc_cus9),
		.mode_setting_table = addr_data_pair_custom9,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom9),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 576,
		.framelength = 3333,
		.max_framerate = 600,
		.mipi_pixel_rate = 594000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6000,
			.scale_w = 4000,
			.scale_h = 3000,
			.x1_offset = 836,
			.y1_offset = 628,
			.w1_size = 2328,
			.h1_size = 1744,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 2328,
			.h2_tg_size = 1744,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_cus10,
		.num_entries = ARRAY_SIZE(frame_desc_cus10),
		.mode_setting_table = addr_data_pair_custom10,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom10),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = addr_data_pair_custom10,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(addr_data_pair_custom10),
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 1872,
		.framelength = 6152,
		.max_framerate = 100,
		.mipi_pixel_rate = 548000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 0,
			.y0_offset = 0,
			.w0_size = 8000,
			.h0_size = 6000,
			.scale_w = 8000,
			.scale_h = 6000,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 8000,
			.h1_size = 6000,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 8000,
			.h2_tg_size = 6000,
		},
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
	},
	{
		.frame_desc = frame_desc_cus11,
		.num_entries = ARRAY_SIZE(frame_desc_cus11),
		.mode_setting_table = addr_data_pair_custom11,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom11),
		.seamless_switch_group = 1,
		.seamless_switch_mode_setting_table = addr_data_pair_custom11,
		.seamless_switch_mode_setting_len = ARRAY_SIZE(addr_data_pair_custom11),
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 1200,
		.framelength = 3200,
		.max_framerate = 300,
		.mipi_pixel_rate = 548000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 2000,
			.y0_offset = 1500,
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
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
		.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_B,
	},
	{
		.frame_desc = frame_desc_cus12,
		.num_entries = ARRAY_SIZE(frame_desc_cus12),
		.mode_setting_table = addr_data_pair_custom12,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom12),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 1200,
		.framelength = 3200,
		.max_framerate = 300,
		.mipi_pixel_rate = 548000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 2000,
			.y0_offset = 1500,
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
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
	{
		.frame_desc = frame_desc_cus13,
		.num_entries = ARRAY_SIZE(frame_desc_cus13),
		.mode_setting_table = addr_data_pair_custom13,
		.mode_setting_len = ARRAY_SIZE(addr_data_pair_custom13),
		.seamless_switch_group = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_table = PARAM_UNDEFINED,
		.seamless_switch_mode_setting_len = PARAM_UNDEFINED,
		.hdr_group = PARAM_UNDEFINED,
		.hdr_mode = HDR_NONE,
		.pclk = 115200000,
		.linelength = 34560,
		.framelength = 1666,
		.max_framerate = 20,
		.mipi_pixel_rate = 273000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 8000,
			.full_h = 6000,
			.x0_offset = 1440,
			.y0_offset = 1560,
			.w0_size = 5120,
			.h0_size = 2880,
			.scale_w = 640,
			.scale_h = 480,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 640,
			.h1_size = 480,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 640,
			.h2_tg_size = 480,
		},
		.aov_mode = 1,
		.pdaf_cap = FALSE,
		.imgsensor_pd_info = PARAM_UNDEFINED,
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.cphy_settle = 98,
		},
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = OV48B_SENSOR_ID,
	.reg_addr_sensor_id = {0x300A, 0x300B, 0x300C},
	.i2c_addr_table = {0x6D, 0x20, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {8000, 6000},
	.mirror = IMAGE_NORMAL,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_CPHY,
	.mipi_lane_num = SENSOR_MIPI_3_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 15.5,
	.ana_gain_type = 1,
	.ana_gain_step = 4,
	.ana_gain_table = ov48b_ana_gain_table,
	.ana_gain_table_size = sizeof(ov48b_ana_gain_table),
	.min_gain_iso = 100,
	.exposure_def = 0x3D0,
	.exposure_min = 8,
	.exposure_max = 0xFFFFFF - 22,
	.exposure_step = 2,
	.exposure_margin = 22,

	.frame_length_max = 0xFFFFFF,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 3,
	.start_exposure_offset = 0,

	.pdaf_type = PDAF_SUPPORT_CAMSV,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = TRUE,
	.temperature_support = TRUE,
	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_cali = set_sensor_cali,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = PARAM_UNDEFINED,
	.reg_addr_exposure = {{0x3500, 0x3501, 0x3502},},
	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {{0x3508, 0x3509},},
	.reg_addr_frame_length = {0x3840, 0x380E, 0x380F},
	.reg_addr_temp_en = 0x4D12,
	.reg_addr_temp_read = 0x4D13,
	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = PARAM_UNDEFINED,

	.init_setting_table = addr_data_pair_init_ov48b2q,
	.init_setting_len = ARRAY_SIZE(addr_data_pair_init_ov48b2q),
	.mode = mode_struct,
	.sensor_mode_num = ARRAY_SIZE(mode_struct),
	.list = feature_control_list,
	.list_len = ARRAY_SIZE(feature_control_list),
	.chk_s_off_sta = 1,
	.chk_s_off_end = 0,

	.checksum_value = 0x388C7147,
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
	.update_sof_cnt = common_update_sof_cnt,
};

static struct subdrv_pw_seq_entry pw_seq[] = {
	{HW_ID_MCLK, 24, 0},
	{HW_ID_RST, 0, 1},
	{HW_ID_MCLK_DRIVING_CURRENT, 8, 0},
	{HW_ID_DOVDD, 1800000, 0},
	{HW_ID_AVDD, 2800000, 0},
	{HW_ID_DVDD, 1090000, 5},
	{HW_ID_RST, 1, 5},
};

const struct subdrv_entry ov48b_mipi_raw_entry = {
	.name = "ov48b_mipi_raw",
	.id = OV48B_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* STRUCT */

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

	/* PDC data */
	support = info[idx].pdc_support;
	if (support) {
		pbuf = info[idx].preload_pdc_table;
		if (pbuf != NULL) {
			size = 8;
			addr = 0x5C0E;
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			pbuf += size;
			size = 720;
			addr = 0x5900;
			subdrv_i2c_wr_seq_p8(ctx, addr, pbuf, size);
			DRV_LOG(ctx, "set PDC calibration data done.");
		}
	}
}

static int get_sensor_temperature(void *arg)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;
	int temperature = 0;

	/*TEMP_SEN_CTL */
	subdrv_i2c_wr_u8(ctx, ctx->s_ctx.reg_addr_temp_en, 0x01);
	temperature = subdrv_i2c_rd_u8(ctx, ctx->s_ctx.reg_addr_temp_read);
	temperature = (temperature > 0xC0) ? (temperature - 0x100) : temperature;

	DRV_LOG(ctx, "temperature: %d degrees\n", temperature);
	return temperature;
}

static void set_group_hold(void *arg, u8 en)
{
	struct subdrv_ctx *ctx = (struct subdrv_ctx *)arg;

	if (en) {
		set_i2c_buffer(ctx, 0x3208, 0x00);
	} else {
		set_i2c_buffer(ctx, 0x3208, 0x10);
		set_i2c_buffer(ctx, 0x3208, 0xA0);
	}
}

static void ov48b_set_dummy(struct subdrv_ctx *ctx)
{
	// bool gph = !ctx->is_seamless && (ctx->s_ctx.s_gph != NULL);

	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 1);
	write_frame_length(ctx, ctx->frame_length);
	// if (gph)
	// ctx->s_ctx.s_gph((void *)ctx, 0);

	commit_i2c_buffer(ctx);
}

static void ov48b_set_max_framerate_by_scenario(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u64 *feature_data = (u64 *)para;
	enum SENSOR_SCENARIO_ID_ENUM scenario_id = (enum SENSOR_SCENARIO_ID_ENUM)*feature_data;
	u32 framerate = *(feature_data + 1);
	u32 frame_length;

	if (scenario_id >= ctx->s_ctx.sensor_mode_num) {
		DRV_LOG(ctx, "invalid sid:%u, mode_num:%u\n",
			scenario_id, ctx->s_ctx.sensor_mode_num);
		scenario_id = SENSOR_SCENARIO_ID_NORMAL_PREVIEW;
	}

	if (framerate == 0) {
		DRV_LOG(ctx, "framerate should not be 0\n");
		return;
	}

	if (ctx->s_ctx.mode[scenario_id].linelength == 0) {
		DRV_LOG(ctx, "linelength should not be 0\n");
		return;
	}

	if (ctx->line_length == 0) {
		DRV_LOG(ctx, "ctx->line_length should not be 0\n");
		return;
	}

	if (ctx->frame_length == 0) {
		DRV_LOG(ctx, "ctx->frame_length should not be 0\n");
		return;
	}

	frame_length = ctx->s_ctx.mode[scenario_id].pclk / framerate * 10
		/ ctx->s_ctx.mode[scenario_id].linelength;
	ctx->frame_length =
		max(frame_length, ctx->s_ctx.mode[scenario_id].framelength);
	ctx->frame_length = min(ctx->frame_length, ctx->s_ctx.frame_length_max);
	ctx->current_fps = ctx->pclk / ctx->frame_length * 10 / ctx->line_length;
	ctx->min_frame_length = ctx->frame_length;
	DRV_LOG(ctx, "max_fps(input/output):%u/%u(sid:%u), min_fl_en:1\n",
		framerate, ctx->current_fps, scenario_id);
	if (ctx->frame_length > (ctx->exposure[0] + ctx->s_ctx.exposure_margin))
		ov48b_set_dummy(ctx);
}

static u16 get_gain2reg(u32 gain)
{
	return gain * 256 / BASEGAIN;
}

static void ov48b_seamless_switch(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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

	i2c_table_write(ctx, addr_data_pair_seamless_switch_step1_ov48b2q,
		ARRAY_SIZE(addr_data_pair_seamless_switch_step1_ov48b2q));
	i2c_table_write(ctx,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_table,
		ctx->s_ctx.mode[scenario_id].seamless_switch_mode_setting_len);
	if (ae_ctrl) {
		set_shutter(ctx, ae_ctrl[0]);
		set_gain(ctx, ae_ctrl[5]);
	}
	i2c_table_write(ctx, addr_data_pair_seamless_switch_step2_ov48b2q,
		ARRAY_SIZE(addr_data_pair_seamless_switch_step2_ov48b2q));
	if (ae_ctrl) {
		set_shutter(ctx, ae_ctrl[10]);
		set_gain(ctx, ae_ctrl[15]);
	}
	i2c_table_write(ctx, addr_data_pair_seamless_switch_step3_ov48b2q,
		ARRAY_SIZE(addr_data_pair_seamless_switch_step3_ov48b2q));

	ctx->is_seamless = FALSE;
	DRV_LOG(ctx, "X: set seamless switch done\n");
}

static void ov48b_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
{
	u32 mode = *((u32 *)para);

	if (mode != ctx->test_pattern)
		DRV_LOG(ctx, "mode(%u->%u)\n", ctx->test_pattern, mode);
	/* 1:Solid Color 2:Color Bar 5:Black */
	switch (mode) {
	case 2:
		subdrv_i2c_wr_u8(ctx, 0x5000, 0x81);
		subdrv_i2c_wr_u8(ctx, 0x5001, 0x00);
		subdrv_i2c_wr_u8(ctx, 0x5002, 0x92);
		subdrv_i2c_wr_u8(ctx, 0x5081, 0x01);
		break;
	case 5:
		subdrv_i2c_wr_u8(ctx, 0x3019, 0xF0);
		subdrv_i2c_wr_u8(ctx, 0x4308, 0x01);
		break;
	default:
		break;
	}

	if (mode != ctx->test_pattern)
		switch (ctx->test_pattern) {
		case 2:
			subdrv_i2c_wr_u8(ctx, 0x5000, 0xCB);
			subdrv_i2c_wr_u8(ctx, 0x5001, 0x43);
			subdrv_i2c_wr_u8(ctx, 0x5002, 0x9E);
			subdrv_i2c_wr_u8(ctx, 0x5081, 0x00);
			break;
		case 5:
			subdrv_i2c_wr_u8(ctx, 0x3019, 0xD2);
			subdrv_i2c_wr_u8(ctx, 0x4308, 0x00);
			break;
		default:
			break;
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
