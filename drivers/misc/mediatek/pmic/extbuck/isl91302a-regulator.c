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

#include "isl91302a-spi.h"
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/mediatek/mtk_regulator_core.h>
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
#include <mach/mtk_pmic_ipi.h>
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
#include <linux/delay.h>

#define ISL91302A_BUCK_MAX	3

struct isl91302a_regulator_info {
	struct mtk_simple_regulator_desc *mreg_desc;
	uint32_t vol_up_reg;
	uint32_t vol_up_mask;
	uint32_t vol_up_shift;
	uint32_t vol_lo_reg;
	uint32_t vol_lo_mask;
	uint32_t vol_lo_shift;
	uint32_t mode_reg;
	uint32_t mode_bit;
	uint32_t ramp_reg;
	uint32_t ramp_mask;
	uint32_t ramp_shift;
	uint32_t ramp_sel;
};

#define ISL91302A_MIN_VOLTAGE	(1200)
#define ISL91302A_MAX_VOLTAGE	(1231000)
#define ISL91302A_RAMP_RATE	(12500)	/* uV/us */

/* buck 1 */
#define isl91302a_gpu_vol_reg		(0)
#define isl91302a_gpu_vol_mask		(0)
#define isl91302a_gpu_vol_shift		(0)
#define isl91302a_gpu_enable_reg	ISL91302A_MODECTRL_R
#define isl91302a_gpu_enable_bit	(0x80)
#define isl91302a_gpu_min_uV		(ISL91302A_MIN_VOLTAGE)
#define isl91302a_gpu_max_uV		(ISL91302A_MAX_VOLTAGE)
#define isl91302a_gpu_id		(0)

/* buck 2 */
#define isl91302a_proc2_vol_reg		(0)
#define isl91302a_proc2_vol_mask	(0)
#define isl91302a_proc2_vol_shift	(0)
#define isl91302a_proc2_enable_reg	ISL91302A_MODECTRL_R
#define isl91302a_proc2_enable_bit	(0x40)
#define isl91302a_proc2_min_uV		(ISL91302A_MIN_VOLTAGE)
#define isl91302a_proc2_max_uV		(ISL91302A_MAX_VOLTAGE)
#define isl91302a_proc2_id		(1)

/* buck 3 */
#define isl91302a_proc1_vol_reg		(0)
#define isl91302a_proc1_vol_mask	(0)
#define isl91302a_proc1_vol_shift	(0)
#define isl91302a_proc1_enable_reg	ISL91302A_MODECTRL_R
#define isl91302a_proc1_enable_bit	(0x20)
#define isl91302a_proc1_min_uV		(ISL91302A_MIN_VOLTAGE)
#define isl91302a_proc1_max_uV		(ISL91302A_MAX_VOLTAGE)
#define isl91302a_proc1_id		(2)

static struct mtk_simple_regulator_control_ops isl91302a_mreg_ctrl_ops = {
	.register_read = isl91302a_read_byte,
	.register_write = isl91302a_write_byte,
	.register_update_bits = isl91302a_assign_bit,
};

static int isl91302a_list_voltage(
		struct mtk_simple_regulator_desc *mreg_desc,
		unsigned int selector)
{
	return (selector >= mreg_desc->rdesc.n_voltages) ?
		-EINVAL : DIV_ROUND_UP(ISL91302A_MAX_VOLTAGE * selector, 1024);
}

static struct mtk_simple_regulator_desc isl91302a_desc_table[] = {
	mreg_decl(isl91302a_gpu, isl91302a_list_voltage,
			1024, &isl91302a_mreg_ctrl_ops),
	mreg_decl(isl91302a_proc2, isl91302a_list_voltage,
			1024, &isl91302a_mreg_ctrl_ops),
	mreg_decl(isl91302a_proc1, isl91302a_list_voltage,
			1024, &isl91302a_mreg_ctrl_ops),
};

