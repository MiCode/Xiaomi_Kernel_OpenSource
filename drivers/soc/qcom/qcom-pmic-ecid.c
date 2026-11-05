// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2025, Qualcomm Innovation Center, Inc. All rights reserved. */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/byteorder/generic.h>

#define PMIC_ECID_OFFSET	0xF3
#define MAX_REGS		12
#define MAX_CHAR_SIZE		32

struct pmic_ecid {
	struct device *dev;
	char ecid_str[MAX_CHAR_SIZE];
	int index;
};

static DEFINE_IDA(pmic_ecid_dev_ida);

static int store_ecid_info(struct pmic_ecid *pmic_data, u8 *reg_value)
{
	int rc = 0;

	if (pmic_data == NULL)
		return -EINVAL;

	rc = scnprintf(pmic_data->ecid_str, MAX_CHAR_SIZE, "0x%016llx%08x",
			cpu_to_be64(*(u64 *)reg_value), cpu_to_be32(*(u32 *)(reg_value + 8)));
	if (rc == 0)
		return -EINVAL;
	return rc;
}

static ssize_t ecid_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct pmic_ecid *pmic_data = dev_get_drvdata(dev);

	return scnprintf(buf, MAX_CHAR_SIZE, "%s\n",  pmic_data->ecid_str);
}
static DEVICE_ATTR_RO(ecid);

static struct attribute *pmic_ecid_attrs[] = {
	&dev_attr_ecid.attr,
	NULL,
};
ATTRIBUTE_GROUPS(pmic_ecid);

static struct class pmic_ecid_class = {
	.name = "qcom-pmic-ecid",
	.dev_groups = pmic_ecid_groups,
};

static int pmic_ecid_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *regmap;
	struct pmic_ecid *pmic;
	u8 reg_value[MAX_REGS];
	const char *pmic_name = NULL;
	char name[MAX_CHAR_SIZE];
	u32 val = 0;
	int rc;

	pmic = devm_kzalloc(dev, sizeof(*pmic), GFP_KERNEL);
	if (!pmic)
		return -ENOMEM;

	regmap = dev_get_regmap(dev->parent, NULL);
	if (!regmap) {
		dev_err(dev, "Couldn't get parent's regmap\n");
		return -ENODEV;
	}

	pmic->index = ida_alloc(&pmic_ecid_dev_ida, GFP_KERNEL);
	if (pmic->index < 0) {
		dev_err(dev, "ida_alloc failed: %d\n", pmic->index);
		return pmic->index;
	}

	rc = of_property_read_u32(dev->of_node, "reg", &val);
	if (rc < 0)
		return dev_err_probe(dev, rc, "Couldn't find reg in node\n");

	if (of_property_present(dev->of_node, "qcom,pmic-name")) {
		rc = of_property_read_string(dev->of_node, "qcom,pmic-name", &pmic_name);
		if (rc < 0)
			return dev_err_probe(dev, rc, "Couldn't find pmic-name in node\n");
		scnprintf(name, MAX_CHAR_SIZE, "%s:%s", dev_name(dev->parent), pmic_name);
	} else {
		scnprintf(name, MAX_CHAR_SIZE, "%s", dev_name(dev->parent));
	}

	pmic->dev = device_create(&pmic_ecid_class, NULL,
			MKDEV(0, pmic->index), pmic, "%s", name);
	if (IS_ERR(dev))
		return dev_err_probe(dev, PTR_ERR(dev), "Failed to create qcom-pmic-ecid device\n");

	rc = regmap_bulk_read(regmap, val + PMIC_ECID_OFFSET, reg_value, MAX_REGS);
	if (rc) {
		device_remove_file(pmic->dev, &dev_attr_ecid);
		device_destroy(&pmic_ecid_class, MKDEV(0, pmic->index));
		return dev_err_probe(dev, rc, "Failed to read the ECID\n");
	}

	rc = store_ecid_info(pmic, reg_value);
	if (rc < 0) {
		device_remove_file(pmic->dev, &dev_attr_ecid);
		device_destroy(&pmic_ecid_class, MKDEV(0, pmic->index));
		return dev_err_probe(dev, rc, "Failed to store the ecid info\n");
	}

	platform_set_drvdata(pdev, pmic);

	dev_dbg(dev, "Name : %s ECID : %s\n", name, pmic->ecid_str);

	return 0;
}

static int pmic_ecid_remove(struct platform_device *pdev)
{
	struct pmic_ecid *pmic_data = platform_get_drvdata(pdev);

	device_remove_file(pmic_data->dev, &dev_attr_ecid);
	device_destroy(&pmic_ecid_class, MKDEV(0, pmic_data->index));

	ida_free(&pmic_ecid_dev_ida, pmic_data->index);

	return 0;
}

static const struct of_device_id pmic_ecid_match_table[] = {
	{ .compatible = "qcom,pmic-ecid" },
	{ }
};
MODULE_DEVICE_TABLE(of, pmic_ecid_match_table);

static struct platform_driver pmic_ecid_driver = {
	.probe = pmic_ecid_probe,
	.remove = pmic_ecid_remove,
	.driver = {
		.name = "qcom,pmic-ecid",
		.of_match_table = pmic_ecid_match_table,
	},
};

static int __init qcom_pmic_ecid_init(void)
{
	int err;

	err = class_register(&pmic_ecid_class);
	if (err) {
		pr_err("Failed to register pmic_ecid class rc = %d\n", err);
		return err;
	}

	return platform_driver_register(&pmic_ecid_driver);
}

static void __exit qcom_pmic_ecid_exit(void)
{
	platform_driver_unregister(&pmic_ecid_driver);
	ida_destroy(&pmic_ecid_dev_ida);
	class_unregister(&pmic_ecid_class);
}

module_init(qcom_pmic_ecid_init);
module_exit(qcom_pmic_ecid_exit);

MODULE_DESCRIPTION("QTI PMIC ECID driver");
MODULE_LICENSE("GPL");
