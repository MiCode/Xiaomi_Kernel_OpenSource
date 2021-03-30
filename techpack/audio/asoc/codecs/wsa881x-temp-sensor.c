// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2015, 2017-2019 The Linux Foundation. All rights reserved.
 */

#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/suspend.h>
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
#define WSA881X_TEMP_RETRY 3
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
		     int *temp)
{
	struct wsa881x_tz_priv *pdata;
	struct snd_soc_component *component;
	struct wsa_temp_register reg;
	int dmeas, d1, d2;
	int ret = 0;
	int temp_val;
	int t1 = T1_TEMP;
	int t2 = T2_TEMP;
	u8 retry = WSA881X_TEMP_RETRY;

	if (!thermal)
		return -EINVAL;

	if (thermal->devdata) {
		pdata = thermal->devdata;
		if (pdata->component) {
			component = pdata->component;
		} else {
			pr_err("%s: codec is NULL\n", __func__);
			return -EINVAL;
		}
	} else {
		pr_err("%s: pdata is NULL\n", __func__);
		return -EINVAL;
	}
	if (atomic_cmpxchg(&pdata->is_suspend_spk, 1, 0)) {
		/*
		 * get_temp query happens as part of POST_PM_SUSPEND
		 * from thermal core. To avoid calls to slimbus
		 * as part of this thermal query, return default temp
		 * and reset the suspend flag.
		 */
		if (!pdata->t0_init) {
			if (temp)
				*temp = pdata->curr_temp;
			return 0;
		}
	}

temp_retry:
	if (pdata->wsa_temp_reg_read) {
		ret = pdata->wsa_temp_reg_read(component, &reg);
		if (ret) {
			pr_err("%s: temp read failed: %d, current temp: %d\n",
				__func__, ret, pdata->curr_temp);
			if (temp)
				*temp = pdata->curr_temp;
			return 0;
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
		pr_debug("%s: T0: %d is out of range[%d, %d]\n", __func__,
			 temp_val, LOW_TEMP_THRESHOLD, HIGH_TEMP_THRESHOLD);
		if (retry--) {
			msleep(20);
			goto temp_retry;
		}
	}
	pdata->curr_temp = temp_val;

	if (temp)
		*temp = temp_val;
	pr_debug("%s: t0 measured: %d dmeas = %d, d1 = %d, d2 = %d\n",
		  __func__, temp_val, dmeas, d1, d2);
	return ret;
}
EXPORT_SYMBOL(wsa881x_get_temp);

static struct thermal_zone_device_ops wsa881x_thermal_ops = {
	.get_temp = wsa881x_get_temp,
};


static int wsa881x_pm_notify(struct notifier_block *nb,
				unsigned long mode, void *_unused)
{
	struct wsa881x_tz_priv *pdata =
			container_of(nb, struct wsa881x_tz_priv, pm_nb);

	switch (mode) {
	case PM_SUSPEND_PREPARE:
		atomic_set(&pdata->is_suspend_spk, 1);
		break;
	default:
		break;
	}
	return 0;
}

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
	tz_pdata->pm_nb.notifier_call = wsa881x_pm_notify;
	register_pm_notifier(&tz_pdata->pm_nb);
	atomic_set(&tz_pdata->is_suspend_spk, 0);

	return 0;
}
EXPORT_SYMBOL(wsa881x_init_thermal);

void wsa881x_deinit_thermal(struct thermal_zone_device *tz_dev)
{
	struct wsa881x_tz_priv *pdata;

	if (tz_dev && tz_dev->devdata) {
		pdata = tz_dev->devdata;
		if (pdata)
			unregister_pm_notifier(&pdata->pm_nb);
	}
	if (tz_dev)
		thermal_zone_device_unregister(tz_dev);
}
EXPORT_SYMBOL(wsa881x_deinit_thermal);
