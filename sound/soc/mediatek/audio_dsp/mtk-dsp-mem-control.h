/* SPDX-License-Identifier: GPL-2.0 */
/*
 * audio_mem_control.h --  Mediatek ADSP dmemory control
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Chipeng <Chipeng.chang@mediatek.com>
 */

#ifndef MTK_DSP_MEM_CONTROL_H
#define MTK_DSP_MEM_CONTROL_H

#include <linux/genalloc.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>

#include "mtk-base-dsp.h"

struct audio_dsp_dram;
struct gen_pool;
struct mtk_base_dsp;
struct mtk_base_dsp_mem;

enum {
	ADSP_TASK_ATOD_MSG_MEM,
	ADSP_TASK_DTOA_MSG_MEM,
	ADSP_TASK_RINGBUF_MEM,
	ADSP_TASK_HWBUF_MEM,
	ADSP_TASK_SHAREMEM_NUM
};

/* first time to inint scp dram segment */
int init_mtk_adsp_dram_segment(void);

int get_mtk_adsp_dram(struct audio_dsp_dram *dsp_dram, int id);

/* dump dsp reserved dram stats */
void dump_mtk_adsp_dram(struct audio_dsp_dram buffer);
void dump_all_adsp_dram(void);
void dump_task_attr(struct mtk_adsp_task_attr *task_attr);
void dump_all_task_attr(void);

/* gen pool create */
int mtk_adsp_gen_pool_create(int min_alloc_order, int nid);
struct gen_pool *mtk_get_adsp_dram_gen_pool(int id);

void mtk_dump_sndbuffer(struct snd_dma_buffer *dma_audio_buffer);
int wrap_dspdram_sndbuffer(struct snd_dma_buffer *dma_audio_buffer,
			   struct audio_dsp_dram *dsp_dram_buffer);

int scp_reservedid_to_dsp_daiid(int id);
int dsp_daiid_to_scp_reservedid(int task_dai_id);
int get_taskid_by_afe_daiid(int task_dai_id);
int get_afememdl_by_afe_taskid(int task_dai_id);
int get_afememul_by_afe_taskid(int task_dai_id);
int get_afememref_by_afe_taskid(int task_dai_id);
int get_featureid_by_dsp_daiid(int dspid);
int set_task_attr(int task_id, int task_enum, int param);
int get_task_attr(int dspid, int task_enum);


int dump_mtk_adsp_gen_pool(void);

int mtk_adsp_genpool_allocate_sharemem_ring(struct mtk_base_dsp_mem *dsp_mem,
					    unsigned int size, int id);
int mtk_adsp_genpool_free_sharemem_ring(struct mtk_base_dsp_mem *dsp_mem,
					int id);

int mtk_adsp_genpool_allocate_sharemem_msg(struct mtk_base_dsp_mem *dsp_mem,
					  int size,
					  int id);
int mtk_init_adsp_msg_sharemem(struct audio_dsp_dram *msg_atod_share_buf,
				unsigned long vaddr, unsigned long long paddr,
				int size);

/* get struct of sharemem_block */
unsigned int mtk_get_adsp_sharemem_size(int audio_task_id,
					int task_sharemem_id);

/* init dsp share memory */
int mtk_adsp_init_gen_pool(struct mtk_base_dsp *dsp);
int mtk_init_adsp_audio_share_mem(struct mtk_base_dsp *dsp);
int mtk_reinit_adsp_audio_share_mem(void);


int dsp_dram_request(struct device *dev);
int dsp_dram_release(struct device *dev);

#endif /* end of MTK_DSP_MEM_CONTROL_H */
