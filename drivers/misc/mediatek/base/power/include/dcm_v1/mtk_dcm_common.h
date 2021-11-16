/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_DCM_COMMON_H__
#define __MTK_DCM_COMMON_H__

#include <linux/ratelimit.h>

#define DCM_OFF (0)
#define DCM_ON (1)

#define TAG	"[Power/dcm] "
#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) TAG " : " fmt

#define dcm_pr_err(fmt, args...)		pr_err(fmt, ##args)
#define dcm_pr_warn(fmt, args...)		pr_warn(fmt, ##args)
#define dcm_pr_info_limit(fmt, args...)		pr_info_ratelimited(fmt, ##args)
#define dcm_pr_info(fmt, args...)		pr_info(fmt, ##args)
#define dcm_pr_dbg(fmt, args...)			\
	do {						\
		if (dcm_debug)				\
			pr_info(fmt, ##args);	\
	} while (0)

/** macro **/
#define and(v, a) ((v) & (a))
#define or(v, o) ((v) | (o))
#define aor(v, a, o) (((v) & (a)) | (o))

#define DCM_BASE_INFO(_name) \
{ \
	.name = #_name, \
	.base = &_name, \
}
/*****************************************************/
typedef int (*DCM_FUNC)(int);
typedef bool (*DCM_FUNC_IS_ON)(void);
typedef void (*DCM_PRESET_FUNC)(void);

struct DCM_BASE {
	char *name;
	unsigned long *base;
};

struct DCM {
	int current_state;
	int saved_state;
	int disable_refcnt;
	int default_state;
	DCM_FUNC func;
	DCM_FUNC_IS_ON func_is_on;
	DCM_PRESET_FUNC preset_func;
	int typeid;
	char *name;
};

extern short dcm_debug;
extern short dcm_initiated;
extern unsigned int all_dcm_type;
extern unsigned int init_dcm_type;
extern struct mutex dcm_lock;

void dcm_dump_regs(void);
int dcm_smc_get_cnt(int type_id);
void dcm_smc_msg_send(unsigned int msg);
short is_dcm_bringup(void);

#endif /* #ifndef __MTK_DCM_COMMON_H__ */

