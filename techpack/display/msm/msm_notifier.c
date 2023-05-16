// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/of_platform.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/sched.h>

#include <drm/drm_panel.h>
#include "sde_dbg.h"

struct msm_display_fps_info {
	uint32_t id;
	enum fps fps;
	struct list_head list;
	struct drm_panel *panel;
};

static int msm_notifier_fps_chg_callback(struct notifier_block *nb,
	unsigned long val, void *data);

static struct notifier_block msm_notifier_block = {
	.notifier_call = msm_notifier_fps_chg_callback,
};

struct notifier_info {
	struct notifier_block notifier;
};

struct display_list {
	struct list_head list;
	enum fps max_fps;
};

static struct display_list *active_displays;

static int msm_notifier_fps_chg_callback(struct notifier_block *nb,
			unsigned long val, void *data)
{
	int fps;
	enum fps sched_fps, max_fps;
	struct drm_panel_notifier *notifier_data =
				(struct drm_panel_notifier *) data;
	struct msm_display_fps_info *display;
	bool display_id_match = false;

	/*
	 * Get ceiling of fps from notifier data to pass to scheduler.
	 * Default will be FPS60 and sent to scheduler during suspend.
	 */
	fps = notifier_data->refresh_rate;
	if (fps > FPS120)
		sched_fps = FPS144;
	else if (fps > FPS90)
		sched_fps = FPS120;
	else if (fps > FPS60)
		sched_fps = FPS90;
	else if (fps > FPS48)
		sched_fps = FPS60;
	else if (fps > FPS30)
		sched_fps = FPS48;
	else if (fps > FPS0)
		sched_fps = FPS30;
	else
		sched_fps = FPS60;

	max_fps = sched_fps;

	/*
	 * Iterate displays and set id and fps if uninitialized.
	 * Update display's current fps if id match is found.
	 * Find max refresh rate to pass to scheduler.
	 */
	list_for_each_entry(display, &active_displays->list, list) {
		if (!display->fps && !display_id_match) {
			display->id = notifier_data->id;
			display->fps = sched_fps;
		}

		if (display->id == notifier_data->id) {
			display_id_match = true;
			display->fps = sched_fps;
		}

		if (display->fps > max_fps)
			max_fps = display->fps;
	}

	if (max_fps != active_displays->max_fps) {
		SDE_EVT32(notifier_data->id,
				notifier_data->refresh_rate, max_fps);

		active_displays->max_fps = max_fps;
		sched_set_refresh_rate(max_fps);
	}

	return 0;
}

static int msm_notifier_remove(struct platform_device *pdev)
{
	struct msm_display_fps_info *display;
	struct notifier_info *info = platform_get_drvdata(pdev);

	list_for_each_entry(display, &active_displays->list, list)
		drm_panel_notifier_unregister(display->panel, &info->notifier);

	return 0;
}

static int msm_notifier_probe(struct platform_device *pdev)
{
	int i, count, ret = 0;
	struct device_node *node;
	struct drm_panel *panel;
	struct notifier_info *info = NULL;
	struct msm_display_fps_info *display;

	info = devm_kzalloc(&pdev->dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->notifier = msm_notifier_block;

	platform_set_drvdata(pdev, info);

	active_displays = devm_kzalloc(&pdev->dev,
				sizeof(*active_displays), GFP_KERNEL);

	if (!active_displays) {
		ret = -ENOMEM;
		goto end;
	}

	INIT_LIST_HEAD(&active_displays->list);

	/* Set default max fps to 0 */
	active_displays->max_fps = 0;

	count = of_count_phandle_with_args(pdev->dev.of_node, "panel", NULL);

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(pdev->dev.of_node, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			/*
			 * Add new msm_display_fps_info to linked list
			 * of active displays. Initialize fps as
			 * 0 to mark unassigned node. Assign when
			 * you get the first callback for that display
			 */
			struct msm_display_fps_info *display_fps_info =
					devm_kzalloc(&pdev->dev,
					sizeof(*display_fps_info), GFP_KERNEL);
			if (!display_fps_info) {
				ret = -ENOMEM;
				goto fail;
			}

			display_fps_info->panel = panel;

			list_add(&display_fps_info->list,
					&active_displays->list);

			drm_panel_notifier_register(panel, &info->notifier);
		}
	}

	pr_info("msm notifier probed successfully\n");

	return ret;
fail:
	list_for_each_entry(display, &active_displays->list, list)
		drm_panel_notifier_unregister(display->panel, &info->notifier);

	devm_kfree(&pdev->dev, active_displays);
end:
	devm_kfree(&pdev->dev, info);

	return ret;
}

static const struct of_device_id dt_match[] = {
	{ .compatible = "qcom,msm-notifier"},
	{},
};

MODULE_DEVICE_TABLE(of, dt_match);

static struct platform_driver msm_notifier_platform_driver = {
	.probe     = msm_notifier_probe,
	.remove    = msm_notifier_remove,
	.driver     = {
		.name   = "msm_notifier",
		.of_match_table = dt_match,
		.suppress_bind_attrs = true,
	},
};

static int __init msm_notifier_register(void)
{
	return platform_driver_register(&msm_notifier_platform_driver);
}

static void __exit msm_notifier_unregister(void)
{
	platform_driver_unregister(&msm_notifier_platform_driver);
}

late_initcall(msm_notifier_register);
module_exit(msm_notifier_unregister);
