/* drivers/input/misc/ots_pat9125/pat9125_linux_driver.c
 *
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 */

#include <linux/input.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include "pixart_ots.h"

struct pixart_pat9125_data {
	struct i2c_client *client;
	struct input_dev *input;
	int irq_gpio;
	u32 press_keycode;
	bool press_en;
	bool inverse_x;
	bool inverse_y;
	struct regulator *vdd;
	struct regulator *vld;
	struct pinctrl *pinctrl;
	struct pinctrl_state *pinctrl_state_active;
	struct pinctrl_state *pinctrl_state_suspend;
	struct pinctrl_state *pinctrl_state_release;
};

/* Declaration of suspend and resume functions */
static int pat9125_suspend(struct device *dev);
static int pat9125_resume(struct device *dev);

static int pat9125_i2c_write(struct i2c_client *client, u8 reg, u8 *data,
		int len)
{
	u8 buf[MAX_BUF_SIZE];
	int ret = 0, i;
	struct device *dev = &client->dev;

	buf[0] = reg;
	if (len >= MAX_BUF_SIZE) {
		dev_err(dev, "%s Failed: buffer size is %d [Max Limit is %d]\n",
			__func__, len, MAX_BUF_SIZE);
		return -ENODEV;
	}
	for (i = 0 ; i < len; i++)
		buf[i+1] = data[i];
	/* Returns negative errno, or else the number of bytes written. */
	ret = i2c_master_send(client, buf, len+1);
	if (ret != len+1)
		dev_err(dev, "%s Failed: writing to reg 0x%x\n", __func__, reg);

	return ret;
}

static int pat9125_i2c_read(struct i2c_client *client, u8 reg, u8 *data)
{
	u8 buf[MAX_BUF_SIZE];
	int ret;
	struct device *dev = &client->dev;

	buf[0] = reg;
	/*
	 * If everything went ok (1 msg transmitted), return #bytes transmitted,
	 * else error code. thus if transmit is ok return value 1
	 */
	ret = i2c_master_send(client, buf, 1);
	if (ret != 1) {
		dev_err(dev, "%s Failed: writing to reg 0x%x\n", __func__, reg);
		return ret;
	}
	/* returns negative errno, or else the number of bytes read */
	ret = i2c_master_recv(client, buf, 1);
	if (ret != 1) {
		dev_err(dev, "%s Failed: reading reg 0x%x\n", __func__, reg);
		return ret;
	}
	*data = buf[0];

	return ret;
}

u8 read_data(struct i2c_client *client, u8 addr)
{
	u8 data = 0xff;

	pat9125_i2c_read(client, addr, &data);
	return data;
}

void write_data(struct i2c_client *client, u8 addr, u8 data)
{
	pat9125_i2c_write(client, addr, &data, 1);
}

static irqreturn_t pat9125_irq(int irq, void *dev_data)
{
	u8 delta_x = 0, delta_y = 0, motion;
	struct pixart_pat9125_data *data = dev_data;
	struct input_dev *ipdev = data->input;
	struct device *dev = &data->client->dev;

	motion = read_data(data->client, PIXART_PAT9125_MOTION_STATUS_REG);
	do {
		/* check if MOTION bit is set or not */
		if (motion & PIXART_PAT9125_VALID_MOTION_DATA) {
			delta_x = read_data(data->client,
					PIXART_PAT9125_DELTA_X_LO_REG);
			delta_y = read_data(data->client,
					PIXART_PAT9125_DELTA_Y_LO_REG);

			/* Inverse x depending upon the device orientation */
			delta_x = (data->inverse_x) ? -delta_x : delta_x;
			/* Inverse y depending upon the device orientation */
			delta_y = (data->inverse_y) ? -delta_y : delta_y;
		}

		dev_dbg(dev, "motion = %x, delta_x = %x, delta_y = %x\n",
					motion, delta_x, delta_y);

		if (delta_x != 0) {
			/* Send delta_x as REL_WHEEL for rotation */
			input_report_rel(ipdev, REL_WHEEL, (s8) delta_x);
			input_sync(ipdev);
		}

		if (data->press_en && delta_y != 0) {
			if ((s8) delta_y > 0) {
				/* Send DOWN event for press keycode */
				input_report_key(ipdev, data->press_keycode, 1);
				input_sync(ipdev);
			} else {
				/* Send UP event for press keycode */
				input_report_key(ipdev, data->press_keycode, 0);
				input_sync(ipdev);
			}
		}
		usleep_range(PIXART_SAMPLING_PERIOD_US_MIN,
					PIXART_SAMPLING_PERIOD_US_MAX);

		motion = read_data(data->client,
				PIXART_PAT9125_MOTION_STATUS_REG);
	} while (motion & PIXART_PAT9125_VALID_MOTION_DATA);

	return IRQ_HANDLED;
}

