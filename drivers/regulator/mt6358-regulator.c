// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2019 MediaTek Inc.

#include <linux/interrupt.h>
#include <linux/mfd/mt6358/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6358-regulator.h>
#include <linux/regulator/of_regulator.h>

#define MT6358_BUCK_MODE_AUTO	0
#define MT6358_BUCK_MODE_FORCE_PWM	1

#define DEF_OC_IRQ_ENABLE_DELAY_MS	10

/*
 * MT6358 regulators' information
 *
 * @desc: standard fields of regulator description.
 * @qi: Mask for query enable signal status of regulators
 */
struct mt6358_regulator_info {
	int irq;
	int oc_irq_enable_delay_ms;
	struct delayed_work oc_work;
	struct regulator_desc desc;
	u32 status_reg;
	u32 qi;
	u32 vsel_shift;
	u32 da_vsel_reg;
	u32 da_vsel_mask;
	u32 da_vsel_shift;
	u32 modeset_reg;
	u32 modeset_mask;
	u32 modeset_shift;
};

#define MT6358_BUCK(match, vreg, min, max, step,		\
	volt_ranges, vosel_mask, _da_vsel_reg, _da_vsel_mask,	\
	_da_vsel_shift, _modeset_reg, _modeset_shift)		\
[MT6358_ID_##vreg] = {	\
	.desc = {	\
		.name = #vreg,	\
		.of_match = of_match_ptr(match),	\
		.of_parse_cb = mt6358_of_parse_cb,	\
		.ops = &mt6358_volt_range_ops,	\
		.type = REGULATOR_VOLTAGE,	\
		.id = MT6358_ID_##vreg,		\
		.owner = THIS_MODULE,		\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.linear_ranges = volt_ranges,		\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.vsel_reg = MT6358_BUCK_##vreg##_ELR0,	\
		.vsel_mask = vosel_mask,	\
		.enable_reg = MT6358_BUCK_##vreg##_CON0,	\
		.enable_mask = BIT(0),	\
		.of_map_mode = mt6358_map_mode,	\
	},	\
	.status_reg = MT6358_BUCK_##vreg##_DBG1,	\
	.qi = BIT(0),	\
	.da_vsel_reg = _da_vsel_reg,	\
	.da_vsel_mask = _da_vsel_mask,	\
	.da_vsel_shift = _da_vsel_shift,	\
	.modeset_reg = _modeset_reg,	\
	.modeset_mask = BIT(_modeset_shift),	\
	.modeset_shift = _modeset_shift	\
}

#define MT6358_SSHUB(match, vreg, min, max, step,	\
	volt_ranges, _enable_reg, _status_reg,		\
	_da_vsel_reg, _da_vsel_mask, _da_vsel_shift,	\
	_vsel_reg, _vsel_mask)				\
[MT6358_ID_##vreg] = {	\
	.desc = {	\
		.name = #vreg,					\
		.of_match = of_match_ptr(match),		\
		.ops = &mt6358_sshub_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6358_ID_##vreg,				\
		.owner = THIS_MODULE,				\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.linear_ranges = volt_ranges,			\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.vsel_reg = _vsel_reg,		\
		.vsel_mask = _vsel_mask,	\
		.enable_reg = _enable_reg,	\
		.enable_mask = BIT(0),		\
	},				\
	.status_reg = _status_reg,	\
	.qi = BIT(0),			\
	.da_vsel_reg = _da_vsel_reg,	\
	.da_vsel_mask = _da_vsel_mask,	\
	.da_vsel_shift = _da_vsel_shift,\
}

#define MT6358_LDO(match, vreg, ldo_volt_table,	\
	enreg, enbit, vosel, vosel_mask, vosel_shift) \
[MT6358_ID_##vreg] = {	\
	.desc = {	\
		.name = #vreg,	\
		.of_match = of_match_ptr(match),	\
		.of_parse_cb = mt6358_of_parse_cb,	\
		.ops = &mt6358_volt_table_ops,	\
		.type = REGULATOR_VOLTAGE,	\
		.id = MT6358_ID_##vreg,	\
		.owner = THIS_MODULE,	\
		.n_voltages = ARRAY_SIZE(ldo_volt_table),	\
		.volt_table = ldo_volt_table,	\
		.vsel_reg = vosel,	\
		.vsel_mask = vosel_mask,	\
		.enable_reg = enreg,	\
		.enable_mask = BIT(enbit),	\
	},	\
	.status_reg = MT6358_LDO_##vreg##_CON1,	\
	.qi = BIT(15),	\
	.vsel_shift = vosel_shift,	\
}

