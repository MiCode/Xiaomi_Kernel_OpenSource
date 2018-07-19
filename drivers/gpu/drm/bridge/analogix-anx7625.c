/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * Copyright(c) 2016, Analogix Semiconductor. All rights reserved.
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

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/types.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/component.h>

#include <drm/drmP.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_dp_helper.h>
#include <drm/drm_edid.h>

#include "analogix-anx7625.h"

#define TX_P0			0x70
#define TX_P1			0x7A
#define TX_P2			0x72
#define RX_P0			0x7e
#define RX_P1			0x84
#define RX_P2			0x54
#define TCPC_INTERFACE	0x58

#define ReadReg(addr, offset) ({\
	unsigned int buf;\
	reg_read(anx7625, addr, offset, &buf);\
	buf;\
})

#define Read_Reg(addr, offset, buf) ({\
	reg_read(anx7625, addr, offset, buf);\
})

#define ReadBlockReg(addr, offset, len, dat)\
	reg_read_block(anx7625, addr, offset, dat, len)

#define WriteReg(addr, offset, val) ({\
	reg_write(anx7625, addr, offset, val);\
})

#define WriteBlockReg(addr, offset, len, dat)\
	reg_write_block(anx7625, addr, offset, dat, len)

#define sp_write_reg_or(address, offset, mask) \
{ WriteReg(address, offset, ((unsigned char)ReadReg(address, offset) \
	| (mask))); }
#define sp_write_reg_and(address, offset, mask) \
{ WriteReg(address, offset, ((unsigned char)ReadReg(address, offset) \
	&(mask))); }

#define sp_write_reg_and_or(address, offset, and_mask, or_mask) \
{ WriteReg(address, offset, (((unsigned char)ReadReg(address, offset)) \
	&and_mask) | (or_mask)); }
#define sp_write_reg_or_and(address, offset, or_mask, and_mask) \
{ WriteReg(address, offset, (((unsigned char)ReadReg(address, offset)) \
	| or_mask) & (and_mask)); }

struct anx7625_platform_data {
	struct gpio_desc *gpiod_cdet;
	struct gpio_desc *gpiod_p_on;
	struct gpio_desc *gpiod_reset;

	int cdet_irq;
	int intp_irq;
};

struct MIPI_Video_Format {
	unsigned char timing_id;
	unsigned char MIPI_video_type[32];
	unsigned char MIPI_lane_count;
	unsigned long MIPI_pixel_frequency;  /*Hz*/

	unsigned long  M;
	unsigned long  N;
	unsigned char  post_divider;
	/* bit[7:4]: DIFF_I_RATIO, bit[3:0]: DIFF_K_RATIO; i.e. 0x84:0x1B.
	 * These settings affect ODFC PLL locking range.
	 */
	unsigned char  diff_ratio;

	unsigned char compress_ratio;
	unsigned char video_3D_type;
	unsigned char *pps_reg;
	const struct RegisterValueConfig *custom_reg0;
	const struct RegisterValueConfig *custom_reg1;

	struct TimingInfor {
		unsigned int MIPI_HTOTAL;
		unsigned int MIPI_HActive;
		unsigned int MIPI_VTOTAL;
		unsigned int MIPI_VActive;

		unsigned int MIPI_H_Front_Porch;
		unsigned int MIPI_H_Sync_Width;
		unsigned int MIPI_H_Back_Porch;


		unsigned int MIPI_V_Front_Porch;
		unsigned int MIPI_V_Sync_Width;
		unsigned int MIPI_V_Back_Porch;
	} MIPI_inputl[2];
};

struct anx7625 {
	struct drm_dp_aux aux;
	struct drm_bridge bridge;
	struct i2c_client *client;
	struct edid *edid;
	struct drm_dp_link link;
	struct anx7625_platform_data pdata;
	struct mutex lock;
	int mode_idx;

	u16 chipid;

	bool powered;
	bool enabled;
	int connected;
	bool hpd_status;
	u8 sys_sta_bak;

	unsigned char last_read_DevAddr;
};

static void Reg_Access_Conflict_Workaround(struct anx7625 *anx7625,
		unsigned char DevAddr)
{
	unsigned char RegAddr;
	int ret = 0, i;

	if (DevAddr != anx7625->last_read_DevAddr) {
		switch (DevAddr) {
		case  0x54:
		case  0x72:
		default:
			RegAddr = 0x00;
			break;

		case  0x58:
			RegAddr = 0x00;
			break;

		case  0x70:
			RegAddr = 0xD1;
			break;

		case  0x7A:
			RegAddr = 0x60;
			break;

		case  0x7E:
			RegAddr = 0x39;
			break;

		case  0x84:
			RegAddr = 0x7F;
			break;
		}

		anx7625->client->addr = (DevAddr >> 1);
		for (i = 0; i < 5; i++) {
			ret = i2c_smbus_write_byte_data(anx7625->client,
				RegAddr, 0x00);
			if (ret >= 0)
				break;
			pr_err("failed to write i2c addr=%x:%x, retry %d...\n",
				DevAddr, RegAddr, i);
			usleep_range(1000, 1100);
		}
		anx7625->last_read_DevAddr = DevAddr;
	}
}

static int reg_read(struct anx7625 *anx7625,
		int addr, int offset, unsigned int *buf)
{
	int ret, i;

	Reg_Access_Conflict_Workaround(anx7625, addr);
	anx7625->client->addr = (addr >> 1);
	for (i = 0; i < 5; i++) {
		ret = i2c_smbus_read_byte_data(
			anx7625->client, offset);
		if (ret >= 0)
			break;
		pr_err("failed to read anx7625 %x:%x, retry %d...\n",
			addr, offset, i);
		usleep_range(1000, 1100);
	}
	*buf = ret;
	return 0;
}

static int reg_write(struct anx7625 *anx7625,
		int addr, int offset, unsigned int val)
{
	int ret, i;

	Reg_Access_Conflict_Workaround(anx7625, addr);
	anx7625->client->addr = (addr >> 1);
	for (i = 0; i < 5; i++) {
		ret = i2c_smbus_write_byte_data(
			anx7625->client, offset, val);
		if (ret >= 0)
			break;
		pr_err("failed to write anx7625 %x:%x, retry %d...\n",
			addr, offset, i);
		usleep_range(1000, 1100);
	}
	return 0;
}

static int reg_read_block(struct anx7625 *anx7625,
		int addr, int offset, u8 *buf, int len)
{
	int ret, i;

	Reg_Access_Conflict_Workaround(anx7625, addr);
	anx7625->client->addr = (addr >> 1);
	for (i = 0; i < 5; i++) {
		ret = i2c_smbus_read_i2c_block_data(
			anx7625->client, offset, len, buf);
		if (ret >= 0)
			break;
		pr_err("failed to read anx7625 %x:%x, retry %d...\n",
			addr, offset, i);
		usleep_range(1000, 1100);
	}
	return 0;
}

static int reg_write_block(struct anx7625 *anx7625,
		int addr, int offset, u8 *buf, int len)
{
	int ret, i;

	Reg_Access_Conflict_Workaround(anx7625, addr);
	anx7625->client->addr = (addr >> 1);
	for (i = 0; i < 5; i++) {
		ret = i2c_smbus_write_i2c_block_data(
			anx7625->client, offset, len, buf);
		if (ret >= 0)
			break;
		pr_err("failed to write anx7625 %x:%x, retry %d...\n",
			addr, offset, i);
		usleep_range(1000, 1100);
	}
	return 0;
}

#define mipi_pixel_frequency(id)   \
	mipi_video_timing_table[id].MIPI_pixel_frequency
