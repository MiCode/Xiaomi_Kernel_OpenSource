/*
 * as3720-regulator.c - voltage regulator support for AS3720
 *
 * Copyright (C) 2012 ams
 *
 * Author: Bernhard Breinbauer <bernhard.breinbauer@ams.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/bug.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/regulator/driver.h>
#include <linux/export.h>
#include <linux/module.h>
#include <linux/mfd/as3720.h>

struct as3720_register_mapping {
	u8 reg_id;
	u8 reg_vsel;
	u32 reg_enable;
	u8 enable_bit;
};

struct as3720_register_mapping as3720_reg_lookup[] = {
	{
		.reg_id = AS3720_LDO0,
		.reg_vsel = AS3720_LDO0_VOLTAGE_REG,
		.reg_enable = AS3720_LDOCONTROL0_REG,
		.enable_bit = AS3720_LDO0_ON,
	},
	{
		.reg_id = AS3720_LDO1,
		.reg_vsel = AS3720_LDO1_VOLTAGE_REG,
		.reg_enable = AS3720_LDOCONTROL0_REG,
		.enable_bit = AS3720_LDO1_ON,
	},
	{
		.reg_id = AS3720_LDO2,
		.reg_vsel = AS3720_LDO2_VOLTAGE_REG,
		.reg_enable = AS3720_LDOCONTROL0_REG,
		.enable_bit = AS3720_LDO2_ON,
	},
	{
		.reg_id = AS3720_LDO3,
		.reg_vsel = AS3720_LDO3_VOLTAGE_REG,
		.reg_enable = AS3720_LDOCONTROL0_REG,
		.enable_bit = AS3720_LDO3_ON,
	},
	{
		.reg_id = AS3720_LDO4,
		.reg_vsel = AS3720_LDO4_VOLTAGE_REG,
		.reg_enable = AS3720_LDOCONTROL0_REG,
		.enable_bit = AS3720_LDO4_ON,
	},
	{
		.reg_id = AS3720_LDO5,
		.reg_vsel = AS3720_LDO5_VOLTAGE_REG,
		.reg_enable = AS3720_LDOCONTROL0_REG,
		.enable_bit = AS3720_LDO5_ON,
	},
	{
		.reg_id = AS3720_LDO6,
		.reg_vsel = AS3720_LDO6_VOLTAGE_REG,
		.reg_enable = AS3720_LDOCONTROL0_REG,
		.enable_bit = AS3720_LDO6_ON,
	},
	{
		.reg_id = AS3720_LDO7,
		.reg_vsel = AS3720_LDO7_VOLTAGE_REG,
		.reg_enable = AS3720_LDOCONTROL0_REG,
		.enable_bit = AS3720_LDO7_ON,
	},
	{
		.reg_id = AS3720_LDO8,
		.reg_vsel = AS3720_LDO8_VOLTAGE_REG,
		.reg_enable = AS3720_LDOCONTROL1_REG,
		.enable_bit = AS3720_LDO8_ON,
	},
	{
		.reg_id = AS3720_LDO9,
		.reg_vsel = AS3720_LDO9_VOLTAGE_REG,
		.reg_enable = AS3720_LDOCONTROL1_REG,
		.enable_bit = AS3720_LDO9_ON,
	},
	{
		.reg_id = AS3720_LDO10,
		.reg_vsel = AS3720_LDO10_VOLTAGE_REG,
		.reg_enable = AS3720_LDOCONTROL1_REG,
		.enable_bit = AS3720_LDO10_ON,
	},
	{
		.reg_id = AS3720_LDO11,
		.reg_vsel = AS3720_LDO11_VOLTAGE_REG,
		.reg_enable = AS3720_LDOCONTROL1_REG,
		.enable_bit = AS3720_LDO11_ON,
	},
	{
		.reg_id = AS3720_SD0,
		.reg_vsel = AS3720_SD0_VOLTAGE_REG,
		.reg_enable = AS3720_SD_CONTROL_REG,
		.enable_bit = AS3720_SD0_ON,
	},
	{
		.reg_id = AS3720_SD1,
		.reg_vsel = AS3720_SD1_VOLTAGE_REG,
		.reg_enable = AS3720_SD_CONTROL_REG,
		.enable_bit = AS3720_SD1_ON,
	},
	{
		.reg_id = AS3720_SD2,
		.reg_vsel = AS3720_SD2_VOLTAGE_REG,
		.reg_enable = AS3720_SD_CONTROL_REG,
		.enable_bit = AS3720_SD2_ON,
	},
	{
		.reg_id = AS3720_SD3,
		.reg_vsel = AS3720_SD3_VOLTAGE_REG,
		.reg_enable = AS3720_SD_CONTROL_REG,
		.enable_bit = AS3720_SD3_ON,
	},
	{
		.reg_id = AS3720_SD4,
		.reg_vsel = AS3720_SD4_VOLTAGE_REG,
		.reg_enable = AS3720_SD_CONTROL_REG,
		.enable_bit = AS3720_SD4_ON,
	},
	{
		.reg_id = AS3720_SD5,
		.reg_vsel = AS3720_SD5_VOLTAGE_REG,
		.reg_enable = AS3720_SD_CONTROL_REG,
		.enable_bit = AS3720_SD5_ON,
	},
	{
		.reg_id = AS3720_SD6,
		.reg_vsel = AS3720_SD6_VOLTAGE_REG,
		.reg_enable = AS3720_SD_CONTROL_REG,
		.enable_bit = AS3720_SD6_ON,
	},
};

/*
 * as3720 ldo0 extended input range (0.825-1.25V)
 */
