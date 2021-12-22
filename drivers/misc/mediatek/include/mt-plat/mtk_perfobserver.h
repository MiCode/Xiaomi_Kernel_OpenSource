/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MTK_PERFOBSERVER_H__
#define __MTK_PERFOBSERVER_H__

#include <linux/notifier.h>

enum pob_fpsgo_info_num {
	POB_FPSGO_TURNON,
	POB_FPSGO_TURNOFF,
	POB_FPSGO_QTSK_ADD,
	POB_FPSGO_QTSK_DEL,
	POB_FPSGO_QTSK_DELALL,
	POB_FPSGO_FSTB_STATS_START,
	POB_FPSGO_FSTB_STATS_UPDATE,
	POB_FPSGO_FSTB_STATS_END,
	POB_FPSGO_QTSK_CPUCAP_UPDATE,
	POB_FPSGO_QTSK_GPUCAP_UPDATE,
	POB_FPSGO_QTSK_APUCAP_UPDATE,
};

struct pob_fpsgo_fpsstats_info {
	int tskid;
	int quantile_weighted_cpu_time;
	int quantile_weighted_gpu_time;
	int quantile_weighted_apu_time;
};

struct pob_fpsgo_qtsk_info {
	int tskid;

	int rescue_cpu;
	unsigned int cur_cpu_cap;
	unsigned int max_cpu_cap;

	unsigned int cur_gpu_cap;
	unsigned int max_gpu_cap;

	unsigned int cur_apu_cap;
	unsigned int max_apu_cap;
};

#ifdef CONFIG_MTK_PERF_OBSERVER
extern int pob_fpsgo_register_client(struct notifier_block *nb);
extern int pob_fpsgo_unregister_client(struct notifier_block *nb);

extern int pob_fpsgo_fstb_stats_update(unsigned long infonum,
			struct pob_fpsgo_fpsstats_info *info);
extern int pob_fpsgo_qtsk_update(unsigned long infonum,
			struct pob_fpsgo_qtsk_info *info);
#else
static inline int pob_fpsgo_register_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_fpsgo_unregister_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_fpsgo_fstb_stats_update(unsigned long infonum,
			struct pob_fpsgo_fpsstats_info *info)
{ return 0; }
static inline int pob_fpsgo_qtsk_update(unsigned long infonum,
			struct pob_fpsgo_qtsk_info *info)
{ return 0; }
#endif

enum pob_qos_info_num {
	POB_QOS_EMI_ALL,
	POB_QOS_EMI_BWBOUND,
	POB_QOS_EMI_LATENCY,
};

struct pob_qos_info {
	int size;
	void *pstats;
};

#ifdef CONFIG_MTK_PERF_OBSERVER
extern int pob_qos_register_client(struct notifier_block *nb);
extern int pob_qos_unregister_client(struct notifier_block *nb);

extern int pob_qos_monitor_update(enum pob_qos_info_num info_num,
					void *info);

extern void pob_qos_tracker(u64 wallclock);
#else
static inline int pob_qos_register_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_qos_unregister_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_qos_monitor_update(enum pob_qos_info_num info_num,
						void *info)
{ return 0; }

static inline void pob_qos_tracker(u64 wallclock)
{ return; }
#endif


enum pob_qos_ind_info_num {
	POB_QOS_IND_BWBOUND_FREE,
	POB_QOS_IND_BWBOUND_CONGESTIVE,
	POB_QOS_IND_BWBOUND_FULL,
};

#ifdef CONFIG_MTK_PERF_OBSERVER
extern int pob_qos_ind_register_client(struct notifier_block *nb);
extern int pob_qos_ind_unregister_client(struct notifier_block *nb);

extern int pob_qos_ind_monitor_update(enum pob_qos_ind_info_num info_num,
					void *info);
#else
static inline int pob_qos_ind_register_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_qos_ind_unregister_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_qos_ind_monitor_update(enum pob_qos_ind_info_num info_num,
					void *info)
{ return 0; }
#endif

enum pob_nn_info_num {
	POB_NN_BEGIN,
	POB_NN_END,
};

struct pob_nn_model_info {
	__u32 pid;
	__u32 tid;
	__u64 mid;
};

#ifdef CONFIG_MTK_PERF_OBSERVER
extern int pob_nn_register_client(struct notifier_block *nb);
extern int pob_nn_unregister_client(struct notifier_block *nb);

extern int pob_nn_update(enum pob_nn_info_num info_num,
				void *info);
#else
static inline int pob_nn_register_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_nn_unregister_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_nn_update(enum pob_nn_info_num info_num,
				void *info)
{ return 0; }
#endif

enum pob_xpufreq_info_num {
	POB_XPUFREQ_VPU,
	POB_XPUFREQ_MDLA,
};

struct pob_xpufreq_info {
	unsigned int id;
	int opp;
};

#ifdef CONFIG_MTK_PERF_OBSERVER
extern int pob_xpufreq_register_client(struct notifier_block *nb);
extern int pob_xpufreq_unregister_client(struct notifier_block *nb);

extern int pob_xpufreq_update(enum pob_xpufreq_info_num info_num,
				struct pob_xpufreq_info *info);
#else
static inline int pob_xpufreq_register_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_xpufreq_unregister_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_xpufreq_update(enum pob_xpufreq_info_num info_num,
			struct pob_xpufreq_info *info)
{ return 0; }
#endif

enum pob_eara_thrm_info_num {
	POB_EARA_THRM_UNTHROTTLED,
	POB_EARA_THRM_THROTTLED,
};

#ifdef CONFIG_MTK_PERF_OBSERVER
extern int pob_eara_thrm_register_client(struct notifier_block *nb);
extern int pob_eara_thrm_unregister_client(struct notifier_block *nb);

extern int pob_eara_thrm_stats_update(enum pob_eara_thrm_info_num info_num);
#else
static inline int pob_eara_thrm_register_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_eara_thrm_unregister_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_eara_thrm_stats_update(
		enum pob_eara_thrm_info_num info_num)
{ return 0; }
#endif

#endif /* end __MTK_PERFOBSERVER_H__ */
