/*
 * drivers/regulator/max77660-regulator.c
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
#include <linux/mfd/max77660/max77660-core.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#define DVFS_BASE_VOLTAGE_UV	600000
#define DVFS_VOLTAGE_STEP_UV	6250

enum {
	VOLT_REG = 0,  /* XX_VOUT */
	CFG_REG,       /* XX_CNFG */
	FPS_REG,       /* XX_FPS */
	PWR_MODE_REG   /* XX_PWR_MODE */
};

struct max77660_register {
	u8 addr;
};

struct max77660_regulator_info {
	u8 id;
	u8 type;
	u32 min_uV;
	u32 max_uV;
	u32 step_uV;

	struct max77660_register regs[4]; /* volt, cfg, fps, pwrmode */

	struct regulator_desc desc;

	u8 volt_mask;

	u8 power_mode_mask;
	u8 power_mode_shift;
	int enable_bit;
};

struct max77660_regulator {
	struct max77660_regulator_info *rinfo;
	struct regulator_dev *rdev;
	struct device *dev;
	struct max77660_regulator_platform_data *pdata;
	u32 regulator_mode;
	u8 power_mode;
	enum max77660_regulator_fps_src fps_src;
	u8 val[4]; /* volt, cfg, fps, power mode */
	int safe_down_uV; /* for stable down scaling */
	int external_flags;
};

#define fps_src_name(fps_src)	\
	(fps_src == FPS_SRC_0 ? "FPS_SRC_0" :	\
	fps_src == FPS_SRC_1 ? "FPS_SRC_1" :	\
	fps_src == FPS_SRC_2 ? "FPS_SRC_2" :	\
	fps_src == FPS_SRC_3 ? "FPS_SRC_3" :	\
	fps_src == FPS_SRC_4 ? "FPS_SRC_4" :	\
	fps_src == FPS_SRC_5 ? "FPS_SRC_5" :	\
	fps_src == FPS_SRC_6 ? "FPS_SRC_6" : "FPS_SRC_NONE")

static struct max77660_register fps_cfg_regs[] = {
	{
		.addr = MAX77660_REG_CNFG_FPS_AP_OFF,
	},
	{
		.addr = MAX77660_REG_CNFG_FPS_AP_SLP,
	},
	{
		.addr = MAX77660_REG_CNFG_FPS_6,
	},
};

static inline struct device *to_max77660_chip(struct max77660_regulator *reg)
{
	return reg->dev->parent;
}

static int
max77660_regulator_set_fps(struct max77660_regulator *reg)
{
	struct max77660_regulator_platform_data *pdata = reg->pdata;
	struct max77660_regulator_info *rinfo = reg->rinfo;
	u8 val;
	u8 mask;
	int ret;

	if ((rinfo->regs[FPS_REG].addr == MAX77660_REG_FPS_NONE) ||
			(reg->fps_src ==  pdata->fps_src))
		return 0;

	/* FPS SRC setting */
	switch (pdata->fps_src) {
	case FPS_SRC_0:
	case FPS_SRC_1:
	case FPS_SRC_2:
	case FPS_SRC_3:
	case FPS_SRC_4:
	case FPS_SRC_5:
	case FPS_SRC_6:
		val = pdata->fps_src << MAX77660_FPS_SRC_SHIFT;
		mask = MAX77660_FPS_SRC_MASK;
		break;
	case FPS_SRC_NONE:
		val = MAX77660_FPS_SRC_MASK | MAX77660_FPS_PU_PERIOD_MASK |
					MAX77660_FPS_PD_PERIOD_MASK;
		mask = MAX77660_FPS_SRC_MASK | MAX77660_FPS_PU_PERIOD_MASK |
					MAX77660_FPS_PD_PERIOD_MASK;
		goto reg_update;
	case FPS_SRC_DEF:
		return 0;
	default:
		return -EINVAL;
	}

	/* FPS power up period setting */
	if (pdata->fps_pu_period != FPS_POWER_PERIOD_DEF) {
		val |= (pdata->fps_pu_period << MAX77660_FPS_PU_PERIOD_SHIFT);
		mask |= MAX77660_FPS_PU_PERIOD_MASK;
	}

	/* FPS power down period setting */
	if (pdata->fps_pd_period != FPS_POWER_PERIOD_DEF) {
		val |= (pdata->fps_pd_period << MAX77660_FPS_PD_PERIOD_SHIFT);
		mask |= MAX77660_FPS_PD_PERIOD_MASK;
	}

reg_update:
	ret = max77660_reg_update(to_max77660_chip(reg), MAX77660_PWR_SLAVE,
					rinfo->regs[FPS_REG].addr, val,
					mask);
	if (ret < 0)
		return ret;

	reg->fps_src = pdata->fps_src;
	return 0;
}

static int
max77660_regulator_set_fps_cfg(struct max77660_regulator *reg,
			       struct max77660_regulator_fps_cfg *fps_cfg)
{
	u8 val = 0, mask = 0;
	int ret = 0;

	if ((reg->fps_src < FPS_SRC_0) || (reg->fps_src >= FPS_SRC_NONE))
		return -EINVAL;

