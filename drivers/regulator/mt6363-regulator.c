// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2021 MediaTek Inc.

#include <linux/interrupt.h>
#include <linux/mfd/mt6363/registers.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6363-regulator.h>
#include <linux/regulator/of_regulator.h>

#define SET_OFFSET	0x1
#define CLR_OFFSET	0x2

#define MT6363_REGULATOR_MODE_NORMAL	0
#define MT6363_REGULATOR_MODE_FCCM	1
#define MT6363_REGULATOR_MODE_LP	2
#define MT6363_REGULATOR_MODE_ULP	3

#define DEFAULT_DELAY_MS		10

/*
 * MT6363 regulator lock register
 */
#define MT6363_TMA_UNLOCK_VALUE		0x9c9c
#define MT6363_BUCK_TOP_UNLOCK_VALUE	0x5543

/*
 * MT6363 regulators' information
 *
 * @desc: standard fields of regulator description.
 * @lp_mode_reg: for operating NORMAL/IDLE mode register.
 * @lp_mode_mask: MASK for operating lp_mode register.
 * @modeset_reg: for operating AUTO/PWM mode register.
 * @modeset_mask: MASK for operating modeset register.
 * @modeset_reg: Calibrates output voltage register.
 * @modeset_mask: MASK of Calibrates output voltage register.
 */
struct mt6363_regulator_info {
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

#define MT6363_BUCK(_name, min, max, step, volt_ranges,		\
		    _enable_reg, en_bit, _vsel_reg, _vsel_mask, \
		    _lp_mode_reg, lp_bit,			\
		    _modeset_reg, modeset_bit)			\
[MT6363_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6363_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6363_buck_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6363_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.linear_ranges = volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6363_map_mode,			\
	},							\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
	.modeset_reg = _modeset_reg,				\
	.modeset_mask = BIT(modeset_bit),			\
}

#define MT6363_LDO_LINEAR1(_name, min, max, step, volt_ranges,	\
			   _enable_reg, en_bit, _vsel_reg,	\
			   _vsel_mask, _lp_mode_reg, lp_bit)	\
[MT6363_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6363_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6363_buck_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6363_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.linear_ranges = volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6363_map_mode,			\
	},							\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
}

#define MT6363_LDO_LINEAR2(_name, min, max, step, volt_ranges,	\
			   _enable_reg, en_bit, _vsel_reg,	\
			   _vsel_mask, _lp_mode_reg, lp_bit)	\
[MT6363_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6363_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6363_volt_range_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6363_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.linear_ranges = volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6363_map_mode,			\
	},							\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
}

#define MT6363_LDO(_name, _volt_table, _enable_reg, en_bit,	\
		   _vsel_reg, _vsel_mask, _vocal_reg,		\
		   _vocal_mask, _lp_mode_reg, lp_bit)		\
[MT6363_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6363_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &mt6363_volt_table_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6363_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ARRAY_SIZE(_volt_table),		\
		.volt_table = _volt_table,			\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6363_map_mode,			\
	},							\
	.vocal_reg = _vocal_reg,				\
	.vocal_mask = _vocal_mask,				\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
}

#define MT6363_LDO_OPS(_name, _ops, _volt_table, _enable_reg, en_bit,	\
		       _vsel_reg, _vsel_mask, _vocal_reg,	\
		       _vocal_mask, _lp_mode_reg, lp_bit)	\
[MT6363_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(#_name),		\
		.of_parse_cb = mt6363_of_parse_cb,		\
		.regulators_node = "regulators",		\
		.ops = &_ops,					\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6363_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ARRAY_SIZE(_volt_table),		\
		.volt_table = _volt_table,			\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(en_bit),			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.of_map_mode = mt6363_map_mode,			\
	},							\
	.vocal_reg = _vocal_reg,				\
	.vocal_mask = _vocal_mask,				\
	.lp_mode_reg = _lp_mode_reg,				\
	.lp_mode_mask = BIT(lp_bit),				\
}

static const struct linear_range mt_volt_range0[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 128, 12500),
};

static const struct linear_range mt_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 191, 6250),
};

static const struct linear_range mt_volt_range2[] = {
	REGULATOR_LINEAR_RANGE(0, 0, 176, 12500),
};

static const struct linear_range mt_volt_range3[] = {
	REGULATOR_LINEAR_RANGE(400000, 0, 127, 6250),
};

static const unsigned int ldo_volt_table0[] = {
	1200000, 1300000, 1500000, 1700000, 1800000, 2000000, 2500000, 2600000,
	2700000, 2800000, 2900000, 3000000, 3100000, 3300000, 3400000, 3500000,
};

