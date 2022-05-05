// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/sched/clock.h> /* local_clock() */
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/fs.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/random.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include <linux/dma-mapping.h>
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <mt-plat/aee.h>
#endif
#include <linux/clk.h>

#include "ccci_hif_dpmaif_comm.h"
#include "ccci_core.h"



#define TAG "dpmaif"

unsigned int g_dpmaif_ver;
unsigned int g_chip_info;
struct ccci_dpmaif_platform_ops g_plt_ops;



int ccci_dpmaif_init_clk(struct device *dev,
		struct dpmaif_clk_node *clk)
{
	if ((!dev) || (!clk)) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: dev(%p) or clk(%p) is NULL.\n",
			__func__, dev, clk);
		return -1;
	}

	while (clk->clk_name) {
		clk->clk_ref = devm_clk_get(dev, clk->clk_name);
		if (IS_ERR(clk->clk_ref)) {
			CCCI_ERROR_LOG(0, TAG,
				 "[%s] error: dpmaif get %s failed.\n",
				 __func__, clk->clk_name);

			clk->clk_ref = NULL;
			return -1;
		}

		clk += 1;
	}

	return 0;
}

void ccci_dpmaif_set_clk(unsigned int on,
		struct dpmaif_clk_node *clk)
{
	int ret;

	if (!clk) {
		CCCI_ERROR_LOG(0, TAG,
			"[%s] error: clk is NULL.\n", __func__);
		return;
	}

	while (clk->clk_name) {
		if (!clk->clk_ref) {
			CCCI_ERROR_LOG(-1, TAG,
				"[%s] error: clock: %s is NULL.\n",
				__func__, clk->clk_name);

			clk += 1;
			continue;
		}

		if (on) {
			ret = clk_prepare_enable(clk->clk_ref);
			if (ret)
				CCCI_ERROR_LOG(-1, TAG,
					"[%s] error: prepare %s fail. %d\n",
					__func__, clk->clk_name, ret);

		} else
			clk_disable_unprepare(clk->clk_ref);

		clk += 1;
	}
}

static int ccci_hif_dpmaif_probe(struct platform_device *pdev)
{
	of_property_read_u32(pdev->dev.of_node,
		"mediatek,chip_info", &g_chip_info);
	CCCI_NORMAL_LOG(-1, TAG, "g_chip_info: %u\n", g_chip_info);

	if (of_property_read_u32(pdev->dev.of_node,
			"mediatek,dpmaif_ver", &g_dpmaif_ver))
		g_dpmaif_ver = 2;

	CCCI_NORMAL_LOG(-1, TAG, "g_dpmaif_ver: %u\n", g_dpmaif_ver);

	if (g_dpmaif_ver == 3)
		return ccci_dpmaif_hif_init_v3(pdev);
	else if (g_dpmaif_ver == 2)
		return ccci_dpmaif_hif_init_v2(pdev);
	else if (g_dpmaif_ver == 1)
		return ccci_dpmaif_hif_init_v1(pdev);
	else {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: g_dpmaif_ver(%d) is invalid.\n",
			__func__, g_dpmaif_ver);
		return -1;
	}
}

static int dpmaif_suspend_noirq(struct device *dev)
{
	if (g_dpmaif_ver == 3)
		return ccci_dpmaif_suspend_noirq_v3(dev);
	else if (g_dpmaif_ver == 2)
		return ccci_dpmaif_suspend_noirq_v2(dev);
	else if (g_dpmaif_ver == 1)
		return ccci_dpmaif_suspend_noirq_v1(dev);
	else {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: g_dpmaif_ver(%u) is invalid.\n",
			__func__, g_dpmaif_ver);
		return 0;
	}
}

static int dpmaif_resume_noirq(struct device *dev)
{
	if (g_dpmaif_ver == 3)
		return ccci_dpmaif_resume_noirq_v3(dev);
	else if (g_dpmaif_ver == 2)
		return ccci_dpmaif_resume_noirq_v2(dev);
	else if (g_dpmaif_ver == 1)
		return ccci_dpmaif_resume_noirq_v1(dev);
	else {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: g_dpmaif_ver(%u) is invalid.\n",
			__func__, g_dpmaif_ver);
		return 0;
	}
}

static const struct dev_pm_ops dpmaif_pm_ops = {
	.suspend_noirq = dpmaif_suspend_noirq,
	.resume_noirq = dpmaif_resume_noirq,
};

static const struct of_device_id ccci_dpmaif_of_ids[] = {
	{.compatible = "mediatek,dpmaif"},
	{}
};

static struct platform_driver ccci_hif_dpmaif_driver = {

	.driver = {
		.name = "ccci_hif_dpmaif",
		.of_match_table = ccci_dpmaif_of_ids,
		.pm = &dpmaif_pm_ops,
	},

	.probe = ccci_hif_dpmaif_probe,
};

static int __init ccci_hif_dpmaif_init(void)
{
	int ret;

	ret = platform_driver_register(&ccci_hif_dpmaif_driver);
	if (ret) {
		CCCI_ERROR_LOG(-1, TAG, "ccci hif_dpmaif driver init fail %d",
			ret);
		return ret;
	}
	return 0;
}

static void __exit ccci_hif_dpmaif_exit(void)
{
}

module_init(ccci_hif_dpmaif_init);
module_exit(ccci_hif_dpmaif_exit);

MODULE_AUTHOR("ccci");
MODULE_DESCRIPTION("ccci hif dpmaif driver");
MODULE_LICENSE("GPL");


