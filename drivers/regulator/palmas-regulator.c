/*
 * Driver for Regulator part of Palmas PMIC Chips
 *
 * Copyright 2011-2012 Texas Instruments Inc.
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Graeme Gregory <gg@slimlogic.co.uk>
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/mfd/palmas.h>

#define PALMA_SMPS10_VSEL	BIT(3)
#define PALMA_SMPS10_BOOST_EN	BIT(2)
#define PALMA_SMPS10_BYPASS_EN	BIT(1)
#define PALMA_SMPS10_SWITCH_EN	BIT(0)

#define EXT_PWR_REQ (PALMAS_EXT_CONTROL_ENABLE1 |	\
		     PALMAS_EXT_CONTROL_ENABLE2 |	\
		     PALMAS_EXT_CONTROL_NSLEEP)

struct regs_info {
	char	*name;
	u8	vsel_addr;
	u8	ctrl_addr;
	u8	tstep_addr;
	u8	fvsel_addr;
	int	sleep_id;
};

static const struct regs_info palmas_regs_info[] = {
	{
		.name		= "SMPS12",
		.vsel_addr	= PALMAS_SMPS12_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS12_CTRL,
		.tstep_addr	= PALMAS_SMPS12_TSTEP,
		.fvsel_addr	= PALMAS_SMPS12_FORCE,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_SMPS12,
	},
	{
		.name		= "SMPS123",
		.vsel_addr	= PALMAS_SMPS12_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS12_CTRL,
		.tstep_addr	= PALMAS_SMPS12_TSTEP,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_SMPS12,
	},
	{
		.name		= "SMPS3",
		.vsel_addr	= PALMAS_SMPS3_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS3_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_SMPS3,
	},
	{
		.name		= "SMPS45",
		.vsel_addr	= PALMAS_SMPS45_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS45_CTRL,
		.tstep_addr	= PALMAS_SMPS45_TSTEP,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_SMPS45,
	},
	{
		.name		= "SMPS457",
		.vsel_addr	= PALMAS_SMPS45_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS45_CTRL,
		.tstep_addr	= PALMAS_SMPS45_TSTEP,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_SMPS45,
	},
	{
		.name		= "SMPS6",
		.vsel_addr	= PALMAS_SMPS6_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS6_CTRL,
		.tstep_addr	= PALMAS_SMPS6_TSTEP,
		.fvsel_addr	= PALMAS_SMPS6_FORCE,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_SMPS6,
	},
	{
		.name		= "SMPS7",
		.vsel_addr	= PALMAS_SMPS7_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS7_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_SMPS7,
	},
	{
		.name		= "SMPS8",
		.vsel_addr	= PALMAS_SMPS8_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS8_CTRL,
		.tstep_addr	= PALMAS_SMPS8_TSTEP,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_SMPS8,
	},
	{
		.name		= "SMPS9",
		.vsel_addr	= PALMAS_SMPS9_VOLTAGE,
		.ctrl_addr	= PALMAS_SMPS9_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_SMPS9,
	},
	{
		.name		= "SMPS10",
		.ctrl_addr	= PALMAS_SMPS10_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_SMPS10,
	},
	{
		.name		= "LDO1",
		.vsel_addr	= PALMAS_LDO1_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO1_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_LDO1,
	},
	{
		.name		= "LDO2",
		.vsel_addr	= PALMAS_LDO2_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO2_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_LDO2,
	},
	{
		.name		= "LDO3",
		.vsel_addr	= PALMAS_LDO3_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO3_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_LDO3,
	},
	{
		.name		= "LDO4",
		.vsel_addr	= PALMAS_LDO4_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO4_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_LDO4,
	},
	{
		.name		= "LDO5",
		.vsel_addr	= PALMAS_LDO5_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO5_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_LDO5,
	},
	{
		.name		= "LDO6",
		.vsel_addr	= PALMAS_LDO6_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO6_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_LDO6,
	},
	{
		.name		= "LDO7",
		.vsel_addr	= PALMAS_LDO7_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO7_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_LDO7,
	},
	{
		.name		= "LDO8",
		.vsel_addr	= PALMAS_LDO8_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO8_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_LDO8,
	},
	{
		.name		= "LDO9",
		.vsel_addr	= PALMAS_LDO9_VOLTAGE,
		.ctrl_addr	= PALMAS_LDO9_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_LDO9,
	},
	{
		.name		= "LDOLN",
		.vsel_addr	= PALMAS_LDOLN_VOLTAGE,
		.ctrl_addr	= PALMAS_LDOLN_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_LDOLN,
	},
	{
		.name		= "LDOUSB",
		.vsel_addr	= PALMAS_LDOUSB_VOLTAGE,
		.ctrl_addr	= PALMAS_LDOUSB_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_LDOUSB,
	},
	{
		.name		= "REGEN1",
		.ctrl_addr	= PALMAS_REGEN1_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_REGEN1,
	},
	{
		.name		= "REGEN2",
		.ctrl_addr	= PALMAS_REGEN2_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_REGEN2,
	},
	{
		.name		= "REGEN3",
		.ctrl_addr	= PALMAS_REGEN3_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_REGEN3,
	},
	{
		.name		= "SYSEN1",
		.ctrl_addr	= PALMAS_SYSEN1_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_SYSEN1,
	},
	{
		.name		= "SYSEN2",
		.ctrl_addr	= PALMAS_SYSEN2_CTRL,
		.sleep_id	= PALMAS_SLEEP_REQSTR_ID_SYSEN2,
	},
};

static unsigned int palmas_smps_ramp_delay[4] = {0, 10000, 5000, 2500};

#define SMPS_CTRL_MODE_OFF		0x00
#define SMPS_CTRL_MODE_ON		0x01
#define SMPS_CTRL_MODE_ECO		0x02
#define SMPS_CTRL_MODE_PWM		0x03

#define SMPS_CTRL_SLEEP_MODE_OFF	0x00
#define SMPS_CTRL_SLEEP_MODE_ON		0x04
#define SMPS_CTRL_SLEEP_MODE_ECO	0x08
#define SMPS_CTRL_SLEEP_MODE_PWM	0x0C

/* These values are derived from the data sheet. And are the number of steps
 * where there is a voltage change, the ranges at beginning and end of register
 * max/min values where there are no change are ommitted.
 *
 * So they are basically (maxV-minV)/stepV
 */
