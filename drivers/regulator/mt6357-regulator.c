// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mfd/mt6357/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6357-regulator.h>
#include <linux/regulator/of_regulator.h>

#define MT6357_BUCK_MODE_AUTO		0
#define MT6357_BUCK_MODE_FORCE_PWM	1
#define MT6357_BUCK_MODE_NORMAL		0
#define MT6357_BUCK_MODE_LP		2

#define DEF_OC_IRQ_ENABLE_DELAY_MS	10

/*
 * MT6357 regulators' information
 *
 * @desc: standard fields of regulator description.
 * @status_reg: for query status of regulators.
 * @qi: Mask for query enable signal status of regulators.
 * @modeset_reg: for operating AUTO/PWM mode register.
 * @modeset_mask: MASK for operating modeset register.
 * @modeset_shift: SHIFT for operating modeset register.
 */
struct mt6357_regulator_info {
	int irq;
	int oc_irq_enable_delay_ms;
	struct delayed_work oc_work;
	struct regulator_desc desc;
	u32 status_reg;
	u32 qi;
	const u32 *index_table;
	unsigned int n_table;
	u32 vsel_shift;
	u32 da_vsel_reg;
	u32 da_vsel_mask;
	u32 da_vsel_shift;
	u32 modeset_reg;
	u32 modeset_mask;
	u32 modeset_shift;
	u32 maskirq_reg;
	u32 maskirq_shift;
};

#define MT6357_BUCK(match, _name, min, max, step, min_sel,	\
	volt_ranges, _enable_reg, _status_reg,			\
	_da_vsel_reg, _da_vsel_mask, _da_vsel_shift,		\
	_vsel_reg, _vsel_mask,					\
	_modeset_reg, _modeset_shift,				\
	_maskirq_reg, _maskirq_shift)				\
[MT6357_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(match),		\
		.ops = &mt6357_volt_range_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6357_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.uV_step = (step),				\
		.linear_min_sel = (min_sel),			\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.min_uV = (min),				\
		.linear_ranges = volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(0),				\
		.of_map_mode = mt6357_map_mode,			\
	},							\
	.da_vsel_reg = _da_vsel_reg,				\
	.da_vsel_mask = _da_vsel_mask,				\
	.da_vsel_shift = _da_vsel_shift,			\
	.status_reg = _status_reg,				\
	.qi = BIT(0),						\
	.modeset_reg = _modeset_reg,				\
	.modeset_mask = BIT(_modeset_shift),			\
	.modeset_shift = _modeset_shift,			\
	.maskirq_reg = _maskirq_reg,				\
	.maskirq_shift = _maskirq_shift,			\
}

#define MT6357_LDO_LINEAR(match, _name, min, max, step, min_sel,\
	volt_ranges, _enable_reg, _status_reg,			\
	_da_vsel_reg, _da_vsel_mask, _da_vsel_shift,		\
	_vsel_reg, _vsel_mask,					\
	_maskirq_reg, _maskirq_shift)				\
[MT6357_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(match),		\
		.ops = &mt6357_volt_range_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6357_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.uV_step = (step),				\
		.linear_min_sel = (min_sel),			\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.min_uV = (min),				\
		.linear_ranges = volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(0),				\
	},							\
	.da_vsel_reg = _da_vsel_reg,				\
	.da_vsel_mask = _da_vsel_mask,				\
	.da_vsel_shift = _da_vsel_shift,			\
	.status_reg = _status_reg,				\
	.maskirq_reg = _maskirq_reg,				\
	.maskirq_shift = _maskirq_shift,			\
	.qi = BIT(0),						\
}

#define MT6357_LDO(match, _name, _volt_table, _index_table,	\
	_enable_reg, _enable_mask, _status_reg,			\
	_vsel_reg, _vsel_mask, _vsel_shift,			\
	_maskirq_reg, _maskirq_shift)				\
[MT6357_ID_##_name] = {						\
	.desc = {						\
		.name = #_name,					\
		.of_match = of_match_ptr(match),		\
		.ops = &mt6357_volt_table_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6357_ID_##_name,			\
		.owner = THIS_MODULE,				\
		.n_voltages = ARRAY_SIZE(_volt_table),		\
		.volt_table = _volt_table,			\
		.vsel_reg = _vsel_reg,				\
		.vsel_mask = _vsel_mask,			\
		.enable_reg = _enable_reg,			\
		.enable_mask = BIT(_enable_mask),		\
	},							\
	.status_reg = _status_reg,				\
	.maskirq_reg = _maskirq_reg,				\
	.maskirq_shift = _maskirq_shift,			\
	.qi = BIT(15),						\
	.index_table = _index_table,				\
	.n_table = ARRAY_SIZE(_index_table),			\
	.vsel_shift = _vsel_shift,				\
}

#define MT6357_REG_FIXED(match, _name, _enable_reg,	\
	_status_reg, _fixed_volt,			\
	_maskirq_reg, _maskirq_shift)			\
[MT6357_ID_##_name] = {					\
	.desc = {					\
		.name = #_name,				\
		.of_match = of_match_ptr(match),	\
		.ops = &mt6357_volt_fixed_ops,		\
		.type = REGULATOR_VOLTAGE,		\
		.id = MT6357_ID_##_name,		\
		.owner = THIS_MODULE,			\
		.n_voltages = 1,			\
		.enable_reg = _enable_reg,		\
		.enable_mask = BIT(0),			\
		.fixed_uV = (_fixed_volt),		\
	},						\
	.status_reg = _status_reg,			\
	.maskirq_reg = _maskirq_reg,			\
	.maskirq_shift = _maskirq_shift,		\
	.qi = BIT(15),					\
}

