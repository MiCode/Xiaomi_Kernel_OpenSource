/* Copyright (c) 2010-2015, The Linux Foundation. All rights reserved.
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
#include <linux/interrupt.h>
#include <asm/page.h>
#include <linux/pm_runtime.h>
#include <linux/msm-bus.h>
#include <linux/msm-bus-board.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/msm_adreno_devfreq.h>
#include <linux/of_device.h>

#include "kgsl.h"
#include "kgsl_pwrscale.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"
#include <soc/qcom/devfreq_devbw.h>

#define KGSL_PWRFLAGS_POWER_ON 0
#define KGSL_PWRFLAGS_CLK_ON   1
#define KGSL_PWRFLAGS_AXI_ON   2
#define KGSL_PWRFLAGS_IRQ_ON   3

#define UPDATE_BUSY_VAL		1000000

/*
 * Expected delay for post-interrupt processing on A3xx.
 * The delay may be longer, gradually increase the delay
 * to compensate.  If the GPU isn't done by max delay,
 * it's working on something other than just the final
 * command sequence so stop waiting for it to be idle.
 */
#define INIT_UDELAY		200
#define MAX_UDELAY		2000

/* Number of jiffies for a full thermal cycle */
#define TH_HZ			20

#define KGSL_MAX_BUSLEVELS	20

#define DEFAULT_BUS_P 25
#define DEFAULT_BUS_DIV (100 / DEFAULT_BUS_P)

struct clk_pair {
	const char *name;
	uint map;
};

static struct clk_pair clks[KGSL_MAX_CLKS] = {
	{
		.name = "src_clk",
		.map = KGSL_CLK_SRC,
	},
	{
		.name = "core_clk",
		.map = KGSL_CLK_CORE,
	},
	{
		.name = "iface_clk",
		.map = KGSL_CLK_IFACE,
	},
	{
		.name = "mem_clk",
		.map = KGSL_CLK_MEM,
	},
	{
		.name = "mem_iface_clk",
		.map = KGSL_CLK_MEM_IFACE,
	},
	{
		.name = "alt_mem_iface_clk",
		.map = KGSL_CLK_ALT_MEM_IFACE,
	},
	{
		.name = "rbbmtimer_clk",
		.map = KGSL_CLK_RBBMTIMER,
	},
	{
		.name = "gtcu_clk",
		.map = KGSL_CLK_GFX_GTCU,
	},
	{
		.name = "gtbu_clk",
		.map = KGSL_CLK_GFX_GTBU,
	},
};

static unsigned int ib_votes[KGSL_MAX_BUSLEVELS];
static int last_vote_buslevel;
static int max_vote_buslevel;

static void kgsl_pwrctrl_clk(struct kgsl_device *device, int state,
					int requested_state);
static void kgsl_pwrctrl_axi(struct kgsl_device *device, int state);
static int kgsl_pwrctrl_pwrrail(struct kgsl_device *device, int state);
static void kgsl_pwrctrl_set_state(struct kgsl_device *device,
				unsigned int state);
static void kgsl_pwrctrl_request_state(struct kgsl_device *device,
				unsigned int state);

/**
 * kgsl_get_bw() - Return latest msm bus IB vote
 */
static unsigned int kgsl_get_bw(void)
{
	return ib_votes[last_vote_buslevel];
}

/**
 * _ab_buslevel_update() - Return latest msm bus AB vote
 * @pwr: Pointer to the kgsl_pwrctrl struct
 * @ab: Pointer to be updated with the calculated AB vote
 */
static void _ab_buslevel_update(struct kgsl_pwrctrl *pwr,
				unsigned long *ab)
{
	unsigned int ib = ib_votes[last_vote_buslevel];
	unsigned int max_bw = ib_votes[max_vote_buslevel];
	if (!ab)
		return;
	if (ib == 0)
		*ab = 0;
	else if (!pwr->bus_percent_ab)
		*ab = DEFAULT_BUS_P * ib / 100;
	else
		*ab = (pwr->bus_percent_ab * max_bw) / 100;

	if (*ab > ib)
		*ab = ib;
}

/**
 * _adjust_pwrlevel() - Given a requested power level do bounds checking on the
 * constraints and return the nearest possible level
 * @device: Pointer to the kgsl_device struct
 * @level: Requested level
 * @pwrc: Pointer to the power constraint to be applied
 *
 * Apply thermal and max/min limits first.  Then force the level with a
 * constraint if one exists.
 */
static unsigned int _adjust_pwrlevel(struct kgsl_pwrctrl *pwr, int level,
					struct kgsl_pwr_constraint *pwrc)
{
	unsigned int max_pwrlevel = max_t(unsigned int, pwr->thermal_pwrlevel,
		pwr->max_pwrlevel);
	unsigned int min_pwrlevel = max_t(unsigned int, pwr->thermal_pwrlevel,
		pwr->min_pwrlevel);

	switch (pwrc->type) {
	case KGSL_CONSTRAINT_PWRLEVEL: {
		switch (pwrc->sub_type) {
		case KGSL_CONSTRAINT_PWR_MAX:
			return max_pwrlevel;
			break;
		case KGSL_CONSTRAINT_PWR_MIN:
			return min_pwrlevel;
			break;
		default:
			break;
		}
	}
	break;
	}

	if (level < max_pwrlevel)
		return max_pwrlevel;
	if (level > min_pwrlevel)
		return min_pwrlevel;

	return level;
}

/**
 * kgsl_pwrctrl_buslevel_update() - Recalculate the bus vote and send it
 * @device: Pointer to the kgsl_device struct
 * @on: true for setting and active bus vote, false to turn off the vote
 */
void kgsl_pwrctrl_buslevel_update(struct kgsl_device *device,
			bool on)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int cur = pwr->pwrlevels[pwr->active_pwrlevel].bus_freq;
	int buslevel = 0;
	unsigned long ab;
	if (!pwr->pcl)
		return;
	/* the bus should be ON to update the active frequency */
	if (on && !(test_bit(KGSL_PWRFLAGS_AXI_ON, &pwr->power_flags)))
		return;
	/*
	 * If the bus should remain on calculate our request and submit it,
	 * otherwise request bus level 0, off.
	 */
	if (on) {
		buslevel = min_t(int, pwr->pwrlevels[0].bus_freq,
				cur + pwr->bus_mod);
		buslevel = max_t(int, buslevel, 1);
	} else {
		/* If the bus is being turned off, reset to default level */
		pwr->bus_mod = 0;
	}
	trace_kgsl_buslevel(device, pwr->active_pwrlevel, buslevel);
	last_vote_buslevel = buslevel;
	/* buslevel is the IB vote, update the AB */
	_ab_buslevel_update(pwr, &ab);
	/* vote for ocmem */
	msm_bus_scale_client_update_request(pwr->pcl, buslevel);
	/* ask a governor to vote on behalf of us */
	devfreq_vbif_update_bw(ib_votes[last_vote_buslevel], ab);
}
EXPORT_SYMBOL(kgsl_pwrctrl_buslevel_update);

/**
 * kgsl_pwrctrl_pwrlevel_change() - Validate and change power levels
 * @device: Pointer to the kgsl_device struct
 * @new_level: Requested powerlevel, an index into the pwrlevel array
 *
 * Check that any power level constraints are still valid.  Update the
 * requested level according to any thermal, max/min, or power constraints.
 * If a new GPU level is going to be set, update the bus to that level's
 * default value.  Do not change the bus if a constraint keeps the new
 * level at the current level.  Set the new GPU frequency.
 */
