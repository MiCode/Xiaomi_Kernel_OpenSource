// SPDX-License-Identifier: GPL-2.0
//
// mtk-dsp-mem-control.c --  Mediatek ADSP dmemory control
//
// Copyright (c) 2018 MediaTek Inc.
// Author: Chipeng <Chipeng.chang@mediatek.com>

#include <linux/genalloc.h>
#include <linux/string.h>
#include <linux/types.h>
#include <sound/pcm.h>

/* adsp or scp*/
#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
#include <adsp_helper.h>
#else
#include <scp_helper.h>
#endif

/* platform related header file*/
#include "dsp-platform-mem-control.h"
#include "mtk-dsp-mem-control.h"
#include "mtk-base-dsp.h"
#include "mtk-dsp-common.h"

#include <mtk_spm_resource_req.h>

#ifdef CONFIG_MTK_ACAO_SUPPORT
#include "mtk_mcdi_governor_hint.h"
#endif

/* page size */
#define MIN_DSP_SHIFT (8)

static DEFINE_MUTEX(adsp_control_mutex);
static DEFINE_MUTEX(adsp_mutex_request_dram);
static bool binitadsp_share_mem;
static struct audio_dsp_dram dsp_dram_buffer[AUDIO_DSP_SHARE_MEM_NUM];
static struct gen_pool *dsp_dram_pool[AUDIO_DSP_SHARE_MEM_NUM];
static struct snd_dma_buffer dma_audio_buffer[AUDIO_DSP_SHARE_MEM_NUM];

/* function */
static int get_dsp_dram_sement_by_id(struct audio_dsp_dram *buffer, int id);
static int checkdspbuffer(struct audio_dsp_dram *buffer);

#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
static int get_dsp_dram_sement_by_id(struct audio_dsp_dram *buffer, int id)
{
	int ret = 0;

	buffer->phy_addr =
		adsp_get_reserve_mem_phys(dsp_daiid_to_scp_reservedid(id));
	buffer->va_addr = (unsigned long long)
		adsp_get_reserve_mem_virt(dsp_daiid_to_scp_reservedid(id));
	buffer->vir_addr = adsp_get_reserve_mem_virt(
		dsp_daiid_to_scp_reservedid(id));
	buffer->size = adsp_get_reserve_mem_size(
		dsp_daiid_to_scp_reservedid(id));

	ret = checkdspbuffer(buffer);
	if (ret)
		pr_warn("get buffer id = %dis not get\n", id);

	return ret;
}
#else
static int get_dsp_dram_sement_by_id(struct audio_dsp_dram *buffer, int id)
{
	int ret = 0;

	buffer->phy_addr =
		scp_get_reserve_mem_phys(dsp_daiid_to_scp_reservedid(id));
	buffer->va_addr =
		scp_get_reserve_mem_virt(dsp_daiid_to_scp_reservedid(id));
	buffer->vir_addr = (unsigned char *)scp_get_reserve_mem_virt(
		dsp_daiid_to_scp_reservedid(id));
	buffer->size = (uint32_t)scp_get_reserve_mem_size(
		dsp_daiid_to_scp_reservedid(id));

	ret = checkdspbuffer(buffer);
	if (ret)
		pr_warn("get buffer id = %dis not get\n", id);

	return ret;
}
#endif

/* dram */
int dsp_dram_request(struct device *dev)
{
	struct mtk_base_dsp *dsp = dev_get_drvdata(dev);

	mutex_lock(&adsp_mutex_request_dram);
	dsp->dsp_dram_resource_counter++;
	mutex_unlock(&adsp_mutex_request_dram);
	return 0;
}

int dsp_dram_release(struct device *dev)
{
	struct mtk_base_dsp *dsp = dev_get_drvdata(dev);

	mutex_lock(&adsp_mutex_request_dram);
	dsp->dsp_dram_resource_counter--;

	if (dsp->dsp_dram_resource_counter < 0) {
		dev_warn(dev, "%s(), dsp_dram_resource_counter %d\n",
			 __func__, dsp->dsp_dram_resource_counter);
		dsp->dsp_dram_resource_counter = 0;
	}
	mutex_unlock(&adsp_mutex_request_dram);
	return 0;
}


