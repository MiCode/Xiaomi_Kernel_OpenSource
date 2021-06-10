/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_DEBUG_H__
#define __APUSYS_DEBUG_H__

#define APUSYS_REG_SIZE (0x100000)
#define APUSYS_BASE (0x19000000)
#define NA	(-1)
#define NO_READ_VALUE (0xcdcdcdcd)
#define PLATFORM_COUNT (3)

struct dbg_mux_sel_value {
	char name[128];
	int status_reg_offset;
	int dbg_sel[13];
	//Max of MUX_SEL_COUNT_MT6853, MUX_SEL_COUNT_MT6873, MUX_SEL_COUNT_MT6885
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


struct dbg_hw_info {
	int mux_sel_count;
	int total_mux_ount;
	int seg_count;

	struct dbg_mux_sel_info *mux_sel_tbl;
	struct dbg_mux_sel_value *value_tbl;
	struct reg_dump_info *range_tbl;
};

//MT6853
static struct dbg_mux_sel_info info_table_mt6853[] = {
	{0x29010, 1,  1 }, //vcore_dbg_sel
	{0x29010, 4,  2 }, //vcore_dbg_sel0
	{0x29010, 7,  5 }, //vcore_dbg_sel1
	{0x20138, 2,  0 }, //conn_dbg0_sel
	{0x20138, 11, 9 }, //conn_dbg3_sel
	{0x20138, 14, 12}, //conn_dbg4_sel
	{0x20138, 17, 15}, //conn_dbg5_sel
	{0x20138, 20, 18}, //conn_dbg6_sel
	{0x6E000, 29, 24}, //apu_noc_tip_cfg0
};

#define MUX_SEL_COUNT_MT6853 \
	(sizeof(info_table_mt6853) / sizeof(struct dbg_mux_sel_info))

static struct dbg_mux_sel_value value_table_mt6853[] = {

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
};

#define TOTAL_MUX_COUNT_MT6853 \
	(sizeof(value_table_mt6853) / sizeof(struct dbg_mux_sel_value))

static struct reg_dump_info range_table_mt6853[] = {

	{"md32_sysCtrl",        0x19001000, 0x800 },
	{"md32_sysCtrl_PMU",    0x19001800, 0x800 },
	{"md32_wdt",            0x19002000, 0x1000},
	{"apu_iommu0_r0",       0x19010000, 0x1000},
	{"apu_iommu0_r1",       0x19011000, 0x1000},
	{"apu_iommu0_r2",       0x19012000, 0x1000},
	{"apu_iommu0_r3",       0x19013000, 0x1000},
	{"apu_iommu0_r4",       0x19014000, 0x1000},
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
#define SEGMENT_COUNT_MT6853 \
	(sizeof(range_table_mt6853) / sizeof(struct reg_dump_info))

//MT6873

static struct dbg_mux_sel_info info_table_mt6873[] = {

	{0x29010, 1,  1 }, //vcore_dbg_sel
	{0x29010, 4,  2 }, //vcore_dbg_sel0
	{0x29010, 7,  5 }, //vcore_dbg_sel1
	{0x20138, 2,  0 }, //conn_dbg0_sel
	{0x20138, 11, 9 }, //conn_dbg3_sel
	{0x20138, 20, 18}, //conn_dbg6_sel
	{0x01098, 15, 8 }, //edma_up_dbg_bus_sel
};
#define MUX_SEL_COUNT_MT6873 \
	(sizeof(info_table_mt6873) / sizeof(struct dbg_mux_sel_info))

static struct dbg_mux_sel_value value_table_mt6873[] = {

