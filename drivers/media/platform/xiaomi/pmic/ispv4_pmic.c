// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022, Xiaomi, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "ispv4 pmic: " fmt

#include <linux/printk.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <uapi/media/ispv4_defs.h>
#include <linux/mfd/ispv4_defs.h>
#include <linux/component.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/bits.h>
#include <linux/list.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/debug-regulator.h>
#include "../regulator/internal.h"
#include "ispv4_regops.h"
#include "pmic/ispv4_pmic.h"

#define BUCK_SLAVE_ADDR 0x25
#define BUCK_X_ENABLE_BASE 0x24
#define BUCK_E_REG(x) (0x20 * (x) + 0x24)
#define BUCK_ENABLE_MASK 0x80
#define BUCK_ENABLE(reg) (reg |= BUCK_ENABLE_MASK)
#define BUCK_DISABLE(reg) (reg &= ~(BUCK_ENABLE_MASK))
#define BUCK_REG(x) (0x20 * ((x)) + 0x40)
#define BUCK_VSET0(x) (BUCK_REG(x) + 0x2)
#define BUCK_VSET_MASK GENMASK(6,0)
#define BUCK_EN_REG(x) (BUCK_REG(x) + 0x4)
#define BUCK_EN_MASK GENMASK(7,7)
#define BUCK_VOUT_RANGE(x) (BUCK_REG(x) + 0x6)
#define BUCK_VOUT_RANGE_MASK GENMASK(1,1)

#define BUCK7_REG 0x04
#define BUCK7_ADDR 0x28
#define BUCK7_VSET_REG 0x2
#define BUCK7_RANGE_RGE 0x6
#define BUCK7_EN_RGE 0x4

#define LDO_SLAVE_ADDR 0x28
#define LDO_1_REG 0x22
#define LDO_4_REG 0x68
#define LDO_5_REG 0x42
#define LDO_ENABLE_MASK 0x80
#define LDO_ENABLE(reg) (reg |= LDO_ENABLE_MASK)
#define LDO_DISABLE(reg) (reg &= ~(LDO_ENABLE_MASK))
#define LDO1_REG (0x20)
#define LDO2_REG (0x26)
#define LDO3_REG (0x46)
#define LDO4_REG (0x66)
#define LDO5_REG (0x40)
#define LDO6_REG (0x60)
#define LDO_VSET_RANGE(ldox_reg) (ldox_reg + 0x1)
#define LDO_VSET_MASK GENMASK(5,0)
#define LDO_EN_REG(ldox_reg) (ldox_reg + 0x2)
#define LDO_EN_MASK GENMASK(7,7)
#define LDO12_VOUT_RANGE_MASK GENMASK(6,6)
#define LDO_VOUT_RANGE_MASK GENMASK(7,7)

extern struct dentry *ispv4_debugfs;
static u8 debug_reg;
static u8 slave_addr = 0x25;
#ifdef PMIC_REGU
static u32 debug_regulator_id = 0;
#endif

struct ispv4_pmic_data {
	struct device comp_dev;
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regm;
	struct dentry *dbg;
	struct dentry *iic_reg;
	struct dentry *regulator;
	struct list_head debug_regu;
	const char *regu_name[ACT88760_ID_MAX];
};

struct debug_regu {
	struct list_head	list;
	enum act88760_reg_id id;
	struct regulator	*reg;
	struct device		*dev;
	struct regulator_dev	*rdev;
};

static struct regmap_config ispv4_pmic_regmap_cfg = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0xFF,
};

static struct ispv4_pmic_data *v4pmic;

#ifdef PMIC_REGU
static void regulator_set_slave_addr(struct regulator_dev *rdev) {
	if (!v4pmic) {
		pr_err("v4pmic is NULL");
		return;
	}
	if (rdev->desc->id > ACT88760_ID_BUCK_REG6) {
		v4pmic->client->addr = LDO_SLAVE_ADDR;
	} else {
		v4pmic->client->addr = BUCK_SLAVE_ADDR;
	}
}

static const char *rdev_name(struct regulator_dev *rdev)
{
	if (rdev->constraints && rdev->constraints->name)
		return rdev->constraints->name;
	else if (rdev->desc->name)
		return rdev->desc->name;
	else
		return "";
}

static int act88760_list_voltage_pickable(struct regulator_dev *rdev,
				 unsigned int selector)
{
	int ret = 0;

	dev_info(&rdev->dev ,"%s into %s selector %d", rdev_name(rdev), __func__, selector);
	regulator_set_slave_addr(rdev);
	ret = regulator_list_voltage_pickable_linear_range(rdev, selector);
	dev_info(&rdev->dev ,"%s list %dmV", rdev_name(rdev), ret);

	return ret;
}

static int act88760_map_voltage_pickable(struct regulator_dev *rdev,
				int min_uV, int max_uV)
{
	int ret = 0;

	dev_info(&rdev->dev ,"%s into %s", rdev_name(rdev), __func__);
	regulator_set_slave_addr(rdev);
	ret = regulator_map_voltage_pickable_linear_range(rdev, min_uV, max_uV);

	return ret;
}

static int act88760_get_voltage_sel_pickable(struct regulator_dev *rdev)
{
	int ret = 0;

	dev_info(&rdev->dev ,"%s into %s", rdev_name(rdev), __func__);
	regulator_set_slave_addr(rdev);
	ret = regulator_get_voltage_sel_pickable_regmap(rdev);


	return ret;
}