static int as3720_ldo0_is_enabled(struct regulator_dev *dev)
{
	u32 val;
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	as3720_reg_read(as3720, AS3720_LDOCONTROL0_REG, &val);
	return (val & AS3720_LDO0_CTRL_MASK) != 0;
}

static int as3720_ldo0_enable(struct regulator_dev *dev)
{
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	return as3720_set_bits(as3720,
			       as3720_reg_lookup[rdev_get_id(dev)].reg_enable,
			       AS3720_LDO0_CTRL_MASK, AS3720_LDO0_ON);
}

static int as3720_ldo0_disable(struct regulator_dev *dev)
{
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	return as3720_set_bits(as3720,
			       as3720_reg_lookup[rdev_get_id(dev)].reg_enable,
			       AS3720_LDO0_CTRL_MASK, 0);
}

static int as3720_ldo0_list_voltage(struct regulator_dev *dev,
				       unsigned selector)
{
	if (selector >= AS3720_LDO0_VSEL_MAX)
		return -EINVAL;

	return 800000 + (selector + 1) * 25000;
}

static int as3720_ldo0_get_voltage(struct regulator_dev *dev)
{
	u32 val;
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	as3720_reg_read(as3720,
		as3720_reg_lookup[rdev_get_id(dev)].reg_vsel, &val);
	val &= AS3720_LDO_VSEL_MASK;
	if (val > 0)
		val--;	/* ldo vsel has min value of 1, selector starts at 0 */

	return as3720_ldo0_list_voltage(dev, val);
}

static int as3720_ldo0_set_voltage(struct regulator_dev *dev,
				      int min_uV, int max_uV)
{
	u8 reg_val;
	int val;
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	if (min_uV > 1250000 || max_uV < 825000)
		return -EINVAL;

	/* 25mV steps from 0.825V-1.25V */
	val = (min_uV - 800001) / 25000 + 1;
	if (val < 1)
		val = 1;

	reg_val = (u8) val;
	if (reg_val * 25000 + 800000 > max_uV)
		return -EINVAL;

	BUG_ON(reg_val * 25000 + 800000 < min_uV);
	BUG_ON(reg_val > AS3720_LDO0_VSEL_MAX);

	return as3720_set_bits(as3720,
			       as3720_reg_lookup[rdev_get_id(dev)].reg_vsel,
			       AS3720_LDO_VSEL_MASK, reg_val);
}

