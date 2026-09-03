// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Communications Inc.
 *
 * Filename : sprd_wcn.c
 * Abstract : This file is a implementation for wcn bsp driver
 */
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/mfd/syscon.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/regmap.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <linux/nvmem-consumer.h>
#include <linux/thermal.h>

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <misc/wcn_bus.h>
#include "sprd_wcn.h"
#include "./sipc/wcn_sipc.h"
#include "pcie.h"

#ifdef CONFIG_PM_SLEEP
static int wcn_resume(struct device *dev)
{
	int chn;
	int ret;
	struct sipc_chn_info *sipc_chn;

	for (chn = 0; chn < SIPC_CHN_NUM; chn++) {
		sipc_chn = wcn_sipc_channel_get(chn);
		if ((sipc_chn != NULL) && (sipc_chn->ops != NULL) &&
			(sipc_chn->ops->power_notify != NULL)) {
			ret = sipc_chn->ops->power_notify(chn, 1);
			if (ret != 0) {
				WCN_INFO("[%s] chn:%d resume fail\n", __func__, chn);
				return ret;
			}
		}
	}
	return 0;
}

static int wcn_suspend(struct device *dev)
{
	int chn;
	int ret;
	struct sipc_chn_info *sipc_chn;

	for (chn = 0; chn < SIPC_CHN_NUM; chn++) {
		sipc_chn = wcn_sipc_channel_get(chn);
		if ((sipc_chn != NULL) && (sipc_chn->ops != NULL) &&
			(sipc_chn->ops->power_notify != NULL)) {
			ret = sipc_chn->ops->power_notify(chn, 0);
			if (ret != 0) {
				WCN_INFO("[%s] chn:%d suspend fail\n", __func__, chn);
				return ret;
			}
		}
	}
	return 0;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(wcn_pm_ops, wcn_suspend, wcn_resume);

static const struct wcn_match_data g_integ_wcn_data = {
	.unisoc_wcn_integrated = true,
	.unisoc_wcn_sipc = true,
	.unisoc_wcn_l6 = true,
};

static const struct wcn_match_data g_marlin3lite_sdio_data = {
	.unisoc_wcn_sdio = true,
	.unisoc_wcn_slp = true,
	.unisoc_wcn_m3lite = true,
};

static const struct wcn_match_data g_marlin3_sdio_data = {
	.unisoc_wcn_sdio = true,
	.unisoc_wcn_slp = true,
	.unisoc_wcn_m3 = true,
};

static const struct wcn_match_data g_marlin3_pcie_data = {
	.unisoc_wcn_pcie = true,
	.unisoc_wcn_m3 = true,
};

#if 0
static const struct wcn_match_data g_marlin3e_sdio_data = {
	.unisoc_wcn_sdio = true,
	.unisoc_wcn_slp = true,
	.unisoc_wcn_m3e = true,
};

static const struct wcn_match_data g_marlin3e_pcie_data = {
	.unisoc_wcn_pcie = true,
	.unisoc_wcn_m3e = true,
};

static const struct wcn_match_data g_m3sdio_marlin_only_integ_gnss_only_data = {
	.unisoc_wcn_sdio = true,
	.unisoc_wcn_slp = true,
	.unisoc_wcn_m3 = true,
	.unisoc_wcn_marlin_only = true,

	.unisoc_wcn_integrated = true,
	.unisoc_wcn_sipc = true,
	.unisoc_wcn_gnss_only = true,
};

static const struct wcn_match_data g_integ_wcn_only_m3sdio_gnss_only_data = {
	.unisoc_wcn_integrated = true,
	.unisoc_wcn_sipc = true,
	.unisoc_wcn_marlin_only = true,

	.unisoc_wcn_sdio = true,
	.unisoc_wcn_slp = true,
	.unisoc_wcn_m3 = true,
	.unisoc_wcn_gnss_only = true,
};
#endif

static const struct of_device_id wcn_global_match_table[] = {
	{ .compatible = "unisoc,integrate_marlin", .data = &g_integ_wcn_data},
	{ .compatible = "unisoc,integrate_gnss", .data = &g_integ_wcn_data},

	{ .compatible = "unisoc,marlin3lite_sdio", .data = &g_marlin3lite_sdio_data},

	{ .compatible = "unisoc,marlin3_sdio", .data = &g_marlin3_sdio_data},

	{ .compatible = "unisoc,marlin3_pcie", .data = &g_marlin3_pcie_data},

#if 0
	{ .compatible = "unisoc,marlin3E_sdio", .data = &g_marlin3e_sdio_data},

	{ .compatible = "unisoc,marlin3E_pcie", .data = &g_marlin3e_pcie_data},

	{ .compatible = "unisoc,marlin3_sdio_wcn_only",
	.data = &g_m3sdio_marlin_only_integ_gnss_only_data},
	{ .compatible = "unisoc,integrate_gnss_only",
	.data = &g_m3sdio_marlin_only_integ_gnss_only_data},

	{ .compatible = "unisoc,integrate_wcn_only",
	.data = &g_integ_wcn_only_m3sdio_gnss_only_data},
	{ .compatible = "unisoc,marlin3_pcie_gnss_only",
	.data = &g_integ_wcn_only_m3sdio_gnss_only_data},
#endif
	{ },
};

static struct wcn_match_data *g_match_data;
struct wcn_match_data *get_wcn_match_config(void)
{
	if (!g_match_data)
		dump_stack();

	return g_match_data;
}
EXPORT_SYMBOL_GPL(get_wcn_match_config);

void module_bus_init(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_pcie)
		module_bus_pcie_init();
	else if (g_match_config && g_match_config->unisoc_wcn_sdio)
		module_bus_sdio_init();
	else if (g_match_config && g_match_config->unisoc_wcn_sipc)
		module_bus_sipc_init();
	else
		dump_stack();
}
EXPORT_SYMBOL(module_bus_init);

