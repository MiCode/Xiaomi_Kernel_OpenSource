// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#include "devapc-mt6983.h"
#include "devapc-mtk-multi-ao.h"

static const struct mtk_device_info mt6983_devices_infra[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "APMIXEDSYS_APB_S", true},
	{0, 1, 1, "APMIXEDSYS_APB_S-1", true},
	{0, 2, 2, "RESERVED_APB_S", true},
	{0, 3, 3, "TOPCKGEN_APB_S", true},
	{0, 4, 4, "INFRACFG_AO_APB_S", true},
	{0, 5, 5, "GPIO_APB_S", true},
	{0, 6, 6, "DEVICE_APC_INFRA_AO_APB_S", true},
	{0, 7, 7, "BCRM_INFRA_AO_APB_S", true},
	{0, 8, 8, "DEBUG_CTRL_INFRA_AO_APB_S", true},
	{0, 9, 10, "TOP_MISC_APB_S", true},

	/* 10 */
	{0, 10, 11, "MBIST_AO_APB_S", true},
	{0, 11, 12, "DPMAIF_AO_APB_S", true},
	{0, 12, 13, "INFRA_SECURITY_AO_APB_S", true},
	{0, 13, 14, "TOPCKGEN_INFRA_CFG_APB_S", true},
	{0, 14, 15, "DRM_DEBUG_TOP_APB_S", true},
	{0, 15, 16, "EFUSE_DEBUG_AO_APB_S", true},
	{0, 16, 17, "PMSR_APB_S", true},
	{0, 17, 18, "INFRA2PERI_S", true},
	{0, 18, 19, "INFRA2PERI_S-1", true},
	{0, 19, 20, "INFRA2PERI_S-2", true},

	/* 20 */
	{0, 20, 21, "INFRA2MM_S", true},
	{0, 21, 22, "INFRA2MM_S-1", true},
	{0, 22, 23, "INFRA2MM_S-2", true},
	{0, 23, 24, "INFRA2MM_S-3", true},
	{0, 24, 25, "MFG_S_S-0", true},
	{0, 25, 26, "MFG_S_S-2", true},
	{0, 26, 27, "MFG_S_S-3", true},
	{0, 27, 28, "MFG_S_S-4", true},
	{0, 28, 29, "MFG_S_S-5", true},
	{0, 29, 30, "MFG_S_S-7", true},

	/* 30 */
	{0, 30, 31, "MFG_S_S-8", true},
	{0, 31, 32, "MFG_S_S-9", true},
	{0, 32, 33, "MFG_S_S-10", true},
	{0, 33, 34, "MFG_S_S-11", true},
	{0, 34, 35, "MFG_S_S-12", true},
	{0, 35, 36, "MFG_S_S-13", true},
	{0, 36, 37, "MFG_S_S-14", true},
	{0, 37, 38, "MFG_S_S-15", true},
	{0, 38, 39, "MFG_S_S-16", true},
	{0, 39, 40, "MFG_S_S-18", true},

	/* 40 */
	{0, 40, 41, "MFG_S_S-19", true},
	{0, 41, 42, "MFG_S_S-20", true},
	{0, 42, 43, "MFG_S_S-21", true},
	{0, 43, 44, "MFG_S_S-22", true},
	{0, 44, 45, "MFG_S_S-24", true},
	{0, 45, 46, "MFG_S_S-25", true},
	{0, 46, 47, "MFG_S_S-26", true},
	{0, 47, 48, "MFG_S_S-28", true},
	{0, 48, 49, "MFG_S_S-30", true},
	{0, 49, 50, "MFG_S_S-31", true},

	/* 50 */
	{0, 50, 51, "MFG_S_S-32", true},
	{0, 51, 52, "MFG_S_S-34", true},
	{0, 52, 53, "MFG_S_S-36", true},
	{0, 53, 54, "MFG_S_S-37", true},
	{0, 54, 55, "MFG_S_S-38", true},
	{0, 55, 56, "MFG_S_S-39", true},
	{0, 56, 57, "MFG_S_S-40", true},
	{0, 57, 58, "MFG_S_S-41", true},
	{0, 58, 59, "MFG_S_S-42", true},
	{0, 59, 60, "MFG_S_S-43", true},

	/* 60 */
	{0, 60, 61, "MFG_S_S-44", true},
	{0, 61, 62, "MFG_S_S-45", true},
	{0, 62, 63, "MFG_S_S-47", true},
	{0, 63, 64, "MFG_S_S-48", true},
	{0, 64, 65, "MFG_S_S-49", true},
	{0, 65, 66, "MFG_S_S-51", true},
	{0, 66, 67, "MFG_S_S-52", true},
	{0, 67, 68, "MFG_S_S-53", true},
	{0, 68, 69, "MFG_S_S-54", true},
	{0, 69, 70, "APU_S_S", true},

	/* 70 */
	{0, 70, 71, "APU_S_S-1", true},
	{0, 71, 72, "APU_S_S-2", true},
	{0, 72, 73, "APU_S_S-3", true},
	{0, 73, 74, "APU_S_S-4", true},
	{0, 74, 75, "APU_S_S-5", true},
	{0, 75, 76, "APU_S_S-6", true},
	{0, 76, 122, "L3C_S", true},
	{0, 77, 123, "L3C_S-1", true},
	{0, 78, 124, "L3C_S-20", true},
	{0, 79, 125, "L3C_S-21", true},

	/* 80 */
	{0, 80, 126, "L3C_S-22", true},
	{0, 81, 127, "L3C_S-23", true},
	{0, 82, 128, "L3C_S-24", true},
	{0, 83, 129, "L3C_S-25", true},
	{0, 84, 130, "L3C_S-26", true},
	{0, 85, 131, "L3C_S-27", true},
	{0, 86, 132, "L3C_S-28", true},
	{0, 87, 133, "L3C_S-29", true},
	{0, 88, 134, "L3C_S-30", true},
	{0, 89, 135, "L3C_S-31", true},

	/* 90 */
	{0, 90, 136, "L3C_S-32", true},
	{0, 91, 137, "L3C_S-33", true},
	{0, 92, 138, "L3C_S-34", true},
	{0, 93, 139, "L3C_S-35", true},
	{0, 94, 140, "L3C_S-36", true},
	{0, 95, 141, "L3C_S-37", true},
	{0, 96, 142, "L3C_S-38", true},
	{0, 97, 143, "L3C_S-39", true},
	{0, 98, 144, "L3C_S-40", true},
	{0, 99, 145, "L3C_S-41", true},

	/* 100 */
	{0, 100, 146, "L3C_S-42", true},
	{0, 101, 147, "L3C_S-43", true},
	{0, 102, 148, "L3C_S-44", true},
	{0, 103, 155, "VLPSYS_S", true},
	{0, 104, 160, "DEBUGSYS_APB_S", true},
	{0, 105, 161, "DPMAIF_PDN_APB_S", true},
	{0, 106, 162, "DPMAIF_PDN_APB_S-1", true},
	{0, 107, 163, "DPMAIF_PDN_APB_S-2", true},
	{0, 108, 164, "DPMAIF_PDN_APB_S-3", true},
	{0, 109, 165, "DEVICE_APC_INFRA_PDN_APB_S", true},

	/* 110 */
	{0, 110, 166, "DEBUG_TRACKER_APB_S", true},
	{0, 111, 167, "DEBUG_TRACKER_APB1_S", true},
	{0, 112, 168, "BCRM_INFRA_PDN_APB_S", true},
	{0, 113, 169, "EMI_M4_M_APB_S", true},
	{0, 114, 170, "EMI_M7_M_APB_S", true},
	{0, 115, 171, "EMI_M6_M_APB_S", true},
	{0, 116, 172, "RSI_slb0_APB_S", true},
	{0, 117, 173, "RSI_slb1_APB_S", true},
	{0, 118, 174, "M4_IOMMU0_APB_S", true},
	{0, 119, 175, "M4_IOMMU1_APB_S", true},

	/* 120 */
	{0, 120, 176, "M4_IOMMU2_APB_S", true},
	{0, 121, 177, "M4_IOMMU3_APB_S", true},
	{0, 122, 178, "M4_IOMMU4_APB_S", true},
	{0, 123, 179, "M6_IOMMU0_APB_S", true},
	{0, 124, 180, "M6_IOMMU1_APB_S", true},
	{0, 125, 181, "M6_IOMMU2_APB_S", true},
	{0, 126, 182, "M6_IOMMU3_APB_S", true},
	{0, 127, 183, "M6_IOMMU4_APB_S", true},
	{0, 128, 184, "M7_IOMMU0_APB_S", true},
	{0, 129, 185, "M7_IOMMU1_APB_S", true},

	/* 130 */
	{0, 130, 186, "M7_IOMMU2_APB_S", true},
	{0, 131, 187, "M7_IOMMU3_APB_S", true},
	{0, 132, 188, "M7_IOMMU4_APB_S", true},
	{0, 133, 189, "PTP_THERM_CTRL_APB_S", true},
	{0, 134, 190, "PTP_THERM_CTRL2_APB_S", true},
	{0, 135, 191, "SYS_CIRQ_APB_S", true},
	{0, 136, 192, "CCIF0_AP_APB_S", true},
	{0, 137, 193, "CCIF0_MD_APB_S", true},
	{0, 138, 194, "CCIF1_AP_APB_S", true},
	{0, 139, 195, "CCIF1_MD_APB_S", true},

	/* 140 */
	{0, 140, 196, "MBIST_PDN_APB_S", true},
	{0, 141, 197, "INFRACFG_PDN_APB_S", true},
	{0, 142, 198, "TRNG_APB_S", true},
	{0, 143, 199, "DX_CC_APB_S", true},
	{0, 144, 200, "CQ_DMA_APB_S", true},
	{0, 145, 201, "SRAMROM_APB_S", true},
	{0, 146, 202, "RESERVED_DVFS_PROC_APB_S", true},
	{0, 147, 204, "SYS_CIRQ2_APB_S", true},
	{0, 148, 205, "CCIF2_AP_APB_S", true},
	{0, 149, 206, "CCIF2_MD_APB_S", true},

	/* 150 */
	{0, 150, 207, "CCIF3_AP_APB_S", true},
	{0, 151, 208, "CCIF3_MD_APB_S", true},
	{0, 152, 209, "CCIF4_AP_APB_S", true},
	{0, 153, 210, "CCIF4_MD_APB_S", true},
	{0, 154, 211, "CCIF5_AP_APB_S", true},
	{0, 155, 212, "CCIF5_MD_APB_S", true},
	{0, 156, 213, "HWCCF_APB_S", true},
	{0, 157, 214, "INFRA_BUS_HRE_APB_S", true},
	{0, 158, 215, "IPI_APB_S", true},
	{1, 0, 77, "ADSPSYS_S", true},

	/* 160 */
	{1, 1, 78, "MD_AP_S", true},
	{1, 2, 79, "MD_AP_S-1", true},
	{1, 3, 80, "MD_AP_S-2", true},
	{1, 4, 81, "MD_AP_S-3", true},
	{1, 5, 82, "MD_AP_S-4", true},
	{1, 6, 83, "MD_AP_S-5", true},
	{1, 7, 84, "MD_AP_S-6", true},
	{1, 8, 85, "MD_AP_S-7", true},
	{1, 9, 86, "MD_AP_S-8", true},
	{1, 10, 87, "MD_AP_S-9", true},

	/* 170 */
	{1, 11, 88, "MD_AP_S-10", true},
	{1, 12, 89, "MD_AP_S-11", true},
	{1, 13, 90, "MD_AP_S-12", true},
	{1, 14, 91, "MD_AP_S-13", true},
	{1, 15, 92, "MD_AP_S-14", true},
	{1, 16, 93, "MD_AP_S-15", true},
	{1, 17, 94, "MD_AP_S-16", true},
	{1, 18, 95, "MD_AP_S-17", true},
	{1, 19, 96, "MD_AP_S-18", true},
	{1, 20, 97, "MD_AP_S-19", true},

	/* 180 */
	{1, 21, 98, "MD_AP_S-20", true},
	{1, 22, 99, "MD_AP_S-21", true},
	{1, 23, 100, "MD_AP_S-22", true},
	{1, 24, 101, "MD_AP_S-23", true},
	{1, 25, 102, "MD_AP_S-24", true},
	{1, 26, 103, "MD_AP_S-25", true},
	{1, 27, 104, "MD_AP_S-26", true},
	{1, 28, 105, "MD_AP_S-27", true},
	{1, 29, 106, "MD_AP_S-28", true},
	{1, 30, 107, "MD_AP_S-29", true},

	/* 190 */
	{1, 31, 108, "MD_AP_S-30", true},
	{1, 32, 109, "MD_AP_S-31", true},
	{1, 33, 110, "MD_AP_S-32", true},
	{1, 34, 111, "MD_AP_S-33", true},
	{1, 35, 112, "MD_AP_S-34", true},
	{1, 36, 113, "MD_AP_S-35", true},
	{1, 37, 114, "MD_AP_S-36", true},
	{1, 38, 115, "MD_AP_S-37", true},
	{1, 39, 116, "MD_AP_S-38", true},
	{1, 40, 117, "MD_AP_S-39", true},

	/* 200 */
	{1, 41, 118, "MD_AP_S-40", true},
	{1, 42, 119, "MD_AP_S-41", true},
	{1, 43, 120, "MD_AP_S-42", true},
	{2, 0, 121, "CONN_S", true},

	{-1, -1, 216, "OOB_way_en", true},
	{-1, -1, 217, "OOB_way_en", true},
	{-1, -1, 218, "OOB_way_en", true},
	{-1, -1, 219, "OOB_way_en", true},

	{-1, -1, 220, "OOB_way_en", true},
	{-1, -1, 221, "OOB_way_en", true},
	{-1, -1, 222, "OOB_way_en", true},
	{-1, -1, 223, "OOB_way_en", true},
	{-1, -1, 224, "OOB_way_en", true},
	{-1, -1, 225, "OOB_way_en", true},
	{-1, -1, 226, "OOB_way_en", true},
	{-1, -1, 227, "OOB_way_en", true},
	{-1, -1, 228, "OOB_way_en", true},
	{-1, -1, 229, "OOB_way_en", true},

	{-1, -1, 230, "OOB_way_en", true},
	{-1, -1, 231, "OOB_way_en", true},
	{-1, -1, 232, "OOB_way_en", true},
	{-1, -1, 233, "OOB_way_en", true},
	{-1, -1, 234, "OOB_way_en", true},
	{-1, -1, 235, "OOB_way_en", true},
	{-1, -1, 236, "OOB_way_en", true},
	{-1, -1, 237, "OOB_way_en", true},
	{-1, -1, 238, "OOB_way_en", true},
	{-1, -1, 239, "OOB_way_en", true},

	{-1, -1, 240, "OOB_way_en", true},
	{-1, -1, 241, "OOB_way_en", true},
	{-1, -1, 242, "OOB_way_en", true},
	{-1, -1, 243, "OOB_way_en", true},
	{-1, -1, 244, "OOB_way_en", true},
	{-1, -1, 245, "OOB_way_en", true},
	{-1, -1, 246, "OOB_way_en", true},
	{-1, -1, 247, "OOB_way_en", true},
	{-1, -1, 248, "OOB_way_en", true},
	{-1, -1, 249, "OOB_way_en", true},

	{-1, -1, 250, "OOB_way_en", true},
	{-1, -1, 251, "OOB_way_en", true},
	{-1, -1, 252, "OOB_way_en", true},
	{-1, -1, 253, "OOB_way_en", true},
	{-1, -1, 254, "OOB_way_en", true},
	{-1, -1, 255, "OOB_way_en", true},
	{-1, -1, 256, "OOB_way_en", true},
	{-1, -1, 257, "OOB_way_en", true},
	{-1, -1, 258, "OOB_way_en", true},
	{-1, -1, 259, "OOB_way_en", true},

	{-1, -1, 260, "OOB_way_en", true},
	{-1, -1, 261, "OOB_way_en", true},
	{-1, -1, 262, "OOB_way_en", true},
	{-1, -1, 263, "OOB_way_en", true},
	{-1, -1, 264, "OOB_way_en", true},
	{-1, -1, 265, "OOB_way_en", true},
	{-1, -1, 266, "OOB_way_en", true},
	{-1, -1, 267, "OOB_way_en", true},
	{-1, -1, 268, "OOB_way_en", true},
	{-1, -1, 269, "OOB_way_en", true},

	{-1, -1, 270, "OOB_way_en", true},
	{-1, -1, 271, "OOB_way_en", true},
	{-1, -1, 272, "OOB_way_en", true},
	{-1, -1, 273, "OOB_way_en", true},
	{-1, -1, 274, "OOB_way_en", true},
	{-1, -1, 275, "OOB_way_en", true},
	{-1, -1, 276, "OOB_way_en", true},
	{-1, -1, 277, "OOB_way_en", true},
	{-1, -1, 278, "OOB_way_en", true},
	{-1, -1, 279, "OOB_way_en", true},

	{-1, -1, 280, "OOB_way_en", true},
	{-1, -1, 281, "OOB_way_en", true},
	{-1, -1, 282, "OOB_way_en", true},
	{-1, -1, 283, "OOB_way_en", true},
	{-1, -1, 284, "OOB_way_en", true},
	{-1, -1, 285, "OOB_way_en", true},
	{-1, -1, 286, "OOB_way_en", true},
	{-1, -1, 287, "OOB_way_en", true},
	{-1, -1, 288, "OOB_way_en", true},
	{-1, -1, 289, "OOB_way_en", true},

	{-1, -1, 290, "OOB_way_en", true},
	{-1, -1, 291, "OOB_way_en", true},
	{-1, -1, 292, "OOB_way_en", true},
	{-1, -1, 293, "OOB_way_en", true},
	{-1, -1, 294, "OOB_way_en", true},
	{-1, -1, 295, "OOB_way_en", true},
	{-1, -1, 296, "OOB_way_en", true},
	{-1, -1, 297, "OOB_way_en", true},
	{-1, -1, 298, "OOB_way_en", true},
	{-1, -1, 299, "OOB_way_en", true},

	{-1, -1, 300, "OOB_way_en", true},
	{-1, -1, 301, "OOB_way_en", true},
	{-1, -1, 302, "OOB_way_en", true},
	{-1, -1, 303, "OOB_way_en", true},
	{-1, -1, 304, "OOB_way_en", true},
	{-1, -1, 305, "OOB_way_en", true},
	{-1, -1, 306, "OOB_way_en", true},
	{-1, -1, 307, "OOB_way_en", true},

	{-1, -1, 308, "Decode_error", true},
	{-1, -1, 309, "Decode_error", true},
	{-1, -1, 310, "Decode_error", true},
	{-1, -1, 311, "Decode_error", true},
	{-1, -1, 312, "Decode_error", true},
	{-1, -1, 313, "Decode_error", true},

	{-1, -1, 314, "SRAMROM", true},
	{-1, -1, 315, "reserve", false},
	{-1, -1, 316, "reserve", false},
	{-1, -1, 317, "reserve", false},
	{-1, -1, 318, "CQ_DMA", false},
	{-1, -1, 319, "DEVICE_APC_INFRA_AO", false},
	{-1, -1, 320, "DEVICE_APC_INFRA_PDN", false},
};