void kgsl_pwrctrl_pwrlevel_change(struct kgsl_device *device,
				unsigned int new_level)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct kgsl_pwrlevel *pwrlevel;
	unsigned int old_level = pwr->active_pwrlevel;

	/* If a pwr constraint is expired, remove it */
	if ((pwr->constraint.type != KGSL_CONSTRAINT_NONE) &&
		(time_after(jiffies, pwr->constraint.expires))) {
		/* Trace the constraint being un-set by the driver */
		trace_kgsl_constraint(device, pwr->constraint.type,
						old_level, 0);
		/*Invalidate the constraint set */
		pwr->constraint.expires = 0;
		pwr->constraint.type = KGSL_CONSTRAINT_NONE;
	}

	/*
	 * Adjust the power level if required by thermal, max/min,
	 * constraints, etc
	 */
	new_level = _adjust_pwrlevel(pwr, new_level, &pwr->constraint);

	/*
	 * If thermal cycling is required and the new level hits the
	 * thermal limit, kick off the cycling.
	 */
	if ((pwr->thermal_cycle == CYCLE_ENABLE) &&
			(new_level == pwr->thermal_pwrlevel)) {
		pwr->thermal_cycle = CYCLE_ACTIVE;
		mod_timer(&pwr->thermal_timer, jiffies +
				(TH_HZ - pwr->thermal_timeout));
		pwr->thermal_highlow = 1;
	}

	if (new_level == old_level)
		return;

	/*
	 * Set the active powerlevel first in case the clocks are off - if we
	 * don't do this then the pwrlevel change won't take effect when the
	 * clocks come back
	 */
	pwr->active_pwrlevel = new_level;

	/*
	 * Update the bus before the GPU clock to prevent underrun during
	 * frequency increases.
	 */
	pwr->bus_mod = 0;
	pwr->bus_percent_ab = 0;
	kgsl_pwrctrl_buslevel_update(device, true);

	pwrlevel = &pwr->pwrlevels[pwr->active_pwrlevel];
	clk_set_rate(pwr->grp_clks[0], pwrlevel->gpu_freq);
	trace_kgsl_pwrlevel(device, pwr->active_pwrlevel,
			pwrlevel->gpu_freq);
}
EXPORT_SYMBOL(kgsl_pwrctrl_pwrlevel_change);

/**
 * kgsl_pwrctrl_set_constraint() - Validate and change enforced constraint
 * @device: Pointer to the kgsl_device struct
 * @pwrc: Pointer to requested constraint
 * @id: Context id which owns the constraint
 *
 * Accept the new constraint if no previous constraint existed or if the
 * new constraint is faster than the previous one.  If the new and previous
 * constraints are equal, update the timestamp and ownership to make sure
 * the constraint expires at the correct time.
 */
void kgsl_pwrctrl_set_constraint(struct kgsl_device *device,
			struct kgsl_pwr_constraint *pwrc, uint32_t id)
{
	unsigned int constraint;
	struct kgsl_pwr_constraint *pwrc_old;

	if (device == NULL || pwrc == NULL)
		return;
	constraint = _adjust_pwrlevel(&device->pwrctrl,
				device->pwrctrl.active_pwrlevel, pwrc);
	pwrc_old = &device->pwrctrl.constraint;

	/*
	 * If a constraint is already set, set a new constraint only
	 * if it is faster.  If the requested constraint is the same
	 * as the current one, update ownership and timestamp.
	 */
	if ((pwrc_old->type == KGSL_CONSTRAINT_NONE) ||
		(constraint < pwrc_old->hint.pwrlevel.level)) {
		pwrc_old->type = pwrc->type;
		pwrc_old->sub_type = pwrc->sub_type;
		pwrc_old->hint.pwrlevel.level = constraint;
		pwrc_old->owner_id = id;
		pwrc_old->expires = jiffies + device->pwrctrl.interval_timeout;
		kgsl_pwrctrl_pwrlevel_change(device, constraint);
		/* Trace the constraint being set by the driver */
		trace_kgsl_constraint(device, pwrc_old->type, constraint, 1);
	} else if ((pwrc_old->type == pwrc->type) &&
		(pwrc_old->hint.pwrlevel.level == constraint)) {
			pwrc_old->owner_id = id;
			pwrc_old->expires = jiffies +
					device->pwrctrl.interval_timeout;
	}
}
EXPORT_SYMBOL(kgsl_pwrctrl_set_constraint);

static ssize_t kgsl_pwrctrl_thermal_pwrlevel_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	int ret;
	unsigned int level = 0;

	if (device == NULL)
		return 0;

	pwr = &device->pwrctrl;

	ret = kgsl_sysfs_store(buf, &level);

	if (ret)
		return ret;

	mutex_lock(&device->mutex);

	if (level > pwr->num_pwrlevels - 2)
		level = pwr->num_pwrlevels - 2;

	pwr->thermal_pwrlevel = level;

	/* Update the current level using the new limit */
	kgsl_pwrctrl_pwrlevel_change(device, pwr->active_pwrlevel);
	mutex_unlock(&device->mutex);

	return count;
}

static ssize_t kgsl_pwrctrl_thermal_pwrlevel_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{

	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;
	return snprintf(buf, PAGE_SIZE, "%d\n", pwr->thermal_pwrlevel);
}

static ssize_t kgsl_pwrctrl_max_pwrlevel_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	int ret;
	unsigned int level = 0;

	if (device == NULL)
		return 0;

	pwr = &device->pwrctrl;

	ret = kgsl_sysfs_store(buf, &level);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);

	/* You can't set a maximum power level lower than the minimum */
	if (level > pwr->min_pwrlevel)
		level = pwr->min_pwrlevel;

	pwr->max_pwrlevel = level;

	/* Update the current level using the new limit */
	kgsl_pwrctrl_pwrlevel_change(device, pwr->active_pwrlevel);
	mutex_unlock(&device->mutex);

	return count;
}

static ssize_t kgsl_pwrctrl_max_pwrlevel_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{

	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;
	return snprintf(buf, PAGE_SIZE, "%u\n", pwr->max_pwrlevel);
}

static ssize_t kgsl_pwrctrl_min_pwrlevel_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	int ret;
	unsigned int level = 0;

	if (device == NULL)
		return 0;

	pwr = &device->pwrctrl;

	ret = kgsl_sysfs_store(buf, &level);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);
	if (level > pwr->num_pwrlevels - 2)
		level = pwr->num_pwrlevels - 2;

	/* You can't set a minimum power level lower than the maximum */
	if (level < pwr->max_pwrlevel)
		level = pwr->max_pwrlevel;

	pwr->min_pwrlevel = level;

	/* Update the current level using the new limit */
	kgsl_pwrctrl_pwrlevel_change(device, pwr->active_pwrlevel);

	mutex_unlock(&device->mutex);

	return count;
}

static ssize_t kgsl_pwrctrl_min_pwrlevel_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;
	return snprintf(buf, PAGE_SIZE, "%u\n", pwr->min_pwrlevel);
}

static ssize_t kgsl_pwrctrl_num_pwrlevels_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{

	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;
	return snprintf(buf, PAGE_SIZE, "%d\n", pwr->num_pwrlevels - 1);
}

/* Given a GPU clock value, return the lowest matching powerlevel */

static int _get_nearest_pwrlevel(struct kgsl_pwrctrl *pwr, unsigned int clock)
{
	int i;

	for (i = pwr->num_pwrlevels - 1; i >= 0; i--) {
		if (abs(pwr->pwrlevels[i].gpu_freq - clock) < 5000000)
			return i;
	}

	return -ERANGE;
}

