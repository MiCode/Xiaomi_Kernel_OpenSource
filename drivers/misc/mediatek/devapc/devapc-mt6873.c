// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <asm-generic/io.h>

#include "devapc-mt6873.h"
#include "devapc-mtk-multi-ao.h"

static struct mtk_device_info mt6873_devices_infra[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "MFGSYS", true},
	{0, 1, 1, "MFGSYS", true},
	{0, 2, 2, "MFGSYS", true},
	{0, 3, 3, "MFGSYS", true},
	{0, 4, 4, "MFGSYS", true},
	{0, 5, 5, "MFGSYS", true},
	{0, 6, 6, "MFGSYS", true},
	{0, 7, 7, "MFGSYS", true},
	{0, 8, 8, "MFGSYS", true},
	{0, 9, 9, "APUSYS", true},

	/* 10 */
	{0, 10, 10, "APUSYS", true},
	{0, 11, 11, "APUSYS", true},
	{0, 12, 12, "APUSYS", true},
	{0, 13, 13, "APUSYS", true},
	{0, 14, 14, "APUSYS", true},
	{0, 15, 15, "MCUSYS_CFGREG_APB_S", true},
	{0, 16, 16, "MCUSYS_CFGREG_APB_S-1", true},
	{0, 17, 17, "MCUSYS_CFGREG_APB_S-2", true},
	{0, 18, 18, "MCUSYS_CFGREG_APB_S-3", true},
	{0, 19, 19, "MCUSYS_CFGREG_APB_S-4", true},

	/* 20 */
	{0, 20, 20, "L3C_S", true},
	{0, 21, 21, "L3C_S-1", true},
	{0, 22, 352, "PCIE_AXI_S", true},
	{1, 0, 22, "MMSYS", true},
	{1, 1, 23, "MMSYS_DISP", true},
	{1, 2, 24, "SMI", true},
	{1, 3, 25, "SMI", true},
	{1, 4, 26, "SMI", true},
	{1, 5, 27, "MMSYS_DISP", true},
	{1, 6, 28, "MMSYS_DISP", true},

	/* 30 */
	{1, 7, 29, "MMSYS_DISP", true},
	{1, 8, 30, "MMSYS_DISP", true},
	{1, 9, 31, "MMSYS_DISP", true},
	{1, 10, 32, "MMSYS_DISP", true},
	{1, 11, 33, "MMSYS_DISP", true},
	{1, 12, 34, "MMSYS_DISP", true},
	{1, 13, 35, "MMSYS_DISP", true},
	{1, 14, 36, "MMSYS_DISP", true},
	{1, 15, 37, "MMSYS_DISP", true},
	{1, 16, 38, "MMSYS_DISP", true},

	/* 40 */
	{1, 17, 39, "MMSYS_DISP", true},
	{1, 18, 40, "MMSYS_DISP", true},
	{1, 19, 41, "MMSYS_DISP", true},
	{1, 20, 42, "MMSYS_DISP", true},
	{1, 21, 43, "MMSYS_DISP", true},
	{1, 22, 44, "MMSYS_DISP", true},
	{1, 23, 45, "MMSYS_MDP", true},
	{1, 24, 46, "MMSYS_MDP", true},
	{1, 25, 47, "MMSYS_MDP", true},
	{1, 26, 48, "MMSYS_MDP", true},

	/* 50 */
	{1, 27, 49, "MMSYS_MDP", true},
	{1, 28, 50, "MMSYS_MDP", true},
	{1, 29, 51, "MMSYS_IOMMU", true},
	{1, 30, 52, "MMSYS_IOMMU", true},
	{1, 31, 53, "MMSYS_IOMMU", true},
	{1, 32, 54, "MMSYS_IOMMU", true},
	{1, 33, 55, "MMSYS_IOMMU", true},
	{1, 34, 56, "SMI", true},
	{1, 35, 57, "SMI", true},
	{1, 36, 58, "SMI", true},

	/* 60 */
	{1, 37, 59, "SMI", true},
	{1, 38, 60, "MMSYS", true},
	{1, 39, 61, "MMSYS", true},
	{1, 40, 62, "IMGSYS", true},
	{1, 41, 63, "IMGSYS", true},
	{1, 42, 64, "IMGSYS", true},
	{1, 43, 65, "IMGSYS", true},
	{1, 44, 66, "IMGSYS", true},
	{1, 45, 67, "IMGSYS", true},
	{1, 46, 68, "IMGSYS", true},

	/* 70 */
	{1, 47, 69, "IMGSYS", true},
	{1, 48, 70, "IMGSYS", true},
	{1, 49, 71, "IMGSYS", true},
	{1, 50, 72, "IMGSYS", true},
	{1, 51, 73, "IMGSYS", true},
	{1, 52, 74, "IMGSYS", true},
	{1, 53, 75, "IMGSYS", true},
	{1, 54, 76, "SMI", true},
	{1, 55, 77, "IMGSYS", true},
	{1, 56, 78, "IMGSYS", true},

	/* 80 */
	{1, 57, 79, "IMGSYS", true},
	{1, 58, 80, "IMGSYS", true},
	{1, 59, 81, "IMGSYS", true},
	{1, 60, 82, "IMGSYS", true},
	{1, 61, 83, "IMGSYS", true},
	{1, 62, 84, "IMGSYS", true},
	{1, 63, 85, "IMGSYS", true},
	{1, 64, 86, "IMGSYS", true},
	{1, 65, 87, "IMGSYS", true},
	{1, 66, 88, "IMGSYS", true},

	/* 90 */
	{1, 67, 89, "IMGSYS", true},
	{1, 68, 90, "IMGSYS", true},
	{1, 69, 91, "IMGSYS", true},
	{1, 70, 92, "IMGSYS", true},
	{1, 71, 93, "IMGSYS", true},
	{1, 72, 94, "IMGSYS", true},
	{1, 73, 95, "IMGSYS", true},
	{1, 74, 96, "IMGSYS", true},
	{1, 75, 97, "IMGSYS", true},
	{1, 76, 98, "SMI", true},

	/* 100 */
	{1, 77, 99, "IMGSYS", true},
	{1, 78, 100, "IMGSYS", true},
	{1, 79, 101, "IMGSYS", true},
	{1, 80, 102, "IMGSYS", true},
	{1, 81, 103, "IMGSYS", true},
	{1, 82, 104, "IMGSYS", true},
	{1, 83, 105, "IMGSYS", true},
	{1, 84, 106, "VDECSYS", true},
	{1, 85, 107, "VDECSYS", true},
	{1, 86, 108, "VDECSYS", true},

	/* 110 */
	{1, 87, 109, "VDECSYS", true},
	{1, 88, 110, "VDECSYS", true},
	{1, 89, 111, "VDECSYS", true},
	{1, 90, 112, "VDECSYS", true},
	{1, 91, 113, "VDECSYS", true},
	{1, 92, 114, "VENCSYS", true},
	{1, 93, 115, "VENCSYS", true},
	{1, 94, 116, "VENCSYS", true},
	{1, 95, 117, "VENCSYS", true},
	{1, 96, 118, "VENCSYS", true},

	/* 120 */
	{1, 97, 119, "VENCSYS", true},
	{1, 98, 120, "VENCSYS", true},
	{1, 99, 121, "VENCSYS", true},
	{1, 100, 122, "CAMSYS", true},
	{1, 101, 123, "SMI", true},
	{1, 102, 124, "SMI", true},
	{1, 103, 125, "CAMSYS", true},
	{1, 104, 126, "CAMSYS_SENINF", true},
	{1, 105, 127, "CAMSYS_SENINF", true},
	{1, 106, 128, "CAMSYS_SENINF", true},

	/* 130 */
	{1, 107, 129, "CAMSYS_SENINF", true},
	{1, 108, 130, "CAMSYS_SENINF", true},
	{1, 109, 131, "CAMSYS_SENINF", true},
	{1, 110, 132, "CAMSYS_SENINF", true},
	{1, 111, 133, "CAMSYS_SENINF", true},
	{1, 112, 134, "SMI", true},
	{1, 113, 135, "SMI", true},
	{1, 114, 136, "CAMSYS", true},
	{1, 115, 137, "SMI", true},
	{1, 116, 138, "SMI", true},

	/* 140 */
	{1, 117, 139, "SMI", true},
	{1, 118, 140, "CAMSYS", true},
	{1, 119, 141, "CAMSYS", true},
	{1, 120, 142, "CAMSYS", true},
	{1, 121, 143, "CAMSYS", true},
	{1, 122, 144, "CAMSYS", true},
	{1, 123, 145, "CAMSYS", true},
	{1, 124, 146, "CAMSYS", true},
	{1, 125, 147, "CAMSYS", true},
	{1, 126, 148, "CAMSYS", true},

	/* 150 */
	{1, 127, 149, "CAMSYS", true},
	{1, 128, 150, "CAMSYS", true},
	{1, 129, 151, "CAMSYS", true},
	{1, 130, 152, "CAMSYS", true},
	{1, 131, 153, "CAMSYS", true},
	{1, 132, 154, "CAMSYS", true},
	{1, 133, 155, "CAMSYS", true},
	{1, 134, 156, "CAMSYS", true},
	{1, 135, 157, "CAMSYS", true},
	{1, 136, 158, "CAMSYS", true},

	/* 160 */
	{1, 137, 159, "CAMSYS", true},
	{1, 138, 160, "CAMSYS", true},
	{1, 139, 161, "CAMSYS", true},
	{1, 140, 162, "CAMSYS", true},
	{1, 141, 163, "CAMSYS", true},
	{1, 142, 164, "CAMSYS", true},
	{1, 143, 165, "CAMSYS", true},
	{1, 144, 166, "CAMSYS", true},
	{1, 145, 167, "CAMSYS", true},
	{1, 146, 168, "CAMSYS", true},

	/* 170 */
	{1, 147, 169, "CAMSYS", true},
	{1, 148, 170, "CAMSYS", true},
	{1, 149, 171, "CAMSYS", true},
	{1, 150, 172, "CAMSYS", true},
	{1, 151, 173, "CAMSYS", true},
	{1, 152, 174, "CAMSYS", true},
	{1, 153, 175, "CAMSYS", true},
	{1, 154, 176, "CAMSYS", true},
	{1, 155, 177, "CAMSYS", true},
	{1, 156, 178, "CAMSYS", true},

	/* 180 */
	{1, 157, 179, "CAMSYS", true},
	{1, 158, 180, "CAMSYS", true},
	{1, 159, 181, "CAMSYS", true},
	{1, 160, 182, "CAMSYS", true},
	{1, 161, 183, "CAMSYS", true},
	{1, 162, 184, "CAMSYS", true},
	{1, 163, 185, "CAMSYS", true},
	{1, 164, 186, "CAMSYS", true},
	{1, 165, 187, "CAMSYS", true},
	{1, 166, 188, "CAMSYS", true},

	/* 190 */
	{1, 167, 189, "CAMSYS", true},
	{1, 168, 190, "CAMSYS", true},
	{1, 169, 191, "CAMSYS", true},
	{1, 170, 192, "CAMSYS", true},
	{1, 171, 193, "CAMSYS", true},
	{1, 172, 194, "CAMSYS", true},
	{1, 173, 195, "CAMSYS", true},
	{1, 174, 196, "CAMSYS", true},
	{1, 175, 197, "CAMSYS", true},
	{1, 176, 198, "CAMSYS", true},

	/* 200 */
	{1, 177, 199, "CAMSYS", true},
	{1, 178, 200, "CAMSYS", true},
	{1, 179, 201, "CAMSYS", true},
	{1, 180, 202, "CAMSYS", true},
	{1, 181, 203, "CAMSYS", true},
	{1, 182, 204, "CAMSYS", true},
	{1, 183, 205, "CAMSYS", true},
	{1, 184, 206, "CAMSYS", true},
	{1, 185, 207, "CAMSYS", true},
	{1, 186, 208, "CAMSYS", true},

	/* 210 */
	{1, 187, 209, "CAMSYS", true},
	{1, 188, 210, "CAMSYS", true},
	{1, 189, 211, "CAMSYS", true},
	{1, 190, 212, "CAMSYS", true},
	{1, 191, 213, "CAMSYS", true},
	{1, 192, 214, "CAMSYS", true},
	{1, 193, 215, "CAMSYS", true},
	{1, 194, 216, "CAMSYS", true},
	{1, 195, 217, "CAMSYS", true},
	{1, 196, 218, "CAMSYS", true},

	/* 220 */
	{1, 197, 219, "CAMSYS", true},
	{1, 198, 220, "CAMSYS", true},
	{1, 199, 221, "CAMSYS", true},
	{1, 200, 222, "CAMSYS", true},
	{1, 201, 223, "CAMSYS", true},
	{1, 202, 224, "CAMSYS", true},
	{1, 203, 225, "CAMSYS", true},
	{1, 204, 226, "CAMSYS", true},
	{1, 205, 227, "CAMSYS", true},
	{1, 206, 228, "CAMSYS", true},

	/* 230 */
	{1, 207, 229, "CAMSYS", true},
	{1, 208, 230, "CAMSYS", true},
	{1, 209, 231, "CAMSYS", true},
	{1, 210, 232, "CAMSYS", true},
	{1, 211, 233, "CAMSYS", true},
	{1, 212, 234, "CAMSYS", true},
	{1, 213, 235, "CAMSYS", true},
	{1, 214, 236, "CAMSYS", true},
	{1, 215, 237, "CAMSYS", true},
	{1, 216, 238, "CAMSYS", true},

	/* 240 */
	{1, 217, 239, "CAMSYS", true},
	{1, 218, 240, "CAMSYS", true},
	{1, 219, 241, "CAMSYS", true},
	{1, 220, 242, "CAMSYS", true},
	{1, 221, 243, "CAMSYS", true},
	{1, 222, 244, "CAMSYS", true},
	{1, 223, 245, "CAMSYS", true},
	{1, 224, 246, "CAMSYS", true},
	{1, 225, 247, "CAMSYS", true},
	{1, 226, 248, "CAMSYS", true},

	/* 250 */
	{1, 227, 249, "CAMSYS", true},
	{1, 228, 250, "CAMSYS", true},
	{1, 229, 251, "CAMSYS", true},
	{1, 230, 252, "CAMSYS", true},
	{1, 231, 253, "CAMSYS", true},
	{1, 232, 254, "CAMSYS", true},
	{1, 233, 255, "CAMSYS", true},
	{1, 234, 256, "CAMSYS", true},
	{1, 235, 257, "CAMSYS", true},
	{1, 236, 258, "CAMSYS", true},

	/* 260 */
	{1, 237, 259, "CAMSYS", true},
	{1, 238, 260, "CAMSYS", true},
	{1, 239, 261, "CAMSYS", true},
	{1, 240, 262, "CAMSYS", true},
	{1, 241, 263, "CAMSYS", true},
	{1, 242, 264, "CAMSYS", true},
	{1, 243, 265, "CAMSYS", true},
	{1, 244, 266, "CAMSYS", true},
	{1, 245, 267, "CAMSYS", true},
	{1, 246, 268, "CAMSYS", true},

	/* 270 */
	{1, 247, 269, "CAMSYS", true},
	{1, 248, 270, "CAMSYS", true},
	{1, 249, 271, "CAMSYS", true},
	{1, 250, 272, "CAMSYS", true},
	{1, 251, 273, "CAMSYS", true},
	{1, 252, 274, "CAMSYS", true},
	{1, 253, 275, "CAMSYS", true},
	{1, 254, 276, "CAMSYS", true},
	{1, 255, 277, "CAMSYS", true},
	{2, 0, 278, "CAMSYS", true},

	/* 280 */
	{2, 1, 279, "CAMSYS", true},
	{2, 2, 280, "CAMSYS", true},
	{2, 3, 281, "CAMSYS", true},
	{2, 4, 282, "CAMSYS", true},
	{2, 5, 283, "CAMSYS", true},
	{2, 6, 284, "CAMSYS", true},
	{2, 7, 285, "CAMSYS", true},
	{2, 8, 286, "CAMSYS", true},
	{2, 9, 287, "CAMSYS", true},
	{2, 10, 288, "CAMSYS", true},

	/* 290 */
	{2, 11, 289, "CAMSYS", true},
	{2, 12, 290, "CAMSYS", true},
	{2, 13, 291, "CAMSYS", true},
	{2, 14, 292, "CAMSYS", true},
	{2, 15, 293, "CAMSYS", true},
	{2, 16, 294, "CAMSYS", true},
	{2, 17, 295, "CAMSYS_CCU", true},
	{2, 18, 296, "CAMSYS_CCU", true},
	{2, 19, 297, "CAMSYS_CCU", true},
	{2, 20, 298, "CAMSYS_CCU", true},

	/* 300 */
	{2, 21, 299, "CAMSYS_CCU", true},
	{2, 22, 300, "CAMSYS_CCU", true},
	{2, 23, 301, "CAMSYS_CCU", true},
	{2, 24, 302, "CAMSYS_CCU", true},
	{2, 25, 303, "CAMSYS_CCU", true},
	{2, 26, 304, "CAMSYS_CCU", true},
	{2, 27, 305, "CAMSYS_CCU", true},
	{2, 28, 306, "CAMSYS_CCU", true},
	{2, 29, 307, "CAMSYS_CCU", true},
	{2, 30, 308, "CAMSYS_CCU", true},

	/* 310 */
	{2, 31, 309, "CAMSYS_CCU", true},
	{2, 32, 310, "CAMSYS_CCU", true},
	{2, 33, 311, "CAMSYS_CCU", true},
	{2, 34, 312, "CAMSYS_CCU", true},
	{2, 35, 313, "CAMSYS_CCU", true},
	{2, 36, 314, "CAMSYS_CCU", true},
	{2, 37, 315, "CAMSYS_CCU", true},
	{2, 38, 316, "CAMSYS_CCU", true},
	{2, 39, 317, "CAMSYS_CCU", true},
	{2, 40, 318, "IPESYS", true},

	/* 320 */
	{2, 41, 319, "IPESYS", true},
	{2, 42, 320, "IPESYS", true},
	{2, 43, 321, "IPESYS", true},
	{2, 44, 322, "IPESYS", true},
	{2, 45, 323, "SMI", true},
	{2, 46, 324, "SMI", true},
	{2, 47, 325, "IPESYS", true},
	{2, 48, 326, "IPESYS", true},
	{2, 49, 327, "IPESYS", true},
	{2, 50, 328, "SMI", true},

	/* 330 */
	{2, 51, 329, "IPESYS", true},
	{2, 52, 330, "MDPSYS", true},
	{2, 53, 331, "MDPSYS", true},
	{2, 54, 332, "SMI", true},
	{2, 55, 333, "MDPSYS", true},
	{2, 56, 334, "MDPSYS", true},
	{2, 57, 335, "MDPSYS", true},
	{2, 58, 336, "MDPSYS", true},
	{2, 59, 337, "MDPSYS", true},
	{2, 60, 338, "MDPSYS", true},

	/* 340 */
	{2, 61, 339, "MDPSYS", true},
	{2, 62, 340, "MDPSYS", true},
	{2, 63, 341, "MDPSYS", true},
	{2, 64, 342, "MDPSYS", true},
	{2, 65, 343, "MDPSYS", true},
	{2, 66, 344, "MDPSYS", true},
	{2, 67, 345, "MDPSYS", true},
	{2, 68, 346, "MDPSYS", true},
	{2, 69, 347, "MDPSYS", true},
	{-1, -1, 355, "OOB_way_en", true},

	/* 350 */
	{-1, -1, 356, "OOB_way_en", true},
	{-1, -1, 357, "OOB_way_en", true},
	{-1, -1, 358, "OOB_way_en", true},
	{-1, -1, 359, "OOB_way_en", true},
	{-1, -1, 360, "OOB_way_en", true},
	{-1, -1, 361, "OOB_way_en", true},
	{-1, -1, 362, "OOB_way_en", true},
	{-1, -1, 363, "OOB_way_en", true},
	{-1, -1, 364, "OOB_way_en", true},
	{-1, -1, 365, "OOB_way_en", true},

	/* 360 */
	{-1, -1, 366, "Decode_error", true},
	{-1, -1, 367, "SRAMROM", true},
	{-1, -1, 368, "CQDMA_SECURE", false},
	{-1, -1, 369, "reserve", false},
	{-1, -1, 370, "DEVICE_APC_INFRA_AO", false},
	{-1, -1, 371, "DEVICE_APC_INFRA_PDN", false},

};

