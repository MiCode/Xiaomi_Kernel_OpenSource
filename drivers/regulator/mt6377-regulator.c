// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2022 MediaTek Inc.

#include <linux/interrupt.h>
#include <linux/mfd/mt6377/registers.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6377-regulator.h>
#include <linux/regulator/of_regulator.h>

#define OP_CFG_OFFSET	0x5
#define NORMAL_OP_CFG	0x10
#define NORMAL_OP_EN	0x800000

#define MT6377_REGULATOR_MODE_NORMAL	0
#define MT6377_REGULATOR_MODE_FCCM	1
#define MT6377_REGULATOR_MODE_LP	2
#define MT6377_REGULATOR_MODE_ULP	3

#define DEFAULT_DELAY_MS		10

/*
 * MT6377 regulators' information
 *
 * @desc: standard fields of regulator description.
 * @lp_mode_reg: for operating NORMAL/IDLE mode register.
 * @lp_mode_mask: MASK for operating lp_mode register.
 * @hw_lp_mode_reg: hardware NORMAL/IDLE mode status register.
 * @hw_lp_mode_mask: MASK for hardware NORMAL/IDLE mode status register.
 * @modeset_reg: for operating AUTO/PWM mode register.
 * @modeset_mask: MASK for operating modeset register.
 * @vocal_reg: Calibrates output voltage register.
 * @vocal_mask: MASK of Calibrates output voltage register.
 * @lp_imax_uA: Maximum load current in Low power mode.
 * @op_en_reg: for HW control operating mode register.
 */
struct mt6377_regulator_info {
	int irq;
	int oc_irq_enable_delay_ms;
	struct delayed_work oc_work;
	struct regulator_desc desc;
	u32 lp_mode_reg;
	u32 lp_mode_mask;
	u32 hw_lp_mode_reg;
	u32 hw_lp_mode_mask;
	u32 modeset_reg;
	u32 modeset_mask;
	u32 vocal_reg;
	u32 vocal_mask;
	int lp_imax_uA;
	u32 op_en_reg;
	u32 orig_op_en;
	u32 orig_op_cfg;
};

#define MT6377_BUCK(_name, min, max, step, volt_ranges,		\
		    _enable_reg, en_bit, _vsel_reg, _vsel_mask, \
		    _lp_mode_reg, lp_bit,			\
		    _modeset_reg, modeset_bit)			\
[MT6377_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6377_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6377_buck_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6377_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.linear_ranges = volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6377_map_mode,			\
	},							\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
	.hw_lp_mode_reg = MT6377_DA_##_name##_EN_ADDR,		\
	.hw_lp_mode_mask = 0xc,					\
	.modeset_reg = _modeset_reg,				\
	.modeset_mask = BIT(modeset_bit),			\
	.lp_imax_uA = 100000,					\
	.op_en_reg = MT6377_BUCK_##_name##_OP_EN_0,		\
}

#define MT6377_VPA(_name, min, max, step, volt_ranges,		\
		   _enable_reg, en_bit, _vsel_reg, _vsel_mask,  \
		   _lp_mode_reg, lp_bit,			\
		   _modeset_reg, modeset_bit)			\
[MT6377_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6377_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6377_buck_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6377_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.linear_ranges = volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6377_map_mode,			\
	},							\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
	.modeset_reg = _modeset_reg,				\
	.modeset_mask = BIT(modeset_bit),			\
	.lp_imax_uA = 0,					\
}

#define MT6377_SSHUB(_name, min, max, step, volt_ranges,	\
		     _enable_reg, _vsel_reg, _vsel_mask)	\
[MT6377_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6377_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6377_sshub_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6377_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.linear_ranges = volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(0),				\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
	},							\
}

#define MT6377_LDO_LINEAR1(_name, min, max, step, volt_ranges,	\
			   _enable_reg, en_bit, _vsel_reg,	\
			   _vsel_mask, _lp_mode_reg, lp_bit)	\
[MT6377_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6377_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6377_volt_range_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6377_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.linear_ranges = volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6377_map_mode,			\
	},							\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
	.hw_lp_mode_reg = MT6377_DA_##_name##_B_LP_ADDR,	\
	.hw_lp_mode_mask = 0x4,					\
}

#define MT6377_LDO(_name, _volt_table, _enable_reg, en_bit,	\
		   _vsel_reg, _vsel_mask, _vocal_reg,		\
		   _vocal_mask, _lp_mode_reg, lp_bit)		\
[MT6377_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6377_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6377_volt_table_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6377_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ARRAY_SIZE(_volt_table),		\
		.volt_table = _volt_table,			\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6377_map_mode,			\
	},							\
	.vocal_reg = _vocal_reg,				\
	.vocal_mask = _vocal_mask,				\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
	.hw_lp_mode_reg = MT6377_DA_##_name##_B_LP_ADDR,	\
	.hw_lp_mode_mask = 0x4,					\
	.lp_imax_uA = 10000,					\
	.op_en_reg = MT6377_LDO_##_name##_OP_EN0,		\
}

#define MT6377_REG_FIXED(_name, _enable_reg, _lp_mode_reg, lp_bit, _fixed_volt)	\
[MT6377_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6377_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6377_volt_fixed_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6377_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = 1,				\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(0),				\
		.fixed_uV = (_fixed_volt),			\
		.of_map_mode = mt6377_map_mode,			\
	},							\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
	.hw_lp_mode_reg = MT6377_DA_##_name##_B_LP_ADDR,	\
	.hw_lp_mode_mask = 0x4,					\
	.lp_imax_uA = 10000,					\
	.op_en_reg = MT6377_LDO_##_name##_OP_EN0,		\
}

#define MT6377_LDO_OPS(_name, _ops, _volt_table, _enable_reg, en_bit,	\
		       _vsel_reg, _vsel_mask, _vocal_reg,	\
		       _vocal_mask, _lp_mode_reg, lp_bit)	\
[MT6377_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6377_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &_ops,					\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6377_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ARRAY_SIZE(_volt_table),		\
		.volt_table = _volt_table,			\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6377_map_mode,			\
	},							\
	.vocal_reg = _vocal_reg,				\
	.vocal_mask = _vocal_mask,				\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
	.hw_lp_mode_reg = MT6377_DA_##_name##_B_LP_ADDR,	\
	.hw_lp_mode_mask = 0x4,					\
	.lp_imax_uA = 10000,					\
	.op_en_reg = MT6377_LDO_##_name##_OP_EN0,		\
}

