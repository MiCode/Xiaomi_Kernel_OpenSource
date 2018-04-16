/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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

static void probe_subsystem(struct device *dev, struct device_node *np)
{
	struct qcom_glink *glink = ERR_PTR(-EINVAL);
	const char *transport;
	u32 pid;
	int ret;

	ret = of_property_read_u32(np, "qcom,remote-pid", &pid);
	if (ret || pid >= NUM_SUBSYSTEMS) {
		dev_err(dev, "invalid pid:%d ret:%d\n", pid, ret);
		return;
	}

	ret = of_property_read_string(np, "transport", &transport);
	if (ret < 0) {
		dev_err(dev, "missing transport pid:%d\n", pid);
		return;
	}
	if (!strcmp(transport, "smem"))
		glink = qcom_glink_smem_register(dev, np);
	else if (!strcmp(transport, "spss"))
		glink = qcom_glink_spss_register(dev, np);

	if (IS_ERR(glink)) {
		dev_err(dev, "%s failed %d\n", np->name, PTR_ERR(glink));
		return;
	}
	edge_infos[pid] = glink;
}

static int glink_probe(struct platform_device *pdev)
{
	struct device_node *cn, *pn = pdev->dev.of_node;

	for_each_available_child_of_node(pn, cn) {
		probe_subsystem(&pdev->dev, cn);
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
	int ret;

	ret = platform_driver_register(&glink_probe_driver);
	if (ret) {
		pr_err("%s: glink_probe register failed %d\n",
			__func__, ret);
		return ret;
	}

	return 0;
}
arch_initcall(glink_probe_init);

MODULE_DESCRIPTION("Qualcomm GLINK probe helper driver");
MODULE_LICENSE("GPL v2");
