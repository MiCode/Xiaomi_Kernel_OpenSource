/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_DEBUG_H__
#define __APUSYS_DEBUG_H__

#define APUSYS_REG_SIZE (0x100000)
#define APUSYS_BASE (0x19000000)
#define APUSYS_TO_INFRA_BASE (0x10000000)
#define NA	(-1)

#define DBG_MUX_SEL_COUNT (11)
#define TOTAL_DBG_MUX_COUNT (28)
#define DBG_MUX_VPU_START_IDX (15)
#define DBG_MUX_VPU_END_IDX (18)
#define SEGMENT_COUNT (27)


struct dbg_mux_sel_value {
	char name[128];
	int status_reg_offset;
	int dbg_sel[DBG_MUX_SEL_COUNT];
};

struct dbg_mux_sel_info {
	int offset;
	int start_bit;
	int end_bit;
};

struct reg_dump_info {
	char name[128];
	int base;
	int size;
};


static struct dbg_mux_sel_info info_table[DBG_MUX_SEL_COUNT] = {

	{0x29010, 1,  1 }, //vcore_dbg_sel
	{0x29010, 4,  2 }, //vcore_dbg_sel0
	{0x29010, 7,  5 }, //vcore_dbg_sel1
	{0x20138, 2,  0 }, //conn_dbg0_sel
	{0x20138, 11, 9 }, //conn_dbg3_sel
	{0x20138, 14, 12}, //conn_dbg4_sel
	{0x20138, 17, 15}, //conn_dbg5_sel
	{0x20138, 20, 18}, //conn_dbg6_sel
	{0x30a10, 10, 10}, //vpu0_apu_gals_m_ctl_sel
	{0x31a10, 10, 10}, //vpu1_apu_gals_m_ctl_sel
	{0x6E000, 29, 24}, //apu_noc_tip_cfg0
};

static struct dbg_mux_sel_value value_table[TOTAL_DBG_MUX_COUNT] = {

	{"VCORE2EMI_S0_GALS_TX",      0x2901C,
		{ 0,  3, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"VCORE2EMI_S1_GALS_TX",      0x2901C,
		{ 0,  4, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_S0_GALS_RX", 0x2901C,
		{ 1, NA,  3, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_S1_GALS_RX", 0x2901C,
		{ 1, NA,  4, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"XPU2APUSYS_VCORE_GALS_TX",  0x2901C,
		{ 1, NA,  6, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"XPU2APUSYS_VCORE_GALS_RX",  0x2901C,
		{ 0,  6, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"XPU2APUSYS_CONN_GALS_RX",   0x2901C,
		{ 0,  7, NA,  3, NA, NA,  0, NA, NA, NA, NA} },
	{"VPU02CONN_GALS_RX",         0x2901C,
		{ 0,  7, NA,  1,  1, NA, NA, NA, NA, NA, NA} },
	{"VPU12CONN_GALS_RX",         0x2901C,
		{ 0,  7, NA,  1,  2, NA, NA, NA, NA, NA, NA} },
	{"CONN2VPU0_GALS_TX",         0x2901C,
		{ 0,  7, NA,  1,  4, NA, NA, NA, NA, NA, NA} },
	{"CONN2VPU1_GALS_TX",         0x2901C,
		{ 0,  7, NA,  1,  5, NA, NA, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_N0_GALS_TX", 0x2901C,
		{ 0,  7, NA,  3, NA, NA,  3, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_N1_GALS_TX", 0x2901C,
		{ 0,  7, NA,  3, NA, NA,  4, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_S0_GALS_TX", 0x2901C,
		{ 0,  7, NA,  3, NA, NA,  1, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_S1_GALS_TX", 0x2901C,
		{ 0,  7, NA,  3, NA, NA,  2, NA, NA, NA, NA} },
	{"VPU02CONN_GALS_TX",         0x30A28,
		{NA, NA, NA, NA, NA, NA, NA, NA,  0, NA, NA} },
	{"CONN2VPU0_GALS_RX",         0x30A28,
		{NA, NA, NA, NA, NA, NA, NA, NA,  1, NA, NA} },
	{"VPU12CONN_GALS_TX",         0x31A28,
		{NA, NA, NA, NA, NA, NA, NA, NA, NA,  0, NA} },
	{"CONN2VPU1_GALS_RX",         0x31A28,
		{NA, NA, NA, NA, NA, NA, NA, NA, NA,  1, NA} },
	{"MNOC_TOP_DBG_BUS_OUT_MST0", 0x2901C,
		{ 0,  7, NA,  4, NA, NA, NA,  2, NA, NA,  0} },
	{"MNOC_TOP_DBG_BUS_OUT_MST1", 0x2901C,
		{ 0,  7, NA,  4, NA, NA, NA,  2, NA, NA,  1} },
	{"MNOC_TOP_DBG_BUS_OUT_MST2", 0x2901C,
		{ 0,  7, NA,  4, NA, NA, NA,  2, NA, NA,  2} },
	{"MNOC_TOP_DBG_BUS_OUT_MST3", 0x2901C,
		{ 0,  7, NA,  4, NA, NA, NA,  2, NA, NA,  3} },
	{"MNOC_TOP_DBG_BUS_OUT_SLV0", 0x2901C,
		{ 0,  7, NA,  4, NA, NA, NA,  2, NA, NA,  4} },
	{"MNOC_TOP_DBG_BUS_OUT_SLV1", 0x2901C,
		{ 0,  7, NA,  4, NA, NA, NA,  2, NA, NA,  5} },
	{"MNOC_TOP_DBG_BUS_OUT_SLV2", 0x2901C,
		{ 0,  7, NA,  4, NA, NA, NA,  2, NA, NA,  6} },
	{"MNOC_TOP_DBG_BUS_OUT_SLV3", 0x2901C,
		{ 0,  7, NA,  4, NA, NA, NA,  2, NA, NA,  7} },
	{"MNOC_TOP_DBG_BUS_OUT_SLV4", 0x2901C,
		{ 0,  7, NA,  4, NA, NA, NA,  2, NA, NA,  8} },
};

static struct reg_dump_info range_table[SEGMENT_COUNT] = {

	{"md32_sysCtrl",        0x19001000, 0x800 },
	{"md32_sysCtrl_PMU",    0x19001800, 0x800 },
	{"md32_wdt",            0x19002000, 0x1000},
	{"apu_iommu0_r0",       0x19010000, 0x1000},
	{"apu_iommu0_r1",       0x19011000, 0x1000},
	{"apu_iommu0_r2",       0x19012000, 0x1000},
	{"apu_iommu0_r3",       0x19013000, 0x1000},
	{"apu_iommu0_r4",       0x19014000, 0x1000},
	{"apu_conn_config",     0x19020000, 0x1000},
	{"apu_sctrl_reviser",   0x19021000, 0x1000},
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

#endif