static int act88760_set_voltage_sel_pickable(struct regulator_dev *rdev,
				    unsigned int sel)
{
	int ret = 0;

	dev_info(&rdev->dev ,"%s into %s", rdev_name(rdev), __func__);
	regulator_set_slave_addr(rdev);
	ret = regulator_set_voltage_sel_pickable_regmap(rdev, sel);

	return ret;
}

static int act88760_regulator_enable(struct regulator_dev *rdev)
{
	dev_info(&rdev->dev ,"%s into %s", rdev_name(rdev), __func__);
	regulator_set_slave_addr(rdev);
	return regulator_enable_regmap(rdev);
}

static int act88760_regulator_disable(struct regulator_dev *rdev)
{
	dev_info(&rdev->dev ,"%s into %s",  rdev_name(rdev), __func__);
	regulator_set_slave_addr(rdev);
	return regulator_disable_regmap(rdev);
}

static int act88760_regulator_is_enable(struct regulator_dev *rdev)
{
	dev_info(&rdev->dev ,"%s into %s", rdev_name(rdev), __func__);
	regulator_set_slave_addr(rdev);
	return regulator_is_enabled_regmap(rdev);
}

static const struct linear_range act88760_voltage_ranges_pickable[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 127, 5000),
	REGULATOR_LINEAR_RANGE(500000, 0, 127, 25000),
};

static const unsigned int act88760_voltage_range_pickable_sel_buck_1_2[] = {
	0x0, 0x2
};

static const unsigned int act88760_voltage_range_pickable_sel_buck_3_4[] = {
	0x0, 0x8
};

static const struct linear_range act88760_voltage_ranges_pickable_ldo_1_2[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 63, 12500),
	REGULATOR_LINEAR_RANGE(1200000, 0, 63, 12500),
};

static const unsigned int act88760_voltage_range_pickable_sel_ldo_1_2[] = {
	0x0, 0x40
};

static const struct linear_range act88760_voltage_ranges_pickable_ldo[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 63, 12500),
	REGULATOR_LINEAR_RANGE(1000000, 0, 63, 50000),
};

static const unsigned int act88760_voltage_range_pickable_sel_ldo[] = {
	0x0, 0x80
};

struct regulator_ops act88760_ops_pickable_linear_range = {
	.list_voltage		= act88760_list_voltage_pickable,
	.map_voltage		= act88760_map_voltage_pickable,
	.get_voltage_sel	= act88760_get_voltage_sel_pickable,
	.set_voltage_sel	= act88760_set_voltage_sel_pickable,
	.enable			= act88760_regulator_enable,
	.disable		= act88760_regulator_disable,
	.is_enabled		= act88760_regulator_is_enable,
};


static int act88760_list_voltage(struct regulator_dev *rdev,
				 unsigned int selector)
{
	int ret = 0;

	dev_info(&rdev->dev ,"%s into %s selector %d", rdev_name(rdev), __func__, selector);
	regulator_set_slave_addr(rdev);
	ret = regulator_list_voltage_linear_range(rdev, selector);
	dev_info(&rdev->dev ,"%s list %dmV", rdev_name(rdev), ret);

	return ret;
}

static int act88760_map_voltage(struct regulator_dev *rdev,
				int min_uV, int max_uV)
{
	int ret = 0;

	dev_info(&rdev->dev ,"%s into %s", rdev_name(rdev), __func__);
	regulator_set_slave_addr(rdev);
	ret = regulator_map_voltage_linear_range(rdev, min_uV, max_uV);

	return ret;
}

static int act88760_get_voltage_sel(struct regulator_dev *rdev)
{
	int ret = 0;

	dev_info(&rdev->dev ,"%s into %s", rdev_name(rdev), __func__);
	regulator_set_slave_addr(rdev);
	ret = regulator_get_voltage_sel_regmap(rdev);

	return ret;
}

static int act88760_set_voltage_sel(struct regulator_dev *rdev,
				    unsigned int sel)
{
	int ret = 0;

	dev_info(&rdev->dev ,"%s into %s", rdev_name(rdev), __func__);
	regulator_set_slave_addr(rdev);
	ret = regulator_set_voltage_sel_regmap(rdev, sel);

	return ret;
}

static const struct linear_range act88760_voltage_ranges[] = {
	REGULATOR_LINEAR_RANGE(500000, 0, 127, 25000),
};

struct regulator_ops act88760_ops_linear_range = {
	.list_voltage		= act88760_list_voltage,
	.map_voltage		= act88760_map_voltage,
	.get_voltage_sel	= act88760_get_voltage_sel,
	.set_voltage_sel	= act88760_set_voltage_sel,
	.enable			= act88760_regulator_enable,
	.disable		= act88760_regulator_disable,
	.is_enabled		= act88760_regulator_is_enable,
};

#define ACT88760_BUCK_1_2_REG_DESC(_name, _id)	\
	[_id] = {							\
		.name			= _name,			\
		.of_match		= of_match_ptr(_name),		\
		.regulators_node	= of_match_ptr("regulators"),	\
		.id			= _id,				\
		.type			= REGULATOR_VOLTAGE,		\
		.ops			= &act88760_ops_pickable_linear_range,\
		.n_voltages		= 256,				\
		.linear_ranges		= act88760_voltage_ranges_pickable,\
		.linear_range_selectors = act88760_voltage_range_pickable_sel_buck_1_2,\
		.n_linear_ranges	= ARRAY_SIZE(act88760_voltage_ranges_pickable),\
		.vsel_range_reg		= BUCK_VOUT_RANGE(_id),		\
		.vsel_range_mask	= BUCK_VOUT_RANGE_MASK,		\
		.vsel_reg		= BUCK_VSET0(_id),		\
		.vsel_mask		= BUCK_VSET_MASK,		\
		.enable_reg		= BUCK_EN_REG(_id),		\
		.enable_mask		= BUCK_EN_MASK,			\
		.owner			= THIS_MODULE,			\
	}

