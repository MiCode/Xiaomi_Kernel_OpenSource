/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __MTK_SWPM_PLATFORM_H__
#define __MTK_SWPM_PLATFORM_H__

#include <mtk_gpu_swpm_plat.h>
#include <mtk_isp_swpm_plat.h>
#include <mtk_me_swpm_plat.h>

#define SWPM_TEST (0)

#define GET_UW_LKG                              (1)
#define MAX_RECORD_CNT				(64)
#define MAX_APHY_CORE_PWR			(12)
#define MAX_APHY_OTHERS_PWR			(16)
#define DEFAULT_LOG_INTERVAL_MS			(1000)
/* VPROC2 + VPROC1 + VDRAM + VGPU + VCORE */
#define DEFAULT_LOG_MASK			(0x1F)

#define POWER_INDEX_CHAR_SIZE			(4096)

#define NR_CORE_VOLT				(5)
#define NR_CPU_OPP				(16)
#define NR_CPU_CORE				(8)
#define NR_CPU_L_CORE				(6)

#define ALL_METER_TYPE				(0xFFFF)
#define EN_POWER_METER_ONLY			(0x1)
#define EN_POWER_METER_ALL			(0x3)

/* data shared w/ SSPM */
enum profile_point {
	MON_TIME,
	CALC_TIME,
	REC_TIME,
	TOTAL_TIME,

	NR_PROFILE_POINT
};

enum power_meter_type {
	CPU_POWER_METER,
	GPU_POWER_METER,
	CORE_POWER_METER,
	MEM_POWER_METER,
	ISP_POWER_METER,
	ME_POWER_METER,

	NR_POWER_METER
};

enum power_rail {
	VPROC2,
	VPROC1,
	VGPU,
	VCORE,
	VDRAM,
	VIO18_DDR,
	VIO18_DRAM,

	NR_POWER_RAIL
};

enum ddr_freq {
	DDR_400,
	DDR_600,
	DDR_800,
	DDR_933,
	DDR_1333,
	DDR_1866,
	DDR_2133,
	DDR_2750,

	NR_DDR_FREQ
};

enum aphy_core_pwr_type {
	APHY_VCORE,
	NR_APHY_CORE_PWR_TYPE
};

enum aphy_other_pwr_type {
	/* APHY_VCORE, independent */
	APHY_VDDQ_0P6V,
	APHY_VM_0P75V,
	APHY_VIO_1P2V,
	/* APHY_VIO_1P8V, */

	NR_APHY_OTHERS_PWR_TYPE
};

enum dram_pwr_type {
	DRAM_VDD1_1P8V,
	DRAM_VDD2_1P1V,
	DRAM_VDDQ_0P6V,

	NR_DRAM_PWR_TYPE
};

enum cpu_type {
	CPU_TYPE_L,
	CPU_TYPE_B,

	NR_CPU_TYPE
};

enum cpu_lkg_type {
	CPU_L_LKG,
	CPU_B_LKG,
	DSU_LKG,

	NR_CPU_LKG_TYPE
};

enum pmu_idx {
	PMU_IDX_L3DC,
	PMU_IDX_INST_SPEC,
	PMU_IDX_CYCLES,
	PMU_IDX_NON_WFX_COUNTER, /* put non-WFX counter here */

	MAX_PMU_CNT
};

enum cpu_core_power_state {
	CPU_CORE_ACTIVE,
	CPU_CORE_IDLE,
	CPU_CORE_POWER_OFF,

	NR_CPU_CORE_POWER_STATE
};

enum cpu_cluster_power_state {
	CPU_CLUSTER_ACTIVE,
	CPU_CLUSTER_IDLE,
	CPU_CLUSTER_POWER_OFF,

	NR_CPU_CLUSTER_POWER_STATE
};

enum mcusys_power_state {
	MCUSYS_ACTIVE,
	MCUSYS_IDLE,
	MCUSYS_POWER_OFF,

	NR_MCUSYS_POWER_STATE
};

enum cpu_pwr_type {
	CPU_PWR_TYPE_L,
	CPU_PWR_TYPE_B,
	CPU_PWR_TYPE_DSU,
	CPU_PWR_TYPE_MCUSYS,

	NR_CPU_PWR_TYPE
};

