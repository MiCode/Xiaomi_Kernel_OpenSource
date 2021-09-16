/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-scp-vow.h  --
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

#ifndef _MTK_SCP_VOW_H_
#define _MTK_SCP_VOW_H_

int mtk_scp_vow_barge_in_allocate_mem(struct snd_pcm_substream *substream,
			     dma_addr_t *phys_addr, unsigned char **virt_addr,
			     unsigned int size);

#endif
