/*
 * Tegra Graphics Host Unit clock scaling
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

#include <linux/devfreq.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/clk.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/clk/tegra.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/tegra-soc.h>
#include <linux/tegra-soc.h>
#include <linux/platform_data/tegra_edp.h>
#include <linux/pm_qos.h>

#include <governor.h>

#include "dev.h"
#include "chip_support.h"
#include "nvhost_acm.h"
#include "nvhost_scale.h"
#include "host1x/host1x_actmon.h"

static ssize_t nvhost_scale_load_show(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	struct nvhost_device_data *pdata = dev_get_drvdata(dev);
	struct nvhost_device_profile *profile = pdata->power_profile;
	u32 busy_time;
	ssize_t res;

	actmon_op().read_avg_norm(profile->actmon, &busy_time);
	res = snprintf(buf, PAGE_SIZE, "%u\n", busy_time);

	return res;
}

static DEVICE_ATTR(load, S_IRUGO, nvhost_scale_load_show, NULL);

/*
 * nvhost_scale_make_freq_table(profile)
 *
 * This function initialises the frequency table for the given device profile
 */

static int nvhost_scale_make_freq_table(struct nvhost_device_profile *profile)
{
	unsigned long *freqs;
	int num_freqs, err;
	unsigned long max_freq =  clk_round_rate(profile->clk, UINT_MAX);
	unsigned long min_freq =  clk_round_rate(profile->clk, 0);

	err = tegra_dvfs_get_freqs(clk_get_parent(profile->clk),
				   &freqs, &num_freqs);
	if (err)
		return -ENOSYS;

	/* check for duplicate frequencies at higher end */
	while ((num_freqs >= 2 &&
		freqs[num_freqs - 2] == freqs[num_freqs - 1]) ||
	       (num_freqs && max_freq < freqs[num_freqs - 1]))
		num_freqs--;

	/* check low end */
	while ((num_freqs >= 2 && freqs[0] == freqs[1]) ||
	       (num_freqs && freqs[0] < min_freq)) {
		freqs++;
		num_freqs--;
	}

	if (!num_freqs)
		dev_warn(&profile->pdev->dev, "dvfs table had no applicable frequencies!\n");

	profile->devfreq_profile.freq_table = (unsigned int *)freqs;
	profile->devfreq_profile.max_state = num_freqs;

	return 0;
}

/*
 * nvhost_scale_target(dev, *freq, flags)
 *
 * This function scales the clock
 */

static int nvhost_scale_target(struct device *dev, unsigned long *freq,
			       u32 flags)
{
	struct nvhost_device_data *pdata = dev_get_drvdata(dev);
	struct nvhost_device_profile *profile = pdata->power_profile;

	if (!tegra_is_clk_enabled(profile->clk)) {
		*freq = profile->ext_stat.min_freq;
		return 0;
	}

	*freq = clk_round_rate(clk_get_parent(profile->clk), *freq);
	if (clk_get_rate(profile->clk) == *freq)
		return 0;

	nvhost_module_set_devfreq_rate(profile->pdev, 0, *freq);
	if (pdata->scaling_post_cb)
		pdata->scaling_post_cb(profile, *freq);

	*freq = clk_get_rate(profile->clk);

	return 0;
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
	unsigned long freq;

	if (!pdata->scaling_post_cb)
		return NOTIFY_OK;

	/* get the frequency requirement. if devfreq is enabled, check if it
	 * has higher demand than qos */
	freq = clk_round_rate(clk_get_parent(profile->clk),
			      pm_qos_request(pdata->qos_id));
	if (pdata->power_manager)
		freq = max(pdata->power_manager->previous_freq, freq);

	pdata->scaling_post_cb(profile, freq);

	return NOTIFY_OK;
}

/*
 * update_load_estimate(profile)
 *
 * Update load estimate using busy/idle flag.
 */

static void update_load_estimate(struct nvhost_device_profile *profile,
				 bool busy)
{
	ktime_t t;
	unsigned long dt;

	t = ktime_get();
	dt = ktime_us_delta(t, profile->last_event_time);

	profile->dev_stat.total_time += dt;
	profile->last_event_time = t;

	if (profile->busy)
		profile->dev_stat.busy_time += dt;

	profile->busy = busy;
}

/*
 * update_load_estimate_actmon(profile)
 *
 * Update load estimate using hardware actmon. The actmon value is normalised
 * based on the time it was asked last time.
 */

static void update_load_estimate_actmon(struct nvhost_device_profile *profile)
{
	ktime_t t;
	unsigned long dt;
	u32 busy_time;

	t = ktime_get();
	dt = ktime_us_delta(t, profile->last_event_time);

	profile->dev_stat.total_time = dt;
	profile->last_event_time = t;
	actmon_op().read_avg_norm(profile->actmon, &busy_time);
	profile->dev_stat.busy_time = (busy_time * dt) / 1000;
}

/*
 * nvhost_scale_notify(pdev, busy)
 *
 * Calling this function informs that the device is idling (..or busy). This
 * data is used to estimate the current load
 */

