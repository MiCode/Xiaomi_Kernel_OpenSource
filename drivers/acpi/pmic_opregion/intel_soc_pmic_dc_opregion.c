/*
 * intel_pmic_xpower.c - XPower AXP288 PMIC operation region driver
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
#include <linux/platform_device.h>
#include <linux/iio/consumer.h>
#include <linux/mfd/intel_soc_pmic.h>
#include "intel_soc_pmic_opregion.h"

#define DRV_NAME "dollar_cove_region"

#define XPOWER_PMICTEMP_LOW	0x57
#define XPOWER_BATTEMP_LOW	0x59
#define XPOWER_GPADC_LOW	0x5b

#define XPOWER_GPIO1_CTL	0x92
#define XPOWER_GPIO1_LDO_OFF	0x04
#define XPOWER_GPIO1_LDO_ON	0x03

static struct pmic_pwr_table pwr_table[] = {
	{
		.address = 0x00,
		.pwr_reg = {
			.reg = 0x13,
			.bit = 0x05,
		},
	},
	{
		.address = 0x04,
		.pwr_reg = {
			.reg = 0x13,
			.bit = 0x06,
		},
	},
	{
		.address = 0x08,
		.pwr_reg = {
			.reg = 0x13,
			.bit = 0x07,
		},
	},
	{
		.address = 0x0c,
		.pwr_reg = {
			.reg = 0x12,
			.bit = 0x03,
		},
	},
	{
		.address = 0x10,
		.pwr_reg = {
			.reg = 0x12,
			.bit = 0x04,
		},
	},
	{
		.address = 0x14,
		.pwr_reg = {
			.reg = 0x12,
			.bit = 0x05,
		},
	},
	{
		.address = 0x18,
		.pwr_reg = {
			.reg = 0x12,
			.bit = 0x06,
		},
	},
	{
		.address = 0x1c,
		.pwr_reg = {
			.reg = 0x12,
			.bit = 0x00,
		},
	},
	{
		.address = 0x20,
		.pwr_reg = {
			.reg = 0x12,
			.bit = 0x01,
		},
	},
	{
		.address = 0x24,
		.pwr_reg = {
			.reg = 0x12,
			.bit = 0x02,
		},
	},
	{
		.address = 0x28,
		.pwr_reg = {
			.reg = 0x13,
			.bit = 0x02,
		},
	},
	{
		.address = 0x2c,
		.pwr_reg = {
			.reg = 0x13,
			.bit = 0x03,
		},
	},
	{
		.address = 0x30,
		.pwr_reg = {
			.reg = 0x13,
			.bit = 0x04,
		},
	},
	{
		.address = 0x34,
		.pwr_reg = {
			.reg = 0x10,
			.bit = 0x03,
		},
	},
	{
		.address = 0x38,
		.pwr_reg = {
			.reg = 0x10,
			.bit = 0x06,
		},
	},
	{
		.address = 0x3c,
		.pwr_reg = {
			.reg = 0x10,
			.bit = 0x05,
		},
	},
	{
		.address = 0x40,
		.pwr_reg = {
			.reg = 0x10,
			.bit = 0x04,
		},
	},
	{
		.address = 0x44,
		.pwr_reg = {
			.reg = 0x10,
			.bit = 0x01,
		},
	},
	{
		.address = 0x48,
		.pwr_reg = {
			.reg = 0x10,
			.bit = 0x00,
		},
	},
	{
		.address = 0x4c,
		.pwr_reg = {
			.reg = 0x92,
			.bit = 0x00,
		},
	},
};

static struct pmic_dptf_table dptf_table[] = {
	{
		.address = 0x00,
		.reg = XPOWER_GPADC_LOW
	},
	{
		.address = 0x0c,
		.reg = XPOWER_GPADC_LOW
	},
	{
		.address = 0x18,
		.reg = XPOWER_GPADC_LOW
	}, /* TMP2 -> SYSTEMP */
	{
		.address = 0x24,
		.reg = XPOWER_BATTEMP_LOW
	}, /* TMP3 -> BATTEMP */
	{
		.address = 0x30,
		.reg = XPOWER_GPADC_LOW
	},
	{
		.address = 0x3c,
		.reg = XPOWER_PMICTEMP_LOW
	}, /* TMP5 -> PMICTEMP */
};

