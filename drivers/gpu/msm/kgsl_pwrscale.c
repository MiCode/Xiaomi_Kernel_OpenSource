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

#include <linux/export.h>
#include <linux/kernel.h>

#include "kgsl.h"
#include "kgsl_pwrscale.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"

#define FAST_BUS 1
#define SLOW_BUS -1

static void do_devfreq_suspend(struct work_struct *work);
static void do_devfreq_resume(struct work_struct *work);
static void do_devfreq_notify(struct work_struct *work);

/*
 * These variables are used to keep the latest data
 * returned by kgsl_devfreq_get_dev_status
 */
static struct xstats last_xstats;
static struct devfreq_dev_status last_status = { .private_data = &last_xstats };

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
	device->pwrscale.on_time = 0;

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
	struct kgsl_pwrscale *psc = &device->pwrscale;
	BUG_ON(!mutex_is_locked(&device->mutex));

	if (!device->pwrscale.enabled)
		return;
	/* clear old stats before waking */
	memset(&psc->accum_stats, 0, sizeof(psc->accum_stats));
	memset(&last_xstats, 0, sizeof(last_xstats));

	/* and any hw activity from waking up*/
	device->ftbl->power_stats(device, &stats);

	psc->time = ktime_get();

	psc->next_governor_call = ktime_add_us(psc->time,
			KGSL_GOVERNOR_CALL_INTERVAL);

	/* to call devfreq_resume_device() from a kernel thread */
	queue_work(psc->devfreq_wq, &psc->devfreq_resume_ws);
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

/**
 * kgsl_pwrscale_update_stats() - update device busy statistics
 * @device: The device
 *
 * Read hardware busy counters and accumulate the results.
 */
void kgsl_pwrscale_update_stats(struct kgsl_device *device)
{
	BUG_ON(!mutex_is_locked(&device->mutex));

	if (!device->pwrscale.enabled)
		return;

	if (device->state == KGSL_STATE_ACTIVE) {
		struct kgsl_power_stats stats;
		device->ftbl->power_stats(device, &stats);
		device->pwrscale.accum_stats.busy_time += stats.busy_time;
		device->pwrscale.accum_stats.ram_time += stats.ram_time;
		device->pwrscale.accum_stats.ram_wait += stats.ram_wait;
	}
}
EXPORT_SYMBOL(kgsl_pwrscale_update_stats);

/**
 * kgsl_pwrscale_update() - update device busy statistics
 * @device: The device
 *
 * If enough time has passed schedule the next call to devfreq
 * get_dev_status.
 */
void kgsl_pwrscale_update(struct kgsl_device *device)
{
	ktime_t t;
	BUG_ON(!mutex_is_locked(&device->mutex));

	if (!device->pwrscale.enabled)
		return;

	t = ktime_get();
	if (ktime_compare(t, device->pwrscale.next_governor_call) < 0)
		return;

	device->pwrscale.next_governor_call = ktime_add_us(t,
			KGSL_GOVERNOR_CALL_INTERVAL);

	/* to call srcu_notifier_call_chain() from a kernel thread */
	if (device->state != KGSL_STATE_SLUMBER)
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
	if (device->pwrscale.devfreqptr)
		queue_work(device->pwrscale.devfreq_wq,
			&device->pwrscale.devfreq_suspend_ws);
	device->pwrscale.enabled = false;
	kgsl_pwrctrl_pwrlevel_change(device, KGSL_PWRLEVEL_TURBO);
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

	if (device->pwrscale.devfreqptr) {
		queue_work(device->pwrscale.devfreq_wq,
			&device->pwrscale.devfreq_resume_ws);
		device->pwrscale.enabled = true;
	} else {
		/*
		 * Don't enable it if devfreq is not set and let the device
		 * run at default level;
		 */
		kgsl_pwrctrl_pwrlevel_change(device,
					device->pwrctrl.default_pwrlevel);
		device->pwrscale.enabled = false;
	}
}
EXPORT_SYMBOL(kgsl_pwrscale_enable);

