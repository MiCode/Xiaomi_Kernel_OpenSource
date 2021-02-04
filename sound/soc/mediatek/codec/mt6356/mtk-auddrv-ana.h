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
 *	AudDrv_Ana.h
 *
 * Project:
 * --------
 *	MT6797  Audio Driver Ana
 *
 * Description:
 * ------------
 *	Audio register
 *
 * Author:
 * -------
 *	Chipeng Chang (mtk02308)
 *
 *------------------------------------------------------------------------------
 *
 *
 ******************************************************************************
 */

#ifndef _AUDDRV_ANA_H_
#define _AUDDRV_ANA_H_

/*****************************************************************************
 *					 C O M P I L E R	F L A G S
 *****************************************************************************/

/*****************************************************************************
 *				E X T E R N A L	R E F E R E N C E S
 *****************************************************************************/
#include "mtk-auddrv-def.h"

/*****************************************************************************
 *						 D A T A	T Y P E S
 *****************************************************************************/

/*****************************************************************************
 *						 M A C R O
 *****************************************************************************/

/*****************************************************************************
 *				  R E G I S T E R		D E F I N I T I
 *O
 *N
 *****************************************************************************/
#define PMIC_REG_BASE (0x0)
#define AUD_TOP_ID ((unsigned int)(PMIC_REG_BASE + 0x1540))
#define AUD_TOP_REV0 ((unsigned int)(PMIC_REG_BASE + 0x1542))
#define AUD_TOP_REV1 ((unsigned int)(PMIC_REG_BASE + 0x1544))
#define AUD_TOP_CKPDN_PM0 ((unsigned int)(PMIC_REG_BASE + 0x1546))
#define AUD_TOP_CKPDN_PM1 ((unsigned int)(PMIC_REG_BASE + 0x1548))
#ifdef AUD_TOP_CKPDN_CON0
#undef AUD_TOP_CKPDN_CON0
#endif
#define AUD_TOP_CKPDN_CON0 ((unsigned int)(PMIC_REG_BASE + 0x154a))
#define AUD_TOP_CKSEL_CON0 ((unsigned int)(PMIC_REG_BASE + 0x154c))
#define AUD_TOP_CKTST_CON0 ((unsigned int)(PMIC_REG_BASE + 0x154e))
#ifdef AUD_TOP_RST_CON0
#undef AUD_TOP_RST_CON0
#endif
#define AUD_TOP_RST_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1550))
#define AUD_TOP_RST_BANK_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1552))
#ifdef AUD_TOP_INT_CON0
#undef AUD_TOP_INT_CON0
#endif
#define AUD_TOP_INT_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1554))
#ifdef AUD_TOP_INT_CON0_SET
#undef AUD_TOP_INT_CON0_SET
#endif
#define AUD_TOP_INT_CON0_SET ((unsigned int)(PMIC_REG_BASE + 0x1556))
#ifdef AUD_TOP_INT_CON0_CLR
#undef AUD_TOP_INT_CON0_CLR
#endif
#define AUD_TOP_INT_CON0_CLR ((unsigned int)(PMIC_REG_BASE + 0x1558))
#ifdef AUD_TOP_INT_MASK_CON0
#undef AUD_TOP_INT_MASK_CON0
#endif
#define AUD_TOP_INT_MASK_CON0 ((unsigned int)(PMIC_REG_BASE + 0x155a))
#ifdef AUD_TOP_INT_MASK_CON0_SET
#undef AUD_TOP_INT_MASK_CON0_SET
#endif
#define AUD_TOP_INT_MASK_CON0_SET ((unsigned int)(PMIC_REG_BASE + 0x155c))
#ifdef AUD_TOP_INT_MASK_CON0_CLR
#undef AUD_TOP_INT_MASK_CON0_CLR
#endif
#define AUD_TOP_INT_MASK_CON0_CLR ((unsigned int)(PMIC_REG_BASE + 0x155e))
#ifdef AUD_TOP_INT_STATUS0
#undef AUD_TOP_INT_STATUS0
#endif
#define AUD_TOP_INT_STATUS0 ((unsigned int)(PMIC_REG_BASE + 0x1560))
#define AUD_TOP_INT_RAW_STATUS0 ((unsigned int)(PMIC_REG_BASE + 0x1562))
#define AUD_TOP_INT_MISC_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1564))
#define AUDNCP_CLKDIV_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1566))
#define AUDNCP_CLKDIV_CON1 ((unsigned int)(PMIC_REG_BASE + 0x1568))
#define AUDNCP_CLKDIV_CON2 ((unsigned int)(PMIC_REG_BASE + 0x156a))
#define AUDNCP_CLKDIV_CON3 ((unsigned int)(PMIC_REG_BASE + 0x156c))
#define AUDNCP_CLKDIV_CON4 ((unsigned int)(PMIC_REG_BASE + 0x156e))
#define AUD_TOP_MON_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1570))
#define AUDIO_DIG_ID ((unsigned int)(PMIC_REG_BASE + 0x1580))
#define AUDIO_DIG_REV0 ((unsigned int)(PMIC_REG_BASE + 0x1582))
#define AUDIO_DIG_REV1 ((unsigned int)(PMIC_REG_BASE + 0x1584))
#define AFE_UL_DL_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1586))
#define AFE_DL_SRC2_CON0_L ((unsigned int)(PMIC_REG_BASE + 0x1588))
#define AFE_UL_SRC_CON0_H ((unsigned int)(PMIC_REG_BASE + 0x158a))
#define AFE_UL_SRC_CON0_L ((unsigned int)(PMIC_REG_BASE + 0x158c))
#define PMIC_AFE_TOP_CON0 ((unsigned int)(PMIC_REG_BASE + 0x158e))
#define PMIC_AUDIO_TOP_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1590))
#define AFE_MON_DEBUG0 ((unsigned int)(PMIC_REG_BASE + 0x1592))
#define AFUNC_AUD_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1594))
#define AFUNC_AUD_CON1 ((unsigned int)(PMIC_REG_BASE + 0x1596))
#define AFUNC_AUD_CON2 ((unsigned int)(PMIC_REG_BASE + 0x1598))
#define AFUNC_AUD_CON3 ((unsigned int)(PMIC_REG_BASE + 0x159a))
#define AFUNC_AUD_CON4 ((unsigned int)(PMIC_REG_BASE + 0x159c))
#define AFUNC_AUD_MON0 ((unsigned int)(PMIC_REG_BASE + 0x159e))
#define AUDRC_TUNE_MON0 ((unsigned int)(PMIC_REG_BASE + 0x15a0))
#define AFE_ADDA_MTKAIF_FIFO_CFG0 ((unsigned int)(PMIC_REG_BASE + 0x15a2))
#define AFE_ADDA_MTKAIF_FIFO_LOG_MON1 ((unsigned int)(PMIC_REG_BASE + 0x15a4))
#define PMIC_AFE_ADDA_MTKAIF_MON0 ((unsigned int)(PMIC_REG_BASE + 0x15a6))
#define PMIC_AFE_ADDA_MTKAIF_MON1 ((unsigned int)(PMIC_REG_BASE + 0x15a8))
#define PMIC_AFE_ADDA_MTKAIF_MON2 ((unsigned int)(PMIC_REG_BASE + 0x15aa))
#define PMIC_AFE_ADDA_MTKAIF_MON3 ((unsigned int)(PMIC_REG_BASE + 0x15ac))
#define PMIC_AFE_ADDA_MTKAIF_CFG0 ((unsigned int)(PMIC_REG_BASE + 0x15ae))
#define PMIC_AFE_ADDA_MTKAIF_RX_CFG0 ((unsigned int)(PMIC_REG_BASE + 0x15b0))
#define PMIC_AFE_ADDA_MTKAIF_RX_CFG1 ((unsigned int)(PMIC_REG_BASE + 0x15b2))
#define PMIC_AFE_ADDA_MTKAIF_RX_CFG2 ((unsigned int)(PMIC_REG_BASE + 0x15b4))
#define PMIC_AFE_ADDA_MTKAIF_RX_CFG3 ((unsigned int)(PMIC_REG_BASE + 0x15b6))
#define PMIC_AFE_ADDA_MTKAIF_TX_CFG1 ((unsigned int)(PMIC_REG_BASE + 0x15b8))
#define AFE_SGEN_CFG0 ((unsigned int)(PMIC_REG_BASE + 0x15ba))
#define AFE_SGEN_CFG1 ((unsigned int)(PMIC_REG_BASE + 0x15bc))
#define AFE_ADC_ASYNC_FIFO_CFG ((unsigned int)(PMIC_REG_BASE + 0x15be))
#define AFE_DCCLK_CFG0 ((unsigned int)(PMIC_REG_BASE + 0x15c0))
#define AFE_DCCLK_CFG1 ((unsigned int)(PMIC_REG_BASE + 0x15c2))
#define AUDIO_DIG_CFG ((unsigned int)(PMIC_REG_BASE + 0x15c4))
#define AFE_AUD_PAD_TOP ((unsigned int)(PMIC_REG_BASE + 0x15c6))
#define AFE_AUD_PAD_TOP_MON ((unsigned int)(PMIC_REG_BASE + 0x15c8))
#define AFE_AUD_PAD_TOP_MON1 ((unsigned int)(PMIC_REG_BASE + 0x15ca))
#define AUDENC_DSN_ID ((unsigned int)(PMIC_REG_BASE + 0x1600))
#define AUDENC_DSN_REV0 ((unsigned int)(PMIC_REG_BASE + 0x1602))
#define AUDENC_DSN_REV1 ((unsigned int)(PMIC_REG_BASE + 0x1604))
#define AUDENC_ANA_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1606))
#define AUDENC_ANA_CON1 ((unsigned int)(PMIC_REG_BASE + 0x1608))
#define AUDENC_ANA_CON2 ((unsigned int)(PMIC_REG_BASE + 0x160a))
#define AUDENC_ANA_CON3 ((unsigned int)(PMIC_REG_BASE + 0x160c))
#define AUDENC_ANA_CON4 ((unsigned int)(PMIC_REG_BASE + 0x160e))
#define AUDENC_ANA_CON5 ((unsigned int)(PMIC_REG_BASE + 0x1610))
#define AUDENC_ANA_CON6 ((unsigned int)(PMIC_REG_BASE + 0x1612))
#define AUDENC_ANA_CON7 ((unsigned int)(PMIC_REG_BASE + 0x1614))
#define AUDENC_ANA_CON8 ((unsigned int)(PMIC_REG_BASE + 0x1616))
#define AUDENC_ANA_CON9 ((unsigned int)(PMIC_REG_BASE + 0x1618))
#ifdef AUDENC_ANA_CON10
#undef AUDENC_ANA_CON10
#endif
#define AUDENC_ANA_CON10 ((unsigned int)(PMIC_REG_BASE + 0x161a))
#ifdef AUDENC_ANA_CON11
#undef AUDENC_ANA_CON11
#endif
#define AUDENC_ANA_CON11 ((unsigned int)(PMIC_REG_BASE + 0x161c))
#define AUDENC_ANA_CON12 ((unsigned int)(PMIC_REG_BASE + 0x161e))
#define AUDDEC_DSN_ID ((unsigned int)(PMIC_REG_BASE + 0x1640))
#define AUDDEC_DSN_REV0 ((unsigned int)(PMIC_REG_BASE + 0x1642))
#define AUDDEC_DSN_REV1 ((unsigned int)(PMIC_REG_BASE + 0x1644))
#define AUDDEC_ANA_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1646))
#define AUDDEC_ANA_CON1 ((unsigned int)(PMIC_REG_BASE + 0x1648))
#define AUDDEC_ANA_CON2 ((unsigned int)(PMIC_REG_BASE + 0x164a))
#define AUDDEC_ANA_CON3 ((unsigned int)(PMIC_REG_BASE + 0x164c))
#define AUDDEC_ANA_CON4 ((unsigned int)(PMIC_REG_BASE + 0x164e))
#define AUDDEC_ANA_CON5 ((unsigned int)(PMIC_REG_BASE + 0x1650))
#define AUDDEC_ANA_CON6 ((unsigned int)(PMIC_REG_BASE + 0x1652))
#define AUDDEC_ANA_CON7 ((unsigned int)(PMIC_REG_BASE + 0x1654))
#define AUDDEC_ANA_CON8 ((unsigned int)(PMIC_REG_BASE + 0x1656))
#define AUDDEC_ANA_CON9 ((unsigned int)(PMIC_REG_BASE + 0x1658))
#define AUDDEC_ANA_CON10 ((unsigned int)(PMIC_REG_BASE + 0x165a))
#define AUDDEC_ANA_CON11 ((unsigned int)(PMIC_REG_BASE + 0x165c))
#define AUDDEC_ANA_CON12 ((unsigned int)(PMIC_REG_BASE + 0x165e))
#define AUDDEC_ANA_CON13 ((unsigned int)(PMIC_REG_BASE + 0x1660))
#define AUDDEC_ANA_CON14 ((unsigned int)(PMIC_REG_BASE + 0x1662))
#define AUDDEC_ANA_CON15 ((unsigned int)(PMIC_REG_BASE + 0x1664))
#define AUDDEC_ELR_NUM ((unsigned int)(PMIC_REG_BASE + 0x1666))
#define AUDDEC_ELR_0 ((unsigned int)(PMIC_REG_BASE + 0x1668))
#define AUDZCDID ((unsigned int)(PMIC_REG_BASE + 0x1680))
#define AUDZCDREV0 ((unsigned int)(PMIC_REG_BASE + 0x1682))
#define AUDZCDREV1 ((unsigned int)(PMIC_REG_BASE + 0x1684))
#define ZCD_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1686))
#define ZCD_CON1 ((unsigned int)(PMIC_REG_BASE + 0x1688))
#define ZCD_CON2 ((unsigned int)(PMIC_REG_BASE + 0x168a))
#define ZCD_CON3 ((unsigned int)(PMIC_REG_BASE + 0x168c))
#define ZCD_CON4 ((unsigned int)(PMIC_REG_BASE + 0x168e))
#define ZCD_CON5 ((unsigned int)(PMIC_REG_BASE + 0x1690))

