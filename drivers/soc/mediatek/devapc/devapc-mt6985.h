/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __DEVAPC_MT6985_H__
#define __DEVAPC_MT6985_H__

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
	SLAVE_TYPE_INFRA1,
	SLAVE_TYPE_PERI_PAR,
	SLAVE_TYPE_VLP,
	SLAVE_TYPE_ADSP,
	SLAVE_TYPE_MMINFRA,
	SLAVE_TYPE_MMUP,
	SLAVE_TYPE_GPU,
	SLAVE_TYPE_NUM,
};

enum DEVAPC_VIO_MASK_STA_NUM {
	VIO_MASK_STA_NUM_INFRA = 14,
	VIO_MASK_STA_NUM_INFRA1 = 8,
	VIO_MASK_STA_NUM_PERI_PAR = 4,
	VIO_MASK_STA_NUM_VLP = 4,
	VIO_MASK_STA_NUM_ADSP = 3,
	VIO_MASK_STA_NUM_MMINFRA = 17,
	VIO_MASK_STA_NUM_MMUP = 2,
	VIO_MASK_STA_NUM_GPU = 1,
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

#define SRAMROM_SLAVE_TYPE	SLAVE_TYPE_INFRA1	/* Infra1 */
#define MM2ND_SLAVE_TYPE	SLAVE_TYPE_NUM		/* No MM2ND */

enum OTHER_TYPES_INDEX {
	SRAMROM_VIO_INDEX = 232,
	CONN_VIO_INDEX = 121, /* starts from 0x18 */
	MDP_VIO_INDEX = 0,
	DISP2_VIO_INDEX = 0,
	MMSYS_VIO_INDEX = 0,
};

enum INFRACFG_MM2ND_VIO_NUM {
	INFRACFG_MM_VIO_STA_NUM = 4,
	INFRACFG_MDP_VIO_STA_NUM = 10,
	INFRACFG_DISP2_VIO_STA_NUM = 4,
};

enum INFRACFG_MM2ND_OFFSET {
	INFRACFG_MM_SEC_VIO0_OFFSET = 0xB30,
	INFRACFG_MDP_SEC_VIO0_OFFSET = 0xB40,
	INFRACFG_DISP2_SEC_VIO0_OFFSET = 0xB70,
};

enum BUSID_LENGTH {
	INFRAAXI_MI_BIT_LENGTH = 18,
	ADSPAXI_MI_BIT_LENGTH = 8,
	MMINFRAAXI_MI_BIT_LENGTH = 19,
};

struct INFRAAXI_ID_INFO {
	const char	*master;
	uint8_t		bit[INFRAAXI_MI_BIT_LENGTH];
};

struct ADSPAXI_ID_INFO {
	const char	*master;
	uint8_t		bit[ADSPAXI_MI_BIT_LENGTH];
};

struct MMINFRAAXI_ID_INFO {
	const char	*master;
	uint8_t		bit[MMINFRAAXI_MI_BIT_LENGTH];
};

enum DEVAPC_IRQ_TYPE {
	IRQ_TYPE_INFRA = 0,
	IRQ_TYPE_PERI,
	IRQ_TYPE_VLP,
	IRQ_TYPE_ADSP,
	IRQ_TYPE_MMINFRA,
	IRQ_TYPE_MMUP,
	IRQ_TYPE_GPU,
	IRQ_TYPE_NUM,
};

enum ADSP_MI_SELECT {
	ADSP_MI13 = 0,
	ADSP_MI15
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
#define INFRA_VIO_ADDR_HIGH			0xFFFFFFFF
#define INFRA_VIO_ADDR_HIGH_START_BIT		0

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
#define SRAM_END_ADDR				(0x1FFFFF)

/* For VLP Bus Parser */
#define VLP_SCP_START_ADDR			(0x1C400000)
#define VLP_SCP_END_ADDR			(0x1C7FFFFF)
#define VLP_INFRA_START				(0x00000000)
#define VLP_INFRA_END				(0x1BFFFFFF)
#define VLP_INFRA_1_START			(0x1D000000)
#define VLP_INFRA_1_END				(0x7FFFFFFFF)

/* For ADSP Bus Parser */
#define ADSP_INFRA_START			(0x00000000)
#define ADSP_INFRA_END				(0x1CFFFFFF)
#define ADSP_INFRA_1_START			(0x1E200000)
#define ADSP_INFRA_1_END			(0x4DFFFFFF)
#define ADSP_OTHER_START			(0x1E000000)
#define ADSP_OTHER_END				(0x1E01FFFF)

/* For MMINFRA Bus Parser */
#define IMG_START_ADDR				(0x15000000)
#define IMG_END_ADDR				(0x1572FFFF)
#define CAM_START_ADDR				(0x1a000000)
#define CAM_END_ADDR				(0x1BFFFFFF)
#define CODEC_START_ADDR			(0x16000000)
#define CODEC_END_ADDR				(0x17FFFFFF)
#define DISP_START_ADDR				(0x14000000)
#define DISP_END_ADDR				(0x143FFFFF)
#define OVL_START_ADDR				(0x14400000)
#define OVL_END_ADDR				(0x149FFFFF)
#define MML_START_ADDR				(0x1F000000)
#define MML_END_ADDR				(0x1FFFFFFF)

/* For GPU Bus Parser */
#define GPU_PD_START				(0xC00000)
#define GPU_PD_END				(0xC620FF)

static const struct mtk_device_info mt6985_devices_infra[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "APMIXEDSYS_APB_S", true},
	{0, 1, 1, "APMIXEDSYS_APB_S-1", true},
	{0, 2, 2, "BCRM_INFRA_AO_APB_S", true},
	{0, 3, 3, "DEVICE_APC_INFRA_AO_APB_S", true},
	{0, 4, 4, "DEBUG_CTRL_INFRA_AO_APB_S", true},
	{0, 5, 5, "INFRACFG_AO_APB_S", true},
	{0, 6, 6, "INFRA_SECURITY_AO_APB_S", true},
	{0, 7, 7, "EFUSE_DEBUG_AO_APB_S", true},
	{0, 8, 8, "PMSR_APB_S", true},
	{0, 9, 9, "RESERVED_APB_S", true},

	/* 10 */
	{0, 10, 10, "TOPCKGEN_APB_S", true},
	{0, 11, 11, "GPIO_APB_S", true},
	{0, 12, 13, "TOP_MISC_APB_S", true},
	{0, 13, 14, "MBIST_AO_APB_S", true},
	{0, 14, 15, "DPMAIF_AO_APB_S", true},
	{0, 15, 16, "TOPCKGEN_INFRA_CFG_APB_S", true},
	{0, 16, 17, "DRM_DEBUG_TOP_APB_S", true},
	{0, 17, 18, "DRAMC_MD32_S0_APB_S", true},
	{0, 18, 19, "DRAMC_MD32_S0_APB_S-1", true},
	{0, 19, 20, "DRAMC_MD32_S1_APB_S", true},

	/* 20 */
	{0, 20, 21, "DRAMC_MD32_S1_APB_S-1", true},
	{0, 21, 22, "DRAMC_MD32_S2_APB_S", true},
	{0, 22, 23, "DRAMC_MD32_S2_APB_S-1", true},
	{0, 23, 24, "DRAMC_MD32_S3_APB_S", true},
	{0, 24, 25, "DRAMC_MD32_S3_APB_S-1", true},
	{0, 25, 26, "DRAMC_CH0_TOP0_APB_S", true},
	{0, 26, 27, "DRAMC_CH0_TOP1_APB_S", true},
	{0, 27, 28, "DRAMC_CH0_TOP2_APB_S", true},
	{0, 28, 29, "DRAMC_CH0_TOP3_APB_S", true},
	{0, 29, 30, "DRAMC_CH0_TOP4_APB_S", true},

	/* 30 */
	{0, 30, 31, "DRAMC_CH0_TOP5_APB_S", true},
	{0, 31, 32, "DRAMC_CH0_TOP6_APB_S", true},
	{0, 32, 33, "DRAMC_CH1_TOP0_APB_S", true},
	{0, 33, 34, "DRAMC_CH1_TOP1_APB_S", true},
	{0, 34, 35, "DRAMC_CH1_TOP2_APB_S", true},
	{0, 35, 36, "DRAMC_CH1_TOP3_APB_S", true},
	{0, 36, 37, "DRAMC_CH1_TOP4_APB_S", true},
	{0, 37, 38, "DRAMC_CH1_TOP5_APB_S", true},
	{0, 38, 39, "DRAMC_CH1_TOP6_APB_S", true},
	{0, 39, 40, "DRAMC_CH2_TOP0_APB_S", true},

	/* 40 */
	{0, 40, 41, "DRAMC_CH2_TOP1_APB_S", true},
	{0, 41, 42, "DRAMC_CH2_TOP2_APB_S", true},
	{0, 42, 43, "DRAMC_CH2_TOP3_APB_S", true},
	{0, 43, 44, "DRAMC_CH2_TOP4_APB_S", true},
	{0, 44, 45, "DRAMC_CH2_TOP5_APB_S", true},
	{0, 45, 46, "DRAMC_CH2_TOP6_APB_S", true},
	{0, 46, 47, "DRAMC_CH3_TOP0_APB_S", true},
	{0, 47, 48, "DRAMC_CH3_TOP1_APB_S", true},
	{0, 48, 49, "DRAMC_CH3_TOP2_APB_S", true},
	{0, 49, 50, "DRAMC_CH3_TOP3_APB_S", true},

	/* 50 */
	{0, 50, 51, "DRAMC_CH3_TOP4_APB_S", true},
	{0, 51, 52, "DRAMC_CH3_TOP5_APB_S", true},
	{0, 52, 53, "DRAMC_CH3_TOP6_APB_S", true},
	{0, 53, 58, "NEMI_RSI_APB_S", true},
	{0, 54, 59, "NEMI_HRE_EMI_APB_S", true},
	{0, 55, 60, "NEMI_HRE_EMI_MPU_APB_S", true},
	{0, 56, 61, "NEMI_HRE_EMI_SLB_APB_S", true},
	{0, 57, 62, "NEMI_SMPU0_APB_S", true},
	{0, 58, 63, "NEMI_SMPU1_APB_S", true},
	{0, 59, 64, "NEMI_SMPU2_APB_S", true},

	/* 60 */
	{0, 60, 65, "NEMI_SLB_APB_S", true},
	{0, 61, 66, "NEMI_HRE_SMPU_APB_S", true},
	{0, 62, 67, "NEMI_SSC_APB0_S", true},
	{0, 63, 68, "NEMI_SSC_APB1_S", true},
	{0, 64, 69, "NEMI_SSC_APB2_S", true},
	{0, 65, 70, "NEMI_APB_S", true},
	{0, 66, 71, "NEMI_MPU_APB_S", true},
	{0, 67, 72, "NEMI_RSV0_PDN_APB_S", true},
	{0, 68, 73, "NEMI_RSV1_PDN_APB_S", true},
	{0, 69, 74, "NEMI_CFG_APB_S", true},

