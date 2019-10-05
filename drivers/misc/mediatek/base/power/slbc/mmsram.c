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
#include <linux/clk.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "mmsram.h"

#define MMSYSRAM_INTEN0		(0x000)
#define MMSYSRAM_INTEN1		(0x004)
#define MMSYSRAM_INTEN2		(0x008)
#define MMSYSRAM_SEC_ADDR0	(0x040)

struct mmsram_dev {
	void __iomem *ctrl_base;
	void __iomem *sram_paddr;
	void __iomem *sram_vaddr;
	ssize_t sram_size;
	struct clk *mux;
};

static struct mmsram_dev *mmsram;


int mmsram_power_on(void)
{
	int ret = 0;

	if (!IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)) {
		ret = clk_prepare_enable(mmsram->mux);

		if (ret)
			pr_notice("mmsram clk enable fail:%d\n", ret);
	}
	pr_notice("mmsram power on\n");
	return ret;
}
EXPORT_SYMBOL_GPL(mmsram_power_on);

void mmsram_power_off(void)
{
	if (!IS_ENABLED(CONFIG_FPGA_EARLY_PORTING))
		clk_disable_unprepare(mmsram->mux);
	pr_notice("mmsram power off\n");
}
EXPORT_SYMBOL_GPL(mmsram_power_off);

int enable_mmsram(void)
{
	int ret;

	ret = mmsram_power_on();
	if (!ret) {
		writel(0x1, mmsram->ctrl_base + MMSYSRAM_INTEN0);
		writel(0x1, mmsram->ctrl_base + MMSYSRAM_INTEN1);
		writel(0x1, mmsram->ctrl_base + MMSYSRAM_INTEN2);
		writel((0x1 << 24) | 0x160000,
			mmsram->ctrl_base + MMSYSRAM_SEC_ADDR0);
	}
	pr_notice("enable mmsram\n");

	return ret;
}
EXPORT_SYMBOL_GPL(enable_mmsram);

void disable_mmsram(void)
{
	writel(0, mmsram->ctrl_base + MMSYSRAM_SEC_ADDR0);
	writel(0, mmsram->ctrl_base + MMSYSRAM_INTEN0);
	writel(0, mmsram->ctrl_base + MMSYSRAM_INTEN1);
	writel(0, mmsram->ctrl_base + MMSYSRAM_INTEN2);
	mmsram_power_off();
	pr_notice("disable mmsram\n");
}
EXPORT_SYMBOL_GPL(disable_mmsram);

void mmsram_get_info(struct mmsram_data *data)
{
	data->paddr = mmsram->sram_paddr;
	data->vaddr = mmsram->sram_vaddr;
	data->size = mmsram->sram_size;
	pr_notice("%s: pa:%p va:%p size:%zu\n",
		__func__, data->paddr, data->vaddr, data->size);
}
EXPORT_SYMBOL_GPL(mmsram_get_info);

static int mmsram_probe(struct platform_device *pdev)
{
	struct resource *res;

	mmsram = devm_kzalloc(&pdev->dev, sizeof(*mmsram), GFP_KERNEL);
	if (!mmsram)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_notice(&pdev->dev, "could not get resource for ctrl\n");
		return -EINVAL;
	}

	mmsram->ctrl_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mmsram->ctrl_base)) {
		dev_notice(&pdev->dev, "could not ioremap resource for ctrl\n");
		return PTR_ERR(mmsram->ctrl_base);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res) {
		dev_notice(&pdev->dev, "could not get resource for memory\n");
		return -EINVAL;
	}
	mmsram->sram_paddr = (void *)res->start;
	mmsram->sram_size = resource_size(res);
	mmsram->sram_vaddr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(mmsram->sram_vaddr)) {
		dev_notice(&pdev->dev, "could not ioremap resource for memory\n");
		return PTR_ERR(mmsram->sram_vaddr);
	}

	dev_notice(&pdev->dev, "probe va=%p pa=%p size=%d\n",
		mmsram->sram_vaddr, mmsram->sram_paddr, mmsram->sram_size);

	if (!IS_ENABLED(CONFIG_FPGA_EARLY_PORTING)) {
		mmsram->mux = devm_clk_get(&pdev->dev, "mmsram_mux");
		if (IS_ERR(mmsram->mux)) {
			dev_notice(&pdev->dev, "could not get mmsram mux info\n");
			return PTR_ERR(mmsram->mux);
		}
	}

	return 0;
}

static const struct of_device_id of_mmsram_match_tbl[] = {
	{
		.compatible = "mediatek,mmsram",
	},
	{}
};

static struct platform_driver mmsram_drv = {
	.probe = mmsram_probe,
	.driver = {
		.name = "mtk-mmsram",
		.owner = THIS_MODULE,
		.of_match_table = of_mmsram_match_tbl,
	},
};
static int __init mtk_mmsram_init(void)
{
	s32 status;

	status = platform_driver_register(&mmsram_drv);
	if (status) {
		pr_notice("Failed to register mmsram driver(%d)\n", status);
		return -ENODEV;
	}
	return 0;
}
static void __exit mtk_mmsram_exit(void)
{
	platform_driver_unregister(&mmsram_drv);
}
module_init(mtk_mmsram_init);
module_exit(mtk_mmsram_exit);
MODULE_DESCRIPTION("MTK MMSRAM driver");
MODULE_AUTHOR("Anthony Huang<anthony.huang@mediatek.com>");
MODULE_LICENSE("GPL");
