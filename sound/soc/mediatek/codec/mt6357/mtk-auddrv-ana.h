/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/******************************************************************************
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
 *-----------------------------------------------------------------------------
 *
 *
 *****************************************************************************/

#ifndef _AUDDRV_ANA_H_
#define _AUDDRV_ANA_H_

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/
#include "mtk-auddrv-def.h"

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/


/*****************************************************************************
 *                         M A C R O
 *****************************************************************************/

/*****************************************************************************
 *                  R E G I S T E R       D E F I N I T I O N
 *****************************************************************************/

#define PMIC_REG_BASE          (0x0)
#define AUD_PMIC_REG_BASE          (0x0+0x2080)
#define AUD_TOP_ID           ((unsigned int)(AUD_PMIC_REG_BASE+0x0000))
#define AUD_TOP_REV0         ((unsigned int)(AUD_PMIC_REG_BASE+0x0002))
#define AUD_TOP_REV1          ((unsigned int)(AUD_PMIC_REG_BASE+0x0004))
#define AUD_TOP_DXI          ((unsigned int)(AUD_PMIC_REG_BASE+0x0006))
#define AUD_TOP_CKPDN_PM0   ((unsigned int)(AUD_PMIC_REG_BASE+0x0008))
#define AUD_TOP_CKPDN_PM1  ((unsigned int)(AUD_PMIC_REG_BASE+0x000A))
#ifdef AUD_TOP_CKPDN_CON0
#undef AUD_TOP_CKPDN_CON0
#endif
#define AUD_TOP_CKPDN_CON0   ((unsigned int)(AUD_PMIC_REG_BASE+0x000C))
#define AUD_TOP_CKPDN_CON0_SET ((unsigned int)(AUD_PMIC_REG_BASE+0x000E))
#define AUD_TOP_CKPDN_CON0_CLR ((unsigned int)(AUD_PMIC_REG_BASE+0x0010))
#define AUD_TOP_CKSEL_CON0   ((unsigned int)(AUD_PMIC_REG_BASE+0x0012))
#define AUD_TOP_CKSEL_CON0_SET ((unsigned int)(AUD_PMIC_REG_BASE+0x0014))
#ifdef AUD_TOP_RST_CON0
#undef AUD_TOP_RST_CON0
#endif
#define AUD_TOP_CKSEL_CON0_CLR ((unsigned int)(AUD_PMIC_REG_BASE+0x0016))
#define AUD_TOP_CKTST_CON0   ((unsigned int)(AUD_PMIC_REG_BASE+0x0018))
#ifdef AUD_TOP_INT_CON0
#undef AUD_TOP_INT_CON0
#endif
#define AUD_TOP_RST_CON0     ((unsigned int)(AUD_PMIC_REG_BASE+0x001A))
#ifdef AUD_TOP_INT_CON0_SET
#undef AUD_TOP_INT_CON0_SET
#endif
#define AUD_TOP_RST_CON0_SET ((unsigned int)(AUD_PMIC_REG_BASE+0x001C))
#ifdef AUD_TOP_INT_CON0_CLR
#undef AUD_TOP_INT_CON0_CLR
#endif
#define AUD_TOP_RST_CON0_CLR ((unsigned int)(AUD_PMIC_REG_BASE+0x001E))
#ifdef AUD_TOP_INT_MASK_CON0
#undef AUD_TOP_INT_MASK_CON0
#endif
#define AUD_TOP_RST_BANK_CON0 ((unsigned int)(AUD_PMIC_REG_BASE+0x0020))
#define AUD_TOP_INT_CON0     ((unsigned int)(AUD_PMIC_REG_BASE+0x0022))
#define AUD_TOP_INT_CON0_SET ((unsigned int)(AUD_PMIC_REG_BASE+0x0024))
#define AUD_TOP_INT_CON0_CLR ((unsigned int)(AUD_PMIC_REG_BASE+0x0026))
#define AUD_TOP_INT_MASK_CON0 ((unsigned int)(AUD_PMIC_REG_BASE+0x0028))
#ifdef AUD_TOP_INT_MASK_CON0_SET
#undef AUD_TOP_INT_MASK_CON0_SET
#endif
#define AUD_TOP_INT_MASK_CON0_SET ((unsigned int)(AUD_PMIC_REG_BASE+0x002A))
#ifdef AUD_TOP_INT_MASK_CON0_CLR
#undef AUD_TOP_INT_MASK_CON0_CLR
#endif
#define AUD_TOP_INT_MASK_CON0_CLR ((unsigned int)(AUD_PMIC_REG_BASE+0x002C))
#ifdef AUD_TOP_INT_STATUS0
#undef AUD_TOP_INT_STATUS0
#endif
#define AUD_TOP_INT_STATUS0  ((unsigned int)(AUD_PMIC_REG_BASE+0x002E))
#define AUD_TOP_INT_RAW_STATUS0 ((unsigned int)(AUD_PMIC_REG_BASE+0x0030))
#define AUD_TOP_INT_MISC_CON0 ((unsigned int)(AUD_PMIC_REG_BASE+0x0032))
#define AUDNCP_CLKDIV_CON0   ((unsigned int)(AUD_PMIC_REG_BASE+0x0034))
#define AUDNCP_CLKDIV_CON1   ((unsigned int)(AUD_PMIC_REG_BASE+0x0036))
#define AUDNCP_CLKDIV_CON2   ((unsigned int)(AUD_PMIC_REG_BASE+0x0038))
#define AUDNCP_CLKDIV_CON3   ((unsigned int)(AUD_PMIC_REG_BASE+0x003A))
#define AUDNCP_CLKDIV_CON4   ((unsigned int)(AUD_PMIC_REG_BASE+0x003C))
#define AUD_TOP_MON_CON0     ((unsigned int)(AUD_PMIC_REG_BASE+0x003E))
#define AUDIO_DIG_ID                  ((unsigned int)(PMIC_REG_BASE + 0x1580))
#define AUDIO_DIG_REV0    ((unsigned int)(AUD_PMIC_REG_BASE+0x0082))
#define AUDIO_DIG_REV1    ((unsigned int)(AUD_PMIC_REG_BASE+0x0084))
#define AUDIO_DIG_DSN_DXI    ((unsigned int)(AUD_PMIC_REG_BASE+0x0086))
#define AFE_UL_DL_CON0       ((unsigned int)(AUD_PMIC_REG_BASE+0x0088))
#define AFE_DL_SRC2_CON0_L   ((unsigned int)(AUD_PMIC_REG_BASE+0x008A))
#define AFE_UL_SRC_CON0_H    ((unsigned int)(AUD_PMIC_REG_BASE+0x008C))
#define AFE_UL_SRC_CON0_L    ((unsigned int)(AUD_PMIC_REG_BASE+0x008E))
#define PMIC_AFE_TOP_CON0         ((unsigned int)(AUD_PMIC_REG_BASE+0x0090))
#define PMIC_AUDIO_TOP_CON0       ((unsigned int)(AUD_PMIC_REG_BASE+0x0092))
#define AFE_MON_DEBUG0       ((unsigned int)(AUD_PMIC_REG_BASE+0x0094))
#define AFUNC_AUD_CON0       ((unsigned int)(AUD_PMIC_REG_BASE+0x0096))
#define AFUNC_AUD_CON1       ((unsigned int)(AUD_PMIC_REG_BASE+0x0098))
#define AFUNC_AUD_CON2       ((unsigned int)(AUD_PMIC_REG_BASE+0x009A))
#define AFUNC_AUD_CON3       ((unsigned int)(AUD_PMIC_REG_BASE+0x009C))
#define AFUNC_AUD_CON4       ((unsigned int)(AUD_PMIC_REG_BASE+0x009E))
#define AFUNC_AUD_CON5       ((unsigned int)(AUD_PMIC_REG_BASE+0x00A0))
#define AFUNC_AUD_CON6       ((unsigned int)(AUD_PMIC_REG_BASE+0x00A2))
#define AFUNC_AUD_MON0       ((unsigned int)(AUD_PMIC_REG_BASE+0x00A4))
#define AUDRC_TUNE_MON0      ((unsigned int)(AUD_PMIC_REG_BASE+0x00A6))
#define AFE_ADDA_MTKAIF_FIFO_CFG0 ((unsigned int)(AUD_PMIC_REG_BASE+0x00A8))
#define AFE_ADDA_MTKAIF_FIFO_LOG_MON1 ((unsigned int)(AUD_PMIC_REG_BASE+0x00AA))
#define PMIC_AFE_ADDA_MTKAIF_MON0 ((unsigned int)(AUD_PMIC_REG_BASE+0x00AC))
#define PMIC_AFE_ADDA_MTKAIF_MON1 ((unsigned int)(AUD_PMIC_REG_BASE+0x00AE))
#define PMIC_AFE_ADDA_MTKAIF_MON2 ((unsigned int)(AUD_PMIC_REG_BASE+0x00B0))
#define PMIC_AFE_ADDA_MTKAIF_MON3 ((unsigned int)(AUD_PMIC_REG_BASE+0x00B2))
#define PMIC_AFE_ADDA_MTKAIF_CFG0 ((unsigned int)(AUD_PMIC_REG_BASE+0x00B4))
#define PMIC_AFE_ADDA_MTKAIF_RX_CFG0 ((unsigned int)(AUD_PMIC_REG_BASE+0x00B6))
#define PMIC_AFE_ADDA_MTKAIF_RX_CFG1 ((unsigned int)(AUD_PMIC_REG_BASE+0x00B8))
#define PMIC_AFE_ADDA_MTKAIF_RX_CFG2 ((unsigned int)(AUD_PMIC_REG_BASE+0x00BA))
#define PMIC_AFE_ADDA_MTKAIF_RX_CFG3 ((unsigned int)(AUD_PMIC_REG_BASE+0x00BC))
#define PMIC_AFE_ADDA_MTKAIF_TX_CFG1 ((unsigned int)(AUD_PMIC_REG_BASE+0x00BE))
#define AFE_SGEN_CFG0        ((unsigned int)(AUD_PMIC_REG_BASE+0x00C0))
#define AFE_SGEN_CFG1        ((unsigned int)(AUD_PMIC_REG_BASE+0x00C2))
#define AFE_ADC_ASYNC_FIFO_CFG ((unsigned int)(AUD_PMIC_REG_BASE+0x00C4))
#define AFE_DCCLK_CFG0       ((unsigned int)(AUD_PMIC_REG_BASE+0x00C6))
#define AFE_DCCLK_CFG1       ((unsigned int)(AUD_PMIC_REG_BASE+0x00C8))
#define AUDIO_DIG_CFG        ((unsigned int)(AUD_PMIC_REG_BASE+0x00CA))
#define AFE_AUD_PAD_TOP      ((unsigned int)(AUD_PMIC_REG_BASE+0x00CC))
#define AFE_AUD_PAD_TOP_MON  ((unsigned int)(AUD_PMIC_REG_BASE+0x00CE))
#define AFE_AUD_PAD_TOP_MON1 ((unsigned int)(AUD_PMIC_REG_BASE+0x00D0))
#define AUDENC_DSN_ID        ((unsigned int)(AUD_PMIC_REG_BASE+0x0100))
#define AUDENC_DSN_REV0      ((unsigned int)(AUD_PMIC_REG_BASE+0x0102))
#define AUDENC_DSN_REV1        ((unsigned int)(AUD_PMIC_REG_BASE+0x0104))
#define AUDENC_DSN_FPI       ((unsigned int)(AUD_PMIC_REG_BASE+0x0106))
#define AUDENC_ANA_CON0      ((unsigned int)(AUD_PMIC_REG_BASE+0x0108))
#define AUDENC_ANA_CON1      ((unsigned int)(AUD_PMIC_REG_BASE+0x010A))
#define AUDENC_ANA_CON2      ((unsigned int)(AUD_PMIC_REG_BASE+0x010C))
#define AUDENC_ANA_CON3      ((unsigned int)(AUD_PMIC_REG_BASE+0x010E))
#define AUDENC_ANA_CON4      ((unsigned int)(AUD_PMIC_REG_BASE+0x0110))
#define AUDENC_ANA_CON5      ((unsigned int)(AUD_PMIC_REG_BASE+0x0112))
#ifdef AUDENC_ANA_CON6
#undef AUDENC_ANA_CON6
#endif
#define AUDENC_ANA_CON6      ((unsigned int)(AUD_PMIC_REG_BASE+0x0114))
#define AUDENC_ANA_CON7      ((unsigned int)(AUD_PMIC_REG_BASE+0x0116))
#define AUDENC_ANA_CON8      ((unsigned int)(AUD_PMIC_REG_BASE+0x0118))
#ifdef AUDENC_ANA_CON9
#undef AUDENC_ANA_CON9
#endif
#define AUDENC_ANA_CON9      ((unsigned int)(AUD_PMIC_REG_BASE+0x011A))
#ifdef AUDENC_ANA_CON10
#undef AUDENC_ANA_CON10
#endif
#define AUDENC_ANA_CON10     ((unsigned int)(AUD_PMIC_REG_BASE+0x011C))
#ifdef AUDENC_ANA_CON11
#undef AUDENC_ANA_CON11
#endif
#define AUDENC_ANA_CON11     ((unsigned int)(AUD_PMIC_REG_BASE+0x011E))
#define AUDDEC_DSN_ID        ((unsigned int)(AUD_PMIC_REG_BASE+0x0180))
#define AUDDEC_DSN_REV0      ((unsigned int)(AUD_PMIC_REG_BASE+0x0182))
#define AUDDEC_DSN_REV1        ((unsigned int)(AUD_PMIC_REG_BASE+0x0184))
#define AUDDEC_DSN_FPI       ((unsigned int)(AUD_PMIC_REG_BASE+0x0186))
#define AUDDEC_ANA_CON0      ((unsigned int)(AUD_PMIC_REG_BASE+0x0188))
#define AUDDEC_ANA_CON1      ((unsigned int)(AUD_PMIC_REG_BASE+0x018A))
#define AUDDEC_ANA_CON2      ((unsigned int)(AUD_PMIC_REG_BASE+0x018C))
#define AUDDEC_ANA_CON3      ((unsigned int)(AUD_PMIC_REG_BASE+0x018E))
#define AUDDEC_ANA_CON4      ((unsigned int)(AUD_PMIC_REG_BASE+0x0190))
#define AUDDEC_ANA_CON5      ((unsigned int)(AUD_PMIC_REG_BASE+0x0192))
#define AUDDEC_ANA_CON6      ((unsigned int)(AUD_PMIC_REG_BASE+0x0194))
#define AUDDEC_ANA_CON7      ((unsigned int)(AUD_PMIC_REG_BASE+0x0196))
#define AUDDEC_ANA_CON8      ((unsigned int)(AUD_PMIC_REG_BASE+0x0198))
#define AUDDEC_ANA_CON9      ((unsigned int)(AUD_PMIC_REG_BASE+0x019A))
#define AUDDEC_ANA_CON10     ((unsigned int)(AUD_PMIC_REG_BASE+0x019C))
#define AUDDEC_ANA_CON11     ((unsigned int)(AUD_PMIC_REG_BASE+0x019E))
#define AUDDEC_ANA_CON12     ((unsigned int)(AUD_PMIC_REG_BASE+0x01A0))
#define AUDDEC_ANA_CON13     ((unsigned int)(AUD_PMIC_REG_BASE+0x01A2))
#define AUDDEC_ELR_NUM       ((unsigned int)(AUD_PMIC_REG_BASE+0x01A4))
#define AUDDEC_ELR_0         ((unsigned int)(AUD_PMIC_REG_BASE+0x01A6))
#define AUDZCDID        ((unsigned int)(AUD_PMIC_REG_BASE+0x0200))
#define AUDZCDREV0      ((unsigned int)(AUD_PMIC_REG_BASE+0x0202))
#define AUDZCDREV1        ((unsigned int)(AUD_PMIC_REG_BASE+0x0204))
#define AUDZCD_DSN_FPI       ((unsigned int)(AUD_PMIC_REG_BASE+0x0206))
#define ZCD_CON0             ((unsigned int)(AUD_PMIC_REG_BASE+0x0208))
#define ZCD_CON1             ((unsigned int)(AUD_PMIC_REG_BASE+0x020A))
#define ZCD_CON2             ((unsigned int)(AUD_PMIC_REG_BASE+0x020C))
#define ZCD_CON3             ((unsigned int)(AUD_PMIC_REG_BASE+0x020E))
#define ZCD_CON4             ((unsigned int)(AUD_PMIC_REG_BASE+0x0210))
#define ZCD_CON5             ((unsigned int)(AUD_PMIC_REG_BASE+0x0212))

