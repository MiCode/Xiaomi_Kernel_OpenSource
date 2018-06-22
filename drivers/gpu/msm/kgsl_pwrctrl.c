/* Copyright (c) 2010-2018, The Linux Foundation. All rights reserved.
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
#include <linux/thermal.h>

#include "kgsl.h"
#include "kgsl_pwrscale.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"
#include "kgsl_gmu_core.h"

#define KGSL_PWRFLAGS_POWER_ON 0
#define KGSL_PWRFLAGS_CLK_ON   1
#define KGSL_PWRFLAGS_AXI_ON   2
#define KGSL_PWRFLAGS_IRQ_ON   3
#define KGSL_PWRFLAGS_NAP_OFF  5

#define UPDATE_BUSY_VAL		1000000

/* Number of jiffies for a full thermal cycle */
#define TH_HZ			(HZ/5)

#define KGSL_MAX_BUSLEVELS	20

#define DEFAULT_BUS_P 25

/* Order deeply matters here because reasons. New entries go on the end */
static const char * const clocks[] = {
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
	"ahb_clk"
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
static int _isense_clk_set_rate(struct kgsl_pwrctrl *pwr, int level);
static int kgsl_pwrctrl_clk_set_rate(struct clk *grp_clk, unsigned int freq,
				const char *name);
static void _gpu_clk_prepare_enable(struct kgsl_device *device,
				struct clk *clk, const char *name);
static void _bimc_clk_prepare_enable(struct kgsl_device *device,
				struct clk *clk, const char *name);

/**
 * _record_pwrevent() - Record the history of the new event
 * @device: Pointer to the kgsl_device struct
 * @t: Timestamp
 * @event: Event type
 *
 * Finish recording the duration of the previous event.  Then update the
 * index, record the start of the new event, and the relevant data.
 */
static void _record_pwrevent(struct kgsl_device *device,
			ktime_t t, int event)
{
	struct kgsl_pwrscale *psc = &device->pwrscale;
	struct kgsl_pwr_history *history = &psc->history[event];
	int i = history->index;

	if (history->events == NULL)
		return;
	history->events[i].duration = ktime_us_delta(t,
					history->events[i].start);
	i = (i + 1) % history->size;
	history->index = i;
	history->events[i].start = t;
	switch (event) {
	case KGSL_PWREVENT_STATE:
		history->events[i].data = device->state;
		break;
	case KGSL_PWREVENT_GPU_FREQ:
		history->events[i].data = device->pwrctrl.active_pwrlevel;
		break;
	case KGSL_PWREVENT_BUS_FREQ:
		history->events[i].data = last_vote_buslevel;
		break;
	default:
		break;
	}
}

#ifdef CONFIG_DEVFREQ_GOV_QCOM_GPUBW_MON
#include <soc/qcom/devfreq_devbw.h>

/**
 * kgsl_get_bw() - Return latest msm bus IB vote
 */
static unsigned int kgsl_get_bw(void)
{
	return ib_votes[last_vote_buslevel];
}
#endif

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
	else if ((!pwr->bus_percent_ab) && (!pwr->bus_ab_mbytes))
		*ab = DEFAULT_BUS_P * ib / 100;
	else if (pwr->bus_width)
		*ab = pwr->bus_ab_mbytes;
	else
		*ab = (pwr->bus_percent_ab * max_bw) / 100;
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
					struct kgsl_pwr_constraint *pwrc,
					int popp)
{
	unsigned int max_pwrlevel = max_t(unsigned int, pwr->thermal_pwrlevel,
					pwr->max_pwrlevel);
	unsigned int min_pwrlevel = min_t(unsigned int,
					pwr->thermal_pwrlevel_floor,
					pwr->min_pwrlevel);

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

	if (popp && (max_pwrlevel < pwr->active_pwrlevel))
		max_pwrlevel = pwr->active_pwrlevel;

	if (level < max_pwrlevel)
		return max_pwrlevel;
	if (level > min_pwrlevel)
		return min_pwrlevel;

	return level;
}

#ifdef CONFIG_DEVFREQ_GOV_QCOM_GPUBW_MON
static void kgsl_pwrctrl_vbif_update(unsigned long ab)
{
	/* ask a governor to vote on behalf of us */
	devfreq_vbif_update_bw(ib_votes[last_vote_buslevel], ab);
}
#else
static void kgsl_pwrctrl_vbif_update(unsigned long ab)
{
}
#endif

/**
 * kgsl_bus_scale_request() - set GPU BW vote
 * @device: Pointer to the kgsl_device struct
 * @buslevel: index of bw vector[] table
 */
static int kgsl_bus_scale_request(struct kgsl_device *device,
		unsigned int buslevel)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret = 0;

	/* GMU scales BW */
	if (gmu_core_gpmu_isenabled(device))
		ret = gmu_core_dcvs_set(device, INVALID_DCVS_IDX, buslevel);
	else if (pwr->pcl)
		/* Linux bus driver scales BW */
		ret = msm_bus_scale_client_update_request(pwr->pcl, buslevel);

	if (ret)
		KGSL_PWR_ERR(device, "GPU BW scaling failure: %d\n", ret);

	return ret;
}

/**
 * kgsl_clk_set_rate() - set GPU clock rate
 * @device: Pointer to the kgsl_device struct
 * @pwrlevel: power level in pwrlevels[] table
 */
int kgsl_clk_set_rate(struct kgsl_device *device,
		unsigned int pwrlevel)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct kgsl_pwrlevel *pl = &pwr->pwrlevels[pwrlevel];
	int ret = 0;

	/* GMU scales GPU freq */
	if (gmu_core_gpmu_isenabled(device)) {
		int num_gpupwrlevels = pwr->num_pwrlevels;

		/* If GMU has not been started, save it */
		if (!gmu_core_testbit(device, GMU_HFI_ON)) {
			/* store clock change request */
			gmu_core_setbit(device, GMU_DCVS_REPLAY);
			return 0;
		}

		if (num_gpupwrlevels < 0)
			return -EINVAL;

		/* If the GMU is on we cannot vote for the lowest level */
		if (pwrlevel == (num_gpupwrlevels - 1)) {
			WARN(1, "Cannot set 0 GPU frequency with GMU\n");
			return -EINVAL;
		}
		ret = gmu_core_dcvs_set(device, pwrlevel, INVALID_DCVS_IDX);
		/* indicate actual clock  change */
		gmu_core_clearbit(device, GMU_DCVS_REPLAY);
	} else
		/* Linux clock driver scales GPU freq */
		ret = kgsl_pwrctrl_clk_set_rate(pwr->grp_clks[0],
			pl->gpu_freq, clocks[0]);

	if (ret)
		KGSL_PWR_ERR(device, "GPU clk freq set failure: %d\n", ret);

	return ret;
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

	/* the bus should be ON to update the active frequency */
	if (on && !(test_bit(KGSL_PWRFLAGS_AXI_ON, &pwr->power_flags)))
		return;
	/*
	 * If the bus should remain on calculate our request and submit it,
	 * otherwise request bus level 0, off.
	 */
	if (on) {
		buslevel = min_t(int, pwr->pwrlevels[0].bus_max,
				cur + pwr->bus_mod);
		buslevel = max_t(int, buslevel, 1);
	} else {
		/* If the bus is being turned off, reset to default level */
		pwr->bus_mod = 0;
		pwr->bus_percent_ab = 0;
		pwr->bus_ab_mbytes = 0;
	}
	trace_kgsl_buslevel(device, pwr->active_pwrlevel, buslevel);
	last_vote_buslevel = buslevel;

	/* buslevel is the IB vote, update the AB */
	_ab_buslevel_update(pwr, &ab);

	/**
	 * vote for ocmem if target supports ocmem scaling,
	 * shut down based on "on" parameter
	 */
	if (pwr->ocmem_pcl)
		msm_bus_scale_client_update_request(pwr->ocmem_pcl,
			on ? pwr->active_pwrlevel : pwr->num_pwrlevels - 1);

	kgsl_bus_scale_request(device, buslevel);

	kgsl_pwrctrl_vbif_update(ab);
}
EXPORT_SYMBOL(kgsl_pwrctrl_buslevel_update);

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
	if (!device->ftbl->pwrlevel_change_settings)
		return;

	device->ftbl->pwrlevel_change_settings(device, old, new, post);
}

