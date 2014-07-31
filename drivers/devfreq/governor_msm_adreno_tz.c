/* Copyright (c) 2010-2014, The Linux Foundation. All rights reserved.
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
#define FLOOR		        5000
/*
 * MIN_BUSY is 1 msec for the sample to be sent
 */
#define MIN_BUSY		1000
#define MAX_TZ_VERSION		0

/*
 * CEILING is 50msec, larger than any standard
 * frame length, but less than the idle timer.
 */
#define CEILING			50000
#define TZ_RESET_ID		0x3
#define TZ_UPDATE_ID		0x4
#define TZ_INIT_ID		0x6

#define TZ_RESET_ID_64          0x7
#define TZ_UPDATE_ID_64         0x8
#define TZ_INIT_ID_64           0x9

#define TAG "msm_adreno_tz: "

struct msm_adreno_extended_profile *partner_gpu_profile;
static void do_partner_start_event(struct work_struct *work);
static void do_partner_stop_event(struct work_struct *work);
static void do_partner_suspend_event(struct work_struct *work);
static void do_partner_resume_event(struct work_struct *work);

/* Trap into the TrustZone, and call funcs there. */
static int __secure_tz_reset_entry2(unsigned int *scm_data, u32 size_scm_data,
					bool is_64)
{
	int ret;
	/* sync memory before sending the commands to tz*/
	__iowmb();
	if (!is_64) {
		spin_lock(&tz_lock);
		ret = scm_call_atomic2(SCM_SVC_IO, TZ_RESET_ID, scm_data[0],
					scm_data[1]);
		spin_unlock(&tz_lock);
	} else {
		ret = scm_call(SCM_SVC_DCVS, TZ_RESET_ID_64, scm_data,
				size_scm_data, NULL, 0);
	}
	return ret;
}

static int __secure_tz_update_entry3(unsigned int *scm_data, u32 size_scm_data,
					int *val, u32 size_val, bool is_64)
{
	int ret;
	/* sync memory before sending the commands to tz*/
	__iowmb();
	if (!is_64) {
		spin_lock(&tz_lock);
		ret = scm_call_atomic3(SCM_SVC_IO, TZ_UPDATE_ID,
					scm_data[0], scm_data[1], scm_data[2]);
		spin_unlock(&tz_lock);
		*val = ret;
	} else {
		ret = scm_call(SCM_SVC_DCVS, TZ_UPDATE_ID_64, scm_data,
				size_scm_data, val, size_val);
	}
	return ret;
}

static int tz_init(struct devfreq_msm_adreno_tz_data *priv,
			unsigned int *tz_pwrlevels, u32 size_pwrlevels,
			unsigned int *version, u32 size_version)
{
	int ret;
	/* Make sure all CMD IDs are avaialble */
	if (scm_is_call_available(SCM_SVC_DCVS, TZ_INIT_ID)) {
		ret = scm_call(SCM_SVC_DCVS, TZ_INIT_ID, tz_pwrlevels,
				size_pwrlevels, NULL, 0);
		*version = 0;

	} else if (scm_is_call_available(SCM_SVC_DCVS, TZ_INIT_ID_64) &&
			scm_is_call_available(SCM_SVC_DCVS, TZ_UPDATE_ID_64) &&
			scm_is_call_available(SCM_SVC_DCVS, TZ_RESET_ID_64)) {

		ret = scm_call(SCM_SVC_DCVS, TZ_INIT_ID_64, tz_pwrlevels,
			size_pwrlevels, version, size_version);
		if (!ret)
			priv->is_64 = true;
	} else
		ret = -EINVAL;

	return ret;
}