	/* 70 */
	{0, 70, 75, "NEMI_BCRM_PDN_APB_S", true},
	{0, 71, 76, "NEMI_FAKE_ENGINE_1_S", true},
	{0, 72, 77, "NEMI_FAKE_ENGINE_0_S", true},
	{0, 73, 78, "SEMI_RSI_APB_S", true},
	{0, 74, 79, "SEMI_HRE_EMI_APB_S", true},
	{0, 75, 80, "SEMI_HRE_EMI_MPU_APB_S", true},
	{0, 76, 81, "SEMI_HRE_EMI_SLB_APB_S", true},
	{0, 77, 82, "SEMI_SMPU0_APB_S", true},
	{0, 78, 83, "SEMI_SMPU1_APB_S", true},
	{0, 79, 84, "SEMI_SMPU2_APB_S", true},

	/* 80 */
	{0, 80, 85, "SEMI_SLB_APB_S", true},
	{0, 81, 86, "SEMI_HRE_SMPU_APB_S", true},
	{0, 82, 87, "SEMI_SSC_APB0_S", true},
	{0, 83, 88, "SEMI_SSC_APB1_S", true},
	{0, 84, 89, "SEMI_SSC_APB2_S", true},
	{0, 85, 90, "SEMI_APB_S", true},
	{0, 86, 91, "SEMI_MPU_APB_S", true},
	{0, 87, 92, "SEMI_RSV0_PDN_APB_S", true},
	{0, 88, 93, "SEMI_RSV1_PDN_APB_S", true},
	{0, 89, 94, "SEMI_CFG_APB_S", true},

	/* 90 */
	{0, 90, 95, "SEMI_BCRM_PDN_APB_S", true},
	{0, 91, 96, "SEMI_FAKE_ENGINE_1_S", true},
	{0, 92, 97, "SEMI_FAKE_ENGINE_0_S", true},
	{0, 93, 98, "NEMI_DEBUG_CTRL_AO_APB_S", true},
	{0, 94, 99, "NEMI_BCRM_AO_APB_S", true},
	{0, 95, 100, "NEMI_CFG_AO_APB_S", true},
	{0, 96, 101, "NEMI_MBIST_APB_S", true},
	{0, 97, 102, "NEMI_RSV0_AO_APB_S", true},
	{0, 98, 103, "NEMI_RSV1_AO_APB_S", true},
	{0, 99, 104, "NEMI_RSV2_AO_APB_S", true},

	/* 100 */
	{0, 100, 105, "SEMI_DEBUG_CTRL_AO_APB_S", true},
	{0, 101, 106, "SEMI_BCRM_AO_APB_S", true},
	{0, 102, 107, "SEMI_CFG_AO_APB_S", true},
	{0, 103, 108, "SEMI_MBIST_APB_S", true},
	{0, 104, 109, "SEMI_RSV0_AO_APB_S", true},
	{0, 105, 110, "SEMI_RSV1_AO_APB_S", true},
	{0, 106, 111, "SEMI_RSV2_AO_APB_S", true},
	{0, 107, 112, "SSR_APB0_S", true},
	{0, 108, 113, "SSR_APB1_S", true},
	{0, 109, 114, "SSR_APB2_S", true},

	/* 110 */
	{0, 110, 115, "SSR_APB3_S", true},
	{0, 111, 116, "SSR_APB4_S", true},
	{0, 112, 117, "SSR_APB5_S", true},
	{0, 113, 118, "SSR_APB6_S", true},
	{0, 114, 119, "SSR_APB7_S", true},
	{0, 115, 120, "SSR_APB8_S", true},
	{0, 116, 121, "SSR_APB9_S", true},
	{0, 117, 122, "SSR_APB10_S", true},
	{0, 118, 123, "SSR_APB11_S", true},
	{0, 119, 124, "SSR_APB12_S", true},

	/* 120 */
	{0, 120, 125, "SSR_APB13_S", true},
	{0, 121, 126, "SSR_APB14_S", true},
	{0, 122, 127, "SSR_APB15_S", true},
	{0, 123, 128, "DEVICE_APC_INFRA_PDN_APB_S", true},
	{0, 124, 129, "DEBUG_TRACKER_APB_S", true},
	{0, 125, 130, "BCRM_INFRA_PDN_APB_S", true},
	{0, 126, 131, "BND_EAST_APB0_S", true},
	{0, 127, 132, "BND_EAST_APB1_S", true},
	{0, 128, 133, "BND_EAST_APB2_S", true},
	{0, 129, 134, "BND_EAST_APB3_S", true},

	/* 130 */
	{0, 130, 135, "BND_EAST_APB4_S", true},
	{0, 131, 136, "BND_EAST_APB5_S", true},
	{0, 132, 137, "BND_EAST_APB6_S", true},
	{0, 133, 138, "BND_EAST_APB7_S", true},
	{0, 134, 139, "BND_EAST_APB8_S", true},
	{0, 135, 140, "BND_EAST_APB9_S", true},
	{0, 136, 141, "BND_EAST_APB10_S", true},
	{0, 137, 142, "BND_EAST_APB11_S", true},
	{0, 138, 143, "BND_EAST_APB12_S", true},
	{0, 139, 144, "BND_EAST_APB13_S", true},

	/* 140 */
	{0, 140, 145, "BND_EAST_APB14_S", true},
	{0, 141, 146, "BND_EAST_APB15_S", true},
	{0, 142, 147, "BND_WEST_APB0_S", true},
	{0, 143, 148, "BND_WEST_APB1_S", true},
	{0, 144, 149, "BND_WEST_APB2_S", true},
	{0, 145, 150, "BND_WEST_APB3_S", true},
	{0, 146, 151, "BND_WEST_APB4_S", true},
	{0, 147, 152, "BND_WEST_APB5_S", true},
	{0, 148, 153, "BND_WEST_APB6_S", true},
	{0, 149, 154, "BND_WEST_APB7_S", true},

	/* 150 */
	{0, 150, 155, "BND_WEST_APB8_S", true},
	{0, 151, 156, "BND_WEST_APB9_S", true},
	{0, 152, 157, "BND_WEST_APB10_S", true},
	{0, 153, 158, "BND_WEST_APB11_S", true},
	{0, 154, 159, "BND_WEST_APB12_S", true},
	{0, 155, 160, "BND_WEST_APB13_S", true},
	{0, 156, 161, "BND_WEST_APB14_S", true},
	{0, 157, 162, "BND_WEST_APB15_S", true},
	{0, 158, 163, "BND_NORTH_APB0_S", true},
	{0, 159, 164, "BND_NORTH_APB1_S", true},

	/* 160 */
	{0, 160, 165, "BND_NORTH_APB2_S", true},
	{0, 161, 166, "BND_NORTH_APB3_S", true},
	{0, 162, 167, "BND_NORTH_APB4_S", true},
	{0, 163, 168, "BND_NORTH_APB5_S", true},
	{0, 164, 169, "BND_NORTH_APB6_S", true},
	{0, 165, 170, "BND_NORTH_APB7_S", true},
	{0, 166, 171, "BND_NORTH_APB8_S", true},
	{0, 167, 172, "BND_NORTH_APB9_S", true},
	{0, 168, 173, "BND_NORTH_APB10_S", true},
	{0, 169, 174, "BND_NORTH_APB11_S", true},

	/* 170 */
	{0, 170, 175, "BND_NORTH_APB12_S", true},
	{0, 171, 176, "BND_NORTH_APB13_S", true},
	{0, 172, 177, "BND_NORTH_APB14_S", true},
	{0, 173, 178, "BND_NORTH_APB15_S", true},
	{0, 174, 179, "BND_SOUTH_APB0_S", true},
	{0, 175, 180, "BND_SOUTH_APB1_S", true},
	{0, 176, 181, "BND_SOUTH_APB2_S", true},
	{0, 177, 182, "BND_SOUTH_APB3_S", true},
	{0, 178, 183, "BND_SOUTH_APB4_S", true},
	{0, 179, 184, "BND_SOUTH_APB5_S", true},

	/* 180 */
	{0, 180, 185, "BND_SOUTH_APB6_S", true},
	{0, 181, 186, "BND_SOUTH_APB7_S", true},
	{0, 182, 187, "BND_SOUTH_APB8_S", true},
	{0, 183, 188, "BND_SOUTH_APB9_S", true},
	{0, 184, 189, "BND_SOUTH_APB10_S", true},
	{0, 185, 190, "BND_SOUTH_APB11_S", true},
	{0, 186, 191, "BND_SOUTH_APB12_S", true},
	{0, 187, 192, "BND_SOUTH_APB13_S", true},
	{0, 188, 193, "BND_SOUTH_APB14_S", true},
	{0, 189, 194, "BND_SOUTH_APB15_S", true},

	/* 190 */
	{0, 190, 195, "BND_EAST_NORTH_APB0_S", true},
	{0, 191, 196, "BND_EAST_NORTH_APB1_S", true},
	{0, 192, 197, "BND_EAST_NORTH_APB2_S", true},
	{0, 193, 198, "BND_EAST_NORTH_APB3_S", true},
	{0, 194, 199, "BND_EAST_NORTH_APB4_S", true},
	{0, 195, 200, "BND_EAST_NORTH_APB5_S", true},
	{0, 196, 201, "BND_EAST_NORTH_APB6_S", true},
	{0, 197, 202, "BND_EAST_NORTH_APB7_S", true},
	{0, 198, 203, "BND_EAST_NORTH_APB8_S", true},
	{0, 199, 204, "BND_EAST_NORTH_APB9_S", true},

	/* 200 */
	{0, 200, 205, "BND_EAST_NORTH_APB10_S", true},
	{0, 201, 206, "BND_EAST_NORTH_APB11_S", true},
	{0, 202, 207, "BND_EAST_NORTH_APB12_S", true},
	{0, 203, 208, "BND_EAST_NORTH_APB13_S", true},
	{0, 204, 209, "BND_EAST_NORTH_APB14_S", true},
	{0, 205, 210, "BND_EAST_NORTH_APB15_S", true},

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
	{-1, -1, 235, "OOB_way_en", false},
	{-1, -1, 236, "OOB_way_en", true},
	{-1, -1, 237, "OOB_way_en", true},
	{-1, -1, 238, "OOB_way_en", false},
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
	{-1, -1, 313, "OOB_way_en", true},
	{-1, -1, 314, "OOB_way_en", true},
	{-1, -1, 315, "OOB_way_en", true},
	{-1, -1, 316, "OOB_way_en", true},
	{-1, -1, 317, "OOB_way_en", true},
	{-1, -1, 318, "OOB_way_en", true},
	{-1, -1, 319, "OOB_way_en", true},

