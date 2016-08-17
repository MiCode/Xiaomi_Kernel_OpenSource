/*
 * ina3221.c - driver for TI INA3221
 *
 * Copyright (c) 2012-2013, NVIDIA Corporation. All Rights Reserved.
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
#include <linux/init.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/cpu.h>

#include "linux/ina3221.h"

#define DRIVER_NAME "ina3221"

/* Set non-zero to enable debug prints */
#define INA3221_DEBUG_PRINTS 0

#if INA3221_DEBUG_PRINTS
#define DEBUG_INA3221(x) (printk x)
#else
#define DEBUG_INA3221(x)
#endif

#define TRIGGERED 0
#define CONTINUOUS 1

#define busv_register_to_mv(x) ((x >> 3) * 8)
#define shuntv_register_to_uv(x) ((x >> 3) * 40)

#define CPU_THRESHOLD 2

struct ina3221_data {
	struct device *hwmon_dev;
	struct i2c_client *client;
	struct ina3221_platform_data *plat_data;
	struct mutex mutex;
	u8 mode;
	struct notifier_block nb;
	int shutdown_complete;
};

static s32 __locked_power_down_ina3221(struct i2c_client *client)
{
	s32 ret;
	struct ina3221_data *data = i2c_get_clientdata(client);
	if (data->shutdown_complete)
		return -ENODEV;
	ret = i2c_smbus_write_word_data(client, INA3221_CONFIG,
		INA3221_POWER_DOWN);
	if (ret < 0)
		dev_err(&client->dev, "Power dowm failure status: 0x%x", ret);
	return ret;
}

static s32 __locked_power_up_ina3221(struct i2c_client *client, int config)
{
	s32 ret;
	struct ina3221_data *data = i2c_get_clientdata(client);
	if (data->shutdown_complete)
		return -ENODEV;
	ret = i2c_smbus_write_word_data(client, INA3221_CONFIG,
					__constant_cpu_to_be16(config));
	if (ret < 0)
		dev_err(&client->dev,
			"Config data write failed, error: 0x%x", ret);
	return ret;
}

static s32 show_mode(struct device *dev, struct device_attribute *attr,
								char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina3221_data *data = i2c_get_clientdata(client);
	int ret;
	mutex_lock(&data->mutex);
	ret = sprintf(buf, "%d\n", data->mode);
	mutex_unlock(&data->mutex);
	return ret;
}

static s32 show_rail_name(struct device *dev,
		struct device_attribute *devattr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina3221_data *data = i2c_get_clientdata(client);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	u8 index = attr->index;
	return sprintf(buf, "%s\n", data->plat_data->rail_name[index]);
}

static s32 show_voltage(struct device *dev,
		struct device_attribute *devattr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina3221_data *data = i2c_get_clientdata(client);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	u8 index, bus_volt_reg_addr;
	s32 ret;
	s32 voltage_mV;

	mutex_lock(&data->mutex);
	if (data->shutdown_complete) {
		ret = -ENODEV;
		goto error;
	}
	index = attr->index;
	bus_volt_reg_addr = (INA3221_BUS_VOL_CHAN1 + (index * 2));

	if (data->mode == TRIGGERED) {
		ret = __locked_power_up_ina3221(client,
				data->plat_data->trig_conf_data);
		if (ret < 0) {
			dev_err(dev,
			"Power up failed, status: 0x%x\n", ret);
			goto error;
		}
	}

	/* getting voltage readings in milli volts*/
	voltage_mV =
		be16_to_cpu(i2c_smbus_read_word_data(client,
			bus_volt_reg_addr));
	DEBUG_INA3221(("Ina3221 voltage reg Value: 0x%x\n", voltage_mV));
	if (voltage_mV < 0)
		goto error;
	voltage_mV = busv_register_to_mv(voltage_mV);
	DEBUG_INA3221(("Ina3221 voltage in mv: %d\n", voltage_mV));

	if (data->mode == TRIGGERED) {
		/* set ina3221 to power down mode */
		ret = __locked_power_down_ina3221(client);
		if (ret < 0)
			goto error;
	}

	DEBUG_INA3221(("%s volt = %d\n", __func__, voltage_mV));
	mutex_unlock(&data->mutex);
	return sprintf(buf, "%d mV\n", voltage_mV);
error:
	mutex_unlock(&data->mutex);
	dev_err(dev, "%s: failed\n", __func__);
	return ret;
}