static ssize_t pat9125_suspend_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct pixart_pat9125_data *data =
		(struct pixart_pat9125_data *) dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	int mode;

	if (kstrtoint(buf, 10, &mode)) {
		dev_err(dev, "failed to read input for sysfs\n");
		return -EINVAL;
	}

	if (mode == 1)
		pat9125_suspend(&client->dev);
	else if (mode == 0)
		pat9125_resume(&client->dev);

	return count;
}

static ssize_t pat9125_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	char s[256], *p = s;
	int reg_data = 0, i;
	long rd_addr, wr_addr, wr_data;
	struct pixart_pat9125_data *data =
		(struct pixart_pat9125_data *) dev_get_drvdata(dev);
	struct i2c_client *client = data->client;

	for (i = 0; i < sizeof(s); i++)
		s[i] = buf[i];
	*(s+1) = '\0';
	*(s+4) = '\0';
	*(s+7) = '\0';
	/* example(in console): echo w 12 34 > rw_reg */
	if (*p == 'w') {
		p += 2;
		if (!kstrtol(p, 16, &wr_addr)) {
			p += 3;
			if (!kstrtol(p, 16, &wr_data)) {
				dev_dbg(dev, "w 0x%x 0x%x\n",
					(u8)wr_addr, (u8)wr_data);
				write_data(client, (u8)wr_addr, (u8)wr_data);
			}
		}
	}
	/* example(in console): echo r 12 > rw_reg */
	else if (*p == 'r') {
		p += 2;

		if (!kstrtol(p, 16, &rd_addr)) {
			reg_data = read_data(client, (u8)rd_addr);
			dev_dbg(dev, "r 0x%x 0x%x\n",
				(unsigned int)rd_addr, reg_data);
		}
	}
	return count;
}

static ssize_t pat9125_test_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return 0;
}

static DEVICE_ATTR(suspend, S_IRUGO | S_IWUSR | S_IWGRP,
		NULL, pat9125_suspend_store);
static DEVICE_ATTR(test, S_IRUGO | S_IWUSR | S_IWGRP,
		pat9125_test_show, pat9125_test_store);

static struct attribute *pat9125_attr_list[] = {
	&dev_attr_test.attr,
	&dev_attr_suspend.attr,
	NULL,
};

static struct attribute_group pat9125_attr_grp = {
	.attrs = pat9125_attr_list,
};

