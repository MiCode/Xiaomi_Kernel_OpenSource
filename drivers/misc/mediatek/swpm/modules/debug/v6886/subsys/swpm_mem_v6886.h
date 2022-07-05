/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SWPM_MEM_V6886_H__
#define __SWPM_MEM_V6886_H__

#define MAX_APHY_OTHERS_PWR			(16)
#define NR_DRAM_PWR_SAMPLE			(3)
/* sync emi in sspm */
#define MAX_EMI_NUM				(1)

enum ddr_freq {
	DDR_400,
	DDR_800,
	DDR_933,
	DDR_1066,
	DDR_1547,
	DDR_2133,
	DDR_2750,
	DDR_3200,

	NR_DDR_FREQ
};

/* dram voltage/freq index */
struct mem_swpm_vf_index {
	unsigned int ddr_freq_mhz;
	unsigned int ddr_cur_opp;
};

/* dram power index structure */
struct mem_swpm_index {
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

struct mem_swpm_data {
	unsigned int read_bw[MAX_EMI_NUM];
	unsigned int write_bw[MAX_EMI_NUM];
	unsigned int ddr_opp_freq[NR_DDR_FREQ];
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

struct aphy_others_bw_data {
	unsigned short bw[MAX_APHY_OTHERS_PWR];
};

struct aphy_others_pwr {
	unsigned short read_coef[MAX_APHY_OTHERS_PWR];
	unsigned short write_coef[MAX_APHY_OTHERS_PWR];
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

/* mem share memory data structure - 2640/2700 bytes */
struct mem_swpm_rec_data {
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

extern spinlock_t mem_swpm_spinlock;
extern struct mem_swpm_data mem_idx_snap;
extern int swpm_mem_v6886_init(void);
extern void swpm_mem_v6886_exit(void);

#endif

