/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudioAfe.h
 *
 * Project:
 * --------
 *   MT6739  Audio Driver Afe Register setting
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 *   Ir Lian (mtk00976)
 *   Harvey Huang (mtk03996)
 *   Chipeng Chang (mtk02308)
 *
 *------------------------------------------------------------------------------
 *
 *
 ******************************************************************************
 */

#ifndef _AUDDRV_AFE_H_
#define _AUDDRV_AFE_H_

#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"
#include "mtk-soc-digital-type.h"
#include <linux/types.h>

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

enum audio_sdm_level {
	AUDIO_SDM_LEVEL_MUTE = 0,
	AUDIO_SDM_LEVEL_NORMAL = 0x1d,
};
/*****************************************************************************
 *                          C O N S T A N T S
 *****************************************************************************/
#define AUDIO_HW_PHYSICAL_BASE  (0x11220000L)
#define AUDIO_CLKCFG_PHYSICAL_BASE  (0x10000000L)
/* need enable this register before access all register */
#define AUDIO_POWER_TOP (0x1000629cL)
#define AUDIO_INFRA_BASE (0x10001000L)
#define AUDIO_HW_VIRTUAL_BASE (0xF1220000L)
#define APMIXEDSYS_BASE (0x1000C000L)
/* Register TOP */
/* We need to write AP_PLL_CON5 to set mux,
 * thus we can set APLL Tuner in AFE setting
 */
#define AP_PLL_CON5 (0x0014)

#ifdef AUDIO_MEM_IOREMAP
#define AFE_BASE (0L)
#else
#define AFE_BASE (AUDIO_HW_VIRTUAL_BASE)
#endif

/* Internal sram */
#define AFE_INTERNAL_SRAM_PHY_BASE  (0x11221000L)
#define AFE_INTERNAL_SRAM_VIR_BASE (AUDIO_HW_VIRTUAL_BASE - 0x70000 + 0x8000)
#define AFE_INTERNAL_SRAM_NORMAL_SIZE (0x6c00) /* 27k, for normal mode */
#define AFE_INTERNAL_SRAM_COMPACT_SIZE (0x9000) /* 36k, for compact mode */
#define AFE_INTERNAL_SRAM_SIZE AFE_INTERNAL_SRAM_COMPACT_SIZE


/* APLL clock base */
#define APLL_44K_BASE (180633600)
#define APLL_48K_BASE (196608000)
/*****************************************************************************
 *                         M A C R O
 *****************************************************************************/

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/

#define AUDIO_CLK_CFG_4 (0x0080)
#define AUDIO_CLK_CFG_6 (0x00A0)
#define AUDIO_CLK_CFG_7 (0x00B0)
#define AUDIO_CLK_CFG_8 (0x00C0)
#define AUDIO_CG_SET (0x88)
#define AUDIO_CG_CLR (0x8c)
#define AUDIO_CG_STATUS (0x94)

/* apmixed sys */
#define APLL1_CON0 0x02a0
#define APLL1_CON1 0x02a4
#define APLL1_CON2 0x02a8
#define APLL1_CON3 0x02ac

#define APLL2_CON0 0x02b4
#define APLL2_CON1 0x02b8
#define APLL2_CON2 0x02bc
#define APLL2_CON3 0x02c0

/* move to clksys 10210000*/
#define CLK_AUDDIV_0 (0x0320)
#define CLK_AUDDIV_1 (0x0324)
#define CLK_AUDDIV_2 (0x0328)

