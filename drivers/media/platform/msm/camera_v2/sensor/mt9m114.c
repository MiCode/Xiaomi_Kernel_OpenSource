/* Copyright (c) 2011-2015, The Linux Foundation. All rights reserved.
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
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#define MT9M114_SENSOR_NAME "mt9m114"
#define PLATFORM_DRIVER_NAME "msm_camera_mt9m114"
#define mt9m114_obj mt9m114_##obj

/*#define CONFIG_MSMB_CAMERA_DEBUG*/
#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

/* Sysctl registers */
#define MT9M114_COMMAND_REGISTER                0x0080
#define MT9M114_COMMAND_REGISTER_APPLY_PATCH    (1 << 0)
#define MT9M114_COMMAND_REGISTER_SET_STATE      (1 << 1)
#define MT9M114_COMMAND_REGISTER_REFRESH        (1 << 2)
#define MT9M114_COMMAND_REGISTER_WAIT_FOR_EVENT (1 << 3)
#define MT9M114_COMMAND_REGISTER_OK             (1 << 15)

DEFINE_MSM_MUTEX(mt9m114_mut);
static struct msm_sensor_ctrl_t mt9m114_s_ctrl;

static struct msm_sensor_power_setting mt9m114_power_setting[] = {
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 1,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 30,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 100,
	},
	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 0,
	},
};

static struct msm_camera_i2c_reg_conf mt9m114_720p_settings[] = {
	{0xdc00, 0x50, MSM_CAMERA_I2C_BYTE_DATA, MSM_CAMERA_I2C_CMD_WRITE},
	{MT9M114_COMMAND_REGISTER, MT9M114_COMMAND_REGISTER_SET_STATE,
		MSM_CAMERA_I2C_UNSET_WORD_MASK, MSM_CAMERA_I2C_CMD_POLL},
	{MT9M114_COMMAND_REGISTER, (MT9M114_COMMAND_REGISTER_OK |
		MT9M114_COMMAND_REGISTER_SET_STATE), MSM_CAMERA_I2C_WORD_DATA,
		MSM_CAMERA_I2C_CMD_WRITE},
	{MT9M114_COMMAND_REGISTER, MT9M114_COMMAND_REGISTER_SET_STATE,
		MSM_CAMERA_I2C_UNSET_WORD_MASK, MSM_CAMERA_I2C_CMD_POLL},
	{0xDC01, 0x52, MSM_CAMERA_I2C_BYTE_DATA, MSM_CAMERA_I2C_CMD_POLL},

	{0x098E, 0, MSM_CAMERA_I2C_BYTE_DATA},
	{0xC800, 0x007C,},/*y_addr_start = 124*/
	{0xC802, 0x0004,},/*x_addr_start = 4*/
	{0xC804, 0x0353,},/*y_addr_end = 851*/
	{0xC806, 0x050B,},/*x_addr_end = 1291*/
	{0xC808, 0x02DC,},/*pixclk = 48000000*/
	{0xC80A, 0x6C00,},/*pixclk = 48000000*/
	{0xC80C, 0x0001,},/*row_speed = 1*/
	{0xC80E, 0x00DB,},/*fine_integ_time_min = 219*/
	{0xC810, 0x05BD,},/*fine_integ_time_max = 1469*/
	{0xC812, 0x03E8,},/*frame_length_lines = 1000*/
	{0xC814, 0x0640,},/*line_length_pck = 1600*/
	{0xC816, 0x0060,},/*fine_correction = 96*/
	{0xC818, 0x02D3,},/*cpipe_last_row = 723*/
	{0xC826, 0x0020,},/*reg_0_data = 32*/
	{0xC834, 0x0000,},/*sensor_control_read_mode = 0*/
	{0xC854, 0x0000,},/*crop_window_xoffset = 0*/
	{0xC856, 0x0000,},/*crop_window_yoffset = 0*/
	{0xC858, 0x0500,},/*crop_window_width = 1280*/
	{0xC85A, 0x02D0,},/*crop_window_height = 720*/
	{0xC85C, 0x03, MSM_CAMERA_I2C_BYTE_DATA},  /*crop_cropmode = 3*/
	{0xC868, 0x0500,},/*output_width = 1280*/
	{0xC86A, 0x02D0,},/*output_height = 720*/
	{0xC878, 0x00, MSM_CAMERA_I2C_BYTE_DATA},  /*aet_aemode = 0*/
	{0xC88C, 0x1E00,},/*aet_max_frame_rate = 7680*/
	{0xC88E, 0x1E00,},/*aet_min_frame_rate = 7680*/
	{0xC914, 0x0000,},/*stat_awb_window_xstart = 0*/
	{0xC916, 0x0000,},/*stat_awb_window_ystart = 0*/
	{0xC918, 0x04FF,},/*stat_awb_window_xend = 1279*/
	{0xC91A, 0x02CF,},/*stat_awb_window_yend = 719*/
	{0xC91C, 0x0000,},/*stat_ae_window_xstart = 0*/
	{0xC91E, 0x0000,},/*stat_ae_window_ystart = 0*/
	{0xC920, 0x00FF,},/*stat_ae_window_xend = 255*/
	{0xC922, 0x008F,},/*stat_ae_window_yend = 143*/
};

static struct msm_camera_i2c_reg_conf mt9m114_recommend_settings[] = {
	{0x301A, 0x0200, MSM_CAMERA_I2C_SET_WORD_MASK},
	{0x098E, 0, MSM_CAMERA_I2C_BYTE_DATA},
	/*cam_sysctl_pll_enable = 1*/
	{0xC97E, 0x01, MSM_CAMERA_I2C_BYTE_DATA},
	/*cam_sysctl_pll_divider_m_n = 288*/
	{0xC980, 0x0120,},
	/*cam_sysctl_pll_divider_p = 1792*/
	{0xC982, 0x0700,},
	/*output_control = 32769*/
	{0xC984, 0x8001,},
	/*mipi_timing_t_hs_zero = 3840*/
	{0xC988, 0x0F00,},
	/*mipi_timing_t_hs_exit_hs_trail = 2823*/
	{0xC98A, 0x0B07,},
	/*mipi_timing_t_clk_post_clk_pre = 3329*/
	{0xC98C, 0x0D01,},
	/*mipi_timing_t_clk_trail_clk_zero = 1821*/
	{0xC98E, 0x071D,},
	/*mipi_timing_t_lpx = 6*/
	{0xC990, 0x0006,},
	/*mipi_timing_init_timing = 2572*/
	{0xC992, 0x0A0C,},
	{0xC800, 0x007C,},/*y_addr_start = 124*/
	{0xC802, 0x0004,},/*x_addr_start = 4*/
	{0xC804, 0x0353,},/*y_addr_end = 851*/
	{0xC806, 0x050B,},/*x_addr_end = 1291*/
	{0xC808, 0x02DC,},/*pixclk = 48000000*/
	{0xC80A, 0x6C00,},/*pixclk = 48000000*/
	{0xC80C, 0x0001,},/*row_speed = 1*/
	{0xC80E, 0x00DB,},/*fine_integ_time_min = 219*/
	{0xC810, 0x05BD,},/*fine_integ_time_max = 1469*/
	{0xC812, 0x03E8,},/*frame_length_lines = 1000*/
	{0xC814, 0x0640,},/*line_length_pck = 1600*/
	{0xC816, 0x0060,},/*fine_correction = 96*/
	{0xC818, 0x02D3,},/*cpipe_last_row = 723*/
	{0xC826, 0x0020,},/*reg_0_data = 32*/
	{0xC834, 0x0000,},/*sensor_control_read_mode = 0*/
	{0xC854, 0x0000,},/*crop_window_xoffset = 0*/
	{0xC856, 0x0000,},/*crop_window_yoffset = 0*/
	{0xC858, 0x0500,},/*crop_window_width = 1280*/
	{0xC85A, 0x02D0,},/*crop_window_height = 720*/
	{0xC85C, 0x03, MSM_CAMERA_I2C_BYTE_DATA},  /*crop_cropmode = 3*/
	{0xC868, 0x0500,},/*output_width = 1280*/
	{0xC86A, 0x02D0,},/*output_height = 720*/
	{0xC878, 0x00, MSM_CAMERA_I2C_BYTE_DATA},  /*aet_aemode = 0*/
	{0xC88C, 0x1E00,},/*aet_max_frame_rate = 7680*/
	{0xC88E, 0x1E00,},/*aet_min_frame_rate = 7680*/
	{0xC914, 0x0000,},/*stat_awb_window_xstart = 0*/
	{0xC916, 0x0000,},/*stat_awb_window_ystart = 0*/
	{0xC918, 0x04FF,},/*stat_awb_window_xend = 1279*/
	{0xC91A, 0x02CF,},/*stat_awb_window_yend = 719*/
	{0xC91C, 0x0000,},/*stat_ae_window_xstart = 0*/
	{0xC91E, 0x0000,},/*stat_ae_window_ystart = 0*/
	{0xC920, 0x00FF,},/*stat_ae_window_xend = 255*/
	{0xC922, 0x008F,},/*stat_ae_window_yend = 143*/

