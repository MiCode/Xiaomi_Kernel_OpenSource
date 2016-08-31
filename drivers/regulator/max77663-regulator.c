/*
 * drivers/regulator/max77663-regulator.c
 * Maxim LDO and Buck regulators driver
 *
 * Copyright 2011-2012 Maxim Integrated Products, Inc.
 * Copyright (C) 2011-2012 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 */

#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77663-core.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/max77663-regulator.h>

/* Regulator types */
#define REGULATOR_TYPE_SD		0
#define REGULATOR_TYPE_LDO_N		1
#define REGULATOR_TYPE_LDO_P		2

/* SD and LDO Registers */
#define MAX77663_REG_SD0		0x16
#define MAX77663_REG_SD1		0x17
#define MAX77663_REG_SD2		0x18
#define MAX77663_REG_SD3		0x19
#define MAX77663_REG_SD4		0x1A
#define MAX77663_REG_DVSSD0		0x1B
#define MAX77663_REG_DVSSD1		0x1C
#define MAX77663_REG_SD0_CFG		0x1D
#define MAX77663_REG_DVSSD0_CFG		MAX77663_REG_SD0_CFG
#define MAX77663_REG_SD1_CFG		0x1E
#define MAX77663_REG_DVSSD1_CFG		MAX77663_REG_SD1_CFG
#define MAX77663_REG_SD2_CFG		0x1F
#define MAX77663_REG_SD3_CFG		0x20
#define MAX77663_REG_SD4_CFG		0x21
#define MAX77663_REG_LDO0_CFG		0x23
#define MAX77663_REG_LDO0_CFG2		0x24
#define MAX77663_REG_LDO1_CFG		0x25
#define MAX77663_REG_LDO1_CFG2		0x26
#define MAX77663_REG_LDO2_CFG		0x27
#define MAX77663_REG_LDO2_CFG2		0x28
#define MAX77663_REG_LDO3_CFG		0x29
#define MAX77663_REG_LDO3_CFG2		0x2A
#define MAX77663_REG_LDO4_CFG		0x2B
#define MAX77663_REG_LDO4_CFG2		0x2C
#define MAX77663_REG_LDO5_CFG		0x2D
#define MAX77663_REG_LDO5_CFG2		0x2E
#define MAX77663_REG_LDO6_CFG		0x2F
#define MAX77663_REG_LDO6_CFG2		0x30
#define MAX77663_REG_LDO7_CFG		0x31
#define MAX77663_REG_LDO7_CFG2		0x32
#define MAX77663_REG_LDO8_CFG		0x33
#define MAX77663_REG_LDO8_CFG2		0x34
#define MAX77663_REG_LDO_CFG3		0x35

/* Power Mode */
#define POWER_MODE_NORMAL		3
#define POWER_MODE_LPM			2
#define POWER_MODE_GLPM			1
#define POWER_MODE_DISABLE		0
#define SD_POWER_MODE_MASK		0x30
#define SD_POWER_MODE_SHIFT		4
#define LDO_POWER_MODE_MASK		0xC0
#define LDO_POWER_MODE_SHIFT		6

/* SD Slew Rate */
#define SD_SR_13_75			0
#define SD_SR_27_5			1
#define SD_SR_55			2
#define SD_SR_100			3
#define SD_SR_MASK			0xC0
#define SD_SR_SHIFT			6

/* SD Forced PWM Mode */
#define SD_FPWM_MASK			0x04
#define SD_FPWM_SHIFT			2

/* SD Failling slew rate Active-Discharge Mode */
#define SD_FSRADE_MASK			0x01
#define SD_FSRADE_SHIFT		0

/* LDO Configuration 3 */
#define TRACK4_MASK			0x20
#define TRACK4_SHIFT			5

/* Voltage */
#define SDX_VOLT_MASK			0xFF
#define SD0_VOLT_MASK			0x3F
#define SD1_VOLT_MASK			0x3F
#define LDO_VOLT_MASK			0x3F

