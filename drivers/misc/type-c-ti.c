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
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>

#define TYPE_C_I2C_NAME	"usb-type-c-ti"
#define TI_I2C_DELAY_MS	100

#define CCD_DEFAULT		0x0
#define CCD_MEDIUM		0x1
#define CCD_HIGH		0x3
#define CCD_MASK		(0x30)	      /* charging current status */

#define MAX_CURRENT_BC1P2	500
#define MAX_CURRENT_MEDIUM     1500
#define MAX_CURRENT_HIGH       3000

#define TIUSB_1P8_VOL_MAX	1800000 /* uV */

#define TI_INTS_ATTACH_MASK	(0xC0)   /* current attach state interrupt */
#define TI_ATTACH_TO_DFP	0x2	 /* configured as UFP attaches to DFP */

#define TI_STS_8_REG		0x8
#define TI_STS_9_REG		0x9
#define TI_INTS_STATUS		BIT(4)

static bool disable_on_suspend;
module_param(disable_on_suspend , bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_on_suspend,
	"Whether to disable chip on suspend if state is not attached");

struct ti_usb_type_c {
	struct i2c_client	*client;
	struct power_supply	*usb_psy;
	int			max_current;
	u8			attach_state;
	u8			status_8_reg;
	u8			status_9_reg;
	int			enb_gpio;
	int			enb_gpio_polarity;
	struct regulator	*i2c_1p8;
};
static struct ti_usb_type_c *ti_usb;

static int tiusb_read_regdata(struct i2c_client *i2c)
{
	int rc;
	uint16_t saddr = i2c->addr;
	u8 attach_state, mask = TI_INTS_ATTACH_MASK;

	rc = i2c_smbus_read_byte_data(i2c, TI_STS_8_REG);
	if (rc < 0)
		return -EIO;
	ti_usb->status_8_reg = rc;

	rc = i2c_smbus_read_byte_data(i2c, TI_STS_9_REG);
	if (rc < 0)
		return -EIO;
	ti_usb->status_9_reg = rc;

	/* Clear interrupt */
	rc = i2c_smbus_write_byte_data(i2c, TI_STS_9_REG, ti_usb->status_9_reg);
	if (rc < 0)
		return -EIO;

	dev_dbg(&i2c->dev, "i2c read from 0x%x-[%x %x]\n", saddr,
				ti_usb->status_8_reg, ti_usb->status_9_reg);
	if (!(ti_usb->status_9_reg & TI_INTS_STATUS)) {
		dev_err(&i2c->dev, "intr_status is 0!, ignore interrupt\n");
		ti_usb->attach_state = false;
		return -EINVAL;
	}

	attach_state = ti_usb->status_9_reg & mask;
	ti_usb->attach_state = attach_state >> find_first_bit((void *)&mask, 8);

	return rc;
}

static int tiusb_update_power_supply(struct power_supply *psy, int limit)
{
	const union power_supply_propval ret = {limit,};

	/* Update USB of max charging current (500 corresponds to bc1.2 */
	if (psy->set_property)
		return psy->set_property(psy,
			POWER_SUPPLY_PROP_INPUT_CURRENT_MAX, &ret);

	return -ENODEV;
}

static void tiusb_update_max_current(struct ti_usb_type_c *ti_usb)
{
	u8 mask = CCD_MASK;
	u8 shift = find_first_bit((void *)&mask, 8);
	u8 chg_mode = ti_usb->status_8_reg & mask;

	chg_mode >>= shift;

	/* update to 0 if type-c UFP detached */
	if (ti_usb->attach_state != TI_ATTACH_TO_DFP) {
		dev_dbg(&ti_usb->client->dev, "attach_state: %x\n",
						ti_usb->attach_state);
		ti_usb->max_current = 0;
		return;
	}

	switch (chg_mode) {
	case CCD_DEFAULT:
		ti_usb->max_current = MAX_CURRENT_BC1P2;
		break;
	case CCD_MEDIUM:
		ti_usb->max_current = MAX_CURRENT_MEDIUM;
		break;
	case CCD_HIGH:
		ti_usb->max_current = MAX_CURRENT_HIGH;
		break;
	default:
		dev_dbg(&ti_usb->client->dev, "wrong chg mode %x\n", chg_mode);
		ti_usb->max_current = 500;
	}

	dev_dbg(&ti_usb->client->dev, "chg mode: %x, mA:%u, attach: %x\n",
			chg_mode, ti_usb->max_current, ti_usb->attach_state);
}

