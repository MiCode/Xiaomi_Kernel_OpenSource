/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __GPUEB_DEBUG_H__
#define __GPUEB_DEBUG_H__

/**************************************************
 * Definition
 **************************************************/
#define WDT_EXCEPTION_EN                "5A5A5A5A"

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
 * Enumeration and Structure
 **************************************************/
static char *gpueb_dram_user_name[] = {
	"GPU_PWR_ON",          //0
	"GPUFREQ",             //1
	"GPUMPU",              //2
	"GPUEB_MET",           //3
	"LOGGER",              //4
	"PLATSERV",            //5
	"REMAP_TEST",          //6
};

/**************************************************
 * Function
 **************************************************/
void gpueb_debug_init(struct platform_device *pdev);
void gpueb_trigger_wdt(const char *name);
void gpu_set_rgx_bus_secure(void);
void gpueb_dump_status(void);
void gpueb_dump_footprint(void);

#endif /* __GPUEB_DEBUG_H__ */
