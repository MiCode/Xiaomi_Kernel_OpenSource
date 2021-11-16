/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2015, 2018-2019, The Linux Foundation. All rights reserved.
 */
#ifndef WSA883X_TEMP_SENSOR_H
#define WSA883X_TEMP_SENSOR_H

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
struct wsa883x_tz_priv {
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

int wsa883x_get_temp(struct thermal_zone_device *tz_dev, int *temp);
int wsa883x_init_thermal(struct wsa883x_tz_priv *tz_pdata);
void wsa883x_deinit_thermal(struct thermal_zone_device *tz_dev);
#endif
