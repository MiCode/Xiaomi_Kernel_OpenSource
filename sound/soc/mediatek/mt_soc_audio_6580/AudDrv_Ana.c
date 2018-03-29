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
#ifndef CONFIG_FPGA_EARLY_PORTING
#define AUDIO_USING_WRAP_DRIVER
#endif

#ifdef AUDIO_USING_WRAP_DRIVER
#include <mach/mt_pmic_wrap.h>
static DEFINE_SPINLOCK(ana_set_reg_lock);
#endif

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

void Ana_Set_Reg(uint32 offset, uint32 value, uint32 mask)
{
#ifdef AUDIO_USING_WRAP_DRIVER
    /* set pmic register or analog CONTROL_IFACE_PATH */
	int ret = 0;
	uint32 Reg_Value;
	unsigned long flags = 0;

	PRINTK_ANA_REG("Ana_Set_Reg offset= 0x%x , value = 0x%x mask = 0x%x\n", offset,
			value, mask);

	spin_lock_irqsave(&ana_set_reg_lock, flags);
	Reg_Value = Ana_Get_Reg(offset);
	Reg_Value &= (~mask);
	Reg_Value |= (value & mask);
	ret = pwrap_write(offset, Reg_Value);
	spin_unlock_irqrestore(&ana_set_reg_lock, flags);

	Reg_Value = Ana_Get_Reg(offset);
	if ((Reg_Value & mask) != (value & mask))
		pr_debug("Ana_Set_Reg  mask = 0x%x ret = %d Reg_Value = 0x%x\n", mask, ret,
			 Reg_Value);
#endif
}
/* export symbols for other module using */
EXPORT_SYMBOL(Ana_Set_Reg);

uint32 Ana_Get_Reg(uint32 offset)
{
	/* get pmic register */
	uint32 Rdata = 0;
#ifdef AUDIO_USING_WRAP_DRIVER
	int ret = 0;
	ret = pwrap_read(offset, &Rdata);
	PRINTK_ANA_REG("Ana_Get_Reg offset= 0x%x  Rdata = 0x%x ret = %d\n", offset, Rdata, ret);
#endif
	return Rdata;
}
/* export symbols for other module using */
EXPORT_SYMBOL(Ana_Get_Reg);

