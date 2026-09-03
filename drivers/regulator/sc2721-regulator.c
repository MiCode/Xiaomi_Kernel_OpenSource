// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Spreadtrum Communications Inc.
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/sc27xx/sprd,sc2721-glb.h>
#include <linux/delay.h>

/*
 * SC2721 regulator base address
 */
#define SC2721_REGULATOR_BASE		0xc00

/*
 * SC2721 regulator lock register
 */
#define SC2721_WR_UNLOCK_VALUE		0x6e7f
#define SC2721_PWR_WR_PROT		(SC2721_REGULATOR_BASE + 0x2fc)

/*
 * SC2721 enable register
 */
#define SC2721_POWER_PD_SW		(SC2721_REGULATOR_BASE + 0x01c)
#define SC2721_LDO_VDDCAMA_PD		(SC2721_REGULATOR_BASE + 0x0b0)
#define SC2721_LDO_VDDCAMMOT_PD		(SC2721_REGULATOR_BASE + 0x0b8)
#define SC2721_LDO_VDDSIM2_PD		(SC2721_REGULATOR_BASE + 0x0dc)
#define SC2721_LDO_VDDEMMCCORE_PD	(SC2721_REGULATOR_BASE + 0x0e8)
#define SC2721_LDO_VDDSDCORE_PD		(SC2721_REGULATOR_BASE + 0x0f4)
#define SC2721_LDO_VDDSDIO_PD		(SC2721_REGULATOR_BASE + 0x100)
#define SC2721_LDO_VDDWIFIPA_PD		(SC2721_REGULATOR_BASE + 0x110)
#define SC2721_LDO_VDDUSB33_PD		(SC2721_REGULATOR_BASE + 0x124)
#define SC2721_LDO_VDDCAMD_PD		(SC2721_REGULATOR_BASE + 0x12c)
#define SC2721_LDO_VDDCON_PD		(SC2721_REGULATOR_BASE + 0x134)
#define SC2721_LDO_VDDCAMIO_PD		(SC2721_REGULATOR_BASE + 0x13c)
#define SC2721_LDO_VDDRF18_PD		(SC2721_REGULATOR_BASE + 0x14c)
#define SC2721_LDO_VDDRF125_PD		(SC2721_REGULATOR_BASE + 0x154)
#define SC2721_LDO_VDDKPLED_PD		(SC2721_REGULATOR_BASE + 0x2ac)

/*
 * SC2721 enable mask
 */
#define SC2721_DCDC_CPU_PD_MASK		BIT(4)
#define SC2721_DCDC_CORE_PD_MASK	BIT(5)
#define SC2721_DCDC_MEM_PD_MASK		BIT(6)
#define SC2721_DCDC_GEN_PD_MASK		BIT(7)
#define SC2721_DCDC_EMM_PD_MASK		BIT(9)
#define SC2721_LDO_VDD28_PD_MASK	BIT(1)
#define SC2721_LDO_AVDD18_PD_MASK	BIT(2)
#define SC2721_LDO_VDDMEM_PD_MASK	BIT(3)
#define SC2721_LDO_VDD18_DCXO_PD_MASK	BIT(10)
#define SC2721_LDO_VDDCAMA_PD_MASK	BIT(0)
#define SC2721_LDO_VDDCAMMOT_PD_MASK	BIT(0)
#define SC2721_LDO_VDDSIM2_PD_MASK	BIT(0)
#define SC2721_LDO_VDDEMMCCORE_PD_MASK	BIT(0)
#define SC2721_LDO_VDDSDCORE_PD_MASK	BIT(0)
#define SC2721_LDO_VDDSDIO_PD_MASK	BIT(0)
#define SC2721_LDO_VDDWIFIPA_PD_MASK	BIT(0)
#define SC2721_LDO_VDDUSB33_PD_MASK	BIT(0)
#define SC2721_LDO_VDDCAMD_PD_MASK	BIT(0)
#define SC2721_LDO_VDDCON_PD_MASK	BIT(0)
#define SC2721_LDO_VDDCAMIO_PD_MASK	BIT(0)
#define SC2721_LDO_VDDRF18_PD_MASK	BIT(0)
#define SC2721_LDO_VDDRF125_PD_MASK	BIT(0)
#define SC2721_LDO_VDDKPLED_PD_MASK	BIT(11)

