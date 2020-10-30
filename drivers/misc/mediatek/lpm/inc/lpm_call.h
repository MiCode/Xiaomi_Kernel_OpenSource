/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_CALL_H__
#define __LPM_CALL_H__

#include <linux/list.h>
#include <lpm_type.h>

struct lpm_callee_ipi {
	int (*send)(int id, const struct lpm_data *val);
	int (*response)(int id);
};

struct lpm_callee_simple {
	int (*set)(unsigned int type, const struct lpm_data *val);
	int (*get)(unsigned int type, struct lpm_data * const res);
};

struct lpm_callee {
	int uid;
	unsigned int ref;
	union {
		struct lpm_callee_ipi ipi;
		struct lpm_callee_simple simple;
	} i;
	struct list_head list;
};

int lpm_callee_registry(struct lpm_callee *callee);
int lpm_callee_unregistry(struct lpm_callee *callee);

int lpm_callee_get_impl(int uid, const struct lpm_callee **callee);
int lpm_callee_put_impl(struct lpm_callee const *callee);


#define lpm_callee_get(uid, callee) ({\
	int ret;\
	const struct lpm_callee *__callee;\
	do {\
		ret = lpm_callee_get_impl(uid, &__callee);\
		if (ret)\
			break;\
		*callee = (typeof(**callee) *)&__callee->i;\
	} while (0); ret; })


#define lpm_callee_put(callee) ({\
	int ret;\
	typeof(((struct lpm_callee *)0)->i) * __call =\
	(typeof(((struct lpm_callee *)0)->i) *)callee;\
	 lpm_callee *__callee =\
		container_of(__call, struct lpm_callee, i);\
	ret = lpm_callee_put_impl(__callee); ret; })


#endif