#define ISL91302A_REGULATOR_VOLUP_REG0		ISL91302A_BUCK1_UP_R
#define ISL91302A_REGULATOR_VOLUP_REG1		ISL91302A_BUCK2_UP_R
#define ISL91302A_REGULATOR_VOLUP_REG2		ISL91302A_BUCK3_UP_R
#define ISL91302A_REGULATOR_VOLUP_SHIFT		0
#define ISL91302A_REGULATOR_VOLUP_MASK		(0xff)
#define ISL91302A_REGULATOR_VOLLO_REG0		ISL91302A_BUCK1_LO_R
#define ISL91302A_REGULATOR_VOLLO_REG1		ISL91302A_BUCK2_LO_R
#define ISL91302A_REGULATOR_VOLLO_REG2		ISL91302A_BUCK3_LO_R
#define ISL91302A_REGULATOR_VOLLO_SHIFT		6
#define ISL91302A_REGULATOR_VOLLO_MASK		(0xc0)
#define ISL91302A_REGULATOR_MODE_REG0		ISL91302A_BUCK1_DCM_R
#define ISL91302A_REGULATOR_MODE_MASK0		(0x04)
#define ISL91302A_REGULATOR_MODE_REG1		ISL91302A_BUCK2_DCM_R
#define ISL91302A_REGULATOR_MODE_MASK1		(0x04)
#define ISL91302A_REGULATOR_MODE_REG2		ISL91302A_BUCK3_DCM_R
#define ISL91302A_REGULATOR_MODE_MASK2		(0x04)
#define ISL91302A_REGULATOR_RAMP_REG0		ISL91302A_BUCK1_RSPCFG1_R
#define ISL91302A_REGULATOR_RAMP_REG1		ISL91302A_BUCK2_RSPCFG1_R
#define ISL91302A_REGULATOR_RAMP_REG2		ISL91302A_BUCK3_RSPCFG1_R
#define ISL91302A_REGULATOR_RAMP_MASK0		ISL91302A_BUCK_RSPCFG1_RSPUP_M
#define ISL91302A_REGULATOR_RAMP_MASK1		ISL91302A_BUCK_RSPCFG1_RSPUP_M
#define ISL91302A_REGULATOR_RAMP_MASK2		ISL91302A_BUCK_RSPCFG1_RSPUP_M
#define ISL91302A_REGULATOR_RAMP_SHIFT0		ISL91302A_BUCK_RSPCFG1_RSPUP_S
#define ISL91302A_REGULATOR_RAMP_SHIFT1		ISL91302A_BUCK_RSPCFG1_RSPUP_S
#define ISL91302A_REGULATOR_RAMP_SHIFT2		ISL91302A_BUCK_RSPCFG1_RSPUP_S
#define ISL91302A_REGULATOR_RAMP_SEL0		ISL91302A_BUCK_RSPSEL_M
#define ISL91302A_REGULATOR_RAMP_SEL1		ISL91302A_BUCK_RSPSEL_M
#define ISL91302A_REGULATOR_RAMP_SEL2		ISL91302A_BUCK_RSPSEL_M

#define ISL91302A_REGULATOR_DECL(_id)	\
{	\
	.mreg_desc = &isl91302a_desc_table[_id],	\
	.vol_up_reg = ISL91302A_REGULATOR_VOLUP_REG##_id,	\
	.vol_up_shift = ISL91302A_REGULATOR_VOLUP_SHIFT,	\
	.vol_up_mask = ISL91302A_REGULATOR_VOLUP_MASK,		\
	.vol_lo_reg = ISL91302A_REGULATOR_VOLLO_REG##_id,	\
	.vol_lo_shift = ISL91302A_REGULATOR_VOLLO_SHIFT,	\
	.vol_lo_mask = ISL91302A_REGULATOR_VOLLO_MASK,		\
	.mode_reg = ISL91302A_REGULATOR_MODE_REG##_id,		\
	.mode_bit = ISL91302A_REGULATOR_MODE_MASK##_id,		\
	.ramp_reg = ISL91302A_REGULATOR_RAMP_REG##_id,		\
	.ramp_mask = ISL91302A_REGULATOR_RAMP_MASK##_id,	\
	.ramp_shift = ISL91302A_REGULATOR_RAMP_SHIFT##_id,	\
}