static int pixart_pinctrl_init(struct pixart_pat9125_data *data)
{
	int err;
	struct device *dev = &data->client->dev;

	data->pinctrl = devm_pinctrl_get(&(data->client->dev));
	if (IS_ERR_OR_NULL(data->pinctrl)) {
		err = PTR_ERR(data->pinctrl);
		dev_err(dev, "Target does not use pinctrl %d\n", err);
		return err;
	}

	data->pinctrl_state_active = pinctrl_lookup_state(data->pinctrl,
			PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(data->pinctrl_state_active)) {
		err = PTR_ERR(data->pinctrl_state_active);
		dev_err(dev, "Can not lookup active pinctrl state %d\n", err);
		return err;
	}

	data->pinctrl_state_suspend = pinctrl_lookup_state(data->pinctrl,
			PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(data->pinctrl_state_suspend)) {
		err = PTR_ERR(data->pinctrl_state_suspend);
		dev_err(dev, "Can not lookup suspend pinctrl state %d\n", err);
		return err;
	}

	data->pinctrl_state_release = pinctrl_lookup_state(data->pinctrl,
			PINCTRL_STATE_RELEASE);
	if (IS_ERR_OR_NULL(data->pinctrl_state_release)) {
		err = PTR_ERR(data->pinctrl_state_release);
		dev_err(dev, "Can not lookup release pinctrl state %d\n", err);
		return err;
	}
	return 0;
}

static int pat9125_regulator_init(struct pixart_pat9125_data *data)
{
	int err = 0;
	struct device *dev = &data->client->dev;

	data->vdd = devm_regulator_get(dev, "vdd");
	if (IS_ERR(data->vdd)) {
		dev_err(dev, "Failed to get regulator vdd %ld\n",
					PTR_ERR(data->vdd));
		return PTR_ERR(data->vdd);
	}

	data->vld = devm_regulator_get(dev, "vld");
	if (IS_ERR(data->vld)) {
		dev_err(dev, "Failed to get regulator vld %ld\n",
					PTR_ERR(data->vld));
		return PTR_ERR(data->vld);
	}

	err = regulator_set_voltage(data->vdd, VDD_VTG_MIN_UV, VDD_VTG_MAX_UV);
	if (err) {
		dev_err(dev, "Failed to set voltage for vdd reg %d\n", err);
		return err;
	}

	err = regulator_set_optimum_mode(data->vdd, VDD_ACTIVE_LOAD_UA);
	if (err < 0) {
		dev_err(dev, "Failed to set opt mode for vdd reg %d\n", err);
		return err;
	}

	err = regulator_set_voltage(data->vld, VLD_VTG_MIN_UV, VLD_VTG_MAX_UV);
	if (err) {
		dev_err(dev, "Failed to set voltage for vld reg %d\n", err);
		return err;
	}

	err = regulator_set_optimum_mode(data->vld, VLD_ACTIVE_LOAD_UA);
	if (err < 0) {
		dev_err(dev, "Failed to set opt mode for vld reg %d\n", err);
		return err;
	}

	return 0;
}

static int pat9125_power_on(struct pixart_pat9125_data *data, bool on)
{
	int err = 0;
	struct device *dev = &data->client->dev;

	if (on) {
		err = regulator_enable(data->vdd);
		if (err) {
			dev_err(dev, "Failed to enable vdd reg %d\n", err);
			return err;
		}

		usleep_range(DELAY_BETWEEN_REG_US, DELAY_BETWEEN_REG_US + 1);

		/*
		 * Initialize pixart sensor after some delay, when vdd
		 * regulator is enabled
		 */
		if (!ots_sensor_init(data->client)) {
			err = -ENODEV;
			dev_err(dev, "Failed to initialize sensor %d\n", err);
			return err;
		}

		err = regulator_enable(data->vld);
		if (err) {
			dev_err(dev, "Failed to enable vld reg %d\n", err);
			return err;
		}
	} else {
		err = regulator_disable(data->vld);
		if (err) {
			dev_err(dev, "Failed to disable vld reg %d\n", err);
			return err;
		}

		err = regulator_disable(data->vdd);
		if (err) {
			dev_err(dev, "Failed to disable vdd reg %d\n", err);
			return err;
		}
	}

	return 0;
}

static int pat9125_parse_dt(struct device *dev,
		struct pixart_pat9125_data *data)
{
	struct device_node *np = dev->of_node;
	u32 temp_val;
	int ret;

