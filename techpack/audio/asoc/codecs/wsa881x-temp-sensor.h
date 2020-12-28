/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2015, 2018, 2020 The Linux Foundation. All rights reserved.
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
typedef int32_t (*wsa_temp_register_read)(struct snd_soc_component *component,
					struct wsa_temp_register *wsa_temp_reg);
struct wsa881x_tz_priv {
	struct thermal_zone_device *tz_dev;
	struct snd_soc_component *component;
	struct wsa_temp_register *wsa_temp_reg;
	char name[80];
	wsa_temp_register_read wsa_temp_reg_read;
	struct notifier_block pm_nb;
	atomic_t is_suspend_spk;
	int t0_init;
	int curr_temp;
};

#ifndef CONFIG_WSA881X_TEMP_SENSOR_DISABLE
int wsa881x_init_thermal(struct wsa881x_tz_priv *tz_pdata);
void wsa881x_deinit_thermal(struct thermal_zone_device *tz_dev);
int wsa881x_get_temp(struct thermal_zone_device *tz_dev, int *temp);
#else
int wsa881x_init_thermal(struct wsa881x_tz_priv *tz_pdata){ return 0; }
void wsa881x_deinit_thermal(struct thermal_zone_device *tz_dev){}
int wsa881x_get_temp(struct thermal_zone_device *tz_dev, int *temp){ return 0; }
#endif
#endif