static struct mtk_device_info mt6873_devices_peri[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "SPM_APB_S", true},
	{0, 1, 1, "SPM_APB_S-1", true},
	{0, 2, 2, "SPM_APB_S-2", true},
	{0, 3, 3, "SPM_APB_S-3", true},
	{0, 4, 4, "SPM_APB_S-4", true},
	{0, 5, 5, "APMIXEDSYS_APB_S", true},
	{0, 6, 6, "APMIXEDSYS_APB_S-1", true},
	{0, 7, 7, "TOPCKGEN_APB_S", true},
	{0, 8, 8, "INFRACFG_AO_APB_S", true},
	{0, 9, 9, "INFRACFG_AO_MEM_APB_S", true},

	/* 10 */
	{0, 10, 10, "PERICFG_AO_APB_S", true},
	{0, 11, 11, "GPIO_APB_S", true},
	{0, 12, 13, "TOPRGU_APB_S", true},
	{0, 13, 14, "RESERVED_APB_S", true},
	{0, 14, 15, "DEVICE_APC_INFRA_AO_APB_S", true},
	{0, 15, 16, "BCRM_INFRA_AO_APB_S", true},
	{0, 16, 17, "DEBUG_CTRL_INFRA_AO_APB_S", true},
	{0, 17, 18, "DEVICE_APC_PERI_AO_APB_S", true},
	{0, 18, 19, "BCRM_PERI_AO_APB_S", true},
	{0, 19, 20, "DEBUG_CTRL_PERI_AO_APB_S", true},

	/* 20 */
	{0, 20, 21, "AP_CIRQ_EINT_APB_S", true},
	{0, 21, 23, "PMIC_WRAP_APB_S", true},
	{0, 22, 24, "DEVICE_APC_AO_MM_APB_S", true},
	{0, 23, 25, "KP_APB_S", true},
	{0, 24, 26, "TOP_MISC_APB_S", true},
	{0, 25, 27, "DVFSRC_APB_S", true},
	{0, 26, 28, "MBIST_AO_APB_S", true},
	{0, 27, 29, "DPMAIF_AO_APB_S", true},
	{0, 28, 30, "DEVICE_MPU_AO_APB_S", true},
	{0, 29, 31, "SYS_TIMER_APB_S", true},

	/* 30 */
	{0, 30, 32, "MODEM_TEMP_SHARE_APB_S", true},
	{0, 31, 33, "DEVICE_APC_AO_MD_APB_S", true},
	{0, 32, 34, "PMIF1_APB_S", true},
	{0, 33, 35, "PMICSPI_MST_APB_S", true},
	{0, 34, 36, "TIA_APB_S", true},
	{0, 35, 37, "TOPCKGEN_INFRA_CFG_APB_S", true},
	{0, 36, 38, "DRM_DEBUG_TOP_APB_S", true},
	{0, 37, 62, "PWR_MD32_S", true},
	{0, 38, 63, "PWR_MD32_S-1", true},
	{0, 39, 64, "PWR_MD32_S-2", true},

	/* 40 */
	{0, 40, 65, "PWR_MD32_S-3", true},
	{0, 41, 66, "PWR_MD32_S-4", true},
	{0, 42, 67, "PWR_MD32_S-5", true},
	{0, 43, 68, "PWR_MD32_S-6", true},
	{0, 44, 69, "PWR_MD32_S-7", true},
	{0, 45, 70, "PWR_MD32_S-8", true},
	{0, 46, 71, "PWR_MD32_S-9", true},
	{0, 47, 72, "PWR_MD32_S-10", true},
	{0, 48, 73, "AUDIO_S", true},
	{0, 49, 74, "AUDIO_S-1", true},

	/* 50 */
	{0, 50, 119, "SSUSB_S", true},
	{0, 51, 120, "SSUSB_S-1", true},
	{0, 52, 121, "SSUSB_S-2", true},
	{0, 53, 122, "UFS_S", true},
	{0, 54, 123, "UFS_S-1", true},
	{0, 55, 124, "UFS_S-2", true},
	{0, 56, 125, "UFS_S-3", true},
	{0, 57, 126, "DEBUGSYS_APB_S", true},
	{0, 58, 127, "DRAMC_MD32_S0_APB_S", true},
	{0, 59, 128, "DRAMC_MD32_S0_APB_S-1", true},

	/* 60 */
	{0, 60, 129, "DRAMC_MD32_S1_APB_S", true},
	{0, 61, 130, "DRAMC_MD32_S1_APB_S-1", true},
	{0, 62, 137, "NOR_AXI_S", true},
	{0, 63, 143, "PCIE_AHB_S", true},
	{0, 64, 144, "DRAMC_CH0_TOP0_APB_S", true},
	{0, 65, 145, "DRAMC_CH0_TOP1_APB_S", true},
	{0, 66, 146, "DRAMC_CH0_TOP2_APB_S", true},
	{0, 67, 147, "DRAMC_CH0_TOP3_APB_S", true},
	{0, 68, 148, "DRAMC_CH0_TOP4_APB_S", true},
	{0, 69, 149, "DRAMC_CH0_TOP5_APB_S", true},

	/* 70 */
	{0, 70, 150, "DRAMC_CH0_TOP6_APB_S", true},
	{0, 71, 151, "DRAMC_CH1_TOP0_APB_S", true},
	{0, 72, 152, "DRAMC_CH1_TOP1_APB_S", true},
	{0, 73, 153, "DRAMC_CH1_TOP2_APB_S", true},
	{0, 74, 154, "DRAMC_CH1_TOP3_APB_S", true},
	{0, 75, 155, "DRAMC_CH1_TOP4_APB_S", true},
	{0, 76, 156, "DRAMC_CH1_TOP5_APB_S", true},
	{0, 77, 157, "DRAMC_CH1_TOP6_APB_S", true},
	{0, 78, 158, "DRAMC_CH2_TOP0_APB_S", true},
	{0, 79, 159, "DRAMC_CH2_TOP1_APB_S", true},

	/* 80 */
	{0, 80, 160, "DRAMC_CH2_TOP2_APB_S", true},
	{0, 81, 161, "DRAMC_CH2_TOP3_APB_S", true},
	{0, 82, 162, "DRAMC_CH2_TOP4_APB_S", true},
	{0, 83, 163, "DRAMC_CH2_TOP5_APB_S", true},
	{0, 84, 164, "DRAMC_CH2_TOP6_APB_S", true},
	{0, 85, 165, "DRAMC_CH3_TOP0_APB_S", true},
	{0, 86, 166, "DRAMC_CH3_TOP1_APB_S", true},
	{0, 87, 167, "DRAMC_CH3_TOP2_APB_S", true},
	{0, 88, 168, "DRAMC_CH3_TOP3_APB_S", true},
	{0, 89, 169, "DRAMC_CH3_TOP4_APB_S", true},

	/* 90 */
	{0, 90, 170, "DRAMC_CH3_TOP5_APB_S", true},
	{0, 91, 171, "DRAMC_CH3_TOP6_APB_S", true},
	{0, 92, 174, "CCIF2_AP_APB_S", true},
	{0, 93, 175, "CCIF2_MD_APB_S", true},
	{0, 94, 176, "CCIF3_AP_APB_S", true},
	{0, 95, 177, "CCIF3_MD_APB_S", true},
	{0, 96, 178, "CCIF4_AP_APB_S", true},
	{0, 97, 179, "CCIF4_MD_APB_S", true},
	{0, 98, 180, "INFRA_BUS_TRACE_APB_S", true},
	{0, 99, 181, "CCIF5_AP_APB_S", true},

	/* 100 */
	{0, 100, 182, "CCIF5_MD_APB_S", true},
	{0, 101, 183, "SSC_INFRA_APB0_S", true},
	{0, 102, 184, "SSC_INFRA_APB1_S", true},
	{0, 103, 185, "SSC_INFRA_APB2_S", true},
	{0, 104, 186, "DEVICE_MPU_ACP_APB_S", true},
	{1, 0, 39, "TINYSYS", true},
	{1, 1, 40, "TINYSYS", true},
	{1, 2, 41, "TINYSYS", true},
	{1, 3, 42, "TINYSYS", true},
	{1, 4, 43, "TINYSYS", true},

	/* 110 */
	{1, 5, 44, "TINYSYS", true},
	{1, 6, 45, "TINYSYS", true},
	{1, 7, 46, "TINYSYS", true},
	{1, 8, 47, "TINYSYS", true},
	{1, 9, 48, "TINYSYS", true},
	{1, 10, 49, "TINYSYS", true},
	{1, 11, 50, "TINYSYS", true},
	{1, 12, 51, "TINYSYS", true},
	{1, 13, 52, "TINYSYS", true},
	{1, 14, 53, "TINYSYS", true},

	/* 120 */
	{1, 15, 54, "TINYSYS", true},
	{1, 16, 55, "TINYSYS", true},
	{1, 17, 56, "TINYSYS", true},
	{1, 18, 57, "TINYSYS", true},
	{1, 19, 58, "TINYSYS", true},
	{1, 20, 59, "TINYSYS", true},
	{1, 21, 60, "TINYSYS", true},
	{1, 22, 61, "TINYSYS", true},
	{1, 23, 76, "MDSYS", true},
	{1, 24, 77, "MDSYS", true},

	/* 130 */
	{1, 25, 78, "MDSYS", true},
	{1, 26, 79, "MDSYS", true},
	{1, 27, 80, "MDSYS", true},
	{1, 28, 81, "MDSYS", true},
	{1, 29, 82, "MDSYS", true},
	{1, 30, 83, "MDSYS", true},
	{1, 31, 84, "MDSYS", true},
	{1, 32, 85, "MDSYS", true},
	{1, 33, 86, "MDSYS", true},
	{1, 34, 87, "MDSYS", true},

	/* 140 */
	{1, 35, 88, "MDSYS", true},
	{1, 36, 89, "MDSYS", true},
	{1, 37, 90, "MDSYS", true},
	{1, 38, 91, "MDSYS", true},
	{1, 39, 92, "MDSYS", true},
	{1, 40, 93, "MDSYS", true},
	{1, 41, 94, "MDSYS", true},
	{1, 42, 95, "MDSYS", true},
	{1, 43, 96, "MDSYS", true},
	{1, 44, 97, "MDSYS", true},

	/* 150 */
	{1, 45, 98, "MDSYS", true},
	{1, 46, 99, "MDSYS", true},
	{1, 47, 100, "MDSYS", true},
	{1, 48, 101, "MDSYS", true},
	{1, 49, 102, "MDSYS", true},
	{1, 50, 103, "MDSYS", true},
	{1, 51, 104, "MDSYS", true},
	{1, 52, 105, "MDSYS", true},
	{1, 53, 106, "MDSYS", true},
	{1, 54, 107, "MDSYS", true},

	/* 160 */
	{1, 55, 108, "MDSYS", true},
	{1, 56, 109, "MDSYS", true},
	{1, 57, 110, "MDSYS", true},
	{1, 58, 111, "MDSYS", true},
	{1, 59, 112, "MDSYS", true},
	{1, 60, 113, "MDSYS", true},
	{1, 61, 114, "MDSYS", true},
	{1, 62, 115, "MDSYS", true},
	{1, 63, 116, "MDSYS", true},
	{1, 64, 117, "MDSYS", true},

	/* 170 */
	{1, 65, 118, "MDSYS", true},
	{2, 0, 75, "CONNSYS", true},
	{-1, -1, 187, "OOB_way_en", true},
	{-1, -1, 188, "OOB_way_en", true},
	{-1, -1, 189, "OOB_way_en", true},
	{-1, -1, 190, "OOB_way_en", true},
	{-1, -1, 191, "OOB_way_en", true},
	{-1, -1, 192, "OOB_way_en", true},
	{-1, -1, 193, "OOB_way_en", true},
	{-1, -1, 194, "OOB_way_en", true},

	/* 180 */
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

	/* 190 */
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

	/* 200 */
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

	/* 210 */
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

	/* 220 */
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

	/* 230 */
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

	/* 240 */
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

	/* 250 */
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

	/* 260 */
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

	/* 270 */
	{-1, -1, 285, "OOB_way_en", true},
	{-1, -1, 286, "Decode_error", true},
	{-1, -1, 287, "Decode_error", true},
	{-1, -1, 288, "Decode_error", true},
	{-1, -1, 289, "Decode_error", true},
	{-1, -1, 290, "Decode_error", true},
	{-1, -1, 291, "Decode_error", true},
	{-2, -2, 292, "MDP_MALI", true},
	{-1, -1, 293, "reserve", false},
	{-2, -2, 294, "MMSYS_MALI", true},

	/* 280 */
	{-1, -1, 295, "PMIC_WRAP", false},
	{-1, -1, 296, "PMIF1", false},
	{-1, -1, 297, "DEVICE_APC_PERI_AO", false},
	{-1, -1, 298, "DEVICE_APC_PERI_PDN", false},

};

