// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2010-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/clk/qcom.h>
#include <linux/interconnect.h>
#include <linux/of_device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/msm_kgsl.h>
#include <soc/qcom/dcvs.h>

#include "kgsl_device.h"
#include "kgsl_bus.h"
#include "kgsl_pwrscale.h"
#include "kgsl_sysfs.h"
#include "kgsl_trace.h"
#include "kgsl_util.h"

#define UPDATE_BUSY_VAL		1000000

#define KGSL_MAX_BUSLEVELS	20

/* Order deeply matters here because reasons. New entries go on the end */
static const char * const clocks[KGSL_MAX_CLKS] = {
	"src_clk",
	"core_clk",
	"iface_clk",
	"mem_clk",
	"mem_iface_clk",
	"alt_mem_iface_clk",
	"rbbmtimer_clk",
	"gtcu_clk",
	"gtbu_clk",
	"gtcu_iface_clk",
	"alwayson_clk",
	"isense_clk",
	"rbcpr_clk",
	"iref_clk",
	"gmu_clk",
	"ahb_clk",
	"smmu_vote",
	"apb_pclk",
	"hub_cx_int_clk",
};

static void kgsl_pwrctrl_clk(struct kgsl_device *device, bool state,
					int requested_state);
static int kgsl_pwrctrl_pwrrail(struct kgsl_device *device, bool state);
static int _isense_clk_set_rate(struct kgsl_pwrctrl *pwr, int level);
static int kgsl_pwrctrl_clk_set_rate(struct clk *grp_clk, unsigned int freq,
				const char *name);
static void _gpu_clk_prepare_enable(struct kgsl_device *device,
				struct clk *clk, const char *name);
static void _bimc_clk_prepare_enable(struct kgsl_device *device,
				struct clk *clk, const char *name);

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
	unsigned int min_pwrlevel = min_t(unsigned int,
					pwr->thermal_pwrlevel_floor,
					pwr->min_pwrlevel);

	/* Ensure that max/min pwrlevels are within thermal max/min limits */
	max_pwrlevel = min_t(unsigned int, max_pwrlevel,
					pwr->thermal_pwrlevel_floor);
	min_pwrlevel = max_t(unsigned int, min_pwrlevel,
					pwr->thermal_pwrlevel);

	switch (pwrc->type) {
	case KGSL_CONSTRAINT_PWRLEVEL: {
		switch (pwrc->sub_type) {
		case KGSL_CONSTRAINT_PWR_MAX:
			return max_pwrlevel;
		case KGSL_CONSTRAINT_PWR_MIN:
			return min_pwrlevel;
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
 * kgsl_pwrctrl_pwrlevel_change_settings() - Program h/w during powerlevel
 * transitions
 * @device: Pointer to the kgsl_device struct
 * @post: flag to check if the call is before/after the clk_rate change
 * @wake_up: flag to check if device is active or waking up
 */
static void kgsl_pwrctrl_pwrlevel_change_settings(struct kgsl_device *device,
			bool post)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	unsigned int old = pwr->previous_pwrlevel;
	unsigned int new = pwr->active_pwrlevel;

	if (device->state != KGSL_STATE_ACTIVE)
		return;
	if (old == new)
		return;

	device->ftbl->pwrlevel_change_settings(device, old, new, post);
}

/**
 * kgsl_pwrctrl_adjust_pwrlevel() - Adjust the power level if
 * required by thermal, max/min, constraints, etc
 * @device: Pointer to the kgsl_device struct
 * @new_level: Requested powerlevel, an index into the pwrlevel array
 */
unsigned int kgsl_pwrctrl_adjust_pwrlevel(struct kgsl_device *device,
				unsigned int new_level)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	unsigned int old_level = pwr->active_pwrlevel;
	bool reset = false;

	/* If a pwr constraint is expired, remove it */
	if ((pwr->constraint.type != KGSL_CONSTRAINT_NONE) &&
		(time_after(jiffies, pwr->constraint.expires))) {

		struct kgsl_context *context = kgsl_context_get(device,
				pwr->constraint.owner_id);

		/* We couldn't get a reference, clear the constraint */
		if (!context) {
			reset = true;
			goto done;
		}

		/*
		 * If the last timestamp that set the constraint has retired,
		 * clear the constraint
		 */
		if (kgsl_check_timestamp(device, context,
			pwr->constraint.owner_timestamp)) {
			reset = true;
			kgsl_context_put(context);
			goto done;
		}

		/*
		 * Increase the timeout to keep the constraint at least till
		 * the timestamp retires
		 */
		pwr->constraint.expires = jiffies +
			msecs_to_jiffies(device->pwrctrl.interval_timeout);

		kgsl_context_put(context);
	}

done:
	if (reset) {
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
	return _adjust_pwrlevel(pwr, new_level, &pwr->constraint);
}

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

	new_level = kgsl_pwrctrl_adjust_pwrlevel(device, new_level);

	if (new_level == old_level)
		return;

	kgsl_pwrscale_update_stats(device);

	/*
	 * Set the active and previous powerlevel first in case the clocks are
	 * off - if we don't do this then the pwrlevel change won't take effect
	 * when the clocks come back
	 */
	pwr->active_pwrlevel = new_level;
	pwr->previous_pwrlevel = old_level;

	/*
	 * If the bus is running faster than its default level and the GPU
	 * frequency is moving down keep the DDR at a relatively high level.
	 */
	if (pwr->bus_mod < 0 || new_level < old_level) {
		pwr->bus_mod = 0;
		pwr->bus_percent_ab = 0;
	}
	/*
	 * Update the bus before the GPU clock to prevent underrun during
	 * frequency increases.
	 */
	if (new_level < old_level)
		kgsl_bus_update(device, KGSL_BUS_VOTE_ON);

	pwrlevel = &pwr->pwrlevels[pwr->active_pwrlevel];
	/* Change register settings if any  BEFORE pwrlevel change*/
	kgsl_pwrctrl_pwrlevel_change_settings(device, 0);
	device->ftbl->gpu_clock_set(device, pwr->active_pwrlevel);
	_isense_clk_set_rate(pwr, pwr->active_pwrlevel);

	trace_kgsl_pwrlevel(device,
			pwr->active_pwrlevel, pwrlevel->gpu_freq,
			pwr->previous_pwrlevel,
			pwr->pwrlevels[old_level].gpu_freq);

	trace_gpu_frequency(pwrlevel->gpu_freq/1000, 0);

	/*  Update the bus after GPU clock decreases. */
	if (new_level > old_level)
		kgsl_bus_update(device, KGSL_BUS_VOTE_ON);

	/*
	 * Some targets do not support the bandwidth requirement of
	 * GPU at TURBO, for such targets we need to set GPU-BIMC
	 * interface clocks to TURBO directly whenever GPU runs at
	 * TURBO. The TURBO frequency of gfx-bimc need to be defined
	 * in target device tree.
	 */
	if (pwr->gpu_bimc_int_clk) {
		if (pwr->active_pwrlevel == 0 &&
				!pwr->gpu_bimc_interface_enabled) {
			kgsl_pwrctrl_clk_set_rate(pwr->gpu_bimc_int_clk,
					pwr->gpu_bimc_int_clk_freq,
					"bimc_gpu_clk");
			_bimc_clk_prepare_enable(device,
					pwr->gpu_bimc_int_clk,
					"bimc_gpu_clk");
			pwr->gpu_bimc_interface_enabled = true;
		} else if (pwr->previous_pwrlevel == 0
				&& pwr->gpu_bimc_interface_enabled) {
			clk_disable_unprepare(pwr->gpu_bimc_int_clk);
			pwr->gpu_bimc_interface_enabled = false;
		}
	}

	/* Change register settings if any AFTER pwrlevel change*/
	kgsl_pwrctrl_pwrlevel_change_settings(device, 1);
}

void kgsl_pwrctrl_set_constraint(struct kgsl_device *device,
			struct kgsl_pwr_constraint *pwrc, uint32_t id, u32 ts)
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
		pwrc_old->expires = jiffies +
			msecs_to_jiffies(device->pwrctrl.interval_timeout);
		pwrc_old->owner_timestamp = ts;
		kgsl_pwrctrl_pwrlevel_change(device, constraint);
		/* Trace the constraint being set by the driver */
		trace_kgsl_constraint(device, pwrc_old->type, constraint, 1);
	} else if ((pwrc_old->type == pwrc->type) &&
		(pwrc_old->hint.pwrlevel.level == constraint)) {
		pwrc_old->owner_id = id;
		pwrc_old->owner_timestamp = ts;
		pwrc_old->expires = jiffies +
			msecs_to_jiffies(device->pwrctrl.interval_timeout);
	}
}

