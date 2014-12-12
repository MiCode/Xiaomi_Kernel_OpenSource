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
#include <sound/soc.h>
#include "wsa881x-registers.h"

#define WSA881X_MAX_SWR_PORTS   4

extern int wsa881x_set_channel_map(struct snd_soc_codec *codec, u8 *port,
				u8 num_port, unsigned int *ch_mask,
				unsigned int *ch_rate);

extern const u8 wsa881x_reg_readable[WSA881X_CACHE_SIZE];
extern struct regmap_config wsa881x_regmap_config;

#endif /* _WSA881X_H */
