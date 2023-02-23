// SPDX-License-Identifier: GPL-2.0
//
// Copyright (C) 2018 MediaTek Inc.

#include "mtk-scp-spk-mem-control.h"
#include "mtk-scp-spk-common.h"
#include "mtk-base-afe.h"
#include "mtk-base-scp-spk.h"
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
#include "mtk-scp-spk-platform-mem-control.h"
#include "audio_spkprotect_msg_id.h"

static struct audio_dsp_dram scp_spk_reserved_mem;
static struct audio_dsp_dram scp_spk_iv_tcm_buf;

int mtk_scp_spk_reserved_dram_init(void)
{
	struct mtk_base_scp_spk *scp_spk = get_scp_spk_base();
	struct audio_dsp_dram *dump_resv_mem =
			&scp_spk->spk_dump.dump_resv_mem;

	scp_spk_reserved_mem.phy_addr =
		scp_get_reserve_mem_phys(SPK_PROTECT_MEM_ID);
	scp_spk_reserved_mem.vir_addr =
		(unsigned char *)scp_get_reserve_mem_virt(SPK_PROTECT_MEM_ID);
	scp_spk_reserved_mem.size =
		scp_get_reserve_mem_size(SPK_PROTECT_MEM_ID);

	if (!scp_spk_reserved_mem.phy_addr) {
		pr_err("%s(), scp_spk_reserved_mem phy_addr error\n");
		return -1;
	}

	if (!scp_spk_reserved_mem.vir_addr) {
		pr_err("%s(), scp_spk_reserved_mem vir_addr error\n");
		return -1;
	}

	if (!scp_spk_reserved_mem.size) {
		pr_err("%s(), scp_spk_reserved_mem size error\n");
		return -1;
	}

	memset_io((void *)scp_spk_reserved_mem.vir_addr, 0,
		  scp_spk_reserved_mem.size);

	dump_resv_mem->phy_addr =
		scp_get_reserve_mem_phys(SPK_PROTECT_DUMP_MEM_ID);
	dump_resv_mem->vir_addr =
		(unsigned char *)scp_get_reserve_mem_virt
				 (SPK_PROTECT_DUMP_MEM_ID);
	dump_resv_mem->size =
		scp_get_reserve_mem_size(SPK_PROTECT_DUMP_MEM_ID);

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

	dev_info(scp_spk->dev,
		 "%s(), reserved dram: pa %p, va %p, size 0x%x, reserved dump dram: pa %p, va %p, size 0x%x\n",
		 __func__,
		 scp_spk_reserved_mem.phy_addr,
		 scp_spk_reserved_mem.vir_addr,
		 scp_spk_reserved_mem.size,
		 dump_resv_mem->phy_addr,
		 dump_resv_mem->vir_addr,
		 dump_resv_mem->size);

	return 0;
}

int mtk_scp_spk_allocate_tcm_iv_buf(void)
{
	struct mtk_base_scp_spk *scp_spk = get_scp_spk_base();
	struct mtk_base_scp_spk_mem *spk_mem = &scp_spk->spk_mem;
	struct ipi_msg_t ipi_msg;
	int ret = 0;

	memset((void *)&ipi_msg, 0, sizeof(struct ipi_msg_t));
	ret = audio_send_ipi_msg(&ipi_msg,
				 TASK_SCENE_SPEAKER_PROTECTION,
				 AUDIO_IPI_LAYER_TO_DSP,
				 AUDIO_IPI_MSG_ONLY,
				 AUDIO_IPI_MSG_NEED_ACK,
				 SPK_PROTECT_GET_TCM_BUF,
				 0, 0, NULL);

	if (ret < 0) {
		dev_err(scp_spk->dev, "%s(), send error:%d\n",
			__func__, ret);
		return -1;
	}

	if (ipi_msg.param1) {
		scp_spk_iv_tcm_buf.phy_addr =
			SCP_TCM_PHY_ADDR + ipi_msg.param1;
		scp_spk_iv_tcm_buf.vir_addr =
			(unsigned char *)(SCP_TCM + ipi_msg.param1);
		scp_spk_iv_tcm_buf.size = ipi_msg.param2;

		dev_info(scp_spk->dev,
			 "%s(), spk tcm iv pa:0x%x, va:0x%x, bytes:%d, SCP_TCM:0x%x, ipi_msg.param1:0x%x\n",
			 __func__,
			 scp_spk_iv_tcm_buf.phy_addr,
			 scp_spk_iv_tcm_buf.vir_addr,
			 scp_spk_iv_tcm_buf.size, SCP_TCM, ipi_msg.param1);
		spk_mem->is_iv_buf_in_tcm = true;
	} else {
		dev_info(scp_spk->dev,
			 "%s(), not support\n", __func__);
		spk_mem->is_iv_buf_in_tcm = false;
	}

	return 0;
}

int mtk_scp_spk_free_mem(struct snd_pcm_substream *substream,
			 struct mtk_base_afe *afe)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = rtd->cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	struct mtk_base_scp_spk *scp_spk = get_scp_spk_base();
	struct mtk_base_scp_spk_mem *spk_mem = &scp_spk->spk_mem;

	dev_info(scp_spk->dev, "%s(), %s\n", __func__, memif->data->name);

	if (id == get_scp_spk_memif_id(SCP_SPK_DL_DAI_ID)) {
		spk_mem->spk_dl_memif_id = -1;
		reset_audio_dma_buf(&spk_mem->spk_dl_dma_buf);
	} else if (id == get_scp_spk_memif_id(SCP_SPK_IV_DAI_ID)) {
		spk_mem->spk_iv_memif_id = -1;
		reset_audio_dma_buf(&spk_mem->spk_iv_dma_buf);
	} else if (id == get_scp_spk_memif_id(SCP_SPK_MDUL_DAI_ID)) {
		spk_mem->spk_md_ul_memif_id = -1;
		reset_audio_dma_buf(&spk_mem->spk_md_ul_dma_buf);
	} else {
		dev_err(scp_spk->dev, "%s(), wrong memif id\n");
		return -EINVAL;
	}

	if (memif->using_sram) {
		memif->using_sram = 0;
		mtk_audio_sram_free(afe->sram, substream);
	} else {
		if (afe->release_dram_resource)
			afe->release_dram_resource(afe->dev);
	}
	substream->runtime->dma_addr = NULL;
	substream->runtime->dma_area = NULL;

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_scp_spk_free_mem);