/* FPS Registers */
#define MAX77663_REG_FPS_CFG0		0x43
#define MAX77663_REG_FPS_CFG1		0x44
#define MAX77663_REG_FPS_CFG2		0x45
#define MAX77663_REG_FPS_LDO0		0x46
#define MAX77663_REG_FPS_LDO1		0x47
#define MAX77663_REG_FPS_LDO2		0x48
#define MAX77663_REG_FPS_LDO3		0x49
#define MAX77663_REG_FPS_LDO4		0x4A
#define MAX77663_REG_FPS_LDO5		0x4B
#define MAX77663_REG_FPS_LDO6		0x4C
#define MAX77663_REG_FPS_LDO7		0x4D
#define MAX77663_REG_FPS_LDO8		0x4E
#define MAX77663_REG_FPS_SD0		0x4F
#define MAX77663_REG_FPS_SD1		0x50
#define MAX77663_REG_FPS_SD2		0x51
#define MAX77663_REG_FPS_SD3		0x52
#define MAX77663_REG_FPS_SD4		0x53
#define MAX77663_REG_FPS_NONE		0

#define FPS_TIME_PERIOD_MASK		0x38
#define FPS_TIME_PERIOD_SHIFT		3
#define FPS_EN_SRC_MASK			0x06
#define FPS_EN_SRC_SHIFT		1
#define FPS_SW_EN_MASK			0x01
#define FPS_SW_EN_SHIFT			0
#define FPS_SRC_MASK			0xC0
#define FPS_SRC_SHIFT			6
#define FPS_PU_PERIOD_MASK		0x38
#define FPS_PU_PERIOD_SHIFT		3
#define FPS_PD_PERIOD_MASK		0x07
#define FPS_PD_PERIOD_SHIFT		0

/* Chip Identification Register */
#define MAX77663_REG_CID5		0x5D

#define CID_DIDM_MASK			0xF0
#define CID_DIDM_SHIFT			4

#define SD_SAFE_DOWN_UV			50000 /* 50mV */

enum {
	VOLT_REG = 0,
	CFG_REG,
	FPS_REG,
};

struct max77663_register {
	u8 addr;
	u8 val;
};

struct max77663_regulator_info {
	u8 id;
	u8 type;
	u32 min_uV;
	u32 max_uV;
	u32 step_uV;

	struct max77663_register regs[3]; /* volt, cfg, fps */

	struct regulator_desc desc;

	u8 volt_mask;

	u8 power_mode_mask;
	u8 power_mode_shift;
};

struct max77663_regulator {
	struct max77663_regulator_info *rinfo;
	struct regulator_dev *rdev;
	struct device *dev;
	struct max77663_regulator_platform_data *pdata;
	u32 regulator_mode;
	u8 power_mode;
	enum max77663_regulator_fps_src fps_src;
	u8 val[3]; /* volt, cfg, fps */
	int safe_down_uV; /* for stable down scaling */
};

#define fps_src_name(fps_src)	\
	(fps_src == FPS_SRC_0 ? "FPS_SRC_0" :	\
	fps_src == FPS_SRC_1 ? "FPS_SRC_1" :	\
	fps_src == FPS_SRC_2 ? "FPS_SRC_2" : "FPS_SRC_NONE")

static int fps_cfg_init;
static struct max77663_register fps_cfg_regs[] = {
	{
		.addr = MAX77663_REG_FPS_CFG0,
	},
	{
		.addr = MAX77663_REG_FPS_CFG1,
	},
	{
		.addr = MAX77663_REG_FPS_CFG2,
	},
};

static inline struct max77663_regulator_platform_data
*_to_pdata(struct max77663_regulator *reg)
{
	return reg->pdata;
}

static inline struct device *_to_parent(struct max77663_regulator *reg)
{
	return reg->dev->parent;
}

static inline int max77663_regulator_cache_write(struct max77663_regulator *reg,
					u8 addr, u8 mask, u8 val, u8 *cache)
{
	struct device *parent = _to_parent(reg);
	u8 new_val;
	int ret;

	new_val = (*cache & ~mask) | (val & mask);
	if (*cache != new_val) {
		ret = max77663_write(parent, addr, &new_val, 1, 0);
		if (ret < 0)
			return ret;

		*cache = new_val;
	}
	return 0;
}

static int
max77663_regulator_set_fps_src(struct max77663_regulator *reg,
			       enum max77663_regulator_fps_src fps_src)
{
	int ret;
	struct max77663_regulator_info *rinfo = reg->rinfo;

	if ((rinfo->regs[FPS_REG].addr == MAX77663_REG_FPS_NONE) ||
			(reg->fps_src == fps_src))
		return 0;

	switch (fps_src) {
	case FPS_SRC_0:
	case FPS_SRC_1:
	case FPS_SRC_2:
	case FPS_SRC_NONE:
		break;
	case FPS_SRC_DEF:
		return 0;
	default:
		return -EINVAL;
	}

