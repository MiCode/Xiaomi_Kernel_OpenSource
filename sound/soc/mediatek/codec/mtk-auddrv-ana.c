// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/******************************************************************************
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
 *-----------------------------------------------------------------------------
 *
 *
 *****************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/
#include "mtk-auddrv-ana.h"
#include "mtk-soc-codec-63xx.h"

#include <linux/regmap.h>
#include <linux/module.h>
/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/
/* don't use this directly if not necessary */
static struct mt6357_priv *local_priv;

int mt63xx_set_local_priv(struct mt6357_priv *priv)
{
	local_priv = priv;
	return 0;
}
EXPORT_SYMBOL(mt63xx_set_local_priv);

unsigned int Ana_Get_Reg(unsigned int addr)
{
	unsigned int val = 0;

	regmap_read(local_priv->regmap, addr, &val);
	return val;
}
EXPORT_SYMBOL(Ana_Get_Reg);

void Ana_Set_Reg(unsigned int addr, unsigned int value, unsigned int mask)
{
	regmap_update_bits(local_priv->regmap, addr, mask, value);
}
EXPORT_SYMBOL(Ana_Set_Reg);

void Ana_Log_Print(void)
{
	audckbufEnable(true);
	pr_debug("AUD_TOP_ID = 0x%x\n",
		Ana_Get_Reg(AUD_TOP_ID));
	pr_debug("AUD_TOP_REV0 = 0x%x\n",
		Ana_Get_Reg(AUD_TOP_REV0));
	pr_debug("AUD_TOP_REV1 = 0x%x\n",
		Ana_Get_Reg(AUD_TOP_REV1));
	pr_debug("AUD_TOP_CKPDN_PM0 = 0x%x\n",
		Ana_Get_Reg(AUD_TOP_CKPDN_PM0));
	pr_debug("AUD_TOP_CKPDN_PM1 = 0x%x\n",
		Ana_Get_Reg(AUD_TOP_CKPDN_PM1));
	pr_debug("AUD_TOP_CKPDN_CON0 = 0x%x\n",
		Ana_Get_Reg(AUD_TOP_CKPDN_CON0));
	pr_debug("AUD_TOP_CKSEL_CON0 = 0x%x\n",
		Ana_Get_Reg(AUD_TOP_CKSEL_CON0));
	pr_debug("AUD_TOP_CKTST_CON0 = 0x%x\n",
		Ana_Get_Reg(AUD_TOP_CKTST_CON0));
	pr_debug("AUD_TOP_RST_CON0 = 0x%x\n",
		Ana_Get_Reg(AUD_TOP_RST_CON0));
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
	pr_debug("AUDIO_DIG_ID = 0x%x\n",
		Ana_Get_Reg(AUDIO_DIG_ID));
	pr_debug("AUDIO_DIG_REV0 = 0x%x\n",
		Ana_Get_Reg(AUDIO_DIG_REV0));
	pr_debug("AUDIO_DIG_REV1 = 0x%x\n",
		Ana_Get_Reg(AUDIO_DIG_REV1));
	pr_debug("AFE_UL_DL_CON0 = 0x%x\n",
		Ana_Get_Reg(AFE_UL_DL_CON0));
	pr_debug("AFE_DL_SRC2_CON0_L = 0x%x\n",
		Ana_Get_Reg(AFE_DL_SRC2_CON0_L));
	pr_debug("AFE_UL_SRC_CON0_H = 0x%x\n",
		Ana_Get_Reg(AFE_UL_SRC_CON0_H));
	pr_debug("AFE_UL_SRC_CON0_L = 0x%x\n",
		Ana_Get_Reg(AFE_UL_SRC_CON0_L));
	pr_debug("PMIC_AFE_TOP_CON0 = 0x%x\n",
		Ana_Get_Reg(PMIC_AFE_TOP_CON0));
	pr_debug("PMIC_AUDIO_TOP_CON0 = 0x%x\n",
		Ana_Get_Reg(PMIC_AUDIO_TOP_CON0));
	pr_debug("AFE_MON_DEBUG0 = 0x%x\n", Ana_Get_Reg(AFE_MON_DEBUG0));
	pr_debug("AFUNC_AUD_CON0 = 0x%x\n", Ana_Get_Reg(AFUNC_AUD_CON0));
	pr_debug("AFUNC_AUD_CON1 = 0x%x\n", Ana_Get_Reg(AFUNC_AUD_CON1));
	pr_debug("AFUNC_AUD_CON2 = 0x%x\n", Ana_Get_Reg(AFUNC_AUD_CON2));
	pr_debug("AFUNC_AUD_CON3 = 0x%x\n", Ana_Get_Reg(AFUNC_AUD_CON3));
	pr_debug("AFUNC_AUD_CON4 = 0x%x\n", Ana_Get_Reg(AFUNC_AUD_CON4));
	pr_debug("AFUNC_AUD_MON0 = 0x%x\n", Ana_Get_Reg(AFUNC_AUD_MON0));
	pr_debug("AUDRC_TUNE_MON0 = 0x%x\n", Ana_Get_Reg(AUDRC_TUNE_MON0));
	pr_debug("AFE_ADDA_MTKAIF_FIFO_CFG0 = 0x%x\n",
		Ana_Get_Reg(AFE_ADDA_MTKAIF_FIFO_CFG0));
	pr_debug("AFE_ADDA_MTKAIF_FIFO_LOG_MON1 = 0x%x\n",
		Ana_Get_Reg(AFE_ADDA_MTKAIF_FIFO_LOG_MON1));
	pr_debug("PMIC_AFE_ADDA_MTKAIF_MON0 = 0x%x\n",
		Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON0));
	pr_debug("PMIC_AFE_ADDA_MTKAIF_MON1 = 0x%x\n",
		Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON1));
	pr_debug("PMIC_AFE_ADDA_MTKAIF_MON2 = 0x%x\n",
		Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON2));
	pr_debug("PMIC_AFE_ADDA_MTKAIF_MON3 = 0x%x\n",
		Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON3));
	pr_debug("PMIC_AFE_ADDA_MTKAIF_CFG0 = 0x%x\n",
		Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_CFG0));
	pr_debug("PMIC_AFE_ADDA_MTKAIF_RX_CFG0 = 0x%x\n",
		Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG0));
	pr_debug("PMIC_AFE_ADDA_MTKAIF_RX_CFG1 = 0x%x\n",
		Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG1));
	pr_debug("PMIC_AFE_ADDA_MTKAIF_RX_CFG2 = 0x%x\n",
		Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG2));
	pr_debug("PMIC_AFE_ADDA_MTKAIF_RX_CFG3 = 0x%x\n",
		Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG3));
	pr_debug("PMIC_AFE_ADDA_MTKAIF_TX_CFG1 = 0x%x\n",
		Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_TX_CFG1));
	pr_debug("AFE_SGEN_CFG0 = 0x%x\n", Ana_Get_Reg(AFE_SGEN_CFG0));
	pr_debug("AFE_SGEN_CFG1 = 0x%x\n", Ana_Get_Reg(AFE_SGEN_CFG1));
	pr_debug("AFE_ADC_ASYNC_FIFO_CFG = 0x%x\n",
		Ana_Get_Reg(AFE_ADC_ASYNC_FIFO_CFG));
	pr_debug("AFE_DCCLK_CFG0 = 0x%x\n", Ana_Get_Reg(AFE_DCCLK_CFG0));
	pr_debug("AFE_DCCLK_CFG1 = 0x%x\n", Ana_Get_Reg(AFE_DCCLK_CFG1));
	pr_debug("AUDIO_DIG_CFG = 0x%x\n", Ana_Get_Reg(AUDIO_DIG_CFG));
	pr_debug("AFE_AUD_PAD_TOP = 0x%x\n", Ana_Get_Reg(AFE_AUD_PAD_TOP));
	pr_debug("AFE_AUD_PAD_TOP_MON = 0x%x\n",
		Ana_Get_Reg(AFE_AUD_PAD_TOP_MON));
	pr_debug("AFE_AUD_PAD_TOP_MON1 = 0x%x\n",
		Ana_Get_Reg(AFE_AUD_PAD_TOP_MON1));
	pr_debug("AUDENC_DSN_ID = 0x%x\n", Ana_Get_Reg(AUDENC_DSN_ID));
	pr_debug("AUDENC_DSN_REV0 = 0x%x\n", Ana_Get_Reg(AUDENC_DSN_REV0));
	pr_debug("AUDENC_DSN_REV1 = 0x%x\n", Ana_Get_Reg(AUDENC_DSN_REV1));
	pr_debug("AUDENC_ANA_CON0 = 0x%x\n", Ana_Get_Reg(AUDENC_ANA_CON0));
	pr_debug("AUDENC_ANA_CON1 = 0x%x\n", Ana_Get_Reg(AUDENC_ANA_CON1));
	pr_debug("AUDENC_ANA_CON2 = 0x%x\n", Ana_Get_Reg(AUDENC_ANA_CON2));
	pr_debug("AUDENC_ANA_CON3 = 0x%x\n", Ana_Get_Reg(AUDENC_ANA_CON3));
	pr_debug("AUDENC_ANA_CON4 = 0x%x\n", Ana_Get_Reg(AUDENC_ANA_CON4));
	pr_debug("AUDENC_ANA_CON5 = 0x%x\n", Ana_Get_Reg(AUDENC_ANA_CON5));
	pr_debug("AUDENC_ANA_CON6 = 0x%x\n", Ana_Get_Reg(AUDENC_ANA_CON6));
	pr_debug("AUDENC_ANA_CON7 = 0x%x\n", Ana_Get_Reg(AUDENC_ANA_CON7));
	pr_debug("AUDENC_ANA_CON8 = 0x%x\n", Ana_Get_Reg(AUDENC_ANA_CON8));
	pr_debug("AUDENC_ANA_CON9 = 0x%x\n", Ana_Get_Reg(AUDENC_ANA_CON9));
	pr_debug("AUDENC_ANA_CON10 = 0x%x\n", Ana_Get_Reg(AUDENC_ANA_CON10));
	pr_debug("AUDENC_ANA_CON11 = 0x%x\n", Ana_Get_Reg(AUDENC_ANA_CON11));
	pr_debug("AUDDEC_DSN_ID = 0x%x\n", Ana_Get_Reg(AUDDEC_DSN_ID));
	pr_debug("AUDDEC_DSN_REV0 = 0x%x\n", Ana_Get_Reg(AUDDEC_DSN_REV0));
	pr_debug("AUDDEC_DSN_REV1 = 0x%x\n", Ana_Get_Reg(AUDDEC_DSN_REV1));
	pr_debug("AUDDEC_ANA_CON0 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON0));
	pr_debug("AUDDEC_ANA_CON1 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON1));
	pr_debug("AUDDEC_ANA_CON2 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON2));
	pr_debug("AUDDEC_ANA_CON3 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON3));
	pr_debug("AUDDEC_ANA_CON4 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON4));
	pr_debug("AUDDEC_ANA_CON5 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON5));
	pr_debug("AUDDEC_ANA_CON6 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON6));
	pr_debug("AUDDEC_ANA_CON7 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON7));
	pr_debug("AUDDEC_ANA_CON8 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON8));
	pr_debug("AUDDEC_ANA_CON9 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON9));
	pr_debug("AUDDEC_ANA_CON10 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON10));
	pr_debug("AUDDEC_ANA_CON11 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON11));
	pr_debug("AUDDEC_ANA_CON12 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON12));
	pr_debug("AUDDEC_ANA_CON13 = 0x%x\n", Ana_Get_Reg(AUDDEC_ANA_CON13));
	pr_debug("AUDDEC_ELR_NUM = 0x%x\n", Ana_Get_Reg(AUDDEC_ELR_NUM));
	pr_debug("AUDDEC_ELR_0 = 0x%x\n", Ana_Get_Reg(AUDDEC_ELR_0));
	pr_debug("AUDZCDID = 0x%x\n", Ana_Get_Reg(AUDZCDID));
	pr_debug("AUDZCDREV0 = 0x%x\n", Ana_Get_Reg(AUDZCDREV0));
	pr_debug("AUDZCDREV1 = 0x%x\n", Ana_Get_Reg(AUDZCDREV1));
	pr_debug("ZCD_CON0 = 0x%x\n", Ana_Get_Reg(ZCD_CON0));
	pr_debug("ZCD_CON1 = 0x%x\n", Ana_Get_Reg(ZCD_CON1));
	pr_debug("ZCD_CON2 = 0x%x\n", Ana_Get_Reg(ZCD_CON2));
	pr_debug("ZCD_CON3 = 0x%x\n", Ana_Get_Reg(ZCD_CON3));
	pr_debug("ZCD_CON4 = 0x%x\n", Ana_Get_Reg(ZCD_CON4));
	pr_debug("ZCD_CON5 = 0x%x\n", Ana_Get_Reg(ZCD_CON5));


	pr_debug("GPIO_MODE2 = 0x%x\n", Ana_Get_Reg(GPIO_MODE2));
	pr_debug("GPIO_MODE3 = 0x%x\n", Ana_Get_Reg(GPIO_MODE3));
	pr_debug("GPIO_DIR0 = 0x%x\n", Ana_Get_Reg(GPIO_DIR0));
	pr_debug("DRV_CON3 = 0x%x\n", Ana_Get_Reg(DRV_CON3));
	pr_debug("DCXO_CW14 = 0x%x\n", Ana_Get_Reg(DCXO_CW14));

	audckbufEnable(false);

	pr_debug("-%s()\n", __func__);
}
EXPORT_SYMBOL(Ana_Log_Print);

int Ana_Debug_Read(char *buffer, const int size)
{
	int n = 0;

	n += scnprintf(buffer + n, size - n, "AUD_TOP_ID = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_ID));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_REV0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_REV0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_REV1 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_REV1));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKPDN_PM0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKPDN_PM0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKPDN_PM1 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKPDN_PM1));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKPDN_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKPDN_CON0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKSEL_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKSEL_CON0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_CKTST_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_CKTST_CON0));
	n += scnprintf(buffer + n, size - n, "AUD_TOP_RST_CON0 = 0x%x\n",
		       Ana_Get_Reg(AUD_TOP_RST_CON0));
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
	n += scnprintf(buffer + n, size - n,
		       "AUD_TOP_INT_RAW_STATUS0 = 0x%x\n",
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
	n += scnprintf(buffer + n, size - n, "AUDIO_DIG_ID = 0x%x\n",
		       Ana_Get_Reg(AUDIO_DIG_ID));
	n += scnprintf(buffer + n, size - n, "AUDIO_DIG_REV0 = 0x%x\n",
		       Ana_Get_Reg(AUDIO_DIG_REV0));
	n += scnprintf(buffer + n, size - n, "AUDIO_DIG_REV1 = 0x%x\n",
		       Ana_Get_Reg(AUDIO_DIG_REV1));
	n += scnprintf(buffer + n, size - n, "AFE_UL_DL_CON0 = 0x%x\n",
		       Ana_Get_Reg(AFE_UL_DL_CON0));
	n += scnprintf(buffer + n, size - n, "AFE_DL_SRC2_CON0_L = 0x%x\n",
		       Ana_Get_Reg(AFE_DL_SRC2_CON0_L));
	n += scnprintf(buffer + n, size - n, "AFE_UL_SRC_CON0_H = 0x%x\n",
		       Ana_Get_Reg(AFE_UL_SRC_CON0_H));
	n += scnprintf(buffer + n, size - n, "AFE_UL_SRC_CON0_L = 0x%x\n",
		       Ana_Get_Reg(AFE_UL_SRC_CON0_L));
	n += scnprintf(buffer + n, size - n, "PMIC_AFE_TOP_CON0 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_TOP_CON0));
	n += scnprintf(buffer + n, size - n, "PMIC_AUDIO_TOP_CON0 = 0x%x\n",
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
	n += scnprintf(buffer + n, size - n,
		       "PMIC_AFE_ADDA_MTKAIF_MON0 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON0));
	n += scnprintf(buffer + n, size - n,
		       "PMIC_AFE_ADDA_MTKAIF_MON1 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON1));
	n += scnprintf(buffer + n, size - n,
		       "PMIC_AFE_ADDA_MTKAIF_MON2 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON2));
	n += scnprintf(buffer + n, size - n,
		       "PMIC_AFE_ADDA_MTKAIF_MON3 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_MON3));
	n += scnprintf(buffer + n, size - n,
		       "PMIC_AFE_ADDA_MTKAIF_CFG0 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_CFG0));
	n += scnprintf(buffer + n, size - n,
		       "PMIC_AFE_ADDA_MTKAIF_RX_CFG0 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG0));
	n += scnprintf(buffer + n, size - n,
		       "PMIC_AFE_ADDA_MTKAIF_RX_CFG1 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG1));
	n += scnprintf(buffer + n, size - n,
		       "PMIC_AFE_ADDA_MTKAIF_RX_CFG2 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG2));
	n += scnprintf(buffer + n, size - n,
		       "PMIC_AFE_ADDA_MTKAIF_RX_CFG3 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_RX_CFG3));
	n += scnprintf(buffer + n, size - n,
		       "PMIC_AFE_ADDA_MTKAIF_TX_CFG1 = 0x%x\n",
		       Ana_Get_Reg(PMIC_AFE_ADDA_MTKAIF_TX_CFG1));
	n += scnprintf(buffer + n, size - n, "AFE_SGEN_CFG0 = 0x%x\n",
		       Ana_Get_Reg(AFE_SGEN_CFG0));
	n += scnprintf(buffer + n, size - n, "AFE_SGEN_CFG1 = 0x%x\n",
		       Ana_Get_Reg(AFE_SGEN_CFG1));
	n += scnprintf(buffer + n, size - n,
		       "AFE_ADC_ASYNC_FIFO_CFG = 0x%x\n",
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
	n += scnprintf(buffer + n, size - n, "AUDENC_DSN_ID = 0x%x\n",
		       Ana_Get_Reg(AUDENC_DSN_ID));
	n += scnprintf(buffer + n, size - n, "AUDENC_DSN_REV0 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_DSN_REV0));
	n += scnprintf(buffer + n, size - n, "AUDENC_DSN_REV1 = 0x%x\n",
		       Ana_Get_Reg(AUDENC_DSN_REV1));
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
	n += scnprintf(buffer + n, size - n, "AUDDEC_DSN_ID = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_DSN_ID));
	n += scnprintf(buffer + n, size - n, "AUDDEC_DSN_REV0 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_DSN_REV0));
	n += scnprintf(buffer + n, size - n, "AUDDEC_DSN_REV1 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_DSN_REV1));
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
	n += scnprintf(buffer + n, size - n, "AUDDEC_ELR_NUM = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ELR_NUM));
	n += scnprintf(buffer + n, size - n, "AUDDEC_ELR_0 = 0x%x\n",
		       Ana_Get_Reg(AUDDEC_ELR_0));
	n += scnprintf(buffer + n, size - n, "AUDZCDID = 0x%x\n",
		       Ana_Get_Reg(AUDZCDID));
	n += scnprintf(buffer + n, size - n, "AUDZCDREV0 = 0x%x\n",
		       Ana_Get_Reg(AUDZCDREV0));
	n += scnprintf(buffer + n, size - n, "AUDZCDREV1 = 0x%x\n",
		       Ana_Get_Reg(AUDZCDREV1));
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
	n += scnprintf(buffer + n, size - n, "DCXO_CW14  = 0x%x\n",
		       Ana_Get_Reg(DCXO_CW14));
	return n;
}
EXPORT_SYMBOL(Ana_Debug_Read);
MODULE_LICENSE("GPL v2");

/* export symbols for other module using */