static ssize_t kgsl_pwrctrl_max_gpuclk_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	unsigned int val = 0;
	int level, ret;

	if (device == NULL)
		return 0;

	pwr = &device->pwrctrl;

	ret = kgsl_sysfs_store(buf, &val);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);
	level = _get_nearest_pwrlevel(pwr, val);
	/* If the requested power level is not supported by hw, try cycling */
	if (level < 0) {
		unsigned int hfreq, diff, udiff, i;
		if ((val < pwr->pwrlevels[pwr->num_pwrlevels - 1].gpu_freq) ||
			(val > pwr->pwrlevels[0].gpu_freq))
			goto done;
		/* Find the neighboring frequencies */
		for (i = 0; i < pwr->num_pwrlevels - 1; i++) {
			if ((pwr->pwrlevels[i].gpu_freq > val) &&
				(pwr->pwrlevels[i + 1].gpu_freq < val)) {
				level = i;
				break;
			}
		}
		hfreq = pwr->pwrlevels[i].gpu_freq;
		diff =  hfreq - pwr->pwrlevels[i + 1].gpu_freq;
		udiff = hfreq - val;
		pwr->thermal_timeout = (udiff * TH_HZ) / diff;
		pwr->thermal_cycle = CYCLE_ENABLE;
	} else {
		pwr->thermal_cycle = CYCLE_DISABLE;
		del_timer_sync(&pwr->thermal_timer);
	}

	pwr->thermal_pwrlevel = (unsigned int) level;

	/* Update the current level using the new limit */
	kgsl_pwrctrl_pwrlevel_change(device, pwr->active_pwrlevel);

done:
	mutex_unlock(&device->mutex);
	return count;
}

static ssize_t kgsl_pwrctrl_max_gpuclk_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{

	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	unsigned int freq;
	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;
	freq = pwr->pwrlevels[pwr->thermal_pwrlevel].gpu_freq;
	/* Calculate the effective frequency if we're cycling */
	if (pwr->thermal_cycle) {
		unsigned int hfreq = freq;
		unsigned int lfreq = pwr->pwrlevels[pwr->
				thermal_pwrlevel + 1].gpu_freq;
		freq = pwr->thermal_timeout * (lfreq / TH_HZ) +
			(TH_HZ - pwr->thermal_timeout) * (hfreq / TH_HZ);
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", freq);
}

static ssize_t kgsl_pwrctrl_gpuclk_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	unsigned int val = 0;
	int ret, level;

	if (device == NULL)
		return 0;

	pwr = &device->pwrctrl;

	ret = kgsl_sysfs_store(buf, &val);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);
	level = _get_nearest_pwrlevel(pwr, val);
	if (level >= 0)
		kgsl_pwrctrl_pwrlevel_change(device, (unsigned int) level);

	mutex_unlock(&device->mutex);
	return count;
}

static ssize_t kgsl_pwrctrl_gpuclk_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;
	return snprintf(buf, PAGE_SIZE, "%ld\n", kgsl_pwrctrl_active_freq(pwr));
}

static ssize_t kgsl_pwrctrl_idle_timer_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val = 0;
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	int ret;

	if (device == NULL)
		return 0;

	ret = kgsl_sysfs_store(buf, &val);
	if (ret)
		return ret;

	/*
	 * We don't quite accept a maximum of 0xFFFFFFFF due to internal jiffy
	 * math, so make sure the value falls within the largest offset we can
	 * deal with
	 */

	if (val > jiffies_to_usecs(MAX_JIFFY_OFFSET))
		return -EINVAL;

	mutex_lock(&device->mutex);

	/* Let the timeout be requested in ms, but convert to jiffies. */
	device->pwrctrl.interval_timeout = msecs_to_jiffies(val);

	mutex_unlock(&device->mutex);

	return count;
}

static ssize_t kgsl_pwrctrl_idle_timer_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	if (device == NULL)
		return 0;
	/* Show the idle_timeout converted to msec */
	return snprintf(buf, PAGE_SIZE, "%u\n",
		jiffies_to_msecs(device->pwrctrl.interval_timeout));
}

static ssize_t kgsl_pwrctrl_pmqos_active_latency_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val = 0;
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	int ret;

	if (device == NULL)
		return 0;

	ret = kgsl_sysfs_store(buf, &val);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);
	device->pwrctrl.pm_qos_active_latency = val;
	mutex_unlock(&device->mutex);

	return count;
}

static ssize_t kgsl_pwrctrl_pmqos_active_latency_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	if (device == NULL)
		return 0;
	return snprintf(buf, PAGE_SIZE, "%d\n",
		device->pwrctrl.pm_qos_active_latency);
}

static ssize_t kgsl_pwrctrl_gpubusy_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret;
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_clk_stats *stats;

	if (device == NULL)
		return 0;
	stats = &device->pwrctrl.clk_stats;
	ret = snprintf(buf, PAGE_SIZE, "%7d %7d\n",
			stats->busy_old, stats->total_old);
	if (!test_bit(KGSL_PWRFLAGS_AXI_ON, &device->pwrctrl.power_flags)) {
		stats->busy_old = 0;
		stats->total_old = 0;
	}
	return ret;
}

static ssize_t kgsl_pwrctrl_gpu_available_frequencies_show(
					struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	int index, num_chars = 0;

	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;
	for (index = 0; index < pwr->num_pwrlevels - 1; index++)
		num_chars += snprintf(buf + num_chars, PAGE_SIZE, "%d ",
		pwr->pwrlevels[index].gpu_freq);
	buf[num_chars++] = '\n';
	return num_chars;
}

static ssize_t kgsl_pwrctrl_reset_count_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	if (device == NULL)
		return 0;
	return snprintf(buf, PAGE_SIZE, "%d\n", device->reset_counter);
}

static void __force_on(struct kgsl_device *device, int flag, int on)
{
	if (on) {
		switch (flag) {
		case KGSL_PWRFLAGS_CLK_ON:
			kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_ON,
				KGSL_STATE_ACTIVE);
			break;
		case KGSL_PWRFLAGS_AXI_ON:
			kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_ON);
			break;
		case KGSL_PWRFLAGS_POWER_ON:
			kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_ON);
			break;
		}
		set_bit(flag, &device->pwrctrl.ctrl_flags);
	} else {
		clear_bit(flag, &device->pwrctrl.ctrl_flags);
	}
}

static ssize_t __force_on_show(struct device *dev,
					struct device_attribute *attr,
					char *buf, int flag)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	if (device == NULL)
		return 0;
	return snprintf(buf, PAGE_SIZE, "%d\n",
		test_bit(flag, &device->pwrctrl.ctrl_flags));
}

static ssize_t __force_on_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count,
					int flag)
{
	unsigned int val = 0;
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	int ret;

	if (device == NULL)
		return 0;

	ret = kgsl_sysfs_store(buf, &val);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);
	__force_on(device, flag, val);
	mutex_unlock(&device->mutex);

	return count;
}

static ssize_t kgsl_pwrctrl_force_clk_on_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return __force_on_show(dev, attr, buf, KGSL_PWRFLAGS_CLK_ON);
}

static ssize_t kgsl_pwrctrl_force_clk_on_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	return __force_on_store(dev, attr, buf, count, KGSL_PWRFLAGS_CLK_ON);
}

static ssize_t kgsl_pwrctrl_force_bus_on_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return __force_on_show(dev, attr, buf, KGSL_PWRFLAGS_AXI_ON);
}

static ssize_t kgsl_pwrctrl_force_bus_on_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	return __force_on_store(dev, attr, buf, count, KGSL_PWRFLAGS_AXI_ON);
}

static ssize_t kgsl_pwrctrl_force_rail_on_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return __force_on_show(dev, attr, buf, KGSL_PWRFLAGS_POWER_ON);
}

static ssize_t kgsl_pwrctrl_force_rail_on_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	return __force_on_store(dev, attr, buf, count, KGSL_PWRFLAGS_POWER_ON);
}

static ssize_t kgsl_pwrctrl_bus_split_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	if (device == NULL)
		return 0;
	return snprintf(buf, PAGE_SIZE, "%d\n",
		device->pwrctrl.bus_control);
}

static ssize_t kgsl_pwrctrl_bus_split_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val = 0;
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	int ret;

	if (device == NULL)
		return 0;

	ret = kgsl_sysfs_store(buf, &val);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);
	device->pwrctrl.bus_control = val ? true : false;
	mutex_unlock(&device->mutex);

	return count;
}

