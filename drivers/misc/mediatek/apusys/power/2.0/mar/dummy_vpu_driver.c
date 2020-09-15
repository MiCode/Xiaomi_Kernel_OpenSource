// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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

	if (strcmp(pdev->dev.of_node->name, "mtk_vpu0") == 0) {
		register_user = VPU0;
		ret = apu_power_device_register(register_user, pdev);
	} else if (strcmp(pdev->dev.of_node->name, "mtk_vpu1") == 0) {
		register_user = VPU1;
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

	return 0;
}

static int vpu_remove(struct platform_device *pdev)
{
	int ret = 0;
	enum DVFS_USER register_user;

	LOG_INF("remove 0, pdev id = %d name = %s, name = %s\n",
						pdev->id, pdev->name,
						pdev->dev.of_node->name);

	if (strcmp(pdev->dev.of_node->name, "mtk_vpu0") == 0)
		register_user = VPU0;
	else if (strcmp(pdev->dev.of_node->name, "mtk_vpu1") == 0)
		register_user = VPU1;
	else
		return -1;

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
	{.compatible = "mtk,vpu0",},
	{.compatible = "mtk,vpu1",},
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

int vpu_init(struct apusys_core_info *info)
{
	return platform_driver_register(&vpu_driver);
}

void vpu_exit(void)
{
	platform_driver_unregister(&vpu_driver);
}

