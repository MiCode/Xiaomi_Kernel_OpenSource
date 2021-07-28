/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_QOS_BOUND_H__
#define __MTK_QOS_BOUND_H__

#define QOS_BOUND_BUF_SIZE		16
#define QOS_BOUND_VER_TAG		0xA3

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
    QOS_SMIBM_GPU,
    QOS_SMIBM_APU,

    NR_QOS_SMIBM_TYPE
};

struct qos_bound_stat {
	unsigned short num;
	unsigned short event;
	unsigned short emibw_mon[NR_QOS_EMIBM_TYPE];
	unsigned short smibw_mon[NR_QOS_SMIBM_TYPE];
};

struct qos_bound {
	unsigned short ver;
	unsigned short apu_num;
	unsigned short idx;
	unsigned short state;
	struct qos_bound_stat stats[QOS_BOUND_BUF_SIZE];
};

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
extern unsigned short get_qos_bound_apubw_mon(int idx, int master);
extern unsigned short get_qos_bound_apulat_mon(int idx, int master);
extern unsigned short get_qos_bound_emibw_mon(int idx, int master);
extern unsigned short get_qos_bound_smibw_mon(int idx, int master);
#endif
