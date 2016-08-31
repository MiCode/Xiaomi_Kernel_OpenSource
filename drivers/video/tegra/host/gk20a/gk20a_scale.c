/*
 * gk20a clock scaling profile
 *
 * Copyright (c) 2013, NVIDIA Corporation. All rights reserved.
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

#include <linux/devfreq.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/clk/tegra.h>
#include <linux/tegra-soc.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/pm_qos.h>

#include <governor.h>

#include "dev.h"
#include "chip_support.h"
#include "nvhost_acm.h"
#include "gk20a.h"
#include "pmu_gk20a.h"
#include "clk_gk20a.h"
#include "nvhost_scale.h"
#include "gk20a_scale.h"
#include "gr3d/scale3d.h"

static ssize_t nvhost_gk20a_scale_load_show(struct device *dev,
					    struct device_attribute *attr,
					    char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gk20a *g = get_gk20a(pdev);
	u32 busy_time;
	ssize_t res;

	if (!g->power_on) {
		busy_time = 0;
	} else {
		gk20a_busy(g->dev);
		gk20a_pmu_load_norm(g, &busy_time);
		gk20a_idle(g->dev);
	}

	res = snprintf(buf, PAGE_SIZE, "%u\n", busy_time);

	return res;
}

static DEVICE_ATTR(load, S_IRUGO, nvhost_gk20a_scale_load_show, NULL);

/*
 * nvhost_gk20a_scale_callback(profile, freq)
 *
 * This function sets emc frequency based on current gpu frequency
 */

void nvhost_gk20a_scale_callback(struct nvhost_device_profile *profile,
				 unsigned long freq)
{
	struct gk20a *g = get_gk20a(profile->pdev);
	struct nvhost_emc_params *emc_params = profile->private_data;
	long after = gk20a_clk_get_rate(g);
	long emc_target = nvhost_scale3d_get_emc_rate(emc_params, after);

	nvhost_module_set_devfreq_rate(profile->pdev, 2, emc_target);
}

/*
 * nvhost_scale_qos_notify()
 *
 * This function is called when the minimum QoS requirement for the device
 * has changed. The function calls postscaling callback if it is defined.
 */

static int nvhost_scale_qos_notify(struct notifier_block *nb,
				   unsigned long n, void *p)
{
	struct nvhost_device_profile *profile =
		container_of(nb, struct nvhost_device_profile,
			     qos_notify_block);
	struct nvhost_device_data *pdata = platform_get_drvdata(profile->pdev);
	struct gk20a *g = get_gk20a(profile->pdev);
	unsigned long freq;

	if (!pdata->scaling_post_cb)
		return NOTIFY_OK;

	/* get the frequency requirement. if devfreq is enabled, check if it
	 * has higher demand than qos */
	freq = gk20a_clk_round_rate(g, pm_qos_request(pdata->qos_id));
	if (pdata->power_manager)
		freq = max(pdata->power_manager->previous_freq, freq);

	pdata->scaling_post_cb(profile, freq);

	return NOTIFY_OK;
}

/*
 * nvhost_scale_make_freq_table(profile)
 *
 * This function initialises the frequency table for the given device profile
 */

static int nvhost_scale_make_freq_table(struct nvhost_device_profile *profile)
{
	struct gk20a *g = get_gk20a(profile->pdev);
	unsigned long *freqs;
	int num_freqs, err;

	/* make sure the clock is available */
	if (!gk20a_clk_get(g))
		return -ENOSYS;

	/* get gpu dvfs table */
	err = tegra_dvfs_get_freqs(clk_get_parent(g->clk.tegra_clk),
				   &freqs, &num_freqs);
	if (err)
		return -ENOSYS;

	profile->devfreq_profile.freq_table = (unsigned int *)freqs;
	profile->devfreq_profile.max_state = num_freqs;

	return 0;
}

/*
 * gk20a_scale_target(dev, *freq, flags)
 *
 * This function scales the clock
 */

static int gk20a_scale_target(struct device *dev, unsigned long *freq,
			      u32 flags)
{
	struct gk20a *g = get_gk20a(to_platform_device(dev));
	struct nvhost_device_data *pdata = dev_get_drvdata(dev);
	struct nvhost_device_profile *profile = pdata->power_profile;
	unsigned long rounded_rate = gk20a_clk_round_rate(g, *freq);

	if (gk20a_clk_get_rate(g) == rounded_rate) {
		*freq = rounded_rate;
		return 0;
	}

	gk20a_clk_set_rate(g, rounded_rate);
	if (pdata->scaling_post_cb)
		pdata->scaling_post_cb(profile, rounded_rate);
	*freq = gk20a_clk_get_rate(g);

	return 0;
}

/*
 * update_load_estimate_gpmu(profile)
 *
 * Update load estimate using gpmu. The gpmu value is normalised
 * based on the time it was asked last time.
 */

static void update_load_estimate_gpmu(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile = pdata->power_profile;
	struct gk20a *g = get_gk20a(pdev);
	unsigned long dt;
	u32 busy_time;
	ktime_t t;

	t = ktime_get();
	dt = ktime_us_delta(t, profile->last_event_time);

	profile->dev_stat.total_time = dt;
	profile->last_event_time = t;
	gk20a_pmu_load_norm(g, &busy_time);
	profile->dev_stat.busy_time = (busy_time * dt) / 1000;
}

/*
 * gk20a_scale_notify(pdev, busy)
 *
 * Calling this function informs that the device is idling (..or busy). This
 * data is used to estimate the current load
 */