#ifdef TOP_CKPDN_CON0
#undef TOP_CKPDN_CON0
#endif
#define TOP_CKPDN_CON0 ((unsigned int)(PMIC_REG_BASE + 0x10a))
#ifdef TOP_CKPDN_CON0_SET
#undef TOP_CKPDN_CON0_SET
#endif
#define TOP_CKPDN_CON0_SET ((unsigned int)(PMIC_REG_BASE + 0x10c))
#ifdef TOP_CKPDN_CON0_CLR
#undef TOP_CKPDN_CON0_CLR
#endif
#define TOP_CKPDN_CON0_CLR ((unsigned int)(PMIC_REG_BASE + 0x10e))

#define TOP_CKHWEN_CON0 ((unsigned int)(PMIC_REG_BASE + 0x128))
#define TOP_CKHWEN_CON0_SET ((unsigned int)(PMIC_REG_BASE + 0x12a))
#define TOP_CKHWEN_CON0_CLR ((unsigned int)(PMIC_REG_BASE + 0x12c))

#define TOP_CLKSQ ((unsigned int)(PMIC_REG_BASE + 0x132))
#define TOP_CLKSQ_SET ((unsigned int)(PMIC_REG_BASE + 0x134))
#define TOP_CLKSQ_CLR ((unsigned int)(PMIC_REG_BASE + 0x136))

