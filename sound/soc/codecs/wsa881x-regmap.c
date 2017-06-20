/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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
#include <linux/device.h>
#include "wsa881x-registers.h"
#include "wsa881x.h"

/*
 * Default register reset values that are common across different versions
 * are defined here. If a register reset value is changed based on version
 * then remove it from this structure and add it in version specific
 * structures.
 */
static struct reg_default wsa881x_defaults[] = {
	{WSA881X_CHIP_ID0, 0x00},
	{WSA881X_CHIP_ID1, 0x00},
	{WSA881X_CHIP_ID2, 0x00},
	{WSA881X_CHIP_ID3, 0x02},
	{WSA881X_BUS_ID, 0x00},
	{WSA881X_CDC_RST_CTL, 0x00},
	{WSA881X_CDC_TOP_CLK_CTL, 0x03},
	{WSA881X_CDC_ANA_CLK_CTL, 0x00},
	{WSA881X_CDC_DIG_CLK_CTL, 0x00},
	{WSA881X_CLOCK_CONFIG, 0x00},
	{WSA881X_ANA_CTL, 0x08},
	{WSA881X_SWR_RESET_EN, 0x00},
	{WSA881X_TEMP_DETECT_CTL, 0x01},
	{WSA881X_TEMP_MSB, 0x00},
	{WSA881X_TEMP_LSB, 0x00},
	{WSA881X_TEMP_CONFIG0, 0x00},
	{WSA881X_TEMP_CONFIG1, 0x00},
	{WSA881X_CDC_CLIP_CTL, 0x03},
	{WSA881X_SDM_PDM9_LSB, 0x00},
	{WSA881X_SDM_PDM9_MSB, 0x00},
	{WSA881X_CDC_RX_CTL, 0x7E},
	{WSA881X_DEM_BYPASS_DATA0, 0x00},
	{WSA881X_DEM_BYPASS_DATA1, 0x00},
	{WSA881X_DEM_BYPASS_DATA2, 0x00},
	{WSA881X_DEM_BYPASS_DATA3, 0x00},
	{WSA881X_OTP_CTRL0, 0x00},
	{WSA881X_OTP_CTRL1, 0x00},
	{WSA881X_HDRIVE_CTL_GROUP1, 0x00},
	{WSA881X_INTR_MODE, 0x00},
	{WSA881X_INTR_STATUS, 0x00},
	{WSA881X_INTR_CLEAR, 0x00},
	{WSA881X_INTR_LEVEL, 0x00},
	{WSA881X_INTR_SET, 0x00},
	{WSA881X_INTR_TEST, 0x00},
	{WSA881X_PDM_TEST_MODE, 0x00},
	{WSA881X_ATE_TEST_MODE, 0x00},
	{WSA881X_PIN_CTL_MODE, 0x00},
	{WSA881X_PIN_CTL_OE, 0x00},
	{WSA881X_PIN_WDATA_IOPAD, 0x00},
	{WSA881X_PIN_STATUS, 0x00},
	{WSA881X_DIG_DEBUG_MODE, 0x00},
	{WSA881X_DIG_DEBUG_SEL, 0x00},
	{WSA881X_DIG_DEBUG_EN, 0x00},
	{WSA881X_SWR_HM_TEST1, 0x08},
	{WSA881X_SWR_HM_TEST2, 0x00},
	{WSA881X_TEMP_DETECT_DBG_CTL, 0x00},
	{WSA881X_TEMP_DEBUG_MSB, 0x00},
	{WSA881X_TEMP_DEBUG_LSB, 0x00},
	{WSA881X_SAMPLE_EDGE_SEL, 0x0C},
	{WSA881X_SPARE_0, 0x00},
	{WSA881X_SPARE_1, 0x00},
	{WSA881X_SPARE_2, 0x00},
	{WSA881X_OTP_REG_0, 0x01},
	{WSA881X_OTP_REG_1, 0xFF},
	{WSA881X_OTP_REG_2, 0xC0},
	{WSA881X_OTP_REG_3, 0xFF},
	{WSA881X_OTP_REG_4, 0xC0},
	{WSA881X_OTP_REG_5, 0xFF},
	{WSA881X_OTP_REG_6, 0xFF},
	{WSA881X_OTP_REG_7, 0xFF},
	{WSA881X_OTP_REG_8, 0xFF},
	{WSA881X_OTP_REG_9, 0xFF},
	{WSA881X_OTP_REG_10, 0xFF},
	{WSA881X_OTP_REG_11, 0xFF},
	{WSA881X_OTP_REG_12, 0xFF},
	{WSA881X_OTP_REG_13, 0xFF},
	{WSA881X_OTP_REG_14, 0xFF},
	{WSA881X_OTP_REG_15, 0xFF},
	{WSA881X_OTP_REG_16, 0xFF},
	{WSA881X_OTP_REG_17, 0xFF},
	{WSA881X_OTP_REG_18, 0xFF},
	{WSA881X_OTP_REG_19, 0xFF},
	{WSA881X_OTP_REG_20, 0xFF},
	{WSA881X_OTP_REG_21, 0xFF},
	{WSA881X_OTP_REG_22, 0xFF},
	{WSA881X_OTP_REG_23, 0xFF},
	{WSA881X_OTP_REG_24, 0x03},
	{WSA881X_OTP_REG_25, 0x01},
	{WSA881X_OTP_REG_26, 0x03},
	{WSA881X_OTP_REG_27, 0x11},
	{WSA881X_OTP_REG_63, 0x40},
	/* WSA881x Analog registers */
	{WSA881X_BIAS_REF_CTRL, 0x6C},
	{WSA881X_BIAS_TEST, 0x16},
	{WSA881X_BIAS_BIAS, 0xF0},
	{WSA881X_TEMP_OP, 0x00},
	{WSA881X_TEMP_IREF_CTRL, 0x56},
	{WSA881X_TEMP_ISENS_CTRL, 0x47},
	{WSA881X_TEMP_CLK_CTRL, 0x87},
	{WSA881X_TEMP_TEST, 0x00},
	{WSA881X_TEMP_BIAS, 0x51},
	{WSA881X_TEMP_DOUT_MSB, 0x00},
	{WSA881X_TEMP_DOUT_LSB, 0x00},
	{WSA881X_ADC_EN_MODU_V, 0x00},
	{WSA881X_ADC_EN_MODU_I, 0x00},
	{WSA881X_ADC_EN_DET_TEST_V, 0x00},
	{WSA881X_ADC_EN_DET_TEST_I, 0x00},
	{WSA881X_ADC_EN_SEL_IBAIS, 0x10},
	{WSA881X_SPKR_DRV_EN, 0x74},
	{WSA881X_SPKR_DRV_DBG, 0x15},
	{WSA881X_SPKR_PWRSTG_DBG, 0x00},
	{WSA881X_SPKR_OCP_CTL, 0xD4},
	{WSA881X_SPKR_CLIP_CTL, 0x90},
	{WSA881X_SPKR_PA_INT, 0x54},
	{WSA881X_SPKR_BIAS_CAL, 0xAC},
	{WSA881X_SPKR_STATUS1, 0x00},
	{WSA881X_SPKR_STATUS2, 0x00},
	{WSA881X_BOOST_EN_CTL, 0x18},
	{WSA881X_BOOST_CURRENT_LIMIT, 0x7A},
	{WSA881X_BOOST_PRESET_OUT2, 0x70},
	{WSA881X_BOOST_FORCE_OUT, 0x0E},
	{WSA881X_BOOST_LDO_PROG, 0x16},
	{WSA881X_BOOST_SLOPE_COMP_ISENSE_FB, 0x71},
	{WSA881X_BOOST_RON_CTL, 0x0F},
	{WSA881X_BOOST_ZX_CTL, 0x34},
	{WSA881X_BOOST_START_CTL, 0x23},
	{WSA881X_BOOST_MISC1_CTL, 0x80},
	{WSA881X_BOOST_MISC2_CTL, 0x00},
	{WSA881X_BOOST_MISC3_CTL, 0x00},
	{WSA881X_BOOST_ATEST_CTL, 0x00},
	{WSA881X_SPKR_PROT_FE_GAIN, 0x46},
	{WSA881X_SPKR_PROT_FE_CM_LDO_SET, 0x3B},
	{WSA881X_SPKR_PROT_FE_ISENSE_BIAS_SET1, 0x8D},
	{WSA881X_SPKR_PROT_FE_ISENSE_BIAS_SET2, 0x8D},
	{WSA881X_SPKR_PROT_ATEST1, 0x01},
	{WSA881X_SPKR_PROT_FE_VSENSE_VCM, 0x8D},
	{WSA881X_SPKR_PROT_FE_VSENSE_BIAS_SET1, 0x4D},
	{WSA881X_SPKR_PROT_SAR, 0x00},
	{WSA881X_SPKR_STATUS3, 0x00},
};

