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

/**************************************************
 * IPI Command ID
 **************************************************/
static char *gpufreq_ipi_cmd_name[] = {
	/* Common */
	"CMD_INIT_SHARED_MEM",
	"CMD_GET_POWER_STATE",
	"CMD_GET_SHADER_PRESENT",
	"CMD_GET_CUR_FREQ",
	"CMD_GET_CUR_VOLT",
	"CMD_GET_CUR_VSRAM",
	"CMD_GET_CUR_POWER",
	"CMD_GET_MAX_POWER",
	"CMD_GET_MIN_POWER",
	"CMD_GET_CUR_OPPIDX",
	"CMD_GET_OPP_NUM",
	"CMD_GET_FREQ_BY_IDX",
	"CMD_GET_POWER_BY_IDX",
	"CMD_GET_OPPIDX_BY_FREQ",
	"CMD_GET_LEAKAGE_POWER",
	"CMD_GET_DYNAMIC_POWER",
	"CMD_SET_LIMIT",
	"CMD_GET_CUR_LIMIT_IDX",
	"CMD_GET_CUR_LIMITER",
	"CMD_POWER_CONTROL",
	"CMD_COMMIT",
	/* Debug */
	"CMD_GET_DEBUG_OPP_INFO",
	"CMD_GET_DEBUG_LIMIT_INFO",
	"CMD_GET_WORKING_TABLE",
	"CMD_GET_SIGNED_TABLE",
	"CMD_GET_LIMIT_TABLE",
	"CMD_SWITCH_LIMIT",
	"CMD_FIX_TARGET_OPPIDX",
	"CMD_FIX_CUSTOM_FREQ_VOLT",
	"CMD_SET_STRESS_TEST",
	"CMD_SET_AGING_MODE",
	"CMD_SET_GPM_MODE",
	"CMD_GET_ASENSOR_INFO",
};

enum gpufreq_ipi_cmd {
	/* Common */
	CMD_INIT_SHARED_MEM           = 0,
	CMD_GET_POWER_STATE           = 1,
	CMD_GET_SHADER_PRESENT        = 2,
	CMD_GET_CUR_FREQ              = 3,
	CMD_GET_CUR_VOLT              = 4,
	CMD_GET_CUR_VSRAM             = 5,
	CMD_GET_CUR_POWER             = 6,
	CMD_GET_MAX_POWER             = 7,
	CMD_GET_MIN_POWER             = 8,
	CMD_GET_CUR_OPPIDX            = 9,
	CMD_GET_OPP_NUM               = 10,
	CMD_GET_FREQ_BY_IDX           = 11,
	CMD_GET_POWER_BY_IDX          = 12,
	CMD_GET_OPPIDX_BY_FREQ        = 13,
	CMD_GET_LEAKAGE_POWER         = 14,
	CMD_GET_DYNAMIC_POWER         = 15,
	CMD_SET_LIMIT                 = 16,
	CMD_GET_CUR_LIMIT_IDX         = 17,
	CMD_GET_CUR_LIMITER           = 18,
	CMD_POWER_CONTROL             = 19,
	CMD_COMMIT                    = 20,
	/* Debug */
	CMD_GET_DEBUG_OPP_INFO        = 21,
	CMD_GET_DEBUG_LIMIT_INFO      = 22,
	CMD_GET_WORKING_TABLE         = 23,
	CMD_GET_SIGNED_TABLE          = 24,
	CMD_GET_LIMIT_TABLE           = 25,
	CMD_SWITCH_LIMIT              = 26,
	CMD_FIX_TARGET_OPPIDX         = 27,
	CMD_FIX_CUSTOM_FREQ_VOLT      = 28,
	CMD_SET_STRESS_TEST           = 29,
	CMD_SET_AGING_MODE            = 30,
	CMD_SET_GPM_MODE              = 31,
	CMD_GET_ASENSOR_INFO          = 32,
	CMD_NUM                       = 33,
};

/**************************************************
 * IPI Data Structure
 **************************************************/
struct gpufreq_ipi_data {
	enum gpufreq_ipi_cmd cmd_id;
	unsigned int target;
	union {
		int oppidx;
		int opp_num;
		int limit_idx;
		int return_value;
		unsigned int freq;
		unsigned int volt;
		unsigned int power;
		unsigned int limit_type;
		unsigned int limiter;
		unsigned int power_state;
		unsigned int shader_present;
		unsigned int mode;
		struct {
			unsigned int base;
			unsigned int size;
		} addr;
		struct {
			unsigned int freq;
			unsigned int volt;
		} dyn;
		struct {
			unsigned int freq;
			unsigned int volt;
		} custom;
		struct {
			unsigned int limiter;
			int ceiling_info;
			int floor_info;
		} setlimit;
		struct gpufreq_debug_opp_info opp_info;
		struct gpufreq_debug_limit_info limit_info;
		struct gpufreq_asensor_info asensor_info;
	} u;
};

#endif /* __GPUFREQ_IPI_H__ */
