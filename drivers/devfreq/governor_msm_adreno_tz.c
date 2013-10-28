/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/devfreq.h>
#include <linux/math64.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/ftrace.h>
#include <linux/msm_adreno_devfreq.h>
#include <soc/qcom/scm.h>
#include "governor.h"

static DEFINE_SPINLOCK(tz_lock);

/*
 * FLOOR is 5msec to capture up to 3 re-draws
 * per frame for 60fps content.
 */
#define FLOOR			5000
#define LONG_FLOOR		50000
#define HIST			5
#define TARGET			80
#define CAP			75

/*
 * CEILING is 50msec, larger than any standard
 * frame length, but less than the idle timer.
 */
#define CEILING			50000
#define TZ_RESET_ID		0x3
#define TZ_UPDATE_ID		0x4
#define TZ_INIT_ID		0x6

#define TAG "msm_adreno_tz: "

/* Trap into the TrustZone, and call funcs there. */
static int __secure_tz_entry2(u32 cmd, u32 val1, u32 val2)
{
	int ret;
	spin_lock(&tz_lock);
	/* sync memory before sending the commands to tz*/
	__iowmb();
	ret = scm_call_atomic2(SCM_SVC_IO, cmd, val1, val2);
	spin_unlock(&tz_lock);
	return ret;
}

static int __secure_tz_entry3(u32 cmd, u32 val1, u32 val2, u32 val3)
{
	int ret;
	spin_lock(&tz_lock);
	/* sync memory before sending the commands to tz*/
	__iowmb();
	ret = scm_call_atomic3(SCM_SVC_IO, cmd, val1, val2, val3);
	spin_unlock(&tz_lock);
	return ret;
}

static void _update_cutoff(struct devfreq_msm_adreno_tz_data *priv,
				unsigned int norm_max)
{
	int i;

	priv->bus.max = norm_max;
	for (i = 0; i < priv->bus.num; i++) {
		priv->bus.up[i] = priv->bus.p_up[i] * norm_max / 100;
		priv->bus.down[i] = priv->bus.p_down[i] * norm_max / 100;
	}
}

static int tz_get_target_freq(struct devfreq *devfreq, unsigned long *freq,
				u32 *flag)
{
	int result = 0;
	struct devfreq_msm_adreno_tz_data *priv = devfreq->data;
	struct devfreq_dev_status stats;
	struct xstats b;
	int val, level = 0;
	int act_level;
	int norm_cycles;
	int gpu_percent;

	if (priv->bus.num)
		stats.private_data = &b;
	else
		stats.private_data = NULL;
	result = devfreq->profile->get_dev_status(devfreq->dev.parent, &stats);
	if (result) {
		pr_err(TAG "get_status failed %d\n", result);
		return result;
	}

	*freq = stats.current_frequency;
	*flag = 0;
	priv->bin.total_time += stats.total_time;
	priv->bin.busy_time += stats.busy_time;
	if (priv->bus.num) {
		priv->bus.total_time += stats.total_time;
		priv->bus.gpu_time += stats.busy_time;
		priv->bus.ram_time += b.ram_time;
		priv->bus.ram_time += b.ram_wait;
	}

	/*
	 * Do not waste CPU cycles running this algorithm if
	 * the GPU just started, or if less than FLOOR time
	 * has passed since the last run.
	 */
	if ((stats.total_time == 0) ||
		(priv->bin.total_time < FLOOR)) {
		return 1;
	}

	level = devfreq_get_freq_level(devfreq, stats.current_frequency);
	if (level < 0) {
		pr_err(TAG "bad freq %ld\n", stats.current_frequency);
		return level;
	}

	/*
	 * If there is an extended block of busy processing,
	 * increase frequency.  Otherwise run the normal algorithm.
	 */
	if (priv->bin.busy_time > CEILING) {
		val = -1 * level;
	} else {
		val = __secure_tz_entry3(TZ_UPDATE_ID,
				level,
				priv->bin.total_time,
				priv->bin.busy_time);
	}
	priv->bin.total_time = 0;
	priv->bin.busy_time = 0;

	/*
	 * If the decision is to move to a different level, make sure the GPU
	 * frequency changes.
	 */
	if (val) {
		level += val;
		level = max(level, 0);
		level = min_t(int, level, devfreq->profile->max_state);
		goto clear;
	}

	if (priv->bus.total_time < LONG_FLOOR)
		goto end;
	norm_cycles = (unsigned int)priv->bus.ram_time /
			(unsigned int) priv->bus.total_time;
	gpu_percent = (100 * (unsigned int)priv->bus.gpu_time) /
			(unsigned int) priv->bus.total_time;

	/*
	 * If there's a new high watermark, update the cutoffs and send the
	 * FAST hint.  Otherwise check the current value against the current
	 * cutoffs.
	 */
	if (norm_cycles > priv->bus.max) {
		_update_cutoff(priv, norm_cycles);
		*flag = DEVFREQ_FLAG_FAST_HINT;
	} else {
		/* GPU votes for IB not AB so don't under vote the system */
		norm_cycles = (100 * norm_cycles) / TARGET;
		act_level = priv->bus.index[level] + b.mod;
		act_level = (act_level < 0) ? 0 : act_level;
		act_level = (act_level >= priv->bus.num) ?
			(priv->bus.num - 1) : act_level;
		if (norm_cycles > priv->bus.up[act_level] &&
				gpu_percent > CAP)
			*flag = DEVFREQ_FLAG_FAST_HINT;
		else if (norm_cycles < priv->bus.down[act_level] && level)
			*flag = DEVFREQ_FLAG_SLOW_HINT;
	}

clear:
	priv->bus.total_time = 0;
	priv->bus.gpu_time = 0;
	priv->bus.ram_time = 0;

end:
	*freq = devfreq->profile->freq_table[level];
	return 0;
}

