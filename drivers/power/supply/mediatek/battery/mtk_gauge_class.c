/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <mach/mtk_pmic.h>
#include <mt-plat/mtk_battery.h>
#include <mt-plat/upmu_common.h>
#include <mt-plat/mtk_rtc.h>
#include <linux/proc_fs.h>

#include "mtk_gauge_class.h"
#include "mtk_battery_internal.h"

static struct class *gauge_class;

void gauge_lock(struct gauge_device *gauge_dev)
{
	mutex_lock(&gauge_dev->ops_lock);
}

void gauge_unlock(struct gauge_device *gauge_dev)
{
	mutex_unlock(&gauge_dev->ops_lock);
}


static ssize_t gauge_show_name(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct gauge_device *gauge_dev = to_gauge_device(dev);

	return snprintf(buf, 20, "%s\n",
		       gauge_dev->props.alias_name ?
		       gauge_dev->props.alias_name : "anonymous");
}

static int gauge_suspend(struct device *dev, pm_message_t state)
{
	struct gauge_device *gauge_dev = to_gauge_device(dev);
	int ret = 0;

	gauge_lock(gauge_dev);
	if (gauge_dev->ops->suspend)
		ret = gauge_dev->ops->suspend(gauge_dev, state);
	gauge_unlock(gauge_dev);

	return ret;
}

static int gauge_resume(struct device *dev)
{
	struct gauge_device *gauge_dev = to_gauge_device(dev);
	int ret = 0;

	gauge_lock(gauge_dev);
	if (gauge_dev->ops->resume)
		ret = gauge_dev->ops->resume(gauge_dev);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_initial(struct gauge_device *gauge_dev)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_initial)
		ret = gauge_dev->ops->gauge_initial(gauge_dev);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_get_current(
	struct gauge_device *gauge_dev, bool *is_charging, int *battery_current)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_read_current)
		ret =
		gauge_dev->ops->gauge_read_current(
			gauge_dev, is_charging, battery_current);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_get_average_current(
	struct gauge_device *gauge_dev, int *data, bool *valid)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_average_current)
		ret = gauge_dev->ops->gauge_get_average_current(
		gauge_dev, data, valid);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_get_coulomb(
	struct gauge_device *gauge_dev, int *data)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_coulomb)
		ret =
			gauge_dev->ops->gauge_get_coulomb(
				gauge_dev, data);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_reset_hw(struct gauge_device *gauge_dev)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_reset_hw)
		ret =
			gauge_dev->ops->gauge_reset_hw(
				gauge_dev);
	gauge_unlock(gauge_dev);
	return ret;
}

int gauge_dev_get_hwocv(
	struct gauge_device *gauge_dev, int *data)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_hwocv)
		ret =
			gauge_dev->ops->gauge_get_hwocv(
			gauge_dev, data);
	gauge_unlock(gauge_dev);
	return ret;
}

int gauge_dev_set_coulomb_interrupt1_ht(
	struct gauge_device *gauge_dev, int car)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_set_coulomb_interrupt1_ht)
		ret =
		gauge_dev->ops->gauge_set_coulomb_interrupt1_ht(
			gauge_dev, car);
	gauge_unlock(gauge_dev);
	return ret;
}

int gauge_dev_set_coulomb_interrupt1_lt(
	struct gauge_device *gauge_dev, int car)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_set_coulomb_interrupt1_lt)
		ret =
			gauge_dev->ops->gauge_set_coulomb_interrupt1_lt(
			gauge_dev, car);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_get_boot_battery_plug_out_status(
	struct gauge_device *gauge_dev, int *is_plugout, int *plutout_time)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_boot_battery_plug_out_status)
		ret =
		gauge_dev->ops->gauge_get_boot_battery_plug_out_status(
		gauge_dev, is_plugout, plutout_time);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_get_ptim_current(
	struct gauge_device *gauge_dev, int *ptim_current, bool *is_charging)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	/*gauge_lock(gauge_dev);*/
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_ptim_current)
		ret =
			gauge_dev->ops->gauge_get_ptim_current(
				gauge_dev, ptim_current, is_charging);
	/*gauge_unlock(gauge_dev);*/

	return ret;
}

