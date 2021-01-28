// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   mtk-auddrv-afe.c
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
 *----------------------------------------------------------------------------
 *
 *
 *****************************************************************************
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "mtk-auddrv-common.h"
#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/types.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

/*****************************************************************************
 *                         D A T A   T Y P E S
 *****************************************************************************/

/*****************************************************************************
 *                         FUNCTION IMPLEMENTATION
 *****************************************************************************/

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
#ifndef CONFIG_FPGA_EARLY_PORTING
static DEFINE_SPINLOCK(clksys_set_reg_lock);
#endif
static const unsigned int SramCaptureOffSet = (16 * 1024);

static const struct regmap_config mtk_afe_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = AFE_MAXLENGTH,
	.cache_type = REGCACHE_NONE,
	.fast_io = true,
};

/* address for ioremap audio hardware register */
void *AFE_BASE_ADDRESS;
void *AFE_SRAM_ADDRESS;
void *AFE_TOP_ADDRESS;
void *APMIXEDSYS_ADDRESS;
void *CLKSYS_ADDRESS;

struct regmap *pregmap;

int Auddrv_Reg_map(struct device *pdev)
{
	int ret = 0;
#ifdef CONFIG_OF
	struct device_node *audio_sram_node = NULL;

	audio_sram_node =
		of_find_compatible_node(NULL, NULL, "mediatek,audio_sram");

	pr_debug("%s\n", __func__);

	if (!pdev->of_node)
		pr_warn("%s invalid of_node\n", __func__);

	if (audio_sram_node == NULL)
		pr_warn("%s invalid audio_sram_node\n", __func__);

	/* mapping AFE reg*/
	AFE_BASE_ADDRESS = of_iomap(pdev->of_node, 0);
	if (AFE_BASE_ADDRESS == NULL) {
		pr_err("AFE_BASE_ADDRESS=0x%p\n", AFE_BASE_ADDRESS);
		return -ENODEV;
	}

	AFE_SRAM_ADDRESS = of_iomap(audio_sram_node, 0);
	if (AFE_SRAM_ADDRESS == NULL) {
		pr_err("AFE_SRAM_ADDRESS=0x%p\n", AFE_SRAM_ADDRESS);
		return -ENODEV;
	}

	pregmap = devm_regmap_init_mmio(pdev, AFE_BASE_ADDRESS,
					&mtk_afe_regmap_config);
	if (IS_ERR(pregmap)) {
		pr_err("devm_regmap_init_mmio error\n");
		return -ENODEV;
	}
#else
	AFE_BASE_ADDRESS = ioremap_nocache(AUDIO_HW_PHYSICAL_BASE, 0x1000);
	AFE_SRAM_ADDRESS = ioremap_nocache(AFE_INTERNAL_SRAM_PHY_BASE,
					   AFE_INTERNAL_SRAM_SIZE);
#endif

	/* temp for hardawre code  set 0x1000629c = 0xd */
	AFE_TOP_ADDRESS = ioremap_nocache(AUDIO_POWER_TOP, 0x1000);
	APMIXEDSYS_ADDRESS = ioremap_nocache(APMIXEDSYS_BASE, 0x1000);
	CLKSYS_ADDRESS = ioremap_nocache(AUDIO_CLKCFG_PHYSICAL_BASE, 0x1000);

	return ret;
}

unsigned int Get_Afe_Sram_Length(void)
{
	return AFE_INTERNAL_SRAM_SIZE;
}

dma_addr_t Get_Afe_Sram_Phys_Addr(void)
{
	return (dma_addr_t)AFE_INTERNAL_SRAM_PHY_BASE;
}

dma_addr_t Get_Afe_Sram_Capture_Phys_Addr(void)
{
	return (dma_addr_t)(AFE_INTERNAL_SRAM_PHY_BASE + SramCaptureOffSet);
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

/* function to access apmixed sys */
unsigned int GetApmixedCfg(unsigned int offset)
{
	long address = (long)((char *)APMIXEDSYS_ADDRESS + offset);
	unsigned int *value;

	value = (unsigned int *)(address);

	return *value;
}

void SetApmixedCfg(unsigned int offset, unsigned int value, unsigned int mask)
{
	long address = (long)((char *)APMIXEDSYS_ADDRESS + offset);
	unsigned int *AFE_Register = (unsigned int *)address;
	unsigned int val_tmp;

	val_tmp = GetApmixedCfg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
}

/* function to access clksys */
unsigned int clksys_get_reg(unsigned int offset)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	long address = (long)((char *)CLKSYS_ADDRESS + offset);
	unsigned int *value;

	if (CLKSYS_ADDRESS == NULL) {
		pr_warn("%s(), CLKSYS_ADDRESS is null\n", __func__);
		return 0;
	}

	value = (unsigned int *)(address);
#if defined(AUD_DEBUG_LOG)
	pr_debug("%s(), offset = %x, address = %lx, value = 0x%x\n", __func__,
	       offset, address, *value);
#endif
	return *value;
#else
	return 0;
#endif
}

void clksys_set_reg(unsigned int offset, unsigned int value, unsigned int mask)
{
#ifndef CONFIG_FPGA_EARLY_PORTING
	long address = (long)((char *)CLKSYS_ADDRESS + offset);
	unsigned int *val_addr = (unsigned int *)address;
	unsigned int val_tmp;
	unsigned long flags = 0;

	if (CLKSYS_ADDRESS == NULL) {
		pr_warn("%s(), CLKSYS_ADDRESS is null\n", __func__);
		return;
	}
#if defined(AUD_DEBUG_LOG)
	pr_debug("%s(), offset = %x, value = %x, mask = %x\n", __func__, offset,
	       value, mask);
#endif
	spin_lock_irqsave(&clksys_set_reg_lock, flags);
	val_tmp = clksys_get_reg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, val_addr);
	spin_unlock_irqrestore(&clksys_set_reg_lock, flags);
#endif
}

void Afe_Set_Reg(unsigned int offset, unsigned int value, unsigned int mask)
{
	int ret = 0;

	ret = regmap_update_bits(pregmap, offset, mask, value);
	if (ret) {
		pr_warn("%s ret = %d offset = 0x%x value = 0x%x mask = 0x%x\n",
		       __func__, ret, offset, value, mask);
	}
}
EXPORT_SYMBOL(Afe_Set_Reg);

unsigned int Afe_Get_Reg(unsigned int offset)
{
	unsigned int value = 0;
	int ret;

	ret = regmap_read(pregmap, offset, &value);
	if (ret)
		pr_warn("%s ret = %d value = 0x%x mask = 0x%x\n", __func__, ret,
		       offset, value);
	return value;
}
EXPORT_SYMBOL(Afe_Get_Reg);
