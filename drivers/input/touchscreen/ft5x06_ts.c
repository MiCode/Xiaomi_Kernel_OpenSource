/*
 * drivers/input/touchscreen/ft5x06_ts.c
 *
 * FocalTech ft5x0x TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
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
 * VERSION		DATE			AUTHOR
 *    1.0		  2010-01-05			WenFS
 *
 * note: only support mulititouch	Wenfs 2010-10-01
 */

#define DEBUG

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/input/mt.h>
#include <linux/acpi.h>
#include <linux/gpio/consumer.h>
#include <linux/input/ft5x06_ts.h>

#ifdef CONFIG_PM_SLEEP
#include <linux/power_hal_sysfs.h>
#endif

#define FT5X0X_NAME "ft5x0x"
#define FT5X0X_IRQ_NAME "ft5x0x_gpio_irq"
#define FT5X0X_RESET_NAME "ft5x0x_gpio_reset"
#define FT5X0X_WAKE_NAME "ft5x0x_gpio_wake"
#define BYTES_PER_LINE 16
#define MAX_REG_LEN 256

struct ts_event {
	u16 au16_x[CFG_MAX_TOUCH_POINTS];	/*x coordinate */
	u16 au16_y[CFG_MAX_TOUCH_POINTS];	/*y coordinate */
	u8 au8_touch_event[CFG_MAX_TOUCH_POINTS];	/*touch event:
					0 -- down; 1-- contact; 2 -- contact */
	u8 au8_finger_id[CFG_MAX_TOUCH_POINTS];	/*touch ID */
	u16 pressure;
	u8 touch_point;
};

struct ft5x0x_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	int irq;
	struct mutex lock;
	u16 touch_points;
	u8 addr;
	u8 reg;
	bool x_flip;
	bool y_flip;
	bool swap_axis;
	bool wake_avail;
	struct gpio_desc *gpio_irq;
	struct gpio_desc *gpio_wake;
	struct gpio_desc *gpio_reset;
	u8 power_mode;
	u8 op_mode;
	struct ts_event event;

#ifdef CONFIG_PM_SLEEP
	struct mutex suspend_lock;
	bool suspended;
	bool power_hal_want_suspend;
#endif
};

#ifdef CONFIG_PM_SLEEP
static void ft5x0x_ts_power_hal_suspend(struct device *dev);
static void ft5x0x_ts_power_hal_resume(struct device *dev);
static int ft5x0x_ts_power_hal_suspend_init(struct device *dev);
static void ft5x0x_ts_power_hal_suspend_destroy(struct device *dev);
#endif

static int ft5x0x_i2c_rxdata(struct i2c_client *client,
	char *rxdata, int length)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= 1,
			.buf	= rxdata,
		},
		{
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= length,
			.buf	= rxdata,
		},
	};

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret < 0)
		dev_err(&client->dev, "%s error: %d\n", __func__, ret);

	return ret;
}

static int ft5x0x_i2c_txdata(struct i2c_client *client,
	char *txdata, int length)
{
	int ret;

	struct i2c_msg msg[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.len	= length,
			.buf	= txdata,
		},
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s error: %d\n", __func__, ret);

	return ret;
}

static int ft5x0x_write_reg(struct i2c_client *client, u8 addr, u8 value)
{
	u8 buf[2];
	int ret;

	buf[0] = addr;
	buf[1] = value;

	ret = ft5x0x_i2c_txdata(client, buf, 2);
	if (ret < 0)
		dev_err(&client->dev, "%s failed! %#x ret: %d\n",
						__func__, buf[0], ret);

	return ret;
}

static int ft5x0x_read_reg(struct i2c_client *client, u8 addr, u8 *pdata)
{
	int ret;
	u8 buf[2] = {0};

	buf[0] = addr;

	ret = ft5x0x_i2c_rxdata(client, buf, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s failed! %#x ret: %d\n",
						__func__, buf[0], ret);

	*pdata = buf[0];

	return ret;
}

/* set ft5x0x power mode 0-active, 1-monitor, 3-hibernate */
static int ft5x0x_set_pmode(struct ft5x0x_ts_data *ts_data, u8 mode)
{
	int ret;
	struct i2c_client *client = ts_data->client;

	/* set the touch pmode */
	ret = ft5x0x_write_reg(client, FT5X0X_REG_PMODE, mode);

	if (ret < 0)
		dev_err(&client->dev, "set pmode %d failed!\n", mode);

	mutex_lock(&ts_data->lock);
	ts_data->power_mode = mode;
	mutex_unlock(&ts_data->lock);

	return ret;
}

