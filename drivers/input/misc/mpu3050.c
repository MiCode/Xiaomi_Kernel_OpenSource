/*
 * MPU3050 Tri-axis gyroscope driver
 *
 * Copyright (C) 2011 Wistron Co.Ltd
 * Joseph Lai <joseph_lai@wistron.com>
 *
 * Trimmed down by Alan Cox <alan@linux.intel.com> to produce this version
 *
 * This is a 'lite' version of the driver, while we consider the right way
 * to present the other features to user space. In particular it requires the
 * device has an IRQ, and it only provides an input interface, so is not much
 * use for device orientation. A fuller version is available from the Meego
 * tree.
 *
 * This program is based on bma023.c.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/gpio.h>
#include <linux/input/mpu3050.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <mach/gpiomux.h>

#define MPU3050_AUTO_DELAY	1000

#define MPU3050_MIN_VALUE	-32768
#define MPU3050_MAX_VALUE	32767

#define MPU3050_MIN_POLL_INTERVAL	1
#define MPU3050_MAX_POLL_INTERVAL	250
#define MPU3050_DEFAULT_POLL_INTERVAL	200
#define MPU3050_DEFAULT_FS_RANGE	3

/* Register map */
#define MPU3050_CHIP_ID_REG	0x00
#define MPU3050_SMPLRT_DIV	0x15
#define MPU3050_DLPF_FS_SYNC	0x16
#define MPU3050_INT_CFG		0x17
#define MPU3050_XOUT_H		0x1D
#define MPU3050_PWR_MGM		0x3E
#define MPU3050_PWR_MGM_POS	6

/* Register bits */

/* DLPF_FS_SYNC */
#define MPU3050_EXT_SYNC_NONE		0x00
#define MPU3050_EXT_SYNC_TEMP		0x20
#define MPU3050_EXT_SYNC_GYROX		0x40
#define MPU3050_EXT_SYNC_GYROY		0x60
#define MPU3050_EXT_SYNC_GYROZ		0x80
#define MPU3050_EXT_SYNC_ACCELX	0xA0
#define MPU3050_EXT_SYNC_ACCELY	0xC0
#define MPU3050_EXT_SYNC_ACCELZ	0xE0
#define MPU3050_EXT_SYNC_MASK		0xE0
#define MPU3050_FS_250DPS		0x00
#define MPU3050_FS_500DPS		0x08
#define MPU3050_FS_1000DPS		0x10
#define MPU3050_FS_2000DPS		0x18
#define MPU3050_FS_MASK		0x18
#define MPU3050_DLPF_CFG_256HZ_NOLPF2	0x00
#define MPU3050_DLPF_CFG_188HZ		0x01
#define MPU3050_DLPF_CFG_98HZ		0x02
#define MPU3050_DLPF_CFG_42HZ		0x03
#define MPU3050_DLPF_CFG_20HZ		0x04
#define MPU3050_DLPF_CFG_10HZ		0x05
#define MPU3050_DLPF_CFG_5HZ		0x06
#define MPU3050_DLPF_CFG_2100HZ_NOLPF	0x07
#define MPU3050_DLPF_CFG_MASK		0x07
/* INT_CFG */
#define MPU3050_RAW_RDY_EN		0x01
#define MPU3050_MPU_RDY_EN		0x04
#define MPU3050_LATCH_INT_EN		0x20
#define MPU3050_OPEN_DRAIN		0x40
#define MPU3050_ACTIVE_LOW		0x80
/* PWR_MGM */
#define MPU3050_PWR_MGM_PLL_X		0x01
#define MPU3050_PWR_MGM_PLL_Y		0x02
#define MPU3050_PWR_MGM_PLL_Z		0x03
#define MPU3050_PWR_MGM_CLKSEL		0x07
#define MPU3050_PWR_MGM_STBY_ZG	0x08
#define MPU3050_PWR_MGM_STBY_YG	0x10
#define MPU3050_PWR_MGM_STBY_XG	0x20
#define MPU3050_PWR_MGM_SLEEP		0x40
#define MPU3050_PWR_MGM_RESET		0x80
#define MPU3050_PWR_MGM_MASK		0x40

struct axis_data {
	s16 x;
	s16 y;
	s16 z;
};

