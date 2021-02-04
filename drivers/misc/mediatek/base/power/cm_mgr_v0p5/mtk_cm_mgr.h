/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_CM_MGR_H__
#define __MTK_CM_MGR_H__

#if defined(CONFIG_MACH_MT6763)
#include <mtk_cm_mgr_mt6763.h>
#endif

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <mtk_cpufreq_internal.h>
#include <mtk_vcorefs_manager.h>

#define CM_MGR_ARRAY(x)	ARRAY_SIZE(x)
#if 0
#define PROC_FOPS_RW(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
		.write		  = name ## _proc_write,		\
	}

#define PROC_FOPS_RO(name)					\
	static int name ## _proc_open(struct inode *inode,	\
		struct file *file)				\
	{							\
		return single_open(file, name ## _proc_show,	\
			PDE_DATA(inode));			\
	}							\
	static const struct file_operations name ## _proc_fops = {	\
		.owner		  = THIS_MODULE,			\
		.open		   = name ## _proc_open,		\
		.read		   = seq_read,				\
		.llseek		 = seq_lseek,				\
		.release		= single_release,		\
	}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}
#endif

#ifdef CONFIG_MTK_MET
/******************** MET BEGIN ********************/
typedef void (*cm_mgr_value_handler_t) (unsigned int cnt, unsigned int *value);

static struct cm_mgr_met_data met_data;
static cm_mgr_value_handler_t cm_mgr_power_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_count_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_opp_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_loading_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_ratio_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_bw_dbg_handler;
static cm_mgr_value_handler_t cm_mgr_valid_dbg_handler;

#define cm_mgr_MET_REG_FN_VALUE(name)				\
	void cm_mgr_register_##name(cm_mgr_value_handler_t handler)	\
{								\
	name##_dbg_handler = handler;				\
}								\
EXPORT_SYMBOL(cm_mgr_register_##name)

cm_mgr_MET_REG_FN_VALUE(cm_mgr_power);
cm_mgr_MET_REG_FN_VALUE(cm_mgr_count);
cm_mgr_MET_REG_FN_VALUE(cm_mgr_opp);
cm_mgr_MET_REG_FN_VALUE(cm_mgr_loading);
cm_mgr_MET_REG_FN_VALUE(cm_mgr_ratio);
cm_mgr_MET_REG_FN_VALUE(cm_mgr_bw);
cm_mgr_MET_REG_FN_VALUE(cm_mgr_valid);
/********************* MET END *********************/
#endif

enum mt_cpu_dvfs_id;

extern unsigned int mt_cpufreq_get_cur_phy_freq_no_lock(enum mt_cpu_dvfs_id id);

extern void sched_get_percpu_load2(int cpu, bool reset,
	unsigned int *rel_load, unsigned int *abs_load);

extern void (*ged_kpi_set_game_hint_value_fp_cmmgr)(int is_game_mode);

extern spinlock_t cache_lock;

#endif	/* __MTK_CM_MGR_H__ */