static struct isl91302a_regulator_info isl91302a_regulator_infos[] = {
	ISL91302A_REGULATOR_DECL(0), /* GPU */
	ISL91302A_REGULATOR_DECL(1), /* PROC2 */
	ISL91302A_REGULATOR_DECL(2), /* PROC1 */
};

static struct isl91302a_regulator_info *isl91302a_find_regulator_info(int id)
{
	struct isl91302a_regulator_info *info;
	int i;

	for (i = 0; i < ARRAY_SIZE(isl91302a_regulator_infos); i++) {
		info = &isl91302a_regulator_infos[i];
		if (info->mreg_desc->rdesc.id == id)
			return info;
	}
	return NULL;
}

static int isl91302a_set_voltage_sel(
		struct mtk_simple_regulator_desc *mreg_desc,
		unsigned int selector)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	pr_notice("%s not support for sspm\n", __func__);
	return 0;
#else
	struct isl91302a_regulator_info *info =
		isl91302a_find_regulator_info(mreg_desc->rdesc.id);
	uint32_t data = 0;
	int ret;
	const int count = mreg_desc->rdesc.n_voltages;

	if (selector > count)
		return -EINVAL;

	data = (selector>>2)&0xff;
	ret = isl91302a_assign_bit(mreg_desc->client, info->vol_up_reg,
				info->vol_up_mask, data<<info->vol_up_shift);
	data = (selector&0x03);
	ret |= isl91302a_assign_bit(mreg_desc->client, info->vol_lo_reg,
			info->vol_lo_mask, data << info->vol_lo_shift);
	return ret;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
}

static int isl91302a_get_voltage_sel(
			struct mtk_simple_regulator_desc *mreg_desc)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	pr_notice("%s not support for sspm\n", __func__);
	return 0;
#else
	struct isl91302a_regulator_info *info =
		isl91302a_find_regulator_info(mreg_desc->rdesc.id);
	uint32_t regval[2] = {0};
	int ret;

	ret = isl91302a_read_byte(mreg_desc->client,
				info->vol_up_reg, &regval[0]);
	ret |= isl91302a_read_byte(mreg_desc->client,
				info->vol_lo_reg, &regval[1]);
	if (ret < 0)
		return ret;
	ret = (regval[0]<<2) | ((regval[1]>>6)&0x03);
	return ret;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
}

static int isl91302a_set_mode(
		struct mtk_simple_regulator_desc *mreg_desc, unsigned int mode)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	pr_notice("%s not support for sspm\n", __func__);
	return 0;
#else
	struct isl91302a_regulator_info *info =
		isl91302a_find_regulator_info(mreg_desc->rdesc.id);
	int ret;

	switch (mode) {
	case REGULATOR_MODE_FAST: /* force pwm mode */
		ret = isl91302a_set_bit(mreg_desc->client,
			info->mode_reg, info->mode_bit);
		break;
	case REGULATOR_MODE_NORMAL: /* auto mode */
	default:
		ret = isl91302a_clr_bit(mreg_desc->client,
				info->mode_reg, info->mode_bit);
		break;
	}
	return ret;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
}

static unsigned int isl91302a_get_mode(
		struct mtk_simple_regulator_desc *mreg_desc)
{
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	pr_notice("%s not support for sspm\n", __func__);
	return 0;
#else
	struct isl91302a_regulator_info *info =
		isl91302a_find_regulator_info(mreg_desc->rdesc.id);
	int ret;
	uint32_t regval = 0;

	ret = isl91302a_read_byte(mreg_desc->client, info->mode_reg, &regval);
	if (ret < 0) {
		ISL91302A_pr_notice("%s read mode fail\n", __func__);
		return ret;
	}

