/*
 * Copyright (C) 2015 MediaTek Inc.
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

/*
 * @file    mt_hotplug_strategy.h
 * @brief   hotplug strategy(hps) - external header file
 */

#ifndef __MT_HOTPLUG_STRATEGY_H__
#define __MT_HOTPLUG_STRATEGY_H__

#ifdef __cplusplus
extern "C" {
#endif

enum hps_base_type_e {
	BASE_PERF_SERV = 0,
	BASE_COUNT
};

enum hps_limit_type_e {
	LIMIT_THERMAL = 0,
	LIMIT_LOW_BATTERY,
	LIMIT_ULTRA_POWER_SAVING,
	LIMIT_POWER_SERV,
	LIMIT_COUNT
};

extern int hps_get_enabled(unsigned int *enabled_ptr);
extern int hps_set_enabled(unsigned int enabled);
extern int hps_get_cpu_num_base(
		enum hps_base_type_e type,
		unsigned int *little_cpu_ptr,
		unsigned int *big_cpu_ptr);
extern int hps_set_cpu_num_base(
		enum hps_base_type_e type,
		unsigned int little_cpu,
		unsigned int big_cpu);
extern int hps_get_cpu_num_limit(
		enum hps_limit_type_e type,
		unsigned int *little_cpu_ptr,
		unsigned int *big_cpu_ptr);
extern int hps_set_cpu_num_limit(
		enum hps_limit_type_e type,
		unsigned int little_cpu,
		unsigned int big_cpu);
extern int hps_get_tlp(unsigned int *tlp_ptr);
extern int hps_get_num_possible_cpus(
		unsigned int *little_cpu_ptr,
		unsigned int *big_cpu_ptr);
extern int hps_get_num_online_cpus(
		unsigned int *little_cpu_ptr,
		unsigned int *big_cpu_ptr);

#ifdef __cplusplus
}
#endif

#endif /* __MT_HOTPLUG_STRATEGY_H__ */

