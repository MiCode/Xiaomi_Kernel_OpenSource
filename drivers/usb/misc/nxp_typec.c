/* Copyright (c) 2018, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/extcon.h>
#include <linux/regmap.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>

#define CONTROL_REG		0x02
#define STATUS_REG		0x04
#define INTR_MASK_REG		0x18
#define INTR_STATUS_REG		0x19

#define DISABLE_CABLE_INTR	0x05
#define ENABLE_ROLE_CHANGE_INTR	0x08

#define ROLE_CHANGE_INTR_STATUS	BIT(3)
#define GET_PORT_ROLE(n)	(((n) >> 2) & 0x03)

struct nxpusb5150a {
	struct device *dev;
	struct regmap *regmap;
	struct extcon_dev *extcon;

	struct gpio_desc *vbus_out_gpio;
};

enum port_role  {
	PORT_ROLE_DISCONNECTED,
	PORT_ROLE_UFP,
	PORT_ROLE_DFP,
};

static const unsigned int nxpusb5150a_extcon_cable[] = {
	EXTCON_USB_HOST,
	EXTCON_NONE,
};

static const struct regmap_config nxpusb5150a_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
};

static int nxpusb5150a_read_reg(struct nxpusb5150a *nxp, u8 reg, u8 *val)
{
	int ret;
	unsigned int reg_val;

	ret = regmap_read(nxp->regmap, (unsigned int)reg, &reg_val);
	if (ret < 0) {
		dev_err(nxp->dev, "Failed to read reg:0x%02x\n", reg);
		return ret;
	}

	*val = (u8)reg_val;
	dev_dbg(nxp->dev, "read reg:0x%02x val:0x%02x\n", reg, *val);

	return 0;
}

static int nxpusb5150a_write_reg(struct nxpusb5150a *nxp, u8 reg, u8 mask,
									u8 val)
{
	int ret;
	u8 reg_val;

	ret = nxpusb5150a_read_reg(nxp, reg, &reg_val);
	if (ret)
		return ret;

	val |= (reg_val & mask);
	ret = regmap_write(nxp->regmap, (unsigned int)reg, (unsigned int)val);
	if (ret < 0) {
		dev_err(nxp->dev, "failed to write 0x%02x to reg: 0x%02x\n",
							val, reg);
		return ret;
	}

	dev_dbg(nxp->dev, "write reg:0x%02x val:0x%02x\n", reg, val);

	return 0;
}
static irqreturn_t nxusb5150a_cable_thread_handler(int irq, void *dev_id)
{
	struct nxpusb5150a *nxp = dev_id;
	union extcon_property_value val;
	u8 status;
	enum port_role role;
	int ret;

	ret = nxpusb5150a_read_reg(nxp, INTR_STATUS_REG, &status);
	if (ret)
		goto error;

	if (!(status & ROLE_CHANGE_INTR_STATUS)) {
		dev_dbg(nxp->dev, "No change in role of port\n");
		return IRQ_HANDLED;
	}

	ret = nxpusb5150a_read_reg(nxp, STATUS_REG, &status);
	if (ret)
		goto error;

	role = GET_PORT_ROLE(status);
	switch (role) {
	case PORT_ROLE_DFP:
		val.intval = 1;
		if (nxp->vbus_out_gpio)
			gpiod_set_value_cansleep(nxp->vbus_out_gpio, 1);
		extcon_set_state_sync(nxp->extcon, EXTCON_USB_HOST, 1);
		break;

	case PORT_ROLE_UFP:
		dev_err(nxp->dev, "Port does not support device mode(UFP)\n");
	case PORT_ROLE_DISCONNECTED:
	default:
		if (nxp->vbus_out_gpio)
			gpiod_set_value_cansleep(nxp->vbus_out_gpio, 0);
		extcon_set_state_sync(nxp->extcon, EXTCON_USB_HOST, 0);
		break;
	}

	return IRQ_HANDLED;

	/*
	 * Register read failures are treated as errors.
	 * So notify the state as disconnected whenever the failures are seen.
	 */