/*
 * SC2721 vsel register
 */
#define SC2721_DCDC_CPU_VOL		(SC2721_REGULATOR_BASE + 0x44)
#define SC2721_DCDC_CORE_VOL		(SC2721_REGULATOR_BASE + 0x54)
#define SC2721_DCDC_MEM_VOL		(SC2721_REGULATOR_BASE + 0x64)
#define SC2721_DCDC_GEN_VOL		(SC2721_REGULATOR_BASE + 0x74)
#define SC2721_LDO_VDDCAMA_VOL		(SC2721_REGULATOR_BASE + 0xb4)
#define SC2721_LDO_VDDCAMMOT_VOL	(SC2721_REGULATOR_BASE + 0xbc)
#define SC2721_LDO_VDDSIM2_VOL		(SC2721_REGULATOR_BASE + 0xe0)
#define SC2721_LDO_VDDEMMCCORE_VOL	(SC2721_REGULATOR_BASE + 0xec)
#define SC2721_LDO_VDDSDCORE_VOL	(SC2721_REGULATOR_BASE + 0xf8)
#define SC2721_LDO_VDDSDIO_VOL		(SC2721_REGULATOR_BASE + 0x104)
#define SC2721_LDO_VDD28_VOL		(SC2721_REGULATOR_BASE + 0x10c)
#define SC2721_LDO_VDDWIFIPA_VOL	(SC2721_REGULATOR_BASE + 0x114)
#define SC2721_LDO_VDD18_DCXO_VOL	(SC2721_REGULATOR_BASE + 0x11c)
#define SC2721_LDO_VDDUSB33_VOL		(SC2721_REGULATOR_BASE + 0x128)
#define SC2721_LDO_VDDCAMD_VOL		(SC2721_REGULATOR_BASE + 0x130)
#define SC2721_LDO_VDDCON_VOL		(SC2721_REGULATOR_BASE + 0x138)
#define SC2721_LDO_VDDCAMIO_VOL		(SC2721_REGULATOR_BASE + 0x140)
#define SC2721_LDO_AVDD18_VOL		(SC2721_REGULATOR_BASE + 0x148)
#define SC2721_LDO_VDDRF18_VOL		(SC2721_REGULATOR_BASE + 0x150)
#define SC2721_LDO_VDDRF125_VOL		(SC2721_REGULATOR_BASE + 0x158)
#define SC2721_LDO_VDDMEM_VOL		(SC2721_REGULATOR_BASE + 0x160)
#define SC2721_LDO_VDDKPLED_VOL		(SC2721_REGULATOR_BASE + 0x2b0)

/*
 * SC2721 vsel register mask
 */
#define SC2721_DCDC_CPU_VOL_MASK	GENMASK(8, 0)
#define SC2721_DCDC_CORE_VOL_MASK	GENMASK(8, 0)
#define SC2721_DCDC_MEM_VOL_MASK	GENMASK(9, 0)
#define SC2721_DCDC_GEN_VOL_MASK	GENMASK(9, 0)
#define SC2721_LDO_VDDCAMA_VOL_MASK	GENMASK(7, 0)
#define SC2721_LDO_VDDCAMMOT_VOL_MASK	GENMASK(7, 0)
#define SC2721_LDO_VDDSIM2_VOL_MASK	GENMASK(7, 0)
#define SC2721_LDO_VDDEMMCCORE_VOL_MASK	GENMASK(7, 0)
#define SC2721_LDO_VDDSDCORE_VOL_MASK	GENMASK(7, 0)
#define SC2721_LDO_VDDSDIO_VOL_MASK	GENMASK(7, 0)
#define SC2721_LDO_VDD28_VOL_MASK	GENMASK(7, 0)
#define SC2721_LDO_VDDWIFIPA_VOL_MASK	GENMASK(7, 0)
#define SC2721_LDO_VDD18_DCXO_VOL_MASK	GENMASK(7, 0)
#define SC2721_LDO_VDDUSB33_VOL_MASK	GENMASK(7, 0)
#define SC2721_LDO_VDDCAMD_VOL_MASK	GENMASK(6, 0)
#define SC2721_LDO_VDDCON_VOL_MASK	GENMASK(6, 0)
#define SC2721_LDO_VDDCAMIO_VOL_MASK	GENMASK(6, 0)
#define SC2721_LDO_AVDD18_VOL_MASK	GENMASK(6, 0)
#define SC2721_LDO_VDDRF18_VOL_MASK	GENMASK(6, 0)
#define SC2721_LDO_VDDRF125_VOL_MASK	GENMASK(6, 0)
#define SC2721_LDO_VDDMEM_VOL_MASK	GENMASK(6, 0)
#define SC2721_LDO_VDDKPLED_VOL_MASK	GENMASK(14, 7)

