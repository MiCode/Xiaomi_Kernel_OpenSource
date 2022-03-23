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
	{0x1000, 0x4000000},	/* DIPDMATOP_D1A_DIPDMATOP_SPECIAL_FUN_EN */
#ifdef ONEPIXEL_MODE
	{0x1224, 0x112A00F2},	/* IMGI_D1A_ORIRDMA_CON1 */
	{0x1228, 0x01C001C0},	/* IMGI_D1A_ORIRDMA_CON2 */
	{0x1294, 0x10950079},	/* IMGBI_D1A_ORIRDMA_CON1 */
	{0x1298, 0x00E000E0},	/* IMGBI_D1A_ORIRDMA_CON2 */
	{0x1304, 0x10950079},	/* IMGCI_D1A_ORIRDMA_CON1 */
	{0x1308, 0x00E000E0},	/* IMGCI_D1A_ORIRDMA_CON2 */
	{0x1374, 0x10150011},	/* IMGDI_D1A_ORIRDMA_CON1 */
	{0x1378, 0x00200020},	/* IMGDI_D1A_ORIRDMA_CON2 */
	{0x13E4, 0x10550045},	/* DEPI_D1A_ORIRDMA_CON1 */
	{0x13E8, 0x00800080},	/* DEPI_D1A_ORIRDMA_CON2 */
	{0x1454, 0x10550045},	/* DMGI_D1A_ORIRDMA_CON1 */
	{0x1458, 0x00800080},	/* DMGI_D1A_ORIRDMA_CON2 */
	{0x1614, 0x1060004E},	/* TNRWI_D1A_ULCRDMA_CON1 */
	{0x1618, 0x00900090},	/* TNRWI_D1A_ULCRDMA_CON2 */
	{0x1654, 0x1060004E},	/* TNRMI_D1A_ULCRDMA_CON1 */
	{0x1658, 0x00900090},	/* TNRMI_D1A_ULCRDMA_CON2 */
	{0x1694, 0x10150011},	/* TNRCI_D1A_ULCRDMA_CON1 */
	{0x1698, 0x00200020},	/* TNRCI_D1A_ULCRDMA_CON2 */
	{0x16D4, 0x1060004E},	/* TNRVBI_D1A_ULCRDMA_CON1 */
	{0x16D8, 0x00900090},	/* TNRVBI_D1A_ULCRDMA_CON2 */
	{0x1714, 0x10150011},	/* TNRLYI_D1A_ULCRDMA_CON1 */
	{0x1718, 0x00200020},	/* TNRLYI_D1A_ULCRDMA_CON2 */
	{0x1754, 0x10150011},	/* TNRLCI_D1A_ULCRDMA_CON1 */
	{0x1758, 0x00200020},	/* TNRLCI_D1A_ULCRDMA_CON2 */
	{0x1794, 0x10150011},	/* TNRSI_D1A_ULCRDMA_CON1 */
	{0x1798, 0x00200020},	/* TNRSI_D1A_ULCRDMA_CON2 */
	{0x17D4, 0x10950079},	/* RECI_D1A_ULCRDMA_CON1 */
	{0x17D8, 0x00E000E0},	/* RECI_D1A_ULCRDMA_CON2 */
	{0x1814, 0x104A003C},	/* RECBI_D1A_ULCRDMA_CON1 */
	{0x1818, 0x00700070},	/* RECBI_D1A_ULCRDMA_CON2 */
	{0x1854, 0x10950079},	/* RECI_D2A_ULCRDMA_CON1 */
	{0x1858, 0x00E000E0},	/* RECI_D2A_ULCRDMA_CON2 */
	{0x1894, 0x104A003C},	/* RECBI_D2A_ULCRDMA_CON1 */
	{0x1898, 0x00700070},	/* RECBI_D2A_ULCRDMA_CON2 */
	{0x18D4, 0x10150011},	/* SMTI_D1A_ULCRDMA_CON1 */
	{0x18D8, 0x00200020},	/* SMTI_D1A_ULCRDMA_CON2 */
	{0x1914, 0x10150011},	/* SMTI_D2A_ULCRDMA_CON1 */
	{0x1918, 0x00200020},	/* SMTI_D2A_ULCRDMA_CON2 */
	{0x1954, 0x10150011},	/* SMTI_D3A_ULCRDMA_CON1 */
	{0x1958, 0x00200020},	/* SMTI_D3A_ULCRDMA_CON2 */
	{0x1A94, 0x10150011},	/* SMTI_D8A_ULCRDMA_CON1 */
	{0x1A98, 0x00200020},	/* SMTI_D8A_ULCRDMA_CON2 */
	{0x22E4, 0x10950079},	/* IMG4O_D1A_ORIWDMA_CON1 */
	{0x22E8, 0x00E000E0},	/* IMG4O_D1A_ORIWDMA_CON2 */
	{0x2394, 0x104A003C},	/* IMG4BO_D1A_ORIWDMA_CON1 */
	{0x2398, 0x00700070},	/* IMG4BO_D1A_ORIWDMA_CON2 */
	{0x2444, 0x10150011},	/* IMG4CO_D1A_ORIWDMA_CON1 */
	{0x2448, 0x00200020},	/* IMG4CO_D1A_ORIWDMA_CON2 */
	{0x24F4, 0x10150011},	/* IMG4DO_D1A_ORIWDMA_CON1 */
	{0x24F8, 0x00200020},	/* IMG4DO_D1A_ORIWDMA_CON2 */
	{0x26D4, 0x1060004E},	/* TNRWO_D1A_ULCWDMA_CON1 */
	{0x26D8, 0x00900090},	/* TNRWO_D1A_ULCWDMA_CON2 */
	{0x2714, 0x1060004E},	/* TNRMO_D1A_ULCWDMA_CON1 */
	{0x2718, 0x00900090},	/* TNRMO_D1A_ULCWDMA_CON2 */
	{0x2754, 0x10150011},	/* TNRSO_D1A_ULCWDMA_CON1 */
	{0x2758, 0x00200020},	/* TNRSO_D1A_ULCWDMA_CON2 */
	{0x2794, 0x10150011},	/* SMTO_D1A_ULCWDMA_CON1 */
	{0x2798, 0x00200020},	/* SMTO_D1A_ULCWDMA_CON2 */
	{0x27D4, 0x10150011},	/* SMTO_D2A_ULCWDMA_CON1 */
	{0x27D8, 0x00200020},	/* SMTO_D2A_ULCWDMA_CON2 */
	{0x2814, 0x10150011},	/* SMTO_D3A_ULCWDMA_CON1 */
	{0x2818, 0x00200020},	/* SMTO_D3A_ULCWDMA_CON2 */
	{0x2954, 0x10150011},	/* SMTO_D8A_ULCWDMA_CON1 */
	{0x2958, 0x00200020},	/* SMTO_D8A_ULCWDMA_CON2 */
	{0x2C14, 0x10150011}, /*SMTCO_D1A_ULCWDMA_CON1*/
	{0x2C18, 0x00200020}, /*SMTCO_D1A_ULCWDMA_CON2*/
	{0x2C54, 0x10150011}, /*SMTCO_D2A_ULCWDMA_CON1*/
	{0x2C58, 0x00200020}, /*SMTCO_D2A_ULCWDMA_CON2*/
	{0x2D94, 0x10150011}, /*SMTCO_D8A_ULCWDMA_CON1*/
	{0x2D98, 0x00200020}, /*SMTCO_D8A_ULCWDMA_CON2*/
	{0x1B14, 0x10150011},  /* SMTCI_D1A_ULCRDMA_CON1*/
	{0x1B18, 0x00200020},  /* SMTCI_D1A_ULCRDMA_CON2*/
	{0x1B54, 0x10150011},  /* SMTCI_D2A_ULCRDMA_CON1*/
	{0x1B58, 0x00200020},  /* SMTCI_D2A_ULCRDMA_CON2*/
	{0x1C94, 0x10150011},  /* SMTCI_D8A_ULCRDMA_CON1*/
	{0x1C98, 0x00200020},  /* SMTCI_D8A_ULCRDMA_CON2*/
