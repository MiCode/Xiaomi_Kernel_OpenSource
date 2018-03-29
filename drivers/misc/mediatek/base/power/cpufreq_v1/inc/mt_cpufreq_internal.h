/*
 * Copyright (C) 2015 MediaTek Inc.
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

/*
 * @file mt_cpufreq_internal.h
 * @brief CPU DVFS driver interface
 */

#ifndef __MT_CPUFREQ_INTERNAL_H__
#define __MT_CPUFREQ_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

/* system includes */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/jiffies.h>
#include <linux/bitops.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/types.h>
#include <linux/suspend.h>
#include <linux/topology.h>
#include <mt-plat/sync_write.h>
#include <mt-plat/mt_io.h>
#include <mt-plat/aee.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif

#include "mach/mt_cpufreq_api.h"
#include "mt_cpufreq_config.h"
#include "mt_cpufreq_struct.h"

#define CPU_LEVEL_0             (0x0)
#define CPU_LEVEL_1             (0x1)
#define CPU_LEVEL_2             (0x2)
#define CPU_LEVEL_3             (0x3)
#define CPU_LV_TO_OPP_IDX(lv)   ((lv))	/* cpu_level to opp_idx */

#define MAX(a, b) ((a) >= (b) ? (a) : (b))
#define MIN(a, b) ((a) >= (b) ? (b) : (a))

/*
 * LOCK
 */
extern struct mutex cpufreq_mutex;
extern bool is_in_cpufreq;
#define cpufreq_lock(flags) \
	do { \
		flags = (unsigned long)&flags; \
		mutex_lock(&cpufreq_mutex); \
		is_in_cpufreq = 1;\
	} while (0)

#define cpufreq_unlock(flags) \
	do { \
		flags = (unsigned long)&flags; \
		is_in_cpufreq = 0;\
		mutex_unlock(&cpufreq_mutex); \
	} while (0)


extern struct mutex cpufreq_para_mutex;
#define cpufreq_para_lock(flags) \
	do { \
		flags = (unsigned long)&flags; \
		mutex_lock(&cpufreq_para_mutex); \
	} while (0)

#define cpufreq_para_unlock(flags) \
	do { \
		flags = (unsigned long)&flags; \
		mutex_unlock(&cpufreq_para_mutex); \
	} while (0)

/* Debugging */
extern unsigned int func_lv_mask;

#define DEBUG 1
#undef TAG
#define TAG     "[Power/cpufreq] "

