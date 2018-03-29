/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program
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
 *   MT6797  Audio Driver Ana
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 *   Chipeng Chang (mtk02308)
 *
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

#include "AudDrv_Common.h"
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
#define PMIC_REG_BASE                    (0x0000)
#define AFE_UL_DL_CON0               ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0000))
#define AFE_DL_SRC2_CON0_H           ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0002))
#define AFE_DL_SRC2_CON0_L           ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0004))
#define AFE_DL_SDM_CON0              ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0006))
#define AFE_DL_SDM_CON1              ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0008))
#define AFE_UL_SRC_CON0_H            ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x000a))
#define AFE_UL_SRC_CON0_L            ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x000c))
#define AFE_UL_SRC_CON1_H            ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x000e))
#define AFE_UL_SRC_CON1_L            ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0010))
#define PMIC_AFE_TOP_CON0            ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0012))
#define AFE_AUDIO_TOP_CON0           ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0014))
#define AFE_DL_SRC_MON0              ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0016))
#define AFE_DL_SDM_TEST0             ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0018))
#define AFE_MON_DEBUG0               ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x001a))
#define AFUNC_AUD_CON0               ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x001c))
#define AFUNC_AUD_CON1               ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x001e))
#define AFUNC_AUD_CON2               ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0020))
#define AFUNC_AUD_CON3               ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0022))
#define AFUNC_AUD_CON4               ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0024))
#define AFUNC_AUD_MON0               ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0026))
#define AFUNC_AUD_MON1               ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0028))
#define AUDRC_TUNE_MON0              ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x002a))
#define AFE_UP8X_FIFO_CFG0           ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x002c))
#define AFE_UP8X_FIFO_LOG_MON0       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x002e))
#define AFE_UP8X_FIFO_LOG_MON1       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0030))
#define AFE_DL_DC_COMP_CFG0          ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0032))
#define AFE_DL_DC_COMP_CFG1          ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0034))
#define AFE_DL_DC_COMP_CFG2          ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0036))
#define AFE_PMIC_NEWIF_CFG0          ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0038))
#define AFE_PMIC_NEWIF_CFG1          ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x003a))
#define AFE_PMIC_NEWIF_CFG2          ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x003c))
#define AFE_PMIC_NEWIF_CFG3          ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x003e))
#define AFE_SGEN_CFG0                ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0040))
#define AFE_SGEN_CFG1                ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0042))
#define AFE_ADDA2_UP8X_FIFO_LOG_MON0 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x004c))
#define AFE_ADDA2_UP8X_FIFO_LOG_MON1 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x004e))
#define AFE_ADDA2_PMIC_NEWIF_CFG0    ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0050))
#define AFE_ADDA2_PMIC_NEWIF_CFG1    ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0052))
#define AFE_ADDA2_PMIC_NEWIF_CFG2    ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0054))
#define AFE_VOW_TOP                  ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0070))
#define AFE_VOW_CFG0                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0072))
#define AFE_VOW_CFG1                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0074))
#define AFE_VOW_CFG2                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0076))
#define AFE_VOW_CFG3                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0078))
#define AFE_VOW_CFG4                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x007a))
#define AFE_VOW_CFG5                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x007c))
#define AFE_VOW_MON0                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x007e))
#define AFE_VOW_MON1                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0080))
#define AFE_VOW_MON2                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0082))
#define AFE_VOW_MON3                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0084))
#define AFE_VOW_MON4                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0086))
#define AFE_VOW_MON5                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0088))
#define AFE_VOW_TGEN_CFG0            ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x008a))
#define AFE_VOW_POSDIV_CFG0          ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x008c))
#define AFE_VOW_HPF_CFG0             ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x008e))
#define AFE_DCCLK_CFG0               ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0090))
#define AFE_DCCLK_CFG1               ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0092))
#define AFE_HPANC_CFG0               ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0094))
#define AFE_NCP_CFG0                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0096))
#define AFE_NCP_CFG1                 ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x0098))
#define AFE_VOW_PERIODIC_CFG0        ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00a0))
#define AFE_VOW_PERIODIC_CFG1        ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00a2))
#define AFE_VOW_PERIODIC_CFG2        ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00a4))
#define AFE_VOW_PERIODIC_CFG3        ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00a6))
#define AFE_VOW_PERIODIC_CFG4        ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00a8))
#define AFE_VOW_PERIODIC_CFG5        ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00aa))
#define AFE_VOW_PERIODIC_CFG6        ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00ac))
#define AFE_VOW_PERIODIC_CFG7        ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00ae))
#define AFE_VOW_PERIODIC_CFG8        ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00b0))
#define AFE_VOW_PERIODIC_CFG9        ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00b2))
#define AFE_VOW_PERIODIC_CFG10       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00b4))
#define AFE_VOW_PERIODIC_CFG11       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00b6))
#define AFE_VOW_PERIODIC_CFG12       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00b8))
#define AFE_VOW_PERIODIC_CFG13       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00ba))
#define AFE_VOW_PERIODIC_CFG14       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00bc))
#define AFE_VOW_PERIODIC_CFG15       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00be))
#define AFE_VOW_PERIODIC_CFG16       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00c0))
#define AFE_VOW_PERIODIC_CFG17       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00c2))
#define AFE_VOW_PERIODIC_CFG18       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00c4))
#define AFE_VOW_PERIODIC_CFG19       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00c6))
#define AFE_VOW_PERIODIC_CFG20       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00c8))
#define AFE_VOW_PERIODIC_CFG21       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00ca))
#define AFE_VOW_PERIODIC_CFG22       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00cc))
#define AFE_VOW_PERIODIC_CFG23       ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00ce))
#define AFE_VOW_PERIODIC_MON0        ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00d0))
#define AFE_VOW_PERIODIC_MON1        ((UINT32)(PMIC_REG_BASE + 0x2000 + 0x00d2))


