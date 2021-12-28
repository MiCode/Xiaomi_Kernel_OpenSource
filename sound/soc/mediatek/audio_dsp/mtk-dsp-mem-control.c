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

#include <adsp_helper.h>

/* platform related header file*/
#include "mtk-dsp-mem-control.h"
#include "mtk-base-dsp.h"
#include "mtk-dsp-common.h"
#include "audio_ipi_platform.h"
#include "mtk-base-afe.h"


static DEFINE_MUTEX(adsp_control_mutex);
static DEFINE_MUTEX(adsp_mutex_request_dram);
static bool binitadsp_share_mem;
static struct audio_dsp_dram dsp_dram_buffer[AUDIO_DSP_SHARE_MEM_NUM];
static struct gen_pool *dsp_dram_pool[AUDIO_DSP_SHARE_MEM_NUM];
static struct snd_dma_buffer dma_audio_buffer[AUDIO_DSP_SHARE_MEM_NUM];

/* function */
static int get_dsp_dram_sement_by_id(struct audio_dsp_dram *buffer, int id);
static int checkdspbuffer(struct audio_dsp_dram *buffer);

/* task share mem block */
static struct audio_dsp_dram
	adsp_sharemem_primary_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_playback_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};
static struct audio_dsp_dram
	adsp_sharemem_offload_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, // 1024 bytes
			.phy_addr = 0,
		},
		{
			.size = 0x400, // 1024 bytes
			.phy_addr = 0,
		},
};
static struct audio_dsp_dram
	adsp_sharemem_deepbuffer_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_voip_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_capture_ul1_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_a2dp_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_bledl_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_dataprovider_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_call_final_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_fast_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_ktv_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_capture_raw_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_fm_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct audio_dsp_dram
	adsp_sharemem_bleul_mblock[ADSP_TASK_SHAREMEM_NUM] = {
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
		{
			.size = 0x400, /* 1024 bytes */
			.phy_addr = 0,
		},
};

static struct mtk_adsp_task_attr adsp_task_attr[AUDIO_TASK_DAI_NUM] = {
	[AUDIO_TASK_VOIP_ID] = {true, -1, -1, -1,
				VOIP_FEATURE_ID, false},
	[AUDIO_TASK_PRIMARY_ID] = {true, -1, -1, -1,
				   PRIMARY_FEATURE_ID, false},
	[AUDIO_TASK_OFFLOAD_ID] = {true, -1, -1, -1,
				   OFFLOAD_FEATURE_ID, false},
	[AUDIO_TASK_DEEPBUFFER_ID] = {false, -1, -1, -1,
				      DEEPBUF_FEATURE_ID, false},
	[AUDIO_TASK_PLAYBACK_ID] = {false, -1, -1, -1,
				    AUDIO_PLAYBACK_FEATURE_ID, false},
	[AUDIO_TASK_MUSIC_ID] = {false, -1, -1, -1,
				 AUDIO_MUSIC_FEATURE_ID, false},
	[AUDIO_TASK_CAPTURE_UL1_ID] = {true, -1, -1, -1,
				       CAPTURE_UL1_FEATURE_ID, false},
	[AUDIO_TASK_A2DP_ID] = {true, -1, -1, -1,
				A2DP_PLAYBACK_FEATURE_ID, false},
	[AUDIO_TASK_BLEDL_ID] = {true, -1, -1, -1,
				BLEDL_FEATURE_ID, false},
	[AUDIO_TASK_BLEUL_ID] = {true, -1, -1, -1,
				BLEUL_FEATURE_ID, false},
	[AUDIO_TASK_DATAPROVIDER_ID] = {true, -1, -1, -1,
				       AUDIO_DATAPROVIDER_FEATURE_ID, false},
	[AUDIO_TASK_CALL_FINAL_ID] = {true, -1, -1, -1,
				      CALL_FINAL_FEATURE_ID, false},
	[AUDIO_TASK_FAST_ID] = {false, -1, -1, -1, FAST_FEATURE_ID, false},
	[AUDIO_TASK_KTV_ID] = {true, -1, -1, -1, KTV_FEATURE_ID, false},
	[AUDIO_TASK_CAPTURE_RAW_ID] = {false, -1, -1, -1,
				       CAPTURE_RAW_FEATURE_ID, false},
	[AUDIO_TASK_FM_ADSP_ID] = {true, -1, -1, -1,
				   FM_ADSP_FEATURE_ID, false},
};

