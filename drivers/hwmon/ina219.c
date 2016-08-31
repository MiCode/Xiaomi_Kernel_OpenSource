/*
 * ina219.c - driver for TI INA219 current / power monitor sensor
 *
 * Copyright (c) 2011-2013, NVIDIA Corporation.
 *
 * The INA219 is a sensor chip made by Texas Instruments. It measures
 * power, voltage and current on a power rail.
 * Complete datasheet can be obtained from website:
 *   http://focus.ti.com/lit/ds/symlink/ina219.pdf
 *
 * This program is free software. you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include "linux/ina219.h"
#include <linux/init.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>

#define DRIVER_NAME "ina219"

/* INA219 register offsets */
#define INA219_CONFIG	0
#define INA219_SHUNT	1
#define INA219_VOLTAGE	2
#define INA219_POWER	3
#define INA219_CURRENT	4
#define INA219_CAL	5

#define INA219_RESET 0x8000

#define busv_register_to_mv(x) (((x) >> 3) * 4)
#define shuntv_register_to_uv(x) ((x) * 10)
struct power_mon_data {
	s32 voltage;
	s32 currentInMillis;
	s32 power;
};

struct ina219_data {
	struct device *hwmon_dev;
	struct i2c_client *client;
	struct ina219_platform_data *pInfo;
	struct power_mon_data pm_data;
	struct mutex mutex;
	int state;
};

#define STOPPED 0
#define RUNNING 1

/* Set non-zero to enable debug prints */
#define INA219_DEBUG_PRINTS 0

#if INA219_DEBUG_PRINTS
#define DEBUG_INA219(x) printk x
#else
#define DEBUG_INA219(x)
#endif

static s32 power_down_INA219(struct i2c_client *client)
{
	s32 retval;
	retval = i2c_smbus_write_word_data(client, INA219_CONFIG, 0);
	if (retval < 0)
		dev_err(&client->dev, "power down failure sts: 0x%x\n", retval);
	return retval;
}

static s32 power_up_INA219(struct i2c_client *client, u16 config_data)
{
	s32 retval;
	struct ina219_data *data = i2c_get_clientdata(client);
	retval = i2c_smbus_write_word_data(client, INA219_CONFIG,
				__constant_cpu_to_be16(config_data));
	if (retval < 0)
		goto error;

	retval = i2c_smbus_write_word_data(client, INA219_CAL,
			__constant_cpu_to_be16(data->pInfo->calibration_data));
	if (retval < 0)
		goto error;

	return 0;
error:
	dev_err(&client->dev, "power up failure sts: 0x%x\n", retval);
	return retval;

}

static s32 show_rail_name(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina219_data *data = i2c_get_clientdata(client);
	return sprintf(buf, "%s\n", data->pInfo->rail_name);
}

static s32 show_voltage(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina219_data *data = i2c_get_clientdata(client);
	s32 voltage_mV;
	int cur_state;

	mutex_lock(&data->mutex);
	cur_state = data->state;

	if (data->state == STOPPED)
		if (power_up_INA219(client, data->pInfo->trig_conf) < 0)
			goto error;
	/* getting voltage readings in milli volts*/
	voltage_mV =
		(s16)be16_to_cpu(i2c_smbus_read_word_data(client,
			INA219_VOLTAGE));

	DEBUG_INA219(("Ina219 voltage reg Value: 0x%x\n", voltage_mV));
	if (voltage_mV < 0)
		goto error;
	voltage_mV = busv_register_to_mv(voltage_mV);
	DEBUG_INA219(("Ina219 voltage in mv: %d\n", voltage_mV));

	DEBUG_INA219(("%s volt = %d\n", __func__, voltage_mV));

	if (cur_state == STOPPED)
		if (power_down_INA219(client) < 0)
			goto error;

	mutex_unlock(&data->mutex);
	return sprintf(buf, "%d mV\n", voltage_mV);
error:
	mutex_unlock(&data->mutex);
	dev_err(dev, "%s: failed\n", __func__);
	return -EAGAIN;
}