#define MT6377_VMCH_EINT(_eint_pol, _volt_table)		\
[MT6377_ID_VMCH_##_eint_pol] = {				\
	.desc = {						\
		.name = "VMCH_"#_eint_pol,			\
		.of_match = of_match_ptr("VMCH_"#_eint_pol),	\
		.of_parse_cb = mt6377_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6377_vmch_eint_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6377_ID_VMCH_##_eint_pol,		\
		.owner = THIS_MODULE,				\
		.n_voltages = ARRAY_SIZE(_volt_table),		\
		.volt_table = _volt_table,			\
		.enable_reg = MT6377_LDO_VMCH_EINT,		\
		.enable_mask = MT6377_PMIC_RG_LDO_VMCH_EINT_EN_MASK,	\
		.vsel_reg = MT6377_RG_VMCH_VOSEL_ADDR,	\
		.vsel_mask = MT6377_RG_VMCH_VOSEL_MASK,	\
		.of_map_mode = mt6377_map_mode,			\
	},							\
	.vocal_reg = MT6377_RG_VMCH_VOCAL_ADDR,			\
	.vocal_mask = MT6377_RG_VMCH_VOCAL_MASK,		\
	.lp_mode_reg = MT6377_RG_LDO_VMCH_LP_ADDR,		\
	.lp_mode_mask = BIT(MT6377_RG_LDO_VMCH_LP_SHIFT),	\
}

static const struct linear_range mt_volt_range0[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 191, 6250),
};

static const struct linear_range mt_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 176, 12500),
};

static const struct linear_range mt_volt_range2[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 128, 12500),
};

static const struct linear_range mt_volt_range3[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 127, 6250),
};

static const struct linear_range mt_volt_range4[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 62, 50000),
};

static const unsigned int ldo_volt_table0[] = {
	0, 0, 0, 0, 0, 1100000, 1200000, 1300000,
};

static const unsigned int ldo_volt_table1[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 1700000, 1800000, 1900000,
};

static const unsigned int ldo_volt_table2[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 2700000, 2800000, 2900000,
};

static const unsigned int ldo_volt_table3[] = {
	1800000, 1900000,
};

static const unsigned int ldo_volt_table4[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 2800000, 0, 0, 0, 3300000, 3400000, 3500000,
};

static const unsigned int ldo_volt_table5[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 1600000, 1700000, 1800000,
};

static const unsigned int ldo_volt_table6[] = {
	1240000, 1600000,
};

static const unsigned int ldo_volt_table7[] = {
	1200000, 1240000, 1260000, 1290000,
};

static const unsigned int ldo_volt_table8[] = {
	1800000, 0, 0, 0, 2200000,
};

static const unsigned int ldo_volt_table9[] = {
	550000, 600000, 650000, 700000, 750000, 800000, 900000, 950000,
	1000000, 1050000, 1100000, 1150000, 1700000, 1750000, 1800000, 1850000,
};

static const unsigned int ldo_volt_table10[] = {
	1200000, 1300000, 1500000, 1700000, 1800000, 2000000, 2500000, 2600000,
	2700000, 2800000, 2900000, 3000000, 3100000, 3300000, 3400000, 3500000,
};

static const unsigned int ldo_volt_table11[] = {
	0, 0, 1100000, 1200000, 1300000,
};

static const unsigned int ldo_volt_table12[] = {
	900000, 1000000,
};

static const unsigned int ldo_volt_table13[] = {
	0, 0, 0, 1700000, 1800000, 0, 0, 0,
	0, 0, 2900000, 3000000, 0, 3300000,
};

static const unsigned int ldo_volt_table14[] = {
	0, 0, 0, 1700000, 1800000, 0, 0, 0,
	0, 0, 2900000, 3000000,
};

static const unsigned int ldo_volt_table15[] = {
	1200000, 1300000, 1500000, 0, 1800000, 2000000, 0, 0,
	0, 2800000, 0, 3000000, 0, 3300000,
};

