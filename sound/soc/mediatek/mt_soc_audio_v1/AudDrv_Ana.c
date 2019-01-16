/*
 * Copyright (C) 2007 The Android Open Source Project
 * Copyright (C) 2018 XiaoMi, Inc.
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
 *   AudDrv_Ana.c
 *
 * Project:
 * --------
 *   MT6583  Audio Driver ana Register setting
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
 * $Revision: #1 $
 * $Modtime:$
 * $Log:$
 *
 *
 *******************************************************************************/


/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"

// define this to use wrapper to control
#ifndef CONFIG_MTK_FPGA
#define AUDIO_USING_WRAP_DRIVER
#endif

#ifdef AUDIO_USING_WRAP_DRIVER
#include <mach/mt_pmic_wrap.h>
#endif

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

void Ana_Set_Reg(uint32 offset, uint32 value, uint32 mask)
{
    // set pmic register or analog CONTROL_IFACE_PATH
    int ret = 0;
    uint32 Reg_Value = 0;
    PRINTK_ANA_REG("Ana_Set_Reg offset= 0x%x , value = 0x%x mask = 0x%x\n", offset, value, mask);
#ifdef AUDIO_USING_WRAP_DRIVER
    Reg_Value = Ana_Get_Reg(offset);
    Reg_Value &= (~mask);
    Reg_Value |= (value & mask);
    ret = pwrap_write(offset, Reg_Value);
    Reg_Value = Ana_Get_Reg(offset);
    if ((Reg_Value & mask) != (value & mask))
    {
        //delete // by lct huangchunyan from mtk
        printk("Ana_Set_Reg offset= 0x%x , value = 0x%x mask = 0x%x ret = %d Reg_Value = 0x%x\n", offset, value, mask, ret, Reg_Value);  
    }
#endif
}

uint32 Ana_Get_Reg(uint32 offset)
{
    // get pmic register
    int ret = 0;
    uint32 Rdata = 0;
#ifdef AUDIO_USING_WRAP_DRIVER
    ret = pwrap_read(offset, &Rdata);
#endif
    PRINTK_ANA_REG("Ana_Get_Reg offset= 0x%x  Rdata = 0x%x ret = %d\n", offset, Rdata, ret);
    return Rdata;
}

