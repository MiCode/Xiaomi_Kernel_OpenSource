// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <asm-generic/io.h>

#include "devapc-mt6833.h"
#include "devapc-mtk-multi-ao.h"

static struct mtk_device_info mt6833_devices_infra[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "SPM_APB_S", true},
	{0, 1, 1, "SPM_APB_S-1", true},
	{0, 2, 2, "SPM_APB_S-2", true},
	{0, 3, 3, "SPM_APB_S-4", true},
	{0, 4, 4, "APMIXEDSYS_APB_S", true},
	{0, 5, 5, "APMIXEDSYS_APB_S-1", true},
	{0, 6, 6, "TOPCKGEN_APB_S", true},
	{0, 7, 7, "INFRACFG_AO_APB_S", true},
	{0, 8, 8, "INFRACFG_AO_MEM_APB_S", true},
	{0, 9, 9, "PERICFG_AO_APB_S", true},

	/* 10 */
	{0, 10, 10, "GPIO_APB_S", true},
	{0, 11, 12, "TOPRGU_APB_S", true},
	{0, 12, 13, "RESERVED_APB_S", true},
	{0, 13, 14, "DEVICE_APC_INFRA_AO_APB_S", true},
	{0, 14, 15, "BCRM_INFRA_AO_APB_S", true},
	{0, 15, 16, "DEBUG_CTRL_INFRA_AO_APB_S", true},
	{0, 16, 17, "AP_CIRQ_EINT_APB_S", true},
	{0, 17, 19, "PMIC_WRAP_APB_S", true},
	{0, 18, 20, "KP_APB_S", true},
	{0, 19, 21, "TOP_MISC_APB_S", true},

	/* 20 */
	{0, 20, 22, "DVFSRC_APB_S", true},
	{0, 21, 23, "MBIST_AO_APB_S", true},
	{0, 22, 24, "DPMAIF_AO_APB_S", true},
	{0, 23, 25, "SYS_TIMER_APB_S", true},
	{0, 24, 26, "MODEM_TEMP_SHARE_APB_S", true},
	{0, 25, 27, "PMIF1_APB_S", true},
	{0, 26, 28, "PMICSPI_MST_APB_S", true},
	{0, 27, 29, "TIA_APB_S", true},
	{0, 28, 30, "TOPCKGEN_INFRA_CFG_APB_S", true},
	{0, 29, 31, "DRM_DEBUG_TOP_APB_S", true},

	/* 30 */
	{0, 30, 32, "EFUSE_DEBUG_AO_APB_S", true},
	{0, 31, 33, "APXGPT_APB_S", true},
	{0, 32, 34, "SEJ_APB_S", true},
	{0, 33, 35, "AES_TOP0_APB_S", true},
	{0, 34, 36, "SECURITY_AO_APB_S", true},
	{0, 35, 37, "SPMI_MST_APB_S", true},
	{0, 36, 38, "DEBUG_CTRL_FMEM_AO_APB_S", true},
	{0, 37, 39, "BCRM_FMEM_AO_APB_S", true},
	{0, 38, 40, "DEVICE_APC_FMEM_AO_APB_S", true},
	{0, 39, 41, "PWM_APB_S", true},

	/* 40 */
	{0, 40, 42, "PMIF2_APB_S", true},
	{0, 41, 43, "SPMI_MST2_APB_S", true},
	{0, 42, 44, "PMSR_APB_S", true},
	{0, 43, 45, "SRCLKEN_RC_APB_S", true},
	{0, 44, 46, "MFGSYS", true},
	{0, 45, 47, "MFGSYS", true},
	{0, 46, 48, "MFGSYS", true},
	{0, 47, 49, "MFGSYS", true},
	{0, 48, 50, "MFGSYS", true},
	{0, 49, 51, "MFGSYS", true},

	/* 50 */
	{0, 50, 52, "MFGSYS", true},
	{0, 51, 53, "MFGSYS", true},
	{0, 52, 54, "MFGSYS", true},
	{0, 53, 55, "MFGSYS", true},
	{0, 54, 56, "MFGSYS", true},
	{0, 55, 57, "MFGSYS", true},
	{0, 56, 58, "MFGSYS", true},
	{0, 57, 59, "MFGSYS", true},
	{0, 58, 60, "MFGSYS", true},
	{0, 59, 61, "MFGSYS", true},

	/* 60 */
	{0, 60, 62, "MFGSYS", true},
	{0, 61, 63, "MFGSYS", true},
	{0, 62, 64, "MCUSYS_CFGREG_APB_S", true},
	{0, 63, 65, "MCUSYS_CFGREG_APB_S-1", true},
	{0, 64, 66, "MCUSYS_CFGREG_APB_S-2", true},
	{0, 65, 67, "MCUSYS_CFGREG_APB_S-3", true},
	{0, 66, 68, "MCUSYS_CFGREG_APB_S-4", true},
	{0, 67, 69, "L3C_S", true},
	{0, 68, 70, "L3C_S-1", true},
	{1, 0, 71, "MMSYS_DISP", true},

	/* 70 */
	{1, 1, 72, "MMSYS_DISP", true},
	{1, 2, 73, "MMSYS_DISP", true},
	{1, 3, 74, "SMI", true},
	{1, 4, 75, "SMI", true},
	{1, 5, 76, "MMSYS_DISP", true},
	{1, 6, 77, "MMSYS_DISP", true},
	{1, 7, 78, "MMSYS_DISP", true},
	{1, 8, 79, "MMSYS_DISP", true},
	{1, 9, 80, "MMSYS_DISP", true},
	{1, 10, 81, "MMSYS_DISP", true},

	/* 80 */
	{1, 11, 82, "MMSYS_DISP", true},
	{1, 12, 83, "MMSYS_DISP", true},
	{1, 13, 84, "MMSYS_DISP", true},
	{1, 14, 85, "MMSYS_DISP", true},
	{1, 15, 86, "MMSYS_DISP", true},
	{1, 16, 87, "MMSYS_DISP", true},
	{1, 17, 88, "MMSYS_DISP", true},
	{1, 18, 89, "MMSYS_DISP", true},
	{1, 19, 90, "MMSYS_DISP", true},
	{1, 20, 91, "MMSYS_DISP", true},

	/* 90 */
	{1, 21, 92, "MMSYS_DISP", true},
	{1, 22, 93, "MMSYS_IOMMU", true},
	{1, 23, 94, "MMSYS_IOMMU", true},
	{1, 24, 95, "MMSYS_IOMMU", true},
	{1, 25, 96, "MMSYS_IOMMU", true},
	{1, 26, 97, "MMSYS_IOMMU", true},
	{1, 27, 98, "SMI", true},
	{1, 28, 99, "SMI", true},
	{1, 29, 100, "MMSYS_DISP", true},
	{1, 30, 101, "SMI", true},

	/* 100 */
	{1, 31, 102, "MMSYS_DISP", true},
	{1, 32, 103, "MMSYS_DISP", true},
	{1, 33, 104, "IMGSYS", true},
	{1, 34, 105, "IMGSYS", true},
	{1, 35, 106, "IMGSYS", true},
	{1, 36, 107, "IMGSYS", true},
	{1, 37, 108, "IMGSYS", true},
	{1, 38, 109, "IMGSYS", true},
	{1, 39, 110, "IMGSYS", true},
	{1, 40, 111, "IMGSYS", true},

	/* 110 */
	{1, 41, 112, "IMGSYS", true},
	{1, 42, 113, "IMGSYS", true},
	{1, 43, 114, "IMGSYS", true},
	{1, 44, 115, "IMGSYS", true},
	{1, 45, 116, "IMGSYS", true},
	{1, 46, 117, "IMGSYS", true},
	{1, 47, 118, "SMI", true},
	{1, 48, 119, "IMGSYS", true},
	{1, 49, 120, "IMGSYS", true},
	{1, 50, 121, "IMGSYS", true},

	/* 120 */
	{1, 51, 122, "IMGSYS", true},
	{1, 52, 123, "IMGSYS", true},
	{1, 53, 124, "IMGSYS", true},
	{1, 54, 125, "IMGSYS", true},
	{1, 55, 126, "IMGSYS", true},
	{1, 56, 127, "IMGSYS", true},
	{1, 57, 128, "IMGSYS", true},
	{1, 58, 129, "IMGSYS", true},
	{1, 59, 130, "IMGSYS", true},
	{1, 60, 131, "IMGSYS", true},

	/* 130 */
	{1, 61, 132, "IMGSYS", true},
	{1, 62, 133, "IMGSYS", true},
	{1, 63, 134, "IMGSYS", true},
	{1, 64, 135, "IMGSYS", true},
	{1, 65, 136, "IMGSYS", true},
	{1, 66, 137, "IMGSYS", true},
	{1, 67, 138, "IMGSYS", true},
	{1, 68, 139, "IMGSYS", true},
	{1, 69, 140, "SMI", true},
	{1, 70, 141, "IMGSYS", true},

	/* 140 */
	{1, 71, 142, "IMGSYS", true},
	{1, 72, 143, "IMGSYS", true},
	{1, 73, 144, "IMGSYS", true},
	{1, 74, 145, "IMGSYS", true},
	{1, 75, 146, "IMGSYS", true},
	{1, 76, 147, "IMGSYS", true},
	{1, 77, 148, "VDECSYS", true},
	{1, 78, 149, "VDECSYS", true},
	{1, 79, 150, "VDECSYS", true},
	{1, 80, 151, "VDECSYS", true},

	/* 150 */
	{1, 81, 152, "VDECSYS", true},
	{1, 82, 153, "VDECSYS", true},
	{1, 83, 154, "VDECSYS", true},
	{1, 84, 155, "VDECSYS", true},
	{1, 85, 156, "VENCSYS", true},
	{1, 86, 157, "VENCSYS", true},
	{1, 87, 158, "VENCSYS", true},
	{1, 88, 159, "VENCSYS", true},
	{1, 89, 160, "VENCSYS", true},
	{1, 90, 161, "VENCSYS", true},

	/* 160 */
	{1, 91, 162, "VENCSYS", true},
	{1, 92, 163, "VENCSYS", true},
	{1, 93, 164, "CAMSYS", true},
	{1, 94, 165, "SMI", true},
	{1, 95, 166, "SMI", true},
	{1, 96, 167, "CAMSYS", true},
	{1, 97, 168, "CAMSYS_SENINF", true},
	{1, 98, 169, "CAMSYS_SENINF", true},
	{1, 99, 170, "CAMSYS_SENINF", true},
	{1, 100, 171, "CAMSYS_SENINF", true},

	/* 170 */
	{1, 101, 172, "CAMSYS_SENINF", true},
	{1, 102, 173, "CAMSYS_SENINF", true},
	{1, 103, 174, "SMI", true},
	{1, 104, 175, "SMI", true},
	{1, 105, 176, "CAMSYS", true},
	{1, 106, 177, "SMI", true},
	{1, 107, 178, "SMI", true},
	{1, 108, 179, "CAMSYS", true},
	{1, 109, 180, "CAMSYS", true},
	{1, 110, 181, "CAMSYS", true},

	/* 180 */
	{1, 111, 182, "CAMSYS", true},
	{1, 112, 183, "CAMSYS", true},
	{1, 113, 184, "CAMSYS", true},
	{1, 114, 185, "CAMSYS", true},
	{1, 115, 186, "CAMSYS", true},
	{1, 116, 187, "CAMSYS", true},
	{1, 117, 188, "CAMSYS", true},
	{1, 118, 189, "CAMSYS", true},
	{1, 119, 190, "CAMSYS", true},
	{1, 120, 191, "CAMSYS", true},

	/* 190 */
	{1, 121, 192, "CAMSYS", true},
	{1, 122, 193, "CAMSYS", true},
	{1, 123, 194, "CAMSYS", true},
	{1, 124, 195, "CAMSYS", true},
	{1, 125, 196, "CAMSYS", true},
	{1, 126, 197, "CAMSYS", true},
	{1, 127, 198, "CAMSYS", true},
	{1, 128, 199, "CAMSYS", true},
	{1, 129, 200, "CAMSYS", true},
	{1, 130, 201, "CAMSYS", true},

	/* 200 */
	{1, 131, 202, "CAMSYS", true},
	{1, 132, 203, "CAMSYS", true},
	{1, 133, 204, "CAMSYS", true},
	{1, 134, 205, "CAMSYS", true},
	{1, 135, 206, "CAMSYS", true},
	{1, 136, 207, "CAMSYS", true},
	{1, 137, 208, "CAMSYS", true},
	{1, 138, 209, "CAMSYS", true},
	{1, 139, 210, "CAMSYS", true},
	{1, 140, 211, "CAMSYS", true},

	/* 210 */
	{1, 141, 212, "CAMSYS", true},
	{1, 142, 213, "CAMSYS", true},
	{1, 143, 214, "CAMSYS", true},
	{1, 144, 215, "CAMSYS", true},
	{1, 145, 216, "CAMSYS", true},
	{1, 146, 217, "CAMSYS", true},
	{1, 147, 218, "CAMSYS", true},
	{1, 148, 219, "CAMSYS", true},
	{1, 149, 220, "CAMSYS", true},
	{1, 150, 221, "CAMSYS", true},

	/* 220 */
	{1, 151, 222, "CAMSYS", true},
	{1, 152, 223, "CAMSYS", true},
	{1, 153, 224, "CAMSYS", true},
	{1, 154, 225, "CAMSYS", true},
	{1, 155, 226, "CAMSYS", true},
	{1, 156, 227, "CAMSYS", true},
	{1, 157, 228, "CAMSYS", true},
	{1, 158, 229, "CAMSYS", true},
	{1, 159, 230, "CAMSYS", true},
	{1, 160, 231, "CAMSYS", true},

	/* 230 */
	{1, 161, 232, "CAMSYS", true},
	{1, 162, 233, "CAMSYS", true},
	{1, 163, 234, "CAMSYS", true},
	{1, 164, 235, "CAMSYS", true},
	{1, 165, 236, "CAMSYS", true},
	{1, 166, 237, "CAMSYS", true},
	{1, 167, 238, "CAMSYS", true},
	{1, 168, 239, "CAMSYS", true},
	{1, 169, 240, "CAMSYS", true},
	{1, 170, 241, "CAMSYS", true},

	/* 240 */
	{1, 171, 242, "CAMSYS", true},
	{1, 172, 243, "CAMSYS", true},
	{1, 173, 244, "CAMSYS", true},
	{1, 174, 245, "CAMSYS", true},
	{1, 175, 246, "CAMSYS", true},
	{1, 176, 247, "CAMSYS", true},
	{1, 177, 248, "CAMSYS", true},
	{1, 178, 249, "CAMSYS", true},
	{1, 179, 250, "CAMSYS", true},
	{1, 180, 251, "CAMSYS", true},

	/* 250 */
	{1, 181, 252, "CAMSYS", true},
	{1, 182, 253, "CAMSYS", true},
	{1, 183, 254, "CAMSYS", true},
	{1, 184, 255, "CAMSYS", true},
	{1, 185, 256, "CAMSYS", true},
	{1, 186, 257, "CAMSYS", true},
	{1, 187, 258, "CAMSYS", true},
	{1, 188, 259, "CAMSYS", true},
	{1, 189, 260, "CAMSYS", true},
	{1, 190, 261, "CAMSYS", true},

	/* 260 */
	{1, 191, 262, "CAMSYS", true},
	{1, 192, 263, "CAMSYS", true},
	{1, 193, 264, "CAMSYS", true},
	{1, 194, 265, "CAMSYS", true},
	{1, 195, 266, "CAMSYS", true},
	{1, 196, 267, "CAMSYS", true},
	{1, 197, 268, "CAMSYS", true},
	{1, 198, 269, "CAMSYS", true},
	{1, 199, 270, "CAMSYS", true},
	{1, 200, 271, "CAMSYS", true},

	/* 270 */
	{1, 201, 272, "CAMSYS", true},
	{1, 202, 273, "CAMSYS", true},
	{1, 203, 274, "CAMSYS", true},
	{1, 204, 275, "CAMSYS", true},
	{1, 205, 276, "CAMSYS", true},
	{1, 206, 277, "CAMSYS", true},
	{1, 207, 278, "CAMSYS", true},
	{1, 208, 279, "CAMSYS", true},
	{1, 209, 280, "CAMSYS", true},
	{1, 210, 281, "CAMSYS", true},

	/* 280 */
	{1, 211, 282, "CAMSYS", true},
	{1, 212, 283, "CAMSYS", true},
	{1, 213, 284, "CAMSYS", true},
	{1, 214, 285, "CAMSYS", true},
	{1, 215, 286, "CAMSYS", true},
	{1, 216, 287, "CAMSYS", true},
	{1, 217, 288, "CAMSYS", true},
	{1, 218, 289, "CAMSYS", true},
	{1, 219, 290, "CAMSYS", true},
	{1, 220, 291, "CAMSYS", true},

	/* 290 */
	{1, 221, 292, "CAMSYS", true},
	{1, 222, 293, "CAMSYS", true},
	{1, 223, 294, "CAMSYS", true},
	{1, 224, 295, "CAMSYS_CCU", true},
	{1, 225, 296, "CAMSYS_CCU", true},
	{1, 226, 297, "CAMSYS_CCU", true},
	{1, 227, 298, "CAMSYS_CCU", true},
	{1, 228, 299, "CAMSYS_CCU", true},
	{1, 229, 300, "CAMSYS_CCU", true},
	{1, 230, 301, "CAMSYS_CCU", true},

	/* 300 */
	{1, 231, 302, "CAMSYS_CCU", true},
	{1, 232, 303, "CAMSYS_CCU", true},
	{1, 233, 304, "CAMSYS_CCU", true},
	{1, 234, 305, "CAMSYS_CCU", true},
	{1, 235, 306, "CAMSYS_CCU", true},
	{1, 236, 307, "CAMSYS_CCU", true},
	{1, 237, 308, "CAMSYS_CCU", true},
	{1, 238, 309, "CAMSYS_CCU", true},
	{1, 239, 310, "CAMSYS_CCU", true},
	{1, 240, 311, "CAMSYS_CCU", true},

	/* 310 */
	{1, 241, 312, "CAMSYS_CCU", true},
	{1, 242, 313, "CAMSYS_CCU", true},
	{1, 243, 314, "CAMSYS_CCU", true},
	{1, 244, 315, "CAMSYS_CCU", true},
	{1, 245, 316, "CAMSYS_CCU", true},
	{1, 246, 317, "CAMSYS_CCU", true},
	{1, 247, 318, "CAMSYS_CCU", true},
	{1, 248, 319, "IPESYS", true},
	{1, 249, 320, "IPESYS", true},
	{1, 250, 321, "IPESYS", true},

	/* 320 */
	{1, 251, 322, "IPESYS", true},
	{1, 252, 323, "IPESYS", true},
	{1, 253, 324, "SMI", true},
	{1, 254, 325, "SMI", true},
	{1, 255, 326, "IPESYS", true},
	{2, 0, 327, "IPESYS", true},
	{2, 1, 328, "IPESYS", true},
	{2, 2, 329, "SMI", true},
	{2, 3, 330, "IPESYS", true},
	{2, 4, 331, "MDPSYS", true},

	/* 330 */
	{2, 5, 332, "MDPSYS", true},
	{2, 6, 333, "SMI", true},
	{2, 7, 334, "MDPSYS", true},
	{2, 8, 335, "MDPSYS", true},
	{2, 9, 336, "MDPSYS", true},
	{2, 10, 337, "MDPSYS", true},
	{2, 11, 338, "MDPSYS", true},
	{2, 12, 339, "MDPSYS", true},
	{2, 13, 340, "MDPSYS", true},
	{2, 14, 341, "MDPSYS", true},

	/* 340 */
	{2, 15, 342, "MDPSYS", true},
	{2, 16, 343, "MDPSYS", true},
	{2, 17, 344, "MDPSYS", true},
	{2, 18, 345, "MDPSYS", true},
	{2, 19, 346, "MDPSYS", true},
	{-1, -1, 347, "OOB_way_en", true},
	{-1, -1, 348, "OOB_way_en", true},
	{-1, -1, 349, "OOB_way_en", true},
	{-1, -1, 350, "OOB_way_en", true},
	{-1, -1, 351, "OOB_way_en", true},

	/* 350 */
	{-1, -1, 352, "OOB_way_en", true},
	{-1, -1, 353, "OOB_way_en", true},
	{-1, -1, 354, "OOB_way_en", true},
	{-1, -1, 355, "OOB_way_en", true},
	{-1, -1, 356, "OOB_way_en", true},
	{-1, -1, 357, "OOB_way_en", true},
	{-1, -1, 358, "OOB_way_en", true},
	{-1, -1, 359, "OOB_way_en", true},
	{-1, -1, 360, "OOB_way_en", true},
	{-1, -1, 361, "OOB_way_en", true},

	/* 360 */
	{-1, -1, 362, "OOB_way_en", true},
	{-1, -1, 363, "OOB_way_en", true},
	{-1, -1, 364, "OOB_way_en", true},
	{-1, -1, 365, "OOB_way_en", true},
	{-1, -1, 366, "OOB_way_en", true},
	{-1, -1, 367, "OOB_way_en", true},
	{-1, -1, 368, "OOB_way_en", true},
	{-1, -1, 369, "OOB_way_en", true},
	{-1, -1, 370, "OOB_way_en", true},
	{-1, -1, 371, "OOB_way_en", true},

	/* 370 */
	{-1, -1, 372, "OOB_way_en", true},
	{-1, -1, 373, "OOB_way_en", true},
	{-1, -1, 374, "OOB_way_en", true},
	{-1, -1, 375, "OOB_way_en", true},
	{-1, -1, 376, "OOB_way_en", true},
	{-1, -1, 377, "OOB_way_en", true},
	{-1, -1, 378, "OOB_way_en", true},
	{-1, -1, 379, "OOB_way_en", true},
	{-1, -1, 380, "OOB_way_en", true},
	{-1, -1, 381, "OOB_way_en", true},

	/* 380 */
	{-1, -1, 382, "OOB_way_en", true},
	{-1, -1, 383, "OOB_way_en", true},
	{-1, -1, 384, "OOB_way_en", true},
	{-1, -1, 385, "OOB_way_en", true},
	{-1, -1, 386, "OOB_way_en", true},
	{-1, -1, 387, "OOB_way_en", true},
	{-1, -1, 388, "OOB_way_en", true},
	{-1, -1, 389, "OOB_way_en", true},
	{-1, -1, 390, "OOB_way_en", true},
	{-1, -1, 391, "OOB_way_en", true},

	/* 390 */
	{-1, -1, 392, "OOB_way_en", true},
	{-1, -1, 393, "OOB_way_en", true},
	{-1, -1, 394, "SRAMROM", true},
	{-1, -1, 395, "MDP_MALI", true},
	{-1, -1, 396, "OOB_way_en", true},
	{-1, -1, 397, "MMSYS_MALI", true},

};