void module_bus_deinit(void)
{
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && g_match_config->unisoc_wcn_pcie)
		module_bus_pcie_deinit();
	else if (g_match_config && g_match_config->unisoc_wcn_sdio)
		module_bus_sdio_deinit();
	else if (g_match_config && g_match_config->unisoc_wcn_sipc)
		module_bus_sipc_deinit();
	else
		dump_stack();
}
EXPORT_SYMBOL(module_bus_deinit);

marlin_reset_callback marlin_reset_func;
void *marlin_callback_para;
int marlin_reset_register_notify(void *callback_func, void *para)
{
	marlin_reset_func = (marlin_reset_callback)callback_func;
	marlin_callback_para = para;

	return 0;
}
EXPORT_SYMBOL_GPL(marlin_reset_register_notify);

int marlin_reset_unregister_notify(void)
{
	marlin_reset_func = NULL;
	marlin_callback_para = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(marlin_reset_unregister_notify);

static int sprd_wcn_probe(struct platform_device *pdev)
{
	struct wcn_match_data *p_match_data;
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *of_id =
		of_match_node(wcn_global_match_table, np);

	if (!of_id) {
		pr_info("%s not find matched id!", __func__);
		return -EINVAL;
	}

	p_match_data = (struct wcn_match_data *)of_id->data;
	if (!p_match_data) {
		pr_info("%s not find matched data!", __func__);
		return -EINVAL;
	}

	g_match_data = p_match_data;
	if (p_match_data->unisoc_wcn_integrated)
		return wcn_probe(pdev);
	else
		return marlin_probe(pdev);
}

static int sprd_wcn_remove(struct platform_device *pdev)
{
	struct wcn_match_data *p_match_data;
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *of_id =
		of_match_node(wcn_global_match_table, np);

	if (!of_id) {
		pr_info("%s not find matched id!", __func__);
		return -EINVAL;
	}

	p_match_data = (struct wcn_match_data *)of_id->data;
	if (!p_match_data) {
		pr_info("%s not find matched data!", __func__);
		return -EINVAL;
	}

	if (p_match_data->unisoc_wcn_integrated)
		return wcn_remove(pdev);
	else
		return marlin_remove(pdev);
}

static void sprd_wcn_shutdown(struct platform_device *pdev)
{
	struct wcn_match_data *p_match_data;
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *of_id =
		of_match_node(wcn_global_match_table, np);

	if (!of_id) {
		pr_info("%s not find matched id!", __func__);
		return;
	}

	p_match_data = (struct wcn_match_data *)of_id->data;
	if (!p_match_data) {
		pr_info("%s not find matched data!", __func__);
		return;
	}

	if (p_match_data->unisoc_wcn_integrated)
		wcn_shutdown(pdev);
	else
		marlin_shutdown(pdev);
}

static struct platform_driver sprd_wcn_driver = {
	.driver = {
		.name = "sprd_wcn",
		.pm = &wcn_pm_ops,
		.of_match_table = wcn_global_match_table,
	},
	.probe = sprd_wcn_probe,
	.remove = sprd_wcn_remove,
	.shutdown = sprd_wcn_shutdown,
};

static int __init sprd_wcn_init(void)
{
	pr_info("%s entry!\n", __func__);

	return platform_driver_register(&sprd_wcn_driver);
}

late_initcall(sprd_wcn_init);

static void __exit sprd_wcn_exit(void)
{
	platform_driver_unregister(&sprd_wcn_driver);
}
module_exit(sprd_wcn_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum WCN Driver");
MODULE_AUTHOR("Carson Yang <carson.yang@unisoc.com>");
