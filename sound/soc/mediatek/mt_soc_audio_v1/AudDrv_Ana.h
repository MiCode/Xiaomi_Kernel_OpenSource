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
#define AFE_ADDA2_UL_SRC_CON0_H           ((UINT32)(PMIC_REG_BASE+0x2000+0x44))
#define AFE_ADDA2_UL_SRC_CON0_L            ((UINT32)(PMIC_REG_BASE+0x2000+0x46))
#define AFE_UL_SRC_CON1_H                          ((UINT32)(PMIC_REG_BASE+0x2000+0x48))
#define AFE_ADDA2_UL_SRC_CON1_L          ((UINT32)(PMIC_REG_BASE+0x2000+0x4a))
#define AFE_ADDA2_UP8X_FIFO_LOG_MON0   ((UINT32)(PMIC_REG_BASE+0x2000+0x4c))
#define AFE_ADDA2_UP8X_FIFO_LOG_MON1   ((UINT32)(PMIC_REG_BASE+0x2000+0x4e))
#define AFE_ADDA2_PMIC_NEWIF_CFG0       ((UINT32)(PMIC_REG_BASE+0x2000+0x50))
#define AFE_ADDA2_PMIC_NEWIF_CFG1       ((UINT32)(PMIC_REG_BASE+0x2000+0x52))
#define AFE_ADDA2_PMIC_NEWIF_CFG2       ((UINT32)(PMIC_REG_BASE+0x2000+0x54))
#define AFE_MIC_ARRAY_CFG                          ((UINT32)(PMIC_REG_BASE+0x2000+0x56))
#define AFE_ADC_ASYNC_FIFO_CFG              ((UINT32)(PMIC_REG_BASE+0x2000+0x58))
#define AFE_ANC_CFG0                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x5a))
#define AFE_ANC_CFG1                                     ((UINT32)(PMIC_REG_BASE+0x2000+0x5c))
#define AFE_ANC_COEF_B00                            ((UINT32)(PMIC_REG_BASE+0x2000+0x5e))
#define AFE_ANC_COEF_ADDR                         ((UINT32)(PMIC_REG_BASE+0x2000+0x60))
#define AFE_ANC_COEF_WDATA                      ((UINT32)(PMIC_REG_BASE+0x2000+0x62))
#define AFE_ANC_COEF_RDATA                       ((UINT32)(PMIC_REG_BASE+0x2000+0x64))
#define AUDRC_TUNE_UL2_MON0                     ((UINT32)(PMIC_REG_BASE+0x2000+0x66))
#define AFE_MBIST_CFG0                                  ((UINT32)(PMIC_REG_BASE+0x2000+0x68))
#define AFE_MBIST_CFG1                                  ((UINT32)(PMIC_REG_BASE+0x2000+0x6a))
#define AFE_MBIST_CFG2                                  ((UINT32)(PMIC_REG_BASE+0x2000+0x6c))
#define AFE_MBIST_CFG3                                  ((UINT32)(PMIC_REG_BASE+0x2000+0x6e))
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
#define AFE_CLASSH_CFG0                               ((UINT32)(PMIC_REG_BASE+0x2000+0x8a))
#define AFE_CLASSH_CFG1                               ((UINT32)(PMIC_REG_BASE+0x2000+0x8c))
#define AFE_CLASSH_CFG2                               ((UINT32)(PMIC_REG_BASE+0x2000+0x8e))
#define AFE_CLASSH_CFG3                               ((UINT32)(PMIC_REG_BASE+0x2000+0x90))
#define AFE_CLASSH_CFG4                               ((UINT32)(PMIC_REG_BASE+0x2000+0x92))
#define AFE_CLASSH_CFG5                               ((UINT32)(PMIC_REG_BASE+0x2000+0x94))
#define AFE_CLASSH_CFG6                               ((UINT32)(PMIC_REG_BASE+0x2000+0x96))
#define AFE_CLASSH_CFG7                               ((UINT32)(PMIC_REG_BASE+0x2000+0x98))
#define AFE_CLASSH_CFG8                               ((UINT32)(PMIC_REG_BASE+0x2000+0x9a))
#define AFE_CLASSH_CFG9                               ((UINT32)(PMIC_REG_BASE+0x2000+0x9c))
#define AFE_CLASSH_CFG10                             ((UINT32)(PMIC_REG_BASE+0x2000+0x9e))
#define AFE_CLASSH_CFG11                             ((UINT32)(PMIC_REG_BASE+0x2000+0xa0))
#define AFE_CLASSH_CFG12                               ((UINT32)(PMIC_REG_BASE+0x2000+0xa2))
#define AFE_CLASSH_CFG13                               ((UINT32)(PMIC_REG_BASE+0x2000+0xa4))
#define AFE_CLASSH_CFG14                               ((UINT32)(PMIC_REG_BASE+0x2000+0xa6))
#define AFE_CLASSH_CFG15                               ((UINT32)(PMIC_REG_BASE+0x2000+0xa8))
#define AFE_CLASSH_CFG16                               ((UINT32)(PMIC_REG_BASE+0x2000+0xaa))
#define AFE_CLASSH_CFG17                               ((UINT32)(PMIC_REG_BASE+0x2000+0xac))
#define AFE_CLASSH_CFG18                               ((UINT32)(PMIC_REG_BASE+0x2000+0xae))
#define AFE_CLASSH_CFG19                               ((UINT32)(PMIC_REG_BASE+0x2000+0xb0))
#define AFE_CLASSH_CFG20                               ((UINT32)(PMIC_REG_BASE+0x2000+0xb2))
#define AFE_CLASSH_CFG21                               ((UINT32)(PMIC_REG_BASE+0x2000+0xb4))
#define AFE_CLASSH_CFG22                               ((UINT32)(PMIC_REG_BASE+0x2000+0xb6))
#define AFE_CLASSH_CFG23                               ((UINT32)(PMIC_REG_BASE+0x2000+0xb8))
#define AFE_CLASSH_CFG24                               ((UINT32)(PMIC_REG_BASE+0x2000+0xba))
#define AFE_CLASSH_CFG25                               ((UINT32)(PMIC_REG_BASE+0x2000+0xbc))
#define AFE_CLASSH_CFG26                               ((UINT32)(PMIC_REG_BASE+0x2000+0xbe))
#define AFE_CLASSH_CFG27                               ((UINT32)(PMIC_REG_BASE+0x2000+0xc0))
#define AFE_CLASSH_CFG28                               ((UINT32)(PMIC_REG_BASE+0x2000+0xc2))
#define AFE_CLASSH_CFG29                               ((UINT32)(PMIC_REG_BASE+0x2000+0xc4))
#define AFE_CLASSH_CFG30                               ((UINT32)(PMIC_REG_BASE+0x2000+0xc6))
#define AFE_CLASSH_MON00                               ((UINT32)(PMIC_REG_BASE+0x2000+0xc8))
#define AFE_CLASSH_MON1                                 ((UINT32)(PMIC_REG_BASE+0x2000+0xca))
#define AFE_CLASSH_RESERVED0                       ((UINT32)(PMIC_REG_BASE+0x2000+0xcc))
#define AFE_CLASSH_RESERVED1                       ((UINT32)(PMIC_REG_BASE+0x2000+0xce))
#define AFE_DCCLK_CFG0                                     ((UINT32)(PMIC_REG_BASE+0x2000+0xd0))
#define AFE_ANC_COEF_MON1                            ((UINT32)(PMIC_REG_BASE+0x2000+0xd2))
#define AFE_ANC_COEF_MON2                            ((UINT32)(PMIC_REG_BASE+0x2000+0xd4))
#define AFE_ANC_COEF_MON3                            ((UINT32)(PMIC_REG_BASE+0x2000+0xd6))


