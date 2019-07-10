/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LPM_CALL_H__
#define __MTK_LPM_CALL_H__

#include <linux/list.h>
#include <mtk_lpm_type.h>

struct mtk_lpm_callee_ipi {
	int (*send)(int level, unsigned int ival,
				const struct mtk_lpm_data *pval);
	int (*response)(int level, unsigned int ival,
					struct mtk_lpm_data * const pres);
};

struct mtk_lpm_callee_mio {
	int (*write)(unsigned int type, unsigned int ival,
					const struct mtk_lpm_data *pval);
	int (*read)(unsigned int type, unsigned int ival,
					struct mtk_lpm_data * const pres);
};

struct mtk_lpm_callee_simple {
	int (*set)(unsigned int type,
				unsigned int val,
				const struct mtk_lpm_data *pval);
	int (*get)(unsigned int type,
				unsigned int val,
				struct mtk_lpm_data * const pres);
};

struct mtk_lpm_callee {
	int type;
	unsigned int ref;
	union {
		struct mtk_lpm_callee_ipi ipi;
		struct mtk_lpm_callee_mio mio;
		struct mtk_lpm_callee_simple simple;
	} i;
	struct list_head list;
};

int mtk_lpm_callee_registry(struct mtk_lpm_callee *callee);
int mtk_lpm_callee_unregistry(struct mtk_lpm_callee *callee);

int mtk_lpm_callee_get(int type, const struct mtk_lpm_callee **callee);
int mtk_lpm_callee_put(struct mtk_lpm_callee const *callee);

#endif