unsigned int mtk_get_adsp_sharemem_size(int audio_task_id, int task_sharemem_id)
{
	struct audio_dsp_dram *adspsharemem =
		mtk_get_adsp_sharemem_block(audio_task_id);

	if (adspsharemem == NULL)
		return 0;

	return adspsharemem[task_sharemem_id].size;
}

/* init hare memory for msg */
int mtk_init_adsp_msg_sharemem(struct audio_dsp_dram *share_buf,
				unsigned long vaddr, unsigned long long paddr,
				int size)
{
	if (share_buf == NULL) {
		pr_warn("%s msg_atod_share_buf == NULL\n", __func__);
		return -1;
	}

	share_buf->phy_addr = paddr;
	share_buf->va_addr = vaddr;
	share_buf->vir_addr = (char *)vaddr;
	share_buf->size = size;

	return 0;
}

int mtk_adsp_genpool_allocate_sharemem_ring(struct mtk_base_dsp_mem *dsp_mem,
					    unsigned int size, int id)
{
	struct ringbuf_bridge *buf_bridge;
	struct RingBuf *ring_buf;
	struct gen_pool *gen_pool_dsp;
	unsigned long vaddr;
	dma_addr_t paddr;

	gen_pool_dsp = dsp_mem->gen_pool_buffer;

	if (gen_pool_dsp == NULL) {
		pr_warn("%s gen_pool_dsp == NULL\n", __func__);
		return -1;
	}

	buf_bridge = &(dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	ring_buf = &dsp_mem->ring_buf;

	/* allocate VA with gen pool */
	vaddr = gen_pool_alloc(gen_pool_dsp, size);
	paddr = gen_pool_virt_to_phys(gen_pool_dsp, vaddr);

	mtk_init_adsp_msg_sharemem(&dsp_mem->dsp_ring_share_buf, vaddr, paddr,
				    size);
	init_ring_buf(ring_buf, (char *)vaddr, size);
	init_ring_buf_bridge(buf_bridge, (unsigned long long)paddr, size);

	return 0;
}

int mtk_adsp_genpool_free_sharemem_ring(struct mtk_base_dsp_mem *dsp_mem,
					int id)
{
	struct ringbuf_bridge *buf_bridge;
	struct RingBuf *ring_buf;
	struct gen_pool *gen_pool_dsp;

	gen_pool_dsp = dsp_mem->gen_pool_buffer;

	if (gen_pool_dsp == NULL) {
		pr_warn("%s gen_pool_dsp == NULL\n", __func__);
		return -1;
	}

	buf_bridge = &(dsp_mem->adsp_buf.aud_buffer.buf_bridge);
	ring_buf = &dsp_mem->ring_buf;

	if (ring_buf->pBufBase != NULL) {
		gen_pool_free(gen_pool_dsp, (unsigned long)ring_buf->pBufBase,
		ring_buf->bufLen);
		RingBuf_Bridge_Clear(buf_bridge);
		RingBuf_Clear(ring_buf);
	} else
		pr_info("%s ring_buf->pBufBase = null\n", __func__);

	return 0;
}

int mtk_adsp_allocate_mem(struct snd_pcm_substream *substream,
			  unsigned int size,
			  int id)
{
	int ret = 0;

	if (mtk_adsp_dai_id_support_share_mem(id)) {
		if (substream->runtime->dma_area) {
			ret = mtk_adsp_genpool_free_memory(
				&substream->runtime->dma_area,
				&substream->runtime->dma_bytes,
				AUDIO_DSP_AFE_SHARE_MEM_ID);
		}
		ret =  mtk_adsp_genpool_allocate_memory
			(&substream->runtime->dma_area,
			 &substream->runtime->dma_addr,
			 size,
			 AUDIO_DSP_AFE_SHARE_MEM_ID);
	} else
		ret =  snd_pcm_lib_malloc_pages(substream, size);
	return ret;
}

int mtk_adsp_free_mem(struct snd_pcm_substream *substream,
		      unsigned int size,
		      int id)
{
	int ret = 0;

	if (mtk_adsp_dai_id_support_share_mem(id)) {
		ret = mtk_adsp_genpool_free_memory(
		       &substream->runtime->dma_area,
		       &substream->runtime->dma_bytes,
		       AUDIO_DSP_AFE_SHARE_MEM_ID);
	} else
		ret = snd_pcm_lib_free_pages(substream);
	return ret;
}

int mtk_adsp_genpool_allocate_memory(unsigned char **vaddr,
				    dma_addr_t *paddr,
				    unsigned int size,
				    int id)
{
	/* gen pool related */
	struct gen_pool *gen_pool_dsp = mtk_get_adsp_dram_gen_pool(id);

	if (gen_pool_dsp == NULL) {
		pr_warn("%s gen_pool_dsp == NULL\n", __func__);
		return -1;
	}

	/* allocate VA with gen pool */
	if (*vaddr == NULL) {
		*vaddr = (unsigned char *)gen_pool_alloc(gen_pool_dsp, size);
		*paddr = gen_pool_virt_to_phys(gen_pool_dsp,
					       (unsigned long)*vaddr);
	}

	pr_debug("%s size =%u id = %d vaddr = %p paddr =0x%llx\n", __func__,
		size, id, vaddr, (unsigned long long)*paddr);

	return 0;
}

int mtk_adsp_genpool_free_memory(unsigned char **vaddr,
				 size_t *size, int id)
{
	/* gen pool related */
	struct gen_pool *gen_pool_dsp = mtk_get_adsp_dram_gen_pool(id);

	if (gen_pool_dsp == NULL) {
		pr_warn("%s gen_pool_dsp == NULL\n", __func__);
		return -1;
	}

	/* allocate VA with gen pool */
	if (*vaddr) {
		gen_pool_free(gen_pool_dsp, (unsigned long)*vaddr, *size);
		*vaddr = NULL;
		*size = 0;
	}
;
	return 0;
}

int aud_genppol_allocate_sharemem_msg(struct mtk_base_dsp_mem *dsp_mem,
				      int size,
				      int id)
{
	int ret = 0;
	unsigned long vaddr;
	dma_addr_t paddr;
	struct gen_pool *gen_pool_dsp;

	if (dsp_mem == NULL) {
		pr_warn("get buffer dsp_mem == NULL\n");
		return -1;
	}

	/* get gen pool pointer */
	gen_pool_dsp = dsp_mem->gen_pool_buffer;

	if (size == 0 || id >= ADSP_TASK_SHAREMEM_NUM) {
		pr_info("%s size[%d] id[%d]\n", __func__,
			size, id);
		return -1;
	}

	/* allocate VA with gen pool*/
	vaddr = gen_pool_alloc(gen_pool_dsp, size);
	paddr = gen_pool_virt_to_phys(gen_pool_dsp, vaddr);

	switch (id) {
	case ADSP_TASK_ATOD_MSG_MEM: {
		ret = mtk_init_adsp_msg_sharemem
			(&dsp_mem->msg_atod_share_buf, vaddr,
			 paddr, (int)size);
		break;
	}
	case ADSP_TASK_DTOA_MSG_MEM: {
		ret = mtk_init_adsp_msg_sharemem
			(&dsp_mem->msg_dtoa_share_buf, vaddr,
			 paddr, (int)size);
		break;
	}
	default:
		pr_info("%s id[%d] not support\n",
			__func__, id);
		break;
	}

	if (ret < 0) {
		pr_warn(
			"mtk_init_adsp_msg_sharemem msg_atod_share_buf err\n");
		return ret;
	}

	return 0;
}

int scp_reservedid_to_dsp_daiid(int id)
{
	switch (id) {
	case ADSP_PRIMARY_MEM_ID:
		return AUDIO_TASK_PRIMARY_ID;
	case ADSP_VOIP_MEM_ID:
		return AUDIO_TASK_VOIP_ID;
	case ADSP_DEEPBUF_MEM_ID:
		return AUDIO_TASK_DEEPBUFFER_ID;
	case ADSP_PLAYBACK_MEM_ID:
		return AUDIO_TASK_PLAYBACK_ID;
	case ADSP_CAPTURE_UL1_MEM_ID:
		return AUDIO_TASK_CAPTURE_UL1_ID;
	case ADSP_A2DP_PLAYBACK_MEM_ID:
		return AUDIO_TASK_A2DP_ID;
	case ADSP_DATAPROVIDER_MEM_ID:
		return AUDIO_TASK_DATAPROVIDER_ID;
	case ADSP_CALL_FINAL_MEM_ID:
		return AUDIO_TASK_CALL_FINAL_ID;
	case ADSP_KTV_MEM_ID:
		return AUDIO_TASK_KTV_ID;
	default:
		pr_warn("%s id = %d\n", __func__, id);
		return -1;
	}
	return -1;
}

int dsp_daiid_to_scp_reservedid(int task_dai_id)
{
	switch (task_dai_id) {
	case AUDIO_TASK_VOIP_ID:
		return ADSP_VOIP_MEM_ID;
	case AUDIO_TASK_PRIMARY_ID:
		return ADSP_PRIMARY_MEM_ID;
	case AUDIO_TASK_OFFLOAD_ID:
		return ADSP_OFFLOAD_MEM_ID;
	case AUDIO_TASK_DEEPBUFFER_ID:
		return ADSP_DEEPBUF_MEM_ID;
	case AUDIO_TASK_PLAYBACK_ID:
		return ADSP_PLAYBACK_MEM_ID;
	case AUDIO_DSP_AFE_SHARE_MEM_ID:
		return ADSP_AFE_MEM_ID;
	case AUDIO_TASK_CAPTURE_UL1_ID:
		return ADSP_CAPTURE_UL1_MEM_ID;
	case AUDIO_TASK_A2DP_ID:
		return ADSP_A2DP_PLAYBACK_MEM_ID;
	case AUDIO_TASK_DATAPROVIDER_ID:
		return ADSP_DATAPROVIDER_MEM_ID;
	case AUDIO_TASK_CALL_FINAL_ID:
		return ADSP_CALL_FINAL_MEM_ID;
	case AUDIO_TASK_KTV_ID:
		return ADSP_KTV_MEM_ID;
	default:
		pr_warn("%s id = %d\n", __func__, task_dai_id);
		return -1;
	}
	return -1;
}

int set_task_attr(int dsp_id, int task_enum, int param)
{

	struct mtk_adsp_task_attr *task_attr =
		mtk_get_adsp_task_attr(dsp_id);

	if (!task_attr) {
		pr_warn("%s get task NULL  dsp_id %d\n", __func__, dsp_id);
		return -1;
	}

	if (dsp_id >= AUDIO_TASK_DAI_NUM) {
		pr_warn("%s dspid %d\n", __func__, dsp_id);
		return -1;
	}
	if (task_enum >= ADSP_TASK_ATTR_NUM) {
		pr_warn("%s task_enum %d\n", __func__, task_enum);
		return -1;
	}

	switch (task_enum) {
	case ADSP_TASK_ATTR_DEFAULT:
		task_attr->default_enable = param;
		break;
	case ADSP_TASK_ATTR_MEMDL:
		task_attr->afe_memif_dl = param;
		break;
	case ADSP_TASK_ATTR_MEMUL:
		task_attr->afe_memif_ul = param;
		break;
	case ADSP_TASK_ATTR_MEMREF:
		task_attr->afe_memif_ref = param;
		break;
	case ADSP_TASK_ATTR_RUMTIME:
		task_attr->runtime_enable = param;
		break;
	case ADSP_TASK_ATTR_SMARTPA:
		task_attr->spk_protect_in_dsp = param;
		break;
	}
	return 0;
}

int get_task_attr(int dsp_id, int task_enum)
{
	struct mtk_adsp_task_attr *task_attr =
		mtk_get_adsp_task_attr(dsp_id);

	if (dsp_id >= AUDIO_TASK_DAI_NUM) {
		pr_warn("%s dsp_id %d\n", __func__, dsp_id);
		return -1;
	}
	if (task_enum >= ADSP_TASK_ATTR_NUM) {
		pr_warn("%s task_enum %d\n", __func__, task_enum);
		return -1;
	}
	if (task_attr == NULL) {
		pr_warn("%s task_attr is NULL\n", __func__);
		return -1;
	}

	switch (task_enum) {
	case ADSP_TASK_ATTR_DEFAULT:
		return task_attr->default_enable;
	case ADSP_TASK_ATTR_MEMDL:
		return task_attr->afe_memif_dl;
	case ADSP_TASK_ATTR_MEMUL:
		return task_attr->afe_memif_ul;
	case ADSP_TASK_ATTR_MEMREF:
		return task_attr->afe_memif_ref;
	case ADSP_TASK_ATTR_RUMTIME:
		return task_attr->runtime_enable;
	case ADSP_TASK_ATTR_SMARTPA:
		return task_attr->spk_protect_in_dsp;
	default:
		return -1;
	}
	return 0;
}

int get_featureid_by_dsp_daiid(int task_id)
{
	int ret = 0;
	struct mtk_adsp_task_attr *task_attr =
		mtk_get_adsp_task_attr(task_id);

	if (task_id > AUDIO_TASK_DAI_NUM || !task_attr) {
		pr_info("%s task_id = %d\n", __func__, task_id);
		return -1;
	}
	ret = task_attr->adsp_feature_id;
	return ret;
}

int get_afememdl_by_afe_taskid(int task_id)
{
	int ret = 0;
	struct mtk_adsp_task_attr *task_attr =
		mtk_get_adsp_task_attr(task_id);

	if (task_id > AUDIO_TASK_DAI_NUM || !task_attr) {
		pr_info("%s id = %d\n", __func__, task_id);
		return -1;
	}
	ret = task_attr->afe_memif_dl;
	return ret;
}


int get_afememul_by_afe_taskid(int task_id)
{
	int ret = 0;
	struct mtk_adsp_task_attr *task_attr =
		mtk_get_adsp_task_attr(task_id);

	if (task_id > AUDIO_TASK_DAI_NUM || !task_attr) {
		pr_info("%s id = %d\n", __func__, task_id);
		return -1;
	}
	ret = task_attr->afe_memif_ul;
	return ret;
}

int get_afememref_by_afe_taskid(int task_id)
{
	int ret = 0;
	struct mtk_adsp_task_attr *task_attr =
		mtk_get_adsp_task_attr(task_id);

	if (task_id > AUDIO_TASK_DAI_NUM || !task_attr) {
		pr_info("%s id = %d\n", __func__, task_id);
		return -1;
	}
	ret = task_attr->afe_memif_ref;
	return ret;
}

int get_aferefmem_by_afe_taskid(int task_id)
{
	int ret = 0;
	struct mtk_adsp_task_attr *task_attr =
		mtk_get_adsp_task_attr(task_id);

	if (task_id > AUDIO_TASK_DAI_NUM || !task_attr) {
		pr_info("%s id = %d\n", __func__, task_id);
		return -1;
	}
	ret = task_attr->afe_memif_ref;
	return ret;
}

int get_taskid_by_afe_daiid(int task_dai_id)
{
	int i = 0;
	struct mtk_adsp_task_attr *task_attr = NULL;

	if (task_dai_id >= MEMIF_NUM_MAX) {
		pr_warn("%s afe_dai_id = %d\n", __func__, task_dai_id);
		return -1;
	}

	for (i = 0; i < AUDIO_TASK_DAI_NUM; i++) {
		task_attr = mtk_get_adsp_task_attr(i);
		if (task_attr == NULL)
			continue;
		if ((task_attr->afe_memif_dl == task_dai_id ||
		     task_attr->afe_memif_ul == task_dai_id ||
		     task_attr->afe_memif_ref == task_dai_id) &&
		     (task_attr->default_enable == true &&
		     task_attr->runtime_enable == true))
			return i;
	}

	return -1;
}

static int checkdspbuffer(struct audio_dsp_dram *buffer)
{
	if (buffer->phy_addr == 0 || buffer->size == 0 ||
	    buffer->vir_addr == NULL)
		return -1;
	return 0;
}

int get_mtk_adsp_dram(struct audio_dsp_dram *dsp_dram, int id)
{
	if (id >= AUDIO_DSP_SHARE_MEM_NUM) {
		pr_warn("%s: id = %d\n", __func__, id);
		return -1;
	}
	memcpy((void *)dsp_dram, (void *)&dsp_dram_buffer[id],
	       sizeof(struct audio_dsp_dram));
	return 0;
}

int dump_mtk_adsp_gen_pool(void)
{
	int i = 0;

	for (i = 0; i < AUDIO_DSP_SHARE_MEM_NUM; i++) {
		pr_info("gen_pool_avail = %zu gen_pool_size = %zu\n",
			       gen_pool_avail(dsp_dram_pool[i]),
			       gen_pool_size(dsp_dram_pool[i]));
	}
	return 0;
}

struct gen_pool *mtk_get_adsp_dram_gen_pool(int id)
{
	if (id >= AUDIO_DSP_SHARE_MEM_NUM) {
		pr_warn("%s: id = %d\n", __func__, id);
		return NULL;
	}
	return dsp_dram_pool[id];
}

void dump_mtk_adsp_dram(struct audio_dsp_dram buffer)
{
	pr_debug("%s phy_addr = 0x%llx vir_addr = %p  size = %llu\n",
		       __func__, buffer.phy_addr, buffer.vir_addr, buffer.size);
}

void dump_all_adsp_dram(void)
{
	int i;

	for (i = 0; i < AUDIO_DSP_SHARE_MEM_NUM; i++)
		dump_mtk_adsp_dram(dsp_dram_buffer[i]);
}

void dump_task_attr(struct mtk_adsp_task_attr *task_attr)
{
	pr_info("%s dl[%d] ul[%d] ref[%d] feature[%d] default[%d] runtime[%d]\n",
		__func__,
		task_attr->afe_memif_dl,
		task_attr->afe_memif_ul,
		task_attr->afe_memif_ref,
		task_attr->adsp_feature_id,
		task_attr->default_enable,
		task_attr->runtime_enable
		);
}

void dump_all_task_attr(void)
{
	int i = 0;
	struct mtk_adsp_task_attr *task_attr = NULL;

	for (i = 0; i < AUDIO_TASK_DAI_NUM; i++) {
		task_attr = mtk_get_adsp_task_attr(i);
		if (!task_attr)
			continue;

		dump_task_attr(task_attr);
	}
}


/*
 * gen_pool_create - create a new special memory pool
 * @min_alloc_order: log base 2 of number of bytes each bitmap bit represents
 * @nid: node id of the node the pool structure should be allocated on, or -1
 */
int mtk_adsp_gen_pool_create(int min_alloc_order, int nid)
{
	int i;
	unsigned long va_start;
	size_t va_chunk;

	if (min_alloc_order <= 0)
		return -1;

	for (i = 0; i < AUDIO_DSP_SHARE_MEM_NUM; i++) {
		dsp_dram_pool[i] = gen_pool_create(MIN_DSP_SHIFT, -1);
		if (!dsp_dram_pool[i])
			return -ENOMEM;

		va_start = dsp_dram_buffer[i].va_addr;

		va_chunk = dsp_dram_buffer[i].size;
		if (gen_pool_add_virt(dsp_dram_pool[i], (unsigned long)va_start,
				      dsp_dram_buffer[i].phy_addr, va_chunk,
				      -1)) {
			pr_warn(
				"%s failed to add chunk va_start= 0x%lx va_chunk = %zu\n",
				__func__, va_start, va_chunk);
		}

		pr_info(
			"%s success to add chunk va_start= 0x%lx va_chunk = %zu dsp_dram_pool[i] = %p\n",
			__func__, va_start, va_chunk, dsp_dram_pool[i]);
	}
	dump_mtk_adsp_gen_pool();
	return 0;
}

void mtk_dump_sndbuffer(struct snd_dma_buffer *dma_audio_buffer)
{
	pr_debug("snd_dma_buffer addr = 0x%llx area = %p size = %zu\n",
		       dma_audio_buffer->addr, dma_audio_buffer->area,
		       dma_audio_buffer->bytes);
}

int wrap_dspdram_sndbuffer(struct snd_dma_buffer *dma_audio_buffer,
			   struct audio_dsp_dram *dsp_dram_buffer)
{
	dma_audio_buffer->addr = dsp_dram_buffer->phy_addr;
	dma_audio_buffer->area = dsp_dram_buffer->vir_addr;
	dma_audio_buffer->bytes = dsp_dram_buffer->size;
	mtk_dump_sndbuffer(dma_audio_buffer);
	return 0;
}

void init_mtk_adsp_dram_segment(void)
{
	int i;

	for (i = 0; i < AUDIO_DSP_SHARE_MEM_NUM; i++) {
		get_dsp_dram_sement_by_id(&dsp_dram_buffer[i], i);
		wrap_dspdram_sndbuffer(&dma_audio_buffer[i],
				       &dsp_dram_buffer[i]);
	}

	mtk_adsp_gen_pool_create(MIN_DSP_SHIFT, -1);
	dump_all_adsp_dram();
}

int mtk_reinit_adsp_audio_share_mem(void)
{
	struct mtk_base_dsp *dsp = NULL;

	dsp = get_dsp_base();

	if (dsp == NULL) {
		pr_info("%s dsp == NULL\n", __func__);
		return -1;
	}

	mutex_lock(&adsp_control_mutex);
	binitadsp_share_mem = false;
	mutex_unlock(&adsp_control_mutex);
	return mtk_init_adsp_audio_share_mem(dsp);
}

int mtk_adsp_init_gen_pool(struct mtk_base_dsp *dsp)
{
	int task_id, ret, i;

	/* init for dsp-audio task share memory address */
	for (task_id = 0; task_id < AUDIO_TASK_DAI_NUM; task_id++) {
		struct audio_dsp_dram *adsp_share_mem;

		adsp_share_mem = mtk_get_adsp_sharemem_block(task_id);

		pr_info("%s task_id = %d\n", __func__, task_id);

		if (adsp_share_mem == NULL) {
			pr_info("%s adsp_share_mem = NULL task_id = %d\n",
				__func__,
				task_id);
			continue;
		}
		dsp->dsp_mem[task_id].gen_pool_buffer =
			mtk_get_adsp_dram_gen_pool(task_id);

		/* allocate msg buffer with share memory */
		if (dsp->dsp_mem[task_id].gen_pool_buffer == NULL) {
			pr_info("%s gen_pool_buffer = NULL\n", __func__);
			continue;
		}

		for (i = 0; i < ADSP_TASK_SHAREMEM_NUM; i++) {
			ret = aud_genppol_allocate_sharemem_msg(
				&dsp->dsp_mem[task_id],
				mtk_get_adsp_sharemem_size(task_id,
				i),
				i);

			if (ret < 0) {
				pr_info("%s not allocate task_id[%d] i[%d]\n",
					__func__, task_id, i);
				continue;
			}
		}

		dump_audio_dsp_dram(
			&dsp->dsp_mem[task_id].msg_atod_share_buf);
		dump_audio_dsp_dram(
			&dsp->dsp_mem[task_id].msg_dtoa_share_buf);
	}
	return 0;
}

/* init dsp share memory */
int mtk_init_adsp_audio_share_mem(struct mtk_base_dsp *dsp)
{
	int task_id, ret;
	struct ipi_msg_t ipi_msg;

	if (dsp == NULL) {
		pr_info("%s dsp == NULL\n", __func__);
		return -1;
	}

	mutex_lock(&adsp_control_mutex);
	if (binitadsp_share_mem == true) {
		mutex_unlock(&adsp_control_mutex);
		return 0;
	}

	binitadsp_share_mem = true;
	adsp_register_feature(AUDIO_PLAYBACK_FEATURE_ID);

	/* init for dsp-audio task share memory address */
	for (task_id = 0; task_id < AUDIO_TASK_DAI_NUM; task_id++) {

		/* send share message to SCP side */
		ret = copy_ipi_payload(
			(void *)dsp->dsp_mem[task_id].ipi_payload_buf,
			(void *)&dsp->dsp_mem[task_id].msg_atod_share_buf,
			sizeof(struct audio_dsp_dram));

		if (ret < 0) {
			pr_info("copy_ipi_payload err\n");
			continue;
		}
		ret = audio_send_ipi_msg(
			&ipi_msg, get_dspscene_by_dspdaiid(task_id),
			AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_PAYLOAD,
			AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_MSGA2DSHAREMEM,
			sizeof(struct audio_dsp_dram), 0,
			(char *)dsp->dsp_mem[task_id].ipi_payload_buf);
		if (ret) {
			pr_info("%s(), task [%d]send ipi fail\n",
				__func__, task_id);
		}

		/* send share message to SCP side */
		ret = copy_ipi_payload(
			(void *)dsp->dsp_mem[task_id].ipi_payload_buf,
			(void *)&dsp->dsp_mem[task_id].msg_dtoa_share_buf,
			sizeof(struct audio_dsp_dram));

		if (ret < 0) {
			pr_info("copy_ipi_payload err\n");
			continue;
		}

		ret = audio_send_ipi_msg(
			&ipi_msg, get_dspscene_by_dspdaiid(task_id),
			AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_PAYLOAD,
			AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_MSGD2ASHAREMEM,
			sizeof(struct audio_dsp_dram), 0,
			(char *)dsp->dsp_mem[task_id].ipi_payload_buf);
		if (ret) {
			pr_info("%s(), task [%d]send ipi fail\n",
				__func__, task_id);
		}

	}

	adsp_deregister_feature(AUDIO_PLAYBACK_FEATURE_ID);
	mutex_unlock(&adsp_control_mutex);
	pr_debug("-%s task_id = %d\n", __func__, task_id);
	return 0;
}
