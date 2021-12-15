/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
*/

#include "rt5734-spi.h"
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/mediatek/mtk_regulator_core.h>

#define RT5734_BUCK_DRV_VERSION	"1.0.0_MTK"

#define RT5734_BUCK_MAX	3

struct rt5734_regulator_info {
	struct mtk_simple_regulator_desc *mreg_desc;
	uint32_t mode_reg;
	uint32_t mode_bit;
	unsigned char ramp_up_reg;
	unsigned char ramp_down_reg;
	unsigned char ramp_up_mask;
	unsigned char ramp_up_shift;
	unsigned char ramp_down_mask;
	unsigned char ramp_down_shift;
};

#define RT5734_MIN_VOLTAGE	(300000)
#define RT5734_MAX_VOLTAGE	(1850000)
#define RT5734_RAMP_RATE	(8000)	/* uV/us */

/* buck 1 */
#define rt5734_gpu_vol_reg		(RT5734_BUCK1_R)
#define rt5734_gpu_vol_mask		(0xff)
#define rt5734_gpu_vol_shift		(0)
#define rt5734_gpu_enable_reg		RT5734_BUCK1_MODE_R
#define rt5734_gpu_enable_bit		(0x01)
#define rt5734_gpu_min_uV		(RT5734_MIN_VOLTAGE)
#define rt5734_gpu_max_uV		(RT5734_MAX_VOLTAGE)
#define rt5734_gpu_id			(0)

/* buck 2 */
#define rt5734_proc2_vol_reg		(RT5734_BUCK2_R)
#define rt5734_proc2_vol_mask		(0xff)
#define rt5734_proc2_vol_shift		(0)
#define rt5734_proc2_enable_reg		RT5734_BUCK2_MODE_R
#define rt5734_proc2_enable_bit		(0x01)
#define rt5734_proc2_min_uV		(RT5734_MIN_VOLTAGE)
#define rt5734_proc2_max_uV		(RT5734_MAX_VOLTAGE)
#define rt5734_proc2_id			(1)

/* buck 3 */
#define rt5734_proc1_vol_reg		(RT5734_BUCK3_R)
#define rt5734_proc1_vol_mask		(0xff)
#define rt5734_proc1_vol_shift		(0)
#define rt5734_proc1_enable_reg		RT5734_BUCK3_MODE_R
#define rt5734_proc1_enable_bit		(0x01)
#define rt5734_proc1_min_uV		(RT5734_MIN_VOLTAGE)
#define rt5734_proc1_max_uV		(RT5734_MAX_VOLTAGE)
#define rt5734_proc1_id			(2)

static struct mtk_simple_regulator_control_ops rt5734_mreg_ctrl_ops = {
	.register_read = rt5734_read_byte,
	.register_write = rt5734_write_byte,
	.register_update_bits = rt5734_assign_bit,
};

static int rt5734_list_voltage(
		struct mtk_simple_regulator_desc *mreg_desc,
		unsigned int selector)
{
	unsigned int vout = 0;

	if (selector > 300)
		vout = 1300000 + selector * 10000;
	else
		vout = mreg_desc->min_uV + 5000 * selector;
	if (vout > mreg_desc->max_uV)
		return -EINVAL;
	return vout;
}

static struct mtk_simple_regulator_desc rt5734_desc_table[] = {
	mreg_decl(rt5734_gpu, rt5734_list_voltage,
			256, &rt5734_mreg_ctrl_ops),
	mreg_decl(rt5734_proc2, rt5734_list_voltage,
			256, &rt5734_mreg_ctrl_ops),
	mreg_decl(rt5734_proc1, rt5734_list_voltage,
			256, &rt5734_mreg_ctrl_ops),
};

#define RT5734_REGULATOR_MODE_REG0	RT5734_BUCK1_MODE_R
#define RT5734_REGULATOR_MODE_REG1	RT5734_BUCK2_MODE_R
#define RT5734_REGULATOR_MODE_REG2	RT5734_BUCK3_MODE_R
#define RT5734_REGULATOR_MODE_MASK0	(0x20)
#define RT5734_REGULATOR_MODE_MASK1	(0x20)
#define RT5734_REGULATOR_MODE_MASK2	(0x20)
#define RT5734_REGULATOR_RAMP_UP_REG0	RT5734_BUCK1_RSPCFG1_R
#define RT5734_REGULATOR_RAMP_UP_REG1	RT5734_BUCK2_RSPCFG1_R
#define RT5734_REGULATOR_RAMP_UP_REG2	RT5734_BUCK3_RSPCFG1_R
#define RT5734_REGULATOR_RAMP_DOWN_REG0	RT5734_BUCK1_RSPCFG1_R
#define RT5734_REGULATOR_RAMP_DOWN_REG1	RT5734_BUCK2_RSPCFG1_R
#define RT5734_REGULATOR_RAMP_DOWN_REG2	RT5734_BUCK3_RSPCFG1_R

#define RT5734_REGULATOR_DECL(_id)	\
{	\
	.mreg_desc = &rt5734_desc_table[_id],	\
	.mode_reg = RT5734_REGULATOR_MODE_REG##_id,		\
	.mode_bit = RT5734_REGULATOR_MODE_MASK##_id,		\
	.ramp_up_reg = RT5734_REGULATOR_RAMP_UP_REG##_id,	\
	.ramp_down_reg = RT5734_REGULATOR_RAMP_DOWN_REG##_id,	\
	.ramp_up_mask = (0x70),					\
	.ramp_down_mask = (0x07),				\
	.ramp_up_shift = 4,					\
	.ramp_down_shift = 0,					\
}

