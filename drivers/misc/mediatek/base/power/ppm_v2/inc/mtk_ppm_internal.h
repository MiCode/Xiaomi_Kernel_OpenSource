/*
 * Copyright (C) 2016 MediaTek Inc.
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


#ifndef __MT_PPM_INTERNAL_H__
#define __MT_PPM_INTERNAL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cpufreq.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/printk.h>
#include <linux/sched.h>

#include "mach/mtk_ppm_api.h"
#include "mtk_ppm_platform.h"
#include "mtk_ppm_ipi.h"

/*==============================================================*/
/* Definitions							*/
/*==============================================================*/
/* POLICY */
/* If priority value is the same, it will decide by ppm_policy enum value */
#define PPM_POLICY_PRIO_HIGHEST			(0x0)
#define PPM_POLICY_PRIO_POWER_BUDGET_BASE	(0x1)
#define PPM_POLICY_PRIO_USER_SPECIFY_BASE	(0x40)
#define PPM_POLICY_PRIO_PERFORMANCE_BASE	(0x80)
#define PPM_POLICY_PRIO_SYSTEM_BASE		(0xC0)
#define PPM_POLICY_PRIO_LOWEST			(0xFF)

/* Cluster setting */
#define get_cluster_min_cpufreq_idx(id)		(ppm_main_info.cluster_info[id].dvfs_opp_num - 1)
#define get_cluster_max_cpufreq_idx(id)		(0)
#define get_cluster_min_cpu_core(id)		(0)
#define get_cluster_max_cpu_core(id)		(ppm_main_info.cluster_info[id].core_num)
#define get_cluster_max_cpufreq(id)	\
	((ppm_main_info.cluster_info[id].dvfs_tbl) ? ppm_main_info.cluster_info[id].dvfs_tbl[0].frequency : ~0)
#define get_cluster_min_cpufreq(id)		\
	((ppm_main_info.cluster_info[id].dvfs_tbl)	\
	? ppm_main_info.cluster_info[id].dvfs_tbl[DVFS_OPP_NUM-1].frequency	\
	: 0)

/* loop macros */
#define for_each_ppm_clusters(i)	for (i = 0; i < ppm_main_info.cluster_num; i++)
#define for_each_ppm_clients(i)		for (i = 0; i < NR_PPM_CLIENTS; i++)
#define for_each_ppm_power_state(i)	for (i = 0; i < NR_PPM_POWER_STATE; i++)

/* operation */
#define MAX(a, b)		((a) >= (b) ? (a) : (b))
#define MIN(a, b)		((a) >= (b) ? (b) : (a))

/* LOCK */
#define ppm_lock(lock)		mutex_lock(lock)
#define ppm_unlock(lock)	mutex_unlock(lock)

/* Mode */
#define PPM_MODE_MASK_ALL_MODE	((1 << PPM_MODE_LOW_POWER) |	\
		(1 << PPM_MODE_JUST_MAKE) | (1 << PPM_MODE_PERFORMANCE))
#define PPM_MODE_MASK_LOW_POWER_ONLY	((1 << PPM_MODE_LOW_POWER))
#define PPM_MODE_MASK_JUST_MAKE_ONLY	((1 << PPM_MODE_JUST_MAKE))
#define PPM_MODE_MASK_PERFORMANCE_ONLY	((1 << PPM_MODE_PERFORMANCE))

/* PROCFS */
#define PROC_FOPS_RW(name)							\
static int ppm_ ## name ## _proc_open(struct inode *inode, struct file *file)	\
{										\
	return single_open(file, ppm_ ## name ## _proc_show, PDE_DATA(inode));	\
}										\
static const struct file_operations ppm_ ## name ## _proc_fops = {		\
	.owner	= THIS_MODULE,							\
	.open	= ppm_ ## name ## _proc_open,					\
	.read	= seq_read,							\
	.llseek	= seq_lseek,							\
	.release	= single_release,					\
	.write	= ppm_ ## name ## _proc_write,					\
}

