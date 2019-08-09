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

/************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_Ana.h
 *
 * Project:
 * --------
 *   MT6355  Audio Driver Ana
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 *   Michael Hsiao (mtk08429)
 *-----------------------------------------------------------------------
 *
 *
 ************************************************************************
 */

#ifndef _AUDDRV_ANA_H_
#define _AUDDRV_ANA_H_

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/
#include "mtk-auddrv-common.h"

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

/*****************************************************************************
 *                         M A C R O
 *****************************************************************************/

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/

#define PMIC_REG_BASE (0x0000)
#define PMIC_AUDIO_SYS_TOP_REG_BASE (0x6000)

#define MT6355_PORTING

/*PMIC Digital Audio Register*/
#define AFE_UL_DL_CON0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0000))
#define AFE_DL_SRC2_CON0_H                                                     \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0002))
#define AFE_DL_SRC2_CON0_L                                                     \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0004))
#define AFE_DL_SDM_CON0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0006))
#define AFE_DL_SDM_CON1 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0008))
#define AFE_UL_SRC_CON0_H ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x000a))
#define AFE_UL_SRC_CON0_L ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x000c))
#define AFE_UL_SRC_CON1_H ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x000e))
#define AFE_UL_SRC_CON1_L ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0010))
#define PMIC_AFE_TOP_CON0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0012))
#define AFE_AUDIO_TOP_CON0                                                     \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0014))
#define AFE_DL_SRC_MON0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0016))
#define AFE_DL_SDM_TEST0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0018))
#define AFE_MON_DEBUG0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x001a))
#define AFUNC_AUD_CON0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x001c))
#define AFUNC_AUD_CON1 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x001e))
#define AFUNC_AUD_CON2 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0020))
#define AFUNC_AUD_CON3 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0022))
#define AFUNC_AUD_CON4 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0024))
#define AFUNC_AUD_MON0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0026))
#define AFUNC_AUD_MON1 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0028))
#define AUDRC_TUNE_MON0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x002a))
#define AFE_UP8X_FIFO_CFG0                                                     \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x002c))
#define AFE_UP8X_FIFO_LOG_MON0                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x002e))
#define AFE_UP8X_FIFO_LOG_MON1                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0030))
#define AFE_DL_DC_COMP_CFG0                                                    \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0032))
#define AFE_DL_DC_COMP_CFG1                                                    \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0034))
#define AFE_DL_DC_COMP_CFG2                                                    \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0036))
#define AFE_PMIC_NEWIF_CFG0                                                    \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0038))
#define AFE_PMIC_NEWIF_CFG1                                                    \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x003a))
#define AFE_PMIC_NEWIF_CFG2                                                    \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x003c))
#define AFE_PMIC_NEWIF_CFG3                                                    \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x003e))
#define AFE_SGEN_CFG0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0040))
#define AFE_SGEN_CFG1 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0042))
#if 0
#define AFE_ADDA2_UP8X_FIFO_LOG_MON0                                           \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x004c))
#define AFE_ADDA2_UP8X_FIFO_LOG_MON1                                           \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x004e))
#define AFE_ADDA2_PMIC_NEWIF_CFG0                                              \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0050))
#define AFE_ADDA2_PMIC_NEWIF_CFG1                                              \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0052))
#define AFE_ADDA2_PMIC_NEWIF_CFG2                                              \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0054))
#define AFE_ADC_ASYNC_FIFO_CFG                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0058))
#endif
#define AFE_VOW_TOP ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0070))
#define AFE_VOW_CFG0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0072))
#define AFE_VOW_CFG1 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0074))
#define AFE_VOW_CFG2 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0076))
#define AFE_VOW_CFG3 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0078))
#define AFE_VOW_CFG4 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x007a))
#define AFE_VOW_CFG5 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x007c))
#define AFE_VOW_CFG6 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x007e))
#define AFE_VOW_MON0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0080))
#define AFE_VOW_MON1 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0082))
#define AFE_VOW_MON2 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0084))
#define AFE_VOW_MON3 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0086))
#define AFE_VOW_SN_INI_CFG                                                     \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0088))
#define AFE_VOW_TGEN_CFG0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x008a))
#define AFE_VOW_POSDIV_CFG0                                                    \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x008c))
#define AFE_VOW_HPF_CFG0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x008e))
#define AFE_DCCLK_CFG0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0090))
#define AFE_DCCLK_CFG1 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0092))
#define AFE_NCP_CFG0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0096))
#define AFE_NCP_CFG1 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0098))
#define AFE_VOW_MON4 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x009a))
#define AFE_VOW_MON5 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x009C))
#define AFE_VOW_PERIODIC_CFG0                                                  \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00a0))
#define AFE_VOW_PERIODIC_CFG1                                                  \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00a2))
#define AFE_VOW_PERIODIC_CFG2                                                  \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00a4))
#define AFE_VOW_PERIODIC_CFG3                                                  \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00a6))
#define AFE_VOW_PERIODIC_CFG4                                                  \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00a8))
#define AFE_VOW_PERIODIC_CFG5                                                  \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00aa))
#define AFE_VOW_PERIODIC_CFG6                                                  \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00ac))
#define AFE_VOW_PERIODIC_CFG7                                                  \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00ae))
#define AFE_VOW_PERIODIC_CFG8                                                  \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00b0))
#define AFE_VOW_PERIODIC_CFG9                                                  \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00b2))
#define AFE_VOW_PERIODIC_CFG10                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00b4))
#define AFE_VOW_PERIODIC_CFG11                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00b6))
#define AFE_VOW_PERIODIC_CFG12                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00b8))
#define AFE_VOW_PERIODIC_CFG13                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00ba))
#define AFE_VOW_PERIODIC_CFG14                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00bc))
#define AFE_VOW_PERIODIC_CFG15                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00be))
#define AFE_VOW_PERIODIC_CFG16                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00c0))
#define AFE_VOW_PERIODIC_CFG17                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00c2))
#define AFE_VOW_PERIODIC_CFG18                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00c4))
#define AFE_VOW_PERIODIC_CFG19                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00c6))
#define AFE_VOW_PERIODIC_CFG20                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00c8))
#define AFE_VOW_PERIODIC_CFG21                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00ca))
#define AFE_VOW_PERIODIC_CFG22                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00cc))
#define AFE_VOW_PERIODIC_CFG23                                                 \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00ce))
#define AFE_VOW_PERIODIC_MON0                                                  \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00d0))
#define AFE_VOW_PERIODIC_MON1                                                  \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00d2))
#define AFE_NCP_CFG2 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0108))
#define AFE_DL_NLE_R_CFG0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x010a))
#define AFE_DL_NLE_R_CFG1 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x010c))
#define AFE_DL_NLE_R_CFG2 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x010e))
#define AFE_DL_NLE_R_CFG3 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0110))
#define AFE_DL_NLE_L_CFG0 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0112))
#define AFE_DL_NLE_L_CFG1 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0114))
#define AFE_DL_NLE_L_CFG2 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0116))
#define AFE_DL_NLE_L_CFG3 ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0118))
#define AFE_RGS_NLE_R_CFG0                                                     \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x011a))
#define AFE_RGS_NLE_R_CFG1                                                     \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x011c))
#define AFE_RGS_NLE_R_CFG2                                                     \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x011e))
#define AFE_RGS_NLE_R_CFG3                                                     \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0120))
#define AFE_RGS_NLE_L_CFG0                                                     \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0122))
#define AFE_RGS_NLE_L_CFG1                                                     \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0124))
#define AFE_RGS_NLE_L_CFG2                                                     \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0126))
#define AFE_RGS_NLE_L_CFG3                                                     \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0128))
#define AUD_TOP_CFG ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0130))
#define AFE_DL_DC_COMP_CFG3                                                    \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0136))
#define AFE_DL_DC_COMP_CFG4                                                    \
	((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0138))
