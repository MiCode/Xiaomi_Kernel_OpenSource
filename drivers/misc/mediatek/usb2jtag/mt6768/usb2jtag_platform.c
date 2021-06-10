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

#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include "../usb2jtag_v1.h"
#include <mach/upmu_hw.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/sync_write.h>

void __iomem *INFRACFG_AO_BASE;

static int mtk_usb2jtag_hw_init(void)
{
	struct device_node *node = NULL;
	unsigned int temp;

	node = of_find_compatible_node(NULL, NULL, "mediatek,infracfg_ao");
	if (!node) {
		pr_err("[USB2JTAG] map node @ mediatek,infracfg_ao failed\n");
		return -1;
	}

	INFRACFG_AO_BASE = of_iomap(node, 0);
	if (!INFRACFG_AO_BASE) {
		pr_err("[USB2JTAG] iomap INFRACFG_AO_BASE failed\n");
		return -1;
	}

	temp = readl(INFRACFG_AO_BASE + 0xF00);
	writel(temp | 0x4030, INFRACFG_AO_BASE + 0xF00);

	/* Init USB config */
	if (usb2jtag_usb_init() != 0) {
		pr_err("[USB2JTAG] USB init failed\n");
		return -1;
	}

	return 0;
}

static int __init mtk_usb2jtag_platform_init(void)
{
	struct mtk_usb2jtag_driver *mtk_usb2jtag_drv;

	mtk_usb2jtag_drv = get_mtk_usb2jtag_drv();
	mtk_usb2jtag_drv->usb2jtag_init = mtk_usb2jtag_hw_init;
	mtk_usb2jtag_drv->usb2jtag_suspend = NULL;
	mtk_usb2jtag_drv->usb2jtag_resume = NULL;

	return 0;
}

arch_initcall(mtk_usb2jtag_platform_init);
