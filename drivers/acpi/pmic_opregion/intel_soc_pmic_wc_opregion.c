/*
 * intel_soc_pmic_wc_opregion.c - Intel SoC PMIC operation region Driver
 *
 * Copyright (C) 2014 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/platform_device.h>
#include "intel_soc_pmic_opregion.h"

#define DRV_NAME "whiskey_cove_region"

#define PWR_SOURCE_SELECT       BIT(1)
#define PMIC_A0LOCK_REG         0xc5
#define CURR_SRC_MULTIPLIER	130210

static struct pmic_pwr_table pwr_table[] = {
	{
		.address = 0x04,
		.pwr_reg = {
			.reg = 0x57,
			.bit = 0x00,
		},
	}, /* V18X -> V1P8SX */
	{
		.address = 0x10,
		.pwr_reg = {
			.reg = 0x5a,
			.bit = 0x00,
		},
	}, /* V12X -> V1P2SXCNT */
	{
		.address = 0x14,
		.pwr_reg = {
			.reg = 0x5d,
			.bit = 0x00,
		},
	}, /* V28X -> V2P8SXCNT */
	{
		.address = 0x1c,
		.pwr_reg = {
			.reg = 0x5f,
			.bit = 0x00,
		},
	}, /* V3SD -> V3P3SDCNT */
	{
		.address = 0x20,
		.pwr_reg = {
			.reg = 0x67,
			.bit = 0x00,
		},
	}, /* VSD -> VSDIOCNT */
	{
		.address = 0x24,
		.pwr_reg = {
			.reg = 0x69,
			.bit = 0x05,
		},
	}, /* VSW2 -> VLD0CNT Bit 5*/
	{
		.address = 0x28,
		.pwr_reg = {
			.reg = 0x69,
			.bit = 0x04,
		},
	}, /* VSW1 -> VLD0CNT Bit 4 */
	{
		.address = 0x2C,
		.pwr_reg = {
			.reg = 0x69,
			.bit = 0x01,
		},
	}, /* VUPY -> VLDOCNT Bit 1 */
	{
		.address = 0x30,
		.pwr_reg = {
			.reg = 0x6B,
			.bit = 0x00,
		},
	}, /* VRSO -> VREFSOCCNT*/
	{
		.address = 0x34,
		.pwr_reg = {
			.reg = 0x90,
			.bit = 0x00,
		},
	}, /* VP1A -> VPROG1ACNT */
	{
		.address = 0x38,
		.pwr_reg = {
			.reg = 0x91,
			.bit = 0x00,
		},
	}, /* VP1B -> VPROG1BCNT */
	{
		.address = 0x3c,
		.pwr_reg = {
			.reg = 0x95,
			.bit = 0x00,
		},
	}, /* VP1F -> VPROG1FCNT */
	{
		.address = 0x40,
		.pwr_reg = {
			.reg = 0x99,
			.bit = 0x00,
		},
	}, /* VP2D -> VPROG2DCNT */
	{
		.address = 0x44,
		.pwr_reg = {
			.reg = 0x9a,
			.bit = 0x00,
		},
	}, /* VP3A -> VPROG3ACNT */
	{
		.address = 0x48,
		.pwr_reg = {
			.reg = 0x9b,
			.bit = 0x00,
		},
	}, /* VP3B -> VPROG3BCNT */
	{
		.address = 0x4c,
		.pwr_reg = {
			.reg = 0x9c,
			.bit = 0x00,
		},
	}, /* VP4A -> VPROG4ACNT */
	{
		.address = 0x50,
		.pwr_reg = {
			.reg = 0x9d,
			.bit = 0x00,
		},
	}, /* VP4B -> VPROG4BCNT*/
	{
		.address = 0x54,
		.pwr_reg = {
			.reg = 0x9e,
			.bit = 0x00,
		},
	}, /* VP4C -> VPROG4CCNT */
	{
		.address = 0x58,
		.pwr_reg = {
			.reg = 0x9f,
			.bit = 0x00,
		},
	}, /* VP4D -> VPROG4DCNT*/

	{
		.address = 0x5c,
		.pwr_reg = {
			.reg = 0xa0,
			.bit = 0x00,
		},
	}, /* VP5A -> VPROG5ACNT */
	{
		.address = 0x60,
		.pwr_reg = {
			.reg = 0xa1,
			.bit = 0x00,
		},
	}, /* VP5B -> VPROG5BCNT*/
	{
		.address = 0x64,
		.pwr_reg = {
			.reg = 0xa2,
			.bit = 0x00,
		},
	}, /* VP6A -> VPROG6ACNT */
	{
		.address = 0x68,
		.pwr_reg = {
			.reg = 0xa3,
			.bit = 0x00,
		},
	}  /* VP6B -> VPROG6BCNT*/
};