#define PALMAS_SMPS_NUM_VOLTAGES	117
#define PALMAS_SMPS10_NUM_VOLTAGES	2
#define PALMAS_LDO_NUM_VOLTAGES		50

#define SMPS10_VSEL			(1<<3)
#define SMPS10_BOOST_EN			(1<<2)
#define SMPS10_BYPASS_EN		(1<<1)
#define SMPS10_SWITCH_EN		(1<<0)

#define REGULATOR_SLAVE			0

static void palmas_disable_smps10_boost(struct palmas *palmas);
static void palmas_enable_smps10_boost(struct palmas *palmas);

static int palmas_smps_read(struct palmas *palmas, unsigned int reg,
		unsigned int *dest)
{
	unsigned int addr;

	addr = PALMAS_BASE_TO_REG(PALMAS_SMPS_BASE, reg);

	return regmap_read(palmas->regmap[REGULATOR_SLAVE], addr, dest);
}

static int palmas_smps_write(struct palmas *palmas, unsigned int reg,
		unsigned int value)
{
	unsigned int addr;

	addr = PALMAS_BASE_TO_REG(PALMAS_SMPS_BASE, reg);

	return regmap_write(palmas->regmap[REGULATOR_SLAVE], addr, value);
}

static int palmas_ldo_read(struct palmas *palmas, unsigned int reg,
		unsigned int *dest)
{
	unsigned int addr;

	addr = PALMAS_BASE_TO_REG(PALMAS_LDO_BASE, reg);

	return regmap_read(palmas->regmap[REGULATOR_SLAVE], addr, dest);
}

static int palmas_ldo_write(struct palmas *palmas, unsigned int reg,
		unsigned int value)
{
	unsigned int addr;

	addr = PALMAS_BASE_TO_REG(PALMAS_LDO_BASE, reg);

	return regmap_write(palmas->regmap[REGULATOR_SLAVE], addr, value);
}

static int palmas_resource_read(struct palmas *palmas, unsigned int reg,
		unsigned int *dest)
{
	unsigned int addr;

	addr = PALMAS_BASE_TO_REG(PALMAS_RESOURCE_BASE, reg);

	return regmap_read(palmas->regmap[REGULATOR_SLAVE], addr, dest);
}

static int palmas_resource_write(struct palmas *palmas, unsigned int reg,
		unsigned int value)
{
	unsigned int addr;

	addr = PALMAS_BASE_TO_REG(PALMAS_RESOURCE_BASE, reg);

	return regmap_write(palmas->regmap[REGULATOR_SLAVE], addr, value);
}

static int palmas_is_enabled_smps(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	if (EXT_PWR_REQ & pmic->roof_floor[id])
		return true;

	palmas_smps_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);

	reg &= PALMAS_SMPS12_CTRL_MODE_ACTIVE_MASK;
	reg >>= PALMAS_SMPS12_CTRL_MODE_ACTIVE_SHIFT;

	return !!(reg);
}

static int palmas_enable_smps(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	if (EXT_PWR_REQ & pmic->roof_floor[id])
		return 0;

	palmas_smps_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);

	reg &= ~PALMAS_SMPS12_CTRL_MODE_ACTIVE_MASK;
	if (pmic->current_mode_reg[id])
		reg |= pmic->current_mode_reg[id];
	else
		reg |= SMPS_CTRL_MODE_ON;

	palmas_smps_write(pmic->palmas, palmas_regs_info[id].ctrl_addr, reg);

	return 0;
}

static int palmas_disable_smps(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	if (EXT_PWR_REQ & pmic->roof_floor[id])
		return 0;

	palmas_smps_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);

	reg &= ~PALMAS_SMPS12_CTRL_MODE_ACTIVE_MASK;

	palmas_smps_write(pmic->palmas, palmas_regs_info[id].ctrl_addr, reg);

	return 0;
}

static int palmas_set_mode_smps(struct regulator_dev *dev, unsigned int mode)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;
	int avoid_update = 0;

	palmas_smps_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);
	reg &= ~PALMAS_SMPS12_CTRL_MODE_ACTIVE_MASK;

	if (reg == SMPS_CTRL_MODE_OFF)
		avoid_update = 1;

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		reg |= SMPS_CTRL_MODE_ON;
		break;
	case REGULATOR_MODE_IDLE:
		reg |= SMPS_CTRL_MODE_ECO;
		break;
	case REGULATOR_MODE_FAST:
		reg |= SMPS_CTRL_MODE_PWM;
		break;
	case REGULATOR_MODE_STANDBY:
		reg |= SMPS_CTRL_MODE_OFF;
		break;
	default:
		return -EINVAL;
	}
	pmic->current_mode_reg[id] = reg & PALMAS_SMPS12_CTRL_MODE_ACTIVE_MASK;
	if (!avoid_update)
		palmas_smps_write(pmic->palmas,
			palmas_regs_info[id].ctrl_addr, reg);
	return 0;
}

static unsigned int palmas_get_mode_smps(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	reg = pmic->current_mode_reg[id] & PALMAS_SMPS12_CTRL_MODE_ACTIVE_MASK;

	switch (reg) {
	case SMPS_CTRL_MODE_ON:
		return REGULATOR_MODE_NORMAL;
	case SMPS_CTRL_MODE_ECO:
		return REGULATOR_MODE_IDLE;
	case SMPS_CTRL_MODE_PWM:
		return REGULATOR_MODE_FAST;
	case SMPS_CTRL_MODE_OFF:
		return REGULATOR_MODE_STANDBY;
	}

	return 0;
}

static int palmas_set_sleep_mode_smps(struct regulator_dev *dev,
	unsigned int mode)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	palmas_smps_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);
	reg &= ~PALMAS_SMPS12_CTRL_MODE_SLEEP_MASK;

	switch (mode) {
	case REGULATOR_MODE_NORMAL:
		reg |= SMPS_CTRL_SLEEP_MODE_ON;
		break;
	case REGULATOR_MODE_IDLE:
		reg |= SMPS_CTRL_SLEEP_MODE_ECO;
		break;
	case REGULATOR_MODE_FAST:
		reg |= SMPS_CTRL_SLEEP_MODE_PWM;
		break;
	case REGULATOR_MODE_STANDBY:
	case REGULATOR_MODE_OFF:
		reg |= SMPS_CTRL_SLEEP_MODE_OFF;
		break;
	default:
		return -EINVAL;
	}
	palmas_smps_write(pmic->palmas, palmas_regs_info[id].ctrl_addr, reg);
	return 0;
}