	/*Sensor optimization*/
	{0x316A, 0x8270,},
	{0x316C, 0x8270,},
	{0x3ED0, 0x2305,},
	{0x3ED2, 0x77CF,},
	{0x316E, 0x8202,},
	{0x3180, 0x87FF,},
	{0x30D4, 0x6080,},
	{0xA802, 0x0008,},/*AE_TRACK_MODE*/
	{0x3E14, 0xFF39,},
	{0x0982, 0x0001,},/*ACCESS_CTL_STAT*/
	{0x098A, 0x5000,},/*PHYSICAL_ADDRESS_ACCESS*/
	{0xD000, 0x70CF,},
	{0xD002, 0xFFFF,},
	{0xD004, 0xC5D4,},
	{0xD006, 0x903A,},
	{0xD008, 0x2144,},
	{0xD00A, 0x0C00,},
	{0xD00C, 0x2186,},
	{0xD00E, 0x0FF3,},
	{0xD010, 0xB844,},
	{0xD012, 0xB948,},
	{0xD014, 0xE082,},
	{0xD016, 0x20CC,},
	{0xD018, 0x80E2,},
	{0xD01A, 0x21CC,},
	{0xD01C, 0x80A2,},
	{0xD01E, 0x21CC,},
	{0xD020, 0x80E2,},
	{0xD022, 0xF404,},
	{0xD024, 0xD801,},
	{0xD026, 0xF003,},
	{0xD028, 0xD800,},
	{0xD02A, 0x7EE0,},
	{0xD02C, 0xC0F1,},
	{0xD02E, 0x08BA,},
	{0xD030, 0x0600,},
	{0xD032, 0xC1A1,},
	{0xD034, 0x76CF,},
	{0xD036, 0xFFFF,},
	{0xD038, 0xC130,},
	{0xD03A, 0x6E04,},
	{0xD03C, 0xC040,},
	{0xD03E, 0x71CF,},
	{0xD040, 0xFFFF,},
	{0xD042, 0xC790,},
	{0xD044, 0x8103,},
	{0xD046, 0x77CF,},
	{0xD048, 0xFFFF,},
	{0xD04A, 0xC7C0,},
	{0xD04C, 0xE001,},
	{0xD04E, 0xA103,},
	{0xD050, 0xD800,},
	{0xD052, 0x0C6A,},
	{0xD054, 0x04E0,},
	{0xD056, 0xB89E,},
	{0xD058, 0x7508,},
	{0xD05A, 0x8E1C,},
	{0xD05C, 0x0809,},
	{0xD05E, 0x0191,},
	{0xD060, 0xD801,},
	{0xD062, 0xAE1D,},
	{0xD064, 0xE580,},
	{0xD066, 0x20CA,},
	{0xD068, 0x0022,},
	{0xD06A, 0x20CF,},
	{0xD06C, 0x0522,},
	{0xD06E, 0x0C5C,},
	{0xD070, 0x04E2,},
	{0xD072, 0x21CA,},
	{0xD074, 0x0062,},
	{0xD076, 0xE580,},
	{0xD078, 0xD901,},
	{0xD07A, 0x79C0,},
	{0xD07C, 0xD800,},
	{0xD07E, 0x0BE6,},
	{0xD080, 0x04E0,},
	{0xD082, 0xB89E,},
	{0xD084, 0x70CF,},
	{0xD086, 0xFFFF,},
	{0xD088, 0xC8D4,},
	{0xD08A, 0x9002,},
	{0xD08C, 0x0857,},
	{0xD08E, 0x025E,},
	{0xD090, 0xFFDC,},
	{0xD092, 0xE080,},
	{0xD094, 0x25CC,},
	{0xD096, 0x9022,},
	{0xD098, 0xF225,},
	{0xD09A, 0x1700,},
	{0xD09C, 0x108A,},
	{0xD09E, 0x73CF,},
	{0xD0A0, 0xFF00,},
	{0xD0A2, 0x3174,},
	{0xD0A4, 0x9307,},
	{0xD0A6, 0x2A04,},
	{0xD0A8, 0x103E,},
	{0xD0AA, 0x9328,},
	{0xD0AC, 0x2942,},
	{0xD0AE, 0x7140,},
	{0xD0B0, 0x2A04,},
	{0xD0B2, 0x107E,},
	{0xD0B4, 0x9349,},
	{0xD0B6, 0x2942,},
	{0xD0B8, 0x7141,},
	{0xD0BA, 0x2A04,},
	{0xD0BC, 0x10BE,},
	{0xD0BE, 0x934A,},
	{0xD0C0, 0x2942,},
	{0xD0C2, 0x714B,},
	{0xD0C4, 0x2A04,},
	{0xD0C6, 0x10BE,},
	{0xD0C8, 0x130C,},
	{0xD0CA, 0x010A,},
	{0xD0CC, 0x2942,},
	{0xD0CE, 0x7142,},
	{0xD0D0, 0x2250,},
	{0xD0D2, 0x13CA,},
	{0xD0D4, 0x1B0C,},
	{0xD0D6, 0x0284,},
	{0xD0D8, 0xB307,},
	{0xD0DA, 0xB328,},
	{0xD0DC, 0x1B12,},
	{0xD0DE, 0x02C4,},
	{0xD0E0, 0xB34A,},
	{0xD0E2, 0xED88,},
	{0xD0E4, 0x71CF,},
	{0xD0E6, 0xFF00,},
	{0xD0E8, 0x3174,},
	{0xD0EA, 0x9106,},
	{0xD0EC, 0xB88F,},
	{0xD0EE, 0xB106,},
	{0xD0F0, 0x210A,},
	{0xD0F2, 0x8340,},
	{0xD0F4, 0xC000,},
	{0xD0F6, 0x21CA,},
	{0xD0F8, 0x0062,},
	{0xD0FA, 0x20F0,},
	{0xD0FC, 0x0040,},
	{0xD0FE, 0x0B02,},
	{0xD100, 0x0320,},
	{0xD102, 0xD901,},
	{0xD104, 0x07F1,},
	{0xD106, 0x05E0,},
	{0xD108, 0xC0A1,},
	{0xD10A, 0x78E0,},
	{0xD10C, 0xC0F1,},
	{0xD10E, 0x71CF,},
	{0xD110, 0xFFFF,},
	{0xD112, 0xC7C0,},
	{0xD114, 0xD840,},
	{0xD116, 0xA900,},
	{0xD118, 0x71CF,},
	{0xD11A, 0xFFFF,},
	{0xD11C, 0xD02C,},
	{0xD11E, 0xD81E,},
	{0xD120, 0x0A5A,},
	{0xD122, 0x04E0,},
	{0xD124, 0xDA00,},
	{0xD126, 0xD800,},
	{0xD128, 0xC0D1,},
	{0xD12A, 0x7EE0,},
	{0x098E, 0x0000,},

