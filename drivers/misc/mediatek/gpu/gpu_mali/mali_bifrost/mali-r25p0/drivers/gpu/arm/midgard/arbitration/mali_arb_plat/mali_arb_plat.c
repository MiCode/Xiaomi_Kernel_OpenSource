// SPDX-License-Identifier: GPL-2.0

/*
 *
 * (C) COPYRIGHT 2019-2020 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU license.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 *
 */

/**
 * @file
 * Part of the Mali arbiter interface related to platform operations.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/devfreq.h>
#include <linux/clk.h>
#include <linux/pm_opp.h>
#include <linux/version.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <linux/mutex.h>
#include <gpu/mali_arb_plat.h>

/**
 * How long pm runtime should wait in milliseconds before suspending GPU
 */
#define AUTO_SUSPEND_DELAY (100)

/**
 * struct mali_arb_plat - Internal DVFS Integrator state information
 * @mutex:                  Protect DVFS callback and arbiter callback threads
 * @arb_plat_dev:           Embedded public DVFS integrator device data.
 * @dev:                    Device for DVFS integrator
 * @arbiter_dev:            The arbiter device that we can query about GPU
 *                          utilization
 * @arbiter_ops:            callback(s) for communicating with arbiter,
 *                          to query utilization
 * @clock:                  Pointer to the input clock resource
 *                          (having an id of 0),
 *                          referenced by the GPU device node.
 * @devfreq_profile:        devfreq profile for the Mali GPU device,
 *                          passed to devfreq_add_device()
 *                          to add devfreq feature to Mali GPU device.
 * @devfreq:                Pointer to devfreq structure for Mali GPU device,
 *                          returned on the call to devfreq_add_device().
 * @current_freq:           The real frequency, corresponding
 *                          to @current_nominal_freq, at which the
 *                          Mali GPU device is currently operating, as
 *                          retrieved from @opp_table in the target callback of
 *                          @devfreq_profile.
 * @current_nominal_freq:   The nominal frequency currently used for
 *                          the Mali GPU device as retrieved through
 *                          devfreq_recommended_opp() using the freq value
 *                          passed as an argument to target callback
 *                          of @devfreq_profile
 * @current_voltage:        The voltage corresponding to @current_nominal_freq,
 *                          as retrieved through dev_pm_opp_get_voltage().
 * @supports_power_mgmt:    Whether or not the platform supports
 *                          power management (for example has power domains)
 *
 * Structure used to handle DVFS information
 */
struct mali_arb_plat {
	struct mutex mutex;
	struct mali_arb_plat_dev arb_plat_dev;
	struct device *dev;
	struct device *arbiter_dev;
	struct mali_arb_plat_arbiter_cb_ops arbiter_ops;
	struct devfreq *devfreq;
	struct devfreq_dev_profile devfreq_profile;

	struct clk *clock;
	unsigned long current_freq;
	unsigned long current_nominal_freq;
	unsigned long current_voltage;
#ifdef CONFIG_REGULATOR
	struct regulator *regulator;
#endif
	bool supports_power_mgmt;
};

/**
 * arb_plat_from_pdev() - Convert arb_plat_dev to arb_plat
 * @arb_plat_dev: The arbiter platform device
 *
 * Extract a pointer to the struct mali_arb_plat from the mali_arb_plat_dev
 *
 * Return: arb_plat or NULL if input parameter was NULL
 */
static inline struct mali_arb_plat *arb_plat_from_pdev(
		struct mali_arb_plat_dev *arb_plat_dev)
{
	if (likely(arb_plat_dev))
		return container_of(arb_plat_dev, struct mali_arb_plat,
				arb_plat_dev);
	return NULL;
}

/**
 * devfreq_target() - Modify the target frequency of the device
 * @dev: Device of which the frequency should be updated
 * @target_freq: Minimum frequency that should be set into the device
 * @flags: Additional flags
 *
 * This function should pick a frequency at least as high as the passed
 * in *target_freq, then update *target_freq to reflect the actual
 * frequency chosen
 *
 * Return: 0 if success or an error code
 */