static int kgsl_pwrctrl_set_thermal_limit(struct kgsl_device *device,
		u32 level)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret = -EINVAL;

	if (level >= pwr->num_pwrlevels)
		level = pwr->num_pwrlevels - 1;

	if (dev_pm_qos_request_active(&pwr->sysfs_thermal_req))
		ret = dev_pm_qos_update_request(&pwr->sysfs_thermal_req,
			(pwr->pwrlevels[level].gpu_freq / 1000));

	return (ret < 0) ? ret : 0;
}

static ssize_t thermal_pwrlevel_store(struct device *dev,
				struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	int ret;
	u32 level;

	ret = kstrtou32(buf, 0, &level);
	if (ret)
		return ret;

	ret = kgsl_pwrctrl_set_thermal_limit(device, level);
	if (ret)
		return ret;

	return count;
}

static ssize_t thermal_pwrlevel_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{

	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pwr->thermal_pwrlevel);
}

static ssize_t max_pwrlevel_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret;
	unsigned int level = 0;

	ret = kstrtou32(buf, 0, &level);
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

static ssize_t max_pwrlevel_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{

	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pwr->max_pwrlevel);
}

static void kgsl_pwrctrl_min_pwrlevel_set(struct kgsl_device *device,
					int level)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	mutex_lock(&device->mutex);

	if (level > pwr->min_render_pwrlevel)
		level = pwr->min_render_pwrlevel;

	/* You can't set a minimum power level lower than the maximum */
	if (level < pwr->max_pwrlevel)
		level = pwr->max_pwrlevel;

	pwr->min_pwrlevel = level;

	/* Update the current level using the new limit */
	kgsl_pwrctrl_pwrlevel_change(device, pwr->active_pwrlevel);

	mutex_unlock(&device->mutex);
}

static ssize_t min_pwrlevel_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	int ret;
	unsigned int level = 0;

	ret = kstrtou32(buf, 0, &level);
	if (ret)
		return ret;

	kgsl_pwrctrl_min_pwrlevel_set(device, level);

	return count;
}

static ssize_t min_pwrlevel_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	return scnprintf(buf, PAGE_SIZE, "%u\n", pwr->min_pwrlevel);
}

static ssize_t num_pwrlevels_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{

	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	return scnprintf(buf, PAGE_SIZE, "%d\n", pwr->num_pwrlevels);
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

static ssize_t max_gpuclk_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	u32 freq;
	int ret, level;

	ret = kstrtou32(buf, 0, &freq);
	if (ret)
		return ret;

	level = _get_nearest_pwrlevel(&device->pwrctrl, freq);
	if (level < 0)
		return level;

	/*
	 * You would think this would set max_pwrlevel but the legacy behavior
	 * is that it set thermal_pwrlevel instead so we don't want to mess with
	 * that.
	 */
	ret = kgsl_pwrctrl_set_thermal_limit(device, level);
	if (ret)
		return ret;

	return count;
}

static ssize_t max_gpuclk_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		device->pwrctrl.pwrlevels[pwr->thermal_pwrlevel].gpu_freq);
}

static ssize_t gpuclk_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	unsigned int val = 0;
	int ret, level;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);
	level = _get_nearest_pwrlevel(pwr, val);
	if (level >= 0)
		kgsl_pwrctrl_pwrlevel_change(device, (unsigned int) level);

	mutex_unlock(&device->mutex);
	return count;
}

static ssize_t gpuclk_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%ld\n",
		kgsl_pwrctrl_active_freq(&device->pwrctrl));
}

static ssize_t idle_timer_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int val = 0;
	struct kgsl_device *device = dev_get_drvdata(dev);
	int ret;

	ret = kstrtou32(buf, 0, &val);
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
	device->pwrctrl.interval_timeout = val;
	mutex_unlock(&device->mutex);

	return count;
}

static ssize_t idle_timer_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", device->pwrctrl.interval_timeout);
}

static ssize_t minbw_timer_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	u32 val;
	int ret;

	if (device->pwrctrl.ctrl_flags & BIT(KGSL_PWRFLAGS_NAP_OFF))
		return -EINVAL;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return ret;

	device->pwrctrl.minbw_timeout = val;
	return count;
}

static ssize_t minbw_timer_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n",
		device->pwrctrl.minbw_timeout);
}

static ssize_t gpubusy_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret;
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_clk_stats *stats = &device->pwrctrl.clk_stats;

	ret = scnprintf(buf, PAGE_SIZE, "%7d %7d\n",
			stats->busy_old, stats->total_old);
	if (!test_bit(KGSL_PWRFLAGS_AXI_ON, &device->pwrctrl.power_flags)) {
		stats->busy_old = 0;
		stats->total_old = 0;
	}
	return ret;
}

static ssize_t gpu_available_frequencies_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int index, num_chars = 0;

	for (index = 0; index < pwr->num_pwrlevels; index++) {
		num_chars += scnprintf(buf + num_chars,
			PAGE_SIZE - num_chars - 1,
			"%d ", pwr->pwrlevels[index].gpu_freq);
		/* One space for trailing null and another for the newline */
		if (num_chars >= PAGE_SIZE - 2)
			break;
	}
	buf[num_chars++] = '\n';
	return num_chars;
}

static ssize_t gpu_clock_stats_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int index, num_chars = 0;

	mutex_lock(&device->mutex);
	kgsl_pwrscale_update_stats(device);
	mutex_unlock(&device->mutex);
	for (index = 0; index < pwr->num_pwrlevels; index++)
		num_chars += scnprintf(buf + num_chars, PAGE_SIZE - num_chars,
			"%llu ", pwr->clock_times[index]);

	if (num_chars < PAGE_SIZE)
		buf[num_chars++] = '\n';

	return num_chars;
}

static ssize_t reset_count_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", device->reset_counter);
}

