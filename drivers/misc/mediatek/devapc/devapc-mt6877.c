// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <asm-generic/io.h>

#include "devapc-mt6877.h"
#include "devapc-mtk-multi-ao.h"

static struct mtk_device_info mt6877_devices_infra[] = {
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
	{0, 44, 46, "MFG_S_S", true},
	{0, 45, 47, "MFG_S_S-1", true},
	{0, 46, 48, "MFG_S_S-2", true},
	{0, 47, 49, "MFG_S_S-3", true},
	{0, 48, 50, "MFG_S_S-4", true},
	{0, 49, 51, "MFG_S_S-5", true},

	/* 50 */
	{0, 50, 52, "MFG_S_S-6", true},
	{0, 51, 53, "MFG_S_S-7", true},
	{0, 52, 54, "MFG_S_S-8", true},
	{0, 53, 55, "MFG_S_S-9", true},
	{0, 54, 56, "MFG_S_S-10", true},
	{0, 55, 57, "MFG_S_S-11", true},
	{0, 56, 58, "MFG_S_S-12", true},
	{0, 57, 59, "MFG_S_S-13", true},
	{0, 58, 60, "MFG_S_S-14", true},
	{0, 59, 61, "MFG_S_S-15", true},

	/* 60 */
	{0, 60, 62, "MFG_S_S-16", true},
	{0, 61, 63, "MFG_S_S-17", true},
	{0, 62, 64, "MFG_S_S-18", true},
	{0, 63, 65, "MFG_S_S-19", true},
	{0, 64, 66, "MFG_S_S-20", true},
	{0, 65, 67, "MFG_S_S-21", true},
	{0, 66, 68, "MFG_S_S-22", true},
	{0, 67, 69, "MFG_S_S-23", true},
	{0, 68, 70, "MFG_S_S-24", true},
	{0, 69, 71, "MFG_S_S-25", true},

	/* 70 */
	{0, 70, 72, "MFG_S_S-26", true},
	{0, 71, 73, "MFG_S_S-27", true},
	{0, 72, 74, "MFG_S_S-28", true},
	{0, 73, 75, "APU_S_S", true},
	{0, 74, 76, "APU_S_S-1", true},
	{0, 75, 77, "APU_S_S-2", true},
	{0, 76, 78, "APU_S_S-3", true},
	{0, 77, 79, "APU_S_S-4", true},
	{0, 78, 80, "APU_S_S-5", true},
	{0, 79, 81, "APU_S_S-6", true},

	/* 80 */
	{0, 80, 82, "APU_S_S-7", true},
	{0, 81, 83, "MCUSYS_CFGREG_APB_S", true},
	{0, 82, 84, "MCUSYS_CFGREG_APB_S-1", true},
	{0, 83, 85, "MCUSYS_CFGREG_APB_S-2", true},
	{0, 84, 86, "MCUSYS_CFGREG_APB_S-3", true},
	{0, 85, 87, "MCUSYS_CFGREG_APB_S-4", true},
	{0, 86, 88, "L3C_S", true},
	{0, 87, 89, "L3C_S-1", true},
	{0, 88, 90, "L3C_S-2", true},
	{1, 0, 91, "MM_S_S", true},

	/* 90 */
	{1, 1, 92, "MM_S_S-1", true},
	{1, 2, 93, "MM_S_S-2", true},
	{1, 3, 94, "MM_S_S-3", true},
	{1, 4, 95, "MM_S_S-4", true},
	{1, 5, 96, "MM_S_S-5", true},
	{1, 6, 97, "MM_S_S-6", true},
	{1, 7, 98, "MM_S_S-7", true},
	{1, 8, 99, "MM_S_S-8", true},
	{1, 9, 100, "MM_S_S-9", true},
	{1, 10, 101, "MM_S_S-10", true},

	/* 100 */
	{1, 11, 102, "MM_S_S-11", true},
	{1, 12, 103, "MM_S_S-12", true},
	{1, 13, 104, "MM_S_S-13", true},
	{1, 14, 105, "MM_S_S-14", true},
	{1, 15, 106, "MM_S_S-15", true},
	{1, 16, 107, "MM_S_S-16", true},
	{1, 17, 108, "MM_S_S-17", true},
	{1, 18, 109, "MM_S_S-18", true},
	{1, 19, 110, "MM_S_S-19", true},
	{1, 20, 111, "MM_S_S-20", true},

	/* 110 */
	{1, 21, 112, "MM_S_S-21", true},
	{1, 22, 113, "MM_S_S-22", true},
	{1, 23, 114, "MM_S_S-23", true},
	{1, 24, 115, "MM_S_S-24", true},
	{1, 25, 116, "MM_S_S-25", true},
	{1, 26, 117, "MM_S_S-26", true},
	{1, 27, 118, "MM_S_S-27", true},
	{1, 28, 119, "MM_S_S-28", true},
	{1, 29, 120, "MM_S_S-29", true},
	{1, 30, 121, "MM_S_S-30", true},

	/* 120 */
	{1, 31, 122, "MM_S_S-31", true},
	{1, 32, 123, "MM_S_S-32", true},
	{1, 33, 124, "MM_S_S-33", true},
	{1, 34, 125, "MM_S_S-34", true},
	{1, 35, 126, "MM_S_S-100", true},
	{1, 36, 127, "MM_S_S-101", true},
	{1, 37, 128, "MM_S_S-102", true},
	{1, 38, 129, "MM_S_S-103", true},
	{1, 39, 130, "MM_S_S-104", true},
	{1, 40, 131, "MM_S_S-105", true},

	/* 130 */
	{1, 41, 132, "MM_S_S-106", true},
	{1, 42, 133, "MM_S_S-107", true},
	{1, 43, 134, "MM_S_S-108", true},
	{1, 44, 135, "MM_S_S-109", true},
	{1, 45, 136, "MM_S_S-110", true},
	{1, 46, 137, "MM_S_S-111", true},
	{1, 47, 138, "MM_S_S-112", true},
	{1, 48, 139, "MM_S_S-113", true},
	{1, 49, 140, "MM_S_S-114", true},
	{1, 50, 141, "MM_S_S-115", true},

	/* 140 */
	{1, 51, 142, "MM_S_S-116", true},
	{1, 52, 143, "MM_S_S-117", true},
	{1, 53, 144, "MM_S_S-118", true},
	{1, 54, 145, "MM_S_S-119", true},
	{1, 55, 146, "MM_S_S-120", true},
	{1, 56, 147, "MM_S_S-121", true},
	{1, 57, 148, "MM_S_S-122", true},
	{1, 58, 149, "MM_S_S-123", true},
	{1, 59, 150, "MM_S_S-124", true},
	{1, 60, 151, "MM_S_S-125", true},

	/* 150 */
	{1, 61, 152, "MM_S_S-126", true},
	{1, 62, 153, "MM_S_S-127", true},
	{1, 63, 154, "MM_S_S-128", true},
	{1, 64, 155, "MM_S_S-129", true},
	{1, 65, 156, "MM_S_S-130", true},
	{1, 66, 157, "MM_S_S-131", true},
	{1, 67, 158, "MM_S_S-132", true},
	{1, 68, 159, "MM_S_S-133", true},
	{1, 69, 160, "MM_S_S-134", true},
	{1, 70, 161, "MM_S_S-135", true},

