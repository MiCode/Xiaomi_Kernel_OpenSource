/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MTK_QOS_BOUND_H__
#define __MTK_QOS_BOUND_H__

#define QOS_BOUND_BUF_SIZE		64

#define QOS_BOUND_BW_FREE		0x1
#define QOS_BOUND_BW_CONGESTIVE		0x2
#define QOS_BOUND_BW_FULL		0x4

#define QOS_BOUND_BW_CONGESTIVE_PCT	70
#define QOS_BOUND_BW_FULL_PCT		95

#define QOS_BOUND_EMI_CH		2

enum qos_emibm_type {
	QOS_EMIBM_TOTAL,
	QOS_EMIBM_CPU,
	QOS_EMIBM_GPU,
	QOS_EMIBM_MM,
	QOS_EMIBM_MD,

	NR_QOS_EMIBM_TYPE
};
enum qos_smibm_type {
	QOS_SMIBM_VENC,
	QOS_SMIBM_CAM,
	QOS_SMIBM_IMG,
	QOS_SMIBM_MDP,
	QOS_SMIBM_GPU,
	QOS_SMIBM_APU,
	QOS_SMIBM_VPU0,
	QOS_SMIBM_VPU1,
	QOS_SMIBM_MDLA,

	NR_QOS_SMIBM_TYPE
};

enum qos_lat_type {
	QOS_LAT_CPU,
	QOS_LAT_VPU0,
	QOS_LAT_VPU1,
	QOS_LAT_MDLA,

	NR_QOS_LAT_TYPE
};

struct qos_bound_stat {
	unsigned short num;
	unsigned short event;
	unsigned short emibw_mon[NR_QOS_EMIBM_TYPE];
	unsigned short emibw_req[NR_QOS_EMIBM_TYPE];
	unsigned short smibw_mon[NR_QOS_SMIBM_TYPE];
	unsigned short smibw_req[NR_QOS_SMIBM_TYPE];
	unsigned short lat_mon[NR_QOS_LAT_TYPE];
};

struct qos_bound {
	unsigned short idx;
	unsigned short state;
	struct qos_bound_stat stats[QOS_BOUND_BUF_SIZE];
};

#ifdef CONFIG_MTK_QOS_FRAMEWORK
extern void qos_bound_init(void);
extern struct qos_bound *get_qos_bound(void);
extern int get_qos_bound_bw_threshold(int state);
extern unsigned short get_qos_bound_idx(void);
extern int register_qos_notifier(struct notifier_block *nb);
extern int unregister_qos_notifier(struct notifier_block *nb);
extern int qos_notifier_call_chain(unsigned long val, void *v);
extern int is_qos_bound_enabled(void);
extern void qos_bound_enable(int enable);
extern int is_qos_bound_stress_enabled(void);
extern void qos_bound_stress_enable(int enable);
extern int is_qos_bound_log_enabled(void);
extern void qos_bound_log_enable(int enable);
extern unsigned int get_qos_bound_count(void);
extern unsigned int *get_qos_bound_buf(void);
#else
__weak void qos_bound_init(void) { }
__weak struct qos_bound *get_qos_bound(void) { return NULL; }
__weak int get_qos_bound_bw_threshold(int state) { return 0; }
__weak unsigned short get_qos_bound_idx(void) { return 0; }
__weak int register_qos_notifier(struct notifier_block *nb) { return 0; }
__weak int unregister_qos_notifier(struct notifier_block *nb) { return 0; }
__weak int qos_notifier_call_chain(unsigned long val, void *v) { return 0; }
__weak int is_qos_bound_enabled(void) { return 0; }
__weak void qos_bound_enable(int enable) { }
__weak int is_qos_bound_stress_enabled(void) { return 0; }
__weak void qos_bound_stress_enable(int enable) { }
__weak int is_qos_bound_log_enabled(void) { return 0; }
__weak void qos_bound_log_enable(int enable) { }
__weak unsigned int get_qos_bound_count(void) { return 0; }
__weak unsigned int *get_qos_bound_buf(void) { return NULL; }
#endif

#endif