	{-1, -1, 320, "OOB_way_en", true},
	{-1, -1, 321, "OOB_way_en", true},
	{-1, -1, 322, "OOB_way_en", true},
	{-1, -1, 323, "OOB_way_en", true},
	{-1, -1, 324, "OOB_way_en", true},
	{-1, -1, 325, "OOB_way_en", true},
	{-1, -1, 326, "OOB_way_en", true},
	{-1, -1, 327, "OOB_way_en", true},
	{-1, -1, 328, "OOB_way_en", true},
	{-1, -1, 329, "OOB_way_en", true},

	{-1, -1, 330, "OOB_way_en", true},
	{-1, -1, 331, "OOB_way_en", true},
	{-1, -1, 332, "OOB_way_en", true},
	{-1, -1, 333, "OOB_way_en", true},
	{-1, -1, 334, "OOB_way_en", true},
	{-1, -1, 335, "OOB_way_en", false},
	{-1, -1, 336, "OOB_way_en", true},
	{-1, -1, 337, "OOB_way_en", true},
	{-1, -1, 338, "OOB_way_en", false},
	{-1, -1, 339, "OOB_way_en", true},

	{-1, -1, 340, "OOB_way_en", true},
	{-1, -1, 341, "OOB_way_en", true},
	{-1, -1, 342, "OOB_way_en", true},
	{-1, -1, 343, "OOB_way_en", true},
	{-1, -1, 344, "OOB_way_en", true},
	{-1, -1, 345, "OOB_way_en", true},
	{-1, -1, 346, "OOB_way_en", true},
	{-1, -1, 347, "OOB_way_en", true},
	{-1, -1, 348, "OOB_way_en", true},
	{-1, -1, 349, "OOB_way_en", true},

	{-1, -1, 350, "OOB_way_en", true},
	{-1, -1, 351, "OOB_way_en", true},
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
	{-1, -1, 392, "OOB_way_en", true},
	{-1, -1, 393, "OOB_way_en", true},
	{-1, -1, 394, "OOB_way_en", true},
	{-1, -1, 395, "OOB_way_en", true},
	{-1, -1, 396, "OOB_way_en", true},
	{-1, -1, 397, "OOB_way_en", true},
	{-1, -1, 398, "OOB_way_en", true},
	{-1, -1, 399, "OOB_way_en", true},

	{-1, -1, 400, "OOB_way_en", true},
	{-1, -1, 401, "OOB_way_en", true},
	{-1, -1, 402, "OOB_way_en", true},
	{-1, -1, 403, "OOB_way_en", true},
	{-1, -1, 404, "OOB_way_en", true},
	{-1, -1, 405, "OOB_way_en", true},
	{-1, -1, 406, "OOB_way_en", true},
	{-1, -1, 407, "OOB_way_en", true},
	{-1, -1, 408, "OOB_way_en", true},
	{-1, -1, 409, "OOB_way_en", true},

	{-1, -1, 410, "OOB_way_en", true},
	{-1, -1, 411, "OOB_way_en", true},
	{-1, -1, 412, "OOB_way_en", true},
	{-1, -1, 413, "OOB_way_en", true},
	{-1, -1, 414, "OOB_way_en", true},
	{-1, -1, 415, "OOB_way_en", true},
	{-1, -1, 416, "OOB_way_en", true},
	{-1, -1, 417, "OOB_way_en", true},
	{-1, -1, 418, "OOB_way_en", true},
	{-1, -1, 419, "OOB_way_en", true},

	{-1, -1, 420, "OOB_way_en", true},
	{-1, -1, 421, "OOB_way_en", true},
	{-1, -1, 422, "OOB_way_en", true},
	{-1, -1, 423, "OOB_way_en", true},

	{-1, -1, 424, "Decode_error", true},
	{-1, -1, 425, "Decode_error", true},
	{-1, -1, 426, "Decode_error", true},
	{-1, -1, 427, "Decode_error", true},
	{-1, -1, 428, "Decode_error", true},
	{-1, -1, 429, "Decode_error", true},

	{-1, -1, 430, "Decode_error", true},
	{-1, -1, 431, "Decode_error", true},
	{-1, -1, 432, "Decode_error", true},
	{-1, -1, 433, "Decode_error", true},
	{-1, -1, 434, "Decode_error", true},
	{-1, -1, 435, "Decode_error", true},

	{-1, -1, 436, "NA", false},
	{-1, -1, 437, "NA", false},
	{-1, -1, 438, "NA", false},
	{-1, -1, 439, "North EMI", false},
	{-1, -1, 440, "North EMI MPU", false},
	{-1, -1, 441, "South EMI", false},
	{-1, -1, 442, "South EMI MPU", false},
	{-1, -1, 443, "DEVICE_APC_INFRA_AO", false},
	{-1, -1, 444, "DEVICE_APC_INFRA_PDN", false},
};

static const struct mtk_device_info mt6985_devices_infra1[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "BCRM_INFRA_AO1_APB_S", true},
	{0, 1, 1, "DEVICE_APC_INFRA_AO1_APB_S", true},
	{0, 2, 2, "DEBUG_CTRL_INFRA_AO1_APB_S", true},
	{0, 3, 3, "IFRBUS_AO_APB_S", true},
	{0, 4, 4, "DPMAIF_PDN_APB_S", true},
	{0, 5, 5, "DPMAIF_PDN_APB_S-1", true},
	{0, 6, 6, "DPMAIF_PDN_APB_S-2", true},
	{0, 7, 7, "DPMAIF_PDN_APB_S-3", true},
	{0, 8, 8, "DEBUG_TRACKER_APB1_S", true},
	{0, 9, 9, "BCRM_INFRA_PDN1_APB_S", true},

	/* 10 */
	{0, 10, 10, "DEVICE_APC_INFRA_PDN1_APB_S", true},
	{0, 11, 11, "INFRACFG_PDN_APB_S", true},
	{0, 12, 12, "ERSI0_APB_S", true},
	{0, 13, 13, "ERSI1_APB_S", true},
	{0, 14, 14, "ERSI2_APB_S", true},
	{0, 15, 15, "ERSI3_APB_S", true},
	{0, 16, 16, "ERSI4_APB_S", true},
	{0, 17, 17, "M4_IOMMU0_APB_S", true},
	{0, 18, 18, "M4_IOMMU1_APB_S", true},
	{0, 19, 19, "M4_IOMMU2_APB_S", true},

	/* 20 */
	{0, 20, 20, "M4_IOMMU3_APB_S", true},
	{0, 21, 21, "M4_IOMMU4_APB_S", true},
	{0, 22, 22, "M6_IOMMU0_APB_S", true},
	{0, 23, 23, "M6_IOMMU1_APB_S", true},
	{0, 24, 24, "M6_IOMMU2_APB_S", true},
	{0, 25, 25, "M6_IOMMU3_APB_S", true},
	{0, 26, 26, "M6_IOMMU4_APB_S", true},
	{0, 27, 27, "M7_IOMMU0_APB_S", true},
	{0, 28, 28, "M7_IOMMU1_APB_S", true},
	{0, 29, 29, "M7_IOMMU2_APB_S", true},

	/* 30 */
	{0, 30, 30, "M7_IOMMU3_APB_S", true},
	{0, 31, 31, "M7_IOMMU4_APB_S", true},
	{0, 32, 32, "PTP_THERM_CTRL_APB_S", true},
	{0, 33, 33, "PTP_THERM_CTRL2_APB_S", true},
	{0, 34, 34, "SYS_CIRQ_APB_S", true},
	{0, 35, 35, "CCIF0_AP_APB_S", true},
	{0, 36, 36, "CCIF0_MD_APB_S", true},
	{0, 37, 37, "CCIF1_AP_APB_S", true},
	{0, 38, 38, "CCIF1_MD_APB_S", true},
	{0, 39, 39, "IFRBUS_PDN_APB_S", true},

	/* 40 */
	{0, 40, 40, "DEBUGSYS_APB_S", true},
	{0, 41, 41, "CQ_DMA_APB_S", true},
	{0, 42, 42, "DCM_APB_S", true},
	{0, 43, 43, "SRAMROM_APB_S", true},
	{0, 44, 44, "RESERVED_DVFS_PROC_APB_S", true},
	{0, 45, 46, "CCIF2_AP_APB_S", true},
	{0, 46, 47, "CCIF2_MD_APB_S", true},
	{0, 47, 48, "CCIF3_AP_APB_S", true},
	{0, 48, 49, "CCIF3_MD_APB_S", true},
	{0, 49, 50, "CCIF4_AP_APB_S", true},

	/* 50 */
	{0, 50, 51, "CCIF4_MD_APB_S", true},
	{0, 51, 52, "CCIF5_AP_APB_S", true},
	{0, 52, 53, "CCIF5_MD_APB_S", true},
	{0, 53, 54, "HWCCF_APB_S", true},
	{0, 54, 55, "INFRA_BUS_HRE_APB_S", true},
	{0, 55, 56, "IPI_APB_S", true},
	{0, 56, 57, "APU_S_S", true},
	{0, 57, 58, "APU_S_S-1", true},
	{0, 58, 59, "APU_S_S-2", true},
	{0, 59, 60, "APU_S_S-3", true},

	/* 60 */
	{0, 60, 61, "APU_S_S-4", true},
	{0, 61, 62, "APU_S_S-5", true},
	{0, 62, 63, "APU_S_S-6", true},
	{0, 63, 108, "INFRA2PERI_S", true},
	{0, 64, 109, "INFRA2PERI_S-1", true},
	{0, 65, 110, "INFRA2PERI_S-2", true},
	{0, 66, 111, "INFRA2MM_S", true},
	{0, 67, 112, "INFRA2MM_S-1", true},
	{0, 68, 113, "INFRA2MM_S-2", true},
	{0, 69, 114, "INFRA2MM_S-3", true},

	/* 70 */
	{0, 70, 115, "L3C_S", true},
	{0, 71, 116, "L3C_S-1", true},
	{0, 72, 117, "L3C_S-20", true},
	{0, 73, 118, "L3C_S-21", true},
	{0, 74, 119, "L3C_S-22", true},
	{0, 75, 120, "L3C_S-23", true},
	{0, 76, 121, "L3C_S-24", true},
	{0, 77, 122, "L3C_S-25", true},
	{0, 78, 123, "L3C_S-26", true},
	{0, 79, 124, "L3C_S-27", true},

	/* 80 */
	{0, 80, 125, "L3C_S-28", true},
	{0, 81, 126, "L3C_S-29", true},
	{0, 82, 127, "L3C_S-30", true},
	{0, 83, 128, "L3C_S-31", true},
	{0, 84, 129, "L3C_S-32", true},
	{0, 85, 130, "L3C_S-33", true},
	{0, 86, 131, "L3C_S-34", true},
	{0, 87, 132, "L3C_S-35", true},
	{0, 88, 133, "L3C_S-36", true},
	{0, 89, 134, "L3C_S-37", true},