	/* 160 */
	{1, 71, 162, "MM_S_S-136", true},
	{1, 72, 163, "MM_S_S-137", true},
	{1, 73, 164, "MM_S_S-138", true},
	{1, 74, 165, "MM_S_S-139", true},
	{1, 75, 166, "MM_S_S-140", true},
	{1, 76, 167, "MM_S_S-141", true},
	{1, 77, 168, "MM_S_S-142", true},
	{1, 78, 169, "MM_S_S-143", true},
	{1, 79, 170, "MM_S_S-200", true},
	{1, 80, 171, "MM_S_S-201", true},

	/* 170 */
	{1, 81, 172, "MM_S_S-202", true},
	{1, 82, 173, "MM_S_S-203", true},
	{1, 83, 174, "MM_S_S-204", true},
	{1, 84, 175, "MM_S_S-205", true},
	{1, 85, 176, "MM_S_S-206", true},
	{1, 86, 177, "MM_S_S-207", true},
	{1, 87, 178, "MM_S_S-300", true},
	{1, 88, 179, "MM_S_S-301", true},
	{1, 89, 180, "MM_S_S-302", true},
	{1, 90, 181, "MM_S_S-303", true},

	/* 180 */
	{1, 91, 182, "MM_S_S-304", true},
	{1, 92, 183, "MM_S_S-305", true},
	{1, 93, 184, "MM_S_S-400", true},
	{1, 94, 185, "MM_S_S-401", true},
	{1, 95, 186, "MM_S_S-402", true},
	{1, 96, 187, "MM_S_S-403", true},
	{1, 97, 188, "MM_S_S-404", true},
	{1, 98, 189, "MM_S_S-405", true},
	{1, 99, 190, "MM_S_S-406", true},
	{1, 100, 191, "MM_S_S-407", true},

	/* 190 */
	{1, 101, 192, "MM_S_S-408", true},
	{1, 102, 193, "MM_S_S-409", true},
	{1, 103, 194, "MM_S_S-410", true},
	{1, 104, 195, "MM_S_S-411", true},
	{1, 105, 196, "MM_S_S-412", true},
	{1, 106, 197, "MM_S_S-413", true},
	{1, 107, 198, "MM_S_S-414", true},
	{1, 108, 199, "MM_S_S-415", true},
	{1, 109, 200, "MM_S_S-416", true},
	{1, 110, 201, "MM_S_S-417", true},

	/* 200 */
	{1, 111, 202, "MM_S_S-418", true},
	{1, 112, 203, "MM_S_S-419", true},
	{1, 113, 204, "MM_S_S-420", true},
	{1, 114, 205, "MM_S_S-421", true},
	{1, 115, 206, "MM_S_S-422", true},
	{1, 116, 207, "MM_S_S-423", true},
	{1, 117, 208, "MM_S_S-424", true},
	{1, 118, 209, "MM_S_S-425", true},
	{1, 119, 210, "MM_S_S-426", true},
	{1, 120, 211, "MM_S_S-427", true},

	/* 210 */
	{1, 121, 212, "MM_S_S-428", true},
	{1, 122, 213, "MM_S_S-429", true},
	{1, 123, 214, "MM_S_S-430", true},
	{1, 124, 215, "MM_S_S-431", true},
	{1, 125, 216, "MM_S_S-432", true},
	{1, 126, 217, "MM_S_S-433", true},
	{1, 127, 218, "MM_S_S-434", true},
	{1, 128, 219, "MM_S_S-435", true},
	{1, 129, 220, "MM_S_S-436", true},
	{1, 130, 221, "MM_S_S-437", true},

	/* 220 */
	{1, 131, 222, "MM_S_S-438", true},
	{1, 132, 223, "MM_S_S-439", true},
	{1, 133, 224, "MM_S_S-440", true},
	{1, 134, 225, "MM_S_S-441", true},
	{1, 135, 226, "MM_S_S-442", true},
	{1, 136, 227, "MM_S_S-443", true},
	{1, 137, 228, "MM_S_S-444", true},
	{1, 138, 229, "MM_S_S-445", true},
	{1, 139, 230, "MM_S_S-446", true},
	{1, 140, 231, "MM_S_S-447", true},

	/* 230 */
	{1, 141, 232, "MM_S_S-448", true},
	{1, 142, 233, "MM_S_S-449", true},
	{1, 143, 234, "MM_S_S-450", true},
	{1, 144, 235, "MM_S_S-451", true},
	{1, 145, 236, "MM_S_S-452", true},
	{1, 146, 237, "MM_S_S-453", true},
	{1, 147, 238, "MM_S_S-454", true},
	{1, 148, 239, "MM_S_S-455", true},
	{1, 149, 240, "MM_S_S-456", true},
	{1, 150, 241, "MM_S_S-457", true},

	/* 240 */
	{1, 151, 242, "MM_S_S-458", true},
	{1, 152, 243, "MM_S_S-459", true},
	{1, 153, 244, "MM_S_S-460", true},
	{1, 154, 245, "MM_S_S-461", true},
	{1, 155, 246, "MM_S_S-462", true},
	{1, 156, 247, "MM_S_S-463", true},
	{1, 157, 248, "MM_S_S-464", true},
	{1, 158, 249, "MM_S_S-465", true},
	{1, 159, 250, "MM_S_S-466", true},
	{1, 160, 251, "MM_S_S-467", true},

	/* 250 */
	{1, 161, 252, "MM_S_S-468", true},
	{1, 162, 253, "MM_S_S-469", true},
	{1, 163, 254, "MM_S_S-470", true},
	{1, 164, 255, "MM_S_S-471", true},
	{1, 165, 256, "MM_S_S-472", true},
	{1, 166, 257, "MM_S_S-473", true},
	{1, 167, 258, "MM_S_S-474", true},
	{1, 168, 259, "MM_S_S-475", true},
	{1, 169, 260, "MM_S_S-476", true},
	{1, 170, 261, "MM_S_S-477", true},

	/* 260 */
	{1, 171, 262, "MM_S_S-478", true},
	{1, 172, 263, "MM_S_S-479", true},
	{1, 173, 264, "MM_S_S-480", true},
	{1, 174, 265, "MM_S_S-481", true},
	{1, 175, 266, "MM_S_S-482", true},
	{1, 176, 267, "MM_S_S-483", true},
	{1, 177, 268, "MM_S_S-484", true},
	{1, 178, 269, "MM_S_S-485", true},
	{1, 179, 270, "MM_S_S-486", true},
	{1, 180, 271, "MM_S_S-487", true},

	/* 270 */
	{1, 181, 272, "MM_S_S-488", true},
	{1, 182, 273, "MM_S_S-489", true},
	{1, 183, 274, "MM_S_S-490", true},
	{1, 184, 275, "MM_S_S-491", true},
	{1, 185, 276, "MM_S_S-492", true},
	{1, 186, 277, "MM_S_S-493", true},
	{1, 187, 278, "MM_S_S-494", true},
	{1, 188, 279, "MM_S_S-495", true},
	{1, 189, 280, "MM_S_S-496", true},
	{1, 190, 281, "MM_S_S-497", true},

	/* 280 */
	{1, 191, 282, "MM_S_S-498", true},
	{1, 192, 283, "MM_S_S-499", true},
	{1, 193, 284, "MM_S_S-500", true},
	{1, 194, 285, "MM_S_S-501", true},
	{1, 195, 286, "MM_S_S-502", true},
	{1, 196, 287, "MM_S_S-503", true},
	{1, 197, 288, "MM_S_S-504", true},
	{1, 198, 289, "MM_S_S-505", true},
	{1, 199, 290, "MM_S_S-506", true},
	{1, 200, 291, "MM_S_S-507", true},