static void __force_on(struct kgsl_device *device, int flag, int on)
{
	if (on) {
		switch (flag) {
		case KGSL_PWRFLAGS_CLK_ON:
			/* make sure pwrrail is ON before enabling clocks */
			kgsl_pwrctrl_pwrrail(device, true);
			kgsl_pwrctrl_clk(device, true,
				KGSL_STATE_ACTIVE);
			break;
		case KGSL_PWRFLAGS_AXI_ON:
			kgsl_pwrctrl_axi(device, true);
			break;
		case KGSL_PWRFLAGS_POWER_ON:
			kgsl_pwrctrl_pwrrail(device, true);
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
	struct kgsl_device *device = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		test_bit(flag, &device->pwrctrl.ctrl_flags));
}

static ssize_t __force_on_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count,
					int flag)
{
	unsigned int val = 0;
	struct kgsl_device *device = dev_get_drvdata(dev);
	int ret;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);
	__force_on(device, flag, val);
	mutex_unlock(&device->mutex);

	return count;
}

static ssize_t force_clk_on_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return __force_on_show(dev, attr, buf, KGSL_PWRFLAGS_CLK_ON);
}

static ssize_t force_clk_on_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return __force_on_store(dev, attr, buf, count, KGSL_PWRFLAGS_CLK_ON);
}

static ssize_t force_bus_on_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return __force_on_show(dev, attr, buf, KGSL_PWRFLAGS_AXI_ON);
}

static ssize_t force_bus_on_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return __force_on_store(dev, attr, buf, count, KGSL_PWRFLAGS_AXI_ON);
}

static ssize_t force_rail_on_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return __force_on_show(dev, attr, buf, KGSL_PWRFLAGS_POWER_ON);
}

static ssize_t force_rail_on_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return __force_on_store(dev, attr, buf, count, KGSL_PWRFLAGS_POWER_ON);
}

static ssize_t force_no_nap_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return __force_on_show(dev, attr, buf, KGSL_PWRFLAGS_NAP_OFF);
}

static ssize_t force_no_nap_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	return __force_on_store(dev, attr, buf, count,
					KGSL_PWRFLAGS_NAP_OFF);
}

static ssize_t bus_split_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		device->pwrctrl.bus_control);
}

static ssize_t bus_split_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	unsigned int val = 0;
	struct kgsl_device *device = dev_get_drvdata(dev);
	int ret;

	ret = kstrtou32(buf, 0, &val);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);
	device->pwrctrl.bus_control = val ? true : false;
	mutex_unlock(&device->mutex);

	return count;
}

static ssize_t default_pwrlevel_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		device->pwrctrl.default_pwrlevel);
}

static ssize_t default_pwrlevel_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct kgsl_pwrscale *pwrscale = &device->pwrscale;
	int ret;
	unsigned int level = 0;

	ret = kstrtou32(buf, 0, &level);
	if (ret)
		return ret;

	if (level >= pwr->num_pwrlevels)
		return count;

	mutex_lock(&device->mutex);
	pwr->default_pwrlevel = level;
	pwrscale->gpu_profile.profile.initial_freq
			= pwr->pwrlevels[level].gpu_freq;

	mutex_unlock(&device->mutex);
	return count;
}

static ssize_t popp_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	/* POPP is deprecated, so return it as always disabled */
	return scnprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t _gpu_busy_show(struct kgsl_device *device,
					char *buf)
{
	int ret;
	struct kgsl_clk_stats *stats = &device->pwrctrl.clk_stats;
	unsigned int busy_percent = 0;

	if (stats->total_old != 0)
		busy_percent = (stats->busy_old * 100) / stats->total_old;

	ret = scnprintf(buf, PAGE_SIZE, "%d %%\n", busy_percent);

	/* Reset the stats if GPU is OFF */
	if (!test_bit(KGSL_PWRFLAGS_AXI_ON, &device->pwrctrl.power_flags)) {
		stats->busy_old = 0;
		stats->total_old = 0;
	}
	return ret;
}

static ssize_t gpu_busy_percentage_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return _gpu_busy_show(device, buf);
}

static ssize_t _min_clock_mhz_show(struct kgsl_device *device,
					char *buf)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			pwr->pwrlevels[pwr->min_pwrlevel].gpu_freq / 1000000);
}


static ssize_t min_clock_mhz_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return _min_clock_mhz_show(device, buf);
}

static ssize_t _min_clock_mhz_store(struct kgsl_device *device,
				const char *buf, size_t count)
{
	int level, ret;
	unsigned int freq;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	ret = kstrtou32(buf, 0, &freq);
	if (ret)
		return ret;

	freq *= 1000000;
	level = _get_nearest_pwrlevel(pwr, freq);

	if (level >= 0)
		kgsl_pwrctrl_min_pwrlevel_set(device, level);

	return count;
}

static ssize_t min_clock_mhz_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return _min_clock_mhz_store(device, buf, count);
}

static ssize_t _max_clock_mhz_show(struct kgsl_device *device, char *buf)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	return scnprintf(buf, PAGE_SIZE, "%d\n",
		pwr->pwrlevels[pwr->thermal_pwrlevel].gpu_freq / 1000000);
}

static ssize_t max_clock_mhz_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return _max_clock_mhz_show(device, buf);
}

static ssize_t _max_clock_mhz_store(struct kgsl_device *device,
				const char *buf, size_t count)
{
	u32 freq;
	int ret, level;

	ret = kstrtou32(buf, 0, &freq);
	if (ret)
		return ret;

	level = _get_nearest_pwrlevel(&device->pwrctrl, freq * 1000000);
	if (level < 0)
		return level;

	/*
	 * You would think this would set max_pwrlevel but the legacy behavior
	 * is that it set thermal_pwrlevel instead so we don't want to mess with
	 * that.
	 */
	ret = kgsl_pwrctrl_set_thermal_limit(device, level);
	if (ret)
		return ret;

	return count;
}

static ssize_t max_clock_mhz_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return _max_clock_mhz_store(device, buf, count);
}

static ssize_t _clock_mhz_show(struct kgsl_device *device, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%ld\n",
			kgsl_pwrctrl_active_freq(&device->pwrctrl) / 1000000);
}

static ssize_t clock_mhz_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return _clock_mhz_show(device, buf);
}

static ssize_t _freq_table_mhz_show(struct kgsl_device *device,
					char *buf)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int index, num_chars = 0;

	for (index = 0; index < pwr->num_pwrlevels; index++) {
		num_chars += scnprintf(buf + num_chars,
			PAGE_SIZE - num_chars - 1,
			"%d ", pwr->pwrlevels[index].gpu_freq / 1000000);
		/* One space for trailing null and another for the newline */
		if (num_chars >= PAGE_SIZE - 2)
			break;
	}

	buf[num_chars++] = '\n';

	return num_chars;
}

static ssize_t freq_table_mhz_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return _freq_table_mhz_show(device, buf);
}

static ssize_t _gpu_tmu_show(struct kgsl_device *device,
					char *buf)
{
	struct device *dev;
	struct thermal_zone_device *thermal_dev;
	int temperature = 0, max_temp = 0;
	const char *name;
	struct property *prop;

	dev = &device->pdev->dev;

	of_property_for_each_string(dev->of_node, "qcom,tzone-names", prop, name) {
		thermal_dev = thermal_zone_get_zone_by_name(name);
		if (IS_ERR(thermal_dev))
			continue;

		if (thermal_zone_get_temp(thermal_dev, &temperature))
			continue;

		max_temp = max(temperature, max_temp);
	}

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			max_temp);
}

static ssize_t temp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);

	return _gpu_tmu_show(device, buf);
}

