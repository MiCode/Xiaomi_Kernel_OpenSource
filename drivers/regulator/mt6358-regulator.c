/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Hsin-Hsiung Wang <hsin-hsiung.wang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/mfd/mt6358/registers.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/mt6358-regulator.h>
#include <linux/regulator/of_regulator.h>

/*
 * MT6358 regulators' information
 *
 * @desc: standard fields of regulator description.
 * @qi: Mask for query enable signal status of regulators
 */
struct mt6358_regulator_info {
	struct regulator_desc desc;
	u32 status_reg;
	u32 qi;
	const u32 *index_table;
	unsigned int n_table;
	u32 vsel_shift;
};

#define MT6358_BUCK(match, vreg, min, max, step, \
		volt_ranges, vosel_mask)		\
[MT6358_ID_##vreg] = {							\
	.desc = {							\
		.name = #vreg,						\
		.of_match = of_match_ptr(match),			\
		.ops = &mt6358_volt_range_ops,				\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6358_ID_##vreg,					\
		.owner = THIS_MODULE,					\
		.n_voltages = (max - min)/step + 1,			\
		.linear_ranges = volt_ranges,				\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),		\
		.vsel_reg = MT6358_BUCK_##vreg##_ELR0,	\
		.vsel_mask = vosel_mask,				\
		.enable_reg = MT6358_BUCK_##vreg##_CON0,	\
		.enable_mask = BIT(0),					\
	},								\
	.status_reg = MT6358_BUCK_##vreg##_DBG1,	\
	.qi = BIT(0),							\
}

#define MT6358_LDO(match, vreg, ldo_volt_table, ldo_index_table, enreg,	\
	enbit, vosel, vosel_mask, vosel_shift)		\
[MT6358_ID_##vreg] = {							\
	.desc = {							\
		.name = #vreg,						\
		.of_match = of_match_ptr(match),			\
		.ops = &mt6358_volt_table_ops,				\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6358_ID_##vreg,					\
		.owner = THIS_MODULE,					\
		.n_voltages = ARRAY_SIZE(ldo_volt_table),		\
		.volt_table = ldo_volt_table,				\
		.vsel_reg = vosel,					\
		.vsel_mask = vosel_mask,				\
		.enable_reg = enreg,					\
		.enable_mask = BIT(enbit),				\
	},								\
	.status_reg = MT6358_LDO_##vreg##_CON1,			\
	.qi = BIT(15),							\
	.index_table = ldo_index_table,					\
	.n_table = ARRAY_SIZE(ldo_index_table),			\
	.vsel_shift = vosel_shift,				\
}

#define MT6358_LDO1(match, vreg, min, max, step, \
	volt_ranges, vosel, vosel_mask)	\
[MT6358_ID_##vreg] = {							\
	.desc = {							\
		.name = #vreg,						\
		.of_match = of_match_ptr(match),			\
		.ops = &mt6358_volt_range_ops,				\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6358_ID_##vreg,					\
		.owner = THIS_MODULE,					\
		.n_voltages = (max - min)/step + 1,			\
		.linear_ranges = volt_ranges,				\
		.n_linear_ranges = ARRAY_SIZE(volt_ranges),		\
		.vsel_reg = vosel,					\
		.vsel_mask = vosel_mask,				\
		.enable_reg = MT6358_LDO_##vreg##_CON0,	\
		.enable_mask = BIT(0),					\
	},								\
	.status_reg = MT6358_LDO_##vreg##_DBG1,		\
	.qi = BIT(0),							\
}

#define MT6358_REG_FIXED(match, vreg, enreg, enbit, volt)		\
[MT6358_ID_##vreg] = {							\
	.desc = {							\
		.name = #vreg,						\
		.of_match = of_match_ptr(match),			\
		.ops = &mt6358_volt_fixed_ops,				\
		.type = REGULATOR_VOLTAGE,				\
		.id = MT6358_ID_##vreg,					\
		.owner = THIS_MODULE,					\
		.n_voltages = 1,					\
		.enable_reg = enreg,					\
		.enable_mask = BIT(enbit),				\
		.min_uV = volt,						\
	},								\
	.status_reg = MT6358_LDO_##vreg##_CON1,			\
	.qi = BIT(15),							\
}

static const struct regulator_linear_range buck_volt_range1[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 0x7f, 6250),
};

static const struct regulator_linear_range buck_volt_range2[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 0x7f, 12500),
};

static const struct regulator_linear_range buck_volt_range3[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 0x3f, 50000),
};

static const struct regulator_linear_range buck_volt_range4[] = {
	REGULATOR_LINEAR_RANGE(1000000, 0, 0x7f, 12500),
};

static const u32 vdram2_voltages[] = {
	600000, 1800000,
};

