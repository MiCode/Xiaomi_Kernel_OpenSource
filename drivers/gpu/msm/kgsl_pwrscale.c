// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2010-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/devfreq_cooling.h>
#include <linux/slab.h>

#include "kgsl_bus.h"
#include "kgsl_device.h"
#include "kgsl_pwrscale.h"
#include "kgsl_trace.h"

static struct devfreq_msm_adreno_tz_data adreno_tz_data = {
	.bus = {
		.max = 350,
		.floating = true,
	},
	.mod_percent = 100,
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
 * kgsl_pwrscale_fast_bus_hint - enable fast_bus_hint feature in
 * adreno_tz governer
 * @on: boolean flag to ON/OFF fast_bus_hint
 *
 * Called when fast_bus_hint feature should be enabled.
 */
void kgsl_pwrscale_fast_bus_hint(bool on)
{
	adreno_tz_data.fast_bus_hint = on;
}

/*
 * kgsl_pwrscale_sleep - notify governor that device is going off
 * @device: The device
 *
 * Called shortly after all pending work is completed.
 */
void kgsl_pwrscale_sleep(struct kgsl_device *device)
{
	if (!device->pwrscale.enabled)
		return;
	device->pwrscale.on_time = 0;

	/* to call devfreq_suspend_device() from a kernel thread */
	queue_work(device->pwrscale.devfreq_wq,
		&device->pwrscale.devfreq_suspend_ws);
}

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

/*
 * kgsl_pwrscale_busy - update pwrscale state for new work
 * @device: The device
 *
 * Called when new work is submitted to the device.
 * This function must be called with the device mutex locked.
 */
void kgsl_pwrscale_busy(struct kgsl_device *device)
{
	if (!device->pwrscale.enabled)
		return;
	if (device->pwrscale.on_time == 0)
		device->pwrscale.on_time = ktime_to_us(ktime_get());
}

/**
 * kgsl_pwrscale_update_stats() - update device busy statistics
 * @device: The device
 *
 * Read hardware busy counters and accumulate the results.
 */
void kgsl_pwrscale_update_stats(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwrctrl = &device->pwrctrl;
	struct kgsl_pwrscale *psc = &device->pwrscale;

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return;

	if (!psc->enabled)
		return;

	if (device->state == KGSL_STATE_ACTIVE) {
		struct kgsl_power_stats stats;
		ktime_t cur_time = ktime_get();

		device->ftbl->power_stats(device, &stats);
		device->pwrscale.accum_stats.busy_time += stats.busy_time;
		device->pwrscale.accum_stats.ram_time += stats.ram_time;
		device->pwrscale.accum_stats.ram_wait += stats.ram_wait;
		pwrctrl->clock_times[pwrctrl->active_pwrlevel] +=
				stats.busy_time;
		pwrctrl->time_in_pwrlevel[pwrctrl->active_pwrlevel] +=
			ktime_us_delta(cur_time, pwrctrl->last_stat_updated);
		pwrctrl->last_stat_updated = cur_time;
	}
}

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

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return;

	if (!device->pwrscale.enabled)
		return;

	t = ktime_get();
	if (ktime_compare(t, device->pwrscale.next_governor_call) < 0)
		return;

	device->pwrscale.next_governor_call = ktime_add_us(t,
			KGSL_GOVERNOR_CALL_INTERVAL);

	/* to call update_devfreq() from a kernel thread */
	if (device->state != KGSL_STATE_SLUMBER)
		queue_work(device->pwrscale.devfreq_wq,
			&device->pwrscale.devfreq_notify_ws);
}

/*
 * kgsl_pwrscale_disable - temporarily disable the governor
 * @device: The device
 * @turbo: Indicates if pwrlevel should be forced to turbo
 *
 * Temporarily disable the governor, to prevent interference
 * with profiling tools that expect a fixed clock frequency.
 * This function must be called with the device mutex locked.
 */
void kgsl_pwrscale_disable(struct kgsl_device *device, bool turbo)
{
	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return;

	if (device->pwrscale.devfreqptr)
		queue_work(device->pwrscale.devfreq_wq,
			&device->pwrscale.devfreq_suspend_ws);
	device->pwrscale.enabled = false;
	if (turbo)
		kgsl_pwrctrl_pwrlevel_change(device, 0);
}