#define mipi_lane_count(id)   \
	mipi_video_timing_table[id].MIPI_lane_count
#define mipi_m_value(id)   \
	mipi_video_timing_table[id].M
#define mipi_n_value(id)   \
	mipi_video_timing_table[id].N
#define mipi_post_divider(id)   \
	mipi_video_timing_table[id].post_divider
#define mipi_diff_ratio(id)   \
	mipi_video_timing_table[id].diff_ratio
#define mipi_compress_ratio(id)   \
	mipi_video_timing_table[id].compress_ratio

#define mipi_original_htotal(id)   \
	mipi_video_timing_table[id].MIPI_inputl[0].MIPI_HTOTAL
#define mipi_original_hactive(id)   \
	mipi_video_timing_table[id].MIPI_inputl[0].MIPI_HActive
#define mipi_original_vtotal(id)   \
	mipi_video_timing_table[id].MIPI_inputl[0].MIPI_VTOTAL
#define mipi_original_vactive(id)   \
	mipi_video_timing_table[id].MIPI_inputl[0].MIPI_VActive
#define mipi_original_hfp(id)   \
	mipi_video_timing_table[id].MIPI_inputl[0].MIPI_H_Front_Porch
#define mipi_original_hsw(id)  \
	mipi_video_timing_table[id].MIPI_inputl[0].MIPI_H_Sync_Width
#define mipi_original_hbp(id)   \
	mipi_video_timing_table[id].MIPI_inputl[0].MIPI_H_Back_Porch
#define mipi_original_vfp(id)   \
	mipi_video_timing_table[id].MIPI_inputl[0].MIPI_V_Front_Porch
#define mipi_original_vsw(id)   \
	mipi_video_timing_table[id].MIPI_inputl[0].MIPI_V_Sync_Width
#define mipi_original_vbp(id)   \
	mipi_video_timing_table[id].MIPI_inputl[0].MIPI_V_Back_Porch

#define mipi_decompressed_htotal(id)   \
	mipi_video_timing_table[id].MIPI_inputl[1].MIPI_HTOTAL
#define mipi_decompressed_hactive(id)   \
	mipi_video_timing_table[id].MIPI_inputl[1].MIPI_HActive
#define mipi_decompressed_vtotal(id)   \
	mipi_video_timing_table[id].MIPI_inputl[1].MIPI_VTOTAL
#define mipi_decompressed_vactive(id)   \
	mipi_video_timing_table[id].MIPI_inputl[1].MIPI_VActive
#define mipi_decompressed_hfp(id)   \
	mipi_video_timing_table[id].MIPI_inputl[1].MIPI_H_Front_Porch
#define mipi_decompressed_hsw(id)   \
	mipi_video_timing_table[id].MIPI_inputl[1].MIPI_H_Sync_Width
#define mipi_decompressed_hbp(id)  \
	mipi_video_timing_table[id].MIPI_inputl[1].MIPI_H_Back_Porch
#define mipi_decompressed_vfp(id)   \
	mipi_video_timing_table[id].MIPI_inputl[1].MIPI_V_Front_Porch
#define mipi_decompressed_vsw(id)   \
	mipi_video_timing_table[id].MIPI_inputl[1].MIPI_V_Sync_Width
#define mipi_decompressed_vbp(id)   \
	mipi_video_timing_table[id].MIPI_inputl[1].MIPI_V_Back_Porch

#define video_3d(id)   mipi_video_timing_table[id].video_3D_type

