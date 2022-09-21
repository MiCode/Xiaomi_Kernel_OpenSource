/* Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/of_device.h>
#include <linux/pm_qos.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/info.h>
#include <dsp/audio_notifier.h>
#include "msm_dailink.h"


#define DRV_NAME "spf-asoc-snd"

#define __CHIPSET__ "SA8xx5 "
#define MSM_DAILINK_NAME(name) (__CHIPSET__#name)

#define DEV_NAME_STR_LEN            32

#define SAMPLING_RATE_8KHZ      8000
#define SAMPLING_RATE_11P025KHZ 11025
#define SAMPLING_RATE_16KHZ     16000
#define SAMPLING_RATE_22P05KHZ  22050
#define SAMPLING_RATE_32KHZ     32000
#define SAMPLING_RATE_44P1KHZ   44100
#define SAMPLING_RATE_48KHZ     48000
#define SAMPLING_RATE_88P2KHZ   88200
#define SAMPLING_RATE_96KHZ     96000
#define SAMPLING_RATE_176P4KHZ  176400
#define SAMPLING_RATE_192KHZ    192000
#define SAMPLING_RATE_352P8KHZ  352800
#define SAMPLING_RATE_384KHZ    384000

#define WCD9XXX_MBHC_DEF_RLOADS     5

#define WSA8810_NAME_1 "wsa881x.20170211"
#define WSA8810_NAME_2 "wsa881x.20170212"
#define WCN_CDC_SLIM_RX_CH_MAX 2
#define WCN_CDC_SLIM_TX_CH_MAX 3
#define TDM_CHANNEL_MAX 8

#define ADSP_STATE_READY_TIMEOUT_MS 3000
#define MSM_LL_QOS_VALUE 300 /* time in us to ensure LPM doesn't go in C3/C4 */
#define MSM_HIFI_ON 1

enum {
	SLIM_RX_0 = 0,
	SLIM_RX_1,
	SLIM_RX_2,
	SLIM_RX_3,
	SLIM_RX_4,
	SLIM_RX_5,
	SLIM_RX_6,
	SLIM_RX_7,
	SLIM_RX_MAX,
};
enum {
	SLIM_TX_0 = 0,
	SLIM_TX_1,
	SLIM_TX_2,
	SLIM_TX_3,
	SLIM_TX_4,
	SLIM_TX_5,
	SLIM_TX_6,
	SLIM_TX_7,
	SLIM_TX_8,
	SLIM_TX_MAX,
};

enum {
	PRIM_MI2S = 0,
	SEC_MI2S,
	TERT_MI2S,
	QUAT_MI2S,
	QUIN_MI2S,
	MI2S_MAX,
};

enum {
	PRIM_AUX_PCM = 0,
	SEC_AUX_PCM,
	TERT_AUX_PCM,
	QUAT_AUX_PCM,
	QUIN_AUX_PCM,
	AUX_PCM_MAX,
};

struct mi2s_conf {
	struct mutex lock;
	u32 ref_cnt;
	u32 msm_is_mi2s_master;
};

struct dev_config {
	u32 sample_rate;
	u32 bit_format;
	u32 channels;
};

enum {
	DP_RX_IDX = 0,
	EXT_DISP_RX_IDX_MAX,
};

struct msm_wsa881x_dev_info {
	struct device_node *of_node;
	u32 index;
};

enum pinctrl_pin_state {
	STATE_DISABLE = 0, /* All pins are in sleep state */
	STATE_MI2S_ACTIVE,  /* IS2 = active, TDM = sleep */
	STATE_TDM_ACTIVE,  /* IS2 = sleep, TDM = active */
};

struct msm_pinctrl_info {
	struct pinctrl *pinctrl;
	struct pinctrl_state *mi2s_disable;
	struct pinctrl_state *tdm_disable;
	struct pinctrl_state *mi2s_active;
	struct pinctrl_state *tdm_active;
	enum pinctrl_pin_state curr_state;
};

enum {
	TDM_0 = 0,
	TDM_1,
	TDM_2,
	TDM_3,
	TDM_4,
	TDM_5,
	TDM_6,
	TDM_7,
	TDM_PORT_MAX,
};

enum {
	TDM_PRI = 0,
	TDM_SEC,
	TDM_TERT,
	TDM_QUAT,
	TDM_QUIN,
	TDM_INTERFACE_MAX,
};

struct tdm_port {
	u32 mode;
	u32 channel;
};