static int as3720_ldo0_get_current_limit(struct regulator_dev *dev)
{
	u32 val;
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	as3720_reg_read(as3720, as3720_reg_lookup[rdev_get_id(dev)].reg_vsel,
			&val);
	val &= AS3720_LDO_ILIMIT_MASK;

	/* return ldo specific values */
	if (val)
		return 300000;

	return 150000;
}

static int as3720_ldo0_set_current_limit(struct regulator_dev *dev,
					    int min_uA, int max_uA)
{
	u8 val;
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	/* we check the values in case the constraints are wrong */
	if (min_uA <= 150000 && max_uA >= 150000)
		val = 0;
	else if (min_uA > 150000 && max_uA >= 300000)
		val = AS3720_LDO_ILIMIT_BIT;
	else
		return -EINVAL;

	return as3720_set_bits(as3720,
			       as3720_reg_lookup[rdev_get_id(dev)].reg_vsel,
			       AS3720_LDO_ILIMIT_MASK, val);
}

static struct regulator_ops as3720_ldo0_ops = {
	.is_enabled = as3720_ldo0_is_enabled,
	.enable = as3720_ldo0_enable,
	.disable = as3720_ldo0_disable,
	.list_voltage = as3720_ldo0_list_voltage,
	.get_voltage = as3720_ldo0_get_voltage,
	.set_voltage = as3720_ldo0_set_voltage,
	.get_current_limit = as3720_ldo0_get_current_limit,
	.set_current_limit = as3720_ldo0_set_current_limit,
};

/*
 * as3720 ldo3 low output range (0.61V-1.5V)
 */
static int as3720_ldo3_is_enabled(struct regulator_dev *dev)
{
	u32 val = 0;
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	as3720_reg_read(as3720, as3720_reg_lookup[rdev_get_id(dev)].reg_enable,
			&val);
	return (val & AS3720_LDO3_CTRL_MASK) != 0;
}

static int as3720_ldo3_enable(struct regulator_dev *dev)
{
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	return as3720_set_bits(as3720,
			       as3720_reg_lookup[rdev_get_id(dev)].reg_enable,
			       AS3720_LDO3_CTRL_MASK, AS3720_LDO3_ON);
}

static int as3720_ldo3_disable(struct regulator_dev *dev)
{
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	return as3720_set_bits(as3720,
			       as3720_reg_lookup[rdev_get_id(dev)].reg_enable,
			       AS3720_LDO3_CTRL_MASK, 0);
}

static int as3720_ldo3_list_voltage(struct regulator_dev *dev,
				       unsigned selector)
{
	if (selector >= AS3720_LDO3_VSEL_MAX)
		return -EINVAL;

	return 600000 + (selector + 1) * 10000;
}

static int as3720_ldo3_get_voltage(struct regulator_dev *dev)
{
	u32 val;
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	as3720_reg_read(as3720,
		as3720_reg_lookup[rdev_get_id(dev)].reg_vsel, &val);
	val &= AS3720_LDO3_VSEL_MASK;
	if (val > 0)
		val--;	/* ldo vsel has min value 1, selector starts at 0 */

	return as3720_ldo3_list_voltage(dev, val);
}

static int as3720_ldo3_set_voltage(struct regulator_dev *dev,
				      int min_uV, int max_uV)
{
	u8 reg_val;
	int val;
	struct as3720 *as3720 = rdev_get_drvdata(dev);


	if (min_uV > 1800000 || max_uV < 610000)
		return -EINVAL;

	/* 10mV steps from 0.61V to 1.8V */
	val = (min_uV - 600001) / 10000 + 1;
	if (val < 1)
		val = 1;

	reg_val = (u8) val;
	if (reg_val * 10000 + 600000 > max_uV)
		return -EINVAL;

	BUG_ON(reg_val * 10000 + 600000 < min_uV);
	BUG_ON(reg_val > AS3720_LDO3_VSEL_MAX);

	return as3720_set_bits(as3720,
			       as3720_reg_lookup[rdev_get_id(dev)].reg_vsel,
			       AS3720_LDO3_VSEL_MASK, reg_val);
}

static int as3720_ldo3_get_current_limit(struct regulator_dev *dev)
{
	return 150000;
}