	{0x0982, 0x0001,},
	{0x098A, 0x5C10,},
	{0xDC10, 0xC0F1,},
	{0xDC12, 0x0CDA,},
	{0xDC14, 0x0580,},
	{0xDC16, 0x76CF,},
	{0xDC18, 0xFF00,},
	{0xDC1A, 0x2184,},
	{0xDC1C, 0x9624,},
	{0xDC1E, 0x218C,},
	{0xDC20, 0x8FC3,},
	{0xDC22, 0x75CF,},
	{0xDC24, 0xFFFF,},
	{0xDC26, 0xE058,},
	{0xDC28, 0xF686,},
	{0xDC2A, 0x1550,},
	{0xDC2C, 0x1080,},
	{0xDC2E, 0xE001,},
	{0xDC30, 0x1D50,},
	{0xDC32, 0x1002,},
	{0xDC34, 0x1552,},
	{0xDC36, 0x1100,},
	{0xDC38, 0x6038,},
	{0xDC3A, 0x1D52,},
	{0xDC3C, 0x1004,},
	{0xDC3E, 0x1540,},
	{0xDC40, 0x1080,},
	{0xDC42, 0x081B,},
	{0xDC44, 0x00D1,},
	{0xDC46, 0x8512,},
	{0xDC48, 0x1000,},
	{0xDC4A, 0x00C0,},
	{0xDC4C, 0x7822,},
	{0xDC4E, 0x2089,},
	{0xDC50, 0x0FC1,},
	{0xDC52, 0x2008,},
	{0xDC54, 0x0F81,},
	{0xDC56, 0xFFFF,},
	{0xDC58, 0xFF80,},
	{0xDC5A, 0x8512,},
	{0xDC5C, 0x1801,},
	{0xDC5E, 0x0052,},
	{0xDC60, 0xA512,},
	{0xDC62, 0x1544,},
	{0xDC64, 0x1080,},
	{0xDC66, 0xB861,},
	{0xDC68, 0x262F,},
	{0xDC6A, 0xF007,},
	{0xDC6C, 0x1D44,},
	{0xDC6E, 0x1002,},
	{0xDC70, 0x20CA,},
	{0xDC72, 0x0021,},
	{0xDC74, 0x20CF,},
	{0xDC76, 0x04E1,},
	{0xDC78, 0x0850,},
	{0xDC7A, 0x04A1,},
	{0xDC7C, 0x21CA,},
	{0xDC7E, 0x0021,},
	{0xDC80, 0x1542,},
	{0xDC82, 0x1140,},
	{0xDC84, 0x8D2C,},
	{0xDC86, 0x6038,},
	{0xDC88, 0x1D42,},
	{0xDC8A, 0x1004,},
	{0xDC8C, 0x1542,},
	{0xDC8E, 0x1140,},
	{0xDC90, 0xB601,},
	{0xDC92, 0x046D,},
	{0xDC94, 0x0580,},
	{0xDC96, 0x78E0,},
	{0xDC98, 0xD800,},
	{0xDC9A, 0xB893,},
	{0xDC9C, 0x002D,},
	{0xDC9E, 0x04A0,},
	{0xDCA0, 0xD900,},
	{0xDCA2, 0x78E0,},
	{0xDCA4, 0x72CF,},
	{0xDCA6, 0xFFFF,},
	{0xDCA8, 0xE058,},
	{0xDCAA, 0x2240,},
	{0xDCAC, 0x0340,},
	{0xDCAE, 0xA212,},
	{0xDCB0, 0x208A,},
	{0xDCB2, 0x0FFF,},
	{0xDCB4, 0x1A42,},
	{0xDCB6, 0x0004,},
	{0xDCB8, 0xD830,},
	{0xDCBA, 0x1A44,},
	{0xDCBC, 0x0002,},
	{0xDCBE, 0xD800,},
	{0xDCC0, 0x1A50,},
	{0xDCC2, 0x0002,},
	{0xDCC4, 0x1A52,},
	{0xDCC6, 0x0004,},
	{0xDCC8, 0x1242,},
	{0xDCCA, 0x0140,},
	{0xDCCC, 0x8A2C,},
	{0xDCCE, 0x6038,},
	{0xDCD0, 0x1A42,},
	{0xDCD2, 0x0004,},
	{0xDCD4, 0x1242,},
	{0xDCD6, 0x0141,},
	{0xDCD8, 0x70CF,},
	{0xDCDA, 0xFF00,},
	{0xDCDC, 0x2184,},
	{0xDCDE, 0xB021,},
	{0xDCE0, 0xD800,},
	{0xDCE2, 0xB893,},
	{0xDCE4, 0x07E5,},
	{0xDCE6, 0x0460,},
	{0xDCE8, 0xD901,},
	{0xDCEA, 0x78E0,},
	{0xDCEC, 0xC0F1,},
	{0xDCEE, 0x0BFA,},
	{0xDCF0, 0x05A0,},
	{0xDCF2, 0x216F,},
	{0xDCF4, 0x0043,},
	{0xDCF6, 0xC1A4,},
	{0xDCF8, 0x220A,},
	{0xDCFA, 0x1F80,},
	{0xDCFC, 0xFFFF,},
	{0xDCFE, 0xE058,},
	{0xDD00, 0x2240,},
	{0xDD02, 0x134F,},
	{0xDD04, 0x1A48,},
	{0xDD06, 0x13C0,},
	{0xDD08, 0x1248,},
	{0xDD0A, 0x1002,},
	{0xDD0C, 0x70CF,},
	{0xDD0E, 0x7FFF,},
	{0xDD10, 0xFFFF,},
	{0xDD12, 0xE230,},
	{0xDD14, 0xC240,},
	{0xDD16, 0xDA00,},
	{0xDD18, 0xF00C,},
	{0xDD1A, 0x1248,},
	{0xDD1C, 0x1003,},
	{0xDD1E, 0x1301,},
	{0xDD20, 0x04CB,},
	{0xDD22, 0x7261,},
	{0xDD24, 0x2108,},
	{0xDD26, 0x0081,},
	{0xDD28, 0x2009,},
	{0xDD2A, 0x0080,},
	{0xDD2C, 0x1A48,},
	{0xDD2E, 0x10C0,},
	{0xDD30, 0x1248,},
	{0xDD32, 0x100B,},
	{0xDD34, 0xC300,},
	{0xDD36, 0x0BE7,},
	{0xDD38, 0x90C4,},
	{0xDD3A, 0x2102,},
	{0xDD3C, 0x0003,},
	{0xDD3E, 0x238C,},
	{0xDD40, 0x8FC3,},
	{0xDD42, 0xF6C7,},
	{0xDD44, 0xDAFF,},
	{0xDD46, 0x1A05,},
	{0xDD48, 0x1082,},
	{0xDD4A, 0xC241,},
	{0xDD4C, 0xF005,},
	{0xDD4E, 0x7A6F,},
	{0xDD50, 0xC241,},
	{0xDD52, 0x1A05,},
	{0xDD54, 0x10C2,},
	{0xDD56, 0x2000,},
	{0xDD58, 0x8040,},
	{0xDD5A, 0xDA00,},
	{0xDD5C, 0x20C0,},
	{0xDD5E, 0x0064,},
	{0xDD60, 0x781C,},
	{0xDD62, 0xC042,},
	{0xDD64, 0x1C0E,},
	{0xDD66, 0x3082,},
	{0xDD68, 0x1A48,},
	{0xDD6A, 0x13C0,},
	{0xDD6C, 0x7548,},
	{0xDD6E, 0x7348,},
	{0xDD70, 0x7148,},
	{0xDD72, 0x7648,},
	{0xDD74, 0xF002,},
	{0xDD76, 0x7608,},
	{0xDD78, 0x1248,},
	{0xDD7A, 0x1000,},
	{0xDD7C, 0x1400,},
	{0xDD7E, 0x300B,},
	{0xDD80, 0x084D,},
	{0xDD82, 0x02C5,},
	{0xDD84, 0x1248,},
	{0xDD86, 0x1000,},
	{0xDD88, 0xE101,},
	{0xDD8A, 0x1001,},
	{0xDD8C, 0x04CB,},
	{0xDD8E, 0x1A48,},
	{0xDD90, 0x1000,},
	{0xDD92, 0x7361,},
	{0xDD94, 0x1408,},
	{0xDD96, 0x300B,},
	{0xDD98, 0x2302,},
	{0xDD9A, 0x02C0,},
	{0xDD9C, 0x780D,},
	{0xDD9E, 0x2607,},
	{0xDDA0, 0x903E,},
	{0xDDA2, 0x07D6,},
	{0xDDA4, 0xFFE3,},
	{0xDDA6, 0x792F,},
	{0xDDA8, 0x09CF,},
	{0xDDAA, 0x8152,},
	{0xDDAC, 0x1248,},
	{0xDDAE, 0x100E,},
	{0xDDB0, 0x2400,},
	{0xDDB2, 0x334B,},
	{0xDDB4, 0xE501,},
	{0xDDB6, 0x7EE2,},
	{0xDDB8, 0x0DBF,},
	{0xDDBA, 0x90F2,},
	{0xDDBC, 0x1B0C,},
	{0xDDBE, 0x1382,},
	{0xDDC0, 0xC123,},
	{0xDDC2, 0x140E,},
	{0xDDC4, 0x3080,},
	{0xDDC6, 0x7822,},
	{0xDDC8, 0x1A07,},
	{0xDDCA, 0x1002,},
	{0xDDCC, 0x124C,},
	{0xDDCE, 0x1000,},
	{0xDDD0, 0x120B,},
	{0xDDD2, 0x1081,},
	{0xDDD4, 0x1207,},
	{0xDDD6, 0x1083,},
	{0xDDD8, 0x2142,},
	{0xDDDA, 0x004B,},
	{0xDDDC, 0x781B,},
	{0xDDDE, 0x0B21,},
	{0xDDE0, 0x02E2,},
	{0xDDE2, 0x1A4C,},
	{0xDDE4, 0x1000,},
	{0xDDE6, 0xE101,},
	{0xDDE8, 0x0915,},
	{0xDDEA, 0x00C2,},
	{0xDDEC, 0xC101,},
	{0xDDEE, 0x1204,},
	{0xDDF0, 0x1083,},
	{0xDDF2, 0x090D,},
	{0xDDF4, 0x00C2,},
	{0xDDF6, 0xE001,},
	{0xDDF8, 0x1A4C,},
	{0xDDFA, 0x1000,},
	{0xDDFC, 0x1A06,},
	{0xDDFE, 0x1002,},
	{0xDE00, 0x234A,},
	{0xDE02, 0x1000,},
	{0xDE04, 0x7169,},
	{0xDE06, 0xF008,},
	{0xDE08, 0x2053,},
	{0xDE0A, 0x0003,},
	{0xDE0C, 0x6179,},
	{0xDE0E, 0x781C,},
	{0xDE10, 0x2340,},
	{0xDE12, 0x104B,},
	{0xDE14, 0x1203,},
	{0xDE16, 0x1083,},
	{0xDE18, 0x0BF1,},
	{0xDE1A, 0x90C2,},
	{0xDE1C, 0x1202,},
	{0xDE1E, 0x1080,},
	{0xDE20, 0x091D,},
	{0xDE22, 0x0004,},
	{0xDE24, 0x70CF,},
	{0xDE26, 0xFFFF,},
	{0xDE28, 0xC644,},
	{0xDE2A, 0x881B,},
	{0xDE2C, 0xE0B2,},
	{0xDE2E, 0xD83C,},
	{0xDE30, 0x20CA,},
	{0xDE32, 0x0CA2,},
	{0xDE34, 0x1A01,},
	{0xDE36, 0x1002,},
	{0xDE38, 0x1A4C,},
	{0xDE3A, 0x1080,},
	{0xDE3C, 0x02B9,},
	{0xDE3E, 0x05A0,},
	{0xDE40, 0xC0A4,},
	{0xDE42, 0x78E0,},
	{0xDE44, 0xC0F1,},
	{0xDE46, 0xFF95,},
	{0xDE48, 0xD800,},
	{0xDE4A, 0x71CF,},
	{0xDE4C, 0xFF00,},
	{0xDE4E, 0x1FE0,},
	{0xDE50, 0x19D0,},
	{0xDE52, 0x001C,},
	{0xDE54, 0x19D1,},
	{0xDE56, 0x001C,},
	{0xDE58, 0x70CF,},
	{0xDE5A, 0xFFFF,},
	{0xDE5C, 0xE058,},
	{0xDE5E, 0x901F,},
	{0xDE60, 0xB861,},
	{0xDE62, 0x19D2,},
	{0xDE64, 0x001C,},
	{0xDE66, 0xC0D1,},
	{0xDE68, 0x7EE0,},
	{0xDE6A, 0x78E0,},
	{0xDE6C, 0xC0F1,},
	{0xDE6E, 0x0A7A,},
	{0xDE70, 0x0580,},
	{0xDE72, 0x70CF,},
	{0xDE74, 0xFFFF,},
	{0xDE76, 0xC5D4,},
	{0xDE78, 0x9041,},
	{0xDE7A, 0x9023,},
	{0xDE7C, 0x75CF,},
	{0xDE7E, 0xFFFF,},
	{0xDE80, 0xE058,},
	{0xDE82, 0x7942,},
	{0xDE84, 0xB967,},
	{0xDE86, 0x7F30,},
	{0xDE88, 0xB53F,},
	{0xDE8A, 0x71CF,},
	{0xDE8C, 0xFFFF,},
	{0xDE8E, 0xC84C,},
	{0xDE90, 0x91D3,},
	{0xDE92, 0x108B,},
	{0xDE94, 0x0081,},
	{0xDE96, 0x2615,},
	{0xDE98, 0x1380,},
	{0xDE9A, 0x090F,},
	{0xDE9C, 0x0C91,},
	{0xDE9E, 0x0A8E,},
	{0xDEA0, 0x05A0,},
	{0xDEA2, 0xD906,},
	{0xDEA4, 0x7E10,},
	{0xDEA6, 0x2615,},
	{0xDEA8, 0x1380,},
	{0xDEAA, 0x0A82,},
	{0xDEAC, 0x05A0,},
	{0xDEAE, 0xD960,},
	{0xDEB0, 0x790F,},
	{0xDEB2, 0x090D,},
	{0xDEB4, 0x0133,},
	{0xDEB6, 0xAD0C,},
	{0xDEB8, 0xD904,},
	{0xDEBA, 0xAD2C,},
	{0xDEBC, 0x79EC,},
	{0xDEBE, 0x2941,},
	{0xDEC0, 0x7402,},
	{0xDEC2, 0x71CF,},
	{0xDEC4, 0xFF00,},
	{0xDEC6, 0x2184,},
	{0xDEC8, 0xB142,},
	{0xDECA, 0x1906,},
	{0xDECC, 0x0E44,},
	{0xDECE, 0xFFDE,},
	{0xDED0, 0x70C9,},
	{0xDED2, 0x0A5A,},
	{0xDED4, 0x05A0,},
	{0xDED6, 0x8D2C,},
	{0xDED8, 0xAD0B,},
	{0xDEDA, 0xD800,},
	{0xDEDC, 0xAD01,},
	{0xDEDE, 0x0219,},
	{0xDEE0, 0x05A0,},
	{0xDEE2, 0xA513,},
	{0xDEE4, 0xC0F1,},
	{0xDEE6, 0x71CF,},
	{0xDEE8, 0xFFFF,},
	{0xDEEA, 0xC644,},
	{0xDEEC, 0xA91B,},
	{0xDEEE, 0xD902,},
	{0xDEF0, 0x70CF,},
	{0xDEF2, 0xFFFF,},
	{0xDEF4, 0xC84C,},
	{0xDEF6, 0x093E,},
	{0xDEF8, 0x03A0,},
	{0xDEFA, 0xA826,},
	{0xDEFC, 0xFFDC,},
	{0xDEFE, 0xF1B5,},
	{0xDF00, 0xC0F1,},
	{0xDF02, 0x09EA,},
	{0xDF04, 0x0580,},
	{0xDF06, 0x75CF,},
	{0xDF08, 0xFFFF,},
	{0xDF0A, 0xE058,},
	{0xDF0C, 0x1540,},
	{0xDF0E, 0x1080,},
	{0xDF10, 0x08A7,},
	{0xDF12, 0x0010,},
	{0xDF14, 0x8D00,},
	{0xDF16, 0x0813,},
	{0xDF18, 0x009E,},
	{0xDF1A, 0x1540,},
	{0xDF1C, 0x1081,},
	{0xDF1E, 0xE181,},
	{0xDF20, 0x20CA,},
	{0xDF22, 0x00A1,},
	{0xDF24, 0xF24B,},
	{0xDF26, 0x1540,},
	{0xDF28, 0x1081,},
	{0xDF2A, 0x090F,},
	{0xDF2C, 0x0050,},
	{0xDF2E, 0x1540,},
	{0xDF30, 0x1081,},
	{0xDF32, 0x0927,},
	{0xDF34, 0x0091,},
	{0xDF36, 0x1550,},
	{0xDF38, 0x1081,},
	{0xDF3A, 0xDE00,},
	{0xDF3C, 0xAD2A,},
	{0xDF3E, 0x1D50,},
	{0xDF40, 0x1382,},
	{0xDF42, 0x1552,},
	{0xDF44, 0x1101,},
	{0xDF46, 0x1D52,},
	{0xDF48, 0x1384,},
	{0xDF4A, 0xB524,},
	{0xDF4C, 0x082D,},
	{0xDF4E, 0x015F,},
	{0xDF50, 0xFF55,},
	{0xDF52, 0xD803,},
	{0xDF54, 0xF033,},
	{0xDF56, 0x1540,},
	{0xDF58, 0x1081,},
	{0xDF5A, 0x0967,},
	{0xDF5C, 0x00D1,},
	{0xDF5E, 0x1550,},
	{0xDF60, 0x1081,},
	{0xDF62, 0xDE00,},
	{0xDF64, 0xAD2A,},
	{0xDF66, 0x1D50,},
	{0xDF68, 0x1382,},
	{0xDF6A, 0x1552,},
	{0xDF6C, 0x1101,},
	{0xDF6E, 0x1D52,},
	{0xDF70, 0x1384,},
	{0xDF72, 0xB524,},
	{0xDF74, 0x0811,},
	{0xDF76, 0x019E,},
	{0xDF78, 0xB8A0,},
	{0xDF7A, 0xAD00,},
	{0xDF7C, 0xFF47,},
	{0xDF7E, 0x1D40,},
	{0xDF80, 0x1382,},
	{0xDF82, 0xF01F,},
	{0xDF84, 0xFF5A,},
	{0xDF86, 0x8D01,},
	{0xDF88, 0x8D40,},
	{0xDF8A, 0xE812,},
	{0xDF8C, 0x71CF,},
	{0xDF8E, 0xFFFF,},
	{0xDF90, 0xC644,},
	{0xDF92, 0x893B,},
	{0xDF94, 0x7030,},
	{0xDF96, 0x22D1,},
	{0xDF98, 0x8062,},
	{0xDF9A, 0xF20A,},
	{0xDF9C, 0x0A0F,},
	{0xDF9E, 0x009E,},
	{0xDFA0, 0x71CF,},
	{0xDFA2, 0xFFFF,},
	{0xDFA4, 0xC84C,},
	{0xDFA6, 0x893B,},
	{0xDFA8, 0xE902,},
	{0xDFAA, 0xFFCF,},
	{0xDFAC, 0x8D00,},
	{0xDFAE, 0xB8E7,},
	{0xDFB0, 0x26CA,},
	{0xDFB2, 0x1022,},
	{0xDFB4, 0xF5E2,},
	{0xDFB6, 0xFF3C,},
	{0xDFB8, 0xD801,},
	{0xDFBA, 0x1D40,},
	{0xDFBC, 0x1002,},
	{0xDFBE, 0x0141,},
	{0xDFC0, 0x0580,},
	{0xDFC2, 0x78E0,},
	{0xDFC4, 0xC0F1,},
	{0xDFC6, 0xC5E1,},
	{0xDFC8, 0xFF34,},
	{0xDFCA, 0xDD00,},
	{0xDFCC, 0x70CF,},
	{0xDFCE, 0xFFFF,},
	{0xDFD0, 0xE090,},
	{0xDFD2, 0xA8A8,},
	{0xDFD4, 0xD800,},
	{0xDFD6, 0xB893,},
	{0xDFD8, 0x0C8A,},
	{0xDFDA, 0x0460,},
	{0xDFDC, 0xD901,},
	{0xDFDE, 0x71CF,},
	{0xDFE0, 0xFFFF,},
	{0xDFE2, 0xDC10,},
	{0xDFE4, 0xD813,},
	{0xDFE6, 0x0B96,},
	{0xDFE8, 0x0460,},
	{0xDFEA, 0x72A9,},
	{0xDFEC, 0x0119,},
	{0xDFEE, 0x0580,},
	{0xDFF0, 0xC0F1,},
	{0xDFF2, 0x71CF,},
	{0xDFF4, 0x0000,},
	{0xDFF6, 0x5BAE,},
	{0xDFF8, 0x7940,},
	{0xDFFA, 0xFF9D,},
	{0xDFFC, 0xF135,},
	{0xDFFE, 0x78E0,},
	{0xE000, 0xC0F1,},
	{0xE002, 0x70CF,},
	{0xE004, 0x0000,},
	{0xE006, 0x5CBA,},
	{0xE008, 0x7840,},
	{0xE00A, 0x70CF,},
	{0xE00C, 0xFFFF,},
	{0xE00E, 0xE058,},
	{0xE010, 0x8800,},
	{0xE012, 0x0815,},
	{0xE014, 0x001E,},
	{0xE016, 0x70CF,},
	{0xE018, 0xFFFF,},
	{0xE01A, 0xC84C,},
	{0xE01C, 0x881A,},
	{0xE01E, 0xE080,},
	{0xE020, 0x0EE0,},
	{0xE022, 0xFFC1,},
	{0xE024, 0xF121,},
	{0xE026, 0x78E0,},
	{0xE028, 0xC0F1,},
	{0xE02A, 0xD900,},
	{0xE02C, 0xF009,},
	{0xE02E, 0x70CF,},
	{0xE030, 0xFFFF,},
	{0xE032, 0xE0AC,},
	{0xE034, 0x7835,},
	{0xE036, 0x8041,},
	{0xE038, 0x8000,},
	{0xE03A, 0xE102,},
	{0xE03C, 0xA040,},
	{0xE03E, 0x09F3,},
	{0xE040, 0x8114,},
	{0xE042, 0x71CF,},
	{0xE044, 0xFFFF,},
	{0xE046, 0xE058,},
	{0xE048, 0x70CF,},
	{0xE04A, 0xFFFF,},
	{0xE04C, 0xC594,},
	{0xE04E, 0xB030,},
	{0xE050, 0xFFDD,},
	{0xE052, 0xD800,},
	{0xE054, 0xF109,},
	{0xE056, 0x0000,},
	{0xE058, 0x0300,},
	{0xE05A, 0x0204,},
	{0xE05C, 0x0700,},
	{0xE05E, 0x0000,},
	{0xE060, 0x0000,},
	{0xE062, 0x0000,},
	{0xE064, 0x0000,},
	{0xE066, 0x0000,},
	{0xE068, 0x0000,},
	{0xE06A, 0x0000,},
	{0xE06C, 0x0000,},
	{0xE06E, 0x0000,},
	{0xE070, 0x0000,},
	{0xE072, 0x0000,},
	{0xE074, 0x0000,},
	{0xE076, 0x0000,},
	{0xE078, 0x0000,},
	{0xE07A, 0x0000,},
	{0xE07C, 0x0000,},
	{0xE07E, 0x0000,},
	{0xE080, 0x0000,},
	{0xE082, 0x0000,},
	{0xE084, 0x0000,},
	{0xE086, 0x0000,},
	{0xE088, 0x0000,},
	{0xE08A, 0x0000,},
	{0xE08C, 0x0000,},
	{0xE08E, 0x0000,},
	{0xE090, 0x0000,},
	{0xE092, 0x0000,},
	{0xE094, 0x0000,},
	{0xE096, 0x0000,},
	{0xE098, 0x0000,},
	{0xE09A, 0x0000,},
	{0xE09C, 0x0000,},
	{0xE09E, 0x0000,},
	{0xE0A0, 0x0000,},
	{0xE0A2, 0x0000,},
	{0xE0A4, 0x0000,},
	{0xE0A6, 0x0000,},
	{0xE0A8, 0x0000,},
	{0xE0AA, 0x0000,},
	{0xE0AC, 0xFFFF,},
	{0xE0AE, 0xCB68,},
	{0xE0B0, 0xFFFF,},
	{0xE0B2, 0xDFF0,},
	{0xE0B4, 0xFFFF,},
	{0xE0B6, 0xCB6C,},
	{0xE0B8, 0xFFFF,},
	{0xE0BA, 0xE000,},
	{0x098E, 0x0000,},