#define AFE_DL_DCM_CON ((unsigned int)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x013a))

/* PMIC Analog Audio Register */
#define TOP_CKPDN_CON0 ((unsigned int)(PMIC_REG_BASE + 0x0400))
#define TOP_CKPDN_CON0_SET ((unsigned int)(PMIC_REG_BASE + 0x0402))
#define TOP_CKPDN_CON0_CLR ((unsigned int)(PMIC_REG_BASE + 0x0404))
#define TOP_CKPDN_CON1 ((unsigned int)(PMIC_REG_BASE + 0x0406))
#define TOP_CKPDN_CON1_SET ((unsigned int)(PMIC_REG_BASE + 0x0408))
#define TOP_CKPDN_CON1_CLR ((unsigned int)(PMIC_REG_BASE + 0x040a))
#define TOP_CKPDN_CON2 ((unsigned int)(PMIC_REG_BASE + 0x040c))
#define TOP_CKPDN_CON2_SET ((unsigned int)(PMIC_REG_BASE + 0x040e))
#define TOP_CKPDN_CON2_CLR ((unsigned int)(PMIC_REG_BASE + 0x0410))
#define TOP_CKPDN_CON3 ((unsigned int)(PMIC_REG_BASE + 0x0412))
#define TOP_CKPDN_CON3_SET ((unsigned int)(PMIC_REG_BASE + 0x0414))
#define TOP_CKPDN_CON3_CLR ((unsigned int)(PMIC_REG_BASE + 0x0416))
#define TOP_CKPDN_CON4 ((unsigned int)(PMIC_REG_BASE + 0x0418))
#define TOP_CKPDN_CON4_SET ((unsigned int)(PMIC_REG_BASE + 0x041a))
#define TOP_CKPDN_CON4_CLR ((unsigned int)(PMIC_REG_BASE + 0x041c))
#define TOP_CKSEL_CON0 ((unsigned int)(PMIC_REG_BASE + 0x041e))
#define TOP_CKSEL_CON0_SET ((unsigned int)(PMIC_REG_BASE + 0x0420))
#define TOP_CKSEL_CON0_CLR ((unsigned int)(PMIC_REG_BASE + 0x0422))
#define TOP_CKSEL_CON2_SET ((unsigned int)(PMIC_REG_BASE + 0x0426))
#define TOP_CKSEL_CON2_CLR ((unsigned int)(PMIC_REG_BASE + 0x0428))
#define TOP_CKDIVSEL_CON0 ((unsigned int)(PMIC_REG_BASE + 0x042a))
#define TOP_CKDIVSEL_CON0_SET ((unsigned int)(PMIC_REG_BASE + 0x042c))
#define TOP_CKDIVSEL_CON0_CLR ((unsigned int)(PMIC_REG_BASE + 0x042e))
#define TOP_CKHWEN_CON0 ((unsigned int)(PMIC_REG_BASE + 0x0430))
#define TOP_CKHWEN_CON0_SET ((unsigned int)(PMIC_REG_BASE + 0x0432))
#define TOP_CKHWEN_CON0_CLR ((unsigned int)(PMIC_REG_BASE + 0x0434))
#define TOP_CKHWEN_CON1 ((unsigned int)(PMIC_REG_BASE + 0x0436))
#define TOP_CKHWEN_CON1_SET ((unsigned int)(PMIC_REG_BASE + 0x0438))
#define TOP_CKHWEN_CON1_CLR ((unsigned int)(PMIC_REG_BASE + 0x043a))