static struct regulator_ops as3720_ldo3_ops = {
	.is_enabled = as3720_ldo3_is_enabled,
	.enable = as3720_ldo3_enable,
	.disable = as3720_ldo3_disable,
	.list_voltage = as3720_ldo3_list_voltage,
	.get_voltage = as3720_ldo3_get_voltage,
	.set_voltage = as3720_ldo3_set_voltage,
	.get_current_limit = as3720_ldo3_get_current_limit,
};

/*
 * as3720 ldo 1-2 and 4-11 (0.8V-3.3V)
 */
static int as3720_ldo_is_enabled(struct regulator_dev *dev)
{
	u32 val = 0;
	int id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	as3720_reg_read(as3720, as3720_reg_lookup[id].reg_enable, &val);
	return (val & as3720_reg_lookup[id].enable_bit) != 0;
}

static int as3720_ldo_enable(struct regulator_dev *dev)
{
	int id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	return as3720_set_bits(as3720, as3720_reg_lookup[id].reg_enable,
			as3720_reg_lookup[id].enable_bit,
			as3720_reg_lookup[id].enable_bit);
}

static int as3720_ldo_disable(struct regulator_dev *dev)
{
	int id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	return as3720_set_bits(as3720, as3720_reg_lookup[id].reg_enable,
			as3720_reg_lookup[id].enable_bit, 0);
}

static int as3720_ldo_list_voltage(struct regulator_dev *dev,
				       unsigned selector)
{
	if (selector >= AS3720_LDO_NUM_VOLT)
		return -EINVAL;

	selector++;	/* ldo vsel min value is 1, selector starts at 0. */
	return 800000 + selector * 25000;
}

static int as3720_ldo_get_voltage(struct regulator_dev *dev)
{
	u32 val;
	int id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	as3720_reg_read(as3720, as3720_reg_lookup[id].reg_vsel, &val);
	val &= AS3720_LDO_VSEL_MASK;
	/* ldo vsel has a gap from 0x25 to 0x3F (27 values). */
	if (val > AS3720_LDO_VSEL_DNU_MAX)
		val -= 27;
	/* ldo vsel min value is 1, selector starts at 0. */
	if (val > 0)
		val--;

	return as3720_ldo_list_voltage(dev, val);
}

static int as3720_ldo_set_voltage(struct regulator_dev *dev,
				      int min_uV, int max_uV)
{
	u8 reg_val;
	int val;
	int id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);


	if (min_uV > 3300000 || max_uV < 825000)
		return -EINVAL;

	if (min_uV <= 1700000) {
		/* 25mV steps from 0.825V to 1.7V */
		val = (min_uV - 800001) / 25000 + 1;
		if (val < 1)
			val = 1;
		reg_val = (u8) val;
		if (reg_val * 25000 + 800000 > max_uV)
			return -EINVAL;
		BUG_ON(reg_val * 25000 + 800000 < min_uV);
	} else {
		/* 25mV steps from 1.725V to 3.3V */
		reg_val = (min_uV - 1700001) / 25000 + 0x40;
		if ((reg_val - 0x40) * 25000 + 1725000 > max_uV)
			return -EINVAL;
		BUG_ON((reg_val - 0x40) * 25000 + 1725000 < min_uV);
	}

	BUG_ON(reg_val > AS3720_LDO_VSEL_MAX);

	return as3720_set_bits(as3720, as3720_reg_lookup[id].reg_vsel,
			       AS3720_LDO_VSEL_MASK, reg_val);
}

static int as3720_ldo_get_current_limit(struct regulator_dev *dev)
{
	u32 val;
	int id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	as3720_reg_read(as3720, as3720_reg_lookup[id].reg_vsel, &val);
	val &= AS3720_LDO_ILIMIT_MASK;

	/* return ldo specific values */
	if (val)
		return 300000;

	/* ldo2,4,5 have 150mA and ldo6-11 have 160mA current limit */
	if (id > AS3720_LDO5)
		return 160000;
	return 150000;
}

