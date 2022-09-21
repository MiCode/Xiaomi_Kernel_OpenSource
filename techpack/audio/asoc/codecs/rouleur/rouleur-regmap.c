// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/regmap.h>
#include <linux/device.h>
#include "rouleur-registers.h"

extern const u8 rouleur_reg_access_analog[
			ROULEUR_REG(ROULEUR_ANALOG_REGISTERS_MAX_SIZE)];
extern const u8 rouleur_reg_access_digital[
			ROULEUR_REG(ROULEUR_DIGITAL_REGISTERS_MAX_SIZE)];

static const struct reg_default rouleur_defaults[] = {
	{ ROULEUR_ANA_MICBIAS_MICB_1_2_EN,        0x01 },
	{ ROULEUR_ANA_MICBIAS_MICB_3_EN,          0x00 },
	{ ROULEUR_ANA_MICBIAS_LDO_1_SETTING,      0x21 },
	{ ROULEUR_ANA_MICBIAS_LDO_1_CTRL,         0x01 },
	{ ROULEUR_ANA_TX_AMIC1,                   0x00 },
	{ ROULEUR_ANA_TX_AMIC2,                   0x00 },
	{ ROULEUR_ANA_MBHC_MECH,                  0x39 },
	{ ROULEUR_ANA_MBHC_ELECT,                 0x08 },
	{ ROULEUR_ANA_MBHC_ZDET,                  0x10 },
	{ ROULEUR_ANA_MBHC_RESULT_1,              0x00 },
	{ ROULEUR_ANA_MBHC_RESULT_2,              0x00 },
	{ ROULEUR_ANA_MBHC_RESULT_3,              0x00 },
	{ ROULEUR_ANA_MBHC_BTN0_ZDET_VREF1,       0x00 },
	{ ROULEUR_ANA_MBHC_BTN1_ZDET_VREF2,       0x10 },
	{ ROULEUR_ANA_MBHC_BTN2_ZDET_VREF3,       0x20 },
	{ ROULEUR_ANA_MBHC_BTN3_ZDET_DBG_400,     0x30 },
	{ ROULEUR_ANA_MBHC_BTN4_ZDET_DBG_1400,    0x40 },
	{ ROULEUR_ANA_MBHC_MICB2_RAMP,            0x00 },
	{ ROULEUR_ANA_MBHC_CTL_1,                 0x02 },
	{ ROULEUR_ANA_MBHC_CTL_2,                 0x05 },
	{ ROULEUR_ANA_MBHC_PLUG_DETECT_CTL,       0xE9 },
	{ ROULEUR_ANA_MBHC_ZDET_ANA_CTL,          0x0F },
	{ ROULEUR_ANA_MBHC_ZDET_RAMP_CTL,         0x00 },
	{ ROULEUR_ANA_MBHC_FSM_STATUS,            0x00 },
	{ ROULEUR_ANA_MBHC_ADC_RESULT,            0x00 },
	{ ROULEUR_ANA_MBHC_CTL_CLK,               0x30 },
	{ ROULEUR_ANA_MBHC_ZDET_CALIB_RESULT,     0x00 },
	{ ROULEUR_ANA_NCP_EN,                     0x00 },
	{ ROULEUR_ANA_NCP_VCTRL,                  0xA7 },
	{ ROULEUR_ANA_HPHPA_CNP_CTL_1,            0x54 },
	{ ROULEUR_ANA_HPHPA_CNP_CTL_2,            0x2B },
	{ ROULEUR_ANA_HPHPA_PA_STATUS,            0x00 },
	{ ROULEUR_ANA_HPHPA_FSM_CLK,              0x12 },
	{ ROULEUR_ANA_HPHPA_L_GAIN,               0x00 },
	{ ROULEUR_ANA_HPHPA_R_GAIN,               0x00 },
	{ ROULEUR_SWR_HPHPA_HD2,                  0x1B },
	{ ROULEUR_ANA_HPHPA_SPARE_CTL,            0x02 },
	{ ROULEUR_ANA_SURGE_EN,                   0x38 },
	{ ROULEUR_ANA_COMBOPA_CTL,                0x35 },
	{ ROULEUR_ANA_COMBOPA_CTL_4,              0x84 },
	{ ROULEUR_ANA_COMBOPA_CTL_5,              0x05 },
	{ ROULEUR_ANA_RXLDO_CTL,                  0x86 },
	{ ROULEUR_ANA_MBIAS_EN,                   0x00 },
	{ ROULEUR_DIG_SWR_CHIP_ID0,               0x00 },
	{ ROULEUR_DIG_SWR_CHIP_ID1,               0x00 },
	{ ROULEUR_DIG_SWR_CHIP_ID2,               0x0C },
	{ ROULEUR_DIG_SWR_CHIP_ID3,               0x01 },
	{ ROULEUR_DIG_SWR_SWR_TX_CLK_RATE,        0x00 },
	{ ROULEUR_DIG_SWR_CDC_RST_CTL,            0x03 },
	{ ROULEUR_DIG_SWR_TOP_CLK_CFG,            0x00 },
	{ ROULEUR_DIG_SWR_CDC_RX_CLK_CTL,         0x00 },
	{ ROULEUR_DIG_SWR_CDC_TX_CLK_CTL,         0x33 },
	{ ROULEUR_DIG_SWR_SWR_RST_EN,             0x00 },
	{ ROULEUR_DIG_SWR_CDC_RX_RST,             0x00 },
	{ ROULEUR_DIG_SWR_CDC_RX0_CTL,            0xFC },
	{ ROULEUR_DIG_SWR_CDC_RX1_CTL,            0xFC },
	{ ROULEUR_DIG_SWR_CDC_TX_ANA_MODE_0_1,    0x00 },
	{ ROULEUR_DIG_SWR_CDC_COMP_CTL_0,         0x00 },
	{ ROULEUR_DIG_SWR_CDC_RX_DELAY_CTL,       0x66 },
	{ ROULEUR_DIG_SWR_CDC_RX_GAIN_0,          0x55 },
	{ ROULEUR_DIG_SWR_CDC_RX_GAIN_1,          0xA9 },
	{ ROULEUR_DIG_SWR_CDC_RX_GAIN_CTL,        0x00 },
	{ ROULEUR_DIG_SWR_CDC_TX0_CTL,            0x68 },
	{ ROULEUR_DIG_SWR_CDC_TX1_CTL,            0x68 },
	{ ROULEUR_DIG_SWR_CDC_TX_RST,             0x00 },
	{ ROULEUR_DIG_SWR_CDC_REQ0_CTL,           0x01 },
	{ ROULEUR_DIG_SWR_CDC_REQ1_CTL,           0x01 },
	{ ROULEUR_DIG_SWR_CDC_RST,                0x00 },
	{ ROULEUR_DIG_SWR_CDC_AMIC_CTL,           0x02 },
	{ ROULEUR_DIG_SWR_CDC_DMIC_CTL,           0x00 },
	{ ROULEUR_DIG_SWR_CDC_DMIC1_CTL,          0x00 },
	{ ROULEUR_DIG_SWR_CDC_DMIC1_RATE,         0x01 },
	{ ROULEUR_DIG_SWR_PDM_WD_CTL0,            0x00 },
	{ ROULEUR_DIG_SWR_PDM_WD_CTL1,            0x00 },
	{ ROULEUR_DIG_SWR_INTR_MODE,              0x00 },
	{ ROULEUR_DIG_SWR_INTR_MASK_0,            0xFF },
	{ ROULEUR_DIG_SWR_INTR_MASK_1,            0x7F },
	{ ROULEUR_DIG_SWR_INTR_MASK_2,            0x0C },
	{ ROULEUR_DIG_SWR_INTR_STATUS_0,          0x00 },
	{ ROULEUR_DIG_SWR_INTR_STATUS_1,          0x00 },
	{ ROULEUR_DIG_SWR_INTR_STATUS_2,          0x00 },
	{ ROULEUR_DIG_SWR_INTR_CLEAR_0,           0x00 },
	{ ROULEUR_DIG_SWR_INTR_CLEAR_1,           0x00 },
	{ ROULEUR_DIG_SWR_INTR_CLEAR_2,           0x00 },
	{ ROULEUR_DIG_SWR_INTR_LEVEL_0,           0x00 },
	{ ROULEUR_DIG_SWR_INTR_LEVEL_1,           0x2A },
	{ ROULEUR_DIG_SWR_INTR_LEVEL_2,           0x00 },
	{ ROULEUR_DIG_SWR_CDC_CONN_RX0_CTL,       0x00 },
	{ ROULEUR_DIG_SWR_CDC_CONN_RX1_CTL,       0x00 },
	{ ROULEUR_DIG_SWR_LOOP_BACK_MODE,         0x00 },
	{ ROULEUR_DIG_SWR_DRIVE_STRENGTH_0,       0x00 },
	{ ROULEUR_DIG_SWR_DIG_DEBUG_CTL,          0x00 },
	{ ROULEUR_DIG_SWR_DIG_DEBUG_EN,           0x00 },
	{ ROULEUR_DIG_SWR_DEM_BYPASS_DATA0,       0x55 },
	{ ROULEUR_DIG_SWR_DEM_BYPASS_DATA1,       0x55 },
	{ ROULEUR_DIG_SWR_DEM_BYPASS_DATA2,       0x55 },
	{ ROULEUR_DIG_SWR_DEM_BYPASS_DATA3,       0x01 },
};

