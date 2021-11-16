/*
 * aw87xxx_monitor.c  aw87xxx pa module
 *
 * Copyright (c) 2020 AWINIC Technology CO., LTD
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * Author: Alex <zhangpengbiao@awinic.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/power_supply.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/gameport.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include "aw87xxx.h"
#include "aw87xxx_monitor.h"
#include "awinic_dsp.h"

/****************************************************************************
* aw87xxx get battery capacity
*****************************************************************************/
int aw87xxx_get_battery_capacity(struct aw87xxx *aw87xxx,
				 uint32_t *vbat_capacity)
{
	char name[] = "battery";
	int ret;
	union power_supply_propval prop = { 0 };
	struct power_supply *psy = NULL;

	aw_dev_info(aw87xxx->dev, "%s:enter\n", __func__);
	psy = power_supply_get_by_name(name);
	if (psy) {
		ret = power_supply_get_property(psy,
						POWER_SUPPLY_PROP_CAPACITY,
						&prop);
		if (ret < 0) {
			aw_dev_err(aw87xxx->dev,
				   "%s: get vbat capacity failed\n", __func__);
			return -EINVAL;
		}
		*vbat_capacity = prop.intval;
		aw_dev_info(aw87xxx->dev, "The percentage is %d\n",
			    *vbat_capacity);
	} else {
		aw_dev_err(aw87xxx->dev, "no struct power supply name :%s",
			   name);
		return -EINVAL;
	}
	return 0;
}

/*****************************************************
 * aw87xxx monitor control
*****************************************************/
void aw87xxx_monitor_stop(struct aw87xxx_monitor *monitor)
{
	struct aw87xxx *aw87xxx = container_of(monitor,
						struct aw87xxx, monitor);

	aw_dev_info(aw87xxx->dev, "%s enter, dev_i2c%d@0x%02X\n", __func__,
			aw87xxx->i2c_seq, aw87xxx->i2c_addr);

	if (delayed_work_pending(&aw87xxx->monitor.work))
		cancel_delayed_work(&aw87xxx->monitor.work);
}

void aw87xxx_monitor_start(struct aw87xxx_monitor *monitor)
{
	struct aw87xxx *aw87xxx = container_of(monitor,
					struct aw87xxx, monitor);

	if (aw87xxx->current_mode == AW87XXX_RCV_MODE) {
		aw87xxx_monitor_stop(&aw87xxx->monitor);
	} else if (aw87xxx->monitor.monitor_flag &&
			aw87xxx->open_dsp_en &&
			(aw87xxx->monitor.cfg_update_flag == AW87XXX_CFG_OK)) {
		aw87xxx->monitor.pre_vmax = AW_VMAX_INIT_VAL;
		aw87xxx->monitor.first_entry = AW_FIRST_ENTRY;
		schedule_delayed_work(&aw87xxx->monitor.work,
				msecs_to_jiffies(WAIT_DSP_OPEN_TIME));
	}
}

static int aw87xxx_vbat_monitor_update_vmax(struct aw87xxx *aw87xxx,
					    int vbat_vol)
{
	int ret = -1;
	int i = 0;
	uint32_t vmax_flag = 0;
	uint32_t vmax_set = 0;
	struct aw87xxx_monitor *monitor = NULL;

	monitor = &aw87xxx->monitor;

	for (i = 0; i < monitor->vmax_cfg->vmax_cfg_num; i++) {
		if (vbat_vol > monitor->vmax_cfg->vmax_cfg_total[i].min_thr) {
			vmax_set = monitor->vmax_cfg->vmax_cfg_total[i].vmax;
			vmax_flag = 1;
			aw_dev_dbg(aw87xxx->dev,
				   "%s: read setting vmax=0x%x, step[%d]: min_thr=%d\n",
				   __func__, vmax_set, i,
				   monitor->vmax_cfg->vmax_cfg_total[i].
				   min_thr);
			break;
		}
	}
	pr_info("%s:vmax_flag=%d\n", __func__, vmax_flag);
	if (vmax_flag) {
		if (monitor->pre_vmax != vmax_set) {
			ret = aw_set_vmax_to_dsp(vmax_set, aw87xxx->pa_channel);
			if (ret) {
				aw_dev_err(aw87xxx->dev,
					   "%s: set dsp msg fail, ret=%d\n",
					   __func__, ret);
				return ret;
			} else {
				aw_dev_info(aw87xxx->dev,
					    "%s: set dsp vmax=0x%x sucess\n",
					    __func__, vmax_set);
				monitor->pre_vmax = vmax_set;
			}
		} else {
			aw_dev_info(aw87xxx->dev, "%s:vmax=0x%x no change\n",
				    __func__, vmax_set);
		}
	}
	return 0;
}

