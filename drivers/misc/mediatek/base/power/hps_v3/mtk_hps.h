/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef __MTK_HPS_H__
#define __MTK_HPS_H__

enum hps_base_type_e {
	BASE_PERF_SERV = 0,
	BASE_PPM_SERV,
	BASE_COUNT
};

enum hps_limit_type_e {
	LIMIT_THERMAL = 0,
	LIMIT_PPM_SERV,
	LIMIT_LOW_BATTERY,
	LIMIT_ULTRA_POWER_SAVING,
	LIMIT_POWER_SERV,
	LIMIT_COUNT
};

extern int hps_get_enabled(unsigned int *enabled_ptr);
extern int hps_set_enabled(unsigned int enabled);
extern int hps_get_cpu_num_base(enum hps_base_type_e type,
	unsigned int *little_cpu_ptr, unsigned int *big_cpu_ptr);
extern int hps_set_cpu_num_base(enum hps_base_type_e type,
				unsigned int little_cpu, unsigned int big_cpu);
extern int hps_get_cpu_num_limit(enum hps_limit_type_e type,
	unsigned int *little_cpu_ptr, unsigned int *big_cpu_ptr);
extern int hps_set_cpu_num_limit(enum hps_limit_type_e type,
	unsigned int little_cpu, unsigned int big_cpu);
extern int hps_get_tlp(unsigned int *tlp_ptr);
extern int hps_get_num_possible_cpus(unsigned int *little_cpu_ptr,
	unsigned int *big_cpu_ptr);
extern int hps_get_num_online_cpus(unsigned int *little_cpu_ptr,
	unsigned int *big_cpu_ptr);
extern int hps_set_PPM_request(unsigned int little_min,
	unsigned int little_max, unsigned int big_min, unsigned int big_max);
extern unsigned int hps_get_hvytsk(unsigned int cluster_id);

#endif