	if ((reg->fps_src >= FPS_SRC_0) && (reg->fps_src <= FPS_SRC_5))	{
		if (fps_cfg->tu_ap_off != FPS_TIME_PERIOD_DEF) {
			val |= (fps_cfg->tu_ap_off <<
						MAX77660_FPS_AP_OFF_TU_SHIFT);
			mask |= MAX77660_FPS_AP_OFF_TU_MASK;
		}

		if (fps_cfg->td_ap_off != FPS_TIME_PERIOD_DEF) {
			val |= (fps_cfg->tu_ap_off <<
						MAX77660_FPS_AP_OFF_TD_SHIFT);
			mask |= MAX77660_FPS_AP_OFF_TD_MASK;
		}

		ret = max77660_reg_update(to_max77660_chip(reg),
						MAX77660_PWR_SLAVE,
						fps_cfg_regs[0].addr, val,
						mask);
		mask = 0;
		val = 0;
		if (fps_cfg->tu_ap_slp != FPS_TIME_PERIOD_DEF) {
			val |= (fps_cfg->tu_ap_off <<
						MAX77660_FPS_AP_SLP_TU_SHIFT);
			mask |= MAX77660_FPS_AP_SLP_TU_MASK;
		}

		if (fps_cfg->td_ap_slp != FPS_TIME_PERIOD_DEF) {
			val |= (fps_cfg->tu_ap_off <<
						MAX77660_FPS_AP_SLP_TD_SHIFT);
			mask |= MAX77660_FPS_AP_SLP_TD_MASK;
		}

		ret = max77660_reg_update(to_max77660_chip(reg),
						MAX77660_PWR_SLAVE,
						fps_cfg_regs[1].addr, val,
						mask);
	}

	if (reg->fps_src == FPS_SRC_6) {
		if (fps_cfg->tu_fps_6 != FPS_TIME_PERIOD_DEF) {
			val |= (fps_cfg->tu_fps_6 << MAX77660_FPS_6_TU_SHIFT);
			mask |= MAX77660_FPS_6_TU_MASK;
		}

		if (fps_cfg->tu_fps_6 != FPS_TIME_PERIOD_DEF) {
			val |= (fps_cfg->td_ap_off << MAX77660_FPS_6_TD_SHIFT);
			mask |= MAX77660_FPS_AP_SLP_TD_MASK;
		}

		ret = max77660_reg_update(to_max77660_chip(reg),
						MAX77660_PWR_SLAVE,
						fps_cfg_regs[2].addr, val,
						mask);
	}

	return ret;
}

static int
max77660_regulator_set_fps_cfgs(struct max77660_regulator *reg,
				struct max77660_regulator_fps_cfg *fps_cfgs,
				int num_fps_cfgs)
{
	int i, ret;
	static int fps_cfg_init;

	if (fps_cfg_init)
		return 0;

	for (i = 0; i < num_fps_cfgs; i++) {
		ret = max77660_regulator_set_fps_cfg(reg, &fps_cfgs[i]);
		if (ret < 0)
			return ret;
	}

	fps_cfg_init = 1;

	return 0;
}

static int
max77660_regulator_set_power_mode(struct max77660_regulator *reg, u8 power_mode)
{
	struct max77660_regulator_info *rinfo = reg->rinfo;
	u8 mask = rinfo->power_mode_mask;
	u8 shift = rinfo->power_mode_shift;
	int ret = 0;

	if (rinfo->type == REGULATOR_TYPE_SW)
		return 0;
	ret = max77660_reg_update(to_max77660_chip(reg), MAX77660_PWR_SLAVE,
					rinfo->regs[PWR_MODE_REG].addr,
					power_mode << shift, mask);
	if (ret < 0)
		return ret;
	reg->power_mode = power_mode;
	return ret;
}

static u8 max77660_regulator_get_power_mode(struct max77660_regulator *reg)
{
	struct max77660_regulator_info *rinfo = reg->rinfo;
	u8 mask = rinfo->power_mode_mask;
	u8 shift = rinfo->power_mode_shift;
	int ret;
	u8 val;

	if (rinfo->type == REGULATOR_TYPE_SW)
		return 0;

	ret = max77660_reg_read(to_max77660_chip(reg), MAX77660_PWR_SLAVE,
				rinfo->regs[PWR_MODE_REG].addr, &val);
	if (ret < 0)
		return ret;

	reg->power_mode = (val & mask) >> shift;
	return reg->power_mode;
}

static int max77660_regulator_do_set_voltage(struct max77660_regulator *reg,
					     int min_uV, int max_uV)
{
	struct max77660_regulator_info *rinfo = reg->rinfo;
	u8 addr = rinfo->regs[VOLT_REG].addr;
	u8 mask = rinfo->volt_mask;
	u8 val;
	int old_uV, new_uV, safe_uV;
	int i, steps = 1;
	int ret = 0;

	if (min_uV < rinfo->min_uV || max_uV > rinfo->max_uV)
		return -EDOM;

	ret = max77660_reg_read(to_max77660_chip(reg), MAX77660_PWR_SLAVE,
					addr, &val);
	if (ret < 0)
		return ret;

	old_uV = (val & mask) * rinfo->step_uV + rinfo->min_uV;

	if ((old_uV > min_uV) && (reg->safe_down_uV >= rinfo->step_uV)) {
		steps = DIV_ROUND_UP(old_uV - min_uV, reg->safe_down_uV);
		safe_uV = -reg->safe_down_uV;
	}

	if (steps == 1) {
		val = (min_uV - rinfo->min_uV) / rinfo->step_uV;
		ret = max77660_reg_update(to_max77660_chip(reg),
						MAX77660_PWR_SLAVE,
						addr, val, mask);
	} else {
		for (i = 0; i < steps; i++) {
			if (abs(min_uV - old_uV) > abs(safe_uV))
				new_uV = old_uV + safe_uV;
			else
				new_uV = min_uV;

			dev_dbg(&reg->rdev->dev, "do_set_voltage: name=%s, %d/%d, old_uV=%d, new_uV=%d\n",
				reg->rdev->desc->name, i + 1, steps, old_uV,
				new_uV);

			val = (new_uV - rinfo->min_uV) / rinfo->step_uV;
			ret = max77660_reg_update(to_max77660_chip(reg),
							MAX77660_PWR_SLAVE,
							addr, val, mask);
			if (ret < 0)
				return ret;

			old_uV = new_uV;
		}
	}

	return ret;
}

