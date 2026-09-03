// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/sched.h>

#define UMP9621_REGULATOR_BASE		0x2000

/* UMP9621 regulator lock register */
#define UMP9621_WR_UNLOCK_VALUE		0x6e7f
#define UMP9621_PWR_WR_PROT		(UMP9621_REGULATOR_BASE + 0x134)

/*
 * UMP9621 enable register
 */
#define UMP9621_POWER_PD_SW		(UMP9621_REGULATOR_BASE + 0x01c)

/*
 * UMP9621 enable mask
 */
#define UMP9621_DCDC_SRAM1_PD_MASK		BIT(8)
#define UMP9621_DCDC_SRAM2_PD_MASK		BIT(7)
#define UMP9621_DCDC_BOB_PD_MASK		BIT(6)
#define UMP9621_DCDC_AI_PD_MASK			BIT(5)
#define UMP9621_DCDC_MODEM_PD_MASK		BIT(4)
#define UMP9621_DCDC_CPU0_PD_MASK		BIT(2)
#define UMP9621_DCDC_MM_PD_MASK			BIT(1)
/*
 * UMP9621 vsel register
 */
#define UMP9621_DCDC_SRAM1_VOL		(UMP9621_REGULATOR_BASE + 0x0e8)
#define UMP9621_DCDC_SRAM2_VOL		(UMP9621_REGULATOR_BASE + 0x74)
#define UMP9621_DCDC_BOB_VOL		(UMP9621_REGULATOR_BASE + 0x1bc)
#define UMP9621_DCDC_AI_VOL		(UMP9621_REGULATOR_BASE + 0x1a0)
#define UMP9621_DCDC_MODEM_VOL		(UMP9621_REGULATOR_BASE + 0x54)
#define UMP9621_DCDC_CPU0_VOL		(UMP9621_REGULATOR_BASE + 0x44)
#define UMP9621_DCDC_MM_VOL		(UMP9621_REGULATOR_BASE + 0x180)

/*
 * UMP9621 vsel register mask
 */
#define UMP9621_DCDC_SRAM1_VOL_MASK		GENMASK(8, 0)
#define UMP9621_DCDC_SRAM2_VOL_MASK		GENMASK(8, 0)
#define UMP9621_DCDC_BOB_VOL_MASK		GENMASK(8, 0)
#define UMP9621_DCDC_AI_VOL_MASK		GENMASK(8, 0)
#define UMP9621_DCDC_MODEM_VOL_MASK		GENMASK(8, 0)
#define UMP9621_DCDC_CPU0_VOL_MASK		GENMASK(8, 0)
#define UMP9621_DCDC_MM_VOL_MASK		GENMASK(8, 0)

#define SPRD_MIN_VOLTAGE_UV	300000

enum ump9621_regulator_id {
	UMP9621_DCDC_SRAM1,
	UMP9621_DCDC_SRAM2,
	UMP9621_DCDC_BOB,
	UMP9621_DCDC_AI,
	UMP9621_DCDC_MODEM,
	UMP9621_DCDC_CPU0,
	UMP9621_DCDC_MM,
};

static struct dentry *debugfs_root;

static int regulator_set_voltage_sel_sprd(struct regulator_dev *rdev, unsigned int sel)
{
	if (!rdev->desc->min_uV) {
		if (sel < (SPRD_MIN_VOLTAGE_UV / rdev->desc->uV_step)) {
			char kevent[] = "kevent_begin:{\"event_id\":\"107000001\",\"event_time\"";
			char *pr_str[2];

			pr_str[0] = kasprintf(GFP_KERNEL,
					      "%s:%ld,\"task\":%s,\"PID\":%d,\"%s[0x%04x]\":0x%04x}:kevent_end\n",
					      kevent, jiffies, current->comm, current->pid,
					      rdev->desc->name, rdev->desc->vsel_reg, sel);
			pr_str[1] = NULL;
			kobject_uevent_env(&rdev->dev.kobj, KOBJ_CHANGE, pr_str);
			kfree(pr_str[0]);
		}
	}
	return regulator_set_voltage_sel_regmap(rdev, sel);
};

static const struct regulator_ops ump9621_regu_linear_ops = {
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_sprd,
};

