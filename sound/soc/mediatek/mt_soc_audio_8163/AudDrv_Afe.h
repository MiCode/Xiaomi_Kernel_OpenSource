/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */



#ifndef _AUDDRV_AFE_H_
#define _AUDDRV_AFE_H_

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"
#include <linux/types.h>

/*****************************************************************************
 *                          C O N S T A N T S
 *****************************************************************************/
#define AUDIO_HW_PHYSICAL_BASE  (0x11220000L)
#define AUDIO_CLKCFG_PHYSICAL_BASE  (0x10000000L)  /* bo.pan */
/* need enable this register before access all register */
#define AUDIO_POWER_TOP (0x1000629cL)
#define AUDIO_INFRA_BASE (0x10001000L)
#define AUDIO_HW_VIRTUAL_BASE   (0xF1220000L)

#ifdef AUDIO_MEM_IOREMAP
#define AFE_BASE                (0L)
#else
#define AFE_BASE                (AUDIO_HW_VIRTUAL_BASE)
#endif

/* Internal sram: 0x1200000 (48K) */
#define AFE_INTERNAL_SRAM_PHY_BASE  (0x11221000L)
#define AFE_INTERNAL_SRAM_VIR_BASE  (AUDIO_HW_VIRTUAL_BASE - 0x70000+0x8000)
#define AFE_INTERNAL_SRAM_SIZE  (0x9000)

/* Dram */
#define AFE_EXTERNAL_DRAM_SIZE  (0x9000)
/*****************************************************************************
 *                         M A C R O
 *****************************************************************************/

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/

#define AUD_GPIO_BASE (0xF0005000L)
#define AUD_GPIO_MODE39 (0x860)
#define AUD_DRV_SEL4 (0xB40)

#define APLL_PHYSICAL_BASE (0x1000C000L)
#define AP_PLL_CON5 (0x0014)
#define AUD2PLL_CON1 (0x02B4)
#define AUD2PLL_CON2 (0x02B8)
#define AUD2PLL_CON3 (0x0304)

#define AUD1PLL_CON1 (0x02A4)
#define AUD1PLL_CON2 (0x02A8)
#define AUD1PLL_CON3 (0x0300)


#define AUDIO_CLK_CFG_4 (0x0080)
#define AUDIO_CLK_CFG_6 (0x00A0)
#define AUDIO_CLK_CFG_7 (0x00B0)
#define AUDIO_CG_SET (0x88)
#define AUDIO_CG_CLR (0x8c)
#define AUDIO_CG_STATUS (0x94)

/* 8163 APLL Clock Config Register */
#define AUDIO_CLK_CFG_6       (0x00A0)
#define AUDIO_CLK_CFG_6_SET   (0x00A4)
#define AUDIO_CLK_CFG_6_CLR   (0x00A8)
#define AUDIO_CLK_AUD_DIV0    (0x0110)
#define AUDIO_CLK_AUD_DIV1    (0x0114)
#define AUDIO_CLK_AUD_DIV2    (0x0118)
#define AUDIO_CLK_AUDDIV_0    (0x05A0)
#define AUDIO_CLK_AUDDIV_1    (0x05A4)


#ifdef AUDIO_TOP_CON0
#undef AUDIO_TOP_CON0
#endif
#define AUDIO_TOP_CON0  (AFE_BASE + 0x0000)

#ifdef AUDIO_TOP_CON1
#undef AUDIO_TOP_CON1
#endif
#define AUDIO_TOP_CON1  (AFE_BASE + 0x0004)
#define AUDIO_TOP_CON2  (AFE_BASE + 0x0008)
#define AUDIO_TOP_CON3  (AFE_BASE + 0x000C)
#define AFE_DAC_CON0    (AFE_BASE + 0x0010)
#define AFE_DAC_CON1    (AFE_BASE + 0x0014)
#define AFE_I2S_CON     (AFE_BASE + 0x0018)
#define AFE_DAIBT_CON0  (AFE_BASE + 0x001c)

#define AFE_CONN0       (AFE_BASE + 0x0020)
#define AFE_CONN1       (AFE_BASE + 0x0024)
#define AFE_CONN2       (AFE_BASE + 0x0028)
#define AFE_CONN3       (AFE_BASE + 0x002C)
#define AFE_CONN4       (AFE_BASE + 0x0030)

#define AFE_I2S_CON1    (AFE_BASE + 0x0034)
#define AFE_I2S_CON2    (AFE_BASE + 0x0038)
#define AFE_MRGIF_CON    (AFE_BASE + 0x003C)