static ssize_t pwrscale_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	int ret;
	unsigned int enable = 0;

	ret = kstrtou32(buf, 0, &enable);
	if (ret)
		return ret;

	mutex_lock(&device->mutex);

	if (enable)
		kgsl_pwrscale_enable(device);
	else
		kgsl_pwrscale_disable(device, false);

	mutex_unlock(&device->mutex);

	return count;
}

static ssize_t pwrscale_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct kgsl_device *device = dev_get_drvdata(dev);
	struct kgsl_pwrscale *psc = &device->pwrscale;

	return scnprintf(buf, PAGE_SIZE, "%u\n", psc->enabled);
}

static DEVICE_ATTR_RO(temp);
static DEVICE_ATTR_RW(gpuclk);
static DEVICE_ATTR_RW(max_gpuclk);
static DEVICE_ATTR_RW(idle_timer);
static DEVICE_ATTR_RW(minbw_timer);
static DEVICE_ATTR_RO(gpubusy);
static DEVICE_ATTR_RO(gpu_available_frequencies);
static DEVICE_ATTR_RO(gpu_clock_stats);
static DEVICE_ATTR_RW(max_pwrlevel);
static DEVICE_ATTR_RW(min_pwrlevel);
static DEVICE_ATTR_RW(thermal_pwrlevel);
static DEVICE_ATTR_RO(num_pwrlevels);
static DEVICE_ATTR_RO(reset_count);
static DEVICE_ATTR_RW(force_clk_on);
static DEVICE_ATTR_RW(force_bus_on);
static DEVICE_ATTR_RW(force_rail_on);
static DEVICE_ATTR_RW(bus_split);
static DEVICE_ATTR_RW(default_pwrlevel);
static DEVICE_ATTR_RO(popp);
static DEVICE_ATTR_RW(force_no_nap);
static DEVICE_ATTR_RO(gpu_busy_percentage);
static DEVICE_ATTR_RW(min_clock_mhz);
static DEVICE_ATTR_RW(max_clock_mhz);
static DEVICE_ATTR_RO(clock_mhz);
static DEVICE_ATTR_RO(freq_table_mhz);
static DEVICE_ATTR_RW(pwrscale);

static const struct attribute *pwrctrl_attr_list[] = {
	&dev_attr_gpuclk.attr,
	&dev_attr_max_gpuclk.attr,
	&dev_attr_idle_timer.attr,
	&dev_attr_minbw_timer.attr,
	&dev_attr_gpubusy.attr,
	&dev_attr_gpu_available_frequencies.attr,
	&dev_attr_gpu_clock_stats.attr,
	&dev_attr_max_pwrlevel.attr,
	&dev_attr_min_pwrlevel.attr,
	&dev_attr_thermal_pwrlevel.attr,
	&dev_attr_num_pwrlevels.attr,
	&dev_attr_reset_count.attr,
	&dev_attr_force_clk_on.attr,
	&dev_attr_force_bus_on.attr,
	&dev_attr_force_rail_on.attr,
	&dev_attr_force_no_nap.attr,
	&dev_attr_bus_split.attr,
	&dev_attr_default_pwrlevel.attr,
	&dev_attr_popp.attr,
	&dev_attr_gpu_busy_percentage.attr,
	&dev_attr_min_clock_mhz.attr,
	&dev_attr_max_clock_mhz.attr,
	&dev_attr_clock_mhz.attr,
	&dev_attr_freq_table_mhz.attr,
	&dev_attr_temp.attr,
	&dev_attr_pwrscale.attr,
	NULL,
};

static GPU_SYSFS_ATTR(gpu_busy, 0444, _gpu_busy_show, NULL);
static GPU_SYSFS_ATTR(gpu_min_clock, 0644, _min_clock_mhz_show,
		_min_clock_mhz_store);
static GPU_SYSFS_ATTR(gpu_max_clock, 0644, _max_clock_mhz_show,
		_max_clock_mhz_store);
static GPU_SYSFS_ATTR(gpu_clock, 0444, _clock_mhz_show, NULL);
static GPU_SYSFS_ATTR(gpu_freq_table, 0444, _freq_table_mhz_show, NULL);
static GPU_SYSFS_ATTR(gpu_tmu, 0444, _gpu_tmu_show, NULL);

static const struct attribute *gpu_sysfs_attr_list[] = {
	&gpu_sysfs_attr_gpu_busy.attr,
	&gpu_sysfs_attr_gpu_min_clock.attr,
	&gpu_sysfs_attr_gpu_max_clock.attr,
	&gpu_sysfs_attr_gpu_clock.attr,
	&gpu_sysfs_attr_gpu_freq_table.attr,
	&gpu_sysfs_attr_gpu_tmu.attr,
	NULL,
};

int kgsl_pwrctrl_init_sysfs(struct kgsl_device *device)
{
	int ret;

	ret = sysfs_create_files(&device->dev->kobj, pwrctrl_attr_list);
	if (ret)
		return ret;

	if (!device->gpu_sysfs_kobj.state_in_sysfs)
		return 0;

	return sysfs_create_files(&device->gpu_sysfs_kobj, gpu_sysfs_attr_list);
}

/*
 * Track the amount of time the gpu is on vs the total system time.
 * Regularly update the percentage of busy time displayed by sysfs.
 */
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

static void kgsl_pwrctrl_clk(struct kgsl_device *device, bool state,
					  int requested_state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i = 0;

	if (gmu_core_gpmu_isenabled(device))
		return;
	if (test_bit(KGSL_PWRFLAGS_CLK_ON, &pwr->ctrl_flags))
		return;

	if (!state) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_CLK_ON,
			&pwr->power_flags)) {
			trace_kgsl_clk(device, state,
					kgsl_pwrctrl_active_freq(pwr));
			/* Disable gpu-bimc-interface clocks */
			if (pwr->gpu_bimc_int_clk &&
					pwr->gpu_bimc_interface_enabled) {
				clk_disable_unprepare(pwr->gpu_bimc_int_clk);
				pwr->gpu_bimc_interface_enabled = false;
			}

			for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
				clk_disable(pwr->grp_clks[i]);
			/* High latency clock maintenance. */
			if ((pwr->pwrlevels[0].gpu_freq > 0) &&
				(requested_state != KGSL_STATE_NAP) &&
				(requested_state != KGSL_STATE_MINBW)) {
				for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
					clk_unprepare(pwr->grp_clks[i]);
				device->ftbl->gpu_clock_set(device,
						pwr->num_pwrlevels - 1);
				_isense_clk_set_rate(pwr,
					pwr->num_pwrlevels - 1);
			}

			/* Turn off the IOMMU clocks */
			kgsl_mmu_disable_clk(&device->mmu);
		} else if (requested_state == KGSL_STATE_SLUMBER) {
			/* High latency clock maintenance. */
			for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
				clk_unprepare(pwr->grp_clks[i]);
			if ((pwr->pwrlevels[0].gpu_freq > 0)) {
				device->ftbl->gpu_clock_set(device,
						pwr->num_pwrlevels - 1);
				_isense_clk_set_rate(pwr,
					pwr->num_pwrlevels - 1);
			}
		}
	} else {
		if (!test_and_set_bit(KGSL_PWRFLAGS_CLK_ON,
			&pwr->power_flags)) {
			trace_kgsl_clk(device, state,
					kgsl_pwrctrl_active_freq(pwr));
			/* High latency clock maintenance. */
			if ((device->state != KGSL_STATE_NAP) &&
				(device->state != KGSL_STATE_MINBW)) {
				if (pwr->pwrlevels[0].gpu_freq > 0) {
					device->ftbl->gpu_clock_set(device,
							pwr->active_pwrlevel);
					_isense_clk_set_rate(pwr,
						pwr->active_pwrlevel);
				}
			}

			for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
				_gpu_clk_prepare_enable(device,
						pwr->grp_clks[i], clocks[i]);

			/* Enable the gpu-bimc-interface clocks */
			if (pwr->gpu_bimc_int_clk) {
				if (pwr->active_pwrlevel == 0 &&
					!pwr->gpu_bimc_interface_enabled) {
					kgsl_pwrctrl_clk_set_rate(
						pwr->gpu_bimc_int_clk,
						pwr->gpu_bimc_int_clk_freq,
						"bimc_gpu_clk");
					_bimc_clk_prepare_enable(device,
						pwr->gpu_bimc_int_clk,
						"bimc_gpu_clk");
					pwr->gpu_bimc_interface_enabled = true;
				}
			}

			/* Turn on the IOMMU clocks */
			kgsl_mmu_enable_clk(&device->mmu);
		}

	}
}

