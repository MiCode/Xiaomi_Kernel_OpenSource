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

struct ip_vol_times {
	int64_t active_time;
	int64_t idle_time;
	int64_t off_time;
};
struct ip_stats {
	char ip_name[16];
	struct ip_vol_times *vol_times;
};
struct ddr_times {
	int64_t active_time;
	int64_t sr_time;
	int64_t pd_time;
};
struct ddr_bw_stats {
	int32_t freq;
	uint64_t value;
};
struct ddr_ip_bw_stats {
	char ip_name[16];
	struct ddr_bw_stats *bw_stats;
};

extern int32_t get_ddr_times(int32_t freq_num,
			     struct ddr_times *ddr_times);
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
				      int64_t *duration);

#endif /* __MTK_SWPM_SP_INTERFACE_H__ */

