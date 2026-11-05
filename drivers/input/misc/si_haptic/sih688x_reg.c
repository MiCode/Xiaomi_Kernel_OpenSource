/*
 *  Silicon Integrated Co., Ltd haptic sih688x regmap parameter define
 *
 *  Copyright (c) 2021 kugua <canzhen.peng@si-in.com>
 *  Copyright (c) 2021 tianchi <tianchi.zheng@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#include <linux/regmap.h>
#include <linux/device.h>
#include "haptic_regmap.h"
#include "sih688x_reg.h"

static bool sih688x_writeable_register(struct device *dev,
	unsigned int reg)
{
	return true;
}

static bool sih688x_readable_register(struct device *dev,
	unsigned int reg)
{
	return true;
}

static bool sih688x_volatile_register(struct device *dev,
	unsigned int reg)
{
	return true;
}

const struct regmap_config sih688x_regmap_config = {
	.name = "sih688x",
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = SIH688X_REG_MAX,
	.writeable_reg = sih688x_writeable_register,
	.readable_reg = sih688x_readable_register,
	.volatile_reg = sih688x_volatile_register,
	.cache_type = REGCACHE_NONE,
};

