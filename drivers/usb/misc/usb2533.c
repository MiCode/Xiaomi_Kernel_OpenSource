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

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/usb/class-dual-role.h>
#include <linux/usb.h>
#include <linux/usb/msm_hsusb.h>

#define USB_HUB_I2C_NAME	"usb2533-flex-hub"

static u8 write_connect_cfg[] = {0x00, 0x00, 0x05, 0x00, 0x02,
						0x31, 0x8E, 0x83};
static u8 conf_access[] = {0x99, 0x37, 0x00};
static u8 read_connect_cfg[] = {0x00, 0x00, 0x04, 0x01, 0x01, 0x31, 0x8E};
static u8 read_cmd[] = {0x00, 0x04};
static u8 usb_attach[] = {0xAA, 0x55, 0x00};

struct flex_hub {
	struct i2c_client	*client;
	struct  device		*dev;
	struct regulator	*vbus_det;
	struct notifier_block	usbdev_nb;
	int			hub_reset_gpio;
	int			usbeth_reset_gpio;
};
static struct flex_hub *usb_hub;

static int hub_read_regdata(struct i2c_client *i2c)
{
	int rc;
	u8 data[3];

	struct i2c_msg msgs[] = {
		{
			.addr  = i2c->addr,
			.flags = I2C_M_RD,
			.len   = 3,
			.buf   = data,
		}
	};

	rc = i2c_transfer(i2c->adapter, msgs, 1);
	if (rc < 0) {
		dev_dbg(&i2c->dev, "i2c read from 0x%x failed %d\n",
			i2c->addr, rc);
		return -ENXIO;
	}

	return rc;
}

static int hub_i2c_write(struct flex_hub *usb_hub, u8 *data, int len)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
		.addr  = usb_hub->client->addr,
		.flags = 0,
		.len   = len,
		.buf   = data,
		}
	};
	ret = i2c_transfer(usb_hub->client->adapter, msgs, 1);
	if (ret != 1) {
		pr_err("i2c write to [%x] failed %d\n",
			usb_hub->client->addr, ret);
		return -EIO;
	}

	return 0;
}

