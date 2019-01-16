/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*******************************************************************************
 *
 * Filename:
 * ---------
 *   AudDrv_Ana.h
 *
 * Project:
 * --------
 *   MT6583  Audio Driver Ana
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
 * $Revision: #1 $
 * $Modtime:$
 * $Log:$
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
#define AFE_UL_DL_CON0                                   ((UINT32)(PMIC_REG_BASE+0x2000+0x0))
#define AFE_DL_SRC2_CON0_H                         ((UINT32)(PMIC_REG_BASE+0x2000+0x2))
#define AFE_DL_SRC2_CON0_L                          ((UINT32)(PMIC_REG_BASE+0x2000+0x4))
#define AFE_DL_SDM_CON0                                ((UINT32)(PMIC_REG_BASE+0x2000+0x6))
#define AFE_DL_SDM_CON1                                ((UINT32)(PMIC_REG_BASE+0x2000+0x8))
#define AFE_UL_SRC0_CON0_H                         ((UINT32)(PMIC_REG_BASE+0x2000+0xa))
#define AFE_UL_SRC0_CON0_L                          ((UINT32)(PMIC_REG_BASE+0x2000+0xc))
#define AFE_UL_SRC1_CON0_H                         ((UINT32)(PMIC_REG_BASE+0x2000+0xe))
#define AFE_UL_SRC1_CON0_L                          ((UINT32)(PMIC_REG_BASE+0x2000+0x10))
#define PMIC_AFE_TOP_CON0                            ((UINT32)(PMIC_REG_BASE+0x2000+0x12))
#define AFE_AUDIO_TOP_CON0                         ((UINT32)(PMIC_REG_BASE+0x2000+0x14))
#define AFE_DL_SRC_MON0                                ((UINT32)(PMIC_REG_BASE+0x2000+0x16))
#define AFE_DL_SDM_TEST0                               ((UINT32)(PMIC_REG_BASE+0x2000+0x18))
#define AFE_MON_DEBUG0                                 ((UINT32)(PMIC_REG_BASE+0x2000+0x1a))
#define AFUNC_AUD_CON0                                 ((UINT32)(PMIC_REG_BASE+0x2000+0x1c))
#define AFUNC_AUD_CON1                                 ((UINT32)(PMIC_REG_BASE+0x2000+0x1e))
#define AFUNC_AUD_CON2                                 ((UINT32)(PMIC_REG_BASE+0x2000+0x20))
#define AFUNC_AUD_CON3                                 ((UINT32)(PMIC_REG_BASE+0x2000+0x22))
#define AFUNC_AUD_CON4                                 ((UINT32)(PMIC_REG_BASE+0x2000+0x24))
#define AFUNC_AUD_MON0                                ((UINT32)(PMIC_REG_BASE+0x2000+0x26))
#define AFUNC_AUD_MON1                                ((UINT32)(PMIC_REG_BASE+0x2000+0x28))
#define AUDRC_TUNE_MON0                              ((UINT32)(PMIC_REG_BASE+0x2000+0x2a))
#define AFE_UP8X_FIFO_CFG0                         ((UINT32)(PMIC_REG_BASE+0x2000+0x2c))
#define AFE_UP8X_FIFO_LOG_MON0              ((UINT32)(PMIC_REG_BASE+0x2000+0x2e))
#define AFE_UP8X_FIFO_LOG_MON1              ((UINT32)(PMIC_REG_BASE+0x2000+0x30))
#define AFE_DL_DC_COMP_CFG0                    ((UINT32)(PMIC_REG_BASE+0x2000+0x32))
#define AFE_DL_DC_COMP_CFG1                    ((UINT32)(PMIC_REG_BASE+0x2000+0x34))
#define AFE_DL_DC_COMP_CFG2                    ((UINT32)(PMIC_REG_BASE+0x2000+0x36))
#define AFE_PMIC_NEWIF_CFG0                     ((UINT32)(PMIC_REG_BASE+0x2000+0x38))
#define AFE_PMIC_NEWIF_CFG1                     ((UINT32)(PMIC_REG_BASE+0x2000+0x3a))
#define AFE_PMIC_NEWIF_CFG2                     ((UINT32)(PMIC_REG_BASE+0x2000+0x3c))
#define AFE_PMIC_NEWIF_CFG3                     ((UINT32)(PMIC_REG_BASE+0x2000+0x3e))
#define AFE_SGEN_CFG0                                   ((UINT32)(PMIC_REG_BASE+0x2000+0x40))
#define AFE_SGEN_CFG1                                   ((UINT32)(PMIC_REG_BASE+0x2000+0x42))
#define AFE_VOW_TOP                                        ((UINT32)(PMIC_REG_BASE+0x2000+0x70))
#define AFE_VOW_CFG0                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x72))
#define AFE_VOW_CFG1                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x74))
#define AFE_VOW_CFG2                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x76))
#define AFE_VOW_CFG3                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x78))
#define AFE_VOW_CFG4                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x7a))
#define AFE_VOW_CFG5                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x7c))
#define AFE_VOW_MON0                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x7e))
#define AFE_VOW_MON1                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x80))
#define AFE_VOW_MON2                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x82))
#define AFE_VOW_MON3                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x84))
#define AFE_VOW_MON4                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x86))
#define AFE_VOW_MON5                                    ((UINT32)(PMIC_REG_BASE+0x2000+0x88))
#define AFE_VOW_TGEN_CFG0                               ((UINT32)(PMIC_REG_BASE+0x2000+0x8A))
#define AFE_VOW_POSDIV_CFG0                             ((UINT32)(PMIC_REG_BASE+0x2000+0x8C))
#define AFE_DCCLK_CFG0                                  ((UINT32)(PMIC_REG_BASE+0x2000+0x90))
#define AFE_DCCLK_CFG1                                  ((UINT32)(PMIC_REG_BASE+0x2000+0x92))