/**
 * kgsl_pwrctrl_set_thermal_cycle() - set the thermal cycle if required
 * @device: Pointer to the kgsl_device struct
 * @new_level: the level to transition to
 */
void kgsl_pwrctrl_set_thermal_cycle(struct kgsl_device *device,
						unsigned int new_level)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	if ((new_level != pwr->thermal_pwrlevel) || !pwr->sysfs_pwr_limit)
		return;
	if (pwr->thermal_pwrlevel == pwr->sysfs_pwr_limit->level) {
		/* Thermal cycle for sysfs pwr limit, start cycling*/
		if (pwr->thermal_cycle == CYCLE_ENABLE) {
			pwr->thermal_cycle = CYCLE_ACTIVE;
			mod_timer(&pwr->thermal_timer, jiffies +
					(TH_HZ - pwr->thermal_timeout));
			pwr->thermal_highlow = 1;
		}
	} else {
		/* Non sysfs pwr limit, stop thermal cycle if active*/
		if (pwr->thermal_cycle == CYCLE_ACTIVE) {
			pwr->thermal_cycle = CYCLE_ENABLE;
			del_timer_sync(&pwr->thermal_timer);
		}
	}
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
	return _adjust_pwrlevel(pwr, new_level, &pwr->constraint,
					device->pwrscale.popp_level);
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

	/*
	 * If thermal cycling is required and the new level hits the
	 * thermal limit, kick off the cycling.
	 */
	kgsl_pwrctrl_set_thermal_cycle(device, new_level);

	if (new_level == old_level &&
		!gmu_core_testbit(device, GMU_DCVS_REPLAY))
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
	kgsl_pwrctrl_buslevel_update(device, true);

	pwrlevel = &pwr->pwrlevels[pwr->active_pwrlevel];
	/* Change register settings if any  BEFORE pwrlevel change*/
	kgsl_pwrctrl_pwrlevel_change_settings(device, 0);
	kgsl_clk_set_rate(device, pwr->active_pwrlevel);
	_isense_clk_set_rate(pwr, pwr->active_pwrlevel);

	trace_kgsl_pwrlevel(device,
			pwr->active_pwrlevel, pwrlevel->gpu_freq,
			pwr->previous_pwrlevel,
			pwr->pwrlevels[old_level].gpu_freq);

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
			pwr->gpu_bimc_interface_enabled = 1;
		} else if (pwr->previous_pwrlevel == 0
				&& pwr->gpu_bimc_interface_enabled) {
			clk_disable_unprepare(pwr->gpu_bimc_int_clk);
			pwr->gpu_bimc_interface_enabled = 0;
		}
	}

	/* Change register settings if any AFTER pwrlevel change*/
	kgsl_pwrctrl_pwrlevel_change_settings(device, 1);

	/* Timestamp the frequency change */
	device->pwrscale.freq_change_time = ktime_to_ms(ktime_get());
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
				device->pwrctrl.active_pwrlevel, pwrc, 0);
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
		pwrc_old->expires = jiffies + device->pwrctrl.interval_timeout;
	}
}
EXPORT_SYMBOL(kgsl_pwrctrl_set_constraint);

/**
 * kgsl_pwrctrl_update_l2pc() - Update existing qos request
 * @device: Pointer to the kgsl_device struct
 * @timeout_us: the effective duration of qos request in usecs.
 *
 * Updates an existing qos request to avoid L2PC on the
 * CPUs (which are selected through dtsi) on which GPU
 * thread is running. This would help for performance.
 */
void kgsl_pwrctrl_update_l2pc(struct kgsl_device *device,
			unsigned long timeout_us)
{
	int cpu;

	if (device->pwrctrl.l2pc_cpus_mask == 0)
		return;

	cpu = get_cpu();
	put_cpu();

	if ((1 << cpu) & device->pwrctrl.l2pc_cpus_mask) {
		pm_qos_update_request_timeout(
				&device->pwrctrl.l2pc_cpus_qos,
				device->pwrctrl.pm_qos_cpu_mask_latency,
				timeout_us);
	}
}
EXPORT_SYMBOL(kgsl_pwrctrl_update_l2pc);

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

static void kgsl_pwrctrl_min_pwrlevel_set(struct kgsl_device *device,
					int level)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

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
}

static ssize_t kgsl_pwrctrl_min_pwrlevel_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	int ret;
	unsigned int level = 0;

	if (device == NULL)
		return 0;

	ret = kgsl_sysfs_store(buf, &level);
	if (ret)
		return ret;

	kgsl_pwrctrl_min_pwrlevel_set(device, level);

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

	for (i = pwr->num_pwrlevels - 2; i >= 0; i--) {
		if (abs(pwr->pwrlevels[i].gpu_freq - clock) < 5000000)
			return i;
	}

	return -ERANGE;
}

static void kgsl_pwrctrl_max_clock_set(struct kgsl_device *device, int val)
{
	struct kgsl_pwrctrl *pwr;
	int level;

	pwr = &device->pwrctrl;

	mutex_lock(&device->mutex);
	level = _get_nearest_pwrlevel(pwr, val);
	/* If the requested power level is not supported by hw, try cycling */
	if (level < 0) {
		unsigned int hfreq, diff, udiff, i;

		if ((val < pwr->pwrlevels[pwr->num_pwrlevels - 1].gpu_freq) ||
			(val > pwr->pwrlevels[0].gpu_freq))
			goto err;

		/* Find the neighboring frequencies */
		for (i = 0; i < pwr->num_pwrlevels - 1; i++) {
			if ((pwr->pwrlevels[i].gpu_freq > val) &&
				(pwr->pwrlevels[i + 1].gpu_freq < val)) {
				level = i;
				break;
			}
		}
		if (i == pwr->num_pwrlevels - 1)
			goto err;
		hfreq = pwr->pwrlevels[i].gpu_freq;
		diff =  hfreq - pwr->pwrlevels[i + 1].gpu_freq;
		udiff = hfreq - val;
		pwr->thermal_timeout = (udiff * TH_HZ) / diff;
		pwr->thermal_cycle = CYCLE_ENABLE;
	} else {
		pwr->thermal_cycle = CYCLE_DISABLE;
		del_timer_sync(&pwr->thermal_timer);
	}
	mutex_unlock(&device->mutex);

	if (pwr->sysfs_pwr_limit)
		kgsl_pwr_limits_set_freq(pwr->sysfs_pwr_limit,
					pwr->pwrlevels[level].gpu_freq);
	return;

err:
	mutex_unlock(&device->mutex);
}