	/* 90 */
	{0, 90, 135, "L3C_S-38", true},
	{0, 91, 136, "L3C_S-39", true},
	{0, 92, 137, "L3C_S-40", true},
	{0, 93, 138, "L3C_S-41", true},
	{0, 94, 139, "L3C_S-42", true},
	{0, 95, 140, "L3C_S-43", true},
	{0, 96, 141, "L3C_S-44", true},
	{0, 97, 143, "CONN_S", true},
	{0, 98, 145, "VLPSYS_S", true},
	{0, 99, 151, "MFG_S_S", true},

	/* 100 */
	{1, 0, 64, "ADSPSYS_S", true},
	{1, 1, 65, "MD_AP_S", true},
	{1, 2, 66, "MD_AP_S-1", true},
	{1, 3, 67, "MD_AP_S-2", true},
	{1, 4, 68, "MD_AP_S-3", true},
	{1, 5, 69, "MD_AP_S-4", true},
	{1, 6, 70, "MD_AP_S-5", true},
	{1, 7, 71, "MD_AP_S-6", true},
	{1, 8, 72, "MD_AP_S-7", true},
	{1, 9, 73, "MD_AP_S-8", true},

	/* 110 */
	{1, 10, 74, "MD_AP_S-9", true},
	{1, 11, 75, "MD_AP_S-10", true},
	{1, 12, 76, "MD_AP_S-11", true},
	{1, 13, 77, "MD_AP_S-12", true},
	{1, 14, 78, "MD_AP_S-13", true},
	{1, 15, 79, "MD_AP_S-14", true},
	{1, 16, 80, "MD_AP_S-15", true},
	{1, 17, 81, "MD_AP_S-16", true},
	{1, 18, 82, "MD_AP_S-17", true},
	{1, 19, 83, "MD_AP_S-18", true},

	/* 120 */
	{1, 20, 84, "MD_AP_S-19", true},
	{1, 21, 85, "MD_AP_S-20", true},
	{1, 22, 86, "MD_AP_S-21", true},
	{1, 23, 87, "MD_AP_S-22", true},
	{1, 24, 88, "MD_AP_S-23", true},
	{1, 25, 89, "MD_AP_S-24", true},
	{1, 26, 90, "MD_AP_S-25", true},
	{1, 27, 91, "MD_AP_S-26", true},
	{1, 28, 92, "MD_AP_S-27", true},
	{1, 29, 93, "MD_AP_S-28", true},

	/* 130 */
	{1, 30, 94, "MD_AP_S-29", true},
	{1, 31, 95, "MD_AP_S-30", true},
	{1, 32, 96, "MD_AP_S-31", true},
	{1, 33, 97, "MD_AP_S-32", true},
	{1, 34, 98, "MD_AP_S-33", true},
	{1, 35, 99, "MD_AP_S-34", true},
	{1, 36, 100, "MD_AP_S-35", true},
	{1, 37, 101, "MD_AP_S-36", true},
	{1, 38, 102, "MD_AP_S-37", true},
	{1, 39, 103, "MD_AP_S-38", true},

	/* 140 */
	{1, 40, 104, "MD_AP_S-39", true},
	{1, 41, 105, "MD_AP_S-40", true},
	{1, 42, 106, "MD_AP_S-41", true},
	{1, 43, 107, "MD_AP_S-42", true},

	{-1, -1, 153, "OOB_way_en", true},
	{-1, -1, 154, "OOB_way_en", true},
	{-1, -1, 155, "OOB_way_en", true},
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

	{-1, -1, 227, "Decode_error", true},
	{-1, -1, 228, "Decode_error", true},
	{-1, -1, 229, "Decode_error", true},
	{-1, -1, 230, "Decode_error", true},
	{-1, -1, 231, "Decode_error", false},

	{-1, -1, 232, "SRAMROM", true},
	{-1, -1, 233, "CQDMA", false},
	{-1, -1, 234, "DEVICE_APC_INFRA_AO1", false},
	{-1, -1, 235, "DEVICE_APC_INFRA_PDN1", false},
};

static const struct mtk_device_info mt6985_devices_peri_par[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "UART0_APB_S", true},
	{0, 1, 1, "UART1_APB_S", true},
	{0, 2, 2, "UART2_APB_S", true},
	{0, 3, 3, "UART3_APB_S", true},
	{0, 4, 4, "UARTHUB_APB_S", true},
	{0, 5, 5, "PWM_PERI_APB_S", true},
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
	{0, 18, 18, "BCRM_PERI_PAR_PDN_APB_S", true},
	{0, 19, 19, "IIC_P2P_REMAP_APB_S", true},

	/* 20 */
	{0, 20, 20, "APDMA_APB_S", true},
	{0, 21, 21, "SSUSB_S", true},
	{0, 22, 22, "SSUSB_S-1", true},
	{0, 23, 23, "SSUSB_S-2", true},
	{0, 24, 24, "USB_S-1", true},
	{0, 25, 25, "USB_S-2", true},
	{0, 26, 26, "MSDC1_S-1", true},
	{0, 27, 27, "MSDC1_S-2", true},
	{0, 28, 28, "MSDC2_S-1", true},
	{0, 29, 29, "MSDC2_S-2", true},

	/* 30 */
	{0, 30, 30, "AUDIO_S-1", true},
	{0, 31, 31, "AUDIO_S-2", true},
	{0, 32, 32, "PCIE_S", true},
	{0, 33, 33, "PCIE_S-1", true},
	{0, 34, 34, "PCIE_S-2", true},
	{0, 35, 35, "PCIE_S-3", true},
	{0, 36, 36, "PCIE_S-4", true},
	{0, 37, 37, "PCIE_S-5", true},
	{0, 38, 38, "PCIE_S-6", true},
	{0, 39, 39, "PCIE_S-7", true},

	/* 40 */
	{0, 40, 40, "PCIE_S-8", true},
	{0, 41, 41, "PCIE_S-9", true},
	{0, 42, 42, "PCIE_S-10", true},
	{0, 43, 43, "PCIE_S-11", true},
	{0, 44, 44, "PCIE_S-12", true},
	{0, 45, 45, "PCIE_S-13", true},
	{0, 46, 46, "PCIE_S-14", true},
	{0, 47, 47, "PCIE_S-15", true},
	{0, 48, 48, "PCIE_S-16", true},
	{0, 49, 49, "PCIE_S-17", true},

	/* 50 */
	{0, 50, 50, "PCIE_S-18", true},
	{0, 51, 51, "PCIE_S-19", true},
	{0, 52, 52, "PCIE_S-20", true},
	{0, 53, 53, "PCIE_S-21", true},
	{0, 54, 55, "NOR_AXI_S", true},
	{0, 55, 58, "MBIST_AO_APB_S", true},
	{0, 56, 59, "BCRM_PERI_PAR_AO_APB_S", true},
	{0, 57, 60, "PERICFG_AO_APB_S", true},
	{0, 58, 61, "DEBUG_CTRL_PERI_PAR_AO_APB_S", true},
	{0, 59, 62, "DEVICE_APC_PERI_PAR_AO_APB_S", true},

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
	{-1, -1, 88, "OOB_way_en", true},
	{-1, -1, 89, "OOB_way_en", true},

	{-1, -1, 90, "OOB_way_en", true},
	{-1, -1, 91, "OOB_way_en", true},
	{-1, -1, 92, "OOB_way_en", true},
	{-1, -1, 93, "OOB_way_en", true},
	{-1, -1, 94, "OOB_way_en", true},
	{-1, -1, 95, "OOB_way_en", true},

	{-1, -1, 96, "Decode_error", true},
	{-1, -1, 97, "Decode_error", true},
	{-1, -1, 98, "Decode_error", true},

	{-1, -1, 99, "APDMA", false},
	{-1, -1, 100, "IIC_P2P_REMAP", false},
	{-1, -1, 101, "DEVICE_APC_PERI_AO", false},
	{-1, -1, 102, "DEVICE_APC_PERI_PDN", false},
};