static ssize_t kgsl_pwrctrl_default_pwrlevel_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	if (device == NULL)
		return 0;
	return snprintf(buf, PAGE_SIZE, "%d\n",
		device->pwrctrl.default_pwrlevel);
}

static ssize_t kgsl_pwrctrl_default_pwrlevel_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	struct kgsl_pwrscale *pwrscale;
	int ret;
	unsigned int level = 0;

	if (device == NULL)
		return 0;

	pwr = &device->pwrctrl;
	pwrscale = &device->pwrscale;

	ret = kgsl_sysfs_store(buf, &level);
	if (ret)
		return ret;

	if (level > pwr->num_pwrlevels - 2)
		goto done;

	mutex_lock(&device->mutex);
	pwr->default_pwrlevel = level;
	pwrscale->gpu_profile.profile.initial_freq
			= pwr->pwrlevels[level].gpu_freq;

	mutex_unlock(&device->mutex);
done:
	return count;
}

static DEVICE_ATTR(gpuclk, 0644, kgsl_pwrctrl_gpuclk_show,
	kgsl_pwrctrl_gpuclk_store);
static DEVICE_ATTR(max_gpuclk, 0644, kgsl_pwrctrl_max_gpuclk_show,
	kgsl_pwrctrl_max_gpuclk_store);
static DEVICE_ATTR(idle_timer, 0644, kgsl_pwrctrl_idle_timer_show,
	kgsl_pwrctrl_idle_timer_store);
static DEVICE_ATTR(gpubusy, 0444, kgsl_pwrctrl_gpubusy_show,
	NULL);
static DEVICE_ATTR(gpu_available_frequencies, 0444,
	kgsl_pwrctrl_gpu_available_frequencies_show,
	NULL);
static DEVICE_ATTR(max_pwrlevel, 0644,
	kgsl_pwrctrl_max_pwrlevel_show,
	kgsl_pwrctrl_max_pwrlevel_store);
static DEVICE_ATTR(min_pwrlevel, 0644,
	kgsl_pwrctrl_min_pwrlevel_show,
	kgsl_pwrctrl_min_pwrlevel_store);
static DEVICE_ATTR(thermal_pwrlevel, 0644,
	kgsl_pwrctrl_thermal_pwrlevel_show,
	kgsl_pwrctrl_thermal_pwrlevel_store);
static DEVICE_ATTR(num_pwrlevels, 0444,
	kgsl_pwrctrl_num_pwrlevels_show,
	NULL);
static DEVICE_ATTR(pmqos_active_latency, 0644,
	kgsl_pwrctrl_pmqos_active_latency_show,
	kgsl_pwrctrl_pmqos_active_latency_store);
static DEVICE_ATTR(reset_count, 0444,
	kgsl_pwrctrl_reset_count_show,
	NULL);
static DEVICE_ATTR(force_clk_on, 0644,
	kgsl_pwrctrl_force_clk_on_show,
	kgsl_pwrctrl_force_clk_on_store);
static DEVICE_ATTR(force_bus_on, 0644,
	kgsl_pwrctrl_force_bus_on_show,
	kgsl_pwrctrl_force_bus_on_store);
static DEVICE_ATTR(force_rail_on, 0644,
	kgsl_pwrctrl_force_rail_on_show,
	kgsl_pwrctrl_force_rail_on_store);
static DEVICE_ATTR(bus_split, 0644,
	kgsl_pwrctrl_bus_split_show,
	kgsl_pwrctrl_bus_split_store);
static DEVICE_ATTR(default_pwrlevel, 0644,
	kgsl_pwrctrl_default_pwrlevel_show,
	kgsl_pwrctrl_default_pwrlevel_store);

static const struct device_attribute *pwrctrl_attr_list[] = {
	&dev_attr_gpuclk,
	&dev_attr_max_gpuclk,
	&dev_attr_idle_timer,
	&dev_attr_gpubusy,
	&dev_attr_gpu_available_frequencies,
	&dev_attr_max_pwrlevel,
	&dev_attr_min_pwrlevel,
	&dev_attr_thermal_pwrlevel,
	&dev_attr_num_pwrlevels,
	&dev_attr_pmqos_active_latency,
	&dev_attr_reset_count,
	&dev_attr_force_clk_on,
	&dev_attr_force_bus_on,
	&dev_attr_force_rail_on,
	&dev_attr_bus_split,
	&dev_attr_default_pwrlevel,
	NULL
};

int kgsl_pwrctrl_init_sysfs(struct kgsl_device *device)
{
	return kgsl_create_device_sysfs_files(device->dev, pwrctrl_attr_list);
}

void kgsl_pwrctrl_uninit_sysfs(struct kgsl_device *device)
{
	kgsl_remove_device_sysfs_files(device->dev, pwrctrl_attr_list);
}

/* Track the amount of time the gpu is on vs the total system time. *
 * Regularly update the percentage of busy time displayed by sysfs. */
void kgsl_pwrctrl_busy_time(struct kgsl_device *device, u64 time, u64 busy)
{
	struct kgsl_clk_stats *stats = &device->pwrctrl.clk_stats;
	stats->total += time;
	stats->busy += busy;

	if (stats->total < UPDATE_BUSY_VAL)
		return;

	/* Update the output regularly and reset the counters. */
	stats->total_old = stats->total;
	stats->busy_old = stats->busy;
	stats->total = 0;
	stats->busy = 0;

	trace_kgsl_gpubusy(device, stats->busy_old, stats->total_old);
}
EXPORT_SYMBOL(kgsl_pwrctrl_busy_time);

void kgsl_pwrctrl_clk(struct kgsl_device *device, int state,
					  int requested_state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i = 0;

	if (test_bit(KGSL_PWRFLAGS_CLK_ON, &pwr->ctrl_flags))
		return;

	if (state == KGSL_PWRFLAGS_OFF) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_CLK_ON,
			&pwr->power_flags)) {
			trace_kgsl_clk(device, state);
			for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
				if (pwr->grp_clks[i])
					clk_disable(pwr->grp_clks[i]);
			/* High latency clock maintenance. */
			if ((pwr->pwrlevels[0].gpu_freq > 0) &&
				(requested_state != KGSL_STATE_NAP)) {
				for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
					if (pwr->grp_clks[i])
						clk_unprepare(pwr->grp_clks[i]);
				clk_set_rate(pwr->grp_clks[0],
					pwr->pwrlevels[pwr->num_pwrlevels - 1].
					gpu_freq);
			}
		} else if (requested_state == KGSL_STATE_SLEEP) {
			/* High latency clock maintenance. */
			for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
				if (pwr->grp_clks[i])
					clk_unprepare(pwr->grp_clks[i]);
			if ((pwr->pwrlevels[0].gpu_freq > 0))
				clk_set_rate(pwr->grp_clks[0],
					pwr->pwrlevels[pwr->num_pwrlevels - 1].
					gpu_freq);
		}
	} else if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_CLK_ON,
			&pwr->power_flags)) {
			trace_kgsl_clk(device, state);
			/* High latency clock maintenance. */
			if (device->state != KGSL_STATE_NAP) {
				if (pwr->pwrlevels[0].gpu_freq > 0)
					clk_set_rate(pwr->grp_clks[0],
						pwr->pwrlevels
						[pwr->active_pwrlevel].
						gpu_freq);
				for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
					if (pwr->grp_clks[i])
						clk_prepare(pwr->grp_clks[i]);
			}
			/* as last step, enable grp_clk
			   this is to let GPU interrupt to come */
			for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
				if (pwr->grp_clks[i])
					clk_enable(pwr->grp_clks[i]);
		}
	}
}

