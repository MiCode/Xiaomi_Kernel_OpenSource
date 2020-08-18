/*
 * Copyright (C) 2017 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MT6691_REGULATOR_H
#define __MT6691_REGULATOR_H

#include <linux/mutex.h>

#define MT6691_DRV_VERSION	"1.0.0_MTK"

struct mt6691_regulator_info {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex io_lock;
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *regmap_dev;
	struct rt_regmap_properties *regmap_props;
#endif /* #ifdef CONFIG_RT_REGMAP */
	struct regulator_dev *regulator;
	const struct regulator_chip *reg_chip;
	int id;
	int pin_sel;
};

/* pmic extbuck extern functions */
extern int is_mt6691_exist(void);

/* Custom MT6691_NAME */
#define MT6691_NAME_0		"ext_buck_lp4"
#define	MT6691_NAME_1		"ext_buck_lp4x"
#define	MT6691_NAME_2		"ext_buck_vmddr"

/* MT6691 operation register */
#define MT6691_REG_VSEL0	(0x00)
#define MT6691_REG_VSEL1	(0x01)
#define MT6691_REG_CTRL1	(0x02)
#define MT6691_REG_ID1		(0x03)
#define MT6691_REG_ID2		(0x04)
#define MT6691_REG_MONITOR	(0x05)
#define MT6691_REG_CTRL2	(0x06)
#define MT6691_REG_CTRL3	(0x07)
#define MT6691_REG_CTRL4	(0x08)
/* Hidden mode */
/* #define MT6691_REG_CTRL5	(0x09) */

#if defined(MT6691_NAME_0)
#define MT6691_CMPT_STR_0	"mediatek,ext_buck_lp4"
#define MT6691_VSEL_0		MT6691_REG_VSEL0
#define MT6691_CTRL_0		MT6691_REG_CTRL1
#define	MT6691_CTRL_BIT_0	0x01
#define	MT6691_EN_0		MT6691_REG_CTRL2
#define	MT6691_EN_BIT_0		0x01
#endif

#if defined(MT6691_NAME_1)
#define MT6691_CMPT_STR_1	"mediatek,ext_buck_lp4x"
#define MT6691_VSEL_1		MT6691_REG_VSEL1
#define MT6691_CTRL_1		MT6691_REG_CTRL1
#define	MT6691_CTRL_BIT_1	0x02
#define	MT6691_EN_1		MT6691_REG_CTRL2
#define	MT6691_EN_BIT_1		0x02
#endif

#if defined(MT6691_NAME_2)
#define MT6691_CMPT_STR_2	"mediatek,ext_buck_vmddr"
#define MT6691_VSEL_2		MT6691_REG_VSEL0
#define MT6691_CTRL_2		MT6691_REG_CTRL1
#define	MT6691_CTRL_BIT_2	0x01
#define	MT6691_EN_2		MT6691_REG_CTRL2
#define	MT6691_EN_BIT_2		0x01
#endif

#endif /*--__MT6691_REGULATOR_H--*/