static const u32 vsim1_voltages[] = {
	1700000, 1800000, 2700000, 3000000, 3100000,
};

static const u32 vibr_voltages[] = {
	1200000, 1300000, 1500000, 1800000,
	2000000, 2800000, 3000000, 3300000,
};

static const u32 vusb_voltages[] = {
	3000000, 3100000,
};

static const u32 vcamd_voltages[] = {
	900000, 1000000, 1100000, 1200000,
	1300000, 1500000, 1800000,
};

static const u32 vefuse_voltages[] = {
	1700000, 1800000, 1900000,
};

static const u32 vmch_voltages[] = {
	2900000, 3000000, 3300000,
};

static const u32 vcama1_voltages[] = {
	1800000, 2500000, 2700000,
	2800000, 2900000, 3000000,
};

static const u32 vemc_voltages[] = {
	2900000, 3000000, 3300000,
};

static const u32 vcn33_bt_voltages[] = {
	3300000, 3400000, 3500000,
};

static const u32 vcn33_wifi_voltages[] = {
	3300000, 3400000, 3500000,
};

static const u32 vcama2_voltages[] = {
	1800000, 2500000, 2700000,
	2800000, 2900000, 3000000,
};

static const u32 vmc_voltages[] = {
	1800000, 2900000, 3000000, 3300000,
};

static const u32 vldo28_voltages[] = {
	2800000, 3000000,
};

static const u32 vsim2_voltages[] = {
	1700000, 1800000, 2700000,
	3000000, 3100000,
};

static const u32 vdram2_idx[] = {
	0, 12,
};

static const u32 vsim1_idx[] = {
	3, 4, 8, 11, 12,
};

static const u32 vibr_idx[] = {
	0, 1, 2, 4, 5, 9, 11, 13,
};

static const u32 vusb_idx[] = {
	3, 4,
};

static const u32 vcamd_idx[] = {
	3, 4, 5, 6, 7, 9, 12,
};

static const u32 vefuse_idx[] = {
	11, 12, 13,
};

static const u32 vmch_idx[] = {
	2, 3, 5,
};

static const u32 vcama1_idx[] = {
	0, 7, 9, 10, 11, 12,
};

static const u32 vemc_idx[] = {
	2, 3, 5,
};

static const u32 vcn33_bt_idx[] = {
	1, 2, 3,
};

static const u32 vcn33_wifi_idx[] = {
	1, 2, 3,
};

static const u32 vcama2_idx[] = {
	0, 7, 9, 10, 11, 12,
};

static const u32 vmc_idx[] = {
	4, 10, 11, 13,
};

static const u32 vldo28_idx[] = {
	1, 3,
};

static const u32 vsim2_idx[] = {
	3, 4, 8, 11, 12,
};

static int mt6358_set_voltage_sel(
	struct regulator_dev *rdev, unsigned int selector)
{
	int idx, ret;
	const u32 *pVoltidx;
	struct mt6358_regulator_info *info = rdev_get_drvdata(rdev);

	pVoltidx = (const u32 *)info->index_table;

	idx = pVoltidx[selector];
	ret = regmap_update_bits(rdev->regmap, info->desc.vsel_reg,
			info->desc.vsel_mask, idx << info->vsel_shift);

	return ret;
}

static int mt6358_get_voltage_sel(struct regulator_dev *rdev)
{
	int idx, ret;
	u32 selector = 0;
	struct mt6358_regulator_info *info = rdev_get_drvdata(rdev);
	const u32 *pVoltidx;

	ret = regmap_read(rdev->regmap, info->desc.vsel_reg, &selector);
	if (ret != 0) {
		dev_info(&rdev->dev,
			"Failed to get mt6358 %s vsel reg: %d\n",
			info->desc.name, ret);
		return ret;
	}

	selector = (selector & info->desc.vsel_mask) >> info->vsel_shift;
	pVoltidx = (const u32 *)info->index_table;
	ret = -1;
	for (idx = 0; idx < info->desc.n_voltages; idx++) {
		if (pVoltidx[idx] == selector) {
			ret = idx;
			break;
		}
	}

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

static const struct regulator_ops mt6358_volt_range_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.map_voltage = regulator_map_voltage_linear_range,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_time_sel = regulator_set_voltage_time_sel,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.get_status = mt6358_get_status,
};

