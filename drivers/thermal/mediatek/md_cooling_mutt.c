// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "../thermal_core.h"
#include "md_cooling.h"

/**
 * struct mutt_platform_data - platform data for MD cooling MUTT driver
 * @state_to_cooler_lv: callback function to transfer cooling state
 *			to cooler LV defined by MD
 * @max_lv: max cooler LV supported by MD
 */
struct mutt_platform_data {
	unsigned long (*state_to_cooler_lv)(unsigned long state);
	unsigned long max_lv;
};

struct mutt_driver_data {
	unsigned long current_level;
	struct mutex lock;
	struct platform_device *pdev;
	struct mutt_platform_data *pdata;
};

static unsigned long find_max_mutt_level(unsigned int id, unsigned long lv)
{
	struct md_cooling_device *md_cdev;
	unsigned long final_lv = lv;
	int i, pa_num;

	pa_num = get_pa_num();
	if (pa_num == 1)
		return final_lv;

	for (i = 0; i < pa_num; i++) {
		if (i == id)
			continue;

		md_cdev = get_md_cdev(MD_COOLING_TYPE_MUTT, i);
		if (md_cdev)
			final_lv = max(final_lv, md_cdev->target_level);
	}

	return final_lv;
}

static enum md_status
state_to_md_status(unsigned long state, unsigned long max_lv)
{
	enum md_status status;

	if (state == max_lv)
		status = MD_NO_IMS;
	else if (state == max_lv - 1)
		status = MD_IMS_ONLY;
	else if (state == MD_COOLING_UNLIMITED_LV)
		status = MD_LV_THROTTLE_DISABLED;
	else
		status = MD_LV_THROTTLE_ENABLED;

	return status;
}

static void notify_data_status_changed(
	struct thermal_cooling_device *cdev, unsigned long target, int status)
{
	struct thermal_instance *instance;
	char *thermal_prop[5];
	int i;

	list_for_each_entry(instance, &cdev->thermal_instances, cdev_node) {
		if (instance->target == target) {
			thermal_prop[0] = kasprintf(GFP_KERNEL,
				"NAME=%s", instance->tz->type);
			thermal_prop[1] = kasprintf(GFP_KERNEL,
				"TEMP=%d", instance->tz->temperature);
			thermal_prop[2] = kasprintf(GFP_KERNEL,
				"TRIP=%d", instance->trip);
			thermal_prop[3] = kasprintf(GFP_KERNEL,
				"EVENT=%d", status);
			thermal_prop[4] = NULL;
			kobject_uevent_env(&instance->tz->device.kobj,
				KOBJ_CHANGE, thermal_prop);
			for (i = 0; i < 4; ++i)
				kfree(thermal_prop[i]);

			break;
		}
	}
}

static int md_cooling_mutt_get_max_state(
	struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct md_cooling_device *md_cdev = cdev->devdata;

	*state = md_cdev->max_level;

	return 0;
}

static int md_cooling_mutt_get_cur_state(
	struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct md_cooling_device *md_cdev = cdev->devdata;

	*state = md_cdev->target_level;

	return 0;
}

static int md_cooling_mutt_set_cur_state(
		struct thermal_cooling_device *cdev, unsigned long state)
{
	struct md_cooling_device *md_cdev = cdev->devdata;
	struct mutt_driver_data *drv_data;
	struct device *dev;
	enum md_status status, new_status;
	unsigned int msg;
	unsigned long final_lv;
	int ret = 0;

	/* Request state should be less than max_level */
	if (WARN_ON(state > md_cdev->max_level))
		return -EINVAL;

	drv_data = (struct mutt_driver_data *)md_cdev->dev_data;
	if (!drv_data)
		return -EINVAL;
	dev = &drv_data->pdev->dev;

	if (md_cdev->target_level == state)
		return 0;

	status = get_md_status();
	if (is_md_off(status)) {
		dev_info(dev, "skip mutt due to MD is off\n");
		md_cdev->target_level = MD_COOLING_UNLIMITED_LV;
		mutex_lock(&drv_data->lock);
		drv_data->current_level = MD_COOLING_UNLIMITED_LV;
		mutex_unlock(&drv_data->lock);
		return 0;
	}

	mutex_lock(&drv_data->lock);
	final_lv = find_max_mutt_level(md_cdev->pa_id, state);
	/**
	 * state < final_lv implies the other cooler has higher target state.
	 * state == drv_data->current_level implies the same target state
	 * was already set by the other cooler before.
	 * We should ignore current request in both cases.
	 */
	if (state < final_lv || state == drv_data->current_level) {
		if (state < final_lv)
			dev_info(dev,
				"%s: state(%ld) < final_lv(%ld), skip...\n",
				md_cdev->name, state, final_lv);
		else
			dev_info(dev,
				"%s: state(%ld) is equal to current level\n",
				md_cdev->name, state);
		md_cdev->target_level = state;
		mutex_unlock(&drv_data->lock);
		return 0;
	}

