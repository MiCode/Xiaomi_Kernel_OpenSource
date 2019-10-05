/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include "mt6311-i2c.h"
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/mediatek/mtk_regulator_core.h>
#ifdef IPIMB_MT6311
#include <mach/mtk_pmic_ipi.h>
#endif /* IPIMB */
#include <linux/delay.h>

#define MT6311_MIN_VOLTAGE   (600000)
#define MT6311_MAX_VOLTAGE   (1393750)

#define MT6311_step_uV		6250
#define MT6311_enable_bit	0
#define MT6311_enable_mask	1
#define MT6311_mode_bit		6
#define MT6311_mode_mask	1

#define mt6311_proc_vol_reg	(MT6311_PMIC_VDVFS11_VOSEL_ON_ADDR)
#define mt6311_proc_vol_mask	(MT6311_PMIC_VDVFS11_VOSEL_ON_MASK)
#define mt6311_proc_vol_shift	(MT6311_PMIC_VDVFS11_VOSEL_ON_SHIFT)
#define mt6311_proc_enable_reg	(MT6311_PMIC_VDVFS11_EN_ADDR)
#define mt6311_proc_enable_bit	(MT6311_enable_bit)
#define mt6311_proc_min_uV	(MT6311_MIN_VOLTAGE)
#define mt6311_proc_max_uV	(MT6311_MAX_VOLTAGE)
#define mt6311_proc_id		(0)
#define mt6311_proc_mode_reg	(MT6311_PMIC_RG_VDVFS11_MODESET_ADDR)
#define mt6311_proc_mode_bit	(MT6311_mode_bit)
#define mt6311_n_voltages	\
	((MT6311_MAX_VOLTAGE - MT6311_MIN_VOLTAGE) / MT6311_step_uV + 1)

static int mt6311_list_voltage(
		struct mtk_simple_regulator_desc *mreg_desc,
		unsigned int selector)
{
	return (selector >= mreg_desc->rdesc.n_voltages) ?
		-EINVAL : (mreg_desc->min_uV + selector * MT6311_step_uV);
}

static struct mtk_simple_regulator_desc mt6311_desc_table[] = {
	mreg_decl(mt6311_proc, mt6311_list_voltage, mt6311_n_voltages, NULL),
};

static int mt6311_set_voltage_sel(
		struct mtk_simple_regulator_desc *mreg_desc,
		unsigned int selector)
{
	int ret;

	MT6311LOG("[%s] (%s) selector = %d\n"
		  , __func__, mreg_desc->rdesc.name, selector);

	if (selector > mreg_desc->rdesc.n_voltages)
		return -EINVAL;

	MT6311LOG("[%s] (%s) Vout = %d\n"
		  , __func__, mreg_desc->rdesc.name
		  , mt6311_list_voltage(mreg_desc, selector));
	ret = mt6311_assign_bit(mreg_desc->vol_reg
				, mreg_desc->vol_mask, selector);

	MT6311LOG("[%s] (%s) ret = %d\n", __func__, mreg_desc->rdesc.name, ret);

	return ret;
}

static int mt6311_get_voltage_sel(
		struct mtk_simple_regulator_desc *mreg_desc)
{
	int ret;
	unsigned char data;

	ret = mt6311_read_byte(mreg_desc->vol_reg, &data);
	if (ret < 0)
		return ret;

	data = (data & mreg_desc->vol_mask) >> mreg_desc->vol_shift;
	MT6311LOG("[%s] (%s) selector = %d\n"
		  , __func__, mreg_desc->rdesc.name, data);

	return data;
}

static int mt6311_enable(struct mtk_simple_regulator_desc *mreg_desc)
{
	int ret = 0;

	MT6311LOG("[%s] enable (%s)\n", __func__, mreg_desc->rdesc.name);
	ret = mt6311_config_interface(mreg_desc->enable_reg
				, 1, MT6311_enable_mask, mreg_desc->enable_bit);
	if (ret < 0)
		pr_notice(MT6311TAG "[%s] enable (%s) fail, ret = %d\n",
			__func__, mreg_desc->rdesc.name, ret);

	return ret;
}

