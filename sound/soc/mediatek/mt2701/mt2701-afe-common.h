/*
 * mt2701-afe-common.h  --  Mediatek 2701 audio driver definitions
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

#ifndef _MT_2701_AFE_COMMON_H_
#define _MT_2701_AFE_COMMON_H_
#include <sound/soc.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include "mt2701-reg.h"
#include "../common/mtk-base-afe.h"

void mt2701_regmap_update_bits(struct regmap *map, unsigned int reg,
			    unsigned int mask, unsigned int val);

void mt2701_regmap_write(struct regmap *map, unsigned int reg, unsigned int val);
void mt2701_regmap_read(struct regmap *map, unsigned int reg, unsigned int *val);

#define MT2701_STREAM_DIR_NUM (SNDRV_PCM_STREAM_LAST + 1)
#define MT2701_PLL_DOMAIN_0_RATE	98304000U
#define MT2701_PLL_DOMAIN_1_RATE	90316800U
#define MT2701_AUD_AUD_MUX1_DIV_RATE (MT2701_PLL_DOMAIN_0_RATE / 2U)
#define MT2701_AUD_AUD_MUX2_DIV_RATE (MT2701_PLL_DOMAIN_1_RATE / 2U)

#define MT2712_PLL_DOMAIN_0_RATE	196608000U
#define MT2712_PLL_DOMAIN_1_RATE	180633600U
#define AUDIO_CLOCK_45M 45158400U
#define AUDIO_CLOCK_49M 49152000U
#define AUDIO_CLOCK_196M 196608000U
#define AUDIO_CLOCK_180M 180633600U
#define MT2701_TDM_IN_CLK_THR 12288000U
#define MT2701_TDM_OUT_CLK_THR 24576000U
#define MT2701_TDM_MAX_CLK_DIV 256U

enum {
	MT2701_I2S_1,
	MT2701_I2S_2,
	MT2701_I2S_3,
	MT2701_I2S_4,
	MT2701_I2S_NUM,
};

enum {
	MT2701_MEMIF_DL1,
	MT2701_MEMIF_DL2,
	MT2701_MEMIF_DL3,
	MT2701_MEMIF_DL4,
	MT2701_MEMIF_DL5,
	MT2701_MEMIF_DL_SINGLE_NUM,
	MT2701_MEMIF_DLM = MT2701_MEMIF_DL_SINGLE_NUM,
	MT2701_MEMIF_UL1,
	MT2701_MEMIF_UL2,
	MT2701_MEMIF_UL3,
	MT2701_MEMIF_UL4,
	MT2701_MEMIF_UL5,
	MT2701_MEMIF_AWB2,
	MT2701_MEMIF_DLBT,
	MT2701_MEMIF_ULBT,
	MT2701_MEMIF_DLMOD,
	MT2701_MEMIF_ULMOD,
	MT2701_MEMIF_DLTDM1,
	MT2701_MEMIF_DLTDM2,
	MT2701_MEMIF_ULTDM,
	MT2701_MEMIF_NUM,
	MT2701_IO_I2S = MT2701_MEMIF_NUM,
	MT2701_IO_2ND_I2S,
	MT2701_IO_3RD_I2S,
	MT2701_IO_4TH_I2S,
	MT2701_IO_5TH_I2S,
	MT2701_IO_6TH_I2S,
	MT2701_IO_AADC,
	MT2701_IO_MRG,
	MT2701_IO_MOD,
	MT2701_IO_TDMO1,
	MT2701_IO_TDMO2,
	MT2701_IO_TDMI,
};

enum {
	MT2701_IRQ_ASYS_START,
	MT2701_IRQ_ASYS_IRQ1 = MT2701_IRQ_ASYS_START,
	MT2701_IRQ_ASYS_IRQ2,
	MT2701_IRQ_ASYS_IRQ3,
	MT2701_IRQ_ASYS_IRQ4,
	MT2701_IRQ_ASYS_IRQ5,
	MT2701_IRQ_ASYS_IRQ6,
	MT2701_IRQ_ASYS_END,
};

enum {
	MT2701_TDMO_1,
	MT2701_TDMO_2,
	MT2701_TDMI,
	MT2701_TDM_NUM,
};

/* 2701 clock def */
enum audio_system_clock_type {
	MT2701_AUD_INFRA_SYS_AUDIO,
	MT2701_AUD_AUD_MUX1_SEL,
	MT2701_AUD_AUD_MUX2_SEL,
	MT2701_AUD_AUD_MUX1_DIV,
	MT2701_AUD_AUD_MUX2_DIV,
	MT2701_AUD_AUD_48K_TIMING,
	MT2701_AUD_AUD_44K_TIMING,
	MT2701_AUD_AUDPLL_MUX_SEL,
	MT2701_AUD_APLL_SEL,
	MT2701_AUD_AUD1PLL_98M,
	MT2701_AUD_AUD2PLL_90M,
	MT2701_AUD_HADDS2PLL_98M,
	MT2701_AUD_HADDS2PLL_294M,
	MT2701_AUD_AUDPLL,
	MT2701_AUD_AUDPLL_D4,
	MT2701_AUD_AUDPLL_D8,
	MT2701_AUD_AUDPLL_D16,
	MT2701_AUD_AUDPLL_D24,
	MT2701_AUD_AUDINTBUS,
	MT2701_AUD_CLK_26M,
	MT2701_AUD_SYSPLL1_D4,
	MT2701_AUD_AUD_K1_SRC_SEL,
	MT2701_AUD_AUD_K2_SRC_SEL,
	MT2701_AUD_AUD_K3_SRC_SEL,
	MT2701_AUD_AUD_K4_SRC_SEL,
	MT2701_AUD_AUD_K5_SRC_SEL,
	MT2701_AUD_AUD_K6_SRC_SEL,
	MT2701_AUD_AUD_K1_SRC_DIV,
	MT2701_AUD_AUD_K2_SRC_DIV,
	MT2701_AUD_AUD_K3_SRC_DIV,
	MT2701_AUD_AUD_K4_SRC_DIV,
	MT2701_AUD_AUD_K5_SRC_DIV,
	MT2701_AUD_AUD_K6_SRC_DIV,
	MT2701_AUD_AUD_I2S1_MCLK,
	MT2701_AUD_AUD_I2S2_MCLK,
	MT2701_AUD_AUD_I2S3_MCLK,
	MT2701_AUD_AUD_I2S4_MCLK,
	MT2701_AUD_AUD_I2S5_MCLK,
	MT2701_AUD_AUD_I2S6_MCLK,
	MT2701_AUD_ASM_M_SEL,
	MT2701_AUD_ASM_H_SEL,
	MT2701_AUD_UNIVPLL2_D4,
	MT2701_AUD_UNIVPLL2_D2,
	MT2701_AUD_SYSPLL_D5,
	MT2701_CLOCK_NUM
};