/* not used */
#define STRUP_CON0  ((UINT32)(PMIC_REG_BASE + 0x0000))
#define STRUP_CON1  ((UINT32)(PMIC_REG_BASE + 0x0002))
#define STRUP_CON2  ((UINT32)(PMIC_REG_BASE + 0x0004))
#define STRUP_CON3  ((UINT32)(PMIC_REG_BASE + 0x0006))
#define STRUP_CON4  ((UINT32)(PMIC_REG_BASE + 0x0008))
#define STRUP_CON5  ((UINT32)(PMIC_REG_BASE + 0x000a))
#define STRUP_CON6  ((UINT32)(PMIC_REG_BASE + 0x000c))
#define STRUP_CON7  ((UINT32)(PMIC_REG_BASE + 0x000e))
#define STRUP_CON8  ((UINT32)(PMIC_REG_BASE + 0x0010))
#define STRUP_CON9  ((UINT32)(PMIC_REG_BASE + 0x0012))
#define STRUP_CON10 ((UINT32)(PMIC_REG_BASE + 0x0014))
#define STRUP_CON11 ((UINT32)(PMIC_REG_BASE + 0x0016))
#define STRUP_CON12 ((UINT32)(PMIC_REG_BASE + 0x0018))
#define STRUP_CON13 ((UINT32)(PMIC_REG_BASE + 0x001a))
#define STRUP_CON14 ((UINT32)(PMIC_REG_BASE + 0x001c))
#define STRUP_CON15 ((UINT32)(PMIC_REG_BASE + 0x001e))
#define STRUP_CON16 ((UINT32)(PMIC_REG_BASE + 0x0020))
#define STRUP_CON17 ((UINT32)(PMIC_REG_BASE + 0x0022))
#define STRUP_CON18 ((UINT32)(PMIC_REG_BASE + 0x0024))
#define STRUP_CON19 ((UINT32)(PMIC_REG_BASE + 0x0026))
#define STRUP_CON20 ((UINT32)(PMIC_REG_BASE + 0x0028))
#define STRUP_CON21 ((UINT32)(PMIC_REG_BASE + 0x002a))
#define STRUP_CON22 ((UINT32)(PMIC_REG_BASE + 0x002c))
#define STRUP_CON23 ((UINT32)(PMIC_REG_BASE + 0x002e))
#define STRUP_ANA_CON0      ((UINT32)(PMIC_REG_BASE+0x0030))
#define STRUP_ANA_CON1      ((UINT32)(PMIC_REG_BASE+0x0032))
/* not used end */

#define HWCID               ((UINT32)(PMIC_REG_BASE+0x0200))
#define SWCID               ((UINT32)(PMIC_REG_BASE+0x0202))
#define TOP_CON             ((UINT32)(PMIC_REG_BASE+0x0204))
#define TEST_OUT            ((UINT32)(PMIC_REG_BASE+0x0206))
#define TEST_CON0           ((UINT32)(PMIC_REG_BASE+0x0208))
#define TEST_CON1           ((UINT32)(PMIC_REG_BASE+0x020A))

