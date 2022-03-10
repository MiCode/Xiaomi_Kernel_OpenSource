// SPDX-License-Identifier: GPL-2.0

/**
 * Copyright (C) Fourier Semiconductor Inc. 2016-2020. All rights reserved.
 * 2020-01-20 File created.
 */

#if defined(CONFIG_FSM_SYSFS)
#include "fsm_public.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/version.h>


static int g_fsm_class_inited;

static ssize_t fsm_re25_calib_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	struct fsm_cal_result result;
	struct fsm_calib *data;
	int len = 0;
	int dev;

	memset(&result, 0, sizeof(result));
	fsm_test_result(&result);
	pr_info("ndev:%d", result.ndev);
	for (dev = 0; dev < result.ndev; dev++) {
		data = &result.data[dev];
		// [addr,re25,errcode]
		pr_info("%02X,%d,%d", data->addr, data->re25, data->errcode);
		len += scnprintf(buf + len, PAGE_SIZE, "[%02X,%d,%d]",
				data->addr, data->re25, data->errcode);
	}
	len += scnprintf(buf + len, PAGE_SIZE, "\n");

	return len;
}

static ssize_t fsm_re25_calib_store(struct class *class,
				struct class_attribute *attr, const char *buf, size_t count)
{
	int value = simple_strtoul(buf, NULL, 0);

	fsm_re25_test(value != 0);

	return count;
}

static ssize_t fsm_f0_calib_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	struct fsm_cal_result result;
	struct fsm_calib *data;
	int len = 0;
	int dev;

	memset(&result, 0, sizeof(result));
	fsm_test_result(&result);
	pr_info("ndev:%d", result.ndev);
	for (dev = 0; dev < result.ndev; dev++) {
		data = &result.data[dev];
		// [addr,f0,errcode]
		pr_info("%02X,%d,%d", data->addr, data->f0, data->errcode);
		len += scnprintf(buf + len, PAGE_SIZE, "[%02X,%d,%d]",
				data->addr, data->f0, data->errcode);
	}
	len += scnprintf(buf + len, PAGE_SIZE, "\n");

	return len;
}

static ssize_t fsm_f0_calib_store(struct class *class,
				struct class_attribute *attr, const char *buf, size_t len)
{
	fsm_f0_test();
	return len;
}

static ssize_t fsm_reg_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%s\n", __func__);
}

static ssize_t fsm_reg_store(struct class *class,
				struct class_attribute *attr, const char *buf, size_t len)
{
	return 0;
}

static ssize_t fsm_dev_info_show(struct class *class,
				struct class_attribute *attr, char *buf)
{
	fsm_version_t version;
	struct preset_file *pfile;
	int dev_count;
	int len = 0;

	fsm_get_version(&version);
	len  = scnprintf(buf + len, PAGE_SIZE, "version: %s\n",
			version.code_version);
	len += scnprintf(buf + len, PAGE_SIZE, "branch : %s\n",
			version.git_branch);
	len += scnprintf(buf + len, PAGE_SIZE, "commit : %s\n",
			version.git_commit);
	len += scnprintf(buf + len, PAGE_SIZE, "date   : %s\n",
			version.code_date);
	pfile = fsm_get_presets();
	dev_count = (pfile ? pfile->hdr.ndev : 0);
	len += scnprintf(buf + len, PAGE_SIZE, "device : %d\n",
			dev_count);

	return len;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0))
static struct class_attribute g_fsm_class_attrs[] = {
	__ATTR(re25, S_IRUGO|S_IWUSR, fsm_re25_calib_show, fsm_re25_calib_store),
	__ATTR(f0, S_IRUGO|S_IWUSR, fsm_f0_calib_show, fsm_f0_calib_store),
	__ATTR(reg, S_IRUGO|S_IWUSR, fsm_reg_show, fsm_reg_store),
	__ATTR(info, S_IRUGO, fsm_dev_info_show, NULL),
	__ATTR_NULL
};

static struct class g_fsm_class = {
	.name = FSM_DRV_NAME,
	.class_attrs = g_fsm_class_attrs,
};

#else
static CLASS_ATTR_RW(fsm_re25_calib);
static CLASS_ATTR_RW(fsm_f0_calib);
static CLASS_ATTR_RW(fsm_reg);
static CLASS_ATTR_RO(fsm_dev_info);

static struct attribute *fsm_class_attrs[] = {
	&class_attr_fsm_re25_calib.attr,
	&class_attr_fsm_f0_calib.attr,
	&class_attr_fsm_reg.attr,
	&class_attr_fsm_dev_info.attr,
	NULL,
};
ATTRIBUTE_GROUPS(fsm_class);

/** Device model classes */
struct class g_fsm_class = {
	.name = FSM_DRV_NAME,
	.class_groups = fsm_class_groups,
};
#endif

int fsm_sysfs_init(struct device *dev)
{
	int ret;

	if (g_fsm_class_inited) {
		return MODULE_INITED;
	}
	// path: sys/class/$(FSM_DRV_NAME)
	ret = class_register(&g_fsm_class);
	if (!ret) {
		g_fsm_class_inited = 1;
	}

	return ret;
}

void fsm_sysfs_deinit(struct device *dev)
{
	class_unregister(&g_fsm_class);
	g_fsm_class_inited = 0;
}
#endif
