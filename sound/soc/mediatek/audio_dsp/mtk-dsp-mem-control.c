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

bool is_adsp_genpool_addr_valid(struct snd_pcm_substream *substream)
{
	struct gen_pool *pool = mtk_get_adsp_dram_gen_pool(AUDIO_DSP_AFE_SHARE_MEM_ID);

	if (pool == NULL) {
		pr_warn("%s pool == NULL\n", __func__);
		return false;
	}

	return gen_pool_has_addr(pool, (unsigned long)substream->runtime->dma_area,
				 substream->runtime->dma_bytes);
}
EXPORT_SYMBOL(is_adsp_genpool_addr_valid);

int mtk_adsp_genpool_allocate_sharemem_ring(struct mtk_base_dsp_mem *dsp_mem,
					    unsigned int size, int id)
{
	struct audio_dsp_dram *share_buf;
	struct gen_pool *pool;
	unsigned long vaddr;
	dma_addr_t paddr;

	pool = dsp_mem->gen_pool_buffer;
	if (pool == NULL) {
		pr_warn("%s pool == NULL\n", __func__);
		return -1;
	}

	vaddr = gen_pool_alloc(pool, size);
	if (!vaddr)
		return -ENOMEM;

	paddr = gen_pool_virt_to_phys(pool, vaddr);

	share_buf = &dsp_mem->dsp_ring_share_buf;
	share_buf->phy_addr = paddr;
	share_buf->va_addr = vaddr;
	share_buf->vir_addr = (unsigned char *)vaddr;
	share_buf->size = size;

	init_ring_buf(&dsp_mem->ring_buf, share_buf->vir_addr, size);
	init_ring_buf_bridge(&dsp_mem->adsp_buf.aud_buffer.buf_bridge, paddr, size);

	pr_debug("%s, id:%d allocate %u bytes, pool use/total:%zu,%zu\n",
		 __func__, id, size,
		 gen_pool_size(pool) - gen_pool_avail(pool),
		 gen_pool_size(pool));

	return 0;
}
EXPORT_SYMBOL(mtk_adsp_genpool_allocate_sharemem_ring);

int mtk_adsp_genpool_free_sharemem_ring(struct mtk_base_dsp_mem *dsp_mem, int id)
{
	struct audio_dsp_dram *share_buf;
	struct gen_pool *pool;

	pool = dsp_mem->gen_pool_buffer;
	if (pool == NULL) {
		pr_warn("%s pool == NULL\n", __func__);
		return -1;
	}

	share_buf = &dsp_mem->dsp_ring_share_buf;
	if (share_buf->vir_addr) {
		gen_pool_free(pool, share_buf->va_addr, share_buf->size);
		memset(share_buf, 0, sizeof(*share_buf));

		RingBuf_Bridge_Clear(&dsp_mem->adsp_buf.aud_buffer.buf_bridge);
		RingBuf_Clear(&dsp_mem->ring_buf);
	}

	return 0;
}
EXPORT_SYMBOL(mtk_adsp_genpool_free_sharemem_ring);

int mtk_adsp_allocate_mem(struct snd_pcm_substream *substream, unsigned int size)
{
	int ret = 0;
	struct gen_pool *pool = mtk_get_adsp_dram_gen_pool(AUDIO_DSP_AFE_SHARE_MEM_ID);
	struct snd_dma_buffer *dmab = NULL;

	if (!pool) {
		pr_warn("%s pool == NULL\n", __func__);
		return -1;
	}

	dmab = kzalloc(sizeof(*dmab), GFP_KERNEL);
	if (!dmab)
		return -ENOMEM;

	dmab->dev = substream->dma_buffer.dev;
	dmab->bytes = size;

	dmab->area = (unsigned char *)gen_pool_alloc(pool, dmab->bytes);
	if (!dmab->area) {
		kfree(dmab);
		return -ENOMEM;
	}

	dmab->addr = gen_pool_virt_to_phys(pool, (unsigned long)dmab->area);

	snd_pcm_set_runtime_buffer(substream, dmab);

	pr_debug("%s, allocate %u bytes, pool use/total:%zu,%zu\n",
		 __func__, size,
		 gen_pool_size(pool) - gen_pool_avail(pool),
		 gen_pool_size(pool));

	return ret;
}
EXPORT_SYMBOL_GPL(mtk_adsp_allocate_mem);

int mtk_adsp_free_mem(struct snd_pcm_substream *substream)
{
	struct gen_pool *pool = mtk_get_adsp_dram_gen_pool(AUDIO_DSP_AFE_SHARE_MEM_ID);
	struct snd_pcm_runtime *runtime;

	if (!substream || !substream->runtime)
		return -EINVAL;

	if (!pool) {
		pr_warn("%s pool == NULL\n", __func__);
		return -1;
	}

	runtime = substream->runtime;
	if (runtime->dma_area == NULL)
		return 0;

	if (runtime->dma_buffer_p) {
		gen_pool_free(pool, (unsigned long)runtime->dma_area, runtime->dma_bytes);
		kfree(runtime->dma_buffer_p);
	}

	snd_pcm_set_runtime_buffer(substream, NULL);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_adsp_free_mem);