#define MT6357_LDO_VMC_DESC(match, _name, volt_ranges,	\
	_enable_reg, _status_reg, _vsel_reg, _vsel_mask,\
	_maskirq_reg, _maskirq_shift)			\
[MT6357_ID_##_name] = {					\
	.desc = {					\
		.name = #_name,				\
		.of_match = of_match_ptr(match),	\
		.ops = &mt6357_vmc_ops,			\
		.type = REGULATOR_VOLTAGE,		\
		.id = MT6357_ID_##_name,		\
		.owner = THIS_MODULE,			\
		.n_voltages = 0xd01,			\
		.linear_ranges = volt_ranges,		\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),\
		.vsel_reg = _vsel_reg,			\
		.vsel_mask = _vsel_mask,		\
		.enable_reg = _enable_reg,		\
		.enable_mask = BIT(0),			\
	},						\
	.status_reg = _status_reg,			\
	.maskirq_reg = _maskirq_reg,			\
	.maskirq_shift = _maskirq_shift,		\
	.qi = BIT(15),					\
}

static const struct regulator_linear_range mt_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 0x7f, 6250),
};

static const struct regulator_linear_range mt_volt_range2[] = {
	REGULATOR_LINEAR_RANGE(518750, 0, 0x7f, 6250),
};

static const struct regulator_linear_range mt_volt_range3[] = {
	REGULATOR_LINEAR_RANGE(1200000, 0, 0x50, 12500),
};

static const struct regulator_linear_range mt_volt_range4[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 0x3f, 50000),
};

/* for vmc voltage calibration: 1.86V, range 1.8 ~ 3.3V */
static const struct regulator_linear_range mt_volt_range5[] = {
	REGULATOR_LINEAR_RANGE(1800000, 0x400, 0x400, 0),
	REGULATOR_LINEAR_RANGE(1810000, 0x401, 0x40a, 10000),
	REGULATOR_LINEAR_RANGE(2900000, 0xa00, 0xa00, 0),
	REGULATOR_LINEAR_RANGE(2910000, 0xa01, 0xa09, 10000),
	REGULATOR_LINEAR_RANGE(3000000, 0xb00, 0xb00, 0),
	REGULATOR_LINEAR_RANGE(3010000, 0xb01, 0xb0a, 10000),
	REGULATOR_LINEAR_RANGE(3300000, 0xd00, 0xd00, 0),
};

static const u32 vxo22_voltages[] = {
	2200000, 2400000,
};

static const u32 vefuse_voltages[] = {
	1200000, 1300000, 1500000, 1800000, 2800000, 2900000, 3000000, 3300000,
};

static const u32 vcn33_bt_voltages[] = {
	3300000, 3400000, 3500000,
};

static const u32 vcn33_wifi_voltages[] = {
	3300000, 3400000, 3500000,
};

static const u32 vcama_voltages[] = {
	2500000, 2800000,
};

static const u32 vcamd_voltages[] = {
	1000000, 1100000, 1200000, 1300000, 1500000, 1800000,
};

static const u32 vldo28_voltages[] = {
	2800000, 3000000,
};

static const u32 vdram_voltages[] = {
	1100000, 1200000,
};

static const u32 vmch_voltages[] = {
	2900000, 3000000, 3300000,
};

static const u32 vemc_voltages[] = {
	2900000, 3000000, 3300000,
};

static const u32 vsim1_voltages[] = {
	1700000, 1800000, 2700000, 3000000, 3100000,
};

static const u32 vsim2_voltages[] = {
	1700000, 1800000, 2700000, 3000000, 3100000,
};

static const u32 vibr_voltages[] = {
	1200000, 1300000, 1500000, 1800000, 2000000, 2800000, 3000000, 3300000,
};

static const u32 vusb33_voltages[] = {
	3000000, 3100000,
};

static const u32 vxo22_idx[] = {
	0, 2,
};

static const u32 vefuse_idx[] = {
	0, 1, 2, 4, 9, 10, 11, 13,
};

static const u32 vcn33_bt_idx[] = {
	1, 2, 3,
};

static const u32 vcn33_wifi_idx[] = {
	1, 2, 3,
};

static const u32 vcama_idx[] = {
	7, 10,
};

static const u32 vcamd_idx[] = {
	4, 5, 6, 7, 9, 12,
};

static const u32 vldo28_idx[] = {
	1, 3,
};

static const u32 vdram_idx[] = {
	1, 2,
};

static const u32 vmch_idx[] = {
	2, 3, 5,
};

static const u32 vemc_idx[] = {
	2, 3, 5,
};

static const u32 vsim1_idx[] = {
	3, 4, 8, 11, 12,
};

static const u32 vsim2_idx[] = {
	3, 4, 8, 11, 12,
};

static const u32 vibr_idx[] = {
	0, 1, 2, 4, 5, 9, 11, 13,
};

static const u32 vusb33_idx[] = {
	3, 4,
};

static int mt6357_regulator_enable(struct regulator_dev *rdev)
{
	struct mt6357_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0, ret2 = 0;

	ret = regulator_enable_regmap(rdev);
	/* Unmask oc irq after enable regulator */
	ret2 = regmap_update_bits(rdev->regmap, info->maskirq_reg,
				 0x1 << info->maskirq_shift,
				 0 << info->maskirq_shift);

	return ret;
}

