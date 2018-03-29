/*
 * mtk_afe_regs.h  --  Mediatek audio register definitions
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

#ifndef _MTK_AFE_REGS_H_
#define _MTK_AFE_REGS_H_

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
#define AUDIO_TOP_CON0		0x0000
#define AUDIO_TOP_CON1		0x0004
#define AUDIO_TOP_CON3		0x000c
#define AFE_DAC_CON0		0x0010
#define AFE_DAC_CON1		0x0014
#define AFE_I2S_CON		0x0018
#define AFE_I2S_CON1		0x0034
#define AFE_I2S_CON2		0x0038
#define AFE_I2S_CON3		0x004c
#define AFE_DAIBT_CON0          0x001c
#define AFE_MRGIF_CON           0x003c
#define AFE_CONN_24BIT		0x006c

#define AFE_CONN0		0x0020
#define AFE_CONN1		0x0024
#define AFE_CONN2		0x0028
#define AFE_CONN3		0x002C
#define AFE_CONN4		0x0030
#define AFE_CONN5		0x005C
#define AFE_HDMI_CONN0		0x0390

/* Memory interface */
#define AFE_DL1_BASE		0x0040
#define AFE_DL1_CUR		0x0044
#define AFE_DL1_END		0x0048
#define AFE_DL2_BASE		0x0050
#define AFE_DL2_CUR		0x0054
#define AFE_DL2_END             0x0058
#define AFE_AWB_BASE		0x0070
#define AFE_AWB_END             0x0078
#define AFE_AWB_CUR		0x007c
#define AFE_VUL_BASE		0x0080
#define AFE_VUL_CUR		0x008c
#define AFE_VUL_END		0x0088
#define AFE_DAI_BASE		0x0090
#define AFE_DAI_END		0x0098
#define AFE_DAI_CUR		0x009c
#define AFE_MOD_PCM_BASE	0x0330
#define AFE_MOD_PCM_END		0x0338
#define AFE_MOD_PCM_CUR		0x033c
#define AFE_HDMI_OUT_BASE	0x0374
#define AFE_HDMI_OUT_CUR	0x0378
#define AFE_HDMI_OUT_END	0x037c

#define AFE_MEMIF_MSB           0x00cc
#define AFE_MEMIF_MON0          0x00d0
#define AFE_MEMIF_MON1          0x00d4
#define AFE_MEMIF_MON2          0x00d8
#define AFE_MEMIF_MON3          0x00dc

#define AFE_ADDA_DL_SRC2_CON0   0x0108
#define AFE_ADDA_DL_SRC2_CON1   0x010c
#define AFE_ADDA_UL_SRC_CON0    0x0114
#define AFE_ADDA_UL_SRC_CON1    0x0118
#define AFE_ADDA_TOP_CON0	0x0120
#define AFE_ADDA_UL_DL_CON0     0x0124
#define AFE_ADDA_NEWIF_CFG0     0x0138
#define AFE_ADDA_NEWIF_CFG1     0x013c
#define AFE_ADDA_PREDIS_CON0    0x0260
#define AFE_ADDA_PREDIS_CON1    0x0264

#define AFE_SGEN_CON0           0x01f0

#define AFE_HDMI_OUT_CON0	0x0370

#define AFE_IRQ_MCU_CON		0x03a0
#define AFE_IRQ_STATUS		0x03a4
#define AFE_IRQ_CLR		0x03a8
#define AFE_IRQ_CNT1		0x03ac
#define AFE_IRQ_CNT2		0x03b0
#define AFE_IRQ_MCU_EN		0x03b4
#define AFE_IRQ_CNT5		0x03bc
#define AFE_IRQ_CNT7		0x03dc
#define AFE_IRQ1_MCU_CNT_MON    0x03c0
#define AFE_IRQ2_MCU_CNT_MON    0x03c4
#define AFE_IRQ_MCU_CON2	0x03f8

#define AFE_MEMIF_PBUF_SIZE	0x03d8
#define AFE_MEMIF_PBUF2_SIZE	0x03ec

#define AFE_TDM_CON1		0x0548
#define AFE_TDM_CON2		0x054c

#define AFE_MEMIF_MON15		0x0d7c
#define ABB_AFE_SDM_TEST    0x0f4c



#define AFE_IRQ_STATUS_BITS	0xff

/* AUDIO_TOP_CON0 (0x0000) */
#define AUD_TCON0_PDN_DAC_PREDIS	(0x1 << 26)
#define AUD_TCON0_PDN_DAC		(0x1 << 25)
#define AUD_TCON0_PDN_ADC		(0x1 << 24)
#define AUD_TCON0_PDN_SPDF		(0x1 << 21)
#define AUD_TCON0_PDN_HDMI		(0x1 << 20)
#define AUD_TCON0_PDN_APLL_TUNER	(0x1 << 19)
#define AUD_TCON0_PDN_APLL2_TUNER	(0x1 << 18)
#define AUD_TCON0_PDN_24M		(0x1 << 9)
#define AUD_TCON0_PDN_22M		(0x1 << 8)
#define AUD_TCON0_PDN_I2S		(0x1 << 6)
#define AUD_TCON0_PDN_AFE		(0x1 << 2)