static int devfreq_target(struct device *dev, unsigned long *target_freq,
		u32 flags)
{
	int err;
	unsigned long new_freq = 0;
	struct dev_pm_opp *new_opp;
	unsigned long new_voltage;

	struct mali_arb_plat *arb_plat =
		arb_plat_from_pdev(dev_get_drvdata(dev));

#if  KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	rcu_read_lock();
#endif
	new_opp = devfreq_recommended_opp(dev, target_freq, flags);
	new_voltage = dev_pm_opp_get_voltage(new_opp);
	new_freq = dev_pm_opp_get_freq(new_opp);

#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	rcu_read_unlock();
#endif
	if (IS_ERR_OR_NULL(new_opp)) {
		dev_err(dev, "Failed to get opp %ld\n", PTR_ERR(new_opp));
		return PTR_ERR(new_opp);
	}

#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
	dev_pm_opp_put(new_opp);
#endif
#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	rcu_read_lock();

	rcu_read_unlock();
#endif
	/* Only update if there is a change of frequency */
	if (arb_plat->current_nominal_freq == new_freq) {
		*target_freq = new_freq;
		return 0;
	}

#ifdef CONFIG_REGULATOR
	if (arb_plat->regulator && arb_plat->current_voltage != new_voltage
			&& arb_plat->current_freq < new_freq) {
		err = regulator_set_voltage(arb_plat->regulator,
				new_voltage, new_voltage);
		if (err) {
			dev_err(dev, "Failed to increase voltage %d\n", err);
			return err;
		}
		arb_plat->current_voltage = new_voltage;
		dev_info(dev, "voltage set to %lu", new_voltage);
	}
#endif

	err = clk_set_rate(arb_plat->clock, new_freq);

	if (err) {
		dev_err(arb_plat->dev,
			"arb_plat: clk_set_rate returned error %d\n", err);
		return err;
	}

	if (arb_plat->current_nominal_freq != new_freq) {
		dev_info(arb_plat->dev,
			"arb_plat: clk_set_rate to %lu\n", new_freq);
		arb_plat->current_nominal_freq = new_freq;
	}

#ifdef CONFIG_REGULATOR
	if (arb_plat->regulator && arb_plat->current_voltage != new_voltage
			&& arb_plat->current_freq > new_freq) {
		err = regulator_set_voltage(arb_plat->regulator, new_voltage,
				new_voltage);
		if (err) {
			dev_err(dev, "Failed to decrease voltage %d\n", err);
			return err;
		}
		arb_plat->current_voltage = new_voltage;
		dev_info(dev, "voltage set to %lu", new_voltage);
	}
#endif

	return 0;
}

static int devfreq_status(struct device *dev, struct devfreq_dev_status *stat)
{
	struct mali_arb_plat *arb_plat = arb_plat_from_pdev(
						dev_get_drvdata(dev));

	u32 busy_time, total_time;

	mutex_lock(&arb_plat->mutex);
	stat->current_frequency = arb_plat->current_nominal_freq;
	mutex_unlock(&arb_plat->mutex);

	/* report busy time since last call */
	arb_plat->arbiter_ops.dvfs_arb_get_utilisation(arb_plat->arbiter_dev,
			&busy_time, &total_time);
	mutex_lock(&arb_plat->mutex);
	stat->busy_time = busy_time;
	stat->total_time = total_time;
	dev_dbg(arb_plat->dev, "arb_plat: %s busy_time = %lu, total_time = %lu (~ %lu%%)\n",
			__func__, stat->busy_time, stat->total_time,
			(stat->busy_time*100)/stat->total_time);
	stat->current_frequency = arb_plat->current_nominal_freq;
	stat->private_data = NULL;
	mutex_unlock(&arb_plat->mutex);

	return 0;
}

static void devfreq_exit(struct device *dev)
{
	struct mali_arb_plat *arb_plat = arb_plat_from_pdev(
						dev_get_drvdata(dev));
	struct devfreq_dev_profile *dp;

	dev_info(dev, "arb_plat: %s\n", __func__);

	dp = &arb_plat->devfreq_profile;
	if (dp && dp->freq_table) {
		devm_kfree(dev, dp->freq_table);
		dp->freq_table = NULL;
	}
}