#define ACT88760_BUCK_3_4_REG_DESC(_name, _id)	\
	[_id] = {							\
		.name			= _name,			\
		.of_match		= of_match_ptr(_name),		\
		.regulators_node	= of_match_ptr("regulators"),	\
		.id			= _id,				\
		.type			= REGULATOR_VOLTAGE,		\
		.ops			= &act88760_ops_pickable_linear_range,\
		.n_voltages		= 256,				\
		.linear_ranges		= act88760_voltage_ranges_pickable,\
		.linear_range_selectors = act88760_voltage_range_pickable_sel_buck_3_4,\
		.n_linear_ranges	= ARRAY_SIZE(act88760_voltage_ranges_pickable),\
		.vsel_range_reg		= (BUCK_REG(_id) + 0x1),	\
		.vsel_range_mask	= GENMASK(3,3),			\
		.vsel_reg		= BUCK_VSET0(_id),		\
		.vsel_mask		= BUCK_VSET_MASK,		\
		.enable_reg		= BUCK_EN_REG(_id),		\
		.enable_mask		= BUCK_EN_MASK,			\
		.owner			= THIS_MODULE,			\
	}

#define ACT88760_BUCK_5_6_REG_DESC(_name, _id)	\
	[_id] = {							\
		.name			= _name,			\
		.of_match		= of_match_ptr(_name),		\
		.regulators_node	= of_match_ptr("regulators"),	\
		.id			= _id,				\
		.type			= REGULATOR_VOLTAGE,		\
		.ops			= &act88760_ops_linear_range,	\
		.n_voltages		= 128,				\
		.linear_ranges		= act88760_voltage_ranges,	\
		.n_linear_ranges	= ARRAY_SIZE(act88760_voltage_ranges),\
		.vsel_reg		= BUCK_VSET0(_id),		\
		.vsel_mask		= BUCK_VSET_MASK,		\
		.enable_reg		= BUCK_EN_REG(_id),		\
		.enable_mask		= BUCK_EN_MASK,			\
		.owner			= THIS_MODULE,			\
	}

#define ACT88760_LDO_1_2_REG_DESC(_name, _id, _reg)	\
	[_id] = {							\
		.name			= _name,			\
		.of_match		= of_match_ptr(_name),	\
		.regulators_node	= of_match_ptr("regulators"),	\
		.id			= _id,				\
		.type			= REGULATOR_VOLTAGE,		\
		.ops			= &act88760_ops_pickable_linear_range,\
		.n_voltages		= 128,				\
		.linear_ranges		= act88760_voltage_ranges_pickable_ldo_1_2,\
		.linear_range_selectors = act88760_voltage_range_pickable_sel_ldo_1_2,\
		.n_linear_ranges	= ARRAY_SIZE(act88760_voltage_ranges_pickable_ldo_1_2),\
		.vsel_range_reg		= LDO_VSET_RANGE(_reg),		\
		.vsel_range_mask	= LDO12_VOUT_RANGE_MASK,	\
		.vsel_reg		= LDO_VSET_RANGE(_reg),		\
		.vsel_mask		= LDO_VSET_MASK,		\
		.enable_reg		= LDO_EN_REG(_reg),		\
		.enable_mask		= LDO_EN_MASK,			\
		.owner			= THIS_MODULE,			\
	}

#define ACT88760_LDO_REG_DESC(_name, _id, _reg, _supply)	\
	[_id] = {							\
		.name			= _name,			\
		.supply_name		= _supply,			\
		.of_match		= of_match_ptr(_name),		\
		.regulators_node	= of_match_ptr("regulators"),	\
		.id			= _id,				\
		.type			= REGULATOR_VOLTAGE,		\
		.ops			= &act88760_ops_pickable_linear_range,\
		.n_voltages		= 128,				\
		.linear_ranges		= act88760_voltage_ranges_pickable_ldo,\
		.linear_range_selectors = act88760_voltage_range_pickable_sel_ldo,\
		.n_linear_ranges	= ARRAY_SIZE(act88760_voltage_ranges_pickable_ldo),\
		.vsel_range_reg		= LDO_VSET_RANGE(_reg),		\
		.vsel_range_mask	= LDO_VOUT_RANGE_MASK,		\
		.vsel_reg		= LDO_VSET_RANGE(_reg),		\
		.vsel_mask		= LDO_VSET_MASK,		\
		.enable_reg		= LDO_EN_REG(_reg),		\
		.enable_mask		= LDO_EN_MASK,			\
		.min_dropout_uV		= 50000,			\
		.owner			= THIS_MODULE,			\
	}