#define cpufreq_err(fmt, args...)       \
	pr_err(TAG"[ERROR]"fmt, ##args)
#define cpufreq_warn(fmt, args...)      \
	pr_warn(TAG"[WARNING]"fmt, ##args)
#define cpufreq_info(fmt, args...)      \
	pr_warn(TAG""fmt, ##args)
#define cpufreq_dbg(fmt, args...)       \
	pr_debug(TAG""fmt, ##args)
#define cpufreq_ver(fmt, args...)       \
	do {                                \
		if (func_lv_mask)           \
			cpufreq_info(TAG""fmt, ##args);    \
	} while (0)

#define FUNC_LV_MODULE         BIT(0)  /* module, platform driver interface */
#define FUNC_LV_CPUFREQ        BIT(1)  /* cpufreq driver interface          */
#define FUNC_LV_API                BIT(2)  /* mt_cpufreq driver global function */
#define FUNC_LV_LOCAL            BIT(3)  /* mt_cpufreq driver local function  */
#define FUNC_LV_HELP              BIT(4)  /* mt_cpufreq driver help function   */

/* #define CONFIG_CPU_DVFS_SHOWLOG 1 */
/*
*  unsigned int func_lv_mask =
* (FUNC_LV_MODULE | FUNC_LV_CPUFREQ | FUNC_LV_API | FUNC_LV_LOCAL | FUNC_LV_HELP);
*/
#ifdef CONFIG_CPU_DVFS_SHOWLOG
#define FUNC_ENTER(lv) \
	do { if ((lv) & func_lv_mask) cpufreq_dbg(">> %s()\n", __func__); } while (0)
#define FUNC_EXIT(lv) \
	do { if ((lv) & func_lv_mask) cpufreq_dbg("<< %s():%d\n", __func__, __LINE__); } while (0)
#else
#define FUNC_ENTER(lv)
#define FUNC_EXIT(lv)
#endif				/* CONFIG_CPU_DVFS_SHOWLOG */

/* PROCFS */
#define PROC_FOPS_RW(name)							\
	static int name ## _proc_open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, name ## _proc_show, PDE_DATA(inode));	\
}									\
static const struct file_operations name ## _proc_fops = {		\
	.owner          = THIS_MODULE,					\
	.open           = name ## _proc_open,				\
	.read           = seq_read,					\
	.llseek         = seq_lseek,					\
	.release        = single_release,				\
	.write          = name ## _proc_write,				\
}

#define PROC_FOPS_RO(name)							\
	static int name ## _proc_open(struct inode *inode, struct file *file)	\
{									\
	return single_open(file, name ## _proc_show, PDE_DATA(inode));	\
}									\
static const struct file_operations name ## _proc_fops = {		\
	.owner          = THIS_MODULE,					\
	.open           = name ## _proc_open,				\
	.read           = seq_read,					\
	.llseek         = seq_lseek,					\
	.release        = single_release,				\
}

#define PROC_ENTRY(name)	{__stringify(name), &name ## _proc_fops}

/*
 * BIT Operation
 */
#define _BIT_(_bit_)                    (unsigned)(1 << (_bit_))
#define _BITS_(_bits_, _val_) \
	((((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define _BITMASK_(_bits_)               (((unsigned) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1))
#define _GET_BITS_VAL_(_bits_, _val_)   (((_val_) & (_BITMASK_(_bits_))) >> ((0) ? _bits_))

/*
 * REG ACCESS
 */
#define cpufreq_read(addr)                  __raw_readl(IOMEM(addr))
#define cpufreq_write(addr, val)            mt_reg_sync_writel((val), ((void *)addr))
#define cpufreq_write_mask(addr, mask, val) \
cpufreq_write(addr, (cpufreq_read(addr) & ~(_BITMASK_(mask))) | _BITS_(mask, val))

#define for_each_cpu_dvfs(i, p)			for (i = 0, p = cpu_dvfs; i < NR_MT_CPU_DVFS; i++, p = &cpu_dvfs[i])
#define for_each_cpu_dvfs_only(i, p)	\
	for (i = 0, p = cpu_dvfs; (i < NR_MT_CPU_DVFS) && (i != MT_CPU_DVFS_CCI); i++, p = &cpu_dvfs[i])
#define for_each_cpu_dvfs_in_buck(i, p, pp)	\
	for (i = 0, p = cpu_dvfs; (i < NR_MT_CPU_DVFS) \
		&& (p->Vproc_buck_id == pp->Vproc_buck_id); i++, p = &cpu_dvfs[i])

#define cpu_dvfs_is(p, id)				(p == &cpu_dvfs[id])
#define cpu_dvfs_is_available(p)		(p->opp_tbl)
#define cpu_dvfs_get_name(p)			(p->name)

#define cpu_dvfs_get_cur_freq(p)		(p->opp_tbl[p->idx_opp_tbl].cpufreq_khz)
#define cpu_dvfs_get_freq_by_idx(p, idx)		(p->opp_tbl[idx].cpufreq_khz)

#define cpu_dvfs_get_max_freq(p)				(p->opp_tbl[0].cpufreq_khz)
#define cpu_dvfs_get_normal_max_freq(p)			(p->opp_tbl[p->idx_normal_max_opp].cpufreq_khz)
#define cpu_dvfs_get_min_freq(p)				(p->opp_tbl[p->nr_opp_tbl - 1].cpufreq_khz)

#define cpu_dvfs_get_cur_volt(p)				(p->opp_tbl[p->idx_opp_tbl].cpufreq_volt)
#define cpu_dvfs_get_volt_by_idx(p, idx)		(p->opp_tbl[idx].cpufreq_volt)
#define cpu_dvfs_get_org_volt_by_idx(p, idx)	(p->opp_tbl[idx].cpufreq_volt_org)

/* Table Define */
#define FP(pos, clk) { \
	.pos_div = pos,			\
	.clk_div = clk,			\
}

struct mt_cpu_freq_method {
	const char pos_div;
	const char clk_div;
};

struct opp_idx_tbl {
	struct mt_cpu_dvfs *p;
	struct mt_cpu_freq_method *slot;
};

enum opp_idx_type {
		CUR_OPP_IDX = 0,
		TARGET_OPP_IDX = 1,

		NR_OPP_IDX,
};

#define OP(khz, volt) {            \
	.cpufreq_khz = khz,             \
	.cpufreq_volt = volt,           \
}

struct mt_cpu_freq_info {
	const unsigned int cpufreq_khz;
	unsigned int cpufreq_volt;
};

struct opp_tbl_info {
	struct mt_cpu_freq_info *const opp_tbl;
	const int size;
};

struct opp_tbl_m_info {
	struct mt_cpu_freq_method *const opp_tbl_m;
};

enum mt_cpu_dvfs_action_id {
	MT_CPU_DVFS_NORMAL,
	MT_CPU_DVFS_PPM,
	MT_CPU_DVFS_ONLINE,
	MT_CPU_DVFS_DP,
	MT_CPU_DVFS_PBM,
	MT_CPU_DVFS_EEM_UPDATE,

	NR_MT_CPU_DVFS_ACTION,
};

enum hp_action_type {
	FREQ_NONE,
	FREQ_HIGH,
	FREQ_LOW,
	FREQ_DEPEND_VOLT,
	FREQ_USR_REQ,

	NR_HP_ACTION_TYPE,
};

struct hp_action_tbl {
	enum mt_cpu_dvfs_id cluster;
	unsigned long action;
	int trigged_core;
	struct {
		enum hp_action_type action_id;
		int freq_idx;
	} hp_action_cfg[NR_MT_CPU_DVFS];
};

enum dvfs_time_profile {
	SET_DVFS = 0,
	SET_FREQ = 1,
	SET_VOLT = 2,
	SET_VPROC = 3,
	SET_VSRAM = 4,
	SET_DELAY = 5,

	NR_SET_V_F,
};

extern void _mt_cpufreq_dvfs_request_wrapper(struct mt_cpu_dvfs *p, int new_opp_idx,
	enum mt_cpu_dvfs_action_id action, void *data);
extern int set_cur_volt_wrapper(struct mt_cpu_dvfs *p, unsigned int volt);
extern void set_cur_freq_wrapper(struct mt_cpu_dvfs *p, unsigned int cur_khz, unsigned int target_khz);

extern struct buck_ctrl_t buck_ctrl[NR_MT_BUCK];
extern struct pll_ctrl_t pll_ctrl[NR_MT_PLL];
extern struct hp_action_tbl cpu_dvfs_hp_action[16];
extern struct mt_cpu_dvfs cpu_dvfs[NR_MT_CPU_DVFS];
extern struct mt_cpu_dvfs *id_to_cpu_dvfs(enum mt_cpu_dvfs_id id);
extern struct buck_ctrl_t *id_to_buck_ctrl(enum mt_cpu_dvfs_buck_id id);
extern struct pll_ctrl_t *id_to_pll_ctrl(enum mt_cpu_dvfs_pll_id id);

extern unsigned int _mt_cpufreq_get_cpu_level(void);
extern u32 get_devinfo_with_index(u32 index);

extern void _kick_PBM_by_cpu(void);
extern unsigned int dvfs_power_mode;
extern unsigned int do_dvfs_stress_test;
extern int dvfs_disable_flag;
extern int release_dvfs;
extern int thres_ll;
extern int thres_l;
extern int thres_b;
extern ktime_t now[NR_SET_V_F];
extern ktime_t delta[NR_SET_V_F];
extern ktime_t max[NR_SET_V_F];

extern cpuVoltsampler_func g_pCpuVoltSampler;

extern int cpufreq_procfs_init(void);
extern char *_copy_from_user_for_proc(const char __user *buffer, size_t count);
extern void _mt_cpufreq_aee_init(void);

/* #ifdef CONFIG_CPU_DVFS_AEE_RR_REC */
#if 1
/* SRAM debugging*/
extern void aee_rr_rec_cpu_dvfs_vproc_big(u8 val);
extern void aee_rr_rec_cpu_dvfs_vproc_little(u8 val);
extern void aee_rr_rec_cpu_dvfs_oppidx(u8 val);
extern u8 aee_rr_curr_cpu_dvfs_oppidx(void);
extern void aee_rr_rec_cpu_dvfs_cci_oppidx(u8 val);
extern u8 aee_rr_curr_cpu_dvfs_cci_oppidx(void);
extern void aee_rr_rec_cpu_dvfs_status(u8 val);
extern u8 aee_rr_curr_cpu_dvfs_status(void);
extern void aee_rr_rec_cpu_dvfs_step(u8 val);
extern u8 aee_rr_curr_cpu_dvfs_step(void);
extern void aee_rr_rec_cpu_dvfs_cb(u8 val);
extern u8 aee_rr_curr_cpu_dvfs_cb(void);
extern void aee_rr_rec_cpufreq_cb(u8 val);
extern u8 aee_rr_curr_cpufreq_cb(void);
#endif

#ifdef __cplusplus
}
#endif
#endif
