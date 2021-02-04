// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Che-Jui Chang <Che-Jui.Chang@mediatek.com>
 */

#ifndef _MTK_SCP_VOW_COMMON_H_
#define _MTK_SCP_VOW_COMMON_H_

int allocate_vow_bargein_mem(struct snd_pcm_substream *substream,
			     dma_addr_t *phys_addr, unsigned char **virt_addr,
			     unsigned int size,
			     snd_pcm_format_t format,
			     struct mtk_base_afe *afe);

int get_scp_vow_memif_id(void);
#endif