static int mt6357_regulator_disable(struct regulator_dev *rdev)
{
	struct mt6357_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0;

	if (rdev->use_count == 0) {
		dev_notice(&rdev->dev, "%s:%s should not be disable.(use_count=0)\n"
			   , __func__, rdev->desc->name);
		ret = -EIO;
	} else {
		/* Mask oc irq before disable regulator */
		ret = regmap_update_bits(rdev->regmap, info->maskirq_reg,
					 0x1 << info->maskirq_shift,
					 0x1 << info->maskirq_shift);
		ret = regulator_disable_regmap(rdev);
	}

	return ret;
}

static inline unsigned int mt6357_map_mode(unsigned int mode)
{
	switch (mode) {
	case MT6357_BUCK_MODE_NORMAL:
		return REGULATOR_MODE_NORMAL;
	case MT6357_BUCK_MODE_FORCE_PWM:
		return REGULATOR_MODE_FAST;
	case MT6357_BUCK_MODE_LP:
		return REGULATOR_MODE_IDLE;
	default:
		return REGULATOR_MODE_INVALID;
	}
}

static int mt6357_set_voltage_sel(struct regulator_dev *rdev,
				  unsigned int selector)
{
	int idx = 0, ret = 0;
	const u32 *pvol;
	struct mt6357_regulator_info *info = rdev_get_drvdata(rdev);

	pvol = info->index_table;

	idx = pvol[selector];
	ret = regmap_update_bits(rdev->regmap, info->desc.vsel_reg,
				 info->desc.vsel_mask,
				 idx << info->vsel_shift);

	return ret;
}

static int mt6357_get_voltage_sel(struct regulator_dev *rdev)
{
	int idx = 0, ret = 0;
	u32 selector = 0;
	struct mt6357_regulator_info *info = rdev_get_drvdata(rdev);
	const u32 *pvol;

	ret = regmap_read(rdev->regmap, info->desc.vsel_reg, &selector);
	if (ret != 0) {
		dev_err(&rdev->dev,
			"Failed to get mt6357 %s vsel reg: %d\n",
			info->desc.name, ret);
		return ret;
	}

	selector = (selector & info->desc.vsel_mask) >> info->vsel_shift;
	pvol = info->index_table;
	for (idx = 0; idx < info->desc.n_voltages; idx++) {
		if (pvol[idx] == selector)
			return idx;
	}

	return -EINVAL;
}

static int mt6357_get_linear_voltage_sel(struct regulator_dev *rdev)
{
	struct mt6357_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0, regval = 0;

	ret = regmap_read(rdev->regmap, info->da_vsel_reg, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev,
			"Failed to get mt6357 Buck %s vsel reg: %d\n",
			info->desc.name, ret);
		return ret;
	}

	ret = (regval >> info->da_vsel_shift) & info->da_vsel_mask;

	return ret;
}

static int mt6357_get_status(struct regulator_dev *rdev)
{
	int ret = 0;
	u32 regval = 0;
	struct mt6357_regulator_info *info = rdev_get_drvdata(rdev);

	ret = regmap_read(rdev->regmap, info->status_reg, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev, "Failed to get enable reg: %d\n", ret);
		return ret;
	}

	return (regval & info->qi) ? REGULATOR_STATUS_ON : REGULATOR_STATUS_OFF;
}

static unsigned int mt6357_regulator_get_mode(struct regulator_dev *rdev)
{
	struct mt6357_regulator_info *info = rdev_get_drvdata(rdev);
	int ret = 0, regval = 0;

	ret = regmap_read(rdev->regmap, info->modeset_reg, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev,
			"Failed to get mt6357 buck mode: %d\n", ret);
		return ret;
	}

	switch ((regval & info->modeset_mask) >> info->modeset_shift) {
	case MT6357_BUCK_MODE_AUTO:
		return REGULATOR_MODE_NORMAL;
	case MT6357_BUCK_MODE_FORCE_PWM:
		return REGULATOR_MODE_FAST;
	default:
		return -EINVAL;
	}
}

static int mt6357_regulator_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct mt6357_regulator_info *info = rdev_get_drvdata(rdev);
	int val = 0;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		val = MT6357_BUCK_MODE_FORCE_PWM;
		break;
	case REGULATOR_MODE_NORMAL:
		val = MT6357_BUCK_MODE_AUTO;
		break;
	default:
		return -EINVAL;
	}

	dev_dbg(&rdev->dev, "mt6357 buck set_mode %#x, %#x, %#x, %#x\n",
		info->modeset_reg, info->modeset_mask,
		info->modeset_shift, val);

	val <<= info->modeset_shift;

	return regmap_update_bits(rdev->regmap, info->modeset_reg,
				  info->modeset_mask, val);
}

static const struct regulator_ops mt6357_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = mt6357_get_linear_voltage_sel,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6357_regulator_enable,
	.disable = mt6357_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6357_get_status,
	.set_mode = mt6357_regulator_set_mode,
	.get_mode = mt6357_regulator_get_mode,
};

static const struct regulator_ops mt6357_volt_table_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = mt6357_set_voltage_sel,
	.get_voltage_sel = mt6357_get_voltage_sel,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6357_regulator_enable,
	.disable = mt6357_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6357_get_status,
};