#define MT6358_LDO1(match, vreg, min, max, step,	\
	volt_ranges, _da_vsel_reg, _da_vsel_mask,	\
	_da_vsel_shift, vosel, vosel_mask)	\
[MT6358_ID_##vreg] = {	\
	.desc = {	\
		.name = #vreg,	\
		.of_match = of_match_ptr(match),	\
		.of_parse_cb = mt6358_of_parse_cb,	\
		.ops = &mt6358_volt_range_ops,	\
		.type = REGULATOR_VOLTAGE,	\
		.id = MT6358_ID_##vreg,	\
		.owner = THIS_MODULE,	\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.linear_ranges = volt_ranges,	\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),	\
		.vsel_reg = vosel,	\
		.vsel_mask = vosel_mask,	\
		.enable_reg = MT6358_LDO_##vreg##_CON0,	\
		.enable_mask = BIT(0),	\
	},	\
	.da_vsel_reg = _da_vsel_reg,	\
	.da_vsel_mask = _da_vsel_mask,	\
	.da_vsel_shift = _da_vsel_shift,	\
	.status_reg = MT6358_LDO_##vreg##_DBG1,	\
	.qi = BIT(0),	\
}

#define MT6358_REG_FIXED(match, vreg, enreg, enbit, volt) \
[MT6358_ID_##vreg] = {	\
	.desc = {	\
		.name = #vreg,	\
		.of_match = of_match_ptr(match),	\
		.of_parse_cb = mt6358_of_parse_cb,	\
		.ops = &mt6358_volt_fixed_ops,	\
		.type = REGULATOR_VOLTAGE,	\
		.id = MT6358_ID_##vreg,	\
		.owner = THIS_MODULE,	\
		.n_voltages = 1,	\
		.enable_reg = enreg,	\
		.enable_mask = BIT(enbit),	\
		.min_uV = volt,	\
	},	\
	.status_reg = MT6358_LDO_##vreg##_CON1,	\
	.qi = BIT(15),							\
}

#define MT6358_LDO_VMC_DESC(match, _name, volt_ranges,	\
	_enable_reg, _da_reg, _vsel_reg, _vsel_mask)\
[MT6358_ID_##_name] = {					\
	.desc = {					\
		.name = #_name,				\
		.of_match = of_match_ptr(match),	\
		.of_parse_cb = mt6358_of_parse_cb,	\
		.ops = &mt6358_vmc_ops,			\
		.type = REGULATOR_VOLTAGE,		\
		.id = MT6358_ID_##_name,		\
		.owner = THIS_MODULE,			\
		.n_voltages = 0xd01,			\
		.linear_ranges = volt_ranges,		\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),\
		.vsel_reg = _vsel_reg,			\
		.vsel_mask = _vsel_mask,		\
		.enable_reg = _enable_reg,		\
		.enable_mask = BIT(0),			\
	},						\
	.status_reg = _da_reg,				\
	.qi = BIT(15),					\
}

#define MT6358_LDO_VA09_DESC(match, _name)		\
[MT6358_ID_##_name] = {					\
	.desc = {					\
		.name = #_name,				\
		.of_match = of_match_ptr(match),	\
		.of_parse_cb = mt6358_of_parse_cb,	\
		.ops = &pmic_regulator_ext2_ops,	\
		.type = REGULATOR_VOLTAGE,		\
		.id = MT6358_ID_##_name,		\
		.owner = THIS_MODULE,			\
		.n_voltages = 1,			\
		.fixed_uV = 900000,			\
	},						\
	.status_reg = MT6358_STRUP_CON5,		\
	.qi = BIT(9),					\
}

#define MT6358_VMCH_EINT(_eint_pol, _volt_table)		\
[MT6358_ID_VMCH_##_eint_pol] = {				\
	.desc = {						\
		.name = "VMCH_"#_eint_pol,			\
		.of_match = of_match_ptr("VMCH_"#_eint_pol),	\
		.of_parse_cb = mt6358_of_parse_cb,		\
		.ops = &mt6358_vmch_eint_ops,			\
		.type = REGULATOR_VOLTAGE,			\
		.id = MT6358_ID_VMCH_##_eint_pol,		\
		.owner = THIS_MODULE,				\
		.n_voltages = ARRAY_SIZE(_volt_table),		\
		.volt_table = _volt_table,			\
		.enable_reg = MT6358_LDO_VMCH_EINT,		\
		.enable_mask = MT6358_PMIC_RG_LDO_VMCH_EINT_EN_MASK,	\
		.vsel_reg = MT6358_VMCH_ANA_CON0,		\
		.vsel_mask = 0x700,				\
	},							\
	.status_reg = MT6358_LDO_VMCH_CON1,	\
	.qi = BIT(15),	\
}

static const struct linear_range buck_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 0x7f, 6250),
};

