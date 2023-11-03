// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2022 MediaTek Inc.

/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 bailuov13b10mipiraw_Sensor.c
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
#include "bailuov13b10mipiraw_Sensor.h"

static void set_sensor_cali(void *arg);
static int get_sensor_temperature(void *arg);
static void set_group_hold(void *arg, u8 en);
static u16 get_gain2reg(u32 gain);
static void bailuov13b10_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len);
static int init_ctx(struct subdrv_ctx *ctx,	struct i2c_client *i2c_client, u8 i2c_write_id);

/* STRUCT */
static struct subdrv_feature_control feature_control_list[] = {
	{SENSOR_FEATURE_SET_TEST_PATTERN, bailuov13b10_set_test_pattern},
};


static struct eeprom_info_struct eeprom_info[] = {
	{
		.header_id = 0x01480005,
		.addr_header_id = 0x00000006,
		.i2c_write_id = 0xA0,

		.qsc_support = TRUE,
		.qsc_size = 0x0C00,
		.addr_qsc = 0x1E30,
		.sensor_reg_addr_qsc = 0xC800,
	},
	{
		.header_id = 0x0148000E,
		.addr_header_id = 0x00000006,
		.i2c_write_id = 0xA0,

		.qsc_support = TRUE,
		.qsc_size = 0x0C00,
		.addr_qsc = 0x1E30,
		.sensor_reg_addr_qsc = 0xC800,
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_prev[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4000,
			.vsize = 3000,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cap[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4000,
			.vsize = 3000,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4000,
			.vsize = 2252,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};

static struct mtk_mbus_frame_desc_entry frame_desc_hs_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4000,
			.vsize = 3000,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_slim_vid[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4000,
			.vsize = 3000,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus1[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4000,
			.vsize = 3000,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};
static struct mtk_mbus_frame_desc_entry frame_desc_cus2[] = {
	{
		.bus.csi2 = {
			.channel = 0,
			.data_type = 0x2b,
			.hsize = 4000,
			.vsize = 3000,
			.user_data_desc = VC_STAGGER_NE,
		},
	},
};


static struct subdrv_mode_struct mode_struct[] = {
	{
		.frame_desc = frame_desc_prev,
		.num_entries = ARRAY_SIZE(frame_desc_prev),
		.mode_setting_table = bailuov13b10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(bailuov13b10_preview_setting),
		.pclk = 112000000,
		.linelength = 1176,
		.framelength = 3172,
		.max_framerate = 300,
		.mipi_pixel_rate = 448000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 104,
			.y0_offset = 60,
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
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.not_fixed_dphy_settle = 1,
			.not_fixed_trail_settle = 1,
			.dphy_data_settle = 0x0,
			.dphy_trail = 0xd0,
		},
	},
	{
		.frame_desc = frame_desc_cap,
		.num_entries = ARRAY_SIZE(frame_desc_cap),
		.mode_setting_table = bailuov13b10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(bailuov13b10_preview_setting),
		.pclk = 112000000,
		.linelength = 1176,
		.framelength = 3172,
		.max_framerate = 300,
		.mipi_pixel_rate = 448000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 104,
			.y0_offset = 60,
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
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.not_fixed_dphy_settle = 1,
			.not_fixed_trail_settle = 1,
			.dphy_data_settle = 0x0,
			.dphy_trail = 0xd0,
		},
	},
	{
		.frame_desc = frame_desc_vid,
		.num_entries = ARRAY_SIZE(frame_desc_vid),
		.mode_setting_table = bailuov13b10_normal_video_setting,
		.mode_setting_len = ARRAY_SIZE(bailuov13b10_normal_video_setting),
		.pclk = 112000000,
		.linelength = 1176,
		.framelength = 3172,
		.max_framerate = 300,
		.mipi_pixel_rate = 448000000,
		.readout_length = 0,
		.read_margin = 0,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 104,
			.y0_offset = 434,
			.w0_size = 4000,
			.h0_size = 2252,
			.scale_w = 4000,
			.scale_h = 2252,
			.x1_offset = 0,
			.y1_offset = 0,
			.w1_size = 4000,
			.h1_size = 2252,
			.x2_tg_offset = 0,
			.y2_tg_offset = 0,
			.w2_tg_size = 4000,
			.h2_tg_size = 2252,
		},
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.not_fixed_dphy_settle = 1,
			.not_fixed_trail_settle = 1,
			.dphy_data_settle = 0x0,
			.dphy_trail = 0xd0,
		},
	},
	{
		.frame_desc = frame_desc_hs_vid,
		.num_entries = ARRAY_SIZE(frame_desc_hs_vid),
		.mode_setting_table = bailuov13b10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(bailuov13b10_preview_setting),
		.pclk = 112000000,
		.linelength = 1176,
		.framelength = 3172,
		.max_framerate = 300,
		.mipi_pixel_rate = 448000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 104,
			.y0_offset = 60,
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
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.not_fixed_dphy_settle = 1,
			.not_fixed_trail_settle = 1,
			.dphy_data_settle = 0x0,
			.dphy_trail = 0xd0,
		},
	},
	{
		.frame_desc = frame_desc_slim_vid,
		.num_entries = ARRAY_SIZE(frame_desc_slim_vid),
		.mode_setting_table = bailuov13b10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(bailuov13b10_preview_setting),
		.pclk = 112000000,
		.linelength = 1176,
		.framelength = 3172,
		.max_framerate = 300,
		.mipi_pixel_rate = 448000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 104,
			.y0_offset = 60,
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
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.not_fixed_dphy_settle = 1,
			.not_fixed_trail_settle = 1,
			.dphy_data_settle = 0x0,
			.dphy_trail = 0xd0,
		},
	},
	{
		.frame_desc = frame_desc_cus1,
		.num_entries = ARRAY_SIZE(frame_desc_cus1),
		.mode_setting_table = bailuov13b10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(bailuov13b10_preview_setting),
		.pclk = 112000000,
		.linelength = 1176,
		.framelength = 3172,
		.max_framerate = 300,
		.mipi_pixel_rate = 448000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 104,
			.y0_offset = 60,
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
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.not_fixed_dphy_settle = 1,
			.not_fixed_trail_settle = 1,
			.dphy_data_settle = 0x0,
			.dphy_trail = 0xd0,
		},
	},
	{
		.frame_desc = frame_desc_cus2,
		.num_entries = ARRAY_SIZE(frame_desc_cus2),
		.mode_setting_table = bailuov13b10_preview_setting,
		.mode_setting_len = ARRAY_SIZE(bailuov13b10_preview_setting),
		.pclk = 112000000,
		.linelength = 1176,
		.framelength = 3172,
		.max_framerate = 300,
		.mipi_pixel_rate = 448000000,
		.readout_length = 0,
		.read_margin = 10,
		.framelength_step = 2,
		.coarse_integ_step = 2,
		.min_exposure_line = 8,
		.imgsensor_winsize_info = {
			.full_w = 4208,
			.full_h = 3120,
			.x0_offset = 104,
			.y0_offset = 60,
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
		.ae_binning_ratio = 2,
		.fine_integ_line = 0,
		.delay_frame = 2,
		.csi_param = {
			.not_fixed_dphy_settle = 1,
			.not_fixed_trail_settle = 1,
			.dphy_data_settle = 0x0,
			.dphy_trail = 0xd0,
		},
	},
};