#ifdef TOP_CKPDN_CON0
#undef TOP_CKPDN_CON0
#endif
#define TOP_CKPDN_CON0       ((unsigned int)(PMIC_REG_BASE+0x0000+0x010C))
#ifdef TOP_CKPDN_CON0_SET
#undef TOP_CKPDN_CON0_SET
#endif
#define TOP_CKPDN_CON0_SET   ((unsigned int)(PMIC_REG_BASE+0x0000+0x010E))
#ifdef TOP_CKPDN_CON0_CLR
#undef TOP_CKPDN_CON0_CLR
#endif
#define TOP_CKPDN_CON0_CLR   ((unsigned int)(PMIC_REG_BASE+0x0000+0x0110))

#define TOP_CKHWEN_CON0      ((unsigned int)(PMIC_REG_BASE+0x0000+0x012A))
#define TOP_CKHWEN_CON0_SET  ((unsigned int)(PMIC_REG_BASE+0x0000+0x012C))
#define TOP_CKHWEN_CON0_CLR  ((unsigned int)(PMIC_REG_BASE+0x0000+0x012E))

#define OTP_CON0             ((unsigned int)(PMIC_REG_BASE+0x0380+0x0010))
#define OTP_CON8             ((unsigned int)(PMIC_REG_BASE+0x0380+0x0020))
#define OTP_CON11            ((unsigned int)(PMIC_REG_BASE+0x0380+0x0026))
#define OTP_CON12            ((unsigned int)(PMIC_REG_BASE+0x0380+0x0028))
#define OTP_CON13            ((unsigned int)(PMIC_REG_BASE+0x0380+0x002A))