/* cpu voltage/freq index */
struct cpu_swpm_vf_index {
	unsigned int cpu_volt_mv[NR_CPU_TYPE];
	unsigned int cpu_freq_mhz[NR_CPU_TYPE];
	unsigned int cpu_opp[NR_CPU_TYPE];
	unsigned int cci_volt_mv;
	unsigned int cci_freq_mhz;
	unsigned int cci_opp;
};
/* cpu power index structure */
struct cpu_swpm_index {
	/* for calculation */
	unsigned int core_state_ratio[NR_CPU_CORE_POWER_STATE][NR_CPU_CORE];
	unsigned int cpu_stall_ratio[NR_CPU_CORE];
	unsigned int cluster_state_ratio[NR_CPU_CLUSTER_POWER_STATE];
	unsigned int mcusys_state_ratio[NR_MCUSYS_POWER_STATE];
	unsigned int pmu_val[MAX_PMU_CNT][NR_CPU_CORE];
	unsigned int l3_bw;
	unsigned int cpu_emi_bw;
	struct cpu_swpm_vf_index vf;
	unsigned int cpu_lkg[NR_CPU_LKG_TYPE];
	unsigned int thermal[NR_CPU_LKG_TYPE];
	unsigned int cpu_pwr[NR_CPU_PWR_TYPE];
};

/* TODO: infra power state for core power */
enum infra_power_state {
	INFRA_DATA_ACTIVE,
	INFRA_CMD_ACTIVE,
	INFRA_IDLE,
	INFRA_DCM,

	NR_INFRA_POWER_STATE
};

enum core_lkg_type {
	CORE_LKG_SOC_ON,
	CORE_LKG_INFRA_TOP,
	CORE_LKG_DRAMC,
	CORE_LKG_MMSYS,
	CORE_LKG_VDEC,
	CORE_LKG_VENC,
	CORE_LKG_IMGSYS,
	CORE_LKG_CAMSYS,
	CORE_LKG_CONNSYS,
	CORE_LKG_IPESYS,

	NR_CORE_LKG_TYPE
};

enum core_lkg_rec_type {
	CORE_LKG_REC_SOC_ON,
	CORE_LKG_REC_INFRA_TOP,
	CORE_LKG_REC_DRAMC,
	CORE_LKG_REC_MMSYS,
	CORE_LKG_REC_VDEC,
	CORE_LKG_REC_VENC,

	NR_CORE_LKG_REC_TYPE
};

/* sync with mt6885 emi in sspm */
#define MAX_EMI_NUM (1)
/* core voltage/freq index */
struct core_swpm_vf_index {
	unsigned int vcore_mv;
	unsigned int ddr_freq_mhz;
};
/* core lkg index */
struct core_swpm_lkg_index {
	unsigned int core_lkg_pwr[NR_CORE_LKG_REC_TYPE];
	unsigned int thermal;
};
/* core power index structure */
struct core_swpm_index {
	/* for calculation */
	unsigned int infra_state_ratio[NR_INFRA_POWER_STATE];
	unsigned int read_bw[MAX_EMI_NUM];
	unsigned int write_bw[MAX_EMI_NUM];
	struct core_swpm_vf_index vf;
	struct core_swpm_lkg_index lkg;
};

/* dram voltage/freq index */
struct mem_swpm_vf_index {
	unsigned int ddr_freq_mhz;
};
/* dram power index structure */
struct mem_swpm_index {
	/* for calculation */
	unsigned int read_bw[MAX_EMI_NUM];
	unsigned int write_bw[MAX_EMI_NUM];
	unsigned int srr_pct;			/* self refresh rate */
	unsigned int pdir_pct[MAX_EMI_NUM];	/* power-down idle rate */
	unsigned int phr_pct[MAX_EMI_NUM];	/* page-hit rate */
	unsigned int acc_util[MAX_EMI_NUM];	/* accumulate EMI utilization */
	unsigned int trans[MAX_EMI_NUM];	/* transaction count */
	unsigned int ddr_ratio[MAX_EMI_NUM];	/* ddr_ratio */
	unsigned int index_in_us;		/* data collection in us */
	unsigned int mr4;
	struct mem_swpm_vf_index vf;
};

struct share_index {
	struct cpu_swpm_index cpu_idx;
	struct core_swpm_index core_idx;
	struct mem_swpm_index mem_idx;
	struct gpu_swpm_index gpu_idx;
	struct isp_swpm_index isp_idx;
	struct me_swpm_index me_idx;
	unsigned int window_cnt;
};

struct share_ctrl {
	unsigned int lock;
	unsigned int clear_flag;
};

struct share_wrap {
	unsigned int share_index_addr;
	unsigned int share_ctrl_addr;
	unsigned int share_index_ext_addr;
	unsigned int share_ctrl_ext_addr;
};

struct aphy_core_bw_data {
	unsigned short bw[MAX_APHY_CORE_PWR];
};

struct aphy_core_pwr {
	unsigned short read_coef[MAX_APHY_CORE_PWR];
	unsigned short write_coef[MAX_APHY_CORE_PWR];
};


struct aphy_others_bw_data {
	unsigned short bw[MAX_APHY_OTHERS_PWR];
};