static int palmas_list_voltage_smps(struct regulator_dev *dev,
					unsigned selector)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	int mult = 1;

	if (!selector)
		return 0;

	/* Read the multiplier set in VSEL register to return
	 * the correct voltage.
	 */
	if (pmic->range[id])
		mult = 2;

	/* Voltage is (0.49V + (selector * 0.01V)) * RANGE
	 * as defined in data sheet. RANGE is either x1 or x2
	 */
	return  (490000 + (selector * 10000)) * mult;
}

static int palmas_get_voltage_smps_sel(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	int selector;
	unsigned int reg;
	unsigned int addr;

	addr = palmas_regs_info[id].vsel_addr;

	palmas_smps_read(pmic->palmas, addr, &reg);

	selector = reg & PALMAS_SMPS12_VOLTAGE_VSEL_MASK;

	/* Adjust selector to match list_voltage ranges */
	if ((selector > 0) && (selector < 6))
		selector = 6;
	if (!selector)
		selector = 5;
	if (selector > 121)
		selector = 121;
	selector -= 5;

	return selector;
}

static int palmas_set_voltage_smps_sel(struct regulator_dev *dev,
		unsigned selector)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg = 0;
	unsigned int addr;

	addr = palmas_regs_info[id].vsel_addr;

	/* Make sure we don't change the value of RANGE */
	if (pmic->range[id])
		reg |= PALMAS_SMPS12_VOLTAGE_RANGE;

	/* Adjust the linux selector into range used in VSEL register */
	if (selector)
		reg |= selector + 5;

	palmas_smps_write(pmic->palmas, addr, reg);

	return 0;
}

static int palma_smps_set_voltage_smps_time_sel(struct regulator_dev *rdev,
	unsigned int old_selector, unsigned int new_selector)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	int old_uV, new_uV;
	unsigned int ramp_delay = pmic->ramp_delay[id];

	/* ES2.1, have the 1.5X slower slew rate than configured */
	if (palmas_is_es_version_or_less(pmic->palmas, 2, 1))
		ramp_delay = (ramp_delay * 10)/15;

	if (!ramp_delay)
		return 0;

	old_uV = palmas_list_voltage_smps(rdev, old_selector);
	if (old_uV < 0)
		return old_uV;

	new_uV = palmas_list_voltage_smps(rdev, new_selector);
	if (new_uV < 0)
		return new_uV;

	return DIV_ROUND_UP(abs(old_uV - new_uV), ramp_delay);
}

static int palmas_smps_set_ramp_delay(struct regulator_dev *rdev,
		int ramp_delay)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(rdev);
	int id = rdev_get_id(rdev);
	unsigned int reg = 0;
	unsigned int addr = palmas_regs_info[id].tstep_addr;
	int ret;

	if (ramp_delay <= 0)
		reg = 0;
	else if (ramp_delay <= 2500)
		reg = 3;
	else if (ramp_delay <= 5000)
		reg = 2;
	else
		reg = 1;

	ret = palmas_smps_write(pmic->palmas, addr, reg);
	if (ret < 0) {
		dev_err(pmic->palmas->dev, "TSTEP write failed: %d\n", ret);
		return ret;
	}

	pmic->ramp_delay[id] = palmas_smps_ramp_delay[reg];
	return ret;
}

static struct regulator_ops palmas_ops_smps = {
	.is_enabled		= palmas_is_enabled_smps,
	.enable			= palmas_enable_smps,
	.disable		= palmas_disable_smps,
	.set_mode		= palmas_set_mode_smps,
	.get_mode		= palmas_get_mode_smps,
	.set_sleep_mode		= palmas_set_sleep_mode_smps,
	.get_voltage_sel	= palmas_get_voltage_smps_sel,
	.set_voltage_sel	= palmas_set_voltage_smps_sel,
	.list_voltage		= palmas_list_voltage_smps,
	.set_voltage_time_sel	= palma_smps_set_voltage_smps_time_sel,
	.set_ramp_delay		= palmas_smps_set_ramp_delay,
};

static int palmas_is_enabled_smps10(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;
	int ret;

	if (EXT_PWR_REQ & pmic->roof_floor[id])
		return true;

	ret = palmas_smps_read(pmic->palmas, PALMAS_SMPS10_STATUS, &reg);
	if (ret < 0) {
		dev_err(pmic->palmas->dev,
			"Error in reading smps10 status reg\n");
		return ret;
	}

	if (reg & PALMA_SMPS10_SWITCH_EN)
		return 1;

	return 0;
}

static int palmas_enable_smps10(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;
	int ret;

	if (EXT_PWR_REQ & pmic->roof_floor[id])
		return 0;

	ret = palmas_smps_read(pmic->palmas,
				palmas_regs_info[id].ctrl_addr, &reg);
	if (ret < 0) {
		dev_err(pmic->palmas->dev,
			"Error in reading smps10 control reg\n");
		return ret;
	}

	reg |= PALMA_SMPS10_SWITCH_EN;
	ret = palmas_smps_write(pmic->palmas,
				palmas_regs_info[id].ctrl_addr, reg);
	if (ret < 0)
		dev_err(pmic->palmas->dev,
			"Error in writing smps10 control reg\n");

	palmas_enable_smps10_boost(pmic->palmas);
	pmic->smps10_regulator_enabled = true;
	return ret;
}

static int palmas_disable_smps10(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;
	int ret;

	if (EXT_PWR_REQ & pmic->roof_floor[id])
		return 0;

	ret = palmas_smps_read(pmic->palmas,
				palmas_regs_info[id].ctrl_addr, &reg);
	if (ret < 0) {
		dev_err(pmic->palmas->dev,
			"Error in reading smps10 control reg\n");
		return ret;
	}

	reg &= ~PALMA_SMPS10_SWITCH_EN;
	ret = palmas_smps_write(pmic->palmas,
				palmas_regs_info[id].ctrl_addr, reg);
	if (ret < 0)
		dev_err(pmic->palmas->dev,
			"Error in writing smps10 control reg\n");
	pmic->smps10_regulator_enabled = false;

	if (pmic->smps10_boost_disable_deferred) {
		palmas_disable_smps10_boost(pmic->palmas);
		pmic->smps10_boost_disable_deferred = false;
	}

	return ret;
}

