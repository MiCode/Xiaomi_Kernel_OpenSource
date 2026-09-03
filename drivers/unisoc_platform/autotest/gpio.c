// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2022 Spreadtrum Communications Inc.

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/gpio/driver.h>
#include <linux/gpio/consumer.h>

#include "core.h"
#include "../../gpio/gpiolib.h"

#define SPRD_GPIO_TEST_NUM_MSK		GENMASK(15, 0)
#define SPRD_GPIO_TEST_DIR_MSK		GENMASK(16, 16)
#define SPRD_GPIO_TEST_VAL_MSK		GENMASK(17, 17)
#define SPRD_GPIO_TEST_VAL_SHIFT	17

#define SPRD_GPIO_TEST_NUM(i)		((i) & SPRD_GPIO_TEST_NUM_MSK)
#define SPRD_GPIO_TEST_DIR(i)		(!!((i) & SPRD_GPIO_TEST_DIR_MSK))
#define SPRD_GPIO_TEST_VAL(i)		(!!((i) & SPRD_GPIO_TEST_VAL_MSK))
#define SPRD_GPIO_TEST_NR	256

static int gpio_match_name_gpio(struct gpio_chip *chip, void *data)
{
	if (chip->ngpio == SPRD_GPIO_TEST_NR)
		return 1;
	else
		return 0;
}

static int gpio_test(struct autotest_handler *handler, void *arg)
{
	int gpio_data, offset, num, dir, val, ret = 0;
	struct gpio_desc *desc;
	struct gpio_chip *chip;

	if (get_user(gpio_data, (int __user *)arg))
		return -EFAULT;

	offset = SPRD_GPIO_TEST_NUM(gpio_data);
	dir = SPRD_GPIO_TEST_DIR(gpio_data);
	val = SPRD_GPIO_TEST_VAL(gpio_data);
	pr_info("offset=%d, dir=%d, val=%d\n", offset, dir, val);

	chip = gpiochip_find(NULL, gpio_match_name_gpio);
	if (!chip) {
		pr_err("get gpio chip failed.\n");
		return -EINVAL;
	}

	num = chip->base + offset;
	desc = gpio_to_desc(num);

	ret = gpio_request(num, "autotest-gpio");
	if (ret < 0 && ret != -EBUSY) {
		pr_err("gpio request failed.\n");
		return ret;
	}

	if (dir) {
		if (gpiochip_line_is_irq(chip, offset))
			ret = chip->direction_output(chip, offset, val);
		else
			ret = gpiod_direction_output(desc, val);

		if (ret < 0) {
			pr_err("set direction failed, %d", ret);
			return ret;
		}
	} else {
		ret = gpiod_direction_input(desc);
		if (ret < 0) {
			pr_err("set direction failed, %d", ret);
			return ret;
		}

		val = gpiod_get_value(desc) ? 1 : 0;
		gpio_data &= ~SPRD_GPIO_TEST_VAL_MSK;
		gpio_data |= (val << SPRD_GPIO_TEST_VAL_SHIFT) &
			SPRD_GPIO_TEST_VAL_MSK;
		ret = put_user(gpio_data, (int __user *)arg);
		if (ret < 0) {
			pr_err("write to user failed.\n");
			return -EFAULT;
		}
	}

	return ret;
}

static struct autotest_handler gpio_handler = {
	.label = "gpio",
	.type = AT_GPIO,
	.start_test = gpio_test,
};

static int __init gpio_test_init(void)
{
	return sprd_autotest_register_handler(&gpio_handler);
}

static void __exit gpio_test_exit(void)
{
	sprd_autotest_unregister_handler(&gpio_handler);
}

late_initcall(gpio_test_init);
module_exit(gpio_test_exit);

MODULE_DESCRIPTION("sprd autotest gpio driver");
MODULE_LICENSE("GPL v2");
