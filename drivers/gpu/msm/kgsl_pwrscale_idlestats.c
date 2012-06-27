/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
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

#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/idle_stats_device.h>
#include <linux/cpufreq.h>
#include <linux/notifier.h>
#include <linux/cpumask.h>
#include <linux/tick.h>

#include "kgsl.h"
#include "kgsl_pwrscale.h"
#include "kgsl_device.h"

#define MAX_CORES 4
struct _cpu_info {
	spinlock_t lock;
	struct notifier_block cpu_nb;
	u64 start[MAX_CORES];
	u64 end[MAX_CORES];
	int curr_freq[MAX_CORES];
	int max_freq[MAX_CORES];
};

struct idlestats_priv {
	char name[32];
	struct msm_idle_stats_device idledev;
	struct kgsl_device *device;
	struct msm_idle_pulse pulse;
	struct _cpu_info cpu_info;
};

static int idlestats_cpufreq_notifier(
				struct notifier_block *nb,
				unsigned long val, void *data)
{
	struct _cpu_info *cpu = container_of(nb,
						struct _cpu_info, cpu_nb);
	struct cpufreq_freqs *freq = data;

	if (val != CPUFREQ_POSTCHANGE)
		return 0;

	spin_lock(&cpu->lock);
	if (freq->cpu < num_possible_cpus())
		cpu->curr_freq[freq->cpu] = freq->new / 1000;
	spin_unlock(&cpu->lock);

	return 0;
}

static void idlestats_get_sample(struct msm_idle_stats_device *idledev,
	struct msm_idle_pulse *pulse)
{
	struct kgsl_power_stats stats;
	struct idlestats_priv *priv = container_of(idledev,
		struct idlestats_priv, idledev);
	struct kgsl_device *device = priv->device;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	mutex_lock(&device->mutex);
	/* If the GPU is asleep, don't wake it up - assume that we
	   are idle */

	if (device->state == KGSL_STATE_ACTIVE) {
		device->ftbl->power_stats(device, &stats);
		pulse->busy_start_time = pwr->time - stats.busy_time;
		pulse->busy_interval = stats.busy_time;
	} else {
		pulse->busy_start_time = pwr->time;
		pulse->busy_interval = 0;
	}
	pulse->wait_interval = 0;
	mutex_unlock(&device->mutex);
}

static void idlestats_busy(struct kgsl_device *device,
			struct kgsl_pwrscale *pwrscale)
{
	struct idlestats_priv *priv = pwrscale->priv;
	struct kgsl_power_stats stats;
	int i, busy, nr_cpu = 1;

	if (priv->pulse.busy_start_time != 0) {
		priv->pulse.wait_interval = 0;
		/* Calculate the total CPU busy time for this GPU pulse */
		for (i = 0; i < num_possible_cpus(); i++) {
			spin_lock(&priv->cpu_info.lock);
			if (cpu_online(i)) {
				priv->cpu_info.end[i] =
						(u64)ktime_to_us(ktime_get()) -
						get_cpu_idle_time_us(i, NULL);
				busy = priv->cpu_info.end[i] -
						priv->cpu_info.start[i];
				/* Normalize the busy time by frequency */
				busy = priv->cpu_info.curr_freq[i] *
					(busy / priv->cpu_info.max_freq[i]);
				priv->pulse.wait_interval += busy;
				nr_cpu++;
			}
			spin_unlock(&priv->cpu_info.lock);
		}
		priv->pulse.wait_interval /= nr_cpu;

		/* This is called from within a mutex protected function, so
		   no additional locking required */
		device->ftbl->power_stats(device, &stats);

		/* If total_time is zero, then we don't have
		   any interesting statistics to store */
		if (stats.total_time == 0) {
			priv->pulse.busy_start_time = 0;
			return;
		}

		priv->pulse.busy_interval = stats.busy_time;
		msm_idle_stats_idle_end(&priv->idledev, &priv->pulse);
	}
	priv->pulse.busy_start_time = ktime_to_us(ktime_get());
}

static void idlestats_idle(struct kgsl_device *device,
			struct kgsl_pwrscale *pwrscale)
{
	int i, nr_cpu;
	struct idlestats_priv *priv = pwrscale->priv;

	nr_cpu = num_possible_cpus();
	for (i = 0; i < nr_cpu; i++)
		if (cpu_online(i))
			priv->cpu_info.start[i] =
					(u64)ktime_to_us(ktime_get()) -
					get_cpu_idle_time_us(i, NULL);

	msm_idle_stats_idle_start(&priv->idledev);
}

static void idlestats_sleep(struct kgsl_device *device,
			struct kgsl_pwrscale *pwrscale)
{
	struct idlestats_priv *priv = pwrscale->priv;
	msm_idle_stats_update_event(&priv->idledev,
		MSM_IDLE_STATS_EVENT_IDLE_TIMER_EXPIRED);
}

static void idlestats_wake(struct kgsl_device *device,
			struct kgsl_pwrscale *pwrscale)
{
	/* Use highest perf level on wake-up from
	   sleep for better performance */
	kgsl_pwrctrl_pwrlevel_change(device, KGSL_PWRLEVEL_TURBO);
}

static int idlestats_init(struct kgsl_device *device,
		     struct kgsl_pwrscale *pwrscale)
{
	struct idlestats_priv *priv;
	struct cpufreq_policy cpu_policy;
	int ret, i;

	priv = pwrscale->priv = kzalloc(sizeof(struct idlestats_priv),
		GFP_KERNEL);
	if (pwrscale->priv == NULL)
		return -ENOMEM;

	snprintf(priv->name, sizeof(priv->name), "idle_stats_%s",
		 device->name);

	priv->device = device;

	priv->idledev.name = (const char *) priv->name;
	priv->idledev.get_sample = idlestats_get_sample;

	spin_lock_init(&priv->cpu_info.lock);
	priv->cpu_info.cpu_nb.notifier_call =
			idlestats_cpufreq_notifier;
	ret = cpufreq_register_notifier(&priv->cpu_info.cpu_nb,
				CPUFREQ_TRANSITION_NOTIFIER);
	if (ret)
		goto err;
	for (i = 0; i < num_possible_cpus(); i++) {
		cpufreq_frequency_table_cpuinfo(&cpu_policy,
					cpufreq_frequency_get_table(i));
		priv->cpu_info.max_freq[i] = cpu_policy.max / 1000;
		priv->cpu_info.curr_freq[i] = cpu_policy.max / 1000;
	}
	ret = msm_idle_stats_register_device(&priv->idledev);
err:
	if (ret) {
		kfree(pwrscale->priv);
		pwrscale->priv = NULL;
	}

	return ret;
}

static void idlestats_close(struct kgsl_device *device,
		      struct kgsl_pwrscale *pwrscale)
{
	struct idlestats_priv *priv = pwrscale->priv;

	if (pwrscale->priv == NULL)
		return;

	cpufreq_unregister_notifier(&priv->cpu_info.cpu_nb,
						CPUFREQ_TRANSITION_NOTIFIER);
	msm_idle_stats_deregister_device(&priv->idledev);

	kfree(pwrscale->priv);
	pwrscale->priv = NULL;
}

struct kgsl_pwrscale_policy kgsl_pwrscale_policy_idlestats = {
	.name = "idlestats",
	.init = idlestats_init,
	.idle = idlestats_idle,
	.busy = idlestats_busy,
	.sleep = idlestats_sleep,
	.wake = idlestats_wake,
	.close = idlestats_close
};