static void kgsl_pwrctrl_axi(struct kgsl_device *device, int state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if (test_bit(KGSL_PWRFLAGS_AXI_ON, &pwr->ctrl_flags))
		return;

	if (state == KGSL_PWRFLAGS_OFF) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_AXI_ON,
			&pwr->power_flags)) {
			trace_kgsl_bus(device, state);
			kgsl_pwrctrl_buslevel_update(device, false);

			if (pwr->devbw)
				devfreq_suspend_devbw(pwr->devbw);
		}
	} else if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_AXI_ON,
			&pwr->power_flags)) {
			trace_kgsl_bus(device, state);
			kgsl_pwrctrl_buslevel_update(device, true);

			if (pwr->devbw)
				devfreq_resume_devbw(pwr->devbw);
		}
	}
}

static int kgsl_pwrctrl_pwrrail(struct kgsl_device *device, int state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int status_gpu = 0;
	int status_cx = 0;

	if (test_bit(KGSL_PWRFLAGS_POWER_ON, &pwr->ctrl_flags))
		return 0;

	if (state == KGSL_PWRFLAGS_OFF) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_POWER_ON,
			&pwr->power_flags)) {
			trace_kgsl_rail(device, state);
			if (pwr->gpu_cx)
				regulator_disable(pwr->gpu_cx);
			if (pwr->gpu_reg)
				regulator_disable(pwr->gpu_reg);
		}
	} else if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_POWER_ON,
			&pwr->power_flags)) {
			if (pwr->gpu_reg) {
				status_gpu = regulator_enable(pwr->gpu_reg);
				if (status_gpu)
					KGSL_DRV_ERR(device,
							"core regulator_enable "
							"failed: %d\n",
							status_gpu);
			}
			if (pwr->gpu_cx) {
				status_cx = regulator_enable(pwr->gpu_cx);
				if (status_cx)
					KGSL_DRV_ERR(device,
							"cx regulator_enable "
							"failed: %d\n",
							status_cx);
			}
			if (status_gpu || status_cx) {
				/*
				 * If only one rail succeeded, disable it
				 * before we bail out.
				 */
				if (pwr->gpu_reg && !status_gpu)
					regulator_disable(pwr->gpu_reg);
				if (pwr->gpu_cx && !status_cx)
					regulator_disable(pwr->gpu_cx);
				clear_bit(KGSL_PWRFLAGS_POWER_ON,
					&pwr->power_flags);
			} else
				trace_kgsl_rail(device, state);
		}
	}

	return status_gpu || status_cx;
}

static void kgsl_pwrctrl_irq(struct kgsl_device *device, int state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_IRQ_ON,
			&pwr->power_flags)) {
			trace_kgsl_irq(device, state);
			enable_irq(pwr->interrupt_num);
		}
	} else if (state == KGSL_PWRFLAGS_OFF) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_IRQ_ON,
			&pwr->power_flags)) {
			trace_kgsl_irq(device, state);
			if (in_interrupt())
				disable_irq_nosync(pwr->interrupt_num);
			else
				disable_irq(pwr->interrupt_num);
		}
	}
}

/**
 * kgsl_thermal_cycle() - Work function for thermal timer.
 * @work: The input work
 *
 * This function is called for work that is queued by the thermal
 * timer.  It cycles to the alternate thermal frequency.
 */
static void kgsl_thermal_cycle(struct work_struct *work)
{
	struct kgsl_pwrctrl *pwr = container_of(work, struct kgsl_pwrctrl,
						thermal_cycle_ws);
	struct kgsl_device *device = container_of(pwr, struct kgsl_device,
							pwrctrl);

	if (device == NULL)
		return;

	mutex_lock(&device->mutex);
	if (pwr->thermal_cycle == CYCLE_ACTIVE) {
		if (pwr->thermal_highlow)
			kgsl_pwrctrl_pwrlevel_change(device,
					pwr->thermal_pwrlevel);
		else
			kgsl_pwrctrl_pwrlevel_change(device,
					pwr->thermal_pwrlevel + 1);
	}
	mutex_unlock(&device->mutex);
}

void kgsl_thermal_timer(unsigned long data)
{
	struct kgsl_device *device = (struct kgsl_device *) data;

	/* Keep the timer running consistently despite processing time */
	if (device->pwrctrl.thermal_highlow) {
		mod_timer(&device->pwrctrl.thermal_timer,
					jiffies +
					device->pwrctrl.thermal_timeout);
		device->pwrctrl.thermal_highlow = 0;
	} else {
		mod_timer(&device->pwrctrl.thermal_timer,
					jiffies + (TH_HZ -
					device->pwrctrl.thermal_timeout));
		device->pwrctrl.thermal_highlow = 1;
	}
	/* Have work run in a non-interrupt context. */
	queue_work(device->work_queue, &device->pwrctrl.thermal_cycle_ws);
}