// TODO: 6328 analog part

#define STRUP_CON0          ((UINT32)(PMIC_REG_BASE+0x0000))
#define STRUP_CON2          ((UINT32)(PMIC_REG_BASE+0x0002))
#define STRUP_CON3          ((UINT32)(PMIC_REG_BASE+0x0004))
#define STRUP_CON4          ((UINT32)(PMIC_REG_BASE+0x0006))
#define STRUP_CON5          ((UINT32)(PMIC_REG_BASE+0x0008))
#define STRUP_CON6          ((UINT32)(PMIC_REG_BASE+0x000A))
#define STRUP_CON7          ((UINT32)(PMIC_REG_BASE+0x000C))
#define STRUP_CON8          ((UINT32)(PMIC_REG_BASE+0x000E))
#define STRUP_CON9          ((UINT32)(PMIC_REG_BASE+0x0010))
#define STRUP_CON10         ((UINT32)(PMIC_REG_BASE+0x0012))
#define STRUP_CON11         ((UINT32)(PMIC_REG_BASE+0x0014))
#define STRUP_CON12         ((UINT32)(PMIC_REG_BASE+0x0016))
#define STRUP_CON13         ((UINT32)(PMIC_REG_BASE+0x0018))
#define STRUP_CON14         ((UINT32)(PMIC_REG_BASE+0x001A))
#define STRUP_CON15         ((UINT32)(PMIC_REG_BASE+0x001C))
#define STRUP_CON16         ((UINT32)(PMIC_REG_BASE+0x001E))
#define STRUP_CON17         ((UINT32)(PMIC_REG_BASE+0x0020))
#define STRUP_CON18         ((UINT32)(PMIC_REG_BASE+0x0022))
#define STRUP_CON19         ((UINT32)(PMIC_REG_BASE+0x0024))
#define STRUP_CON20         ((UINT32)(PMIC_REG_BASE+0x0026))
#define STRUP_CON21         ((UINT32)(PMIC_REG_BASE+0x0028))
#define STRUP_CON22         ((UINT32)(PMIC_REG_BASE+0x002A))
#define STRUP_CON23         ((UINT32)(PMIC_REG_BASE+0x002C))
#define STRUP_ANA_CON0      ((UINT32)(PMIC_REG_BASE+0x0040))
#define HWCID               ((UINT32)(PMIC_REG_BASE+0x0200))
#define SWCID               ((UINT32)(PMIC_REG_BASE+0x0202))
#define TOP_CON             ((UINT32)(PMIC_REG_BASE+0x0204))
#define TEST_OUT            ((UINT32)(PMIC_REG_BASE+0x0206))
#define TEST_CON0           ((UINT32)(PMIC_REG_BASE+0x0208))
#define TEST_CON1           ((UINT32)(PMIC_REG_BASE+0x020A))
#define TESTMODE_SW         ((UINT32)(PMIC_REG_BASE+0x020C))
#define EN_STATUS0          ((UINT32)(PMIC_REG_BASE+0x020E))
#define EN_STATUS1          ((UINT32)(PMIC_REG_BASE+0x0210))
#define EN_STATUS2          ((UINT32)(PMIC_REG_BASE+0x0212))
#define OCSTATUS0           ((UINT32)(PMIC_REG_BASE+0x0214))
#define OCSTATUS1           ((UINT32)(PMIC_REG_BASE+0x0216))
#define OCSTATUS2           ((UINT32)(PMIC_REG_BASE+0x0218))
#define PGSTATUS            ((UINT32)(PMIC_REG_BASE+0x021C))
#define TOPSTATUS           ((UINT32)(PMIC_REG_BASE+0x0220))
#define TDSEL_CON           ((UINT32)(PMIC_REG_BASE+0x0222))
#define RDSEL_CON           ((UINT32)(PMIC_REG_BASE+0x0224))
#define SMT_CON0            ((UINT32)(PMIC_REG_BASE+0x0226))
#define SMT_CON1            ((UINT32)(PMIC_REG_BASE+0x0228))
#define SMT_CON2            ((UINT32)(PMIC_REG_BASE+0x022A))
#define DRV_CON0            ((UINT32)(PMIC_REG_BASE+0x022C))
#define DRV_CON1            ((UINT32)(PMIC_REG_BASE+0x022E))
#define DRV_CON2            ((UINT32)(PMIC_REG_BASE+0x0230))
#define DRV_CON3            ((UINT32)(PMIC_REG_BASE+0x0232))
#define TOP_STATUS          ((UINT32)(PMIC_REG_BASE+0x0234))
#define TOP_STATUS_SET      ((UINT32)(PMIC_REG_BASE+0x0236))
#define TOP_STATUS_CLR      ((UINT32)(PMIC_REG_BASE+0x0238))
#define RGS_ANA_MON         ((UINT32)(PMIC_REG_BASE+0x023A))
#define TOP_CKPDN_CON0      ((UINT32)(PMIC_REG_BASE+0x023C))
#define TOP_CKPDN_CON0_SET  ((UINT32)(PMIC_REG_BASE+0x023E))
#define TOP_CKPDN_CON0_CLR  ((UINT32)(PMIC_REG_BASE+0x0240))
#define TOP_CKPDN_CON1      ((UINT32)(PMIC_REG_BASE+0x0242))
#define TOP_CKPDN_CON1_SET  ((UINT32)(PMIC_REG_BASE+0x0244))
#define TOP_CKPDN_CON1_CLR  ((UINT32)(PMIC_REG_BASE+0x0246))
#define TOP_CKPDN_CON2      ((UINT32)(PMIC_REG_BASE+0x0248))
#define TOP_CKPDN_CON2_SET  ((UINT32)(PMIC_REG_BASE+0x024A))
#define TOP_CKPDN_CON2_CLR  ((UINT32)(PMIC_REG_BASE+0x024C))
#define TOP_CKPDN_CON3      ((UINT32)(PMIC_REG_BASE+0x024E))
#define TOP_CKPDN_CON3_SET  ((UINT32)(PMIC_REG_BASE+0x0250))
#define TOP_CKPDN_CON3_CLR  ((UINT32)(PMIC_REG_BASE+0x0252))
#define TOP_CKSEL_CON0      ((UINT32)(PMIC_REG_BASE+0x025A))
#define TOP_CKSEL_CON0_SET  ((UINT32)(PMIC_REG_BASE+0x025C))
#define TOP_CKSEL_CON0_CLR  ((UINT32)(PMIC_REG_BASE+0x025E))
#define TOP_CKSEL_CON1      ((UINT32)(PMIC_REG_BASE+0x0260))
#define TOP_CKSEL_CON1_SET  ((UINT32)(PMIC_REG_BASE+0x0262))
#define TOP_CKSEL_CON1_CLR  ((UINT32)(PMIC_REG_BASE+0x0264))
#define TOP_CKSEL_CON2      ((UINT32)(PMIC_REG_BASE+0x0266))
#define TOP_CKSEL_CON2_SET  ((UINT32)(PMIC_REG_BASE+0x0268))
#define TOP_CKSEL_CON2_CLR  ((UINT32)(PMIC_REG_BASE+0x026A))
#define TOP_CKDIVSEL_CON    ((UINT32)(PMIC_REG_BASE+0x026C))
#define TOP_CKDIVSEL_CON_SET ((UINT32)(PMIC_REG_BASE+0x026E))
#define TOP_CKDIVSEL_CON_CLR ((UINT32)(PMIC_REG_BASE+0x0270))
#define TOP_CKHWEN_CON      ((UINT32)(PMIC_REG_BASE+0x0278))
#define TOP_CKHWEN_CON_SET  ((UINT32)(PMIC_REG_BASE+0x027A))
#define TOP_CKHWEN_CON_CLR  ((UINT32)(PMIC_REG_BASE+0x027C))
#define TOP_CKTST_CON0      ((UINT32)(PMIC_REG_BASE+0x0284))
#define TOP_CKTST_CON1      ((UINT32)(PMIC_REG_BASE+0x0286))
#define TOP_CKTST_CON2      ((UINT32)(PMIC_REG_BASE+0x0288))
#define TOP_CLKSQ           ((UINT32)(PMIC_REG_BASE+0x028A))
#define TOP_CLKSQ_SET       ((UINT32)(PMIC_REG_BASE+0x028C))
#define TOP_CLKSQ_CLR       ((UINT32)(PMIC_REG_BASE+0x028E))
#define TOP_CLKSQ_RTC       ((UINT32)(PMIC_REG_BASE+0x0290))
#define TOP_CLKSQ_RTC_SET   ((UINT32)(PMIC_REG_BASE+0x0292))
#define TOP_CLKSQ_RTC_CLR   ((UINT32)(PMIC_REG_BASE+0x0294))
#define TOP_CLK_TRIM        ((UINT32)(PMIC_REG_BASE+0x0296))
#define TOP_RST_CON0        ((UINT32)(PMIC_REG_BASE+0x0298))
#define TOP_RST_CON0_SET    ((UINT32)(PMIC_REG_BASE+0x029A))
#define TOP_RST_CON0_CLR    ((UINT32)(PMIC_REG_BASE+0x029C))
#define TOP_RST_CON1        ((UINT32)(PMIC_REG_BASE+0x029E))
#define TOP_RST_MISC        ((UINT32)(PMIC_REG_BASE+0x02A0))
#define TOP_RST_MISC_SET    ((UINT32)(PMIC_REG_BASE+0x02A2))
#define TOP_RST_MISC_CLR    ((UINT32)(PMIC_REG_BASE+0x02A4))
#define TOP_RST_STATUS      ((UINT32)(PMIC_REG_BASE+0x02A6))
#define TOP_RST_STATUS_SET  ((UINT32)(PMIC_REG_BASE+0x02A8))
#define TOP_RST_STATUS_CLR  ((UINT32)(PMIC_REG_BASE+0x02AA))
#define INT_CON0            ((UINT32)(PMIC_REG_BASE+0x02AC))
#define INT_CON0_SET        ((UINT32)(PMIC_REG_BASE+0x02AE))
#define INT_CON0_CLR        ((UINT32)(PMIC_REG_BASE+0x02B0))
#define INT_CON1            ((UINT32)(PMIC_REG_BASE+0x02B2))
#define INT_CON1_SET        ((UINT32)(PMIC_REG_BASE+0x02B4))
#define INT_CON1_CLR        ((UINT32)(PMIC_REG_BASE+0x02B6))
#define INT_CON2            ((UINT32)(PMIC_REG_BASE+0x02B8))
#define INT_CON2_SET        ((UINT32)(PMIC_REG_BASE+0x02BA))
#define INT_CON2_CLR        ((UINT32)(PMIC_REG_BASE+0x02BC))
#define INT_MISC_CON        ((UINT32)(PMIC_REG_BASE+0x02BE))
#define INT_MISC_CON_SET    ((UINT32)(PMIC_REG_BASE+0x02C0))
#define INT_MISC_CON_CLR    ((UINT32)(PMIC_REG_BASE+0x02C2))
#define INT_STATUS0         ((UINT32)(PMIC_REG_BASE+0x02C4))
#define INT_STATUS1         ((UINT32)(PMIC_REG_BASE+0x02C6))
#define INT_STATUS2         ((UINT32)(PMIC_REG_BASE+0x02C8))
#define OC_GEAR_0           ((UINT32)(PMIC_REG_BASE+0x02CA))
#define FQMTR_CON0          ((UINT32)(PMIC_REG_BASE+0x02CC))
#define FQMTR_CON1          ((UINT32)(PMIC_REG_BASE+0x02CE))
#define FQMTR_CON2          ((UINT32)(PMIC_REG_BASE+0x02D0))
#define RG_SPI_CON          ((UINT32)(PMIC_REG_BASE+0x02D2))
#define DEW_DIO_EN          ((UINT32)(PMIC_REG_BASE+0x02D4))
#define DEW_READ_TEST       ((UINT32)(PMIC_REG_BASE+0x02D6))
#define DEW_WRITE_TEST      ((UINT32)(PMIC_REG_BASE+0x02D8))
#define DEW_CRC_SWRST       ((UINT32)(PMIC_REG_BASE+0x02DA))
#define DEW_CRC_EN          ((UINT32)(PMIC_REG_BASE+0x02DC))
#define DEW_CRC_VAL         ((UINT32)(PMIC_REG_BASE+0x02DE))
#define DEW_DBG_MON_SEL     ((UINT32)(PMIC_REG_BASE+0x02E0))
#define DEW_CIPHER_KEY_SEL  ((UINT32)(PMIC_REG_BASE+0x02E2))
#define DEW_CIPHER_IV_SEL   ((UINT32)(PMIC_REG_BASE+0x02E4))
#define DEW_CIPHER_EN       ((UINT32)(PMIC_REG_BASE+0x02E6))
#define DEW_CIPHER_RDY      ((UINT32)(PMIC_REG_BASE+0x02E8))
#define DEW_CIPHER_MODE     ((UINT32)(PMIC_REG_BASE+0x02EA))
#define DEW_CIPHER_SWRST    ((UINT32)(PMIC_REG_BASE+0x02EC))
#define DEW_RDDMY_NO        ((UINT32)(PMIC_REG_BASE+0x02EE))
#define INT_TYPE_CON0       ((UINT32)(PMIC_REG_BASE+0x02F0))
#define INT_TYPE_CON0_SET   ((UINT32)(PMIC_REG_BASE+0x02F2))
#define INT_TYPE_CON0_CLR   ((UINT32)(PMIC_REG_BASE+0x02F4))
#define INT_TYPE_CON1       ((UINT32)(PMIC_REG_BASE+0x02F6))
#define INT_TYPE_CON1_SET   ((UINT32)(PMIC_REG_BASE+0x02F8))
#define INT_TYPE_CON1_CLR   ((UINT32)(PMIC_REG_BASE+0x02FA))
#define INT_TYPE_CON2       ((UINT32)(PMIC_REG_BASE+0x02FC))
#define INT_TYPE_CON2_SET   ((UINT32)(PMIC_REG_BASE+0x02FE))`
#define INT_TYPE_CON2_CLR   ((UINT32)(PMIC_REG_BASE+0x0300))
#define INT_STA             ((UINT32)(PMIC_REG_BASE+0x0302))
#define BUCK_ALL_CON0       ((UINT32)(PMIC_REG_BASE+0x0400))
#define BUCK_ALL_CON1       ((UINT32)(PMIC_REG_BASE+0x0402))
#define BUCK_ALL_CON2       ((UINT32)(PMIC_REG_BASE+0x0404))
#define BUCK_ALL_CON3       ((UINT32)(PMIC_REG_BASE+0x0406))
#define BUCK_ALL_CON4       ((UINT32)(PMIC_REG_BASE+0x0408))
#define BUCK_ALL_CON5       ((UINT32)(PMIC_REG_BASE+0x040A))
#define BUCK_ALL_CON6       ((UINT32)(PMIC_REG_BASE+0x040C))
//#define BUCK_ALL_CON7       ((UINT32)(PMIC_REG_BASE+0x040E))
//#define BUCK_ALL_CON8       ((UINT32)(PMIC_REG_BASE+0x0410))
//#define BUCK_ALL_CON9       ((UINT32)(PMIC_REG_BASE+0x040E))
//#define BUCK_ALL_CON10      ((UINT32)(PMIC_REG_BASE+0x0414))
//#define BUCK_ALL_CON11      ((UINT32)(PMIC_REG_BASE+0x0416))
#define BUCK_ALL_CON12      ((UINT32)(PMIC_REG_BASE+0x0410))
#define BUCK_ALL_CON13      ((UINT32)(PMIC_REG_BASE+0x0412))
#define BUCK_ALL_CON14      ((UINT32)(PMIC_REG_BASE+0x0414))
//#define BUCK_ALL_CON15      ((UINT32)(PMIC_REG_BASE+0x041E))
#define BUCK_ALL_CON16      ((UINT32)(PMIC_REG_BASE+0x0416))
//#define BUCK_ALL_CON17      ((UINT32)(PMIC_REG_BASE+0x0422))
#define BUCK_ALL_CON18      ((UINT32)(PMIC_REG_BASE+0x0418))
#define BUCK_ALL_CON19      ((UINT32)(PMIC_REG_BASE+0x041A))
#define BUCK_ALL_CON20      ((UINT32)(PMIC_REG_BASE+0x041C))
#define BUCK_ALL_CON21      ((UINT32)(PMIC_REG_BASE+0x041E))
#define BUCK_ALL_CON22      ((UINT32)(PMIC_REG_BASE+0x0420))
#define BUCK_ALL_CON23      ((UINT32)(PMIC_REG_BASE+0x0422))
#define BUCK_ALL_CON24      ((UINT32)(PMIC_REG_BASE+0x0424))
#define BUCK_ALL_CON25      ((UINT32)(PMIC_REG_BASE+0x0426))
#define BUCK_ALL_CON26      ((UINT32)(PMIC_REG_BASE+0x0428))
#define BUCK_ALL_CON27      ((UINT32)(PMIC_REG_BASE+0x042A))
#define BUCK_ALL_CON28      ((UINT32)(PMIC_REG_BASE+0x042C))
//#define VDRAM_ANA_CON0      ((UINT32)(PMIC_REG_BASE+0x043A))
//#define VDRAM_ANA_CON1      ((UINT32)(PMIC_REG_BASE+0x043C))
//#define VDRAM_ANA_CON2      ((UINT32)(PMIC_REG_BASE+0x043E))
//#define VDRAM_ANA_CON3      ((UINT32)(PMIC_REG_BASE+0x0440))
//#define VDRAM_ANA_CON4      ((UINT32)(PMIC_REG_BASE+0x0442))
#define VCORE1_ANA_CON0     ((UINT32)(PMIC_REG_BASE+0x0440))
#define VCORE1_ANA_CON1     ((UINT32)(PMIC_REG_BASE+0x0442))
#define VCORE1_ANA_CON2     ((UINT32)(PMIC_REG_BASE+0x0444))
#define VCORE1_ANA_CON3     ((UINT32)(PMIC_REG_BASE+0x0446))
#define VCORE1_ANA_CON4     ((UINT32)(PMIC_REG_BASE+0x0448))
#define SMPS_TOP_ANA_CON0   ((UINT32)(PMIC_REG_BASE+0x042E))
#define SMPS_TOP_ANA_CON1   ((UINT32)(PMIC_REG_BASE+0x0430))
#define SMPS_TOP_ANA_CON2   ((UINT32)(PMIC_REG_BASE+0x0432))
#define SMPS_TOP_ANA_CON3   ((UINT32)(PMIC_REG_BASE+0x0434))
#define SMPS_TOP_ANA_CON4   ((UINT32)(PMIC_REG_BASE+0x0436))
#define SMPS_TOP_ANA_CON5   ((UINT32)(PMIC_REG_BASE+0x0438))
#define SMPS_TOP_ANA_CON6   ((UINT32)(PMIC_REG_BASE+0x043A))
#define SMPS_TOP_ANA_CON7   ((UINT32)(PMIC_REG_BASE+0x043C))
#define SMPS_TOP_ANA_CON8   ((UINT32)(PMIC_REG_BASE+0x043E))
//#define SMPS_TOP_ANA_CON9   ((UINT32)(PMIC_REG_BASE+0x0460))
//#define VDVFS1_ANA_CON0     ((UINT32)(PMIC_REG_BASE+0x0462))
//#define VDVFS1_ANA_CON1     ((UINT32)(PMIC_REG_BASE+0x0464))
//#define VDVFS1_ANA_CON2     ((UINT32)(PMIC_REG_BASE+0x0466))
//#define VDVFS1_ANA_CON3     ((UINT32)(PMIC_REG_BASE+0x0468))
//#define VDVFS1_ANA_CON4     ((UINT32)(PMIC_REG_BASE+0x046A))
//#define VDVFS1_ANA_CON5     ((UINT32)(PMIC_REG_BASE+0x046C))
//#define VDVFS1_ANA_CON6     ((UINT32)(PMIC_REG_BASE+0x046E))
//#define VDVFS1_ANA_CON7     ((UINT32)(PMIC_REG_BASE+0x0470))
//#define VGPU_ANA_CON0       ((UINT32)(PMIC_REG_BASE+0x0472))
//#define VGPU_ANA_CON1       ((UINT32)(PMIC_REG_BASE+0x0474))
//#define VGPU_ANA_CON2       ((UINT32)(PMIC_REG_BASE+0x0476))
//#define VGPU_ANA_CON3       ((UINT32)(PMIC_REG_BASE+0x0478))
//#define VGPU_ANA_CON4       ((UINT32)(PMIC_REG_BASE+0x047A))
#define VPA_ANA_CON0        ((UINT32)(PMIC_REG_BASE+0x0462))
#define VPA_ANA_CON1        ((UINT32)(PMIC_REG_BASE+0x0464))
#define VPA_ANA_CON2        ((UINT32)(PMIC_REG_BASE+0x0466))
#define VPA_ANA_CON3        ((UINT32)(PMIC_REG_BASE+0x0468))
#if 0
#define VCORE2_ANA_CON0     ((UINT32)(PMIC_REG_BASE+0x0484))
#define VCORE2_ANA_CON1     ((UINT32)(PMIC_REG_BASE+0x0486))
#define VCORE2_ANA_CON2     ((UINT32)(PMIC_REG_BASE+0x0488))
#define VCORE2_ANA_CON3     ((UINT32)(PMIC_REG_BASE+0x048A))
#define VCORE2_ANA_CON4     ((UINT32)(PMIC_REG_BASE+0x048C))
#define VIO18_ANA_CON0      ((UINT32)(PMIC_REG_BASE+0x048E))
#define VIO18_ANA_CON1      ((UINT32)(PMIC_REG_BASE+0x0490))
#define VIO18_ANA_CON2      ((UINT32)(PMIC_REG_BASE+0x0492))
#define VIO18_ANA_CON3      ((UINT32)(PMIC_REG_BASE+0x0494))
#define VIO18_ANA_CON4      ((UINT32)(PMIC_REG_BASE+0x0496))
#define VRF18_0_ANA_CON0    ((UINT32)(PMIC_REG_BASE+0x0498))
#define VRF18_0_ANA_CON1    ((UINT32)(PMIC_REG_BASE+0x049A))
#define VRF18_0_ANA_CON2    ((UINT32)(PMIC_REG_BASE+0x049C))
#define VRF18_0_ANA_CON3    ((UINT32)(PMIC_REG_BASE+0x049E))
#define VRF18_0_ANA_CON4    ((UINT32)(PMIC_REG_BASE+0x04A0))
#define VDVFS11_CON0        ((UINT32)(PMIC_REG_BASE+0x04A2))
#define VDVFS11_CON7        ((UINT32)(PMIC_REG_BASE+0x04B0))
#define VDVFS11_CON8        ((UINT32)(PMIC_REG_BASE+0x04B2))
#define VDVFS11_CON9        ((UINT32)(PMIC_REG_BASE+0x04B4))
#define VDVFS11_CON10       ((UINT32)(PMIC_REG_BASE+0x04B6))
#define VDVFS11_CON11       ((UINT32)(PMIC_REG_BASE+0x04B8))
#define VDVFS11_CON12       ((UINT32)(PMIC_REG_BASE+0x04BA))
#define VDVFS11_CON13       ((UINT32)(PMIC_REG_BASE+0x04BC))
#define VDVFS11_CON14       ((UINT32)(PMIC_REG_BASE+0x04BE))
#define VDVFS11_CON18       ((UINT32)(PMIC_REG_BASE+0x04C6))
#define VDVFS12_CON0        ((UINT32)(PMIC_REG_BASE+0x04C8))
#define VDVFS12_CON7        ((UINT32)(PMIC_REG_BASE+0x04D6))
#define VDVFS12_CON8        ((UINT32)(PMIC_REG_BASE+0x04D8))
#define VDVFS12_CON9        ((UINT32)(PMIC_REG_BASE+0x04DA))
#define VDVFS12_CON10       ((UINT32)(PMIC_REG_BASE+0x04DC))
#define VDVFS12_CON11       ((UINT32)(PMIC_REG_BASE+0x04DE))
#define VDVFS12_CON12       ((UINT32)(PMIC_REG_BASE+0x04E0))
#define VDVFS12_CON13       ((UINT32)(PMIC_REG_BASE+0x04E2))
#define VDVFS12_CON14       ((UINT32)(PMIC_REG_BASE+0x04E4))
#define VDVFS12_CON18       ((UINT32)(PMIC_REG_BASE+0x04EC))
#define VSRAM_DVFS1_CON0    ((UINT32)(PMIC_REG_BASE+0x04EE))
#define VSRAM_DVFS1_CON7    ((UINT32)(PMIC_REG_BASE+0x04FC))
#define VSRAM_DVFS1_CON8    ((UINT32)(PMIC_REG_BASE+0x04FE))
#define VSRAM_DVFS1_CON9    ((UINT32)(PMIC_REG_BASE+0x0500))
#endif

