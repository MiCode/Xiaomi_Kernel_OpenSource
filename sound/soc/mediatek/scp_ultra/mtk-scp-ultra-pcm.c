// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2018 MediaTek Inc.

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>

#include "mtk-base-scp-ultra.h"
#include "mtk-scp-ultra-common.h"
#include "mtk-scp-ultra-mem-control.h"
#include "mtk-scp-ultra-platform-driver.h"
#include "ultra_ipi.h"

#define MTK_PCM_RATES (SNDRV_PCM_RATE_8000_48000 |\
			SNDRV_PCM_RATE_88200 |\
			SNDRV_PCM_RATE_96000 |\
			SNDRV_PCM_RATE_176400 |\
			SNDRV_PCM_RATE_192000)

#define MTK_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE |\
			SNDRV_PCM_FMTBIT_S24_LE |\
			SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_pcm_hardware scp_ultra_hardware = {
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
	.period_bytes_max = 0xC000,
	.periods_min = 2,
	.periods_max = 4096,
	.buffer_bytes_max = 0xC000,
	.fifo_size = 0,
};

static int scp_ultra_pcm_dev_probe(struct platform_device *pdev)
{
	struct device *dev;
	struct mtk_base_scp_ultra *scp_ultra;
	int ret = 0;

	dev_dbg(&pdev->dev, "%s()\n", __func__);

	scp_ultra = devm_kzalloc(&pdev->dev,
			sizeof(struct mtk_base_scp_ultra), GFP_KERNEL);
	if (!scp_ultra)
		return -ENOMEM;

	ret = of_property_read_u32(pdev->dev.of_node, "scp_ultra_dl_memif_id",
				   &scp_ultra->scp_ultra_dl_memif_id);
	if (ret != 0) {
		pr_info("%s scp_ultra_dl_memif_id error\n", __func__);
		return 0;
	}
	ret = of_property_read_u32(pdev->dev.of_node, "scp_ultra_ul_memif_id",
				   &scp_ultra->scp_ultra_ul_memif_id);
	if (ret != 0) {
		pr_info("%s scp_ultra_ul_memif_id error\n", __func__);
		return 0;
	}
	/*  register dsp dai driver*/
	scp_ultra->mtk_scp_hardware = &scp_ultra_hardware;
	scp_ultra->dev = &pdev->dev;
	scp_ultra->ultra_mem.ultra_ul_memif_id = -1;
	scp_ultra->ultra_mem.ultra_dl_memif_id = -1;
	scp_ultra->usnd_state = SCP_ULTRA_STATE_OFF;
	dev = scp_ultra->dev;

	platform_set_drvdata(pdev, scp_ultra);
	pm_runtime_enable(&pdev->dev);

	if (pdev->dev.of_node) {
		dev_set_name(&pdev->dev, "%s", "snd_scp_ultra");
		pdev->name = pdev->dev.kobj.name;
	} else {
		pr_debug("%s(), pdev->dev.of_node NULL!!!\n", __func__);
	}

	ret = snd_soc_register_platform(&pdev->dev,
			&mtk_scp_ultra_pcm_platform);
	if (ret) {
		dev_warn(&pdev->dev, "err_platform\n");
		goto err_platform;
	}
	set_scp_ultra_base((void *)scp_ultra);

	return 0;

err_platform:
	snd_soc_unregister_platform(&pdev->dev);

	return ret;
}

static int scp_ultra_pcm_dev_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id scp_ultra_pcm_dt_match[] = {
	{ .compatible = "mediatek,snd_scp_ultra", },
};
MODULE_DEVICE_TABLE(of, scp_ultra_pcm_dt_match);

static struct platform_driver scp_ultra_pcm_driver = {
	.driver = {
		   .name = "snd_scp_ultra",
		   .owner = THIS_MODULE,
		   .of_match_table = scp_ultra_pcm_dt_match,
	},
	.probe = scp_ultra_pcm_dev_probe,
	.remove = scp_ultra_pcm_dev_remove,
};

module_platform_driver(scp_ultra_pcm_driver);

MODULE_DESCRIPTION("Mediatek ALSA SoC scp ultrasound platform driver");
MODULE_AUTHOR("Youwei Dong <Youwei.Dong@mediatek.com>");
MODULE_LICENSE("GPL v2");

