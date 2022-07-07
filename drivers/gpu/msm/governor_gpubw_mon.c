// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/devfreq.h>
#include <linux/slab.h>

#include "../../devfreq/governor.h"
#include "msm_adreno_devfreq.h"

#define MIN_BUSY                1000
#define LONG_FLOOR              50000
#define HIST                    5
#define TARGET                  80
#define CAP                     75
#define WAIT_THRESHOLD          10
/* AB vote is in multiple of BW_STEP Mega bytes */
#define BW_STEP                 50

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

static ssize_t cur_ab_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct msm_busmon_extended_profile *bus_profile = container_of(
					(df->profile),
					struct msm_busmon_extended_profile,
					profile);

	return scnprintf(buf, PAGE_SIZE, "%llu\n", bus_profile->ab_mbytes);
}

static ssize_t sampling_interval_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct devfreq *df = to_devfreq(dev);
	struct msm_busmon_extended_profile *bus_profile = container_of(
					(df->profile),
					struct msm_busmon_extended_profile,
					profile);

	return scnprintf(buf, PAGE_SIZE, "%d\n", bus_profile->sampling_ms);
}

static ssize_t sampling_interval_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct devfreq *df = to_devfreq(dev);
	struct msm_busmon_extended_profile *bus_profile = container_of(
					(df->profile),
					struct msm_busmon_extended_profile,
					profile);
	u32 value;
	int ret;

	ret = kstrtou32(buf, 0, &value);
	if (ret)
		return ret;

	bus_profile->sampling_ms = value;

	return count;
}

static DEVICE_ATTR_RW(sampling_interval);
static DEVICE_ATTR_RO(cur_ab);

static const struct device_attribute *gpubw_attr_list[] = {
	&dev_attr_sampling_interval,
	&dev_attr_cur_ab,
	NULL
};

static u32 generate_hint(struct devfreq_msm_adreno_tz_data *priv, int buslevel,
		unsigned long freq, unsigned long minfreq)
{
	int act_level;
	int norm_max_cycles;
	int norm_cycles;
	int wait_active_percent;
	int gpu_percent;

	norm_max_cycles = (unsigned int)(priv->bus.ram_time) /
			(unsigned int) priv->bus.total_time;
	norm_cycles = (unsigned int)(priv->bus.ram_time + priv->bus.ram_wait) /
			(unsigned int) priv->bus.total_time;
	wait_active_percent = (100 * (unsigned int)priv->bus.ram_wait) /
			(unsigned int) priv->bus.ram_time;
	gpu_percent = (100 * (unsigned int)priv->bus.gpu_time) /
			(unsigned int) priv->bus.total_time;

	/*
	 * If there's a new high watermark, update the cutoffs and send the
	 * FAST hint, provided that we are using a floating watermark.
	 * Otherwise check the current value against the current
	 * cutoffs.
	 */
	if (norm_max_cycles > priv->bus.max && priv->bus.floating) {
		_update_cutoff(priv, norm_max_cycles);
		return BUSMON_FLAG_FAST_HINT;
	}

	/* Increase BW vote to avoid starving GPU for BW if required */
	if (priv->fast_bus_hint && minfreq == freq) {
		if (wait_active_percent > 95)
			return BUSMON_FLAG_SUPER_FAST_HINT;

		if (wait_active_percent > 80)
			return BUSMON_FLAG_FAST_HINT;
	}

	/* GPU votes for IB not AB so don't under vote the system */
	norm_cycles = (100 * norm_cycles) / TARGET;
	act_level = max_t(int, buslevel, 0);
	act_level = min_t(int, act_level, priv->bus.num - 1);

	if ((norm_cycles > priv->bus.up[act_level] ||
			wait_active_percent > WAIT_THRESHOLD) &&
			gpu_percent > CAP)
		return BUSMON_FLAG_FAST_HINT;

	if (norm_cycles < priv->bus.down[act_level] && buslevel)
		return BUSMON_FLAG_SLOW_HINT;

	return 0;
}

