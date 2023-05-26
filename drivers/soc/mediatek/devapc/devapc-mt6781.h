/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __DEVAPC_MT6781_H__
#define __DEVAPC_MT6781_H__

/******************************************************************************
 * VARIABLE DEFINATION
 ******************************************************************************/

/* dbg status default setting */
#define PLAT_DBG_UT_DEFAULT		false
#define PLAT_DBG_KE_DEFAULT		true
#define PLAT_DBG_AEE_DEFAULT		true
#define PLAT_DBG_DAPC_DEFAULT		false

#define PLAT_VIO_CFG_MAX_IDX		518
#define PLAT_VIO_MAX_IDX		558
#define PLAT_VIO_MASK_STA_NUM		18
#define PLAT_VIO_SHIFT_MAX_BIT		21

/******************************************************************************
 * DATA STRUCTURE & FUNCTION DEFINATION
 ******************************************************************************/

const char *bus_id_to_master(int bus_id, uint32_t vio_addr, int vio_idx);
const char *index_to_subsys(unsigned int index);

/* violation index corresponds to subsys */
enum SMI_INDEX {
	SMI_COMMON = 161,
	SMI_LARB0 = 162,
	SMI_LARB1 = 163,
	DISP_SMI_2X1_SUB_COMMON_U0 = 186,
	DISP_SMI_2X1_SUB_COMMON_U1 = 187,
	IMG1_SMI_2X1_SUB_COMMON = 189,
	SMI_LARB9 = 222,
	SMI_LARB11 = 255,
	SMI_LARB12 = 256,
	SMI_LARB7 = 277,
	SMI_LARB13 = 285,
	SMI_LARB14 = 286,
	CAM_SIM_3X1_SUB_COMMON_U0 = 296,
	CAM_SIM_4X1_SUB_COMMON_U0 = 297,
	SMI_LARB_16 = 299,
	SMI_LARB_17 = 300,
	SMI_LARB0_S = 472,
	IPE_SMI_2X1_SUB_COMMON = 500,
	SMI_LARB20 = 501,
	SMI_LARB19 = 517,
};

enum MMSYS_INDEX {
	MMSYS_MDP_START = 245,
	MMSYS_MDP_END = 252,
	MMSYS_DISP_START = 253,
	MMSYS_DISP_END = 271,
	MMSYS_MDP2_START = 272,
	MMSYS_MDP2_END = 279,
};

enum IMGSYS_INDEX {
	IMGSYS1_TOP = 208,
	IMGSYS2_TOP = 240,
};

enum VENCSYS_INDEX {
	VENC_GLOBAL_CON = 276,
	VENC = 278,
	VENC_MBIST_CTR = 282,
};

enum VDECSYS_INDEX {
	VDECSYS_START = 258,
	VDECSYS_END = 274,
};

enum CAMSYS_INDEX {
	CAMSYS_START = 332,
	CAMSYS_END = 443,
	CAMSYS_SENINF_START = 288,
	CAMSYS_SENINF_END = 295,
	CAMSYS_P1_START = 430,
	CAMSYS_P1_END = 443,
};

enum OTHER_TYPES_INDEX {
	SRAMROM_VIO_INDEX = 546,
	TOPAXI_SI0_DECERR = 540,
	PERIAXI_SI1_DECERR = 542,
};

enum BUSID_LENGTH {
	PERIAXI_INT_MI_BIT_LENGTH = 4,
	TOPAXI_MI0_BIT_LENGTH = 14,
};

/* bit == 2 means don't care */
struct PERIAXI_ID_INFO {
	const char	*master;
	uint8_t		bit[PERIAXI_INT_MI_BIT_LENGTH];
};

struct TOPAXI_ID_INFO {
	const char	*master;
	uint8_t		bit[TOPAXI_MI0_BIT_LENGTH];
};

/******************************************************************************
 * PLATFORM DEFINATION
 ******************************************************************************/

/* For Infra VIO_DBG */
#define INFRA_VIO_DBG_MSTID			0x0000FFFF
#define INFRA_VIO_DBG_MSTID_START_BIT		0
#define INFRA_VIO_DBG_DMNID			0x003F0000
#define INFRA_VIO_DBG_DMNID_START_BIT		16
#define INFRA_VIO_DBG_W_VIO			0x00400000
#define INFRA_VIO_DBG_W_VIO_START_BIT		22
#define INFRA_VIO_DBG_R_VIO			0x00800000
#define INFRA_VIO_DBG_R_VIO_START_BIT		23
#define INFRA_VIO_ADDR_HIGH			0x0F000000
#define INFRA_VIO_ADDR_HIGH_START_BIT		24

/* For SRAMROM VIO */
#define SRAMROM_SEC_VIO_ID_MASK			0x00FFFF00
#define SRAMROM_SEC_VIO_ID_SHIFT		8
#define SRAMROM_SEC_VIO_DOMAIN_MASK		0x0F000000
#define SRAMROM_SEC_VIO_DOMAIN_SHIFT		24
#define SRAMROM_SEC_VIO_RW_MASK			0x80000000
#define SRAMROM_SEC_VIO_RW_SHIFT		31

/* For DEVAPC PD */
#define PD_VIO_MASK_OFFSET			0x0
#define PD_VIO_STA_OFFSET			0x400
#define PD_VIO_DBG0_OFFSET			0x900
#define PD_VIO_DBG1_OFFSET			0x904
#define PD_APC_CON_OFFSET			0xF00
#define PD_SHIFT_STA_OFFSET			0xF10
#define PD_SHIFT_SEL_OFFSET			0xF14
#define PD_SHIFT_CON_OFFSET			0xF20

#endif /* __DEVAPC_MT6781_H__ */
