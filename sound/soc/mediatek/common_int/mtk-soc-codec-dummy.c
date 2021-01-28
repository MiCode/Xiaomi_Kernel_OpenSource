// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_codec_dummy
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   Audio codec dummy file
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 ******************************************************************************
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "mtk-auddrv-common.h"
#include "mtk-soc-analog-type.h"
#include "mtk-soc-pcm-common.h"
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/soc.h>

#define CODEC_DUMMY_NAME "mtk-codec-dummy"

static int dummy_codec_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *Daiport)
{
	return 0;
}

static int dummy_codec_prepare(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *Daiport)
{
	return 0;
}

static int dummy_codec_trigger(struct snd_pcm_substream *substream, int command,
			       struct snd_soc_dai *Daiport)
{
	switch (command) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}


	return 0;
}

static const struct snd_soc_dai_ops dummy_aif1_dai_ops = {
	.startup = dummy_codec_startup,
	.prepare = dummy_codec_prepare,
	.trigger = dummy_codec_trigger,
};

static struct snd_soc_dai_driver dummy_6323_dai_codecs[] = {
	{
		.name = MT_SOC_CODEC_TXDAI_NAME,
		.playback = {

				.stream_name = MT_SOC_DL1_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 8,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
	{
		.name = MT_SOC_CODEC_RXDAI_NAME,
		.capture = {

				.stream_name = MT_SOC_UL1_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 8,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
	{
		.name = MT_SOC_CODEC_VOICE_MD1DAI_NAME,
		.playback = {

				.stream_name = MT_SOC_VOICE_MD1_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 2,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
		.capture = {

				.stream_name = MT_SOC_VOICE_MD1_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 2,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
	{
		.name = MT_SOC_CODEC_VOICE_MD2DAI_NAME,
		.playback = {

				.stream_name = MT_SOC_VOICE_MD2_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 2,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
		.capture = {

				.stream_name = MT_SOC_VOICE_MD2_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 2,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
	{
		.name = MT_SOC_CODEC_FMI2S2RXDAI_NAME,
		.playback = {

				.stream_name = MT_SOC_FM_I2S2_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 2,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
		.capture = {

				.stream_name =
					MT_SOC_FM_I2S2_RECORD_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 2,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
	{
		.name = MT_SOC_CODEC_STUB_NAME,
		.playback = {

				.stream_name = MT_SOC_ROUTING_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 8,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
	{.name = MT_SOC_CODEC_DL1AWBDAI_NAME,
	 .capture = {

			 .stream_name = MT_SOC_DL1_AWB_RECORD_STREAM_NAME,
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = SOC_HIGH_USE_RATE,
			 .formats = SND_SOC_ADV_MT_FMTS,
		 } },
	{.name = MT_SOC_CODEC_I2S0AWB_NAME,
	 .capture = {

			 .stream_name = MT_SOC_I2S0AWB_STREAM_NAME,
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = SOC_HIGH_USE_RATE,
			 .formats = SND_SOC_ADV_MT_FMTS,
		 } },
	{.name = MT_SOC_CODEC_VOICE_MD2_BTDAI_NAME,
	 .playback = {

			 .stream_name = MT_SOC_VOICE_MD2_BT_STREAM_NAME,
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = SOC_HIGH_USE_RATE,
			 .formats = SND_SOC_ADV_MT_FMTS,
		 } },
	{.name = MT_SOC_CODEC_VOICE_MD1_BTDAI_NAME,
	 .playback = {

			 .stream_name = MT_SOC_VOICE_MD1_BT_STREAM_NAME,
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = SOC_HIGH_USE_RATE,
			 .formats = SND_SOC_ADV_MT_FMTS,
		 } },

	{.name = MT_SOC_CODEC_VOIPCALLBTOUTDAI_NAME,
	 .playback = {

			 .stream_name = MT_SOC_VOIP_BT_OUT_STREAM_NAME,
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = SOC_HIGH_USE_RATE,
			 .formats = SND_SOC_ADV_MT_FMTS,
		 } },
	{.name = MT_SOC_CODEC_VOIPCALLBTINDAI_NAME,
	 .capture = {

			 .stream_name = MT_SOC_VOIP_BT_IN_STREAM_NAME,
			 .channels_min = 1,
			 .channels_max = 2,
			 .rates = SOC_HIGH_USE_RATE,
			 .formats = SND_SOC_ADV_MT_FMTS,
		 } },
	{
		.name = MT_SOC_CODEC_DUMMY_DAI_NAME,
		.playback = {

				.stream_name = MT_SOC_ROUTING_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 8,
				.rates = SOC_HIGH_USE_RATE,
				.formats = (SNDRV_PCM_FMTBIT_U8 |
					    SNDRV_PCM_FMTBIT_S8 |
					    SNDRV_PCM_FMTBIT_U16_LE |
					    SNDRV_PCM_FMTBIT_S16_LE |
					    SNDRV_PCM_FMTBIT_U16_BE |
					    SNDRV_PCM_FMTBIT_S16_BE |
					    SNDRV_PCM_FMTBIT_U24_LE |
					    SNDRV_PCM_FMTBIT_S24_LE |
					    SNDRV_PCM_FMTBIT_U24_BE |
					    SNDRV_PCM_FMTBIT_S24_BE |
					    SNDRV_PCM_FMTBIT_U24_3LE |
					    SNDRV_PCM_FMTBIT_S24_3LE |
					    SNDRV_PCM_FMTBIT_U24_3BE |
					    SNDRV_PCM_FMTBIT_S24_3BE |
					    SNDRV_PCM_FMTBIT_U32_LE |
					    SNDRV_PCM_FMTBIT_S32_LE |
					    SNDRV_PCM_FMTBIT_U32_BE |
					    SNDRV_PCM_FMTBIT_S32_BE),
			},
		.capture = {

				.stream_name = MT_SOC_ROUTING_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 8,
				.rates = SOC_HIGH_USE_RATE,
				.formats = (SNDRV_PCM_FMTBIT_U8 |
					    SNDRV_PCM_FMTBIT_S8 |
					    SNDRV_PCM_FMTBIT_U16_LE |
					    SNDRV_PCM_FMTBIT_S16_LE |
					    SNDRV_PCM_FMTBIT_U16_BE |
					    SNDRV_PCM_FMTBIT_S16_BE |
					    SNDRV_PCM_FMTBIT_U24_LE |
					    SNDRV_PCM_FMTBIT_S24_LE |
					    SNDRV_PCM_FMTBIT_U24_BE |
					    SNDRV_PCM_FMTBIT_S24_BE |
					    SNDRV_PCM_FMTBIT_U24_3LE |
					    SNDRV_PCM_FMTBIT_S24_3LE |
					    SNDRV_PCM_FMTBIT_U24_3BE |
					    SNDRV_PCM_FMTBIT_S24_3BE |
					    SNDRV_PCM_FMTBIT_U32_LE |
					    SNDRV_PCM_FMTBIT_S32_LE |
					    SNDRV_PCM_FMTBIT_U32_BE |
					    SNDRV_PCM_FMTBIT_S32_BE),
			},
	},
#ifdef CONFIG_MTK_HDMI_TDM
	{
		.name = MT_SOC_CODEC_HDMI_DUMMY_DAI_NAME,
		.playback = {

				.stream_name = MT_SOC_HDMI_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 8,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,

			},
		.capture = {

				.stream_name = MT_SOC_HDMI_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 8,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
#endif
	{
		.name = MT_SOC_CODEC_I2S0_DUMMY_DAI_NAME,
		.playback = {

				.stream_name = MT_SOC_I2S0_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 8,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
	{
		.name = MT_SOC_CODEC_MRGRX_DUMMY_DAI_NAME,
		.playback = {

				.stream_name = MT_SOC_MRGRX_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 8,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
		.capture = {

				.stream_name = MT_SOC_MRGRX_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 8,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
	{
		.name = MT_SOC_CODEC_FMMRGTXDAI_DUMMY_DAI_NAME,
		.playback = {

				.stream_name = MT_SOC_FM_MRGTX_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 2,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
	{
		.name = MT_SOC_CODEC_TDMRX_DAI_NAME,
		.capture = {

				.stream_name = MT_SOC_TDM_CAPTURE_STREAM_NAME,
				.channels_min = 2,
				.channels_max = 2,
				.rates = SOC_HIGH_USE_RATE,
				.formats = (SNDRV_PCM_FMTBIT_U8 |
					    SNDRV_PCM_FMTBIT_S8 |
					    SNDRV_PCM_FMTBIT_U16_LE |
					    SNDRV_PCM_FMTBIT_S16_LE |
					    SNDRV_PCM_FMTBIT_U16_BE |
					    SNDRV_PCM_FMTBIT_S16_BE |
					    SNDRV_PCM_FMTBIT_U24_LE |
					    SNDRV_PCM_FMTBIT_S24_LE |
					    SNDRV_PCM_FMTBIT_U24_BE |
					    SNDRV_PCM_FMTBIT_S24_BE |
					    SNDRV_PCM_FMTBIT_U24_3LE |
					    SNDRV_PCM_FMTBIT_S24_3LE |
					    SNDRV_PCM_FMTBIT_U24_3BE |
					    SNDRV_PCM_FMTBIT_S24_3BE |
					    SNDRV_PCM_FMTBIT_U32_LE |
					    SNDRV_PCM_FMTBIT_S32_LE |
					    SNDRV_PCM_FMTBIT_U32_BE |
					    SNDRV_PCM_FMTBIT_S32_BE),
			},
	},
	{
		.name = MT_SOC_CODEC_FM_I2S_DUMMY_DAI_NAME,
		.capture = {

				.stream_name =
					MT_SOC_FM_I2S_PLAYBACK_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 8,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
	{
		.name = MT_SOC_CODEC_BTCVSD_RX_DAI_NAME,
		.capture = {

				.stream_name =
					MT_SOC_BTCVSD_CAPTURE_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 2,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
	{
		.name = MT_SOC_CODEC_BTCVSD_TX_DAI_NAME,
		.playback = {

				.stream_name =
					MT_SOC_BTCVSD_PLAYBACK_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 2,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
	{
		.name = MT_SOC_CODEC_BTCVSD_DAI_NAME,
		.playback = {

				.stream_name =
					MT_SOC_BTCVSD_PLAYBACK_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 2,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
		.capture = {

				.stream_name =
					MT_SOC_BTCVSD_CAPTURE_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 2,
				.rates = SOC_HIGH_USE_RATE,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
#ifdef _NON_COMMON_FEATURE_READY
	{
		.name = MT_SOC_CODEC_MOD_DAI_NAME,
		.capture = {

				.stream_name = MT_SOC_MODDAI_STREAM_NAME,
				.channels_min = 1,
				.channels_max = 2,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SND_SOC_ADV_MT_FMTS,
			},
	},
#endif
};

static int dummy_codec_probe(struct snd_soc_component *component)
{

	return 0;
}

static int dummy_codec_remove(struct snd_soc_component *component)
{

	return 0;
}

static const struct snd_soc_component_driver soc_mtk_codec = {
	.name = CODEC_DUMMY_NAME,
	.probe = dummy_codec_probe,
	.remove = dummy_codec_remove,
};

static int mtk_dummy_codec_dev_probe(struct platform_device *pdev)
{
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	if (pdev->dev.dma_mask == NULL)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_CODEC_DUMMY_NAME);
	pdev->name = pdev->dev.kobj.name;

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_component(&pdev->dev, &soc_mtk_codec,
				      dummy_6323_dai_codecs,
				      ARRAY_SIZE(dummy_6323_dai_codecs));
}

static int mtk_dummy_codec_dev_remove(struct platform_device *pdev)
{
	pr_debug("%s:\n", __func__);
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_codec_dummy_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_codec_dummy",
	},
	{} };
#endif

static struct platform_driver mtk_codec_dummy_driver = {
	.driver = {

			.name = MT_SOC_CODEC_DUMMY_NAME,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_codec_dummy_of_ids,
#endif
		},
	.probe = mtk_dummy_codec_dev_probe,
	.remove = mtk_dummy_codec_dev_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtk_codec_dummy_dev;
#endif

static int __init mtk_dummy_codec_init(void)
{
	pr_debug("%s:\n", __func__);
#ifndef CONFIG_OF
	int ret = 0;

	soc_mtk_codec_dummy_dev =
		platform_device_alloc(MT_SOC_CODEC_DUMMY_NAME, -1);

	if (!soc_mtk_codec_dummy_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtk_codec_dummy_dev);
	if (ret != 0) {
		platform_device_put(soc_mtk_codec_dummy_dev);
		return ret;
	}
#endif
	return platform_driver_register(&mtk_codec_dummy_driver);
}
module_init(mtk_dummy_codec_init);

static void __exit mtk_codec_dummy_exit(void)
{
	pr_debug("%s:\n", __func__);

	platform_driver_unregister(&mtk_codec_dummy_driver);
}
module_exit(mtk_codec_dummy_exit);

/* Module information */
MODULE_DESCRIPTION("MTK  dummy codec driver");
MODULE_LICENSE("GPL v2");
