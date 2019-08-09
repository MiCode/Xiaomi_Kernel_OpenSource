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


#ifndef AUDDRV_KERNEL_H
#define AUDDRV_KERNEL_H

#include "AudDrv_Common.h"
#include "AudDrv_Def.h"

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/


/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

struct AudAfe_Suspend_Reg {
	uint32 Suspend_AUDIO_TOP_CON0;
	uint32 Suspend_AUDIO_TOP_CON3;
	uint32 Suspend_AFE_DAC_CON0;
	uint32 Suspend_AFE_DAC_CON1;
	uint32 Suspend_AFE_I2S_CON;

	uint32 Suspend_AFE_CONN0;
	uint32 Suspend_AFE_CONN1;
	uint32 Suspend_AFE_CONN2;
	uint32 Suspend_AFE_CONN3;
	uint32 Suspend_AFE_CONN4;

	uint32 Suspend_AFE_I2S_CON1;
	uint32 Suspend_AFE_I2S_CON2;

	uint32 Suspend_AFE_DL1_BASE;
	uint32 Suspend_AFE_DL1_CUR;
	uint32 Suspend_AFE_DL1_END;
	uint32 Suspend_AFE_DL2_BASE;
	uint32 Suspend_AFE_DL2_CUR;
	uint32 Suspend_AFE_DL2_END;
	uint32 Suspend_AFE_AWB_BASE;
	uint32 Suspend_AFE_AWB_CUR;
	uint32 Suspend_AFE_AWB_END;
	uint32 Suspend_AFE_VUL_BASE;
	uint32 Suspend_AFE_VUL_CUR;
	uint32 Suspend_AFE_VUL_END;

	uint32 Suspend_AFE_MEMIF_MON0;
	uint32 Suspend_AFE_MEMIF_MON1;
	uint32 Suspend_AFE_MEMIF_MON2;
	uint32 Suspend_AFE_MEMIF_MON4;


	uint32 Suspend_AFE_SIDETONE_DEBUG;
	uint32 Suspend_AFE_SIDETONE_MON;
	uint32 Suspend_AFE_SIDETONE_CON0;
	uint32 Suspend_AFE_SIDETONE_COEFF;
	uint32 Suspend_AFE_SIDETONE_CON1;
	uint32 Suspend_AFE_SIDETONE_GAIN;
	uint32 Suspend_AFE_SGEN_CON0;

	uint32 Suspend_AFE_TOP_CON0;

	uint32 Suspend_AFE_PREDIS_CON0;
	uint32 Suspend_AFE_PREDIS_CON1;


	uint32 Suspend_AFE_MOD_PCM_BASE;
	uint32 Suspend_AFE_MOD_PCM_END;
	uint32 Suspend_AFE_MOD_PCM_CUR;
	uint32 Suspend_AFE_IRQ_MCU_CON;
	uint32 Suspend_AFE_IRQ_MCU_STATUS;
	uint32 Suspend_AFE_IRQ_CLR;
	uint32 Suspend_AFE_IRQ_MCU_CNT1;
	uint32 Suspend_AFE_IRQ_MCU_CNT2;
	uint32 Suspend_AFE_IRQ_MCU_MON2;

	uint32 Suspend_AFE_IRQ1_MCN_CNT_MON;
	uint32 Suspend_AFE_IRQ2_MCN_CNT_MON;
	uint32 Suspend_AFE_IRQ1_MCU_EN_CNT_MON;

	uint32 Suspend_AFE_MEMIF_MINLEN;
	uint32 Suspend_AFE_MEMIF_MAXLEN;
	uint32 Suspend_AFE_MEMIF_PBUF_SIZE;

	uint32 Suspend_AFE_GAIN1_CON0;
	uint32 Suspend_AFE_GAIN1_CON1;
	uint32 Suspend_AFE_GAIN1_CON2;
	uint32 Suspend_AFE_GAIN1_CON3;
	uint32 Suspend_AFE_GAIN1_CONN;
	uint32 Suspend_AFE_GAIN1_CUR;
	uint32 Suspend_AFE_GAIN2_CON0;
	uint32 Suspend_AFE_GAIN2_CON1;
	uint32 Suspend_AFE_GAIN2_CON2;
	uint32 Suspend_AFE_GAIN2_CON3;
	uint32 Suspend_AFE_GAIN2_CONN;