static int as3720_ldo_set_current_limit(struct regulator_dev *dev,
					    int min_uA, int max_uA)
{
	u8 val;
	int loweruA = 150000;
	int id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	/* ldo6-11 have 160mA current limit */
	if (id > AS3720_LDO5)
		loweruA = 160000;
	/* we check the values in case the constraints are wrong */
	if (min_uA <= loweruA && max_uA >= loweruA)
		val = 0;
	else if (min_uA > loweruA && max_uA >= 300000)
		val = AS3720_LDO_ILIMIT_BIT;
	else
		return -EINVAL;

	return as3720_set_bits(as3720, as3720_reg_lookup[id].reg_vsel,
			       AS3720_LDO_ILIMIT_MASK, val);
}

static struct regulator_ops as3720_ldo_ops = {
	.is_enabled = as3720_ldo_is_enabled,
	.enable = as3720_ldo_enable,
	.disable = as3720_ldo_disable,
	.list_voltage = as3720_ldo_list_voltage,
	.get_voltage = as3720_ldo_get_voltage,
	.set_voltage = as3720_ldo_set_voltage,
	.get_current_limit = as3720_ldo_get_current_limit,
	.set_current_limit = as3720_ldo_set_current_limit,
};

/*
 * as3720 step down
 */
static int as3720_sd_is_enabled(struct regulator_dev *dev)
{
	u32 val;
	u8 id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	as3720_reg_read(as3720, as3720_reg_lookup[id].reg_enable, &val);

	return (val & as3720_reg_lookup[id].enable_bit) != 0;
}

static int as3720_sd_enable(struct regulator_dev *dev)
{
	u8 id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	return as3720_set_bits(as3720, as3720_reg_lookup[id].reg_enable,
			as3720_reg_lookup[id].enable_bit,
			as3720_reg_lookup[id].enable_bit);
}

static int as3720_sd_disable(struct regulator_dev *dev)
{
	u8 id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	return as3720_set_bits(as3720, as3720_reg_lookup[id].reg_enable,
			as3720_reg_lookup[id].enable_bit, 0);
}

static unsigned int as3720_sd_get_mode(struct regulator_dev *dev)
{
	u32 val;
	u8 reg_id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	as3720_reg_read(as3720, AS3720_SD_CONTROL_REG, &val);

	switch (rdev_get_id(dev)) {
	case AS3720_SD1:
		if ((val & AS3720_SD1_MODE_MASK) == AS3720_SD1_MODE_FAST)
			return REGULATOR_MODE_FAST;
		else
			return REGULATOR_MODE_NORMAL;
	case AS3720_SD2:
		if ((val & AS3720_SD2_MODE_MASK) == AS3720_SD2_MODE_FAST)
			return REGULATOR_MODE_FAST;
		else
			return REGULATOR_MODE_NORMAL;
	case AS3720_SD3:
		if ((val & AS3720_SD3_MODE_MASK) == AS3720_SD3_MODE_FAST)
			return REGULATOR_MODE_FAST;
		else
			return REGULATOR_MODE_NORMAL;
	default:
		printk(KERN_ERR "%s: regulator id %d invalid.\n", __func__,
		       reg_id);
	}

	return -ERANGE;
}

