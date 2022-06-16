/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __DEVAPC_MT6789_H__
#define __DEVAPC_MT6789_H__

#include "devapc-mtk-multi-ao.h"

/******************************************************************************
 * VARIABLE DEFINITION
 ******************************************************************************/
/* dbg status default setting */
#define PLAT_DBG_UT_DEFAULT		false
#define PLAT_DBG_KE_DEFAULT		true
#define PLAT_DBG_AEE_DEFAULT		true
#define PLAT_DBG_WARN_DEFAULT		true
#define PLAT_DBG_DAPC_DEFAULT		false

/******************************************************************************
 * STRUCTURE DEFINITION
 ******************************************************************************/
enum DEVAPC_SLAVE_TYPE {
	SLAVE_TYPE_INFRA = 0,
	SLAVE_TYPE_PERI,
	SLAVE_TYPE_PERI2,
	SLAVE_TYPE_PERI_PAR,
	SLAVE_TYPE_NUM,
};

enum DEVAPC_VIO_MASK_STA_NUM {
	VIO_MASK_STA_NUM_INFRA = 13,
	VIO_MASK_STA_NUM_PERI = 6,
	VIO_MASK_STA_NUM_PERI2 = 8,
	VIO_MASK_STA_NUM_PERI_PAR = 2,
};

enum DEVAPC_PD_OFFSET {
	PD_VIO_MASK_OFFSET = 0x0,
	PD_VIO_STA_OFFSET = 0x400,
	PD_VIO_DBG0_OFFSET = 0x900,
	PD_VIO_DBG1_OFFSET = 0x904,
	PD_VIO_DBG2_OFFSET = 0x908,
	PD_APC_CON_OFFSET = 0xF00,
	PD_SHIFT_STA_OFFSET = 0xF20,
	PD_SHIFT_SEL_OFFSET = 0xF30,
	PD_SHIFT_CON_OFFSET = 0xF10,
	PD_VIO_DBG3_OFFSET = 0x90C,
};

#define SRAMROM_SLAVE_TYPE	SLAVE_TYPE_INFRA	/* Infra */
#define MM2ND_SLAVE_TYPE	SLAVE_TYPE_INFRA	/* Infra */

enum OTHER_TYPES_INDEX {
	SRAMROM_VIO_INDEX = 392,
	MDP_VIO_INDEX = 393,
	MMSYS_VIO_INDEX = 395,
};

enum INFRACFG_MM2ND_VIO_NUM {
	INFRACFG_MM_VIO_STA_NUM = 2,
	INFRACFG_MDP_VIO_STA_NUM = 8,
};

enum DEVAPC_MODULE_ADDR {
	TINYSYS_START_ADDR = 0x10500000,
	TINYSYS_END_ADDR = 0x108FFFFF,
	MD_START_ADDR = 0x20000000,
	MD_END_ADDR = 0x2FFFFFFF,
	CONN_START_ADDR = 0x18000000,
	CONN_END_ADDR = 0x18FFFFFF,
	MDP_REGION1_START_ADDR = 0x15000000,
	MDP_REGION1_END_ADDR = 0x15FFFFFF,
	MDP_REGION2_START_ADDR = 0x1A000000,
	MDP_REGION2_END_ADDR = 0x1BFFFFFF,
	MDP_REGION3_START_ADDR = 0x1F000000,
	MDP_REGION3_END_ADDR = 0x1FFFFFFF,
	MMSYS_REGION1_START_ADDR = 0x14000000,
	MMSYS_REGION1_END_ADDR = 0x14FFFFFF,
	MMSYS_REGION2_START_ADDR = 0x16000000,
	MMSYS_REGION2_END_ADDR = 0x17FFFFFF,
};

enum INFRACFG_MM2ND_OFFSET {
	INFRACFG_MM_SEC_VIO0_OFFSET = 0xB30,
	INFRACFG_MDP_SEC_VIO0_OFFSET = 0xB40,
};

enum BUSID_LENGTH {
	PERIAXI_MI_BIT_LENGTH = 3,
	INFRAAXI_MI_BIT_LENGTH = 14,
};

struct PERIAXI_ID_INFO {
	const char	*master;
	uint8_t		bit[PERIAXI_MI_BIT_LENGTH];
};

struct INFRAAXI_ID_INFO {
	const char	*master;
	uint8_t		bit[INFRAAXI_MI_BIT_LENGTH];
};

/******************************************************************************
 * PLATFORM DEFINATION
 ******************************************************************************/