void mtk_scp_spk_allocate_platform_buf(unsigned int size,
				       dma_addr_t *phys_addr,
				       unsigned char **virt_addr)
{
	struct mtk_base_scp_spk *scp_spk = get_scp_spk_base();
	struct mtk_base_scp_spk_mem *spk_mem = &scp_spk->spk_mem;

	spk_mem->platform_dma_buf.addr = scp_spk_reserved_mem.phy_addr;
	spk_mem->platform_dma_buf.area = scp_spk_reserved_mem.vir_addr;
	spk_mem->platform_dma_buf.bytes = size;
	memset_io(spk_mem->platform_dma_buf.area, 0,
		  spk_mem->platform_dma_buf.bytes);

	*phys_addr = spk_mem->platform_dma_buf.addr;
	*virt_addr = spk_mem->platform_dma_buf.area;

	dev_info(scp_spk->dev,
		 "%s(), spk platform VA:0x%p, PA:0x%lx, size:%d\n",
		 __func__,
		 spk_mem->platform_dma_buf.area,
		 spk_mem->platform_dma_buf.addr,
		 spk_mem->platform_dma_buf.bytes);
}

int mtk_scp_spk_allocate_mem(struct snd_pcm_substream *substream,
			     dma_addr_t *phys_addr, unsigned char **virt_addr,
			     unsigned int size,
			     snd_pcm_format_t format,
			     struct mtk_base_afe *afe)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct mtk_base_scp_spk *scp_spk = get_scp_spk_base();
	struct mtk_base_scp_spk_mem *spk_mem = &scp_spk->spk_mem;
	int id = rtd->cpu_dai->id;
	struct mtk_base_afe_memif *memif = &afe->memif[id];
	struct snd_dma_buffer *spk_dma_buf = NULL;
	int buf_offset;
	int ret;

	if (id == get_scp_spk_memif_id(SCP_SPK_DL_DAI_ID)) {
		spk_dma_buf = &spk_mem->spk_dl_dma_buf;
		spk_mem->spk_dl_memif_id = id;
		buf_offset = SPK_BUF_OFFSET;
	} else if (id == get_scp_spk_memif_id(SCP_SPK_IV_DAI_ID)) {
		spk_dma_buf = &spk_mem->spk_iv_dma_buf;
		spk_mem->spk_iv_memif_id = id;
		buf_offset = SPK_BUF_OFFSET * 2;
	} else if (id == get_scp_spk_memif_id(SCP_SPK_MDUL_DAI_ID)) {
		spk_dma_buf = &spk_mem->spk_md_ul_dma_buf;
		spk_mem->spk_md_ul_memif_id = id;
		buf_offset = 0;
	} else {
		dev_err(scp_spk->dev, "%s(), wrong memif id\n");
		return -EINVAL;
	}

	spk_dma_buf->bytes = size;
	if (mtk_audio_sram_allocate(afe->sram,
				    &spk_dma_buf->addr,
				    &spk_dma_buf->area,
				    spk_dma_buf->bytes,
				    substream,
				    format, false) == 0) {
		memif->using_sram = 1;
	} else {
		memif->using_sram = 0;

		if (id == get_scp_spk_memif_id(SCP_SPK_IV_DAI_ID) &&
		    spk_mem->is_iv_buf_in_tcm) {
			spk_dma_buf->addr =
				scp_spk_iv_tcm_buf.phy_addr;
			spk_dma_buf->area =
				scp_spk_iv_tcm_buf.vir_addr;
			spk_dma_buf->bytes =
				scp_spk_iv_tcm_buf.size;
			goto BYPASS_RSVED_MEM_ALLOCATE;
		}

		spk_dma_buf->addr =
			scp_spk_reserved_mem.phy_addr + buf_offset;
		spk_dma_buf->area =
			scp_spk_reserved_mem.vir_addr + buf_offset;
	}

	memset_io(spk_dma_buf->area, 0, spk_dma_buf->bytes);

BYPASS_RSVED_MEM_ALLOCATE:
	/* set memif addr */
	ret = mtk_memif_set_addr(afe, id,
				 spk_dma_buf->area,
				 spk_dma_buf->addr,
				 spk_dma_buf->bytes);
	if (ret) {
		dev_err(afe->dev, "%s(), error, id %d, set addr, ret %d\n",
			__func__, id, ret);
		return ret;
	}

	dev_info(scp_spk->dev,
		 "%s(), %s, spk VA:0x%p, spk PA:0x%lx, size:%d, iv buf in tcm:%d\n",
		 __func__, memif->data->name,
		 spk_dma_buf->area,
		 spk_dma_buf->addr,
		 spk_dma_buf->bytes,
		 spk_mem->is_iv_buf_in_tcm);

	*phys_addr = spk_dma_buf->addr;
	*virt_addr = (char *)spk_dma_buf->area;
	if (memif->using_sram == 0 && afe->request_dram_resource)
		afe->request_dram_resource(afe->dev);

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_scp_spk_allocate_mem);

