/*
 * drivers/video/tegra/host/gr3d/scale3d_actmon.c
 *
 * Tegra Graphics Host 3D clock scaling
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * 3d clock scaling mechanism
 *
 * module3d_notify_busy() is called upon submit, module3d_notify_idle() is
 * called when all outstanding submits are completed. Both functions notify
 * the governor about changed state.
 *
 * 3d.emc clock is scaled proportionately to 3d clock, with a quadratic-
 * bezier-like factor added to pull 3d.emc rate a bit lower.
 */

#include <linux/devfreq.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/platform_data/tegra_edp.h>

#include <chip_support.h>
#include <host1x/host1x.h>
#include <host1x/hw_host1x02_sync.h>

#include <mach/clk.h>
#include <mach/hardware.h>

#include <governor.h>

#include "pod_scaling.h"
#include "scale3d_actmon.h"
#include "dev.h"
#include "nvhost_acm.h"

#define POW2(x) ((x) * (x))

static int nvhost_scale3d_target(struct device *d, unsigned long *freq,
					u32 flags);

/*******************************************************************************
 * power_profile_rec - Device specific power management variables
 ******************************************************************************/

struct power_profile_gr3d {

	int				init;

	/*
	 * Use emc_slope and emc_dip_slope as unsigned fixed point<20,12> to
	 * have finer granularity. Without doing this, emc_dip_slope is zero all
	 * the time. Need to scale down emc_offset and emc_dip_offset
	 * from Hz to MHz to avoid integer overflow due to
	 * increased emc_slope and emc_dip_slope values.
	 */
	unsigned long			max_rate_3d;
	unsigned long			min_rate_3d;
	long				emc_slope;
	long				emc_offset;
	long				emc_dip_slope;
	long				emc_dip_offset;
	long				emc_xmid;

	struct platform_device		*dev;
	struct clk			*clk_3d;
	struct clk			*clk_3d2;
	struct clk			*clk_3d_emc;

	struct devfreq_dev_status	*dev_stat;

	ktime_t				last_request_time;

	int				last_event_type;
};

/*******************************************************************************
 * Static variables for holding the device specific data
 ******************************************************************************/

static struct power_profile_gr3d power_profile;

/* Convert struct clk to clk index */
static inline int clk_to_idx(struct clk *clk)
{
	struct nvhost_device_data *pdata =
		platform_get_drvdata(power_profile.dev);
	int i;

	for (i = 0; i < NVHOST_MODULE_MAX_CLOCKS; i++) {
		if (clk == pdata->clk[i])
			break;
	}

	if (i == NVHOST_MODULE_MAX_CLOCKS)
		pr_err("scale3d: there is no such clk!\n");

	return i;
}

/*******************************************************************************
 * Export load info in sysfs
 ******************************************************************************/

static ssize_t nvhost_scale3d_load_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct nvhost_master *host = nvhost_get_host(to_platform_device(dev));
	u32 avg;
	ssize_t res;

	actmon_op().read_avg_norm(host, &avg);
	res = snprintf(buf, PAGE_SIZE, "%u\n", avg);

	return res;
}

static DEVICE_ATTR(load, S_IRUGO, nvhost_scale3d_load_show, NULL);

/*******************************************************************************
 * nvhost_scale3d_notify(dev, busy)
 *
 * Calling this function informs that gr3d is idling (..or busy). This data is
 * used to estimate the current load
 ******************************************************************************/

static void nvhost_scale3d_notify(struct platform_device *dev, int busy)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);
	struct devfreq *df = pdata->power_manager;

	/* If defreq is disabled, do nothing */
	if (!df) {

		/* Ok.. make sure the 3d gets highest frequency always */
		if (busy) {
			unsigned long freq = power_profile.max_rate_3d;
			nvhost_scale3d_target(&dev->dev, &freq, 0);
		}

		return;
	}

	mutex_lock(&df->lock);
	power_profile.last_event_type = busy ? DEVICE_BUSY : DEVICE_IDLE;
	update_devfreq(df);
	mutex_unlock(&df->lock);
}