/* For Infra VIO_DBG */
#define INFRA_VIO_DBG_MSTID			0xFFFFFFFF
#define INFRA_VIO_DBG_MSTID_START_BIT		0
#define INFRA_VIO_DBG_DMNID			0x0000003F
#define INFRA_VIO_DBG_DMNID_START_BIT		0
#define INFRA_VIO_DBG_W_VIO			0x00000040
#define INFRA_VIO_DBG_W_VIO_START_BIT		6
#define INFRA_VIO_DBG_R_VIO			0x00000080
#define INFRA_VIO_DBG_R_VIO_START_BIT		7
#define INFRA_VIO_ADDR_HIGH			0x00000F00
#define INFRA_VIO_ADDR_HIGH_START_BIT		8

/* For SRAMROM VIO */
#define SRAMROM_SEC_VIO_ID_MASK			0x00FFFF00
#define SRAMROM_SEC_VIO_ID_SHIFT		8
#define SRAMROM_SEC_VIO_DOMAIN_MASK		0x0F000000
#define SRAMROM_SEC_VIO_DOMAIN_SHIFT		24
#define SRAMROM_SEC_VIO_RW_MASK			0x80000000
#define SRAMROM_SEC_VIO_RW_SHIFT		31

/* For MM 2nd VIO */
#define INFRACFG_MM2ND_VIO_DOMAIN_MASK		0x00000030
#define INFRACFG_MM2ND_VIO_DOMAIN_SHIFT		4
#define INFRACFG_MM2ND_VIO_ID_MASK		0x00FFFF00
#define INFRACFG_MM2ND_VIO_ID_SHIFT		8
#define INFRACFG_MM2ND_VIO_RW_MASK		0x01000000
#define INFRACFG_MM2ND_VIO_RW_SHIFT		24

#define SRAM_START_ADDR				(0x100000)

static struct mtk_device_info mt6789_devices_infra[] = {
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
	{0, 40, 42, "PMSR_APB_S", true},
	{0, 41, 43, "SRCLKEN_RC_APB_S", true},
	{0, 42, 44, "MFG_S_S", true},
	{0, 43, 45, "MFG_S_S-1", true},
	{0, 44, 46, "MFG_S_S-2", true},
	{0, 45, 47, "MFG_S_S-3", true},
	{0, 46, 48, "MFG_S_S-4", true},
	{0, 47, 49, "MFG_S_S-5", true},
	{0, 48, 50, "MFG_S_S-6", true},
	{0, 49, 51, "MFG_S_S-7", true},

	/* 50 */
	{0, 50, 52, "MFG_S_S-8", true},
	{0, 51, 53, "MFG_S_S-9", true},
	{0, 52, 54, "MFG_S_S-10", true},
	{0, 53, 55, "MFG_S_S-11", true},
	{0, 54, 56, "MFG_S_S-12", true},
	{0, 55, 57, "MFG_S_S-13", true},
	{0, 56, 58, "MFG_S_S-14", true},
	{0, 57, 59, "MFG_S_S-15", true},
	{0, 58, 60, "MFG_S_S-16", true},
	{0, 59, 61, "MFG_S_S-17", true},

	/* 60 */
	{0, 60, 62, "MCUSYS_CFGREG_APB_S", true},
	{0, 61, 63, "MCUSYS_CFGREG_APB_S-1", true},
	{0, 62, 64, "MCUSYS_CFGREG_APB_S-2", true},
	{0, 63, 65, "MCUSYS_CFGREG_APB_S-3", true},
	{0, 64, 66, "MCUSYS_CFGREG_APB_S-4", true},
	{0, 65, 67, "L3C_S", true},
	{0, 66, 68, "L3C_S-1", true},
	{1, 0, 69, "MM_S_S", true},
	{1, 1, 70, "MM_S_S-1", true},
	{1, 2, 71, "MM_S_S-2", true},

	/* 70 */
	{1, 3, 72, "MM_S_S-3", true},
	{1, 4, 73, "MM_S_S-4", true},
	{1, 5, 74, "MM_S_S-5", true},
	{1, 6, 75, "MM_S_S-6", true},
	{1, 7, 76, "MM_S_S-7", true},
	{1, 8, 77, "MM_S_S-8", true},
	{1, 9, 78, "MM_S_S-9", true},
	{1, 10, 79, "MM_S_S-10", true},
	{1, 11, 80, "MM_S_S-11", true},
	{1, 12, 81, "MM_S_S-12", true},

