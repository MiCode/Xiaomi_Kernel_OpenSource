/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_DEBUG_H__
#define __GPUFREQ_DEBUG_H__

/**************************************************
 * Definition
 **************************************************/
#define GPUFREQ_DIR_NAME                "gpufreqv2"

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
	{                                                 \
		.proc_open = name ## _proc_open,              \
		.proc_read = seq_read,                        \
		.proc_lseek = seq_lseek,                      \
		.proc_release = single_release,               \
		.proc_write = name ## _proc_write,            \
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
	{                                                 \
		.proc_open = name ## _proc_open,              \
		.proc_read = seq_read,                        \
		.proc_lseek = seq_lseek,                      \
		.proc_release = single_release,               \
	}

#define PROC_ENTRY(name)              \
	{                                 \
		__stringify(name),            \
		&name ## _proc_fops           \
	}
#endif /* CONFIG_PROC_FS */

#if defined(CONFIG_DEBUG_FS)
#define DEBUG_FOPS_RO(name)           \
	static int name ## _debug_open(   \
			struct inode *inode,      \
			struct file *file)        \
	{                                 \
		return single_open(           \
				file,                 \
				name ## _debug_show,  \
				NULL);                \
	}                                 \
	static const struct file_operations name ## _debug_fops = \
	{                                                         \
		.open = name ## _debug_open,                          \
		.read = seq_read,                                     \
		.llseek = seq_lseek,                                  \
		.release = single_release,                            \
	}

#define DEBUG_ENTRY(name)             \
	{                                 \
		__stringify(name),            \
		&name ## _debug_fops          \
	}
#endif /* CONFIG_DEBUG_FS */

/**************************************************
 * Structure
 **************************************************/


/**************************************************
 * Function
 **************************************************/
void gpufreq_debug_init(unsigned int dual_buck, unsigned int gpueb_support,
	const struct gpufreq_shared_status *shared_status);

#endif /* __GPUFREQ_DEBUG_H__ */
