/* drivers/input/misc/isl29035.c
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/input/isl29035.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/* isl29035 register list */
#define ISL29035_CMD_1					0x00
#define ISL29035_CMD_2					0x01
#define ISL29035_DATA_L					0x02
#define ISL29035_DATA_H					0x03
#define ISL29035_THRES_LOW_L				0x04
#define ISL29035_THRES_LOW_H				0x05
#define ISL29035_THRES_HIGH_L				0x06
#define ISL29035_THRES_HIGH_H				0x07
#define ISL29035_DEVICE_ID_REG				0x0F

/* configure command1 bits */
#define ISL29035_INT_PRST_MASK				0xFC
#define ISL29035_INT_PRST_1				0x00
#define ISL29035_INT_PRST_4				0x01
#define ISL29035_INT_PRST_8				0x02
#define ISL29035_INT_PRST_6				0x03
#define ISL29035_MODE_MASK				0x1F
#define ISL29035_PWR_DOWN				0x00
#define ISL29035_ALS_ONCE				0x20
#define ISL29035_IR_ONCE				0x40
#define ISL29035_ALS_CONTINUE				0xA0
#define ISL29035_IR_CONTINUE				0xC0

#define ISL29035_CMD1_DEF				ISL29035_INT_PRST_1

/* configure command2 bits */
#define ISL29035_RANGE_MASK				0xFC
#define ISL29035_RANGE_LOW				0x00
#define ISL29035_RANGE_HIGH				0x02
#define ISL29035_RES_MASK				0xF3
#define ISL29035_RES_4					0x00
#define ISL29035_RES_8					0x04
#define ISL29035_RES_12					0x80
#define ISL29035_RES_16					0xC0

#define ISL29035_CMD2_DEF				ISL29035_RES_16



/* functionality need to support by i2c adapter */
#define ISL29035_FUNC	\
	(I2C_FUNC_SMBUS_BYTE_DATA | I2C_FUNC_SMBUS_READ_I2C_BLOCK)

/* ambient light convertion time */
#define ISL29035_ALS_CONVERTION_TIME			350

/* ambient light range and resolution(unit is mLux) */
#define ISL29035_ALS_MIN				0
#define ISL29035_ALS_MAX				2137590
#define ISL29035_ALS_RES				33

#define ISL29035_DEVICE_ID_MASK				0xB8
#define ISL29035_DEVICE_ID				0xA8


/* isl29035 device context */
struct isl29035_data {
	/* common state */
	struct mutex		mutex;
	struct wake_lock	wake_lock;
	struct delayed_work	delayed_work;
	struct i2c_client	*client;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend	early_suspend;
#endif
	bool			suspended;
	bool			early_suspended;
	unsigned long		dump_period;
	bool			dump_output;
	bool			dump_register;
	/* for light sensor */
	struct input_dev	*als_input;
	bool			als_opened;
	bool			als_enabled;
	bool			als_first_convertion;
	bool			als_highrange;
	bool			als_autorange;
	unsigned long		als_sensitive;
	u16			als_light;
	int			als_last_value;
	bool			irq_process;
};

static bool adjust_als_range(u16 light, bool autorange,
		bool highrange, u16 lowthres, u16 highthres)
{
	if (autorange) {
		if (light <= lowthres)
			highrange = false;
		else if (light >= highthres)
			highrange = true;
	}

	return highrange;
}

static int scale_als_output(u16 light, bool highrange, unsigned long factor)
{
	int mlux;

	if (highrange)
		mlux = 244 * light;
	else
		mlux = 15 * light;

	return factor * mlux;
}

static u8 get_reg_command1(bool als_active)
{
	u8 config = ISL29035_CMD1_DEF;

	if (als_active)
		config |= ISL29035_ALS_CONTINUE;

	return config;
}

static u8 get_reg_command2(bool highrange)
{
	u8 config = ISL29035_CMD2_DEF;

	if (highrange)
		config |= ISL29035_RANGE_HIGH;

	return config;
}