static s32 show_current(struct device *dev,
		struct device_attribute *devattr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina3221_data *data = i2c_get_clientdata(client);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	u8 index, shunt_volt_reg_addr;
	s32 ret;
	s32 voltage_uV;
	s32 current_mA;
	s32 inverse_shunt_resistor;

	mutex_lock(&data->mutex);
	if (data->shutdown_complete) {
		ret = -ENODEV;
		goto error;
	}
	index = attr->index;
	shunt_volt_reg_addr = (INA3221_SHUNT_VOL_CHAN1 + (index * 2));

	if (data->mode == TRIGGERED) {
		ret = __locked_power_up_ina3221(client,
				data->plat_data->trig_conf_data);
		if (ret < 0) {
			dev_err(dev,
				"power up failed sts: 0x%x\n", ret);
			goto error;
		}
	}

	/* getting voltage readings in micro volts*/
	voltage_uV =
		be16_to_cpu(i2c_smbus_read_word_data(client,
			shunt_volt_reg_addr));
	DEBUG_INA3221(("Ina3221 voltage reg Value: 0x%x\n", voltage_uV));
	if (voltage_uV < 0)
		goto error;
	voltage_uV = shuntv_register_to_uv(voltage_uV);
	DEBUG_INA3221(("Ina3221 voltage in uv: %d\n", voltage_uV));

	/* shunt_resistor is received in mOhms */
	inverse_shunt_resistor = 1000 / data->plat_data->shunt_resistor[index];
	current_mA = (voltage_uV * inverse_shunt_resistor) / 1000;

	if (data->mode == TRIGGERED) {
		/* set ina3221 to power down mode */
		ret = __locked_power_down_ina3221(client);
		if (ret < 0)
			goto error;
	}

	DEBUG_INA3221(("%s current = %d\n", __func__, current_mA));
	mutex_unlock(&data->mutex);
	return sprintf(buf, "%d mA\n", current_mA);
error:
	mutex_unlock(&data->mutex);
	dev_err(dev, "%s: failed\n", __func__);
	return ret;
}

static s32 __locked_calculate_power(struct i2c_client *client,
					u8 shunt_volt_reg_addr,
					u8 bus_volt_reg_addr,
					int index)
{

	struct ina3221_data *data = i2c_get_clientdata(client);
	s32 voltage_mV;
	s32 voltage_uV;
	s32 inverse_shunt_resistor;
	s32 current_mA;
	s32 power_mW;
	/* getting voltage readings in micro volts*/
	voltage_uV =
		be16_to_cpu(i2c_smbus_read_word_data(client,
			shunt_volt_reg_addr));
	DEBUG_INA3221(("Ina3221 voltage reg Value: 0x%x\n", voltage_uV));
	if (voltage_uV < 0)
		goto error;
	voltage_uV = shuntv_register_to_uv(voltage_uV);
	DEBUG_INA3221(("Ina3221 voltage in uv: %d\n", voltage_uV));

	/* getting voltage readings in milli volts*/
	voltage_mV =
		be16_to_cpu(i2c_smbus_read_word_data(client,
			bus_volt_reg_addr));
	DEBUG_INA3221(("Ina3221 voltage reg Value: 0x%x\n", voltage_mV));
	if (voltage_mV < 0)
		goto error;
	voltage_mV = busv_register_to_mv(voltage_mV);
	DEBUG_INA3221(("Ina3221 voltage in mv: %d\n", voltage_mV));

	/* shunt_resistor is received in mOhms */
	inverse_shunt_resistor = 1000 / data->plat_data->shunt_resistor[index];
	current_mA = voltage_uV * inverse_shunt_resistor / 1000;
	power_mW = voltage_mV * current_mA / 1000;
	return power_mW;
error:
	return -EIO;
}

static s32 show_power(struct device *dev,
		struct device_attribute *devattr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina3221_data *data = i2c_get_clientdata(client);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	u8 index, bus_volt_reg_addr, shunt_volt_reg_addr;
	s32 ret;
	s32 power_mW;

	mutex_lock(&data->mutex);
	if (data->shutdown_complete) {
		ret = -ENODEV;
		goto error;
	}
	index = attr->index;
	bus_volt_reg_addr = (INA3221_BUS_VOL_CHAN1 + (index * 2));
	shunt_volt_reg_addr = (INA3221_SHUNT_VOL_CHAN1 + (index * 2));

	if (data->mode == TRIGGERED) {
		ret = __locked_power_up_ina3221(client,
				data->plat_data->trig_conf_data);
		if (ret < 0) {
			dev_err(dev,
			"power up failed sts: 0x%x\n", ret);
			goto error;
		}
	}
	/*Will get -EIO on error*/
	power_mW = __locked_calculate_power(client, shunt_volt_reg_addr,
						bus_volt_reg_addr, index);
	if (power_mW < 0) {
		ret = power_mW;
		goto error;
	}

	if (data->mode == TRIGGERED) {
		/* set ina3221 to power down mode */
		ret = __locked_power_down_ina3221(client);
		if (ret < 0)
			goto error;
	}

	DEBUG_INA3221(("%s power = %d\n", __func__, power_mW));
	mutex_unlock(&data->mutex);
	return sprintf(buf, "%d mW\n", power_mW);
error:
	mutex_unlock(&data->mutex);
	dev_err(dev, "%s: failed\n", __func__);
	return ret;
}

