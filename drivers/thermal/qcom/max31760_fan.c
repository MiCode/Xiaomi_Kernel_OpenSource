// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/power_supply.h>
#include <linux/regulator/consumer.h>
#include <linux/thermal.h>

#define MAX31760_CTRL_REG1		0x00
#define MAX31760_CTRL_REG2		0x01
#define MAX31760_CTRL_REG3		0x02
#define MAX31760_DUTY_CYCLE_CTRL_REG	0x50

#define VDD_MAX_UV	3300000
#define VDD_MIN_UV	3300000
#define VDD_LOAD_UA	300000
#define VCCA_MAX_UV	1800000
#define VCCA_MIN_UV	1800000
#define VCCA_LOAD_UA	600000

enum {
	FAN_SPEED_LEVEL0 = 0,
	FAN_SPEED_LEVEL1,
	FAN_SPEED_LEVEL2,
	FAN_SPEED_LEVEL3,
	FAN_SPEED_LEVEL4,
	FAN_SPEED_MAX,
};

struct max31760_data {
	struct device *dev;
	struct i2c_client *i2c_client;
	struct thermal_cooling_device *cdev;
	struct mutex update_lock;
	struct regulator *vdd_reg;
	struct regulator *vcca_reg;
	u32 pwr_en_gpio;
	unsigned int cur_state;
	atomic_t in_suspend;
	bool debug;
	struct delayed_work work;
};

static int max31760_speed_map[FAN_SPEED_MAX] = {0x00, 0x30, 0x85, 0xCF, 0xFF};

static int max31760_write_byte(struct max31760_data *pdata, u8 reg, u8 val)
{
	int ret = 0;
	struct i2c_client *client = pdata->i2c_client;

	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0) {
		dev_err(pdata->dev, "failed write reg %#x failure, ret:%d\n", reg, ret);
		return ret;
	}

	dev_dbg(pdata->dev, "successfully write reg %#x=%#x\n", reg, val);
	return 0;
}

static void max31760_enable_gpio(struct max31760_data *pdata, int on)
{
	gpio_direction_output(pdata->pwr_en_gpio, on);
	dev_dbg(pdata->dev, "max31760 gpio:%d set to %d\n", pdata->pwr_en_gpio, on);
	usleep_range(20000, 20100);
}

static void max31760_speed_control(struct max31760_data *pdata, unsigned long level)
{
	max31760_write_byte(pdata, MAX31760_DUTY_CYCLE_CTRL_REG, max31760_speed_map[level]);
}

static void max31760_set_cur_state_common(struct max31760_data *pdata,
				unsigned long state)
{
	if (state > FAN_SPEED_LEVEL4)
		state = FAN_SPEED_LEVEL4;
	if (state < FAN_SPEED_LEVEL0)
		state = FAN_SPEED_LEVEL0;

	if (!atomic_read(&pdata->in_suspend))
		max31760_speed_control(pdata, state);
	pdata->cur_state = state;
}

static int max31760_get_max_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	*state = FAN_SPEED_LEVEL4;
	return 0;
}

static int max31760_get_cur_state(struct thermal_cooling_device *cdev,
				unsigned long *state)
{
	struct max31760_data *data = cdev->devdata;

	mutex_lock(&data->update_lock);
	*state = data->cur_state;
	mutex_unlock(&data->update_lock);

	return 0;
}

static int max31760_set_cur_state(struct thermal_cooling_device *cdev,
				unsigned long state)
{
	struct max31760_data *data = cdev->devdata;

	mutex_lock(&data->update_lock);
	max31760_set_cur_state_common(data, state);
	mutex_unlock(&data->update_lock);

	return 0;
}

static struct thermal_cooling_device_ops max31760_cooling_ops = {
	.get_max_state = max31760_get_max_state,
	.get_cur_state = max31760_get_cur_state,
	.set_cur_state = max31760_set_cur_state,
};

