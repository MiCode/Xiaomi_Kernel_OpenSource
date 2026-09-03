// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

/*
 * SC2720 regulator base address
 */
#define SC2720_REGULATOR_BASE		0xc00

/*
 * SC2720 regulator lock register
 */
#define SC2720_WR_UNLOCK_VALUE		0x6e7f
#define SC2720_PWR_WR_PROT		(SC2720_REGULATOR_BASE + 0x248)

/*
 * SC2720 enable register
 */
#define SC2720_POWER_PD_SW		(SC2720_REGULATOR_BASE + 0x1c)
#define SC2720_DCDC_WPA_PD		(SC2720_REGULATOR_BASE + 0x60)
#define SC2720_LDO_VDDCAMIO_PD	        (SC2720_REGULATOR_BASE + 0x8c)
#define SC2720_LDO_VDDRF18A_PD		(SC2720_REGULATOR_BASE + 0x94)
#define SC2720_LDO_VDDRF18B_PD		(SC2720_REGULATOR_BASE + 0x9c)
#define SC2720_LDO_VDDCAMD_PD		(SC2720_REGULATOR_BASE + 0xa4)
#define SC2720_LDO_VDDCON_PD		(SC2720_REGULATOR_BASE + 0xac)
#define SC2720_LDO_VDDSIM0_PD		(SC2720_REGULATOR_BASE + 0xc0)
#define SC2720_LDO_VDDSIM1_PD		(SC2720_REGULATOR_BASE + 0xcc)
#define SC2720_LDO_VDDSIM2_PD		(SC2720_REGULATOR_BASE + 0xd8)
#define SC2720_LDO_VDDCAMA_PD		(SC2720_REGULATOR_BASE + 0xe0)
#define SC2720_LDO_VDDCAMMOT_PD		(SC2720_REGULATOR_BASE + 0xe8)
#define SC2720_LDO_VDDEMMCCORE_PD	(SC2720_REGULATOR_BASE + 0xf4)
#define SC2720_LDO_VDDSDCORE_PD		(SC2720_REGULATOR_BASE + 0x100)
#define SC2720_LDO_VDDSDIO_PD		(SC2720_REGULATOR_BASE + 0x10c)
#define SC2720_LDO_VDDWIFIPA_PD		(SC2720_REGULATOR_BASE + 0x11c)
#define SC2720_LDO_VDDUSB33_PD		(SC2720_REGULATOR_BASE + 0x130)

/*
 * SC2720 enable mask
 */
#define SC2720_DCDC_CORE_PD_MASK	BIT(5)
#define SC2720_DCDC_GEN_PD_MASK		BIT(7)
#define SC2720_DCDC_WPA_PD_MASK		BIT(0)
#define SC2720_LDO_AVDD18_PD_MASK	BIT(2)
#define SC2720_LDO_VDDCAMIO_PD_MASK	BIT(0)
#define SC2720_LDO_VDDRF18A_PD_MASK	BIT(0)
#define SC2720_LDO_VDDRF18B_PD_MASK	BIT(0)
#define SC2720_LDO_VDDCAMD_PD_MASK	BIT(0)
#define SC2720_LDO_VDDCON_PD_MASK	BIT(0)
#define SC2720_LDO_MEM_PD_MASK		BIT(3)
#define SC2720_LDO_VDDSIM0_PD_MASK	BIT(0)
#define SC2720_LDO_VDDSIM1_PD_MASK	BIT(0)
#define SC2720_LDO_VDDSIM2_PD_MASK	BIT(0)
#define SC2720_LDO_VDDCAMA_PD_MASK	BIT(0)
#define SC2720_LDO_VDDCAMMOT_PD_MASK	BIT(0)
#define SC2720_LDO_VDDEMMCCORE_PD_MASK	BIT(0)
#define SC2720_LDO_VDDSDCORE_PD_MASK	BIT(0)
#define SC2720_LDO_VDDSDIO_PD_MASK	BIT(0)
#define SC2720_LDO_VDD28_PD_MASK	BIT(1)
#define SC2720_LDO_VDDWIFIPA_PD_MASK	BIT(0)
#define SC2720_LDO_VDD18_DCXO_PD_MASK	BIT(10)
#define SC2720_LDO_VDDUSB33_PD_MASK	BIT(0)

/*
 * SC2720 vsel register
 */