void nvhost_scale3d_actmon_notify_idle(struct platform_device *dev)
{
	nvhost_scale3d_notify(dev, 0);
}

void nvhost_scale3d_actmon_notify_busy(struct platform_device *dev)
{
	nvhost_scale3d_notify(dev, 1);
}

/*******************************************************************************
 * nvhost_scale3d_target(dev, *freq, flags)
 *
 * This function scales the clock
 ******************************************************************************/

static int nvhost_scale3d_target(struct device *d, unsigned long *freq,
				u32 flags)
{
	long hz;
	long after;

	/* Inform that the clock is disabled */
	if (!tegra_is_clk_enabled(power_profile.clk_3d)) {
		*freq = 0;
		return 0;
	}

	/* Limit the frequency */
	if (*freq < power_profile.min_rate_3d)
		*freq = power_profile.min_rate_3d;
	else if (*freq > power_profile.max_rate_3d)
		*freq = power_profile.max_rate_3d;

	/* Check if we're already running at the desired speed */
	if (*freq == clk_get_rate(power_profile.clk_3d))
		return 0;

	/* Set GPU clockrate */
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA3)
		nvhost_module_set_devfreq_rate(power_profile.dev,
					clk_to_idx(power_profile.clk_3d2), 0);
	nvhost_module_set_devfreq_rate(power_profile.dev,
				clk_to_idx(power_profile.clk_3d), *freq);

	/* Set EMC clockrate */
	after = (long) clk_get_rate(power_profile.clk_3d);
	after = INT_TO_FX(HZ_TO_MHZ(after));
	hz = FXMUL(after, power_profile.emc_slope) +
		power_profile.emc_offset;

	hz -= FXMUL(power_profile.emc_dip_slope,
		FXMUL(after - power_profile.emc_xmid,
			after - power_profile.emc_xmid)) +
		power_profile.emc_dip_offset;

	hz = MHZ_TO_HZ(FX_TO_INT(hz + FX_HALF)); /* round to nearest */
	hz = (hz < 0) ? 0 : hz;

	nvhost_module_set_devfreq_rate(power_profile.dev,
			clk_to_idx(power_profile.clk_3d_emc), hz);

	/* Get the new clockrate */
	*freq = clk_get_rate(power_profile.clk_3d);

	return 0;
}

/*******************************************************************************
 * nvhost_scale3d_get_dev_status(dev, *stat)
 *
 * This function queries the current device status. *stat will have its private
 * field set to an instance of nvhost_devfreq_ext_stat.
 ******************************************************************************/

static int nvhost_scale3d_get_dev_status(struct device *d,
		      struct devfreq_dev_status *stat)
{
	struct platform_device *dev = to_platform_device(d);
	struct nvhost_master *host_master = nvhost_get_host(dev);
	struct nvhost_devfreq_ext_stat *ext_stat =
		power_profile.dev_stat->private_data;
	u32 avg = 0;
	ktime_t t;

	/* Make sure there are correct values for the current frequency */
	power_profile.dev_stat->current_frequency =
		clk_get_rate(power_profile.clk_3d);

	/* Copy the contents of the current device status */
	ext_stat->busy = power_profile.last_event_type;
	*stat = *power_profile.dev_stat;

	/* Read and scale AVG. AVG is scaled to interval 0-dt, where dt
	 * is the last time it was read. (this is really clumsy, but the
	 * governor uses internally time differences) */
	actmon_op().read_avg_norm(host_master, &avg);
	t = ktime_get();
	stat->total_time = ktime_us_delta(t, power_profile.last_request_time);
	stat->busy_time = (avg * stat->total_time) / 1000;
	power_profile.last_request_time = t;