	data->inverse_x = of_property_read_bool(np, "pixart,inverse-x");
	data->inverse_y = of_property_read_bool(np, "pixart,inverse-y");
	data->press_en = of_property_read_bool(np, "pixart,press-enabled");
	if (data->press_en) {
		ret = of_property_read_u32(np, "pixart,press-keycode",
						&temp_val);
		if (!ret) {
			data->press_keycode = temp_val;
		} else {
			dev_err(dev, "Unable to parse press-keycode\n");
			return ret;
		}
	}

	data->irq_gpio = of_get_named_gpio_flags(np, "pixart,irq-gpio",
						0, NULL);

	return 0;
}

static int pat9125_i2c_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int err = 0;
	struct pixart_pat9125_data *data;
	struct input_dev *input;
	struct device *dev = &client->dev;

	err = i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE);
	if (err < 0) {
		dev_err(dev, "I2C not supported\n");
		return -ENXIO;
	}

	if (client->dev.of_node) {
		data = devm_kzalloc(dev, sizeof(struct pixart_pat9125_data),
				GFP_KERNEL);
		if (!data)
			return -ENOMEM;
		err = pat9125_parse_dt(dev, data);
		if (err) {
			dev_err(dev, "DT parsing failed, errno:%d\n", err);
			return err;
		}
	} else {
		data = client->dev.platform_data;
		if (!data) {
			dev_err(dev, "Invalid pat9125 data\n");
			return -EINVAL;
		}
	}
	data->client = client;

	input = devm_input_allocate_device(dev);
	if (!input) {
		dev_err(dev, "Failed to alloc input device\n");
		return -ENOMEM;
	}

	input_set_capability(input, EV_REL, REL_WHEEL);
	if (data->press_en)
		input_set_capability(input, EV_KEY, data->press_keycode);

	i2c_set_clientdata(client, data);
	input_set_drvdata(input, data);
	input->name = PAT9125_DEV_NAME;

	data->input = input;
	err = input_register_device(data->input);
	if (err < 0) {
		dev_err(dev, "Failed to register input device\n");
		return err;
	}

	err = pixart_pinctrl_init(data);
	if (!err && data->pinctrl) {
		/*
		 * Pinctrl handle is optional. If pinctrl handle is found
		 * let pins to be configured in active state. If not
		 * found continue further without error.
		 */
		err = pinctrl_select_state(data->pinctrl,
				data->pinctrl_state_active);
		if (err < 0)
			dev_err(dev, "Could not set pin to active state %d\n",
									err);
	} else {
		if (gpio_is_valid(data->irq_gpio)) {
			err = devm_gpio_request(dev, data->irq_gpio,
						"pixart_pat9125_irq_gpio");
			if (err) {
				dev_err(dev, "Couldn't request gpio %d\n", err);
				return err;
			}
			err = gpio_direction_input(data->irq_gpio);
			if (err) {
				dev_err(dev, "Couldn't set dir for gpio %d\n",
									err);
				return err;
			}
		} else {
			dev_err(dev, "Invalid gpio %d\n", data->irq_gpio);
			return -EINVAL;
		}
	}

	err = pat9125_regulator_init(data);
	if (err) {
		dev_err(dev, "Failed to init regulator, %d\n", err);
		return err;
	}

	err = pat9125_power_on(data, true);
	if (err) {
		dev_err(dev, "Failed to power-on the sensor %d\n", err);
		goto err_power_on;
	}

	err = devm_request_threaded_irq(dev, client->irq, NULL, pat9125_irq,
			 IRQF_ONESHOT | IRQF_TRIGGER_FALLING | IRQF_TRIGGER_LOW,
			"pixart_pat9125_irq", data);
	if (err) {
		dev_err(dev, "Req irq %d failed, errno:%d\n", client->irq, err);
		goto err_request_threaded_irq;
	}

	err = sysfs_create_group(&(input->dev.kobj), &pat9125_attr_grp);
	if (err) {
		dev_err(dev, "Failed to create sysfs group, errno:%d\n", err);
		goto err_sysfs_create;
	}

	return 0;

