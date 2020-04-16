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
 *   AudDrv_Ana.c
 *
 * Project:
 * --------
 *   MT6797  Audio Driver ana Register setting
 *
 * Description:
 * ------------
 *   Audio register
 *
 * Author:
 * -------
 * Chipeng Chang
 *
 *------------------------------------------------------------------------------
 *
 *
 ******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/
#include <linux/types.h>
#include <linux/kernel.h>

#include "mtk-auddrv-ana.h"
#include "mtk-soc-codec-63xx.h"

#ifdef AUDIO_USING_WRAP_DRIVER
/*#include <mach/mt_pmic_wrap.h>*/
#include <mach/mtk_pmic_wrap.h>
#endif


#ifdef AUDIO_USING_WRAP_DRIVER
static DEFINE_SPINLOCK(ana_set_reg_lock);
#endif

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/
unsigned int Ana_Get_Reg(unsigned int offset)
{
	/* get pmic register */
	unsigned int Rdata = 0;
#ifdef AUDIO_USING_WRAP_DRIVER
	int ret = 0;

	ret = pwrap_read(offset, &Rdata);
#endif

	return Rdata;
}
EXPORT_SYMBOL(Ana_Get_Reg);

void Ana_Set_Reg(unsigned int offset, unsigned int value, unsigned int mask)
{
	/* set pmic register or analog CONTROL_IFACE_PATH */

#ifdef AUDIO_USING_WRAP_DRIVER
	int ret = 0;
	unsigned int Reg_Value;
	unsigned long flags = 0;

	spin_lock_irqsave(&ana_set_reg_lock, flags);
	Reg_Value = Ana_Get_Reg(offset);
	Reg_Value &= (~mask);
	Reg_Value |= (value & mask);
	ret = pwrap_write(offset, Reg_Value);
	spin_unlock_irqrestore(&ana_set_reg_lock, flags);
#endif
}
EXPORT_SYMBOL(Ana_Set_Reg);