/* Default register reset values for WSA881x rev 2.0 */
static struct reg_default wsa881x_rev_2_0[] = {
	{WSA881X_RESET_CTL, 0x00},
	{WSA881X_TADC_VALUE_CTL, 0x01},
	{WSA881X_INTR_MASK, 0x1B},
	{WSA881X_IOPAD_CTL, 0x00},
	{WSA881X_OTP_REG_28, 0x3F},
	{WSA881X_OTP_REG_29, 0x3F},
	{WSA881X_OTP_REG_30, 0x01},
	{WSA881X_OTP_REG_31, 0x01},
	{WSA881X_TEMP_ADC_CTRL, 0x03},
	{WSA881X_ADC_SEL_IBIAS, 0x45},
	{WSA881X_SPKR_DRV_GAIN, 0xC1},
	{WSA881X_SPKR_DAC_CTL, 0x42},
	{WSA881X_SPKR_BBM_CTL, 0x02},
	{WSA881X_SPKR_MISC_CTL1, 0x40},
	{WSA881X_SPKR_MISC_CTL2, 0x07},
	{WSA881X_SPKR_BIAS_INT, 0x5F},
	{WSA881X_SPKR_BIAS_PSRR, 0x44},
	{WSA881X_BOOST_PS_CTL, 0xA0},
	{WSA881X_BOOST_PRESET_OUT1, 0xB7},
	{WSA881X_BOOST_LOOP_STABILITY, 0x8D},
	{WSA881X_SPKR_PROT_ATEST2, 0x02},
	{WSA881X_BONGO_RESRV_REG1, 0x5E},
	{WSA881X_BONGO_RESRV_REG2, 0x07},
};

