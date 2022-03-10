/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SWPM_V6855_H__
#define __SWPM_V6855_H__

#include <swpm_gpu_v6855.h>
#include <swpm_isp_v6855.h>
#include <swpm_me_v6855.h>

#define SWPM_TEST (0)
#define SWPM_DEPRECATED (0)

#define MAX_RECORD_CNT				(64)
#define MAX_APHY_CORE_PWR			(12)
#define MAX_APHY_OTHERS_PWR			(11)

#define DEFAULT_AVG_WINDOW		(50)

/* VPROC2 + VPROC1 + VDRAM + VGPU + VCORE */
#define DEFAULT_LOG_MASK			(0x3F)

#define POWER_CHAR_SIZE				(256)
#define POWER_INDEX_CHAR_SIZE			(4096)

#define NR_DRAM_PWR_SAMPLE			(3)
#define NR_CORE_VOLT				(4)
#define NR_CPU_OPP				(36)
#define NR_CPU_LKG				(9)
#define NR_CPU_CORE				(8)
#define NR_CPU_L_CORE				(6)

#define ALL_METER_TYPE				(0xFFFF)
#define EN_POWER_METER_ONLY			(0x1)
#define EN_POWER_METER_ALL			(0x3)

#define MAX_POWER_NAME_LENGTH (16)

#define for_each_pwr_mtr(i)    for (i = 0; i < NR_POWER_METER; i++)

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
/*	VPROC3, */
	VPROC2,
	VPROC1,
	VGPU,
	VCORE,
	VDRAM,
/*	VIO18_DRAM, */

	NR_POWER_RAIL
};

enum ddr_freq {
	DDR_400,
	DDR_600,
	DDR_800,
	DDR_933,
	DDR_1066,
	DDR_1600,
	DDR_2133,
	DDR_2750,

	NR_DDR_FREQ
};

enum core_ts_sample {
	CORE_TS_SOC1,
	CORE_TS_SOC2,
	CORE_TS_SOC3,
	CORE_TS_SOC4,

	NR_CORE_TS,
};

enum aphy_core_pwr_type {
	APHY_VCORE,
	NR_APHY_CORE_PWR_TYPE
};

enum aphy_other_pwr_type {
	/* APHY_VCORE, independent */
	APHY_VDDQ,
	APHY_VM,
	APHY_VIO12,
	/* APHY_VIO_1P8V, */

	NR_APHY_OTHERS_PWR_TYPE
};

enum dram_pwr_type {
	DRAM_VDD1,
	DRAM_VDD2H,
	DRAM_VDD2L,
	DRAM_VDDQ,

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

enum dsu_pmu_idx {
	DSU_PMU_IDX_CYCLES,

	MAX_DSU_PMU_CNT
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
	CPU_CLUSTER_DORMANT,
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

/* power rail */
struct power_rail_data {
	unsigned int avg_power;
	char name[MAX_POWER_NAME_LENGTH];
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
	unsigned int dsu_pmu_val[MAX_DSU_PMU_CNT];
	unsigned int l3_bw;
	unsigned int cpu_emi_bw;
	struct cpu_swpm_vf_index vf;
	unsigned int cpu_lkg[NR_CPU_LKG];
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

enum core_static_type {
	CORE_STATIC_IPESYS,
	CORE_STATIC_VENC,
	CORE_STATIC_TOP,
	CORE_STATIC_VDEC,
	CORE_STATIC_MMSYS,
	CORE_STATIC_IMGSYS,
	CORE_STATIC_DRAMC,
	CORE_STATIC_CONNSYS,
	CORE_STATIC_INFRA,
	CORE_STATIC_CAMSYS,
	NR_CORE_STATIC_TYPE,
};

enum core_static_rec_type {
	CORE_STATIC_REC_INFRA,
	CORE_STATIC_REC_DRAMC,
	NR_CORE_STATIC_REC_TYPE
};

/* sync with sspm */
#define MAX_EMI_NUM (1)
/* core voltage/freq index */
struct core_swpm_vf_index {
	unsigned int vcore_mv;
	unsigned int ddr_freq_mhz;
};
/* core static index */
struct core_swpm_static_index {
	unsigned int core_static_pwr[NR_CORE_STATIC_REC_TYPE];
	unsigned int thermal;
};
/* core power index structure */
struct core_swpm_index {
	/* for calculation */
	unsigned int infra_state_ratio[NR_INFRA_POWER_STATE];
	unsigned int read_bw[MAX_EMI_NUM];
	unsigned int write_bw[MAX_EMI_NUM];
	struct core_swpm_vf_index vf;
	struct core_swpm_static_index static_idx;
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
	unsigned int srr_pct;			/* (s1) self refresh rate */
	unsigned int ssr_pct;			/* (s0) sleep rate */
	unsigned int pdir_pct[MAX_EMI_NUM];	/* power-down idle rate */
	unsigned int phr_pct[MAX_EMI_NUM];	/* page-hit rate */
	unsigned int acc_util[MAX_EMI_NUM];	/* accumulate EMI utilization */
	unsigned int trans[MAX_EMI_NUM];	/* transaction count */
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

/* mW * 100 */
struct aphy_core_pwr_data {
	struct aphy_core_pwr pwr[NR_DDR_FREQ];
	unsigned short coef_idle[NR_DDR_FREQ];
	unsigned short coef_srst[NR_DDR_FREQ];
	unsigned short coef_ssr[NR_DDR_FREQ];
	unsigned short volt[NR_DDR_FREQ];
};

struct aphy_others_pwr_data {
	struct aphy_others_pwr pwr[NR_DDR_FREQ];
	unsigned short coef_idle[NR_DDR_FREQ];
	unsigned short coef_srst[NR_DDR_FREQ];
	unsigned short coef_ssr[NR_DDR_FREQ];
	unsigned short volt[NR_DDR_FREQ];
};

/* unit: uA */
struct dram_pwr_conf {
	unsigned int volt;
	unsigned int i_dd0;
	unsigned int i_dd2p;
	unsigned int i_dd2n;
	unsigned int i_dd4r;
	unsigned int i_dd4w;
	unsigned int i_dd5;
	unsigned int i_dd6;
};

struct dram_pwr_data {
	struct dram_pwr_conf idd_conf[NR_DRAM_PWR_SAMPLE];
};

/* numbers of unsigned int for cpu reserved memory */
#define CPU_SWPM_RESERVED_SIZE (8)

struct cpu_swpm_rec_data {
	/* 4(int) * 8(cores) = 32 bytes */
	unsigned int cpu_temp[NR_CPU_CORE];
};

/* numbers of unsigned int for mem reserved memory */
#define MEM_SWPM_RESERVED_SIZE (675)

/* mem share memory data structure - 2640/2700 bytes */
struct mem_swpm_rec_data {
	/* 2(short) * 9(ddr_opp) = 18 bytes */
	unsigned short ddr_opp_freq[NR_DDR_FREQ];