#define TOP_STATUS                                               ((UINT32)(PMIC_REG_BASE+0x132))
#define TOP_STATUS_SET                                       ((UINT32)(PMIC_REG_BASE+0x134))
#define TOP_STATUS_CLR                                       ((UINT32)(PMIC_REG_BASE+0x136))
#define TOP_CKPDN_CON0                                     ((UINT32)(PMIC_REG_BASE+0x138))
#define TOP_CKPDN_CON0_SET                             ((UINT32)(PMIC_REG_BASE+0x13a))
#define TOP_CKPDN_CON0_CLR                             ((UINT32)(PMIC_REG_BASE+0x13c))
#define TOP_CKPDN_CON1                                     ((UINT32)(PMIC_REG_BASE+0x13e))
#define TOP_CKPDN_CON1_SET                             ((UINT32)(PMIC_REG_BASE+0x140))
#define TOP_CKPDN_CON1_CLR                             ((UINT32)(PMIC_REG_BASE+0x142))
#define TOP_CKPDN_CON2                                      ((UINT32)(PMIC_REG_BASE+0x144))
#define TOP_CKPDN_CON2_SET                             ((UINT32)(PMIC_REG_BASE+0x146))
#define TOP_CKPDN_CON2_CLR                             ((UINT32)(PMIC_REG_BASE+0x148))
#define TOP_CKSEL_CON                                         ((UINT32)(PMIC_REG_BASE+0x14a))
#define TOP_CKSEL_CON_SET                                 ((UINT32)(PMIC_REG_BASE+0x14c))
#define TOP_CKSEL_CON_CLR                                 ((UINT32)(PMIC_REG_BASE+0x14e))
#define TOP_CKHWEN_CON                                     ((UINT32)(PMIC_REG_BASE+0x150))
#define TOP_CKHWEN_CON_SET                             ((UINT32)(PMIC_REG_BASE+0x152))
#define TOP_CKHWEN_CON_CLR                             ((UINT32)(PMIC_REG_BASE+0x154))
#define TOP_CKTST_CON0                                        ((UINT32)(PMIC_REG_BASE+0x156))
#define TOP_CKTST_CON0_SET                              ((UINT32)(PMIC_REG_BASE+0x158))
#define TOP_CKTST_CON0_GET                              ((UINT32)(PMIC_REG_BASE+0x158))