	/* 290 */
	{1, 201, 292, "MM_S_S-508", true},
	{1, 202, 293, "MM_S_S-509", true},
	{1, 203, 294, "MM_S_S-510", true},
	{1, 204, 295, "MM_S_S-511", true},
	{1, 205, 296, "MM_S_S-512", true},
	{1, 206, 297, "MM_S_S-513", true},
	{1, 207, 298, "MM_S_S-514", true},
	{1, 208, 299, "MM_S_S-515", true},
	{1, 209, 300, "MM_S_S-516", true},
	{1, 210, 301, "MM_S_S-517", true},

	/* 300 */
	{1, 211, 302, "MM_S_S-518", true},
	{1, 212, 303, "MM_S_S-519", true},
	{1, 213, 304, "MM_S_S-520", true},
	{1, 214, 305, "MM_S_S-521", true},
	{1, 215, 306, "MM_S_S-522", true},
	{1, 216, 307, "MM_S_S-523", true},
	{1, 217, 308, "MM_S_S-524", true},
	{1, 218, 309, "MM_S_S-525", true},
	{1, 219, 310, "MM_S_S-526", true},
	{1, 220, 311, "MM_S_S-527", true},

	/* 310 */
	{1, 221, 312, "MM_S_S-528", true},
	{1, 222, 313, "MM_S_S-529", true},
	{1, 223, 314, "MM_S_S-530", true},
	{1, 224, 315, "MM_S_S-531", true},
	{1, 225, 316, "MM_S_S-532", true},
	{1, 226, 317, "MM_S_S-533", true},
	{1, 227, 318, "MM_S_S-534", true},
	{1, 228, 319, "MM_S_S-535", true},
	{1, 229, 320, "MM_S_S-536", true},
	{1, 230, 321, "MM_S_S-537", true},

	/* 320 */
	{1, 231, 322, "MM_S_S-538", true},
	{1, 232, 323, "MM_S_S-539", true},
	{1, 233, 324, "MM_S_S-540", true},
	{1, 234, 325, "MM_S_S-541", true},
	{1, 235, 326, "MM_S_S-542", true},
	{1, 236, 327, "MM_S_S-543", true},
	{1, 237, 328, "MM_S_S-544", true},
	{1, 238, 329, "MM_S_S-545", true},
	{1, 239, 330, "MM_S_S-546", true},
	{1, 240, 331, "MM_S_S-547", true},

	/* 330 */
	{1, 241, 332, "MM_S_S-548", true},
	{1, 242, 333, "MM_S_S-549", true},
	{1, 243, 334, "MM_S_S-550", true},
	{1, 244, 335, "MM_S_S-551", true},
	{1, 245, 336, "MM_S_S-552", true},
	{1, 246, 337, "MM_S_S-553", true},
	{1, 247, 338, "MM_S_S-554", true},
	{1, 248, 339, "MM_S_S-555", true},
	{1, 249, 340, "MM_S_S-556", true},
	{1, 250, 341, "MM_S_S-557", true},

	/* 340 */
	{1, 251, 342, "MM_S_S-558", true},
	{1, 252, 343, "MM_S_S-559", true},
	{1, 253, 344, "MM_S_S-560", true},
	{1, 254, 345, "MM_S_S-561", true},
	{1, 255, 346, "MM_S_S-562", true},
	{2, 0, 347, "MM_S_S-563", true},
	{2, 1, 348, "MM_S_S-564", true},
	{2, 2, 349, "MM_S_S-565", true},
	{2, 3, 350, "MM_S_S-566", true},
	{2, 4, 351, "MM_S_S-567", true},

	/* 350 */
	{2, 5, 352, "MM_S_S-568", true},
	{2, 6, 353, "MM_S_S-569", true},
	{2, 7, 354, "MM_S_S-570", true},
	{2, 8, 355, "MM_S_S-571", true},
	{2, 9, 356, "MM_S_S-572", true},
	{2, 10, 357, "MM_S_S-573", true},
	{2, 11, 358, "MM_S_S-574", true},
	{2, 12, 359, "MM_S_S-575", true},
	{2, 13, 360, "MM_S_S-576", true},
	{2, 14, 361, "MM_S_S-577", true},

	/* 360 */
	{2, 15, 362, "MM_S_S-578", true},
	{2, 16, 363, "MM_S_S-579", true},
	{2, 17, 364, "MM_S_S-580", true},
	{2, 18, 365, "MM_S_S-581", true},
	{2, 19, 366, "MM_S_S-582", true},
	{2, 20, 367, "MM_S_S-583", true},
	{2, 21, 368, "MM_S_S-584", true},
	{2, 22, 369, "MM_S_S-585", true},
	{2, 23, 370, "MM_S_S-586", true},
	{2, 24, 371, "MM_S_S-587", true},

	/* 370 */
	{2, 25, 372, "MM_S_S-588", true},
	{2, 26, 373, "MM_S_S-589", true},
	{2, 27, 374, "MM_S_S-590", true},
	{2, 28, 375, "MM_S_S-591", true},
	{2, 29, 376, "MM_S_S-592", true},
	{2, 30, 377, "MM_S_S-593", true},
	{2, 31, 378, "MM_S_S-594", true},
	{2, 32, 379, "MM_S_S-595", true},
	{2, 33, 380, "MM_S_S-596", true},
	{2, 34, 381, "MM_S_S-597", true},

	/* 380 */
	{2, 35, 382, "MM_S_S-598", true},
	{2, 36, 383, "MM_S_S-599", true},
	{2, 37, 384, "MM_S_S-600", true},
	{2, 38, 385, "MM_S_S-601", true},
	{2, 39, 386, "MM_S_S-602", true},
	{2, 40, 387, "MM_S_S-603", true},
	{2, 41, 388, "MM_S_S-800", true},
	{2, 42, 389, "MM_S_S-801", true},
	{2, 43, 390, "MM_S_S-802", true},
	{2, 44, 391, "MM_S_S-803", true},

	/* 390 */
	{2, 45, 392, "MM_S_S-804", true},
	{2, 46, 393, "MM_S_S-805", true},
	{2, 47, 394, "MM_S_S-806", true},
	{2, 48, 395, "MM_S_S-807", true},
	{2, 49, 396, "MM_S_S-808", true},
	{2, 50, 397, "MM_S_S-809", true},
	{2, 51, 398, "MM_S_S-810", true},
	{2, 52, 399, "MM_S_S-811", true},
	{2, 53, 400, "MM_S_S-700", true},
	{2, 54, 401, "MM_S_S-701", true},

	/* 400 */
	{2, 55, 402, "MM_S_S-702", true},
	{2, 56, 403, "MM_S_S-703", true},
	{2, 57, 404, "MM_S_S-704", true},
	{2, 58, 405, "MM_S_S-705", true},
	{2, 59, 406, "MM_S_S-706", true},
	{2, 60, 407, "MM_S_S-707", true},
	{2, 61, 408, "MM_S_S-708", true},
	{2, 62, 409, "MM_S_S-709", true},
	{2, 63, 410, "MM_S_S-710", true},
	{2, 64, 411, "MM_S_S-711", true},

