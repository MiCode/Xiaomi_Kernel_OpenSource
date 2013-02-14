/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/module.h>

struct msm_cpr_debug_device {
	struct mutex	debug_mutex;
	struct dentry	*dir;
	int		addr_offset;
	void __iomem	*base;
};

static inline
void write_reg(struct msm_cpr_debug_device *cpr, u32 value)
{
	writel_relaxed(value, cpr->base + cpr->addr_offset);
}

static inline u32 read_reg(struct msm_cpr_debug_device *cpr)
{
	return readl_relaxed(cpr->base + cpr->addr_offset);
}

static bool msm_cpr_debug_addr_is_valid(int addr)
{
	if (addr < 0 || addr > 0x15C) {
		pr_err("CPR register address is invalid: %d\n", addr);
		return false;
	}
	return true;
}

static int msm_cpr_debug_data_set(void *data, u64 val)
{
	struct msm_cpr_debug_device *debugdev = data;
	uint32_t reg = val;

	mutex_lock(&debugdev->debug_mutex);

	if (msm_cpr_debug_addr_is_valid(debugdev->addr_offset))
		write_reg(debugdev, reg);

	mutex_unlock(&debugdev->debug_mutex);
	return 0;
}

static int msm_cpr_debug_data_get(void *data, u64 *val)
{
	struct msm_cpr_debug_device *debugdev = data;
	uint32_t reg;

	mutex_lock(&debugdev->debug_mutex);

	if (msm_cpr_debug_addr_is_valid(debugdev->addr_offset)) {
		reg = read_reg(debugdev);
		*val = reg;
	}
	mutex_unlock(&debugdev->debug_mutex);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_data_fops, msm_cpr_debug_data_get,
			msm_cpr_debug_data_set, "0x%02llX\n");

static int msm_cpr_debug_addr_set(void *data, u64 val)
{
	struct msm_cpr_debug_device *debugdev = data;

	if (msm_cpr_debug_addr_is_valid(val)) {
		mutex_lock(&debugdev->debug_mutex);
		debugdev->addr_offset = val;
		mutex_unlock(&debugdev->debug_mutex);
	}

	return 0;
}

static int msm_cpr_debug_addr_get(void *data, u64 *val)
{
	struct msm_cpr_debug_device *debugdev = data;

	mutex_lock(&debugdev->debug_mutex);

	if (msm_cpr_debug_addr_is_valid(debugdev->addr_offset))
		*val = debugdev->addr_offset;

	mutex_unlock(&debugdev->debug_mutex);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_addr_fops, msm_cpr_debug_addr_get,
			msm_cpr_debug_addr_set, "0x%03llX\n");

int msm_cpr_debug_init(void *data)
{
	char *name = "cpr-debug";
	struct msm_cpr_debug_device *debugdev;
	struct dentry *dir;
	struct dentry *temp;
	int rc;

	debugdev = kzalloc(sizeof(struct msm_cpr_debug_device), GFP_KERNEL);
	if (debugdev == NULL) {
		pr_err("kzalloc failed\n");
		return -ENOMEM;
	}

	dir = debugfs_create_dir(name, NULL);
	if (dir == NULL || IS_ERR(dir)) {
		pr_err("debugfs_create_dir failed: rc=%ld\n", PTR_ERR(dir));
		rc = PTR_ERR(dir);
		goto dir_error;
	}

	temp = debugfs_create_file("address", S_IRUGO | S_IWUSR, dir, debugdev,
				   &debug_addr_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		rc = PTR_ERR(temp);
		goto file_error;
	}

	temp = debugfs_create_file("data", S_IRUGO | S_IWUSR, dir, debugdev,
				   &debug_data_fops);
	if (temp == NULL  || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		rc = PTR_ERR(temp);
		goto file_error;
	}
	debugdev->base = data;
	debugdev->addr_offset = -1;
	debugdev->dir = dir;
	mutex_init(&debugdev->debug_mutex);

	return 0;

file_error:
	debugfs_remove_recursive(dir);
dir_error:
	kfree(debugdev);

	return rc;
}
