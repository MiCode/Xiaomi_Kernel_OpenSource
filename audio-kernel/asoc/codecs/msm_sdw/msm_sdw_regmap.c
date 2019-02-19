/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/regmap.h>
#include "msm_sdw.h"

static const struct reg_default msm_sdw_defaults[] = {
	/* Page #10 registers */
	{ MSM_SDW_PAGE_REGISTER, 0x00 },
	{ MSM_SDW_TX9_SPKR_PROT_PATH_CTL, 0x02 },
	{ MSM_SDW_TX9_SPKR_PROT_PATH_CFG0, 0x00 },
	{ MSM_SDW_TX10_SPKR_PROT_PATH_CTL, 0x02 },
	{ MSM_SDW_TX10_SPKR_PROT_PATH_CFG0, 0x00 },
	{ MSM_SDW_TX11_SPKR_PROT_PATH_CTL, 0x02 },
	{ MSM_SDW_TX11_SPKR_PROT_PATH_CFG0, 0x00 },
	{ MSM_SDW_TX12_SPKR_PROT_PATH_CTL, 0x02 },
	{ MSM_SDW_TX12_SPKR_PROT_PATH_CFG0, 0x00 },
	/* Page #11 registers */
	{ MSM_SDW_COMPANDER7_CTL0, 0x60 },
	{ MSM_SDW_COMPANDER7_CTL1, 0xdb },
	{ MSM_SDW_COMPANDER7_CTL2, 0xff },
	{ MSM_SDW_COMPANDER7_CTL3, 0x35 },
	{ MSM_SDW_COMPANDER7_CTL4, 0xff },
	{ MSM_SDW_COMPANDER7_CTL5, 0x00 },
	{ MSM_SDW_COMPANDER7_CTL6, 0x01 },
	{ MSM_SDW_COMPANDER8_CTL0, 0x60 },
	{ MSM_SDW_COMPANDER8_CTL1, 0xdb },
	{ MSM_SDW_COMPANDER8_CTL2, 0xff },
	{ MSM_SDW_COMPANDER8_CTL3, 0x35 },
	{ MSM_SDW_COMPANDER8_CTL4, 0xff },
	{ MSM_SDW_COMPANDER8_CTL5, 0x00 },
	{ MSM_SDW_COMPANDER8_CTL6, 0x01 },
	{ MSM_SDW_RX7_RX_PATH_CTL, 0x04 },
	{ MSM_SDW_RX7_RX_PATH_CFG0, 0x00 },
	{ MSM_SDW_RX7_RX_PATH_CFG2, 0x8f },
	{ MSM_SDW_RX7_RX_VOL_CTL, 0x00 },
	{ MSM_SDW_RX7_RX_PATH_MIX_CTL, 0x04 },
	{ MSM_SDW_RX7_RX_VOL_MIX_CTL, 0x00 },
	{ MSM_SDW_RX7_RX_PATH_SEC2, 0x00 },
	{ MSM_SDW_RX7_RX_PATH_SEC3, 0x00 },
	{ MSM_SDW_RX7_RX_PATH_SEC5, 0x00 },
	{ MSM_SDW_RX7_RX_PATH_SEC6, 0x00 },
	{ MSM_SDW_RX7_RX_PATH_SEC7, 0x00 },
	{ MSM_SDW_RX7_RX_PATH_MIX_SEC1, 0x00 },
	{ MSM_SDW_RX8_RX_PATH_CTL, 0x04 },
	{ MSM_SDW_RX8_RX_PATH_CFG0, 0x00 },
	{ MSM_SDW_RX8_RX_PATH_CFG2, 0x8f },
	{ MSM_SDW_RX8_RX_VOL_CTL, 0x00 },
	{ MSM_SDW_RX8_RX_PATH_MIX_CTL, 0x04 },
	{ MSM_SDW_RX8_RX_VOL_MIX_CTL, 0x00 },
	{ MSM_SDW_RX8_RX_PATH_SEC2, 0x00 },
	{ MSM_SDW_RX8_RX_PATH_SEC3, 0x00 },
	{ MSM_SDW_RX8_RX_PATH_SEC5, 0x00 },
	{ MSM_SDW_RX8_RX_PATH_SEC6, 0x00 },
	{ MSM_SDW_RX8_RX_PATH_SEC7, 0x00 },
	{ MSM_SDW_RX8_RX_PATH_MIX_SEC1, 0x00 },
	/* Page #12 registers */
	{ MSM_SDW_BOOST0_BOOST_PATH_CTL, 0x00 },
	{ MSM_SDW_BOOST0_BOOST_CTL, 0xb2 },
	{ MSM_SDW_BOOST0_BOOST_CFG1, 0x00 },
	{ MSM_SDW_BOOST0_BOOST_CFG2, 0x00 },
	{ MSM_SDW_BOOST1_BOOST_PATH_CTL, 0x00 },
	{ MSM_SDW_BOOST1_BOOST_CTL, 0xb2 },
	{ MSM_SDW_BOOST1_BOOST_CFG1, 0x00 },
	{ MSM_SDW_BOOST1_BOOST_CFG2, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_WR_DATA_0, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_WR_DATA_1, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_WR_DATA_2, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_WR_DATA_3, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_WR_ADDR_0, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_WR_ADDR_1, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_WR_ADDR_2, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_WR_ADDR_3, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_RD_ADDR_0, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_RD_ADDR_1, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_RD_ADDR_2, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_RD_ADDR_3, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_RD_DATA_0, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_RD_DATA_1, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_RD_DATA_2, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_RD_DATA_3, 0x00 },
	{ MSM_SDW_AHB_BRIDGE_ACCESS_CFG, 0x0f },
	{ MSM_SDW_AHB_BRIDGE_ACCESS_STATUS, 0x03 },
	/* Page #13 registers */
	{ MSM_SDW_CLK_RST_CTRL_MCLK_CONTROL, 0x00 },
	{ MSM_SDW_CLK_RST_CTRL_FS_CNT_CONTROL, 0x00 },
	{ MSM_SDW_CLK_RST_CTRL_SWR_CONTROL, 0x00 },
	{ MSM_SDW_TOP_TOP_CFG0, 0x00 },
	{ MSM_SDW_TOP_TOP_CFG1, 0x00 },
	{ MSM_SDW_TOP_RX_I2S_CTL, 0x0C },
	{ MSM_SDW_TOP_TX_I2S_CTL, 0x00 },
	{ MSM_SDW_TOP_I2S_CLK, 0x00 },
	{ MSM_SDW_TOP_RX7_PATH_INPUT0_MUX, 0x00 },
	{ MSM_SDW_TOP_RX7_PATH_INPUT1_MUX, 0x00 },
	{ MSM_SDW_TOP_RX8_PATH_INPUT0_MUX, 0x00 },
	{ MSM_SDW_TOP_RX8_PATH_INPUT1_MUX, 0x00 },
	{ MSM_SDW_TOP_FREQ_MCLK, 0x00 },
	{ MSM_SDW_TOP_DEBUG_BUS_SEL, 0x00 },
	{ MSM_SDW_TOP_DEBUG_EN, 0x00 },
	{ MSM_SDW_TOP_I2S_RESET, 0x00 },
	{ MSM_SDW_TOP_BLOCKS_RESET, 0x00 },
};