error:
	if (nxp->vbus_out_gpio)
		gpiod_set_value_cansleep(nxp->vbus_out_gpio, 0);
	extcon_set_state_sync(nxp->extcon, EXTCON_USB_HOST, 0);

	return IRQ_HANDLED;
}

/*
 * Configure the NXP 5150a chipset to enable the role change
 * interrupt and disable the remaining interrupts.
 */
static int nxpusb5150a_configure_interrupts(struct nxpusb5150a *nxp)
{
	int ret;

	/*
	 * Cable detach and attach interrupts are enabled after
	 * reset. So disable the same.
	 * Also set the mode to DRP so that the chipset can
	 * detect DFP connections.
	 */
	ret = nxpusb5150a_write_reg(nxp, CONTROL_REG, ~DISABLE_CABLE_INTR,
						DISABLE_CABLE_INTR);
	if (ret)
		return ret;

	/* Enable the interrupt to detect role change.*/
	ret = nxpusb5150a_write_reg(nxp, INTR_MASK_REG,
				~ENABLE_ROLE_CHANGE_INTR, 0);
	if (ret)
		return ret;

	return 0;
}

static int nxpusb5150a_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct nxpusb5150a *nxp;
	int ret;

	nxp = devm_kzalloc(dev, sizeof(nxp), GFP_KERNEL);
	if (!nxp)
		return -ENOMEM;

	nxp->dev = dev;

	nxp->regmap = devm_regmap_init_i2c(client, &nxpusb5150a_regmap);
	if (IS_ERR(nxp->regmap)) {
		dev_err(nxp->dev, "Failed to allocate register map: %d\n",
							PTR_ERR(nxp->regmap));
		return PTR_ERR(nxp->regmap);
	}

	i2c_set_clientdata(client, nxp);

	nxp->extcon = devm_extcon_dev_allocate(dev, nxpusb5150a_extcon_cable);
	if (IS_ERR(nxp->extcon)) {
		dev_err(nxp->dev, "Failed to allocate extcon device: %d\n",
							PTR_ERR(nxp->extcon));
		return PTR_ERR(nxp->extcon);
	}

	ret = devm_extcon_dev_register(dev, nxp->extcon);
	if (ret) {
		dev_err(nxp->dev, "Failed to register extcon device: %d\n",
									ret);
		return ret;
	}

	nxp->vbus_out_gpio = devm_gpiod_get_optional(nxp->dev, "vbus-out",
							GPIOD_OUT_HIGH);
	if (!nxp->vbus_out_gpio)
		dev_err(dev, "No external VBUS GPIO supplied\n");

	if (!client->irq) {
		dev_err(dev, "no client irq\n");
		return -EINVAL;
	}

	ret = devm_request_threaded_irq(dev, client->irq, NULL,
					nxusb5150a_cable_thread_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					client->name, nxp);
	if (ret < 0) {
		dev_err(nxp->dev, "Failed to request interrupt handler\n");
		return ret;
	}

	ret = nxpusb5150a_configure_interrupts(nxp);
	if (ret) {
		dev_err(dev, "failed to configure interrupts\n");
		return ret;
	}

	nxusb5150a_cable_thread_handler(client->irq, nxp);

	return 0;
}

static const struct of_device_id nxpusb5150a_match_table[] = {
	{ .compatible = "nxp,5150a",},
	{ },
};

static const struct i2c_device_id nxpusb5150a_table[] = {
	{ "nxpusb5150a" },
	{}
};
MODULE_DEVICE_TABLE(i2c, nxpusb5150a_table);

static struct i2c_driver nxpusb5150a_driver = {
	.driver = {
		.name = "nxpusb5150a",
		.owner = THIS_MODULE,
		.of_match_table = nxpusb5150a_match_table,
	},
	.probe_new	= nxpusb5150a_probe,
	.id_table	= nxpusb5150a_table,
};
module_i2c_driver(nxpusb5150a_driver);
