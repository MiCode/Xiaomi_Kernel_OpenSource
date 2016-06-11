/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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
#include <linux/usb/class-dual-role.h>

#define PERICOM_I2C_NAME	"usb-type-c-pericom"
#define PERICOM_I2C_DELAY_MS	30

#define CCD_DEFAULT		0x1
#define CCD_MEDIUM		0x2
#define CCD_HIGH		0x3

#define MAX_CURRENT_BC1P2	500
#define MAX_CURRENT_MEDIUM     1500
#define MAX_CURRENT_HIGH       3000

#define PIUSB_1P8_VOL_MAX	1800000 /* uV */

#define DETACH_DEBOUNCE_MS	500

static bool disable_on_suspend;
module_param(disable_on_suspend , bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(disable_on_suspend,
	"Whether to disable chip on suspend if state is not attached");

static unsigned int detach_debounce_delay_ms = DETACH_DEBOUNCE_MS;
module_param(detach_debounce_delay_ms , uint, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(detach_debounce_delay_ms,
	"Delay to use before updating mode during forced role-switch");

struct piusb_regs {
	u8		dev_id;
	u8		control;
#define CTL_MODE_UFP	(0x0)
#define CTL_MODE_DFP	(0x2)
#define CTL_MODE_DRP	(0x4)
#define CTL_MODE_MASK	(0x6)	      /* Port setting - ufp/dfp/drp */
	u8		intr_status;
#define	INTS_ATTACH	0x1
#define	INTS_DETACH	0x2
#define INTS_ATTACH_MASK	0x3   /* current attach state interrupt */

	u8		port_status;
#define STS_CCD_MASK	(0x60)	      /* charging current status */
#define STS_VBUS_MASK	(0x80)	      /* vbus status */

#define STS_MODE_DFP	(0x4)         /* port is connected to UFP */
#define STS_MODE_UFP	(0x8)         /* port is connected to DFP */
#define STS_MODE_MASK	(0x1C)	      /* Port Status- connected to ufp or dfp */
} __packed;

struct pi_usb_type_c {
	struct i2c_client		*client;
	struct piusb_regs		reg_data;
	struct power_supply		*usb_psy;
	int				max_current;
	bool				attach_state;
	int				enb_gpio;
	int				enb_gpio_polarity;
	struct regulator		*i2c_1p8;
	struct dual_role_phy_instance	*dual_role;
	struct dual_role_phy_desc	dr_desc;
	struct mutex			mutex;
	unsigned int			current_mode;
	struct delayed_work		handle_detach_work;
};
static struct pi_usb_type_c *pi_usb;

static enum dual_role_property pi_usb_dr_properties[] = {
	DUAL_ROLE_PROP_SUPPORTED_MODES,
	DUAL_ROLE_PROP_MODE,
	DUAL_ROLE_PROP_PR,
	DUAL_ROLE_PROP_DR,
};

/* requested mode */
static char *dual_mode_text[] = {
	"ufp", "dfp", "none"
};

static int piusb_i2c_enable(struct pi_usb_type_c *pi, bool enable, u8 mode);

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
		/* i2c read may fail if device not enabled or not present */
		dev_dbg(&i2c->dev, "i2c read from 0x%x failed %d\n", saddr, rc);
		pi_usb->attach_state = false;
		return -ENXIO;
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
	msleep(PERICOM_I2C_DELAY_MS);

	mutex_lock(&pi_usb->mutex);
	ret = piusb_read_regdata(pi_usb->client);
	if (ret < 0) {
		mutex_unlock(&pi_usb->mutex);
		goto out;
	}

	piusb_update_max_current(pi_usb);

	ret = piusb_update_power_supply(pi_usb->usb_psy, pi_usb->max_current);
	if (ret < 0)
		dev_err(&pi_usb->client->dev, "failed to notify USB-%d\n", ret);
	mutex_unlock(&pi_usb->mutex);

	/* On detach, go back to DRP if there is no immediate attach */
	if (pi_usb->attach_state)
		cancel_delayed_work_sync(&pi_usb->handle_detach_work);
	else if (pi_usb->current_mode != CTL_MODE_DRP)
		schedule_delayed_work(&pi_usb->handle_detach_work,
				msecs_to_jiffies(detach_debounce_delay_ms));
out:
	return IRQ_HANDLED;
}

static int piusb_i2c_write(struct pi_usb_type_c *pi, u8 *data, int len)
{
	int ret;
	struct i2c_msg msgs[] = {
		{
			.addr  = pi->client->addr,
			.flags = 0,
			.len   = len,
			.buf   = data,
		}
	};

	ret = i2c_transfer(pi->client->adapter, msgs, 1);
	if (ret != 1) {
		dev_err(&pi->client->dev, "i2c write to [%x] failed %d\n",
				pi->client->addr, ret);
		return -EIO;
	}
	return 0;
}

static int piusb_i2c_enable(struct pi_usb_type_c *pi, bool enable, u8 port_mode)
{
	u8 rst_assert[] = {0, 0x1};
	u8 rst_deassert[] = {0, port_mode};
	u8 pi_disable[] = {0, 0x80};

	if (!enable) {
		if (piusb_i2c_write(pi, pi_disable, sizeof(pi_disable)))
			return -EIO;
		return 0;
	}

	if (piusb_i2c_write(pi, rst_assert, sizeof(rst_assert)))
		return -EIO;

	msleep(PERICOM_I2C_DELAY_MS);
	/* Program type-c port (and CC lines) to appropriate state */
	if (piusb_i2c_write(pi, rst_deassert, sizeof(rst_deassert)))
		return -EIO;

	pi_usb->current_mode = port_mode;
	dev_dbg(&pi->client->dev, "mode set to - %d\n", port_mode);

	return 0;
}

static int piusb_gpio_config(struct pi_usb_type_c *pi, bool enable)
{
	int ret = 0;

	if (!enable) {
		gpio_set_value(pi_usb->enb_gpio, !pi_usb->enb_gpio_polarity);
		return 0;
	}

	ret = devm_gpio_request(&pi->client->dev, pi->enb_gpio,
					"pi_typec_enb_gpio");
	if (ret) {
		pr_err("unable to request gpio [%d]\n", pi->enb_gpio);
		return ret;
	}

	ret = gpio_direction_output(pi->enb_gpio, pi->enb_gpio_polarity);
	if (ret) {
		dev_err(&pi->client->dev, "set dir[%d] failed for gpio[%d]\n",
			pi->enb_gpio_polarity, pi->enb_gpio);
		return ret;
	}
	dev_dbg(&pi->client->dev, "set dir[%d] for gpio[%d]\n",
			pi->enb_gpio_polarity, pi->enb_gpio);

	gpio_set_value(pi->enb_gpio, pi->enb_gpio_polarity);
	msleep(PERICOM_I2C_DELAY_MS);

	return ret;
}

static int piusb_ldo_init(struct pi_usb_type_c *pi, bool init)
{
	int rc = 0;

	if (!init) {
		regulator_set_voltage(pi->i2c_1p8, 0, PIUSB_1P8_VOL_MAX);
		rc = regulator_disable(pi->i2c_1p8);
		return rc;
	}

	pi->i2c_1p8 = devm_regulator_get(&pi->client->dev, "vdd_io");
	if (IS_ERR(pi->i2c_1p8)) {
		rc = PTR_ERR(pi->i2c_1p8);
		dev_err(&pi->client->dev, "unable to get 1p8(%d)\n", rc);
		return rc;
	}
	rc = regulator_set_voltage(pi->i2c_1p8, PIUSB_1P8_VOL_MAX,
					PIUSB_1P8_VOL_MAX);
	if (rc) {
		dev_err(&pi->client->dev, "unable to set voltage(%d)\n", rc);
		goto put_1p8;
	}

	rc = regulator_enable(pi->i2c_1p8);
	if (rc) {
		dev_err(&pi->client->dev, "unable to enable 1p8-reg(%d)\n", rc);
		return rc;
	}

	return 0;

put_1p8:
	regulator_set_voltage(pi->i2c_1p8, 0, PIUSB_1P8_VOL_MAX);
	return rc;
}

static int piusb_dr_get_property(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop, unsigned int *val)
{
	u8 curr_port_status;
	int mode, pr, dr;

	mutex_lock(&pi_usb->mutex);

	curr_port_status = pi_usb->reg_data.port_status & STS_MODE_MASK;
	dev_dbg(&pi_usb->client->dev, "%s: prop(%d), ctl_mode: %u current sts_mode: %u\n",
			__func__, prop, pi_usb->current_mode, curr_port_status);

	/* Allow role-switch to finish before returning updated mode */
	if (prop == DUAL_ROLE_PROP_MODE &&
			((pi_usb->current_mode == CTL_MODE_DFP &&
			     curr_port_status != STS_MODE_DFP) ||
			(pi_usb->current_mode == CTL_MODE_UFP &&
				curr_port_status != STS_MODE_UFP))) {
		mutex_unlock(&pi_usb->mutex);
		msleep(detach_debounce_delay_ms);
		mutex_lock(&pi_usb->mutex);
	}

	curr_port_status = pi_usb->reg_data.port_status & STS_MODE_MASK;
	if (curr_port_status == STS_MODE_DFP) {
		dev_dbg(&pi_usb->client->dev, "%s: Mode is DFP\n", __func__);
		mode = DUAL_ROLE_PROP_MODE_DFP;
		pr = DUAL_ROLE_PROP_PR_SRC;
		dr = DUAL_ROLE_PROP_DR_HOST;
	} else {
		dev_dbg(&pi_usb->client->dev, "%s: Mode is UFP\n", __func__);
		mode = DUAL_ROLE_PROP_MODE_UFP;
		pr = DUAL_ROLE_PROP_PR_SNK;
		dr = DUAL_ROLE_PROP_DR_DEVICE;
	}

	mutex_unlock(&pi_usb->mutex);

	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		*val = mode;
		break;
	case DUAL_ROLE_PROP_PR:
		*val = pr;
		break;
	case DUAL_ROLE_PROP_DR:
		*val = dr;
		break;
	default:
		dev_warn(&pi_usb->client->dev, "%s: unsupported property %d\n",
				__func__, prop);
		return -ENODATA;
	}

	return 0;
}