/*
 * kgsl_pwrscale_enable - re-enable the governor
 * @device: The device
 *
 * Reenable the governor after a kgsl_pwrscale_disable() call.
 * This function must be called with the device mutex locked.
 */
void kgsl_pwrscale_enable(struct kgsl_device *device)
{
	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return;

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

/*
 * kgsl_devfreq_target - devfreq_dev_profile.target callback
 * @dev: see devfreq.h
 * @freq: see devfreq.h
 * @flags: see devfreq.h
 *
 * This is a devfreq callback function for dcvs recommendations and
 * thermal constraints. If any thermal constraints are present,
 * devfreq adjusts the gpu frequency range to cap the max frequency
 * thereby not recommending anything above the constraint.
 * This function expects the device mutex to be unlocked.
 */
int kgsl_devfreq_target(struct device *dev, unsigned long *freq, u32 flags)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrctrl *pwr;
	int level;
	unsigned int i;
	unsigned long cur_freq, rec_freq;
	struct kgsl_pwrscale *pwrscale = &device->pwrscale;

	if (device == NULL)
		return -ENODEV;
	if (freq == NULL)
		return -EINVAL;

	if (!pwrscale->devfreq_enabled) {
		/*
		 * When we try to use performance governor, this function
		 * will called by devfreq driver, while adding governor using
		 * devfreq_add_device.
		 * To add and start performance governor successfully during
		 * probe, return 0 when we reach here. pwrscale->enabled will
		 * be set to true after successfully starting the governor.
		 */
		if (!pwrscale->enabled)
			return 0;
		return -EPROTO;
	}
	pwr = &device->pwrctrl;

	rec_freq = *freq;

	mutex_lock(&device->mutex);
	cur_freq = kgsl_pwrctrl_active_freq(pwr);
	level = pwr->active_pwrlevel;

	/* If the governor recommends a new frequency, update it here */
	if (rec_freq != cur_freq) {
		for (i = 0; i < pwr->num_pwrlevels; i++)
			if (rec_freq == pwr->pwrlevels[i].gpu_freq) {
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
	ktime_t tmp1, tmp2;

	if (device == NULL)
		return -ENODEV;
	if (stat == NULL)
		return -EINVAL;
	if (!device->pwrscale.devfreq_enabled)
		return -EPROTO;

	pwrscale = &device->pwrscale;
	pwrctrl = &device->pwrctrl;

	mutex_lock(&device->mutex);

	tmp1 = ktime_get();
	/*
	 * If the GPU clock is on grab the latest power counter
	 * values.  Otherwise the most recent ACTIVE values will
	 * already be stored in accum_stats.
	 */
	kgsl_pwrscale_update_stats(device);

	tmp2 = ktime_get();
	stat->total_time = ktime_us_delta(tmp2, pwrscale->time);
	pwrscale->time = tmp1;

	stat->busy_time = pwrscale->accum_stats.busy_time;

	stat->current_frequency = kgsl_pwrctrl_active_freq(&device->pwrctrl);

	stat->private_data = &device->active_context_count;

	/*
	 * keep the latest devfreq_dev_status values
	 * and vbif counters data
	 * to be (re)used by kgsl_busmon_get_dev_status()
	 */
	if (pwrctrl->bus_control) {
		struct kgsl_pwrlevel *pwrlevel;
		struct xstats *last_b =
			(struct xstats *)last_status.private_data;

		last_status.total_time = stat->total_time;
		last_status.busy_time = stat->busy_time;
		last_status.current_frequency = stat->current_frequency;

		last_b->ram_time = device->pwrscale.accum_stats.ram_time;
		last_b->ram_wait = device->pwrscale.accum_stats.ram_wait;
		last_b->buslevel = device->pwrctrl.cur_dcvs_buslevel;

		pwrlevel = &pwrctrl->pwrlevels[pwrctrl->min_pwrlevel];
		last_b->gpu_minfreq = pwrlevel->gpu_freq;
	}

	kgsl_pwrctrl_busy_time(device, stat->total_time, stat->busy_time);
	trace_kgsl_pwrstats(device, stat->total_time,
		&pwrscale->accum_stats, device->active_context_count);
	memset(&pwrscale->accum_stats, 0, sizeof(pwrscale->accum_stats));

	mutex_unlock(&device->mutex);

	return 0;
}

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
	struct kgsl_pwrscale *pwrscale = &device->pwrscale;

	if (device == NULL)
		return -ENODEV;
	if (freq == NULL)
		return -EINVAL;

	if (!pwrscale->devfreq_enabled) {
		/*
		 * When we try to use performance governor, this function
		 * will called by devfreq driver, while adding governor using
		 * devfreq_add_device.
		 * To add and start performance governor successfully during
		 * probe, return 0 when we reach here. pwrscale->enabled will
		 * be set to true after successfully starting the governor.
		 */
		if (!pwrscale->enabled)
			return 0;
		return -EPROTO;
	}

	mutex_lock(&device->mutex);
	*freq = kgsl_pwrctrl_active_freq(&device->pwrctrl);
	mutex_unlock(&device->mutex);

	return 0;
}

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
	struct kgsl_device *device = dev_get_drvdata(dev);

	if (!device->pwrscale.devfreq_enabled)
		return -EPROTO;

	stat->total_time = last_status.total_time;
	stat->busy_time = last_status.busy_time;
	stat->current_frequency = last_status.current_frequency;

	if (stat->private_data) {
		struct xstats *last_b =
			(struct xstats *)last_status.private_data;
		b = (struct xstats *)stat->private_data;
		b->ram_time = last_b->ram_time;
		b->ram_wait = last_b->ram_wait;
		b->buslevel = last_b->buslevel;
		b->gpu_minfreq = last_b->gpu_minfreq;
	}
	return 0;
}

