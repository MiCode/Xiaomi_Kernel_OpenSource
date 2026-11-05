/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __QCOM_STATS_H__
#define __QCOM_STATS_H__

#define DDR_HISTORY_MAX_ELEMENTS	0x2
#define NUM_MAX_SCID	64
#define LLC_ISLAND_STATS_RESVD	3

struct ddr_freq_residency {
	u32 freq;
	u64 residency;
};

struct ddr_stats_ss_vote_info {
	u32 ab; /* vote_x */
	u32 ib; /* vote_y */
};

struct ddr_stats_change_his_info {
	u32 mc_his; /* mc change history */
	u32 shub_his; /* shub change history */
	/* upper 24 bits of last max elements mc changes timestamp */
	u32 last_2_mc_changes_hi[DDR_HISTORY_MAX_ELEMENTS];
	/* lower 32 bits of last max elements mc changes timestamp */
	u32 last_2_mc_changes_lo[DDR_HISTORY_MAX_ELEMENTS];
	/* upper 24 bits of last max elements shub changes timestamp */
	u32 last_2_shub_changes_hi[DDR_HISTORY_MAX_ELEMENTS];
	/* lower 32 bits of last max elements shub changes timestamp */
	u32 last_2_shub_changes_lo[DDR_HISTORY_MAX_ELEMENTS];
};

struct llc_island_stats_active_scids {
	u32 versionID;
	/* counters per SCID which blocks the entry or forces exit of LLC island */
	u64 scid_count[NUM_MAX_SCID];
	/* current SCID status */
	u64 cur_scid_status;
	u64 reserved[LLC_ISLAND_STATS_RESVD];
} __packed;

struct qcom_stats_cx_vote_info {
	u8 level; /* CX LEVEL */
};

#if IS_ENABLED(CONFIG_QCOM_STATS)

int ddr_stats_get_ss_count(void);
int ddr_stats_get_ss_vote_info(int ss_count,
			       struct ddr_stats_ss_vote_info *vote_info);

int qcom_stats_ddr_freqsync_msg(void);
int ddr_stats_get_freq_count(void);
int ddr_stats_get_residency(int freq_count, struct ddr_freq_residency *data);
int ddr_stats_get_change_his(struct ddr_stats_change_his_info *ddr_his_info);
int llc_stats_get_active_scids(struct llc_island_stats_active_scids *llc_active_scids);

bool has_system_slept(bool *aoss_debug);
bool has_subsystem_slept(void);
void subsystem_sleep_debug_enable(bool enable);

int cx_stats_get_ss_vote_info(int ss_count,
			       struct qcom_stats_cx_vote_info *vote_info);

#else

static inline int ddr_stats_get_ss_count(void)
{ return -ENODEV; }
static inline int ddr_stats_get_ss_vote_info(int ss_count,
					     struct ddr_stats_ss_vote_info *vote_info)
{ return -ENODEV; }
static inline int qcom_stats_ddr_freqsync_msg(void)
{ return -ENODEV; }
static inline int ddr_stats_get_freq_count(void)
{ return -ENODEV; }
int ddr_stats_get_residency(int freq_count, struct ddr_freq_residency *data)
{ return -ENODEV; }
int ddr_stats_get_change_his(struct ddr_stats_change_his_info *ddr_his_info)
{ return -ENODEV; }
int llc_stats_get_active_scids(struct llc_island_stats_active_scids *llc_active_scids)
{ return -ENODEV; }

bool has_system_slept(void)
{ return false; }
bool has_subsystem_slept(void)
{ return false; }
void subsystem_sleep_debug_enable(bool enable)
{ return; }

static inline int cx_stats_get_ss_vote_info(int ss_count,
			       struct qcom_stats_cx_vote_info *vote_info)
{ return -ENODEV; }

#endif
#endif /*__QCOM_STATS_H__ */
