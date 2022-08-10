// SPDX-License-Identifier: GPL-2.0
//
// mtk-scp-audio-mem-control.c --  Mediatek scp audio memory control
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Zhixiong <Zhixiong.Wang@mediatek.com>

/* platform related header file*/
#include "scp.h"
#include "mtk-scp-audio-mem-control.h"
#include "mtk-scp-audio-pcm.h"

static DEFINE_MUTEX(scp_audio_control_mutex);
static DEFINE_MUTEX(scp_audio_mutex_request_dram);

/*
 * gen_pool_create - create a new special memory pool
 * @min_alloc_order: log base 2 of number of bytes each bitmap bit represents
 * @nid: node id of the node the pool structure should be allocated on, or -1
 */
int scp_aud_gen_pool_create(int min_alloc_order, int nid)
{
	int ret = 0;
	unsigned long va_start;
	size_t va_chunk;
	struct gen_pool *genpool;
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();
	struct scp_audio_reserve_mem *scp_audio_rsv_mem = &scp_audio->rsv_mem;

	if (min_alloc_order <= 0)
		return -1;

	genpool = gen_pool_create(MIN_SCP_AUD_SHIFT, -1);
	if (!genpool)
		return -ENOMEM;

	va_start = scp_audio_rsv_mem->va_addr;
	va_chunk = scp_audio_rsv_mem->size;
	if ((!va_start) || (!va_chunk)) {
		ret = -1;
		return ret;
	}
	if (gen_pool_add_virt(genpool, (unsigned long)va_start,
			      scp_audio_rsv_mem->phy_addr, va_chunk,
			      -1)) {
		pr_warn(
			"%s failed to add chunk va_start= 0x%lx va_chunk = %zu\n",
			__func__, va_start, va_chunk);
	}

	scp_audio->genpool = genpool;
	pr_info(
		"%s success to add chunk va_start= 0x%lx va_chunk = %zu genpool = %p\n",
		__func__, va_start, va_chunk, genpool);
	//TODO dump_mtk_adsp_gen_pool();
	return ret;
}

int mtk_scp_audio_init_mem(void)
{
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();
	struct scp_audio_reserve_mem *scp_audio_rsv_mem = &scp_audio->rsv_mem;
	struct scp_aud_task_base *task_base;
	unsigned long vaddr;
	dma_addr_t paddr;

	scp_audio_rsv_mem->phy_addr =
		scp_get_reserve_mem_phys(SCP_SPK_MEM_ID);
	scp_audio_rsv_mem->va_addr = (unsigned long long)
		scp_get_reserve_mem_virt(SCP_SPK_MEM_ID);
	scp_audio_rsv_mem->vir_addr =
		(unsigned char *)scp_get_reserve_mem_virt(SCP_SPK_MEM_ID);
	scp_audio_rsv_mem->size =
		scp_get_reserve_mem_size(SCP_SPK_MEM_ID);

	if (!scp_audio_rsv_mem->phy_addr) {
		pr_err("%s(), scp audio rsv mem phy_addr error\n", __func__);
		return -1;
	}

	if (!scp_audio_rsv_mem->vir_addr) {
		pr_err("%s(), scp audio rsv mem phy_addr vir_addr error\n",
		       __func__);
		return -1;
	}

	if (!scp_audio_rsv_mem->size) {
		pr_err("%s(), scp audio rsv mem phy_addr size error\n",
		       __func__);
		return -1;
	}

	memset_io((void *)scp_audio_rsv_mem->vir_addr, 0, scp_audio_rsv_mem->size);

	scp_aud_gen_pool_create(MIN_SCP_AUD_SHIFT, -1);

	//init A2D/D2A share mem
	task_base = get_taskbase_by_daiid(SCP_AUD_TASK_SPK_PROCESS_ID);
	vaddr = gen_pool_alloc(scp_audio->genpool, A2D_SHAREMEM_SIZE);
	if (!vaddr) {
		pr_err("%s(), alloc ATOD mem vaddr Fail!!!\n", __func__);
		return -1;
	}

	paddr = gen_pool_virt_to_phys(scp_audio->genpool, vaddr);
	task_base->msg_atod_share_buf.phy_addr = paddr;
	task_base->msg_atod_share_buf.va_addr = vaddr;
	task_base->msg_atod_share_buf.vir_addr = (char *)vaddr;
	task_base->msg_atod_share_buf.size = A2D_SHAREMEM_SIZE;

	dev_info(scp_audio->dev,
		 "%s(), a2d mem pa=0x%llx, va=0x%llx, size=0x%llx\n",
		 __func__,
		 paddr,
		 vaddr,
		 task_base->msg_atod_share_buf.size);

	vaddr = gen_pool_alloc(scp_audio->genpool, D2A_SHAREMEM_SIZE);
	if (!vaddr) {
		pr_err("%s(), alloc DTOA mem vaddr Fail!!!\n", __func__);
		return -1;
	}

	paddr = gen_pool_virt_to_phys(scp_audio->genpool, vaddr);
	task_base->msg_dtoa_share_buf.phy_addr = paddr;
	task_base->msg_dtoa_share_buf.va_addr = vaddr;
	task_base->msg_dtoa_share_buf.vir_addr = (char *)vaddr;
	task_base->msg_dtoa_share_buf.size = D2A_SHAREMEM_SIZE;

	dev_info(scp_audio->dev,
		 "%s(), d2a mem pa=0x%llx, va=0x%llx, size=0x%llx\n",
		 __func__,
		 paddr,
		 vaddr,
		 task_base->msg_dtoa_share_buf.size);
	dev_info(scp_audio->dev,
		 "%s(), scp reserve mem pa=0x%llx, va=0x%llx, size=0x%llx\n",
		 __func__,
		 scp_audio_rsv_mem->phy_addr,
		 scp_audio_rsv_mem->vir_addr,
		 scp_audio_rsv_mem->size);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_scp_audio_init_mem);