static void aw87xxx_monitor_work_func(struct work_struct *work)
{
	int ret;
	uint32_t vbat_capacity;
	uint32_t ave_capacity;
	struct aw87xxx *aw87xxx = container_of(work,
					       struct aw87xxx, monitor.work.work);

	aw_dev_info(aw87xxx->dev, "%s: enter\n", __func__);

	ret = aw87xxx_get_battery_capacity(aw87xxx, &vbat_capacity);
	if (ret < 0)
		return;

	if (aw87xxx->monitor.timer_cnt < aw87xxx->monitor.timer_cnt_max) {
		aw87xxx->monitor.timer_cnt++;
		aw87xxx->monitor.vbat_sum += vbat_capacity;
	}
	if ((aw87xxx->monitor.timer_cnt == aw87xxx->monitor.timer_cnt_max) ||
	    (aw87xxx->monitor.first_entry == AW_FIRST_ENTRY)) {

		if (aw87xxx->monitor.first_entry == AW_FIRST_ENTRY)
			aw87xxx->monitor.first_entry = AW_NOT_FIRST_ENTRY;

		ave_capacity = aw87xxx->monitor.vbat_sum /
		    aw87xxx->monitor.timer_cnt;

		if (aw87xxx->monitor.custom_capacity)
			ave_capacity = aw87xxx->monitor.custom_capacity;

		aw_dev_info(aw87xxx->dev, "%s: get average capacity = %d\n",
			    __func__, ave_capacity);

		aw87xxx_vbat_monitor_update_vmax(aw87xxx, ave_capacity);

		aw87xxx->monitor.timer_cnt = 0;
		aw87xxx->monitor.vbat_sum = 0;
	}
	schedule_delayed_work(&aw87xxx->monitor.work,
				msecs_to_jiffies(aw87xxx->monitor.timer_val));
}

static int aw87xxx_nodsp_get_vmax(struct aw87xxx *aw87xxx,
					uint32_t *vmax_get)
{
	int ret = -1, i = 0;
	uint32_t vbat_capacity = 0;
	struct vmax_config *vmax_cfg = aw87xxx->monitor.vmax_cfg;

	ret = aw87xxx_get_battery_capacity(aw87xxx, &vbat_capacity);
	if (ret < 0)
		return ret;
	aw_dev_info(aw87xxx->dev, "%s: get_battery_capacity is [%d]\n",
		__func__, vbat_capacity);

	for (i = 0; i < vmax_cfg->vmax_cfg_num; i++) {
		if (vbat_capacity > vmax_cfg->vmax_cfg_total[i].min_thr) {
			*vmax_get = vmax_cfg->vmax_cfg_total[i].vmax;
			aw_dev_info(aw87xxx->dev, "%s: read setting vmax=0x%x, step[%d]: min_thr=%d\n",
				__func__, *vmax_get, i,
				vmax_cfg->vmax_cfg_total[i].min_thr);
			return ret;
		}
	}

	return -EPERM;
}

/**********************************************************
 * aw873xx monitor attribute
***********************************************************/
static ssize_t aw87xxx_get_vbat(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);

	len += snprintf(buf + len, PAGE_SIZE - len,
			"vbat capacity=%d\n", aw87xxx->monitor.custom_capacity);

	return len;
}