#define ZCD_CON0            ((UINT32)(PMIC_REG_BASE+0x0800))
#define ZCD_CON1            ((UINT32)(PMIC_REG_BASE+0x0802))
#define ZCD_CON2            ((UINT32)(PMIC_REG_BASE+0x0804))
#define ZCD_CON3            ((UINT32)(PMIC_REG_BASE+0x0806))
#define ZCD_CON4            ((UINT32)(PMIC_REG_BASE+0x0808))
#define ZCD_CON5            ((UINT32)(PMIC_REG_BASE+0x080A))

#define LDO_CON1            ((UINT32)(PMIC_REG_BASE + 0x0A02))
#define LDO_CON2            ((UINT32)(PMIC_REG_BASE + 0x0A04))

#define LDO_VCON1           ((UINT32)(PMIC_REG_BASE + 0x0A40))

#define SPK_CON0            ((UINT32)(PMIC_REG_BASE+0x0A90))
#define SPK_CON1            ((UINT32)(PMIC_REG_BASE+0x0A92))
#define SPK_CON2            ((UINT32)(PMIC_REG_BASE+0x0A94))
#define SPK_CON3            ((UINT32)(PMIC_REG_BASE+0x0A96))
#define SPK_CON4            ((UINT32)(PMIC_REG_BASE+0x0A98))
#define SPK_CON5            ((UINT32)(PMIC_REG_BASE+0x0A9A))
#define SPK_CON6            ((UINT32)(PMIC_REG_BASE+0x0A9C))
#define SPK_CON7            ((UINT32)(PMIC_REG_BASE+0x0A9E))
#define SPK_CON8            ((UINT32)(PMIC_REG_BASE+0x0AA0))
#define SPK_CON9            ((UINT32)(PMIC_REG_BASE+0x0AA2))
#define SPK_CON10           ((UINT32)(PMIC_REG_BASE+0x0AA4))
#define SPK_CON11           ((UINT32)(PMIC_REG_BASE+0x0AA6))
#define SPK_CON12           ((UINT32)(PMIC_REG_BASE+0x0AA8))
#define SPK_CON13           ((UINT32)(PMIC_REG_BASE+0x0AAA))
#define SPK_CON14           ((UINT32)(PMIC_REG_BASE+0x0AAC))
#define SPK_CON15           ((UINT32)(PMIC_REG_BASE+0x0AAE))
#define SPK_CON16           ((UINT32)(PMIC_REG_BASE+0x0AB0))
#define SPK_ANA_CON0        ((UINT32)(PMIC_REG_BASE+0x0AB2))
#define SPK_ANA_CON1        ((UINT32)(PMIC_REG_BASE+0x0AB4))
#define SPK_ANA_CON3        ((UINT32)(PMIC_REG_BASE+0x0AB6))

