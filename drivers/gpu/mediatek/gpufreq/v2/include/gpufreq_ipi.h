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
	"CMD_GET_DVFS_STATE",
	"CMD_GET_SHADER_PRESENT",
	"CMD_GET_CUR_FREQ",
	"CMD_GET_MAX_FREQ",
	"CMD_GET_MIN_FREQ",
	"CMD_GET_CUR_VOLT",
	"CMD_GET_MAX_VOLT",
	"CMD_GET_MIN_VOLT",
	"CMD_GET_CUR_VSRAM",
	"CMD_GET_CUR_POWER",
	"CMD_GET_MAX_POWER",
	"CMD_GET_MIN_POWER",
	"CMD_GET_CUR_OPPIDX",
	"CMD_GET_MAX_OPPIDX",
	"CMD_GET_MIN_OPPIDX",
	"CMD_GET_OPP_NUM",
	"CMD_GET_FREQ_BY_IDX",
	"CMD_GET_VOLT_BY_IDX",
	"CMD_GET_POWER_BY_IDX",
	"CMD_GET_OPPIDX_BY_FREQ",
	"CMD_GET_OPPIDX_BY_VOLT",
	"CMD_GET_OPPIDX_BY_POWER",
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
	CMD_INIT_SHARED_MEM = 0,
	CMD_GET_POWER_STATE,
	CMD_GET_DVFS_STATE,
	CMD_GET_SHADER_PRESENT,
	CMD_GET_CUR_FREQ,
	CMD_GET_MAX_FREQ,
	CMD_GET_MIN_FREQ,
	CMD_GET_CUR_VOLT,
	CMD_GET_MAX_VOLT,
	CMD_GET_MIN_VOLT,
	CMD_GET_CUR_VSRAM,
	CMD_GET_CUR_POWER,
	CMD_GET_MAX_POWER,
	CMD_GET_MIN_POWER,
	CMD_GET_CUR_OPPIDX,
	CMD_GET_MAX_OPPIDX,
	CMD_GET_MIN_OPPIDX,
	CMD_GET_OPP_NUM,
	CMD_GET_FREQ_BY_IDX,
	CMD_GET_VOLT_BY_IDX,
	CMD_GET_POWER_BY_IDX,
	CMD_GET_OPPIDX_BY_FREQ,
	CMD_GET_OPPIDX_BY_VOLT,
	CMD_GET_OPPIDX_BY_POWER,
	CMD_GET_LEAKAGE_POWER,
	CMD_GET_DYNAMIC_POWER,
	CMD_SET_LIMIT,
	CMD_GET_CUR_LIMIT_IDX,
	CMD_GET_CUR_LIMITER,
	CMD_POWER_CONTROL,
	CMD_COMMIT,
	/* Debug */
	CMD_GET_DEBUG_OPP_INFO,
	CMD_GET_DEBUG_LIMIT_INFO,
	CMD_GET_WORKING_TABLE,
	CMD_GET_SIGNED_TABLE,
	CMD_GET_LIMIT_TABLE,
	CMD_SWITCH_LIMIT,
	CMD_FIX_TARGET_OPPIDX,
	CMD_FIX_CUSTOM_FREQ_VOLT,
	CMD_SET_STRESS_TEST,
	CMD_SET_AGING_MODE,
	CMD_SET_GPM_MODE,
	CMD_GET_ASENSOR_INFO,
	CMD_NUM,
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
		unsigned int dvfs_state;
		unsigned int shader_present;
		unsigned int mode;
		struct {
			unsigned long long base;
			unsigned long long size;
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
