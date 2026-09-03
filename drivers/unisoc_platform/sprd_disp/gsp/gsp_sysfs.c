// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include "gsp_core.h"
#include "gsp_debug.h"
#include "gsp_dev.h"
#include "gsp_workqueue.h"

/*
 * store all gsp cores to match core
 * for device attribute function
 */
struct gsp_core_match_table {
	struct gsp_core **cores_arr;
	int core_cnt;
};

static struct gsp_core_match_table table;

static int gsp_core_match_table_create(struct gsp_dev *gsp)
{
	int index = 0;
	int num = 0;
	struct gsp_core **cores = NULL;
	struct gsp_core *core = NULL;

	num = gsp->core_cnt;
	if (num > 0)
		table.core_cnt = num;
	else
		return -1;

	cores = kcalloc(num, sizeof(struct gsp_core *), GFP_KERNEL);
	if (IS_ERR_OR_NULL(cores)) {
		GSP_ERR("cores array allocated failed\n");
		return -1;
	}

	table.cores_arr = cores;
	for_each_gsp_core(core, gsp) {
		table.cores_arr[index] = core;
		index++;
	}

	return 0;
}

static void gsp_core_match_table_destroy(void)
{
	kfree(table.cores_arr);

	table.core_cnt = 0;
}

static struct gsp_core *gsp_core_match(struct device *dev)
{
	int i;
	struct gsp_core *core = NULL;

	for (i = 0; i < table.core_cnt; i++) {
		core = table.cores_arr[i];
		if (dev == gsp_core_to_device(core))
			break;
	}

	return core;
}

static ssize_t core_cnt_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct gsp_dev *gsp = NULL;

	gsp = dev_get_drvdata(dev);
	if (gsp_dev_verify(gsp)) {
		GSP_ERR("invalidate gsp device show core cnt\n");
		return -ENXIO;
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n", gsp->core_cnt);
}

static ssize_t dev_state_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{

	/* 1: idle, 0: busy*/
	int idle = 0;
	struct gsp_dev *gsp = NULL;

	gsp = dev_get_drvdata(dev);
	if (gsp_dev_verify(gsp)) {
		GSP_ERR("invalidate gsp device show state\n");
		return -ENXIO;
	}

	idle = gsp_dev_is_idle(gsp);

	return scnprintf(buf, PAGE_SIZE, "%s\n", idle == 1 ? "idle" : "busy");
}

static ssize_t core_state_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	/* 1: idle, 0: busy*/
	int idle = 0;
	struct gsp_core *core = NULL;

	core = gsp_core_match(dev);
	if (gsp_core_verify(core)) {
		GSP_ERR("invalidate gsp device show core cnt\n");
		return -ENXIO;
	}

	idle = gsp_core_is_idle(core);

	return scnprintf(buf, PAGE_SIZE, "%s\n", idle == 1 ? "idle" : "busy");
}

static ssize_t total_kcfg_num_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int num = 0;
	struct gsp_core *core = NULL;

	core = gsp_core_match(dev);
	if (gsp_core_verify(core)) {
		GSP_ERR("invalidate gsp device show core cnt\n");
		return -ENXIO;
	}

	num = gsp_core_get_kcfg_num(core);

	return scnprintf(buf, PAGE_SIZE, "%d\n", num);
}

static ssize_t empty_kcfg_num_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int num = 0;
	struct gsp_core *core = NULL;
	struct gsp_workqueue *wq = NULL;

	core = gsp_core_match(dev);
	if (gsp_core_verify(core)) {
		GSP_ERR("invalidate gsp device show core cnt\n");
		return -ENXIO;
	}

	wq = gsp_core_to_workqueue(core);
	num = gsp_workqueue_get_empty_kcfg_num(wq);

	return scnprintf(buf, PAGE_SIZE, "%d\n", num);
}

static ssize_t fill_kcfg_num_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int num = 0;
	struct gsp_core *core = NULL;
	struct gsp_workqueue *wq = NULL;

	core = gsp_core_match(dev);
	if (gsp_core_verify(core)) {
		GSP_ERR("invalidate gsp device show core cnt\n");
		return -ENXIO;
	}

	wq = gsp_core_to_workqueue(core);
	num = gsp_workqueue_get_fill_kcfg_num(wq);

	return scnprintf(buf, PAGE_SIZE, "%d\n", num);
}

static struct device_attribute gsp_dev_attrs[] = {
	__ATTR_RO(core_cnt),
	__ATTR_RO(dev_state),
};

static struct device_attribute gsp_core_attrs[] = {
	__ATTR_RO(core_state),
	__ATTR_RO(total_kcfg_num),
	__ATTR_RO(empty_kcfg_num),
	__ATTR_RO(fill_kcfg_num),
};

int gsp_dev_sysfs_init(struct gsp_dev *gsp)
{
	int ret = -1;
	size_t i, j;
	struct device *dev = NULL;

	dev = gsp_dev_to_device(gsp);
	for (i = 0; i < ARRAY_SIZE(gsp_dev_attrs); i++) {
		ret = device_create_file(dev, &gsp_dev_attrs[i]);
		if (ret < 0) {
			GSP_ERR("create gsp sysfs attribute %s failed: %d\n",
				gsp_dev_attrs[i].attr.name, ret);
			goto err;
		}
		GSP_INFO("create gsp sysfs attribute: %s success\n",
			 gsp_dev_attrs[i].attr.name);
	}

	ret = gsp_core_match_table_create(gsp);
	if (ret) {
		GSP_ERR("gsp core match table create failed\n");
		goto err;
	}

	return 0;

err:
	gsp_core_match_table_destroy();

	for (j = 0; j < i; j++)
		device_remove_file(dev, &gsp_dev_attrs[j]);

	return ret;
}

void gsp_dev_sysfs_destroy(struct gsp_dev *gsp)
{
	size_t i;
	struct device *dev = NULL;

	dev = gsp_dev_to_device(gsp);
	for (i = 0; i < ARRAY_SIZE(gsp_dev_attrs); i++)
		device_remove_file(dev, &gsp_dev_attrs[i]);
}

int gsp_core_sysfs_init(struct gsp_core *core)
{
	int ret = -1;
	size_t i, j;
	struct device *dev = NULL;

	dev = gsp_core_to_device(core);
	for (i = 0; i < ARRAY_SIZE(gsp_core_attrs); i++) {
		ret = device_create_file(dev, &gsp_core_attrs[i]);
		if (ret < 0) {
			GSP_ERR("create core[%d] sysfs attribute: %s failed\n",
				gsp_core_to_id(core),
				gsp_core_attrs[i].attr.name);
			goto err;
		}
		GSP_INFO("create core[%d] sysfs attribute: %s success\n",
			 gsp_core_to_id(core), gsp_core_attrs[i].attr.name);
	}

	return 0;

err:
	for (j = 0; j < i; j++)
		device_remove_file(dev, &gsp_core_attrs[i]);

	return ret;
}

void gsp_core_sysfs_destroy(struct gsp_core *core)
{
	size_t i;
	struct device *dev = NULL;

	dev = gsp_core_to_device(core);
	for (i = 0; i < ARRAY_SIZE(gsp_dev_attrs); i++)
		device_remove_file(dev, &gsp_core_attrs[i]);
}