static const struct linear_range buck_volt_range2[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 0x7f, 12500),
};

static const struct linear_range buck_volt_range3[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 0x3f, 50000),
};

static const struct linear_range buck_volt_range4[] = {
	REGULATOR_LINEAR_RANGE(1000000, 0, 0x7f, 12500),
};

/* for vmc voltage calibration: 1.86V */
static const struct linear_range buck_volt_range5[] = {
	REGULATOR_LINEAR_RANGE(1800000, 0x400, 0x400, 0),
	REGULATOR_LINEAR_RANGE(1810000, 0x401, 0x40a, 10000),
	REGULATOR_LINEAR_RANGE(2900000, 0xa00, 0xa00, 0),
	REGULATOR_LINEAR_RANGE(2910000, 0xa01, 0xa09, 10000),
	REGULATOR_LINEAR_RANGE(3000000, 0xb00, 0xb00, 0),
	REGULATOR_LINEAR_RANGE(3010000, 0xb01, 0xb0a, 10000),
	REGULATOR_LINEAR_RANGE(3300000, 0xd00, 0xd00, 0),
};

static const u32 vdram2_voltages[] = {
	600000,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1800000,
};

static const u32 vsim_voltages[] = {
	0,
	0,
	0,
	1700000,
	1800000,
	0,
	0,
	0,
	2700000,
	0,
	0,
	3000000,
	3100000,
};

static const u32 vibr_voltages[] = {
	1200000,
	1300000,
	1500000,
	0,
	1800000,
	2000000,
	0,
	0,
	0,
	2800000,
	0,
	3000000,
	0,
	3300000,
};

static const u32 vrf12_voltages[] = {
	1200000,
};

static const u32 vio18_voltages[] = {
	1800000,
};

static const u32 vusb_voltages[] = {
	0,
	0,
	0,
	3000000,
	3100000,
};

static const u32 vcamio_voltages[] = {
	1800000,
};

static const u32 vcamd_voltages[] = {
	0,
	0,
	0,
	900000,
	1000000,
	1100000,
	1200000,
	1300000,
	0,
	1500000,
	0,
	0,
	1800000,
};

static const u32 vcn18_voltages[] = {
	1800000,
};

static const u32 vfe28_voltages[] = {
	2800000,
};

static const u32 vcn28_voltages[] = {
	2800000,
};

static const u32 vxo22_voltages[] = {
	2200000,
};

static const u32 vefuse_voltages[] = {
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1700000,
	1800000,
	1900000,
};

static const u32 vaux18_voltages[] = {
	1800000,
};

static const u32 vmch_vemc_voltages[] = {
	0,
	0,
	2900000,
	3000000,
	0,
	3300000,
};

static const u32 vbif28_voltages[] = {
	2800000,
};

static const u32 vcama_voltages[] = {
	1800000,
	0,
	0,
	0,
	0,
	0,
	0,
	2500000,
	0,
	2700000,
	2800000,
	2900000,
	3000000,
};

static const u32 vio28_voltages[] = {
	2800000,
};

static const u32 va12_voltages[] = {
	1200000,
};

static const u32 vrf18_voltages[] = {
	1800000,
};

static const u32 vcn33_bt_wifi_voltages[] = {
	0,
	3300000,
	3400000,
	3500000,
};

static const u32 vldo28_voltages[] = {
	0,
	2800000,
	0,
	3000000,
};

static const u32 vaud28_voltages[] = {
	2800000,
};

static unsigned int mt6358_map_mode(unsigned int mode)
{
	return mode == MT6358_BUCK_MODE_AUTO ?
		REGULATOR_MODE_NORMAL : REGULATOR_MODE_FAST;
}

static int mt6358_get_buck_voltage_sel(struct regulator_dev *rdev)
{
	int ret, regval = 0;
	struct mt6358_regulator_info *info = rdev_get_drvdata(rdev);

	ret = regmap_read(rdev->regmap, info->da_vsel_reg, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev,
			"Failed to get mt6358 Buck %s vsel reg: %d\n",
			info->desc.name, ret);
		return ret;
	}

	ret = (regval >> info->da_vsel_shift) & info->da_vsel_mask;

	return ret;
}

