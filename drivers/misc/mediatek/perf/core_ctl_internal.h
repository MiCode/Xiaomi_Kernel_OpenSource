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

extern void set_overutil_threshold(int index, int val);
extern int get_immediate_tslvts1_1_wrap(void); /* CPU7 TS */
extern int sched_get_nr_overutil_avg(int cluster_id,
				     int *l_avg,
				     int *h_avg,
				     int *sum_nr_overutil_l,
				     int *sum_nr_overutil_h,
				     int *max_nr);
extern int update_userlimit_cpu_freq(int kicker,
				     int num_cluster,
				     struct ppm_limit_data *freq_limit);
extern int _sched_isolate_cpu(int cpu);
extern int _sched_deisolate_cpu(int cpu);
extern unsigned int get_overutil_threshold(int index);