int kgsl_pwrctrl_init(struct kgsl_device *device)
{
	int i, k, m, set_bus = 1, n = 0, result = 0;
	unsigned int freq_i, rbbmtimer_freq;
	struct clk *clk;
	struct platform_device *pdev = device->pdev;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct kgsl_device_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct device_node *ocmem_bus_node;
	struct msm_bus_scale_pdata *ocmem_scale_table = NULL;
	struct device_node *gpubw_dev_node;
	struct platform_device *p2dev;

	/*acquire clocks */
	for (i = 0; i < KGSL_MAX_CLKS; i++) {
		if (pdata->clk_map & clks[i].map) {
			clk = clk_get(&pdev->dev, clks[i].name);
			if (IS_ERR(clk))
				goto clk_err;
			pwr->grp_clks[i] = clk;
		}
	}
	/* Make sure we have a source clk for freq setting */
	if (pwr->grp_clks[0] == NULL)
		pwr->grp_clks[0] = pwr->grp_clks[1];

	if (pdata->num_levels > KGSL_MAX_PWRLEVELS ||
	    pdata->num_levels < 1) {
		KGSL_PWR_ERR(device, "invalid power level count: %d\n",
					 pdata->num_levels);
		result = -EINVAL;
		goto done;
	}
	pwr->num_pwrlevels = pdata->num_levels;

	/* Initialize the user and thermal clock constraints */

	pwr->max_pwrlevel = 0;
	pwr->min_pwrlevel = pdata->num_levels - 2;
	pwr->thermal_pwrlevel = 0;

	pwr->active_pwrlevel = pdata->init_level;
	pwr->default_pwrlevel = pdata->init_level;
	pwr->init_pwrlevel = pdata->init_level;
	pwr->wakeup_maxpwrlevel = 0;
	for (i = 0; i < pdata->num_levels; i++) {
		pwr->pwrlevels[i].gpu_freq =
		(pdata->pwrlevel[i].gpu_freq > 0) ?
		clk_round_rate(pwr->grp_clks[0],
					   pdata->pwrlevel[i].
					   gpu_freq) : 0;
		pwr->pwrlevels[i].bus_freq =
			pdata->pwrlevel[i].bus_freq;
		pwr->pwrlevels[i].bus_min =
			pdata->pwrlevel[i].bus_min;
		pwr->pwrlevels[i].bus_max =
			pdata->pwrlevel[i].bus_max;
		/*
		 * If the bus min/max values are specified to be something
		 * other than defaults, do not attempt to generate them
		 * below.
		 */
		if (pwr->pwrlevels[i].bus_min != pwr->pwrlevels[i].bus_max)
			set_bus = 0;
	}
	/* Do not set_rate for targets in sync with AXI */
	if (pwr->pwrlevels[0].gpu_freq > 0) {
		clk_set_rate(pwr->grp_clks[0], pwr->
				pwrlevels[pwr->num_pwrlevels - 1].gpu_freq);
		rbbmtimer_freq = clk_round_rate(pwr->grp_clks[6],
						KGSL_RBBMTIMER_CLK_FREQ);
		clk_set_rate(pwr->grp_clks[6], rbbmtimer_freq);
	}

	pwr->gpu_reg = regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(pwr->gpu_reg))
		pwr->gpu_reg = NULL;

	if (pwr->gpu_reg) {
		pwr->gpu_cx = regulator_get(&pdev->dev, "vddcx");
		if (IS_ERR(pwr->gpu_cx))
			pwr->gpu_cx = NULL;
	} else
		pwr->gpu_cx = NULL;

	pwr->power_flags = 0;

	pwr->interval_timeout = pdata->idle_timeout;
	pwr->strtstp_sleepwake = pdata->strtstp_sleepwake;

	if (kgsl_property_read_u32(device, "qcom,pm-qos-active-latency",
		&pwr->pm_qos_active_latency))
		pwr->pm_qos_active_latency = 501;

	if (kgsl_property_read_u32(device, "qcom,pm-qos-wakeup-latency",
		&pwr->pm_qos_wakeup_latency))
		pwr->pm_qos_wakeup_latency = 101;

	pm_runtime_enable(&pdev->dev);

	if (pdata->bus_scale_table == NULL)
		return result;

	ocmem_bus_node = of_find_node_by_name(
				device->pdev->dev.of_node,
				"qcom,ocmem-bus-client");
	/* If platform has splitted ocmem bus client - use it */
	if (ocmem_bus_node) {
		ocmem_scale_table = msm_bus_pdata_from_node
				(device->pdev, ocmem_bus_node);
		if (ocmem_scale_table)
			pwr->pcl = msm_bus_scale_register_client
					(ocmem_scale_table);
	} else
		pwr->pcl = msm_bus_scale_register_client
					(pdata->bus_scale_table);

	if (!pwr->pcl) {
		KGSL_PWR_ERR(device,
				"msm_bus_scale_register_client failed: "
				"id %d table %p %p", device->id,
				pdata->bus_scale_table,
				ocmem_scale_table);
		result = -EINVAL;
		goto done;
	}

	/* Set if independent bus BW voting is supported */
	pwr->bus_control = pdata->bus_control;

	/* Check if gpu bandwidth vote device is defined in dts */
	gpubw_dev_node = of_parse_phandle(pdev->dev.of_node,
					"qcom,gpubw-dev", 0);
	if (gpubw_dev_node) {
		p2dev = of_find_device_by_node(gpubw_dev_node);
		if (p2dev)
			pwr->devbw = &p2dev->dev;
	}

	/*
	 * Set the range permitted for BIMC votes per-GPU frequency.
	 * For the moment assume the BIMC votes are listed in order
	 * per GPU frequency.  If this is no longer needed in the bus
	 * table the min/max values can be explicitly set in the dtsi
	 * file.
	 */
	freq_i = pwr->min_pwrlevel;
	pwr->pwrlevels[freq_i].bus_min = 1;

	/*
	 * Pull the BW vote out of the bus table.  They will be used to
	 * calculate the ratio between the votes.
	 */
	for (i = 0; i < pdata->bus_scale_table->num_usecases; i++) {
		struct msm_bus_paths *usecase =
				&pdata->bus_scale_table->usecase[i];
		struct msm_bus_vectors *vector = &usecase->vectors[0];
		if (vector->dst == MSM_BUS_SLAVE_EBI_CH0 &&
				vector->ib != 0) {

			if (i < KGSL_MAX_BUSLEVELS) {
				/* Convert bytes to Mbytes. */
				ib_votes[i] =
					DIV_ROUND_UP_ULL(vector->ib, 1048576)
					- 1;
				if (ib_votes[i] > ib_votes[max_vote_buslevel])
					max_vote_buslevel = i;
			}

			for (k = 0; k < n; k++)
				if (vector->ib == pwr->bus_ib[k]) {
					static uint64_t last_ib = 0xFFFFFFFF;
					/*
					 * if the bus min/max are already set
					 * leave them alone.
					 */
					if (set_bus == 0)
						break;
					if (vector->ib <= last_ib) {
						pwr->pwrlevels[freq_i--].
							bus_max = i - 1;
						pwr->pwrlevels[freq_i].
							bus_min = i;
					}
					last_ib = vector->ib;
					break;
				}
			/* if this is a new ib value, save it */
			if (k == n) {
				pwr->bus_ib[k] = vector->ib;
				n++;
				/* find which pwrlevels use this ib */
				for (m = 0; m < pwr->num_pwrlevels - 1; m++) {
					if (pdata->bus_scale_table->
						usecase[pwr->pwrlevels[m].
						bus_freq].vectors[0].ib
						== vector->ib)
						pwr->bus_index[m] = k;
				}
			}
		}
	}
	pwr->pwrlevels[0].bus_max = i - 1;

	INIT_WORK(&pwr->thermal_cycle_ws, kgsl_thermal_cycle);
	setup_timer(&pwr->thermal_timer, kgsl_thermal_timer,
			(unsigned long) device);

	devfreq_vbif_register_callback(kgsl_get_bw);

	return result;

clk_err:
	result = PTR_ERR(clk);
	KGSL_PWR_ERR(device, "clk_get(%s) failed: %d\n",
				 clks[i].name, result);

done:
	return result;
}

void kgsl_pwrctrl_close(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i;

	KGSL_PWR_INFO(device, "close device %d\n", device->id);

	pm_runtime_disable(&device->pdev->dev);

	if (pwr->pcl)
		msm_bus_scale_unregister_client(pwr->pcl);

	pwr->pcl = 0;

	if (pwr->gpu_reg) {
		regulator_put(pwr->gpu_reg);
		pwr->gpu_reg = NULL;
	}

	if (pwr->gpu_cx) {
		regulator_put(pwr->gpu_cx);
		pwr->gpu_cx = NULL;
	}

	for (i = 1; i < KGSL_MAX_CLKS; i++)
		if (pwr->grp_clks[i]) {
			clk_put(pwr->grp_clks[i]);
			pwr->grp_clks[i] = NULL;
		}

	pwr->grp_clks[0] = NULL;
	pwr->power_flags = 0;
}

/**
 * kgsl_idle_check() - Work function for GPU interrupts and idle timeouts.
 * @device: The device
 *
 * This function is called for work that is queued by the interrupt
 * handler or the idle timer. It attempts to transition to a clocks
 * off state if the active_cnt is 0 and the hardware is idle.
 */
void kgsl_idle_check(struct work_struct *work)
{
	struct kgsl_device *device = container_of(work, struct kgsl_device,
							idle_check_ws);
	WARN_ON(device == NULL);
	if (device == NULL)
		return;

	mutex_lock(&device->mutex);

	if (device->state == KGSL_STATE_ACTIVE
		   || device->state ==  KGSL_STATE_NAP) {

		if (!atomic_read(&device->active_cnt))
			kgsl_pwrctrl_change_state(device,
					device->requested_state);

		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
		if (device->state == KGSL_STATE_ACTIVE)
			mod_timer(&device->idle_timer,
					jiffies +
					device->pwrctrl.interval_timeout);
	}

	kgsl_pwrscale_update(device);
	mutex_unlock(&device->mutex);
}
EXPORT_SYMBOL(kgsl_idle_check);

void kgsl_timer(unsigned long data)
{
	struct kgsl_device *device = (struct kgsl_device *) data;

	KGSL_PWR_INFO(device, "idle timer expired device %d\n", device->id);
	if (device->requested_state != KGSL_STATE_SUSPEND) {
		if (device->pwrctrl.strtstp_sleepwake)
			kgsl_pwrctrl_request_state(device, KGSL_STATE_SLUMBER);
		else
			kgsl_pwrctrl_request_state(device, KGSL_STATE_SLEEP);
		/* Have work run in a non-interrupt context. */
		queue_work(device->work_queue, &device->idle_check_ws);
	}
}

static bool kgsl_pwrctrl_isenabled(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	return ((test_bit(KGSL_PWRFLAGS_CLK_ON, &pwr->power_flags) != 0) &&
		(test_bit(KGSL_PWRFLAGS_AXI_ON, &pwr->power_flags) != 0));
}

