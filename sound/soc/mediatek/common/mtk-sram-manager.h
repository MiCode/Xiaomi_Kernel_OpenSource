/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-sram-manager.h  --  Mediatek afe sram manager
 *
 * Copyright (c) 2017 MediaTek Inc.
 * Author: Kai Chieh Chuang <kaichieh.chuang@mediatek.com>
 */

#ifndef _MTK_SRAM_MANAGER_H_
#define _MTK_SRAM_MANAGER_H_

#include <sound/soc.h>

enum mtk_audio_sram_mode {
	MTK_AUDIO_SRAM_NORMAL_MODE = 0,
	MTK_AUDIO_SRAM_COMPACT_MODE,
	MTK_AUDIO_SRAM_MODE_NUM,
};

struct mtk_audio_sram_block {
	bool valid;
	void *user;
	unsigned int size;
	dma_addr_t phys_addr;
	void *virt_addr;
};

struct mtk_audio_sram_ops {
	int (*set_sram_mode)(struct device *dev,
			     enum mtk_audio_sram_mode sram_mode);
};

struct mtk_audio_sram {
	struct device *dev;
	spinlock_t lock;

	dma_addr_t phys_addr;
	void *virt_addr;
	unsigned int size;
	unsigned int block_size;
	unsigned int block_num;
	struct mtk_audio_sram_block *blocks;

	enum mtk_audio_sram_mode prefer_mode;
	enum mtk_audio_sram_mode sram_mode;
	unsigned int mode_size[MTK_AUDIO_SRAM_MODE_NUM];

	struct mtk_audio_sram_ops ops;
};

int mtk_audio_sram_init(struct device *dev,
			struct mtk_audio_sram *sram,
			const struct mtk_audio_sram_ops *ops);
int mtk_audio_sram_allocate(struct mtk_audio_sram *sram,
			    dma_addr_t *phys_addr, unsigned char **virt_addr,
			    unsigned int size, void *user,
			    snd_pcm_format_t format, bool force_normal);
int mtk_audio_sram_free(struct mtk_audio_sram *sram, void *user);

unsigned int mtk_audio_sram_get_size(struct mtk_audio_sram *sram, int mode);

#endif

