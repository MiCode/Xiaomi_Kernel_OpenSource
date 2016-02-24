/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _WSA881X_H
#define _WSA881X_H

#include <linux/regmap.h>
#include "wsa881x-registers-analog.h"
#include <sound/soc.h>

#define WSA881X_I2C_SPK0_SLAVE0_ADDR	0x0E
#define WSA881X_I2C_SPK0_SLAVE1_ADDR	0x44
#define WSA881X_I2C_SPK1_SLAVE0_ADDR	0x0F
#define WSA881X_I2C_SPK1_SLAVE1_ADDR	0x45

#define WSA881X_I2C_SPK0_SLAVE0	0
#define WSA881X_I2C_SPK1_SLAVE0	1
#define MAX_WSA881X_DEVICE 2
#define WSA881X_DIGITAL_SLAVE 0
#define WSA881X_ANALOG_SLAVE 1

enum {
	WSA881X_1_X = 0,
	WSA881X_2_0,
};

#define WSA881X_IS_2_0(ver) \
	((ver == WSA881X_2_0) ? 1 : 0)

extern const u8 wsa881x_ana_reg_readable[WSA881X_CACHE_SIZE];
extern struct reg_default wsa881x_ana_reg_defaults[WSA881X_CACHE_SIZE];
extern struct regmap_config wsa881x_ana_regmap_config[2];
int wsa881x_get_client_index(void);
int wsa881x_get_probing_count(void);
int wsa881x_get_presence_count(void);
int wsa881x_set_mclk_callback(
	int (*enable_mclk_callback)(struct snd_soc_card *, bool));
void wsa881x_update_reg_defaults_2_0(void);
void wsa881x_update_regmap_2_0(struct regmap *regmap, int flag);

#endif /* _WSA881X_H */