#define DRV_CON2            ((UINT32)(PMIC_REG_BASE+0x0230))

#define TOP_STATUS          ((UINT32)(PMIC_REG_BASE+0x0234))

#define TOP_CKPDN_CON0      ((UINT32)(PMIC_REG_BASE+0x023A))
#define TOP_CKPDN_CON0_SET  ((UINT32)(PMIC_REG_BASE+0x023C))
#define TOP_CKPDN_CON0_CLR  ((UINT32)(PMIC_REG_BASE+0x023E))
#define TOP_CKPDN_CON1      ((UINT32)(PMIC_REG_BASE+0x0240))
#define TOP_CKPDN_CON1_SET  ((UINT32)(PMIC_REG_BASE+0x0242))
#define TOP_CKPDN_CON1_CLR  ((UINT32)(PMIC_REG_BASE+0x0244))
#define TOP_CKPDN_CON2      ((UINT32)(PMIC_REG_BASE+0x0246))
#define TOP_CKPDN_CON2_SET  ((UINT32)(PMIC_REG_BASE+0x0248))
#define TOP_CKPDN_CON2_CLR  ((UINT32)(PMIC_REG_BASE+0x024A))
#define TOP_CKPDN_CON3      ((UINT32)(PMIC_REG_BASE+0x024C))
#define TOP_CKPDN_CON3_SET  ((UINT32)(PMIC_REG_BASE+0x024E))
#define TOP_CKPDN_CON3_CLR  ((UINT32)(PMIC_REG_BASE+0x0250))
#define TOP_CKPDN_CON4      ((UINT32)(PMIC_REG_BASE+0x0252))
#define TOP_CKPDN_CON4_SET  ((UINT32)(PMIC_REG_BASE+0x0254))
#define TOP_CKPDN_CON4_CLR  ((UINT32)(PMIC_REG_BASE+0x0256))
#define TOP_CKPDN_CON5      ((UINT32)(PMIC_REG_BASE+0x0258))
#define TOP_CKPDN_CON5_SET  ((UINT32)(PMIC_REG_BASE+0x025A))
#define TOP_CKPDN_CON5_CLR  ((UINT32)(PMIC_REG_BASE+0x025C))

#define TOP_CKSEL_CON0      ((UINT32)(PMIC_REG_BASE+0x025E))
#define TOP_CKSEL_CON0_SET  ((UINT32)(PMIC_REG_BASE+0x0260))
#define TOP_CKSEL_CON0_CLR  ((UINT32)(PMIC_REG_BASE+0x0262))
#define TOP_CKSEL_CON1      ((UINT32)(PMIC_REG_BASE+0x0264))
#define TOP_CKSEL_CON1_SET  ((UINT32)(PMIC_REG_BASE+0x0266))
#define TOP_CKSEL_CON1_CLR  ((UINT32)(PMIC_REG_BASE+0x0268))
#define TOP_CKSEL_CON2      ((UINT32)(PMIC_REG_BASE+0x026A))
#define TOP_CKSEL_CON2_SET  ((UINT32)(PMIC_REG_BASE+0x026C))
#define TOP_CKSEL_CON2_CLR  ((UINT32)(PMIC_REG_BASE+0x026E))
#define TOP_CKSEL_CON3      ((UINT32)(PMIC_REG_BASE+0x0270))
#define TOP_CKSEL_CON3_SET  ((UINT32)(PMIC_REG_BASE+0x0272))
#define TOP_CKSEL_CON3_CLR  ((UINT32)(PMIC_REG_BASE+0x0274))

#define TOP_CKDIVSEL_CON0     ((UINT32)(PMIC_REG_BASE+0x0276))
#define TOP_CKDIVSEL_CON0_SET ((UINT32)(PMIC_REG_BASE+0x0278))
#define TOP_CKDIVSEL_CON0_CLR ((UINT32)(PMIC_REG_BASE+0x027A))
#define TOP_CKDIVSEL_CON1     ((UINT32)(PMIC_REG_BASE+0x027C))
#define TOP_CKDIVSEL_CON1_SET ((UINT32)(PMIC_REG_BASE+0x027E))
#define TOP_CKDIVSEL_CON1_CLR ((UINT32)(PMIC_REG_BASE+0x0280))

