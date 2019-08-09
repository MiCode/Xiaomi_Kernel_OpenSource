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
#include "AudDrv_Afe.h"
#include "AudDrv_Clk.h"
#include "AudDrv_Def.h"
#include <linux/types.h>


/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/


/*****************************************************************************
 *                         FUNCTION IMPLEMENTATION
 *****************************************************************************/
static bool CheckOffset(uint32 offset)
{
	if (offset > AFE_MAXLENGTH)
		return false;
	return true;
}


/*****************************************************************************
 * FUNCTION
 *  Auddrv_Reg_map
 *
 * DESCRIPTION
 * Auddrv_Reg_map
 *
 *****************************************************************************
 */

/*
 *    global variable control
 */
static const unsigned int SramCaptureOffSet = (16 * 1024);

/* address for ioremap audio hardware register */
void *AFE_BASE_ADDRESS;
void *AFE_SRAM_ADDRESS;
void *AFE_TOP_ADDRESS;
void *AFE_CLK_ADDRESS;
void *AFE_INFRA_ADDRESS;
void *APLL_BASE_ADDRESS;

void Auddrv_Reg_map(void)
{
	AFE_SRAM_ADDRESS = ioremap_nocache(AFE_INTERNAL_SRAM_PHY_BASE,
		AFE_INTERNAL_SRAM_SIZE);
#ifndef CONFIG_OF
	AFE_BASE_ADDRESS = ioremap_nocache(AUDIO_HW_PHYSICAL_BASE, 0x1000);
#endif
	/* temp for hardawre code  set 0x1000629c = 0xd */
	AFE_TOP_ADDRESS = ioremap_nocache(AUDIO_POWER_TOP, 0x1000);
	AFE_INFRA_ADDRESS = ioremap_nocache(AUDIO_INFRA_BASE, 0x1000);
	/* temp for hardawre code  set clg cfg */
	AFE_CLK_ADDRESS = ioremap_nocache(AUDIO_CLKCFG_PHYSICAL_BASE, 0x1000);
	/* temp for hardawre code  set pll cfg */
	APLL_BASE_ADDRESS = ioremap_nocache(APLL_PHYSICAL_BASE, 0x1000);
}

dma_addr_t Get_Afe_Sram_Phys_Addr(void)
{
	return (dma_addr_t) AFE_INTERNAL_SRAM_PHY_BASE;
}

dma_addr_t Get_Afe_Sram_Capture_Phys_Addr(void)
{
	return (dma_addr_t)
		(AFE_INTERNAL_SRAM_PHY_BASE + SramCaptureOffSet);
}

void *Get_Afe_SramBase_Pointer()
{
	return AFE_SRAM_ADDRESS;
}

void *Get_Afe_SramCaptureBase_Pointer()
{
	char *CaptureSramPointer =
		(char *)(AFE_SRAM_ADDRESS) + SramCaptureOffSet;

	return (void *)CaptureSramPointer;
}

void *Get_Afe_Powertop_Pointer()
{
	return AFE_TOP_ADDRESS;
}

void *Get_AudClk_Pointer()
{
	return AFE_CLK_ADDRESS;
}

void *Get_Afe_Infra_Pointer()
{
	return AFE_INFRA_ADDRESS;
}

void Afe_Set_Reg(uint32 offset, uint32 value, uint32 mask)
{
	long address;
	uint32 *AFE_Register;
	uint32 val_tmp;

	if (CheckOffset(offset) == false)
		return;
#ifdef AUDIO_MEM_IOREMAP
	pr_debug
		("Afe_Set_Reg AUDIO_MEM_IOREMAP AFE_BASE_ADDRESS = %p\n",
		AFE_BASE_ADDRESS);
	address = (long)((char *)AFE_BASE_ADDRESS + offset);
#else
	address = (long)(AFE_BASE + offset);
#endif
	AFE_Register = (uint32 *)address;
	pr_debug("Afe_Set_Reg offset=%x, value=%x, mask=%x\n",
		offset, value, mask);
	val_tmp = Afe_Get_Reg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
}
EXPORT_SYMBOL(Afe_Set_Reg);