static int devfreq_gpubw_get_target(struct devfreq *df,
				unsigned long *freq)
{

	struct devfreq_msm_adreno_tz_data *priv = df->data;
	struct msm_busmon_extended_profile *bus_profile = container_of(
					(df->profile),
					struct msm_busmon_extended_profile,
					profile);
	struct devfreq_dev_status *stats = &df->last_status;
	struct xstats b;
	int result;
	int norm_ab;
	unsigned long ab_mbytes = 0;
	/*
	 * Normalized AB should at max usage be the gpu_bimc frequency in MHz.
	 * Start with a reasonable value and let the system push it up to max.
	 */
	static int norm_ab_max = 300;

	if (priv == NULL)
		return 0;

	stats->private_data = &b;

	result = devfreq_update_stats(df);
	/* Return if devfreq is not enabled */
	if (result)
		return result;

	*freq = stats->current_frequency;

	priv->bus.total_time += stats->total_time;
	priv->bus.gpu_time += stats->busy_time;
	priv->bus.ram_time += b.ram_time;
	priv->bus.ram_wait += b.ram_wait;

	if (priv->bus.total_time < bus_profile->sampling_ms)
		return result;

	bus_profile->flag = generate_hint(priv, b.buslevel, *freq,
			b.gpu_minfreq);

	/* Calculate the AB vote based on bus width if defined */
	if (priv->bus.width) {
		norm_ab =  (unsigned int)priv->bus.ram_time /
			(unsigned int) priv->bus.total_time;
		/* Calculate AB in Mega Bytes and roundup in BW_STEP */
		ab_mbytes = (norm_ab * priv->bus.width * 1000000ULL) >> 20;
		bus_profile->ab_mbytes = roundup(ab_mbytes, BW_STEP);
	} else if (bus_profile->flag) {
		/* Re-calculate the AB percentage for a new IB vote */
		norm_ab =  (unsigned int)priv->bus.ram_time /
			(unsigned int) priv->bus.total_time;
		if (norm_ab > norm_ab_max)
			norm_ab_max = norm_ab;
		bus_profile->percent_ab = (100 * norm_ab) / norm_ab_max;
	}

	priv->bus.total_time = 0;
	priv->bus.gpu_time = 0;
	priv->bus.ram_time = 0;
	priv->bus.ram_wait = 0;

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
	int i, bus_size;


	devfreq->data = bus_profile->private_data;
	priv = devfreq->data;

	bus_size = sizeof(u32) * priv->bus.num;
	priv->bus.up = kzalloc(bus_size, GFP_KERNEL);
	priv->bus.down = kzalloc(bus_size, GFP_KERNEL);
	priv->bus.p_up = kzalloc(bus_size, GFP_KERNEL);
	priv->bus.p_down = kzalloc(bus_size, GFP_KERNEL);
	if (priv->bus.up == NULL || priv->bus.down == NULL ||
		priv->bus.p_up == NULL || priv->bus.p_down == NULL)
		return -ENOMEM;

	/* Set up the cut-over percentages for the bus calculation. */
	for (i = 0; i < priv->bus.num; i++) {
		t1 = (u32)(100 * priv->bus.ib_kbps[i]) /
				(u32)priv->bus.ib_kbps[priv->bus.num - 1];
		priv->bus.p_up[i] = t1 - HIST;
		priv->bus.p_down[i] = t2 - 2 * HIST;
		t2 = t1;
	}
	/* Set the upper-most and lower-most bounds correctly. */
	priv->bus.p_down[0] = 0;

	for (i = 0; i < priv->bus.num; i++) {
		if (priv->bus.p_down[i] < 2 * HIST)
			priv->bus.p_down[i] = 2 * HIST;
	}

	if (priv->bus.num >= 1)
		priv->bus.p_up[priv->bus.num - 1] = 100;
	_update_cutoff(priv, priv->bus.max);

	bus_profile->sampling_ms = LONG_FLOOR;

	for (i = 0; gpubw_attr_list[i] != NULL; i++)
		device_create_file(&devfreq->dev, gpubw_attr_list[i]);

	return 0;
}

static int gpubw_stop(struct devfreq *devfreq)
{
	struct devfreq_msm_adreno_tz_data *priv = devfreq->data;
	int i;

	for (i = 0; gpubw_attr_list[i] != NULL; i++)
		device_remove_file(&devfreq->dev, gpubw_attr_list[i]);

	if (priv) {
		kfree(priv->bus.up);
		kfree(priv->bus.down);
		kfree(priv->bus.p_up);
		kfree(priv->bus.p_down);
	}
	devfreq->data = NULL;
	return 0;
}

static int devfreq_gpubw_event_handler(struct devfreq *devfreq,
				unsigned int event, void *data)
{
	int result = 0;
	unsigned long freq;

	if (strcmp(dev_name(devfreq->dev.parent), "kgsl-busmon"))
		return -EINVAL;

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

			if (priv) {
				priv->bus.total_time = 0;
				priv->bus.gpu_time = 0;
				priv->bus.ram_time = 0;
			}
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
	.immutable = 1,
};

int devfreq_gpubw_init(void)
{
	return devfreq_add_governor(&devfreq_gpubw);
}

void devfreq_gpubw_exit(void)
{
	int ret;

	ret = devfreq_remove_governor(&devfreq_gpubw);
	if (ret)
		pr_err("%s: failed remove governor %d\n", __func__, ret);

}
