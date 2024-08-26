/*
 * Copyright (C) 2022 Nuvolta Co., Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#ifndef __LINUX_NU6601_QC_H
#define __LINUX_NU6601_QC_H
#include <linux/mutex.h>
#include <linux/workqueue.h>

enum qc35_pluse {
	NO_ACTION = 0,
	DP_16PULSE,
	DM_16PULSE,
	DPDM_3PULSE,
	DPDM_2PULSE,
	DP_COT_PULSE,
	DM_COT_PULSE,
};

enum soft_qc35_dpdm_state {
	DP_16PLUSE_DONE,
	DM_16PLUSE_DONE,
	DPDM_3PLUSE_DONE,
	DP_COT_PLUSE_DONE,
	DM_COT_PLUSE_DONE,
	DPDM_2PLUSE_DONE,
};

enum soft_qc35_type {
	QC35_HVDCP_NONE,
	QC35_HVDCP_30,
	QC35_HVDCP_3_PLUS_18,
	QC35_HVDCP_3_PLUS_27,
	QC35_HVDCP_3_PLUS_40,
	QC35_UNKNOW,
};


#define QC20_5V                 (3)
#define QC20_9V                 (1)
#define QC20_12V                (0)
#define QC30_5V                 (2)

#define FOREACH_STATE(S)			\
	S(QC_NONE),			\
	S(QC20_DONE),			\
	S(QC30),			\
	S(QC30_TRY_WAIT),			\
	S(QC30_DP_16PULSES),			\
	S(QC30_DM_16PULSES),			\
	S(QC30_DONE),			\
	S(QC35),			\
	S(QC35_V6_WAIT),			\
	S(QC35_V6),			\
	S(QC35_DPDM_3PULSES),			\
	S(QC35_V7),			\
	S(QC35_DPDM_2PULSES),			\
	S(QC35_DONE)

#define GENERATE_ENUM(e)	e
#define GENERATE_STRING(s)	#s

enum soft_qc35_state {
	FOREACH_STATE(GENERATE_ENUM)
};

struct soft_qc35;
struct soft_qc35_ops {
    int (*generate_pulses)(struct soft_qc35 *, u8 pulses);
    int (*set_qc_mode)(struct soft_qc35 *, u8 pulses);
    int (*get_vbus)(struct soft_qc35 *);
};

struct soft_qc35 {
	struct mutex lock;		/* state machine lock */
    struct mutex noti_mutex;

    void *private;
	struct delayed_work state_machine;
	enum soft_qc35_state state;
	enum soft_qc35_state delayed_state;
    struct soft_qc35_ops *ops;

	unsigned long delay_ms;
	bool state_machine_running;

	enum soft_qc35_type qc_type;
};

struct soft_qc35 *soft_qc35_register(void *private, struct soft_qc35_ops *ops);
void soft_qc35_unregister(struct soft_qc35 *qc);
void soft_qc35_update_dpdm_state(struct soft_qc35 *qc, 
		enum soft_qc35_dpdm_state state );
int qc35_detect_start(struct soft_qc35 *bc);
int qc35_detect_stop(struct soft_qc35 *bc);
int qc35_register_notifier(struct soft_qc35 *bc, struct notifier_block *nb);
void qc35_unregister_notifier(struct soft_qc35 *bc, struct notifier_block *nb);

#endif /* __LINUX_NU6601_QC_H */
