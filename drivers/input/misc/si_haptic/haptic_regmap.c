/*
 *  Silicon Integrated Co., Ltd haptic sih688x regmap driver file
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
#include <linux/i2c.h>

#include "haptic_regmap.h"
#include "haptic_mid.h"
#include "sih688x_reg.h"

int  haptic_regmap_read(struct regmap *regmap, unsigned int start_reg,
	unsigned int reg_num, char *buf)
{
	unsigned int val = 0;
	int ret = -1;

	if (regmap == NULL) {
		hp_err("%s: NULL == regmap\n", __func__);
		return -EINVAL;
	}

	if (buf == NULL) {
		hp_err("%s: NULL == buf\n", __func__);
		return -EINVAL;
	}

	if (reg_num == 1) {
		ret = regmap_read(regmap, start_reg, &val);
		if (!ret)
			*buf = (char)val;

		return ret;
	}

	return regmap_raw_read(regmap, start_reg, buf, reg_num);
}

int haptic_regmap_write(struct regmap *regmap, unsigned int start_reg,
	unsigned int reg_num, const char *buf)
{
	unsigned int val = 0;

	if (regmap == NULL) {
		hp_err("%s: NULL == regmap\n", __func__);
		return -EINVAL;
	}

	if (buf == NULL) {
		hp_err("%s: NULL == buf\n", __func__);
		return -EINVAL;
	}

	if (reg_num == 1) {
		val = (unsigned int)(*buf);
		return regmap_write(regmap, start_reg, val);
	}

	regcache_cache_bypass(regmap, true);
	return regmap_raw_write(regmap, start_reg, buf, reg_num);
}

int haptic_regmap_bulk_write(struct regmap *regmap, unsigned int start_reg,
	unsigned int reg_num, const char *buf)
{
	if (regmap == NULL) {
		hp_err("%s: NULL == regmap\n", __func__);
		return -EINVAL;
	}

	if (buf == NULL) {
		hp_err("%s: NULL == buf\n", __func__);
		return -EINVAL;
	}

	regcache_cache_bypass(regmap, true);
	return regmap_bulk_write(regmap, start_reg, buf, reg_num);
}

int haptic_regmap_bulk_read(struct regmap *regmap, unsigned int start_reg,
	unsigned int reg_num, char *buf)
{
	if (regmap == NULL) {
		hp_err("%s: NULL == regmap\n", __func__);
		return -EINVAL;
	}

	if (buf == NULL) {
		hp_err("%s: NULL == buf\n", __func__);
		return -EINVAL;
	}

	return regmap_bulk_read(regmap, start_reg, buf, reg_num);
}

int haptic_regmap_update_bits(struct regmap *regmap, unsigned int reg,
	unsigned int mask, unsigned int val)
{
	if (regmap == NULL)
		hp_err("%s: NULL == regmap\n", __func__);

	return regmap_update_bits(regmap, reg, mask, val);
}

struct regmap *haptic_regmap_init(struct i2c_client *client,
	const struct regmap_config *config)
{
	if (client == NULL)
		return NULL;

	return devm_regmap_init_i2c(client, config);
}

void haptic_regmap_remove(struct regmap *regmap)
{
	if (!IS_ERR(regmap))
		regmap_exit(regmap);
}