	/* 410 */
	{2, 65, 412, "MM_S_S-712", true},
	{2, 66, 413, "MM_S_S-713", true},
	{2, 67, 414, "MM_S_S-714", true},
	{2, 68, 415, "MM_S_S-715", true},
	{-1, -1, 416, "OOB_way_en", true},
	{-1, -1, 417, "OOB_way_en", true},
	{-1, -1, 418, "OOB_way_en", true},
	{-1, -1, 419, "OOB_way_en", true},
	{-1, -1, 420, "OOB_way_en", true},
	{-1, -1, 421, "OOB_way_en", true},

	/* 420 */
	{-1, -1, 422, "OOB_way_en", true},
	{-1, -1, 423, "OOB_way_en", true},
	{-1, -1, 424, "OOB_way_en", true},
	{-1, -1, 425, "OOB_way_en", true},
	{-1, -1, 426, "OOB_way_en", true},
	{-1, -1, 427, "OOB_way_en", true},
	{-1, -1, 428, "OOB_way_en", true},
	{-1, -1, 429, "OOB_way_en", true},
	{-1, -1, 430, "OOB_way_en", true},
	{-1, -1, 431, "OOB_way_en", true},

	/* 430 */
	{-1, -1, 432, "OOB_way_en", true},
	{-1, -1, 433, "OOB_way_en", true},
	{-1, -1, 434, "OOB_way_en", true},
	{-1, -1, 435, "OOB_way_en", true},
	{-1, -1, 436, "OOB_way_en", true},
	{-1, -1, 437, "OOB_way_en", true},
	{-1, -1, 438, "OOB_way_en", true},
	{-1, -1, 439, "OOB_way_en", true},
	{-1, -1, 440, "OOB_way_en", true},
	{-1, -1, 441, "OOB_way_en", true},

	/* 440 */
	{-1, -1, 442, "OOB_way_en", true},
	{-1, -1, 443, "OOB_way_en", true},
	{-1, -1, 444, "OOB_way_en", true},
	{-1, -1, 445, "OOB_way_en", true},
	{-1, -1, 446, "OOB_way_en", true},
	{-1, -1, 447, "OOB_way_en", true},
	{-1, -1, 448, "OOB_way_en", true},
	{-1, -1, 449, "OOB_way_en", true},
	{-1, -1, 450, "OOB_way_en", true},
	{-1, -1, 451, "OOB_way_en", true},

	/* 450 */
	{-1, -1, 452, "OOB_way_en", true},
	{-1, -1, 453, "OOB_way_en", true},
	{-1, -1, 454, "OOB_way_en", true},
	{-1, -1, 455, "OOB_way_en", true},
	{-1, -1, 456, "OOB_way_en", true},
	{-1, -1, 457, "OOB_way_en", true},
	{-1, -1, 458, "OOB_way_en", true},
	{-1, -1, 459, "OOB_way_en", true},
	{-1, -1, 460, "OOB_way_en", true},
	{-1, -1, 461, "OOB_way_en", true},

	/* 460 */
	{-1, -1, 462, "Decode_error", true},
	{-1, -1, 463, "Decode_error", true},
	{-1, -1, 464, "SRAMROM", true},
	{-1, -1, 465, "MDP_MALI", true},
	{-1, -1, 466, "reserve", false},
	{-1, -1, 467, "MMSYS_MALI", true},
	{-1, -1, 468, "PMIC_WRAP", false},
	{-1, -1, 469, "PMIF1", false},
	{-1, -1, 470, "PMIF2", false},
	{-1, -1, 471, "Reserve", false},

	/* 470 */
	{-1, -1, 472, "Reserve", false},
	{-1, -1, 473, "DEVICE_APC_INFRA_AO", false},
	{-1, -1, 474, "DEVICE_APC_INFRA_PDN", false},

};

static struct mtk_device_info mt6877_devices_peri[] = {
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
	{0, 14, 37, "AUDIO_S", true},
	{0, 15, 38, "AUDIO_S-1", true},
	{0, 16, 82, "SSUSB_S", true},
	{0, 17, 83, "SSUSB_S-1", true},
	{0, 18, 84, "SSUSB_S-2", true},
	{0, 19, 85, "DEBUGSYS_APB_S", true},

	/* 20 */
	{0, 20, 86, "DRAMC_MD32_S0_APB_S", true},
	{0, 21, 87, "DRAMC_MD32_S0_APB_S-1", true},
	{0, 22, 97, "CONN_S", true},
	{0, 23, 98, "DRAMC_CH0_TOP0_APB_S", true},
	{0, 24, 99, "DRAMC_CH0_TOP1_APB_S", true},
	{0, 25, 100, "DRAMC_CH0_TOP2_APB_S", true},
	{0, 26, 101, "DRAMC_CH0_TOP3_APB_S", true},
	{0, 27, 102, "DRAMC_CH0_TOP4_APB_S", true},
	{0, 28, 103, "DRAMC_CH0_TOP5_APB_S", true},
	{0, 29, 104, "DRAMC_CH0_TOP6_APB_S", true},

	/* 30 */
	{0, 30, 105, "DRAMC_CH1_TOP0_APB_S", true},
	{0, 31, 106, "DRAMC_CH1_TOP1_APB_S", true},
	{0, 32, 107, "DRAMC_CH1_TOP2_APB_S", true},
	{0, 33, 108, "DRAMC_CH1_TOP3_APB_S", true},
	{0, 34, 109, "DRAMC_CH1_TOP4_APB_S", true},
	{0, 35, 110, "DRAMC_CH1_TOP5_APB_S", true},
	{0, 36, 111, "DRAMC_CH1_TOP6_APB_S", true},
	{0, 37, 113, "CCIF2_AP_APB_S", true},
	{0, 38, 114, "CCIF2_MD_APB_S", true},
	{0, 39, 115, "CCIF3_AP_APB_S", true},

	/* 40 */
	{0, 40, 116, "CCIF3_MD_APB_S", true},
	{0, 41, 117, "CCIF4_AP_APB_S", true},
	{0, 42, 118, "CCIF4_MD_APB_S", true},
	{0, 43, 119, "CCIF5_AP_APB_S", true},
	{0, 44, 120, "CCIF5_MD_APB_S", true},
	{0, 45, 121, "SSC_INFRA_APB0_S", true},
	{0, 46, 122, "SSC_INFRA_APB1_S", true},
	{0, 47, 123, "SSC_INFRA_APB2_S", true},
	{0, 48, 124, "DEVICE_MPU_ACP_APB_S", true},
	{1, 0, 3, "TINSYS_S", true},

	/* 50 */
	{1, 1, 4, "TINSYS_S-1", true},
	{1, 2, 5, "TINSYS_S-2", true},
	{1, 3, 6, "TINSYS_S-3", true},
	{1, 4, 7, "TINSYS_S-4", true},
	{1, 5, 8, "TINSYS_S-5", true},
	{1, 6, 9, "TINSYS_S-6", true},
	{1, 7, 10, "TINSYS_S-7", true},
	{1, 8, 11, "TINSYS_S-8", true},
	{1, 9, 12, "TINSYS_S-9", true},
	{1, 10, 13, "TINSYS_S-10", true},

	/* 60 */
	{1, 11, 14, "TINSYS_S-11", true},
	{1, 12, 15, "TINSYS_S-12", true},
	{1, 13, 16, "TINSYS_S-13", true},
	{1, 14, 17, "TINSYS_S-14", true},
	{1, 15, 18, "TINSYS_S-15", true},
	{1, 16, 19, "TINSYS_S-16", true},
	{1, 17, 20, "TINSYS_S-17", true},
	{1, 18, 21, "TINSYS_S-18", true},
	{1, 19, 22, "TINSYS_S-19", true},
	{1, 20, 23, "TINSYS_S-20", true},