static int intel_xpower_pmic_get_power(struct pmic_pwr_reg *preg, u64 *value)
{
	int ret;
	u8 data;

	ret = intel_soc_pmic_readb(preg->reg);

	if (ret < 0)
		return -EIO;

	data = (u8)ret;
	*value = (data & BIT(preg->bit)) ? 1 : 0;
	return 0;
}

static int intel_xpower_pmic_update_power(struct pmic_pwr_reg *preg, bool on)
{
	int ret;
	u8 data;

	ret = intel_soc_pmic_readb(preg->reg);
	if (ret < 0)
		return -EIO;

	/* For GPIO1 switch on/off the LDO */
	if (preg->reg == XPOWER_GPIO1_CTL) {
		if (on)
			data = XPOWER_GPIO1_LDO_ON;
		else
			data = XPOWER_GPIO1_LDO_OFF;
	} else {
		data = (u8)ret;
		if (on)
			data |= BIT(preg->bit);
		else
			data &= ~BIT(preg->bit);
	}

	ret = intel_soc_pmic_writeb(preg->reg, data);
	if (ret < 0)
		return -EIO;

	return 0;
}

/**
 * intel_xpower_pmic_get_raw_temp(): Get raw temperature reading from the PMIC
 *
 * @reg: register to get the reading
 *
 * We could get the sensor value by manipulating the HW regs here, but since
 * the axp288 IIO driver may also access the same regs at the same time, the
 * APIs provided by IIO subsystem are used here instead to avoid problems. As
 * a result, the two passed in params are of no actual use.
 *
 * Return a positive value on success, errno on failure.
 */
static int intel_xpower_pmic_get_raw_temp(int reg)
{
	struct iio_channel *gpadc_chan;
	int ret, val;
	char *channel_name = NULL;

	if (reg == XPOWER_GPADC_LOW)
		channel_name = "SYSTEMP0";
	else if (reg == XPOWER_BATTEMP_LOW)
		channel_name = "BATTEMP";
	else if (reg == XPOWER_PMICTEMP_LOW)
		channel_name = "PMICTEMP";

	if (channel_name == NULL)
		return -EINVAL;

	gpadc_chan = iio_channel_get(NULL, channel_name);
	if (IS_ERR_OR_NULL(gpadc_chan))
		return -EACCES;

	ret = iio_read_channel_raw(gpadc_chan, &val);
	if (ret < 0)
		val = ret;

	iio_channel_release(gpadc_chan);
	return val;
}

static struct intel_soc_pmic_opregion_data intel_xpower_pmic_opregion_data = {
	.get_power = intel_xpower_pmic_get_power,
	.update_power = intel_xpower_pmic_update_power,
	.get_raw_temp = intel_xpower_pmic_get_raw_temp,
	.pwr_table = pwr_table,
	.pwr_table_count = ARRAY_SIZE(pwr_table),
	.dptf_table = dptf_table,
	.dptf_table_count = ARRAY_SIZE(dptf_table)
};


static int intel_xpower_pmic_opregion_probe(struct platform_device *pdev)
{
	return intel_soc_pmic_install_opregion_handler(&pdev->dev,
			ACPI_HANDLE(pdev->dev.parent),
			&intel_xpower_pmic_opregion_data);
}

static int intel_xpower_pmic_opregion_remove(struct platform_device *pdev)
{
	intel_soc_pmic_remove_opregion_handler(ACPI_HANDLE(pdev->dev.parent));
	return 0;
}

static struct platform_device_id dollar_cove_opregion_id_table[] = {
	{ .name = DRV_NAME },
	{},
};

static struct platform_driver intel_xpower_pmic_opregion_driver = {
	.probe = intel_xpower_pmic_opregion_probe,
	.remove = intel_xpower_pmic_opregion_remove,
	.id_table = dollar_cove_opregion_id_table,
	.driver = {
		.name = DRV_NAME,
	},
};

MODULE_DEVICE_TABLE(platform, dollar_cove_opregion_id_table);
module_platform_driver(intel_xpower_pmic_opregion_driver);

MODULE_DESCRIPTION("XPower AXP288 ACPI operation region driver");
MODULE_LICENSE("GPL");