/* set ft5x0x device mode 0-normal, 1-system, 4-test */
static int ft5x0x_set_devmode(struct ft5x0x_ts_data *ts_data, u8 mode)
{
	int ret;
	struct i2c_client *client = ts_data->client;

	/* set the touch mode */
	ret = ft5x0x_write_reg(client, FT5X0X_REG_DEV_MODE, mode);

	if (ret < 0)
		dev_err(&client->dev, "set mode %d failed!\n", mode);

	mutex_lock(&ts_data->lock);
	ts_data->op_mode = mode;
	mutex_unlock(&ts_data->lock);

	msleep(FT5X0X_DEVMODE_MS);

	return ret;
}

static int ft5x0x_reg_init(struct ft5x0x_ts_data *ts_data)
{
	int ret;
	struct i2c_client *client = ts_data->client;

	/* set the touch threshold */
	ret = ft5x0x_write_reg(client, FT5X0X_REG_THGROUP, FT5X0X_THGROUP);
	if (ret < 0) {
		dev_err(&client->dev, "set threshold group failed!\n");
		return ret;
	}

	/* set device mode to normal */
	ret = ft5x0x_set_devmode(ts_data, DEV_MODE_NORMAL);

	if (ret < 0)
		return ret;

	return 0;
}

static int ft5x0x_wake_device(struct ft5x0x_ts_data *ts_data)
{
	/* wake the device by wake line*/
	gpiod_set_value(ts_data->gpio_reset, 0);
	usleep_range(6000, 7000);
	gpiod_set_value(ts_data->gpio_reset, 1);
	msleep(FT5X0X_RST_MS);

	return 0;
}

