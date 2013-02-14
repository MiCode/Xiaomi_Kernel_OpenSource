/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
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
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <mach/qdsp5v2/audio_dev_ctl.h>
#include <mach/qdsp5v2/audio_interct.h>
#include <mach/qdsp5v2/mi2s.h>
#include <mach/qdsp5v2/afe.h>
#include <mach/debug_mm.h>
#include <mach/qdsp5v2/snddev_mi2s.h>

/* Global state for the driver */
struct snddev_mi2s_drv_state {
	struct clk *mclk;
	struct clk *sclk;
	struct mutex lock;
	u8 sd_lines_used;
	u8 clocks_enabled;
};

static struct snddev_mi2s_drv_state snddev_mi2s_drv;

static int snddev_mi2s_open_tx(struct msm_snddev_info *dev_info)
{
	u8 channels;
	struct msm_afe_config afe_config;
	int rc;
	struct snddev_mi2s_data *snddev_mi2s_data = dev_info->private_data;

	MM_DBG("%s: channel_mode = %u sd_line_mask = 0x%x "
		"default_sample_rate = %u\n", __func__,
		snddev_mi2s_data->channel_mode, snddev_mi2s_data->sd_lines,
		snddev_mi2s_data->default_sample_rate);

	if (snddev_mi2s_data->channel_mode == 2) {
		channels = MI2S_CHAN_STEREO;
	} else {
		MM_ERR("%s: Invalid number of channels = %u\n", __func__,
			snddev_mi2s_data->channel_mode);
		return -EINVAL;
	}

	/* Set MI2S */
	mi2s_set_hdmi_input_path(channels, WT_16_BIT,
				 snddev_mi2s_data->sd_lines);

	afe_config.sample_rate = snddev_mi2s_data->default_sample_rate / 1000;
	afe_config.channel_mode = snddev_mi2s_data->channel_mode;
	afe_config.volume = AFE_VOLUME_UNITY;
	rc = afe_enable(AFE_HW_PATH_MI2S_TX, &afe_config);

	if (IS_ERR_VALUE(rc)) {
		MM_ERR("%s: afe_enable failed for AFE_HW_PATH_MI2S_TX "
		       "rc = %d\n", __func__, rc);
		return -ENODEV;
	}

	/* Enable audio path */
	if (snddev_mi2s_data->route)
		snddev_mi2s_data->route();

	return 0;
}

static int snddev_mi2s_open_rx(struct msm_snddev_info *dev_info)
{
	int rc;
	struct msm_afe_config afe_config;
	u8 channels;
	struct snddev_mi2s_data *snddev_mi2s_data = dev_info->private_data;

	MM_DBG("%s: channel_mode = %u sd_line_mask = 0x%x "
		"default_sample_rate = %u\n", __func__,
		snddev_mi2s_data->channel_mode, snddev_mi2s_data->sd_lines,
		snddev_mi2s_data->default_sample_rate);

	if (snddev_mi2s_data->channel_mode == 2)
		channels = MI2S_CHAN_STEREO;
	else if (snddev_mi2s_data->channel_mode == 4)
		channels = MI2S_CHAN_4CHANNELS;
	else if (snddev_mi2s_data->channel_mode == 6)
		channels = MI2S_CHAN_6CHANNELS;
	else if (snddev_mi2s_data->channel_mode == 8)
		channels = MI2S_CHAN_8CHANNELS;
	else
		channels = MI2S_CHAN_MONO_RAW;

	/* Set MI2S */
	mi2s_set_hdmi_output_path(channels, WT_16_BIT,
				  snddev_mi2s_data->sd_lines);

	/* Start AFE */
	afe_config.sample_rate = snddev_mi2s_data->default_sample_rate / 1000;
	afe_config.channel_mode = snddev_mi2s_data->channel_mode;
	afe_config.volume = AFE_VOLUME_UNITY;
	rc = afe_enable(AFE_HW_PATH_MI2S_RX, &afe_config);

	if (IS_ERR_VALUE(rc)) {
		MM_ERR("%s: encounter error\n", __func__);
		return -ENODEV;
	}

	/* Enable audio path */
	if (snddev_mi2s_data->route)
		snddev_mi2s_data->route();

	MM_DBG("%s: enabled %s \n", __func__, snddev_mi2s_data->name);

	return 0;
}