static const unsigned int ldo_volt_table1[] = {
	900000, 1000000, 1100000, 1200000, 1300000, 1700000, 1800000, 1810000,
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
	550000, 600000, 650000, 700000, 750000, 800000, 900000, 950000,
	1000000, 1050000, 1100000, 1150000, 1700000, 1750000, 1800000, 1850000,
};

static const unsigned int ldo_volt_table5[] = {
	600000, 650000, 700000, 750000, 800000,
};

static int mt6363_buck_enable(struct regulator_dev *rdev)
{
	return regmap_write(rdev->regmap, rdev->desc->enable_reg + SET_OFFSET,
			    rdev->desc->enable_mask);
}

static int mt6363_buck_disable(struct regulator_dev *rdev)
{
	return regmap_write(rdev->regmap, rdev->desc->enable_reg + CLR_OFFSET,
			    rdev->desc->enable_mask);
}

static inline unsigned int mt6363_map_mode(unsigned int mode)
{
	switch (mode) {
	case MT6363_REGULATOR_MODE_NORMAL:
		return REGULATOR_MODE_NORMAL;
	case MT6363_REGULATOR_MODE_FCCM:
		return REGULATOR_MODE_FAST;
	case MT6363_REGULATOR_MODE_LP:
		return REGULATOR_MODE_IDLE;
	case MT6363_REGULATOR_MODE_ULP:
		return REGULATOR_MODE_STANDBY;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static unsigned int mt6363_regulator_get_mode(struct regulator_dev *rdev)
{
	struct mt6363_regulator_info *info = rdev_get_drvdata(rdev);
	unsigned int val = 0;
	int ret;

	ret = regmap_read(rdev->regmap, info->modeset_reg, &val);
	if (ret) {
		dev_err(&rdev->dev, "Failed to get mt6363 mode: %d\n", ret);
		return ret;
	}

	if (val & info->modeset_mask)
		return REGULATOR_MODE_FAST;

	ret = regmap_read(rdev->regmap, info->lp_mode_reg, &val);
	if (ret) {
		dev_err(&rdev->dev,
			"Failed to get mt6363 lp mode: %d\n", ret);
		return ret;
	}

	if (val & info->lp_mode_mask)
		return REGULATOR_MODE_IDLE;
	else
		return REGULATOR_MODE_NORMAL;
}

static int mt6363_buck_unlock(struct regmap *map, bool unlock)
{
	u16 buf = unlock ? MT6363_BUCK_TOP_UNLOCK_VALUE : 0;

	return regmap_bulk_write(map, MT6363_BUCK_TOP_KEY_PROT_LO, &buf, 2);
}

static int mt6363_regulator_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct mt6363_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0;
	int curr_mode;

	curr_mode = mt6363_regulator_get_mode(rdev);
	switch (mode) {
	case REGULATOR_MODE_FAST:
		ret = mt6363_buck_unlock(rdev->regmap, true);
		if (ret)
			return ret;
		ret = regmap_update_bits(rdev->regmap,
					 info->modeset_reg,
					 info->modeset_mask,
					 info->modeset_mask);
		ret |= mt6363_buck_unlock(rdev->regmap, false);
		break;
	case REGULATOR_MODE_NORMAL:
		if (curr_mode == REGULATOR_MODE_FAST) {
			ret = mt6363_buck_unlock(rdev->regmap, true);
			if (ret)
				return ret;
			ret = regmap_update_bits(rdev->regmap,
						 info->modeset_reg,
						 info->modeset_mask,
						 0);
			ret |= mt6363_buck_unlock(rdev->regmap, false);
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
			"Failed to set mt6363 mode(%d): %d\n", mode, ret);
	}
	return ret;
}

static int mt6363_vemc_set_voltage_sel(struct regulator_dev *rdev, unsigned int sel)
{
	int ret;
	u16 buf = MT6363_TMA_UNLOCK_VALUE;
	unsigned int val = 0;

	ret = regmap_bulk_write(rdev->regmap, MT6363_TOP_TMA_KEY_L, &buf, 2);
	if (ret)
		return ret;

	ret = regmap_read(rdev->regmap, MT6363_TOP_TRAP, &val);
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
					 rdev->desc->vsel_reg,
					 rdev->desc->vsel_mask << 4, sel << 4);
		break;
	default:
		break;
	}
	if (ret)
		return ret;

	buf = 0;
	ret = regmap_bulk_write(rdev->regmap, MT6363_TOP_TMA_KEY_L, &buf, 2);
	return ret;
}