static const struct mtk_device_info mt6983_devices_infra1[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "DRAMC_MD32_S0_APB_S", true},
	{0, 1, 1, "DRAMC_MD32_S0_APB_S-1", true},
	{0, 2, 2, "DRAMC_MD32_S1_APB_S", true},
	{0, 3, 3, "DRAMC_MD32_S1_APB_S-1", true},
	{0, 4, 4, "DRAMC_MD32_S2_APB_S", true},
	{0, 5, 5, "DRAMC_MD32_S2_APB_S-1", true},
	{0, 6, 6, "DRAMC_MD32_S3_APB_S", true},
	{0, 7, 7, "DRAMC_MD32_S3_APB_S-1", true},
	{0, 8, 8, "BND_EAST_APB0_S", true},
	{0, 9, 9, "BND_EAST_APB1_S", true},

	/* 10 */
	{0, 10, 10, "BND_EAST_APB2_S", true},
	{0, 11, 11, "BND_EAST_APB3_S", true},
	{0, 12, 12, "BND_EAST_APB4_S", true},
	{0, 13, 13, "BND_EAST_APB5_S", true},
	{0, 14, 14, "BND_EAST_APB6_S", true},
	{0, 15, 15, "BND_EAST_APB7_S", true},
	{0, 16, 16, "BND_EAST_APB8_S", true},
	{0, 17, 17, "BND_EAST_APB9_S", true},
	{0, 18, 18, "BND_EAST_APB10_S", true},
	{0, 19, 19, "BND_EAST_APB11_S", true},

	/* 20 */
	{0, 20, 20, "BND_EAST_APB12_S", true},
	{0, 21, 21, "BND_EAST_APB13_S", true},
	{0, 22, 22, "BND_EAST_APB14_S", true},
	{0, 23, 23, "BND_EAST_APB15_S", true},
	{0, 24, 24, "BND_WEST_APB0_S", true},
	{0, 25, 25, "BND_WEST_APB1_S", true},
	{0, 26, 26, "BND_WEST_APB2_S", true},
	{0, 27, 27, "BND_WEST_APB3_S", true},
	{0, 28, 28, "BND_WEST_APB4_S", true},
	{0, 29, 29, "BND_WEST_APB5_S", true},

	/* 30 */
	{0, 30, 30, "BND_WEST_APB6_S", true},
	{0, 31, 31, "BND_WEST_APB7_S", true},
	{0, 32, 32, "BND_NORTH_APB0_S", true},
	{0, 33, 33, "BND_NORTH_APB1_S", true},
	{0, 34, 34, "BND_NORTH_APB2_S", true},
	{0, 35, 35, "BND_NORTH_APB3_S", true},
	{0, 36, 36, "BND_NORTH_APB4_S", true},
	{0, 37, 37, "BND_NORTH_APB5_S", true},
	{0, 38, 38, "BND_NORTH_APB6_S", true},
	{0, 39, 39, "BND_NORTH_APB7_S", true},

	/* 40 */
	{0, 40, 40, "BND_NORTH_APB8_S", true},
	{0, 41, 41, "BND_NORTH_APB9_S", true},
	{0, 42, 42, "BND_NORTH_APB10_S", true},
	{0, 43, 43, "BND_NORTH_APB11_S", true},
	{0, 44, 44, "BND_NORTH_APB12_S", true},
	{0, 45, 45, "BND_NORTH_APB13_S", true},
	{0, 46, 46, "BND_NORTH_APB14_S", true},
	{0, 47, 47, "BND_NORTH_APB15_S", true},
	{0, 48, 48, "BND_SOUTH_APB0_S", true},
	{0, 49, 49, "BND_SOUTH_APB1_S", true},

	/* 50 */
	{0, 50, 50, "BND_SOUTH_APB2_S", true},
	{0, 51, 51, "BND_SOUTH_APB3_S", true},
	{0, 52, 52, "BND_SOUTH_APB4_S", true},
	{0, 53, 53, "BND_SOUTH_APB5_S", true},
	{0, 54, 54, "BND_SOUTH_APB6_S", true},
	{0, 55, 55, "BND_SOUTH_APB7_S", true},
	{0, 56, 56, "BND_SOUTH_APB8_S", true},
	{0, 57, 57, "BND_SOUTH_APB9_S", true},
	{0, 58, 58, "BND_SOUTH_APB10_S", true},
	{0, 59, 59, "BND_SOUTH_APB11_S", true},

	/* 60 */
	{0, 60, 60, "BND_SOUTH_APB12_S", true},
	{0, 61, 61, "BND_SOUTH_APB13_S", true},
	{0, 62, 62, "BND_SOUTH_APB14_S", true},
	{0, 63, 63, "BND_SOUTH_APB15_S", true},
	{0, 64, 64, "BND_EAST_NORTH_APB0_S", true},
	{0, 65, 65, "BND_EAST_NORTH_APB1_S", true},
	{0, 66, 66, "BND_EAST_NORTH_APB2_S", true},
	{0, 67, 67, "BND_EAST_NORTH_APB3_S", true},
	{0, 68, 68, "BND_EAST_NORTH_APB4_S", true},
	{0, 69, 69, "BND_EAST_NORTH_APB5_S", true},

	/* 70 */
	{0, 70, 70, "BND_EAST_NORTH_APB6_S", true},
	{0, 71, 71, "BND_EAST_NORTH_APB7_S", true},
	{0, 72, 72, "DRAMC_CH0_TOP0_APB_S", true},
	{0, 73, 73, "DRAMC_CH0_TOP1_APB_S", true},
	{0, 74, 74, "DRAMC_CH0_TOP2_APB_S", true},
	{0, 75, 75, "DRAMC_CH0_TOP3_APB_S", true},
	{0, 76, 76, "DRAMC_CH0_TOP4_APB_S", true},
	{0, 77, 77, "DRAMC_CH0_TOP5_APB_S", true},
	{0, 78, 78, "DRAMC_CH0_TOP6_APB_S", true},
	{0, 79, 79, "DRAMC_CH1_TOP0_APB_S", true},

	/* 80 */
	{0, 80, 80, "DRAMC_CH1_TOP1_APB_S", true},
	{0, 81, 81, "DRAMC_CH1_TOP2_APB_S", true},
	{0, 82, 82, "DRAMC_CH1_TOP3_APB_S", true},
	{0, 83, 83, "DRAMC_CH1_TOP4_APB_S", true},
	{0, 84, 84, "DRAMC_CH1_TOP5_APB_S", true},
	{0, 85, 85, "DRAMC_CH1_TOP6_APB_S", true},
	{0, 86, 86, "DRAMC_CH2_TOP0_APB_S", true},
	{0, 87, 87, "DRAMC_CH2_TOP1_APB_S", true},
	{0, 88, 88, "DRAMC_CH2_TOP2_APB_S", true},
	{0, 89, 89, "DRAMC_CH2_TOP3_APB_S", true},

	/* 90 */
	{0, 90, 90, "DRAMC_CH2_TOP4_APB_S", true},
	{0, 91, 91, "DRAMC_CH2_TOP5_APB_S", true},
	{0, 92, 92, "DRAMC_CH2_TOP6_APB_S", true},
	{0, 93, 93, "DRAMC_CH3_TOP0_APB_S", true},
	{0, 94, 94, "DRAMC_CH3_TOP1_APB_S", true},
	{0, 95, 95, "DRAMC_CH3_TOP2_APB_S", true},
	{0, 96, 96, "DRAMC_CH3_TOP3_APB_S", true},
	{0, 97, 97, "DRAMC_CH3_TOP4_APB_S", true},
	{0, 98, 98, "DRAMC_CH3_TOP5_APB_S", true},
	{0, 99, 99, "DRAMC_CH3_TOP6_APB_S", true},

	/* 100 */
	{0, 100, 104, "NTH_EMI_MBIST_PDN_APB_S", true},
	{0, 101, 105, "INFRACFG_MEM_APB_S", true},
	{0, 102, 106, "EMI_APB_S", true},
	{0, 103, 107, "EMI_MPU_APB_S", true},
	{0, 104, 108, "DEVICE_MPU_PDN_APB_S", true},
	{0, 105, 109, "BCRM_FMEM_PDN_APB_S", true},
	{0, 106, 110, "FAKE_ENGINE_1_S", true},
	{0, 107, 111, "FAKE_ENGINE_0_S", true},
	{0, 108, 112, "EMI_SUB_INFRA_APB_S", true},
	{0, 109, 113, "EMI_MPU_SUB_INFRA_APB_S", true},

	/* 110 */
	{0, 110, 114, "DEVICE_MPU_PDN_SUB_INFRA_APB_S", true},
	{0, 111, 115, "MBIST_PDN_SUB_INFRA_APB_S", true},
	{0, 112, 116, "INFRACFG_MEM_SUB_INFRA_APB_S", true},
	{0, 113, 117, "BCRM_SUB_INFRA_AO_APB_S", true},
	{0, 114, 118, "DEBUG_CTRL_SUB_INFRA_AO_APB_S", true},
	{0, 115, 119, "BCRM_SUB_INFRA_PDN_APB_S", true},
	{0, 116, 120, "SSC_SUB_INFRA_APB0_S", true},
	{0, 117, 121, "SSC_SUB_INFRA_APB1_S", true},
	{0, 118, 122, "SSC_SUB_INFRA_APB2_S", true},
	{0, 119, 123, "INFRACFG_AO_MEM_SUB_INFRA_APB_S", true},

	/* 120 */
	{0, 120, 124, "SUB_FAKE_ENGINE_MM_S", true},
	{0, 121, 125, "SUB_FAKE_ENGINE_MDP_S", true},
	{0, 122, 126, "SSC_INFRA_APB0_S", true},
	{0, 123, 127, "SSC_INFRA_APB1_S", true},
	{0, 124, 128, "SSC_INFRA_APB2_S", true},
	{0, 125, 129, "INFRACFG_AO_MEM_APB_S", true},
	{0, 126, 130, "DEBUG_CTRL_FMEM_AO_APB_S", true},
	{0, 127, 131, "BCRM_FMEM_AO_APB_S", true},
	{0, 128, 132, "NEMI_RSI_APB_S", true},
	{0, 129, 133, "SEMI_RSI_APB_S", true},

	/* 130 */
	{0, 130, 134, "DEVICE_MPU_ACP_APB_S", true},
	{0, 131, 135, "NEMI_HRE_EMI_APB_S", true},
	{0, 132, 136, "SEMI_HRE_EMI_APB_S", true},
	{0, 133, 137, "NEMI_HRE_EMI_MPU_APB_S", true},
	{0, 134, 138, "SEMI_HRE_EMI_MPU_APB_S", true},
	{0, 135, 139, "NEMI_HRE_EMI_SLB_APB_S", true},
	{0, 136, 140, "SEMI_HRE_EMI_SLB_APB_S", true},
	{0, 137, 141, "NEMI_SMPU0_APB_S", true},
	{0, 138, 142, "SEMI_SMPU0_APB_S", true},
	{0, 139, 143, "NEMI_SMPU1_APB_S", true},

	/* 140 */
	{0, 140, 144, "SEMI_SMPU1_APB_S", true},
	{0, 141, 145, "NEMI_SMPU2_APB_S", true},
	{0, 142, 146, "SEMI_SMPU2_APB_S", true},
	{0, 143, 147, "NEMI_SLB_APB_S", true},
	{0, 144, 148, "SEMI_SLB_APB_S", true},
	{0, 145, 149, "NEMI_HRE_SMPU_APB_S", true},
	{0, 146, 150, "SEMI_HRE_SMPU_APB_S", true},
	{0, 147, 151, "BCRM_INFRA_PDN1_APB_S", true},
	{0, 148, 152, "DEVICE_APC_INFRA_PDN1_APB_S", true},
	{0, 149, 153, "BCRM_INFRA_AO1_APB_S", true},

	/* 150 */
	{0, 150, 154, "DEVICE_APC_INFRA_AO1_APB_S", true},
	{0, 151, 155, "DEBUG_CTRL_INFRA_AO1_APB_S", true},

	{-1, -1, 156, "OOB_way_en", true},
	{-1, -1, 157, "OOB_way_en", true},
	{-1, -1, 158, "OOB_way_en", true},
	{-1, -1, 159, "OOB_way_en", true},

	{-1, -1, 160, "OOB_way_en", true},
	{-1, -1, 161, "OOB_way_en", true},
	{-1, -1, 162, "OOB_way_en", true},
	{-1, -1, 163, "OOB_way_en", true},
	{-1, -1, 164, "OOB_way_en", true},
	{-1, -1, 165, "OOB_way_en", true},
	{-1, -1, 166, "OOB_way_en", true},
	{-1, -1, 167, "OOB_way_en", true},
	{-1, -1, 168, "OOB_way_en", true},
	{-1, -1, 169, "OOB_way_en", true},

	{-1, -1, 170, "OOB_way_en", true},
	{-1, -1, 171, "OOB_way_en", true},
	{-1, -1, 172, "OOB_way_en", true},
	{-1, -1, 173, "OOB_way_en", true},
	{-1, -1, 174, "OOB_way_en", true},
	{-1, -1, 175, "OOB_way_en", true},
	{-1, -1, 176, "OOB_way_en", true},
	{-1, -1, 177, "OOB_way_en", true},
	{-1, -1, 178, "OOB_way_en", true},
	{-1, -1, 179, "OOB_way_en", true},

	{-1, -1, 180, "OOB_way_en", true},
	{-1, -1, 181, "OOB_way_en", true},
	{-1, -1, 182, "OOB_way_en", true},
	{-1, -1, 183, "OOB_way_en", true},
	{-1, -1, 184, "OOB_way_en", true},
	{-1, -1, 185, "OOB_way_en", true},
	{-1, -1, 186, "OOB_way_en", true},
	{-1, -1, 187, "OOB_way_en", true},
	{-1, -1, 188, "OOB_way_en", true},
	{-1, -1, 189, "OOB_way_en", true},

	{-1, -1, 190, "OOB_way_en", true},
	{-1, -1, 191, "OOB_way_en", true},
	{-1, -1, 192, "OOB_way_en", true},
	{-1, -1, 193, "OOB_way_en", true},
	{-1, -1, 194, "OOB_way_en", true},
	{-1, -1, 195, "OOB_way_en", true},
	{-1, -1, 196, "OOB_way_en", true},
	{-1, -1, 197, "OOB_way_en", true},
	{-1, -1, 198, "OOB_way_en", true},
	{-1, -1, 199, "OOB_way_en", true},

	{-1, -1, 200, "OOB_way_en", true},
	{-1, -1, 201, "OOB_way_en", true},
	{-1, -1, 202, "OOB_way_en", true},
	{-1, -1, 203, "OOB_way_en", true},
	{-1, -1, 204, "OOB_way_en", true},
	{-1, -1, 205, "OOB_way_en", true},
	{-1, -1, 206, "OOB_way_en", true},
	{-1, -1, 207, "OOB_way_en", true},
	{-1, -1, 208, "OOB_way_en", true},
	{-1, -1, 209, "OOB_way_en", true},

	{-1, -1, 210, "OOB_way_en", true},
	{-1, -1, 211, "OOB_way_en", true},
	{-1, -1, 212, "OOB_way_en", true},
	{-1, -1, 213, "OOB_way_en", true},
	{-1, -1, 214, "OOB_way_en", true},
	{-1, -1, 215, "OOB_way_en", true},
	{-1, -1, 216, "OOB_way_en", true},
	{-1, -1, 217, "OOB_way_en", true},
	{-1, -1, 218, "OOB_way_en", true},
	{-1, -1, 219, "OOB_way_en", true},

	{-1, -1, 220, "OOB_way_en", true},
	{-1, -1, 221, "OOB_way_en", true},
	{-1, -1, 222, "OOB_way_en", true},
	{-1, -1, 223, "OOB_way_en", true},
	{-1, -1, 224, "OOB_way_en", true},
	{-1, -1, 225, "OOB_way_en", true},
	{-1, -1, 226, "OOB_way_en", true},
	{-1, -1, 227, "OOB_way_en", true},
	{-1, -1, 228, "OOB_way_en", true},
	{-1, -1, 229, "OOB_way_en", true},

	{-1, -1, 230, "OOB_way_en", true},
	{-1, -1, 231, "OOB_way_en", true},
	{-1, -1, 232, "OOB_way_en", true},
	{-1, -1, 233, "OOB_way_en", true},
	{-1, -1, 234, "OOB_way_en", true},
	{-1, -1, 235, "OOB_way_en", true},
	{-1, -1, 236, "OOB_way_en", true},
	{-1, -1, 237, "OOB_way_en", true},
	{-1, -1, 238, "OOB_way_en", true},
	{-1, -1, 239, "OOB_way_en", true},

	{-1, -1, 240, "OOB_way_en", true},
	{-1, -1, 241, "OOB_way_en", true},
	{-1, -1, 242, "OOB_way_en", true},
	{-1, -1, 243, "OOB_way_en", true},
	{-1, -1, 244, "OOB_way_en", true},
	{-1, -1, 245, "OOB_way_en", true},
	{-1, -1, 246, "OOB_way_en", true},
	{-1, -1, 247, "OOB_way_en", true},
	{-1, -1, 248, "OOB_way_en", true},
	{-1, -1, 249, "OOB_way_en", true},

	{-1, -1, 250, "OOB_way_en", true},
	{-1, -1, 251, "OOB_way_en", true},
	{-1, -1, 252, "OOB_way_en", true},
	{-1, -1, 253, "OOB_way_en", true},
	{-1, -1, 254, "OOB_way_en", true},
	{-1, -1, 255, "OOB_way_en", true},
	{-1, -1, 256, "OOB_way_en", true},
	{-1, -1, 257, "OOB_way_en", true},
	{-1, -1, 258, "OOB_way_en", true},
	{-1, -1, 259, "OOB_way_en", true},

	{-1, -1, 260, "OOB_way_en", true},
	{-1, -1, 261, "OOB_way_en", true},
	{-1, -1, 262, "OOB_way_en", true},
	{-1, -1, 263, "OOB_way_en", true},
	{-1, -1, 264, "OOB_way_en", true},
	{-1, -1, 265, "OOB_way_en", true},
	{-1, -1, 266, "OOB_way_en", true},
	{-1, -1, 267, "OOB_way_en", true},
	{-1, -1, 268, "OOB_way_en", true},
	{-1, -1, 269, "OOB_way_en", true},

	{-1, -1, 270, "OOB_way_en", true},
	{-1, -1, 271, "OOB_way_en", true},
	{-1, -1, 272, "OOB_way_en", true},
	{-1, -1, 273, "OOB_way_en", true},
	{-1, -1, 274, "OOB_way_en", true},
	{-1, -1, 275, "OOB_way_en", true},
	{-1, -1, 276, "OOB_way_en", true},
	{-1, -1, 277, "OOB_way_en", true},
	{-1, -1, 278, "OOB_way_en", true},
	{-1, -1, 279, "OOB_way_en", true},

	{-1, -1, 280, "OOB_way_en", true},
	{-1, -1, 281, "OOB_way_en", true},
	{-1, -1, 282, "OOB_way_en", true},
	{-1, -1, 283, "OOB_way_en", true},
	{-1, -1, 284, "OOB_way_en", true},
	{-1, -1, 285, "OOB_way_en", true},
	{-1, -1, 286, "OOB_way_en", true},
	{-1, -1, 287, "OOB_way_en", true},
	{-1, -1, 288, "OOB_way_en", true},
	{-1, -1, 289, "OOB_way_en", true},

	{-1, -1, 290, "OOB_way_en", true},
	{-1, -1, 291, "OOB_way_en", true},
	{-1, -1, 292, "OOB_way_en", true},
	{-1, -1, 293, "OOB_way_en", true},
	{-1, -1, 294, "OOB_way_en", true},
	{-1, -1, 295, "OOB_way_en", true},
	{-1, -1, 296, "OOB_way_en", true},
	{-1, -1, 297, "OOB_way_en", true},
	{-1, -1, 298, "OOB_way_en", true},
	{-1, -1, 299, "OOB_way_en", true},

	{-1, -1, 300, "OOB_way_en", true},
	{-1, -1, 301, "OOB_way_en", true},
	{-1, -1, 302, "OOB_way_en", true},
	{-1, -1, 303, "OOB_way_en", true},
	{-1, -1, 304, "OOB_way_en", true},
	{-1, -1, 305, "OOB_way_en", true},
	{-1, -1, 306, "OOB_way_en", true},
	{-1, -1, 307, "OOB_way_en", true},
	{-1, -1, 308, "OOB_way_en", true},
	{-1, -1, 309, "OOB_way_en", true},

	{-1, -1, 310, "OOB_way_en", true},
	{-1, -1, 311, "OOB_way_en", true},
	{-1, -1, 312, "OOB_way_en", true},

	{-1, -1, 313, "Decode_error", true},
	{-1, -1, 314, "Decode_error", true},
	{-1, -1, 315, "Decode_error", true},
	{-1, -1, 316, "Decode_error", true},
	{-1, -1, 317, "Decode_error", true},
	{-1, -1, 318, "Decode_error", true},
	{-1, -1, 319, "Decode_error", true},
	{-1, -1, 320, "Decode_error", true},
	{-1, -1, 321, "Decode_error", true},

	{-1, -1, 322, "North EMI", false},
	{-1, -1, 323, "South EMI", false},
	{-1, -1, 324, "South EMI MPU", false},
	{-1, -1, 325, "North EMI MPU", false},
	{-1, -1, 326, "DEVICE_APC_INFRA_AO1", false},
	{-1, -1, 327, "DEVICE_APC_INFRA_PDN1", false},
};

