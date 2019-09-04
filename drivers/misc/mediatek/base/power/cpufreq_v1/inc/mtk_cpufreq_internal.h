/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __MTK_CPUFREQ_INTERNAL_H__
#define __MTK_CPUFREQ_INTERNAL_H__

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
#include <mt-plat/mtk_io.h>
#include <mt-plat/aee.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif

#include <linux/regulator/consumer.h>
#include "mach/mtk_cpufreq_api.h"
#include "mtk_cpufreq_config.h"
#include "mtk_cpufreq_struct.h"

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

#define TAG	"[Power/cpufreq] "
#define tag_pr_notice(fmt, args...)	pr_notice(TAG fmt, ##args)
#define tag_pr_info(fmt, args...)	pr_info(TAG fmt, ##args)
#define tag_pr_debug(fmt, args...)	pr_debug(TAG fmt, ##args)

#define cpufreq_ver(fmt, args...)		\
do {						\
	if (func_lv_mask)			\
		tag_pr_info(fmt, ##args);	\
} while (0)

#define GEN_DB_ON(condition, fmt, args...)			\
({								\
	int _r = !!(condition);					\
	if (unlikely(_r))					\
		aee_kernel_exception("CPUDVFS", fmt, ##args);	\
	unlikely(_r);						\
})

#define FUNC_LV_MODULE         BIT(0)  /* module, platform driver interface */
#define FUNC_LV_CPUFREQ        BIT(1)  /* cpufreq driver interface          */
#define FUNC_LV_API            BIT(2)  /* mt_cpufreq driver global function */
#define FUNC_LV_LOCAL          BIT(3)  /* mt_cpufreq driver local function  */
#define FUNC_LV_HELP           BIT(4)  /* mt_cpufreq driver help function   */

/* #define CONFIG_CPU_DVFS_SHOWLOG 1 */

#ifdef CONFIG_CPU_DVFS_SHOWLOG
#define FUNC_ENTER(lv) \
	do { if ((lv) & func_lv_mask) \
	tag_pr_debug(">> %s()\n", __func__); } while (0)
#define FUNC_EXIT(lv) \
	do { if ((lv) & func_lv_mask) \
	tag_pr_debug("<< %s():%d\n", __func__, __LINE__); } while (0)
#else
#define FUNC_ENTER(lv)
#define FUNC_EXIT(lv)
#endif				/* CONFIG_CPU_DVFS_SHOWLOG */

/* PROCFS */
#define PROC_FOPS_RW(name)						\
	static int name ## _proc_open(struct inode *inode, struct file *file)\
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

#define PROC_FOPS_RO(name)						\
	static int name ## _proc_open(struct inode *inode, struct file *file)\
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
#define PROC_ENTRY_DATA(name)	\
{__stringify(name), &name ## _proc_fops, g_ ## name}

/*
 * BIT Operation
 */
#define _BIT_(_bit_)                    (unsigned int)(1 << (_bit_))
#define _BITS_(_bits_, _val_) \
((((unsigned int) -1 >> (31 - ((1) ? _bits_))) & \
~((1U << ((0) ? _bits_)) - 1)) & ((_val_)<<((0) ? _bits_)))
#define _BITMASK_(_bits_)               \
(((unsigned int) -1 >> (31 - ((1) ? _bits_))) & ~((1U << ((0) ? _bits_)) - 1))
#define _GET_BITS_VAL_(_bits_, _val_)   \
(((_val_) & (_BITMASK_(_bits_))) >> ((0) ? _bits_))

/*
 * REG ACCESS
 */
#define cpufreq_read(addr)                  __raw_readl(IOMEM(addr))
#define cpufreq_write(addr, val)            \
mt_reg_sync_writel((val), ((void *)addr))
#define cpufreq_write_mask(addr, mask, val) \
cpufreq_write(addr, (cpufreq_read(addr) & ~(_BITMASK_(mask))) | \
_BITS_(mask, val))

extern struct mt_cpu_dvfs cpu_dvfs[NR_MT_CPU_DVFS];

#define for_each_cpu_dvfs(i, p)			\
for (i = 0, p = cpu_dvfs; i < NR_MT_CPU_DVFS; i++, p = &cpu_dvfs[i])

#ifndef ONE_CLUSTER
#define for_each_cpu_dvfs_only(i, p)	\
for (i = 0, p = cpu_dvfs; (i < NR_MT_CPU_DVFS) && \
(i != MT_CPU_DVFS_CCI); i++, p = &cpu_dvfs[i])
#else
#define for_each_cpu_dvfs_only(i, p)	\
for (i = 0, p = cpu_dvfs; i < NR_MT_CPU_DVFS; i++, p = &cpu_dvfs[i])
#endif

#define cpu_dvfs_is(p, id) (p == &cpu_dvfs[id])
#define cpu_dvfs_is_available(p) (p->opp_tbl)
#define cpu_dvfs_get_name(p) (p->name)

#define cpu_dvfs_get_cur_freq(p) (p->opp_tbl[p->idx_opp_tbl].cpufreq_khz)
#define cpu_dvfs_get_freq_by_idx(p, idx) (p->opp_tbl[idx].cpufreq_khz)

#define cpu_dvfs_get_max_freq(p) (p->opp_tbl[0].cpufreq_khz)
#define cpu_dvfs_get_normal_max_freq(p) \
(p->opp_tbl[p->idx_normal_max_opp].cpufreq_khz)
#define cpu_dvfs_get_min_freq(p) (p->opp_tbl[p->nr_opp_tbl - 1].cpufreq_khz)

#define cpu_dvfs_get_cur_volt(p) (p->opp_tbl[p->idx_opp_tbl].cpufreq_volt)
#define cpu_dvfs_get_volt_by_idx(p, idx) (p->opp_tbl[idx].cpufreq_volt)

struct opp_idx_tbl {
	struct mt_cpu_dvfs *p;
	struct mt_cpu_freq_method *slot;
};

enum opp_idx_type {
		CUR_OPP_IDX = 0,
		TARGET_OPP_IDX = 1,

		NR_OPP_IDX,
};

enum mt_cpu_dvfs_action_id {
	MT_CPU_DVFS_NORMAL,
	MT_CPU_DVFS_PPM,
	MT_CPU_DVFS_ONLINE,
	MT_CPU_DVFS_DP,
	MT_CPU_DVFS_EEM_UPDATE,

	NR_MT_CPU_DVFS_ACTION,
};

enum hp_action {
	CPUFREQ_CPU_ONLINE,
	CPUFREQ_CPU_DOWN_PREPARE,
	CPUFREQ_CPU_DOWN_FAIED,

	NR_HP_ACTION,
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

extern int _search_available_freq_idx(struct mt_cpu_dvfs *p,
	unsigned int target_khz, unsigned int relation);
extern int _search_available_freq_idx_under_v(struct mt_cpu_dvfs *p,
	unsigned int volt);
extern void _mt_cpufreq_dvfs_request_wrapper(struct mt_cpu_dvfs *p,
	int new_opp_idx, enum mt_cpu_dvfs_action_id action, void *data);
extern int set_cur_volt_wrapper(struct mt_cpu_dvfs *p, unsigned int volt);
extern void set_cur_freq_wrapper(struct mt_cpu_dvfs *p, unsigned int cur_khz,
	unsigned int target_khz);

extern struct mt_cpu_dvfs *id_to_cpu_dvfs(enum mt_cpu_dvfs_id id);
extern struct buck_ctrl_t *id_to_buck_ctrl(enum mt_cpu_dvfs_buck_id id);
extern struct pll_ctrl_t *id_to_pll_ctrl(enum mt_cpu_dvfs_pll_id id);

extern u32 get_devinfo_with_index(u32 index);
extern int turbo_flag;
extern unsigned int dvfs_init_flag;

extern void _kick_PBM_by_cpu(void);
extern unsigned int dvfs_power_mode;
extern unsigned int sched_dvfs_enable;
extern unsigned int do_dvfs_stress_test;
extern int dvfs_disable_flag;
extern ktime_t now[NR_SET_V_F];
extern ktime_t delta[NR_SET_V_F];
extern ktime_t max[NR_SET_V_F];

extern cpuVoltsampler_func g_pCpuVoltSampler;
extern int is_in_suspend(void);

extern int cpufreq_procfs_init(void);
extern char *_copy_from_user_for_proc(const char __user *buffer, size_t count);

/* SRAM debugging*/
extern void aee_rr_rec_cpu_dvfs_vproc_big(u8 val);
extern void aee_rr_rec_cpu_dvfs_vproc_little(u8 val);
extern void aee_rr_rec_cpu_dvfs_oppidx(u8 val);
extern void aee_rr_rec_cpu_dvfs_cci_oppidx(u8 val);
extern void aee_rr_rec_cpu_dvfs_status(u8 val);
extern void aee_rr_rec_cpu_dvfs_step(u8 val);
extern void aee_rr_rec_cpu_dvfs_cb(u8 val);
extern void aee_rr_rec_cpufreq_cb(u8 val);

extern u8 aee_rr_curr_cpu_dvfs_oppidx(void);
extern u8 aee_rr_curr_cpu_dvfs_cci_oppidx(void);
extern u8 aee_rr_curr_cpu_dvfs_status(void);
extern u8 aee_rr_curr_cpu_dvfs_step(void);
extern u8 aee_rr_curr_cpu_dvfs_cb(void);
extern u8 aee_rr_curr_cpufreq_cb(void);

#endif	/* __MTK_CPUFREQ_INTERNAL_H__ */
