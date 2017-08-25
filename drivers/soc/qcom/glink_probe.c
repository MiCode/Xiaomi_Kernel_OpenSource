/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include <linux/of.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rpmsg/qcom_glink.h>

#define NUM_SUBSYSTEMS 10

static struct qcom_glink *edge_infos[NUM_SUBSYSTEMS];

static int glink_probe(struct platform_device *pdev)
{
	struct device_node *cn, *pn = pdev->dev.of_node;
	struct qcom_glink *glink = ERR_PTR(-EINVAL);
	const char *xprt;
	u32 pid;
	int ret;

	for_each_available_child_of_node(pn, cn) {
		ret = of_property_read_u32(cn, "qcom,remote-pid", &pid);
		if (ret || pid >= NUM_SUBSYSTEMS) {
			dev_err(&pdev->dev, "invalid pid:%d ret:%d\n",
				pid, ret);
			return -EINVAL;
		}
		xprt = of_get_property(cn, "transport", NULL);
		if (!xprt) {
			dev_err(&pdev->dev, "missing xprt pid:%d\n", pid);
			return -EINVAL;
		}
		if (!strcmp(xprt, "smem"))
			glink = qcom_glink_smem_register(&pdev->dev, cn);
		if (IS_ERR(glink)) {
			dev_err(&pdev->dev, "%s failed\n", cn->name);
			return PTR_ERR(glink);
		}
		edge_infos[pid] = glink;
	}
	return 0;
}

static const struct of_device_id glink_match_table[] = {
	{ .compatible = "qcom,glink" },
	{},
};

static struct platform_driver glink_probe_driver = {
	.probe = glink_probe,
	.driver = {
		.name = "msm_glink",
		.owner = THIS_MODULE,
		.of_match_table = glink_match_table,
	},
};

static int __init glink_probe_init(void)
{
	int rc;

	rc = platform_driver_register(&glink_probe_driver);
	if (rc) {
		pr_err("%s: glink_probe register failed %d\n",
			__func__, rc);
		return rc;
	}

	return 0;
}
arch_initcall(glink_probe_init);

MODULE_DESCRIPTION("Qualcomm GLINK probe helper driver");
MODULE_LICENSE("GPL v2");
