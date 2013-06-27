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

#include <linux/export.h>
#include <linux/kernel.h>

#include "kgsl.h"
#include "kgsl_pwrscale.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"

static void do_devfreq_suspend(struct work_struct *work);
static void do_devfreq_resume(struct work_struct *work);
static void do_devfreq_notify(struct work_struct *work);

/*
 * kgsl_pwrscale_sleep - notify governor that device is going off
 * @device: The device
 *
 * Called shortly after all pending work is completed.
 */
void kgsl_pwrscale_sleep(struct kgsl_device *device)
{
	BUG_ON(!mutex_is_locked(&device->mutex));
	if (!device->pwrscale.enabled)
		return;
	device->pwrscale.time = device->pwrscale.on_time = 0;

	/* to call devfreq_suspend_device() from a kernel thread */
	queue_work(device->pwrscale.devfreq_wq,
		&device->pwrscale.devfreq_suspend_ws);
}
EXPORT_SYMBOL(kgsl_pwrscale_sleep);

/*
 * kgsl_pwrscale_wake - notify governor that device is going on
 * @device: The device
 *
 * Called when the device is returning to an active state.
 */
void kgsl_pwrscale_wake(struct kgsl_device *device)
{
	struct kgsl_power_stats stats;
	BUG_ON(!mutex_is_locked(&device->mutex));

	if (!device->pwrscale.enabled)
		return;
	/* clear old stats before waking */
	memset(&device->pwrscale.accum_stats, 0,
		sizeof(device->pwrscale.accum_stats));

	/* and any hw activity from waking up*/
	device->ftbl->power_stats(device, &stats);

	device->pwrscale.time = ktime_to_us(ktime_get());

	device->pwrscale.next_governor_call = 0;

	/* to call devfreq_resume_device() from a kernel thread */
	queue_work(device->pwrscale.devfreq_wq,
		&device->pwrscale.devfreq_resume_ws);
}
EXPORT_SYMBOL(kgsl_pwrscale_wake);

/*
 * kgsl_pwrscale_busy - update pwrscale state for new work
 * @device: The device
 *
 * Called when new work is submitted to the device.
 * This function must be called with the device mutex locked.
 */
void kgsl_pwrscale_busy(struct kgsl_device *device)
{
	BUG_ON(!mutex_is_locked(&device->mutex));
	if (!device->pwrscale.enabled)
		return;
	if (device->pwrscale.on_time == 0)
		device->pwrscale.on_time = ktime_to_us(ktime_get());
}
EXPORT_SYMBOL(kgsl_pwrscale_busy);

/*
 * kgsl_pwrscale_update - update device busy statistics
 * @device: The device
 *
 * Read hardware busy counters when the device is likely to be
 * on and accumulate the results between devfreq get_dev_status
 * calls. This is limits the need to turn on clocks to read these
 * values for governers that run independtly of hardware
 * activity (for example, by time based polling).
 */
void kgsl_pwrscale_update(struct kgsl_device *device)
{
	BUG_ON(!mutex_is_locked(&device->mutex));

	if (!device->pwrscale.enabled)
		return;

	if (device->pwrscale.next_governor_call == 0)
		device->pwrscale.next_governor_call = jiffies;

	if (time_before(jiffies, device->pwrscale.next_governor_call))
		return;

	device->pwrscale.next_governor_call = jiffies
			+ msecs_to_jiffies(KGSL_GOVERNOR_CALL_INTERVAL);

	/* to call srcu_notifier_call_chain() from a kernel thread */
	queue_work(device->pwrscale.devfreq_wq,
		&device->pwrscale.devfreq_notify_ws);
}
EXPORT_SYMBOL(kgsl_pwrscale_update);

/*
 * kgsl_pwrscale_disable - temporarily disable the governor
 * @device: The device
 *
 * Temporarily disable the governor, to prevent interference
 * with profiling tools that expect a fixed clock frequency.
 * This function must be called with the device mutex locked.
 */
void kgsl_pwrscale_disable(struct kgsl_device *device)
{
	BUG_ON(!mutex_is_locked(&device->mutex));

	if (device->pwrscale.enabled) {
		queue_work(device->pwrscale.devfreq_wq,
			&device->pwrscale.devfreq_suspend_ws);
		device->pwrscale.enabled = false;
		kgsl_pwrctrl_pwrlevel_change(device, KGSL_PWRLEVEL_TURBO);
	}
}
EXPORT_SYMBOL(kgsl_pwrscale_disable);

/*
 * kgsl_pwrscale_enable - re-enable the governor
 * @device: The device
 *
 * Reenable the governor after a kgsl_pwrscale_disable() call.
 * This function must be called with the device mutex locked.
 */
void kgsl_pwrscale_enable(struct kgsl_device *device)
{
	BUG_ON(!mutex_is_locked(&device->mutex));

	if (!device->pwrscale.enabled) {
		device->pwrscale.enabled = true;
		queue_work(device->pwrscale.devfreq_wq,
			&device->pwrscale.devfreq_resume_ws);
	}
}
EXPORT_SYMBOL(kgsl_pwrscale_enable);

