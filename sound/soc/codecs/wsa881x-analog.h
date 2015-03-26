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

#define WSA881X_I2C_SPK0_SLAVE0_ADDR	0x0E
#define WSA881X_I2C_SPK0_SLAVE1_ADDR	0x44
#define WSA881X_I2C_SPK0_SLAVE0	0
#define WSA881X_I2C_SPK0_SLAVE1	1
#define MAX_WSA881X_DEVICE 2

extern const u8 wsa881x_reg_readable[WSA881X_CACHE_SIZE];
extern const struct reg_default wsa881x_reg_defaults[WSA881X_CACHE_SIZE];
extern struct regmap_config wsa881x_regmap_config[MAX_WSA881X_DEVICE];

#endif /* _WSA881X_H */