static void max31760_get_charging_status(struct max31760_data *pdata)
{
	static struct power_supply *batt_psy;
	union power_supply_propval ret = {0,};
	int err = 0;

	if (!batt_psy)
		batt_psy = power_supply_get_by_name("battery");
	if (batt_psy) {
		err = power_supply_get_property(batt_psy,
				POWER_SUPPLY_PROP_STATUS, &ret);
		if (err) {
			dev_err(pdata->dev, "failed to get charging status, error:%d\n", err);
			return;
		}
	}

	mutex_lock(&pdata->update_lock);
	if (pdata->debug) {
		dev_err(pdata->dev, "debug mode is on and speed is controlled manually.\n");
		mutex_unlock(&pdata->update_lock);
		return;
	}
	if (ret.intval == POWER_SUPPLY_STATUS_CHARGING)
		max31760_set_cur_state_common(pdata, FAN_SPEED_LEVEL4);
	else if (ret.intval == POWER_SUPPLY_STATUS_DISCHARGING)
		max31760_set_cur_state_common(pdata, FAN_SPEED_LEVEL0);
	mutex_unlock(&pdata->update_lock);
}

static void max31760_charging_check(struct work_struct *work)
{
	struct max31760_data *pdata = container_of(work,
			struct max31760_data, work.work);

	max31760_get_charging_status(pdata);
	schedule_delayed_work(&pdata->work, msecs_to_jiffies(2000));
}

static ssize_t speed_control_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct max31760_data *data = dev_get_drvdata(dev);
	int ret;

	if (!data) {
		pr_err("invalid driver pointer\n");
		return -ENODEV;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", data->cur_state);

	return ret;
}

static ssize_t speed_control_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct max31760_data *data = dev_get_drvdata(dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	mutex_lock(&data->update_lock);
	if (!data->debug) {
		dev_err(data->dev, "debug mode is off and speed isn't controlled manually.\n");
		mutex_unlock(&data->update_lock);
		return count;
	}

	if (value == 0x0)
		max31760_set_cur_state_common(data, FAN_SPEED_LEVEL0);
	else if (value == 0x1)
		max31760_set_cur_state_common(data, FAN_SPEED_LEVEL1);
	else if (value == 0x2)
		max31760_set_cur_state_common(data, FAN_SPEED_LEVEL2);
	else if (value == 0x3)
		max31760_set_cur_state_common(data, FAN_SPEED_LEVEL3);
	else if (value == 0x4)
		max31760_set_cur_state_common(data, FAN_SPEED_LEVEL4);
	mutex_unlock(&data->update_lock);

	return count;
}

static DEVICE_ATTR_RW(speed_control);

static ssize_t debug_show(struct device *dev, struct device_attribute *attr,
				char *buf)
{
	struct max31760_data *data = dev_get_drvdata(dev);
	int ret;

	if (!data) {
		pr_err("invalid driver pointer\n");
		return -ENODEV;
	}

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", data->debug);
	return ret;
}

static ssize_t debug_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct max31760_data *data = dev_get_drvdata(dev);
	unsigned long value;

	if (kstrtoul(buf, 0, &value))
		return -EINVAL;

	mutex_lock(&data->update_lock);
	if (value == 0x0)
		data->debug = false;
	else if (value == 0x1)
		data->debug = true;
	mutex_unlock(&data->update_lock);

	return count;
}

static DEVICE_ATTR_RW(debug);

static struct attribute *max31760_sysfs_attrs[] = {
	&dev_attr_speed_control.attr,
	&dev_attr_debug.attr,
	NULL
};

static int max31760_register_cdev(struct max31760_data *pdata)
{
	int ret = 0;
	char cdev_name[THERMAL_NAME_LENGTH] = "";

	snprintf(cdev_name, THERMAL_NAME_LENGTH, "fan-max31760");

	pdata->cdev = thermal_of_cooling_device_register(pdata->dev->of_node, cdev_name,
						pdata, &max31760_cooling_ops);
	if (IS_ERR(pdata->cdev)) {
		ret = PTR_ERR(pdata->cdev);
		dev_err(pdata->dev, "register failed for %s, ret:%d\n", cdev_name, ret);
		pdata->cdev = NULL;
		return ret;
	}

	dev_dbg(pdata->dev, "register success for %s\n", cdev_name);
	return 0;
}