static void nvhost_scale_notify(struct platform_device *pdev, bool busy)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile = pdata->power_profile;
	struct devfreq *devfreq = pdata->power_manager;

	/* Is the device profile initialised? */
	if (!profile)
		return;

	/* inform edp about new constraint */
	if (pdata->gpu_edp_device) {
		u32 avg = 0;
		actmon_op().read_avg_norm(profile->actmon, &avg);
		tegra_edp_notify_gpu_load(avg);
	}

	/* If defreq is disabled, set the freq to max or min */
	if (!devfreq) {
		unsigned long freq = busy ? UINT_MAX : 0;
		nvhost_scale_target(&pdev->dev, &freq, 0);
		return;
	}

	mutex_lock(&devfreq->lock);
	if (!profile->actmon)
		update_load_estimate(profile, busy);
	profile->last_event_type = busy ? DEVICE_BUSY : DEVICE_IDLE;
	update_devfreq(devfreq);
	mutex_unlock(&devfreq->lock);
}

void nvhost_scale_notify_idle(struct platform_device *pdev)
{
	nvhost_scale_notify(pdev, false);

}

void nvhost_scale_notify_busy(struct platform_device *pdev)
{
	nvhost_scale_notify(pdev, true);
}

/*
 * nvhost_scale_get_dev_status(dev, *stat)
 *
 * This function queries the current device status.
 */

static int nvhost_scale_get_dev_status(struct device *dev,
					      struct devfreq_dev_status *stat)
{
	struct nvhost_device_data *pdata = dev_get_drvdata(dev);
	struct nvhost_device_profile *profile = pdata->power_profile;

	/* Make sure there are correct values for the current frequency */
	profile->dev_stat.current_frequency = clk_get_rate(profile->clk);

	if (profile->actmon)
		update_load_estimate_actmon(profile);

	/* Copy the contents of the current device status */
	profile->ext_stat.busy = profile->last_event_type;
	*stat = profile->dev_stat;

	/* Finally, clear out the local values */
	profile->dev_stat.total_time = 0;
	profile->dev_stat.busy_time = 0;

	return 0;
}

/*
 * nvhost_scale_init(pdev)
 */

void nvhost_scale_init(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile;

	if (pdata->power_profile)
		return;

	profile = kzalloc(sizeof(struct nvhost_device_profile), GFP_KERNEL);
	if (!profile)
		return;
	pdata->power_profile = profile;
	profile->pdev = pdev;
	profile->clk = pdata->clk[0];
	profile->last_event_type = DEVICE_IDLE;

	/* Initialize devfreq related structures */
	profile->dev_stat.private_data = &profile->ext_stat;
	profile->ext_stat.min_freq =
		clk_round_rate(clk_get_parent(profile->clk), 0);
	profile->ext_stat.max_freq =
		clk_round_rate(clk_get_parent(profile->clk), UINT_MAX);
	profile->ext_stat.busy = DEVICE_IDLE;

	if (profile->ext_stat.min_freq == profile->ext_stat.max_freq) {
		dev_warn(&pdev->dev, "max rate = min rate (%lu), disabling scaling\n",
			 profile->ext_stat.min_freq);
		goto err_fetch_clocks;
	}

	/* Initialize actmon */
	if (pdata->actmon_enabled) {

		if (device_create_file(&pdev->dev,
		    &dev_attr_load))
			goto err_create_sysfs_entry;

		profile->actmon = kzalloc(sizeof(struct host1x_actmon),
					  GFP_KERNEL);
		if (!profile->actmon)
			goto err_allocate_actmon;

		profile->actmon->host = nvhost_get_host(pdev);
		profile->actmon->regs = nvhost_get_host(pdev)->aperture +
			pdata->actmon_regs;

		actmon_op().init(profile->actmon);
		actmon_op().debug_init(profile->actmon, pdata->debugfs);
		actmon_op().deinit(profile->actmon);
	}

	if (pdata->devfreq_governor) {
		struct devfreq *devfreq;
		int err;

		profile->devfreq_profile.initial_freq =
			profile->ext_stat.min_freq;
		profile->devfreq_profile.target = nvhost_scale_target;
		profile->devfreq_profile.get_dev_status =
			nvhost_scale_get_dev_status;
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
err_allocate_actmon:
	device_remove_file(&pdev->dev, &dev_attr_load);
err_create_sysfs_entry:
err_fetch_clocks:
	kfree(pdata->power_profile);
	pdata->power_profile = NULL;
}

/*
 * nvhost_scale_deinit(dev)
 *
 * Stop scaling for the given device.
 */

void nvhost_scale_deinit(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile = pdata->power_profile;

	if (!profile)
		return;

	if (pdata->power_manager)
		devfreq_remove_device(pdata->power_manager);

	if (pdata->actmon_enabled)
		device_remove_file(&pdev->dev, &dev_attr_load);

	kfree(profile->devfreq_profile.freq_table);
	kfree(profile->actmon);
	kfree(profile);
	pdata->power_profile = NULL;
}

/*
 * nvhost_scale_hw_init(dev)
 *
 * Initialize hardware portion of the device
 */

int nvhost_scale_hw_init(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile = pdata->power_profile;

	if (profile && profile->actmon)
		actmon_op().init(profile->actmon);

	return 0;
}

/*
 * nvhost_scale_hw_deinit(dev)
 *
 * Deinitialize the hw partition related to scaling
 */

void nvhost_scale_hw_deinit(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);
	struct nvhost_device_profile *profile = pdata->power_profile;

	if (profile && profile->actmon)
		actmon_op().deinit(profile->actmon);
}
