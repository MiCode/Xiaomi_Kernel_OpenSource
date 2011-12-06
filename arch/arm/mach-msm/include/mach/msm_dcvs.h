/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

/**
 * struct msm_dcvs_idle
 *
 * API for idle code to register and send idle enter/exit
 * notifications to msm_dcvs driver.
 */
struct msm_dcvs_idle {
	const char *core_name;
	/* Enable/Disable idle state/notifications */
	int (*enable)(struct msm_dcvs_idle *self,
			enum msm_core_control_event event);
};

/**
 * msm_dcvs_idle_source_register
 * @drv: Pointer to the source driver
 * @return: Handle to be used for sending idle state notifications.
 *
 * Register the idle driver with the msm_dcvs driver to send idle
 * state notifications for the core.
 */
extern int msm_dcvs_idle_source_register(struct msm_dcvs_idle *drv);

/**
 * msm_dcvs_idle_source_unregister
 * @drv: Pointer to the source driver
 * @return:
 *	0 on success
 *	-EINVAL
 *
 * Description: Unregister the idle driver with the msm_dcvs driver
 */
extern int msm_dcvs_idle_source_unregister(struct msm_dcvs_idle *drv);

/**
 * msm_dcvs_idle
 * @handle: Handle provided back at registration
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
int msm_dcvs_idle(int handle, enum msm_core_idle_state state,
		uint32_t iowaited);

/**
 * struct msm_dcvs_core_info
 *
 * Core specific information used by algorithm. Need to provide this
 * before the sink driver can be registered.
 */
struct msm_dcvs_core_info {
	struct msm_dcvs_freq_entry *freq_tbl;
	struct msm_dcvs_core_param core_param;
	struct msm_dcvs_algo_param algo_param;
};

/**
 * msm_dcvs_register_core
 * @core_name: Unique name identifier for the core.
 * @group_id: Cores that are to be grouped for synchronized frequency scaling
 * @info: The core specific algorithm parameters.
 * @return :
 *	0 on success,
 *	-ENOSYS,
 *	-ENOMEM
 *
 * Register the core with msm_dcvs driver. Done once at init before calling
 * msm_dcvs_freq_sink_register
 * Cores that need to run synchronously must share the same group id.
 * If a core doesnt care to be in any group, the group_id should be 0.
 */
extern int msm_dcvs_register_core(const char *core_name, uint32_t group_id,
		struct msm_dcvs_core_info *info);

/**
 * struct msm_dcvs_freq
 *
 * API for clock driver code to register and receive frequency change
 * request for the core from the msm_dcvs driver.
 */
struct msm_dcvs_freq {
	const char *core_name;
	/* Callback from msm_dcvs to set the core frequency */
	int (*set_frequency)(struct msm_dcvs_freq *self,
			unsigned int freq);
	unsigned int (*get_frequency)(struct msm_dcvs_freq *self);
};

/**
 * msm_dcvs_freq_sink_register
 * @drv: The sink driver
 * @return: Handle unique to the core.
 *
 * Register the clock driver code with the msm_dvs driver to get notified about
 * frequency change requests.
 */
extern int msm_dcvs_freq_sink_register(struct msm_dcvs_freq *drv);

/**
 * msm_dcvs_freq_sink_unregister
 * @drv: The sink driver
 * @return:
 *	0 on success,
 *	-EINVAL
 *
 * Unregister the sink driver for the core. This will cause the source driver
 * for the core to stop sending idle pulses.
 */
extern int msm_dcvs_freq_sink_unregister(struct msm_dcvs_freq *drv);

#endif