#else
	{0x1224, 0x125501E5},	/* IMGI_D1A_ORIRDMA_CON1 */
	{0x1228, 0x03800380},	/* IMGI_D1A_ORIRDMA_CON2 */
	{0x1294, 0x112A00F2},	/* IMGBI_D1A_ORIRDMA_CON1 */
	{0x1298, 0x01C001C0},	/* IMGBI_D1A_ORIRDMA_CON2 */
	{0x1304, 0x112A00F2},	/* IMGCI_D1A_ORIRDMA_CON1 */
	{0x1308, 0x01C001C0},	/* IMGCI_D1A_ORIRDMA_CON2 */
	{0x1374, 0x102A0022},	/* IMGDI_D1A_ORIRDMA_CON1 */
	{0x1378, 0x00400040},	/* IMGDI_D1A_ORIRDMA_CON2 */
	{0x13E4, 0x10AA008A},	/* DEPI_D1A_ORIRDMA_CON1 */
	{0x13E8, 0x01000100},	/* DEPI_D1A_ORIRDMA_CON2 */
	{0x1454, 0x10AA008A},	/* DMGI_D1A_ORIRDMA_CON1 */
	{0x1458, 0x01000100},	/* DMGI_D1A_ORIRDMA_CON2 */
	{0x1614, 0x10C0009C},	/* TNRWI_D1A_ULCRDMA_CON1 */
	{0x1618, 0x01200120},	/* TNRWI_D1A_ULCRDMA_CON2 */
	{0x1654, 0x10C0009C},	/* TNRMI_D1A_ULCRDMA_CON1 */
	{0x1658, 0x01200120},	/* TNRMI_D1A_ULCRDMA_CON2 */
	{0x1694, 0x102A0022},	/* TNRCI_D1A_ULCRDMA_CON1 */
	{0x1698, 0x00400040},	/* TNRCI_D1A_ULCRDMA_CON2 */
	{0x16D4, 0x10C0009C},	/* TNRVBI_D1A_ULCRDMA_CON1 */
	{0x16D8, 0x01200120},	/* TNRVBI_D1A_ULCRDMA_CON2 */
	{0x1714, 0x102A0022},	/* TNRLYI_D1A_ULCRDMA_CON1 */
	{0x1718, 0x00400040},	/* TNRLYI_D1A_ULCRDMA_CON2 */
	{0x1754, 0x102A0022},	/* TNRLCI_D1A_ULCRDMA_CON1 */
	{0x1758, 0x00400040},	/* TNRLCI_D1A_ULCRDMA_CON2 */
	{0x1794, 0x102A0022},	/* TNRSI_D1A_ULCRDMA_CON1 */
	{0x1798, 0x00400040},	/* TNRSI_D1A_ULCRDMA_CON2 */
	{0x17D4, 0x112A00F2},	/* RECI_D1A_ULCRDMA_CON1 */
	{0x17D8, 0x01C001C0},	/* RECI_D1A_ULCRDMA_CON2 */
	{0x1814, 0x10950079},	/* RECBI_D1A_ULCRDMA_CON1 */
	{0x1818, 0x00E000E0},	/* RECBI_D1A_ULCRDMA_CON2 */
	{0x1854, 0x112A00F2},	/* RECI_D2A_ULCRDMA_CON1 */
	{0x1858, 0x01C001C0},	/* RECI_D2A_ULCRDMA_CON2 */
	{0x1894, 0x10950079},	/* RECBI_D2A_ULCRDMA_CON1 */
	{0x1898, 0x00E000E0},	/* RECBI_D2A_ULCRDMA_CON2 */
	{0x18D4, 0x102A0022},	/* SMTI_D1A_ULCRDMA_CON1 */
	{0x18D8, 0x00400040},	/* SMTI_D1A_ULCRDMA_CON2 */
	{0x1914, 0x102A0022},	/* SMTI_D2A_ULCRDMA_CON1 */
	{0x1918, 0x00400040},	/* SMTI_D2A_ULCRDMA_CON2 */
	{0x1954, 0x102A0022},	/* SMTI_D3A_ULCRDMA_CON1 */
	{0x1958, 0x00400040},	/* SMTI_D3A_ULCRDMA_CON2 */
	{0x1A94, 0x102A0022},	/* SMTI_D8A_ULCRDMA_CON1 */
	{0x1A98, 0x00400040},	/* SMTI_D8A_ULCRDMA_CON2 */
	{0x22E4, 0x112A00F2},	/* IMG4O_D1A_ORIWDMA_CON1 */
	{0x22E8, 0x01C001C0},	/* IMG4O_D1A_ORIWDMA_CON2 */
	{0x2394, 0x10950079},	/* IMG4BO_D1A_ORIWDMA_CON1 */
	{0x2398, 0x00E000E0},	/* IMG4BO_D1A_ORIWDMA_CON2 */
	{0x2444, 0x102A0022},	/* IMG4CO_D1A_ORIWDMA_CON1 */
	{0x2448, 0x00400040},	/* IMG4CO_D1A_ORIWDMA_CON2 */
	{0x24F4, 0x102A0022},	/* IMG4DO_D1A_ORIWDMA_CON1 */
	{0x24F8, 0x00400040},	/* IMG4DO_D1A_ORIWDMA_CON2 */
	{0x26D4, 0x10C0009C},	/* TNRWO_D1A_ULCWDMA_CON1 */
	{0x26D8, 0x01200120},	/* TNRWO_D1A_ULCWDMA_CON2 */
	{0x2714, 0x10C0009C},	/* TNRMO_D1A_ULCWDMA_CON1 */
	{0x2718, 0x01200120},	/* TNRMO_D1A_ULCWDMA_CON2 */
	{0x2754, 0x102A0022},	/* TNRSO_D1A_ULCWDMA_CON1 */
	{0x2758, 0x00400040},	/* TNRSO_D1A_ULCWDMA_CON2 */
	{0x2794, 0x102A0022},	/* SMTO_D1A_ULCWDMA_CON1 */
	{0x2798, 0x00400040},	/* SMTO_D1A_ULCWDMA_CON2 */
	{0x27D4, 0x102A0022},	/* SMTO_D2A_ULCWDMA_CON1 */
	{0x27D8, 0x00400040},	/* SMTO_D2A_ULCWDMA_CON2 */
	{0x2814, 0x102A0022},	/* SMTO_D3A_ULCWDMA_CON1 */
	{0x2818, 0x00400040},	/* SMTO_D3A_ULCWDMA_CON2 */
	{0x2954, 0x102A0022},	/* SMTO_D8A_ULCWDMA_CON1 */
	{0x2958, 0x00400040},	/* SMTO_D8A_ULCWDMA_CON2 */
	{0x2C14, 0x102A0022}, /*SMTCO_D1A_ULCWDMA_CON1*/
	{0x2C18, 0x00400040}, /*SMTCO_D1A_ULCWDMA_CON2*/
	{0x2C54, 0x102A0022}, /*SMTCO_D2A_ULCWDMA_CON1*/
	{0x2C58, 0x00400040}, /*SMTCO_D2A_ULCWDMA_CON2*/
	{0x2D94, 0x102A0022}, /*SMTCO_D8A_ULCWDMA_CON1*/
	{0x2D98, 0x00400040}, /*SMTCO_D8A_ULCWDMA_CON2*/
	{0x1B14, 0x102A0022},  /* SMTCI_D1A_ULCRDMA_CON1*/
	{0x1B18, 0x00400040},  /* SMTCI_D1A_ULCRDMA_CON2*/
	{0x1B54, 0x102A0022},  /* SMTCI_D2A_ULCRDMA_CON1*/
	{0x1B58, 0x00400040},  /* SMTCI_D2A_ULCRDMA_CON2*/
	{0x1C94, 0x102A0022},  /* SMTCI_D8A_ULCRDMA_CON1*/
	{0x1C98, 0x00400040},  /* SMTCI_D8A_ULCRDMA_CON2*/