struct tdm_conf {
	struct mutex lock;
	u32 ref_cnt;
};

struct msm_asoc_mach_data {
	struct snd_info_entry *codec_root;
	struct msm_common_pdata *common_pdata;
	struct msm_pinctrl_info pinctrl_info;
	struct device_node *us_euro_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en1_gpio_p; /* used by pinctrl API */
	struct device_node *hph_en0_gpio_p; /* used by pinctrl API */
	struct device_node *fsa_handle;
	struct snd_soc_codec *codec;
	struct work_struct adsp_power_up_work;
	struct tdm_conf tdm_intf_conf[TDM_INTERFACE_MAX];
};

static const char *const pin_states[] = {"sleep", "i2s-active",
					 "tdm-active"};

static struct platform_device *spdev;

static bool codec_reg_done;

/*
 * Need to report LINEIN
 * if R/L channel impedance is larger than 5K ohm
 */

static const struct snd_soc_dapm_widget msm_dapm_widgets[] = {

	SND_SOC_DAPM_MIC("Handset Mic", NULL),
	SND_SOC_DAPM_MIC("Analog Mic3", NULL),
	SND_SOC_DAPM_MIC("Analog Mic4", NULL),

	SND_SOC_DAPM_MIC("Digital Mic0", NULL),
	SND_SOC_DAPM_MIC("Digital Mic1", NULL),
	SND_SOC_DAPM_MIC("Digital Mic2", NULL),
	SND_SOC_DAPM_MIC("Digital Mic3", NULL),
	SND_SOC_DAPM_MIC("Digital Mic4", NULL),
	SND_SOC_DAPM_MIC("Digital Mic5", NULL),
	SND_SOC_DAPM_MIC("Digital Mic6", NULL),
	SND_SOC_DAPM_MIC("Digital Mic7", NULL),
};