static struct mtk_device_info mt6833_devices_peri[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "DEVICE_APC_PERI_AO_APB_S", true},
	{0, 1, 1, "BCRM_PERI_AO_APB_S", true},
	{0, 2, 2, "DEBUG_CTRL_PERI_AO_APB_S", true},
	{0, 3, 26, "PWR_MD32_S", true},
	{0, 4, 27, "PWR_MD32_S-1", true},
	{0, 5, 28, "PWR_MD32_S-2", true},
	{0, 6, 29, "PWR_MD32_S-3", true},
	{0, 7, 30, "PWR_MD32_S-4", true},
	{0, 8, 31, "PWR_MD32_S-5", true},
	{0, 9, 32, "PWR_MD32_S-6", true},

	/* 10 */
	{0, 10, 33, "PWR_MD32_S-7", true},
	{0, 11, 34, "PWR_MD32_S-8", true},
	{0, 12, 35, "PWR_MD32_S-9", true},
	{0, 13, 36, "PWR_MD32_S-10", true},
	{0, 14, 37, "PWR_MD32_S-11", true},
	{0, 15, 38, "PWR_MD32_S-12", true},
	{0, 16, 39, "PWR_MD32_S-13", true},
	{0, 17, 40, "PWR_MD32_S-14", true},
	{0, 18, 84, "AUDIO_S", true},
	{0, 19, 85, "AUDIO_S-1", true},

	/* 20 */
	{0, 20, 86, "UFS_S", true},
	{0, 21, 87, "UFS_S-1", true},
	{0, 22, 88, "UFS_S-2", true},
	{0, 23, 89, "UFS_S-3", true},
	{0, 24, 91, "SSUSB_S", true},
	{0, 25, 92, "SSUSB_S-1", true},
	{0, 26, 93, "SSUSB_S-2", true},
	{0, 27, 94, "DEBUGSYS_APB_S", true},
	{0, 28, 95, "DRAMC_MD32_S0_APB_S", true},
	{0, 29, 96, "DRAMC_MD32_S0_APB_S-1", true},

	/* 30 */
	{0, 30, 97, "DRAMC_CH0_TOP0_APB_S", true},
	{0, 31, 98, "DRAMC_CH0_TOP1_APB_S", true},
	{0, 32, 99, "DRAMC_CH0_TOP2_APB_S", true},
	{0, 33, 100, "DRAMC_CH0_TOP3_APB_S", true},
	{0, 34, 101, "DRAMC_CH0_TOP4_APB_S", true},
	{0, 35, 102, "DRAMC_CH0_TOP5_APB_S", true},
	{0, 36, 103, "DRAMC_CH0_TOP6_APB_S", true},
	{0, 37, 104, "DRAMC_CH1_TOP0_APB_S", true},
	{0, 38, 105, "DRAMC_CH1_TOP1_APB_S", true},
	{0, 39, 106, "DRAMC_CH1_TOP2_APB_S", true},

	/* 40 */
	{0, 40, 107, "DRAMC_CH1_TOP3_APB_S", true},
	{0, 41, 108, "DRAMC_CH1_TOP4_APB_S", true},
	{0, 42, 109, "DRAMC_CH1_TOP5_APB_S", true},
	{0, 43, 110, "DRAMC_CH1_TOP6_APB_S", true},
	{0, 44, 112, "CCIF2_AP_APB_S", true},
	{0, 45, 113, "CCIF2_MD_APB_S", true},
	{0, 46, 114, "CCIF3_AP_APB_S", true},
	{0, 47, 115, "CCIF3_MD_APB_S", true},
	{0, 48, 116, "CCIF4_AP_APB_S", true},
	{0, 49, 117, "CCIF4_MD_APB_S", true},

	/* 50 */
	{0, 50, 118, "CCIF5_AP_APB_S", true},
	{0, 51, 119, "CCIF5_MD_APB_S", true},
	{0, 52, 120, "SSC_INFRA_APB2_S", true},
	{1, 0, 3, "TINYSYS", true},
	{1, 1, 4, "TINYSYS", true},
	{1, 2, 5, "TINYSYS", true},
	{1, 3, 6, "TINYSYS", true},
	{1, 4, 7, "TINYSYS", true},
	{1, 5, 8, "TINYSYS", true},
	{1, 6, 9, "TINYSYS", true},

	/* 60 */
	{1, 7, 10, "TINYSYS", true},
	{1, 8, 11, "TINYSYS", true},
	{1, 9, 12, "TINYSYS", true},
	{1, 10, 13, "TINYSYS", true},
	{1, 11, 14, "TINYSYS", true},
	{1, 12, 15, "TINYSYS", true},
	{1, 13, 16, "TINYSYS", true},
	{1, 14, 17, "TINYSYS", true},
	{1, 15, 18, "TINYSYS", true},
	{1, 16, 19, "TINYSYS", true},

	/* 70 */
	{1, 17, 20, "TINYSYS", true},
	{1, 18, 21, "TINYSYS", true},
	{1, 19, 22, "TINYSYS", true},
	{1, 20, 23, "TINYSYS", true},
	{1, 21, 24, "TINYSYS", true},
	{1, 22, 25, "TINYSYS", true},
	{1, 23, 41, "MDSYS", true},
	{1, 24, 42, "MDSYS", true},
	{1, 25, 43, "MDSYS", true},
	{1, 26, 44, "MDSYS", true},

	/* 80 */
	{1, 27, 45, "MDSYS", true},
	{1, 28, 46, "MDSYS", true},
	{1, 29, 47, "MDSYS", true},
	{1, 30, 48, "MDSYS", true},
	{1, 31, 49, "MDSYS", true},
	{1, 32, 50, "MDSYS", true},
	{1, 33, 51, "MDSYS", true},
	{1, 34, 52, "MDSYS", true},
	{1, 35, 53, "MDSYS", true},
	{1, 36, 54, "MDSYS", true},

	/* 90 */
	{1, 37, 55, "MDSYS", true},
	{1, 38, 56, "MDSYS", true},
	{1, 39, 57, "MDSYS", true},
	{1, 40, 58, "MDSYS", true},
	{1, 41, 59, "MDSYS", true},
	{1, 42, 60, "MDSYS", true},
	{1, 43, 61, "MDSYS", true},
	{1, 44, 62, "MDSYS", true},
	{1, 45, 63, "MDSYS", true},
	{1, 46, 64, "MDSYS", true},

	/* 100 */
	{1, 47, 65, "MDSYS", true},
	{1, 48, 66, "MDSYS", true},
	{1, 49, 67, "MDSYS", true},
	{1, 50, 68, "MDSYS", true},
	{1, 51, 69, "MDSYS", true},
	{1, 52, 70, "MDSYS", true},
	{1, 53, 71, "MDSYS", true},
	{1, 54, 72, "MDSYS", true},
	{1, 55, 73, "MDSYS", true},
	{1, 56, 74, "MDSYS", true},

	/* 110 */
	{1, 57, 75, "MDSYS", true},
	{1, 58, 76, "MDSYS", true},
	{1, 59, 77, "MDSYS", true},
	{1, 60, 78, "MDSYS", true},
	{1, 61, 79, "MDSYS", true},
	{1, 62, 80, "MDSYS", true},
	{1, 63, 81, "MDSYS", true},
	{1, 64, 82, "MDSYS", true},
	{1, 65, 83, "MDSYS", true},
	{2, 0, 90, "CONNSYS", true},

	/* 120 */
	{-1, -1, 121, "OOB_way_en", true},
	{-1, -1, 122, "OOB_way_en", true},
	{-1, -1, 123, "OOB_way_en", true},
	{-1, -1, 124, "OOB_way_en", true},
	{-1, -1, 125, "OOB_way_en", true},
	{-1, -1, 126, "OOB_way_en", true},
	{-1, -1, 127, "OOB_way_en", true},
	{-1, -1, 128, "OOB_way_en", true},
	{-1, -1, 129, "OOB_way_en", true},
	{-1, -1, 130, "OOB_way_en", true},

	/* 130 */
	{-1, -1, 131, "OOB_way_en", true},
	{-1, -1, 132, "OOB_way_en", true},
	{-1, -1, 133, "OOB_way_en", true},
	{-1, -1, 134, "OOB_way_en", true},
	{-1, -1, 135, "OOB_way_en", true},
	{-1, -1, 136, "OOB_way_en", true},
	{-1, -1, 137, "OOB_way_en", true},
	{-1, -1, 138, "OOB_way_en", true},
	{-1, -1, 139, "OOB_way_en", true},
	{-1, -1, 140, "OOB_way_en", true},

	/* 140 */
	{-1, -1, 141, "OOB_way_en", true},
	{-1, -1, 142, "OOB_way_en", true},
	{-1, -1, 143, "OOB_way_en", true},
	{-1, -1, 144, "OOB_way_en", true},
	{-1, -1, 145, "OOB_way_en", true},
	{-1, -1, 146, "OOB_way_en", true},
	{-1, -1, 147, "OOB_way_en", true},
	{-1, -1, 148, "OOB_way_en", true},
	{-1, -1, 149, "OOB_way_en", true},
	{-1, -1, 150, "OOB_way_en", true},

	/* 150 */
	{-1, -1, 151, "OOB_way_en", true},
	{-1, -1, 152, "OOB_way_en", true},
	{-1, -1, 153, "OOB_way_en", true},
	{-1, -1, 154, "OOB_way_en", true},
	{-1, -1, 155, "OOB_way_en", true},
	{-1, -1, 156, "OOB_way_en", true},
	{-1, -1, 157, "OOB_way_en", true},
	{-1, -1, 158, "OOB_way_en", true},
	{-1, -1, 159, "OOB_way_en", true},
	{-1, -1, 160, "OOB_way_en", true},

	/* 160 */
	{-1, -1, 161, "OOB_way_en", true},
	{-1, -1, 162, "OOB_way_en", true},
	{-1, -1, 163, "OOB_way_en", true},

};