static int max77660_regulator_set_voltage(struct regulator_dev *rdev,
					  int min_uV, int max_uV,
					  unsigned *selector)
{
	struct max77660_regulator *reg = rdev_get_drvdata(rdev);

	dev_dbg(&rdev->dev, "set_voltage: name=%s, min_uV=%d, max_uV=%d\n",
		rdev->desc->name, min_uV, max_uV);
	return max77660_regulator_do_set_voltage(reg, min_uV, max_uV);
}

static int max77660_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct max77660_regulator *reg = rdev_get_drvdata(rdev);
	struct max77660_regulator_info *rinfo = reg->rinfo;
	int volt, ret;
	u8 val;

	ret = max77660_reg_read(to_max77660_chip(reg), MAX77660_PWR_SLAVE,
					rinfo->regs[VOLT_REG].addr, &val);
	if (ret < 0)
		return ret;

	volt  = (val & rinfo->volt_mask) * rinfo->step_uV + rinfo->min_uV;
	dev_dbg(&rdev->dev, "get_voltage: name=%s, volt=%d, val=0x%02x\n",
		rdev->desc->name, volt, val);
	return volt;
}

static int max77660_regulator_mask_ext_control(struct device *dev)
{
	u8 mask;
	int ret;

	mask = GLBLCNFG5_EN1_MASK_MASK | GLBLCNFG5_EN5_MASK_MASK |
			GLBLCNFG5_EN1_FPS6_MASK_MASK;
	ret = max77660_reg_update(dev->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_GLOBAL_CFG5, mask, mask);
	if (ret < 0) {
		dev_err(dev, "GLOBAL_CFG5 update failed: %d\n", ret);
		return ret;
	}

	mask = GLBLCNFG7_EN4_MASK_MASK | GLBLCNFG7_EN3_MASK_MASK |
			GLBLCNFG7_EN2_MASK_MASK;
	ret = max77660_reg_update(dev->parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_GLOBAL_CFG7, mask, mask);
	if (ret < 0) {
		dev_err(dev, "GLOBAL_CFG7 update failed: %d\n", ret);
		return ret;
	}
	return 0;
}

static int max77660_regulator_unmask_ext_control(struct device *dev,
	unsigned ext_control)
{
	u8 mask_cfg7 = 0;
	u8 mask_cfg5 = 0;
	int ret;

	if ((ext_control & MAX77660_EXT_ENABLE_EN1) ||
			(ext_control & MAX77660_EXT_ENABLE_EN2)) {
		mask_cfg5 |= GLBLCNFG5_EN1_MASK_MASK;
		mask_cfg7 |= GLBLCNFG7_EN2_MASK_MASK;
	}

	if (ext_control & MAX77660_EXT_ENABLE_EN3)
		mask_cfg7 |= GLBLCNFG7_EN3_MASK_MASK;

	if (ext_control & MAX77660_EXT_ENABLE_EN4)
		mask_cfg7 |= GLBLCNFG7_EN4_MASK_MASK;

	if (ext_control & MAX77660_EXT_ENABLE_EN5)
		mask_cfg5 |= GLBLCNFG5_EN5_MASK_MASK;

	if (ext_control & MAX77660_EXT_ENABLE_EN1FPS6)
		mask_cfg5 |= GLBLCNFG5_EN1_FPS6_MASK_MASK;

	if (mask_cfg5) {
		ret = max77660_reg_update(dev->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_GLOBAL_CFG5, 0, mask_cfg5);
		if (ret < 0) {
			dev_err(dev, "GLOBAL_CFG5 update failed: %d\n", ret);
			return ret;
		}
	}

	if (mask_cfg7) {
		ret = max77660_reg_update(dev->parent, MAX77660_PWR_SLAVE,
				MAX77660_REG_GLOBAL_CFG7, 0, mask_cfg7);
		if (ret < 0) {
			dev_err(dev, "GLOBAL_CFG7 update failed: %d\n", ret);
			return ret;
		}
	}
	return 0;
}

static int max77660_regulator_enable(struct regulator_dev *rdev)
{
	struct max77660_regulator *reg = rdev_get_drvdata(rdev);
	struct max77660_regulator_info *rinfo = reg->rinfo;
	struct max77660_regulator_platform_data *pdata = reg->pdata;
	int power_mode = (pdata->flags & GLPM_ENABLE) ?
			 POWER_MODE_GLPM : POWER_MODE_NORMAL;

	if (reg->external_flags & MAX77660_EXTERNAL_ENABLE)
		return 0;

	/* ES 1.1 suggest to keep BUCK3 and BUCK5 in GLPM */
	if (max77660_is_es_1_1(reg->dev))
		if (reg->rinfo->id == MAX77660_REGULATOR_ID_BUCK3 ||
			reg->rinfo->id == MAX77660_REGULATOR_ID_BUCK5)
			max77660_regulator_set_power_mode(reg,
					POWER_MODE_GLPM);

	if (reg->fps_src != FPS_SRC_NONE) {
		dev_dbg(&rdev->dev, "enable: Regulator %s using %s\n",
			rdev->desc->name, fps_src_name(reg->fps_src));
		return 0;
	}

	/* N-Channel LDOs don't support Low-Power mode. */
	if ((rinfo->type != REGULATOR_TYPE_LDO_N) &&
			(reg->regulator_mode == REGULATOR_MODE_STANDBY))
		power_mode = POWER_MODE_LPM;

	return max77660_regulator_set_power_mode(reg, power_mode);
}