static int mt6358_get_status(struct regulator_dev *rdev)
{
	int ret;
	u32 regval = 0;
	struct mt6358_regulator_info *info = rdev_get_drvdata(rdev);

	ret = regmap_read(rdev->regmap, info->status_reg, &regval);
	if (ret != 0) {
		dev_info(&rdev->dev, "Failed to get enable reg: %d\n", ret);
		return ret;
	}

	return (regval & info->qi) ? REGULATOR_STATUS_ON : REGULATOR_STATUS_OFF;
}

static int mt6358_regulator_set_mode(struct regulator_dev *rdev,
				     unsigned int mode)
{
	struct mt6358_regulator_info *info = rdev_get_drvdata(rdev);
	int val;

	switch (mode) {
	case REGULATOR_MODE_FAST:
		val = MT6358_BUCK_MODE_FORCE_PWM;
		break;
	case REGULATOR_MODE_NORMAL:
		val = MT6358_BUCK_MODE_AUTO;
		break;
	default:
		return -EINVAL;
	}

	dev_dbg(&rdev->dev, "mt6358 buck set_mode %#x, %#x, %#x, %#x\n",
		info->modeset_reg, info->modeset_mask,
		info->modeset_shift, val);

	val <<= info->modeset_shift;

	return regmap_update_bits(rdev->regmap, info->modeset_reg,
				  info->modeset_mask, val);
}

static unsigned int mt6358_regulator_get_mode(struct regulator_dev *rdev)
{
	struct mt6358_regulator_info *info = rdev_get_drvdata(rdev);
	int ret, regval = 0;

	ret = regmap_read(rdev->regmap, info->modeset_reg, &regval);
	if (ret != 0) {
		dev_err(&rdev->dev,
			"Failed to get mt6358 buck mode: %d\n", ret);
		return ret;
	}

	switch ((regval & info->modeset_mask) >> info->modeset_shift) {
	case MT6358_BUCK_MODE_AUTO:
		return REGULATOR_MODE_NORMAL;
	case MT6358_BUCK_MODE_FORCE_PWM:
		return REGULATOR_MODE_FAST;
	default:
		return -EINVAL;
	}
}

/* Regulator EXT_PMIC2 enable */
#define PMIC_RG_STRUP_EXT_PMIC_EN_ADDR	MT6358_STRUP_CON5
#define PMIC_RG_STRUP_EXT_PMIC_EN_MASK	0x3
#define PMIC_RG_STRUP_EXT_PMIC_EN_SHIFT 0
#define PMIC_RG_STRUP_EXT_PMIC_SEL_ADDR	MT6358_STRUP_CON5
#define PMIC_RG_STRUP_EXT_PMIC_SEL_MASK 0x3
#define PMIC_RG_STRUP_EXT_PMIC_SEL_SHIFT 4

static int mt6358_regulator_ext2_enable(struct regulator_dev *rdev)
{
	unsigned int ext_en = 0, ext_sel = 0;
	unsigned int ext_en_mask = PMIC_RG_STRUP_EXT_PMIC_EN_MASK <<
				   PMIC_RG_STRUP_EXT_PMIC_EN_SHIFT;
	unsigned int ext_sel_mask = PMIC_RG_STRUP_EXT_PMIC_SEL_MASK <<
				    PMIC_RG_STRUP_EXT_PMIC_SEL_SHIFT;
	int ret = 0;

	dev_info(&rdev->dev, "regulator ext_pmic2 enable\n");
	ret = regmap_read(rdev->regmap,
			  PMIC_RG_STRUP_EXT_PMIC_EN_ADDR, &ext_en);
	if ((ext_en & 0x2) != 0x2) {
		ret = regmap_update_bits(rdev->regmap,
					 PMIC_RG_STRUP_EXT_PMIC_EN_ADDR,
					 ext_en_mask,
					 ext_en | 0x2);
	}
	ret = regmap_read(rdev->regmap,
			  PMIC_RG_STRUP_EXT_PMIC_SEL_ADDR, &ext_sel);
	if ((ext_sel & 0x20) != 0x20) {
		ret = regmap_update_bits(rdev->regmap,
					 PMIC_RG_STRUP_EXT_PMIC_SEL_ADDR,
					 ext_sel_mask,
					 ext_sel | 0x20);
	}
	return ret;
}