static u16 read_als_output(struct isl29035_data *data)
{
	u8			buf[2];
	u16			light	= 0;
	int			error	= 0;
	struct i2c_client	*client	= data->client;

	/* read this register to clear the interrupte status bit */
	error = i2c_smbus_read_i2c_block_data(client,
			ISL29035_CMD_1, 1, buf);
	if (error < 0)
		dev_err(&client->dev, "fail to read command1 register.");

	error = i2c_smbus_read_i2c_block_data(client,
			ISL29035_DATA_L, sizeof(buf), buf);
	if (error >= 0)
		light = (buf[1] << 8) | buf[0];
	else
		dev_err(&client->dev, "fail to read als output register.");

	return light;
}

static int set_als_threshold(struct isl29035_data *data,
		u16 light, u16 sensitive)
{
	u8			buf[4];
	u16			lowthres;
	u16			highthres;
	int			error	= 0;
	struct i2c_client	*client	= data->client;

	if (data->als_first_convertion) {
		lowthres = 0x00;
		highthres = 0xffff;
	} else {
		lowthres  = max((light*(100-sensitive))/100, 0x0000);
		highthres = min((light*(100+sensitive))/100, 0xffff);
	}

	buf[0] = lowthres;
	buf[1] = lowthres >> 8;
	buf[2] = highthres;
	buf[3] = highthres >> 8;

	error = i2c_smbus_write_i2c_block_data(client,
			ISL29035_THRES_LOW_L, sizeof(buf), buf);
	if (error < 0)
		dev_err(&client->dev, "fail to set als threshold range.");

	return error;
}


static bool als_active(struct isl29035_data *data)
{
	return data->als_opened && data->als_enabled &&
		!data->suspended && !data->early_suspended;
}

#define DUMP_REGISTER(data, reg) \
		dev_info(&data->client->dev, "%s\t: %02x", #reg, \
			i2c_smbus_read_byte_data(data->client, reg))

static void dump_register(struct isl29035_data *data)
{
	DUMP_REGISTER(data, ISL29035_CMD_1);
	DUMP_REGISTER(data, ISL29035_CMD_2);
	DUMP_REGISTER(data, ISL29035_DATA_L);
	DUMP_REGISTER(data, ISL29035_DATA_H);
	DUMP_REGISTER(data, ISL29035_THRES_LOW_L);
	DUMP_REGISTER(data, ISL29035_THRES_LOW_H);
	DUMP_REGISTER(data, ISL29035_THRES_HIGH_L);
	DUMP_REGISTER(data, ISL29035_THRES_HIGH_H);
	DUMP_REGISTER(data, ISL29035_DEVICE_ID_REG);
}

static int update_device(struct isl29035_data *data)
{
	int				error	= 0;
	struct i2c_client		*client	= data->client;


	/* set sensor threshold */
	if (als_active(data)) {
		error = set_als_threshold(data,
				data->als_light, data->als_sensitive);
		if (error < 0)
			goto exit;
	}

	/* set configure command2 */
	error = i2c_smbus_write_byte_data(client, ISL29035_CMD_2,
			get_reg_command2(data->als_highrange));
	if (error < 0) {
		dev_err(&client->dev, "fail to set configure command1.");
		goto exit;
	}

	/* set configure command1 */
	error = i2c_smbus_write_byte_data(client, ISL29035_CMD_1,
			get_reg_command1(als_active(data)));
	if (error < 0) {
		dev_err(&client->dev, "fail to set configure command1.");
		goto exit;
	}

	/* wait the initial sampling finish except interrupt handle */
	if (als_active(data) && !data->irq_process)
		msleep(100);

	/* schedule period work if need */
	if ((data->dump_period) && als_active(data)) {
		schedule_delayed_work(&data->delayed_work,
			data->dump_period * HZ / 1000);
	} else
		cancel_delayed_work(&data->delayed_work);

	if (data->dump_register)
		dump_register(data);

exit:
	return error;
}


