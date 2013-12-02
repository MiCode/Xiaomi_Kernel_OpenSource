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
#include <asm/intel-mid.h>
#include <linux/gpio.h>
#include <linux/lnw_gpio.h>
#include <linux/delay.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/fixed.h>
#include <linux/acpi.h>
#include <linux/acpi_gpio.h>

#define DELAY_ONOFF 250

struct acpi_ids { char *hid; char *uid; };

static struct acpi_ids intel_sdio_ids[] = {
	{"INT33BB", "2"}, /* BYT SDIO */
	{ },
};

static struct acpi_ids intel_brc_ids[] = {
	{"BCM4321", NULL}, /* BYT SDIO */
	{ },
};

static struct regulator_consumer_supply wlan_vmmc_supply = {
	.supply = "vmmc",
};

static struct regulator_init_data wlan_vmmc_data = {
	.constraints = {
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies  = 1,
	.consumer_supplies = &wlan_vmmc_supply,
};

static struct fixed_voltage_config vwlan = {
	.supply_name            = "wlan_en_acpi",
	.microvolts             = 1800000,
	.gpio                   = -EINVAL,
	.startup_delay          = 1000 * DELAY_ONOFF,
	.enable_high            = 1,
	.enabled_at_boot        = 0,
	.init_data              = &wlan_vmmc_data,
};

static void vwlan_device_release(struct device *dev) {}

static struct platform_device vwlan_device = {
	.name   = "reg-fixed-voltage",
	.id             = PLATFORM_DEVID_AUTO,
	.dev = {
		.platform_data  = &vwlan,
		.release = vwlan_device_release,
	},
};

static struct acpi_device *acpi_bus_get_parent(acpi_handle handle)
{
	struct acpi_device *device = NULL;
	acpi_status status;
	int result;
	struct acpi_device *acpi_root;

	result = acpi_bus_get_device(ACPI_ROOT_OBJECT, &acpi_root);
	if (result)
		return NULL;

	/*
	 * Fixed hardware devices do not appear in the namespace and do not
	 * have handles, but we fabricate acpi_devices for them, so we have
	 * to deal with them specially.
	 */
	if (!handle)
		return acpi_root;

	do {
		status = acpi_get_parent(handle, &handle);
		if (ACPI_FAILURE(status))
			return status == AE_NULL_ENTRY ? NULL : acpi_root;
	} while (acpi_bus_get_device(handle, &device));

	return device;
}

static int sdio_acpi_match(struct device *dev, void *data)
{
	struct acpi_ids *ids = data;
	struct acpi_handle *handle = ACPI_HANDLE(dev);
	struct acpi_device_info *info;
	char *uid = NULL;
	acpi_status status;

	status = acpi_get_object_info(handle, &info);
	if (!ACPI_FAILURE(status) && (info->valid & ACPI_VALID_UID))
		uid = info->unique_id.string;
	else
		return false;

	if (!strncmp(ids->hid, dev_name(dev), strlen(ids->hid)))
		if (!strcmp(ids->uid, uid))
			return true;

	return false;
}

static int brc_acpi_match(struct device *dev, void *data)
{
	struct acpi_ids *ids = data;

	if (!strncmp(ids->hid, dev_name(dev), strlen(ids->hid)))
			return true;

	return false;
}


static int brc_fixed_regulator_register_by_acpi(struct platform_device *pdev)
{
	struct device *dev;
	struct acpi_ids *brc_ids;
	struct fixed_voltage_config *fixedcfg = NULL;
	struct regulator_init_data *data = NULL;
	struct acpi_handle *handle;
	struct acpi_device *parent;

	if (!pdev)
		return -ENODEV;
	fixedcfg = pdev->dev.platform_data;
	if (!fixedcfg)
		return -ENODEV;
	data = fixedcfg->init_data;
	if (!data || !data->consumer_supplies)
		return -ENODEV;

	/* get the GPIO pin from ACPI device first */
	for (brc_ids = intel_brc_ids; brc_ids->hid; brc_ids++) {
		dev = bus_find_device(&platform_bus_type, NULL,
				brc_ids, brc_acpi_match);
		if (dev) {
			handle = ACPI_HANDLE(dev);
			if (!ACPI_HANDLE(dev))
				continue;
			parent = acpi_bus_get_parent(handle);
			if (!parent)
				continue;

			data->consumer_supplies->dev_name =
						dev_name(&parent->dev);
			fixedcfg->gpio = acpi_get_gpio_by_index(dev, 1, NULL);
			if (fixedcfg->gpio < 0) {
				dev_info(dev, "No wlan-enable GPIO\n");
				continue;
			}
			dev_info(dev, "wlan-enable GPIO %d found\n",
					fixedcfg->gpio);
			break;
		}
	}

	if (brc_ids->hid) {
		/* add a regulator to control wlan enable gpio */
		return platform_device_register(&vwlan_device);
	}

	return -ENODEV;
}

static int sdio_fixed_regulator_register_by_acpi(struct platform_device *pdev)
{
	struct device *dev;
	struct acpi_ids *sdio_ids;
	struct fixed_voltage_config *fixedcfg = NULL;
	struct regulator_init_data *data = NULL;

	if (!pdev)
		return -ENODEV;
	fixedcfg = pdev->dev.platform_data;
	if (!fixedcfg)
		return -ENODEV;
	data = fixedcfg->init_data;
	if (!data || !data->consumer_supplies)
		return -ENODEV;

	/* get the GPIO pin from ACPI device first */
	for (sdio_ids = intel_sdio_ids; sdio_ids->hid; sdio_ids++) {
		dev = bus_find_device(&platform_bus_type, NULL,
				sdio_ids, sdio_acpi_match);
		if (dev) {
			data->consumer_supplies->dev_name = dev_name(dev);

			fixedcfg->gpio = acpi_get_gpio_by_index(dev, 0, NULL);
			if (fixedcfg->gpio < 0) {
				dev_info(dev, "No wlan-enable GPIO\n");
				continue;
			}
			dev_info(dev, "wlan-enable GPIO %d found\n",
					fixedcfg->gpio);
			break;
		}
	}

	if (sdio_ids->hid) {
		/* add a regulator to control wlan enable gpio */
		return platform_device_register(&vwlan_device);
	}

	return -ENODEV;
}

static int __init wifi_regulator_init(void)
{
	int ret;
	/* register fixed regulator through ACPI device */
	ret = brc_fixed_regulator_register_by_acpi(&vwlan_device);
	if (!ret)
		return ret;

	pr_err("%s: No SDIO host in platform devices\n", __func__);
	return ret;
}
rootfs_initcall(wifi_regulator_init);
