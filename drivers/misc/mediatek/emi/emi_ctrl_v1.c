/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/printk.h>

#include <mt_emi.h>
#include "emi_ctrl_v1.h"

static void __iomem *CEN_EMI_BASE;
static void __iomem *CHN_EMI_BASE[MAX_CH];
static void __iomem *EMI_MPU_BASE;

static struct emi_info_t emi_info;

static int emi_probe(struct platform_device *pdev);

static int emi_remove(struct platform_device *dev)
{
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id emi_of_ids[] = {
	{.compatible = "mediatek,emi",},
	{}
};
#endif

#ifdef CONFIG_PM
static int emi_suspend_noirq(struct device *dev)
{
	/* pr_info("[EMI] suspend\n"); */

#if ENABLE_ELM
	suspend_elm();
#endif

	return 0;
}

static int emi_resume_noirq(struct device *dev)
{
	/* pr_info("[EMI] resume\n"); */

#if ENABLE_ELM
	resume_elm();
#endif

#ifdef DECS_ON_SSPM
	resume_decs(CEN_EMI_BASE);
#endif

	return 0;
}

static const struct dev_pm_ops emi_pm_ops = {
	.suspend_noirq = emi_suspend_noirq,
	.resume_noirq = emi_resume_noirq,
};
#define EMI_PM_OPS	(&emi_pm_ops)
#else
#define EMI_PM_OPS	NULL
#endif

static struct platform_driver emi_ctrl = {
	.probe = emi_probe,
	.remove = emi_remove,
	.driver = {
		.name = "emi_ctrl",
		.owner = THIS_MODULE,
		.pm = EMI_PM_OPS,
#ifdef CONFIG_OF
		.of_match_table = emi_of_ids,
#endif
	},
};

__weak void plat_debug_api_init(void)
{
}

static int emi_probe(struct platform_device *pdev)
{
	struct resource *res;
	int i;

	pr_info("[EMI] module probe.\n");

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	CEN_EMI_BASE = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(CEN_EMI_BASE)) {
		pr_err("[EMI] unable to map CEN_EMI_BASE\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	EMI_MPU_BASE = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(EMI_MPU_BASE)) {
		pr_err("[EMI] unable to map EMI_MPU_BASE\n");
		return -EINVAL;
	}

	for (i = 0; i < MAX_CH; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 2 + i);
		CHN_EMI_BASE[i] = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(CHN_EMI_BASE[i])) {
			pr_err("[EMI] unable to map CH%d_EMI_BASE\n", i);
			return -EINVAL;
		}
	}

	plat_debug_api_init();

	pr_info("[EMI] get CEN_EMI_BASE @ %p\n", mt_cen_emi_base_get());
	pr_info("[EMI] get EMI_MPU_BASE @ %p\n", mt_emi_mpu_base_get());
	for (i = 0; i < MAX_CH; i++)
		pr_info("[EMI] get CH%d_EMI_BASE @ %p\n",
			i, mt_chn_emi_base_get(i));

#if ENABLE_BWL
	bwl_init(&emi_ctrl);
#endif

#if ENABLE_MPU
	mpu_init(&emi_ctrl, pdev);
#endif

#if ENABLE_ELM
	elm_init(&emi_ctrl, pdev);
#endif

#if ENABLE_MBW
	mbw_init(&emi_ctrl);
#endif

	return 0;
}

/*
 * emi_ctrl_init: module init function.
 */
static int __init emi_ctrl_init(void)
{
	int ret;
	int i;
	struct device_node *node;

	/* register EMI ctrl interface */
	ret = platform_driver_register(&emi_ctrl);
	if (ret)
		pr_err("[EMI/BWL] fail to register emi_ctrl driver\n");

	/* get EMI info from boot tags */
	node = of_find_compatible_node(NULL, NULL, "mediatek,emi");
	if (node) {
		ret = of_property_read_u32(node,
			"emi_info,dram_type", &(emi_info.dram_type));
		if (ret)
			pr_err("[EMI] fail to get dram_type\n");
		ret = of_property_read_u32(node,
			"emi_info,ch_num", &(emi_info.ch_num));
		if (ret)
			pr_err("[EMI] fail to get ch_num\n");
		ret = of_property_read_u32(node,
			"emi_info,rk_num", &(emi_info.rk_num));
		if (ret)
			pr_err("[EMI] fail to get rk_num\n");
		ret = of_property_read_u32_array(node, "emi_info,rank_size",
			emi_info.rank_size, MAX_RK);
		if (ret)
			pr_err("[EMI] fail to get rank_size\n");
	}

	pr_info("[EMI] dram_type(%d)\n", get_dram_type());
	pr_info("[EMI] ch_num(%d)\n", get_ch_num());
	pr_info("[EMI] rk_num(%d)\n", get_rk_num());
	for (i = 0; i < get_rk_num(); i++)
		pr_info("[EMI] rank%d_size(0x%x)", i, get_rank_size(i));

	return 0;
}

/*
 * emi_ctrl_exit: module exit function.
 */
static void __exit emi_ctrl_exit(void)
{
}

postcore_initcall(emi_ctrl_init);
module_exit(emi_ctrl_exit);

unsigned int get_dram_type(void)
{
	return emi_info.dram_type;
}

unsigned int get_ch_num(void)
{
	return emi_info.ch_num;
}

unsigned int get_rk_num(void)
{
	return emi_info.rk_num;
}

unsigned int get_rank_size(unsigned int rank_index)
{
	if ((rank_index < emi_info.rk_num) && (rank_index < MAX_RK))
		return emi_info.rank_size[rank_index];

	return 0;
}

void __iomem *mt_cen_emi_base_get(void)
{
	return CEN_EMI_BASE;
}
EXPORT_SYMBOL(mt_cen_emi_base_get);

/* for legacy use */
void __iomem *mt_emi_base_get(void)
{
	return mt_cen_emi_base_get();
}
EXPORT_SYMBOL(mt_emi_base_get);

void __iomem *mt_chn_emi_base_get(unsigned int channel_index)
{
	if (channel_index < MAX_CH)
		return CHN_EMI_BASE[channel_index];

	return NULL;
}
EXPORT_SYMBOL(mt_chn_emi_base_get);

void __iomem *mt_emi_mpu_base_get(void)
{
	return EMI_MPU_BASE;
}
EXPORT_SYMBOL(mt_emi_mpu_base_get);