#define TOP_BUCK_ANACK_FREQ_SEL_CON0 ((unsigned int)(PMIC_REG_BASE + 0x043c))
#define TOP_BUCK_ANACK_FREQ_SEL_CON0_SET                                       \
	((unsigned int)(PMIC_REG_BASE + 0x043e))
#define TOP_BUCK_ANACK_FREQ_SEL_CON0_CLR                                       \
	((unsigned int)(PMIC_REG_BASE + 0x0440))
#define TOP_BUCK_ANACK_FREQ_SEL_CON1 ((unsigned int)(PMIC_REG_BASE + 0x0442))
#define TOP_BUCK_ANACK_FREQ_SEL_CON1_SET                                       \
	((unsigned int)(PMIC_REG_BASE + 0x0444))
#define TOP_BUCK_ANACK_FREQ_SEL_CON1_CLR                                       \
	((unsigned int)(PMIC_REG_BASE + 0x0446))

#define TOP_CLKSQ ((unsigned int)(PMIC_REG_BASE + 0x044c))
#define TOP_CLKSQ_SET ((unsigned int)(PMIC_REG_BASE + 0x044e))
#define TOP_CLKSQ_CLR ((unsigned int)(PMIC_REG_BASE + 0x0450))
#define TOP_CLKSQ_RTC ((unsigned int)(PMIC_REG_BASE + 0x0452))
#define TOP_CLKSQ_RTC_SET ((unsigned int)(PMIC_REG_BASE + 0x0454))
#define TOP_CLKSQ_RTC_CLR ((unsigned int)(PMIC_REG_BASE + 0x0456))
#define TOP_CLK_TRIM ((unsigned int)(PMIC_REG_BASE + 0x0458))
#define TOP_CLK_CON0_SET ((unsigned int)(PMIC_REG_BASE + 0x045c))
#define TOP_CLK_CON0_CLR ((unsigned int)(PMIC_REG_BASE + 0x045e))
#define TOP_CLK_CON1_SET ((unsigned int)(PMIC_REG_BASE + 0x0462))
#define TOP_CLK_CON1_CLR ((unsigned int)(PMIC_REG_BASE + 0x0464))

