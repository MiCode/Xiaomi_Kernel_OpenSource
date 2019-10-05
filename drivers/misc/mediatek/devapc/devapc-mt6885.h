/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __DEVAPC_MT6885_H__
#define __DEVAPC_MT6885_H__

/******************************************************************************
 * VARIABLE DEFINITION
 ******************************************************************************/
/* dbg status default setting */
#define PLAT_DBG_UT_DEFAULT		false
#define PLAT_DBG_KE_DEFAULT		false
#define PLAT_DBG_AEE_DEFAULT		false
#define PLAT_DBG_WARN_DEFAULT		false
#define PLAT_DBG_DAPC_DEFAULT		false

/******************************************************************************
 * STRUCTURE DEFINITION
 ******************************************************************************/
enum DEVAPC_VIO_MASK_STA_NUM {
	VIO_MASK_STA_NUM_INFRA = 14,
	VIO_MASK_STA_NUM_PERI = 12,
	VIO_MASK_STA_NUM_PERI2 = 9,
};

enum DEVAPC_VIO_SLAVE_NUM {
	VIO_SLAVE_NUM_INFRA = 425,
	VIO_SLAVE_NUM_PERI = 362,
	VIO_SLAVE_NUM_PERI2 = 269,
};

enum DEVAPC_PD_OFFSET {
	PD_VIO_MASK_OFFSET = 0x0,
	PD_VIO_STA_OFFSET = 0x400,
	PD_VIO_DBG0_OFFSET = 0x900,
	PD_VIO_DBG1_OFFSET = 0x904,
	PD_VIO_DBG2_OFFSET = 0x908,
	PD_APC_CON_OFFSET = 0xF00,
	PD_SHIFT_STA_OFFSET = 0xF10,
	PD_SHIFT_SEL_OFFSET = 0xF14,
	PD_SHIFT_CON_OFFSET = 0xF20,
};

enum OTHER_TYPES_INDEX {
	SRAMROM_VIO_INDEX = 419,
	CONN_VIO_INDEX = 126, /* starts from 0x18 */
	MDP_VIO_INDEX = 352,
	DISP2_VIO_INDEX = 353,
	MMSYS_VIO_INDEX = 254,
};

enum INFRACFG_MM2ND_VIO_NUM {
	INFRACFG_MM_VIO_STA_NUM = 2,
	INFRACFG_MDP_VIO_STA_NUM = 8,
	INFRACFG_DISP2_VIO_STA_NUM = 2,
};

enum INFRACFG_MM2ND_OFFSET {
	INFRACFG_MM_SEC_VIO0_OFFSET = 0xB30,
	INFRACFG_MDP_SEC_VIO0_OFFSET = 0xB40,
	INFRACFG_DISP2_SEC_VIO0_OFFSET = 0xB70,
};

/* starts from 0x13 */
enum MFG_VIO_INDEX {
	MFG_START = 0,
	MFG_END = 8,
};

/* starts from 0x14 */
enum MM_VIO_INDEX {
	MM_START = 19,
	MM_END = 43,
	MM_SSRAM_VIO_INDEX = 411,
};

/* starts from 0x15 */
enum IMG_VIO_INDEX {
	IMG_START = 71,
	IMG_END = 114,
};

/* starts from 0x16 */
enum VDEC_VIO_INDEX {
	VDEC_START = 44,
	VDEC_END = 54,
};

/* starts from 0x17 */
enum VENC_VIO_INDEX {
	VENC_START = 55,
	VENC_END = 70,
};

/* starts from 0x19 */
enum APU_VIO_INDEX {
	APU_START = 9,
	APU_END = 14,
	APU_SSRAM_VIO_INDEX = 410,
};

/* starts from 0x1A */
enum CAM_VIO_INDEX {
	CAM_START = 115,
	CAM_END = 310,
};

/* starts from 0x1B */
enum IPE_VIO_INDEX {
	IPE_START = 311,
	IPE_END = 322,
};

/* starts from 0x105 ~ 0x108 */
enum TINY_VIO_INDEX {
	TINY_START = 39,
	TINY_END = 61,
};

/* starts from 0x1F */
enum MDP_VIO_INDEX {
	MDP_START = 323,
	MDP_END = 369,
};

/* starts from 0x2 */
enum MD_VIO_INDEX {
	MD_START = 79,
	MD_END = 121,
};

enum BUSID_LENGTH {
	PERIAXI_MI_BIT_LENGTH = 3,
	INFRAAXI_MI_BIT_LENGTH = 13,
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

#endif /* __DEVAPC_MT6885_H__ */
