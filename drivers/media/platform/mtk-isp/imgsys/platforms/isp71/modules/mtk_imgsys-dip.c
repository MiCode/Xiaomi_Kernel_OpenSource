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
	{0x1220, 0x10000380},	/* IMGI_D1A_ORIRDMA_CON0 */
	{0x1224, 0x13800380},	/* IMGI_D1A_ORIRDMA_CON1 */
	{0x1228, 0x13800380},	/* IMGI_D1A_ORIRDMA_CON2 */
	{0x1290, 0x100001C0},	/* IMGBI_D1A_ORIRDMA_CON0 */
	{0x1294, 0x11C001C0},	/* IMGBI_D1A_ORIRDMA_CON1 */
	{0x1298, 0x11C001C0},	/* IMGBI_D1A_ORIRDMA_CON2 */
	{0x1300, 0x100001C0},	/* IMGCI_D1A_ORIRDMA_CON0 */
	{0x1304, 0x11C001C0},	/* IMGCI_D1A_ORIRDMA_CON1 */
	{0x1308, 0x11C001C0},	/* IMGCI_D1A_ORIRDMA_CON2 */
	{0x1370, 0x10000040},	/* IMGDI_D1A_ORIRDMA_CON0 */
	{0x1374, 0x10040040},	/* IMGDI_D1A_ORIRDMA_CON1 */
	{0x1378, 0x10040040},	/* IMGDI_D1A_ORIRDMA_CON2 */
	{0x13E0, 0x10000100},	/* DEPI_D1A_ORIRDMA_CON0 */
	{0x13E4, 0x11000100},	/* DEPI_D1A_ORIRDMA_CON1 */
	{0x13E8, 0x11000100},	/* DEPI_D1A_ORIRDMA_CON2 */
	{0x1450, 0x10000100},	/* DMGI_D1A_ORIRDMA_CON0 */
	{0x1454, 0x11000100},	/* DMGI_D1A_ORIRDMA_CON1 */
	{0x1458, 0x11000100},	/* DMGI_D1A_ORIRDMA_CON2 */
	{0x1610, 0x10000120},	/* TNRWI_D1A_ULCRDMA_CON0 */
	{0x1614, 0x11200120},	/* TNRWI_D1A_ULCRDMA_CON1 */
	{0x1618, 0x11200120},	/* TNRWI_D1A_ULCRDMA_CON2 */
	{0x1650, 0x10000120},	/* TNRMI_D1A_ULCRDMA_CON0 */
	{0x1654, 0x11200120},	/* TNRMI_D1A_ULCRDMA_CON1 */
	{0x1658, 0x11200120},	/* TNRMI_D1A_ULCRDMA_CON2 */
	{0x1690, 0x10000040},	/* TNRCI_D1A_ULCRDMA_CON0 */
	{0x1694, 0x10400040},	/* TNRCI_D1A_ULCRDMA_CON1 */
	{0x1698, 0x10400040},	/* TNRCI_D1A_ULCRDMA_CON2 */
	{0x16D0, 0x10000120},	/* TNRVBI_D1A_ULCRDMA_CON0 */
	{0x16D4, 0x11200120},	/* TNRVBI_D1A_ULCRDMA_CON1 */
	{0x16D8, 0x11200120},	/* TNRVBI_D1A_ULCRDMA_CON2 */
	{0x1710, 0x10000040},	/* TNRLYI_D1A_ULCRDMA_CON0 */
	{0x1714, 0x10400040},	/* TNRLYI_D1A_ULCRDMA_CON1 */
	{0x1718, 0x10400040},	/* TNRLYI_D1A_ULCRDMA_CON2 */
	{0x1750, 0x10000040},	/* TNRLCI_D1A_ULCRDMA_CON0 */
	{0x1754, 0x10400040},	/* TNRLCI_D1A_ULCRDMA_CON1 */
	{0x1758, 0x10400040},	/* TNRLCI_D1A_ULCRDMA_CON2 */
	{0x1790, 0x10000040},	/* TNRSI_D1A_ULCRDMA_CON0 */
	{0x1794, 0x10400040},	/* TNRSI_D1A_ULCRDMA_CON1 */
	{0x1798, 0x10400040},	/* TNRSI_D1A_ULCRDMA_CON2 */
	{0x17D0, 0x100001C0},	/* RECI_D1A_ULCRDMA_CON0 */
	{0x17D4, 0x11C001C0},	/* RECI_D1A_ULCRDMA_CON1 */
	{0x17D8, 0x11C001C0},	/* RECI_D1A_ULCRDMA_CON2 */
	{0x1810, 0x100000E0},	/* RECBI_D1A_ULCRDMA_CON0 */
	{0x1814, 0x10E000E0},	/* RECBI_D1A_ULCRDMA_CON1 */
	{0x1818, 0x10E000E0},	/* RECBI_D1A_ULCRDMA_CON2 */
	{0x1850, 0x100001C0},	/* RECI_D2A_ULCRDMA_CON0 */
	{0x1854, 0x11C001C0},	/* RECI_D2A_ULCRDMA_CON1 */
	{0x1858, 0x11C001C0},	/* RECI_D2A_ULCRDMA_CON2 */
	{0x1890, 0x100000E0},	/* RECBI_D2A_ULCRDMA_CON0 */
	{0x1894, 0x10E000E0},	/* RECBI_D2A_ULCRDMA_CON1 */
	{0x1898, 0x10E000E0},	/* RECBI_D2A_ULCRDMA_CON2 */
	{0x18D0, 0x10000040},	/* SMTI_D1A_ULCRDMA_CON0 */
	{0x18D4, 0x10400040},	/* SMTI_D1A_ULCRDMA_CON1 */
	{0x18D8, 0x10400040},	/* SMTI_D1A_ULCRDMA_CON2 */
	{0x1910, 0x10000040},	/* SMTI_D2A_ULCRDMA_CON0 */
	{0x1914, 0x10400040},	/* SMTI_D2A_ULCRDMA_CON1 */
	{0x1918, 0x10400040},	/* SMTI_D2A_ULCRDMA_CON2 */
	{0x1950, 0x10000040},	/* SMTI_D3A_ULCRDMA_CON0 */
	{0x1954, 0x10400040},	/* SMTI_D3A_ULCRDMA_CON1 */
	{0x1958, 0x10400040},	/* SMTI_D3A_ULCRDMA_CON2 */
	{0x1A90, 0x10000040},	/* SMTI_D8A_ULCRDMA_CON0 */
	{0x1A94, 0x10400040},	/* SMTI_D8A_ULCRDMA_CON1 */
	{0x1A98, 0x10400040},	/* SMTI_D8A_ULCRDMA_CON2 */
	{0x22E0, 0x100001C0},	/* IMG4O_D1A_ORIWDMA_CON0 */
	{0x22E4, 0x11C001C0},	/* IMG4O_D1A_ORIWDMA_CON1 */
	{0x22E8, 0x11C001C0},	/* IMG4O_D1A_ORIWDMA_CON2 */
	{0x2390, 0x100000E0},	/* IMG4BO_D1A_ORIWDMA_CON0 */
	{0x2394, 0x10E000E0},	/* IMG4BO_D1A_ORIWDMA_CON1 */
	{0x2398, 0x10E000E0},	/* IMG4BO_D1A_ORIWDMA_CON2 */
	{0x2440, 0x10000040},	/* IMG4CO_D1A_ORIWDMA_CON0 */
	{0x2444, 0x10400040},	/* IMG4CO_D1A_ORIWDMA_CON1 */
	{0x2448, 0x10400040},	/* IMG4CO_D1A_ORIWDMA_CON2 */
	{0x24F0, 0x10000040},	/* IMG4DO_D1A_ORIWDMA_CON0 */
	{0x24F4, 0x10400040},	/* IMG4DO_D1A_ORIWDMA_CON1 */
	{0x24F8, 0x10400040},	/* IMG4DO_D1A_ORIWDMA_CON2 */
	{0x26D0, 0x10000120},	/* TNRWO_D1A_ULCWDMA_CON0 */
	{0x26D4, 0x11200120},	/* TNRWO_D1A_ULCWDMA_CON1 */
	{0x26D8, 0x11200120},	/* TNRWO_D1A_ULCWDMA_CON2 */
	{0x2710, 0x10000120},	/* TNRMO_D1A_ULCWDMA_CON0 */
	{0x2714, 0x11200120},	/* TNRMO_D1A_ULCWDMA_CON1 */
	{0x2718, 0x11200120},	/* TNRMO_D1A_ULCWDMA_CON2 */
	{0x2750, 0x10000040},	/* TNRSO_D1A_ULCWDMA_CON0 */
	{0x2754, 0x10400040},	/* TNRSO_D1A_ULCWDMA_CON1 */
	{0x2758, 0x10400040},	/* TNRSO_D1A_ULCWDMA_CON2 */
	{0x2790, 0x10000040},	/* SMTO_D1A_ULCWDMA_CON0 */
	{0x2794, 0x10400040},	/* SMTO_D1A_ULCWDMA_CON1 */
	{0x2798, 0x10400040},	/* SMTO_D1A_ULCWDMA_CON2 */
	{0x27D0, 0x10000040},	/* SMTO_D2A_ULCWDMA_CON0 */
	{0x27D4, 0x10400040},	/* SMTO_D2A_ULCWDMA_CON1 */
	{0x27D8, 0x10400040},	/* SMTO_D2A_ULCWDMA_CON2 */
	{0x2810, 0x10000040},	/* SMTO_D3A_ULCWDMA_CON0 */
	{0x2814, 0x10400040},	/* SMTO_D3A_ULCWDMA_CON1 */
	{0x2818, 0x10400040},	/* SMTO_D3A_ULCWDMA_CON2 */
	{0x2950, 0x10000040},	/* SMTO_D8A_ULCWDMA_CON0 */
	{0x2954, 0x10400040},	/* SMTO_D8A_ULCWDMA_CON1 */
	{0x2958, 0x10400040},	/* SMTO_D8A_ULCWDMA_CON2 */
	{0x2C10, 0x10000040}, /*SMTCO_D1A_ULCWDMA_CON0*/
	{0x2C14, 0x10400040}, /*SMTCO_D1A_ULCWDMA_CON1*/
	{0x2C18, 0x10400040}, /*SMTCO_D1A_ULCWDMA_CON2*/
	{0x2C50, 0x10000040}, /*SMTCO_D2A_ULCWDMA_CON0*/
	{0x2C54, 0x10400040}, /*SMTCO_D2A_ULCWDMA_CON1*/
	{0x2C58, 0x10400040}, /*SMTCO_D2A_ULCWDMA_CON2*/
	{0x2D90, 0x10000040}, /*SMTCO_D8A_ULCWDMA_CON0*/
	{0x2D94, 0x10400040}, /*SMTCO_D8A_ULCWDMA_CON1*/
	{0x2D98, 0x10400040}, /*SMTCO_D8A_ULCWDMA_CON2*/
	{0x1B10, 0x10000040},  /* SMTCI_D1A_ULCRDMA_CON0*/
	{0x1B14, 0x10400040},  /* SMTCI_D1A_ULCRDMA_CON1*/
	{0x1B18, 0x10400040},  /* SMTCI_D1A_ULCRDMA_CON2*/
	{0x1B50, 0x10000040},  /* SMTCI_D2A_ULCRDMA_CON0*/
	{0x1B54, 0x10400040},  /* SMTCI_D2A_ULCRDMA_CON1*/
	{0x1B58, 0x10400040},  /* SMTCI_D2A_ULCRDMA_CON2*/
	{0x1C90, 0x10000040},  /* SMTCI_D8A_ULCRDMA_CON0*/
	{0x1C94, 0x10400040},  /* SMTCI_D8A_ULCRDMA_CON1*/
	{0x1C98, 0x10400040},  /* SMTCI_D8A_ULCRDMA_CON2*/
};