int kgsl_pwrctrl_axi(struct kgsl_device *device, bool state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if (test_bit(KGSL_PWRFLAGS_AXI_ON, &pwr->ctrl_flags))
		return 0;

	if (!state) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_AXI_ON,
			&pwr->power_flags)) {
			trace_kgsl_bus(device, state);
			return kgsl_bus_update(device, KGSL_BUS_VOTE_OFF);
		}
	} else {
		if (!test_and_set_bit(KGSL_PWRFLAGS_AXI_ON,
			&pwr->power_flags)) {
			trace_kgsl_bus(device, state);
			return kgsl_bus_update(device, KGSL_BUS_VOTE_ON);
		}
	}

	return 0;
}

int kgsl_pwrctrl_enable_cx_gdsc(struct kgsl_device *device, struct regulator *regulator)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret;

	if (IS_ERR_OR_NULL(regulator))
		return 0;

	ret = wait_for_completion_timeout(&pwr->cx_gdsc_gate, msecs_to_jiffies(5000));
	if (!ret) {
		dev_err(device->dev, "GPU CX wait timeout. Dumping CX votes:\n");
		/* Dump the cx regulator consumer list */
		qcom_clk_dump(NULL, regulator, false);
	}

	ret = regulator_enable(regulator);
	if (ret)
		dev_err(device->dev, "Failed to enable CX regulator: %d\n", ret);

	pwr->cx_gdsc_wait = false;
	return ret;
}

static int kgsl_pwtctrl_enable_gx_gdsc(struct kgsl_device *device, struct regulator *regulator)
{
	int ret;

	if (IS_ERR_OR_NULL(regulator))
		return 0;

	ret = regulator_enable(regulator);
	if (ret)
		dev_err(device->dev, "Failed to enable GX regulator: %d\n", ret);
	return ret;
}

void kgsl_pwrctrl_disable_cx_gdsc(struct kgsl_device *device, struct regulator *regulator)
{
	if (IS_ERR_OR_NULL(regulator))
		return;

	reinit_completion(&device->pwrctrl.cx_gdsc_gate);
	device->pwrctrl.cx_gdsc_wait = true;
	regulator_disable(regulator);
}

static void kgsl_pwrctrl_disable_gx_gdsc(struct kgsl_device *device, struct regulator *regulator)
{
	if (IS_ERR_OR_NULL(regulator))
		return;

	if (!kgsl_regulator_disable_wait(regulator, 200))
		dev_err(device->dev, "Regulator vdd is stuck on\n");
}

static int enable_regulators(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret;

	if (test_and_set_bit(KGSL_PWRFLAGS_POWER_ON, &pwr->power_flags))
		return 0;

	ret = kgsl_pwrctrl_enable_cx_gdsc(device, pwr->cx_gdsc);
	if (!ret) {
		/* Set parent in retention voltage to power up vdd supply */
		ret = kgsl_regulator_set_voltage(device->dev,
				pwr->gx_gdsc_parent,
				pwr->gx_gdsc_parent_min_corner);
		if (!ret)
			ret = kgsl_pwtctrl_enable_gx_gdsc(device, pwr->gx_gdsc);
	}

	if (ret) {
		clear_bit(KGSL_PWRFLAGS_POWER_ON, &pwr->power_flags);
		return ret;
	}

	trace_kgsl_rail(device, KGSL_PWRFLAGS_POWER_ON);
	return 0;
}

static int kgsl_pwrctrl_pwrrail(struct kgsl_device *device, bool state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int status = 0;

	if (gmu_core_gpmu_isenabled(device))
		return 0;
	/*
	 * Disabling the regulator means also disabling dependent clocks.
	 * Hence don't disable it if force clock ON is set.
	 */
	if (test_bit(KGSL_PWRFLAGS_POWER_ON, &pwr->ctrl_flags) ||
		test_bit(KGSL_PWRFLAGS_CLK_ON, &pwr->ctrl_flags))
		return 0;

	if (!state) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_POWER_ON,
			&pwr->power_flags)) {
			trace_kgsl_rail(device, state);
			kgsl_pwrctrl_disable_gx_gdsc(device, pwr->gx_gdsc);
			kgsl_pwrctrl_disable_cx_gdsc(device, pwr->cx_gdsc);
		}
	} else
		status = enable_regulators(device);

	return status;
}

void kgsl_pwrctrl_irq(struct kgsl_device *device, bool state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if (state) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_IRQ_ON,
			&pwr->power_flags)) {
			trace_kgsl_irq(device, state);
			enable_irq(pwr->interrupt_num);
			if (device->freq_limiter_intr_num > 0)
				enable_irq(device->freq_limiter_intr_num);
		}
	} else {
		if (test_and_clear_bit(KGSL_PWRFLAGS_IRQ_ON,
			&pwr->power_flags)) {
			trace_kgsl_irq(device, state);
			if (device->freq_limiter_intr_num > 0)
				disable_irq(device->freq_limiter_intr_num);
			if (in_interrupt())
				disable_irq_nosync(pwr->interrupt_num);
			else
				disable_irq(pwr->interrupt_num);
		}
	}
}

static void kgsl_minbw_timer(struct timer_list *t)
{
	struct kgsl_pwrctrl *pwr = from_timer(pwr, t, minbw_timer);
	struct kgsl_device *device = container_of(pwr,
					struct kgsl_device, pwrctrl);

	if (device->state == KGSL_STATE_NAP) {
		kgsl_pwrctrl_request_state(device, KGSL_STATE_MINBW);
		kgsl_schedule_work(&device->idle_check_ws);
	}
}

static int _get_clocks(struct kgsl_device *device)
{
	struct device *dev = &device->pdev->dev;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	const char *name;
	struct property *prop;

	pwr->isense_clk_indx = 0;
	of_property_for_each_string(dev->of_node, "clock-names", prop, name) {
		int i;

		for (i = 0; i < KGSL_MAX_CLKS; i++) {
			if (pwr->grp_clks[i] || strcmp(clocks[i], name))
				continue;
			/* apb_pclk should only be enabled if QCOM_KGSL_QDSS_STM is enabled */
			if (!strcmp(name, "apb_pclk") && !IS_ENABLED(CONFIG_QCOM_KGSL_QDSS_STM))
				continue;

			pwr->grp_clks[i] = devm_clk_get(dev, name);

			if (IS_ERR(pwr->grp_clks[i])) {
				int ret = PTR_ERR(pwr->grp_clks[i]);

				dev_err(dev, "Couldn't get clock: %s (%d)\n",
					name, ret);
				pwr->grp_clks[i] = NULL;
				return ret;
			}

			if (!strcmp(name, "isense_clk"))
				pwr->isense_clk_indx = i;
			break;
		}
	}

	if (pwr->isense_clk_indx && of_property_read_u32(dev->of_node,
		"qcom,isense-clk-on-level", &pwr->isense_clk_on_level)) {
		dev_err(dev, "Couldn't get isense clock on level\n");
		return -ENXIO;
	}
	return 0;
}