/* Memory interface */
#define AFE_DL1_BASE    (AFE_BASE + 0x0040)
#define AFE_DL1_CUR     (AFE_BASE + 0x0044)
#define AFE_DL1_END     (AFE_BASE + 0x0048)
#define AFE_DL1_D2_BASE (AFE_BASE + 0x0340)
#define AFE_DL1_D2_CUR  (AFE_BASE + 0x0344)
#define AFE_DL1_D2_END  (AFE_BASE + 0x0348)
#define AFE_VUL_D2_BASE (AFE_BASE + 0x0350)
#define AFE_VUL_D2_END  (AFE_BASE + 0x0358)
#define AFE_VUL_D2_CUR  (AFE_BASE + 0x035C)

#define AFE_I2S_CON3    (AFE_BASE + 0x004C)
#define AFE_DL2_BASE    (AFE_BASE + 0x0050)
#define AFE_DL2_CUR     (AFE_BASE + 0x0054)
#define AFE_DL2_END     (AFE_BASE + 0x0058)
#define AFE_CONN5       (AFE_BASE + 0x005C)
#define AFE_CONN_24BIT  (AFE_BASE + 0x006C)
#define AFE_AWB_BASE    (AFE_BASE + 0x0070)
#define AFE_AWB_END     (AFE_BASE + 0x0078)
#define AFE_AWB_CUR     (AFE_BASE + 0x007C)
#define AFE_VUL_BASE    (AFE_BASE + 0x0080)
#define AFE_VUL_END     (AFE_BASE + 0x0088)
#define AFE_VUL_CUR     (AFE_BASE + 0x008C)
#define AFE_DAI_BASE    (AFE_BASE + 0x0090)
#define AFE_DAI_END     (AFE_BASE + 0x0098)
#define AFE_DAI_CUR     (AFE_BASE + 0x009C)
#define AFE_CONN6       (AFE_BASE + 0x00BC)

#define AFE_MEMIF_MSB   (AFE_BASE + 0x00CC)

/* Memory interface monitor */
#define AFE_MEMIF_MON0 (AFE_BASE + 0x00D0)
#define AFE_MEMIF_MON1 (AFE_BASE + 0x00D4)
#define AFE_MEMIF_MON2 (AFE_BASE + 0x00D8)
#define AFE_MEMIF_MON4 (AFE_BASE + 0x00E0)


/* 6582 Add */
#define AFE_ADDA_DL_SRC2_CON0   (AFE_BASE+0x00108)
#define AFE_ADDA_DL_SRC2_CON1   (AFE_BASE+0x0010C)
#define AFE_ADDA_UL_SRC_CON0    (AFE_BASE+0x00114)
#define AFE_ADDA_UL_SRC_CON1    (AFE_BASE+0x00118)
#define AFE_ADDA_TOP_CON0       (AFE_BASE+0x00120)
#define AFE_ADDA_UL_DL_CON0     (AFE_BASE+0x00124)
#define AFE_ADDA_SRC_DEBUG      (AFE_BASE+0x0012C)
#define AFE_ADDA_SRC_DEBUG_MON0 (AFE_BASE+0x00130)
#define AFE_ADDA_SRC_DEBUG_MON1 (AFE_BASE+0x00134)
#define AFE_ADDA_NEWIF_CFG0     (AFE_BASE+0x00138)
#define AFE_ADDA_NEWIF_CFG1     (AFE_BASE+0x0013C)

#define AFE_SIDETONE_DEBUG  (AFE_BASE + 0x01D0)
#define AFE_SIDETONE_MON    (AFE_BASE + 0x01D4)
#define AFE_SIDETONE_CON0   (AFE_BASE + 0x01E0)
#define AFE_SIDETONE_COEFF  (AFE_BASE + 0x01E4)
#define AFE_SIDETONE_CON1   (AFE_BASE + 0x01E8)
#define AFE_SIDETONE_GAIN   (AFE_BASE + 0x01EC)

#define AFE_SGEN_CON0   (AFE_BASE + 0x01F0)
#define AFE_TOP_CON0    (AFE_BASE + 0x0200)

#define AFE_ADDA_PREDIS_CON0    (AFE_BASE+0x00260)
#define AFE_ADDA_PREDIS_CON1    (AFE_BASE+0x00264)

#define AFE_MRGIF_MON0          (AFE_BASE+0x00270)
#define AFE_MRGIF_MON1          (AFE_BASE+0x00274)
#define AFE_MRGIF_MON2          (AFE_BASE+0x00278)