enum sc2721_regulator_id {
	SC2721_DCDC_CPU,
	SC2721_DCDC_CORE,
	SC2721_DCDC_MEM,
	SC2721_DCDC_GEN,
	SC2721_LDO_AVDD18,
	SC2721_LDO_VDDRF18,
	SC2721_LDO_VDDCAMIO,
	SC2721_LDO_VDDCAMD,
	SC2721_LDO_VDDCON,
	SC2721_LDO_VDDRF125,
	SC2721_LDO_VDDCAMA,
	SC2721_LDO_VDDCAMMOT,
	SC2721_LDO_VDDSIM0,
	SC2721_LDO_VDDSIM1,
	SC2721_LDO_VDDSIM2,
	SC2721_LDO_VDDEMMCCORE,
	SC2721_LDO_VDDSDCORE,
	SC2721_LDO_VDDSDIO,
	SC2721_LDO_VDD28,
	SC2721_LDO_VDDWIFIPA,
	SC2721_LDO_VDDDCXO,
	SC2721_LDO_VDDUSB33,
	SC2721_LDO_VDDLDOMEM,
	SC2721_LDO_VDDKPLED,
	SC2721_LDO_AVDD18_SLP,	//SLP PD & SLP_LP
};

/* only for sc27xx Project */
static int sprd_regulator_disable_regmap(struct regulator_dev *rdev)
{
	int id = 0, ret = 0;

	id = rdev_get_id(rdev);
	if (id == SC2721_LDO_VDDSDCORE || id == SC2721_LDO_VDDSDIO) {
		ret = regulator_disable_regmap(rdev);
		usleep_range(10 * 1000, 10 * 1250);
		return ret;
	} else {
		return regulator_disable_regmap(rdev);
	}
};

static const struct regulator_ops sc2721_regu_linear_ops = {
	.enable = regulator_enable_regmap,
	.disable = sprd_regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
};

