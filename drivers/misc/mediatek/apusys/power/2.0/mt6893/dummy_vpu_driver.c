/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>

#include "apusys_power.h"
#include "apu_log.h"


static int vpu_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int vpu_resume(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
int vpu_pm_suspend(struct device *device)
{
	return 0;
}

int vpu_pm_resume(struct device *device)
{
	return 0;
}

int vpu_pm_restore_noirq(struct device *device)
{
	return 0;
}
#else
#define vpu_pm_suspend NULL
#define vpu_pm_resume  NULL
#define vpu_pm_restore_noirq NULL
#endif


static int vpu_probe(struct platform_device *pdev)
{
	int ret = 0;
	enum DVFS_USER register_user;

	LOG_INF("probe 0, pdev id = %d name = %s, name = %s\n",
						pdev->id, pdev->name,
						pdev->dev.of_node->name);

	if (strcmp(pdev->dev.of_node->name, "vpu_core0") == 0) {
		register_user = VPU0;
		ret = apu_power_device_register(register_user, pdev);
	} else if (strcmp(pdev->dev.of_node->name, "vpu_core1") == 0) {
		register_user = VPU1;
		ret = apu_power_device_register(register_user, NULL);
	} else if (strcmp(pdev->dev.of_node->name, "vpu_core2") == 0) {
		register_user = VPU2;
		ret = apu_power_device_register(register_user, NULL);
	} else {
		return -1;
	}

	if (!ret) {
		LOG_INF("%s register power device %d success\n",
						__func__, register_user);
	} else {
		LOG_ERR("%s register power device %d fail\n",
						__func__, register_user);
		return -1;
	}

	ret = apu_device_power_on(register_user);
	if (!ret) {
		LOG_INF("%s power on device %d success\n",
						__func__, register_user);
	} else {
		LOG_ERR("%s power on device %d fail\n",
						__func__, register_user);
		return -1;
	}

	apu_device_set_opp(register_user, 3);

	return 0;
}

static int vpu_remove(struct platform_device *pdev)
{
	int ret = 0;
	enum DVFS_USER register_user;

	LOG_INF("remove 0, pdev id = %d name = %s, name = %s\n",
						pdev->id, pdev->name,
						pdev->dev.of_node->name);

	if (strcmp(pdev->dev.of_node->name, "vpu_core0") == 0)
		register_user = VPU0;
	else if (strcmp(pdev->dev.of_node->name, "vpu_core1") == 0)
		register_user = VPU1;
	else
		return -1;

	ret = apu_device_power_off(register_user);
	if (!ret) {
		LOG_INF("%s power off device %d success\n",
						__func__, register_user);
	} else {
		LOG_ERR("%s power off device %d fail\n",
						__func__, register_user);
		return -1;
	}

	apu_power_device_unregister(register_user);
	LOG_INF("%s unregister power device %d success\n",
						__func__, register_user);
	return 0;
}

static const struct dev_pm_ops vpu_pm_ops = {
	.suspend = vpu_pm_suspend,
	.resume = vpu_pm_resume,
	.freeze = vpu_pm_suspend,
	.thaw = vpu_pm_resume,
	.poweroff = vpu_pm_suspend,
	.restore = vpu_pm_resume,
	.restore_noirq = vpu_pm_restore_noirq,
};

static const struct of_device_id vpu_of_ids[] = {
	{.compatible = "mediatek,vpu_core0",},
	{.compatible = "mediatek,vpu_core1",},
	{}
};

static struct platform_driver vpu_driver = {
	.probe   = vpu_probe,
	.remove  = vpu_remove,
	.suspend = vpu_suspend,
	.resume  = vpu_resume,
	.driver  = {
		.name = "vpu",
		.owner = THIS_MODULE,
		.of_match_table = vpu_of_ids,
#ifdef CONFIG_PM
		.pm = &vpu_pm_ops,
#endif
	}
};

static int __init vpu_init(void)
{
	return platform_driver_register(&vpu_driver);
}
late_initcall(vpu_init)

static void __exit vpu_exit(void)
{
	platform_driver_unregister(&vpu_driver);
}
module_exit(vpu_exit)

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("vpu dummy driver");