static bool msm_sdw_is_readable_register(struct device *dev, unsigned int reg)
{
	return msm_sdw_reg_readable[reg];
}

static bool msm_sdw_is_writeable_register(struct device *dev, unsigned int reg)
{
	return msm_sdw_reg_writeable[reg];
}

static bool msm_sdw_is_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case MSM_SDW_AHB_BRIDGE_WR_DATA_0:
	case MSM_SDW_AHB_BRIDGE_WR_DATA_1:
	case MSM_SDW_AHB_BRIDGE_WR_DATA_2:
	case MSM_SDW_AHB_BRIDGE_WR_DATA_3:
	case MSM_SDW_AHB_BRIDGE_WR_ADDR_0:
	case MSM_SDW_AHB_BRIDGE_WR_ADDR_1:
	case MSM_SDW_AHB_BRIDGE_WR_ADDR_2:
	case MSM_SDW_AHB_BRIDGE_WR_ADDR_3:
	case MSM_SDW_AHB_BRIDGE_RD_DATA_0:
	case MSM_SDW_AHB_BRIDGE_RD_DATA_1:
	case MSM_SDW_AHB_BRIDGE_RD_DATA_2:
	case MSM_SDW_AHB_BRIDGE_RD_DATA_3:
	case MSM_SDW_AHB_BRIDGE_RD_ADDR_0:
	case MSM_SDW_AHB_BRIDGE_RD_ADDR_1:
	case MSM_SDW_AHB_BRIDGE_RD_ADDR_2:
	case MSM_SDW_AHB_BRIDGE_RD_ADDR_3:
	case MSM_SDW_CLK_RST_CTRL_MCLK_CONTROL:
	case MSM_SDW_CLK_RST_CTRL_FS_CNT_CONTROL:
		return true;
	default:
		return false;
	}
}

const struct regmap_config msm_sdw_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.reg_stride = 4,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = msm_sdw_defaults,
	.num_reg_defaults = ARRAY_SIZE(msm_sdw_defaults),
	.max_register = MSM_SDW_MAX_REGISTER,
	.writeable_reg = msm_sdw_is_writeable_register,
	.volatile_reg = msm_sdw_is_volatile_register,
	.readable_reg = msm_sdw_is_readable_register,
};
