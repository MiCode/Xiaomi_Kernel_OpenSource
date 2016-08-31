/*
 * ina230.c - driver for TI INA230/INA226/HPA02149/HPA01112 current/power
 * monitor sensor
 *
 *
 * Copyright (c) 2009-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 *
 * The INA230(/INA226) is a sensor chip made by Texas Instruments. It measures
 * power, voltage and current on a power rail and provides an alert on
 * over voltage/power
 * Complete datasheet can be obtained from ti.com
 *
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
#include <linux/platform_data/ina230.h>
#include <linux/init.h>
#include <linux/hwmon-sysfs.h>
#include <linux/hwmon.h>
#include <linux/cpu.h>


#define DRIVER_NAME "ina230"

/* ina230 (/ ina226)register offsets */
#define INA230_CONFIG	0
#define INA230_SHUNT	1
#define INA230_VOLTAGE	2
#define INA230_POWER	3
#define INA230_CURRENT	4
#define INA230_CAL	5
#define INA230_MASK	6
#define INA230_ALERT	7

/*
Mask register for ina230 (/ina 226):
D15|D14|D13|D12|D11 D10 D09 D08 D07 D06 D05 D04 D03 D02 D01 D00
SOL|SUL|BOL|BUL|POL|CVR|-   -   -   -   -  |AFF|CVF|OVF|APO|LEN
*/
#define INA230_MASK_SOL		(1 << 15)
#define INA230_MASK_SUL		(1 << 14)
#define INA230_MASK_CVF		(1 << 3)
#define INA230_MAX_CONVERSION_TRIALS	50

struct ina230_data {
	struct device *hwmon_dev;
	struct i2c_client *client;
	struct ina230_platform_data *pdata;
	struct mutex mutex;
	bool running;
	struct notifier_block nb;
};

/* bus voltage resolution: 1.25mv */
#define busv_register_to_mv(x) (((x) * 5) >> 2)

/* shunt voltage resolution: 2.5uv */
#define shuntv_register_to_uv(x) (((x) * 5) >> 1)
#define uv_to_alert_register(x) (((x) << 1) / 5)

static s32 ensure_enabled_start(struct i2c_client *client)
{
	struct ina230_data *data = i2c_get_clientdata(client);
	int retval;

	if (data->running)
		return 0;

	retval = i2c_smbus_write_word_data(client, INA230_CONFIG,
			   __constant_cpu_to_be16(data->pdata->trig_conf));
	if (retval < 0)
		dev_err(&client->dev, "config data write failed sts: 0x%x\n",
			retval);

	return retval;
}


static void ensure_enabled_end(struct i2c_client *client)
{
	struct ina230_data *data = i2c_get_clientdata(client);
	int retval;

	if (data->running)
		return;

	retval = i2c_smbus_write_word_data(client, INA230_CONFIG,
				     __constant_cpu_to_be16(INA230_POWER_DOWN));
	if (retval < 0)
		dev_err(&client->dev, "power down failure sts: 0x%x\n",
			retval);
}


static s32 __locked_power_down_ina230(struct i2c_client *client)
{
	s32 retval;
	struct ina230_data *data = i2c_get_clientdata(client);

	if (!data->running)
		return 0;

	retval = i2c_smbus_write_word_data(client, INA230_MASK, 0);
	if (retval < 0)
		dev_err(&client->dev, "mask write failure sts: 0x%x\n",
			retval);

	retval = i2c_smbus_write_word_data(client, INA230_CONFIG,
				     __constant_cpu_to_be16(INA230_POWER_DOWN));
	if (retval < 0)
		dev_err(&client->dev, "power down failure sts: 0x%x\n",
			retval);

	data->running = false;

	return retval;
}


static s32 power_down_ina230(struct i2c_client *client)
{
	s32 retval;
	struct ina230_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->mutex);
	retval = __locked_power_down_ina230(client);
	mutex_unlock(&data->mutex);

	return retval;
}