static struct mtk_device_info mt6873_devices_peri2[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "EFUSE_DEBUG_AO_APB_S", true},
	{0, 1, 1, "APXGPT_APB_S", true},
	{0, 2, 2, "SEJ_APB_S", true},
	{0, 3, 3, "AES_TOP0_APB_S", true},
	{0, 4, 4, "SECURITY_AO_APB_S", true},
	{0, 5, 5, "DEVICE_APC_PERI_AO2_APB_S", true},
	{0, 6, 6, "BCRM_PERI_AO2_APB_S", true},
	{0, 7, 7, "DEBUG_CTRL_PERI_AO2_APB_S", true},
	{0, 8, 8, "SPMI_MST_APB_S", true},
	{0, 9, 9, "DEBUG_CTRL_FMEM_AO_APB_S", true},

	/* 10 */
	{0, 10, 10, "BCRM_FMEM_AO_APB_S", true},
	{0, 11, 11, "DEVICE_APC_FMEM_AO_APB_S", true},
	{0, 12, 12, "PWM_APB_S", true},
	{0, 13, 13, "GCE_APB_S", true},
	{0, 14, 14, "GCE_APB_S-1", true},
	{0, 15, 15, "GCE_APB_S-2", true},
	{0, 16, 16, "GCE_APB_S-3", true},
	{0, 17, 17, "DPMAIF_PDN_APB_S", true},
	{0, 18, 18, "DPMAIF_PDN_APB_S-1", true},
	{0, 19, 19, "DPMAIF_PDN_APB_S-2", true},

	/* 20 */
	{0, 20, 20, "DPMAIF_PDN_APB_S-3", true},
	{0, 21, 21, "BND_EAST_APB0_S", true},
	{0, 22, 22, "BND_EAST_APB1_S", true},
	{0, 23, 23, "BND_EAST_APB2_S", true},
	{0, 24, 24, "BND_EAST_APB3_S", true},
	{0, 25, 25, "BND_EAST_APB4_S", true},
	{0, 26, 26, "BND_EAST_APB5_S", true},
	{0, 27, 27, "BND_EAST_APB6_S", true},
	{0, 28, 28, "BND_EAST_APB7_S", true},
	{0, 29, 29, "BND_EAST_APB8_S", true},

	/* 30 */
	{0, 30, 30, "BND_EAST_APB9_S", true},
	{0, 31, 31, "BND_EAST_APB10_S", true},
	{0, 32, 32, "BND_EAST_APB11_S", true},
	{0, 33, 33, "BND_EAST_APB12_S", true},
	{0, 34, 34, "BND_EAST_APB13_S", true},
	{0, 35, 35, "BND_EAST_APB14_S", true},
	{0, 36, 36, "BND_EAST_APB15_S", true},
	{0, 37, 37, "BND_WEST_APB0_S", true},
	{0, 38, 38, "BND_WEST_APB1_S", true},
	{0, 39, 39, "BND_WEST_APB2_S", true},

	/* 40 */
	{0, 40, 40, "BND_WEST_APB3_S", true},
	{0, 41, 41, "BND_WEST_APB4_S", true},
	{0, 42, 42, "BND_WEST_APB5_S", true},
	{0, 43, 43, "BND_WEST_APB6_S", true},
	{0, 44, 44, "BND_WEST_APB7_S", true},
	{0, 45, 45, "BND_NORTH_APB0_S", true},
	{0, 46, 46, "BND_NORTH_APB1_S", true},
	{0, 47, 47, "BND_NORTH_APB2_S", true},
	{0, 48, 48, "BND_NORTH_APB3_S", true},
	{0, 49, 49, "BND_NORTH_APB4_S", true},

	/* 50 */
	{0, 50, 50, "BND_NORTH_APB5_S", true},
	{0, 51, 51, "BND_NORTH_APB6_S", true},
	{0, 52, 52, "BND_NORTH_APB7_S", true},
	{0, 53, 53, "BND_NORTH_APB8_S", true},
	{0, 54, 54, "BND_NORTH_APB9_S", true},
	{0, 55, 55, "BND_NORTH_APB10_S", true},
	{0, 56, 56, "BND_NORTH_APB11_S", true},
	{0, 57, 57, "BND_NORTH_APB12_S", true},
	{0, 58, 58, "BND_NORTH_APB13_S", true},
	{0, 59, 59, "BND_NORTH_APB14_S", true},

	/* 60 */
	{0, 60, 60, "BND_NORTH_APB15_S", true},
	{0, 61, 61, "BND_SOUTH_APB0_S", true},
	{0, 62, 62, "BND_SOUTH_APB1_S", true},
	{0, 63, 63, "BND_SOUTH_APB2_S", true},
	{0, 64, 64, "BND_SOUTH_APB3_S", true},
	{0, 65, 65, "BND_SOUTH_APB4_S", true},
	{0, 66, 66, "BND_SOUTH_APB5_S", true},
	{0, 67, 67, "BND_SOUTH_APB6_S", true},
	{0, 68, 68, "BND_SOUTH_APB7_S", true},
	{0, 69, 69, "BND_SOUTH_APB8_S", true},

	/* 70 */
	{0, 70, 70, "BND_SOUTH_APB9_S", true},
	{0, 71, 71, "BND_SOUTH_APB10_S", true},
	{0, 72, 72, "BND_SOUTH_APB11_S", true},
	{0, 73, 73, "BND_SOUTH_APB12_S", true},
	{0, 74, 74, "BND_SOUTH_APB13_S", true},
	{0, 75, 75, "BND_SOUTH_APB14_S", true},
	{0, 76, 76, "BND_SOUTH_APB15_S", true},
	{0, 77, 77, "BND_EAST_NORTH_APB0_S", true},
	{0, 78, 78, "BND_EAST_NORTH_APB1_S", true},
	{0, 79, 79, "BND_EAST_NORTH_APB2_S", true},

	/* 80 */
	{0, 80, 80, "BND_EAST_NORTH_APB3_S", true},
	{0, 81, 81, "BND_EAST_NORTH_APB4_S", true},
	{0, 82, 82, "BND_EAST_NORTH_APB5_S", true},
	{0, 83, 83, "BND_EAST_NORTH_APB6_S", true},
	{0, 84, 84, "BND_EAST_NORTH_APB7_S", true},
	{0, 85, 85, "SYS_CIRQ_APB_S", true},
	{0, 86, 86, "EFUSE_DEBUG_PDN_APB_S", true},
	{0, 87, 87, "DEVICE_APC_INFRA_PDN_APB_S", true},
	{0, 88, 88, "DEBUG_TRACKER_APB_S", true},
	{0, 89, 89, "CCIF0_AP_APB_S", true},

	/* 90 */
	{0, 90, 90, "CCIF0_MD_APB_S", true},
	{0, 91, 91, "CCIF1_AP_APB_S", true},
	{0, 92, 92, "CCIF1_MD_APB_S", true},
	{0, 93, 93, "MBIST_PDN_APB_S", true},
	{0, 94, 94, "INFRACFG_PDN_APB_S", true},
	{0, 95, 95, "TRNG_APB_S", true},
	{0, 96, 96, "DX_CC_APB_S", true},
	{0, 97, 97, "CQ_DMA_APB_S", true},
	{0, 98, 98, "SRAMROM_APB_S", true},
	{0, 99, 99, "INFRACFG_MEM_APB_S", true},

	/* 100 */
	{0, 100, 100, "RESERVED_DVFS_PROC_APB_S", true},
	{0, 101, 103, "SYS_CIRQ1_APB_S", true},
	{0, 102, 104, "SYS_CIRQ2_APB_S", true},
	{0, 103, 105, "DEBUG_TRACKER_APB1_S", true},
	{0, 104, 106, "EMI_APB_S", true},
	{0, 105, 107, "EMI_MPU_APB_S", true},
	{0, 106, 108, "DEVICE_MPU_PDN_APB_S", true},
	{0, 107, 109, "APDMA_APB_S", true},
	{0, 108, 110, "DEBUG_TRACKER_APB2_S", true},
	{0, 109, 111, "BCRM_INFRA_PDN_APB_S", true},

	/* 110 */
	{0, 110, 112, "BCRM_PERI_PDN_APB_S", true},
	{0, 111, 113, "BCRM_PERI_PDN2_APB_S", true},
	{0, 112, 114, "DEVICE_APC_PERI_PDN_APB_S", true},
	{0, 113, 115, "DEVICE_APC_PERI_PDN2_APB_S", true},
	{0, 114, 116, "BCRM_FMEM_PDN_APB_S", true},
	{-1, -1, 117, "OOB_way_en", true},
	{-1, -1, 118, "OOB_way_en", true},
	{-1, -1, 119, "OOB_way_en", true},
	{-1, -1, 120, "OOB_way_en", true},
	{-1, -1, 121, "OOB_way_en", true},

	/* 120 */
	{-1, -1, 122, "OOB_way_en", true},
	{-1, -1, 123, "OOB_way_en", true},
	{-1, -1, 124, "OOB_way_en", true},
	{-1, -1, 125, "OOB_way_en", true},
	{-1, -1, 126, "OOB_way_en", true},
	{-1, -1, 127, "OOB_way_en", true},
	{-1, -1, 128, "OOB_way_en", true},
	{-1, -1, 129, "OOB_way_en", true},
	{-1, -1, 130, "OOB_way_en", true},
	{-1, -1, 131, "OOB_way_en", true},

	/* 130 */
	{-1, -1, 132, "OOB_way_en", true},
	{-1, -1, 133, "OOB_way_en", true},
	{-1, -1, 134, "OOB_way_en", true},
	{-1, -1, 135, "OOB_way_en", true},
	{-1, -1, 136, "OOB_way_en", true},
	{-1, -1, 137, "OOB_way_en", true},
	{-1, -1, 138, "OOB_way_en", true},
	{-1, -1, 139, "OOB_way_en", true},
	{-1, -1, 140, "OOB_way_en", true},
	{-1, -1, 141, "OOB_way_en", true},

	/* 140 */
	{-1, -1, 142, "OOB_way_en", true},
	{-1, -1, 143, "OOB_way_en", true},
	{-1, -1, 144, "OOB_way_en", true},
	{-1, -1, 145, "OOB_way_en", true},
	{-1, -1, 146, "OOB_way_en", true},
	{-1, -1, 147, "OOB_way_en", true},
	{-1, -1, 148, "OOB_way_en", true},
	{-1, -1, 149, "OOB_way_en", true},
	{-1, -1, 150, "OOB_way_en", true},
	{-1, -1, 151, "OOB_way_en", true},

	/* 150 */
	{-1, -1, 152, "OOB_way_en", true},
	{-1, -1, 153, "OOB_way_en", true},
	{-1, -1, 154, "OOB_way_en", true},
	{-1, -1, 155, "OOB_way_en", true},
	{-1, -1, 156, "OOB_way_en", true},
	{-1, -1, 157, "OOB_way_en", true},
	{-1, -1, 158, "OOB_way_en", true},
	{-1, -1, 159, "OOB_way_en", true},
	{-1, -1, 160, "OOB_way_en", true},
	{-1, -1, 161, "OOB_way_en", true},

	/* 160 */
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

	/* 170 */
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

	/* 180 */
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

	/* 190 */
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

	/* 200 */
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

	/* 210 */
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

	/* 220 */
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

	/* 230 */
	{-1, -1, 232, "OOB_way_en", true},
	{-1, -1, 233, "OOB_way_en", true},
	{-1, -1, 234, "Decode_error", true},
	{-1, -1, 235, "Decode_error", true},
	{-1, -1, 236, "Decode_error", true},
	{-1, -1, 237, "Decode_error", true},
	{-1, -1, 238, "Decode_error", true},
	{-1, -1, 239, "Decode_error", true},
	{-1, -1, 240, "Decode_error", true},
	{-1, -1, 241, "Decode_error", true},

	/* 240 */
	{-1, -1, 242, "CQ_DMA", false},
	{-1, -1, 243, "EMI", false},
	{-1, -1, 244, "EMI_MPU", false},
	{-1, -1, 245, "GCE", false},
	{-1, -1, 246, "AP_DMA", false},
	{-1, -1, 347, "DEVICE_APC_PERI_AO2", false},
	{-1, -1, 248, "DEVICE_APC_PERI_PDN2", false},

};

