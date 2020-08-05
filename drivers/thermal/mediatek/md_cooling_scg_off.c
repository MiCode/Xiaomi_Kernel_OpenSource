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
#include "md_cooling.h"
#include "thermal_trace.h"

#define SCG_OFF_MAX_LEVEL	(1)

static int md_cooling_scg_off_get_max_state(
	struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct md_cooling_device *md_cdev = cdev->devdata;

	*state = md_cdev->max_level;

	return 0;
}

static int md_cooling_scg_off_get_cur_state(
	struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct md_cooling_device *md_cdev = cdev->devdata;

	*state = md_cdev->target_level;

	return 0;
}

static int md_cooling_scg_off_set_cur_state(
		struct thermal_cooling_device *cdev, unsigned long state)
{
	struct md_cooling_device *md_cdev = cdev->devdata;
	struct device *dev = (struct device *)md_cdev->dev_data;
	enum md_status status;
	unsigned int msg;
	int ret;

	/* Request state should be less than max_level */
	if (WARN_ON(state > md_cdev->max_level))
		return -EINVAL;

	if (md_cdev->target_level == state)
		return 0;

	status = get_md_status();
	/*
	 * Ignore when MUTT is activated because SCG off is the one of MUTT
	 * cooling levels
	 */
	if (is_mutt_enabled(status)) {
		dev_info(dev, "skip SCG control due to MUTT is enabled\n");
		/*
		 * SCG will be turned on again when MD is reset. Clear target
		 * LV to avoid sending unnecessary SCG on command to MD
		 */
		if (is_md_off(status))
			md_cdev->target_level = MD_COOLING_UNLIMITED_LV;
		trace_md_scg_off(md_cdev, status);
		return -EACCES;
	}

	msg = (state == MD_COOLING_UNLIMITED_LV)
		? scg_off_to_tmc_msg(0) : scg_off_to_tmc_msg(1);
	ret = send_throttle_msg(msg);
	if (!ret)
		md_cdev->target_level = state;

	dev_dbg(dev, "%s: set lv = %ld done\n", md_cdev->name, state);
	trace_md_scg_off(md_cdev, status);

	return ret;
}

static const struct of_device_id md_cooling_scg_off_of_match[] = {
	{ .compatible = "mediatek,md-cooler-scg-off", },
	{},
};
MODULE_DEVICE_TABLE(of, md_cooling_scg_off_of_match);

static struct thermal_cooling_device_ops md_cooling_scg_off_ops = {
	.get_max_state		= md_cooling_scg_off_get_max_state,
	.get_cur_state		= md_cooling_scg_off_get_cur_state,
	.set_cur_state		= md_cooling_scg_off_set_cur_state,
};

static int md_cooling_scg_off_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int ret;

	if (!np) {
		dev_err(dev, "MD cooler DT node not found\n");
		return -ENODEV;
	}

	ret = md_cooling_register(np,
				MD_COOLING_TYPE_SCG_OFF,
				SCG_OFF_MAX_LEVEL,
				NULL,
				&md_cooling_scg_off_ops,
				dev);
	if (ret)
		dev_err(dev, "register scg-off cdev failed!\n");

	return ret;
}

static int md_cooling_scg_off_remove(struct platform_device *pdev)
{
	md_cooling_unregister(MD_COOLING_TYPE_SCG_OFF);

	return 0;
}

static struct platform_driver md_cooling_scg_off_driver = {
	.probe = md_cooling_scg_off_probe,
	.remove = md_cooling_scg_off_remove,
	.driver = {
		.name = "mtk-md-cooling-scg-off",
		.of_match_table = md_cooling_scg_off_of_match,
	},
};
module_platform_driver(md_cooling_scg_off_driver);

MODULE_AUTHOR("Shun-Yao Yang <brian-sy.yang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek modem cooling SCG off driver");
MODULE_LICENSE("GPL v2");
