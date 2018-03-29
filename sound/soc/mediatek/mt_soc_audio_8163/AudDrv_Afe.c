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
 *   AudioAfe.h
 *
 * Project:
 * --------
 *   MT6583  Audio Driver Afe Register setting
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
void *AFE_BASE_ADDRESS = 0;
void *AFE_SRAM_ADDRESS = 0;
void *AFE_TOP_ADDRESS = 0;
void *AFE_CLK_ADDRESS = 0;
void *AFE_INFRA_ADDRESS = 0;
void *APLL_BASE_ADDRESS = 0;

void Auddrv_Reg_map(void)
{
	AFE_SRAM_ADDRESS = ioremap_nocache(AFE_INTERNAL_SRAM_PHY_BASE, AFE_INTERNAL_SRAM_SIZE);
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
	return (dma_addr_t) (AFE_INTERNAL_SRAM_PHY_BASE + SramCaptureOffSet);
}

void *Get_Afe_SramBase_Pointer()
{
	return AFE_SRAM_ADDRESS;
}

void *Get_Afe_SramCaptureBase_Pointer()
{
	char *CaptureSramPointer = (char *)(AFE_SRAM_ADDRESS) + SramCaptureOffSet;

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
	volatile long address;
	volatile uint32 *AFE_Register;
	volatile uint32 val_tmp;

	if (CheckOffset(offset) == false)
		return;
#ifdef AUDIO_MEM_IOREMAP
	PRINTK_AFE_REG("Afe_Set_Reg AUDIO_MEM_IOREMAP AFE_BASE_ADDRESS = %p\n", AFE_BASE_ADDRESS);
	address = (long)((char *)AFE_BASE_ADDRESS + offset);
#else
	address = (long)(AFE_BASE + offset);
#endif
	AFE_Register = (volatile uint32 *)address;
	PRINTK_AFE_REG("Afe_Set_Reg offset=%x, value=%x, mask=%x\n", offset, value, mask);
	val_tmp = Afe_Get_Reg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
}
EXPORT_SYMBOL(Afe_Set_Reg);

uint32 Afe_Get_Reg(uint32 offset)
{
	volatile long address;
	volatile uint32 *value;

	if (CheckOffset(offset) == false)
		return 0xffffffff;
#ifdef AUDIO_MEM_IOREMAP
	PRINTK_AFE_REG("Afe_Get_Reg AUDIO_MEM_IOREMAP AFE_BASE_ADDRESS = %p, offset = %x\n",
		      AFE_BASE_ADDRESS, offset);
	address = (long)((char *)AFE_BASE_ADDRESS + offset);
#else
	address = (long)(AFE_BASE + offset);
#endif
	value = (volatile uint32 *)(address);
	PRINTK_AFE_REG("Afe_Get_Reg offset=%x address = %x value = 0x%x\n", offset, (volatile uint32)address,
		       *value);
	return *value;
}
EXPORT_SYMBOL(Afe_Get_Reg);

/* function to Set Cfg */
uint32 GetClkCfg(uint32 offset)
{
	volatile long address = (long)((char *)AFE_CLK_ADDRESS + offset);
	volatile uint32 *value;

	value = (volatile uint32 *)(address);
	/* PRINTK_AUDDRV("GetClkCfg offset=%x address=%x value=%x\n", offset, address, *value); */
	return *value;
}
EXPORT_SYMBOL(GetClkCfg);

void SetClkCfg(uint32 offset, uint32 value, uint32 mask)
{
	volatile long address = (long)((char *)AFE_CLK_ADDRESS + offset);
	volatile uint32 *AFE_Register = (volatile uint32 *)address;
	volatile uint32 val_tmp;

	PRINTK_AUDDRV("SetClkCfg offset=%x, value=%x, mask=%x\n", offset, value, mask);
	val_tmp = GetClkCfg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
}
EXPORT_SYMBOL(SetClkCfg);

/* function to Set Cfg */
uint32 GetInfraCfg(uint32 offset)
{
	volatile long address = (long)((char *)AFE_INFRA_ADDRESS + offset);
	volatile uint32 *value;

	value = (volatile uint32 *)(address);
	/* PRINTK_AUDDRV("GetInfraCfg offset=%x address=%x value=%x\n", offset, address, *value); */
	return *value;
}