static irqreturn_t tiusb_irq(int irq, void *data)
{
	int ret;
	struct ti_usb_type_c *ti_usb = (struct ti_usb_type_c *)data;

	ret = tiusb_read_regdata(ti_usb->client);
	if (ret < 0)
		goto out;

	tiusb_update_max_current(ti_usb);

	ret = tiusb_update_power_supply(ti_usb->usb_psy, ti_usb->max_current);
	if (ret < 0)
		dev_err(&ti_usb->client->dev, "failed to notify USB-%d\n", ret);

out:
	return IRQ_HANDLED;
}

static int tiusb_gpio_config(struct ti_usb_type_c *ti, bool enable)
{
	int ret = 0;

	if (!enable) {
		gpio_set_value(ti_usb->enb_gpio, !ti_usb->enb_gpio_polarity);
		return 0;
	}

	ret = devm_gpio_request(&ti->client->dev, ti->enb_gpio,
					"ti_typec_enb_gpio");
	if (ret) {
		pr_err("unable to request gpio [%d]\n", ti->enb_gpio);
		return ret;
	}

	ret = gpio_direction_output(ti->enb_gpio, ti->enb_gpio_polarity);
	if (ret) {
		dev_err(&ti->client->dev, "set dir[%d] failed for gpio[%d]\n",
			ti->enb_gpio_polarity, ti->enb_gpio);
		return ret;
	}
	dev_dbg(&ti->client->dev, "set dir[%d] for gpio[%d]\n",
			ti->enb_gpio_polarity, ti->enb_gpio);

	gpio_set_value(ti->enb_gpio, ti->enb_gpio_polarity);
	msleep(TI_I2C_DELAY_MS);

	return ret;
}

static int tiusb_ldo_init(struct ti_usb_type_c *ti, bool init)
{
	int rc = 0;

	if (!init) {
		regulator_set_voltage(ti->i2c_1p8, 0, TIUSB_1P8_VOL_MAX);
		rc = regulator_disable(ti->i2c_1p8);
		return rc;
	}

	ti->i2c_1p8 = devm_regulator_get(&ti->client->dev, "vdd_io");
	if (IS_ERR(ti->i2c_1p8)) {
		rc = PTR_ERR(ti->i2c_1p8);
		dev_err(&ti->client->dev, "unable to get 1p8(%d)\n", rc);
		return rc;
	}
	rc = regulator_set_voltage(ti->i2c_1p8, TIUSB_1P8_VOL_MAX,
					TIUSB_1P8_VOL_MAX);
	if (rc) {
		dev_err(&ti->client->dev, "unable to set voltage(%d)\n", rc);
		goto put_1p8;
	}

	rc = regulator_enable(ti->i2c_1p8);
	if (rc) {
		dev_err(&ti->client->dev, "unable to enable 1p8-reg(%d)\n", rc);
		return rc;
	}

	return 0;

put_1p8:
	regulator_set_voltage(ti->i2c_1p8, 0, TIUSB_1P8_VOL_MAX);
	return rc;
}

static int tiusb_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	int ret;
	struct power_supply *usb_psy;
	struct device_node *np = i2c->dev.of_node;
	enum of_gpio_flags flags;

	usb_psy = power_supply_get_by_name("usb");
	if (!usb_psy) {
		dev_dbg(&i2c->dev, "USB power_supply not found, defer probe\n");
		return -EPROBE_DEFER;
	}

	ti_usb = devm_kzalloc(&i2c->dev, sizeof(struct ti_usb_type_c),
				GFP_KERNEL);
	if (!ti_usb)
		return -ENOMEM;

	i2c_set_clientdata(i2c, ti_usb);
	ti_usb->client = i2c;
	ti_usb->usb_psy = usb_psy;

	if (i2c->irq < 0) {
		dev_err(&i2c->dev, "irq not defined (%d)\n", i2c->irq);
		ret = -EINVAL;
		goto out;
	}

	/* override with module-param */
	if (!disable_on_suspend)
		disable_on_suspend = of_property_read_bool(np,
						"ti,disable-on-suspend");
	ti_usb->enb_gpio = of_get_named_gpio_flags(np, "ti,enb-gpio", 0,
							&flags);
	if (!gpio_is_valid(ti_usb->enb_gpio)) {
		dev_dbg(&i2c->dev, "enb gpio_get fail:%d\n", ti_usb->enb_gpio);
	} else {
		ti_usb->enb_gpio_polarity = !(flags & OF_GPIO_ACTIVE_LOW);
		ret = tiusb_gpio_config(ti_usb, true);
		if (ret)
			goto out;
	}

	ret = tiusb_ldo_init(ti_usb, true);
	if (ret) {
		dev_err(&ti_usb->client->dev, "i2c ldo init failed\n");
		goto gpio_disable;
	}

	ret = tiusb_read_regdata(i2c);
	if (ret == -EIO) {
		dev_err(&ti_usb->client->dev, "i2c access failed\n");
		ret = -EPROBE_DEFER;
		goto ldo_disable;
	}

	/* Update initial state to USB */
	tiusb_irq(i2c->irq, ti_usb);

	ret = devm_request_threaded_irq(&i2c->dev, i2c->irq, NULL, tiusb_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
					TYPE_C_I2C_NAME, ti_usb);
	if (ret) {
		dev_err(&i2c->dev, "irq(%d) req failed-%d\n", i2c->irq, ret);
		goto ldo_disable;
	}

	dev_dbg(&i2c->dev, "%s finished, addr:%d\n", __func__, i2c->addr);

	return 0;

