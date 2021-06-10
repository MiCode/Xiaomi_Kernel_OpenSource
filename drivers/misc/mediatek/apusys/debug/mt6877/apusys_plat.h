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
	{0x20138, 14, 12}, //conn_dbg4_sel
	{0x20138, 17, 15}, //conn_dbg5_sel
	{0x20138, 20, 18}, //conn_dbg6_sel
	{0x20138, 23, 21}, //conn_dbg7_sel
};

#define DBG_MUX_SEL_COUNT (sizeof(info_table)/sizeof(struct dbg_mux_sel_info))

struct dbg_mux_sel_value {
	char name[128];
	int status_reg_offset;
	int dbg_sel[DBG_MUX_SEL_COUNT];
};

struct dbg_mux_sel_value value_table[] = {
	{"VCORE2EMI_N0_GALS_TX",      0x2901C,
		{ 0,  1, NA, NA, NA, NA, NA, NA, NA} },
	{"VCORE2EMI_N1_GALS_TX",      0x2901C,
		{ 0,  2, NA, NA, NA, NA, NA, NA, NA} },
	{"DLA02CONN_M0_GALS_RX",      0x2901C,
		{ 0,  6, NA,  2, NA,  1, NA, NA, NA} },
	{"DLA02CONN_M1_GALS_RX",      0x2901C,
		{ 0,  6, NA,  2, NA,  2, NA, NA, NA} },
	{"MMU0_N0_GALS_TX",           0x2901C,
		{ 0,  6, NA,  3, NA, NA,  1, NA, NA} },
	{"MMU0_N1_GALS_TX",           0x2901C,
		{ 0,  6, NA,  3, NA, NA,  2, NA, NA} },
	{"EDMMA_NOC_0_RX",            0x2901C,
		{ 0,  6, NA,  4, NA, NA, NA,  1, NA} },
	{"MNOC_MISC_0_DBG_BUS",       0x2901C,
		{ 0,  6, NA,  5, NA, NA, NA, NA,  1} },
	{"APU_UP_SYS_DBG_BUS",        0x2901C,
		{ 0,  6, NA,  5, NA, NA, NA, NA,  4} },
	{"APUSYS_AO_DBG_PCU",         0x2901C,
		{ 1, NA,  7, NA, NA, NA, NA, NA, NA} },
	{"APUSYS_AO_DBG_RPC",         0x2901C,
		{ 1, NA,  7, NA, NA, NA, NA, NA, NA} },
};

#define TOTAL_DBG_MUX_COUNT \
	(sizeof(value_table)/sizeof(struct dbg_mux_sel_value))

struct reg_dump_info {
	char name[128];
	u32 base;
	u32 size;
};

struct reg_dump_info range_table[] = {
	{"apusys_ao-0", 0x190f0000, 0x1000},
	{"apusys_ao-1", 0x190f1000, 0x1000},
	{"apusys_ao-2", 0x190f2000, 0x1000},
	{"apusys_ao-3", 0x190f3000, 0x1000},
	{"apusys_ao-4", 0x190f4000, 0x800},
	{"apusys_ao-5", 0x190f4800, 0x800},
	{"apusys_ao-6", 0x19029000, 0x1000},
	{"apusys_ao-8", 0x190f8000, 0x400},
	{"apusys_ao-9", 0x190fc000, 0x400},
	{"md32_apb_s-0", 0x19001000, 0x800},
	{"md32_apb_s-1", 0x19002000, 0x1000},
	{"noc_axi", 0x1d000000, 0x0},
	{"apu_con2_config", 0x19020000, 0x1000},
	{"apu_con1_config", 0x19024000, 0x1000},
	{"apu_sema_stimer", 0x19022000, 0x1000},
	{"apu_emi_config", 0x19023000, 0x1000},
	{"apu_edma0", 0x19025000, 0x1000},
	{"apu_dapc", 0x19064000, 0x1000},
	{"infra_bcrm", 0x19065000, 0x1000},
	{"infra_ao_bcrm", 0x19067000, 0x1000},
	{"noc_dapc", 0x1906c000, 0x1000},
	{"apu_noc_bcrm", 0x1906d000, 0x1000},
	{"apu_noc_config_0", 0x1906e000, 0x2000},
	{"vpu_core0_config-0", 0x19030000, 0x800},
	{"vpu_core0_config-1", 0x19030800, 0x800},
	{"vpu_core1_config-0", 0x19031000, 0x800},
	{"vpu_core1_config-1", 0x19031800, 0x800},
	{"mdla0_apb-0", 0x19034000, 0x1000},
	{"mdla0_apb-1", 0x19035000, 0x800},
	{"mdla0_apb-2", 0x19035800, 0x800},
	{"mdla0_apb-3", 0x19036000, 0x2000},
	{"apu_iommu0_r0", 0x19010000, 0x1000},
	{"apu_n0_ssc_config", 0x1901e000, 0x1000},
};

#define SEGMENT_COUNT (sizeof(range_table)/sizeof(struct reg_dump_info))

#endif