const struct mtk_imgsys_init_array mtk_imgsys_dip_init_ary_nr[] = {
	{0x14C0, 0x10000380},	/* VIPI_D1A_ORIRDMA_CON0 */
	{0x14C4, 0x13800380},	/* VIPI_D1A_ORIRDMA_CON1 */
	{0x14C8, 0x13800380},	/* VIPI_D1A_ORIRDMA_CON2 */
	{0x1530, 0x100001C0},	/* VIPBI_D1A_ORIRDMA_CON0 */
	{0x1534, 0x11C001C0},	/* VIPBI_D1A_ORIRDMA_CON1 */
	{0x1538, 0x11C001C0},	/* VIPBI_D1A_ORIRDMA_CON2 */
	{0x15A0, 0x100001C0},	/* VIPCI_D1A_ORIRDMA_CON0 */
	{0x15A4, 0x11C001C0},	/* VIPCI_D1A_ORIRDMA_CON1 */
	{0x15A8, 0x11C001C0},	/* VIPCI_D1A_ORIRDMA_CON2 */
	{0x1990, 0x10000040},	/* SMTI_D4A_ULCRDMA_CON0 */
	{0x1994, 0x10400040},	/* SMTI_D4A_ULCRDMA_CON1 */
	{0x1998, 0x10400040},	/* SMTI_D4A_ULCRDMA_CON2 */
	{0x19D0, 0x10000040},	/* SMTI_D5A_ULCRDMA_CON0 */
	{0x19D4, 0x10400040},	/* SMTI_D5A_ULCRDMA_CON1 */
	{0x19D8, 0x10400040},	/* SMTI_D5A_ULCRDMA_CON2 */
	{0x1A10, 0x10000040},	/* SMTI_D6A_ULCRDMA_CON0 */
	{0x1A14, 0x10400040},	/* SMTI_D6A_ULCRDMA_CON1 */
	{0x1A18, 0x10400040},	/* SMTI_D6A_ULCRDMA_CON2 */
	{0x1A50, 0x10000040},	/* SMTI_D7A_ULCRDMA_CON0 */
	{0x1A54, 0x10400040},	/* SMTI_D7A_ULCRDMA_CON1 */
	{0x1A58, 0x10400040},	/* SMTI_D7A_ULCRDMA_CON2 */
	{0x1AD0, 0x10000040},	/* SMTI_D9A_ULCRDMA_CON0 */
	{0x1AD4, 0x10400040},	/* SMTI_D9A_ULCRDMA_CON1 */
	{0x1AD8, 0x10400040},	/* SMTI_D9A_ULCRDMA_CON2 */
	{0x2020, 0x10000380},	/* IMG3O_D1A_ORIWDMA_CON0 */
	{0x2024, 0x13800380},	/* IMG3O_D1A_ORIWDMA_CON1 */
	{0x2028, 0x13800380},	/* IMG3O_D1A_ORIWDMA_CON2 */
	{0x20D0, 0x100001C0},	/* IMG3BO_D1A_ORIWDMA_CON0 */
	{0x20D4, 0x11C001C0},	/* IMG3BO_D1A_ORIWDMA_CON1 */
	{0x20D8, 0x11C001C0},	/* IMG3BO_D1A_ORIWDMA_CON2 */
	{0x2180, 0x100001C0},	/* IMG3CO_D1A_ORIWDMA_CON0 */
	{0x2184, 0x11C001C0},	/* IMG3CO_D1A_ORIWDMA_CON1 */
	{0x2188, 0x11C001C0},	/* IMG3CO_D1A_ORIWDMA_CON2 */
	{0x2230, 0x10000040},	/* IMG3DO_D1A_ORIWDMA_CON0 */
	{0x2234, 0x10040040},	/* IMG3DO_D1A_ORIWDMA_CON1 */
	{0x2238, 0x10040040},	/* IMG3DO_D1A_ORIWDMA_CON2 */
	{0x25A0, 0x10000200},	/* FEO_D1A_ORIWDMA_CON0 */
	{0x25A4, 0x12000200},	/* FEO_D1A_ORIWDMA_CON1 */
	{0x25A8, 0x12000200},	/* FEO_D1A_ORIWDMA_CON2 */
	{0x2650, 0x10000200},	/* IMG2O_D1A_ULCWDMA_CON0 */
	{0x2654, 0x12000200},	/* IMG2O_D1A_ULCWDMA_CON1 */
	{0x2658, 0x12000200},	/* IMG2O_D1A_ULCWDMA_CON2 */
	{0x2690, 0x10000100},	/* IMG2BO_D1A_ULCWDMA_CON0 */
	{0x2694, 0x11000100},	/* IMG2BO_D1A_ULCWDMA_CON1 */
	{0x2698, 0x11000100},	/* IMG2BO_D1A_ULCWDMA_CON2 */
	{0x2850, 0x10000040},	/* SMTO_D4A_ULCWDMA_CON0 */
	{0x2854, 0x10400040},	/* SMTO_D4A_ULCWDMA_CON1 */
	{0x2858, 0x10400040},	/* SMTO_D4A_ULCWDMA_CON2 */
	{0x2890, 0x10000040},	/* SMTO_D5A_ULCWDMA_CON0 */
	{0x2894, 0x10400040},	/* SMTO_D5A_ULCWDMA_CON1 */
	{0x2898, 0x10400040},	/* SMTO_D5A_ULCWDMA_CON2 */
	{0x28D0, 0x10000040},	/* SMTO_D6A_ULCWDMA_CON0 */
	{0x28D4, 0x10400040},	/* SMTO_D6A_ULCWDMA_CON1 */
	{0x28D8, 0x10400040},	/* SMTO_D6A_ULCWDMA_CON2 */
	{0x2910, 0x10000040},	/* SMTO_D7A_ULCWDMA_CON0 */
	{0x2914, 0x10400040},	/* SMTO_D7A_ULCWDMA_CON1 */
	{0x2918, 0x10400040},	/* SMTO_D7A_ULCWDMA_CON2 */
	{0x2990, 0x10000040},	/* SMTO_D9A_ULCWDMA_CON0 */
	{0x2994, 0x10400040},	/* SMTO_D9A_ULCWDMA_CON1 */
	{0x2998, 0x10400040},	/* SMTO_D9A_ULCWDMA_CON2 */
	{0x2C90, 0x10000040},  /*SMTCO_D4A_ULCWDMA_CON0*/
	{0x2C94, 0x10400040},  /*SMTCO_D4A_ULCWDMA_CON1*/
	{0x2C98, 0x10400040},  /*SMTCO_D4A_ULCWDMA_CON2*/
	{0x2CD0, 0x10000040},  /*SMTCO_D5A_ULCWDMA_CON0*/
	{0x2CD4, 0x10400040},  /*SMTCO_D5A_ULCWDMA_CON1*/
	{0x2CD8, 0x10400040},  /*SMTCO_D5A_ULCWDMA_CON2*/
	{0x2D10, 0x10000040},  /*SMTCO_D6A_ULCWDMA_CON0*/
	{0x2D14, 0x10400040},  /*SMTCO_D6A_ULCWDMA_CON1*/
	{0x2D18, 0x10400040},  /*SMTCO_D6A_ULCWDMA_CON2*/
	{0x2D50, 0x10000040},  /*SMTCO_D7A_ULCWDMA_CON0*/
	{0x2D54, 0x10400040},  /*SMTCO_D7A_ULCWDMA_CON1*/
	{0x2D58, 0x10400040},  /*SMTCO_D7A_ULCWDMA_CON2*/
	{0x2DD0, 0x10000040},  /*SMTCO_D9A_ULCWDMA_CON0*/
	{0x2DD4, 0x10400040},  /*SMTCO_D9A_ULCWDMA_CON1*/
	{0x2DD8, 0x10400040},  /*SMTCO_D9A_ULCWDMA_CON2*/
	{0x1B90, 0x10000040},  /* SMTCI_D4A_ULCRDMA_CON0*/
	{0x1B94, 0x10400040},  /* SMTCI_D4A_ULCRDMA_CON1*/
	{0x1B98, 0x10400040},  /* SMTCI_D4A_ULCRDMA_CON2*/
	{0x1BD0, 0x10000040},  /* SMTCI_D5A_ULCRDMA_CON0*/
	{0x1BD4, 0x10400040},  /* SMTCI_D5A_ULCRDMA_CON1*/
	{0x1BD8, 0x10400040},  /* SMTCI_D5A_ULCRDMA_CON2*/
	{0x1C10, 0x10000040},  /* SMTCI_D6A_ULCRDMA_CON0*/
	{0x1C14, 0x10400040},  /* SMTCI_D6A_ULCRDMA_CON1*/
	{0x1C18, 0x10400040},  /* SMTCI_D6A_ULCRDMA_CON2*/
	{0x1C50, 0x10000040},  /* SMTCI_D7A_ULCRDMA_CON0*/
	{0x1C54, 0x10400040},  /* SMTCI_D7A_ULCRDMA_CON1*/
	{0x1C58, 0x10400040},  /* SMTCI_D7A_ULCRDMA_CON2*/
	{0x1CD0, 0x10000040},  /* SMTCI_D9A_ULCRDMA_CON0*/
	{0x1CD4, 0x10400040},  /* SMTCI_D9A_ULCRDMA_CON1*/
	{0x1CD8, 0x10400040},  /* SMTCI_D9A_ULCRDMA_CON2*/
	{0x29D0, 0x10000100},  /*TNCSO_D1A_ULCWDMA_CON0*/
	{0x29D4, 0x11000100},  /*TNCSO_D1A_ULCWDMA_CON1*/
	{0x29D8, 0x11000100},  /*TNCSO_D1A_ULCWDMA_CON2*/
	{0x2A10, 0x10000060},  /*TNCSBO_D1A_ULCWDMA_CON0*/
	{0x2A14, 0x10600060},  /*TNCSBO_D1A_ULCWDMA_CON1*/
	{0x2A18, 0x10600060},  /*TNCSBO_D1A_ULCWDMA_CON2*/
	{0x2A50, 0x10000040},  /*TNCSHO_D1A_ULCWDMA_CON0*/
	{0x2A54, 0x10400040},  /*TNCSHO_D1A_ULCWDMA_CON1*/
	{0x2A58, 0x10400040},  /*TNCSHO_D1A_ULCWDMA_CON2*/
	{0x2A90, 0x10000040},  /*TNCSYO_D1A_ULCWDMA_CON0*/
	{0x2A94, 0x10400040},  /*TNCSYO_D1A_ULCWDMA_CON1*/
	{0x2A98, 0x10400040},  /*TNCSYO_D1A_ULCWDMA_CON2*/
	{0x2AD0, 0x10000020},  /*TNCSTO_D1A_ULCWDMA_CON0*/
	{0x2AD4, 0x10200020},  /*TNCSTO_D1A_ULCWDMA_CON1*/
	{0x2AD8, 0x10200020},  /*TNCSTO_D1A_ULCWDMA_CON2*/
	{0x2B10, 0x10000070},  /*TNCSTO_D2A_ULCWDMA_CON0*/
	{0x2B14, 0x10700070},  /*TNCSTO_D2A_ULCWDMA_CON1*/
	{0x2B18, 0x10700070},  /*TNCSTO_D2A_ULCWDMA_CON2*/
	{0x2B50, 0x10000020},  /*TNCSTO_D3A_ULCWDMA_CON0*/
	{0x2B54, 0x10200020},  /*TNCSTO_D3A_ULCWDMA_CON1*/
	{0x2B58, 0x10200020},  /*TNCSTO_D3A_ULCWDMA_CON2*/
	{0x2B90, 0x10000200},  /*TNCO_D1A_ULCWDMA_CON0*/
	{0x2B94, 0x12000200},  /*TNCO_D1A_ULCWDMA_CON1*/
	{0x2B98, 0x12000200},  /*TNCO_D1A_ULCWDMA_CON2*/
	{0x2BD0, 0x10000100},  /*TNCBO_D1A_ULCWDMA_CON0*/
	{0x2BD4, 0x11000100},  /*TNCBO_D1A_ULCWDMA_CON1*/
	{0x2BD8, 0x11000100},  /*TNCBO_D1A_ULCWDMA_CON2*/
	{0x1D10, 0x10000020},  /*TNCSTI_D1A_ULCRDMA_CON0*/
	{0x1D14, 0x10200020},  /*TNCSTI_D1A_ULCRDMA_CON1*/
	{0x1D18, 0x10200020},  /*TNCSTI_D1A_ULCRDMA_CON2*/
	{0x1D50, 0x10000070},  /*TNCSTI_D2A_ULCRDMA_CON0*/
	{0x1D54, 0x10700070},  /*TNCSTI_D2A_ULCRDMA_CON1*/
	{0x1D58, 0x10700070},  /*TNCSTI_D2A_ULCRDMA_CON2*/
	{0x1D90, 0x10000020},  /*TNCSTI_D3A_ULCRDMA_CON0*/
	{0x1D94, 0x10200020},  /*TNCSTI_D3A_ULCRDMA_CON1*/
	{0x1D98, 0x10200020},  /*TNCSTI_D3A_ULCRDMA_CON2*/
	{0x1DD0, 0x10000064},  /*TNCSTI_D4A_ULCRDMA_CON0*/
	{0x1DD4, 0x10640064},  /*TNCSTI_D4A_ULCRDMA_CON1*/
	{0x1DD8, 0x10640064},  /*TNCSTI_D4A_ULCRDMA_CON2*/
	{0x1E10, 0x10000020},  /*TNCSTI_D5A_ULCRDMA_CON0*/
	{0x1E14, 0x10200020},  /*TNCSTI_D5A_ULCRDMA_CON1*/
	{0x1E18, 0x10200020},  /*TNCSTI_D5A_ULCRDMA_CON2*/
	{0x1E50, 0x10000080},  /*MASKI_D1A_ULCRDMA_CON0*/
	{0x1E54, 0x10800080},  /*MASKI_D1A_ULCRDMA_CON1*/
	{0x1E58, 0x10800080},  /*MASKI_D1A_ULCRDMA_CON2*/
	{0x1E90, 0x10000040},  /*DHZAI_D1A_ULCRDMA_CON0*/
	{0x1E94, 0x10400040},  /*DHZAI_D1A_ULCRDMA_CON1*/
	{0x1E98, 0x10400040},  /*DHZAI_D1A_ULCRDMA_CON2*/
	{0x1ED0, 0x10000040},  /*DHZGI_D1A_ULCRDMA_CON0*/
	{0x1ED4, 0x10400040},  /*DHZGI_D1A_ULCRDMA_CON1*/
	{0x1ED8, 0x10400040},  /*DHZGI_D1A_ULCRDMA_CON2*/
	{0x1F10, 0x10000040},  /*DHZDI_D1A_ULCRDMA_CON0*/
	{0x1F14, 0x10400040},  /*DHZDI_D1A_ULCRDMA_CON1*/
	{0x1F18, 0x10400040}  /*DHZDI_D1A_ULCRDMA_CON2*/
};

