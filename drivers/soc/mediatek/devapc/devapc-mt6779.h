/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __DEVAPC_MT6779_H__
#define __DEVAPC_MT6779_H__

/******************************************************************************
 * VARIABLE DEFINATION
 ******************************************************************************/

/* dbg status default setting */
#define PLAT_DBG_UT_DEFAULT		false
#define PLAT_DBG_WARN_DEFAULT		false
#define PLAT_DBG_DAPC_DEFAULT		false

#define PLAT_VIO_CFG_MAX_IDX		478
#define PLAT_VIO_MAX_IDX		525
#define PLAT_VIO_MASK_STA_NUM		17
#define PLAT_VIO_SHIFT_MAX_BIT		25

/******************************************************************************
 * DATA STRUCTURE
 ******************************************************************************/

enum OTHER_TYPES_INDEX {
	SRAMROM_VIO_INDEX = 511,
	TOPAXI_SI0_DECERR = 503,
};

enum BUSID_LENGTH {
	PERIAXI_INT_MI_BIT_LENGTH = 5,
	TOPAXI_MI0_BIT_LENGTH = 12,
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

#endif /* __DEVAPC_MT6779_H__ */