static struct rt5734_regulator_info rt5734_regulator_infos[] = {
	RT5734_REGULATOR_DECL(0), /* GPU */
	RT5734_REGULATOR_DECL(1), /* PROC2 */
	RT5734_REGULATOR_DECL(2), /* PROC1 */
};

static struct rt5734_regulator_info *rt5734_find_regulator_info(int id)
{
	struct rt5734_regulator_info *info;
	int i;

	for (i = 0; i < ARRAY_SIZE(rt5734_regulator_infos); i++) {
		info = &rt5734_regulator_infos[i];
		if (info->mreg_desc->rdesc.id == id)
			return info;
	}
	return NULL;
}

static int rt5734_set_mode(
		struct mtk_simple_regulator_desc *mreg_desc, unsigned int mode)
{
	struct rt5734_regulator_info *info =
		rt5734_find_regulator_info(mreg_desc->rdesc.id);
	int ret;

	switch (mode) {
	case REGULATOR_MODE_FAST: /* force pwm mode */
		ret = rt5734_set_bit(mreg_desc->client,
			info->mode_reg, info->mode_bit);
		break;
	case REGULATOR_MODE_NORMAL: /* auto mode */
	default:
		ret = rt5734_clr_bit(mreg_desc->client,
				info->mode_reg, info->mode_bit);
		break;
	}
	return ret;
}

static unsigned int rt5734_get_mode(
		struct mtk_simple_regulator_desc *mreg_desc)
{
	struct rt5734_regulator_info *info =
		rt5734_find_regulator_info(mreg_desc->rdesc.id);
	int ret;
	uint32_t regval = 0;

	ret = rt5734_read_byte(mreg_desc->client, info->mode_reg, &regval);
	if (ret < 0) {
		RT5734_pr_notice("%s read mode fail\n", __func__);
		return ret;
	}

	if (regval & info->mode_bit)
		return REGULATOR_MODE_FAST;
	return REGULATOR_MODE_NORMAL;
}

static struct mtk_simple_regulator_ext_ops rt5734_regulator_ext_ops = {
	.get_mode = rt5734_get_mode,
	.set_mode = rt5734_set_mode,
};

static struct regulator_init_data
	rt5734_buck_init_data[RT5734_BUCK_MAX] = {
	{
		.constraints = {
			.name = "ext_buck_gpu",
			.min_uV = RT5734_MIN_VOLTAGE,
			.max_uV = RT5734_MAX_VOLTAGE,
			.valid_modes_mask =
				(REGULATOR_MODE_NORMAL|REGULATOR_MODE_FAST),
			.valid_ops_mask =
				REGULATOR_CHANGE_MODE|REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
		},
	},
	{
		.constraints = {
			.name = "ext_buck_proc2",
			.min_uV = RT5734_MIN_VOLTAGE,
			.max_uV = RT5734_MAX_VOLTAGE,
			.valid_modes_mask =
				(REGULATOR_MODE_NORMAL|REGULATOR_MODE_FAST),
			.valid_ops_mask =
				REGULATOR_CHANGE_MODE|REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
		},
	},
	{
		.constraints = {
			.name = "ext_buck_proc1",
			.min_uV = RT5734_MIN_VOLTAGE,
			.max_uV = RT5734_MAX_VOLTAGE,
			.valid_modes_mask =
				(REGULATOR_MODE_NORMAL|REGULATOR_MODE_FAST),
			.valid_ops_mask =
				REGULATOR_CHANGE_MODE|REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
		},
	},
};

static int rt5734_set_ramp_dly(struct mtk_simple_regulator_desc *mreg_desc,
								int ramp_dly)
{
	struct rt5734_regulator_info *info =
		rt5734_find_regulator_info(mreg_desc->rdesc.id);
	int ret = 0;
	unsigned char regval;

	if (ramp_dly > 63)
		regval = 63;
	else
		regval = ramp_dly;

	ret = rt5734_assign_bit(mreg_desc->client, info->ramp_up_reg,
			info->ramp_up_mask, regval << info->ramp_up_shift);
	ret = rt5734_assign_bit(mreg_desc->client, info->ramp_down_reg,
			info->ramp_down_mask, regval << info->ramp_down_shift);

	return ret;
}

int rt5734_regulator_init(struct rt5734_chip *chip)
{
	int ret = 0, i = 0;

	if (chip == NULL) {
		RT5734_pr_notice("%s Null chip info\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(rt5734_desc_table); i++) {
		rt5734_desc_table[i].client = chip->spi;
		rt5734_desc_table[i].def_init_data =
					&rt5734_buck_init_data[i];
		ret = mtk_simple_regulator_register(&rt5734_desc_table[i],
				chip->dev, &rt5734_regulator_ext_ops, NULL);
		if (ret < 0) {
			RT5734_pr_notice(
				"%s register mtk simple regulator fail\n"
				, __func__);
		}
		ret = rt5734_set_ramp_dly(
			&rt5734_desc_table[i], RT5734_RAMP_RATE);
		if (ret < 0) {
			RT5734_pr_notice("%s (%s)set ramp dly fail\n", __func__,
				rt5734_desc_table[i].rdesc.name);
		}
	}
	pr_info("%s Successfully\n", __func__);
	return 0;
}

int rt5734_regulator_deinit(struct rt5734_chip *chip)
{
	int ret = 0, i = 0;

	for (i = 0; i < ARRAY_SIZE(rt5734_desc_table); i++) {
		ret |= mtk_simple_regulator_unregister(
						&rt5734_desc_table[i]);
	}
	return ret;
}
