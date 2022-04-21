/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUFREQ_HISTORY_DEBUG_H__
#define __GPUFREQ_HISTORY_DEBUG_H__

/**************************************************
 * Definition
 **************************************************/

#define GPUFREQ_DEBUGFS_DIR_NAME "gpufreqv2"
#define HISTORY_TOTAL_SIZE 0x1000


#define DEBUG_FOPS_RO(name)            \
	static int name ## _debug_open(    \
			struct inode *inode,      \
			struct file *file)        \
	{                                 \
		return single_open(           \
				file,                 \
				name ## _debug_show,   \
				NULL);     \
	}                                 \
	static const struct file_operations name ## _debug_fops = \
	{                                 \
		.open = name ## _debug_open,   \
		.read = seq_read,             \
		.llseek = seq_lseek,           \
		.release = single_release,    \
	}
#define DEBUG_ENTRY(name)              \
	{                                 \
		__stringify(name),            \
		&name ## _debug_fops           \
	}

/**************************************************
 * Structure
 **************************************************/


/**************************************************
 * Function
 **************************************************/
int gpufreq_create_debugfs(void);

#endif /* __GPUFREQ_HISTORY_DEBUG_H__ */