/*Afe register*/
#define AUDIO_TOP_CON0 (AFE_BASE + 0x0000)
#define AUDIO_TOP_CON1 (AFE_BASE + 0x0004)
#define AUDIO_TOP_CON3 (AFE_BASE + 0x000c)
#define AFE_DAC_CON0 (AFE_BASE + 0x0010)
#define AFE_DAC_CON1 (AFE_BASE + 0x0014)
#define AFE_I2S_CON (AFE_BASE + 0x0018)
#define AFE_DAIBT_CON0 (AFE_BASE + 0x001c)
#define AFE_CONN0 (AFE_BASE + 0x0020)
#define AFE_CONN1 (AFE_BASE + 0x0024)
#define AFE_CONN2 (AFE_BASE + 0x0028)
#define AFE_CONN3 (AFE_BASE + 0x002c)
#define AFE_CONN4 (AFE_BASE + 0x0030)
#define AFE_I2S_CON1 (AFE_BASE + 0x0034)
#define AFE_I2S_CON2 (AFE_BASE + 0x0038)
#define AFE_MRGIF_CON (AFE_BASE + 0x003c)
#define AFE_DL1_BASE (AFE_BASE + 0x0040)
#define AFE_DL1_CUR (AFE_BASE + 0x0044)
#define AFE_DL1_END (AFE_BASE + 0x0048)
#define AFE_I2S_CON3 (AFE_BASE + 0x004c)
#define AFE_DL2_BASE (AFE_BASE + 0x0050)
#define AFE_DL2_CUR (AFE_BASE + 0x0054)
#define AFE_DL2_END (AFE_BASE + 0x0058)
#define AFE_CONN5 (AFE_BASE + 0x005c)
#define AFE_CONN_24BIT (AFE_BASE + 0x006c)
#define AFE_AWB_BASE (AFE_BASE + 0x0070)
#define AFE_AWB_END (AFE_BASE + 0x0078)
#define AFE_AWB_CUR (AFE_BASE + 0x007c)
#define AFE_VUL_BASE (AFE_BASE + 0x0080)
#define AFE_VUL_END (AFE_BASE + 0x0088)
#define AFE_VUL_CUR (AFE_BASE + 0x008c)
#define AFE_DAI_BASE (AFE_BASE + 0x0090)
#define AFE_DAI_END (AFE_BASE + 0x0098)
#define AFE_DAI_CUR (AFE_BASE + 0x009c)
#define AFE_CONN6 (AFE_BASE + 0x00bc)
#define AFE_MEMIF_MSB (AFE_BASE + 0x00cc)
#define AFE_MEMIF_MON0 (AFE_BASE + 0x00d0)
#define AFE_MEMIF_MON1 (AFE_BASE + 0x00d4)
#define AFE_MEMIF_MON2 (AFE_BASE + 0x00d8)
#define AFE_MEMIF_MON3 (AFE_BASE + 0x00dc)
#define AFE_MEMIF_MON4 (AFE_BASE + 0x00e0)
#define AFE_MEMIF_MON5 (AFE_BASE + 0x00e4)
#define AFE_MEMIF_MON6 (AFE_BASE + 0x00e8)
#define AFE_MEMIF_MON7 (AFE_BASE + 0x00ec)
#define AFE_MEMIF_MON8 (AFE_BASE + 0x00f0)
#define AFE_MEMIF_MON9 (AFE_BASE + 0x00f4)
#define AFE_ADDA_DL_SRC2_CON0 (AFE_BASE + 0x0108)
#define AFE_ADDA_DL_SRC2_CON1 (AFE_BASE + 0x010c)
#define AFE_ADDA_UL_SRC_CON0 (AFE_BASE + 0x0114)
#define AFE_ADDA_UL_SRC_CON1 (AFE_BASE + 0x0118)
#define AFE_ADDA_TOP_CON0 (AFE_BASE + 0x0120)
#define AFE_ADDA_UL_DL_CON0 (AFE_BASE + 0x0124)
#define AFE_ADDA_SRC_DEBUG (AFE_BASE + 0x012c)
#define AFE_ADDA_SRC_DEBUG_MON0 (AFE_BASE + 0x0130)
#define AFE_ADDA_SRC_DEBUG_MON1 (AFE_BASE + 0x0134)
#define AFE_ADDA_UL_SRC_MON0       (AFE_BASE + 0x0148)
#define AFE_ADDA_UL_SRC_MON1       (AFE_BASE + 0x014c)
#define AFE_SIDETONE_DEBUG (AFE_BASE + 0x01d0)
#define AFE_SIDETONE_MON (AFE_BASE + 0x01d4)
#define AFE_SGEN_CON2 (AFE_BASE + 0x01dc)
#define AFE_SIDETONE_CON0 (AFE_BASE + 0x01e0)
#define AFE_SIDETONE_COEFF (AFE_BASE + 0x01e4)
#define AFE_SIDETONE_CON1 (AFE_BASE + 0x01e8)
#define AFE_SIDETONE_GAIN (AFE_BASE + 0x01ec)
#define AFE_SGEN_CON0 (AFE_BASE + 0x01f0)
#define AFE_TOP_CON0 (AFE_BASE + 0x0200)
#define AFE_BUS_CFG (AFE_BASE + 0x0240)
#define AFE_BUS_MON0 (AFE_BASE + 0x0244)
#define AFE_ADDA_PREDIS_CON0 (AFE_BASE + 0x0260)
#define AFE_ADDA_PREDIS_CON1 (AFE_BASE + 0x0264)
#define AFE_MRGIF_MON0 (AFE_BASE + 0x0270)
#define AFE_MRGIF_MON1 (AFE_BASE + 0x0274)
#define AFE_MRGIF_MON2 (AFE_BASE + 0x0278)
#define AFE_I2S_MON (AFE_BASE + 0x027c)
#define AFE_DAC_CON2 (AFE_BASE + 0x02e0)
#define AFE_IRQ_MCU_CON1 (AFE_BASE + 0x02e4)
#define AFE_IRQ_MCU_CON2 (AFE_BASE + 0x02e8)
#define AFE_DAC_MON (AFE_BASE + 0x02ec)
#define AFE_VUL2_BASE (AFE_BASE + 0x02f0)
#define AFE_VUL2_END (AFE_BASE + 0x02f8)
#define AFE_VUL2_CUR (AFE_BASE + 0x02fc)
#define AFE_IRQ_MCU_CNT0 (AFE_BASE + 0x0300)
#define AFE_IRQ_MCU_CNT6 (AFE_BASE + 0x0304)
#define AFE_IRQ0_MCU_CNT_MON (AFE_BASE + 0x0310)
#define AFE_IRQ6_MCU_CNT_MON (AFE_BASE + 0x0314)
#define AFE_MOD_DAI_BASE (AFE_BASE + 0x0330)
#define AFE_MOD_DAI_END (AFE_BASE + 0x0338)
#define AFE_MOD_DAI_CUR (AFE_BASE + 0x033c)
#define AFE_IRQ3_MCU_CNT_MON (AFE_BASE + 0x0398)
#define AFE_IRQ4_MCU_CNT_MON (AFE_BASE + 0x039c)
#define AFE_IRQ_MCU_CON0 (AFE_BASE + 0x03a0)
#define AFE_IRQ_MCU_STATUS (AFE_BASE + 0x03a4)
#define AFE_IRQ_MCU_CLR (AFE_BASE + 0x03a8)
#define AFE_IRQ_MCU_CNT1 (AFE_BASE + 0x03ac)
#define AFE_IRQ_MCU_CNT2 (AFE_BASE + 0x03b0)
#define AFE_IRQ_MCU_EN (AFE_BASE + 0x03b4)
#define AFE_IRQ_MCU_MON2 (AFE_BASE + 0x03b8)
#define AFE_IRQ_MCU_CNT5 (AFE_BASE + 0x03bc)
#define AFE_IRQ1_MCU_CNT_MON (AFE_BASE + 0x03c0)
#define AFE_IRQ2_MCU_CNT_MON (AFE_BASE + 0x03c4)
#define AFE_IRQ1_MCU_EN_CNT_MON (AFE_BASE + 0x03c8)
#define AFE_IRQ5_MCU_CNT_MON (AFE_BASE + 0x03cc)
#define AFE_MEMIF_MINLEN (AFE_BASE + 0x03d0)
#define AFE_MEMIF_MAXLEN (AFE_BASE + 0x03d4)
#define AFE_MEMIF_PBUF_SIZE (AFE_BASE + 0x03d8)
#define AFE_IRQ_MCU_CNT7 (AFE_BASE + 0x03dc)
#define AFE_IRQ7_MCU_CNT_MON (AFE_BASE + 0x03e0)
#define AFE_IRQ_MCU_CNT3 (AFE_BASE + 0x03e4)
#define AFE_IRQ_MCU_CNT4 (AFE_BASE + 0x03e8)
#define AFE_IRQ_MCU_CNT11 (AFE_BASE + 0x03ec)
#define AFE_APLL1_TUNER_CFG (AFE_BASE + 0x03f0)
#define AFE_APLL2_TUNER_CFG (AFE_BASE + 0x03f0)
#define AFE_MEMIF_HD_MODE (AFE_BASE + 0x03f8)
#define AFE_MEMIF_HDALIGN (AFE_BASE + 0x03fc)
#define AFE_CONN33 (AFE_BASE + 0x0408)
#define AFE_IRQ_MCU_CNT12 (AFE_BASE + 0x040c)
#define AFE_GAIN1_CON0 (AFE_BASE + 0x0410)
#define AFE_GAIN1_CON1 (AFE_BASE + 0x0414)
#define AFE_GAIN1_CON2 (AFE_BASE + 0x0418)
#define AFE_GAIN1_CON3 (AFE_BASE + 0x041c)
#define AFE_CONN7 (AFE_BASE + 0x0420)
#define AFE_GAIN1_CUR (AFE_BASE + 0x0424)
#define AFE_GAIN2_CON0 (AFE_BASE + 0x0428)
#define AFE_GAIN2_CON1 (AFE_BASE + 0x042c)
#define AFE_GAIN2_CON2 (AFE_BASE + 0x0430)
#define AFE_GAIN2_CON3 (AFE_BASE + 0x0434)
#define AFE_CONN8 (AFE_BASE + 0x0438)
#define AFE_GAIN2_CUR (AFE_BASE + 0x043c)
#define AFE_CONN9 (AFE_BASE + 0x0440)
#define AFE_CONN10 (AFE_BASE + 0x0444)
#define AFE_CONN11 (AFE_BASE + 0x0448)
#define AFE_CONN12 (AFE_BASE + 0x044c)
#define AFE_CONN13 (AFE_BASE + 0x0450)
#define AFE_CONN14 (AFE_BASE + 0x0454)
#define AFE_CONN15 (AFE_BASE + 0x0458)
#define AFE_CONN16 (AFE_BASE + 0x045c)
#define AFE_CONN17 (AFE_BASE + 0x0460)
#define AFE_CONN18 (AFE_BASE + 0x0464)
#define AFE_CONN21 (AFE_BASE + 0x0470)
#define AFE_CONN22 (AFE_BASE + 0x0474)
#define AFE_CONN23 (AFE_BASE + 0x0478)
#define AFE_CONN24 (AFE_BASE + 0x047c)
#define AFE_CONN_RS (AFE_BASE + 0x0494)
#define AFE_CONN_DI (AFE_BASE + 0x0498)
#define AFE_CONN25 (AFE_BASE + 0x04b0)
#define AFE_CONN26 (AFE_BASE + 0x04b4)
#define AFE_CONN27 (AFE_BASE + 0x04b8)
#define AFE_CONN28 (AFE_BASE + 0x04bc)
#define AFE_CONN29 (AFE_BASE + 0x04c0)
#define AFE_CONN30 (AFE_BASE + 0x04c4)
#define AFE_CONN31 (AFE_BASE + 0x04c8)
#define AFE_CONN32 (AFE_BASE + 0x04cc)
#define AFE_SRAM_DELSEL_CON0 (AFE_BASE + 0x04f0)
#define AFE_SRAM_DELSEL_CON2 (AFE_BASE + 0x04f8)
#define AFE_SRAM_DELSEL_CON3 (AFE_BASE + 0x04fc)
#define AFE_ASRC_2CH_CON12 (AFE_BASE + 0x0528)
#define AFE_ASRC_2CH_CON13 (AFE_BASE + 0x052c)
#define PCM_INTF_CON1 (AFE_BASE + 0x0530)
#define PCM_INTF_CON2 (AFE_BASE + 0x0538)
#define PCM2_INTF_CON (AFE_BASE + 0x053c)
#define AFE_CONN34 (AFE_BASE + 0x0580)
#define FPGA_CFG0 (AFE_BASE + 0x05b0)
#define FPGA_CFG1 (AFE_BASE + 0x05b4)
#define FPGA_CFG2 (AFE_BASE + 0x05c0)
#define FPGA_CFG3 (AFE_BASE + 0x05c4)
#define AFE_IRQ8_MCU_CNT_MON (AFE_BASE + 0x05e4)
#define AFE_IRQ11_MCU_CNT_MON (AFE_BASE + 0x05e8)
#define AFE_IRQ12_MCU_CNT_MON (AFE_BASE + 0x05ec)
#define AFE_GENERAL_REG0 (AFE_BASE + 0x0800)
#define AFE_GENERAL_REG1 (AFE_BASE + 0x0804)
#define AFE_GENERAL_REG2 (AFE_BASE + 0x0808)
#define AFE_GENERAL_REG3 (AFE_BASE + 0x080c)
#define AFE_GENERAL_REG4 (AFE_BASE + 0x0810)
#define AFE_GENERAL_REG5 (AFE_BASE + 0x0814)
#define AFE_GENERAL_REG6 (AFE_BASE + 0x0818)
#define AFE_GENERAL_REG7 (AFE_BASE + 0x081c)
#define AFE_GENERAL_REG8 (AFE_BASE + 0x0820)
#define AFE_GENERAL_REG9 (AFE_BASE + 0x0824)
#define AFE_GENERAL_REG10 (AFE_BASE + 0x0828)
#define AFE_GENERAL_REG11 (AFE_BASE + 0x082c)
#define AFE_GENERAL_REG12 (AFE_BASE + 0x0830)
#define AFE_GENERAL_REG13 (AFE_BASE + 0x0834)
#define AFE_GENERAL_REG14 (AFE_BASE + 0x0838)
#define AFE_GENERAL_REG15 (AFE_BASE + 0x083c)
#define AFE_CBIP_CFG0 (AFE_BASE + 0x0840)
#define AFE_CBIP_MON0 (AFE_BASE + 0x0844)
#define AFE_CBIP_SLV_MUX_MON0 (AFE_BASE + 0x0848)
#define AFE_CBIP_SLV_DECODER_MON0 (AFE_BASE + 0x084c)
#define AFE_DAI2_CUR (AFE_BASE + 0x08bc)
#define AFE_DAI2_CUR_MSB (AFE_BASE + 0x08cc)
#define AFE_CONN0_1 (AFE_BASE + 0x0900)
#define AFE_CONN1_1 (AFE_BASE + 0x0904)
#define AFE_CONN2_1 (AFE_BASE + 0x0908)
#define AFE_CONN3_1 (AFE_BASE + 0x090c)
#define AFE_CONN4_1 (AFE_BASE + 0x0910)
#define AFE_CONN5_1 (AFE_BASE + 0x0914)
#define AFE_CONN6_1 (AFE_BASE + 0x0918)
#define AFE_CONN7_1 (AFE_BASE + 0x091c)
#define AFE_CONN8_1 (AFE_BASE + 0x0920)
#define AFE_CONN9_1 (AFE_BASE + 0x0924)
#define AFE_CONN10_1 (AFE_BASE + 0x0928)
#define AFE_CONN11_1 (AFE_BASE + 0x092c)
#define AFE_CONN12_1 (AFE_BASE + 0x0930)
#define AFE_CONN13_1 (AFE_BASE + 0x0934)
#define AFE_CONN14_1 (AFE_BASE + 0x0938)
#define AFE_CONN15_1 (AFE_BASE + 0x093c)
#define AFE_CONN16_1 (AFE_BASE + 0x0940)
#define AFE_CONN17_1 (AFE_BASE + 0x0944)
#define AFE_CONN18_1 (AFE_BASE + 0x0948)
#define AFE_CONN21_1 (AFE_BASE + 0x0954)
#define AFE_CONN22_1 (AFE_BASE + 0x0958)
#define AFE_CONN23_1 (AFE_BASE + 0x095c)
#define AFE_CONN24_1 (AFE_BASE + 0x0960)
#define AFE_CONN25_1 (AFE_BASE + 0x0964)
#define AFE_CONN26_1 (AFE_BASE + 0x0968)
#define AFE_CONN27_1 (AFE_BASE + 0x096c)
#define AFE_CONN28_1 (AFE_BASE + 0x0970)
#define AFE_CONN29_1 (AFE_BASE + 0x0974)
#define AFE_CONN30_1 (AFE_BASE + 0x0978)
#define AFE_CONN31_1 (AFE_BASE + 0x097c)
#define AFE_CONN32_1 (AFE_BASE + 0x0980)
#define AFE_CONN33_1 (AFE_BASE + 0x0984)
#define AFE_CONN34_1 (AFE_BASE + 0x0988)
#define AFE_CONN_RS_1 (AFE_BASE + 0x098c)
#define AFE_CONN_DI_1 (AFE_BASE + 0x0990)
#define AFE_CONN_24BIT_1 (AFE_BASE + 0x0994)
#define AFE_CONN_REG (AFE_BASE + 0x0998)
#define AFE_CONN35 (AFE_BASE + 0x09a0)
#define AFE_CONN36 (AFE_BASE + 0x09a4)
#define AFE_CONN37 (AFE_BASE + 0x09a8)
#define AFE_CONN38 (AFE_BASE + 0x09ac)
#define AFE_CONN35_1 (AFE_BASE + 0x09b0)
#define AFE_CONN36_1 (AFE_BASE + 0x09b4)
#define AFE_CONN37_1 (AFE_BASE + 0x09b8)
#define AFE_CONN38_1 (AFE_BASE + 0x09bc)
#define AFE_CONN39 (AFE_BASE + 0x09c0)
#define AFE_CONN39_1 (AFE_BASE + 0x09e0)
#define AFE_DL1_BASE_MSB (AFE_BASE + 0x0b00)
#define AFE_DL1_CUR_MSB (AFE_BASE + 0x0b04)
#define AFE_DL1_END_MSB (AFE_BASE + 0x0b08)
#define AFE_DL2_BASE_MSB (AFE_BASE + 0x0b10)
#define AFE_DL2_CUR_MSB (AFE_BASE + 0x0b14)
#define AFE_DL2_END_MSB (AFE_BASE + 0x0b18)
#define AFE_AWB_BASE_MSB (AFE_BASE + 0x0b20)
#define AFE_AWB_END_MSB (AFE_BASE + 0x0b28)
#define AFE_AWB_CUR_MSB (AFE_BASE + 0x0b2c)
#define AFE_VUL_BASE_MSB (AFE_BASE + 0x0b30)
#define AFE_VUL_END_MSB (AFE_BASE + 0x0b38)
#define AFE_VUL_CUR_MSB (AFE_BASE + 0x0b3c)
#define AFE_DAI_BASE_MSB (AFE_BASE + 0x0b40)
#define AFE_DAI_END_MSB (AFE_BASE + 0x0b48)
#define AFE_DAI_CUR_MSB (AFE_BASE + 0x0b4c)
#define AFE_VUL2_BASE_MSB (AFE_BASE + 0x0b50)
#define AFE_VUL2_END_MSB (AFE_BASE + 0x0b58)
#define AFE_VUL2_CUR_MSB (AFE_BASE + 0x0b5c)
#define AFE_MOD_DAI_BASE_MSB (AFE_BASE + 0x0b60)
#define AFE_MOD_DAI_END_MSB (AFE_BASE + 0x0b68)
#define AFE_MOD_DAI_CUR_MSB (AFE_BASE + 0x0b6c)
#define AFE_AWB2_BASE (AFE_BASE + 0x0bd0)
#define AFE_AWB2_END (AFE_BASE + 0x0bd8)
#define AFE_AWB2_CUR (AFE_BASE + 0x0bdc)
#define AFE_VUL_D2_CUR             AFE_AWB2_CUR
#define AFE_AWB2_BASE_MSB (AFE_BASE + 0x0be0)
#define AFE_AWB2_END_MSB (AFE_BASE + 0x0be8)
#define AFE_AWB2_CUR_MSB (AFE_BASE + 0x0bec)
#define AFE_ADDA_DL_SDM_DCCOMP_CON (AFE_BASE + 0x0c50)
#define AFE_ADDA_DL_SDM_TEST       (AFE_BASE + 0x0c54)
#define AFE_ADDA_DL_DC_COMP_CFG0   (AFE_BASE + 0x0c58)
#define AFE_ADDA_DL_DC_COMP_CFG1   (AFE_BASE + 0x0c5c)
#define AFE_ADDA_DL_SDM_FIFO_MON   (AFE_BASE + 0x0c60)
#define AFE_ADDA_DL_SRC_LCH_MON    (AFE_BASE + 0x0c64)
#define AFE_ADDA_DL_SRC_RCH_MON    (AFE_BASE + 0x0c68)
#define AFE_ADDA_DL_SDM_OUT_MON    (AFE_BASE + 0x0c6c)
#define AFE_CONNSYS_I2S_CON (AFE_BASE + 0x0c78)
#define AFE_CONNSYS_I2S_MON (AFE_BASE + 0x0c7c)
#define AFE_ASRC_CONNSYS_CON0 (AFE_BASE + 0x0c80) /*AFE_ASRC_2CH_CON0*/
#define AFE_ASRC_2CH_CON1 (AFE_BASE + 0x0c84)
#define AFE_ASRC_CONNSYS_CON13     (AFE_BASE + 0x0c88)
#define AFE_ASRC_CONNSYS_CON14     (AFE_BASE + 0x0c8c)
#define AFE_ASRC_CONNSYS_CON15     (AFE_BASE + 0x0c90)
#define AFE_ASRC_CONNSYS_CON16     (AFE_BASE + 0x0c94)
#define AFE_ASRC_CONNSYS_CON17     (AFE_BASE + 0x0c98)
#define AFE_ASRC_CONNSYS_CON18     (AFE_BASE + 0x0c9c)
#define AFE_ASRC_CONNSYS_CON19     (AFE_BASE + 0x0ca0)
#define AFE_ASRC_CONNSYS_CON20     (AFE_BASE + 0x0ca4)
#define AFE_ASRC_CONNSYS_CON21     (AFE_BASE + 0x0ca8)
#define AFE_ADDA_PREDIS_CON2       (AFE_BASE + 0x0d40)
#define AFE_ADDA_PREDIS_CON3       (AFE_BASE + 0x0d44)
#define AFE_MEMIF_MON12 (AFE_BASE + 0x0d70)
#define AFE_MEMIF_MON13 (AFE_BASE + 0x0d74)
#define AFE_MEMIF_MON14 (AFE_BASE + 0x0d78)
#define AFE_MEMIF_MON15 (AFE_BASE + 0x0d7c)
#define AFE_MEMIF_MON16 (AFE_BASE + 0x0d80)
#define AFE_MEMIF_MON17 (AFE_BASE + 0x0d84)
#define AFE_MEMIF_MON18 (AFE_BASE + 0x0d88)
#define AFE_MEMIF_MON19 (AFE_BASE + 0x0d8c)
#define AFE_MEMIF_MON20 (AFE_BASE + 0x0d90)
#define AFE_MEMIF_MON21 (AFE_BASE + 0x0d94)
#define AFE_MEMIF_MON22 (AFE_BASE + 0x0d98)
#define AFE_MEMIF_MON23 (AFE_BASE + 0x0d9c)
#define AFE_MEMIF_MON24 (AFE_BASE + 0x0da0)
#define AFE_HD_ENGEN_ENABLE        (AFE_BASE + 0x0dd0)
#define AFE_ADDA_MTKAIF_CFG0       (AFE_BASE + 0x0e00)
#define AFE_ADDA_MTKAIF_TX_CFG1    (AFE_BASE + 0x0e14)
#define AFE_ADDA_MTKAIF_RX_CFG0    (AFE_BASE + 0x0e20)
#define AFE_ADDA_MTKAIF_RX_CFG1    (AFE_BASE + 0x0e24)
#define AFE_ADDA_MTKAIF_RX_CFG2    (AFE_BASE + 0x0e28)
#define AFE_ADDA_MTKAIF_MON0       (AFE_BASE + 0x0e34)
#define AFE_ADDA_MTKAIF_MON1       (AFE_BASE + 0x0e38)
#define AFE_AUD_PAD_TOP_CFG        (AFE_BASE + 0x0e40)