static ssize_t aw87xxx_set_vbat(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t len)
{
	int ret = -1;
	uint32_t capacity;
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);

	if (len == 0)
		return 0;

	ret = kstrtouint(buf, 0, &capacity);
	if (ret < 0)
		return ret;
	aw_dev_info(aw87xxx->dev, "%s: set capacity = %d\n",
		    __func__, capacity);
	if (capacity >= AW87XXX_VBAT_CAPACITY_MIN &&
	    capacity <= AW87XXX_VBAT_CAPACITY_MAX)
		aw87xxx->monitor.custom_capacity = capacity;

	return len;
}

static ssize_t aw87xxx_get_vmax(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	uint32_t vmax_get = 0;
	int ret = -1;
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);

	aw_dev_info(aw87xxx->dev, "%s: enter", __func__);

	if (aw87xxx->open_dsp_en) {
		ret = aw_get_vmax_from_dsp(&vmax_get, aw87xxx->pa_channel);
		if (ret < 0) {
			aw_dev_err(aw87xxx->dev, "%s: get dsp vmax fail, ret = %d\n",
				__func__, ret);
		} else {
			len += snprintf(buf + len, PAGE_SIZE - len,
				"get_vmax=0x%x\n", vmax_get);
			aw_dev_info(aw87xxx->dev, "%s: get vmax=[%08x] from dsp\n",
				   __func__, vmax_get);
		}
	} else {
		if (aw87xxx->monitor.cfg_update_flag != AW87XXX_CFG_OK) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"%x\n", 0);
			aw_dev_err(aw87xxx->dev, "%s: vmax not ready,return 0 to hal\n",
				   __func__);
			return len;
		}
		ret = aw87xxx_nodsp_get_vmax(aw87xxx, &vmax_get);
		if (ret < 0) {
			aw_dev_err(aw87xxx->dev, "%s: get nodsp vmax fail, ret = %d\n",
				   __func__, ret);
		} else {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"%x\n", vmax_get);
			aw_dev_info(aw87xxx->dev, "%s: set vmax=[%08x] to hal\n",
				   __func__, vmax_get);
		}
	}

	return len;
}

static ssize_t aw87xxx_set_vmax(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t len)
{
	uint32_t vmax_set = 0;
	int ret = -1;
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);

	if (len == 0)
		return 0;

	ret = kstrtouint(buf, 0, &vmax_set);
	if (ret < 0)
		return ret;

	aw_dev_info(aw87xxx->dev, "%s: vmax_set=%d\n", __func__, vmax_set);

	if (aw87xxx->open_dsp_en) {
		ret = aw_set_vmax_to_dsp(vmax_set, aw87xxx->pa_channel);
		if (ret)
			aw_dev_err(aw87xxx->dev, "%s: send dsp_msg error, ret = %d\n",
				   __func__, ret);

		mdelay(2);
	}

	return len;
}

static ssize_t aw87xxx_get_monitor(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	ssize_t len = 0;
	uint32_t local_enable;

	local_enable = aw87xxx->monitor.monitor_flag;
	len += snprintf(buf + len, PAGE_SIZE - len,
			"aw87xxx monitor enable: %d\n", local_enable);
	return len;
}

static ssize_t aw87xxx_set_monitor(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	uint32_t enable = 0;
	int ret = -1;

	if (count == 0)
		return 0;

	ret = kstrtouint(buf, 0, &enable);
	if (ret < 0)
		return ret;

	aw_dev_info(aw87xxx->dev, "%s:monitor enable set =%d\n",
		    __func__, enable);
	aw87xxx->monitor.monitor_flag = enable;

	if (!aw87xxx->monitor.monitor_flag) {
		aw87xxx_monitor_stop(&aw87xxx->monitor);
	} else {
		if (aw87xxx->current_mode != AW87XXX_RCV_MODE &&
		     aw87xxx->monitor.cfg_update_flag == AW87XXX_CFG_OK)
			schedule_delayed_work(&aw87xxx->monitor.work,
				msecs_to_jiffies(aw87xxx->monitor.timer_val));
	}
	return count;
}

static ssize_t aw87xxx_get_vmax_time(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	ssize_t len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len,
			"aw87xxx_vmax_timer_val = %d\n",
			aw87xxx->monitor.timer_val);
	return len;
}

