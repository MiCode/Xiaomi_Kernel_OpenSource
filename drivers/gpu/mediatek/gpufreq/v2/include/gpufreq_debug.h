/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_DEBUG_H__
#define __GPUFREQ_DEBUG_H__

/**************************************************
 * Definition
 **************************************************/
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
	int opp_num;
	int signed_opp_num;
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
	int opp_num;
	int signed_opp_num;
	unsigned int dvfs_state;
	unsigned int shader_present;
	unsigned int aging_enable;
	unsigned int avs_enable;
	unsigned int gpm_enable;
	unsigned int sb_version;
	unsigned int ptp_version;
	unsigned int temp_compensate;
};

struct gpufreq_debug_limit_info {
	int ceiling;
	unsigned int c_limiter;
	unsigned int c_priority;
	int floor;
	unsigned int f_limiter;
	unsigned int f_priority;
};

struct gpufreq_asensor_info {
	unsigned int efuse1;
	unsigned int efuse2;
	unsigned int efuse3;
	unsigned int efuse4;
	unsigned int efuse1_addr;
	unsigned int efuse2_addr;
	unsigned int efuse3_addr;
	unsigned int efuse4_addr;
	unsigned int a_t0_efuse1;
	unsigned int a_t0_efuse2;
	unsigned int a_t0_efuse3;
	unsigned int a_t0_efuse4;
	unsigned int a_tn_sensor1;
	unsigned int a_tn_sensor2;
	unsigned int a_tn_sensor3;
	unsigned int a_tn_sensor4;
	int a_diff1;
	int a_diff2;
	int a_diff3;
	int a_diff4;
	int tj_max;
	unsigned int aging_table_idx;
	unsigned int aging_table_idx_agrresive;
	unsigned int leakage_power;
	unsigned int lvts5_0_y_temperature;
};

/**************************************************
 * Function
 **************************************************/
void gpufreq_debug_init(unsigned int dual_buck, unsigned int gpueb_support);

#endif /* __GPUFREQ_DEBUG_H__ */