static s32 show_power2(struct device *dev,
		struct device_attribute *devattr,
		char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina3221_data *data = i2c_get_clientdata(client);
	struct sensor_device_attribute *attr = to_sensor_dev_attr(devattr);
	u8 index, bus_volt_reg_addr, shunt_volt_reg_addr;
	s32 power_mW;
	s32 ret;

	mutex_lock(&data->mutex);
	if (data->shutdown_complete) {
		ret = -ENODEV;
		goto error;
	}

	/*return 0 if INA is off*/
	if (data->mode == TRIGGERED) {
		mutex_unlock(&data->mutex);
		return sprintf(buf, "%d mW\n", 0);
	}
	index = attr->index;
	bus_volt_reg_addr = (INA3221_BUS_VOL_CHAN1 + (index * 2));
	shunt_volt_reg_addr = (INA3221_SHUNT_VOL_CHAN1 + (index * 2));

	power_mW = __locked_calculate_power(client, shunt_volt_reg_addr,
						bus_volt_reg_addr, index);
	if (power_mW < 0) {
		ret = power_mW;
		goto error;
	}

	DEBUG_INA3221(("%s power = %d\n", __func__, power_mW));
	mutex_unlock(&data->mutex);
	return sprintf(buf, "%d mW\n", power_mW);
error:
	mutex_unlock(&data->mutex);
	dev_err(dev, "%s: failed\n", __func__);
	return ret;
}

static int ina3221_hotplug_notify(struct notifier_block *nb,
					unsigned long event,
					void *hcpu)
{
	struct ina3221_data *data = container_of(nb, struct ina3221_data,
						nb);
	struct i2c_client *client = data->client;
	int cpus;
	int ret = 0;
	if (event == CPU_ONLINE || event == CPU_DEAD) {
		mutex_lock(&data->mutex);
		cpus = num_online_cpus();
		DEBUG_INA3221(("INA3221 got CPU notification %d\n", cpus));
		if ((cpus >= CPU_THRESHOLD) && (data->mode == TRIGGERED)) {
			/**
			 * Turn INA on when number of cpu
			 * cores crosses threshold
			 */
			ret = __locked_power_up_ina3221(client,
					data->plat_data->cont_conf_data);
			DEBUG_INA3221(("Turning on ina3221, cpus:%d\n", cpus));
			if (ret < 0) {
				dev_err(&client->dev,
					"INA can't be turned on: 0x%x\n", ret);
				goto error;
			}
			data->mode = CONTINUOUS;
		} else if ((cpus < CPU_THRESHOLD) &&
						(data->mode == CONTINUOUS)) {
			/**
			 * Turn off ina when number of cpu
			 * cores on are below threshold
			 */
			ret = __locked_power_down_ina3221(client);
			DEBUG_INA3221(("Turning off INA3221 cpus%d\n", cpus));
			if (ret < 0) {
				dev_err(&client->dev,
					"INA can't be turned off: 0x%x\n", ret);
				goto error;
			}
			data->mode = TRIGGERED;
		}
		mutex_unlock(&data->mutex);
		return 0;
	} else
		return 0;

error:
	mutex_unlock(&data->mutex);
	dev_err(&client->dev, "INA can't be turned off/on: 0x%x\n", ret);
	return 0;
}

