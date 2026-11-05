/*
 *  Silicon Integrated Co., Ltd haptic sih688x regmap header file
 *
 *  Copyright (c) 2021 kugua <canzhen.peng@si-in.com>
 *  Copyright (c) 2021 tianchi <tianchi.zheng@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#ifndef _HAPTIC_REGMAP_H
#define _HAPTIC_REGMAP_H


int haptic_regmap_read(struct regmap *regmap, unsigned int start_reg,
	unsigned int reg_num, char *buf);
int haptic_regmap_write(struct regmap *regmap, unsigned int start_reg,
	unsigned int reg_num, const char *buf);
int haptic_regmap_bulk_read(struct regmap *regmap, unsigned int start_reg,
	unsigned int reg_num, char *buf);
int haptic_regmap_bulk_write(struct regmap *regmap,
	unsigned int start_reg, unsigned int reg_num, const char *buf);
int haptic_regmap_update_bits(struct regmap *regmap, unsigned int reg,
	unsigned int mask, unsigned int val);
struct regmap *haptic_regmap_init(struct i2c_client *client,
	const struct regmap_config *config);
void haptic_regmap_remove(struct regmap *regmap);

#endif /* _HAPTIC_REGMAP_H */