void SetInfraCfg(uint32 offset, uint32 value, uint32 mask)
{
	volatile long address = (long)((char *)AFE_INFRA_ADDRESS + offset);
	volatile uint32 *AFE_Register = (volatile uint32 *)address;
	volatile uint32 val_tmp;

	PRINTK_AUDDRV("SetInfraCfg offset=%x, value=%x, mask=%x\n", offset, value, mask);
	val_tmp = GetInfraCfg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
}


/* function to Set pll */
uint32 GetpllCfg(uint32 offset)
{
	volatile long address = (long)((char *)APLL_BASE_ADDRESS + offset);
	volatile uint32 *value;

	value = (volatile uint32 *)(address);
	/* PRINTK_AUDDRV("GetClkCfg offset=%x address=%x value=%x\n", offset, address, *value); */
	return *value;
}
EXPORT_SYMBOL(GetInfraCfg);

void SetpllCfg(uint32 offset, uint32 value, uint32 mask)
{
	volatile long address = (long)((char *)APLL_BASE_ADDRESS + offset);
	volatile uint32 *AFE_Register = (volatile uint32 *)address;
	volatile uint32 val_tmp;

	PRINTK_AUDDRV("SetpllCfg offset=%x, value=%x, mask=%x\n", offset, value, mask);
	val_tmp = GetpllCfg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
}
EXPORT_SYMBOL(SetInfraCfg);

