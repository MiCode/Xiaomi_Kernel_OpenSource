/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_LPM_CALL_H__
#define __MTK_LPM_CALL_H__

#include <linux/list.h>
#include <mtk_lpm_type.h>

struct mtk_lpm_callee_ipi {
	int (*send)(int id, const struct mtk_lpm_data *val);
	int (*response)(int id);
};

struct mtk_lpm_callee_simple {
	int (*set)(unsigned int type, const struct mtk_lpm_data *val);
	int (*get)(unsigned int type, struct mtk_lpm_data * const res);
};

struct mtk_lpm_callee {
	int uid;
	unsigned int ref;
	union {
		struct mtk_lpm_callee_ipi ipi;
		struct mtk_lpm_callee_simple simple;
	} i;
	struct list_head list;
};

int mtk_lpm_callee_registry(struct mtk_lpm_callee *callee);
int mtk_lpm_callee_unregistry(struct mtk_lpm_callee *callee);

int mtk_lpm_callee_get_impl(int uid, const struct mtk_lpm_callee **callee);
int mtk_lpm_callee_put_impl(struct mtk_lpm_callee const *callee);


#define mtk_lpm_callee_get(uid, callee) ({\
	int ret;\
	const struct mtk_lpm_callee *__callee;\
	do {\
		ret = mtk_lpm_callee_get_impl(uid, &__callee);\
		if (ret)\
			break;\
		*callee = (typeof(**callee) *)&__callee->i;\
	} while (0); ret; })


#define mtk_lpm_callee_put(callee) ({\
	int ret;\
	typeof(((struct mtk_lpm_callee *)0)->i) * __call =\
	(typeof(((struct mtk_lpm_callee *)0)->i) *)callee;\
	 mtk_lpm_callee *__callee =\
		container_of(__call, struct mtk_lpm_callee, i);\
	ret = mtk_lpm_callee_put_impl(__callee); ret; })


#endif