static int ft5x0x_report_touch(struct ft5x0x_ts_data *ft)
{
	struct ts_event *event = &ft->event;
	u8 buf[62] = {0};
	u8 num_touch, pointid, i = 0;
	int ret, uppoint = 0;

	ret = ft5x0x_i2c_rxdata(ft->client, buf, 62);
	if (ret < 0) {
		dev_err(&ft->client->dev, "ft5x0x %s i2c_rxdata failed: %d",
			__func__, ret);
		return ret;
	}

	memset(event, 0, sizeof(struct ts_event));
	event->touch_point = 0;

	num_touch = buf[FT5X0X_REG_TS_NUM];

	if (!num_touch) {
		for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++) {
			input_mt_slot(ft->input_dev, i);
			input_mt_report_slot_state(ft->input_dev,
				MT_TOOL_FINGER, false);
		}
	}

	for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++) {
		pointid = (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
		if (pointid >= 0x0F)
			break;
		else
			event->touch_point++;

		event->au16_x[i] =
		    (s16) (buf[FT_TOUCH_X_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_X_L_POS + FT_TOUCH_STEP * i];
		event->au16_y[i] =
		    (s16) (buf[FT_TOUCH_Y_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_Y_L_POS + FT_TOUCH_STEP * i];

		event->au8_touch_event[i] =
		    buf[FT_TOUCH_EVENT_POS + FT_TOUCH_STEP * i] >> 6;
		event->au8_finger_id[i] =
		    (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;

	}

	event->pressure = FT_PRESS;

	/* report the touch*/
	for (i = 0; i < event->touch_point; i++) {
		input_mt_slot(ft->input_dev, event->au8_finger_id[i]);

		if (event->au8_touch_event[i] == 0
			|| event->au8_touch_event[i] == 2) {

			input_mt_report_slot_state(ft->input_dev,
				MT_TOOL_FINGER,
				true);
			input_report_abs(ft->input_dev, ABS_MT_TOUCH_MAJOR,
					event->pressure);

			input_report_abs(ft->input_dev, ABS_MT_POSITION_X,
					event->au16_x[i]);

			input_report_abs(ft->input_dev, ABS_MT_POSITION_Y,
					event->au16_y[i]);
		} else {
			uppoint++;
			input_mt_report_slot_state(ft->input_dev,
				MT_TOOL_FINGER,
				false);
		}
	}

	if (event->touch_point == uppoint)
		input_report_key(ft->input_dev, BTN_TOUCH, 0);
	else
		input_report_key(ft->input_dev, BTN_TOUCH,
				event->touch_point > 0);
	input_sync(ft->input_dev);

	return 0;
}

static irqreturn_t ft5x0x_irq_handler(int irq, void *dev_id)
{

	struct ft5x0x_ts_data *ft = dev_id;

	mutex_lock(&ft->lock);
	ft5x0x_report_touch(ft);
	mutex_unlock(&ft->lock);

	return IRQ_HANDLED;
}

static int ft5x0x_gpio_init(struct ft5x0x_ts_data *ts_data)
{
	struct i2c_client *client;
	struct device *dev;
	int ret;

	if (!ts_data)
		return -EINVAL;

	client = ts_data->client;
	dev = &client->dev;

	ret = gpiod_direction_input(ts_data->gpio_irq);
	if (ret) {
		dev_err(dev, "gpio %d dir set failed\n",
				desc_to_gpio(ts_data->gpio_irq));
		return ret;
	}

	ret = gpiod_direction_output(ts_data->gpio_reset, 1);
	if (ret) {
		dev_err(dev, "gpio %d dir set failed\n",
				desc_to_gpio(ts_data->gpio_reset));
		return ret;
	}

	if (ts_data->wake_avail) {
		ret = gpiod_direction_output(ts_data->gpio_wake, 1);
		if (ret) {
			dev_err(dev, "gpio %d dir set failed\n",
					desc_to_gpio(ts_data->gpio_wake));
			return ret;
		}
	}

	return 0;
}

static int ft5x0x_irq_init(struct ft5x0x_ts_data *ts_data)
{
	struct i2c_client *client;
	struct device *dev;
	int ret;

	if (!ts_data)
		return -EINVAL;

	client = ts_data->client;
	dev = &client->dev;

	ret = gpiod_to_irq(ts_data->gpio_irq);
	if (ret < 0) {
		dev_err(dev, "gpio %d irq failed\n",
				desc_to_gpio(ts_data->gpio_irq));
		return ret;
	}

	/* Update client irq if its invalid */
	if (client->irq < 0)
		client->irq = ret;

	dev_dbg(dev, "gpio no:%d irq:%d\n",
			desc_to_gpio(ts_data->gpio_irq), ret);

	return 0;
}

static int ft5x0x_platform_probe(struct ft5x0x_ts_data *ts_data)
{
	struct i2c_client *client = ts_data->client;
	struct device *dev = &client->dev;
	struct ft5x0x_ts_platform_data *pdata;
	int err;

	pdata = dev->platform_data;

	if (!pdata)
		return -ENODEV;

	ts_data->gpio_irq = gpio_to_desc(pdata->irq);
	ts_data->gpio_reset = gpio_to_desc(pdata->reset);
	ts_data->x_flip = pdata->x_flip;
	ts_data->y_flip = pdata->y_flip;
	ts_data->swap_axis = pdata->swap_axis;

	if (pdata->wake > 0) {
		ts_data->gpio_wake = gpio_to_desc(pdata->wake);
		ts_data->wake_avail = true;
	} else {
		ts_data->gpio_wake = NULL;
		ts_data->wake_avail = false;
	}

	err = gpio_request(desc_to_gpio(ts_data->gpio_irq),
					FT5X0X_IRQ_NAME);
	if (err < 0) {
		dev_err(dev, "Failed request gpio=%d irq error=%d\n",
			pdata->irq, err);
		return err;
	}

	err = gpio_request(desc_to_gpio(ts_data->gpio_reset),
						FT5X0X_RESET_NAME);
	if (err < 0) {
		dev_err(dev, "Failed request gpio=%d reset error=%d\n",
			pdata->reset, err);
		return err;
	}

	if (ts_data->wake_avail) {
		err = gpio_request(desc_to_gpio(ts_data->gpio_wake),
						FT5X0X_WAKE_NAME);
		if (err < 0) {
			dev_err(dev, "Failed request gpio=%d wake error=%d\n",
				pdata->wake, err);
			return err;
		}
	}

	err = ft5x0x_gpio_init(ts_data);
	if (err < 0) {
		dev_err(dev, "gpio init failed\n");
		return err;
	}

	err = ft5x0x_irq_init(ts_data);
	if (err < 0) {
		dev_err(dev, "irq init failed\n");
		return err;
	}

	return 0;
}

static int ft5x0x_acpi_probe(struct ft5x0x_ts_data *ts_data)
{
	const struct acpi_device_id *id;
	struct i2c_client *client = ts_data->client;
	struct device *dev;
	struct gpio_desc *gpio;
	int ret, index = 0;

	if (!client)
		return -EINVAL;

	dev = &client->dev;

	if (!ACPI_HANDLE(dev))
		return -ENODEV;

	id = acpi_match_device(dev->driver->acpi_match_table, dev);

	if (!id)
		return -ENODEV;

	/* touch gpio interrupt pin */
	gpio = devm_gpiod_get_index(dev, FT5X0X_IRQ_NAME, index);

	if (IS_ERR(gpio)) {
		dev_err(dev, "acpi gpio get index %d failed\n", index);
		return PTR_ERR(gpio);
	}

	ts_data->gpio_irq = gpio;

	dev_dbg(dev, "gpio resource %d, no %d\n", index, desc_to_gpio(gpio));

	index++;

	/* touch gpio reset pin */
	gpio = devm_gpiod_get_index(dev, FT5X0X_RESET_NAME, index);

	if (IS_ERR(gpio)) {
		dev_err(dev, "acpi gpio get index %d failed\n", index);
		return PTR_ERR(gpio);
	}

	ts_data->gpio_reset = gpio;

	dev_dbg(dev, "gpio resource %d, no %d\n", index, desc_to_gpio(gpio));

	index++;

	/* touch gpio wake pin */
	gpio = devm_gpiod_get_index(dev, FT5X0X_WAKE_NAME, index);

	if (IS_ERR(gpio)) {
		dev_warn(dev, "acpi gpio get index 2 failed\n");
		ts_data->wake_avail = false;
		ts_data->gpio_wake = NULL;
	} else {
		ts_data->gpio_wake = gpio;
		ts_data->wake_avail = true;
	}

	dev_dbg(dev, "gpio resource %d, no %d\n", index, desc_to_gpio(gpio));

	ret = ft5x0x_gpio_init(ts_data);
	if (ret < 0) {
		dev_err(dev, "gpio init failed\n");
		return ret;
	}

	ret = ft5x0x_irq_init(ts_data);
	if (ret < 0) {
		dev_err(dev, "irq init failed\n");
		return ret;
	}

	return 0;
}

#ifdef DEBUG

int ft5x0x_reg_dump(struct ft5x0x_ts_data *ts_data, u8 mode)
{
	int ret, i, j;
	u8 reg_buf[MAX_REG_LEN] = {0};
	u8 dbg_buf_str[BYTES_PER_LINE * 3 + 1] = "";
	u8 reg_buf_str[BYTES_PER_LINE * 3 + 1] = "";
	struct i2c_client *client = ts_data->client;

	ret = ft5x0x_set_devmode(ts_data, mode);
	if (ret < 0)
		return ret;

	ret = ft5x0x_i2c_rxdata(ts_data->client, reg_buf, MAX_REG_LEN);
	if (ret < 0)
		return ret;

	for (j = 0; j < MAX_REG_LEN;) {
		for (i = 0; i < BYTES_PER_LINE; i++, j++) {
			sprintf(dbg_buf_str + i * 3, "%02x%c",
				j,
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));
			sprintf(reg_buf_str + i * 3, "%02x%c",
				reg_buf[j],
				(((i + 1) % BYTES_PER_LINE == 0) ? '\n' : ' '));

		}
		dev_dbg(&client->dev, "%s\n", dbg_buf_str);
		dev_dbg(&client->dev, "%s\n", reg_buf_str);
	}

	return 0;
}

static ssize_t attr_reg_dump_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ft5x0x_ts_data *ts_data = dev_get_drvdata(dev);
	struct i2c_client *client = ts_data->client;
	u8 mode = ts_data->op_mode;
	u8 ret;

	ft5x0x_reg_dump(ts_data, DEV_MODE_NORMAL);
	ft5x0x_reg_dump(ts_data, DEV_MODE_SYSTEM);
	ft5x0x_reg_dump(ts_data, DEV_MODE_TEST);

	ret = ft5x0x_set_devmode(ts_data, mode);
	if (ret < 0) {
		dev_err(&client->dev, "reg dump set mode failed\n");
		return ret;
	}

	return 0;
}

static DEVICE_ATTR(reg_dump, S_IRUSR, attr_reg_dump_show, NULL);

static struct attribute *attrs[] = {
	&dev_attr_reg_dump.attr,
	NULL
};

static struct attribute_group debug_attr = {
	.name = "ft_debug",
	.attrs = attrs
};
#endif

static int ft5x0x_ts_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct ft5x0x_ts_data *ts_data;
	struct input_dev *input_dev;
	int err;

	dev_dbg(&client->dev, "probe start\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_dbg(&client->dev, "Invalid I2C function\n");
		return -ENODEV;
	}

	ts_data = devm_kzalloc(&client->dev, sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data) {
		dev_err(&client->dev, "failed to allocate driver data.\n");
		return -ENOMEM;
	}

	input_dev = devm_input_allocate_device(&client->dev);
	if (!input_dev) {
		dev_err(&client->dev, "failed to allocate input device.\n");
		return -ENOMEM;
	}

	mutex_init(&ts_data->lock);

#ifdef CONFIG_PM_SLEEP
	mutex_init(&ts_data->suspend_lock);
#endif

	ts_data->client = client;
	ts_data->input_dev = input_dev;

	if (client->dev.platform_data)
		err = ft5x0x_platform_probe(ts_data);
	else if (ACPI_HANDLE(&client->dev))
		err = ft5x0x_acpi_probe(ts_data);
	else
		err = -ENODEV;

	if (err) {
		dev_err(&client->dev, "no platform resources found\n");
		return err;
	}

	input_dev->name = FT5X0X_NAME;
	input_mt_init_slots(ts_data->input_dev, CFG_MAX_TOUCH_POINTS, 0);
	input_set_abs_params(ts_data->input_dev, ABS_MT_TOUCH_MAJOR,
				0, PRESS_MAX, 0, 0);
	input_set_abs_params(ts_data->input_dev, ABS_MT_POSITION_X,
				0, SCREEN_MAX_X, 0, 0);
	input_set_abs_params(ts_data->input_dev, ABS_MT_POSITION_Y,
				0, SCREEN_MAX_Y, 0, 0);

	__set_bit(EV_ABS, ts_data->input_dev->evbit);
	__set_bit(EV_KEY, ts_data->input_dev->evbit);
	__set_bit(BTN_TOUCH, ts_data->input_dev->keybit);

	input_set_drvdata(input_dev, ts_data);
	i2c_set_clientdata(client, ts_data);

	if (client->irq < 0)
		return -ENODEV;

	err = devm_request_threaded_irq(&client->dev, client->irq,
					  NULL, ft5x0x_irq_handler,
					  IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					  client->name, ts_data);
	if (err) {
		dev_err(&client->dev, "Unable to request touchscreen irq.\n");
		return err;
	}

	disable_irq(client->irq);

	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev, "failed to register input device: %s\n",
			dev_name(&client->dev));
		return err;
	}