static int palmas_get_voltage_smps10_sel(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;
	int ret;

	ret = palmas_smps_read(pmic->palmas,
				palmas_regs_info[id].ctrl_addr, &reg);
	if (ret < 0) {
		dev_err(pmic->palmas->dev,
			"Error in reading smps10 control reg\n");
		return ret;
	}

	if (reg & PALMA_SMPS10_VSEL)
		return 1;

	return 0;
}

static int palmas_set_voltage_smps10_sel(struct regulator_dev *dev,
		unsigned selector)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;
	int ret;

	ret = palmas_smps_read(pmic->palmas,
				palmas_regs_info[id].ctrl_addr, &reg);
	if (ret < 0) {
		dev_err(pmic->palmas->dev,
			"Error in reading smps10 control reg\n");
		return ret;
	}

	if (selector)
		reg |= PALMA_SMPS10_VSEL;
	else
		reg &= ~PALMA_SMPS10_VSEL;

	/* Enable boost mode */
	reg |= PALMA_SMPS10_BOOST_EN;

	ret = palmas_smps_write(pmic->palmas,
				palmas_regs_info[id].ctrl_addr, reg);
	if (ret < 0)
		dev_err(pmic->palmas->dev,
			"Error in writing smps10 control reg\n");
	return ret;
}

static int palmas_list_voltage_smps10(struct regulator_dev *dev,
					unsigned selector)
{
	return 3750000 + (selector * 1250000);
}

static struct regulator_ops palmas_ops_smps10 = {
	.is_enabled		= palmas_is_enabled_smps10,
	.enable			= palmas_enable_smps10,
	.disable		= palmas_disable_smps10,
	.get_voltage_sel	= palmas_get_voltage_smps10_sel,
	.set_voltage_sel	= palmas_set_voltage_smps10_sel,
	.list_voltage		= palmas_list_voltage_smps10,
};

static int palmas_ldo_enable_time(struct regulator_dev *dev)
{
	return 500;
}

static int palmas_is_enabled_ldo(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	if (EXT_PWR_REQ & pmic->roof_floor[id])
		return true;

	palmas_ldo_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);

	reg &= PALMAS_LDO1_CTRL_STATUS;

	return !!(reg);
}

static int palmas_enable_ldo(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	if (EXT_PWR_REQ & pmic->roof_floor[id])
		return 0;

	palmas_ldo_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);

	reg |= SMPS_CTRL_MODE_ON;

	palmas_ldo_write(pmic->palmas, palmas_regs_info[id].ctrl_addr, reg);

	return 0;
}

static int palmas_disable_ldo(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;

	if (EXT_PWR_REQ & pmic->roof_floor[id])
		return 0;

	palmas_ldo_read(pmic->palmas, palmas_regs_info[id].ctrl_addr, &reg);

	reg &= ~SMPS_CTRL_MODE_ON;

	palmas_ldo_write(pmic->palmas, palmas_regs_info[id].ctrl_addr, reg);

	return 0;
}

static int palmas_list_voltage_ldo(struct regulator_dev *dev,
					unsigned selector)
{
	if (!selector)
		return 0;

	/* voltage is 0.85V + (selector * 0.05v) */
	return  850000 + (selector * 50000);
}

static int palmas_get_voltage_ldo_sel(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	int selector;
	unsigned int reg;
	unsigned int addr;

	addr = palmas_regs_info[id].vsel_addr;

	palmas_ldo_read(pmic->palmas, addr, &reg);

	selector = reg & PALMAS_LDO1_VOLTAGE_VSEL_MASK;

	/* Adjust selector to match list_voltage ranges */
	if (selector > 49)
		selector = 49;

	return selector;
}

static int palmas_set_voltage_ldo_sel(struct regulator_dev *dev,
		unsigned selector)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg = 0;
	unsigned int addr;

	addr = palmas_regs_info[id].vsel_addr;

	reg = selector;

	palmas_ldo_write(pmic->palmas, addr, reg);

	return 0;
}

static struct regulator_ops palmas_ops_ldo = {
	.enable_time		= palmas_ldo_enable_time,
	.is_enabled		= palmas_is_enabled_ldo,
	.enable			= palmas_enable_ldo,
	.disable		= palmas_disable_ldo,
	.get_voltage_sel	= palmas_get_voltage_ldo_sel,
	.set_voltage_sel	= palmas_set_voltage_ldo_sel,
	.list_voltage		= palmas_list_voltage_ldo,
};

static int palmas_is_enabled_extreg(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;
	int ret;

	if (EXT_PWR_REQ & pmic->roof_floor[id])
		return true;

	ret = palmas_resource_read(pmic->palmas,
			palmas_regs_info[id].ctrl_addr, &reg);
	reg &= PALMAS_REGEN1_CTRL_STATUS;
	if (ret < 0)
		return ret;

	return !!(reg);
}

static int palmas_enable_extreg(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;
	int ret;

	if (EXT_PWR_REQ & pmic->roof_floor[id])
		return 0;

	ret = palmas_resource_read(pmic->palmas,
			palmas_regs_info[id].ctrl_addr, &reg);
	if (ret < 0)
		return ret;

	reg |= PALMAS_REGEN1_CTRL_MODE_ACTIVE;
	ret = palmas_resource_write(pmic->palmas,
			palmas_regs_info[id].ctrl_addr, reg);
	return ret;
}

static int palmas_disable_extreg(struct regulator_dev *dev)
{
	struct palmas_pmic *pmic = rdev_get_drvdata(dev);
	int id = rdev_get_id(dev);
	unsigned int reg;
	int ret;

	if (EXT_PWR_REQ & pmic->roof_floor[id])
		return 0;

	ret = palmas_resource_read(pmic->palmas,
			palmas_regs_info[id].ctrl_addr, &reg);
	if (ret < 0)
		return ret;

	reg &= ~PALMAS_REGEN1_CTRL_MODE_ACTIVE;
	ret = palmas_resource_write(pmic->palmas,
			palmas_regs_info[id].ctrl_addr, reg);
	return ret;
}

static int palmas_getvoltage_extreg(struct regulator_dev *rdev)
{
	return 4300 * 1000;
}


