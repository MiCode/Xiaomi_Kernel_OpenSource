// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2022 Spreadtrum Communications Inc.

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/uaccess.h>
#include <linux/pinctrl/consumer.h>

#include "core.h"

#define NAME_MAX_SIZE	9
static struct pinctrl *p;
static struct platform_device *pdev;

static int pinctrl_pre_test(struct autotest_handler *handler, void *data)
{
	struct device_node *autotest_node, *pinctrl_node;

	autotest_node = of_find_compatible_node(NULL, NULL, "sprd,autotest");
	if (!autotest_node) {
		pr_err("failed to find autotest node.\n");
		return -ENODEV;
	}

	pinctrl_node = of_parse_phandle(autotest_node, "sprd,pinctrl", 0);
	if (!pinctrl_node) {
		of_node_put(autotest_node);
		pr_err("failed to find pinctrl node.\n");
		return -ENODEV;
	}
	of_node_put(autotest_node);

	pdev = of_find_device_by_node(pinctrl_node);
	if (!pdev) {
		of_node_put(pinctrl_node);
		pr_err("failed to get pinctrl platform device.\n");
		return -ENODEV;
	}
	of_node_put(pinctrl_node);

	return 0;
}

static int pinctrl_test(struct autotest_handler *handler, void *arg)
{
	struct pinctrl_state *pinctrl_state;
	char state_name[NAME_MAX_SIZE];
	int gpio;

	if (get_user(gpio, (int __user *)arg))
		return -EFAULT;

	if (!p) {
		p = devm_pinctrl_get(&pdev->dev);
		if (IS_ERR(p)) {
			pr_err("get pinctrl handle failed.\n");
			return PTR_ERR(p);
		}
	}

	pr_info("gpio = %d\n", gpio);
	snprintf(state_name, NAME_MAX_SIZE, "gpio_%d", gpio);

	pinctrl_state = pinctrl_lookup_state(p, state_name);
	if (IS_ERR(pinctrl_state)) {
		pr_err("there are no autotest gpios state.\n");
		return PTR_ERR(pinctrl_state);
	}

	return pinctrl_select_state(p, pinctrl_state);
}

static struct autotest_handler pinctrl_handler = {
	.label = "pinctrl",
	.type = AT_PINCTRL,
	.pre_test = pinctrl_pre_test,
	.start_test = pinctrl_test,
};

static int __init pinctrl_init(void)
{
	return sprd_autotest_register_handler(&pinctrl_handler);
}

static void __exit pinctrl_exit(void)
{
	if (p)
		devm_pinctrl_put(p);

	sprd_autotest_unregister_handler(&pinctrl_handler);
}

late_initcall(pinctrl_init);
module_exit(pinctrl_exit);

MODULE_DESCRIPTION("sprd autotest pinctrl driver");
MODULE_LICENSE("GPL v2");
