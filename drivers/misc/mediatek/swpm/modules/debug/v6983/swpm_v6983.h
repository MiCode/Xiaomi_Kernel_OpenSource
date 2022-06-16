/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SWPM_V6983_H__
#define __SWPM_V6983_H__

#define SWPM_TEST (0)
#define SWPM_DEPRECATED (0)

#define MAX_RECORD_CNT				(64)

#define DEFAULT_AVG_WINDOW		(50)

/* VPROC3 + VPROC2 + VPROC1 + VDRAM + VGPU + VCORE */
#define DEFAULT_LOG_MASK			(0x3F)

#define POWER_CHAR_SIZE				(256)
#define POWER_INDEX_CHAR_SIZE			(4096)

#define ALL_SWPM_TYPE				(0xFFFF)
#define EN_POWER_METER_ONLY			(0x1)
#define EN_POWER_METER_ALL			(0x3)

#define MAX_POWER_NAME_LENGTH (16)

#define for_each_pwr_mtr(i)    for (i = 0; i < NR_SWPM_TYPE; i++)

/* data shared w/ SSPM */
enum profile_point {
	MON_TIME,
	CALC_TIME,
	REC_TIME,
	TOTAL_TIME,

	NR_PROFILE_POINT
};

enum swpm_type {
	CPU_SWPM_TYPE,
	GPU_SWPM_TYPE,
	CORE_SWPM_TYPE,
	MEM_SWPM_TYPE,
	ISP_SWPM_TYPE,
	ME_SWPM_TYPE,

	NR_SWPM_TYPE,
};

enum pmsr_cmd_action {
	PMSR_SET_EN,
	PMSR_SET_SIG_SEL,
};

enum power_rail {
	VPROC3,
	VPROC2,
	VPROC1,
	VGPU,
	VCORE,
	VDRAM,
	VIO18_DRAM,

	NR_POWER_RAIL
};

/* power rail */
struct power_rail_data {
	unsigned int avg_power;
	char name[MAX_POWER_NAME_LENGTH];
};

struct share_wrap {
	unsigned int share_index_ext_addr;
	unsigned int share_ctrl_ext_addr;
	unsigned int mem_swpm_data_addr;
	unsigned int core_swpm_data_addr;
};

struct swpm_common_rec_data {
	/* 8 bytes */
	unsigned int cur_idx;
	unsigned int profile_enable;

	/* 8(long) * 5(prof_pt) * 3 = 120 bytes */
	unsigned long long avg_latency[NR_PROFILE_POINT];
	unsigned long long max_latency[NR_PROFILE_POINT];
	unsigned long long prof_cnt[NR_PROFILE_POINT];

	/* 4(int) * 64(rec_cnt) * 7 = 1792 bytes */
	unsigned int pwr[NR_POWER_RAIL][MAX_RECORD_CNT];
};

struct swpm_rec_data {
	struct swpm_common_rec_data common_rec_data;
	/* total size = 8192 bytes */
} __aligned(8);


extern struct share_wrap *wrap_d;

extern struct power_rail_data swpm_power_rail[NR_POWER_RAIL];
extern char *swpm_power_rail_to_string(enum power_rail p);
extern void swpm_set_update_cnt(unsigned int type, unsigned int cnt);
extern void swpm_set_enable(unsigned int type, unsigned int enable);
extern int swpm_v6983_init(void);
extern void swpm_v6983_exit(void);

#endif