static const struct mtk_device_info mt6985_devices_vlp[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "INFRA_S0_S", true},
	{0, 1, 1, "INFRA_S0_S-1", true},
	{0, 2, 2, "INFRA_S0_S-2", true},
	{0, 3, 3, "SCP_S", true},
	{0, 4, 4, "SCP_S-1", true},
	{0, 5, 5, "SCP_S-2", true},
	{0, 6, 6, "SCP_S-3", true},
	{0, 7, 7, "SCP_S-4", true},
	{0, 8, 8, "SCP_S-5", true},
	{0, 9, 9, "SPM_APB_S", true},

	/* 10 */
	{0, 10, 10, "SPM_APB_S-1", true},
	{0, 11, 11, "SPM_APB_S-2", true},
	{0, 12, 12, "SPM_APB_S-3", true},
	{0, 13, 13, "SPM_APB_S-4", true},
	{0, 14, 14, "SPM_APB_S-5", true},
	{0, 15, 15, "SPM_APB_S-6", true},
	{0, 16, 16, "SPM_APB_S-7", true},
	{0, 17, 17, "SPM_APB_S-8", true},
	{0, 18, 18, "SPM_APB_S-9", true},
	{0, 19, 19, "SPM_APB_S-10", true},

	/* 20 */
	{0, 20, 20, "SPM_APB_S-11", true},
	{0, 21, 21, "SPM_APB_S-12", true},
	{0, 22, 22, "SPM_APB_S-13", true},
	{0, 23, 23, "SSPM_S", true},
	{0, 24, 24, "SSPM_S-1", true},
	{0, 25, 25, "SSPM_S-2", true},
	{0, 26, 26, "SSPM_S-3", true},
	{0, 27, 27, "SSPM_S-4", true},
	{0, 28, 28, "SSPM_S-5", true},
	{0, 29, 29, "SSPM_S-6", true},

	/* 30 */
	{0, 30, 30, "SSPM_S-7", true},
	{0, 31, 31, "SSPM_S-8", true},
	{0, 32, 32, "SSPM_S-9", true},
	{0, 33, 33, "SSPM_S-10", true},
	{0, 34, 34, "SSPM_S-11", true},
	{0, 35, 35, "SSPM_S-12", true},
	{0, 36, 36, "VLPCFG_AO_APB_S", true},
	{0, 37, 38, "SRCLKEN_RC_APB_S", true},
	{0, 38, 39, "TOPRGU_APB_S", true},
	{0, 39, 40, "APXGPT_APB_S", true},

	/* 40 */
	{0, 40, 41, "KP_APB_S", true},
	{0, 41, 42, "DVFSRC_APB_S", true},
	{0, 42, 43, "MBIST_APB_S", true},
	{0, 43, 44, "SYS_TIMER_APB_S", true},
	{0, 44, 45, "VLP_CKSYS_APB_S", true},
	{0, 45, 46, "PMIF1_APB_S", true},
	{0, 46, 48, "PWM_VLP_APB_S", true},
	{0, 47, 49, "SEJ_APB_S", true},
	{0, 48, 50, "AES_TOP0_APB_S", true},
	{0, 49, 51, "SECURITY_AO_APB_S", true},

	/* 50 */
	{0, 50, 52, "DEVICE_APC_VLP_AO_APB_S", true},
	{0, 51, 53, "DEVICE_APC_VLP_APB_S", true},
	{0, 52, 54, "BCRM_VLP_AO_APB_S", true},
	{0, 53, 55, "DEBUG_CTRL_VLP_AO_APB_S", true},
	{0, 54, 56, "VLPCFG_APB_S", true},
	{0, 55, 57, "SPMI_M_MST_APB_S", true},
	{0, 56, 58, "EFUSE_DEBUG_AO_APB_S", true},
	{0, 57, 59, "MD_BUCK_CTRL_SEQUENCER_APB_S", true},
	{0, 58, 60, "AP_CIRQ_EINT_APB_S", true},
	{0, 59, 61, "SRAMRC_APB_S", true},

	/* 60 */
	{0, 60, 62, "TRNG_APB_S", true},
	{0, 61, 63, "DX_CC_APB_S", true},
	{0, 62, 64, "DPSW_CTRL_APB_S", true},
	{0, 63, 65, "DPSW_CENTRAL_CTRL_APB_S", true},
	{0, 64, 66, "IPS_APB_S", true},

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
	{-1, -1, 88, "OOB_way_en", true},
	{-1, -1, 89, "OOB_way_en", true},

	{-1, -1, 90, "OOB_way_en", true},
	{-1, -1, 91, "OOB_way_en", true},
	{-1, -1, 92, "OOB_way_en", true},
	{-1, -1, 93, "OOB_way_en", true},
	{-1, -1, 94, "OOB_way_en", true},
	{-1, -1, 95, "OOB_way_en", true},
	{-1, -1, 96, "OOB_way_en", true},
	{-1, -1, 97, "OOB_way_en", true},
	{-1, -1, 98, "OOB_way_en", true},
	{-1, -1, 99, "OOB_way_en", true},

	{-1, -1, 100, "OOB_way_en", true},

	{-1, -1, 101, "Decode_error", true},

	{-1, -1, 102, "PMIF1", false},
	{-1, -1, 103, "DEVICE_APC_VLP_AO", false},
	{-1, -1, 104, "DEVICE_APC_VLP_PDN", false},
};

static const struct mtk_device_info mt6985_devices_adsp[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "INFRA_S", true},
	{0, 1, 1, "INFRA_S-1", true},
	{0, 2, 2, "INFRA_S-2", true},
	{0, 3, 3, "EMI_S", true},
	{0, 4, 4, "DSP1_S", true},
	{0, 5, 5, "DSP1_S-1", true},
	{0, 6, 6, "DSP2_S", true},
	{0, 7, 7, "DSP2_S-1", true},
	{0, 8, 8, "F_SRAM_S", true},
	{0, 9, 9, "H_SRAM_S", true},

	/* 10 */
	{0, 10, 10, "AFE_S", true},
	{0, 11, 12, "DSPCFG_0_S", true},
	{0, 12, 13, "DSPCKCTL_S", true},
	{0, 13, 14, "DMA_0_CFG_S", true},
	{0, 14, 15, "DMA_1_CFG_S", true},
	{0, 15, 16, "DMA_AXI_CFG_S", true},
	{0, 16, 17, "DSPCFG_SEC_S", true},
	{0, 17, 18, "DSPMBOX_0_S", true},
	{0, 18, 19, "DSPMBOX_1_S", true},
	{0, 19, 20, "DSPMBOX_2_S", true},

	/* 20 */
	{0, 20, 21, "DSPMBOX_3_S", true},
	{0, 21, 22, "DSPMBOX_4_S", true},
	{0, 22, 23, "DSP_TIMER_0_S", true},
	{0, 23, 24, "DSP_TIMER_1_S", true},
	{0, 24, 25, "DSP_UART_0_S", true},
	{0, 25, 26, "DSP_UART_1_S", true},
	{0, 26, 27, "ADSP_BUSCFG_S", true},
	{0, 27, 28, "ADSP_TMBIST_S", true},
	{0, 28, 29, "ADSP_RSV_S", true},
	{0, 29, 30, "HRE_S", true},

	/* 30 */
	{0, 30, 31, "SYSCFG_AO_S", true},
	{0, 31, 32, "BUSMON_DRAM_S", true},
	{0, 32, 33, "BUSMON_INFRA_S", true},
	{0, 33, 34, "DBG_TRACKER_EMI_APB_S", true},
	{0, 34, 35, "DBG_TRACKER_INFRA_APB_S", true},
	{0, 35, 36, "BUS_DEBUG_S", true},
	{0, 36, 37, "DAPC_S", true},
	{0, 37, 38, "K_BCRM_S", true},
	{0, 38, 39, "BCRM_S", true},
	{0, 39, 40, "DAPC_AO_S", true},

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

	{-1, -1, 79, "Decode_error", true},
	{-1, -1, 80, "Decode_error", true},
	{-1, -1, 81, "Decode_error", true},

	{-1, -1, 82, "DEVICE_APC_AO_AUD_BUS_AO_PD", false},
	{-1, -1, 83, "DEVICE_APC_AUD_BUS_AO_PDN", false},
};