static ssize_t kgsl_pwrctrl_max_gpuclk_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	unsigned int val = 0;
	int ret;

	if (device == NULL)
		return 0;

	ret = kgsl_sysfs_store(buf, &val);
	if (ret)
		return ret;

	kgsl_pwrctrl_max_clock_set(device, val);

	return count;
}

static unsigned int kgsl_pwrctrl_max_clock_get(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr;
	unsigned int freq;

	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;
	freq = pwr->pwrlevels[pwr->thermal_pwrlevel].gpu_freq;
	/* Calculate the effective frequency if we're cycling */
	if (pwr->thermal_cycle) {
		unsigned int hfreq = freq;
		unsigned int lfreq =
			pwr->pwrlevels[pwr->thermal_pwrlevel + 1].gpu_freq;

		freq = pwr->thermal_timeout * (lfreq / TH_HZ) +
			(TH_HZ - pwr->thermal_timeout) * (hfreq / TH_HZ);
	}

	return freq;
}

static ssize_t kgsl_pwrctrl_max_gpuclk_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n",
		kgsl_pwrctrl_max_clock_get(device));
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

static ssize_t __timer_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count,
					enum kgsl_pwrctrl_timer_type timer)
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
	if (timer == KGSL_PWR_IDLE_TIMER)
		device->pwrctrl.interval_timeout = msecs_to_jiffies(val);

	mutex_unlock(&device->mutex);

	return count;
}

static ssize_t kgsl_pwrctrl_idle_timer_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	return __timer_store(dev, attr, buf, count, KGSL_PWR_IDLE_TIMER);
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
	for (index = 0; index < pwr->num_pwrlevels - 1; index++) {
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

static ssize_t kgsl_pwrctrl_gpu_clock_stats_show(
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
	mutex_lock(&device->mutex);
	kgsl_pwrscale_update_stats(device);
	mutex_unlock(&device->mutex);
	for (index = 0; index < pwr->num_pwrlevels - 1; index++)
		num_chars += snprintf(buf + num_chars, PAGE_SIZE - num_chars,
			"%llu ", pwr->clock_times[index]);

	if (num_chars < PAGE_SIZE)
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
			/* make sure pwrrail is ON before enabling clocks */
			kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_ON);
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

static ssize_t kgsl_pwrctrl_force_no_nap_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	return __force_on_show(dev, attr, buf, KGSL_PWRFLAGS_NAP_OFF);
}

static ssize_t kgsl_pwrctrl_force_no_nap_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	return __force_on_store(dev, attr, buf, count,
					KGSL_PWRFLAGS_NAP_OFF);
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


static ssize_t kgsl_popp_store(struct device *dev,
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
	if (val)
		set_bit(POPP_ON, &device->pwrscale.popp_state);
	else
		clear_bit(POPP_ON, &device->pwrscale.popp_state);
	mutex_unlock(&device->mutex);

	return count;
}

static ssize_t kgsl_popp_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);

	if (device == NULL)
		return 0;
	return snprintf(buf, PAGE_SIZE, "%d\n",
		test_bit(POPP_ON, &device->pwrscale.popp_state));
}

static ssize_t kgsl_pwrctrl_gpu_model_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	char model_str[32] = {0};

	if (device == NULL)
		return 0;

	device->ftbl->gpu_model(device, model_str, sizeof(model_str));

	return snprintf(buf, PAGE_SIZE, "%s\n", model_str);
}

static ssize_t kgsl_pwrctrl_gpu_busy_percentage_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret;
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_clk_stats *stats;
	unsigned int busy_percent = 0;

	if (device == NULL)
		return 0;
	stats = &device->pwrctrl.clk_stats;

	if (stats->total_old != 0)
		busy_percent = (stats->busy_old * 100) / stats->total_old;

	ret = snprintf(buf, PAGE_SIZE, "%d %%\n", busy_percent);

	/* Reset the stats if GPU is OFF */
	if (!test_bit(KGSL_PWRFLAGS_AXI_ON, &device->pwrctrl.power_flags)) {
		stats->busy_old = 0;
		stats->total_old = 0;
	}
	return ret;
}

static ssize_t kgsl_pwrctrl_min_clock_mhz_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;

	if (device == NULL)
		return 0;
	pwr = &device->pwrctrl;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			pwr->pwrlevels[pwr->min_pwrlevel].gpu_freq / 1000000);
}

static ssize_t kgsl_pwrctrl_min_clock_mhz_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	int level, ret;
	unsigned int freq;
	struct kgsl_pwrctrl *pwr;

	if (device == NULL)
		return 0;

	pwr = &device->pwrctrl;

	ret = kgsl_sysfs_store(buf, &freq);
	if (ret)
		return ret;

	freq *= 1000000;
	level = _get_nearest_pwrlevel(pwr, freq);

	if (level >= 0)
		kgsl_pwrctrl_min_pwrlevel_set(device, level);

	return count;
}

static ssize_t kgsl_pwrctrl_max_clock_mhz_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	unsigned int freq;

	if (device == NULL)
		return 0;

	freq = kgsl_pwrctrl_max_clock_get(device);

	return snprintf(buf, PAGE_SIZE, "%d\n", freq / 1000000);
}

static ssize_t kgsl_pwrctrl_max_clock_mhz_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	unsigned int val = 0;
	int ret;

	if (device == NULL)
		return 0;

	ret = kgsl_sysfs_store(buf, &val);
	if (ret)
		return ret;

	val *= 1000000;
	kgsl_pwrctrl_max_clock_set(device, val);

	return count;
}

static ssize_t kgsl_pwrctrl_clock_mhz_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);

	if (device == NULL)
		return 0;

	return snprintf(buf, PAGE_SIZE, "%ld\n",
			kgsl_pwrctrl_active_freq(&device->pwrctrl) / 1000000);
}

static ssize_t kgsl_pwrctrl_freq_table_mhz_show(
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
	for (index = 0; index < pwr->num_pwrlevels - 1; index++) {
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

static ssize_t kgsl_pwrctrl_temp_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrctrl *pwr;
	struct thermal_zone_device *thermal_dev;
	int ret, temperature = 0;

	if (device == NULL)
		goto done;

	pwr = &device->pwrctrl;

	if (!pwr->tzone_name)
		goto done;

	thermal_dev = thermal_zone_get_zone_by_name((char *)pwr->tzone_name);
	if (thermal_dev == NULL)
		goto done;

	ret = thermal_zone_get_temp(thermal_dev, &temperature);
	if (ret)
		goto done;

	return snprintf(buf, PAGE_SIZE, "%d\n",
			temperature);
done:
	return 0;
}

static ssize_t kgsl_pwrctrl_pwrscale_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t count)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	int ret;
	unsigned int enable = 0;

	if (device == NULL)
		return 0;

	ret = kgsl_sysfs_store(buf, &enable);
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

static ssize_t kgsl_pwrctrl_pwrscale_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct kgsl_device *device = kgsl_device_from_dev(dev);
	struct kgsl_pwrscale *psc;

	if (device == NULL)
		return 0;
	psc = &device->pwrscale;

	return snprintf(buf, PAGE_SIZE, "%u\n", psc->enabled);
}

