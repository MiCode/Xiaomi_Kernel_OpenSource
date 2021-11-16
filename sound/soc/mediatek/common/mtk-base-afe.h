/*
 * mtk-base-afe.h  --  Mediatek base afe structure
 *
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Garlic Tseng <garlic.tseng@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _MTK_BASE_AFE_H_
#define _MTK_BASE_AFE_H_

struct mtk_base_memif_data {
	int id;
	const char *name;
	int reg_ofs_base;
	int reg_ofs_cur;
	int reg_ofs_end;
	int reg_ofs_base_msb;
	int reg_ofs_cur_msb;
	int reg_ofs_end_msb;
	int fs_reg;
	int fs_shift;
	int fs_maskbit;
	int mono_reg;
	int mono_shift;
	int mono_invert;
	int quad_ch_reg;
	int quad_ch_mask_shift;
	int quad_ch_shift;
	int enable_reg;
	int enable_shift;
	int hd_reg;
	int hd_shift;
	int msb_reg;
	int msb_shift;
	int msb2_reg;
	int msb2_shift;
	int agent_disable_reg;
	int agent_disable_shift;
	/* playback memif only */
	int pbuf_reg;
	int pbuf_mask_shift;
	int pbuf_shift;
	int minlen_reg;
	int minlen_mask_shift;
	int minlen_shift;
};

struct mtk_base_irq_data {
	int id;
	int irq_cnt_reg;
	int irq_cnt_shift;
	int irq_cnt_maskbit;
	int irq_fs_reg;
	int irq_fs_shift;
	int irq_fs_maskbit;
	int irq_en_reg;
	int irq_en_shift;
	int irq_clr_reg;
	int irq_clr_shift;
	int irq_ap_en_reg;
	int irq_ap_en_shift;
	int irq_scp_en_reg;
	int irq_scp_en_shift;
};

struct dentry;
struct device;
struct list_head;
struct mtk_base_afe_memif;
struct mtk_base_afe_irq;
struct mtk_base_afe_dai;
struct regmap;
struct snd_pcm_substream;
struct snd_soc_dai;
struct snd_soc_dai_driver;
typedef int (*mtk_sp_copy_f)(struct snd_pcm_substream *substream,
				 int channel, unsigned long hwoff,
				 void *buf, unsigned long bytes);

struct mtk_base_afe {
	void __iomem *base_addr;
	struct device *dev;
	struct regmap *regmap;
	struct mutex irq_alloc_lock; /* dynamic alloc irq lock */

	unsigned int const *reg_back_up_list;
	unsigned int *reg_back_up;
	unsigned int reg_back_up_list_num;

	int (*runtime_suspend)(struct device *dev);
	int (*runtime_resume)(struct device *dev);
	bool suspended;

	struct mtk_base_afe_memif *memif;
	int memif_size;
	int memif_32bit_supported;
	struct mtk_base_afe_irq *irqs;
	int irqs_size;

	struct list_head sub_dais;
	struct snd_soc_dai_driver *dai_drivers;
	unsigned int num_dai_drivers;

	const struct snd_pcm_hardware *mtk_afe_hardware;
	int (*memif_fs)(struct snd_pcm_substream *substream,
			unsigned int rate);
	int (*irq_fs)(struct snd_pcm_substream *substream,
		      unsigned int rate);
	int (*get_dai_fs)(struct mtk_base_afe *afe,
			  int dai_id, unsigned int rate);
	int (*get_memif_pbuf_size)(struct snd_pcm_substream *substream);

	void *sram;
	int (*request_dram_resource)(struct device *dev);
	int (*release_dram_resource)(struct device *dev);

	struct dentry *debugfs;
	const struct mtk_afe_debug_cmd *debug_cmds;

	void *platform_priv;

	int (*copy)(struct snd_pcm_substream *substream,
		    int channel, unsigned long hwoff,
		    void *buf, unsigned long bytes,
		    mtk_sp_copy_f sp_copy);
};

struct mtk_base_afe_memif {
	unsigned char *dma_area;
	dma_addr_t dma_addr;
	size_t dma_bytes;

	struct snd_pcm_substream *substream;
	const struct mtk_base_memif_data *data;
	int irq_usage;
	int const_irq;
	unsigned int phys_buf_addr;
	int buffer_size;

	int using_sram;
	int use_dram_only;
	int use_adsp_share_mem;
#if defined(CONFIG_MTK_VOW_BARGE_IN_SUPPORT)
	bool vow_bargein_enable;
#endif
#if defined(CONFIG_SND_SOC_MTK_SCP_SMARTPA)
	bool scp_spk_enable;
#endif

	int use_mmap_share_mem;  // 1 : dl   2 : ul

	bool ack_enable;
	int (*ack)(struct snd_pcm_substream *substream);
};

struct mtk_base_afe_irq {
	const struct mtk_base_irq_data *irq_data;
	int irq_occupyed;
};

struct mtk_base_afe_dai {
	struct snd_soc_dai_driver *dai_drivers;
	unsigned int num_dai_drivers;

	const struct snd_kcontrol_new *controls;
	unsigned int num_controls;
	const struct snd_soc_dapm_widget *dapm_widgets;
	unsigned int num_dapm_widgets;
	const struct snd_soc_dapm_route *dapm_routes;
	unsigned int num_dapm_routes;

	struct list_head list;
};

#endif