void Ana_Log_Print(void)
{
	pr_debug("Ana_Log_Print++\n");
	AudDrv_ANA_Clk_On();

	pr_debug("ABB_AFE_CON0 = 0x%x, CON1 = 0x%x, CON2 = 0x%x, CON3 = 0x%x, CON4 = 0x%x, CON5 = 0x%x\n",
	Ana_Get_Reg(ABB_AFE_CON0), Ana_Get_Reg(ABB_AFE_CON1), Ana_Get_Reg(ABB_AFE_CON2), Ana_Get_Reg(ABB_AFE_CON3),
	Ana_Get_Reg(ABB_AFE_CON4), Ana_Get_Reg(ABB_AFE_CON5));

	pr_debug("ABB_AFE_CON6 = 0x%x, CON7 = 0x%x, CON8 = 0x%x, CON9 = 0x%x, CON10 = 0x%x, CON11 = 0x%x\n",
	Ana_Get_Reg(ABB_AFE_CON6), Ana_Get_Reg(ABB_AFE_CON7), Ana_Get_Reg(ABB_AFE_CON8), Ana_Get_Reg(ABB_AFE_CON9),
	Ana_Get_Reg(ABB_AFE_CON10), Ana_Get_Reg(ABB_AFE_CON11));

	pr_debug("ABB_AFE_STA0=0x%x,STA1=0x%x,STA2=0x%x,AFE_UP8X_FIFO_CFG0=0x%x,LOG_MON0=0x%x,MON1= 0x%x\n",
	Ana_Get_Reg(ABB_AFE_STA0), Ana_Get_Reg(ABB_AFE_STA1), Ana_Get_Reg(ABB_AFE_STA2),
	Ana_Get_Reg(AFE_UP8X_FIFO_CFG0), Ana_Get_Reg(AFE_PMIC_NEWIF_CFG1), Ana_Get_Reg(AFE_PMIC_NEWIF_CFG2));

	pr_debug("AFE_PMIC_NEWIF_CFG0=0x%x,CFG1=0x%x,CFG2=0x%x,CFG3=0x%x,ABB_AFE_TOP_CON0=0x%x,DEBUG0=0x%x\n",
	Ana_Get_Reg(AFE_PMIC_NEWIF_CFG0), Ana_Get_Reg(AFE_PMIC_NEWIF_CFG1), Ana_Get_Reg(AFE_PMIC_NEWIF_CFG2),
	Ana_Get_Reg(AFE_PMIC_NEWIF_CFG3), Ana_Get_Reg(ABB_AFE_TOP_CON0), Ana_Get_Reg(ABB_MON_DEBUG0));

	pr_debug("SPK_CON0=0x%x,CON1=0x%x,CON2=0x%x,CON6=0x%x,CON7=0x%x,CON8=0x%x,CON9=0x%x,CON10=0x%x\n",
	Ana_Get_Reg(SPK_CON0), Ana_Get_Reg(SPK_CON1), Ana_Get_Reg(SPK_CON2), Ana_Get_Reg(SPK_CON6),
	Ana_Get_Reg(SPK_CON7), Ana_Get_Reg(SPK_CON8), Ana_Get_Reg(SPK_CON9), Ana_Get_Reg(SPK_CON10));

	pr_debug("SPK_CON11=0x%x,CON12=0x%x,CID=0x%x,TOP_CKPDN0=0x%x,CKPDN0_SET=0x%x,CKPDN0_CLR=0x%x\n",
	Ana_Get_Reg(SPK_CON11), Ana_Get_Reg(SPK_CON12), Ana_Get_Reg(CID), Ana_Get_Reg(TOP_CKPDN0),
	Ana_Get_Reg(TOP_CKPDN0_SET), Ana_Get_Reg(TOP_CKPDN0_CLR));

	pr_debug("TOP_CKPDN1=0x%x,SET=0x%x,CLR=0x%x,TOP_CKPDN2=0x%x,SET=0x%x,CLR=0x%x, TOP_CKCON1 = 0x%x\n",
	Ana_Get_Reg(TOP_CKPDN1), Ana_Get_Reg(TOP_CKPDN1_SET), Ana_Get_Reg(TOP_CKPDN1_CLR), Ana_Get_Reg(TOP_CKPDN2),
	Ana_Get_Reg(TOP_CKPDN2_SET), Ana_Get_Reg(TOP_CKPDN2_CLR), Ana_Get_Reg(TOP_CKCON1));

	pr_debug("AUDTOP_CON0=0x%x,CON1=0x%x,CON2=0x%x,CON3=0x%x,CON4=0x%x,CON5=0x%x,CON6=0x%x\n",
	Ana_Get_Reg(AUDTOP_CON0), Ana_Get_Reg(AUDTOP_CON1), Ana_Get_Reg(AUDTOP_CON2), Ana_Get_Reg(AUDTOP_CON3),
	Ana_Get_Reg(AUDTOP_CON4), Ana_Get_Reg(AUDTOP_CON5), Ana_Get_Reg(AUDTOP_CON6));

	pr_debug("AUDTOP_CON7 = 0x%x, AUDTOP_CON8 = 0x%x, AUDTOP_CON9 = 0x%x\n",
	Ana_Get_Reg(AUDTOP_CON7), Ana_Get_Reg(AUDTOP_CON8), Ana_Get_Reg(AUDTOP_CON9));

	AudDrv_ANA_Clk_Off();
	pr_debug("-Ana_Log_Print\n");
}
/* export symbols for other module using */
EXPORT_SYMBOL(Ana_Log_Print);