void Afe_Log_Print(void)
{
	AudDrv_Clk_On();
	PRINTK_AFE_REG("+AudDrv Afe_Log_Print\n");
	PRINTK_AFE_REG("AUDIO_TOP_CON0		   = 0x%x\n", Afe_Get_Reg(AUDIO_TOP_CON0));
	PRINTK_AFE_REG("AUDIO_TOP_CON1		   = 0x%x\n", Afe_Get_Reg(AUDIO_TOP_CON1));
	PRINTK_AFE_REG("AUDIO_TOP_CON2		   = 0x%x\n", Afe_Get_Reg(AUDIO_TOP_CON2));
	PRINTK_AFE_REG("AUDIO_TOP_CON3		   = 0x%x\n", Afe_Get_Reg(AUDIO_TOP_CON3));
	PRINTK_AFE_REG("AFE_DAC_CON0		   = 0x%x\n", Afe_Get_Reg(AFE_DAC_CON0));
	PRINTK_AFE_REG("AFE_DAC_CON1		   = 0x%x\n", Afe_Get_Reg(AFE_DAC_CON1));
	PRINTK_AFE_REG("AFE_I2S_CON		   = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON));
	PRINTK_AFE_REG("AFE_DAIBT_CON0		   = 0x%x\n", Afe_Get_Reg(AFE_DAIBT_CON0));
	PRINTK_AFE_REG("AFE_CONN0			   = 0x%x\n", Afe_Get_Reg(AFE_CONN0));
	PRINTK_AFE_REG("AFE_CONN1			   = 0x%x\n", Afe_Get_Reg(AFE_CONN1));
	PRINTK_AFE_REG("AFE_CONN2			   = 0x%x\n", Afe_Get_Reg(AFE_CONN2));
	PRINTK_AFE_REG("AFE_CONN3			   = 0x%x\n", Afe_Get_Reg(AFE_CONN3));
	PRINTK_AFE_REG("AFE_CONN4			   = 0x%x\n", Afe_Get_Reg(AFE_CONN4));
	PRINTK_AFE_REG("AFE_I2S_CON1		   = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON1));
	PRINTK_AFE_REG("AFE_I2S_CON2		   = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON2));
	PRINTK_AFE_REG("AFE_MRGIF_CON		   = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_CON));
	PRINTK_AFE_REG("AFE_DL1_BASE		   = 0x%x\n", Afe_Get_Reg(AFE_DL1_BASE));
	PRINTK_AFE_REG("AFE_DL1_CUR		   = 0x%x\n", Afe_Get_Reg(AFE_DL1_CUR));
	PRINTK_AFE_REG("AFE_DL1_END		   = 0x%x\n", Afe_Get_Reg(AFE_DL1_END));
	PRINTK_AFE_REG("AFE_DL1_D2_BASE	   = 0x%x\n", Afe_Get_Reg(AFE_DL1_D2_BASE));
	PRINTK_AFE_REG("AFE_DL1_D2_CUR		   = 0x%x\n", Afe_Get_Reg(AFE_DL1_D2_CUR));
	PRINTK_AFE_REG("AFE_DL1_D2_END		   = 0x%x\n", Afe_Get_Reg(AFE_DL1_D2_END));
	PRINTK_AFE_REG("AFE_VUL_D2_BASE	   = 0x%x\n", Afe_Get_Reg(AFE_VUL_D2_BASE));
	PRINTK_AFE_REG("AFE_VUL_D2_END		   = 0x%x\n", Afe_Get_Reg(AFE_VUL_D2_END));
	PRINTK_AFE_REG("AFE_VUL_D2_CUR		   = 0x%x\n", Afe_Get_Reg(AFE_VUL_D2_CUR));
	PRINTK_AFE_REG("AFE_I2S_CON3		   = 0x%x\n", Afe_Get_Reg(AFE_I2S_CON3));
	PRINTK_AFE_REG("AFE_DL2_BASE		   = 0x%x\n", Afe_Get_Reg(AFE_DL2_BASE));
	PRINTK_AFE_REG("AFE_DL2_CUR		   = 0x%x\n", Afe_Get_Reg(AFE_DL2_CUR));
	PRINTK_AFE_REG("AFE_DL2_END		   = 0x%x\n", Afe_Get_Reg(AFE_DL2_END));
	PRINTK_AFE_REG("AFE_CONN5			   = 0x%x\n", Afe_Get_Reg(AFE_CONN5));
	PRINTK_AFE_REG("AFE_CONN_24BIT		   = 0x%x\n", Afe_Get_Reg(AFE_CONN_24BIT));
	PRINTK_AFE_REG("AFE_AWB_BASE		   = 0x%x\n", Afe_Get_Reg(AFE_AWB_BASE));
	PRINTK_AFE_REG("AFE_AWB_END		   = 0x%x\n", Afe_Get_Reg(AFE_AWB_END));
	PRINTK_AFE_REG("AFE_AWB_CUR		   = 0x%x\n", Afe_Get_Reg(AFE_AWB_CUR));
	PRINTK_AFE_REG("AFE_VUL_BASE		   = 0x%x\n", Afe_Get_Reg(AFE_VUL_BASE));
	PRINTK_AFE_REG("AFE_VUL_END		   = 0x%x\n", Afe_Get_Reg(AFE_VUL_END));
	PRINTK_AFE_REG("AFE_VUL_CUR		   = 0x%x\n", Afe_Get_Reg(AFE_VUL_CUR));
	PRINTK_AFE_REG("AFE_DAI_BASE		   = 0x%x\n", Afe_Get_Reg(AFE_DAI_BASE));
	PRINTK_AFE_REG("AFE_DAI_END		   = 0x%x\n", Afe_Get_Reg(AFE_DAI_END));
	PRINTK_AFE_REG("AFE_DAI_CUR		   = 0x%x\n", Afe_Get_Reg(AFE_DAI_CUR));
	PRINTK_AFE_REG("AFE_CONN6			   = 0x%x\n", Afe_Get_Reg(AFE_CONN6));
	PRINTK_AFE_REG("AFE_MEMIF_MSB		   = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MSB));
	PRINTK_AFE_REG("AFE_MEMIF_MON0		   = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON0));
	PRINTK_AFE_REG("AFE_MEMIF_MON1		   = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON1));
	PRINTK_AFE_REG("AFE_MEMIF_MON2		   = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON2));
	PRINTK_AFE_REG("AFE_MEMIF_MON4		   = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MON4));
	PRINTK_AFE_REG("AFE_ADDA_DL_SRC2_CON0  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON0));
	PRINTK_AFE_REG("AFE_ADDA_DL_SRC2_CON1  = 0x%x\n", Afe_Get_Reg(AFE_ADDA_DL_SRC2_CON1));
	PRINTK_AFE_REG("AFE_ADDA_UL_SRC_CON0   = 0x%x\n", Afe_Get_Reg(AFE_ADDA_UL_SRC_CON0));
	PRINTK_AFE_REG("AFE_ADDA_UL_SRC_CON1   = 0x%x\n", Afe_Get_Reg(AFE_ADDA_UL_SRC_CON1));
	PRINTK_AFE_REG("AFE_ADDA_TOP_CON0	   = 0x%x\n", Afe_Get_Reg(AFE_ADDA_TOP_CON0));
	PRINTK_AFE_REG("AFE_ADDA_UL_DL_CON0    = 0x%x\n", Afe_Get_Reg(AFE_ADDA_UL_DL_CON0));
	PRINTK_AFE_REG("AFE_ADDA_SRC_DEBUG	   = 0x%x\n", Afe_Get_Reg(AFE_ADDA_SRC_DEBUG));
	PRINTK_AFE_REG("AFE_ADDA_SRC_DEBUG_MON0= 0x%x\n", Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON0));
	PRINTK_AFE_REG("AFE_ADDA_SRC_DEBUG_MON1= 0x%x\n", Afe_Get_Reg(AFE_ADDA_SRC_DEBUG_MON1));
	PRINTK_AFE_REG("AFE_ADDA_NEWIF_CFG0    = 0x%x\n", Afe_Get_Reg(AFE_ADDA_NEWIF_CFG0));
	PRINTK_AFE_REG("AFE_ADDA_NEWIF_CFG1    = 0x%x\n", Afe_Get_Reg(AFE_ADDA_NEWIF_CFG1));
	PRINTK_AFE_REG("AFE_SIDETONE_DEBUG	   = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_DEBUG));
	PRINTK_AFE_REG("AFE_SIDETONE_MON	   = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_MON));
	PRINTK_AFE_REG("AFE_SIDETONE_CON0	   = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_CON0));
	PRINTK_AFE_REG("AFE_SIDETONE_COEFF	   = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_COEFF));
	PRINTK_AFE_REG("AFE_SIDETONE_CON1	   = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_CON1));
	PRINTK_AFE_REG("AFE_SIDETONE_GAIN	   = 0x%x\n", Afe_Get_Reg(AFE_SIDETONE_GAIN));
	PRINTK_AFE_REG("AFE_SGEN_CON0		   = 0x%x\n", Afe_Get_Reg(AFE_SGEN_CON0));
	PRINTK_AFE_REG("AFE_TOP_CON0		   = 0x%x\n", Afe_Get_Reg(AFE_TOP_CON0));
	PRINTK_AFE_REG("AFE_ADDA_PREDIS_CON0   = 0x%x\n", Afe_Get_Reg(AFE_ADDA_PREDIS_CON0));
	PRINTK_AFE_REG("AFE_ADDA_PREDIS_CON1   = 0x%x\n", Afe_Get_Reg(AFE_ADDA_PREDIS_CON1));
	PRINTK_AFE_REG("AFE_MRGIF_MON0		   = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_MON0));
	PRINTK_AFE_REG("AFE_MRGIF_MON1		   = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_MON1));
	PRINTK_AFE_REG("AFE_MRGIF_MON2		   = 0x%x\n", Afe_Get_Reg(AFE_MRGIF_MON2));
	PRINTK_AFE_REG("AFE_MOD_DAI_BASE	   = 0x%x\n", Afe_Get_Reg(AFE_MOD_DAI_BASE));
	PRINTK_AFE_REG("AFE_MOD_DAI_END	   = 0x%x\n", Afe_Get_Reg(AFE_MOD_DAI_END));
	PRINTK_AFE_REG("AFE_MOD_DAI_CUR	   = 0x%x\n", Afe_Get_Reg(AFE_MOD_DAI_CUR));
	PRINTK_AFE_REG("AFE_IRQ_MCU_CON	   = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CON));
	PRINTK_AFE_REG("AFE_IRQ_MCU_STATUS	   = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_STATUS));
	PRINTK_AFE_REG("AFE_IRQ_MCU_CLR	   = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CLR));
	PRINTK_AFE_REG("AFE_IRQ_MCU_CNT1	   = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CNT1));
	PRINTK_AFE_REG("AFE_IRQ_MCU_CNT2	   = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CNT2));
	PRINTK_AFE_REG("AFE_IRQ_MCU_EN		   = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_EN));
	PRINTK_AFE_REG("AFE_IRQ_MCU_MON2	   = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_MON2));
	PRINTK_AFE_REG("AFE_IRQ_CNT5		   = 0x%x\n", Afe_Get_Reg(AFE_IRQ_CNT5));
	PRINTK_AFE_REG("AFE_IRQ1_MCU_CNT_MON   = 0x%x\n", Afe_Get_Reg(AFE_IRQ1_MCU_CNT_MON));
	PRINTK_AFE_REG("AFE_IRQ2_MCU_CNT_MON   = 0x%x\n", Afe_Get_Reg(AFE_IRQ2_MCU_CNT_MON));
	PRINTK_AFE_REG("AFE_IRQ1_MCU_EN_CNT_MON= 0x%x\n", Afe_Get_Reg(AFE_IRQ1_MCU_EN_CNT_MON));
	PRINTK_AFE_REG("AFE_IRQ_DEBUG		   = 0x%x\n", Afe_Get_Reg(AFE_IRQ_DEBUG));
	PRINTK_AFE_REG("AFE_MEMIF_MAXLEN	   = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_MAXLEN));
	PRINTK_AFE_REG("AFE_MEMIF_PBUF_SIZE    = 0x%x\n", Afe_Get_Reg(AFE_MEMIF_PBUF_SIZE));
	PRINTK_AFE_REG("AFE_IRQ_MCU_CNT7	   = 0x%x\n", Afe_Get_Reg(AFE_IRQ_MCU_CNT7));
	PRINTK_AFE_REG("AFE_APLL1_TUNER_CFG    = 0x%x\n", Afe_Get_Reg(AFE_APLL1_TUNER_CFG));
	PRINTK_AFE_REG("AFE_APLL2_TUNER_CFG    = 0x%x\n", Afe_Get_Reg(AFE_APLL2_TUNER_CFG));
	PRINTK_AFE_REG("AFE_GAIN1_CON0		   = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CON0));
	PRINTK_AFE_REG("AFE_GAIN1_CON1		   = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CON1));
	PRINTK_AFE_REG("AFE_GAIN1_CON2		   = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CON2));
	PRINTK_AFE_REG("AFE_GAIN1_CON3		   = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CON3));
	PRINTK_AFE_REG("AFE_GAIN1_CONN		   = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CONN));
	PRINTK_AFE_REG("AFE_GAIN1_CUR		   = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CUR));
	PRINTK_AFE_REG("AFE_GAIN2_CON0		   = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CON0));
	PRINTK_AFE_REG("AFE_GAIN2_CON1		   = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CON1));
	PRINTK_AFE_REG("AFE_GAIN2_CON2		   = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CON2));
	PRINTK_AFE_REG("AFE_GAIN2_CON3		   = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CON3));
	PRINTK_AFE_REG("AFE_GAIN2_CONN		   = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CONN));
	PRINTK_AFE_REG("AFE_GAIN2_CUR		   = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CUR));
	PRINTK_AFE_REG("AFE_GAIN2_CONN2	   = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CONN2));
	PRINTK_AFE_REG("AFE_GAIN2_CONN3	   = 0x%x\n", Afe_Get_Reg(AFE_GAIN2_CONN3));
	PRINTK_AFE_REG("AFE_GAIN1_CONN2	   = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CONN2));
	PRINTK_AFE_REG("AFE_GAIN1_CONN3	   = 0x%x\n", Afe_Get_Reg(AFE_GAIN1_CONN3));
	PRINTK_AFE_REG("AFE_CONN7			   = 0x%x\n", Afe_Get_Reg(AFE_CONN7));
	PRINTK_AFE_REG("AFE_CONN8			   = 0x%x\n", Afe_Get_Reg(AFE_CONN8));
	PRINTK_AFE_REG("AFE_CONN9			   = 0x%x\n", Afe_Get_Reg(AFE_CONN9));
	PRINTK_AFE_REG("AFE_CONN10			   = 0x%x\n", Afe_Get_Reg(AFE_CONN10));
	PRINTK_AFE_REG("FPGA_CFG2			   = 0x%x\n", Afe_Get_Reg(FPGA_CFG2));
	PRINTK_AFE_REG("FPGA_CFG3			   = 0x%x\n", Afe_Get_Reg(FPGA_CFG3));
	PRINTK_AFE_REG("FPGA_CFG0			   = 0x%x\n", Afe_Get_Reg(FPGA_CFG0));
	PRINTK_AFE_REG("FPGA_CFG1			   = 0x%x\n", Afe_Get_Reg(FPGA_CFG1));
	PRINTK_AFE_REG("FPGA_VER			   = 0x%x\n", Afe_Get_Reg(FPGA_VER));
	PRINTK_AFE_REG("FPGA_STC			   = 0x%x\n", Afe_Get_Reg(FPGA_STC));
	PRINTK_AFE_REG("AFE_ASRC_CON0		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON0));
	PRINTK_AFE_REG("AFE_ASRC_CON1		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON1));
	PRINTK_AFE_REG("AFE_ASRC_CON2		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON2));
	PRINTK_AFE_REG("AFE_ASRC_CON3		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON3));
	PRINTK_AFE_REG("AFE_ASRC_CON4		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON4));
	PRINTK_AFE_REG("AFE_ASRC_CON5		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON5));
	PRINTK_AFE_REG("AFE_ASRC_CON6		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON6));
	PRINTK_AFE_REG("AFE_ASRC_CON7		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON7));
	PRINTK_AFE_REG("AFE_ASRC_CON8		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON8));
	PRINTK_AFE_REG("AFE_ASRC_CON9		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON9));
	PRINTK_AFE_REG("AFE_ASRC_CON10		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON10));
	PRINTK_AFE_REG("AFE_ASRC_CON11		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON11));
	PRINTK_AFE_REG("PCM_INTF_CON		   = 0x%x\n", Afe_Get_Reg(PCM_INTF_CON));
	PRINTK_AFE_REG("PCM_INTF_CON2		   = 0x%x\n", Afe_Get_Reg(PCM_INTF_CON2));
	PRINTK_AFE_REG("PCM2_INTF_CON		   = 0x%x\n", Afe_Get_Reg(PCM2_INTF_CON));
	PRINTK_AFE_REG("AFE_ASRC_CON13		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON13));
	PRINTK_AFE_REG("AFE_ASRC_CON14		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON14));
	PRINTK_AFE_REG("AFE_ASRC_CON15		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON15));
	PRINTK_AFE_REG("AFE_ASRC_CON16		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON16));
	PRINTK_AFE_REG("AFE_ASRC_CON17		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON17));
	PRINTK_AFE_REG("AFE_ASRC_CON18		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON18));
	PRINTK_AFE_REG("AFE_ASRC_CON19		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON19));
	PRINTK_AFE_REG("AFE_ASRC_CON20		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON20));
	PRINTK_AFE_REG("AFE_ASRC_CON21		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC_CON21));
	PRINTK_AFE_REG("AFE_ASRC4_CON0		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON0));
	PRINTK_AFE_REG("AFE_ASRC4_CON1		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON1));
	PRINTK_AFE_REG("AFE_ASRC4_CON2		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON2));
	PRINTK_AFE_REG("AFE_ASRC4_CON3		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON3));
	PRINTK_AFE_REG("AFE_ASRC4_CON4		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON4));
	PRINTK_AFE_REG("AFE_ASRC4_CON5		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON5));
	PRINTK_AFE_REG("AFE_ASRC4_CON6		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON6));
	PRINTK_AFE_REG("AFE_ASRC4_CON7		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON7));
	PRINTK_AFE_REG("AFE_ASRC4_CON8		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON8));
	PRINTK_AFE_REG("AFE_ASRC4_CON9		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON9));
	PRINTK_AFE_REG("AFE_ASRC4_CON10	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON10));
	PRINTK_AFE_REG("AFE_ASRC4_CON11	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON11));
	PRINTK_AFE_REG("AFE_ASRC4_CON12	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON12));
	PRINTK_AFE_REG("AFE_ASRC4_CON13	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON13));
	PRINTK_AFE_REG("AFE_ASRC4_CON14	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC4_CON14));
	PRINTK_AFE_REG("AFE_ASRC2_CON0		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON0));
	PRINTK_AFE_REG("AFE_ASRC2_CON1		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON1));
	PRINTK_AFE_REG("AFE_ASRC2_CON2		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON2));
	PRINTK_AFE_REG("AFE_ASRC2_CON3		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON3));
	PRINTK_AFE_REG("AFE_ASRC2_CON4		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON4));
	PRINTK_AFE_REG("AFE_ASRC2_CON5		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON5));
	PRINTK_AFE_REG("AFE_ASRC2_CON6		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON6));
	PRINTK_AFE_REG("AFE_ASRC2_CON7		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON7));
	PRINTK_AFE_REG("AFE_ASRC2_CON8		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON8));
	PRINTK_AFE_REG("AFE_ASRC2_CON9		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON9));
	PRINTK_AFE_REG("AFE_ASRC2_CON10	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON10));
	PRINTK_AFE_REG("AFE_ASRC2_CON11	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON11));
	PRINTK_AFE_REG("AFE_ASRC2_CON12	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON12));
	PRINTK_AFE_REG("AFE_ASRC2_CON13	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON13));
	PRINTK_AFE_REG("AFE_ASRC2_CON14	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC2_CON14));
	PRINTK_AFE_REG("AFE_ASRC3_CON0		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON0));
	PRINTK_AFE_REG("AFE_ASRC3_CON1		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON1));
	PRINTK_AFE_REG("AFE_ASRC3_CON2		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON2));
	PRINTK_AFE_REG("AFE_ASRC3_CON3		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON3));
	PRINTK_AFE_REG("AFE_ASRC3_CON4		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON4));
	PRINTK_AFE_REG("AFE_ASRC3_CON5		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON5));
	PRINTK_AFE_REG("AFE_ASRC3_CON6		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON6));
	PRINTK_AFE_REG("AFE_ASRC3_CON7		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON7));
	PRINTK_AFE_REG("AFE_ASRC3_CON8		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON8));
	PRINTK_AFE_REG("AFE_ASRC3_CON9		   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON9));
	PRINTK_AFE_REG("AFE_ASRC3_CON10	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON10));
	PRINTK_AFE_REG("AFE_ASRC3_CON11	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON11));
	PRINTK_AFE_REG("AFE_ASRC3_CON12	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON12));
	PRINTK_AFE_REG("AFE_ASRC3_CON13	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON13));
	PRINTK_AFE_REG("AFE_ASRC3_CON14	   = 0x%x\n", Afe_Get_Reg(AFE_ASRC3_CON14));
	PRINTK_AFE_REG("AFE_ADDA4_TOP_CON0	   = 0x%x\n", Afe_Get_Reg(AFE_ADDA4_TOP_CON0));
	PRINTK_AFE_REG("AFE_ADDA4_UL_SRC_CON0  = 0x%x\n", Afe_Get_Reg(AFE_ADDA4_UL_SRC_CON0));
	PRINTK_AFE_REG("AFE_ADDA4_UL_SRC_CON1  = 0x%x\n", Afe_Get_Reg(AFE_ADDA4_UL_SRC_CON1));
	PRINTK_AFE_REG("AFE_ADDA4_SRC_DEBUG    = 0x%x\n", Afe_Get_Reg(AFE_ADDA4_SRC_DEBUG));
	PRINTK_AFE_REG("AFE_ADDA4_SRC_DEBUG_MON= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_SRC_DEBUG_MON0));
	PRINTK_AFE_REG("AFE_ADDA4_SRC_DEBUG_MON= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_SRC_DEBUG_MON1));
	PRINTK_AFE_REG("AFE_ADDA4_NEWIF_CFG0   = 0x%x\n", Afe_Get_Reg(AFE_ADDA4_NEWIF_CFG0));
	PRINTK_AFE_REG("AFE_ADDA4_NEWIF_CFG1   = 0x%x\n", Afe_Get_Reg(AFE_ADDA4_NEWIF_CFG1));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_02_0= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_02_01));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_04_0= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_04_03));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_06_0= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_06_05));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_08_0= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_08_07));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_10_0= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_10_09));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_12_1= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_12_11));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_14_1= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_14_13));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_16_1= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_16_15));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_18_1= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_18_17));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_20_1= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_20_19));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_22_2= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_22_21));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_24_2= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_24_23));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_26_2= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_26_25));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_28_2= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_28_27));
	PRINTK_AFE_REG("AFE_ADDA4_ULCF_CFG_30_2= 0x%x\n", Afe_Get_Reg(AFE_ADDA4_ULCF_CFG_30_29));
	AudDrv_Clk_Off();
	pr_debug("-AudDrv Afe_Log_Print\n");
}
EXPORT_SYMBOL(Afe_Log_Print);

