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
 *   MT6797  Audio Driver Afe Register setting
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

static DEFINE_SPINLOCK(afe_set_reg_lock);


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
void *APMIXEDSYS_ADDRESS = 0;

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

	APMIXEDSYS_ADDRESS = ioremap_nocache(APMIXEDSYS_BASE, 0x1000);
}

unsigned int Get_Afe_Sram_Length(void)
{
	return AFE_INTERNAL_SRAM_SIZE;
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
	unsigned long flags = 0;

	if (CheckOffset(offset) == false)
		return;

#ifdef AUDIO_MEM_IOREMAP
	/* PRINTK_AUDDRV("Afe_Set_Reg AUDIO_MEM_IOREMAP AFE_BASE_ADDRESS = %p\n",AFE_BASE_ADDRESS); */
	address = (long)((char *)AFE_BASE_ADDRESS + offset);
#else
	address = (long)(AFE_BASE + offset);
#endif

	AFE_Register = (volatile uint32 *)address;
	/* PRINTK_AFE_REG("Afe_Set_Reg offset=%x, value=%x, mask=%x\n",offset,value,mask); */
	spin_lock_irqsave(&afe_set_reg_lock, flags);
	val_tmp = Afe_Get_Reg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
	spin_unlock_irqrestore(&afe_set_reg_lock, flags);
}
EXPORT_SYMBOL(Afe_Set_Reg);

uint32 Afe_Get_Reg(uint32 offset)
{
	volatile long address;
	volatile uint32 *value;

	if (CheckOffset(offset) == false)
		return 0xffffffff;

#ifdef AUDIO_MEM_IOREMAP
	/* PRINTK_AUDDRV("Afe_Get_Reg  AFE_BASE_ADDRESS = %p\ offset = %xn",AFE_BASE_ADDRESS,offset); */
	address = (long)((char *)AFE_BASE_ADDRESS + offset);
#else
	address = (long)(AFE_BASE + offset);
#endif

	value = (volatile uint32 *)(address);
	/* PRINTK_AFE_REG("Afe_Get_Reg offset=%x address = %x value = 0x%x\n",offset,address,*value); */
	return *value;
}
EXPORT_SYMBOL(Afe_Get_Reg);

/* function to Set Cfg */
uint32 GetClkCfg(uint32 offset)
{
	volatile long address = (long)((char *)AFE_CLK_ADDRESS + offset);
	volatile uint32 *value;

	value = (volatile uint32 *)(address);
	/* pr_debug("GetClkCfg offset=%x address = %x value = 0x%x\n", offset, address, *value); */
	return *value;
}
EXPORT_SYMBOL(GetClkCfg);

void SetClkCfg(uint32 offset, uint32 value, uint32 mask)
{
	volatile long address = (long)((char *)AFE_CLK_ADDRESS + offset);
	volatile uint32 *AFE_Register = (volatile uint32 *)address;
	volatile uint32 val_tmp;
	/* pr_debug("SetClkCfg offset=%x, value=%x, mask=%x\n",offset,value,mask); */
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
	/* pr_debug("GetInfraCfg offset=%x address = %x value = 0x%x\n", offset, address, *value); */
	return *value;
}
EXPORT_SYMBOL(GetInfraCfg);

void SetInfraCfg(uint32 offset, uint32 value, uint32 mask)
{
	volatile long address = (long)((char *)AFE_INFRA_ADDRESS + offset);
	volatile uint32 *AFE_Register = (volatile uint32 *)address;
	volatile uint32 val_tmp;
	/* pr_debug("SetInfraCfg offset=%x, value=%x, mask=%x\n",offset,value,mask); */
	val_tmp = GetInfraCfg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
}
EXPORT_SYMBOL(SetInfraCfg);


/* function to Set pll */
uint32 GetpllCfg(uint32 offset)
{
	volatile long address = (long)((char *)APLL_BASE_ADDRESS + offset);
	volatile uint32 *value;

	value = (volatile uint32 *)(address);
	/* pr_debug("GetClkCfg offset=%x address = %x value = 0x%x\n", offset, address, *value); */
	return *value;
}

void SetpllCfg(uint32 offset, uint32 value, uint32 mask)
{
	volatile long address = (long)((char *)APLL_BASE_ADDRESS + offset);
	volatile uint32 *AFE_Register = (volatile uint32 *)address;
	volatile uint32 val_tmp;
	/* pr_debug("SetpllCfg offset=%x, value=%x, mask=%x\n",offset,value,mask); */
	val_tmp = GetpllCfg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
}

/* function to access apmixed sys */
uint32 GetApmixedCfg(uint32 offset)
{
	volatile long address = (long)((char *)APMIXEDSYS_ADDRESS + offset);
	volatile uint32 *value;

	value = (volatile uint32 *)(address);
	/* pr_debug("GetClkCfg offset=%x address = %x value = 0x%x\n", offset, address, *value); */
	return *value;
}

void SetApmixedCfg(uint32 offset, uint32 value, uint32 mask)
{
	volatile long address = (long)((char *)APMIXEDSYS_ADDRESS + offset);
	volatile uint32 *AFE_Register = (volatile uint32 *)address;
	volatile uint32 val_tmp;
	/* pr_debug("SetpllCfg offset=%x, value=%x, mask=%x\n",offset,value,mask); */
	val_tmp = GetApmixedCfg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
}