static int max77660_regulator_disable(struct regulator_dev *rdev)
{
	struct max77660_regulator *reg = rdev_get_drvdata(rdev);
	int power_mode = POWER_MODE_DISABLE;

	if (reg->external_flags & MAX77660_EXTERNAL_ENABLE)
		return 0;

	if (reg->fps_src != FPS_SRC_NONE) {
		dev_dbg(&rdev->dev, "disable: Regulator %s using %s\n",
			rdev->desc->name, fps_src_name(reg->fps_src));
		return 0;
	}

	return max77660_regulator_set_power_mode(reg, power_mode);
}

static int max77660_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct max77660_regulator *reg = rdev_get_drvdata(rdev);
	int ret = 1;

	if (reg->external_flags & MAX77660_EXTERNAL_ENABLE)
		return 1;

	if (reg->fps_src != FPS_SRC_NONE) {
		dev_dbg(&rdev->dev, "is_enable: Regulator %s using %s\n",
			rdev->desc->name, fps_src_name(reg->fps_src));
		return 1;
	}

	if (max77660_regulator_get_power_mode(reg) == POWER_MODE_DISABLE)
		ret = 0;

	return ret;
}

static int max77660_regulator_set_mode(struct regulator_dev *rdev,
				       unsigned int mode)
{
	struct max77660_regulator *reg = rdev_get_drvdata(rdev);
	struct max77660_regulator_platform_data *pdata = reg->pdata;
	struct max77660_regulator_info *rinfo = reg->rinfo;
	u8 power_mode;
	u8 mask;
	u8 val = 0;
	int ret;

	if ((mode == REGULATOR_MODE_NORMAL) ||
			(mode == REGULATOR_MODE_FAST))
		power_mode = (pdata->flags & GLPM_ENABLE) ?
			     POWER_MODE_GLPM : POWER_MODE_NORMAL;
	else if (mode == REGULATOR_MODE_STANDBY) {
		/* N-Channel LDOs don't support Low-Power mode. */
		power_mode = (rinfo->type != REGULATOR_TYPE_LDO_N) ?
			     POWER_MODE_LPM : POWER_MODE_NORMAL;
	} else
		return -EINVAL;

	if (reg->rinfo->id == MAX77660_REGULATOR_ID_BUCK6 ||
			reg->rinfo->id == MAX77660_REGULATOR_ID_BUCK7) {
		mask = MAX77660_BUCK6_7_CNFG_FPWM_MASK;
		switch (mode) {
		case REGULATOR_MODE_FAST:
			val = 0;
			break;
		case REGULATOR_MODE_NORMAL:
			val = 1 << MAX77660_BUCK6_7_CNFG_FPWM_SHIFT;
			break;
		}

		ret = max77660_reg_update(to_max77660_chip(reg),
				MAX77660_PWR_SLAVE, rinfo->regs[CFG_REG].addr,
				mask, val);
		if (ret < 0)
			return ret;
	}

	ret = max77660_regulator_set_power_mode(reg, power_mode);
	if (!ret)
		reg->regulator_mode = mode;

	return ret;
}

static unsigned int max77660_regulator_get_mode(struct regulator_dev *rdev)
{
	struct max77660_regulator *reg = rdev_get_drvdata(rdev);

	return reg->regulator_mode;
}

static int max77660_switch_enable(struct regulator_dev *rdev)
{
	struct max77660_regulator *reg = rdev_get_drvdata(rdev);
	struct max77660_regulator_info *rinfo = reg->rinfo;
	int idx, ret;

	if (rinfo->id == MAX77660_REGULATOR_ID_SW4)
		return 0;

	idx = rinfo->enable_bit;
	ret = max77660_reg_set_bits(to_max77660_chip(reg), MAX77660_PWR_SLAVE,
					rinfo->regs[VOLT_REG].addr, 1 << idx);
	return ret;
}

static int max77660_switch_disable(struct regulator_dev *rdev)
{
	struct max77660_regulator *reg = rdev_get_drvdata(rdev);
	struct max77660_regulator_info *rinfo = reg->rinfo;
	int idx, ret;

	if (rinfo->id == MAX77660_REGULATOR_ID_SW4)
		return 0;

	idx = rinfo->enable_bit;
	ret = max77660_reg_clr_bits(to_max77660_chip(reg), MAX77660_PWR_SLAVE,
					rinfo->regs[VOLT_REG].addr, 1 << idx);
	return ret;
}

static int max77660_switch_is_enabled(struct regulator_dev *rdev)
{
	struct max77660_regulator *reg = rdev_get_drvdata(rdev);
	struct max77660_regulator_info *rinfo = reg->rinfo;
	int idx, ret;
	u8 val = 0;

	if (rinfo->id == MAX77660_REGULATOR_ID_SW4)
		return 1;

	idx = rinfo->enable_bit;
	ret = max77660_reg_read(to_max77660_chip(reg), MAX77660_PWR_SLAVE,
					rinfo->regs[VOLT_REG].addr, &val);

	return !!(val & (1 << idx));
}