static struct mtk_device_info mt6833_devices_peri2[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "DEVICE_APC_PERI_AO2_APB_S", true},
	{0, 1, 1, "BCRM_PERI_AO2_APB_S", true},
	{0, 2, 2, "DEBUG_CTRL_PERI_AO2_APB_S", true},
	{0, 3, 3, "GCE_APB_S", true},
	{0, 4, 4, "GCE_APB_S-1", true},
	{0, 5, 5, "GCE_APB_S-2", true},
	{0, 6, 6, "GCE_APB_S-3", true},
	{0, 7, 7, "DPMAIF_PDN_APB_S", true},
	{0, 8, 8, "DPMAIF_PDN_APB_S-1", true},
	{0, 9, 9, "DPMAIF_PDN_APB_S-2", true},

	/* 10 */
	{0, 10, 10, "DPMAIF_PDN_APB_S-3", true},
	{0, 11, 11, "BND_EAST_APB0_S", true},
	{0, 12, 12, "BND_EAST_APB1_S", true},
	{0, 13, 13, "BND_EAST_APB2_S", true},
	{0, 14, 14, "BND_EAST_APB3_S", true},
	{0, 15, 15, "BND_EAST_APB4_S", true},
	{0, 16, 16, "BND_EAST_APB5_S", true},
	{0, 17, 17, "BND_EAST_APB6_S", true},
	{0, 18, 18, "BND_EAST_APB7_S", true},
	{0, 19, 19, "BND_EAST_APB8_S", true},

	/* 20 */
	{0, 20, 20, "BND_EAST_APB9_S", true},
	{0, 21, 21, "BND_EAST_APB10_S", true},
	{0, 22, 22, "BND_EAST_APB11_S", true},
	{0, 23, 23, "BND_EAST_APB12_S", true},
	{0, 24, 24, "BND_EAST_APB13_S", true},
	{0, 25, 25, "BND_EAST_APB14_S", true},
	{0, 26, 26, "BND_EAST_APB15_S", true},
	{0, 27, 27, "BND_WEST_APB0_S", true},
	{0, 28, 28, "BND_WEST_APB1_S", true},
	{0, 29, 29, "BND_WEST_APB2_S", true},

	/* 30 */
	{0, 30, 30, "BND_WEST_APB3_S", true},
	{0, 31, 31, "BND_WEST_APB4_S", true},
	{0, 32, 32, "BND_WEST_APB5_S", true},
	{0, 33, 33, "BND_WEST_APB6_S", true},
	{0, 34, 34, "BND_WEST_APB7_S", true},
	{0, 35, 35, "BND_NORTH_APB0_S", true},
	{0, 36, 36, "BND_NORTH_APB1_S", true},
	{0, 37, 37, "BND_NORTH_APB2_S", true},
	{0, 38, 38, "BND_NORTH_APB3_S", true},
	{0, 39, 39, "BND_NORTH_APB4_S", true},

	/* 40 */
	{0, 40, 40, "BND_NORTH_APB5_S", true},
	{0, 41, 41, "BND_NORTH_APB6_S", true},
	{0, 42, 42, "BND_NORTH_APB7_S", true},
	{0, 43, 43, "BND_NORTH_APB8_S", true},
	{0, 44, 44, "BND_NORTH_APB9_S", true},
	{0, 45, 45, "BND_NORTH_APB10_S", true},
	{0, 46, 46, "BND_NORTH_APB11_S", true},
	{0, 47, 47, "BND_NORTH_APB12_S", true},
	{0, 48, 48, "BND_NORTH_APB13_S", true},
	{0, 49, 49, "BND_NORTH_APB14_S", true},

	/* 50 */
	{0, 50, 50, "BND_NORTH_APB15_S", true},
	{0, 51, 51, "BND_SOUTH_APB0_S", true},
	{0, 52, 52, "BND_SOUTH_APB1_S", true},
	{0, 53, 53, "BND_SOUTH_APB2_S", true},
	{0, 54, 54, "BND_SOUTH_APB3_S", true},
	{0, 55, 55, "BND_SOUTH_APB4_S", true},
	{0, 56, 56, "BND_SOUTH_APB5_S", true},
	{0, 57, 57, "BND_SOUTH_APB6_S", true},
	{0, 58, 58, "BND_SOUTH_APB7_S", true},
	{0, 59, 59, "BND_SOUTH_APB8_S", true},

	/* 60 */
	{0, 60, 60, "BND_SOUTH_APB9_S", true},
	{0, 61, 61, "BND_SOUTH_APB10_S", true},
	{0, 62, 62, "BND_SOUTH_APB11_S", true},
	{0, 63, 63, "BND_SOUTH_APB12_S", true},
	{0, 64, 64, "BND_SOUTH_APB13_S", true},
	{0, 65, 65, "BND_SOUTH_APB14_S", true},
	{0, 66, 66, "BND_SOUTH_APB15_S", true},
	{0, 67, 67, "BND_EAST_NORTH_APB0_S", true},
	{0, 68, 68, "BND_EAST_NORTH_APB1_S", true},
	{0, 69, 69, "BND_EAST_NORTH_APB2_S", true},

	/* 70 */
	{0, 70, 70, "BND_EAST_NORTH_APB3_S", true},
	{0, 71, 71, "BND_EAST_NORTH_APB4_S", true},
	{0, 72, 72, "BND_EAST_NORTH_APB5_S", true},
	{0, 73, 73, "BND_EAST_NORTH_APB6_S", true},
	{0, 74, 74, "BND_EAST_NORTH_APB7_S", true},
	{0, 75, 75, "SYS_CIRQ_APB_S", true},
	{0, 76, 76, "DEVICE_APC_INFRA_PDN_APB_S", true},
	{0, 77, 77, "DEBUG_TRACKER_APB_S", true},
	{0, 78, 78, "CCIF0_AP_APB_S", true},
	{0, 79, 79, "CCIF0_MD_APB_S", true},

	/* 80 */
	{0, 80, 80, "CCIF1_AP_APB_S", true},
	{0, 81, 81, "CCIF1_MD_APB_S", true},
	{0, 82, 82, "MBIST_PDN_APB_S", true},
	{0, 83, 83, "INFRACFG_PDN_APB_S", true},
	{0, 84, 84, "TRNG_APB_S", true},
	{0, 85, 85, "DX_CC_APB_S", true},
	{0, 86, 86, "CQ_DMA_APB_S", true},
	{0, 87, 87, "SRAMROM_APB_S", true},
	{0, 88, 88, "INFRACFG_MEM_APB_S", true},
	{0, 89, 89, "RESERVED_DVFS_PROC_APB_S", true},

	/* 90 */
	{0, 90, 92, "SYS_CIRQ2_APB_S", true},
	{0, 91, 93, "DEBUG_TRACKER_APB1_S", true},
	{0, 92, 94, "EMI_APB_S", true},
	{0, 93, 95, "EMI_MPU_APB_S", true},
	{0, 94, 96, "DEVICE_MPU_PDN_APB_S", true},
	{0, 95, 97, "APDMA_APB_S", true},
	{0, 96, 98, "DEBUG_TRACKER_APB2_S", true},
	{0, 97, 99, "BCRM_INFRA_PDN_APB_S", true},
	{0, 98, 100, "BCRM_PERI_PDN_APB_S", true},
	{0, 99, 101, "BCRM_PERI_PDN2_APB_S", true},

	/* 100 */
	{0, 100, 102, "DEVICE_APC_PERI_PDN_APB_S", true},
	{0, 101, 103, "DEVICE_APC_PERI_PDN2_APB_S", true},
	{0, 102, 104, "BCRM_FMEM_PDN_APB_S", true},
	{0, 103, 105, "HWCCF_APB_S", true},
	{-1, -1, 106, "OOB_way_en", true},
	{-1, -1, 107, "OOB_way_en", true},
	{-1, -1, 108, "OOB_way_en", true},
	{-1, -1, 109, "OOB_way_en", true},
	{-1, -1, 110, "OOB_way_en", true},
	{-1, -1, 111, "OOB_way_en", true},

	/* 110 */
	{-1, -1, 112, "OOB_way_en", true},
	{-1, -1, 113, "OOB_way_en", true},
	{-1, -1, 114, "OOB_way_en", true},
	{-1, -1, 115, "OOB_way_en", true},
	{-1, -1, 116, "OOB_way_en", true},
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

};