static void max31760_hw_init(struct max31760_data *pdata)
{
	max31760_write_byte(pdata, MAX31760_CTRL_REG1, 0x19);
	max31760_write_byte(pdata, MAX31760_CTRL_REG2, 0x11);
	max31760_write_byte(pdata, MAX31760_CTRL_REG3, 0x31);
	mutex_lock(&pdata->update_lock);
	max31760_speed_control(pdata, FAN_SPEED_LEVEL0);
	pdata->cur_state = FAN_SPEED_LEVEL0;
	mutex_unlock(&pdata->update_lock);

	atomic_set(&pdata->in_suspend, 0);
}

static int max31760_parse_dt(struct max31760_data *pdata)
{
	int ret = 0;
	struct device_node *node = pdata->dev->of_node;

	if (!node) {
		pr_err("device tree info missing\n");
		return -EINVAL;
	}

	pdata->pwr_en_gpio = of_get_named_gpio(node, "maxim,pwr-en-gpio", 0);
	if (!gpio_is_valid(pdata->pwr_en_gpio)) {
		dev_err(pdata->dev, "enable gpio not specified\n");
		return -EINVAL;
	}

	ret = gpio_request(pdata->pwr_en_gpio, "pwr_en_gpio");
	if (ret) {
		pr_err("enable gpio request failed, ret:%d\n", ret);
		goto error;
	}

	max31760_enable_gpio(pdata, 1);

	return ret;

error:
	gpio_free(pdata->pwr_en_gpio);
	return ret;
}

static struct attribute_group max31760_attribute_group = {
	.attrs = max31760_sysfs_attrs
};

static int max31760_enable_vregs(struct max31760_data *pdata)
{
	int ret = 0;

	pdata->vdd_reg = devm_regulator_get(pdata->dev, "maxim,vdd");
	if (IS_ERR(pdata->vdd_reg)) {
		ret = PTR_ERR(pdata->vdd_reg);
		dev_err(pdata->dev, "couldn't get vdd_reg regulator, ret:%d\n", ret);
		pdata->vdd_reg = NULL;
		return ret;
	}

	regulator_set_voltage(pdata->vdd_reg, VDD_MIN_UV, VDD_MAX_UV);
	regulator_set_load(pdata->vdd_reg, VDD_LOAD_UA);
	ret = regulator_enable(pdata->vdd_reg);
	if (ret < 0) {
		dev_err(pdata->dev, "vdd_reg regulator failed, ret:%d\n", ret);
		regulator_set_voltage(pdata->vdd_reg, 0, VDD_MAX_UV);
		regulator_set_load(pdata->vdd_reg, 0);
		return -EINVAL;
	}

	pdata->vcca_reg = devm_regulator_get(pdata->dev, "maxim,vcca");
	if (IS_ERR(pdata->vcca_reg)) {
		ret = PTR_ERR(pdata->vcca_reg);
		dev_err(pdata->dev, "couldn't get vcca_reg regulator, ret:%d\n", ret);
		pdata->vcca_reg = NULL;
		return ret;
	}

	regulator_set_voltage(pdata->vcca_reg, VCCA_MIN_UV, VCCA_MAX_UV);
	regulator_set_load(pdata->vcca_reg, VCCA_LOAD_UA);
	ret = regulator_enable(pdata->vcca_reg);
	if (ret < 0) {
		dev_err(pdata->dev, "vcca_reg regulator failed, ret:%d\n", ret);
		regulator_set_voltage(pdata->vcca_reg, 0, VCCA_MAX_UV);
		regulator_set_load(pdata->vcca_reg, 0);
		return -EINVAL;
	}

	return 0;
}

static int max31760_remove(struct i2c_client *client)
{
	struct max31760_data *pdata = i2c_get_clientdata(client);

	if (!pdata)
		return 0;

	thermal_cooling_device_unregister(pdata->cdev);
	regulator_disable(pdata->vdd_reg);
	regulator_disable(pdata->vcca_reg);
	max31760_enable_gpio(pdata, 0);
	gpio_free(pdata->pwr_en_gpio);

	return 0;
}