static int as3720_sd_set_mode(struct regulator_dev *dev, unsigned int mode)
{
	u8 val, mask, reg;
	u8 id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	if (mode != REGULATOR_MODE_FAST && mode != REGULATOR_MODE_NORMAL)
		return -EINVAL;

	switch (id) {
	case AS3720_SD0:
		if (mode == REGULATOR_MODE_FAST)
			val = AS3720_SD0_MODE_FAST;
		else
			val = AS3720_SD0_MODE_NORMAL;

		reg = AS3720_SD0_CONTROL_REG;
		mask = AS3720_SD0_MODE_MASK;
		break;
	case AS3720_SD1:
		if (mode == REGULATOR_MODE_FAST)
			val = AS3720_SD1_MODE_FAST;
		else
			val = AS3720_SD1_MODE_NORMAL;

		reg = AS3720_SD1_CONTROL_REG;
		mask = AS3720_SD1_MODE_MASK;
		break;
	case AS3720_SD2:
		if (mode == REGULATOR_MODE_FAST)
			val = AS3720_SD2_MODE_FAST;
		else
			val = AS3720_SD2_MODE_NORMAL;

		reg = AS3720_SD23_CONTROL_REG;
		mask = AS3720_SD2_MODE_MASK;
		break;
	case AS3720_SD3:
		if (mode == REGULATOR_MODE_FAST)
			val = AS3720_SD3_MODE_FAST;
		else
			val = AS3720_SD3_MODE_NORMAL;

		reg = AS3720_SD23_CONTROL_REG;
		mask = AS3720_SD3_MODE_MASK;
		break;
	case AS3720_SD4:
		if (mode == REGULATOR_MODE_FAST)
			val = AS3720_SD4_MODE_FAST;
		else
			val = AS3720_SD4_MODE_NORMAL;

		reg = AS3720_SD4_CONTROL_REG;
		mask = AS3720_SD4_MODE_MASK;
		break;
	case AS3720_SD5:
		if (mode == REGULATOR_MODE_FAST)
			val = AS3720_SD5_MODE_FAST;
		else
			val = AS3720_SD5_MODE_NORMAL;

		reg = AS3720_SD5_CONTROL_REG;
		mask = AS3720_SD5_MODE_MASK;
		break;
	case AS3720_SD6:
		if (mode == REGULATOR_MODE_FAST)
			val = AS3720_SD6_MODE_FAST;
		else
			val = AS3720_SD6_MODE_NORMAL;

		reg = AS3720_SD6_CONTROL_REG;
		mask = AS3720_SD6_MODE_MASK;
		break;
	default:
		printk(KERN_ERR "%s: regulator id %d invalid.\n", __func__,
		       id);
		return -EINVAL;
	}

	return as3720_set_bits(as3720, reg, mask, val);
}

static int as3720_sd_list_voltage(struct regulator_dev *dev, unsigned selector)
{
	u8 id = rdev_get_id(dev);

	if (id == AS3720_SD0 || id == AS3720_SD1 || id == AS3720_SD6) {
		if (selector >= AS3720_SD0_VSEL_MAX)
			return -EINVAL;

		return 600000 + (selector + 1) * 10000;
	} else {
		if (selector > AS3720_SD2_VSEL_MAX)
			return -EINVAL;

		/* ldo vsel min value is 1, selector starts at 0. */
		selector++;
		if (selector <= 0x40)
			return 600000 + selector * 12500;
		if (selector <= 0x70)
			return 1400000 + (selector - 0x40) * 25000;
		if (selector <= 0x7F)
			return 2600000 + (selector - 0x70) * 50000;

		return -ERANGE;
	}
	return -EINVAL;
}

static int as3720_sd_get_voltage(struct regulator_dev *dev)
{
	u32 val;
	u8 id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	as3720_reg_read(as3720, as3720_reg_lookup[id].reg_vsel, &val);
	val &= AS3720_SD_VSEL_MASK;
	if (val > 0)
		val--;	/* ldo vsel min value is 1, selector starts at 0. */

	return as3720_sd_list_voltage(dev, val);
}

static int as3720_sd_lowpower_set_voltage(struct regulator_dev *dev,
					  int min_uV, int max_uV)
{
	u8 reg_val;
	int val;
	u8 id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	/*       0 ... 0        0x00 : not allowed as voltage setting
	 *  610000 ... 1500000: 0x01 - 0x40, 10mV steps */

	if (min_uV > 1500000 || max_uV < 610000)
		return -EINVAL;

	val = (min_uV - 600001) / 10000 + 1;
	if (val < 1)
		val = 1;

	reg_val = (u8) val;
	if (reg_val * 10000 + 600000 > max_uV)
		return -EINVAL;
	BUG_ON(reg_val * 10000 + 600000 < min_uV);

	return as3720_set_bits(as3720, as3720_reg_lookup[id].reg_vsel,
			       AS3720_SD_VSEL_MASK, reg_val);
}