	/* Finally, clear out the local values */
	power_profile.dev_stat->total_time = 0;
	power_profile.dev_stat->busy_time = 0;
	power_profile.last_event_type = DEVICE_UNKNOWN;

	tegra_edp_notify_gpu_load(avg);

	return 0;
}

static struct devfreq_dev_profile nvhost_scale3d_devfreq_profile = {
	.target         = nvhost_scale3d_target,
	.get_dev_status = nvhost_scale3d_get_dev_status,
};

/*******************************************************************************
 * nvhost_scale3d_calibrate_emc()
 *
 * Compute emc scaling parameters
 *
 * Remc = S * R3d + O - (Sd * (R3d - Rm)^2 + Od)
 *
 * Remc - 3d.emc rate
 * R3d  - 3d.cbus rate
 * Rm   - 3d.cbus 'middle' rate = (max + min)/2
 * S    - emc_slope
 * O    - emc_offset
 * Sd   - emc_dip_slope
 * Od   - emc_dip_offset
 *
 * this superposes a quadratic dip centered around the middle 3d
 * frequency over a linear correlation of 3d.emc to 3d clock
 * rates.
 *
 * S, O are chosen so that the maximum 3d rate produces the
 * maximum 3d.emc rate exactly, and the minimum 3d rate produces
 * at least the minimum 3d.emc rate.
 *
 * Sd and Od are chosen to produce the largest dip that will
 * keep 3d.emc frequencies monotonously decreasing with 3d
 * frequencies. To achieve this, the first derivative of Remc
 * with respect to R3d should be zero for the minimal 3d rate:
 *
 *   R'emc = S - 2 * Sd * (R3d - Rm)
 *   R'emc(R3d-min) = 0
 *   S = 2 * Sd * (R3d-min - Rm)
 *     = 2 * Sd * (R3d-min - R3d-max) / 2
 *   Sd = S / (R3d-min - R3d-max)
 *
 *   +---------------------------------------------------+
 *   | Sd = -(emc-max - emc-min) / (R3d-min - R3d-max)^2 |
 *   +---------------------------------------------------+
 *
 *   dip = Sd * (R3d - Rm)^2 + Od
 *
 * requiring dip(R3d-min) = 0 and dip(R3d-max) = 0 gives
 *
 *   Sd * (R3d-min - Rm)^2 + Od = 0
 *   Od = -Sd * ((R3d-min - R3d-max) / 2)^2
 *      = -Sd * ((R3d-min - R3d-max)^2) / 4
 *
 *   +------------------------------+
 *   | Od = (emc-max - emc-min) / 4 |
 *   +------------------------------+
 *
 ******************************************************************************/

static void nvhost_scale3d_calibrate_emc(void)
{
	long correction;
	unsigned long max_emc;
	unsigned long min_emc;
	unsigned long min_rate_3d;
	unsigned long max_rate_3d;

	max_emc = clk_round_rate(power_profile.clk_3d_emc, UINT_MAX);
	max_emc = INT_TO_FX(HZ_TO_MHZ(max_emc));

	min_emc = clk_round_rate(power_profile.clk_3d_emc, 0);
	min_emc = INT_TO_FX(HZ_TO_MHZ(min_emc));

	max_rate_3d = INT_TO_FX(HZ_TO_MHZ(power_profile.max_rate_3d));
	min_rate_3d = INT_TO_FX(HZ_TO_MHZ(power_profile.min_rate_3d));

	power_profile.emc_slope =
		FXDIV((max_emc - min_emc), (max_rate_3d - min_rate_3d));
	power_profile.emc_offset = max_emc -
		FXMUL(power_profile.emc_slope, max_rate_3d);
	/* Guarantee max 3d rate maps to max emc rate */
	power_profile.emc_offset += max_emc -
		(FXMUL(power_profile.emc_slope, max_rate_3d) +
		power_profile.emc_offset);

	power_profile.emc_dip_offset = (max_emc - min_emc) / 4;
	power_profile.emc_dip_slope =
		-4 * FXDIV(power_profile.emc_dip_offset,
		(FXMUL(max_rate_3d - min_rate_3d,
			max_rate_3d - min_rate_3d)));
	power_profile.emc_xmid = (max_rate_3d + min_rate_3d) / 2;
	correction =
		power_profile.emc_dip_offset +
			FXMUL(power_profile.emc_dip_slope,
			FXMUL(max_rate_3d - power_profile.emc_xmid,
				max_rate_3d - power_profile.emc_xmid));
	power_profile.emc_dip_offset -= correction;
}