static int max77660_reg_enable_time(struct regulator_dev *dev)
{
	return 500;
}

static struct regulator_ops max77660_regulator_ops = {
	.set_voltage = max77660_regulator_set_voltage,
	.get_voltage = max77660_regulator_get_voltage,
	.enable_time = max77660_reg_enable_time,
	.enable = max77660_regulator_enable,
	.disable = max77660_regulator_disable,
	.is_enabled = max77660_regulator_is_enabled,
	.set_mode = max77660_regulator_set_mode,
	.get_mode = max77660_regulator_get_mode,
};

static struct regulator_ops max77660_sw_ops = {
	.enable = max77660_switch_enable,
	.disable = max77660_switch_disable,
	.enable_time = max77660_reg_enable_time,
	.is_enabled = max77660_switch_is_enabled,
};

static int max77660_regulator_preinit(struct max77660_regulator *reg)
{
	struct max77660_regulator_platform_data *pdata = reg->pdata;
	struct max77660_regulator_info *rinfo = reg->rinfo;
	u8 idx;
	u8 val, mask;
	int ret;
	int addr;

	/* Unmask extern control */
	if (pdata->flags & MAX77660_EXTERNAL_ENABLE) {
		ret = max77660_regulator_unmask_ext_control(reg->dev,
				pdata->flags);
		if (ret < 0) {
			dev_err(reg->dev, "Unmasking ext control failed: %d\n",
				ret);
			return ret;
		}
	}

	/* Update Power Mode register mask and offset */
	if (rinfo->type == REGULATOR_TYPE_BUCK) {

		idx = reg->rinfo->id - MAX77660_REGULATOR_ID_BUCK1;
		/*  4 Bucks Pwr Mode in 1 register */
		reg->rinfo->regs[PWR_MODE_REG].addr =
				MAX77660_REG_BUCK_PWR_MODE1 + (idx/4);
		reg->rinfo->power_mode_shift = (idx%4)*2 ;
		reg->rinfo->power_mode_mask = reg->rinfo->power_mode_mask <<
						(reg->rinfo->power_mode_shift);
	}

	if ((rinfo->type == REGULATOR_TYPE_LDO_N) ||
			(rinfo->type == REGULATOR_TYPE_LDO_P)) {

		idx = reg->rinfo->id - MAX77660_REGULATOR_ID_LDO1;
		/* 4 LDOs Pwr Mode in 1 register  */
		reg->rinfo->regs[PWR_MODE_REG].addr =
				MAX77660_REG_LDO_PWR_MODE1 + + (idx/4);
		reg->rinfo->power_mode_shift = (idx%4)*2 ;
		reg->rinfo->power_mode_mask = reg->rinfo->power_mode_mask <<
						(reg->rinfo->power_mode_shift);
	}

		/* Update FPS source */
	if (rinfo->regs[FPS_REG].addr == MAX77660_REG_FPS_NONE)
		reg->fps_src = FPS_SRC_NONE;
	else
		reg->fps_src = (reg->val[FPS_REG] & MAX77660_FPS_SRC_MASK) >>
					MAX77660_FPS_SRC_SHIFT;

	dev_dbg(reg->dev, "preinit: initial fps_src=%s\n",
		fps_src_name(reg->fps_src));

	/* Update power mode */
	max77660_regulator_get_power_mode(reg);

	/* Check Chip Identification */
	ret = max77660_reg_read(to_max77660_chip(reg), MAX77660_PWR_SLAVE,
					MAX77660_REG_CID5, &val);
	if (ret < 0) {
		dev_err(reg->dev, "preinit: Failed to get register 0x%x\n",
			MAX77660_REG_CID5);
		return ret;
	}

	/* Set FPS */
	ret = max77660_regulator_set_fps_cfgs(reg, pdata->fps_cfgs,
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
		ret = max77660_regulator_set_power_mode(reg, val);
		if (ret < 0) {
			dev_err(reg->dev, "preinit: Failed to set power mode to POWER_MODE_NORMAL\n");
			return ret;
		}
	}

	if (reg->rinfo->id == MAX77660_REGULATOR_ID_LDO15 ||
		reg->rinfo->id == MAX77660_REGULATOR_ID_LDO16) {
		mask = 0;
		addr = (reg->rinfo->id == MAX77660_REGULATOR_ID_LDO15) ?
			MAX77660_REG_SIM_SIM1CNFG1 :
			MAX77660_REG_SIM_SIM2CNFG1;

		ret = max77660_reg_read(to_max77660_chip(reg),
				MAX77660_PWR_SLAVE,
				addr, &val);

		if (ret < 0) {
			dev_err(reg->dev, "preinit: Failed to get register 0x%x\n",
				addr);
		}
		mask |= SIM_SIM1_2_CNFG1_BATREM_EN_MASK |
			SIM_SIM1_2_CNFG1_SIM1DBCNT_MASK;
		val &= ~(SIM_SIM1_2_CNFG1_SIM1DBCNT_MASK);
		val |= SIM_SIM1_2_DBCNT;
		/* FIXME: if BAT remove is considered */
		val &= ~(1 << SIM_SIM1_2_CNFG1_BATREM_EN_SHIFT);

		max77660_reg_update(to_max77660_chip(reg),
				MAX77660_PWR_SLAVE,
				addr, val, mask);

		max77660_regulator_set_power_mode(reg,
				POWER_MODE_NORMAL);
	}


	ret = max77660_regulator_set_fps(reg);
	if (ret < 0) {
		dev_err(reg->dev, "preinit: Failed to set FPS\n");
		return ret;
	}

	/*
	* ES 1.0 errata suggest to keep BUCK3 and BUCK5 in FPWM mode
	*/
	if (max77660_is_es_1_0(reg->dev))
		if (reg->rinfo->id == MAX77660_REGULATOR_ID_BUCK3 ||
			reg->rinfo->id == MAX77660_REGULATOR_ID_BUCK5)
			pdata->flags |= SD_FORCED_PWM_MODE;

	if (rinfo->type == REGULATOR_TYPE_BUCK) {
		val = 0;
		mask = 0;
		if ((reg->rinfo->id >= MAX77660_REGULATOR_ID_BUCK1) &&
			(reg->rinfo->id <= MAX77660_REGULATOR_ID_BUCK5)) {
			mask |= MAX77660_BUCK1_5_CNFG_FPWM_MASK;
			if (pdata->flags & SD_FORCED_PWM_MODE)
				val |= MAX77660_BUCK1_5_CNFG_FPWM_MASK;

			mask |= MAX77660_BUCK1_5_CNFG_FSRADE_MASK;
			if (pdata->flags & SD_FSRADE_DISABLE)
				val |= MAX77660_BUCK1_5_CNFG_FSRADE_MASK;

			mask |= MAX77660_BUCK1_5_CNFG_DVFS_EN_MASK;
			if (pdata->flags & DISABLE_DVFS)
				val &= ~MAX77660_BUCK1_5_CNFG_DVFS_EN_MASK;
		} else if ((reg->rinfo->id >= MAX77660_REGULATOR_ID_BUCK6) &&
			(reg->rinfo->id <= MAX77660_REGULATOR_ID_BUCK7)) {
			mask |= MAX77660_BUCK6_7_CNFG_FPWM_MASK;
			/* ES 1.1 suggest to remove all BUCKS from FPWM */
			if ((pdata->flags & SD_FORCED_PWM_MODE) &&
					!(max77660_is_es_1_1(reg->dev)))
				val |= MAX77660_BUCK6_7_CNFG_FPWM_MASK;
		}

		ret = max77660_reg_update(to_max77660_chip(reg),
						MAX77660_PWR_SLAVE,
						rinfo->regs[CFG_REG].addr, val,
						mask);
		if (ret < 0) {
			dev_err(reg->dev, "%s:Failed to set register 0x%x\n",
				__func__, rinfo->regs[CFG_REG].addr);
			return ret;
		}
	}
	return 0;
}

