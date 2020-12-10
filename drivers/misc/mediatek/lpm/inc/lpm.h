/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_H__
#define __LPM_H__

#include <linux/soc/mediatek/mtk-lpm.h>
#include <linux/cpumask.h>
#include <linux/notifier.h>

#include <lpm_type.h>

#define MTK_LPM_DTS_COMPATIBLE		"mediatek,mtk-lpm"

#define LPM_REQ_NONE				0
#define LPM_REQ_NOLOCK			(1<<0L)
#define LPM_REQ_NOBROADCAST		(1<<1L)
#define LPM_REQ_NOSUSPEND		(1<<2L)
#define LPM_REQ_NOSYSCORE_CB		(1<<3L)

enum LPM_ISSUER_TYPE {
	LPM_ISSUER_SUSPEND,
	LPM_ISSUER_CPUIDLE
};

enum LPM_SUSPEND_TYPE {
	LPM_SUSPEND_SYSTEM,
	LPM_SUSPEND_S2IDLE,
};

enum LPM_ISSUER_LOG_TYPE {
	LOG_SUCCEESS = 0,
	LOG_MCUSYS_NOT_OFF = 0x78797070,
};

struct lpm_issuer {
	int (*log)
		(int type, const char *prefix, void *data);
	int log_type;
};

struct lpm_model_op {
	int (*prompt)(int cpu,
					const struct lpm_issuer *issuer);
	int (*prepare_enter)(int promtp, int cpu,
					const struct lpm_issuer *issuer);
	void (*prepare_resume)(int cpu,
					const struct lpm_issuer *issuer);
	void (*reflect)(int cpu,
					const struct lpm_issuer *issuer);
};

struct lpm_model {
	unsigned int flag;
	struct lpm_model_op op;
};

struct lpm_nb_data {
	int cpu;
	int index;
	struct lpm_model *model;
	struct lpm_issuer *issuer;
	int ret;
};

int lpm_model_register(const char *name, struct lpm_model *lpm);
int lpm_model_unregister(const char *name);

int lpm_issuer_register(struct lpm_issuer *issuer);
int lpm_issuer_unregister(struct lpm_issuer *issuer);


#define LPM_NB_ACT_BIT			24
#define LPM_NB_ATOMIC_ACT_BIT	28
#define LPM_NB_ACT_BITS \
	(LPM_NB_ATOMIC_ACT_BIT - LPM_NB_ACT_BIT)
#define LPM_NB_ACT_MASK	\
	((1<<LPM_NB_ACT_BITS) - 1)
#define LPM_NB_ACT(x) \
	((x & LPM_NB_ACT_MASK)<<LPM_NB_ACT_BIT)
#define LPM_NB_ATOMIC_ACT(x) \
	(x<<LPM_NB_ATOMIC_ACT_BIT)

/* atomic notification id */
#define LPM_NB_AFTER_PROMPT		LPM_NB_ATOMIC_ACT(0x01)
#define LPM_NB_BEFORE_REFLECT		LPM_NB_ATOMIC_ACT(0x02)
/* normal notification id */
#define LPM_NB_PREPARE			LPM_NB_ACT(0x01)
#define LPM_NB_RESUME		LPM_NB_ACT(0x02)

#define LPM_NB_MASK			((1<<LPM_NB_ACT_BIT) - 1)

int lpm_notifier_register(struct notifier_block *n);

int lpm_notifier_unregister(struct notifier_block *n);

int lpm_suspend_registry(const char *name, struct lpm_model *suspend);

int lpm_suspend_type_get(void);

#endif /* __LPM_H__ */