/* AFE_I2S_CON (0x0018) */
#define AFE_I2S_CON_PHASE_SHIFT_FIX     (0x1 << 31)
#define AFE_I2S_CON_FROM_IO_MUX	        (0x1 << 28)
#define AFE_I2S_CON_LOW_JITTER_CLK	(0x1 << 12)
#define AFE_I2S_CON_FORMAT_I2S		(0x1 << 3)
#define AFE_I2S_CON_WLEN_32BIT		(0x1 << 1)
#define AFE_I2S_CON_EN			(0x1 << 0)

/* AFE_DAIBT_CON0 (0x001c) */
#define AFE_DAIBT_CON0_USE_MRG_INPUT    (0x1 << 12)
#define AFE_DAIBT_CON0_DATA_DRY         (0x1 << 3)

/* AFE_I2S_CON1 (0x0034) */
#define AFE_I2S_CON1_TDMOUT_MUX		(0x1 << 18)
#define AFE_I2S_CON1_LOW_JITTER_CLK	(0x1 << 12)
#define AFE_I2S_CON1_RATE(x)		(((x) & 0xf) << 8)
#define AFE_I2S_CON1_FORMAT_I2S		(0x1 << 3)
#define AFE_I2S_CON1_WLEN_32BIT		(0x1 << 1)
#define AFE_I2S_CON1_EN			(0x1 << 0)

/* AFE_I2S_CON2 (0x0038) */
#define AFE_I2S_CON2_LOW_JITTER_CLK	(0x1 << 12)
#define AFE_I2S_CON2_RATE(x)		(((x) & 0xf) << 8)
#define AFE_I2S_CON2_FORMAT_I2S		(0x1 << 3)
#define AFE_I2S_CON2_WLEN_32BIT		(0x1 << 1)
#define AFE_I2S_CON2_EN			(0x1 << 0)

/* AFE_I2S_CON3 (0x004C) */
#define AFE_I2S_CON3_LOW_JITTER_CLK	(0x1 << 12)
#define AFE_I2S_CON3_RATE(x)		(((x) & 0xf) << 8)
#define AFE_I2S_CON3_FORMAT_I2S		(0x1 << 3)
#define AFE_I2S_CON3_WLEN_32BIT		(0x1 << 1)
#define AFE_I2S_CON3_EN			(0x1 << 0)

/* AFE_CONN_24BIT (0x006c) */
#define AFE_CONN_24BIT_O10		(0x1 << 10)
#define AFE_CONN_24BIT_O09		(0x1 << 9)
#define AFE_CONN_24BIT_O06		(0x1 << 6)
#define AFE_CONN_24BIT_O05		(0x1 << 5)
#define AFE_CONN_24BIT_O04		(0x1 << 4)
#define AFE_CONN_24BIT_O03		(0x1 << 3)
#define AFE_CONN_24BIT_O02		(0x1 << 2)
#define AFE_CONN_24BIT_O01		(0x1 << 1)

/* AFE_ADDA_DL_SRC2_CON0 (0x0108) */
#define AFE_ADDA_DL_8X_UPSAMPLE    (0x3 << 24)
#define AFE_ADDA_DL_MUTE_OFF       (0x3 << 11)
#define AFE_ADDA_DL_VOICE_DATA     (0x1 << 5)
#define AFE_ADDA_DL_DEGRADE_GAIN   (0x1 << 1)

/* AFE_HDMI_CONN0 (0x0390) */
#define AFE_HDMI_CONN0_O37_I37		(0x7 << 21)
#define AFE_HDMI_CONN0_O36_I36		(0x6 << 18)
#define AFE_HDMI_CONN0_O35_I33		(0x3 << 15)
#define AFE_HDMI_CONN0_O34_I32		(0x2 << 12)
#define AFE_HDMI_CONN0_O33_I35		(0x5 << 9)
#define AFE_HDMI_CONN0_O32_I34		(0x4 << 6)
#define AFE_HDMI_CONN0_O31_I31		(0x1 << 3)
#define AFE_HDMI_CONN0_O30_I30		(0x0 << 0)

/* AFE_TDM_CON1 (0x0548) */
#define AFE_TDM_CON1_LRCK_WIDTH(x)	(((x) - 1) << 24)
#define AFE_TDM_CON1_32_BCK_CYCLES	(0x2 << 12)
#define AFE_TDM_CON1_WLEN_32BIT		(0x2 << 8)
#define AFE_TDM_CON1_MSB_ALIGNED	(0x1 << 4)
#define AFE_TDM_CON1_1_BCK_DELAY	(0x1 << 3)
#define AFE_TDM_CON1_BCK_INV		(0x1 << 1)
#define AFE_TDM_CON1_EN			(0x1 << 0)

#endif
