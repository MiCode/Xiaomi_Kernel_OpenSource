/*
 * intel_warn_int.c - This driver configures the warning interrupt that fires
 at first TCO watchdog expiration.
 * Copyright (c) 2014, Intel Corporation.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/acpi.h>
#include <linux/nmi.h>
#include <acpi/actypes.h>

#define DRV_NAME "warn_int"
#define TCO_CLASS DRV_NAME
#define ACPI_TCO_HID "8086229C"

static const struct acpi_device_id tco_device_ids[] = {
	{ACPI_TCO_HID, 0},
	{"", 0},
};
MODULE_DEVICE_TABLE(acpi, tco_device_ids);

static u32 warn_irq_handler(acpi_handle gpe_device, u32 gpe, void *context)
{
	pr_warn("[SHTDWN] %s, WATCHDOG TIMEOUT HANDLER!\n", __func__);

	trigger_all_cpu_backtrace();

	/* Let the tco watchdog reboot the platform */
	if (panic_timeout)
		panic_timeout = 0;

	panic("Kernel Watchdog");

	/* This code should not be reached */
	return IRQ_HANDLED;
}

static int acpi_tco_add(struct acpi_device *device)
{
	acpi_status status;
	unsigned long long tco_gpe;

	status = acpi_evaluate_integer(device->handle, "_GPE", NULL, &tco_gpe);
	if (ACPI_FAILURE(status))
		return -EINVAL;

	status = acpi_install_gpe_handler(NULL, tco_gpe,
					  ACPI_GPE_EDGE_TRIGGERED,
					  warn_irq_handler, NULL);
	if (ACPI_FAILURE(status))
		return -ENODEV;

	acpi_enable_gpe(NULL, tco_gpe);

	pr_info("initialized. Interrupt=SCI GPE 0x%02llx", tco_gpe);
	return 0;
}

static struct acpi_driver tco_driver = {
	.name = "warn_int",
	.class = TCO_CLASS,
	.ids = tco_device_ids,
	.ops = {
		.add = acpi_tco_add,
	},
};

static int __init warn_int_init(void)
{
	return acpi_bus_register_driver(&tco_driver);
}

module_init(warn_int_init);
/* no module_exit, this driver shouldn't be unloaded */

MODULE_AUTHOR("Francois-Nicolas Muller <francois-nicolas.muller@intel.com>");
MODULE_DESCRIPTION("Intel TCO WatchDog Warning Interrupt");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
