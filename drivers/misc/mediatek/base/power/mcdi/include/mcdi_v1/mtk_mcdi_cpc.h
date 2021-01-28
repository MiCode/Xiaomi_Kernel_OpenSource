/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __MTK_MCDI_CPC_H__
#define __MTK_MCDI_CPC_H__

#include <linux/proc_fs.h>
#include <mtk_mcdi_plat.h>

#define CPC_LAT_NAME_SIZE 36

/**
 * number of profile type :
 *  - (number of cpu type) * (cpu/cluster state) + mcusys
 */
#define PROF_TYPE_NUM (NF_CPU_TYPE * 2 + 1)

enum prof_distribute {
	MCDI_PROF_U10_US,
	MCDI_PROF_U20_US,
	MCDI_PROF_U50_US,
	MCDI_PROF_O50_US,

	NF_MCDI_PROF_DIST
};

struct mcdi_cpc_prof {
	char name[CPC_LAT_NAME_SIZE];
	unsigned int on_sum;
	unsigned int on_max;
	unsigned int off_sum;
	unsigned int off_max;
	unsigned int on_cnt;
	unsigned int off_cnt;
};

struct mcdi_cpc_distribute {
	unsigned int on[NF_MCDI_PROF_DIST];
	unsigned int off[NF_MCDI_PROF_DIST];
	unsigned int cnt;
};

struct mcdi_cpc_status {
	bool prof_en;
	bool prof_pause;
	bool prof_saving;
	bool dbg_en;
};

struct mcdi_cpc_dev {
	struct mcdi_cpc_status sta;
	union {
		struct mcdi_cpc_prof p[PROF_TYPE_NUM];
		struct {
			struct mcdi_cpc_prof cpu[NF_CPU_TYPE];
			struct mcdi_cpc_prof cluster[NF_CPU_TYPE];
			struct mcdi_cpc_prof mcusys;
		};
	};
	struct mcdi_cpc_distribute dist_cnt[NF_CPU_TYPE];
	unsigned int mp_off_cnt[NF_CLUSTER];
	unsigned int mp_off_lat[NF_CPU];
};

#define cpc_tick_to_us(val) ((val) / 13)

void mcdi_cpc_init(void);
void mcdi_cpc_reflect(int cpu, int last_core);
void mcdi_cpc_prof_en(bool enable);
void mcdi_procfs_cpc_init(struct proc_dir_entry *mcdi_dir);

#endif /* __MTK_MCDI_CPC_H__ */