static struct audio_dsp_dram *mtk_get_adsp_sharemem_block(int audio_task_id)
{
	if (audio_task_id > AUDIO_TASK_DAI_NUM)
		pr_info("%s err\n", __func__);

	switch (audio_task_id) {
	case AUDIO_TASK_VOIP_ID:
		return adsp_sharemem_voip_mblock;
	case AUDIO_TASK_PRIMARY_ID:
		return adsp_sharemem_primary_mblock;
	case AUDIO_TASK_OFFLOAD_ID:
		return adsp_sharemem_offload_mblock;
	case AUDIO_TASK_DEEPBUFFER_ID:
		return adsp_sharemem_deepbuffer_mblock;
	case AUDIO_TASK_PLAYBACK_ID:
		return adsp_sharemem_playback_mblock;
	case AUDIO_TASK_CAPTURE_UL1_ID:
		return adsp_sharemem_capture_ul1_mblock;
	case AUDIO_TASK_A2DP_ID:
		return adsp_sharemem_a2dp_mblock;
	case AUDIO_TASK_BLEDL_ID:
		return adsp_sharemem_bledl_mblock;
	case AUDIO_TASK_DATAPROVIDER_ID:
		return adsp_sharemem_dataprovider_mblock;
	case AUDIO_TASK_CALL_FINAL_ID:
		return adsp_sharemem_call_final_mblock;
	case AUDIO_TASK_FAST_ID:
		return adsp_sharemem_fast_mblock;
	case AUDIO_TASK_KTV_ID:
		return adsp_sharemem_ktv_mblock;
	case AUDIO_TASK_CAPTURE_RAW_ID:
		return adsp_sharemem_capture_raw_mblock;
	case AUDIO_TASK_FM_ADSP_ID:
		return adsp_sharemem_fm_mblock;
	case AUDIO_TASK_BLEUL_ID:
		return adsp_sharemem_bleul_mblock;
	default:
		pr_info("%s err audio_task_id = %d\n", __func__, audio_task_id);
	}

	return NULL;
}

static int get_dsp_dram_sement_by_id(struct audio_dsp_dram *buffer, int id)
{
	int ret = 0;

	buffer->phy_addr =
		adsp_get_reserve_mem_phys(ADSP_AUDIO_COMMON_MEM_ID);
	buffer->va_addr = (unsigned long long)
		adsp_get_reserve_mem_virt(ADSP_AUDIO_COMMON_MEM_ID);
	buffer->vir_addr = adsp_get_reserve_mem_virt(ADSP_AUDIO_COMMON_MEM_ID);
	buffer->size = adsp_get_reserve_mem_size(ADSP_AUDIO_COMMON_MEM_ID);

	ret = checkdspbuffer(buffer);
	if (ret)
		pr_warn("get buffer id = %dis not get\n", id);

	return ret;
}

/* todo dram ?*/
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

bool is_adsp_genpool_addr_valid(struct snd_pcm_substream *substream)
{
	struct gen_pool *gen_pool_dsp =
		mtk_get_adsp_dram_gen_pool(AUDIO_DSP_AFE_SHARE_MEM_ID);

	return gen_pool_has_addr(gen_pool_dsp,
				 (unsigned long)substream->runtime->dma_area,
				 substream->runtime->dma_bytes);
}
EXPORT_SYMBOL(is_adsp_genpool_addr_valid);

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
EXPORT_SYMBOL(mtk_adsp_genpool_allocate_sharemem_ring);

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
EXPORT_SYMBOL(mtk_adsp_genpool_free_sharemem_ring);