#define SC2720_DCDC_CORE_VOL		(SC2720_REGULATOR_BASE + 0x44)
#define SC2720_DCDC_GEN_VOL		(SC2720_REGULATOR_BASE + 0x54)
#define SC2720_DCDC_WPA_VOL		(SC2720_REGULATOR_BASE + 0x64)
#define SC2720_LDO_AVDD18_VOL		(SC2720_REGULATOR_BASE + 0x88)
#define SC2720_LDO_VDDCAMIO_VOL		(SC2720_REGULATOR_BASE + 0x90)
#define SC2720_LDO_VDDRF18A_VOL		(SC2720_REGULATOR_BASE + 0x98)
#define SC2720_LDO_VDDRF18B_VOL		(SC2720_REGULATOR_BASE + 0xa0)
#define SC2720_LDO_VDDCAMD_VOL		(SC2720_REGULATOR_BASE + 0xa8)
#define SC2720_LDO_VDDCON_VOL		(SC2720_REGULATOR_BASE + 0xb0)
#define SC2720_LDO_MEM_VOL		(SC2720_REGULATOR_BASE + 0xb8)
#define SC2720_LDO_VDDSIM0_VOL		(SC2720_REGULATOR_BASE + 0xc4)
#define SC2720_LDO_VDDSIM1_VOL		(SC2720_REGULATOR_BASE + 0xd0)
#define SC2720_LDO_VDDSIM2_VOL		(SC2720_REGULATOR_BASE + 0xdc)
#define SC2720_LDO_VDDCAMA_VOL		(SC2720_REGULATOR_BASE + 0xe4)
#define SC2720_LDO_VDDCAMMOT_VOL	(SC2720_REGULATOR_BASE + 0xec)
#define SC2720_LDO_VDDEMMCCORE_VOL	(SC2720_REGULATOR_BASE + 0xf8)
#define SC2720_LDO_VDDSDCORE_VOL	(SC2720_REGULATOR_BASE + 0x104)
#define SC2720_LDO_VDDSDIO_VOL		(SC2720_REGULATOR_BASE + 0x110)
#define SC2720_LDO_VDD28_VOL		(SC2720_REGULATOR_BASE + 0x118)
#define SC2720_LDO_VDDWIFIPA_VOL	(SC2720_REGULATOR_BASE + 0x120)
#define SC2720_LDO_VDD18_DCXO_VOL	(SC2720_REGULATOR_BASE + 0x128)
#define SC2720_LDO_VDDUSB33_VOL		(SC2720_REGULATOR_BASE + 0x134)

/*
 * SC2720 vsel register mask
 */
#define SC2720_DCDC_CORE_VOL_MASK	GENMASK(8, 0)
#define SC2720_DCDC_GEN_VOL_MASK	GENMASK(7, 0)
#define SC2720_DCDC_WPA_VOL_MASK	GENMASK(6, 0)
#define SC2720_LDO_AVDD18_VOL_MASK	GENMASK(5, 0)
#define SC2720_LDO_VDDCAMIO_VOL_MASK	GENMASK(5, 0)
#define SC2720_LDO_VDDRF18A_VOL_MASK	GENMASK(5, 0)
#define SC2720_LDO_VDDRF18B_VOL_MASK	GENMASK(5, 0)
#define SC2720_LDO_VDDCAMD_VOL_MASK	GENMASK(5, 0)
#define SC2720_LDO_VDDCON_VOL_MASK	GENMASK(5, 0)
#define SC2720_LDO_MEM_VOL_MASK		GENMASK(5, 0)
#define SC2720_LDO_VDDSIM0_VOL_MASK	GENMASK(6, 0)
#define SC2720_LDO_VDDSIM1_VOL_MASK	GENMASK(6, 0)
#define SC2720_LDO_VDDSIM2_VOL_MASK	GENMASK(6, 0)
#define SC2720_LDO_VDDCAMA_VOL_MASK	GENMASK(6, 0)
#define SC2720_LDO_VDDCAMMOT_VOL_MASK	GENMASK(6, 0)
#define SC2720_LDO_VDDEMMCCORE_VOL_MASK	GENMASK(6, 0)
#define SC2720_LDO_VDDSDCORE_VOL_MASK	GENMASK(6, 0)
#define SC2720_LDO_VDDSDIO_VOL_MASK	GENMASK(6, 0)
#define SC2720_LDO_VDD28_VOL_MASK	GENMASK(6, 0)
#define SC2720_LDO_VDDWIFIPA_VOL_MASK	GENMASK(6, 0)
#define SC2720_LDO_VDD18_DCXO_VOL_MASK	GENMASK(6, 0)
#define SC2720_LDO_VDDUSB33_VOL_MASK	GENMASK(6, 0)

