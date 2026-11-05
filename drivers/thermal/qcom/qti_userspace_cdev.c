// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>

#define USERSPACE_CDEV_DRIVER		"userspace-cdev"

struct userspace_cdev {
	struct device_node		*np;
	char				cdev_name[THERMAL_NAME_LENGTH];
	struct thermal_cooling_device	*cdev;
	unsigned int			cur_level;
	unsigned int			max_level;
};

static int userspace_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct userspace_cdev *usr_cdev = cdev->devdata;

	if (!usr_cdev)
		return -EINVAL;
	*state = usr_cdev->max_level;

	return 0;
}

static int userspace_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct userspace_cdev *usr_cdev = cdev->devdata;

	if (!usr_cdev)
		return -EINVAL;
	*state = usr_cdev->cur_level;

	return 0;
}

static int userspace_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	struct userspace_cdev *usr_cdev = cdev->devdata;

	if (!usr_cdev)
		return -EINVAL;

	if (state > usr_cdev->max_level)
		return -EINVAL;

	if (usr_cdev->cur_level == state)
		return 0;
	usr_cdev->cur_level = state;

	return 0;
}

static struct thermal_cooling_device_ops userspace_cdev_ops = {
	.get_max_state = userspace_get_max_state,
	.get_cur_state = userspace_get_cur_state,
	.set_cur_state = userspace_set_cur_state,
};

static int userspace_device_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret = 0, idx = 0, subsys_cnt = 0;
	struct device_node *np = dev->of_node, *subsys_np = NULL;
	struct userspace_cdev *cdev_instances;

	subsys_cnt = of_get_available_child_count(np);
	if (!subsys_cnt) {
		dev_err(dev, "No child node to process\n");
		return -EFAULT;
	}
	cdev_instances = devm_kcalloc(dev, subsys_cnt, sizeof(*cdev_instances),
					GFP_KERNEL);
	if (!cdev_instances)
		return -ENOMEM;

	for_each_available_child_of_node(np, subsys_np) {
		if (idx >= subsys_cnt) {
			of_node_put(subsys_np);
			break;
		}

		ret = of_property_read_u32(subsys_np, "qcom,max-level",
				&cdev_instances[idx].max_level);
		if (ret) {
			dev_err(dev, "error reading qcom,max-level. ret:%d\n",
				ret);
			goto probe_error;
		}

		cdev_instances[idx].np = subsys_np;
		strscpy(cdev_instances[idx].cdev_name, subsys_np->name,
				THERMAL_NAME_LENGTH);

		cdev_instances[idx].cdev = devm_thermal_of_cooling_device_register(
						dev,
						subsys_np,
						cdev_instances[idx].cdev_name,
						&cdev_instances[idx],
						&userspace_cdev_ops);
		if (IS_ERR(cdev_instances[idx].cdev)) {
			dev_err(dev, "Error registering cdev:%s err:%ld\n",
					cdev_instances[idx].cdev_name,
					PTR_ERR(cdev_instances[idx].cdev));
			cdev_instances[idx].cdev = NULL;
			goto probe_error;
		}
		dev_info(dev, "cdev:%s lvl:%d registered\n",
				cdev_instances[idx].cdev_name,
				cdev_instances[idx].max_level);
		idx++;
	}
	of_node_put(np);

	return 0;
probe_error:
	of_node_put(subsys_np);
	of_node_put(np);
	return ret;
}

static const struct of_device_id userspace_device_match[] = {
	{.compatible = "qcom,userspace-cooling-devices"},
	{}
};

static struct platform_driver userspace_cdev_driver = {
	.probe          = userspace_device_probe,
	.driver         = {
		.name   = USERSPACE_CDEV_DRIVER,
		.of_match_table = userspace_device_match,
	},
};

static int __init userspace_cdev_init(void)
{
	return platform_driver_register(&userspace_cdev_driver);
}
module_init(userspace_cdev_init);

static void __exit userspace_cdev_exit(void)
{
	platform_driver_unregister(&userspace_cdev_driver);
}
module_exit(userspace_cdev_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Userspace cooling device driver");
