/*
 * intel_soc_pmic_crc_opregion.c - Intel SoC PMIC operation region Driver
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

#define DRV_NAME "crystal_cove_region"

#define PWR_SOURCE_SELECT       BIT(1)
#define PMIC_A0LOCK_REG         0xc5

static struct pmic_pwr_table pwr_table[] = {
	{
		.address = 0x04,
		.pwr_reg = {
			.reg = 0x63,
			.bit = 0x00,
		},
	},/* SYSX -> VSYS_SX */
	{
		.address = 0x08,
		.pwr_reg = {
			.reg = 0x62,
			.bit = 0x00,
		},
	},/* SYSU -> VSYS_U */
	{
		.address = 0x0c,
		.pwr_reg = {
			.reg = 0x64,
			.bit = 0x00,
		},
	},/* SYSS -> VSYS_S */
	{
		.address = 0x10,
		.pwr_reg = {
			.reg = 0x6a,
			.bit = 0x00,
		},
	},/* V50S -> V5P0S */
	{
		.address = 0x14,
		.pwr_reg = {
			.reg = 0x6b,
			.bit = 0x00,
		},
	},/* HOST -> VHOST, USB2/3 host */
	{
		.address = 0x18,
		.pwr_reg = {
			.reg = 0x6c,
			.bit = 0x00,
		},
	},/* VBUS -> VBUS, USB2/3 OTG */
	{
		.address = 0x1c,
		.pwr_reg = {
			.reg = 0x6d,
			.bit = 0x00,
		},
	},/* HDMI -> VHDMI */
	{
		.address = 0x24,
		.pwr_reg = {
			.reg = 0x66,
			.bit = 0x00,
		},
	},/* X285 -> V2P85SX, camara */
	{
		.address = 0x2c,
		.pwr_reg = {
			.reg = 0x69,
			.bit = 0x00,
		},
	},/* V33S -> V3P3S, display/ssd/audio */
	{
		.address = 0x30,
		.pwr_reg = {
			.reg = 0x68,
			.bit = 0x00,
		},
	},/* V33U -> V3P3U, SDIO wifi&bt */
	{
		.address = 0x44,
		.pwr_reg = {
			.reg = 0x5c,
			.bit = 0x00,
		},
	},/* V18S -> V1P8S, SOC/USB PHY/SIM */
	{
		.address = 0x48,
		.pwr_reg = {
			.reg = 0x5d,
			.bit = 0x00,
		},
	},/* V18X -> V1P8SX, eMMC/camara/audio */
	{
		.address = 0x4c,
		.pwr_reg = {
			.reg = 0x5b,
			.bit = 0x00,
		},
	},/* V18U -> V1P8U, LPDDR */
	{
		.address = 0x50,
		.pwr_reg = {
			.reg = 0x61,
			.bit = 0x00,
		},
	},/* V12X -> V1P2SX, SOC SFR */
	{
		.address = 0x54,
		.pwr_reg = {
			.reg = 0x60,
			.bit = 0x00,
		},
	},/* V12S -> V1P2S, MIPI */
	{
		.address = 0x5c,
		.pwr_reg = {
			.reg = 0x56,
			.bit = 0x00,
		},
	},/* V10S -> V1P0S, SOC GFX */
	{
		.address = 0x60,
		.pwr_reg = {
			.reg = 0x57,
			.bit = 0x00,
		},
	},/* V10X -> V1P0SX, SOC display/DDR IO/PCIe */
	{
		.address = 0x64,
		.pwr_reg = {
			.reg = 0x59,
			.bit = 0x00,
		},
	},/* V105 -> V1P05S, L2 SRAM */
	{
		.address = 0x68,
		.pwr_reg = {
			.reg = 0x5F,
			.bit = 0x00,
		},
	},/* V3P3SX */
};

static struct pmic_dptf_table dptf_table[] = {
	{
		.address = 0x00,
		.reg = 0x75
	},      /* TMP0 -> SYS0_THRM_RSLT_L */
	{
		.address = 0x04,
		.reg = 0x95
	},      /* AX00 -> SYS0_THRMALRT0_L */
	{
		.address = 0x08,
		.reg = 0x97
	},      /* AX01 -> SYS0_THRMALRT1_L */
	{
		.address = 0x0c,
		.reg = 0x77
	},      /* TMP1 -> SYS1_THRM_RSLT_L */
	{
		.address = 0x10,
		.reg = 0x9a
	},      /* AX10 -> SYS1_THRMALRT0_L */
	{
		.address = 0x14,
		.reg = 0x9c
	},      /* AX11 -> SYS1_THRMALRT1_L */
	{
		.address = 0x18,
		.reg = 0x79
	},      /* TMP2 -> SYS2_THRM_RSLT_L */
	{
		.address = 0x1c,
		.reg = 0x9f
	},      /* AX20 -> SYS2_THRMALRT0_L */
	{
		.address = 0x20,
		.reg = 0xa1
	},      /* AX21 -> SYS2_THRMALRT1_L */
	{
		.address = 0x24,
		.reg = 0x7b,
	},	/* TMP3 -> BAT0_THRM_RSLT_L */
	{
		.address = 0x28,
		.reg = 0xa4
	},	/* AX30 -> BAT0_THRMALRT0_L */
	{
		.address = 0x2c,
		.reg = 0xa6
	},	/* AX31 -> BAT0_THRMALRT1_L */
	{
		.address = 0x30,
		.reg = 0x7d
	},	/* TMP4 -> BAT1_THRM_RSLT_L */
	{
		.address = 0x34,
		.reg = 0xaa
	},	/* AX40 -> BAT1_THRMALRT0_L */
	{
		.address = 0x38,
		.reg = 0xac
	},	/* AX41 -> BAT1_THRMALRT1_L */
	{
		.address = 0x3c,
		.reg = 0x7f
	},	/* TMP5 -> PMIC_THRM_RSLT_L */
	{
		.address = 0x40,
		.reg = 0xb0
	},	/* AX50 -> PMIC_THRMALRT0_L */
	{
		.address = 0x44,
		.reg = 0xb2
	},	/* AX51 -> PMIC_THRMALRT1_L */
	{
		.address = 0x48,
		.reg = 0x94
	},      /* PEN0 -> SYS0_THRMALRT0_H */
	{
		.address = 0x4c,
		.reg = 0x99
	},      /* PEN1 -> SYS1_THRMALRT1_H */
	{
		.address = 0x50,
		.reg = 0x9e
	},      /* PEN2 -> SYS2_THRMALRT2_H */
	{
		.address = 0x54,
		.reg = 0xa3
	},	/* PEN3 -> BAT0_THRMALRT0_H */
	{
		.address = 0x58,
		.reg = 0xa9
	},	/* PEN4 -> BAT1_THRMALRT0_H */
	{
		.address = 0x5c,
		.reg = 0xaf
	},	/* PEN5 -> PMIC_THRMALRT0_H */
};