static bool rouleur_readable_register(struct device *dev, unsigned int reg)
{
	if (reg > ROULEUR_ANA_BASE_ADDR && reg <
				ROULEUR_ANALOG_REGISTERS_MAX_SIZE)
		return rouleur_reg_access_analog[ROULEUR_REG(reg)] & RD_REG;
	if (reg > ROULEUR_DIG_BASE_ADDR && reg <
				ROULEUR_DIGITAL_REGISTERS_MAX_SIZE)
		return rouleur_reg_access_digital[ROULEUR_REG(reg)] & RD_REG;
	return 0;
}

static bool rouleur_writeable_register(struct device *dev, unsigned int reg)
{
	if (reg > ROULEUR_ANA_BASE_ADDR && reg <
					ROULEUR_ANALOG_REGISTERS_MAX_SIZE)
		return rouleur_reg_access_analog[ROULEUR_REG(reg)] & WR_REG;
	if (reg > ROULEUR_DIG_BASE_ADDR && reg <
					ROULEUR_DIGITAL_REGISTERS_MAX_SIZE)
		return rouleur_reg_access_digital[ROULEUR_REG(reg)] & WR_REG;
	return 0;
}

static bool rouleur_volatile_register(struct device *dev, unsigned int reg)
{
	if (reg > ROULEUR_ANA_BASE_ADDR && reg <
					ROULEUR_ANALOG_REGISTERS_MAX_SIZE)
		if ((rouleur_reg_access_analog[ROULEUR_REG(reg)] & RD_REG)
		    && !(rouleur_reg_access_analog[ROULEUR_REG(reg)] & WR_REG))
			return true;
	if (reg > ROULEUR_DIG_BASE_ADDR && reg <
					ROULEUR_DIGITAL_REGISTERS_MAX_SIZE)
		if ((rouleur_reg_access_digital[ROULEUR_REG(reg)] & RD_REG)
		    && !(rouleur_reg_access_digital[ROULEUR_REG(reg)] & WR_REG))
			return true;
	return 0;
}

struct regmap_config rouleur_regmap_config = {
	.name = "rouleur_csr",
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
	.reg_defaults = rouleur_defaults,
	.num_reg_defaults = ARRAY_SIZE(rouleur_defaults),
	.max_register = ROULEUR_ANALOG_MAX_REGISTER +
				ROULEUR_DIGITAL_MAX_REGISTER,
	.readable_reg = rouleur_readable_register,
	.writeable_reg = rouleur_writeable_register,
	.volatile_reg = rouleur_volatile_register,
	.can_multi_write = true,
};