#define TOP_CLKSQ                                                   ((UINT32)(PMIC_REG_BASE+0x15a))
#define TOP_CLKSQ_SET                                          ((UINT32)(PMIC_REG_BASE+0x15C))
#define TOP_CLKSQ_CLR                                          ((UINT32)(PMIC_REG_BASE+0x15e))
#define TOP_RST_CON                                              ((UINT32)(PMIC_REG_BASE+0x160))
#define TOP_RST_CON_SET                                     ((UINT32)(PMIC_REG_BASE+0x162))
#define TOP_RST_CON_CLR                                     ((UINT32)(PMIC_REG_BASE+0x164))

#define ZCD_CON0                                                    ((UINT32)(PMIC_REG_BASE+0x400))
#define ZCD_CON1                                                    ((UINT32)(PMIC_REG_BASE+0x402))
#define ZCD_CON2                                                    ((UINT32)(PMIC_REG_BASE+0x404))
#define ZCD_CON3                                                    ((UINT32)(PMIC_REG_BASE+0x406))
#define ZCD_CON4                                                    ((UINT32)(PMIC_REG_BASE+0x408))
#define ZCD_CON5                                                    ((UINT32)(PMIC_REG_BASE+0x40a))


#define ANALDO_CON3                                            ((UINT32)(PMIC_REG_BASE+0x506))