	msg = (state == MD_COOLING_UNLIMITED_LV)
		? TMC_COOLER_LV_DISABLE_MSG
		: mutt_lv_to_tmc_msg(md_cdev->pa_id,
			drv_data->pdata->state_to_cooler_lv(final_lv));
	ret = send_throttle_msg(msg);
	if (ret) {
		mutex_unlock(&drv_data->lock);
		return ret;
	}

	new_status = state_to_md_status(state, md_cdev->max_level);
	set_md_status(new_status);
	md_cdev->target_level = state;
	drv_data->current_level = state;

	/* send notification to userspace due to data is on/off */
	if (new_status == MD_IMS_ONLY && !is_md_inactive(status)) {
		notify_data_status_changed(cdev, state, 0);
		dev_info(dev, "%s: data is off\n", md_cdev->name);
	} else if (new_status == MD_LV_THROTTLE_ENABLED &&
		status == MD_IMS_ONLY) {
		notify_data_status_changed(cdev, state, 1);
		dev_info(dev, "%s: data is on\n", md_cdev->name);
	}
	mutex_unlock(&drv_data->lock);

	dev_dbg(dev, "%s: set lv = %ld done\n", md_cdev->name, state);

	return ret;
}

/**
 * For MT6295, throttle LV start from LV 1
 * For MT6297, throttle LV start from LV 0
 */
static unsigned long mt6295_thermal_state_to_cooler_lv(unsigned long state)
{
	return state;
}
static unsigned long mt6297_thermal_state_to_cooler_lv(unsigned long state)
{
	return (state > 0) ? (state - 1) : 0;
}

static const struct mutt_platform_data mt6295_mutt_pdata = {
	.state_to_cooler_lv = mt6295_thermal_state_to_cooler_lv,
	.max_lv = 5,
};
static const struct mutt_platform_data mt6297_mutt_pdata = {
	.state_to_cooler_lv = mt6297_thermal_state_to_cooler_lv,
	.max_lv = 9,
};

static const struct of_device_id md_cooling_mutt_of_match[] = {
	{
		.compatible = "mediatek,mt6295-md-cooler-mutt",
		.data = (void *)&mt6295_mutt_pdata,
	},
	{
		.compatible = "mediatek,mt6297-md-cooler-mutt",
		.data = (void *)&mt6297_mutt_pdata,
	},
	{},
};
MODULE_DEVICE_TABLE(of, md_cooling_mutt_of_match);

static struct thermal_cooling_device_ops md_cooling_mutt_ops = {
	.get_max_state		= md_cooling_mutt_get_max_state,
	.get_cur_state		= md_cooling_mutt_get_cur_state,
	.set_cur_state		= md_cooling_mutt_set_cur_state,
};

static int md_cooling_mutt_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int ret = -1;
	struct mutt_driver_data *drv_data;

	if (!np) {
		dev_err(dev, "MD cooler DT node not found\n");
		return -ENODEV;
	}

	drv_data = devm_kzalloc(dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;

	mutex_init(&drv_data->lock);
	drv_data->current_level = MD_COOLING_UNLIMITED_LV;
	drv_data->pdev = pdev;
	drv_data->pdata =
		(struct mutt_platform_data *)of_device_get_match_data(dev);

	platform_set_drvdata(pdev, drv_data);

	ret = md_cooling_register(np,
				MD_COOLING_TYPE_MUTT,
				drv_data->pdata->max_lv,
				NULL,
				&md_cooling_mutt_ops,
				drv_data);
	if (ret) {
		dev_err(dev, "register mutt cdev failed!\n");
		return ret;
	}

	return ret;
}

static int md_cooling_mutt_remove(struct platform_device *pdev)
{
	md_cooling_unregister(MD_COOLING_TYPE_MUTT);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver md_cooling_mutt_driver = {
	.probe = md_cooling_mutt_probe,
	.remove = md_cooling_mutt_remove,
	.driver = {
		.name = "mtk-md-cooling-mutt",
		.of_match_table = md_cooling_mutt_of_match,
	},
};
module_platform_driver(md_cooling_mutt_driver);

MODULE_AUTHOR("Shun-Yao Yang <brian-sy.yang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek modem cooling MUTT driver");
MODULE_LICENSE("GPL v2");