static struct regulator_ops palmas_ops_extreg = {
	.is_enabled		= palmas_is_enabled_extreg,
	.enable			= palmas_enable_extreg,
	.disable		= palmas_disable_extreg,
	.get_voltage		= palmas_getvoltage_extreg,
};

/*
 * setup the hardware based sleep configuration of the SMPS/LDO regulators
 * from the platform data. This is different to the software based control
 * supported by the regulator framework as it is controlled by toggling
 * pins on the PMIC such as PREQ, SYSEN, ...
 */
static int palmas_smps_init(struct palmas *palmas, int id,
		struct palmas_reg_init *reg_init)
{
	unsigned int reg;
	unsigned int addr;
	int ret;

	addr = palmas_regs_info[id].ctrl_addr;

	ret = palmas_smps_read(palmas, addr, &reg);
	if (ret)
		return ret;

	if (id != PALMAS_REG_SMPS10) {
		if (reg_init->warm_reset)
			reg |= PALMAS_SMPS12_CTRL_WR_S;

		if (reg_init->roof_floor)
			reg |= PALMAS_SMPS12_CTRL_ROOF_FLOOR_EN;

		reg &= ~PALMAS_SMPS12_CTRL_MODE_SLEEP_MASK;
		if (reg_init->mode_sleep)
			reg |= reg_init->mode_sleep <<
					PALMAS_SMPS12_CTRL_MODE_SLEEP_SHIFT;
	} else {
		if (reg_init->mode_sleep) {
			reg &= ~PALMAS_SMPS10_CTRL_MODE_SLEEP_MASK;
			reg |= reg_init->mode_sleep <<
					PALMAS_SMPS10_CTRL_MODE_SLEEP_SHIFT;
		}

	}
	ret = palmas_smps_write(palmas, addr, reg);
	if (ret)
		return ret;

	if ((id != PALMAS_REG_SMPS10) && reg_init->roof_floor) {
		int sleep_id = palmas_regs_info[id].sleep_id;
		ret = palmas_ext_power_req_config(palmas, sleep_id,
					reg_init->roof_floor, true);
		if (ret < 0) {
			dev_err(palmas->dev,
				"Error in configuring external control\n");
			return ret;
		}

		if (id == PALMAS_REG_SMPS123) {
			ret = palmas_ext_power_req_config(palmas,
					PALMAS_SLEEP_REQSTR_ID_SMPS3,
					reg_init->roof_floor, true);
			if (ret < 0) {
				dev_err(palmas->dev,
					"Error in configuring ext control\n");
				return ret;
			}
		}
	}

	if (palmas_regs_info[id].tstep_addr && reg_init->tstep) {
		addr = palmas_regs_info[id].tstep_addr;

		reg = reg_init->tstep & PALMAS_SMPS12_TSTEP_TSTEP_MASK;

		ret = palmas_smps_write(palmas, addr, reg);
		if (ret)
			return ret;
	}

	if (palmas_regs_info[id].vsel_addr && reg_init->vsel) {
		addr = palmas_regs_info[id].vsel_addr;

		reg = reg_init->vsel;

		ret = palmas_smps_write(palmas, addr, reg);
		if (ret)
			return ret;
	}


	return 0;
}

static int palmas_ldo_init(struct palmas *palmas, int id,
		struct palmas_reg_init *reg_init)
{
	unsigned int reg;
	unsigned int addr;
	int ret;

	addr = palmas_regs_info[id].ctrl_addr;

	ret = palmas_ldo_read(palmas, addr, &reg);
	if (ret)
		return ret;

	if (reg_init->warm_reset)
		reg |= PALMAS_LDO1_CTRL_WR_S;

	if (reg_init->mode_sleep)
		reg |= PALMAS_LDO1_CTRL_MODE_SLEEP;
	else
		reg &= ~PALMAS_LDO1_CTRL_MODE_SLEEP;

	ret = palmas_ldo_write(palmas, addr, reg);
	if (ret)
		return ret;

	if (reg_init->roof_floor) {
		int sleep_id = palmas_regs_info[id].sleep_id;

		ret = palmas_ext_power_req_config(palmas, sleep_id,
			reg_init->roof_floor, true);
		if (ret < 0) {
			dev_err(palmas->dev,
				"Error in configuring external control\n");
			return ret;
		}
	}

	return 0;
}

static int palmas_extreg_init(struct palmas *palmas, int id,
		struct palmas_reg_init *reg_init)
{
	unsigned int reg;
	unsigned int addr;
	int ret;

	addr = palmas_regs_info[id].ctrl_addr;

	ret = palmas_resource_read(palmas, addr, &reg);
	if (ret)
		return ret;

	if (reg_init->mode_sleep)
		reg |= PALMAS_REGEN1_CTRL_MODE_SLEEP;
	else
		reg &= ~PALMAS_REGEN1_CTRL_MODE_SLEEP;

	ret = palmas_resource_write(palmas, addr, reg);
	if (ret < 0)
		return ret;

	if (reg_init->roof_floor) {
		int sleep_id = palmas_regs_info[id].sleep_id;

		ret = palmas_ext_power_req_config(palmas, sleep_id,
			reg_init->roof_floor, true);
		if (ret < 0) {
			dev_err(palmas->dev,
				"Error in configuring external control\n");
			return ret;
		}
	}
	return 0;
}

static void palmas_disable_smps10_boost(struct palmas *palmas)
{
	unsigned int addr;
	int ret;

	addr = palmas_regs_info[PALMAS_REG_SMPS10].ctrl_addr;

	ret = palmas_smps_write(palmas, addr, 0x00);
	if (ret < 0) {
		dev_err(palmas->dev, "Error in disabling smps10 boost\n");
		return;
	}

}

static void palmas_enable_smps10_boost(struct palmas *palmas)
{
	unsigned int reg;
	unsigned int addr;
	int ret;

	addr = palmas_regs_info[PALMAS_REG_SMPS10].ctrl_addr;

	ret = palmas_smps_read(palmas, addr, &reg);
	if (ret) {
		dev_err(palmas->dev, "Error in reading smps10 control reg\n");
		return;
	}

	reg |= PALMA_SMPS10_VSEL;
	reg |= PALMA_SMPS10_BOOST_EN;

	ret = palmas_smps_write(palmas, addr, reg);
	if (ret < 0) {
		dev_err(palmas->dev, "Error in disabling smps10 boost\n");
		return;
	}
}