	if (regval & info->mode_bit)
		return REGULATOR_MODE_FAST;
	return REGULATOR_MODE_NORMAL;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
}

#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
static int isl91302a_enable_ipi(struct mtk_simple_regulator_desc *mreg_desc)
{
	unsigned int ret = 0;
	unsigned char buck_id;

	buck_id = mreg_desc->rdesc.id;
	if (buck_id != 1) {
		pr_notice("%s only support proc2\n", __func__);
		return 0;
	}

	ret = extbuck_ipi_enable(buck_id, 1);
	pr_info_ratelimited("%s [%s] id(%d), ret(%d)\n", __func__,
				mreg_desc->rdesc.name, buck_id, ret);
	dsb(sy);
	mdelay(1);
	dsb(sy);
	if (ret != 0)
		return -EINVAL;
	return 0;
}

static int isl91302a_disable_ipi(struct mtk_simple_regulator_desc *mreg_desc)
{
	unsigned int ret = 0;
	unsigned char buck_id;

	buck_id = mreg_desc->rdesc.id;
	if (buck_id != 1) {
		pr_notice("%s only support proc2\n", __func__);
		return 0;
	}

	ret = extbuck_ipi_enable(buck_id, 0);
	pr_info_ratelimited("%s [%s] id(%d), ret(%d)\n", __func__,
				mreg_desc->rdesc.name, buck_id, ret);
	if (ret != 0)
		return -EINVAL;
	return 0;
}

static int isl91302a_is_enabled_ipi(struct mtk_simple_regulator_desc *mreg_desc)
{
	unsigned int ret = 0;
	unsigned char buck_id;

	buck_id = mreg_desc->rdesc.id;
	if (buck_id != 1) {
		pr_notice("%s only support proc2\n", __func__);
		return 0;
	}

	ret = extbuck_ipi_enable(buck_id, 0xF);
	pr_info_ratelimited("%s [%s] id(%d), ret(%d)\n", __func__,
			mreg_desc->rdesc.name, buck_id, ret);
	if (ret != 0 && ret != 1)
		return -EINVAL;
	return ret;
}
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */

static struct mtk_simple_regulator_ext_ops isl91302a_regulator_ext_ops = {
#ifdef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
	.enable = isl91302a_enable_ipi,
	.disable = isl91302a_disable_ipi,
	.is_enabled = isl91302a_is_enabled_ipi,
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
	.set_voltage_sel = isl91302a_set_voltage_sel,
	.get_voltage_sel = isl91302a_get_voltage_sel,
	.get_mode = isl91302a_get_mode,
	.set_mode = isl91302a_set_mode,
};

static struct regulator_init_data
	isl91302a_buck_init_data[ISL91302A_BUCK_MAX] = {
	{
		.constraints = {
			.name = "ext_buck_gpu",
			.min_uV = ISL91302A_MIN_VOLTAGE,
			.max_uV = ISL91302A_MAX_VOLTAGE,
			.valid_modes_mask =
				(REGULATOR_MODE_NORMAL|REGULATOR_MODE_FAST),
			.valid_ops_mask =
				REGULATOR_CHANGE_MODE|REGULATOR_CHANGE_VOLTAGE|
				REGULATOR_CHANGE_STATUS,
		},
	},
	{
		.constraints = {
			.name = "ext_buck_proc2",
			.min_uV = ISL91302A_MIN_VOLTAGE,
			.max_uV = ISL91302A_MAX_VOLTAGE,
			.valid_modes_mask =
				(REGULATOR_MODE_NORMAL|REGULATOR_MODE_FAST),
			.valid_ops_mask =
				REGULATOR_CHANGE_MODE|REGULATOR_CHANGE_VOLTAGE|
				REGULATOR_CHANGE_STATUS,
			.boot_on = 1,
		},
	},
	{
		.constraints = {
			.name = "ext_buck_proc1",
			.min_uV = ISL91302A_MIN_VOLTAGE,
			.max_uV = ISL91302A_MAX_VOLTAGE,
			.valid_modes_mask =
				(REGULATOR_MODE_NORMAL|REGULATOR_MODE_FAST),
			.valid_ops_mask =
				REGULATOR_CHANGE_MODE|REGULATOR_CHANGE_VOLTAGE|
				REGULATOR_CHANGE_STATUS,
			.always_on = 1,
		},
	},
};