struct mpu3050_sensor {
	struct i2c_client *client;
	struct device *dev;
	struct input_dev *idev;
	struct mpu3050_gyro_platform_data *platform_data;
	struct delayed_work input_work;
	u32    use_poll;
	u32    poll_interval;
	u32    dlpf_index;
	u32    enable_gpio;
	u32    enable;
};

struct sensor_regulator {
	struct regulator *vreg;
	const char *name;
	u32	min_uV;
	u32	max_uV;
};

struct sensor_regulator mpu_vreg[] = {
	{NULL, "vdd", 2100000, 3600000},
	{NULL, "vlogic", 1800000, 1800000},
};

static const int mpu3050_chip_ids[] = {
	0x68,
	0x69,
};

struct dlpf_cfg_tb {
	u8  cfg;	/* cfg index */
	u32 lpf_bw;	/* low pass filter bandwidth in Hz */
	u32 sample_rate; /* analog sample rate in Khz, 1 or 8 */
};

static struct dlpf_cfg_tb dlpf_table[] = {
	{6,   5, 1},
	{5,  10, 1},
	{4,  20, 1},
	{3,  42, 1},
	{2,  98, 1},
	{1, 188, 1},
	{0, 256, 8},
};

static u8 interval_to_dlpf_cfg(u32 interval)
{
	u32 sample_rate = 1000 / interval;
	u32 i;

	/* the filter bandwidth needs to be greater or
	 * equal to half of the sample rate
	 */
	for (i = 0; i < sizeof(dlpf_table)/sizeof(dlpf_table[0]); i++) {
		if (dlpf_table[i].lpf_bw * 2 >= sample_rate)
			return i;
	}

	/* return the maximum possible */
	return --i;
}

static int mpu3050_config_regulator(struct i2c_client *client, bool on)
{
	int rc = 0, i;
	int num_reg = sizeof(mpu_vreg) / sizeof(struct sensor_regulator);

	if (on) {
		for (i = 0; i < num_reg; i++) {
			mpu_vreg[i].vreg = regulator_get(&client->dev,
						mpu_vreg[i].name);
			if (IS_ERR(mpu_vreg[i].vreg)) {
				rc = PTR_ERR(mpu_vreg[i].vreg);
				pr_err("%s:regulator get failed rc=%d\n",
						__func__, rc);
				mpu_vreg[i].vreg = NULL;
				goto error_vdd;
			}

			if (regulator_count_voltages(mpu_vreg[i].vreg) > 0) {
				rc = regulator_set_voltage(mpu_vreg[i].vreg,
					mpu_vreg[i].min_uV, mpu_vreg[i].max_uV);
				if (rc) {
					pr_err("%s:set_voltage failed rc=%d\n",
						__func__, rc);
					regulator_put(mpu_vreg[i].vreg);
					mpu_vreg[i].vreg = NULL;
					goto error_vdd;
				}
			}

			rc = regulator_enable(mpu_vreg[i].vreg);
			if (rc) {
				pr_err("%s: regulator_enable failed rc =%d\n",
						__func__,
						rc);

				if (regulator_count_voltages(
					mpu_vreg[i].vreg) > 0) {
					regulator_set_voltage(mpu_vreg[i].vreg,
						0, mpu_vreg[i].max_uV);
				}
				regulator_put(mpu_vreg[i].vreg);
				mpu_vreg[i].vreg = NULL;
				goto error_vdd;
			}
		}
		return rc;
	} else {
		i = num_reg;
	}
error_vdd:
	while (--i >= 0) {
		if (!IS_ERR_OR_NULL(mpu_vreg[i].vreg)) {
			if (regulator_count_voltages(
				mpu_vreg[i].vreg) > 0) {
				regulator_set_voltage(mpu_vreg[i].vreg, 0,
						mpu_vreg[i].max_uV);
			}
			regulator_disable(mpu_vreg[i].vreg);
			regulator_put(mpu_vreg[i].vreg);
			mpu_vreg[i].vreg = NULL;
		}
	}
	return rc;
}

/**
 *	mpu3050_attr_get_polling_rate	-	get the sampling rate
 */