static int _thermal_adjust(struct kgsl_pwrctrl *pwr, int level)
{
	if (level < pwr->active_pwrlevel)
		return pwr->active_pwrlevel;

	/*
	 * A lower frequency has been recommended!  Stop thermal
	 * cycling (but keep the upper thermal limit) and switch to
	 * the lower frequency.
	 */
	pwr->thermal_cycle = CYCLE_ENABLE;
	del_timer_sync(&pwr->thermal_timer);
	return level;
}

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
	struct kgsl_pwrctrl *pwr;
	struct kgsl_pwrlevel *pwr_level;
	int level, i;
	unsigned long cur_freq;

	if (device == NULL)
		return -ENODEV;
	if (freq == NULL)
		return -EINVAL;
	if (!device->pwrscale.enabled)
		return 0;

	pwr = &device->pwrctrl;
	if (flags & DEVFREQ_FLAG_WAKEUP_MAXFREQ) {
		/*
		 * The GPU is about to get suspended,
		 * but it needs to be at the max power level when waking up
		*/
		pwr->wakeup_maxpwrlevel = 1;
		return 0;
	}

	mutex_lock(&device->mutex);
	cur_freq = kgsl_pwrctrl_active_freq(pwr);
	level = pwr->active_pwrlevel;
	pwr_level = &pwr->pwrlevels[level];

	/* If the governor recommends a new frequency, update it here */
	if (*freq != cur_freq) {
		level = pwr->max_pwrlevel;
		for (i = pwr->min_pwrlevel; i >= pwr->max_pwrlevel; i--)
			if (*freq <= pwr->pwrlevels[i].gpu_freq) {
				if (pwr->thermal_cycle == CYCLE_ACTIVE)
					level = _thermal_adjust(pwr, i);
				else
					level = i;
				break;
			}
		if (level != pwr->active_pwrlevel)
			kgsl_pwrctrl_pwrlevel_change(device, level);
	}

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
	struct kgsl_pwrctrl *pwrctrl;
	struct kgsl_pwrscale *pwrscale;
	ktime_t tmp;

	if (device == NULL)
		return -ENODEV;
	if (stat == NULL)
		return -EINVAL;

	pwrscale = &device->pwrscale;
	pwrctrl = &device->pwrctrl;

	mutex_lock(&device->mutex);
	/*
	 * If the GPU clock is on grab the latest power counter
	 * values.  Otherwise the most recent ACTIVE values will
	 * already be stored in accum_stats.
	 */
	kgsl_pwrscale_update_stats(device);

	tmp = ktime_get();
	stat->total_time = ktime_us_delta(tmp, pwrscale->time);
	pwrscale->time = tmp;

	stat->busy_time = pwrscale->accum_stats.busy_time;

	stat->current_frequency = kgsl_pwrctrl_active_freq(&device->pwrctrl);

	/*
	 * keep the latest devfreq_dev_status values
	 * and vbif counters data
	 * to be (re)used by kgsl_busmon_get_dev_status()
	 */
	if (pwrctrl->bus_control) {
		struct xstats *last_b =
			(struct xstats *)last_status.private_data;

		last_status.total_time = stat->total_time;
		last_status.busy_time = stat->busy_time;
		last_status.current_frequency = stat->current_frequency;

		last_b->ram_time = device->pwrscale.accum_stats.ram_time;
		last_b->ram_wait = device->pwrscale.accum_stats.ram_wait;
		last_b->mod = device->pwrctrl.bus_mod;
	}

	kgsl_pwrctrl_busy_time(device, stat->total_time, stat->busy_time);
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
int kgsl_devfreq_add_notifier(struct device *dev,
		struct notifier_block *nb)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	if (device == NULL)
		return -ENODEV;

	if (nb == NULL)
		return -EINVAL;

	return srcu_notifier_chain_register(&device->pwrscale.nh, nb);
}
EXPORT_SYMBOL(kgsl_devfreq_add_notifier);

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
 * kgsl_busmon_get_dev_status - devfreq_dev_profile.get_dev_status callback
 * @dev: see devfreq.h
 * @freq: see devfreq.h
 * @flags: see devfreq.h
 *
 * This function expects the device mutex to be unlocked.
 */
