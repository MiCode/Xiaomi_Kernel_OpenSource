/*
 * platform_sdio_regulator.c: sdio regulator platform device initilization file
 *
 * (C) Copyright 2011 Intel Corporation
 * Author: chuanxiao.dong@intel.com, feiyix.ning@intel.com
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/lnw_gpio.h>
#include <linux/delay.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/gpio-regulator.h>
#include <linux/acpi.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/platform_device.h>
#include <asm/cpu_device_id.h>
#include <linux/regulator/intel_whiskey_cove_pmic.h>

struct acpi_ids { char *hid; char *uid; };

static struct acpi_ids intel_sd_ids[] = {
	{"80860F14", "3"}, /* BYT SD */
	{"INT33BB", "3"},
	{ },
};

static struct regulator_consumer_supply sd_vqmmc_consumer[] = {
	REGULATOR_SUPPLY("vqmmc", "80860F14:01"),
	REGULATOR_SUPPLY("vqmmc", "INT33BB:01"),
};

static struct regulator_consumer_supply sd_vmmc_consumer[] = {
	REGULATOR_SUPPLY("vmmc", "80860F14:01"),
	REGULATOR_SUPPLY("vmmc", "INT33BB:01"),
};

static int sdhc_acpi_match(struct device *dev, void *data)
{
	struct acpi_ids *ids = data;
	struct acpi_handle *handle = ACPI_HANDLE(dev);
	struct acpi_device_info *info;
	acpi_status status;
	int match = 0;

	status = acpi_get_object_info(handle, &info);
	if (ACPI_FAILURE(status))
		return 0;

	if (!(info->valid & ACPI_VALID_UID) ||
		!(info->valid & ACPI_VALID_HID))
		goto free;

	if (!strncmp(ids->hid, info->hardware_id.string, strlen(ids->hid)))
		if (!strncmp(ids->uid, info->unique_id.string,
					strlen(ids->uid)))
			match = 1;
free:
	kfree(info);
	return match;
}

/* vsdcard regulator */
static struct regulator_init_data ccove_vsdcard_data = {
	.constraints = {
		.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
	},
	.num_consumer_supplies	= ARRAY_SIZE(sd_vmmc_consumer),
	.consumer_supplies	= sd_vmmc_consumer,
};

static struct fixed_voltage_config ccove_vsdcard = {
	.supply_name	= "gpio_vsdcard",
	.microvolts	= 3300000,
	.init_data	= &ccove_vsdcard_data,
};

/* vsdio regulator */
static struct regulator_init_data ccove_vsdio_data = {
	.constraints = {
		.min_uV			= 1700000,
		.max_uV			= 3300000,
		.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.valid_modes_mask	= REGULATOR_MODE_NORMAL,
	},
	.num_consumer_supplies	= ARRAY_SIZE(sd_vqmmc_consumer),
	.consumer_supplies	= sd_vqmmc_consumer,
};

static struct gpio_regulator_state ccove_vsdio_states[] = {
	{
		.value = 3300000,
		.gpios = 0,
	},
	{
		.value = 1800000,
		.gpios = 1,
	},
};

static struct gpio ccove_vsdio_gpios = {
	.flags = GPIOF_OUT_INIT_LOW,
	.label = "vsdio",
};

static struct gpio_regulator_config ccove_vsdio = {
	.supply_name	= "gpio_vsdio",
	.enable_gpio	= -ENODEV,
	.gpios		= &ccove_vsdio_gpios,
	.nr_gpios	= 1,
	.states		= ccove_vsdio_states,
	.nr_states	= ARRAY_SIZE(ccove_vsdio_states),
	.type		= REGULATOR_VOLTAGE,
	.init_data	= &ccove_vsdio_data,
};

static void intel_setup_ccove_sd_regulators(void)
{
	struct device *dev = NULL;
	struct acpi_ids *sdhc_ids;
	struct gpio_desc *desc = NULL;

	for (sdhc_ids = intel_sd_ids; sdhc_ids->hid; sdhc_ids++) {
		dev = bus_find_device(&platform_bus_type, NULL,
				sdhc_ids, sdhc_acpi_match);
		if (dev)
			break;
	}

	if (!dev) {
		pr_warn("%s: no match device found\n", __func__);
		return;
	}

	/* configure vsdio */
	desc = devm_gpiod_get_index(dev, "sd_vsdio", 2);
	if (!IS_ERR(desc)) {
		ccove_vsdio_gpios.gpio = desc_to_gpio(desc);
		devm_gpiod_put(dev, desc);
		pr_info("%s: sd_vsdio gpio %d\n", __func__,
				ccove_vsdio_gpios.gpio);
	} else
		/* set NULL data for GPIO name */
		ccove_vsdio.supply_name = NULL;

	intel_soc_pmic_set_pdata("gpio-regulator", &ccove_vsdio,
			sizeof(struct gpio_regulator_config), 0);

	/* configure vsdcard */
	desc = devm_gpiod_get_index(dev, "sd_vsdcard", 3);
	if (!IS_ERR(desc)) {
		ccove_vsdcard.gpio = desc_to_gpio(desc);
		devm_gpiod_put(dev, desc);
		pr_info("%s: sd_vsdcard gpio %d\n", __func__,
				ccove_vsdcard.gpio);
	} else
		/* set NULL data for GPIO name */
		ccove_vsdcard.supply_name = NULL;

	intel_soc_pmic_set_pdata("reg-fixed-voltage", &ccove_vsdcard,
			sizeof(struct fixed_voltage_config), 0);
}

/*************************************************************
*
* WCOVE SD card related regulator
*
*************************************************************/
static struct regulator_init_data wcove_vmmc_data;
static struct regulator_init_data wcove_vqmmc_data;

static struct wcove_regulator_info wcove_vmmc_info = {
	.init_data = &wcove_vmmc_data,
};

static struct wcove_regulator_info wcove_vqmmc_info = {
	.init_data = &wcove_vqmmc_data,
};

static void intel_setup_whiskey_cove_sd_regulators(void)
{
	memcpy((void *)&wcove_vmmc_data, (void *)&vmmc_data,
			sizeof(struct regulator_init_data));
	memcpy((void *)&wcove_vqmmc_data, (void *)&vqmmc_data,
			sizeof(struct regulator_init_data));

	/* set enable time for vqmmc regulator, stabilize power rail */
	wcove_vqmmc_data.constraints.enable_time = 20000;

	/* register SD card regulator for whiskey cove PMIC */
	intel_soc_pmic_set_pdata("wcove_regulator", &wcove_vmmc_info,
		sizeof(struct wcove_regulator_info), WCOVE_ID_V3P3SD + 1);
	intel_soc_pmic_set_pdata("wcove_regulator", &wcove_vqmmc_info,
		sizeof(struct wcove_regulator_info), WCOVE_ID_VSDIO + 1);
}

static int __init sdio_regulator_init(void)
{
	intel_setup_ccove_sd_regulators();

	intel_setup_whiskey_cove_sd_regulators();

	return 0;
}
fs_initcall_sync(sdio_regulator_init);
