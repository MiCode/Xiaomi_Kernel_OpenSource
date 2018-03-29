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

/* define this to use wrapper to control */
#ifdef AUDIO_USING_WRAP_DRIVER
#include <mt-plat/mt_pmic_wrap.h>
#endif

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

void Ana_Set_Reg(uint32 offset, uint32 value, uint32 mask)
{
	/* set pmic register or analog CONTROL_IFACE_PATH */
	int ret = 0;
	uint32 Reg_Value;

	PRINTK_ANA_REG("Ana_Set_Reg offset= 0x%x, value = 0x%x, mask = 0x%x\n", offset, value, mask);
#ifdef AUDIO_USING_WRAP_DRIVER
	Reg_Value = Ana_Get_Reg(offset);
	Reg_Value &= (~mask);
	Reg_Value |= (value & mask);
	ret = pwrap_write(offset, Reg_Value);
	Reg_Value = Ana_Get_Reg(offset);
#if 0
	if ((Reg_Value & mask) != (value & mask)) {
		pr_warn("Ana_Set_Reg offset= 0x%x, value = 0x%x mask = 0x%x ret = %d Reg_Value = 0x%x\n",
			offset, value, mask, ret, Reg_Value);
	}
#endif
#endif
}
EXPORT_SYMBOL(Ana_Set_Reg);

uint32 Ana_Get_Reg(uint32 offset)
{
	/* get pmic register */
	int ret = 0;
	uint32 Rdata = 0;
#ifdef AUDIO_USING_WRAP_DRIVER
	ret = pwrap_read(offset, &Rdata);
#endif
	PRINTK_ANA_REG("Ana_Get_Reg offset= 0x%x  Rdata = 0x%x ret = %d\n", offset, Rdata, ret);
	return Rdata;
}
EXPORT_SYMBOL(Ana_Get_Reg);

void Ana_Log_Print(void)
{
	AudDrv_ANA_Clk_On();
	PRINTK_ANA_REG("ABB_AFE_CON0	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON0));
	PRINTK_ANA_REG("ABB_AFE_CON1	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON1));
	PRINTK_ANA_REG("ABB_AFE_CON2	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON2));
	PRINTK_ANA_REG("ABB_AFE_CON3  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON3));
	PRINTK_ANA_REG("ABB_AFE_CON4  = 0x%x\n", Ana_Get_Reg(ABB_AFE_CON4));
	PRINTK_ANA_REG("ABB_AFE_CON5	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON5));
	PRINTK_ANA_REG("ABB_AFE_CON6	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON6));
	PRINTK_ANA_REG("ABB_AFE_CON7	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON7));
	PRINTK_ANA_REG("ABB_AFE_CON8	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON8));
	PRINTK_ANA_REG("ABB_AFE_CON9	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON9));
	PRINTK_ANA_REG("ABB_AFE_CON10	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON10));
	PRINTK_ANA_REG("ABB_AFE_CON11	= 0x%x\n", Ana_Get_Reg(ABB_AFE_CON11));
	PRINTK_ANA_REG("ABB_AFE_STA0	= 0x%x\n", Ana_Get_Reg(ABB_AFE_STA0));
	PRINTK_ANA_REG("ABB_AFE_STA1	= 0x%x\n", Ana_Get_Reg(ABB_AFE_STA1));
	PRINTK_ANA_REG("ABB_AFE_STA2	= 0x%x\n", Ana_Get_Reg(ABB_AFE_STA2));
	PRINTK_ANA_REG("AFE_UP8X_FIFO_CFG0	= 0x%x\n", Ana_Get_Reg(AFE_UP8X_FIFO_CFG0));
	PRINTK_ANA_REG("AFE_UP8X_FIFO_LOG_MON0	= 0x%x\n", Ana_Get_Reg(AFE_UP8X_FIFO_LOG_MON0));
	PRINTK_ANA_REG("AFE_UP8X_FIFO_LOG_MON1	= 0x%x\n", Ana_Get_Reg(AFE_UP8X_FIFO_LOG_MON1));
	PRINTK_ANA_REG("AFE_PMIC_NEWIF_CFG0	= 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG0));
	PRINTK_ANA_REG("AFE_PMIC_NEWIF_CFG1	= 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG1));
	PRINTK_ANA_REG("AFE_PMIC_NEWIF_CFG2	= 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG2));
	PRINTK_ANA_REG("AFE_PMIC_NEWIF_CFG3	= 0x%x\n", Ana_Get_Reg(AFE_PMIC_NEWIF_CFG3));
	PRINTK_ANA_REG("ABB_AFE_TOP_CON0	= 0x%x\n", Ana_Get_Reg(ABB_AFE_TOP_CON0));
	PRINTK_ANA_REG("ABB_MON_DEBUG0	= 0x%x\n", Ana_Get_Reg(ABB_MON_DEBUG0));
	PRINTK_ANA_REG("TOP_CKPDN0	= 0x%x\n", Ana_Get_Reg(TOP_CKPDN0));
	PRINTK_ANA_REG("TOP_CKPDN1	= 0x%x\n", Ana_Get_Reg(TOP_CKPDN1));
	PRINTK_ANA_REG("TOP_CKPDN2	= 0x%x\n", Ana_Get_Reg(TOP_CKPDN2));
	PRINTK_ANA_REG("TOP_CKCON1	= 0x%x\n", Ana_Get_Reg(TOP_CKCON1));
	PRINTK_ANA_REG("SPK_CON0	= 0x%x\n", Ana_Get_Reg(SPK_CON0));
	PRINTK_ANA_REG("SPK_CON1	= 0x%x\n", Ana_Get_Reg(SPK_CON1));
	PRINTK_ANA_REG("SPK_CON2	= 0x%x\n", Ana_Get_Reg(SPK_CON2));
	PRINTK_ANA_REG("SPK_CON6	= 0x%x\n", Ana_Get_Reg(SPK_CON6));
	PRINTK_ANA_REG("SPK_CON7	= 0x%x\n", Ana_Get_Reg(SPK_CON7));
	PRINTK_ANA_REG("SPK_CON8	= 0x%x\n", Ana_Get_Reg(SPK_CON8));
	PRINTK_ANA_REG("SPK_CON9	= 0x%x\n", Ana_Get_Reg(SPK_CON9));
	PRINTK_ANA_REG("SPK_CON10	= 0x%x\n", Ana_Get_Reg(SPK_CON10));
	PRINTK_ANA_REG("SPK_CON11	= 0x%x\n", Ana_Get_Reg(SPK_CON11));
	PRINTK_ANA_REG("AUDTOP_CON0	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON0));
	PRINTK_ANA_REG("AUDTOP_CON1	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON1));
	PRINTK_ANA_REG("AUDTOP_CON2	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON2));
	PRINTK_ANA_REG("AUDTOP_CON3	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON3));
	PRINTK_ANA_REG("AUDTOP_CON4	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON4));
	PRINTK_ANA_REG("AUDTOP_CON5	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON5));
	PRINTK_ANA_REG("AUDTOP_CON7	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON6));
	PRINTK_ANA_REG("AUDTOP_CON8	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON7));
	PRINTK_ANA_REG("AUDTOP_CON9	= 0x%x\n", Ana_Get_Reg(AUDTOP_CON9));
	AudDrv_ANA_Clk_Off();
	pr_debug("-Ana_Log_Print\n");
}
EXPORT_SYMBOL(Ana_Log_Print);