static struct sensor_device_attribute ina3221[] = {
	SENSOR_ATTR(rail_name_0, S_IRUGO, show_rail_name, NULL, 0),
	SENSOR_ATTR(in1_input_0, S_IRUGO, show_voltage, NULL, 0),
	SENSOR_ATTR(curr1_input_0, S_IRUGO, show_current, NULL, 0),
	SENSOR_ATTR(power1_input_0, S_IRUGO, show_power, NULL, 0),
	SENSOR_ATTR(power2_input_0, S_IRUGO, show_power2, NULL, 0),
	SENSOR_ATTR(rail_name_1, S_IRUGO, show_rail_name, NULL, 1),
	SENSOR_ATTR(in1_input_1, S_IRUGO, show_voltage, NULL, 1),
	SENSOR_ATTR(curr1_input_1, S_IRUGO, show_current, NULL, 1),
	SENSOR_ATTR(power1_input_1, S_IRUGO, show_power, NULL, 1),
	SENSOR_ATTR(power2_input_1, S_IRUGO, show_power2, NULL, 1),
	SENSOR_ATTR(rail_name_2, S_IRUGO, show_rail_name, NULL, 2),
	SENSOR_ATTR(in1_input_2, S_IRUGO, show_voltage, NULL, 2),
	SENSOR_ATTR(curr1_input_2, S_IRUGO, show_current, NULL, 2),
	SENSOR_ATTR(power1_input_2, S_IRUGO, show_power, NULL, 2),
	SENSOR_ATTR(power2_input_2, S_IRUGO, show_power2, NULL, 2),
	SENSOR_ATTR(running_mode, S_IRUGO, show_mode, NULL, 0),
/* mode setting :
 * running_mode = 0 ---> Triggered mode
 * running_mode > 0 ---> Continuous mode
 */
};

static int __devinit ina3221_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ina3221_data *data;
	int ret, i;

	data = devm_kzalloc(&client->dev, sizeof(struct ina3221_data),
						GFP_KERNEL);
	if (!data) {
		ret = -ENOMEM;
		goto exit;
	}
	i2c_set_clientdata(client, data);
	data->plat_data = client->dev.platform_data;
	mutex_init(&data->mutex);

	data->mode = TRIGGERED;
	data->shutdown_complete = 0;
	/* reset ina3221 */
	ret = i2c_smbus_write_word_data(client, INA3221_CONFIG,
		__constant_cpu_to_be16((INA3221_RESET)));
	if (ret < 0) {
		dev_err(&client->dev, "ina3221 reset failure status: 0x%x\n",
			ret);
		goto exit_free;
	}

	for (i = 0; i < ARRAY_SIZE(ina3221); i++) {
		ret = device_create_file(&client->dev, &ina3221[i].dev_attr);
		if (ret) {
			dev_err(&client->dev, "device_create_file failed.\n");
			goto exit_remove;
		}
	}
	data->client = client;
	data->nb.notifier_call = ina3221_hotplug_notify;
	register_hotcpu_notifier(&(data->nb));

	data->hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(data->hwmon_dev)) {
		ret = PTR_ERR(data->hwmon_dev);
		goto exit_remove;
	}

	/* set ina3221 to power down mode */
	ret = __locked_power_down_ina3221(client);
	if (ret < 0)
		goto exit_remove;

	return 0;

exit_remove:
	while (i--)
		device_remove_file(&client->dev, &ina3221[i].dev_attr);
exit_free:
	devm_kfree(&client->dev, data);
exit:
	return ret;
}

static int __devexit ina3221_remove(struct i2c_client *client)
{
	u8 i;
	struct ina3221_data *data = i2c_get_clientdata(client);
	mutex_lock(&data->mutex);
	__locked_power_down_ina3221(client);
	hwmon_device_unregister(data->hwmon_dev);
	mutex_unlock(&data->mutex);
	unregister_hotcpu_notifier(&(data->nb));
	for (i = 0; i < ARRAY_SIZE(ina3221); i++)
		device_remove_file(&client->dev, &ina3221[i].dev_attr);
	return 0;
}

static void ina3221_shutdown(struct i2c_client *client)
{
	struct ina3221_data *data = i2c_get_clientdata(client);
	mutex_lock(&data->mutex);
	__locked_power_down_ina3221(client);
	data->shutdown_complete = 1;
	mutex_unlock(&data->mutex);
}

static const struct i2c_device_id ina3221_id[] = {
	{DRIVER_NAME, 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ina3221_id);

static struct i2c_driver ina3221_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= DRIVER_NAME,
	},
	.probe		= ina3221_probe,
	.remove		= __devexit_p(ina3221_remove),
	.shutdown	= ina3221_shutdown,
	.id_table	= ina3221_id,
};

static int __init ina3221_init(void)
{
	return i2c_add_driver(&ina3221_driver);
}

static void __exit ina3221_exit(void)
{
	i2c_del_driver(&ina3221_driver);
}

module_init(ina3221_init);
module_exit(ina3221_exit);
MODULE_LICENSE("GPL");
