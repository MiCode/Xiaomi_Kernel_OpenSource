// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   mt6583.c
 *
 * Project:
 * --------
 *   MT6583  Audio Driver Kernel Function
 *
 * Description:
 * ------------
 *   Audio register
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

#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"
#include "mtk-soc-afe-control.h"
#include "mtk-soc-pcm-common.h"
#include "mtk-soc-pcm-platform.h"
#include <linux/dma-mapping.h>

/*
 *    function implementation
 */

static int mtk_dummy_probe(struct platform_device *pdev);
static int mtk_dummypcm_close(struct snd_pcm_substream *substream);
static int mtk_afe_dummy_component_probe(struct snd_soc_component *component);

static struct snd_pcm_hardware mtk_dummy_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP | SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME | SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SND_SOC_ADV_MT_FMTS,
	.rates = SOC_HIGH_USE_RATE,
	.rate_min = SOC_HIGH_USE_RATE_MIN,
	.rate_max = SOC_HIGH_USE_RATE_MAX,
	.channels_min = SOC_NORMAL_USE_CHANNELS_MIN,
	.channels_max = SOC_NORMAL_USE_CHANNELS_MAX,
	.buffer_bytes_max = SOC_NORMAL_USE_BUFFERSIZE_MAX,
	.period_bytes_max = SOC_NORMAL_USE_BUFFERSIZE_MAX,
	.periods_min = SOC_NORMAL_USE_PERIODS_MIN,
	.periods_max = SOC_NORMAL_USE_PERIODS_MAX,
	.fifo_size = 0,
};

static int mtk_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;

	runtime->hw = mtk_dummy_hardware;
	return 0;
}

static int mtk_dummypcm_close(struct snd_pcm_substream *substream)
{
	return 0;
}

static int mtk_dummypcm_trigger(struct snd_pcm_substream *substream, int cmd)
{

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		break;
	}
	return -EINVAL;
}

static int mtk_pcm_copy(struct snd_pcm_substream *substream,
			int channel,
			unsigned long pos,
			void __user *buf,
			unsigned long bytes)
{

	return 0;
}

static int mtk_pcm_silence(struct snd_pcm_substream *substream,
			   int channel,
			   unsigned long pos,
			   unsigned long bytes)
{

	return 0; /* do nothing */
}

static void *dummy_page[2];

static struct page *mtk_pcm_page(struct snd_pcm_substream *substream,
				 unsigned long offset)
{
	return virt_to_page(dummy_page[substream->stream]); /* the same page */
}

static int mtk_pcm_prepare(struct snd_pcm_substream *substream)
{

	return 0;
}

static int mtk_pcm_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *hw_params)
{
	int ret = 0;

	return ret;
}

static int mtk_dummy_pcm_hw_free(struct snd_pcm_substream *substream)
{
	return snd_pcm_lib_free_pages(substream);
}

static struct snd_pcm_ops mtk_afe_ops = {
	.open = mtk_pcm_open,
	.close = mtk_dummypcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = mtk_pcm_hw_params,
	.hw_free = mtk_dummy_pcm_hw_free,
	.prepare = mtk_pcm_prepare,
	.trigger = mtk_dummypcm_trigger,
	.copy_user = mtk_pcm_copy,
	.fill_silence = mtk_pcm_silence,
	.page = mtk_pcm_page,
};

static const struct snd_soc_component_driver mtk_soc_dummy_component = {
	.name = AFE_PCM_NAME,
	.ops = &mtk_afe_ops,
	.probe = mtk_afe_dummy_component_probe,
};

static int mtk_dummy_probe(struct platform_device *pdev)
{
	pr_debug("%s\n", __func__);

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_DUMMY_PCM);
	pdev->name = pdev->dev.kobj.name;

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));
	return snd_soc_register_component(&pdev->dev,
					  &mtk_soc_dummy_component,
					  NULL,
					  0);
}

static int mtk_afe_dummy_component_probe(struct snd_soc_component *component)
{
	pr_debug("%s\n", __func__);
	return 0;
}

static int mtk_afedummy_remove(struct platform_device *pdev)
{
	snd_soc_unregister_component(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_pcm_dummy_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_pcm_dummy",
	},
	{} };
#endif

static struct platform_driver mtk_afedummy_driver = {
	.driver = {

			.name = MT_SOC_DUMMY_PCM,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_pcm_dummy_of_ids,
#endif
		},
	.probe = mtk_dummy_probe,
	.remove = mtk_afedummy_remove,
};

#ifndef CONFIG_OF
static struct platform_device *soc_mtkafe_dummy_dev;
#endif

static int __init mtk_soc_dummy_platform_init(void)
{
	int ret = 0;

	pr_debug("%s\n", __func__);
#ifndef CONFIG_OF
	soc_mtkafe_dummy_dev = platform_device_alloc(MT_SOC_DUMMY_PCM, -1);
	if (!soc_mtkafe_dummy_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_mtkafe_dummy_dev);
	if (ret != 0) {
		platform_device_put(soc_mtkafe_dummy_dev);
		return ret;
	}
#endif
	ret = platform_driver_register(&mtk_afedummy_driver);

	return ret;
}
module_init(mtk_soc_dummy_platform_init);

static void __exit mtk_soc_dummy_platform_exit(void)
{

	platform_driver_unregister(&mtk_afedummy_driver);
}
module_exit(mtk_soc_dummy_platform_exit);

MODULE_DESCRIPTION("AFE PCM module platform driver");
MODULE_LICENSE("GPL");
