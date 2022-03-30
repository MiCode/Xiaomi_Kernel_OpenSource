/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 MediaTek Inc.
 */

#ifndef _MTK_SCP_ULTRA_H_
#define _MTK_SCP_ULTRA_H_

#define ULTRA_BUF_OFFSET (0xC000)

struct audio_ultra_dram {
	unsigned long long phy_addr;
	unsigned long long va_addr;
	unsigned long long size;
	unsigned char *vir_addr;
};

struct mtk_base_scp_ultra_dump {
	bool dump_flag;
	struct audio_ultra_dram dump_resv_mem;
};

struct mtk_base_scp_ultra_mem {
	struct snd_dma_buffer ultra_dl_dma_buf;
	struct snd_dma_buffer ultra_ul_dma_buf;
	int ultra_ul_memif_id;
	int ultra_dl_memif_id;
};

struct mtk_base_scp_ultra {
	struct device *dev;
	const struct snd_pcm_hardware *mtk_scp_hardware;
	const struct snd_soc_component_driver *component_driver;
	struct mtk_base_scp_ultra_mem ultra_mem;
	struct mtk_base_scp_ultra_dump ultra_dump;
	struct audio_ultra_dram ultra_reserve_dram;
	unsigned int usnd_state;
	unsigned int scp_ultra_dl_memif_id;
	unsigned int scp_ultra_ul_memif_id;
};

int mtk_scp_ultra_allocate_mem(struct snd_pcm_substream *substream,
			       dma_addr_t *phys_addr,
			       unsigned char **virt_addr,
			       unsigned int size);

#endif
