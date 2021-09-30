// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.

#include <linux/interrupt.h>
#include <linux/mfd/mt6373/registers.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6373-regulator.h>
#include <linux/regulator/of_regulator.h>

#define SET_OFFSET	0x1
#define CLR_OFFSET	0x2

#define MT6373_REGULATOR_MODE_NORMAL	0
#define MT6373_REGULATOR_MODE_FCCM	1
#define MT6373_REGULATOR_MODE_LP	2
#define MT6373_REGULATOR_MODE_ULP	3

#define DEFAULT_DELAY_MS		10

#define MT6373_PLG_CFG_ELR1		0x3ab
#define MT6373_ELR_MASK			0xc

/*
 * MT6373 regulators' information
 *
 * @desc: standard fields of regulator description.
 * @lp_mode_reg: for operating NORMAL/IDLE mode register.
 * @lp_mode_mask: MASK for operating lp_mode register.
 * @modeset_reg: for operating AUTO/PWM mode register.
 * @modeset_mask: MASK for operating modeset register.
 * @modeset_reg: Calibrates output voltage register.
 * @modeset_mask: MASK of Calibrates output voltage register.
 */
struct mt6373_regulator_info {
	int irq;
	int oc_irq_enable_delay_ms;
	struct delayed_work oc_work;
	struct regulator_desc desc;
	u32 lp_mode_reg;
	u32 lp_mode_mask;
	u32 modeset_reg;
	u32 modeset_mask;
	u32 vocal_reg;
	u32 vocal_mask;
};

#define MT6373_BUCK(_name, min, max, step, volt_ranges,		\
		    _enable_reg, en_bit, _vsel_reg, _vsel_mask, \
		    _lp_mode_reg, lp_bit,			\
		    _modeset_reg, modeset_bit)			\
[MT6373_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6373_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6373_buck_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6373_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.linear_ranges = volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6373_map_mode,			\
	},							\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
	.modeset_reg = _modeset_reg,				\
	.modeset_mask = BIT(modeset_bit),			\
}

#define MT6373_LDO_LINEAR(_name, min, max, step, volt_ranges,	\
			  _enable_reg, en_bit, _vsel_reg,	\
			  _vsel_mask, _lp_mode_reg, lp_bit)	\
[MT6373_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6373_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6373_volt_range_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6373_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.linear_ranges = volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6373_map_mode,			\
	},							\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
}

#define MT6373_LDO(_name, _volt_table, _enable_reg, en_bit,	\
		   _vsel_reg, _vsel_mask, _vocal_reg,		\
		   _vocal_mask, _lp_mode_reg, lp_bit)		\
[MT6373_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6373_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6373_volt_table_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6373_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ARRAY_SIZE(_volt_table),		\
		.volt_table = _volt_table,			\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6373_map_mode,			\
	},							\
	.vocal_reg = _vocal_reg,				\
	.vocal_mask = _vocal_mask,				\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
}

#define MT6373_VMCH_EINT(_eint_pol, _volt_table)		\
[MT6373_ID_VMCH_##_eint_pol] = {				\
	.desc = {						\
		.name = "VMCH_"#_eint_pol,			\
		.of_match = of_match_ptr("VMCH_"#_eint_pol),	\
		.of_parse_cb = mt6373_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6373_vmch_eint_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6373_ID_VMCH_##_eint_pol,		\
		.owner = THIS_MODULE,				\
		.n_voltages = ARRAY_SIZE(_volt_table),		\
		.volt_table = _volt_table,			\
		.enable_reg = MT6373_LDO_VMCH_EINT,	\
		.enable_mask = MT6373_PMIC_RG_LDO_VMCH_EINT_EN_MASK,	\
		.vsel_reg = MT6373_PMIC_RG_VMCH_VOSEL_ADDR,	\
		.vsel_mask = MT6373_PMIC_RG_VMCH_VOSEL_MASK,	\
		.of_map_mode = mt6373_map_mode,			\
	},							\
	.vocal_reg = MT6373_PMIC_RG_VMCH_VOCAL_ADDR,		\
	.vocal_mask = MT6373_PMIC_RG_VMCH_VOCAL_MASK,		\
	.lp_mode_reg = MT6373_PMIC_RG_LDO_VMCH_LP_ADDR,		\
	.lp_mode_mask = BIT(MT6373_PMIC_RG_LDO_VMCH_LP_SHIFT),	\
}

static const struct linear_range mt_volt_range0[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 191, 6250),
};