static int _isense_clk_set_rate(struct kgsl_pwrctrl *pwr, int level)
{
	int rate;

	if (!pwr->isense_clk_indx)
		return -EINVAL;

	rate = clk_round_rate(pwr->grp_clks[pwr->isense_clk_indx],
		level > pwr->isense_clk_on_level ?
		KGSL_XO_CLK_FREQ : KGSL_ISENSE_CLK_FREQ);
	return kgsl_pwrctrl_clk_set_rate(pwr->grp_clks[pwr->isense_clk_indx],
			rate, clocks[pwr->isense_clk_indx]);
}

/*
 * _gpu_clk_prepare_enable - Enable the specified GPU clock
 * Try once to enable it and then BUG() for debug
 */
static void _gpu_clk_prepare_enable(struct kgsl_device *device,
		struct clk *clk, const char *name)
{
	int ret;

	if (kgsl_state_is_nap_or_minbw(device)) {
		ret = clk_enable(clk);
		if (ret)
			goto err;
		return;
	}

	ret = clk_prepare_enable(clk);
	if (!ret)
		return;
err:
	/* Failure is fatal so BUG() to facilitate debug */
	dev_err(device->dev, "GPU Clock %s enable error:%d\n", name, ret);
}

/*
 * _bimc_clk_prepare_enable - Enable the specified GPU clock
 *  Try once to enable it and then BUG() for debug
 */
static void _bimc_clk_prepare_enable(struct kgsl_device *device,
		struct clk *clk, const char *name)
{
	int ret = clk_prepare_enable(clk);
	/* Failure is fatal so BUG() to facilitate debug */
	if (ret)
		dev_err(device->dev, "GPU clock %s enable error:%d\n",
				name, ret);
}

static int kgsl_pwrctrl_clk_set_rate(struct clk *grp_clk, unsigned int freq,
		const char *name)
{
	int ret = clk_set_rate(grp_clk, freq);

	WARN(ret, "%s set freq %d failed:%d\n", name, freq, ret);
	return ret;
}

int kgsl_pwrctrl_init(struct kgsl_device *device)
{
	int i, result, freq;
	struct platform_device *pdev = device->pdev;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	result = _get_clocks(device);
	if (result)
		return result;

	/* Make sure we have a source clk for freq setting */
	if (pwr->grp_clks[0] == NULL)
		pwr->grp_clks[0] = pwr->grp_clks[1];

	/* Getting gfx-bimc-interface-clk frequency */
	if (!of_property_read_u32(pdev->dev.of_node,
			"qcom,gpu-bimc-interface-clk-freq",
			&pwr->gpu_bimc_int_clk_freq))
		pwr->gpu_bimc_int_clk = devm_clk_get(&pdev->dev,
					"bimc_gpu_clk");

	if (of_property_read_bool(pdev->dev.of_node, "qcom,no-nap"))
		device->pwrctrl.ctrl_flags |= BIT(KGSL_PWRFLAGS_NAP_OFF);
	else if (!IS_ENABLED(CONFIG_COMMON_CLK_QCOM)) {
		dev_warn(device->dev, "KGSL nap state is not supported\n");
		device->pwrctrl.ctrl_flags |= BIT(KGSL_PWRFLAGS_NAP_OFF);
	}

	if (pwr->num_pwrlevels == 0) {
		dev_err(device->dev, "No power levels are defined\n");
		return -EINVAL;
	}

	init_waitqueue_head(&device->active_cnt_wq);

	/* Initialize the thermal clock constraints */
	pwr->thermal_pwrlevel = 0;
	pwr->thermal_pwrlevel_floor = pwr->num_pwrlevels - 1;

	result = dev_pm_qos_add_request(&pdev->dev, &pwr->sysfs_thermal_req,
			DEV_PM_QOS_MAX_FREQUENCY,
			PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
	if (result < 0)
		dev_err(device->dev, "PM QoS thermal request failed:\n", result);

	for (i = 0; i < pwr->num_pwrlevels; i++) {
		freq = pwr->pwrlevels[i].gpu_freq;

		if (freq > 0)
			freq = clk_round_rate(pwr->grp_clks[0], freq);

		if (freq >= pwr->pwrlevels[i].gpu_freq)
			pwr->pwrlevels[i].gpu_freq = freq;
	}

	clk_set_rate(pwr->grp_clks[0],
		pwr->pwrlevels[pwr->num_pwrlevels - 1].gpu_freq);

	freq = clk_round_rate(pwr->grp_clks[6], KGSL_XO_CLK_FREQ);
	if (freq > 0)
		kgsl_pwrctrl_clk_set_rate(pwr->grp_clks[6],
			freq, clocks[6]);

	_isense_clk_set_rate(pwr, pwr->num_pwrlevels - 1);

	if (of_property_read_bool(pdev->dev.of_node, "vddcx-supply"))
		pwr->cx_gdsc = devm_regulator_get(&pdev->dev, "vddcx");

	if (of_property_read_bool(pdev->dev.of_node, "vdd-supply"))
		pwr->gx_gdsc = devm_regulator_get(&pdev->dev, "vdd");

	if (of_property_read_bool(pdev->dev.of_node, "vdd-parent-supply")) {
		pwr->gx_gdsc_parent = devm_regulator_get(&pdev->dev,
				"vdd-parent");
		if (IS_ERR(pwr->gx_gdsc_parent)) {
			dev_err(device->dev,
				"Failed to get vdd-parent regulator:%ld\n",
				PTR_ERR(pwr->gx_gdsc_parent));
			return -ENODEV;
		}
		if (of_property_read_u32(pdev->dev.of_node,
					"vdd-parent-min-corner",
					&pwr->gx_gdsc_parent_min_corner)) {
			dev_err(device->dev,
				"vdd-parent-min-corner not found\n");
			return -ENODEV;
		}
	}

	init_completion(&pwr->cx_gdsc_gate);
	complete_all(&pwr->cx_gdsc_gate);

	result = device->ftbl->register_gdsc_notifier(device);
	if (result) {
		dev_err(&pdev->dev, "Failed to register gdsc notifier: %d\n", result);
		return result;
	}

	pwr->power_flags = 0;

	pm_runtime_enable(&pdev->dev);

	timer_setup(&pwr->minbw_timer, kgsl_minbw_timer, 0);

	return 0;
}

void kgsl_pwrctrl_close(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	pwr->power_flags = 0;

	if (dev_pm_qos_request_active(&pwr->sysfs_thermal_req))
		dev_pm_qos_remove_request(&pwr->sysfs_thermal_req);

	pm_runtime_disable(&device->pdev->dev);
}

void kgsl_idle_check(struct work_struct *work)
{
	struct kgsl_device *device = container_of(work, struct kgsl_device,
							idle_check_ws);
	int ret = 0;
	unsigned int requested_state;

	mutex_lock(&device->mutex);

	/*
	 * After scheduling idle work for transitioning to either NAP or
	 * SLUMBER, it's possible that requested state can change to NONE
	 * if any new workload comes before kgsl_idle_check is executed or
	 * it gets the device mutex. In such case, no need to change state
	 * to NONE.
	 */
	if (device->requested_state == KGSL_STATE_NONE) {
		mutex_unlock(&device->mutex);
		return;
	}

	requested_state = device->requested_state;

	if (device->state == KGSL_STATE_ACTIVE
		   || kgsl_state_is_nap_or_minbw(device)) {

		if (!atomic_read(&device->active_cnt)) {
			spin_lock(&device->submit_lock);
			if (device->submit_now) {
				spin_unlock(&device->submit_lock);
				goto done;
			}
			/* Don't allow GPU inline submission in SLUMBER */
			if (requested_state == KGSL_STATE_SLUMBER)
				device->slumber = true;
			spin_unlock(&device->submit_lock);

			ret = kgsl_pwrctrl_change_state(device,
					device->requested_state);
			if (ret == -EBUSY) {
				if (requested_state == KGSL_STATE_SLUMBER) {
					spin_lock(&device->submit_lock);
					device->slumber = false;
					spin_unlock(&device->submit_lock);
				}
				/*
				 * If the GPU is currently busy, restore
				 * the requested state and reschedule
				 * idle work.
				 */
				kgsl_pwrctrl_request_state(device,
					requested_state);
				kgsl_schedule_work(&device->idle_check_ws);
			}
		}
done:
		if (!ret)
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);

		if (device->state == KGSL_STATE_ACTIVE)
			kgsl_start_idle_timer(device);
	}

	if (device->state != KGSL_STATE_MINBW)
		kgsl_pwrscale_update(device);
	mutex_unlock(&device->mutex);
}