	ret = max77663_regulator_cache_write(reg, rinfo->regs[FPS_REG].addr,
					FPS_SRC_MASK, fps_src << FPS_SRC_SHIFT,
					&reg->val[FPS_REG]);
	if (ret < 0)
		return ret;

	reg->fps_src = fps_src;
	return 0;
}

static int max77663_regulator_set_fps(struct max77663_regulator *reg)
{
	struct max77663_regulator_platform_data *pdata = _to_pdata(reg);
	struct max77663_regulator_info *rinfo = reg->rinfo;
	u8 fps_val = 0, fps_mask = 0;
	int ret = 0;

	if (reg->rinfo->regs[FPS_REG].addr == MAX77663_REG_FPS_NONE)
		return 0;

	if (reg->fps_src == FPS_SRC_NONE)
		return 0;

	/* FPS power up period setting */
	if (pdata->fps_pu_period != FPS_POWER_PERIOD_DEF) {
		fps_val |= (pdata->fps_pu_period << FPS_PU_PERIOD_SHIFT);
		fps_mask |= FPS_PU_PERIOD_MASK;
	}

	/* FPS power down period setting */
	if (pdata->fps_pd_period != FPS_POWER_PERIOD_DEF) {
		fps_val |= (pdata->fps_pd_period << FPS_PD_PERIOD_SHIFT);
		fps_mask |= FPS_PD_PERIOD_MASK;
	}

	if (fps_val || fps_mask)
		ret = max77663_regulator_cache_write(reg,
					rinfo->regs[FPS_REG].addr, fps_mask,
					fps_val, &reg->val[FPS_REG]);

	return ret;
}

static int
max77663_regulator_set_fps_cfg(struct max77663_regulator *reg,
			       struct max77663_regulator_fps_cfg *fps_cfg)
{
	u8 val, mask;

	if ((fps_cfg->src < FPS_SRC_0) || (fps_cfg->src > FPS_SRC_2))
		return -EINVAL;

	val = (fps_cfg->en_src << FPS_EN_SRC_SHIFT);
	mask = FPS_EN_SRC_MASK;

	if (fps_cfg->time_period != FPS_TIME_PERIOD_DEF) {
		val |= (fps_cfg->time_period << FPS_TIME_PERIOD_SHIFT);
		mask |= FPS_TIME_PERIOD_MASK;
	}

	return max77663_regulator_cache_write(reg,
					fps_cfg_regs[fps_cfg->src].addr, mask,
					val, &fps_cfg_regs[fps_cfg->src].val);
}

static int
max77663_regulator_set_fps_cfgs(struct max77663_regulator *reg,
				struct max77663_regulator_fps_cfg *fps_cfgs,
				int num_fps_cfgs)
{
	struct device *parent = _to_parent(reg);
	int i, ret;

	if (fps_cfg_init)
		return 0;

	for (i = 0; i <= FPS_SRC_2; i++) {
		ret = max77663_read(parent, fps_cfg_regs[i].addr,
				    &fps_cfg_regs[i].val, 1, 0);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < num_fps_cfgs; i++) {
		ret = max77663_regulator_set_fps_cfg(reg, &fps_cfgs[i]);
		if (ret < 0)
			return ret;
	}
	fps_cfg_init = 1;

	return 0;
}

static int
max77663_regulator_set_power_mode(struct max77663_regulator *reg, u8 power_mode)
{
	struct max77663_regulator_info *rinfo = reg->rinfo;
	u8 mask = rinfo->power_mode_mask;
	u8 shift = rinfo->power_mode_shift;
	int ret;

	if (rinfo->type == REGULATOR_TYPE_SD)
		ret = max77663_regulator_cache_write(reg,
						     rinfo->regs[CFG_REG].addr,
						     mask, power_mode << shift,
						     &reg->val[CFG_REG]);
	else
		ret = max77663_regulator_cache_write(reg,
						     rinfo->regs[VOLT_REG].addr,
						     mask, power_mode << shift,
						     &reg->val[VOLT_REG]);

	if (ret < 0)
		return ret;

	reg->power_mode = power_mode;
	return ret;
}

static u8 max77663_regulator_get_power_mode(struct max77663_regulator *reg)
{
	struct max77663_regulator_info *rinfo = reg->rinfo;
	u8 mask = rinfo->power_mode_mask;
	u8 shift = rinfo->power_mode_shift;

	if (rinfo->type == REGULATOR_TYPE_SD)
		reg->power_mode = (reg->val[CFG_REG] & mask) >> shift;
	else
		reg->power_mode = (reg->val[VOLT_REG] & mask) >> shift;

	return reg->power_mode;
}

