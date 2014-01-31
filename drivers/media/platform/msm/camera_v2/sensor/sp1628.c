/* Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
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

#define CONFIG_MSMB_CAMERA_DEBUG

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif

#define SP1628_SENSOR_NAME "sp1628"
DEFINE_MSM_MUTEX(sp1628_mut);

static struct msm_sensor_ctrl_t sp1628_s_ctrl;

static struct msm_sensor_power_setting sp1628_power_setting[] = {
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VDIG,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VIO,
		.config_val = 0,
		.delay = 0,
	},
	{
		.seq_type = SENSOR_VREG,
		.seq_val = CAM_VANA,
		.config_val = 0,
		.delay = 5,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_HIGH,
		.delay = 50,
	},

	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 50,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_LOW,
		.delay = 50,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_RESET,
		.config_val = GPIO_OUT_HIGH,
		.delay = 50,
	},
	{
		.seq_type = SENSOR_GPIO,
		.seq_val = SENSOR_GPIO_STANDBY,
		.config_val = GPIO_OUT_LOW,
		.delay = 50,
	},
	{
		.seq_type = SENSOR_CLK,
		.seq_val = SENSOR_CAM_MCLK,
		.config_val = 0,
		.delay = 1,
	},

	{
		.seq_type = SENSOR_I2C_MUX,
		.seq_val = 0,
		.config_val = 0,
		.delay = 5,
	},
};

static struct msm_camera_i2c_reg_conf sp1628_start_settings[] = {
	{0x92, 0x81},
};

static struct msm_camera_i2c_reg_conf sp1628_stop_settings[] = {
	{0x92, 0x01},
};

static struct msm_camera_i2c_reg_conf sp1628_recommend_settings[] = {
	{0xfd, 0x00,},
	{0x91, 0x00,},
	{0x92, 0x81,},
	{0x98, 0x2a,},
	{0x96, 0xd0,}, /* c0*/
	{0x97, 0x02,}, /* 03*/
	{0x2f, 0x20,},	/* 24M*3=72M*/
	{0x0b, 0x48,},	/* analog*/
	{0x30, 0x80,},	/* 00*/
	{0x0c, 0x66,},	/* analog*/
	{0x0d, 0x12,},
	{0x13, 0x0f,},	/* 10*/
	{0x14, 0x00,},
	{0x12, 0x00,},
	{0x6b, 0x10,},	/* 11*/
	{0x6c, 0x00,},
	{0x6d, 0x00,},
	{0x6e, 0x00,},
	{0x6f, 0x10,},	/* 11*/
	{0x73, 0x11,},	/* 12*/
	{0x7a, 0x10,},	/* 11*/
	{0x15, 0x17,},	/* 18*/
	{0x71, 0x18,},	/* 19*/
	{0x76, 0x18,},	/* 19*/
	{0x29, 0x08,},
	{0x18, 0x01,},
	{0x19, 0x10,},
	{0x1a, 0xc3,},	/* c1*/
	{0x1b, 0x6f,},
	{0x1d, 0x11,},	/* 01*/
	{0x1e, 0x00,},	/* 1e*/
	{0x1f, 0x80,},
	{0x20, 0x7f,},
	{0x22, 0x3c,},	/* 1b*/
	{0x25, 0xff,},
	{0x2b, 0x88,},
	{0x2c, 0x85,},
	{0x2d, 0x00,},
	{0x2e, 0x80,},
	{0x27, 0x38,},
	{0x28, 0x03,},
	{0x70, 0x1a,},
	{0x72, 0x18,},	/* 1a*/
	{0x74, 0x18,},
	{0x75, 0x18,},
	{0x77, 0x16,},	/* 18*/
	{0x7f, 0x19,},
	{0x31, 0x71,},	/*70 mirror/flip 720P*/
	{0xfd, 0x01,},
	{0x5d, 0x11,},	/* position*/
	{0x5f, 0x00,},
	{0x36, 0x08,},
	{0x2f, 0xff,},
	{0xfb, 0x25,},	/* blacklevl*/
	{0x48, 0x00,},	/* dp*/
	{0x49, 0x99,},
	{0xf2, 0x0A,},
	{0xfd, 0x02,},	/* AE*/
	{0x52, 0x34,},
	{0x53, 0x02,},
	{0x54, 0x0c,},
	{0x55, 0x08,},
	{0x86, 0x0c,},
	{0x87, 0x10,},
	{0x8b, 0x10,},

	/* 12-30 50Hz*/
	{0xfd, 0x00,},	/* ae setting*/
	{0x03, 0x05,},
	{0x04, 0x64,},
	{0x05, 0x00,},
	{0x06, 0x00,},
	{0x09, 0x00,},
	{0x0a, 0x02,},
	{0xfd, 0x01,},
	{0xf0, 0x00,},
	{0xf7, 0xe6,},
	{0xf8, 0xc1,},
	{0x02, 0x08,},
	{0x03, 0x01,},
	{0x06, 0xe6,},
	{0x07, 0x00,},
	{0x08, 0x01,},
	{0x09, 0x00,},
	{0xfd, 0x02,},
	{0x40, 0x0a,},
	{0x41, 0xc1,},
	{0x42, 0x00,},
	{0x88, 0x37,},
	{0x89, 0xa7,},
	{0x8a, 0x22,},
	{0xfd, 0x02,},	/* Status*/
	{0xbe, 0x30,},
	{0xbf, 0x07,},
	{0xd0, 0x30,},
	{0xd1, 0x07,},
	{0xfd, 0x01,},
	{0x5b, 0x07,},
	{0x5c, 0x30,},
	{0xfd, 0x00,},

	/* 12-30	60Hz*/
	{0xfd, 0x00,},	/* ae setting*/
	{0x03, 0x04,},
	{0x04, 0x80,},
	{0x05, 0x00,},
	{0x06, 0x00,},
	{0x09, 0x00,},
	{0x0a, 0x01,},
	{0xfd, 0x01,},
	{0xf0, 0x00,},
	{0xf7, 0xc0,},
	{0xf8, 0xc1,},
	{0x02, 0x0a,},
	{0x03, 0x01,},
	{0x06, 0xc0,},
	{0x07, 0x00,},
	{0x08, 0x01,},
	{0x09, 0x00,},
	{0xfd, 0x02,},
	{0x40, 0x0a,},
	{0x41, 0xc1,},
	{0x42, 0x00,},
	{0x88, 0x37,},
	{0x89, 0xa7,},
	{0x8a, 0x22,},
	{0xfd, 0x02,},	/* Status*/
	{0xbe, 0x80,},
	{0xbf, 0x07,},
	{0xd0, 0x80,},
	{0xd1, 0x07,},
	{0xfd, 0x01,},
	{0x5b, 0x07,},
	{0x5c, 0x80,},
	{0xfd, 0x00,},

	{0xfd, 0x01,},	/* fix status*/
	{0x5a, 0x38,},	/* DP_gain*/
	{0xfd, 0x02,},
	{0xba, 0x30,},	/* mean_dummy_low*/
	{0xbb, 0x50,},	/* mean_low_dummy*/
	{0xbc, 0xc0,},	/* rpc_heq_low*/
	{0xbd, 0xa0,},	/* rpc_heq_dummy*/
	{0xb8, 0x80,},	/* mean_nr_dummy*/
	{0xb9, 0x90,},	/* mean_dummy_nr*/
	{0xfd, 0x01,},	/* rpc*/
	{0xe0, 0x54,},
	{0xe1, 0x40,},
	{0xe2, 0x38,},
	{0xe3, 0x34,},
	{0xe4, 0x34,},
	{0xe5, 0x30,},
	{0xe6, 0x30,},
	{0xe7, 0x2e,},
	{0xe8, 0x2e,},
	{0xe9, 0x2e,},
	{0xea, 0x2c,},
	{0xf3, 0x2c,},
	{0xf4, 0x2c,},
	{0xfd, 0x01,},	/* min gain*/
	{0x04, 0xc0,},	/* rpc_max_indr*/
	{0x05, 0x2c,},	/* rpc_min_indr*/
	{0x0a, 0xc0,},	/* rpc_max_outdr*/
	{0x0b, 0x2c,},	/* rpc_min_outdr*/
	{0xfd, 0x01,},	/* ae target*/
	{0xeb, 0x78,},
	{0xec, 0x78,},
	{0xed, 0x05,},
	{0xee, 0x0a,},
	{0xfd, 0x01,},	/* lsc*/
	{0x26, 0x30,},
	{0x27, 0xdc,},
	{0x28, 0x05,},
	{0x29, 0x08,},
	{0x2a, 0x00,},
	{0x2b, 0x03,},
	{0x2c, 0x00,},
	{0x2d, 0x2f,},
	{0xfd, 0x01,},	/* RGainf*/
	{0xa1, 0x37,},	/* left*/
	{0xa2, 0x26,},	/* right*/
	{0xa3, 0x32,},	/* up*/
	{0xa4, 0x2b,},	/* down*/
	{0xad, 0x0f,},	/* lu*/
	{0xae, 0x0a,},	/* ru*/
	{0xaf, 0x0a,},	/* ld*/
	{0xb0, 0x0a,},	/* rd*/
	{0x18, 0x2f,},	/* left*/
	{0x19, 0x30,},	/* right*/
	{0x1a, 0x32,},	/* up*/
	{0x1b, 0x30,},	/* down*/
	{0xbf, 0xa5,},	/* lu*/
	{0xc0, 0x12,},	/* ru*/
	{0xc1, 0x08,},	/* ld*/
	{0xfa, 0x00,},	/* rd*/
	{0xa5, 0x35,},	/* GGain*/
	{0xa6, 0x24,},
	{0xa7, 0x2e,},
	{0xa8, 0x25,},
	{0xb1, 0x00,},
	{0xb2, 0x04,},
	{0xb3, 0x00,},
	{0xb4, 0x00,},
	{0x1c, 0x24,},
	{0x1d, 0x23,},
	{0x1e, 0x2c,},
	{0xb9, 0x25,},
	{0x21, 0xa0,},
	{0x22, 0x13,},
	{0x23, 0x1c,},
	{0x24, 0x0d,},
	{0xa9, 0x2f,},	/* BGain*/
	{0xaa, 0x24,},
	{0xab, 0x2d,},
	{0xac, 0x24,},
	{0xb5, 0x00,},
	{0xb6, 0x00,},
	{0xb7, 0x00,},
	{0xb8, 0x00,},
	{0xba, 0x22,},
	{0xbc, 0x24,},
	{0xbd, 0x31,},
	{0xbe, 0x24,},
	{0x25, 0xa0,},
	{0x45, 0x08,},
	{0x46, 0x12,},
	{0x47, 0x09,},
	{0xfd, 0x01,},	/* awb*/
	{0x32, 0x15,},
	{0xfd, 0x02,},
	{0x26, 0xc9,},
	{0x27, 0x8b,},
	{0x1b, 0x80,},
	{0x1a, 0x80,},
	{0x18, 0x27,},
	{0x19, 0x26,},
	{0x2a, 0x01,},
	{0x2b, 0x10,},
	{0x28, 0xf8,},
	{0x29, 0x08,},

	/* d65*/
	{0x66, 0x35,},
	{0x67, 0x60,},
	{0x68, 0xb0,},
	{0x69, 0xe0,},
	{0x6a, 0xa5,},

	/* indoor*/
	{0x7c, 0x38,},
	{0x7d, 0x58,},
	{0x7e, 0xdb,},
	{0x7f, 0x13,},
	{0x80, 0xa6,},

	/* cwftl84*/
	{0x70, 0x18,},	/* 2f*/
	{0x71, 0x4a,},
	{0x72, 0x08,},
	{0x73, 0x32,},	/* 24*/
	{0x74, 0xaa,},

	/* tl84--F*/
	{0x6b, 0x02,},	/* 18*/
	{0x6c, 0x2a,},	/* 34*/
	{0x6d, 0x1e,},	/* 17*/
	{0x6e, 0x49,},	/* 32*/
	{0x6f, 0xaa,},

	/* f--H*/
	{0x61, 0xea,},	/* 02*/
	{0x62, 0xf8,},	/* 2a*/
	{0x63, 0x4f,},	/* 1e*/
	{0x64, 0x5f,},	/* 49*/
	{0x65, 0x5a,},	/* aa*/

	{0x75, 0x80,},
	{0x76, 0x09,},
	{0x77, 0x02,},
	{0x24, 0x25,},
	{0x0e, 0x16,},
	{0x3b, 0x09,},
	{0xfd, 0x02,},	/*	sharp*/
	{0xde, 0x0f,},
	{0xd2, 0x0c,},	/* control black-white edge; 0 - bolder, f - thinner*/
	{0xd3, 0x0a,},
	{0xd4, 0x08,},
	{0xd5, 0x08,},
	{0xd7, 0x10,},	/* outline judgement*/
	{0xd8, 0x1d,},
	{0xd9, 0x32,},
	{0xda, 0x48,},
	{0xdb, 0x08,},
	{0xe8, 0x38,},	/* outline strength*/
	{0xe9, 0x38,},
	{0xea, 0x38,},	/* 30*/
	{0xeb, 0x38,},	/* 2*/
	{0xec, 0x60,},
	{0xed, 0x40,},
	{0xee, 0x38,},	/* 30*/
	{0xef, 0x38,},	/* 20*/
	{0xf3, 0x00,},	/* sharpness level of flat area*/
	{0xf4, 0x00,},
	{0xf5, 0x00,},
	{0xf6, 0x00,},
	{0xfd, 0x02,},	/* skin sharpen*/
	{0xdc, 0x04,},	/* skin de-sharpen*/
	{0x05, 0x6f,},
	{0x09, 0x10,},
	{0xfd, 0x01,},	/* dns*/
	{0x64, 0x22,},	/* 0 - max, 8 - min*/
	{0x65, 0x22,},
	{0x86, 0x20,},	/* threshold, 0 - min*/
	{0x87, 0x20,},
	{0x88, 0x20,},
	{0x89, 0x20,},
	{0x6d, 0x0f,},
	{0x6e, 0x0f,},
	{0x6f, 0x10,},
	{0x70, 0x10,},
	{0x71, 0x0d,},
	{0x72, 0x23,},
	{0x73, 0x23,},	/* 28*/
	{0x74, 0x23,},	/* 2a*/
	{0x75, 0x46,},	/* [7:4] strength of flat area,
					[3:0]strength of un-flat area;
					0-max, 8-min*/
	{0x76, 0x36,},
	{0x77, 0x36,},	/* 25*/
	{0x78, 0x36,},	/* 12*/
	{0x81, 0x1d,},	/* 2x*/
	{0x82, 0x2b,},	/* 4x*/
	{0x83, 0x2b,},	/* 50; 8x*/
	{0x84, 0x2b,},	/* 80; 16x*/
	{0x85, 0x0a,},	/* 12/8reg0x81*/
	{0xfd, 0x01,},	/* gamma*/
	{0x8b, 0x00,},	/* 00; 00; 00;*/
	{0x8c, 0x0d,},	/* 02; 0b; 0b;*/
	{0x8d, 0x1f,},	/* 0a; 19; 17;*/
	{0x8e, 0x2d,},	/* 13; 2a; 27;*/
	{0x8f, 0x3a,},	/* 1d; 37; 35;*/
	{0x90, 0x4b,},	/* 30; 4b; 51;*/
	{0x91, 0x59,},	/* 40; 5e; 64;*/
	{0x92, 0x64,},	/* 4e; 6c; 74;*/
	{0x93, 0x70,},	/* 5a; 78; 80;*/
	{0x94, 0x83,},	/* 71; 92; 92;*/
	{0x95, 0x92,},	/* 85; a6; a2;*/
	{0x96, 0xa1,},	/* 96; b5; af;*/
	{0x97, 0xae,},	/* a6; bf; bb;*/
	{0x98, 0xba,},	/* b3; ca; c6;*/
	{0x99, 0xc4,},	/* c0; d2; d0;*/
	{0x9a, 0xcf,},	/* cb; d9; d9;*/
	{0x9b, 0xdb,},	/* d5; e1; e0;*/
	{0x9c, 0xe5,},	/* df; e8; e8;*/
	{0x9d, 0xec,},	/* e9; ee; ee;*/
	{0x9e, 0xf3,},	/* f2; f4; f4;*/
	{0x9f, 0xfa,},	/* fa; fa; fa;*/
	{0xa0, 0xff,},	/* ff; ff; ff;*/
	{0xfd, 0x02,},	/* CCM*/
	{0x15, 0xc8,},	/* b>th ab*/
	{0x16, 0x95,},	/* r<th 87*/

	{0xa0, 0x8c,},	/* !F*/
	{0xa1, 0xfa,},
	{0xa2, 0xfa,},
	{0xa3, 0xf4,},
	{0xa4, 0x99,},
	{0xa5, 0xf4,},
	{0xa6, 0x00,},
	{0xa7, 0xb4,},
	{0xa8, 0xcc,},
	{0xa9, 0x3c,},
	{0xaa, 0x33,},
	{0xab, 0x0c,},

	{0xac, 0x80,}, /* F*/
	{0xad, 0x00,},
	{0xae, 0x00,},
	{0xaf, 0xe7,},
	{0xb0, 0xc0,},
	{0xb1, 0xda,},
	{0xb2, 0xe7,},
	{0xb3, 0xb4,},
	{0xb4, 0xe6,},
	{0xb5, 0x00,},
	{0xb6, 0x33,},
	{0xb7, 0x0f,},
	{0xfd, 0x01,},	/* sat u*/
	{0xd3, 0x8a,},	/* 90 105%*/
	{0xd4, 0x8a,},	/* 90*/
	{0xd5, 0x88,},
	{0xd6, 0x80,},
	{0xd7, 0x8a,},	/* 90; sat v*/
	{0xd8, 0x8a,},	/* 90*/
	{0xd9, 0x88,},
	{0xda, 0x80,},
	{0xfd, 0x01,},	/* auto_sat*/
	{0xd2, 0x00,},	/* autosa_en*/
	{0xfd, 0x01,},	/* uv_th*/
	{0xc2, 0xee,},
	{0xc3, 0xee,},
	{0xc4, 0xdd,},
	{0xc5, 0xbb,},
	{0xfd, 0x01,},	/* low_lum_offset*/
	{0xcd, 0x10,},
	{0xce, 0x1f,},
	{0xfd, 0x02,},	/* gw*/
	{0x35, 0x6f,},
	{0x37, 0x13,},
	{0xfd, 0x01,},	/* heq*/
	{0xdb, 0x00,},
	{0x10, 0x00,},
	{0x14, 0x25,},
	{0x11, 0x10,},
	{0x15, 0x25,},
	{0x16, 0x15,},
	{0xfd, 0x02,},	/* cnr*/
	{0x8e, 0x10,},
	{0x90, 0x20,},
	{0x91, 0x20,},
	{0x92, 0x60,},
	{0x93, 0x80,},
	{0xfd, 0x02,},	/* auto*/
	{0x85, 0x00,},
	{0xfd, 0x01,},
	{0x00, 0x00,},	/* fix mode*/
	{0x32, 0x15,},	/* ae en*/
	{0x33, 0xef,},	/* lsc\bpc en*/
	{0x34, 0xc7,},	/* ynr\cnr\gamma\color en*/
	{0x35, 0x40,},	/* YUYV*/
	{0xfd, 0x00,},

};