static int intel_crc_pmic_get_power(struct pmic_pwr_reg *preg, u64 *value)
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

static int intel_crc_pmic_update_power(struct pmic_pwr_reg *preg, bool on)
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
static int intel_crc_pmic_get_raw_temp(int reg)
{
	int ret;
	u8 temp_l, temp_h;

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

	return temp_l | ((temp_h & 0x3) << 8);
}

static int
intel_crc_pmic_update_aux(int reg, int raw)
{
	int ret;

	ret = intel_soc_pmic_writeb(reg, (u8)raw);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "write reg 0x%x failed, 0x%x\n",
			reg, ret);
		return -EIO;
	}
	ret = intel_soc_pmic_update(reg - 1, raw >> 8, 0x3);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "update reg 0x%x failed, 0x%x\n",
			reg, ret);
		return -EIO;
	}

	return 0;
}

static int
intel_crc_pmic_get_policy(int reg, u64 *value)
{
	int ret;

	ret = intel_soc_pmic_readb(reg);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "read reg 0x%x failed, 0x%x\n",
			reg, ret);
		return -EIO;
	}
	*value = ret >> 7;
	return 0;
}

static int intel_crc_pmic_update_policy(int reg, int enable)
{
	int alert0;
	int ret;

	/* Update to policy enable bit requires unlocking a0lock */
	ret = intel_soc_pmic_readb(PMIC_A0LOCK_REG);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "read reg 0x%x failed, 0x%x\n",
			reg, ret);
		return -EIO;
	}
	alert0 = ret;
	ret = intel_soc_pmic_update(PMIC_A0LOCK_REG, 0, 0x01);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "update reg 0x%x failed, 0x%x\n",
			reg, ret);
		return -EIO;
	}
	ret = intel_soc_pmic_update(reg, (enable << 7), 0x80);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "update reg 0x%x failed, 0x%x\n",
			reg, ret);
		return -EIO;
	}
	/* restore alert0 */
	ret = intel_soc_pmic_writeb(PMIC_A0LOCK_REG, alert0);
	if (ret < 0) {
		dev_err(intel_soc_pmic_dev(), "write reg 0x%x failed, 0x%x\n",
			reg, ret);
		return -EIO;
	}

	return 0;
}

static struct intel_soc_pmic_opregion_data intel_crc_pmic_opregion_data = {
	.get_power      = intel_crc_pmic_get_power,
	.update_power   = intel_crc_pmic_update_power,
	.get_raw_temp   = intel_crc_pmic_get_raw_temp,
	.update_aux     = intel_crc_pmic_update_aux,
	.get_policy     = intel_crc_pmic_get_policy,
	.update_policy  = intel_crc_pmic_update_policy,
	.pwr_table      = pwr_table,
	.pwr_table_count = ARRAY_SIZE(pwr_table),
	.dptf_table     = dptf_table,
	.dptf_table_count = ARRAY_SIZE(dptf_table),
};

static int intel_crc_pmic_opregion_probe(struct platform_device *pdev)
{
	return intel_soc_pmic_install_opregion_handler(&pdev->dev,
			ACPI_HANDLE(pdev->dev.parent),
			&intel_crc_pmic_opregion_data);
}

static int intel_crc_pmic_opregion_remove(struct platform_device *pdev)
{
	intel_soc_pmic_remove_opregion_handler(ACPI_HANDLE(pdev->dev.parent));
	return 0;
}

static struct platform_device_id crystal_cove_opregion_id_table[] = {
	{ .name = DRV_NAME },
	{},
};

static struct platform_driver intel_crc_pmic_opregion_driver = {
	.probe = intel_crc_pmic_opregion_probe,
	.remove = intel_crc_pmic_opregion_remove,
	.id_table = crystal_cove_opregion_id_table,
	.driver = {
		.name = DRV_NAME,
	},
};

MODULE_DEVICE_TABLE(platform, crystal_cove_opregion_id_table);

module_platform_driver(intel_crc_pmic_opregion_driver);

MODULE_DESCRIPTION("CrystalCove ACPI opregion driver");
MODULE_LICENSE("GPL");
