/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
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
#include <linux/cpumask.h>
#include <linux/topology.h>

#include "mtk_ppm_api.h"
#if IS_ENABLED(CONFIG_MTK_PLAT_POWER_MT6765)
#include "mtk_ppm_platform_6765.h"
#else
#include "mtk_ppm_platform.h"
#endif
#include "mtk_ppm_ipi.h"

/*==============================================================*/
/* Definitions                                                  */
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
#define get_cluster_min_cpufreq_idx(id)	\
	(ppm_main_info.cluster_info[id].dvfs_opp_num - 1)
#define get_cluster_max_cpufreq_idx(id)		(0)
#define get_cluster_min_cpu_core(id)		(0)
#define get_cluster_max_cpu_core(id)	\
	(ppm_main_info.cluster_info[id].core_num)
#define get_cluster_max_cpufreq(id)	\
	((ppm_main_info.cluster_info[id].dvfs_tbl)	\
	? ppm_main_info.cluster_info[id].dvfs_tbl[0].frequency	\
	: ~0)
#define get_cluster_min_cpufreq(id)		\
	((ppm_main_info.cluster_info[id].dvfs_tbl)	\
	? ppm_main_info.cluster_info[id].dvfs_tbl[DVFS_OPP_NUM-1].frequency \
	: 0)

/* loop macros */
#define for_each_ppm_clusters(i)	\
	for (i = 0; i < ppm_main_info.cluster_num; i++)
#define for_each_ppm_clients(i)		for (i = 0; i < NR_PPM_CLIENTS; i++)

/* operation */
#ifndef MAX
#define MAX(a, b)		((a) >= (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a, b)		((a) >= (b) ? (b) : (a))
#endif

/* LOCK */
#define ppm_lock(lock)		mutex_lock(lock)
#define ppm_unlock(lock)	mutex_unlock(lock)

/* PROCFS */
#define PROC_FOPS_RW(name)                                                    \
static int ppm_ ## name ## _proc_open(struct inode *inode, struct file *file) \
{                                                                             \
	return single_open(file, ppm_ ## name ## _proc_show, PDE_DATA(inode));\
}                                                                             \
static const struct proc_ops ppm_ ## name ## _proc_fops = {            \
	.proc_open	= ppm_ ## name ## _proc_open,                                 \
	.proc_read	= seq_read,                                                   \
	.proc_lseek	= seq_lseek,                                                  \
	.proc_release	= single_release,                                     \
	.proc_write	= ppm_ ## name ## _proc_write,                                \
}

#define PROC_FOPS_RO(name)                                                    \
static int ppm_ ## name ## _proc_open(struct inode *inode, struct file *file) \
{                                                                             \
	return single_open(file, ppm_ ## name ## _proc_show, PDE_DATA(inode));\
}                                                                             \
static const struct proc_ops ppm_ ## name ## _proc_fops = {            \
	.proc_open	= ppm_ ## name ## _proc_open,                                 \
	.proc_read	= seq_read,                                                   \
	.proc_lseek	= seq_lseek,                                                  \
	.proc_release	= single_release,                                     \
}

#define PROC_ENTRY(name) {__stringify(name), &ppm_ ## name ## _proc_fops}

/* LOG */
#undef TAG
#define TAG     "[Power/PPM] "

#define ppm_err			ppm_info
#define ppm_warn		ppm_info
#define ppm_info(fmt, args...)	pr_notice(TAG""fmt, ##args)
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


#define FUNC_LV_MODULE		BIT(0)	/* module, platform driver interface */
#define FUNC_LV_API		BIT(1)	/* mt_ppm driver global function */
#define FUNC_LV_MAIN		BIT(2)	/* mt_ppm driver main function */
#define FUNC_LV_POLICY		BIT(4)	/* mt_ppm driver policy function */

#define FUNC_ENTER(lv)	\
	do { if ((lv) & ppm_func_lv_mask)	\
		ppm_info(">> %s()\n", __func__); } while (0)
#define FUNC_EXIT(lv)	\
	do { if ((lv) & ppm_func_lv_mask)	\
		ppm_info("<< %s():%d\n", __func__, __LINE__); } while (0)


/*==============================================================*/
/* Enum                                                         */
/*==============================================================*/
enum {
	NO_LOG	= 0,
	ALL	= 1 << 0,
	MAIN	= 1 << 1,
	HICA	= 1 << 2,
	DLPT	= 1 << 3,
	USER_LIMIT = 1 << 4,
	TIME_PROFILE = 1 << 5,
	COBRA = 1 << 6,
	SYS_BOOST = 1 << 7,
	IPI	= 1 << 8,
	CPI	= 1 << 9,
	HARD_USER_LIMIT = 1 << 10,
};

enum ppm_policy {
	PPM_POLICY_PTPOD = 0, /* highest priority */
	PPM_POLICY_UT,
	PPM_POLICY_FORCE_LIMIT,
	PPM_POLICY_PWR_THRO,
	PPM_POLICY_THERMAL,
	PPM_POLICY_DLPT,
	PPM_POLICY_HARD_USER_LIMIT,
	PPM_POLICY_USER_LIMIT,
	PPM_POLICY_LCM_OFF,
	PPM_POLICY_SYS_BOOST,
	PPM_POLICY_HICA,

