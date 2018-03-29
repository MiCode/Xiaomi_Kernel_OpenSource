/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
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
/*
typedef struct {
	volatile uint32 Suspend_AUDIO_TOP_CON0;
	volatile uint32 Suspend_AUDIO_TOP_CON3;
	volatile uint32 Suspend_AFE_DAC_CON0;
	volatile uint32 Suspend_AFE_DAC_CON1;
	volatile uint32 Suspend_AFE_I2S_CON;

	volatile uint32 Suspend_AFE_CONN0;
	volatile uint32 Suspend_AFE_CONN1;
	volatile uint32 Suspend_AFE_CONN2;
	volatile uint32 Suspend_AFE_CONN3;
	volatile uint32 Suspend_AFE_CONN4;

	volatile uint32 Suspend_AFE_I2S_CON1;
	volatile uint32 Suspend_AFE_I2S_CON2;

	volatile uint32 Suspend_AFE_DL1_BASE;
	volatile uint32 Suspend_AFE_DL1_CUR;
	volatile uint32 Suspend_AFE_DL1_END;
	volatile uint32 Suspend_AFE_DL2_BASE;
	volatile uint32 Suspend_AFE_DL2_CUR;
	volatile uint32 Suspend_AFE_DL2_END;
	volatile uint32 Suspend_AFE_AWB_BASE;
	volatile uint32 Suspend_AFE_AWB_CUR;
	volatile uint32 Suspend_AFE_AWB_END;
	volatile uint32 Suspend_AFE_VUL_BASE;
	volatile uint32 Suspend_AFE_VUL_CUR;
	volatile uint32 Suspend_AFE_VUL_END;

	volatile uint32 Suspend_AFE_MEMIF_MON0;
	volatile uint32 Suspend_AFE_MEMIF_MON1;
	volatile uint32 Suspend_AFE_MEMIF_MON2;
	volatile uint32 Suspend_AFE_MEMIF_MON4;


	volatile uint32 Suspend_AFE_SIDETONE_DEBUG;
	volatile uint32 Suspend_AFE_SIDETONE_MON;
	volatile uint32 Suspend_AFE_SIDETONE_CON0;
	volatile uint32 Suspend_AFE_SIDETONE_COEFF;
	volatile uint32 Suspend_AFE_SIDETONE_CON1;
	volatile uint32 Suspend_AFE_SIDETONE_GAIN;
	volatile uint32 Suspend_AFE_SGEN_CON0;

	volatile uint32 Suspend_AFE_TOP_CON0;

	volatile uint32 Suspend_AFE_PREDIS_CON0;
	volatile uint32 Suspend_AFE_PREDIS_CON1;


	volatile uint32 Suspend_AFE_MOD_PCM_BASE;
	volatile uint32 Suspend_AFE_MOD_PCM_END;
	volatile uint32 Suspend_AFE_MOD_PCM_CUR;
	volatile uint32 Suspend_AFE_IRQ_MCU_CON;
	volatile uint32 Suspend_AFE_IRQ_MCU_STATUS;
	volatile uint32 Suspend_AFE_IRQ_CLR;
	volatile uint32 Suspend_AFE_IRQ_MCU_CNT1;
	volatile uint32 Suspend_AFE_IRQ_MCU_CNT2;
	volatile uint32 Suspend_AFE_IRQ_MCU_MON2;

	volatile uint32 Suspend_AFE_IRQ1_MCN_CNT_MON;
	volatile uint32 Suspend_AFE_IRQ2_MCN_CNT_MON;
	volatile uint32 Suspend_AFE_IRQ1_MCU_EN_CNT_MON;

	volatile uint32 Suspend_AFE_MEMIF_MINLEN;
	volatile uint32 Suspend_AFE_MEMIF_MAXLEN;
	volatile uint32 Suspend_AFE_MEMIF_PBUF_SIZE;

	volatile uint32 Suspend_AFE_GAIN1_CON0;
	volatile uint32 Suspend_AFE_GAIN1_CON1;
	volatile uint32 Suspend_AFE_GAIN1_CON2;
	volatile uint32 Suspend_AFE_GAIN1_CON3;
	volatile uint32 Suspend_AFE_GAIN1_CUR;
	volatile uint32 Suspend_AFE_GAIN2_CON0;
	volatile uint32 Suspend_AFE_GAIN2_CON1;
	volatile uint32 Suspend_AFE_GAIN2_CON2;
	volatile uint32 Suspend_AFE_GAIN2_CON3;

	volatile uint32 Suspend_DBG_MON0;
	volatile uint32 Suspend_DBG_MON1;
	volatile uint32 Suspend_DBG_MON2;
	volatile uint32 Suspend_DBG_MON3;
	volatile uint32 Suspend_DBG_MON4;
	volatile uint32 Suspend_DBG_MON5;
	volatile uint32 Suspend_DBG_MON6;
	volatile uint32 Suspend_AFE_ASRC_CON0;
	volatile uint32 Suspend_AFE_ASRC_CON1;
	volatile uint32 Suspend_AFE_ASRC_CON2;
	volatile uint32 Suspend_AFE_ASRC_CON3;
	volatile uint32 Suspend_AFE_ASRC_CON4;
	volatile uint32 Suspend_AFE_ASRC_CON6;
	volatile uint32 Suspend_AFE_ASRC_CON7;
	volatile uint32 Suspend_AFE_ASRC_CON8;
	volatile uint32 Suspend_AFE_ASRC_CON9;
	volatile uint32 Suspend_AFE_ASRC_CON10;
	volatile uint32 Suspend_AFE_ASRC_CON11;
	volatile uint32 Suspend_PCM_INTF_CON1;
	volatile uint32 Suspend_PCM_INTF_CON2;
	volatile uint32 Suspend_PCM2_INTF_CON;
	volatile uint32 Suspend_FOC_ROM_SIG;


	volatile uint32 Suspend_AUDIO_TOP_CON1;
	volatile uint32 Suspend_AFE_I2S_CON3;
	volatile uint32 Suspend_AFE_ADDA_DL_SRC2_CON0;
	volatile uint32 Suspend_AFE_ADDA_DL_SRC2_CON1;
	volatile uint32 Suspend_AFE_ADDA_UL_SRC_CON0;
	volatile uint32 Suspend_AFE_ADDA_UL_SRC_CON1;
	volatile uint32 Suspend_AFE_ADDA_TOP_CON0;
	volatile uint32 Suspend_AFE_ADDA_UL_DL_CON0;
	volatile uint32 Suspend_AFE_ADDA_SRC_DEBUG;
	volatile uint32 Suspend_AFE_ADDA_SRC_DEBUG_MON0;
	volatile uint32 Suspend_AFE_ADDA_SRC_DEBUG_MON1;
	volatile uint32 Suspend_AFE_ADDA_NEWIF_CFG0;
	volatile uint32 Suspend_AFE_ADDA_NEWIF_CFG1;
	volatile uint32 Suspend_AFE_ASRC_CON13;
	volatile uint32 Suspend_AFE_ASRC_CON14;
	volatile uint32 Suspend_AFE_ASRC_CON15;
	volatile uint32 Suspend_AFE_ASRC_CON16;
	volatile uint32 Suspend_AFE_ASRC_CON17;
	volatile uint32 Suspend_AFE_ASRC_CON18;
	volatile uint32 Suspend_AFE_ASRC_CON19;
	volatile uint32 Suspend_AFE_ASRC_CON20;
	volatile uint32 Suspend_AFE_ASRC_CON21;


} AudAfe_Suspend_Reg;

typedef struct {
	volatile uint16 Suspend_Ana_ABB_AFE_CON0;
	volatile uint16 Suspend_Ana_ABB_AFE_CON1;
	volatile uint16 Suspend_Ana_ABB_AFE_CON2;
	volatile uint16 Suspend_Ana_ABB_AFE_CON3;
	volatile uint16 Suspend_Ana_ABB_AFE_CON4;
	volatile uint16 Suspend_Ana_ABB_AFE_CON5;
	volatile uint16 Suspend_Ana_ABB_AFE_CON6;
	volatile uint16 Suspend_Ana_ABB_AFE_CON7;
	volatile uint16 Suspend_Ana_ABB_AFE_CON8;
	volatile uint16 Suspend_Ana_ABB_AFE_CON9;
	volatile uint16 Suspend_Ana_ABB_AFE_CON10;
	volatile uint16 Suspend_Ana_ABB_AFE_CON11;
	volatile uint16 Suspend_Ana_ABB_AFE_UP8X_FIFO_CFG0;
	volatile uint16 Suspend_Ana_ABB_AFE_PMIC_NEWIF_CFG0;
	volatile uint16 Suspend_Ana_ABB_AFE_PMIC_NEWIF_CFG1;
	volatile uint16 Suspend_Ana_ABB_AFE_PMIC_NEWIF_CFG2;
	volatile uint16 Suspend_Ana_ABB_AFE_PMIC_NEWIF_CFG3;
	volatile uint16 Suspend_Ana_ABB_AFE_TOP_CON0;
	volatile uint16 Suspend_Ana_ABB_AFE_MON_DEBUG0;

	volatile uint16 Suspend_Ana_SPK_CON0;
	volatile uint16 Suspend_Ana_SPK_CON1;
	volatile uint16 Suspend_Ana_SPK_CON2;
	volatile uint16 Suspend_Ana_SPK_CON6;
	volatile uint16 Suspend_Ana_SPK_CON7;
	volatile uint16 Suspend_Ana_SPK_CON8;
	volatile uint16 Suspend_Ana_SPK_CON9;
	volatile uint16 Suspend_Ana_SPK_CON10;
	volatile uint16 Suspend_Ana_SPK_CON11;
	volatile uint16 Suspend_Ana_SPK_CON12;
	volatile uint16 Suspend_Ana_TOP_CKPDN0;
	volatile uint16 Suspend_Ana_TOP_CKPDN0_SET;
	volatile uint16 Suspend_Ana_TOP_CKPDN0_CLR;
	volatile uint16 Suspend_Ana_TOP_CKPDN1;
	volatile uint16 Suspend_Ana_TOP_CKPDN1_SET;
	volatile uint16 Suspend_Ana_TOP_CKPDN1_CLR;
	volatile uint16 Suspend_Ana_TOP_CKPDN2;
	volatile uint16 Suspend_Ana_TOP_CKPDN2_SET;
	volatile uint16 Suspend_Ana_TOP_CKPDN2_CLR;
	volatile uint16 Suspend_Ana_TOP_RST_CON;
	volatile uint16 Suspend_Ana_TOP_RST_CON_SET;
	volatile uint16 Suspend_Ana_TOP_RST_CON_CLR;
	volatile uint16 Suspend_Ana_TOP_CKCON0;
	volatile uint16 Suspend_Ana_TOP_CKCON0_SET;
	volatile uint16 Suspend_Ana_TOP_CKCON0_CLR;
	volatile uint16 Suspend_Ana_TOP_CKCON1;
	volatile uint16 Suspend_Ana_TOP_CKCON1_SET;
	volatile uint16 Suspend_Ana_TOP_CKCON1_CLR;
	volatile uint16 Suspend_Ana_TOP_CKTST0;
	volatile uint16 Suspend_Ana_TOP_CKTST1;
	volatile uint16 Suspend_Ana_TOP_CKTST2;

	volatile uint16 Suspend_Ana_AUDTOP_CON0;
	volatile uint16 Suspend_Ana_AUDTOP_CON1;
	volatile uint16 Suspend_Ana_AUDTOP_CON2;
	volatile uint16 Suspend_Ana_AUDTOP_CON3;
	volatile uint16 Suspend_Ana_AUDTOP_CON4;
	volatile uint16 Suspend_Ana_AUDTOP_CON5;
	volatile uint16 Suspend_Ana_AUDTOP_CON6;
	volatile uint16 Suspend_Ana_AUDTOP_CON7;
	volatile uint16 Suspend_Ana_AUDTOP_CON8;
	volatile uint16 Suspend_Ana_AUDTOP_CON9;
} AudAna_Suspend_Reg;
*/
/*
typedef enum {
	MEM_DL1,
	MEM_DL2,
	MEM_VUL,
	MEM_DAI,
	MEM_AWB,
	MEM_MOD_DAI,
	NUM_OF_MEM_INTERFACE
} MEMIF_BUFFER_TYPE;
*/
/* TODO: KC: not used */
/*
enum {
	CLOCK_AUD_AFE = 0,
	CLOCK_AUD_I2S,
	CLOCK_AUD_ADC,
	CLOCK_AUD_DAC,
	CLOCK_AUD_LINEIN,
	CLOCK_AUD_HDMI,
	CLOCK_AUD_26M,
	CLOCK_TYPE_MAX
};
*/
#endif
