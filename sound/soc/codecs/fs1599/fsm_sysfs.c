/**
 * Copyright (c) 2018 HuaQin Technologies Co., Ltd. 2018-2019. All rights reserved.
 *Description: Core Defination For Foursemi Device .
 *Author: Fourier Semiconductor Inc.
 * Create: 2019-03-17 File created.
 */

#include "fsm_public.h"
#if defined(CONFIG_FSM_SYSFS)
//#include "fsm_q6afe.h"
#include <linux/sysfs.h>
#include <linux/stat.h>
#include <linux/slab.h>

static int g_fsm_sysfs_inited = 0;

#define FSM_VBAT_MAX (100)
#define FSM_VMAX_MAX (0)

struct vmax_step_config {
	uint32_t vbat_min;
	uint32_t vbat_max;
	int vbat_val;
};

struct fsm_vbats_data {
	uint32_t vbat_sum;
	uint8_t time_cnt;
	int pre_vmax;
	int ndev;
	int state; // 0: off, 1: on
};

static struct vmax_step_config g_vmax_step[] = {
	{ 50, 100, 0x00000000 },
	{ 30,  50, 0xfff8df1f },
	{  0,  30, 0xfff1568c },
};

static int fsm_monitor_get_battery_capacity(uint32_t *vbat_capacity)
{
	union power_supply_propval prop = { 0 };
	struct power_supply *psy;
	char name[] = "battery";
	int ret;

	if (vbat_capacity == NULL) {
		pr_err("invalid paramter");
		return -EINVAL;
	}

	psy = power_supply_get_by_name(name);
	if (psy == NULL) {
		pr_err("get power supply failed");
		return -EINVAL;
	}
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &prop);
	if (ret < 0) {
		pr_err("get vbat capacity failed");
		return ret;
	}
	*vbat_capacity = prop.intval;

	return 0;
}

static int fsm_search_vmax_from_table(const int vbat_capacity, int *vmax_val)
{
	struct vmax_step_config *vmax_cfg = g_vmax_step;
	int idx;

	if (vmax_val == NULL) {
		pr_err("invalid paramter");
		return -EINVAL;
	}

	if (vbat_capacity >= FSM_VBAT_MAX) {
		*vmax_val = FSM_VMAX_MAX;
		return 0;
	}
	for (idx = 0; idx < ARRAY_SIZE(g_vmax_step); idx++) {
		if (vbat_capacity >= vmax_cfg[idx].vbat_min
				&& vbat_capacity < vmax_cfg[idx].vbat_max) {
			*vmax_val = vmax_cfg[idx].vbat_val;
			return 0;
		}
	}
	pr_err("vmax_val not found!");
	*vmax_val = 0;

	return -ENODATA;
}

static int fsm_get_vbat_vmax(int *vmax_val)
{
	uint32_t vbat_capacity;
	int vmax_set = 0;
	int ret;

	if (vmax_val == NULL)
		return -EINVAL;

	ret = fsm_monitor_get_battery_capacity(&vbat_capacity);
	if (ret) {
		pr_err("get bat copacity fail:%d", ret);
		return ret;
	}
	pr_info("vbat_capacity: %d", vbat_capacity);
	ret = fsm_search_vmax_from_table(vbat_capacity, &vmax_set);
	if (ret < 0) {
		pr_err("not find vmax_set");
		return ret;
	}
	*vmax_val = vmax_set;

	return ret;
}

static ssize_t fsm_vmax_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int vmax = 0;
	int size;
	int ret;

	ret = fsm_get_vbat_vmax(&vmax);
	if (ret)
		pr_err("get vmax fail:%d", ret);
	size = scnprintf(buf, PAGE_SIZE, "0x%X\n", vmax);

	return size;
}

static ssize_t fsm_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size = 0;
	size = scnprintf(buf, PAGE_SIZE, "version:%s\n", FSM_CODE_VERSION);

	return size;
}

static DEVICE_ATTR(vmax, S_IRUGO | S_IWUSR,
		fsm_vmax_show, NULL);

static DEVICE_ATTR(version, S_IRUGO | S_IWUSR,
		fsm_version_show, NULL);

static struct attribute *fsm_attributes[] = {
	&dev_attr_vmax.attr,
	&dev_attr_version.attr,	
	NULL
};

static const struct attribute_group fsm_attr_group = {
	.attrs = fsm_attributes,
};

int fsm_sysfs_init(struct device *dev)
{
	int ret;

	if (g_fsm_sysfs_inited) {
		return MODULE_INITED;
	}
	ret = sysfs_create_group(&dev->kobj, &fsm_attr_group);
	if (!ret) {
		g_fsm_sysfs_inited = 1;
	}

	return ret;
}

void fsm_sysfs_deinit(struct device *dev)
{
	sysfs_remove_group(&dev->kobj, &fsm_attr_group);
	g_fsm_sysfs_inited = 0;
}
#endif

