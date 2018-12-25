/*
 * mt2701-reg.h  --  Mediatek 2701 audio driver reg definition
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

#ifndef _MT2701_REG_H_
#define _MT2701_REG_H_

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <sound/soc.h>
#include "mt2701-afe-common.h"

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
#define AUDIO_TOP_CON0 0x0000U
#define AUDIO_TOP_CON1 0x0004U
#define AUDIO_TOP_CON2 0x0008U
#define AUDIO_TOP_CON4 0x0010U
#define AUDIO_TOP_CON5 0x0014U
#define AFE_DAIBT_CON0 0x001cU
#define AFE_MRGIF_CON 0x003cU
#define ASMI_TIMING_CON1 0x0100U
#define ASMO_TIMING_CON1 0x0104U
#define AFE_SGEN_CON0 0x01f0U
#define PWR1_ASM_CON1 0x0108U
#define ASYS_TOP_CON 0x0600U
#define ASYS_I2SIN1_CON 0x0604U
#define ASYS_I2SIN2_CON 0x0608U
#define ASYS_I2SIN3_CON 0x060cU
#define ASYS_I2SIN4_CON 0x0610U
#define ASYS_I2SIN5_CON 0x0614U
#define ASYS_I2SO1_CON 0x061CU
#define ASYS_I2SO2_CON 0x0620U
#define ASYS_I2SO3_CON 0x0624U
#define ASYS_I2SO4_CON 0x0628U
#define ASYS_I2SO5_CON 0x062cU
#define AFE_PCM_INTF_CON1 0x063CU
#define AFE_PCM_INTF_CON2 0x0640U
#define PWR2_TOP_CON 0x0634U
#define AFE_CONN0 0x06c0U
#define AFE_CONN1 0x06c4U
#define AFE_CONN2 0x06c8U
#define AFE_CONN3 0x06ccU
#define AFE_CONN4 0x06d0U
#define AFE_CONN5 0x06d4U
#define AFE_CONN10 0x06e8U
#define AFE_CONN11 0x06ecU
#define AFE_CONN14 0x06f8U
#define AFE_CONN15 0x06fcU
#define AFE_CONN16 0x0700U
#define AFE_CONN17 0x0704U
#define AFE_CONN18 0x0708U
#define AFE_CONN19 0x070cU
#define AFE_CONN20 0x0710U
#define AFE_CONN21 0x0714U
#define AFE_CONN22 0x0718U
#define AFE_CONN23 0x071cU
#define AFE_CONN24 0x0720U
#define AFE_CONN25 0x0724U
#define AFE_CONN26 0x0728U
#define AFE_CONN32 0x0740U
#define AFE_CONN41 0x0764U
#define ASYS_IRQ1_CON 0x0780U
#define ASYS_IRQ2_CON 0x0784U
#define ASYS_IRQ3_CON 0x0788U
#define ASYS_IRQ4_CON 0x078cU
#define ASYS_IRQ5_CON 0x0790U
#define ASYS_IRQ6_CON 0x0794U
#define ASYS_IRQ_CLR 0x07c0U
#define ASYS_IRQ_STATUS 0x07c4U
#define PWR2_ASM_CON1 0x1070U
#define AFE_DAC_CON0 0x1200U
#define AFE_DAC_CON1 0x1204U
#define AFE_DAC_CON2 0x1208U
#define AFE_DAC_CON3 0x120cU
#define AFE_DAC_CON4 0x1210U
#define AFE_MEMIF_HD_CON1 0x121cU
#define AFE_MEMIF_PBUF_SIZE 0x1238U
#define AFE_MEMIF_HD_CON0 0x123cU
#define AFE_DL1_BASE 0x1240U
#define AFE_DL1_CUR 0x1244U
#define AFE_DL2_BASE 0x1250U
#define AFE_DL2_CUR 0x1254U
#define AFE_DL3_BASE 0x1260U
#define AFE_DL3_CUR 0x1264U
#define AFE_DL4_BASE 0x1270U
#define AFE_DL4_CUR 0x1274U
#define AFE_DL5_BASE 0x1280U
#define AFE_DL5_CUR 0x1284U
#define AFE_DLMCH_BASE 0x12a0U
#define AFE_DLMCH_CUR 0x12a4U
#define AFE_ARB1_BASE 0x12b0U
#define AFE_ARB1_CUR 0x12b4U
#define AFE_VUL_BASE 0x1300U
#define AFE_VUL_CUR 0x130cU
#define AFE_UL2_BASE 0x1310U
#define AFE_UL2_END 0x1318U
#define AFE_UL2_CUR 0x131cU
#define AFE_UL3_BASE 0x1320U
#define AFE_UL3_END 0x1328U
#define AFE_UL3_CUR 0x132cU
#define AFE_UL4_BASE 0x1330U
#define AFE_UL4_END 0x1338U
#define AFE_UL4_CUR 0x133cU
#define AFE_UL5_BASE 0x1340U
#define AFE_UL5_END 0x1348U
#define AFE_UL5_CUR 0x134cU
#define AFE_AWB2_BASE 0x12e0U
#define AFE_AWB2_END 0x12e8U
#define AFE_AWB2_CUR 0x12ecU
#define AFE_DAI_BASE 0x1370U
#define AFE_DAI_CUR 0x137cU
#define AFE_PCMI_BASE 0x1330U
#define AFE_PCMI_CUR 0x1334U
#define AFE_PCMO_BASE 0x12c0U
#define AFE_PCMO_CUR 0x12c4U
#define AFE_MEMIF_BASE_MSB 0x0304U
#define AFE_MEMIF_END_MSB 0x0308U
#define AFE_TDM_G1_CON1 0x0290U
#define AFE_TDM_G1_CON2 0x0294U
#define AFE_TDM_G1_CONN_CON0 0x029cU
#define AFE_TDM_G1_CONN_CON1 0x0298U
#define AFE_TDM_G1_BASE 0x1280U
#define AFE_TDM_G1_CUR 0x1284U
#define AFE_TDM_G1_END 0x1288U
#define AFE_TDM_G2_CON1 0x02A0U
#define AFE_TDM_G2_CON2 0x02A4U
#define AFE_TDM_G2_CONN_CON0 0x02AcU
#define AFE_TDM_G2_CONN_CON1 0x02A8U
#define AFE_TDM_G2_BASE 0x1290U
#define AFE_TDM_G2_CUR 0x1294U
#define AFE_TDM_G2_END 0x1298U
#define AFE_TDM_IN_CON1 0x02B8U
#define AFE_TDM_IN_CON2 0x02BCU
#define AFE_TDM_IN_BASE 0x1360U
#define AFE_TDM_IN_CUR 0x136cU
#define AFE_TDM_IN_END 0x1368U
#define AFE_TDM_AGENT_CFG 0x02c0U
#define AFE_MCH_OUT_CFG 0x0300U
#define AFE_ASRC_PCMO_CON0   0x0ac0U
#define AFE_ASRC_PCMO_CON1   0x0ac4U
#define AFE_ASRC_PCMO_CON2   0x0ac8U
#define AFE_ASRC_PCMO_CON3   0x0accU
#define AFE_ASRC_PCMO_CON4   0x0ad0U
#define AFE_ASRC_PCMO_CON5   0x0ad4U
#define AFE_ASRC_PCMO_CON6   0x0ad8U
#define AFE_ASRC_PCMO_CON7   0x0adcU
#define AFE_ASRC_PCMO_CON10  0x0ae8U
#define AFE_ASRC_PCMO_CON11  0x0aecU
#define AFE_ASRC_PCMO_CON13  0x0af4U
#define AFE_ASRC_PCMO_CON14  0x0af8U
#define AFE_ASRC_PCMI_CON0   0x0a80U
#define AFE_ASRC_PCMI_CON1   0x0a84U
#define AFE_ASRC_PCMI_CON2   0x0a88U
#define AFE_ASRC_PCMI_CON3   0x0a8cU
#define AFE_ASRC_PCMI_CON4   0x0a90U
#define AFE_ASRC_PCMI_CON5   0x0a94U
#define AFE_ASRC_PCMI_CON6   0x0a98U
#define AFE_ASRC_PCMI_CON7   0x0a9cU
#define AFE_ASRC_PCMI_CON10  0x0aa8U
#define AFE_ASRC_PCMI_CON11  0x0aacU
#define AFE_ASRC_PCMI_CON13  0x0ab4U
#define AFE_ASRC_PCMI_CON14  0x0ab8U


#define FPGA_CFG0 0x04e0U
#define FPGA_CFG1 0x04e4U
#define FPGA_CFG2 0x04e8U
#define FPGA_CFG3 0x04ecU


/* AUDIO_TOP_CON0 (0x0000) */
#define AUDIO_TOP_CON0_A1SYS_A2SYS_ON	(0x3U << 0U)
#define AUDIO_TOP_CON0_PDN_AFE		(0x1U << 2U)
#define AUDIO_TOP_CON0_PDN_APLL_CK	(0x1U << 23U)

