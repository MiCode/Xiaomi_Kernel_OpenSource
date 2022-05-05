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

/* page size */
#define MIN_DSP_SHIFT (8)
#define MIN_DSP_POOL_SIZE (1 << MIN_DSP_SHIFT)

struct audio_dsp_dram;
struct gen_pool;
struct mtk_base_dsp;
struct mtk_base_dsp_mem;

enum {
	ADSP_TASK_ATOD_MSG_MEM,
	ADSP_TASK_DTOA_MSG_MEM,
	ADSP_TASK_SHAREMEM_NUM
};

/* first time to inint scp dram segment */
int init_mtk_adsp_dram_segment(void);

/* set audio share dram mpu write-through */
int set_mtk_adsp_mpu_sharedram(unsigned int dram_segment);

/* dump dsp reserved dram stats */
void dump_all_task_attr(void);

/* gen pool create */
int mtk_adsp_gen_pool_create(struct gen_pool *pool, struct audio_dsp_dram *dram);
struct gen_pool *mtk_get_adsp_dram_gen_pool(unsigned int id);

int get_taskid_by_afe_daiid(int afe_dai_id);
int get_afememdl_by_afe_taskid(int task_dai_id);
int get_afememul_by_afe_taskid(int task_dai_id);
int get_afememref_by_afe_taskid(int task_dai_id);
int get_featureid_by_dsp_daiid(int dspid);
int set_task_attr(int task_id, int task_enum, int param);
int get_task_attr(int dspid, int task_enum);


bool is_adsp_genpool_addr_valid(struct snd_pcm_substream *substream);
int mtk_adsp_genpool_allocate_sharemem_ring(struct mtk_base_dsp_mem *dsp_mem,
					    unsigned int size, int id);
int mtk_adsp_genpool_free_sharemem_ring(struct mtk_base_dsp_mem *dsp_mem,
					int id);

int mtk_adsp_genpool_allocate_sharemem_msg(struct mtk_base_dsp_mem *dsp_mem,
					  int size,
					  int id);
/* get struct of sharemem_block */
unsigned int mtk_get_adsp_sharemem_size(int audio_task_id,
					int task_sharemem_id);

/* init dsp share memory */
int mtk_adsp_init_gen_pool(struct mtk_base_dsp *dsp);
int mtk_init_adsp_audio_share_mem(struct mtk_base_dsp *dsp);

/* init task with certain task id */
int adsp_task_init(int task_id, struct mtk_base_dsp *dsp);
int mtk_reinit_adsp(void);

int dsp_dram_request(struct device *dev);
int dsp_dram_release(struct device *dev);

#endif /* end of MTK_DSP_MEM_CONTROL_H */