static int devfreq_cur_freq(struct device *dev, unsigned long *freq)
{
	struct mali_arb_plat *arb_plat = arb_plat_from_pdev(
						dev_get_drvdata(dev));

	dev_dbg(arb_plat->dev, "arb_plat: %s %lu\n", __func__,
			arb_plat->current_nominal_freq);

	mutex_lock(&arb_plat->mutex);
	*freq = arb_plat->current_nominal_freq;
	mutex_unlock(&arb_plat->mutex);

	return 0;
}

static int register_arb_dev(struct mali_arb_plat_dev *arb_plat_dev,
		struct device *dev, struct mali_arb_plat_arbiter_cb_ops *ops)
{
	struct mali_arb_plat *arb_plat = arb_plat_from_pdev(arb_plat_dev);

	dev_info(arb_plat->dev, "arb_plat: %s\n", __func__);

	if (!arb_plat || !dev || !ops)
		return -EFAULT;

	mutex_lock(&arb_plat->mutex);
	/* we'll pass this when we call arbiter to ask for utilization. */
	arb_plat->arbiter_dev = dev;
	memcpy(&arb_plat->arbiter_ops, ops, sizeof(arb_plat->arbiter_ops));
	mutex_unlock(&arb_plat->mutex);
	return 0;
}

static void unregister_arb_dev(struct mali_arb_plat_dev *arb_plat_dev)
{
	struct mali_arb_plat *arb_plat = arb_plat_from_pdev(arb_plat_dev);

	dev_info(arb_plat->dev, "arb_plat: %s\n", __func__);
	mutex_lock(&arb_plat->mutex);
	arb_plat->arbiter_dev = NULL;
	mutex_unlock(&arb_plat->mutex);
}

static int devfreq_init_freq_table_locked(struct mali_arb_plat *arb_plat,
		struct devfreq_dev_profile *dp)
{
	int count;
	int i = 0;
	unsigned long freq;
	struct dev_pm_opp *opp;

#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	rcu_read_lock();
#endif

	count = dev_pm_opp_get_opp_count(arb_plat->dev);
	dev_dbg(arb_plat->dev, "opp count = %d", count);
#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	rcu_read_unlock();
#endif
	if (count < 0)
		return count;

	dp->freq_table = devm_kmalloc_array(arb_plat->dev, count,
				sizeof(dp->freq_table[0]), GFP_KERNEL);
	if (!dp->freq_table)
		return -ENOMEM;

#if  KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	rcu_read_lock();
#endif
	for (i = 0, freq = ULONG_MAX; i < count; i++, freq--) {
		opp = dev_pm_opp_find_freq_floor(arb_plat->dev, &freq);
		if (IS_ERR(opp))
			break;

#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
		dev_pm_opp_put(opp);
#endif

		dp->freq_table[i] = freq;
	}
#if KERNEL_VERSION(4, 11, 0) > LINUX_VERSION_CODE
	rcu_read_unlock();
#endif

	if (count != i) {
		dev_warn(arb_plat->dev,
			"Unable to enumerate all OPPs %d!=%d\n", count, i);
	}

	dp->max_state = i;

	return 0;
}

/**
 * start_dvfs() - Register the device with devfreq framework
 * @arb_plat_dev: The arbiter platform device
 *
 * Read device tree and register with the devfreq framework
 * see @kbase_devfreq_init and @power_control_init in the DDK
 *
 * Return: 0 if success or an error code
 */