static s32 show_shunt_voltage(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina219_data *data = i2c_get_clientdata(client);
	s32 voltage_uV;
	int cur_state;
	mutex_lock(&data->mutex);
	cur_state = data->state;
	if (data->state == STOPPED)
		if (power_up_INA219(client, data->pInfo->trig_conf) < 0)
			goto error;
	/* getting voltage readings in milli volts*/
	voltage_uV =
		be16_to_cpu(i2c_smbus_read_word_data(client,
			INA219_SHUNT));

	DEBUG_INA219(("Ina219 voltage reg Value: 0x%x\n", voltage_uV));
	if (voltage_uV < 0)
		goto error;
	voltage_uV = shuntv_register_to_uv(voltage_uV);

	if (cur_state == STOPPED)
		if (power_down_INA219(client) < 0)
			goto error;

	mutex_unlock(&data->mutex);

	DEBUG_INA219(("%s volt = %d\n", __func__, voltage_uV));
	return sprintf(buf, "%d uV\n", voltage_uV);
error:
	mutex_unlock(&data->mutex);
	dev_err(dev, "%s: failed\n", __func__);
	return -EAGAIN;
}

static s32 show_power2(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina219_data *data = i2c_get_clientdata(client);
	s32 power_mW;
	s32 voltage_shunt_uV;
	s32 voltage_bus_mV;
	s32 inverse_shunt_resistor;
	int cur_state;
#if INA219_DEBUG_PRINTS
	s32 power_raw;
#endif

	mutex_lock(&data->mutex);
	cur_state = data->state;
	if (data->state == STOPPED)
		if (power_up_INA219(client, data->pInfo->trig_conf) < 0)
			goto error;

	voltage_shunt_uV = be16_to_cpu(i2c_smbus_read_word_data(client,
			INA219_SHUNT));
	if (voltage_shunt_uV < 0)
		goto error;
	voltage_shunt_uV = shuntv_register_to_uv(voltage_shunt_uV);

	voltage_bus_mV = be16_to_cpu(i2c_smbus_read_word_data(client,
			INA219_VOLTAGE));
	if (voltage_bus_mV < 0)
		goto error;

	voltage_bus_mV = busv_register_to_mv(voltage_bus_mV);

	/*avoid overflow*/
	inverse_shunt_resistor = 1000/(data->pInfo->shunt_resistor);
	power_mW = voltage_shunt_uV * inverse_shunt_resistor; /*current uAmps*/
	power_mW = power_mW / 1000; /*current mAmps*/
	power_mW = power_mW * (voltage_bus_mV); /*Power uW*/
	power_mW = power_mW / 1000; /*Power mW*/

#if INA219_DEBUG_PRINTS
	power_raw = be16_to_cpu(i2c_smbus_read_word_data(client, INA219_POWER));
	power_raw *= data->pInfo->power_lsb;
	power_raw /= data->pInfo->precision_multiplier;
	DEBUG_INA219(("INA219: power_mW: %d, power_raw:%d\n", power_mW,
								power_raw));
#endif
	if (cur_state == STOPPED)
		if (power_down_INA219(client) < 0)
			goto error;


	mutex_unlock(&data->mutex);
	DEBUG_INA219(("%s pow = %d\n", __func__, power_mW));
	return sprintf(buf, "%d mW\n", power_mW);
error:
	mutex_unlock(&data->mutex);
	dev_err(dev, "%s: failed\n", __func__);
	return -EAGAIN;
}