#define AUDDAC_CFG0                                            ((UINT32)(PMIC_REG_BASE+0x662))
#define AUDBUF_CFG0                                            ((UINT32)(PMIC_REG_BASE+0x664))
#define AUDBUF_CFG1                                            ((UINT32)(PMIC_REG_BASE+0x666))
#define AUDBUF_CFG2                                            ((UINT32)(PMIC_REG_BASE+0x668))
#define AUDBUF_CFG3                                            ((UINT32)(PMIC_REG_BASE+0x66a))
#define AUDBUF_CFG4                                            ((UINT32)(PMIC_REG_BASE+0x66c))
#define AUDBUF_CFG5                                            ((UINT32)(PMIC_REG_BASE+0x66e))
#define AUDBUF_CFG6                                            ((UINT32)(PMIC_REG_BASE+0x670))
#define AUDBUF_CFG7                                            ((UINT32)(PMIC_REG_BASE+0x672))
#define AUDBUF_CFG8                                            ((UINT32)(PMIC_REG_BASE+0x674))
#define IBIASDIST_CFG0                                         ((UINT32)(PMIC_REG_BASE+0x676))
#define AUDCLKGEN_CFG0                                      ((UINT32)(PMIC_REG_BASE+0x678))
#define AUDLDO_CFG0                                            ((UINT32)(PMIC_REG_BASE+0x67a))
#define AUDDCDC_CFG1                                          ((UINT32)(PMIC_REG_BASE+0x67c))
#define AUDNVREGGLB_CFG0                                 ((UINT32)(PMIC_REG_BASE+0x680))
#define AUD_NCP0                                                   ((UINT32)(PMIC_REG_BASE+0x682))
#define AUD_ZCD_CFG0                                          ((UINT32)(PMIC_REG_BASE+0x684))
#define AUDPREAMP_CFG0                                     ((UINT32)(PMIC_REG_BASE+0x686))
#define AUDPREAMP_CFG1                                     ((UINT32)(PMIC_REG_BASE+0x688))
#define AUDPREAMP_CFG2                                     ((UINT32)(PMIC_REG_BASE+0x68a))
#define AUDADC_CFG0                                            ((UINT32)(PMIC_REG_BASE+0x68c))
#define AUDADC_CFG1                                            ((UINT32)(PMIC_REG_BASE+0x68e))
#define AUDADC_CFG2                                            ((UINT32)(PMIC_REG_BASE+0x690))
#define AUDADC_CFG3                                            ((UINT32)(PMIC_REG_BASE+0x692))
#define AUDADC_CFG4                                            ((UINT32)(PMIC_REG_BASE+0x694))
#define AUDADC_CFG5                                            ((UINT32)(PMIC_REG_BASE+0x696))
#define AUDDIGMI_CFG0                                         ((UINT32)(PMIC_REG_BASE+0x698))
#define AUDDIGMI_CFG1                                         ((UINT32)(PMIC_REG_BASE+0x69a))
#define AUDMICBIAS_CFG0                                     ((UINT32)(PMIC_REG_BASE+0x69c))
#define AUDMICBIAS_CFG1                                     ((UINT32)(PMIC_REG_BASE+0x69e))
#define AUDENCSPARE_CFG0                                  ((UINT32)(PMIC_REG_BASE+0x6a0))
#define AUDPREAMPGAIN_CFG0                             ((UINT32)(PMIC_REG_BASE+0x6a2))
#define AUDVOWPLL_CFG0                                      ((UINT32)(PMIC_REG_BASE+0x6a4))
#define AUDVOWPLL_CFG1                                       ((UINT32)(PMIC_REG_BASE+0x6a6))
#define AUDVOWPLL_CFG2                                       ((UINT32)(PMIC_REG_BASE+0x6a8))
#define AUDLDO_NVREG_CFG0                                ((UINT32)(PMIC_REG_BASE+0x6aa))
#define AUDLDO_NVREG_CFG1                                ((UINT32)(PMIC_REG_BASE+0x6ac))
#define AUDLDO_NVREG_CFG2                                ((UINT32)(PMIC_REG_BASE+0x6ae))

#define SPK_TOP_CKPDN_CON0                                   ((UINT32)(0x8094))
#define SPK_TOP_CKPDN_CON0_SET                          ((UINT32)(0x8096))
#define SPK_TOP_CKPDN_CON0_CLR                          ((UINT32)(0x8098))
#define SPK_TOP_CKPDN_CON1                                   ((UINT32)(0x809a))
#define SPK_TOP_CKPDN_CON1_SET                          ((UINT32)(0x809c))
#define SPK_TOP_CKPDN_CON1_CLR                          ((UINT32)(0x809e))


