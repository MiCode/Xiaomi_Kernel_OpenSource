/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef _ARCH_ARM_MACH_MSM_MSM_DCVS_H
#define _ARCH_ARM_MACH_MSM_MSM_DCVS_H

#include <mach/msm_dcvs_scm.h>

#define CORE_NAME_MAX (32)
#define CORES_MAX (10)

#define CPU_OFFSET	1  /* used to notify TZ the core number */
#define GPU_OFFSET (CORES_MAX * 2/3)  /* there will be more cpus than gpus,
				     * let the GPU be assigned fewer core
				     * elements and start later
				     */

enum msm_core_idle_state {
	MSM_DCVS_IDLE_ENTER,
	MSM_DCVS_IDLE_EXIT,
};

enum msm_core_control_event {
	MSM_DCVS_ENABLE_IDLE_PULSE,
	MSM_DCVS_DISABLE_IDLE_PULSE,
	MSM_DCVS_ENABLE_HIGH_LATENCY_MODES,
	MSM_DCVS_DISABLE_HIGH_LATENCY_MODES,
};

struct msm_dcvs_sync_rule {
	unsigned long cpu_khz;
	unsigned long gpu_floor_khz;
};

struct msm_dcvs_platform_data {
	struct msm_dcvs_sync_rule *sync_rules;
	unsigned num_sync_rules;
	unsigned long gpu_max_nom_khz;
};

struct msm_gov_platform_data {
	struct msm_dcvs_core_info *info;
	int latency;
};

/**
 * msm_dcvs_register_cpu_freq
 * @freq: the frequency value to register
 * @voltage: the operating voltage (in mV) associated with the above frequency
 *
 * Register a cpu frequency and its operating voltage with dcvs.
 */
#ifdef CONFIG_MSM_DCVS
void msm_dcvs_register_cpu_freq(uint32_t freq, uint32_t voltage);
#else
static inline void msm_dcvs_register_cpu_freq(uint32_t freq, uint32_t voltage)
{}
#endif

/**
 * msm_dcvs_idle
 * @dcvs_core_id: The id returned by msm_dcvs_register_core
 * @state: The enter/exit idle state the core is in
 * @iowaited: iowait in us
 * on iMSM_DCVS_IDLE_EXIT.
 * @return:
 *	0 on success,
 *	-ENOSYS,
 *	-EINVAL,
 *	SCM return values
 *
 * Send idle state notifications to the msm_dcvs driver
 */
int msm_dcvs_idle(int dcvs_core_id, enum msm_core_idle_state state,
		uint32_t iowaited);

/**
 * struct msm_dcvs_core_info
 *
 * Core specific information used by algorithm. Need to provide this
 * before the sink driver can be registered.
 */
struct msm_dcvs_core_info {
	int					num_cores;
	int					*sensors;
	int					thermal_poll_ms;
	struct msm_dcvs_freq_entry		*freq_tbl;
	struct msm_dcvs_core_param		core_param;
	struct msm_dcvs_algo_param		algo_param;
	struct msm_dcvs_energy_curve_coeffs	energy_coeffs;
	struct msm_dcvs_power_params		power_param;
};

/**
 * msm_dcvs_register_core
 * @type: whether this is a CPU or a GPU
 * @type_core_num: The number of the core for a type
 * @info: The core specific algorithm parameters.
 * @sensor: The thermal sensor number of the core in question
 * @return :
 *	0 on success,
 *	-ENOSYS,
 *	-ENOMEM
 *
 * Register the core with msm_dcvs driver. Done once at init before calling
 * msm_dcvs_freq_sink_register
 * Cores that need to run synchronously must share the same group id.
 */
extern int msm_dcvs_register_core(
	enum msm_dcvs_core_type type,
	int type_core_num,
	struct msm_dcvs_core_info *info,
	int (*set_frequency)(int type_core_num, unsigned int freq),
	unsigned int (*get_frequency)(int type_core_num),
	int (*idle_enable)(int type_core_num,
				enum msm_core_control_event event),
	int (*set_floor_frequency)(int type_core_num, unsigned int freq),
	int sensor);

/**
 * msm_dcvs_freq_sink_start
 * @drv: The sink driver
 * @return: Handle unique to the core.
 *
 * Register the clock driver code with the msm_dvs driver to get notified about
 * frequency change requests.
 */
extern int msm_dcvs_freq_sink_start(int dcvs_core_id);

/**
 * msm_dcvs_freq_sink_stop
 * @drv: The sink driver
 * @return:
 *	0 on success,
 *	-EINVAL
 *
 * Unregister the sink driver for the core. This will cause the source driver
 * for the core to stop sending idle pulses.
 */
extern int msm_dcvs_freq_sink_stop(int dcvs_core_id);

/**
 * msm_dcvs_update_limits
 * @drv: The sink driver
 *
 * Update the frequency known to dcvs when the limits are changed.
 */
extern void msm_dcvs_update_limits(int dcvs_core_id);

/**
 * msm_dcvs_apply_gpu_floor
 * @cpu_freq: CPU frequency to compare to GPU sync rules
 *
 * Apply a GPU floor frequency if the corresponding CPU frequency,
 * or the number of CPUs online, requires it.
 */
extern void msm_dcvs_apply_gpu_floor(unsigned long cpu_freq);

/**
 * msm_dcvs_update_algo_params
 * @return:
 *      0 on success, < 0 on error
 *
 * Updates the DCVS algorithm with parameters depending on the
 * number of CPUs online.
 */
extern int msm_dcvs_update_algo_params(void);
#endif
