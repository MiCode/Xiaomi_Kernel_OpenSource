/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/* An example test TA implementation.
 */

#ifndef __TRUSTZONE_TA_M4U__
#define __TRUSTZONE_TA_M4U__

#define TZ_TA_M4U_UUID   "m4u-smi-mau-spc"

/* Data Structure for Test TA */
/* You should define data structure used both in REE/TEE here
 *  N/A for Test TA
 */

/* Command for Test TA */
#define M4U_TZCMD_TEST              0
#define M4U_TZCMD_CONFIG_PORT       66
#define M4U_TZCMD_REG_BACKUP        67
#define M4U_TZCMD_REG_RESTORE       68
#define M4U_TZCMD_ALLOC_MVA_SEC     70
#define M4U_TZCMD_DEALLOC_MVA_SEC   71
/*====syn nonsec pgt start*/
#define M4U_TZCMD_SEC_INIT          72
#define M4U_TZCMD_MAP_NONSEC_BUF    73
#define M4U_TZCMD_DEALLOC_MVA_SYNSEC 74
/*====syn nonsec pgt end */

#define M4U_TZCMD_SECPGTDUMP            100
#define M4U_TZCMD_LARB_REG_BACKUP       101
#define M4U_TZCMD_LARB_REG_RESTORE      102


#if 1  /* for m4u whole in tee. mt8135*/
#define M4U_TZCMD_INVALID_TLB       75
#define M4U_TZCMD_HW_INIT           76
#define M4U_TZCMD_DUMP_REG          77
#define M4U_TZCMD_WAIT_ISR          78
#define M4U_TZCMD_INVALID_CHECK     79
#define M4U_TZCMD_INSERT_SEQ        80
#define M4U_TZCMD_ERRHANGE_EN       81

#define M4U_CHECKSELF_VALUE  0x12345678

#define MMU_TOTAL_RS_NR_MT8135       8
#define M4U_MAIN_TLB_NR_MT8135       48

struct M4U_ISR_INFO {
	unsigned int u4Check;	/* fixed is M4U_CHECKSELF_VALUE */
	unsigned int u4IrqM4uIndex;
	unsigned int IntrSrc;
	unsigned int faultMva;
	unsigned int port_regval;
	int portID;
	int larbID;

	unsigned int invalidPA;

	unsigned int rs_va[MMU_TOTAL_RS_NR_MT8135];
	unsigned int rs_pa[MMU_TOTAL_RS_NR_MT8135];
	unsigned int rs_st[MMU_TOTAL_RS_NR_MT8135];

	unsigned int main_tags[M4U_MAIN_TLB_NR_MT8135];
	unsigned int pfh_tags[M4U_MAIN_TLB_NR_MT8135];

	unsigned int main_des[M4U_MAIN_TLB_NR_MT8135];
	unsigned int pfn_des[M4U_MAIN_TLB_NR_MT8135*4];
};
#endif

#endif	/* __TRUSTZONE_TA_TEST__ */