static const struct regulator_ops mt6357_volt_fixed_ops = {
	.enable = mt6357_regulator_enable,
	.disable = mt6357_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6357_get_status,
};

static const struct regulator_ops mt6357_vmc_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.enable = mt6357_regulator_enable,
	.disable = mt6357_regulator_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6357_get_status,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
};

/* The array is indexed by id(MT6357_ID_XXX) */
static struct mt6357_regulator_info mt6357_regulators[] = {
	MT6357_BUCK("buck_vs1", VS1, 1200000, 2200000, 12500, 0,
		    mt_volt_range3, MT6357_RG_BUCK_VS1_EN_ADDR,
		    MT6357_DA_VS1_EN_ADDR, MT6357_DA_VS1_VOSEL_ADDR,
		    MT6357_DA_VS1_VOSEL_MASK, MT6357_DA_VS1_VOSEL_SHIFT,
		    MT6357_RG_BUCK_VS1_VOSEL_ADDR,
		    MT6357_RG_BUCK_VS1_VOSEL_MASK <<
		    MT6357_RG_BUCK_VS1_VOSEL_SHIFT,
		    MT6357_RG_VS1_MODESET_ADDR, MT6357_RG_VS1_MODESET_SHIFT,
		    MT6357_RG_INT_MASK_VS1_OC_ADDR, MT6357_RG_INT_MASK_VS1_OC_SHIFT),
	MT6357_BUCK("buck_vmodem", VMODEM, 500000, 1193750, 6250, 0,
		    mt_volt_range1, MT6357_RG_BUCK_VMODEM_EN_ADDR,
		    MT6357_DA_VMODEM_EN_ADDR, MT6357_DA_VMODEM_VOSEL_ADDR,
		    MT6357_DA_VMODEM_VOSEL_MASK, MT6357_DA_VMODEM_VOSEL_SHIFT,
		    MT6357_RG_BUCK_VMODEM_VOSEL_ADDR,
		    MT6357_RG_BUCK_VMODEM_VOSEL_MASK <<
		    MT6357_RG_BUCK_VMODEM_VOSEL_SHIFT,
		    MT6357_RG_VMODEM_FPWM_ADDR, MT6357_RG_VMODEM_FPWM_SHIFT,
		    MT6357_RG_INT_MASK_VMODEM_OC_ADDR, MT6357_RG_INT_MASK_VMODEM_OC_SHIFT),
	MT6357_BUCK("buck_vcore", VCORE, 518750, 1312500, 6250, 0,
		    mt_volt_range2, MT6357_RG_BUCK_VCORE_EN_ADDR,
		    MT6357_DA_VCORE_EN_ADDR, MT6357_DA_VCORE_VOSEL_ADDR,
		    MT6357_DA_VCORE_VOSEL_MASK, MT6357_DA_VCORE_VOSEL_SHIFT,
		    MT6357_RG_BUCK_VCORE_VOSEL_ADDR,
		    MT6357_RG_BUCK_VCORE_VOSEL_MASK <<
		    MT6357_RG_BUCK_VCORE_VOSEL_SHIFT,
		    MT6357_RG_VCORE_FPWM_ADDR, MT6357_RG_VCORE_FPWM_SHIFT,
		    MT6357_RG_INT_MASK_VCORE_OC_ADDR, MT6357_RG_INT_MASK_VCORE_OC_SHIFT),
	MT6357_BUCK("buck_vproc", VPROC, 518750, 1312500, 6250, 0,
		    mt_volt_range2, MT6357_RG_BUCK_VPROC_EN_ADDR,
		    MT6357_DA_VPROC_EN_ADDR, MT6357_DA_VPROC_VOSEL_ADDR,
		    MT6357_DA_VPROC_VOSEL_MASK, MT6357_DA_VPROC_VOSEL_SHIFT,
		    MT6357_RG_BUCK_VPROC_VOSEL_ADDR,
		    MT6357_RG_BUCK_VPROC_VOSEL_MASK <<
		    MT6357_RG_BUCK_VPROC_VOSEL_SHIFT,
		    MT6357_RG_VPROC_FPWM_ADDR, MT6357_RG_VPROC_FPWM_SHIFT,
		    MT6357_RG_INT_MASK_VPROC_OC_ADDR, MT6357_RG_INT_MASK_VPROC_OC_SHIFT),
	MT6357_BUCK("buck_vpa", VPA, 500000, 3650000, 50000, 0,
		    mt_volt_range4, MT6357_RG_BUCK_VPA_EN_ADDR,
		    MT6357_DA_VPA_EN_ADDR, MT6357_DA_VPA_VOSEL_ADDR,
		    MT6357_DA_VPA_VOSEL_MASK, MT6357_DA_VPA_VOSEL_SHIFT,
		    MT6357_RG_BUCK_VPA_VOSEL_ADDR,
		    MT6357_RG_BUCK_VPA_VOSEL_MASK <<
		    MT6357_RG_BUCK_VPA_VOSEL_SHIFT,
		    MT6357_RG_VPA_MODESET_ADDR, MT6357_RG_VPA_MODESET_SHIFT,
		    MT6357_RG_INT_MASK_VPA_OC_ADDR, MT6357_RG_INT_MASK_VPA_OC_SHIFT),
	MT6357_REG_FIXED("ldo_vfe28", VFE28, MT6357_RG_LDO_VFE28_EN_ADDR,
			 MT6357_DA_VFE28_EN_ADDR, 2800000,
			 MT6357_RG_INT_MASK_VFE28_OC_ADDR, MT6357_RG_INT_MASK_VFE28_OC_SHIFT),
	MT6357_LDO("ldo_vxo22", VXO22, vxo22_voltages, vxo22_idx,
		   MT6357_RG_LDO_VXO22_EN_ADDR, MT6357_RG_LDO_VXO22_EN_SHIFT,
		   MT6357_DA_VXO22_EN_ADDR, MT6357_RG_VXO22_VOSEL_ADDR,
		   MT6357_RG_VXO22_VOSEL_MASK << MT6357_RG_VXO22_VOSEL_SHIFT,
		   MT6357_RG_VXO22_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VXO22_OC_ADDR, MT6357_RG_INT_MASK_VXO22_OC_SHIFT),
	MT6357_REG_FIXED("ldo_vrf18", VRF18, MT6357_RG_LDO_VRF18_EN_ADDR,
			 MT6357_DA_VRF18_EN_ADDR, 1800000,
			 MT6357_RG_INT_MASK_VRF18_OC_ADDR, MT6357_RG_INT_MASK_VRF18_OC_SHIFT),
	MT6357_REG_FIXED("ldo_vrf12", VRF12, MT6357_RG_LDO_VRF12_EN_ADDR,
			 MT6357_DA_VRF12_EN_ADDR, 1200000,
			 MT6357_RG_INT_MASK_VRF12_OC_ADDR, MT6357_RG_INT_MASK_VRF12_OC_SHIFT),
	MT6357_LDO("ldo_vefuse", VEFUSE, vefuse_voltages, vefuse_idx,
		   MT6357_RG_LDO_VEFUSE_EN_ADDR, MT6357_RG_LDO_VEFUSE_EN_SHIFT,
		   MT6357_DA_VEFUSE_EN_ADDR, MT6357_RG_VEFUSE_VOSEL_ADDR,
		   MT6357_RG_VEFUSE_VOSEL_MASK <<
		   MT6357_RG_VEFUSE_VOSEL_SHIFT,
		   MT6357_RG_VEFUSE_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VEFUSE_OC_ADDR, MT6357_RG_INT_MASK_VEFUSE_OC_SHIFT),
	MT6357_LDO("ldo_vcn33_bt", VCN33_BT, vcn33_bt_voltages, vcn33_bt_idx,
		   MT6357_RG_LDO_VCN33_EN_0_ADDR,
		   MT6357_RG_LDO_VCN33_EN_0_SHIFT,
		   MT6357_DA_VCN33_EN_ADDR, MT6357_RG_VCN33_VOSEL_ADDR,
		   MT6357_RG_VCN33_VOSEL_MASK <<
		   MT6357_RG_VCN33_VOSEL_SHIFT,
		   MT6357_RG_VCN33_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VCN33_OC_ADDR, MT6357_RG_INT_MASK_VCN33_OC_SHIFT),
	MT6357_LDO("ldo_vcn33_wifi", VCN33_WIFI, vcn33_wifi_voltages,
		   vcn33_wifi_idx, MT6357_RG_LDO_VCN33_EN_1_ADDR,
		   MT6357_RG_LDO_VCN33_EN_1_SHIFT,
		   MT6357_DA_VCN33_EN_ADDR,
		   MT6357_RG_VCN33_VOSEL_ADDR,
		   MT6357_RG_VCN33_VOSEL_MASK <<
		   MT6357_RG_VCN33_VOSEL_SHIFT,
		   MT6357_RG_VCN33_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VCN33_OC_ADDR, MT6357_RG_INT_MASK_VCN33_OC_SHIFT),
	MT6357_REG_FIXED("ldo_vcn28", VCN28, MT6357_RG_LDO_VCN28_EN_ADDR,
			 MT6357_DA_VCN28_EN_ADDR, 2800000,
			 MT6357_RG_INT_MASK_VCN28_OC_ADDR, MT6357_RG_INT_MASK_VCN28_OC_SHIFT),
	MT6357_REG_FIXED("ldo_vcn18", VCN18, MT6357_RG_LDO_VCN18_EN_ADDR,
			 MT6357_DA_VCN18_EN_ADDR, 1800000,
			 MT6357_RG_INT_MASK_VCN18_OC_ADDR, MT6357_RG_INT_MASK_VCN18_OC_SHIFT),
	MT6357_LDO("ldo_vcama", VCAMA, vcama_voltages, vcama_idx,
		   MT6357_RG_LDO_VCAMA_EN_ADDR, MT6357_RG_LDO_VCAMA_EN_SHIFT,
		   MT6357_DA_VCAMA_EN_ADDR, MT6357_RG_VCAMA_VOSEL_ADDR,
		   MT6357_RG_VCAMA_VOSEL_MASK << MT6357_RG_VCAMA_VOSEL_SHIFT,
		   MT6357_RG_VCAMA_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VCAMA_OC_ADDR, MT6357_RG_INT_MASK_VCAMA_OC_SHIFT),
	MT6357_LDO("ldo_vcamd", VCAMD, vcamd_voltages, vcamd_idx,
		   MT6357_RG_LDO_VCAMD_EN_ADDR, MT6357_RG_LDO_VCAMD_EN_SHIFT,
		   MT6357_DA_VCAMD_EN_ADDR, MT6357_RG_VCAMD_VOSEL_ADDR,
		   MT6357_RG_VCAMD_VOSEL_MASK << MT6357_RG_VCAMD_VOSEL_SHIFT,
		   MT6357_RG_VCAMD_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VCAMD_OC_ADDR, MT6357_RG_INT_MASK_VCAMD_OC_SHIFT),
	MT6357_REG_FIXED("ldo_vcamio", VCAMIO, MT6357_RG_LDO_VCAMIO_EN_ADDR,
			 MT6357_DA_VCAMIO_EN_ADDR, 1800000,
			 MT6357_RG_INT_MASK_VCAMIO_OC_ADDR, MT6357_RG_INT_MASK_VCAMIO_OC_SHIFT),
	MT6357_LDO("ldo_vldo28", VLDO28, vldo28_voltages, vldo28_idx,
		   MT6357_RG_LDO_VLDO28_EN_0_ADDR,
		   MT6357_RG_LDO_VLDO28_EN_1_SHIFT,
		   MT6357_DA_VLDO28_EN_ADDR, MT6357_RG_VLDO28_VOSEL_ADDR,
		   MT6357_RG_VLDO28_VOSEL_MASK << MT6357_RG_VLDO28_VOSEL_SHIFT,
		   MT6357_RG_VLDO28_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VLDO28_OC_ADDR, MT6357_RG_INT_MASK_VLDO28_OC_SHIFT),
	MT6357_LDO_LINEAR("ldo_vsram_others", VSRAM_OTHERS, 518750, 1312500,
			  6250, 0, mt_volt_range2,
			  MT6357_RG_LDO_VSRAM_OTHERS_EN_ADDR,
			  MT6357_DA_VSRAM_OTHERS_EN_ADDR,
			  MT6357_DA_VSRAM_OTHERS_VOSEL_ADDR,
			  MT6357_DA_VSRAM_OTHERS_VOSEL_MASK,
			  MT6357_DA_VSRAM_OTHERS_VOSEL_SHIFT,
			  MT6357_RG_LDO_VSRAM_OTHERS_VOSEL_ADDR,
			  MT6357_RG_LDO_VSRAM_OTHERS_VOSEL_MASK <<
			  MT6357_RG_LDO_VSRAM_OTHERS_VOSEL_SHIFT,
			  MT6357_RG_INT_MASK_VSRAM_OTHERS_OC_ADDR,
			  MT6357_RG_INT_MASK_VSRAM_OTHERS_OC_SHIFT),
	MT6357_LDO_LINEAR("ldo_vsram_proc", VSRAM_PROC, 518750, 1312500, 6250,
			  0, mt_volt_range2, MT6357_RG_LDO_VSRAM_PROC_EN_ADDR,
			  MT6357_DA_VSRAM_PROC_EN_ADDR,
			  MT6357_DA_VSRAM_PROC_VOSEL_ADDR,
			  MT6357_DA_VSRAM_PROC_VOSEL_MASK,
			  MT6357_DA_VSRAM_PROC_VOSEL_SHIFT,
			  MT6357_RG_LDO_VSRAM_PROC_VOSEL_ADDR,
			  MT6357_RG_LDO_VSRAM_PROC_VOSEL_MASK <<
			  MT6357_RG_LDO_VSRAM_PROC_VOSEL_SHIFT,
			  MT6357_RG_INT_MASK_VSRAM_PROC_OC_ADDR,
			  MT6357_RG_INT_MASK_VSRAM_PROC_OC_SHIFT),
	MT6357_REG_FIXED("ldo_vaux18", VAUX18, MT6357_RG_LDO_VAUX18_EN_ADDR,
			 MT6357_DA_VAUX18_EN_ADDR, 1800000,
			 MT6357_RG_INT_MASK_VAUX18_OC_ADDR, MT6357_RG_INT_MASK_VAUX18_OC_SHIFT),
	MT6357_REG_FIXED("ldo_vaud28", VAUD28, MT6357_RG_LDO_VAUD28_EN_ADDR,
			 MT6357_DA_VAUD28_EN_ADDR, 2800000,
			 MT6357_RG_INT_MASK_VAUD28_OC_ADDR, MT6357_RG_INT_MASK_VAUD28_OC_SHIFT),
	MT6357_REG_FIXED("ldo_vio28", VIO28, MT6357_RG_LDO_VIO28_EN_ADDR,
			 MT6357_DA_VIO28_EN_ADDR, 2800000,
			 MT6357_RG_INT_MASK_VIO28_OC_ADDR, MT6357_RG_INT_MASK_VIO28_OC_SHIFT),
	MT6357_REG_FIXED("ldo_vio18", VIO18, MT6357_RG_LDO_VIO18_EN_ADDR,
			 MT6357_DA_VIO18_EN_ADDR, 1800000,
			 MT6357_RG_INT_MASK_VIO18_OC_ADDR, MT6357_RG_INT_MASK_VIO18_OC_SHIFT),
	MT6357_LDO("ldo_vdram", VDRAM, vdram_voltages, vdram_idx,
		   MT6357_RG_LDO_VDRAM_EN_ADDR, MT6357_RG_LDO_VDRAM_EN_SHIFT,
		   MT6357_DA_VDRAM_EN_ADDR, MT6357_RG_VDRAM_VOSEL_ADDR,
		   MT6357_RG_VDRAM_VOSEL_MASK << MT6357_RG_VDRAM_VOSEL_SHIFT,
		   MT6357_RG_VDRAM_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VDRAM_OC_ADDR, MT6357_RG_INT_MASK_VDRAM_OC_SHIFT),
	MT6357_LDO_VMC_DESC("ldo_vmc", VMC, mt_volt_range5, MT6357_RG_LDO_VMC_EN_ADDR,
			    MT6357_DA_VMC_EN_ADDR, MT6357_RG_VMC_VOSEL_ADDR, 0xFFF,
			    MT6357_RG_INT_MASK_VMC_OC_ADDR, MT6357_RG_INT_MASK_VMC_OC_SHIFT),
	MT6357_LDO("ldo_vmch", VMCH, vmch_voltages, vmch_idx,
		   MT6357_RG_LDO_VMCH_EN_ADDR, MT6357_RG_LDO_VMCH_EN_SHIFT,
		   MT6357_DA_VMCH_EN_ADDR, MT6357_RG_VMCH_VOSEL_ADDR,
		   MT6357_RG_VMCH_VOSEL_MASK << MT6357_RG_VMCH_VOSEL_SHIFT,
		   MT6357_RG_VMCH_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VMCH_OC_ADDR, MT6357_RG_INT_MASK_VMCH_OC_SHIFT),
	MT6357_LDO("ldo_vemc", VEMC, vemc_voltages, vemc_idx,
		   MT6357_RG_LDO_VEMC_EN_ADDR, MT6357_RG_LDO_VEMC_EN_SHIFT,
		   MT6357_DA_VEMC_EN_ADDR, MT6357_RG_VEMC_VOSEL_ADDR,
		   MT6357_RG_VEMC_VOSEL_MASK << MT6357_RG_VEMC_VOSEL_SHIFT,
		   MT6357_RG_VEMC_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VEMC_OC_ADDR, MT6357_RG_INT_MASK_VEMC_OC_SHIFT),
	MT6357_LDO("ldo_vsim1", VSIM1, vsim1_voltages, vsim1_idx,
		   MT6357_RG_LDO_VSIM1_EN_ADDR, MT6357_RG_LDO_VSIM1_EN_SHIFT,
		   MT6357_DA_VSIM1_EN_ADDR, MT6357_RG_VSIM1_VOSEL_ADDR,
		   MT6357_RG_VSIM1_VOSEL_MASK << MT6357_RG_VSIM1_VOSEL_SHIFT,
		   MT6357_RG_VSIM1_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VSIM1_OC_ADDR, MT6357_RG_INT_MASK_VSIM1_OC_SHIFT),
	MT6357_LDO("ldo_vsim2", VSIM2, vsim2_voltages, vsim2_idx,
		   MT6357_RG_LDO_VSIM2_EN_ADDR, MT6357_RG_LDO_VSIM2_EN_SHIFT,
		   MT6357_DA_VSIM2_EN_ADDR, MT6357_RG_VSIM2_VOSEL_ADDR,
		   MT6357_RG_VSIM2_VOSEL_MASK << MT6357_RG_VSIM2_VOSEL_SHIFT,
		   MT6357_RG_VSIM2_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VSIM2_OC_ADDR, MT6357_RG_INT_MASK_VSIM2_OC_SHIFT),
	MT6357_LDO("ldo_vibr", VIBR, vibr_voltages, vibr_idx,
		   MT6357_RG_LDO_VIBR_EN_ADDR, MT6357_RG_LDO_VIBR_EN_SHIFT,
		   MT6357_DA_VIBR_EN_ADDR, MT6357_RG_VIBR_VOSEL_ADDR,
		   MT6357_RG_VIBR_VOSEL_MASK << MT6357_RG_VIBR_VOSEL_SHIFT,
		   MT6357_RG_VIBR_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VIBR_OC_ADDR, MT6357_RG_INT_MASK_VIBR_OC_SHIFT),
	MT6357_LDO("ldo_vusb33", VUSB33, vusb33_voltages, vusb33_idx,
		   MT6357_RG_LDO_VUSB33_EN_0_ADDR,
		   MT6357_RG_LDO_VUSB33_EN_0_SHIFT,
		   MT6357_DA_VUSB33_EN_ADDR, MT6357_RG_VUSB33_VOSEL_ADDR,
		   MT6357_RG_VUSB33_VOSEL_MASK << MT6357_RG_VUSB33_VOSEL_SHIFT,
		   MT6357_RG_VUSB33_VOSEL_SHIFT,
		   MT6357_RG_INT_MASK_VUSB33_OC_ADDR, MT6357_RG_INT_MASK_VUSB33_OC_SHIFT),
};