static const struct linear_range mt_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 191, 13875),
};

static const struct linear_range mt_volt_range2[] = {
	REGULATOR_LINEAR_RANGE(400000, 0, 127, 6250),
};

static const unsigned int ldo_volt_table1[] = {
	1200000, 1300000, 1500000, 1700000, 1800000, 2000000, 2100000, 2200000,
	2700000, 2800000, 2900000, 3000000, 3100000, 3300000, 3400000, 3500000,
};

static const unsigned int ldo_volt_table2[] = {
	1800000, 1900000, 2000000, 2100000, 2200000, 2300000, 2400000, 2500000,
	2600000, 2700000, 2800000, 2900000, 3000000, 3100000, 3200000, 3300000,
};

static const unsigned int ldo_volt_table3[] = {
	600000, 700000, 800000, 900000, 1000000, 1100000, 1200000, 1300000,
	1400000, 1500000, 1600000, 1700000, 1800000, 1900000, 2000000, 2100000,
};

static const unsigned int ldo_volt_table4[] = {
	1200000, 1300000, 1500000, 1700000, 1800000, 2000000, 2500000, 2600000,
	2700000, 2800000, 2900000, 3000000, 3100000, 3300000, 3400000, 3500000,
};

static const unsigned int ldo_volt_table5[] = {
	900000, 1000000, 1100000, 1200000, 1300000, 1700000, 1800000, 1810000,
};

static int mt6373_buck_enable(struct regulator_dev *rdev)
{
	return regmap_write(rdev->regmap, rdev->desc->enable_reg + SET_OFFSET,
			    rdev->desc->enable_mask);
}

static int mt6373_buck_disable(struct regulator_dev *rdev)
{
	return regmap_write(rdev->regmap, rdev->desc->enable_reg + CLR_OFFSET,
			    rdev->desc->enable_mask);
}

static inline unsigned int mt6373_map_mode(unsigned int mode)
{
	switch (mode) {
	case MT6373_REGULATOR_MODE_NORMAL:
		return REGULATOR_MODE_NORMAL;
	case MT6373_REGULATOR_MODE_FCCM:
		return REGULATOR_MODE_FAST;
	case MT6373_REGULATOR_MODE_LP:
		return REGULATOR_MODE_IDLE;
	case MT6373_REGULATOR_MODE_ULP:
		return REGULATOR_MODE_STANDBY;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static unsigned int mt6373_regulator_get_mode(struct regulator_dev *rdev)
{
	struct mt6373_regulator_info *info = rdev_get_drvdata(rdev);
	unsigned int val = 0;
	int ret;

	ret = regmap_read(rdev->regmap, info->modeset_reg, &val);
	if (ret) {
		dev_err(&rdev->dev, "Failed to get mt6373 mode: %d\n", ret);
		return ret;
	}

	if (val & info->modeset_mask)
		return REGULATOR_MODE_FAST;

	ret = regmap_read(rdev->regmap, info->lp_mode_reg, &val);
	if (ret) {
		dev_err(&rdev->dev,
			"Failed to get mt6373 lp mode: %d\n", ret);
		return ret;
	}

	if (val & info->lp_mode_mask)
		return REGULATOR_MODE_IDLE;
	else
		return REGULATOR_MODE_NORMAL;
}

static int mt6373_buck_unlock(struct regmap *map, bool unlock)
{
	u8 buf[2];

	if (unlock) {
		buf[0] = 0x43;
		buf[1] = 0x55;
	} else
		buf[0] = buf[1] = 0;
	return regmap_bulk_write(map, MT6373_BUCK_TOP_KEY_PROT_LO, buf, 2);
}

static int mt6373_regulator_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct mt6373_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0;
	int curr_mode;

	curr_mode = mt6373_regulator_get_mode(rdev);
	switch (mode) {
	case REGULATOR_MODE_FAST:
		ret = mt6373_buck_unlock(rdev->regmap, true);
		if (ret)
			return ret;
		ret = regmap_update_bits(rdev->regmap,
					 info->modeset_reg,
					 info->modeset_mask,
					 info->modeset_mask);
		ret |= mt6373_buck_unlock(rdev->regmap, false);
		break;
	case REGULATOR_MODE_NORMAL:
		if (curr_mode == REGULATOR_MODE_FAST) {
			ret = mt6373_buck_unlock(rdev->regmap, true);
			if (ret)
				return ret;
			ret = regmap_update_bits(rdev->regmap,
						 info->modeset_reg,
						 info->modeset_mask,
						 0);
			ret |= mt6373_buck_unlock(rdev->regmap, false);
		} else if (curr_mode == REGULATOR_MODE_IDLE) {
			ret = regmap_update_bits(rdev->regmap,
						 info->lp_mode_reg,
						 info->lp_mode_mask,
						 0);
			udelay(100);
		}
		break;
	case REGULATOR_MODE_IDLE:
		ret = regmap_update_bits(rdev->regmap,
					 info->lp_mode_reg,
					 info->lp_mode_mask,
					 info->lp_mode_mask);
		break;
	default:
		return -EINVAL;
	}

	if (ret) {
		dev_err(&rdev->dev,
			"Failed to set mt6373 mode(%d): %d\n", mode, ret);
	}
	return ret;
}

static int mt6373_vmch_eint_enable(struct regulator_dev *rdev)
{
	unsigned int val;
	int ret;

	if (rdev->desc->id == MT6373_ID_VMCH_EINT_HIGH)
		val = MT6373_PMIC_RG_LDO_VMCH_EINT_POL_MASK;
	else
		val = 0;
	ret = regmap_update_bits(rdev->regmap, MT6373_LDO_VMCH_EINT,
				 MT6373_PMIC_RG_LDO_VMCH_EINT_POL_MASK, val);
	if (ret)
		return ret;

	ret = regmap_update_bits(rdev->regmap, MT6373_PMIC_RG_LDO_VMCH_EN_ADDR,
				 BIT(MT6373_PMIC_RG_LDO_VMCH_EN_SHIFT),
				 BIT(MT6373_PMIC_RG_LDO_VMCH_EN_SHIFT));
	if (ret)
		return ret;

	ret = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				 rdev->desc->enable_mask, rdev->desc->enable_mask);
	return ret;
}