	uint32 Suspend_DBG_MON0;
	uint32 Suspend_DBG_MON1;
	uint32 Suspend_DBG_MON2;
	uint32 Suspend_DBG_MON3;
	uint32 Suspend_DBG_MON4;
	uint32 Suspend_DBG_MON5;
	uint32 Suspend_DBG_MON6;
	uint32 Suspend_AFE_ASRC_CON0;
	uint32 Suspend_AFE_ASRC_CON1;
	uint32 Suspend_AFE_ASRC_CON2;
	uint32 Suspend_AFE_ASRC_CON3;
	uint32 Suspend_AFE_ASRC_CON4;
	uint32 Suspend_AFE_ASRC_CON6;
	uint32 Suspend_AFE_ASRC_CON7;
	uint32 Suspend_AFE_ASRC_CON8;
	uint32 Suspend_AFE_ASRC_CON9;
	uint32 Suspend_AFE_ASRC_CON10;
	uint32 Suspend_AFE_ASRC_CON11;
	uint32 Suspend_PCM_INTF_CON1;
	uint32 Suspend_PCM_INTF_CON2;
	uint32 Suspend_PCM2_INTF_CON;
	uint32 Suspend_FOC_ROM_SIG;


	uint32 Suspend_AUDIO_TOP_CON1;
	uint32 Suspend_AUDIO_TOP_CON2;
	uint32 Suspend_AFE_I2S_CON3;
	uint32 Suspend_AFE_ADDA_DL_SRC2_CON0;
	uint32 Suspend_AFE_ADDA_DL_SRC2_CON1;
	uint32 Suspend_AFE_ADDA_UL_SRC_CON0;
	uint32 Suspend_AFE_ADDA_UL_SRC_CON1;
	uint32 Suspend_AFE_ADDA_TOP_CON0;
	uint32 Suspend_AFE_ADDA_UL_DL_CON0;
	uint32 Suspend_AFE_ADDA_SRC_DEBUG;
	uint32 Suspend_AFE_ADDA_SRC_DEBUG_MON0;
	uint32 Suspend_AFE_ADDA_SRC_DEBUG_MON1;
	uint32 Suspend_AFE_ADDA_NEWIF_CFG0;
	uint32 Suspend_AFE_ADDA_NEWIF_CFG1;
	uint32 Suspend_AFE_ASRC_CON13;
	uint32 Suspend_AFE_ASRC_CON14;
	uint32 Suspend_AFE_ASRC_CON15;
	uint32 Suspend_AFE_ASRC_CON16;
	uint32 Suspend_AFE_ASRC_CON17;
	uint32 Suspend_AFE_ASRC_CON18;
	uint32 Suspend_AFE_ASRC_CON19;
	uint32 Suspend_AFE_ASRC_CON20;
	uint32 Suspend_AFE_ASRC_CON21;


};

struct AudAna_Suspend_Reg {
	uint16 Suspend_Ana_ABB_AFE_CON0;
	uint16 Suspend_Ana_ABB_AFE_CON1;
	uint16 Suspend_Ana_ABB_AFE_CON2;
	uint16 Suspend_Ana_ABB_AFE_CON3;
	uint16 Suspend_Ana_ABB_AFE_CON4;
	uint16 Suspend_Ana_ABB_AFE_CON5;
	uint16 Suspend_Ana_ABB_AFE_CON6;
	uint16 Suspend_Ana_ABB_AFE_CON7;
	uint16 Suspend_Ana_ABB_AFE_CON8;
	uint16 Suspend_Ana_ABB_AFE_CON9;
	uint16 Suspend_Ana_ABB_AFE_CON10;
	uint16 Suspend_Ana_ABB_AFE_CON11;
	uint16 Suspend_Ana_ABB_AFE_UP8X_FIFO_CFG0;
	uint16 Suspend_Ana_ABB_AFE_PMIC_NEWIF_CFG0;
	uint16 Suspend_Ana_ABB_AFE_PMIC_NEWIF_CFG1;
	uint16 Suspend_Ana_ABB_AFE_PMIC_NEWIF_CFG2;
	uint16 Suspend_Ana_ABB_AFE_PMIC_NEWIF_CFG3;
	uint16 Suspend_Ana_ABB_AFE_TOP_CON0;
	uint16 Suspend_Ana_ABB_AFE_MON_DEBUG0;