	/* 2(short) * 16(sample point) * 9(opp_num) = 288 bytes */
	struct aphy_others_bw_data aphy_others_bw_tbl[NR_DDR_FREQ];

	/* 2(short) * 3(pwr_type) */
	/* * ((16+16)(r/w_coef) * 9(opp) + 9(idle) + 9(srst) + 9(ssr) + 9(volt)) = 1944 bytes */
	struct aphy_others_pwr_data
		aphy_others_pwr_tbl[NR_APHY_OTHERS_PWR_TYPE];

	/* 2(short) * 3(dram_pwr_sample) = 6 bytes */
	unsigned short dram_pwr_sample[NR_DRAM_PWR_SAMPLE];

	/* 4(int) * 12(pwr_type * pwr_sample) * 8 = 384 bytes */
	struct dram_pwr_data dram_conf[NR_DRAM_PWR_TYPE];
};

/* numbers of unsigned int for core reserved memory */
#define CORE_SWPM_RESERVED_SIZE (250)

/* core share memory data structure - 874/1000 bytes */
struct core_swpm_rec_data {
	/* 2(short) * 5(core_volt) = 10 bytes */
	unsigned short core_volt_tbl[NR_CORE_VOLT];

	/* 2(short) * 12(sample point) * 9(opp_num) = 216 bytes */
	struct aphy_core_bw_data aphy_core_bw_tbl[NR_DDR_FREQ];

	/* 2(short) * 1(pwr_type) */
	/* * ((12+12)(r/w_coef) * 9(opp) + 9(idle) + 9(srst) + 9(ssr) + 9(volt)) = 504 bytes */
	struct aphy_core_pwr_data
		aphy_core_pwr_tbl[NR_APHY_CORE_PWR_TYPE];

	/* 4 (int) * 5(volt) * 7(core_static_type) = 140 bytes */
	unsigned int core_static_pwr[NR_CORE_VOLT][NR_CORE_STATIC_TYPE];

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

	/* 32/32 bytes */
	unsigned int cpu_reserved[CPU_SWPM_RESERVED_SIZE];

	/* 2640/2700 bytes */
	unsigned int mem_reserved[MEM_SWPM_RESERVED_SIZE];

	/* 874/1000 bytes */
	unsigned int core_reserved[CORE_SWPM_RESERVED_SIZE];

	/* 4(int) * 15 = 60 bytes */
	unsigned int gpu_reserved[GPU_SWPM_RESERVED_SIZE];

	/* 4(int) * 256 = 1024 bytes */
	unsigned int isp_reserved[ISP_SWPM_RESERVED_SIZE];

	/* 4(int) * 11 = 44 bytes */
	unsigned int me_reserved[ME_SWPM_RESERVED_SIZE];
	/* used/remaining/total size = 6780/1412/8192 bytes */
} __aligned(8);

#if IS_ENABLED(CONFIG_MTK_CACHE_CONTROL)
//extern int ca_force_stop_set_in_kernel(int val);
#endif

extern struct power_rail_data swpm_power_rail[NR_POWER_RAIL];
extern spinlock_t swpm_snap_spinlock;
extern struct mem_swpm_index mem_idx_snap;
extern struct swpm_rec_data *swpm_info_ref;
extern struct core_swpm_rec_data *core_ptr;
extern struct mem_swpm_rec_data *mem_ptr;
extern struct me_swpm_rec_data *me_ptr;
extern struct share_wrap *wrap_d;

extern char *swpm_power_rail_to_string(enum power_rail p);
extern void swpm_set_update_cnt(unsigned int type, unsigned int cnt);
extern void swpm_set_enable(unsigned int type, unsigned int enable);
extern int swpm_v6855_init(void);
extern void swpm_v6855_exit(void);

#endif