static ssize_t aw87xxx_set_vmax_time(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct aw87xxx *aw87xxx = dev_get_drvdata(dev);
	unsigned int timer_val = 0;
	int ret;

	ret = kstrtouint(buf, 0, &timer_val);
	if (ret < 0)
		return ret;
	pr_info("%s:timer_val =%d\n", __func__, timer_val);

	aw87xxx->monitor.timer_val = timer_val;

	return count;
}

static DEVICE_ATTR(vbat, S_IWUSR | S_IRUGO, aw87xxx_get_vbat, aw87xxx_set_vbat);
static DEVICE_ATTR(vmax, S_IWUSR | S_IRUGO, aw87xxx_get_vmax, aw87xxx_set_vmax);
static DEVICE_ATTR(monitor, S_IWUSR | S_IRUGO,
		   aw87xxx_get_monitor, aw87xxx_set_monitor);
static DEVICE_ATTR(vmax_time, S_IWUSR | S_IRUGO,
		   aw87xxx_get_vmax_time, aw87xxx_set_vmax_time);

static struct attribute *aw87xxx_monitor_attr[] = {
	&dev_attr_vbat.attr,
	&dev_attr_vmax.attr,
	&dev_attr_monitor.attr,
	&dev_attr_vmax_time.attr,
	NULL
};

static struct attribute_group aw87xxx_monitor_attr_group = {
	.attrs = aw87xxx_monitor_attr
};

/**********************************************************
 * aw87xxx monitor init
***********************************************************/
void aw87xxx_monitor_init(struct aw87xxx_monitor *monitor)
{
	int ret;
	struct aw87xxx *aw87xxx = container_of(monitor,
					       struct aw87xxx, monitor);

	aw_dev_info(aw87xxx->dev, "%s: enter\n", __func__);
	INIT_DELAYED_WORK(&monitor->work, aw87xxx_monitor_work_func);

	ret = sysfs_create_group(&aw87xxx->dev->kobj,
				 &aw87xxx_monitor_attr_group);
	if (ret < 0) {
		aw_dev_err(aw87xxx->dev,
			   "%s error creating monitor sysfs attr files\n",
			   __func__);
	}
}

void aw87xxx_parse_monitor_dt(struct aw87xxx_monitor *monitor)
{
	int ret;
	struct aw87xxx *aw87xxx = container_of(monitor,
					       struct aw87xxx, monitor);
	struct device_node *np = aw87xxx->dev->of_node;

	ret = of_property_read_u32(np, "monitor-flag", &monitor->monitor_flag);
	if (ret) {
		monitor->monitor_flag = AW87XXX_MONITOR_DEFAULT_FLAG;
		aw_dev_err(aw87xxx->dev,
			   "%s: monitor-flag get failed ,user default value!\n",
			   __func__);
	} else {
		aw_dev_info(aw87xxx->dev, "%s: monitor-flag = %d\n",
			    __func__, monitor->monitor_flag);
	}

	ret = of_property_read_u32(np, "monitor-timer-val",
				   &monitor->timer_val);
	if (ret) {
		monitor->timer_val = AW87XXX_MONITOR_DEFAULT_TIMER_VAL;
		aw_dev_err(aw87xxx->dev,
			   "%s: monitor-timer-val get failed,user default value!\n",
			   __func__);
	} else {
		aw_dev_info(aw87xxx->dev, "%s: monitor-timer-val = %d\n",
			    __func__, monitor->timer_val);
	}

	ret = of_property_read_u32(np, "monitor-timer-count-max",
				   &monitor->timer_cnt_max);
	if (ret) {
		monitor->timer_cnt_max = AW87XXX_MONITOR_DEFAULT_TIMER_COUNT;
		aw_dev_err(aw87xxx->dev,
			   "%s: monitor-timer-count-max get failed,user default config!\n",
			   __func__);
	} else {
		aw_dev_info(aw87xxx->dev, "%s: monitor-timer-count-max = %d\n",
			    __func__, monitor->timer_cnt_max);
	}
}