int mtk_adsp_allocate_mem(struct snd_pcm_substream *substream,
			  unsigned int size)
{
	int ret = 0;

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

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_adsp_allocate_mem);

int mtk_adsp_free_mem(struct snd_pcm_substream *substream)
{
	int ret = 0;

	ret = mtk_adsp_genpool_free_memory(
		       &substream->runtime->dma_area,
		       &substream->runtime->dma_bytes,
		       AUDIO_DSP_AFE_SHARE_MEM_ID);

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_adsp_free_mem);

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
		pr_warn("%s() gen_pool_dsp == NULL\n", __func__);
		return -1;
	}

	if (!gen_pool_has_addr(gen_pool_dsp, (unsigned long)*vaddr, *size)) {
		pr_warn("%s() vaddr is not in genpool\n", __func__);
		return -1;
	}

	/* allocate VA with gen pool */
	if (*vaddr) {
		gen_pool_free(gen_pool_dsp, (unsigned long)*vaddr, *size);
		*vaddr = NULL;
		*size = 0;
	}

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

static struct mtk_adsp_task_attr *mtk_get_adsp_task_attr(int adsp_id)
{
	if (adsp_id >= AUDIO_TASK_DAI_NUM || adsp_id < 0) {
		pr_info("%s(), adsp_id err: %d, return\n",
			__func__, adsp_id);
		return NULL;
	}

	return &adsp_task_attr[adsp_id];
}