	uint16 Suspend_Ana_SPK_CON0;
	uint16 Suspend_Ana_SPK_CON1;
	uint16 Suspend_Ana_SPK_CON2;
	uint16 Suspend_Ana_SPK_CON6;
	uint16 Suspend_Ana_SPK_CON7;
	uint16 Suspend_Ana_SPK_CON8;
	uint16 Suspend_Ana_SPK_CON9;
	uint16 Suspend_Ana_SPK_CON10;
	uint16 Suspend_Ana_SPK_CON11;
	uint16 Suspend_Ana_SPK_CON12;
	uint16 Suspend_Ana_TOP_CKPDN0;
	uint16 Suspend_Ana_TOP_CKPDN0_SET;
	uint16 Suspend_Ana_TOP_CKPDN0_CLR;
	uint16 Suspend_Ana_TOP_CKPDN1;
	uint16 Suspend_Ana_TOP_CKPDN1_SET;
	uint16 Suspend_Ana_TOP_CKPDN1_CLR;
	uint16 Suspend_Ana_TOP_CKPDN2;
	uint16 Suspend_Ana_TOP_CKPDN2_SET;
	uint16 Suspend_Ana_TOP_CKPDN2_CLR;
	uint16 Suspend_Ana_TOP_RST_CON;
	uint16 Suspend_Ana_TOP_RST_CON_SET;
	uint16 Suspend_Ana_TOP_RST_CON_CLR;
	uint16 Suspend_Ana_TOP_RST_MISC;
	uint16 Suspend_Ana_TOP_RST_MISC_SET;
	uint16 Suspend_Ana_TOP_RST_MISC_CLR;
	uint16 Suspend_Ana_TOP_CKCON0;
	uint16 Suspend_Ana_TOP_CKCON0_SET;
	uint16 Suspend_Ana_TOP_CKCON0_CLR;
	uint16 Suspend_Ana_TOP_CKCON1;
	uint16 Suspend_Ana_TOP_CKCON1_SET;
	uint16 Suspend_Ana_TOP_CKCON1_CLR;
	uint16 Suspend_Ana_TOP_CKTST0;
	uint16 Suspend_Ana_TOP_CKTST1;
	uint16 Suspend_Ana_TOP_CKTST2;

	uint16 Suspend_Ana_AUDTOP_CON0;
	uint16 Suspend_Ana_AUDTOP_CON1;
	uint16 Suspend_Ana_AUDTOP_CON2;
	uint16 Suspend_Ana_AUDTOP_CON3;
	uint16 Suspend_Ana_AUDTOP_CON4;
	uint16 Suspend_Ana_AUDTOP_CON5;
	uint16 Suspend_Ana_AUDTOP_CON6;
	uint16 Suspend_Ana_AUDTOP_CON7;
	uint16 Suspend_Ana_AUDTOP_CON8;
	uint16 Suspend_Ana_AUDTOP_CON9;
};

enum {
	MEM_DL1,
	MEM_DL2,
	MEM_VUL,
	MEM_DAI,
	MEM_I2S, /* Cuurently not used. Add for sync with user space */
	MEM_AWB,
	MEM_MOD_DAI,
	NUM_OF_MEM_INTERFACE
};


enum {
	INTERRUPT_IRQ1_MCU = 1,
	INTERRUPT_IRQ2_MCU = 2,
	INTERRUPT_IRQ3_MCU = 4,
	INTERRUPT_IRQ4_MCU = 8,
	INTERRUPT_IRQ5_MCU = 16,
	INTERRUPT_IRQ7_MCU = 64,
};

enum {
	CLOCK_AUD_AFE = 0,
	CLOCK_AUD_I2S,
	CLOCK_AUD_ADC,
	CLOCK_AUD_DAC,
	CLOCK_AUD_LINEIN,
	CLOCK_AUD_HDMI,
	CLOCK_AUD_26M,  /* core clock */
	CLOCK_TYPE_MAX
};

#endif