#define OTP_CON0 ((unsigned int)(PMIC_REG_BASE + 0x1a2))
#define OTP_CON8 ((unsigned int)(PMIC_REG_BASE + 0x1b2))
#define OTP_CON11 ((unsigned int)(PMIC_REG_BASE + 0x1b8))
#define OTP_CON12 ((unsigned int)(PMIC_REG_BASE + 0x1ba))
#define OTP_CON13 ((unsigned int)(PMIC_REG_BASE + 0x1bc))

#define DRV_CON3 ((unsigned int)(PMIC_REG_BASE + 0x3a))
#define GPIO_DIR0 ((unsigned int)(PMIC_REG_BASE + 0x4c))

#define GPIO_MODE2 ((unsigned int)(PMIC_REG_BASE + 0x7a)) /* mosi */
#define GPIO_MODE2_SET ((unsigned int)(PMIC_REG_BASE + 0x7c))
#define GPIO_MODE2_CLR ((unsigned int)(PMIC_REG_BASE + 0x7e))

#define GPIO_MODE3 ((unsigned int)(PMIC_REG_BASE + 0x80)) /* miso */
#define GPIO_MODE3_SET ((unsigned int)(PMIC_REG_BASE + 0x82))
#define GPIO_MODE3_CLR ((unsigned int)(PMIC_REG_BASE + 0x84))

#define DCXO_CW14 ((unsigned int)(PMIC_REG_BASE + 0x9ea))

#define AUXADC_CON10 ((unsigned int)(PMIC_REG_BASE + 0x816))

#if 1
/* register number */

#else
#include <mach/upmu_hw.h>
#endif

void Ana_Set_Reg(unsigned int offset, unsigned int value, unsigned int mask);
unsigned int Ana_Get_Reg(unsigned int offset);

/* for debug usage */
void Ana_Log_Print(void);

int Ana_Debug_Read(char *buffer, const int size);

#endif