#define AFE_MOD_DAI_BASE (AFE_BASE + 0x0330)
#define AFE_MOD_DAI_END  (AFE_BASE + 0x0338)
#define AFE_MOD_DAI_CUR  (AFE_BASE + 0x033C)

/* HDMI Memory interface */
#define AFE_HDMI_OUT_CON0           (AFE_BASE + 0x0370)
#define AFE_HDMI_BASE               (AFE_BASE + 0x0374)
#define AFE_HDMI_CUR                (AFE_BASE + 0x0378)
#define AFE_HDMI_END                (AFE_BASE + 0x037C)

#define AFE_HDMI_CONN0                (AFE_BASE + 0x0390)
#define AFE_HDMI_CONN1                (AFE_BASE + 0x0398)


#define AFE_IRQ_MCU_CON             (AFE_BASE + 0x03A0)
#define AFE_IRQ_MCU_STATUS          (AFE_BASE + 0x03A4)
#define AFE_IRQ_MCU_CLR             (AFE_BASE + 0x03A8)
#define AFE_IRQ_MCU_CNT1            (AFE_BASE + 0x03AC)
#define AFE_IRQ_MCU_CNT2            (AFE_BASE + 0x03B0)
#define AFE_IRQ_MCU_EN              (AFE_BASE + 0x03B4)
#define AFE_IRQ_MCU_MON2            (AFE_BASE + 0x03B8)

#define AFE_IRQ_CNT5            (AFE_BASE + 0x03BC)
#define AFE_IRQ1_MCU_CNT_MON        (AFE_BASE + 0x03C0)

#define AFE_IRQ2_MCU_CNT_MON        (AFE_BASE + 0x03C4)
#define AFE_IRQ1_MCU_EN_CNT_MON     (AFE_BASE + 0x03C8)

#define AFE_IRQ_DEBUG            (AFE_BASE + 0x03CC)   /* 93 w/o */

#define AFE_MEMIF_MINLEN        (AFE_BASE + 0x03D0)   /* 93 w/o */
#define AFE_MEMIF_MAXLEN                (AFE_BASE + 0x03D4)
#define AFE_MEMIF_PBUF_SIZE         (AFE_BASE + 0x03D8)
#define AFE_IRQ_MCU_CNT7         (AFE_BASE + 0x03DC)

#define AFE_APLL1_TUNER_CFG             (AFE_BASE + 0x03f0)
#define AFE_APLL2_TUNER_CFG           (AFE_BASE + 0x03f4)

/* AFE GAIN CONTROL REGISTER */
#define AFE_GAIN1_CON0         (AFE_BASE + 0x0410)
#define AFE_GAIN1_CON1         (AFE_BASE + 0x0414)
#define AFE_GAIN1_CON2         (AFE_BASE + 0x0418)
#define AFE_GAIN1_CON3         (AFE_BASE + 0x041C)
#define AFE_GAIN1_CONN         (AFE_BASE + 0x0420)
#define AFE_GAIN1_CUR          (AFE_BASE + 0x0424)
#define AFE_GAIN2_CON0         (AFE_BASE + 0x0428)
#define AFE_GAIN2_CON1         (AFE_BASE + 0x042C)
#define AFE_GAIN2_CON2         (AFE_BASE + 0x0430)
#define AFE_GAIN2_CON3         (AFE_BASE + 0x0434)
#define AFE_GAIN2_CONN         (AFE_BASE + 0x0438)
#define AFE_GAIN2_CUR          (AFE_BASE + 0x043C)
#define AFE_GAIN2_CONN2        (AFE_BASE + 0x0440)
#define AFE_GAIN2_CONN3        (AFE_BASE + 0x0444)
#define AFE_GAIN1_CONN2        (AFE_BASE + 0x0448)
#define AFE_GAIN1_CONN3        (AFE_BASE + 0x044C)
#define AFE_CONN7              (AFE_BASE + 0x0460)
#define AFE_CONN8              (AFE_BASE + 0x0464)
#define AFE_CONN9              (AFE_BASE + 0x0468)
#define AFE_CONN10             (AFE_BASE + 0x046C)

#define FPGA_CFG2           (AFE_BASE + 0x4B8)
#define FPGA_CFG3           (AFE_BASE + 0x4BC)
#define FPGA_CFG0           (AFE_BASE + 0x4C0)
#define FPGA_CFG1           (AFE_BASE + 0x4C4)
#define FPGA_VER               (AFE_BASE + 0x4C8)
#define FPGA_STC                (AFE_BASE + 0x4CC)