static ssize_t mpu3050_attr_get_polling_rate(struct device *dev,
				struct device_attribute *attr,
				char *buf)
{
	int val;
	struct mpu3050_sensor *sensor = dev_get_drvdata(dev);
	val = sensor ? sensor->poll_interval : 0;
	return snprintf(buf, 8, "%d\n", val);
}

/**
 *	mpu3050_attr_set_polling_rate	-	set the sampling rate
 */
static ssize_t mpu3050_attr_set_polling_rate(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t size)
{
	struct mpu3050_sensor *sensor = dev_get_drvdata(dev);
	unsigned long interval_ms;
	unsigned int  dlpf_index;
	u8  divider, reg;
	int ret;

	if (kstrtoul(buf, 10, &interval_ms))
		return -EINVAL;
	if ((interval_ms < MPU3050_MIN_POLL_INTERVAL) ||
		(interval_ms > MPU3050_MAX_POLL_INTERVAL))
		return -EINVAL;

	dlpf_index = interval_to_dlpf_cfg(interval_ms);
	divider = interval_ms * dlpf_table[dlpf_index].sample_rate - 1;

	if (sensor->dlpf_index != dlpf_index) {
		/* Set low pass filter and full scale */
		reg = dlpf_table[dlpf_index].cfg;
		reg |= MPU3050_DEFAULT_FS_RANGE << 3;
		reg |= MPU3050_EXT_SYNC_NONE << 5;
		ret = i2c_smbus_write_byte_data(sensor->client,
				MPU3050_DLPF_FS_SYNC, reg);
		if (ret == 0)
			sensor->dlpf_index = dlpf_index;
	}

	if (sensor->poll_interval != interval_ms) {
		/* Output frequency divider. The poll interval */
		ret = i2c_smbus_write_byte_data(sensor->client,
				MPU3050_SMPLRT_DIV, divider);
		if (ret == 0)
			sensor->poll_interval = interval_ms;
	}

	return size;
}

/**
 *  Set/get enable function is just needed by sensor HAL.
 */

static ssize_t mpu3050_attr_set_enable(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct mpu3050_sensor *sensor = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;
	sensor->enable = (u32)val == 0 ? 0 : 1;
	if (sensor->enable) {
		pm_runtime_get_sync(sensor->dev);
		gpio_set_value(sensor->enable_gpio, 1);
		if (sensor->use_poll)
			schedule_delayed_work(&sensor->input_work,
				msecs_to_jiffies(sensor->poll_interval));
		else {
			i2c_smbus_write_byte_data(sensor->client,
					MPU3050_INT_CFG,
					MPU3050_ACTIVE_LOW |
					MPU3050_OPEN_DRAIN |
					MPU3050_RAW_RDY_EN);
			enable_irq(sensor->client->irq);
		}
	} else {
		if (sensor->use_poll)
			cancel_delayed_work_sync(&sensor->input_work);
		else
			disable_irq(sensor->client->irq);
		gpio_set_value(sensor->enable_gpio, 0);
		pm_runtime_put(sensor->dev);
	}
	return count;
}

static ssize_t mpu3050_attr_get_enable(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct mpu3050_sensor *sensor = dev_get_drvdata(dev);

	return snprintf(buf, 4, "%d\n", sensor->enable);
}

static struct device_attribute attributes[] = {
	__ATTR(pollrate_ms, 0664,
		mpu3050_attr_get_polling_rate,
		mpu3050_attr_set_polling_rate),
	__ATTR(enable, 0644,
		mpu3050_attr_get_enable,
		mpu3050_attr_set_enable),
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;
	int err;
	for (i = 0; i < ARRAY_SIZE(attributes); i++) {
		err = device_create_file(dev, attributes + i);
		if (err)
			goto error;
	}
	return 0;

error:
	for ( ; i >= 0; i--)
		device_remove_file(dev, attributes + i);
	dev_err(dev, "%s:Unable to create interface\n", __func__);
	return err;
}

static int remove_sysfs_interfaces(struct device *dev)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(attributes); i++)
		device_remove_file(dev, attributes + i);
	return 0;
}

/**
 *	mpu3050_xyz_read_reg	-	read the axes values
 *	@buffer: provide register addr and get register
 *	@length: length of register
 *
 *	Reads the register values in one transaction or returns a negative
 *	error code on failure.
 */
