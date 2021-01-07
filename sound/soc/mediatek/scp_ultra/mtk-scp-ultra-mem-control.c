// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 MediaTek Inc.
// Copyright (C) 2020 XiaoMi, Inc.

#include "mtk-scp-ultra-mem-control.h"
#include "mtk-scp-ultra-common.h"
#include "mtk-base-afe.h"
#include "mtk-base-scp-ultra.h"
#include "mtk-afe-fe-dai.h"
#include "audio_buf.h"
#include <sound/soc.h>
#include <linux/device.h>
#include <linux/compat.h>
#include <linux/io.h>
#include "scp_helper.h"
#include "scp_ipi.h"
#include "audio_ipi_platform.h"
#include "mtk-sram-manager.h"
#include "mtk-scp-ultra-platform-mem-control.h"
#include "audio_ultra_msg_id.h"
#include "audio_buf.h"

static struct audio_dsp_dram scp_ultra_reserved_mem;

int mtk_scp_ultra_reserved_dram_init(void)
{
	struct mtk_base_scp_ultra *scp_ultra = get_scp_ultra_base();
	struct audio_dsp_dram *dump_resv_mem =
			&scp_ultra->ultra_dump.dump_resv_mem;

	scp_ultra_reserved_mem.phy_addr =
		scp_get_reserve_mem_phys(ULTRA_MEM_ID);
	scp_ultra_reserved_mem.vir_addr =
		(unsigned char *)scp_get_reserve_mem_virt(ULTRA_MEM_ID);
	scp_ultra_reserved_mem.size =
		scp_get_reserve_mem_size(ULTRA_MEM_ID);

	if (!scp_ultra_reserved_mem.phy_addr) {
		pr_err("%s(), scp_ultra_reserved_mem phy_addr error\n");
		return -1;
	}

	if (!scp_ultra_reserved_mem.vir_addr) {
		pr_err("%s(), scp_ultra_reserved_mem vir_addr error\n");
		return -1;
	}

	if (!scp_ultra_reserved_mem.size) {
		pr_err("%s(), scp_ultra_reserved_mem size error\n");
		return -1;
	}

	memset_io((void *)scp_ultra_reserved_mem.vir_addr, 0,
		  scp_ultra_reserved_mem.size);

	dump_resv_mem->phy_addr =
		scp_get_reserve_mem_phys(ULTRA_MEM_ID);
	dump_resv_mem->vir_addr =
		(unsigned char *)scp_get_reserve_mem_virt
				 (ULTRA_MEM_ID);
	dump_resv_mem->size =
		scp_get_reserve_mem_size(ULTRA_MEM_ID);

	if (!dump_resv_mem->phy_addr) {
		pr_err("%s(), dump_resv_mem phy_addr error\n");
		return -1;
	}

	if (!dump_resv_mem->vir_addr) {
		pr_err("%s(), dump_resv_mem vir_addr error\n");
		return -1;
	}

	if (!dump_resv_mem->size) {
		pr_err("%s(), dump_resv_mem size error\n");
		return -1;
	}

	memset_io((void *)dump_resv_mem->vir_addr, 0,
		  dump_resv_mem->size);

	dev_info(scp_ultra->dev,
		 "%s(), dram pa=%p,va=%p,size=0x%x,dump pa=%p,va=%p,size=0x%x\n",
		 __func__,
		 scp_ultra_reserved_mem.phy_addr,
		 scp_ultra_reserved_mem.vir_addr,
		 scp_ultra_reserved_mem.size,
		 dump_resv_mem->phy_addr,
		 dump_resv_mem->vir_addr,
		 dump_resv_mem->size);

	return 0;
}

