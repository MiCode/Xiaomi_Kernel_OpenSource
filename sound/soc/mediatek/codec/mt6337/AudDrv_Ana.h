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
/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_Ana.h
 *
 * Project:
 * --------
 *   MT6337  Audio Driver Ana
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 *   Chipeng Chang (mtk02308)
 *   Chien-Wei Hsu (mtk10177)
 *------------------------------------------------------------------------------
 *
 *
 *******************************************************************************/

#ifndef _AUDDRV_ANA_H_
#define _AUDDRV_ANA_H_

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/
#include "AudDrv_Def.h"

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/


/*****************************************************************************
 *                         M A C R O
 *****************************************************************************/

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/
typedef	uint8 kal_uint8;
typedef	int8 kal_int8;
typedef	uint32 kal_uint32;
typedef	int32 kal_int32;
typedef	uint64 kal_uint64;
typedef	int64 kal_int64;

#define PMIC_REG_BASE                    (0x8000)
#define PMIC_AUDIO_SYS_TOP_REG_BASE      (0xE000)

/*PMIC Digital Audio Register*/
#define AFE_UL_DL_CON0               ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0000))
#define AFE_DL_SRC2_CON0_H           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0002))
#define AFE_DL_SRC2_CON0_L           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0004))
#define AFE_DL_SDM_CON0              ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0006))
#define AFE_DL_SDM_CON1              ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0008))
#define AFE_UL_SRC_CON0_H            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x000a))
#define AFE_UL_SRC_CON0_L            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x000c))
#define AFE_UL_SRC_CON1_H            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x000e))
#define AFE_UL_SRC_CON1_L            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0010))
#define PMIC_AFE_TOP_CON0            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0012))
#define AFE_AUDIO_TOP_CON0           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0014))
#define AFE_DL_SRC_MON0              ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0016))
#define AFE_DL_SDM_TEST0             ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0018))
#define AFE_MON_DEBUG0               ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x001a))
#define AFUNC_AUD_CON0               ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x001c))
#define AFUNC_AUD_CON1               ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x001e))
#define AFUNC_AUD_CON2               ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0020))
#define AFUNC_AUD_CON3               ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0022))
#define AFUNC_AUD_CON4               ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0024))
#define AFUNC_AUD_MON0               ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0026))
#define AFUNC_AUD_MON1               ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0028))
#define AUDRC_TUNE_MON0              ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x002a))
#define AFE_UP8X_FIFO_CFG0           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x002c))
#define AFE_UP8X_FIFO_LOG_MON0       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x002e))
#define AFE_UP8X_FIFO_LOG_MON1       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0030))
#define AFE_DL_DC_COMP_CFG0          ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0032))
#define AFE_DL_DC_COMP_CFG1          ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0034))
#define AFE_DL_DC_COMP_CFG2          ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0036))
#define AFE_PMIC_NEWIF_CFG0          ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0038))
#define AFE_PMIC_NEWIF_CFG1          ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x003a))
#define AFE_PMIC_NEWIF_CFG2          ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x003c))
#define AFE_PMIC_NEWIF_CFG3          ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x003e))
#define AFE_SGEN_CFG0                ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0040))
#define AFE_SGEN_CFG1                ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0042))
#define AFE_ADDA2_UP8X_FIFO_LOG_MON0 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x004c))
#define AFE_ADDA2_UP8X_FIFO_LOG_MON1 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x004e))
#define AFE_ADDA2_PMIC_NEWIF_CFG0    ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0050))
#define AFE_ADDA2_PMIC_NEWIF_CFG1    ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0052))
#define AFE_ADDA2_PMIC_NEWIF_CFG2    ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0054))
#define AFE_ADC_ASYNC_FIFO_CFG       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0058))
#define AFE_VOW_TOP                  ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0070))
#define AFE_VOW_CFG0                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0072))
#define AFE_VOW_CFG1                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0074))
#define AFE_VOW_CFG2                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0076))
#define AFE_VOW_CFG3                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0078))
#define AFE_VOW_CFG4                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x007a))
#define AFE_VOW_CFG5                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x007c))
#define AFE_VOW_CFG6                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x007e))
#define AFE_VOW_MON0                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0080))
#define AFE_VOW_MON1                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0082))
#define AFE_VOW_MON2                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0084))
#define AFE_VOW_MON3                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0086))
#define AFE_VOW_MON4                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0088))
#define AFE_VOW_TGEN_CFG0            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x008a))
#define AFE_VOW_POSDIV_CFG0          ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x008c))
#define AFE_VOW_HPF_CFG0             ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x008e))
#define AFE_DCCLK_CFG0               ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0090))
#define AFE_DCCLK_CFG1               ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0092))
#define AFE_HPANC_CFG0               ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0094))
#define AFE_NCP_CFG0                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0096))
#define AFE_NCP_CFG1                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0098))
#define AFE_VOW_MON5                 ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x009C))
#define AFE_VOW_PERIODIC_CFG0        ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00a0))
#define AFE_VOW_PERIODIC_CFG1        ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00a2))
#define AFE_VOW_PERIODIC_CFG2        ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00a4))
#define AFE_VOW_PERIODIC_CFG3        ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00a6))
#define AFE_VOW_PERIODIC_CFG4        ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00a8))
#define AFE_VOW_PERIODIC_CFG5        ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00aa))
#define AFE_VOW_PERIODIC_CFG6        ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00ac))
#define AFE_VOW_PERIODIC_CFG7        ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00ae))
#define AFE_VOW_PERIODIC_CFG8        ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00b0))
#define AFE_VOW_PERIODIC_CFG9        ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00b2))
#define AFE_VOW_PERIODIC_CFG10       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00b4))
#define AFE_VOW_PERIODIC_CFG11       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00b6))
#define AFE_VOW_PERIODIC_CFG12       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00b8))
#define AFE_VOW_PERIODIC_CFG13       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00ba))
#define AFE_VOW_PERIODIC_CFG14       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00bc))
#define AFE_VOW_PERIODIC_CFG15       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00be))
#define AFE_VOW_PERIODIC_CFG16       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00c0))
#define AFE_VOW_PERIODIC_CFG17       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00c2))
#define AFE_VOW_PERIODIC_CFG18       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00c4))
#define AFE_VOW_PERIODIC_CFG19       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00c6))
#define AFE_VOW_PERIODIC_CFG20       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00c8))
#define AFE_VOW_PERIODIC_CFG21       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00ca))
#define AFE_VOW_PERIODIC_CFG22       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00cc))
#define AFE_VOW_PERIODIC_CFG23       ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00ce))
#define AFE_VOW_PERIODIC_MON0        ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00d0))
#define AFE_VOW_PERIODIC_MON1        ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00d2))
#define AFE_ADDA6_UL_SRC_CON0_H      ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00f0))
#define AFE_ADDA6_UL_SRC_CON0_L      ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00f2))
#define AFE_ADDA6_UL_SRC_CON1_H      ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00f4))
#define AFE_ADDA6_UL_SRC_CON1_L      ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00f6))
#define AFE_ADDA6_UL_SRC_TOP_CON0_H  ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00f8))
#define AFE_ADDA6_UL_SRC_TOP_CON0_L  ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00fa))
#define AFE_ADDA6_PMIC_NEWIF_CFG1    ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x00fe))
#define AFE_AMIC_ARRAY_CFG           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0102))
#define AFE_DMIC_ARRAY_CFG           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0104))
#define AUDRC_TUNE_MON1              ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0106))
#define AFE_DL_NLE_R_CFG0            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x010a))
#define AFE_DL_NLE_R_CFG1            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x010c))
#define AFE_DL_NLE_R_CFG2            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x010e))
#define AFE_DL_NLE_R_CFG3            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0110))
#define AFE_DL_NLE_L_CFG0            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0112))
#define AFE_DL_NLE_L_CFG1            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0114))
#define AFE_DL_NLE_L_CFG2            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0116))
#define AFE_DL_NLE_L_CFG3            ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0118))
#define AFE_RGS_NLE_R_CFG0           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x011a))
#define AFE_RGS_NLE_R_CFG1           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x011c))
#define AFE_RGS_NLE_R_CFG2           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x011e))
#define AFE_RGS_NLE_R_CFG3           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0120))
#define AFE_RGS_NLE_L_CFG0           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0122))
#define AFE_RGS_NLE_L_CFG1           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0124))
#define AFE_RGS_NLE_L_CFG2           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0126))
#define AFE_RGS_NLE_L_CFG3           ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0128))
#define AUD_TOP_CFG                  ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0130))
#define AFE_DL_DC_COMP_CFG3          ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0136))
#define AFE_DL_DC_COMP_CFG4          ((UINT32)(PMIC_AUDIO_SYS_TOP_REG_BASE + 0x0138))

