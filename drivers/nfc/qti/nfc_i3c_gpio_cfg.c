// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#include <linux/delay.h>

/* NFC I3C DT node name */
#define NFC_I3C_DT_NODE_STR	"sn"

/* NFC GPIO names in the I3C slave node */
#define DTS_VEN_GPIO_STR	"qcom,sn-ven"
#define DTS_FWDN_GPIO_STR	"qcom,sn-firm"

static int __init nfc_i3c_gpio_cfg_init(void)
{
	int ret = 0;
	int ven_gpio = 0, firm_gpio = 0;
	struct device_node *np = NULL;

	pr_debug("%s\n", __func__);

	np = of_find_node_by_name(NULL, NFC_I3C_DT_NODE_STR);
	if (!np) {
		pr_err("finding NFC I3C slave node failed\n");
		return -ENODEV;
	}

	ven_gpio = of_get_named_gpio(np, DTS_VEN_GPIO_STR, 0);
	pr_debug("NFC VEN %d\n", ven_gpio);

	if ((!gpio_is_valid(ven_gpio))) {
		pr_err("invalid ven gpio %d from dt\n", ven_gpio);
		return -EINVAL;
	}

	firm_gpio = of_get_named_gpio(np, DTS_FWDN_GPIO_STR, 0);
	pr_debug("NFC FIRM GPIO %d\n", firm_gpio);

	if ((!gpio_is_valid(firm_gpio))) {
		pr_err("invalid firm gpio %d from dt\n", firm_gpio);
		return -EINVAL;
	}

	ret = gpio_request(firm_gpio, "nfc_fw_gpio");
	if (ret) {
		pr_err("NFC firm gpio request failed ret %d\n", ret);
		return -EIO;
	}

	ret = gpio_direction_output(firm_gpio, 0);
	if (ret) {
		pr_err("NFC firm gpio direction set failed ret %d\n", ret);
		gpio_free(firm_gpio);
		return -EIO;
	}

	usleep_range(10000, 10100);
	pr_debug("NFC firm gpio %d\n", gpio_get_value(firm_gpio));

	ret = gpio_request(ven_gpio, "nfc_reset_gpio");
	if (ret) {
		pr_err("nfc ven gpio req failed ret %d\n", ret);
		gpio_set_value(firm_gpio, 0);
		gpio_free(firm_gpio);
		return -EIO;
	}

	ret = gpio_direction_output(ven_gpio, 0);
	if (ret) {
		pr_err("ven direction set failed ret %d\n", ret);
		gpio_set_value(firm_gpio, 0);
		gpio_free(firm_gpio);
		gpio_free(ven_gpio);
		return -EIO;
	}

	pr_debug("VEN GPIO level %d\n", gpio_get_value(ven_gpio));
	usleep_range(10000, 10100);
	gpio_set_value(ven_gpio, 1);
	pr_debug("VEN GPIO level %d\n", gpio_get_value(ven_gpio));

	gpio_free(ven_gpio);
	gpio_free(firm_gpio);
	return ret;
}

module_init(nfc_i3c_gpio_cfg_init);

static void __exit nfc_i3c_gpio_cfg_exit(void)
{
	pr_debug("Unloading I3C NFC GPIO Config driver\n");
}

module_exit(nfc_i3c_gpio_cfg_exit);

MODULE_DESCRIPTION("QTI NFC I3C GPIO Config driver");
MODULE_LICENSE("GPL v2");