void kgsl_timer(struct timer_list *t)
{
	struct kgsl_device *device = from_timer(device, t, idle_timer);

	if (device->requested_state != KGSL_STATE_SUSPEND) {
		kgsl_pwrctrl_request_state(device, KGSL_STATE_SLUMBER);
		/* Have work run in a non-interrupt context. */
		kgsl_schedule_work(&device->idle_check_ws);
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
	/* In order to touch a register you must hold the device mutex */
	WARN_ON(!mutex_is_locked(&device->mutex));

	/*
	 * A register access without device power will cause a fatal timeout.
	 * This is not valid for targets with a GMU.
	 */
	if (!gmu_core_gpmu_isenabled(device))
		WARN_ON(!kgsl_pwrctrl_isenabled(device));
}

static int kgsl_pwrctrl_enable(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int level, status;

	level = pwr->default_pwrlevel;

	kgsl_pwrctrl_pwrlevel_change(device, level);

	/* Order pwrrail/clk sequence based upon platform */
	status = kgsl_pwrctrl_pwrrail(device, true);
	if (status)
		return status;
	kgsl_pwrctrl_clk(device, true, KGSL_STATE_ACTIVE);
	kgsl_pwrctrl_axi(device, true);

	return device->ftbl->regulator_enable(device);
}

void kgsl_pwrctrl_clear_l3_vote(struct kgsl_device *device)
{
	int status;
	struct dcvs_freq freq = {0};

	if (!device->num_l3_pwrlevels)
		return;

	freq.hw_type = DCVS_L3;

	status = qcom_dcvs_update_votes(KGSL_L3_DEVICE, &freq, 1,
			DCVS_SLOW_PATH);
	if (!status)
		device->cur_l3_pwrlevel = 0;
	else
		dev_err(device->dev, "Could not clear l3_vote: %d\n",
			     status);
}

static void kgsl_pwrctrl_disable(struct kgsl_device *device)
{
	kgsl_pwrctrl_clear_l3_vote(device);

	/* Order pwrrail/clk sequence based upon platform */
	device->ftbl->regulator_disable(device);
	kgsl_pwrctrl_axi(device, false);
	kgsl_pwrctrl_clk(device, false, KGSL_STATE_SLUMBER);
	kgsl_pwrctrl_pwrrail(device, false);
}

static void
kgsl_pwrctrl_clk_set_options(struct kgsl_device *device, bool on)
{
	int i;

	for (i = 0; i < KGSL_MAX_CLKS; i++)
		device->ftbl->clk_set_options(device, clocks[i],
			device->pwrctrl.grp_clks[i], on);
}

/**
 * _init() - Get the GPU ready to start, but don't turn anything on
 * @device - Pointer to the kgsl_device struct
 */
static int _init(struct kgsl_device *device)
{
	int status = 0;

	switch (device->state) {
	case KGSL_STATE_MINBW:
		fallthrough;
	case KGSL_STATE_NAP:
		del_timer_sync(&device->pwrctrl.minbw_timer);
		/* Force power on to do the stop */
		status = kgsl_pwrctrl_enable(device);
		fallthrough;
	case KGSL_STATE_ACTIVE:
		kgsl_pwrctrl_irq(device, false);
		del_timer_sync(&device->idle_timer);
		device->ftbl->stop(device);
		fallthrough;
	case KGSL_STATE_AWARE:
		kgsl_pwrctrl_disable(device);
		fallthrough;
	case KGSL_STATE_SLUMBER:
		fallthrough;
	case KGSL_STATE_NONE:
		kgsl_pwrctrl_set_state(device, KGSL_STATE_INIT);
	}

	return status;
}

/**
 * _wake() - Power up the GPU from a slumber state
 * @device - Pointer to the kgsl_device struct
 *
 * Resume the GPU from a lower power state to ACTIVE.
 */
static int _wake(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int status = 0;

	switch (device->state) {
	case KGSL_STATE_SUSPEND:
		complete_all(&device->hwaccess_gate);
		/* Call the GPU specific resume function */
		device->ftbl->resume(device);
		fallthrough;
	case KGSL_STATE_SLUMBER:
		kgsl_pwrctrl_clk_set_options(device, true);
		status = device->ftbl->start(device,
				device->pwrctrl.superfast);
		device->pwrctrl.superfast = false;

		if (status) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
			dev_err(device->dev, "start failed %d\n", status);
			break;
		}
		kgsl_pwrctrl_axi(device, true);
		kgsl_pwrscale_wake(device);
		kgsl_pwrctrl_irq(device, true);
		trace_gpu_frequency(
			pwr->pwrlevels[pwr->active_pwrlevel].gpu_freq/1000, 0);
		fallthrough;
	case KGSL_STATE_MINBW:
		kgsl_bus_update(device, KGSL_BUS_VOTE_ON);
		fallthrough;
	case KGSL_STATE_NAP:
		/* Turn on the core clocks */
		kgsl_pwrctrl_clk(device, true, KGSL_STATE_ACTIVE);

		device->ftbl->deassert_gbif_halt(device);
		pwr->last_stat_updated = ktime_get();
		/*
		 * No need to turn on/off irq here as it no longer affects
		 * power collapse
		 */
		kgsl_pwrctrl_set_state(device, KGSL_STATE_ACTIVE);

		/*
		 * Change register settings if any after pwrlevel change.
		 * If there was dcvs level change during nap - call
		 * pre and post in the row after clock is enabled.
		 */
		kgsl_pwrctrl_pwrlevel_change_settings(device, 0);
		kgsl_pwrctrl_pwrlevel_change_settings(device, 1);
		/* All settings for power level transitions are complete*/
		pwr->previous_pwrlevel = pwr->active_pwrlevel;
		kgsl_start_idle_timer(device);
		del_timer_sync(&device->pwrctrl.minbw_timer);
		break;
	case KGSL_STATE_AWARE:
		kgsl_pwrctrl_clk_set_options(device, true);
		/* Enable state before turning on irq */
		kgsl_pwrctrl_set_state(device, KGSL_STATE_ACTIVE);
		kgsl_pwrctrl_irq(device, true);
		kgsl_start_idle_timer(device);
		del_timer_sync(&device->pwrctrl.minbw_timer);
		break;
	default:
		dev_warn(device->dev, "unhandled state %s\n",
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
	/* The following 4 cases shouldn't occur, but don't panic. */
	case KGSL_STATE_MINBW:
		fallthrough;
	case KGSL_STATE_NAP:
		status = _wake(device);
		fallthrough;
	case KGSL_STATE_ACTIVE:
		kgsl_pwrctrl_irq(device, false);
		del_timer_sync(&device->idle_timer);
		break;
	case KGSL_STATE_SLUMBER:
		status = kgsl_pwrctrl_enable(device);
		break;
	default:
		status = -EINVAL;
	}

	if (!status)
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

		mod_timer(&device->pwrctrl.minbw_timer, jiffies +
			msecs_to_jiffies(device->pwrctrl.minbw_timeout));

		kgsl_pwrctrl_clk(device, false, KGSL_STATE_NAP);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_NAP);
		fallthrough;
	case KGSL_STATE_SLUMBER:
		break;
	case KGSL_STATE_AWARE:
		dev_warn(device->dev,
			"transition AWARE -> NAP is not permitted\n");
		fallthrough;
	default:
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
		break;
	}
	return 0;
}