int gauge_dev_get_zcv_current(struct gauge_device *gauge_dev, int *zcv_current)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_zcv_current)
		ret =
			gauge_dev->ops->gauge_get_zcv_current(
				gauge_dev, zcv_current);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_get_zcv(
	struct gauge_device *gauge_dev, int *zcv)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_zcv)
		ret =
			gauge_dev->ops->gauge_get_zcv(
				gauge_dev, zcv);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_is_gauge_initialized(
	struct gauge_device *gauge_dev, int *init)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_is_gauge_initialized)
		ret =
			gauge_dev->ops->gauge_is_gauge_initialized(
				gauge_dev, init);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_set_gauge_initialized(
	struct gauge_device *gauge_dev, int init)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_set_gauge_initialized)
		ret =
			gauge_dev->ops->gauge_set_gauge_initialized(
			gauge_dev, init);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_set_battery_cycle_interrupt(
	struct gauge_device *gauge_dev, int car)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_set_battery_cycle_interrupt)
		ret =
		gauge_dev->ops->gauge_set_battery_cycle_interrupt(
			gauge_dev, car);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_reset_shutdown_time(struct gauge_device *gauge_dev)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_reset_shutdown_time)
		ret = gauge_dev->ops->gauge_reset_shutdown_time(gauge_dev);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_reset_ncar(struct gauge_device *gauge_dev)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_reset_ncar)
		ret = gauge_dev->ops->gauge_reset_ncar(gauge_dev);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_set_nag_zcv(struct gauge_device *gauge_dev, int zcv)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_set_nag_zcv)
		ret =
			gauge_dev->ops->gauge_set_nag_zcv(
				gauge_dev, zcv);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_set_nag_c_dltv(
	struct gauge_device *gauge_dev, int c_dltv_mv)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_set_nag_c_dltv)
		ret =
			gauge_dev->ops->gauge_set_nag_c_dltv(
			gauge_dev, c_dltv_mv);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_enable_nag_interrupt(
	struct gauge_device *gauge_dev, int en)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_enable_nag_interrupt)
		ret = gauge_dev->ops->gauge_enable_nag_interrupt(gauge_dev, en);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_get_nag_cnt(
	struct gauge_device *gauge_dev, int *nag_cnt)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_nag_cnt)
		ret = gauge_dev->ops->gauge_get_nag_cnt(gauge_dev, nag_cnt);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_get_nag_dltv(
	struct gauge_device *gauge_dev, int *nag_dltv)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_nag_dltv)
		ret = gauge_dev->ops->gauge_get_nag_dltv(gauge_dev, nag_dltv);
	gauge_unlock(gauge_dev);

	return ret;
}


