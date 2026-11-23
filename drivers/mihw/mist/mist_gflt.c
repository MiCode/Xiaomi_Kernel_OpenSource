// MIUI ADD: Performance_TurboSched
/*
 * Copyright (c) Xiaomi Technologies Co., Ltd. 2019. All rights reserved.
 *
 * File name: mist_gflt.c
 * Descrviption: gpu frame load tracker
 * Author: guchao1@xiaomi.com
 * Version: 1.0
 * Date:  2023/03/01
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define pr_fmt(fmt) "mist-gflt: " fmt
#include <linux/module.h>
#include <linux/mi_soc.h>
#include <linux/pm_qos.h>
#include "mist_gflt.h"

struct gflt_work_struct {
	struct work_struct work;
	struct workqueue_struct *wq;
};

static int gflt_debug = 0;
module_param(gflt_debug, uint, 0644);

static unsigned int sync_cost_thresh = 3; /*ms*/
module_param(sync_cost_thresh, uint, 0644);

static unsigned int gflt_slack_time_jif = 3 * HZ; /*3S*/
module_param(gflt_slack_time_jif, uint, 0644);

static unsigned int gflt_update_freq_failed;
module_param(gflt_update_freq_failed, uint, 0644);

static struct devfreq *gpu_devfreq;
static u64 last_update_time;
static int gflt_prepared = 0;
static int gflt_enable = 0;
static bool mi_soc_governor_enable = false;
static unsigned int frame_period_thresh  = 8; /*ms*/
static struct gflt_work_struct gflt_freq_adjwork;
static struct delayed_work slack_work;
static struct gflt_load_info gpubw_gflt_load;
static struct gflt_load_info adreno_gflt_load;
extern int devfreq_update_target(struct devfreq *devfreq, unsigned long freq);
extern u32 get_ddr_freq(void);
typedef int (*MI_SOC_FREQ_ADJHOOK)(struct devfreq *devf, unsigned int min_ms);
struct soc_freq_update_call {
	MI_SOC_FREQ_ADJHOOK f;
};
struct soc_freq_update_call soc_freq_update_hook[SOC_DEV_NUM];

void mi_soc_register_freq_adjhook(char *name, enum SOC_FREQ_MOD mod,
	MI_SOC_FREQ_ADJHOOK f)
{
	if (!f || mod >= SOC_DEV_NUM)
		return;

	pr_info("%s %s %d %s\n", __FUNCTION__, name , mod,
		(f ? "register" : "unregister"));
	soc_freq_update_hook[mod].f = f;
}
EXPORT_SYMBOL_GPL(mi_soc_register_freq_adjhook);

bool mi_gpu_flt_working(bool print, const char *func)
{
	ktime_t now, delta;
	u64 frame_period_max_thresh = (frame_period_thresh << 1) * NSEC_PER_MSEC;

	if (!mi_soc_governor_enable) {
		if (unlikely(gflt_debug && print))
				pr_info("now func %s-mi_soc_governor_enable is false\n",
					(func ? func : "nop"));
		return false;
	}

	now = ktime_get();
	delta = now - last_update_time;
	if (unlikely(gflt_debug && print)) {
		if (delta >= frame_period_max_thresh)
			pr_info("now %llu last %llu delta %llu thresh %llu runfunc %s\n",
				now, last_update_time,
				now - last_update_time,
				frame_period_max_thresh,
				(func ? func : "nop"));
	}

	return delta < frame_period_max_thresh;
}
EXPORT_SYMBOL_GPL(mi_gpu_flt_working);

static int set_gflt_enable(const char *buf,
				   const struct kernel_param *kp)
{
	unsigned int val = 0;
	if (sscanf(buf, "%u\n", &val) != 1) {
		return -EINVAL;
	}
	gflt_enable = val;
	return 0;
}

static int get_gflt_enable(char *buf, const struct kernel_param *kp)
{
	return snprintf(buf, PAGE_SIZE, "%d \n", gflt_enable);
}

static const struct kernel_param_ops param_ops_gflt_enable = {
	.set = set_gflt_enable,
	.get = get_gflt_enable,
};
module_param_cb(gflt_enable, &param_ops_gflt_enable, NULL, 0644);

int mi_get_gflt_enable(void)
{
	return gflt_enable;
}

void notify_gflt_frame_period_thresh(unsigned int new_thresh)
{
	frame_period_thresh = new_thresh;
}
EXPORT_SYMBOL_GPL(notify_gflt_frame_period_thresh);

void set_gpubw_gflt_load(u64 total_time, u64 busy_time, u64 ram_time, u64 ram_wait)
{
	u64 ddr_freq = (u64)get_ddr_freq();
	do_div(ddr_freq, 1000);

	if (!gflt_enable)
		return;

	gpubw_gflt_load.total_time = total_time;
	gpubw_gflt_load.busy_time = busy_time;
	gpubw_gflt_load.ram_time = ram_time;
	gpubw_gflt_load.ram_wait = ram_wait;

	/*
	 * ram_time: Number of DDR read/write clock cycles
	 * ram_wait: Number of cycles GPU is stalled by DDR
	 * change the cycles to time(us)
	 */
	do_div(gpubw_gflt_load.ram_time, ddr_freq);
	do_div(gpubw_gflt_load.ram_wait, ddr_freq);

	if (unlikely(gflt_debug))
		trace_printk("gpubw total = %llu, busy = %llu, ram_time = %llu, ram_wait = %llu\n",
			gpubw_gflt_load.total_time, gpubw_gflt_load.busy_time,
			gpubw_gflt_load.ram_time, gpubw_gflt_load.ram_wait);
}
EXPORT_SYMBOL_GPL(set_gpubw_gflt_load);