static const struct mtk_device_info mt6985_devices_mminfra[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "GCE_M_APB_S", true},
	{0, 1, 1, "GCE_M_APB_S-1", true},
	{0, 2, 2, "GCE_M_APB_S-2", true},
	{0, 3, 3, "GCE_M_APB_S-3", true},
	{0, 4, 4, "GCE_M_APB_S-4", true},
	{0, 5, 5, "GCE_M_APB_S-5", true},
	{0, 6, 6, "GCE_M_APB_S-6", true},
	{0, 7, 7, "GCE_M_APB_S-7", true},
	{0, 8, 8, "GCE_D_APB_S", true},
	{0, 9, 9, "GCE_D_APB_S-1", true},

	/* 10 */
	{0, 10, 10, "GCE_D_APB_S-2", true},
	{0, 11, 11, "GCE_D_APB_S-3", true},
	{0, 12, 12, "GCE_D_APB_S-4", true},
	{0, 13, 13, "GCE_D_APB_S-5", true},
	{0, 14, 14, "GCE_D_APB_S-6", true},
	{0, 15, 15, "GCE_D_APB_S-7", true},
	{0, 16, 16, "MMINFRA_APB_S", true},
	{0, 17, 17, "MMINFRA_APB_S-1", true},
	{0, 18, 18, "MMINFRA_APB_S-2", true},
	{0, 19, 19, "MMINFRA_APB_S-3", true},

	/* 20 */
	{0, 20, 20, "MMINFRA_APB_S-4", true},
	{0, 21, 21, "MMINFRA_APB_S-5", true},
	{0, 22, 22, "MMINFRA_APB_S-6", true},
	{0, 23, 23, "MMINFRA_APB_S-7", true},
	{0, 24, 24, "MMINFRA_APB_S-8", true},
	{0, 25, 25, "MMINFRA_APB_S-9", true},
	{0, 26, 26, "MMINFRA_APB_S-10", true},
	{0, 27, 27, "MMINFRA_APB_S-11", true},
	{0, 28, 28, "MMINFRA_APB_S-12", true},
	{0, 29, 29, "MMINFRA_APB_S-13", true},

	/* 30 */
	{0, 30, 30, "MMINFRA_APB_S-14", true},
	{0, 31, 31, "MMINFRA_APB_S-15", true},
	{0, 32, 32, "MMINFRA_APB_S-16", true},
	{0, 33, 33, "MMINFRA_APB_S-17", true},
	{0, 34, 34, "MMINFRA_APB_S-18", true},
	{0, 35, 35, "MMINFRA_APB_S-19", true},
	{0, 36, 36, "MMINFRA_APB_S-20", true},
	{0, 37, 37, "MMINFRA_APB_S-21", true},
	{0, 38, 38, "MMINFRA_APB_S-22", true},
	{0, 39, 39, "MMINFRA_APB_S-23", true},

	/* 40 */
	{0, 40, 40, "MMINFRA_APB_S-24", true},
	{0, 41, 41, "MMINFRA_APB_S-25", true},
	{0, 42, 42, "MMINFRA_APB_S-26", true},
	{0, 43, 43, "MMINFRA_APB_S-27", true},
	{0, 44, 44, "MMINFRA_APB_S-28", true},
	{0, 45, 45, "MMINFRA_APB_S-37", true},
	{0, 46, 46, "DAPC_PDN_S", true},
	{0, 47, 47, "BCRM_PDN_S", true},
	{0, 48, 48, "HFRP_APB_S", true},
	{0, 49, 49, "HFRP_APB_S-1", true},

	/* 50 */
	{0, 50, 50, "HFRP_APB_S-2", true},
	{0, 51, 51, "HFRP_APB_S-3", true},
	{0, 52, 52, "HFRP_APB_S-4", true},
	{0, 53, 53, "HFRP_APB_S-5", true},
	{0, 54, 54, "HFRP_APB_S-6", true},
	{0, 55, 55, "HFRP_APB_S-7", true},
	{0, 56, 56, "HFRP_APB_S-8", true},
	{0, 57, 57, "HFRP_APB_S-9", true},
	{0, 58, 58, "HFRP_APB_S-10", true},
	{0, 59, 59, "HFRP_APB_S-11", true},

	/* 60 */
	{0, 60, 60, "HFRP_APB_S-12", true},
	{0, 61, 61, "HFRP_APB_S-13", true},
	{0, 62, 62, "HFRP_APB_S-14", true},
	{0, 63, 63, "HFRP_APB_S-15", true},
	{0, 64, 64, "VENC1_APB_S", true},
	{0, 65, 65, "VENC1_APB_S-1", true},
	{0, 66, 66, "VENC1_APB_S-2", true},
	{0, 67, 67, "VENC1_APB_S-3", true},
	{0, 68, 68, "VENC1_APB_S-4", true},
	{0, 69, 69, "VENC1_APB_S-5", true},

	/* 70 */
	{0, 70, 70, "VENC1_APB_S-6", true},
	{0, 71, 71, "VENC1_APB_S-7", true},
	{0, 72, 72, "VENC1_APB_S-8", true},
	{0, 73, 73, "VENC1_APB_S-9", true},
	{0, 74, 74, "VENC2_APB_S", true},
	{0, 75, 75, "VENC2_APB_S-1", true},
	{0, 76, 76, "VENC2_APB_S-2", true},
	{0, 77, 77, "VENC2_APB_S-3", true},
	{0, 78, 78, "VENC2_APB_S-4", true},
	{0, 79, 79, "VENC2_APB_S-5", true},

	/* 80 */
	{0, 80, 80, "VENC2_APB_S-6", true},
	{0, 81, 81, "VENC2_APB_S-7", true},
	{0, 82, 82, "VENC2_APB_S-8", true},
	{0, 83, 83, "VENC2_APB_S-9", true},
	{0, 84, 84, "VENC2_APB_S-10", true},
	{0, 85, 85, "VENC2_APB_S-11", true},
	{0, 86, 86, "VENC3_APB_S", true},
	{0, 87, 87, "VENC3_APB_S-1", true},
	{0, 88, 88, "VENC3_APB_S-2", true},
	{0, 89, 89, "VENC3_APB_S-3", true},

	/* 90 */
	{0, 90, 90, "VENC3_APB_S-4", true},
	{0, 91, 91, "VENC3_APB_S-5", true},
	{0, 92, 92, "VENC3_APB_S-6", true},
	{0, 93, 93, "VENC3_APB_S-7", true},
	{0, 94, 94, "VENC3_APB_S-8", true},
	{0, 95, 95, "VENC3_APB_S-9", true},
	{0, 96, 96, "VDEC_APB_S", true},
	{0, 97, 97, "VDEC_APB_S-1", true},
	{0, 98, 98, "VDEC_APB_S-2", true},
	{0, 99, 99, "VDEC_APB_S-3", true},

	/* 100 */
	{0, 100, 100, "VDEC_APB_S-4", true},
	{0, 101, 101, "VDEC_APB_S-5", true},
	{0, 102, 102, "VDEC_APB_S-6", true},
	{0, 103, 103, "VDEC_APB_S-7", true},
	{0, 104, 104, "VDEC_APB_S-8", true},
	{0, 105, 105, "VDEC_APB_S-9", true},
	{0, 106, 106, "VDEC_APB_S-10", true},
	{0, 107, 107, "VDEC_APB_S-11", true},
	{0, 108, 108, "VDEC_APB_S-12", true},
	{0, 109, 109, "VDEC_APB_S-13", true},

	/* 110 */
	{0, 110, 110, "VDEC_APB_S-14", true},
	{0, 111, 111, "VDEC_APB_S-15", true},
	{0, 112, 112, "CCU_APB_S", true},
	{0, 113, 113, "CCU_APB_S-1", true},
	{0, 114, 114, "CCU_APB_S-2", true},
	{0, 115, 115, "CCU_APB_S-3", true},
	{0, 116, 116, "CCU_APB_S-4", true},
	{0, 117, 117, "CCU_APB_S-5", true},
	{0, 118, 118, "CCU_APB_S-6", true},
	{0, 119, 119, "CCU_APB_S-7", true},

	/* 120 */
	{0, 120, 120, "CCU_APB_S-8", true},
	{0, 121, 121, "CCU_APB_S-9", true},
	{0, 122, 122, "CCU_APB_S-10", true},
	{0, 123, 123, "CCU_APB_S-11", true},
	{0, 124, 124, "CCU_APB_S-12", true},
	{0, 125, 125, "CCU_APB_S-13", true},
	{0, 126, 126, "CCU_APB_S-14", true},
	{0, 127, 127, "CCU_APB_S-15", true},
	{0, 128, 128, "CCU_APB_S-16", true},
	{0, 129, 129, "CCU_APB_S-17", true},

	/* 130 */
	{0, 130, 130, "CCU_APB_S-18", true},
	{0, 131, 131, "CCU_APB_S-19", true},
	{0, 132, 132, "IMG_APB_S", true},
	{0, 133, 133, "IMG_APB_S-1", true},
	{0, 134, 134, "IMG_APB_S-2", true},
	{0, 135, 135, "IMG_APB_S-3", true},
	{0, 136, 136, "IMG_APB_S-4", true},
	{0, 137, 137, "IMG_APB_S-5", true},
	{0, 138, 138, "IMG_APB_S-6", true},
	{0, 139, 139, "IMG_APB_S-7", true},

	/* 140 */
	{0, 140, 140, "IMG_APB_S-8", true},
	{0, 141, 141, "IMG_APB_S-9", true},
	{0, 142, 142, "IMG_APB_S-10", true},
	{0, 143, 143, "IMG_APB_S-11", true},
	{0, 144, 144, "IMG_APB_S-12", true},
	{0, 145, 145, "IMG_APB_S-13", true},
	{0, 146, 146, "IMG_APB_S-14", true},
	{0, 147, 147, "IMG_APB_S-15", true},
	{0, 148, 148, "IMG_APB_S-16", true},
	{0, 149, 149, "IMG_APB_S-17", true},

	/* 150 */
	{0, 150, 150, "IMG_APB_S-18", true},
	{0, 151, 151, "IMG_APB_S-19", true},
	{0, 152, 152, "IMG_APB_S-20", true},
	{0, 153, 153, "IMG_APB_S-21", true},
	{0, 154, 154, "IMG_APB_S-22", true},
	{0, 155, 155, "IMG_APB_S-23", true},
	{0, 156, 156, "IMG_APB_S-24", true},
	{0, 157, 157, "IMG_APB_S-25", true},
	{0, 158, 158, "IMG_APB_S-26", true},
	{0, 159, 159, "IMG_APB_S-27", true},

	/* 160 */
	{0, 160, 160, "IMG_APB_S-28", true},
	{0, 161, 161, "IMG_APB_S-29", true},
	{0, 162, 162, "IMG_APB_S-30", true},
	{0, 163, 163, "IMG_APB_S-31", true},
	{0, 164, 164, "IMG_APB_S-32", true},
	{0, 165, 165, "IMG_APB_S-33", true},
	{0, 166, 166, "IMG_APB_S-34", true},
	{0, 167, 167, "IMG_APB_S-35", true},
	{0, 168, 168, "IMG_APB_S-36", true},
	{0, 169, 169, "IMG_APB_S-37", true},

	/* 170 */
	{0, 170, 170, "IMG_APB_S-38", true},
	{0, 171, 171, "IMG_APB_S-39", true},
	{0, 172, 172, "IMG_APB_S-40", true},
	{0, 173, 173, "IMG_APB_S-41", true},
	{0, 174, 174, "IMG_APB_S-42", true},
	{0, 175, 175, "IMG_APB_S-43", true},
	{0, 176, 176, "IMG_APB_S-44", true},
	{0, 177, 177, "IMG_APB_S-45", true},
	{0, 178, 178, "IMG_APB_S-46", true},
	{0, 179, 179, "IMG_APB_S-47", true},

	/* 180 */
	{0, 180, 180, "IMG_APB_S-48", true},
	{0, 181, 181, "IMG_APB_S-49", true},
	{0, 182, 182, "IMG_APB_S-50", true},
	{0, 183, 183, "IMG_APB_S-51", true},
	{0, 184, 184, "IMG_APB_S-52", true},
	{0, 185, 185, "IMG_APB_S-53", true},
	{0, 186, 186, "IMG_APB_S-54", true},
	{0, 187, 187, "CAM_APB_S", true},
	{0, 188, 188, "CAM_APB_S-1", true},
	{0, 189, 189, "CAM_APB_S-2", true},

	/* 190 */
	{0, 190, 190, "CAM_APB_S-3", true},
	{0, 191, 191, "CAM_APB_S-4", true},
	{0, 192, 192, "CAM_APB_S-5", true},
	{0, 193, 193, "CAM_APB_S-6", true},
	{0, 194, 194, "CAM_APB_S-7", true},
	{0, 195, 195, "CAM_APB_S-8", true},
	{0, 196, 196, "CAM_APB_S-9", true},
	{0, 197, 197, "CAM_APB_S-10", true},
	{0, 198, 198, "CAM_APB_S-11", true},
	{0, 199, 199, "CAM_APB_S-12", true},

	/* 200 */
	{0, 200, 200, "CAM_APB_S-13", true},
	{0, 201, 201, "CAM_APB_S-14", true},
	{0, 202, 202, "CAM_APB_S-15", true},
	{0, 203, 203, "CAM_APB_S-16", true},
	{0, 204, 204, "CAM_APB_S-17", true},
	{0, 205, 205, "CAM_APB_S-18", true},
	{0, 206, 206, "CAM_APB_S-19", true},
	{0, 207, 207, "CAM_APB_S-20", true},
	{0, 208, 208, "CAM_APB_S-21", true},
	{0, 209, 209, "CAM_APB_S-22", true},

	/* 210 */
	{0, 210, 210, "CAM_APB_S-23", true},
	{0, 211, 211, "CAM_APB_S-24", true},
	{0, 212, 212, "CAM_APB_S-25", true},
	{0, 213, 213, "CAM_APB_S-26", true},
	{0, 214, 214, "CAM_APB_S-27", true},
	{0, 215, 215, "CAM_APB_S-28", true},
	{0, 216, 216, "CAM_APB_S-29", true},
	{0, 217, 217, "CAM_APB_S-30", true},
	{0, 218, 218, "CAM_APB_S-31", true},
	{0, 219, 219, "CAM_APB_S-32", true},

	/* 220 */
	{0, 220, 220, "CAM_APB_S-33", true},
	{0, 221, 221, "CAM_APB_S-34", true},
	{0, 222, 222, "CAM_APB_S-35", true},
	{0, 223, 223, "CAM_APB_S-36", true},
	{0, 224, 224, "CAM_APB_S-37", true},
	{0, 225, 225, "CAM_APB_S-38", true},
	{0, 226, 226, "CAM_APB_S-39", true},
	{0, 227, 227, "CAM_APB_S-40", true},
	{0, 228, 228, "CAM_APB_S-41", true},
	{0, 229, 229, "CAM_APB_S-42", true},

	/* 230 */
	{0, 230, 230, "CAM_APB_S-43", true},
	{0, 231, 231, "CAM_APB_S-44", true},
	{0, 232, 232, "CAM_APB_S-45", true},
	{0, 233, 233, "CAM_APB_S-46", true},
	{0, 234, 234, "CAM_APB_S-47", true},
	{0, 235, 235, "CAM_APB_S-48", true},
	{0, 236, 236, "CAM_APB_S-49", true},
	{0, 237, 237, "CAM_APB_S-50", true},
	{0, 238, 238, "CAM_APB_S-51", true},
	{0, 239, 239, "CAM_APB_S-52", true},

	/* 240 */
	{0, 240, 240, "CAM_APB_S-53", true},
	{0, 241, 241, "CAM_APB_S-54", true},
	{0, 242, 242, "CAM_APB_S-55", true},
	{0, 243, 243, "CAM_APB_S-56", true},
	{0, 244, 244, "CAM_APB_S-57", true},
	{0, 245, 245, "CAM_APB_S-58", true},
	{0, 246, 246, "CAM_APB_S-59", true},
	{0, 247, 247, "CAM_APB_S-60", true},
	{0, 248, 248, "CAM_APB_S-61", true},
	{0, 249, 249, "CAM_APB_S-62", true},

	/* 250 */
	{0, 250, 250, "CAM_APB_S-63", true},
	{0, 251, 251, "CAM_APB_S-64", true},
	{0, 252, 252, "CAM_APB_S-65", true},
	{0, 253, 253, "CAM_APB_S-66", true},
	{0, 254, 254, "CAM_APB_S-67", true},
	{0, 255, 255, "CAM_APB_S-68", true},
	{1, 0, 256, "CAM_APB_S-69", true},
	{1, 1, 257, "CAM_APB_S-70", true},
	{1, 2, 258, "CAM_APB_S-71", true},
	{1, 3, 259, "CAM_APB_S-72", true},

	/* 260 */
	{1, 4, 260, "CAM_APB_S-73", true},
	{1, 5, 261, "CAM_APB_S-74", true},
	{1, 6, 262, "CAM_APB_S-75", true},
	{1, 7, 263, "CAM_APB_S-76", true},
	{1, 8, 264, "CAM_APB_S-77", true},
	{1, 9, 265, "CAM_APB_S-78", true},
	{1, 10, 266, "CAM_APB_S-79", true},
	{1, 11, 267, "CAM_APB_S-80", true},
	{1, 12, 268, "CAM_APB_S-81", true},
	{1, 13, 269, "CAM_APB_S-82", true},

	/* 270 */
	{1, 14, 270, "CAM_APB_S-83", true},
	{1, 15, 271, "CAM_APB_S-84", true},
	{1, 16, 272, "CAM_APB_S-85", true},
	{1, 17, 273, "CAM_APB_S-86", true},
	{1, 18, 274, "CAM_APB_S-87", true},
	{1, 19, 275, "CAM_APB_S-88", true},
	{1, 20, 276, "CAM_APB_S-89", true},
	{1, 21, 277, "CAM_APB_S-90", true},
	{1, 22, 278, "CAM_APB_S-91", true},
	{1, 23, 279, "CAM_APB_S-92", true},

	/* 280 */
	{1, 24, 280, "CAM_APB_S-93", true},
	{1, 25, 281, "CAM_APB_S-94", true},
	{1, 26, 282, "CAM_APB_S-95", true},
	{1, 27, 283, "CAM_APB_S-96", true},
	{1, 28, 284, "CAM_APB_S-97", true},
	{1, 29, 285, "CAM_APB_S-98", true},
	{1, 30, 286, "CAM_APB_S-99", true},
	{1, 31, 287, "CAM_APB_S-100", true},
	{1, 32, 288, "CAM_APB_S-101", true},
	{1, 33, 289, "CAM_APB_S-102", true},

	/* 290 */
	{1, 34, 290, "CAM_APB_S-103", true},
	{1, 35, 291, "CAM_APB_S-104", true},
	{1, 36, 292, "CAM_APB_S-105", true},
	{1, 37, 293, "CAM_APB_S-106", true},
	{1, 38, 294, "CAM_APB_S-107", true},
	{1, 39, 295, "CAM_APB_S-108", true},
	{1, 40, 296, "CAM_APB_S-109", true},
	{1, 41, 297, "CAM_APB_S-110", true},
	{1, 42, 298, "CAM_APB_S-111", true},
	{1, 43, 299, "CAM_APB_S-112", true},

	/* 300 */
	{1, 44, 300, "CAM_APB_S-113", true},
	{1, 45, 301, "CAM_APB_S-114", true},
	{1, 46, 302, "CAM_APB_S-115", true},
	{1, 47, 303, "CAM_APB_S-116", true},
	{1, 48, 304, "CAM_APB_S-117", true},
	{1, 49, 305, "CAM_APB_S-118", true},
	{1, 50, 306, "CAM_APB_S-119", true},
	{1, 51, 307, "CAM_APB_S-120", true},
	{1, 52, 308, "CAM_APB_S-121", true},
	{1, 53, 309, "DISP_APB_S", true},

	/* 310 */
	{1, 54, 310, "DISP_APB_S-1", true},
	{1, 55, 311, "DISP_APB_S-2", true},
	{1, 56, 312, "DISP_APB_S-3", true},
	{1, 57, 313, "DISP_APB_S-4", true},
	{1, 58, 314, "DISP_APB_S-5", true},
	{1, 59, 315, "DISP_APB_S-6", true},
	{1, 60, 316, "DISP_APB_S-7", true},
	{1, 61, 317, "DISP_APB_S-8", true},
	{1, 62, 318, "DISP_APB_S-9", true},
	{1, 63, 319, "DISP_APB_S-10", true},

	/* 320 */
	{1, 64, 320, "DISP_APB_S-11", true},
	{1, 65, 321, "DISP_APB_S-12", true},
	{1, 66, 322, "DISP_APB_S-13", true},
	{1, 67, 323, "DISP_APB_S-14", true},
	{1, 68, 324, "DISP_APB_S-15", true},
	{1, 69, 325, "DISP_APB_S-16", true},
	{1, 70, 326, "DISP_APB_S-17", true},
	{1, 71, 327, "DISP_APB_S-18", true},
	{1, 72, 328, "DISP_APB_S-19", true},
	{1, 73, 329, "DISP_APB_S-20", true},

	/* 330 */
	{1, 74, 330, "DISP_APB_S-21", true},
	{1, 75, 331, "DISP_APB_S-22", true},
	{1, 76, 332, "DISP_APB_S-23", true},
	{1, 77, 333, "DISP_APB_S-24", true},
	{1, 78, 334, "DISP_APB_S-25", true},
	{1, 79, 335, "DISP_APB_S-26", true},
	{1, 80, 336, "DISP_APB_S-27", true},
	{1, 81, 337, "DISP_APB_S-28", true},
	{1, 82, 338, "DISP_APB_S-29", true},
	{1, 83, 339, "DISP_APB_S-30", true},

	/* 340 */
	{1, 84, 340, "DISP_APB_S-31", true},
	{1, 85, 341, "DISP_APB_S-32", true},
	{1, 86, 342, "DISP_APB_S-33", true},
	{1, 87, 343, "DISP_APB_S-34", true},
	{1, 88, 344, "DISP_APB_S-35", true},
	{1, 89, 345, "DISP_APB_S-36", true},
	{1, 90, 346, "DISP1_APB_S", true},
	{1, 91, 347, "DISP1_APB_S-1", true},
	{1, 92, 348, "DISP1_APB_S-2", true},
	{1, 93, 349, "DISP1_APB_S-3", true},

	/* 350 */
	{1, 94, 350, "DISP1_APB_S-4", true},
	{1, 95, 351, "DISP1_APB_S-5", true},
	{1, 96, 352, "DISP1_APB_S-6", true},
	{1, 97, 353, "DISP1_APB_S-7", true},
	{1, 98, 354, "DISP1_APB_S-8", true},
	{1, 99, 355, "DISP1_APB_S-9", true},
	{1, 100, 356, "DISP1_APB_S-10", true},
	{1, 101, 357, "DISP1_APB_S-11", true},
	{1, 102, 358, "DISP1_APB_S-12", true},
	{1, 103, 359, "DISP1_APB_S-13", true},

	/* 360 */
	{1, 104, 360, "DISP1_APB_S-14", true},
	{1, 105, 361, "DISP1_APB_S-15", true},
	{1, 106, 362, "DISP1_APB_S-16", true},
	{1, 107, 363, "DISP1_APB_S-17", true},
	{1, 108, 364, "DISP1_APB_S-18", true},
	{1, 109, 365, "DISP1_APB_S-19", true},
	{1, 110, 366, "DISP1_APB_S-20", true},
	{1, 111, 367, "DISP1_APB_S-21", true},
	{1, 112, 368, "DISP1_APB_S-22", true},
	{1, 113, 369, "DISP1_APB_S-23", true},

	/* 370 */
	{1, 114, 370, "DISP1_APB_S-24", true},
	{1, 115, 371, "DISP1_APB_S-25", true},
	{1, 116, 372, "DISP1_APB_S-26", true},
	{1, 117, 373, "DISP1_APB_S-27", true},
	{1, 118, 374, "DISP1_APB_S-28", true},
	{1, 119, 375, "DISP1_APB_S-29", true},
	{1, 120, 376, "DISP1_APB_S-30", true},
	{1, 121, 377, "DISP1_APB_S-31", true},
	{1, 122, 378, "DISP1_APB_S-32", true},
	{1, 123, 379, "DISP1_APB_S-33", true},

	/* 380 */
	{1, 124, 380, "DISP1_APB_S-34", true},
	{1, 125, 381, "DISP1_APB_S-35", true},
	{1, 126, 382, "DISP1_APB_S-36", true},
	{1, 127, 383, "OVL_APB_S", true},
	{1, 128, 384, "OVL_APB_S-1", true},
	{1, 129, 385, "OVL_APB_S-2", true},
	{1, 130, 386, "OVL_APB_S-3", true},
	{1, 131, 387, "OVL_APB_S-4", true},
	{1, 132, 388, "OVL_APB_S-5", true},
	{1, 133, 389, "OVL_APB_S-6", true},

	/* 390 */
	{1, 134, 390, "OVL_APB_S-7", true},
	{1, 135, 391, "OVL_APB_S-8", true},
	{1, 136, 392, "OVL_APB_S-9", true},
	{1, 137, 393, "OVL_APB_S-10", true},
	{1, 138, 394, "OVL_APB_S-11", true},
	{1, 139, 395, "OVL_APB_S-12", true},
	{1, 140, 396, "OVL_APB_S-13", true},
	{1, 141, 397, "OVL_APB_S-14", true},
	{1, 142, 398, "OVL_APB_S-15", true},
	{1, 143, 399, "OVL_APB_S-16", true},

	/* 400 */
	{1, 144, 400, "OVL1_APB_S", true},
	{1, 145, 401, "OVL1_APB_S-1", true},
	{1, 146, 402, "OVL1_APB_S-2", true},
	{1, 147, 403, "OVL1_APB_S-3", true},
	{1, 148, 404, "OVL1_APB_S-4", true},
	{1, 149, 405, "OVL1_APB_S-5", true},
	{1, 150, 406, "OVL1_APB_S-6", true},
	{1, 151, 407, "OVL1_APB_S-7", true},
	{1, 152, 408, "OVL1_APB_S-8", true},
	{1, 153, 409, "OVL1_APB_S-9", true},

	/* 410 */
	{1, 154, 410, "OVL1_APB_S-10", true},
	{1, 155, 411, "OVL1_APB_S-11", true},
	{1, 156, 412, "OVL1_APB_S-12", true},
	{1, 157, 413, "OVL1_APB_S-13", true},
	{1, 158, 414, "OVL1_APB_S-14", true},
	{1, 159, 415, "OVL1_APB_S-15", true},
	{1, 160, 416, "OVL1_APB_S-16", true},
	{1, 161, 417, "MML_APB_S", true},
	{1, 162, 418, "MML_APB_S-1", true},
	{1, 163, 419, "MML_APB_S-2", false},

	/* 420 */
	{1, 164, 420, "MML_APB_S-3", true},
	{1, 165, 421, "MML_APB_S-4", true},
	{1, 166, 422, "MML_APB_S-5", true},
	{1, 167, 423, "MML_APB_S-6", true},
	{1, 168, 424, "MML_APB_S-7", true},
	{1, 169, 425, "MML_APB_S-8", true},
	{1, 170, 426, "MML_APB_S-9", true},
	{1, 171, 427, "MML_APB_S-10", true},
	{1, 172, 428, "MML_APB_S-11", true},
	{1, 173, 429, "MML_APB_S-12", true},

	/* 430 */
	{1, 174, 430, "MML_APB_S-13", true},
	{1, 175, 431, "MML_APB_S-14", true},
	{1, 176, 432, "MML_APB_S-15", true},
	{1, 177, 433, "MML_APB_S-16", true},
	{1, 178, 434, "MML_APB_S-17", true},
	{1, 179, 435, "MML_APB_S-18", true},
	{1, 180, 436, "MML_APB_S-19", true},
	{1, 181, 437, "MML_APB_S-20", true},
	{1, 182, 438, "MML_APB_S-21", true},
	{1, 183, 439, "MML_APB_S-22", true},

	/* 440 */
	{1, 184, 440, "MML_APB_S-23", false},
	{1, 185, 441, "MML_APB_S-24", true},
	{1, 186, 442, "MML_APB_S-25", true},
	{1, 187, 443, "MML_APB_S-26", true},
	{1, 188, 444, "MML1_APB_S", true},
	{1, 189, 445, "MML1_APB_S-1", true},
	{1, 190, 446, "MML1_APB_S-2", false},
	{1, 191, 447, "MML1_APB_S-3", true},
	{1, 192, 448, "MML1_APB_S-4", true},
	{1, 193, 449, "MML1_APB_S-5", true},

	/* 450 */
	{1, 194, 450, "MML1_APB_S-6", true},
	{1, 195, 451, "MML1_APB_S-7", true},
	{1, 196, 452, "MML1_APB_S-8", true},
	{1, 197, 453, "MML1_APB_S-9", true},
	{1, 198, 454, "MML1_APB_S-10", true},
	{1, 199, 455, "MML1_APB_S-11", true},
	{1, 200, 456, "MML1_APB_S-12", true},
	{1, 201, 457, "MML1_APB_S-13", true},
	{1, 202, 458, "MML1_APB_S-14", true},
	{1, 203, 459, "MML1_APB_S-15", true},

	/* 460 */
	{1, 204, 460, "MML1_APB_S-16", true},
	{1, 205, 461, "MML1_APB_S-17", true},
	{1, 206, 462, "MML1_APB_S-18", true},
	{1, 207, 463, "MML1_APB_S-19", true},
	{1, 208, 464, "MML1_APB_S-20", true},
	{1, 209, 465, "MML1_APB_S-21", true},
	{1, 210, 466, "MML1_APB_S-22", true},
	{1, 211, 467, "MML1_APB_S-23", false},
	{1, 212, 468, "MML1_APB_S-24", true},
	{1, 213, 469, "MML1_APB_S-25", true},

	/* 470 */
	{1, 214, 470, "MML1_APB_S-26", true},
	{1, 215, 472, "HRE_APB_S", true},
	{1, 216, 478, "DPTX_APB_S", true},
	{1, 217, 485, "DAPC_AO_S", true},
	{1, 218, 486, "BCRM_AO_S", true},
	{1, 219, 487, "DEBUG_CTL_AO_S", true},

	{-1, -1, 488, "OOB_way_en", true},
	{-1, -1, 489, "OOB_way_en", true},

	{-1, -1, 490, "OOB_way_en", true},
	{-1, -1, 491, "OOB_way_en", true},
	{-1, -1, 492, "OOB_way_en", true},
	{-1, -1, 493, "OOB_way_en", true},
	{-1, -1, 494, "OOB_way_en", true},
	{-1, -1, 495, "OOB_way_en", true},
	{-1, -1, 496, "OOB_way_en", true},
	{-1, -1, 497, "OOB_way_en", true},
	{-1, -1, 498, "OOB_way_en", true},
	{-1, -1, 499, "OOB_way_en", true},

	{-1, -1, 500, "OOB_way_en", true},
	{-1, -1, 501, "OOB_way_en", true},
	{-1, -1, 502, "OOB_way_en", true},
	{-1, -1, 503, "OOB_way_en", true},
	{-1, -1, 504, "OOB_way_en", true},
	{-1, -1, 505, "OOB_way_en", true},
	{-1, -1, 506, "OOB_way_en", true},
	{-1, -1, 507, "OOB_way_en", true},
	{-1, -1, 508, "OOB_way_en", true},
	{-1, -1, 509, "OOB_way_en", true},

	{-1, -1, 510, "OOB_way_en", true},
	{-1, -1, 511, "OOB_way_en", true},

	{-1, -1, 512, "Decode_error", true},
	{-1, -1, 513, "Decode_error", true},
	{-1, -1, 514, "Decode_error", true},
	{-1, -1, 515, "Decode_error", true},
	{-1, -1, 516, "Decode_error", true},

	{-1, -1, 517, "GCE_D", false},
	{-1, -1, 518, "GCE_M", false},
	{-1, -1, 519, "DEVICE_APC_MM_AO", false},
	{-1, -1, 520, "DEVICE_APC_MM_PDN", false},
};