	/* 70 */
	{1, 21, 24, "TINSYS_S-21", true},
	{1, 22, 25, "TINSYS_S-22", true},
	{1, 23, 39, "MD_AP_S", true},
	{1, 24, 40, "MD_AP_S-1", true},
	{1, 25, 41, "MD_AP_S-2", true},
	{1, 26, 42, "MD_AP_S-3", true},
	{1, 27, 43, "MD_AP_S-4", true},
	{1, 28, 44, "MD_AP_S-5", true},
	{1, 29, 45, "MD_AP_S-6", true},
	{1, 30, 46, "MD_AP_S-7", true},

	/* 80 */
	{1, 31, 47, "MD_AP_S-8", true},
	{1, 32, 48, "MD_AP_S-9", true},
	{1, 33, 49, "MD_AP_S-10", true},
	{1, 34, 50, "MD_AP_S-11", true},
	{1, 35, 51, "MD_AP_S-12", true},
	{1, 36, 52, "MD_AP_S-13", true},
	{1, 37, 53, "MD_AP_S-14", true},
	{1, 38, 54, "MD_AP_S-15", true},
	{1, 39, 55, "MD_AP_S-16", true},
	{1, 40, 56, "MD_AP_S-17", true},

	/* 90 */
	{1, 41, 57, "MD_AP_S-18", true},
	{1, 42, 58, "MD_AP_S-19", true},
	{1, 43, 59, "MD_AP_S-20", true},
	{1, 44, 60, "MD_AP_S-21", true},
	{1, 45, 61, "MD_AP_S-22", true},
	{1, 46, 62, "MD_AP_S-23", true},
	{1, 47, 63, "MD_AP_S-24", true},
	{1, 48, 64, "MD_AP_S-25", true},
	{1, 49, 65, "MD_AP_S-26", true},
	{1, 50, 66, "MD_AP_S-27", true},

	/* 100 */
	{1, 51, 67, "MD_AP_S-28", true},
	{1, 52, 68, "MD_AP_S-29", true},
	{1, 53, 69, "MD_AP_S-30", true},
	{1, 54, 70, "MD_AP_S-31", true},
	{1, 55, 71, "MD_AP_S-32", true},
	{1, 56, 72, "MD_AP_S-33", true},
	{1, 57, 73, "MD_AP_S-34", true},
	{1, 58, 74, "MD_AP_S-35", true},
	{1, 59, 75, "MD_AP_S-36", true},
	{1, 60, 76, "MD_AP_S-37", true},

	/* 110 */
	{1, 61, 77, "MD_AP_S-38", true},
	{1, 62, 78, "MD_AP_S-39", true},
	{1, 63, 79, "MD_AP_S-40", true},
	{1, 64, 80, "MD_AP_S-41", true},
	{1, 65, 81, "MD_AP_S-42", true},
	{-1, -1, 125, "OOB_way_en", true},
	{-1, -1, 126, "OOB_way_en", true},
	{-1, -1, 127, "OOB_way_en", true},
	{-1, -1, 128, "OOB_way_en", true},
	{-1, -1, 129, "OOB_way_en", true},

	/* 120 */
	{-1, -1, 130, "OOB_way_en", true},
	{-1, -1, 131, "OOB_way_en", true},
	{-1, -1, 132, "OOB_way_en", true},
	{-1, -1, 133, "OOB_way_en", true},
	{-1, -1, 134, "OOB_way_en", true},
	{-1, -1, 135, "OOB_way_en", true},
	{-1, -1, 136, "OOB_way_en", true},
	{-1, -1, 137, "OOB_way_en", true},
	{-1, -1, 138, "OOB_way_en", true},
	{-1, -1, 139, "OOB_way_en", true},

	/* 130 */
	{-1, -1, 140, "OOB_way_en", true},
	{-1, -1, 141, "OOB_way_en", true},
	{-1, -1, 142, "OOB_way_en", true},
	{-1, -1, 143, "OOB_way_en", true},
	{-1, -1, 144, "OOB_way_en", true},
	{-1, -1, 145, "OOB_way_en", true},
	{-1, -1, 146, "OOB_way_en", true},
	{-1, -1, 147, "OOB_way_en", true},
	{-1, -1, 148, "OOB_way_en", true},
	{-1, -1, 149, "OOB_way_en", true},

	/* 140 */
	{-1, -1, 150, "OOB_way_en", true},
	{-1, -1, 151, "OOB_way_en", true},
	{-1, -1, 152, "OOB_way_en", true},
	{-1, -1, 153, "OOB_way_en", true},
	{-1, -1, 154, "OOB_way_en", true},
	{-1, -1, 155, "OOB_way_en", true},
	{-1, -1, 156, "OOB_way_en", true},
	{-1, -1, 157, "OOB_way_en", true},
	{-1, -1, 158, "OOB_way_en", true},
	{-1, -1, 159, "OOB_way_en", true},

	/* 150 */
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

	/* 160 */
	{-1, -1, 170, "OOB_way_en", true},
	{-1, -1, 171, "OOB_way_en", true},
	{-1, -1, 172, "OOB_way_en", true},
	{-1, -1, 173, "OOB_way_en", true},
	{-1, -1, 174, "OOB_way_en", true},
	{-1, -1, 175, "Decode_error", true},
	{-1, -1, 176, "Decode_error", true},
	{-1, -1, 177, "Decode_error", true},
	{-1, -1, 178, "Decode_error", true},
	{-1, -1, 179, "Decode_error", true},

	/* 170 */
	{-1, -1, 180, "DEVICE_APC_PERI_AO", false},
	{-1, -1, 181, "DEVICE_APC_PERI_PDN", false},

};

static struct mtk_device_info mt6877_devices_peri2[] = {
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
	{0, 90, 92, "SYS_CIRQ1_APB_S", true},
	{0, 91, 93, "SYS_CIRQ2_APB_S", true},
	{0, 92, 94, "DEBUG_TRACKER_APB1_S", true},
	{0, 93, 95, "EMI_APB_S", true},
	{0, 94, 96, "EMI_MPU_APB_S", true},
	{0, 95, 97, "DEVICE_MPU_PDN_APB_S", true},
	{0, 96, 98, "APDMA_APB_S", true},
	{0, 97, 99, "DEBUG_TRACKER_APB2_S", true},
	{0, 98, 100, "BCRM_INFRA_PDN_APB_S", true},
	{0, 99, 101, "BCRM_PERI_PDN_APB_S", true},

	/* 100 */
	{0, 100, 102, "BCRM_PERI_PDN2_APB_S", true},
	{0, 101, 103, "DEVICE_APC_PERI_PDN_APB_S", true},
	{0, 102, 104, "DEVICE_APC_PERI_PDN2_APB_S", true},
	{0, 103, 105, "BCRM_FMEM_PDN_APB_S", true},
	{0, 104, 106, "FAKE_ENGINE_1_S", true},
	{0, 105, 107, "FAKE_ENGINE_0_S", true},
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
	{-1, -1, 216, "Decode_error", true},
	{-1, -1, 217, "Decode_error", true},
	{-1, -1, 218, "Decode_error", true},
	{-1, -1, 219, "Decode_error", true},
	{-1, -1, 220, "Decode_error", true},
	{-1, -1, 221, "Decode_error", true},