static int as3720_sd_nom_set_voltage(struct regulator_dev *dev,
				     int min_uV, int max_uV)
{
	u8 reg_val;
	int val;
	u8 id = rdev_get_id(dev);
	struct as3720 *as3720 = rdev_get_drvdata(dev);

	/*       0 ... 0        0x00 : not allowed as voltage setting
	 *  612500 ... 1400000: 0x01 - 0x40, 12.5mV steps
	 * 1425000 ... 2600000: 0x41 - 0x70, 25mV steps
	 * 2650000 ... 3350000: 0x41 - 0x70, 50mV steps */

	if (min_uV > 3350000 || max_uV < 612500)
		return -EINVAL;

	if (min_uV <= 1400000) {
		val = (min_uV - 600001) / 12500 + 1;
		if (val < 1)
			val = 1;

		reg_val = (u8) val;
		if ((reg_val * 12500) + 600000 > max_uV)
			return -EINVAL;

		BUG_ON((reg_val * 12500) + 600000 < min_uV);

	} else if (min_uV <= 2600000) {
		reg_val = (min_uV - 1400001) / 25000 + 1;

		if ((reg_val * 25000) + 1400000 > max_uV)
			return -EINVAL;

		BUG_ON((reg_val * 25000) + 1400000 < min_uV);

		reg_val += 0x40;

	} else {

		reg_val = (min_uV - 2600001) / 50000 + 1;

		if ((reg_val * 50000) + 2600000 > max_uV)
			return -EINVAL;

		BUG_ON((reg_val * 50000) + 2600000 < min_uV);

		reg_val += 0x70;
	}

	return as3720_set_bits(as3720, as3720_reg_lookup[id].reg_vsel,
			       AS3720_SD_VSEL_MASK, reg_val);
}

static int as3720_sd_set_voltage(struct regulator_dev *dev,
				 int min_uV, int max_uV)
{
	u8 id = rdev_get_id(dev);

	if (id == AS3720_SD0 || id == AS3720_SD1 || id == AS3720_SD6)
		return as3720_sd_lowpower_set_voltage(dev, min_uV, max_uV);
	else
		return as3720_sd_nom_set_voltage(dev, min_uV, max_uV);
}

static struct regulator_ops as3720_sd_ops = {
	.is_enabled = as3720_sd_is_enabled,
	.enable = as3720_sd_enable,
	.disable = as3720_sd_disable,
	.list_voltage = as3720_sd_list_voltage,
	.get_voltage = as3720_sd_get_voltage,
	.set_voltage = as3720_sd_set_voltage,
	.get_mode = as3720_sd_get_mode,
	.set_mode = as3720_sd_set_mode,
};