bool is_scp_genpool_addr_valid(struct snd_pcm_substream *substream)
{
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();
	struct gen_pool *genpool;

	if (scp_audio == NULL)
		return false;

	genpool = scp_audio->genpool;
	if (genpool == NULL) {
		pr_debug("%s genpool == NULL\n", __func__);
		return false;
	}

	return gen_pool_has_addr(genpool,
				 (unsigned long)substream->runtime->dma_area,
				 substream->runtime->dma_bytes);
}
EXPORT_SYMBOL(is_scp_genpool_addr_valid);

int scp_audio_allocate_sharemem_ring(struct scp_aud_task_base *taskbase,
				     unsigned int size,
				     struct gen_pool *genpool)
{
	struct ringbuf_bridge *buf_bridge;
	struct RingBuf *ring_buf;
	unsigned long vaddr;
	dma_addr_t paddr;

	if (genpool == NULL) {
		pr_warn("%s genpool == NULL\n", __func__);
		return -1;
	}

	buf_bridge = &(taskbase->share_hw_buf.aud_buffer.buf_bridge);
	ring_buf = &taskbase->ring_buf;

	/* allocate VA with gen pool */
	vaddr = gen_pool_alloc(genpool, size);
	if (!vaddr) {
		pr_err("%s(), alloc pcm mem vaddr Fail!!!\n", __func__);
		return -1;
	}

	paddr = gen_pool_virt_to_phys(genpool, vaddr);
	taskbase->ring_share_buf.phy_addr = paddr;
	taskbase->ring_share_buf.va_addr = vaddr;
	taskbase->ring_share_buf.vir_addr = (char *)vaddr;
	taskbase->ring_share_buf.size = size;

	init_ring_buf(ring_buf, (char *)vaddr, size);
	init_ring_buf_bridge(buf_bridge, (unsigned long long)paddr, size);

	return 0;
}
EXPORT_SYMBOL(scp_audio_allocate_sharemem_ring);

int scp_audio_free_sharemem_ring(struct scp_aud_task_base *taskbase,
				 struct gen_pool *genpool)
{
	struct ringbuf_bridge *buf_bridge;
	struct RingBuf *ring_buf;

	if (genpool == NULL) {
		pr_warn("%s genpool == NULL\n", __func__);
		return -1;
	}

	buf_bridge = &(taskbase->share_hw_buf.aud_buffer.buf_bridge);
	ring_buf = &taskbase->ring_buf;