int set_task_attr(int dsp_id, int task_enum, int param)
{

	struct mtk_adsp_task_attr *task_attr;

	if (dsp_id >= AUDIO_TASK_DAI_NUM) {
		pr_warn("%s() dspid over max: %d\n", __func__, dsp_id);
		return -1;
	}

	if (task_enum >= ADSP_TASK_ATTR_NUM) {
		pr_warn("%s() task_enum over max: %d\n", __func__, task_enum);
		return -1;
	}

	task_attr = mtk_get_adsp_task_attr(dsp_id);
	if (!task_attr) {
		pr_warn("%s() task_attr NULL dsp_id %d, return\n",
			__func__, dsp_id);
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
	case ADSP_TASK_ATTR_RUNTIME:
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
	struct mtk_adsp_task_attr *task_attr;

	if (dsp_id >= AUDIO_TASK_DAI_NUM) {
		pr_warn("%s() dsp_id over max %d, return\n", __func__, dsp_id);
		return -1;
	}

	if (task_enum >= ADSP_TASK_ATTR_NUM) {
		pr_warn("%s() task_enum %d over max, return\n",
			__func__, task_enum);
		return -1;
	}

	task_attr = mtk_get_adsp_task_attr(dsp_id);
	if (!task_attr) {
		pr_warn("%s() task_attr NULL, return\n", __func__);
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
	case ADSP_TASK_ATTR_RUNTIME:
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
EXPORT_SYMBOL_GPL(get_task_attr);

int get_featureid_by_dsp_daiid(int task_id)
{
	int ret = 0;
	struct mtk_adsp_task_attr *task_attr =
		mtk_get_adsp_task_attr(task_id);

	if (task_id > AUDIO_TASK_DAI_NUM || !task_attr) {
		pr_info("%s() task_id %d error or task_attr NULL, return\n",
			__func__, task_id);
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
		pr_info("%s() task_id %d error or task_attr NULL, return\n",
			__func__, task_id);
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
		pr_info("%s() task_id %d error or task_attr NULL, return\n",
			__func__, task_id);
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
		pr_info("%s() task_id %d error or task_attr NULL, return\n",
			__func__, task_id);
		return -1;
	}
	ret = task_attr->afe_memif_ref;
	return ret;
}

int get_taskid_by_afe_daiid(int afe_dai_id)
{
	int i = 0;
	struct mtk_adsp_task_attr *task_attr = NULL;
	struct mtk_base_afe *afe = get_afe_base();

	if (afe_dai_id >= afe->memif_size) {
		pr_warn("%s() afe_dai_id over max %d, return\n",
			__func__, afe_dai_id);
		return -1;
	}

	for (i = 0; i < AUDIO_TASK_DAI_NUM; i++) {
		task_attr = mtk_get_adsp_task_attr(i);
		if ((task_attr == NULL) || !(task_attr->default_enable & 0x1))
			continue;
		if ((task_attr->afe_memif_dl == afe_dai_id ||
		     task_attr->afe_memif_ul == afe_dai_id) &&
		     (task_attr->runtime_enable))
			return i;
		else if ((task_attr->afe_memif_ref == afe_dai_id) &&
			 (task_attr->ref_runtime_enable))
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
EXPORT_SYMBOL(mtk_get_adsp_dram_gen_pool);

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
	struct ipi_msg_t ipi_msg;
	unsigned long long payload_buf[2];
	int ret;

	if (dram_segment >= AUDIO_DSP_SHARE_MEM_NUM) {
		pr_info("%s dram_segment= %u\n", __func__, dram_segment);
		return -1;
	}

	adsp_register_feature(AUDIO_CONTROLLER_FEATURE_ID);

	pr_info("%s phy_addr[0x%llx] size[0x%llx]\n", __func__,
		dsp_dram_buffer[dram_segment].phy_addr,
		dsp_dram_buffer[dram_segment].size);
	payload_buf[0] = dsp_dram_buffer[dram_segment].phy_addr;
	payload_buf[1] = dsp_dram_buffer[dram_segment].size;

	ret = audio_send_ipi_msg(
		&ipi_msg, TASK_SCENE_AUD_DAEMON_A,
		AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_PAYLOAD,
		AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_EINGBUFASHAREMEM,
		sizeof(payload_buf), 0, (void *)payload_buf);

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

static int adsp_core_mem_initall(struct mtk_base_dsp *dsp, unsigned int core_id)
{
	int ret = 0;
	unsigned int task_scene;
	unsigned long vaddr;
	unsigned long long size;
	dma_addr_t paddr;
	struct audio_dsp_dram *pshare_dram;
	struct gen_pool *gen_pool_buffer;
	struct ipi_msg_t ipi_msg;
	unsigned char ipi_payload_buf[MAX_PAYLOAD_SIZE];

	if (dsp == NULL) {
		pr_info("%s dsp == NULL\n", __func__);
		return -1;
	}

	task_scene = (core_id == ADSP_A_ID) ?
		     TASK_SCENE_AUD_DAEMON_A : TASK_SCENE_AUD_DAEMON_B;

	/* get gen pool*/
	dsp->core_share_mem.gen_pool_buffer =
		mtk_get_adsp_dram_gen_pool(AUDIO_DSP_AFE_SHARE_MEM_ID);
	if (!dsp->core_share_mem.gen_pool_buffer) {
		pr_info("%s get gen_pool_buffer NULL\n", __func__);
		return -1;
	}

	/* allocate for adsp core share mem */
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
	pr_info("%s task_scene[%u] core_id[%d]\n",
		__func__, task_scene, core_id);
	ret = audio_send_ipi_msg(
		&ipi_msg, task_scene,
		AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_PAYLOAD,
		AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_COREMEM_SET,
		sizeof(struct audio_dsp_dram), 0,
		(char *)ipi_payload_buf);

	return ret;
}

/* init for adsp/ap share mem ex:irq */
static int adsp_core_mem_init(struct mtk_base_dsp *dsp)
{
	int ret, core_id;

	memset(&dsp->core_share_mem, 0, sizeof(struct mtk_ap_adsp_mem));

	for (core_id = 0; core_id < get_adsp_core_total(); ++core_id) {
		ret = adsp_core_mem_initall(dsp, core_id);
		if (ret)
			pr_info("%s fail, core id: %d\n", __func__, core_id);
	}
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