static void palmas_enable_ldo8_track(struct palmas *palmas)
{
	unsigned int reg;
	unsigned int addr;
	int ret;

	addr = palmas_regs_info[PALMAS_REG_LDO8].ctrl_addr;

	ret = palmas_ldo_read(palmas, addr, &reg);
	if (ret) {
		dev_err(palmas->dev, "Error in reading ldo8 control reg\n");
		return;
	}

	reg |= PALMAS_LDO8_CTRL_LDO_TRACKING_EN;
	ret = palmas_ldo_write(palmas, addr, reg);
	if (ret < 0) {
		dev_err(palmas->dev, "Error in enabling tracking mode\n");
		return;
	}
	/*
	 * When SMPS4&5 is set to off and LDO8 tracking is enabled, the LDO8
	 * output is defined by the LDO8_VOLTAGE.VSEL register divided by two,
	 * and can be set from 0.45 to 1.65 V.
	 */
	addr = palmas_regs_info[PALMAS_REG_LDO8].vsel_addr;
	ret = palmas_ldo_read(palmas, addr, &reg);
	if (ret) {
		dev_err(palmas->dev, "Error in reading ldo8 voltage reg\n");
		return;
	}

	reg = (reg << 1) & PALMAS_LDO8_VOLTAGE_VSEL_MASK;
	ret = palmas_ldo_write(palmas, addr, reg);
	if (ret < 0) {
		dev_err(palmas->dev, "Error in setting ldo8 voltage reg\n");
		return;
	}

	/*
	 * Errata ES1.0, 2,0 and 2.1
	 * When Tracking is enbled, it need to disable Pull-Down for LDO8 and
	 * when tracking is disabled, SW has to enabe Pull-Down.
	 */
	if (palmas_is_es_version_or_less(palmas, 2, 1)) {
		addr = PALMAS_LDO_PD_CTRL1;
		ret = palmas_ldo_read(palmas, addr, &reg);
		if (ret < 0) {
			dev_err(palmas->dev,
				"Error in reading pulldown control reg\n");
			return;
		}
		reg &= ~PALMAS_LDO_PD_CTRL1_LDO8;
		ret = palmas_ldo_write(palmas, addr, reg);
		if (ret < 0) {
			dev_err(palmas->dev,
				"Error in setting pulldown control reg\n");
			return;
		}
	}

	return;
}

static void palmas_disable_ldo8_track(struct palmas *palmas)
{
	unsigned int reg;
	unsigned int addr;
	int ret;

	/*
	 * When SMPS4&5 is set to off and LDO8 tracking is enabled, the LDO8
	 * output is defined by the LDO8_VOLTAGE.VSEL register divided by two,
	 * and can be set from 0.45 to 1.65 V.
	 */
	addr = palmas_regs_info[PALMAS_REG_LDO8].vsel_addr;
	ret = palmas_ldo_read(palmas, addr, &reg);
	if (ret) {
		dev_err(palmas->dev, "Error in reading ldo8 voltage reg\n");
		return;
	}

	reg = (reg >> 1) & PALMAS_LDO8_VOLTAGE_VSEL_MASK;
	ret = palmas_ldo_write(palmas, addr, reg);
	if (ret < 0) {
		dev_err(palmas->dev, "Error in setting ldo8 voltage reg\n");
		return;
	}

	/* Disable the tracking mode */
	addr = palmas_regs_info[PALMAS_REG_LDO8].ctrl_addr;
	ret = palmas_ldo_read(palmas, addr, &reg);
	if (ret) {
		dev_err(palmas->dev, "Error in reading ldo8 control reg\n");
		return;
	}
	reg &= ~PALMAS_LDO8_CTRL_LDO_TRACKING_EN;
	ret = palmas_ldo_write(palmas, addr, reg);
	if (ret < 0) {
		dev_err(palmas->dev, "Error in disabling tracking mode\n");
		return;
	}

	/*
	 * Errata ES1.0, 2,0 and 2.1
	 * When Tracking is enbled, it need to disable Pull-Down for LDO8 and
	 * when tracking is disabled, SW has to enabe Pull-Down.
	 */
	if (palmas_is_es_version_or_less(palmas, 2, 1)) {
		addr = PALMAS_LDO_PD_CTRL1;
		ret = palmas_ldo_read(palmas, addr, &reg);
		if (ret < 0) {
			dev_err(palmas->dev,
				"Error in reading pulldown control reg\n");
			return;
		}
		reg |= PALMAS_LDO_PD_CTRL1_LDO8;
		ret = palmas_ldo_write(palmas, addr, reg);
		if (ret < 0) {
			dev_err(palmas->dev,
				"Error in setting pulldown control reg\n");
			return;
		}
	}

	return;
}

static void palmas_dvfs_init(struct palmas *palmas,
			struct palmas_pmic_platform_data *pdata)
{
	int slave;
	struct palmas_dvfs_init_data *dvfs_idata = pdata->dvfs_init_data;
	int data_size = pdata->dvfs_init_data_size;
	unsigned int reg, addr;
	int ret;
	int sleep_id;
	int i;