/* sysfs interface */
static ssize_t isl29035_show_dump_period(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned long		dump_period;
	struct isl29035_data	*data;

	data = dev_get_drvdata(dev);

	mutex_lock(&data->mutex);
	dump_period = data->dump_period;
	mutex_unlock(&data->mutex);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", dump_period);
}

static ssize_t isl29035_store_dump_period(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t cnt)
{
	int			error;
	unsigned long		dump_period;
	struct isl29035_data	*data;

	data = dev_get_drvdata(dev);

	error = strict_strtoul(buf, 0, &dump_period);
	if (error >= 0) {
		mutex_lock(&data->mutex);
		data->dump_period = dump_period;
		error = update_device(data);
		mutex_unlock(&data->mutex);
	}

	return error < 0 ? error : cnt;
}

static ssize_t isl29035_show_dump_output(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned long		dump_output;
	struct isl29035_data	*data;

	data = dev_get_drvdata(dev);

	mutex_lock(&data->mutex);
	dump_output = data->dump_output;
	mutex_unlock(&data->mutex);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", dump_output);
}

static ssize_t isl29035_store_dump_output(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t cnt)
{
	int			error;
	unsigned long		dump_output;
	struct isl29035_data	*data;

	data = dev_get_drvdata(dev);

	error = strict_strtoul(buf, 0, &dump_output);
	if (error >= 0) {
		mutex_lock(&data->mutex);
		data->dump_output = dump_output;
		error = update_device(data);
		mutex_unlock(&data->mutex);
	}

	return error < 0 ? error : cnt;
}

static ssize_t isl29035_show_dump_register(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned long		dump_register;
	struct isl29035_data	*data;

	data = dev_get_drvdata(dev);

	mutex_lock(&data->mutex);
	dump_register = data->dump_register;
	mutex_unlock(&data->mutex);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", dump_register);
}

static ssize_t isl29035_store_dump_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t cnt)
{
	int			error;
	unsigned long		dump_register;
	struct isl29035_data	*data;

	data = dev_get_drvdata(dev);

	error = strict_strtoul(buf, 0, &dump_register);
	if (error >= 0) {
		mutex_lock(&data->mutex);
		data->dump_register = dump_register;
		error = update_device(data);
		mutex_unlock(&data->mutex);
	}

	return error < 0 ? error : cnt;
}

static ssize_t isl29035_show_als_enable(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned long		enabled;
	struct isl29035_data	*data;

	data = dev_get_drvdata(dev);

	mutex_lock(&data->mutex);
	enabled = data->als_enabled;
	mutex_unlock(&data->mutex);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", enabled);
}

static ssize_t isl29035_store_als_enable(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t cnt)
{
	int			error;
	unsigned long		enabled;
	struct isl29035_data	*data;

	data = dev_get_drvdata(dev);
	error = strict_strtoul(buf, 0, &enabled);
	if (error >= 0) {
		mutex_lock(&data->mutex);
		data->als_enabled = enabled;
		if (enabled) {
			data->als_first_convertion = true;
			data->dump_period = ISL29035_ALS_CONVERTION_TIME;
		} else {
			data->dump_period = 0;
		}
		error = update_device(data);
		mutex_unlock(&data->mutex);
	}

	return error < 0 ? error : cnt;
}

static ssize_t isl29035_show_als_period(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* isl29035 just support 800ms fix period */
	return scnprintf(buf, PAGE_SIZE, "%lu\n", 800000000UL);
}

static ssize_t isl29035_store_als_period(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t cnt)
{
	int		error;
	unsigned long	period;

	/* period is fix in isl29035, just check format */
	error = strict_strtoul(buf, 0, &period);
	return error < 0 ? error : cnt;
}

static ssize_t isl29035_show_als_range(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned long		range;
	struct isl29035_data	*data;

	data = dev_get_drvdata(dev);

	mutex_lock(&data->mutex);
	range = data->als_highrange;
	mutex_unlock(&data->mutex);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", range);
}