/* AUDIO_TOP_CON1 (0x0004) */
#define AUDIO_TOP_CON1_TDMIN_CLK_AGENT_PDN     (0x1U<<15U)
#define AUDIO_TOP_CON1_TDMIN_PLL_SEL           (0x1U<<16U)
#define AUDIO_TOP_CON1_TDMIN_PLL_SEL_APLL1     (0x0U<<16U)
#define AUDIO_TOP_CON1_TDMIN_PLL_SEL_APLL2     (0x1U<<16U)
#define AUDIO_TOP_CON1_TDMIN_BCK_EN           (0x1U<<17U)
#define AUDIO_TOP_CON1_TDMIN_BCK_INV           (0x1U<<18U)
/* tdmin_bck = clock source * (1 / x+1)*/
#define AUDIO_TOP_CON1_TDMIN_BCK_PLL_DIV       (0xffU<<24U)
#define AUDIO_TOP_CON1_TDMIN_BCK_PLL_DIV_SET(x)       ((x)<<24U)

/* AUDIO_TOP_CON4 (0x0010) */
#define AUDIO_TOP_CON4_I2SO1_PWN	(0x1U << 6U)
#define AUDIO_TOP_CON4_PDN_ASRC_PCMI	(0x1U << 16U)
#define AUDIO_TOP_CON4_PDN_ASRC_PCMO	(0x1U << 17U)
#define AUDIO_TOP_CON4_PDN_A1SYS	(0x1U << 21U)
#define AUDIO_TOP_CON4_PDN_A2SYS	(0x1U << 22U)
#define AUDIO_TOP_CON4_PDN_AFE_CONN	(0x1U << 23U)
#define AUDIO_TOP_CON4_PDN_PCM          (0x1U << 24U)
#define AUDIO_TOP_CON4_PDN_MRGIF	(0x1U << 25U)