static s32 __locked_start_current_mon(struct i2c_client *client)
{
	s32 retval;
	s32 shunt_uV;
	s16 shunt_limit;
	s16 alert_mask;
	struct ina230_data *data = i2c_get_clientdata(client);
	int mask_len;

	if (!data->pdata->current_threshold) {
		dev_err(&client->dev, "no current threshold specified\n");
		return -EINVAL;
	}

	retval = i2c_smbus_write_word_data(client, INA230_CONFIG,
			    __constant_cpu_to_be16(data->pdata->cont_conf));
	if (retval < 0) {
		dev_err(&client->dev, "config data write failed sts: 0x%x\n",
			retval);
		return retval;
	}

	if (data->pdata->resistor) {
		shunt_uV = data->pdata->resistor;
		shunt_uV *= data->pdata->current_threshold;
	} else {
		s32 v;
		/* no resistor value defined, compute shunt_uV the hard way */
		v = data->pdata->precision_multiplier * 5120 * 25;
		v /= data->pdata->calibration_data;
		v *= data->pdata->current_threshold;
		v /= data->pdata->power_lsb;
		shunt_uV = (s16)(v & 0xffff);
	}
	if (data->pdata->shunt_polarity_inverted)
		shunt_uV *= -1;

	shunt_limit = (s16) uv_to_alert_register(shunt_uV);

	retval = i2c_smbus_write_word_data(client, INA230_ALERT,
					   cpu_to_be16(shunt_limit));
	if (retval < 0) {
		dev_err(&client->dev, "alert data write failed sts: 0x%x\n",
			retval);
		return retval;
	}

	mask_len = data->pdata->alert_latch_enable ? 0x1 : 0x0;
	alert_mask = shunt_limit >= 0 ? INA230_MASK_SOL + mask_len :
		INA230_MASK_SUL + mask_len;
	retval = i2c_smbus_write_word_data(client, INA230_MASK,
					   cpu_to_be16(alert_mask));
	if (retval < 0) {
		dev_err(&client->dev, "mask data write failed sts: 0x%x\n",
			retval);
		return retval;
	}

	data->running = true;

	return 0;
}


static void __locked_evaluate_state(struct i2c_client *client)
{
	struct ina230_data *data = i2c_get_clientdata(client);
	int cpus = num_online_cpus();

	if (data->running) {
		if (cpus < data->pdata->min_cores_online ||
		    !data->pdata->current_threshold)
			__locked_power_down_ina230(client);
	} else {
		if (cpus >= data->pdata->min_cores_online &&
		    data->pdata->current_threshold)
			__locked_start_current_mon(client);
	}
}


static void evaluate_state(struct i2c_client *client)
{
	struct ina230_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->mutex);
	__locked_evaluate_state(client);
	mutex_unlock(&data->mutex);
}


static s32 show_rail_name(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina230_data *data = i2c_get_clientdata(client);
	return sprintf(buf, "%s\n", data->pdata->rail_name);
}


static s32 show_current_threshold(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina230_data *data = i2c_get_clientdata(client);
	return sprintf(buf, "%d mA\n", data->pdata->current_threshold);
}


static s32 set_current_threshold(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina230_data *data = i2c_get_clientdata(client);
	s32 retval;

	mutex_lock(&data->mutex);

	if (strict_strtol(buf, 10, (long *)&(data->pdata->current_threshold))) {
		retval = -EINVAL;
		goto out;
	}

	if (data->pdata->current_threshold) {
		if (data->running) {
			/* force restart */
			retval = __locked_start_current_mon(client);
		} else {
			__locked_evaluate_state(client);
			retval = 0;
		}
	} else {
		retval = __locked_power_down_ina230(client);
	}

out:
	mutex_unlock(&data->mutex);
	if (retval >= 0)
		return count;
	return retval;
}




#if MEASURE_BUS_VOLT
static s32 show_bus_voltage(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina230_data *data = i2c_get_clientdata(client);
	s32 voltage_mV;
	int retval;

	mutex_lock(&data->mutex);
	retval = ensure_enabled_start(client);
	if (retval < 0) {
		mutex_unlock(&data->mutex);
		return retval;
	}

	/* getting voltage readings in milli volts*/
	voltage_mV =
		(s16)be16_to_cpu(i2c_smbus_read_word_data(client,
							  INA230_VOLTAGE));

	ensure_enabled_end(client);
	mutex_unlock(&data->mutex);

	if (voltage_mV < 0) {
		dev_err(dev, "%s: failed\n", __func__);
		return -1;
	}

	voltage_mV = busv_register_to_mv(voltage_mV);

	return sprintf(buf, "%d mV\n", voltage_mV);
}
#endif




