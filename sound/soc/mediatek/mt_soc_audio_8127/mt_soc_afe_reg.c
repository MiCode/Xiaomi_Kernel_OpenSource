/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/

/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#include "mt_soc_afe_reg.h"
#include "mt_soc_afe_clk.h"
#include "mt_soc_afe_control.h"
#include <sync_write.h>
#include <linux/of_address.h>
#include <linux/device.h>

/* address for ioremap audio hardware register */
static void *afe_base_address;
static void *afe_sram_address;
static void *spm_base_address;
static void *topckgen_base_address;
static void *apmixedsys_base_address;
static phys_addr_t afe_sram_phy_address = AFE_INTERNAL_SRAM_PHY_BASE;


int mt_afe_reg_remap(void *dev)
{
#ifdef AUDIO_IOREMAP_FROM_DT
	int ret = 0;
	struct device *pdev = dev;
	struct resource res;
	struct device_node *node;

	pr_debug("%s ", __func__);
	/* AFE register base */
	ret = of_address_to_resource(pdev->of_node, 0, &res);
	if (ret) {
		pr_err("%s of_address_to_resource#0 fail %d\n", __func__, ret);
		goto exit;
	}

	afe_base_address = ioremap_nocache(res.start, resource_size(&res));
	if (!afe_base_address) {
		pr_err("%s ioremap_nocache#0 addr:0x%llx size:0x%llx fail\n",
		       __func__, (unsigned long long)res.start,
		       (unsigned long long)resource_size(&res));
		ret = -ENXIO;
		goto exit;
	}

	/* audio SRAM base */
	ret = of_address_to_resource(pdev->of_node, 1, &res);
	if (ret) {
		pr_err("%s of_address_to_resource#1 fail %d\n", __func__, ret);
		goto exit;
	}

	afe_sram_address = ioremap_nocache(res.start, resource_size(&res));
	if (!afe_sram_address) {
		pr_err("%s ioremap_nocache#1 addr:0x%llx size:0x%llx fail\n",
		       __func__, (unsigned long long)res.start,
		       (unsigned long long)resource_size(&res));
		ret = -ENXIO;
		goto exit;
	}
	afe_sram_phy_address = res.start;

	/* TOPCKGEN register base */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8127-topckgen");
	if (!node) {
		pr_warn("%s of_find_compatible_node(mediatek,mt8127-topckgen) fail\n", __func__);
		topckgen_base_address = ioremap_nocache(AUDIO_CLKCFG_PHYSICAL_BASE, 0x1000);
	} else {
		topckgen_base_address = of_iomap(node, 0);
	}

	if (!topckgen_base_address) {
		pr_err("%s ioremap topckgen_base_address fail\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	/* SPM register base */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8127-scpsys");
	if (!node) {
		pr_warn("%s of_find_compatible_node(mediatek,mt8127-scpsys) fail\n", __func__);
		spm_base_address = ioremap_nocache(SPM_BASE, 0x1000);
	} else {
		spm_base_address = of_iomap(node, 0);
	}

	if (!spm_base_address) {
		pr_err("%s ioremap spm_base_address fail\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	/* APMIXEDSYS register base */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8127-apmixedsys");
	if (!node) {
		pr_warn("%s of_find_compatible_node(mediatek,mt8127-apmixedsys) fail\n", __func__);
		apmixedsys_base_address = ioremap_nocache(APMIXEDSYS_BASE, 0x1000);
	} else {
		apmixedsys_base_address = of_iomap(node, 0);
	}

	if (!apmixedsys_base_address) {
		pr_err("%s ioremap apmixedsys_base_address fail\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

 exit:
	if (ret)
		mt_afe_reg_unmap();

	return ret;
#else
	afe_sram_address = ioremap_nocache(AFE_INTERNAL_SRAM_PHY_BASE, AFE_INTERNAL_SRAM_SIZE);
	afe_base_address = ioremap_nocache(AUDIO_HW_PHYSICAL_BASE, 0x1000);
	spm_base_address = ioremap_nocache(SPM_BASE, 0x1000);
	topckgen_base_address = ioremap_nocache(AUDIO_CLKCFG_PHYSICAL_BASE, 0x1000);
	apmixedsys_base_address = ioremap_nocache(APMIXEDSYS_BASE, 0x1000);
	return 0;
#endif
}

void mt_afe_reg_unmap(void)
{
	if (afe_base_address) {
		iounmap(afe_base_address);
		afe_base_address = NULL;
	}
	if (afe_sram_address) {
		iounmap(afe_sram_address);
		afe_sram_address = NULL;
	}
	if (topckgen_base_address) {
		iounmap(topckgen_base_address);
		topckgen_base_address = NULL;
	}
	if (spm_base_address) {
		iounmap(spm_base_address);
		spm_base_address = NULL;
	}
	if (apmixedsys_base_address) {
		iounmap(apmixedsys_base_address);
		apmixedsys_base_address = NULL;
	}
}

void mt_afe_set_reg(uint32_t offset, uint32_t value, uint32_t mask)/*Afe_Set_Reg*/
{
#ifdef AUDIO_MEM_IOREMAP
	const uint32_t *address = (uint32_t *) ((char *)afe_base_address + offset);
#else
	const uint32_t address = (AFE_BASE + offset);
#endif
	uint32_t val_tmp;

	val_tmp = mt_afe_get_reg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, address);
}
EXPORT_SYMBOL(mt_afe_set_reg);

uint32_t mt_afe_get_reg(uint32_t offset) /*Afe_Get_Reg*/
{
#ifdef AUDIO_MEM_IOREMAP
	const uint32_t *address = (uint32_t *) ((char *)afe_base_address + offset);
#else
	const uint32_t address = (AFE_BASE + offset);
#endif
	return readl((const void __iomem *)address);
}
EXPORT_SYMBOL(mt_afe_get_reg);

uint32_t mt_afe_topck_get_reg(uint32_t offset)
{
	const uint32_t *address = (uint32_t *) ((char *)topckgen_base_address + offset);

	return readl((const void __iomem *)address);
}

void mt_afe_topck_set_reg(uint32_t offset, uint32_t value, uint32_t mask)
{
	const uint32_t *address = (uint32_t *) ((char *)topckgen_base_address + offset);
	uint32_t val_tmp;

	val_tmp = mt_afe_topck_get_reg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, address);
}

uint32_t mt_afe_pll_get_reg(uint32_t offset)
{
	const uint32_t *address = (uint32_t *) ((char *)apmixedsys_base_address + offset);

	return readl((const void __iomem *)address);
}

void mt_afe_pll_set_reg(uint32_t offset, uint32_t value, uint32_t mask)
{
	const uint32_t *address = (uint32_t *) ((char *)apmixedsys_base_address + offset);
	uint32_t val_tmp;

	val_tmp = mt_afe_pll_get_reg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, address);
}

uint32_t mt_afe_spm_get_reg(uint32_t offset)
{
	const uint32_t *address = (uint32_t *) ((char *)spm_base_address + offset);

	return readl((const void __iomem *)address);
}

void mt_afe_spm_set_reg(uint32_t offset, uint32_t value, uint32_t mask)
{
	const uint32_t *address = (uint32_t *) ((char *)spm_base_address + offset);
	uint32_t val_tmp;

	val_tmp = mt_afe_spm_get_reg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, address);
}