static const struct regulator_desc act88760_reg_desc[ACT88760_ID_MAX] = {
	ACT88760_BUCK_1_2_REG_DESC("BUCK_REG1", ACT88760_ID_BUCK_REG1),
	ACT88760_BUCK_1_2_REG_DESC("BUCK_REG2", ACT88760_ID_BUCK_REG2),
	ACT88760_BUCK_3_4_REG_DESC("BUCK_REG3", ACT88760_ID_BUCK_REG3),
	ACT88760_BUCK_3_4_REG_DESC("BUCK_REG4", ACT88760_ID_BUCK_REG4),
	ACT88760_BUCK_5_6_REG_DESC("BUCK_REG5", ACT88760_ID_BUCK_REG5),
	ACT88760_BUCK_5_6_REG_DESC("BUCK_REG6", ACT88760_ID_BUCK_REG6),
	[ACT88760_ID_BUCK_REG7] = {
		.name			= "BUCK_REG7",
		.of_match		= of_match_ptr("BUCK_REG7"),
		.regulators_node	= of_match_ptr("regulators"),
		.id			= ACT88760_ID_BUCK_REG7,
		.type			= REGULATOR_VOLTAGE,
		.ops			= &act88760_ops_pickable_linear_range,
		.n_voltages		= 256,
		.linear_ranges		= act88760_voltage_ranges_pickable,
		.linear_range_selectors = act88760_voltage_range_pickable_sel_buck_1_2,
		.n_linear_ranges	= ARRAY_SIZE(act88760_voltage_ranges_pickable),
		.vsel_range_reg		= BUCK7_RANGE_RGE,
		.vsel_range_mask	= BUCK_VOUT_RANGE_MASK,
		.vsel_reg		= BUCK7_VSET_REG,
		.vsel_mask		= BUCK_VSET_MASK,
		.enable_reg		= BUCK7_EN_RGE,
		.enable_mask		= BUCK_EN_MASK,
		.owner			= THIS_MODULE,
	},
	ACT88760_LDO_1_2_REG_DESC("LDO_REG1", ACT88760_ID_LDO_REG1, LDO1_REG),
	ACT88760_LDO_1_2_REG_DESC("LDO_REG2", ACT88760_ID_LDO_REG2, LDO2_REG),
	ACT88760_LDO_REG_DESC("LDO_REG3", ACT88760_ID_LDO_REG3, LDO3_REG, "vdd"),
	ACT88760_LDO_REG_DESC("LDO_REG4", ACT88760_ID_LDO_REG4, LDO4_REG, "vdd"),
	ACT88760_LDO_REG_DESC("LDO_REG5", ACT88760_ID_LDO_REG5, LDO5_REG, "vdd"),
	ACT88760_LDO_REG_DESC("LDO_REG6", ACT88760_ID_LDO_REG6, LDO6_REG, "vdd"),
};

__maybe_unused
static struct of_regulator_match act88760_matches[ACT88760_ID_MAX] = {
	{ .name = "BUCK_REG1", },
	{ .name = "BUCK_REG2", },
	{ .name = "BUCK_REG3", },
	{ .name = "BUCK_REG4", },
	{ .name = "BUCK_REG5", },
	{ .name = "BUCK_REG6", },
	{ .name = "BUCK_REG7", },
	{ .name = "LDO_REG1", },
	{ .name = "LDO_REG2", },
	{ .name = "LDO_REG3", },
	{ .name = "LDO_REG4", },
	{ .name = "LDO_REG5", },
	{ .name = "LDO_REG6", },
};
#endif

static int disable_buck(struct ispv4_pmic_data *pdata, int n)
{
	int ret;
	unsigned int tmp = 0;
	int reg = BUCK_E_REG(n);

	pdata->client->addr = BUCK_SLAVE_ADDR;
	if (n == 7) {
		reg = BUCK7_REG;
		pdata->client->addr = BUCK7_ADDR;
	}

	ret = regmap_read(pdata->regm, reg, &tmp);
	if (ret != 0) {
		pr_info("read buck %d failed. ret:%d\n", n, ret);
		goto err;
	}
	pr_info("read buck %d reg:0x%x = 0x%x . ret:%d\n", n, reg, tmp, ret);
	ret = regmap_write(pdata->regm, reg, BUCK_DISABLE(tmp));
	if (ret != 0) {
		pr_info("write buck %d failed. ret:%d\n", n, ret);
		goto err;
	}

err:
	return ret;
}

static int disable_ldo(struct ispv4_pmic_data *pdata, int n)
{
	int ret;
	unsigned int tmp = 0;
	u32 reg = 0;

	switch (n) {
	case 1:
		reg = LDO_1_REG;
		break;
	case 4:
		reg = LDO_4_REG;
		break;
	case 5:
		reg = LDO_5_REG;
		break;
	default:
		return -EINVAL;
	}

	pdata->client->addr = LDO_SLAVE_ADDR;
	ret = regmap_read(pdata->regm, reg, &tmp);
	if (ret != 0) {
		pr_info("read ldo %d failed. ret:%d\n", n, ret);
		goto err;
	}
	pr_info("read ldo %d reg:0x%x = 0x%x . ret:%d\n", n, reg, tmp, ret);
	ret = regmap_write(pdata->regm, reg, LDO_DISABLE(tmp));
	if (ret != 0) {
		pr_info("write ldo %d failed. ret:%d\n", n, ret);
		goto err;
	}

	return 0;

err:
	return ret;
}

static int config_analog_bypass(struct ispv4_pmic_data *pdata)
{
	int ret;

	ret = disable_buck(pdata, 6);
	if (ret != 0)
		return ret;

	ret = disable_ldo(pdata, 1);
	if (ret != 0)
		return ret;

	ret = disable_buck(pdata, 7);
	if (ret != 0)
		return ret;

	ret = disable_ldo(pdata, 4);
	if (ret != 0)
		return ret;

	ret = disable_ldo(pdata, 5);
	if (ret != 0)
		return ret;

	ret = disable_buck(pdata, 4);
	if (ret != 0)
		return ret;

	return 0;
}

