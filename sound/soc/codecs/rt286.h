/*
 * rt286.h  --  RT286 ALSA SoC audio driver
 *
 * Copyright 2011 Realtek Microelectronics
 * Author: Johnny Hsu <johnnyhsu@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __RT286_H__
#define __RT286_H__

#define NODE_ID_AUDIO_FUNCTION_GROUP			0x01
#define NODE_ID_DAC_OUT1				0x02
#define NODE_ID_DAC_OUT2				0x03
#define NODE_ID_ADC_IN1					0x09
#define NODE_ID_ADC_IN2					0x08
#define NODE_ID_MIXER_IN				0x0b
#define NODE_ID_MIXER_OUT1				0x0c
#define NODE_ID_MIXER_OUT2				0x0d
#define NODE_ID_DMIC1					0x12
#define NODE_ID_DMIC2					0x13
#define NODE_ID_SPK_OUT					0x14
#define NODE_ID_MIC1					0x18
#define NODE_ID_LINE1					0x1a
#define NODE_ID_BEEP					0x1d
#define NODE_ID_SPDIF					0x1e
#define NODE_ID_VENDOR_REGISTERS			0x20
#define NODE_ID_HP_OUT					0x21
#define NODE_ID_MIXER_IN1				0x22
#define NODE_ID_MIXER_IN2				0x23
#define TOTAL_NODE_ID					0x23

#define CONNECTION_INDEX_MIC1				0X0
#define CONNECTION_INDEX_DMIC				0X5

/* SPDIF (0x06) */
#define RT286_SPDIF_SEL_SFT	0
#define RT286_SPDIF_SEL_PCM0	0
#define RT286_SPDIF_SEL_PCM1	1
#define RT286_SPDIF_SEL_SPOUT	2
#define RT286_SPDIF_SEL_PP	3

/* RECMIX (0x0b) */
#define RT286_M_REC_BEEP_SFT	0
#define RT286_M_REC_LINE1_SFT	1
#define RT286_M_REC_MIC1_SFT	2
#define RT286_M_REC_I2S_SFT	3

/* Front (0x0c) */
#define RT286_M_FRONT_DAC_SFT	0
#define RT286_M_FRONT_REC_SFT	1

/* SPK-OUT (0x14) */
#define RT286_SPK_SEL_MASK	0x1
#define RT286_SPK_SEL_SFT	0
#define RT286_SPK_SEL_F		0
#define RT286_SPK_SEL_S		1

/* HP-OUT (0x21) */
#define RT286_HP_SEL_MASK	0x1
#define RT286_HP_SEL_SFT	0
#define RT286_HP_SEL_F		0
#define RT286_HP_SEL_S		1

/* ADC (0x22) (0x23) */
#define RT286_ADC_SEL_MASK	0x7
#define RT286_ADC_SEL_SFT	0
#define RT286_ADC_SEL_SURR	0
#define RT286_ADC_SEL_FRONT	1
#define RT286_ADC_SEL_DMIC	2
#define RT286_ADC_SEL_BEEP	4
#define RT286_ADC_SEL_LINE1	5
#define RT286_ADC_SEL_I2S	6
#define RT286_ADC_SEL_MIC1	7

/* System Clock Source */
enum {
	RT286_SCLK_S_MCLK,
	RT286_SCLK_S_PLL1,
	RT286_SCLK_S_RCCLK,
};

enum {
	RT286_AIF1,
	RT286_AIF2,
	RT286_AIF3,
	RT286_AIFS,
};

enum {
	RT286_DMIC_DIS,
	RT286_DMIC1,
	RT286_DMIC2,
};

enum {
	RT286_J_IN_EVENT, /* Jack insert */
	RT286_J_OUT_EVENT, /* Jack evulse */
	RT286_BP_EVENT, /* Button Press */
	RT286_BR_EVENT, /* Button Release */
	RT286_UN_EVENT, /* Unknown */
};

struct rt286_pll_code {
	bool m_bp; /* Indicates bypass m code or not. */
	int m_code;
	int n_code;
	int k_code;
};
#endif /* __RT286_H__ */