static int _read_hint(u32 flags)
{
	switch (flags) {
	case BUSMON_FLAG_FAST_HINT:
		return 1;
	case BUSMON_FLAG_SUPER_FAST_HINT:
		return 2;
	case BUSMON_FLAG_SLOW_HINT:
		return -1;
	default:
		return 0;
	}
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
	if (!device->pwrscale.devfreq_enabled)
		return -EPROTO;

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
	pwr->bus_mod += _read_hint(bus_flag);

	/* trim calculated change to fit range */
	if (pwr_level->bus_freq + pwr->bus_mod < pwr_level->bus_min)
		pwr->bus_mod = -(pwr_level->bus_freq - pwr_level->bus_min);
	else if (pwr_level->bus_freq + pwr->bus_mod > pwr_level->bus_max)
		pwr->bus_mod = pwr_level->bus_max - pwr_level->bus_freq;

	/* Update bus vote if AB or IB is modified */
	if ((pwr->bus_mod != b) || (pwr->bus_ab_mbytes != ab_mbytes)) {
		pwr->bus_percent_ab = device->pwrscale.bus_profile.percent_ab;
		/*
		 * When gpu is thermally throttled to its lowest power level,
		 * drop GPU's AB vote as a last resort to lower CX voltage and
		 * to prevent thermal reset.
		 * Ignore this check when only single power level in use to
		 * avoid setting default AB vote in normal situations too.
		 */
		if (pwr->thermal_pwrlevel != pwr->num_pwrlevels - 1 ||
			pwr->num_pwrlevels == 1)
			pwr->bus_ab_mbytes = ab_mbytes;
		else
			pwr->bus_ab_mbytes = 0;
		kgsl_bus_update(device, KGSL_BUS_VOTE_ON);
	}

	mutex_unlock(&device->mutex);
	return 0;
}

int kgsl_busmon_get_cur_freq(struct device *dev, unsigned long *freq)
{
	return 0;
}