static int start_dvfs(struct mali_arb_plat_dev *arb_plat_dev)
{
	int err;
	struct devfreq_dev_profile *dp;
	struct mali_arb_plat *arb_plat = arb_plat_from_pdev(arb_plat_dev);

	dev_dbg(arb_plat->dev, "%s\n", __func__);

	if (!arb_plat) {
		pr_alert("arb_plat_from_pdev returned NULL");
		return -EFAULT;
	}

	mutex_lock(&arb_plat->mutex);
	if (!arb_plat->clock) {
		dev_err(arb_plat->dev, "Clock not available for devfreq\n");
		err = 0;
		goto cleanup_mutex;
	}

	arb_plat->current_freq = clk_get_rate(arb_plat->clock);
	dev_info(arb_plat->dev, "%s - current_freq = %lu\n", __func__,
			arb_plat->current_freq);
	arb_plat->current_nominal_freq = arb_plat->current_freq;

	dp = &arb_plat->devfreq_profile;

	dp->initial_freq = arb_plat->current_nominal_freq;
	dp->polling_ms = 100;	/* DEFAULT_PM_DVFS_PERIOD */
	dp->target = devfreq_target;
	dp->get_dev_status = devfreq_status;
	dp->get_cur_freq = devfreq_cur_freq;
	dp->exit = devfreq_exit;

	if (devfreq_init_freq_table_locked(arb_plat, dp)) {
		dev_err(arb_plat->dev,
			"devfreq_init_freq_table_locked failed\n");
		err = -EFAULT;
		goto cleanup_mutex;
	}

	arb_plat->devfreq = devfreq_add_device(arb_plat->dev, dp,
				"simple_ondemand", NULL);
	if (IS_ERR(arb_plat->devfreq)) {
		err = PTR_ERR(arb_plat->devfreq);
		goto cleanup_freqtable;
	}
	dev_dbg(arb_plat->dev, "%s - added devfreq device\n", __func__);

	err = devfreq_register_opp_notifier(arb_plat->dev, arb_plat->devfreq);
	if (err) {
		dev_err(arb_plat->dev,
			"Failed to register OPP notifier %d\n", err);
		goto cleanup_device;
	}

	dev_dbg(arb_plat->dev,
		"arb_plat: init - registered devfreq device as opp notifier\n");
	mutex_unlock(&arb_plat->mutex);
	return 0;

cleanup_device:
	if (devfreq_remove_device(arb_plat->devfreq))
		dev_warn(arb_plat->dev, "Failed to terminate devfreq %d\n",
				err);
	arb_plat->devfreq = NULL;
cleanup_freqtable:
	dev_set_drvdata(arb_plat->dev, NULL);
	if (dp->freq_table) {
		devm_kfree(arb_plat->dev, dp->freq_table);
		dp->freq_table = NULL;
	}
cleanup_mutex:
	mutex_unlock(&arb_plat->mutex);
	return err;

}

/**
 * stop_dvfs() - Unregister the device with devfreq framework
 * @arb_plat_dev: The arbiter platform device
 *
 * Unregister the device from the devfreq framework and release the resources
 */
static void stop_dvfs(struct mali_arb_plat_dev *arb_plat_dev)
{
	int err;
	struct devfreq_dev_profile *dp;
	struct mali_arb_plat *arb_plat = arb_plat_from_pdev(arb_plat_dev);

	dev_dbg(arb_plat->dev, "Term Mali Arbiter devfreq\n");
	if (!arb_plat->clock)
		return;

	mutex_lock(&arb_plat->mutex);
	devfreq_unregister_opp_notifier(arb_plat->dev, arb_plat->devfreq);
	err = devfreq_remove_device(arb_plat->devfreq);
	if (err)
		dev_err(arb_plat->dev, "Failed to terminate devfreq %d\n",
				err);

	arb_plat->devfreq = NULL;
	dp = &arb_plat->devfreq_profile;
	if (dp->freq_table) {
		devm_kfree(arb_plat->dev, dp->freq_table);
		dp->freq_table = NULL;
	}
	mutex_unlock(&arb_plat->mutex);
}

/**
 * power_on() - Power on the GPU
 * @arb_plat_dev: The arbiter platform device
 *
 * Enable the GPU power to the device
 */
static void power_on(struct mali_arb_plat_dev *arb_plat_dev)
{
	int err;

	struct mali_arb_plat *arb_plat = arb_plat_from_pdev(arb_plat_dev);

	if (!arb_plat->supports_power_mgmt)
		return;

	dev_dbg(arb_plat->dev, "Enabling GPU power...\n");
	mutex_lock(&arb_plat->mutex);

	err = clk_enable(arb_plat->clock);
	if (err < 0)
		dev_err(arb_plat->dev, "clk_enable returned %d\n",
				err);
	else
		dev_dbg(arb_plat->dev, "clk_enable returned %d\n",
				err);

	err = pm_runtime_get_sync(arb_plat->dev);
	if (err < 0)
		dev_err(arb_plat->dev, "pm_runtime_get_sync returned %d\n",
				err);
	else
		dev_dbg(arb_plat->dev, "pm_runtime_get_sync returned %d\n",
				err);

	mutex_unlock(&arb_plat->mutex);
}