/* Digital audio interface glue - connects codec <---> CPU */
static struct snd_soc_dai_link msm_common_dai_links[] = {
	/* BackEnd DAI Links */
  {
    .name = "TERT_TDM_RX_0_DUMMY",
    .stream_name = "TDM-LPAIF-RX-TERTIARY",
    .dpcm_playback = 1,
    .trigger = {SND_SOC_DPCM_TRIGGER_POST,
                SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(tert_tdm_rx_0_dummy),
  },
  {
    .name = "TERT_TDM_TX_0_DUMMY",
    .stream_name = "TDM-LPAIF-TX-TERTIARY",
    .dpcm_capture = 1,
    .trigger = {SND_SOC_DPCM_TRIGGER_POST,
                SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(tert_tdm_tx_0_dummy),
  },
  {
    .name = "LPASS_BE_AUXPCM_RX_DUMMY",
    .stream_name = "AUXPCM-LPAIF-RX-PRIMARY",
    .dpcm_playback = 1,
    .trigger = {SND_SOC_DPCM_TRIGGER_POST,
                SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(lpass_be_auxpcm_rx_dummy),
  },
  {
    .name = "LPASS_BE_AUXPCM_TX_DUMMY",
    .stream_name = "AUXPCM-LPAIF-TX-PRIMARY",
    .dpcm_capture = 1,
    .trigger = {SND_SOC_DPCM_TRIGGER_POST,
                SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(lpass_be_auxpcm_tx_dummy),
  },
};

static struct snd_soc_dai_link msm_talos_dai_links[] = {
	/* BackEnd DAI Links */
  {
    .name = "PRIMARY_TDM_RX_0_DUMMY",
    .stream_name = "TDM-LPAIF_WSA-RX-PRIMARY",
    .dpcm_playback = 1,
    .trigger = {SND_SOC_DPCM_TRIGGER_POST,
                SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(primary_tdm_rx_0_dummy),
  },
  {
    .name = "PRIMARY_TDM_TX_0_DUMMY",
    .stream_name = "TDM-LPAIF_WSA-TX-PRIMARY",
    .dpcm_capture = 1,
    .trigger = {SND_SOC_DPCM_TRIGGER_POST,
                SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(primary_tdm_tx_0_dummy),
  },
  {
    .name = "LPASS_BE_AUXPCM_RX_DUMMY",
    .stream_name = "AUXPCM-LPAIF-RX-PRIMARY",
    .dpcm_playback = 1,
    .trigger = {SND_SOC_DPCM_TRIGGER_POST,
                SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(lpass_be_auxpcm_rx_dummy),
  },
  {
    .name = "LPASS_BE_AUXPCM_TX_DUMMY",
    .stream_name = "AUXPCM-LPAIF-TX-PRIMARY",
    .dpcm_capture = 1,
    .trigger = {SND_SOC_DPCM_TRIGGER_POST,
                SND_SOC_DPCM_TRIGGER_POST},
	.ignore_suspend = 1,
	.ignore_pmdown_time = 1,
	SND_SOC_DAILINK_REG(lpass_be_auxpcm_tx_dummy),
  },
};

struct snd_soc_card snd_soc_card_auto_hana_dummy_msm = {
	.name = "gvmauto-8155-snd-card",
};

struct snd_soc_card snd_soc_card_auto_talos_dummy_msm = {
	.name = "gvmauto-6155-snd-card",
};

static int msm_populate_dai_link_component_of_node(
					struct snd_soc_card *card)
{
	int i, j, index, ret = 0;
	struct device *cdev = card->dev;
	struct snd_soc_dai_link *dai_link = card->dai_link;
	struct device_node *np;

	if (!cdev) {
		pr_err("%s: Sound card device memory NULL\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < card->num_links; i++) {
		if (dai_link[i].platforms->of_node && dai_link[i].cpus->of_node)
			continue;

		/* populate platform_of_node for snd card dai links Skip this part for dummy snd card*/
        if(0) {
		if (dai_link[i].platforms->name &&
		    !dai_link[i].platforms->of_node) {
			index = of_property_match_string(cdev->of_node,
						"asoc-platform-names",
						dai_link[i].platforms->name);
			if (index < 0) {
				pr_err("%s: No match found for platform name: %s\n index: %i cdev_of_node: %s",
					__func__, dai_link[i].platforms->name, index, cdev->of_node);
				ret = index;
				goto err;
			}
			np = of_parse_phandle(cdev->of_node, "asoc-platform",
					      index);
			if (!np) {
				pr_err("%s: retrieving phandle for platform %s, index %d failed\n",
					__func__, dai_link[i].platforms->name,
					index);
				ret = -ENODEV;
				goto err;
			}
			dai_link[i].platforms->of_node = np;
			dai_link[i].platforms->name = NULL;
		}
		}

		/* populate cpu_of_node for snd card dai links */
		if (dai_link[i].cpus->dai_name && !dai_link[i].cpus->of_node) {
			index = of_property_match_string(cdev->of_node,
						 "asoc-cpu-names",
						 dai_link[i].cpus->dai_name);
        pr_err("%s: retrieving cpu_of_node for %s\n",
						__func__,
						dai_link[i].cpus->dai_name);
			if (index >= 0) {
				np = of_parse_phandle(cdev->of_node, "asoc-cpu",
						index);
				if (!np) {
					pr_err("%s: retrieving phandle for cpu dai %s failed\n",
						__func__,
						dai_link[i].cpus->dai_name);
					ret = -ENODEV;
					goto err;
				}
				dai_link[i].cpus->of_node = np;
				dai_link[i].cpus->dai_name = NULL;
			}
		}

		/* populate codec_of_node for snd card dai links */
		if (dai_link[i].num_codecs > 0) {
			for (j = 0; j < dai_link[i].num_codecs; j++) {
				if (dai_link[i].codecs[j].of_node ||
						!dai_link[i].codecs[j].name)
					continue;

				index = of_property_match_string(cdev->of_node,
						"asoc-codec-names",
						dai_link[i].codecs[j].name);
				if (index < 0)
					continue;
				np = of_parse_phandle(cdev->of_node,
						      "asoc-codec",
						      index);
				if (!np) {
					pr_err("%s: retrieving phandle for codec %s failed\n",
					        __func__, dai_link[i].codecs[j].name);
					ret = -ENODEV;
					goto err;
				}
				dai_link[i].codecs[j].of_node = np;
				dai_link[i].codecs[j].name = NULL;
			}
		}
	}

err:
	return ret;
}

static const struct of_device_id gvm_asoc_machine_of_match[]  = {
	{ .compatible = "qcom,8155-spf-asoc-snd-adp-star",
      .data = "adp_star_codec"},
	{ .compatible = "qcom,6155-spf-asoc-snd-adp-star",
      .data = "adp_star_codec"},
    {},
};
static struct snd_soc_dai_link msm_auto_dai_links[
			 ARRAY_SIZE(msm_common_dai_links)];

static struct snd_soc_dai_link msm_auto_talos_dai_links[
			 ARRAY_SIZE(msm_talos_dai_links)];

static struct snd_soc_card *populate_snd_card_dailinks(struct device *dev)
{
	struct snd_soc_card *card = NULL;
	struct snd_soc_dai_link *dailink;
	int total_links;
	const struct of_device_id *match;

	match = of_match_node(gvm_asoc_machine_of_match, dev->of_node);
	if (!match) {
		dev_err(dev, "%s: No DT match found for sound card\n",
			__func__);
		return NULL;
	}

	if (!strcmp(match->compatible, "qcom,8155-spf-asoc-snd-adp-star")) {
		card = &snd_soc_card_auto_hana_dummy_msm;
		total_links = ARRAY_SIZE(msm_common_dai_links);
		memcpy(msm_auto_dai_links,
			msm_common_dai_links,
			sizeof(msm_common_dai_links));

		dailink = msm_auto_dai_links;
	} else if (!strcmp(match->compatible, "qcom,6155-spf-asoc-snd-adp-star")) {
		card = &snd_soc_card_auto_talos_dummy_msm;
		total_links = ARRAY_SIZE(msm_talos_dai_links);
		memcpy(msm_auto_talos_dai_links,
			msm_talos_dai_links,
			sizeof(msm_talos_dai_links));

		dailink = msm_auto_talos_dai_links;
	}

	if (card) {
		card->dai_link = dailink;
		card->num_links = total_links;
	}

	return card;
}

struct msm_common_pdata *msm_common_get_pdata(struct snd_soc_card *card)
{
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	if (!pdata)
		return NULL;

	return pdata->common_pdata;
}

void msm_common_set_pdata(struct snd_soc_card *card,
			  struct msm_common_pdata *common_pdata)
{
	struct msm_asoc_mach_data *pdata = snd_soc_card_get_drvdata(card);

	if (!pdata)
		return;

	pdata->common_pdata = common_pdata;
}

static int msm_asoc_machine_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card;
	struct msm_asoc_mach_data *pdata;
	int ret;



	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "No platform supplied from device tree\n");
		return -EINVAL;
	}

	dev_err(&pdev->dev, "%s", __func__);

	pdata = devm_kzalloc(&pdev->dev,
			sizeof(struct msm_asoc_mach_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	card = populate_snd_card_dailinks(&pdev->dev);
	if (!card) {
		dev_err(&pdev->dev, "%s: Card uninitialized\n", __func__);
		ret = -EINVAL;
		goto err;
	}
	card->dev = &pdev->dev;
	platform_set_drvdata(pdev, card);
	snd_soc_card_set_drvdata(card, pdata);

	ret = snd_soc_of_parse_card_name(card, "qcom,model");
	if (ret) {
		dev_err(&pdev->dev, "parse card name failed, err:%d\n",
			ret);
    pr_err("parse card name failed, err:%d\n", __func__, ret);
		goto err;
	}

	ret = msm_populate_dai_link_component_of_node(card);
	if (ret) {
		ret = -EPROBE_DEFER;
		goto err;
	}

	ret = devm_snd_soc_register_card(&pdev->dev, card);

	if (ret == -EPROBE_DEFER) {
		if (codec_reg_done)
			ret = -EINVAL;
		goto err;
	} else if (ret) {
		dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n",
			ret);
		pr_err("snd_soc_register_card failed (%d)\n",
			ret);
		goto err;
	}
	dev_info(&pdev->dev, "Sound card %s registered\n", card->name);
	pr_err("Sound card %s registered\n", card->name);
	spdev = pdev;

	return 0;
err:
	devm_kfree(&pdev->dev, pdata);
	return ret;
}

static int msm_asoc_machine_remove(struct platform_device *pdev)
{

	return 0;
}

static struct platform_driver gvm_asoc_machine_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.pm = &snd_soc_pm_ops,
		.of_match_table = gvm_asoc_machine_of_match,
	},
	.probe = msm_asoc_machine_probe,
	.remove = msm_asoc_machine_remove,
};

int __init gvm_auto_spf_init(void)
{
	pr_debug("%s\n", __func__);
	return platform_driver_register(&gvm_asoc_machine_driver);
}

void gvm_auto_spf_exit(void)
{
	pr_debug("%s\n", __func__);
	platform_driver_unregister(&gvm_asoc_machine_driver);
}

module_init(gvm_auto_spf_init);
module_exit(gvm_auto_spf_exit);

MODULE_DESCRIPTION("ALSA SoC Machine Driver for SPF");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, gvm_asoc_machine_of_match);