int kgsl_busmon_get_dev_status(struct device *dev,
			struct devfreq_dev_status *stat)
{
	struct xstats *b;
	stat->total_time = last_status.total_time;
	stat->busy_time = last_status.busy_time;
	stat->current_frequency = last_status.current_frequency;
	if (stat->private_data) {
		struct xstats *last_b =
			(struct xstats *)last_status.private_data;
		b = (struct xstats *)stat->private_data;
		b->ram_time = last_b->ram_time;
		b->ram_wait = last_b->ram_wait;
		b->mod = last_b->mod;
	}
	return 0;
}

/*
 * kgsl_busmon_target - devfreq_dev_profile.target callback
 * @dev: see devfreq.h
 * @freq: see devfreq.h
 * @flags: see devfreq.h
 *
 * This function expects the device mutex to be unlocked.
 */
int kgsl_busmon_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrctrl *pwr;
	struct kgsl_pwrlevel *pwr_level;
	int  level, b;
	u32 bus_flag;

	if (device == NULL)
		return -ENODEV;
	if (freq == NULL)
		return -EINVAL;
	if (!device->pwrscale.enabled)
		return 0;

	pwr = &device->pwrctrl;

	if (!pwr->bus_control)
		return 0;

	mutex_lock(&device->mutex);
	level = pwr->active_pwrlevel;
	pwr_level = &pwr->pwrlevels[level];
	bus_flag = device->pwrscale.bus_profile.flag;
	device->pwrscale.bus_profile.flag = 0;

	/*
	 * Bus devfreq governor has calculated its recomendations
	 * when gpu was running with *freq frequency.
	 * If the gpu frequency is different now it's better to
	 * ignore the call
	 */
	if (pwr_level->gpu_freq != *freq) {
		mutex_unlock(&device->mutex);
		return 0;
	}

	b = pwr->bus_mod;
	if ((bus_flag & DEVFREQ_FLAG_FAST_HINT) &&
		((pwr_level->bus_freq + pwr->bus_mod) < pwr_level->bus_max))
			pwr->bus_mod++;
	 else if ((bus_flag & DEVFREQ_FLAG_SLOW_HINT) &&
		((pwr_level->bus_freq + pwr->bus_mod) > pwr_level->bus_min))
			pwr->bus_mod--;

	if (pwr->bus_mod != b) {
		pwr->bus_percent_ab = device->pwrscale.bus_profile.percent_ab;
		kgsl_pwrctrl_buslevel_update(device, true);
	}

	mutex_unlock(&device->mutex);
	return 0;
}