static const struct mtk_device_info mt6983_devices_peri_par[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "UART0_APB_S", true},
	{0, 1, 1, "UART1_APB_S", true},
	{0, 2, 2, "UART2_APB_S", true},
	{0, 3, 3, "UART3_APB_S", true},
	{0, 4, 4, "PWM_PERI_APB_S", true},
	{0, 5, 5, "BTIF_APB_S", true},
	{0, 6, 6, "DISP_PWM0_APB_S", true},
	{0, 7, 7, "DISP_PWM1_APB_S", true},
	{0, 8, 8, "SPI0_APB_S", true},
	{0, 9, 9, "SPI1_APB_S", true},

	/* 10 */
	{0, 10, 10, "SPI2_APB_S", true},
	{0, 11, 11, "SPI3_APB_S", true},
	{0, 12, 12, "SPI4_APB_S", true},
	{0, 13, 13, "SPI5_APB_S", true},
	{0, 14, 14, "SPI6_APB_S", true},
	{0, 15, 15, "SPI7_APB_S", true},
	{0, 16, 16, "NOR_APB_S", true},
	{0, 17, 17, "DEVICE_APC_PERI_PAR_PDN_APB_S", true},
	{0, 18, 18, "PERI_MBIST_PDN_APB_S", true},
	{0, 19, 19, "BCRM_PERI_PAR_PDN_APB_S", true},

	/* 20 */
	{0, 20, 20, "IIC_P2P_REMAP_APB_S", true},
	{0, 21, 21, "APDMA_APB_S", true},
	{0, 22, 22, "SSUSB_S", true},
	{0, 23, 23, "SSUSB_S-1", true},
	{0, 24, 24, "SSUSB_S-2", true},
	{0, 25, 25, "USB_S-1", true},
	{0, 26, 26, "USB_S-2", true},
	{0, 27, 27, "MSDC1_S-1", true},
	{0, 28, 28, "MSDC1_S-2", true},
	{0, 29, 29, "MSDC2_S-1", true},

	/* 30 */
	{0, 30, 30, "MSDC2_S-2", true},
	{0, 31, 31, "UFS0_S", true},
	{0, 32, 32, "UFS0_S-1", true},
	{0, 33, 33, "UFS0_S-2", true},
	{0, 34, 34, "UFS0_S-3", true},
	{0, 35, 35, "UFS0_S-4", true},
	{0, 36, 36, "UFS0_S-5", true},
	{0, 37, 37, "UFS0_S-6", true},
	{0, 38, 38, "UFS0_S-7", true},
	{0, 39, 39, "UFS0_S-8", true},

	/* 40 */
	{0, 40, 40, "PCIE0_AHB_S-1", true},
	{0, 41, 41, "PCIE0_AHB_S-2", true},
	{0, 42, 42, "PCIE1_AHB_S-1", true},
	{0, 43, 43, "PCIE1_AHB_S-2", true},
	{0, 44, 44, "PCIE0_AXI_S", true},
	{0, 45, 45, "PCIE1_AXI_S", true},
	{0, 46, 46, "NOR_AXI_S", true},
	{0, 47, 49, "DEVICE_APC_PERI_PAR_AO_APB_S", true},
	{0, 48, 50, "MBIST_AO_APB_S", true},
	{0, 49, 51, "BCRM_PERI_PAR_AO_APB_S", true},

	/* 50 */
	{0, 50, 52, "PERICFG_AO_APB_S", true},
	{0, 51, 53, "DEBUG_CTRL_PERI_PAR_AO_APB_S", true},

	{-1, -1, 54, "OOB_way_en", true},
	{-1, -1, 55, "OOB_way_en", true},
	{-1, -1, 56, "OOB_way_en", true},
	{-1, -1, 57, "OOB_way_en", true},
	{-1, -1, 58, "OOB_way_en", true},
	{-1, -1, 59, "OOB_way_en", true},

	{-1, -1, 60, "OOB_way_en", true},
	{-1, -1, 61, "OOB_way_en", true},
	{-1, -1, 62, "OOB_way_en", true},
	{-1, -1, 63, "OOB_way_en", true},
	{-1, -1, 64, "OOB_way_en", true},
	{-1, -1, 65, "OOB_way_en", true},
	{-1, -1, 66, "OOB_way_en", true},
	{-1, -1, 67, "OOB_way_en", true},
	{-1, -1, 68, "OOB_way_en", true},
	{-1, -1, 69, "OOB_way_en", true},

	{-1, -1, 70, "OOB_way_en", true},
	{-1, -1, 71, "OOB_way_en", true},
	{-1, -1, 72, "OOB_way_en", true},
	{-1, -1, 73, "OOB_way_en", true},
	{-1, -1, 74, "OOB_way_en", true},
	{-1, -1, 75, "OOB_way_en", true},
	{-1, -1, 76, "OOB_way_en", true},
	{-1, -1, 77, "OOB_way_en", true},
	{-1, -1, 78, "OOB_way_en", true},
	{-1, -1, 79, "OOB_way_en", true},

	{-1, -1, 80, "OOB_way_en", true},
	{-1, -1, 81, "OOB_way_en", true},
	{-1, -1, 82, "OOB_way_en", true},
	{-1, -1, 83, "OOB_way_en", true},
	{-1, -1, 84, "OOB_way_en", true},
	{-1, -1, 85, "OOB_way_en", true},
	{-1, -1, 86, "OOB_way_en", true},
	{-1, -1, 87, "OOB_way_en", true},

	{-1, -1, 88, "Decode_error", true},
	{-1, -1, 89, "Decode_error", true},
	{-1, -1, 90, "Decode_error", true},

	{-1, -1, 91, "APDMA", true},
	{-1, -1, 92, "IIC_P2P_REMAP", true},
	{-1, -1, 93, "DEVICE_APC_PERI_AO", false},
	{-1, -1, 94, "DEVICE_APC_PERI_PDN", false},
};

