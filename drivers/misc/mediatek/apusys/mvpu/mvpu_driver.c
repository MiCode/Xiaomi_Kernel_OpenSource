// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include "apusys_power.h"
#include "mvpu_plat_device.h"

#include "apu_config.h"
#include "mvpu_driver.h"
#include <linux/module.h>
#include <linux/of_device.h>
#include "mvpu_plat_device.h"
#include <linux/of_device.h>
#include <linux/dma-mapping.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/sched/clock.h>

static int mvpu_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	return 0;
}

static int mvpu_resume(struct platform_device *pdev)
{
	return 0;
}

static int mvpu_probe(struct platform_device *pdev)
{
	int ret = 0;

	struct device *dev = &pdev->dev;

	dev_info(dev, "mvpu probe start\n");

	/* Initialize platform to allocate mvpu devices first. */
	ret = mvpu_plat_init(pdev);

	if (!ret) {
		dev_info(dev, "(f:%s/l:%d) mvpu register power device pass\n", __func__, __LINE__);
	} else {
		dev_info(dev, "(f:%s/l:%d) register mvpu power fail\n", __func__, __LINE__);
		return ret;
	}
	dev_info(dev, "%s probe pass\n", __func__);

	return 0;
}

static int mvpu_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	dev_info(dev, "%s remove pass\n", __func__);
	return 0;
}

static struct platform_driver mvpu_driver = {
	.probe = mvpu_probe,
	.remove = mvpu_remove,
	.suspend = mvpu_suspend,
	.resume = mvpu_resume,
	.driver = {
		.name = "mtk_mvpu",
		.owner = THIS_MODULE,
	}
};

static int mvpu_drv_init(void)
{
	int ret;

	mvpu_driver.driver.of_match_table = mvpu_plat_get_device();

	ret = platform_driver_register(&mvpu_driver);
	if (ret != 0) {
		pr_info("mvpu, register platform driver fail\n");
		return ret;
	}
	pr_info("mvpu, register platform driver pass\n");
	return 0;
}

static void mvpu_drv_exit(void)
{
	platform_driver_unregister(&mvpu_driver);
}

int mvpu_init(void)
{
	//mvpu_sysfs_init();

	/* Register platform driver after debugfs initialization */
	if (mvpu_drv_init()) {
		//mvpu_sysfs_init();
		return -ENODEV;
	}

	pr_info("%s() done\n", __func__);

	return 0;
}

void mvpu_exit(void)
{
	pr_info("%s()!!\n", __func__);
	//mvpu_sysfs_exit();
	mvpu_drv_exit();
}