/*******************************************************************************
 * nvhost_scale3d_init(dev)
 *
 * Initialise 3d clock scaling for the given device. This function installs
 * pod_scaling governor to handle the clock scaling.
 ******************************************************************************/

void nvhost_scale3d_actmon_init(struct platform_device *dev)
{
	struct nvhost_devfreq_ext_stat *ext_stat;
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	if (power_profile.init)
		return;

	/* Get clocks */
	power_profile.dev = dev;
	power_profile.clk_3d = pdata->clk[0];
	if (tegra_get_chipid() == TEGRA_CHIPID_TEGRA3) {
		power_profile.clk_3d2 = pdata->clk[1];
		power_profile.clk_3d_emc = pdata->clk[2];
	} else
		power_profile.clk_3d_emc = pdata->clk[1];

	/* Get frequency settings */
	power_profile.max_rate_3d =
		clk_round_rate(power_profile.clk_3d, UINT_MAX);
	power_profile.min_rate_3d =
		clk_round_rate(power_profile.clk_3d, 0);

	nvhost_scale3d_devfreq_profile.initial_freq = power_profile.max_rate_3d;

	if (power_profile.max_rate_3d == power_profile.min_rate_3d) {
		pr_warn("scale3d: 3d max rate = min rate (%lu), disabling\n",
			power_profile.max_rate_3d);
		goto err_bad_power_profile;
	}

	/* Reserve space for devfreq structures (dev_stat and ext_dev_stat) */
	power_profile.dev_stat =
		kzalloc(sizeof(struct power_profile_gr3d), GFP_KERNEL);
	if (!power_profile.dev_stat)
		goto err_devfreq_alloc;
	ext_stat = kzalloc(sizeof(struct nvhost_devfreq_ext_stat), GFP_KERNEL);
	if (!ext_stat)
		goto err_devfreq_ext_stat_alloc;

	/* Initialise the dev_stat and ext_stat structures */
	power_profile.dev_stat->private_data = ext_stat;
	power_profile.last_event_type = DEVICE_UNKNOWN;
	ext_stat->min_freq = power_profile.min_rate_3d;
	ext_stat->max_freq = power_profile.max_rate_3d;

	power_profile.last_request_time = ktime_get();

	nvhost_scale3d_calibrate_emc();

	if (device_create_file(&dev->dev, &dev_attr_load))
		goto err_create_sysfs_entry;

	/* Start using devfreq */
	pdata->power_manager = devfreq_add_device(&dev->dev,
				&nvhost_scale3d_devfreq_profile,
				&nvhost_podgov,
				NULL);

	power_profile.init = 1;
	return;

err_create_sysfs_entry:
err_devfreq_ext_stat_alloc:
	kfree(power_profile.dev_stat);
err_devfreq_alloc:
err_bad_power_profile:

	return;

}

/*******************************************************************************
 * nvhost_scale3d_deinit(dev)
 *
 * Stop 3d scaling for the given device.
 ******************************************************************************/

void nvhost_scale3d_actmon_deinit(struct platform_device *dev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(dev);

	if (!power_profile.init)
		return;

	if (pdata->power_manager)
		devfreq_remove_device(pdata->power_manager);

	device_remove_file(&dev->dev, &dev_attr_load);

	kfree(power_profile.dev_stat->private_data);
	kfree(power_profile.dev_stat);

	power_profile.init = 0;
}