/* 2712 clock def */
enum audio_system_clock_type_2712 {
	MT2712_AUD_AUD_INTBUS_SEL,
	MT2712_AUD_AUD_APLL1_SEL,
	MT2712_AUD_AUD_APLL2_SEL,
	MT2712_AUD_A1SYS_HP_SEL,
	MT2712_AUD_A2SYS_HP_SEL,
	MT2712_AUD_APLL_SEL,
	MT2712_AUD_APLL2_SEL,
	MT2712_AUD_I2SO1_SEL,
	MT2712_AUD_I2SO2_SEL,
	MT2712_AUD_I2SO3_SEL,
	MT2712_AUD_TDMO0_SEL,
	MT2712_AUD_TDMO1_SEL,
	MT2712_AUD_I2SI1_SEL,
	MT2712_AUD_I2SI2_SEL,
	MT2712_AUD_I2SI3_SEL,
	MT2712_AUD_APLL_DIV0,
	MT2712_AUD_APLL_DIV1,
	MT2712_AUD_APLL_DIV2,
	MT2712_AUD_APLL_DIV3,
	MT2712_AUD_APLL_DIV4,
	MT2712_AUD_APLL_DIV5,
	MT2712_AUD_APLL_DIV6,
	MT2712_AUD_APLL_DIV7,
	MT2712_AUD_DIV_PDN0,
	MT2712_AUD_DIV_PDN1,
	MT2712_AUD_DIV_PDN2,
	MT2712_AUD_DIV_PDN3,
	MT2712_AUD_DIV_PDN4,
	MT2712_AUD_DIV_PDN5,
	MT2712_AUD_DIV_PDN6,
	MT2712_AUD_DIV_PDN7,
	MT2712_AUD_APMIXED_APLL1,
	MT2712_AUD_APMIXED_APLL2,
	MT2712_AUD_EXT_I_1,
	MT2712_AUD_EXT_I_2,
	MT2712_AUD_CLK_26M,
	MT2712_AUD_SYSPLL1_D4,
	MT2712_AUD_SYSPLL1_D2,
	MT2712_AUD_UNIVPLL3_D2,
	MT2712_AUD_UNIVPLL2_D8,
	MT2712_AUD_SYSPLL3_D2,
	MT2712_AUD_SYSPLL3_D4,
	MT2712_AUD_APLL1,
	MT2712_AUD_APLL1_D2,
	MT2712_AUD_APLL1_D4,
	MT2712_AUD_APLL1_D8,
	MT2712_AUD_APLL1_D16,
	MT2712_AUD_APLL2,
	MT2712_AUD_APLL2_D2,
	MT2712_AUD_APLL2_D4,
	MT2712_AUD_APLL2_D8,
	MT2712_AUD_APLL2_D16,
	MT2712_AUD_ASM_L_SEL,
	MT2712_AUD_UNIVPLL2_D4,
	MT2712_AUD_UNIVPLL2_D2,
	MT2712_AUD_SYSPLL_D5,
	MT2712_CLOCK_NUM
};
#define MCLK_I2SIN_OFFSET 5
#define MCLK_TDM_OFFSET 3

