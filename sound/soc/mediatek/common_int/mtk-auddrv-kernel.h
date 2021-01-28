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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AUDDRV_KERNEL_H
#define AUDDRV_KERNEL_H

#include "mtk-auddrv-common.h"
#include "mtk-auddrv-def.h"

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

#if 0
struct afe_suspend_reg {
	unsigned int Suspend_AUDIO_TOP_CON0;
	unsigned int Suspend_AUDIO_TOP_CON3;
	unsigned int Suspend_AFE_DAC_CON0;
	unsigned int Suspend_AFE_DAC_CON1;
	unsigned int Suspend_AFE_I2S_CON;

	unsigned int Suspend_AFE_CONN0;
	unsigned int Suspend_AFE_CONN1;
	unsigned int Suspend_AFE_CONN2;
	unsigned int Suspend_AFE_CONN3;
	unsigned int Suspend_AFE_CONN4;

	unsigned int Suspend_AFE_I2S_CON1;
	unsigned int Suspend_AFE_I2S_CON2;

	unsigned int Suspend_AFE_DL1_BASE;
	unsigned int Suspend_AFE_DL1_CUR;
	unsigned int Suspend_AFE_DL1_END;
	unsigned int Suspend_AFE_DL2_BASE;
	unsigned int Suspend_AFE_DL2_CUR;
	unsigned int Suspend_AFE_DL2_END;
	unsigned int Suspend_AFE_AWB_BASE;
	unsigned int Suspend_AFE_AWB_CUR;
	unsigned int Suspend_AFE_AWB_END;
	unsigned int Suspend_AFE_VUL_BASE;
	unsigned int Suspend_AFE_VUL_CUR;
	unsigned int Suspend_AFE_VUL_END;

	unsigned int Suspend_AFE_MEMIF_MON0;
	unsigned int Suspend_AFE_MEMIF_MON1;
	unsigned int Suspend_AFE_MEMIF_MON2;
	unsigned int Suspend_AFE_MEMIF_MON4;

	unsigned int Suspend_AFE_SIDETONE_DEBUG;
	unsigned int Suspend_AFE_SIDETONE_MON;
	unsigned int Suspend_AFE_SIDETONE_CON0;
	unsigned int Suspend_AFE_SIDETONE_COEFF;
	unsigned int Suspend_AFE_SIDETONE_CON1;
	unsigned int Suspend_AFE_SIDETONE_GAIN;
	unsigned int Suspend_AFE_SGEN_CON0;

	unsigned int Suspend_AFE_TOP_CON0;

	unsigned int Suspend_AFE_PREDIS_CON0;
	unsigned int Suspend_AFE_PREDIS_CON1;

	unsigned int Suspend_AFE_MOD_PCM_BASE;
	unsigned int Suspend_AFE_MOD_PCM_END;
	unsigned int Suspend_AFE_MOD_PCM_CUR;
	unsigned int Suspend_AFE_IRQ_MCU_CON;
	unsigned int Suspend_AFE_IRQ_MCU_STATUS;
	unsigned int Suspend_AFE_IRQ_CLR;
	unsigned int Suspend_AFE_IRQ_MCU_CNT1;
	unsigned int Suspend_AFE_IRQ_MCU_CNT2;
	unsigned int Suspend_AFE_IRQ_MCU_MON2;

	unsigned int Suspend_AFE_IRQ1_MCN_CNT_MON;
	unsigned int Suspend_AFE_IRQ2_MCN_CNT_MON;
	unsigned int Suspend_AFE_IRQ1_MCU_EN_CNT_MON;

	unsigned int Suspend_AFE_MEMIF_MINLEN;
	unsigned int Suspend_AFE_MEMIF_MAXLEN;
	unsigned int Suspend_AFE_MEMIF_PBUF_SIZE;

