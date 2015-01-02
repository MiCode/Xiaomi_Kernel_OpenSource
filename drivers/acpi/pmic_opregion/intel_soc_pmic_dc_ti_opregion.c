/*
 * intel_soc_pmic_dc_ti_opregion.c - TI Dollar Cove PMIC operation region driver
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

#define DRV_NAME "dc_ti_region"

#define TI_DC_PMICTEMP_LOW	0x57
#define TI_DC_BATTEMP_LOW	0x59
#define TI_DC_GPADC_LOW	0x5b

static struct pmic_pwr_table pwr_table[] = {
	{
		.address = 0x00,
		.pwr_reg = {
			.reg = 0x41,
			.bit = 0x00,
		},
	},
	{
		.address = 0x04,
		.pwr_reg = {
			.reg = 0x42,
			.bit = 0x00,
		},
	},
	{
		.address = 0x08,
		.pwr_reg = {
			.reg = 0x43,
			.bit = 0x00,
		},
	},
	{
		.address = 0x0c,
		.pwr_reg = {
			.reg = 0x45,
			.bit = 0x00,
		},
	},
	{
		.address = 0x10,
		.pwr_reg = {
			.reg = 0x46,
			.bit = 0x00,
		},
	},
	{
		.address = 0x14,
		.pwr_reg = {
			.reg = 0x47,
			.bit = 0x00,
		},
	},
	{
		.address = 0x18,
		.pwr_reg = {
			.reg = 0x48,
			.bit = 0x00,
		},
	},
	{
		.address = 0x1c,
		.pwr_reg = {
			.reg = 0x49,
			.bit = 0x00,
		},
	},
	{
		.address = 0x20,
		.pwr_reg = {
			.reg = 0x4A,
			.bit = 0x00,
		},
	},
	{
		.address = 0x24,
		.pwr_reg = {
			.reg = 0x4B,
			.bit = 0x00,
		},
	},
	{
		.address = 0x28,
		.pwr_reg = {
			.reg = 0x4C,
			.bit = 0x00,
		},
	},
	{
		.address = 0x2c,
		.pwr_reg = {
			.reg = 0x4D,
			.bit = 0x00,
		},
	},
	{
		.address = 0x30,
		.pwr_reg = {
			.reg = 0x4E,
			.bit = 0x00,
		},
	},
};

static struct pmic_dptf_table dptf_table[] = {
	{
		.address = 0x00,
		.reg = TI_DC_GPADC_LOW
	},
	{
		.address = 0x0c,
		.reg = TI_DC_GPADC_LOW
	},
	{
		.address = 0x18,
		.reg = TI_DC_GPADC_LOW
	}, /* TMP2 -> SYSTEMP */
	{
		.address = 0x24,
		.reg = TI_DC_BATTEMP_LOW
	}, /* TMP3 -> BATTEMP */
	{
		.address = 0x30,
		.reg = TI_DC_GPADC_LOW
	},
	{
		.address = 0x3c,
		.reg = TI_DC_PMICTEMP_LOW
	}, /* TMP5 -> PMICTEMP */
};

static int ti_dollar_cove_pmic_get_power(struct pmic_pwr_reg *preg, u64 *value)
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

static int ti_dollar_cove_pmic_update_power(struct pmic_pwr_reg *preg, bool on)
{
	int ret;
	u8 data;

	ret = intel_soc_pmic_readb(preg->reg);
	if (ret < 0)
		return -EIO;

	data = (u8)ret;
	if (on)
		data |= BIT(preg->bit);
	else
		data &= ~BIT(preg->bit);

	ret = intel_soc_pmic_writeb(preg->reg, data);
	if (ret < 0)
		return -EIO;

	return 0;
}

/**
 * ti_dollar_cove_pmic_get_raw_temp(): Get raw temperature reading from the PMIC
 *
 * @reg: register to get the reading
 *
 * We could get the sensor value by manipulating the HW regs here, but since
 * the dc_ti IIO driver may also access the same regs at the same time, the
 * APIs provided by IIO subsystem are used here instead to avoid problems. As
 * a result, the two passed in params are of no actual use.
 *
 * Return a positive value on success, errno on failure.
 */
static int ti_dollar_cove_pmic_get_raw_temp(int reg)
{
	struct iio_channel *gpadc_chan;
	int ret, val;
	char *channel_name = NULL;

	if (reg == TI_DC_GPADC_LOW)
		channel_name = "SYSTEMP0";
	else if (reg == TI_DC_BATTEMP_LOW)
		channel_name = "BATTEMP";
	else if (reg == TI_DC_PMICTEMP_LOW)
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

static struct intel_soc_pmic_opregion_data ti_dollar_cove_pmic_opregion_data = {
	.get_power = ti_dollar_cove_pmic_get_power,
	.update_power = ti_dollar_cove_pmic_update_power,
	.get_raw_temp = ti_dollar_cove_pmic_get_raw_temp,
	.pwr_table = pwr_table,
	.pwr_table_count = ARRAY_SIZE(pwr_table),
	.dptf_table = dptf_table,
	.dptf_table_count = ARRAY_SIZE(dptf_table)
};

static acpi_status ti_dollar_cove_pmic_gpio_handler(u32 function,
		acpi_physical_address address, u32 bit_width, u64 *value,
		void *handler_context, void *region_context)
{
	return AE_OK;
}

static int ti_dollar_cove_pmic_opregion_probe(struct platform_device *pdev)
{
	struct device *parent = pdev->dev.parent;
	acpi_status status;
	int result;

	result = intel_soc_pmic_install_opregion_handler(&pdev->dev,
			ACPI_HANDLE(parent),
			&ti_dollar_cove_pmic_opregion_data);
	if (!result) {
		status = acpi_install_address_space_handler(
				ACPI_HANDLE(parent), ACPI_ADR_SPACE_GPIO,
				ti_dollar_cove_pmic_gpio_handler, NULL, NULL);
		if (ACPI_FAILURE(status))
			result = -ENODEV;
	}

	return result;
}

static int ti_dollar_cove_pmic_opregion_remove(struct platform_device *pdev)
{
	intel_soc_pmic_remove_opregion_handler(ACPI_HANDLE(pdev->dev.parent));
	return 0;
}

static struct platform_device_id dollar_cove_ti_opregion_id_table[] = {
	{ .name = DRV_NAME },
	{},
};

static struct platform_driver ti_dollar_cove_pmic_opregion_driver = {
	.probe = ti_dollar_cove_pmic_opregion_probe,
	.remove = ti_dollar_cove_pmic_opregion_remove,
	.id_table = dollar_cove_ti_opregion_id_table,
	.driver = {
		.name = DRV_NAME,
	},
};

MODULE_DEVICE_TABLE(platform, dollar_cove_ti_opregion_id_table);
module_platform_driver(ti_dollar_cove_pmic_opregion_driver);

MODULE_DESCRIPTION("TI Dollar Cove ACPI operation region driver");
MODULE_LICENSE("GPL");