static DEVICE_ATTR(temp, 0444, kgsl_pwrctrl_temp_show, NULL);
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
static DEVICE_ATTR(gpu_clock_stats, 0444,
	kgsl_pwrctrl_gpu_clock_stats_show,
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
static DEVICE_ATTR(popp, 0644, kgsl_popp_show, kgsl_popp_store);
static DEVICE_ATTR(force_no_nap, 0644,
	kgsl_pwrctrl_force_no_nap_show,
	kgsl_pwrctrl_force_no_nap_store);
static DEVICE_ATTR(gpu_model, 0444, kgsl_pwrctrl_gpu_model_show, NULL);
static DEVICE_ATTR(gpu_busy_percentage, 0444,
	kgsl_pwrctrl_gpu_busy_percentage_show, NULL);
static DEVICE_ATTR(min_clock_mhz, 0644, kgsl_pwrctrl_min_clock_mhz_show,
	kgsl_pwrctrl_min_clock_mhz_store);
static DEVICE_ATTR(max_clock_mhz, 0644, kgsl_pwrctrl_max_clock_mhz_show,
	kgsl_pwrctrl_max_clock_mhz_store);
static DEVICE_ATTR(clock_mhz, 0444, kgsl_pwrctrl_clock_mhz_show, NULL);
static DEVICE_ATTR(freq_table_mhz, 0444,
	kgsl_pwrctrl_freq_table_mhz_show, NULL);
static DEVICE_ATTR(pwrscale, 0644,
	kgsl_pwrctrl_pwrscale_show,
	kgsl_pwrctrl_pwrscale_store);

static const struct device_attribute *pwrctrl_attr_list[] = {
	&dev_attr_gpuclk,
	&dev_attr_max_gpuclk,
	&dev_attr_idle_timer,
	&dev_attr_gpubusy,
	&dev_attr_gpu_available_frequencies,
	&dev_attr_gpu_clock_stats,
	&dev_attr_max_pwrlevel,
	&dev_attr_min_pwrlevel,
	&dev_attr_thermal_pwrlevel,
	&dev_attr_num_pwrlevels,
	&dev_attr_pmqos_active_latency,
	&dev_attr_reset_count,
	&dev_attr_force_clk_on,
	&dev_attr_force_bus_on,
	&dev_attr_force_rail_on,
	&dev_attr_force_no_nap,
	&dev_attr_bus_split,
	&dev_attr_default_pwrlevel,
	&dev_attr_popp,
	&dev_attr_gpu_model,
	&dev_attr_gpu_busy_percentage,
	&dev_attr_min_clock_mhz,
	&dev_attr_max_clock_mhz,
	&dev_attr_clock_mhz,
	&dev_attr_freq_table_mhz,
	&dev_attr_temp,
	&dev_attr_pwrscale,
	NULL
};

struct sysfs_link {
	const char *src;
	const char *dst;
};

static struct sysfs_link link_names[] = {
	{ "gpu_model", "gpu_model",},
	{ "gpu_busy_percentage", "gpu_busy",},
	{ "min_clock_mhz", "gpu_min_clock",},
	{ "max_clock_mhz", "gpu_max_clock",},
	{ "clock_mhz", "gpu_clock",},
	{ "freq_table_mhz", "gpu_freq_table",},
	{ "temp", "gpu_tmu",},
};

int kgsl_pwrctrl_init_sysfs(struct kgsl_device *device)
{
	int i, ret;

	ret = kgsl_create_device_sysfs_files(device->dev, pwrctrl_attr_list);
	if (ret)
		return ret;

	device->gpu_sysfs_kobj = kobject_create_and_add("gpu", kernel_kobj);
	if (IS_ERR_OR_NULL(device->gpu_sysfs_kobj))
		return (device->gpu_sysfs_kobj == NULL) ?
		-ENOMEM : PTR_ERR(device->gpu_sysfs_kobj);

	for (i = 0; i < ARRAY_SIZE(link_names); i++)
		kgsl_gpu_sysfs_add_link(device->gpu_sysfs_kobj,
			&device->dev->kobj, link_names[i].src,
			link_names[i].dst);

	return 0;
}

void kgsl_pwrctrl_uninit_sysfs(struct kgsl_device *device)
{
	kgsl_remove_device_sysfs_files(device->dev, pwrctrl_attr_list);
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
EXPORT_SYMBOL(kgsl_pwrctrl_busy_time);

static void kgsl_pwrctrl_clk(struct kgsl_device *device, int state,
					  int requested_state)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i = 0;

	if (gmu_core_gpmu_isenabled(device))
		return;
	if (test_bit(KGSL_PWRFLAGS_CLK_ON, &pwr->ctrl_flags))
		return;

	if (state == KGSL_PWRFLAGS_OFF) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_CLK_ON,
			&pwr->power_flags)) {
			trace_kgsl_clk(device, state,
					kgsl_pwrctrl_active_freq(pwr));
			/* Disable gpu-bimc-interface clocks */
			if (pwr->gpu_bimc_int_clk &&
					pwr->gpu_bimc_interface_enabled) {
				clk_disable_unprepare(pwr->gpu_bimc_int_clk);
				pwr->gpu_bimc_interface_enabled = 0;
			}

			for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
				clk_disable(pwr->grp_clks[i]);
			/* High latency clock maintenance. */
			if ((pwr->pwrlevels[0].gpu_freq > 0) &&
				(requested_state != KGSL_STATE_NAP)) {
				for (i = KGSL_MAX_CLKS - 1; i > 0; i--)
					clk_unprepare(pwr->grp_clks[i]);
				kgsl_clk_set_rate(device,
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
				kgsl_clk_set_rate(device,
						pwr->num_pwrlevels - 1);
				_isense_clk_set_rate(pwr,
					pwr->num_pwrlevels - 1);
			}
		}
	} else if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_CLK_ON,
			&pwr->power_flags)) {
			trace_kgsl_clk(device, state,
					kgsl_pwrctrl_active_freq(pwr));
			/* High latency clock maintenance. */
			if (device->state != KGSL_STATE_NAP) {
				if (pwr->pwrlevels[0].gpu_freq > 0) {
					kgsl_clk_set_rate(device,
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
					pwr->gpu_bimc_interface_enabled = 1;
				}
			}

			/* Turn on the IOMMU clocks */
			kgsl_mmu_enable_clk(&device->mmu);
		}

	}
}

#ifdef CONFIG_DEVFREQ_GOV_QCOM_GPUBW_MON
static void kgsl_pwrctrl_suspend_devbw(struct kgsl_pwrctrl *pwr)
{
	if (pwr->devbw)
		devfreq_suspend_devbw(pwr->devbw);
}

static void kgsl_pwrctrl_resume_devbw(struct kgsl_pwrctrl *pwr)
{
	if (pwr->devbw)
		devfreq_resume_devbw(pwr->devbw);
}
#else
static void kgsl_pwrctrl_suspend_devbw(struct kgsl_pwrctrl *pwr)
{
}

static void kgsl_pwrctrl_resume_devbw(struct kgsl_pwrctrl *pwr)
{
}
#endif

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

			kgsl_pwrctrl_suspend_devbw(pwr);
		}
	} else if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_AXI_ON,
			&pwr->power_flags)) {
			trace_kgsl_bus(device, state);
			kgsl_pwrctrl_buslevel_update(device, true);

			kgsl_pwrctrl_resume_devbw(pwr);
		}
	}
}

static int _regulator_enable(struct kgsl_device *device,
		struct kgsl_regulator *regulator)
{
	int ret;

	if (IS_ERR_OR_NULL(regulator->reg))
		return 0;

	ret = regulator_enable(regulator->reg);
	if (ret)
		KGSL_DRV_ERR(device, "Failed to enable regulator '%s': %d\n",
			regulator->name, ret);
	return ret;
}