#define DRV_CON3             ((unsigned int)(PMIC_REG_BASE+0x0000+0x0038))
#define GPIO_DIR0            ((unsigned int)(PMIC_REG_BASE+0x0000+0x0088))
/* mosi */
#define GPIO_MODE2           ((unsigned int)(PMIC_REG_BASE+0x0000+0x00B6))
#define GPIO_MODE2_SET       ((unsigned int)(PMIC_REG_BASE+0x0000+0x00B8))
#define GPIO_MODE2_CLR       ((unsigned int)(PMIC_REG_BASE+0x0000+0x00BA))
#define SMT_CON1           ((unsigned int)(PMIC_REG_BASE+0x0000+0x002c))

/* miso */
#define GPIO_MODE3           ((unsigned int)(PMIC_REG_BASE+0x0000+0x00BC))
#define GPIO_MODE3_SET       ((unsigned int)(PMIC_REG_BASE+0x0000+0x00BE))
#define GPIO_MODE3_CLR       ((unsigned int)(PMIC_REG_BASE+0x0000+0x00C0))

#define DCXO_CW14            ((unsigned int)(PMIC_REG_BASE+0x0780+0x002C))

#define AUXADC_CON10         ((unsigned int)(PMIC_REG_BASE+0x0F80+0x01B8))

void Ana_Set_Reg(unsigned int offset, unsigned int value, unsigned int mask);
unsigned int Ana_Get_Reg(unsigned int offset);

/* for debug usage */
void Ana_Log_Print(void);

int Ana_Debug_Read(char *buffer, const int size);

#endif