#define AFE_ASRC_CON0   (AFE_BASE + 0x500)
#define AFE_ASRC_CON1   (AFE_BASE + 0x504)
#define AFE_ASRC_CON2   (AFE_BASE + 0x508)
#define AFE_ASRC_CON3   (AFE_BASE + 0x50C)
#define AFE_ASRC_CON4   (AFE_BASE + 0x510)
#define AFE_ASRC_CON5   (AFE_BASE + 0x514)
#define AFE_ASRC_CON6   (AFE_BASE + 0x518)
#define AFE_ASRC_CON7   (AFE_BASE + 0x51C)
#define AFE_ASRC_CON8   (AFE_BASE + 0x520)
#define AFE_ASRC_CON9   (AFE_BASE + 0x524)
#define AFE_ASRC_CON10  (AFE_BASE + 0x528)
#define AFE_ASRC_CON11  (AFE_BASE + 0x52C)

#define PCM_INTF_CON    (AFE_BASE + 0x530)
#define PCM_INTF_CON2   (AFE_BASE + 0x538)
#define PCM2_INTF_CON   (AFE_BASE + 0x53C)
#define AFE_TDM_CON1    (AFE_BASE + 0x548)
#define AFE_TDM_CON2    (AFE_BASE + 0x54C)

/* 6582 Add */
#define AFE_ASRC_CON13  (AFE_BASE+0x00550)
#define AFE_ASRC_CON14  (AFE_BASE+0x00554)
#define AFE_ASRC_CON15  (AFE_BASE+0x00558)
#define AFE_ASRC_CON16  (AFE_BASE+0x0055C)
#define AFE_ASRC_CON17  (AFE_BASE+0x00560)
#define AFE_ASRC_CON18  (AFE_BASE+0x00564)
#define AFE_ASRC_CON19  (AFE_BASE+0x00568)
#define AFE_ASRC_CON20  (AFE_BASE+0x0056C)
#define AFE_ASRC_CON21  (AFE_BASE+0x00570)

#define AFE_ASRC4_CON0          (AFE_BASE+0x06C0)
#define AFE_ASRC4_CON1          (AFE_BASE+0x06C4)
#define AFE_ASRC4_CON2          (AFE_BASE+0x06C8)
#define AFE_ASRC4_CON3          (AFE_BASE+0x06CC)
#define AFE_ASRC4_CON4          (AFE_BASE+0x06D0)
#define AFE_ASRC4_CON5          (AFE_BASE+0x06D4)
#define AFE_ASRC4_CON6          (AFE_BASE+0x06D8)
#define AFE_ASRC4_CON7          (AFE_BASE+0x06DC)
#define AFE_ASRC4_CON8          (AFE_BASE+0x06E0)
#define AFE_ASRC4_CON9          (AFE_BASE+0x06E4)
#define AFE_ASRC4_CON10         (AFE_BASE+0x06E8)
#define AFE_ASRC4_CON11         (AFE_BASE+0x06EC)
#define AFE_ASRC4_CON12         (AFE_BASE+0x06F0)
#define AFE_ASRC4_CON13         (AFE_BASE+0x06F4)
#define AFE_ASRC4_CON14         (AFE_BASE+0x06F8)

#define AFE_ASRC2_CON0          (AFE_BASE+0x0700)
#define AFE_ASRC2_CON1          (AFE_BASE+0x0704)
#define AFE_ASRC2_CON2          (AFE_BASE+0x0708)
#define AFE_ASRC2_CON3          (AFE_BASE+0x070C)
#define AFE_ASRC2_CON4          (AFE_BASE+0x0710)
#define AFE_ASRC2_CON5          (AFE_BASE+0x0714)
#define AFE_ASRC2_CON6          (AFE_BASE+0x0718)
#define AFE_ASRC2_CON7          (AFE_BASE+0x071C)
#define AFE_ASRC2_CON8          (AFE_BASE+0x0720)
#define AFE_ASRC2_CON9          (AFE_BASE+0x0724)
#define AFE_ASRC2_CON10         (AFE_BASE+0x0728)
#define AFE_ASRC2_CON11         (AFE_BASE+0x072C)
#define AFE_ASRC2_CON12         (AFE_BASE+0x0730)
#define AFE_ASRC2_CON13         (AFE_BASE+0x0734)
#define AFE_ASRC2_CON14         (AFE_BASE+0x0738)
#define AFE_ASRC3_CON0          (AFE_BASE+0x0740)
#define AFE_ASRC3_CON1          (AFE_BASE+0x0744)
#define AFE_ASRC3_CON2          (AFE_BASE+0x0748)
#define AFE_ASRC3_CON3          (AFE_BASE+0x074C)
#define AFE_ASRC3_CON4          (AFE_BASE+0x0750)
#define AFE_ASRC3_CON5          (AFE_BASE+0x0754)
#define AFE_ASRC3_CON6          (AFE_BASE+0x0758)
#define AFE_ASRC3_CON7          (AFE_BASE+0x075C)
#define AFE_ASRC3_CON8          (AFE_BASE+0x0760)
#define AFE_ASRC3_CON9          (AFE_BASE+0x0764)
#define AFE_ASRC3_CON10         (AFE_BASE+0x0768)
#define AFE_ASRC3_CON11         (AFE_BASE+0x076C)
#define AFE_ASRC3_CON12         (AFE_BASE+0x0770)
#define AFE_ASRC3_CON13         (AFE_BASE+0x0774)
#define AFE_ASRC3_CON14         (AFE_BASE+0x0778)