static int mpu3050_xyz_read_reg(struct i2c_client *client,
			       u8 *buffer, int length)
{
	/*
	 * Annoying we can't make this const because the i2c layer doesn't
	 * declare input buffers const.
	 */
	char cmd = MPU3050_XOUT_H;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &cmd,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = buffer,
		},
	};

	return i2c_transfer(client->adapter, msg, 2);
}

/**
 *	mpu3050_read_xyz	-	get co-ordinates from device
 *	@client: i2c address of sensor
 *	@coords: co-ordinates to update
 *
 *	Return the converted X Y and Z co-ordinates from the sensor device
 */
static void mpu3050_read_xyz(struct i2c_client *client,
			     struct axis_data *coords)
{
	u16 buffer[3];

	mpu3050_xyz_read_reg(client, (u8 *)buffer, 6);
	coords->x = be16_to_cpu(buffer[0]);
	coords->y = be16_to_cpu(buffer[1]);
	coords->z = be16_to_cpu(buffer[2]);
	dev_dbg(&client->dev, "%s: x %d, y %d, z %d\n", __func__,
					coords->x, coords->y, coords->z);
}

/**
 *	mpu3050_set_power_mode	-	set the power mode
 *	@client: i2c client for the sensor
 *	@val: value to switch on/off of power, 1: normal power, 0: low power
 *
 *	Put device to normal-power mode or low-power mode.
 */
static void mpu3050_set_power_mode(struct i2c_client *client, u8 val)
{
	u8 value;
	struct mpu3050_sensor *sensor = i2c_get_clientdata(client);

	if (val) {
		mpu3050_config_regulator(client, 1);
		udelay(10);
		gpio_set_value(sensor->enable_gpio, 1);
		msleep(60);
	}

	value = i2c_smbus_read_byte_data(client, MPU3050_PWR_MGM);
	value = (value & ~MPU3050_PWR_MGM_MASK) |
		(((val << MPU3050_PWR_MGM_POS) & MPU3050_PWR_MGM_MASK) ^
		 MPU3050_PWR_MGM_MASK);
	i2c_smbus_write_byte_data(client, MPU3050_PWR_MGM, value);

	if (!val) {
		udelay(10);
		gpio_set_value(sensor->enable_gpio, 0);
		udelay(10);
		mpu3050_config_regulator(client, 0);
	}
}

/**
 *	mpu3050_input_open	-	called on input event open
 *	@input: input dev of opened device
 *
 *	The input layer calls this function when input event is opened. The
 *	function will push the device to resume. Then, the device is ready
 *	to provide data.
 */
static int mpu3050_input_open(struct input_dev *input)
{
	struct mpu3050_sensor *sensor = input_get_drvdata(input);
	int error;

	pm_runtime_get_sync(sensor->dev);

	/* Enable interrupts */
	error = i2c_smbus_write_byte_data(sensor->client, MPU3050_INT_CFG,
					MPU3050_ACTIVE_LOW |
					MPU3050_OPEN_DRAIN |
					MPU3050_RAW_RDY_EN);
	if (error < 0) {
		pm_runtime_put(sensor->dev);
		return error;
	}
	if (sensor->use_poll)
		schedule_delayed_work(&sensor->input_work,
			msecs_to_jiffies(sensor->poll_interval));

	return 0;
}

/**
 *	mpu3050_input_close	-	called on input event close
 *	@input: input dev of closed device
 *
 *	The input layer calls this function when input event is closed. The
 *	function will push the device to suspend.
 */
static void mpu3050_input_close(struct input_dev *input)
{
	struct mpu3050_sensor *sensor = input_get_drvdata(input);

	if (sensor->use_poll)
		cancel_delayed_work_sync(&sensor->input_work);

	pm_runtime_put(sensor->dev);
}

/**
 *	mpu3050_interrupt_thread	-	handle an IRQ
 *	@irq: interrupt numner
 *	@data: the sensor
 *
 *	Called by the kernel single threaded after an interrupt occurs. Read
 *	the sensor data and generate an input event for it.
 */