	/* 220 */
	{-1, -1, 222, "Decode_error", true},
	{-1, -1, 223, "Decode_error", true},
	{-1, -1, 224, "CQ_DMA", false},
	{-1, -1, 225, "EMI", false},
	{-1, -1, 226, "EMI_MPU", false},
	{-1, -1, 227, "GCE", false},
	{-1, -1, 228, "AP_DMA", false},
	{-1, -1, 229, "DEVICE_APC_PERI_AO2", false},
	{-1, -1, 230, "DEVICE_APC_PERI_PDN2", false},

};

static struct mtk_device_info mt6877_devices_peri_par[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "UFS_S", true},
	{0, 1, 1, "UFS_S-1", true},
	{0, 2, 2, "UFS_S-2", true},
	{0, 3, 3, "UFS_S-3", true},
	{0, 4, 4, "MSDC0_S", true},
	{0, 5, 5, "MSDC1_S", true},
	{0, 6, 7, "AUXADC_APB_S", true},
	{0, 7, 8, "UART0_APB_S", true},
	{0, 8, 9, "UART1_APB_S", true},
	{0, 9, 10, "UART2_APB_S", true},

	/* 10 */
	{0, 10, 11, "UART3_APB_S", true},
	{0, 11, 12, "SPI0_APB_S", true},
	{0, 12, 13, "PTP_THERM_CTRL_APB_S", true},
	{0, 13, 14, "BTIF_APB_S", true},
	{0, 14, 15, "PERI_MBIST_PDN_APB_S", true},
	{0, 15, 16, "DISP_PWM_APB_S", true},
	{0, 16, 17, "SPI1_APB_S", true},
	{0, 17, 18, "SPI2_APB_S", true},
	{0, 18, 19, "SPI3_APB_S", true},
	{0, 19, 20, "SPI4_APB_S", true},

	/* 20 */
	{0, 20, 21, "SPI5_APB_S", true},
	{0, 21, 22, "SPI6_APB_S", true},
	{0, 22, 23, "SPI7_APB_S", true},
	{0, 23, 24, "BCRM_PERI_PAR_PDN_APB_S", true},
	{0, 24, 25, "DEVICE_APC_PERI_PAR_PDN_APB_S", true},
	{0, 25, 26, "PTP_THERM_CTRL2_APB_S", true},
	{0, 26, 27, "IIC_P2P_REMAP_APB_S", true},
	{0, 27, 28, "HWCCF_APB_S", true},
	{0, 28, 29, "DEVICE_APC_PERI_PAR_AO_APB_S", true},
	{0, 29, 30, "DEBUG_CTRL_PERI_PAR_AO_APB_S", true},

	/* 30 */
	{0, 30, 31, "BCRM_PERI_PAR_AO_APB_S", true},
	{-1, -1, 32, "OOB_way_en", true},
	{-1, -1, 33, "OOB_way_en", true},
	{-1, -1, 34, "OOB_way_en", true},
	{-1, -1, 35, "OOB_way_en", true},
	{-1, -1, 36, "OOB_way_en", true},
	{-1, -1, 37, "OOB_way_en", true},
	{-1, -1, 38, "OOB_way_en", true},
	{-1, -1, 39, "OOB_way_en", true},
	{-1, -1, 40, "OOB_way_en", true},

	/* 40 */
	{-1, -1, 41, "OOB_way_en", true},
	{-1, -1, 42, "OOB_way_en", true},
	{-1, -1, 43, "OOB_way_en", true},
	{-1, -1, 44, "OOB_way_en", true},
	{-1, -1, 45, "OOB_way_en", true},
	{-1, -1, 46, "OOB_way_en", true},
	{-1, -1, 47, "OOB_way_en", true},
	{-1, -1, 48, "OOB_way_en", true},
	{-1, -1, 49, "OOB_way_en", true},
	{-1, -1, 50, "OOB_way_en", true},

	/* 50 */
	{-1, -1, 51, "OOB_way_en", true},
	{-1, -1, 52, "OOB_way_en", true},
	{-1, -1, 53, "OOB_way_en", true},
	{-1, -1, 54, "OOB_way_en", true},
	{-1, -1, 55, "OOB_way_en", true},
	{-1, -1, 56, "OOB_way_en", true},
	{-1, -1, 57, "OOB_way_en", true},
	{-1, -1, 58, "OOB_way_en", true},
	{-1, -1, 59, "OOB_way_en", true},
	{-1, -1, 60, "OOB_way_en", true},

	/* 60 */
	{-1, -1, 61, "OOB_way_en", true},
	{-1, -1, 62, "Decode_error", true},
	{-1, -1, 63, "Decode_error", true},
	{-1, -1, 64, "Decode_error", true},
	{-1, -1, 65, "DISP_PWM", true},
	{-1, -1, 66, "IMP_IIC_WRAP", false},
	{-1, -1, 67, "DEVICE_APC_PERI_PAR_AO", false},
	{-1, -1, 68, "DEVICE_APC_PERI_PAR_PDN", false},

};

static struct mtk_device_num mtk6877_devices_num[] = {
	{SLAVE_TYPE_INFRA, VIO_SLAVE_NUM_INFRA},
	{SLAVE_TYPE_PERI, VIO_SLAVE_NUM_PERI},
	{SLAVE_TYPE_PERI2, VIO_SLAVE_NUM_PERI2},
	{SLAVE_TYPE_PERI_PAR, VIO_SLAVE_NUM_PERI_PAR},
};

static struct PERIAXI_ID_INFO peri_mi_id_to_master[] = {
	{"THERM",        { 0, 1, 1 } },
	{"SPM",          { 0, 1, 0 } },
	{"CCU",          { 0, 0, 1 } },
	{"THERM2",       { 0, 0, 0 } },
	{"SPM",          { 1, 2, 2 } },
};