#define REGULATOR_BUCK(_id, _volt_mask, _fps_reg, _min_uV, _max_uV, _step_uV)	\
	[MAX77660_REGULATOR_ID_##_id] = {					\
		.id = MAX77660_REGULATOR_ID_##_id,				\
		.type = REGULATOR_TYPE_BUCK,					\
		.volt_mask = MAX77660_##_volt_mask##_VOLT_MASK,			\
		.regs = {							\
			[VOLT_REG] = {						\
				.addr = MAX77660_REG_##_id##_VOUT,		\
			},							\
			[CFG_REG] = {						\
				.addr = MAX77660_REG_##_id##_CNFG,		\
			},							\
			[FPS_REG] = {						\
				.addr = MAX77660_REG_FPS_##_id, 		\
			},							\
		},								\
		.min_uV = _min_uV,						\
		.max_uV = _max_uV,						\
		.step_uV = _step_uV,						\
		.power_mode_mask = MAX77660_BUCK_POWER_MODE_MASK,		\
		.power_mode_shift = MAX77660_BUCK_POWER_MODE_SHIFT,		\
		.desc = {							\
			.name = max77660_rails(_id),				\
			.id = MAX77660_REGULATOR_ID_##_id,			\
			.ops = &max77660_regulator_ops,				\
			.type = REGULATOR_VOLTAGE,				\
			.owner = THIS_MODULE,					\
		},								\
	}

#define REGULATOR_LDO(_id, _type, _min_uV, _max_uV, _step_uV)			\
	[MAX77660_REGULATOR_ID_##_id] = {					\
		.id = MAX77660_REGULATOR_ID_##_id,				\
		.type = REGULATOR_TYPE_LDO_##_type,				\
		.volt_mask = MAX77660_LDO_VOLT_MASK,				\
		.regs = {							\
			[VOLT_REG] = {						\
				.addr = MAX77660_REG_##_id##_CNFG, 		\
			},							\
			[CFG_REG] = {						\
				.addr = MAX77660_REG_##_id##_CNFG,		\
			},							\
			[FPS_REG] = {						\
				.addr = MAX77660_REG_FPS_##_id,			\
			},							\
		},								\
		.min_uV = _min_uV,						\
		.max_uV = _max_uV,						\
		.step_uV = _step_uV,						\
		.power_mode_mask = MAX77660_LDO_POWER_MODE_MASK,		\
		.power_mode_shift = MAX77660_LDO_POWER_MODE_SHIFT,		\
		.desc = {							\
			.name = max77660_rails(_id),				\
			.id = MAX77660_REGULATOR_ID_##_id,			\
			.ops = &max77660_regulator_ops,				\
			.type = REGULATOR_VOLTAGE,				\
			.owner = THIS_MODULE,					\
		},								\
	}

