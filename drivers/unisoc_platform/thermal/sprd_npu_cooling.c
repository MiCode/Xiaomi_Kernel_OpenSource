// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Spreadtrum Communications Inc.

#define pr_fmt(fmt) "sprd_npu_cooling: " fmt

#include <linux/devfreq_cooling.h>
#include <linux/thermal.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/printk.h>
#include <linux/unisoc_npu_cooling.h>

static struct thermal_cooling_device *npu_cooling;

int npu_cooling_device_register(struct devfreq *npudev)
{
	int ret = 0;

	if (npudev == NULL) {
		pr_err("params is not complete!\n");
		return -ENODEV;
	}
	npu_cooling = devfreq_cooling_em_register(npudev, NULL);

	if (IS_ERR_OR_NULL(npu_cooling))
		ret = PTR_ERR(npu_cooling);

	return ret;
}
EXPORT_SYMBOL_GPL(npu_cooling_device_register);

int npu_cooling_device_unregister(void)
{
	devfreq_cooling_unregister(npu_cooling);
	return 0;
}
EXPORT_SYMBOL_GPL(npu_cooling_device_unregister);

MODULE_DESCRIPTION("sprd npu cooling driver");
MODULE_LICENSE("GPL v2");