#define DIP_HW_SET 2

void __iomem *gdipRegBA[DIP_HW_SET] = {0L};

void imgsys_dip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *ofset = NULL;
	unsigned int i = 0;
	unsigned int hw_idx = 0, ary_idx = 0;

	pr_info("%s: +\n", __func__);
	dev_dbg(imgsys_dev->dev, "%s: +\n", __func__);

	for (hw_idx = REG_MAP_E_DIP; hw_idx <= REG_MAP_E_DIP_NR; hw_idx++) {
		/* iomap registers */
		ary_idx = hw_idx - REG_MAP_E_DIP;
		gdipRegBA[ary_idx] = of_iomap(imgsys_dev->dev->of_node, hw_idx);
		if (!gdipRegBA[ary_idx]) {
			dev_info(imgsys_dev->dev,
				"%s: error: unable to iomap dip_%d registers, devnode(%s).\n",
				__func__, hw_idx, imgsys_dev->dev->of_node->name);
			continue;
		}
	}

for (i = 0 ; i < sizeof(mtk_imgsys_dip_init_ary)/sizeof(struct mtk_imgsys_init_array) ; i++) {
	ofset = gdipRegBA[0] + mtk_imgsys_dip_init_ary[i].ofset;
	writel(mtk_imgsys_dip_init_ary[i].val, ofset);
}