#define REGULATOR_SW(_id, _enable_bit)				\
	[MAX77660_REGULATOR_ID_##_id] = {			\
		.id = MAX77660_REGULATOR_ID_##_id,		\
		.type = REGULATOR_TYPE_SW,			\
		.volt_mask = 0,					\
		.enable_bit = _enable_bit,			\
		.regs = { 					\
			[VOLT_REG] = { 				\
				.addr = MAX77660_REG_SW_EN,	\
			},					\
			[CFG_REG] = {				\
				.addr = MAX77660_REG_##_id##_CNFG, \
			},					\
			[FPS_REG] = {				\
				.addr = MAX77660_REG_FPS_NONE,	\
			},					\
		},						\
		.desc = {					\
			.name = max77660_rails(_id),		\
			.id = MAX77660_REGULATOR_ID_##_id,	\
			.ops = &max77660_sw_ops,		\
			.type = REGULATOR_VOLTAGE,		\
			.owner = THIS_MODULE,			\
		},						\
	}

static struct max77660_regulator_info
	max77660_regs_info[MAX77660_REGULATOR_ID_NR] = {
	REGULATOR_BUCK(BUCK1, SDX, BUCK1,  600000, 1500000, 6250),
	REGULATOR_BUCK(BUCK2, SDX, BUCK2,  600000, 1500000, 6250),
	REGULATOR_BUCK(BUCK3, SDX, BUCK3,  600000, 3787500, 12500),
	REGULATOR_BUCK(BUCK4, SDX, BUCK4,  600000, 1500000, 6250),
	REGULATOR_BUCK(BUCK5, SDX, BUCK5,  600000, 3787500, 12500),
	REGULATOR_BUCK(BUCK6, SD1, BUCK6, 1000000, 4150000, 50000),
	REGULATOR_BUCK(BUCK7, SD1, BUCK7, 1000000, 4150000, 50000),

	REGULATOR_LDO(LDO1, N, 600000, 2175000, 25000),
	REGULATOR_LDO(LDO2, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO3, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO4, P, 800000, 3950000, 12500),
	REGULATOR_LDO(LDO5, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO6, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO7, N, 600000, 2175000, 25000),
	REGULATOR_LDO(LDO8, N, 600000, 2175000, 25000),
	REGULATOR_LDO(LDO9, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO10, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO11, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO12, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO13, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO14, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO15, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO16, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO17, P, 800000, 3950000, 50000),
	REGULATOR_LDO(LDO18, P, 800000, 3950000, 50000),

	REGULATOR_SW(SW1, 0),
	REGULATOR_SW(SW2, 1),
	REGULATOR_SW(SW3, 2),
	REGULATOR_SW(SW4, -1),
	REGULATOR_SW(SW5, 3),
};

static ssize_t max77660_show_dvfs_data(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct max77660_platform_data *pdata =
					dev_get_platdata(dev->parent);
	struct max77660_pwm_dvfs_init_data *dvfs_pd = &pdata->dvfs_pd;
	int count = 0;

	if (!dvfs_pd->en_pwm)
		return 0;

	count += sprintf(buf+count, "base_voltage:%d\n",
			dvfs_pd->base_voltage_uV);
	count += sprintf(buf+count, "step_size:%d\n",
			dvfs_pd->step_voltage_uV);
	count += sprintf(buf+count, "max_voltage:%d\n",
			dvfs_pd->max_voltage_uV);
	count += sprintf(buf+count, "default_voltage:%d\n",
			dvfs_pd->default_voltage_uV);

	return count;
}
static DEVICE_ATTR(dvfs_data, 0444, max77660_show_dvfs_data, NULL);

static int max77660_pwm_dvfs_init(struct device *max77660_pmic_dev,
					struct max77660_platform_data *pdata)
{
	u8 val = 0;
	int ret;
	struct device *parent = max77660_pmic_dev->parent;
	struct max77660_pwm_dvfs_init_data *dvfs_pd = &pdata->dvfs_pd;

	if (!dvfs_pd->en_pwm)
		return 0;

	ret = max77660_reg_update(parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_BUCK4_CNFG,
			1 << MAX77660_BUCK4_DVFS_EN_SHIFT,
			MAX77660_BUCK4_DVFS_EN_MASK);
	if (ret < 0)
		return ret;

	/* VSR gets reset after DVFS_EN changes from 0 to 1 */
	val = DIV_ROUND_UP((dvfs_pd->default_voltage_uV - DVFS_BASE_VOLTAGE_UV),
			DVFS_VOLTAGE_STEP_UV);
	ret = max77660_reg_write(parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_BUCK4_VSR, val);
	if (ret < 0)
		return ret;

	val = (1 << MAX77660_BUCK4_DVFS_PWMEN_SHIFT);
	switch (dvfs_pd->step_voltage_uV) {
	case 12500:
		val |= 0x1;
		break;
	case 25000:
		val |= 0x2;
		break;
	}

	ret = max77660_reg_write(parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_BUCK4_DVFS_CNFG, val);
	if (ret < 0)
		return ret;

	val = DIV_ROUND_UP((dvfs_pd->base_voltage_uV - DVFS_BASE_VOLTAGE_UV),
			DVFS_VOLTAGE_STEP_UV);
	ret = max77660_reg_write(parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_BUCK4_VBR, val);
	if (ret < 0)
		return ret;

	val = DIV_ROUND_UP((dvfs_pd->max_voltage_uV - DVFS_BASE_VOLTAGE_UV),
			DVFS_VOLTAGE_STEP_UV);
	ret = max77660_reg_write(parent, MAX77660_PWR_SLAVE,
			MAX77660_REG_BUCK4_MVR, val);
	if (ret < 0)
		return ret;