/**
 * power_off() - Power off the GPU
 * @arb_plat_dev: The arbiter platform device
 *
 * Disable the GPU power of the device
 */
static void power_off(struct mali_arb_plat_dev *arb_plat_dev)
{
	int err;

	struct mali_arb_plat *arb_plat = arb_plat_from_pdev(arb_plat_dev);

	if (!arb_plat->supports_power_mgmt)
		return;

	dev_dbg(arb_plat->dev, "Disabling GPU power...\n");
	mutex_lock(&arb_plat->mutex);

	pm_runtime_mark_last_busy(arb_plat->dev);
	err = pm_runtime_put_autosuspend(arb_plat->dev);
	if (err < 0)
		dev_err(arb_plat->dev,
			"pm_runtime_put_autosuspend returned %d\n", err);
	else
		dev_dbg(arb_plat->dev,
			"pm_runtime_put_autosuspend returned %d\n", err);

	clk_disable(arb_plat->clock);
	mutex_unlock(&arb_plat->mutex);
}

/**
 * arb_plat_probe() - Called when device is matched in device tree
 * @pdev: Platform device to probe
 *
 * Register the device in the system and initialize several power
 * related resources
 *
 * Return: 0 if success or an error code
 */
static int arb_plat_probe(struct platform_device *pdev)
{
	int err;
	struct device *dev = &pdev->dev;
	struct mali_arb_plat *arb_plat;

	dev_info(&pdev->dev, "%s\n", __func__);
	arb_plat = devm_kzalloc(dev, sizeof(struct mali_arb_plat), GFP_KERNEL);
	if (!arb_plat)
		return -ENOMEM;

	arb_plat->dev = dev;
	mutex_init(&arb_plat->mutex);

#if defined(CONFIG_OF) && defined(CONFIG_REGULATOR)
	arb_plat->regulator = regulator_get_optional(&pdev->dev, "mali");
	if (IS_ERR_OR_NULL(arb_plat->regulator)) {
		err = PTR_ERR(arb_plat->regulator);
		arb_plat->regulator = NULL;
		if (err == -EPROBE_DEFER) {
			dev_err(&pdev->dev, "Failed to get regulator\n");
			goto fail;
		}
		dev_info(arb_plat->dev,
			"Continuing without Mali regulator control\n");
	}
		dev_info(arb_plat->dev,
			"Arbiter doing Mali regulator control\n");
#endif /* defined(CONFIG_OF) && defined(CONFIG_REGULATOR) */

#if defined(CONFIG_OF)
	/* find the GPU clock via Device Tree and enable */
	arb_plat->clock = of_clk_get(arb_plat->dev->of_node, 0);
	if (IS_ERR_OR_NULL(arb_plat->clock)) {
		err = PTR_ERR(arb_plat->clock);
		arb_plat->clock = NULL;
		if (err == -EPROBE_DEFER) {
			dev_err(&pdev->dev, "Failed to get clock\n");
			goto fail;
		}
		dev_info(arb_plat->dev,
			"Continuing without Mali clock control\n");

	} else {
		err = clk_prepare(arb_plat->clock);
		if (err) {
			dev_err(arb_plat->dev, "Failed to prepare clock %d\n",
				err);
			goto fail;
		}
	}

	/* Initialize OPP table from Device Tree */
	if (dev_pm_opp_of_add_table(arb_plat->dev))
		dev_warn(arb_plat->dev,
			"Invalid operating-points in device tree.\n");
	else
		dev_info(arb_plat->dev,
			"arb_plat: opp table initialised from device tree operating-points.\n");

	/* Discover whether power management is supported
	 * by looking for power domain in Device Tree.
	 */
	arb_plat->supports_power_mgmt = of_find_property(arb_plat->dev->of_node,
						"power-domains", NULL);
	if (arb_plat->supports_power_mgmt) {
		dev_info(arb_plat->dev,
				"Platform supports power management.\n");
		pm_runtime_set_autosuspend_delay(dev, AUTO_SUSPEND_DELAY);
		pm_runtime_use_autosuspend(dev);
		pm_runtime_set_active(dev);
		pm_runtime_enable(dev);
		if (!pm_runtime_enabled(dev))
			dev_warn(dev, "pm_runtime not enabled!");
	} else
		dev_info(arb_plat->dev,
			"Platform does not support power management.\n");

#else
	arb_plat->clock = NULL;
	arb_plat->supports_power_mgmt = false;
#endif

	arb_plat->arb_plat_dev.plat_ops.mali_arb_plat_register =
							register_arb_dev;
	arb_plat->arb_plat_dev.plat_ops.mali_arb_plat_unregister =
							unregister_arb_dev;
	arb_plat->arb_plat_dev.plat_ops.mali_arb_start_dvfs = start_dvfs;
	arb_plat->arb_plat_dev.plat_ops.mali_arb_stop_dvfs = stop_dvfs;
	arb_plat->arb_plat_dev.plat_ops.mali_arb_gpu_power_on = power_on;
	arb_plat->arb_plat_dev.plat_ops.mali_arb_gpu_power_off = power_off;

	platform_set_drvdata(pdev, &arb_plat->arb_plat_dev);
	return 0;

fail:
	devm_kfree(&pdev->dev, arb_plat);
	return err;
}

