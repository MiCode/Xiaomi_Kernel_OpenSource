/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mt6877-afe-clk.h  --  Mediatek 6833 afe clock ctrl definition
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Eason Yen <eason.yen@mediatek.com>
 */

#ifndef _MT6877_AFE_CLOCK_CTRL_H_
#define _MT6877_AFE_CLOCK_CTRL_H_

/* mtkaif_calibration */
#define CKSYS_AUD_TOP_CFG 0x032c
#define CKSYS_AUD_TOP_MON 0x0330

/* audio dcm */
#define PERI_BUS_DCM_CTRL 0x0074

/* APLL */
#define APLL1_W_NAME "APLL1"
#define APLL2_W_NAME "APLL2"
enum {
	MT6877_APLL1 = 0,
	MT6877_APLL2,
};

enum {
	CLK_AFE = 0,
	CLK_APLL22M,
	CLK_APLL24M,
	CLK_APLL1_TUNER,
	CLK_APLL2_TUNER,
	CLK_INFRA_SYS_AUDIO,
	CLK_INFRA_AUDIO_26M,
	CLK_SCP_SYS_AUD,
	CLK_MUX_AUDIO,
	CLK_MUX_AUDIO_H,
	CLK_MUX_AUDIOINTBUS,
	CLK_TOP_MAINPLL_D4_D4,
	/* apll related mux */
	CLK_TOP_MUX_AUD_1,
	CLK_TOP_APLL1_CK,
	CLK_TOP_MUX_AUD_2,
	CLK_TOP_APLL2_CK,
	CLK_TOP_MUX_AUD_ENG1,
	CLK_TOP_APLL1_D4,
	CLK_TOP_MUX_AUD_ENG2,
	CLK_TOP_APLL2_D4,
	CLK_TOP_I2S0_M_SEL,
	CLK_TOP_I2S1_M_SEL,
	CLK_TOP_I2S2_M_SEL,
	CLK_TOP_I2S3_M_SEL,
	CLK_TOP_I2S4_M_SEL,
	CLK_TOP_I2S5_M_SEL,
	CLK_TOP_I2S6_M_SEL,
	CLK_TOP_I2S7_M_SEL,
	CLK_TOP_I2S8_M_SEL,
	CLK_TOP_I2S9_M_SEL,
	CLK_TOP_APLL12_DIV0,
	CLK_TOP_APLL12_DIV1,
	CLK_TOP_APLL12_DIV2,
	CLK_TOP_APLL12_DIV3,
	CLK_TOP_APLL12_DIV4,
	CLK_TOP_APLL12_DIVB,
	CLK_TOP_APLL12_DIV5,
	CLK_TOP_APLL12_DIV6,
	CLK_TOP_APLL12_DIV7,
	CLK_TOP_APLL12_DIV8,
	CLK_TOP_APLL12_DIV9,
	CLK_CLK26M,
	CLK_NUM
};

struct mtk_base_afe;

int mt6877_init_clock(struct mtk_base_afe *afe);
int mt6877_afe_enable_clock(struct mtk_base_afe *afe);
void mt6877_afe_disable_clock(struct mtk_base_afe *afe);

int mt6877_afe_dram_request(struct device *dev);
int mt6877_afe_dram_release(struct device *dev);

int mt6877_apll1_enable(struct mtk_base_afe *afe);
void mt6877_apll1_disable(struct mtk_base_afe *afe);

int mt6877_apll2_enable(struct mtk_base_afe *afe);
void mt6877_apll2_disable(struct mtk_base_afe *afe);

int mt6877_get_apll_rate(struct mtk_base_afe *afe, int apll);
int mt6877_get_apll_by_rate(struct mtk_base_afe *afe, int rate);
int mt6877_get_apll_by_name(struct mtk_base_afe *afe, const char *name);

/* these will be replaced by using CCF */
int mt6877_mck_enable(struct mtk_base_afe *afe, int mck_id, int rate);
void mt6877_mck_disable(struct mtk_base_afe *afe, int mck_id);

int mt6877_set_audio_int_bus_parent(struct mtk_base_afe *afe,
				    int clk_id);

#endif