int mtk_scp_ultra_free_mem(struct snd_pcm_substream *substream,
			 struct mtk_base_afe *afe)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	struct mtk_base_scp_ultra *scp_ultra = get_scp_ultra_base();
	struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;

	dev_info(scp_ultra->dev, "%s(), %s\n", __func__, memif->data->name);

	if (id == get_scp_ultra_memif_id(SCP_ULTRA_DL_DAI_ID)) {
		ultra_mem->ultra_dl_memif_id = -1;
		// reset_audio_dma_buf(&ultra_mem->ultra_dl_dma_buf);
	} else if (id == get_scp_ultra_memif_id(SCP_ULTRA_UL_DAI_ID)) {
		ultra_mem->ultra_ul_memif_id = -1;
		// reset_audio_dma_buf(&ultra_mem->ultra_ul_dma_buf);
	} else {
		dev_err(scp_ultra->dev, "%s(), wrong memif id\n");
		return -EINVAL;
	}

	if (memif->using_sram) {
		memif->using_sram = 0;
		mtk_audio_sram_free(afe->sram, substream);
	} else {
		if (afe->release_dram_resource)
			afe->release_dram_resource(afe->dev);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_scp_ultra_free_mem);

int mtk_scp_ultra_allocate_mem(struct snd_pcm_substream *substream,
			     dma_addr_t *phys_addr, unsigned char **virt_addr,
			     unsigned int size,
			     snd_pcm_format_t format,
			     struct mtk_base_afe *afe)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_ultra *scp_ultra = get_scp_ultra_base();
	struct mtk_base_scp_ultra_mem *ultra_mem = &scp_ultra->ultra_mem;
	int id = rtd->cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	struct snd_dma_buffer *ultra_dma_buf = NULL;
	int buf_offset;
	int ret;

	dev_err(scp_ultra->dev, "%s(),\n", __func__);
	if (id == get_scp_ultra_memif_id(SCP_ULTRA_DL_DAI_ID)) {
		dev_err(scp_ultra->dev, "SCP_ULTRA_DL_DAI_ID id = %d\n", id);
		ultra_dma_buf = &ultra_mem->ultra_dl_dma_buf;
		ultra_mem->ultra_dl_memif_id = id;
		buf_offset = ULTRA_BUF_OFFSET;
	} else if (id == get_scp_ultra_memif_id(SCP_ULTRA_UL_DAI_ID)) {
		dev_err(scp_ultra->dev, "SCP_ULTRA_UL_DAI_ID id = %d\n", id);
		ultra_dma_buf = &ultra_mem->ultra_ul_dma_buf;
		ultra_mem->ultra_ul_memif_id = id;
		buf_offset = ULTRA_BUF_OFFSET * 2;
	}  else {
		dev_err(scp_ultra->dev, "%s(), wrong memif id\n");
		return -EINVAL;
	}

	ultra_dma_buf->bytes = size;
	if (mtk_audio_sram_allocate(afe->sram,
				    &ultra_dma_buf->addr,
				    &ultra_dma_buf->area,
				    ultra_dma_buf->bytes,
				    substream,
				    format, false) == 0) {
		memif->using_sram = 1;
	} else {
		memif->using_sram = 0;
		ultra_dma_buf->addr =
			scp_ultra_reserved_mem.phy_addr + buf_offset;
		ultra_dma_buf->area =
			scp_ultra_reserved_mem.vir_addr + buf_offset;
	}

	memset_io(ultra_dma_buf->area, 0, ultra_dma_buf->bytes);

//BYPASS_RSVED_MEM_ALLOCATE:
	/* set memif addr */
	ret = mtk_memif_set_addr(afe, id,
				 ultra_dma_buf->area,
				 ultra_dma_buf->addr,
				 ultra_dma_buf->bytes);
	if (ret) {
		dev_err(afe->dev, "%s(), error, id %d, set addr, ret %d\n",
			__func__, id, ret);
		return ret;
	}

	dev_info(scp_ultra->dev,
		 "%s(), ultra VA:0x%p, ultra PA:0x%lx, size:%d,memif->using_sram = %d\n",
		 __func__,
		 ultra_dma_buf->area,
		 ultra_dma_buf->addr,
		 ultra_dma_buf->bytes,
		 memif->using_sram);

	if (memif->using_sram == 0 && afe->request_dram_resource)
		afe->request_dram_resource(afe->dev);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_scp_ultra_allocate_mem);

