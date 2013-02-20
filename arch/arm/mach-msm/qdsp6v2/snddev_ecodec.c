/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <mach/clk.h>
#include <mach/qdsp6v2/audio_dev_ctl.h>
#include <sound/apr_audio.h>
#include <sound/q6afe.h>
#include "snddev_ecodec.h"

#define ECODEC_SAMPLE_RATE 8000

/* Context for each external codec device */
struct snddev_ecodec_state {
	struct snddev_ecodec_data *data;
	u32 sample_rate;
};

/* Global state for the driver */
struct snddev_ecodec_drv_state {
	struct mutex dev_lock;
	int ref_cnt;		/* ensure one rx device at a time */
	struct clk *ecodec_clk;
};

static struct snddev_ecodec_drv_state snddev_ecodec_drv;

struct aux_pcm_state {
	unsigned int dout;
	unsigned int din;
	unsigned int syncout;
	unsigned int clkin_a;
};

static struct aux_pcm_state the_aux_pcm_state;

static int aux_pcm_gpios_request(void)
{
	int rc = 0;

	pr_debug("%s\n", __func__);
	rc = gpio_request(the_aux_pcm_state.dout, "AUX PCM DOUT");
	if (rc < 0) {
		pr_err("%s: GPIO request for AUX PCM DOUT failed\n", __func__);
		return rc;
	}

	rc = gpio_request(the_aux_pcm_state.din, "AUX PCM DIN");
	if (rc < 0) {
		pr_err("%s: GPIO request for AUX PCM DIN failed\n", __func__);
		gpio_free(the_aux_pcm_state.dout);
		return rc;
	}

	rc = gpio_request(the_aux_pcm_state.syncout, "AUX PCM SYNC OUT");
	if (rc < 0) {
		pr_err("%s: GPIO request for AUX PCM SYNC OUT failed\n",
				__func__);
		gpio_free(the_aux_pcm_state.dout);
		gpio_free(the_aux_pcm_state.din);
		return rc;
	}

	rc = gpio_request(the_aux_pcm_state.clkin_a, "AUX PCM CLKIN A");
	if (rc < 0) {
		pr_err("%s: GPIO request for AUX PCM CLKIN A failed\n",
				__func__);
		gpio_free(the_aux_pcm_state.dout);
		gpio_free(the_aux_pcm_state.din);
		gpio_free(the_aux_pcm_state.syncout);
		return rc;
	}

	return rc;
}

static void aux_pcm_gpios_free(void)
{
	pr_debug("%s\n", __func__);
	gpio_free(the_aux_pcm_state.dout);
	gpio_free(the_aux_pcm_state.din);
	gpio_free(the_aux_pcm_state.syncout);
	gpio_free(the_aux_pcm_state.clkin_a);
}

static int get_aux_pcm_gpios(struct platform_device *pdev)
{
	int rc = 0;
	struct resource *res;

	/* Claim all of the GPIOs. */
	res = platform_get_resource_byname(pdev, IORESOURCE_IO, "aux_pcm_dout");
	if (!res) {
		pr_err("%s: failed to get gpio AUX PCM DOUT\n", __func__);
		return -ENODEV;
	}

	the_aux_pcm_state.dout = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO, "aux_pcm_din");
	if (!res) {
		pr_err("%s: failed to get gpio AUX PCM DIN\n", __func__);
		return -ENODEV;
	}

	the_aux_pcm_state.din = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					   "aux_pcm_syncout");
	if (!res) {
		pr_err("%s: failed to get gpio AUX PCM SYNC OUT\n", __func__);
		return -ENODEV;
	}

	the_aux_pcm_state.syncout = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_IO,
					   "aux_pcm_clkin_a");
	if (!res) {
		pr_err("%s: failed to get gpio AUX PCM CLKIN A\n", __func__);
		return -ENODEV;
	}

	the_aux_pcm_state.clkin_a = res->start;

	return rc;
}

static int aux_pcm_probe(struct platform_device *pdev)
{
	int rc = 0;

	rc = get_aux_pcm_gpios(pdev);
	if (rc < 0) {
		pr_err("%s: GPIO configuration failed\n", __func__);
		return -ENODEV;
	}
	return rc;
}

static struct platform_driver aux_pcm_driver = {
	.probe = aux_pcm_probe,
	.driver = { .name = "msm_aux_pcm"}
};

