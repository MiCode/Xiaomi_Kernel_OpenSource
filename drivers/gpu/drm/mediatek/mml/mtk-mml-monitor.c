// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Ping-Hsun Wu <ping-hsun.wu@mediatek.com>
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/delay.h>

#include "mtk-mml-driver.h"
#include "mtk-mml-core.h"

#define SWPM_NEEDED_MOUDLE_CNT	5 /* for 1 sys and maximum 4 wrot */
#define VIDO_INT_EN		0x018
#define VIDO_DEBUG		0x0d0
#define SYS_CG_CON0		0x100
#define SYS_CG_CON1		0x110
#define WROT_DEBUG_19		0x00001900
#define INIT_INTERVAL		100 /* us */
#define INIT_INTERVAL_BIAS	50

/* Monitor thread */
static struct task_struct *monitor_tsk;
static bool mml_monitor_enable;
static u32 monitor_cnt;
static u32 monitor_interval = INIT_INTERVAL;
module_param(monitor_interval, int, 0644);

/* Store info of mml comp when probe */
struct mml_comp_with_status {
	u32 status;
	struct mml_comp *comp;
	struct mutex comp_mutex;
};
static struct mml_comp_with_status mml_swpm_comp[SWPM_NEEDED_MOUDLE_CNT];

/* Store status that will update to swpm */
static u32 wrot_status[SWPM_NEEDED_MOUDLE_CNT - 1];
static u32 cg_con0, cg_con1, mml_freq;

/* Function pointer assigned by swpm */
static struct mml_swpm_func swpm_funcs;

struct mml_swpm_func *mml_get_swpm_func(void)
{
	return &swpm_funcs;
}
EXPORT_SYMBOL(mml_get_swpm_func);

/* Called when sys and wrot probe */
void mml_init_swpm_comp(u32 idx, struct mml_comp *comp)
{
	if (idx < SWPM_NEEDED_MOUDLE_CNT) {
		mml_swpm_comp[idx].comp = comp;
		mutex_init(&mml_swpm_comp[idx].comp_mutex);
	} else
		mml_err("[MonitorThread] wrong idx: %d", idx);
}

/* Called when comp clk enable/disable */
void mml_update_comp_status(u32 idx, u32 status)
{
	if (!mml_monitor_enable || !mml_get_swpm_func()->set_func)
		return;

	if (idx < SWPM_NEEDED_MOUDLE_CNT) {
		mutex_lock(&mml_swpm_comp[idx].comp_mutex);
		mml_swpm_comp[idx].status = status;
		mutex_unlock(&mml_swpm_comp[idx].comp_mutex);
	} else
		mml_err("[MonitorThread] wrong idx: %d", idx);
}

/* Called when freq change */
void mml_update_freq_status(u32 freq)
{
	if (!mml_monitor_enable || !mml_get_swpm_func()->set_func)
		return;

	mml_freq = freq;
}

/* Monitor */
static int create_monitor(void *arg)
{
	void __iomem *base;
	u32 i;

	while (!kthread_should_stop()) {
		/* Update status here */
		/* freq*/
		mml_get_swpm_func()->update_freq(mml_freq);

		/* sys : CGs */
		mutex_lock(&mml_swpm_comp[mml_mon_mmlsys].comp_mutex);
		if (mml_swpm_comp[mml_mon_mmlsys].status) {
			base = mml_swpm_comp[mml_mon_mmlsys].comp->base;
			cg_con0 = readl(base + SYS_CG_CON0);
			cg_con1 = readl(base + SYS_CG_CON1);
			mml_get_swpm_func()->update_cg(cg_con0, cg_con1);
		}
		mutex_unlock(&mml_swpm_comp[mml_mon_mmlsys].comp_mutex);

		/* wrots */
		for (i = mml_mon_wrot; i < SWPM_NEEDED_MOUDLE_CNT; i++) {
			mutex_lock(&mml_swpm_comp[i].comp_mutex);
			if (mml_swpm_comp[i].status) {
				base = mml_swpm_comp[i].comp->base;
				writel(WROT_DEBUG_19, base + VIDO_INT_EN);
				/* get bit 3 for wrot status */
				wrot_status[i - mml_mon_wrot] = (readl(base + VIDO_DEBUG) >> 3) & 1;
			}
			mutex_unlock(&mml_swpm_comp[i].comp_mutex);
		}
		mml_get_swpm_func()->update_wrot(wrot_status[0], wrot_status[1],
			wrot_status[2], wrot_status[3]);

		/* Sleep 100us then take status again */
		usleep_range(monitor_interval, monitor_interval + INIT_INTERVAL_BIAS);
	}

	return 0;
}

/* Enable/Disable monitor thread by adb command */
static int enable_monitor_thread(const char *val, const struct kernel_param *kp)
{
	int ret;
	u32 enable;

	ret = kstrtoint(val, 0, &enable);
	if (ret) {
		mml_err("[MonitorThread] Fail to set monitor enable/disable");
		return ret;
	}

	if (!mml_get_swpm_func()->set_func) {
		mml_err("[MonitorThread] mml_get_swpm_func not set");
		return -EPERM;
	}

	mml_monitor_enable = enable;
	if (enable) {  /* enable = 1 */
		if (!monitor_tsk) {
			monitor_tsk = kthread_run(create_monitor, NULL, "mml_monitor_for_swpm");
			if (IS_ERR(monitor_tsk)) {
				ret = PTR_ERR(monitor_tsk);
				monitor_tsk = NULL;
				return ret;
			}
			monitor_cnt++;
			mml_log("[MonitorThread] thread: %s[PID = %d] Hi I'm on",
				monitor_tsk->comm, monitor_tsk->pid);
		} else
			mml_err("[MonitorThread] cur thread running: %s[PID = %d] no need a new one",
				monitor_tsk->comm, monitor_tsk->pid);
	} else { /* disable = 0 */
		if (monitor_tsk) {
			mml_log("[MonitorThread] stop thread");
			kthread_stop(monitor_tsk);
			monitor_tsk = NULL;
			monitor_cnt--;
		} else
			mml_err("[MonitorThread] no valid thread to stop");
	}

	return 0;
}

static struct kernel_param_ops monitor_ops = {
	.set = enable_monitor_thread,
};

module_param_cb(mml_monitor_for_swpm, &monitor_ops, NULL, 0644);