static int piusb_dr_set_property(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop, const unsigned int *val)
{
	mutex_lock(&pi_usb->mutex);

	dev_dbg(&pi_usb->client->dev, "%s: prop(%d), curr_mode: %u, new_mode:%s\n",
		__func__, prop, pi_usb->current_mode, dual_mode_text[*val]);
	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		if (*val == DUAL_ROLE_PROP_MODE_UFP &&
		     pi_usb->current_mode != CTL_MODE_UFP) {
			piusb_i2c_enable(pi_usb, true, CTL_MODE_UFP);
		} else if (*val == DUAL_ROLE_PROP_MODE_DFP &&
			  pi_usb->current_mode != CTL_MODE_DFP) {
			piusb_i2c_enable(pi_usb, true, CTL_MODE_DFP);
		} else {
			dev_warn(&pi_usb->client->dev, "%s: unsupported mode %d\n",
				__func__, prop);
		}
		break;
	case DUAL_ROLE_PROP_PR:
	case DUAL_ROLE_PROP_DR:
	default:
		dev_warn(&pi_usb->client->dev, "%s: unsupported property %d\n",
				__func__, prop);
		mutex_unlock(&pi_usb->mutex);
		return -ENOTSUPP;
	}
	mutex_unlock(&pi_usb->mutex);

	return 0;
}