static void pwrscale_busmon_create(struct kgsl_device *device,
		struct platform_device *pdev, unsigned long *table)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct kgsl_pwrscale *pwrscale = &device->pwrscale;
	struct device *dev = &pwrscale->busmondev;
	struct msm_busmon_extended_profile *bus_profile;
	struct devfreq *bus_devfreq;
	int i, ret;

	bus_profile = &pwrscale->bus_profile;
	bus_profile->private_data = &adreno_tz_data;

	bus_profile->profile.target = kgsl_busmon_target;
	bus_profile->profile.get_dev_status = kgsl_busmon_get_dev_status;
	bus_profile->profile.get_cur_freq = kgsl_busmon_get_cur_freq;

	bus_profile->profile.max_state = pwr->num_pwrlevels;
	bus_profile->profile.freq_table = table;

	dev->parent = &pdev->dev;

	dev_set_name(dev, "kgsl-busmon");
	dev_set_drvdata(dev, device);
	if (device_register(dev)) {
		put_device(dev);
		return;
	}

	/* Build out the OPP table for the busmon device */
	for (i = 0; i < pwr->num_pwrlevels; i++) {
		if (!pwr->pwrlevels[i].gpu_freq)
			continue;

		dev_pm_opp_add(dev, pwr->pwrlevels[i].gpu_freq, 0);
	}

	ret = devfreq_gpubw_init();
	if (ret) {
		dev_err(&pdev->dev, "Failed to add busmon governor: %d\n", ret);
		device_unregister(dev);
		return;
	}

	bus_devfreq = devfreq_add_device(dev, &pwrscale->bus_profile.profile,
		"gpubw_mon", NULL);

	if (IS_ERR_OR_NULL(bus_devfreq)) {
		dev_err(&pdev->dev, "Bus scaling not enabled\n");
		devfreq_gpubw_exit();
		device_unregister(dev);
		return;
	}

	pwrscale->bus_devfreq = bus_devfreq;
}

static void pwrscale_of_get_ca_target_pwrlevel(struct kgsl_device *device,
		struct device_node *node)
{
	u32 pwrlevel = 1;

	of_property_read_u32(node, "qcom,ca-target-pwrlevel", &pwrlevel);

	if (pwrlevel >= device->pwrctrl.num_pwrlevels)
		pwrlevel = 1;

	device->pwrscale.ctxt_aware_target_pwrlevel = pwrlevel;
}

/* Get context aware properties */
static void pwrscale_of_ca_aware(struct kgsl_device *device)
{
	struct kgsl_pwrscale *pwrscale = &device->pwrscale;
	struct device_node *parent = device->pdev->dev.of_node;
	struct device_node *node, *child;

	pwrscale->ctxt_aware_enable =
		of_property_read_bool(parent, "qcom,enable-ca-jump");

	if (!pwrscale->ctxt_aware_enable)
		return;

	pwrscale->ctxt_aware_busy_penalty = 12000;
	of_property_read_u32(parent, "qcom,ca-busy-penalty",
		&pwrscale->ctxt_aware_busy_penalty);


	pwrscale->ctxt_aware_target_pwrlevel = 1;

	node = of_find_node_by_name(parent, "qcom,gpu-pwrlevel-bins");
	if (node == NULL) {
		pwrscale_of_get_ca_target_pwrlevel(device, parent);
		return;
	}

	for_each_child_of_node(node, child) {
		u32 bin;

		if (of_property_read_u32(child, "qcom,speed-bin", &bin))
			continue;

		if (bin == device->speed_bin) {
			pwrscale_of_get_ca_target_pwrlevel(device, child);
			of_node_put(child);
			break;
		}
	}

	of_node_put(node);
}

/*
 * thermal_max_notifier_call - Callback function registered to receive qos max
 * frequency events.
 * @nb: The notifier block
 * @val: Max frequency value in KHz for GPU
 *
 * The function subscribes to GPU max frequency change and updates thermal
 * power level accordingly.
 */