static int max77663_regulator_do_set_voltage(struct max77663_regulator *reg,
					     int min_uV, int max_uV)
{
	struct max77663_regulator_info *rinfo = reg->rinfo;
	u8 addr = rinfo->regs[VOLT_REG].addr;
	u8 mask = rinfo->volt_mask;
	u8 *cache = &reg->val[VOLT_REG];
	u8 val;
	int old_uV, new_uV, safe_uV;
	int i, steps = 1;
	int ret = 0;

	if (min_uV < rinfo->min_uV || max_uV > rinfo->max_uV)
		return -EDOM;

	old_uV = (*cache & mask) * rinfo->step_uV + rinfo->min_uV;

	if ((old_uV > min_uV) && (reg->safe_down_uV >= rinfo->step_uV)) {
		steps = DIV_ROUND_UP(old_uV - min_uV, reg->safe_down_uV);
		safe_uV = -reg->safe_down_uV;
	}

	if (steps == 1) {
		val = (min_uV - rinfo->min_uV) / rinfo->step_uV;
		ret = max77663_regulator_cache_write(reg, addr, mask, val,
						     cache);
	} else {
		for (i = 0; i < steps; i++) {
			if (abs(min_uV - old_uV) > abs(safe_uV))
				new_uV = old_uV + safe_uV;
			else
				new_uV = min_uV;

			dev_dbg(&reg->rdev->dev, "do_set_voltage: name=%s, "
				"%d/%d, old_uV=%d, new_uV=%d\n",
				reg->rdev->desc->name, i + 1, steps, old_uV,
				new_uV);

			val = (new_uV - rinfo->min_uV) / rinfo->step_uV;
			ret = max77663_regulator_cache_write(reg, addr, mask,
							     val, cache);
			if (ret < 0)
				return ret;

			old_uV = new_uV;
		}
	}

	return ret;
}

static int max77663_regulator_set_voltage(struct regulator_dev *rdev,
					  int min_uV, int max_uV,
					  unsigned *selector)
{
	struct max77663_regulator *reg = rdev_get_drvdata(rdev);

	dev_dbg(&rdev->dev, "set_voltage: name=%s, min_uV=%d, max_uV=%d\n",
		rdev->desc->name, min_uV, max_uV);
	return max77663_regulator_do_set_voltage(reg, min_uV, max_uV);
}

static int max77663_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct max77663_regulator *reg = rdev_get_drvdata(rdev);
	struct max77663_regulator_info *rinfo = reg->rinfo;
	int volt;

	volt = (reg->val[VOLT_REG] & rinfo->volt_mask)
		* rinfo->step_uV + rinfo->min_uV;

	dev_dbg(&rdev->dev, "get_voltage: name=%s, volt=%d, val=0x%02x\n",
		rdev->desc->name, volt, reg->val[VOLT_REG]);
	return volt;
}

static int max77663_regulator_enable(struct regulator_dev *rdev)
{
	struct max77663_regulator *reg = rdev_get_drvdata(rdev);
	struct max77663_regulator_info *rinfo = reg->rinfo;
	struct max77663_regulator_platform_data *pdata = _to_pdata(reg);
	int power_mode = (pdata->flags & GLPM_ENABLE) ?
			 POWER_MODE_GLPM : POWER_MODE_NORMAL;

	if (reg->fps_src != FPS_SRC_NONE) {
		dev_dbg(&rdev->dev, "enable: Regulator %s using %s\n",
			rdev->desc->name, fps_src_name(reg->fps_src));
		return 0;
	}

	if ((rinfo->id == MAX77663_REGULATOR_ID_SD0)
			&& (pdata->flags & EN2_CTRL_SD0)) {
		dev_dbg(&rdev->dev,
			"enable: Regulator %s is controlled by EN2\n",
			rdev->desc->name);
		return 0;
	}

	/* N-Channel LDOs don't support Low-Power mode. */
	if ((rinfo->type != REGULATOR_TYPE_LDO_N) &&
			(reg->regulator_mode == REGULATOR_MODE_STANDBY))
		power_mode = POWER_MODE_LPM;

	return max77663_regulator_set_power_mode(reg, power_mode);
}

