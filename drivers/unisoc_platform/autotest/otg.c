// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2022 Spreadtrum Communications Inc.

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>
#include <linux/gpio/driver.h>
#include <linux/of.h>
#include <linux/of_gpio.h>

#include "core.h"

enum otg_cmd {
	CMD_DIS_OTG = 1,
	CMD_READ_ID_STATUS,
};

struct otg_info {
	int cmd;
	int value;
};

static int gpio_num;

static int otg_pre_test(struct autotest_handler *handler, void *data)
{
	struct device_node *otg_node;

	otg_node = of_find_compatible_node(NULL, NULL, "linux,extcon-usb-gpio");
	if (!otg_node) {
		pr_err("failed to find otg node.\n");
		return -ENODEV;
	}

	gpio_num = of_get_named_gpio(otg_node, "id-gpio", 0);
	of_node_put(otg_node);

	pr_info("%s, gpio_num: %d\n", __func__, gpio_num);

	return 0;
}

static int otg_disable(struct gpio_desc *gpiod)
{
	int id_irq;

	id_irq = gpiod_to_irq(gpiod);
	if (id_irq < 0) {
		pr_err("failed to get ID IRQ\n");
		return id_irq;
	}

	disable_irq(id_irq);

	return 0;
}

static int get_otg_status(struct gpio_desc *gpiod)
{
	int ret;

	ret = gpiod_direction_input(gpiod);
	if (ret < 0) {
		pr_err("failed to in put gpio\n");
		return ret;
	}

	return gpiod_get_value(gpiod);
}

static int otg_test(struct autotest_handler *handler, void *arg)
{
	int ret;
	void __user *buf = (void __user *)arg;
	struct otg_info otg;
	struct gpio_desc *gpiod;

	gpiod = gpio_to_desc(gpio_num);
	if (IS_ERR(gpiod)) {
		pr_err("otg gpio is invalid\n");
		return PTR_ERR(gpiod);
	}

	if (copy_from_user(&otg, (struct otg_info __user *)arg, sizeof(otg)))
		return -EFAULT;

	switch (otg.cmd) {
	case CMD_DIS_OTG:
		ret = otg_disable(gpiod);
		if (ret < 0) {
			pr_err("fail to otg gpio disable\n");
			return ret;
		}
		break;
	case CMD_READ_ID_STATUS:
		otg.value = get_otg_status(gpiod);
		if (copy_to_user(buf, &otg, sizeof(otg)))
			return -EFAULT;
		break;
	default:
		pr_err("otg cmd is invalid\n");
		return 0;
	}
	pr_info("%s, otg cmd: %d success\n", __func__, otg.cmd);

	return 0;
}

static struct autotest_handler otg_handler = {
	.label = "otg",
	.type = AT_OTG,
	.pre_test = otg_pre_test,
	.start_test = otg_test,
};

static int __init otg_init(void)
{
	return sprd_autotest_register_handler(&otg_handler);
}

static void __exit otg_exit(void)
{
	sprd_autotest_unregister_handler(&otg_handler);
}

late_initcall(otg_init);
module_exit(otg_exit);

MODULE_DESCRIPTION("sprd autotest otg driver");
MODULE_LICENSE("GPL v2");