static unsigned int is_mt6357_pmic_mrv(struct regmap *regmap)
{
	unsigned int is_mrv = 0;

	regmap_read(regmap, MT6357_RG_TOP2_RSV0_ADDR, &is_mrv);
	is_mrv = (is_mrv & 0x8000) >> 15;

	return is_mrv;
}

static void mt6357_oc_irq_enable_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6357_regulator_info *info
		= container_of(dwork, struct mt6357_regulator_info, oc_work);

	enable_irq(info->irq);
}

static irqreturn_t mt6357_oc_irq(int irq, void *data)
{
	struct regulator_dev *rdev = (struct regulator_dev *)data;
	struct mt6357_regulator_info *info = rdev_get_drvdata(rdev);

	disable_irq_nosync(info->irq);
	if (!regulator_is_enabled_regmap(rdev))
		goto delayed_enable;
	mutex_lock(&rdev->mutex);
	regulator_notifier_call_chain(rdev, REGULATOR_EVENT_OVER_CURRENT,
				      NULL);
	mutex_unlock(&rdev->mutex);
delayed_enable:
	schedule_delayed_work(&info->oc_work,
			      msecs_to_jiffies(info->oc_irq_enable_delay_ms));
	return IRQ_HANDLED;
}

static int mt6357_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config)
{
	int ret = 0;
	struct mt6357_regulator_info *info = config->driver_data;

	ret = of_property_read_u32(np, "mediatek,oc-irq-enable-delay-ms",
				   &info->oc_irq_enable_delay_ms);
	if (ret || !info->oc_irq_enable_delay_ms)
		info->oc_irq_enable_delay_ms = DEF_OC_IRQ_ENABLE_DELAY_MS;

	return 0;
}

