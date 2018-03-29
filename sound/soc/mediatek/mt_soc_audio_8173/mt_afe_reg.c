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

#include "mt_afe_reg.h"
#include "mt_afe_clk.h"
#include "mt_afe_control.h"
#include <sync_write.h>
#include <linux/of_address.h>

/* address for ioremap audio hardware register */
static void *afe_base_address;
static void *afe_sram_address;
static void *spm_base_address;
static void *topckgen_base_address;
static void *apmixedsys_base_address;
static phys_addr_t afe_sram_phy_address = AFE_INTERNAL_SRAM_PHY_BASE;
static uint32_t afe_sram_size = AFE_INTERNAL_SRAM_SIZE;


int mt_afe_reg_remap(void *dev)
{
#ifdef AUDIO_IOREMAP_FROM_DT
	int ret = 0;
	struct device *pdev = dev;
	struct resource res;
	struct device_node *node;

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
	afe_sram_size = resource_size(&res);

	/* TOPCKGEN register base */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-topckgen");
	if (!node) {
		pr_warn("%s of_find_compatible_node(mediatek,mt8173-topckgen) fail\n", __func__);
		topckgen_base_address = ioremap_nocache(CKSYS_TOP, 0x1000);
	} else {
		topckgen_base_address = of_iomap(node, 0);
	}

	if (!topckgen_base_address) {
		pr_err("%s ioremap topckgen_base_address fail\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	/* SPM register base */
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-scpsys");
	if (!node) {
		pr_warn("%s of_find_compatible_node(mediatek,mt8173-scpsys) fail\n", __func__);
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
	node = of_find_compatible_node(NULL, NULL, "mediatek,mt8173-apmixedsys");
	if (!node) {
		pr_warn("%s of_find_compatible_node(mediatek,mt8173-apmixedsys) fail\n", __func__);
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
	topckgen_base_address = ioremap_nocache(CKSYS_TOP, 0x1000);
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

void mt_afe_set_reg(uint32_t offset, uint32_t value, uint32_t mask)
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

uint32_t mt_afe_get_reg(uint32_t offset)
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

void *mt_afe_get_sram_base_ptr()
{
	return afe_sram_address;
}

phys_addr_t mt_afe_get_sram_phy_addr(void)
{
	return afe_sram_phy_address;
}

uint32_t mt_afe_get_sram_size(void)
{
	return afe_sram_size;
}

void mt_afe_log_print(void)
{
	mt_afe_main_clk_on();
	pr_debug("+AudDrv mt_afe_log_print\n");
	pr_debug("AUDIO_TOP_CON0  = 0x%x\n", mt_afe_get_reg(AUDIO_TOP_CON0));
	pr_debug("AUDIO_TOP_CON3  = 0x%x\n", mt_afe_get_reg(AUDIO_TOP_CON3));
	pr_debug("AFE_DAC_CON0  = 0x%x\n", mt_afe_get_reg(AFE_DAC_CON0));
	pr_debug("AFE_DAC_CON1  = 0x%x\n", mt_afe_get_reg(AFE_DAC_CON1));
	pr_debug("AFE_I2S_CON  = 0x%x\n", mt_afe_get_reg(AFE_I2S_CON));
	pr_debug("AFE_DAIBT_CON0  = 0x%x\n", mt_afe_get_reg(AFE_DAIBT_CON0));
	pr_debug("AFE_CONN0  = 0x%x\n", mt_afe_get_reg(AFE_CONN0));
	pr_debug("AFE_CONN1  = 0x%x\n", mt_afe_get_reg(AFE_CONN1));
	pr_debug("AFE_CONN2  = 0x%x\n", mt_afe_get_reg(AFE_CONN2));
	pr_debug("AFE_CONN3  = 0x%x\n", mt_afe_get_reg(AFE_CONN3));
	pr_debug("AFE_CONN4  = 0x%x\n", mt_afe_get_reg(AFE_CONN4));
	pr_debug("AFE_I2S_CON1  = 0x%x\n", mt_afe_get_reg(AFE_I2S_CON1));
	pr_debug("AFE_I2S_CON2  = 0x%x\n", mt_afe_get_reg(AFE_I2S_CON2));
	pr_debug("AFE_MRGIF_CON  = 0x%x\n", mt_afe_get_reg(AFE_MRGIF_CON));

	pr_debug("AFE_DL1_BASE  = 0x%x\n", mt_afe_get_reg(AFE_DL1_BASE));
	pr_debug("AFE_DL1_CUR  = 0x%x\n", mt_afe_get_reg(AFE_DL1_CUR));
	pr_debug("AFE_DL1_END  = 0x%x\n", mt_afe_get_reg(AFE_DL1_END));
	pr_debug("AFE_DL2_BASE  = 0x%x\n", mt_afe_get_reg(AFE_DL2_BASE));
	pr_debug("AFE_DL2_CUR  = 0x%x\n", mt_afe_get_reg(AFE_DL2_CUR));
	pr_debug("AFE_DL2_END  = 0x%x\n", mt_afe_get_reg(AFE_DL2_END));
	pr_debug("AFE_AWB_BASE  = 0x%x\n", mt_afe_get_reg(AFE_AWB_BASE));
	pr_debug("AFE_AWB_END  = 0x%x\n", mt_afe_get_reg(AFE_AWB_END));
	pr_debug("AFE_AWB_CUR  = 0x%x\n", mt_afe_get_reg(AFE_AWB_CUR));
	pr_debug("AFE_VUL_BASE  = 0x%x\n", mt_afe_get_reg(AFE_VUL_BASE));
	pr_debug("AFE_VUL_END  = 0x%x\n", mt_afe_get_reg(AFE_VUL_END));
	pr_debug("AFE_VUL_CUR  = 0x%x\n", mt_afe_get_reg(AFE_VUL_CUR));
	pr_debug("AFE_DAI_BASE  = 0x%x\n", mt_afe_get_reg(AFE_DAI_BASE));
	pr_debug("AFE_DAI_END  = 0x%x\n", mt_afe_get_reg(AFE_DAI_END));
	pr_debug("AFE_DAI_CUR  = 0x%x\n", mt_afe_get_reg(AFE_DAI_CUR));

	pr_debug("AFE_MEMIF_MON0  = 0x%x\n", mt_afe_get_reg(AFE_MEMIF_MON0));
	pr_debug("AFE_MEMIF_MON1  = 0x%x\n", mt_afe_get_reg(AFE_MEMIF_MON1));
	pr_debug("AFE_MEMIF_MON2  = 0x%x\n", mt_afe_get_reg(AFE_MEMIF_MON2));
	pr_debug("AFE_MEMIF_MON4  = 0x%x\n", mt_afe_get_reg(AFE_MEMIF_MON4));

	pr_debug("AFE_ADDA_DL_SRC2_CON0  = 0x%x\n", mt_afe_get_reg(AFE_ADDA_DL_SRC2_CON0));
	pr_debug("AFE_ADDA_DL_SRC2_CON1  = 0x%x\n", mt_afe_get_reg(AFE_ADDA_DL_SRC2_CON1));
	pr_debug("AFE_ADDA_UL_SRC_CON0  = 0x%x\n", mt_afe_get_reg(AFE_ADDA_UL_SRC_CON0));
	pr_debug("AFE_ADDA_UL_SRC_CON1  = 0x%x\n", mt_afe_get_reg(AFE_ADDA_UL_SRC_CON1));
	pr_debug("AFE_ADDA_TOP_CON0  = 0x%x\n", mt_afe_get_reg(AFE_ADDA_TOP_CON0));
	pr_debug("AFE_ADDA_UL_DL_CON0  = 0x%x\n", mt_afe_get_reg(AFE_ADDA_UL_DL_CON0));
	pr_debug("AFE_ADDA_SRC_DEBUG  = 0x%x\n", mt_afe_get_reg(AFE_ADDA_SRC_DEBUG));
	pr_debug("AFE_ADDA_SRC_DEBUG_MON0  = 0x%x\n", mt_afe_get_reg(AFE_ADDA_SRC_DEBUG_MON0));
	pr_debug("AFE_ADDA_SRC_DEBUG_MON1  = 0x%x\n", mt_afe_get_reg(AFE_ADDA_SRC_DEBUG_MON1));
	pr_debug("AFE_ADDA_NEWIF_CFG0  = 0x%x\n", mt_afe_get_reg(AFE_ADDA_NEWIF_CFG0));
	pr_debug("AFE_ADDA_NEWIF_CFG1  = 0x%x\n", mt_afe_get_reg(AFE_ADDA_NEWIF_CFG1));
	pr_debug("AFE_ADDA2_TOP_CON0  = 0x%x\n", mt_afe_get_reg(AFE_ADDA2_TOP_CON0));

	pr_debug("AFE_SIDETONE_DEBUG  = 0x%x\n", mt_afe_get_reg(AFE_SIDETONE_DEBUG));
	pr_debug("AFE_SIDETONE_MON  = 0x%x\n", mt_afe_get_reg(AFE_SIDETONE_MON));
	pr_debug("AFE_SIDETONE_CON0  = 0x%x\n", mt_afe_get_reg(AFE_SIDETONE_CON0));
	pr_debug("AFE_SIDETONE_COEFF  = 0x%x\n", mt_afe_get_reg(AFE_SIDETONE_COEFF));
	pr_debug("AFE_SIDETONE_CON1  = 0x%x\n", mt_afe_get_reg(AFE_SIDETONE_CON1));
	pr_debug("AFE_SIDETONE_GAIN  = 0x%x\n", mt_afe_get_reg(AFE_SIDETONE_GAIN));
	pr_debug("AFE_SIDETONE_GAIN  = 0x%x\n", mt_afe_get_reg(AFE_SGEN_CON0));
	pr_debug("AFE_TOP_CON0  = 0x%x\n", mt_afe_get_reg(AFE_TOP_CON0));

	pr_debug("AFE_ADDA_PREDIS_CON0  = 0x%x\n", mt_afe_get_reg(AFE_ADDA_PREDIS_CON0));
	pr_debug("AFE_ADDA_PREDIS_CON1  = 0x%x\n", mt_afe_get_reg(AFE_ADDA_PREDIS_CON1));

	pr_debug("AFE_MRGIF_MON0  = 0x%x\n", mt_afe_get_reg(AFE_MRGIF_MON0));
	pr_debug("AFE_MRGIF_MON1  = 0x%x\n", mt_afe_get_reg(AFE_MRGIF_MON1));
	pr_debug("AFE_MRGIF_MON2  = 0x%x\n", mt_afe_get_reg(AFE_MRGIF_MON2));
	pr_debug("AFE_MOD_DAI_BASE  = 0x%x\n", mt_afe_get_reg(AFE_MOD_DAI_BASE));
	pr_debug("AFE_MOD_DAI_END  = 0x%x\n", mt_afe_get_reg(AFE_MOD_DAI_END));
	pr_debug("AFE_MOD_DAI_CUR  = 0x%x\n", mt_afe_get_reg(AFE_MOD_DAI_CUR));

	pr_debug("AFE_HDMI_OUT_CON0  = 0x%x\n", mt_afe_get_reg(AFE_HDMI_OUT_CON0));
	pr_debug("AFE_HDMI_OUT_BASE  = 0x%x\n", mt_afe_get_reg(AFE_HDMI_OUT_BASE));
	pr_debug("AFE_HDMI_OUT_CUR  = 0x%x\n", mt_afe_get_reg(AFE_HDMI_OUT_CUR));
	pr_debug("AFE_HDMI_OUT_END  = 0x%x\n", mt_afe_get_reg(AFE_HDMI_OUT_END));
	pr_debug("AFE_SPDIF_OUT_CON0  = 0x%x\n", mt_afe_get_reg(AFE_SPDIF_OUT_CON0));
	pr_debug("AFE_SPDIF_BASE  = 0x%x\n", mt_afe_get_reg(AFE_SPDIF_BASE));
	pr_debug("AFE_SPDIF_CUR  = 0x%x\n", mt_afe_get_reg(AFE_SPDIF_CUR));
	pr_debug("AFE_SPDIF_END  = 0x%x\n", mt_afe_get_reg(AFE_SPDIF_END));
	pr_debug("AFE_HDMI_CONN0  = 0x%x\n", mt_afe_get_reg(AFE_HDMI_CONN0));

	pr_debug("AFE_IRQ_MCU_CON  = 0x%x\n", mt_afe_get_reg(AFE_IRQ_MCU_CON));
	pr_debug("AFE_IRQ_MCU_STATUS  = 0x%x\n", mt_afe_get_reg(AFE_IRQ_MCU_STATUS));
	pr_debug("AFE_IRQ_MCU_CLR  = 0x%x\n", mt_afe_get_reg(AFE_IRQ_MCU_CLR));
	pr_debug("AFE_IRQ_MCU_CNT1  = 0x%x\n", mt_afe_get_reg(AFE_IRQ_MCU_CNT1));
	pr_debug("AFE_IRQ_MCU_CNT2  = 0x%x\n", mt_afe_get_reg(AFE_IRQ_MCU_CNT2));
	pr_debug("AFE_IRQ_MCU_MON2  = 0x%x\n", mt_afe_get_reg(AFE_IRQ_MCU_MON2));
	pr_debug("AFE_IRQ_MCU_CNT5  = 0x%x\n", mt_afe_get_reg(AFE_IRQ_MCU_CNT5));
	pr_debug("AFE_IRQ1_MCU_CNT_MON  = 0x%x\n", mt_afe_get_reg(AFE_IRQ1_MCU_CNT_MON));
	pr_debug("AFE_IRQ2_MCU_CNT_MON  = 0x%x\n", mt_afe_get_reg(AFE_IRQ2_MCU_CNT_MON));
	pr_debug("AFE_IRQ1_MCU_EN_CNT_MON  = 0x%x\n", mt_afe_get_reg(AFE_IRQ1_MCU_EN_CNT_MON));
	pr_debug("AFE_IRQ5_MCU_CNT_MON  = 0x%x\n", mt_afe_get_reg(AFE_IRQ5_MCU_CNT_MON));
	pr_debug("AFE_MEMIF_MAXLEN  = 0x%x\n", mt_afe_get_reg(AFE_MEMIF_MAXLEN));
	pr_debug("AFE_MEMIF_PBUF_SIZE  = 0x%x\n", mt_afe_get_reg(AFE_MEMIF_PBUF_SIZE));

	pr_debug("AFE_GAIN1_CON0  = 0x%x\n", mt_afe_get_reg(AFE_GAIN1_CON0));
	pr_debug("AFE_GAIN1_CON1  = 0x%x\n", mt_afe_get_reg(AFE_GAIN1_CON1));
	pr_debug("AFE_GAIN1_CON2  = 0x%x\n", mt_afe_get_reg(AFE_GAIN1_CON2));
	pr_debug("AFE_GAIN1_CON3  = 0x%x\n", mt_afe_get_reg(AFE_GAIN1_CON3));
	pr_debug("AFE_GAIN1_CONN  = 0x%x\n", mt_afe_get_reg(AFE_GAIN1_CONN));
	pr_debug("AFE_GAIN1_CUR   = 0x%x\n", mt_afe_get_reg(AFE_GAIN1_CUR));
	pr_debug("AFE_GAIN2_CON0  = 0x%x\n", mt_afe_get_reg(AFE_GAIN2_CON0));
	pr_debug("AFE_GAIN2_CON1  = 0x%x\n", mt_afe_get_reg(AFE_GAIN2_CON1));
	pr_debug("AFE_GAIN2_CON2  = 0x%x\n", mt_afe_get_reg(AFE_GAIN2_CON2));
	pr_debug("AFE_GAIN2_CON3  = 0x%x\n", mt_afe_get_reg(AFE_GAIN2_CON3));
	pr_debug("AFE_GAIN2_CONN  = 0x%x\n", mt_afe_get_reg(AFE_GAIN2_CONN));
	pr_debug("AFE_GAIN2_CUR   = 0x%x\n", mt_afe_get_reg(AFE_GAIN2_CUR));
	pr_debug("AFE_GAIN2_CONN2 = 0x%x\n", mt_afe_get_reg(AFE_GAIN2_CONN2));

	pr_debug("AFE_IEC_CFG = 0x%x\n", mt_afe_get_reg(AFE_IEC_CFG));
	pr_debug("AFE_IEC_NSNUM = 0x%x\n", mt_afe_get_reg(AFE_IEC_NSNUM));
	pr_debug("AFE_IEC_BURST_INFO = 0x%x\n", mt_afe_get_reg(AFE_IEC_BURST_INFO));
	pr_debug("AFE_IEC_BURST_LEN = 0x%x\n", mt_afe_get_reg(AFE_IEC_BURST_LEN));
	pr_debug("AFE_IEC_NSADR = 0x%x\n", mt_afe_get_reg(AFE_IEC_NSADR));
	pr_debug("AFE_IEC_CHL_STAT0 = 0x%x\n", mt_afe_get_reg(AFE_IEC_CHL_STAT0));
	pr_debug("AFE_IEC_CHL_STAT1 = 0x%x\n", mt_afe_get_reg(AFE_IEC_CHL_STAT1));
	pr_debug("AFE_IEC_CHR_STAT0 = 0x%x\n", mt_afe_get_reg(AFE_IEC_CHR_STAT0));
	pr_debug("AFE_IEC_CHR_STAT1 = 0x%x\n", mt_afe_get_reg(AFE_IEC_CHR_STAT1));

	pr_debug("AFE_ASRC_CON0  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON0));
	pr_debug("AFE_ASRC_CON1  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON1));
	pr_debug("AFE_ASRC_CON2  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON2));
	pr_debug("AFE_ASRC_CON3  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON3));
	pr_debug("AFE_ASRC_CON4  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON4));
	pr_debug("AFE_ASRC_CON5  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON5));
	pr_debug("AFE_ASRC_CON6  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON6));
	pr_debug("AFE_ASRC_CON7  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON7));
	pr_debug("AFE_ASRC_CON8  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON8));
	pr_debug("AFE_ASRC_CON9  = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON9));
	pr_debug("AFE_ASRC_CON10 = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON10));
	pr_debug("AFE_ASRC_CON11 = 0x%x\n", mt_afe_get_reg(AFE_ASRC_CON11));
	pr_debug("PCM_INTF_CON1  = 0x%x\n", mt_afe_get_reg(PCM_INTF_CON1));
	pr_debug("PCM_INTF_CON2  = 0x%x\n", mt_afe_get_reg(PCM_INTF_CON2));
	pr_debug("PCM2_INTF_CON  = 0x%x\n", mt_afe_get_reg(PCM2_INTF_CON));
	mt_afe_main_clk_off();
	pr_debug("-AudDrv mt_afe_log_print\n");
}
EXPORT_SYMBOL(mt_afe_log_print);