static int thermal_max_notifier_call(struct notifier_block *nb, unsigned long val, void *data)
{
	struct kgsl_pwrctrl *pwr = container_of(nb, struct kgsl_pwrctrl, nb_max);
	struct kgsl_device *device = container_of(pwr, struct kgsl_device, pwrctrl);
	u32 max_freq = val * 1000;
	int level;

	if (!device->pwrscale.devfreq_enabled)
		return NOTIFY_DONE;

	for (level = pwr->num_pwrlevels - 1; level >= 0; level--) {
		/* get nearest power level with a maximum delta of 5MHz */
		if (abs(pwr->pwrlevels[level].gpu_freq - max_freq) < 5000000)
			break;
	}

	if (level < 0)
		return NOTIFY_DONE;

	if (level == pwr->thermal_pwrlevel)
		return NOTIFY_OK;

	trace_kgsl_thermal_constraint(max_freq);
	pwr->thermal_pwrlevel = level;

	mutex_lock(&device->mutex);

	/* Update the current level using the new limit */
	kgsl_pwrctrl_pwrlevel_change(device, pwr->active_pwrlevel);

	mutex_unlock(&device->mutex);
	return NOTIFY_OK;
}

int kgsl_pwrscale_init(struct kgsl_device *device, struct platform_device *pdev,
		const char *governor)
{
	struct kgsl_pwrscale *pwrscale = &device->pwrscale;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct devfreq *devfreq;
	struct msm_adreno_extended_profile *gpu_profile;
	int i, ret;

	gpu_profile = &pwrscale->gpu_profile;
	gpu_profile->private_data = &adreno_tz_data;

	gpu_profile->profile.target = kgsl_devfreq_target;
	gpu_profile->profile.get_dev_status = kgsl_devfreq_get_dev_status;
	gpu_profile->profile.get_cur_freq = kgsl_devfreq_get_cur_freq;

	gpu_profile->profile.initial_freq =
		pwr->pwrlevels[pwr->default_pwrlevel].gpu_freq;

	gpu_profile->profile.polling_ms = 10;

	pwrscale_of_ca_aware(device);

	for (i = 0; i < pwr->num_pwrlevels; i++)
		pwrscale->freq_table[i] = pwr->pwrlevels[i].gpu_freq;

	/*
	 * Max_state is the number of valid power levels.
	 * The valid power levels range from 0 - (max_state - 1)
	 */
	gpu_profile->profile.max_state = pwr->num_pwrlevels;
	/* link storage array to the devfreq profile pointer */
	gpu_profile->profile.freq_table = pwrscale->freq_table;

	/* initialize msm-adreno-tz governor specific data here */
	adreno_tz_data.disable_busy_time_burst =
		of_property_read_bool(pdev->dev.of_node,
		"qcom,disable-busy-time-burst");

	if (pwrscale->ctxt_aware_enable) {
		adreno_tz_data.ctxt_aware_enable = pwrscale->ctxt_aware_enable;
		adreno_tz_data.bin.ctxt_aware_target_pwrlevel =
			pwrscale->ctxt_aware_target_pwrlevel;
		adreno_tz_data.bin.ctxt_aware_busy_penalty =
			pwrscale->ctxt_aware_busy_penalty;
	}

	/*
	 * If there is a separate GX power rail, allow
	 * independent modification to its voltage through
	 * the bus bandwidth vote.
	 */
	if (pwr->bus_control) {
		adreno_tz_data.bus.num = pwr->ddr_table_count;
		adreno_tz_data.bus.ib_kbps = pwr->ddr_table;
		adreno_tz_data.bus.width = pwr->bus_width;

		if (!kgsl_of_property_read_ddrtype(device->pdev->dev.of_node,
			"qcom,bus-accesses", &adreno_tz_data.bus.max))
			adreno_tz_data.bus.floating = false;
	}

	pwrscale->devfreq_wq = create_freezable_workqueue("kgsl_devfreq_wq");
	if (!pwrscale->devfreq_wq) {
		dev_err(device->dev, "Failed to allocate kgsl devfreq workqueue\n");
		device->pwrscale.enabled = false;
		return -ENOMEM;
	}

	ret = msm_adreno_tz_init();
	if (ret) {
		dev_err(device->dev, "Failed to add adreno tz governor: %d\n", ret);
		device->pwrscale.enabled = false;
		return ret;
	}

	pwr->nb_max.notifier_call = thermal_max_notifier_call;
	ret = dev_pm_qos_add_notifier(&pdev->dev, &pwr->nb_max, DEV_PM_QOS_MAX_FREQUENCY);

	if (ret) {
		dev_err(device->dev, "Unable to register notifier call for thermal: %d\n", ret);
		device->pwrscale.enabled = false;
		msm_adreno_tz_exit();
		return ret;
	}

	devfreq = devfreq_add_device(&pdev->dev, &gpu_profile->profile,
			governor, &adreno_tz_data);
	if (IS_ERR_OR_NULL(devfreq)) {
		device->pwrscale.enabled = false;
		msm_adreno_tz_exit();
		return IS_ERR(devfreq) ? PTR_ERR(devfreq) : -EINVAL;
	}

	pwrscale->enabled = true;
	pwrscale->devfreqptr = devfreq;
	pwrscale->cooling_dev = of_devfreq_cooling_register(pdev->dev.of_node,
		devfreq);
	if (IS_ERR(pwrscale->cooling_dev))
		pwrscale->cooling_dev = NULL;

	if (adreno_tz_data.bus.num)
		pwrscale_busmon_create(device, pdev, pwrscale->freq_table);

	WARN_ON(sysfs_create_link(&device->dev->kobj,
			&devfreq->dev.kobj, "devfreq"));

	INIT_WORK(&pwrscale->devfreq_suspend_ws, do_devfreq_suspend);
	INIT_WORK(&pwrscale->devfreq_resume_ws, do_devfreq_resume);
	INIT_WORK(&pwrscale->devfreq_notify_ws, do_devfreq_notify);

	pwrscale->next_governor_call = ktime_add_us(ktime_get(),
			KGSL_GOVERNOR_CALL_INTERVAL);

	return 0;
}