static irqreturn_t mpu3050_interrupt_thread(int irq, void *data)
{
	struct mpu3050_sensor *sensor = data;
	struct axis_data axis;

	mpu3050_read_xyz(sensor->client, &axis);

	input_report_abs(sensor->idev, ABS_X, axis.x);
	input_report_abs(sensor->idev, ABS_Y, axis.y);
	input_report_abs(sensor->idev, ABS_Z, axis.z);
	input_sync(sensor->idev);

	return IRQ_HANDLED;
}

/**
 *	mpu3050_input_work_fn -		polling work
 *	@work: the work struct
 *
 *	Called by the work queue; read sensor data and generate an input
 *	event
 */
static void mpu3050_input_work_fn(struct work_struct *work)
{
	struct mpu3050_sensor *sensor;
	struct axis_data axis;

	sensor = container_of((struct delayed_work *)work,
				struct mpu3050_sensor, input_work);

	mpu3050_read_xyz(sensor->client, &axis);

	input_report_abs(sensor->idev, ABS_X, axis.x);
	input_report_abs(sensor->idev, ABS_Y, axis.y);
	input_report_abs(sensor->idev, ABS_Z, axis.z);
	input_sync(sensor->idev);

	if (sensor->use_poll)
		schedule_delayed_work(&sensor->input_work,
			msecs_to_jiffies(sensor->poll_interval));
}

/**
 *	mpu3050_hw_init	-	initialize hardware
 *	@sensor: the sensor
 *
 *	Called during device probe; configures the sampling method.
 */
static int mpu3050_hw_init(struct mpu3050_sensor *sensor)
{
	struct i2c_client *client = sensor->client;
	int ret;
	u8 reg;

	/* Reset */
	ret = i2c_smbus_write_byte_data(client, MPU3050_PWR_MGM,
					MPU3050_PWR_MGM_RESET);
	if (ret < 0)
		return ret;

	ret = i2c_smbus_read_byte_data(client, MPU3050_PWR_MGM);
	if (ret < 0)
		return ret;

	ret &= ~MPU3050_PWR_MGM_CLKSEL;
	ret |= MPU3050_PWR_MGM_PLL_Z;
	ret = i2c_smbus_write_byte_data(client, MPU3050_PWR_MGM, ret);
	if (ret < 0)
		return ret;

	/* Output frequency divider. The poll interval */
	ret = i2c_smbus_write_byte_data(client, MPU3050_SMPLRT_DIV,
					sensor->poll_interval - 1);
	if (ret < 0)
		return ret;

	/* Set low pass filter and full scale */
	reg = MPU3050_DLPF_CFG_42HZ;
	reg |= MPU3050_DEFAULT_FS_RANGE << 3;
	reg |= MPU3050_EXT_SYNC_NONE << 5;
	ret = i2c_smbus_write_byte_data(client, MPU3050_DLPF_FS_SYNC, reg);
	if (ret < 0)
		return ret;

	return 0;
}
#ifdef CONFIG_OF
static int mpu3050_parse_dt(struct device *dev,
			struct mpu3050_gyro_platform_data *pdata)
{
	int rc = 0;

	rc = of_property_read_u32(dev->of_node, "invn,poll-interval",
				&pdata->poll_interval);
	if (rc) {
		dev_err(dev, "Failed to read poll-interval\n");
		return rc;
	}

	/* check gpio_int later, if it is invalid, just use poll */
	pdata->gpio_int = of_get_named_gpio_flags(dev->of_node,
				"invn,gpio-int", 0, NULL);

	pdata->gpio_en = of_get_named_gpio_flags(dev->of_node,
				"invn,gpio-en", 0, NULL);
	if (!gpio_is_valid(pdata->gpio_en))
		return -EINVAL;

	return 0;
}
#else
static int mpu3050_parse_dt(struct device *dev,
			struct mpu3050_gyro_platform_data *pdata)
{
	return -EINVAL;
}
#endif

/**
 *	mpu3050_probe	-	device detection callback
 *	@client: i2c client of found device
 *	@id: id match information
 *
 *	The I2C layer calls us when it believes a sensor is present at this
 *	address. Probe to see if this is correct and to validate the device.
 *
 *	If present install the relevant sysfs interfaces and input device.
 */