/**
 * kgsl_pre_hwaccess - Enforce preconditions for touching registers
 * @device: The device
 *
 * This function ensures that the correct lock is held and that the GPU
 * clock is on immediately before a register is read or written. Note
 * that this function does not check active_cnt because the registers
 * must be accessed during device start and stop, when the active_cnt
 * may legitimately be 0.
 */
void kgsl_pre_hwaccess(struct kgsl_device *device)
{
	/* In order to touch a register you must hold the device mutex...*/
	BUG_ON(!mutex_is_locked(&device->mutex));
	/* and have the clock on! */
	BUG_ON(!kgsl_pwrctrl_isenabled(device));
}
EXPORT_SYMBOL(kgsl_pre_hwaccess);

static int kgsl_pwrctrl_enable(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int level, status;

	if (pwr->wakeup_maxpwrlevel) {
		level = pwr->max_pwrlevel;
		pwr->wakeup_maxpwrlevel = 0;
	} else
		level = pwr->default_pwrlevel;

	kgsl_pwrctrl_pwrlevel_change(device, level);

	/* Order pwrrail/clk sequence based upon platform */
	status = kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_ON);
	if (status)
		return status;
	kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_ON, KGSL_STATE_ACTIVE);
	kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_ON);
	device->ftbl->regulator_enable(device);
	return status;
}

static void kgsl_pwrctrl_disable(struct kgsl_device *device)
{
	/* Order pwrrail/clk sequence based upon platform */
	device->ftbl->regulator_disable(device);
	kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_OFF);
	kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_OFF, KGSL_STATE_SLEEP);
	kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_OFF);
}

/**
 * _init() - Get the GPU ready to start, but don't turn anything on
 * @device - Pointer to the kgsl_device struct
 */
static int _init(struct kgsl_device *device)
{
	/* Suspend the pwrscale if it is currently enabled. */
	int status = 0;
	switch (device->state) {
	case KGSL_STATE_NAP:
	case KGSL_STATE_SLEEP:
		/* Force power on to do the stop */
		status = kgsl_pwrctrl_enable(device);
	case KGSL_STATE_ACTIVE:
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
		del_timer_sync(&device->idle_timer);
		device->ftbl->stop(device);
		/* fall through */
	case KGSL_STATE_AWARE:
		kgsl_pwrctrl_disable(device);
		/* fall through */
	case KGSL_STATE_SLUMBER:
	case KGSL_STATE_NONE:
		kgsl_pwrctrl_set_state(device, KGSL_STATE_INIT);
	}

	return status;
}

/**
 * _wake() - Power up the GPU from a slumber/sleep state
 * @device - Pointer to the kgsl_device struct
 *
 * Resume the GPU from a lower power state to ACTIVE.
 */
static int _wake(struct kgsl_device *device)
{
	int status = 0;

	switch (device->state) {
	case KGSL_STATE_SUSPEND:
		complete_all(&device->hwaccess_gate);
		/* Call the GPU specific resume function */
		device->ftbl->resume(device);
		/* fall through */
	case KGSL_STATE_SLUMBER:
		status = device->ftbl->start(device,
				device->pwrctrl.superfast);
		device->pwrctrl.superfast = false;

		if (status) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
			KGSL_DRV_ERR(device, "start failed %d\n", status);
			break;
		}
		/* fall through */
	case KGSL_STATE_SLEEP:
		kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_ON);
		kgsl_pwrscale_wake(device);
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_ON);
		/* fall through */
	case KGSL_STATE_NAP:
		/* Turn on the core clocks */
		kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_ON, KGSL_STATE_ACTIVE);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_ACTIVE);

		/*
		 * No need to turn on/off irq here as it no longer affects
		 * power collapse
		 */

		mod_timer(&device->idle_timer, jiffies +
				device->pwrctrl.interval_timeout);
		break;
	case KGSL_STATE_AWARE:
		/* Enable state before turning on irq */
		kgsl_pwrctrl_set_state(device, KGSL_STATE_ACTIVE);
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_ON);
		mod_timer(&device->idle_timer, jiffies +
				device->pwrctrl.interval_timeout);
		break;
	case KGSL_STATE_INIT:
		kgsl_pwrscale_wake(device);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_ACTIVE);
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
		break;
	default:
		KGSL_PWR_WARN(device, "unhandled state %s\n",
				kgsl_pwrstate_to_str(device->state));
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
		status = -EINVAL;
		break;
	}
	return status;
}

/*
 * _aware() - Put device into AWARE
 * @device: Device pointer
 *
 * The GPU should be available for register reads/writes and able
 * to communicate with the rest of the system.  However disable all
 * paths that allow a switch to an interrupt context (interrupts &
 * timers).
 * Return 0 on success else error code
 */
static int
_aware(struct kgsl_device *device)
{
	int status = 0;
	switch (device->state) {
	case KGSL_STATE_INIT:
		status = kgsl_pwrctrl_enable(device);
		break;
	/* The following 2 cases shouldn't occur, but don't panic. */
	case KGSL_STATE_NAP:
	case KGSL_STATE_SLEEP:
		status = _wake(device);
	case KGSL_STATE_ACTIVE:
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
		del_timer_sync(&device->idle_timer);
		break;
	case KGSL_STATE_SLUMBER:
		status = kgsl_pwrctrl_enable(device);
		break;
	default:
		status = -EINVAL;
	}
	if (status)
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
	else
		kgsl_pwrctrl_set_state(device, KGSL_STATE_AWARE);
	return status;
}

static int
_nap(struct kgsl_device *device)
{
	switch (device->state) {
	case KGSL_STATE_ACTIVE:
		if (!device->ftbl->is_hw_collapsible(device)) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
			return -EBUSY;
		}

		/*
		 * Read HW busy counters before going to NAP state.
		 * The data might be used by power scale governors
		 * independently of the HW activity. For example
		 * the simple-on-demand governor will get the latest
		 * busy_time data even if the gpu isn't active.
		*/
		kgsl_pwrscale_update_stats(device);

		kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_OFF, KGSL_STATE_NAP);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_NAP);
	case KGSL_STATE_SLEEP:
	case KGSL_STATE_SLUMBER:
		break;
	case KGSL_STATE_AWARE:
		KGSL_PWR_WARN(device,
			"transition AWARE -> NAP is not permitted\n");
	default:
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
		break;
	}
	return 0;
}

static int
_sleep(struct kgsl_device *device)
{
	switch (device->state) {
	case KGSL_STATE_ACTIVE:
		if (!device->ftbl->is_hw_collapsible(device)) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
			return -EBUSY;
		}
		/* fall through */
	case KGSL_STATE_NAP:
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
		kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_OFF);
		kgsl_pwrscale_sleep(device);
		kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_OFF, KGSL_STATE_SLEEP);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_SLEEP);
		pm_qos_update_request(&device->pwrctrl.pm_qos_req_dma,
					PM_QOS_DEFAULT_VALUE);
		break;
	case KGSL_STATE_SLUMBER:
		break;
	case KGSL_STATE_AWARE:
		KGSL_PWR_WARN(device,
			"transition AWARE -> SLEEP is not permitted\n");
	default:
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
		break;
	}

	return 0;
}