#define ZCD_CON0 ((unsigned int)(PMIC_REG_BASE + 0x3800))
#define ZCD_CON1 ((unsigned int)(PMIC_REG_BASE + 0x3802))
#define ZCD_CON2 ((unsigned int)(PMIC_REG_BASE + 0x3804))
#define ZCD_CON3 ((unsigned int)(PMIC_REG_BASE + 0x3806))
#define ZCD_CON4 ((unsigned int)(PMIC_REG_BASE + 0x3808))
#define ZCD_CON5 ((unsigned int)(PMIC_REG_BASE + 0x380A))

#define LDO_VA18_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1666))
#define LDO_VA18_CON1 ((unsigned int)(PMIC_REG_BASE + 0x1674))
#define LDO_VA18_CON2 ((unsigned int)(PMIC_REG_BASE + 0x1676))
#define LDO_VA18_CON3 ((unsigned int)(PMIC_REG_BASE + 0x1678))

#define DRV_CON2 ((unsigned int)(PMIC_REG_BASE + 0x0224))
#define DRV_CON3 ((unsigned int)(PMIC_REG_BASE + 0x0226))

/* PMIC Analog Audio Register */
#define AUDDEC_ANA_CON0 ((unsigned int)(PMIC_REG_BASE + 0x3600))
#define AUDDEC_ANA_CON1 ((unsigned int)(PMIC_REG_BASE + 0x3602))
#define AUDDEC_ANA_CON2 ((unsigned int)(PMIC_REG_BASE + 0x3604))
#define AUDDEC_ANA_CON3 ((unsigned int)(PMIC_REG_BASE + 0x3606))
#define AUDDEC_ANA_CON4 ((unsigned int)(PMIC_REG_BASE + 0x3608))
#define AUDDEC_ANA_CON5 ((unsigned int)(PMIC_REG_BASE + 0x360A))
#define AUDDEC_ANA_CON6 ((unsigned int)(PMIC_REG_BASE + 0x360C))
#define AUDDEC_ANA_CON7 ((unsigned int)(PMIC_REG_BASE + 0x360E))
#define AUDDEC_ANA_CON8 ((unsigned int)(PMIC_REG_BASE + 0x3610))
#define AUDDEC_ANA_CON9 ((unsigned int)(PMIC_REG_BASE + 0x3612))
#define AUDDEC_ANA_CON10 ((unsigned int)(PMIC_REG_BASE + 0x3614))
#define AUDDEC_ANA_CON11 ((unsigned int)(PMIC_REG_BASE + 0x3616))
#define AUDDEC_ANA_CON12 ((unsigned int)(PMIC_REG_BASE + 0x3618))
#define AUDDEC_ANA_CON13 ((unsigned int)(PMIC_REG_BASE + 0x361A))
#define AUDDEC_ANA_CON14 ((unsigned int)(PMIC_REG_BASE + 0x361C))

