/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Min.Guo <min.guo@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/mfd/mt6397/core.h>

#include "pinctrl-mtk-common.h"
#include "pinctrl-mtk-mt6392.h"

#define MT6392_PIN_REG_BASE  0xc000

static const struct mtk_pinctrl_devdata mt6392_pinctrl_data = {
	.pins = mtk_pins_mt6392,
	.npins = ARRAY_SIZE(mtk_pins_mt6392),
	.dir_offset = (MT6392_PIN_REG_BASE + 0x000),
	.pullen_offset = (MT6392_PIN_REG_BASE + 0x020),
	.pullsel_offset = (MT6392_PIN_REG_BASE + 0x040),
	.dout_offset = (MT6392_PIN_REG_BASE + 0x080),
	.din_offset = (MT6392_PIN_REG_BASE + 0x0a0),
	.pinmux_offset = (MT6392_PIN_REG_BASE + 0x0c0),
	.type1_start = 7,
	.type1_end = 7,
	.port_shf = 3,
	.port_mask = 0x3,
	.port_align = 2,
};

static int mt6392_pinctrl_probe(struct platform_device *pdev)
{
	struct mt6397_chip *mt6392;

	mt6392 = dev_get_drvdata(pdev->dev.parent);
	return mtk_pctrl_init(pdev, &mt6392_pinctrl_data, mt6392->regmap);
}

static const struct of_device_id mt6392_pctrl_match[] = {
	{ .compatible = "mediatek,mt6392-pinctrl", },
	{ }
};
MODULE_DEVICE_TABLE(of, mt6392_pctrl_match);

static struct platform_driver mtk_pinctrl_driver = {
	.probe = mt6392_pinctrl_probe,
	.driver = {
		.name = "mediatek-mt6392-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = mt6392_pctrl_match,
	},
};

static int __init mtk_pinctrl_init(void)
{
	return platform_driver_register(&mtk_pinctrl_driver);
}

module_init(mtk_pinctrl_init);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek MT6392 Pinctrl Driver");
MODULE_AUTHOR("Min Guo <min.guo@mediatek.com>");