uint32 Afe_Get_Reg(uint32 offset)
{
	long address;
	uint32 *value;

	if (CheckOffset(offset) == false)
		return 0xffffffff;
#ifdef AUDIO_MEM_IOREMAP
	pr_debug
		("Afe_Get_Reg AFE_BASE_ADDRESS %p,offset %x\n",
	AFE_BASE_ADDRESS, offset);
	address = (long)((char *)AFE_BASE_ADDRESS + offset);
#else
	address = (long)(AFE_BASE + offset);
#endif
	value = (uint32 *)(address);
	pr_debug
		("Afe_Get_Reg offset=%x value%x\n", offset, *value);
	return *value;
}
EXPORT_SYMBOL(Afe_Get_Reg);

/* function to Set Cfg */
uint32 GetClkCfg(uint32 offset)
{
	long address = (long)((char *)AFE_CLK_ADDRESS + offset);
	uint32 *value;

	value = (uint32 *)(address);
	return *value;
}
EXPORT_SYMBOL(GetClkCfg);

void SetClkCfg(uint32 offset, uint32 value, uint32 mask)
{
	long address = (long)((char *)AFE_CLK_ADDRESS + offset);
	uint32 *AFE_Register = (uint32 *)address;
	uint32 val_tmp;

	pr_debug("SetClkCfg offset=%x, value=%x, mask=%x\n",
		offset, value, mask);
	val_tmp = GetClkCfg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
}
EXPORT_SYMBOL(SetClkCfg);

/* function to Set Cfg */
uint32 GetInfraCfg(uint32 offset)
{
	long address = (long)((char *)AFE_INFRA_ADDRESS + offset);
	uint32 *value;

	value = (uint32 *)(address);
	return *value;
}

void SetInfraCfg(uint32 offset, uint32 value, uint32 mask)
{
	long address = (long)((char *)AFE_INFRA_ADDRESS + offset);
	uint32 *AFE_Register = (uint32 *)address;
	uint32 val_tmp;

	pr_debug("SetInfraCfg offset=%x, value=%x, mask=%x\n",
		offset, value, mask);
	val_tmp = GetInfraCfg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
}


/* function to Set pll */
uint32 GetpllCfg(uint32 offset)
{
	long address = (long)((char *)APLL_BASE_ADDRESS + offset);
	uint32 *value;

	value = (uint32 *)(address);
	return *value;
}
EXPORT_SYMBOL(GetInfraCfg);

void SetpllCfg(uint32 offset, uint32 value, uint32 mask)
{
	long address = (long)((char *)APLL_BASE_ADDRESS + offset);
	uint32 *AFE_Register = (uint32 *)address;
	uint32 val_tmp;

	pr_debug
		("SetpllCfg offset=%x, value=%x, mask=%x\n",
		offset, value, mask);
	val_tmp = GetpllCfg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
}
EXPORT_SYMBOL(SetInfraCfg);

