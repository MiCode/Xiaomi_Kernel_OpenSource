// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/****************************************************************************
 *
 * Filename:
 * ---------
 *   mtk_routing
 *
 * Project:
 * --------
 *
 *
 * Description:
 * ------------
 *   Audio routing path
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *----------------------------------------------------------------------------
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

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/soc.h>

#include "mtk-auddrv-afe.h"
#include "mtk-auddrv-ana.h"
#include "mtk-auddrv-clk.h"
#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-auddrv-kernel.h"

static struct snd_soc_dai_driver mtk_routing_dai[] = {
	{
		.playback = {

				.stream_name = MT_SOC_ROUTING_STREAM_NAME,
				.rates = SNDRV_PCM_RATE_8000_48000,
				.formats = SNDRV_PCM_FMTBIT_S16_LE,
				.channels_min = 1,
				.channels_max = 2,
				.rate_min = 8000,
				.rate_max = 48000,
			},
		.name = "PLATOFRM_CONTROL",
	},
};

static const struct snd_soc_component_driver dai_routing_component = {
	.name = "PLATOFRM_CONTROL",
};

static int mtk_routing_dev_probe(struct platform_device *pdev)
{
	int rc = 0;

	pr_debug("%s(), name = %s\n", __func__, dev_name(&pdev->dev));

	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	if (pdev->dev.dma_mask == NULL)
		pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	if (pdev->dev.of_node)
		dev_set_name(&pdev->dev, "%s", MT_SOC_ROUTING_DAI_NAME);
	pdev->name = pdev->dev.kobj.name;

	pr_debug("%s: dev name %s\n", __func__, dev_name(&pdev->dev));

	rc = snd_soc_register_component(&pdev->dev, &dai_routing_component,
					mtk_routing_dai,
					ARRAY_SIZE(mtk_routing_dai));
	return rc;
}

static int mtk_routing_dev_remove(struct platform_device *pdev)
{
	pr_debug("%s:\n", __func__);

	snd_soc_unregister_component(&pdev->dev);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id mt_soc_dai_routing_of_ids[] = {
	{
		.compatible = "mediatek,mt_soc_dai_routing",
	},
	{} };
#endif

static struct platform_driver mtk_routing_driver = {
	.probe = mtk_routing_dev_probe,
	.remove = mtk_routing_dev_remove,
	.driver = {

			.name = MT_SOC_ROUTING_DAI_NAME,
			.owner = THIS_MODULE,
#ifdef CONFIG_OF
			.of_match_table = mt_soc_dai_routing_of_ids,
#endif
		},
};

#ifndef CONFIG_OF
static struct platform_device *soc_routing_dev;
#endif

static int __init mtk_routing_init(void)
{
	pr_debug("%s:\n", __func__);
#ifndef CONFIG_OF
	int ret;

	soc_routing_dev = platform_device_alloc(MT_SOC_ROUTING_DAI_NAME, -1);

	if (!soc_routing_dev)
		return -ENOMEM;

	ret = platform_device_add(soc_routing_dev);

	if (ret != 0) {
		platform_device_put(soc_routing_dev);
		return ret;
	}
#endif
	return platform_driver_register(&mtk_routing_driver);
}
module_init(mtk_routing_init);

static void __exit mtk_routing_exit(void)
{
	pr_debug("%s:\n", __func__);

	platform_driver_unregister(&mtk_routing_driver);
}
module_exit(mtk_routing_exit);

/* Module information */
MODULE_DESCRIPTION("MTK Routing driver");
MODULE_LICENSE("GPL v2");
