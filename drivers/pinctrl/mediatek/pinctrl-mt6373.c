// SPDX-License-Identifier: GPL-2.0
//
/*
 * Copyright (C) 2021 MediaTek Inc.
 * Author: Light Hsieh <light.hsieh@mediatek.com>
 *
 */

#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#include "pinctrl-mtk-common-v2.h"
#include "pinctrl-paris.h"

#define MTK_SIMPLE_PIN(_number, _name, ...) {		\
		.number = _number,			\
		.name = _name,				\
		.funcs = (struct mtk_func_desc[]){	\
			__VA_ARGS__, { } },		\
	}

static const struct mtk_pin_desc mtk_pins_mt6373[] = {
	MTK_SIMPLE_PIN(1, "GPIO1", MTK_FUNCTION(0, "GPIO1")),
	MTK_SIMPLE_PIN(2, "GPIO2", MTK_FUNCTION(0, "GPIO2")),
	MTK_SIMPLE_PIN(3, "GPIO3", MTK_FUNCTION(0, "GPIO3")),
	MTK_SIMPLE_PIN(4, "GPIO4", MTK_FUNCTION(0, "GPIO4")),
	MTK_SIMPLE_PIN(5, "GPIO5", MTK_FUNCTION(0, "GPIO5")),
	MTK_SIMPLE_PIN(6, "GPIO6", MTK_FUNCTION(0, "GPIO6")),
	MTK_SIMPLE_PIN(7, "GPIO7", MTK_FUNCTION(0, "GPIO7")),
	MTK_SIMPLE_PIN(8, "GPIO8", MTK_FUNCTION(0, "GPIO8")),
	MTK_SIMPLE_PIN(9, "GPIO9", MTK_FUNCTION(0, "GPIO9")),
	MTK_SIMPLE_PIN(10, "GPIO10", MTK_FUNCTION(0, "GPIO10")),
	MTK_SIMPLE_PIN(11, "GPIO11", MTK_FUNCTION(0, "GPIO11")),
	MTK_SIMPLE_PIN(12, "GPIO12", MTK_FUNCTION(0, "GPIO12")),
	MTK_SIMPLE_PIN(13, "GPIO13", MTK_FUNCTION(0, "GPIO13")),
	MTK_SIMPLE_PIN(0xFFFFFFFF, "DUMMY", MTK_FUNCTION(0, NULL)),
};

#define PIN_SIMPLE_FIELD_BASE(pin, addr, bit, width) {		\
		.s_pin = pin,					\
		.s_addr = addr,					\
		.s_bit = bit,					\
		.x_bits = width					\
	}

static const struct mtk_pin_field_calc mt6373_pin_smt_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x22, 7, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x23, 0, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x23, 1, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x23, 2, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0x23, 3, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0x23, 4, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0x23, 5, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0x23, 6, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0x23, 7, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0x24, 0, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0x24, 1, 1),
	PIN_SIMPLE_FIELD_BASE(12, 0x24, 2, 1),
	PIN_SIMPLE_FIELD_BASE(13, 0x24, 3, 1),
};

static const struct mtk_pin_field_calc mt6373_pin_drv_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x29, 4, 4),
	PIN_SIMPLE_FIELD_BASE(2, 0x2a, 0, 4),
	PIN_SIMPLE_FIELD_BASE(3, 0x2a, 4, 4),
	PIN_SIMPLE_FIELD_BASE(4, 0x2b, 0, 4),
	PIN_SIMPLE_FIELD_BASE(5, 0x2b, 4, 4),
	PIN_SIMPLE_FIELD_BASE(6, 0x2c, 0, 4),
	PIN_SIMPLE_FIELD_BASE(7, 0x2c, 4, 4),
	PIN_SIMPLE_FIELD_BASE(8, 0x2d, 0, 4),
	PIN_SIMPLE_FIELD_BASE(9, 0x2d, 4, 4),
	PIN_SIMPLE_FIELD_BASE(10, 0x2e, 0, 4),
	PIN_SIMPLE_FIELD_BASE(11, 0x2e, 4, 4),
	PIN_SIMPLE_FIELD_BASE(12, 0x2f, 0, 4),
	PIN_SIMPLE_FIELD_BASE(13, 0x2f, 4, 4),
};

static const struct mtk_pin_field_calc mt6373_pin_dir_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x88, 7, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x8b, 0, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x8b, 1, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x8b, 2, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0x8b, 3, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0x8b, 4, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0x8b, 5, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0x8b, 6, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0x8b, 7, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0x8e, 0, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0x8e, 1, 1),
	PIN_SIMPLE_FIELD_BASE(12, 0x8e, 2, 1),
	PIN_SIMPLE_FIELD_BASE(13, 0x8e, 3, 1),
};

static const struct mtk_pin_field_calc mt6373_pin_pullen_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x91, 7, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x94, 0, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x94, 1, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x94, 2, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0x94, 3, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0x94, 4, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0x94, 5, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0x94, 6, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0x94, 7, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0x97, 0, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0x97, 1, 1),
	PIN_SIMPLE_FIELD_BASE(12, 0x97, 2, 1),
	PIN_SIMPLE_FIELD_BASE(13, 0x97, 3, 1),
};

static const struct mtk_pin_field_calc mt6373_pin_pullsel_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0x9a, 7, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0x9d, 0, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0x9d, 1, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0x9d, 2, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0x9d, 3, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0x9d, 4, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0x9d, 5, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0x9d, 6, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0x9d, 7, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0xa0, 0, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0xa0, 1, 1),
	PIN_SIMPLE_FIELD_BASE(12, 0xa0, 2, 1),
	PIN_SIMPLE_FIELD_BASE(13, 0xa0, 3, 1),
};