/* AFE_DAIBT_CON0 (0x001c) */
#define AFE_DAIBT_CON0_DAIBT_EN		(0x1U << 0U)
#define AFE_DAIBT_CON0_BT_FUNC_EN	(0x1U << 1U)
#define AFE_DAIBT_CON0_BT_FUNC_RDY	(0x1U << 3U)
#define AFE_DAIBT_CON0_BT_WIDE_MODE_EN	(0x1U << 9U)
#define AFE_DAIBT_CON0_MRG_USE		(0x1U << 12U)

/* PWR1_ASM_CON1 (0x0108) */
#define PWR1_ASM_CON1_INIT_VAL		(0x492U)

/* AFE_MRGIF_CON (0x003c) */
#define AFE_MRGIF_CON_MRG_EN		(0x1U << 0U)
#define AFE_MRGIF_CON_MRG_I2S_EN	(0x1U << 16U)
#define AFE_MRGIF_CON_I2S_MODE_MASK	(0xfU << 20U)
#define AFE_MRGIF_CON_I2S_MODE_32K	(0x4U << 20U)

/* ASYS_I2SO1_CON (0x061c) */
#define ASYS_I2SO1_CON_FS		(0x1fU << 8U)
#define ASYS_I2SO1_CON_FS_SET(x)	((x) << 8U)
#define ASYS_I2SO1_CON_MULTI_CH		(0x1U << 16U)
#define ASYS_I2SO1_CON_SIDEGEN		(0x1U << 30U)
#define ASYS_I2SO1_CON_I2S_EN		(0x1U << 0U)
/* 0:EIAJ 1:I2S */
#define ASYS_I2SO1_CON_I2S_MODE		(0x1U << 3U)
#define ASYS_I2SO1_CON_WIDE_MODE	(0x1U << 1U)
#define ASYS_I2SO1_CON_WIDE_MODE_SET(x)	((x) << 1U)

