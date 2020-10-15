// SPDX-License-Identifier: GPL-2.0
/*
 *  MediaTek SCP VoW
 *
 *  Copyright (c) 2020 MediaTek Inc.
 *  Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

#include <linux/module.h>
#include "mtk-base-afe.h"
#include "mtk-sram-manager.h"

#include "mtk-scp-vow.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#include "scp.h"
#endif

int mtk_scp_vow_barge_in_allocate_mem(struct snd_pcm_substream *substream,
			     dma_addr_t *phys_addr,
			     unsigned char **virt_addr,
			     unsigned int size,
			     struct mtk_base_afe *afe)
{
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;
	dma_buf->bytes = size;

	dev_info(afe->dev, "%s(), use DRAM\n", __func__);
#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
	dma_buf->addr = scp_get_reserve_mem_phys(VOW_BARGEIN_MEM_ID);
	dma_buf->area =
		(uint8_t *)scp_get_reserve_mem_virt(VOW_BARGEIN_MEM_ID);

	*phys_addr = dma_buf->addr;
	*virt_addr = dma_buf->area;
#else
	dev_err(afe->dev, "%s(), error, SCP not support\n", __func__);
	return -EINVAL;
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_scp_vow_barge_in_allocate_mem);

MODULE_DESCRIPTION("Mediatek SCP VoW Common Driver");
MODULE_AUTHOR("Michael Hsiao <michael.hsiao@mediatek.com>");
MODULE_LICENSE("GPL v2");