static int config_digital_bypass(struct ispv4_pmic_data *pdata)
{
	int ret = 0;
	int i;
	typedef struct {
		uint32_t addr;
		uint32_t val;
	} reg_t;

	reg_t reg[] = {
		{0xD800000, 0xf00f},
		{0xD800200, 0x8000040},
		{0xD80022c, 0x3},
		{0xD800404, 0x3ff0f},
		{0xD800444, 0x19},
		{0xD88000c, 0x1},
		{0xD880018, 0x7},
		{0xD880100, 0x1},
		{0xD880100, 0x1},
		{0xD880200, 0x0},
		{0xD880204, 0x0},
		{0xD880400, 0x1},
		{0xD880504, 0x7},
		{0xD880510, 0xf000f},
		{0xD88051c, 0x1},
		{0xD880528, 0x1ff01ff},
		{0xD880534, 0x1ff01ff},
		{0xD88054c, 0x1},
		{0xD880558, 0x3f},
		{0xD880564, 0x3},
		{0xD880570, 0x3},
		{0xD900304, 0x3},
		{0xD900310, 0x9},
		{0xD900500, 0x1},
		{0xD900004, 0x1},
		{0xD900300, 0x7},
	};

	for (i = 0; i <  ARRAY_SIZE(reg); i ++) {
		ret = ispv4_regops_write(reg[i].addr, reg[i].val);
		if (ret) {
			dev_err(pdata->dev, "reg: %llx val: %llx write fail",
				 reg[i].addr, reg[i].val);
			break;
		} else {
			dev_info(pdata->dev, "reg: %llx val: %llx write success",
				 reg[i].addr, reg[i].val);
		}
	}

	return ret;
}

static int config_save_current(struct ispv4_pmic_data *pdata, bool on)
{
	int ret = 0;
	unsigned int reg = 0x34;
	u32 data = ~(1 << 7);
	unsigned int tmp;
	unsigned short slave_addr;

	if (on)
		data = 1 << 7;

	slave_addr = pdata->client->addr;

	pdata->client->addr = BUCK_SLAVE_ADDR;
	ret = regmap_read(pdata->regm, reg, &tmp);
	if (ret != 0) {
		dev_err(pdata->dev ,"read reg:0x%x failed. ret:%d\n", reg, ret);
		goto err;

	}
	dev_info(pdata->dev ,"read reg:0x%x = 0x%x . ret:%d\n", reg, tmp, ret);

	ret = regmap_write(pdata->regm, reg, (data&tmp));
	if (ret != 0) {
		dev_err(pdata->dev ,"write reg:0x%x failed. ret:%d\n", reg, ret);
	}

err:
	pdata->client->addr = slave_addr;

	return ret;
}

static int ispv4_pmic_config(struct ispv4_pmic_data *pdata,
			     enum ispv4_pmic_config cfg)
{
	int ret = 0;