static s32 show_shunt_voltage(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina230_data *data = i2c_get_clientdata(client);
	s32 voltage_uV;
	int retval;

	mutex_lock(&data->mutex);
	retval = ensure_enabled_start(client);
	if (retval < 0) {
		mutex_unlock(&data->mutex);
		return retval;
	}

	voltage_uV =
		(s16)be16_to_cpu(i2c_smbus_read_word_data(client,
							  INA230_SHUNT));

	ensure_enabled_end(client);
	mutex_unlock(&data->mutex);

	voltage_uV = shuntv_register_to_uv(voltage_uV);

	return sprintf(buf, "%d uV\n", voltage_uV);
}

static int  __locked_wait_for_conversion(struct device *dev)
{
	int retval, conversion, trials = 0;
	struct i2c_client *client = to_i2c_client(dev);

	/* wait till conversion ready bit is set */
	do {
		retval = be16_to_cpu(i2c_smbus_read_word_data(client,
							INA230_MASK));
		if (retval < 0) {
			dev_err(dev, "mask data read failed sts: 0x%x\n",
				retval);
			return retval;
		}
		conversion = retval & INA230_MASK_CVF;
	} while ((!conversion) && (++trials < INA230_MAX_CONVERSION_TRIALS));

	if (trials == INA230_MAX_CONVERSION_TRIALS) {
		dev_err(dev, "maximum retries exceeded\n");
		return -EAGAIN;
	}

	return 0;
}

static s32 show_current(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina230_data *data = i2c_get_clientdata(client);
	s32 current_mA;
	int retval;

	mutex_lock(&data->mutex);
	retval = ensure_enabled_start(client);
	if (retval < 0) {
		mutex_unlock(&data->mutex);
		return retval;
	}

	/* fill calib data */
	retval = i2c_smbus_write_word_data(client, INA230_CAL,
		__constant_cpu_to_be16(data->pdata->calibration_data));
	if (retval < 0) {
		dev_err(dev, "calibration data write failed sts: 0x%x\n",
			retval);
		mutex_unlock(&data->mutex);
		return retval;
	}

	retval = __locked_wait_for_conversion(dev);
	if (retval) {
		mutex_unlock(&data->mutex);
		return retval;
	}

	/* getting current readings in milli amps*/
	retval = i2c_smbus_read_word_data(client, INA230_CURRENT);
	if (retval < 0) {
		mutex_unlock(&data->mutex);
		return retval;
	}
	current_mA = (s16) be16_to_cpu(retval);

	ensure_enabled_end(client);
	mutex_unlock(&data->mutex);

	if (data->pdata->shunt_polarity_inverted)
		current_mA *= -1;

	current_mA *= (s16) data->pdata->power_lsb;
	if (data->pdata->divisor)
		current_mA /= (s16) data->pdata->divisor;
	if (data->pdata->precision_multiplier)
		current_mA /= (s16) data->pdata->precision_multiplier;

	return sprintf(buf, "%d mA\n", current_mA);
}

static s32 show_current2(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina230_data *data = i2c_get_clientdata(client);
	s32 voltage_uV;
	s32 inverse_shunt_resistor, current_mA;
	int retval;

	mutex_lock(&data->mutex);
	retval = ensure_enabled_start(client);
	if (retval < 0) {
		mutex_unlock(&data->mutex);
		return retval;
	}

	voltage_uV =
		(s16)be16_to_cpu(i2c_smbus_read_word_data(client,
							  INA230_SHUNT));

	ensure_enabled_end(client);
	mutex_unlock(&data->mutex);

	voltage_uV = shuntv_register_to_uv(voltage_uV);
	voltage_uV = abs(voltage_uV);

	inverse_shunt_resistor = 1000 / data->pdata->resistor;
	current_mA = voltage_uV * inverse_shunt_resistor / 1000;

	return sprintf(buf, "%d mA\n", current_mA);
}