static struct mtk_device_info mt6833_devices_peri_par[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "MSDC0_S", true},
	{0, 1, 1, "MSDC1_S", true},
	{0, 2, 2, "AUXADC_APB_S", true},
	{0, 3, 3, "UART0_APB_S", true},
	{0, 4, 4, "UART1_APB_S", true},
	{0, 5, 5, "UART2_APB_S", true},
	{0, 6, 6, "IIC_P2P_REMAP_APB4_S", true},
	{0, 7, 7, "SPI0_APB_S", true},
	{0, 8, 8, "PTP_THERM_CTRL_APB_S", true},
	{0, 9, 9, "BTIF_APB_S", true},

	/* 10 */
	{0, 10, 10, "DISP_PWM_APB_S", true},
	{0, 11, 11, "SPI1_APB_S", true},
	{0, 12, 12, "SPI2_APB_S", true},
	{0, 13, 13, "SPI3_APB_S", true},
	{0, 14, 14, "IIC_P2P_REMAP_APB0_S", true},
	{0, 15, 15, "IIC_P2P_REMAP_APB1_S", true},
	{0, 16, 16, "SPI4_APB_S", true},
	{0, 17, 17, "SPI5_APB_S", true},
	{0, 18, 18, "IIC_P2P_REMAP_APB2_S", true},
	{0, 19, 19, "IIC_P2P_REMAP_APB3_S", true},

	/* 20 */
	{0, 20, 20, "SPI6_APB_S", true},
	{0, 21, 21, "SPI7_APB_S", true},
	{0, 22, 22, "BCRM_PERI_PAR_PDN_APB_S", true},
	{0, 23, 23, "DEVICE_APC_PERI_PAR_PDN_APB_S", true},
	{0, 24, 24, "PTP_THERM_CTRL2_APB_S", true},
	{0, 25, 25, "DEVICE_APC_PERI_PAR_AO_APB_S", true},
	{0, 26, 26, "DEBUG_CTRL_PERI_PAR_AO_APB_S", true},
	{0, 27, 27, "BCRM_PERI_PAR_AO_APB_S", true},
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
	{-1, -1, 56, "OOB_way_en", true},
	{-1, -1, 57, "OOB_way_en", true},
	{-1, -1, 58, "OOB_way_en", true},
	{-1, -1, 59, "OOB_way_en", true},

	/* 60 */
	{-1, -1, 60, "OOB_way_en", true},

};