void Ana_Log_Print(void)
{
	audckbufEnable(true);
	pr_debug("AUD_TOP_ID = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_ID));
	pr_debug("AUD_TOP_REV0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_REV0));
	pr_debug("AUD_TOP_DBI = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_DBI));
	pr_debug("AUD_TOP_DXI = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_DXI));
	pr_debug("AUD_TOP_CKPDN_TPM0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_CKPDN_TPM0));
	pr_debug("AUD_TOP_CKPDN_TPM1 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_CKPDN_TPM1));
	pr_debug("AUD_TOP_CKPDN_CON0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_CKPDN_CON0));
	pr_debug("AUD_TOP_CKPDN_CON0_SET = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_CKPDN_CON0_SET));
	pr_debug("AUD_TOP_CKPDN_CON0_CLR = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_CKPDN_CON0_CLR));
	pr_debug("AUD_TOP_CKSEL_CON0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_CKSEL_CON0));
	pr_debug("AUD_TOP_CKSEL_CON0_SET = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_CKSEL_CON0_SET));
	pr_debug("AUD_TOP_CKSEL_CON0_CLR = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_CKSEL_CON0_CLR));
	pr_debug("AUD_TOP_CKTST_CON0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_CKTST_CON0));
	pr_debug("AUD_TOP_CLK_HWEN_CON0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_CLK_HWEN_CON0));
	pr_debug("AUD_TOP_CLK_HWEN_CON0_SET = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_CLK_HWEN_CON0_SET));
	pr_debug("AUD_TOP_CLK_HWEN_CON0_CLR = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_CLK_HWEN_CON0_CLR));
	pr_debug("AUD_TOP_RST_CON0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_RST_CON0));
	pr_debug("AUD_TOP_RST_CON0_SET = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_RST_CON0_SET));
	pr_debug("AUD_TOP_RST_CON0_CLR = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_RST_CON0_CLR));
	pr_debug("AUD_TOP_RST_BANK_CON0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_RST_BANK_CON0));
	pr_debug("AUD_TOP_INT_CON0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_INT_CON0));
	pr_debug("AUD_TOP_INT_CON0_SET = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_INT_CON0_SET));
	pr_debug("AUD_TOP_INT_CON0_CLR = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_INT_CON0_CLR));
	pr_debug("AUD_TOP_INT_MASK_CON0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_INT_MASK_CON0));
	pr_debug("AUD_TOP_INT_MASK_CON0_SET = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_INT_MASK_CON0_SET));
	pr_debug("AUD_TOP_INT_MASK_CON0_CLR = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_INT_MASK_CON0_CLR));
	pr_debug("AUD_TOP_INT_STATUS0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_INT_STATUS0));
	pr_debug("AUD_TOP_INT_RAW_STATUS0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_INT_RAW_STATUS0));
	pr_debug("AUD_TOP_INT_MISC_CON0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_INT_MISC_CON0));
	pr_debug("AUDNCP_CLKDIV_CON0 = 0x%x\n",
		 Ana_Get_Reg(AUDNCP_CLKDIV_CON0));
	pr_debug("AUDNCP_CLKDIV_CON1 = 0x%x\n",
		 Ana_Get_Reg(AUDNCP_CLKDIV_CON1));
	pr_debug("AUDNCP_CLKDIV_CON2 = 0x%x\n",
		 Ana_Get_Reg(AUDNCP_CLKDIV_CON2));
	pr_debug("AUDNCP_CLKDIV_CON3 = 0x%x\n",
		 Ana_Get_Reg(AUDNCP_CLKDIV_CON3));
	pr_debug("AUDNCP_CLKDIV_CON4 = 0x%x\n",
		 Ana_Get_Reg(AUDNCP_CLKDIV_CON4));
	pr_debug("AUD_TOP_MON_CON0 = 0x%x\n",
		 Ana_Get_Reg(AUD_TOP_MON_CON0));
	pr_debug("AUDIO_DIG_DSN_ID = 0x%x\n",
		 Ana_Get_Reg(AUDIO_DIG_DSN_ID));
	pr_debug("AUDIO_DIG_DSN_REV0 = 0x%x\n",
		 Ana_Get_Reg(AUDIO_DIG_DSN_REV0));
	pr_debug("AUDIO_DIG_DSN_DBI = 0x%x\n",
		 Ana_Get_Reg(AUDIO_DIG_DSN_DBI));
	pr_debug("AUDIO_DIG_DSN_DXI = 0x%x\n",
		 Ana_Get_Reg(AUDIO_DIG_DSN_DXI));
	pr_debug("AFE_UL_DL_CON0 = 0x%x\n",
		 Ana_Get_Reg(AFE_UL_DL_CON0));
	pr_debug("AFE_DL_SRC2_CON0_L = 0x%x\n",
		 Ana_Get_Reg(AFE_DL_SRC2_CON0_L));
	pr_debug("AFE_UL_SRC_CON0_H = 0x%x\n",
		 Ana_Get_Reg(AFE_UL_SRC_CON0_H));
	pr_debug("AFE_UL_SRC_CON0_L = 0x%x\n",
		 Ana_Get_Reg(AFE_UL_SRC_CON0_L));
	pr_debug("AFE_TOP_CON0 = 0x%x\n",
		 Ana_Get_Reg(PMIC_AFE_TOP_CON0));
	pr_debug("AUDIO_TOP_CON0 = 0x%x\n",
		 Ana_Get_Reg(PMIC_AUDIO_TOP_CON0));
	pr_debug("AFE_MON_DEBUG0 = 0x%x\n",
		 Ana_Get_Reg(AFE_MON_DEBUG0));
	pr_debug("AFUNC_AUD_CON0 = 0x%x\n",
		 Ana_Get_Reg(AFUNC_AUD_CON0));
	pr_debug("AFUNC_AUD_CON1 = 0x%x\n",
		 Ana_Get_Reg(AFUNC_AUD_CON1));
	pr_debug("AFUNC_AUD_CON2 = 0x%x\n",
		 Ana_Get_Reg(AFUNC_AUD_CON2));
	pr_debug("AFUNC_AUD_CON3 = 0x%x\n",
		 Ana_Get_Reg(AFUNC_AUD_CON3));
	pr_debug("AFUNC_AUD_CON4 = 0x%x\n",
		 Ana_Get_Reg(AFUNC_AUD_CON4));
	pr_debug("AFUNC_AUD_CON5 = 0x%x\n",
		 Ana_Get_Reg(AFUNC_AUD_CON5));
	pr_debug("AFUNC_AUD_CON6 = 0x%x\n",
		 Ana_Get_Reg(AFUNC_AUD_CON6));
	pr_debug("AFUNC_AUD_MON0 = 0x%x\n",
		 Ana_Get_Reg(AFUNC_AUD_MON0));
	pr_debug("AUDRC_TUNE_MON0 = 0x%x\n",
		 Ana_Get_Reg(AUDRC_TUNE_MON0));
	pr_debug("AFE_ADDA_MTKAIF_FIFO_CFG0 = 0x%x\n",
		 Ana_Get_Reg(AFE_ADDA_MTKAIF_FIFO_CFG0));
	pr_debug("AFE_ADDA_MTKAIF_FIFO_LOG_MON1 = 0x%x\n",
		 Ana_Get_Reg(AFE_ADDA_MTKAIF_FIFO_LOG_MON1));
	pr_debug("AFE_ADDA_MTKAIF_MON0 = 0x%x\n",
		 Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON0));
	pr_debug("AFE_ADDA_MTKAIF_MON1 = 0x%x\n",
		 Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON1));
	pr_debug("AFE_ADDA_MTKAIF_MON2 = 0x%x\n",
		 Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON2));
	pr_debug("AFE_ADDA_MTKAIF_MON3 = 0x%x\n",
		 Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON3));
	pr_debug("AFE_ADDA_MTKAIF_CFG0 = 0x%x\n",
		 Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_CFG0));
	pr_debug("AFE_ADDA_MTKAIF_RX_CFG0 = 0x%x\n",
		 Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG0));
	pr_debug("AFE_ADDA_MTKAIF_RX_CFG1 = 0x%x\n",
		 Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG1));
	pr_debug("AFE_ADDA_MTKAIF_RX_CFG2 = 0x%x\n",
		 Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG2));
	pr_debug("AFE_ADDA_MTKAIF_RX_CFG3 = 0x%x\n",
		 Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG3));
	pr_debug("AFE_ADDA_MTKAIF_TX_CFG1 = 0x%x\n",
		 Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_TX_CFG1));
	pr_debug("AFE_SGEN_CFG0 = 0x%x\n",
		 Ana_Get_Reg(AFE_SGEN_CFG0));
	pr_debug("AFE_SGEN_CFG1 = 0x%x\n",
		 Ana_Get_Reg(AFE_SGEN_CFG1));
	pr_debug("AFE_ADC_ASYNC_FIFO_CFG = 0x%x\n",
		 Ana_Get_Reg(AFE_ADC_ASYNC_FIFO_CFG));
	pr_debug("AFE_DCCLK_CFG0 = 0x%x\n",
		 Ana_Get_Reg(AFE_DCCLK_CFG0));
	pr_debug("AFE_DCCLK_CFG1 = 0x%x\n",
		 Ana_Get_Reg(AFE_DCCLK_CFG1));
	pr_debug("AUDIO_DIG_CFG = 0x%x\n",
		 Ana_Get_Reg(AUDIO_DIG_CFG));
	pr_debug("AFE_AUD_PAD_TOP = 0x%x\n",
		 Ana_Get_Reg(AFE_AUD_PAD_TOP));
	pr_debug("AFE_AUD_PAD_TOP_MON = 0x%x\n",
		 Ana_Get_Reg(AFE_AUD_PAD_TOP_MON));
	pr_debug("AFE_AUD_PAD_TOP_MON1 = 0x%x\n",
		 Ana_Get_Reg(AFE_AUD_PAD_TOP_MON1));
	pr_debug("AFE_DL_NLE_CFG = 0x%x\n",
		 Ana_Get_Reg(AFE_DL_NLE_CFG));
	pr_debug("AFE_DL_NLE_MON = 0x%x\n",
		 Ana_Get_Reg(AFE_DL_NLE_MON));
	pr_debug("AFE_CG_EN_MON = 0x%x\n",
		 Ana_Get_Reg(AFE_CG_EN_MON));
	pr_debug("AUDIO_DIG_2ND_DSN_ID = 0x%x\n",
		 Ana_Get_Reg(AUDIO_DIG_2ND_DSN_ID));
	pr_debug("AUDIO_DIG_2ND_DSN_REV0 = 0x%x\n",
		 Ana_Get_Reg(AUDIO_DIG_2ND_DSN_REV0));
	pr_debug("AUDIO_DIG_2ND_DSN_DBI = 0x%x\n",
		 Ana_Get_Reg(AUDIO_DIG_2ND_DSN_DBI));
	pr_debug("AUDIO_DIG_2ND_DSN_DXI = 0x%x\n",
		 Ana_Get_Reg(AUDIO_DIG_2ND_DSN_DXI));
	pr_debug("AFE_PMIC_NEWIF_CFG3 = 0x%x\n",
		 Ana_Get_Reg(AFE_PMIC_NEWIF_CFG3));
	pr_debug("AFE_VOW_TOP = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_TOP));
	pr_debug("AFE_VOW_CFG0 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_CFG0));
	pr_debug("AFE_VOW_CFG1 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_CFG1));
	pr_debug("AFE_VOW_CFG2 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_CFG2));
	pr_debug("AFE_VOW_CFG3 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_CFG3));
	pr_debug("AFE_VOW_CFG4 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_CFG4));
	pr_debug("AFE_VOW_CFG5 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_CFG5));
	pr_debug("AFE_VOW_CFG6 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_CFG6));
	pr_debug("AFE_VOW_MON0 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_MON0));
	pr_debug("AFE_VOW_MON1 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_MON1));
	pr_debug("AFE_VOW_MON2 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_MON2));
	pr_debug("AFE_VOW_MON3 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_MON3));
	pr_debug("AFE_VOW_MON4 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_MON4));
	pr_debug("AFE_VOW_MON5 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_MON5));
	pr_debug("AFE_VOW_SN_INI_CFG = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_SN_INI_CFG));
	pr_debug("AFE_VOW_TGEN_CFG0 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_TGEN_CFG0));
	pr_debug("AFE_VOW_POSDIV_CFG0 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_POSDIV_CFG0));
	pr_debug("AFE_VOW_HPF_CFG0 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_HPF_CFG0));
	pr_debug("AFE_VOW_PERIODIC_CFG0 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG0));
	pr_debug("AFE_VOW_PERIODIC_CFG1 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG1));
	pr_debug("AFE_VOW_PERIODIC_CFG2 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG2));
	pr_debug("AFE_VOW_PERIODIC_CFG3 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG3));
	pr_debug("AFE_VOW_PERIODIC_CFG4 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG4));
	pr_debug("AFE_VOW_PERIODIC_CFG5 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG5));
	pr_debug("AFE_VOW_PERIODIC_CFG6 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG6));
	pr_debug("AFE_VOW_PERIODIC_CFG7 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG7));
	pr_debug("AFE_VOW_PERIODIC_CFG8 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG8));
	pr_debug("AFE_VOW_PERIODIC_CFG9 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG9));
	pr_debug("AFE_VOW_PERIODIC_CFG10 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG10));
	pr_debug("AFE_VOW_PERIODIC_CFG11 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG11));
	pr_debug("AFE_VOW_PERIODIC_CFG12 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG12));
	pr_debug("AFE_VOW_PERIODIC_CFG13 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG13));
	pr_debug("AFE_VOW_PERIODIC_CFG14 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG14));
	pr_debug("AFE_VOW_PERIODIC_CFG15 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG15));
	pr_debug("AFE_VOW_PERIODIC_CFG16 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG16));
	pr_debug("AFE_VOW_PERIODIC_CFG17 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG17));
	pr_debug("AFE_VOW_PERIODIC_CFG18 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG18));
	pr_debug("AFE_VOW_PERIODIC_CFG19 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG19));
	pr_debug("AFE_VOW_PERIODIC_CFG20 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG20));
	pr_debug("AFE_VOW_PERIODIC_CFG21 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG21));
	pr_debug("AFE_VOW_PERIODIC_CFG22 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG22));
	pr_debug("AFE_VOW_PERIODIC_CFG23 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_CFG23));
	pr_debug("AFE_VOW_PERIODIC_MON0 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_MON0));
	pr_debug("AFE_VOW_PERIODIC_MON1 = 0x%x\n",
		 Ana_Get_Reg(AFE_VOW_PERIODIC_MON1));
	pr_debug("AUDENC_DSN_ID = 0x%x\n",
		 Ana_Get_Reg(AUDENC_DSN_ID));
	pr_debug("AUDENC_DSN_REV0 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_DSN_REV0));
	pr_debug("AUDENC_DSN_DBI = 0x%x\n",
		 Ana_Get_Reg(AUDENC_DSN_DBI));
	pr_debug("AUDENC_DSN_FPI = 0x%x\n",
		 Ana_Get_Reg(AUDENC_DSN_FPI));
	pr_debug("AUDENC_ANA_CON0 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_ANA_CON0));
	pr_debug("AUDENC_ANA_CON1 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_ANA_CON1));
	pr_debug("AUDENC_ANA_CON2 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_ANA_CON2));
	pr_debug("AUDENC_ANA_CON3 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_ANA_CON3));
	pr_debug("AUDENC_ANA_CON4 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_ANA_CON4));
	pr_debug("AUDENC_ANA_CON5 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_ANA_CON5));
	pr_debug("AUDENC_ANA_CON6 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_ANA_CON6));
	pr_debug("AUDENC_ANA_CON7 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_ANA_CON7));
	pr_debug("AUDENC_ANA_CON8 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_ANA_CON8));
	pr_debug("AUDENC_ANA_CON9 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_ANA_CON9));
	pr_debug("AUDENC_ANA_CON10 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_ANA_CON10));
	pr_debug("AUDENC_ANA_CON11 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_ANA_CON11));
	pr_debug("AUDENC_ANA_CON12 = 0x%x\n",
		 Ana_Get_Reg(AUDENC_ANA_CON12));
	pr_debug("AUDDEC_DSN_ID = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_DSN_ID));
	pr_debug("AUDDEC_DSN_REV0 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_DSN_REV0));
	pr_debug("AUDDEC_DSN_DBI = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_DSN_DBI));
	pr_debug("AUDDEC_DSN_FPI = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_DSN_FPI));
	pr_debug("AUDDEC_ANA_CON0 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON0));
	pr_debug("AUDDEC_ANA_CON1 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON1));
	pr_debug("AUDDEC_ANA_CON2 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON2));
	pr_debug("AUDDEC_ANA_CON3 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON3));
	pr_debug("AUDDEC_ANA_CON4 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON4));
	pr_debug("AUDDEC_ANA_CON5 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON5));
	pr_debug("AUDDEC_ANA_CON6 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON6));
	pr_debug("AUDDEC_ANA_CON7 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON7));
	pr_debug("AUDDEC_ANA_CON8 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON8));
	pr_debug("AUDDEC_ANA_CON9 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON9));
	pr_debug("AUDDEC_ANA_CON10 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON10));
	pr_debug("AUDDEC_ANA_CON11 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON11));
	pr_debug("AUDDEC_ANA_CON12 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON12));
	pr_debug("AUDDEC_ANA_CON13 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON13));
	pr_debug("AUDDEC_ANA_CON14 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON14));
	pr_debug("AUDDEC_ANA_CON15 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ANA_CON15));
	pr_debug("AUDDEC_ELR_NUM = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ELR_NUM));
	pr_debug("AUDDEC_ELR_0 = 0x%x\n",
		 Ana_Get_Reg(AUDDEC_ELR_0));
	pr_debug("AUDZCD_DSN_ID = 0x%x\n",
		 Ana_Get_Reg(AUDZCD_DSN_ID));
	pr_debug("AUDZCD_DSN_REV0 = 0x%x\n",
		 Ana_Get_Reg(AUDZCD_DSN_REV0));
	pr_debug("AUDZCD_DSN_DBI = 0x%x\n",
		 Ana_Get_Reg(AUDZCD_DSN_DBI));
	pr_debug("AUDZCD_DSN_FPI = 0x%x\n",
		 Ana_Get_Reg(AUDZCD_DSN_FPI));
	pr_debug("ZCD_CON0 = 0x%x\n",
		 Ana_Get_Reg(ZCD_CON0));
	pr_debug("ZCD_CON1 = 0x%x\n",
		 Ana_Get_Reg(ZCD_CON1));
	pr_debug("ZCD_CON2 = 0x%x\n",
		 Ana_Get_Reg(ZCD_CON2));
	pr_debug("ZCD_CON3 = 0x%x\n",
		 Ana_Get_Reg(ZCD_CON3));
	pr_debug("ZCD_CON4 = 0x%x\n",
		 Ana_Get_Reg(ZCD_CON4));
	pr_debug("ZCD_CON5 = 0x%x\n",
		 Ana_Get_Reg(ZCD_CON5));
	pr_debug("ACCDET_DSN_DIG_ID = 0x%x\n",
		 Ana_Get_Reg(ACCDET_DSN_DIG_ID));
	pr_debug("ACCDET_DSN_DIG_REV0 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_DSN_DIG_REV0));
	pr_debug("ACCDET_DSN_DBI = 0x%x\n",
		 Ana_Get_Reg(ACCDET_DSN_DBI));
	pr_debug("ACCDET_DSN_FPI = 0x%x\n",
		 Ana_Get_Reg(ACCDET_DSN_FPI));
	pr_debug("ACCDET_CON0 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON0));
	pr_debug("ACCDET_CON1 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON1));
	pr_debug("ACCDET_CON2 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON2));
	pr_debug("ACCDET_CON3 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON3));
	pr_debug("ACCDET_CON4 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON4));
	pr_debug("ACCDET_CON5 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON5));
	pr_debug("ACCDET_CON6 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON6));
	pr_debug("ACCDET_CON7 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON7));
	pr_debug("ACCDET_CON8 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON8));
	pr_debug("ACCDET_CON9 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON9));
	pr_debug("ACCDET_CON10 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON10));
	pr_debug("ACCDET_CON11 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON11));
	pr_debug("ACCDET_CON12 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON12));
	pr_debug("ACCDET_CON13 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON13));
	pr_debug("ACCDET_CON14 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON14));
	pr_debug("ACCDET_CON15 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON15));
	pr_debug("ACCDET_CON16 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON16));
	pr_debug("ACCDET_CON17 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON17));
	pr_debug("ACCDET_CON18 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON18));
	pr_debug("ACCDET_CON19 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON19));
	pr_debug("ACCDET_CON20 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON20));
	pr_debug("ACCDET_CON21 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON21));
	pr_debug("ACCDET_CON22 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON22));
	pr_debug("ACCDET_CON23 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON23));
	pr_debug("ACCDET_CON24 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON24));
	pr_debug("ACCDET_CON25 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON25));
	pr_debug("ACCDET_CON26 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON26));
	pr_debug("ACCDET_CON27 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON27));
	pr_debug("ACCDET_CON28 = 0x%x\n",
		 Ana_Get_Reg(ACCDET_CON28));

	pr_debug("GPIO_MODE2 = 0x%x\n", Ana_Get_Reg(GPIO_MODE2));
	pr_debug("GPIO_MODE3 = 0x%x\n", Ana_Get_Reg(GPIO_MODE3));
	pr_debug("GPIO_DIR0 = 0x%x\n", Ana_Get_Reg(GPIO_DIR0));
	pr_debug("DRV_CON3 = 0x%x\n", Ana_Get_Reg(DRV_CON3));

	audckbufEnable(false);
}
EXPORT_SYMBOL(Ana_Log_Print);

int Ana_Debug_Read(char *buffer, const int size)
{
	int n = 0;

	n += scnprintf(buffer + n, size - n, "AUD_TOP_ID = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_ID));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_REV0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_REV0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_DBI = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_DBI));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_DXI = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_DXI));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKPDN_TPM0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKPDN_TPM0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKPDN_TPM1 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKPDN_TPM1));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKPDN_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKPDN_CON0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKPDN_CON0_SET = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKPDN_CON0_SET));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKPDN_CON0_CLR = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKPDN_CON0_CLR));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKSEL_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKSEL_CON0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKSEL_CON0_SET = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKSEL_CON0_SET));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKSEL_CON0_CLR = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKSEL_CON0_CLR));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKTST_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKTST_CON0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CLK_HWEN_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CLK_HWEN_CON0));
	n += scnprintf(buffer + n, size - n,
		       "AUD_TOP_CLK_HWEN_CON0_SET = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CLK_HWEN_CON0_SET));
	n += scnprintf(buffer + n, size - n,
		       "AUD_TOP_CLK_HWEN_CON0_CLR = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CLK_HWEN_CON0_CLR));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_RST_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_RST_CON0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_RST_CON0_SET = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_RST_CON0_SET));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_RST_CON0_CLR = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_RST_CON0_CLR));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_RST_BANK_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_RST_BANK_CON0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_INT_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_INT_CON0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_INT_CON0_SET = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_INT_CON0_SET));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_INT_CON0_CLR = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_INT_CON0_CLR));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_INT_MASK_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_INT_MASK_CON0));
	n += scnprintf(buffer + n, size - n,
		       "AUD_TOP_INT_MASK_CON0_SET = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_INT_MASK_CON0_SET));
	n += scnprintf(buffer + n, size - n,
		       "AUD_TOP_INT_MASK_CON0_CLR = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_INT_MASK_CON0_CLR));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_INT_STATUS0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_INT_STATUS0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_INT_RAW_STATUS0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_INT_RAW_STATUS0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_INT_MISC_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_INT_MISC_CON0));
	n += scnprintf(buffer + n, size - n, "AUDNCP_CLKDIV_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUDNCP_CLKDIV_CON0));
	n += scnprintf(buffer + n, size - n, "AUDNCP_CLKDIV_CON1 = 0x%x\n",
		       Ana_Get_Reg(AUDNCP_CLKDIV_CON1));
	n += scnprintf(buffer + n, size - n, "AUDNCP_CLKDIV_CON2 = 0x%x\n",
		       Ana_Get_Reg(AUDNCP_CLKDIV_CON2));
	n += scnprintf(buffer + n, size - n, "AUDNCP_CLKDIV_CON3 = 0x%x\n",
		       Ana_Get_Reg(AUDNCP_CLKDIV_CON3));
	n += scnprintf(buffer + n, size - n, "AUDNCP_CLKDIV_CON4 = 0x%x\n",
		       Ana_Get_Reg(AUDNCP_CLKDIV_CON4));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_MON_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_MON_CON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_DIG_DSN_ID = 0x%x\n",
		       Ana_Get_Reg(AUDIO_DIG_DSN_ID));
	n += scnprintf(buffer + n, size - n, "AUDIO_DIG_DSN_REV0 = 0x%x\n",
		       Ana_Get_Reg(AUDIO_DIG_DSN_REV0));
	n += scnprintf(buffer + n, size - n, "AUDIO_DIG_DSN_DBI = 0x%x\n",
		       Ana_Get_Reg(AUDIO_DIG_DSN_DBI));
	n += scnprintf(buffer + n, size - n, "AUDIO_DIG_DSN_DXI = 0x%x\n",
		       Ana_Get_Reg(AUDIO_DIG_DSN_DXI));
	n += scnprintf(buffer + n, size - n, "AFE_UL_DL_CON0 = 0x%x\n",
		       Ana_Get_Reg(AFE_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_DL_SRC2_CON0_L = 0x%x\n",
		       Ana_Get_Reg(AFE_DL_SRC2_CON0_L));
	n += scnprintf(buffer + n, size - n, "AFE_UL_SRC_CON0_H = 0x%x\n",
		       Ana_Get_Reg(AFE_UL_SRC_CON0_H));
	n += scnprintf(buffer + n, size - n, "AFE_UL_SRC_CON0_L = 0x%x\n",
		       Ana_Get_Reg(AFE_UL_SRC_CON0_L));
	n += scnprintf(buffer + n, size - n, "AFE_TOP_CON0 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AUDIO_TOP_CON0 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AUDIO_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_MON_DEBUG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_MON_DEBUG0));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON0 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_CON0));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON1 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_CON1));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON2 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_CON2));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON3 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_CON3));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON4 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_CON4));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON5 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_CON5));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_CON6 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_CON6));
	n += scnprintf(buffer + n, size - n, "AFUNC_AUD_MON0 = 0x%x\n",
		       Ana_Get_Reg(AFUNC_AUD_MON0));
	n += scnprintf(buffer + n, size - n, "AUDRC_TUNE_MON0 = 0x%x\n",
		       Ana_Get_Reg(AUDRC_TUNE_MON0));
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_MTKAIF_FIFO_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_ADDA_MTKAIF_FIFO_CFG0));
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADDA_MTKAIF_FIFO_LOG_MON1 = 0x%x\n",
		       Ana_Get_Reg(AFE_ADDA_MTKAIF_FIFO_LOG_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_MON0 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_MON1 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_MON2 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_MON3 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON3));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_CFG0 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_RX_CFG0 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_RX_CFG1 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_RX_CFG2 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG2));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_RX_CFG3 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG3));
	n += scnprintf(buffer + n, size - n, "AFE_ADDA_MTKAIF_TX_CFG1 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_TX_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_SGEN_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_SGEN_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_SGEN_CFG1 = 0x%x\n",
		       Ana_Get_Reg(AFE_SGEN_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_ADC_ASYNC_FIFO_CFG = 0x%x\n",
		       Ana_Get_Reg(AFE_ADC_ASYNC_FIFO_CFG));
	n += scnprintf(buffer + n, size - n, "AFE_DCCLK_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_DCCLK_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_DCCLK_CFG1 = 0x%x\n",
		       Ana_Get_Reg(AFE_DCCLK_CFG1));
	n += scnprintf(buffer + n, size - n, "AUDIO_DIG_CFG = 0x%x\n",
		       Ana_Get_Reg(AUDIO_DIG_CFG));
	n += scnprintf(buffer + n, size - n, "AFE_AUD_PAD_TOP = 0x%x\n",
		       Ana_Get_Reg(AFE_AUD_PAD_TOP));
	n += scnprintf(buffer + n, size - n, "AFE_AUD_PAD_TOP_MON = 0x%x\n",
		       Ana_Get_Reg(AFE_AUD_PAD_TOP_MON));
	n += scnprintf(buffer + n, size - n, "AFE_AUD_PAD_TOP_MON1 = 0x%x\n",
		       Ana_Get_Reg(AFE_AUD_PAD_TOP_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_DL_NLE_CFG = 0x%x\n",
		       Ana_Get_Reg(AFE_DL_NLE_CFG));
	n += scnprintf(buffer + n, size - n, "AFE_DL_NLE_MON = 0x%x\n",
		       Ana_Get_Reg(AFE_DL_NLE_MON));
	n += scnprintf(buffer + n, size - n, "AFE_CG_EN_MON = 0x%x\n",
		       Ana_Get_Reg(AFE_CG_EN_MON));
	n += scnprintf(buffer + n, size - n, "AUDIO_DIG_2ND_DSN_ID = 0x%x\n",
		       Ana_Get_Reg(AUDIO_DIG_2ND_DSN_ID));
	n += scnprintf(buffer + n, size - n, "AUDIO_DIG_2ND_DSN_REV0 = 0x%x\n",
		       Ana_Get_Reg(AUDIO_DIG_2ND_DSN_REV0));
	n += scnprintf(buffer + n, size - n, "AUDIO_DIG_2ND_DSN_DBI = 0x%x\n",
		       Ana_Get_Reg(AUDIO_DIG_2ND_DSN_DBI));
	n += scnprintf(buffer + n, size - n, "AUDIO_DIG_2ND_DSN_DXI = 0x%x\n",
		       Ana_Get_Reg(AUDIO_DIG_2ND_DSN_DXI));
	n += scnprintf(buffer + n, size - n, "AFE_PMIC_NEWIF_CFG3 = 0x%x\n",
		       Ana_Get_Reg(AFE_PMIC_NEWIF_CFG3));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_TOP = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_TOP));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_CFG1 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_CFG2 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_CFG2));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_CFG3 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_CFG3));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_CFG4 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_CFG4));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_CFG5 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_CFG5));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_CFG6 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_CFG6));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_MON0 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_MON1 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_MON1));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_MON2 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_MON2));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_MON3 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_MON3));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_MON4 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_MON4));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_MON5 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_MON5));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_SN_INI_CFG = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_SN_INI_CFG));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_TGEN_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_TGEN_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_POSDIV_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_POSDIV_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_HPF_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_HPF_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG1 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG2 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG2));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG3 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG3));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG4 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG4));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG5 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG5));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG6 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG6));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG7 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG7));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG8 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG8));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG9 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG9));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG10 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG10));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG11 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG11));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG12 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG12));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG13 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG13));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG14 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG14));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG15 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG15));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG16 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG16));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG17 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG17));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG18 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG18));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG19 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG19));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG20 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG20));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG21 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG21));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG22 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG22));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_CFG23 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_CFG23));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_MON0 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_MON0));
	n += scnprintf(buffer + n, size - n, "AFE_VOW_PERIODIC_MON1 = 0x%x\n",
		       Ana_Get_Reg(AFE_VOW_PERIODIC_MON1));
	n += scnprintf(buffer + n, size - n, "AUDENC_DSN_ID = 0x%x\n",
		       Ana_Get_Reg(AUDENC_DSN_ID));
	n += scnprintf(buffer + n, size - n, "AUDENC_DSN_REV0 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_DSN_REV0));
	n += scnprintf(buffer + n, size - n, "AUDENC_DSN_DBI = 0x%x\n",
		       Ana_Get_Reg(AUDENC_DSN_DBI));
	n += scnprintf(buffer + n, size - n, "AUDENC_DSN_FPI = 0x%x\n",
		       Ana_Get_Reg(AUDENC_DSN_FPI));
	n += scnprintf(buffer + n, size - n, "AUDENC_ANA_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_ANA_CON0));
	n += scnprintf(buffer + n, size - n, "AUDENC_ANA_CON1 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_ANA_CON1));
	n += scnprintf(buffer + n, size - n, "AUDENC_ANA_CON2 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_ANA_CON2));
	n += scnprintf(buffer + n, size - n, "AUDENC_ANA_CON3 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_ANA_CON3));
	n += scnprintf(buffer + n, size - n, "AUDENC_ANA_CON4 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_ANA_CON4));
	n += scnprintf(buffer + n, size - n, "AUDENC_ANA_CON5 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_ANA_CON5));
	n += scnprintf(buffer + n, size - n, "AUDENC_ANA_CON6 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_ANA_CON6));
	n += scnprintf(buffer + n, size - n, "AUDENC_ANA_CON7 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_ANA_CON7));
	n += scnprintf(buffer + n, size - n, "AUDENC_ANA_CON8 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_ANA_CON8));
	n += scnprintf(buffer + n, size - n, "AUDENC_ANA_CON9 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_ANA_CON9));
	n += scnprintf(buffer + n, size - n, "AUDENC_ANA_CON10 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_ANA_CON10));
	n += scnprintf(buffer + n, size - n, "AUDENC_ANA_CON11 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_ANA_CON11));
	n += scnprintf(buffer + n, size - n, "AUDENC_ANA_CON12 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_ANA_CON12));
	n += scnprintf(buffer + n, size - n, "AUDDEC_DSN_ID = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_DSN_ID));
	n += scnprintf(buffer + n, size - n, "AUDDEC_DSN_REV0 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_DSN_REV0));
	n += scnprintf(buffer + n, size - n, "AUDDEC_DSN_DBI = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_DSN_DBI));
	n += scnprintf(buffer + n, size - n, "AUDDEC_DSN_FPI = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_DSN_FPI));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON0));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON1 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON1));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON2 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON2));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON3 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON3));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON4 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON4));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON5 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON5));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON6 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON6));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON7 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON7));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON8 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON8));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON9 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON9));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON10 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON10));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON11 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON11));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON12 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON12));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON13 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON13));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON14 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON14));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ANA_CON15 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ANA_CON15));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ELR_NUM = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ELR_NUM));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ELR_0 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ELR_0));
	n += scnprintf(buffer + n, size - n, "AUDZCD_DSN_ID = 0x%x\n",
		       Ana_Get_Reg(AUDZCD_DSN_ID));
	n += scnprintf(buffer + n, size - n, "AUDZCD_DSN_REV0 = 0x%x\n",
		       Ana_Get_Reg(AUDZCD_DSN_REV0));
	n += scnprintf(buffer + n, size - n, "AUDZCD_DSN_DBI = 0x%x\n",
		       Ana_Get_Reg(AUDZCD_DSN_DBI));
	n += scnprintf(buffer + n, size - n, "AUDZCD_DSN_FPI = 0x%x\n",
		       Ana_Get_Reg(AUDZCD_DSN_FPI));
	n += scnprintf(buffer + n, size - n, "ZCD_CON0 = 0x%x\n",
		       Ana_Get_Reg(ZCD_CON0));
	n += scnprintf(buffer + n, size - n, "ZCD_CON1 = 0x%x\n",
		       Ana_Get_Reg(ZCD_CON1));
	n += scnprintf(buffer + n, size - n, "ZCD_CON2 = 0x%x\n",
		       Ana_Get_Reg(ZCD_CON2));
	n += scnprintf(buffer + n, size - n, "ZCD_CON3 = 0x%x\n",
		       Ana_Get_Reg(ZCD_CON3));
	n += scnprintf(buffer + n, size - n, "ZCD_CON4 = 0x%x\n",
		       Ana_Get_Reg(ZCD_CON4));
	n += scnprintf(buffer + n, size - n, "ZCD_CON5 = 0x%x\n",
		       Ana_Get_Reg(ZCD_CON5));
	n += scnprintf(buffer + n, size - n, "ACCDET_DSN_DIG_ID = 0x%x\n",
		       Ana_Get_Reg(ACCDET_DSN_DIG_ID));
	n += scnprintf(buffer + n, size - n, "ACCDET_DSN_DIG_REV0 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_DSN_DIG_REV0));
	n += scnprintf(buffer + n, size - n, "ACCDET_DSN_DBI = 0x%x\n",
		       Ana_Get_Reg(ACCDET_DSN_DBI));
	n += scnprintf(buffer + n, size - n, "ACCDET_DSN_FPI = 0x%x\n",
		       Ana_Get_Reg(ACCDET_DSN_FPI));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON0 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON0));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON1 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON1));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON2 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON2));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON3 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON3));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON4 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON4));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON5 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON5));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON6 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON6));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON7 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON7));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON8 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON8));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON9 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON9));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON10 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON10));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON11 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON11));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON12 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON12));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON13 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON13));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON14 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON14));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON15 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON15));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON16 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON16));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON17 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON17));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON18 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON18));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON19 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON19));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON20 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON20));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON21 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON21));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON22 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON22));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON23 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON23));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON24 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON24));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON25 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON25));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON26 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON26));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON27 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON27));
	n += scnprintf(buffer + n, size - n, "ACCDET_CON28 = 0x%x\n",
		       Ana_Get_Reg(ACCDET_CON28));

	n += scnprintf(buffer + n, size - n, "GPIO_MODE2  = 0x%x\n",
		       Ana_Get_Reg(GPIO_MODE2));
	n += scnprintf(buffer + n, size - n, "GPIO_MODE3  = 0x%x\n",
		       Ana_Get_Reg(GPIO_MODE3));
	n += scnprintf(buffer + n, size - n, "GPIO_DIR0  = 0x%x\n",
		       Ana_Get_Reg(GPIO_DIR0));
	n += scnprintf(buffer + n, size - n, "DRV_CON3  = 0x%x\n",
		       Ana_Get_Reg(DRV_CON3));
	return n;
}
EXPORT_SYMBOL(Ana_Debug_Read);

/* export symbols for other module using */
