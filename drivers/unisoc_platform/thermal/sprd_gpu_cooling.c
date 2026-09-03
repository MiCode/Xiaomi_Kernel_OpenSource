// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Spreadtrum Communications Inc.

#define pr_fmt(fmt) "sprd_gpu_cooling: " fmt

#include <linux/devfreq_cooling.h>
#include <linux/thermal.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/printk.h>
#include <linux/unisoc_gpu_cooling.h>

static struct thermal_cooling_device *gpu_cooling;

int create_gpu_cooling_device(struct devfreq *gpudev, u64 *mask)
{
	int ret = 0;

	if (gpudev == NULL || mask == NULL) {
		pr_err("params is not complete!\n");
		return -ENODEV;
	}
	gpu_cooling = devfreq_cooling_em_register(gpudev, NULL);

	if (IS_ERR_OR_NULL(gpu_cooling))
		ret = PTR_ERR(gpu_cooling);

	return ret;
}
EXPORT_SYMBOL_GPL(create_gpu_cooling_device);

int destroy_gpu_cooling_device(void)
{
	devfreq_cooling_unregister(gpu_cooling);
	gpu_cooling = NULL;
	return 0;
}
EXPORT_SYMBOL_GPL(destroy_gpu_cooling_device);

MODULE_DESCRIPTION("sprd gpu cooling driver");
MODULE_LICENSE("GPL v2");

