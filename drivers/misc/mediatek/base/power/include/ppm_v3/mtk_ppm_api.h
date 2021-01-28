/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MT_PPM_API_H__
#define __MT_PPM_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/cpufreq.h>
#include <linux/cpumask.h>


/*==============================================================*/
/* Enum                                                         */
/*==============================================================*/
enum ppm_client {
	PPM_CLIENT_DVFS	= 0,
	PPM_CLIENT_HOTPLUG,

	NR_PPM_CLIENTS,
};

enum dvfs_table_type {
	DVFS_TABLE_TYPE_FY = 0,
	DVFS_TABLE_TYPE_SB,

	NR_DVFS_TABLE_TYPE,
};

enum ppm_sysboost_user {
	BOOST_BY_UT = 0,
	BOOST_BY_WIFI,
	BOOST_BY_PERFSERV,
	BOOST_BY_USB,
	BOOST_BY_USB_PD,
	/* dedicate ID for debugd to avoid over-writing other kernel users */
	BOOST_BY_DEBUGD = 5,
	BOOST_BY_DEBUGD_64,
	BOOST_BY_BOOT_TIME_OPT,

	NR_PPM_SYSBOOST_USER,
};

enum ppm_cluster_lkg {
	CLUSTER_LL_LKG = 0,
	CLUSTER_L_LKG,
	TOTAL_CLUSTER_LKG,
};

/*==============================================================*/
/* Data Structures                                              */
/*==============================================================*/
struct ppm_client_req {
	unsigned int cluster_num;
	unsigned int root_cluster;
	bool is_ptp_policy_activate;
	unsigned int smart_detect;
	cpumask_var_t online_core;
	struct ppm_client_limit {
		unsigned int cluster_id;
		unsigned int cpu_id;

		int min_cpufreq_idx;
		int max_cpufreq_idx;
		unsigned int min_cpu_core;
		unsigned int max_cpu_core;

		bool has_advise_freq;
		bool has_advise_core;
		int advise_cpufreq_idx;
		int advise_cpu_core;
	} *cpu_limit;
};

struct ppm_client_data {
	const char *name;
	enum ppm_client	client;

	/* callback */
	void (*limit_cb)(struct ppm_client_req req);
};

struct ppm_cluster_status {
	int core_num;
	int freq_idx;	/* -1 if core_num = 0 */
	int volt;
};

struct ppm_limit_data {
	int min;
	int max;
};

/*==============================================================*/
/* APIs                                                         */
/*==============================================================*/
extern void mt_ppm_set_dvfs_table(unsigned int cpu,
	struct cpufreq_frequency_table *tbl,
	unsigned int num, enum dvfs_table_type type);
extern void mt_ppm_register_client(enum ppm_client client,
	void (*limit)(struct ppm_client_req req));

/* SYS boost policy */
extern void mt_ppm_sysboost_core(enum ppm_sysboost_user user,
	unsigned int core_num);
extern void mt_ppm_sysboost_freq(enum ppm_sysboost_user user,
	unsigned int freq);
extern void mt_ppm_sysboost_set_core_limit(
	enum ppm_sysboost_user user, unsigned int cluster,
	int min_core, int max_core);
extern void mt_ppm_sysboost_set_freq_limit(
	enum ppm_sysboost_user user, unsigned int cluster,
	int min_freq, int max_freq);

/* DLPT policy */
extern void mt_ppm_dlpt_set_limit_by_pbm(unsigned int limited_power);
extern void mt_ppm_dlpt_kick_PBM(struct ppm_cluster_status *cluster_status,
	unsigned int cluster_num);

/* Thermal policy */
extern void mt_ppm_cpu_thermal_protect(unsigned int limited_power);
extern unsigned int mt_ppm_thermal_get_min_power(void);
extern unsigned int mt_ppm_thermal_get_max_power(void);
extern unsigned int mt_ppm_thermal_get_power_big_max_opp(unsigned int opp);
extern unsigned int mt_ppm_thermal_get_cur_power(void);
extern int ppm_find_pwr_idx(struct ppm_cluster_status *cluster_status);

/* User limit policy */
extern unsigned int mt_ppm_userlimit_cpu_core(unsigned int cluster_num,
	struct ppm_limit_data *data);
extern unsigned int mt_ppm_userlimit_cpu_freq(unsigned int cluster_num,
	struct ppm_limit_data *data);
extern unsigned int mt_ppm_userlimit_freq_limit_by_others(
	unsigned int cluster);
extern void ppm_game_mode_change_cb(int is_game_mode);

/*Hard User limit policy */
extern unsigned int mt_ppm_hard_userlimit_cpu_freq(unsigned int cluster_num,
	struct ppm_limit_data *data);

/* Force limit policy */
extern unsigned int mt_ppm_forcelimit_cpu_core(unsigned int cluster_num,
	struct ppm_limit_data *data);

/* PTPOD policy */
extern void mt_ppm_ptpod_policy_activate(void);
extern void mt_ppm_ptpod_policy_deactivate(void);

extern unsigned int mt_ppm_get_leakage_mw(enum ppm_cluster_lkg cluster);

/* CPI */
extern unsigned int ppm_get_cluster_cpi(unsigned int cluster);

#ifdef __cplusplus
}
#endif

#endif