static int piusb_dr_prop_writeable(struct dual_role_phy_instance *dual_role,
		enum dual_role_property prop)
{
	switch (prop) {
	case DUAL_ROLE_PROP_MODE:
		return 1;
	case DUAL_ROLE_PROP_PR:
	case DUAL_ROLE_PROP_DR:
	default:
		break;
	}

	return 0;
}

static void piusb_handle_detach_work(struct work_struct *w)
{
	mutex_lock(&pi_usb->mutex);
	if (pi_usb->attach_state || pi_usb->current_mode == CTL_MODE_DRP) {
		dev_dbg(&pi_usb->client->dev, "%s: nothing to do (%u %u)\n",
			__func__, pi_usb->attach_state, pi_usb->current_mode);
		mutex_unlock(&pi_usb->mutex);
		return;
	}
	dev_dbg(&pi_usb->client->dev, "%s: Set DRP mode on detach\n", __func__);

	/* On real detach (no soft-switch), move to DRP mode */
	piusb_i2c_enable(pi_usb, true, CTL_MODE_DRP);
	mutex_unlock(&pi_usb->mutex);
}

static int piusb_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
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

	/* override with module-param */
	if (!disable_on_suspend)
		disable_on_suspend = of_property_read_bool(np,
						"pericom,disable-on-suspend");
	pi_usb->enb_gpio = of_get_named_gpio_flags(np, "pericom,enb-gpio", 0,
							&flags);
	if (!gpio_is_valid(pi_usb->enb_gpio)) {
		dev_dbg(&i2c->dev, "enb gpio_get fail:%d\n", pi_usb->enb_gpio);
	} else {
		pi_usb->enb_gpio_polarity = !(flags & OF_GPIO_ACTIVE_LOW);
		ret = piusb_gpio_config(pi_usb, true);
		if (ret)
			goto out;
	}

	ret = piusb_ldo_init(pi_usb, true);
	if (ret) {
		dev_err(&pi_usb->client->dev, "i2c ldo init failed\n");
		goto gpio_disable;
	}

	ret = piusb_i2c_enable(pi_usb, true, CTL_MODE_DRP);
	if (ret) {
		dev_err(&pi_usb->client->dev, "i2c access failed\n");
		ret = -EPROBE_DEFER;
		goto ldo_disable;
	}

	mutex_init(&pi_usb->mutex);
	/* Update initial state to USB */
	piusb_irq(i2c->irq, pi_usb);

	ret = devm_request_threaded_irq(&i2c->dev, i2c->irq, NULL, piusb_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
					PERICOM_I2C_NAME, pi_usb);
	if (ret) {
		dev_err(&i2c->dev, "irq(%d) req failed-%d\n", i2c->irq, ret);
		goto i2c_disable;
	}

	/*
	 * Register the Android dual-role class (/sys/class/dual_role_usb/)
	 */
	pi_usb->dr_desc.name = "otg_default";
	pi_usb->dr_desc.supported_modes = DUAL_ROLE_SUPPORTED_MODES_DFP_AND_UFP;
	pi_usb->dr_desc.properties = pi_usb_dr_properties;
	pi_usb->dr_desc.num_properties = ARRAY_SIZE(pi_usb_dr_properties);
	pi_usb->dr_desc.get_property = piusb_dr_get_property;
	pi_usb->dr_desc.set_property = piusb_dr_set_property;
	pi_usb->dr_desc.property_is_writeable = piusb_dr_prop_writeable;

	pi_usb->dual_role = devm_dual_role_instance_register(&i2c->dev,
			&pi_usb->dr_desc);
	if (IS_ERR(pi_usb->dual_role))
		dev_dbg(&i2c->dev, "failed to register dual_role_class\n");

	INIT_DELAYED_WORK(&pi_usb->handle_detach_work,
				  piusb_handle_detach_work);

	dev_dbg(&i2c->dev, "%s finished, addr:%d\n", __func__, i2c->addr);

	return 0;

