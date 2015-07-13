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

#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/thermal.h>
#include <sound/soc.h>
#include "wsa881x-temp-sensor.h"

#define T1_TEMP -10
#define T2_TEMP 150
#define LOW_TEMP_THRESHOLD 5
#define HIGH_TEMP_THRESHOLD 45
#define WSA881X_OTP_REG_1	0x0081
#define WSA881X_OTP_REG_2	0x0082
#define WSA881X_OTP_REG_3	0x0083
#define WSA881X_OTP_REG_4	0x0084
#define WSA881X_TEMP_DOUT_MSB	0x000A
#define WSA881X_TEMP_DOUT_LSB	0x000B
#define TEMP_INVALID	0xFFFF
#define WSA881X_THERMAL_TEMP_OP 0x0003

static void calculate_temp(long *temp_val, int dmeas,
			struct snd_soc_codec *codec,
			int dig_base_addr)
{
	/* Tmeas = T1 + ((Dmeas - D1)/(D2 - D1))(T2 - T1) */
	int t1 = T1_TEMP;
	int t2 = T2_TEMP;
	int d1_lsb, d1_msb, d2_lsb, d2_msb;
	int d1, d2;

	d1_msb = snd_soc_read(codec, dig_base_addr + WSA881X_OTP_REG_1);
	d1_lsb = snd_soc_read(codec, dig_base_addr + WSA881X_OTP_REG_2);
	d2_msb = snd_soc_read(codec, dig_base_addr + WSA881X_OTP_REG_3);
	d2_lsb = snd_soc_read(codec, dig_base_addr + WSA881X_OTP_REG_4);

	d1 = ((d1_msb << 0x8) | d1_lsb) >> 0x6;
	d2 = ((d2_msb << 0x8) | d2_lsb) >> 0x6;

	if (d1 == d2) {
		*temp_val = TEMP_INVALID;
		return;
	}
	*temp_val = t1 + (((dmeas - d1) * (t2 - t1))/(d2 - d1));
}

static int wsa881x_get_temp(struct thermal_zone_device *thermal,
			unsigned long *temp)
{
	struct wsa881x_tz_priv *pdata;
	struct snd_soc_codec *codec;
	int dmeas_cur_msb, dmeas_cur_lsb;
	int dmeas;
	int ret = 0;
	long temp_val;

	if (thermal->devdata) {
		pdata = thermal->devdata;
		if (pdata->codec) {
			codec = pdata->codec;
		} else {
			pr_err("%s: codec is NULL\n", __func__);
			return -EINVAL;
		}
	} else {
		pr_err("%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}
	ret = pdata->wsa_resource_acquire(codec, true);
	if (ret) {
		pr_err("%s: wsa acquire failed: %d\n", __func__, ret);
		return ret;
	}
	snd_soc_update_bits(codec,
			    pdata->ana_base + WSA881X_THERMAL_TEMP_OP,
			    0x04, 0x04);
	dmeas_cur_msb =
		snd_soc_read(codec,
			pdata->ana_base + WSA881X_TEMP_DOUT_MSB);
	dmeas_cur_lsb =
		snd_soc_read(codec,
			pdata->ana_base + WSA881X_TEMP_DOUT_LSB);
	dmeas = ((dmeas_cur_msb << 0x8) | dmeas_cur_lsb) >> 0x6;
	pr_debug("%s: dmeas: %d\n", __func__, dmeas);
	calculate_temp(&temp_val, dmeas, codec, pdata->dig_base);
	*temp = temp_val;
	if (temp_val <= LOW_TEMP_THRESHOLD ||
			temp_val >= HIGH_TEMP_THRESHOLD) {
		pr_err("%s: T0: %ld is out of range [%d, %d]\n", __func__,
			temp_val, LOW_TEMP_THRESHOLD, HIGH_TEMP_THRESHOLD);
		ret = -EAGAIN;
		goto rel;
	}
	pr_debug("%s: t0 measured: %ld\n", __func__, temp_val);
rel:
	ret = pdata->wsa_resource_acquire(codec, false);
	if (ret)
		pr_err("%s: wsa release failed: %d\n", __func__, ret);
	return ret;
}

static struct thermal_zone_device_ops wsa881x_thermal_ops = {
	.get_temp = wsa881x_get_temp,
};

int wsa881x_init_thermal(struct wsa881x_tz_priv *tz_pdata)
{
	struct thermal_zone_device *tz_dev;

	if (tz_pdata == NULL) {
		pr_err("%s: thermal pdata is NULL\n", __func__);
		return -EINVAL;
	}
	/* Register with the thermal zone */
	tz_dev = thermal_zone_device_register(tz_pdata->name,
				0, 0, tz_pdata,
				&wsa881x_thermal_ops, NULL, 0, 0);
	if (IS_ERR(tz_dev)) {
		pr_err("%s: thermal device register failed.\n", __func__);
		return -EINVAL;
	}
	tz_pdata->tz_dev = tz_dev;
	return 0;
}
EXPORT_SYMBOL(wsa881x_init_thermal);

void wsa881x_deinit_thermal(struct thermal_zone_device *tz_dev)
{
	if (tz_dev)
		thermal_zone_device_unregister(tz_dev);
}
EXPORT_SYMBOL(wsa881x_deinit_thermal);