static const struct mtk_device_num mtk6983_devices_num[] = {
	{SLAVE_TYPE_INFRA, VIO_SLAVE_NUM_INFRA, IRQ_TYPE_INFRA},
	{SLAVE_TYPE_INFRA1, VIO_SLAVE_NUM_INFRA1, IRQ_TYPE_INFRA},
	{SLAVE_TYPE_PERI_PAR, VIO_SLAVE_NUM_PERI_PAR, IRQ_TYPE_INFRA},
};

static const struct INFRAAXI_ID_INFO infra_mi_id_to_master[] = {
	{"ADSPSYS_M1",        { 0, 0, 0, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0 } },
	{"CONN_M",            { 0, 0, 1, 0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0 } },
	{"CQ_DMA",            { 0, 0, 0, 1, 0, 0, 0, 0, 2, 2, 2, 0, 0, 0, 0, 0 } },
	{"DebugTop",          { 0, 0, 0, 1, 0, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"GPU_EB",            { 0, 0, 0, 1, 0, 0, 1, 0, 2, 2, 2, 2, 2, 2, 2, 0 } },
	{"CPUM_M",            { 0, 0, 0, 1, 0, 1, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"DXCC_M",            { 0, 0, 0, 1, 0, 0, 0, 1, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"VLPSYS_M",          { 0, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0 } },
	{"DPMAIF_M",          { 0, 0, 0, 0, 1, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0 } },
	{"HWCCF_M",           { 0, 0, 1, 0, 1, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"THERM_M",           { 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"THERM2_M",          { 0, 0, 1, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"CCU_M",             { 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"PERI2INFRA1_M",     { 0, 0, 0, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0, 0 } },
	{"IPU_M",             { 0, 0, 1, 1, 1, 0, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0 } },
	{"INFRA_BUS_HRE_M",   { 0, 0, 1, 1, 1, 1, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 } },
	{"MCU_AP_M",          { 1, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 0, 0 } },
	{"MM2SLB1_M",         { 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
	{"MD_AP_M",           { 1, 1, 2, 2, 2, 2, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0 } },
};

static const char *infra_mi_trans(uint32_t bus_id)
{
	int master_count = ARRAY_SIZE(infra_mi_id_to_master);
	const char *master = "UNKNOWN_MASTER_FROM_INFRA";
	int i, j;

	for (i = 0; i < master_count; i++) {
		for (j = 0; j < INFRAAXI_MI_BIT_LENGTH; j++) {
			if (infra_mi_id_to_master[i].bit[j] == 2)
				continue;
			if (((bus_id >> j) & 0x1) ==
					infra_mi_id_to_master[i].bit[j])
				continue;
			break;
		}
		if (j == INFRAAXI_MI_BIT_LENGTH) {
			pr_debug(PFX "%s %s %s\n",
				"catch it from INFRAAXI_MI",
				"Master is:",
				infra_mi_id_to_master[i].master);
			master = infra_mi_id_to_master[i].master;
		}
	}

	return master;
}

static const char *mt6983_bus_id_to_master(uint32_t bus_id, uint32_t vio_addr,
		int slave_type, int shift_sta_bit, int domain)
{
	pr_debug(PFX "%s:0x%x, %s:0x%x, %s:0x%x, %s:%d\n",
		"bus_id", bus_id, "vio_addr", vio_addr,
		"slave_type", slave_type,
		"shift_sta_bit", shift_sta_bit);

	if (vio_addr <= SRAM_END_ADDR) {
		pr_info(PFX "vio_addr is from on-chip SRAMROM\n");
		if ((bus_id & 0x3) == 0x0)
			return "NTH_EMI_GMC_M";
		else if ((bus_id & 0x3) == 0x2)
			return "STH_EMI_GMC_M";
		else
			return infra_mi_trans(bus_id >> 1);
	} else {
		return infra_mi_trans(bus_id);
	}
}

/* violation index corresponds to subsys */
const char *index_to_subsys(int slave_type, uint32_t vio_index,
		uint32_t vio_addr)
{
	int i;

	/* Filter by violation index */
	if (slave_type == SLAVE_TYPE_INFRA &&
			vio_index < VIO_SLAVE_NUM_INFRA) {
		for (i = 0; i < VIO_SLAVE_NUM_INFRA; i++) {
			if (vio_index == mt6983_devices_infra[i].vio_index)
				return mt6983_devices_infra[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_INFRA1 &&
			vio_index < VIO_SLAVE_NUM_INFRA1) {
		for (i = 0; i < VIO_SLAVE_NUM_INFRA1; i++) {
			if (vio_index == mt6983_devices_infra1[i].vio_index)
				return mt6983_devices_infra1[i].device;
		}
	} else if (slave_type == SLAVE_TYPE_PERI_PAR &&
			vio_index < VIO_SLAVE_NUM_PERI_PAR) {
		for (i = 0; i < VIO_SLAVE_NUM_PERI_PAR; i++) {
			if (vio_index == mt6983_devices_peri_par[i].vio_index)
				return mt6983_devices_peri_par[i].device;
		}
	}

	return "OUT_OF_BOUND";
}

static void mm2nd_vio_handler(void __iomem *infracfg,
			      struct mtk_devapc_vio_info *vio_info,
			      bool mdp_vio, bool disp2_vio, bool mmsys_vio)
{
	uint32_t vio_sta, vio_dbg, rw;
	uint32_t vio_sta_num;
	uint32_t vio0_offset;
	char mm_str[64] = {0};
	void __iomem *reg;
	int i;

	if (!infracfg) {
		pr_err(PFX "%s, param check failed, infracfg ptr is NULL\n",
				__func__);
		return;
	}

	if (mdp_vio) {
		vio_sta_num = INFRACFG_MDP_VIO_STA_NUM;
		vio0_offset = INFRACFG_MDP_SEC_VIO0_OFFSET;

		strncpy(mm_str, "INFRACFG_MDP_SEC_VIO", sizeof(mm_str));

	} else if (disp2_vio) {
		vio_sta_num = INFRACFG_DISP2_VIO_STA_NUM;
		vio0_offset = INFRACFG_DISP2_SEC_VIO0_OFFSET;

		strncpy(mm_str, "INFRACFG_DISP2_SEC_VIO", sizeof(mm_str));

	} else if (mmsys_vio) {
		vio_sta_num = INFRACFG_MM_VIO_STA_NUM;
		vio0_offset = INFRACFG_MM_SEC_VIO0_OFFSET;

		strncpy(mm_str, "INFRACFG_MM_SEC_VIO", sizeof(mm_str));

	} else {
		pr_err(PFX "%s: param check failed, %s:%s, %s:%s, %s:%s\n",
				__func__,
				"mdp_vio", mdp_vio ? "true" : "false",
				"disp2_vio", disp2_vio ? "true" : "false",
				"mmsys_vio", mmsys_vio ? "true" : "false");
		return;
	}

	/* Get mm2nd violation status */
	for (i = 0; i < vio_sta_num; i++) {
		reg = infracfg + vio0_offset + i * 4;
		vio_sta = readl(reg);
		if (vio_sta)
			pr_info(PFX "MM 2nd violation: %s%d:0x%x\n",
					mm_str, i, vio_sta);
	}

	/* Get mm2nd violation address */
	reg = infracfg + vio0_offset + i * 4;
	vio_info->vio_addr = readl(reg);

	/* Get mm2nd violation information */
	reg = infracfg + vio0_offset + (i + 1) * 4;
	vio_dbg = readl(reg);

	vio_info->domain_id = (vio_dbg & INFRACFG_MM2ND_VIO_DOMAIN_MASK) >>
		INFRACFG_MM2ND_VIO_DOMAIN_SHIFT;

	vio_info->master_id = (vio_dbg & INFRACFG_MM2ND_VIO_ID_MASK) >>
		INFRACFG_MM2ND_VIO_ID_SHIFT;

	rw = (vio_dbg & INFRACFG_MM2ND_VIO_RW_MASK) >>
		INFRACFG_MM2ND_VIO_RW_SHIFT;
	vio_info->read = (rw == 0);
	vio_info->write = (rw == 1);
}

static uint32_t mt6983_shift_group_get(int slave_type, uint32_t vio_idx)
{
	return 31;
}

void devapc_catch_illegal_range(phys_addr_t phys_addr, size_t size)
{
	phys_addr_t test_pa = 0x17a54c50;

	/*
	 * Catch BROM addr mapped
	 */
	if (phys_addr >= 0x0 && phys_addr < SRAM_START_ADDR) {
		pr_err(PFX "%s %s:(%pa), %s:(0x%lx)\n",
				"catch BROM address mapped!",
				"phys_addr", &phys_addr,
				"size", size);
		BUG_ON(1);
	}

	if ((phys_addr <= test_pa) && (phys_addr + size > test_pa)) {
		pr_err(PFX "%s %s:(%pa), %s:(0x%lx), %s:(%pa)\n",
				"catch VENCSYS address mapped!",
				"phys_addr", &phys_addr,
				"size", size, "test_pa", &test_pa);
		BUG_ON(1);
	}
}

static struct mtk_devapc_dbg_status mt6983_devapc_dbg_stat = {
	.enable_ut = PLAT_DBG_UT_DEFAULT,
	.enable_KE = PLAT_DBG_KE_DEFAULT,
	.enable_AEE = PLAT_DBG_AEE_DEFAULT,
	.enable_WARN = PLAT_DBG_WARN_DEFAULT,
	.enable_dapc = PLAT_DBG_DAPC_DEFAULT,
};

static const char * const slave_type_to_str[] = {
	"SLAVE_TYPE_INFRA",
	"SLAVE_TYPE_INFRA1",
	"SLAVE_TYPE_PERI_PAR",
	"WRONG_SLAVE_TYPE",
};

static int mtk_vio_mask_sta_num[] = {
	VIO_MASK_STA_NUM_INFRA,
	VIO_MASK_STA_NUM_INFRA1,
	VIO_MASK_STA_NUM_PERI_PAR,
};

static struct mtk_devapc_vio_info mt6983_devapc_vio_info = {
	.vio_mask_sta_num = mtk_vio_mask_sta_num,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
	.mdp_vio_idx = MDP_VIO_INDEX,
	.disp2_vio_idx = DISP2_VIO_INDEX,
	.mmsys_vio_idx = MMSYS_VIO_INDEX,
	.sramrom_slv_type = SRAMROM_SLAVE_TYPE,
	.mm2nd_slv_type = MM2ND_SLAVE_TYPE,
};

static const struct mtk_infra_vio_dbg_desc mt6983_vio_dbgs = {
	.vio_dbg_mstid = INFRA_VIO_DBG_MSTID,
	.vio_dbg_mstid_start_bit = INFRA_VIO_DBG_MSTID_START_BIT,
	.vio_dbg_dmnid = INFRA_VIO_DBG_DMNID,
	.vio_dbg_dmnid_start_bit = INFRA_VIO_DBG_DMNID_START_BIT,
	.vio_dbg_w_vio = INFRA_VIO_DBG_W_VIO,
	.vio_dbg_w_vio_start_bit = INFRA_VIO_DBG_W_VIO_START_BIT,
	.vio_dbg_r_vio = INFRA_VIO_DBG_R_VIO,
	.vio_dbg_r_vio_start_bit = INFRA_VIO_DBG_R_VIO_START_BIT,
	.vio_addr_high = INFRA_VIO_ADDR_HIGH,
	.vio_addr_high_start_bit = INFRA_VIO_ADDR_HIGH_START_BIT,
};

static const struct mtk_sramrom_sec_vio_desc mt6983_sramrom_sec_vios = {
	.vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const uint32_t mt6983_devapc_pds[] = {
	PD_VIO_MASK_OFFSET,
	PD_VIO_STA_OFFSET,
	PD_VIO_DBG0_OFFSET,
	PD_VIO_DBG1_OFFSET,
	PD_VIO_DBG2_OFFSET,
	PD_APC_CON_OFFSET,
	PD_SHIFT_STA_OFFSET,
	PD_SHIFT_SEL_OFFSET,
	PD_SHIFT_CON_OFFSET,
	PD_VIO_DBG3_OFFSET,
};

static struct mtk_devapc_soc mt6983_data = {
	.dbg_stat = &mt6983_devapc_dbg_stat,
	.slave_type_arr = slave_type_to_str,
	.slave_type_num = SLAVE_TYPE_NUM,
	.device_info[SLAVE_TYPE_INFRA] = mt6983_devices_infra,
	.device_info[SLAVE_TYPE_INFRA1] = mt6983_devices_infra1,
	.device_info[SLAVE_TYPE_PERI_PAR] = mt6983_devices_peri_par,
	.ndevices = mtk6983_devices_num,
	.vio_info = &mt6983_devapc_vio_info,
	.vio_dbgs = &mt6983_vio_dbgs,
	.sramrom_sec_vios = &mt6983_sramrom_sec_vios,
	.devapc_pds = mt6983_devapc_pds,
	.irq_type_num = IRQ_TYPE_NUM,
	.subsys_get = &index_to_subsys,
	.master_get = &mt6983_bus_id_to_master,
	.mm2nd_vio_handler = &mm2nd_vio_handler,
	.shift_group_get = mt6983_shift_group_get,
};

static const struct of_device_id mt6983_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6983-devapc" },
	{},
};

static int mt6983_devapc_probe(struct platform_device *pdev)
{
	return mtk_devapc_probe(pdev, &mt6983_data);
}

static int mt6983_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6983_devapc_driver = {
	.probe = mt6983_devapc_probe,
	.remove = mt6983_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = mt6983_devapc_dt_match,
	},
};

module_platform_driver(mt6983_devapc_driver);

MODULE_DESCRIPTION("Mediatek MT6983 Device APC Driver");
MODULE_AUTHOR("Jackson Chang <jackson-kt.chang@mediatek.com>");
MODULE_LICENSE("GPL");