static struct mtk_device_info mt6873_devices_peri_par[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "AUXADC_APB_S", true},
	{0, 1, 1, "UART0_APB_S", true},
	{0, 2, 2, "UART1_APB_S", true},
	{0, 3, 3, "UART2_APB_S", true},
	{0, 4, 4, "IIC_P2P_REMAP_APB4_S", true},
	{0, 5, 5, "SPI0_APB_S", true},
	{0, 6, 6, "PTP_THERM_CTRL_APB_S", true},
	{0, 7, 7, "BTIF_APB_S", true},
	{0, 8, 8, "DISP_PWM_APB_S", true},
	{0, 9, 9, "SPI1_APB_S", true},

	/* 10 */
	{0, 10, 10, "SPI2_APB_S", true},
	{0, 11, 11, "SPI3_APB_S", true},
	{0, 12, 12, "IIC_P2P_REMAP_APB0_S", true},
	{0, 13, 13, "IIC_P2P_REMAP_APB1_S", true},
	{0, 14, 14, "SPI4_APB_S", true},
	{0, 15, 15, "SPI5_APB_S", true},
	{0, 16, 16, "IIC_P2P_REMAP_APB2_S", true},
	{0, 17, 17, "IIC_P2P_REMAP_APB3_S", true},
	{0, 18, 18, "SPI6_APB_S", true},
	{0, 19, 19, "SPI7_APB_S", true},

	/* 20 */
	{0, 20, 20, "BCRM_PERI_PAR_PDN_APB_S", true},
	{0, 21, 21, "DEVICE_APC_PERI_PAR_PDN_APB_S", true},
	{0, 22, 22, "PTP_THERM_CTRL2_APB_S", true},
	{0, 23, 23, "NOR_APB_S", true},
	{0, 24, 24, "DEVICE_APC_PERI_PAR_AO_APB_S", true},
	{0, 25, 25, "DEBUG_CTRL_PERI_PAR_AO_APB_S", true},
	{0, 26, 26, "BCRM_PERI_PAR_AO_APB_S", true},
	{-1, -1, 27, "OOB_way_en", true},
	{-1, -1, 28, "OOB_way_en", true},
	{-1, -1, 29, "OOB_way_en", true},

	/* 30 */
	{-1, -1, 30, "OOB_way_en", true},
	{-1, -1, 31, "OOB_way_en", true},
	{-1, -1, 32, "OOB_way_en", true},
	{-1, -1, 33, "OOB_way_en", true},
	{-1, -1, 34, "OOB_way_en", true},
	{-1, -1, 35, "OOB_way_en", true},
	{-1, -1, 36, "OOB_way_en", true},
	{-1, -1, 37, "OOB_way_en", true},
	{-1, -1, 38, "OOB_way_en", true},
	{-1, -1, 39, "OOB_way_en", true},

	/* 40 */
	{-1, -1, 40, "OOB_way_en", true},
	{-1, -1, 41, "OOB_way_en", true},
	{-1, -1, 42, "OOB_way_en", true},
	{-1, -1, 43, "OOB_way_en", true},
	{-1, -1, 44, "OOB_way_en", true},
	{-1, -1, 45, "OOB_way_en", true},
	{-1, -1, 46, "OOB_way_en", true},
	{-1, -1, 47, "OOB_way_en", true},
	{-1, -1, 48, "OOB_way_en", true},
	{-1, -1, 49, "OOB_way_en", true},

	/* 50 */
	{-1, -1, 50, "OOB_way_en", true},
	{-1, -1, 51, "OOB_way_en", true},
	{-1, -1, 52, "OOB_way_en", true},
	{-1, -1, 53, "OOB_way_en", true},
	{-1, -1, 54, "OOB_way_en", true},
	{-1, -1, 55, "OOB_way_en", true},
	{-1, -1, 56, "Decode_error", true},
	{-1, -1, 57, "Decode_error", true},
	{-1, -1, 58, "DISP_PWM", false},
	{-1, -1, 59, "IMP_IIC_WRAP", false},

	/* 60 */
	{-1, -1, 60, "DEVICE_APC_PERI_PAR__AO", false},
	{-1, -1, 61, "DEVICE_APC_PERI_PAR_PDN", false},
};