i2c_disable:
	piusb_i2c_enable(pi_usb, false, pi_usb->current_mode);
ldo_disable:
	piusb_ldo_init(pi_usb, false);
gpio_disable:
	if (gpio_is_valid(pi_usb->enb_gpio))
		piusb_gpio_config(pi_usb, false);
out:
	return ret;
}

static int piusb_remove(struct i2c_client *i2c)
{
	struct pi_usb_type_c *pi_usb = i2c_get_clientdata(i2c);

	piusb_i2c_enable(pi_usb, false, pi_usb->current_mode);
	piusb_ldo_init(pi_usb, false);
	if (gpio_is_valid(pi_usb->enb_gpio))
		piusb_gpio_config(pi_usb, false);
	devm_kfree(&i2c->dev, pi_usb);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int piusb_i2c_suspend(struct device *dev)
{
	struct i2c_client *i2c = to_i2c_client(dev);
	struct pi_usb_type_c *pi = i2c_get_clientdata(i2c);

	dev_dbg(dev, "pi_usb PM suspend.. attach(%d) disable(%d)\n",
			pi->attach_state, disable_on_suspend);
	disable_irq(pi->client->irq);
	/* Keep type-c chip enabled during session */
	if (pi->attach_state)
		return 0;

	if (disable_on_suspend)
		piusb_i2c_enable(pi, false, pi->current_mode);

	regulator_set_voltage(pi->i2c_1p8, 0, PIUSB_1P8_VOL_MAX);
	regulator_disable(pi->i2c_1p8);

	if (disable_on_suspend)
		gpio_set_value(pi->enb_gpio, !pi->enb_gpio_polarity);

	return 0;
}

static int piusb_i2c_resume(struct device *dev)
{
	int rc;
	struct i2c_client *i2c = to_i2c_client(dev);
	struct pi_usb_type_c *pi = i2c_get_clientdata(i2c);

	dev_dbg(dev, "pi_usb PM resume\n");
	/* suspend was no-op, just re-enable interrupt */
	if (pi->attach_state) {
		enable_irq(pi->client->irq);
		return 0;
	}

	if (disable_on_suspend) {
		gpio_set_value(pi->enb_gpio, pi->enb_gpio_polarity);
		msleep(PERICOM_I2C_DELAY_MS);
	}

	rc = regulator_set_voltage(pi->i2c_1p8, PIUSB_1P8_VOL_MAX,
					PIUSB_1P8_VOL_MAX);
	if (rc)
		dev_err(&pi->client->dev, "unable to set voltage(%d)\n", rc);

	rc = regulator_enable(pi->i2c_1p8);
	if (rc)
		dev_err(&pi->client->dev, "unable to enable 1p8-reg(%d)\n", rc);

	if (disable_on_suspend)
		rc = piusb_i2c_enable(pi, true, pi->current_mode);
	enable_irq(pi->client->irq);

	return rc;
}
#endif

static SIMPLE_DEV_PM_OPS(piusb_i2c_pm_ops, piusb_i2c_suspend,
			  piusb_i2c_resume);

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
		.pm	= &piusb_i2c_pm_ops,
	},
	.probe		= piusb_probe,
	.remove		= piusb_remove,
	.id_table	= piusb_id,
};

module_i2c_driver(piusb_driver);

MODULE_DESCRIPTION("Pericom TypeC Detection driver");
MODULE_LICENSE("GPL v2");