static struct regulator_desc regulators[] = {
	{
	 .name = "as3720-ldo0",
	 .id = AS3720_LDO0,
	 .ops = &as3720_ldo0_ops,
	 .n_voltages = AS3720_LDO0_VSEL_MAX,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-ldo1",
	 .id = AS3720_LDO1,
	 .ops = &as3720_ldo_ops,
	 .n_voltages = AS3720_LDO_NUM_VOLT,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-ldo2",
	 .id = AS3720_LDO2,
	 .ops = &as3720_ldo_ops,
	 .n_voltages = AS3720_LDO_NUM_VOLT,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-ldo3",
	 .id = AS3720_LDO3,
	 .ops = &as3720_ldo3_ops,
	 .n_voltages = AS3720_LDO3_VSEL_MAX,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-ldo4",
	 .id = AS3720_LDO4,
	 .ops = &as3720_ldo_ops,
	 .n_voltages = AS3720_LDO_NUM_VOLT,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-ldo5",
	 .id = AS3720_LDO5,
	 .ops = &as3720_ldo_ops,
	 .n_voltages = AS3720_LDO_NUM_VOLT,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-ldo6",
	 .id = AS3720_LDO6,
	 .ops = &as3720_ldo_ops,
	 .n_voltages = AS3720_LDO_NUM_VOLT,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-ldo7",
	 .id = AS3720_LDO7,
	 .ops = &as3720_ldo_ops,
	 .n_voltages = AS3720_LDO_NUM_VOLT,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-ldo8",
	 .id = AS3720_LDO8,
	 .ops = &as3720_ldo_ops,
	 .n_voltages = AS3720_LDO_NUM_VOLT,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-ldo9",
	 .id = AS3720_LDO9,
	 .ops = &as3720_ldo_ops,
	 .n_voltages = AS3720_LDO_NUM_VOLT,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-ldo10",
	 .id = AS3720_LDO10,
	 .ops = &as3720_ldo_ops,
	 .n_voltages = AS3720_LDO_NUM_VOLT,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-ldo11",
	 .id = AS3720_LDO11,
	 .ops = &as3720_ldo_ops,
	 .n_voltages = AS3720_LDO_NUM_VOLT,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-sd0",
	 .id = AS3720_SD0,
	 .ops = &as3720_sd_ops,
	 .n_voltages = AS3720_SD0_VSEL_MAX,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-sd1",
	 .id = AS3720_SD1,
	 .ops = &as3720_sd_ops,
	 .n_voltages = AS3720_SD0_VSEL_MAX,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-sd2",
	 .id = AS3720_SD2,
	 .ops = &as3720_sd_ops,
	 .n_voltages = AS3720_SD2_VSEL_MAX,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-sd3",
	 .id = AS3720_SD3,
	 .ops = &as3720_sd_ops,
	 .n_voltages = AS3720_SD2_VSEL_MAX,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-sd4",
	 .id = AS3720_SD4,
	 .ops = &as3720_sd_ops,
	 .n_voltages = AS3720_SD2_VSEL_MAX,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-sd5",
	 .id = AS3720_SD5,
	 .ops = &as3720_sd_ops,
	 .n_voltages = AS3720_SD2_VSEL_MAX,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
	{
	 .name = "as3720-sd6",
	 .id = AS3720_SD6,
	 .ops = &as3720_sd_ops,
	 .n_voltages = AS3720_SD0_VSEL_MAX,
	 .type = REGULATOR_VOLTAGE,
	 .owner = THIS_MODULE,
	 },
};

static int as3720_regulator_probe(struct platform_device *pdev)
{
	int regulator;
	struct regulator_dev *rdev;
	struct as3720 *as3720 = dev_get_drvdata(pdev->dev.parent);
	struct as3720_platform_data *pdata;
	struct regulator_config config = { };

	pdata = dev_get_platdata(pdev->dev.parent);

	for (regulator = 0; regulator < AS3720_NUM_REGULATORS; regulator++) {
		if (pdata->reg_init[regulator]) {
			config.dev = &pdev->dev;
			config.init_data = pdata->reg_init[regulator];
			config.driver_data = as3720;

			rdev = regulator_register(&regulators[regulator],
				&config);
			if (IS_ERR(rdev)) {
				dev_err(&pdev->dev, "as3720 regulator err\n");
				return PTR_ERR(rdev);
			}
			as3720->rdevs[regulator] = rdev;
		}
	}
	return 0;
}

static int as3720_regulator_remove(struct platform_device *pdev)
{
	struct as3720 *as3720 = dev_get_drvdata(&pdev->dev);
	int regulator;

	for (regulator = 0; regulator < AS3720_NUM_REGULATORS; regulator++)
		regulator_unregister(as3720->rdevs[regulator]);
	return 0;
}

static struct platform_driver as3720_regulator_driver = {
	.driver = {
		   .name = "as3720-regulator",
		   .owner = THIS_MODULE,
		   },
	.probe = as3720_regulator_probe,
	.remove = as3720_regulator_remove,
};

static int __init as3720_regulator_init(void)
{
	return platform_driver_register(&as3720_regulator_driver);
}

subsys_initcall(as3720_regulator_init);

static void __exit as3720_regulator_exit(void)
{
	platform_driver_unregister(&as3720_regulator_driver);
}

module_exit(as3720_regulator_exit);

MODULE_AUTHOR("Bernhard Breinbauer <bernhard.breinbauer@ams.com>");
MODULE_DESCRIPTION("AS3720 regulator driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:as3720-regulator");