static struct mtk_device_num mtk6873_devices_num[] = {
	{SLAVE_TYPE_INFRA, VIO_SLAVE_NUM_INFRA},
	{SLAVE_TYPE_PERI, VIO_SLAVE_NUM_PERI},
	{SLAVE_TYPE_PERI2, VIO_SLAVE_NUM_PERI2},
	{SLAVE_TYPE_PERI_PAR, VIO_SLAVE_NUM_PERI_PAR},
};

static struct PERIAXI_ID_INFO peri_mi_id_to_master[] = {
	{"THERM2",       { 0, 0, 0 } },
	{"SPM",          { 0, 1, 0 } },
	{"CCU",          { 0, 0, 1 } },
	{"THERM",        { 0, 1, 1 } },
	{"SPM_DRAMC",    { 1, 1, 0 } },
};

static struct INFRAAXI_ID_INFO infra_mi_id_to_master[] = {
	{"CONNSYS_WFDMA",     { 0, 0, 0, 0,	0, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"CONNSYS_ICAP",      { 0, 0, 0, 0,	1, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"CONNSYS_MCU_SYS",   { 0, 0, 0, 0,	0, 1, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"CONNSYS_GPS",       { 0, 0, 0, 0,	1, 1, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"Tinysys",           { 0, 1, 0, 0,	2, 2, 2, 2,	2, 2, 0, 0,	0, 0 } },
	{"CQ_DMA",            { 0, 0, 1, 0,	0, 0, 0, 2,	2, 2, 0, 0,	0, 0 } },
	{"DebugTop",          { 0, 0, 1, 0,	1, 0, 0, 2,	0, 0, 0, 0,	0, 0 } },
	{"SSUSB",             { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 0, 0,	2, 0 } },
	{"SSUSB2",            { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 1, 0,	2, 0 } },
	{"NOR",               { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 0, 1,	2, 0 } },
	{"PWM",               { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 1, 1,	0, 0 } },
	{"SPI6",              { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 1, 1,	0, 0 } },
	{"SPI0",              { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 1, 1,	1, 0 } },
	{"APU",               { 0, 0, 1, 0,	0, 1, 0, 1,	0, 0, 2, 2,	0, 0 } },
	{"SPI2",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 0, 0,	0, 0 } },
	{"SPI3",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 1, 0,	0, 0 } },
	{"SPI4",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 0, 1,	0, 0 } },
	{"SPI5",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 1, 1,	0, 0 } },
	{"SPI7",              { 0, 0, 1, 0,	0, 1, 0, 1,	1, 0, 0, 0,	0, 0 } },
	{"Audio",             { 0, 0, 1, 0,	0, 1, 0, 1,	1, 0, 0, 1,	0, 0 } },
	{"SPI1",              { 0, 0, 1, 0,	0, 1, 0, 1,	1, 0, 1, 1,	0, 0 } },
	{"AP_DMA_EXT",        { 0, 0, 1, 0,	0, 1, 0, 0,	0, 1, 2, 2,	2, 0 } },
	{"THERM2",            { 0, 0, 1, 0,	0, 1, 0, 1,	0, 1, 0, 0,	0, 0 } },
	{"SPM",               { 0, 0, 1, 0,	0, 1, 0, 1,	0, 1, 1, 0,	0, 0 } },
	{"CCU",               { 0, 0, 1, 0,	0, 1, 0, 1,	0, 1, 0, 1,	0, 0 } },
	{"THERM",             { 0, 0, 1, 0,	0, 1, 0, 1,	0, 1, 1, 1,	0, 0 } },
	{"DX_CC",             { 0, 0, 1, 0,	1, 1, 0, 2,	2, 2, 2, 0,	0, 0 } },
	{"GCE",               { 0, 0, 1, 0,	0, 0, 1, 2,	2, 0, 0, 0,	0, 0 } },
	{"PCIE",              { 0, 0, 1, 0,	0, 1, 1, 2,	2, 2, 2, 2,	0, 0 } },
	{"DPMAIF",            { 0, 1, 1, 0,	2, 2, 2, 2,	0, 0, 0, 0,	0, 0 } },
	{"SSPM",              { 0, 0, 0, 1,	2, 2, 2, 0,	0, 0, 0, 0,	0, 0 } },
	{"UFS",               { 0, 1, 0, 1,	0, 2, 2, 0,	0, 0, 0, 0,	0, 0 } },
	{"MSDC0",             { 0, 1, 0, 1,	1, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"MSDC1",             { 0, 1, 0, 1,	1, 1, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"MSDC2",             { 0, 1, 0, 1,	1, 1, 1, 0,	0, 0, 0, 0,	0, 0 } },
	{"CPUEB",             { 0, 0, 1, 1,	2, 2, 2, 2,	2, 2, 0, 0,	0, 0 } },
	{"APMCU_write",       { 1, 2, 2, 2,	2, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_write",       { 1, 2, 2, 2,	2, 0, 0, 1,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_write",       { 1, 2, 2, 2,	2, 2, 2, 2,	2, 1, 0, 0,	0, 0 } },
	{"APMCU_read",        { 1, 2, 2, 2,	2, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_read",        { 1, 2, 2, 2,	2, 0, 0, 1,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_read",        { 1, 2, 0, 0,	0, 0, 0, 0,	1, 0, 0, 0,	0, 0 } },
	{"APMCU_read",        { 1, 2, 1, 0,	0, 0, 0, 0,	1, 0, 0, 0,	0, 0 } },
	{"APMCU_read",        { 1, 2, 2, 2,	2, 2, 2, 2,	2, 1, 0, 0,	0, 0 } },
	{"APMCU_read",        { 1, 2, 2, 2,	2, 2, 2, 2,	2, 2, 1, 0,	0, 0 } },
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

static const char *peri_mi_trans(uint32_t bus_id)
{
	int master_count = ARRAY_SIZE(peri_mi_id_to_master);
	const char *master = "UNKNOWN_MASTER_FROM_PERI";
	int i, j;

	if ((bus_id & 0x3) == 0x0)
		return infra_mi_trans(bus_id >> 2);
	else if ((bus_id & 0x3) == 0x2)
		return "MD_AP_M";
	else if ((bus_id & 0x3) == 0x3)
		return "AP_DMA_M";

	bus_id = bus_id >> 2;

	for (i = 0 ; i < master_count; i++) {
		for (j = 0 ; j < PERIAXI_MI_BIT_LENGTH ; j++) {
			if (peri_mi_id_to_master[i].bit[j] == 2)
				continue;
			if (((bus_id >> j) & 0x1) ==
					peri_mi_id_to_master[i].bit[j])
				continue;
			break;
		}
		if (j == PERIAXI_MI_BIT_LENGTH) {
			pr_debug(PFX "%s %s %s\n",
				"catch it from PERIAXI_MI.",
				"Master is:",
				peri_mi_id_to_master[i].master);
			master = peri_mi_id_to_master[i].master;
		}
	}

	return master;
}

static const char *mt6873_bus_id_to_master(uint32_t bus_id, uint32_t vio_addr,
		int slave_type, int shift_sta_bit, int domain)
{
	uint8_t h_1byte;

	pr_debug(PFX "[DEVAPC] %s:0x%x, %s:0x%x, %s:0x%x, %s:%d\n",
		"bus_id", bus_id, "vio_addr", vio_addr,
		"slave_type", slave_type,
		"shift_sta_bit", shift_sta_bit);

	if (bus_id == 0x0 && vio_addr == 0x0)
		return NULL;

	h_1byte = (vio_addr >> 24) & 0xFF;

	if ((vio_addr >= TINYSYS_START_ADDR && vio_addr <= TINYSYS_END_ADDR) ||
	    (vio_addr >= MD_START_ADDR && vio_addr <= MD_END_ADDR)) {
		pr_info(PFX "[DEVAPC] bus_id might be wrong\n");

		if (domain == 0x1)
			return "SSPM";
		else if (domain == 0x2)
			return "CONNSYS";

	} else if (vio_addr >= CONN_START_ADDR && vio_addr <= CONN_END_ADDR) {
		pr_info(PFX "[DEVAPC] bus_id might be wrong\n");

		if (domain == 0x1)
			return "MD";
		else if (domain == 0x2)
			return "ADSP";
	}

	if (slave_type == SLAVE_TYPE_INFRA) {
		if (vio_addr <= 0x1FFFFF) {
			pr_info(PFX "vio_addr is from on-chip SRAMROM\n");
			if ((bus_id & 0x1) == 0)
				return "EMI_L2C_M";

			return infra_mi_trans(bus_id >> 1);

		} else if (shift_sta_bit == 3) {
			if ((bus_id & 0x1) == 0)
				return "EMI_L2C_M";

			return infra_mi_trans(bus_id >> 1);

		} else if (shift_sta_bit == 4) {
			if ((bus_id & 0x1) == 1)
				return "GCE_M";

			return infra_mi_trans(bus_id >> 1);
		}

		return infra_mi_trans(bus_id);

	} else if (slave_type == SLAVE_TYPE_PERI) {
		if ((h_1byte >= 0x14 && h_1byte < 0x18) ||
				(h_1byte >= 0x1A && h_1byte < 0x1C) ||
				(h_1byte >= 0x1F && h_1byte < 0x20)) {
			pr_info(PFX "vio addr is from MM 2nd\n");
			if ((bus_id & 0x1) == 1)
				return "GCE_M";

			return infra_mi_trans(bus_id >> 1);
		}

		if (shift_sta_bit == 3 || shift_sta_bit == 4 ||
				shift_sta_bit == 8) {
			if ((bus_id & 0x1) == 0)
				return "MD_AP_M";

			return peri_mi_trans(bus_id >> 1);
		}
		return peri_mi_trans(bus_id);

	} else if (slave_type == SLAVE_TYPE_PERI2) {
		return peri_mi_trans(bus_id);

	} else if (slave_type == SLAVE_TYPE_PERI_PAR) {
		return peri_mi_trans(bus_id);

	}

	return "UNKNOWN_MASTER";
}

/* violation index corresponds to subsys */
const char *index_to_subsys(int slave_type, uint32_t vio_index,
		uint32_t vio_addr)
{
	int i;

	/* Filter by violation address */
	if ((vio_addr & 0xFF000000) == CONN_START_ADDR)
		return "CONNSYS";

	/* Filter by violation index */
	if (slave_type == SLAVE_TYPE_INFRA &&
			vio_index < VIO_SLAVE_NUM_INFRA) {
		for (i = 0; i < VIO_SLAVE_NUM_INFRA; i++) {
			if (vio_index == mt6873_devices_infra[i].vio_index)
				return mt6873_devices_infra[i].device;
		}

	} else if (slave_type == SLAVE_TYPE_PERI &&
			vio_index < VIO_SLAVE_NUM_PERI)
		for (i = 0; i < VIO_SLAVE_NUM_PERI; i++) {
			if (vio_index == mt6873_devices_peri[i].vio_index)
				return mt6873_devices_peri[i].device;
		}

	else if (slave_type == SLAVE_TYPE_PERI2 &&
			vio_index < VIO_SLAVE_NUM_PERI2)
		for (i = 0; i < VIO_SLAVE_NUM_PERI2; i++) {
			if (vio_index == mt6873_devices_peri2[i].vio_index)
				return mt6873_devices_peri2[i].device;
		}

	else if (slave_type == SLAVE_TYPE_PERI_PAR &&
			vio_index < VIO_SLAVE_NUM_PERI_PAR)
		for (i = 0; i < VIO_SLAVE_NUM_PERI_PAR; i++) {
			if (vio_index == mt6873_devices_peri_par[i].vio_index)
				return mt6873_devices_peri_par[i].device;
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

		strncpy(mm_str, "INFRACFG_MDP_SEC_VIO",
				sizeof("INFRACFG_MDP_SEC_VIO"));

	} else if (mmsys_vio) {
		vio_sta_num = INFRACFG_MM_VIO_STA_NUM;
		vio0_offset = INFRACFG_MM_SEC_VIO0_OFFSET;

		strncpy(mm_str, "INFRACFG_MM_SEC_VIO",
				sizeof("INFRACFG_MM_SEC_VIO"));

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

static uint32_t mt6873_shift_group_get(int slave_type, uint32_t vio_idx)
{
	if (slave_type == SLAVE_TYPE_INFRA) {
		if ((vio_idx >= 0 && vio_idx <= 8) || vio_idx == 355)
			return 0;
		else if ((vio_idx >= 9 && vio_idx <= 14) || vio_idx == 356)
			return 1;
		else if ((vio_idx >= 15 && vio_idx <= 19) || vio_idx == 357)
			return 2;
		else if ((vio_idx >= 20 && vio_idx <= 21) || vio_idx == 358)
			return 3;
		else if (vio_idx >= 22 && vio_idx <= 347)
			return 4;
		else if ((vio_idx >= 348 && vio_idx <= 354) ||
				(vio_idx >= 359 && vio_idx <= 365) ||
				vio_idx == 366)
			return 5;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI) {
		if (vio_idx >= 0 && vio_idx <= 4)
			return 0;
		else if (vio_idx >= 5 && vio_idx <= 6)
			return 1;
		else if ((vio_idx >= 7 && vio_idx <= 38) || vio_idx == 187 ||
				(vio_idx >= 188 && vio_idx <= 219) ||
				vio_idx == 286)
			return 2;
		else if ((vio_idx >= 39 && vio_idx <= 61) || vio_idx == 220)
			return 3;
		else if ((vio_idx >= 62 && vio_idx <= 72) || vio_idx == 221)
			return 4;
		else if ((vio_idx >= 73 && vio_idx <= 74) || vio_idx == 222)
			return 5;
		else if (vio_idx == 75 || vio_idx == 223)
			return 6;
		else if ((vio_idx >= 76 && vio_idx <= 118) || vio_idx == 224)
			return 7;
		else if ((vio_idx >= 119 && vio_idx <= 121) || vio_idx == 225)
			return 8;
		if (vio_idx >= 122 && vio_idx <= 125)
			return 9;
		else if (vio_idx == 126 || (vio_idx >= 226 && vio_idx <= 227) ||
				vio_idx == 287)
			return 10;
		if (vio_idx >= 127 && vio_idx <= 128)
			return 11;
		if (vio_idx >= 129 && vio_idx <= 130)
			return 12;
		else if ((vio_idx >= 131 && vio_idx <= 141) ||
				(vio_idx >= 228 && vio_idx <= 238) ||
				vio_idx == 288)
			return 13;
		else if ((vio_idx >= 142 && vio_idx <= 143) ||
				(vio_idx >= 239 && vio_idx <= 240) ||
				vio_idx == 289)
			return 14;
		else if ((vio_idx >= 144 && vio_idx <= 173) || vio_idx == 241 ||
				(vio_idx >= 242 && vio_idx <= 271) ||
				vio_idx == 290)
			return 15;
		else if ((vio_idx >= 174 && vio_idx <= 186) || vio_idx == 272 ||
				(vio_idx >= 273 && vio_idx <= 285) ||
				vio_idx == 291)
			return 16;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI2) {
		if ((vio_idx >= 0 && vio_idx <= 12) || vio_idx == 117 ||
				(vio_idx >= 118 && vio_idx <= 130) ||
				vio_idx == 234)
			return 0;
		else if (vio_idx >= 13 && vio_idx <= 16)
			return 1;
		else if (vio_idx >= 17 && vio_idx <= 20)
			return 2;
		else if ((vio_idx >= 21 && vio_idx <= 36) || vio_idx == 131 ||
				(vio_idx >= 132 && vio_idx <= 147) ||
				vio_idx == 235)
			return 3;
		else if ((vio_idx >= 37 && vio_idx <= 44) || vio_idx == 148 ||
				(vio_idx >= 149 && vio_idx <= 156) ||
				vio_idx == 236)
			return 4;
		else if ((vio_idx >= 45 && vio_idx <= 60) || vio_idx == 157 ||
				(vio_idx >= 158 && vio_idx <= 173) ||
				vio_idx == 237)
			return 5;
		else if ((vio_idx >= 61 && vio_idx <= 76) || vio_idx == 174 ||
				(vio_idx >= 175 && vio_idx <= 190) ||
				vio_idx == 238)
			return 6;
		else if ((vio_idx >= 77 && vio_idx <= 84) || vio_idx == 191 ||
				(vio_idx >= 192 && vio_idx <= 199) ||
				vio_idx == 239)
			return 7;
		else if ((vio_idx >= 85 && vio_idx <= 105) || vio_idx == 200 ||
				(vio_idx >= 201 && vio_idx <= 221) ||
				vio_idx == 240)
			return 8;
		else if ((vio_idx >= 106 && vio_idx <= 116) || vio_idx == 222 ||
				(vio_idx >= 223 && vio_idx <= 233) ||
				vio_idx == 241)
			return 9;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI_PAR) {
		if ((vio_idx >= 0 && vio_idx <= 23) || vio_idx == 27 ||
				(vio_idx >= 28 && vio_idx <= 51) ||
				vio_idx == 56)
			return 0;
		else if ((vio_idx >= 24 && vio_idx <= 26) || vio_idx == 52 ||
				(vio_idx >= 53 && vio_idx <= 55) ||
				vio_idx == 57)
			return 1;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else {
		pr_err(PFX "%s:%d Wrong slave_type:0x%x\n",
				__func__, __LINE__, slave_type);
	}

	return 31;
}

void devapc_catch_illegal_range(phys_addr_t phys_addr, size_t size)
{
	/*
	 * Catch BROM addr mapped
	 */
	if (phys_addr >= 0x0 && phys_addr < SRAM_START_ADDR) {
		pr_err(PFX "%s: %s %s:(%pa), %s:(0x%lx)\n",
				"catch BROM address mapped!",
				__func__, "phys_addr", &phys_addr,
				"size", size);
		BUG_ON(1);
	}
}
EXPORT_SYMBOL(devapc_catch_illegal_range);

static struct mtk_devapc_dbg_status mt6873_devapc_dbg_stat = {
	.enable_ut = PLAT_DBG_UT_DEFAULT,
	.enable_KE = PLAT_DBG_KE_DEFAULT,
	.enable_AEE = PLAT_DBG_AEE_DEFAULT,
	.enable_WARN = PLAT_DBG_WARN_DEFAULT,
	.enable_dapc = PLAT_DBG_DAPC_DEFAULT,
};

static const char * const slave_type_to_str[] = {
	"SLAVE_TYPE_INFRA",
	"SLAVE_TYPE_PERI",
	"SLAVE_TYPE_PERI2",
	"SLAVE_TYPE_PERI_PAR",
	"WRONG_SLAVE_TYPE",
};

static int mtk_vio_mask_sta_num[] = {
	VIO_MASK_STA_NUM_INFRA,
	VIO_MASK_STA_NUM_PERI,
	VIO_MASK_STA_NUM_PERI2,
	VIO_MASK_STA_NUM_PERI_PAR,
};

static struct mtk_devapc_vio_info mt6873_devapc_vio_info = {
	.vio_mask_sta_num = mtk_vio_mask_sta_num,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
	.mdp_vio_idx = MDP_VIO_INDEX,
	.disp2_vio_idx = MDP_VIO_INDEX,
	.mmsys_vio_idx = MMSYS_VIO_INDEX,
	.sramrom_slv_type = SRAMROM_SLAVE_TYPE,
	.mm2nd_slv_type = MM2ND_SLAVE_TYPE,
};

static const struct mtk_infra_vio_dbg_desc mt6873_vio_dbgs = {
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

static const struct mtk_sramrom_sec_vio_desc mt6873_sramrom_sec_vios = {
	.vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const uint32_t mt6873_devapc_pds[] = {
	PD_VIO_MASK_OFFSET,
	PD_VIO_STA_OFFSET,
	PD_VIO_DBG0_OFFSET,
	PD_VIO_DBG1_OFFSET,
	PD_VIO_DBG2_OFFSET,
	PD_APC_CON_OFFSET,
	PD_SHIFT_STA_OFFSET,
	PD_SHIFT_SEL_OFFSET,
	PD_SHIFT_CON_OFFSET,
};

static struct mtk_devapc_soc mt6873_data = {
	.dbg_stat = &mt6873_devapc_dbg_stat,
	.slave_type_arr = slave_type_to_str,
	.slave_type_num = SLAVE_TYPE_NUM,
	.device_info[SLAVE_TYPE_INFRA] = mt6873_devices_infra,
	.device_info[SLAVE_TYPE_PERI] = mt6873_devices_peri,
	.device_info[SLAVE_TYPE_PERI2] = mt6873_devices_peri2,
	.device_info[SLAVE_TYPE_PERI_PAR] = mt6873_devices_peri_par,
	.ndevices = mtk6873_devices_num,
	.vio_info = &mt6873_devapc_vio_info,
	.vio_dbgs = &mt6873_vio_dbgs,
	.sramrom_sec_vios = &mt6873_sramrom_sec_vios,
	.devapc_pds = mt6873_devapc_pds,
	.subsys_get = &index_to_subsys,
	.master_get = &mt6873_bus_id_to_master,
	.mm2nd_vio_handler = &mm2nd_vio_handler,
	.shift_group_get = mt6873_shift_group_get,
};

static const struct of_device_id mt6873_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6873-devapc" },
	{},
};

static int mt6873_devapc_probe(struct platform_device *pdev)
{
	return mtk_devapc_probe(pdev, &mt6873_data);
}

static int mt6873_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6873_devapc_driver = {
	.probe = mt6873_devapc_probe,
	.remove = mt6873_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = mt6873_devapc_dt_match,
	},
};

module_platform_driver(mt6873_devapc_driver);

MODULE_DESCRIPTION("Mediatek MT6873 Device APC Driver");
MODULE_AUTHOR("Neal Liu <neal.liu@mediatek.com>");
MODULE_LICENSE("GPL");
