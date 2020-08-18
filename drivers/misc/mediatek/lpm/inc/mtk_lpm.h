/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LP_MODULE_H__
#define __MTK_LP_MODULE_H__

#include <linux/soc/mediatek/mtk-lpm.h>
#include <linux/cpumask.h>
#include <linux/notifier.h>

#include <mtk_lpm_type.h>

#define MTK_LPM_DTS_COMPATIBLE		"mediatek,mtk-lpm"

#define MTK_LP_REQ_NONE				0
#define MTK_LP_REQ_NOLOCK			(1<<0L)
#define MTK_LP_REQ_NOBROADCAST		(1<<1L)
#define MTK_LP_REQ_NOSUSPEND		(1<<2L)
#define MTK_LP_REQ_NOSYSCORE_CB		(1<<3L)

enum MT_LPM_ISSUER_TYPE {
	MT_LPM_ISSUER_SUSPEND,
	MT_LPM_ISSUER_CPUIDLE
};

struct mtk_lpm_issuer {
	int (*log)(int type, const char *prefix, void *data);
};

struct mtk_lpm_model_op {
	int (*prompt)(int cpu,
					const struct mtk_lpm_issuer *issuer);
	int (*prepare_enter)(int promtp, int cpu,
					const struct mtk_lpm_issuer *issuer);
	void (*prepare_resume)(int cpu,
					const struct mtk_lpm_issuer *issuer);
	void (*reflect)(int cpu,
					const struct mtk_lpm_issuer *issuer);
};

struct mtk_lpm_model {
	unsigned int flag;
	struct mtk_lpm_model_op op;
};

struct mtk_lpm_nb_data {
	int cpu;
	int index;
	struct mtk_lpm_model *model;
	struct mtk_lpm_issuer *issuer;
};

int mtk_lp_model_register(const char *name, struct mtk_lpm_model *lpm);
int mtk_lp_model_unregister(const char *name);

int mtk_lp_issuer_register(struct mtk_lpm_issuer *issuer);
int mtk_lp_issuer_unregister(struct mtk_lpm_issuer *issuer);


#define MTK_LPM_NB_ACT_BIT			24
#define MTK_LPM_NB_ATOMIC_ACT_BIT	28
#define MTK_LPM_NB_ACT_BITS \
	(MTK_LPM_NB_ATOMIC_ACT_BIT - MTK_LPM_NB_ACT_BIT)
#define MTK_LPM_NB_ACT_MASK	\
	((1<<MTK_LPM_NB_ACT_BITS) - 1)
#define MTK_LPM_NB_ACT(x) \
	((x & MTK_LPM_NB_ACT_MASK)<<MTK_LPM_NB_ACT_BIT)
#define MTK_LPM_NB_ATOMIC_ACT(x) \
	(x<<MTK_LPM_NB_ATOMIC_ACT_BIT)

/* atomic notification id */
#define MTK_LPM_NB_AFTER_PROMPT		MTK_LPM_NB_ATOMIC_ACT(0x01)
#define MTK_LPM_NB_BEFORE_REFLECT	MTK_LPM_NB_ATOMIC_ACT(0x02)
/* normal notification id */
#define MTK_LPM_NB_PREPARE			MTK_LPM_NB_ACT(0x01)
#define MTK_LPM_NB_RESUME			MTK_LPM_NB_ACT(0x02)

#define MTK_LPM_NB_MASK				((1<<MTK_LPM_NB_ACT_BIT) - 1)

int mtk_lpm_notifier_register(struct notifier_block *n);

int mtk_lpm_notifier_unregister(struct notifier_block *n);

int mtk_lpm_suspend_registry(const char *name, struct mtk_lpm_model *suspend);

extern int mtk_lpm_drv_cpuidle_ops_set(struct mtk_cpuidle_op *op);

#endif /* __MTK_LP_MODULE_H__ */