	if (!dvfs_idata || !data_size)
		return;

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_DVFS_BASE);
	for (i = 0; i < data_size; i++) {
		struct palmas_dvfs_init_data *dvfs_pd =  &dvfs_idata[i];

		sleep_id = palmas_regs_info[dvfs_pd->reg_id].sleep_id;
		if (!dvfs_pd->en_pwm)
			continue;

		ret = palmas_ext_power_req_config(palmas, sleep_id,
				dvfs_pd->ext_ctrl, true);
		if (ret < 0) {
			dev_err(palmas->dev,
					"Error in configuring external control\n");
			goto err;
		}

		addr = PALMAS_BASE_TO_REG(PALMAS_DVFS_BASE,
				(PALMAS_SMPS_DVFS1_CTRL) + i*3);
		reg =  (1 << PALMAS_SMPS_DVFS1_ENABLE_SHIFT);
		if (dvfs_pd->step_20mV)
			reg |= (1 << PALMAS_SMPS_DVFS1_OFFSET_STEP_SHIFT);

		ret = regmap_write(palmas->regmap[slave], addr, reg);
		if (ret)
			goto err;

		addr = PALMAS_BASE_TO_REG(PALMAS_DVFS_BASE,
				(PALMAS_SMPS_DVFS1_VOLTAGE_MAX) + i*3);
		if (!(dvfs_pd->max_voltage_uV >= DVFS_BASE_VOLTAGE_UV &&
			dvfs_pd->max_voltage_uV <= DVFS_MAX_VOLTAGE_UV))
			goto err;

		reg = DIV_ROUND_UP((dvfs_pd->max_voltage_uV -
			DVFS_BASE_VOLTAGE_UV), DVFS_VOLTAGE_STEP_UV) + 6;
		ret = regmap_write(palmas->regmap[slave], addr, reg);
		if (ret)
			goto err;

		addr = palmas_regs_info[dvfs_pd->reg_id].fvsel_addr;
		reg = (1 << PALMAS_SMPS12_FORCE_CMD_SHIFT);
		reg |= DIV_ROUND_UP((dvfs_pd->base_voltage_uV -
			DVFS_BASE_VOLTAGE_UV), DVFS_VOLTAGE_STEP_UV) + 6;
		ret = palmas_smps_write(palmas, addr, reg);
		if (ret)
			goto  err;

	}

	return;
err:
	dev_err(palmas->dev, "Failed to initilize cl dvfs(%d)", i);
	return;
}

static __devinit int palmas_probe(struct platform_device *pdev)
{
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	struct palmas_pmic_platform_data *pdata = pdev->dev.platform_data;
	struct regulator_dev *rdev;
	struct palmas_pmic *pmic;
	struct palmas_reg_init *reg_init;
	struct regulator_init_data *reg_data;
	int id = 0, ret;
	unsigned int addr, reg;

	if (!pdata)
		return -EINVAL;
	if (!pdata->reg_data)
		return -EINVAL;

	pmic = devm_kzalloc(&pdev->dev, sizeof(*pmic), GFP_KERNEL);
	if (!pmic)
		return -ENOMEM;

	pmic->dev = &pdev->dev;
	pmic->palmas = palmas;
	palmas->pmic = pmic;
	platform_set_drvdata(pdev, pmic);

	ret = palmas_smps_read(palmas, PALMAS_SMPS_CTRL, &reg);
	if (ret)
		goto err_unregister_regulator;

	if (reg & PALMAS_SMPS_CTRL_SMPS12_SMPS123_EN)
		pmic->smps123 = 1;

	if (reg & PALMAS_SMPS_CTRL_SMPS45_SMPS457_EN)
		pmic->smps457 = 1;

	for (id = 0; id < PALMAS_REG_LDO1; id++) {
		bool ramp_delay_support = false;

		reg_init = NULL;
		reg_data = pdata->reg_data[id];

		/*
		 * Miss out regulators which are not available due
		 * to slaving configurations.
		 */
		switch (id) {
		case PALMAS_REG_SMPS12:
		case PALMAS_REG_SMPS3:
			if (pmic->smps123)
				continue;
			if (id == PALMAS_REG_SMPS12)
				ramp_delay_support = true;
			break;
		case PALMAS_REG_SMPS123:
			if (!pmic->smps123)
				continue;
			ramp_delay_support = true;
			break;
		case PALMAS_REG_SMPS45:
		case PALMAS_REG_SMPS7:
			if (pmic->smps457)
				continue;
			if (id == PALMAS_REG_SMPS45)
				ramp_delay_support = true;
			break;
		case PALMAS_REG_SMPS457:
			if (!pmic->smps457)
				continue;
			ramp_delay_support = true;
			break;
		}
		if ((id == PALMAS_REG_SMPS6) && (id == PALMAS_REG_SMPS8))
				ramp_delay_support = true;

		if (ramp_delay_support) {
			addr = palmas_regs_info[id].tstep_addr;
			ret = palmas_smps_read(pmic->palmas, addr, &reg);
			if (ret < 0) {
				dev_err(&pdev->dev,
					"reading TSTEP reg failed: %d\n", ret);
				goto err_unregister_regulator;
			}
			pmic->desc[id].ramp_delay =
					palmas_smps_ramp_delay[reg & 0x3];
			pmic->ramp_delay[id] = pmic->desc[id].ramp_delay;
		}

		/* Register the regulators */
		pmic->desc[id].name = palmas_regs_info[id].name;
		pmic->desc[id].id = id;

		if (id != PALMAS_REG_SMPS10) {
			pmic->desc[id].ops = &palmas_ops_smps;
			pmic->desc[id].n_voltages = PALMAS_SMPS_NUM_VOLTAGES;
		} else {
			pmic->desc[id].n_voltages = PALMAS_SMPS10_NUM_VOLTAGES;
			pmic->desc[id].ops = &palmas_ops_smps10;
			pmic->desc[id].vsel_reg = PALMAS_SMPS10_CTRL;
			pmic->desc[id].vsel_mask = SMPS10_VSEL;
			pmic->desc[id].enable_reg = PALMAS_SMPS10_STATUS;
			pmic->desc[id].enable_mask = SMPS10_BOOST_EN;
		}

		pmic->desc[id].type = REGULATOR_VOLTAGE;
		pmic->desc[id].owner = THIS_MODULE;

		/* Initialise sleep/init values from platform data */
		if (pdata && reg_data && pdata->reg_init) {
			reg_init = pdata->reg_init[id];
			if (reg_init) {
				ret = palmas_smps_init(palmas, id, reg_init);
				if (ret)
					goto err_unregister_regulator;
			}
		}

		/*
		 * read and store the RANGE bit for later use
		 * This must be done before regulator is probed otherwise
		 * we error in probe with unsuportable ranges.
		 * Read the smps mode for later use.
		 */
		if (id != PALMAS_REG_SMPS10) {
			addr = palmas_regs_info[id].vsel_addr;

			ret = palmas_smps_read(pmic->palmas, addr, &reg);
			if (ret)
				goto err_unregister_regulator;
			if (reg & PALMAS_SMPS12_VOLTAGE_RANGE)
				pmic->range[id] = 1;

			/* Read the smps mode for later use. */
			addr = palmas_regs_info[id].ctrl_addr;
			ret = palmas_smps_read(pmic->palmas, addr, &reg);
			if (ret)
				goto err_unregister_regulator;
			pmic->current_mode_reg[id] = reg &
					PALMAS_SMPS12_CTRL_MODE_ACTIVE_MASK;
		}

		rdev = regulator_register(&pmic->desc[id],
			palmas->dev, reg_data, pmic, NULL);

		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev,
				"failed to register %s regulator\n",
				pdev->name);
			ret = PTR_ERR(rdev);
			goto err_unregister_regulator;
		}

		if (reg_init && reg_data) {
			pmic->roof_floor[id] = reg_init->roof_floor;
			pmic->config_flags[id] = reg_init->config_flags;
		}

		/* Save regulator for cleanup */
		pmic->rdev[id] = rdev;
	}

	/* Start this loop from the id left from previous loop */
	for (; id < PALMAS_NUM_REGS; id++) {

		reg_data = pdata->reg_data[id];
		/* Miss out regulators which are not available due
		 * to alternate functions.
		 */

		/* Register the regulators */
		pmic->desc[id].name = palmas_regs_info[id].name;
		pmic->desc[id].id = id;
		pmic->desc[id].type = REGULATOR_VOLTAGE;
		pmic->desc[id].owner = THIS_MODULE;

		if (id < PALMAS_REG_REGEN1) {
			pmic->desc[id].n_voltages = PALMAS_LDO_NUM_VOLTAGES;
			pmic->desc[id].ops = &palmas_ops_ldo;
			pmic->desc[id].enable_reg =
					palmas_regs_info[id].ctrl_addr;
			pmic->desc[id].enable_mask =
					PALMAS_LDO1_CTRL_MODE_ACTIVE;
		} else {
			pmic->desc[id].n_voltages = 1;
			pmic->desc[id].ops = &palmas_ops_extreg;
		}

		rdev = regulator_register(&pmic->desc[id],
			palmas->dev, reg_data, pmic, NULL);

		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev,
				"failed to register %s regulator\n",
				pdev->name);
			ret = PTR_ERR(rdev);
			goto err_unregister_regulator;
		}

		/* Save regulator for cleanup */
		pmic->rdev[id] = rdev;

		/* Initialise sleep/init values from platform data */
		if (reg_data && pdata->reg_init) {
			reg_init = pdata->reg_init[id];
			if (reg_init) {
				pmic->roof_floor[id] = reg_init->roof_floor;
				pmic->config_flags[id] = reg_init->config_flags;

				if (id < PALMAS_REG_REGEN1)
					ret = palmas_ldo_init(palmas, id,
								reg_init);
				else
					ret = palmas_extreg_init(palmas, id,
								reg_init);
				if (ret)
					goto err_unregister_regulator;
			}
		}
	}

	/* Check if LDO8 is in tracking mode or not */
	if (pdata->enable_ldo8_tracking)
		palmas_enable_ldo8_track(palmas);

	palmas_dvfs_init(palmas, pdata);
	return 0;