static int max31760_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct max31760_data *pdata;

	if (!client || !client->dev.of_node) {
		pr_err("max31760 invalid input\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "device doesn't support I2C\n");
		return -ENODEV;
	}

	pdata = devm_kzalloc(&client->dev, sizeof(struct max31760_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	pdata->dev = &client->dev;
	pdata->i2c_client = client;
	i2c_set_clientdata(client, pdata);
	dev_set_drvdata(&client->dev, pdata);
	mutex_init(&pdata->update_lock);

	ret = max31760_parse_dt(pdata);
	if (ret) {
		dev_err(pdata->dev, "failed to parse device tree, ret:%d\n", ret);
		goto fail_parse_dt;
	}

	ret = max31760_enable_vregs(pdata);
	if (ret) {
		dev_err(pdata->dev, "failed to enable regulators, ret:%d\n", ret);
		goto fail_enable_vregs;
	}

	max31760_hw_init(pdata);
	ret = max31760_register_cdev(pdata);
	if (ret) {
		dev_err(pdata->dev, "failed to register cooling device, ret:%d\n", ret);
		goto fail_register_cdev;
	}

	ret = devm_device_add_group(&client->dev, &max31760_attribute_group);
	if (ret < 0) {
		dev_err(pdata->dev, "couldn't register sysfs group\n");
		return ret;
	}

	pdata->debug = false;

	INIT_DELAYED_WORK(&pdata->work, max31760_charging_check);
	schedule_delayed_work(&pdata->work, msecs_to_jiffies(45000));

	return ret;

fail_register_cdev:
	max31760_remove(client);
	return ret;
fail_enable_vregs:
	max31760_enable_gpio(pdata, 0);
	gpio_free(pdata->pwr_en_gpio);
fail_parse_dt:
	i2c_set_clientdata(client, NULL);
	dev_set_drvdata(&client->dev, NULL);
	return ret;
}

static void max31760_shutdown(struct i2c_client *client)
{
	max31760_remove(client);
}

static int max31760_suspend(struct device *dev)
{
	struct max31760_data *pdata = dev_get_drvdata(dev);

	dev_dbg(dev, "enter suspend now\n");
	if (pdata) {
		atomic_set(&pdata->in_suspend, 1);
		mutex_lock(&pdata->update_lock);
		max31760_speed_control(pdata, FAN_SPEED_LEVEL0);
		max31760_enable_gpio(pdata, 0);
		regulator_disable(pdata->vdd_reg);
		mutex_unlock(&pdata->update_lock);
	}

	return 0;
}

static int max31760_resume(struct device *dev)
{
	struct max31760_data *pdata = dev_get_drvdata(dev);
	int ret;

	dev_dbg(dev, "enter resume now\n");
	if (pdata) {
		atomic_set(&pdata->in_suspend, 0);
		mutex_lock(&pdata->update_lock);
		max31760_enable_gpio(pdata, 1);

		ret = regulator_enable(pdata->vdd_reg);
		if (ret < 0)
			dev_err(pdata->dev, "vdd_reg regulator failed, ret:%d\n", ret);

		max31760_write_byte(pdata, MAX31760_CTRL_REG1, 0x19);
		max31760_write_byte(pdata, MAX31760_CTRL_REG2, 0x11);
		max31760_write_byte(pdata, MAX31760_CTRL_REG3, 0x31);
		max31760_set_cur_state_common(pdata, pdata->cur_state);
		mutex_unlock(&pdata->update_lock);
	}

	return 0;
}

static const struct of_device_id max31760_id_table[] = {
	{ .compatible = "maxim,max31760",},
	{ },
};

static const struct i2c_device_id max31760_i2c_table[] = {
	{ "max31760", 0 },
	{ },
};

static SIMPLE_DEV_PM_OPS(max31760_pm_ops, max31760_suspend, max31760_resume);

static struct i2c_driver max31760_i2c_driver = {
	.probe = max31760_probe,
	.remove = max31760_remove,
	.shutdown = max31760_shutdown,
	.driver = {
		.name = "max31760",
		.of_match_table = max31760_id_table,
		.pm = &max31760_pm_ops,
	},
	.id_table = max31760_i2c_table,
};

module_i2c_driver(max31760_i2c_driver);
MODULE_DEVICE_TABLE(i2c, max31760_i2c_table);
MODULE_DESCRIPTION("Maxim 31760 Fan Controller");
MODULE_LICENSE("GPL v2");