static int
_minbw(struct kgsl_device *device)
{
	switch (device->state) {
		/*
		 * Device is expected to be clock gated to move to
		 * a deeper low power state. No other transition is
		 * permitted
		 */
	case KGSL_STATE_NAP:
		kgsl_bus_update(device, KGSL_BUS_VOTE_MINIMUM);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_MINBW);
		break;
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
		fallthrough;
	case KGSL_STATE_NAP:
		fallthrough;
	case KGSL_STATE_MINBW:
		del_timer_sync(&device->pwrctrl.minbw_timer);
		del_timer_sync(&device->idle_timer);
		kgsl_pwrctrl_irq(device, false);
		/* make sure power is on to stop the device*/
		status = kgsl_pwrctrl_enable(device);
		device->ftbl->suspend_context(device);
		device->ftbl->stop(device);
		kgsl_pwrctrl_clk_set_options(device, false);
		kgsl_pwrctrl_disable(device);
		kgsl_pwrscale_sleep(device);
		trace_gpu_frequency(0, 0);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_SLUMBER);
		break;
	case KGSL_STATE_SUSPEND:
		complete_all(&device->hwaccess_gate);
		device->ftbl->resume(device);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_SLUMBER);
		break;
	case KGSL_STATE_AWARE:
		kgsl_pwrctrl_disable(device);
		trace_gpu_frequency(0, 0);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_SLUMBER);
		break;
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
static int _suspend(struct kgsl_device *device)
{
	int ret = 0;

	if ((device->state == KGSL_STATE_NONE) ||
			(device->state == KGSL_STATE_INIT) ||
			(device->state == KGSL_STATE_SUSPEND))
		return ret;

	/*
	 * drain to prevent from more commands being submitted
	 * and wait for it to go idle
	 */
	ret = device->ftbl->drain_and_idle(device);
	if (ret)
		goto err;

	ret = _slumber(device);
	if (ret)
		goto err;

	kgsl_pwrctrl_set_state(device, KGSL_STATE_SUSPEND);
	return ret;

err:
	device->ftbl->resume(device);
	dev_err(device->dev, "device failed to SUSPEND %d\n", ret);
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
	case KGSL_STATE_MINBW:
		status = _minbw(device);
		break;
	case KGSL_STATE_SLUMBER:
		status = _slumber(device);
		break;
	case KGSL_STATE_SUSPEND:
		status = _suspend(device);
		break;
	default:
		dev_err(device->dev, "bad state request 0x%x\n", state);
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
		status = -EINVAL;
		break;
	}

	return status;
}

void kgsl_pwrctrl_set_state(struct kgsl_device *device,
				unsigned int state)
{
	trace_kgsl_pwr_set_state(device, state);
	device->state = state;
	device->requested_state = KGSL_STATE_NONE;

	spin_lock(&device->submit_lock);
	if (state == KGSL_STATE_SLUMBER || state == KGSL_STATE_SUSPEND)
		device->slumber = true;
	else
		device->slumber = false;
	spin_unlock(&device->submit_lock);
}

void kgsl_pwrctrl_request_state(struct kgsl_device *device,
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
	case KGSL_STATE_MINBW:
		return "MINBW";
	case KGSL_STATE_SUSPEND:
		return "SUSPEND";
	case KGSL_STATE_SLUMBER:
		return "SLUMBER";
	default:
		break;
	}
	return "UNKNOWN";
}

static int _check_active_count(struct kgsl_device *device, int count)
{
	/* Return 0 if the active count is greater than the desired value */
	return atomic_read(&device->active_cnt) > count ? 0 : 1;
}

int kgsl_active_count_wait(struct kgsl_device *device, int count,
	unsigned long wait_jiffies)
{
	int result = 0;

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return -EINVAL;

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

/**
 * kgsl_pwrctrl_set_default_gpu_pwrlevel() - Set GPU to default power level
 * @device: Pointer to the kgsl_device struct
 */
int kgsl_pwrctrl_set_default_gpu_pwrlevel(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	unsigned int new_level = pwr->default_pwrlevel;
	unsigned int old_level = pwr->active_pwrlevel;

	/*
	 * Update the level according to any thermal,
	 * max/min, or power constraints.
	 */
	new_level = kgsl_pwrctrl_adjust_pwrlevel(device, new_level);

	pwr->active_pwrlevel = new_level;
	pwr->previous_pwrlevel = old_level;

	/* Request adjusted DCVS level */
	return device->ftbl->gpu_clock_set(device, pwr->active_pwrlevel);
}

int kgsl_gpu_num_freqs(void)
{
	struct kgsl_device *device = kgsl_get_device(0);

	if (!device)
		return -ENODEV;

	return device->pwrctrl.num_pwrlevels;
}
EXPORT_SYMBOL(kgsl_gpu_num_freqs);

int kgsl_gpu_stat(struct kgsl_gpu_freq_stat *stats, u32 numfreq)
{
	struct kgsl_device *device = kgsl_get_device(0);
	struct kgsl_pwrctrl *pwr;
	int i;

	if (!device)
		return -ENODEV;

	pwr = &device->pwrctrl;

	if (!stats || (numfreq < pwr->num_pwrlevels))
		return -EINVAL;

	mutex_lock(&device->mutex);
	kgsl_pwrscale_update_stats(device);

	for (i = 0; i < pwr->num_pwrlevels; i++) {
		stats[i].freq = pwr->pwrlevels[i].gpu_freq;
		stats[i].active_time = pwr->clock_times[i];
		stats[i].idle_time = pwr->time_in_pwrlevel[i] - pwr->clock_times[i];
	}
	mutex_unlock(&device->mutex);

	return 0;
}
EXPORT_SYMBOL(kgsl_gpu_stat);