/*This function is kept to support INA219 on cardhu*/
static s32 show_power(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina219_data *data = i2c_get_clientdata(client);
	s32 retval;
	s32 power_mW;
	s32 voltage_mV;
	s32 overflow, conversion;
	int cur_state;

	mutex_lock(&data->mutex);
	cur_state = data->state;
	if (data->state == STOPPED) {
		if (power_up_INA219(client, data->pInfo->trig_conf) < 0) {
			retval = -EAGAIN;
			goto error;
		}
	} else {
		mutex_unlock(&data->mutex);
		return show_power2(dev, attr, buf);
	}

	/* check if the readings are valid */
	do {
		/* read power register to clear conversion bit */
		retval = be16_to_cpu(i2c_smbus_read_word_data(client,
			INA219_POWER));
		if (retval < 0) {
			dev_err(dev, "CNVR bit clearing failure sts: 0x%x\n",
				retval);
			goto error;
		}

		voltage_mV =
			be16_to_cpu(i2c_smbus_read_word_data(client,
				INA219_VOLTAGE));
		DEBUG_INA219(("Ina219 voltage reg Value: 0x%x\n", voltage_mV));
		overflow = voltage_mV & 1;
		if (overflow) {
			dev_err(dev, "overflow error\n");
			goto error;
		}
		conversion = (voltage_mV >> 1) & 1;
		DEBUG_INA219(("\n ina219 CNVR value:%d", conversion));
	} while (!conversion);

	/* getting power readings in milli watts*/
	power_mW = be16_to_cpu(i2c_smbus_read_word_data(client,
		INA219_POWER));
	DEBUG_INA219(("Ina219 power Reg: 0x%x\n", power_mW));
	power_mW *= data->pInfo->power_lsb;
	if (data->pInfo->precision_multiplier)
		power_mW /= data->pInfo->precision_multiplier;
	DEBUG_INA219(("Ina219 power Val: %d\n", power_mW));
	if (power_mW < 0)
		goto error;

	/* set ina219 to power down mode */
	retval = power_down_INA219(client);
	if (retval < 0)
		goto error;
	mutex_unlock(&data->mutex);
	DEBUG_INA219(("%s pow = %d\n", __func__, power_mW));
	return sprintf(buf, "%d mW\n", power_mW);
error:
	mutex_unlock(&data->mutex);
	dev_err(dev, "%s: failed\n", __func__);
	return retval;
}
static s32 show_current2(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina219_data *data = i2c_get_clientdata(client);
	s32 current_mA;
	s32 voltage_uV;
	s32 inverse_shunt_resistor;
	int cur_state;
#if INA219_DEBUG_PRINTS
	s32 current_raw;
#endif
	mutex_lock(&data->mutex);
	cur_state = data->state;
	if (data->state == STOPPED)
		if (power_up_INA219(client, data->pInfo->trig_conf) < 0)
			goto error;

	voltage_uV =
		(s16)be16_to_cpu(i2c_smbus_read_word_data(client,
							INA219_SHUNT));
	if (voltage_uV < 0)
		goto error;
	inverse_shunt_resistor = 1000/(data->pInfo->shunt_resistor);
	voltage_uV = shuntv_register_to_uv(voltage_uV);
	current_mA = voltage_uV * inverse_shunt_resistor;
	current_mA = current_mA / 1000;

#if INA219_DEBUG_PRINTS
	current_raw =
		(s16)be16_to_cpu(i2c_smbus_read_word_data(client,
							INA219_CURRENT));
	current_raw *= data->pInfo->power_lsb;
	current_raw /= data->pInfo->divisor;
	current_raw /= data->pInfo->precision_multiplier;

	DEBUG_INA219(("%s current = %d current_raw=%d\n", __func__, current_mA,
	current_raw));
#endif
	if (cur_state == STOPPED)
		if (power_down_INA219(client) < 0)
			goto error;
	mutex_unlock(&data->mutex);
	return sprintf(buf, "%d mA\n", current_mA);
error:
	dev_err(dev, "%s: failed\n", __func__);
	mutex_unlock(&data->mutex);
	return -EAGAIN;
}

/*This function is kept to support INA219 on cardhu*/
static s32 show_current(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina219_data *data = i2c_get_clientdata(client);
	s32 retval;
	s32 current_mA;
	s32 voltage_mV;
	s32 overflow, conversion;
	int cur_state;

	mutex_lock(&data->mutex);

	cur_state = data->state;
	if (data->state == STOPPED) {
		if (power_up_INA219(client, data->pInfo->trig_conf) < 0) {
			retval = -EAGAIN;
			goto error;
		}
	} else {
		mutex_unlock(&data->mutex);
		show_current2(dev, attr, buf);
	}
	/* check if the readings are valid */
	do {
		/* read power register to clear conversion bit */
		retval = be16_to_cpu(i2c_smbus_read_word_data(client,
			INA219_POWER));
		if (retval < 0) {
			dev_err(dev, "CNVR bit clearing failure sts: 0x%x\n",
				retval);
			goto error;
		}

		voltage_mV =
			be16_to_cpu(i2c_smbus_read_word_data(client,
				INA219_VOLTAGE));
		DEBUG_INA219(("Ina219 voltage reg Value: 0x%x\n", voltage_mV));
		overflow = voltage_mV & 1;
		if (overflow) {
			dev_err(dev, "overflow error\n");
			goto error;
		}
		conversion = (voltage_mV >> 1) & 1;
		DEBUG_INA219(("\n ina219 CNVR value:%d", conversion));
	} while (!conversion);

	/* getting current readings in milli amps*/
	current_mA = be16_to_cpu(i2c_smbus_read_word_data(client,
		INA219_CURRENT));
	DEBUG_INA219(("Ina219 current Reg: 0x%x\n", current_mA));
	if (current_mA < 0)
		goto error;
	current_mA =
		(current_mA * data->pInfo->power_lsb) / data->pInfo->divisor;
	if (data->pInfo->precision_multiplier)
		current_mA /= data->pInfo->precision_multiplier;
	DEBUG_INA219(("Ina219 current Value: %d\n", current_mA));

	if (cur_state == STOPPED)
		if (power_down_INA219(client) < 0)
			goto error;

	mutex_unlock(&data->mutex);

	DEBUG_INA219(("%s current = %d\n", __func__, current_mA));
	return sprintf(buf, "%d mA\n", current_mA);
error:
	mutex_unlock(&data->mutex);
	dev_err(dev, "%s: failed\n", __func__);
	return retval;
}