	{"VCORE2EMI_S0_GALS_TX",      0x2901C,
		{ 0,  3, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"VCORE2EMI_S1_GALS_TX",      0x2901C,
		{ 0,  4, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"APUSYS2ACP_VCORE_GALS_TX",  0x2901C,
		{ 0,  5, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"XPU2APUSYS_VCORE_GALS_RX",  0x2901C,
		{ 0,  6, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"VPU02CONN_GALS_RX",         0x2901C,
		{ 0,  7, NA,  1,  1, NA, NA, NA, NA, NA, NA} },
	{"VPU12CONN_GALS_RX",         0x2901C,
		{ 0,  7, NA,  1,  2, NA, NA, NA, NA, NA, NA} },
	{"CONN2VPU0_GALS_TX",         0x2901C,
		{ 0,  7, NA,  1,  4, NA, NA, NA, NA, NA, NA} },
	{"CONN2VPU1_GALS_TX",         0x2901C,
		{ 0,  7, NA,  1,  5, NA, NA, NA, NA, NA, NA} },
	{"MNOC_MISC_1/0_DBG_BUS",     0x2901C,
		{ 0,  7, NA,  4, NA,  2, NA, NA, NA, NA, NA} },
	{"MNOC_MISC_3/2_DBG_BUS",     0x2901C,
		{ 0,  7, NA,  4, NA,  3, NA, NA, NA, NA, NA} },
	{"MNOC_MISC_4_DBG_BUS",       0x2901C,
		{ 0,  7, NA,  4, NA,  4, NA, NA, NA, NA, NA} },
	{"APU_UP_SYS_DBG_BUS",        0x2901C,
		{ 0,  7, NA,  4, NA,  5,  1, NA, NA, NA, NA} },
	{"APU_EDMA_0_DBG_BUS_0",      0x2901C,
		{ 0,  7, NA,  4, NA,  5,  2, NA, NA, NA, NA} },
	{"APU_EDMA_0_DBG_BUS_1",      0x2901C,
		{ 0,  7, NA,  4, NA,  5,  3, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_S0_GALS_RX", 0x2901C,
		{ 1, NA,  3, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_S1_GALS_RX", 0x2901C,
		{ 1, NA,  4, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"APUSYS2ACP_VCORE_GALS_RX",  0x2901C,
		{ 1, NA,  5, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"XPU2APUSYS_VCORE_GALS_TX",  0x2901C,
		{ 1, NA,  6, NA, NA, NA, NA, NA, NA, NA, NA} },
};
#define TOTAL_MUX_COUNT_MT6873 \
	(sizeof(value_table_mt6873) / sizeof(struct dbg_mux_sel_value))

static struct reg_dump_info range_table_mt6873[] = {

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
#define SEGMENT_COUNT_MT6873 \
	(sizeof(range_table_mt6873) / sizeof(struct reg_dump_info))

//MT6885
#define MUX_VPU_START_IDX_MT6885 (29)
#define MUX_VPU_END_IDX_MT6885 (34)


static struct dbg_mux_sel_info info_table_mt6885[] = {

	{0x29010, 1,  1 }, //vcore_dbg_sel
	{0x29010, 4,  2 }, //vcore_dbg_sel0
	{0x29010, 7,  5 }, //vcore_dbg_sel1
	{0x20138, 2,  0 }, //conn_dbg0_sel
	{0x20138, 11, 9 }, //conn_dbg3_sel
	{0x20138, 14, 12}, //conn_dbg4_sel
	{0x20138, 17, 15}, //conn_dbg5_sel
	{0x20138, 20, 18}, //conn_dbg6_sel
	{0x34130, 10, 10}, //mdla0_axi_gals_dbg_sel
	{0x38130, 10, 10}, //mdla1_axi_gals_dbg_sel
	{0x30a10, 10, 10}, //vpu0_apu_gals_m_ctl_sel
	{0x31a10, 10, 10}, //vpu1_apu_gals_m_ctl_sel
	{0x32a10, 10, 10}, //vpu2_apu_gals_m_ctl_sel
};
#define MUX_SEL_COUNT_MT6885 \
	(sizeof(info_table_mt6885) / sizeof(struct dbg_mux_sel_info))

static struct dbg_mux_sel_value value_table_mt6885[] = {

	{"VCORE2EMI_N0_GALS_TX",      0x2901C,
		{ 0,  1, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"VCORE2EMI_N1_GALS_TX",      0x2901C,
		{ 0,  2, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"VCORE2EMI_S0_GALS_TX",      0x2901C,
		{ 0,  3, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"VCORE2EMI_S1_GALS_TX",      0x2901C,
		{ 0,  4, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_N0_GALS_RX", 0x2901C,
		{ 1, NA,  1, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_N1_GALS_RX", 0x2901C,
		{ 1, NA,  2, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_S0_GALS_RX", 0x2901C,
		{ 1, NA,  3, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_S1_GALS_RX", 0x2901C,
		{ 1, NA,  4, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"XPU2APUSYS_VCORE_GALS_TX",  0x2901C,
		{ 1, NA,  6, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"XPU2APUSYS_VCORE_GALS_RX",  0x2901C,
		{ 0,  6, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"XPU2APUSYS_CONN_GALS_RX",   0x2901C,
		{ 0,  7, NA,  3, NA, NA,  0, NA, NA, NA, NA, NA, NA} },
	{"VPU02CONN_GALS_RX",         0x2901C,
		{ 0,  7, NA,  1,  1, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"VPU12CONN_GALS_RX",         0x2901C,
		{ 0,  7, NA,  1,  2, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"VPU22CONN_GALS_RX",         0x2901C,
		{ 0,  7, NA,  1,  3, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"CONN2VPU0_GALS_TX",         0x2901C,
		{ 0,  7, NA,  1,  4, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"CONN2VPU1_GALS_TX",         0x2901C,
		{ 0,  7, NA,  1,  5, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"CONN2VPU2_GALS_TX",         0x2901C,
		{ 0,  7, NA,  1,  0, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"MDLA0M02CONN_GALS_RX",      0x2901C,
		{ 0,  7, NA,  2, NA,  1, NA, NA, NA, NA, NA, NA, NA} },
	{"MDLA0M12CONN_GALS_RX",      0x2901C,
		{ 0,  7, NA,  2, NA,  2, NA, NA, NA, NA, NA, NA, NA} },
	{"MDLA1M02CONN_GALS_RX",      0x2901C,
		{ 0,  7, NA,  2, NA,  3, NA, NA, NA, NA, NA, NA, NA} },
	{"MDLA1M12CONN_GALS_RX",      0x2901C,
		{ 0,  7, NA,  2, NA,  0, NA, NA, NA, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_N0_GALS_TX", 0x2901C,
		{ 0,  7, NA,  3, NA, NA,  3, NA, NA, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_N1_GALS_TX", 0x2901C,
		{ 0,  7, NA,  3, NA, NA,  4, NA, NA, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_S0_GALS_TX", 0x2901C,
		{ 0,  7, NA,  3, NA, NA,  1, NA, NA, NA, NA, NA, NA} },
	{"CONN2VCORE_EMI_S1_GALS_TX", 0x2901C,
		{ 0,  7, NA,  3, NA, NA,  2, NA, NA, NA, NA, NA, NA} },
	{"MDLA0M02CONN_GALS_TX",      0x3413C,
		{NA, NA, NA, NA, NA, NA, NA, NA,  0, NA, NA, NA, NA} },
	{"MDLA0M12CONN_GALS_TX",      0x3413C,
		{NA, NA, NA, NA, NA, NA, NA, NA,  1, NA, NA, NA, NA} },
	{"MDLA1M02CONN_GALS_TX",      0x3813C,
		{NA, NA, NA, NA, NA, NA, NA, NA, NA,  0, NA, NA, NA} },
	{"MDLA1M12CONN_GALS_TX",      0x3813C,
		{NA, NA, NA, NA, NA, NA, NA, NA, NA,  1, NA, NA, NA} },
	{"APUSYS2ACP_VCORE_GALS_TX",  0x2901C,
		{ 0,  5, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"APUSYS2ACP_VCORE_GALS_RX",  0x2901C,
		{ 1, NA,  5, NA, NA, NA, NA, NA, NA, NA, NA, NA, NA} },
	{"APUSYS2ACP_CONN_GALS_TX",   0x2901C,
		{ 0,  7, NA,  3, NA, NA,  5, NA, NA, NA, NA, NA, NA} },
};
#define TOTAL_MUX_COUNT_MT6885 \
	(sizeof(value_table_mt6885) / sizeof(struct dbg_mux_sel_value))

static struct reg_dump_info range_table_mt6885[] = {

	{"md32_sysCtrl",         0x19001000, 0x800 },
	{"md32_sysCtrl_PMU",     0x19001800, 0x800 },
	{"md32_wdt",             0x19002000, 0x1000},
	{"apu_iommu0_r0",        0x19010000, 0x1000},
	{"apu_iommu0_r1",        0x19011000, 0x1000},
	{"apu_iommu0_r2",        0x19012000, 0x1000},
	{"apu_iommu0_r3",        0x19013000, 0x1000},
	{"apu_iommu0_r4",        0x19014000, 0x1000},
	{"apu_iommu1_r0",        0x19015000, 0x1000},
	{"apu_iommu1_r1",        0x19016000, 0x1000},
	{"apu_iommu1_r2",        0x19017000, 0x1000},
	{"apu_iommu1_r3",        0x19018000, 0x1000},
	{"apu_iommu1_r4",        0x19019000, 0x1000},
	{"apu_south_rsi_config", 0x1901A000, 0x1000},
	{"apu_north_rsi_config", 0x1901B000, 0x1000},
	{"apu_xpu_rsi_config",   0x1901C000, 0x1000},
	{"apu_south_ssc_config", 0x1901D000, 0x1000},
	{"apu_north_ssc_config", 0x1901E000, 0x1000},
	{"apu_apcssc_config",    0x1901F000, 0x1000},
	{"apu_conn_config",      0x19020000, 0x1000},
	{"apu_sema_stimer",      0x19022000, 0x1000},
	{"emi_config",           0x19023000, 0x1000},
	{"apu_adl",              0x19024000, 0x1000},
	{"apu_edma_lite0",       0x19025000, 0x1000},
	{"apu_edma_lite1",       0x19026000, 0x1000},
	{"apu_edma0",            0x19027000, 0x1000},
	{"apu_edma1",            0x19028000, 0x1000},
	{"apu_Vcore_config",     0x19029000, 0x1000},
	{"apb_dapc_wrapper",     0x19064000, 0x1000},
	{"infra_bcrm",           0x19065000, 0x1000},
	{"apb_debug_ctl",        0x19066000, 0x1000},
	{"noc_dapc_wrapper",     0x1906C000, 0x1000},
	{"apu_noc_bcrm",         0x1906D000, 0x1000},
	{"apu_noc_config_0",     0x1906E000, 0x2000},
	{"apu_rpc(CPC)",         0x190F0000, 0x1000},
	{"apu_rpc(DVFS)",        0x190F1000, 0x1000},
	{"apb_dapc_ap_wrapper",  0x190F8000, 0x4000},
	{"noc_dapc_ap_wrapper",  0x190FC000, 0x4000},
};
#define SEGMENT_COUNT_MT6885 \
	(sizeof(range_table_mt6885) / sizeof(struct reg_dump_info))

static struct dbg_hw_info hw_info_set[PLATFORM_COUNT] = {
	{MUX_SEL_COUNT_MT6853,
	 TOTAL_MUX_COUNT_MT6853,
	 SEGMENT_COUNT_MT6853,
	 info_table_mt6853,
	 value_table_mt6853,
	 range_table_mt6853},

	{MUX_SEL_COUNT_MT6873,
	 TOTAL_MUX_COUNT_MT6873,
	 SEGMENT_COUNT_MT6873,
	 info_table_mt6873,
	 value_table_mt6873,
	 range_table_mt6873},

	{MUX_SEL_COUNT_MT6885,
	 TOTAL_MUX_COUNT_MT6885,
	 SEGMENT_COUNT_MT6885,
	 info_table_mt6885,
	 value_table_mt6885,
	 range_table_mt6885},
};


#endif