#define SPK_INT_CON2                                            ((UINT32)(0x80d4))
#define SPK_INT_CON2_SET                                   ((UINT32)(0x80d4))
#define SPK_INT_CON2_CLR                                   ((UINT32)(0x809A))

#define VSBST_CON5                                                  ((UINT32)(0x8534))

#define VSBST_CON0                                                  ((UINT32)(0x852a))
#define VSBST_CON1                                                  ((UINT32)(0x852c))
#define VSBST_CON2                                                  ((UINT32)(0x852e))
#define VSBST_CON3                                                  ((UINT32)(0x8530))
#define VSBST_CON4                                                  ((UINT32)(0x8532))
#define VSBST_CON5                                                  ((UINT32)(0x8534))
#define VSBST_CON6                                                  ((UINT32)(0x8536))
#define VSBST_CON7                                                  ((UINT32)(0x8538))
#define VSBST_CON8                                                  ((UINT32)(0x853a))
#define VSBST_CON9                                                  ((UINT32)(0x853c))
#define VSBST_CON10                                               ((UINT32)(0x853e))
#define VSBST_CON11                                               ((UINT32)(0x8540))
#define VSBST_CON12                                               ((UINT32)(0x8542))
#define VSBST_CON13                                               ((UINT32)(0x8544))
#define VSBST_CON14                                               ((UINT32)(0x8546))
#define VSBST_CON15                                               ((UINT32)(0x8548))
#define VSBST_CON16                                               ((UINT32)(0x854a))
#define VSBST_CON17                                               ((UINT32)(0x854c))
#define VSBST_CON18                                               ((UINT32)(0x854e))
#define VSBST_CON19                                               ((UINT32)(0x8550))
#define VSBST_CON20                                               ((UINT32)(0x8552))
#define VSBST_CON21                                               ((UINT32)(0x8554))


#define SPK_CON0                                                      ((UINT32)(0x8cf2))
#define SPK_CON1                                                      ((UINT32)(0x8cf4))
#define SPK_CON2                                                      ((UINT32)(0x8cf6))
#define SPK_CON3                                                      ((UINT32)(0x8cf8))
#define SPK_CON4                                                      ((UINT32)(0x8cfa))
#define SPK_CON5                                                      ((UINT32)(0x8cfc))
#define SPK_CON6                                                      ((UINT32)(0x8cfe))
#define SPK_CON7                                                      ((UINT32)(0x8d00))
#define SPK_CON8                                                      ((UINT32)(0x8d02))
#define SPK_CON9                                                      ((UINT32)(0x8d04))
#define SPK_CON10                                                    ((UINT32)(0x8d06))
#define SPK_CON11                                                    ((UINT32)(0x8d08))
#define SPK_CON12                                                    ((UINT32)(0x8d0a))
#define SPK_CON13                                                    ((UINT32)(0x8d0c))
#define SPK_CON14                                                    ((UINT32)(0x8d0e))
#define SPK_CON15                                                    ((UINT32)(0x8d10))
#define SPK_CON16                                                    ((UINT32)(0x8d12))


#define MT6332_PMIC_REG_BASE (0x8000)

#define MT6332_TOP_CKPDN_CON0     ((UINT32)(MT6332_PMIC_REG_BASE+0x0094))

#define MT6332_TOP_CKHWEN_CON     ((UINT32)(MT6332_PMIC_REG_BASE+0x00B2))
#define MT6332_TOP_RST_CON     ((UINT32)(MT6332_PMIC_REG_BASE+0x00BC))

#define MT6332_TOP_RST_CON0	      ((UINT32)(MT6332_PMIC_REG_BASE+0x0280))
#define MT6332_LDO_CON1	      ((UINT32)(MT6332_PMIC_REG_BASE+0x0CB6))