struct gflt_load_info get_gpubw_gflt_load(void)
{
	return gpubw_gflt_load;
}
EXPORT_SYMBOL_GPL(get_gpubw_gflt_load);

void set_adreno_gflt_load(u64 total_time, u64 busy_time,  u64 ram_time, u64 ram_wait)
{
	u64 ddr_freq = (u64)get_ddr_freq();
	do_div(ddr_freq, 1000);

	if (!gflt_enable)
		return;

	adreno_gflt_load.total_time = total_time;
	adreno_gflt_load.busy_time = busy_time;
	adreno_gflt_load.ram_time = ram_time;
	adreno_gflt_load.ram_wait = ram_wait;

	/*
	 * ram_time: Number of DDR read/write clock cycles
	 * ram_wait: Number of cycles GPU is stalled by DDR
	 * change the cycles to time(us)
	 */
	do_div(adreno_gflt_load.ram_time, ddr_freq);
	do_div(adreno_gflt_load.ram_wait, ddr_freq);

	if (unlikely(gflt_debug))
		trace_printk("adreno total = %llu, busy = %llu, ram_time = %llu, ram_wait = %llu\n",
			adreno_gflt_load.total_time, adreno_gflt_load.busy_time,
			adreno_gflt_load.ram_time, adreno_gflt_load.ram_wait);
}
EXPORT_SYMBOL_GPL(set_adreno_gflt_load);

struct gflt_load_info get_adreno_gflt_load(void)
{
	return adreno_gflt_load;
}
EXPORT_SYMBOL_GPL(get_adreno_gflt_load);

static void do_gpu_freq_update_async(struct workqueue_struct *migt_wq)
{
	gflt_freq_adjwork.wq = migt_wq;
	queue_work(migt_wq, &gflt_freq_adjwork.work);
}

static void do_slack_work(struct work_struct *work)
{
	if (unlikely(gflt_debug))
		pr_info("slack_work running now %llu last gpufreq change time %llu\n",
		ktime_get(), last_update_time);

	mi_soc_governor_enable = false;
}

static void do_freq_adjwork(struct work_struct *work)
{
	int failed;
	bool work_pending = false;
	ktime_t tm1, delta;
	struct gflt_work_struct *gflt_work =
			container_of(work, struct gflt_work_struct, work);

	if (!gflt_enable || !gflt_prepared)
		return;

	tm1 = ktime_get();
	delta = tm1 - last_update_time;
	if (delta < (frame_period_thresh >> 1) * NSEC_PER_MSEC)
		return;

	if (!soc_freq_update_hook[GPU_DEV].f)
		return;

	mi_soc_governor_enable = true;
	failed = soc_freq_update_hook[GPU_DEV].f(gpu_devfreq,
				frame_period_thresh >> 1);
	if (failed) {
		gflt_update_freq_failed++;
		if (unlikely(gflt_debug))
			pr_info("mi_soc_gov_update_gpufreq failed\n");
	} else {
		if (unlikely(gflt_debug))
			pr_info("soc_freq_update_hook GPU_DEV now %llu\n", tm1);
		last_update_time = ktime_get();
	}

	work_pending = cancel_delayed_work_sync(&slack_work);
	if (unlikely(delta > sync_cost_thresh && gflt_debug) && work_pending) {
		delta = (ktime_get() - tm1) / NSEC_PER_MSEC;
		pr_info("cancel_delayed_work_sync slack work cost %llu ms", delta);
	}
	queue_delayed_work(gflt_work->wq, &slack_work, gflt_slack_time_jif);
}

void trigger_mist_gflt(struct workqueue_struct *migt_wq)
{
	if (!gflt_enable || !gflt_prepared)
		return;

	if (!migt_wq)
		return;

	if (unlikely(gflt_debug))
		pr_info("%s enter", __FUNCTION__);

	do_gpu_freq_update_async(migt_wq);
}
EXPORT_SYMBOL_GPL(trigger_mist_gflt);

void mist_gflt_start(struct devfreq *devf)
{
	pr_info("%s start", __FUNCTION__);
	if (unlikely(!devf || !devf->profile)) {
		return;
	}

	if (gpu_devfreq != NULL && devf != gpu_devfreq) {
		return;
	}
	gpu_devfreq = devf;
	gflt_prepared = 1;

}
EXPORT_SYMBOL_GPL(mist_gflt_start);

void mist_gflt_stop(struct devfreq *devf)
{
	if (unlikely(!devf || (gpu_devfreq != NULL && devf != gpu_devfreq)))
		return;

	gpu_devfreq = NULL;
	gflt_prepared = 0;
}
EXPORT_SYMBOL_GPL(mist_gflt_stop);

int mist_gflt_init(void)
{
	INIT_WORK(&gflt_freq_adjwork.work, do_freq_adjwork);
	INIT_DELAYED_WORK(&slack_work, do_slack_work);
	pr_info("LINK: %s: inited!\n", __func__);
	return 0;
}
EXPORT_SYMBOL_GPL(mist_gflt_init);
// END Performance_TurboSched