enum sc2720_regulator_id {
	SC2720_DCDC_CORE,
	SC2720_DCDC_GEN,
	SC2720_DCDC_WPA,
	SC2720_LDO_AVDD18,
	SC2720_LDO_VDDCAMIO,
	SC2720_LDO_VDDRF18A,
	SC2720_LDO_VDDRF18B,
	SC2720_LDO_VDDCAMD,
	SC2720_LDO_VDDCON,
	SC2720_LDO_MEM,
	SC2720_LDO_VDDSIM0,
	SC2720_LDO_VDDSIM1,
	SC2720_LDO_VDDSIM2,
	SC2720_LDO_VDDCAMA,
	SC2720_LDO_VDDCAMMOT,
	SC2720_LDO_VDDEMMCCORE,
	SC2720_LDO_VDDSDCORE,
	SC2720_LDO_VDDSDIO,
	SC2720_LDO_VDD28,
	SC2720_LDO_VDDWIFIPA,
	SC2720_LDO_VDD18_DCXO,
	SC2720_LDO_VDDUSB33,
};

static struct dentry *debugfs_root;

static const struct regulator_ops sc2720_regu_linear_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
};

#define SC2720_REGU_LINEAR(_id, en_reg, en_mask, vreg, vmask,	\
			  vstep, vmin, vmax, min_sel, vsel_set_step) {		\
	.name			= #_id,				\
	.of_match		= of_match_ptr(#_id),		\
	.ops			= &sc2720_regu_linear_ops,	\
	.type			= REGULATOR_VOLTAGE,		\
	.id			= SC2720_##_id,			\
	.owner			= THIS_MODULE,			\
	.min_uV			= vmin,				\
	.n_voltages		= ((vmax) - (vmin)) / (vstep) + 1,	\
	.uV_step		= vstep,			\
	.enable_is_inverted	= true,				\
	.enable_val		= 0,				\
	.enable_reg		= en_reg,			\
	.enable_mask		= en_mask,			\
	.vsel_reg		= vreg,				\
	.vsel_mask		= vmask,			\
	.linear_min_sel		= min_sel,			\
	.vsel_step		=  vsel_set_step,             \
}

static struct regulator_desc regulators[] = {
	SC2720_REGU_LINEAR(DCDC_CORE, SC2720_POWER_PD_SW,
			   SC2720_DCDC_CORE_PD_MASK, SC2720_DCDC_CORE_VOL,
			   SC2720_DCDC_CORE_VOL_MASK, 3125, 200000,
			   1596875, 64, 4),
	SC2720_REGU_LINEAR(DCDC_GEN, SC2720_POWER_PD_SW,
			   SC2720_DCDC_GEN_PD_MASK, SC2720_DCDC_GEN_VOL,
			   SC2720_DCDC_GEN_VOL_MASK, 12500, 1300000,
			   4487500, 0, 0),
	SC2720_REGU_LINEAR(DCDC_WPA, SC2720_DCDC_WPA_PD,
			   SC2720_DCDC_WPA_PD_MASK, SC2720_DCDC_WPA_VOL,
			   SC2720_DCDC_WPA_VOL_MASK, 25000, 400000,
			   3575000, 0, 0),
	SC2720_REGU_LINEAR(LDO_AVDD18, SC2720_POWER_PD_SW,
			   SC2720_LDO_AVDD18_PD_MASK, SC2720_LDO_AVDD18_VOL,
			   SC2720_LDO_AVDD18_VOL_MASK, 12500, 1400000,
			   2187500, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDCAMIO, SC2720_LDO_VDDCAMIO_PD,
			   SC2720_LDO_VDDCAMIO_PD_MASK, SC2720_LDO_VDDCAMIO_VOL,
			   SC2720_LDO_VDDCAMIO_VOL_MASK, 12500, 1400000,
			   2187500, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDRF18A, SC2720_LDO_VDDRF18A_PD,
			   SC2720_LDO_VDDRF18A_PD_MASK, SC2720_LDO_VDDRF18A_VOL,
			   SC2720_LDO_VDDRF18A_VOL_MASK, 12500, 1400000,
			   2187500, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDRF18B, SC2720_LDO_VDDRF18B_PD,
			   SC2720_LDO_VDDRF18B_PD_MASK, SC2720_LDO_VDDRF18B_VOL,
			   SC2720_LDO_VDDRF18B_VOL_MASK, 12500, 1400000,
			   2187500, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDCAMD, SC2720_LDO_VDDCAMD_PD,
			   SC2720_LDO_VDDCAMD_PD_MASK, SC2720_LDO_VDDCAMD_VOL,
			   SC2720_LDO_VDDCAMD_VOL_MASK, 12500, 800000,
			   1587500, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDCON, SC2720_LDO_VDDCON_PD,
			   SC2720_LDO_VDDCON_PD_MASK, SC2720_LDO_VDDCON_VOL,
			   SC2720_LDO_VDDCON_VOL_MASK, 12500, 800000,
			   1587500, 0, 0),
	SC2720_REGU_LINEAR(LDO_MEM, SC2720_POWER_PD_SW,
			   SC2720_LDO_MEM_PD_MASK, SC2720_LDO_MEM_VOL,
			   SC2720_LDO_MEM_VOL_MASK, 12500, 800000,
			   1587500, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDSIM0, SC2720_LDO_VDDSIM0_PD,
			   SC2720_LDO_VDDSIM0_PD_MASK, SC2720_LDO_VDDSIM0_VOL,
			   SC2720_LDO_VDDSIM0_VOL_MASK, 12500, 1612500,
			   3200000, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDSIM1, SC2720_LDO_VDDSIM1_PD,
			   SC2720_LDO_VDDSIM1_PD_MASK, SC2720_LDO_VDDSIM1_VOL,
			   SC2720_LDO_VDDSIM1_VOL_MASK, 12500, 1612500,
			   3200000, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDSIM2, SC2720_LDO_VDDSIM2_PD,
			   SC2720_LDO_VDDSIM2_PD_MASK, SC2720_LDO_VDDSIM2_VOL,
			   SC2720_LDO_VDDSIM2_VOL_MASK, 12500, 1612500,
			   3200000, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDCAMA, SC2720_LDO_VDDCAMA_PD,
			   SC2720_LDO_VDDCAMA_PD_MASK, SC2720_LDO_VDDCAMA_VOL,
			   SC2720_LDO_VDDCAMA_VOL_MASK, 12500, 1612500,
			   3200000, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDCAMMOT, SC2720_LDO_VDDCAMMOT_PD,
			   SC2720_LDO_VDDCAMMOT_PD_MASK,
			   SC2720_LDO_VDDCAMMOT_VOL,
			   SC2720_LDO_VDDCAMMOT_VOL_MASK, 12500, 2000000,
			   3587500, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDEMMCCORE, SC2720_LDO_VDDEMMCCORE_PD,
			   SC2720_LDO_VDDEMMCCORE_PD_MASK,
			   SC2720_LDO_VDDEMMCCORE_VOL,
			   SC2720_LDO_VDDEMMCCORE_VOL_MASK, 12500, 2000000,
			   3587500, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDSDCORE, SC2720_LDO_VDDSDCORE_PD,
			   SC2720_LDO_VDDSDCORE_PD_MASK,
			   SC2720_LDO_VDDSDCORE_VOL,
			   SC2720_LDO_VDDSDCORE_VOL_MASK, 12500, 2000000,
			   3587500, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDSDIO, SC2720_LDO_VDDSDIO_PD,
			   SC2720_LDO_VDDSDIO_PD_MASK, SC2720_LDO_VDDSDIO_VOL,
			   SC2720_LDO_VDDSDIO_VOL_MASK, 12500, 1612500,
			   3200000, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDD28, SC2720_POWER_PD_SW,
			   SC2720_LDO_VDD28_PD_MASK, SC2720_LDO_VDD28_VOL,
			   SC2720_LDO_VDD28_VOL_MASK, 12500, 1612500,
			   3200000, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDWIFIPA, SC2720_LDO_VDDWIFIPA_PD,
			   SC2720_LDO_VDDWIFIPA_PD_MASK,
			   SC2720_LDO_VDDWIFIPA_VOL,
			   SC2720_LDO_VDDWIFIPA_VOL_MASK, 12500, 2100000,
			   3687500, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDD18_DCXO, SC2720_POWER_PD_SW,
			   SC2720_LDO_VDD18_DCXO_PD_MASK,
			   SC2720_LDO_VDD18_DCXO_VOL,
			   SC2720_LDO_VDD18_DCXO_VOL_MASK, 12500, 1500000,
			   3087500, 0, 0),
	SC2720_REGU_LINEAR(LDO_VDDUSB33, SC2720_LDO_VDDUSB33_PD,
			   SC2720_LDO_VDDUSB33_PD_MASK, SC2720_LDO_VDDUSB33_VOL,
			   SC2720_LDO_VDDUSB33_VOL_MASK, 12500, 2100000,
			   3687500, 0, 0),
};

static int debugfs_enable_get(void *data, u64 *val)
{
	struct regulator_dev *rdev = data;

	if (rdev && rdev->desc->ops->is_enabled)
		*val = rdev->desc->ops->is_enabled(rdev);
	else
		*val = ~0ULL;
	return 0;
}

static int debugfs_enable_set(void *data, u64 val)
{
	struct regulator_dev *rdev = data;

	if (rdev && rdev->desc->ops->enable && rdev->desc->ops->disable) {
		if (val)
			rdev->desc->ops->enable(rdev);
		else
			rdev->desc->ops->disable(rdev);
	}

	return 0;
}

static int debugfs_voltage_get(void *data, u64 *val)
{
	int sel, ret;
	struct regulator_dev *rdev = data;

	if (!rdev || !rdev->desc->ops->get_voltage_sel || !rdev->desc->ops->list_voltage)
		return -EINVAL;

	sel = rdev->desc->ops->get_voltage_sel(rdev);
	if (sel < 0)
		return sel;

	ret = rdev->desc->ops->list_voltage(rdev, sel);
	*val = ret / 1000;

	return 0;
}

static int debugfs_voltage_set(void *data, u64 val)
{
	int selector;
	struct regulator_dev *rdev = data;

	if (!rdev || !rdev->desc->ops->set_voltage_sel)
		return -EINVAL;

	val = val * 1000;
	if (val < rdev->desc->min_uV)
		return -EINVAL;

	selector = regulator_map_voltage_linear(rdev,
						val - rdev->desc->uV_step / 2,
						val + rdev->desc->uV_step / 2);

	if (selector < 0)
		return selector;

	return rdev->desc->ops->set_voltage_sel(rdev, selector);
}

DEFINE_SIMPLE_ATTRIBUTE(fops_enable,
			debugfs_enable_get, debugfs_enable_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_ldo,
			debugfs_voltage_get, debugfs_voltage_set, "%llu\n");

static void sc2720_regulator_debugfs_init(struct regulator_dev *rdev)
{
	struct dentry *debugfs;

	if (IS_ERR_OR_NULL(debugfs_root)) {
		debugfs_root = debugfs_create_dir("sprd_regulator", NULL);
		if (IS_ERR_OR_NULL(debugfs_root)) {
			dev_warn(&rdev->dev, "Failed to recreate debugfs directory\n");
			return;
		}
	}

	debugfs = debugfs_create_dir(rdev->desc->name, debugfs_root);
	if (IS_ERR_OR_NULL(debugfs)) {
		dev_warn(&rdev->dev, "Failed to create %s directory\n", rdev->desc->name);
		return;
	}

	debugfs_create_file("enable", 0644, debugfs, rdev, &fops_enable);
	debugfs_create_file("voltage", 0644, debugfs, rdev, &fops_ldo);
}

static int sc2720_regulator_unlock(struct regmap *regmap)
{
	return regmap_write(regmap, SC2720_PWR_WR_PROT, SC2720_WR_UNLOCK_VALUE);
}

static int sc2720_regulator_probe(struct platform_device *pdev)
{
	int i, ret;
	struct regmap *regmap;
	struct regulator_config config = { };
	struct regulator_dev *rdev;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		dev_err(&pdev->dev, "failed to get regmap.\n");
		return -ENODEV;
	}

	ret = sc2720_regulator_unlock(regmap);
	if (ret) {
		dev_err(&pdev->dev, "failed to release regulator lock\n");
		return ret;
	}

	config.dev = &pdev->dev;
	config.regmap = regmap;

	debugfs_root = debugfs_create_dir("sprd_regulator", NULL);
	if (IS_ERR(debugfs_root))
		dev_warn(&pdev->dev, "Failed to create debugfs directory\n");

	for (i = 0; i < ARRAY_SIZE(regulators); i++) {
		rdev = devm_regulator_register(&pdev->dev, &regulators[i],
					       &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev, "failed to register regulator %s\n",
				regulators[i].name);
			continue;
		}
		sc2720_regulator_debugfs_init(rdev);
	}

	return 0;
}

static int sc2720_regulator_remove(struct platform_device *pdev)
{
	debugfs_remove_recursive(debugfs_root);
	return 0;
}

static const struct of_device_id sc27xx_regulator_of_match[] = {
	{ .compatible = "sprd,sc2720-regulator" },
	{ },
};

static struct platform_driver sc2720_regulator_driver = {
	.driver = {
		.name = "sc27xx-regulator",
		.of_match_table = sc27xx_regulator_of_match,
	},
	.probe = sc2720_regulator_probe,
	.remove = sc2720_regulator_remove,
};

module_platform_driver(sc2720_regulator_driver);

MODULE_AUTHOR("Zhongfa Wang <zhongfa.wang@unisoc.com>");
MODULE_DESCRIPTION("Spreadtrum SC2720 regulator driver");
MODULE_LICENSE("GPL v2");