/* Regulator EXT_PMIC2 disable */
static int mt6358_regulator_ext2_disable(struct regulator_dev *rdev)
{
	unsigned int ext_en = 0, ext_sel = 0;
	unsigned int ext_en_mask = PMIC_RG_STRUP_EXT_PMIC_EN_MASK <<
				   PMIC_RG_STRUP_EXT_PMIC_EN_SHIFT;
	unsigned int ext_sel_mask = PMIC_RG_STRUP_EXT_PMIC_SEL_MASK <<
				    PMIC_RG_STRUP_EXT_PMIC_SEL_SHIFT;
	int ret = 0;

	dev_info(&rdev->dev, "regulator ext_pmic2 disable\n");
	if (rdev->use_count == 0) {
		dev_notice(&rdev->dev, "%s:%s should not be disable.(use_count=0)\n"
			, __func__
			, rdev->desc->name);
		return -1;
	}
	ret = regmap_read(rdev->regmap,
			  PMIC_RG_STRUP_EXT_PMIC_SEL_ADDR, &ext_sel);
	if ((ext_sel & 0x20) != 0x20) {
		ret = regmap_update_bits(rdev->regmap,
					 PMIC_RG_STRUP_EXT_PMIC_SEL_ADDR,
					 ext_sel_mask,
					 ext_sel | 0x20);
	}
	ret = regmap_read(rdev->regmap,
			  PMIC_RG_STRUP_EXT_PMIC_EN_ADDR, &ext_en);
	if ((ext_en & 0x2) != 0x2) {
		ret = regmap_update_bits(rdev->regmap,
					 PMIC_RG_STRUP_EXT_PMIC_EN_ADDR,
					 ext_en_mask,
					 ext_en & ~0x2);
	}
	return ret;
}

static int mt6358_vmch_eint_enable(struct regulator_dev *rdev)
{
	unsigned int val;
	int ret;

	if (rdev->desc->id == MT6358_ID_VMCH_EINT_HIGH)
		val = MT6358_PMIC_RG_LDO_VMCH_EINT_POL_MASK;
	else
		val = 0;
	ret = regmap_update_bits(rdev->regmap, MT6358_LDO_VMCH_EINT,
				 MT6358_PMIC_RG_LDO_VMCH_EINT_POL_MASK, val);
	if (ret)
		return ret;

	ret = regmap_update_bits(rdev->regmap, MT6358_LDO_VMCH_CON0, BIT(0), BIT(0));
	if (ret)
		return ret;

	ret = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				 rdev->desc->enable_mask, rdev->desc->enable_mask);
	return ret;
}

static int mt6358_vmch_eint_disable(struct regulator_dev *rdev)
{
	int ret;

	ret = regmap_update_bits(rdev->regmap, MT6358_LDO_VMCH_CON0, BIT(0), 0);
	if (ret)
		return ret;

	udelay(1500); /* Must delay for VMCH discharging */
	ret = regmap_update_bits(rdev->regmap, rdev->desc->enable_reg,
				 rdev->desc->enable_mask, 0);
	return ret;
}

static const struct regulator_ops mt6358_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = mt6358_get_buck_voltage_sel,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6358_get_status,
	.set_mode = mt6358_regulator_set_mode,
	.get_mode = mt6358_regulator_get_mode,
};

static const struct regulator_ops mt6358_volt_table_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6358_get_status,
};

static const struct regulator_ops mt6358_volt_fixed_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6358_get_status,
};

/* for sshub */
static const struct regulator_ops mt6358_sshub_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = mt6358_get_buck_voltage_sel,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6358_get_status,
};

/* Regulator EXT_PMIC2 ops */
static const struct regulator_ops pmic_regulator_ext2_ops = {
	.enable = mt6358_regulator_ext2_enable,
	.disable = mt6358_regulator_ext2_disable,
	.is_enabled = mt6358_get_status,
};

static const struct regulator_ops mt6358_vmc_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6358_get_status,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
};

static const struct regulator_ops mt6358_vmch_eint_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = mt6358_vmch_eint_enable,
	.disable = mt6358_vmch_eint_disable,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6358_get_status,
};

static int mt6358_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config);

