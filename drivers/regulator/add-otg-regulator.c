// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 Spreadtrum Communications Inc.
 */

#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>

struct otg_info {
	struct device *dev;
	int otg_vbus;
	int gpio_irq;
};

static int add_otg_vbus_dt_parse_pdata(struct otg_info *info,
	struct device_node *regulators_np)
{
	int gpio;

	gpio = of_get_named_gpio(regulators_np, "add-otg-vbus-gpio", 0);
	if (!gpio_is_valid(gpio)) {
		dev_err(info->dev, "invalid gpio: %d\n", gpio);
		return -EINVAL;
	}
	info->otg_vbus = gpio;

	return 0;
}

static int regulator_enable_otg(struct regulator_dev *rdev)
{
	int gpio;
	struct otg_info *info;

	if (!rdev) {
		dev_err(&rdev->dev, "%s rdev is null\n", __func__);
		return -EINVAL;
	}

	info = rdev_get_drvdata(rdev);
	gpio = info->otg_vbus;
	if (!gpio_is_valid(gpio)) {
		dev_err(&rdev->dev, "invalid gpio: %d\n", gpio);
		return -EINVAL;
	}

	gpio_set_value(gpio, 1);

	return 0;
}

static int regulator_disable_otg(struct regulator_dev *rdev)
{
	int gpio;
	struct otg_info *info;

	if (!rdev) {
		dev_err(&rdev->dev, "%s rdev is null\n", __func__);
		return -EINVAL;
	}

	info = rdev_get_drvdata(rdev);
	gpio = info->otg_vbus;
	if (!gpio_is_valid(gpio)) {
		dev_err(&rdev->dev, "invalid gpio: %d\n", gpio);
		return -EINVAL;
	}

	gpio_set_value(gpio, 0);

	return 0;
}

static int regulator_is_enabled_otg(struct regulator_dev *rdev)
{
	int gpio;
	struct otg_info *info;

	if (!rdev) {
		dev_err(&rdev->dev, "%s rdev is null\n", __func__);
		return -EINVAL;
	}

	info = rdev_get_drvdata(rdev);
	gpio = info->otg_vbus;
	if (!gpio_is_valid(gpio)) {
		dev_err(&rdev->dev, "invalid gpio: %d\n", gpio);
		return -EINVAL;
	}

	return gpio_get_value(gpio);
}

static const struct regulator_ops add_otg_vbus_ops = {
	.enable = regulator_enable_otg,
	.disable = regulator_disable_otg,
	.is_enabled = regulator_is_enabled_otg,
};

static const struct regulator_desc add_otg_vbus_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &add_otg_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static struct dentry *debugfs_root;

static int debugfs_enable_get(void *data, u64 *val)
{
	struct regulator_dev *rdev = data;

	if (rdev && rdev->desc->ops->is_enabled)
		*val = rdev->desc->ops->is_enabled(rdev);
	else
		*val = ~0ULL;

	return 0;
}

static int debugfs_enable_set(void *data, u64 val)
{
	struct regulator_dev *rdev = data;

	if (rdev && rdev->desc->ops->enable && rdev->desc->ops->disable) {
		if (val)
			rdev->desc->ops->enable(rdev);
		else
			rdev->desc->ops->disable(rdev);
	}

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(fops_enable, debugfs_enable_get, debugfs_enable_set, "%llu\n");

static void add_otg_vbus_regulator_debugfs_init(struct regulator_dev *rdev)
{
	debugfs_root = debugfs_create_dir(rdev->desc->name, NULL);
	if (IS_ERR_OR_NULL(debugfs_root)) {
		dev_err(&rdev->dev, "Failed to create debugfs directory\n");
		rdev->debugfs = NULL;
		return;
	}

	debugfs_create_file("enable", 0644, debugfs_root, rdev, &fops_enable);
}

static irqreturn_t add_otg_vbus_interrupt_handler(int irq, void *dev_id)
{
	struct otg_info *info = dev_id;

	dev_err(info->dev, "irq: Interrupt occurred\n");

	return IRQ_HANDLED;
}

static int add_otg_vbus_interrupt_init(struct platform_device *pdev)
{
	int ret;
	struct otg_info *info;

	info = platform_get_drvdata(pdev);

	info->gpio_irq = of_get_named_gpio(pdev->dev.of_node, "add-otg-vbus-irq-gpio", 0);
	if (!gpio_is_valid(info->gpio_irq))
		return -EINVAL;

	ret = devm_gpio_request(&pdev->dev, info->gpio_irq, "regulator_int");
	if (ret)
		goto err;

	ret = gpio_direction_input(info->gpio_irq);
	if (ret)
		goto err;

	ret = devm_request_threaded_irq(&pdev->dev, gpio_to_irq(info->gpio_irq),
					NULL, add_otg_vbus_interrupt_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					"otg_vbus irq", info);
	if (ret) {
		dev_err(&pdev->dev, "request_irq %d failed\n",
			gpio_to_irq(info->gpio_irq));
		goto err;
	}

	return 0;

err:
	gpio_free(info->gpio_irq);

	return ret;
}

static int add_otg_vbus_regulator_probe(struct platform_device *pdev)
{
	int ret;
	struct device_node *regulators_np = pdev->dev.of_node;
	struct regulator_config cfg = { };
	struct otg_info *info;
	struct regulator_dev *rdev;

	info = devm_kzalloc(&pdev->dev, sizeof(struct otg_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		ret = add_otg_vbus_dt_parse_pdata(info, regulators_np);
		if (ret)
			return ret;
	}

	info->dev = &pdev->dev;
	cfg.dev = &pdev->dev;
	platform_set_drvdata(pdev, info);

	cfg.driver_data = info;
	cfg.of_node = regulators_np;

	ret = add_otg_vbus_interrupt_init(pdev);
	if (ret) {
		dev_err(&pdev->dev, "add_otg_vbus_interrupt_init failed\n");
		return -EINTR;
	}

	rdev = devm_regulator_register(&pdev->dev, &add_otg_vbus_charger_vbus_desc, &cfg);
	if (IS_ERR(rdev)) {
		dev_err(&pdev->dev, "failed to register regulator %s\n",
			add_otg_vbus_charger_vbus_desc.name);
		return PTR_ERR(rdev);
	}
	add_otg_vbus_regulator_debugfs_init(rdev);

	ret = devm_gpio_request(&pdev->dev, info->otg_vbus, "otg-vbus");
	if (ret)
		return ret;

	/* SET GPIO */
	gpio_direction_output(info->otg_vbus, 0);

	return 0;
}

static int add_otg_vbus_regulator_remove(struct platform_device *pdev)
{
	struct otg_info *info = platform_get_drvdata(pdev);

	debugfs_remove_recursive(debugfs_root);
	gpio_free(info->gpio_irq);

	return 0;
}

static const struct of_device_id add_otg_vbus_regulator_match[] = {
	{ .compatible = "sprd,add-otg-vbus-regulator" },
	{},
};
MODULE_DEVICE_TABLE(of, add_otg_vbus_regulator_match);

static struct platform_driver add_otg_vbus_regulator_driver = {
	.driver = {
		.name = "add-otg-vbus-regulator",
		.of_match_table = add_otg_vbus_regulator_match,
	},
	.probe = add_otg_vbus_regulator_probe,
	.remove = add_otg_vbus_regulator_remove,
};

module_platform_driver(add_otg_vbus_regulator_driver);

MODULE_AUTHOR("Fei.Gao3 <fei.gao3@unisoc.com>");
MODULE_DESCRIPTION("Unisoc add otg vbus regulator driver");
MODULE_LICENSE("GPL v2");
