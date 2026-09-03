// SPDX-License-Identifier: GPL-2.0-only
/*
 * Spreadtrum sprd-sipc-virt-bus driver
 *
 * Copyright (c) 2020 Spreadtrum Communications Inc.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
/*
 * sipc: sipc-virt {
 *	compatible = "sprd,sipc-virt-bus";
 *	#address-cells = <1>;
 *	#size-cells = <0>;
 *	};
 *
 * Add sipc virt bus driver,this node is parent node, Create devices
 * for the child node, Otherwise, The driver of child node cannot probe
 * successfully. If you want to know more about it, Please look at
 * devm_of_platform_populate().
 *
 * sipc_lte: core@5 {
 *	compatible = "sprd,sipc";
 *	......
 *	......
 * sipc_sp: core@6 {
 *	compatible = "sprd,sipc";
 *	......
 *	......
 * sipc_wcn: core@3 {
 *	compatible = "sprd,sipc";
 *	......
 *	......
 */

static int sprd_sipc_virt_bus_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int ret;

	/* populating sub-devices */
	ret = devm_of_platform_populate(dev);
	if (ret) {
		dev_err(dev, "Failed to populate sub sipc-virt-bus\n");
		return ret;
	}

	dev_info(dev, "Populate sub sipc-virt-bus success\n");

	return 0;
}

static const struct of_device_id sprd_sipc_virt_bus_of_match[] = {
	{ .compatible = "unisoc,sipc-virt-bus", },
	{ },
};
MODULE_DEVICE_TABLE(of, sprd_sipc_virt_bus_of_match);

static struct platform_driver sprd_sipc_virt_bus_driver = {
	.driver = {
		.name = "unisoc-sipc-virt-bus",
		.of_match_table = sprd_sipc_virt_bus_of_match,
	},
	.probe	= sprd_sipc_virt_bus_probe,
};

static int __init sprd_sipc_virt_bus_init(void)
{
	return platform_driver_register(&sprd_sipc_virt_bus_driver);
}

static void __exit sprd_sipc_virt_bus_exit(void)
{
	platform_driver_unregister(&sprd_sipc_virt_bus_driver);
}

arch_initcall(sprd_sipc_virt_bus_init);
module_exit(sprd_sipc_virt_bus_exit);

MODULE_AUTHOR("Haidong Yao <haidong.yao@unisoc.com>");
MODULE_DESCRIPTION("Spreadtrum sipc virt bus driver");
MODULE_LICENSE("GPL v2");