#ifdef DEBUG
	err = sysfs_create_group(&client->dev.kobj, &debug_attr);
	if (err < 0) {
		dev_err(&client->dev, "debug sysfs register failed\n");
		goto err_sysfs_create;
	}
#endif
	err = ft5x0x_wake_device(ts_data);

	if (err) {
		dev_err(&client->dev, "enable device failed\n");
		goto err_sysfs_create;
	}

	dev_err(&client->dev, "Doing reg init now\n");

	err = ft5x0x_reg_init(ts_data);

	if (err) {
		dev_err(&client->dev, "reg init failed\n");
		goto err_sysfs_create;
	}

	enable_irq(client->irq);

	dev_dbg(&client->dev, "probe end\n");

#ifdef CONFIG_PM_SLEEP
	err = ft5x0x_ts_power_hal_suspend_init(&client->dev);
	if (err < 0)
		dev_err(&client->dev, "unable to register for power hal");

	ts_data->suspended = false;
#endif

	return 0;

err_sysfs_create:
	input_unregister_device(ts_data->input_dev);
	return err;
}

static int ft5x0x_ts_remove(struct i2c_client *client)
{
	struct ft5x0x_ts_data *ts_data = i2c_get_clientdata(client);

#ifdef DEBUG
	sysfs_remove_group(&client->dev.kobj, &debug_attr);
#endif
	input_unregister_device(ts_data->input_dev);
	i2c_set_clientdata(client, NULL);

#ifdef CONFIG_PM_SLEEP
	ft5x0x_ts_power_hal_suspend_destroy(&client->dev);
#endif

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int ft5x0x_ts_suspend(struct device *dev)
{
	struct ft5x0x_ts_data *tsdata = i2c_get_clientdata(to_i2c_client(dev));
	int ret = 0;

	mutex_lock(&tsdata->suspend_lock);

	if (tsdata->suspended)
		goto out;

	dev_dbg(&tsdata->client->dev, "suspend");

	disable_irq(tsdata->irq);

	ret = ft5x0x_set_pmode(tsdata, PMODE_HIBERNATE);

	/* (ret > 0) indicates number of messages sent.         */
	/* Set ret to 0, so not to confuse device pm functions. */
	if (ret > 0)
		ret = 0;
	if (ret < 0)
		goto out;

	tsdata->suspended = true;

out:
	mutex_unlock(&tsdata->suspend_lock);
	return ret;
}

static int ft5x0x_ts_resume(struct device *dev)
{
	struct ft5x0x_ts_data *tsdata = i2c_get_clientdata(to_i2c_client(dev));
	int ret = 0;

	mutex_lock(&tsdata->suspend_lock);

	if (!tsdata->suspended)
		goto out;

	if (tsdata->power_hal_want_suspend)
		goto out;

	dev_dbg(&tsdata->client->dev, "resume");

	ft5x0x_wake_device(tsdata);

	ret = ft5x0x_set_pmode(tsdata, PMODE_ACTIVE);

	/* (ret > 0) indicates number of messages sent.         */
	/* Set ret to 0, so not to confuse device pm functions. */
	if (ret > 0)
		ret = 0;
	if (ret < 0)
		goto out;

	enable_irq(tsdata->irq);

	tsdata->suspended = false;

out:
	mutex_unlock(&tsdata->suspend_lock);
	return ret;
}

static ssize_t ft5x0x_ts_power_hal_suspend_store(struct device *dev,
						 struct device_attribute *attr,
						 const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ft5x0x_ts_data *ts = i2c_get_clientdata(client);

	if (!strncmp(buf, POWER_HAL_SUSPEND_ON,
		     POWER_HAL_SUSPEND_STATUS_LEN)) {
		if (!ts->suspended)
			ft5x0x_ts_power_hal_suspend(dev);
	} else {
		if (ts->suspended)
			ft5x0x_ts_power_hal_resume(dev);
	}

	return count;
}
static DEVICE_POWER_HAL_SUSPEND_ATTR(ft5x0x_ts_power_hal_suspend_store);

static int ft5x0x_ts_power_hal_suspend_init(struct device *dev)
{
	int ret = 0;

	ret = device_create_file(dev, &dev_attr_power_HAL_suspend);
	if (ret)
	return ret;

	return register_power_hal_suspend_device(dev);
}

static void ft5x0x_ts_power_hal_suspend_destroy(struct device *dev)
{
	device_remove_file(dev, &dev_attr_power_HAL_suspend);
	unregister_power_hal_suspend_device(dev);
}

static void ft5x0x_ts_power_hal_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ft5x0x_ts_data *ts = i2c_get_clientdata(client);

	ts->power_hal_want_suspend = true;

	ft5x0x_ts_suspend(dev);
}