/*
 * kgsl_devfreq_target - devfreq_dev_profile.target callback
 * @dev: see devfreq.h
 * @freq: see devfreq.h
 * @flags: see devfreq.h
 *
 * This function expects the device mutex to be unlocked.
 */
int kgsl_devfreq_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct devfreq_dev_profile *profile;
	struct kgsl_pwrctrl *pwr;
	int level = -1, i;
	unsigned long cur_freq;
	int bus_mod = 0;

	if (device == NULL)
		return -ENODEV;
	if (freq == NULL)
		return -EINVAL;
	profile = &device->pwrscale.profile;
	pwr = &device->pwrctrl;

	if (flags & DEVFREQ_FLAG_FAST_HINT)
		bus_mod = 1;

	mutex_lock(&device->mutex);
	cur_freq = kgsl_pwrctrl_active_freq(pwr);

	if (*freq > cur_freq && pwr->active_pwrlevel > 0) {
		/*
		 * If FAST is requested, move up just one level,
		 * otherwise - move up until required freq or higher
		 */
		level = pwr->active_pwrlevel - 1;
		if (!bus_mod)
			while (*freq > pwr->pwrlevels[level].gpu_freq
					&& level > 0)
				level--;
	} else if (*freq < cur_freq
			&& pwr->active_pwrlevel < (pwr->num_pwrlevels - 2)) {
		/*
		 * Move down at least 1 frequency. If we fall out the bottom
		 * of the loop, use the lowest frequency.
		 */
		level = (pwr->num_pwrlevels - 1);
		for (i = pwr->active_pwrlevel; i < level; i += pwr->step_mul)
			if (pwr->pwrlevels[i].gpu_freq <= *freq) {
				level = i;
				break;
			}
	} else {
		/* already at current freq, min, or max */
		level = pwr->active_pwrlevel;
	}

	if (device->pwrscale.enabled)
		kgsl_pwrctrl_pwrlevel_change(device, level);

	*freq = kgsl_pwrctrl_active_freq(pwr);

	mutex_unlock(&device->mutex);
	return 0;
}
EXPORT_SYMBOL(kgsl_devfreq_target);

/*
 * kgsl_devfreq_get_dev_status - devfreq_dev_profile.get_dev_status callback
 * @dev: see devfreq.h
 * @freq: see devfreq.h
 * @flags: see devfreq.h
 *
 * This function expects the device mutex to be unlocked.
 */
int kgsl_devfreq_get_dev_status(struct device *dev,
				struct devfreq_dev_status *stat)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrscale *pwrscale;
	s64 tmp;

	if (device == NULL)
		return -ENODEV;
	if (stat == NULL)
		return -EINVAL;

	pwrscale = &device->pwrscale;
	memset(stat, 0, sizeof(*stat));

	mutex_lock(&device->mutex);
	/* make sure we don't turn on clocks just to read stats */
	if (device->state == KGSL_STATE_ACTIVE) {
		struct kgsl_power_stats extra;
		device->ftbl->power_stats(device, &extra);
		device->pwrscale.accum_stats.busy_time += extra.busy_time;
	}

	tmp = ktime_to_us(ktime_get());
	stat->total_time = tmp - pwrscale->time;
	pwrscale->time = tmp;

	stat->busy_time = pwrscale->accum_stats.busy_time;

	stat->current_frequency = kgsl_pwrctrl_active_freq(&device->pwrctrl);

	trace_kgsl_pwrstats(device, stat->total_time, &pwrscale->accum_stats);
	memset(&pwrscale->accum_stats, 0, sizeof(pwrscale->accum_stats));

	mutex_unlock(&device->mutex);

	return 0;
}
EXPORT_SYMBOL(kgsl_devfreq_get_dev_status);

/*
 * kgsl_devfreq_get_cur_freq - devfreq_dev_profile.get_cur_freq callback
 * @dev: see devfreq.h
 * @freq: see devfreq.h
 * @flags: see devfreq.h
 *
 * This function expects the device mutex to be unlocked.
 */
int kgsl_devfreq_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	if (device == NULL)
		return -ENODEV;
	if (freq == NULL)
		return -EINVAL;

	mutex_lock(&device->mutex);
	*freq = kgsl_pwrctrl_active_freq(&device->pwrctrl);
	mutex_unlock(&device->mutex);

	return 0;
}
EXPORT_SYMBOL(kgsl_devfreq_get_cur_freq);

/*
 * kgsl_devfreq_add_notifier - add a fine grained notifier.
 * @dev: The device
 * @nb: Notifier block that will recieve updates.
 *
 * Add a notifier to recieve ADRENO_DEVFREQ_NOTIFY_* events
 * from the device.
 */
int kgsl_devfreq_add_notifier(struct device *dev, struct notifier_block *nb)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	if (device == NULL)
		return -ENODEV;

	if (nb == NULL)
		return -EINVAL;

	return srcu_notifier_chain_register(&device->pwrscale.nh, nb);
}