static struct v4l2_subdev_info sp1628_subdev_info[] = {
	{
		.code   = V4L2_MBUS_FMT_YUYV8_2X8,
		.colorspace = V4L2_COLORSPACE_JPEG,
		.fmt    = 1,
		.order  = 0,
	},
};

static const struct i2c_device_id sp1628_i2c_id[] = {
	{SP1628_SENSOR_NAME, (kernel_ulong_t)&sp1628_s_ctrl},
	{ }
};

static int32_t msm_sp1628_i2c_probe(struct i2c_client *client,
	const struct i2c_device_id *id)
{
	CDBG("%s, E.", __func__);

	return msm_sensor_i2c_probe(client, id, &sp1628_s_ctrl);
}

static struct i2c_driver sp1628_i2c_driver = {
	.id_table = sp1628_i2c_id,
	.probe  = msm_sp1628_i2c_probe,
	.driver = {
		.name = SP1628_SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client sp1628_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_BYTE_ADDR,
};

static const struct of_device_id sp1628_dt_match[] = {
	{.compatible = "qcom,sp1628", .data = &sp1628_s_ctrl},
	{}
};

MODULE_DEVICE_TABLE(of, sp1628_dt_match);

static struct platform_driver sp1628_platform_driver = {
	.driver = {
		.name = "qcom,sp1628",
		.owner = THIS_MODULE,
		.of_match_table = sp1628_dt_match,
	},
};

static int32_t sp1628_platform_probe(struct platform_device *pdev)
{
	int32_t rc;
	const struct of_device_id *match;
	CDBG("%s, E.", __func__);
	match = of_match_device(sp1628_dt_match, &pdev->dev);
	rc = msm_sensor_platform_probe(pdev, match->data);
	return rc;
}

static int __init sp1628_init_module(void)
{
	int32_t rc;
	pr_info("%s:%d\n", __func__, __LINE__);
	rc = platform_driver_probe(&sp1628_platform_driver,
		sp1628_platform_probe);
	if (!rc)
		return rc;
	pr_err("%s:%d rc %d\n", __func__, __LINE__, rc);
	return i2c_add_driver(&sp1628_i2c_driver);
}

static void __exit sp1628_exit_module(void)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	if (sp1628_s_ctrl.pdev) {
		msm_sensor_free_sensor_data(&sp1628_s_ctrl);
		platform_driver_unregister(&sp1628_platform_driver);
	} else
		i2c_del_driver(&sp1628_i2c_driver);
	return;
}