static int mt6363_vemc_get_voltage_sel(struct regulator_dev *rdev)
{
	int ret;
	unsigned int val = 0, sel = 0;

	ret = regmap_read(rdev->regmap, rdev->desc->vsel_reg, &sel);
	if (ret)
		return ret;
	ret = regmap_read(rdev->regmap, MT6363_TOP_TRAP, &val);
	if (ret)
		return ret;
	switch (val) {
	case 0:
		/* If HW trapping is 0, use VEMC_VOSEL_0 */
		sel &= rdev->desc->vsel_mask;
		break;
	case 1:
		/* If HW trapping is 1, use VEMC_VOSEL_1 */
		sel = (sel >> 4) & rdev->desc->vsel_mask;
		break;
	default:
		return -EINVAL;
	}

	return sel;
}

static const struct regulator_ops mt6363_buck_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6363_buck_enable,
	.disable = mt6363_buck_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6363_regulator_set_mode,
	.get_mode = mt6363_regulator_get_mode,
};

static const struct regulator_ops mt6363_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6363_regulator_set_mode,
	.get_mode = mt6363_regulator_get_mode,
};

static const struct regulator_ops mt6363_volt_table_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6363_regulator_set_mode,
	.get_mode = mt6363_regulator_get_mode,
};

static const struct regulator_ops mt6363_vemc_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = mt6363_vemc_set_voltage_sel,
	.get_voltage_sel = mt6363_vemc_get_voltage_sel,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_mode = mt6363_regulator_set_mode,
	.get_mode = mt6363_regulator_get_mode,
};