static const unsigned int mt2701_afe_backup_list[] = {
	AUDIO_TOP_CON0,
	AUDIO_TOP_CON4,
	AUDIO_TOP_CON5,
	ASYS_TOP_CON,
	AFE_CONN0,
	AFE_CONN1,
	AFE_CONN2,
	AFE_CONN3,
	AFE_CONN15,
	AFE_CONN16,
	AFE_CONN17,
	AFE_CONN18,
	AFE_CONN19,
	AFE_CONN20,
	AFE_CONN21,
	AFE_CONN22,
	AFE_DAC_CON0,
	AFE_MEMIF_PBUF_SIZE,
};

struct snd_pcm_substream;
struct mtk_base_irq_data;

struct mt2701_i2s_data {
	unsigned int i2s_ctrl_reg;
	unsigned int i2s_pwn_shift;
	unsigned int i2s_asrc_fs_shift;
	unsigned int i2s_asrc_fs_mask;
};

enum mt2701_i2s_dir {
	I2S_OUT,
	I2S_IN,
	I2S_DIR_NUM,
};

enum mt2701_i2s_mode {
	I2S_COCLK,
	I2S_SEPCLK,
};

struct mt2701_i2s_path {
	int dai_id;
	unsigned int mclk_rate;
	int on[I2S_DIR_NUM];
	int occupied[I2S_DIR_NUM];
	enum mt2701_i2s_mode i2s_mode;
	const struct mt2701_i2s_data *i2s_data[2];
};

enum mt2701_tdm_channel {
	TDM_2CH = 0,
	TDM_4CH,
	TDM_8CH,
	TDM_12CH,
	TDM_16CH,
};

enum mt2701_tdm_mode {
	TDM_MODE_COCLK_O1,
	TDM_MODE_COCLK_O2,
	TDM_MODE_SEPCLK,
};

struct mt2701_tdm_data {
	unsigned int tdm_bck_reg;
	unsigned int tdm_plldiv_shift;
	unsigned int tdm_pll_sel_shift;
	unsigned int tdm_bck_on_shift;
	unsigned int tdm_ctrl_reg;
	unsigned int tdm_ctrl_2nd_reg;
	unsigned int tdm_conn_reg;
	unsigned int tdm_conn_2nd_reg;
	unsigned int tdm_lrck_cycle_shift;
	unsigned int tdm_on_shift;
	unsigned int tdm_agent_reg;
	unsigned int tdm_agent_bit_width_shift;
	unsigned int tdm_agent_ch_num_shift;
};

struct mt2701_tdm_path {
	unsigned int mclk_rate;
	const struct mt2701_tdm_data *tdm_data;
};

struct mt2701_tdm_coclk_info {
	int src;
	int on;
	int hw_on;
	unsigned int channels;
	int bit_width;
	unsigned int sample_rate;
};

struct tdm_in_lrck_setting {
	unsigned int delay_half_T;
	unsigned int width;
};

struct clock_ctrl {
	int (*init_clock)(struct mtk_base_afe *afe);
	int (*afe_enable_clock)(struct mtk_base_afe *afe);
	void (*afe_disable_clock)(struct mtk_base_afe *afe);
	void (*mclk_configuration)(struct mtk_base_afe *afe, int id, int domain,
			       unsigned int mclk);
	int (*enable_mclk)(struct mtk_base_afe *afe, int id);
	void (*disable_mclk)(struct mtk_base_afe *afe, int id);
	unsigned int apll0_rate;
	unsigned int apll1_rate;
};

struct mt2701_afe_private {
	struct clk *clocks[MT2712_CLOCK_NUM];
	struct mt2701_i2s_path i2s_path[MT2701_I2S_NUM];
	struct mt2701_tdm_path tdm_path[MT2701_TDM_NUM];
	bool mrg_enable[MT2701_STREAM_DIR_NUM];
	bool pcm_enable[MT2701_STREAM_DIR_NUM];
	bool pcm_slave;
	bool tdm_coclk;
	bool i2so2_mclk;
	struct mt2701_tdm_coclk_info tdm_coclk_info;
	struct tdm_in_lrck_setting tdm_in_lrck;
	struct clock_ctrl *clk_ctrl;
};

#endif