static int mpu3050_probe(struct i2c_client *client,
				   const struct i2c_device_id *id)
{
	struct mpu3050_sensor *sensor;
	struct input_dev *idev;
	struct mpu3050_gyro_platform_data *pdata;
	int ret;
	int error;
	u32 i;

	sensor = kzalloc(sizeof(struct mpu3050_sensor), GFP_KERNEL);
	idev = input_allocate_device();
	if (!sensor || !idev) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	sensor->client = client;
	sensor->dev = &client->dev;
	sensor->idev = idev;
	i2c_set_clientdata(client, sensor);

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct mpu3050_gyro_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allcated memory\n");
			error = -ENOMEM;
			goto err_free_mem;
		}
		ret = mpu3050_parse_dt(&client->dev, pdata);
		if (ret) {
			dev_err(&client->dev, "Failed to parse device tree\n");
			error = ret;
			goto err_free_mem;
		}
	} else
		pdata = client->dev.platform_data;
	sensor->platform_data = pdata;

	if (sensor->platform_data) {
		u32 interval = sensor->platform_data->poll_interval;
		sensor->enable_gpio = sensor->platform_data->gpio_en;

		if ((interval < MPU3050_MIN_POLL_INTERVAL) ||
			(interval > MPU3050_MAX_POLL_INTERVAL))
			sensor->poll_interval = MPU3050_DEFAULT_POLL_INTERVAL;
		else
			sensor->poll_interval = interval;
	} else {
		sensor->poll_interval = MPU3050_DEFAULT_POLL_INTERVAL;
		sensor->enable_gpio = -EINVAL;
	}

	if (gpio_is_valid(sensor->enable_gpio)) {
		ret = gpio_request(sensor->enable_gpio, "GYRO_EN_PM");
		gpio_direction_output(sensor->enable_gpio, 1);
	}

	mpu3050_set_power_mode(client, 1);

	ret = i2c_smbus_read_byte_data(client, MPU3050_CHIP_ID_REG);
	if (ret < 0) {
		dev_err(&client->dev, "failed to detect device\n");
		error = -ENXIO;
		goto err_free_mem;
	}

	for (i = 0; i < ARRAY_SIZE(mpu3050_chip_ids); i++)
		if (ret == mpu3050_chip_ids[i])
			break;

	if (i == ARRAY_SIZE(mpu3050_chip_ids)) {
		dev_err(&client->dev, "unsupported chip id\n");
		error = -ENXIO;
		goto err_free_mem;
	}

	idev->name = "MPU3050";
	idev->id.bustype = BUS_I2C;

	idev->open = mpu3050_input_open;
	idev->close = mpu3050_input_close;

	input_set_capability(idev, EV_ABS, ABS_MISC);
	input_set_abs_params(idev, ABS_X,
			     MPU3050_MIN_VALUE, MPU3050_MAX_VALUE, 0, 0);
	input_set_abs_params(idev, ABS_Y,
			     MPU3050_MIN_VALUE, MPU3050_MAX_VALUE, 0, 0);
	input_set_abs_params(idev, ABS_Z,
			     MPU3050_MIN_VALUE, MPU3050_MAX_VALUE, 0, 0);

	input_set_drvdata(idev, sensor);

	pm_runtime_set_active(&client->dev);

	error = mpu3050_hw_init(sensor);
	if (error)
		goto err_pm_set_suspended;

	if (client->irq == 0) {
		sensor->use_poll = 1;
		INIT_DELAYED_WORK(&sensor->input_work, mpu3050_input_work_fn);
	} else {
		sensor->use_poll = 0;

		if (gpio_is_valid(sensor->platform_data->gpio_int)) {
			/* configure interrupt gpio */
			ret = gpio_request(sensor->platform_data->gpio_int,
								"gyro_gpio_int");
			if (ret) {
				pr_err("%s: unable to request interrupt gpio %d\n",
					__func__,
					sensor->platform_data->gpio_int);
				goto err_pm_set_suspended;
			}

			ret = gpio_direction_input(
				sensor->platform_data->gpio_int);
			if (ret) {
				pr_err("%s: unable to set direction for gpio %d\n",
				__func__, sensor->platform_data->gpio_int);
				goto err_free_gpio;
			}
			client->irq = gpio_to_irq(
					sensor->platform_data->gpio_int);
		} else {
			ret = -EINVAL;
			goto err_pm_set_suspended;
		}

		error = request_threaded_irq(client->irq,
				     NULL, mpu3050_interrupt_thread,
				     IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				     "mpu3050", sensor);
		if (error) {
			dev_err(&client->dev,
				"can't get IRQ %d, error %d\n",
				client->irq, error);
			goto err_pm_set_suspended;
		}
		disable_irq(client->irq);
	}

	error = input_register_device(idev);
	if (error) {
		dev_err(&client->dev, "failed to register input device\n");
		goto err_free_irq;
	}

	error = create_sysfs_interfaces(&idev->dev);
	if (error < 0) {
		dev_err(&client->dev, "failed to create sysfs\n");
		goto err_input_cleanup;
	}

	pm_runtime_enable(&client->dev);
	pm_runtime_set_autosuspend_delay(&client->dev, MPU3050_AUTO_DELAY);

	return 0;