	/*MIPI setting for SOC1040*/
	{0x3C5A, 0x0009,},
	{0x3C44, 0x0080,},/*MIPI_CUSTOM_SHORT_PKT*/

	/*[Tuning_settings]*/

	/*[CCM]*/
	{0xC892, 0x0267,},/*CAM_AWB_CCM_L_0*/
	{0xC894, 0xFF1A,},/*CAM_AWB_CCM_L_1*/
	{0xC896, 0xFFB3,},/*CAM_AWB_CCM_L_2*/
	{0xC898, 0xFF80,},/*CAM_AWB_CCM_L_3*/
	{0xC89A, 0x0166,},/*CAM_AWB_CCM_L_4*/
	{0xC89C, 0x0003,},/*CAM_AWB_CCM_L_5*/
	{0xC89E, 0xFF9A,},/*CAM_AWB_CCM_L_6*/
	{0xC8A0, 0xFEB4,},/*CAM_AWB_CCM_L_7*/
	{0xC8A2, 0x024D,},/*CAM_AWB_CCM_L_8*/
	{0xC8A4, 0x01BF,},/*CAM_AWB_CCM_M_0*/
	{0xC8A6, 0xFF01,},/*CAM_AWB_CCM_M_1*/
	{0xC8A8, 0xFFF3,},/*CAM_AWB_CCM_M_2*/
	{0xC8AA, 0xFF75,},/*CAM_AWB_CCM_M_3*/
	{0xC8AC, 0x0198,},/*CAM_AWB_CCM_M_4*/
	{0xC8AE, 0xFFFD,},/*CAM_AWB_CCM_M_5*/
	{0xC8B0, 0xFF9A,},/*CAM_AWB_CCM_M_6*/
	{0xC8B2, 0xFEE7,},/*CAM_AWB_CCM_M_7*/
	{0xC8B4, 0x02A8,},/*CAM_AWB_CCM_M_8*/
	{0xC8B6, 0x01D9,},/*CAM_AWB_CCM_R_0*/
	{0xC8B8, 0xFF26,},/*CAM_AWB_CCM_R_1*/
	{0xC8BA, 0xFFF3,},/*CAM_AWB_CCM_R_2*/
	{0xC8BC, 0xFFB3,},/*CAM_AWB_CCM_R_3*/
	{0xC8BE, 0x0132,},/*CAM_AWB_CCM_R_4*/
	{0xC8C0, 0xFFE8,},/*CAM_AWB_CCM_R_5*/
	{0xC8C2, 0xFFDA,},/*CAM_AWB_CCM_R_6*/
	{0xC8C4, 0xFECD,},/*CAM_AWB_CCM_R_7*/
	{0xC8C6, 0x02C2,},/*CAM_AWB_CCM_R_8*/
	{0xC8C8, 0x0075,},/*CAM_AWB_CCM_L_RG_GAIN*/
	{0xC8CA, 0x011C,},/*CAM_AWB_CCM_L_BG_GAIN*/
	{0xC8CC, 0x009A,},/*CAM_AWB_CCM_M_RG_GAIN*/
	{0xC8CE, 0x0105,},/*CAM_AWB_CCM_M_BG_GAIN*/
	{0xC8D0, 0x00A4,},/*CAM_AWB_CCM_R_RG_GAIN*/
	{0xC8D2, 0x00AC,},/*CAM_AWB_CCM_R_BG_GAIN*/
	{0xC8D4, 0x0A8C,},/*CAM_AWB_CCM_L_CTEMP*/
	{0xC8D6, 0x0F0A,},/*CAM_AWB_CCM_M_CTEMP*/
	{0xC8D8, 0x1964,},/*CAM_AWB_CCM_R_CTEMP*/

