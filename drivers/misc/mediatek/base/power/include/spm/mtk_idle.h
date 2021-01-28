/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __MTK_IDLE_H__
#define __MTK_IDLE_H__

/**********************************************************
 * mtk idle types
 **********************************************************/

enum mtk_idle_type_id {
	IDLE_TYPE_DP = 0,
	IDLE_TYPE_SO3,
	IDLE_TYPE_SO,
	IDLE_TYPE_RG,
	NR_IDLE_TYPES,
	NR_TYPES = NR_IDLE_TYPES,
};

static inline const char *mtk_idle_name(int idle_type)
{
	return idle_type == IDLE_TYPE_DP ? "dpidle" :
		idle_type == IDLE_TYPE_SO3 ? "sodi3" :
		idle_type == IDLE_TYPE_SO ? "sodi" :
		idle_type == IDLE_TYPE_RG ? "rgidle" : "null";
}

/* --------------------------------------------------------
 * mtk idle notification
 **********************************************************/
#include <linux/notifier.h>

enum mtk_idle_notify_id {
	NOTIFY_DPIDLE_ENTER = 0,
	NOTIFY_DPIDLE_LEAVE,
	NOTIFY_SOIDLE_ENTER,
	NOTIFY_SOIDLE_LEAVE,
	NOTIFY_SOIDLE3_ENTER,
	NOTIFY_SOIDLE3_LEAVE,
};

extern int mtk_idle_notifier_register(struct notifier_block *n);
extern void mtk_idle_notifier_unregister(struct notifier_block *n);


/* --------------------------------------------------------
 * For MCDI module
 **********************************************************/

extern int mtk_idle_select(int cpu);    /* return idle_type */
extern int dpidle_enter(int cpu);       /* dpidle */
extern int soidle_enter(int cpu);       /* sodi */
extern int soidle3_enter(int cpu);      /* sodi3 */


/* --------------------------------------------------------
 * For ufs module
 **********************************************************/

extern void idle_lock_by_ufs(unsigned int lock);


/* --------------------------------------------------------
 * For other modules
 **********************************************************/

/* For DVT only: Verify SODI/DP w/o MCDI enable */
/* #define MTK_IDLE_DVT_TEST_ONLY */
extern int mtk_idle_enter_dvt(int cpu);

/* FIXME: DPIDLE - Refine dpidle api */
#define dpidle_active_status() mtk_dpidle_is_active()
extern struct timeval pre_dpidle_time;
extern bool mtk_dpidle_is_active(void);

/* Call as disp driver is ready */
extern void mtk_idle_disp_is_ready(bool enable);


/* --------------------------------------------------------
 * Misc: get recent idle ratio
 **********************************************************/

/* FIXME: To be removed. Externally used by hps, only for mt6799 project */
struct mtk_idle_recent_ratio {
	unsigned long long value;
	unsigned long long value_dp;
	unsigned long long value_so3;
	unsigned long long value_so;
	unsigned long long last_end_ts;
	unsigned long long start_ts;
	unsigned long long end_ts;
};

/* FIXME: To be removed. Externally used by hps, only for mt6799 project */
void mtk_idle_recent_ratio_get(int *window_length_ms,
	struct mtk_idle_recent_ratio *ratio);

/* FIXME: To be removed. Externally used by hps, only for mt6799 project */
unsigned long long idle_get_current_time_ms(void);


#endif /* __MTK_IDLE_H__ */
