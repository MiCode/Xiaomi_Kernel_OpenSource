/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __MTK_PERFOBSERVER_H__
#define __MTK_PERFOBSERVER_H__

#include <linux/notifier.h>

enum pob_dqd_info_num {
	POB_BQD_QUEUE,
	POB_BQD_ACQUIRE,
};

struct pob_bqd_info {
	unsigned long long bqid;
	int connectapi;
	unsigned long long cameraid;
};

#ifdef CONFIG_MTK_PERF_OBSERVER
extern int pob_bqd_register_client(struct notifier_block *nb);
extern int pob_bqd_unregister_client(struct notifier_block *nb);

extern int pob_bqd_queue_update(unsigned long long bufferid,
			int connectapi,
			unsigned long long cameraid);
extern int pob_bqd_acquire_update(unsigned long long bufferid,
			int connectapi);
#else
static inline int pob_bqd_register_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_bqd_unregister_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_bqd_queue_update(unsigned long long bufferid,
			int connectapi,
			unsigned long long cameraid)
{ return 0; }
static inline int pob_bqd_acquire_update(unsigned long long bufferid,
			int connectapi)
{ return 0; }
#endif

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

struct pob_rs_quaweitime_info {
	int MaxQWCPUTime;
	int MaxQWGPUTime;
	int MaxQWAPUTime;
};

enum pob_rs_info_num {
	POB_RS_CPURESCUE_START,
	POB_RS_CPURESCUE_END,
	POB_RS_QUAWEITIME,
};

#ifdef CONFIG_MTK_PERF_OBSERVER
extern int pob_rs_register_client(struct notifier_block *nb);
extern int pob_rs_unregister_client(struct notifier_block *nb);

extern int pob_rs_fps_update(enum pob_rs_info_num info_num);
extern int pob_rs_qw_update(struct pob_rs_quaweitime_info *info);
#else
static inline int pob_rs_register_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_rs_unregister_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_rs_fps_update(enum pob_rs_info_num info_num)
{ return 0; }
static inline int pob_rs_qw_update(struct pob_rs_quaweitime_info *info)
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
#else
static inline int pob_qos_register_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_qos_unregister_client(struct notifier_block *nb)
{ return 0; }
static inline int pob_qos_monitor_update(enum pob_qos_info_num info_num,
						void *info)
{ return 0; }
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

#endif /* end __MTK_PERFOBSERVER_H__ */