static struct pmic_dptf_table dptf_table[] = {
	{
		.address = 0x00,
		.reg = 0x4F39
	},      /* TMP0 -> SYS0_THRM_RSLT_L */
	{
		.address = 0x04,
		.reg = 0x4F24
	},      /* AX00 -> SYS0_THRMALRT0_L */
	{
		.address = 0x08,
		.reg = 0x4F26
	},      /* AX01 -> SYS0_THRMALRT1_L */
	{
		.address = 0x0c,
		.reg = 0x4F3B
	},      /* TMP1 -> SYS1_THRM_RSLT_L */
	{
		.address = 0x10,
		.reg = 0x4F28
	},      /* AX10 -> SYS1_THRMALRT0_L */
	{
		.address = 0x14,
		.reg = 0x4F2A
	},      /* AX11 -> SYS1_THRMALRT1_L */
	{
		.address = 0x18,
		.reg = 0x4F3D
	},      /* TMP2 -> SYS2_THRM_RSLT_L */
	{
		.address = 0x1c,
		.reg = 0x4F2C
	},      /* AX20 -> SYS2_THRMALRT0_L */
	{
		.address = 0x20,
		.reg = 0x4F2E
	},      /* AX21 -> SYS2_THRMALRT1_L */
	{
		.address = 0x24,
		.reg = 0x4F3F
	},      /* TMP3 -> BAT0_THRM_RSLT_L */
	{
		.address = 0x28,
		.reg = 0x4F30
	},	/* AX30 -> BAT0_THRMALRT0_L */
	{
		.address = 0x30,
		.reg = 0x4F41
	},      /* TMP4 -> BAT1_THRM_RSLT_L */
	{
		.address = 0x34,
		.reg = 0x4F32
	},	/* AX40 -> BAT1_THRMALRT0_L */
	{
		.address = 0x3c,
		.reg = 0x4F43
	},      /* TMP5 -> PMIC_THRM_RSLT_L */
	{
		.address = 0x40,
		.reg = 0x4F34
	},	/* AX50 -> PMIC_THRMALRT0_L */
	{
		.address = 0x48,
		.reg = 0x4F6A00
	},	/* PEN0 -> THRMALER3PAEN Bit 00 for SYS0 */
	{
		.address = 0x4C,
		.reg = 0x4F6A01
	},	/* PEN1 -> THRMALER3PAEN Bit 01 for SYS1 */
	{
		.address = 0x50,
		.reg = 0x4F6A02
	},	/* PEN2 -> THRMALER3PAEN Bit 02 for SYS2 */
	{
		.address = 0x54,
		.reg = 0x4F6A04
	},	/* PEN3 -> THRMALER3PAEN Bit 03 for Bat 0 */
	{
		.address = 0x58,
		.reg = 0x4F6A05
	},	/* PEN4 -> THRMALER3PAEN Bit 04 for Bat 1*/
	{
		.address = 0x5C,
		.reg = 0x4F6A03
	}	/* PEN5 -> THRMALER3PAEN Bit 03 for PMIC */
};

static int intel_wc_pmic_get_power(struct pmic_pwr_reg *preg, u64 *value)
{
	int ret;
	u8 data;

	ret = intel_soc_pmic_readb(preg->reg);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "read reg 0x%x failed, 0x%x\n",
			preg->reg, ret);
		return -EIO;
	}
	data = (u8) ret;

	*value = (data & PWR_SOURCE_SELECT) && (data & BIT(preg->bit)) ? 1 : 0;
	return 0;
}

static int intel_wc_pmic_update_power(struct pmic_pwr_reg *preg, bool on)
{
	int ret;
	u8 data;

	ret = intel_soc_pmic_readb(preg->reg);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "read reg 0x%x failed, 0x%x\n",
			preg->reg, ret);
		return -EIO;
	}

	data = (u8)ret;
	if (on) {
		data |= PWR_SOURCE_SELECT | BIT(preg->bit);
	} else {
		data &= ~BIT(preg->bit);
		data |= PWR_SOURCE_SELECT;
	}

	ret = intel_soc_pmic_writeb(preg->reg, data);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "write reg 0x%x failed, 0x%x\n",
			preg->reg, ret);
		return -EIO;
	}
	return 0;
}