int32_t sp1628_sensor_config(struct msm_sensor_ctrl_t *s_ctrl,
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
		/* Write Recommend settings */
		pr_err("%s, sensor write init setting!!", __func__);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(s_ctrl->sensor_i2c_client,
			sp1628_recommend_settings,
			ARRAY_SIZE(sp1628_recommend_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CFG_SET_RESOLUTION:
		break;
	case CFG_SET_STOP_STREAM:
		pr_err("%s, sensor stop stream!!", __func__);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(s_ctrl->sensor_i2c_client,
			sp1628_stop_settings,
			ARRAY_SIZE(sp1628_stop_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
		break;
	case CFG_SET_START_STREAM:
		pr_err("%s, sensor start stream!!", __func__);
		rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->
			i2c_write_conf_tbl(s_ctrl->sensor_i2c_client,
			sp1628_start_settings,
			ARRAY_SIZE(sp1628_start_settings),
			MSM_CAMERA_I2C_BYTE_DATA);
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
		struct msm_sensor_power_setting_array *power_setting_array;
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
		s_ctrl->power_setting_array =
			sensor_slave_info.power_setting_array;
		power_setting_array = &s_ctrl->power_setting_array;
		power_setting_array->power_setting = kzalloc(
			power_setting_array->size *
			sizeof(struct msm_sensor_power_setting), GFP_KERNEL);
		if (!power_setting_array->power_setting) {
			pr_err("%s:%d failed\n", __func__, __LINE__);
			rc = -ENOMEM;
			break;
		}
		if (copy_from_user(power_setting_array->power_setting,
		    (void *)sensor_slave_info.power_setting_array.power_setting,
		    power_setting_array->size *
		    sizeof(struct msm_sensor_power_setting))) {
			kfree(power_setting_array->power_setting);
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
			power_setting_array->size; slave_index++) {
			CDBG("%s i %d power setting %d %d %ld %d\n", __func__,
				slave_index,
				power_setting_array->power_setting[slave_index].
				seq_type,
				power_setting_array->power_setting[slave_index].
				seq_val,
				power_setting_array->power_setting[slave_index].
				config_val,
				power_setting_array->power_setting[slave_index].
				delay);
		}
		kfree(power_setting_array->power_setting);
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
			rc = s_ctrl->func_tbl->sensor_power_down(
				s_ctrl);
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

		break;
	}
	case CFG_SET_CONTRAST: {

		break;
	}
	case CFG_SET_SHARPNESS: {

		break;
	}
	case CFG_SET_ISO: {

		break;
	}
	case CFG_SET_EXPOSURE_COMPENSATION: {

		break;
	}
	case CFG_SET_EFFECT: {

		break;
	}
	case CFG_SET_ANTIBANDING: {

		break;
	}
	case CFG_SET_BESTSHOT_MODE: {

		break;
	}
	case CFG_SET_WHITE_BALANCE: {

		break;
	}
	default:
		rc = -EFAULT;
		break;
	}

	mutex_unlock(s_ctrl->msm_sensor_mutex);

	return rc;
}

int32_t sp1628_match_id(struct msm_sensor_ctrl_t *s_ctrl)
{
	int32_t rc = 0;
	uint16_t chipid = 0;

	CDBG("%s, E. calling i2c_read:, i2c_addr:%d, id_reg_addr:%d",
		__func__,
		s_ctrl->sensordata->slave_info->sensor_slave_addr,
		s_ctrl->sensordata->slave_info->sensor_id_reg_addr);

	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			0x02,
			&chipid, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	CDBG("%s: read id: %x expected id 0x16:\n", __func__, chipid);
	if (chipid != 0x16) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}

	chipid = 0;
	rc = s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
			s_ctrl->sensor_i2c_client,
			0xa0,
			&chipid, MSM_CAMERA_I2C_BYTE_DATA);
	if (rc < 0) {
		pr_err("%s: %s: read id failed\n", __func__,
			s_ctrl->sensordata->sensor_name);
		return rc;
	}

	CDBG("%s: read id: %x expected id 0x28:\n", __func__, chipid);
	if (chipid != 0x28) {
		pr_err("msm_sensor_match_id chip id doesnot match\n");
		return -ENODEV;
	}

	return rc;
}


static struct msm_sensor_fn_t sp1628_sensor_func_tbl = {
	.sensor_config = sp1628_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_match_id = sp1628_match_id,
};

static struct msm_sensor_ctrl_t sp1628_s_ctrl = {
	.sensor_i2c_client = &sp1628_sensor_i2c_client,
	.power_setting_array.power_setting = sp1628_power_setting,
	.power_setting_array.size = ARRAY_SIZE(sp1628_power_setting),
	.msm_sensor_mutex = &sp1628_mut,
	.sensor_v4l2_subdev_info = sp1628_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(sp1628_subdev_info),
	.func_tbl = &sp1628_sensor_func_tbl,
};

module_init(sp1628_init_module);
module_exit(sp1628_exit_module);
MODULE_DESCRIPTION("Aptina 1.26MP YUV sensor driver");
MODULE_LICENSE("GPL v2");