static ssize_t isl29035_store_als_range(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t cnt)
{
	int			error;
	unsigned long		range;
	struct isl29035_data	*data;

	data = dev_get_drvdata(dev);

	error = strict_strtoul(buf, 0, &range);
	if (error >= 0) {
		mutex_lock(&data->mutex);
		/* 0==>low range 1==>high range 2==>auto range */
		data->als_highrange = (range == 1);
		data->als_autorange = (range == 2);
		error = update_device(data);
		mutex_unlock(&data->mutex);
	}

	return error < 0 ? error : cnt;
}

static ssize_t isl29035_show_als_sensitive(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned long		sensitive;
	struct isl29035_data	*data;

	data = dev_get_drvdata(dev);

	mutex_lock(&data->mutex);
	sensitive = data->als_sensitive;
	mutex_unlock(&data->mutex);

	return scnprintf(buf, PAGE_SIZE, "%lu\n", sensitive);
}

static ssize_t isl29035_store_als_sensitive(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t cnt)
{
	int			error;
	unsigned long		sensitive;
	struct isl29035_data	*data;

	data = dev_get_drvdata(dev);

	error = strict_strtoul(buf, 0, &sensitive);
	if (error >= 0) {
		mutex_lock(&data->mutex);
		data->als_sensitive = sensitive;
		error = update_device(data);
		mutex_unlock(&data->mutex);
	}

	return error < 0 ? error : cnt;
}


static struct device_attribute dev_attr_dump_period =
	__ATTR(dump_period , S_IWUSR | S_IRUGO,
		isl29035_show_dump_period, isl29035_store_dump_period);
static struct device_attribute dev_attr_dump_output =
	__ATTR(dump_output, S_IWUSR | S_IRUGO,
		isl29035_show_dump_output, isl29035_store_dump_output);
static struct device_attribute dev_attr_dump_register =
	__ATTR(dump_register, S_IWUSR | S_IRUGO,
		isl29035_show_dump_register, isl29035_store_dump_register);

static struct device_attribute dev_attr_als_enable =
	__ATTR(enable, S_IWUSR | S_IRUGO,
		isl29035_show_als_enable, isl29035_store_als_enable);
static struct device_attribute dev_attr_als_poll_delay =
	__ATTR(poll_delay, S_IWUSR | S_IRUGO,
		isl29035_show_als_period, isl29035_store_als_period);
static struct device_attribute dev_attr_als_range =
	__ATTR(range, S_IWUSR | S_IRUGO,
		isl29035_show_als_range , isl29035_store_als_range);
static struct device_attribute dev_attr_als_sensitive =
	__ATTR(sensitive, S_IWUSR | S_IRUGO,
		isl29035_show_als_sensitive, isl29035_store_als_sensitive);

static struct attribute *isl29035_als_attrs[] = {
	&dev_attr_dump_period.attr   ,
	&dev_attr_dump_output.attr   ,
	&dev_attr_dump_register.attr ,
	&dev_attr_als_enable.attr    ,
	&dev_attr_als_poll_delay.attr,
	&dev_attr_als_range.attr     ,
	&dev_attr_als_sensitive.attr ,
	NULL
};

static struct attribute_group isl29035_als_attr_grp = {
	.attrs = isl29035_als_attrs,
};

static const struct attribute_group *isl29035_als_attr_grps[] = {
	&isl29035_als_attr_grp,
	NULL
};


/* input device driver interface */
static int isl29035_als_open(struct input_dev *dev)
{
	int			error = 0;
	struct isl29035_data	*data = input_get_drvdata(dev);

	mutex_lock(&data->mutex);
	data->als_opened = true;
	error = update_device(data);
	mutex_unlock(&data->mutex);

	return error;
}