/* Raw temperature value is 10bits: 8bits in reg and 2bits in reg-1 bit0,1 */
static int intel_wc_pmic_get_raw_temp(int reg)
{
	int ret;
	unsigned int adc_val;
	unsigned int reg_val;
	u8 temp_l, temp_h;
	u8 cursrc;
	unsigned long rlsb;
	static const unsigned long rlsb_array[] = {
		0, 260420, 130210, 65100, 32550, 16280,
		8140, 4070, 2030, 0, 260420, 130210};

	ret = intel_soc_pmic_readb(reg);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "read reg 0x%x failed, 0x%x\n",
			reg, ret);
		return -EIO;
	}
	temp_l = (u8)ret;
	ret = intel_soc_pmic_readb(reg - 1);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "read reg 0x%x failed, 0x%x\n",
			reg, ret);
		return -EIO;
	}
	temp_h = (u8)ret;

	reg_val = (temp_l | ((temp_h & 0x0F) << 8));
	cursrc = (temp_h & 0xF0) >> 4;
	rlsb = rlsb_array[cursrc];
	adc_val = reg_val * rlsb / 1000;

	dev_dbg(intel_soc_pmic_dev(), "adc_val = %x temp_h=%x temp_l=%x\n",
		adc_val, temp_h, temp_l);

	return adc_val;
}

static int
intel_wc_pmic_update_aux(int reg, int resi_val)
{
	int ret;
	u16 raw;
	u32 bsr_num;
	u16 count = 0;
	u16 thrsh = 0;
	u8 cursel = 0;

	bsr_num = resi_val;
	bsr_num /= (1 << 5);

	count = fls(bsr_num) - 1;

	cursel = clamp_t(s8, (count-7), 0, 7);
	thrsh = resi_val / (1 << (4+cursel));

	raw = ((cursel << 9) | thrsh);

	ret = intel_soc_pmic_update((reg - 1), (raw >> 8), 0x0F);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "update reg 0x%x failed, 0x%x\n",
			reg, ret);
		return -EIO;
	}
	ret = intel_soc_pmic_writeb(reg, (u8)raw);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "write reg 0x%x failed, 0x%x\n",
			reg, ret);
		return -EIO;
	}

	return 0;
}

static int
intel_wc_pmic_get_policy(int reg, u64 *value)
{
	int pmic_reg = ((reg >> 8) & 0xFFFF);
	int ret;
	u8 bit;
	u8 mask;

	bit = (u8)(reg & 0xFF);
	mask = (1 << bit);

	dev_dbg(intel_soc_pmic_dev(), "reading reg:%x bit:%x\n", pmic_reg, bit);

	ret = intel_soc_pmic_readb(pmic_reg);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "Error reading reg %x\n",
			pmic_reg);
		return -EIO;
	}

	*value = ((ret & mask) >> bit);
	return 0;
}

static int intel_wc_pmic_update_policy(int reg, int enable)
{
	int pmic_reg = ((reg >> 8) & 0xFFFF);
	int ret;
	u8 bit;
	u8 mask;

	bit = (u8)(reg & 0xFF);
	mask = (1 << bit);

	dev_dbg(intel_soc_pmic_dev(), "updating reg:%x bit:%x value:%x\n",
		pmic_reg, bit, enable);
	ret = intel_soc_pmic_update(pmic_reg, enable, mask);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(),
			"Error updating reg:%x bit:%d enable:%x\n",
			pmic_reg, bit, enable);
		return -EIO;
	}

	return 0;
}

static struct intel_soc_pmic_opregion_data intel_wc_pmic_opregion_data = {
	.get_power      = intel_wc_pmic_get_power,
	.update_power   = intel_wc_pmic_update_power,
	.get_raw_temp   = intel_wc_pmic_get_raw_temp,
	.update_aux     = intel_wc_pmic_update_aux,
	.get_policy     = intel_wc_pmic_get_policy,
	.update_policy  = intel_wc_pmic_update_policy,
	.pwr_table      = pwr_table,
	.pwr_table_count = ARRAY_SIZE(pwr_table),
	.dptf_table     = dptf_table,
	.dptf_table_count = ARRAY_SIZE(dptf_table),
};

static int intel_wc_pmic_opregion_probe(struct platform_device *pdev)
{
	return intel_soc_pmic_install_opregion_handler(&pdev->dev,
			ACPI_HANDLE(pdev->dev.parent),
			&intel_wc_pmic_opregion_data);
}

static int intel_wc_pmic_opregion_remove(struct platform_device *pdev)
{
	intel_soc_pmic_remove_opregion_handler(ACPI_HANDLE(pdev->dev.parent));
	return 0;
}

static struct platform_device_id whiskey_cove_opregion_id_table[] = {
	{ .name = DRV_NAME },
	{},
};

static struct platform_driver intel_wc_pmic_opregion_driver = {
	.probe = intel_wc_pmic_opregion_probe,
	.remove = intel_wc_pmic_opregion_remove,
	.id_table = whiskey_cove_opregion_id_table,
	.driver = {
		.name = DRV_NAME,
	},
};

MODULE_DEVICE_TABLE(platform, whiskey_cove_opregion_id_table);

module_platform_driver(intel_wc_pmic_opregion_driver);

MODULE_DESCRIPTION("WhiskeyCove ACPI opregion driver");
MODULE_LICENSE("GPL");