static int mt6373_vmch_eint_disable(struct regulator_dev *rdev)
{
	int ret;

	ret = regmap_update_bits(rdev->regmap, MT6373_PMIC_RG_LDO_VMCH_EN_ADDR,
				 BIT(MT6373_PMIC_RG_LDO_VMCH_EN_SHIFT), 0);
	if (ret)
		return ret;

	udelay(1500); /* Must delay for VMCH discharging */
	ret = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				 rdev->desc->enable_mask, 0);
	return ret;
}

static const struct regulator_ops mt6373_buck_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6373_buck_enable,
	.disable = mt6373_buck_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6373_regulator_set_mode,
	.get_mode = mt6373_regulator_get_mode,
};

static const struct regulator_ops mt6373_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6373_regulator_set_mode,
	.get_mode = mt6373_regulator_get_mode,
};

static const struct regulator_ops mt6373_volt_table_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6373_regulator_set_mode,
	.get_mode = mt6373_regulator_get_mode,
};

static const struct regulator_ops mt6373_vmch_eint_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6373_vmch_eint_enable,
	.disable = mt6373_vmch_eint_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6373_regulator_set_mode,
	.get_mode = mt6373_regulator_get_mode,
};

static int mt6373_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config);

/* The array is indexed by id(MT6373_ID_XXX) */
static struct mt6373_regulator_info mt6373_regulators[] = {
	MT6373_BUCK(VBUCK0, 0, 1193750, 6250, mt_volt_range0,
		    MT6373_PMIC_RG_BUCK_VBUCK0_EN_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK0_EN_SHIFT,
		    MT6373_PMIC_RG_BUCK_VBUCK0_VOSEL_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK0_VOSEL_MASK,
		    MT6373_PMIC_RG_BUCK_VBUCK0_LP_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK0_LP_SHIFT,
		    MT6373_PMIC_RG_VBUCK0_FCCM_ADDR,
		    MT6373_PMIC_RG_VBUCK0_FCCM_SHIFT),
	MT6373_BUCK(VBUCK1, 0, 1193750, 6250, mt_volt_range0,
		    MT6373_PMIC_RG_BUCK_VBUCK1_EN_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK1_EN_SHIFT,
		    MT6373_PMIC_RG_BUCK_VBUCK1_VOSEL_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK1_VOSEL_MASK,
		    MT6373_PMIC_RG_BUCK_VBUCK1_LP_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK1_LP_SHIFT,
		    MT6373_PMIC_RG_VBUCK1_FCCM_ADDR,
		    MT6373_PMIC_RG_VBUCK1_FCCM_SHIFT),
	MT6373_BUCK(VBUCK2, 0, 1193750, 6250, mt_volt_range0,
		    MT6373_PMIC_RG_BUCK_VBUCK2_EN_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK2_EN_SHIFT,
		    MT6373_PMIC_RG_BUCK_VBUCK2_VOSEL_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK2_VOSEL_MASK,
		    MT6373_PMIC_RG_BUCK_VBUCK2_LP_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK2_LP_SHIFT,
		    MT6373_PMIC_RG_VBUCK2_FCCM_ADDR,
		    MT6373_PMIC_RG_VBUCK2_FCCM_SHIFT),
	MT6373_BUCK(VBUCK3, 0, 1193750, 6250, mt_volt_range0,
		    MT6373_PMIC_RG_BUCK_VBUCK3_EN_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK3_EN_SHIFT,
		    MT6373_PMIC_RG_BUCK_VBUCK3_VOSEL_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK3_VOSEL_MASK,
		    MT6373_PMIC_RG_BUCK_VBUCK3_LP_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK3_LP_SHIFT,
		    MT6373_PMIC_RG_VBUCK3_FCCM_ADDR,
		    MT6373_PMIC_RG_VBUCK3_FCCM_SHIFT),
	MT6373_BUCK(VBUCK4, 0, 2650125, 13875, mt_volt_range1,
		    MT6373_PMIC_RG_BUCK_VBUCK4_EN_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK4_EN_SHIFT,
		    MT6373_PMIC_RG_BUCK_VBUCK4_VOSEL_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK4_VOSEL_MASK,
		    MT6373_PMIC_RG_BUCK_VBUCK4_LP_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK4_LP_SHIFT,
		    MT6373_PMIC_RG_VBUCK4_FCCM_ADDR,
		    MT6373_PMIC_RG_VBUCK4_FCCM_SHIFT),
	MT6373_BUCK(VBUCK5, 0, 1193750, 6250, mt_volt_range0,
		    MT6373_PMIC_RG_BUCK_VBUCK5_EN_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK5_EN_SHIFT,
		    MT6373_PMIC_RG_BUCK_VBUCK5_VOSEL_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK5_VOSEL_MASK,
		    MT6373_PMIC_RG_BUCK_VBUCK5_LP_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK5_LP_SHIFT,
		    MT6373_PMIC_RG_VBUCK5_FCCM_ADDR,
		    MT6373_PMIC_RG_VBUCK5_FCCM_SHIFT),
	MT6373_BUCK(VBUCK6, 0, 1193750, 6250, mt_volt_range0,
		    MT6373_PMIC_RG_BUCK_VBUCK6_EN_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK6_EN_SHIFT,
		    MT6373_PMIC_RG_BUCK_VBUCK6_VOSEL_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK6_VOSEL_MASK,
		    MT6373_PMIC_RG_BUCK_VBUCK6_LP_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK6_LP_SHIFT,
		    MT6373_PMIC_RG_VBUCK6_FCCM_ADDR,
		    MT6373_PMIC_RG_VBUCK6_FCCM_SHIFT),
	MT6373_BUCK(VBUCK7, 0, 1193750, 6250, mt_volt_range0,
		    MT6373_PMIC_RG_BUCK_VBUCK7_EN_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK7_EN_SHIFT,
		    MT6373_PMIC_RG_BUCK_VBUCK7_VOSEL_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK7_VOSEL_MASK,
		    MT6373_PMIC_RG_BUCK_VBUCK7_LP_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK7_LP_SHIFT,
		    MT6373_PMIC_RG_VBUCK7_FCCM_ADDR,
		    MT6373_PMIC_RG_VBUCK7_FCCM_SHIFT),
	MT6373_BUCK(VBUCK8, 0, 1193750, 6250, mt_volt_range0,
		    MT6373_PMIC_RG_BUCK_VBUCK8_EN_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK8_EN_SHIFT,
		    MT6373_PMIC_RG_BUCK_VBUCK8_VOSEL_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK8_VOSEL_MASK,
		    MT6373_PMIC_RG_BUCK_VBUCK8_LP_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK8_LP_SHIFT,
		    MT6373_PMIC_RG_VBUCK8_FCCM_ADDR,
		    MT6373_PMIC_RG_VBUCK8_FCCM_SHIFT),
	MT6373_BUCK(VBUCK9, 0, 1193750, 6250, mt_volt_range0,
		    MT6373_PMIC_RG_BUCK_VBUCK9_EN_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK9_EN_SHIFT,
		    MT6373_PMIC_RG_BUCK_VBUCK9_VOSEL_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK9_VOSEL_MASK,
		    MT6373_PMIC_RG_BUCK_VBUCK9_LP_ADDR,
		    MT6373_PMIC_RG_BUCK_VBUCK9_LP_SHIFT,
		    MT6373_PMIC_RG_VBUCK9_FCCM_ADDR,
		    MT6373_PMIC_RG_VBUCK9_FCCM_SHIFT),
	MT6373_LDO_LINEAR(VSRAM_DIGRF_AIF, 400000, 1193750, 6250, mt_volt_range2,
			  MT6373_PMIC_RG_LDO_VSRAM_DIGRF_AIF_EN_ADDR,
			  MT6373_PMIC_RG_LDO_VSRAM_DIGRF_AIF_EN_SHIFT,
			  MT6373_PMIC_RG_LDO_VSRAM_DIGRF_AIF_VOSEL_ADDR,
			  MT6373_PMIC_RG_LDO_VSRAM_DIGRF_AIF_VOSEL_MASK,
			  MT6373_PMIC_RG_LDO_VSRAM_DIGRF_AIF_LP_ADDR,
			  MT6373_PMIC_RG_LDO_VSRAM_DIGRF_AIF_LP_SHIFT),
	MT6373_LDO(VUSB, ldo_volt_table1,
		   MT6373_PMIC_RG_LDO_VUSB_EN_ADDR, MT6373_PMIC_RG_LDO_VUSB_EN_SHIFT,
		   MT6373_PMIC_RG_VUSB_VOSEL_ADDR,
		   MT6373_PMIC_RG_VUSB_VOSEL_MASK,
		   MT6373_PMIC_RG_VUSB_VOCAL_ADDR,
		   MT6373_PMIC_RG_VUSB_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VUSB_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VUSB_LP_SHIFT),
	MT6373_LDO(VAUX18, ldo_volt_table2,
		   MT6373_PMIC_RG_LDO_VAUX18_EN_ADDR, MT6373_PMIC_RG_LDO_VAUX18_EN_SHIFT,
		   MT6373_PMIC_RG_VAUX18_VOSEL_ADDR,
		   MT6373_PMIC_RG_VAUX18_VOSEL_MASK,
		   MT6373_PMIC_RG_VAUX18_VOCAL_ADDR,
		   MT6373_PMIC_RG_VAUX18_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VAUX18_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VAUX18_LP_SHIFT),
	MT6373_LDO(VRF13_AIF, ldo_volt_table3,
		   MT6373_PMIC_RG_LDO_VRF13_AIF_EN_ADDR, MT6373_PMIC_RG_LDO_VRF13_AIF_EN_SHIFT,
		   MT6373_PMIC_RG_VRF13_AIF_VOSEL_ADDR,
		   MT6373_PMIC_RG_VRF13_AIF_VOSEL_MASK,
		   MT6373_PMIC_RG_VRF13_AIF_VOCAL_ADDR,
		   MT6373_PMIC_RG_VRF13_AIF_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VRF13_AIF_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VRF13_AIF_LP_SHIFT),
	MT6373_LDO(VRF18_AIF, ldo_volt_table3,
		   MT6373_PMIC_RG_LDO_VRF18_AIF_EN_ADDR, MT6373_PMIC_RG_LDO_VRF18_AIF_EN_SHIFT,
		   MT6373_PMIC_RG_VRF18_AIF_VOSEL_ADDR,
		   MT6373_PMIC_RG_VRF18_AIF_VOSEL_MASK,
		   MT6373_PMIC_RG_VRF18_AIF_VOCAL_ADDR,
		   MT6373_PMIC_RG_VRF18_AIF_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VRF18_AIF_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VRF18_AIF_LP_SHIFT),
	MT6373_LDO(VRFIO18_AIF, ldo_volt_table3,
		   MT6373_PMIC_RG_LDO_VRFIO18_AIF_EN_ADDR, MT6373_PMIC_RG_LDO_VRFIO18_AIF_EN_SHIFT,
		   MT6373_PMIC_RG_VRFIO18_AIF_VOSEL_ADDR,
		   MT6373_PMIC_RG_VRFIO18_AIF_VOSEL_MASK,
		   MT6373_PMIC_RG_VRFIO18_AIF_VOCAL_ADDR,
		   MT6373_PMIC_RG_VRFIO18_AIF_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VRFIO18_AIF_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VRFIO18_AIF_LP_SHIFT),
	MT6373_LDO(VRF09_AIF, ldo_volt_table3,
		   MT6373_PMIC_RG_LDO_VRF09_AIF_EN_ADDR, MT6373_PMIC_RG_LDO_VRF09_AIF_EN_SHIFT,
		   MT6373_PMIC_RG_VRF09_AIF_VOSEL_ADDR,
		   MT6373_PMIC_RG_VRF09_AIF_VOSEL_MASK,
		   MT6373_PMIC_RG_VRF09_AIF_VOCAL_ADDR,
		   MT6373_PMIC_RG_VRF09_AIF_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VRF09_AIF_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VRF09_AIF_LP_SHIFT),
	MT6373_LDO(VRF12_AIF, ldo_volt_table5,
		   MT6373_PMIC_RG_LDO_VRF12_AIF_EN_ADDR, MT6373_PMIC_RG_LDO_VRF12_AIF_EN_SHIFT,
		   MT6373_PMIC_RG_VRF12_AIF_VOSEL_ADDR,
		   MT6373_PMIC_RG_VRF12_AIF_VOSEL_MASK,
		   MT6373_PMIC_RG_VRF12_AIF_VOCAL_ADDR,
		   MT6373_PMIC_RG_VRF12_AIF_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VRF12_AIF_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VRF12_AIF_LP_SHIFT),
	MT6373_LDO(VANT18, ldo_volt_table3,
		   MT6373_PMIC_RG_LDO_VANT18_EN_ADDR, MT6373_PMIC_RG_LDO_VANT18_EN_SHIFT,
		   MT6373_PMIC_RG_VANT18_VOSEL_ADDR,
		   MT6373_PMIC_RG_VANT18_VOSEL_MASK,
		   MT6373_PMIC_RG_VANT18_VOCAL_ADDR,
		   MT6373_PMIC_RG_VANT18_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VANT18_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VANT18_LP_SHIFT),
	MT6373_LDO(VIBR, ldo_volt_table1,
		   MT6373_PMIC_RG_LDO_VIBR_EN_ADDR, MT6373_PMIC_RG_LDO_VIBR_EN_SHIFT,
		   MT6373_PMIC_RG_VIBR_VOSEL_ADDR,
		   MT6373_PMIC_RG_VIBR_VOSEL_MASK,
		   MT6373_PMIC_RG_VIBR_VOCAL_ADDR,
		   MT6373_PMIC_RG_VIBR_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VIBR_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VIBR_LP_SHIFT),
	MT6373_LDO(VIO28, ldo_volt_table1,
		   MT6373_PMIC_RG_LDO_VIO28_EN_ADDR, MT6373_PMIC_RG_LDO_VIO28_EN_SHIFT,
		   MT6373_PMIC_RG_VIO28_VOSEL_ADDR,
		   MT6373_PMIC_RG_VIO28_VOSEL_MASK,
		   MT6373_PMIC_RG_VIO28_VOCAL_ADDR,
		   MT6373_PMIC_RG_VIO28_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VIO28_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VIO28_LP_SHIFT),
	MT6373_LDO(VFP, ldo_volt_table1,
		   MT6373_PMIC_RG_LDO_VFP_EN_ADDR, MT6373_PMIC_RG_LDO_VFP_EN_SHIFT,
		   MT6373_PMIC_RG_VFP_VOSEL_ADDR,
		   MT6373_PMIC_RG_VFP_VOSEL_MASK,
		   MT6373_PMIC_RG_VFP_VOCAL_ADDR,
		   MT6373_PMIC_RG_VFP_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VFP_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VFP_LP_SHIFT),
	MT6373_LDO(VTP, ldo_volt_table1,
		   MT6373_PMIC_RG_LDO_VTP_EN_ADDR, MT6373_PMIC_RG_LDO_VTP_EN_SHIFT,
		   MT6373_PMIC_RG_VTP_VOSEL_ADDR,
		   MT6373_PMIC_RG_VTP_VOSEL_MASK,
		   MT6373_PMIC_RG_VTP_VOCAL_ADDR,
		   MT6373_PMIC_RG_VTP_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VTP_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VTP_LP_SHIFT),
	MT6373_LDO(VMCH, ldo_volt_table4,
		   MT6373_PMIC_RG_LDO_VMCH_EN_ADDR, MT6373_PMIC_RG_LDO_VMCH_EN_SHIFT,
		   MT6373_PMIC_RG_VMCH_VOSEL_ADDR,
		   MT6373_PMIC_RG_VMCH_VOSEL_MASK,
		   MT6373_PMIC_RG_VMCH_VOCAL_ADDR,
		   MT6373_PMIC_RG_VMCH_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VMCH_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VMCH_LP_SHIFT),
	MT6373_LDO(VMC, ldo_volt_table1,
		   MT6373_PMIC_RG_LDO_VMC_EN_ADDR, MT6373_PMIC_RG_LDO_VMC_EN_SHIFT,
		   MT6373_PMIC_RG_VMC_VOSEL_ADDR,
		   MT6373_PMIC_RG_VMC_VOSEL_MASK,
		   MT6373_PMIC_RG_VMC_VOCAL_ADDR,
		   MT6373_PMIC_RG_VMC_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VMC_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VMC_LP_SHIFT),
	MT6373_LDO(VAUD18, ldo_volt_table3,
		   MT6373_PMIC_RG_LDO_VAUD18_EN_ADDR, MT6373_PMIC_RG_LDO_VAUD18_EN_SHIFT,
		   MT6373_PMIC_RG_VAUD18_VOSEL_ADDR,
		   MT6373_PMIC_RG_VAUD18_VOSEL_MASK,
		   MT6373_PMIC_RG_VAUD18_VOCAL_ADDR,
		   MT6373_PMIC_RG_VAUD18_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VAUD18_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VAUD18_LP_SHIFT),
	MT6373_LDO(VCN33_1, ldo_volt_table4,
		   MT6373_PMIC_RG_LDO_VCN33_1_EN_ADDR, MT6373_PMIC_RG_LDO_VCN33_1_EN_SHIFT,
		   MT6373_PMIC_RG_VCN33_1_VOSEL_ADDR,
		   MT6373_PMIC_RG_VCN33_1_VOSEL_MASK,
		   MT6373_PMIC_RG_VCN33_1_VOCAL_ADDR,
		   MT6373_PMIC_RG_VCN33_1_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VCN33_1_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VCN33_1_LP_SHIFT),
	MT6373_LDO(VCN33_2, ldo_volt_table4,
		   MT6373_PMIC_RG_LDO_VCN33_2_EN_ADDR, MT6373_PMIC_RG_LDO_VCN33_2_EN_SHIFT,
		   MT6373_PMIC_RG_VCN33_2_VOSEL_ADDR,
		   MT6373_PMIC_RG_VCN33_2_VOSEL_MASK,
		   MT6373_PMIC_RG_VCN33_2_VOCAL_ADDR,
		   MT6373_PMIC_RG_VCN33_2_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VCN33_2_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VCN33_2_LP_SHIFT),
	MT6373_LDO(VCN33_3, ldo_volt_table4,
		   MT6373_PMIC_RG_LDO_VCN33_3_EN_ADDR, MT6373_PMIC_RG_LDO_VCN33_3_EN_SHIFT,
		   MT6373_PMIC_RG_VCN33_3_VOSEL_ADDR,
		   MT6373_PMIC_RG_VCN33_3_VOSEL_MASK,
		   MT6373_PMIC_RG_VCN33_3_VOCAL_ADDR,
		   MT6373_PMIC_RG_VCN33_3_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VCN33_3_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VCN33_3_LP_SHIFT),
	MT6373_LDO(VCN18IO, ldo_volt_table3,
		   MT6373_PMIC_RG_LDO_VCN18IO_EN_ADDR, MT6373_PMIC_RG_LDO_VCN18IO_EN_SHIFT,
		   MT6373_PMIC_RG_VCN18IO_VOSEL_ADDR,
		   MT6373_PMIC_RG_VCN18IO_VOSEL_MASK,
		   MT6373_PMIC_RG_VCN18IO_VOCAL_ADDR,
		   MT6373_PMIC_RG_VCN18IO_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VCN18IO_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VCN18IO_LP_SHIFT),
	MT6373_LDO(VEFUSE, ldo_volt_table1,
		   MT6373_PMIC_RG_LDO_VEFUSE_EN_ADDR, MT6373_PMIC_RG_LDO_VEFUSE_EN_SHIFT,
		   MT6373_PMIC_RG_VEFUSE_VOSEL_ADDR,
		   MT6373_PMIC_RG_VEFUSE_VOSEL_MASK,
		   MT6373_PMIC_RG_VEFUSE_VOCAL_ADDR,
		   MT6373_PMIC_RG_VEFUSE_VOCAL_MASK,
		   MT6373_PMIC_RG_LDO_VEFUSE_LP_ADDR,
		   MT6373_PMIC_RG_LDO_VEFUSE_LP_SHIFT),
	MT6373_VMCH_EINT(EINT_HIGH, ldo_volt_table4),
	MT6373_VMCH_EINT(EINT_LOW, ldo_volt_table4),
};