	/* 80 */
	{1, 13, 82, "MM_S_S-13", true},
	{1, 14, 83, "MM_S_S-14", true},
	{1, 15, 84, "MM_S_S-15", true},
	{1, 16, 85, "MM_S_S-16", true},
	{1, 17, 86, "MM_S_S-17", true},
	{1, 18, 87, "MM_S_S-18", true},
	{1, 19, 88, "MM_S_S-19", true},
	{1, 20, 89, "MM_S_S-20", true},
	{1, 21, 90, "MM_S_S-21", true},
	{1, 22, 91, "MM_S_S-22", true},

	/* 90 */
	{1, 23, 92, "MM_S_S-23", true},
	{1, 24, 93, "MM_S_S-24", true},
	{1, 25, 94, "MM_S_S-25", true},
	{1, 26, 95, "MM_S_S-26", true},
	{1, 27, 96, "MM_S_S-27", true},
	{1, 28, 97, "MM_S_S-28", true},
	{1, 29, 98, "MM_S_S-29", true},
	{1, 30, 99, "MM_S_S-30", true},
	{1, 31, 100, "MM_S_S-31", true},
	{1, 32, 101, "MM_S_S-32", true},

	/* 100 */
	{1, 33, 102, "MM_S_S-100", true},
	{1, 34, 103, "MM_S_S-101", true},
	{1, 35, 104, "MM_S_S-102", true},
	{1, 36, 105, "MM_S_S-103", true},
	{1, 37, 106, "MM_S_S-104", true},
	{1, 38, 107, "MM_S_S-105", true},
	{1, 39, 108, "MM_S_S-106", true},
	{1, 40, 109, "MM_S_S-107", true},
	{1, 41, 110, "MM_S_S-108", true},
	{1, 42, 111, "MM_S_S-109", true},

	/* 110 */
	{1, 43, 112, "MM_S_S-110", true},
	{1, 44, 113, "MM_S_S-111", true},
	{1, 45, 114, "MM_S_S-112", true},
	{1, 46, 115, "MM_S_S-113", true},
	{1, 47, 116, "MM_S_S-114", true},
	{1, 48, 117, "MM_S_S-115", true},
	{1, 49, 118, "MM_S_S-116", true},
	{1, 50, 119, "MM_S_S-117", true},
	{1, 51, 120, "MM_S_S-118", true},
	{1, 52, 121, "MM_S_S-119", true},

	/* 120 */
	{1, 53, 122, "MM_S_S-120", true},
	{1, 54, 123, "MM_S_S-121", true},
	{1, 55, 124, "MM_S_S-122", true},
	{1, 56, 125, "MM_S_S-123", true},
	{1, 57, 126, "MM_S_S-124", true},
	{1, 58, 127, "MM_S_S-125", true},
	{1, 59, 128, "MM_S_S-126", true},
	{1, 60, 129, "MM_S_S-127", true},
	{1, 61, 130, "MM_S_S-128", true},
	{1, 62, 131, "MM_S_S-129", true},

	/* 130 */
	{1, 63, 132, "MM_S_S-130", true},
	{1, 64, 133, "MM_S_S-131", true},
	{1, 65, 134, "MM_S_S-132", true},
	{1, 66, 135, "MM_S_S-133", true},
	{1, 67, 136, "MM_S_S-134", true},
	{1, 68, 137, "MM_S_S-135", true},
	{1, 69, 138, "MM_S_S-136", true},
	{1, 70, 139, "MM_S_S-137", true},
	{1, 71, 140, "MM_S_S-138", true},
	{1, 72, 141, "MM_S_S-139", true},

	/* 140 */
	{1, 73, 142, "MM_S_S-140", true},
	{1, 74, 143, "MM_S_S-141", true},
	{1, 75, 144, "MM_S_S-142", true},
	{1, 76, 145, "MM_S_S-143", true},
	{1, 77, 146, "MM_S_S-200", true},
	{1, 78, 147, "MM_S_S-201", true},
	{1, 79, 148, "MM_S_S-202", true},
	{1, 80, 149, "MM_S_S-203", true},
	{1, 81, 150, "MM_S_S-204", true},
	{1, 82, 151, "MM_S_S-205", true},

	/* 150 */
	{1, 83, 152, "MM_S_S-206", true},
	{1, 84, 153, "MM_S_S-207", true},
	{1, 85, 154, "MM_S_S-208", true},
	{1, 86, 155, "MM_S_S-209", true},
	{1, 87, 156, "MM_S_S-300", true},
	{1, 88, 157, "MM_S_S-301", true},
	{1, 89, 158, "MM_S_S-302", true},
	{1, 90, 159, "MM_S_S-303", true},
	{1, 91, 160, "MM_S_S-304", true},
	{1, 92, 161, "MM_S_S-305", true},