struct aphy_others_pwr {
	unsigned short read_coef[MAX_APHY_OTHERS_PWR];
	unsigned short write_coef[MAX_APHY_OTHERS_PWR];
};

/* unit: uW / V^2 */
struct aphy_core_pwr_data {
	struct aphy_core_pwr pwr[NR_DDR_FREQ];
	unsigned short coef_idle[NR_DDR_FREQ];
};

struct aphy_others_pwr_data {
	struct aphy_others_pwr pwr[NR_DDR_FREQ];
	unsigned short coef_idle[NR_DDR_FREQ];
};

/* unit: uA */
struct dram_pwr_conf {
	unsigned int i_dd0;
	unsigned int i_dd2p;
	unsigned int i_dd2n;
	unsigned int i_dd4r;
	unsigned int i_dd4w;
	unsigned int i_dd5;
	unsigned int i_dd6;
};

/* numbers of unsigned int for mem reserved memory */
#define MEM_SWPM_RESERVED_SIZE (500)

/* mem share memory data structure - 1940/2000 bytes */
struct mem_swpm_rec_data {
	/* 2(short) * 8(ddr_opp) = 16 bytes */
	unsigned short ddr_opp_freq[NR_DDR_FREQ];

	/* 2(short) * 16(sample point) * 8(opp_num) = 256 bytes */
	struct aphy_others_bw_data aphy_others_bw_tbl[NR_DDR_FREQ];

	/* 2(short) * 3(pwr_type) */
	/* * ((16+16)(r/w_coef) * 8(opp) + 8(idle)) = 1584 bytes */
	struct aphy_others_pwr_data
		aphy_others_pwr_tbl[NR_APHY_OTHERS_PWR_TYPE];

	/* 4(int) * 3(pwr_type) * 7 = 84 bytes */
	struct dram_pwr_conf dram_conf[NR_DRAM_PWR_TYPE];
};

/* numbers of unsigned int for core reserved memory */
#define CORE_SWPM_RESERVED_SIZE (208)

/* core share memory data structure - 826/832 bytes */
struct core_swpm_rec_data {
	/* 2(short) * 5(core_volt) = 10 bytes */
	unsigned short core_volt_tbl[NR_CORE_VOLT];

	/* 2(short) * 12(sample point) * 8(opp_num) = 192 bytes */
	struct aphy_core_bw_data aphy_core_bw_tbl[NR_DDR_FREQ];

	/* 2(short) * 1(pwr_type) */
	/* * ((12+12)(r/w_coef) * 8(opp) + 8(idle)) = 400 bytes */
	struct aphy_core_pwr_data
		aphy_core_pwr_tbl[NR_APHY_CORE_PWR_TYPE];

	/* 4 (int) * 5(core_volt) * 11(core_lkg_type) = 220 bytes */
	unsigned int core_lkg_pwr[NR_CORE_VOLT][NR_CORE_LKG_TYPE];

	/* 4 (int) * 1 = 4 bytes */
	unsigned int thermal;
};

struct swpm_rec_data {
	/* 8 bytes */
	unsigned int cur_idx;
	unsigned int profile_enable;

	/* 8(long) * 5(prof_pt) * 3 = 120 bytes */
	unsigned long long avg_latency[NR_PROFILE_POINT];
	unsigned long long max_latency[NR_PROFILE_POINT];
	unsigned long long prof_cnt[NR_PROFILE_POINT];

	/* 4(int) * 64(rec_cnt) * 7 = 1792 bytes */
	unsigned int pwr[NR_POWER_RAIL][MAX_RECORD_CNT];

	/* 4(int) * 3(lkg_type) = 12 bytes */
	unsigned int cpu_temp[NR_CPU_LKG_TYPE];

	/* 4(int) * 3(lkg_type) * 16 = 192 bytes */
	unsigned int cpu_lkg_pwr[NR_CPU_LKG_TYPE][NR_CPU_OPP];

	/* 1940/2000 bytes */
	unsigned int mem_reserved[MEM_SWPM_RESERVED_SIZE];

	/* 828/832 bytes */
	unsigned int core_reserved[CORE_SWPM_RESERVED_SIZE];

	/* 4(int) * 15 = 60 bytes */
	unsigned int gpu_reserved[GPU_SWPM_RESERVED_SIZE];

	/* 4(int) * 256 = 1024 bytes */
	unsigned int isp_reserved[ISP_SWPM_RESERVED_SIZE];

	/* 4(int) * 11 = 44 bytes */
	unsigned int me_reserved[ME_SWPM_RESERVED_SIZE];
	/* remaining size = 60/6144 bytes */
} __aligned(8);

extern struct swpm_rec_data *swpm_info_ref;

#ifdef CONFIG_MTK_CACHE_CONTROL
extern int ca_force_stop_set_in_kernel(int val);
#endif

#endif