static void gk20a_scale_notify(struct platform_device *pdev, bool busy)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile = pdata->power_profile;
	struct devfreq *devfreq = pdata->power_manager;
	struct gk20a *g = get_gk20a(pdev);

	/* inform edp about new constraint */
	if (pdata->gpu_edp_device) {
		u32 avg = 0;
		gk20a_pmu_load_norm(g, &avg);
		tegra_edp_notify_gpu_load(avg);
	}

	/* Is the device profile initialised? */
	if (!(profile && devfreq))
		return;

	mutex_lock(&devfreq->lock);
	profile->last_event_type = busy ? DEVICE_BUSY : DEVICE_IDLE;
	update_devfreq(devfreq);
	mutex_unlock(&devfreq->lock);
}

void nvhost_gk20a_scale_notify_idle(struct platform_device *pdev)
{
	gk20a_scale_notify(pdev, false);

}

void nvhost_gk20a_scale_notify_busy(struct platform_device *pdev)
{
	gk20a_scale_notify(pdev, true);
}

/*
 * gk20a_scale_get_dev_status(dev, *stat)
 *
 * This function queries the current device status.
 */

static int gk20a_scale_get_dev_status(struct device *dev,
				      struct devfreq_dev_status *stat)
{
	struct nvhost_device_data *pdata = dev_get_drvdata(dev);
	struct nvhost_device_profile *profile = pdata->power_profile;
	struct gk20a *g = get_gk20a(to_platform_device(dev));

	/* Make sure there are correct values for the current frequency */
	profile->dev_stat.current_frequency = gk20a_clk_get_rate(g);

	/* Update load estimate */
	update_load_estimate_gpmu(to_platform_device(dev));

	/* Copy the contents of the current device status */
	profile->ext_stat.busy = profile->last_event_type;
	*stat = profile->dev_stat;

	/* Finally, clear out the local values */
	profile->dev_stat.total_time = 0;
	profile->dev_stat.busy_time = 0;

	return 0;
}

/*
 * gk20a_scale_init(pdev)
 */

void nvhost_gk20a_scale_init(struct platform_device *pdev)
{
	struct gk20a *g = get_gk20a(pdev);
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile;
	struct nvhost_emc_params *emc_params;

	if (pdata->power_profile)
		return;

	profile = kzalloc(sizeof(struct nvhost_device_profile), GFP_KERNEL);
	emc_params = kzalloc(sizeof(*emc_params), GFP_KERNEL);
	if (!(profile && emc_params)) {
		kfree(profile);
		kfree(emc_params);
		return;
	}

	profile->pdev = pdev;
	profile->last_event_type = DEVICE_IDLE;
	profile->private_data = emc_params;

	/* Initialize devfreq related structures */
	profile->dev_stat.private_data = &profile->ext_stat;
	profile->ext_stat.min_freq = gk20a_clk_round_rate(g, 0);
	profile->ext_stat.max_freq = gk20a_clk_round_rate(g, UINT_MAX);
	profile->ext_stat.busy = DEVICE_IDLE;

	if (profile->ext_stat.min_freq == profile->ext_stat.max_freq) {
		dev_warn(&pdev->dev, "max rate = min rate (%lu), disabling scaling\n",
			 profile->ext_stat.min_freq);
		goto err_fetch_clocks;
	}

	nvhost_scale3d_calibrate_emc(emc_params,
				     gk20a_clk_get(g), pdata->clk[2],
				     pdata->linear_emc);

	if (device_create_file(&pdev->dev, &dev_attr_load))
		goto err_create_sysfs_entry;

	/* Store device profile so we can access it if devfreq governor
	 * init needs that */
	pdata->power_profile = profile;

	if (pdata->devfreq_governor) {
		struct devfreq *devfreq;
		int err;

		profile->devfreq_profile.initial_freq =
			profile->ext_stat.min_freq;
		profile->devfreq_profile.target = gk20a_scale_target;
		profile->devfreq_profile.get_dev_status =
			gk20a_scale_get_dev_status;
		err = nvhost_scale_make_freq_table(profile);
		if (err)
			goto err_get_freqs;

		devfreq = devfreq_add_device(&pdev->dev,
					&profile->devfreq_profile,
					pdata->devfreq_governor, NULL);

		if (IS_ERR(devfreq))
			devfreq = NULL;

		pdata->power_manager = devfreq;
	}

	/* Should we register QoS callback for this device? */
	if (pdata->qos_id < PM_QOS_NUM_CLASSES &&
	    pdata->qos_id != PM_QOS_RESERVED) {
		profile->qos_notify_block.notifier_call =
			&nvhost_scale_qos_notify;
		pm_qos_add_notifier(pdata->qos_id,
				    &profile->qos_notify_block);
	}

	return;

err_get_freqs:
	device_remove_file(&pdev->dev, &dev_attr_load);
err_create_sysfs_entry:
err_fetch_clocks:
	kfree(pdata->power_profile);
	pdata->power_profile = NULL;
}

/*
 * gk20a_scale_deinit(dev)
 *
 * Stop scaling for the given device.
 */

void nvhost_gk20a_scale_deinit(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile = pdata->power_profile;

	if (!profile)
		return;

	if (pdata->power_manager)
		devfreq_remove_device(pdata->power_manager);

	device_remove_file(&pdev->dev, &dev_attr_load);

	kfree(profile);
	pdata->power_profile = NULL;
}

/*
 * gk20a_scale_hw_init(dev)
 *
 * Initialize hardware portion of the device
 */

void nvhost_gk20a_scale_hw_init(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile = pdata->power_profile;

	/* make sure that scaling has bee initialised */
	if (!profile)
		return;

	profile->dev_stat.total_time = 0;
	profile->last_event_time = ktime_get();
}
