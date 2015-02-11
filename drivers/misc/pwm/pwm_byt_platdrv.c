/*
 * Intel Baytrail PWM ACPI driver.
 *
 * Copyright (C) 2013 Intel corporation.
 *
 * ----------------------------------------------------------------------------
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/acpi.h>
#include "pwm_byt_core.h"
#include <linux/dmi.h>

#ifdef CONFIG_ACPI
static const struct acpi_device_id pwm_byt_acpi_ids[] = {
	{ "80860F09", PWM_BYT_CLK_KHZ },
	{ "80862288", PWM_CHT_CLK_KHZ },
	{ "80862289", PWM_CHT_CLK_KHZ },
	{ }
};
MODULE_DEVICE_TABLE(acpi, pwm_byt_acpi_ids);
#endif

static int pwm_byt_plat_probe(struct platform_device *pdev)
{
	static int pwm_num;
	struct resource *mem, *ioarea;
	void __iomem *base;
	int r;
	const struct acpi_device_id *id;
	int clk = PWM_BYT_CLK_KHZ;
	const char *board_name;

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "no mem resource?\n");
		return -EINVAL;
	}
	ioarea = request_mem_region(mem->start, resource_size(mem),
			pdev->name);
	if (!ioarea) {
		dev_err(&pdev->dev, "pwm region already claimed\n");
		return -EBUSY;
	}
	base = ioremap_nocache(mem->start, resource_size(mem));
	if (!base) {
		dev_err(&pdev->dev, "I/O memory remapping failed\n");
		r = -ENOMEM;
		goto err_release_region;
	}

#ifdef CONFIG_ACPI
	for (id = pwm_byt_acpi_ids; id->id[0]; id++)
		if (!strncmp(id->id, dev_name(&pdev->dev), strlen(id->id)))
			clk = id->driver_data;
#endif
	board_name = dmi_get_system_info(DMI_BOARD_NAME);
	if (strcmp(board_name, "Cherry Trail CR") == 0)
		clk = PWM_CHT_CLK_KHZ;
	r = pwm_byt_init(&pdev->dev, base, pwm_num, clk);
	if (r)
		goto err_iounmap;

	pm_runtime_set_autosuspend_delay(&pdev->dev, 5);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_allow(&pdev->dev);

	++pwm_num;
	return 0;

err_iounmap:
	iounmap(base);
err_release_region:
	release_mem_region(mem->start, resource_size(mem));
	dev_info(&pdev->dev, "PWM device probe failed!\n");
	return r;
}

static int pwm_byt_plat_remove(struct platform_device *pdev)
{
	struct resource *mem;
	pm_runtime_forbid(&pdev->dev);
	pwm_byt_remove(&pdev->dev);
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem)
		release_mem_region(mem->start, resource_size(mem));
	return 0;
}

static struct platform_driver pwm_byt_plat_driver = {
	.remove	= pwm_byt_plat_remove,
	.driver	= {
		.name	= "pwm-byt-plat",
		.owner	= THIS_MODULE,
		.pm     = &pwm_byt_pm,
#ifdef CONFIG_ACPI
		.acpi_match_table = ACPI_PTR(pwm_byt_acpi_ids),
#endif
	},
};

static int __init pwm_byt_init_driver(void)
{
	return platform_driver_probe(&pwm_byt_plat_driver, pwm_byt_plat_probe);
}
subsys_initcall(pwm_byt_init_driver);

static void __exit pwm_byt_exit_driver(void)
{
	platform_driver_unregister(&pwm_byt_plat_driver);
}
module_exit(pwm_byt_exit_driver);

MODULE_ALIAS("pwm-byt-plat");
MODULE_AUTHOR("Wang, Zhifeng<zhifeng.wang@intel.com>");
MODULE_DESCRIPTION("Intel Baytrail PWM driver");
MODULE_LICENSE("GPL");