static int isl91302a_set_ramp_val(struct mtk_simple_regulator_desc *mreg_desc,
		unsigned char regval, unsigned char fast_en)
{
	struct isl91302a_regulator_info *info =
		isl91302a_find_regulator_info(mreg_desc->rdesc.id);
	int ret = 0;

	if (fast_en)
		ret = isl91302a_set_bit(mreg_desc->client,
					info->ramp_reg, info->ramp_sel);
	else
		ret = isl91302a_clr_bit(mreg_desc->client,
					info->ramp_reg, info->ramp_sel);
	ret |= isl91302a_assign_bit(mreg_desc->client, info->ramp_reg,
			info->ramp_mask, regval << info->ramp_shift);
	return ret;
}


static int isl91302a_buck_set_ramp_dly(
	struct mtk_simple_regulator_desc *mreg_desc, int ramp_dly)
{
	int ret = 0;

	switch (ramp_dly) {
	default:
		pr_info("%s Invalid ramp delay, set to default 2.5 mV/us\n",
			__func__);
		ret = isl91302a_set_ramp_val(mreg_desc, 0, 0);
		break;
	case 2500:
		ret = isl91302a_set_ramp_val(mreg_desc, 0, 0);
		break;
	case 5000:
		ret = isl91302a_set_ramp_val(mreg_desc, 1, 0);
		break;
	case 10000:
		ret = isl91302a_set_ramp_val(mreg_desc, 0, 1);
		break;
	case 12500:
		ret = isl91302a_set_ramp_val(mreg_desc, 2, 0);
		break;
	case 20000:
		ret = isl91302a_set_ramp_val(mreg_desc, 1, 1);
		break;
	case 25000:
		ret = isl91302a_set_ramp_val(mreg_desc, 3, 0);
		break;
	case 50000:
		ret = isl91302a_set_ramp_val(mreg_desc, 2, 1);
		break;
	case 100000:
		ret = isl91302a_set_ramp_val(mreg_desc, 3, 1);
		break;
	}

	return ret;
}

int isl91302a_regulator_init(struct isl91302a_chip *chip)
{
	int ret = 0, i = 0;

	if (chip == NULL) {
		ISL91302A_pr_notice("%s Null chip info\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(isl91302a_desc_table); i++) {
#ifndef CONFIG_MTK_TINYSYS_SSPM_SUPPORT
		isl91302a_desc_table[i].client = chip->spi;
#endif /* CONFIG_MTK_TINYSYS_SSPM_SUPPORT */
		isl91302a_desc_table[i].def_init_data =
					&isl91302a_buck_init_data[i];
		ret = mtk_simple_regulator_register(&isl91302a_desc_table[i],
				chip->dev, &isl91302a_regulator_ext_ops, NULL);
		if (ret < 0) {
			ISL91302A_pr_notice(
				"%s register mtk simple regulator fail\n"
				, __func__);
		}
		isl91302a_buck_set_ramp_dly(
			&isl91302a_desc_table[i], ISL91302A_RAMP_RATE);
	}
	return 0;
}

int isl91302a_regulator_deinit(struct isl91302a_chip *chip)
{
	int ret = 0, i = 0;

	for (i = 0; i < ARRAY_SIZE(isl91302a_desc_table); i++) {
		ret |= mtk_simple_regulator_unregister(
						&isl91302a_desc_table[i]);
	}
	return ret;
}