	ret = device_create_file(max77660_pmic_dev, &dev_attr_dvfs_data);
	if (ret)
		dev_warn(max77660_pmic_dev,
				"Can't register dvfs sysfs attribute\n");

	ret = sysfs_create_link(kernel_kobj, &(max77660_pmic_dev->kobj),
				"pmic");
	if (ret)
		dev_warn(max77660_pmic_dev, "Can't create sysfs link\n");

	return ret;
}

static int max77660_regulator_probe(struct platform_device *pdev)
{
	struct max77660_platform_data *pdata =
					dev_get_platdata(pdev->dev.parent);
	struct regulator_desc *rdesc;
	struct max77660_regulator *reg;
	struct max77660_regulator *max_regs;
	struct regulator_config config = { };
	int ret = 0;
	int id;
	int reg_id;

	if (!pdata) {
		dev_err(&pdev->dev, "No Platform data\n");
		return -ENODEV;
	}

	max_regs = devm_kzalloc(&pdev->dev,
			MAX77660_REGULATOR_ID_NR * sizeof(*max_regs), GFP_KERNEL);
	if (!max_regs) {
		dev_err(&pdev->dev, "mem alloc for reg failed\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, max_regs);

	ret = max77660_regulator_mask_ext_control(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Masking ext control failed, %d\n", ret);
		return ret;
	}

	for (id = 0; id < MAX77660_REGULATOR_ID_NR; ++id) {
		struct max77660_regulator_platform_data *reg_pdata;
		struct regulator_init_data *reg_init_data = NULL;

		reg_pdata = pdata->regulator_pdata[id];

		reg_id = id;
		reg  = &max_regs[id];
		rdesc = &max77660_regs_info[reg_id].desc;
		reg->rinfo = &max77660_regs_info[reg_id];
		reg->dev = &pdev->dev;
		reg->pdata = reg_pdata;
		if (reg_pdata)
			reg_init_data = reg_pdata->reg_init_data;

		reg->regulator_mode = REGULATOR_MODE_NORMAL;
		reg->power_mode = POWER_MODE_NORMAL;

		dev_dbg(&pdev->dev, "probe: name=%s\n", rdesc->name);

		if (reg_pdata) {
			ret = max77660_regulator_preinit(reg);
			if (ret < 0) {
				dev_err(&pdev->dev,
					"Preinit regulator %s failed: %d\n",
					rdesc->name, ret);
				goto clean_exit;
			}
		}

		/* ES1.0 errata: Clear active discharge for LDO1 */
		if (max77660_is_es_1_0(&pdev->dev) &&
			(id == MAX77660_REGULATOR_ID_LDO1)) {
			ret = max77660_reg_clr_bits(to_max77660_chip(reg),
				MAX77660_PWR_SLAVE, MAX77660_REG_LDO1_CNFG,
				MAX77660_LDO1_18_CNFG_ADE_MASK);
			if (ret < 0) {
				dev_err(&pdev->dev,
					"LDO1_CNFG update failed: %d\n", ret);
				goto clean_exit;
			}
		}

		config.dev = &pdev->dev;
		config.init_data = reg_init_data;
		config.driver_data = reg;

		reg->rdev = regulator_register(rdesc, &config);
		if (IS_ERR(reg->rdev)) {
			ret = PTR_ERR(reg->rdev);
			dev_err(&pdev->dev,
				"regulator %s register failed: %d\n",
				rdesc->name, ret);
			goto clean_exit;
		}

		if (reg_pdata &&
			(reg_pdata->flags & MAX77660_EXTERNAL_ENABLE)) {
			reg->external_flags = reg_pdata->flags;
			ret = max77660_regulator_set_power_mode(reg,
					POWER_MODE_DISABLE);
			if (ret < 0) {
				dev_err(&pdev->dev,
					"power mode config for regulator %s failed, %d\n",
					rdesc->name, ret);
				goto clean_exit;
			}
		}
	}

	ret = max77660_pwm_dvfs_init(&pdev->dev, pdata);
	if (ret)
		dev_err(&pdev->dev, "Failed to initialize BUCK4 dvfs");

	return 0;

clean_exit:
	while (--id >= 0) {
		reg  = &max_regs[id];
		if (reg->dev)
			regulator_unregister(reg->rdev);
	}
	return ret;
}

static int max77660_regulator_remove(struct platform_device *pdev)
{
	struct max77660_regulator *max_regs = platform_get_drvdata(pdev);
	struct max77660_regulator *reg;
	int reg_count = MAX77660_REGULATOR_ID_NR;

	while (--reg_count >= 0) {
		reg  = &max_regs[reg_count];
		if (reg->dev)
			regulator_unregister(reg->rdev);
	}

	return 0;
}

static struct platform_driver max77660_regulator_driver = {
	.probe = max77660_regulator_probe,
	.remove = max77660_regulator_remove,
	.driver = {
		.name = "max77660-pmic",
		.owner = THIS_MODULE,
	},
};

static int __init max77660_regulator_init(void)
{
	return platform_driver_register(&max77660_regulator_driver);
}
subsys_initcall(max77660_regulator_init);

static void __exit max77660_reg_exit(void)
{
	platform_driver_unregister(&max77660_regulator_driver);
}
module_exit(max77660_reg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MAX77660 Regulator Driver");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Maxim Integrated");
MODULE_ALIAS("platform:max77660-regulator");