static const struct mtk_device_info mt6985_devices_mmup[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "p_main_BCRM", true},
	{0, 1, 1, "p_main_DEBUG", true},
	{0, 2, 2, "p_main_DEVAPCAO", true},
	{0, 3, 3, "p_main_DEVAPC", true},
	{0, 4, 4, "p_par_top", true},
	{0, 5, 5, "pslv_clk_ctrl", true},
	{0, 6, 6, "pslv_cfgreg", true},
	{0, 7, 7, "pslv_uart", true},
	{0, 8, 8, "pslv_uart1", true},
	{0, 9, 9, "pslv_cfg_core0", true},

	/* 10 */
	{0, 10, 10, "pslv_dma_core0", true},
	{0, 11, 11, "pslv_irq_core0", true},
	{0, 12, 12, "pslv_tmr_core0", true},
	{0, 13, 13, "pslv_dbg_core0", true},
	{0, 14, 14, "pbus_tracker", true},
	{0, 15, 15, "pCACHE0", true},
	{0, 16, 16, "pslv_cfgreg_sec", true},
	{0, 17, 17, "p_mbox0", true},
	{0, 18, 18, "p_mbox1", true},
	{0, 19, 19, "p_mbox2", true},

	/* 20 */
	{0, 20, 20, "p_mbox3", true},
	{0, 21, 21, "p_mbox4", true},
	{0, 22, 22, "pslv_rsv00", true},
	{0, 23, 23, "pslv_rsv01", true},

	{-1, -1, 24, "OOB_way_en", true},
	{-1, -1, 25, "OOB_way_en", true},
	{-1, -1, 26, "OOB_way_en", true},
	{-1, -1, 27, "OOB_way_en", true},
	{-1, -1, 28, "OOB_way_en", true},
	{-1, -1, 29, "OOB_way_en", true},

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

	{-1, -1, 50, "Decode_error", true},
	{-1, -1, 51, "Decode_error", true},

	{-1, -1, 52, "DEVICE_APC_MMUP_AO", false},
	{-1, -1, 53, "DEVICE_APC_MMUP_PDN", false},
};