static const struct regulator_ops mt6358_volt_table_ops = {
	.list_voltage = regulator_list_voltage_table,
	.map_voltage = regulator_map_voltage_iterate,
	.set_voltage_sel = mt6358_set_voltage_sel,
	.get_voltage_sel = mt6358_get_voltage_sel,
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

/* The array is indexed by id(MT6358_ID_XXX) */
static struct mt6358_regulator_info mt6358_regulators[] = {
	MT6358_BUCK("buck_vdram1", VDRAM1, 500000, 2087500, 12500,
		buck_volt_range2, 0x7f),
	MT6358_BUCK("buck_vcore", VCORE, 500000, 1293750, 6250,
		buck_volt_range1, 0x7f),
	MT6358_BUCK("buck_vpa", VPA, 500000, 3650000, 50000,
		buck_volt_range3, 0x3f),
	MT6358_BUCK("buck_vproc11", VPROC11, 500000, 1293750, 6250,
		buck_volt_range1, 0x7f),
	MT6358_BUCK("buck_vproc12", VPROC12, 500000, 1293750, 6250,
		buck_volt_range1, 0x7f),
	MT6358_BUCK("buck_vgpu", VGPU, 500000, 1293750, 6250,
		buck_volt_range1, 0x7f),
	MT6358_BUCK("buck_vs2", VS2, 500000, 2087500, 12500,
		buck_volt_range2, 0x7f),
	MT6358_BUCK("buck_vmodem", VMODEM, 500000, 1293750, 6250,
		buck_volt_range1, 0x7f),
	MT6358_BUCK("buck_vs1", VS1, 1000000, 2587500, 12500,
		buck_volt_range4, 0x7f),
	MT6358_REG_FIXED("ldo_vrf12", VRF12,
		MT6358_LDO_VRF12_CON0, 0, 1200000),
	MT6358_REG_FIXED("ldo_vio18", VIO18,
		MT6358_LDO_VIO18_CON0, 0, 1800000),
	MT6358_REG_FIXED("ldo_vcamio", VCAMIO,
		MT6358_LDO_VCAMIO_CON0, 0, 1800000),
	MT6358_REG_FIXED("ldo_vcn18", VCN18, MT6358_LDO_VCN18_CON0, 0, 1800000),
	MT6358_REG_FIXED("ldo_vfe28", VFE28, MT6358_LDO_VFE28_CON0, 0, 2800000),
	MT6358_REG_FIXED("ldo_vcn28", VCN28, MT6358_LDO_VCN28_CON0, 0, 2800000),
	MT6358_REG_FIXED("ldo_vxo22", VXO22, MT6358_LDO_VXO22_CON0, 0, 2200000),
	MT6358_REG_FIXED("ldo_vaux18", VAUX18,
		MT6358_LDO_VAUX18_CON0, 0, 1800000),
	MT6358_REG_FIXED("ldo_vbif28", VBIF28,
		MT6358_LDO_VBIF28_CON0, 0, 2800000),
	MT6358_REG_FIXED("ldo_vio28", VIO28, MT6358_LDO_VIO28_CON0, 0, 2800000),
	MT6358_REG_FIXED("ldo_va12", VA12, MT6358_LDO_VA12_CON0, 0, 1200000),
	MT6358_REG_FIXED("ldo_vrf18", VRF18, MT6358_LDO_VRF18_CON0, 0, 1800000),
	MT6358_REG_FIXED("ldo_vaud28", VAUD28,
		MT6358_LDO_VAUD28_CON0, 0, 2800000),
	MT6358_LDO("ldo_vdram2", VDRAM2, vdram2_voltages, vdram2_idx,
		MT6358_LDO_VDRAM2_CON0, 0, MT6358_LDO_VDRAM2_ELR0, 0x10, 0),
	MT6358_LDO("ldo_vsim1", VSIM1, vsim1_voltages, vsim1_idx,
		MT6358_LDO_VSIM1_CON0, 0, MT6358_VSIM1_ANA_CON0, 0xf00, 8),
	MT6358_LDO("ldo_vibr", VIBR, vibr_voltages, vibr_idx,
		MT6358_LDO_VIBR_CON0, 0, MT6358_VIBR_ANA_CON0, 0xf00, 8),
	MT6358_LDO("ldo_vusb", VUSB, vusb_voltages, vusb_idx,
		MT6358_LDO_VUSB_CON0_0, 0, MT6358_VUSB_ANA_CON0, 0x700, 8),
	MT6358_LDO("ldo_vcamd", VCAMD, vcamd_voltages, vcamd_idx,
		MT6358_LDO_VCAMD_CON0, 0, MT6358_VCAMD_ANA_CON0, 0xf00, 8),
	MT6358_LDO("ldo_vefuse", VEFUSE, vefuse_voltages, vefuse_idx,
		MT6358_LDO_VEFUSE_CON0, 0, MT6358_VEFUSE_ANA_CON0, 0xf00, 8),
	MT6358_LDO("ldo_vmch", VMCH, vmch_voltages, vmch_idx,
		MT6358_LDO_VMCH_CON0, 0, MT6358_VMCH_ANA_CON0, 0x700, 8),
	MT6358_LDO("ldo_vcama1", VCAMA1, vcama1_voltages, vcama1_idx,
		MT6358_LDO_VCAMA1_CON0, 0, MT6358_VCAMA1_ANA_CON0, 0xf00, 8),
	MT6358_LDO("ldo_vemc", VEMC, vemc_voltages, vemc_idx,
		MT6358_LDO_VEMC_CON0, 0, MT6358_VEMC_ANA_CON0, 0x700, 8),
	MT6358_LDO("ldo_vcn33_bt", VCN33_BT, vcn33_bt_voltages, vcn33_bt_idx,
		MT6358_LDO_VCN33_CON0_0, 0, MT6358_VCN33_ANA_CON0, 0x300, 8),
	MT6358_LDO("ldo_vcn33_wifi", VCN33_WIFI, vcn33_wifi_voltages,
		vcn33_wifi_idx, MT6358_LDO_VCN33_CON0_1,
		0, MT6358_VCN33_ANA_CON0, 0x300, 8),
	MT6358_LDO("ldo_vcama2", VCAMA2, vcama2_voltages, vcama2_idx,
		MT6358_LDO_VCAMA2_CON0, 0, MT6358_VCAMA2_ANA_CON0, 0xf00, 8),
	MT6358_LDO("ldo_vmc", VMC, vmc_voltages, vmc_idx,
		MT6358_LDO_VMC_CON0, 0, MT6358_VMC_ANA_CON0, 0xf00, 8),
	MT6358_LDO("ldo_vldo28", VLDO28, vldo28_voltages, vldo28_idx,
		MT6358_LDO_VLDO28_CON0_0, 0, MT6358_VLDO28_ANA_CON0, 0x300, 8),
	MT6358_LDO("ldo_vsim2", VSIM2, vsim2_voltages, vsim2_idx,
		MT6358_LDO_VSIM2_CON0, 0, MT6358_VSIM2_ANA_CON0, 0xf00, 8),
	MT6358_LDO1("ldo_vsram_proc11", VSRAM_PROC11, 500000, 1293750, 6250,
		buck_volt_range1, MT6358_LDO_VSRAM_CON0, 0x7f),
	MT6358_LDO1("ldo_vsram_others", VSRAM_OTHERS, 500000, 1293750, 6250,
		buck_volt_range1, MT6358_LDO_VSRAM_CON2, 0x7f),
	MT6358_LDO1("ldo_vsram_gpu", VSRAM_GPU, 500000, 1293750, 6250,
		buck_volt_range1, MT6358_LDO_VSRAM_CON3, 0x7f),
	MT6358_LDO1("ldo_vsram_proc12", VSRAM_PROC12, 500000, 1293750, 6250,
		buck_volt_range1, MT6358_LDO_VSRAM_CON1, 0x7f),
};

static int mt6358_regulator_probe(struct platform_device *pdev)
{
	struct mt6397_chip *mt6397 = dev_get_drvdata(pdev->dev.parent);
	struct regulator_config config = {};
	struct regulator_dev *rdev;
	int i;
	u32 reg_value = 0;

	/* Read PMIC chip revision to update constraints and voltage table */
	if (regmap_read(mt6397->regmap, MT6358_SWCID, &reg_value) < 0) {
		dev_err(&pdev->dev, "Failed to read Chip ID\n");
		return -EIO;
	}

	for (i = 0; i < MT6358_MAX_REGULATOR; i++) {
		config.dev = &pdev->dev;
		config.driver_data = &mt6358_regulators[i];
		config.regmap = mt6397->regmap;

		rdev = devm_regulator_register(&pdev->dev,
				&mt6358_regulators[i].desc, &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev, "failed to register %s\n",
				mt6358_regulators[i].desc.name);
			return PTR_ERR(rdev);
		}
	}

	return 0;
}

static const struct platform_device_id mt6358_platform_ids[] = {
	{"mt6358-regulator", 0},
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(platform, mt6358_platform_ids);

static const struct of_device_id mt6358_of_match[] = {
	{ .compatible = "mediatek,mt6358-regulator", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, mt6358_of_match);

static struct platform_driver mt6358_regulator_driver = {
	.driver = {
		.name = "mt6358-regulator",
		.of_match_table = of_match_ptr(mt6358_of_match),
	},
	.probe = mt6358_regulator_probe,
	.id_table = mt6358_platform_ids,
};

module_platform_driver(mt6358_regulator_driver);

MODULE_AUTHOR("Hsin-Hsiung Wang <hsin-hsiung.wang@mediatek.com>");
MODULE_DESCRIPTION("Regulator Driver for MediaTek MT6358 PMIC");
MODULE_LICENSE("GPL");
