/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SWPM_CORE_V6886_H__
#define __SWPM_CORE_V6886_H__

#include <swpm_mem_v6886.h>

#define MAX_APHY_CORE_PWR			(12)
#define NR_CORE_VOLT				(5)

/* infra power state for core power */
enum infra_power_state {
	INFRA_DATA_ACTIVE,
	INFRA_CMD_ACTIVE,
	INFRA_IDLE,
	INFRA_DCM,

	NR_INFRA_POWER_STATE
};

enum core_static_type {
	CORE_STATIC_MMINFRA, /* 4% */
	CORE_STATIC_MDPSYS,  /* 5% */
	CORE_STATIC_VDEC,    /* 6% */
	CORE_STATIC_MMSYS,   /* 7% */
	CORE_STATIC_VEND,    /* 9% */
	CORE_STATIC_DRAMC,   /* 10% */
	CORE_STATIC_TOP,     /* 11% */
	CORE_STATIC_INFRA,   /* 23% */
	CORE_STATIC_CONNSYS, /* 25% */

	NR_CORE_STATIC_TYPE
};

enum core_static_rec_type {
	CORE_STATIC_REC_INFRA,
	CORE_STATIC_REC_DRAMC,
	NR_CORE_STATIC_REC_TYPE
};

/* core voltage/freq index */
struct core_swpm_vf_index {
	unsigned int vcore_mv;
	unsigned int ddr_freq_mhz;
	unsigned int vcore_cur_opp;
	unsigned int ddr_cur_opp;
};

/* core power index structure */
struct core_swpm_index {
	unsigned int infra_state_ratio[NR_INFRA_POWER_STATE];
	unsigned int read_bw[MAX_EMI_NUM];
	unsigned int write_bw[MAX_EMI_NUM];
	unsigned int srr_pct;
	unsigned int ssr_pct;
	struct core_swpm_vf_index vf;
};

struct core_swpm_data {
	unsigned int core_volt_tbl[NR_CORE_VOLT];
	unsigned int core_static_pwr[NR_CORE_VOLT][NR_CORE_STATIC_TYPE];
	unsigned int thermal;
};

enum aphy_core_pwr_type {
	APHY_VCORE,
	NR_APHY_CORE_PWR_TYPE
};

struct aphy_core_bw_data {
	unsigned short bw[MAX_APHY_CORE_PWR];
};

struct aphy_core_pwr {
	unsigned short read_coef[MAX_APHY_CORE_PWR];
	unsigned short write_coef[MAX_APHY_CORE_PWR];
};

struct aphy_core_pwr_data {
	struct aphy_core_pwr pwr[NR_DDR_FREQ];
	unsigned short coef_idle[NR_DDR_FREQ];
	unsigned short coef_srst[NR_DDR_FREQ];
	unsigned short coef_ssr[NR_DDR_FREQ];
	unsigned short volt[NR_DDR_FREQ];
};

/* core share memory data structure - 874/1000 bytes */
struct core_swpm_rec_data {
	/* 2(short) * 12(sample point) * 9(opp_num) = 216 bytes */
	struct aphy_core_bw_data aphy_core_bw_tbl[NR_DDR_FREQ];

	/* 2(short) * 1(pwr_type) */
	/* * ((12+12)(r/w_coef) * 9(opp) + 9(idle) + 9(srst) + 9(ssr) + 9(volt)) = 504 bytes */
	struct aphy_core_pwr_data
		aphy_core_pwr_tbl[NR_APHY_CORE_PWR_TYPE];
};

extern unsigned int swpm_core_static_data_get(void);
extern void swpm_core_static_replaced_data_set(unsigned int data);
extern void swpm_core_static_data_init(void);
extern int swpm_core_v6886_init(void);
extern void swpm_core_v6886_exit(void);

#endif