ldo_disable:
	tiusb_ldo_init(ti_usb, false);
gpio_disable:
	if (gpio_is_valid(ti_usb->enb_gpio))
		tiusb_gpio_config(ti_usb, false);
out:
	return ret;
}

static int tiusb_remove(struct i2c_client *i2c)
{
	struct ti_usb_type_c *ti_usb = i2c_get_clientdata(i2c);

	tiusb_ldo_init(ti_usb, false);
	if (gpio_is_valid(ti_usb->enb_gpio))
		tiusb_gpio_config(ti_usb, false);
	devm_kfree(&i2c->dev, ti_usb);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int tiusb_i2c_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct ti_usb_type_c *ti = i2c_get_clientdata(i2c);

	dev_dbg(dev, "ti_usb PM suspend.. attach(%d) disable(%d)\n",
			ti->attach_state, disable_on_suspend);
	disable_irq(ti->client->irq);
	/* Keep type-c chip enabled during session */
	if (ti->attach_state)
		return 0;

	regulator_set_voltage(ti->i2c_1p8, 0, TIUSB_1P8_VOL_MAX);
	regulator_disable(ti->i2c_1p8);

	if (disable_on_suspend)
		gpio_set_value(ti->enb_gpio, !ti->enb_gpio_polarity);

	return 0;
}

static int tiusb_i2c_resume(struct device *dev)
{
	int rc;
	struct i2c_client *i2c = to_i2c_client(dev);
	struct ti_usb_type_c *ti = i2c_get_clientdata(i2c);

	dev_dbg(dev, "ti_usb PM resume\n");
	/* suspend was no-op, just re-enable interrupt */
	if (ti->attach_state) {
		enable_irq(ti->client->irq);
		return 0;
	}

	if (disable_on_suspend) {
		gpio_set_value(ti->enb_gpio, ti->enb_gpio_polarity);
		msleep(TI_I2C_DELAY_MS);
	}

	rc = regulator_set_voltage(ti->i2c_1p8, TIUSB_1P8_VOL_MAX,
					TIUSB_1P8_VOL_MAX);
	if (rc)
		dev_err(&ti->client->dev, "unable to set voltage(%d)\n", rc);

	rc = regulator_enable(ti->i2c_1p8);
	if (rc)
		dev_err(&ti->client->dev, "unable to enable 1p8-reg(%d)\n", rc);

	enable_irq(ti->client->irq);

	return rc;
}
#endif

static SIMPLE_DEV_PM_OPS(tiusb_i2c_pm_ops, tiusb_i2c_suspend,
			  tiusb_i2c_resume);

static const struct i2c_device_id tiusb_id[] = {
	{ TYPE_C_I2C_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, tiusb_id);

#ifdef CONFIG_OF
static const struct of_device_id tiusb_of_match[] = {
	{ .compatible = "ti,usb-type-c", },
	{},
};
MODULE_DEVICE_TABLE(of, tiusb_of_match);
#endif

static struct i2c_driver tiusb_driver = {
	.driver = {
		.name = TYPE_C_I2C_NAME,
		.of_match_table = of_match_ptr(tiusb_of_match),
		.pm	= &tiusb_i2c_pm_ops,
	},
	.probe		= tiusb_probe,
	.remove		= tiusb_remove,
	.id_table	= tiusb_id,
};

module_i2c_driver(tiusb_driver);

MODULE_DESCRIPTION("TI TypeC Detection driver");
MODULE_LICENSE("GPL v2");