int i2c_hub_attach(void)
{
	if (hub_i2c_write(usb_hub, usb_attach, sizeof(usb_attach))) {
		pr_err("%s usb_attach failed\n", __func__);
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(i2c_hub_attach);

int i2c_hub_flex_enable(struct flex_hub *usb_hub)
{
	int ret;

	if (hub_i2c_write(usb_hub, write_connect_cfg,
			sizeof(write_connect_cfg))) {
		pr_err("%s write_connect_cfg cmd failed\n", __func__);
		return -EIO;
	}

	if (hub_i2c_write(usb_hub, conf_access, sizeof(conf_access))) {
		pr_err("%s conf_access cmd failed\n", __func__);
		return -EIO;
	}

	if (hub_i2c_write(usb_hub, read_connect_cfg,
			sizeof(read_connect_cfg))) {
		pr_err("%s read_connect cmd failed\n", __func__);
		return -EIO;
	}

	if (hub_i2c_write(usb_hub, conf_access, sizeof(conf_access))) {
		pr_err("%s conf_access cmd failed\n", __func__);
		return -EIO;
	}

	if (hub_i2c_write(usb_hub, read_cmd, sizeof(read_cmd))) {
		pr_err("%s read_cmd failed\n", __func__);
		return -EIO;
	}

	ret = hub_read_regdata(usb_hub->client);
	if (ret < 0) {
		pr_err("read_regdata failed\n");
		return -EIO;
	}

	/*
	 * After the read/write of USB2533 HUB's configuration register,
	 * We need to wait for 300ms before doing USB_ATTACH(special command).
	 */
	msleep(300);
	if (hub_i2c_write(usb_hub, usb_attach, sizeof(usb_attach))) {
		pr_err("%s usb_attach cmd failed\n", __func__);
		return -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(i2c_hub_flex_enable);

static int flex_hub_usbdev_notify(struct notifier_block *self,
			unsigned long action, void *priv)
{
	static bool flex_enabled;

	pr_debug("%s: action:%lu flex:%d\n", __func__, action, flex_enabled);

	if (action != USB_BUS_ADD && action != USB_BUS_REMOVE)
		return 0;

	switch (action) {
	case USB_BUS_ADD:
		if (flex_enabled) {
			pr_debug("%s: already flexconnect enabled\n", __func__);
			break;
		}

		if (gpio_is_valid(usb_hub->hub_reset_gpio)) {
			gpio_direction_output(
				usb_hub->hub_reset_gpio, 0);
			/* 5 microsecs reset signaling to hub */
			usleep_range(5, 10);
			gpio_direction_output(
				usb_hub->hub_reset_gpio, 1);
		}

		/*
		 * After HUB reset, 300ms delay is required in
		 * SOC config stage before doing any i2c access.
		 */
		msleep(300);
		if (gpio_is_valid(usb_hub->usbeth_reset_gpio)) {
			gpio_direction_output(
				usb_hub->usbeth_reset_gpio, 0);
			/* 100 microsecs reset signaling to LAN7500 */
			usleep_range(100, 110);
			gpio_direction_output(
				usb_hub->usbeth_reset_gpio, 1);
		}
		i2c_hub_flex_enable(usb_hub);
		flex_enabled = true;
		break;

	case USB_BUS_REMOVE:
		if (!flex_enabled) {
			pr_debug("%s: already flexconnect disabled\n",
				__func__);
			break;
		}

		if (gpio_is_valid(usb_hub->hub_reset_gpio)) {
			gpio_direction_output(
				usb_hub->hub_reset_gpio, 0);
			/* 5 microsecs reset signaling to usb hub */
			usleep_range(5, 10);
			gpio_direction_output(
				usb_hub->hub_reset_gpio, 1);
		}
		msleep(300);
		i2c_hub_attach();
		flex_enabled = false;
		break;

	default:
		break;
	}

	return 0;
}

static int usb2533_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	int ret = 0;
	struct device_node *node = i2c->dev.of_node;

	usb_hub = devm_kzalloc(&i2c->dev, sizeof(struct flex_hub),
				GFP_KERNEL);

	if (!usb_hub)
		return -ENOMEM;

	i2c_set_clientdata(i2c, usb_hub);
	usb_hub->client = i2c;
	usb_hub->dev = &usb_hub->client->dev;

	usb_hub->vbus_det = devm_regulator_get(usb_hub->dev, "vbus");
	if (IS_ERR(usb_hub->vbus_det)) {
		dev_err(usb_hub->dev, "Failed to get regulator: %ld\n",
			PTR_ERR(usb_hub->vbus_det));
		return PTR_ERR(usb_hub->vbus_det);
	}

	ret = regulator_enable(usb_hub->vbus_det);
	if (ret) {
		pr_err("unable to enable vbus_det\n");
		return ret;
	}

	usb_hub->hub_reset_gpio = of_get_named_gpio(
			node, "qcom,hub-reset-gpio", 0);
	if (usb_hub->hub_reset_gpio < 0)
		pr_debug("hub_reset_gpio is not available\n");

	usb_hub->usbeth_reset_gpio = of_get_named_gpio(
			node, "qcom,usbeth-reset-gpio", 0);
	if (usb_hub->usbeth_reset_gpio < 0)
		pr_debug("usbeth_reset_gpio is not available\n");

	if (gpio_is_valid(usb_hub->hub_reset_gpio)) {
		ret = devm_gpio_request(usb_hub->dev,
				usb_hub->hub_reset_gpio, "HUB_RESET");
		if (ret < 0) {
			dev_err(usb_hub->dev, "gpio req failed for hub_reset\n");
			return ret;
		}
	}

	if (gpio_is_valid(usb_hub->usbeth_reset_gpio)) {
		ret = devm_gpio_request(usb_hub->dev,
				usb_hub->usbeth_reset_gpio, "ETH_RESET");
		if (ret < 0) {
			dev_err(usb_hub->dev, "gpio req failed for usbeth_reset\n");
			return ret;
		}
	}

	usb_hub->usbdev_nb.notifier_call = flex_hub_usbdev_notify;
	usb_register_notify(&usb_hub->usbdev_nb);

	return 0;
}

static int usb2533_remove(struct i2c_client *i2c)
{
	struct flex_hub *usb_hub = i2c_get_clientdata(i2c);

	usb_unregister_notify(&usb_hub->usbdev_nb);
	devm_kfree(&i2c->dev, usb_hub);

	return 0;
}

static const struct i2c_device_id usb2533_flex_hub_id[] = {
	{ USB_HUB_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, flex_hub_id);

static const struct of_device_id flex_hub_of_match[] = {
	{ .compatible = "qcom,usb2533-flex-hub", },
	{},
};
MODULE_DEVICE_TABLE(of, flex_hub_of_match);

static struct i2c_driver flex_hub_driver = {
	.probe		= usb2533_probe,
	.remove		= usb2533_remove,
	.id_table	= usb2533_flex_hub_id,
	.driver = {
		.name = USB_HUB_I2C_NAME,
		.of_match_table = flex_hub_of_match,
	},
};

module_i2c_driver(flex_hub_driver);

MODULE_DESCRIPTION("Microchip USB2533 Flex Hub driver");
MODULE_LICENSE("GPL v2");