#define AUDENC_ANA_CON0 ((unsigned int)(PMIC_REG_BASE + 0x361E))
#define AUDENC_ANA_CON1 ((unsigned int)(PMIC_REG_BASE + 0x3620))
#define AUDENC_ANA_CON2 ((unsigned int)(PMIC_REG_BASE + 0x3622))
#define AUDENC_ANA_CON3 ((unsigned int)(PMIC_REG_BASE + 0x3624))
#define AUDENC_ANA_CON4 ((unsigned int)(PMIC_REG_BASE + 0x3626))
#define AUDENC_ANA_CON5 ((unsigned int)(PMIC_REG_BASE + 0x3628))
#define AUDENC_ANA_CON6 ((unsigned int)(PMIC_REG_BASE + 0x362A))
#define AUDENC_ANA_CON7 ((unsigned int)(PMIC_REG_BASE + 0x362C))
#define AUDENC_ANA_CON8 ((unsigned int)(PMIC_REG_BASE + 0x362E))
#define AUDENC_ANA_CON9 ((unsigned int)(PMIC_REG_BASE + 0x3630))
#define AUDENC_ANA_CON10 ((unsigned int)(PMIC_REG_BASE + 0x3632))
#define AUDENC_ANA_CON11 ((unsigned int)(PMIC_REG_BASE + 0x3634))
#define AUDENC_ANA_CON12 ((unsigned int)(PMIC_REG_BASE + 0x3636))
#define AUDENC_ANA_CON13 ((unsigned int)(PMIC_REG_BASE + 0x3638))
#define AUDENC_ANA_CON14 ((unsigned int)(PMIC_REG_BASE + 0x363A))
#define AUDENC_ANA_CON15 ((unsigned int)(PMIC_REG_BASE + 0x363C))
#define AUDENC_ANA_CON16 ((unsigned int)(PMIC_REG_BASE + 0x363E))

/* ACCDET register */
#define ACCDET_CON14 ((unsigned int)(PMIC_REG_BASE + 0x1C1C))

/* PMIC GPIO register */
#define GPIO_MODE0 ((unsigned int)(PMIC_REG_BASE + 0x2422))
#define GPIO_MODE1 ((unsigned int)(PMIC_REG_BASE + 0x2428))
#define GPIO_MODE2 ((unsigned int)(PMIC_REG_BASE + 0x242E))

/* DCXO register */
#define DCXO_CW14 ((unsigned int)(PMIC_REG_BASE + 0x2024))

/* Efuse register */
#define OTP_DOUT_816_831 ((unsigned int)(PMIC_REG_BASE + 0x1E84))
#define OTP_DOUT_1136_1151 ((unsigned int)(PMIC_REG_BASE + 0x1EAC))
#define OTP_DOUT_1152_1167 ((unsigned int)(PMIC_REG_BASE + 0x1EAE))
#define OTP_DOUT_1440_1455 ((unsigned int)(PMIC_REG_BASE + 0x1ED2))

/* Buck DIG register - Voltage vote */
#define RG_BUCK_VS1_VOTER_EN ((unsigned int)(PMIC_REG_BASE + 0x1134))
#define RG_BUCK_VS1_VOTER_EN_SET ((unsigned int)(PMIC_REG_BASE + 0x1136))
#define RG_BUCK_VS1_VOTER_EN_CLR ((unsigned int)(PMIC_REG_BASE + 0x1138))

/* AUXADC HP Impedance debug */
#define AUXADC_IMPEDANCE ((unsigned int)(PMIC_REG_BASE + 0x3300))

/* AUXADC use large scale for DC trim */
#define AUXADC_CON2 ((unsigned int)(PMIC_REG_BASE + 0x32B4))

void Ana_Set_Reg(unsigned int offset, unsigned int value, unsigned int mask);
unsigned int Ana_Get_Reg(unsigned int offset);

/* Mtkaif Calibration*/
#define MTKAIF_SCENARIO1_DEFAULT (15)
#define MTKAIF_SCENARIO2_SHIFT (11)

/* for debug usage */
void Ana_Log_Print(void);
void Ana_NleLog_Print(void);

int Ana_Debug_Read(char *buffer, const int size);

#endif
