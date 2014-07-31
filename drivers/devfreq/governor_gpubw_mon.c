/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#include <linux/devfreq.h>
#include <linux/module.h>
#include <linux/msm_adreno_devfreq.h>

#include "devfreq_trace.h"
#include "governor.h"

#define MIN_BUSY                1000
#define LONG_FLOOR              50000
#define HIST                    5
#define TARGET                  80
#define CAP                     75

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

static int devfreq_gpubw_get_target(struct devfreq *df,
				unsigned long *freq,
				u32 *flag)
{

	struct devfreq_msm_adreno_tz_data *priv = df->data;
	struct msm_busmon_extended_profile *bus_profile = container_of(
					(df->profile),
					struct msm_busmon_extended_profile,
					profile);
	struct devfreq_dev_status stats;
	struct xstats b;
	int result;
	int level = 0;
	int act_level;
	int norm_cycles;
	int gpu_percent;

	stats.private_data = &b;

	result = df->profile->get_dev_status(df->dev.parent, &stats);

	*freq = stats.current_frequency;

	priv->bus.total_time += stats.total_time;
	priv->bus.gpu_time += stats.busy_time;
	priv->bus.ram_time += b.ram_time;
	priv->bus.ram_time += b.ram_wait;

	level = devfreq_get_freq_level(df, stats.current_frequency);

	if (priv->bus.total_time < LONG_FLOOR)
		return result;

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
		bus_profile->flag = DEVFREQ_FLAG_FAST_HINT;
	} else {
		/* GPU votes for IB not AB so don't under vote the system */
		norm_cycles = (100 * norm_cycles) / TARGET;
		act_level = priv->bus.index[level] + b.mod;
		act_level = (act_level < 0) ? 0 : act_level;
		act_level = (act_level >= priv->bus.num) ?
		(priv->bus.num - 1) : act_level;
		if (norm_cycles > priv->bus.up[act_level] &&
				gpu_percent > CAP)
			bus_profile->flag = DEVFREQ_FLAG_FAST_HINT;
		else if (norm_cycles < priv->bus.down[act_level] && level)
			bus_profile->flag = DEVFREQ_FLAG_SLOW_HINT;
	}

	priv->bus.total_time = 0;
	priv->bus.gpu_time = 0;
	priv->bus.ram_time = 0;

	return result;
}

static int gpubw_start(struct devfreq *devfreq)
{
	struct devfreq_msm_adreno_tz_data *priv;

	struct msm_busmon_extended_profile *bus_profile = container_of(
					(devfreq->profile),
					struct msm_busmon_extended_profile,
					profile);
	unsigned int t1, t2 = 2 * HIST;
	int i;


	devfreq->data = bus_profile->private_data;
	priv = devfreq->data;

	/* Set up the cut-over percentages for the bus calculation. */
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
	if (priv->bus.num >= 1)
		priv->bus.p_up[priv->bus.num - 1] = 100;
	_update_cutoff(priv, priv->bus.max);

	return 0;
}

static int gpubw_stop(struct devfreq *devfreq)
{
	devfreq->data = NULL;
	return 0;
}

static int devfreq_gpubw_event_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	int result = 0;
	unsigned long freq;

	mutex_lock(&devfreq->lock);
	freq = devfreq->previous_freq;
	switch (event) {
	case DEVFREQ_GOV_START:
		result = gpubw_start(devfreq);
		break;
	case DEVFREQ_GOV_STOP:
		result = gpubw_stop(devfreq);
		break;
	case DEVFREQ_GOV_RESUME:
		/* TODO ..... */
		/* ret = update_devfreq(devfreq); */
		break;
	case DEVFREQ_GOV_SUSPEND:
		{
			struct devfreq_msm_adreno_tz_data *priv = devfreq->data;
			priv->bus.total_time = 0;
			priv->bus.gpu_time = 0;
			priv->bus.ram_time = 0;
		}
		break;
	default:
		result = 0;
		break;
	}
	mutex_unlock(&devfreq->lock);
	return result;
}

static struct devfreq_governor devfreq_gpubw = {
	.name = "gpubw_mon",
	.get_target_freq = devfreq_gpubw_get_target,
	.event_handler = devfreq_gpubw_event_handler,
};

static int __init devfreq_gpubw_init(void)
{
	return devfreq_add_governor(&devfreq_gpubw);
}
subsys_initcall(devfreq_gpubw_init);

static void __exit devfreq_gpubw_exit(void)
{
	int ret;

	ret = devfreq_remove_governor(&devfreq_gpubw);
	if (ret)
		pr_err("%s: failed remove governor %d\n", __func__, ret);

	return;
}
module_exit(devfreq_gpubw_exit);

MODULE_DESCRIPTION("GPU bus bandwidth voting driver. Uses VBIF counters");
MODULE_LICENSE("GPL v2");

