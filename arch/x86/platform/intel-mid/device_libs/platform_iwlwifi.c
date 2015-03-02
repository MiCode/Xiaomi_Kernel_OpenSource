/*
 * platform_iwlwifi_pcie.c: iwlwifi pcie platform device.
 *
 * (C) Copyright 2014 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/platform_data/iwlwifi.h>
#ifdef CONFIG_ACPI
#include <linux/acpi.h>
#endif


#ifdef CONFIG_ACPI
static struct acpi_device_id iwlwifi_acpi_id[] = {
	{ "INT5502" , 0 },
	{ "INT8260" , 0 },
	{},
};

static int iwlwifi_acpi_match_platform(struct device *dev, void *data)
{
	struct acpi_device_id *ids = data, *id;
	struct platform_device *pdev = to_platform_device(dev);

	if (!pdev)
		return FALSE;

	for (id = ids; id->id[0]; id++) {
		if (!strncmp(id->id, pdev->name, strlen(id->id)))
			return TRUE;
	}
	return FALSE;
}
#endif

static void iwlwifi_pcie_enable_regulator(void)
{
#ifdef CONFIG_ACPI
	struct device *dev = NULL;
	struct acpi_device *adev = NULL;
	acpi_handle handle;

	dev = bus_find_device(&platform_bus_type, NULL, iwlwifi_acpi_id,
			      iwlwifi_acpi_match_platform);

	if (dev && ACPI_HANDLE(dev)) {
		handle = ACPI_HANDLE(dev);
		if (acpi_bus_get_device(handle, &adev) == 0)
			acpi_device_set_power(adev, ACPI_STATE_D0);
	} else
		pr_err("%s device not found\n", __func__);
#endif
	msleep(100);

}

static void iwlwifi_pcie_disable_regulator(void)
{
#ifdef CONFIG_ACPI
	struct device *dev = NULL;
	struct acpi_device *adev = NULL;
	acpi_handle handle;

	dev = bus_find_device(&platform_bus_type, NULL, iwlwifi_acpi_id,
			      iwlwifi_acpi_match_platform);

	if (dev && ACPI_HANDLE(dev)) {
		handle = ACPI_HANDLE(dev);
		if (acpi_bus_get_device(handle, &adev) == 0)
			acpi_device_set_power(adev, ACPI_STATE_D3_COLD);
	} else
		pr_err("%s device not found\n", __func__);
#endif
}

static struct iwl_trans_platform_ops iwlwifi_pcie_platform_ops = {
	iwlwifi_pcie_enable_regulator,
	iwlwifi_pcie_disable_regulator
};

static struct platform_device iwlwifi_pcie_platform_device = {
	.name = "iwlwifi_pcie_platform",
	.id = -1,
	.dev = {
		.platform_data = &iwlwifi_pcie_platform_ops,
	},
};

int __init iwlwifi_platform_init(void)
{
	int ret = 0;

	ret = platform_device_register(&iwlwifi_pcie_platform_device);
	pr_info("iwlwifi platform init\n");

	return ret;
}
device_initcall(iwlwifi_platform_init);