static struct INFRAAXI_ID_INFO infra_mi_id_to_master[] = {
	{"CONNSYS",           { 0, 0, 0, 0,	2, 2, 2, 2,	2, 2, 2, 2,	2, 2 } },
	{"Tinysys",           { 0, 1, 0, 0,	2, 2, 2, 2,	2, 2, 0, 0,	0, 0 } },
	{"CQ_DMA",            { 0, 0, 1, 0,	0, 0, 0, 2,	2, 2, 0, 0,	0, 0 } },
	{"DebugTop",          { 0, 0, 1, 0,	1, 0, 0, 2,	0, 0, 0, 0,	0, 0 } },
	{"IPU",               { 0, 0, 1, 0,	0, 1, 0, 0,	0, 0, 2, 2,	0, 0 } },
	{"SSUSB",             { 0, 0, 1, 0,	0, 1, 0, 1,	0, 0, 0, 0,	2, 2 } },
	{"SSUSB2",            { 0, 0, 1, 0,	0, 1, 0, 1,	0, 0, 1, 0,	2, 2 } },
	{"SPI6",              { 0, 0, 1, 0,	0, 1, 0, 1,	0, 0, 0, 1,	2, 2 } },
	{"SPI0",              { 0, 0, 1, 0,	0, 1, 0, 1,	0, 0, 0, 1,	2, 2 } },
	{"MSDC1",             { 0, 0, 1, 0,	0, 1, 0, 1,	0, 0, 0, 1,	2, 2 } },
	{"PWM",               { 0, 0, 1, 0,	0, 1, 0, 1,	0, 0, 0, 1,	2, 2 } },
	{"HWCCF",             { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 0, 0,	2, 2 } },
	{"MSDC0",             { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 1, 0,	2, 2 } },
	{"SPI5",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 0, 1,	2, 2 } },
	{"SPI4",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 0, 1,	2, 2 } },
	{"SPI3",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 0, 1,	2, 2 } },
	{"SPI2",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 0, 1,	2, 2 } },
	{"SPI7",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 1, 1,	2, 2 } },
	{"SPI1",              { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 1, 1,	2, 2 } },
	{"Audio",             { 0, 0, 1, 0,	0, 1, 0, 0,	1, 0, 1, 1,	2, 2 } },
	{"AP_DMA_EXT",        { 0, 0, 1, 0,	0, 1, 0, 1,	1, 0, 2, 2,	2, 0 } },
	{"THERM",             { 0, 0, 1, 0,	0, 1, 0, 0,	0, 1, 2, 2,	0, 0 } },
	{"SPM",               { 0, 0, 1, 0,	0, 1, 0, 0,	0, 1, 2, 2,	0, 0 } },
	{"CCU",               { 0, 0, 1, 0,	0, 1, 0, 0,	0, 1, 2, 2,	0, 0 } },
	{"THERM2",            { 0, 0, 1, 0,	0, 1, 0, 0,	0, 1, 2, 2,	0, 0 } },
	{"DX_CC",             { 0, 0, 1, 0,	1, 1, 0, 2,	2, 2, 2, 0,	0, 0 } },
	{"GCE",               { 0, 0, 1, 0,	0, 0, 1, 2,	2, 0, 0, 0,	0, 0 } },
	{"CPUEB",             { 0, 0, 1, 0,	1, 0, 1, 2,	2, 2, 2, 2,	2, 0 } },
	{"DPMAIF",            { 0, 1, 1, 0,	2, 2, 2, 2,	0, 0, 0, 0,	0, 0 } },
	{"SCP_SSPM",          { 0, 0, 0, 1,	2, 2, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"UFS",               { 0, 1, 0, 1,	2, 2, 0, 0,	0, 0, 0, 0,	0, 0 } },
	{"CPUEB_2",           { 0, 0, 1, 1,	2, 2, 2, 2,	2, 2, 0, 0,	0, 0 } },
	{"MFG_M0_M",          { 0, 1, 1, 1,	2, 2, 2, 2,	2, 2, 2, 0,	0, 0 } },
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

static const char *mt6877_bus_id_to_master(uint32_t bus_id, uint32_t vio_addr,
		int slave_type, int shift_sta_bit, int domain)
{
	const char *err_master = "UNKNOWN_MASTER";
	uint8_t h_1byte;

	pr_debug(PFX "%s:0x%x, %s:0x%x, %s:0x%x, %s:%d\n",
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
		if (vio_addr <= 0x1FFFFF || shift_sta_bit == 7) {
			pr_info(PFX "vio_addr is from on-chip SRAMROM\n");
			if ((bus_id & 0x1) == 0)
				return "EMI_L2C_M";

			return infra_mi_trans(bus_id >> 1);

		} else if (shift_sta_bit >= 0 && shift_sta_bit <= 3) {
			return peri_mi_trans(bus_id);

		} else if (shift_sta_bit >= 4 && shift_sta_bit <= 6) {
			return infra_mi_trans(bus_id);

		} else if (shift_sta_bit == 8) {
			pr_info(PFX "vio_addr is from MMSYS_MALI\n");
			if ((bus_id & 0x1) == 1)
				return "GCE_M";

			return infra_mi_trans(bus_id >> 1);
		}

		return err_master;

	} else if (slave_type == SLAVE_TYPE_PERI) {
		if (shift_sta_bit == 1 || shift_sta_bit == 2 ||
				shift_sta_bit == 5) {
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
			if (vio_index == mt6877_devices_infra[i].vio_index)
				return mt6877_devices_infra[i].device;
		}

	} else if (slave_type == SLAVE_TYPE_PERI &&
			vio_index < VIO_SLAVE_NUM_PERI)
		for (i = 0; i < VIO_SLAVE_NUM_PERI; i++) {
			if (vio_index == mt6877_devices_peri[i].vio_index)
				return mt6877_devices_peri[i].device;
		}

	else if (slave_type == SLAVE_TYPE_PERI2 &&
			vio_index < VIO_SLAVE_NUM_PERI2)
		for (i = 0; i < VIO_SLAVE_NUM_PERI2; i++) {
			if (vio_index == mt6877_devices_peri2[i].vio_index)
				return mt6877_devices_peri2[i].device;
		}

	else if (slave_type == SLAVE_TYPE_PERI_PAR &&
			vio_index < VIO_SLAVE_NUM_PERI_PAR)
		for (i = 0; i < VIO_SLAVE_NUM_PERI_PAR; i++) {
			if (vio_index == mt6877_devices_peri_par[i].vio_index)
				return mt6877_devices_peri_par[i].device;
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

static uint32_t mt6877_shift_group_get(int slave_type, uint32_t vio_idx)
{
	if (slave_type == SLAVE_TYPE_INFRA) {
		if (vio_idx >= 0 && vio_idx <= 3)
			return 0;
		else if (vio_idx >= 4 && vio_idx <= 5)
			return 1;
		else if ((vio_idx >= 6 && vio_idx <= 31) ||
			 (vio_idx >= 416 && vio_idx <= 442) ||
			 vio_idx == 462)
			return 2;
		else if ((vio_idx >= 32 && vio_idx <= 45) ||
			 (vio_idx >= 443 && vio_idx <= 457) ||
			 vio_idx == 463)
			return 3;
		else if ((vio_idx >= 46 && vio_idx <= 74) ||
			vio_idx == 458)
			return 4;
		else if ((vio_idx >= 75 && vio_idx <= 82) ||
			vio_idx == 459)
			return 5;
		else if ((vio_idx >= 83 && vio_idx <= 87) ||
			vio_idx == 460)
			return 6;
		else if ((vio_idx >= 88 && vio_idx <= 90) ||
			vio_idx == 461)
			return 7;
		else if (vio_idx >= 91 && vio_idx <= 415)
			return 8;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI) {
		if ((vio_idx >= 0 && vio_idx <= 2) ||
		    (vio_idx >= 125 && vio_idx <= 128) ||
		    vio_idx == 175)
			return 0;
		else if ((vio_idx >= 3 && vio_idx <= 25) ||
			vio_idx == 129)
			return 1;
		else if ((vio_idx >= 26 && vio_idx <= 36) ||
			vio_idx == 130)
			return 2;
		else if ((vio_idx >= 37 && vio_idx <= 38) ||
			vio_idx == 131)
			return 3;
		else if ((vio_idx >= 39 && vio_idx <= 81) ||
			vio_idx == 132)
			return 4;
		else if ((vio_idx >= 82 && vio_idx <= 84) ||
			vio_idx == 133)
			return 5;
		else if (vio_idx == 85 ||
			(vio_idx >= 134 && vio_idx <= 135) ||
			vio_idx == 176)
			return 6;
		else if (vio_idx >= 86 && vio_idx <= 87)
			return 7;
		else if ((vio_idx >= 88 && vio_idx <= 97) ||
			(vio_idx >= 136 && vio_idx <= 145) ||
			vio_idx == 177)
			return 8;
		else if ((vio_idx >= 98 && vio_idx <= 112) ||
			(vio_idx >= 146 && vio_idx <= 161) ||
			vio_idx == 178)
			return 9;
		else if ((vio_idx >= 113 && vio_idx <= 124) ||
			(vio_idx >= 162 && vio_idx <= 174) ||
			vio_idx == 179)
			return 10;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI2) {
		if ((vio_idx >= 0 && vio_idx <= 2) ||
		    (vio_idx >= 108 && vio_idx <= 111) ||
		    vio_idx == 216)
			return 0;
		else if (vio_idx >= 3 && vio_idx <= 6)
			return 1;
		else if (vio_idx >= 7 && vio_idx <= 10)
			return 2;
		else if ((vio_idx >= 11 && vio_idx <= 26) ||
			 (vio_idx >= 112 && vio_idx <= 128) ||
			 vio_idx == 217)
			return 3;
		else if ((vio_idx >= 27 && vio_idx <= 34) ||
			 (vio_idx >= 129 && vio_idx <= 137) ||
			 vio_idx == 218)
			return 4;
		else if ((vio_idx >= 35 && vio_idx <= 50) ||
			 (vio_idx >= 138 && vio_idx <= 154) ||
			 vio_idx == 219)
			return 5;
		else if ((vio_idx >= 51 && vio_idx <= 66) ||
			 (vio_idx >= 155 && vio_idx <= 171) ||
			 vio_idx == 220)
			return 6;
		else if ((vio_idx >= 67 && vio_idx <= 74) ||
			 (vio_idx >= 172 && vio_idx <= 180) ||
			 vio_idx == 221)
			return 7;
		else if ((vio_idx >= 75 && vio_idx <= 94) ||
			 (vio_idx >= 181 && vio_idx <= 201) ||
			 vio_idx == 222)
			return 8;
		else if ((vio_idx >= 95 && vio_idx <= 107) ||
			 (vio_idx >= 202 && vio_idx <= 215) ||
			 vio_idx == 223)
			return 9;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	} else if (slave_type == SLAVE_TYPE_PERI_PAR) {
		if (vio_idx >= 0 && vio_idx <= 3)
			return 0;
		else if ((vio_idx >= 4 && vio_idx <= 6) ||
			 (vio_idx >= 32 && vio_idx <= 34) ||
			 vio_idx == 62)
			return 1;
		else if ((vio_idx >= 7 && vio_idx <= 28) ||
			 (vio_idx >= 35 && vio_idx <= 57) ||
			 vio_idx == 63)
			return 2;
		else if ((vio_idx >= 29 && vio_idx <= 31) ||
			 (vio_idx >= 58 && vio_idx <= 61) ||
			 vio_idx == 64)
			return 3;

		pr_err(PFX "%s:%d Wrong vio_idx:0x%x\n",
				__func__, __LINE__, vio_idx);

	}

	pr_err(PFX "%s:%d Wrong slave_type:0x%x\n",
			__func__, __LINE__, slave_type);

	return 31;
}

static struct mtk_devapc_dbg_status mt6877_devapc_dbg_stat = {
	.enable_ut = PLAT_DBG_UT_DEFAULT,
	.enable_KE = PLAT_DBG_KE_DEFAULT,
	.enable_AEE = PLAT_DBG_AEE_DEFAULT,
	.enable_WARN = PLAT_DBG_WARN_DEFAULT,
	.enable_dapc = PLAT_DBG_DAPC_DEFAULT,
};

void devapc_catch_illegal_range(phys_addr_t phys_addr, size_t size)
{
	struct mtk_devapc_dbg_status *dbg_stat = &mt6877_devapc_dbg_stat;

	/*
	 * Catch BROM addr mapped
	 */
	if (phys_addr >= 0x0 && phys_addr < SRAM_START_ADDR) {
		pr_err(PFX "%s: %s %s:(%pa), %s:(0x%lx)\n",
				"catch BROM address mapped!",
				__func__, "phys_addr", &phys_addr,
				"size", size);

		if (dbg_stat->enable_KE)
			BUG_ON(1);
	}
}
EXPORT_SYMBOL(devapc_catch_illegal_range);

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

static struct mtk_devapc_vio_info mt6877_devapc_vio_info = {
	.vio_mask_sta_num = mtk_vio_mask_sta_num,
	.sramrom_vio_idx = SRAMROM_VIO_INDEX,
	.mdp_vio_idx = MDP_VIO_INDEX,
	.disp2_vio_idx = MDP_VIO_INDEX,
	.mmsys_vio_idx = MMSYS_VIO_INDEX,
	.sramrom_slv_type = SRAMROM_SLAVE_TYPE,
	.mm2nd_slv_type = MM2ND_SLAVE_TYPE,
};

static const struct mtk_infra_vio_dbg_desc mt6877_vio_dbgs = {
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

static const struct mtk_sramrom_sec_vio_desc mt6877_sramrom_sec_vios = {
	.vio_id_mask = SRAMROM_SEC_VIO_ID_MASK,
	.vio_id_shift = SRAMROM_SEC_VIO_ID_SHIFT,
	.vio_domain_mask = SRAMROM_SEC_VIO_DOMAIN_MASK,
	.vio_domain_shift = SRAMROM_SEC_VIO_DOMAIN_SHIFT,
	.vio_rw_mask = SRAMROM_SEC_VIO_RW_MASK,
	.vio_rw_shift = SRAMROM_SEC_VIO_RW_SHIFT,
};

static const uint32_t mt6877_devapc_pds[] = {
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

static struct mtk_devapc_soc mt6877_data = {
	.dbg_stat = &mt6877_devapc_dbg_stat,
	.slave_type_arr = slave_type_to_str,
	.slave_type_num = SLAVE_TYPE_NUM,
	.device_info[SLAVE_TYPE_INFRA] = mt6877_devices_infra,
	.device_info[SLAVE_TYPE_PERI] = mt6877_devices_peri,
	.device_info[SLAVE_TYPE_PERI2] = mt6877_devices_peri2,
	.device_info[SLAVE_TYPE_PERI_PAR] = mt6877_devices_peri_par,
	.ndevices = mtk6877_devices_num,
	.vio_info = &mt6877_devapc_vio_info,
	.vio_dbgs = &mt6877_vio_dbgs,
	.sramrom_sec_vios = &mt6877_sramrom_sec_vios,
	.devapc_pds = mt6877_devapc_pds,
	.subsys_get = &index_to_subsys,
	.master_get = &mt6877_bus_id_to_master,
	.mm2nd_vio_handler = &mm2nd_vio_handler,
	.shift_group_get = mt6877_shift_group_get,
};

static const struct of_device_id mt6877_devapc_dt_match[] = {
	{ .compatible = "mediatek,mt6877-devapc" },
	{},
};

static int mt6877_devapc_probe(struct platform_device *pdev)
{
	return mtk_devapc_probe(pdev, &mt6877_data);
}

static int mt6877_devapc_remove(struct platform_device *dev)
{
	return mtk_devapc_remove(dev);
}

static struct platform_driver mt6877_devapc_driver = {
	.probe = mt6877_devapc_probe,
	.remove = mt6877_devapc_remove,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = mt6877_devapc_dt_match,
	},
};

module_platform_driver(mt6877_devapc_driver);

MODULE_DESCRIPTION("Mediatek MT6877 Device APC Driver");
MODULE_AUTHOR("Jackson Chang <jackson-kt.chang@mediatek.com>");
MODULE_LICENSE("GPL");