#define TOP_CKHWEN_CON0      ((UINT32)(PMIC_REG_BASE+0x0282))
#define TOP_CKHWEN_CON0_SET  ((UINT32)(PMIC_REG_BASE+0x0284))
#define TOP_CKHWEN_CON0_CLR  ((UINT32)(PMIC_REG_BASE+0x0286))
#define TOP_CKHWEN_CON1      ((UINT32)(PMIC_REG_BASE+0x0288))
#define TOP_CKHWEN_CON1_SET  ((UINT32)(PMIC_REG_BASE+0x028A))
#define TOP_CKHWEN_CON1_CLR  ((UINT32)(PMIC_REG_BASE+0x028C))
#define TOP_CKHWEN_CON2      ((UINT32)(PMIC_REG_BASE+0x028E))
#define TOP_CKHWEN_CON2_SET  ((UINT32)(PMIC_REG_BASE+0x0290))
#define TOP_CKHWEN_CON2_CLR  ((UINT32)(PMIC_REG_BASE+0x0292))

#define TOP_CKTST_CON0      ((UINT32)(PMIC_REG_BASE+0x0294))
#define TOP_CKTST_CON1      ((UINT32)(PMIC_REG_BASE+0x0296))
#define TOP_CKTST_CON2      ((UINT32)(PMIC_REG_BASE+0x0298))
#define TOP_CLKSQ           ((UINT32)(PMIC_REG_BASE+0x029A))
#define TOP_CLKSQ_SET       ((UINT32)(PMIC_REG_BASE+0x029C))
#define TOP_CLKSQ_CLR       ((UINT32)(PMIC_REG_BASE+0x029E))
#define TOP_CLKSQ_RTC       ((UINT32)(PMIC_REG_BASE+0x02A0))
#define TOP_CLKSQ_RTC_SET   ((UINT32)(PMIC_REG_BASE+0x02A2))
#define TOP_CLKSQ_RTC_CLR   ((UINT32)(PMIC_REG_BASE+0x02A4))
#define TOP_CLK_TRIM        ((UINT32)(PMIC_REG_BASE+0x02A6))

#define TOP_RST_CON0        ((UINT32)(PMIC_REG_BASE+0x02A8))
#define TOP_RST_CON0_SET    ((UINT32)(PMIC_REG_BASE+0x02AA))
#define TOP_RST_CON0_CLR    ((UINT32)(PMIC_REG_BASE+0x02AC))
#define TOP_RST_CON1        ((UINT32)(PMIC_REG_BASE+0x02AE))
#define TOP_RST_CON1_SET    ((UINT32)(PMIC_REG_BASE+0x02B0))
#define TOP_RST_CON1_CLR    ((UINT32)(PMIC_REG_BASE+0x02B2))
#define TOP_RST_CON2        ((UINT32)(PMIC_REG_BASE+0x02B4))
#define TOP_RST_MISC        ((UINT32)(PMIC_REG_BASE+0x02B6))
#define TOP_RST_MISC_SET    ((UINT32)(PMIC_REG_BASE+0x02B8))
#define TOP_RST_MISC_CLR    ((UINT32)(PMIC_REG_BASE+0x02BA))
#define TOP_RST_STATUS      ((UINT32)(PMIC_REG_BASE+0x02BC))
#define TOP_RST_STATUS_SET  ((UINT32)(PMIC_REG_BASE+0x02BE))
#define TOP_RST_STATUS_CLR  ((UINT32)(PMIC_REG_BASE+0x02C0))

#define ZCD_CON0            ((UINT32)(PMIC_REG_BASE+0x0800))
#define ZCD_CON1            ((UINT32)(PMIC_REG_BASE+0x0802))
#define ZCD_CON2            ((UINT32)(PMIC_REG_BASE+0x0804))
#define ZCD_CON3            ((UINT32)(PMIC_REG_BASE+0x0806))
#define ZCD_CON4            ((UINT32)(PMIC_REG_BASE+0x0808))
#define ZCD_CON5            ((UINT32)(PMIC_REG_BASE+0x080A))