void Afe_Log_Print(void)
{
	AudDrv_Clk_On();
	pr_debug("+AudDrv Afe_Log_Print\n");
	pr_debug("AUDIO_TOP_CON0		   = 0x%x\n",
		Afe_Get_Reg(AUDIO_TOP_CON0));
	pr_debug("AUDIO_TOP_CON1		   = 0x%x\n",
		Afe_Get_Reg(AUDIO_TOP_CON1));
	pr_debug("AUDIO_TOP_CON2		   = 0x%x\n",
		Afe_Get_Reg(AUDIO_TOP_CON2));
	pr_debug("AUDIO_TOP_CON3		   = 0x%x\n",
		Afe_Get_Reg(AUDIO_TOP_CON3));
	pr_debug("AFE_DAC_CON0		   = 0x%x\n",
		Afe_Get_Reg(AFE_DAC_CON0));
	pr_debug("AFE_DAC_CON1		   = 0x%x\n",
		Afe_Get_Reg(AFE_DAC_CON1));
	pr_debug("AFE_I2S_CON		   = 0x%x\n",
		Afe_Get_Reg(AFE_I2S_CON));
	pr_debug("AFE_DAIBT_CON0		   = 0x%x\n",
		Afe_Get_Reg(AFE_DAIBT_CON0));
	pr_debug("AFE_CONN0			   = 0x%x\n",
		Afe_Get_Reg(AFE_CONN0));
	pr_debug("AFE_CONN1			   = 0x%x\n",
		Afe_Get_Reg(AFE_CONN1));
	pr_debug("AFE_CONN2			   = 0x%x\n",
		Afe_Get_Reg(AFE_CONN2));
	pr_debug("AFE_CONN3			   = 0x%x\n",
		Afe_Get_Reg(AFE_CONN3));
	pr_debug("AFE_CONN4			   = 0x%x\n",
		Afe_Get_Reg(AFE_CONN4));
	pr_debug("AFE_I2S_CON1		   = 0x%x\n",
		Afe_Get_Reg(AFE_I2S_CON1));
	pr_debug("AFE_I2S_CON2		   = 0x%x\n",
		Afe_Get_Reg(AFE_I2S_CON2));
	pr_debug("AFE_MRGIF_CON		   = 0x%x\n",
		Afe_Get_Reg(AFE_MRGIF_CON));
	pr_debug("AFE_DL1_BASE		   = 0x%x\n",
		Afe_Get_Reg(AFE_DL1_BASE));
	pr_debug("AFE_DL1_CUR		   = 0x%x\n",
		Afe_Get_Reg(AFE_DL1_CUR));
	pr_debug("AFE_DL1_END		   = 0x%x\n",
		Afe_Get_Reg(AFE_DL1_END));
	pr_debug("AFE_DL1_D2_BASE	   = 0x%x\n",
		Afe_Get_Reg(AFE_DL1_D2_BASE));
	pr_debug("AFE_DL1_D2_CUR		   = 0x%x\n",
		Afe_Get_Reg(AFE_DL1_D2_CUR));
	pr_debug("AFE_DL1_D2_END		   = 0x%x\n",
		Afe_Get_Reg(AFE_DL1_D2_END));
	pr_debug("AFE_VUL_D2_BASE	   = 0x%x\n",
		Afe_Get_Reg(AFE_VUL_D2_BASE));
	pr_debug("AFE_VUL_D2_END		   = 0x%x\n",
		Afe_Get_Reg(AFE_VUL_D2_END));
	pr_debug("AFE_VUL_D2_CUR		   = 0x%x\n",
		Afe_Get_Reg(AFE_VUL_D2_CUR));
	pr_debug("AFE_I2S_CON3		   = 0x%x\n",
		Afe_Get_Reg(AFE_I2S_CON3));
	pr_debug("AFE_DL2_BASE		   = 0x%x\n",
		Afe_Get_Reg(AFE_DL2_BASE));
	pr_debug("AFE_DL2_CUR		   = 0x%x\n",
		Afe_Get_Reg(AFE_DL2_CUR));
	pr_debug("AFE_DL2_END		   = 0x%x\n",
		Afe_Get_Reg(AFE_DL2_END));
	pr_debug("AFE_CONN5			   = 0x%x\n",
		Afe_Get_Reg(AFE_CONN5));
	pr_debug("AFE_CONN_24BIT		   = 0x%x\n",
		Afe_Get_Reg(AFE_CONN_24BIT));
	pr_debug("AFE_AWB_BASE		   = 0x%x\n",
		Afe_Get_Reg(AFE_AWB_BASE));
	pr_debug("AFE_AWB_END		   = 0x%x\n",
		Afe_Get_Reg(AFE_AWB_END));
	pr_debug("AFE_AWB_CUR		   = 0x%x\n",
		Afe_Get_Reg(AFE_AWB_CUR));
	pr_debug("AFE_VUL_BASE		   = 0x%x\n",
		Afe_Get_Reg(AFE_VUL_BASE));
	pr_debug("AFE_VUL_END		   = 0x%x\n",
		Afe_Get_Reg(AFE_VUL_END));
	pr_debug("AFE_VUL_CUR		   = 0x%x\n",
		Afe_Get_Reg(AFE_VUL_CUR));
	pr_debug("AFE_DAI_BASE		   = 0x%x\n",
		Afe_Get_Reg(AFE_DAI_BASE));
	pr_debug("AFE_DAI_END		   = 0x%x\n",
		Afe_Get_Reg(AFE_DAI_END));
	pr_debug("AFE_DAI_CUR		   = 0x%x\n",
		Afe_Get_Reg(AFE_DAI_CUR));
	pr_debug("AFE_CONN6			   = 0x%x\n",
		Afe_Get_Reg(AFE_CONN6));
	pr_debug("AFE_MEMIF_MSB		   = 0x%x\n",
		Afe_Get_Reg(AFE_MEMIF_MSB));
	pr_debug("AFE_MEMIF_MON0		   = 0x%x\n",
		Afe_Get_Reg(AFE_MEMIF_MON0));
	pr_debug("AFE_MEMIF_MON1		   = 0x%x\n",
		Afe_Get_Reg(AFE_MEMIF_MON1));
	pr_debug("AFE_MEMIF_MON2		   = 0x%x\n",
		Afe_Get_Reg(AFE_MEMIF_MON2));
	pr_debug("AFE_MEMIF_MON4		   = 0x%x\n",
		Afe_Get_Reg(AFE_MEMIF_MON4));
	pr_debug("AFE_ADDA_DL_SRC2_CON0  = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON0));
	pr_debug("AFE_ADDA_DL_SRC2_CON1  = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON1));
	pr_debug("AFE_ADDA_UL_SRC_CON0   = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_UL_SRC_CON0));
	pr_debug("AFE_ADDA_UL_SRC_CON1   = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_UL_SRC_CON1));
	pr_debug("AFE_ADDA_TOP_CON0	   = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_TOP_CON0));
	pr_debug("AFE_ADDA_UL_DL_CON0    = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_UL_DL_CON0));
	pr_debug("AFE_ADDA_SRC_DEBUG	   = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_SRC_DEBUG));
	pr_debug("AFE_ADDA_SRC_DEBUG_MON0= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON0));
	pr_debug("AFE_ADDA_SRC_DEBUG_MON1= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON1));
	pr_debug("AFE_ADDA_NEWIF_CFG0    = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_NEWIF_CFG0));
	pr_debug("AFE_ADDA_NEWIF_CFG1    = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_NEWIF_CFG1));
	pr_debug("AFE_SIDETONE_DEBUG	   = 0x%x\n",
		Afe_Get_Reg(AFE_SIDETONE_DEBUG));
	pr_debug("AFE_SIDETONE_MON	   = 0x%x\n",
		Afe_Get_Reg(AFE_SIDETONE_MON));
	pr_debug("AFE_SIDETONE_CON0	   = 0x%x\n",
		Afe_Get_Reg(AFE_SIDETONE_CON0));
	pr_debug("AFE_SIDETONE_COEFF	   = 0x%x\n",
		Afe_Get_Reg(AFE_SIDETONE_COEFF));
	pr_debug("AFE_SIDETONE_CON1	   = 0x%x\n",
		Afe_Get_Reg(AFE_SIDETONE_CON1));
	pr_debug("AFE_SIDETONE_GAIN	   = 0x%x\n",
		Afe_Get_Reg(AFE_SIDETONE_GAIN));
	pr_debug("AFE_SGEN_CON0		   = 0x%x\n",
		Afe_Get_Reg(AFE_SGEN_CON0));
	pr_debug("AFE_TOP_CON0		   = 0x%x\n",
		Afe_Get_Reg(AFE_TOP_CON0));
	pr_debug("AFE_ADDA_PREDIS_CON0   = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_PREDIS_CON0));
	pr_debug("AFE_ADDA_PREDIS_CON1   = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA_PREDIS_CON1));
	pr_debug("AFE_MRGIF_MON0		   = 0x%x\n",
		Afe_Get_Reg(AFE_MRGIF_MON0));
	pr_debug("AFE_MRGIF_MON1		   = 0x%x\n",
		Afe_Get_Reg(AFE_MRGIF_MON1));
	pr_debug("AFE_MRGIF_MON2		   = 0x%x\n",
		Afe_Get_Reg(AFE_MRGIF_MON2));
	pr_debug("AFE_MOD_DAI_BASE	   = 0x%x\n",
		Afe_Get_Reg(AFE_MOD_DAI_BASE));
	pr_debug("AFE_MOD_DAI_END	   = 0x%x\n",
		Afe_Get_Reg(AFE_MOD_DAI_END));
	pr_debug("AFE_MOD_DAI_CUR	   = 0x%x\n",
		Afe_Get_Reg(AFE_MOD_DAI_CUR));
	pr_debug("AFE_IRQ_MCU_CON	   = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ_MCU_CON));
	pr_debug("AFE_IRQ_MCU_STATUS	   = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ_MCU_STATUS));
	pr_debug("AFE_IRQ_MCU_CLR	   = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ_MCU_CLR));
	pr_debug("AFE_IRQ_MCU_CNT1	   = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ_MCU_CNT1));
	pr_debug("AFE_IRQ_MCU_CNT2	   = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ_MCU_CNT2));
	pr_debug("AFE_IRQ_MCU_EN		   = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ_MCU_EN));
	pr_debug("AFE_IRQ_MCU_MON2	   = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ_MCU_MON2));
	pr_debug("AFE_IRQ_CNT5		   = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ_CNT5));
	pr_debug("AFE_IRQ1_MCU_CNT_MON   = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ1_MCU_CNT_MON));
	pr_debug("AFE_IRQ2_MCU_CNT_MON   = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ2_MCU_CNT_MON));
	pr_debug("AFE_IRQ1_MCU_EN_CNT_MON= 0x%x\n",
		Afe_Get_Reg(AFE_IRQ1_MCU_EN_CNT_MON));
	pr_debug("AFE_IRQ_DEBUG		   = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ_DEBUG));
	pr_debug("AFE_MEMIF_MAXLEN	   = 0x%x\n",
		Afe_Get_Reg(AFE_MEMIF_MAXLEN));
	pr_debug("AFE_MEMIF_PBUF_SIZE    = 0x%x\n",
		Afe_Get_Reg(AFE_MEMIF_PBUF_SIZE));
	pr_debug("AFE_IRQ_MCU_CNT7	   = 0x%x\n",
		Afe_Get_Reg(AFE_IRQ_MCU_CNT7));
	pr_debug("AFE_APLL1_TUNER_CFG    = 0x%x\n",
		Afe_Get_Reg(AFE_APLL1_TUNER_CFG));
	pr_debug("AFE_APLL2_TUNER_CFG    = 0x%x\n",
		Afe_Get_Reg(AFE_APLL2_TUNER_CFG));
	pr_debug("AFE_GAIN1_CON0		   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN1_CON0));
	pr_debug("AFE_GAIN1_CON1		   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN1_CON1));
	pr_debug("AFE_GAIN1_CON2		   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN1_CON2));
	pr_debug("AFE_GAIN1_CON3		   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN1_CON3));
	pr_debug("AFE_GAIN1_CONN		   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN1_CONN));
	pr_debug("AFE_GAIN1_CUR		   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN1_CUR));
	pr_debug("AFE_GAIN2_CON0		   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN2_CON0));
	pr_debug("AFE_GAIN2_CON1		   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN2_CON1));
	pr_debug("AFE_GAIN2_CON2		   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN2_CON2));
	pr_debug("AFE_GAIN2_CON3		   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN2_CON3));
	pr_debug("AFE_GAIN2_CONN		   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN2_CONN));
	pr_debug("AFE_GAIN2_CUR		   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN2_CUR));
	pr_debug("AFE_GAIN2_CONN2	   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN2_CONN2));
	pr_debug("AFE_GAIN2_CONN3	   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN2_CONN3));
	pr_debug("AFE_GAIN1_CONN2	   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN1_CONN2));
	pr_debug("AFE_GAIN1_CONN3	   = 0x%x\n",
		Afe_Get_Reg(AFE_GAIN1_CONN3));
	pr_debug("AFE_CONN7			   = 0x%x\n",
		Afe_Get_Reg(AFE_CONN7));
	pr_debug("AFE_CONN8			   = 0x%x\n",
		Afe_Get_Reg(AFE_CONN8));
	pr_debug("AFE_CONN9			   = 0x%x\n",
		Afe_Get_Reg(AFE_CONN9));
	pr_debug("AFE_CONN10			   = 0x%x\n",
		Afe_Get_Reg(AFE_CONN10));
	pr_debug("FPGA_CFG2			   = 0x%x\n",
		Afe_Get_Reg(FPGA_CFG2));
	pr_debug("FPGA_CFG3			   = 0x%x\n",
		Afe_Get_Reg(FPGA_CFG3));
	pr_debug("FPGA_CFG0			   = 0x%x\n",
		Afe_Get_Reg(FPGA_CFG0));
	pr_debug("FPGA_CFG1			   = 0x%x\n",
		Afe_Get_Reg(FPGA_CFG1));
	pr_debug("FPGA_VER			   = 0x%x\n",
		Afe_Get_Reg(FPGA_VER));
	pr_debug("FPGA_STC			   = 0x%x\n",
		Afe_Get_Reg(FPGA_STC));
	pr_debug("AFE_ASRC_CON0		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON0));
	pr_debug("AFE_ASRC_CON1		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON1));
	pr_debug("AFE_ASRC_CON2		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON2));
	pr_debug("AFE_ASRC_CON3		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON3));
	pr_debug("AFE_ASRC_CON4		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON4));
	pr_debug("AFE_ASRC_CON5		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON5));
	pr_debug("AFE_ASRC_CON6		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON6));
	pr_debug("AFE_ASRC_CON7		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON7));
	pr_debug("AFE_ASRC_CON8		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON8));
	pr_debug("AFE_ASRC_CON9		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON9));
	pr_debug("AFE_ASRC_CON10		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON10));
	pr_debug("AFE_ASRC_CON11		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON11));
	pr_debug("PCM_INTF_CON		   = 0x%x\n",
		Afe_Get_Reg(PCM_INTF_CON));
	pr_debug("PCM_INTF_CON2		   = 0x%x\n",
		Afe_Get_Reg(PCM_INTF_CON2));
	pr_debug("PCM2_INTF_CON		   = 0x%x\n",
		Afe_Get_Reg(PCM2_INTF_CON));
	pr_debug("AFE_ASRC_CON13		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON13));
	pr_debug("AFE_ASRC_CON14		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON14));
	pr_debug("AFE_ASRC_CON15		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON15));
	pr_debug("AFE_ASRC_CON16		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON16));
	pr_debug("AFE_ASRC_CON17		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON17));
	pr_debug("AFE_ASRC_CON18		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON18));
	pr_debug("AFE_ASRC_CON19		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON19));
	pr_debug("AFE_ASRC_CON20		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON20));
	pr_debug("AFE_ASRC_CON21		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC_CON21));
	pr_debug("AFE_ASRC4_CON0		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON0));
	pr_debug("AFE_ASRC4_CON1		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON1));
	pr_debug("AFE_ASRC4_CON2		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON2));
	pr_debug("AFE_ASRC4_CON3		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON3));
	pr_debug("AFE_ASRC4_CON4		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON4));
	pr_debug("AFE_ASRC4_CON5		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON5));
	pr_debug("AFE_ASRC4_CON6		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON6));
	pr_debug("AFE_ASRC4_CON7		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON7));
	pr_debug("AFE_ASRC4_CON8		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON8));
	pr_debug("AFE_ASRC4_CON9		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON9));
	pr_debug("AFE_ASRC4_CON10	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON10));
	pr_debug("AFE_ASRC4_CON11	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON11));
	pr_debug("AFE_ASRC4_CON12	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON12));
	pr_debug("AFE_ASRC4_CON13	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON13));
	pr_debug("AFE_ASRC4_CON14	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC4_CON14));
	pr_debug("AFE_ASRC2_CON0		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON0));
	pr_debug("AFE_ASRC2_CON1		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON1));
	pr_debug("AFE_ASRC2_CON2		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON2));
	pr_debug("AFE_ASRC2_CON3		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON3));
	pr_debug("AFE_ASRC2_CON4		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON4));
	pr_debug("AFE_ASRC2_CON5		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON5));
	pr_debug("AFE_ASRC2_CON6		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON6));
	pr_debug("AFE_ASRC2_CON7		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON7));
	pr_debug("AFE_ASRC2_CON8		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON8));
	pr_debug("AFE_ASRC2_CON9		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON9));
	pr_debug("AFE_ASRC2_CON10	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON10));
	pr_debug("AFE_ASRC2_CON11	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON11));
	pr_debug("AFE_ASRC2_CON12	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON12));
	pr_debug("AFE_ASRC2_CON13	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON13));
	pr_debug("AFE_ASRC2_CON14	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC2_CON14));
	pr_debug("AFE_ASRC3_CON0		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON0));
	pr_debug("AFE_ASRC3_CON1		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON1));
	pr_debug("AFE_ASRC3_CON2		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON2));
	pr_debug("AFE_ASRC3_CON3		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON3));
	pr_debug("AFE_ASRC3_CON4		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON4));
	pr_debug("AFE_ASRC3_CON5		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON5));
	pr_debug("AFE_ASRC3_CON6		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON6));
	pr_debug("AFE_ASRC3_CON7		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON7));
	pr_debug("AFE_ASRC3_CON8		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON8));
	pr_debug("AFE_ASRC3_CON9		   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON9));
	pr_debug("AFE_ASRC3_CON10	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON10));
	pr_debug("AFE_ASRC3_CON11	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON11));
	pr_debug("AFE_ASRC3_CON12	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON12));
	pr_debug("AFE_ASRC3_CON13	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON13));
	pr_debug("AFE_ASRC3_CON14	   = 0x%x\n",
		Afe_Get_Reg(AFE_ASRC3_CON14));
	pr_debug("AFE_ADDA4_TOP_CON0	   = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_TOP_CON0));
	pr_debug("AFE_ADDA4_UL_SRC_CON0  = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_UL_SRC_CON0));
	pr_debug("AFE_ADDA4_UL_SRC_CON1  = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_UL_SRC_CON1));
	pr_debug("AFE_ADDA4_SRC_DEBUG    = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_SRC_DEBUG));
	pr_debug("AFE_ADDA4_SRC_DEBUG_MON= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_SRC_DEBUG_MON0));
	pr_debug("AFE_ADDA4_SRC_DEBUG_MON= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_SRC_DEBUG_MON1));
	pr_debug("AFE_ADDA4_NEWIF_CFG0   = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_NEWIF_CFG0));
	pr_debug("AFE_ADDA4_NEWIF_CFG1   = 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_NEWIF_CFG1));
	pr_debug("AFE_ADDA4_ULCF_CFG_02_0= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_02_01));
	pr_debug("AFE_ADDA4_ULCF_CFG_04_0= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_04_03));
	pr_debug("AFE_ADDA4_ULCF_CFG_06_0= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_06_05));
	pr_debug("AFE_ADDA4_ULCF_CFG_08_0= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_08_07));
	pr_debug("AFE_ADDA4_ULCF_CFG_10_0= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_10_09));
	pr_debug("AFE_ADDA4_ULCF_CFG_12_1= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_12_11));
	pr_debug("AFE_ADDA4_ULCF_CFG_14_1= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_14_13));
	pr_debug("AFE_ADDA4_ULCF_CFG_16_1= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_16_15));
	pr_debug("AFE_ADDA4_ULCF_CFG_18_1= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_18_17));
	pr_debug("AFE_ADDA4_ULCF_CFG_20_1= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_20_19));
	pr_debug("AFE_ADDA4_ULCF_CFG_22_2= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_22_21));
	pr_debug("AFE_ADDA4_ULCF_CFG_24_2= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_24_23));
	pr_debug("AFE_ADDA4_ULCF_CFG_26_2= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_26_25));
	pr_debug("AFE_ADDA4_ULCF_CFG_28_2= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_28_27));
	pr_debug("AFE_ADDA4_ULCF_CFG_30_2= 0x%x\n",
		Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_30_29));
	AudDrv_Clk_Off();
	pr_debug("-AudDrv Afe_Log_Print\n");
}
EXPORT_SYMBOL(Afe_Log_Print);