#define MT6332_AUXADC_ADC0        ((UINT32)(MT6332_PMIC_REG_BASE+0x0800))
#define MT6332_AUXADC_ADC1        ((UINT32)(MT6332_PMIC_REG_BASE+0x0802))
#define MT6332_AUXADC_ADC2        ((UINT32)(MT6332_PMIC_REG_BASE+0x0804))
#define MT6332_AUXADC_ADC3        ((UINT32)(MT6332_PMIC_REG_BASE+0x0806))
#define MT6332_AUXADC_ADC4        ((UINT32)(MT6332_PMIC_REG_BASE+0x0808))
#define MT6332_AUXADC_ADC5        ((UINT32)(MT6332_PMIC_REG_BASE+0x080A))
#define MT6332_AUXADC_ADC6        ((UINT32)(MT6332_PMIC_REG_BASE+0x080C))
#define MT6332_AUXADC_ADC7        ((UINT32)(MT6332_PMIC_REG_BASE+0x080E))
#define MT6332_AUXADC_ADC8        ((UINT32)(MT6332_PMIC_REG_BASE+0x0810))
#define MT6332_AUXADC_ADC9        ((UINT32)(MT6332_PMIC_REG_BASE+0x0812))
#define MT6332_AUXADC_ADC10       ((UINT32)(MT6332_PMIC_REG_BASE+0x0814))
#define MT6332_AUXADC_ADC11       ((UINT32)(MT6332_PMIC_REG_BASE+0x0816))
#define MT6332_AUXADC_ADC12       ((UINT32)(MT6332_PMIC_REG_BASE+0x0818))
#define MT6332_AUXADC_ADC13       ((UINT32)(MT6332_PMIC_REG_BASE+0x081A))
#define MT6332_AUXADC_ADC14       ((UINT32)(MT6332_PMIC_REG_BASE+0x081C))
#define MT6332_AUXADC_ADC15       ((UINT32)(MT6332_PMIC_REG_BASE+0x081E))
#define MT6332_AUXADC_ADC16       ((UINT32)(MT6332_PMIC_REG_BASE+0x0820))
#define MT6332_AUXADC_ADC17       ((UINT32)(MT6332_PMIC_REG_BASE+0x0822))
#define MT6332_AUXADC_ADC18       ((UINT32)(MT6332_PMIC_REG_BASE+0x0824))
#define MT6332_AUXADC_ADC19       ((UINT32)(MT6332_PMIC_REG_BASE+0x0826))
#define MT6332_AUXADC_ADC20       ((UINT32)(MT6332_PMIC_REG_BASE+0x0828))
#define MT6332_AUXADC_ADC21       ((UINT32)(MT6332_PMIC_REG_BASE+0x082A))
#define MT6332_AUXADC_ADC22       ((UINT32)(MT6332_PMIC_REG_BASE+0x082C))
#define MT6332_AUXADC_ADC23       ((UINT32)(MT6332_PMIC_REG_BASE+0x082E))
#define MT6332_AUXADC_ADC24       ((UINT32)(MT6332_PMIC_REG_BASE+0x0830))
#define MT6332_AUXADC_ADC25       ((UINT32)(MT6332_PMIC_REG_BASE+0x0832))
#define MT6332_AUXADC_ADC26       ((UINT32)(MT6332_PMIC_REG_BASE+0x0834))
#define MT6332_AUXADC_ADC27       ((UINT32)(MT6332_PMIC_REG_BASE+0x0836))
#define MT6332_AUXADC_ADC28       ((UINT32)(MT6332_PMIC_REG_BASE+0x0838))
#define MT6332_AUXADC_ADC29       ((UINT32)(MT6332_PMIC_REG_BASE+0x083A))
#define MT6332_AUXADC_ADC30       ((UINT32)(MT6332_PMIC_REG_BASE+0x083C))
#define MT6332_AUXADC_ADC31       ((UINT32)(MT6332_PMIC_REG_BASE+0x083E))
#define MT6332_AUXADC_ADC32       ((UINT32)(MT6332_PMIC_REG_BASE+0x0840))
#define MT6332_AUXADC_ADC33       ((UINT32)(MT6332_PMIC_REG_BASE+0x0842))
#define MT6332_AUXADC_ADC34       ((UINT32)(MT6332_PMIC_REG_BASE+0x0844))
#define MT6332_AUXADC_ADC35       ((UINT32)(MT6332_PMIC_REG_BASE+0x0846))
#define MT6332_AUXADC_ADC36       ((UINT32)(MT6332_PMIC_REG_BASE+0x0848))
#define MT6332_AUXADC_ADC37       ((UINT32)(MT6332_PMIC_REG_BASE+0x084A))
#define MT6332_AUXADC_ADC38       ((UINT32)(MT6332_PMIC_REG_BASE+0x084C))
#define MT6332_AUXADC_ADC39       ((UINT32)(MT6332_PMIC_REG_BASE+0x084E))
#define MT6332_AUXADC_ADC40       ((UINT32)(MT6332_PMIC_REG_BASE+0x0850))
#define MT6332_AUXADC_ADC41       ((UINT32)(MT6332_PMIC_REG_BASE+0x0852))
#define MT6332_AUXADC_ADC42       ((UINT32)(MT6332_PMIC_REG_BASE+0x0854))
#define MT6332_AUXADC_ADC43       ((UINT32)(MT6332_PMIC_REG_BASE+0x0856))
#define MT6332_AUXADC_STA0        ((UINT32)(MT6332_PMIC_REG_BASE+0x0858))
#define MT6332_AUXADC_STA1        ((UINT32)(MT6332_PMIC_REG_BASE+0x085A))
#define MT6332_AUXADC_RQST0       ((UINT32)(MT6332_PMIC_REG_BASE+0x085C))
#define MT6332_AUXADC_RQST0_SET   ((UINT32)(MT6332_PMIC_REG_BASE+0x085E))
#define MT6332_AUXADC_RQST0_CLR   ((UINT32)(MT6332_PMIC_REG_BASE+0x0860))
#define MT6332_AUXADC_RQST1       ((UINT32)(MT6332_PMIC_REG_BASE+0x0862))
#define MT6332_AUXADC_RQST1_SET   ((UINT32)(MT6332_PMIC_REG_BASE+0x0864))
#define MT6332_AUXADC_RQST1_CLR   ((UINT32)(MT6332_PMIC_REG_BASE+0x0866))
#define MT6332_AUXADC_CON0        ((UINT32)(MT6332_PMIC_REG_BASE+0x0868))
#define MT6332_AUXADC_CON1        ((UINT32)(MT6332_PMIC_REG_BASE+0x086A))
#define MT6332_AUXADC_CON2        ((UINT32)(MT6332_PMIC_REG_BASE+0x086C))
#define MT6332_AUXADC_CON3        ((UINT32)(MT6332_PMIC_REG_BASE+0x086E))
#define MT6332_AUXADC_CON4        ((UINT32)(MT6332_PMIC_REG_BASE+0x0870))
#define MT6332_AUXADC_CON5        ((UINT32)(MT6332_PMIC_REG_BASE+0x0872))
#define MT6332_AUXADC_CON6        ((UINT32)(MT6332_PMIC_REG_BASE+0x0874))
#define MT6332_AUXADC_CON7        ((UINT32)(MT6332_PMIC_REG_BASE+0x0876))
#define MT6332_AUXADC_CON8        ((UINT32)(MT6332_PMIC_REG_BASE+0x0878))
#define MT6332_AUXADC_CON9        ((UINT32)(MT6332_PMIC_REG_BASE+0x087A))
#define MT6332_AUXADC_CON10       ((UINT32)(MT6332_PMIC_REG_BASE+0x087C))
#define MT6332_AUXADC_CON11       ((UINT32)(MT6332_PMIC_REG_BASE+0x087E))
#define MT6332_AUXADC_CON12       ((UINT32)(MT6332_PMIC_REG_BASE+0x0880))
#define MT6332_AUXADC_CON13       ((UINT32)(MT6332_PMIC_REG_BASE+0x0882))
#define MT6332_AUXADC_CON14       ((UINT32)(MT6332_PMIC_REG_BASE+0x0884))
#define MT6332_AUXADC_CON15       ((UINT32)(MT6332_PMIC_REG_BASE+0x0886))
#define MT6332_AUXADC_CON16       ((UINT32)(MT6332_PMIC_REG_BASE+0x0888))
#define MT6332_AUXADC_CON17       ((UINT32)(MT6332_PMIC_REG_BASE+0x088A))
#define MT6332_AUXADC_CON18       ((UINT32)(MT6332_PMIC_REG_BASE+0x088C))
#define MT6332_AUXADC_CON19       ((UINT32)(MT6332_PMIC_REG_BASE+0x088E))
#define MT6332_AUXADC_CON20       ((UINT32)(MT6332_PMIC_REG_BASE+0x0890))
#define MT6332_AUXADC_CON21       ((UINT32)(MT6332_PMIC_REG_BASE+0x0892))
#define MT6332_AUXADC_CON22       ((UINT32)(MT6332_PMIC_REG_BASE+0x0894))
#define MT6332_AUXADC_CON23       ((UINT32)(MT6332_PMIC_REG_BASE+0x0896))
#define MT6332_AUXADC_CON24       ((UINT32)(MT6332_PMIC_REG_BASE+0x0898))
#define MT6332_AUXADC_CON25       ((UINT32)(MT6332_PMIC_REG_BASE+0x089A))
#define MT6332_AUXADC_CON26       ((UINT32)(MT6332_PMIC_REG_BASE+0x089C))
#define MT6332_AUXADC_CON27       ((UINT32)(MT6332_PMIC_REG_BASE+0x089E))
#define MT6332_AUXADC_CON28       ((UINT32)(MT6332_PMIC_REG_BASE+0x08A0))
#define MT6332_AUXADC_CON29       ((UINT32)(MT6332_PMIC_REG_BASE+0x08A2))
#define MT6332_AUXADC_CON30       ((UINT32)(MT6332_PMIC_REG_BASE+0x08A4))
#define MT6332_AUXADC_CON31       ((UINT32)(MT6332_PMIC_REG_BASE+0x08A6))
#define MT6332_AUXADC_CON32       ((UINT32)(MT6332_PMIC_REG_BASE+0x08A8))
#define MT6332_AUXADC_CON33       ((UINT32)(MT6332_PMIC_REG_BASE+0x08AA))
#define MT6332_AUXADC_CON34       ((UINT32)(MT6332_PMIC_REG_BASE+0x08AC))
#define MT6332_AUXADC_CON35       ((UINT32)(MT6332_PMIC_REG_BASE+0x08AE))
#define MT6332_AUXADC_CON36       ((UINT32)(MT6332_PMIC_REG_BASE+0x08B0))
#define MT6332_AUXADC_CON37       ((UINT32)(MT6332_PMIC_REG_BASE+0x08B2))
#define MT6332_AUXADC_CON38       ((UINT32)(MT6332_PMIC_REG_BASE+0x08B4))
#define MT6332_AUXADC_CON39       ((UINT32)(MT6332_PMIC_REG_BASE+0x08B6))
#define MT6332_AUXADC_CON40       ((UINT32)(MT6332_PMIC_REG_BASE+0x08B8))
#define MT6332_AUXADC_CON41       ((UINT32)(MT6332_PMIC_REG_BASE+0x08BA))
#define MT6332_AUXADC_CON42       ((UINT32)(MT6332_PMIC_REG_BASE+0x08BC))
#define MT6332_AUXADC_CON43       ((UINT32)(MT6332_PMIC_REG_BASE+0x08BE))
#define MT6332_AUXADC_CON44       ((UINT32)(MT6332_PMIC_REG_BASE+0x08C0))
#define MT6332_AUXADC_CON45       ((UINT32)(MT6332_PMIC_REG_BASE+0x08C2))
#define MT6332_AUXADC_CON46       ((UINT32)(MT6332_PMIC_REG_BASE+0x08C4))
#define MT6332_AUXADC_CON47       ((UINT32)(MT6332_PMIC_REG_BASE+0x08C6))


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