//#define FGADC_ANA_CON0      ((UINT32)(PMIC_REG_BASE+0x0CDC))
#define AUDDEC_ANA_CON0     ((UINT32)(PMIC_REG_BASE+0x0CDC))
#define AUDDEC_ANA_CON1     ((UINT32)(PMIC_REG_BASE+0x0CDE))
#define AUDDEC_ANA_CON2     ((UINT32)(PMIC_REG_BASE+0x0CE0))
#define AUDDEC_ANA_CON3     ((UINT32)(PMIC_REG_BASE+0x0CE2))
#define AUDDEC_ANA_CON4     ((UINT32)(PMIC_REG_BASE+0x0CE4))
#define AUDDEC_ANA_CON5     ((UINT32)(PMIC_REG_BASE+0x0CE6))
#define AUDDEC_ANA_CON6     ((UINT32)(PMIC_REG_BASE+0x0CE8))
#define AUDDEC_ANA_CON7     ((UINT32)(PMIC_REG_BASE+0x0CEA))
#define AUDDEC_ANA_CON8     ((UINT32)(PMIC_REG_BASE+0x0CEC))
#define AUDENC_ANA_CON0     ((UINT32)(PMIC_REG_BASE+0x0CEE))
#define AUDENC_ANA_CON1     ((UINT32)(PMIC_REG_BASE+0x0CF0))
#define AUDENC_ANA_CON2     ((UINT32)(PMIC_REG_BASE+0x0CF2))
#define AUDENC_ANA_CON3     ((UINT32)(PMIC_REG_BASE+0x0CF4))
#define AUDENC_ANA_CON4     ((UINT32)(PMIC_REG_BASE+0x0CF6))
#define AUDENC_ANA_CON5     ((UINT32)(PMIC_REG_BASE+0x0CF8))
#define AUDENC_ANA_CON6     ((UINT32)(PMIC_REG_BASE+0x0CFA))
#define AUDENC_ANA_CON7     ((UINT32)(PMIC_REG_BASE+0x0CFC))
#define AUDENC_ANA_CON8     ((UINT32)(PMIC_REG_BASE+0x0CFE))
#define AUDENC_ANA_CON9     ((UINT32)(PMIC_REG_BASE+0x0D00))
#define AUDENC_ANA_CON10    ((UINT32)(PMIC_REG_BASE+0x0D02))
//#define AUDENC_ANA_CON12    ((UINT32)(PMIC_REG_BASE+0x0D06))
//#define AUDENC_ANA_CON13    ((UINT32)(PMIC_REG_BASE+0x0D08))

//#define AUDENC_ANA_CON14    ((UINT32)(PMIC_REG_BASE+0x0D0A))
//#define AUDENC_ANA_CON15    ((UINT32)(PMIC_REG_BASE+0xFFFF)) // George temp checkreg
#define AUDNCP_CLKDIV_CON0  ((UINT32)(PMIC_REG_BASE+0x0D04))
#define AUDNCP_CLKDIV_CON1  ((UINT32)(PMIC_REG_BASE+0x0D06))
#define AUDNCP_CLKDIV_CON2  ((UINT32)(PMIC_REG_BASE+0x0D08))
#define AUDNCP_CLKDIV_CON3  ((UINT32)(PMIC_REG_BASE+0x0D0A))
#define AUDNCP_CLKDIV_CON4  ((UINT32)(PMIC_REG_BASE+0x0D0C))

#define GPIO_MODE3          ((UINT32)(0x60D0))

#if 1
//register number

#else
#include <mach/upmu_hw.h>
#endif

void Ana_Set_Reg(uint32 offset, uint32 value, uint32 mask);
uint32  Ana_Get_Reg(uint32 offset);

// for debug usage
void Ana_Log_Print(void);

#endif


