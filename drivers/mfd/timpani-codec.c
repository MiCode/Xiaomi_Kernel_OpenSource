/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/mfd/msm-adie-codec.h>
#include <linux/mfd/marimba.h>
#include <linux/mfd/timpani-audio.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/string.h>

/* Timpani codec driver is activated through Marimba core driver */

#define MAX_MDELAY_US 20000

#define TIMPANI_PATH_MASK(x) (1 << (x))

#define TIMPANI_CODEC_AUXPGA_GAIN_RANGE (0x0F)

#define TIMPANI_RX1_ST_MASK (TIMPANI_CDC_RX1_CTL_SIDETONE_EN1_L_M |\
		TIMPANI_CDC_RX1_CTL_SIDETONE_EN1_R_M)
#define TIMPANI_RX1_ST_ENABLE ((1 << TIMPANI_CDC_RX1_CTL_SIDETONE_EN1_L_S) |\
		(1 << TIMPANI_CDC_RX1_CTL_SIDETONE_EN1_R_S))
#define TIMPANI_CDC_ST_MIXING_TX1_MASK (TIMPANI_CDC_ST_MIXING_TX1_L_M |\
		TIMPANI_CDC_ST_MIXING_TX1_R_M)
#define TIMPANI_CDC_ST_MIXING_TX1_ENABLE ((1 << TIMPANI_CDC_ST_MIXING_TX1_L_S)\
		| (1 << TIMPANI_CDC_ST_MIXING_TX1_R_S))
#define TIMPANI_CDC_ST_MIXING_TX2_MASK (TIMPANI_CDC_ST_MIXING_TX2_L_M |\
		TIMPANI_CDC_ST_MIXING_TX2_R_M)
#define TIMPANI_CDC_ST_MIXING_TX2_ENABLE ((1 << TIMPANI_CDC_ST_MIXING_TX2_L_S)\
		| (1 << TIMPANI_CDC_ST_MIXING_TX2_R_S))

enum refcnt {
	DEC = 0,
	INC = 1,
	IGNORE = 2,
};
#define TIMPANI_ARRAY_SIZE	(TIMPANI_A_CDC_COMP_HALT + 1)
#define MAX_SHADOW_RIGISTERS	TIMPANI_A_CDC_COMP_HALT

static u8 timpani_shadow[TIMPANI_ARRAY_SIZE];

struct adie_codec_path {
	struct adie_codec_dev_profile *profile;
	struct adie_codec_register_image img;
	u32 hwsetting_idx;
	u32 stage_idx;
	u32 curr_stage;
	u32 reg_owner;
};

enum /* regaccess blk id */
{
	RA_BLOCK_RX1 = 0,
	RA_BLOCK_RX2,
	RA_BLOCK_TX1,
	RA_BLOCK_TX2,
	RA_BLOCK_LB,
	RA_BLOCK_SHARED_RX_LB,
	RA_BLOCK_SHARED_TX,
	RA_BLOCK_TXFE1,
	RA_BLOCK_TXFE2,
	RA_BLOCK_PA_COMMON,
	RA_BLOCK_PA_EAR,
	RA_BLOCK_PA_HPH,
	RA_BLOCK_PA_LINE,
	RA_BLOCK_PA_AUX,
	RA_BLOCK_ADC,
	RA_BLOCK_DMIC,
	RA_BLOCK_TX_I2S,
	RA_BLOCK_DRV,
	RA_BLOCK_TEST,
	RA_BLOCK_RESERVED,
	RA_BLOCK_NUM,
};

enum /* regaccess onwer ID */
{
	RA_OWNER_NONE = 0,
	RA_OWNER_PATH_RX1,
	RA_OWNER_PATH_RX2,
	RA_OWNER_PATH_TX1,
	RA_OWNER_PATH_TX2,
	RA_OWNER_PATH_LB,
	RA_OWNER_DRV,
	RA_OWNER_NUM,
};

struct reg_acc_blk_cfg {
	u8 valid_owners[RA_OWNER_NUM];
};

struct reg_ref_cnt {
	u8 mask;
	u8 path_mask;
};

#define TIMPANI_MAX_FIELDS	5

struct timpani_regaccess {
	u8 reg_addr;
	u8 blk_mask[RA_BLOCK_NUM];
	u8 reg_mask;
	u8 reg_default;
	struct reg_ref_cnt fld_ref_cnt[TIMPANI_MAX_FIELDS];
};