static int max77663_regulator_disable(struct regulator_dev *rdev)
{
	struct max77663_regulator *reg = rdev_get_drvdata(rdev);
	struct max77663_regulator_platform_data *pdata = _to_pdata(reg);
	struct max77663_regulator_info *rinfo = reg->rinfo;
	int power_mode = POWER_MODE_DISABLE;

	if (reg->fps_src != FPS_SRC_NONE) {
		dev_dbg(&rdev->dev, "disable: Regulator %s using %s\n",
			rdev->desc->name, fps_src_name(reg->fps_src));
		return 0;
	}

	if ((rinfo->id == MAX77663_REGULATOR_ID_SD0)
			&& (pdata->flags & EN2_CTRL_SD0)) {
		dev_dbg(&rdev->dev,
			"disable: Regulator %s is controlled by EN2\n",
			rdev->desc->name);
		return 0;
	}

	return max77663_regulator_set_power_mode(reg, power_mode);
}

static int max77663_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct max77663_regulator *reg = rdev_get_drvdata(rdev);
	struct max77663_regulator_platform_data *pdata = _to_pdata(reg);
	struct max77663_regulator_info *rinfo = reg->rinfo;
	int ret = 1;

	if (reg->fps_src != FPS_SRC_NONE) {
		dev_dbg(&rdev->dev, "is_enable: Regulator %s using %s\n",
			rdev->desc->name, fps_src_name(reg->fps_src));
		return 1;
	}

	if ((rinfo->id == MAX77663_REGULATOR_ID_SD0)
			&& (pdata->flags & EN2_CTRL_SD0)) {
		dev_dbg(&rdev->dev,
			"is_enable: Regulator %s is controlled by EN2\n",
			rdev->desc->name);
		return 1;
	}

	if (max77663_regulator_get_power_mode(reg) == POWER_MODE_DISABLE)
		ret = 0;

	return ret;
}

static int max77663_regulator_set_mode(struct regulator_dev *rdev,
				       unsigned int mode)
{
	struct max77663_regulator *reg = rdev_get_drvdata(rdev);
	struct max77663_regulator_platform_data *pdata = _to_pdata(reg);
	struct max77663_regulator_info *rinfo = reg->rinfo;
	u8 power_mode;
	int ret;

	if (mode == REGULATOR_MODE_NORMAL)
		power_mode = (pdata->flags & GLPM_ENABLE) ?
			     POWER_MODE_GLPM : POWER_MODE_NORMAL;
	else if (mode == REGULATOR_MODE_STANDBY) {
		/* N-Channel LDOs don't support Low-Power mode. */
		power_mode = (rinfo->type != REGULATOR_TYPE_LDO_N) ?
			     POWER_MODE_LPM : POWER_MODE_NORMAL;
	} else
		return -EINVAL;

	ret = max77663_regulator_set_power_mode(reg, power_mode);
	if (!ret)
		reg->regulator_mode = mode;

	return ret;
}

static unsigned int max77663_regulator_get_mode(struct regulator_dev *rdev)
{
	struct max77663_regulator *reg = rdev_get_drvdata(rdev);

	return reg->regulator_mode;
}

static struct regulator_ops max77663_ldo_ops = {
	.set_voltage = max77663_regulator_set_voltage,
	.get_voltage = max77663_regulator_get_voltage,
	.enable = max77663_regulator_enable,
	.disable = max77663_regulator_disable,
	.is_enabled = max77663_regulator_is_enabled,
	.set_mode = max77663_regulator_set_mode,
	.get_mode = max77663_regulator_get_mode,
};

