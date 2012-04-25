/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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


#include <linux/clk.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/mfd/pmic8058.h>
#include <linux/mfd/pmic8901.h>
#include <linux/platform_device.h>
#include <mach/board.h>
#include <mach/mpp.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/dai.h>
#include "msm8660-pcm.h"
#include "../codecs/timpani.h"

#define PM8058_GPIO_BASE			NR_MSM_GPIOS
#define PM8901_GPIO_BASE			(PM8058_GPIO_BASE + \
						PM8058_GPIOS + PM8058_MPPS)
#define PM8901_GPIO_PM_TO_SYS(pm_gpio)		(pm_gpio + PM8901_GPIO_BASE)
#define GPIO_EXPANDER_GPIO_BASE \
	(PM8901_GPIO_BASE + PM8901_MPPS)

static struct clk *rx_osr_clk;
static struct clk *rx_bit_clk;
static struct clk *tx_osr_clk;
static struct clk *tx_bit_clk;

static int rx_hw_param_status;
static int tx_hw_param_status;
/* Platform specific logic */

static int timpani_rx_route_enable(void)
{
	int ret = 0;
	pr_debug("%s\n", __func__);
	ret = gpio_request(109, "I2S_Clock");
	if (ret != 0) {
		pr_err("%s: I2s clk gpio 109 request"
			"failed\n", __func__);
		return ret;
	}
	return ret;
}

static int timpani_rx_route_disable(void)
{
	int ret = 0;
	pr_debug("%s\n", __func__);
	gpio_free(109);
	return ret;
}


#define GPIO_CLASS_D1_EN (GPIO_EXPANDER_GPIO_BASE + 0)
#define PM8901_MPP_3 (2) /* PM8901 MPP starts from 0 */
static void config_class_d1_gpio(int enable)
{
	int rc;

	if (enable) {
		rc = gpio_request(GPIO_CLASS_D1_EN, "CLASSD1_EN");
		if (rc) {
			pr_err("%s: spkr pamp gpio %d request"
			"failed\n", __func__, GPIO_CLASS_D1_EN);
			return;
		}
		gpio_direction_output(GPIO_CLASS_D1_EN, 1);
		gpio_set_value_cansleep(GPIO_CLASS_D1_EN, 1);
	} else {
		gpio_set_value_cansleep(GPIO_CLASS_D1_EN, 0);
		gpio_free(GPIO_CLASS_D1_EN);
	}
}

static void config_class_d0_gpio(int enable)
{
	int rc;

	if (enable) {
		rc = pm8901_mpp_config_digital_out(PM8901_MPP_3,
			PM8901_MPP_DIG_LEVEL_MSMIO, 1);

		if (rc) {
			pr_err("%s: CLASS_D0_EN failed\n", __func__);
			return;
		}

		rc = gpio_request(PM8901_GPIO_PM_TO_SYS(PM8901_MPP_3),
			"CLASSD0_EN");

		if (rc) {
			pr_err("%s: spkr pamp gpio pm8901 mpp3 request"
			"failed\n", __func__);
			pm8901_mpp_config_digital_out(PM8901_MPP_3,
			PM8901_MPP_DIG_LEVEL_MSMIO, 0);
			return;
		}

		gpio_direction_output(PM8901_GPIO_PM_TO_SYS(PM8901_MPP_3), 1);
		gpio_set_value_cansleep(PM8901_GPIO_PM_TO_SYS(PM8901_MPP_3), 1);

	} else {
		pm8901_mpp_config_digital_out(PM8901_MPP_3,
		PM8901_MPP_DIG_LEVEL_MSMIO, 0);
		gpio_set_value_cansleep(PM8901_GPIO_PM_TO_SYS(PM8901_MPP_3), 0);
		gpio_free(PM8901_GPIO_PM_TO_SYS(PM8901_MPP_3));
	}
}

static void timpani_poweramp_on(void)
{

	pr_debug("%s: enable stereo spkr amp\n", __func__);
	timpani_rx_route_enable();
	config_class_d0_gpio(1);
	config_class_d1_gpio(1);
}

static void timpani_poweramp_off(void)
{

	pr_debug("%s: disable stereo spkr amp\n", __func__);
	timpani_rx_route_disable();
	config_class_d0_gpio(0);
	config_class_d1_gpio(0);
}

static int msm8660_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	int rate = params_rate(params);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (rx_hw_param_status)
			return 0;
		clk_set_rate(rx_osr_clk, rate * 256);
		rx_hw_param_status++;
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		if (tx_hw_param_status)
			return 0;
		clk_set_rate(tx_osr_clk, rate * 256);
		tx_hw_param_status++;
	}
	return 0;
}

