/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

struct wsa_temp_register {
	u8 d1_msb;
	u8 d1_lsb;
	u8 d2_msb;
	u8 d2_lsb;
	u8 dmeas_msb;
	u8 dmeas_lsb;
};
typedef int32_t (*wsa_temp_register_read)(struct snd_soc_codec *codec,
					struct wsa_temp_register *wsa_temp_reg);
struct wsa881x_tz_priv {
	struct thermal_zone_device *tz_dev;
	struct snd_soc_codec *codec;
	struct wsa_temp_register *wsa_temp_reg;
	char name[80];
	wsa_temp_register_read wsa_temp_reg_read;
};

int wsa881x_get_temp(struct thermal_zone_device *tz_dev, unsigned long *temp);
int wsa881x_init_thermal(struct wsa881x_tz_priv *tz_pdata);
void wsa881x_deinit_thermal(struct thermal_zone_device *tz_dev);
#endif