static int max77663_regulator_preinit(struct max77663_regulator *reg)
{
	struct max77663_regulator_platform_data *pdata = _to_pdata(reg);
	struct max77663_regulator_info *rinfo = reg->rinfo;
	struct device *parent = _to_parent(reg);
	int i;
	u8 val, mask;
	int ret;

	/* Update registers */
	for (i = 0; i <= FPS_REG; i++) {
		ret = max77663_read(parent, rinfo->regs[i].addr,
				    &reg->val[i], 1, 0);
		if (ret < 0) {
			dev_err(reg->dev,
				"preinit: Failed to get register 0x%x\n",
				rinfo->regs[i].addr);
			return ret;
		}
	}

	/* Update FPS source */
	if (rinfo->regs[FPS_REG].addr == MAX77663_REG_FPS_NONE)
		reg->fps_src = FPS_SRC_NONE;
	else
		reg->fps_src = (reg->val[FPS_REG] & FPS_SRC_MASK)
				>> FPS_SRC_SHIFT;

	dev_dbg(reg->dev, "preinit: initial fps_src=%s\n",
		fps_src_name(reg->fps_src));

	/* Update power mode */
	max77663_regulator_get_power_mode(reg);

	/* Check Chip Identification */
	ret = max77663_read(parent, MAX77663_REG_CID5, &val, 1, 0);
	if (ret < 0) {
		dev_err(reg->dev, "preinit: Failed to get register 0x%x\n",
			MAX77663_REG_CID5);
		return ret;
	}

	/* If metal revision is less than rev.3,
	 * set safe_down_uV for stable down scaling. */
	if ((rinfo->type == REGULATOR_TYPE_SD) &&
			((val & CID_DIDM_MASK) >> CID_DIDM_SHIFT) <= 2)
		reg->safe_down_uV = SD_SAFE_DOWN_UV;
	else
		reg->safe_down_uV = 0;

	/* Set FPS */
	ret = max77663_regulator_set_fps_cfgs(reg, pdata->fps_cfgs,
					      pdata->num_fps_cfgs);
	if (ret < 0) {
		dev_err(reg->dev, "preinit: Failed to set FPSCFG\n");
		return ret;
	}

	/* N-Channel LDOs don't support Low-Power mode. */
	if ((rinfo->type == REGULATOR_TYPE_LDO_N) &&
			(pdata->flags & GLPM_ENABLE))
		pdata->flags &= ~GLPM_ENABLE;

	/* To prevent power rail turn-off when change FPS source,
	 * it must set power mode to NORMAL before change FPS source to NONE
	 * from SRC_0, SRC_1 and SRC_2. */
	if ((reg->fps_src != FPS_SRC_NONE) && (pdata->fps_src == FPS_SRC_NONE)
			&& (reg->power_mode != POWER_MODE_NORMAL)) {
		val = (pdata->flags & GLPM_ENABLE) ?
		      POWER_MODE_GLPM : POWER_MODE_NORMAL;
		ret = max77663_regulator_set_power_mode(reg, val);
		if (ret < 0) {
			dev_err(reg->dev, "preinit: Failed to "
				"set power mode to POWER_MODE_NORMAL\n");
			return ret;
		}
	}

	ret = max77663_regulator_set_fps_src(reg, pdata->fps_src);
	if (ret < 0) {
		dev_err(reg->dev, "preinit: Failed to set FPSSRC to %d\n",
			pdata->fps_src);
		return ret;
	}

	ret = max77663_regulator_set_fps(reg);
	if (ret < 0) {
		dev_err(reg->dev, "preinit: Failed to set FPS\n");
		return ret;
	}

	if (rinfo->type == REGULATOR_TYPE_SD) {
		val = 0;
		mask = 0;

		if (pdata->flags & SD_SLEW_RATE_MASK) {
			mask |= SD_SR_MASK;
			if (pdata->flags & SD_SLEW_RATE_SLOWEST)
				val |= (SD_SR_13_75 << SD_SR_SHIFT);
			else if (pdata->flags & SD_SLEW_RATE_SLOW)
				val |= (SD_SR_27_5 << SD_SR_SHIFT);
			else if (pdata->flags & SD_SLEW_RATE_FAST)
				val |= (SD_SR_55 << SD_SR_SHIFT);
			else
				val |= (SD_SR_100 << SD_SR_SHIFT);
		}

		mask |= SD_FPWM_MASK;
		if (pdata->flags & SD_FORCED_PWM_MODE)
			val |= SD_FPWM_MASK;

		mask |= SD_FSRADE_MASK;
		if (pdata->flags & SD_FSRADE_DISABLE)
			val |= SD_FSRADE_MASK;

		ret = max77663_regulator_cache_write(reg,
				rinfo->regs[CFG_REG].addr, mask, val,
				&reg->val[CFG_REG]);
		if (ret < 0) {
			dev_err(reg->dev, "preinit: "
				"Failed to set register 0x%x\n",
				rinfo->regs[CFG_REG].addr);
			return ret;
		}

		if ((rinfo->id == MAX77663_REGULATOR_ID_SD0)
				&& (pdata->flags & EN2_CTRL_SD0)) {
			val = POWER_MODE_DISABLE;
			ret = max77663_regulator_set_power_mode(reg, val);
			if (ret < 0) {
				dev_err(reg->dev, "preinit: "
					"Failed to set power mode to %d for "
					"EN2_CTRL_SD0\n", val);
				return ret;
			}

			ret = max77663_regulator_set_fps_src(reg, FPS_SRC_NONE);
			if (ret < 0) {
				dev_err(reg->dev, "preinit: "
					"Failed to set FPSSRC to FPS_SRC_NONE "
					"for EN2_CTRL_SD0\n");
				return ret;
			}
		}
	}

	if ((rinfo->id == MAX77663_REGULATOR_ID_LDO4)
			&& (pdata->flags & LDO4_EN_TRACKING)) {
		val = TRACK4_MASK;
		ret = max77663_write(parent, MAX77663_REG_LDO_CFG3, &val, 1, 0);
		if (ret < 0) {
			dev_err(reg->dev, "preinit: "
				"Failed to set register 0x%x\n",
				MAX77663_REG_LDO_CFG3);
			return ret;
		}
	}

	return 0;
}