static void mt6373_oc_irq_enable_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6373_regulator_info *info
		= container_of(dwork, struct mt6373_regulator_info, oc_work);

	enable_irq(info->irq);
}

static irqreturn_t mt6373_oc_irq(int irq, void *data)
{
	struct regulator_dev *rdev = (struct regulator_dev *)data;
	struct mt6373_regulator_info *info = rdev_get_drvdata(rdev);

	disable_irq_nosync(info->irq);
	if (!regulator_is_enabled_regmap(rdev))
		goto delayed_enable;
	regulator_notifier_call_chain(rdev, REGULATOR_EVENT_OVER_CURRENT,
				      NULL);
delayed_enable:
	schedule_delayed_work(&info->oc_work,
			      msecs_to_jiffies(info->oc_irq_enable_delay_ms));
	return IRQ_HANDLED;
}

static int mt6373_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config)
{
	int ret;
	struct mt6373_regulator_info *info = config->driver_data;

	if (info->irq > 0) {
		ret = of_property_read_u32(np, "mediatek,oc-irq-enable-delay-ms",
					   &info->oc_irq_enable_delay_ms);
		if (ret || !info->oc_irq_enable_delay_ms)
			info->oc_irq_enable_delay_ms = DEFAULT_DELAY_MS;
		INIT_DELAYED_WORK(&info->oc_work, mt6373_oc_irq_enable_work);
	}
	return 0;
}