static int _isink_load_control(struct regulator_dev *rdev, bool enable)
{
	const struct {
		unsigned int reg;
		unsigned int mask;
		unsigned int val;
	} en_settings[] = {
		{ MT6363_ISINK_EN_CTRL0, 0xFF, 0xFF },
		{ MT6363_ISINK_EN_CTRL1, 0xF0, 0xF0 },
	}, dis_settings[] = {
		{ MT6363_ISINK_EN_CTRL1, 0xF0, 0 },
		{ MT6363_ISINK_EN_CTRL0, 0xFF, 0 },
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

	ret = regmap_read(rdev->regmap, MT6363_ISINK_EN_CTRL1, &val);
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

static int mt6363_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config);

/* The array is indexed by id(MT6363_ID_XXX) */
static struct mt6363_regulator_info mt6363_regulators[] = {
	MT6363_BUCK(VS2, 0, 1600000, 12500, mt_volt_range0,
		    MT6363_RG_BUCK_VS2_EN_ADDR,
		    MT6363_RG_BUCK_VS2_EN_SHIFT,
		    MT6363_RG_BUCK_VS2_VOSEL_ADDR,
		    MT6363_RG_BUCK_VS2_VOSEL_MASK,
		    MT6363_RG_BUCK_VS2_LP_ADDR,
		    MT6363_RG_BUCK_VS2_LP_SHIFT,
		    MT6363_RG_VS2_FCCM_ADDR,
		    MT6363_RG_VS2_FCCM_SHIFT),
	MT6363_BUCK(VBUCK1, 0, 1193750, 6250, mt_volt_range1,
		    MT6363_RG_BUCK_VBUCK1_EN_ADDR,
		    MT6363_RG_BUCK_VBUCK1_EN_SHIFT,
		    MT6363_RG_BUCK_VBUCK1_VOSEL_ADDR,
		    MT6363_RG_BUCK_VBUCK1_VOSEL_MASK,
		    MT6363_RG_BUCK_VBUCK1_LP_ADDR,
		    MT6363_RG_BUCK_VBUCK1_LP_SHIFT,
		    MT6363_RG_VBUCK1_FCCM_ADDR,
		    MT6363_RG_VBUCK1_FCCM_SHIFT),
	MT6363_BUCK(VBUCK2, 0, 1193750, 6250, mt_volt_range1,
		    MT6363_RG_BUCK_VBUCK2_EN_ADDR,
		    MT6363_RG_BUCK_VBUCK2_EN_SHIFT,
		    MT6363_RG_BUCK_VBUCK2_VOSEL_ADDR,
		    MT6363_RG_BUCK_VBUCK2_VOSEL_MASK,
		    MT6363_RG_BUCK_VBUCK2_LP_ADDR,
		    MT6363_RG_BUCK_VBUCK2_LP_SHIFT,
		    MT6363_RG_VBUCK2_FCCM_ADDR,
		    MT6363_RG_VBUCK2_FCCM_SHIFT),
	MT6363_BUCK(VBUCK3, 0, 1193750, 6250, mt_volt_range1,
		    MT6363_RG_BUCK_VBUCK3_EN_ADDR,
		    MT6363_RG_BUCK_VBUCK3_EN_SHIFT,
		    MT6363_RG_BUCK_VBUCK3_VOSEL_ADDR,
		    MT6363_RG_BUCK_VBUCK3_VOSEL_MASK,
		    MT6363_RG_BUCK_VBUCK3_LP_ADDR,
		    MT6363_RG_BUCK_VBUCK3_LP_SHIFT,
		    MT6363_RG_VBUCK3_FCCM_ADDR,
		    MT6363_RG_VBUCK3_FCCM_SHIFT),
	MT6363_BUCK(VBUCK4, 0, 1193750, 6250, mt_volt_range1,
		    MT6363_RG_BUCK_VBUCK4_EN_ADDR,
		    MT6363_RG_BUCK_VBUCK4_EN_SHIFT,
		    MT6363_RG_BUCK_VBUCK4_VOSEL_ADDR,
		    MT6363_RG_BUCK_VBUCK4_VOSEL_MASK,
		    MT6363_RG_BUCK_VBUCK4_LP_ADDR,
		    MT6363_RG_BUCK_VBUCK4_LP_SHIFT,
		    MT6363_RG_VBUCK4_FCCM_ADDR,
		    MT6363_RG_VBUCK4_FCCM_SHIFT),
	MT6363_BUCK(VBUCK5, 0, 1193750, 6250, mt_volt_range1,
		    MT6363_RG_BUCK_VBUCK5_EN_ADDR,
		    MT6363_RG_BUCK_VBUCK5_EN_SHIFT,
		    MT6363_RG_BUCK_VBUCK5_VOSEL_ADDR,
		    MT6363_RG_BUCK_VBUCK5_VOSEL_MASK,
		    MT6363_RG_BUCK_VBUCK5_LP_ADDR,
		    MT6363_RG_BUCK_VBUCK5_LP_SHIFT,
		    MT6363_RG_VBUCK5_FCCM_ADDR,
		    MT6363_RG_VBUCK5_FCCM_SHIFT),
	MT6363_BUCK(VBUCK6, 0, 1193750, 6250, mt_volt_range1,
		    MT6363_RG_BUCK_VBUCK6_EN_ADDR,
		    MT6363_RG_BUCK_VBUCK6_EN_SHIFT,
		    MT6363_RG_BUCK_VBUCK6_VOSEL_ADDR,
		    MT6363_RG_BUCK_VBUCK6_VOSEL_MASK,
		    MT6363_RG_BUCK_VBUCK6_LP_ADDR,
		    MT6363_RG_BUCK_VBUCK6_LP_SHIFT,
		    MT6363_RG_VBUCK6_FCCM_ADDR,
		    MT6363_RG_VBUCK6_FCCM_SHIFT),
	MT6363_BUCK(VBUCK7, 0, 1193750, 6250, mt_volt_range1,
		    MT6363_RG_BUCK_VBUCK7_EN_ADDR,
		    MT6363_RG_BUCK_VBUCK7_EN_SHIFT,
		    MT6363_RG_BUCK_VBUCK7_VOSEL_ADDR,
		    MT6363_RG_BUCK_VBUCK7_VOSEL_MASK,
		    MT6363_RG_BUCK_VBUCK7_LP_ADDR,
		    MT6363_RG_BUCK_VBUCK7_LP_SHIFT,
		    MT6363_RG_VBUCK7_FCCM_ADDR,
		    MT6363_RG_VBUCK7_FCCM_SHIFT),
	MT6363_BUCK(VS1, 0, 2200000, 12500, mt_volt_range2,
		    MT6363_RG_BUCK_VS1_EN_ADDR,
		    MT6363_RG_BUCK_VS1_EN_SHIFT,
		    MT6363_RG_BUCK_VS1_VOSEL_ADDR,
		    MT6363_RG_BUCK_VS1_VOSEL_MASK,
		    MT6363_RG_BUCK_VS1_LP_ADDR,
		    MT6363_RG_BUCK_VS1_LP_SHIFT,
		    MT6363_RG_VS1_FCCM_ADDR,
		    MT6363_RG_VS1_FCCM_SHIFT),
	MT6363_BUCK(VS3, 0, 1193750, 6250, mt_volt_range1,
		    MT6363_RG_BUCK_VS3_EN_ADDR,
		    MT6363_RG_BUCK_VS3_EN_SHIFT,
		    MT6363_RG_BUCK_VS3_VOSEL_ADDR,
		    MT6363_RG_BUCK_VS3_VOSEL_MASK,
		    MT6363_RG_BUCK_VS3_LP_ADDR,
		    MT6363_RG_BUCK_VS3_LP_SHIFT,
		    MT6363_RG_VS3_FCCM_ADDR,
		    MT6363_RG_VS3_FCCM_SHIFT),
	MT6363_LDO_LINEAR1(VSRAM_DIGRF, 400000, 1193750, 6250, mt_volt_range3,
			   MT6363_RG_LDO_VSRAM_DIGRF_EN_ADDR,
			   MT6363_RG_LDO_VSRAM_DIGRF_EN_SHIFT,
			   MT6363_RG_LDO_VSRAM_DIGRF_VOSEL_ADDR,
			   MT6363_RG_LDO_VSRAM_DIGRF_VOSEL_MASK,
			   MT6363_RG_LDO_VSRAM_DIGRF_LP_ADDR,
			   MT6363_RG_LDO_VSRAM_DIGRF_LP_SHIFT),
	MT6363_LDO_LINEAR1(VSRAM_MDFE, 400000, 1193750, 6250, mt_volt_range3,
			   MT6363_RG_LDO_VSRAM_MDFE_EN_ADDR,
			   MT6363_RG_LDO_VSRAM_MDFE_EN_SHIFT,
			   MT6363_RG_LDO_VSRAM_MDFE_VOSEL_ADDR,
			   MT6363_RG_LDO_VSRAM_MDFE_VOSEL_MASK,
			   MT6363_RG_LDO_VSRAM_MDFE_LP_ADDR,
			   MT6363_RG_LDO_VSRAM_MDFE_LP_SHIFT),
	MT6363_LDO_LINEAR1(VSRAM_MODEM, 400000, 1193750, 6250, mt_volt_range3,
			   MT6363_RG_LDO_VSRAM_MODEM_EN_ADDR,
			   MT6363_RG_LDO_VSRAM_MODEM_EN_SHIFT,
			   MT6363_RG_LDO_VSRAM_MODEM_VOSEL_ADDR,
			   MT6363_RG_LDO_VSRAM_MODEM_VOSEL_MASK,
			   MT6363_RG_LDO_VSRAM_MODEM_LP_ADDR,
			   MT6363_RG_LDO_VSRAM_MODEM_LP_SHIFT),
	MT6363_LDO_LINEAR2(VSRAM_CPUB, 400000, 1193750, 6250, mt_volt_range3,
			   MT6363_RG_LDO_VSRAM_CPUB_EN_ADDR,
			   MT6363_RG_LDO_VSRAM_CPUB_EN_SHIFT,
			   MT6363_RG_LDO_VSRAM_CPUB_VOSEL_ADDR,
			   MT6363_RG_LDO_VSRAM_CPUB_VOSEL_MASK,
			   MT6363_RG_LDO_VSRAM_CPUB_LP_ADDR,
			   MT6363_RG_LDO_VSRAM_CPUB_LP_SHIFT),
	MT6363_LDO_LINEAR2(VSRAM_CPUM, 400000, 1193750, 6250, mt_volt_range3,
			   MT6363_RG_LDO_VSRAM_CPUM_EN_ADDR,
			   MT6363_RG_LDO_VSRAM_CPUM_EN_SHIFT,
			   MT6363_RG_LDO_VSRAM_CPUM_VOSEL_ADDR,
			   MT6363_RG_LDO_VSRAM_CPUM_VOSEL_MASK,
			   MT6363_RG_LDO_VSRAM_CPUM_LP_ADDR,
			   MT6363_RG_LDO_VSRAM_CPUM_LP_SHIFT),
	MT6363_LDO_LINEAR2(VSRAM_CPUL, 400000, 1193750, 6250, mt_volt_range3,
			   MT6363_RG_LDO_VSRAM_CPUL_EN_ADDR,
			   MT6363_RG_LDO_VSRAM_CPUL_EN_SHIFT,
			   MT6363_RG_LDO_VSRAM_CPUL_VOSEL_ADDR,
			   MT6363_RG_LDO_VSRAM_CPUL_VOSEL_MASK,
			   MT6363_RG_LDO_VSRAM_CPUL_LP_ADDR,
			   MT6363_RG_LDO_VSRAM_CPUL_LP_SHIFT),
	MT6363_LDO_LINEAR2(VSRAM_APU, 400000, 1193750, 6250, mt_volt_range3,
			   MT6363_RG_LDO_VSRAM_APU_EN_ADDR,
			   MT6363_RG_LDO_VSRAM_APU_EN_SHIFT,
			   MT6363_RG_LDO_VSRAM_APU_VOSEL_ADDR,
			   MT6363_RG_LDO_VSRAM_APU_VOSEL_MASK,
			   MT6363_RG_LDO_VSRAM_APU_LP_ADDR,
			   MT6363_RG_LDO_VSRAM_APU_LP_SHIFT),
	MT6363_LDO_OPS(VEMC, mt6363_vemc_ops, ldo_volt_table0,
		       MT6363_RG_LDO_VEMC_EN_ADDR, MT6363_RG_LDO_VEMC_EN_SHIFT,
		       MT6363_RG_VEMC_VOSEL_0_ADDR,
		       MT6363_RG_VEMC_VOSEL_0_MASK,
		       MT6363_RG_VEMC_VOCAL_0_ADDR,
		       MT6363_RG_VEMC_VOCAL_0_MASK,
		       MT6363_RG_LDO_VEMC_LP_ADDR,
		       MT6363_RG_LDO_VEMC_LP_SHIFT),
	MT6363_LDO(VCN13, ldo_volt_table1,
		   MT6363_RG_LDO_VCN13_EN_ADDR, MT6363_RG_LDO_VCN13_EN_SHIFT,
		   MT6363_RG_VCN13_VOSEL_ADDR,
		   MT6363_RG_VCN13_VOSEL_MASK,
		   MT6363_RG_VCN13_VOCAL_ADDR,
		   MT6363_RG_VCN13_VOCAL_MASK,
		   MT6363_RG_LDO_VCN13_LP_ADDR,
		   MT6363_RG_LDO_VCN13_LP_SHIFT),
	MT6363_LDO(VTREF18, ldo_volt_table2,
		   MT6363_RG_LDO_VTREF18_EN_ADDR, MT6363_RG_LDO_VTREF18_EN_SHIFT,
		   MT6363_RG_VTREF18_VOSEL_ADDR,
		   MT6363_RG_VTREF18_VOSEL_MASK,
		   MT6363_RG_VTREF18_VOCAL_ADDR,
		   MT6363_RG_VTREF18_VOCAL_MASK,
		   MT6363_RG_LDO_VTREF18_LP_ADDR,
		   MT6363_RG_LDO_VTREF18_LP_SHIFT),
	MT6363_LDO(VAUX18, ldo_volt_table2,
		   MT6363_RG_LDO_VAUX18_EN_ADDR, MT6363_RG_LDO_VAUX18_EN_SHIFT,
		   MT6363_RG_VAUX18_VOSEL_ADDR,
		   MT6363_RG_VAUX18_VOSEL_MASK,
		   MT6363_RG_VAUX18_VOCAL_ADDR,
		   MT6363_RG_VAUX18_VOCAL_MASK,
		   MT6363_RG_LDO_VAUX18_LP_ADDR,
		   MT6363_RG_LDO_VAUX18_LP_SHIFT),
	MT6363_LDO(VCN15, ldo_volt_table3,
		   MT6363_RG_LDO_VCN15_EN_ADDR, MT6363_RG_LDO_VCN15_EN_SHIFT,
		   MT6363_RG_VCN15_VOSEL_ADDR,
		   MT6363_RG_VCN15_VOSEL_MASK,
		   MT6363_RG_VCN15_VOCAL_ADDR,
		   MT6363_RG_VCN15_VOCAL_MASK,
		   MT6363_RG_LDO_VCN15_LP_ADDR,
		   MT6363_RG_LDO_VCN15_LP_SHIFT),
	MT6363_LDO(VUFS18, ldo_volt_table3,
		   MT6363_RG_LDO_VUFS18_EN_ADDR, MT6363_RG_LDO_VUFS18_EN_SHIFT,
		   MT6363_RG_VUFS18_VOSEL_ADDR,
		   MT6363_RG_VUFS18_VOSEL_MASK,
		   MT6363_RG_VUFS18_VOCAL_ADDR,
		   MT6363_RG_VUFS18_VOCAL_MASK,
		   MT6363_RG_LDO_VUFS18_LP_ADDR,
		   MT6363_RG_LDO_VUFS18_LP_SHIFT),
	MT6363_LDO(VIO18, ldo_volt_table3,
		   MT6363_RG_LDO_VIO18_EN_ADDR, MT6363_RG_LDO_VIO18_EN_SHIFT,
		   MT6363_RG_VIO18_VOSEL_ADDR,
		   MT6363_RG_VIO18_VOSEL_MASK,
		   MT6363_RG_VIO18_VOCAL_ADDR,
		   MT6363_RG_VIO18_VOCAL_MASK,
		   MT6363_RG_LDO_VIO18_LP_ADDR,
		   MT6363_RG_LDO_VIO18_LP_SHIFT),
	MT6363_LDO(VM18, ldo_volt_table4,
		   MT6363_RG_LDO_VM18_EN_ADDR, MT6363_RG_LDO_VM18_EN_SHIFT,
		   MT6363_RG_VM18_VOSEL_ADDR,
		   MT6363_RG_VM18_VOSEL_MASK,
		   MT6363_RG_VM18_VOCAL_ADDR,
		   MT6363_RG_VM18_VOCAL_MASK,
		   MT6363_RG_LDO_VM18_LP_ADDR,
		   MT6363_RG_LDO_VM18_LP_SHIFT),
	MT6363_LDO(VA15, ldo_volt_table3,
		   MT6363_RG_LDO_VA15_EN_ADDR, MT6363_RG_LDO_VA15_EN_SHIFT,
		   MT6363_RG_VA15_VOSEL_ADDR,
		   MT6363_RG_VA15_VOSEL_MASK,
		   MT6363_RG_VA15_VOCAL_ADDR,
		   MT6363_RG_VA15_VOCAL_MASK,
		   MT6363_RG_LDO_VA15_LP_ADDR,
		   MT6363_RG_LDO_VA15_LP_SHIFT),
	MT6363_LDO(VRF18, ldo_volt_table3,
		   MT6363_RG_LDO_VRF18_EN_ADDR, MT6363_RG_LDO_VRF18_EN_SHIFT,
		   MT6363_RG_VRF18_VOSEL_ADDR,
		   MT6363_RG_VRF18_VOSEL_MASK,
		   MT6363_RG_VRF18_VOCAL_ADDR,
		   MT6363_RG_VRF18_VOCAL_MASK,
		   MT6363_RG_LDO_VRF18_LP_ADDR,
		   MT6363_RG_LDO_VRF18_LP_SHIFT),
	MT6363_LDO(VRFIO18, ldo_volt_table3,
		   MT6363_RG_LDO_VRFIO18_EN_ADDR, MT6363_RG_LDO_VRFIO18_EN_SHIFT,
		   MT6363_RG_VRFIO18_VOSEL_ADDR,
		   MT6363_RG_VRFIO18_VOSEL_MASK,
		   MT6363_RG_VRFIO18_VOCAL_ADDR,
		   MT6363_RG_VRFIO18_VOCAL_MASK,
		   MT6363_RG_LDO_VRFIO18_LP_ADDR,
		   MT6363_RG_LDO_VRFIO18_LP_SHIFT),
	MT6363_LDO(VIO075, ldo_volt_table5,
		   MT6363_RG_LDO_VIO075_EN_ADDR, MT6363_RG_LDO_VIO075_EN_SHIFT,
		   MT6363_RG_VIO075_VOSEL_ADDR,
		   MT6363_RG_VIO075_VOSEL_MASK,
		   MT6363_RG_VIO075_VOCAL_ADDR,
		   MT6363_RG_VIO075_VOCAL_MASK,
		   MT6363_RG_LDO_VIO075_LP_ADDR,
		   MT6363_RG_LDO_VIO075_LP_SHIFT),
	MT6363_LDO(VUFS12, ldo_volt_table4,
		   MT6363_RG_LDO_VUFS12_EN_ADDR, MT6363_RG_LDO_VUFS12_EN_SHIFT,
		   MT6363_RG_VUFS12_VOSEL_ADDR,
		   MT6363_RG_VUFS12_VOSEL_MASK,
		   MT6363_RG_VUFS12_VOCAL_ADDR,
		   MT6363_RG_VUFS12_VOCAL_MASK,
		   MT6363_RG_LDO_VUFS12_LP_ADDR,
		   MT6363_RG_LDO_VUFS12_LP_SHIFT),
	MT6363_LDO(VA12_1, ldo_volt_table3,
		   MT6363_RG_LDO_VA12_1_EN_ADDR, MT6363_RG_LDO_VA12_1_EN_SHIFT,
		   MT6363_RG_VA12_1_VOSEL_ADDR,
		   MT6363_RG_VA12_1_VOSEL_MASK,
		   MT6363_RG_VA12_1_VOCAL_ADDR,
		   MT6363_RG_VA12_1_VOCAL_MASK,
		   MT6363_RG_LDO_VA12_1_LP_ADDR,
		   MT6363_RG_LDO_VA12_1_LP_SHIFT),
	MT6363_LDO(VA12_2, ldo_volt_table3,
		   MT6363_RG_LDO_VA12_2_EN_ADDR, MT6363_RG_LDO_VA12_2_EN_SHIFT,
		   MT6363_RG_VA12_2_VOSEL_ADDR,
		   MT6363_RG_VA12_2_VOSEL_MASK,
		   MT6363_RG_VA12_2_VOCAL_ADDR,
		   MT6363_RG_VA12_2_VOCAL_MASK,
		   MT6363_RG_LDO_VA12_2_LP_ADDR,
		   MT6363_RG_LDO_VA12_2_LP_SHIFT),
	MT6363_LDO(VRF12, ldo_volt_table3,
		   MT6363_RG_LDO_VRF12_EN_ADDR, MT6363_RG_LDO_VRF12_EN_SHIFT,
		   MT6363_RG_VRF12_VOSEL_ADDR,
		   MT6363_RG_VRF12_VOSEL_MASK,
		   MT6363_RG_VRF12_VOCAL_ADDR,
		   MT6363_RG_VRF12_VOCAL_MASK,
		   MT6363_RG_LDO_VRF12_LP_ADDR,
		   MT6363_RG_LDO_VRF12_LP_SHIFT),
	MT6363_LDO(VRF13, ldo_volt_table1,
		   MT6363_RG_LDO_VRF13_EN_ADDR, MT6363_RG_LDO_VRF13_EN_SHIFT,
		   MT6363_RG_VRF13_VOSEL_ADDR,
		   MT6363_RG_VRF13_VOSEL_MASK,
		   MT6363_RG_VRF13_VOCAL_ADDR,
		   MT6363_RG_VRF13_VOCAL_MASK,
		   MT6363_RG_LDO_VRF13_LP_ADDR,
		   MT6363_RG_LDO_VRF13_LP_SHIFT),
	MT6363_LDO(VRF09, ldo_volt_table1,
		   MT6363_RG_LDO_VRF09_EN_ADDR, MT6363_RG_LDO_VRF09_EN_SHIFT,
		   MT6363_RG_VRF09_VOSEL_ADDR,
		   MT6363_RG_VRF09_VOSEL_MASK,
		   MT6363_RG_VRF09_VOCAL_ADDR,
		   MT6363_RG_VRF09_VOCAL_MASK,
		   MT6363_RG_LDO_VRF09_LP_ADDR,
		   MT6363_RG_LDO_VRF09_LP_SHIFT),
	[MT6363_ID_ISINK_LOAD] = {
		.desc = {
			.name = "isink_load",
			.of_match = of_match_ptr("isink_load"),
			.regulators_node = "regulators",
			.id = MT6363_ID_ISINK_LOAD,
			.type = REGULATOR_CURRENT,
			.ops = &isink_load_ops,
			.owner = THIS_MODULE,
		},
	}
};

static void mt6363_oc_irq_enable_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6363_regulator_info *info
		= container_of(dwork, struct mt6363_regulator_info, oc_work);

	enable_irq(info->irq);
}