	/* 160 */
	{1, 93, 162, "MM_S_S-306", true},
	{1, 94, 163, "MM_S_S-307", true},
	{1, 95, 164, "MM_S_S-400", true},
	{1, 96, 165, "MM_S_S-401", true},
	{1, 97, 166, "MM_S_S-402", true},
	{1, 98, 167, "MM_S_S-403", true},
	{1, 99, 168, "MM_S_S-404", true},
	{1, 100, 169, "MM_S_S-405", true},
	{1, 101, 170, "MM_S_S-406", true},
	{1, 102, 171, "MM_S_S-407", true},

	/* 170 */
	{1, 103, 172, "MM_S_S-408", true},
	{1, 104, 173, "MM_S_S-409", true},
	{1, 105, 174, "MM_S_S-412", true},
	{1, 106, 175, "MM_S_S-413", true},
	{1, 107, 176, "MM_S_S-414", true},
	{1, 108, 177, "MM_S_S-415", true},
	{1, 109, 178, "MM_S_S-416", true},
	{1, 110, 179, "MM_S_S-417", true},
	{1, 111, 180, "MM_S_S-448", true},
	{1, 112, 181, "MM_S_S-449", true},

	/* 180 */
	{1, 113, 182, "MM_S_S-450", true},
	{1, 114, 183, "MM_S_S-451", true},
	{1, 115, 184, "MM_S_S-452", true},
	{1, 116, 185, "MM_S_S-453", true},
	{1, 117, 186, "MM_S_S-454", true},
	{1, 118, 187, "MM_S_S-455", true},
	{1, 119, 188, "MM_S_S-456", true},
	{1, 120, 189, "MM_S_S-457", true},
	{1, 121, 190, "MM_S_S-458", true},
	{1, 122, 191, "MM_S_S-459", true},

	/* 190 */
	{1, 123, 192, "MM_S_S-460", true},
	{1, 124, 193, "MM_S_S-461", true},
	{1, 125, 194, "MM_S_S-462", true},
	{1, 126, 195, "MM_S_S-463", true},
	{1, 127, 196, "MM_S_S-464", true},
	{1, 128, 197, "MM_S_S-465", true},
	{1, 129, 198, "MM_S_S-466", true},
	{1, 130, 199, "MM_S_S-467", true},
	{1, 131, 200, "MM_S_S-468", true},
	{1, 132, 201, "MM_S_S-469", true},

	/* 200 */
	{1, 133, 202, "MM_S_S-470", true},
	{1, 134, 203, "MM_S_S-471", true},
	{1, 135, 204, "MM_S_S-472", true},
	{1, 136, 205, "MM_S_S-473", true},
	{1, 137, 206, "MM_S_S-474", true},
	{1, 138, 207, "MM_S_S-475", true},
	{1, 139, 208, "MM_S_S-476", true},
	{1, 140, 209, "MM_S_S-477", true},
	{1, 141, 210, "MM_S_S-478", true},
	{1, 142, 211, "MM_S_S-479", true},

	/* 210 */
	{1, 143, 212, "MM_S_S-480", true},
	{1, 144, 213, "MM_S_S-481", true},
	{1, 145, 214, "MM_S_S-482", true},
	{1, 146, 215, "MM_S_S-483", true},
	{1, 147, 216, "MM_S_S-484", true},
	{1, 148, 217, "MM_S_S-485", true},
	{1, 149, 218, "MM_S_S-486", true},
	{1, 150, 219, "MM_S_S-487", true},
	{1, 151, 220, "MM_S_S-488", true},
	{1, 152, 221, "MM_S_S-489", true},

	/* 220 */
	{1, 153, 222, "MM_S_S-490", true},
	{1, 154, 223, "MM_S_S-491", true},
	{1, 155, 224, "MM_S_S-492", true},
	{1, 156, 225, "MM_S_S-493", true},
	{1, 157, 226, "MM_S_S-494", true},
	{1, 158, 227, "MM_S_S-495", true},
	{1, 159, 228, "MM_S_S-496", true},
	{1, 160, 229, "MM_S_S-497", true},
	{1, 161, 230, "MM_S_S-498", true},
	{1, 162, 231, "MM_S_S-499", true},

	/* 230 */
	{1, 163, 232, "MM_S_S-500", true},
	{1, 164, 233, "MM_S_S-501", true},
	{1, 165, 234, "MM_S_S-502", true},
	{1, 166, 235, "MM_S_S-503", true},
	{1, 167, 236, "MM_S_S-504", true},
	{1, 168, 237, "MM_S_S-505", true},
	{1, 169, 238, "MM_S_S-506", true},
	{1, 170, 239, "MM_S_S-507", true},
	{1, 171, 240, "MM_S_S-508", true},
	{1, 172, 241, "MM_S_S-509", true},