/* PWR2_TOP_CON (0x0634) */
#define PWR2_TOP_CON_INIT_VAL		(0xffe1ffffU)

/* ASYS_IRQ_CLR (0x07c0) */
#define ASYS_IRQ_CLR_ALL		(0xffffffffU)

/* PWR2_ASM_CON1 (0x1070) */
#define PWR2_ASM_CON1_INIT_VAL		(0x492492U)

/* AFE_DAC_CON0 (0x1200) */
#define AFE_DAC_CON0_AFE_ON		(0x1U << 0U)

/* AFE_MEMIF_PBUF_SIZE (0x1238) */
#define AFE_MEMIF_PBUF_SIZE_DLM_MASK		(0x1U << 29U)
#define AFE_MEMIF_PBUF_SIZE_PAIR_INTERLEAVE	(0x0U << 29U)
#define AFE_MEMIF_PBUF_SIZE_FULL_INTERLEAVE	(0x1U << 29U)
#define DLMCH_BIT_WIDTH_MASK			(0x1U << 28U)
#define AFE_MEMIF_PBUF_SIZE_DLM_CH_MASK		(0xfU << 24U)
#define AFE_MEMIF_PBUF_SIZE_DLM_CH(x)		((x) << 24U)
#define AFE_MEMIF_PBUF_SIZE_DLM_BYTE_MASK	(0x3U << 12U)
#define AFE_MEMIF_PBUF_SIZE_DLM_32BYTES		(0x1U << 12U)

/* I2S in/out register bit control */
#define ASYS_I2S_CON_FS			(0x1fU << 8U)
#define ASYS_I2S_CON_FS_SET(x)		((x) << 8U)
#define ASYS_I2S_CON_RESET		(0x1U << 30U)
#define ASYS_I2S_CON_I2S_EN		(0x1U << 0U)
#define ASYS_I2S_CON_I2S_COUPLE_MODE	(0x1U << 17U)
/* 0:EIAJ 1:I2S */
#define ASYS_I2S_CON_I2S_MODE		(0x1U << 3U)
#define ASYS_I2S_CON_I2S_MODE_SET(x)	((x) << 3U)
#define ASYS_I2S_CON_RIGHT_J		(0x1U << 14U)
#define ASYS_I2S_CON_RIGHT_J_SET(x)	((x) << 14U)
#define ASYS_I2S_CON_WIDE_MODE		(0x1U << 1U)
#define ASYS_I2S_CON_WIDE_MODE_SET(x)	((x) << 1U)
#define ASYS_I2S_IN_PHASE_FIX		(0x1U << 31U)
#define ASYS_I2S_CON_INV_LRCK		(0x1U << 5U)
#define ASYS_I2S_CON_INV_LRCK_SET(x)	((x) << 5U)
#define ASYS_I2S_CON_INV_BCK		(0x1U << 6U)
#define ASYS_I2S_CON_INV_BCK_SET(x)	((x) << 6U)

/* TDM in/out register bit control */
#define AFE_TDM_CON_LRCK_WIDTH              (0x1ffU << 23U)
#define AFE_TDM_CON_LRCK_WIDTH_SET(x)       ((x) << 23U)
#define AFE_TDM_CON_INOUT_SYNC     (0x1U << 18U)
#define AFE_TDM_CON_INOUT_SYNC_SET(x)   ((x) << 18U)
#define AFE_TDM_CON_IN_BCK        (0x1U << 19U)
#define AFE_TDM_CON_IN_BCK_SET(x)  ((x) << 19U)
#define AFE_TDM_CON_CH              (0x7U << 12U)
#define AFE_TDM_CON_CH_SET(x)       ((x) << 12U)
#define AFE_TDM_CON_WLEN           (0x3U << 8U)
#define AFE_TDM_CON_WLEN_SET(x)    ((x) << 8U)
#define AFE_TDM_CON_LRCK_DELAY             (0x1U << 5U)
#define AFE_TDM_CON_LRCK_DELAY_SET(x)    ((x) << 5U)
#define AFE_TDM_CON_LEFT_ALIGN             (0x1U << 4U)
#define AFE_TDM_CON_LEFT_ALIGN_SET(x)    ((x) << 4U)
#define AFE_TDM_CON_DELAY             (0x1U << 3U)
#define AFE_TDM_CON_DELAY_SET(x)    ((x) << 3U)
#define AFE_TDM_CON_INV_LRCK             (0x1U << 2U)
#define AFE_TDM_CON_INV_LRCK_SET(x)    ((x) << 2U)
#define AFE_TDM_CON_INV_BCK             (0x1U << 1U)
#define AFE_TDM_CON_INV_BCK_SET(x)    ((x) << 1U)
#define AFE_TDM_CON_EN             (0x1U << 0U)