/*
 * wsa881x_regmap_defaults - update regmap default register values
 * @regmap: pointer to regmap structure
 * @version: wsa881x version id
 *
 * Update regmap default register values based on version id
 *
 */
void wsa881x_regmap_defaults(struct regmap *regmap, u8 version)
{
	u16 ret = 0;

	if (!regmap) {
		pr_debug("%s: regmap structure is NULL\n", __func__);
		return;
	}

	regcache_cache_only(regmap, true);
	ret = regmap_multi_reg_write(regmap, wsa881x_rev_2_0,
				     ARRAY_SIZE(wsa881x_rev_2_0));
	regcache_cache_only(regmap, false);

	if (ret)
		pr_debug("%s: Failed to update regmap defaults ret= %d\n",
			 __func__, ret);
}
EXPORT_SYMBOL(wsa881x_regmap_defaults);

static bool wsa881x_readable_register(struct device *dev, unsigned int reg)
{
	return wsa881x_reg_readable[reg];
}

static bool wsa881x_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case WSA881X_CHIP_ID0:
	case WSA881X_CHIP_ID1:
	case WSA881X_CHIP_ID2:
	case WSA881X_CHIP_ID3:
	case WSA881X_BUS_ID:
	case WSA881X_TEMP_MSB:
	case WSA881X_TEMP_LSB:
	case WSA881X_SDM_PDM9_LSB:
	case WSA881X_SDM_PDM9_MSB:
	case WSA881X_OTP_CTRL1:
	case WSA881X_INTR_STATUS:
	case WSA881X_ATE_TEST_MODE:
	case WSA881X_PIN_STATUS:
	case WSA881X_SWR_HM_TEST2:
	case WSA881X_SPKR_STATUS1:
	case WSA881X_SPKR_STATUS2:
	case WSA881X_SPKR_STATUS3:
	case WSA881X_OTP_REG_0:
	case WSA881X_OTP_REG_1:
	case WSA881X_OTP_REG_2:
	case WSA881X_OTP_REG_3:
	case WSA881X_OTP_REG_4:
	case WSA881X_OTP_REG_5:
	case WSA881X_OTP_REG_31:
	case WSA881X_TEMP_DOUT_MSB:
	case WSA881X_TEMP_DOUT_LSB:
	case WSA881X_TEMP_OP:
	case WSA881X_SPKR_PROT_SAR:
		return true;
	default:
		return false;
	}
}

struct regmap_config wsa881x_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = wsa881x_defaults,
	.num_reg_defaults = ARRAY_SIZE(wsa881x_defaults),
	.max_register = WSA881X_MAX_REGISTER,
	.volatile_reg = wsa881x_volatile_register,
	.readable_reg = wsa881x_readable_register,
	.reg_format_endian = REGMAP_ENDIAN_NATIVE,
	.val_format_endian = REGMAP_ENDIAN_NATIVE,
	.can_multi_write = true,
};