	/* 240 */
	{1, 173, 242, "MM_S_S-510", true},
	{1, 174, 243, "MM_S_S-511", true},
	{1, 175, 244, "MM_S_S-512", true},
	{1, 176, 245, "MM_S_S-513", true},
	{1, 177, 246, "MM_S_S-514", true},
	{1, 178, 247, "MM_S_S-515", true},
	{1, 179, 248, "MM_S_S-516", true},
	{1, 180, 249, "MM_S_S-517", true},
	{1, 181, 250, "MM_S_S-518", true},
	{1, 182, 251, "MM_S_S-519", true},

	/* 250 */
	{1, 183, 252, "MM_S_S-520", true},
	{1, 184, 253, "MM_S_S-521", true},
	{1, 185, 254, "MM_S_S-522", true},
	{1, 186, 255, "MM_S_S-523", true},
	{1, 187, 256, "MM_S_S-524", true},
	{1, 188, 257, "MM_S_S-525", true},
	{1, 189, 258, "MM_S_S-526", true},
	{1, 190, 259, "MM_S_S-527", true},
	{1, 191, 260, "MM_S_S-528", true},
	{1, 192, 261, "MM_S_S-529", true},

	/* 260 */
	{1, 193, 262, "MM_S_S-530", true},
	{1, 194, 263, "MM_S_S-531", true},
	{1, 195, 264, "MM_S_S-532", true},
	{1, 196, 265, "MM_S_S-533", true},
	{1, 197, 266, "MM_S_S-534", true},
	{1, 198, 267, "MM_S_S-535", true},
	{1, 199, 268, "MM_S_S-536", true},
	{1, 200, 269, "MM_S_S-537", true},
	{1, 201, 270, "MM_S_S-538", true},
	{1, 202, 271, "MM_S_S-539", true},

	/* 270 */
	{1, 203, 272, "MM_S_S-540", true},
	{1, 204, 273, "MM_S_S-541", true},
	{1, 205, 274, "MM_S_S-542", true},
	{1, 206, 275, "MM_S_S-543", true},
	{1, 207, 276, "MM_S_S-546", true},
	{1, 208, 277, "MM_S_S-547", true},
	{1, 209, 278, "MM_S_S-548", true},
	{1, 210, 279, "MM_S_S-549", true},
	{1, 211, 280, "MM_S_S-550", true},
	{1, 212, 281, "MM_S_S-551", true},

	/* 280 */
	{1, 213, 282, "MM_S_S-554", true},
	{1, 214, 283, "MM_S_S-555", true},
	{1, 215, 284, "MM_S_S-556", true},
	{1, 216, 285, "MM_S_S-557", true},
	{1, 217, 286, "MM_S_S-558", true},
	{1, 218, 287, "MM_S_S-559", true},
	{1, 219, 288, "MM_S_S-560", true},
	{1, 220, 289, "MM_S_S-561", true},
	{1, 221, 290, "MM_S_S-562", true},
	{1, 222, 291, "MM_S_S-563", true},

	/* 290 */
	{1, 223, 292, "MM_S_S-564", true},
	{1, 224, 293, "MM_S_S-565", true},
	{1, 225, 294, "MM_S_S-570", true},
	{1, 226, 295, "MM_S_S-574", true},
	{1, 227, 296, "MM_S_S-575", true},
	{1, 228, 297, "MM_S_S-576", true},
	{1, 229, 298, "MM_S_S-577", true},
	{1, 230, 299, "MM_S_S-578", true},
	{1, 231, 300, "MM_S_S-579", true},
	{1, 232, 301, "MM_S_S-580", true},

	/* 300 */
	{1, 233, 302, "MM_S_S-581", true},
	{1, 234, 303, "MM_S_S-582", true},
	{1, 235, 304, "MM_S_S-583", true},
	{1, 236, 305, "MM_S_S-584", true},
	{1, 237, 306, "MM_S_S-585", true},
	{1, 238, 307, "MM_S_S-586", true},
	{1, 239, 308, "MM_S_S-587", true},
	{1, 240, 309, "MM_S_S-588", true},
	{1, 241, 310, "MM_S_S-589", true},
	{1, 242, 311, "MM_S_S-590", true},