static irqreturn_t mt6363_oc_irq(int irq, void *data)
{
	struct regulator_dev *rdev = (struct regulator_dev *)data;
	struct mt6363_regulator_info *info = rdev_get_drvdata(rdev);

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

static int mt6363_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config)
{
	int ret;
	struct mt6363_regulator_info *info = config->driver_data;

	if (info->irq > 0) {
		ret = of_property_read_u32(np, "mediatek,oc-irq-enable-delay-ms",
					   &info->oc_irq_enable_delay_ms);
		if (ret || !info->oc_irq_enable_delay_ms)
			info->oc_irq_enable_delay_ms = DEFAULT_DELAY_MS;
		INIT_DELAYED_WORK(&info->oc_work, mt6363_oc_irq_enable_work);
	}
	return 0;
}

static int mt6363_regulator_probe(struct platform_device *pdev)
{
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct mt6363_regulator_info *info;
	int i, ret;

	config.dev = pdev->dev.parent;
	config.regmap = dev_get_regmap(pdev->dev.parent, NULL);
	for (i = 0; i < MT6363_MAX_REGULATOR; i++) {
		info = &mt6363_regulators[i];
		info->irq = platform_get_irq_byname_optional(pdev, info->desc.name);
		config.driver_data = info;

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
						mt6363_oc_irq,
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

static const struct platform_device_id mt6363_regulator_ids[] = {
	{ "mt6363-regulator", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mt6363_regulator_ids);

static struct platform_driver mt6363_regulator_driver = {
	.driver = {
		.name = "mt6363-regulator",
	},
	.probe = mt6363_regulator_probe,
	.id_table = mt6363_regulator_ids,
};
module_platform_driver(mt6363_regulator_driver);

MODULE_AUTHOR("Jeter Chen <jeter.chen@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6363 PMIC");
MODULE_LICENSE("GPL v2");