/*
#define HWCID               ((UINT32)(PMIC_REG_BASE+0x0200))
#define SWCID               ((UINT32)(PMIC_REG_BASE+0x0202))
#define TOP_CON             ((UINT32)(PMIC_REG_BASE+0x0204))
#define TEST_OUT            ((UINT32)(PMIC_REG_BASE+0x0206))
#define TEST_CON0           ((UINT32)(PMIC_REG_BASE+0x0208))
#define TEST_CON1           ((UINT32)(PMIC_REG_BASE+0x020A))

#define DRV_CON2            ((UINT32)(PMIC_REG_BASE+0x0230))

#define TOP_STATUS          ((UINT32)(PMIC_REG_BASE+0x0234))
*/
#define TOP_CKPDN_CON0        ((UINT32)(PMIC_REG_BASE + 0x0200))
#define TOP_CKPDN_CON0_SET    ((UINT32)(PMIC_REG_BASE + 0x0202))
#define TOP_CKPDN_CON0_CLR    ((UINT32)(PMIC_REG_BASE + 0x0204))
#define TOP_CKPDN_CON1        ((UINT32)(PMIC_REG_BASE + 0x0206))
#define TOP_CKPDN_CON1_SET    ((UINT32)(PMIC_REG_BASE + 0x0208))
#define TOP_CKPDN_CON1_CLR    ((UINT32)(PMIC_REG_BASE + 0x020a))
#define TOP_CKSEL_CON0        ((UINT32)(PMIC_REG_BASE + 0x020c))
#define TOP_CKSEL_CON0_SET    ((UINT32)(PMIC_REG_BASE + 0x020e))
#define TOP_CKSEL_CON0_CLR    ((UINT32)(PMIC_REG_BASE + 0x0210))
#define TOP_CKDIVSEL_CON0     ((UINT32)(PMIC_REG_BASE + 0x0212))
#define TOP_CKDIVSEL_CON0_SET ((UINT32)(PMIC_REG_BASE + 0x0214))
#define TOP_CKDIVSEL_CON0_CLR ((UINT32)(PMIC_REG_BASE + 0x0216))
#define TOP_CKHWEN_CON0       ((UINT32)(PMIC_REG_BASE + 0x0218))
#define TOP_CKHWEN_CON0_SET   ((UINT32)(PMIC_REG_BASE + 0x021a))
#define TOP_CKHWEN_CON0_CLR   ((UINT32)(PMIC_REG_BASE + 0x021c))
#define TOP_CKHWEN_CON1       ((UINT32)(PMIC_REG_BASE + 0x021e))
#define TOP_CKTST_CON0        ((UINT32)(PMIC_REG_BASE + 0x0220))
#define TOP_CKTST_CON1        ((UINT32)(PMIC_REG_BASE + 0x0222))
#define TOP_CLKSQ             ((UINT32)(PMIC_REG_BASE + 0x0224))
#define TOP_CLKSQ_SET         ((UINT32)(PMIC_REG_BASE + 0x0226))
#define TOP_CLKSQ_CLR         ((UINT32)(PMIC_REG_BASE + 0x0228))
#define TOP_CLKSQ_RTC         ((UINT32)(PMIC_REG_BASE + 0x022a))
#define TOP_CLKSQ_RTC_SET     ((UINT32)(PMIC_REG_BASE + 0x022c))
#define TOP_CLKSQ_RTC_CLR     ((UINT32)(PMIC_REG_BASE + 0x022e))
#define TOP_CLK_TRIM          ((UINT32)(PMIC_REG_BASE + 0x0230))
#define TOP_CLK_CON0          ((UINT32)(PMIC_REG_BASE + 0x0232))
#define TOP_CLK_CON0_SET      ((UINT32)(PMIC_REG_BASE + 0x0234))
#define TOP_CLK_CON0_CLR      ((UINT32)(PMIC_REG_BASE + 0x0236))