	NR_PPM_POLICIES,
};


/*==============================================================*/
/* Data Structures                                              */
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
	void (*update_limit_cb)(void);
	void (*status_change_cb)(bool enable);
};

struct ppm_cluster_info {
	unsigned int cluster_id;
	unsigned int core_num;
	unsigned int cpu_id;	/* cpu id of the dvfs policy owner */
	unsigned int dvfs_opp_num;
	unsigned int max_freq_except_userlimit;
	struct cpufreq_frequency_table *dvfs_tbl;	/* from DVFS driver */
	int	doe_max;
	int	doe_min;
};

struct ppm_data {
	bool is_enabled;
	bool is_doe_enabled;
	bool is_in_suspend;
	unsigned int min_power_budget;
	cpumask_var_t exclusive_core;

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


/*==============================================================*/
/* Global variables                                             */
/*==============================================================*/
extern struct ppm_data ppm_main_info;
extern struct proc_dir_entry *policy_dir;
extern struct proc_dir_entry *profile_dir;
extern struct proc_dir_entry *cpi_dir;
extern unsigned int ppm_func_lv_mask;
extern unsigned int ppm_debug;

/*==============================================================*/
/* init/exit                                                    */
/*==============================================================*/
extern int ppm_cpi_init(void);
extern void ppm_cpi_exit(void);
extern int ppm_dlpt_policy_init(void);
extern void ppm_dlpt_policy_exit(void);
extern int ppm_forcelimit_policy_init(void);
extern void ppm_forcelimit_policy_exit(void);
extern int ppm_hard_userlimit_policy_init(void);
extern void ppm_hard_userlimit_policy_exit(void);
extern int ppm_ptpod_policy_init(void);
extern void ppm_ptpod_policy_exit(void);
extern int ppm_pwrthro_policy_init(void);
extern void ppm_pwrthro_policy_exit(void);
extern int ppm_sysboost_policy_init(void);
extern void ppm_sysboost_policy_exit(void);
extern int ppm_thermal_policy_init(void);
extern void ppm_thermal_policy_exit(void);
extern int ppm_userlimit_policy_init(void);
extern void ppm_userlimit_policy_exit(void);
extern int ppm_ut_policy_init(void);
extern void ppm_ut_policy_exit(void);

/* Cannot init before FB driver */
extern int ppm_lcmoff_policy_init(void);
extern void ppm_lcmoff_policy_exit(void);

/* should be run after cpufreq and upower init */
extern int ppm_power_data_init(void);

/*==============================================================*/
/* APIs                                                         */
/*==============================================================*/
extern int mt_ppm_main(void);
/* procfs */
extern int ppm_procfs_init(void);
extern char *ppm_copy_from_user_for_proc(
	const char __user *buffer, size_t count);

/* platform dependent APIs */
extern void ppm_update_req_by_pwr(struct ppm_policy_req *req);
extern int ppm_get_min_pwr_idx(void);
extern int ppm_get_max_pwr_idx(void);

/* main */
extern int ppm_main_freq_to_idx(unsigned int cluster_id,
	unsigned int freq, unsigned int relation);
extern void ppm_clear_policy_limit(struct ppm_policy_data *policy);
extern void ppm_main_clear_client_req(struct ppm_client_req *c_req);
extern int ppm_main_register_policy(struct ppm_policy_data *policy);
extern void ppm_main_unregister_policy(struct ppm_policy_data *policy);

/* profiling */
extern int ppm_profile_init(void);
extern void ppm_profile_exit(void);
extern void ppm_profile_update_client_exec_time(
	enum ppm_client client, unsigned long long time);
#ifdef PPM_SSPM_SUPPORT
extern void ppm_profile_update_ipi_exec_time(int id, unsigned long long time);
#endif

/* SRAM debugging */
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
extern void aee_rr_rec_ppm_cluster_limit(int id, u32 val);
extern void aee_rr_rec_ppm_step(u8 val);
extern void aee_rr_rec_ppm_min_pwr_bgt(u32 val);
extern void aee_rr_rec_ppm_policy_mask(u32 val);
extern void aee_rr_rec_ppm_waiting_for_pbm(u8 val);
#endif

#define trace_ppm_update(a, b, c, d) do { } while (0)
static inline int arch_get_cluster_cpus(struct cpumask *cpus, int cluster_id)
{
	int cpu = 0;

	cpumask_clear(cpus);
	if (cluster_id == 0) {
		cpu = 0;

		while (cpu < CORE_NUM_L) {
			cpumask_set_cpu(cpu, cpus);
			cpu++;
		}
	} else {
		cpu = CORE_NUM_L;

		while (cpu < TOTAL_CORE_NUM) {
			cpumask_set_cpu(cpu, cpus);
			cpu++;
		}
	}

	return 0;
}

static inline int arch_get_nr_clusters(void)
{
	return NR_PPM_CLUSTERS;
}

#ifdef __cplusplus
}
#endif

#endif