#define UMP9621_REGU_LINEAR(_id, en_reg, en_mask, vreg, vmask,	\
			  vstep, vmin, vmax, min_sel) {		\
	.name			= #_id,				\
	.of_match		= of_match_ptr(#_id),		\
	.ops			= &ump9621_regu_linear_ops,	\
	.type			= REGULATOR_VOLTAGE,		\
	.id			= UMP9621_##_id,		\
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
	UMP9621_REGU_LINEAR(DCDC_SRAM1, UMP9621_POWER_PD_SW,
			    UMP9621_DCDC_SRAM1_PD_MASK, UMP9621_DCDC_SRAM1_VOL,
			    UMP9621_DCDC_SRAM1_VOL_MASK, 3125, 0, 1596875, 0),
	UMP9621_REGU_LINEAR(DCDC_SRAM2, UMP9621_POWER_PD_SW,
			    UMP9621_DCDC_SRAM2_PD_MASK, UMP9621_DCDC_SRAM2_VOL,
			    UMP9621_DCDC_SRAM2_VOL_MASK, 3125, 0, 1596875, 0),
	UMP9621_REGU_LINEAR(DCDC_AI, UMP9621_POWER_PD_SW,
			    UMP9621_DCDC_AI_PD_MASK, UMP9621_DCDC_AI_VOL,
			    UMP9621_DCDC_AI_VOL_MASK, 3125, 0, 1596875, 0),
	UMP9621_REGU_LINEAR(DCDC_MODEM, UMP9621_POWER_PD_SW,
			    UMP9621_DCDC_MODEM_PD_MASK, UMP9621_DCDC_MODEM_VOL,
			    UMP9621_DCDC_MODEM_VOL_MASK, 3125, 0, 1596875, 0),
	UMP9621_REGU_LINEAR(DCDC_CPU0, UMP9621_POWER_PD_SW,
			    UMP9621_DCDC_CPU0_PD_MASK, UMP9621_DCDC_CPU0_VOL,
			    UMP9621_DCDC_CPU0_VOL_MASK, 3125, 0, 1596875, 0),
	UMP9621_REGU_LINEAR(DCDC_MM, UMP9621_POWER_PD_SW,
			    UMP9621_DCDC_MM_PD_MASK, UMP9621_DCDC_MM_VOL,
			    UMP9621_DCDC_MM_VOL_MASK, 3125, 0, 1596875, 0),
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

DEFINE_SIMPLE_ATTRIBUTE(fops_enable, debugfs_enable_get, debugfs_enable_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_ldo, debugfs_voltage_get, debugfs_voltage_set, "%llu\n");

static void ump9621_regulator_debugfs_init(struct regulator_dev *rdev)
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

static int ump9621_regulator_unlock(struct regmap *regmap)
{
	return regmap_write(regmap, UMP9621_PWR_WR_PROT, UMP9621_WR_UNLOCK_VALUE);
}

static int ump9621_regulator_probe(struct platform_device *pdev)
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

	ret = ump9621_regulator_unlock(regmap);
	if (ret) {
		dev_err(&pdev->dev, "failed to release regulator lock\n");
		return ret;
	}

	config.dev = &pdev->dev;
	config.regmap = regmap;

	debugfs_root = debugfs_lookup("sprd_regulator", NULL);
	if (!debugfs_root)
		debugfs_root = debugfs_create_dir("sprd_regulator", NULL);
	if (IS_ERR(debugfs_root))
		dev_warn(&pdev->dev, "Failed to create debugfs directory\n");

	for (i = 0; i < ARRAY_SIZE(regulators); i++) {
		rdev = devm_regulator_register(&pdev->dev, &regulators[i], &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev, "failed to register regulator %s\n",
				regulators[i].name);
			continue;
		}
		ump9621_regulator_debugfs_init(rdev);
	}

	return 0;
}

static int ump9621_regulator_remove(struct platform_device *pdev)
{
	debugfs_remove_recursive(debugfs_root);
	return 0;
}

static const struct of_device_id ump9621_regulator_match[] = {
	{ .compatible = "sprd,ump9621-regulator" },
	{},
};
MODULE_DEVICE_TABLE(of, ump9621_regulator_match);

static struct platform_driver ump9621_regulator_driver = {
	.driver = {
		   .name = "ump9621-regulator",
		   .of_match_table = ump9621_regulator_match,
		   },
	.probe = ump9621_regulator_probe,
	.remove = ump9621_regulator_remove,
};

module_platform_driver(ump9621_regulator_driver);

MODULE_AUTHOR("Zhongfa.Wang <zhongfa.wang@unisoc.com>");
MODULE_DESCRIPTION("Unisoc ump9621 regulator driver");
MODULE_LICENSE("GPL v2");