static struct mtk_device_num mtk6853_devices_num[] = {
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
	{"SPM",          { 1, 1, 0 } },
};

static struct INFRAAXI_ID_INFO infra_mi_id_to_master[] = {
	{"CONNSYS_WFDMA",     { 0, 0, 0, 0,	0, 1, 2, 2,	2, 2, 2, 2,	2, 0 } },
	{"CONNSYS_ICAP",      { 0, 0, 0, 0,	0, 0, 2, 2,	2, 2, 2, 2,	2, 0 } },
	{"CONNSYS_WF_MCU",    { 0, 0, 0, 0,	1, 1, 2, 2,	2, 2, 2, 2,	2, 0 } },
	{"CONNSYS_BT_MCU",    { 0, 0, 0, 0,	1, 0, 0, 1,	2, 2, 2, 2,	2, 0 } },
	{"CONNSYS_GPS",       { 0, 0, 0, 0,	1, 0, 0, 0,	2, 2, 2, 2,	2, 0 } },
	{"Tinysys",           { 0, 1, 0, 0,	2, 2, 2, 2,	2, 2, 0, 0,	0, 0 } },
	{"CQ_DMA",            { 0, 0, 1, 0,	0, 0, 0, 2,	2, 2, 0, 0,	0, 0 } },
	{"DebugTop",          { 0, 0, 1, 0,	1, 0, 0, 2,	0, 0, 0, 0,	0, 0 } },
	{"SSUSB",             { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 0, 0,	2, 2 } },
	{"PWM",               { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 1, 0,	0, 0 } },
	{"MSDC1",             { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 1, 0,	1, 0 } },
	{"SPI6",              { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 1, 0,	0, 1 } },
	{"SPI0",              { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 1, 0,	1, 1 } },
	{"MSDC0",             { 0, 0, 1, 0,	0, 1, 0, 1,	0, 0, 2, 2,	0, 0 } },
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
	{"HWCCF",             { 0, 0, 1, 0,	0, 1, 0, 0,	1, 1, 2, 2,	0, 0 } },
	{"DX_CC",             { 0, 0, 1, 0,	1, 1, 0, 2,	2, 2, 2, 0,	0, 0 } },
	{"GCE",               { 0, 0, 1, 0,	0, 0, 1, 2,	2, 0, 0, 0,	0, 0 } },
	{"CPUEB",             { 0, 0, 1, 0,	1, 0, 1, 2,	2, 2, 2, 2,	2, 0 } },
	{"DPMAIF",            { 0, 1, 1, 0,	2, 2, 2, 2,	0, 0, 0, 0,	0, 0 } },
	{"SSPM",              { 0, 0, 0, 1,	2, 2, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"UFS",               { 0, 1, 0, 1,	2, 2, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"CPUEB",             { 0, 0, 1, 1,	2, 2, 2, 2,	2, 2, 0, 0,	0, 0 } },
	{"APMCU_Write",       { 1, 2, 2, 2,	2, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_Write",       { 1, 2, 2, 2,	2, 0, 0, 1,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_Write",       { 1, 2, 2, 2,	2, 2, 2, 2,	2, 1, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 1, 2, 2, 2,	2, 0, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 1, 2, 2, 2,	2, 0, 0, 1,	0, 0, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 1, 2, 0, 0,	0, 0, 0, 0,	1, 0, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 1, 2, 1, 0,	0, 0, 0, 0,	1, 0, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 1, 2, 2, 2,	2, 2, 2, 2,	2, 1, 0, 0,	0, 0 } },
	{"APMCU_Read",        { 1, 2, 2, 2,	2, 2, 2, 2,	2, 2, 1, 0,	0, 0 } },
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

static const char *mt6833_bus_id_to_master(uint32_t bus_id, uint32_t vio_addr,
		int slave_type, int shift_sta_bit, int domain)
{
	const char *err_master = "UNKNOWN_MASTER";
	uint8_t h_1byte;

	pr_debug(PFX "[DEVAPC] %s:0x%x, %s:0x%x, %s:0x%x, %s:%d\n",
		"bus_id", bus_id, "vio_addr", vio_addr,
		"slave_type", slave_type,
		"shift_sta_bit", shift_sta_bit);

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
		if (vio_addr <= 0x1FFFFF || shift_sta_bit == 6) {
			pr_info(PFX "vio_addr might from SRAMROM\n");
			if ((bus_id & 0x1) == 0x0)
				return "EMI_L2C_M";

			return infra_mi_trans(bus_id >> 1);

		} else if (shift_sta_bit >= 0 && shift_sta_bit <= 3) {
			return peri_mi_trans(bus_id);

		} else if (shift_sta_bit >= 4 && shift_sta_bit <= 5) {
			return infra_mi_trans(bus_id);

		} else if (shift_sta_bit == 7) {
			pr_info(PFX "vio_addr is from MMSYS_MALI\n");
			if ((bus_id & 0x1) == 0x1)
				return "GCE_M";

			return infra_mi_trans(bus_id >> 1);
		}

		return err_master;

	} else if (slave_type == SLAVE_TYPE_PERI) {
		if (shift_sta_bit == 1 || shift_sta_bit == 2 ||
				shift_sta_bit == 7) {
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

	return err_master;
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
			if (vio_index == mt6833_devices_infra[i].vio_index)
				return mt6833_devices_infra[i].device;
		}

	} else if (slave_type == SLAVE_TYPE_PERI &&
			vio_index < VIO_SLAVE_NUM_PERI)
		for (i = 0; i < VIO_SLAVE_NUM_PERI; i++) {
			if (vio_index == mt6833_devices_peri[i].vio_index)
				return mt6833_devices_peri[i].device;
		}

	else if (slave_type == SLAVE_TYPE_PERI2 &&
			vio_index < VIO_SLAVE_NUM_PERI2)
		for (i = 0; i < VIO_SLAVE_NUM_PERI2; i++) {
			if (vio_index == mt6833_devices_peri2[i].vio_index)
				return mt6833_devices_peri2[i].device;
		}

	else if (slave_type == SLAVE_TYPE_PERI_PAR &&
			vio_index < VIO_SLAVE_NUM_PERI_PAR)
		for (i = 0; i < VIO_SLAVE_NUM_PERI_PAR; i++) {
			if (vio_index == mt6833_devices_peri_par[i].vio_index)
				return mt6833_devices_peri_par[i].device;
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

static uint32_t mt6833_shift_group_get(int slave_type, uint32_t vio_idx)
{
	if (slave_type == SLAVE_TYPE_INFRA) {
		if (vio_idx >= 0 && vio_idx <= 3)
			return 0;
		else if (vio_idx >= 4 && vio_idx <= 5)
			return 1;
		else if ((vio_idx >= 6 && vio_idx <= 31) ||
			 (vio_idx >= 347 && vio_idx <= 373) ||
			 vio_idx == 392)
			return 2;
		else if ((vio_idx >= 32 && vio_idx <= 45) ||
			 (vio_idx >= 374 && vio_idx <= 388) ||
			 vio_idx == 393)
			return 3;
		else if ((vio_idx >= 46 && vio_idx <= 63) || vio_idx == 389)
			return 4;
		else if ((vio_idx >= 64 && vio_idx <= 68) || vio_idx == 390)
			return 5;
		else if ((vio_idx >= 69 && vio_idx <= 70) || vio_idx == 391)
			return 6;
		else if (vio_idx >= 71 && vio_idx <= 346)
			return 7;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI) {
		if ((vio_idx >= 0 && vio_idx <= 2) ||
		    (vio_idx >= 121 && vio_idx <= 124) ||
		    vio_idx == 160)
			return 0;
		else if ((vio_idx >= 3 && vio_idx <= 25) || vio_idx == 125)
			return 1;
		else if ((vio_idx >= 26 && vio_idx <= 40) || vio_idx == 126)
			return 2;
		else if ((vio_idx >= 41 && vio_idx <= 83) || vio_idx == 127)
			return 3;
		else if ((vio_idx >= 84 && vio_idx <= 85) || vio_idx == 128)
			return 4;
		else if ((vio_idx >= 86 && vio_idx <= 89) || vio_idx == 129)
			return 5;
		else if (vio_idx == 90 || vio_idx == 130)
			return 6;
		else if ((vio_idx >= 91 && vio_idx <= 93) || vio_idx == 131)
			return 7;
		else if (vio_idx == 94 ||
			 (vio_idx >= 132 && vio_idx <= 133) ||
			 vio_idx == 161)
			return 8;
		else if (vio_idx >= 95 && vio_idx <= 96)
			return 9;
		else if ((vio_idx >= 97 && vio_idx <= 111) ||
			 (vio_idx >= 134 && vio_idx <= 149) ||
			 vio_idx == 162)
			return 10;
		else if ((vio_idx >= 112 && vio_idx <= 120) ||
			 (vio_idx >= 150 && vio_idx <= 159) ||
			 vio_idx == 163)
			return 11;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI2) {
		if ((vio_idx >= 0 && vio_idx <= 2) ||
		    (vio_idx >= 106 && vio_idx <= 109) ||
		    vio_idx == 212)
			return 0;
		else if (vio_idx >= 3 && vio_idx <= 6)
			return 1;
		else if (vio_idx >= 7 && vio_idx <= 10)
			return 2;
		else if ((vio_idx >= 11 && vio_idx <= 26) ||
			 (vio_idx >= 110 && vio_idx <= 126) ||
			 vio_idx == 213)
			return 3;
		else if ((vio_idx >= 27 && vio_idx <= 34) ||
			 (vio_idx >= 127 && vio_idx <= 135) ||
			 vio_idx == 214)
			return 4;
		else if ((vio_idx >= 35 && vio_idx <= 50) ||
			 (vio_idx >= 136 && vio_idx <= 152) ||
			 vio_idx == 215)
			return 5;
		else if ((vio_idx >= 51 && vio_idx <= 66) ||
			 (vio_idx >= 153 && vio_idx <= 169) ||
			 vio_idx == 216)
			return 6;
		else if ((vio_idx >= 67 && vio_idx <= 74) ||
			 (vio_idx >= 170 && vio_idx <= 178) ||
			 vio_idx == 217)
			return 7;
		else if ((vio_idx >= 75 && vio_idx <= 93) ||
			 (vio_idx >= 179 && vio_idx <= 198) ||
			 vio_idx == 218)
			return 8;
		else if ((vio_idx >= 94 && vio_idx <= 105) ||
			 (vio_idx >= 199 && vio_idx <= 211) ||
			 vio_idx == 219)
			return 9;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI_PAR) {
		if ((vio_idx >= 0 && vio_idx <= 1) ||
		    (vio_idx >= 28 && vio_idx <= 29) ||
		    vio_idx == 58)
			return 0;
		else if ((vio_idx >= 2 && vio_idx <= 24) ||
			 (vio_idx >= 30 && vio_idx <= 53) ||
			 vio_idx == 59)
			return 1;
		else if ((vio_idx >= 25 && vio_idx <= 27) ||
			 (vio_idx >= 54 && vio_idx <= 57) ||
			 vio_idx == 60)
			return 2;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	}

	pr_err(PFX "%s:%d Wrong slave_type:0x%x\n",
			__func__, __LINE__, slave_type);

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

static struct mtk_devapc_dbg_status mt6833_devapc_dbg_stat = {
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

static struct mtk_devapc_vio_info mt6833_devapc_vio_info = {
	.vio_mask_sta_num = mtk_vio_mask_sta_num,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
	.mdp_vio_idx = MDP_VIO_INDEX,
	.disp2_vio_idx = MDP_VIO_INDEX,
	.mmsys_vio_idx = MMSYS_VIO_INDEX,
	.sramrom_slv_type = SRAMROM_SLAVE_TYPE,
	.mm2nd_slv_type = MM2ND_SLAVE_TYPE,
};

static const struct mtk_infra_vio_dbg_desc mt6833_vio_dbgs = {
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

static const struct mtk_sramrom_sec_vio_desc mt6833_sramrom_sec_vios = {
	.vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const uint32_t mt6833_devapc_pds[] = {
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

static struct mtk_devapc_soc mt6833_data = {
	.dbg_stat = &mt6833_devapc_dbg_stat,
	.slave_type_arr = slave_type_to_str,
	.slave_type_num = SLAVE_TYPE_NUM,
	.device_info[SLAVE_TYPE_INFRA] = mt6833_devices_infra,
	.device_info[SLAVE_TYPE_PERI] = mt6833_devices_peri,
	.device_info[SLAVE_TYPE_PERI2] = mt6833_devices_peri2,
	.device_info[SLAVE_TYPE_PERI_PAR] = mt6833_devices_peri_par,
	.ndevices = mtk6853_devices_num,
	.vio_info = &mt6833_devapc_vio_info,
	.vio_dbgs = &mt6833_vio_dbgs,
	.sramrom_sec_vios = &mt6833_sramrom_sec_vios,
	.devapc_pds = mt6833_devapc_pds,
	.subsys_get = &index_to_subsys,
	.master_get = &mt6833_bus_id_to_master,
	.mm2nd_vio_handler = &mm2nd_vio_handler,
	.shift_group_get = mt6833_shift_group_get,
};

static const struct of_device_id mt6833_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6833-devapc" },
	{},
};

static int mt6833_devapc_probe(struct platform_device *pdev)
{
	return mtk_devapc_probe(pdev, &mt6833_data);
}

static int mt6833_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6833_devapc_driver = {
	.probe = mt6833_devapc_probe,
	.remove = mt6833_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = mt6833_devapc_dt_match,
	},
};

module_platform_driver(mt6833_devapc_driver);

MODULE_DESCRIPTION("Mediatek MT6853 Device APC Driver");
MODULE_AUTHOR("Neal Liu <neal.liu@mediatek.com>");
MODULE_LICENSE("GPL");