#endif
};

const struct mtk_imgsys_init_array mtk_imgsys_dip_init_ary_nr[] = {
	{0x1000, 0x4000000},	/* DIPDMATOPNR_D1A_DIPDMATOP_SPECIAL_FUN_EN */
#ifdef ONEPIXEL_MODE
	{0x14C4, 0x112A00F2},	/* VIPI_D1A_ORIRDMA_CON1 */
	{0x14C8, 0x01C001C0},	/* VIPI_D1A_ORIRDMA_CON2 */
	{0x1534, 0x10950079},	/* VIPBI_D1A_ORIRDMA_CON1 */
	{0x1538, 0x00E000E0},	/* VIPBI_D1A_ORIRDMA_CON2 */
	{0x15A4, 0x10950079},	/* VIPCI_D1A_ORIRDMA_CON1 */
	{0x15A8, 0x00E000E0},	/* VIPCI_D1A_ORIRDMA_CON2 */
	{0x1994, 0x10150011},	/* SMTI_D4A_ULCRDMA_CON1 */
	{0x1998, 0x00200020},	/* SMTI_D4A_ULCRDMA_CON2 */
	{0x19D4, 0x10150011},	/* SMTI_D5A_ULCRDMA_CON1 */
	{0x19D8, 0x00200020},	/* SMTI_D5A_ULCRDMA_CON2 */
	{0x1A14, 0x10150011},	/* SMTI_D6A_ULCRDMA_CON1 */
	{0x1A18, 0x00200020},	/* SMTI_D6A_ULCRDMA_CON2 */
	{0x1A54, 0x10150011},	/* SMTI_D7A_ULCRDMA_CON1 */
	{0x1A58, 0x00200020},	/* SMTI_D7A_ULCRDMA_CON2 */
	{0x1AD4, 0x10150011},	/* SMTI_D9A_ULCRDMA_CON1 */
	{0x1AD8, 0x00200020},	/* SMTI_D9A_ULCRDMA_CON2 */
	{0x2024, 0x112A00F2},	/* IMG3O_D1A_ORIWDMA_CON1 */
	{0x2028, 0x01C001C0},	/* IMG3O_D1A_ORIWDMA_CON2 */
	{0x20D4, 0x10950079},	/* IMG3BO_D1A_ORIWDMA_CON1 */
	{0x20D8, 0x00E000E0},	/* IMG3BO_D1A_ORIWDMA_CON2 */
	{0x2184, 0x10950079},	/* IMG3CO_D1A_ORIWDMA_CON1 */
	{0x2188, 0x00E000E0},	/* IMG3CO_D1A_ORIWDMA_CON2 */
	{0x2234, 0x10150011},	/* IMG3DO_D1A_ORIWDMA_CON1 */
	{0x2238, 0x00200020},	/* IMG3DO_D1A_ORIWDMA_CON2 */
	{0x25A4, 0x10AA008A},	/* FEO_D1A_ORIWDMA_CON1 */
	{0x25A8, 0x01000100},	/* FEO_D1A_ORIWDMA_CON2 */
	{0x2654, 0x10AA008A},	/* IMG2O_D1A_ULCWDMA_CON1 */
	{0x2658, 0x01000100},	/* IMG2O_D1A_ULCWDMA_CON2 */
	{0x2694, 0x10550045},	/* IMG2BO_D1A_ULCWDMA_CON1 */
	{0x2698, 0x00800080},	/* IMG2BO_D1A_ULCWDMA_CON2 */
	{0x2854, 0x10150011},	/* SMTO_D4A_ULCWDMA_CON1 */
	{0x2858, 0x00200020},	/* SMTO_D4A_ULCWDMA_CON2 */
	{0x2894, 0x10150011},	/* SMTO_D5A_ULCWDMA_CON1 */
	{0x2898, 0x00200020},	/* SMTO_D5A_ULCWDMA_CON2 */
	{0x28D4, 0x10150011},	/* SMTO_D6A_ULCWDMA_CON1 */
	{0x28D8, 0x00200020},	/* SMTO_D6A_ULCWDMA_CON2 */
	{0x2914, 0x10150011},	/* SMTO_D7A_ULCWDMA_CON1 */
	{0x2918, 0x00200020},	/* SMTO_D7A_ULCWDMA_CON2 */
	{0x2994, 0x10150011},	/* SMTO_D9A_ULCWDMA_CON1 */
	{0x2998, 0x00200020},	/* SMTO_D9A_ULCWDMA_CON2 */
	{0x2C94, 0x10150011},  /*SMTCO_D4A_ULCWDMA_CON1*/
	{0x2C98, 0x00200020},  /*SMTCO_D4A_ULCWDMA_CON2*/
	{0x2CD4, 0x10150011},  /*SMTCO_D5A_ULCWDMA_CON1*/
	{0x2CD8, 0x00200020},  /*SMTCO_D5A_ULCWDMA_CON2*/
	{0x2D14, 0x10150011},  /*SMTCO_D6A_ULCWDMA_CON1*/
	{0x2D18, 0x00200020},  /*SMTCO_D6A_ULCWDMA_CON2*/
	{0x2D54, 0x10150011},  /*SMTCO_D7A_ULCWDMA_CON1*/
	{0x2D58, 0x00200020},  /*SMTCO_D7A_ULCWDMA_CON2*/
	{0x2DD4, 0x10150011},  /*SMTCO_D9A_ULCWDMA_CON1*/
	{0x2DD8, 0x00200020},  /*SMTCO_D9A_ULCWDMA_CON2*/
	{0x1B94, 0x10150011},  /* SMTCI_D4A_ULCRDMA_CON1*/
	{0x1B98, 0x00200020},  /* SMTCI_D4A_ULCRDMA_CON2*/
	{0x1BD4, 0x10150011},  /* SMTCI_D5A_ULCRDMA_CON1*/
	{0x1BD8, 0x00200020},  /* SMTCI_D5A_ULCRDMA_CON2*/
	{0x1C14, 0x10150011},  /* SMTCI_D6A_ULCRDMA_CON1*/
	{0x1C18, 0x00200020},  /* SMTCI_D6A_ULCRDMA_CON2*/
	{0x1C54, 0x10150011},  /* SMTCI_D7A_ULCRDMA_CON1*/
	{0x1C58, 0x00200020},  /* SMTCI_D7A_ULCRDMA_CON2*/
	{0x1CD4, 0x10150011},  /* SMTCI_D9A_ULCRDMA_CON1*/
	{0x1CD8, 0x00200020},  /* SMTCI_D9A_ULCRDMA_CON2*/
	{0x29D4, 0x10AA008A},  /*TNCSO_D1A_ULCWDMA_CON1*/
	{0x29D8, 0x01000100},  /*TNCSO_D1A_ULCWDMA_CON2*/
	{0x2A14, 0x10400034},  /*TNCSBO_D1A_ULCWDMA_CON1*/
	{0x2A18, 0x00600060},  /*TNCSBO_D1A_ULCWDMA_CON2*/
	{0x2A54, 0x102A0022},  /*TNCSHO_D1A_ULCWDMA_CON1*/
	{0x2A58, 0x00400040},  /*TNCSHO_D1A_ULCWDMA_CON2*/
	{0x2A94, 0x102A0022},  /*TNCSYO_D1A_ULCWDMA_CON1*/
	{0x2A98, 0x00400040},  /*TNCSYO_D1A_ULCWDMA_CON2*/
	{0x2AD4, 0x10150011},  /*TNCSTO_D1A_ULCWDMA_CON1*/
	{0x2AD8, 0x00200020},  /*TNCSTO_D1A_ULCWDMA_CON2*/
	{0x2B14, 0x104A003C},  /*TNCSTO_D2A_ULCWDMA_CON1*/
	{0x2B18, 0x00700070},  /*TNCSTO_D2A_ULCWDMA_CON2*/
	{0x2B54, 0x10150011},  /*TNCSTO_D3A_ULCWDMA_CON1*/
	{0x2B58, 0x00200020},  /*TNCSTO_D3A_ULCWDMA_CON2*/
	{0x2B94, 0x10AA008A},  /*TNCO_D1A_ULCWDMA_CON1*/
	{0x2B98, 0x01000100},  /*TNCO_D1A_ULCWDMA_CON2*/
	{0x2BD4, 0x10550045},  /*TNCBO_D1A_ULCWDMA_CON1*/
	{0x2BD8, 0x00800080},  /*TNCBO_D1A_ULCWDMA_CON2*/
	{0x1D14, 0x10150011},  /*TNCSTI_D1A_ULCRDMA_CON1*/
	{0x1D18, 0x00200020},  /*TNCSTI_D1A_ULCRDMA_CON2*/
	{0x1D54, 0x104A003C},  /*TNCSTI_D2A_ULCRDMA_CON1*/
	{0x1D58, 0x00700070},  /*TNCSTI_D2A_ULCRDMA_CON2*/
	{0x1D94, 0x10150011},  /*TNCSTI_D3A_ULCRDMA_CON1*/
	{0x1D98, 0x00200020},  /*TNCSTI_D3A_ULCRDMA_CON2*/
	{0x1DD4, 0x10420036},  /*TNCSTI_D4A_ULCRDMA_CON1*/
	{0x1DD8, 0x00640064},  /*TNCSTI_D4A_ULCRDMA_CON2*/
	{0x1E14, 0x10150011},  /*TNCSTI_D5A_ULCRDMA_CON1*/
	{0x1E18, 0x00200020},  /*TNCSTI_D5A_ULCRDMA_CON2*/
	{0x1E54, 0x102A0022},  /*MASKI_D1A_ULCRDMA_CON1*/
	{0x1E58, 0x00400040},  /*MASKI_D1A_ULCRDMA_CON2*/
	{0x1E94, 0x10150011},  /*DHZAI_D1A_ULCRDMA_CON1*/
	{0x1E98, 0x00200020},  /*DHZAI_D1A_ULCRDMA_CON2*/
	{0x1ED4, 0x10150011},  /*DHZGI_D1A_ULCRDMA_CON1*/
	{0x1ED8, 0x00200020},  /*DHZGI_D1A_ULCRDMA_CON2*/
	{0x1F14, 0x10150011},  /*DHZDI_D1A_ULCRDMA_CON1*/
	{0x1F18, 0x00200020}  /*DHZDI_D1A_ULCRDMA_CON2*/
#else
	{0x14C4, 0x125501E5},	/* VIPI_D1A_ORIRDMA_CON1 */
	{0x14C8, 0x03800380},	/* VIPI_D1A_ORIRDMA_CON2 */
	{0x1534, 0x112A00F2},	/* VIPBI_D1A_ORIRDMA_CON1 */
	{0x1538, 0x01C001C0},	/* VIPBI_D1A_ORIRDMA_CON2 */
	{0x15A4, 0x112A00F2},	/* VIPCI_D1A_ORIRDMA_CON1 */
	{0x15A8, 0x01C001C0},	/* VIPCI_D1A_ORIRDMA_CON2 */
	{0x1994, 0x102A0022},	/* SMTI_D4A_ULCRDMA_CON1 */
	{0x1998, 0x00400040},	/* SMTI_D4A_ULCRDMA_CON2 */
	{0x19D4, 0x102A0022},	/* SMTI_D5A_ULCRDMA_CON1 */
	{0x19D8, 0x00400040},	/* SMTI_D5A_ULCRDMA_CON2 */
	{0x1A14, 0x102A0022},	/* SMTI_D6A_ULCRDMA_CON1 */
	{0x1A18, 0x00400040},	/* SMTI_D6A_ULCRDMA_CON2 */
	{0x1A54, 0x102A0022},	/* SMTI_D7A_ULCRDMA_CON1 */
	{0x1A58, 0x00400040},	/* SMTI_D7A_ULCRDMA_CON2 */
	{0x1AD4, 0x102A0022},	/* SMTI_D9A_ULCRDMA_CON1 */
	{0x1AD8, 0x00400040},	/* SMTI_D9A_ULCRDMA_CON2 */
	{0x2024, 0x125501E5},	/* IMG3O_D1A_ORIWDMA_CON1 */
	{0x2028, 0x03800380},	/* IMG3O_D1A_ORIWDMA_CON2 */
	{0x20D4, 0x112A00F2},	/* IMG3BO_D1A_ORIWDMA_CON1 */
	{0x20D8, 0x01C001C0},	/* IMG3BO_D1A_ORIWDMA_CON2 */
	{0x2184, 0x112A00F2},	/* IMG3CO_D1A_ORIWDMA_CON1 */
	{0x2188, 0x01C001C0},	/* IMG3CO_D1A_ORIWDMA_CON2 */
	{0x2234, 0x102A0022},	/* IMG3DO_D1A_ORIWDMA_CON1 */
	{0x2238, 0x00400040},	/* IMG3DO_D1A_ORIWDMA_CON2 */
	{0x25A4, 0x11550115},	/* FEO_D1A_ORIWDMA_CON1 */
	{0x25A8, 0x02000200},	/* FEO_D1A_ORIWDMA_CON2 */
	{0x2654, 0x11550115},	/* IMG2O_D1A_ULCWDMA_CON1 */
	{0x2658, 0x02000200},	/* IMG2O_D1A_ULCWDMA_CON2 */
	{0x2694, 0x10AA008A},	/* IMG2BO_D1A_ULCWDMA_CON1 */
	{0x2698, 0x01000100},	/* IMG2BO_D1A_ULCWDMA_CON2 */
	{0x2854, 0x102A0022},	/* SMTO_D4A_ULCWDMA_CON1 */
	{0x2858, 0x00400040},	/* SMTO_D4A_ULCWDMA_CON2 */
	{0x2894, 0x102A0022},	/* SMTO_D5A_ULCWDMA_CON1 */
	{0x2898, 0x00400040},	/* SMTO_D5A_ULCWDMA_CON2 */
	{0x28D4, 0x102A0022},	/* SMTO_D6A_ULCWDMA_CON1 */
	{0x28D8, 0x00400040},	/* SMTO_D6A_ULCWDMA_CON2 */
	{0x2914, 0x102A0022},	/* SMTO_D7A_ULCWDMA_CON1 */
	{0x2918, 0x00400040},	/* SMTO_D7A_ULCWDMA_CON2 */
	{0x2994, 0x102A0022},	/* SMTO_D9A_ULCWDMA_CON1 */
	{0x2998, 0x00400040},	/* SMTO_D9A_ULCWDMA_CON2 */
	{0x2C94, 0x102A0022},  /*SMTCO_D4A_ULCWDMA_CON1*/
	{0x2C98, 0x00400040},  /*SMTCO_D4A_ULCWDMA_CON2*/
	{0x2CD4, 0x102A0022},  /*SMTCO_D5A_ULCWDMA_CON1*/
	{0x2CD8, 0x00400040},  /*SMTCO_D5A_ULCWDMA_CON2*/
	{0x2D14, 0x102A0022},  /*SMTCO_D6A_ULCWDMA_CON1*/
	{0x2D18, 0x00400040},  /*SMTCO_D6A_ULCWDMA_CON2*/
	{0x2D54, 0x102A0022},  /*SMTCO_D7A_ULCWDMA_CON1*/
	{0x2D58, 0x00400040},  /*SMTCO_D7A_ULCWDMA_CON2*/
	{0x2DD4, 0x102A0022},  /*SMTCO_D9A_ULCWDMA_CON1*/
	{0x2DD8, 0x00400040},  /*SMTCO_D9A_ULCWDMA_CON2*/
	{0x1B94, 0x102A0022},  /* SMTCI_D4A_ULCRDMA_CON1*/
	{0x1B98, 0x00400040},  /* SMTCI_D4A_ULCRDMA_CON2*/
	{0x1BD4, 0x102A0022},  /* SMTCI_D5A_ULCRDMA_CON1*/
	{0x1BD8, 0x00400040},  /* SMTCI_D5A_ULCRDMA_CON2*/
	{0x1C14, 0x102A0022},  /* SMTCI_D6A_ULCRDMA_CON1*/
	{0x1C18, 0x00400040},  /* SMTCI_D6A_ULCRDMA_CON2*/
	{0x1C54, 0x102A0022},  /* SMTCI_D7A_ULCRDMA_CON1*/
	{0x1C58, 0x00400040},  /* SMTCI_D7A_ULCRDMA_CON2*/
	{0x1CD4, 0x102A0022},  /* SMTCI_D9A_ULCRDMA_CON1*/
	{0x1CD8, 0x00400040},  /* SMTCI_D9A_ULCRDMA_CON2*/
	{0x29D4, 0x10AA008A},  /*TNCSO_D1A_ULCWDMA_CON1*/
	{0x29D8, 0x01000100},  /*TNCSO_D1A_ULCWDMA_CON2*/
	{0x2A14, 0x10400034},  /*TNCSBO_D1A_ULCWDMA_CON1*/
	{0x2A18, 0x00600060},  /*TNCSBO_D1A_ULCWDMA_CON2*/
	{0x2A54, 0x102A0022},  /*TNCSHO_D1A_ULCWDMA_CON1*/
	{0x2A58, 0x00400040},  /*TNCSHO_D1A_ULCWDMA_CON2*/
	{0x2A94, 0x102A0022},  /*TNCSYO_D1A_ULCWDMA_CON1*/
	{0x2A98, 0x00400040},  /*TNCSYO_D1A_ULCWDMA_CON2*/
	{0x2AD4, 0x10150011},  /*TNCSTO_D1A_ULCWDMA_CON1*/
	{0x2AD8, 0x00200020},  /*TNCSTO_D1A_ULCWDMA_CON2*/
	{0x2B14, 0x104A003C},  /*TNCSTO_D2A_ULCWDMA_CON1*/
	{0x2B18, 0x00700070},  /*TNCSTO_D2A_ULCWDMA_CON2*/
	{0x2B54, 0x10150011},  /*TNCSTO_D3A_ULCWDMA_CON1*/
	{0x2B58, 0x00200020},  /*TNCSTO_D3A_ULCWDMA_CON2*/
	{0x2B94, 0x11550115},  /*TNCO_D1A_ULCWDMA_CON1*/
	{0x2B98, 0x02000200},  /*TNCO_D1A_ULCWDMA_CON2*/
	{0x2BD4, 0x10AA008A},  /*TNCBO_D1A_ULCWDMA_CON1*/
	{0x2BD8, 0x01000100},  /*TNCBO_D1A_ULCWDMA_CON2*/
	{0x1D14, 0x10150011},  /*TNCSTI_D1A_ULCRDMA_CON1*/
	{0x1D18, 0x00200020},  /*TNCSTI_D1A_ULCRDMA_CON2*/
	{0x1D54, 0x104A003C},  /*TNCSTI_D2A_ULCRDMA_CON1*/
	{0x1D58, 0x00700070},  /*TNCSTI_D2A_ULCRDMA_CON2*/
	{0x1D94, 0x10150011},  /*TNCSTI_D3A_ULCRDMA_CON1*/
	{0x1D98, 0x00200020},  /*TNCSTI_D3A_ULCRDMA_CON2*/
	{0x1DD4, 0x10420036},  /*TNCSTI_D4A_ULCRDMA_CON1*/
	{0x1DD8, 0x00640064},  /*TNCSTI_D4A_ULCRDMA_CON2*/
	{0x1E14, 0x10150011},  /*TNCSTI_D5A_ULCRDMA_CON1*/
	{0x1E18, 0x00200020},  /*TNCSTI_D5A_ULCRDMA_CON2*/
	{0x1E54, 0x10550045},  /*MASKI_D1A_ULCRDMA_CON1*/
	{0x1E58, 0x00800080},  /*MASKI_D1A_ULCRDMA_CON2*/
	{0x1E94, 0x102A0022},  /*DHZAI_D1A_ULCRDMA_CON1*/
	{0x1E98, 0x00400040},  /*DHZAI_D1A_ULCRDMA_CON2*/
	{0x1ED4, 0x102A0022},  /*DHZGI_D1A_ULCRDMA_CON1*/
	{0x1ED8, 0x00400040},  /*DHZGI_D1A_ULCRDMA_CON2*/
	{0x1F14, 0x102A0022},  /*DHZDI_D1A_ULCRDMA_CON1*/
	{0x1F18, 0x00400040}  /*DHZDI_D1A_ULCRDMA_CON2*/
#endif
};

