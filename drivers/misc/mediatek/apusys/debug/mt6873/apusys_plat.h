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

#ifndef __APUSYS_MIDWARE_PLATFORM_H__
#define __APUSYS_MIDWARE_PLATFORM_H__

extern struct dentry *mdw_dbg_root;
#define APUSYS_VLM_START 0x1D800000 // tcm tmp
#define APUSYS_VLM_SIZE 0x100000
#define APUSYS_REG_SIZE (0x100000)
#define APUSYS_BASE (0x19000000)
#define INFRA_BASE (0x10000000)
#define INFRA_SIZE (0x10000)
#define NA	(-1)

char *reg_all_mem;
bool apusys_dump_force;
static void *apu_top;
static void *apu_to_infra_top;

struct dbg_mux_sel_info {
	int offset;
	int start_bit;
	int end_bit;
};

struct dbg_mux_sel_info info_table[] = {
	{0x29010, 1,  1 }, //vcore_dbg_sel
	{0x29010, 4,  2 }, //vcore_dbg_sel0
	{0x29010, 7,  5 }, //vcore_dbg_sel1
	{0x20138, 2,  0 }, //conn_dbg0_sel
	{0x20138, 11, 9 }, //conn_dbg3_sel
	{0x20138, 20, 18}, //conn_dbg6_sel
	{0x01098, 15, 8 }, //edma_up_dbg_bus_sel
	{0x30a10, 10, 10}, //vpu0_apu_gals_m_ctl_sel
	{0x31a10, 10, 10}, //vpu1_apu_gals_m_ctl_sel
};

#define DBG_MUX_SEL_COUNT (sizeof(info_table)/sizeof(struct dbg_mux_sel_info))

struct dbg_mux_sel_value {
	char name[128];
	int status_reg_offset;
	int dbg_sel[DBG_MUX_SEL_COUNT];
};

struct dbg_mux_sel_value value_table[] = {
	{"VCORE2EMI_S0_GALS_TX",      0x2901C,
		{ 0,  3, NA, NA, NA, NA, NA, NA, NA} },
	{"VCORE2EMI_S1_GALS_TX",      0x2901C,
		{ 0,  4, NA, NA, NA, NA, NA, NA, NA} },
	{"APUSYS2ACP_VCORE_GALS_TX",  0x2901C,
		{ 0,  5, NA, NA, NA, NA, NA, NA, NA} },
	{"XPU2APUSYS_VCORE_GALS_RX",  0x2901C,
		{ 0,  6, NA, NA, NA, NA, NA, NA, NA} },
	{"MNOC_MISC_1/0_DBG_BUS",     0x2901C,
		{ 0,  7, NA,  4, NA,  2, NA, NA, NA} },
	{"MNOC_MISC_3/2_DBG_BUS",     0x2901C,
		{ 0,  7, NA,  4, NA,  3, NA, NA, NA} },
	{"MNOC_MISC_4_DBG_BUS",       0x2901C,
		{ 0,  7, NA,  4, NA,  4, NA, NA, NA} },
	{"APU_UP_SYS_DBG_BUS",        0x2901C,
		{ 0,  7, NA,  4, NA,  5,  1, NA, NA} },
	{"APU_EDMA_0_DBG_BUS_0",      0x2901C,
		{ 0,  7, NA,  4, NA,  5,  2, NA, NA} },
	{"APU_EDMA_0_DBG_BUS_1",      0x2901C,
		{ 0,  7, NA,  4, NA,  5,  3, NA, NA} },
	{"CONN2VCORE_EMI_S0_GALS_RX", 0x2901C,
		{ 1, NA,  3, NA, NA, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_S1_GALS_RX", 0x2901C,
		{ 1, NA,  4, NA, NA, NA, NA, NA, NA} },
	{"APUSYS2ACP_VCORE_GALS_RX",  0x2901C,
		{ 1, NA,  5, NA, NA, NA, NA, NA, NA} },
	{"XPU2APUSYS_VCORE_GALS_TX",  0x2901C,
		{ 1, NA,  6, NA, NA, NA, NA, NA, NA} },
};

#define TOTAL_DBG_MUX_COUNT \
	(sizeof(value_table)/sizeof(struct dbg_mux_sel_value))

struct reg_dump_info {
	char name[128];
	u32 base;
	u32 size;
};

struct reg_dump_info range_table[] = {
	{"md32_sysCtrl",        0x19001000, 0x800 },
	{"md32_sysCtrl_PMU",    0x19001800, 0x800 },
	{"md32_wdt",            0x19002000, 0x1000},
	{"apu_iommu0_r0",       0x19010000, 0x1000},
	{"apu_iommu0_r1",       0x19011000, 0x1000},
	{"apu_iommu0_r2",       0x19012000, 0x1000},
	{"apu_iommu0_r3",       0x19013000, 0x1000},
	{"apu_iommu0_r4",       0x19014000, 0x1000},
	{"apu_xpu_rsi_config",  0x1901C000, 0x1000},
	{"apu_acp_ssc_config",  0x1901F000, 0x1000},
	{"apu_conn_config",     0x19020000, 0x1000},
	{"apu_sema_stimer",     0x19022000, 0x1000},
	{"emi_config",          0x19023000, 0x1000},
	{"apb_dapc_wrapper",    0x19064000, 0x1000},
	{"infra_bcrm",          0x19065000, 0x1000},
	{"apb_debug_ctl",       0x19066000, 0x1000},
	{"noc_dapc_wrapper",    0x1906C000, 0x1000},
	{"apu_noc_bcrm",        0x1906D000, 0x1000},
	{"apu_noc_config_0",    0x1906E000, 0x2000},
	{"apu_noc_config_1",    0x19070000, 0x2000},
	{"apu_noc_config_2",    0x19072000, 0x2000},
	{"apu_noc_config_3",    0x19074000, 0x2000},
	{"apu_noc_config_4",    0x19076000, 0x2000},
	{"apu_rpc(CPC)",        0x190F0000, 0x1000},
	{"apu_rpc(DVFS)",       0x190F1000, 0x1000},
	{"apu_ao_ctrl",         0x190F2000, 0x1000},
	{"apb_dapc_ap_wrapper", 0x190F8000, 0x4000},
	{"noc_dapc_ap_wrapper", 0x190FC000, 0x4000},
};

#define SEGMENT_COUNT (sizeof(range_table)/sizeof(struct reg_dump_info))

#endif
