/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_IPI_H__
#define __GPUFREQ_IPI_H__

/**************************************************
 * IMPORTANT:
 * This file must be aligned with GPUEB gpufreq_ipi.h
 **************************************************/

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_IPI_DATA_LEN \
	(sizeof(struct gpufreq_ipi_data) / sizeof(unsigned int))
#define GPUFREQ_SHARED_STATUS_SIZE \
	sizeof(struct gpufreq_shared_status)

/**************************************************
 * IPI Command ID
 **************************************************/
static char *gpufreq_ipi_cmd_name[] = {
	/* Common */
	"CMD_INIT_SHARED_MEM",        // 0
	"CMD_GET_FREQ_BY_IDX",        // 1
	"CMD_GET_POWER_BY_IDX",       // 2
	"CMD_GET_OPPIDX_BY_FREQ",     // 3
	"CMD_GET_LEAKAGE_POWER",      // 4
	"CMD_SET_LIMIT",              // 5
	"CMD_POWER_CONTROL",          // 6
	"CMD_COMMIT",                 // 7
	/* Debug */
	"CMD_GET_DEBUG_OPP_INFO",     // 8
	"CMD_GET_DEBUG_LIMIT_INFO",   // 9
	"CMD_GET_WORKING_TABLE",      // 10
	"CMD_GET_SIGNED_TABLE",       // 11
	"CMD_GET_LIMIT_TABLE",        // 12
	"CMD_SWITCH_LIMIT",           // 13
	"CMD_FIX_TARGET_OPPIDX",      // 14
	"CMD_FIX_CUSTOM_FREQ_VOLT",   // 15
	"CMD_SET_STRESS_TEST",        // 16
	"CMD_SET_AGING_MODE",         // 17
	"CMD_SET_GPM_MODE",           // 18
	"CMD_SET_TEST_MODE",          // 19
	"CMD_NUM",                    // 20
};

enum gpufreq_ipi_cmd {
	/* Common */
	CMD_INIT_SHARED_MEM           = 0,
	CMD_GET_FREQ_BY_IDX           = 1,
	CMD_GET_POWER_BY_IDX          = 2,
	CMD_GET_OPPIDX_BY_FREQ        = 3,
	CMD_GET_LEAKAGE_POWER         = 4,
	CMD_SET_LIMIT                 = 5,
	CMD_POWER_CONTROL             = 6,
	CMD_COMMIT                    = 7,
	/* Debug */
	CMD_GET_DEBUG_OPP_INFO        = 8,
	CMD_GET_DEBUG_LIMIT_INFO      = 9,
	CMD_GET_WORKING_TABLE         = 10,
	CMD_GET_SIGNED_TABLE          = 11,
	CMD_GET_LIMIT_TABLE           = 12,
	CMD_SWITCH_LIMIT              = 13,
	CMD_FIX_TARGET_OPPIDX         = 14,
	CMD_FIX_CUSTOM_FREQ_VOLT      = 15,
	CMD_SET_STRESS_TEST           = 16,
	CMD_SET_AGING_MODE            = 17,
	CMD_SET_GPM_MODE              = 18,
	CMD_SET_TEST_MODE             = 19,
	CMD_NUM                       = 20,
};

/**************************************************
 * IPI Data Structure
 **************************************************/
struct gpufreq_ipi_data {
	enum gpufreq_ipi_cmd cmd_id;
	unsigned int target;
	union {
		int oppidx;
		int return_value;
		unsigned int freq;
		unsigned int volt;
		unsigned int power;
		unsigned int power_state;
		unsigned int mode;
		struct {
			unsigned long long status_base;
			unsigned long long debug_base;
			unsigned int status_size;
			unsigned int debug_size;
		} addr;
		struct {
			unsigned int freq;
			unsigned int volt;
		} custom;
		struct {
			unsigned int limiter;
			int ceiling_info;
			int floor_info;
		} setlimit;
	} u;
};

struct gpufreq_shared_status {
	int cur_oppidx_gpu;
	int cur_oppidx_stack;
	int opp_num_gpu;
	int opp_num_stack;
	unsigned int cur_fgpu;
	unsigned int cur_fstack;
	unsigned int cur_vgpu;
	unsigned int cur_vstack;
	unsigned int cur_vsram_gpu;
	unsigned int cur_vsram_stack;
	unsigned int cur_power_gpu;
	unsigned int cur_power_stack;
	unsigned int max_power_gpu;
	unsigned int max_power_stack;
	unsigned int min_power_gpu;
	unsigned int min_power_stack;
	unsigned int cur_ceiling_gpu;
	unsigned int cur_floor_gpu;
	unsigned int cur_ceiling_stack;
	unsigned int cur_floor_stack;
	unsigned int cur_c_limiter_gpu;
	unsigned int cur_f_limiter_gpu;
	unsigned int cur_c_limiter_stack;
	unsigned int cur_f_limiter_stack;
	unsigned int power_control;
	unsigned int dvfs_state;
	unsigned int shader_present;
	int power_count;
	unsigned int aging_enable;
	unsigned int avs_enable;
	unsigned int sb_version;
	unsigned int ptp_version;
	struct gpufreq_debug_opp_info opp_info;
	struct gpufreq_debug_limit_info limit_info;
	struct gpufreq_asensor_info asensor_info;
};

#endif /* __GPUFREQ_IPI_H__ */