#define TOP_RST_CON0        ((UINT32)(PMIC_REG_BASE + 0x0400))
#define TOP_RST_CON0_SET    ((UINT32)(PMIC_REG_BASE + 0x0402))
#define TOP_RST_CON0_CLR    ((UINT32)(PMIC_REG_BASE + 0x0404))
#define TOP_RST_CON1        ((UINT32)(PMIC_REG_BASE + 0x0406))
#define TOP_RST_CON1_SET    ((UINT32)(PMIC_REG_BASE + 0x0408))
#define TOP_RST_CON1_CLR    ((UINT32)(PMIC_REG_BASE + 0x040a))
#define TOP_RST_CON2        ((UINT32)(PMIC_REG_BASE + 0x040c))
#define TOP_RST_MISC        ((UINT32)(PMIC_REG_BASE + 0x040e))
#define TOP_RST_MISC_SET    ((UINT32)(PMIC_REG_BASE + 0x0410))
#define TOP_RST_MISC_CLR    ((UINT32)(PMIC_REG_BASE + 0x0412))
#define TOP_RST_STATUS      ((UINT32)(PMIC_REG_BASE + 0x0414))
#define TOP_RST_STATUS_SET  ((UINT32)(PMIC_REG_BASE + 0x0416))
#define TOP_RST_STATUS_CLR  ((UINT32)(PMIC_REG_BASE + 0x0418))

