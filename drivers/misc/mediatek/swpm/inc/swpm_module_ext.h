/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __SWPM_MODULE_EXT_H__
#define __SWPM_MODULE_EXT_H__

#include <linux/types.h>

#define MAX_IP_NAME_LENGTH (16)

/* swpm extension interface types */
enum swpm_num_type {
	DDR_DATA_IP,
	DDR_FREQ,
	CORE_IP,
	CORE_VOL,
};

/* swpm extension structure */
struct ip_vol_times {
	int32_t vol;
	int64_t active_time;
	int64_t idle_time;
	int64_t off_time;
};
struct ip_stats {
	char ip_name[MAX_IP_NAME_LENGTH];
	struct ip_vol_times *vol_times;
};
struct vol_duration {
	int32_t vol;
	int64_t duration;
};
struct ddr_act_times {
	int32_t freq;
	int64_t active_time;
};
struct ddr_sr_pd_times {
	int64_t sr_time;
	int64_t pd_time;
};
struct ddr_bc_stats {
	int32_t freq;
	uint64_t value;
};
struct ddr_ip_bc_stats {
	char ip_name[MAX_IP_NAME_LENGTH];
	struct ddr_bc_stats *bc_stats;
};

/* swpm extension internal ops structure */
struct swpm_internal_ops {
	void (*const cmd)(unsigned int type,
			  unsigned int val);
	int32_t (*const ddr_act_times_get)
		(int32_t freq_num,
		 struct ddr_act_times *ddr_times);
	int32_t (*const ddr_sr_pd_times_get)
		(struct ddr_sr_pd_times *ddr_times);
	int32_t (*const ddr_freq_data_ip_stats_get)
		(int32_t data_ip_num,
		 int32_t freq_num,
		 void *stats);
	int32_t (*const vcore_ip_vol_stats_get)
		(int32_t ip_num,
		 int32_t vol_num,
		 void *stats);
	int32_t (*const vcore_vol_duration_get)
		(int32_t vol_num,
		 struct vol_duration *duration);
	int32_t (*const num_get)
		(enum swpm_num_type type);
};

extern int32_t sync_latest_data(void);
extern int32_t get_ddr_act_times(int32_t freq_num,
				 struct ddr_act_times *ddr_times);
extern int32_t get_ddr_sr_pd_times(struct ddr_sr_pd_times *ddr_times);
extern int32_t get_ddr_data_ip_num(void);
extern int32_t get_ddr_freq_num(void);
extern int32_t get_ddr_freq_data_ip_stats(int32_t data_ip_num,
					  int32_t freq_num,
					  void *stats);
extern int32_t get_vcore_ip_num(void);
extern int32_t get_vcore_vol_num(void);
extern int32_t get_vcore_ip_vol_stats(int32_t ip_num,
				       int32_t vol_num,
				       void *stats);
extern int32_t get_vcore_vol_duration(int32_t vol_num,
				      struct vol_duration *duration);
extern int mtk_register_swpm_ops(struct swpm_internal_ops *ops);

#endif /* __SWPM_MODULE_EXT_H__ */