static void isl29035_als_close(struct input_dev *dev)
{
	struct isl29035_data *data = input_get_drvdata(dev);

	mutex_lock(&data->mutex);
	data->als_opened = false;
	update_device(data);
	mutex_unlock(&data->mutex);
}


/* interrupt handler */
static irqreturn_t isl29035_interrupt(int irq, void *dev)
{
	u16				light;
	int				value;
	int				value_report;
	int				count;
	struct isl29035_data		*data;
	struct isl29035_platform_data	*pdata;

	data = dev;
	pdata = data->client->dev.platform_data;
	mutex_lock(&data->mutex);
	data->irq_process = true;

	/* collect ambient light */
	if (als_active(data)) {
		light = read_als_output(data);
		value = scale_als_output(light,
				data->als_highrange, pdata->als_factor);

		data->als_light = light;
		data->als_highrange = adjust_als_range(light,
				data->als_autorange, data->als_highrange,
				pdata->als_lowthres, pdata->als_highthres);

		value_report = value;

		if (data->als_first_convertion) {
			data->als_first_convertion = false;
			data->dump_period = 0;

			/* force to report the value */
			if (value_report == data->als_last_value)
				value_report += 1;
		}

		data->als_last_value = value;

		input_report_abs(data->als_input, ABS_MISC, value_report);
		input_sync(data->als_input);
		if (data->dump_output) {
			dev_info(&data->client->dev,
				"light=%04hu(%07dmLux).", light, value);
		}
	}

	/* try acknowledge interrupt three time before fail */
	for (count = 0; count < 3; count++) {
		if (update_device(data) >= 0)
			break;

		dev_err(&data->client->dev,
			"fail to acknowledge interrupt(%d).", count);
		msleep(1);
	}

	if (count == 3) /* disable irq to avoid irq storm */
		disable_irq_nosync(data->client->irq);

	data->irq_process = false;
	mutex_unlock(&data->mutex);
	return IRQ_HANDLED;
}

/* work for period dump sensor output */
static void isl29035_work(struct work_struct *work)
{
	struct isl29035_data *data = container_of(
		to_delayed_work(work), struct isl29035_data, delayed_work);

	/* reuse the interrupt handler logic */
	isl29035_interrupt(data->client->irq, data);
}

/* i2c client driver interface */
static void teardown_als_input(struct isl29035_data *data, bool unreg)
{
	if (data->als_input) {
		kfree(data->als_input->phys);
		if (unreg)
			input_unregister_device(data->als_input);
		else
			input_free_device(data->als_input);
		data->als_input = NULL;
	}
}

static int setup_als_input(struct isl29035_data *data)
{
	int				error = 0;
	bool				unreg = false;
	struct isl29035_platform_data	*pdata;

	pdata = data->client->dev.platform_data;

	data->als_input = input_allocate_device();
	if (data->als_input == NULL) {
		dev_err(&data->client->dev, "fail to allocate als input.");
		error = -ENOMEM;
		goto exit_teardown_input;
	}
	input_set_drvdata(data->als_input, data);

	data->als_input->name		= "isl29035";
	data->als_input->open		= isl29035_als_open;
	data->als_input->close		= isl29035_als_close;
	data->als_input->dev.groups	= isl29035_als_attr_grps;

	input_set_capability(data->als_input, EV_ABS, ABS_MISC);
	input_set_abs_params(data->als_input,
		ABS_MISC, pdata->als_factor*ISL29035_ALS_MIN,
		pdata->als_factor*ISL29035_ALS_MAX, 0, 0);

	error = input_register_device(data->als_input);
	unreg = (error >= 0);
	if (error < 0) {
		dev_err(&data->client->dev, "fail to register als input.");
		goto exit_teardown_input;
	}

	data->als_input->phys =
		kobject_get_path(&data->als_input->dev.kobj, GFP_KERNEL);
	if (data->als_input->phys == NULL) {
		dev_err(&data->client->dev, "fail to get als sysfs path.");
		error = -ENOMEM;
		goto exit_teardown_input;
	}

	goto exit; /* all is fine */

exit_teardown_input:
	teardown_als_input(data, unreg);
exit:
	return error;
}