/* The array is indexed by id(MT6358_ID_XXX) */
static struct mt6358_regulator_info mt6358_regulators[] = {
	MT6358_BUCK("buck_vdram1", VDRAM1, 500000, 2087500, 12500,
		    buck_volt_range2, 0x7f, MT6358_BUCK_VDRAM1_DBG0, 0x7f,
		    0, MT6358_VDRAM1_ANA_CON0, 8),
	MT6358_BUCK("buck_vcore", VCORE, 500000, 1293750, 6250,
		    buck_volt_range1, 0x7f, MT6358_BUCK_VCORE_DBG0, 0x7f,
		    0, MT6358_VCORE_VGPU_ANA_CON0, 1),
	MT6358_BUCK("buck_vpa", VPA, 500000, 3650000, 50000,
		    buck_volt_range3, 0x3f, MT6358_BUCK_VPA_DBG0, 0x3f, 0,
		    MT6358_VPA_ANA_CON0, 3),
	MT6358_BUCK("buck_vproc11", VPROC11, 500000, 1293750, 6250,
		    buck_volt_range1, 0x7f, MT6358_BUCK_VPROC11_DBG0, 0x7f,
		    0, MT6358_VPROC_ANA_CON0, 1),
	MT6358_BUCK("buck_vproc12", VPROC12, 500000, 1293750, 6250,
		    buck_volt_range1, 0x7f, MT6358_BUCK_VPROC12_DBG0, 0x7f,
		    0, MT6358_VPROC_ANA_CON0, 2),
	MT6358_BUCK("buck_vgpu", VGPU, 500000, 1293750, 6250,
		    buck_volt_range1, 0x7f, MT6358_BUCK_VGPU_ELR0, 0x7f, 0,
		    MT6358_VCORE_VGPU_ANA_CON0, 2),
	MT6358_BUCK("buck_vs2", VS2, 500000, 2087500, 12500,
		    buck_volt_range2, 0x7f, MT6358_BUCK_VS2_DBG0, 0x7f, 0,
		    MT6358_VS2_ANA_CON0, 8),
	MT6358_BUCK("buck_vmodem", VMODEM, 500000, 1293750, 6250,
		    buck_volt_range1, 0x7f, MT6358_BUCK_VMODEM_DBG0, 0x7f,
		    0, MT6358_VMODEM_ANA_CON0, 8),
	MT6358_BUCK("buck_vs1", VS1, 1000000, 2587500, 12500,
		    buck_volt_range4, 0x7f, MT6358_BUCK_VS1_DBG0, 0x7f, 0,
		    MT6358_VS1_ANA_CON0, 8),
	MT6358_SSHUB("buck_vcore_sshub", VCORE_SSHUB, 500000, 1293750, 6250,
		     buck_volt_range1, MT6358_BUCK_VCORE_SSHUB_CON0, MT6358_BUCK_VCORE_DBG1,
		     MT6358_BUCK_VCORE_DBG0, 0x7f, 0, MT6358_BUCK_VCORE_SSHUB_CON1, 0x7f),
	MT6358_SSHUB("ldo_vsram_others_sshub", VSRAM_OTHERS_SSHUB, 500000, 1293750, 6250,
		     buck_volt_range1, MT6358_LDO_VSRAM_OTHERS_SSHUB_CON0,
		     MT6358_LDO_VSRAM_OTHERS_DBG1, MT6358_LDO_VSRAM_OTHERS_DBG0, 0x7f, 8,
		     MT6358_LDO_VSRAM_OTHERS_SSHUB_CON1, 0x7f),
	MT6358_REG_FIXED("ldo_vrf12", VRF12, MT6358_LDO_VRF12_CON0, 0, 1200000),
	MT6358_REG_FIXED("ldo_vio18", VIO18, MT6358_LDO_VIO18_CON0, 0, 1800000),
	MT6358_REG_FIXED("ldo_vcamio", VCAMIO, MT6358_LDO_VCAMIO_CON0, 0, 1800000),
	MT6358_REG_FIXED("ldo_vcn18", VCN18, MT6358_LDO_VCN18_CON0, 0, 1800000),
	MT6358_REG_FIXED("ldo_vfe28", VFE28, MT6358_LDO_VFE28_CON0, 0, 2800000),
	MT6358_REG_FIXED("ldo_vcn28", VCN28, MT6358_LDO_VCN28_CON0, 0, 2800000),
	MT6358_REG_FIXED("ldo_vxo22", VXO22, MT6358_LDO_VXO22_CON0, 0, 2200000),
	MT6358_REG_FIXED("ldo_vaux18", VAUX18, MT6358_LDO_VAUX18_CON0, 0, 1800000),
	MT6358_REG_FIXED("ldo_vbif28", VBIF28, MT6358_LDO_VBIF28_CON0, 0, 2800000),
	MT6358_REG_FIXED("ldo_vio28", VIO28, MT6358_LDO_VIO28_CON0, 0, 2800000),
	MT6358_REG_FIXED("ldo_va12", VA12, MT6358_LDO_VA12_CON0, 0, 1200000),
	MT6358_REG_FIXED("ldo_vrf18", VRF18, MT6358_LDO_VRF18_CON0, 0, 1800000),
	MT6358_REG_FIXED("ldo_vaud28", VAUD28, MT6358_LDO_VAUD28_CON0, 0, 2800000),
	MT6358_LDO("ldo_vdram2", VDRAM2, vdram2_voltages,
		   MT6358_LDO_VDRAM2_CON0, 0, MT6358_LDO_VDRAM2_ELR0, 0xf, 0),
	MT6358_LDO("ldo_vsim1", VSIM1, vsim_voltages,
		   MT6358_LDO_VSIM1_CON0, 0, MT6358_VSIM1_ANA_CON0, 0xf00, 8),
	MT6358_LDO("ldo_vibr", VIBR, vibr_voltages,
		   MT6358_LDO_VIBR_CON0, 0, MT6358_VIBR_ANA_CON0, 0xf00, 8),
	MT6358_LDO("ldo_vusb", VUSB, vusb_voltages,
		   MT6358_LDO_VUSB_CON0_0, 0, MT6358_VUSB_ANA_CON0, 0x700, 8),
	MT6358_LDO("ldo_vcamd", VCAMD, vcamd_voltages,
		   MT6358_LDO_VCAMD_CON0, 0, MT6358_VCAMD_ANA_CON0, 0xf00, 8),
	MT6358_LDO("ldo_vefuse", VEFUSE, vefuse_voltages,
		   MT6358_LDO_VEFUSE_CON0, 0, MT6358_VEFUSE_ANA_CON0, 0xf00, 8),
	MT6358_LDO("ldo_vmch", VMCH, vmch_vemc_voltages,
		   MT6358_LDO_VMCH_CON0, 0, MT6358_VMCH_ANA_CON0, 0x700, 8),
	MT6358_LDO("ldo_vcama1", VCAMA1, vcama_voltages,
		   MT6358_LDO_VCAMA1_CON0, 0, MT6358_VCAMA1_ANA_CON0, 0xf00, 8),
	MT6358_LDO("ldo_vemc", VEMC, vmch_vemc_voltages,
		   MT6358_LDO_VEMC_CON0, 0, MT6358_VEMC_ANA_CON0, 0x700, 8),
	MT6358_LDO("ldo_vcn33_bt", VCN33_BT, vcn33_bt_wifi_voltages,
		   MT6358_LDO_VCN33_CON0_0, 0, MT6358_VCN33_ANA_CON0, 0x300, 8),
	MT6358_LDO("ldo_vcn33_wifi", VCN33_WIFI, vcn33_bt_wifi_voltages,
		   MT6358_LDO_VCN33_CON0_1, 0, MT6358_VCN33_ANA_CON0, 0x300, 8),
	MT6358_LDO("ldo_vcama2", VCAMA2, vcama_voltages,
		   MT6358_LDO_VCAMA2_CON0, 0, MT6358_VCAMA2_ANA_CON0, 0xf00, 8),
	MT6358_LDO("ldo_vldo28", VLDO28, vldo28_voltages,
		   MT6358_LDO_VLDO28_CON0_0, 0, MT6358_VLDO28_ANA_CON0, 0x300, 8),
	MT6358_LDO("ldo_vsim2", VSIM2, vsim_voltages,
		   MT6358_LDO_VSIM2_CON0, 0, MT6358_VSIM2_ANA_CON0, 0xf00, 8),
	MT6358_LDO1("ldo_vsram_proc11", VSRAM_PROC11, 500000, 1293750, 6250,
		    buck_volt_range1, MT6358_LDO_VSRAM_PROC11_DBG0, 0x7f, 8,
		    MT6358_LDO_VSRAM_CON0, 0x7f),
	MT6358_LDO1("ldo_vsram_others", VSRAM_OTHERS, 500000, 1293750, 6250,
		    buck_volt_range1, MT6358_LDO_VSRAM_OTHERS_DBG0, 0x7f, 8,
		    MT6358_LDO_VSRAM_CON2, 0x7f),
	MT6358_LDO1("ldo_vsram_gpu", VSRAM_GPU, 500000, 1293750, 6250,
		    buck_volt_range1, MT6358_LDO_VSRAM_GPU_DBG0, 0x7f, 8,
		    MT6358_LDO_VSRAM_CON3, 0x7f),
	MT6358_LDO1("ldo_vsram_proc12", VSRAM_PROC12, 500000, 1293750, 6250,
		    buck_volt_range1, MT6358_LDO_VSRAM_PROC12_DBG0, 0x7f, 8,
		    MT6358_LDO_VSRAM_CON1, 0x7f),
	MT6358_LDO1("ldo_vsram_core", VSRAM_CORE, 500000, 1293750, 6250,
		    buck_volt_range1, MT6358_LDO_VSRAM_CORE_DBG0, 0x7f, 8,
		    MT6358_LDO_VSRAM_CON5, 0x7f),
	MT6358_LDO_VMC_DESC("ldo_vmc", VMC, buck_volt_range5, MT6358_LDO_VMC_CON0,
			    MT6358_LDO_VMC_CON1, MT6358_VMC_ANA_CON0, 0xFFF),
	MT6358_LDO_VA09_DESC("ldo_va09", VA09),
	MT6358_VMCH_EINT(EINT_HIGH, vmch_vemc_voltages),
	MT6358_VMCH_EINT(EINT_LOW, vmch_vemc_voltages),
};