	if (ring_buf->pBufBase != NULL) {
		gen_pool_free(genpool, (unsigned long)ring_buf->pBufBase,
		ring_buf->bufLen);
		RingBuf_Bridge_Clear(buf_bridge);
		RingBuf_Clear(ring_buf);
	} else
		pr_info("%s ring_buf->pBufBase = null\n", __func__);

	return 0;
}
EXPORT_SYMBOL(scp_audio_free_sharemem_ring);

int mtk_scp_allocate_mem(struct snd_pcm_substream *substream, unsigned int size)
{
	struct snd_dma_buffer *dma_buf = &substream->dma_buffer;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	int id = asoc_rtd_to_cpu(rtd, 0)->id;
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();
	//struct scp_audio_reserve_mem *scp_audio_rsv_mem = &scp_audio->rsv_mem;
	//int buf_offset;
	unsigned long vaddr;
	dma_addr_t paddr;

	if (scp_audio == NULL)
		return -1;

	if (id != scp_audio->dl_memif &&
	    id != scp_audio->ul_memif &&
	    id != scp_audio->ref_memif) {
		pr_err("%s(), not match scp audio memif\n", __func__);
		return -1;
	}

	//if (!scp_audio_rsv_mem->phy_addr ||
	//    !scp_audio_rsv_mem->vir_addr ||
	//    !scp_audio_rsv_mem->size) {
	//	pr_err("%s(), scp audio rsv mem error\n");
	//	return -1;
	//}

	vaddr = gen_pool_alloc(scp_audio->genpool, size);
	if (!vaddr) {
		pr_err("%s(), alloc afe mem vaddr Fail!!!\n", __func__);
		return -1;
	}

	paddr = gen_pool_virt_to_phys(scp_audio->genpool, vaddr);
	dma_buf->dev.type = SNDRV_DMA_TYPE_DEV;
	dma_buf->dev.dev = substream->pcm->card->dev;
	dma_buf->private_data = NULL;
	dma_buf->bytes = size;
	dma_buf->addr = paddr;
	dma_buf->area = (char *)vaddr;
	substream->runtime->dma_addr = dma_buf->addr;
	substream->runtime->dma_area = dma_buf->area;

	dev_info(scp_audio->dev,
		"%s(), scp audio VA:0x%p,PA:0x%lx,size:%d,using_sram=0\n",
		__func__,
		dma_buf->area,
		dma_buf->addr,
		dma_buf->bytes);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_scp_allocate_mem);

int mtk_scp_free_mem(struct snd_pcm_substream *substream)
{
	struct mtk_scp_audio_base *scp_audio = get_scp_audio_base();
	unsigned char **vaddr = &substream->runtime->dma_area;
	size_t *size = &substream->runtime->dma_bytes;
	struct gen_pool *genpool;

	if (scp_audio == NULL)
		return -1;

	genpool = scp_audio->genpool;
	if (genpool == NULL) {
		pr_warn("%s() genpool == NULL\n", __func__);
		return -1;
	}

	if (!vaddr || !size)
		return -EINVAL;

	if (!gen_pool_has_addr(genpool, (unsigned long)*vaddr, *size)) {
		pr_warn("%s() vaddr is not in genpool\n", __func__);
		return -EFAULT;
	}

	/* allocate VA with gen pool */
	if (*vaddr) {
		gen_pool_free(genpool, (unsigned long)*vaddr, *size);
		*vaddr = NULL;
		*size = 0;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mtk_scp_free_mem);

/* todo dram ?*/
int scp_audio_dram_request(struct device *dev)
{
	struct mtk_scp_audio_base *scp_audio = dev_get_drvdata(dev);

	mutex_lock(&scp_audio_mutex_request_dram);

	scp_audio->dram_resource_counter++;
	mutex_unlock(&scp_audio_mutex_request_dram);
	return 0;
}

int scp_audio_dram_release(struct device *dev)
{
	struct mtk_scp_audio_base *scp_audio = dev_get_drvdata(dev);

	mutex_lock(&scp_audio_mutex_request_dram);
	scp_audio->dram_resource_counter--;

	if (scp_audio->dram_resource_counter < 0) {
		dev_info(dev, "%s(), scp_audio_dram_resource_counter %d\n",
			 __func__, scp_audio->dram_resource_counter);
		scp_audio->dram_resource_counter = 0;
	}
	mutex_unlock(&scp_audio_mutex_request_dram);
	return 0;
}

