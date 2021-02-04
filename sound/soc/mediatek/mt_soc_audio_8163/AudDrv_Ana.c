/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */



/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "AudDrv_Common.h"
#include "AudDrv_Ana.h"
#include "AudDrv_Clk.h"

/* define this to use wrapper to control */
#ifdef AUDIO_USING_WRAP_DRIVER
#include <mt-plat/upmu_common.h>
#endif

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

void Ana_Set_Reg(uint32 offset, uint32 value, uint32 mask)
{
	int ret = 0;
	uint32 Reg_Value;

	pr_debug
	("Ana_Set_Reg offset 0x%x, value 0x%x, mask 0x%x\n",
		offset, value, mask);
#ifdef AUDIO_USING_WRAP_DRIVER
	Reg_Value = Ana_Get_Reg(offset);
	Reg_Value &= (~mask);
	Reg_Value |= (value & mask);
	ret = pmic_config_interface(offset, Reg_Value, 0xFFFF, 0x0);
	Reg_Value = Ana_Get_Reg(offset);
	if ((Reg_Value & mask) != (value & mask)) {
		pr_debug("offset= 0x%x, val= 0x%x mask= 0x%x ret= %d Reg_Val= 0x%x\n",
		offset, value, mask, ret, Reg_Value);
	}
#endif
}
EXPORT_SYMBOL(Ana_Set_Reg);

uint32 Ana_Get_Reg(uint32 offset)
{
	/* get pmic register */
	int ret = 0;
	uint32 Rdata = 0;
#ifdef AUDIO_USING_WRAP_DRIVER
	ret = pmic_read_interface(offset, &Rdata, 0xFFFF, 0x0);
#endif
	pr_debug("Ana_Get_Reg offset= 0x%x  Rdata = 0x%x ret = %d\n",
		offset, Rdata, ret);
	return Rdata;
}
EXPORT_SYMBOL(Ana_Get_Reg);

void Ana_Log_Print(void)
{
	AudDrv_ANA_Clk_On();
	pr_debug("ABB_AFE_CON0	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON0));
	pr_debug("ABB_AFE_CON1	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON1));
	pr_debug("ABB_AFE_CON2	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON2));
	pr_debug("ABB_AFE_CON3  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON3));
	pr_debug("ABB_AFE_CON4  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON4));
	pr_debug("ABB_AFE_CON5	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON5));
	pr_debug("ABB_AFE_CON6	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON6));
	pr_debug("ABB_AFE_CON7	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON7));
	pr_debug("ABB_AFE_CON8	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON8));
	pr_debug("ABB_AFE_CON9	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON9));
	pr_debug("ABB_AFE_CON10	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON10));
	pr_debug("ABB_AFE_CON11	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON11));
	pr_debug("ABB_AFE_STA0	= 0x%x\n", Ana_Get_Reg(ABB_AFE_STA0));
	pr_debug("ABB_AFE_STA1	= 0x%x\n", Ana_Get_Reg(ABB_AFE_STA1));
	pr_debug("ABB_AFE_STA2	= 0x%x\n", Ana_Get_Reg(ABB_AFE_STA2));
	pr_debug("AFE_UP8X_FIFO_CFG0	= 0x%x\n",
		Ana_Get_Reg(AFE_UP8X_FIFO_CFG0));
	pr_debug("AFE_UP8X_FIFO_LOG_MON0	= 0x%x\n",
		Ana_Get_Reg(AFE_UP8X_FIFO_LOG_MON0));
	pr_debug("AFE_UP8X_FIFO_LOG_MON1	= 0x%x\n",
		Ana_Get_Reg(AFE_UP8X_FIFO_LOG_MON1));
	pr_debug("AFE_PMIC_NEWIF_CFG0	= 0x%x\n",
		Ana_Get_Reg(AFE_PMIC_NEWIF_CFG0));
	pr_debug("AFE_PMIC_NEWIF_CFG1	= 0x%x\n",
	 Ana_Get_Reg(AFE_PMIC_NEWIF_CFG1));
	pr_debug("AFE_PMIC_NEWIF_CFG2	= 0x%x\n",
	 Ana_Get_Reg(AFE_PMIC_NEWIF_CFG2));
	pr_debug("AFE_PMIC_NEWIF_CFG3	= 0x%x\n",
	 Ana_Get_Reg(AFE_PMIC_NEWIF_CFG3));
	pr_debug("ABB_AFE_TOP_CON0	= 0x%x\n",
	 Ana_Get_Reg(ABB_AFE_TOP_CON0));
	pr_debug("ABB_MON_DEBUG0	= 0x%x\n",
	Ana_Get_Reg(ABB_MON_DEBUG0));
	pr_debug("TOP_CKPDN0	= 0x%x\n", Ana_Get_Reg(TOP_CKPDN0));
	pr_debug("TOP_CKPDN1	= 0x%x\n", Ana_Get_Reg(TOP_CKPDN1));
	pr_debug("TOP_CKPDN2	= 0x%x\n", Ana_Get_Reg(TOP_CKPDN2));
	pr_debug("TOP_CKCON1	= 0x%x\n", Ana_Get_Reg(TOP_CKCON1));
	pr_debug("SPK_CON0	= 0x%x\n", Ana_Get_Reg(SPK_CON0));
	pr_debug("SPK_CON1	= 0x%x\n", Ana_Get_Reg(SPK_CON1));
	pr_debug("SPK_CON2	= 0x%x\n", Ana_Get_Reg(SPK_CON2));
	pr_debug("SPK_CON6	= 0x%x\n", Ana_Get_Reg(SPK_CON6));
	pr_debug("SPK_CON7	= 0x%x\n", Ana_Get_Reg(SPK_CON7));
	pr_debug("SPK_CON8	= 0x%x\n", Ana_Get_Reg(SPK_CON8));
	pr_debug("SPK_CON9	= 0x%x\n", Ana_Get_Reg(SPK_CON9));
	pr_debug("SPK_CON10	= 0x%x\n", Ana_Get_Reg(SPK_CON10));
	pr_debug("SPK_CON11	= 0x%x\n", Ana_Get_Reg(SPK_CON11));
	pr_debug("AUDTOP_CON0	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON0));
	pr_debug("AUDTOP_CON1	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON1));
	pr_debug("AUDTOP_CON2	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON2));
	pr_debug("AUDTOP_CON3	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON3));
	pr_debug("AUDTOP_CON4	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON4));
	pr_debug("AUDTOP_CON5	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON5));
	pr_debug("AUDTOP_CON7	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON6));
	pr_debug("AUDTOP_CON8	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON7));
	pr_debug("AUDTOP_CON9	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON9));
	AudDrv_ANA_Clk_Off();
	pr_debug("-Ana_Log_Print\n");
}
EXPORT_SYMBOL(Ana_Log_Print);