#define LDO_VA18_CON0       ((UINT32)(PMIC_REG_BASE + 0x0A00))
#define LDO_VA18_CON1       ((UINT32)(PMIC_REG_BASE + 0x0A02))
#define LDO_VUSB33_CON0     ((UINT32)(PMIC_REG_BASE + 0x0A16))
#define LDO_VUSB33_CON1     ((UINT32)(PMIC_REG_BASE + 0x0A18))


#define AUDDEC_ANA_CON0     ((UINT32)(PMIC_REG_BASE+0x0CF2))
#define AUDDEC_ANA_CON1     ((UINT32)(PMIC_REG_BASE+0x0CF4))
#define AUDDEC_ANA_CON2     ((UINT32)(PMIC_REG_BASE+0x0CF6))
#define AUDDEC_ANA_CON3     ((UINT32)(PMIC_REG_BASE+0x0CF8))
#define AUDDEC_ANA_CON4     ((UINT32)(PMIC_REG_BASE+0x0CFA))
#define AUDDEC_ANA_CON5     ((UINT32)(PMIC_REG_BASE+0x0CFC))
#define AUDDEC_ANA_CON6     ((UINT32)(PMIC_REG_BASE+0x0CFE))
#define AUDDEC_ANA_CON7     ((UINT32)(PMIC_REG_BASE+0x0D00))
#define AUDDEC_ANA_CON8     ((UINT32)(PMIC_REG_BASE+0x0D02))
#define AUDDEC_ANA_CON9     ((UINT32)(PMIC_REG_BASE+0x0D04))
#define AUDDEC_ANA_CON10    ((UINT32)(PMIC_REG_BASE+0x0D06))

#define AUDENC_ANA_CON0     ((UINT32)(PMIC_REG_BASE+0x0D08))
#define AUDENC_ANA_CON1     ((UINT32)(PMIC_REG_BASE+0x0D0A))
#define AUDENC_ANA_CON2     ((UINT32)(PMIC_REG_BASE+0x0D0C))
#define AUDENC_ANA_CON3     ((UINT32)(PMIC_REG_BASE+0x0D0E))
#define AUDENC_ANA_CON4     ((UINT32)(PMIC_REG_BASE+0x0D10))
#define AUDENC_ANA_CON5     ((UINT32)(PMIC_REG_BASE+0x0D12))
#define AUDENC_ANA_CON6     ((UINT32)(PMIC_REG_BASE+0x0D14))
#define AUDENC_ANA_CON7     ((UINT32)(PMIC_REG_BASE+0x0D16))
#define AUDENC_ANA_CON8     ((UINT32)(PMIC_REG_BASE+0x0D18))
#define AUDENC_ANA_CON9     ((UINT32)(PMIC_REG_BASE+0x0D1A))
#define AUDENC_ANA_CON10    ((UINT32)(PMIC_REG_BASE+0x0D1C))
#define AUDENC_ANA_CON11    ((UINT32)(PMIC_REG_BASE+0x0D1E))
#define AUDENC_ANA_CON12    ((UINT32)(PMIC_REG_BASE+0x0D20))
#define AUDENC_ANA_CON13    ((UINT32)(PMIC_REG_BASE+0x0D22))
#define AUDENC_ANA_CON14    ((UINT32)(PMIC_REG_BASE+0x0D24))
#define AUDENC_ANA_CON15    ((UINT32)(PMIC_REG_BASE+0x0D26))
#define AUDENC_ANA_CON16    ((UINT32)(PMIC_REG_BASE+0x0D28))

#define AUDNCP_CLKDIV_CON0  ((UINT32)(PMIC_REG_BASE+0x0D2A))
#define AUDNCP_CLKDIV_CON1  ((UINT32)(PMIC_REG_BASE+0x0D2C))
#define AUDNCP_CLKDIV_CON2  ((UINT32)(PMIC_REG_BASE+0x0D2E))
#define AUDNCP_CLKDIV_CON3  ((UINT32)(PMIC_REG_BASE+0x0D30))
#define AUDNCP_CLKDIV_CON4  ((UINT32)(PMIC_REG_BASE+0x0D32))

#define GPIO_MODE3          ((UINT32)(0x60D0))

#if 1
/* register number */

#else
#include <mach/upmu_hw.h>
#endif

void Ana_Set_Reg(uint32 offset, uint32 value, uint32 mask);
uint32 Ana_Get_Reg(uint32 offset);

/* for debug usage */
void Ana_Log_Print(void);

#endif
