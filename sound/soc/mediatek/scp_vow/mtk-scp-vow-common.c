// SPDX-License-Identifier: GPL-2.0
//
// mtk-scp-vow-common.c  --
//
// Copyright (c) 2019 MediaTek Inc.
// Author: Poyen <Poyen.wu@mediatek.com>



#include <linux/io.h>
#include "mtk-sram-manager.h"
#include "mtk-base-afe.h"
#include "mtk-afe-fe-dai.h"
#include "scp_helper.h"
#include "mtk-scp-vow-common.h"
#include "mtk-scp-vow-platform.h"


int allocate_vow_bargein_mem(struct snd_pcm_substream *substream,
			     dma_addr_t *phys_addr,
			     unsigned char **virt_addr,
			     unsigned int size,
			     snd_pcm_format_t format,
			     struct mtk_base_afe *afe)
{
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct mtk_base_afe_memif *memif;
	int id;
	int ret = 0;

	id = get_scp_vow_memif_id();
	memif = &afe->memif[id];

	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;
	dma_buf->bytes = size;

	if (mtk_audio_sram_allocate(afe->sram,
				    &dma_buf->addr,
				    &dma_buf->area,
				    dma_buf->bytes,
				    substream,
				    format, false) == 0) {
		/* Using SRAM */
		dev_info(afe->dev, "%s(), use SRAM\n", __func__);
		memif->using_sram = 1;
	} else {
		/* Using DRAM */
		dev_info(afe->dev, "%s(), use DRAM\n", __func__);
		dma_buf->addr = scp_get_reserve_mem_phys(VOW_BARGEIN_MEM_ID);
		dma_buf->area =
		    (uint8_t *)scp_get_reserve_mem_virt(VOW_BARGEIN_MEM_ID);
		memif->using_sram = 0;

	}

	memset_io(dma_buf->area, 0, dma_buf->bytes);
	ret = mtk_memif_set_addr(afe, id,
				 dma_buf->area,
				 dma_buf->addr,
				 dma_buf->bytes);
	if (ret) {
		dev_info(afe->dev, "%s(), error, set addr, ret %d\n",
			 __func__, ret);
		return ret;
	}

	dev_info(afe->dev, "%s(), addr = %pad, area = %p, bytes = %zu\n",
		 __func__, &dma_buf->addr, dma_buf->area,
		 dma_buf->bytes);

	if ((memif->using_sram == 0) && (afe->request_dram_resource))
		afe->request_dram_resource(afe->dev);

	return ret;
}

int get_scp_vow_memif_id(void)
{
	return get_scp_vow_memif_platform_id();
}