static int isl29035_setup(struct isl29035_data *data)
{
	struct isl29035_platform_data *pdata = data->client->dev.platform_data;
	if (pdata->setup)
		return pdata->setup(data->client, pdata);
	else
		return 0;
}

static int isl29035_get_deviceid(struct isl29035_data *data)
{
	int error = 0;
	unsigned char id;

	error = i2c_smbus_read_byte_data(data->client, ISL29035_DEVICE_ID_REG);
	if (error < 0)
		return error;

	id = (unsigned char)error;
	id &= ISL29035_DEVICE_ID_MASK;
	if (id != ISL29035_DEVICE_ID)
		return -ENODEV;

	return error;
}

static int isl29035_teardown(struct isl29035_data *data)
{
	struct isl29035_platform_data *pdata = data->client->dev.platform_data;
	if (pdata->teardown)
		return pdata->teardown(data->client, pdata);
	else
		return 0;
}

#if defined(CONFIG_PM)
static int isl29035_suspend(struct device *dev)
{
	int			error = 0;
	struct isl29035_data	*data = dev_get_drvdata(dev);

	mutex_lock(&data->mutex);
	data->suspended = true;
	error = update_device(data);
	mutex_unlock(&data->mutex);

	return error;
}

static int isl29035_resume(struct device *dev)
{
	int			error = 0;
	struct isl29035_data	*data = dev_get_drvdata(dev);

	mutex_lock(&data->mutex);
	data->suspended = false;
	error = update_device(data);
	mutex_unlock(&data->mutex);

	return error;
}

static const struct dev_pm_ops isl29035_pm_ops = {
	.suspend	= isl29035_suspend,
	.resume		= isl29035_resume,
};
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void isl29035_early_suspend(struct early_suspend *h)
{
	struct isl29035_data *data = container_of(h,
			struct isl29035_data, early_suspend);

	mutex_lock(&data->mutex);
	data->early_suspended = true;
	update_device(data);
	mutex_unlock(&data->mutex);
}

static void isl29035_early_resume(struct early_suspend *h)
{
	struct isl29035_data *data = container_of(h,
			struct isl29035_data, early_suspend);

	mutex_lock(&data->mutex);
	data->early_suspended = false;
	update_device(data);
	mutex_unlock(&data->mutex);
}
#endif

static int isl29035_parse_dt(struct i2c_client *client)
{
	int err = -EINVAL;
	struct device_node *np = client->dev.of_node;
	struct isl29035_platform_data	*pdata = NULL;
	u32 temp;

	if (!client->dev.of_node)
		return -ENOENT;

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	err = of_property_read_u32(np, "als_factor", (u32 *)&pdata->als_factor);
	if (err < 0)
		goto free_platform_data;

	err = of_property_read_u32(np, "als_highrange", (u32 *)&pdata->als_highrange);
	if (err < 0)
		goto free_platform_data;

	err = of_property_read_u32(np, "als_lowthres", &temp);
	if (err < 0)
		goto free_platform_data;
	pdata->als_lowthres = temp;

	err = of_property_read_u32(np, "als_highthres", &temp);
	if (err < 0)
		goto free_platform_data;
	pdata->als_highthres = temp;

	err = of_property_read_u32(np, "als_sensitive", (u32 *)&pdata->als_sensitive);
	if (err < 0)
		goto free_platform_data;

	client->dev.platform_data = pdata;

	return err;

free_platform_data:
	dev_err(&client->dev, "err=%d\n", err);
	kfree(pdata);
	return err;
}