static s32 show_power(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina230_data *data = i2c_get_clientdata(client);
	s32 power_mW;
	int retval;

	mutex_lock(&data->mutex);
	retval = ensure_enabled_start(client);
	if (retval < 0) {
		mutex_unlock(&data->mutex);
		return retval;
	}

	/* fill calib data */
	retval = i2c_smbus_write_word_data(client, INA230_CAL,
		__constant_cpu_to_be16(data->pdata->calibration_data));
	if (retval < 0) {
		dev_err(dev, "calibration data write failed sts: 0x%x\n",
			retval);
		mutex_unlock(&data->mutex);
		return retval;
	}

	retval = __locked_wait_for_conversion(dev);
	if (retval) {
		mutex_unlock(&data->mutex);
		return retval;
	}

	/* getting power readings in milli watts*/
	power_mW = be16_to_cpu(i2c_smbus_read_word_data(client,
		INA230_POWER));
	if (power_mW < 0) {
		mutex_unlock(&data->mutex);
		return -EINVAL;
	}

	ensure_enabled_end(client);
	mutex_unlock(&data->mutex);

	power_mW =
		power_mW * data->pdata->power_lsb;
	if (data->pdata->precision_multiplier)
		power_mW /= data->pdata->precision_multiplier;

	return sprintf(buf, "%d mW\n", power_mW);
}

static s32 show_power2(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina230_data *data = i2c_get_clientdata(client);
	s32 power_mW, voltage_uV, voltage_mV;
	s32 inverse_shunt_resistor, current_mA;
	int retval;

	mutex_lock(&data->mutex);
	retval = ensure_enabled_start(client);
	if (retval < 0) {
		mutex_unlock(&data->mutex);
		return retval;
	}

	voltage_mV =
		(s16)be16_to_cpu(i2c_smbus_read_word_data(client,
							  INA230_VOLTAGE));

	voltage_uV =
		(s16)be16_to_cpu(i2c_smbus_read_word_data(client,
							  INA230_SHUNT));

	ensure_enabled_end(client);
	mutex_unlock(&data->mutex);

	voltage_mV = busv_register_to_mv(voltage_mV);

	voltage_uV = shuntv_register_to_uv(voltage_uV);
	voltage_uV = abs(voltage_uV);

	inverse_shunt_resistor = 1000 / data->pdata->resistor;
	current_mA = voltage_uV * inverse_shunt_resistor / 1000;
	power_mW = voltage_mV * current_mA / 1000;

	return sprintf(buf, "%d mW\n", power_mW);
}

static s32 show_alert_flag(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ina230_data *data = i2c_get_clientdata(client);
	int retval;
	s32 alert_flag;

	mutex_lock(&data->mutex);
	retval = ensure_enabled_start(client);
	if (retval < 0) {
		mutex_unlock(&data->mutex);
		return retval;
	}

	alert_flag = be16_to_cpu(i2c_smbus_read_word_data(client,
							  INA230_MASK));

	ensure_enabled_end(client);
	mutex_unlock(&data->mutex);

	alert_flag = (alert_flag >> 4) & 0x1;
	return sprintf(buf, "%d\n", alert_flag);
}


static int ina230_hotplug_notify(struct notifier_block *nb, unsigned long event,
				void *hcpu)
{
	struct ina230_data *data = container_of(nb, struct ina230_data,
						nb);
	struct i2c_client *client = data->client;

	if (event == CPU_ONLINE || event == CPU_DEAD)
		evaluate_state(client);

	return 0;
}