	switch (cfg) {
	case ISPV4_PMIC_CONFIG_ANALOG_BYPASS:
		ret = config_analog_bypass(pdata);
		break;
	case ISPV4_PMIC_CONFIG_DIGITAL_BYPASS:
		ret = config_digital_bypass(pdata);
		break;
	case ISPV4_PMIC_CONFIG_SAVE_CURRENT_ON:
		ret = config_save_current(pdata, true);
		break;
	case ISPV4_PMIC_CONFIG_SAVE_CURRENT_OFF:
		ret = config_save_current(pdata, false);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef PMIC_REGU
static struct debug_regu *debug_regulator_get(enum act88760_reg_id id)
{
	struct debug_regu *dregu;

	list_for_each_entry (dregu, &v4pmic->debug_regu, list) {
		if (dregu->id == id) {
			return dregu;
		}
	}
	dregu = kmalloc(sizeof(struct debug_regu), GFP_KERNEL);

	dregu->reg = regulator_get(v4pmic->dev, v4pmic->regu_name[id]);
	if (IS_ERR_OR_NULL(dregu->reg)) {
		dev_err(v4pmic->dev, "get regulator %d fail, err", id);
		kfree(dregu);
		return ERR_PTR(-EINVAL);
	}
	dregu->id = id;
	dregu->rdev = dregu->reg->rdev;
	dregu->dev = dregu->reg->dev;
	list_add_tail(&dregu->list, &v4pmic->debug_regu);
	return dregu;
}

static int ispv4_pmic_regulator(struct ispv4_pmic_data *pdata, uint32_t ops,
				uint32_t id, uint32_t *en, uint32_t *v)
{
	int ret = 0;
	struct regulator *regu;
	struct debug_regu *dregu;

	if (id < 0 || id >= ACT88760_ID_MAX) {
		dev_err(pdata->dev, "error regulator id %d", id);
		return -EINVAL;
	}

	dregu = debug_regulator_get(id);
	if (IS_ERR_OR_NULL(dregu)) {
		ret = PTR_ERR(dregu);
		dev_err(pdata->dev, "get regulator %d %s(%s) fail, ret:%d",
				id,
				act88760_reg_name[id],
				v4pmic->regu_name[id],
				ret);
		return ret;
	}
	regu = dregu->reg;
	/* 0 for set, other for get*/
	if (ops) {
		*en = act88760_regulator_is_enable(dregu->rdev);
		*v = regulator_get_voltage(regu);
		return ret;
	}

	if (*en == 0 && act88760_regulator_is_enable(dregu->rdev)) {
		ret = act88760_regulator_disable(dregu->rdev);
		if (ret)
			dev_err(pdata->dev, "disbale regulator %d %s(%s) fail, ret:%d",
					    id,
					    rdev_name(dregu->rdev),
					    v4pmic->regu_name[id],
					    ret);
	} else if (*en == 1 && !act88760_regulator_is_enable(dregu->rdev)) {
		ret = act88760_regulator_enable(dregu->rdev);
		if (ret)
			dev_err(pdata->dev, "enbale regulator %d %s(%s) fail, ret:%d",
					    id,
					    rdev_name(dregu->rdev),
					    v4pmic->regu_name[id],
					    ret);
	} else if (*en == 3 && act88760_regulator_is_enable(dregu->rdev)) {
		ret = regulator_force_disable(regu);
		if (ret)
			dev_err(pdata->dev, "force disbale regulator %d %s(%s) fail, ret:%d",
					    id,
					    rdev_name(dregu->rdev),
					    v4pmic->regu_name[id],
					    ret);
	}
	if (*v != 0) {
		ret = regulator_set_voltage(regu, *v, *v);
		if (ret)
			dev_err(pdata->dev, "set regulator  %d %s(%s) voltage %d fail, ret:%d",
					id,
					rdev_name(dregu->rdev),
					v4pmic->regu_name[id],
					*v, ret);
	}

	return ret;
}
#else
static int ispv4_pmic_regulator(struct ispv4_pmic_data *pdata, uint32_t ops,
				uint32_t id, uint32_t *en, uint32_t *v)
{
	return -ENODEV;
}
#endif

static int ispv4_comp_bind(struct device *comp, struct device *master,
			   void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	struct ispv4_pmic_data *pdata = NULL;

	pdata = container_of(comp, struct ispv4_pmic_data, comp_dev);
	priv->v4l2_pmic.data = pdata;
	priv->v4l2_pmic.config = ispv4_pmic_config;
	priv->v4l2_pmic.regulator = ispv4_pmic_regulator;
	priv->v4l2_pmic.avalid = true;

	dev_info(comp, "avalid!!\n");
	return 0;
}

static void ispv4_comp_unbind(struct device *comp, struct device *master,
			      void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	priv->v4l2_pmic.data = NULL;
	priv->v4l2_pmic.config = NULL;
	priv->v4l2_pmic.regulator = NULL;
	priv->v4l2_pmic.avalid = false;
}

const struct component_ops comp_ops = {
	.bind = ispv4_comp_bind,
	.unbind = ispv4_comp_unbind,
};

static int pmic_set(void *data, u64 val)
{
	return ispv4_pmic_config(data, val);
}
DEFINE_DEBUGFS_ATTRIBUTE(pmic_dbg, NULL, pmic_set, "%llu/n");

int reg_get_show(struct seq_file *m, void *data)
{
	u32 val = 0;
	int ret = 0;
	struct ispv4_pmic_data *pmic = m->private;

	pmic->client->addr = slave_addr;

	ret = regmap_read(pmic->regm, debug_reg, &val);
	if (ret == 0) {
		seq_printf(m, "get reg 0x%02x = 0x%02x\n", debug_reg, val);
	} else {
		seq_printf(m, "get reg 0x%02x error(%d)\n", debug_reg, ret);
	}

	return ret;
}
DEFINE_SHOW_ATTRIBUTE(reg_get);

static int reg_set(void *data, u64 v)
{
	int ret = 0;
	u8 val = v;
	struct ispv4_pmic_data *pmic = data;

	pmic->client->addr = slave_addr;

	ret = regmap_write(pmic->regm, debug_reg, val);
	if (ret == 0) {
		dev_info(pmic->dev, "set reg 0x%02x = 0x%02x\n", debug_reg, val);
	} else {
		dev_err(pmic->dev, "set reg 0x%02x = 0x%02x error(%d)\n",
				    debug_reg, val, ret);
	}

	return ret;
}
DEFINE_DEBUGFS_ATTRIBUTE(reg_set_fops, NULL, reg_set, "%hhx/n");

#ifdef PMIC_REGU
int regulator_get_show(struct seq_file *m, void *data)
{
	int ret = 0;
	struct ispv4_pmic_data *pmic = m->private;
	struct regulator *regu;
	struct debug_regu *dregu;

	if (debug_regulator_id < 0 || debug_regulator_id >= ACT88760_ID_MAX) {
		dev_err(pmic->dev, "err regulator id %d\n", debug_regulator_id);
		return -EINVAL;
	}

	dregu = debug_regulator_get(debug_regulator_id);
	if (IS_ERR_OR_NULL(dregu)) {
		ret = PTR_ERR(dregu);
		seq_printf(m, "get regulator %d %s(%s) fail, ret:%d",
			       debug_regulator_id,
			       act88760_reg_name[debug_regulator_id],
			       v4pmic->regu_name[debug_regulator_id],
			       ret);
		return ret;
	}
	regu = dregu->reg;

	seq_printf(m, "regulator name = %s(%s)\n", rdev_name(dregu->rdev),
						   v4pmic->regu_name[debug_regulator_id]);
	seq_printf(m, "regulator alwayson = %d\n", regu->always_on);
	seq_printf(m, "regulator enable = %d\n", act88760_regulator_is_enable(dregu->rdev));
	seq_printf(m, "regulator voltage = %d\n", regulator_get_voltage(regu));

	return ret;
}
DEFINE_SHOW_ATTRIBUTE(regulator_get);

static int regulator_en(void *data, u64 v)
{
	int ret = 0;
	struct ispv4_pmic_data *pmic = data;
	struct regulator *regu;
	struct debug_regu *dregu;

	if (debug_regulator_id < 0 || debug_regulator_id >= ACT88760_ID_MAX) {
		dev_err(pmic->dev, "err regulator id %d\n", debug_regulator_id);
		return -EINVAL;
	}

	dregu = debug_regulator_get(debug_regulator_id);
	if (IS_ERR_OR_NULL(dregu)) {
		ret = PTR_ERR(dregu);
		dev_err(pmic->dev, "get regulator %d %s(%s) fail, ret:%d",
				    debug_regulator_id,
				    act88760_reg_name[debug_regulator_id],
				    v4pmic->regu_name[debug_regulator_id],
				    ret);
		return ret;
	}
	regu = dregu->reg;
	if (v == 0 && act88760_regulator_is_enable(dregu->rdev)) {
		ret = act88760_regulator_disable(dregu->rdev);
		if (ret)
			dev_err(pmic->dev, "disable regulator %d %s(%s) fail, ret:%d",
					   debug_regulator_id,
					   rdev_name(dregu->rdev),
					   v4pmic->regu_name[debug_regulator_id],
					   ret);
	} else if (v == 1 && !act88760_regulator_is_enable(dregu->rdev)) {
		ret = act88760_regulator_enable(dregu->rdev);
		if (ret)
			dev_err(pmic->dev, "enable regulator %d %s(%s) fail, ret:%d",
					   debug_regulator_id,
					   rdev_name(dregu->rdev),
					   v4pmic->regu_name[debug_regulator_id],
					   ret);
	} else if (v == 3 && act88760_regulator_is_enable(dregu->rdev)) {
		ret = regulator_force_disable(regu);
		if (ret)
			dev_err(pmic->dev, "force regulator %d %s(%s) fail, ret:%d",
					   debug_regulator_id,
					   rdev_name(dregu->rdev),
					   v4pmic->regu_name[debug_regulator_id],
					   ret);
	}

	return ret;
}
DEFINE_DEBUGFS_ATTRIBUTE(regulator_en_fops, NULL, regulator_en, "%hhx/n");

static int regulator_set_v(void *data, u64 v)
{
	int ret = 0;
	struct ispv4_pmic_data *pmic = data;
	struct regulator *regu;
	struct debug_regu *dregu;

	if (debug_regulator_id < 0 || debug_regulator_id >= ACT88760_ID_MAX) {
		dev_err(pmic->dev, "err regulator id %d\n", debug_regulator_id);
		return -EINVAL;
	}

	dregu = debug_regulator_get(debug_regulator_id);
	if (IS_ERR_OR_NULL(dregu)) {
		ret = PTR_ERR(dregu);
		dev_err(pmic->dev, "get regulator %d %s(%s) fail, ret:%d",
				   debug_regulator_id,
				   act88760_reg_name[debug_regulator_id],
				   v4pmic->regu_name[debug_regulator_id],
				   ret);
		return ret;
	}
	regu = dregu->reg;
	ret = regulator_set_voltage(regu, v, v);
	if (ret)
		dev_err(pmic->dev, "set regulator  %d %s(%s) voltage %d fail, ret:%d",
				   debug_regulator_id,
				   rdev_name(dregu->rdev),
				   v4pmic->regu_name[debug_regulator_id],
				   v, ret);

	return ret;
}
DEFINE_DEBUGFS_ATTRIBUTE(regulator_voltage_fops, NULL, regulator_set_v, "%hhx/n");
#endif

static int ispv4_pmic_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int ret;
	unsigned int val;
#ifdef PMIC_REGU
	int i;
	struct device_node *np, *parent;
	struct regulator_dev *rdev;
	struct regulator_config reg_config;
#endif

	if (IS_ERR_OR_NULL(ispv4_debugfs)) {
		ret = PTR_ERR(ispv4_debugfs);
		pr_err("ispv4_debugfs dir is NULL, probe defer!\n");
		return -EPROBE_DEFER;
	}

	v4pmic = devm_kzalloc(&client->dev, sizeof(struct ispv4_pmic_data),
			      GFP_KERNEL);
	if (!v4pmic)
		return -ENOMEM;

	v4pmic->dev = &client->dev;
	v4pmic->client = client;
	i2c_set_clientdata(client, v4pmic);

	v4pmic->regm = devm_regmap_init_i2c(client, &ispv4_pmic_regmap_cfg);

	/*walkaround pmic issue*/
	ret = regmap_read(v4pmic->regm, 0x0, &val);
	if (ret) {
		dev_err(v4pmic->dev, "regmap_read fail. ret:%d\n", ret);
	}


#ifdef PMIC_REGU
	INIT_LIST_HEAD(&v4pmic->debug_regu);
	np = of_node_get(v4pmic->dev->of_node);
	if (!np)
		return -EINVAL;
	parent = of_get_child_by_name(np, "regulators");
	if (!parent) {
		dev_err(v4pmic->dev, "regulators node not found\n");
		return -EINVAL;
	}

	ret = of_regulator_match(v4pmic->dev, parent, act88760_matches,
				ARRAY_SIZE(act88760_matches));
	of_node_put(parent);
	if (ret != ARRAY_SIZE(act88760_matches)) {
		dev_err(v4pmic->dev, "parse regulator init data fail. ret:%d\n", ret);
		return ret;
	}

	i =  of_property_count_strings(np, "regulator-names");
	if (i != ACT88760_ID_MAX) {
		dev_err(v4pmic->dev, "get supply num(%d) fail", i);
		return -EINVAL;
	}

	for (i = 0; i < ACT88760_ID_MAX; i++) {
		ret = of_property_read_string_index(np,
			"regulator-names", i, &v4pmic->regu_name[i]);
		dev_info(v4pmic->dev, "rgltr_name[%d] = %s\n",
			i, v4pmic->regu_name[i]);
		if (ret) {
			dev_err(v4pmic->dev, "no regulator resource at cnt=%d\n",
					     i);
			return -ENODEV;
		}
	}

	for (i = 0; i < ACT88760_ID_MAX; i++) {
		reg_config.init_data = act88760_matches[i].init_data;
		reg_config.dev = v4pmic->dev;
		reg_config.of_node = act88760_matches[i].of_node;
		reg_config.ena_gpiod = NULL;
		reg_config.regmap = v4pmic->regm;

		rdev = devm_regulator_register(v4pmic->dev,
					&act88760_reg_desc[i], &reg_config);
		if (IS_ERR_OR_NULL(rdev)) {
			ret = PTR_ERR(rdev);
			dev_err(v4pmic->dev, "register regulator fail, ret:%d",
					      ret);
			return ret;
		}
		devm_regulator_debug_register(&rdev->dev, rdev);
		if (rdev->constraints)
			dev_err(v4pmic->dev, "%s  max %duV min %duV",
					      act88760_reg_desc[i].name,
					      rdev->constraints->max_uV,
					      rdev->constraints->min_uV);
	}

#endif

	device_initialize(&v4pmic->comp_dev);
	dev_set_name(&v4pmic->comp_dev, "ispv4-pmic");
	pr_err("comp add %s! priv = %x, comp_name = %s\n", __FUNCTION__, v4pmic,
	       dev_name(&v4pmic->comp_dev));
	ret = component_add(&v4pmic->comp_dev, &comp_ops);
	if (ret != 0) {
		dev_err(v4pmic->dev, "register pmic component failed. ret:%d\n", ret);
		return ret;
	}

	v4pmic->dbg = debugfs_create_dir("ispv4_pmic",ispv4_debugfs);
	if (IS_ERR_OR_NULL(v4pmic->dbg)) {
		ret = PTR_ERR(v4pmic->dbg);
		dev_err(v4pmic->dev, "register pmic debugfs failed. ret:%d\n", ret);
		return ret;
	}
	debugfs_create_file("pmic_cofig", 0222, v4pmic->dbg,
					  v4pmic, &pmic_dbg);

	v4pmic->iic_reg = debugfs_create_dir("iic_reg", v4pmic->dbg);
	if (IS_ERR_OR_NULL(v4pmic->iic_reg)) {
		ret = PTR_ERR(v4pmic->iic_reg);
		dev_err(v4pmic->dev, "register iic_reg debugfs failed. ret:%d\n", ret);
		return ret;
	}
	debugfs_create_u8("slave_addr", 0666, v4pmic->iic_reg, &slave_addr);
	debugfs_create_u8("reg", 0666, v4pmic->iic_reg, &debug_reg);
	debugfs_create_file("get", 0444, v4pmic->iic_reg, v4pmic, &reg_get_fops);
	debugfs_create_file("set", 0222, v4pmic->iic_reg, v4pmic, &reg_set_fops);

#ifdef PMIC_REGU
	v4pmic->regulator = debugfs_create_dir("regulator", v4pmic->dbg);
	if (IS_ERR_OR_NULL(v4pmic->regulator)) {
		ret = PTR_ERR(v4pmic->regulator);
		dev_err(v4pmic->dev, "register iic_reg debugfs failed. ret:%d\n", ret);
		return ret;
	}
	debugfs_create_u32("regulator_id", 0666, v4pmic->regulator, &debug_regulator_id);
	debugfs_create_file("status", 0444, v4pmic->regulator, v4pmic, &regulator_get_fops);
	debugfs_create_file("enable", 0222, v4pmic->regulator, v4pmic, &regulator_en_fops);
	debugfs_create_file("voltage", 0222, v4pmic->regulator, v4pmic, &regulator_voltage_fops);
#endif

	return 0;
};

static void ispv4_pmic_remove(struct i2c_client *client)
{
	struct ispv4_pmic_data *v4pmic = i2c_get_clientdata(client);
	struct debug_regu *dregu, *tmp;
	list_for_each_entry_safe (dregu, tmp, &v4pmic->debug_regu, list) {
			regulator_put(dregu->reg);
			list_del(&dregu->list);
			kfree(dregu);
	}
	debugfs_remove(v4pmic->dbg);
	component_del(&v4pmic->comp_dev, &comp_ops);
}

static const struct of_device_id ispv4_pmic_id[] = {
	[0] = { .compatible = "xm-ispv4-pmic" },
	[1] = {},
};

static struct i2c_driver ispv4_pmic_driver = {
	.driver = {
		.name = "ispv4-pmic",
		.of_match_table = ispv4_pmic_id,
	},
	.probe  = ispv4_pmic_probe,
	.remove = ispv4_pmic_remove,
};
module_i2c_driver(ispv4_pmic_driver);

MODULE_AUTHOR("chenhonglin <chenhonglin@xiaomi.com>");
MODULE_DESCRIPTION("ISPV4 pmic");
MODULE_LICENSE("GPL v2");