	unsigned int Suspend_AFE_GAIN1_CON0;
	unsigned int Suspend_AFE_GAIN1_CON1;
	unsigned int Suspend_AFE_GAIN1_CON2;
	unsigned int Suspend_AFE_GAIN1_CON3;
	unsigned int Suspend_AFE_GAIN1_CUR;
	unsigned int Suspend_AFE_GAIN2_CON0;
	unsigned int Suspend_AFE_GAIN2_CON1;
	unsigned int Suspend_AFE_GAIN2_CON2;
	unsigned int Suspend_AFE_GAIN2_CON3;

	unsigned int Suspend_DBG_MON0;
	unsigned int Suspend_DBG_MON1;
	unsigned int Suspend_DBG_MON2;
	unsigned int Suspend_DBG_MON3;
	unsigned int Suspend_DBG_MON4;
	unsigned int Suspend_DBG_MON5;
	unsigned int Suspend_DBG_MON6;
	unsigned int Suspend_AFE_ASRC_CON0;
	unsigned int Suspend_AFE_ASRC_CON1;
	unsigned int Suspend_AFE_ASRC_CON2;
	unsigned int Suspend_AFE_ASRC_CON3;
	unsigned int Suspend_AFE_ASRC_CON4;
	unsigned int Suspend_AFE_ASRC_CON6;
	unsigned int Suspend_AFE_ASRC_CON7;
	unsigned int Suspend_AFE_ASRC_CON8;
	unsigned int Suspend_AFE_ASRC_CON9;
	unsigned int Suspend_AFE_ASRC_CON10;
	unsigned int Suspend_AFE_ASRC_CON11;
	unsigned int Suspend_PCM_INTF_CON1;
	unsigned int Suspend_PCM_INTF_CON2;
	unsigned int Suspend_PCM2_INTF_CON;
	unsigned int Suspend_FOC_ROM_SIG;

	unsigned int Suspend_AUDIO_TOP_CON1;
	unsigned int Suspend_AFE_I2S_CON3;
	unsigned int Suspend_AFE_ADDA_DL_SRC2_CON0;
	unsigned int Suspend_AFE_ADDA_DL_SRC2_CON1;
	unsigned int Suspend_AFE_ADDA_UL_SRC_CON0;
	unsigned int Suspend_AFE_ADDA_UL_SRC_CON1;
	unsigned int Suspend_AFE_ADDA_TOP_CON0;
	unsigned int Suspend_AFE_ADDA_UL_DL_CON0;
	unsigned int Suspend_AFE_ADDA_SRC_DEBUG;
	unsigned int Suspend_AFE_ADDA_SRC_DEBUG_MON0;
	unsigned int Suspend_AFE_ADDA_SRC_DEBUG_MON1;
	unsigned int Suspend_AFE_ADDA_NEWIF_CFG0;
	unsigned int Suspend_AFE_ADDA_NEWIF_CFG1;
	unsigned int Suspend_AFE_ASRC_CON13;
	unsigned int Suspend_AFE_ASRC_CON14;
	unsigned int Suspend_AFE_ASRC_CON15;
	unsigned int Suspend_AFE_ASRC_CON16;
	unsigned int Suspend_AFE_ASRC_CON17;
	unsigned int Suspend_AFE_ASRC_CON18;
	unsigned int Suspend_AFE_ASRC_CON19;
	unsigned int Suspend_AFE_ASRC_CON20;
	unsigned int Suspend_AFE_ASRC_CON21;
};