int gauge_dev_get_nag_c_dltv(
	struct gauge_device *gauge_dev, int *nag_c_dltv)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_nag_c_dltv)
		ret = gauge_dev->ops->gauge_get_nag_c_dltv(
		gauge_dev, nag_c_dltv);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_enable_zcv_interrupt(
	struct gauge_device *gauge_dev, int en)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_enable_zcv_interrupt)
		ret = gauge_dev->ops->gauge_enable_zcv_interrupt(gauge_dev, en);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_set_zcv_interrupt_threshold(
	struct gauge_device *gauge_dev, int threshold)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_set_zcv_interrupt_threshold)
		ret =
		gauge_dev->ops->gauge_set_zcv_interrupt_threshold(
		gauge_dev, threshold);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_enable_battery_tmp_lt_interrupt(
	struct gauge_device *gauge_dev, bool en, int threshold)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_enable_battery_tmp_lt_interrupt)
		ret = gauge_dev->ops->gauge_enable_battery_tmp_lt_interrupt(
		gauge_dev, en, threshold);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_enable_battery_tmp_ht_interrupt(
	struct gauge_device *gauge_dev, bool en, int threshold)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_enable_battery_tmp_ht_interrupt)
		ret = gauge_dev->ops->gauge_enable_battery_tmp_ht_interrupt(
		gauge_dev, en, threshold);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_get_time(
	struct gauge_device *gauge_dev, unsigned int *time)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_time)
		ret = gauge_dev->ops->gauge_get_time(gauge_dev, time);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_enable_time_interrupt(
	struct gauge_device *gauge_dev, int threshold)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_enable_time_interrupt)
		ret =
			gauge_dev->ops->gauge_enable_time_interrupt(
			gauge_dev, threshold);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_get_hw_status(
	struct gauge_device *gauge_dev,
	struct gauge_hw_status *hw_status, int interno)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_hw_status)
		ret =
		gauge_dev->ops->gauge_get_hw_status(
		gauge_dev, hw_status, interno);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_enable_bat_plugout_interrupt(
	struct gauge_device *gauge_dev, int en)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_enable_bat_plugout_interrupt)
		ret =
		gauge_dev->ops->gauge_enable_bat_plugout_interrupt(
		gauge_dev, en);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_enable_iavg_interrupt(
	struct gauge_device *gauge_dev, bool ht_en, int ht_th,
	bool lt_en, int lt_th)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_enable_iavg_interrupt)
		ret =
		gauge_dev->ops->gauge_enable_iavg_interrupt(
		gauge_dev, ht_en, ht_th, lt_en, lt_th);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_enable_vbat_low_interrupt(
	struct gauge_device *gauge_dev, int en)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_enable_vbat_low_interrupt)
		ret =
		gauge_dev->ops->gauge_enable_vbat_low_interrupt(gauge_dev, en);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_enable_vbat_high_interrupt(
	struct gauge_device *gauge_dev, int en)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_enable_vbat_high_interrupt)
		ret = gauge_dev->ops->gauge_enable_vbat_high_interrupt(
		gauge_dev, en);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_set_vbat_low_threshold(
	struct gauge_device *gauge_dev, int threshold)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_set_vbat_low_threshold)
		ret = gauge_dev->ops->gauge_set_vbat_low_threshold(
		gauge_dev, threshold);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_set_vbat_high_threshold(
	struct gauge_device *gauge_dev, int threshold)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_set_vbat_high_threshold)
		ret = gauge_dev->ops->gauge_set_vbat_high_threshold(
		gauge_dev, threshold);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_enable_car_tune_value_calibration(
	struct gauge_device *gauge_dev, int init_current, int *car_tune_value)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_enable_car_tune_value_calibration)
		ret = gauge_dev->ops->gauge_enable_car_tune_value_calibration(
		gauge_dev, init_current, car_tune_value);
	gauge_unlock(gauge_dev);

	return ret;
}


int gauge_dev_get_nag_vbat(
	struct gauge_device *gauge_dev, int *vbat)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_nag_vbat)
		ret = gauge_dev->ops->gauge_get_nag_vbat(gauge_dev, vbat);
	gauge_unlock(gauge_dev);

	return ret;
}


int gauge_dev_set_rtc_ui_soc(
	struct gauge_device *gauge_dev, int ui_soc)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_set_rtc_ui_soc)
		ret = gauge_dev->ops->gauge_set_rtc_ui_soc(gauge_dev, ui_soc);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_get_rtc_ui_soc(
	struct gauge_device *gauge_dev, int *ui_soc)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_rtc_ui_soc)
		ret = gauge_dev->ops->gauge_get_rtc_ui_soc(gauge_dev, ui_soc);
	gauge_unlock(gauge_dev);

	return ret;
}


int gauge_dev_is_rtc_invalid(
	struct gauge_device *gauge_dev, int *invalid)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_is_rtc_invalid)
		ret = gauge_dev->ops->gauge_is_rtc_invalid(gauge_dev, invalid);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_set_reset_status(
	struct gauge_device *gauge_dev, int reset)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_set_reset_status)
		ret = gauge_dev->ops->gauge_set_reset_status(gauge_dev, reset);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_dump(
	struct gauge_device *gauge_dev, struct seq_file *m,
	int type)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_dump)
		ret = gauge_dev->ops->gauge_dump(
			gauge_dev, m, type);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_get_hw_version(
	struct gauge_device *gauge_dev)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_get_hw_version)
		ret = gauge_dev->ops->gauge_get_hw_version(gauge_dev);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_set_info(
	struct gauge_device *gauge_dev, enum gauge_info ginfo, int value)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL &&
		gauge_dev->ops->gauge_set_info)
		ret = gauge_dev->ops->gauge_set_info(gauge_dev, ginfo, value);
	gauge_unlock(gauge_dev);

	return ret;
}

int gauge_dev_get_info(
	struct gauge_device *gauge_dev, enum gauge_info ginfo, int *value)
{
	int ret = -ENOTSUPP;

	if (gauge_dev == NULL)
		return ret;

	gauge_lock(gauge_dev);
	if (gauge_dev != NULL && gauge_dev->ops != NULL
		&& gauge_dev->ops->gauge_get_info)
		ret = gauge_dev->ops->gauge_get_info(
			gauge_dev, ginfo, value);
	gauge_unlock(gauge_dev);

