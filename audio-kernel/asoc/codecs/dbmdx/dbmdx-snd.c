/*
 * snd-dbmdx.c -- ASoC Machine Driver for DBMDX
 *
 * Copyright (C) 2014 DSP Group
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define DEBUG

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#endif

#include "dbmdx-interface.h"

#define DRIVER_NAME "snd-dbmdx-mach-drv"
#define CODEC_NAME "dbmdx" /* "dbmdx" */
#define PLATFORM_DEV_NAME "dbmdx-snd-soc-platform"

static int board_dai_init(struct snd_soc_pcm_runtime *rtd);

static struct snd_soc_dai_link board_dbmdx_dai_link[] = {
	{
		.name = "dbmdx_dai_link.1",
		.stream_name = "voice_sv",
		/* asoc Cpu-Dai  device name */
		.cpu_dai_name = "DBMDX_codec_dai",
		/* asoc Codec-Dai device name */
		.codec_dai_name = "DBMDX_codec_dai",
		.init = board_dai_init,
	},
};

static int board_dai_init(struct snd_soc_pcm_runtime *rtd)
{
	return 0;
}

static struct snd_soc_card dspg_dbmdx_card = {
	.name		= "dspg-dbmdx",
	.dai_link	= board_dbmdx_dai_link,
	.num_links	= ARRAY_SIZE(board_dbmdx_dai_link),
	.set_bias_level		= NULL,
	.set_bias_level_post	= NULL,
};

#ifdef CONFIG_OF
static int dbmdx_init_dai_link(struct snd_soc_card *card)
{
	int cnt;
	struct snd_soc_dai_link *dai_link;
	struct device_node *codec_node, *platform_node;

	codec_node = of_find_node_by_name(0, CODEC_NAME);
	if (!of_device_is_available(codec_node))
		codec_node = of_find_node_by_name(codec_node, CODEC_NAME);

	if (!codec_node) {
		pr_err("%s: Codec node not found\n", __func__);
		return -EINVAL;
	}

	platform_node = of_find_node_by_name(0, PLATFORM_DEV_NAME);
	if (!platform_node) {
		pr_err("%s: Platform node not found\n", __func__);
		return -EINVAL;
	}

	for (cnt = 0; cnt < card->num_links; cnt++) {
		dai_link = &card->dai_link[cnt];
		dai_link->codec_of_node = codec_node;
		dai_link->platform_of_node = platform_node;
	}

	return 0;
}
#else
static int dbmdx_init_dai_link(struct snd_soc_card *card)
{
	int cnt;
	struct snd_soc_dai_link *dai_link;

	for (cnt = 0; cnt < card->num_links; cnt++) {
		dai_link = &card->dai_link[cnt];
		dai_link->codec_name = "dbmdx-codec.0";
		dai_link->platform_name = "dbmdx-snd-soc-platform.0";
	}

	return 0;
}
#endif

static int dbmdx_snd_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct snd_soc_card *card = &dspg_dbmdx_card;
	/* struct device_node *np = pdev->dev.of_node; */

	/* note: platform_set_drvdata() here saves pointer to the card's data
	 * on the device's 'struct device_private *p'->driver_data
	 */

	dev_info(&pdev->dev, "%s:\n", __func__);

	card->dev = &pdev->dev;
	if (dbmdx_init_dai_link(card) < 0) {
		dev_err(&pdev->dev, "%s: Initialization of DAI links failed\n",
			__func__);
		ret = -1;
		goto ERR_CLEAR;
	}

#if defined(DBMDX_DEFER_IF_SND_CARD_ID_0)
	if (!snd_cards[0] || !snd_cards[0]->id) {
		dev_info(&pdev->dev,
			"%s: Defering DBMDX SND card probe, wait primary card...\n",
			__func__);
		return -EPROBE_DEFER;
	}
#endif
	/* Register ASoC sound Card */
	ret = snd_soc_register_card(card);
	if (ret) {
		dev_err(&pdev->dev,
			"%s: registering of sound card failed: %d\n",
			__func__, ret);
		goto ERR_CLEAR;
	}
	dev_info(&pdev->dev, "%s: DBMDX ASoC card registered\n", __func__);

	return 0;

ERR_CLEAR:
	return ret;
}

static int dbmdx_snd_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);

	snd_soc_unregister_card(card);

	dev_info(&pdev->dev, "%s: DBMDX ASoC card unregistered\n", __func__);

	return 0;
}

static const struct of_device_id snd_dbmdx_of_ids[] = {
	{ .compatible = "dspg,snd-dbmdx-mach-drv" },
	{ },
};

static struct platform_driver board_dbmdx_snd_drv = {
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = snd_dbmdx_of_ids,
		.pm = &snd_soc_pm_ops,
	},
	.probe = dbmdx_snd_probe,
	.remove = dbmdx_snd_remove,
};

#ifdef CONFIG_SND_SOC_DBMDX_M
static int __init board_dbmdx_mod_init(void)
{
	return platform_driver_register(&board_dbmdx_snd_drv);
}
module_init(board_dbmdx_mod_init);

static void __exit board_dbmdx_mod_exit(void)
{
	platform_driver_unregister(&board_dbmdx_snd_drv);
}
module_exit(board_dbmdx_mod_exit);
#else
int board_dbmdx_snd_init(void)
{
	int ret = 0;
	
	pr_err("Enter %s:\n", __func__);
	
	ret =  platform_driver_register(&board_dbmdx_snd_drv);
	pr_err("%s: ret =%d\n", __func__, ret);
	return ret;
}

void board_dbmdx_snd_exit(void)
{
	platform_driver_unregister(&board_dbmdx_snd_drv);
}
#endif

MODULE_DESCRIPTION("ASoC machine driver for DSPG DBMDX");
MODULE_AUTHOR("DSP Group");
MODULE_LICENSE("GPL");
