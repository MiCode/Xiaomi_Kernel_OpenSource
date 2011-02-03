/*
 * omap-hdmi.c  --  OMAP ALSA SoC DAI driver for HDMI audio
 *
 * Copyright (C) 2009 Texas Instruments
 *
 * Contact: Jorge Candelaria <x0107209@ti.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>
#include <sound/soc.h>

#include <plat/dma.h>
#include "omap-pcm.h"
#include "omap-hdmi.h"

#define CONFIG_HDMI_NO_IP_MODULE
#define OMAP_HDMI_RATES	(SNDRV_PCM_RATE_32000 | \
				SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000)

/* Support for 16 and 24 bits */
#define OMAP_HDMI_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | \
				SNDRV_PCM_FMTBIT_S24_LE)

#if defined(CONFIG_HDMI_NO_IP) || defined(CONFIG_HDMI_NO_IP_MODULE)
#include <plat/hdmi_lib.h>

struct omap_hdmi_data {
	struct hdmi_notifier notifier;
};

struct omap_hdmi_data hdmi_data;
#else
struct hdmi_ip_driver hdmi_audio_core;
#endif

static struct omap_pcm_dma_data omap_hdmi_dai_dma_params = {
	.name = "HDMI playback",
	.dma_req = OMAP44XX_DMA_DSS_HDMI_REQ,
	.port_addr = HDMI_WP + HDMI_WP_AUDIO_DATA,
	.sync_mode = OMAP_DMA_SYNC_PACKET,
};

#if defined(CONFIG_HDMI_NO_IP) || defined(CONFIG_HDMI_NO_IP_MODULE)
static void hdmi_hpd_notifier(int state, void *data)
{
	struct snd_pcm_substream *substream = data;

	if (!state && substream)
		snd_pcm_stop(substream, SNDRV_PCM_STATE_DISCONNECTED);
}
#endif

static int omap_hdmi_dai_startup(struct snd_pcm_substream *substream,
				  struct snd_soc_dai *dai)
{
	int err = 0;
#if defined(CONFIG_HDMI_NO_IP) || defined(CONFIG_HDMI_NO_IP_MODULE)
	struct hdmi_notifier *notifier = &hdmi_data.notifier;

	notifier->hpd_notifier = hdmi_hpd_notifier;
	notifier->private_data = substream;
	hdmi_add_notifier(notifier);

	err = hdmi_w1_wrapper_enable(HDMI_WP);
#else
	if (hdmi_audio_core.module_loaded)
		err = hdmi_audio_core.wrapper_enable(HDMI_WP);
	else
		printk(KERN_WARNING "Warning: hdmi_core.ko is not enabled");
#endif
	return err;
}

static void omap_hdmi_dai_shutdown(struct snd_pcm_substream *substream,
				    struct snd_soc_dai *dai)
{
#if defined(CONFIG_HDMI_NO_IP) || defined(CONFIG_HDMI_NO_IP_MODULE)
	struct hdmi_notifier *notifier = &hdmi_data.notifier;

	hdmi_w1_wrapper_disable(HDMI_WP);

	hdmi_remove_notifier(notifier);
	notifier->private_data = NULL;
#else
	if (hdmi_audio_core.module_loaded)
		hdmi_audio_core.wrapper_disable(HDMI_WP);
	else
		printk(KERN_WARNING "Warning: hdmi_core.ko is not enabled");
#endif
	return;
}

static int omap_hdmi_dai_trigger(struct snd_pcm_substream *substream, int cmd,
				  struct snd_soc_dai *dai)
{
	int err = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
#if defined(CONFIG_HDMI_NO_IP) || defined(CONFIG_HDMI_NO_IP_MODULE)
		err = hdmi_w1_start_audio_transfer(HDMI_WP);
#else
		if (hdmi_audio_core.module_loaded)
			err = hdmi_audio_core.start_audio(HDMI_WP);
		else
			printk(KERN_WARNING "Warning: hdmi_core.ko is "
							"not enabled");
#endif
		break;

	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
#if defined(CONFIG_HDMI_NO_IP) || defined(CONFIG_HDMI_NO_IP_MODULE)
		err = hdmi_w1_stop_audio_transfer(HDMI_WP);
#else
		if (hdmi_audio_core.module_loaded)
			err = hdmi_audio_core.stop_audio(HDMI_WP);
		else
			printk(KERN_WARNING "Warning: hdmi_core.ko is "
							"not enabled");
#endif
		break;
	default:
		err = -EINVAL;
	}

	return err;
}