static struct subdrv_static_ctx static_ctx = {
	.sensor_id = BAILUOV13B10_SENSOR_ID,
	.reg_addr_sensor_id = {0x300B, 0x300C},
	.i2c_addr_table = {0x6C, 0xFF},
	.i2c_burst_write_support = TRUE,
	.i2c_transfer_data_type = I2C_DT_ADDR_16_DATA_8,
	.eeprom_info = eeprom_info,
	.eeprom_num = ARRAY_SIZE(eeprom_info),
	.resolution = {4208, 3120},
	.mirror = IMAGE_NORMAL,

	.mclk = 24,
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.ob_pedestal = 0x40,

	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_4CELL_HW_BAYER_B,
	.ana_gain_def = BASEGAIN * 4,
	.ana_gain_min = BASEGAIN * 1,
	.ana_gain_max = BASEGAIN * 15.5,
	.ana_gain_type = 1,
	.ana_gain_step = 1,
	.ana_gain_table = bailuov13b10_ana_gain_table,
	.ana_gain_table_size = sizeof(bailuov13b10_ana_gain_table),
	.min_gain_iso = 50,
	.exposure_def = 0x3D0,
	.exposure_min = 4,
	.exposure_max = 0x36626A - 24,
	.exposure_step = 2,
	.exposure_margin = 24,
	.dig_gain_min = BASE_DGAIN * 1,
	.dig_gain_max = BASE_DGAIN * 16,
	.dig_gain_step = 4,

	.frame_length_max = 0x7fff,
	.ae_effective_frame = 2,
	.frame_time_delay_frame = 2,
	.start_exposure_offset = 0,

	.pdaf_type = PDAF_SUPPORT_NA,
	.hdr_type = HDR_SUPPORT_NA,
	.seamless_switch_support = FALSE,
	.temperature_support = FALSE,

	.g_temp = get_sensor_temperature,
	.g_gain2reg = get_gain2reg,
	.s_gph = set_group_hold,
	.s_cali = set_sensor_cali,

	.reg_addr_stream = 0x0100,
	.reg_addr_mirror_flip = IMAGE_NORMAL,
	.reg_addr_exposure = {{0x3500, 0x3501, 0x3502},},
	.long_exposure_support = FALSE,
	.reg_addr_exposure_lshift = PARAM_UNDEFINED,
	.reg_addr_ana_gain = {{0x3508, 0x3509},},
	.reg_addr_frame_length = {0x3840, 0x380E, 0x380F},
	.reg_addr_temp_en = 0x4D12,
	.reg_addr_temp_read = 0x4D13,

	.reg_addr_auto_extend = PARAM_UNDEFINED,
	.reg_addr_frame_count = PARAM_UNDEFINED,

	.init_setting_table = bailuov13b10_init_setting,
	.init_setting_len = ARRAY_SIZE(bailuov13b10_init_setting),
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
        {HW_ID_RST,   0,       1},
        {HW_ID_AVDD1, 1800000, 1},
        {HW_ID_DVDD,  1200000, 0},
        {HW_ID_DOVDD, 1800000, 0},
        {HW_ID_AVDD,  2800000, 0},
        {HW_ID_RST,   1,       6},
        {HW_ID_MCLK,  24,      0},
        {HW_ID_MCLK_DRIVING_CURRENT, 4, 2},
};

const struct subdrv_entry bailuov13b10_mipi_raw_entry = {
	.name = "bailuov13b10_mipi_raw",
	.id = BAILUOV13B10_SENSOR_ID,
	.pw_seq = pw_seq,
	.pw_seq_cnt = ARRAY_SIZE(pw_seq),
	.ops = &ops,
};

/* FUNCTION */
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


static u16 get_gain2reg(u32 gain)
{
	kal_uint16 iReg = 0x0000;

	iReg = gain * 256 / BASEGAIN;

	return iReg;

}

static void bailuov13b10_set_test_pattern(struct subdrv_ctx *ctx, u8 *para, u32 *len)
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