static int ina219_state_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina219_data *data = i2c_get_clientdata(client);
	int count;
	mutex_lock(&data->mutex);
	count = sprintf(buf, "%d\n", data->state);
	mutex_unlock(&data->mutex);
	return count;
}

static int ina219_state_set(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t size)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina219_data *data = i2c_get_clientdata(client);
	int retval = -1;
	long new;
	retval = kstrtol(buf, 10, &new);
	if (retval < 0 || new > INT_MAX || new < INT_MIN)
		return -EINVAL;
	mutex_lock(&data->mutex);

	if ((new > 0) && (data->state == STOPPED))
		retval = power_up_INA219(client, data->pInfo->cont_conf);
	else if ((new == 0) && (data->state == RUNNING))
		retval = power_down_INA219(client);

	if (retval < 0) {
		dev_err(dev, "Error in switching INA on/off!");
		mutex_unlock(&data->mutex);
		return -EAGAIN;
	}

	if (new)
		data->state = RUNNING;
	else
		data->state = STOPPED;

	mutex_unlock(&data->mutex);
	return 1;
}

static struct sensor_device_attribute ina219[] = {
	SENSOR_ATTR(shunt_voltage, S_IRUGO, show_shunt_voltage, NULL, 0),
	SENSOR_ATTR(rail_name, S_IRUGO, show_rail_name, NULL, 0),
	SENSOR_ATTR(in1_input, S_IRUGO, show_voltage, NULL, 0),
	SENSOR_ATTR(curr1_input, S_IRUGO, show_current, NULL, 0),
	SENSOR_ATTR(curr2_input, S_IRUGO, show_current2, NULL, 0),
	SENSOR_ATTR(power1_input, S_IRUGO, show_power, NULL, 0),
	SENSOR_ATTR(power2_input, S_IRUGO, show_power2, NULL, 0),
	SENSOR_ATTR(cur_state, 0644, ina219_state_show, ina219_state_set, 0),
};

static int ina219_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct ina219_data *data;
	int err;
	u8 i;
	data = kzalloc(sizeof(struct ina219_data), GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, data);
	data->pInfo = client->dev.platform_data;
	mutex_init(&data->mutex);
	data->state = STOPPED;
	/* reset ina219 */
	err = i2c_smbus_write_word_data(client, INA219_CONFIG,
		__constant_cpu_to_be16(INA219_RESET));
	if (err < 0) {
		dev_err(&client->dev, "ina219 reset failure status: 0x%x\n",
			err);
		goto exit_free;
	}

	for (i = 0; i < ARRAY_SIZE(ina219); i++) {
		err = device_create_file(&client->dev, &ina219[i].dev_attr);
		if (err) {
			dev_err(&client->dev, "device_create_file failed.\n");
			goto exit_free;
		}
	}

	data->hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		goto exit_remove;
	}

	err = power_down_INA219(client);
	if (err < 0) {
		dev_err(&client->dev, "ina219 power-down failure status: 0x%x\n",
			err);
		goto exit_remove;
	}

	return 0;

exit_remove:
	for (i = 0; i < ARRAY_SIZE(ina219); i++)
		device_remove_file(&client->dev, &ina219[i].dev_attr);
exit_free:
	kfree(data);
exit:
	return err;
}

static int ina219_remove(struct i2c_client *client)
{
	u8 i;
	struct ina219_data *data = i2c_get_clientdata(client);
	mutex_lock(&data->mutex);
	power_down_INA219(client);
	data->state = STOPPED;
	mutex_unlock(&data->mutex);
	hwmon_device_unregister(data->hwmon_dev);
	for (i = 0; i < ARRAY_SIZE(ina219); i++)
		device_remove_file(&client->dev, &ina219[i].dev_attr);
	kfree(data);
	return 0;
}

static const struct i2c_device_id ina219_id[] = {
	{DRIVER_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ina219_id);

static struct i2c_driver ina219_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= DRIVER_NAME,
	},
	.probe		= ina219_probe,
	.remove		= ina219_remove,
	.id_table	= ina219_id,
};

static int __init ina219_init(void)
{
	return i2c_add_driver(&ina219_driver);
}

static void __exit ina219_exit(void)
{
	i2c_del_driver(&ina219_driver);
}

module_init(ina219_init);
module_exit(ina219_exit);
MODULE_LICENSE("GPL");
