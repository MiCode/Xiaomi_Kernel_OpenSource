/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/printk.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <mt-plat/sync_write.h>

#include <soc/mediatek/emi_legacy_v0.h>
#include "emi_ctrl.h"
#include "emi_mbw.h"
#include "emi_elm.h"

#define MAX_RK 2

static void __iomem *CEN_EMI_BASE;
static void __iomem *CHA_EMI_BASE;
static void __iomem *EMI_MPU_BASE;

static unsigned int mpu_irq;

struct emi_info_t {
	unsigned int dram_type;
	unsigned int ch_num;
	unsigned int rk_num;
	unsigned int rank_size[MAX_RK];
};
static struct emi_info_t emi_info;

/*extern void mpu_init(void);*/

unsigned int get_ch_num(void)
{
	return emi_info.ch_num;
}

unsigned int get_rk_num(void)
{
	if (emi_info.rk_num > MAX_RK)
		pr_info("[EMI] rank overflow\n");
	return emi_info.rk_num;
}

unsigned int get_rank_size(unsigned int rank_index)
{
	if (rank_index < emi_info.rk_num)
		return emi_info.rank_size[rank_index];
	return 0;
}

unsigned int get_dram_type(void)
{
	return emi_info.dram_type;
}
EXPORT_SYMBOL(get_dram_type);

void __iomem *mt_cen_emi_base_get(void)
{
	return CEN_EMI_BASE;
}
EXPORT_SYMBOL(mt_cen_emi_base_get);

void __iomem *mt_chn_emi_base_get(void)
{
	return CHA_EMI_BASE;
}
EXPORT_SYMBOL(mt_chn_emi_base_get);

void __iomem *mt_emi_mpu_base_get(void)
{
	return EMI_MPU_BASE;
}
EXPORT_SYMBOL(mt_emi_mpu_base_get);

unsigned int mt_emi_mpu_irq_get(void)
{
	return mpu_irq;
}
EXPORT_SYMBOL(mt_emi_mpu_irq_get);

static ssize_t elm_ctrl_show(struct device_driver *driver, char *buf)
{
	char *ptr;

	ptr = (char *)buf;
	ptr += sprintf(ptr, "ELM enabled: %d\n", is_elm_enabled());

	return strlen(buf);
}

static ssize_t elm_ctrl_store(struct device_driver *driver,
	const char *buf, size_t count)
{
	if (!strncmp(buf, "ON", strlen("ON")))
		enable_elm();
	else if (!strncmp(buf, "OFF", strlen("OFF")))
		disable_elm();

	return count;
}
static DRIVER_ATTR_RW(elm_ctrl);

static int emi_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device_node *node = pdev->dev.of_node;

	pr_debug("[EMI] module probe.\n");

	mpu_irq = 0;
	if (node) {
		mpu_irq = irq_of_parse_and_map(node, 0);
		pr_info("[EMI] get irq of MPU(%d)\n", mpu_irq);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	CEN_EMI_BASE = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(CEN_EMI_BASE)) {
		pr_info("[EMI] unable to map CEN_EMI_BASE\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	CHA_EMI_BASE = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(CHA_EMI_BASE)) {
		pr_info("[EMI] unable to map CHA_EMI_BASE\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	EMI_MPU_BASE = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(EMI_MPU_BASE)) {
		pr_info("[EMI] unable to map EMI_MPU_BASE\n");
		return -EINVAL;
	}

	pr_info("[EMI] get CEN_EMI_BASE @ %p\n", mt_cen_emi_base_get());
	pr_info("[EMI] get CHA_EMI_BASE @ %p\n", mt_chn_emi_base_get());
	pr_info("[EMI] get EMI_MPU_BASE @ %p\n", mt_emi_mpu_base_get());

	writel(0x00000040, CHA_EMI_BASE+0x0008);
	mt_reg_sync_writel(0x00000913, CEN_EMI_BASE+0x5B0);

	mpu_init();
	mbw_init();
	elm_init();

	return 0;
}

static int emi_remove(struct platform_device *dev)
{
	return 0;
}

static const struct of_device_id emi_of_ids[] = {
	{.compatible = "mediatek,emi",},
	{}
};

static int emi_suspend_noirq(struct device *dev)
{
	suspend_elm();
	return 0;
}

static int emi_resume_noirq(struct device *dev)
{
	resume_elm();
	return 0;
}

static const struct dev_pm_ops emi_pm_ops = {
	.suspend_noirq = emi_suspend_noirq,
	.resume_noirq = emi_resume_noirq,
};

static struct platform_driver emi_ctrl = {
	.probe = emi_probe,
	.remove = emi_remove,
	.driver = {
		.name = "emi_ctrl",
		.owner = THIS_MODULE,
		.pm = &emi_pm_ops,
		.of_match_table = emi_of_ids,
	},
};

static int __init emi_ctrl_init(void)
{
	int ret;
	int i;
	struct device_node *of_chosen;

	ret = platform_driver_register(&emi_ctrl);
	if (ret)
		pr_info("[EMI/BWL] fail to register emi_ctrl driver\n");

	ret = driver_create_file(&emi_ctrl.driver, &driver_attr_elm_ctrl);
	if (ret)
		pr_info("[EMI/ELM] fail to create elm_ctrl file\n");

	/* get EMI info from boot tags */
	of_chosen = of_find_node_by_path("/chosen");
	if (of_chosen) {
		ret = of_property_read_u32(of_chosen, "emi_info,dram_type", &(emi_info.dram_type));
		if (ret)
			pr_info("[EMI] init, fail to get dram_type\n");
		ret = of_property_read_u32(of_chosen, "emi_info,ch_num", &(emi_info.ch_num));
		if (ret)
			pr_info("[EMI] fail to get ch_num\n");
		ret = of_property_read_u32(of_chosen, "emi_info,rk_num", &(emi_info.rk_num));
		if (ret)
			pr_info("[EMI] fail to get rk_num\n");
		ret = of_property_read_u32_array(of_chosen, "emi_info,rank_size",
			emi_info.rank_size, MAX_RK);
		if (ret)
			pr_info("[EMI] fail to get rank_size\n");
	}

	pr_info("[EMI] dram_type(%d)\n", get_dram_type());
	pr_info("[EMI] ch_num(%d)\n", get_ch_num());
	pr_info("[EMI] rk_num(%d)\n", get_rk_num());

	for (i = 0; i < get_rk_num(); i++)
		pr_info("[EMI] rank%d_size(0x%x)", i, get_rank_size(i));

	return 0;
}

module_init(emi_ctrl_init);

MODULE_DESCRIPTION("MediaTek EMI LEGACY V0 Driver");
MODULE_LICENSE("GPL v2");