static void mt6358_oc_irq_enable_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct mt6358_regulator_info *info
		= container_of(dwork, struct mt6358_regulator_info, oc_work);

	enable_irq(info->irq);
}

static irqreturn_t mt6358_oc_irq(int irq, void *data)
{
	struct regulator_dev *rdev = (struct regulator_dev *)data;
	struct mt6358_regulator_info *info = rdev_get_drvdata(rdev);

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

static int mt6358_of_parse_cb(struct device_node *np,
			      const struct regulator_desc *desc,
			      struct regulator_config *config)
{
	int ret;
	struct mt6358_regulator_info *info = config->driver_data;

	if (info->irq > 0) {
		ret = of_property_read_u32(np, "mediatek,oc-irq-enable-delay-ms",
					   &info->oc_irq_enable_delay_ms);
		if (ret || !info->oc_irq_enable_delay_ms)
			info->oc_irq_enable_delay_ms = DEF_OC_IRQ_ENABLE_DELAY_MS;
		INIT_DELAYED_WORK(&info->oc_work, mt6358_oc_irq_enable_work);
	}
	return 0;
}

static bool mt6358_bypass_register(struct mt6358_regulator_info *info)
{
	return (info->desc.id == MT6358_ID_VSRAM_CORE);
}

static bool mt6366_bypass_register(struct mt6358_regulator_info *info)
{
	return (info->desc.id == MT6358_ID_VCAMIO || info->desc.id == MT6358_ID_VCAMD ||
		info->desc.id == MT6358_ID_VCAMA1 || info->desc.id == MT6358_ID_VCAMA2 ||
		info->desc.id == MT6358_ID_VLDO28);
}

static int mt6358_regulator_probe(struct platform_device *pdev)
{
	struct mt6397_chip *mt6397 = dev_get_drvdata(pdev->dev.parent);
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	struct mt6358_regulator_info *info;
	int i, ret;

	config.dev = &pdev->dev;
	config.regmap = mt6397->regmap;
	for (i = 0; i < MT6358_MAX_REGULATOR; i++) {
		info = &mt6358_regulators[i];
		info->irq = platform_get_irq_byname_optional(pdev, info->desc.name);
		config.driver_data = info;

		if (mt6397->chip_id == 0x58 && mt6358_bypass_register(info))
			continue;
		else if (mt6397->chip_id == 0x66 && mt6366_bypass_register(info))
			continue;

		rdev = devm_regulator_register(&pdev->dev, &info->desc, &config);
		if (IS_ERR(rdev)) {
			ret = PTR_ERR(rdev);
			dev_info(&pdev->dev, "failed to register %s, ret=%d\n",
				info->desc.name, ret);
			continue;
		}

		if (info->irq <= 0)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, info->irq, NULL,
						mt6358_oc_irq,
						IRQF_TRIGGER_HIGH,
						info->desc.name,
						rdev);
		if (ret) {
			dev_info(&pdev->dev, "Failed to request IRQ:%s, ret=%d",
				info->desc.name, ret);
			continue;
		}
	}

	return 0;
}

static const struct platform_device_id mt6358_platform_ids[] = {
	{"mt6358-regulator", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mt6358_platform_ids);

static struct platform_driver mt6358_regulator_driver = {
	.driver = {
		.name = "mt6358-regulator",
	},
	.probe = mt6358_regulator_probe,
	.id_table = mt6358_platform_ids,
};

#if IS_BUILTIN(CONFIG_REGULATOR_MT6358)
static int __init mt6358_regulator_init(void)
{
	return platform_driver_register(&mt6358_regulator_driver);
}
subsys_initcall(mt6358_regulator_init);
#else
module_platform_driver(mt6358_regulator_driver);
#endif

MODULE_AUTHOR("Hsin-Hsiung Wang <hsin-hsiung.wang@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6358 PMIC");
MODULE_LICENSE("GPL");
