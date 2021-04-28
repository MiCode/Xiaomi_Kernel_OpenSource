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

#ifndef __MTK_SWPM_SP_INTERFACE_H__
#define __MTK_SWPM_SP_INTERFACE_H__

#define MAX_IP_NAME_LENGTH (16)

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

#endif /* __MTK_SWPM_SP_INTERFACE_H__ */