	/*[AWB]*/
	{0xC914, 0x0000,},/*CAM_STAT_AWB_CLIP_WINDOW_XSTART*/
	{0xC916, 0x0000,},/*CAM_STAT_AWB_CLIP_WINDOW_YSTART*/
	{0xC918, 0x04FF,},/*CAM_STAT_AWB_CLIP_WINDOW_XEND*/
	{0xC91A, 0x02CF,},/*CAM_STAT_AWB_CLIP_WINDOW_YEND*/
	{0xC904, 0x0033,},/*CAM_AWB_AWB_XSHIFT_PRE_ADJ*/
	{0xC906, 0x0040,},/*CAM_AWB_AWB_YSHIFT_PRE_ADJ*/
	{0xC8F2, 0x03, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_AWB_AWB_XSCALE*/
	{0xC8F3, 0x02, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_AWB_AWB_YSCALE*/
	{0xC906, 0x003C,},/*CAM_AWB_AWB_YSHIFT_PRE_ADJ*/
	{0xC8F4, 0x0000,},/*CAM_AWB_AWB_WEIGHTS_0*/
	{0xC8F6, 0x0000,},/*CAM_AWB_AWB_WEIGHTS_1*/
	{0xC8F8, 0x0000,},/*CAM_AWB_AWB_WEIGHTS_2*/
	{0xC8FA, 0xE724,},/*CAM_AWB_AWB_WEIGHTS_3*/
	{0xC8FC, 0x1583,},/*CAM_AWB_AWB_WEIGHTS_4*/
	{0xC8FE, 0x2045,},/*CAM_AWB_AWB_WEIGHTS_5*/
	{0xC900, 0x03FF,},/*CAM_AWB_AWB_WEIGHTS_6*/
	{0xC902, 0x007C,},/*CAM_AWB_AWB_WEIGHTS_7*/
	{0xC90C, 0x80, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_AWB_K_R_L*/
	{0xC90D, 0x80, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_AWB_K_G_L*/
	{0xC90E, 0x80, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_AWB_K_B_L*/
	{0xC90F, 0x88, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_AWB_K_R_R*/
	{0xC910, 0x80, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_AWB_K_G_R*/
	{0xC911, 0x80, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_AWB_K_B_R*/

	/*[Step7-CPIPE_Preference]*/
	{0xC926, 0x0020,},/*CAM_LL_START_BRIGHTNESS*/
	{0xC928, 0x009A,},/*CAM_LL_STOP_BRIGHTNESS*/
	{0xC946, 0x0070,},/*CAM_LL_START_GAIN_METRIC*/
	{0xC948, 0x00F3,},/*CAM_LL_STOP_GAIN_METRIC*/
	{0xC952, 0x0020,},/*CAM_LL_START_TARGET_LUMA_BM*/
	{0xC954, 0x009A,},/*CAM_LL_STOP_TARGET_LUMA_BM*/
	{0xC92A, 0x80, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_START_SATURATION*/
	{0xC92B, 0x4B, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_END_SATURATION*/
	{0xC92C, 0x00, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_START_DESATURATION*/
	{0xC92D, 0xFF, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_END_DESATURATION*/
	{0xC92E, 0x3C, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_START_DEMOSAIC*/
	{0xC92F, 0x02, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_START_AP_GAIN*/
	{0xC930, 0x06, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_START_AP_THRESH*/
	{0xC931, 0x64, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_STOP_DEMOSAIC*/
	{0xC932, 0x01, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_STOP_AP_GAIN*/
	{0xC933, 0x0C, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_STOP_AP_THRESH*/
	{0xC934, 0x3C, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_START_NR_RED*/
	{0xC935, 0x3C, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_START_NR_GREEN*/
	{0xC936, 0x3C, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_START_NR_BLUE*/
	{0xC937, 0x0F, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_START_NR_THRESH*/
	{0xC938, 0x64, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_STOP_NR_RED*/
	{0xC939, 0x64, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_STOP_NR_GREEN*/
	{0xC93A, 0x64, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_STOP_NR_BLUE*/
	{0xC93B, 0x32, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_STOP_NR_THRESH*/
	{0xC93C, 0x0020,},/*CAM_LL_START_CONTRAST_BM*/
	{0xC93E, 0x009A,},/*CAM_LL_STOP_CONTRAST_BM*/
	{0xC940, 0x00DC,},/*CAM_LL_GAMMA*/
	/*CAM_LL_START_CONTRAST_GRADIENT*/
	{0xC942, 0x38, MSM_CAMERA_I2C_BYTE_DATA},
	/*CAM_LL_STOP_CONTRAST_GRADIENT*/
	{0xC943, 0x30, MSM_CAMERA_I2C_BYTE_DATA},
	{0xC944, 0x50, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_START_CONTRAST_LUMA*/
	{0xC945, 0x19, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_STOP_CONTRAST_LUMA*/
	{0xC94A, 0x0230,},/*CAM_LL_START_FADE_TO_BLACK_LUMA*/
	{0xC94C, 0x0010,},/*CAM_LL_STOP_FADE_TO_BLACK_LUMA*/
	{0xC94E, 0x01CD,},/*CAM_LL_CLUSTER_DC_TH_BM*/
	{0xC950, 0x05, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_CLUSTER_DC_GATE*/
	{0xC951, 0x40, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_LL_SUMMING_SENSITIVITY*/
	/*CAM_AET_TARGET_AVERAGE_LUMA_DARK*/
	{0xC87B, 0x1B, MSM_CAMERA_I2C_BYTE_DATA},
	{0xC878, 0x0E, MSM_CAMERA_I2C_BYTE_DATA},/*CAM_AET_AEMODE*/
	{0xC890, 0x0080,},/*CAM_AET_TARGET_GAIN*/
	{0xC886, 0x0100,},/*CAM_AET_AE_MAX_VIRT_AGAIN*/
	{0xC87C, 0x005A,},/*CAM_AET_BLACK_CLIPPING_TARGET*/
	{0xB42A, 0x05, MSM_CAMERA_I2C_BYTE_DATA},/*CCM_DELTA_GAIN*/
	/*AE_TRACK_AE_TRACKING_DAMPENING*/
	{0xA80A, 0x20, MSM_CAMERA_I2C_BYTE_DATA},
	{0x3C44, 0x0080,},
	{0x3C40, 0x0004, MSM_CAMERA_I2C_UNSET_WORD_MASK},
	{0xA802, 0x08, MSM_CAMERA_I2C_SET_BYTE_MASK},
	{0xC908, 0x01, MSM_CAMERA_I2C_BYTE_DATA},
	{0xC879, 0x01, MSM_CAMERA_I2C_BYTE_DATA},
	{0xC909, 0x01, MSM_CAMERA_I2C_UNSET_BYTE_MASK},
	{0xA80A, 0x18, MSM_CAMERA_I2C_BYTE_DATA},
	{0xA80B, 0x18, MSM_CAMERA_I2C_BYTE_DATA},
	{0xAC16, 0x18, MSM_CAMERA_I2C_BYTE_DATA},
	{0xC878, 0x08, MSM_CAMERA_I2C_SET_BYTE_MASK},
	{0xBC02, 0x08, MSM_CAMERA_I2C_UNSET_BYTE_MASK},
};

static struct v4l2_subdev_info mt9m114_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order    = 0,
	},
};

static struct msm_camera_i2c_reg_conf mt9m114_config_change_settings[] = {
	{0xdc00, 0x28, MSM_CAMERA_I2C_BYTE_DATA, MSM_CAMERA_I2C_CMD_WRITE},
	{MT9M114_COMMAND_REGISTER, MT9M114_COMMAND_REGISTER_SET_STATE,
		MSM_CAMERA_I2C_UNSET_WORD_MASK, MSM_CAMERA_I2C_CMD_POLL},
	{MT9M114_COMMAND_REGISTER, (MT9M114_COMMAND_REGISTER_OK |
		MT9M114_COMMAND_REGISTER_SET_STATE), MSM_CAMERA_I2C_WORD_DATA,
		MSM_CAMERA_I2C_CMD_WRITE},
	{MT9M114_COMMAND_REGISTER, MT9M114_COMMAND_REGISTER_SET_STATE,
		MSM_CAMERA_I2C_UNSET_WORD_MASK, MSM_CAMERA_I2C_CMD_POLL},
	{0xDC01, 0x31, MSM_CAMERA_I2C_BYTE_DATA},
};

static const struct i2c_device_id mt9m114_i2c_id[] = {
	{MT9M114_SENSOR_NAME, (kernel_ulong_t)&mt9m114_s_ctrl},
	{ }
};

static int32_t msm_mt9m114_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	return msm_sensor_i2c_probe(client, id, &mt9m114_s_ctrl);
}

static struct i2c_driver mt9m114_i2c_driver = {
	.id_table = mt9m114_i2c_id,
	.probe  = msm_mt9m114_i2c_probe,
	.driver = {
		.name = MT9M114_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client mt9m114_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

static const struct of_device_id mt9m114_dt_match[] = {
	{.compatible = "qcom,mt9m114", .data = &mt9m114_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, mt9m114_dt_match);

static struct platform_driver mt9m114_platform_driver = {
	.driver = {
		.name = "qcom,mt9m114",
		.owner = THIS_MODULE,
		.of_match_table = mt9m114_dt_match,
	},
};

static int32_t mt9m114_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	match = of_match_device(mt9m114_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init mt9m114_init_module(void)
{
	int32_t rc;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&mt9m114_platform_driver,
		mt9m114_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&mt9m114_i2c_driver);
}

static void __exit mt9m114_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (mt9m114_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&mt9m114_s_ctrl);
		platform_driver_unregister(&mt9m114_platform_driver);
	} else
		i2c_del_driver(&mt9m114_i2c_driver);
	return;
}

int32_t mt9m114_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
	void __user *argp)
{
	struct sensorb_cfg_data *cdata = (struct sensorb_cfg_data *)argp;
	long rc = 0;
	int32_t i = 0;
	mutex_lock(s_ctrl->msm_sensor_mutex);
	CDBG("%s:%d %s cfgtype = %d\n", __func__, __LINE__,
		s_ctrl->sensordata->sensor_name, cdata->cfgtype);
	switch (cdata->cfgtype) {
	case CFG_GET_SENSOR_INFO:
		memcpy(cdata->cfg.sensor_info.sensor_name,
			s_ctrl->sensordata->sensor_name,
			sizeof(cdata->cfg.sensor_info.sensor_name));
		cdata->cfg.sensor_info.session_id =
			s_ctrl->sensordata->sensor_info->session_id;
		for (i = 0; i < SUB_MODULE_MAX; i++)
			cdata->cfg.sensor_info.subdev_id[i] =
				s_ctrl->sensordata->sensor_info->subdev_id[i];
		cdata->cfg.sensor_info.is_mount_angle_valid =
			s_ctrl->sensordata->sensor_info->is_mount_angle_valid;
		cdata->cfg.sensor_info.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d sensor name %s\n", __func__, __LINE__,
			cdata->cfg.sensor_info.sensor_name);
		CDBG("%s:%d session id %d\n", __func__, __LINE__,
			cdata->cfg.sensor_info.session_id);
		for (i = 0; i < SUB_MODULE_MAX; i++)
			CDBG("%s:%d subdev_id[%d] %d\n", __func__, __LINE__, i,
				cdata->cfg.sensor_info.subdev_id[i]);
		CDBG("%s:%d mount angle valid %d value %d\n", __func__,
			__LINE__, cdata->cfg.sensor_info.is_mount_angle_valid,
			cdata->cfg.sensor_info.sensor_mount_angle);

		break;
	case CFG_SET_INIT_SETTING:
		/* 1. Write Recommend settings */
		/* 2. Write change settings */
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, mt9m114_recommend_settings,
			ARRAY_SIZE(mt9m114_recommend_settings),
			MSM_CAMERA_I2C_WORD_DATA);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client,
			mt9m114_config_change_settings,
			ARRAY_SIZE(mt9m114_config_change_settings),
			MSM_CAMERA_I2C_WORD_DATA);
		break;
	case CFG_SET_RESOLUTION:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client, mt9m114_720p_settings,
			ARRAY_SIZE(mt9m114_720p_settings),
			MSM_CAMERA_I2C_WORD_DATA);
		break;
	case CFG_SET_STOP_STREAM:
		break;
	case CFG_SET_START_STREAM:
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(
			s_ctrl->sensor_i2c_client,
			mt9m114_config_change_settings,
			ARRAY_SIZE(mt9m114_config_change_settings),
			MSM_CAMERA_I2C_WORD_DATA);
		break;
	case CFG_GET_SENSOR_INIT_PARAMS:
		cdata->cfg.sensor_init_params.modes_supported =
			s_ctrl->sensordata->sensor_info->modes_supported;
		cdata->cfg.sensor_init_params.position =
			s_ctrl->sensordata->sensor_info->position;
		cdata->cfg.sensor_init_params.sensor_mount_angle =
			s_ctrl->sensordata->sensor_info->sensor_mount_angle;
		CDBG("%s:%d init params mode %d pos %d mount %d\n", __func__,
			__LINE__,
			cdata->cfg.sensor_init_params.modes_supported,
			cdata->cfg.sensor_init_params.position,
			cdata->cfg.sensor_init_params.sensor_mount_angle);
		break;
	case CFG_SET_SLAVE_INFO: {
		struct msm_camera_sensor_slave_info sensor_slave_info;
		struct msm_camera_power_ctrl_t *p_ctrl;
		uint16_t size;
		int slave_index = 0;
		if (copy_from_user(&sensor_slave_info,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_sensor_slave_info))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		/* Update sensor slave address */
		if (sensor_slave_info.slave_addr) {
			s_ctrl->sensor_i2c_client->cci_client->sid =
				sensor_slave_info.slave_addr >> 1;
		}

		/* Update sensor address type */
		s_ctrl->sensor_i2c_client->addr_type =
			sensor_slave_info.addr_type;

		/* Update power up / down sequence */
		p_ctrl = &s_ctrl->sensordata->power_info;
		size = sensor_slave_info.power_setting_array.size;
		if (p_ctrl->power_setting_size < size) {
			struct msm_sensor_power_setting *tmp;
			tmp = kmalloc(sizeof(struct msm_sensor_power_setting)
				      * size, GFP_KERNEL);
			if (!tmp) {
				pr_err("%s: failed to alloc mem\n", __func__);
				rc = -ENOMEM;
				break;
			}
			kfree(p_ctrl->power_setting);
			p_ctrl->power_setting = tmp;
		}
		p_ctrl->power_setting_size = size;

		rc = copy_from_user(p_ctrl->power_setting, (void *)
			sensor_slave_info.power_setting_array.power_setting,
			size * sizeof(struct msm_sensor_power_setting));
		if (rc) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.slave_addr);
		CDBG("%s sensor addr type %d\n", __func__,
			sensor_slave_info.addr_type);
		CDBG("%s sensor reg %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id_reg_addr);
		CDBG("%s sensor id %x\n", __func__,
			sensor_slave_info.sensor_id_info.sensor_id);
		for (slave_index = 0; slave_index <
			p_ctrl->power_setting_size; slave_index++) {
			CDBG("%s i %d power setting %d %d %ld %d\n", __func__,
				slave_index,
				p_ctrl->power_setting[slave_index].seq_type,
				p_ctrl->power_setting[slave_index].seq_val,
				p_ctrl->power_setting[slave_index].config_val,
				p_ctrl->power_setting[slave_index].delay);
		}
		break;
	}
	case CFG_WRITE_I2C_ARRAY: {
		struct msm_camera_i2c_reg_setting conf_array;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if ((!conf_array.size) ||
			(conf_array.size > I2C_SEQ_REG_DATA_MAX)) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write_table(
			s_ctrl->sensor_i2c_client, &conf_array);
		kfree(reg_setting);
		break;
	}
	case CFG_WRITE_I2C_SEQ_ARRAY: {
		struct msm_camera_i2c_seq_reg_setting conf_array;
		struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;

		if (copy_from_user(&conf_array,
			(void *)cdata->cfg.setting,
			sizeof(struct msm_camera_i2c_seq_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		if ((!conf_array.size) ||
			(conf_array.size > I2C_SEQ_REG_DATA_MAX)) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}
		reg_setting = kzalloc(conf_array.size *
			(sizeof(struct msm_camera_i2c_seq_reg_array)),
			GFP_KERNEL);
		if (!reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(reg_setting, (void *)conf_array.reg_setting,
			conf_array.size *
			sizeof(struct msm_camera_i2c_seq_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(reg_setting);
			rc = -EFAULT;
			break;
		}

		conf_array.reg_setting = reg_setting;
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_seq_table(s_ctrl->sensor_i2c_client,
			&conf_array);
		kfree(reg_setting);
		break;
	}

	case CFG_POWER_UP:
		if (s_ctrl->func_tbl->sensor_power_up)
			rc = s_ctrl->func_tbl->sensor_power_up(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_POWER_DOWN:
		if (s_ctrl->func_tbl->sensor_power_down)
			rc = s_ctrl->func_tbl->sensor_power_down(s_ctrl);
		else
			rc = -EFAULT;
		break;

	case CFG_SET_STOP_STREAM_SETTING: {
		struct msm_camera_i2c_reg_setting *stop_setting =
			&s_ctrl->stop_setting;
		struct msm_camera_i2c_reg_array *reg_setting = NULL;
		if (copy_from_user(stop_setting, (void *)cdata->cfg.setting,
		    sizeof(struct msm_camera_i2c_reg_setting))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -EFAULT;
			break;
		}

		reg_setting = stop_setting->reg_setting;
		stop_setting->reg_setting = kzalloc(stop_setting->size *
			(sizeof(struct msm_camera_i2c_reg_array)), GFP_KERNEL);
		if (!stop_setting->reg_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(stop_setting->reg_setting,
		    (void *)reg_setting, stop_setting->size *
		    sizeof(struct msm_camera_i2c_reg_array))) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			kfree(stop_setting->reg_setting);
			stop_setting->reg_setting = NULL;
			stop_setting->size = 0;
			rc = -EFAULT;
			break;
		}
		break;
		}
		case CFG_SET_SATURATION: {
			int32_t sat_lev;
			if (copy_from_user(&sat_lev, (void *)cdata->cfg.setting,
				sizeof(int32_t))) {
				pr_err("%s:%d failed\n", __func__, __LINE__);
				rc = -EFAULT;
				break;
			}
		pr_debug("%s: Saturation Value is %d", __func__, sat_lev);
		break;
		}
		case CFG_SET_CONTRAST: {
			int32_t con_lev;
			if (copy_from_user(&con_lev, (void *)cdata->cfg.setting,
				sizeof(int32_t))) {
				pr_err("%s:%d failed\n", __func__, __LINE__);
				rc = -EFAULT;
				break;
			}
		pr_debug("%s: Contrast Value is %d", __func__, con_lev);
		break;
		}
		case CFG_SET_SHARPNESS: {
			int32_t shp_lev;
			if (copy_from_user(&shp_lev, (void *)cdata->cfg.setting,
				sizeof(int32_t))) {
				pr_err("%s:%d failed\n", __func__, __LINE__);
				rc = -EFAULT;
				break;
			}
		pr_debug("%s: Sharpness Value is %d", __func__, shp_lev);
		break;
		}
		case CFG_SET_AUTOFOCUS: {
		/* TO-DO: set the Auto Focus */
		pr_debug("%s: Setting Auto Focus", __func__);
		break;
		}
		case CFG_CANCEL_AUTOFOCUS: {
		/* TO-DO: Cancel the Auto Focus */
		pr_debug("%s: Cancelling Auto Focus", __func__);
		break;
		}
		default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

static struct msm_sensor_fn_t mt9m114_sensor_func_tbl = {
	.sensor_config = mt9m114_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = msm_sensor_match_id,
};

static struct msm_sensor_ctrl_t mt9m114_s_ctrl = {
	.sensor_i2c_client = &mt9m114_sensor_i2c_client,
	.power_setting_array.power_setting = mt9m114_power_setting,
	.power_setting_array.size = ARRAY_SIZE(mt9m114_power_setting),
	.msm_sensor_mutex = &mt9m114_mut,
	.sensor_v4l2_subdev_info = mt9m114_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(mt9m114_subdev_info),
	.func_tbl = &mt9m114_sensor_func_tbl,
};

module_init(mt9m114_init_module);
module_exit(mt9m114_exit_module);
MODULE_DESCRIPTION("Aptina 1.26MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
