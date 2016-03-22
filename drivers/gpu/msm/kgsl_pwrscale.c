/* Copyright (c) 2010-2016, The Linux Foundation. All rights reserved.
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

/*
 * "SLEEP" is generic counting both NAP & SLUMBER
 * PERIODS generally won't exceed 9 for the relavent 150msec
 * window, but can be significantly smaller and still POPP
 * pushable in cases where SLUMBER is involved.  Hence the
 * additional reliance on PERCENT to make sure a reasonable
 * amount of down-time actually exists.
 */
#define MIN_SLEEP_PERIODS	3
#define MIN_SLEEP_PERCENT	5

static struct kgsl_popp popp_param[POPP_MAX] = {
	{0, 0},
	{-5, 20},
	{-5, 0},
	{0, 0},
};

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
	struct kgsl_pwrscale *psc = &device->pwrscale;
	BUG_ON(!mutex_is_locked(&device->mutex));
	if (!device->pwrscale.enabled)
		return;
	device->pwrscale.on_time = 0;

	psc->popp_level = 0;
	clear_bit(POPP_PUSH, &device->pwrscale.popp_state);

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
	struct kgsl_pwrscale *psc = &device->pwrscale;
	BUG_ON(!mutex_is_locked(&device->mutex));

	if (!psc->enabled)
		return;

	if (device->state == KGSL_STATE_ACTIVE) {
		struct kgsl_power_stats stats;
		device->ftbl->power_stats(device, &stats);
		if (psc->popp_level) {
			u64 x = stats.busy_time;
			u64 y = stats.ram_time;
			do_div(x, 100);
			do_div(y, 100);
			x *= popp_param[psc->popp_level].gpu_x;
			y *= popp_param[psc->popp_level].ddr_y;
			trace_kgsl_popp_mod(device, x, y);
			stats.busy_time += x;
			stats.ram_time += y;
		}
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
 * Use various metrics including level stability, NAP intervals, and
 * overall GPU freq / DDR freq combination to decide if POPP should
 * be activated.
 */
static bool popp_stable(struct kgsl_device *device)
{
	s64 t;
	s64 nap_time = 0;
	s64 go_time = 0;
	int i, index;
	int nap = 0;
	s64 percent_nap = 0;
	struct kgsl_pwr_event *e;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct kgsl_pwrscale *psc = &device->pwrscale;

	if (!test_bit(POPP_ON, &psc->popp_state))
		return false;

	/* If already pushed or running naturally at min don't push further */
	if (test_bit(POPP_PUSH, &psc->popp_state))
		return false;
	if (!psc->popp_level &&
			(pwr->active_pwrlevel == pwr->min_pwrlevel))
		return false;
	if (psc->history[KGSL_PWREVENT_STATE].events == NULL)
		return false;

	t = ktime_to_ms(ktime_get());
	/* Check for recent NAP statistics: NAPping regularly and well? */
	if (pwr->active_pwrlevel == 0) {
		index = psc->history[KGSL_PWREVENT_STATE].index;
		i = index > 0 ? (index - 1) :
			(psc->history[KGSL_PWREVENT_STATE].size - 1);
		while (i != index) {
			e = &psc->history[KGSL_PWREVENT_STATE].events[i];
			if (e->data == KGSL_STATE_NAP ||
				e->data == KGSL_STATE_SLUMBER) {
				if (ktime_to_ms(e->start) + STABLE_TIME > t) {
					nap++;
					nap_time += e->duration;
				}
			} else if (e->data == KGSL_STATE_ACTIVE) {
				if (ktime_to_ms(e->start) + STABLE_TIME > t)
					go_time += e->duration;
			}
			if (i == 0)
				i = psc->history[KGSL_PWREVENT_STATE].size - 1;
			else
				i--;
		}
		if (nap_time && go_time) {
			percent_nap = 100 * nap_time;
			do_div(percent_nap, nap_time + go_time);
		}
		trace_kgsl_popp_nap(device, (int)nap_time / 1000, nap,
				percent_nap);
		/* If running high at turbo, don't push */
		if (nap < MIN_SLEEP_PERIODS || percent_nap < MIN_SLEEP_PERCENT)
			return false;
	}

	/* Finally check that there hasn't been a recent change */
	if ((device->pwrscale.freq_change_time + STABLE_TIME) < t) {
		device->pwrscale.freq_change_time = t;
		return true;
	}
	return false;
}

bool kgsl_popp_check(struct kgsl_device *device)
{
	int i;
	struct kgsl_pwrscale *psc = &device->pwrscale;
	struct kgsl_pwr_event *e;

	if (!test_bit(POPP_ON, &psc->popp_state))
		return false;
	if (!test_bit(POPP_PUSH, &psc->popp_state))
		return false;
	if (psc->history[KGSL_PWREVENT_STATE].events == NULL) {
		clear_bit(POPP_PUSH, &psc->popp_state);
		return false;
	}

	e = &psc->history[KGSL_PWREVENT_STATE].
			events[psc->history[KGSL_PWREVENT_STATE].index];
	if (e->data == KGSL_STATE_SLUMBER)
		e->duration = ktime_us_delta(ktime_get(), e->start);

	/* If there's been a long SLUMBER in recent history, clear the _PUSH */
	for (i = 0; i < psc->history[KGSL_PWREVENT_STATE].size; i++) {
		e = &psc->history[KGSL_PWREVENT_STATE].events[i];
		if ((e->data == KGSL_STATE_SLUMBER) &&
			 (e->duration > POPP_RESET_TIME)) {
			clear_bit(POPP_PUSH, &psc->popp_state);
			return false;
		}
	}
	return true;
}

/*
 * The GPU has been running at the current frequency for a while.  Attempt
 * to lower the frequency for boarderline cases.
 */
static void popp_trans1(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct kgsl_pwrlevel *pl = &pwr->pwrlevels[pwr->active_pwrlevel];
	struct kgsl_pwrscale *psc = &device->pwrscale;
	int old_level = psc->popp_level;

	switch (old_level) {
	case 0:
		psc->popp_level = 2;
		/* If the current level has a high default bus don't push it */
		if (pl->bus_freq == pl->bus_max)
			pwr->bus_mod = 1;
		kgsl_pwrctrl_pwrlevel_change(device, pwr->active_pwrlevel + 1);
		break;
	case 1:
	case 2:
		psc->popp_level++;
		break;
	case 3:
		set_bit(POPP_PUSH, &psc->popp_state);
		psc->popp_level = 0;
		break;
	case POPP_MAX:
	default:
		psc->popp_level = 0;
		break;
	}

	trace_kgsl_popp_level(device, old_level, psc->popp_level);
}

/*
 * The GPU DCVS algorithm recommends a level change.  Apply any
 * POPP restrictions and update the level accordingly
 */
static int popp_trans2(struct kgsl_device *device, int level)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct kgsl_pwrscale *psc = &device->pwrscale;
	int old_level = psc->popp_level;

	if (!test_bit(POPP_ON, &psc->popp_state))
		return level;

	clear_bit(POPP_PUSH, &psc->popp_state);
	/* If the governor recommends going down, do it! */
	if (pwr->active_pwrlevel < level) {
		psc->popp_level = 0;
		trace_kgsl_popp_level(device, old_level, psc->popp_level);
		return level;
	}

	switch (psc->popp_level) {
	case 0:
		/* If the feature isn't engaged, go up immediately */
		break;
	case 1:
		/* Turn off mitigation, and go up a level */
		psc->popp_level = 0;
		break;
	case 2:
	case 3:
		/* Try a more aggressive mitigation */
		psc->popp_level--;
		level++;
		/* Update the stable timestamp */
		device->pwrscale.freq_change_time = ktime_to_ms(ktime_get());
		break;
	case POPP_MAX:
	default:
		psc->popp_level = 0;
		break;
	}

	trace_kgsl_popp_level(device, old_level, psc->popp_level);

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
					level = popp_trans2(device, i);
				break;
			}
		if (level != pwr->active_pwrlevel)
			kgsl_pwrctrl_pwrlevel_change(device, level);
	} else if (popp_stable(device)) {
		popp_trans1(device);
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

	stat->private_data = &device->active_context_count;

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
	trace_kgsl_pwrstats(device, stat->total_time,
		&pwrscale->accum_stats, device->active_context_count);
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
	unsigned long ab_mbytes;

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
	ab_mbytes = device->pwrscale.bus_profile.ab_mbytes;

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

	/* Update bus vote if AB or IB is modified */
	if ((pwr->bus_mod != b) || (pwr->bus_ab_mbytes != ab_mbytes)) {
		pwr->bus_percent_ab = device->pwrscale.bus_profile.percent_ab;
		pwr->bus_ab_mbytes = ab_mbytes;
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

	data->disable_busy_time_burst = of_property_read_bool(
		device->pdev->dev.of_node, "qcom,disable-busy-time-burst");

	data->ctxt_aware_enable =
		of_property_read_bool(device->pdev->dev.of_node,
			"qcom,enable-ca-jump");

	if (data->ctxt_aware_enable) {
		if (of_property_read_u32(device->pdev->dev.of_node,
				"qcom,ca-target-pwrlevel",
				&data->bin.ctxt_aware_target_pwrlevel))
			data->bin.ctxt_aware_target_pwrlevel = 1;

		if ((data->bin.ctxt_aware_target_pwrlevel < 0) ||
			(data->bin.ctxt_aware_target_pwrlevel >
						pwr->num_pwrlevels))
			data->bin.ctxt_aware_target_pwrlevel = 1;

		if (of_property_read_u32(device->pdev->dev.of_node,
				"qcom,ca-busy-penalty",
				&data->bin.ctxt_aware_busy_penalty))
			data->bin.ctxt_aware_busy_penalty = 12000;
	}

	/*
	 * If there is a separate GX power rail, allow
	 * independent modification to its voltage through
	 * the bus bandwidth vote.
	 */
	if (pwr->bus_control) {
		out = 0;
		while (pwr->bus_ib[out] && out <= pwr->pwrlevels[0].bus_max) {
			pwr->bus_ib[out] =
				pwr->bus_ib[out] >> 20;
			out++;
		}
		data->bus.num = out;
		data->bus.ib = &pwr->bus_ib[0];
		data->bus.index = &pwr->bus_index[0];
		data->bus.width = pwr->bus_width;
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

	/* history tracking */
	for (i = 0; i < KGSL_PWREVENT_MAX; i++) {
		pwrscale->history[i].events = kzalloc(
				pwrscale->history[i].size *
				sizeof(struct kgsl_pwr_event), GFP_KERNEL);
		pwrscale->history[i].type = i;
	}

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
	int i;
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
	for (i = 0; i < KGSL_PWREVENT_MAX; i++)
		kfree(pwrscale->history[i].events);
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