static inline unsigned int mt6377_map_mode(unsigned int mode)
{
	switch (mode) {
	case MT6377_REGULATOR_MODE_NORMAL:
		return REGULATOR_MODE_NORMAL;
	case MT6377_REGULATOR_MODE_FCCM:
		return REGULATOR_MODE_FAST;
	case MT6377_REGULATOR_MODE_LP:
		return REGULATOR_MODE_IDLE;
	case MT6377_REGULATOR_MODE_ULP:
		return REGULATOR_MODE_STANDBY;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static unsigned int mt6377_regulator_get_mode(struct regulator_dev *rdev)
{
	struct mt6377_regulator_info *info = rdev_get_drvdata(rdev);
	unsigned int val = 0;
	int ret;

	ret = regmap_read(rdev->regmap, info->modeset_reg, &val);
	if (ret) {
		dev_err(&rdev->dev, "Failed to get mt6377 mode: %d\n", ret);
		return ret;
	}

	if (val & info->modeset_mask)
		return REGULATOR_MODE_FAST;

	if (info->hw_lp_mode_reg) {
		ret = regmap_read(rdev->regmap, info->hw_lp_mode_reg, &val);
		val &= info->hw_lp_mode_mask;
	} else {
		ret = regmap_read(rdev->regmap, info->lp_mode_reg, &val);
		val &= info->lp_mode_mask;
	}
	if (ret) {
		dev_err(&rdev->dev,
			"Failed to get mt6377 lp mode: %d\n", ret);
		return ret;
	}

	if (val)
		return REGULATOR_MODE_IDLE;
	else
		return REGULATOR_MODE_NORMAL;
}

static int mt6377_regulator_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct mt6377_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0;
	int curr_mode;

	curr_mode = mt6377_regulator_get_mode(rdev);
	switch (mode) {
	case REGULATOR_MODE_FAST:
		ret = regmap_update_bits(rdev->regmap,
					 info->modeset_reg,
					 info->modeset_mask,
					 info->modeset_mask);
		break;
	case REGULATOR_MODE_NORMAL:
		if (curr_mode == REGULATOR_MODE_FAST) {
			ret = regmap_update_bits(rdev->regmap,
						 info->modeset_reg,
						 info->modeset_mask,
						 0);
			break;
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
			"Failed to set mt6377 mode(%d): %d\n", mode, ret);
	}
	return ret;
}

static int mt6377_regulator_set_load(struct regulator_dev *rdev, int load_uA)
{
	int i, ret;
	struct mt6377_regulator_info *info = rdev_get_drvdata(rdev);

	/* not support */
	if (!info->lp_imax_uA)
		return 0;

	if (load_uA >= info->lp_imax_uA) {
		ret = mt6377_regulator_set_mode(rdev, REGULATOR_MODE_NORMAL);
		if (ret)
			return ret;
		ret = regmap_write(rdev->regmap, info->op_en_reg + OP_CFG_OFFSET, NORMAL_OP_CFG);
		for (i = 0; i < 3; i++) {
			ret |= regmap_write(rdev->regmap, info->op_en_reg + i,
					    (NORMAL_OP_EN >> (i * 8)) & 0xff);
		}
	} else {
		ret = regmap_write(rdev->regmap, info->op_en_reg + OP_CFG_OFFSET,
				   info->orig_op_cfg);
		for (i = 0; i < 3; i++) {
			ret |= regmap_write(rdev->regmap, info->op_en_reg + i,
					    (info->orig_op_en >> (i * 8)) & 0xff);
		}
	}

	return ret;
}

static int mt6377_vemc_set_voltage_sel(struct regulator_dev *rdev, unsigned int sel)
{
	int ret;
	unsigned int val = 0;

	ret = regmap_read(rdev->regmap, MT6377_VM_MODE_ADDR, &val);
	if (ret)
		return ret;
	switch (val) {
	case 0:
		/* If HW trapping is 0, use VEMC_VOSEL_0 */
		ret = regmap_update_bits(rdev->regmap,
					 rdev->desc->vsel_reg,
					 rdev->desc->vsel_mask, sel);
		break;
	case 1:
		/* If HW trapping is 1, use VEMC_VOSEL_1 */
		ret = regmap_update_bits(rdev->regmap,
					 rdev->desc->vsel_reg + 1,
					 rdev->desc->vsel_mask, sel);
		break;
	default:
		break;
	}
	if (ret)
		return ret;

	return ret;
}

static int mt6377_vemc_get_voltage_sel(struct regulator_dev *rdev)
{
	int ret;
	unsigned int val = 0, sel = 0;

	ret = regmap_read(rdev->regmap, MT6377_VM_MODE_ADDR, &val);
	if (ret)
		return ret;
	switch (val) {
	case 0:
		/* If HW trapping is 0, use VEMC_VOSEL_0 */
		ret = regmap_read(rdev->regmap, rdev->desc->vsel_reg, &sel);
		break;
	case 1:
		/* If HW trapping is 1, use VEMC_VOSEL_1 */
		ret = regmap_read(rdev->regmap, rdev->desc->vsel_reg + 1, &sel);
		break;
	default:
		return -EINVAL;
	}
	if (ret)
		return ret;
	sel &= rdev->desc->vsel_mask;

	return sel;
}

static int mt6377_vmch_eint_enable(struct regulator_dev *rdev)
{
	unsigned int val;
	int ret;

	if (rdev->desc->id == MT6377_ID_VMCH_EINT_HIGH)
		val = MT6377_PMIC_RG_LDO_VMCH_EINT_POL_MASK;
	else
		val = 0;
	ret = regmap_update_bits(rdev->regmap, MT6377_LDO_VMCH_EINT,
				 MT6377_PMIC_RG_LDO_VMCH_EINT_POL_MASK, val);
	if (ret)
		return ret;

	ret = regmap_update_bits(rdev->regmap, MT6377_RG_LDO_VMCH_EN_ADDR,
				 BIT(MT6377_RG_LDO_VMCH_EN_SHIFT),
				 BIT(MT6377_RG_LDO_VMCH_EN_SHIFT));
	if (ret)
		return ret;

	ret = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				 rdev->desc->enable_mask, rdev->desc->enable_mask);
	return ret;
}

static int mt6377_vmch_eint_disable(struct regulator_dev *rdev)
{
	int ret;

	ret = regmap_update_bits(rdev->regmap, MT6377_RG_LDO_VMCH_EN_ADDR,
				 BIT(MT6377_RG_LDO_VMCH_EN_SHIFT), 0);
	if (ret)
		return ret;

	udelay(1500); /* Must delay for VMCH discharging */
	ret = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				 rdev->desc->enable_mask, 0);
	return ret;
}

static const struct regulator_ops mt6377_buck_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6377_regulator_set_mode,
	.get_mode = mt6377_regulator_get_mode,
	.set_load = mt6377_regulator_set_load,
};

/* for sshub */
static const struct regulator_ops mt6377_sshub_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
};

static const struct regulator_ops mt6377_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6377_regulator_set_mode,
	.get_mode = mt6377_regulator_get_mode,
};

static const struct regulator_ops mt6377_volt_table_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6377_regulator_set_mode,
	.get_mode = mt6377_regulator_get_mode,
	.set_load = mt6377_regulator_set_load,
};

static const struct regulator_ops mt6377_volt_fixed_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6377_regulator_set_mode,
	.get_mode = mt6377_regulator_get_mode,
	.set_load = mt6377_regulator_set_load,
};

static const struct regulator_ops mt6377_vemc_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = mt6377_vemc_set_voltage_sel,
	.get_voltage_sel = mt6377_vemc_get_voltage_sel,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6377_regulator_set_mode,
	.get_mode = mt6377_regulator_get_mode,
	.set_load = mt6377_regulator_set_load,
};

static const struct regulator_ops mt6377_vmch_eint_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6377_vmch_eint_enable,
	.disable = mt6377_vmch_eint_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6377_regulator_set_mode,
	.get_mode = mt6377_regulator_get_mode,
};

static int _isink_load_control(struct regulator_dev *rdev, bool enable)
{
	const struct {
		unsigned int reg;
		unsigned int mask;
		unsigned int val;
	} en_settings[] = {
		{ MT6377_ISINK_EN_CTRL0, 0xFF, 0xFF },
		{ MT6377_ISINK_EN_CTRL1, 0xF0, 0xF0 },
	}, dis_settings[] = {
		{ MT6377_ISINK_EN_CTRL1, 0xF0, 0 },
		{ MT6377_ISINK_EN_CTRL0, 0xFF, 0 },
	}, *settings;
	int i, setting_size, ret;

	if (enable) {
		settings = en_settings;
		setting_size = ARRAY_SIZE(en_settings);
	} else {
		settings = dis_settings;
		setting_size = ARRAY_SIZE(dis_settings);
	}

	for (i = 0; i < setting_size; i++) {
		ret = regmap_update_bits(rdev->regmap,
					 settings[i].reg, settings[i].mask,
					 settings[i].val);
		if (ret) {
			dev_err(&rdev->dev,
				"Failed to %s isink settings[%d], ret=%d\n",
				enable ? "enable" : "disable",
				i, ret);
			return ret;
		}
	}
	return 0;
}

