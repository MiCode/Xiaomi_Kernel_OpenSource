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


unsigned int g_md_gen;
unsigned int g_ap_palt;
struct ccci_dpmaif_platform_ops g_plt_ops;


static int ccci_hif_dpmaif_probe(struct platform_device *pdev)
{
	struct device_node *node_md = NULL;

	node_md = of_find_compatible_node(NULL, NULL, "mediatek,mddriver");
	if (!node_md) {
		CCCI_ERROR_LOG(-1, TAG, "No md driver node in dtsi\n");
		return -2;
	}

	if (of_property_read_u32(node_md,
			"mediatek,md_generation", &g_md_gen)) {
		CCCI_ERROR_LOG(-1, TAG, "read md_generation fail from dtsi\n");
		return -3;
	}

	if (of_property_read_u32(node_md,
			"mediatek,ap_plat_info", &g_ap_palt)) {
		CCCI_ERROR_LOG(-1, TAG, "read ap_plat_info fail from dtsi\n");
		return -4;
	}

	CCCI_NORMAL_LOG(-1, TAG, "g_md_gen: %u; g_ap_palt: %u\n",
			g_md_gen, g_ap_palt);

	if (g_md_gen == 6298)
		return ccci_dpmaif_hif_init_v3(pdev);
	else if (g_md_gen == 6297)
		return ccci_dpmaif_hif_init_v2(pdev);
	else {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: md %d not suport.\n",
			__func__, g_md_gen);
		return -5;
	}
}

static int dpmaif_suspend_noirq(struct device *dev)
{
	if (g_md_gen == 6298)
		return ccci_dpmaif_suspend_noirq_v3(dev);
	else if (g_md_gen == 6297)
		return ccci_dpmaif_suspend_noirq_v2(dev);
	else {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: md %d not suport.\n",
			__func__, g_md_gen);
		return 0;
	}
}

static int dpmaif_resume_noirq(struct device *dev)
{
	if (g_md_gen == 6298)
		return ccci_dpmaif_resume_noirq_v3(dev);
	else if (g_md_gen == 6297)
		return ccci_dpmaif_resume_noirq_v2(dev);
	else {
		CCCI_ERROR_LOG(-1, TAG,
			"[%s] error: md %d not suport.\n",
			__func__, g_md_gen);
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