/**
 * arb_plat_cleanup() - Free up DVFS resources
 * @arb_plat: The arbiter platform device to cleanup
 *
 * Called to free up and cleanup all the DVFS resources
 */
static void arb_plat_cleanup(struct mali_arb_plat *arb_plat)
{
	struct devfreq_dev_profile *dp;

	dev_dbg(arb_plat->dev, "%s\n", __func__);
	mutex_lock(&arb_plat->mutex);
#if defined(CONFIG_OF)
	dev_pm_opp_of_remove_table(arb_plat->dev);

	clk_unprepare(arb_plat->clock);

	dp = &arb_plat->devfreq_profile;
	if (dp->freq_table) {
		devm_kfree(arb_plat->dev, dp->freq_table);
		dp->freq_table = NULL;
	}
#endif
	pm_runtime_disable(arb_plat->dev);
	mutex_unlock(&arb_plat->mutex);
}

/**
 * arb_plat_remove() - Remove the platform device
 * @pdev: Platform device
 *
 * Called when device is removed
 */
static int arb_plat_remove(struct platform_device *pdev)
{
	struct mali_arb_plat_dev *arb_plat_dev;

	arb_plat_dev = platform_get_drvdata(pdev);
	platform_set_drvdata(pdev, NULL);
	if (arb_plat_dev) {
		struct mali_arb_plat *arb_plat =
					 arb_plat_from_pdev(arb_plat_dev);

		arb_plat_cleanup(arb_plat);
		devm_kfree(&pdev->dev, arb_plat);
	}
	dev_info(&pdev->dev, "dvfs_integrator_remove done\n");
	return 0;
}

/**
 * @arb_plat_dt_match: Match the platform device with the Device Tree.
 */
static const struct of_device_id arb_plat_dt_match[] = {
	{ .compatible = MALI_ARB_PLAT_DT_NAME },
	{/* sentinel */}
};

/**
 * @arb_plat_driver: Platform driver data.
 */
static struct platform_driver arb_plat_driver = {
	.probe = arb_plat_probe,
	.remove = arb_plat_remove,
	.driver = {
		.name = "mali arbiter platform driver",
		.of_match_table = arb_plat_dt_match,
	},
};

/**
 * arb_plat_init() - Register platform driver
 *
 * Register the platform driver during the init process
 */
static int __init arb_plat_init(void)
{
	return platform_driver_register(&arb_plat_driver);
}
module_init(arb_plat_init);

/**
 * arb_plat_exit() - Unregister platform driver
 *
 * Unregister the platform driver when no longer needed
 */
static void __exit arb_plat_exit(void)
{
	platform_driver_unregister(&arb_plat_driver);
}
module_exit(arb_plat_exit);

MODULE_LICENSE("GPL");
MODULE_ALIAS("mali-arbiter-plat-integration-driver");