void Ana_Log_Print(void)
{
    AudDrv_ANA_Clk_On();
    printk("AFE_UL_DL_CON0  = 0x%x\n", Ana_Get_Reg(AFE_UL_DL_CON0));
    printk("AFE_DL_SRC2_CON0_H  = 0x%x\n", Ana_Get_Reg(AFE_DL_SRC2_CON0_H));
    printk("AFE_DL_SRC2_CON0_L  = 0x%x\n", Ana_Get_Reg(AFE_DL_SRC2_CON0_L));
    printk("AFE_DL_SDM_CON0  = 0x%x\n", Ana_Get_Reg(AFE_DL_SDM_CON0));
    printk("AFE_DL_SDM_CON1  = 0x%x\n", Ana_Get_Reg(AFE_DL_SDM_CON1));
    printk("AFE_UL_SRC0_CON0_H  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC0_CON0_H));
    printk("AFE_UL_SRC0_CON0_L  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC0_CON0_L));
    printk("AFE_UL_SRC1_CON0_H  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC1_CON0_H));
    printk("AFE_UL_SRC1_CON0_L  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC1_CON0_L));
    printk("PMIC_AFE_TOP_CON0  = 0x%x\n", Ana_Get_Reg(PMIC_AFE_TOP_CON0));
    printk("AFE_AUDIO_TOP_CON0  = 0x%x\n", Ana_Get_Reg(AFE_AUDIO_TOP_CON0));
    printk("PMIC_AFE_TOP_CON0  = 0x%x\n", Ana_Get_Reg(PMIC_AFE_TOP_CON0));
    printk("AFE_DL_SRC_MON0  = 0x%x\n", Ana_Get_Reg(AFE_DL_SRC_MON0));
    printk("AFE_DL_SDM_TEST0  = 0x%x\n", Ana_Get_Reg(AFE_DL_SDM_TEST0));
    printk("AFE_MON_DEBUG0  = 0x%x\n", Ana_Get_Reg(AFE_MON_DEBUG0));
    printk("AUDRC_TUNE_MON0  = 0x%x\n", Ana_Get_Reg(AUDRC_TUNE_MON0));
    printk("AFE_UP8X_FIFO_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_UP8X_FIFO_CFG0));
    printk("AFE_UP8X_FIFO_LOG_MON0  = 0x%x\n", Ana_Get_Reg(AFE_UP8X_FIFO_LOG_MON0));
    printk("AFE_UP8X_FIFO_LOG_MON1  = 0x%x\n", Ana_Get_Reg(AFE_UP8X_FIFO_LOG_MON1));
    printk("AFE_DL_DC_COMP_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_DL_DC_COMP_CFG0));
    printk("AFE_DL_DC_COMP_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_DL_DC_COMP_CFG1));
    printk("AFE_DL_DC_COMP_CFG2  = 0x%x\n", Ana_Get_Reg(AFE_DL_DC_COMP_CFG2));
    printk("AFE_PMIC_NEWIF_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG0));
    printk("AFE_PMIC_NEWIF_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG1));
    printk("AFE_PMIC_NEWIF_CFG2  = 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG2));
    printk("AFE_PMIC_NEWIF_CFG3  = 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG3));
    printk("AFE_SGEN_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_SGEN_CFG0));
    printk("AFE_SGEN_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_SGEN_CFG1));
    printk("AFE_ADDA2_UL_SRC_CON0_H  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_UL_SRC_CON0_H));
    printk("AFE_ADDA2_UL_SRC_CON0_L  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_UL_SRC_CON0_L));
    printk("AFE_UL_SRC_CON1_H  = 0x%x\n", Ana_Get_Reg(AFE_UL_SRC_CON1_H));

    printk("AFE_ADDA2_UL_SRC_CON1_L  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_UL_SRC_CON1_L));
    printk("AFE_ADDA2_UP8X_FIFO_LOG_MON0  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_UP8X_FIFO_LOG_MON0));
    printk("AFE_ADDA2_UP8X_FIFO_LOG_MON1  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_UP8X_FIFO_LOG_MON1));
    printk("AFE_ADDA2_PMIC_NEWIF_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_PMIC_NEWIF_CFG0));
    printk("AFE_ADDA2_PMIC_NEWIF_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_PMIC_NEWIF_CFG1));
    printk("AFE_ADDA2_PMIC_NEWIF_CFG2  = 0x%x\n", Ana_Get_Reg(AFE_ADDA2_PMIC_NEWIF_CFG2));
    printk("AFE_MIC_ARRAY_CFG  = 0x%x\n", Ana_Get_Reg(AFE_MIC_ARRAY_CFG));
    printk("AFE_ADC_ASYNC_FIFO_CFG  = 0x%x\n", Ana_Get_Reg(AFE_ADC_ASYNC_FIFO_CFG));
    printk("AFE_ANC_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_ANC_CFG0));
    printk("AFE_ANC_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_ANC_CFG1));
    printk("AFE_ANC_COEF_B00  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_B00));
    printk("AFE_ANC_COEF_ADDR  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_ADDR));
    printk("AFE_ANC_COEF_WDATA  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_WDATA));
    printk("AFE_ANC_COEF_RDATA  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_RDATA));
    printk("AUDRC_TUNE_UL2_MON0  = 0x%x\n", Ana_Get_Reg(AUDRC_TUNE_UL2_MON0));
    printk("AFE_MBIST_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_MBIST_CFG0));
    printk("AFE_MBIST_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_MBIST_CFG1));
    printk("AFE_MBIST_CFG2  = 0x%x\n", Ana_Get_Reg(AFE_MBIST_CFG2));
    printk("AFE_MBIST_CFG3  = 0x%x\n", Ana_Get_Reg(AFE_MBIST_CFG3));
    printk("AFE_VOW_TOP  = 0x%x\n", Ana_Get_Reg(AFE_VOW_TOP));
    printk("AFE_VOW_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_VOW_CFG0));
    printk("AFE_VOW_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_VOW_CFG1));
    printk("AFE_VOW_CFG2  = 0x%x\n", Ana_Get_Reg(AFE_VOW_CFG2));
    printk("AFE_VOW_CFG3  = 0x%x\n", Ana_Get_Reg(AFE_VOW_CFG3));
    printk("AFE_VOW_CFG4  = 0x%x\n", Ana_Get_Reg(AFE_VOW_CFG4));
    printk("AFE_VOW_CFG5  = 0x%x\n", Ana_Get_Reg(AFE_VOW_CFG5));
    printk("AFE_VOW_MON0  = 0x%x\n", Ana_Get_Reg(AFE_VOW_MON0));
    printk("AFE_VOW_MON1  = 0x%x\n", Ana_Get_Reg(AFE_VOW_MON1));
    printk("AFE_VOW_MON2  = 0x%x\n", Ana_Get_Reg(AFE_VOW_MON2));
    printk("AFE_VOW_MON3  = 0x%x\n", Ana_Get_Reg(AFE_VOW_MON3));
    printk("AFE_VOW_MON4  = 0x%x\n", Ana_Get_Reg(AFE_VOW_MON4));
    printk("AFE_VOW_MON5  = 0x%x\n", Ana_Get_Reg(AFE_VOW_MON5));

    printk("AFE_CLASSH_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG0));
    printk("AFE_CLASSH_CFG1  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG1));
    printk("AFE_CLASSH_CFG2  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG2));
    printk("AFE_CLASSH_CFG3  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG3));
    printk("AFE_CLASSH_CFG4  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG4));
    printk("AFE_CLASSH_CFG5  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG5));
    printk("AFE_CLASSH_CFG6  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG6));
    printk("AFE_CLASSH_CFG7  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG7));
    printk("AFE_CLASSH_CFG8  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG8));
    printk("AFE_CLASSH_CFG9  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG9));
    printk("AFE_CLASSH_CFG10 = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG10));
    printk("AFE_CLASSH_CFG11 = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG11));
    printk("AFE_CLASSH_CFG12  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG12));
    printk("AFE_CLASSH_CFG13  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG13));
    printk("AFE_CLASSH_CFG14  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG14));
    printk("AFE_CLASSH_CFG15  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG15));
    printk("AFE_CLASSH_CFG16  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG16));
    printk("AFE_CLASSH_CFG17  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG17));
    printk("AFE_CLASSH_CFG18  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG18));
    printk("AFE_CLASSH_CFG19  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG19));
    printk("AFE_CLASSH_CFG20  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG20));
    printk("AFE_CLASSH_CFG21  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG21));
    printk("AFE_CLASSH_CFG22  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG22));
    printk("AFE_CLASSH_CFG23  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG23));
    printk("AFE_CLASSH_CFG24  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG24));
    printk("AFE_CLASSH_CFG25  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG25));
    printk("AFE_CLASSH_CFG26  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG26));
    printk("AFE_CLASSH_CFG27  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG27));
    printk("AFE_CLASSH_CFG28  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG28));
    printk("AFE_CLASSH_CFG29  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG29));
    printk("AFE_CLASSH_CFG30  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_CFG30));
    printk("AFE_CLASSH_MON00  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_MON00));
    printk("AFE_CLASSH_MON1  = 0x%x\n", Ana_Get_Reg(AFE_CLASSH_MON1));
    printk("AFE_DCCLK_CFG0  = 0x%x\n", Ana_Get_Reg(AFE_DCCLK_CFG0));
    printk("AFE_ANC_COEF_MON1  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_MON1));
    printk("AFE_ANC_COEF_MON2  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_MON2));
    printk("AFE_ANC_COEF_MON3  = 0x%x\n", Ana_Get_Reg(AFE_ANC_COEF_MON3));

    printk("TOP_STATUS  = 0x%x\n", Ana_Get_Reg(TOP_STATUS));
    printk("TOP_CKPDN_CON0  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN_CON0));
    printk("TOP_CKPDN_CON1  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN_CON1));
    printk("TOP_CKPDN_CON2  = 0x%x\n", Ana_Get_Reg(TOP_CKPDN_CON2));
    printk("TOP_CKSEL_CON  = 0x%x\n", Ana_Get_Reg(TOP_CKSEL_CON));
    printk("TOP_CLKSQ  = 0x%x\n", Ana_Get_Reg(TOP_CLKSQ));
    printk("ZCD_CON0  = 0x%x\n", Ana_Get_Reg(ZCD_CON0));
    printk("ZCD_CON1  = 0x%x\n", Ana_Get_Reg(ZCD_CON1));
    printk("ZCD_CON2  = 0x%x\n", Ana_Get_Reg(ZCD_CON2));
    printk("ZCD_CON3  = 0x%x\n", Ana_Get_Reg(ZCD_CON3));
    printk("ZCD_CON4  = 0x%x\n", Ana_Get_Reg(ZCD_CON4));
    printk("ZCD_CON5  = 0x%x\n", Ana_Get_Reg(ZCD_CON5));

    printk("AUDDAC_CFG0  = 0x%x\n", Ana_Get_Reg(AUDDAC_CFG0));
    printk("AUDBUF_CFG0  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG0));
    printk("AUDBUF_CFG1  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG1));
    printk("AUDBUF_CFG2  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG2));
    printk("AUDBUF_CFG3  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG3));
    printk("AUDBUF_CFG4  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG4));
    printk("AUDBUF_CFG5  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG5));
    printk("AUDBUF_CFG6  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG6));
    printk("AUDBUF_CFG7  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG7));
    printk("AUDBUF_CFG8  = 0x%x\n", Ana_Get_Reg(AUDBUF_CFG8));
    printk("IBIASDIST_CFG0  = 0x%x\n", Ana_Get_Reg(IBIASDIST_CFG0));
    printk("AUDCLKGEN_CFG0  = 0x%x\n", Ana_Get_Reg(AUDCLKGEN_CFG0));
    printk("AUDLDO_CFG0  = 0x%x\n", Ana_Get_Reg(AUDLDO_CFG0));
    printk("AUDDCDC_CFG1  = 0x%x\n", Ana_Get_Reg(AUDDCDC_CFG1));
    printk("AUDNVREGGLB_CFG0  = 0x%x\n", Ana_Get_Reg(AUDNVREGGLB_CFG0));
    printk("AUD_NCP0  = 0x%x\n", Ana_Get_Reg(AUD_NCP0));
    printk("AUD_ZCD_CFG0  = 0x%x\n", Ana_Get_Reg(AUD_ZCD_CFG0));
    printk("AUDPREAMP_CFG0  = 0x%x\n", Ana_Get_Reg(AUDPREAMP_CFG0));
    printk("AUDPREAMP_CFG1  = 0x%x\n", Ana_Get_Reg(AUDPREAMP_CFG1));
    printk("AUDPREAMP_CFG2  = 0x%x\n", Ana_Get_Reg(AUDPREAMP_CFG2));
    printk("AUDADC_CFG0  = 0x%x\n", Ana_Get_Reg(AUDADC_CFG0));
    printk("AUDADC_CFG1  = 0x%x\n", Ana_Get_Reg(AUDADC_CFG1));
    printk("AUDADC_CFG2  = 0x%x\n", Ana_Get_Reg(AUDADC_CFG2));
    printk("AUDADC_CFG3  = 0x%x\n", Ana_Get_Reg(AUDADC_CFG3));
    printk("AUDADC_CFG4  = 0x%x\n", Ana_Get_Reg(AUDADC_CFG4));
    printk("AUDADC_CFG5  = 0x%x\n", Ana_Get_Reg(AUDADC_CFG5));

    printk("AUDDIGMI_CFG0  = 0x%x\n", Ana_Get_Reg(AUDDIGMI_CFG0));
    printk("AUDDIGMI_CFG1  = 0x%x\n", Ana_Get_Reg(AUDDIGMI_CFG1));
    printk("AUDMICBIAS_CFG0  = 0x%x\n", Ana_Get_Reg(AUDMICBIAS_CFG0));
    printk("AUDMICBIAS_CFG1  = 0x%x\n", Ana_Get_Reg(AUDMICBIAS_CFG1));
    printk("AUDENCSPARE_CFG0  = 0x%x\n", Ana_Get_Reg(AUDENCSPARE_CFG0));
    printk("AUDPREAMPGAIN_CFG0  = 0x%x\n", Ana_Get_Reg(AUDPREAMPGAIN_CFG0));
    printk("AUDVOWPLL_CFG0  = 0x%x\n", Ana_Get_Reg(AUDVOWPLL_CFG0));
    printk("AUDVOWPLL_CFG1  = 0x%x\n", Ana_Get_Reg(AUDVOWPLL_CFG1));
    printk("AUDVOWPLL_CFG2  = 0x%x\n", Ana_Get_Reg(AUDVOWPLL_CFG2));
    printk("AUDENCSPARE_CFG0  = 0x%x\n", Ana_Get_Reg(AUDENCSPARE_CFG0));

    printk("AUDLDO_NVREG_CFG0  = 0x%x\n", Ana_Get_Reg(AUDLDO_NVREG_CFG0));
    printk("AUDLDO_NVREG_CFG1  = 0x%x\n", Ana_Get_Reg(AUDLDO_NVREG_CFG1));
    printk("AUDLDO_NVREG_CFG2  = 0x%x\n", Ana_Get_Reg(AUDLDO_NVREG_CFG2));

    printk("ANALDO_CON3  = 0x%x\n", Ana_Get_Reg(ANALDO_CON3));

#ifdef CONFIG_MTK_SPEAKER
    printk("TOP_CKPDN_CON0  = 0x%x\n", Ana_Get_Reg(SPK_TOP_CKPDN_CON0));
    printk("TOP_CKPDN_CON1  = 0x%x\n", Ana_Get_Reg(SPK_TOP_CKPDN_CON1));
    printk("VSBST_CON5  = 0x%x\n", Ana_Get_Reg(VSBST_CON5));
    printk("VSBST_CON8  = 0x%x\n", Ana_Get_Reg(VSBST_CON8));
    printk("VSBST_CON10  = 0x%x\n", Ana_Get_Reg(VSBST_CON10));
    printk("VSBST_CON12  = 0x%x\n", Ana_Get_Reg(VSBST_CON12));
    printk("VSBST_CON20  = 0x%x\n", Ana_Get_Reg(VSBST_CON20));

    printk("SPK_CON0  = 0x%x\n", Ana_Get_Reg(SPK_CON0));
    printk("SPK_CON2  = 0x%x\n", Ana_Get_Reg(SPK_CON2));
    printk("SPK_CON9  = 0x%x\n", Ana_Get_Reg(SPK_CON9));
    printk("SPK_CON12  = 0x%x\n", Ana_Get_Reg(SPK_CON12));
    printk("SPK_CON13  = 0x%x\n", Ana_Get_Reg(SPK_CON13));
    printk("SPK_CON14  = 0x%x\n", Ana_Get_Reg(SPK_CON14));
    printk("SPK_CON16  = 0x%x\n", Ana_Get_Reg(SPK_CON16));
#endif
    printk("MT6332_AUXADC_CON12  = 0x%x\n", Ana_Get_Reg(MT6332_AUXADC_CON12));
    printk("MT6332_AUXADC_CON13  = 0x%x\n", Ana_Get_Reg(MT6332_AUXADC_CON13));
    printk("MT6332_AUXADC_CON33  = 0x%x\n", Ana_Get_Reg(MT6332_AUXADC_CON33));
    printk("MT6332_AUXADC_CON36  = 0x%x\n", Ana_Get_Reg(MT6332_AUXADC_CON36));
    AudDrv_ANA_Clk_Off();
    printk("-Ana_Log_Print \n");
}


// export symbols for other module using
EXPORT_SYMBOL(Ana_Log_Print);
EXPORT_SYMBOL(Ana_Set_Reg);
EXPORT_SYMBOL(Ana_Get_Reg);