static void _regulator_disable(struct kgsl_regulator *regulator)
{
	if (!IS_ERR_OR_NULL(regulator->reg))
		regulator_disable(regulator->reg);
}

static int _enable_regulators(struct kgsl_device *device,
		struct kgsl_pwrctrl *pwr)
{
	int i;

	for (i = 0; i < KGSL_MAX_REGULATORS; i++) {
		int ret = _regulator_enable(device, &pwr->regulators[i]);

		if (ret) {
			for (i = i - 1; i >= 0; i--)
				_regulator_disable(&pwr->regulators[i]);
			return ret;
		}
	}

	return 0;
}

static int kgsl_pwrctrl_pwrrail(struct kgsl_device *device, int state)
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

	if (state == KGSL_PWRFLAGS_OFF) {
		if (test_and_clear_bit(KGSL_PWRFLAGS_POWER_ON,
			&pwr->power_flags)) {
			trace_kgsl_rail(device, state);
			device->ftbl->regulator_disable_poll(device);
		}
	} else if (state == KGSL_PWRFLAGS_ON) {
		if (!test_and_set_bit(KGSL_PWRFLAGS_POWER_ON,
			&pwr->power_flags)) {
			status = _enable_regulators(device, pwr);

			if (status)
				clear_bit(KGSL_PWRFLAGS_POWER_ON,
					&pwr->power_flags);
			else
				trace_kgsl_rail(device, state);
		}
	}

	return status;
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

static void kgsl_thermal_timer(unsigned long data)
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
	kgsl_schedule_work(&device->pwrctrl.thermal_cycle_ws);
}

#ifdef CONFIG_DEVFREQ_GOV_QCOM_GPUBW_MON
static int kgsl_pwrctrl_vbif_init(void)
{
	devfreq_vbif_register_callback(kgsl_get_bw);
	return 0;
}
#else
static int kgsl_pwrctrl_vbif_init(void)
{
	return 0;
}
#endif

static int _get_regulator(struct kgsl_device *device,
		struct kgsl_regulator *regulator, const char *str)
{
	regulator->reg = devm_regulator_get(&device->pdev->dev, str);
	if (IS_ERR(regulator->reg)) {
		KGSL_CORE_ERR("Couldn't get regulator: %s (%ld)\n",
			str, PTR_ERR(regulator->reg));
		return PTR_ERR(regulator->reg);
	}

	strlcpy(regulator->name, str, sizeof(regulator->name));
	return 0;
}

static int get_legacy_regulators(struct kgsl_device *device)
{
	struct device *dev = &device->pdev->dev;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret;

	ret = _get_regulator(device, &pwr->regulators[0], "vdd");

	/* Use vddcx only on targets that have it. */
	if (ret == 0 && of_find_property(dev->of_node, "vddcx-supply", NULL))
		ret = _get_regulator(device, &pwr->regulators[1], "vddcx");

	return ret;
}

