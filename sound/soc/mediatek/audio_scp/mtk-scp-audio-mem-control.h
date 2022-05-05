/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-scp-audio-mem-control.h --  Mediatek scp audio dmemory control
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Zhixiong <Zhixiong.Wang@mediatek.com>
 */

#ifndef MTK_SCP_AUDIO_MEM_CONTROL_H
#define MTK_SCP_AUDIO_MEM_CONTROL_H
#include "mtk-scp-audio-base.h"

#define SCP_AUD_A2D_D2A_MEM_SIZE 0x1000

int mtk_scp_audio_init_mem(void);
int mtk_scp_get_memif_buf_size(void);
bool is_scp_genpool_addr_valid(struct snd_pcm_substream *substream);
int mtk_scp_allocate_mem(struct snd_pcm_substream *substream,
			  unsigned int size);
int mtk_scp_free_mem(struct snd_pcm_substream *substream);
int scp_audio_dram_request(struct device *dev);
int scp_audio_dram_release(struct device *dev);
int scp_audio_allocate_sharemem_ring(struct scp_aud_task_base *taskbase,
				     unsigned int size,
				     struct gen_pool *genpool);
int scp_audio_free_sharemem_ring(struct scp_aud_task_base *taskbase,
				 struct gen_pool *genpool);
#endif /* end of MTK_DSP_MEM_CONTROL_H */
