/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __MTK_GPUFREQ_DEBUG_H__
#define __MTK_GPUFREQ_DEBUG_H__

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_DBG_DEFAULT_IDX         (-1)
#define GPUFREQ_DBG_DEFAULT_FREQ        (0)
#define GPUFREQ_DBG_DEFAULT_VOLT        (0)
#define GPUFREQ_DBG_KEY                 "detective"

#if defined(CONFIG_PROC_FS)
#define PROC_FOPS_RW(name)            \
	static int name ## _proc_open(    \
			struct inode *inode,      \
			struct file *file)        \
	{                                 \
		return single_open(           \
				file,                 \
				name ## _proc_show,   \
				PDE_DATA(inode));     \
	}                                 \
	static const struct proc_ops name ## _proc_fops = \
	{                                 \
		.proc_open = name ## _proc_open,   \
		.proc_read = seq_read,             \
		.proc_lseek = seq_lseek,           \
		.proc_release = single_release,    \
		.proc_write = name ## _proc_write, \
	}

#define PROC_FOPS_RO(name)            \
	static int name ## _proc_open(    \
			struct inode *inode,      \
			struct file *file)        \
	{                                 \
		return single_open(           \
				file,                 \
				name ## _proc_show,   \
				PDE_DATA(inode));     \
	}                                 \
	static const struct proc_ops name ## _proc_fops = \
	{                                 \
		.proc_open = name ## _proc_open,   \
		.proc_read = seq_read,             \
		.proc_lseek = seq_lseek,           \
		.proc_release = single_release,    \
	}

#define PROC_ENTRY(name)              \
	{                                 \
		__stringify(name),            \
		&name ## _proc_fops           \
	}
#endif /* CONFIG_PROC_FS */

/**************************************************
 * Structure
 **************************************************/
struct gpufreq_debug_status {
	unsigned int opp_num;
	unsigned int signed_opp_num;
	int fixed_oppidx;
	unsigned int fixed_freq;
	unsigned int fixed_volt;
};

struct gpufreq_debug_opp_info {
	int cur_oppidx;
	unsigned int cur_freq;
	unsigned int cur_volt;
	unsigned int cur_vsram;
	unsigned int fmeter_freq;
	unsigned int con1_freq;
	unsigned int regulator_volt;
	unsigned int regulator_vsram;
	int buck_count;
	int mtcmos_count;
	int cg_count;
	int power_count;
	unsigned int segment_id;
	int segment_upbound;
	int segment_lowbound;
	unsigned int dvfs_state;
	unsigned int shader_present;
	bool aging_enable;
	bool stress_test_enable;
};

struct gpufreq_debug_limit_info {
	int ceiling;
	unsigned int c_limiter;
	unsigned int c_priority;
	int floor;
	unsigned int f_limiter;
	unsigned int f_priority;
};

/**************************************************
 * Function
 **************************************************/
int gpufreq_debug_init(void);

#endif /* __MTK_GPUFREQ_DEBUG_H__ */