#define DIP_HW_SET 2

void __iomem *gdipRegBA[DIP_HW_SET] = {0L};

void imgsys_dip_set_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	unsigned int hw_idx = 0, ary_idx = 0;

	pr_debug("%s: +\n", __func__);
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

	pr_debug("%s: -\n", __func__);
	dev_dbg(imgsys_dev->dev, "%s: -\n", __func__);
}
EXPORT_SYMBOL(imgsys_dip_set_initial_value);

void imgsys_dip_set_hw_initial_value(struct mtk_imgsys_dev *imgsys_dev)
{
	void __iomem *dipRegBA = 0L;
	void __iomem *ofset = NULL;
	unsigned int i;

	/* iomap registers */
	dipRegBA = gdipRegBA[0]; // dip: 0x15100000

for (i = 0 ; i < sizeof(mtk_imgsys_dip_init_ary)/sizeof(struct mtk_imgsys_init_array) ; i++) {
	ofset = dipRegBA + mtk_imgsys_dip_init_ary[i].ofset;
	writel(mtk_imgsys_dip_init_ary[i].val, ofset);
}

	dipRegBA = gdipRegBA[1]; // dip_nr: 0x15150000

for (i = 0 ; i < sizeof(mtk_imgsys_dip_init_ary_nr)/sizeof(struct mtk_imgsys_init_array) ; i++) {
	ofset = dipRegBA + mtk_imgsys_dip_init_ary_nr[i].ofset;
	writel(mtk_imgsys_dip_init_ary_nr[i].val, ofset);
}
}
EXPORT_SYMBOL(imgsys_dip_set_hw_initial_value);

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
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip dmatop regs\n", __func__);
	for (i = DMATOP_OFFSET; i <= DMATOP_OFFSET + DMATOP_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip rdma regs\n", __func__);
	for (i = RDMA_OFFSET; i <= RDMA_OFFSET + RDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip wdma regs\n", __func__);
	for (i = WDMA_OFFSET; i <= WDMA_OFFSET + WDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump nr3d ctl regs\n", __func__);
	for (i = NR3D_CTL_OFFSET; i <= NR3D_CTL_OFFSET + NR3D_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump tnr ctl regs\n", __func__);
	for (i = TNR_CTL_OFFSET; i <= TNR_CTL_OFFSET + TNR_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15100000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dipRegBA = gdipRegBA[1]; // dip_nr: 0x15150000
	dev_info(imgsys_dev->dev, "%s: dump mcrop regs\n", __func__);
	for (i = MCRP_OFFSET; i <= MCRP_OFFSET + MCRP_RANGE; i += 0x8) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}


	dev_info(imgsys_dev->dev, "%s: dump dip dmatop regs\n", __func__);
	for (i = N_DMATOP_OFFSET; i <= N_DMATOP_OFFSET + N_DMATOP_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip rdma regs\n", __func__);
	for (i = N_RDMA_OFFSET; i <= N_RDMA_OFFSET + N_RDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dev_info(imgsys_dev->dev, "%s: dump dip wdma regs\n", __func__);
	for (i = N_WDMA_OFFSET; i <= N_WDMA_OFFSET + N_WDMA_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(dipRegBA + i)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x4)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0x8)),
		(unsigned int)ioread32((void *)(dipRegBA + i + 0xc)));
	}

	dipRegBA = gdipRegBA[0]; // dip: 0x15100000

	/* Set DIPCTL_DBG_SEL[3:0] to 0x1 */
	/* Set DIPCTL_DBG_SEL[15:8] to 0x88 */
	/* Set DIPCTL_DBG_SEL[19:6] to 0x1*/
	dev_info(imgsys_dev->dev, "%s: dipctl_dbg_sel_tnc\n", __func__);
	iowrite32(0x18801, (void *)(dipRegBA + DIPCTL_DBG_SEL));
	for (i = 0; i < 17; i++) {
		iowrite32((0x1 + i), (void *)(gdipRegBA[1] + TNC_DEBUG_SET));
		dev_info(imgsys_dev->dev, "%s: tnc_debug: %08X", __func__,
		(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DBG_OUT)));
	}
	for (i = TNC_CTL_OFFSET; i <= TNC_CTL_OFFSET + TNC_CTL_RANGE; i += 0x10) {
		dev_info(imgsys_dev->dev, "%s: [0x%08X] 0x%08X 0x%08X 0x%08X 0x%08X",
		__func__, (unsigned int)(0x15150000 + i),
		(unsigned int)ioread32((void *)(gdipRegBA[1] + i)),
		(unsigned int)ioread32((void *)(gdipRegBA[1] + i + 0x4)),
		(unsigned int)ioread32((void *)(gdipRegBA[1] + i + 0x8)),
		(unsigned int)ioread32((void *)(gdipRegBA[1] + i + 0xc)));
	}



for (i = 0; i <= 6; i++) {
	iowrite32((0x15 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000015 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x115 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000115 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x215 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000215 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x315 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000315 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x415 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000415 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x515 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000515 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x615 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000615 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x715 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000715 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x815 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000815 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
}

for (i = 0; i <= 6; i++) {
	iowrite32((0x23 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000023 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x123 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000123 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x223 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000223 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x323 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000323 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x423 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000423 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x523 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000523 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x623 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000623 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x723 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000723 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x823 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000823 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x923 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000923 + i), (unsigned int)(0x15101074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
}

	dipRegBA = gdipRegBA[1]; // dip: 0x15150000

for (i = 0; i <= 9; i++) {
	iowrite32((0x3 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000003 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x103 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000103 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x203 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000203 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x303 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000303 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x403 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000403 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x503 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000503 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x603 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000603 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x703 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000703 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x803 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000803 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
}

for (i = 0; i <= 9; i++) {
	iowrite32((0x17 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000017 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x117 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000117 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x217 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000217 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x317 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000317 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x417 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000417 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x517 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000517 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x617 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000617 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x717 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000717 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x817 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000817 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
	iowrite32((0x917 + i), (void *)(dipRegBA + DIPCTL_DMA_DBG_SEL));
	dev_info(imgsys_dev->dev, "%s: [0x%08X](0x%08X,0x%08X)",
	__func__, (unsigned int)(0x00000917 + i), (unsigned int)(0x15151074),
	(unsigned int)ioread32((void *)(dipRegBA + DIPCTL_DMA_DBG_OUT)));
}

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
