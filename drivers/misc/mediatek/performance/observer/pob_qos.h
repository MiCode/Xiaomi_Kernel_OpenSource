/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __POB_QOS_H__
#define __POB_QOS_H__

enum pob_qosbm_type {
	PQBT_BOUND = 0,
	PQBT_TOTAL,
	PQBT_CPU,
	PQBT_GPU,
	PQBT_MM,
	PQBT_MD,

	PQBT_APU,
	PQBT_VPU,
	PQBT_MDLA,

	PQBT_VENC,
	PQBT_CAM,
	PQBT_IMG,
	PQBT_MDP,

	NR_PQBT
};

enum pob_qosbm_probe {
	PQBP_EMI = 0,
	PQBP_SMI,

	NR_PQBP
};

enum pob_qosbm_source {
	PQBS_MON = 0,
	PQBS_REQ,

	NR_PQBS
};

enum pob_qosbm_bound {
	PQB_BW_FREE = 0,
	PQB_BW_CONGESTIVE = 1,
	PQB_BW_FULL = 2,
};

enum pob_qoslat_type {
	PQLT_CPU,
	PQLT_VPU,
	PQLT_MDLA,

	NR_PQLT
};

enum pob_qoslat_source {
	PQLS_MON,

	NR_PQLS
};

#if defined(CONFIG_MTK_PERF_OBSERVER)
int pob_qosbm_get_cap(enum pob_qosbm_type pqbt,
			enum pob_qosbm_probe pqbp,
			enum pob_qosbm_source pqbs,
			int *super,
			int *sub);

int pob_qosbm_get_stat(void *pstats,
			int idx,
			enum pob_qosbm_type pqbt,
			enum pob_qosbm_probe pqbp,
			enum pob_qosbm_source pqbs,
			int issub,
			int ssidx);

int pob_qoslat_get_cap(enum pob_qoslat_type pqlt,
			enum pob_qoslat_source pqls,
			int *super,
			int *sub);

int pob_qoslat_get_stat(void *pstats,
			int idx,
			enum pob_qoslat_type pqlt,
			enum pob_qoslat_source pqls,
			int issub,
			int ssidx);

int pob_qosseq_get(void *pstats, int idx);

int pob_qos_get_max_bw_threshold(void);

int pob_qosbm_get_last_avg(int lastcount,
			enum pob_qosbm_type pqbt,
			enum pob_qosbm_probe pqbp,
			enum pob_qosbm_source pqbs,
			int issub,
			int ssidx);

#else
static inline int pob_qosbm_get_cap(enum pob_qosbm_type pqbt,
			enum pob_qosbm_probe pqbp,
			enum pob_qosbm_source pqbs,
			int *super,
			int *sub)
{ return -1; }

static inline int pob_qosbm_get_stat(void *pstats,
			int idx,
			enum pob_qosbm_type pqbt,
			enum pob_qosbm_probe pqbp,
			enum pob_qosbm_source pqbs,
			int issub,
			int ssidx)
{ return -1; }

static inline int pob_qoslat_get_cap(enum pob_qoslat_type pqlt,
			enum pob_qoslat_source pqls,
			int *super,
			int *sub)
{ return -1; }

static inline int pob_qoslat_get_stat(void *pstats,
			int idx,
			enum pob_qoslat_type pqlt,
			enum pob_qoslat_source pqls,
			int issub,
			int ssidx)
{ return -1; }

static inline int pob_qosseq_get(void *pstats, int idx)
{ return -1; }

static inline int pob_qos_get_max_bw_threshold(void)
{ return -1; }

static inline int pob_qosbm_get_last_avg(int lastcount,
			enum pob_qosbm_type pqbt,
			enum pob_qosbm_probe pqbp,
			enum pob_qosbm_source pqbs,
			int issub,
			int ssidx)
{ return -1; }

#endif

#endif