static int isink_load_enable(struct regulator_dev *rdev)
{
	return _isink_load_control(rdev, true);
}

static int isink_load_disable(struct regulator_dev *rdev)
{
	return _isink_load_control(rdev, false);
}

static int isink_load_is_enabled(struct regulator_dev *rdev)
{
	unsigned int val = 0;
	int ret;

	ret = regmap_read(rdev->regmap, MT6377_ISINK_EN_CTRL1, &val);
	if (ret)
		return ret;

	val &= 0xF0;
	return (val == 0xF0);
}

static const struct regulator_ops isink_load_ops = {
	.enable = isink_load_enable,
	.disable = isink_load_disable,
	.is_enabled = isink_load_is_enabled,
};

static int mt6377_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config);

/* The array is indexed by id(MT6377_ID_XXX) */
static struct mt6377_regulator_info mt6377_regulators[] = {
	MT6377_BUCK(VBUCK1, 0, 1193750, 6250, mt_volt_range0,
		    MT6377_RG_BUCK_VBUCK1_EN_ADDR,
		    MT6377_RG_BUCK_VBUCK1_EN_SHIFT,
		    MT6377_RG_BUCK_VBUCK1_VOSEL_ADDR,
		    MT6377_RG_BUCK_VBUCK1_VOSEL_MASK,
		    MT6377_RG_BUCK_VBUCK1_LP_ADDR,
		    MT6377_RG_BUCK_VBUCK1_LP_SHIFT,
		    MT6377_RG_VBUCK1_FCCM_ADDR,
		    MT6377_RG_VBUCK1_FCCM_SHIFT),
	MT6377_BUCK(VBUCK2, 0, 1193750, 6250, mt_volt_range0,
		    MT6377_RG_BUCK_VBUCK2_EN_ADDR,
		    MT6377_RG_BUCK_VBUCK2_EN_SHIFT,
		    MT6377_RG_BUCK_VBUCK2_VOSEL_ADDR,
		    MT6377_RG_BUCK_VBUCK2_VOSEL_MASK,
		    MT6377_RG_BUCK_VBUCK2_LP_ADDR,
		    MT6377_RG_BUCK_VBUCK2_LP_SHIFT,
		    MT6377_RG_VBUCK2_FCCM_ADDR,
		    MT6377_RG_VBUCK2_FCCM_SHIFT),
	MT6377_BUCK(VBUCK3, 0, 1193750, 6250, mt_volt_range0,
		    MT6377_RG_BUCK_VBUCK3_EN_ADDR,
		    MT6377_RG_BUCK_VBUCK3_EN_SHIFT,
		    MT6377_RG_BUCK_VBUCK3_VOSEL_ADDR,
		    MT6377_RG_BUCK_VBUCK3_VOSEL_MASK,
		    MT6377_RG_BUCK_VBUCK3_LP_ADDR,
		    MT6377_RG_BUCK_VBUCK3_LP_SHIFT,
		    MT6377_RG_VBUCK3_FCCM_ADDR,
		    MT6377_RG_VBUCK3_FCCM_SHIFT),
	MT6377_BUCK(VBUCK4, 0, 1193750, 6250, mt_volt_range0,
		    MT6377_RG_BUCK_VBUCK4_EN_ADDR,
		    MT6377_RG_BUCK_VBUCK4_EN_SHIFT,
		    MT6377_RG_BUCK_VBUCK4_VOSEL_ADDR,
		    MT6377_RG_BUCK_VBUCK4_VOSEL_MASK,
		    MT6377_RG_BUCK_VBUCK4_LP_ADDR,
		    MT6377_RG_BUCK_VBUCK4_LP_SHIFT,
		    MT6377_RG_VBUCK4_FCCM_ADDR,
		    MT6377_RG_VBUCK4_FCCM_SHIFT),
	MT6377_BUCK(VBUCK5, 0, 1193750, 6250, mt_volt_range0,
		    MT6377_RG_BUCK_VBUCK5_EN_ADDR,
		    MT6377_RG_BUCK_VBUCK5_EN_SHIFT,
		    MT6377_RG_BUCK_VBUCK5_VOSEL_ADDR,
		    MT6377_RG_BUCK_VBUCK5_VOSEL_MASK,
		    MT6377_RG_BUCK_VBUCK5_LP_ADDR,
		    MT6377_RG_BUCK_VBUCK5_LP_SHIFT,
		    MT6377_RG_VBUCK5_FCCM_ADDR,
		    MT6377_RG_VBUCK5_FCCM_SHIFT),
	MT6377_BUCK(VBUCK6, 0, 1193750, 6250, mt_volt_range0,
		    MT6377_RG_BUCK_VBUCK6_EN_ADDR,
		    MT6377_RG_BUCK_VBUCK6_EN_SHIFT,
		    MT6377_RG_BUCK_VBUCK6_VOSEL_ADDR,
		    MT6377_RG_BUCK_VBUCK6_VOSEL_MASK,
		    MT6377_RG_BUCK_VBUCK6_LP_ADDR,
		    MT6377_RG_BUCK_VBUCK6_LP_SHIFT,
		    MT6377_RG_VBUCK6_FCCM_ADDR,
		    MT6377_RG_VBUCK6_FCCM_SHIFT),
	MT6377_BUCK(VS1, 0, 2200000, 12500, mt_volt_range1,
		    MT6377_RG_BUCK_VS1_EN_ADDR,
		    MT6377_RG_BUCK_VS1_EN_SHIFT,
		    MT6377_RG_BUCK_VS1_VOSEL_ADDR,
		    MT6377_RG_BUCK_VS1_VOSEL_MASK,
		    MT6377_RG_BUCK_VS1_LP_ADDR,
		    MT6377_RG_BUCK_VS1_LP_SHIFT,
		    MT6377_RG_VS1_FCCM_ADDR,
		    MT6377_RG_VS1_FCCM_SHIFT),
	MT6377_BUCK(VS2, 0, 1600000, 12500, mt_volt_range2,
		    MT6377_RG_BUCK_VS2_EN_ADDR,
		    MT6377_RG_BUCK_VS2_EN_SHIFT,
		    MT6377_RG_BUCK_VS2_VOSEL_ADDR,
		    MT6377_RG_BUCK_VS2_VOSEL_MASK,
		    MT6377_RG_BUCK_VS2_LP_ADDR,
		    MT6377_RG_BUCK_VS2_LP_SHIFT,
		    MT6377_RG_VS2_FCCM_ADDR,
		    MT6377_RG_VS2_FCCM_SHIFT),
	MT6377_VPA(VPA, 500000, 3600000, 50000, mt_volt_range4,
		   MT6377_RG_BUCK_VPA_EN_ADDR,
		   MT6377_RG_BUCK_VPA_EN_SHIFT,
		   MT6377_RG_BUCK_VPA_VOSEL_ADDR,
		   MT6377_RG_BUCK_VPA_VOSEL_MASK,
		   MT6377_RG_BUCK_VPA_LP_ADDR,
		   MT6377_RG_BUCK_VPA_LP_SHIFT,
		   MT6377_RG_VPA_MODESET_ADDR,
		   MT6377_RG_VPA_MODESET_SHIFT),
	MT6377_SSHUB(VBUCK3_SSHUB, 0, 1193750, 6250, mt_volt_range0,
		     MT6377_RG_BUCK_VBUCK3_SSHUB_EN_ADDR,
		     MT6377_RG_BUCK_VBUCK3_SSHUB_VOSEL_ADDR,
		     MT6377_RG_BUCK_VBUCK3_SSHUB_VOSEL_MASK),
	MT6377_SSHUB(VBUCK4_SSHUB, 0, 1193750, 6250, mt_volt_range0,
		     MT6377_RG_BUCK_VBUCK4_SSHUB_EN_ADDR,
		     MT6377_RG_BUCK_VBUCK4_SSHUB_VOSEL_ADDR,
		     MT6377_RG_BUCK_VBUCK4_SSHUB_VOSEL_MASK),
	MT6377_SSHUB(VSRAM_OTHERS_SSHUB, 500000, 1293750, 6250, mt_volt_range3,
		     MT6377_RG_LDO_VSRAM_OTHERS_SSHUB_EN_ADDR,
		     MT6377_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL_ADDR,
		     MT6377_RG_LDO_VSRAM_OTHERS_SSHUB_VOSEL_MASK),
	MT6377_LDO_LINEAR1(VSRAM_MD, 500000, 1293750, 6250, mt_volt_range3,
			   MT6377_RG_LDO_VSRAM_MD_EN_ADDR,
			   MT6377_RG_LDO_VSRAM_MD_EN_SHIFT,
			   MT6377_RG_LDO_VSRAM_MD_VOSEL_ADDR,
			   MT6377_RG_LDO_VSRAM_MD_VOSEL_MASK,
			   MT6377_RG_LDO_VSRAM_MD_LP_ADDR,
			   MT6377_RG_LDO_VSRAM_MD_LP_SHIFT),
	MT6377_LDO_LINEAR1(VSRAM_PROC1, 500000, 1293750, 6250, mt_volt_range3,
			   MT6377_RG_LDO_VSRAM_PROC1_EN_ADDR,
			   MT6377_RG_LDO_VSRAM_PROC1_EN_SHIFT,
			   MT6377_RG_LDO_VSRAM_PROC1_VOSEL_ADDR,
			   MT6377_RG_LDO_VSRAM_PROC1_VOSEL_MASK,
			   MT6377_RG_LDO_VSRAM_PROC1_LP_ADDR,
			   MT6377_RG_LDO_VSRAM_PROC1_LP_SHIFT),
	MT6377_LDO_LINEAR1(VSRAM_PROC2, 500000, 1293750, 6250, mt_volt_range3,
			   MT6377_RG_LDO_VSRAM_PROC2_EN_ADDR,
			   MT6377_RG_LDO_VSRAM_PROC2_EN_SHIFT,
			   MT6377_RG_LDO_VSRAM_PROC2_VOSEL_ADDR,
			   MT6377_RG_LDO_VSRAM_PROC2_VOSEL_MASK,
			   MT6377_RG_LDO_VSRAM_PROC2_LP_ADDR,
			   MT6377_RG_LDO_VSRAM_PROC2_LP_SHIFT),
	MT6377_LDO_LINEAR1(VSRAM_OTHERS, 500000, 1293750, 6250, mt_volt_range3,
			   MT6377_RG_LDO_VSRAM_OTHERS_EN_ADDR,
			   MT6377_RG_LDO_VSRAM_OTHERS_EN_SHIFT,
			   MT6377_RG_LDO_VSRAM_OTHERS_VOSEL_ADDR,
			   MT6377_RG_LDO_VSRAM_OTHERS_VOSEL_MASK,
			   MT6377_RG_LDO_VSRAM_OTHERS_LP_ADDR,
			   MT6377_RG_LDO_VSRAM_OTHERS_LP_SHIFT),
	MT6377_LDO(VA12, ldo_volt_table0,
		   MT6377_RG_LDO_VA12_EN_ADDR, MT6377_RG_LDO_VA12_EN_SHIFT,
		   MT6377_RG_VA12_VOSEL_ADDR,
		   MT6377_RG_VA12_VOSEL_MASK,
		   MT6377_RG_VA12_VOCAL_ADDR,
		   MT6377_RG_VA12_VOCAL_MASK,
		   MT6377_RG_LDO_VA12_LP_ADDR,
		   MT6377_RG_LDO_VA12_LP_SHIFT),
	MT6377_LDO(VAUD18, ldo_volt_table1,
		   MT6377_RG_LDO_VAUD18_EN_ADDR, MT6377_RG_LDO_VAUD18_EN_SHIFT,
		   MT6377_RG_VAUD18_VOSEL_ADDR,
		   MT6377_RG_VAUD18_VOSEL_MASK,
		   MT6377_RG_VAUD18_VOCAL_ADDR,
		   MT6377_RG_VAUD18_VOCAL_MASK,
		   MT6377_RG_LDO_VAUD18_LP_ADDR,
		   MT6377_RG_LDO_VAUD18_LP_SHIFT),
	MT6377_LDO(VAUD28, ldo_volt_table2,
		   MT6377_RG_LDO_VAUD28_EN_ADDR, MT6377_RG_LDO_VAUD28_EN_SHIFT,
		   MT6377_RG_VAUD28_VOSEL_ADDR,
		   MT6377_RG_VAUD28_VOSEL_MASK,
		   MT6377_RG_VAUD28_VOCAL_ADDR,
		   MT6377_RG_VAUD28_VOCAL_MASK,
		   MT6377_RG_LDO_VAUD28_LP_ADDR,
		   MT6377_RG_LDO_VAUD28_LP_SHIFT),
	MT6377_LDO(VAUX18, ldo_volt_table3,
		   MT6377_RG_LDO_VAUX18_EN_ADDR, MT6377_RG_LDO_VAUX18_EN_SHIFT,
		   MT6377_RG_VAUX18_VOSEL_ADDR,
		   MT6377_RG_VAUX18_VOSEL_MASK,
		   MT6377_RG_VAUX18_VOCAL_ADDR,
		   MT6377_RG_VAUX18_VOCAL_MASK,
		   MT6377_RG_LDO_VAUX18_LP_ADDR,
		   MT6377_RG_LDO_VAUX18_LP_SHIFT),
	MT6377_LDO(VBIF28, ldo_volt_table2,
		   MT6377_RG_LDO_VBIF28_EN_ADDR, MT6377_RG_LDO_VBIF28_EN_SHIFT,
		   MT6377_RG_VBIF28_VOSEL_ADDR,
		   MT6377_RG_VBIF28_VOSEL_MASK,
		   MT6377_RG_VBIF28_VOCAL_ADDR,
		   MT6377_RG_VBIF28_VOCAL_MASK,
		   MT6377_RG_LDO_VBIF28_LP_ADDR,
		   MT6377_RG_LDO_VBIF28_LP_SHIFT),
	MT6377_LDO(VCN33_1, ldo_volt_table4,
		   MT6377_RG_LDO_VCN33_1_EN_ADDR, MT6377_RG_LDO_VCN33_1_EN_SHIFT,
		   MT6377_RG_VCN33_1_VOSEL_ADDR,
		   MT6377_RG_VCN33_1_VOSEL_MASK,
		   MT6377_RG_VCN33_1_VOCAL_ADDR,
		   MT6377_RG_VCN33_1_VOCAL_MASK,
		   MT6377_RG_LDO_VCN33_1_LP_ADDR,
		   MT6377_RG_LDO_VCN33_1_LP_SHIFT),
	MT6377_LDO(VCN33_2, ldo_volt_table4,
		   MT6377_RG_LDO_VCN33_2_EN_ADDR, MT6377_RG_LDO_VCN33_2_EN_SHIFT,
		   MT6377_RG_VCN33_2_VOSEL_ADDR,
		   MT6377_RG_VCN33_2_VOSEL_MASK,
		   MT6377_RG_VCN33_2_VOCAL_ADDR,
		   MT6377_RG_VCN33_2_VOCAL_MASK,
		   MT6377_RG_LDO_VCN33_2_LP_ADDR,
		   MT6377_RG_LDO_VCN33_2_LP_SHIFT),
	MT6377_LDO(VCN18, ldo_volt_table5,
		   MT6377_RG_LDO_VCN18_EN_ADDR, MT6377_RG_LDO_VCN18_EN_SHIFT,
		   MT6377_RG_VCN18_VOSEL_ADDR,
		   MT6377_RG_VCN18_VOSEL_MASK,
		   MT6377_RG_VCN18_VOCAL_ADDR,
		   MT6377_RG_VCN18_VOCAL_MASK,
		   MT6377_RG_LDO_VCN18_LP_ADDR,
		   MT6377_RG_LDO_VCN18_LP_SHIFT),
	MT6377_LDO(VRFCK, ldo_volt_table6,
		   MT6377_RG_LDO_VRFCK_EN_ADDR, MT6377_RG_LDO_VRFCK_EN_SHIFT,
		   MT6377_RG_VRFCK_VOSEL_ADDR,
		   MT6377_RG_VRFCK_VOSEL_MASK,
		   MT6377_RG_VRFCK_VOCAL_ADDR,
		   MT6377_RG_VRFCK_VOCAL_MASK,
		   MT6377_RG_LDO_VRFCK_LP_ADDR,
		   MT6377_RG_LDO_VRFCK_LP_SHIFT),
	MT6377_LDO(VBBCK, ldo_volt_table7,
		   MT6377_RG_LDO_VBBCK_EN_ADDR, MT6377_RG_LDO_VBBCK_EN_SHIFT,
		   MT6377_RG_VBBCK_VOSEL_ADDR,
		   MT6377_RG_VBBCK_VOSEL_MASK,
		   MT6377_RG_VBBCK_VOCAL_ADDR,
		   MT6377_RG_VBBCK_VOCAL_MASK,
		   MT6377_RG_LDO_VBBCK_LP_ADDR,
		   MT6377_RG_LDO_VBBCK_LP_SHIFT),
	MT6377_LDO(VXO22, ldo_volt_table8,
		   MT6377_RG_LDO_VXO22_EN_ADDR, MT6377_RG_LDO_VXO22_EN_SHIFT,
		   MT6377_RG_VXO22_VOSEL_ADDR,
		   MT6377_RG_VXO22_VOSEL_MASK,
		   MT6377_RG_VXO22_VOCAL_ADDR,
		   MT6377_RG_VXO22_VOCAL_MASK,
		   MT6377_RG_LDO_VXO22_LP_ADDR,
		   MT6377_RG_LDO_VXO22_LP_SHIFT),
	MT6377_LDO(VM18, ldo_volt_table1,
		   MT6377_RG_LDO_VM18_EN_ADDR, MT6377_RG_LDO_VM18_EN_SHIFT,
		   MT6377_RG_VM18_VOSEL_ADDR,
		   MT6377_RG_VM18_VOSEL_MASK,
		   MT6377_RG_VM18_VOCAL_ADDR,
		   MT6377_RG_VM18_VOCAL_MASK,
		   MT6377_RG_LDO_VM18_LP_ADDR,
		   MT6377_RG_LDO_VM18_LP_SHIFT),
	MT6377_LDO(VEFUSE, ldo_volt_table1,
		   MT6377_RG_LDO_VEFUSE_EN_ADDR, MT6377_RG_LDO_VEFUSE_EN_SHIFT,
		   MT6377_RG_VEFUSE_VOSEL_ADDR,
		   MT6377_RG_VEFUSE_VOSEL_MASK,
		   MT6377_RG_VEFUSE_VOCAL_ADDR,
		   MT6377_RG_VEFUSE_VOCAL_MASK,
		   MT6377_RG_LDO_VEFUSE_LP_ADDR,
		   MT6377_RG_LDO_VEFUSE_LP_SHIFT),
	MT6377_LDO_OPS(VEMC, mt6377_vemc_ops, ldo_volt_table10,
		       MT6377_RG_LDO_VEMC_EN_ADDR, MT6377_RG_LDO_VEMC_EN_SHIFT,
		       MT6377_RG_VEMC_VOSEL_0_ADDR,
		       MT6377_RG_VEMC_VOSEL_0_MASK,
		       MT6377_RG_VEMC_VOCAL_0_ADDR,
		       MT6377_RG_VEMC_VOCAL_0_MASK,
		       MT6377_RG_LDO_VEMC_LP_ADDR,
		       MT6377_RG_LDO_VEMC_LP_SHIFT),
	MT6377_LDO(VUFS, ldo_volt_table1,
		   MT6377_RG_LDO_VUFS_EN_ADDR, MT6377_RG_LDO_VUFS_EN_SHIFT,
		   MT6377_RG_VUFS_VOSEL_ADDR,
		   MT6377_RG_VUFS_VOSEL_MASK,
		   MT6377_RG_VUFS_VOCAL_ADDR,
		   MT6377_RG_VUFS_VOCAL_MASK,
		   MT6377_RG_LDO_VUFS_LP_ADDR,
		   MT6377_RG_LDO_VUFS_LP_SHIFT),
	MT6377_LDO(VIO18, ldo_volt_table1,
		   MT6377_RG_LDO_VIO18_EN_ADDR, MT6377_RG_LDO_VIO18_EN_SHIFT,
		   MT6377_RG_VIO18_VOSEL_ADDR,
		   MT6377_RG_VIO18_VOSEL_MASK,
		   MT6377_RG_VIO18_VOCAL_ADDR,
		   MT6377_RG_VIO18_VOCAL_MASK,
		   MT6377_RG_LDO_VIO18_LP_ADDR,
		   MT6377_RG_LDO_VIO18_LP_SHIFT),
	MT6377_LDO(VRF18, ldo_volt_table1,
		   MT6377_RG_LDO_VRF18_EN_ADDR, MT6377_RG_LDO_VRF18_EN_SHIFT,
		   MT6377_RG_VRF18_VOSEL_ADDR,
		   MT6377_RG_VRF18_VOSEL_MASK,
		   MT6377_RG_VRF18_VOCAL_ADDR,
		   MT6377_RG_VRF18_VOCAL_MASK,
		   MT6377_RG_LDO_VRF18_LP_ADDR,
		   MT6377_RG_LDO_VRF18_LP_SHIFT),
	MT6377_LDO(VRF12, ldo_volt_table11,
		   MT6377_RG_LDO_VRF12_EN_ADDR, MT6377_RG_LDO_VRF12_EN_SHIFT,
		   MT6377_RG_VRF12_VOSEL_ADDR,
		   MT6377_RG_VRF12_VOSEL_MASK,
		   MT6377_RG_VRF12_VOCAL_ADDR,
		   MT6377_RG_VRF12_VOCAL_MASK,
		   MT6377_RG_LDO_VRF12_LP_ADDR,
		   MT6377_RG_LDO_VRF12_LP_SHIFT),
	MT6377_LDO(VRF09, ldo_volt_table12,
		   MT6377_RG_LDO_VRF09_EN_ADDR, MT6377_RG_LDO_VRF09_EN_SHIFT,
		   MT6377_RG_VRF09_VOSEL_ADDR,
		   MT6377_RG_VRF09_VOSEL_MASK,
		   MT6377_RG_VRF09_VOCAL_ADDR,
		   MT6377_RG_VRF09_VOCAL_MASK,
		   MT6377_RG_LDO_VRF09_LP_ADDR,
		   MT6377_RG_LDO_VRF09_LP_SHIFT),
	MT6377_LDO(VRFVA12, ldo_volt_table0,
		   MT6377_RG_LDO_VRFVA12_EN_ADDR, MT6377_RG_LDO_VRFVA12_EN_SHIFT,
		   MT6377_RG_VRFVA12_VOSEL_ADDR,
		   MT6377_RG_VRFVA12_VOSEL_MASK,
		   MT6377_RG_VRFVA12_VOCAL_ADDR,
		   MT6377_RG_VRFVA12_VOCAL_MASK,
		   MT6377_RG_LDO_VRFVA12_LP_ADDR,
		   MT6377_RG_LDO_VRFVA12_LP_SHIFT),
	MT6377_LDO(VRFIO18, ldo_volt_table1,
		   MT6377_RG_LDO_VRFIO18_EN_ADDR, MT6377_RG_LDO_VRFIO18_EN_SHIFT,
		   MT6377_RG_VRFIO18_VOSEL_ADDR,
		   MT6377_RG_VRFIO18_VOSEL_MASK,
		   MT6377_RG_VRFIO18_VOCAL_ADDR,
		   MT6377_RG_VRFIO18_VOCAL_MASK,
		   MT6377_RG_LDO_VRFIO18_LP_ADDR,
		   MT6377_RG_LDO_VRFIO18_LP_SHIFT),
	MT6377_LDO(VMCH, ldo_volt_table13,
		   MT6377_RG_LDO_VMCH_EN_ADDR, MT6377_RG_LDO_VMCH_EN_SHIFT,
		   MT6377_RG_VMCH_VOSEL_ADDR,
		   MT6377_RG_VMCH_VOSEL_MASK,
		   MT6377_RG_VMCH_VOCAL_ADDR,
		   MT6377_RG_VMCH_VOCAL_MASK,
		   MT6377_RG_LDO_VMCH_LP_ADDR,
		   MT6377_RG_LDO_VMCH_LP_SHIFT),
	MT6377_LDO(VMC, ldo_volt_table14,
		   MT6377_RG_LDO_VMC_EN_ADDR, MT6377_RG_LDO_VMC_EN_SHIFT,
		   MT6377_RG_VMC_VOSEL_ADDR,
		   MT6377_RG_VMC_VOSEL_MASK,
		   MT6377_RG_VMC_VOCAL_ADDR,
		   MT6377_RG_VMC_VOCAL_MASK,
		   MT6377_RG_LDO_VMC_LP_ADDR,
		   MT6377_RG_LDO_VMC_LP_SHIFT),
	MT6377_REG_FIXED(VUSB, MT6377_RG_LDO_VUSB_EN_ADDR,
			 MT6377_RG_LDO_VUSB_LP_ADDR, MT6377_RG_LDO_VUSB_LP_SHIFT, 3000000),
	MT6377_LDO(VIBR, ldo_volt_table15,
		   MT6377_RG_LDO_VIBR_EN_ADDR, MT6377_RG_LDO_VIBR_EN_SHIFT,
		   MT6377_RG_VIBR_VOSEL_ADDR,
		   MT6377_RG_VIBR_VOSEL_MASK,
		   MT6377_RG_VIBR_VOCAL_ADDR,
		   MT6377_RG_VIBR_VOCAL_MASK,
		   MT6377_RG_LDO_VIBR_LP_ADDR,
		   MT6377_RG_LDO_VIBR_LP_SHIFT),
	MT6377_LDO(VIO28, ldo_volt_table15,
		   MT6377_RG_LDO_VIO28_EN_ADDR, MT6377_RG_LDO_VIO28_EN_SHIFT,
		   MT6377_RG_VIO28_VOSEL_ADDR,
		   MT6377_RG_VIO28_VOSEL_MASK,
		   MT6377_RG_VIO28_VOCAL_ADDR,
		   MT6377_RG_VIO28_VOCAL_MASK,
		   MT6377_RG_LDO_VIO28_LP_ADDR,
		   MT6377_RG_LDO_VIO28_LP_SHIFT),
	MT6377_LDO(VFP, ldo_volt_table15,
		   MT6377_RG_LDO_VFP_EN_ADDR, MT6377_RG_LDO_VFP_EN_SHIFT,
		   MT6377_RG_VFP_VOSEL_ADDR,
		   MT6377_RG_VFP_VOSEL_MASK,
		   MT6377_RG_VFP_VOCAL_ADDR,
		   MT6377_RG_VFP_VOCAL_MASK,
		   MT6377_RG_LDO_VFP_LP_ADDR,
		   MT6377_RG_LDO_VFP_LP_SHIFT),
	MT6377_LDO(VTP, ldo_volt_table15,
		   MT6377_RG_LDO_VTP_EN_ADDR, MT6377_RG_LDO_VTP_EN_SHIFT,
		   MT6377_RG_VTP_VOSEL_ADDR,
		   MT6377_RG_VTP_VOSEL_MASK,
		   MT6377_RG_VTP_VOCAL_ADDR,
		   MT6377_RG_VTP_VOCAL_MASK,
		   MT6377_RG_LDO_VTP_LP_ADDR,
		   MT6377_RG_LDO_VTP_LP_SHIFT),
	MT6377_VMCH_EINT(EINT_HIGH, ldo_volt_table13),
	MT6377_VMCH_EINT(EINT_LOW, ldo_volt_table13),
	[MT6377_ID_ISINK_LOAD] = {
		.desc = {
			.name = "isink_load",
			.of_match = of_match_ptr("isink_load"),
			.regulators_node = "regulators",
			.id = MT6377_ID_ISINK_LOAD,
			.type = REGULATOR_CURRENT,
			.ops = &isink_load_ops,
			.owner = THIS_MODULE,
		},
	}
};

