/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <mach/qdsp6v2/audio_dev_ctl.h>
#include <sound/q6afe.h>
#include <linux/slab.h>
#include "snddev_virtual.h"

static DEFINE_MUTEX(snddev_virtual_lock);

static int snddev_virtual_open(struct msm_snddev_info *dev_info)
{
	int rc = 0;

	pr_debug("%s\n", __func__);

	mutex_lock(&snddev_virtual_lock);

	if (!dev_info)  {
		pr_err("%s: NULL dev_info\n", __func__);

		rc = -EINVAL;
		goto done;
	}

	if (!dev_info->opened) {
		rc = afe_start_pseudo_port(dev_info->copp_id);
	} else {
		pr_err("%s: Pseudo port 0x%x is already open\n",
		       __func__, dev_info->copp_id);

		rc = -EBUSY;
	}

done:
	mutex_unlock(&snddev_virtual_lock);

	return rc;
}

static int snddev_virtual_close(struct msm_snddev_info *dev_info)
{
	int rc = 0;

	pr_debug("%s\n", __func__);

	mutex_lock(&snddev_virtual_lock);

	if (!dev_info) {
		pr_err("%s: NULL dev_info\n", __func__);

		rc = -EINVAL;
		goto done;
	}

	if (dev_info->opened) {
		rc = afe_stop_pseudo_port(dev_info->copp_id);
	} else {
		pr_err("%s: Pseudo port 0x%x is not open\n",
		       __func__, dev_info->copp_id);

		rc = -EPERM;
	}

done:
	mutex_unlock(&snddev_virtual_lock);

	return rc;
}

static int snddev_virtual_set_freq(struct msm_snddev_info *dev_info, u32 rate)
{
	int rc = 0;

	if (!dev_info)
		rc = -EINVAL;

	return rate;
}

static int snddev_virtual_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct snddev_virtual_data *pdata;
	struct msm_snddev_info *dev_info;

	pr_debug("%s\n", __func__);

	if (!pdev || !pdev->dev.platform_data) {
		pr_err("%s: Invalid caller\n", __func__);

		rc = -EPERM;
		goto done;
	}

	pdata = pdev->dev.platform_data;

	dev_info = kmalloc(sizeof(struct msm_snddev_info), GFP_KERNEL);
	if (!dev_info) {
		pr_err("%s: Out of memory\n", __func__);

		rc = -ENOMEM;
		goto done;
	}

	dev_info->name = pdata->name;
	dev_info->copp_id = pdata->copp_id;
	dev_info->private_data = (void *) NULL;
	dev_info->dev_ops.open = snddev_virtual_open;
	dev_info->dev_ops.close = snddev_virtual_close;
	dev_info->dev_ops.set_freq = snddev_virtual_set_freq;
	dev_info->capability = pdata->capability;
	dev_info->sample_rate = 48000;
	dev_info->opened = 0;
	dev_info->sessions = 0;

	msm_snddev_register(dev_info);

done:
	return rc;
}

static int snddev_virtual_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver snddev_virtual_driver = {
	.probe = snddev_virtual_probe,
	.remove = snddev_virtual_remove,
	.driver = { .name = "snddev_virtual" }
};

static int __init snddev_virtual_init(void)
{
	int rc = 0;

	pr_debug("%s\n", __func__);

	rc = platform_driver_register(&snddev_virtual_driver);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: Platform driver register failure\n", __func__);

		return -ENODEV;
	}

	return 0;
}

static void __exit snddev_virtual_exit(void)
{
	platform_driver_unregister(&snddev_virtual_driver);

	return;
}

module_init(snddev_virtual_init);
module_exit(snddev_virtual_exit);

MODULE_DESCRIPTION("Virtual Sound Device driver");
MODULE_LICENSE("GPL v2");
