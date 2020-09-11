// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>

#include "mtk-base-scp-spk.h"
#include "mtk-scp-spk-common.h"
#include "mtk-scp-spk-mem-control.h"
#include "mtk-scp-spk-platform-driver.h"
#include "audio_ipi_client_spkprotect.h"


#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
		       SNDRV_PCM_RATE_88200 |\
		       SNDRV_PCM_RATE_96000 |\
		       SNDRV_PCM_RATE_176400 |\
		       SNDRV_PCM_RATE_192000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE |\
			 SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_pcm_hardware scp_spk_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_NO_PERIOD_WAKEUP |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_RESUME |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = (SNDRV_PCM_FMTBIT_S16_LE |
		    SNDRV_PCM_FMTBIT_S24_LE |
		    SNDRV_PCM_FMTBIT_S32_LE),
	.rates = MTK_PCM_RATES,
	.period_bytes_min = 256,
	.period_bytes_max = SPK_BUF_OFFSET,
	.periods_min = 2,
	.periods_max = 4096,
	.buffer_bytes_max = SPK_BUF_OFFSET,
	.fifo_size = 0,
};

static int scp_spk_pcm_dev_probe(struct platform_device *pdev)
{
	struct device *dev;
	struct mtk_base_scp_spk *scp_spk;
	int ret = 0;

	dev_info(&pdev->dev, "%s()\n", __func__);

	scp_spk = devm_kzalloc(&pdev->dev,
			       sizeof(struct mtk_base_scp_spk), GFP_KERNEL);
	if (!scp_spk)
		return -ENOMEM;

	scp_spk->spk_dump.dump_ops =
			devm_kzalloc(&pdev->dev,
				     sizeof(struct scp_spk_dump_ops),
				     GFP_KERNEL);
	if (!scp_spk->spk_dump.dump_ops)
		return -ENOMEM;

	/*  register dsp dai driver*/
	scp_spk->mtk_dsp_hardware = &scp_spk_hardware;
	scp_spk->dev = &pdev->dev;
	reset_audio_dma_buf(&scp_spk->spk_mem.platform_dma_buf);
	reset_audio_dma_buf(&scp_spk->spk_mem.spk_dl_dma_buf);
	reset_audio_dma_buf(&scp_spk->spk_mem.spk_iv_dma_buf);
	reset_audio_dma_buf(&scp_spk->spk_mem.spk_md_ul_dma_buf);
	scp_spk->spk_mem.spk_iv_memif_id = -1;
	scp_spk->spk_mem.spk_dl_memif_id = -1;
	scp_spk->spk_mem.spk_md_ul_memif_id = -1;
	dev = scp_spk->dev;

	platform_set_drvdata(pdev, scp_spk);
	pm_runtime_enable(&pdev->dev);

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", "snd_scp_spk");

	ret = snd_soc_register_platform(&pdev->dev, &mtk_scp_spk_pcm_platform);
	if (ret) {
		dev_warn(&pdev->dev, "err_platform\n");
		goto err_platform;
	}

	set_ipi_recv_private((void *)scp_spk);
	set_scp_spk_base((void *)scp_spk);

	return 0;

err_platform:
	snd_soc_unregister_platform(&pdev->dev);

	return ret;
}

static int scp_spk_pcm_dev_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id scp_spk_pcm_dt_match[] = {
	{ .compatible = "mediatek,snd_scp_spk", },
};
MODULE_DEVICE_TABLE(of, scp_spk_pcm_dt_match);

static struct platform_driver scp_spk_pcm_driver = {
	.driver = {
		   .name = "snd_scp_spk",
		   .owner = THIS_MODULE,
		   .of_match_table = scp_spk_pcm_dt_match,
	},
	.probe = scp_spk_pcm_dev_probe,
	.remove = scp_spk_pcm_dev_remove,
};

module_platform_driver(scp_spk_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC scp spk platform driver");
MODULE_AUTHOR("Shane Chien <Shane.Chien@mediatek.com>");
MODULE_LICENSE("GPL v2");