static int mt6357_regulator_probe(struct platform_device *pdev)
{
	struct mt6397_chip *mt6397 = dev_get_drvdata(pdev->dev.parent);
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	int i = 0, ret = 0;

	for (i = 0; i < MT6357_MAX_REGULATOR; i++) {
		/* Workaround setting for MT6357 MRV */
		if (is_mt6357_pmic_mrv(mt6397->regmap)) {
			if (strncmp(mt6357_regulators[i].desc.name,
				    "VSRAM_OTHERS", 12) == 0) {
				mt6357_regulators[i].desc.enable_reg =
					MT6357_RG_LDO_VSRAM_PROC_EN_ADDR;
				mt6357_regulators[i].status_reg =
					MT6357_DA_VSRAM_PROC_EN_ADDR;
				mt6357_regulators[i].da_vsel_reg =
					MT6357_DA_VSRAM_PROC_VOSEL_ADDR;
				mt6357_regulators[i].da_vsel_mask =
					MT6357_DA_VSRAM_PROC_VOSEL_MASK;
				mt6357_regulators[i].da_vsel_shift =
					MT6357_DA_VSRAM_PROC_VOSEL_SHIFT;
				mt6357_regulators[i].desc.vsel_reg =
					MT6357_RG_LDO_VSRAM_PROC_VOSEL_ADDR;
				mt6357_regulators[i].desc.vsel_mask =
					MT6357_RG_LDO_VSRAM_PROC_VOSEL_MASK <<
					MT6357_RG_LDO_VSRAM_PROC_VOSEL_SHIFT;
			} else if (strncmp(mt6357_regulators[i].desc.name,
					   "VSRAM_PROC", 10) == 0) {
				mt6357_regulators[i].desc.enable_reg =
					MT6357_RG_LDO_VSRAM_OTHERS_EN_ADDR;
				mt6357_regulators[i].status_reg =
					MT6357_DA_VSRAM_OTHERS_EN_ADDR;
				mt6357_regulators[i].da_vsel_reg =
					MT6357_DA_VSRAM_OTHERS_VOSEL_ADDR;
				mt6357_regulators[i].da_vsel_mask =
					MT6357_DA_VSRAM_OTHERS_VOSEL_MASK;
				mt6357_regulators[i].da_vsel_shift =
					MT6357_DA_VSRAM_OTHERS_VOSEL_SHIFT;
				mt6357_regulators[i].desc.vsel_reg =
					MT6357_RG_LDO_VSRAM_OTHERS_VOSEL_ADDR;
				mt6357_regulators[i].desc.vsel_mask =
					MT6357_RG_LDO_VSRAM_OTHERS_VOSEL_MASK <<
					MT6357_RG_LDO_VSRAM_OTHERS_VOSEL_SHIFT;
			}
		}
		mt6357_regulators[i].desc.of_parse_cb = mt6357_of_parse_cb;
		config.dev = &pdev->dev;
		config.driver_data = &mt6357_regulators[i];
		config.regmap = mt6397->regmap;

		rdev = devm_regulator_register(&pdev->dev,
					       &mt6357_regulators[i].desc,
					       &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev, "failed to register %s\n",
				mt6357_regulators[i].desc.name);
			return PTR_ERR(rdev);
		}
		mt6357_regulators[i].irq =
			platform_get_irq_byname(pdev,
						mt6357_regulators[i].desc.name);
		if (mt6357_regulators[i].irq < 0)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev,
						mt6357_regulators[i].irq, NULL,
						mt6357_oc_irq,
						IRQF_TRIGGER_HIGH,
						mt6357_regulators[i].desc.name,
						rdev);
		if (ret) {
			dev_notice(&pdev->dev, "Failed to request IRQ:%s,%d",
				   mt6357_regulators[i].desc.name, ret);
			continue;
		}
		INIT_DELAYED_WORK(&mt6357_regulators[i].oc_work,
				  mt6357_oc_irq_enable_work);
	}

	return 0;
}

static const struct platform_device_id mt6357_platform_ids[] = {
	{"mt6357-regulator", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mt6357_platform_ids);

static struct platform_driver mt6357_regulator_driver = {
	.driver = {
		.name = "mt6357-regulator",
	},
	.probe = mt6357_regulator_probe,
	.id_table = mt6357_platform_ids,
};

module_platform_driver(mt6357_regulator_driver);

MODULE_AUTHOR("Wen Su <wen.su@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6357 PMIC");
MODULE_LICENSE("GPL");