#define AFE_MAXLENGTH (AFE_BASE + 0x0e40)
#define AFE_REG_UNDEFINED (AFE_MAXLENGTH + 0x1)

#define AFE_VUL_D2_BASE AFE_REG_UNDEFINED
#define AFE_VUL_D2_END AFE_REG_UNDEFINED

/* do afe register ioremap */
int Auddrv_Reg_map(struct device *pdev);

void Afe_Set_Reg(unsigned int offset, unsigned int value, unsigned int mask);
unsigned int Afe_Get_Reg(unsigned int offset);

/* function to apmixed */
unsigned int GetApmixedCfg(unsigned int offset);
void SetApmixedCfg(unsigned int offset, unsigned int value, unsigned int mask);

/* function to get/set clksys register */
unsigned int clksys_get_reg(unsigned int offset);
void clksys_set_reg(unsigned int offset, unsigned int value, unsigned int mask);

/* for debug usage */
void Afe_Log_Print(void);

/* function to get pointer */
unsigned int Get_Afe_Sram_Length(void);
dma_addr_t Get_Afe_Sram_Phys_Addr(void);
dma_addr_t Get_Afe_Sram_Capture_Phys_Addr(void);
void *Get_Afe_SramBase_Pointer(void);
void *Get_Afe_SramCaptureBase_Pointer(void);

void *Get_Afe_Powertop_Pointer(void);
void *Get_AudClk_Pointer(void);
void *Get_Afe_Infra_Pointer(void);

void SetChipModemPcmConfig(int modem_index,
			   struct audio_digital_pcm p_modem_pcm_attribute);
bool SetChipModemPcmEnable(int modem_index, bool modem_pcm_on);

bool EnableSideToneFilter(bool stf_on);
bool CleanPreDistortion(void);
bool SetDLSrc2(unsigned int SampleRate);

bool SetSampleRate(unsigned int Aud_block, unsigned int SampleRate);
bool SetChannels(unsigned int Memory_Interface, unsigned int channel);
int SetMemifMonoSel(unsigned int Memory_Interface, bool mono_use_r_ch);

bool SetMemDuplicateWrite(unsigned int InterfaceType, int dupwrite);

ssize_t AudDrv_Reg_Dump(char *buffer, int size);

void SetSdmLevel(unsigned int level);
#endif