#define REGULATOR_SD(_id, _volt_mask, _fps_reg, _min_uV, _max_uV, _step_uV) \
	[MAX77663_REGULATOR_ID_##_id] = {			\
		.id = MAX77663_REGULATOR_ID_##_id,		\
		.type = REGULATOR_TYPE_SD,			\
		.volt_mask = _volt_mask##_VOLT_MASK,		\
		.regs = {					\
			[VOLT_REG] = {				\
				.addr = MAX77663_REG_##_id,	\
			},					\
			[CFG_REG] = {				\
				.addr = MAX77663_REG_##_id##_CFG, \
			},					\
			[FPS_REG] = {				\
				.addr = MAX77663_REG_FPS_##_fps_reg, \
			},					\
		},						\
		.min_uV = _min_uV,				\
		.max_uV = _max_uV,				\
		.step_uV = _step_uV,				\
		.power_mode_mask = SD_POWER_MODE_MASK,		\
		.power_mode_shift = SD_POWER_MODE_SHIFT,	\
		.desc = {					\
			.name = max77663_rails(_id),		\
			.id = MAX77663_REGULATOR_ID_##_id,	\
			.ops = &max77663_ldo_ops,		\
			.type = REGULATOR_VOLTAGE,		\
			.owner = THIS_MODULE,			\
		},						\
	}

#define REGULATOR_LDO(_id, _type, _min_uV, _max_uV, _step_uV)	\
	[MAX77663_REGULATOR_ID_##_id] = {			\
		.id = MAX77663_REGULATOR_ID_##_id,		\
		.type = REGULATOR_TYPE_LDO_##_type,		\
		.volt_mask = LDO_VOLT_MASK,			\
		.regs = {					\
			[VOLT_REG] = {				\
				.addr = MAX77663_REG_##_id##_CFG, \
			},					\
			[CFG_REG] = {				\
				.addr = MAX77663_REG_##_id##_CFG2, \
			},					\
			[FPS_REG] = {				\
				.addr = MAX77663_REG_FPS_##_id,	\
			},					\
		},						\
		.min_uV = _min_uV,				\
		.max_uV = _max_uV,				\
		.step_uV = _step_uV,				\
		.power_mode_mask = LDO_POWER_MODE_MASK,		\
		.power_mode_shift = LDO_POWER_MODE_SHIFT,	\
		.desc = {					\
			.name = max77663_rails(_id),		\
			.id = MAX77663_REGULATOR_ID_##_id,	\
			.ops = &max77663_ldo_ops,		\
			.type = REGULATOR_VOLTAGE,		\
			.owner = THIS_MODULE,			\
		},						\
	}

static struct max77663_regulator_info max77663_regs_info[MAX77663_REGULATOR_ID_NR] = {
	REGULATOR_SD(SD0,    SDX, SD0,  600000, 3387500, 12500),
	REGULATOR_SD(DVSSD0, SDX, NONE, 600000, 3387500, 12500),
	REGULATOR_SD(SD1,    SD1, SD1,  800000, 1587500, 12500),
	REGULATOR_SD(DVSSD1, SD1, NONE, 800000, 1587500, 12500),
	REGULATOR_SD(SD2,    SDX, SD2,  600000, 3387500, 12500),
	REGULATOR_SD(SD3,    SDX, SD3,  600000, 3387500, 12500),
	REGULATOR_SD(SD4,    SDX, SD4,  600000, 3387500, 12500),

	REGULATOR_LDO(LDO0, N, 800000, 2350000, 25000),
	REGULATOR_LDO(LDO1, N, 800000, 2350000, 25000),
	REGULATOR_LDO(LDO2, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO3, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO4, P, 800000, 1587500, 12500),
	REGULATOR_LDO(LDO5, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO6, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO7, N, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO8, N, 800000, 3950000, 50000),
};

