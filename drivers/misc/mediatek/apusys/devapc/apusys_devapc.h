/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __APUSYS_DEBUG_H__
#define __APUSYS_DEBUG_H__

#define DEVAPC_VIO_MASK(index)	(unsigned int *)(devapc_virt + 0x4 * (index))
#define DEVAPC_VIO_STA(index)	\
			(unsigned int *)(devapc_virt + 0x400 + 0x4 * (index))
#define DEVAPC_APC_CON		(unsigned int *)(devapc_virt + 0xf00)
#define DEVAPC_VIO_SHIFT_STA	(unsigned int *)(devapc_virt + 0xf10)
#define DEVAPC_VIO_SHIFT_SEL	(unsigned int *)(devapc_virt + 0xf14)
#define DEVAPC_VIO_SHIFT_CON	(unsigned int *)(devapc_virt + 0xf20)
#define DEVAPC_VIO_DBG0		(unsigned int *)(devapc_virt + 0x900)
#define DEVAPC_VIO_DBG1		(unsigned int *)(devapc_virt + 0x904)
#define DEVAPC_VIO_DBG2		(unsigned int *)(devapc_virt + 0x908)

#define MOD_NUM_1_DAPC			32
#define DEVAPC_VIO_SHIFT_MAX_BIT	11
#define DEVAPC_VIO_DBG_DMNID		0x0000003F
#define DEVAPC_VIO_DBG_DMNID_START_BIT	0
#define DEVAPC_VIO_DBG_W_VIO		0x00000040
#define DEVAPC_VIO_DBG_W_VIO_START_BIT	6
#define DEVAPC_VIO_DBG_R_VIO		0x00000080
#define DEVAPC_VIO_DBG_R_VIO_START_BIT	7
#define DEVAPC_VIO_ADDR_HIGH		0x00000F00
#define DEVAPC_VIO_ADDR_HIGH_START_BIT	8

struct mod_info {
	int sys_index;
	int ctrl_index;
	int vio_index;
	char *name;
	bool enable_vio_irq;
};

/* module list */
static const struct mod_info apusys_modules[] = {
	{0,	0,	0,	"apusys_ao-0",		true},
	{0,	1,	1,	"apusys_ao-1",		true},
	{0,	2,	2,	"apusys_ao-2",		true},
	{0,	3,	3,	"apusys_ao-3",		true},
	{0,	4,	4,	"apusys_ao-4",		true},
	{0,	5,	5,	"apusys_ao-5",		true},
	{0,	6,	6,	"md32_apb_s-0",		true},
	{0,	7,	7,	"md32_apb_s-1",		true},
	{0,	8,	8,	"md32_apb_s-2",		true},
	{0,	0,	9,	"NULL_PADDING",		false}, /* padding */
	{0,	9,	10,	"md32_debug_apb",	true},
	{0,	10,	11,	"apu_conn_config",	true},
	{0,	11,	12,	"apu_sctrl_reviser",	true},
	{0,	12,	13,	"apu_sema_stimer",	true},
	{0,	13,	14,	"apu_emi_config",	true},
	{0,	14,	15,	"apu_adl",		true},
	{0,	15,	16,	"apu_edma_lite0",	true},
	{0,	16,	17,	"apu_edma_lite1",	true},
	{0,	17,	18,	"apu_edma0",		true},
	{0,	18,	19,	"apu_edma1",		true},
	{0,	19,	20,	"apu_dapc_ao",		true},
	{0,	20,	21,	"apu_dapc",		true},
	{0,	21,	22,	"infra_bcrm",		true},
	{0,	22,	23,	"apb_dbg_ctl",		true},
	{0,	23,	24,	"noc_dapc",		true},
	{0,	24,	25,	"apu_noc_bcrm",		true},
	{0,	25,	26,	"apu_noc_config",	true},
	{0,	26,	27,	"vpu_core0_config-0",	true},
	{0,	27,	28,	"vpu_core0_config-1",	true},
	{0,	28,	29,	"vpu_core1_config-0",	true},
	{0,	29,	30,	"vpu_core1_config-1",	true},
	{0,	30,	31,	"vpu_core2_config-0",	true},
	{0,	31,	32,	"vpu_core2_config-1",	true},
	{0,	32,	33,	"mdla0_apb-0",		true},
	{0,	33,	34,	"mdla0_apb-1",		true},
	{0,	34,	35,	"mdla0_apb-2",		true},
	{0,	35,	36,	"mdla0_apb-3",		true},
	{0,	36,	37,	"mdla1_apb-0",		true},
	{0,	37,	38,	"mdla1_apb-1",		true},
	{0,	38,	39,	"mdla1_apb-2",		true},
	{0,	39,	40,	"mdla1_apb-3",		true},
	{0,	40,	41,	"apu_iommu0_r0",	true},
	{0,	41,	42,	"apu_iommu0_r1",	true},
	{0,	42,	43,	"apu_iommu0_r2",	true},
	{0,	43,	44,	"apu_iommu0_r3",	true},
	{0,	44,	45,	"apu_iommu0_r4",	true},
	{0,	45,	46,	"apu_iommu1_r0",	true},
	{0,	46,	47,	"apu_iommu1_r1",	true},
	{0,	47,	48,	"apu_iommu1_r2",	true},
	{0,	48,	49,	"apu_iommu1_r3",	true},
	{0,	49,	50,	"apu_iommu1_r4",	true},
	{0,	50,	51,	"apu_rsi0_config",	true},
	{0,	51,	52,	"apu_rsi1_config",	true},
	{0,	52,	53,	"apu_rsi2_config",	true},
	{0,	53,	54,	"apu_ssc0_config",	true},
	{0,	54,	55,	"apu_ssc1_config",	true},
	{0,	55,	56,	"apu_ssc2_config",	true},
	{0,	56,	57,	"vp6_core0_debug_apb",	true},
	{0,	57,	58,	"vp6_core1_debug_apb",	true},
	{0,	58,	59,	"vp6_core2_debug_apb",	true},
};

#endif