static struct sensor_device_attribute ina230[] = {
	SENSOR_ATTR(rail_name, S_IRUGO, show_rail_name, NULL, 0),
	SENSOR_ATTR(current_threshold, S_IWUSR | S_IRUGO,
		    show_current_threshold, set_current_threshold, 0),
	SENSOR_ATTR(shuntvolt1_input, S_IRUGO, show_shunt_voltage, NULL, 0),
	SENSOR_ATTR(curr1_input, S_IRUGO, show_current, NULL, 0),
	SENSOR_ATTR(curr2_input, S_IRUGO, show_current2, NULL, 0),
#if MEASURE_BUS_VOLT
	SENSOR_ATTR(in1_input, S_IRUGO, show_bus_voltage, NULL, 0),
#endif
	SENSOR_ATTR(power1_input, S_IRUGO, show_power, NULL, 0),
	SENSOR_ATTR(power2_input, S_IRUGO, show_power2, NULL, 0),
	SENSOR_ATTR(alert_flag, S_IRUGO, show_alert_flag, NULL, 0),
};


static int ina230_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct ina230_data *data;
	int err;
	u8 i;

	data = devm_kzalloc(&client->dev, sizeof(struct ina230_data),
			    GFP_KERNEL);
	if (!data) {
		err = -ENOMEM;
		goto exit;
	}

	i2c_set_clientdata(client, data);
	data->pdata = client->dev.platform_data;
	data->running = false;
	data->nb.notifier_call = ina230_hotplug_notify;
	data->client = client;
	mutex_init(&data->mutex);

	err = i2c_smbus_write_word_data(client, INA230_CONFIG,
		__constant_cpu_to_be16(INA230_RESET));
	if (err < 0) {
		dev_err(&client->dev, "ina230 reset failure status: 0x%x\n",
			err);
		goto exit;
	}

	for (i = 0; i < ARRAY_SIZE(ina230); i++) {
		err = device_create_file(&client->dev, &ina230[i].dev_attr);
		if (err) {
			dev_err(&client->dev, "device_create_file failed.\n");
			goto exit_remove;
		}
	}

	data->hwmon_dev = hwmon_device_register(&client->dev);
	if (IS_ERR(data->hwmon_dev)) {
		err = PTR_ERR(data->hwmon_dev);
		goto exit_remove;
	}

	register_hotcpu_notifier(&(data->nb));

	err = i2c_smbus_write_word_data(client, INA230_MASK, 0);
	if (err < 0) {
		dev_err(&client->dev, "mask write failure sts: 0x%x\n",
			err);
		goto exit_remove;
	}

	/* set ina230 to power down mode */
	err = i2c_smbus_write_word_data(client, INA230_CONFIG,
				     __constant_cpu_to_be16(INA230_POWER_DOWN));
	if (err < 0) {
		dev_err(&client->dev, "power down failure sts: 0x%x\n",
			err);
		goto exit_remove;
	}

	return 0;

exit_remove:
	for (i = 0; i < ARRAY_SIZE(ina230); i++)
		device_remove_file(&client->dev, &ina230[i].dev_attr);
exit:
	return err;
}


static int ina230_remove(struct i2c_client *client)
{
	u8 i;
	struct ina230_data *data = i2c_get_clientdata(client);
	unregister_hotcpu_notifier(&(data->nb));
	power_down_ina230(client);
	hwmon_device_unregister(data->hwmon_dev);
	for (i = 0; i < ARRAY_SIZE(ina230); i++)
		device_remove_file(&client->dev, &ina230[i].dev_attr);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ina230_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	return power_down_ina230(client);
}


static int ina230_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);

	evaluate_state(client);
	return 0;
}
#endif

static const struct dev_pm_ops ina230_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ina230_suspend, ina230_resume)
};

static const struct i2c_device_id ina230_id[] = {
	{"ina226", 0 },
	{"ina230", 0 },
	{"hpa01112", 0 },
	{"hpa02149", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ina230_id);


static struct i2c_driver ina230_driver = {
	.class		= I2C_CLASS_HWMON,
	.driver = {
		.name	= DRIVER_NAME,
		.pm	= &ina230_pm_ops,
	},
	.probe		= ina230_probe,
	.remove		= ina230_remove,
	.id_table	= ina230_id,
};


static int __init ina230_init(void)
{
	return i2c_add_driver(&ina230_driver);
}


static void __exit ina230_exit(void)
{
	i2c_del_driver(&ina230_driver);
}


module_init(ina230_init);
module_exit(ina230_exit);
MODULE_LICENSE("GPL");