struct timpani_regaccess timpani_regset[] = {
	{
		TIMPANI_A_MREF,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFC, 0x0, 0x3},
		TIMPANI_MREF_M,
		TIMPANI_MREF_POR,
		{
			{ .mask = 0xFC, .path_mask = 0},
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDAC_IDAC_REF_CUR,
		{0xFC, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDAC_IDAC_REF_CUR_M,
		TIMPANI_CDAC_IDAC_REF_CUR_POR,
		{
			{ .mask = 0xFC, .path_mask = 0},
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXADC12_REF_CURR,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF},
		TIMPANI_TXADC12_REF_CURR_M,
		TIMPANI_TXADC12_REF_CURR_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXADC3_EN,
		{ 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFE, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1},
		TIMPANI_TXADC3_EN_M,
		TIMPANI_TXADC3_EN_POR,
		{
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXADC4_EN,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFE, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1},
		TIMPANI_TXADC4_EN_M,
		TIMPANI_TXADC4_EN_POR,
		{
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CODEC_TXADC_STATUS_REGISTER_1,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xC0, 0x30, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF},
		TIMPANI_CODEC_TXADC_STATUS_REGISTER_1_M,
		TIMPANI_CODEC_TXADC_STATUS_REGISTER_1_POR,
		{
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x30, .path_mask = 0},
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXFE1,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_TXFE1_M,
		TIMPANI_TXFE1_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXFE2,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_TXFE2_M,
		TIMPANI_TXFE2_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXFE12_ATEST,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_TXFE12_ATEST_M,
		TIMPANI_TXFE12_ATEST_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXFE_CLT,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF8, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7},
		TIMPANI_TXFE_CLT_M,
		TIMPANI_TXFE_CLT_POR,
		{
			{ .mask = 0xF8, .path_mask = 0},
			{ .mask = 0x07, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXADC1_EN,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFE, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1},
		TIMPANI_TXADC1_EN_M,
		TIMPANI_TXADC1_EN_POR,
		{
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXADC2_EN,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFE, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1},
		TIMPANI_TXADC2_EN_M,
		TIMPANI_TXADC2_EN_POR,
		{
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXADC_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_TXADC_CTL_M,
		TIMPANI_TXADC_CTL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXADC_CTL2,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_TXADC_CTL2_M,
		TIMPANI_TXADC_CTL2_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXADC_CTL3,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0xFE, 0x0, 0x0, 0x0, 0x0, 0x1},
		TIMPANI_TXADC_CTL3_M,
		TIMPANI_TXADC_CTL3_POR,
		{
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXADC_CHOP_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0xFC, 0x0, 0x0, 0x0, 0x0, 0x3},
		TIMPANI_TXADC_CHOP_CTL_M,
		TIMPANI_TXADC_CHOP_CTL_POR,
		{
			{ .mask = 0xFC, .path_mask = 0},
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXFE3,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xE2, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1D},
		TIMPANI_TXFE3_M,
		TIMPANI_TXFE3_POR,
		{
			{ .mask = 0xE2, .path_mask = 0},
			{ .mask = 0x1D, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXFE4,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xE2, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1D},
		TIMPANI_TXFE4_M,
		TIMPANI_TXFE4_POR,
		{
			{ .mask = 0xE2, .path_mask = 0},
			{ .mask = 0x1D, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXFE3_ATEST,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_TXFE3_ATEST_M,
		TIMPANI_TXFE3_ATEST_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_TXFE_DIFF_SE,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0xC, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF0},
		TIMPANI_TXFE_DIFF_SE_M,
		TIMPANI_TXFE_DIFF_SE_POR,
		{
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0x0C, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDAC_RX_CLK_CTL,
		{0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDAC_RX_CLK_CTL_M,
		TIMPANI_CDAC_RX_CLK_CTL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDAC_BUFF_CTL,
		{0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDAC_BUFF_CTL_M,
		TIMPANI_CDAC_BUFF_CTL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDAC_REF_CTL1,
		{0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDAC_REF_CTL1_M,
		TIMPANI_CDAC_REF_CTL1_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_IDAC_DWA_FIR_CTL,
		{0xF8, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7},
		TIMPANI_IDAC_DWA_FIR_CTL_M,
		TIMPANI_IDAC_DWA_FIR_CTL_POR,
		{
			{ .mask = 0xF8, .path_mask = 0},
			{ .mask = 0x07, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDAC_REF_CTL2,
		{0x6F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x90},
		TIMPANI_CDAC_REF_CTL2_M,
		TIMPANI_CDAC_REF_CTL2_POR,
		{
			{ .mask = 0x6F, .path_mask = 0},
			{ .mask = 0x90, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDAC_CTL1,
		{0x7F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80},
		TIMPANI_CDAC_CTL1_M,
		TIMPANI_CDAC_CTL1_POR,
		{
			{ .mask = 0x7F, .path_mask = 0},
			{ .mask = 0x80, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDAC_CTL2,
		{0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDAC_CTL2_M,
		TIMPANI_CDAC_CTL2_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_IDAC_L_CTL,
		{0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_IDAC_L_CTL_M,
		TIMPANI_IDAC_L_CTL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_IDAC_R_CTL,
		{0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_IDAC_R_CTL_M,
		TIMPANI_IDAC_R_CTL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_MASTER_BIAS,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1F,
		0xE0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_MASTER_BIAS_M,
		TIMPANI_PA_MASTER_BIAS_POR,
		{
			{ .mask = 0x1F, .path_mask = 0},
			{ .mask = 0xE0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CLASSD_BIAS,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_CLASSD_BIAS_M,
		TIMPANI_PA_CLASSD_BIAS_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_AUXPGA_CUR,
		{0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_AUXPGA_CUR_M,
		TIMPANI_AUXPGA_CUR_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_AUXPGA_CM,
		{0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_AUXPGA_CM_M,
		TIMPANI_AUXPGA_CM_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_HPH_EARPA_MSTB_EN,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x2, 0xFC,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_HPH_EARPA_MSTB_EN_M,
		TIMPANI_PA_HPH_EARPA_MSTB_EN_POR,
		{
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0x02, .path_mask = 0},
			{ .mask = 0xFC, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_LINE_AUXO_EN,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0xF8, 0x7, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_LINE_AUXO_EN_M,
		TIMPANI_PA_LINE_AUXO_EN_POR,
		{
			{ .mask = 0xF8, .path_mask = 0},
			{ .mask = 0x07, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CLASSD_AUXPGA_EN,
		{0x0, 0x0, 0x0, 0x0, 0x30, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xC0},
		TIMPANI_PA_CLASSD_AUXPGA_EN_M,
		TIMPANI_PA_CLASSD_AUXPGA_EN_POR,
		{
			{ .mask = 0x30, .path_mask = 0},
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_LINE_L_GAIN,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFC, 0x0, 0x3},
		TIMPANI_PA_LINE_L_GAIN_M,
		TIMPANI_PA_LINE_L_GAIN_POR,
		{
			{ .mask = 0xFC, .path_mask = 0},
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_LINE_R_GAIN,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFC, 0x0, 0x3},
		TIMPANI_PA_LINE_R_GAIN_M,
		TIMPANI_PA_LINE_R_GAIN_POR,
		{
			{ .mask = 0xFC, .path_mask = 0},
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_HPH_L_GAIN,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFE, 0x0, 0x1},
		TIMPANI_PA_HPH_L_GAIN_M,
		TIMPANI_PA_HPH_L_GAIN_POR,
		{
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_HPH_R_GAIN,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFE, 0x0, 0x1},
		TIMPANI_PA_HPH_R_GAIN_M,
		TIMPANI_PA_HPH_R_GAIN_POR,
		{
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_AUXPGA_LR_GAIN,
		{0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_AUXPGA_LR_GAIN_M,
		TIMPANI_AUXPGA_LR_GAIN_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_AUXO_EARPA_CONN,
		{0x21, 0x42, 0x0, 0x0, 0x84, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x18},
		TIMPANI_PA_AUXO_EARPA_CONN_M,
		TIMPANI_PA_AUXO_EARPA_CONN_POR,
		{
			{ .mask = 0x21, .path_mask = 0},
			{ .mask = 0x42, .path_mask = 0},
			{ .mask = 0x84, .path_mask = 0},
			{ .mask = 0x18, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_LINE_ST_CONN,
		{0x24, 0x48, 0x0, 0x0, 0x93, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_LINE_ST_CONN_M,
		TIMPANI_PA_LINE_ST_CONN_POR,
		{
			{ .mask = 0x24, .path_mask = 0},
			{ .mask = 0x48, .path_mask = 0},
			{ .mask = 0x93, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_LINE_MONO_CONN,
		{0x24, 0x48, 0x0, 0x0, 0x93, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_LINE_MONO_CONN_M,
		TIMPANI_PA_LINE_MONO_CONN_POR,
		{
			{ .mask = 0x24, .path_mask = 0},
			{ .mask = 0x48, .path_mask = 0},
			{ .mask = 0x93, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_HPH_ST_CONN,
		{0x24, 0x48, 0x0, 0x0, 0x90, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_HPH_ST_CONN_M,
		TIMPANI_PA_HPH_ST_CONN_POR,
		{
			{ .mask = 0x24, .path_mask = 0},
			{ .mask = 0x48, .path_mask = 0},
			{ .mask = 0x90, .path_mask = 0},
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_HPH_MONO_CONN,
		{0x24, 0x48, 0x0, 0x0, 0x90, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3},
		TIMPANI_PA_HPH_MONO_CONN_M,
		TIMPANI_PA_HPH_MONO_CONN_POR,
		{
			{ .mask = 0x24, .path_mask = 0},
			{ .mask = 0x48, .path_mask = 0},
			{ .mask = 0x90, .path_mask = 0},
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CLASSD_CONN,
		{0x80, 0x40, 0x0, 0x0, 0x20, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x10,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF},
		TIMPANI_PA_CLASSD_CONN_M,
		TIMPANI_PA_CLASSD_CONN_POR,
		{
			{ .mask = 0x80, .path_mask = 0},
			{ .mask = 0x40, .path_mask = 0},
			{ .mask = 0x20, .path_mask = 0},
			{ .mask = 0x10, .path_mask = 0},
			{ .mask = 0x0F, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CNP_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xCF,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x30},
		TIMPANI_PA_CNP_CTL_M,
		TIMPANI_PA_CNP_CTL_POR,
		{
			{ .mask = 0xCF, .path_mask = 0},
			{ .mask = 0x30, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CLASSD_L_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3F,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xC0},
		TIMPANI_PA_CLASSD_L_CTL_M,
		TIMPANI_PA_CLASSD_L_CTL_POR,
		{
			{ .mask = 0x3F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CLASSD_R_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3F,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xC0},
		TIMPANI_PA_CLASSD_R_CTL_M,
		TIMPANI_PA_CLASSD_R_CTL_POR,
		{
			{ .mask = 0x3F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CLASSD_INT2_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_CLASSD_INT2_CTL_M,
		TIMPANI_PA_CLASSD_INT2_CTL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_HPH_L_OCP_CLK_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_HPH_L_OCP_CLK_CTL_M,
		TIMPANI_PA_HPH_L_OCP_CLK_CTL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CLASSD_L_SW_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF7,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8},
		TIMPANI_PA_CLASSD_L_SW_CTL_M,
		TIMPANI_PA_CLASSD_L_SW_CTL_POR,
		{
			{ .mask = 0xF7, .path_mask = 0},
			{ .mask = 0x08, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CLASSD_L_OCP1,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_CLASSD_L_OCP1_M,
		TIMPANI_PA_CLASSD_L_OCP1_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CLASSD_L_OCP2,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_CLASSD_L_OCP2_M,
		TIMPANI_PA_CLASSD_L_OCP2_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_HPH_R_OCP_CLK_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_HPH_R_OCP_CLK_CTL_M,
		TIMPANI_PA_HPH_R_OCP_CLK_CTL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CLASSD_R_SW_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF7,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x8},
		TIMPANI_PA_CLASSD_R_SW_CTL_M,
		TIMPANI_PA_CLASSD_R_SW_CTL_POR,
		{
			{ .mask = 0xF7, .path_mask = 0},
			{ .mask = 0x08, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CLASSD_R_OCP1,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_CLASSD_R_OCP1_M,
		TIMPANI_PA_CLASSD_R_OCP1_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CLASSD_R_OCP2,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_CLASSD_R_OCP2_M,
		TIMPANI_PA_CLASSD_R_OCP2_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_HPH_CTL1,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF},
		TIMPANI_PA_HPH_CTL1_M,
		TIMPANI_PA_HPH_CTL1_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_HPH_CTL2,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFE,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1},
		TIMPANI_PA_HPH_CTL2_M,
		TIMPANI_PA_HPH_CTL2_POR,
		{
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_LINE_AUXO_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0xC3, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x3C, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_LINE_AUXO_CTL_M,
		TIMPANI_PA_LINE_AUXO_CTL_POR,
		{
			{ .mask = 0xC3, .path_mask = 0},
			{ .mask = 0x3C, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_AUXO_EARPA_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7, 0x0,
		0x0, 0x38, 0x0, 0x0, 0x0, 0x0, 0x0, 0xC0},
		TIMPANI_PA_AUXO_EARPA_CTL_M,
		TIMPANI_PA_AUXO_EARPA_CTL_POR,
		{
			{ .mask = 0x07, .path_mask = 0},
			{ .mask = 0x38, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_EARO_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_PA_EARO_CTL_M,
		TIMPANI_PA_EARO_CTL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_MASTER_BIAS_CUR,
		{0x0, 0x0, 0x0, 0x0, 0x60, 0x0, 0x0, 0x0, 0x0, 0x80, 0x0, 0x18,
		0x6, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1},
		TIMPANI_PA_MASTER_BIAS_CUR_M,
		TIMPANI_PA_MASTER_BIAS_CUR_POR,
		{
			{ .mask = 0x60, .path_mask = 0},
			{ .mask = 0x80, .path_mask = 0},
			{ .mask = 0x18, .path_mask = 0},
			{ .mask = 0x06, .path_mask = 0},
			{ .mask = 0x01, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_CLASSD_SC_STATUS,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xCC,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x33},
		TIMPANI_PA_CLASSD_SC_STATUS_M,
		TIMPANI_PA_CLASSD_SC_STATUS_POR,
		{
			{ .mask = 0xCC, .path_mask = 0},
			{ .mask = 0x33, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_PA_HPH_SC_STATUS,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x88,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x77},
		TIMPANI_PA_HPH_SC_STATUS_M,
		TIMPANI_PA_HPH_SC_STATUS_POR,
		{
			{ .mask = 0x88, .path_mask = 0},
			{ .mask = 0x77, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_ATEST_EN,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80, 0x7F},
		TIMPANI_ATEST_EN_M,
		TIMPANI_ATEST_EN_POR,
		{
			{ .mask = 0x80, .path_mask = 0},
			{ .mask = 0x7F, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_ATEST_TSHKADC,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF, 0xF0},
		TIMPANI_ATEST_TSHKADC_M,
		TIMPANI_ATEST_TSHKADC_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_ATEST_TXADC13,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7F, 0x80},
		TIMPANI_ATEST_TXADC13_M,
		TIMPANI_ATEST_TXADC13_POR,
		{
			{ .mask = 0x7F, .path_mask = 0},
			{ .mask = 0x80, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_ATEST_TXADC24,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x7F, 0x80},
		TIMPANI_ATEST_TXADC24_M,
		TIMPANI_ATEST_TXADC24_POR,
		{
			{ .mask = 0x7F, .path_mask = 0},
			{ .mask = 0x80, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_ATEST_AUXPGA,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF8, 0x7},
		TIMPANI_ATEST_AUXPGA_M,
		TIMPANI_ATEST_AUXPGA_POR,
		{
			{ .mask = 0xF8, .path_mask = 0},
			{ .mask = 0x07, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_ATEST_CDAC,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0},
		TIMPANI_ATEST_CDAC_M,
		TIMPANI_ATEST_CDAC_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_ATEST_IDAC,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0},
		TIMPANI_ATEST_IDAC_M,
		TIMPANI_ATEST_IDAC_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_ATEST_PA1,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0},
		TIMPANI_ATEST_PA1_M,
		TIMPANI_ATEST_PA1_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_ATEST_CLASSD,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0},
		TIMPANI_ATEST_CLASSD_M,
		TIMPANI_ATEST_CLASSD_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_ATEST_LINEO_AUXO,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0},
		TIMPANI_ATEST_LINEO_AUXO_M,
		TIMPANI_ATEST_LINEO_AUXO_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RESET_CTL,
		{0x2, 0x8, 0x5, 0x30, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xC0},
		TIMPANI_CDC_RESET_CTL_M,
		TIMPANI_CDC_RESET_CTL_POR,
		{
			{ .mask = 0x02, .path_mask = 0},
			{ .mask = 0x08, .path_mask = 0},
			{ .mask = 0x05, .path_mask = 0},
			{ .mask = 0x30, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX1_CTL,
		{0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_RX1_CTL_M,
		TIMPANI_CDC_RX1_CTL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TX_I2S_CTL,
		{0x0, 0x0, 0x10, 0x20, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0xF, 0x0, 0x0, 0xC0},
		TIMPANI_CDC_TX_I2S_CTL_M,
		TIMPANI_CDC_TX_I2S_CTL_POR,
		{
			{ .mask = 0x10, .path_mask = 0},
			{ .mask = 0x20, .path_mask = 0},
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_CH_CTL,
		{0x3, 0x30, 0xC, 0xC0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_CH_CTL_M,
		TIMPANI_CDC_CH_CTL_POR,
		{
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0x30, .path_mask = 0},
			{ .mask = 0x0C, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX1LG,
		{0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_RX1LG_M,
		TIMPANI_CDC_RX1LG_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX1RG,
		{0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_RX1RG_M,
		TIMPANI_CDC_RX1RG_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TX1LG,
		{0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_TX1LG_M,
		TIMPANI_CDC_TX1LG_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TX1RG,
		{0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_TX1RG_M,
		TIMPANI_CDC_TX1RG_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX_PGA_TIMER,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_RX_PGA_TIMER_M,
		TIMPANI_CDC_RX_PGA_TIMER_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TX_PGA_TIMER,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_TX_PGA_TIMER_M,
		TIMPANI_CDC_TX_PGA_TIMER_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_GCTL1,
		{0xF, 0x0, 0xF0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_GCTL1_M,
		TIMPANI_CDC_GCTL1_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TX1L_STG,
		{0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_TX1L_STG_M,
		TIMPANI_CDC_TX1L_STG_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ST_CTL,
		{0x0, 0xF, 0x0, 0xF0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_ST_CTL_M,
		TIMPANI_CDC_ST_CTL_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX1L_DCOFFSET,
		{0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_RX1L_DCOFFSET_M,
		TIMPANI_CDC_RX1L_DCOFFSET_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX1R_DCOFFSET,
		{0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_RX1R_DCOFFSET_M,
		TIMPANI_CDC_RX1R_DCOFFSET_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_BYPASS_CTL1,
		{0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF0},
		TIMPANI_CDC_BYPASS_CTL1_M,
		TIMPANI_CDC_BYPASS_CTL1_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_PDM_CONFIG,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF, 0xF0},
		TIMPANI_CDC_PDM_CONFIG_M,
		TIMPANI_CDC_PDM_CONFIG_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TESTMODE1,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3F, 0xC0},
		TIMPANI_CDC_TESTMODE1_M,
		TIMPANI_CDC_TESTMODE1_POR,
		{
			{ .mask = 0x3F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_DMIC_CLK_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x3F, 0x0, 0x0, 0x0, 0xC0},
		TIMPANI_CDC_DMIC_CLK_CTL_M,
		TIMPANI_CDC_DMIC_CLK_CTL_POR,
		{
			{ .mask = 0x3F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ADC12_CLK_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_ADC12_CLK_CTL_M,
		TIMPANI_CDC_ADC12_CLK_CTL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TX1_CTL,
		{0x0, 0x0, 0x3F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xC0},
		TIMPANI_CDC_TX1_CTL_M,
		TIMPANI_CDC_TX1_CTL_POR,
		{
			{ .mask = 0x3F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ADC34_CLK_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_ADC34_CLK_CTL_M,
		TIMPANI_CDC_ADC34_CLK_CTL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TX2_CTL,
		{0x0, 0x0, 0x0, 0x3F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xC0},
		TIMPANI_CDC_TX2_CTL_M,
		TIMPANI_CDC_TX2_CTL_POR,
		{
			{ .mask = 0x3F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX1_CLK_CTL,
		{0x1F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xE0},
		TIMPANI_CDC_RX1_CLK_CTL_M,
		TIMPANI_CDC_RX1_CLK_CTL_POR,
		{
			{ .mask = 0x1F, .path_mask = 0},
			{ .mask = 0xE0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX2_CLK_CTL,
		{0x1F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xE0},
		TIMPANI_CDC_RX2_CLK_CTL_M,
		TIMPANI_CDC_RX2_CLK_CTL_POR,
		{
			{ .mask = 0x1F, .path_mask = 0},
			{ .mask = 0xE0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_DEC_ADC_SEL,
		{0x0, 0x0, 0xF, 0xF0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_DEC_ADC_SEL_M,
		TIMPANI_CDC_DEC_ADC_SEL_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC_INPUT_MUX,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x3F, 0x0, 0xC0},
		TIMPANI_CDC_ANC_INPUT_MUX_M,
		TIMPANI_CDC_ANC_INPUT_MUX_POR,
		{
			{ .mask = 0x3F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC_RX_CLK_NS_SEL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0xFE},
		TIMPANI_CDC_ANC_RX_CLK_NS_SEL_M,
		TIMPANI_CDC_ANC_RX_CLK_NS_SEL_POR,
		{
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC_FB_TUNE_SEL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF},
		TIMPANI_CDC_ANC_FB_TUNE_SEL_M,
		TIMPANI_CDC_ANC_FB_TUNE_SEL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CLK_DIV_SYNC_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0x0, 0xFC},
		TIMPANI_CLK_DIV_SYNC_CTL_M,
		TIMPANI_CLK_DIV_SYNC_CTL_POR,
		{
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0xFC, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ADC_CLK_EN,
		{0x0, 0x0, 0x3, 0xC, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF0},
		TIMPANI_CDC_ADC_CLK_EN_M,
		TIMPANI_CDC_ADC_CLK_EN_POR,
		{
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0x0C, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ST_MIXING,
		{0x0, 0x0, 0x3, 0xC, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF0},
		TIMPANI_CDC_ST_MIXING_M,
		TIMPANI_CDC_ST_MIXING_POR,
		{
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0x0C, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX2_CTL,
		{0x0, 0x7F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80},
		TIMPANI_CDC_RX2_CTL_M,
		TIMPANI_CDC_RX2_CTL_POR,
		{
			{ .mask = 0x7F, .path_mask = 0},
			{ .mask = 0x80, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ARB_CLK_EN,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF},
		TIMPANI_CDC_ARB_CLK_EN_M,
		TIMPANI_CDC_ARB_CLK_EN_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_I2S_CTL2,
		{0x2, 0x4, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x39, 0x0, 0x0, 0xC0},
		TIMPANI_CDC_I2S_CTL2_M,
		TIMPANI_CDC_I2S_CTL2_POR,
		{
			{ .mask = 0x02, .path_mask = 0},
			{ .mask = 0x04, .path_mask = 0},
			{ .mask = 0x39, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX2LG,
		{0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_RX2LG_M,
		TIMPANI_CDC_RX2LG_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX2RG,
		{0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_RX2RG_M,
		TIMPANI_CDC_RX2RG_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TX2LG,
		{0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_TX2LG_M,
		TIMPANI_CDC_TX2LG_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TX2RG,
		{0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_TX2RG_M,
		TIMPANI_CDC_TX2RG_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_DMIC_MUX,
		{0x0, 0x0, 0xF, 0xF0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_DMIC_MUX_M,
		TIMPANI_CDC_DMIC_MUX_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ARB_CLK_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0x0, 0xFC},
		TIMPANI_CDC_ARB_CLK_CTL_M,
		TIMPANI_CDC_ARB_CLK_CTL_POR,
		{
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0xFC, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_GCTL2,
		{0x0, 0xF, 0x0, 0xF0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_GCTL2_M,
		TIMPANI_CDC_GCTL2_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_BYPASS_CTL2,
		{0x0, 0x0, 0x3F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xC0},
		TIMPANI_CDC_BYPASS_CTL2_M,
		TIMPANI_CDC_BYPASS_CTL2_POR,
		{
			{ .mask = 0x3F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_BYPASS_CTL3,
		{0x0, 0x0, 0x0, 0x3F, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xC0},
		TIMPANI_CDC_BYPASS_CTL3_M,
		TIMPANI_CDC_BYPASS_CTL3_POR,
		{
			{ .mask = 0x3F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_BYPASS_CTL4,
		{0x0, 0xF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xF0},
		TIMPANI_CDC_BYPASS_CTL4_M,
		TIMPANI_CDC_BYPASS_CTL4_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX2L_DCOFFSET,
		{0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_RX2L_DCOFFSET_M,
		TIMPANI_CDC_RX2L_DCOFFSET_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX2R_DCOFFSET,
		{0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_RX2R_DCOFFSET_M,
		TIMPANI_CDC_RX2R_DCOFFSET_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_RX_MIX_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0x0, 0xFC},
		TIMPANI_CDC_RX_MIX_CTL_M,
		TIMPANI_CDC_RX_MIX_CTL_POR,
		{
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0xFC, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_SPARE_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0xFE},
		TIMPANI_CDC_SPARE_CTL_M,
		TIMPANI_CDC_SPARE_CTL_POR,
		{
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TESTMODE2,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1F, 0xE0},
		TIMPANI_CDC_TESTMODE2_M,
		TIMPANI_CDC_TESTMODE2_POR,
		{
			{ .mask = 0x1F, .path_mask = 0},
			{ .mask = 0xE0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_PDM_OE,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0},
		TIMPANI_CDC_PDM_OE_M,
		TIMPANI_CDC_PDM_OE_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TX1R_STG,
		{0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_TX1R_STG_M,
		TIMPANI_CDC_TX1R_STG_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TX2L_STG,
		{0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_TX2L_STG_M,
		TIMPANI_CDC_TX2L_STG_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_TX2R_STG,
		{0x0, 0x0, 0x0, 0xFF, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
		TIMPANI_CDC_TX2R_STG_M,
		TIMPANI_CDC_TX2R_STG_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ARB_BYPASS_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF},
		TIMPANI_CDC_ARB_BYPASS_CTL_M,
		TIMPANI_CDC_ARB_BYPASS_CTL_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_CTL1,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x1F, 0x0, 0xE0},
		TIMPANI_CDC_ANC1_CTL1_M,
		TIMPANI_CDC_ANC1_CTL1_POR,
		{
			{ .mask = 0x1F, .path_mask = 0},
			{ .mask = 0xE0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_CTL2,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x3F, 0x0, 0xC0},
		TIMPANI_CDC_ANC1_CTL2_M,
		TIMPANI_CDC_ANC1_CTL2_POR,
		{
			{ .mask = 0x3F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_FF_FB_SHIFT,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_ANC1_FF_FB_SHIFT_M,
		TIMPANI_CDC_ANC1_FF_FB_SHIFT_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_RX_NS,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x7, 0x0, 0xF8},
		TIMPANI_CDC_ANC1_RX_NS_M,
		TIMPANI_CDC_ANC1_RX_NS_POR,
		{
			{ .mask = 0x07, .path_mask = 0},
			{ .mask = 0xF8, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_SPARE,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_ANC1_SPARE_M,
		TIMPANI_CDC_ANC1_SPARE_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_IIR_COEFF_PTR,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x1F, 0x0, 0xE0},
		TIMPANI_CDC_ANC1_IIR_COEFF_PTR_M,
		TIMPANI_CDC_ANC1_IIR_COEFF_PTR_POR,
		{
			{ .mask = 0x1F, .path_mask = 0},
			{ .mask = 0xE0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_IIR_COEFF_MSB,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0xFE},
		TIMPANI_CDC_ANC1_IIR_COEFF_MSB_M,
		TIMPANI_CDC_ANC1_IIR_COEFF_MSB_POR,
		{
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_IIR_COEFF_LSB,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_ANC1_IIR_COEFF_LSB_M,
		TIMPANI_CDC_ANC1_IIR_COEFF_LSB_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_IIR_COEFF_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0x0, 0xFC},
		TIMPANI_CDC_ANC1_IIR_COEFF_CTL_M,
		TIMPANI_CDC_ANC1_IIR_COEFF_CTL_POR,
		{
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0xFC, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_LPF_COEFF_PTR,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xF, 0x0, 0xF0},
		TIMPANI_CDC_ANC1_LPF_COEFF_PTR_M,
		TIMPANI_CDC_ANC1_LPF_COEFF_PTR_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_LPF_COEFF_MSB,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xF, 0x0, 0xF0},
		TIMPANI_CDC_ANC1_LPF_COEFF_MSB_M,
		TIMPANI_CDC_ANC1_LPF_COEFF_MSB_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_LPF_COEFF_LSB,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_ANC1_LPF_COEFF_LSB_M,
		TIMPANI_CDC_ANC1_LPF_COEFF_LSB_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_SCALE_PTR,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_ANC1_SCALE_PTR_M,
		TIMPANI_CDC_ANC1_SCALE_PTR_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_SCALE,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_ANC1_SCALE_M,
		TIMPANI_CDC_ANC1_SCALE_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC1_DEBUG,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xF, 0x0, 0xF0},
		TIMPANI_CDC_ANC1_DEBUG_M,
		TIMPANI_CDC_ANC1_DEBUG_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_CTL1,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x1F, 0x0, 0xE0},
		TIMPANI_CDC_ANC2_CTL1_M,
		TIMPANI_CDC_ANC2_CTL1_POR,
		{
			{ .mask = 0x1F, .path_mask = 0},
			{ .mask = 0xE0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_CTL2,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x3F, 0x0, 0xC0},
		TIMPANI_CDC_ANC2_CTL2_M,
		TIMPANI_CDC_ANC2_CTL2_POR,
		{
			{ .mask = 0x3F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_FF_FB_SHIFT,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_ANC2_FF_FB_SHIFT_M,
		TIMPANI_CDC_ANC2_FF_FB_SHIFT_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_RX_NS,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x7, 0x0, 0xF8},
		TIMPANI_CDC_ANC2_RX_NS_M,
		TIMPANI_CDC_ANC2_RX_NS_POR,
		{
			{ .mask = 0x07, .path_mask = 0},
			{ .mask = 0xF8, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_SPARE,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_ANC2_SPARE_M,
		TIMPANI_CDC_ANC2_SPARE_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_IIR_COEFF_PTR,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xF, 0x0, 0xF0},
		TIMPANI_CDC_ANC2_IIR_COEFF_PTR_M,
		TIMPANI_CDC_ANC2_IIR_COEFF_PTR_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_IIR_COEFF_MSB,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x1, 0x0, 0xFE},
		TIMPANI_CDC_ANC2_IIR_COEFF_MSB_M,
		TIMPANI_CDC_ANC2_IIR_COEFF_MSB_POR,
		{
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_IIR_COEFF_LSB,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_ANC2_IIR_COEFF_LSB_M,
		TIMPANI_CDC_ANC2_IIR_COEFF_LSB_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_IIR_COEFF_CTL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x3, 0x0, 0xFC},
		TIMPANI_CDC_ANC2_IIR_COEFF_CTL_M,
		TIMPANI_CDC_ANC2_IIR_COEFF_CTL_POR,
		{
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0xFC, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_LPF_COEFF_PTR,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xF, 0x0, 0xF0},
		TIMPANI_CDC_ANC2_LPF_COEFF_PTR_M,
		TIMPANI_CDC_ANC2_LPF_COEFF_PTR_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_LPF_COEFF_MSB,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xF, 0x0, 0xF0},
		TIMPANI_CDC_ANC2_LPF_COEFF_MSB_M,
		TIMPANI_CDC_ANC2_LPF_COEFF_MSB_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_LPF_COEFF_LSB,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_ANC2_LPF_COEFF_LSB_M,
		TIMPANI_CDC_ANC2_LPF_COEFF_LSB_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_SCALE_PTR,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_ANC2_SCALE_PTR_M,
		TIMPANI_CDC_ANC2_SCALE_PTR_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_SCALE,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_ANC2_SCALE_M,
		TIMPANI_CDC_ANC2_SCALE_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_ANC2_DEBUG,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xF, 0x0, 0xF0},
		TIMPANI_CDC_ANC2_DEBUG_M,
		TIMPANI_CDC_ANC2_DEBUG_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_LINE_L_AVOL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0xFC, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3},
		TIMPANI_CDC_LINE_L_AVOL_M,
		TIMPANI_CDC_LINE_L_AVOL_POR,
		{
			{ .mask = 0xFC, .path_mask = 0},
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_LINE_R_AVOL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0xFC, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x3},
		TIMPANI_CDC_LINE_R_AVOL_M,
		TIMPANI_CDC_LINE_R_AVOL_POR,
		{
			{ .mask = 0xFC, .path_mask = 0},
			{ .mask = 0x03, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_HPH_L_AVOL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFE,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1},
		TIMPANI_CDC_HPH_L_AVOL_M,
		TIMPANI_CDC_HPH_L_AVOL_POR,
		{
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_HPH_R_AVOL,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFE,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x1},
		TIMPANI_CDC_HPH_R_AVOL_M,
		TIMPANI_CDC_HPH_R_AVOL_POR,
		{
			{ .mask = 0xFE, .path_mask = 0},
			{ .mask = 0x01, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_COMP_CTL1,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x3F, 0x0, 0xC0},
		TIMPANI_CDC_COMP_CTL1_M,
		TIMPANI_CDC_COMP_CTL1_POR,
		{
			{ .mask = 0x3F, .path_mask = 0},
			{ .mask = 0xC0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_COMP_CTL2,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xF, 0x0, 0xF0},
		TIMPANI_CDC_COMP_CTL2_M,
		TIMPANI_CDC_COMP_CTL2_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_COMP_PEAK_METER,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xF, 0x0, 0xF0},
		TIMPANI_CDC_COMP_PEAK_METER_M,
		TIMPANI_CDC_COMP_PEAK_METER_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_COMP_LEVEL_METER_CTL1,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xF, 0x0, 0xF0},
		TIMPANI_CDC_COMP_LEVEL_METER_CTL1_M,
		TIMPANI_CDC_COMP_LEVEL_METER_CTL1_POR,
		{
			{ .mask = 0x0F, .path_mask = 0},
			{ .mask = 0xF0, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_COMP_LEVEL_METER_CTL2,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_COMP_LEVEL_METER_CTL2_M,
		TIMPANI_CDC_COMP_LEVEL_METER_CTL2_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_COMP_ZONE_SELECT,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x7F, 0x0, 0x80},
		TIMPANI_CDC_COMP_ZONE_SELECT_M,
		TIMPANI_CDC_COMP_ZONE_SELECT_POR,
		{
			{ .mask = 0x7F, .path_mask = 0},
			{ .mask = 0x80, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_COMP_ZC_MSB,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_COMP_ZC_MSB_M,
		TIMPANI_CDC_COMP_ZC_MSB_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_COMP_ZC_LSB,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0xFF, 0x0, 0x0},
		TIMPANI_CDC_COMP_ZC_LSB_M,
		TIMPANI_CDC_COMP_ZC_LSB_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_COMP_SHUT_DOWN,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF},
		TIMPANI_CDC_COMP_SHUT_DOWN_M,
		TIMPANI_CDC_COMP_SHUT_DOWN_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_COMP_SHUT_DOWN_STATUS,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF},
		TIMPANI_CDC_COMP_SHUT_DOWN_STATUS_M,
		TIMPANI_CDC_COMP_SHUT_DOWN_STATUS_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	},
	{
		TIMPANI_A_CDC_COMP_HALT,
		{0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xFF},
		TIMPANI_CDC_COMP_HALT_M,
		TIMPANI_CDC_COMP_HALT_POR,
		{
			{ .mask = 0xFF, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
			{ .mask = 0x00, .path_mask = 0},
		}
	}
};

struct reg_acc_blk_cfg timpani_blkcfg[RA_BLOCK_NUM] = {
	{
		.valid_owners = {RA_OWNER_NONE, RA_OWNER_PATH_RX1,
		0, 0, 0, 0, RA_OWNER_DRV}
	},
	/* RA_BLOCK_RX1 */
	{
		.valid_owners = {RA_OWNER_NONE, 0, RA_OWNER_PATH_RX2,
		0, 0, 0, RA_OWNER_DRV}
	},
	/* RA_BLOCK_RX2 */
	{
		.valid_owners = {RA_OWNER_NONE, 0, 0, RA_OWNER_PATH_TX1,
		0, 0, RA_OWNER_DRV}
	},
	/* RA_BLOCK_TX1 */
	{
		.valid_owners = {RA_OWNER_NONE, 0, 0, 0, RA_OWNER_PATH_TX2,
		0, RA_OWNER_DRV}
	},
	/* RA_BLOCK_TX2 */
	{
		.valid_owners = {RA_OWNER_NONE, 0, 0, 0, 0,
		RA_OWNER_PATH_LB, RA_OWNER_DRV}
	},
	/* RA_BLOCK_LB */
	{
		.valid_owners = {RA_OWNER_NONE, RA_OWNER_PATH_RX1,
		RA_OWNER_PATH_RX2, 0, 0, RA_OWNER_PATH_LB, RA_OWNER_DRV}
	},
	/* RA_BLOCK_SHARED_RX_LB */
	{
		.valid_owners = {RA_OWNER_NONE, 0, 0, RA_OWNER_PATH_TX1,
		RA_OWNER_PATH_TX2, 0, RA_OWNER_DRV}
	},
	/* RA_BLOCK_SHARED_TX */
	{
		.valid_owners = {RA_OWNER_NONE, 0, 0, RA_OWNER_PATH_TX1,
		RA_OWNER_PATH_TX2, 0, RA_OWNER_DRV}
	},
	/* RA_BLOCK_TXFE1 */
	{
		.valid_owners = {RA_OWNER_NONE, 0, 0, RA_OWNER_PATH_TX1,
		RA_OWNER_PATH_TX2, 0, RA_OWNER_DRV}
	},
	/* RA_BLOCK_TXFE2 */
	{
		.valid_owners = {RA_OWNER_NONE, RA_OWNER_PATH_RX1,
		RA_OWNER_PATH_RX2, 0, 0, RA_OWNER_PATH_LB, RA_OWNER_DRV}
	},
	/* RA_BLOCK_PA_COMMON */
	{
		.valid_owners = {RA_OWNER_NONE, RA_OWNER_PATH_RX1,
		RA_OWNER_PATH_RX2, 0, 0, RA_OWNER_PATH_LB, RA_OWNER_DRV}
	},
	/* RA_BLOCK_PA_EAR */
	{
		.valid_owners = {RA_OWNER_NONE, RA_OWNER_PATH_RX1,
		RA_OWNER_PATH_RX2, 0, 0, RA_OWNER_PATH_LB, RA_OWNER_DRV}
	},
	/* RA_BLOCK_PA_HPH */
	{
		.valid_owners = {RA_OWNER_NONE, RA_OWNER_PATH_RX1,
		RA_OWNER_PATH_RX2, 0, 0, RA_OWNER_PATH_LB, RA_OWNER_DRV}
	},
	/* RA_BLOCK_PA_LINE */
	{
		.valid_owners = {RA_OWNER_NONE, RA_OWNER_PATH_RX1,
		RA_OWNER_PATH_RX2, 0, 0, RA_OWNER_PATH_LB, RA_OWNER_DRV}
	},
	/* RA_BLOCK_PA_AUX */
	{
		.valid_owners = {RA_OWNER_NONE, 0, 0, RA_OWNER_PATH_TX1,
		RA_OWNER_PATH_TX2, 0, RA_OWNER_DRV}
	},
	/* RA_BLOCK_ADC */
	{
		.valid_owners = {RA_OWNER_NONE, 0, 0, RA_OWNER_PATH_TX1,
		RA_OWNER_PATH_TX2, 0, RA_OWNER_DRV}
	},
	/* RA_BLOCK_DMIC */
	{
		.valid_owners = {RA_OWNER_NONE, 0, 0, RA_OWNER_PATH_TX1,
		RA_OWNER_PATH_TX2, 0, RA_OWNER_DRV}
	},
	/* RA_BLOCK_TX_I2S */
	{
		.valid_owners = {RA_OWNER_NONE, 0, 0, 0, 0, 0, RA_OWNER_DRV}
	},
	/*RA_BLOCK_DRV */
	{
		.valid_owners = {RA_OWNER_NONE, 0, 0, 0, 0, 0, RA_OWNER_DRV}
	},
	/* RA_BLOCK_TEST */
	{
		.valid_owners = {RA_OWNER_NONE, 0, 0, 0, 0, 0, RA_OWNER_DRV}
	},
	/* RA_BLOCK_RESERVED */
};

struct adie_codec_state {
	struct adie_codec_path path[ADIE_CODEC_MAX];
	u32 ref_cnt;
	struct marimba *pdrv_ptr;
	struct marimba_codec_platform_data *codec_pdata;
	struct mutex lock;
};

static struct adie_codec_state adie_codec;

/* A cacheable register is one that if the register's current value is being
 * written to it again, then it is permissable to skip that register write
 * because it does not actually change the value of the hardware register.
 *
 * Some registers are uncacheable, meaning that even they are being written
 * again with their current value, the write has another purpose and must go
 * through.
 *
 * Knowing the codec's uncacheable registers allows the driver to avoid
 * unnecessary codec register writes while making sure important register writes
 * are not skipped.
 */

static bool timpani_register_is_cacheable(u8 reg)
{
	switch (reg) {
	case TIMPANI_A_PA_LINE_L_GAIN:
	case TIMPANI_A_PA_LINE_R_GAIN:
	case TIMPANI_A_PA_HPH_L_GAIN:
	case TIMPANI_A_PA_HPH_R_GAIN:
	case TIMPANI_A_CDC_GCTL1:
	case TIMPANI_A_CDC_ST_CTL:
	case TIMPANI_A_CDC_GCTL2:
	case TIMPANI_A_CDC_ARB_BYPASS_CTL:
	case TIMPANI_A_CDC_CH_CTL:
	case TIMPANI_A_CDC_ANC1_IIR_COEFF_PTR:
	case TIMPANI_A_CDC_ANC1_IIR_COEFF_MSB:
	case TIMPANI_A_CDC_ANC1_IIR_COEFF_LSB:
	case TIMPANI_A_CDC_ANC1_LPF_COEFF_PTR:
	case TIMPANI_A_CDC_ANC1_LPF_COEFF_MSB:
	case TIMPANI_A_CDC_ANC1_LPF_COEFF_LSB:
	case TIMPANI_A_CDC_ANC1_SCALE_PTR:
	case TIMPANI_A_CDC_ANC1_SCALE:
	case TIMPANI_A_CDC_ANC2_IIR_COEFF_PTR:
	case TIMPANI_A_CDC_ANC2_IIR_COEFF_MSB:
	case TIMPANI_A_CDC_ANC2_IIR_COEFF_LSB:
	case TIMPANI_A_CDC_ANC2_LPF_COEFF_PTR:
	case TIMPANI_A_CDC_ANC2_LPF_COEFF_MSB:
	case TIMPANI_A_CDC_ANC2_LPF_COEFF_LSB:
	case TIMPANI_A_CDC_ANC2_SCALE_PTR:
	case TIMPANI_A_CDC_ANC2_SCALE:
	case TIMPANI_A_CDC_ANC1_CTL1:
	case TIMPANI_A_CDC_ANC1_CTL2:
	case TIMPANI_A_CDC_ANC1_FF_FB_SHIFT:
	case TIMPANI_A_CDC_ANC2_CTL1:
	case TIMPANI_A_CDC_ANC2_CTL2:
	case TIMPANI_A_CDC_ANC2_FF_FB_SHIFT:
	case TIMPANI_A_AUXPGA_LR_GAIN:
	case TIMPANI_A_CDC_ANC_INPUT_MUX:
		return false;
	default:
		return true;
	}
}

static int adie_codec_write(u8 reg, u8 mask, u8 val)
{
	int rc = 0;
	u8 new_val;

	if (reg > MAX_SHADOW_RIGISTERS) {
		pr_debug("register number is out of bound for shadow"
					" registers reg = %d\n", reg);
		new_val = (val & mask);
		rc = marimba_write_bit_mask(adie_codec.pdrv_ptr, reg,  &new_val,
			1, 0xFF);
		if (IS_ERR_VALUE(rc)) {
			pr_err("%s: fail to write reg %x\n", __func__, reg);
			rc = -EIO;
			goto error;
		}
		return rc;
	}
	new_val = (val & mask) | (timpani_shadow[reg] & ~mask);
	if (!(timpani_register_is_cacheable(reg) &&
		(new_val == timpani_shadow[reg]))) {

		rc = marimba_write_bit_mask(adie_codec.pdrv_ptr, reg,  &new_val,
			1, 0xFF);
		if (IS_ERR_VALUE(rc)) {
			pr_err("%s: fail to write reg %x\n", __func__, reg);
			rc = -EIO;
			goto error;
		}
		timpani_shadow[reg] = new_val;
		pr_debug("%s: write reg %x val %x new value %x\n", __func__,
			reg, val, new_val);
	}

error:
	return rc;
}


static int reg_in_use(u8 reg_ref, u8 path_type)
{
	if ((reg_ref & ~path_type) == 0)
		return 0;
	else
		return 1;
}

static int adie_codec_refcnt_write(u8 reg, u8 mask, u8 val, enum refcnt cnt,
		u8 path_type)
{
	u8 i;
	int j;
	u8 fld_mask;
	u8 path_mask;
	u8 reg_mask = 0;
	int rc = 0;

	for (i = 0; i < ARRAY_SIZE(timpani_regset); i++) {
		if (timpani_regset[i].reg_addr == reg) {
			for (j = 0; j < TIMPANI_MAX_FIELDS; j++) {
				fld_mask = timpani_regset[i].fld_ref_cnt[j].mask
					& mask;
				path_mask = timpani_regset[i].fld_ref_cnt[j]
							.path_mask;
				if (fld_mask) {
					if (!reg_in_use(path_mask, path_type))
						reg_mask |= fld_mask;
					if (cnt == INC)
						timpani_regset[i].fld_ref_cnt[j]
							.path_mask |= path_type;
					else if (cnt == DEC)
						timpani_regset[i].fld_ref_cnt[j]
							.path_mask &=
								~path_type;
				}
			}

			if (reg_mask)
				rc = adie_codec_write(reg, reg_mask, val);
			reg_mask = 0;
			break;
		}
	}

	return rc;
}

static int adie_codec_read(u8 reg, u8 *val)
{
	return marimba_read(adie_codec.pdrv_ptr, reg, val, 1);
}

static int timpani_adie_codec_setpath(struct adie_codec_path *path_ptr,
					u32 freq_plan, u32 osr)
{
	int rc = 0;
	u32 i, freq_idx = 0, freq = 0;

	if (path_ptr == NULL)
		return -EINVAL;

	if (path_ptr->curr_stage != ADIE_CODEC_DIGITAL_OFF) {
		rc = -EBUSY;
		goto error;
	}

	for (i = 0; i < path_ptr->profile->setting_sz; i++) {
		if (path_ptr->profile->settings[i].osr == osr) {
			if (path_ptr->profile->settings[i].freq_plan >=
				freq_plan) {
				if (freq == 0) {
					freq = path_ptr->profile->settings[i].
								freq_plan;
					freq_idx = i;
				} else if (path_ptr->profile->settings[i].
					freq_plan < freq) {
					freq = path_ptr->profile->settings[i].
								freq_plan;
					freq_idx = i;
				}
			}
		}
	}

	if (freq_idx >= path_ptr->profile->setting_sz)
		rc = -ENODEV;
	else {
		path_ptr->hwsetting_idx = freq_idx;
		path_ptr->stage_idx = 0;
	}

error:
	return rc;
}

static u32 timpani_adie_codec_freq_supported(
				struct adie_codec_dev_profile *profile,
				u32 requested_freq)
{
	u32 i, rc = -EINVAL;

	for (i = 0; i < profile->setting_sz; i++) {
		if (profile->settings[i].freq_plan >= requested_freq) {
			rc = 0;
			break;
		}
	}
	return rc;
}
int timpani_adie_codec_enable_sidetone(struct adie_codec_path *rx_path_ptr,
	u32 enable)
{
	int rc = 0;

	pr_debug("%s()\n", __func__);

	mutex_lock(&adie_codec.lock);

	if (!rx_path_ptr || &adie_codec.path[ADIE_CODEC_RX] != rx_path_ptr) {
		pr_err("%s: invalid path pointer\n", __func__);
		rc = -EINVAL;
		goto error;
	} else if (rx_path_ptr->curr_stage !=
		ADIE_CODEC_DIGITAL_ANALOG_READY) {
		pr_err("%s: bad state\n", __func__);
		rc = -EPERM;
		goto error;
	}

	if (enable) {
		rc = adie_codec_write(TIMPANI_A_CDC_RX1_CTL,
			TIMPANI_RX1_ST_MASK, TIMPANI_RX1_ST_ENABLE);

		if (rx_path_ptr->reg_owner == RA_OWNER_PATH_RX1)
			adie_codec_write(TIMPANI_A_CDC_ST_MIXING,
				TIMPANI_CDC_ST_MIXING_TX1_MASK,
				TIMPANI_CDC_ST_MIXING_TX1_ENABLE);
		else if (rx_path_ptr->reg_owner == RA_OWNER_PATH_RX2)
			adie_codec_write(TIMPANI_A_CDC_ST_MIXING,
				TIMPANI_CDC_ST_MIXING_TX2_MASK,
				TIMPANI_CDC_ST_MIXING_TX2_ENABLE);
	 } else {
		rc = adie_codec_write(TIMPANI_A_CDC_RX1_CTL,
			TIMPANI_RX1_ST_MASK, 0);

		if (rx_path_ptr->reg_owner == RA_OWNER_PATH_RX1)
			adie_codec_write(TIMPANI_A_CDC_ST_MIXING,
				TIMPANI_CDC_ST_MIXING_TX1_MASK, 0);
		else if (rx_path_ptr->reg_owner == RA_OWNER_PATH_RX2)
			adie_codec_write(TIMPANI_A_CDC_ST_MIXING,
				TIMPANI_CDC_ST_MIXING_TX2_MASK, 0);
	 }

error:
	mutex_unlock(&adie_codec.lock);
	return rc;
}
static int timpani_adie_codec_enable_anc(struct adie_codec_path *rx_path_ptr,
	u32 enable, struct adie_codec_anc_data *calibration_writes)
{
	int index = 0;
	int rc = 0;
	u8 reg, mask, val;
	pr_debug("%s: enable = %d\n", __func__, enable);

	mutex_lock(&adie_codec.lock);

	if (!rx_path_ptr || &adie_codec.path[ADIE_CODEC_RX] != rx_path_ptr) {
		pr_err("%s: invalid path pointer\n", __func__);
		rc = -EINVAL;
		goto error;
	} else if (rx_path_ptr->curr_stage !=
		ADIE_CODEC_DIGITAL_ANALOG_READY) {
		pr_err("%s: bad state\n", __func__);
		rc = -EPERM;
		goto error;
	}
	if (enable) {
		if (!calibration_writes || !calibration_writes->writes) {
			pr_err("%s: No ANC calibration data\n", __func__);
			rc = -EPERM;
			goto error;
		}
		while (index < calibration_writes->size) {
			ADIE_CODEC_UNPACK_ENTRY(calibration_writes->
				writes[index], reg, mask, val);
			adie_codec_write(reg, mask, val);
			index++;
		}
	} else {
		adie_codec_write(TIMPANI_A_CDC_ANC1_CTL1,
		TIMPANI_CDC_ANC1_CTL1_ANC1_EN_M,
		TIMPANI_CDC_ANC1_CTL1_ANC1_EN_ANC_DIS <<
		TIMPANI_CDC_ANC1_CTL1_ANC1_EN_S);

		adie_codec_write(TIMPANI_A_CDC_ANC2_CTL1,
		TIMPANI_CDC_ANC2_CTL1_ANC2_EN_M,
		TIMPANI_CDC_ANC2_CTL1_ANC2_EN_ANC_DIS <<
		TIMPANI_CDC_ANC2_CTL1_ANC2_EN_S);
	}

error:
	mutex_unlock(&adie_codec.lock);
	return rc;
}

static void adie_codec_restore_regdefault(u8 path_mask, u32 blk)
{
	u32 ireg;
	u32 regset_sz =
	(sizeof(timpani_regset)/sizeof(struct timpani_regaccess));

	for (ireg = 0; ireg < regset_sz; ireg++) {
		if (timpani_regset[ireg].blk_mask[blk]) {
			/* only process register belong to the block */
			u8 reg = timpani_regset[ireg].reg_addr;
			u8 mask = timpani_regset[ireg].blk_mask[blk];
			u8 val = timpani_regset[ireg].reg_default;
			adie_codec_refcnt_write(reg, mask, val, IGNORE,
				path_mask);
		}
	}
}

static void adie_codec_reach_stage_action(struct adie_codec_path *path_ptr,
	u32 stage)
{
	u32 iblk, iowner; /* iterators */
	u8 path_mask;

	if (path_ptr == NULL)
		return;

	path_mask = TIMPANI_PATH_MASK(path_ptr->reg_owner);

	if (stage != ADIE_CODEC_DIGITAL_OFF)
		return;

	for (iblk = 0 ; iblk <= RA_BLOCK_RESERVED ; iblk++) {
		for (iowner = 0; iowner < RA_OWNER_NUM; iowner++) {
			if (timpani_blkcfg[iblk].valid_owners[iowner] ==
					path_ptr->reg_owner) {
				adie_codec_restore_regdefault(path_mask, iblk);
				break; /* This path owns this block */
			}
		}
	}
}

static int timpani_adie_codec_proceed_stage(struct adie_codec_path *path_ptr,
						u32 state)
{
	int rc = 0, loop_exit = 0;
	struct adie_codec_action_unit *curr_action;
	struct adie_codec_hwsetting_entry *setting;
	u8 reg, mask, val;
	u8 path_mask;

	if (path_ptr == NULL)
		return -EINVAL;

	path_mask = TIMPANI_PATH_MASK(path_ptr->reg_owner);

	mutex_lock(&adie_codec.lock);
	setting = &path_ptr->profile->settings[path_ptr->hwsetting_idx];
	while (!loop_exit) {

		curr_action = &setting->actions[path_ptr->stage_idx];

		switch (curr_action->type) {
		case ADIE_CODEC_ACTION_ENTRY:
			ADIE_CODEC_UNPACK_ENTRY(curr_action->action,
			reg, mask, val);
			if (state == ADIE_CODEC_DIGITAL_OFF)
				adie_codec_refcnt_write(reg, mask, val, DEC,
					path_mask);
			else
				adie_codec_refcnt_write(reg, mask, val, INC,
					path_mask);
			break;
		case ADIE_CODEC_ACTION_DELAY_WAIT:
			if (curr_action->action > MAX_MDELAY_US)
				msleep(curr_action->action/1000);
			else
				usleep_range(curr_action->action,
				curr_action->action);
			break;
		case ADIE_CODEC_ACTION_STAGE_REACHED:
			adie_codec_reach_stage_action(path_ptr,
				curr_action->action);
			if (curr_action->action == state) {
				path_ptr->curr_stage = state;
				loop_exit = 1;
			}
			break;
		default:
			BUG();
		}

		path_ptr->stage_idx++;
		if (path_ptr->stage_idx == setting->action_sz)
			path_ptr->stage_idx = 0;
	}
	mutex_unlock(&adie_codec.lock);

	return rc;
}

static void timpani_codec_bring_up(void)
{
	/* Codec power up sequence */
	adie_codec_write(0xFF, 0xFF, 0x08);
	adie_codec_write(0xFF, 0xFF, 0x0A);
	adie_codec_write(0xFF, 0xFF, 0x0E);
	adie_codec_write(0xFF, 0xFF, 0x07);
	adie_codec_write(0xFF, 0xFF, 0x17);
	adie_codec_write(TIMPANI_A_MREF, 0xFF, 0xF2);
	msleep(15);
	adie_codec_write(TIMPANI_A_MREF, 0xFF, 0x22);

	/* Bypass TX HPFs to prevent pops */
	adie_codec_write(TIMPANI_A_CDC_BYPASS_CTL2, TIMPANI_CDC_BYPASS_CTL2_M,
		TIMPANI_CDC_BYPASS_CTL2_POR);
	adie_codec_write(TIMPANI_A_CDC_BYPASS_CTL3, TIMPANI_CDC_BYPASS_CTL3_M,
		TIMPANI_CDC_BYPASS_CTL3_POR);
}

static void timpani_codec_bring_down(void)
{
	adie_codec_write(TIMPANI_A_MREF, 0xFF, TIMPANI_MREF_POR);
	adie_codec_write(0xFF, 0xFF, 0x07);
	adie_codec_write(0xFF, 0xFF, 0x06);
	adie_codec_write(0xFF, 0xFF, 0x0E);
	adie_codec_write(0xFF, 0xFF, 0x08);
}

static int timpani_adie_codec_open(struct adie_codec_dev_profile *profile,
	struct adie_codec_path **path_pptr)
{
	int rc = 0;

	mutex_lock(&adie_codec.lock);

	if (!profile || !path_pptr) {
		rc = -EINVAL;
		goto error;
	}

	if (adie_codec.path[profile->path_type].profile) {
		rc = -EBUSY;
		goto error;
	}

	if (!adie_codec.ref_cnt) {

		if (adie_codec.codec_pdata &&
				adie_codec.codec_pdata->marimba_codec_power) {

			rc = adie_codec.codec_pdata->marimba_codec_power(1);
			if (rc) {
				pr_err("%s: could not power up timpani "
						"codec\n", __func__);
				goto error;
			}
			timpani_codec_bring_up();
		} else {
			pr_err("%s: couldn't detect timpani codec\n", __func__);
			rc = -ENODEV;
			goto error;
		}

	}

	adie_codec.path[profile->path_type].profile = profile;
	*path_pptr = (void *) &adie_codec.path[profile->path_type];
	adie_codec.ref_cnt++;
	adie_codec.path[profile->path_type].hwsetting_idx = 0;
	adie_codec.path[profile->path_type].curr_stage = ADIE_CODEC_DIGITAL_OFF;
	adie_codec.path[profile->path_type].stage_idx = 0;


error:
	mutex_unlock(&adie_codec.lock);
	return rc;
}

static int timpani_adie_codec_close(struct adie_codec_path *path_ptr)
{
	int rc = 0;

	mutex_lock(&adie_codec.lock);

	if (!path_ptr) {
		rc = -EINVAL;
		goto error;
	}
	if (path_ptr->curr_stage != ADIE_CODEC_DIGITAL_OFF)
		adie_codec_proceed_stage(path_ptr, ADIE_CODEC_DIGITAL_OFF);

	BUG_ON(!adie_codec.ref_cnt);

	path_ptr->profile = NULL;
	adie_codec.ref_cnt--;

	if (!adie_codec.ref_cnt) {
		/* Timpani CDC power down sequence */
		timpani_codec_bring_down();

		if (adie_codec.codec_pdata &&
				adie_codec.codec_pdata->marimba_codec_power) {

			rc = adie_codec.codec_pdata->marimba_codec_power(0);
			if (rc) {
				pr_err("%s: could not power down timpani "
						"codec\n", __func__);
				goto error;
			}
		}
	}

error:
	mutex_unlock(&adie_codec.lock);
	return rc;
}

static int timpani_adie_codec_set_master_mode(struct adie_codec_path *path_ptr,
			u8 master)
{
	u8 val = master ? 1 : 0;

	if (!path_ptr)
		return -EINVAL;

	if (path_ptr->reg_owner == RA_OWNER_PATH_RX1)
		adie_codec_write(TIMPANI_A_CDC_RX1_CTL, 0x01, val);
	else if (path_ptr->reg_owner == RA_OWNER_PATH_TX1)
		adie_codec_write(TIMPANI_A_CDC_TX_I2S_CTL, 0x01, val);
	else
		return -EINVAL;

	return 0;
}

int timpani_adie_codec_set_device_analog_volume(
		struct adie_codec_path *path_ptr,
		u32 num_channels, u32 volume)
{
	u8 val;
	u8 curr_val;
	u8 i;

	adie_codec_read(TIMPANI_A_AUXPGA_LR_GAIN, &curr_val);

	/* Volume is expressed as a percentage. */
	/* The upper nibble is the left channel, lower right channel. */
	val = (u8)((volume * TIMPANI_CODEC_AUXPGA_GAIN_RANGE) / 100);
	val |= val << 4;

	if ((curr_val & 0x0F) < (val & 0x0F)) {
		for (i = curr_val; i < val; i += 0x11)
			adie_codec_write(TIMPANI_A_AUXPGA_LR_GAIN, 0xFF, i);
	} else if ((curr_val & 0x0F) > (val & 0x0F)) {
		for (i = curr_val; i > val; i -= 0x11)
			adie_codec_write(TIMPANI_A_AUXPGA_LR_GAIN, 0xFF, i);
	}

	return 0;
}

enum adie_vol_type {
	ADIE_CODEC_RX_DIG_VOL,
	ADIE_CODEC_TX_DIG_VOL,
	ADIE_CODEC_VOL_TYPE_MAX
};

#define CDC_RX1LG		0x84
#define CDC_RX1RG		0x85
#define CDC_TX1LG		0x86
#define CDC_TX1RG		0x87
#define	DIG_VOL_MASK		0xFF

#define CDC_GCTL1		0x8A
#define RX1_PGA_UPDATE_L	0x04
#define RX1_PGA_UPDATE_R	0x08
#define TX1_PGA_UPDATE_L	0x40
#define TX1_PGA_UPDATE_R	0x80
#define CDC_GCTL1_RX_MASK	0x0F
#define CDC_GCTL1_TX_MASK	0xF0

enum {
	TIMPANI_MIN_DIG_VOL	= -84,	/* in DB*/
	TIMPANI_MAX_DIG_VOL	=  16,	/* in DB*/
	TIMPANI_DIG_VOL_STEP	=  3	/* in DB*/
};

static int timpani_adie_codec_set_dig_vol(enum adie_vol_type vol_type,
	u32 num_chan, u32 vol_per)
{
	u8 reg_left, reg_right;
	u8 gain_reg_val, gain_reg_mask;
	s8 new_reg_val, cur_reg_val;
	s8 step_size;

	adie_codec_read(CDC_GCTL1, &gain_reg_val);

	if (vol_type == ADIE_CODEC_RX_DIG_VOL) {

		pr_debug("%s : RX DIG VOL. num_chan = %u\n", __func__,
				num_chan);
		reg_left =  CDC_RX1LG;
		reg_right = CDC_RX1RG;

		if (num_chan == 1)
			gain_reg_val |=  RX1_PGA_UPDATE_L;
		else
			gain_reg_val |= (RX1_PGA_UPDATE_L | RX1_PGA_UPDATE_R);

		gain_reg_mask = CDC_GCTL1_RX_MASK;
	} else {

		pr_debug("%s : TX DIG VOL. num_chan = %u\n", __func__,
				num_chan);
		reg_left = CDC_TX1LG;
		reg_right = CDC_TX1RG;

		if (num_chan == 1)
			gain_reg_val |=  TX1_PGA_UPDATE_L;
		else
			gain_reg_val |= (TX1_PGA_UPDATE_L | TX1_PGA_UPDATE_R);

		gain_reg_mask = CDC_GCTL1_TX_MASK;
	}

	adie_codec_read(reg_left, &cur_reg_val);

	pr_debug("%s: vol_per = %d cur_reg_val = %d 0x%x\n", __func__, vol_per,
			cur_reg_val, cur_reg_val);

	new_reg_val =  TIMPANI_MIN_DIG_VOL +
		(((TIMPANI_MAX_DIG_VOL - TIMPANI_MIN_DIG_VOL) * vol_per) / 100);

	pr_debug("new_reg_val = %d 0x%x\n", new_reg_val, new_reg_val);

	if (new_reg_val > cur_reg_val) {
		step_size = TIMPANI_DIG_VOL_STEP;
	} else if (new_reg_val < cur_reg_val) {
		step_size = -TIMPANI_DIG_VOL_STEP;
	} else {
		pr_debug("new_reg_val and cur_reg_val are same 0x%x\n",
				new_reg_val);
		return 0;
	}

	while (cur_reg_val != new_reg_val) {

		if (((new_reg_val > cur_reg_val) &&
			((new_reg_val - cur_reg_val) < TIMPANI_DIG_VOL_STEP)) ||
			((cur_reg_val > new_reg_val) &&
			((cur_reg_val - new_reg_val)
			 < TIMPANI_DIG_VOL_STEP))) {

			cur_reg_val = new_reg_val;

			pr_debug("diff less than step. write new_reg_val = %d"
				" 0x%x\n", new_reg_val, new_reg_val);

		 } else {
			cur_reg_val = cur_reg_val + step_size;

			pr_debug("cur_reg_val = %d 0x%x\n",
					cur_reg_val, cur_reg_val);
		 }

		adie_codec_write(reg_left, DIG_VOL_MASK, cur_reg_val);

		if (num_chan == 2)
			adie_codec_write(reg_right, DIG_VOL_MASK, cur_reg_val);

		adie_codec_write(CDC_GCTL1, gain_reg_mask, gain_reg_val);
	}
	return 0;
}

static int timpani_adie_codec_set_device_digital_volume(
		struct adie_codec_path *path_ptr,
		u32 num_channels, u32 vol_percentage /* in percentage */)
{
	enum adie_vol_type vol_type;

	if (!path_ptr  || (path_ptr->curr_stage !=
				ADIE_CODEC_DIGITAL_ANALOG_READY)) {
		pr_info("%s: timpani codec not ready for volume control\n",
		       __func__);
		return  -EPERM;
	}

	if (num_channels > 2) {
		pr_err("%s: timpani odec only supports max two channels\n",
		       __func__);
		return -EINVAL;
	}

	if (path_ptr->profile->path_type == ADIE_CODEC_RX) {
		vol_type = ADIE_CODEC_RX_DIG_VOL;
	} else if (path_ptr->profile->path_type == ADIE_CODEC_TX) {
		vol_type = ADIE_CODEC_TX_DIG_VOL;
	} else {
		pr_err("%s: invalid device data neither RX nor TX\n",
				__func__);
		return -EINVAL;
	}

	timpani_adie_codec_set_dig_vol(vol_type, num_channels, vol_percentage);

	return 0;
}

static const struct adie_codec_operations timpani_adie_ops = {
	.codec_id = TIMPANI_ID,
	.codec_open = timpani_adie_codec_open,
	.codec_close = timpani_adie_codec_close,
	.codec_setpath = timpani_adie_codec_setpath,
	.codec_proceed_stage = timpani_adie_codec_proceed_stage,
	.codec_freq_supported = timpani_adie_codec_freq_supported,
	.codec_enable_sidetone = timpani_adie_codec_enable_sidetone,
	.codec_set_master_mode = timpani_adie_codec_set_master_mode,
	.codec_enable_anc = timpani_adie_codec_enable_anc,
	.codec_set_device_analog_volume =
		timpani_adie_codec_set_device_analog_volume,
	.codec_set_device_digital_volume =
		timpani_adie_codec_set_device_digital_volume,
};

static void timpani_codec_populate_shadow_registers(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(timpani_regset); i++) {
		if (timpani_regset[i].reg_addr < TIMPANI_ARRAY_SIZE) {
			timpani_shadow[timpani_regset[i].reg_addr] =
				timpani_regset[i].reg_default;
		}
	}
}

#ifdef CONFIG_DEBUG_FS
static struct dentry *debugfs_timpani_dent;
static struct dentry *debugfs_peek;
static struct dentry *debugfs_poke;
static struct dentry *debugfs_power;
static struct dentry *debugfs_dump;

static unsigned char read_data;

static int codec_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static int get_parameters(char *buf, long int *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");

	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token != NULL) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (strict_strtoul(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
			}
		else
			return -EINVAL;
	}
	return 0;
}

static ssize_t codec_debug_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char lbuf[8];

	snprintf(lbuf, sizeof(lbuf), "0x%x\n", read_data);
	return simple_read_from_buffer(ubuf, count, ppos, lbuf, strlen(lbuf));
}

static ssize_t codec_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char *access_str = filp->private_data;
	char lbuf[32];
	int rc;
	int i;
	int read_result;
	u8 reg_val;
	long int param[5];

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';

	if (!strcmp(access_str, "power")) {
		if (get_parameters(lbuf, param, 1) == 0) {
			switch (param[0]) {
			case 1:
				adie_codec.codec_pdata->marimba_codec_power(1);
				timpani_codec_bring_up();
				break;
			case 0:
				timpani_codec_bring_down();
				adie_codec.codec_pdata->marimba_codec_power(0);
				break;
			default:
				rc = -EINVAL;
				break;
			}
		} else
			rc = -EINVAL;
	} else if (!strcmp(access_str, "poke")) {
		/* write */
		rc = get_parameters(lbuf, param, 2);
		if ((param[0] <= 0xFF) && (param[1] <= 0xFF) &&
			(rc == 0))
			adie_codec_write(param[0], 0xFF, param[1]);
		else
			rc = -EINVAL;
	} else if (!strcmp(access_str, "peek")) {
		/* read */
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] <= 0xFF) && (rc == 0))
			adie_codec_read(param[0], &read_data);
		else
			rc = -EINVAL;
	} else if (!strcmp(access_str, "dump")) {
		pr_info("************** timpani regs *************\n");
		for (i = 0; i < 0xFF; i++) {
			read_result = adie_codec_read(i, &reg_val);
			if (read_result < 0) {
				pr_info("failed to read codec register\n");
				break;
			} else
				pr_info("reg 0x%02X val 0x%02X\n", i, reg_val);
		}
		pr_info("*****************************************\n");
	}

	if (rc == 0)
		rc = cnt;
	else
		pr_err("%s: rc = %d\n", __func__, rc);

	return rc;
}

static const struct file_operations codec_debug_ops = {
	.open = codec_debug_open,
	.write = codec_debug_write,
	.read = codec_debug_read
};
#endif

static int timpani_codec_probe(struct platform_device *pdev)
{
	int rc;

	adie_codec.pdrv_ptr = platform_get_drvdata(pdev);
	adie_codec.codec_pdata = pdev->dev.platform_data;

	if (adie_codec.codec_pdata->snddev_profile_init)
		adie_codec.codec_pdata->snddev_profile_init();

	timpani_codec_populate_shadow_registers();

	/* Register the timpani ADIE operations */
	rc = adie_codec_register_codec_operations(&timpani_adie_ops);

#ifdef CONFIG_DEBUG_FS
	debugfs_timpani_dent = debugfs_create_dir("msm_adie_codec", 0);
	if (!IS_ERR(debugfs_timpani_dent)) {
		debugfs_peek = debugfs_create_file("peek",
		S_IFREG | S_IRUGO, debugfs_timpani_dent,
		(void *) "peek", &codec_debug_ops);

		debugfs_poke = debugfs_create_file("poke",
		S_IFREG | S_IRUGO, debugfs_timpani_dent,
		(void *) "poke", &codec_debug_ops);

		debugfs_power = debugfs_create_file("power",
		S_IFREG | S_IRUGO, debugfs_timpani_dent,
		(void *) "power", &codec_debug_ops);

		debugfs_dump = debugfs_create_file("dump",
		S_IFREG | S_IRUGO, debugfs_timpani_dent,
		(void *) "dump", &codec_debug_ops);

	}
#endif

	return rc;
}

static struct platform_driver timpani_codec_driver = {
	.probe = timpani_codec_probe,
	.driver = {
		.name = "timpani_codec",
		.owner = THIS_MODULE,
	},
};

static int __init timpani_codec_init(void)
{
	s32 rc;

	rc = platform_driver_register(&timpani_codec_driver);
	if (IS_ERR_VALUE(rc))
		goto error;

	adie_codec.path[ADIE_CODEC_TX].reg_owner = RA_OWNER_PATH_TX1;
	adie_codec.path[ADIE_CODEC_RX].reg_owner = RA_OWNER_PATH_RX1;
	adie_codec.path[ADIE_CODEC_LB].reg_owner = RA_OWNER_PATH_LB;
	mutex_init(&adie_codec.lock);
error:
	return rc;
}

static void __exit timpani_codec_exit(void)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove(debugfs_peek);
	debugfs_remove(debugfs_poke);
	debugfs_remove(debugfs_power);
	debugfs_remove(debugfs_dump);
	debugfs_remove(debugfs_timpani_dent);
#endif
	platform_driver_unregister(&timpani_codec_driver);
}

module_init(timpani_codec_init);
module_exit(timpani_codec_exit);

MODULE_DESCRIPTION("Timpani codec driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