static int
_slumber(struct kgsl_device *device)
{
	int status = 0;
	switch (device->state) {
	case KGSL_STATE_ACTIVE:
		if (!device->ftbl->is_hw_collapsible(device)) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
			return -EBUSY;
		}
		/* fall through */
	case KGSL_STATE_NAP:
	case KGSL_STATE_SLEEP:
		del_timer_sync(&device->idle_timer);
		if (device->pwrctrl.thermal_cycle == CYCLE_ACTIVE) {
			device->pwrctrl.thermal_cycle = CYCLE_ENABLE;
			del_timer_sync(&device->pwrctrl.thermal_timer);
		}
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
		/* make sure power is on to stop the device*/
		status = kgsl_pwrctrl_enable(device);
		device->ftbl->suspend_context(device);
		device->ftbl->stop(device);
		kgsl_pwrctrl_disable(device);
		kgsl_pwrscale_sleep(device);
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_SLUMBER);
		pm_qos_update_request(&device->pwrctrl.pm_qos_req_dma,
						PM_QOS_DEFAULT_VALUE);
		break;
	case KGSL_STATE_SUSPEND:
		complete_all(&device->hwaccess_gate);
		device->ftbl->resume(device);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_SLUMBER);
		break;
	case KGSL_STATE_AWARE:
		KGSL_PWR_WARN(device,
			"transition AWARE -> SLUMBER is not permitted\n");
	default:
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
		break;

	}
	return status;
}

/*
 * _suspend() - Put device into suspend
 * @device: Device pointer
 *
 * Return 0 on success else error code
 */
int _suspend(struct kgsl_device *device)
{
	int ret = 0;

	if (KGSL_STATE_NONE == device->state)
		return ret;

	/* drain to prevent from more commands being submitted */
	device->ftbl->drain(device);
	/* wait for active count so device can be put in slumber */
	ret = kgsl_active_count_wait(device, 0);
	if (ret)
		goto err;

	ret = device->ftbl->idle(device);
	if (ret)
		goto err;

	ret = _slumber(device);
	if (ret)
		goto err;

	kgsl_pwrctrl_set_state(device, KGSL_STATE_SUSPEND);
	return ret;

err:
	device->ftbl->resume(device);
	KGSL_PWR_ERR(device, "device failed to SUSPEND %d\n", ret);
	return ret;
}

/*
 * kgsl_pwrctrl_change_state() changes the GPU state to the input
 * @device: Pointer to a KGSL device
 * @state: desired KGSL state
 *
 * Caller must hold the device mutex. If the requested state change
 * is valid, execute it.  Otherwise return an error code explaining
 * why the change has not taken place.  Also print an error if an
 * unexpected state change failure occurs.  For example, a change to
 * NAP may be rejected because the GPU is busy, this is not an error.
 * A change to SUSPEND should go through no matter what, so if it
 * fails an additional error message will be printed to dmesg.
 */
int kgsl_pwrctrl_change_state(struct kgsl_device *device, int state)
{
	int status = 0;
	if (device->state == state)
		return status;
	kgsl_pwrctrl_request_state(device, state);

	/* Work through the legal state transitions */
	switch (state) {
	case KGSL_STATE_INIT:
		status = _init(device);
		break;
	case KGSL_STATE_AWARE:
		status = _aware(device);
		break;
	case KGSL_STATE_ACTIVE:
		status = _wake(device);
		break;
	case KGSL_STATE_NAP:
		status = _nap(device);
		break;
	case KGSL_STATE_SLEEP:
		status = _sleep(device);
		break;
	case KGSL_STATE_SLUMBER:
		status = _slumber(device);
		break;
	case KGSL_STATE_SUSPEND:
		status = _suspend(device);
		break;
	default:
		KGSL_PWR_INFO(device, "bad state request 0x%x\n", state);
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
		status = -EINVAL;
		break;
	}
	return status;
}
EXPORT_SYMBOL(kgsl_pwrctrl_change_state);

static void kgsl_pwrctrl_set_state(struct kgsl_device *device,
				unsigned int state)
{
	trace_kgsl_pwr_set_state(device, state);
	device->state = state;
	device->requested_state = KGSL_STATE_NONE;
}

static void kgsl_pwrctrl_request_state(struct kgsl_device *device,
				unsigned int state)
{
	if (state != KGSL_STATE_NONE && state != device->requested_state)
		trace_kgsl_pwr_request_state(device, state);
	device->requested_state = state;
}

const char *kgsl_pwrstate_to_str(unsigned int state)
{
	switch (state) {
	case KGSL_STATE_NONE:
		return "NONE";
	case KGSL_STATE_INIT:
		return "INIT";
	case KGSL_STATE_AWARE:
		return "AWARE";
	case KGSL_STATE_ACTIVE:
		return "ACTIVE";
	case KGSL_STATE_NAP:
		return "NAP";
	case KGSL_STATE_SLEEP:
		return "SLEEP";
	case KGSL_STATE_SUSPEND:
		return "SUSPEND";
	case KGSL_STATE_SLUMBER:
		return "SLUMBER";
	default:
		break;
	}
	return "UNKNOWN";
}
EXPORT_SYMBOL(kgsl_pwrstate_to_str);


/**
 * kgsl_active_count_get() - Increase the device active count
 * @device: Pointer to a KGSL device
 *
 * Increase the active count for the KGSL device and turn on
 * clocks if this is the first reference. Code paths that need
 * to touch the hardware or wait for the hardware to complete
 * an operation must hold an active count reference until they
 * are finished. An error code will be returned if waking the
 * device fails. The device mutex must be held while *calling
 * this function.
 */
int kgsl_active_count_get(struct kgsl_device *device)
{
	int ret = 0;
	BUG_ON(!mutex_is_locked(&device->mutex));

	if ((atomic_read(&device->active_cnt) == 0) &&
		(device->state != KGSL_STATE_ACTIVE)) {
		mutex_unlock(&device->mutex);
		wait_for_completion(&device->hwaccess_gate);
		mutex_lock(&device->mutex);
		device->pwrctrl.superfast = true;
		ret = kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);
	}
	if (ret == 0)
		atomic_inc(&device->active_cnt);
	trace_kgsl_active_count(device,
		(unsigned long) __builtin_return_address(0));
	return ret;
}
EXPORT_SYMBOL(kgsl_active_count_get);

/**
 * kgsl_active_count_put() - Decrease the device active count
 * @device: Pointer to a KGSL device
 *
 * Decrease the active count for the KGSL device and turn off
 * clocks if there are no remaining references. This function will
 * transition the device to NAP if there are no other pending state
 * changes. It also completes the suspend gate.  The device mutex must
 * be held while calling this function.
 */
void kgsl_active_count_put(struct kgsl_device *device)
{
	BUG_ON(!mutex_is_locked(&device->mutex));
	BUG_ON(atomic_read(&device->active_cnt) == 0);

	if (atomic_dec_and_test(&device->active_cnt)) {
		if (device->state == KGSL_STATE_ACTIVE &&
			device->requested_state == KGSL_STATE_NONE) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NAP);
			queue_work(device->work_queue, &device->idle_check_ws);
		}

		mod_timer(&device->idle_timer,
			jiffies + device->pwrctrl.interval_timeout);
	}

	trace_kgsl_active_count(device,
		(unsigned long) __builtin_return_address(0));

	wake_up(&device->active_cnt_wq);
}
EXPORT_SYMBOL(kgsl_active_count_put);

static int _check_active_count(struct kgsl_device *device, int count)
{
	/* Return 0 if the active count is greater than the desired value */
	return atomic_read(&device->active_cnt) > count ? 0 : 1;
}

/**
 * kgsl_active_count_wait() - Wait for activity to finish.
 * @device: Pointer to a KGSL device
 * @count: Active count value to wait for
 *
 * Block until the active_cnt value hits the desired value
 */
int kgsl_active_count_wait(struct kgsl_device *device, int count)
{
	int result = 0;
	long wait_jiffies = HZ;

	BUG_ON(!mutex_is_locked(&device->mutex));

	while (atomic_read(&device->active_cnt) > count) {
		long ret;
		mutex_unlock(&device->mutex);
		ret = wait_event_timeout(device->active_cnt_wq,
			_check_active_count(device, count), wait_jiffies);
		mutex_lock(&device->mutex);
		result = ret == 0 ? -ETIMEDOUT : 0;
		if (!result)
			wait_jiffies = ret;
		else
			break;
	}

	return result;
}
EXPORT_SYMBOL(kgsl_active_count_wait);
