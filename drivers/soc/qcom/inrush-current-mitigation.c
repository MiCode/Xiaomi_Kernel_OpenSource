/*
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/async.h>
#include <linux/clk/gdsc.h>
#include <linux/regulator/consumer.h>
#include <soc/qcom/subsystem_notif.h>

struct subsystem {
	const char *name;
	void *notif_handle;
	struct notifier_block nb;
	bool booted;
	struct inrush_driver_data *drv_data;
};

struct inrush_driver_data {
	int subsys_count;
	int subsys_boot_count;
	struct regulator *vreg;
	/* Must be the last member */
	struct subsystem *subsystems;
};

#define notifier_to_subsystem(d) container_of(d, struct subsystem, nb)

static void free_resources(void *data, async_cookie_t cookie)
{
	struct inrush_driver_data *drv_data = data;
	struct subsystem *subsys;
	int i;

	gdsc_allow_clear_retention(drv_data->vreg);
	devm_regulator_put(drv_data->vreg);

	for (i = 0; i < drv_data->subsys_count; i++) {
		subsys = &drv_data->subsystems[i];
		subsys_notif_unregister_notifier(subsys->notif_handle,
						&subsys->nb);
	}

	kfree(drv_data);
	pr_info("inrush-current-mitigation driver exited\n");
}

static int mitigate_inrush_notifier_cb(struct notifier_block *nb,
				unsigned long code, void *ss_handle)
{
	struct subsystem *subsys = notifier_to_subsystem(nb);
	struct inrush_driver_data *drv_data = subsys->drv_data;

	if (subsys->booted)
		return NOTIFY_DONE;

	switch (code) {
	case SUBSYS_AFTER_POWERUP:
		pr_info("%s: subsystem %s has completed powerup\n", __func__,
							subsys->name);
		subsys->booted = true;
		drv_data->subsys_boot_count++;
		break;
	default:
		return NOTIFY_DONE;
	}

	/*
	 * If all subsystems are up, job of this driver ends, lets
	 * free resources.
	 */
	if (drv_data->subsys_count == drv_data->subsys_boot_count)
		async_schedule(free_resources, drv_data);

	return NOTIFY_DONE;
}

static int mitigate_inrush_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	int i, retval;
	struct subsystem *subsys;
	struct inrush_driver_data *drv_data;

	retval = of_property_count_strings(np,
					"qcom,dependent-subsystems");
	if (IS_ERR_VALUE(retval)) {
		dev_err(dev, "Failed to get dependent subsystems\n");
		return -EINVAL;
	}

	drv_data = kzalloc((retval * sizeof(struct subsystem) +
			sizeof(struct inrush_driver_data)), GFP_KERNEL);

	if (!drv_data)
		return -ENOMEM;

	drv_data->subsystems = (void *)drv_data +
				sizeof(struct inrush_driver_data);
	drv_data->subsys_count = retval;

	for (i = 0; i < drv_data->subsys_count; i++) {
		subsys = &drv_data->subsystems[i];
		subsys->drv_data = drv_data;
		of_property_read_string_index(np, "qcom,dependent-subsystems",
					i, &subsys->name);
		subsys->nb.notifier_call = mitigate_inrush_notifier_cb;
		subsys->notif_handle =
			subsys_notif_register_notifier(subsys->name,
							&subsys->nb);
		if (IS_ERR(subsys->notif_handle)) {
			dev_err(dev, "Notifier registration failed for %s\n",
						 subsys->name);
			retval = PTR_ERR(subsys->notif_handle);
			goto err_subsys_notif;
		}
	}

	drv_data->vreg = devm_regulator_get(dev, "vdd");
	if (IS_ERR(drv_data->vreg)) {
		dev_err(dev, "Failed to get regulator\n");
		return PTR_ERR(drv_data->vreg);
	}

	return 0;

err_subsys_notif:
	for (i = 0; i < drv_data->subsys_count; i++) {
		subsys = &drv_data->subsystems[i];
		subsys_notif_unregister_notifier(subsys->notif_handle,
						&subsys->nb);
	}
	kfree(drv_data);
	return retval;
}

static const struct of_device_id mitigate_inrush_match_table[] = {
	{ .compatible = "qcom,msm-inrush-current-mitigation" },
	{},
};

static struct platform_driver mitigate_inrush_driver = {
	.probe = mitigate_inrush_probe,
	.driver = {
		.name = "msm-inrush-current",
		.owner = THIS_MODULE,
		.of_match_table = mitigate_inrush_match_table,
	},
};

static int init_msm_mitigate_inrush(void)
{
	return platform_driver_register(&mitigate_inrush_driver);
}
late_initcall(init_msm_mitigate_inrush);
