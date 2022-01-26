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

#ifndef __RT5738_REGULATOR_H
#define __RT5738_REGULATOR_H

#include <linux/mutex.h>
#include "mtk_rt5738.h"

#define RT5738_DRV_VERSION	"1.0.0_MTK"

struct regulator_chip {
	unsigned char vol_reg;
	unsigned char mode_reg;
	unsigned char mode_bit;
	unsigned char enable_reg;
	unsigned char enable_bit;
};

struct rt5738_regulator_info {
	struct device *dev;
	struct i2c_client *i2c;
	struct mutex io_lock;
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *regmap_dev;
	struct rt_regmap_properties *regmap_props;
#endif /* #ifdef CONFIG_RT_REGMAP */
	struct regulator_desc *desc;
	struct regulator_dev *regulator;
	struct regulator_chip *reg_chip;
	int id;
	int pin_sel;
};

/* pmic extbuck extern functions */
extern int is_rt5738_exist(void);

#endif /*--__RT5738_REGULATOR_H--*/