#define SC2721_REGU_LINEAR(_id, en_reg, en_mask, vreg, vmask,	\
			  vstep, vmin, vmax, min_sel) {		\
	.name			= #_id,				\
	.of_match		= of_match_ptr(#_id),		\
	.ops			= &sc2721_regu_linear_ops,	\
	.type			= REGULATOR_VOLTAGE,		\
	.id			= SC2721_##_id,			\
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
}

static struct regulator_desc regulators[] = {
	SC2721_REGU_LINEAR(DCDC_CPU, SC2721_POWER_PD_SW,
			   SC2721_DCDC_CPU_PD_MASK, SC2721_DCDC_CPU_VOL,
			   SC2721_DCDC_CPU_VOL_MASK, 3125U, 400000U, 1996000U,
			   0),
	SC2721_REGU_LINEAR(DCDC_CORE, SC2721_POWER_PD_SW,
			   SC2721_DCDC_CORE_PD_MASK, SC2721_DCDC_CORE_VOL,
			   SC2721_DCDC_CORE_VOL_MASK, 3125U, 400000U, 1996000U,
			   0),
	SC2721_REGU_LINEAR(DCDC_MEM, SC2721_POWER_PD_SW,
			   SC2721_DCDC_MEM_PD_MASK, SC2721_DCDC_MEM_VOL,
			   SC2721_DCDC_MEM_VOL_MASK, 3125U, 400000U, 3596875U,
			   0),
	SC2721_REGU_LINEAR(DCDC_GEN, SC2721_POWER_PD_SW,
			   SC2721_DCDC_GEN_PD_MASK, SC2721_DCDC_GEN_VOL,
			   SC2721_DCDC_GEN_VOL_MASK, 3125U, 600000U, 3796875U,
			   0),
	SC2721_REGU_LINEAR(LDO_VDDCAMA, SC2721_LDO_VDDCAMA_PD,
			   SC2721_LDO_VDDCAMA_PD_MASK, SC2721_LDO_VDDCAMA_VOL,
			   SC2721_LDO_VDDCAMA_VOL_MASK, 10000U, 1200000U,
			   3750000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDCAMMOT, SC2721_LDO_VDDCAMMOT_PD,
			   SC2721_LDO_VDDCAMMOT_PD_MASK,
			   SC2721_LDO_VDDCAMMOT_VOL,
			   SC2721_LDO_VDDCAMMOT_VOL_MASK, 10000U, 1200000U,
			   3750000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDSIM2, SC2721_LDO_VDDSIM2_PD,
			   SC2721_LDO_VDDSIM2_PD_MASK, SC2721_LDO_VDDSIM2_VOL,
			   SC2721_LDO_VDDSIM2_VOL_MASK, 10000U, 1200000U,
			   3750000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDEMMCCORE, SC2721_LDO_VDDEMMCCORE_PD,
			   SC2721_LDO_VDDEMMCCORE_PD_MASK,
			   SC2721_LDO_VDDEMMCCORE_VOL,
			   SC2721_LDO_VDDEMMCCORE_VOL_MASK, 10000U, 1200000U,
			   3750000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDSDCORE, SC2721_LDO_VDDSDCORE_PD,
			   SC2721_LDO_VDDSDCORE_PD_MASK,
			   SC2721_LDO_VDDSDCORE_VOL,
			   SC2721_LDO_VDDSDCORE_VOL_MASK, 10000U, 1200000U,
			   3750000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDSDIO, SC2721_LDO_VDDSDIO_PD,
			   SC2721_LDO_VDDSDIO_PD_MASK, SC2721_LDO_VDDSDIO_VOL,
			   SC2721_LDO_VDDSDIO_VOL_MASK, 10000U, 1200000U,
			   3750000U, 0),
	SC2721_REGU_LINEAR(LDO_VDD28, SC2721_POWER_PD_SW,
			   SC2721_LDO_VDD28_PD_MASK, SC2721_LDO_VDD28_VOL,
			   SC2721_LDO_VDD28_VOL_MASK, 10000U, 1200000U,
			   3750000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDWIFIPA, SC2721_LDO_VDDWIFIPA_PD,
			   SC2721_LDO_VDDWIFIPA_PD_MASK,
			   SC2721_LDO_VDDWIFIPA_VOL,
			   SC2721_LDO_VDDWIFIPA_VOL_MASK, 10000U, 1200000U,
			   3750000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDDCXO, SC2721_POWER_PD_SW,
			   SC2721_LDO_VDD18_DCXO_PD_MASK,
			   SC2721_LDO_VDD18_DCXO_VOL,
			   SC2721_LDO_VDD18_DCXO_VOL_MASK, 10000U, 1200000U,
			   3750000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDUSB33, SC2721_LDO_VDDUSB33_PD,
			   SC2721_LDO_VDDUSB33_PD_MASK, SC2721_LDO_VDDUSB33_VOL,
			   SC2721_LDO_VDDUSB33_VOL_MASK, 10000U, 1200000U,
			   3750000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDCAMD, SC2721_LDO_VDDCAMD_PD,
			   SC2721_LDO_VDDCAMD_PD_MASK, SC2721_LDO_VDDCAMD_VOL,
			   SC2721_LDO_VDDCAMD_VOL_MASK, 6250U, 1006250U,
			   1500000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDCON, SC2721_LDO_VDDCON_PD,
			   SC2721_LDO_VDDCON_PD_MASK, SC2721_LDO_VDDCON_VOL,
			   SC2721_LDO_VDDCON_VOL_MASK, 6250U, 1106250U,
			   1600000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDCAMIO, SC2721_LDO_VDDCAMIO_PD,
			   SC2721_LDO_VDDCAMIO_PD_MASK, SC2721_LDO_VDDCAMIO_VOL,
			   SC2721_LDO_VDDCAMIO_VOL_MASK, 6250U, 1106250U,
			   1900000U, 0),
	SC2721_REGU_LINEAR(LDO_AVDD18, SC2721_POWER_PD_SW,
			   SC2721_LDO_AVDD18_PD_MASK, SC2721_LDO_AVDD18_VOL,
			   SC2721_LDO_AVDD18_VOL_MASK, 6250U, 1106250U,
			   1900000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDRF18, SC2721_LDO_VDDRF18_PD,
			   SC2721_LDO_VDDRF18_PD_MASK, SC2721_LDO_VDDRF18_VOL,
			   SC2721_LDO_VDDRF18_VOL_MASK, 6250U, 1106250U,
			   1900000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDRF125, SC2721_LDO_VDDRF125_PD,
			   SC2721_LDO_VDDRF125_PD_MASK,
			   SC2721_LDO_VDDRF125_VOL,
			   SC2721_LDO_VDDRF125_VOL_MASK, 6250U, 1106250U,
			   1400000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDLDOMEM, SC2721_POWER_PD_SW,
			   SC2721_LDO_VDDMEM_PD_MASK, SC2721_LDO_VDDMEM_VOL,
			   SC2721_LDO_VDDMEM_VOL_MASK, 6250U, 1106250U,
			   1400000U, 0),
	SC2721_REGU_LINEAR(LDO_VDDKPLED, SC2721_LDO_VDDKPLED_PD,
			   SC2721_LDO_VDDKPLED_PD_MASK, SC2721_LDO_VDDKPLED_VOL,
			   SC2721_LDO_VDDKPLED_VOL_MASK, 10000U, 1200000U,
			   3750000U, 0),
	SC2721_REGU_LINEAR(LDO_AVDD18_SLP, ANA_REG_GLB_SLP_LDO_PD_CTRL1,
			   BIT_SLP_LDOAVDD18_PD_EN, SC2721_LDO_VDDCAMIO_VOL,
			   SC2721_LDO_VDDCAMIO_VOL_MASK, 6250U, 1106250U,
			   1900000U, 0),
};

static struct dentry *debugfs_root;

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

static void sc2721_regulator_debugfs_init(struct regulator_dev *rdev)
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

static int sc2721_regulator_unlock(struct regmap *regmap)
{
	return regmap_write(regmap, SC2721_PWR_WR_PROT, SC2721_WR_UNLOCK_VALUE);
}

static int sc2721_regulator_probe(struct platform_device *pdev)
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

	ret = sc2721_regulator_unlock(regmap);
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
		sc2721_regulator_debugfs_init(rdev);
	}
	return 0;
}

static int sc2721_regulator_remove(struct platform_device *pdev)
{
	debugfs_remove_recursive(debugfs_root);
	return 0;
}

static const struct of_device_id sc2721_regulator_match[] = {
	{ .compatible = "sprd,sc2721-regulator" },
	{},
};
MODULE_DEVICE_TABLE(of, sc2721_regulator_match);

static struct platform_driver sc2721_regulator_driver = {
	.driver = {
		.name = "sc2721-regulator",
		.of_match_table = sc2721_regulator_match,
	},
	.probe = sc2721_regulator_probe,
	.remove = sc2721_regulator_remove,
};

module_platform_driver(sc2721_regulator_driver);

MODULE_AUTHOR("Sherry Zong <sherry.zong@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum SC2721 regulator driver");
MODULE_LICENSE("GPL v2");