#define PROC_FOPS_RO(name)							\
static int ppm_ ## name ## _proc_open(struct inode *inode, struct file *file)	\
{										\
	return single_open(file, ppm_ ## name ## _proc_show, PDE_DATA(inode));	\
}										\
static const struct file_operations ppm_ ## name ## _proc_fops = {		\
	.owner	= THIS_MODULE,							\
	.open	= ppm_ ## name ## _proc_open,					\
	.read	= seq_read,							\
	.llseek	= seq_lseek,							\
	.release	= single_release,					\
}

#define PROC_ENTRY(name)	{__stringify(name), &ppm_ ## name ## _proc_fops}

/* LOG */
#undef TAG
#define TAG     "[Power/PPM] "

#define ppm_err		ppm_info
#define ppm_warn	ppm_info
#define ppm_info(fmt, args...)		\
	pr_notice(TAG""fmt, ##args)
#define ppm_dbg(type, fmt, args...)				\
	do {							\
		if (ppm_debug & ALL || ppm_debug & type)	\
			ppm_info(fmt, ##args);			\
		else if (type == MAIN)				\
			pr_debug(TAG""fmt, ##args);		\
	} while (0)
#define ppm_ver(fmt, args...)			\
	do {					\
		if (ppm_debug == ALL)		\
			ppm_info(fmt, ##args);	\
	} while (0)
#define ppm_cont(fmt, args...)		\
	pr_cont(fmt, ##args)


#define FUNC_LV_MODULE		BIT(0)	/* module, platform driver interface */
#define FUNC_LV_API		BIT(1)	/* mt_ppm driver global function */
#define FUNC_LV_MAIN		BIT(2)	/* mt_ppm driver main function */
#define FUNC_LV_HICA		BIT(3)	/* mt_ppm driver HICA related function */
#define FUNC_LV_POLICY		BIT(4)	/* mt_ppm driver other policy function */

#define FUNC_ENTER(lv)	\
	do { if ((lv) & ppm_func_lv_mask) ppm_info(">> %s()\n", __func__); } while (0)
#define FUNC_EXIT(lv)	\
	do { if ((lv) & ppm_func_lv_mask) ppm_info("<< %s():%d\n", __func__, __LINE__); } while (0)


/*==============================================================*/
/* Enum								*/
/*==============================================================*/
enum {
	NO_LOG	= 0,
	ALL	= 1 << 0,
	MAIN	= 1 << 1,
	HICA	= 1 << 2,
	DLPT	= 1 << 3,
	USER_LIMIT = 1 << 4,
	TIME_PROFILE = 1 << 5,
	COBRA_ALGO = 1 << 6,
	SYS_BOOST = 1 << 7,
	IPI	= 1 << 8,
};

enum ppm_policy {
	PPM_POLICY_PTPOD = 0,		/* highest priority if priority value is the same */
	PPM_POLICY_UT,
	PPM_POLICY_FORCE_LIMIT,
	PPM_POLICY_PWR_THRO,
	PPM_POLICY_THERMAL,
	PPM_POLICY_DLPT,
	PPM_POLICY_USER_LIMIT,
	PPM_POLICY_LCM_OFF,
	PPM_POLICY_PERF_SERV,
	PPM_POLICY_SYS_BOOST,
	PPM_POLICY_HICA,

	NR_PPM_POLICIES,
};

enum ppm_mode {
	PPM_MODE_LOW_POWER = 0,
	PPM_MODE_JUST_MAKE,
	PPM_MODE_PERFORMANCE,
};

enum power_state_search_policy {
	PERFORMANCE = 0,
	LOW_POWER,
};

/*==============================================================*/
/* Data Structures						*/
/*==============================================================*/
struct ppm_cluster_limit {
	int min_cpufreq_idx;
	int max_cpufreq_idx;
	unsigned int min_cpu_core;
	unsigned int max_cpu_core;
};

struct ppm_policy_req {
	unsigned int cluster_num;
	unsigned int power_budget;
	unsigned int perf_idx;
	struct ppm_cluster_limit *limit;
};

struct ppm_policy_data {
	/* settings */
	const char *name;
	enum ppm_policy	policy;
	unsigned int priority;	/* smaller value has higher priority */
	/* status */
	bool is_enabled;
	bool is_activated;
	bool is_limit_updated;
	/* lock */
	struct mutex lock;
	/* list link */
	struct list_head link;
	/* request to PPM */
	struct ppm_policy_req req;

	/* callbacks */
	enum ppm_power_state (*get_power_state_cb)(enum ppm_power_state cur_state);
	void (*update_limit_cb)(enum ppm_power_state new_state);
	void (*status_change_cb)(bool enable);
	void (*mode_change_cb)(enum ppm_mode mode);
};

struct ppm_cluster_info {
	unsigned int cluster_id;
	unsigned int core_num;
	unsigned int cpu_id;	/* cpu id of the dvfs policy owner */
	unsigned int dvfs_opp_num;
	struct cpufreq_frequency_table *dvfs_tbl;	/* from DVFS driver */
};

struct ppm_data {
	/* status */
	enum ppm_mode cur_mode;
	enum ppm_power_state cur_power_state;
#ifdef PPM_IC_SEGMENT_CHECK
	enum ppm_power_state fix_state_by_segment;
#endif
	bool is_enabled;
	bool is_in_suspend;
	int fixed_root_cluster;
	unsigned int min_power_budget;
	unsigned int min_freq_1LL;
	unsigned int smart_detect_boost;

#ifdef PPM_VPROC_5A_LIMIT_CHECK
	/* enable = 0: skip 5A limit check */
	/* on/off is controlled by thermal */
	bool is_5A_limit_enable;
	bool is_5A_limit_on;
#endif
#ifdef PPM_TURBO_CORE_SUPPORT
	bool is_turbo_core;
#endif

	/* platform settings */
	unsigned int cluster_num;
	enum dvfs_table_type dvfs_tbl_type;

	/* platform dev/driver */
	const struct dev_pm_ops ppm_pm_ops;
	struct platform_device ppm_pdev;
	struct platform_driver ppm_pdrv;

	/* PPM core data */
	struct mutex lock;
	struct ppm_cluster_info *cluster_info;
	struct ppm_client_data client_info[NR_PPM_CLIENTS];
	struct ppm_client_req client_req;
	struct ppm_client_req last_req;	/* for debugging purpose */
	struct list_head policy_list;
};

struct ppm_hica_algo_data {
	enum ppm_power_state cur_state;	/* store current state */
	enum ppm_power_state new_state;	/* newest result by HICA algo. */

	unsigned int ppm_cur_loads;
	unsigned int ppm_cur_tlp;
	unsigned int ppm_cur_nr_heavy_task;
};

struct ppm_user_limit {
	int min_freq_idx;
	int max_freq_idx;
	int min_core_num;
	int max_core_num;
};

struct ppm_userlimit_data {
	bool is_freq_limited_by_user;
	bool is_core_limited_by_user;

	struct ppm_user_limit *limit;
};

struct ppm_state_cluster_limit_data {
	const struct ppm_cluster_limit *state_limit;
	const size_t size;
};

struct ppm_state_transfer_data {
	struct ppm_state_transfer {
		enum ppm_power_state next_state;
		unsigned int mode_mask;
		bool (*transition_rule)(struct ppm_hica_algo_data data, struct ppm_state_transfer *settings);

		/* parameters */
#ifdef PPM_HICA_2P0
		unsigned int capacity_hold_time;
		unsigned int capacity_hold_cnt;
		unsigned int capacity_bond;
		unsigned int bigtsk_hold_time;
		unsigned int bigtsk_hold_cnt;
		unsigned int bigtsk_l_bond;
		unsigned int bigtsk_h_bond;
		unsigned int freq_hold_time;
		unsigned int freq_hold_cnt;
#else /* 1p0, 1p5, 1p75 */
		unsigned int loading_delta;
		unsigned int loading_hold_time;
		unsigned int loading_hold_cnt;
		unsigned int loading_bond;
		unsigned int freq_hold_time;
		unsigned int freq_hold_cnt;
		unsigned int tlp_bond;
#if PPM_HICA_VARIANT_SUPPORT
		unsigned int overutil_l_hold_time;
		unsigned int overutil_l_hold_cnt;
		unsigned int overutil_h_hold_time;
		unsigned int overutil_h_hold_cnt;
#endif
#endif
	} *transition_data;
	size_t size;
};

struct ppm_power_state_data {
	const char *name;
	const enum ppm_power_state state;

	/* state decision */
	unsigned int min_pwr_idx;
	unsigned int max_perf_idx;

	/* state default limit */
	const struct ppm_state_cluster_limit_data *cluster_limit;

	/* state transfer data */
	struct ppm_state_transfer_data *transfer_by_pwr;
	struct ppm_state_transfer_data *transfer_by_perf;
};


/*==============================================================*/
/* Global variables						*/
/*==============================================================*/
extern struct ppm_data ppm_main_info;
extern struct ppm_hica_algo_data ppm_hica_algo_data;
extern struct proc_dir_entry *policy_dir;
extern struct proc_dir_entry *profile_dir;
extern unsigned int ppm_func_lv_mask;
extern unsigned int ppm_debug;
extern met_set_ppm_state_funcMET g_pSet_PPM_State;


/*==============================================================*/
/* APIs								*/
/*==============================================================*/
/* procfs */
extern int ppm_procfs_init(void);
extern char *ppm_copy_from_user_for_proc(const char __user *buffer, size_t count);

/* hica */
extern void ppm_hica_set_default_limit_by_state(enum ppm_power_state state,
					struct ppm_policy_data *policy);
extern enum ppm_power_state ppm_hica_get_state_by_perf_idx(enum ppm_power_state state, unsigned int perf_idx);
extern enum ppm_power_state ppm_hica_get_state_by_pwr_budget(enum ppm_power_state state, unsigned int budget);
extern enum ppm_power_state ppm_hica_get_cur_state(void);
extern void ppm_hica_fix_root_cluster_changed(int cluster_id);

/* lcmoff */
extern bool ppm_lcmoff_is_policy_activated(void);

/* power state */
extern struct ppm_power_state_data *ppm_get_power_state_info(void);
extern const char *ppm_get_power_state_name(enum ppm_power_state state);
extern enum ppm_power_state ppm_change_state_with_fix_root_cluster(
			enum ppm_power_state cur_state, int cluster);
extern unsigned int ppm_get_root_cluster_by_state(enum ppm_power_state cur_state);
extern enum ppm_power_state ppm_find_next_state(enum ppm_power_state state,
			unsigned int *level, enum power_state_search_policy policy);
/* platform dependent APIs */
extern void ppm_update_req_by_pwr(enum ppm_power_state new_state, struct ppm_policy_req *req);
extern int ppm_find_pwr_idx(struct ppm_cluster_status *cluster_status);
extern int ppm_get_max_pwr_idx(void);
extern enum ppm_power_state ppm_judge_state_by_user_limit(enum ppm_power_state cur_state,
			struct ppm_userlimit_data user_limit);
extern void ppm_limit_check_for_user_limit(enum ppm_power_state cur_state, struct ppm_policy_req *req,
			struct ppm_userlimit_data user_limit);

/* main */
extern int ppm_main_freq_to_idx(unsigned int cluster_id,
					unsigned int freq, unsigned int relation);
extern void ppm_main_clear_client_req(struct ppm_client_req *c_req);
extern int ppm_main_register_policy(struct ppm_policy_data *policy);
extern void ppm_main_unregister_policy(struct ppm_policy_data *policy);

/* profiling */
extern int ppm_profile_init(void);
extern void ppm_profile_exit(void);
extern void ppm_profile_state_change_notify(enum ppm_power_state old_state, enum ppm_power_state new_state);
extern void ppm_profile_update_client_exec_time(enum ppm_client client, unsigned long long time);
#ifdef PPM_SSPM_SUPPORT
extern void ppm_profile_update_ipi_exec_time(int id, unsigned long long time);
#endif

/* SRAM debugging */
#ifdef CONFIG_MTK_RAM_CONSOLE
extern void aee_rr_rec_ppm_cluster_limit(int id, u32 val);
extern void aee_rr_rec_ppm_step(u8 val);
extern void aee_rr_rec_ppm_cur_state(u8 val);
extern void aee_rr_rec_ppm_min_pwr_bgt(u32 val);
extern void aee_rr_rec_ppm_policy_mask(u32 val);
extern void aee_rr_rec_ppm_waiting_for_pbm(u8 val);
#endif

#ifdef __cplusplus
}
#endif

#endif