static int get_regulators(struct kgsl_device *device)
{
	struct device *dev = &device->pdev->dev;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int index = 0;
	const char *name;
	struct property *prop;

	if (!of_find_property(dev->of_node, "regulator-names", NULL))
		return get_legacy_regulators(device);

	of_property_for_each_string(dev->of_node,
		"regulator-names", prop, name) {
		int ret;

		if (index == KGSL_MAX_REGULATORS) {
			KGSL_CORE_ERR("Too many regulators defined\n");
			return -ENOMEM;
		}

		ret = _get_regulator(device, &pwr->regulators[index], name);
		if (ret)
			return ret;
		index++;
	}

	return 0;
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

			pwr->grp_clks[i] = devm_clk_get(dev, name);

			if (IS_ERR(pwr->grp_clks[i])) {
				int ret = PTR_ERR(pwr->grp_clks[i]);

				KGSL_CORE_ERR("Couldn't get clock: %s (%d)\n",
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
		KGSL_CORE_ERR("Couldn't get isense clock on level\n");
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

	if (device->state == KGSL_STATE_NAP) {
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
	KGSL_DRV_ERR(device, "GPU Clock %s enable error:%d\n", name, ret);
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
		KGSL_DRV_ERR(device, "GPU clock %s enable error:%d\n",
				name, ret);
}

static int kgsl_pwrctrl_clk_set_rate(struct clk *grp_clk, unsigned int freq,
		const char *name)
{
	int ret = clk_set_rate(grp_clk, freq);

	WARN(ret, "%s set freq %d failed:%d\n", name, freq, ret);
	return ret;
}

static inline void _close_pcl(struct kgsl_pwrctrl *pwr)
{
	if (pwr->pcl)
		msm_bus_scale_unregister_client(pwr->pcl);

	pwr->pcl = 0;
}

static inline void _close_ocmem_pcl(struct kgsl_pwrctrl *pwr)
{
	if (pwr->ocmem_pcl)
		msm_bus_scale_unregister_client(pwr->ocmem_pcl);

	pwr->ocmem_pcl = 0;
}

static inline void _close_regulators(struct kgsl_pwrctrl *pwr)
{
	int i;

	for (i = 0; i < KGSL_MAX_REGULATORS; i++)
		pwr->regulators[i].reg = NULL;
}

static inline void _close_clks(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i;

	for (i = 0; i < KGSL_MAX_CLKS; i++)
		pwr->grp_clks[i] = NULL;

	if (pwr->gpu_bimc_int_clk)
		devm_clk_put(&device->pdev->dev, pwr->gpu_bimc_int_clk);
}

static bool _gpu_freq_supported(struct kgsl_pwrctrl *pwr, unsigned int freq)
{
	int i;

	for (i = pwr->num_pwrlevels - 2; i >= 0; i--) {
		if (pwr->pwrlevels[i].gpu_freq == freq)
			return true;
	}

	return false;
}

static void kgsl_pwrctrl_disable_unused_opp(struct kgsl_device *device)
{
	struct device *dev = &device->pdev->dev;
	struct dev_pm_opp *opp;
	unsigned long freq = 0;
	int ret;

	ret = dev_pm_opp_get_opp_count(dev);
	/* Return early, If no OPP table or OPP count is zero */
	if (ret <= 0)
		return;

	while (1) {
		opp = dev_pm_opp_find_freq_ceil(dev, &freq);
		if (IS_ERR(opp))
			break;

		if (!_gpu_freq_supported(&device->pwrctrl, freq))
			dev_pm_opp_disable(dev, freq);

		dev_pm_opp_put(opp);
		freq++;
	}
}

static bool pwrlevel_uses_ib(struct msm_bus_scale_pdata *bus_scale_table,
				struct msm_bus_vectors *vector,
				struct kgsl_pwrctrl *pwr, int m)
{
	if (bus_scale_table->usecase[pwr->pwrlevels[m].bus_freq].vectors[0].ib
		 == vector->ib)
		return true;
	else
		return false;
}

int kgsl_pwrctrl_init(struct kgsl_device *device)
{
	int i, k, m, n = 0, result, freq;
	struct platform_device *pdev = device->pdev;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct device_node *ocmem_bus_node;
	struct msm_bus_scale_pdata *ocmem_scale_table = NULL;
	struct msm_bus_scale_pdata *bus_scale_table;
	struct device_node *gpubw_dev_node = NULL;
	struct platform_device *p2dev;

	bus_scale_table = msm_bus_cl_get_pdata(device->pdev);
	if (bus_scale_table == NULL)
		return -EINVAL;

	result = _get_clocks(device);
	if (result)
		goto error_cleanup_clks;

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

	if (pwr->num_pwrlevels == 0) {
		KGSL_PWR_ERR(device, "No power levels are defined\n");
		result = -EINVAL;
		goto error_cleanup_clks;
	}

	/* Initialize the user and thermal clock constraints */

	pwr->max_pwrlevel = 0;
	pwr->min_pwrlevel = pwr->num_pwrlevels - 2;
	pwr->thermal_pwrlevel = 0;
	pwr->thermal_pwrlevel_floor = pwr->min_pwrlevel;

	pwr->wakeup_maxpwrlevel = 0;

	for (i = 0; i < pwr->num_pwrlevels; i++) {
		freq = pwr->pwrlevels[i].gpu_freq;

		if (freq > 0)
			freq = clk_round_rate(pwr->grp_clks[0], freq);

		if (freq >= pwr->pwrlevels[i].gpu_freq)
			pwr->pwrlevels[i].gpu_freq = freq;
	}

	kgsl_pwrctrl_disable_unused_opp(device);

	kgsl_clk_set_rate(device, pwr->num_pwrlevels - 1);

	freq = clk_round_rate(pwr->grp_clks[6], KGSL_RBBMTIMER_CLK_FREQ);
	if (freq > 0)
		kgsl_pwrctrl_clk_set_rate(pwr->grp_clks[6],
			freq, clocks[6]);

	_isense_clk_set_rate(pwr, pwr->num_pwrlevels - 1);

	result = get_regulators(device);
	if (result)
		goto error_cleanup_regulators;

	pwr->power_flags = 0;

	kgsl_property_read_u32(device, "qcom,l2pc-cpu-mask",
			&pwr->l2pc_cpus_mask);

	pwr->l2pc_update_queue = of_property_read_bool(
				device->pdev->dev.of_node,
				"qcom,l2pc-update-queue");

	pm_runtime_enable(&pdev->dev);

	ocmem_bus_node = of_find_node_by_name(
				device->pdev->dev.of_node,
				"qcom,ocmem-bus-client");
	/* If platform has split ocmem bus client - use it */
	if (ocmem_bus_node) {
		ocmem_scale_table = msm_bus_pdata_from_node
				(device->pdev, ocmem_bus_node);
		if (ocmem_scale_table)
			pwr->ocmem_pcl = msm_bus_scale_register_client
					(ocmem_scale_table);

		if (!pwr->ocmem_pcl) {
			result = -EINVAL;
			goto error_disable_pm;
		}
	}

	/* Bus width in bytes, set it to zero if not found */
	if (of_property_read_u32(pdev->dev.of_node, "qcom,bus-width",
		&pwr->bus_width))
		pwr->bus_width = 0;

	/* Check if gpu bandwidth vote device is defined in dts */
	if (pwr->bus_control)
		/* Check if gpu bandwidth vote device is defined in dts */
		gpubw_dev_node = of_parse_phandle(pdev->dev.of_node,
					"qcom,gpubw-dev", 0);

	/*
	 * Governor support enables the gpu bus scaling via governor
	 * and hence no need to register for bus scaling client
	 * if gpubw-dev is defined.
	 */
	if (gpubw_dev_node) {
		p2dev = of_find_device_by_node(gpubw_dev_node);
		if (p2dev)
			pwr->devbw = &p2dev->dev;
	} else {
		/*
		 * Register for gpu bus scaling if governor support
		 * is not enabled and gpu bus voting is to be done
		 * from the driver.
		 */
		pwr->pcl = msm_bus_scale_register_client(bus_scale_table);
		if (pwr->pcl == 0) {
			result = -EINVAL;
			goto error_cleanup_ocmem_pcl;
		}
	}

	pwr->bus_ib = kzalloc(bus_scale_table->num_usecases *
		sizeof(*pwr->bus_ib), GFP_KERNEL);
	if (pwr->bus_ib == NULL) {
		result = -ENOMEM;
		goto error_cleanup_pcl;
	}

	/*
	 * Pull the BW vote out of the bus table.  They will be used to
	 * calculate the ratio between the votes.
	 */
	for (i = 0; i < bus_scale_table->num_usecases; i++) {
		struct msm_bus_paths *usecase =
				&bus_scale_table->usecase[i];
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

			/* check for duplicate values */
			for (k = 0; k < n; k++)
				if (vector->ib == pwr->bus_ib[k])
					break;

			/* if this is a new ib value, save it */
			if (k == n) {
				pwr->bus_ib[k] = vector->ib;
				n++;
				/* find which pwrlevels use this ib */
				for (m = 0; m < pwr->num_pwrlevels - 1; m++) {
					if (pwrlevel_uses_ib(bus_scale_table,
						vector, pwr, m))
						pwr->bus_index[m] = k;
				}
			}
		}
	}

	INIT_WORK(&pwr->thermal_cycle_ws, kgsl_thermal_cycle);
	setup_timer(&pwr->thermal_timer, kgsl_thermal_timer,
			(unsigned long) device);

	INIT_LIST_HEAD(&pwr->limits);
	spin_lock_init(&pwr->limits_lock);
	pwr->sysfs_pwr_limit = kgsl_pwr_limits_add(KGSL_DEVICE_3D0);

	kgsl_pwrctrl_vbif_init();

	/* temperature sensor name */
	of_property_read_string(pdev->dev.of_node, "qcom,tzone-name",
		&pwr->tzone_name);

	return result;

error_cleanup_pcl:
	_close_pcl(pwr);
error_cleanup_ocmem_pcl:
	_close_ocmem_pcl(pwr);
error_disable_pm:
	pm_runtime_disable(&pdev->dev);
error_cleanup_regulators:
	_close_regulators(pwr);
error_cleanup_clks:
	_close_clks(device);
	return result;
}

void kgsl_pwrctrl_close(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	KGSL_PWR_INFO(device, "close device %d\n", device->id);

	pwr->power_flags = 0;

	if (!IS_ERR_OR_NULL(pwr->sysfs_pwr_limit)) {
		list_del(&pwr->sysfs_pwr_limit->node);
		kfree(pwr->sysfs_pwr_limit);
		pwr->sysfs_pwr_limit = NULL;
	}
	kfree(pwr->bus_ib);

	_close_pcl(pwr);

	_close_ocmem_pcl(pwr);

	pm_runtime_disable(&device->pdev->dev);

	_close_regulators(pwr);

	_close_clks(device);
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
	int ret = 0;
	unsigned int requested_state;

	mutex_lock(&device->mutex);

	requested_state = device->requested_state;

	if (device->state == KGSL_STATE_ACTIVE
		   || device->state ==  KGSL_STATE_NAP) {

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
EXPORT_SYMBOL(kgsl_pre_hwaccess);

static int kgsl_pwrctrl_enable(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int level, status;

	if (pwr->wakeup_maxpwrlevel) {
		level = pwr->max_pwrlevel;
		pwr->wakeup_maxpwrlevel = 0;
	} else if (kgsl_popp_check(device)) {
		level = pwr->active_pwrlevel;
	} else {
		level = pwr->default_pwrlevel;
	}

	kgsl_pwrctrl_pwrlevel_change(device, level);

	if (gmu_core_gpmu_isenabled(device)) {
		int ret = gmu_core_start(device);

		if (!ret)
			kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_ON);
		return ret;
	}

	/* Order pwrrail/clk sequence based upon platform */
	status = kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_ON);
	if (status)
		return status;
	kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_ON, KGSL_STATE_ACTIVE);
	kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_ON);
	return device->ftbl->regulator_enable(device);
}

