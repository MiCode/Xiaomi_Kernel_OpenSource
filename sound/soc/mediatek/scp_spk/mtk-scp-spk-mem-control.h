/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef _MTK_SCP_SPK_MEM_CONTROL_H
#define _MTK_SCP_SPK_MEM_CONTROL_H

#include <linux/types.h>
#include <sound/soc.h>


struct snd_pcm_substream;
struct snd_soc_dai;
struct mtk_base_afe;

#define SPK_BUF_OFFSET (0xC000)

int mtk_scp_spk_reserved_dram_init(void);
int mtk_scp_spk_allocate_tcm_iv_buf(void);
void mtk_scp_spk_allocate_platform_buf(unsigned int size,
				       dma_addr_t *phys_addr,
				       unsigned char **virt_addr);
int mtk_scp_spk_free_mem(struct snd_pcm_substream *substream,
			 struct mtk_base_afe *afe);
int mtk_scp_spk_allocate_mem(struct snd_pcm_substream *substream,
			     dma_addr_t *phys_addr, unsigned char **virt_addr,
			     unsigned int size,
			     snd_pcm_format_t format,
			     struct mtk_base_afe *afe);

#endif
