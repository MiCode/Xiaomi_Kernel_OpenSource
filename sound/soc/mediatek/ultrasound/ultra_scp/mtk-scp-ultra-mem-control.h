/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef _MTK_SCP_ULTRA_MEM_CONTROL_H
#define _MTK_SCP_ULTRA_MEM_CONTROL_H

#include <linux/types.h>
#include <sound/soc.h>


struct snd_pcm_substream;
struct snd_soc_dai;
struct mtk_base_afe;

int mtk_scp_ultra_reserved_dram_init(void);
int mtk_scp_ultra_free_mem(struct snd_pcm_substream *substream,
			   struct mtk_base_afe *afe);

#endif