static int snddev_mi2s_open(struct msm_snddev_info *dev_info)
{
	int rc = 0;
	struct snddev_mi2s_drv_state *drv = &snddev_mi2s_drv;
	u32 dir;
	struct snddev_mi2s_data *snddev_mi2s_data = dev_info->private_data;

	if (!dev_info) {
		MM_ERR("%s:  msm_snddev_info is null \n", __func__);
		return -EINVAL;
	}

	mutex_lock(&drv->lock);

	if (drv->sd_lines_used & snddev_mi2s_data->sd_lines) {
		MM_ERR("%s: conflict in SD data line. can not use the device\n",
		       __func__);
		mutex_unlock(&drv->lock);
		return -EBUSY;
	}

	if (!drv->clocks_enabled) {

		rc = mi2s_config_clk_gpio();
		if (rc) {
			MM_ERR("%s: mi2s GPIO config failed for %s\n",
			       __func__, snddev_mi2s_data->name);
			mutex_unlock(&drv->lock);
			return -EIO;
		}
		clk_prepare_enable(drv->mclk);
		clk_prepare_enable(drv->sclk);
		drv->clocks_enabled = 1;
		MM_DBG("%s: clks enabled\n", __func__);
	} else
		MM_DBG("%s: clks already enabled\n", __func__);

	if (snddev_mi2s_data->capability & SNDDEV_CAP_RX) {

		dir = DIR_RX;
		rc = mi2s_config_data_gpio(dir, snddev_mi2s_data->sd_lines);

		if (rc) {
			rc = -EIO;
			MM_ERR("%s: mi2s GPIO config failed for %s\n",
			       __func__, snddev_mi2s_data->name);
			goto mi2s_data_gpio_failure;
		}

		MM_DBG("%s: done gpio config rx SD lines\n", __func__);

		rc = snddev_mi2s_open_rx(dev_info);

		if (IS_ERR_VALUE(rc)) {
			MM_ERR(" snddev_mi2s_open_rx failed \n");
			goto mi2s_cleanup_open;
		}

		drv->sd_lines_used |= snddev_mi2s_data->sd_lines;

		MM_DBG("%s: sd_lines_used = 0x%x\n", __func__,
			drv->sd_lines_used);
		mutex_unlock(&drv->lock);

	} else {
		dir = DIR_TX;
		rc = mi2s_config_data_gpio(dir, snddev_mi2s_data->sd_lines);

		if (rc) {
			rc = -EIO;
			MM_ERR("%s: mi2s GPIO config failed for %s\n",
			       __func__, snddev_mi2s_data->name);
			goto mi2s_data_gpio_failure;
		}
		MM_DBG("%s: done data line gpio config for %s\n",
			__func__, snddev_mi2s_data->name);

		rc = snddev_mi2s_open_tx(dev_info);

		if (IS_ERR_VALUE(rc)) {
			MM_ERR(" snddev_mi2s_open_tx failed \n");
			goto mi2s_cleanup_open;
		}

		drv->sd_lines_used |= snddev_mi2s_data->sd_lines;
		MM_DBG("%s: sd_lines_used = 0x%x\n", __func__,
			drv->sd_lines_used);
		mutex_unlock(&drv->lock);
	}

	return 0;

mi2s_cleanup_open:
	mi2s_unconfig_data_gpio(dir, snddev_mi2s_data->sd_lines);

	/* Disable audio path */
	if (snddev_mi2s_data->deroute)
		snddev_mi2s_data->deroute();

mi2s_data_gpio_failure:
	if (!drv->sd_lines_used) {
		clk_disable_unprepare(drv->sclk);
		clk_disable_unprepare(drv->mclk);
		drv->clocks_enabled = 0;
		mi2s_unconfig_clk_gpio();
	}
	mutex_unlock(&drv->lock);
	return rc;
}

static int snddev_mi2s_close(struct msm_snddev_info *dev_info)
{
	struct snddev_mi2s_drv_state *drv = &snddev_mi2s_drv;
	int dir;
	struct snddev_mi2s_data *snddev_mi2s_data = dev_info->private_data;

	if (!dev_info) {
		MM_ERR("%s:  msm_snddev_info is null \n", __func__);
		return -EINVAL;
	}

	if (!dev_info->opened) {
		MM_ERR(" %s: calling close device with out opening the"
		       " device \n", __func__);
		return -EIO;
	}

	mutex_lock(&drv->lock);

	drv->sd_lines_used &= ~snddev_mi2s_data->sd_lines;

	MM_DBG("%s: sd_lines in use = 0x%x\n", __func__, drv->sd_lines_used);

	if (snddev_mi2s_data->capability & SNDDEV_CAP_RX) {
		dir = DIR_RX;
		afe_disable(AFE_HW_PATH_MI2S_RX);
	} else {
		dir = DIR_TX;
		afe_disable(AFE_HW_PATH_MI2S_TX);
	}

	mi2s_unconfig_data_gpio(dir, snddev_mi2s_data->sd_lines);

	if (!drv->sd_lines_used) {
		clk_disable_unprepare(drv->sclk);
		clk_disable_unprepare(drv->mclk);
		drv->clocks_enabled = 0;
		mi2s_unconfig_clk_gpio();
	}

	/* Disable audio path */
	if (snddev_mi2s_data->deroute)
		snddev_mi2s_data->deroute();

	mutex_unlock(&drv->lock);

	return 0;
}

