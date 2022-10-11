// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/regmap.h>

#define BCL_OFF_DRIVER    "bcl-off"
#define BCL_OFF_MAX_LVL   1
#define BCL_EN_CTL        0x46

struct bcl_off_cdev {
	struct regmap *regmap;
	bool bcl_off_state;
	uint16_t bcl_ctl_addr;
	struct thermal_cooling_device *cdev;
};

static int bcl_off_set_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long state)
{
	int  ret = 0;
	struct bcl_off_cdev *bcl_off_cdev = cdev->devdata;

	if (state > BCL_OFF_MAX_LVL)
		return -EINVAL;

	/* Check if the old cooling action is same as new cooling action */
	if (bcl_off_cdev->bcl_off_state == state)
		return 0;

	/*
	 * BCL will be disabled with below register write. This function is
	 * supposed to be called only one time. BCL cannot be re-enabled using this
	 * function.
	 */
	ret = regmap_write(bcl_off_cdev->regmap, bcl_off_cdev->bcl_ctl_addr, 0);
	if (ret < 0) {
		pr_err("Error writing register:0x%04x err:%d\n",
				bcl_off_cdev->bcl_ctl_addr, ret);
		return ret;
	}
	bcl_off_cdev->bcl_off_state = state;
	pr_debug("%s bcl off for %s\n",
		state ? "Triggered" : "Cleared", bcl_off_cdev->cdev->type);

	return 0;
}

static int bcl_off_get_cur_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	struct bcl_off_cdev *bcl_off_cdev = cdev->devdata;

	*state = (bcl_off_cdev->bcl_off_state) ?
			BCL_OFF_MAX_LVL : 0;

	return 0;
}

static int bcl_off_get_max_state(struct thermal_cooling_device *cdev,
				 unsigned long *state)
{
	*state = BCL_OFF_MAX_LVL;
	return 0;
}

static struct thermal_cooling_device_ops bcl_off_cooling_ops = {
	.get_max_state = bcl_off_get_max_state,
	.get_cur_state = bcl_off_get_cur_state,
	.set_cur_state = bcl_off_set_cur_state,
};


static int bcl_off_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct bcl_off_cdev *bcl_off_cdev = NULL;
	struct device_node *dn = pdev->dev.of_node;
	uint16_t bcl_ctl_reg;
	char cdev_name[THERMAL_NAME_LENGTH] = "bcl-off";
	const __be32 *addr;

	bcl_off_cdev = devm_kzalloc(&pdev->dev, sizeof(*bcl_off_cdev),
					GFP_KERNEL);
	if (!bcl_off_cdev)
		return -ENOMEM;

	bcl_off_cdev->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!bcl_off_cdev->regmap) {
		dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
		return -EINVAL;
	}

	addr = of_get_address(dn, 0, NULL, NULL);
	if (!addr) {
		dev_err(&pdev->dev, "Property bcl-addr not found\n");
		return -EINVAL;
	}

	bcl_ctl_reg = be32_to_cpu(*addr) + BCL_EN_CTL;
	bcl_off_cdev->bcl_ctl_addr = bcl_ctl_reg;
	bcl_off_cdev->cdev = thermal_of_cooling_device_register(
					dn,
					cdev_name,
					bcl_off_cdev,
					&bcl_off_cooling_ops);
	if (IS_ERR(bcl_off_cdev->cdev)) {
		ret = PTR_ERR(bcl_off_cdev->cdev);
		dev_err(&pdev->dev, "Cooling register failed for %s, ret:%d\n",
			cdev_name, ret);
		bcl_off_cdev->cdev = NULL;
		return ret;
	}

	pr_debug("Cooling device [%s] registered.\n", cdev_name);
	dev_set_drvdata(&pdev->dev, bcl_off_cdev);

	return ret;
}

static int bcl_off_remove(struct platform_device *pdev)
{
	struct bcl_off_cdev *bcl_off_cdev =
		(struct bcl_off_cdev *)dev_get_drvdata(&pdev->dev);

	if (bcl_off_cdev->cdev)
		thermal_cooling_device_unregister(bcl_off_cdev->cdev);

	return 0;
}
static const struct of_device_id bcl_off_match[] = {
	{ .compatible = "qcom,bcl-off", },
	{},
};

static struct platform_driver bcl_off_driver = {
	.probe		= bcl_off_probe,
	.remove     = bcl_off_remove,
	.driver		= {
		.name = BCL_OFF_DRIVER,
		.of_match_table = bcl_off_match,
	},
};

module_platform_driver(bcl_off_driver);
MODULE_LICENSE("GPL v2");