static int isl29035_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int				error;
	struct isl29035_data		*data;
	struct isl29035_platform_data	*pdata;

	error = isl29035_parse_dt(client);
	if (error < 0)
		goto exit;

	pdata = client->dev.platform_data;
	if (pdata == NULL) {
		dev_err(&client->dev, "invalid platform data.");
		error = -EINVAL;
		goto exit;
	}

	/* allocate device status */
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (data == NULL) {
		dev_err(&client->dev, "fail to allocate isl29035_data.");
		error = -ENOMEM;
		goto exit;
	}
	i2c_set_clientdata(client, data);

	mutex_init(&data->mutex);
	INIT_DELAYED_WORK(&data->delayed_work, isl29035_work);
	wake_lock_init(&data->wake_lock, WAKE_LOCK_SUSPEND, "isl29035");

	data->client		= client;
	data->als_highrange	= (pdata->als_highrange == 1);
	data->als_autorange	= (pdata->als_highrange == 2);
	data->als_sensitive	= pdata->als_sensitive;

	/* run platform setup */
	error = isl29035_setup(data);
	if (error < 0) {
		dev_err(&client->dev, "fail to perform platform setup.");
		goto exit_free_data;
	}

	/* verify device */
	if (i2c_check_functionality(client->adapter, ISL29035_FUNC) == 0) {
		dev_err(&client->dev, "incompatible adapter.");
		error = -ENODEV;
		goto exit_teardown;
	}

	error = isl29035_get_deviceid(data);
	if (error < 0) {
		dev_err(&client->dev, "fail to detect isl29035 chip.");
		error = -ENODEV;
		goto exit_teardown;
	}

	/* register to input system */
	error = setup_als_input(data);
	if (error < 0)
		goto exit_teardown;

	/* setup interrupt */
	error = request_threaded_irq(client->irq, NULL, isl29035_interrupt,
			IRQF_TRIGGER_LOW | IRQF_ONESHOT, "isl29035", data);
	if (error < 0) {
		dev_err(&client->dev, "fail to request irq.");
		goto exit_teardown_als_input;
	}


#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = isl29035_early_suspend;
	data->early_suspend.resume = isl29035_early_resume;
	register_early_suspend(&data->early_suspend);
#endif
	goto exit; /* all is fine */

exit_teardown_als_input:
	teardown_als_input(data, true);
exit_teardown:
	isl29035_teardown(data);
exit_free_data:
	kfree(pdata);
	wake_lock_destroy(&data->wake_lock);
	kfree(data);
exit:
	return error;
}

static int isl29035_remove(struct i2c_client *client)
{
	struct isl29035_data *data;
	data = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	free_irq(client->irq, data);
	kfree(client->dev.platform_data);

	teardown_als_input(data, true);

	isl29035_teardown(data);
	wake_lock_destroy(&data->wake_lock);
	cancel_delayed_work_sync(&data->delayed_work);
	kfree(data);

	return 0;
}

static const struct i2c_device_id isl29035_ids[] = {
	{"isl29028", 0 },
	{"isl29035", 0 },
	{/* list end */},
};

static const struct of_device_id intersil_of_match[] = {
	{ .compatible = "intersil,isl29035", },
	{ },
}

MODULE_DEVICE_TABLE(i2c, isl29035_ids);

static struct i2c_driver isl29035_driver = {
	.probe		= isl29035_probe,
	.remove		= isl29035_remove,
	.driver = {
		.name	= "isl29035",
		.of_match_table = of_match_ptr(intersil_of_match),
#ifdef CONFIG_PM
		.pm	= &isl29035_pm_ops,
#endif
	},
	.id_table	= isl29035_ids,
};

/* module initialization and termination */
static int __init isl29035_init(void)
{
	return i2c_add_driver(&isl29035_driver);
}

static void __exit isl29035_exit(void)
{
	i2c_del_driver(&isl29035_driver);
}

module_init(isl29035_init);
module_exit(isl29035_exit);

MODULE_AUTHOR("Qian Wenfa <qianwenfa@xiaomi.com>");
MODULE_DESCRIPTION("isl29035 light sensor input driver");
MODULE_LICENSE("GPL");