static int mt6311_disable(struct mtk_simple_regulator_desc *mreg_desc)
{
	int ret = 0;

	MT6311LOG("[%s] disable (%s)\n", __func__, mreg_desc->rdesc.name);
	if (mreg_desc->rdev->use_count == 0) {
		pr_notice(MT6311TAG "MT6311 should not be disable (use_count=%d)\n"
			  , mreg_desc->rdev->use_count);
		return -1;
	}
	ret = mt6311_config_interface(mreg_desc->enable_reg
				, 0, MT6311_enable_mask, mreg_desc->enable_bit);
	if (ret < 0)
		pr_notice(MT6311TAG "[%s] disable (%s) fail, ret = %d\n",
			__func__, mreg_desc->rdesc.name, ret);

	return ret;
}

static int mt6311_is_enabled(struct mtk_simple_regulator_desc *mreg_desc)
{
	unsigned char en = 0;
	int ret = 0;

	ret = mt6311_read_interface(mreg_desc->enable_reg
			, &en, MT6311_enable_mask, mreg_desc->enable_bit);
	if (ret < 0) {
		pr_notice(MT6311TAG "[%s] Check (%s) status fail, ret = %d\n",
			__func__, mreg_desc->rdesc.name, ret);
		return ret;
	}

	return en;
}

static int mt6311_set_mode(
		struct mtk_simple_regulator_desc *mreg_desc, unsigned int mode)
{
	int ret;

	switch (mode) {
	case 1: /* force pwm mode */
		ret = mt6311_config_interface(mt6311_proc_mode_reg
				, mode, MT6311_mode_mask, MT6311_mode_bit);
		break;
	case 0: /* auto mode */
		ret = mt6311_config_interface(mt6311_proc_mode_reg
				, mode, MT6311_mode_mask, MT6311_mode_bit);
		break;
	default:
		pr_notice(MT6311TAG "[%s] Set Wrong mode = %d\n"
			  , __func__, mode);
		return -1;
	}
	return ret;
}

static unsigned int mt6311_get_mode(
		struct mtk_simple_regulator_desc *mreg_desc)
{
	unsigned char mode = 0;
	int ret;

	ret = mt6311_read_interface(mt6311_proc_mode_reg
				, &mode, MT6311_mode_mask, MT6311_mode_bit);
	if (ret < 0) {
		pr_notice(MT6311TAG "[%s] read mode fail, ret = %d\n"
				, __func__, ret);
		return 2;
	}

	return mode;
}

static struct mtk_simple_regulator_ext_ops mt6311_regulator_ext_ops = {
	.enable = mt6311_enable,
	.disable = mt6311_disable,
	.is_enabled = mt6311_is_enabled,
	.set_voltage_sel = mt6311_set_voltage_sel,
	.get_voltage_sel = mt6311_get_voltage_sel,
	.set_mode = mt6311_set_mode,
	.get_mode = mt6311_get_mode,
};

static struct regulator_init_data mt6311_buck_init_data[] = {
	{
		.constraints = {
			.name = "ext_buck_proc",
			.min_uV = MT6311_MIN_VOLTAGE,
			.max_uV = MT6311_MAX_VOLTAGE,
			.valid_modes_mask =
				(REGULATOR_MODE_NORMAL|REGULATOR_MODE_FAST),
			.valid_ops_mask =
				REGULATOR_CHANGE_MODE|REGULATOR_CHANGE_VOLTAGE|
				REGULATOR_CHANGE_STATUS,
		},
	},
};

int mt6311_vdvfs11_set_mode(unsigned char mode)
{
	int ret = 0;

#ifdef IPIMB_MT6311
	ret = mt6311_ipi_set_mode(mode);
#else
	ret = mt6311_config_interface(MT6311_PMIC_RG_VDVFS11_MODESET_ADDR, mode,
		MT6311_PMIC_RG_VDVFS11_MODESET_MASK,
		MT6311_PMIC_RG_VDVFS11_MODESET_SHIFT);
#endif

	return ret;
}

int mt6311_regulator_init(struct device *dev)
{
	int ret = 0, i = 0;

	for (i = 0; i < ARRAY_SIZE(mt6311_desc_table); i++) {
		mt6311_desc_table[i].def_init_data =
					&mt6311_buck_init_data[i];
		ret = mtk_simple_regulator_register(&mt6311_desc_table[i],
				dev, &mt6311_regulator_ext_ops, NULL);
		if (ret < 0)
			pr_notice(MT6311TAG "%s register mtk simple regulator fail\n"
				  , __func__);
	}
	return 0;
}

int mt6311_regulator_deinit(void)
{
	int ret = 0, i = 0;

	for (i = 0; i < ARRAY_SIZE(mt6311_desc_table); i++)
		ret |= mtk_simple_regulator_unregister(&mt6311_desc_table[i]);

	return ret;
}