/* function to Set Cfg */
uint32_t GetClkCfg(uint32_t offset)
{
	volatile long address = (long)((char *)topckgen_base_address + offset);
	volatile uint32_t *value;

	value = (volatile uint32_t *)(address);
	/* pr_debug("GetClkCfg offset=%x address = %x value = 0x%x\n", offset, address, *value); */
	return *value;
}
EXPORT_SYMBOL(GetClkCfg);


void SetClkCfg(uint32_t offset, uint32_t value, uint32_t mask)
{
	volatile long address = (long)((char *)topckgen_base_address + offset);
	volatile uint32_t *AFE_Register = (volatile uint32_t *)address;
	volatile uint32_t val_tmp;
	/* pr_debug("SetClkCfg offset=%x, value=%x, mask=%x\n",offset,value,mask); */
	val_tmp = GetClkCfg(offset);
	val_tmp &= (~mask);
	val_tmp |= (value & mask);
	mt_reg_sync_writel(val_tmp, AFE_Register);
}
EXPORT_SYMBOL(SetClkCfg);


void *mt_afe_get_sram_base_ptr() /*Get_Afe_SramBase_Pointer*/
{
	return afe_sram_address;
}

phys_addr_t mt_afe_get_sram_phy_addr(void)/*Get_Afe_Sram_Phys_Addr*/
{
	return afe_sram_phy_address;
}