static void mt6377_oc_irq_enable_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6377_regulator_info *info
		= container_of(dwork, struct mt6377_regulator_info, oc_work);

	enable_irq(info->irq);
}

static irqreturn_t mt6377_oc_irq(int irq, void *data)
{
	struct regulator_dev *rdev = (struct regulator_dev *)data;
	struct mt6377_regulator_info *info = rdev_get_drvdata(rdev);

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

static int mt6377_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config)
{
	int ret;
	struct mt6377_regulator_info *info = config->driver_data;

	if (info->irq > 0) {
		ret = of_property_read_u32(np, "mediatek,oc-irq-enable-delay-ms",
					   &info->oc_irq_enable_delay_ms);
		if (ret || !info->oc_irq_enable_delay_ms)
			info->oc_irq_enable_delay_ms = DEFAULT_DELAY_MS;
	}
	return 0;
}

static int mt6377_backup_op_setting(struct regmap *map, struct mt6377_regulator_info *info)
{
	int i, ret;
	u32 val = 0;

	ret = regmap_read(map, info->op_en_reg + OP_CFG_OFFSET, &info->orig_op_cfg);
	for (i = 0; i < 3; i++) {
		ret |= regmap_read(map, info->op_en_reg + i, &val);
		info->orig_op_en |= val << (i * 8);
	}
	if (ret)
		return ret;

	return 0;
}