static int tz_get_target_freq(struct devfreq *devfreq, unsigned long *freq,
				u32 *flag)
{
	int result = 0;
	struct devfreq_msm_adreno_tz_data *priv = devfreq->data;
	struct devfreq_dev_status stats;
	int val, level = 0;
	unsigned int scm_data[3];

	/* keeps stats.private_data == NULL   */
	result = devfreq->profile->get_dev_status(devfreq->dev.parent, &stats);
	if (result) {
		pr_err(TAG "get_status failed %d\n", result);
		return result;
	}

	*freq = stats.current_frequency;
	priv->bin.total_time += stats.total_time;
	priv->bin.busy_time += stats.busy_time;

	/*
	 * Do not waste CPU cycles running this algorithm if
	 * the GPU just started, or if less than FLOOR time
	 * has passed since the last run or the gpu hasn't been
	 * busier than MIN_BUSY.
	 */
	if ((stats.total_time == 0) ||
		(priv->bin.total_time < FLOOR) ||
		(unsigned int) priv->bin.busy_time < MIN_BUSY) {
		return 0;
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

		scm_data[0] = level;
		scm_data[1] = priv->bin.total_time;
		scm_data[2] = priv->bin.busy_time;
		__secure_tz_update_entry3(scm_data, sizeof(scm_data),
					&val, sizeof(val), priv->is_64);
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
		level = min_t(int, level, devfreq->profile->max_state - 1);
	}

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
		/* Nofifying partner bus governor if any */
		if (partner_gpu_profile && partner_gpu_profile->bus_devfreq) {
			mutex_lock(&partner_gpu_profile->bus_devfreq->lock);
			update_devfreq(partner_gpu_profile->bus_devfreq);
			mutex_unlock(&partner_gpu_profile->bus_devfreq->lock);
		}
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
	int i, out, ret;
	unsigned int version;

	struct msm_adreno_extended_profile *gpu_profile = container_of(
					(devfreq->profile),
					struct msm_adreno_extended_profile,
					profile);

	/*
	 * Assuming that we have only one instance of the adreno device
	 * connected to this governor,
	 * can safely restore the pointer to the governor private data
	 * from the container of the device profile
	 */
	devfreq->data = gpu_profile->private_data;
	partner_gpu_profile = gpu_profile;

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

	gpu_profile->partner_wq = create_freezable_workqueue
					("governor_msm_adreno_tz_wq");
	INIT_WORK(&gpu_profile->partner_start_event_ws,
					do_partner_start_event);
	INIT_WORK(&gpu_profile->partner_stop_event_ws,
					do_partner_stop_event);
	INIT_WORK(&gpu_profile->partner_suspend_event_ws,
					do_partner_suspend_event);
	INIT_WORK(&gpu_profile->partner_resume_event_ws,
					do_partner_resume_event);

	ret = tz_init(priv, tz_pwrlevels, sizeof(tz_pwrlevels), &version,
				sizeof(version));
	if (ret != 0 || version > MAX_TZ_VERSION) {
		pr_err(TAG "tz_init failed\n");
		return ret;
	}

	return kgsl_devfreq_add_notifier(devfreq->dev.parent, &priv->nb);
}

static int tz_stop(struct devfreq *devfreq)
{
	struct devfreq_msm_adreno_tz_data *priv = devfreq->data;
	struct msm_adreno_extended_profile *gpu_profile = container_of(
					(devfreq->profile),
					struct msm_adreno_extended_profile,
					profile);

	kgsl_devfreq_del_notifier(devfreq->dev.parent, &priv->nb);

	flush_workqueue(gpu_profile->partner_wq);
	destroy_workqueue(gpu_profile->partner_wq);

	/* leaving the governor and cleaning the pointer to private data */
	devfreq->data = NULL;
	partner_gpu_profile = NULL;
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
	unsigned int scm_data[2] = {0, 0};
	__secure_tz_reset_entry2(scm_data, sizeof(scm_data), priv->is_64);

	priv->bin.total_time = 0;
	priv->bin.busy_time = 0;
	return 0;
}

static int tz_handler(struct devfreq *devfreq, unsigned int event, void *data)
{
	int result;
	struct msm_adreno_extended_profile *gpu_profile = container_of(
					(devfreq->profile),
					struct msm_adreno_extended_profile,
					profile);
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

	if (partner_gpu_profile && partner_gpu_profile->bus_devfreq)
		switch (event) {
		case DEVFREQ_GOV_START:
			queue_work(gpu_profile->partner_wq,
					&gpu_profile->partner_start_event_ws);
			break;
		case DEVFREQ_GOV_STOP:
			queue_work(gpu_profile->partner_wq,
					&gpu_profile->partner_stop_event_ws);
			break;
		case DEVFREQ_GOV_SUSPEND:
			queue_work(gpu_profile->partner_wq,
					&gpu_profile->partner_suspend_event_ws);
			break;
		case DEVFREQ_GOV_RESUME:
			queue_work(gpu_profile->partner_wq,
					&gpu_profile->partner_resume_event_ws);
			break;
		}

	return result;
}

static void _do_partner_event(struct work_struct *work, unsigned int event)
{
	partner_gpu_profile->bus_devfreq->governor->event_handler
			(partner_gpu_profile->bus_devfreq, event, NULL);
}

static void do_partner_start_event(struct work_struct *work)
{
	_do_partner_event(work, DEVFREQ_GOV_START);
}

static void do_partner_stop_event(struct work_struct *work)
{
	_do_partner_event(work, DEVFREQ_GOV_STOP);
}

static void do_partner_suspend_event(struct work_struct *work)
{
	_do_partner_event(work, DEVFREQ_GOV_SUSPEND);
}

static void do_partner_resume_event(struct work_struct *work)
{
	_do_partner_event(work, DEVFREQ_GOV_RESUME);
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