static int omap_hdmi_dai_hw_params(struct snd_pcm_substream *substream,
				    struct snd_pcm_hw_params *params,
				    struct snd_soc_dai *dai)
{
	int err = 0;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
#if defined(CONFIG_HDMI_NO_IP) || defined(CONFIG_HDMI_NO_IP_MODULE)
		err = hdmi_configure_audio_sample_size(HDMI_SAMPLE_16BITS);
#else
		if (hdmi_audio_core.module_loaded)
			err = hdmi_audio_core.config_sample_size(HDMI_WP,
						HDMI_SAMPLE_16BITS);
		else
			printk(KERN_WARNING "Warning: hdmi_core.ko is "
							"not enabled");
#endif
		omap_hdmi_dai_dma_params.data_type = OMAP_DMA_DATA_TYPE_S32;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
#if defined(CONFIG_HDMI_NO_IP) || defined(CONFIG_HDMI_NO_IP_MODULE)
		err = hdmi_configure_audio_sample_size(HDMI_SAMPLE_24BITS);
#else
		if (hdmi_audio_core.module_loaded)
			err = hdmi_audio_core.config_sample_size(HDMI_WP,
						HDMI_SAMPLE_24BITS);
		else
			printk(KERN_WARNING "Warning: hdmi_core.ko is "
							"not enabled");
#endif
		omap_hdmi_dai_dma_params.data_type = OMAP_DMA_DATA_TYPE_S32;
		break;
	default:
		err = -EINVAL;
	}
	if (err < 0)
		return err;

#if defined(CONFIG_HDMI_NO_IP) || defined(CONFIG_HDMI_NO_IP_MODULE)
	err = hdmi_configure_audio_channels(params_channels(params),
						HDMI_CEA_CODE_00);
	if (err < 0)
		return err;
	err = hdmi_configure_audio_sample_rate(params_rate(params));
	if (err < 0)
		return err;
	err = hdmi_configure_audio();
	if (err < 0)
		return err;
#else
	err = hdmi_audio_core.config_audio_channels(HDMI_WP,
			params_channels(params), HDMI_CEA_CODE_00);
	if (err < 0)
		return err;
	err = hdmi_audio_core.config_audio_sample_rate(HDMI_WP,
				params_rate(params));
	if (err < 0)
		return err;
#endif

	omap_hdmi_dai_dma_params.packet_size = 0x20;

	snd_soc_dai_set_dma_data(dai, substream,
				 &omap_hdmi_dai_dma_params);

	return err;
}

static struct snd_soc_dai_ops omap_hdmi_dai_ops = {
	.startup	= omap_hdmi_dai_startup,
	.shutdown	= omap_hdmi_dai_shutdown,
	.trigger	= omap_hdmi_dai_trigger,
	.hw_params	= omap_hdmi_dai_hw_params,
};

static struct snd_soc_dai_driver omap_hdmi_dai = {
	.playback = {
		.channels_min = 2,
		/* currently we support only stereo HDMI */
		.channels_max = 2,
		.rates = OMAP_HDMI_RATES,
		.formats = OMAP_HDMI_FORMATS,
	},
	.ops = &omap_hdmi_dai_ops,
};

static __devinit int omap_hdmi_probe(struct platform_device *pdev)
{
	return snd_soc_register_dai(&pdev->dev, &omap_hdmi_dai);
}

static int __devexit omap_hdmi_remove(struct platform_device *pdev)
{
	snd_soc_unregister_dai(&pdev->dev);
	return 0;
}

static struct platform_driver hdmi_dai_driver = {
	.driver = {
		.name = "hdmi-dai",
		.owner = THIS_MODULE,
	},
	.probe = omap_hdmi_probe,
	.remove = __devexit_p(omap_hdmi_remove),
};

static int __init hdmi_dai_init(void)
{
	return platform_driver_register(&hdmi_dai_driver);
}
module_init(hdmi_dai_init);

static void __exit hdmi_dai_exit(void)
{
	platform_driver_unregister(&hdmi_dai_driver);
}
module_exit(hdmi_dai_exit);

#if !defined(CONFIG_HDMI_NO_IP) && !defined(CONFIG_HDMI_NO_IP_MODULE)

/* stub */
int audio_stub_lib_init(void)
{
	printk(KERN_WARNING "ERR: please install HDMI IP kernel module\n");
	return -1;
}
void audio_stub_lib_exit(void)
{
	printk(KERN_WARNING "HDMI module does not exist!\n");
}

#define EXPORT_SYMTAB

/* HDMI panel driver */
void hdmi_audio_core_stub_init(void)
{
	hdmi_audio_core.stop_video = NULL;
	hdmi_audio_core.start_video = NULL;
	hdmi_audio_core.wrapper_enable = NULL;
	hdmi_audio_core.wrapper_disable = NULL;
	hdmi_audio_core.stop_audio = NULL;
	hdmi_audio_core.start_audio = NULL;
	hdmi_audio_core.config_video = NULL;
	hdmi_audio_core.set_wait_pll = NULL;
	hdmi_audio_core.set_wait_pwr = NULL;
	hdmi_audio_core.set_wait_srst = NULL;
	hdmi_audio_core.read_edid = NULL;
	hdmi_audio_core.config_audio_sample_rate = NULL;
	hdmi_audio_core.config_sample_size = NULL;
	hdmi_audio_core.config_audio_channels = NULL;
	hdmi_audio_core.ip_init = audio_stub_lib_init;
	hdmi_audio_core.ip_exit = audio_stub_lib_exit;
	hdmi_audio_core.module_loaded = 0;
}

#endif


MODULE_AUTHOR("Jorge Candelaria <x0107209@ti.com");
MODULE_DESCRIPTION("OMAP HDMI SoC Interface");
MODULE_LICENSE("GPL");