	/* 310 */
	{1, 243, 312, "MM_S_S-591", true},
	{1, 244, 313, "MM_S_S-592", true},
	{1, 245, 314, "MM_S_S-593", true},
	{1, 246, 315, "MM_S_S-594", true},
	{1, 247, 316, "MM_S_S-595", true},
	{1, 248, 317, "MM_S_S-596", true},
	{1, 249, 318, "MM_S_S-597", true},
	{1, 250, 319, "MM_S_S-600", true},
	{1, 251, 320, "MM_S_S-601", true},
	{1, 252, 321, "MM_S_S-602", true},

	/* 320 */
	{1, 253, 322, "MM_S_S-603", true},
	{1, 254, 323, "MM_S_S-604", true},
	{1, 255, 324, "MM_S_S-605", true},
	{2, 0, 325, "MM_S_S-606", true},
	{2, 1, 326, "MM_S_S-607", true},
	{2, 2, 327, "MM_S_S-608", true},
	{2, 3, 328, "MM_S_S-609", true},
	{2, 4, 329, "MM_S_S-610", true},
	{2, 5, 330, "MM_S_S-611", true},
	{2, 6, 331, "MM_S_S-700", true},

	/* 330 */
	{2, 7, 332, "MM_S_S-701", true},
	{2, 8, 333, "MM_S_S-702", true},
	{2, 9, 334, "MM_S_S-703", true},
	{2, 10, 335, "MM_S_S-704", true},
	{2, 11, 336, "MM_S_S-705", true},
	{2, 12, 337, "MM_S_S-706", true},
	{2, 13, 338, "MM_S_S-707", true},
	{2, 14, 339, "MM_S_S-708", true},
	{2, 15, 340, "MM_S_S-709", true},
	{2, 16, 341, "MM_S_S-710", true},

	/* 340 */
	{2, 17, 342, "MM_S_S-711", true},
	{2, 18, 343, "MM_S_S-712", true},
	{2, 19, 344, "MM_S_S-713", true},
	{2, 20, 345, "MM_S_S-714", true},
	{2, 21, 346, "MM_S_S-715", true},
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
	{-1, -1, 390, "Decode_error", false},
	{-1, -1, 391, "Decode_error", true},

	/* 390 */
	{-1, -1, 392, "SRAMROM", true},
	{-1, -1, 393, "MDP_MALI", true},
	{-1, -1, 394, "reserve", false},
	{-1, -1, 395, "MMSYS_MALI", true},
	{-1, -1, 396, "PMIC_WRAP", false},
	{-1, -1, 397, "PMIF1", false},
	{-1, -1, 398, "reserve", false},
	{-1, -1, 399, "reserve", false},
	{-1, -1, 400, "DEVICE_APC_INFRA_AO", false},
	{-1, -1, 401, "DEVICE_APC_INFRA_PDN", false},
};

static struct mtk_device_info mt6789_devices_peri[] = {
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
	{0, 18, 80, "AUDIO_S", true},
	{0, 19, 81, "AUDIO_S-1", true},

	/* 20 */
	{0, 20, 82, "UFS_S", true},
	{0, 21, 83, "UFS_S-1", true},
	{0, 22, 84, "UFS_S-2", true},
	{0, 23, 85, "UFS_S-3", true},
	{0, 24, 87, "SSUSB_S", true},
	{0, 25, 88, "SSUSB_S-1", true},
	{0, 26, 89, "SSUSB_S-2", true},
	{0, 27, 90, "DEBUGSYS_APB_S", true},
	{0, 28, 91, "DRAMC_MD32_S0_APB_S", true},
	{0, 29, 92, "DRAMC_MD32_S0_APB_S-1", true},

	/* 30 */
	{0, 30, 93, "DRAMC_CH0_TOP0_APB_S", true},
	{0, 31, 94, "DRAMC_CH0_TOP1_APB_S", true},
	{0, 32, 95, "DRAMC_CH0_TOP2_APB_S", true},
	{0, 33, 96, "DRAMC_CH0_TOP3_APB_S", true},
	{0, 34, 97, "DRAMC_CH0_TOP4_APB_S", true},
	{0, 35, 98, "DRAMC_CH0_TOP5_APB_S", true},
	{0, 36, 99, "DRAMC_CH0_TOP6_APB_S", true},
	{0, 37, 100, "DRAMC_CH1_TOP0_APB_S", true},
	{0, 38, 101, "DRAMC_CH1_TOP1_APB_S", true},
	{0, 39, 102, "DRAMC_CH1_TOP2_APB_S", true},