static int snddev_mi2s_set_freq(struct msm_snddev_info *dev_info, u32 req_freq)
{
	if (req_freq != 48000) {
		MM_DBG("%s: Unsupported Frequency:%d\n", __func__, req_freq);
		return -EINVAL;
	}
	return 48000;
}

static int snddev_mi2s_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct snddev_mi2s_data *pdata;
	struct msm_snddev_info *dev_info;

	if (!pdev || !pdev->dev.platform_data) {
		printk(KERN_ALERT "Invalid caller \n");
		return -ENODEV;
	}

	pdata = pdev->dev.platform_data;
	if ((pdata->capability & SNDDEV_CAP_RX) &&
	    (pdata->capability & SNDDEV_CAP_TX)) {
		MM_ERR("%s: invalid device data either RX or TX\n", __func__);
		return -ENODEV;
	}

	dev_info = kzalloc(sizeof(struct msm_snddev_info), GFP_KERNEL);
	if (!dev_info) {
		MM_ERR("%s: uneable to allocate memeory for msm_snddev_info \n",
		       __func__);

		return -ENOMEM;
	}

	dev_info->name = pdata->name;
	dev_info->copp_id = pdata->copp_id;
	dev_info->acdb_id = pdata->acdb_id;
	dev_info->private_data = (void *)pdata;
	dev_info->dev_ops.open = snddev_mi2s_open;
	dev_info->dev_ops.close = snddev_mi2s_close;
	dev_info->dev_ops.set_freq = snddev_mi2s_set_freq;
	dev_info->capability = pdata->capability;
	dev_info->opened = 0;
	msm_snddev_register(dev_info);
	dev_info->sample_rate = pdata->default_sample_rate;

	MM_DBG("%s: probe done for %s\n", __func__, pdata->name);
	return rc;
}

static int snddev_mi2s_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver snddev_mi2s_driver = {
	.probe = snddev_mi2s_probe,
	.remove = snddev_mi2s_remove,
	.driver = {.name = "snddev_mi2s"}
};

static int __init snddev_mi2s_init(void)
{
	s32 rc;
	struct snddev_mi2s_drv_state *drv = &snddev_mi2s_drv;

	rc = platform_driver_register(&snddev_mi2s_driver);
	if (IS_ERR_VALUE(rc)) {

		MM_ERR("%s: platform_driver_register failed  \n", __func__);
		goto error_platform_driver;
	}

	drv->mclk = clk_get(NULL, "mi2s_m_clk");
	if (IS_ERR(drv->mclk)) {
		MM_ERR("%s:  clk_get mi2s_mclk failed  \n", __func__);
		goto error_mclk;
	}

	drv->sclk = clk_get(NULL, "mi2s_s_clk");
	if (IS_ERR(drv->sclk)) {
		MM_ERR("%s:  clk_get mi2s_sclk failed  \n", __func__);

		goto error_sclk;
	}

	mutex_init(&drv->lock);

	MM_DBG("snddev_mi2s_init : done \n");

	return 0;

error_sclk:
	clk_put(drv->mclk);
error_mclk:
	platform_driver_unregister(&snddev_mi2s_driver);
error_platform_driver:

	MM_ERR("%s: encounter error\n", __func__);
	return -ENODEV;
}

static void __exit snddev_mi2s_exit(void)
{
	struct snddev_mi2s_drv_state *drv = &snddev_mi2s_drv;

	platform_driver_unregister(&snddev_mi2s_driver);

	clk_put(drv->sclk);
	clk_put(drv->mclk);
	return;
}

module_init(snddev_mi2s_init);
module_exit(snddev_mi2s_exit);

MODULE_DESCRIPTION("mi2s Sound Device driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