/* 6752 add */
#define AFE_ADDA4_TOP_CON0          (AFE_BASE+0x0780)
#define AFE_ADDA4_UL_SRC_CON0       (AFE_BASE+0x0784)
#define AFE_ADDA4_UL_SRC_CON1       (AFE_BASE+0x0788)
#define AFE_ADDA4_SRC_DEBUG         (AFE_BASE+0x078C)
#define AFE_ADDA4_SRC_DEBUG_MON0    (AFE_BASE+0x0790)
#define AFE_ADDA4_SRC_DEBUG_MON1    (AFE_BASE+0x0794)
#define AFE_ADDA4_NEWIF_CFG0        (AFE_BASE+0x0798)
#define AFE_ADDA4_NEWIF_CFG1        (AFE_BASE+0x079C)
#define AFE_ADDA4_ULCF_CFG_02_01    (AFE_BASE+0x07A0)
#define AFE_ADDA4_ULCF_CFG_04_03    (AFE_BASE+0x07A4)
#define AFE_ADDA4_ULCF_CFG_06_05    (AFE_BASE+0x07A8)
#define AFE_ADDA4_ULCF_CFG_08_07    (AFE_BASE+0x07AC)
#define AFE_ADDA4_ULCF_CFG_10_09    (AFE_BASE+0x07B0)
#define AFE_ADDA4_ULCF_CFG_12_11    (AFE_BASE+0x07B4)
#define AFE_ADDA4_ULCF_CFG_14_13    (AFE_BASE+0x07B8)
#define AFE_ADDA4_ULCF_CFG_16_15    (AFE_BASE+0x07BC)
#define AFE_ADDA4_ULCF_CFG_18_17    (AFE_BASE+0x07C0)
#define AFE_ADDA4_ULCF_CFG_20_19    (AFE_BASE+0x07C4)
#define AFE_ADDA4_ULCF_CFG_22_21    (AFE_BASE+0x07C8)
#define AFE_ADDA4_ULCF_CFG_24_23    (AFE_BASE+0x07CC)
#define AFE_ADDA4_ULCF_CFG_26_25    (AFE_BASE+0x07D0)
#define AFE_ADDA4_ULCF_CFG_28_27    (AFE_BASE+0x07D4)
#define AFE_ADDA4_ULCF_CFG_30_29    (AFE_BASE+0x07D8)

#define AFE_MAXLENGTH           (AFE_BASE+0x07D8)

/* do afe register ioremap */
void Auddrv_Reg_map(void);

void Afe_Set_Reg(uint32 offset, uint32 value, uint32 mask);
uint32  Afe_Get_Reg(uint32 offset);

/* function to Set Cfg */
uint32  GetClkCfg(uint32 offset);
void SetClkCfg(uint32 offset, uint32 value, uint32 mask);

/* function to Set Infra Cfg */
uint32  GetInfraCfg(uint32 offset);
void SetInfraCfg(uint32 offset, uint32 value, uint32 mask);

/* function to Set pll */
uint32 GetpllCfg(uint32 offset);
void SetpllCfg(uint32 offset, uint32 value, uint32 mask);


/* for debug usage */
void Afe_Log_Print(void);

/* function to get pointer */
dma_addr_t  Get_Afe_Sram_Phys_Addr(void);
dma_addr_t  Get_Afe_Sram_Capture_Phys_Addr(void);
void *Get_Afe_SramBase_Pointer(void);
void *Get_Afe_SramCaptureBase_Pointer(void);

void *Get_Afe_Powertop_Pointer(void);
void *Get_AudClk_Pointer(void);
void *Get_Afe_Infra_Pointer(void);

#endif