err_sysfs_create:
err_request_threaded_irq:
err_power_on:
	regulator_set_optimum_mode(data->vdd, 0);
	regulator_set_optimum_mode(data->vld, 0);
	if (pat9125_power_on(data, false) < 0)
		dev_err(dev, "Failed to disable regulators\n");
	if (data->pinctrl)
		if (pinctrl_select_state(data->pinctrl,
				data->pinctrl_state_release) < 0)
			dev_err(dev, "Couldn't set pin to release state\n");

	return err;
}

static int pat9125_i2c_remove(struct i2c_client *client)
{
	struct pixart_pat9125_data *data = i2c_get_clientdata(client);
	struct device *dev = &data->client->dev;

	sysfs_remove_group(&(data->input->dev.kobj), &pat9125_attr_grp);
	if (data->pinctrl)
		if (pinctrl_select_state(data->pinctrl,
				data->pinctrl_state_release) < 0)
			dev_err(dev, "Couldn't set pin to release state\n");
	regulator_set_optimum_mode(data->vdd, 0);
	regulator_set_optimum_mode(data->vld, 0);
	pat9125_power_on(data, false);
	return 0;
}

static int pat9125_suspend(struct device *dev)
{
	int rc;
	struct pixart_pat9125_data *data =
		(struct pixart_pat9125_data *) dev_get_drvdata(dev);

	disable_irq(data->client->irq);
	if (data->pinctrl) {
		rc = pinctrl_select_state(data->pinctrl,
				data->pinctrl_state_suspend);
		if (rc < 0)
			dev_err(dev, "Could not set pin to suspend state %d\n",
									rc);
	}

	rc = pat9125_power_on(data, false);
	if (rc) {
		dev_err(dev, "Failed to disable regulators %d\n", rc);
		return rc;
	}

	return 0;
}

static int pat9125_resume(struct device *dev)
{
	int rc;
	struct pixart_pat9125_data *data =
		(struct pixart_pat9125_data *) dev_get_drvdata(dev);

	if (data->pinctrl) {
		rc = pinctrl_select_state(data->pinctrl,
				data->pinctrl_state_active);
		if (rc < 0)
			dev_err(dev, "Could not set pin to active state %d\n",
									rc);
	}

	rc = pat9125_power_on(data, true);
	if (rc) {
		dev_err(dev, "Failed to power-on the sensor %d\n", rc);
		goto err_sensor_init;
	}

	enable_irq(data->client->irq);

	return 0;

err_sensor_init:
	if (data->pinctrl)
		if (pinctrl_select_state(data->pinctrl,
				data->pinctrl_state_suspend) < 0)
			dev_err(dev, "Couldn't set pin to suspend state\n");
	if (pat9125_power_on(data, false) < 0)
		dev_err(dev, "Failed to disable regulators\n");

	return rc;
}

static const struct i2c_device_id pat9125_device_id[] = {
	{PAT9125_DEV_NAME, 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, pat9125_device_id);

static const struct dev_pm_ops pat9125_pm_ops = {
	.suspend = pat9125_suspend,
	.resume = pat9125_resume
};

static const struct of_device_id pixart_pat9125_match_table[] = {
	{ .compatible = "pixart,pat9125",},
	{ },
};

static struct i2c_driver pat9125_i2c_driver = {
	.driver = {
		   .name = PAT9125_DEV_NAME,
		   .owner = THIS_MODULE,
		   .pm = &pat9125_pm_ops,
		   .of_match_table = pixart_pat9125_match_table,
		   },
	.probe = pat9125_i2c_probe,
	.remove = pat9125_i2c_remove,
	.id_table = pat9125_device_id,
};
module_i2c_driver(pat9125_i2c_driver);

MODULE_AUTHOR("pixart");
MODULE_DESCRIPTION("pixart pat9125 driver");
MODULE_LICENSE("GPL");