int aud_genppol_allocate_sharemem_msg(struct mtk_base_dsp_mem *dsp_mem, int size, int id)
{
	unsigned long vaddr;
	struct gen_pool *pool;
	struct audio_dsp_dram *pshare_dram = NULL;

	if (!dsp_mem)
		return -EINVAL;

	/* get gen pool pointer */
	pool = dsp_mem->gen_pool_buffer;

	if (size == 0 || id >= ADSP_TASK_SHAREMEM_NUM) {
		pr_info("%s size[%d] id[%d]\n", __func__, size, id);
		return -EINVAL;
	}

	vaddr = gen_pool_alloc(pool, size);
	if (!vaddr)
		return -ENOMEM;

	switch (id) {
	case ADSP_TASK_ATOD_MSG_MEM:
		pshare_dram = &dsp_mem->msg_atod_share_buf;
		break;
	case ADSP_TASK_DTOA_MSG_MEM:
		pshare_dram = &dsp_mem->msg_dtoa_share_buf;
		break;
	default:
		pr_info("%s id[%d] not support\n", __func__, id);
		return -EINVAL;
	}

	pshare_dram->phy_addr = gen_pool_virt_to_phys(pool, vaddr);
	pshare_dram->va_addr = vaddr;
	pshare_dram->vir_addr = (unsigned char *)vaddr;
	pshare_dram->size = size;

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

	if (!afe) {
		pr_warn("%s() afe == NULL\n", __func__);
		return -1;
	}

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

struct gen_pool *mtk_get_adsp_dram_gen_pool(unsigned int id)
{
	if (id >= AUDIO_DSP_SHARE_MEM_NUM) {
		pr_warn("%s: id = %d\n", __func__, id);
		return NULL;
	}
	return dsp_dram_pool[id];
}
EXPORT_SYMBOL(mtk_get_adsp_dram_gen_pool);

void dump_all_task_attr(void)
{
	int i = 0;
	struct mtk_adsp_task_attr *task_attr = NULL;

	for (i = 0; i < AUDIO_TASK_DAI_NUM; i++) {
		task_attr = mtk_get_adsp_task_attr(i);
		if (!task_attr)
			continue;

		pr_info("%s dl[%d] ul[%d] ref[%d] feature[%d] default[%d] runtime[%d]\n",
			__func__,
			task_attr->afe_memif_dl,
			task_attr->afe_memif_ul,
			task_attr->afe_memif_ref,
			task_attr->adsp_feature_id,
			task_attr->default_enable,
			task_attr->runtime_enable);
	}
}

int init_mtk_adsp_dram_segment(void)
{
	int i, ret;
	int dsp_mem_id[AUDIO_DSP_SHARE_MEM_NUM] = {ADSP_AUDIO_COMMON_MEM_ID};
	struct audio_dsp_dram *dram;

	for (i = 0; i < AUDIO_DSP_SHARE_MEM_NUM; i++) {
		dram = &dsp_dram_buffer[i];

		dram->phy_addr = adsp_get_reserve_mem_phys(dsp_mem_id[i]);
		dram->vir_addr = adsp_get_reserve_mem_virt(dsp_mem_id[i]);
		dram->size = adsp_get_reserve_mem_size(dsp_mem_id[i]);
		dram->va_addr = (u64)dram->vir_addr;

		if (!dram->phy_addr || !dram->size || !dram->vir_addr)
			return -ENOMEM;

		dsp_dram_pool[i] = gen_pool_create(MIN_DSP_SHIFT, -1);
		if (!dsp_dram_pool[i])
			return -EFAULT;

		gen_pool_set_algo(dsp_dram_pool[i], gen_pool_best_fit, NULL);

		ret = gen_pool_add_virt(dsp_dram_pool[i],
					dram->va_addr, dram->phy_addr, dram->size, -1);

		pr_info("%s ret(%d) add chunk va/sz=(%p, %zu), pool total(%zu)\n",
			__func__, ret, dram->vir_addr, dram->size,
			gen_pool_size(dsp_dram_pool[i]));

		if (ret)
			break;
	}
	return ret;
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

	pr_info("%s vir_addr[%p] size[0x%llx]\n", __func__,
		dsp_dram_buffer[dram_segment].vir_addr,
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
	struct gen_pool *pool = mtk_get_adsp_dram_gen_pool(AUDIO_DSP_AFE_SHARE_MEM_ID);
	struct audio_dsp_dram *sharemem;

	if (!pool) {
		pr_warn("%s gen_pool_buffer = NULL\n", __func__);
		return -1;
	}

	/* init for dsp-audio task share memory address */
	for (task_id = 0; task_id < AUDIO_TASK_DAI_NUM; task_id++) {
		dsp->dsp_mem[task_id].gen_pool_buffer = pool;

		sharemem = mtk_get_adsp_sharemem_block(task_id);
		if (!sharemem)
			continue;

		for (i = 0; i < ADSP_TASK_SHAREMEM_NUM; i++) {
			ret = aud_genppol_allocate_sharemem_msg(
				&dsp->dsp_mem[task_id], sharemem[i].size, i);

			if (ret < 0) {
				pr_warn("%s not allocate task_id[%d] i[%d]\n",
					__func__, task_id, i);
				continue;
			}
		}

		dump_audio_dsp_dram(&dsp->dsp_mem[task_id].msg_atod_share_buf);
		dump_audio_dsp_dram(&dsp->dsp_mem[task_id].msg_dtoa_share_buf);
	}
	return 0;
}

static int adsp_core_mem_initall(struct mtk_base_dsp *dsp, unsigned int core_id)
{
	unsigned int task_scene;
	unsigned long long size = sizeof(struct audio_core_flag);
	unsigned long vaddr;
	struct mtk_ap_adsp_mem *core;
	struct audio_dsp_dram *pshare_dram;
	struct gen_pool *pool;
	struct ipi_msg_t ipi_msg;

	if (dsp == NULL) {
		pr_info("%s dsp == NULL\n", __func__);
		return -1;
	}

	pool = mtk_get_adsp_dram_gen_pool(AUDIO_DSP_AFE_SHARE_MEM_ID);
	if (!pool) {
		pr_info("%s get pool NULL\n", __func__);
		return -1;
	}

	core = &dsp->core_share_mem;
	pshare_dram = &core->ap_adsp_share_buf[core_id];

	if (pshare_dram->size == 0) {
		vaddr = gen_pool_alloc(pool, size);
		if (!vaddr)
			return -ENOMEM;

		pshare_dram->phy_addr = gen_pool_virt_to_phys(pool, vaddr);
		pshare_dram->va_addr = vaddr;
		pshare_dram->vir_addr = (unsigned char *)vaddr;
		pshare_dram->size = size;

		pr_debug("%s vir_addr = %p  size = %llu\n", __func__,
			 pshare_dram->vir_addr,
			 pshare_dram->size);
	} else {
		memset(pshare_dram->vir_addr, 0, pshare_dram->size);
	}

	core->gen_pool_buffer = pool;
	core->ap_adsp_core_mem[core_id] = (struct audio_core_flag *)pshare_dram->vir_addr;

	task_scene = (core_id == ADSP_A_ID) ? TASK_SCENE_AUD_DAEMON_A
					    : TASK_SCENE_AUD_DAEMON_B;

	return audio_send_ipi_msg(&ipi_msg, task_scene,
				  AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_PAYLOAD,
				  AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_COREMEM_SET,
				  sizeof(*pshare_dram), 0, pshare_dram);
}

/* init for adsp/ap share mem ex:irq */
static int adsp_core_mem_init(struct mtk_base_dsp *dsp)
{
	int ret, core_id;

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
	int task_scene = 0;
	struct ipi_msg_t ipi_msg;

	if (!dsp || task_id >= AUDIO_TASK_DAI_NUM || task_id < 0)
		return -EINVAL;

	task_scene = get_dspscene_by_dspdaiid(task_id);

	/* send share message to adsp side */
	ret = audio_send_ipi_msg(&ipi_msg, task_scene,
				 AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_MSG_ONLY,
				 AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_INIT,
				 0, 0, NULL);
	if (ret)
		pr_info("%s(), task[%d] send TASK_INIT fail\n", __func__, task_id);

	ret = audio_send_ipi_msg(&ipi_msg, task_scene,
				 AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_PAYLOAD,
				 AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_MSGA2DSHAREMEM,
				 sizeof(struct audio_dsp_dram), 0,
				 &dsp->dsp_mem[task_id].msg_atod_share_buf);
	if (ret)
		pr_info("%s(), task[%d] send A2DSHAREMEM fail\n", __func__, task_id);


	ret = audio_send_ipi_msg(&ipi_msg, task_scene,
				 AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_PAYLOAD,
				 AUDIO_IPI_MSG_BYPASS_ACK, AUDIO_DSP_TASK_MSGD2ASHAREMEM,
				 sizeof(struct audio_dsp_dram), 0,
				 &dsp->dsp_mem[task_id].msg_dtoa_share_buf);
	if (ret)
		pr_info("%s(), task[%d] send D2ASHAREMEM fail\n", __func__, task_id);

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