for (i = 0 ; i < sizeof(mtk_imgsys_dip_init_ary_nr)/sizeof(struct mtk_imgsys_init_array) ; i++) {
	ofset = gdipRegBA[1] + mtk_imgsys_dip_init_ary_nr[i].ofset;
	writel(mtk_imgsys_dip_init_ary_nr[i].val, ofset);
}

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
	dipRegBA = gdipRegBA[0]; // dip: 0x15100000

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

	dev_info(imgsys_dev->dev, "%s: dump tnr ctl regs\n", __func__);
	for (i = TNR_CTL_OFFSET; i <= TNR_CTL_OFFSET + TNR_CTL_RANGE; i += 0x10) {
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

	dipRegBA = gdipRegBA[1]; // dip_nr: 0x15150000
	dev_info(imgsys_dev->dev, "%s: dump mcrop regs\n", __func__);
	for (i = MCRP_OFFSET; i <= MCRP_OFFSET + MCRP_RANGE; i += 0x8) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15150000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)));
	}


	dev_info(imgsys_dev->dev, "%s: dump dip dmatop regs\n", __func__);
	for (i = N_DMATOP_OFFSET; i <= N_DMATOP_OFFSET + N_DMATOP_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15150000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15150000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15150000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip rdma regs\n", __func__);
	for (i = N_RDMA_OFFSET; i <= N_RDMA_OFFSET + N_RDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15150000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15150000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15150000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip wdma regs\n", __func__);
	for (i = N_WDMA_OFFSET; i <= N_WDMA_OFFSET + N_WDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X][0x%08X 0x%08X]",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)(0x15150000 + i + 0x4),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)(0x15150000 + i + 0x8),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)(0x15150000 + i + 0xc),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dipRegBA = gdipRegBA[0]; // dip: 0x15100000

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

	//dev_dbg(imgsys_dev->dev, "%s: +\n",__func__);
	//
	pr_info("%s: -\n", __func__);
	dev_dbg(imgsys_dev->dev, "%s: -\n", __func__);
}
EXPORT_SYMBOL(imgsys_dip_debug_dump);

void imgsys_dip_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int i;

	pr_debug("%s: +\n", __func__);

	for (i = 0; i < DIP_HW_SET; i++) {
		iounmap(gdipRegBA[i]);
		gdipRegBA[i] = 0L;
	}

	pr_debug("%s: -\n", __func__);
}
EXPORT_SYMBOL(imgsys_dip_uninit);