static const struct mtk_device_info mt6985_devices_gpu[] = {
	/* sys_idx, ctrl_idx, vio_idx, device, vio_irq */
	/* 0 */
	{0, 0, 0, "mfg_rpc", true},
	{0, 1, 1, "mfgpll", true},
	{0, 2, 2, "MFG_DEVICE_APC_AO", true},
	{0, 3, 3, "MFG_DEVICE_APC_VIO", true},
	{0, 4, 4, "GPU EB_TCM", true},
	{0, 5, 5, "GPU EB_CKCTRL", true},
	{0, 6, 6, "GPU EB_INTC", true},
	{0, 7, 7, "GPU EB_DMA", true},
	{0, 8, 8, "GPU EB_CFGREG", true},
	{0, 9, 9, "GPU EB_SCUAPB", true},

	/* 10 */
	{0, 10, 10, "GPU EB_DBGAPB", true},
	{0, 11, 11, "GPU EB_MBOX", true},
	{0, 12, 12, "GPU IP", true},
	{0, 13, 13, "Reserved", true},
	{0, 14, 14, "MPU", true},
	{0, 15, 15, "DFD", true},
	{0, 16, 16, "CPE_controller", true},
	{0, 17, 17, "Mali DVFS Hint", true},
	{0, 18, 18, "G3D Secure Reg", true},
	{0, 19, 19, "G3D_CONFIG", true},

	/* 20 */
	{0, 20, 20, "Sensor network", true},
	{0, 21, 21, "Imax Limter", true},
	{0, 22, 22, "G3D TestBench", true},
	{0, 23, 23, "mfg_ips_ses ", true},
	{0, 24, 24, "PTP_THERM_CTRL", true},

	{-1, -1, 25, "DEVICE_APC_GPU_AO", false},
	{-1, -1, 26, "DEVICE_APC_GPU_PDN", false},
};

enum DEVAPC_VIO_SLAVE_NUM {
	VIO_SLAVE_NUM_INFRA = ARRAY_SIZE(mt6985_devices_infra),
	VIO_SLAVE_NUM_INFRA1 = ARRAY_SIZE(mt6985_devices_infra1),
	VIO_SLAVE_NUM_PERI_PAR = ARRAY_SIZE(mt6985_devices_peri_par),
	VIO_SLAVE_NUM_VLP = ARRAY_SIZE(mt6985_devices_vlp),
	VIO_SLAVE_NUM_ADSP = ARRAY_SIZE(mt6985_devices_adsp),
	VIO_SLAVE_NUM_MMINFRA = ARRAY_SIZE(mt6985_devices_mminfra),
	VIO_SLAVE_NUM_MMUP = ARRAY_SIZE(mt6985_devices_mmup),
	VIO_SLAVE_NUM_GPU = ARRAY_SIZE(mt6985_devices_gpu),
};

#endif /* __DEVAPC_MT6985_H__ */
