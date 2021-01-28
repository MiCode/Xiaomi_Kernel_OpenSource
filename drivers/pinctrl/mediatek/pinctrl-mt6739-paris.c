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

#include "pinctrl-mtk-mt6739.h"
#include "pinctrl-paris.h"

static const struct mtk_pin_reg_calc mt6739_reg_cals[PINCTRL_PIN_REG_MAX] = {
	[PINCTRL_PIN_REG_MODE] = MTK_RANGE(mt6739_pin_mode_range),
	[PINCTRL_PIN_REG_DIR] = MTK_RANGE(mt6739_pin_dir_range),
	[PINCTRL_PIN_REG_DI] = MTK_RANGE(mt6739_pin_di_range),
	[PINCTRL_PIN_REG_DO] = MTK_RANGE(mt6739_pin_do_range),
	[PINCTRL_PIN_REG_SMT] = MTK_RANGE(mt6739_pin_smt_range),
	[PINCTRL_PIN_REG_IES] = MTK_RANGE(mt6739_pin_ies_range),
	[PINCTRL_PIN_REG_PU] = MTK_RANGE(mt6739_pin_pu_range),
	[PINCTRL_PIN_REG_PD] = MTK_RANGE(mt6739_pin_pd_range),
	[PINCTRL_PIN_REG_DRV] = MTK_RANGE(mt6739_pin_drv_range),
	[PINCTRL_PIN_REG_PUPD] = MTK_RANGE(mt6739_pin_pupd_range),
	[PINCTRL_PIN_REG_R0] = MTK_RANGE(mt6739_pin_r0_range),
	[PINCTRL_PIN_REG_R1] = MTK_RANGE(mt6739_pin_r1_range),
};

static const struct mtk_eint_hw mt6739_eint_hw = {
	.port_mask = 7,
	.ports     = 6,
	.ap_num    = 212,
	.db_cnt    = 13,
};

static const struct mtk_pin_soc mt6739_data = {
	.reg_cal = mt6739_reg_cals,
	.pins = mtk_pins_mt6739,
	.npins = ARRAY_SIZE(mtk_pins_mt6739),
	.ngrps = ARRAY_SIZE(mtk_pins_mt6739),
	.nfuncs = 8,
	.eint_hw = &mt6739_eint_hw,
	.gpio_m = 0,
	.bias_set_combo = mtk_pinconf_bias_set_combo,
	.bias_get_combo = mtk_pinconf_bias_get_combo,
	.drive_set = mtk_pinconf_drive_set_direct_val,
	.drive_get = mtk_pinconf_drive_get_direct_val,
};

static const struct of_device_id mt6739_pinctrl_of_match[] = {
	{ .compatible = "mediatek,mt6739-pinctrl", },
	{ }
};

static int mt6739_pinctrl_probe(struct platform_device *pdev)
{
	return mtk_paris_pinctrl_probe(pdev, &mt6739_data);
}

static struct platform_driver mt6739_pinctrl_driver = {
	.driver = {
		.name = "mt6739-pinctrl",
		.of_match_table = mt6739_pinctrl_of_match,
	},
	.probe = mt6739_pinctrl_probe,
};

static int __init mt6739_pinctrl_init(void)
{
	return platform_driver_register(&mt6739_pinctrl_driver);
}
arch_initcall(mt6739_pinctrl_init);