static const struct mtk_pin_field_calc mt6373_pin_do_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0xac, 7, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0xaf, 0, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0xaf, 1, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0xaf, 2, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0xaf, 3, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0xaf, 4, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0xaf, 5, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0xaf, 6, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0xaf, 7, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0xb2, 0, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0xb2, 1, 1),
	PIN_SIMPLE_FIELD_BASE(12, 0xb2, 2, 1),
	PIN_SIMPLE_FIELD_BASE(13, 0xb2, 3, 1),
};

static const struct mtk_pin_field_calc mt6373_pin_di_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0xb5, 7, 1),
	PIN_SIMPLE_FIELD_BASE(2, 0xb6, 0, 1),
	PIN_SIMPLE_FIELD_BASE(3, 0xb6, 1, 1),
	PIN_SIMPLE_FIELD_BASE(4, 0xb6, 2, 1),
	PIN_SIMPLE_FIELD_BASE(5, 0xb6, 3, 1),
	PIN_SIMPLE_FIELD_BASE(6, 0xb6, 4, 1),
	PIN_SIMPLE_FIELD_BASE(7, 0xb6, 5, 1),
	PIN_SIMPLE_FIELD_BASE(8, 0xb6, 6, 1),
	PIN_SIMPLE_FIELD_BASE(9, 0xb6, 7, 1),
	PIN_SIMPLE_FIELD_BASE(10, 0xb7, 0, 1),
	PIN_SIMPLE_FIELD_BASE(11, 0xb7, 1, 1),
	PIN_SIMPLE_FIELD_BASE(12, 0xb7, 2, 1),
	PIN_SIMPLE_FIELD_BASE(13, 0xb7, 3, 1),
};

static const struct mtk_pin_field_calc mt6373_pin_mode_range[] = {
	PIN_SIMPLE_FIELD_BASE(1, 0xc4, 3, 3),
	PIN_SIMPLE_FIELD_BASE(2, 0xc7, 0, 3),
	PIN_SIMPLE_FIELD_BASE(3, 0xc7, 3, 3),
	PIN_SIMPLE_FIELD_BASE(4, 0xca, 0, 3),
	PIN_SIMPLE_FIELD_BASE(5, 0xca, 3, 3),
	PIN_SIMPLE_FIELD_BASE(6, 0xcd, 0, 3),
	PIN_SIMPLE_FIELD_BASE(7, 0xcd, 3, 3),
	PIN_SIMPLE_FIELD_BASE(8, 0xd0, 0, 3),
	PIN_SIMPLE_FIELD_BASE(9, 0xd0, 3, 3),
	PIN_SIMPLE_FIELD_BASE(10, 0xd3, 0, 3),
	PIN_SIMPLE_FIELD_BASE(11, 0xd3, 3, 3),
	PIN_SIMPLE_FIELD_BASE(12, 0xd6, 0, 3),
	PIN_SIMPLE_FIELD_BASE(13, 0xd6, 3, 3),
};

#define MTK_RANGE(_a)		{ .range = (_a), .nranges = ARRAY_SIZE(_a), }

static const struct mtk_pin_reg_calc mt6373_reg_cals[PINCTRL_PIN_REG_MAX] = {
	[PINCTRL_PIN_REG_MODE] = MTK_RANGE(mt6373_pin_mode_range),
	[PINCTRL_PIN_REG_DIR] = MTK_RANGE(mt6373_pin_dir_range),
	[PINCTRL_PIN_REG_DI] = MTK_RANGE(mt6373_pin_di_range),
	[PINCTRL_PIN_REG_DO] = MTK_RANGE(mt6373_pin_do_range),
	[PINCTRL_PIN_REG_SMT] = MTK_RANGE(mt6373_pin_smt_range),
	[PINCTRL_PIN_REG_PULLEN] = MTK_RANGE(mt6373_pin_pullen_range),
	[PINCTRL_PIN_REG_PULLSEL] = MTK_RANGE(mt6373_pin_pullsel_range),
	[PINCTRL_PIN_REG_DRV] = MTK_RANGE(mt6373_pin_drv_range),
};

static const struct mtk_pin_soc mt6373_data = {
	.reg_cal = mt6373_reg_cals,
	.pins = mtk_pins_mt6373,
	.npins = 14,
	.ngrps = 14,
	.nfuncs = 2,
	.gpio_m = 0,
	.capability_flags = FLAG_GPIO_START_IDX_1,
};

static int mt6373_pinctrl_probe(struct platform_device *pdev)
{
	return mt63xx_pinctrl_probe(pdev, &mt6373_data);
}

static const struct of_device_id mt6373_pinctrl_of_match[] = {
	{ .compatible = "mediatek,mt6373-pinctrl", },
	{ }
};

static struct platform_driver mt6373_pinctrl_driver = {
	.driver = {
		.name = "mt6373-pinctrl",
		.of_match_table = mt6373_pinctrl_of_match,
	},
	.probe = mt6373_pinctrl_probe,
};
module_platform_driver(mt6373_pinctrl_driver);

MODULE_AUTHOR("Light Hsieh <light.hsieh@mediatek.com>");
MODULE_DESCRIPTION("MT6373 Pinctrl driver");
MODULE_LICENSE("GPL v2");