static bool mt6373_bypass_register(struct mt6373_regulator_info *info)
{
	return info->desc.id == MT6373_ID_VBUCK4;
}

static int mt6373_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct mt6373_regulator_info *info;
	int i, ret;
	bool is_mt6373_cw = false;
	unsigned int val = 0;

	config.dev = pdev->dev.parent;
	config.regmap = dev_get_regmap(pdev->dev.parent, NULL);

	ret = regmap_read(config.regmap, MT6373_PLG_CFG_ELR1, &val);
	if (ret)
		dev_notice(&pdev->dev, "failed to read ELR, ret=%d\n", ret);
	else if ((val & MT6373_ELR_MASK) == 0x4)
		is_mt6373_cw = true;

	for (i = 0; i < MT6373_MAX_REGULATOR; i++) {
		info = &mt6373_regulators[i];
		info->irq = platform_get_irq_byname_optional(pdev, info->desc.name);
		config.driver_data = info;
		if (is_mt6373_cw && mt6373_bypass_register(info))
			continue;

		rdev = devm_regulator_register(&pdev->dev, &info->desc, &config);
		if (IS_ERR(rdev)) {
			ret = PTR_ERR(rdev);
			dev_err(&pdev->dev, "failed to register %s, ret=%d\n",
				info->desc.name, ret);
			continue;
		}

		if (info->irq <= 0)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, info->irq, NULL,
						mt6373_oc_irq,
						IRQF_TRIGGER_HIGH,
						info->desc.name,
						rdev);
		if (ret) {
			dev_err(&pdev->dev, "Failed to request IRQ:%s, ret=%d",
				info->desc.name, ret);
			continue;
		}
	}

	return 0;
}

static void mt6373_regulator_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *regmap;
	int ret = 0;

	regmap = dev_get_regmap(dev->parent, NULL);
	if (!regmap) {
		dev_notice(&pdev->dev, "invalid regmap.\n");
		return;
	}

	ret = regmap_write(regmap, MT6373_TOP_CFG_ELR5, 0x1);
	if (ret < 0) {
		dev_notice(&pdev->dev, "enable sequence off failed.\n");
		return;
	}
}

static const struct platform_device_id mt6373_regulator_ids[] = {
	{ "mt6373-regulator", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mt6373_regulator_ids);

static struct platform_driver mt6373_regulator_driver = {
	.driver = {
		.name = "mt6373-regulator",
	},
	.probe = mt6373_regulator_probe,
	.shutdown = mt6373_regulator_shutdown,
	.id_table = mt6373_regulator_ids,
};
module_platform_driver(mt6373_regulator_driver);

MODULE_AUTHOR("Wen Su <wen.su@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6373 PMIC");
MODULE_LICENSE("GPL v2");