	return ret;
}

static void gauge_device_release(struct device *dev)
{
	struct gauge_device *gauge_dev = to_gauge_device(dev);

	kfree(gauge_dev);
}

static DEVICE_ATTR(name, 0444, gauge_show_name, NULL);

static struct attribute *gauge_class_attrs[] = {
	&dev_attr_name.attr,
	NULL,
};

static const struct attribute_group gauge_group = {
	.attrs = gauge_class_attrs,
};

static const struct attribute_group *gauge_groups[] = {
	&gauge_group,
	NULL,
};

/**
 * gauge_device_register - create and register a new object of
 *   gauge_device class.
 * @name: the name of the new object
 * @parent: a pointer to the parent device
 * @devdata: an optional pointer to be stored for private driver use.
 * The methods may retrieve it by using gauge_get_data(gauge_dev).
 * @ops: the charger operations structure.
 *
 * Creates and registers new charger device. Returns either an
 * ERR_PTR() or a pointer to the newly allocated device.
 */
struct gauge_device *gauge_device_register(const char *name,
		struct device *parent, void *devdata,
		const struct gauge_ops *ops,
		const struct gauge_properties *props)
{
	struct gauge_device *gauge_dev;
	int rc;

	pr_debug("%s: name=%s\n",
		__func__, name);
	gauge_dev = kzalloc(sizeof(*gauge_dev), GFP_KERNEL);
	if (!gauge_dev)
		return ERR_PTR(-ENOMEM);

	mutex_init(&gauge_dev->ops_lock);
	gauge_dev->dev.class = gauge_class;
	gauge_dev->dev.parent = parent;
	gauge_dev->dev.release = gauge_device_release;
	dev_set_name(&gauge_dev->dev, name);
	dev_set_drvdata(&gauge_dev->dev, devdata);

	/* Copy properties */
	if (props) {
		memcpy(&gauge_dev->props, props,
		       sizeof(struct gauge_properties));
	}
	rc = device_register(&gauge_dev->dev);
	if (rc) {
		kfree(gauge_dev);
		return ERR_PTR(rc);
	}
	gauge_dev->ops = ops;
	return gauge_dev;
}
EXPORT_SYMBOL(gauge_device_register);

/**
 * gauge_device_unregister - unregisters a switching charger device
 * object.
 * @gauge_dev: the switching charger device object to be unregistered
 * and freed.
 *
 * Unregisters a previously registered via gauge_device_register object.
 */
void gauge_device_unregister(struct gauge_device *gauge_dev)
{
	if (!gauge_dev)
		return;

	mutex_lock(&gauge_dev->ops_lock);
	gauge_dev->ops = NULL;
	mutex_unlock(&gauge_dev->ops_lock);
	device_unregister(&gauge_dev->dev);
}
EXPORT_SYMBOL(gauge_device_unregister);


static int gauge_match_device_by_name(struct device *dev,
	const void *data)
{
	const char *name = data;

	return strcmp(dev_name(dev), name) == 0;
}

struct gauge_device *get_gauge_by_name(const char *name)
{
	struct device *dev;

	if (!name)
		return (struct gauge_device *)NULL;
	dev = class_find_device(gauge_class, NULL, name,
				gauge_match_device_by_name);

	return dev ? to_gauge_device(dev) : NULL;

}
EXPORT_SYMBOL(get_gauge_by_name);

static void __exit gauge_class_exit(void)
{
	class_destroy(gauge_class);
}

static int __init gauge_class_init(void)
{
	gauge_class = class_create(THIS_MODULE, "gauge");
	if (IS_ERR(gauge_class)) {
		pr_notice("Unable to create gauge class; errno = %ld\n",
			PTR_ERR(gauge_class));
		return PTR_ERR(gauge_class);
	}
	gauge_class->dev_groups = gauge_groups;
	gauge_class->suspend = gauge_suspend;
	gauge_class->resume = gauge_resume;
	return 0;
}

subsys_initcall(gauge_class_init);
module_exit(gauge_class_exit);

MODULE_DESCRIPTION("Gauge Class Device");
MODULE_AUTHOR("wy.chuang <wy.chuang@mediatek.com>");
MODULE_VERSION("1.0.0_G");
MODULE_LICENSE("GPL");