err_unregister_regulator:
	while (--id >= 0)
		regulator_unregister(pmic->rdev[id]);
	kfree(pmic->rdev);
	kfree(pmic->desc);
	kfree(pmic);
	return ret;
}

static int __devexit palmas_remove(struct platform_device *pdev)
{
	struct palmas_pmic *pmic = platform_get_drvdata(pdev);
	int id;

	for (id = 0; id < PALMAS_NUM_REGS; id++)
		regulator_unregister(pmic->rdev[id]);

	kfree(pmic->rdev);
	kfree(pmic->desc);
	kfree(pmic);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int palmas_suspend(struct device *dev)
{
	struct palmas *palmas = dev_get_drvdata(dev->parent);
	struct palmas_pmic *pmic = dev_get_drvdata(dev);
	struct palmas_pmic_platform_data *pdata = dev_get_platdata(dev);
	int id;

	/* Check if LDO8 is in tracking mode disable in suspend or not */
	if (pdata->enable_ldo8_tracking && pdata->disabe_ldo8_tracking_suspend)
		palmas_disable_ldo8_track(palmas);

	if (pdata->disable_smps10_boost_suspend) {
		if (!pmic->smps10_regulator_enabled)
			palmas_disable_smps10_boost(palmas);
		else
			pmic->smps10_boost_disable_deferred = true;
	}

	for (id = 0; id < PALMAS_NUM_REGS; id++) {
		if (pmic->config_flags[id] &
			PALMAS_REGULATOR_CONFIG_SUSPEND_FORCE_OFF) {
			if (pmic->desc[id].ops->disable)
				pmic->desc[id].ops->disable(pmic->rdev[id]);
		}
	}
	return 0;
}

static int palmas_resume(struct device *dev)
{
	struct palmas *palmas = dev_get_drvdata(dev->parent);
	struct palmas_pmic *pmic = dev_get_drvdata(dev);
	struct palmas_pmic_platform_data *pdata = dev_get_platdata(dev);
	int id;

	/* Check if LDO8 is in tracking mode disable in suspend or not */
	if (pdata->enable_ldo8_tracking && pdata->disabe_ldo8_tracking_suspend)
		palmas_enable_ldo8_track(palmas);

	if (pdata->disable_smps10_boost_suspend &&
			!pmic->smps10_regulator_enabled)
		palmas_enable_smps10_boost(palmas);

	for (id = 0; id < PALMAS_NUM_REGS; id++) {
		if (pmic->config_flags[id] &
			PALMAS_REGULATOR_CONFIG_SUSPEND_FORCE_OFF) {
			if (pmic->desc[id].ops->enable)
				pmic->desc[id].ops->enable(pmic->rdev[id]);
		}
	}
	return 0;
}
#endif
static const struct dev_pm_ops palmas_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(palmas_suspend, palmas_resume)
};


static struct platform_driver palmas_driver = {
	.driver = {
		.name = "palmas-pmic",
		.owner = THIS_MODULE,
		.pm     = &palmas_pm_ops,
	},
	.probe = palmas_probe,
	.remove = __devexit_p(palmas_remove),
};

static int __init palmas_init(void)
{
	return platform_driver_register(&palmas_driver);
}
subsys_initcall(palmas_init);

static void __exit palmas_exit(void)
{
	platform_driver_unregister(&palmas_driver);
}
module_exit(palmas_exit);

MODULE_AUTHOR("Graeme Gregory <gg@slimlogic.co.uk>");
MODULE_DESCRIPTION("Palmas voltage regulator driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:palmas-pmic");
