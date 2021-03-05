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
#include "audio_ipi_platform.h"

#include <mtk_spm_resource_req.h>

#ifdef CONFIG_MTK_ACAO_SUPPORT
#include "mtk_mcdi_governor_hint.h"
#endif


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

/* todo dram ?*/
int dsp_dram_request(struct device *dev)
{
	struct mtk_base_dsp *dsp = dev_get_drvdata(dev);

	mutex_lock(&adsp_mutex_request_dram);
#if 0
	if (dsp->dsp_dram_resource_counter == 0)
		spm_resource_req(SPM_RESOURCE_USER_HIFI3, SPM_RESOURCE_ALL);
#endif
	dsp->dsp_dram_resource_counter++;
	mutex_unlock(&adsp_mutex_request_dram);
	return 0;
}

int dsp_dram_release(struct device *dev)
{
	struct mtk_base_dsp *dsp = dev_get_drvdata(dev);

	mutex_lock(&adsp_mutex_request_dram);
	dsp->dsp_dram_resource_counter--;
#if 0
	if (dsp->dsp_dram_resource_counter == 0)
		spm_resource_req(SPM_RESOURCE_USER_HIFI3, SPM_RESOURCE_RELEASE);
#endif

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

int dsp_daiid_to_scp_reservedid(int task_dai_id)
{
	switch (task_dai_id) {
#ifndef FPGA_EARLY_DEVELOPMENT
	case AUDIO_TASK_OFFLOAD_ID:
	case AUDIO_TASK_VOIP_ID:
	case AUDIO_TASK_PRIMARY_ID:
	case AUDIO_TASK_DEEPBUFFER_ID:
	case AUDIO_TASK_PLAYBACK_ID:
	case AUDIO_TASK_MUSIC_ID:
	case AUDIO_TASK_CAPTURE_UL1_ID:
	case AUDIO_TASK_A2DP_ID:
	case AUDIO_TASK_DATAPROVIDER_ID:
	case AUDIO_TASK_CALL_FINAL_ID:
	case AUDIO_TASK_FAST_ID:
	case AUDIO_TASK_KTV_ID:
	case AUDIO_TASK_CAPTURE_RAW_ID:
	case AUDIO_TASK_FM_ADSP_ID:
		return ADSP_AUDIO_COMMON_MEM_ID;
#endif
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
	case ADSP_TASK_ATTR_REF_RUNTIME:
		task_attr->ref_runtime_enable = param;
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
		pr_warn("%s task_attr NULL\n", __func__);
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
	case ADSP_TASK_ATTR_REF_RUNTIME:
		return task_attr->ref_runtime_enable;
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
		if ((task_attr == NULL) || !(task_attr->default_enable & 0x1))
			continue;
		if ((task_attr->afe_memif_dl == task_dai_id ||
		     task_attr->afe_memif_ul == task_dai_id) &&
		     (task_attr->runtime_enable))
			return i;
		else if ((task_attr->afe_memif_ref == task_dai_id) &&
			 (task_attr->ref_runtime_enable))
			return i;
	}

	pr_err("%s(), afe_dai_id not support: %d", __func__, task_dai_id);

	return -1;
}

static int checkdspbuffer(struct audio_dsp_dram *buffer)
{
	if (buffer->phy_addr == 0 || buffer->size == 0 ||
	    buffer->vir_addr == NULL)
		return -1;
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
	int i, ret = 0;
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
		if ((!va_start) || (!va_chunk)) {
			ret = -1;
			break;
		}
		if (gen_pool_add_virt(dsp_dram_pool[i], (unsigned long)va_start,
				      dsp_dram_buffer[i].phy_addr, va_chunk,
				      -1)) {
			pr_warn(
				"%s failed to add chunk va_start= 0x%lx va_chunk = %zu\n",
				__func__, va_start, va_chunk);
		}

		pr_info(
			"%s success to add chunk va_start= 0x%lx va_chunk = %zu dsp_dram_pool[%d] = %p\n",
			__func__, va_start, va_chunk, i, dsp_dram_pool[i]);
	}
	dump_mtk_adsp_gen_pool();
	return ret;
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

int init_mtk_adsp_dram_segment(void)
{
	int i;

	for (i = 0; i < AUDIO_DSP_SHARE_MEM_NUM; i++) {
		get_dsp_dram_sement_by_id(&dsp_dram_buffer[i], i);
		wrap_dspdram_sndbuffer(&dma_audio_buffer[i],
				       &dsp_dram_buffer[i]);
	}

	return mtk_adsp_gen_pool_create(MIN_DSP_SHIFT, -1);
}

/* set audio share dram mpu write-through */
int set_mtk_adsp_mpu_sharedram(unsigned int dram_segment)
{
	unsigned int phy_addr, size;
	struct ipi_msg_t ipi_msg;
	int ret;

	if (dram_segment >= AUDIO_DSP_SHARE_MEM_NUM) {
		pr_info("%s dram_segment= %u\n", __func__, dram_segment);
		return -1;
	}

	adsp_register_feature(AUDIO_CONTROLLER_FEATURE_ID);

	phy_addr = (unsigned int)dsp_dram_buffer[dram_segment].phy_addr;
	size = (unsigned int)dsp_dram_buffer[dram_segment].size;

	pr_info("%s phy_addr[0x%x] size[0x%x]\n", __func__, phy_addr, size);

	ret = audio_send_ipi_msg(
		&ipi_msg, TASK_SCENE_AUD_DAEMON_A,
		AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_MSG_ONLY,
		AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_EINGBUFASHAREMEM,
		phy_addr, size, 0);

	adsp_deregister_feature(AUDIO_CONTROLLER_FEATURE_ID);

	return ret;
}

int mtk_reinit_adsp(void)
{
	int ret = 0;
	struct mtk_base_dsp *dsp = NULL;

	dsp = get_dsp_base();

	if (dsp == NULL) {
		pr_info("%s dsp == NULL\n", __func__);
		return -1;
	}

	mutex_lock(&adsp_control_mutex);
	binitadsp_share_mem = false;

	ret =  mtk_init_adsp_audio_share_mem(dsp);
	if (ret) {
		pr_info("mtk_init_adsp_audio_share_mem fail\n");
		mutex_unlock(&adsp_control_mutex);
		return -1;
	}
	binitadsp_share_mem = true;
	mutex_unlock(&adsp_control_mutex);

	return 0;
}

int mtk_adsp_init_gen_pool(struct mtk_base_dsp *dsp)
{
	int task_id, ret, i;

	/* init for dsp-audio task share memory address */
	for (task_id = 0; task_id < AUDIO_TASK_DAI_NUM; task_id++) {
		dsp->dsp_mem[task_id].gen_pool_buffer =
			mtk_get_adsp_dram_gen_pool(AUDIO_DSP_AFE_SHARE_MEM_ID);

		/* allocate msg buffer with share memory */
		if (dsp->dsp_mem[task_id].gen_pool_buffer == NULL) {
			pr_warn("%s gen_pool_buffer = NULL\n", __func__);
			continue;
		}

		for (i = 0; i < ADSP_TASK_SHAREMEM_NUM; i++) {
			ret = aud_genppol_allocate_sharemem_msg(
				&dsp->dsp_mem[task_id],
				mtk_get_adsp_sharemem_size(task_id, i),
				i);

			if (ret < 0) {
				pr_warn("%s not allocate task_id[%d] i[%d]\n",
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

static int adsp_core_mem_initall(struct mtk_base_dsp *dsp,
				 unsigned int task_scene)
{
	int ret = 0, core_id = 0, dsp_type;
	unsigned long vaddr;
	unsigned long long size;
	dma_addr_t paddr;
	struct audio_dsp_dram *pshare_dram;
	struct gen_pool *gen_pool_buffer;
	struct ipi_msg_t ipi_msg;
	unsigned char ipi_payload_buf[MAX_PAYLOAD_SIZE];

	dsp_type = audio_get_dsp_id(task_scene);

	if (dsp_type == AUDIO_OPENDSP_ID_INVALID) {
		pr_info("%s AUDIO_OPENDSP_ID_INVALID\n", __func__);
		return -1;
	}

	if (dsp == NULL) {
		pr_info("%s dsp == NULL\n", __func__);
		return -1;
	}

	/* get gen pool*/
	dsp->core_share_mem.gen_pool_buffer =
		mtk_get_adsp_dram_gen_pool(AUDIO_DSP_AFE_SHARE_MEM_ID);
	if (!dsp->core_share_mem.gen_pool_buffer) {
		pr_info("%s get gen_pool_buffer NULL\n", __func__);
		return -1;
	}

	/* allocate for adsp core share mem */
	core_id = mtk_get_core_id(dsp_type);
	if (core_id < 0) {
		pr_info("%s core_id[%d] error\n", __func__, core_id);
		return -1;
	}

	pshare_dram =
		&dsp->core_share_mem.ap_adsp_share_buf[core_id];

	gen_pool_buffer = dsp->core_share_mem.gen_pool_buffer;

	/* allocate VA with gen pool*/
	size = sizeof(struct audio_core_flag);
	if (size <= MIN_DSP_POOL_SIZE)
		size = MIN_DSP_POOL_SIZE;

	if (pshare_dram->size == 0) {
		vaddr = gen_pool_alloc(gen_pool_buffer,
			sizeof(struct audio_core_flag));
		paddr = gen_pool_virt_to_phys(gen_pool_buffer, vaddr);
		if (vaddr) {
			pshare_dram->phy_addr = paddr;
			pshare_dram->va_addr = vaddr;
			pshare_dram->vir_addr = (char *)vaddr;
			pshare_dram->size = size;
			memset(pshare_dram->vir_addr, 0,
			       pshare_dram->size);
		}
	} else {
		pr_info("%s get gen_pool_alloc size used\n", __func__);
	}

	dsp->core_share_mem.ap_adsp_core_mem[core_id] =
		(struct audio_core_flag *)pshare_dram->vir_addr;

	dump_mtk_adsp_dram(dsp->core_share_mem.ap_adsp_share_buf[core_id]);

	/* send share message information to adsp side */
	ret = copy_ipi_payload(
		(void *)ipi_payload_buf,
		(void *)pshare_dram,
		sizeof(struct audio_dsp_dram));
	pr_info("%s task_scene[%u] core_id[%d] dsp_type[%d]\n",
		__func__, task_scene, core_id, dsp_type);
	ret = audio_send_ipi_msg(
		&ipi_msg, task_scene,
		AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_PAYLOAD,
		AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_COREMEM_SET,
		sizeof(struct audio_dsp_dram), 0,
		(char *)ipi_payload_buf);

	return ret;
}

/* init for adsp/ap share mem ex:irq */
int adsp_core_mem_init(struct mtk_base_dsp *dsp)
{
	int ret;

	memset(&dsp->core_share_mem, 0, sizeof(struct mtk_ap_adsp_mem));
	ret = adsp_core_mem_initall(dsp, TASK_SCENE_AUD_DAEMON_A);
	if (ret)
		pr_info("%s fail AUD_DAEMON_A\n", __func__);
	ret = adsp_core_mem_initall(dsp, TASK_SCENE_AUD_DAEMON_B);
	if (ret)
		pr_info("%s fail AUD_DAEMON_B\n", __func__);
	return 0;
}

int adsp_task_init(int task_id, struct mtk_base_dsp *dsp)
{
	int ret = 0;
	struct ipi_msg_t ipi_msg;

	if (dsp == NULL) {
		pr_info("%s dsp == NULL\n", __func__);
		return -1;
	}

	if (task_id >= AUDIO_TASK_DAI_NUM) {
		pr_info("%s task_id = %d\n", __func__, task_id);
		return -1;
	}

	ret = audio_send_ipi_msg(&ipi_msg,
		get_dspscene_by_dspdaiid(task_id),
		AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_MSG_ONLY,
		AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_INIT,
		sizeof(struct audio_dsp_dram), 0,
		(char *)dsp->dsp_mem[task_id].ipi_payload_buf);
	if (ret) {
		pr_info("%s(), task [%d]send ipi fail\n",
			__func__, task_id);
	}

	/* send share message to adsp side */
	ret = copy_ipi_payload(
		(void *)dsp->dsp_mem[task_id].ipi_payload_buf,
		(void *)&dsp->dsp_mem[task_id].msg_atod_share_buf,
		sizeof(struct audio_dsp_dram));

	if (ret < 0)
		pr_info("copy_ipi_payload err\n");

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
	if (ret < 0)
		pr_info("copy_ipi_payload err\n");

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
	return 0;
}

/* init adsp */
int mtk_init_adsp_audio_share_mem(struct mtk_base_dsp *dsp)
{
	int task_id;

	if (dsp == NULL) {
		pr_info("%s dsp == NULL\n", __func__);
		return -1;
	}

	adsp_register_feature(AUDIO_CONTROLLER_FEATURE_ID);

	/* init for dsp-audio task share memory address */
	for (task_id = 0; task_id < AUDIO_TASK_DAI_NUM; task_id++)
		adsp_task_init(task_id, dsp);

	/* here to init ap/adsp share memory */
	adsp_core_mem_init(dsp);

	adsp_deregister_feature(AUDIO_CONTROLLER_FEATURE_ID);

	pr_debug("-%s task_id = %d\n", __func__, task_id);
	return 0;
}

