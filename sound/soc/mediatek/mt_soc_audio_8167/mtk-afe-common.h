/*
 * mtk-afe-common.h  --  Mediatek audio driver common definitions
 *
 * Copyright (c) 2016 MediaTek Inc.
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

#ifndef _MTK_AFE_COMMON_H_
#define _MTK_AFE_COMMON_H_

#include <linux/clk.h>
#include <linux/regmap.h>
#include <sound/asound.h>

/* #define COMMON_CLOCK_FRAMEWORK_API */
/* #define IDLE_TASK_DRIVER_API */


enum {
	MTK_AFE_MEMIF_DL1,
	MTK_AFE_MEMIF_DL2,
	MTK_AFE_MEMIF_VUL,
	MTK_AFE_MEMIF_DAI,
	MTK_AFE_MEMIF_AWB,
	MTK_AFE_MEMIF_MOD_DAI,
	MTK_AFE_MEMIF_HDMI,
	MTK_AFE_MEMIF_NUM,
	MTK_AFE_BACKEND_BASE = MTK_AFE_MEMIF_NUM,
	MTK_AFE_IO_MOD_PCM1 = MTK_AFE_BACKEND_BASE,
	MTK_AFE_IO_MOD_PCM2,
	MTK_AFE_IO_INT_ADDA,
	MTK_AFE_IO_I2S,
	MTK_AFE_IO_2ND_I2S,
	MTK_AFE_IO_HW_GAIN1,
	MTK_AFE_IO_HW_GAIN2,
	MTK_AFE_IO_MRG,
	MTK_AFE_IO_MRG_BT,
	MTK_AFE_IO_PCM_BT,
	MTK_AFE_IO_HDMI,
	MTK_AFE_IO_DL_BE,
	MTK_AFE_BACKEND_END,
	MTK_AFE_BACKEND_NUM = (MTK_AFE_BACKEND_END - MTK_AFE_BACKEND_BASE),
};

enum {
	MTK_CLK_INFRASYS_AUD,
	MTK_CLK_TOP_PDN_AUD,
	MTK_CLK_TOP_PDN_AUD_BUS,
	MTK_CLK_I2S0_M,
	MTK_CLK_I2S1_M,
	MTK_CLK_I2S2_M,
	MTK_CLK_I2S3_M,
	MTK_CLK_I2S3_B,
	MTK_CLK_BCK0,
	MTK_CLK_BCK1,
	MTK_CLK_NUM
};

enum mtk_afe_tdm_ch_start {
	AFE_TDM_CH_START_O30_O31 = 0,
	AFE_TDM_CH_START_O32_O33,
	AFE_TDM_CH_START_O34_O35,
	AFE_TDM_CH_START_O36_O37,
	AFE_TDM_CH_ZERO,
};

enum mtk_afe_irq_mode {
	MTK_AFE_IRQ_1 = 0,
	MTK_AFE_IRQ_2,
	MTK_AFE_IRQ_5, /* dedicated for HDMI */
	MTK_AFE_IRQ_6, /* dedicated for SPDIF */
	MTK_AFE_IRQ_7,
	MTK_AFE_IRQ_NUM
};

enum mtk_afe_top_clock_gate {
	MTK_AFE_CG_AFE,
	MTK_AFE_CG_I2S,
	MTK_AFE_CG_22M,
	MTK_AFE_CG_24M,
	MTK_AFE_CG_APLL_TUNER,
	MTK_AFE_CG_APLL2_TUNER,
	MTK_AFE_CG_HDMI,
	MTK_AFE_CG_SPDIF,
	MTK_AFE_CG_ADC,
	MTK_AFE_CG_DAC,
	MTK_AFE_CG_DAC_PREDIS,
	MTK_AFE_CG_NUM
};

enum {
	MTK_AFE_DEBUGFS_AFE,
	MTK_AFE_DEBUGFS_HDMI,
	MTK_AFE_DEBUGFS_NUM,
};


struct snd_pcm_substream;

struct mtk_afe_memif_data {
	int id;
	const char *name;
	int reg_ofs_base;
	int reg_ofs_end;
	int reg_ofs_cur;
	int fs_shift;
	int mono_shift;
	int enable_shift;
	int irq_reg_cnt;
	int irq_cnt_shift;
	int irq_mode;
	int irq_fs_reg;
	int irq_fs_shift;
	int irq_clr_shift;
	int max_sram_size;
	int sram_offset;
	int format_reg;
	int format_shift;
	int conn_format_mask;
	int prealloc_size;
};

struct mtk_afe_be_dai_data {
	bool prepared[SNDRV_PCM_STREAM_LAST + 1];
};

struct mtk_afe_memif {
	unsigned int phys_buf_addr;
	int buffer_size;
	bool use_sram;
	bool prepared;
	struct snd_pcm_substream *substream;
	const struct mtk_afe_memif_data *data;
};

struct mtk_afe_control_data {
	unsigned int sinegen_type;
	unsigned int sinegen_fs;
	unsigned int loopback_type;
	bool hdmi_force_clk;
};

struct mtk_afe {
	/* address for ioremap audio hardware register */
	void __iomem *base_addr;
	void __iomem *sram_address;
	u32 sram_phy_address;
	u32 sram_size;
	struct device *dev;
	struct regmap *regmap;
	struct mtk_afe_memif memif[MTK_AFE_MEMIF_NUM];
	struct mtk_afe_be_dai_data be_data[MTK_AFE_BACKEND_NUM];
	struct mtk_afe_control_data ctrl_data;
	struct clk *clocks[MTK_CLK_NUM];
	unsigned int *backup_regs;
	bool suspended;
	int afe_on_ref_cnt;
	int adda_afe_on_ref_cnt;
	int i2s_out_on_ref_cnt;
	int daibt_on_ref_cnt;
	int irq_mode_ref_cnt[MTK_AFE_IRQ_NUM];
	int top_cg_ref_cnt[MTK_AFE_CG_NUM];
	/* locks */
	spinlock_t afe_ctrl_lock;
#ifdef IDLE_TASK_DRIVER_API
	int emi_clk_ref_cnt;
	struct mutex emi_clk_mutex;
#endif
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_dentry[MTK_AFE_DEBUGFS_NUM];
#endif
};

#endif