void kgsl_pwrscale_idle(struct kgsl_device *device)
{
	BUG_ON(!mutex_is_locked(&device->mutex));
	queue_work(device->pwrscale.devfreq_wq,
		&device->pwrscale.devfreq_notify_ws);
}
EXPORT_SYMBOL(kgsl_pwrscale_idle);

/*
 * kgsl_devfreq_del_notifier - remove a fine grained notifier.
 * @dev: The device
 * @nb: The notifier block.
 *
 * Remove a notifier registered with kgsl_devfreq_add_notifier().
 */
int kgsl_devfreq_del_notifier(struct device *dev, struct notifier_block *nb)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	if (device == NULL)
		return -ENODEV;

	if (nb == NULL)
		return -EINVAL;

	return srcu_notifier_chain_unregister(&device->pwrscale.nh, nb);
}
EXPORT_SYMBOL(kgsl_devfreq_del_notifier);

/*
 * kgsl_pwrscale_init - Initialize pwrscale.
 * @dev: The device
 * @governor: The initial governor to use.
 *
 * Initialize devfreq and any non-constant profile data.
 */
int kgsl_pwrscale_init(struct device *dev, const char *governor)
{
	struct kgsl_device *device;
	struct kgsl_pwrscale *pwrscale;
	struct kgsl_pwrctrl *pwr;
	struct devfreq *devfreq;
	int i, out = 0;
	int ret;

	device = dev_get_drvdata(dev);
	if (device == NULL)
		return -ENODEV;

	pwrscale = &device->pwrscale;
	pwr = &device->pwrctrl;

	srcu_init_notifier_head(&pwrscale->nh);

	pwrscale->profile.initial_freq =
		pwr->pwrlevels[pwr->default_pwrlevel].gpu_freq;
	/* Let's start with 10 ms and tune in later */
	pwrscale->profile.polling_ms = 10;

	/* do not include the 'off' level or duplicate freq. levels */
	for (i = 0; i < (pwr->num_pwrlevels - 1); i += pwr->step_mul)
		pwrscale->freq_table[out++] = pwr->pwrlevels[i].gpu_freq;

	pwrscale->profile.max_state = out;
	/* link storage array to the devfreq profile pointer */
	pwrscale->profile.freq_table = pwrscale->freq_table;

	/* if there is only 1 freq, no point in running a governor */
	if (pwrscale->profile.max_state == 1)
		governor = "performance";

	devfreq = devfreq_add_device(dev, &pwrscale->profile, governor, NULL);
	if (IS_ERR(devfreq))
		return PTR_ERR(devfreq);

	pwrscale->devfreq = devfreq;

	ret = sysfs_create_link(&device->dev->kobj,
			&devfreq->dev.kobj, "devfreq");

	pwrscale->devfreq_wq = create_freezable_workqueue("kgsl_devfreq_wq");
	INIT_WORK(&pwrscale->devfreq_suspend_ws, do_devfreq_suspend);
	INIT_WORK(&pwrscale->devfreq_resume_ws, do_devfreq_resume);
	INIT_WORK(&pwrscale->devfreq_notify_ws, do_devfreq_notify);

	pwrscale->next_governor_call = 0;

	return 0;
}
EXPORT_SYMBOL(kgsl_pwrscale_init);

/*
 * kgsl_pwrscale_close - clean up pwrscale
 * @device: the device
 *
 * This function should be called with the device mutex locked.
 */
void kgsl_pwrscale_close(struct kgsl_device *device)
{
	struct kgsl_pwrscale *pwrscale;

	BUG_ON(!mutex_is_locked(&device->mutex));

	pwrscale = &device->pwrscale;
	flush_workqueue(pwrscale->devfreq_wq);
	destroy_workqueue(pwrscale->devfreq_wq);
	devfreq_remove_device(device->pwrscale.devfreq);
	device->pwrscale.devfreq = NULL;
	srcu_cleanup_notifier_head(&device->pwrscale.nh);
}
EXPORT_SYMBOL(kgsl_pwrscale_close);

static void do_devfreq_suspend(struct work_struct *work)
{
	struct kgsl_pwrscale *pwrscale = container_of(work,
			struct kgsl_pwrscale, devfreq_suspend_ws);
	struct devfreq *devfreq = pwrscale->devfreq;

	devfreq_suspend_device(devfreq);
}

static void do_devfreq_resume(struct work_struct *work)
{
	struct kgsl_pwrscale *pwrscale = container_of(work,
			struct kgsl_pwrscale, devfreq_resume_ws);
	struct devfreq *devfreq = pwrscale->devfreq;

	devfreq_resume_device(devfreq);
}

static void do_devfreq_notify(struct work_struct *work)
{
	struct kgsl_pwrscale *pwrscale = container_of(work,
			struct kgsl_pwrscale, devfreq_notify_ws);
	struct devfreq *devfreq = pwrscale->devfreq;
	srcu_notifier_call_chain(&pwrscale->nh,
				 ADRENO_DEVFREQ_NOTIFY_RETIRE,
				 devfreq);
}