static void kgsl_pwrctrl_disable(struct kgsl_device *device)
{
	if (gmu_core_gpmu_isenabled(device)) {
		kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_OFF);
		return gmu_core_stop(device);
	}

	/* Order pwrrail/clk sequence based upon platform */
	device->ftbl->regulator_disable(device);
	kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_OFF);
	kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_OFF, KGSL_STATE_SLUMBER);
	kgsl_pwrctrl_pwrrail(device, KGSL_PWRFLAGS_OFF);
}

static void
kgsl_pwrctrl_clk_set_options(struct kgsl_device *device, bool on)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int i;

	for (i = 0; i < KGSL_MAX_CLKS; i++) {
		if (pwr->grp_clks[i] == NULL)
			continue;

		if (device->ftbl->clk_set_options)
			device->ftbl->clk_set_options(device, clocks[i],
				pwr->grp_clks[i], on);
	}
}

/**
 * _init() - Get the GPU ready to start, but don't turn anything on
 * @device - Pointer to the kgsl_device struct
 */
static int _init(struct kgsl_device *device)
{
	int status = 0;

	switch (device->state) {
	case KGSL_STATE_NAP:
		/* Force power on to do the stop */
		status = kgsl_pwrctrl_enable(device);
	case KGSL_STATE_ACTIVE:
		/* fall through */
	case KGSL_STATE_RESET:
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
		del_timer_sync(&device->idle_timer);
		kgsl_pwrscale_midframe_timer_cancel(device);
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
		/* fall through */
	case KGSL_STATE_SLUMBER:
		kgsl_pwrctrl_clk_set_options(device, true);
		status = device->ftbl->start(device,
				device->pwrctrl.superfast);
		device->pwrctrl.superfast = false;

		if (status) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
			KGSL_DRV_ERR(device, "start failed %d\n", status);
			break;
		}
		kgsl_pwrctrl_axi(device, KGSL_PWRFLAGS_ON);
		kgsl_pwrscale_wake(device);
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_ON);
		/* fall through */
	case KGSL_STATE_NAP:
		/* Turn on the core clocks */
		kgsl_pwrctrl_clk(device, KGSL_PWRFLAGS_ON, KGSL_STATE_ACTIVE);

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
		mod_timer(&device->idle_timer, jiffies +
				device->pwrctrl.interval_timeout);
		break;
	case KGSL_STATE_AWARE:
		kgsl_pwrctrl_clk_set_options(device, true);
		/* Enable state before turning on irq */
		kgsl_pwrctrl_set_state(device, KGSL_STATE_ACTIVE);
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_ON);
		mod_timer(&device->idle_timer, jiffies +
				device->pwrctrl.interval_timeout);
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
	unsigned int state = device->state;

	switch (device->state) {
	case KGSL_STATE_RESET:
		if (!gmu_core_gpmu_isenabled(device))
			break;
		status = gmu_core_start(device);
		break;
	case KGSL_STATE_INIT:
		status = kgsl_pwrctrl_enable(device);
		break;
	/* The following 3 cases shouldn't occur, but don't panic. */
	case KGSL_STATE_NAP:
		status = _wake(device);
	case KGSL_STATE_ACTIVE:
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
		del_timer_sync(&device->idle_timer);
		kgsl_pwrscale_midframe_timer_cancel(device);
		break;
	case KGSL_STATE_SLUMBER:
		/* if GMU already in FAULT */
		if (gmu_core_isenabled(device) &&
			gmu_core_testbit(device, GMU_FAULT)) {
			status = -EINVAL;
			break;
		}

		status = kgsl_pwrctrl_enable(device);
		break;
	default:
		status = -EINVAL;
	}

	if (status) {
		if (gmu_core_isenabled(device)) {
			/* GMU hang recovery */
			kgsl_pwrctrl_set_state(device, KGSL_STATE_RESET);
			gmu_core_setbit(device, GMU_FAULT);
			status = kgsl_pwrctrl_enable(device);
			if (status) {
				/*
				 * Cannot recover GMU failure
				 * GPU will not be powered on
				 */
				WARN_ONCE(1, "Failed to recover GMU\n");
				if (device->snapshot)
					device->snapshot->recovered = false;
				/*
				 * On recovery failure, we are clearing
				 * GMU_FAULT bit and also not keeping
				 * the state as RESET to make sure any
				 * attempt to wake GMU/GPU after this
				 * is treated as a fresh start. But on
				 * recovery failure, GMU HS, clocks and
				 * IRQs are still ON/enabled because of
				 * which next GMU/GPU wakeup results in
				 * multiple warnings from GMU start as HS,
				 * clocks and IRQ were ON while doing a
				 * fresh start i.e. wake from SLUMBER.
				 *
				 * Suspend the GMU on recovery failure
				 * to make sure next attempt to wake up
				 * GMU/GPU is indeed a fresh start.
				 */
				gmu_core_suspend(device);
				kgsl_pwrctrl_set_state(device, state);
			} else {
				if (device->snapshot)
					device->snapshot->recovered = true;
				kgsl_pwrctrl_set_state(device,
					KGSL_STATE_AWARE);
			}

			gmu_core_clearbit(device, GMU_FAULT);
			return status;
		}

		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
	} else {
		kgsl_pwrctrl_set_state(device, KGSL_STATE_AWARE);
	}
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

		device->ftbl->stop_fault_timer(device);
		kgsl_pwrscale_midframe_timer_cancel(device);

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
		/* fallthrough */
	case KGSL_STATE_SLUMBER:
	case KGSL_STATE_RESET:
		break;
	case KGSL_STATE_AWARE:
		KGSL_PWR_WARN(device,
			"transition AWARE -> NAP is not permitted\n");
		/* fallthrough */
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
		del_timer_sync(&device->idle_timer);
		kgsl_pwrscale_midframe_timer_cancel(device);
		if (device->pwrctrl.thermal_cycle == CYCLE_ACTIVE) {
			device->pwrctrl.thermal_cycle = CYCLE_ENABLE;
			del_timer_sync(&device->pwrctrl.thermal_timer);
		}
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
		/* make sure power is on to stop the device*/
		status = kgsl_pwrctrl_enable(device);
		device->ftbl->suspend_context(device);
		device->ftbl->stop(device);
		kgsl_pwrctrl_clk_set_options(device, false);
		kgsl_pwrctrl_disable(device);
		kgsl_pwrscale_sleep(device);
		kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_SLUMBER);
		pm_qos_update_request(&device->pwrctrl.pm_qos_req_dma,
						PM_QOS_DEFAULT_VALUE);
		if (device->pwrctrl.l2pc_cpus_mask)
			pm_qos_update_request(
					&device->pwrctrl.l2pc_cpus_qos,
					PM_QOS_DEFAULT_VALUE);
		break;
	case KGSL_STATE_SUSPEND:
		complete_all(&device->hwaccess_gate);
		device->ftbl->resume(device);
		kgsl_pwrctrl_set_state(device, KGSL_STATE_SLUMBER);
		break;
	case KGSL_STATE_AWARE:
		kgsl_pwrctrl_disable(device);
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
	case KGSL_STATE_SLUMBER:
		status = _slumber(device);
		break;
	case KGSL_STATE_SUSPEND:
		status = _suspend(device);
		break;
	case KGSL_STATE_RESET:
		kgsl_pwrctrl_set_state(device, KGSL_STATE_RESET);
		break;
	default:
		KGSL_PWR_INFO(device, "bad state request 0x%x\n", state);
		kgsl_pwrctrl_request_state(device, KGSL_STATE_NONE);
		status = -EINVAL;
		break;
	}

	/* Record the state timing info */
	if (!status) {
		ktime_t t = ktime_get();

		_record_pwrevent(device, t, KGSL_PWREVENT_STATE);
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

	spin_lock(&device->submit_lock);
	if (state == KGSL_STATE_SLUMBER || state == KGSL_STATE_SUSPEND)
		device->slumber = true;
	else
		device->slumber = false;
	spin_unlock(&device->submit_lock);
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
	case KGSL_STATE_SUSPEND:
		return "SUSPEND";
	case KGSL_STATE_SLUMBER:
		return "SLUMBER";
	case KGSL_STATE_RESET:
		return "RESET";
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

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return -EINVAL;

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
	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return;

	if (WARN(atomic_read(&device->active_cnt) == 0,
			"Unbalanced get/put calls to KGSL active count\n"))
		return;

	if (atomic_dec_and_test(&device->active_cnt)) {
		bool nap_on = !(device->pwrctrl.ctrl_flags &
			BIT(KGSL_PWRFLAGS_NAP_OFF));
		if (nap_on && device->state == KGSL_STATE_ACTIVE &&
			device->requested_state == KGSL_STATE_NONE) {
			kgsl_pwrctrl_request_state(device, KGSL_STATE_NAP);
			kgsl_schedule_work(&device->idle_check_ws);
		} else if (!nap_on) {
			kgsl_pwrscale_update_stats(device);
			kgsl_pwrscale_update(device);
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
EXPORT_SYMBOL(kgsl_active_count_wait);

/**
 * _update_limits() - update the limits based on the current requests
 * @limit: Pointer to the limits structure
 * @reason: Reason for the update
 * @level: Level if any to be set
 *
 * Set the thermal pwrlevel based on the current limits
 */
static void _update_limits(struct kgsl_pwr_limit *limit, unsigned int reason,
							unsigned int level)
{
	struct kgsl_device *device = limit->device;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct kgsl_pwr_limit *temp_limit;
	unsigned int max_level = 0;

	spin_lock(&pwr->limits_lock);
	switch (reason) {
	case KGSL_PWR_ADD_LIMIT:
		list_add(&limit->node, &pwr->limits);
		break;
	case KGSL_PWR_DEL_LIMIT:
		list_del(&limit->node);
		if (list_empty(&pwr->limits))
			goto done;
		break;
	case KGSL_PWR_SET_LIMIT:
		limit->level = level;
		break;
	default:
		break;
	}

	list_for_each_entry(temp_limit, &pwr->limits, node) {
		max_level = max_t(unsigned int, max_level, temp_limit->level);
	}

done:
	spin_unlock(&pwr->limits_lock);

	mutex_lock(&device->mutex);
	pwr->thermal_pwrlevel = max_level;
	kgsl_pwrctrl_pwrlevel_change(device, pwr->active_pwrlevel);
	mutex_unlock(&device->mutex);
}

/**
 * kgsl_pwr_limits_add() - Add a new pwr limit
 * @id: Device ID
 *
 * Allocate a pwr limit structure for the client, add it to the limits
 * list and return the pointer to the client
 */
void *kgsl_pwr_limits_add(enum kgsl_deviceid id)
{
	struct kgsl_device *device = kgsl_get_device(id);
	struct kgsl_pwr_limit *limit;

	if (IS_ERR_OR_NULL(device))
		return NULL;

	limit = kzalloc(sizeof(struct kgsl_pwr_limit),
						GFP_KERNEL);
	if (limit == NULL)
		return ERR_PTR(-ENOMEM);
	limit->device = device;

	_update_limits(limit, KGSL_PWR_ADD_LIMIT, 0);
	return limit;
}
EXPORT_SYMBOL(kgsl_pwr_limits_add);

/**
 * kgsl_pwr_limits_del() - Unregister the pwr limit client and
 * adjust the thermal limits
 * @limit_ptr: Client handle
 *
 * Delete the client handle from the thermal list and adjust the
 * active clocks if needed.
 */
void kgsl_pwr_limits_del(void *limit_ptr)
{
	struct kgsl_pwr_limit *limit = limit_ptr;

	if (IS_ERR_OR_NULL(limit))
		return;

	_update_limits(limit, KGSL_PWR_DEL_LIMIT, 0);
	kfree(limit);
}
EXPORT_SYMBOL(kgsl_pwr_limits_del);

/**
 * kgsl_pwr_limits_set_freq() - Set the requested limit for the client
 * @limit_ptr: Client handle
 * @freq: Client requested frequency
 *
 * Set the new limit for the client and adjust the clocks
 */
int kgsl_pwr_limits_set_freq(void *limit_ptr, unsigned int freq)
{
	struct kgsl_pwrctrl *pwr;
	struct kgsl_pwr_limit *limit = limit_ptr;
	int level;

	if (IS_ERR_OR_NULL(limit))
		return -EINVAL;

	pwr = &limit->device->pwrctrl;
	level = _get_nearest_pwrlevel(pwr, freq);
	if (level < 0)
		return -EINVAL;
	_update_limits(limit, KGSL_PWR_SET_LIMIT, level);
	return 0;
}
EXPORT_SYMBOL(kgsl_pwr_limits_set_freq);

/**
 * kgsl_pwr_limits_set_default() - Set the default thermal limit for the client
 * @limit_ptr: Client handle
 *
 * Set the default for the client and adjust the clocks
 */
void kgsl_pwr_limits_set_default(void *limit_ptr)
{
	struct kgsl_pwr_limit *limit = limit_ptr;

	if (IS_ERR_OR_NULL(limit))
		return;

	_update_limits(limit, KGSL_PWR_SET_LIMIT, 0);
}
EXPORT_SYMBOL(kgsl_pwr_limits_set_default);

/**
 * kgsl_pwr_limits_get_freq() - Get the current limit
 * @id: Device ID
 *
 * Get the current limit set for the device
 */
unsigned int kgsl_pwr_limits_get_freq(enum kgsl_deviceid id)
{
	struct kgsl_device *device = kgsl_get_device(id);
	struct kgsl_pwrctrl *pwr;
	unsigned int freq;

	if (IS_ERR_OR_NULL(device))
		return 0;
	pwr = &device->pwrctrl;
	mutex_lock(&device->mutex);
	freq = pwr->pwrlevels[pwr->thermal_pwrlevel].gpu_freq;
	mutex_unlock(&device->mutex);

	return freq;
}
EXPORT_SYMBOL(kgsl_pwr_limits_get_freq);

/**
 * kgsl_pwrctrl_set_default_gpu_pwrlevel() - Set GPU to default power level
 * @device: Pointer to the kgsl_device struct
 */
void kgsl_pwrctrl_set_default_gpu_pwrlevel(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	unsigned int new_level = pwr->default_pwrlevel;
	unsigned int old_level = pwr->active_pwrlevel;

	/*
	 * Update the level according to any thermal,
	 * max/min, or power constraints.
	 */
	new_level = kgsl_pwrctrl_adjust_pwrlevel(device, new_level);

	/*
	 * If thermal cycling is required and the new level hits the
	 * thermal limit, kick off the cycling.
	 */
	kgsl_pwrctrl_set_thermal_cycle(device, new_level);

	pwr->active_pwrlevel = new_level;
	pwr->previous_pwrlevel = old_level;

	/* Request adjusted DCVS level */
	kgsl_clk_set_rate(device, pwr->active_pwrlevel);
}
