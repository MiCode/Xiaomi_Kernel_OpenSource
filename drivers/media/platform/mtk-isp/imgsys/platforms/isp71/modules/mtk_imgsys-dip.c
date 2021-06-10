// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Frederic Chen <frederic.chen@mediatek.com>
 *         Holmes Chiou <holmes.chiou@mediatek.com>
 *
 */

#include <linux/device.h>
#include <linux/of_address.h>
#include <linux/dma-iommu.h>
#include <linux/pm_runtime.h>
#include <linux/remoteproc.h>
#include "mtk_imgsys-dip.h"

#define DIP_INIT_ARRAY_COUNT  181
const struct mtk_imgsys_init_array
			mtk_imgsys_dip_init_ary[DIP_INIT_ARRAY_COUNT] = {
	{0x098, 0x80000000},	/* DIPCTL_D1A_DIPCTL_INT1_EN */
	{0x0A4, 0x0},	/* DIPCTL_D1A_DIPCTL_INT2_EN */
	{0x0B0, 0x0},	/* DIPCTL_D1A_DIPCTL_INT3_EN */
	{0x0BC, 0x0},	/* DIPCTL_D1A_DIPCTL_CQ_INT1_EN */
	{0x0C8, 0x0},	/* DIPCTL_D1A_DIPCTL_CQ_INT2_EN */
	{0x0D4, 0x0},	/* DIPCTL_D1A_DIPCTL_CQ_INT3_EN */
	{0x210, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR0_CTL */
	{0x21C, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR1_CTL */
	{0x228, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR2_CTL */
	{0x234, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR3_CTL */
	{0x240, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR4_CTL */
	{0x24C, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR5_CTL */
	{0x258, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR6_CTL */
	{0x264, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR7_CTL */
	{0x270, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR8_CTL */
	{0x27C, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR9_CTL */
	{0x288, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR10_CTL */
	{0x294, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR11_CTL */
	{0x2A0, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR12_CTL */
	{0x2AC, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR13_CTL */
	{0x2B8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR14_CTL */
	{0x2C4, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR15_CTL */
	{0x2D0, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR16_CTL */
	{0x2DC, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR17_CTL */
	{0x2E8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR18_CTL */
	{0xA1C, 0x80000100},	/* IMGI_D1A_ORIRDMA_CON0 */
	{0xA20, 0x10AA008A},	/* IMGI_D1A_ORIRDMA_CON1 */
	{0xA24, 0x11000100},	/* IMGI_D1A_ORIRDMA_CON2 */
	{0xA7C, 0x80000080},	/* IMGBI_D1A_ORIRDMA_CON0 */
	{0xA80, 0x10550045},	/* IMGBI_D1A_ORIRDMA_CON1 */
	{0xA84, 0x10800080},	/* IMGBI_D1A_ORIRDMA_CON2 */
	{0xADC, 0x80000080},	/* IMGCI_D1A_ORIRDMA_CON0 */
	{0xAE0, 0x10550045},	/* IMGCI_D1A_ORIRDMA_CON1 */
	{0xAE4, 0x10800080},	/* IMGCI_D1A_ORIRDMA_CON2 */
	{0xB3C, 0x80000020},	/* IMGDI_D1A_ORIRDMA_CON0 */
	{0xB40, 0x10150011},	/* IMGDI_D1A_ORIRDMA_CON1 */
	{0xB44, 0x10200020},	/* IMGDI_D1A_ORIRDMA_CON2 */
	{0xB9C, 0x80000080},	/* DEPI_D1A_ORIRDMA_CON0 */
	{0xBA0, 0x10550045},	/* DEPI_D1A_ORIRDMA_CON1 */
	{0xBA4, 0x10800080},	/* DEPI_D1A_ORIRDMA_CON2 */
	{0xBFC, 0x80000080},	/* DMGI_D1A_ORIRDMA_CON0 */
	{0xC00, 0x10550045},	/* DMGI_D1A_ORIRDMA_CON1 */
	{0xC04, 0x10800080},	/* DMGI_D1A_ORIRDMA_CON2 */
	{0xC5C, 0x80000100},	/* VIPI_D1A_ORIRDMA_CON0 */
	{0xC60, 0x10AA008A},	/* VIPI_D1A_ORIRDMA_CON1 */
	{0xC64, 0x11000100},	/* VIPI_D1A_ORIRDMA_CON2 */
	{0xCBC, 0x80000080},	/* VIPBI_D1A_ORIRDMA_CON0 */
	{0xCC0, 0x10550045},	/* VIPBI_D1A_ORIRDMA_CON1 */
	{0xCC4, 0x10800080},	/* VIPBI_D1A_ORIRDMA_CON2 */
	{0xD1C, 0x80000080},	/* VIPCI_D1A_ORIRDMA_CON0 */
	{0xD20, 0x10550045},	/* VIPCI_D1A_ORIRDMA_CON1 */
	{0xD24, 0x10800080},	/* VIPCI_D1A_ORIRDMA_CON2 */
	{0x101C, 0x80000050},	/* TNRWI_D1A_ULCRDMA_CON0 */
	{0x1020, 0x1035002B},	/* TNRWI_D1A_ULCRDMA_CON1 */
	{0x1024, 0x10500050},	/* TNRWI_D1A_ULCRDMA_CON2 */
	{0x105C, 0x80000050},	/* TNRMI_D1A_ULCRDMA_CON0 */
	{0x1060, 0x1035002B},	/* TNRMI_D1A_ULCRDMA_CON1 */
	{0x1064, 0x10500050},	/* TNRMI_D1A_ULCRDMA_CON2 */
	{0x109C, 0x80000020},	/* TNRCI_D1A_ULCRDMA_CON0 */
	{0x10A0, 0x10150011},	/* TNRCI_D1A_ULCRDMA_CON1 */
	{0x10A4, 0x10200020},	/* TNRCI_D1A_ULCRDMA_CON2 */
	{0x10DC, 0x80000050},	/* TNRVBI_D1A_ULCRDMA_CON0 */
	{0x10E0, 0x1035002B},	/* TNRVBI_D1A_ULCRDMA_CON1 */
	{0x10E4, 0x10500050},	/* TNRVBI_D1A_ULCRDMA_CON2 */
	{0x111C, 0x80000020},	/* TNRLYI_D1A_ULCRDMA_CON0 */
	{0x1120, 0x10150011},	/* TNRLYI_D1A_ULCRDMA_CON1 */
	{0x1124, 0x10200020},	/* TNRLYI_D1A_ULCRDMA_CON2 */
	{0x115C, 0x80000020},	/* TNRLCI_D1A_ULCRDMA_CON0 */
	{0x1160, 0x10150011},	/* TNRLCI_D1A_ULCRDMA_CON1 */
	{0x1164, 0x10200020},	/* TNRLCI_D1A_ULCRDMA_CON2 */
	{0x119C, 0x80000020},	/* TNRSI_D1A_ULCRDMA_CON0 */
	{0x11A0, 0x10150011},	/* TNRSI_D1A_ULCRDMA_CON1 */
	{0x11A4, 0x10200020},	/* TNRSI_D1A_ULCRDMA_CON2 */
	{0x11DC, 0x80000080},	/* RECI_D1A_ORIRDMA_CON0 */
	{0x11E0, 0x10550045},	/* RECI_D1A_ORIRDMA_CON1 */
	{0x11E4, 0x10800080},	/* RECI_D1A_ORIRDMA_CON2 */
	{0x121C, 0x80000040},	/* RECBI_D1A_ORIRDMA_CON0 */
	{0x1220, 0x102A0022},	/* RECBI_D1A_ORIRDMA_CON1 */
	{0x1224, 0x10400040},	/* RECBI_D1A_ORIRDMA_CON2 */
	{0x125C, 0x80000080},	/* RECI_D2A_ORIRDMA_CON0 */
	{0x1260, 0x10550045},	/* RECI_D2A_ORIRDMA_CON1 */
	{0x1264, 0x10800080},	/* RECI_D2A_ORIRDMA_CON2 */
	{0x129C, 0x80000040},	/* RECBI_D2A_ORIRDMA_CON0 */
	{0x12A0, 0x102A0022},	/* RECBI_D2A_ORIRDMA_CON1 */
	{0x12A4, 0x10400040},	/* RECBI_D2A_ORIRDMA_CON2 */
	{0x12DC, 0x80000020},	/* SMTI_D1A_ULCRDMA_CON0 */
	{0x12E0, 0x10150011},	/* SMTI_D1A_ULCRDMA_CON1 */
	{0x12E4, 0x10200020},	/* SMTI_D1A_ULCRDMA_CON2 */
	{0x131C, 0x80000020},	/* SMTI_D2A_ULCRDMA_CON0 */
	{0x1320, 0x10150011},	/* SMTI_D2A_ULCRDMA_CON1 */
	{0x1324, 0x10200020},	/* SMTI_D2A_ULCRDMA_CON2 */
	{0x135C, 0x80000020},	/* SMTI_D3A_ULCRDMA_CON0 */
	{0x1360, 0x10150011},	/* SMTI_D3A_ULCRDMA_CON1 */
	{0x1364, 0x10200020},	/* SMTI_D3A_ULCRDMA_CON2 */
	{0x139C, 0x80000020},	/* SMTI_D4A_ULCRDMA_CON0 */
	{0x13A0, 0x10150011},	/* SMTI_D4A_ULCRDMA_CON1 */
	{0x13A4, 0x10200020},	/* SMTI_D4A_ULCRDMA_CON2 */
	{0x13DC, 0x80000020},	/* SMTI_D5A_ULCRDMA_CON0 */
	{0x13E0, 0x10150011},	/* SMTI_D5A_ULCRDMA_CON1 */
	{0x13E4, 0x10200020},	/* SMTI_D5A_ULCRDMA_CON2 */
	{0x141C, 0x80000020},	/* SMTI_D6A_ULCRDMA_CON0 */
	{0x1420, 0x10150011},	/* SMTI_D6A_ULCRDMA_CON1 */
	{0x1424, 0x10200020},	/* SMTI_D6A_ULCRDMA_CON2 */
	{0x145C, 0x80000020},	/* SMTI_D7A_ULCRDMA_CON0 */
	{0x1460, 0x10150011},	/* SMTI_D7A_ULCRDMA_CON1 */
	{0x1464, 0x10200020},	/* SMTI_D7A_ULCRDMA_CON2 */
	{0x149C, 0x80000020},	/* SMTI_D8A_ULCRDMA_CON0 */
	{0x14A0, 0x10150011},	/* SMTI_D8A_ULCRDMA_CON1 */
	{0x14A4, 0x10200020},	/* SMTI_D8A_ULCRDMA_CON2 */
	{0x14DC, 0x80000020},	/* SMTI_D9A_ULCRDMA_CON0 */
	{0x14E0, 0x10150011},	/* SMTI_D9A_ULCRDMA_CON1 */
	{0x14E4, 0x10200020},	/* SMTI_D9A_ULCRDMA_CON2 */
	{0x151C, 0x80000100},	/* IMG3O_D1A_ORIWDMA_CON0 */
	{0x1520, 0x10AA008A},	/* IMG3O_D1A_ORIWDMA_CON1 */
	{0x1524, 0x11000100},	/* IMG3O_D1A_ORIWDMA_CON2 */
	{0x15CC, 0x80000080},	/* IMG3BO_D1A_ORIWDMA_CON0 */
	{0x15D0, 0x10550045},	/* IMG3BO_D1A_ORIWDMA_CON1 */
	{0x15D4, 0x10800080},	/* IMG3BO_D1A_ORIWDMA_CON2 */
	{0x167C, 0x80000080},	/* IMG3CO_D1A_ORIWDMA_CON0 */
	{0x1680, 0x10550045},	/* IMG3CO_D1A_ORIWDMA_CON1 */
	{0x1684, 0x10800080},	/* IMG3CO_D1A_ORIWDMA_CON2 */
	{0x172C, 0x80000020},	/* IMG3DO_D1A_ORIWDMA_CON0 */
	{0x1730, 0x10150011},	/* IMG3DO_D1A_ORIWDMA_CON1 */
	{0x1734, 0x10200020},	/* IMG3DO_D1A_ORIWDMA_CON2 */
	{0x17DC, 0x80000080},	/* IMG4O_D1A_ORIWDMA_CON0 */
	{0x17E0, 0x10550045},	/* IMG4O_D1A_ORIWDMA_CON1 */
	{0x17E4, 0x10800080},	/* IMG4O_D1A_ORIWDMA_CON2 */
	{0x188C, 0x80000040},	/* IMG4BO_D1A_ORIWDMA_CON0 */
	{0x1890, 0x102A0022},	/* IMG4BO_D1A_ORIWDMA_CON1 */
	{0x1894, 0x10400040},	/* IMG4BO_D1A_ORIWDMA_CON2 */
	{0x193C, 0x80000020},	/* IMG4CO_D1A_ORIWDMA_CON0 */
	{0x1940, 0x10150011},	/* IMG4CO_D1A_ORIWDMA_CON1 */
	{0x1944, 0x10200020},	/* IMG4CO_D1A_ORIWDMA_CON2 */
	{0x19EC, 0x80000020},	/* IMG4DO_D1A_ORIWDMA_CON0 */
	{0x19F0, 0x10150011},	/* IMG4DO_D1A_ORIWDMA_CON1 */
	{0x19F4, 0x10200020},	/* IMG4DO_D1A_ORIWDMA_CON2 */
	{0x1A9C, 0x800000A0},	/* FEO_D1A_ORIWDMA_CON0 */
	{0x1AA0, 0x106A0056},	/* FEO_D1A_ORIWDMA_CON1 */
	{0x1AA4, 0x10A000A0},	/* FEO_D1A_ORIWDMA_CON2 */
	{0x1B4C, 0x80000080},	/* IMG2O_D1A_ORIWDMA_CON0 */
	{0x1B50, 0x10550045},	/* IMG2O_D1A_ORIWDMA_CON1 */
	{0x1B54, 0x10800080},	/* IMG2O_D1A_ORIWDMA_CON2 */
	{0x1B8C, 0x80000040},	/* IMG2BO_D1A_ORIWDMA_CON0 */
	{0x1B90, 0x102A0022},	/* IMG2BO_D1A_ORIWDMA_CON1 */
	{0x1B94, 0x10400040},	/* IMG2BO_D1A_ORIWDMA_CON2 */
	{0x1BCC, 0x80000050},	/* TNRWO_D1A_ULCWDMA_CON0 */
	{0x1BD0, 0x1035002B},	/* TNRWO_D1A_ULCWDMA_CON1 */
	{0x1BD4, 0x10500050},	/* TNRWO_D1A_ULCWDMA_CON2 */
	{0x1C0C, 0x80000050},	/* TNRMO_D1A_ULCWDMA_CON0 */
	{0x1C10, 0x1035002B},	/* TNRMO_D1A_ULCWDMA_CON1 */
	{0x1C14, 0x10500050},	/* TNRMO_D1A_ULCWDMA_CON2 */
	{0x1C4C, 0x80000020},	/* TNRSO_D1A_ULCWDMA_CON0 */
	{0x1C50, 0x10150011},	/* TNRSO_D1A_ULCWDMA_CON1 */
	{0x1C54, 0x10200020},	/* TNRSO_D1A_ULCWDMA_CON2 */
	{0x1C8C, 0x80000020},	/* SMTO_D1A_ULCWDMA_CON0 */
	{0x1C90, 0x10150011},	/* SMTO_D1A_ULCWDMA_CON1 */
	{0x1C94, 0x10200020},	/* SMTO_D1A_ULCWDMA_CON2 */
	{0x1CCC, 0x80000020},	/* SMTO_D2A_ULCWDMA_CON0 */
	{0x1CD0, 0x10150011},	/* SMTO_D2A_ULCWDMA_CON1 */
	{0x1CD4, 0x10200020},	/* SMTO_D2A_ULCWDMA_CON2 */
	{0x1D0C, 0x80000020},	/* SMTO_D3A_ULCWDMA_CON0 */
	{0x1D10, 0x10150011},	/* SMTO_D3A_ULCWDMA_CON1 */
	{0x1D14, 0x10200020},	/* SMTO_D3A_ULCWDMA_CON2 */
	{0x1D4C, 0x80000020},	/* SMTO_D4A_ULCWDMA_CON0 */
	{0x1D50, 0x10150011},	/* SMTO_D4A_ULCWDMA_CON1 */
	{0x1D54, 0x10200020},	/* SMTO_D4A_ULCWDMA_CON2 */
	{0x1D8C, 0x80000020},	/* SMTO_D5A_ULCWDMA_CON0 */
	{0x1D90, 0x10150011},	/* SMTO_D5A_ULCWDMA_CON1 */
	{0x1D94, 0x10200020},	/* SMTO_D5A_ULCWDMA_CON2 */
	{0x1DCC, 0x80000020},	/* SMTO_D6A_ULCWDMA_CON0 */
	{0x1DD0, 0x10150011},	/* SMTO_D6A_ULCWDMA_CON1 */
	{0x1DD4, 0x10200020},	/* SMTO_D6A_ULCWDMA_CON2 */
	{0x1E0C, 0x80000020},	/* SMTO_D7A_ULCWDMA_CON0 */
	{0x1E10, 0x10150011},	/* SMTO_D7A_ULCWDMA_CON1 */
	{0x1E14, 0x10200020},	/* SMTO_D7A_ULCWDMA_CON2 */
	{0x1E4C, 0x80000020},	/* SMTO_D8A_ULCWDMA_CON0 */
	{0x1E50, 0x10150011},	/* SMTO_D8A_ULCWDMA_CON1 */
	{0x1E54, 0x10200020},	/* SMTO_D8A_ULCWDMA_CON2 */
	{0x1E8C, 0x80000020},	/* SMTO_D9A_ULCWDMA_CON0 */
	{0x1E90, 0x10150011},	/* SMTO_D9A_ULCWDMA_CON1 */
	{0x1E94, 0x10200020}	/* SMTO_D9A_ULCWDMA_CON2 */
};

void imgsys_dip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *dipRegBA = 0L;
	void __iomem *ofset = NULL;
	unsigned int i = 0;

	pr_info("%s: +\n", __func__);
	dev_dbg(imgsys_dev->dev, "%s: +\n", __func__);

	/* iomap registers */
	dipRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DIP);
	if (!dipRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
	}

	for (i = 0 ; i < DIP_INIT_ARRAY_COUNT ; i++) {
		ofset = dipRegBA + mtk_imgsys_dip_init_ary[i].ofset;
		writel(mtk_imgsys_dip_init_ary[i].val, ofset);
	}

	iounmap(dipRegBA);
	pr_info("%s: -\n", __func__);
	dev_dbg(imgsys_dev->dev, "%s: -\n", __func__);
}
EXPORT_SYMBOL(imgsys_dip_set_initial_value);

void imgsys_dip_debug_dump(struct mtk_imgsys_dev *imgsys_dev,
							unsigned int engine)
{
	void __iomem *dipRegBA = 0L;
	unsigned int i;

	pr_info("%s: +\n", __func__);
	dev_dbg(imgsys_dev->dev, "%s: +\n", __func__);

	/* iomap registers */
	dipRegBA = of_iomap(imgsys_dev->dev->of_node, REG_MAP_E_DIP);
	if (!dipRegBA) {
		dev_info(imgsys_dev->dev, "%s Unable to ioremap dip registers\n",
								__func__);
		dev_info(imgsys_dev->dev, "%s of_iomap fail, devnode(%s).\n",
				__func__, imgsys_dev->dev->of_node->name);
	}

	dev_info(imgsys_dev->dev, "%s: dump dip ctl regs\n", __func__);
	for (i = TOP_CTL_OFFSET; i <= TOP_CTL_OFFSET + TOP_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15100000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15100000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15100000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip dmatop regs\n", __func__);
	for (i = DMATOP_OFFSET; i <= DMATOP_OFFSET + DMATOP_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15100000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15100000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15100000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip rdma regs\n", __func__);
	for (i = RDMA_OFFSET; i <= RDMA_OFFSET + RDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15100000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15100000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15100000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip wdma regs\n", __func__);
	for (i = WDMA_OFFSET; i <= WDMA_OFFSET + WDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15100000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15100000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15100000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump nr3d ctl regs\n", __func__);
	for (i = NR3D_CTL_OFFSET; i <= NR3D_CTL_OFFSET + NR3D_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15100000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15100000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15100000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump mcrop regs\n", __func__);
	for (i = MCRP_OFFSET; i <= MCRP_OFFSET + MCRP_RANGE; i += 0x8) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15100000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)));
	}

	/* Set DIPCTL_DBG_SEL[3:0] to 0x1 */
	/* Set DIPCTL_DBG_SEL[15:8] to 0x13 */
	/* Set DIPCTL_DBG_SEL[19:6] to 0x1~0x4 */
	dev_info(imgsys_dev->dev, "%s: dipctl_dbg_sel_ee\n", __func__);
	iowrite32(0x11301, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: ee_out_debug0: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x21301, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: ee_out_debug1: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x31301, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: ee_out_debug2: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x41301, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: ee_out_debug3: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));

	/* Set DIPCTL_DBG_SEL[3:0] to 0x1 */
	/* Set DIPCTL_DBG_SEL[15:8] to 0x15 */
	/* Set DIPCTL_DBG_SEL[19:6] to 0x1~0x8 */
	dev_info(imgsys_dev->dev, "%s: dipctl_dbg_sel_aks\n", __func__);
	iowrite32(0x11501, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: aks_checksum1: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x21501, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: aks_checksum2: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x31501, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: aks_checksum3: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x41501, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: aks_debug_data0: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x51501, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: aks_debug_data1: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x61501, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: aks_debug_data2: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x71501, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: aks_debug_data3: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x81501, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: aks_debug_data4: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));

	/* Set DIPCTL_DBG_SEL[3:0] to 0x1 */
	/* Set DIPCTL_DBG_SEL[15:8] to 0x16 */
	/* Set DIPCTL_DBG_SEL[19:6] to 0x1~0x9 */
	dev_info(imgsys_dev->dev, "%s: dipctl_dbg_sel_cnr\n", __func__);
	iowrite32(0x11601, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: cnr_debug0: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x21601, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: cnr_debug1: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x31601, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: cnr_debug2: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x41601, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: cnr_debug3: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x51601, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: cnr_debug4: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x61601, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: cnr_debug5: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x71601, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: cnr_debug6: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x81601, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: cnr_debug7: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x91601, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: cnr_debug8: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));

	/* Set DIPCTL_DBG_SEL[3:0] to 0x1 */
	/* Set DIPCTL_DBG_SEL[15:8] to 0x18 */
	/* Set DIPCTL_DBG_SEL[19:6] to 0x1*/
	dev_info(imgsys_dev->dev, "%s: dipctl_dbg_sel_tnc\n", __func__);
	iowrite32(0x1801, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: tnc_debug: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));

	/* Set DIPCTL_DBG_SEL[3:0] to 0x1 */
	/* Set DIPCTL_DBG_SEL[15:8] to 0x0 */
	/* Set DIPCTL_DBG_SEL[19:6] to 0x0~0xD */
	dev_info(imgsys_dev->dev, "%s: dipctl_dbg_sel_nr3d\n", __func__);
	iowrite32(0x13, (void *)(dipRegBA + NR3D_DBG_SEL));
	iowrite32(0x00001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_sot_latch_32~1: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x20001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_eot_latch_32~1: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x10001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_sot_latch_33~39: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x30001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_eot_latch_33~39: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x40001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif4~1: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x50001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif8~5: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x60001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif12~9: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x70001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif16~13: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x80001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif20~17: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0x90001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif24~21: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0xA0001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif28~25: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0xB0001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif32~29: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0xC0001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif36~33: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	iowrite32(0xD0001, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: nr3d_tif39~37: %08X", __func__,
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));

	iounmap(dipRegBA);
	//dev_dbg(imgsys_dev->dev, "%s: +\n",__func__);
	//
	pr_info("%s: -\n", __func__);
	dev_dbg(imgsys_dev->dev, "%s: -\n", __func__);
}
EXPORT_SYMBOL(imgsys_dip_debug_dump);