struct ana_suspend_reg {
	unsigned short Suspend_Ana_ABB_AFE_CON0;
	unsigned short Suspend_Ana_ABB_AFE_CON1;
	unsigned short Suspend_Ana_ABB_AFE_CON2;
	unsigned short Suspend_Ana_ABB_AFE_CON3;
	unsigned short Suspend_Ana_ABB_AFE_CON4;
	unsigned short Suspend_Ana_ABB_AFE_CON5;
	unsigned short Suspend_Ana_ABB_AFE_CON6;
	unsigned short Suspend_Ana_ABB_AFE_CON7;
	unsigned short Suspend_Ana_ABB_AFE_CON8;
	unsigned short Suspend_Ana_ABB_AFE_CON9;
	unsigned short Suspend_Ana_ABB_AFE_CON10;
	unsigned short Suspend_Ana_ABB_AFE_CON11;
	unsigned short Suspend_Ana_ABB_AFE_UP8X_FIFO_CFG0;
	unsigned short Suspend_Ana_ABB_AFE_PMIC_NEWIF_CFG0;
	unsigned short Suspend_Ana_ABB_AFE_PMIC_NEWIF_CFG1;
	unsigned short Suspend_Ana_ABB_AFE_PMIC_NEWIF_CFG2;
	unsigned short Suspend_Ana_ABB_AFE_PMIC_NEWIF_CFG3;
	unsigned short Suspend_Ana_ABB_AFE_TOP_CON0;
	unsigned short Suspend_Ana_ABB_AFE_MON_DEBUG0;

	unsigned short Suspend_Ana_SPK_CON0;
	unsigned short Suspend_Ana_SPK_CON1;
	unsigned short Suspend_Ana_SPK_CON2;
	unsigned short Suspend_Ana_SPK_CON6;
	unsigned short Suspend_Ana_SPK_CON7;
	unsigned short Suspend_Ana_SPK_CON8;
	unsigned short Suspend_Ana_SPK_CON9;
	unsigned short Suspend_Ana_SPK_CON10;
	unsigned short Suspend_Ana_SPK_CON11;
	unsigned short Suspend_Ana_SPK_CON12;
	unsigned short Suspend_Ana_TOP_CKPDN0;
	unsigned short Suspend_Ana_TOP_CKPDN0_SET;
	unsigned short Suspend_Ana_TOP_CKPDN0_CLR;
	unsigned short Suspend_Ana_TOP_CKPDN1;
	unsigned short Suspend_Ana_TOP_CKPDN1_SET;
	unsigned short Suspend_Ana_TOP_CKPDN1_CLR;
	unsigned short Suspend_Ana_TOP_CKPDN2;
	unsigned short Suspend_Ana_TOP_CKPDN2_SET;
	unsigned short Suspend_Ana_TOP_CKPDN2_CLR;
	unsigned short Suspend_Ana_TOP_RST_CON;
	unsigned short Suspend_Ana_TOP_RST_CON_SET;
	unsigned short Suspend_Ana_TOP_RST_CON_CLR;
	unsigned short Suspend_Ana_TOP_CKCON0;
	unsigned short Suspend_Ana_TOP_CKCON0_SET;
	unsigned short Suspend_Ana_TOP_CKCON0_CLR;
	unsigned short Suspend_Ana_TOP_CKCON1;
	unsigned short Suspend_Ana_TOP_CKCON1_SET;
	unsigned short Suspend_Ana_TOP_CKCON1_CLR;
	unsigned short Suspend_Ana_TOP_CKTST0;
	unsigned short Suspend_Ana_TOP_CKTST1;
	unsigned short Suspend_Ana_TOP_CKTST2;

	unsigned short Suspend_Ana_AUDTOP_CON0;
	unsigned short Suspend_Ana_AUDTOP_CON1;
	unsigned short Suspend_Ana_AUDTOP_CON2;
	unsigned short Suspend_Ana_AUDTOP_CON3;
	unsigned short Suspend_Ana_AUDTOP_CON4;
	unsigned short Suspend_Ana_AUDTOP_CON5;
	unsigned short Suspend_Ana_AUDTOP_CON6;
	unsigned short Suspend_Ana_AUDTOP_CON7;
	unsigned short Suspend_Ana_AUDTOP_CON8;
	unsigned short Suspend_Ana_AUDTOP_CON9;
};

enum memif_buffer_type {
	MEM_DL1,
	MEM_DL2,
	MEM_VUL,
	MEM_DAI,
	MEM_AWB,
	MEM_MOD_DAI,
	NUM_OF_MEM_INTERFACE
};

/* TODO: KC: not used */
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

#endif

#endif
