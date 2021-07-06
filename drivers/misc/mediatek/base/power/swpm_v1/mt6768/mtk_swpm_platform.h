/*
 * Copyright (C) 2017 MediaTek Inc.
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

#define MAX_RECORD_CNT                  (64)
#define DEFAULT_LOG_INTERVAL_MS         (1000)
#define DEFAULT_LOG_MASK                (0x13) /* VPROC12 + VPROC11 + VDRAM */

#define NR_CPU_OPP                      (16)
#define NR_CPU_L_CORE                   (6)

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

	NR_POWER_METER
};

enum power_rail {
	VPROC12,
	VPROC11,
	VGPU,
	VCORE,
	VDRAM1,
	VDD1,

	NR_POWER_RAIL
};

enum ddr_freq {
	DDR_OPP0,
	DDR_OPP1,
	DDR_OPP2,
	DDR_OPP3,

	NR_DDR_FREQ
};

enum aphy_pwr_type {
	APHY_VCORE,
	APHY_VDDQ_0P6V,
	APHY_VM_1P1V,
	APHY_VIO_1P8V,

	NR_APHY_PWR_TYPE
};

enum dram_pwr_type {
	DRAM_VDD1_1P8V,
	DRAM_VDD2_1P1V,
	DRAM_VDDQ_0P6V,

	NR_DRAM_PWR_TYPE
};

enum cpu_lkg_type {
	CPU_L_LKG,
	CPU_B_LKG,
	DSU_LKG,

	NR_CPU_LKG_TYPE
};

struct aphy_pwr {
	unsigned short bw[5];
	unsigned short coef[5];
};

/* unit: uW / V^2 */
struct aphy_pwr_data {
	struct aphy_pwr read_pwr[NR_DDR_FREQ];
	struct aphy_pwr write_pwr[NR_DDR_FREQ];
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

struct swpm_rec_data {
	/* 12 bytes */
	unsigned int cur_idx;
	unsigned int profile_enable;
	unsigned int ddr_type;

	/* 2(short) * 4(ddr_opp) = 8 bytes */
	unsigned short ddr_opp_freq[NR_DDR_FREQ];

	/* 4(int) * 64(rec_cnt) * 6 = 1536 1792 bytes */
	unsigned int pwr[NR_POWER_RAIL][MAX_RECORD_CNT];

	/* 8(long) * 5(prof_pt) * 3 = 120 bytes */
	unsigned long long avg_latency[NR_PROFILE_POINT];
	unsigned long long max_latency[NR_PROFILE_POINT];
	unsigned long long prof_cnt[NR_PROFILE_POINT];

	/* 2(short) * 4(pwr_type) * 84 = 672 bytes */
	struct aphy_pwr_data aphy_pwr_tbl[NR_APHY_PWR_TYPE];

	/* 4(int) * 3(pwr_type) * 7 = 84 bytes */
	struct dram_pwr_conf dram_conf[NR_DRAM_PWR_TYPE];

	/* 4(int) * 3(lkg_type) * 16 = 192 bytes */
	unsigned int cpu_lkg_pwr[NR_CPU_LKG_TYPE][NR_CPU_OPP];

	/* remaining size = 448 bytes */
} __packed;

extern struct swpm_rec_data *swpm_info_ref;

#ifdef CONFIG_MTK_CACHE_CONTROL
extern int ca_force_stop_set_in_kernel(int val);
#endif

#endif

