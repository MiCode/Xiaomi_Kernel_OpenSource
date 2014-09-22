/*
 * platform_byt_audio.c: Baytrail audio platform data initilization file
 *
 * (C) Copyright 2013 Intel Corporation
 * Author: Omair Md Abudllah <omair.m.abdullah@intel.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>


static int __init byt_audio_platform_init(void)
{
	struct platform_device *pdev;

	pr_debug("%s: Enter.\n", __func__);

	pdev = platform_device_register_simple("hdmi-audio", -1, NULL, 0);

	return 0;
}
device_initcall(byt_audio_platform_init);