static int tz_notify(struct notifier_block *nb, unsigned long type, void *devp)
{
	int result = 0;
	struct devfreq *devfreq = devp;

	switch (type) {
	case ADRENO_DEVFREQ_NOTIFY_IDLE:
	case ADRENO_DEVFREQ_NOTIFY_RETIRE:
		mutex_lock(&devfreq->lock);
		result = update_devfreq(devfreq);
		mutex_unlock(&devfreq->lock);
		break;
	/* ignored by this governor */
	case ADRENO_DEVFREQ_NOTIFY_SUBMIT:
	default:
		break;
	}
	return notifier_from_errno(result);
}

static int tz_start(struct devfreq *devfreq)
{
	struct devfreq_msm_adreno_tz_data *priv;
	unsigned int tz_pwrlevels[MSM_ADRENO_MAX_PWRLEVELS + 1];
	unsigned int t1, t2 = 2 * HIST;
	int i, out, ret;

	if (devfreq->data == NULL) {
		pr_err(TAG "data is required for this governor\n");
		return -EINVAL;
	}

	priv = devfreq->data;
	priv->nb.notifier_call = tz_notify;

	out = 1;
	if (devfreq->profile->max_state < MSM_ADRENO_MAX_PWRLEVELS) {
		for (i = 0; i < devfreq->profile->max_state; i++)
			tz_pwrlevels[out++] = devfreq->profile->freq_table[i];
		tz_pwrlevels[0] = i;
	} else {
		pr_err(TAG "tz_pwrlevels[] is too short\n");
		return -EINVAL;
	}

	ret = scm_call(SCM_SVC_DCVS, TZ_INIT_ID, tz_pwrlevels,
			sizeof(tz_pwrlevels), NULL, 0);

	if (ret != 0)
		pr_err(TAG "tz_init failed\n");

	/* Set up the cut-over percentages for the bus calculation. */
	if (priv->bus.num) {
		for (i = 0; i < priv->bus.num; i++) {
			t1 = (u32)(100 * priv->bus.ib[i]) /
					(u32)priv->bus.ib[priv->bus.num - 1];
			priv->bus.p_up[i] = t1 - HIST;
			priv->bus.p_down[i] = t2 - 2 * HIST;
			t2 = t1;
		}
		/* Set the upper-most and lower-most bounds correctly. */
		priv->bus.p_down[0] = 0;
		priv->bus.p_down[1] = (priv->bus.p_down[1] > (2 * HIST)) ?
					priv->bus.p_down[1] : (2 * HIST);
		if (priv->bus.num - 1 >= 0)
			priv->bus.p_up[priv->bus.num - 1] = 100;
		_update_cutoff(priv, priv->bus.max);
	}

	return kgsl_devfreq_add_notifier(devfreq->dev.parent, &priv->nb);
}

static int tz_stop(struct devfreq *devfreq)
{
	struct devfreq_msm_adreno_tz_data *priv = devfreq->data;

	kgsl_devfreq_del_notifier(devfreq->dev.parent, &priv->nb);
	return 0;
}


static int tz_resume(struct devfreq *devfreq)
{
	struct devfreq_dev_profile *profile = devfreq->profile;
	unsigned long freq;

	freq = profile->initial_freq;

	return profile->target(devfreq->dev.parent, &freq, 0);
}

static int tz_suspend(struct devfreq *devfreq)
{
	struct devfreq_msm_adreno_tz_data *priv = devfreq->data;

	__secure_tz_entry2(TZ_RESET_ID, 0, 0);

	priv->bin.total_time = 0;
	priv->bin.busy_time = 0;
	priv->bus.total_time = 0;
	priv->bus.gpu_time = 0;
	priv->bus.ram_time = 0;
	return 0;
}

static int tz_handler(struct devfreq *devfreq, unsigned int event, void *data)
{
	int result;
	BUG_ON(devfreq == NULL);

	switch (event) {
	case DEVFREQ_GOV_START:
		result = tz_start(devfreq);
		break;

	case DEVFREQ_GOV_STOP:
		result = tz_stop(devfreq);
		break;

	case DEVFREQ_GOV_SUSPEND:
		result = tz_suspend(devfreq);
		break;

	case DEVFREQ_GOV_RESUME:
		result = tz_resume(devfreq);
		break;

	case DEVFREQ_GOV_INTERVAL:
		/* ignored, this governor doesn't use polling */
	default:
		result = 0;
		break;
	}

	return result;
}

static struct devfreq_governor msm_adreno_tz = {
	.name = "msm-adreno-tz",
	.get_target_freq = tz_get_target_freq,
	.event_handler = tz_handler,
};

static int __init msm_adreno_tz_init(void)
{
	return devfreq_add_governor(&msm_adreno_tz);
}
subsys_initcall(msm_adreno_tz_init);

static void __exit msm_adreno_tz_exit(void)
{
	int ret;
	ret = devfreq_remove_governor(&msm_adreno_tz);
	if (ret)
		pr_err(TAG "failed to remove governor %d\n", ret);

	return;
}

module_exit(msm_adreno_tz_exit);

MODULE_LICENSE("GPLv2");
