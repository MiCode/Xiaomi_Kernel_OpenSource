/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/types.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <mach/qdsp6v2/audio_dev_ctl.h>
#include <mach/debug_mm.h>
#include <sound/q6afe.h>
#include <sound/apr_audio.h>
#include "snddev_hdmi.h"

static DEFINE_MUTEX(snddev_hdmi_lock);
static int snddev_hdmi_active;

static int snddev_hdmi_open(struct msm_snddev_info *dev_info)
{
	int rc = 0;
	union afe_port_config afe_config;
	struct snddev_hdmi_data *snddev_hdmi_data;

	if (!dev_info) {
		pr_err("msm_snddev_info is null\n");
		return -EINVAL;
	}

	snddev_hdmi_data = dev_info->private_data;

	mutex_lock(&snddev_hdmi_lock);

	if (snddev_hdmi_active) {
		pr_err("HDMI snddev already active\n");
		mutex_unlock(&snddev_hdmi_lock);
		return -EBUSY;
	}

	if (snddev_hdmi_data->on_apps) {
		snddev_hdmi_active = 1;
		pr_debug("%s open done\n", dev_info->name);
		mutex_unlock(&snddev_hdmi_lock);
		return 0;
	}

	afe_config.hdmi.channel_mode = snddev_hdmi_data->channel_mode;
	afe_config.hdmi.bitwidth = 16;
	afe_config.hdmi.data_type = 0;
	rc = afe_open(snddev_hdmi_data->copp_id, &afe_config,
		dev_info->sample_rate);

	if (rc < 0) {
		pr_err("afe_open failed\n");
		mutex_unlock(&snddev_hdmi_lock);
		return -EINVAL;
	}
	snddev_hdmi_active = 1;

	pr_debug("%s open done\n", dev_info->name);

	mutex_unlock(&snddev_hdmi_lock);

	return 0;
}

static int snddev_hdmi_close(struct msm_snddev_info *dev_info)
{

	struct snddev_hdmi_data *snddev_hdmi_data;

	if (!dev_info) {
		pr_err("msm_snddev_info is null\n");
		return -EINVAL;
	}

	snddev_hdmi_data = dev_info->private_data;

	if (!dev_info->opened) {
		pr_err("calling close device with out opening the"
		       " device\n");
		return -EPERM;
	}
	mutex_lock(&snddev_hdmi_lock);

	if (!snddev_hdmi_active) {
		pr_err("HDMI snddev not active\n");
		mutex_unlock(&snddev_hdmi_lock);
		return -EPERM;
	}
	snddev_hdmi_active = 0;

	if (snddev_hdmi_data->on_apps) {
		pr_debug("%s Closed\n", dev_info->name);

		mutex_unlock(&snddev_hdmi_lock);
		return 0;
	}


	afe_close(HDMI_RX);

	pr_debug("%s closed\n", dev_info->name);
	mutex_unlock(&snddev_hdmi_lock);

	return 0;
}

static int snddev_hdmi_set_freq(struct msm_snddev_info *dev_info, u32 req_freq)
{
	if (req_freq != 48000) {
		pr_debug("Unsupported Frequency:%d\n", req_freq);
		return -EINVAL;
	}
	return 48000;
}

static int snddev_hdmi_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct snddev_hdmi_data *pdata;
	struct msm_snddev_info *dev_info;

	if (!pdev || !pdev->dev.platform_data) {
		printk(KERN_ALERT "Invalid caller\n");
		return -ENODEV;
	}

	pdata = pdev->dev.platform_data;
	if (!(pdata->capability & SNDDEV_CAP_RX)) {
		pr_err("invalid device data either RX or TX\n");
		return -ENODEV;
	}

	dev_info = kzalloc(sizeof(struct msm_snddev_info), GFP_KERNEL);
	if (!dev_info) {
		pr_err("unable to allocate memeory for msm_snddev_info\n");
		return -ENOMEM;
	}

	dev_info->name = pdata->name;
	dev_info->copp_id = pdata->copp_id;
	dev_info->acdb_id = pdata->acdb_id;
	dev_info->private_data = (void *)pdata;
	dev_info->dev_ops.open = snddev_hdmi_open;
	dev_info->dev_ops.close = snddev_hdmi_close;
	dev_info->dev_ops.set_freq = snddev_hdmi_set_freq;
	dev_info->capability = pdata->capability;
	dev_info->opened = 0;
	msm_snddev_register(dev_info);
	dev_info->sample_rate = pdata->default_sample_rate;

	pr_debug("probe done for %s\n", pdata->name);
	return rc;
}

static struct platform_driver snddev_hdmi_driver = {
	.probe = snddev_hdmi_probe,
	.driver = {.name = "snddev_hdmi"}
};

static int __init snddev_hdmi_init(void)
{
	s32 rc;

	rc = platform_driver_register(&snddev_hdmi_driver);
	if (IS_ERR_VALUE(rc)) {

		pr_err("platform_driver_register failed.\n");
		goto error_platform_driver;
	}

	pr_debug("snddev_hdmi_init : done\n");

	return 0;

error_platform_driver:

	pr_err("encounterd error\n");
	return -ENODEV;
}

module_init(snddev_hdmi_init);

MODULE_DESCRIPTION("HDMI Sound Device driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