static unsigned char PPS_4K[] = { /*VC707 (DPI+DSC)*/
	0x11, 0x00, 0x00, 0x89, 0x10, 0x80, 0x08, 0x70,
	0x0f, 0x00, 0x00, 0x08, 0x07, 0x80, 0x07, 0x80,
	0x02, 0x00, 0x04, 0xc0, 0x00, 0x20, 0x01, 0x1e,
	0x00, 0x1a, 0x00, 0x0c, 0x0d, 0xb7, 0x03, 0x94,
	0x18, 0x00, 0x10, 0xf0, 0x03, 0x0c, 0x20, 0x00,
	0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
	0x7d, 0x7e, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xbe, 0x19, 0xfc, 0x19, 0xfa, 0x19, 0xf8,
	0x1a, 0x38, 0x1a, 0x78, 0x1a, 0xb6, 0x2a, 0xf6,
	0x2b, 0x34, 0x2b, 0x74, 0x3b, 0x74, 0x6b, 0xf4,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static unsigned char PPS_AR[] = {/*1440x2560@70*/
	0x11, 0x00, 0x00, 0x89, 0x30, 0x80, 0x0A, 0x00,
	0x05, 0xA0, 0x00, 0x10, 0x05, 0xa0, 0x05, 0xa0,
	0x02, 0x00, 0x03, 0xd0, 0x00, 0x20, 0x02, 0x33,
	0x00, 0x14, 0x00, 0x0c, 0x06, 0x67, 0x02, 0x63,
	0x18, 0x00, 0x10, 0xf0, 0x03, 0x0c, 0x20, 0x00,
	0x06, 0x0b, 0x0b, 0x33, 0x0e, 0x1c, 0x2a, 0x38,
	0x46, 0x54, 0x62, 0x69, 0x70, 0x77, 0x79, 0x7b,
	0x7d, 0x7e, 0x01, 0x02, 0x01, 0x00, 0x09, 0x40,
	0x09, 0xbe, 0x19, 0xfc, 0x19, 0xfa, 0x19, 0xf8,
	0x1a, 0x38, 0x1a, 0x78, 0x1a, 0xb6, 0x2a, 0xf6,
	0x2b, 0x34, 0x2b, 0x74, 0x3b, 0x74, 0x6b, 0xf4,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static unsigned char PPS_Custom[] = {
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static const struct RegisterValueConfig Bit_Matrix[] = {
	{TX_P2, AUDIO_CONTROL_REGISTER, 0x80},
	{TX_P2, VIDEO_BIT_MATRIX_12, 0x18},
	{TX_P2, VIDEO_BIT_MATRIX_13, 0x19},
	{TX_P2, VIDEO_BIT_MATRIX_14, 0x1a},
	{TX_P2, VIDEO_BIT_MATRIX_15, 0x1b},
	{TX_P2, VIDEO_BIT_MATRIX_16, 0x1c},
	{TX_P2, VIDEO_BIT_MATRIX_17, 0x1d},
	{TX_P2, VIDEO_BIT_MATRIX_18, 0x1e},
	{TX_P2, VIDEO_BIT_MATRIX_19, 0x1f},
	{TX_P2, VIDEO_BIT_MATRIX_20, 0x20},
	{TX_P2, VIDEO_BIT_MATRIX_21, 0x21},
	{TX_P2, VIDEO_BIT_MATRIX_22, 0x22},
	{TX_P2, VIDEO_BIT_MATRIX_23, 0x23},
	{0x00, 0x00, 0x00}
};

static struct MIPI_Video_Format mipi_video_timing_table[] = {
	/*	lane_count--pixel_clk-----M---N--div- */
	/*	-diff--compr--3d--table--custom0--custom1*/
	/*	original timing */
	/*	total-H active-Vtotal-V active-HFP-HSW-HBP-VFP-VSW-VBP*/
	/*	decompressed timing */
	/*	tota-H active-Vtotal-V active-HFP-HSW-HBP-VFP-VSW-VBP*/
	{
		0, "720x480@60", 3, 27000000, 0xC00000, 0x100000, 0x0B,
			0x3B, 0, VIDEO_3D_NONE, NULL, Bit_Matrix, NULL,
		{ { 858, 720, 525, 480, 16, 60, 62, 10, 6, 29 } }
	},
	{
		1, "1280X720P@60", 3, 74250000, 0xB00000, 0x080000, 0x07,
			0x3A, 0, VIDEO_3D_NONE, NULL, Bit_Matrix, NULL,
		{ { 1650, 1280,  750,  720,	110,	 40,  220, 5, 5, 20 } }
	},
	{
		2, "1920x1080p@30", 3, 74000000, 0x940000, 0x06C000, 0x07,
			0x3B, 0, VIDEO_3D_NONE, NULL, Bit_Matrix, NULL,
		{ { 2200,  1920, 1125, 1080, 88, 44, 148, 4, 5, 36  } }
	},
	{
		3, "1920x1080p@60", 3, 148500000, 0xB00000, 0x080000, 0x03,
			0x37, 0, VIDEO_3D_NONE, NULL,  Bit_Matrix, NULL,
		{ { 2200, 1920, 1125, 1080, 88, 44, 148, 4, 5, 36 } }
	},
	/*MTK 4K24 DPI*/
	{
		4, "3840x2160@24",  3, 297000000, 0xB00000, 0x080000, 0x01,
			0x37, 3, VIDEO_3D_NONE, PPS_4K, Bit_Matrix, NULL,
		{	{ 1650, 1280,  2250, 2160, 242, 30, 98, 8, 10, 72 },
			{ 4950, 3840,  2250, 2160, 726, 90, 294, 8, 10, 72 }
		}
	},
	/*MTK 4K30 DPI*/
	{
		5, "3840x2160@30", 3, 297000000, 0xB00000, 0x080000, 0x01,
			0x37, 3, VIDEO_3D_NONE, PPS_4K, Bit_Matrix, NULL,
		{	{ 1474, 1280, 2250, 2160, 66,  30, 98,  8, 10, 72 },
			{ 4422, 3840, 2250, 2160, 198, 90, 294,  8, 10, 72 }
		}
	},

	{/*DSI*/
		6, "720x480@60", 3, 27000000, 0xC00000, 0x100000, 0x0B,
			0x3B,  0, VIDEO_3D_NONE, NULL, NULL, NULL,
		{ { 858, 720, 525, 480, 16, 60, 62, 10, 6, 29 } }
	},
	{/*DSI*/
		7, "1280X720P@60", 3, 74250000,	0xB00000, 0x080000, 0x07,
			0x3A, 0,  VIDEO_3D_NONE, NULL,   NULL, NULL,
		{ { 1650,	1280, 750, 720, 110, 40, 220, 5, 5, 20 } }
	},
	{/*DSI*/
		8, "1920x1080p@30", 3, 74250000, 0xB00000, 0x080000, 0x07,
			0x3B, 0, VIDEO_3D_NONE, NULL, NULL, NULL,
		{ { 2200, 1920, 1125, 1080, 88, 44, 148, 4, 5, 36 } }
	},
	{/*DSI*/
		9, "1920x1080p@60", 3, 148500000, 0xB00000, 0x080000, 0x03,
			0x37,   0,   VIDEO_3D_NONE, NULL,  NULL, NULL,
		{ { 2200, 1920, 1125, 1080, 88, 44, 148, 4, 5, 36 } }
	},

	/* 3840x2160p24  - MTK X30 -DSI*/
	{/*DSI*/
		10, "3840x2160p24", 3, 268176696, 0xAA808D, 0x089544, 0x01,
			0x37, 3, VIDEO_3D_NONE, PPS_4K, NULL, NULL,
		{	{ 1650, 1280, 2250, 2160, 242, 30, 98, 8, 10, 72 },
			{ 4950, 3840, 2250, 2160, 726, 90, 294, 8, 10, 72 }
		}
	},
	/* 3840x2160p30 3:1 DSC - MTK X30 -DSI*/
	{/*DSI*/
		11, "1280x2160p30", 3, 297000000, 0xA7B3AB, 0x07A120, 0x01,
			0x37, 3, VIDEO_3D_NONE, PPS_4K, NULL, NULL,
		{	{ 1467, 1280, 2250, 2160,  66, 30,  91, 8, 10, 72 },
			{ 4400, 3840, 2250, 2160, 198, 90, 272, 8, 10, 72 }
		}
	},

	{
		12, "1440X2560P@70", 3, 285000000, 0xB00000, 0x080000, 0x01,
			0x37, 3, VIDEO_3D_NONE, PPS_AR, Bit_Matrix, NULL,
		{	{524,   480,  2576, 2560, 24, 10, 12, 6, 8, 2 },
			{1580, 1440, 2576, 2560, 80, 20, 40, 6,  8, 2}
		}
	},
	{
		13, "********@60",	 0,		 0,	   0, 0,    0,
			0,  0, VIDEO_3D_NONE, NULL, NULL, NULL,
		{ { 0,   0,  0,	 0, 0,  0, 0,  0,  0,  0 } }
	},
	{
		14, "********@60",	 0,		 0,	   0, 0,    0,
			0,  0, VIDEO_3D_NONE, NULL, NULL, NULL,
		{ { 0,   0,  0,	 0, 0,  0, 0,  0,  0,  0 } }
	},
	{
		15, "custom@DPI/DSI", 3, 297000000, 0xB00000, 0x080000, 0x01,
			0x37, 3, VIDEO_3D_NONE, PPS_Custom, Bit_Matrix, NULL,
		{	{ 0,   0,  0,	 0, 0,  0, 0,  0,  0,  0 },
			{ 0,   0,  0,	 0, 0,  0, 0,  0,  0,  0 }
		}
	},
};

static inline struct anx7625 *bridge_to_anx7625(struct drm_bridge *bridge)
{
	return container_of(bridge, struct anx7625, bridge);
}

#define write_dpcd_addr(addrh, addrm, addrl) \
	do { \
		unsigned int temp; \
		if (ReadReg(RX_P0, AP_AUX_ADDR_7_0) != (unchar)addrl) \
			WriteReg(RX_P0, AP_AUX_ADDR_7_0, (unchar)addrl); \
		if (ReadReg(RX_P0, AP_AUX_ADDR_15_8) != (unchar)addrm) \
			WriteReg(RX_P0, AP_AUX_ADDR_15_8, (unchar)addrm); \
		Read_Reg(RX_P0, AP_AUX_ADDR_19_16, &temp); \
		if ((unchar)(temp & 0x0F)  != ((unchar)addrh & 0x0F)) \
			WriteReg(RX_P0, AP_AUX_ADDR_19_16, \
				(temp  & 0xF0) | ((unchar)addrh)); \
	} while (0)

static void wait_aux_op_finish(struct anx7625 *anx7625, unchar *err_flag)
{
	unchar cnt;
	uint c;

	*err_flag = 0;
	cnt = 150;
	while (ReadReg(RX_P0, AP_AUX_CTRL_STATUS) & AP_AUX_CTRL_OP_EN) {
		usleep_range(2000, 2100);
		if ((cnt--) == 0) {
			TRACE("aux operate failed!\n");
			*err_flag = 1;
			break;
		}
	}

	Read_Reg(RX_P0, AP_AUX_CTRL_STATUS, &c);
	if (c & 0x0F) {
		TRACE1("wait aux operation status %02x\n", (uint)c);
		*err_flag = 1;
	}
}

unchar sp_tx_aux_dpcdread_bytes(struct anx7625 *anx7625,
		unchar addrh, unchar addrm, unchar addrl,
		unchar cCount, unchar *pBuf)
{
	uint c, i;
	unchar bOK;

	/*command and length*/
	c = ((cCount - 1) << 4) | 0x09;
	WriteReg(RX_P0, AP_AUX_COMMAND, c);
	/*address*/
	write_dpcd_addr(addrh, addrm, addrl);
	/*aux en*/
	sp_write_reg_or(RX_P0, AP_AUX_CTRL_STATUS, AP_AUX_CTRL_OP_EN);
	usleep_range(2000, 2100);
	/* TRACE3("auxch addr = 0x%02x%02x%02x\n", addrh,addrm,addrl);*/
	wait_aux_op_finish(anx7625, &bOK);
	if (bOK == AUX_ERR) {
		TRACE("aux read failed\n");
		return AUX_ERR;
	}

	for (i = 0; i < cCount; i++) {
		Read_Reg(RX_P0, AP_AUX_BUFF_START + i, &c);
		*(pBuf + i) = c;
		/*TRACE2("Buf[%d] = 0x%02x\n", (uint)i, *(pBuf + i));*/
		if (i >= MAX_BUF_CNT)
			break;
	}

	return AUX_OK;
}

unchar sp_tx_aux_dpcdwrite_bytes(struct anx7625 *anx7625,
		unchar addrh, unchar addrm, unchar addrl,
		unchar cCount, unchar *pBuf)
{
	unchar c, i, ret;

	/*command and length*/
	c =  ((cCount - 1) << 4) | 0x08;
	WriteReg(RX_P0, AP_AUX_COMMAND, c);
	/*address*/
	write_dpcd_addr(addrh, addrm, addrl);
	/*data*/
	for (i = 0; i < cCount; i++) {
		c = *pBuf;
		pBuf++;
		WriteReg(RX_P0, AP_AUX_BUFF_START + i, c);

		if (i >= 15)
			break;
	}
	/*aux en*/
	sp_write_reg_or(RX_P0,  AP_AUX_CTRL_STATUS, AP_AUX_CTRL_OP_EN);
	wait_aux_op_finish(anx7625, &ret);
	TRACE("aux write done\n");
	return ret;
}

static unchar sp_tx_aux_wr(struct anx7625 *anx7625, unchar offset)
{
	unchar c;

	WriteReg(RX_P0, AP_AUX_BUFF_START, offset);
	WriteReg(RX_P0, AP_AUX_COMMAND, 0x04);
	sp_write_reg_or(RX_P0, AP_AUX_CTRL_STATUS, AP_AUX_CTRL_OP_EN);
	wait_aux_op_finish(anx7625, &c);

	return c;
}

static unchar sp_tx_aux_rd(struct anx7625 *anx7625, unchar len_cmd)
{
	unchar c;

	WriteReg(RX_P0, AP_AUX_COMMAND, len_cmd);
	sp_write_reg_or(RX_P0, AP_AUX_CTRL_STATUS, AP_AUX_CTRL_OP_EN);
	wait_aux_op_finish(anx7625, &c);

	return c;
}

static ssize_t anx7625_aux_transfer(struct drm_dp_aux *aux,
				    struct drm_dp_aux_msg *msg)
{
	struct anx7625 *anx7625 = container_of(aux, struct anx7625, aux);
	u8 *buffer = msg->buffer;
	int err = 0;

	if (!buffer || !msg->size)
		return 0;

	if ((msg->request & DP_AUX_NATIVE_READ) == DP_AUX_NATIVE_READ) {
		err = sp_tx_aux_dpcdread_bytes(anx7625,
			(msg->address >> 16) & 0xff,
			(msg->address >> 8) & 0xff,
			(msg->address) & 0xff,
			msg->size,
			buffer);
	} else if ((msg->request & DP_AUX_NATIVE_WRITE) ==
			DP_AUX_NATIVE_WRITE) {
		err = sp_tx_aux_dpcdwrite_bytes(anx7625,
			(msg->address >> 16) & 0xff,
			(msg->address >> 8) & 0xff,
			(msg->address) & 0xff,
			msg->size,
			buffer);
	} else if ((msg->request & DP_AUX_I2C_READ) == DP_AUX_I2C_READ) {
		err = sp_tx_aux_rd(anx7625, ((msg->size - 1) << 4) | 0x01);
		if (!err) {
			ReadBlockReg(RX_P0, AP_AUX_BUFF_START,
				msg->size, buffer);
		}
	} else if ((msg->request & ~DP_AUX_I2C_MOT) == DP_AUX_I2C_WRITE) {
		WriteReg(RX_P0, AP_AUX_ADDR_7_0, (msg->address) & 0xff);
		WriteReg(RX_P0, AP_AUX_ADDR_15_8, 0);
		sp_write_reg_and(RX_P0, AP_AUX_ADDR_19_16, 0xf0);
		err = sp_tx_aux_wr(anx7625, buffer[0]);
	}

	msg->reply = DP_AUX_I2C_REPLY_ACK;

	if (err)
		pr_err("anx7625 aux transfer failed %d\n", err);

	return msg->size;
}

static int anx7625_enable_interrupts(struct anx7625 *anx7625)
{
	/* enable all interrupts */
	WriteReg(RX_P0, INTERFACE_INTR_MASK, 0x7f);

	return 0;
}

static int anx7625_disable_interrupts(struct anx7625 *anx7625)
{
	/* disable all interrupts */
	WriteReg(RX_P0, INTERFACE_INTR_MASK, 0xff);

	return 0;
}

static int anx7625_poweroff(struct anx7625 *anx7625)
{
	struct anx7625_platform_data *pdata = &anx7625->pdata;

	if (!anx7625->powered)
		return 0;

	anx7625_disable_interrupts(anx7625);

	gpiod_set_value_cansleep(pdata->gpiod_reset, 0);
	usleep_range(1000, 2000);

	gpiod_set_value_cansleep(pdata->gpiod_p_on, 0);
	usleep_range(1000, 2000);

	anx7625->powered = false;
	return 0;
}

static int anx7625_poweron(struct anx7625 *anx7625)
{
	struct anx7625_platform_data *pdata = &anx7625->pdata;

	if (anx7625->powered)
		return 0;

	gpiod_set_value_cansleep(pdata->gpiod_p_on, 1);
	usleep_range(10000, 11000);

	gpiod_set_value_cansleep(pdata->gpiod_reset, 1);
	usleep_range(10000, 11000);

	/* setup clock */
	WriteReg(RX_P0, XTAL_FRQ_SEL, XTAL_FRQ_27M);

	/*First, reset main ocm*/
	WriteReg(RX_P0, 0x88,  0x40);

	/* disable PD */
	WriteReg(RX_P0, AP_AV_STATUS, AP_DISABLE_PD);

	/*after configuration, start main ocm running.*/
	WriteReg(RX_P0, 0x88,  0x00);

	/* enable interrupt */
	anx7625_enable_interrupts(anx7625);

	anx7625->powered = true;
	return 0;
}

static void DSI_Video_Timing_Configuration(struct anx7625 *anx7625)
{
	int table_id = anx7625->mode_idx;

	/*configure clock*/
	WriteReg(RX_P0, PIXEL_CLOCK_L,
		(mipi_pixel_frequency(table_id) / 1000000) & 0xFF);
	WriteReg(RX_P0, PIXEL_CLOCK_H,
		(mipi_pixel_frequency(table_id) / 1000000) >> 8);
	/*lane count*/
	sp_write_reg_and(RX_P1, MIPI_LANE_CTRL_0, 0xfc);
	sp_write_reg_or(RX_P1, MIPI_LANE_CTRL_0,
		mipi_lane_count(table_id));
	/*Htotal*/
	WriteReg(RX_P2, HORIZONTAL_TOTAL_PIXELS_L,
		mipi_original_htotal(table_id) & 0xFF);
	WriteReg(RX_P2, HORIZONTAL_TOTAL_PIXELS_H,
		mipi_original_htotal(table_id) >> 8);
	/*Hactive*/
	WriteReg(RX_P2, HORIZONTAL_ACTIVE_PIXELS_L,
		mipi_original_hactive(table_id) & 0xFF);
	WriteReg(RX_P2, HORIZONTAL_ACTIVE_PIXELS_H,
		mipi_original_hactive(table_id) >> 8);
	/*HFP*/
	WriteReg(RX_P2, HORIZONTAL_FRONT_PORCH_L,
		mipi_original_hfp(table_id) & 0xFF);
	WriteReg(RX_P2, HORIZONTAL_FRONT_PORCH_H,
		mipi_original_hfp(table_id) >> 8);
	/*HWS*/
	WriteReg(RX_P2, HORIZONTAL_SYNC_WIDTH_L,
		mipi_original_hsw(table_id) & 0xFF);
	WriteReg(RX_P2, HORIZONTAL_SYNC_WIDTH_H,
		mipi_original_hsw(table_id) >> 8);
	/*HBP*/
	WriteReg(RX_P2, HORIZONTAL_BACK_PORCH_L,
		mipi_original_hbp(table_id) & 0xFF);
	WriteReg(RX_P2, HORIZONTAL_BACK_PORCH_H,
		mipi_original_hbp(table_id) >> 8);
	/*Vactive*/
	WriteReg(RX_P2, ACTIVE_LINES_L,
		mipi_original_vactive(table_id) & 0xFF);
	WriteReg(RX_P2, ACTIVE_LINES_H,
		mipi_original_vactive(table_id)  >> 8);
	/*VFP*/
	WriteReg(RX_P2, VERTICAL_FRONT_PORCH,
		mipi_original_vfp(table_id));
	/*VWS*/
	WriteReg(RX_P2, VERTICAL_SYNC_WIDTH,
		mipi_original_vsw(table_id));
	/*VBP*/
	WriteReg(RX_P2, VERTICAL_BACK_PORCH,
		mipi_original_vbp(table_id));
	/*M value*/
	WriteReg(RX_P1, MIPI_PLL_M_NUM_23_16,
		(mipi_m_value(table_id) >> 16) & 0xff);
	WriteReg(RX_P1, MIPI_PLL_M_NUM_15_8,
		(mipi_m_value(table_id) >> 8) & 0xff);
	WriteReg(RX_P1, MIPI_PLL_M_NUM_7_0,
		mipi_m_value(table_id) & 0xff);
	/*N value*/
	WriteReg(RX_P1, MIPI_PLL_N_NUM_23_16,
		(mipi_n_value(table_id) >> 16) & 0xff);
	WriteReg(RX_P1, MIPI_PLL_N_NUM_15_8,
		(mipi_n_value(table_id) >> 8) & 0xff);
	WriteReg(RX_P1, MIPI_PLL_N_NUM_7_0,
		mipi_n_value(table_id) & 0xff);
	/*diff*/
	WriteReg(RX_P1, MIPI_DIGITAL_ADJ_1,
		mipi_diff_ratio(table_id));
}

static void API_ODFC_Configuration(struct anx7625 *anx7625)
{
	int table_id = anx7625->mode_idx;

	/*config input reference clock frequency 27MHz/19.2MHz*/
	sp_write_reg_and(RX_P1, MIPI_DIGITAL_PLL_16,
		~(REF_CLK_27000kHz << MIPI_FREF_D_IND));
	sp_write_reg_or(RX_P1, MIPI_DIGITAL_PLL_16,
		(((XTAL_FRQ >= 26000000UL) && (XTAL_FRQ <= 27000000UL)) ?
		(REF_CLK_27000kHz << MIPI_FREF_D_IND)
		: (REF_CLK_19200kHz << MIPI_FREF_D_IND)));
	/*post divider*/
	sp_write_reg_and(RX_P1, MIPI_DIGITAL_PLL_8, 0x0f);
	sp_write_reg_or(RX_P1, MIPI_DIGITAL_PLL_8,
		mipi_post_divider(table_id) << 4);

	/*add patch for MIS2-125 (5pcs ANX7625 fail ATE MBIST test)*/
	sp_write_reg_and(RX_P1, MIPI_DIGITAL_PLL_7,
	~MIPI_PLL_VCO_TUNE_REG_VAL);

	/*reset ODFC PLL*/
	sp_write_reg_and(RX_P1, MIPI_DIGITAL_PLL_7,
		~MIPI_PLL_RESET_N);
	sp_write_reg_or(RX_P1, MIPI_DIGITAL_PLL_7,
		MIPI_PLL_RESET_N);
	/*force PLL lock*/
	//WriteReg(TX_P0, DP_CONFIG_24, 0x0c);
}

static void DSC_Video_Timing_Configuration(struct anx7625 *anx7625,
		unsigned char table_id)
{
	unchar i;

	/*config uncompressed video format*/
	/*Htotal*/
	WriteReg(TX_P2, HORIZONTAL_TOTAL_PIXELS_L,
		(mipi_original_htotal(table_id) * mipi_compress_ratio(table_id))
		& 0xFF);
	WriteReg(TX_P2, HORIZONTAL_TOTAL_PIXELS_H,
		(mipi_original_htotal(table_id) * mipi_compress_ratio(table_id))
		>> 8);
	/*Hactive*/
	WriteReg(TX_P2, HORIZONTAL_ACTIVE_PIXELS_L,
		(mipi_original_hactive(table_id)
		 * mipi_compress_ratio(table_id)) & 0xFF);
	WriteReg(TX_P2, HORIZONTAL_ACTIVE_PIXELS_H,
		(mipi_original_hactive(table_id)
		 * mipi_compress_ratio(table_id)) >> 8);
	/*HFP*/
	WriteReg(TX_P2, HORIZONTAL_FRONT_PORCH_L,
		(mipi_original_hfp(table_id) * mipi_compress_ratio(table_id))
		& 0xFF);
	WriteReg(TX_P2, HORIZONTAL_FRONT_PORCH_H,
		(mipi_original_hfp(table_id) * mipi_compress_ratio(table_id))
		>> 8);
	/*HWS*/
	WriteReg(TX_P2, HORIZONTAL_SYNC_WIDTH_L,
		(mipi_original_hsw(table_id) * mipi_compress_ratio(table_id))
		& 0xFF);
	WriteReg(TX_P2, HORIZONTAL_SYNC_WIDTH_H,
		(mipi_original_hsw(table_id) * mipi_compress_ratio(table_id))
		>> 8);
	/*HBP*/
	WriteReg(TX_P2, HORIZONTAL_BACK_PORCH_L,
		(mipi_original_hbp(table_id) * mipi_compress_ratio(table_id))
		& 0xFF);
	WriteReg(TX_P2, HORIZONTAL_BACK_PORCH_H,
		(mipi_original_hbp(table_id) * mipi_compress_ratio(table_id))
		>> 8);
	/*Vtotal*/
	WriteReg(TX_P2, TOTAL_LINES_L,
		mipi_original_vtotal(table_id) & 0xFF);
	WriteReg(TX_P2, TOTAL_LINES_H,
		mipi_original_vtotal(table_id) >> 8);
	/*Vactive*/
	WriteReg(TX_P2, ACTIVE_LINES_L,
		mipi_original_vactive(table_id) & 0xFF);
	WriteReg(TX_P2, ACTIVE_LINES_H,
		mipi_original_vactive(table_id) >> 8);
	/*VFP*/
	WriteReg(TX_P2, VERTICAL_FRONT_PORCH,
		mipi_original_vfp(table_id));
	/*VWS*/
	WriteReg(TX_P2, VERTICAL_SYNC_WIDTH,
		mipi_original_vsw(table_id));
	/*VBP*/
	WriteReg(TX_P2, VERTICAL_BACK_PORCH,
		mipi_original_vbp(table_id));

	/*config uncompressed video format to woraround */
	/* downstream compatibility issues*/
	/*Htotal*/
	WriteReg(RX_P0, TOTAL_PIXEL_L_7E,
		mipi_decompressed_htotal(table_id) & 0xFF);
	WriteReg(RX_P0, TOTAL_PIXEL_H_7E,
		mipi_decompressed_htotal(table_id) >> 8);
	/*Hactive*/
	WriteReg(RX_P0, ACTIVE_PIXEL_L_7E,
		mipi_decompressed_hactive(table_id) & 0xFF);
	WriteReg(RX_P0, ACTIVE_PIXEL_H_7E,
		mipi_decompressed_hactive(table_id) >> 8);
	/*HFP*/
	WriteReg(RX_P0, HORIZON_FRONT_PORCH_L_7E,
		mipi_decompressed_hfp(table_id) & 0xFF);
	WriteReg(RX_P0, HORIZON_FRONT_PORCH_H_7E,
		mipi_decompressed_hfp(table_id) >> 8);
	/*HWS*/
	WriteReg(RX_P0, HORIZON_SYNC_WIDTH_L_7E,
		mipi_decompressed_hsw(table_id) & 0xFF);
	WriteReg(RX_P0, HORIZON_SYNC_WIDTH_H_7E,
		mipi_decompressed_hsw(table_id) >> 8);
	/*HBP*/
	WriteReg(RX_P0, HORIZON_BACK_PORCH_L_7E,
		mipi_decompressed_hbp(table_id)  & 0xFF);
	WriteReg(RX_P0, HORIZON_BACK_PORCH_H_7E,
		mipi_decompressed_hbp(table_id)  >> 8);

	/*config DSC decoder internal blank timing for decoder to start*/
	WriteReg(RX_P1, H_BLANK_L, ((mipi_original_htotal(table_id)
		- mipi_original_hactive(table_id))) & 0xFF);
	WriteReg(RX_P1, H_BLANK_H, ((mipi_original_htotal(table_id)
		- mipi_original_hactive(table_id))) >> 8);

	/*compress ratio  RATIO [7:6] 3:div2; 0,1,2:div3*/
	sp_write_reg_and(RX_P0, R_I2C_1, 0x3f);
	sp_write_reg_or(RX_P0, R_I2C_1,
		(5 - mipi_compress_ratio(table_id)) << 6);

	/*PPS table*/
	if (mipi_video_timing_table[table_id].pps_reg != NULL) {
		for (i = 0; i < 0x80; i += 0x10)
			WriteBlockReg(RX_P2, R_PPS_REG_0 + i, 0x10,
				(unsigned char *)mipi_video_timing_table
				[table_id].pps_reg + i);
	}
}

static void API_Custom_Register0_Configuration(struct anx7625 *anx7625,
		unsigned char table_id)
{
	unchar i = 0;
	/*custom specific register*/
	if (mipi_video_timing_table[table_id].custom_reg0 != NULL) {
		while (mipi_video_timing_table[table_id].custom_reg0[i]
			.slave_addr) {
			WriteReg(mipi_video_timing_table[table_id]
				.custom_reg0[i].slave_addr,
				mipi_video_timing_table[table_id]
				.custom_reg0[i].reg,
				mipi_video_timing_table[table_id]
				.custom_reg0[i].val);
			i++;
		}
	}
}

static void API_Custom_Register1_Configuration(struct anx7625 *anx7625,
		unsigned char table_id)
{
	unchar i = 0;
	/*custom specific register*/
	if (mipi_video_timing_table[table_id].custom_reg1 != NULL) {
		while (mipi_video_timing_table[table_id].custom_reg1[i]
				.slave_addr) {
			WriteReg(mipi_video_timing_table[table_id]
				.custom_reg1[i].slave_addr,
				mipi_video_timing_table[table_id]
				.custom_reg1[i].reg,
				mipi_video_timing_table[table_id]
				.custom_reg1[i].val);
			i++;
		}
	}
}

static void  swap_DSI_lane3(struct anx7625 *anx7625)
{
	unsigned char  RegValue;
	/* swap MIPI-DSI data lane 3 P and N */
	RegValue = ReadReg(RX_P1, MIPI_SWAP);
	RegValue |= (1 << MIPI_SWAP_CH3);
	WriteReg(RX_P1, MIPI_SWAP, RegValue);
}

static void API_DSI_Configuration(struct anx7625 *anx7625,
		unsigned char table_id)
{
	unsigned char  RegValue;

	/* swap MIPI-DSI data lane 3 P and N */
	swap_DSI_lane3(anx7625);

	/* DSI clock settings */
	RegValue = (0 << MIPI_HS_PWD_CLK) |
		(0 << MIPI_HS_RT_CLK)  |
		(0 << MIPI_PD_CLK)     |
		(1 << MIPI_CLK_RT_MANUAL_PD_EN) |
		(1 << MIPI_CLK_HS_MANUAL_PD_EN) |
		(0 << MIPI_CLK_DET_DET_BYPASS)  |
		(0 << MIPI_CLK_MISS_CTRL)       |
		(0 << MIPI_PD_LPTX_CH_MANUAL_PD_EN);
	WriteReg(RX_P1, MIPI_PHY_CONTROL_3, RegValue);

	/* Decreased HS prepare timing delay from 160ns to 80ns work with
	 *     a) Dragon board 810 series (Qualcomm Technologies, Inc AP)
	 *     b) Moving DSI source (PG3A pattern generator +
	 *        P332 D-PHY Probe) default D-PHY timing
	 */
	WriteReg(RX_P1, MIPI_TIME_HS_PRPR, 0x10);  /* 5ns/step */

	sp_write_reg_or(RX_P1, MIPI_DIGITAL_PLL_18,
		SELECT_DSI<<MIPI_DPI_SELECT); /* enable DSI mode*/

	DSI_Video_Timing_Configuration(anx7625);

	API_ODFC_Configuration(anx7625);

	/*toggle m, n ready*/
	sp_write_reg_and(RX_P1, MIPI_DIGITAL_PLL_6,
		~(MIPI_M_NUM_READY | MIPI_N_NUM_READY));
	usleep_range(1000, 1100);
	sp_write_reg_or(RX_P1, MIPI_DIGITAL_PLL_6,
		MIPI_M_NUM_READY | MIPI_N_NUM_READY);

	/*configure integer stable register*/
	WriteReg(RX_P1, MIPI_VIDEO_STABLE_CNT, 0x02);
	/*power on MIPI RX*/
	WriteReg(RX_P1, MIPI_LANE_CTRL_10, 0x00);
	WriteReg(RX_P1, MIPI_LANE_CTRL_10, 0x80);
	usleep_range(10000, 11000);
}

static void DSI_Configuration(struct anx7625 *anx7625, unsigned char table_id)
{
	TRACE1("%s Input Index = %02X\n", __func__, table_id);

	API_Custom_Register0_Configuration(anx7625, table_id);
	/*DSC disable*/
	sp_write_reg_and(RX_P0, R_DSC_CTRL_0, ~DSC_EN);
	API_DSI_Configuration(anx7625, table_id);
	API_Custom_Register1_Configuration(anx7625, table_id);

	/*set MIPI RX  EN*/
	sp_write_reg_or(RX_P0, AP_AV_STATUS, AP_MIPI_RX_EN);
	/*clear mute flag*/
	sp_write_reg_and(RX_P0, AP_AV_STATUS, ~AP_MIPI_MUTE);
}

static void DSI_DSC_Configuration(struct anx7625 *anx7625,
		unsigned char table_id)
{
	TRACE1("%s Input Index = %02X\n", __func__, table_id);

	API_Custom_Register0_Configuration(anx7625, table_id);

	DSC_Video_Timing_Configuration(anx7625, table_id);
	/*DSC enable*/
	sp_write_reg_or(RX_P0, R_DSC_CTRL_0, DSC_EN);

	API_DSI_Configuration(anx7625, table_id);
	API_Custom_Register1_Configuration(anx7625, table_id);

	/*set MIPI RX  EN*/
	sp_write_reg_or(RX_P0, AP_AV_STATUS, AP_MIPI_RX_EN);
	/*clear mute flag*/
	sp_write_reg_and(RX_P0, AP_AV_STATUS, ~AP_MIPI_MUTE);
}

static int anx7625_start(struct anx7625 *anx7625)
{
	/*not support HDCP*/
	sp_write_reg_and(RX_P1, 0xee, 0x9f);

	/*try auth flag*/
	sp_write_reg_or(RX_P1, 0xec, 0x10);
	/* interrupt for DRM*/
	sp_write_reg_or(RX_P1, 0xff, 0x01);

	if (anx7625->mode_idx < 10)
		DSI_Configuration(anx7625, anx7625->mode_idx);
	else
		DSI_DSC_Configuration(anx7625, anx7625->mode_idx);

	return 0;
}

static int anx7625_stop(struct anx7625 *anx7625)
{
	/*set mute flag*/
	sp_write_reg_or(RX_P0, AP_AV_STATUS, AP_MIPI_MUTE);

	/*clear mipi RX en*/
	sp_write_reg_and(RX_P0, AP_AV_STATUS, ~AP_MIPI_RX_EN);

	return 0;
}

#define STS_HPD_CHANGE \
(((sys_status&HPD_STATUS) != (anx7625->sys_sta_bak&HPD_STATUS)) ?\
	HPD_STATUS_CHANGE:0)

static void handle_intr_vector(struct anx7625 *anx7625)
{
	unsigned char sys_status;
	u8 intr_vector = ReadReg(RX_P0, INTERFACE_CHANGE_INT);

	WriteReg(RX_P0, INTERFACE_CHANGE_INT,
		intr_vector & (~intr_vector));

	sys_status = ReadReg(RX_P0, SYSTEM_STSTUS);

	if ((~INTR_MASK_SETTING) &
		((intr_vector & HPD_STATUS_CHANGE) | STS_HPD_CHANGE)) {
		if (!(sys_status & HPD_STATUS)) {
			anx7625->hpd_status = 0;
			TRACE1("HPD low\n");
			if (anx7625->enabled)
				anx7625_stop(anx7625);
		} else {
			anx7625->hpd_status = 1;
			TRACE1("HPD high\n");
			if (anx7625->enabled)
				anx7625_start(anx7625);
		}
	}

	anx7625->sys_sta_bak = sys_status;
}

static int anx7625_init_pdata(struct anx7625 *anx7625)
{
	struct anx7625_platform_data *pdata = &anx7625->pdata;
	struct device *dev = &anx7625->client->dev;

	/* GPIO for HPD */
	pdata->gpiod_cdet = devm_gpiod_get(dev, "cbl_det", GPIOD_IN);
	if (IS_ERR(pdata->gpiod_cdet))
		return PTR_ERR(pdata->gpiod_cdet);

	/* GPIO for chip power enable */
	pdata->gpiod_p_on = devm_gpiod_get(dev, "power_en", GPIOD_OUT_LOW);
	if (IS_ERR(pdata->gpiod_p_on))
		return PTR_ERR(pdata->gpiod_p_on);

	/* GPIO for chip reset */
	pdata->gpiod_reset = devm_gpiod_get(dev, "reset_n", GPIOD_OUT_LOW);

	return PTR_ERR_OR_ZERO(pdata->gpiod_reset);
}

static int anx7625_get_mode_idx(const struct drm_display_mode *mode)
{
	struct MIPI_Video_Format *fmt;
	int mode_idx = -1, categoly = 0, i;

	if (mode->htotal >= 3840)
		categoly = 1;

	for (i = 6; i < sizeof(mipi_video_timing_table) /
			sizeof(mipi_video_timing_table[0]); i++) {
		fmt = &mipi_video_timing_table[i];
		if (fmt->MIPI_pixel_frequency == mode->clock * 1000 &&
			fmt->MIPI_inputl[categoly].MIPI_HTOTAL ==
				mode->htotal &&
			fmt->MIPI_inputl[categoly].MIPI_VTOTAL ==
				mode->vtotal &&
			fmt->MIPI_inputl[categoly].MIPI_HActive ==
				mode->hdisplay &&
			fmt->MIPI_inputl[categoly].MIPI_VActive ==
				mode->vdisplay &&
			fmt->MIPI_inputl[categoly].MIPI_H_Front_Porch ==
				mode->hsync_start - mode->hdisplay &&
			fmt->MIPI_inputl[categoly].MIPI_H_Sync_Width ==
				mode->hsync_end - mode->hsync_start &&
			fmt->MIPI_inputl[categoly].MIPI_H_Back_Porch ==
				mode->htotal - mode->hsync_end &&
			fmt->MIPI_inputl[categoly].MIPI_V_Front_Porch ==
				mode->vsync_start - mode->vdisplay &&
			fmt->MIPI_inputl[categoly].MIPI_V_Sync_Width ==
				mode->vsync_end - mode->vsync_start &&
			fmt->MIPI_inputl[categoly].MIPI_V_Back_Porch ==
				mode->vtotal - mode->vsync_end) {
			mode_idx = i;
			break;
		}
	}

	return mode_idx;
}

static int anx7625_bridge_attach(struct drm_bridge *bridge)
{
	struct anx7625 *anx7625 = bridge_to_anx7625(bridge);
	int err;

	if (!bridge->encoder) {
		DRM_ERROR("Parent encoder object not found");
		return -ENODEV;
	}

	/* Register aux channel */
	anx7625->aux.name = "DP-AUX";
	anx7625->aux.dev = &anx7625->client->dev;
	anx7625->aux.transfer = anx7625_aux_transfer;

	err = drm_dp_aux_register(&anx7625->aux);
	if (err < 0) {
		DRM_ERROR("Failed to register aux channel: %d\n", err);
		return err;
	}

	return 0;
}

static enum drm_mode_status
anx7625_bridge_mode_valid(struct drm_bridge *bridge,
			  const struct drm_display_mode *mode)
{
	if (anx7625_get_mode_idx(mode) < 0) {
		pr_err("failed to find valid index\n");
		return MODE_NOMODE;
	}

	return MODE_OK;
}

static void anx7625_bridge_disable(struct drm_bridge *bridge)
{
	struct anx7625 *anx7625 = bridge_to_anx7625(bridge);

	mutex_lock(&anx7625->lock);

	anx7625_stop(anx7625);

	anx7625->enabled = false;

	mutex_unlock(&anx7625->lock);

	TRACE("anx7625 disabled\n");
}

static void anx7625_bridge_mode_set(struct drm_bridge *bridge,
				    struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode)
{
	struct anx7625 *anx7625 = bridge_to_anx7625(bridge);
	int mode_idx;

	mode_idx = anx7625_get_mode_idx(adjusted_mode);

	mutex_lock(&anx7625->lock);

	if (mode_idx >= 0)
		anx7625->mode_idx = mode_idx;
	else
		DRM_ERROR("Failed to find pre-defined mode for %s\n",
			mode->name);

	mutex_unlock(&anx7625->lock);
}

static void anx7625_bridge_enable(struct drm_bridge *bridge)
{
	struct anx7625 *anx7625 = bridge_to_anx7625(bridge);
	int err;

	mutex_lock(&anx7625->lock);

	anx7625->enabled = true;

	if (!anx7625->connected)
		DRM_ERROR("cable is not connected\n");

	if (!anx7625->hpd_status)
		DRM_ERROR("hpd is not set\n");

	err = anx7625_start(anx7625);
	if (err)
		DRM_ERROR("Failed to start: %d\n", err);

	mutex_unlock(&anx7625->lock);

	TRACE("anx7625 enabled\n");
}

static const struct drm_bridge_funcs anx7625_bridge_funcs = {
	.attach = anx7625_bridge_attach,
	.mode_valid = anx7625_bridge_mode_valid,
	.disable = anx7625_bridge_disable,
	.mode_set = anx7625_bridge_mode_set,
	.enable = anx7625_bridge_enable,
};

static irqreturn_t anx7625_cdet_threaded_handler(int irq, void *data)
{
	struct anx7625 *anx7625 = data;
	int connected;

	mutex_lock(&anx7625->lock);

	connected = gpiod_get_value_cansleep(anx7625->pdata.gpiod_cdet);

	if (anx7625->connected != connected) {
		anx7625->connected = connected;
		TRACE("cable status %d\n", connected);
	}

	mutex_unlock(&anx7625->lock);

	return IRQ_HANDLED;
}

static irqreturn_t anx7625_intp_threaded_handler(int unused, void *data)
{
	struct anx7625 *anx7625 = data;
	unsigned char c;

	mutex_lock(&anx7625->lock);

	c = ReadReg(TCPC_INTERFACE, INTR_ALERT_1);

	if (c & INTR_SOFTWARE_INT)
		handle_intr_vector(anx7625);

	while (ReadReg(RX_P0,
		INTERFACE_CHANGE_INT) != 0)
		handle_intr_vector(anx7625);

	if (c)
		WriteReg(TCPC_INTERFACE, INTR_ALERT_1, 0xFF);

	mutex_unlock(&anx7625->lock);

	return IRQ_HANDLED;
}

static const u16 anx7625_chipid_list[] = {
	0x7625,
};

static int anx7625_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{
	struct anx7625 *anx7625;
	struct anx7625_platform_data *pdata;
	unsigned int i, idl, idh, version[2];
	bool found = false;
	int err;

	anx7625 = devm_kzalloc(&client->dev, sizeof(*anx7625), GFP_KERNEL);
	if (!anx7625)
		return -ENOMEM;

	pdata = &anx7625->pdata;

	mutex_init(&anx7625->lock);

	anx7625->client = client;
	i2c_set_clientdata(client, anx7625);

	err = anx7625_init_pdata(anx7625);
	if (err) {
		DRM_ERROR("Failed to initialize pdata: %d\n", err);
		return err;
	}

	pdata->cdet_irq = gpiod_to_irq(pdata->gpiod_cdet);
	if (pdata->cdet_irq < 0) {
		DRM_ERROR("Failed to get CDET IRQ: %d\n", pdata->cdet_irq);
		return -ENODEV;
	}

	pdata->intp_irq = client->irq;
	if (!pdata->intp_irq) {
		DRM_ERROR("Failed to get INTP IRQ\n");
		return -ENODEV;
	}

	/* Power on chip */
	err = anx7625_poweron(anx7625);
	if (err)
		goto err_poweroff;

	/* Look for supported chip ID */
	err = Read_Reg(TCPC_INTERFACE, PRODUCT_ID_L, &idl);
	if (err)
		goto err_poweroff;

	err = Read_Reg(TCPC_INTERFACE, PRODUCT_ID_H, &idh);
	if (err)
		goto err_poweroff;

	err = Read_Reg(RX_P0, OCM_FW_VERSION, &version[0]);
	if (err)
		goto err_poweroff;

	err = Read_Reg(RX_P0, OCM_FW_REVERSION, &version[1]);
	if (err)
		goto err_poweroff;

	anx7625->chipid = (u8)idl | ((u8)idh << 8);

	for (i = 0; i < ARRAY_SIZE(anx7625_chipid_list); i++) {
		if (anx7625->chipid == anx7625_chipid_list[i]) {
			DRM_INFO("Found ANX%x (ver. %x%x) Transmitter\n",
				 anx7625->chipid, version[0], version[1]);
			found = true;
			break;
		}
	}

	if (!found) {
		DRM_ERROR("ANX%x (ver. %x%x) not supported by this driver\n",
			  anx7625->chipid, version[0], version[1]);
		err = -ENODEV;
		goto err_poweroff;
	}

	err = devm_request_threaded_irq(&client->dev, pdata->cdet_irq, NULL,
					anx7625_cdet_threaded_handler,
					IRQF_TRIGGER_RISING
					| IRQF_TRIGGER_RISING
					| IRQF_ONESHOT,
					"anx7625-hpd", anx7625);
	if (err) {
		DRM_ERROR("Failed to request CABLE_DET threaded IRQ: %d\n",
			  err);
		goto err_poweroff;
	}

	err = devm_request_threaded_irq(&client->dev, pdata->intp_irq, NULL,
					anx7625_intp_threaded_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"anx7625-intp", anx7625);
	if (err) {
		DRM_ERROR("Failed to request INTP threaded IRQ: %d\n", err);
		goto err_poweroff;
	}

#if IS_ENABLED(CONFIG_OF)
	anx7625->bridge.of_node = client->dev.of_node;
#endif

	anx7625->bridge.funcs = &anx7625_bridge_funcs;

	drm_bridge_add(&anx7625->bridge);

	/* init connected status */
	anx7625->connected =
		gpiod_get_value_cansleep(anx7625->pdata.gpiod_cdet);

	/* init hpd status */
	anx7625->sys_sta_bak = ReadReg(RX_P0, SYSTEM_STSTUS);
	anx7625->hpd_status = (anx7625->sys_sta_bak & HPD_STATUS) ?
		true : false;

	return 0;

err_poweroff:
	anx7625_poweroff(anx7625);
	DRM_ERROR("Failed to load anx7625 driver: %d\n", err);
	return err;
}

static int anx7625_i2c_remove(struct i2c_client *client)
{
	struct anx7625 *anx7625 = i2c_get_clientdata(client);

	anx7625_poweroff(anx7625);

	drm_bridge_remove(&anx7625->bridge);

	kfree(anx7625->edid);

	return 0;
}

static const struct i2c_device_id anx7625_id[] = {
	{ "anx7625", 0 },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(i2c, anx7625_id);

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id anx7625_id_match_table[] = {
	{ .compatible = "analogix,anx7625", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, anx7625_id_match_table);
#endif

static struct i2c_driver anx7625_driver = {
	.driver = {
		.name = "anx7625",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = anx7625_id_match_table,
#endif
	},
	.probe = anx7625_i2c_probe,
	.remove = anx7625_i2c_remove,
	.id_table = anx7625_id,
};

module_i2c_driver(anx7625_driver);
MODULE_DESCRIPTION("anx7625 driver");
MODULE_LICENSE("GPL v2");