int kgsl_busmon_get_cur_freq(struct device *dev, unsigned long *freq)
{
	return 0;
}


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
	struct devfreq *bus_devfreq;
	struct msm_adreno_extended_profile *gpu_profile;
	struct devfreq_dev_profile *profile;
	struct devfreq_msm_adreno_tz_data *data;
	int i, out = 0;
	int ret;

	device = dev_get_drvdata(dev);
	if (device == NULL)
		return -ENODEV;

	pwrscale = &device->pwrscale;
	pwr = &device->pwrctrl;
	gpu_profile = &pwrscale->gpu_profile;
	profile = &pwrscale->gpu_profile.profile;

	srcu_init_notifier_head(&pwrscale->nh);

	profile->initial_freq =
		pwr->pwrlevels[pwr->default_pwrlevel].gpu_freq;
	/* Let's start with 10 ms and tune in later */
	profile->polling_ms = 10;

	/* do not include the 'off' level or duplicate freq. levels */
	for (i = 0; i < (pwr->num_pwrlevels - 1); i++)
		pwrscale->freq_table[out++] = pwr->pwrlevels[i].gpu_freq;

	/*
	 * Max_state is the number of valid power levels.
	 * The valid power levels range from 0 - (max_state - 1)
	 */
	profile->max_state = pwr->num_pwrlevels - 1;
	/* link storage array to the devfreq profile pointer */
	profile->freq_table = pwrscale->freq_table;

	/* if there is only 1 freq, no point in running a governor */
	if (profile->max_state == 1)
		governor = "performance";

	/* initialize msm-adreno-tz governor specific data here */
	data = gpu_profile->private_data;
	/*
	 * If there is a separate GX power rail, allow
	 * independent modification to its voltage through
	 * the bus bandwidth vote.
	 */
	if (pwr->bus_control) {
		out = 0;
		while (pwr->bus_ib[out]) {
			pwr->bus_ib[out] =
				pwr->bus_ib[out] >> 20;
			out++;
		}
		data->bus.num = out;
		data->bus.ib = &pwr->bus_ib[0];
		data->bus.index = &pwr->bus_index[0];
	} else
		data->bus.num = 0;

	devfreq = devfreq_add_device(dev, &pwrscale->gpu_profile.profile,
			governor, pwrscale->gpu_profile.private_data);
	if (IS_ERR(devfreq)) {
		device->pwrscale.enabled = false;
		return PTR_ERR(devfreq);
	}

	pwrscale->devfreqptr = devfreq;

	pwrscale->gpu_profile.bus_devfreq = NULL;
	if (data->bus.num) {
		pwrscale->bus_profile.profile.max_state
					= pwr->num_pwrlevels - 1;
		pwrscale->bus_profile.profile.freq_table
					= pwrscale->freq_table;

		bus_devfreq = devfreq_add_device(device->busmondev,
			&pwrscale->bus_profile.profile, "gpubw_mon", NULL);
		if (!IS_ERR(bus_devfreq))
			pwrscale->gpu_profile.bus_devfreq = bus_devfreq;
	}

	ret = sysfs_create_link(&device->dev->kobj,
			&devfreq->dev.kobj, "devfreq");

	pwrscale->devfreq_wq = create_freezable_workqueue("kgsl_devfreq_wq");
	INIT_WORK(&pwrscale->devfreq_suspend_ws, do_devfreq_suspend);
	INIT_WORK(&pwrscale->devfreq_resume_ws, do_devfreq_resume);
	INIT_WORK(&pwrscale->devfreq_notify_ws, do_devfreq_notify);

	pwrscale->next_governor_call = ktime_add_us(ktime_get(),
			KGSL_GOVERNOR_CALL_INTERVAL);

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
	if (!pwrscale->devfreqptr)
		return;
	flush_workqueue(pwrscale->devfreq_wq);
	destroy_workqueue(pwrscale->devfreq_wq);
	devfreq_remove_device(device->pwrscale.devfreqptr);
	device->pwrscale.devfreqptr = NULL;
	srcu_cleanup_notifier_head(&device->pwrscale.nh);
}
EXPORT_SYMBOL(kgsl_pwrscale_close);

static void do_devfreq_suspend(struct work_struct *work)
{
	struct kgsl_pwrscale *pwrscale = container_of(work,
			struct kgsl_pwrscale, devfreq_suspend_ws);
	struct devfreq *devfreq = pwrscale->devfreqptr;

	devfreq_suspend_device(devfreq);
}

static void do_devfreq_resume(struct work_struct *work)
{
	struct kgsl_pwrscale *pwrscale = container_of(work,
			struct kgsl_pwrscale, devfreq_resume_ws);
	struct devfreq *devfreq = pwrscale->devfreqptr;

	devfreq_resume_device(devfreq);
}

static void do_devfreq_notify(struct work_struct *work)
{
	struct kgsl_pwrscale *pwrscale = container_of(work,
			struct kgsl_pwrscale, devfreq_notify_ws);
	struct devfreq *devfreq = pwrscale->devfreqptr;
	srcu_notifier_call_chain(&pwrscale->nh,
				 ADRENO_DEVFREQ_NOTIFY_RETIRE,
				 devfreq);
}
