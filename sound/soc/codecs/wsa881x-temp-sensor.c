/* Copyright (c) 2015-2016, 2018 The Linux Foundation. All rights reserved.
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
#define TEMP_INVALID	0xFFFF

/*
 * wsa881x_get_temp - get wsa temperature
 * @thermal: thermal zone device
 * @temp: temperature value
 *
 * Get the temperature of wsa881x.
 *
 * Return: 0 on success or negative error code on failure.
 */
int wsa881x_get_temp(struct thermal_zone_device *thermal,
		     unsigned long *temp)
{
	struct wsa881x_tz_priv *pdata;
	struct snd_soc_codec *codec;
	struct wsa_temp_register reg;
	int dmeas, d1, d2;
	int ret = 0;
	long temp_val;
	int t1 = T1_TEMP;
	int t2 = T2_TEMP;

	if (!thermal)
		return -EINVAL;

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
	if (pdata->wsa_temp_reg_read) {
		ret = pdata->wsa_temp_reg_read(codec, &reg);
		if (ret) {
			pr_err("%s: temperature register read failed: %d\n",
				__func__, ret);
			return ret;
		}
	} else {
		pr_err("%s: wsa_temp_reg_read is NULL\n", __func__);
		return -EINVAL;
	}
	/*
	 * Temperature register values are expected to be in the
	 * following range.
	 * d1_msb  = 68 - 92 and d1_lsb  = 0, 64, 128, 192
	 * d2_msb  = 185 -218 and  d2_lsb  = 0, 64, 128, 192
	 */
	if ((reg.d1_msb < 68 || reg.d1_msb > 92) ||
	    (!(reg.d1_lsb == 0 || reg.d1_lsb == 64 || reg.d1_lsb == 128 ||
		reg.d1_lsb == 192)) ||
	    (reg.d2_msb < 185 || reg.d2_msb > 218) ||
	    (!(reg.d2_lsb == 0 || reg.d2_lsb == 64 || reg.d2_lsb == 128 ||
		reg.d2_lsb == 192))) {
		printk_ratelimited("%s: Temperature registers[%d %d %d %d] are out of range\n",
				   __func__, reg.d1_msb, reg.d1_lsb, reg.d2_msb,
				   reg.d2_lsb);
	}
	dmeas = ((reg.dmeas_msb << 0x8) | reg.dmeas_lsb) >> 0x6;
	d1 = ((reg.d1_msb << 0x8) | reg.d1_lsb) >> 0x6;
	d2 = ((reg.d2_msb << 0x8) | reg.d2_lsb) >> 0x6;

	if (d1 == d2)
		temp_val = TEMP_INVALID;
	else
		temp_val = t1 + (((dmeas - d1) * (t2 - t1))/(d2 - d1));

	if (temp_val <= LOW_TEMP_THRESHOLD ||
		temp_val >= HIGH_TEMP_THRESHOLD) {
		pr_debug("%s: T0: %ld is out of range[%d, %d]\n", __func__,
			 temp_val, LOW_TEMP_THRESHOLD, HIGH_TEMP_THRESHOLD);
	}
	if (temp)
		*temp = temp_val;
	pr_debug("%s: t0 measured: %ld dmeas = %d, d1 = %d, d2 = %d\n",
		  __func__, temp_val, dmeas, d1, d2);
	return ret;
}
EXPORT_SYMBOL(wsa881x_get_temp);

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
