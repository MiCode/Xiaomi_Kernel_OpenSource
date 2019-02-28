/*
 * Copyright (C) 2018 MediaTek Inc.
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

#include "pinctrl-mtk-mt6768.h"
#include "pinctrl-paris.h"

static const struct mtk_pin_reg_calc mt6768_reg_cals[PINCTRL_PIN_REG_MAX] = {
	[PINCTRL_PIN_REG_MODE] = MTK_RANGE(mt6768_pin_mode_range),
	[PINCTRL_PIN_REG_DIR] = MTK_RANGE(mt6768_pin_dir_range),
	[PINCTRL_PIN_REG_DI] = MTK_RANGE(mt6768_pin_di_range),
	[PINCTRL_PIN_REG_DO] = MTK_RANGE(mt6768_pin_do_range),
	[PINCTRL_PIN_REG_SMT] = MTK_RANGE(mt6768_pin_smt_range),
	[PINCTRL_PIN_REG_IES] = MTK_RANGE(mt6768_pin_ies_range),
	[PINCTRL_PIN_REG_PU] = MTK_RANGE(mt6768_pin_pu_range),
	[PINCTRL_PIN_REG_PD] = MTK_RANGE(mt6768_pin_pd_range),
	[PINCTRL_PIN_REG_DRV] = MTK_RANGE(mt6768_pin_drv_range),
	[PINCTRL_PIN_REG_PUPD] = MTK_RANGE(mt6768_pin_pupd_range),
	[PINCTRL_PIN_REG_R0] = MTK_RANGE(mt6768_pin_r0_range),
	[PINCTRL_PIN_REG_R1] = MTK_RANGE(mt6768_pin_r1_range),
};

static const struct mtk_eint_hw mt6768_eint_hw = {
	.port_mask = 7,
	.ports     = 6,
	.ap_num    = 212,
	.db_cnt    = 13,
};

static const struct mtk_pin_soc mt6768_data = {
	.reg_cal = mt6768_reg_cals,
	.pins = mtk_pins_mt6768,
	.npins = ARRAY_SIZE(mtk_pins_mt6768),
	.ngrps = ARRAY_SIZE(mtk_pins_mt6768),
	.nfuncs = 8,
	.eint_hw = &mt6768_eint_hw,
	.gpio_m = 0,
	.bias_set_combo = mtk_pinconf_bias_set_combo,
	.bias_get_combo = mtk_pinconf_bias_get_combo,
	.drive_set = mtk_pinconf_drive_set_direct_val,
	.drive_get = mtk_pinconf_drive_get_direct_val,
};

static const struct of_device_id mt6768_pinctrl_of_match[] = {
	{ .compatible = "mediatek,mt6768-pinctrl", },
	{ }
};

static int mt6768_pinctrl_probe(struct platform_device *pdev)
{
	return mtk_paris_pinctrl_probe(pdev, &mt6768_data);
}

static struct platform_driver mt6768_pinctrl_driver = {
	.driver = {
		.name = "mt6768-pinctrl",
		.of_match_table = mt6768_pinctrl_of_match,
	},
	.probe = mt6768_pinctrl_probe,
};

static int __init mt6768_pinctrl_init(void)
{
	return platform_driver_register(&mt6768_pinctrl_driver);
}
arch_initcall(mt6768_pinctrl_init);