#define INT_CON0                       ((UINT32)(PMIC_REG_BASE+0x0600))

#define ZCD_CON0                       ((UINT32)(PMIC_REG_BASE+0x1A00))
#define ZCD_CON1                       ((UINT32)(PMIC_REG_BASE+0x1A02))
#define ZCD_CON2                       ((UINT32)(PMIC_REG_BASE+0x1A04))
#define ZCD_CON3                       ((UINT32)(PMIC_REG_BASE+0x1A06))
#define ZCD_CON4                       ((UINT32)(PMIC_REG_BASE+0x1A08))
#define ZCD_CON5                       ((UINT32)(PMIC_REG_BASE+0x1A0A))

#define LDO_VA18_CON0       ((UINT32)(PMIC_REG_BASE + 0x1006))
#define LDO_VA18_CON1       ((UINT32)(PMIC_REG_BASE + 0x1014))
#define LDO_VA18_CON2       ((UINT32)(PMIC_REG_BASE + 0x1016))
#define LDO_VA18_CON3       ((UINT32)(PMIC_REG_BASE + 0x1018))
#define LDO_VA25_CON0       ((UINT32)(PMIC_REG_BASE + 0x101A))
#define LDO_VA25_OP_EN      ((UINT32)(PMIC_REG_BASE + 0x101C))

#define TEST_CON0           ((UINT32)(PMIC_REG_BASE+0x000A))
#define DRV_CON2            ((UINT32)(PMIC_REG_BASE+0x0022))
#define DRV_CON3            ((UINT32)(PMIC_REG_BASE+0x0024))

/* PMIC Analog Audio Register */
#define AUDDEC_ANA_CON0        ((UINT32)(PMIC_REG_BASE + 0x1800))
#define AUDDEC_ANA_CON1        ((UINT32)(PMIC_REG_BASE + 0x1802))
#define AUDDEC_ANA_CON2        ((UINT32)(PMIC_REG_BASE + 0x1804))
#define AUDDEC_ANA_CON3        ((UINT32)(PMIC_REG_BASE + 0x1806))
#define AUDDEC_ANA_CON4        ((UINT32)(PMIC_REG_BASE + 0x1808))
#define AUDDEC_ANA_CON5        ((UINT32)(PMIC_REG_BASE + 0x180a))
#define AUDDEC_ANA_CON6        ((UINT32)(PMIC_REG_BASE + 0x180c))
#define AUDDEC_ANA_CON7        ((UINT32)(PMIC_REG_BASE + 0x180e))
#define AUDDEC_ANA_CON8        ((UINT32)(PMIC_REG_BASE + 0x1810))
#define AUDDEC_ANA_CON9        ((UINT32)(PMIC_REG_BASE + 0x1812))
#define AUDDEC_ANA_CON10       ((UINT32)(PMIC_REG_BASE + 0x1814))
#define AUDDEC_ANA_CON11       ((UINT32)(PMIC_REG_BASE + 0x1816))
#define AUDDEC_ANA_CON12       ((UINT32)(PMIC_REG_BASE + 0x1818))
#define AUDDEC_ANA_CON13       ((UINT32)(PMIC_REG_BASE + 0x181A))
#define AUDDEC_ANA_CON14       ((UINT32)(PMIC_REG_BASE + 0x181C))