static int max77663_regulator_probe(struct platform_device *pdev)
{
	struct max77663_platform_data *pdata =
					dev_get_platdata(pdev->dev.parent);
	struct regulator_desc *rdesc;
	struct max77663_regulator *reg;
	struct max77663_regulator *max_regs;
	struct max77663_regulator_platform_data *reg_pdata;
	struct regulator_config config = { };
	int ret = 0;
	int id;
	int reg_id;
	int reg_count;
	u8 val;

	if (!pdata) {
		dev_err(&pdev->dev, "No Platform data\n");
		return -ENODEV;
	}

	reg_count = pdata->num_regulator_pdata;
	max_regs = devm_kzalloc(&pdev->dev,
			reg_count * sizeof(*max_regs), GFP_KERNEL);
	if (!max_regs) {
		dev_err(&pdev->dev, "mem alloc for reg failed\n");
		return -ENOMEM;
	}

	ret = max77663_read_chip_version(pdev->dev.parent, &val);
	if (ret == MAX77663_DRV_24) {
		max77663_regs_info[MAX77663_REGULATOR_ID_SD0].volt_mask =
								SD0_VOLT_MASK;
		max77663_regs_info[MAX77663_REGULATOR_ID_SD0].min_uV = 800000;
		max77663_regs_info[MAX77663_REGULATOR_ID_SD0].max_uV = 1587500;
		max77663_regs_info[MAX77663_REGULATOR_ID_SD1].volt_mask =
								SDX_VOLT_MASK;
		max77663_regs_info[MAX77663_REGULATOR_ID_SD1].min_uV = 600000;
		max77663_regs_info[MAX77663_REGULATOR_ID_SD1].max_uV = 3387500;
	}

	for (id = 0; id < reg_count; ++id) {
		reg_pdata = pdata->regulator_pdata[id];
		if (!reg_pdata) {
			dev_err(&pdev->dev,
				"Regulator pltform data not there\n");
			goto clean_exit;
		}

		reg_id = reg_pdata->id;
		reg  = &max_regs[id];
		rdesc = &max77663_regs_info[reg_id].desc;
		reg->rinfo = &max77663_regs_info[reg_id];
		reg->dev = &pdev->dev;
		reg->pdata = reg_pdata;
		reg->regulator_mode = REGULATOR_MODE_NORMAL;
		reg->power_mode = POWER_MODE_NORMAL;

		dev_dbg(&pdev->dev, "probe: name=%s\n", rdesc->name);

		ret = max77663_regulator_preinit(reg);
		if (ret) {
			dev_err(&pdev->dev, "Failed to preinit regulator %s\n",
				rdesc->name);
			goto clean_exit;
		}

		config.dev = &pdev->dev;
		config.init_data = reg->pdata->reg_init_data;
		config.driver_data = reg;

		reg->rdev = regulator_register(rdesc, &config);
		if (IS_ERR(reg->rdev)) {
			dev_err(&pdev->dev, "Failed to register regulator %s\n",
			rdesc->name);
			ret = PTR_ERR(reg->rdev);
			goto clean_exit;
		}
	}
	platform_set_drvdata(pdev, max_regs);

	return 0;

clean_exit:
	while (--id >= 0) {
		reg  = &max_regs[id];
		regulator_unregister(reg->rdev);
	}
	return ret;
}

static int max77663_regulator_remove(struct platform_device *pdev)
{
	struct max77663_regulator *max_regs = platform_get_drvdata(pdev);
	struct max77663_regulator *reg;
	struct max77663_platform_data *pdata =
					dev_get_platdata(pdev->dev.parent);
	int reg_count;

	if (!pdata)
		return 0;

	reg_count = pdata->num_regulator_pdata;
	while (--reg_count >= 0) {
		reg  = &max_regs[reg_count];
		regulator_unregister(reg->rdev);
	}

	return 0;
}

static struct platform_driver max77663_regulator_driver = {
	.probe = max77663_regulator_probe,
	.remove = max77663_regulator_remove,
	.driver = {
		.name = "max77663-pmic",
		.owner = THIS_MODULE,
	},
};

static int __init max77663_regulator_init(void)
{
	return platform_driver_register(&max77663_regulator_driver);
}
subsys_initcall(max77663_regulator_init);

static void __exit max77663_reg_exit(void)
{
	platform_driver_unregister(&max77663_regulator_driver);
}
module_exit(max77663_reg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("max77663 regulator driver");
MODULE_VERSION("1.0");