static int mt6377_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct mt6377_regulator_info *info;
	int i, ret;

	config.dev = pdev->dev.parent;
	config.regmap = dev_get_regmap(pdev->dev.parent, NULL);
	for (i = 0; i < MT6377_MAX_REGULATOR; i++) {
		info = &mt6377_regulators[i];
		info->irq = platform_get_irq_byname_optional(pdev, info->desc.name);
		info->oc_irq_enable_delay_ms = DEFAULT_DELAY_MS;
		config.driver_data = info;

		rdev = devm_regulator_register(&pdev->dev, &info->desc, &config);
		if (IS_ERR(rdev)) {
			ret = PTR_ERR(rdev);
			dev_err(&pdev->dev, "failed to register %s, ret=%d\n",
				info->desc.name, ret);
			continue;
		}
		if (info->lp_imax_uA) {
			ret = mt6377_backup_op_setting(config.regmap, info);
			if (ret) {
				dev_notice(&pdev->dev, "failed to backup op_setting (%s)\n",
					   info->desc.name);
				info->lp_imax_uA = 0;
			}
		}

		if (info->irq <= 0)
			continue;
		INIT_DELAYED_WORK(&info->oc_work, mt6377_oc_irq_enable_work);
		ret = devm_request_threaded_irq(&pdev->dev, info->irq, NULL,
						mt6377_oc_irq,
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

static const struct platform_device_id mt6377_regulator_ids[] = {
	{ "mt6377-regulator", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mt6377_regulator_ids);

static struct platform_driver mt6377_regulator_driver = {
	.driver = {
		.name = "mt6377-regulator",
	},
	.probe = mt6377_regulator_probe,
	.id_table = mt6377_regulator_ids,
};
module_platform_driver(mt6377_regulator_driver);

MODULE_AUTHOR("Wen Su <wen.su@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6377 PMIC");
MODULE_LICENSE("GPL v2");