static void ft5x0x_ts_power_hal_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ft5x0x_ts_data *ts = i2c_get_clientdata(client);

	ts->power_hal_want_suspend = false;

	ft5x0x_ts_resume(dev);
}
#endif

static SIMPLE_DEV_PM_OPS(ft5x0x_ts_pm_ops,
			 ft5x0x_ts_suspend, ft5x0x_ts_resume);

static const struct acpi_device_id ft5x0x_acpi_match[] = {
	{"FT05506", 0},
	{"FTTH5506", 0},
	{ },
};
MODULE_DEVICE_TABLE(acpi, ft5x0x_acpi_match);

static const struct i2c_device_id ft5x0x_ts_id[] = {
	{ "ft5x06", 0,},
	{ }
};
MODULE_DEVICE_TABLE(i2c, ft5x0x_ts_id);

static struct i2c_driver ft5x0x_ts_driver = {
	.probe		= ft5x0x_ts_probe,
	.remove		= ft5x0x_ts_remove,
	.id_table	= ft5x0x_ts_id,
	.driver	= {
		.name	= FT5X0X_NAME,
		.owner	= THIS_MODULE,
		.pm	= &ft5x0x_ts_pm_ops,
		.acpi_match_table = ACPI_PTR(ft5x0x_acpi_match),
	},
};
module_i2c_driver(ft5x0x_ts_driver);

MODULE_AUTHOR("<wenfs@Focaltech-systems.com>");
MODULE_DESCRIPTION("FocalTech ft5x0x TouchScreen driver");
MODULE_LICENSE("GPL");
