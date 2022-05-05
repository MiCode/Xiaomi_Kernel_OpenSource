/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-base-afe.h  --  Mediatek base afe structure
 *
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Garlic Tseng <garlic.tseng@mediatek.com>
 */

#ifndef _MTK_BASE_AFE_H_
#define _MTK_BASE_AFE_H_

#define MTK_STREAM_NUM (SNDRV_PCM_STREAM_LAST + 1)

enum {
	MTK_AFE_RATE_8K,
	MTK_AFE_RATE_11K,
	MTK_AFE_RATE_12K,
	MTK_AFE_RATE_384K,
	MTK_AFE_RATE_16K,
	MTK_AFE_RATE_22K,
	MTK_AFE_RATE_24K,
	MTK_AFE_RATE_352K,
	MTK_AFE_RATE_32K,
	MTK_AFE_RATE_44K,
	MTK_AFE_RATE_48K,
	MTK_AFE_RATE_88K,
	MTK_AFE_RATE_96K,
	MTK_AFE_RATE_176K,
	MTK_AFE_RATE_192K,
	MTK_AFE_RATE_260K,
};

enum {
	MTK_AFE_DAI_MEMIF_RATE_8K,
	MTK_AFE_DAI_MEMIF_RATE_16K,
	MTK_AFE_DAI_MEMIF_RATE_32K,
	MTK_AFE_DAI_MEMIF_RATE_48K,
};

enum {
	MTK_AFE_PCM_RATE_8K,
	MTK_AFE_PCM_RATE_16K,
	MTK_AFE_PCM_RATE_32K,
	MTK_AFE_PCM_RATE_48K,
};

enum {
	MTKAIF_PROTOCOL_1 = 0,
	MTKAIF_PROTOCOL_2,
	MTKAIF_PROTOCOL_2_CLK_P2,
};

enum {
	MTK_AFE_ADDA_DL_GAIN_MUTE = 0,
	MTK_AFE_ADDA_DL_GAIN_NORMAL = 0xf74f,
	/* SA suggest apply -0.3db to audio/speech path */
};

/* SMC CALL Operations */
enum mtk_audio_smc_call_op {
	MTK_AUDIO_SMC_OP_INIT = 0,
	MTK_AUDIO_SMC_OP_DRAM_REQUEST,
	MTK_AUDIO_SMC_OP_DRAM_RELEASE,
	MTK_AUDIO_SMC_OP_FM_REQUEST,
	MTK_AUDIO_SMC_OP_FM_RELEASE,
	MTK_AUDIO_SMC_OP_ADSP_REQUEST,
	MTK_AUDIO_SMC_OP_ADSP_RELEASE,
	MTK_AUDIO_SMC_OP_DOMAIN_SIDEBANDS,
	MTK_AUDIO_SMC_OP_NUM
};

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
	int quad_ch_mask;
	int quad_ch_shift;
	int enable_reg;
	int enable_shift;
	int hd_reg;
	int hd_shift;
	int hd_align_reg;
	int hd_align_mshift;
	int msb_reg;
	int msb_shift;
	int msb2_reg;
	int msb2_shift;
	int agent_disable_reg;
	int agent_disable_shift;
	/* playback memif only */
	int pbuf_reg;
	int pbuf_mask;
	int pbuf_shift;
	int minlen_reg;
	int minlen_mask;
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

	/* using scp semaphore to protect reg access */
	int is_scp_sema_support;

	/* Bit banding of memif use AFE_AGEN_ON_SET/CLR
	 * to control memif enable bit.
	 */
	int is_memif_bit_banding;

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
	unsigned int phys_buf_addr;
	int buffer_size;
	struct snd_pcm_substream *substream;
	const struct mtk_base_memif_data *data;
	int irq_usage;
	int const_irq;

	int using_sram;
	int use_dram_only;
	unsigned char *dma_area;
	dma_addr_t dma_addr;
	size_t dma_bytes;
	int use_adsp_share_mem;
	bool ack_enable;
	int (*ack)(struct snd_pcm_substream *substream);
	int use_mmap_share_mem;  // 1: dl, 2: ul
	bool vow_barge_in_enable;
	bool scp_ultra_enable;
#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
	int use_scp_share_mem;
#endif
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