/*
 * kgsl_pwrscale_close - clean up pwrscale
 * @device: the device
 *
 * This function should be called with the device mutex locked.
 */
void kgsl_pwrscale_close(struct kgsl_device *device)
{
	struct kgsl_pwrscale *pwrscale;
	struct kgsl_pwrctrl *pwr;

	pwr = &device->pwrctrl;
	pwrscale = &device->pwrscale;

	if (pwrscale->bus_devfreq) {
		devfreq_remove_device(pwrscale->bus_devfreq);
		pwrscale->bus_devfreq = NULL;
		devfreq_gpubw_exit();
		device_unregister(&pwrscale->busmondev);
	}

	if (!pwrscale->devfreqptr)
		return;
	if (pwrscale->cooling_dev)
		devfreq_cooling_unregister(pwrscale->cooling_dev);

	if (pwrscale->devfreq_wq) {
		flush_workqueue(pwrscale->devfreq_wq);
		destroy_workqueue(pwrscale->devfreq_wq);
		pwrscale->devfreq_wq = NULL;
	}

	devfreq_remove_device(device->pwrscale.devfreqptr);
	device->pwrscale.devfreqptr = NULL;
	dev_pm_qos_remove_notifier(&device->pdev->dev, &pwr->nb_max, DEV_PM_QOS_MAX_FREQUENCY);
	msm_adreno_tz_exit();
}

static void do_devfreq_suspend(struct work_struct *work)
{
	struct kgsl_pwrscale *pwrscale = container_of(work,
			struct kgsl_pwrscale, devfreq_suspend_ws);

	devfreq_suspend_device(pwrscale->devfreqptr);
	devfreq_suspend_device(pwrscale->bus_devfreq);
}

static void do_devfreq_resume(struct work_struct *work)
{
	struct kgsl_pwrscale *pwrscale = container_of(work,
			struct kgsl_pwrscale, devfreq_resume_ws);

	devfreq_resume_device(pwrscale->devfreqptr);
	devfreq_resume_device(pwrscale->bus_devfreq);
}

static void do_devfreq_notify(struct work_struct *work)
{
	struct kgsl_pwrscale *pwrscale = container_of(work,
			struct kgsl_pwrscale, devfreq_notify_ws);

	mutex_lock(&pwrscale->devfreqptr->lock);
	update_devfreq(pwrscale->devfreqptr);
	mutex_unlock(&pwrscale->devfreqptr->lock);

	if (pwrscale->bus_devfreq) {
		mutex_lock(&pwrscale->bus_devfreq->lock);
		update_devfreq(pwrscale->bus_devfreq);
		mutex_unlock(&pwrscale->bus_devfreq->lock);
	}
}