#define AUDENC_ANA_CON0        ((UINT32)(PMIC_REG_BASE + 0x181E))
#define AUDENC_ANA_CON1        ((UINT32)(PMIC_REG_BASE + 0x1820))
#define AUDENC_ANA_CON2        ((UINT32)(PMIC_REG_BASE + 0x1822))
#define AUDENC_ANA_CON3        ((UINT32)(PMIC_REG_BASE + 0x1824))
#define AUDENC_ANA_CON4        ((UINT32)(PMIC_REG_BASE + 0x1826))
#define AUDENC_ANA_CON5        ((UINT32)(PMIC_REG_BASE + 0x1828))
#define AUDENC_ANA_CON6        ((UINT32)(PMIC_REG_BASE + 0x182A))
#define AUDENC_ANA_CON7        ((UINT32)(PMIC_REG_BASE + 0x182C))
#define AUDENC_ANA_CON8        ((UINT32)(PMIC_REG_BASE + 0x182E))
#define AUDENC_ANA_CON9        ((UINT32)(PMIC_REG_BASE + 0x1830))
#define AUDENC_ANA_CON10       ((UINT32)(PMIC_REG_BASE + 0x1832))
#define AUDENC_ANA_CON11       ((UINT32)(PMIC_REG_BASE + 0x1834))
#define AUDENC_ANA_CON12       ((UINT32)(PMIC_REG_BASE + 0x1836))
#define AUDENC_ANA_CON13       ((UINT32)(PMIC_REG_BASE + 0x1838))
#define AUDENC_ANA_CON14       ((UINT32)(PMIC_REG_BASE + 0x183A))
#define AUDENC_ANA_CON15       ((UINT32)(PMIC_REG_BASE + 0x183C))
#define AUDENC_ANA_CON16       ((UINT32)(PMIC_REG_BASE + 0x183E))
#define AUDENC_ANA_CON17       ((UINT32)(PMIC_REG_BASE + 0x1840))
#define AUDENC_ANA_CON18       ((UINT32)(PMIC_REG_BASE + 0x1842))
#define AUDENC_ANA_CON19       ((UINT32)(PMIC_REG_BASE + 0x1844))
#define AUDENC_ANA_CON20       ((UINT32)(PMIC_REG_BASE + 0x1846))
#define AUDENC_ANA_CON21       ((UINT32)(PMIC_REG_BASE + 0x1848))
#define AUDENC_ANA_CON22       ((UINT32)(PMIC_REG_BASE + 0x184A))
#define AUDENC_ANA_CON23       ((UINT32)(PMIC_REG_BASE + 0x184C))
#define AUDENC_ANA_CON24       ((UINT32)(PMIC_REG_BASE + 0x184E))
#define AUDENC_ANA_CON25       ((UINT32)(PMIC_REG_BASE + 0x1850))
#define AUDENC_ANA_CON26       ((UINT32)(PMIC_REG_BASE + 0x1852))

/* PMIC GPIO register */
#define GPIO_MODE0          ((UINT32)(PMIC_REG_BASE + 0x1C22))
#define GPIO_MODE1          ((UINT32)(PMIC_REG_BASE + 0x1C28))
#define GPIO_MODE2          ((UINT32)(PMIC_REG_BASE + 0x1C2E))

void Ana_Set_Reg(uint32 offset, uint32 value, uint32 mask);
uint32 Ana_Get_Reg(uint32 offset);

/* for debug usage */
void Ana_Log_Print(void);

int Ana_Debug_Read(char *buffer, const int size);

#endif