	/* 40 */
	{0, 40, 103, "DRAMC_CH1_TOP3_APB_S", true},
	{0, 41, 104, "DRAMC_CH1_TOP4_APB_S", true},
	{0, 42, 105, "DRAMC_CH1_TOP5_APB_S", true},
	{0, 43, 106, "DRAMC_CH1_TOP6_APB_S", true},
	{0, 44, 108, "CCIF2_AP_APB_S", true},
	{0, 45, 109, "CCIF2_MD_APB_S", true},
	{0, 46, 110, "CCIF3_AP_APB_S", true},
	{0, 47, 111, "CCIF3_MD_APB_S", true},
	{0, 48, 112, "CCIF4_AP_APB_S", true},
	{0, 49, 113, "CCIF4_MD_APB_S", true},

	/* 50 */
	{0, 50, 114, "CCIF5_AP_APB_S", true},
	{0, 51, 115, "CCIF5_MD_APB_S", true},
	{0, 52, 116, "SSC_INFRA_APB2_S", true},
	{1, 0, 3, "TINSYS_S", true},
	{1, 1, 4, "TINSYS_S-1", true},
	{1, 2, 5, "TINSYS_S-2", true},
	{1, 3, 6, "TINSYS_S-3", true},
	{1, 4, 7, "TINSYS_S-4", true},
	{1, 5, 8, "TINSYS_S-5", true},
	{1, 6, 9, "TINSYS_S-6", true},

	/* 60 */
	{1, 7, 10, "TINSYS_S-7", true},
	{1, 8, 11, "TINSYS_S-8", true},
	{1, 9, 12, "TINSYS_S-9", true},
	{1, 10, 13, "TINSYS_S-10", true},
	{1, 11, 14, "TINSYS_S-11", true},
	{1, 12, 15, "TINSYS_S-12", true},
	{1, 13, 16, "TINSYS_S-13", true},
	{1, 14, 17, "TINSYS_S-14", true},
	{1, 15, 18, "TINSYS_S-15", true},
	{1, 16, 19, "TINSYS_S-16", true},

	/* 70 */
	{1, 17, 20, "TINSYS_S-17", true},
	{1, 18, 21, "TINSYS_S-18", true},
	{1, 19, 22, "TINSYS_S-19", true},
	{1, 20, 23, "TINSYS_S-20", true},
	{1, 21, 24, "TINSYS_S-21", true},
	{1, 22, 25, "TINSYS_S-22", true},
	{1, 23, 41, "MD_AP_S", true},
	{1, 24, 42, "MD_AP_S-1", true},
	{1, 25, 43, "MD_AP_S-2", true},
	{1, 26, 44, "MD_AP_S-3", true},

	/* 80 */
	{1, 27, 45, "MD_AP_S-4", true},
	{1, 28, 46, "MD_AP_S-5", true},
	{1, 29, 47, "MD_AP_S-6", true},
	{1, 30, 48, "MD_AP_S-7", true},
	{1, 31, 49, "MD_AP_S-8", true},
	{1, 32, 50, "MD_AP_S-9", true},
	{1, 33, 51, "MD_AP_S-10", true},
	{1, 34, 52, "MD_AP_S-11", true},
	{1, 35, 53, "MD_AP_S-12", true},
	{1, 36, 54, "MD_AP_S-13", true},

	/* 90 */
	{1, 37, 55, "MD_AP_S-14", true},
	{1, 38, 56, "MD_AP_S-15", true},
	{1, 39, 57, "MD_AP_S-16", true},
	{1, 40, 58, "MD_AP_S-17", true},
	{1, 41, 59, "MD_AP_S-18", true},
	{1, 42, 60, "MD_AP_S-19", true},
	{1, 43, 61, "MD_AP_S-20", true},
	{1, 44, 62, "MD_AP_S-21", true},
	{1, 45, 63, "MD_AP_S-22", true},
	{1, 46, 64, "MD_AP_S-23", true},

	/* 100 */
	{1, 47, 65, "MD_AP_S-24", true},
	{1, 48, 66, "MD_AP_S-25", true},
	{1, 49, 67, "MD_AP_S-26", true},
	{1, 50, 68, "MD_AP_S-27", true},
	{1, 51, 69, "MD_AP_S-28", true},
	{1, 52, 70, "MD_AP_S-29", true},
	{1, 53, 71, "MD_AP_S-30", true},
	{1, 54, 72, "MD_AP_S-31", true},
	{1, 55, 73, "MD_AP_S-32", true},
	{1, 56, 74, "MD_AP_S-33", true},

