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

const struct mtk_imgsys_init_array mtk_imgsys_dip_init_ary[] = {
	{0x094, 0x80000000},	/* DIPCTL_D1A_DIPCTL_INT1_EN */
	{0x0A0, 0x0},	/* DIPCTL_D1A_DIPCTL_INT2_EN */
	{0x0AC, 0x0},	/* DIPCTL_D1A_DIPCTL_INT3_EN */
	{0x0C4, 0x0},	/* DIPCTL_D1A_DIPCTL_CQ_INT1_EN */
	{0x0D0, 0x0},	/* DIPCTL_D1A_DIPCTL_CQ_INT2_EN */
	{0x0DC, 0x0},	/* DIPCTL_D1A_DIPCTL_CQ_INT3_EN */
	{0x208, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR0_CTL */
	{0x218, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR1_CTL */
	{0x228, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR2_CTL */
	{0x238, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR3_CTL */
	{0x248, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR4_CTL */
	{0x258, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR5_CTL */
	{0x268, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR6_CTL */
	{0x278, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR7_CTL */
	{0x288, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR8_CTL */
	{0x298, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR9_CTL */
	{0x2A8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR10_CTL */
	{0x2B8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR11_CTL */
	{0x2C8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR12_CTL */
	{0x2D8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR13_CTL */
	{0x2E8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR14_CTL */
	{0x2F8, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR15_CTL */
	{0x308, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR16_CTL */
	{0x318, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR17_CTL */
	{0x328, 0x11},	/* DIPCQ_D1A_DIPCQ_CQ_THR18_CTL */
	{0x1220, 0x10000100},	/* IMGI_D1A_ORIRDMA_CON0 */
	{0x1224, 0x10AA008A},	/* IMGI_D1A_ORIRDMA_CON1 */
	{0x1228, 0x11000100},	/* IMGI_D1A_ORIRDMA_CON2 */
	{0x1290, 0x10000080},	/* IMGBI_D1A_ORIRDMA_CON0 */
	{0x1294, 0x10550045},	/* IMGBI_D1A_ORIRDMA_CON1 */
	{0x1298, 0x10800080},	/* IMGBI_D1A_ORIRDMA_CON2 */
	{0x1300, 0x10000080},	/* IMGCI_D1A_ORIRDMA_CON0 */
	{0x1304, 0x10550045},	/* IMGCI_D1A_ORIRDMA_CON1 */
	{0x1308, 0x10800080},	/* IMGCI_D1A_ORIRDMA_CON2 */
	{0x1370, 0x10000020},	/* IMGDI_D1A_ORIRDMA_CON0 */
	{0x1374, 0x10150011},	/* IMGDI_D1A_ORIRDMA_CON1 */
	{0x1378, 0x10200020},	/* IMGDI_D1A_ORIRDMA_CON2 */
	{0x13E0, 0x10000080},	/* DEPI_D1A_ORIRDMA_CON0 */
	{0x13E4, 0x10550045},	/* DEPI_D1A_ORIRDMA_CON1 */
	{0x13E8, 0x10800080},	/* DEPI_D1A_ORIRDMA_CON2 */
	{0x1450, 0x10000080},	/* DMGI_D1A_ORIRDMA_CON0 */
	{0x1454, 0x10550045},	/* DMGI_D1A_ORIRDMA_CON1 */
	{0x1458, 0x10800080},	/* DMGI_D1A_ORIRDMA_CON2 */
	{0x514C0, 0x10000100},	/* VIPI_D1A_ORIRDMA_CON0 */
	{0x514C4, 0x10AA008A},	/* VIPI_D1A_ORIRDMA_CON1 */
	{0x514C8, 0x11000100},	/* VIPI_D1A_ORIRDMA_CON2 */
	{0x51530, 0x10000080},	/* VIPBI_D1A_ORIRDMA_CON0 */
	{0x51534, 0x10550045},	/* VIPBI_D1A_ORIRDMA_CON1 */
	{0x51538, 0x10800080},	/* VIPBI_D1A_ORIRDMA_CON2 */
	{0x515A0, 0x10000080},	/* VIPCI_D1A_ORIRDMA_CON0 */
	{0x515A4, 0x10550045},	/* VIPCI_D1A_ORIRDMA_CON1 */
	{0x515A8, 0x10800080},	/* VIPCI_D1A_ORIRDMA_CON2 */
	{0x1610, 0x10000050},	/* TNRWI_D1A_ULCRDMA_CON0 */
	{0x1614, 0x1035002B},	/* TNRWI_D1A_ULCRDMA_CON1 */
	{0x1618, 0x10500050},	/* TNRWI_D1A_ULCRDMA_CON2 */
	{0x1650, 0x10000050},	/* TNRMI_D1A_ULCRDMA_CON0 */
	{0x1654, 0x1035002B},	/* TNRMI_D1A_ULCRDMA_CON1 */
	{0x1658, 0x10500050},	/* TNRMI_D1A_ULCRDMA_CON2 */
	{0x1690, 0x10000020},	/* TNRCI_D1A_ULCRDMA_CON0 */
	{0x1694, 0x10150011},	/* TNRCI_D1A_ULCRDMA_CON1 */
	{0x1698, 0x10200020},	/* TNRCI_D1A_ULCRDMA_CON2 */
	{0x16D0, 0x10000050},	/* TNRVBI_D1A_ULCRDMA_CON0 */
	{0x16D4, 0x1035002B},	/* TNRVBI_D1A_ULCRDMA_CON1 */
	{0x16D8, 0x10500050},	/* TNRVBI_D1A_ULCRDMA_CON2 */
	{0x1710, 0x10000020},	/* TNRLYI_D1A_ULCRDMA_CON0 */
	{0x1714, 0x10150011},	/* TNRLYI_D1A_ULCRDMA_CON1 */
	{0x1718, 0x10200020},	/* TNRLYI_D1A_ULCRDMA_CON2 */
	{0x1750, 0x10000020},	/* TNRLCI_D1A_ULCRDMA_CON0 */
	{0x1754, 0x10150011},	/* TNRLCI_D1A_ULCRDMA_CON1 */
	{0x1758, 0x10200020},	/* TNRLCI_D1A_ULCRDMA_CON2 */
	{0x1790, 0x10000020},	/* TNRSI_D1A_ULCRDMA_CON0 */
	{0x1794, 0x10150011},	/* TNRSI_D1A_ULCRDMA_CON1 */
	{0x1798, 0x10200020},	/* TNRSI_D1A_ULCRDMA_CON2 */
	{0x17D0, 0x10000080},	/* RECI_D1A_ULCRDMA_CON0 */
	{0x17D4, 0x10550045},	/* RECI_D1A_ULCRDMA_CON1 */
	{0x17D8, 0x10800080},	/* RECI_D1A_ULCRDMA_CON2 */
	{0x1810, 0x10000040},	/* RECBI_D1A_ULCRDMA_CON0 */
	{0x1814, 0x102A0022},	/* RECBI_D1A_ULCRDMA_CON1 */
	{0x1818, 0x10400040},	/* RECBI_D1A_ULCRDMA_CON2 */
	{0x1850, 0x10000080},	/* RECI_D2A_ULCRDMA_CON0 */
	{0x1854, 0x10550045},	/* RECI_D2A_ULCRDMA_CON1 */
	{0x1858, 0x10800080},	/* RECI_D2A_ULCRDMA_CON2 */
	{0x1890, 0x10000040},	/* RECBI_D2A_ULCRDMA_CON0 */
	{0x1894, 0x102A0022},	/* RECBI_D2A_ULCRDMA_CON1 */
	{0x1898, 0x10400040},	/* RECBI_D2A_ULCRDMA_CON2 */
	{0x18D0, 0x10000020},	/* SMTI_D1A_ULCRDMA_CON0 */
	{0x18D4, 0x10150011},	/* SMTI_D1A_ULCRDMA_CON1 */
	{0x18D8, 0x10200020},	/* SMTI_D1A_ULCRDMA_CON2 */
	{0x1910, 0x10000020},	/* SMTI_D2A_ULCRDMA_CON0 */
	{0x1914, 0x10150011},	/* SMTI_D2A_ULCRDMA_CON1 */
	{0x1918, 0x10200020},	/* SMTI_D2A_ULCRDMA_CON2 */
	{0x1950, 0x10000020},	/* SMTI_D3A_ULCRDMA_CON0 */
	{0x1954, 0x10150011},	/* SMTI_D3A_ULCRDMA_CON1 */
	{0x1958, 0x10200020},	/* SMTI_D3A_ULCRDMA_CON2 */
	{0x51990, 0x10000020},	/* SMTI_D4A_ULCRDMA_CON0 */
	{0x51994, 0x10150011},	/* SMTI_D4A_ULCRDMA_CON1 */
	{0x51998, 0x10200020},	/* SMTI_D4A_ULCRDMA_CON2 */
	{0x519D0, 0x10000020},	/* SMTI_D5A_ULCRDMA_CON0 */
	{0x519D4, 0x10150011},	/* SMTI_D5A_ULCRDMA_CON1 */
	{0x519D8, 0x10200020},	/* SMTI_D5A_ULCRDMA_CON2 */
	{0x51A10, 0x10000020},	/* SMTI_D6A_ULCRDMA_CON0 */
	{0x51A14, 0x10150011},	/* SMTI_D6A_ULCRDMA_CON1 */
	{0x51A18, 0x10200020},	/* SMTI_D6A_ULCRDMA_CON2 */
	{0x51A50, 0x10000020},	/* SMTI_D7A_ULCRDMA_CON0 */
	{0x51A54, 0x10150011},	/* SMTI_D7A_ULCRDMA_CON1 */
	{0x51A58, 0x10200020},	/* SMTI_D7A_ULCRDMA_CON2 */
	{0x1A90, 0x10000020},	/* SMTI_D8A_ULCRDMA_CON0 */
	{0x1A94, 0x10150011},	/* SMTI_D8A_ULCRDMA_CON1 */
	{0x1A98, 0x10200020},	/* SMTI_D8A_ULCRDMA_CON2 */
	{0x51AD0, 0x10000020},	/* SMTI_D9A_ULCRDMA_CON0 */
	{0x51AD4, 0x10150011},	/* SMTI_D9A_ULCRDMA_CON1 */
	{0x51AD8, 0x10200020},	/* SMTI_D9A_ULCRDMA_CON2 */
	{0x52020, 0x10000100},	/* IMG3O_D1A_ORIWDMA_CON0 */
	{0x52024, 0x10AA008A},	/* IMG3O_D1A_ORIWDMA_CON1 */
	{0x52028, 0x11000100},	/* IMG3O_D1A_ORIWDMA_CON2 */
	{0x520D0, 0x10000080},	/* IMG3BO_D1A_ORIWDMA_CON0 */
	{0x520D4, 0x10550045},	/* IMG3BO_D1A_ORIWDMA_CON1 */
	{0x520D8, 0x10800080},	/* IMG3BO_D1A_ORIWDMA_CON2 */
	{0x52180, 0x10000080},	/* IMG3CO_D1A_ORIWDMA_CON0 */
	{0x52184, 0x10550045},	/* IMG3CO_D1A_ORIWDMA_CON1 */
	{0x52188, 0x10800080},	/* IMG3CO_D1A_ORIWDMA_CON2 */
	{0x52230, 0x10000020},	/* IMG3DO_D1A_ORIWDMA_CON0 */
	{0x52234, 0x10150011},	/* IMG3DO_D1A_ORIWDMA_CON1 */
	{0x52238, 0x10200020},	/* IMG3DO_D1A_ORIWDMA_CON2 */
	{0x22E0, 0x10000080},	/* IMG4O_D1A_ORIWDMA_CON0 */
	{0x22E4, 0x10550045},	/* IMG4O_D1A_ORIWDMA_CON1 */
	{0x22E8, 0x10800080},	/* IMG4O_D1A_ORIWDMA_CON2 */
	{0x2390, 0x10000040},	/* IMG4BO_D1A_ORIWDMA_CON0 */
	{0x2394, 0x102A0022},	/* IMG4BO_D1A_ORIWDMA_CON1 */
	{0x2398, 0x10400040},	/* IMG4BO_D1A_ORIWDMA_CON2 */
	{0x2440, 0x10000020},	/* IMG4CO_D1A_ORIWDMA_CON0 */
	{0x2444, 0x10150011},	/* IMG4CO_D1A_ORIWDMA_CON1 */
	{0x2448, 0x10200020},	/* IMG4CO_D1A_ORIWDMA_CON2 */
	{0x24F0, 0x10000020},	/* IMG4DO_D1A_ORIWDMA_CON0 */
	{0x24F4, 0x10150011},	/* IMG4DO_D1A_ORIWDMA_CON1 */
	{0x24F8, 0x10200020},	/* IMG4DO_D1A_ORIWDMA_CON2 */
	{0x525A0, 0x100000A0},	/* FEO_D1A_ORIWDMA_CON0 */
	{0x525A4, 0x106A0056},	/* FEO_D1A_ORIWDMA_CON1 */
	{0x525A8, 0x10A000A0},	/* FEO_D1A_ORIWDMA_CON2 */
	{0x52650, 0x10000080},	/* IMG2O_D1A_ULCWDMA_CON0 */
	{0x52654, 0x10550045},	/* IMG2O_D1A_ULCWDMA_CON1 */
	{0x52658, 0x10800080},	/* IMG2O_D1A_ULCWDMA_CON2 */
	{0x52690, 0x10000040},	/* IMG2BO_D1A_ULCWDMA_CON0 */
	{0x52694, 0x102A0022},	/* IMG2BO_D1A_ULCWDMA_CON1 */
	{0x52698, 0x10400040},	/* IMG2BO_D1A_ULCWDMA_CON2 */
	{0x26D0, 0x10000050},	/* TNRWO_D1A_ULCWDMA_CON0 */
	{0x26D4, 0x1035002B},	/* TNRWO_D1A_ULCWDMA_CON1 */
	{0x26D8, 0x10500050},	/* TNRWO_D1A_ULCWDMA_CON2 */
	{0x2710, 0x10000050},	/* TNRMO_D1A_ULCWDMA_CON0 */
	{0x2714, 0x1035002B},	/* TNRMO_D1A_ULCWDMA_CON1 */
	{0x2718, 0x10500050},	/* TNRMO_D1A_ULCWDMA_CON2 */
	{0x2750, 0x10000020},	/* TNRSO_D1A_ULCWDMA_CON0 */
	{0x2754, 0x10150011},	/* TNRSO_D1A_ULCWDMA_CON1 */
	{0x2758, 0x10200020},	/* TNRSO_D1A_ULCWDMA_CON2 */
	{0x2790, 0x10000020},	/* SMTO_D1A_ULCWDMA_CON0 */
	{0x2794, 0x10150011},	/* SMTO_D1A_ULCWDMA_CON1 */
	{0x2798, 0x10200020},	/* SMTO_D1A_ULCWDMA_CON2 */
	{0x27D0, 0x10000020},	/* SMTO_D2A_ULCWDMA_CON0 */
	{0x27D4, 0x10150011},	/* SMTO_D2A_ULCWDMA_CON1 */
	{0x27D8, 0x10200020},	/* SMTO_D2A_ULCWDMA_CON2 */
	{0x2810, 0x10000020},	/* SMTO_D3A_ULCWDMA_CON0 */
	{0x2814, 0x10150011},	/* SMTO_D3A_ULCWDMA_CON1 */
	{0x2818, 0x10200020},	/* SMTO_D3A_ULCWDMA_CON2 */
	{0x52850, 0x10000020},	/* SMTO_D4A_ULCWDMA_CON0 */
	{0x52854, 0x10150011},	/* SMTO_D4A_ULCWDMA_CON1 */
	{0x52858, 0x10200020},	/* SMTO_D4A_ULCWDMA_CON2 */
	{0x52890, 0x10000020},	/* SMTO_D5A_ULCWDMA_CON0 */
	{0x52894, 0x10150011},	/* SMTO_D5A_ULCWDMA_CON1 */
	{0x52898, 0x10200020},	/* SMTO_D5A_ULCWDMA_CON2 */
	{0x528D0, 0x10000020},	/* SMTO_D6A_ULCWDMA_CON0 */
	{0x528D4, 0x10150011},	/* SMTO_D6A_ULCWDMA_CON1 */
	{0x528D8, 0x10200020},	/* SMTO_D6A_ULCWDMA_CON2 */
	{0x52910, 0x10000020},	/* SMTO_D7A_ULCWDMA_CON0 */
	{0x52914, 0x10150011},	/* SMTO_D7A_ULCWDMA_CON1 */
	{0x52918, 0x10200020},	/* SMTO_D7A_ULCWDMA_CON2 */
	{0x2950, 0x10000020},	/* SMTO_D8A_ULCWDMA_CON0 */
	{0x2954, 0x10150011},	/* SMTO_D8A_ULCWDMA_CON1 */
	{0x2958, 0x10200020},	/* SMTO_D8A_ULCWDMA_CON2 */
	{0x52990, 0x10000020},	/* SMTO_D9A_ULCWDMA_CON0 */
	{0x52994, 0x10150011},	/* SMTO_D9A_ULCWDMA_CON1 */
	{0x52998, 0x10200020},	/* SMTO_D9A_ULCWDMA_CON2 */
	{0x2C10, 0x10000020}, /*SMTCO_D1A_ULCWDMA_CON0*/
	{0x2C14, 0x10150011}, /*SMTCO_D1A_ULCWDMA_CON1*/
	{0x2C18, 0x10200020}, /*SMTCO_D1A_ULCWDMA_CON2*/
	{0x2C50, 0x10000020}, /*SMTCO_D2A_ULCWDMA_CON0*/
	{0x2C54, 0x10150011}, /*SMTCO_D2A_ULCWDMA_CON1*/
	{0x2C58, 0x10200020}, /*SMTCO_D2A_ULCWDMA_CON2*/
	{0x2D90, 0x10000020}, /*SMTCO_D8A_ULCWDMA_CON0*/
	{0x2D94, 0x10150011}, /*SMTCO_D8A_ULCWDMA_CON1*/
	{0x2D98, 0x10200020}, /*SMTCO_D8A_ULCWDMA_CON2*/
	{0x52C90, 0x10000020},  /*SMTCO_D4A_ULCWDMA_CON0*/
	{0x52C94, 0x10150011},  /*SMTCO_D4A_ULCWDMA_CON1*/
	{0x52C98, 0x10200020},  /*SMTCO_D4A_ULCWDMA_CON2*/
	{0x52CD0, 0x10000020},  /*SMTCO_D5A_ULCWDMA_CON0*/
	{0x52CD4, 0x10150011},  /*SMTCO_D5A_ULCWDMA_CON1*/
	{0x52CD8, 0x10200020},  /*SMTCO_D5A_ULCWDMA_CON2*/
	{0x52D10, 0x10000020},  /*SMTCO_D6A_ULCWDMA_CON0*/
	{0x52D14, 0x10150011},  /*SMTCO_D6A_ULCWDMA_CON1*/
	{0x52D18, 0x10200020},  /*SMTCO_D6A_ULCWDMA_CON2*/
	{0x52D50, 0x10000020},  /*SMTCO_D7A_ULCWDMA_CON0*/
	{0x52D54, 0x10150011},  /*SMTCO_D7A_ULCWDMA_CON1*/
	{0x52D58, 0x10200020},  /*SMTCO_D7A_ULCWDMA_CON2*/
	{0x52DD0, 0x10000020},  /*SMTCO_D9A_ULCWDMA_CON0*/
	{0x52DD4, 0x10150011},  /*SMTCO_D9A_ULCWDMA_CON1*/
	{0x52DD8, 0x10200020},  /*SMTCO_D9A_ULCWDMA_CON2*/
	{0x1B10, 0x10000020},  /* SMTCI_D1A_ULCRDMA_CON0*/
	{0x1B14, 0x10150011},  /* SMTCI_D1A_ULCRDMA_CON1*/
	{0x1B18, 0x10200020},  /* SMTCI_D1A_ULCRDMA_CON2*/
	{0x1B50, 0x10000020},  /* SMTCI_D2A_ULCRDMA_CON0*/
	{0x1B54, 0x10150011},  /* SMTCI_D2A_ULCRDMA_CON1*/
	{0x1B58, 0x10200020},  /* SMTCI_D2A_ULCRDMA_CON2*/
	{0x1C90, 0x10000020},  /* SMTCI_D8A_ULCRDMA_CON0*/
	{0x1C94, 0x10150011},  /* SMTCI_D8A_ULCRDMA_CON1*/
	{0x1C98, 0x10200020},  /* SMTCI_D8A_ULCRDMA_CON2*/
	{0x1B90, 0x10000020},  /* SMTCI_D4A_ULCRDMA_CON0*/
	{0x1B94, 0x10150011},  /* SMTCI_D4A_ULCRDMA_CON1*/
	{0x1B98, 0x10200020},  /* SMTCI_D4A_ULCRDMA_CON2*/
	{0x1BD0, 0x10000020},  /* SMTCI_D5A_ULCRDMA_CON0*/
	{0x1BD4, 0x10150011},  /* SMTCI_D5A_ULCRDMA_CON1*/
	{0x1BD8, 0x10200020},  /* SMTCI_D5A_ULCRDMA_CON2*/
	{0x1C10, 0x10000020},  /* SMTCI_D6A_ULCRDMA_CON0*/
	{0x1C14, 0x10150011},  /* SMTCI_D6A_ULCRDMA_CON1*/
	{0x1C18, 0x10200020},  /* SMTCI_D6A_ULCRDMA_CON2*/
	{0x1C50, 0x10000020},  /* SMTCI_D7A_ULCRDMA_CON0*/
	{0x1C54, 0x10150011},  /* SMTCI_D7A_ULCRDMA_CON1*/
	{0x1C58, 0x10200020},  /* SMTCI_D7A_ULCRDMA_CON2*/
	{0x1CD0, 0x10000020},  /* SMTCI_D9A_ULCRDMA_CON0*/
	{0x1CD4, 0x10150011},  /* SMTCI_D9A_ULCRDMA_CON1*/
	{0x1CD8, 0x10200020},  /* SMTCI_D9A_ULCRDMA_CON2*/
	{0x529D0, 0x10000020},  /*TNCSO_D1A_ULCWDMA_CON0*/
	{0x529D4, 0x10150011},  /*TNCSO_D1A_ULCWDMA_CON1*/
	{0x529D8, 0x10200020},  /*TNCSO_D1A_ULCWDMA_CON2*/
	{0x52A10, 0x10000020},  /*TNCSBO_D1A_ULCWDMA_CON0*/
	{0x52A14, 0x10150011},  /*TNCSBO_D1A_ULCWDMA_CON1*/
	{0x52A18, 0x10200020},  /*TNCSBO_D1A_ULCWDMA_CON2*/
	{0x52A50, 0x10000020},  /*TNCSHO_D1A_ULCWDMA_CON0*/
	{0x52A54, 0x10150011},  /*TNCSHO_D1A_ULCWDMA_CON1*/
	{0x52A58, 0x10200020},  /*TNCSHO_D1A_ULCWDMA_CON2*/
	{0x52A90, 0x10000020},  /*TNCSYO_D1A_ULCWDMA_CON0*/
	{0x52A94, 0x10150011},  /*TNCSYO_D1A_ULCWDMA_CON1*/
	{0x52A98, 0x10200020},  /*TNCSYO_D1A_ULCWDMA_CON2*/
	{0x52AD0, 0x10000020},  /*TNCSTO_D1A_ULCWDMA_CON0*/
	{0x52AD4, 0x10150011},  /*TNCSTO_D1A_ULCWDMA_CON1*/
	{0x52AD8, 0x10200020},  /*TNCSTO_D1A_ULCWDMA_CON2*/
	{0x52B10, 0x10000020},  /*TNCSTO_D2A_ULCWDMA_CON0*/
	{0x52B14, 0x10150011},  /*TNCSTO_D2A_ULCWDMA_CON1*/
	{0x52B18, 0x10200020},  /*TNCSTO_D2A_ULCWDMA_CON2*/
	{0x52B50, 0x10000020},  /*TNCSTO_D3A_ULCWDMA_CON0*/
	{0x52B54, 0x10150011},  /*TNCSTO_D3A_ULCWDMA_CON1*/
	{0x52B58, 0x10200020},  /*TNCSTO_D3A_ULCWDMA_CON2*/
	{0x52B90, 0x10000020},  /*TNCO_D1A_ULCWDMA_CON0*/
	{0x52B94, 0x10150011},  /*TNCO_D1A_ULCWDMA_CON1*/
	{0x52B98, 0x10200020},  /*TNCO_D1A_ULCWDMA_CON2*/
	{0x52BD0, 0x10000020},  /*TNCBO_D1A_ULCWDMA_CON0*/
	{0x52BD4, 0x10150011},  /*TNCBO_D1A_ULCWDMA_CON1*/
	{0x52BD8, 0x10200020},  /*TNCBO_D1A_ULCWDMA_CON2*/
	{0x51D10, 0x10000020},  /*TNCSTI_D1A_ULCRDMA_CON0*/
	{0x51D14, 0x10150011},  /*TNCSTI_D1A_ULCRDMA_CON1*/
	{0x51D18, 0x10200020},  /*TNCSTI_D1A_ULCRDMA_CON2*/
	{0x51D50, 0x10000020},  /*TNCSTI_D2A_ULCRDMA_CON0*/
	{0x51D54, 0x10150011},  /*TNCSTI_D2A_ULCRDMA_CON1*/
	{0x51D58, 0x10200020},  /*TNCSTI_D2A_ULCRDMA_CON2*/
	{0x51D90, 0x10000020},  /*TNCSTI_D3A_ULCRDMA_CON0*/
	{0x51D94, 0x10150011},  /*TNCSTI_D3A_ULCRDMA_CON1*/
	{0x51D98, 0x10200020},  /*TNCSTI_D3A_ULCRDMA_CON2*/
	{0x51DD0, 0x10000020},  /*TNCSTI_D4A_ULCRDMA_CON0*/
	{0x51DD4, 0x10150011},  /*TNCSTI_D4A_ULCRDMA_CON1*/
	{0x51DD8, 0x10200020},  /*TNCSTI_D4A_ULCRDMA_CON2*/
	{0x51E10, 0x10000020},  /*TNCSTI_D5A_ULCRDMA_CON0*/
	{0x51E14, 0x10150011},  /*TNCSTI_D5A_ULCRDMA_CON1*/
	{0x51E18, 0x10200020},  /*TNCSTI_D5A_ULCRDMA_CON2*/
	{0x51E50, 0x10000020},  /*MASKI_D1A_ULCRDMA_CON0*/
	{0x51E54, 0x10150011},  /*MASKI_D1A_ULCRDMA_CON1*/
	{0x51E58, 0x10200020},  /*MASKI_D1A_ULCRDMA_CON2*/
	{0x51E90, 0x10000020},  /*DHZAI_D1A_ULCRDMA_CON0*/
	{0x51E94, 0x10150011},  /*DHZAI_D1A_ULCRDMA_CON1*/
	{0x51E98, 0x10200020},  /*DHZAI_D1A_ULCRDMA_CON2*/
	{0x51ED0, 0x10000020},  /*DHZGI_D1A_ULCRDMA_CON0*/
	{0x51ED4, 0x10150011},  /*DHZGI_D1A_ULCRDMA_CON1*/
	{0x51ED8, 0x10200020},  /*DHZGI_D1A_ULCRDMA_CON2*/
	{0x51F10, 0x10000020},  /*DHZDI_D1A_ULCRDMA_CON0*/
	{0x51F14, 0x10150011},  /*DHZDI_D1A_ULCRDMA_CON1*/
	{0x51F18, 0x10200020}  /*DHZDI_D1A_ULCRDMA_CON2*/
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

for (i = 0 ; i < sizeof(mtk_imgsys_dip_init_ary)/sizeof(struct mtk_imgsys_init_array) ; i++) {
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


	dev_info(imgsys_dev->dev, "%s: dump dip dmatop regs\n", __func__);
	for (i = N_DMATOP_OFFSET; i <= N_DMATOP_OFFSET + N_DMATOP_RANGE; i += 0x10) {
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
	for (i = N_RDMA_OFFSET; i <= N_RDMA_OFFSET + N_RDMA_RANGE; i += 0x10) {
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
	for (i = N_WDMA_OFFSET; i <= N_WDMA_OFFSET + N_WDMA_RANGE; i += 0x10) {
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