static int msm8660_startup(struct snd_pcm_substream *substream)
{
	int ret = 0;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		rx_osr_clk = clk_get(NULL, "i2s_spkr_osr_clk");
		if (IS_ERR(rx_osr_clk)) {
			pr_debug("Failed to get i2s_spkr_osr_clk\n");
			return PTR_ERR(rx_osr_clk);
		}
		/* Master clock OSR 256 */
		/* Initially set to Lowest sample rate Needed */
		clk_set_rate(rx_osr_clk, 8000 * 256);
		ret = clk_prepare_enable(rx_osr_clk);
		if (ret != 0) {
			pr_debug("Unable to enable i2s_spkr_osr_clk\n");
			clk_put(rx_osr_clk);
			return ret;
		}
		rx_bit_clk = clk_get(NULL, "i2s_spkr_bit_clk");
		if (IS_ERR(rx_bit_clk)) {
			pr_debug("Failed to get i2s_spkr_bit_clk\n");
			clk_disable_unprepare(rx_osr_clk);
			clk_put(rx_osr_clk);
			return PTR_ERR(rx_bit_clk);
		}
		clk_set_rate(rx_bit_clk, 8);
		ret = clk_prepare_enable(rx_bit_clk);
		if (ret != 0) {
			pr_debug("Unable to enable i2s_spkr_bit_clk\n");
			clk_put(rx_bit_clk);
			clk_disable_unprepare(rx_osr_clk);
			clk_put(rx_osr_clk);
			return ret;
		}
		timpani_poweramp_on();
		msleep(30);
		/* End of platform specific logic */
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		tx_osr_clk = clk_get(NULL, "i2s_mic_osr_clk");
		if (IS_ERR(tx_osr_clk)) {
			pr_debug("Failed to get i2s_mic_osr_clk\n");
			return PTR_ERR(tx_osr_clk);
		}
		/* Master clock OSR 256 */
		clk_set_rate(tx_osr_clk, 8000 * 256);
		ret = clk_prepare_enable(tx_osr_clk);
		if (ret != 0) {
			pr_debug("Unable to enable i2s_mic_osr_clk\n");
			clk_put(tx_osr_clk);
			return ret;
		}
		tx_bit_clk = clk_get(NULL, "i2s_mic_bit_clk");
		if (IS_ERR(tx_bit_clk)) {
			pr_debug("Failed to get i2s_mic_bit_clk\n");
			clk_disable_unprepare(tx_osr_clk);
			clk_put(tx_osr_clk);
			return PTR_ERR(tx_bit_clk);
		}
		clk_set_rate(tx_bit_clk, 8);
		ret = clk_prepare_enable(tx_bit_clk);
		if (ret != 0) {
			pr_debug("Unable to enable i2s_mic_bit_clk\n");
			clk_put(tx_bit_clk);
			clk_disable_unprepare(tx_osr_clk);
			clk_put(tx_osr_clk);
			return ret;
		}
		msm_snddev_enable_dmic_power();
		msleep(30);
	}
	return ret;
}

/*
 * TODO: rx/tx_hw_param_status should be a counter in the below code
 * when driver starts supporting mutisession else setting it to 0
 * will stop audio in all sessions.
 */
static void msm8660_shutdown(struct snd_pcm_substream *substream)
{
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		rx_hw_param_status = 0;
		timpani_poweramp_off();
		msleep(30);
		if (rx_bit_clk) {
			clk_disable_unprepare(rx_bit_clk);
			clk_put(rx_bit_clk);
			rx_bit_clk = NULL;
		}
		if (rx_osr_clk) {
			clk_disable_unprepare(rx_osr_clk);
			clk_put(rx_osr_clk);
			rx_osr_clk = NULL;
		}
	} else if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		tx_hw_param_status = 0;
		msm_snddev_disable_dmic_power();
		msleep(30);
		if (tx_bit_clk) {
			clk_disable_unprepare(tx_bit_clk);
			clk_put(tx_bit_clk);
			tx_bit_clk = NULL;
		}
		if (tx_osr_clk) {
			clk_disable_unprepare(tx_osr_clk);
			clk_put(tx_osr_clk);
			tx_osr_clk = NULL;
		}
	}
}

static struct snd_soc_ops machine_ops  = {
	.startup	= msm8660_startup,
	.shutdown	= msm8660_shutdown,
	.hw_params	= msm8660_hw_params,
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm8660_dai[] = {
	{
		.name		= "Audio Rx",
		.stream_name	= "Audio Rx",
		.cpu_dai	= &msm_cpu_dai[0],
		.codec_dai	= &timpani_codec_dai[0],
		.ops		= &machine_ops,
	},
	{
		.name		= "Audio Tx",
		.stream_name	= "Audio Tx",
		.cpu_dai	= &msm_cpu_dai[5],
		.codec_dai	= &timpani_codec_dai[1],
		.ops		= &machine_ops,
	}
};

struct snd_soc_card snd_soc_card_msm8660 = {
	.name		= "msm8660-pcm-audio",
	.dai_link	= msm8660_dai,
	.num_links	= ARRAY_SIZE(msm8660_dai),
	.platform = &msm8660_soc_platform,
};

/* msm_audio audio subsystem */
static struct snd_soc_device msm_snd_devdata = {
	.card = &snd_soc_card_msm8660,
	.codec_dev = &soc_codec_dev_timpani,
};

static struct platform_device *msm_snd_device;


static int __init msm_audio_init(void)
{
	int ret;

	msm_snd_device = platform_device_alloc("soc-audio", 0);
	if (!msm_snd_device) {
		pr_err("Platform device allocation failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(msm_snd_device, &msm_snd_devdata);

	msm_snd_devdata.dev = &msm_snd_device->dev;
	ret = platform_device_add(msm_snd_device);
	if (ret) {
		platform_device_put(msm_snd_device);
		return ret;
	}

	return ret;
}
module_init(msm_audio_init);

static void __exit msm_audio_exit(void)
{
	platform_device_unregister(msm_snd_device);
}
module_exit(msm_audio_exit);

MODULE_DESCRIPTION("ALSA SoC MSM8660");
MODULE_LICENSE("GPL v2");
