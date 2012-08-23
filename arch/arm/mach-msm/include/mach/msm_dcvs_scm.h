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
#ifndef _ARCH_ARM_MACH_MSM_MSM_DCVS_SCM_H
#define _ARCH_ARM_MACH_MSM_MSM_DCVS_SCM_H

enum msm_dcvs_algo_param_type {
	MSM_DCVS_ALGO_DCVS_PARAM = 0,
	MSM_DCVS_ALGO_MPD_PARAM  = 1,
};

enum msm_dcvs_scm_event {
	MSM_DCVS_SCM_IDLE_ENTER = 0, /* Core enters idle */
	MSM_DCVS_SCM_IDLE_EXIT = 1, /* Core exits idle */
	MSM_DCVS_SCM_QOS_TIMER_EXPIRED = 2, /* Core slack timer expired */
	MSM_DCVS_SCM_CLOCK_FREQ_UPDATE = 3, /* Core freq change complete */
	MSM_DCVS_SCM_CORE_ONLINE = 4, /* Core is online */
	MSM_DCVS_SCM_CORE_OFFLINE = 5, /* Core is offline */
	MSM_DCVS_SCM_CORE_UNAVAILABLE = 6, /* Core is offline + unavailable */
	MSM_DCVS_SCM_DCVS_ENABLE = 7, /* DCVS is enabled/disabled for core */
	MSM_DCVS_SCM_MPD_ENABLE = 8, /* Enable/disable MP Decision */
	MSM_DCVS_SCM_RUNQ_UPDATE = 9, /* Update running threads */
	MSM_DCVS_SCM_MPD_QOS_TIMER_EXPIRED = 10, /* MPDecision slack timer */
};

struct msm_dcvs_algo_param {
	uint32_t slack_time_us;
	uint32_t scale_slack_time;
	uint32_t scale_slack_time_pct;
	uint32_t disable_pc_threshold;
	uint32_t em_window_size;
	uint32_t em_max_util_pct;
	uint32_t ss_window_size;
	uint32_t ss_util_pct;
	uint32_t ss_iobusy_conv;
};

struct msm_dcvs_freq_entry {
	uint32_t freq; /* Core freq in MHz */
	uint32_t idle_energy;
	uint32_t active_energy;
};

struct msm_dcvs_core_param {
	uint32_t max_time_us;
	uint32_t num_freq; /* number of msm_dcvs_freq_entry passed */
};


struct msm_mpd_algo_param {
	uint32_t em_win_size_min_us;
	uint32_t em_win_size_max_us;
	uint32_t em_max_util_pct;
	uint32_t mp_em_rounding_point_min;
	uint32_t mp_em_rounding_point_max;
	uint32_t online_util_pct_min;
	uint32_t online_util_pct_max;
	uint32_t slack_time_min_us;
	uint32_t slack_time_max_us;
};

#ifdef CONFIG_MSM_DCVS
/**
 * Initialize DCVS algorithm in TrustZone.
 * Must call before invoking any other DCVS call into TZ.
 *
 * @size: Size of buffer in bytes
 *
 * @return:
 *	0 on success.
 *	-EEXIST: DCVS algorithm already initialized.
 *	-EINVAL: Invalid args.
 */
extern int msm_dcvs_scm_init(size_t size);

/**
 * Create an empty core group
 *
 * @return:
 *	0 on success.
 *	-ENOMEM: Insufficient memory.
 *	-EINVAL: Invalid args.
 */
extern int msm_dcvs_scm_create_group(uint32_t id);

/**
 * Registers cores as part of a group
 *
 * @core_id: The core identifier that will be used for communication with DCVS
 * @group_id: The group to which this core will be added to.
 * @param: The core parameters
 * @freq: Array of frequency and energy values
 *
 * @return:
 *	0 on success.
 *	-ENOMEM: Insufficient memory.
 *	-EINVAL: Invalid args.
 */
extern int msm_dcvs_scm_register_core(uint32_t core_id, uint32_t group_id,
		struct msm_dcvs_core_param *param,
		struct msm_dcvs_freq_entry *freq);

/**
 * Set DCVS algorithm parameters
 *
 * @core_id: The algorithm parameters specific for the core
 * @param: The param data structure
 *
 * @return:
 *	0 on success.
 *	-EINVAL: Invalid args.
 */
extern int msm_dcvs_scm_set_algo_params(uint32_t core_id,
		struct msm_dcvs_algo_param *param);

/**
 * Set MPDecision algorithm parameters
 *
 * @param: The param data structure
 *	0 on success.
 *	-EINVAL: Invalid args.
 */
extern int msm_mpd_scm_set_algo_params(struct msm_mpd_algo_param *param);

/**
 * Do an SCM call.
 *
 * @core_id: The core identifier.
 * @event_id: The event that occured.
 *	Possible values:
 *	MSM_DCVS_SCM_IDLE_ENTER
 *		@param0: unused
 *		@param1: unused
 *		@ret0: unused
 *		@ret1: unused
 *	MSM_DCVS_SCM_IDLE_EXIT
 *		@param0: Did the core iowait
 *		@param1: unused
 *		@ret0: New clock frequency for the core in KHz
 *		@ret1: New QoS timer value for the core in usec
 *	MSM_DCVS_SCM_QOS_TIMER_EXPIRED
 *		@param0: unused
 *		@param1: unused
 *		@ret0: New clock frequency for the core in KHz
 *		@ret1: unused
 *	MSM_DCVS_SCM_CLOCK_FREQ_UPDATE
 *		@param0: active clock frequency of the core in KHz
 *		@param1: time taken in usec to switch to the frequency
 *		@ret0: New QoS timer value for the core in usec
 *		@ret1: unused
 *	MSM_DCVS_SCM_ENABLE_CORE
 *		@param0: enable(1) or disable(0) core
 *		@param1: active clock frequency of the core in KHz
 *		@ret0: New clock frequency for the core in KHz
 *		@ret1: unused
 *	MSM_DCVS_SCM_RESET_CORE
 *		@param0: active clock frequency of the core in KHz
 *		@param1: unused
 *		@ret0: New clock frequency for the core in KHz
 *		@ret1: unused
 * @return:
 *	0 on success,
 *	SCM return values
 */
extern int msm_dcvs_scm_event(uint32_t core_id,
		enum msm_dcvs_scm_event event_id,
		uint32_t param0, uint32_t param1,
		uint32_t *ret0, uint32_t *ret1);

#else
static inline int msm_dcvs_scm_init(uint32_t phy, size_t bytes)
{ return -ENOSYS; }
static inline int msm_dcvs_scm_create_group(uint32_t id)
{ return -ENOSYS; }
static inline int msm_dcvs_scm_register_core(uint32_t core_id,
		uint32_t group_id,
		struct msm_dcvs_core_param *param,
		struct msm_dcvs_freq_entry *freq)
{ return -ENOSYS; }
static inline int msm_dcvs_scm_set_algo_params(uint32_t core_id,
		struct msm_dcvs_algo_param *param)
{ return -ENOSYS; }
static inline int msm_mpd_scm_set_algo_params(
		struct msm_mpd_algo_param *param)
{ return -ENOSYS; }
static inline int msm_dcvs_scm_event(uint32_t core_id,
		enum msm_dcvs_scm_event event_id,
		uint32_t param0, uint32_t param1,
		uint32_t *ret0, uint32_t *ret1)
{ return -ENOSYS; }
#endif

#endif