err_input_cleanup:
	input_unregister_device(idev);
err_free_irq:
	if (client->irq > 0)
		free_irq(client->irq, sensor);
err_free_gpio:
	if ((client->irq > 0) &&
		(gpio_is_valid(sensor->platform_data->gpio_int)))
		gpio_free(sensor->platform_data->gpio_int);
err_pm_set_suspended:
	pm_runtime_set_suspended(&client->dev);
err_free_mem:
	input_free_device(idev);
	kfree(sensor);
	return error;
}

/**
 *	mpu3050_remove	-	remove a sensor
 *	@client: i2c client of sensor being removed
 *
 *	Our sensor is going away, clean up the resources.
 */
static int mpu3050_remove(struct i2c_client *client)
{
	struct mpu3050_sensor *sensor = i2c_get_clientdata(client);

	pm_runtime_disable(&client->dev);
	pm_runtime_set_suspended(&client->dev);

	if (client->irq)
		free_irq(client->irq, sensor);

	remove_sysfs_interfaces(&client->dev);
	if (gpio_is_valid(sensor->enable_gpio))
		gpio_free(sensor->enable_gpio);
	input_unregister_device(sensor->idev);

	kfree(sensor);

	return 0;
}

#ifdef CONFIG_PM
/**
 *	mpu3050_suspend		-	called on device suspend
 *	@dev: device being suspended
 *
 *	Put the device into sleep mode before we suspend the machine.
 */
static int mpu3050_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu3050_sensor *sensor = i2c_get_clientdata(client);

	if (!sensor->use_poll)
		disable_irq(client->irq);

	mpu3050_set_power_mode(client, 0);

	return 0;
}

/**
 *	mpu3050_resume		-	called on device resume
 *	@dev: device being resumed
 *
 *	Put the device into powered mode on resume.
 */
static int mpu3050_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mpu3050_sensor *sensor = i2c_get_clientdata(client);

	mpu3050_set_power_mode(client, 1);

	if (!sensor->use_poll)
		enable_irq(client->irq);

	return 0;
}
#endif

static UNIVERSAL_DEV_PM_OPS(mpu3050_pm, mpu3050_suspend, mpu3050_resume, NULL);

static const struct i2c_device_id mpu3050_ids[] = {
	{ "mpu3050", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mpu3050_ids);

static const struct of_device_id mpu3050_of_match[] = {
	{ .compatible = "invn,mpu3050", },
	{ },
};
MODULE_DEVICE_TABLE(of, mpu3050_of_match);

static struct i2c_driver mpu3050_i2c_driver = {
	.driver	= {
		.name	= "mpu3050",
		.owner	= THIS_MODULE,
		.pm	= &mpu3050_pm,
		.of_match_table = mpu3050_of_match,
	},
	.probe		= mpu3050_probe,
	.remove		= mpu3050_remove,
	.id_table	= mpu3050_ids,
};

module_i2c_driver(mpu3050_i2c_driver);

MODULE_AUTHOR("Wistron Corp.");
MODULE_DESCRIPTION("MPU3050 Tri-axis gyroscope driver");
MODULE_LICENSE("GPL");
