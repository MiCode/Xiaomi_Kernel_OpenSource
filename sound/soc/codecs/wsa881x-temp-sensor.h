/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef WSA881X_TEMP_SENSOR_H
#define WSA881X_TEMP_SENSOR_H

#include <linux/thermal.h>
#include <sound/soc.h>

typedef int32_t (*wsa_rsrc_acquire)(struct snd_soc_codec *codec, bool enable);

struct wsa881x_tz_priv {
	struct thermal_zone_device *tz_dev;
	struct snd_soc_codec *codec;
	int dig_base;
	int ana_base;
	char name[80];
	wsa_rsrc_acquire wsa_resource_acquire;
};

int wsa881x_init_thermal(struct wsa881x_tz_priv *tz_pdata);
void wsa881x_deinit_thermal(struct thermal_zone_device *tz_dev);
#endif