static int snddev_ecodec_open(struct msm_snddev_info *dev_info)
{
	int rc;
	struct snddev_ecodec_drv_state *drv = &snddev_ecodec_drv;
	union afe_port_config afe_config;

	pr_debug("%s\n", __func__);

	mutex_lock(&drv->dev_lock);

	if (dev_info->opened) {
		pr_err("%s: ERROR: %s already opened\n", __func__,
				dev_info->name);
		mutex_unlock(&drv->dev_lock);
		return -EBUSY;
	}

	if (drv->ref_cnt != 0) {
		pr_debug("%s: opened %s\n", __func__, dev_info->name);
		drv->ref_cnt++;
		mutex_unlock(&drv->dev_lock);
		return 0;
	}

	pr_info("%s: opening %s\n", __func__, dev_info->name);

	rc = aux_pcm_gpios_request();
	if (rc < 0) {
		pr_err("%s: GPIO request failed\n", __func__);
		return rc;
	}

	clk_reset(drv->ecodec_clk, CLK_RESET_ASSERT);

	afe_config.pcm.mode = AFE_PCM_CFG_MODE_PCM;
	afe_config.pcm.sync = AFE_PCM_CFG_SYNC_INT;
	afe_config.pcm.frame = AFE_PCM_CFG_FRM_256BPF;
	afe_config.pcm.quant = AFE_PCM_CFG_QUANT_LINEAR_NOPAD;
	afe_config.pcm.slot = 0;
	afe_config.pcm.data = AFE_PCM_CFG_CDATAOE_MASTER;

	rc = afe_open(PCM_RX, &afe_config, ECODEC_SAMPLE_RATE);
	if (rc < 0) {
		pr_err("%s: afe open failed for PCM_RX\n", __func__);
		goto err_rx_afe;
	}

	rc = afe_open(PCM_TX, &afe_config, ECODEC_SAMPLE_RATE);
	if (rc < 0) {
		pr_err("%s: afe open failed for PCM_TX\n", __func__);
		goto err_tx_afe;
	}

	rc = clk_set_rate(drv->ecodec_clk, 2048000);
	if (rc < 0) {
		pr_err("%s: clk_set_rate failed\n", __func__);
		goto err_clk;
	}

	clk_prepare_enable(drv->ecodec_clk);

	clk_reset(drv->ecodec_clk, CLK_RESET_DEASSERT);

	drv->ref_cnt++;
	mutex_unlock(&drv->dev_lock);

	return 0;

err_clk:
	afe_close(PCM_TX);
err_tx_afe:
	afe_close(PCM_RX);
err_rx_afe:
	aux_pcm_gpios_free();
	mutex_unlock(&drv->dev_lock);
	return -ENODEV;
}

int snddev_ecodec_close(struct msm_snddev_info *dev_info)
{
	struct snddev_ecodec_drv_state *drv = &snddev_ecodec_drv;

	pr_debug("%s: closing %s\n", __func__, dev_info->name);

	mutex_lock(&drv->dev_lock);

	if (!dev_info->opened) {
		pr_err("%s: ERROR: %s is not opened\n", __func__,
				dev_info->name);
		mutex_unlock(&drv->dev_lock);
		return -EPERM;
	}

	drv->ref_cnt--;

	if (drv->ref_cnt == 0) {

		pr_info("%s: closing all devices\n", __func__);

		clk_disable_unprepare(drv->ecodec_clk);
		aux_pcm_gpios_free();

		afe_close(PCM_RX);
		afe_close(PCM_TX);
	}

	mutex_unlock(&drv->dev_lock);

	return 0;
}

int snddev_ecodec_set_freq(struct msm_snddev_info *dev_info, u32 rate)
{
	int rc = 0;

	if (!dev_info) {
		rc = -EINVAL;
		goto error;
	}
	return ECODEC_SAMPLE_RATE;

error:
	return rc;
}

static int snddev_ecodec_probe(struct platform_device *pdev)
{
	int rc = 0;
	struct snddev_ecodec_data *pdata;
	struct msm_snddev_info *dev_info;
	struct snddev_ecodec_state *ecodec;

	if (!pdev || !pdev->dev.platform_data) {
		printk(KERN_ALERT "Invalid caller\n");
		rc = -1;
		goto error;
	}
	pdata = pdev->dev.platform_data;

	ecodec = kzalloc(sizeof(struct snddev_ecodec_state), GFP_KERNEL);
	if (!ecodec) {
		rc = -ENOMEM;
		goto error;
	}

	dev_info = kzalloc(sizeof(struct msm_snddev_info), GFP_KERNEL);
	if (!dev_info) {
		kfree(ecodec);
		rc = -ENOMEM;
		goto error;
	}

	dev_info->name = pdata->name;
	dev_info->copp_id = pdata->copp_id;
	dev_info->private_data = (void *)ecodec;
	dev_info->dev_ops.open = snddev_ecodec_open;
	dev_info->dev_ops.close = snddev_ecodec_close;
	dev_info->dev_ops.set_freq = snddev_ecodec_set_freq;
	dev_info->dev_ops.enable_sidetone = NULL;
	dev_info->capability = pdata->capability;
	dev_info->opened = 0;

	msm_snddev_register(dev_info);

	ecodec->data = pdata;
	ecodec->sample_rate = ECODEC_SAMPLE_RATE;	/* Default to 8KHz */
error:
	return rc;
}

struct platform_driver snddev_ecodec_driver = {
	.probe = snddev_ecodec_probe,
	.driver = {.name = "msm_snddev_ecodec"}
};

int __init snddev_ecodec_init(void)
{
	int rc = 0;
	struct snddev_ecodec_drv_state *drv = &snddev_ecodec_drv;

	mutex_init(&drv->dev_lock);
	drv->ref_cnt = 0;

	drv->ecodec_clk = clk_get_sys(NULL, "pcm_clk");
	if (IS_ERR(drv->ecodec_clk)) {
		pr_err("%s: could not get pcm_clk\n", __func__);
		return PTR_ERR(drv->ecodec_clk);
	}

	rc = platform_driver_register(&aux_pcm_driver);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: platform_driver_register for aux pcm failed\n",
				__func__);
		goto error_aux_pcm_platform_driver;
	}

	rc = platform_driver_register(&snddev_ecodec_driver);
	if (IS_ERR_VALUE(rc)) {
		pr_err("%s: platform_driver_register for ecodec failed\n",
				__func__);
		goto error_ecodec_platform_driver;
	}

	return 0;

error_ecodec_platform_driver:
	platform_driver_unregister(&aux_pcm_driver);
error_aux_pcm_platform_driver:
	clk_put(drv->ecodec_clk);

	pr_err("%s: encounter error\n", __func__);
	return -ENODEV;
}

device_initcall(snddev_ecodec_init);

MODULE_DESCRIPTION("ECodec Sound Device driver");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