	/* 110 */
	{1, 57, 75, "MD_AP_S-34", true},
	{1, 58, 76, "MD_AP_S-35", true},
	{1, 59, 77, "MD_AP_S-36", true},
	{1, 60, 78, "MD_AP_S-37", true},
	{1, 61, 79, "MD_AP_S-38", true},
	{2, 0, 86, "CONN_S", true},
	{-1, -1, 117, "OOB_way_en", true},
	{-1, -1, 118, "OOB_way_en", true},
	{-1, -1, 119, "OOB_way_en", true},
	{-1, -1, 120, "OOB_way_en", true},

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
	{-1, -1, 156, "Decode_error", true},
	{-1, -1, 157, "Decode_error", true},
	{-1, -1, 158, "Decode_error", true},
	{-1, -1, 159, "Decode_error", true},
	{-1, -1, 160, "DEVICE_APC_PERI_AO", false},

	/* 160 */
	{-1, -1, 161, "DEVICE_APC_PERI_PDN", false},
};

static struct mtk_device_info mt6789_devices_peri2[] = {
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
	{-1, -1, 212, "Decode_error", true},
	{-1, -1, 213, "Decode_error", true},
	{-1, -1, 214, "Decode_error", true},
	{-1, -1, 215, "Decode_error", true},
	{-1, -1, 216, "Decode_error", true},
	{-1, -1, 217, "Decode_error", true},
	{-1, -1, 218, "Decode_error", true},
	{-1, -1, 219, "Decode_error", true},
	{-1, -1, 220, "CQ_DMA", false},
	{-1, -1, 221, "EMI", false},

	/* 220 */
	{-1, -1, 222, "EMI_MPU", false},
	{-1, -1, 223, "GCE", false},
	{-1, -1, 224, "AP_DMA", false},
	{-1, -1, 225, "DEVICE_APC_PERI_AO2", false},
	{-1, -1, 226, "DEVICE_APC_PERI_PDN2", false},
};

static struct mtk_device_info mt6789_devices_peri_par[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "MSDC0_S", true},
	{0, 1, 1, "MSDC1_S", true},
	{0, 2, 2, "AUXADC_APB_S", true},
	{0, 3, 3, "UART0_APB_S", true},
	{0, 4, 4, "UART1_APB_S", true},
	{0, 5, 5, "UART2_APB_S", true},
	{0, 6, 6, "SPI0_APB_S", true},
	{0, 7, 7, "PTP_THERM_CTRL_APB_S", true},
	{0, 8, 8, "BTIF_APB_S", true},
	{0, 9, 9, "DISP_PWM_APB_S", true},

	/* 10 */
	{0, 10, 10, "SPI1_APB_S", true},
	{0, 11, 11, "SPI2_APB_S", true},
	{0, 12, 12, "SPI3_APB_S", true},
	{0, 13, 13, "IIC_P2P_REMAP_APB0_S", true},
	{0, 14, 14, "IIC_P2P_REMAP_APB1_S", true},
	{0, 15, 15, "SPI4_APB_S", true},
	{0, 16, 16, "SPI5_APB_S", true},
	{0, 17, 17, "IIC_P2P_REMAP_APB2_S", true},
	{0, 18, 18, "IIC_P2P_REMAP_APB3_S", true},
	{0, 19, 19, "BCRM_PERI_PAR_PDN_APB_S", true},

	/* 20 */
	{0, 20, 20, "DEVICE_APC_PERI_PAR_PDN_APB_S", true},
	{0, 21, 21, "PTP_THERM_CTRL2_APB_S", true},
	{0, 22, 22, "DEVICE_APC_PERI_PAR_AO_APB_S", true},
	{0, 23, 23, "DEBUG_CTRL_PERI_PAR_AO_APB_S", true},
	{0, 24, 24, "BCRM_PERI_PAR_AO_APB_S", true},
	{-1, -1, 25, "OOB_way_en", true},
	{-1, -1, 26, "OOB_way_en", true},
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
	{-1, -1, 52, "Decode_error", true},
	{-1, -1, 53, "Decode_error", true},
	{-1, -1, 54, "Decode_error", true},
	{-1, -1, 55, "reserve", false},
	{-1, -1, 56, "IMP_IIC_WRAP", false},
	{-1, -1, 57, "DEVICE_APC_PERI_PAR_AO", false},
	{-1, -1, 58, "DEVICE_APC_PERI_PAR_PDN", false},
};

enum DEVAPC_VIO_SLAVE_NUM {
	VIO_SLAVE_NUM_INFRA = ARRAY_SIZE(mt6789_devices_infra),
	VIO_SLAVE_NUM_PERI = ARRAY_SIZE(mt6789_devices_peri),
	VIO_SLAVE_NUM_PERI2 = ARRAY_SIZE(mt6789_devices_peri2),
	VIO_SLAVE_NUM_PERI_PAR = ARRAY_SIZE(mt6789_devices_peri_par),
};

#endif /* __DEVAPC_MT6789_H__ */