/* TDM in channel mask */
#define AFE_TDM_IN_DISABLE_OUT         (0xffU)
#define AFE_TDM_IN_DISABLE_OUT_SET(x)  (x)
#define AFE_TDM_IN_MASK_ODD            (0xffU << 8U)
#define AFE_TDM_IN_MASK_ODD_SET(x)     ((x)<<8U)

/* FPGA0*/
#define DAC_2CH_MCLK_DIVIDER_MASK    (0xFFU<<8U)
#define DAC_2CH_MLK_DIVIDER_POS              8U
#define DAC_2CH_MCLK_CLK_DOMAIN_MASK (0x1U<<16U)
#define DAC_2CH_MCLK_CLK_DOMAIN_48   (0x0U<<16U)
#define DAC_2CH_MCLK_CLK_DOMAIN_44   (0x1U<<16U)
#define XCLK_DIVIDER_MASK            (0xFFU<<24U)
#define XCLK_DIVIDER_POS                    24U

/* PCM_INTF */
#define AFE_PCM_EX_MODEM          (0x1U << 17U)
#define AFE_PCM_EX_MODEM_SET(x)       ((x) << 17U)
#define AFE_PCM_24BIT          (0x1U << 16U)
#define AFE_PCM_24BIT_SET(x)       ((x) << 16U)
#define AFE_PCM_WLEN           (0x3U << 14U)
#define AFE_PCM_WLEN_SET(x)       ((x) << 14U)
#define AFE_PCM_BYP_ASRC           (0x1U << 6U)
#define AFE_PCM_BYP_ASRC_SET(x)       ((x) << 6U)
#define AFE_PCM_SLAVE              (0x1U << 5U)
#define AFE_PCM_SLAVE_SET(x)       ((x) << 5U)
#define AFE_PCM_MODE              (0x3U << 3U)
#define AFE_PCM_MODE_SET(x)       ((x) << 3U)
#define AFE_PCM_FMT              (0x3U << 1U)
#define AFE_PCM_FMT_SET(x)       ((x) << 1U)
#define AFE_PCM_EN              (0x1U << 0U)
#define AFE_PCM_EN_SET(x)       ((x) << 0U)

/* AFE_ASRC_PCMO_CON */
#define AFE_PCM_ASRC_O16BIT          (0x1U << 19U)
#define AFE_PCM_ASRC_O16BIT_SET(x)       ((x) << 19U)
#define AFE_PCM_ASRC_MONO          (0x1U << 16U)
#define AFE_PCM_ASRC_MONO_SET(x)       ((x) << 16U)
#define AFE_PCM_ASRC_OFS          (0x3U << 14U)
#define AFE_PCM_ASRC_OFS_SET(x)       ((x) << 14U)
#define AFE_PCM_ASRC_IFS          (0x3U << 12U)
#define AFE_PCM_ASRC_IFS_SET(x)       ((x) << 12U)
#define AFE_PCM_ASRC_IIR          (0x1U << 11U)
#define AFE_PCM_ASRC_IIR_SET(x)       ((x) << 11U)
#define AFE_PCM_ASRC_PALETTE          (0xffffffU)
#define AFE_PCM_ASRC_PALETTE_SET(x)       ((x))
#define AFE_PCM_ASRC_TH          (0xffffffU)
#define AFE_PCM_ASRC_TH_SET(x)       ((x))
#define AFE_PCM_ASRC_CLR         (0x1U << 4U)
#define AFE_PCM_ASRC_EN         (0x1U << 0U)
#define AFE_PCM_ASRC_EN_SET(x)       ((x))

#define AFE_END_ADDR 0x15e0U
#endif
