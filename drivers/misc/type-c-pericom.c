/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/interrupt.h>
#include <linux/power_supply.h>

#define PERICOM_I2C_NAME	"usb-type-c-pericom"

#define CCD_DEFAULT		0x1
#define CCD_MEDIUM		0x2
#define CCD_HIGH		0x3

#define MAX_CURRENT_BC1P2	500
#define MAX_CURRENT_MEDIUM     1500
#define MAX_CURRENT_HIGH       3000

struct piusb_regs {
	u8		dev_id;
	u8		control;
	u8		intr_status;
#define	INTS_ATTACH	0x1
#define	INTS_DETACH	0x2
#define INTS_ATTACH_MASK	0x3   /* current attach state interrupt */

	u8		port_status;
#define STS_PORT_MASK	(0x1c)	      /* attached port status  - device/host */
#define STS_CCD_MASK	(0x60)	      /* charging current status */
#define STS_VBUS_MASK	(0x80)	      /* vbus status */

} __packed;

struct pi_usb_type_c {
	struct i2c_client	*client;
	struct piusb_regs	reg_data;
	struct power_supply	*usb_psy;
	int			max_current;
	bool			attach_state;
};
static struct pi_usb_type_c *pi_usb;

static int piusb_read_regdata(struct i2c_client *i2c)
{
	int rc;
	int data_length = sizeof(pi_usb->reg_data);
	uint16_t saddr = i2c->addr;
	u8 attach_state;
	struct i2c_msg msgs[] = {
		{
			.addr  = saddr,
			.flags = I2C_M_RD,
			.len   = data_length,
			.buf   = (u8 *)&pi_usb->reg_data,
		}
	};

	rc = i2c_transfer(i2c->adapter, msgs, 1);
	if (rc < 0) {
		/* i2c read may fail if type-c plug removed, treat as detach */
		dev_dbg(&i2c->dev, "i2c read from 0x%x failed %d\n", saddr, rc);
		pi_usb->attach_state = false;
		return 0;
	}

	dev_dbg(&i2c->dev, "i2c read from 0x%x-[%x %x %x %x]\n", saddr,
		    pi_usb->reg_data.dev_id, pi_usb->reg_data.control,
		    pi_usb->reg_data.intr_status, pi_usb->reg_data.port_status);

	if (!pi_usb->reg_data.intr_status) {
		dev_err(&i2c->dev, "intr_status is 0!, ignore interrupt\n");
		pi_usb->attach_state = false;
		return -EINVAL;
	}

	attach_state = pi_usb->reg_data.intr_status & INTS_ATTACH_MASK;
	pi_usb->attach_state = (attach_state == INTS_ATTACH) ? true : false;

	return rc;
}

static int piusb_update_power_supply(struct power_supply *psy, int limit)
{
	const union power_supply_propval ret = {limit,};

	/* Update USB of max charging current (500 corresponds to bc1.2 */
	if (psy->set_property)
		return psy->set_property(psy,
			POWER_SUPPLY_PROP_INPUT_CURRENT_MAX, &ret);

	return -ENODEV;
}

static void piusb_update_max_current(struct pi_usb_type_c *pi_usb)
{
	u8 mask = STS_CCD_MASK;
	u8 shift = find_first_bit((void *)&mask, 8);
	u8 chg_mode = pi_usb->reg_data.port_status & mask;

	chg_mode >>= shift;

	/* update to 0 if type-c detached */
	if (!pi_usb->attach_state) {
		pi_usb->max_current = 0;
		return;
	}

	switch (chg_mode) {
	case CCD_DEFAULT:
		pi_usb->max_current = MAX_CURRENT_BC1P2;
		break;
	case CCD_MEDIUM:
		pi_usb->max_current = MAX_CURRENT_MEDIUM;
		break;
	case CCD_HIGH:
		pi_usb->max_current = MAX_CURRENT_HIGH;
		break;
	default:
		dev_dbg(&pi_usb->client->dev, "wrong chg mode %x\n", chg_mode);
		pi_usb->max_current = MAX_CURRENT_BC1P2;
	}

	dev_dbg(&pi_usb->client->dev, "chg mode: %x, mA:%u\n", chg_mode,
							pi_usb->max_current);
}

static irqreturn_t piusb_irq(int irq, void *data)
{
	int ret;
	struct pi_usb_type_c *pi_usb = (struct pi_usb_type_c *)data;

	/* i2c register update takes time, 30msec sleep required as per HPG */
	msleep(30);

	ret = piusb_read_regdata(pi_usb->client);
	if (ret < 0)
		goto out;

	piusb_update_max_current(pi_usb);

	ret = piusb_update_power_supply(pi_usb->usb_psy, pi_usb->max_current);
	if (ret < 0)
		dev_err(&pi_usb->client->dev, "failed to notify USB-%d\n", ret);

out:
	return IRQ_HANDLED;
}

static int piusb_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	int ret;
	struct power_supply *usb_psy;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_dbg(&i2c->dev, "USB power_supply not found, defer probe\n");
		return -EPROBE_DEFER;
	}

	pi_usb = devm_kzalloc(&i2c->dev, sizeof(struct pi_usb_type_c),
				GFP_KERNEL);
	if (!pi_usb)
		return -ENOMEM;

	i2c_set_clientdata(i2c, pi_usb);
	pi_usb->client = i2c;
	pi_usb->usb_psy = usb_psy;

	if (i2c->irq < 0) {
		dev_err(&i2c->dev, "irq not defined (%d)\n", i2c->irq);
		ret = -EINVAL;
		goto out;
	}

	/* Update initial state to USB */
	piusb_irq(i2c->irq, pi_usb);

	ret = devm_request_threaded_irq(&i2c->dev, i2c->irq, NULL, piusb_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
					PERICOM_I2C_NAME, pi_usb);
	if (ret) {
		dev_err(&i2c->dev, "irq(%d) req failed-%d\n", i2c->irq, ret);
		goto out;
	}

	dev_dbg(&i2c->dev, "%s finished, addr:%d\n", __func__, i2c->addr);

	return 0;

out:
	return ret;
}

static int piusb_remove(struct i2c_client *i2c)
{
	struct pi_usb_type_c *pi_usb = i2c_get_clientdata(i2c);

	devm_kfree(&i2c->dev, pi_usb);

	return 0;
}

static const struct i2c_device_id piusb_id[] = {
	{ PERICOM_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, piusb_id);

#ifdef CONFIG_OF
static const struct of_device_id piusb_of_match[] = {
	{ .compatible = "pericom,usb-type-c", },
	{},
};
MODULE_DEVICE_TABLE(of, piusb_of_match);
#endif

static struct i2c_driver piusb_driver = {
	.driver = {
		.name = PERICOM_I2C_NAME,
		.of_match_table = of_match_ptr(piusb_of_match),
	},
	.probe		= piusb_probe,
	.remove		= piusb_remove,
	.id_table	= piusb_id,
};

module_i2c_driver(piusb_driver);

MODULE_DESCRIPTION("Pericom TypeC Detection driver");
MODULE_LICENSE("GPL v2");
