/* drivers/input/touchscreen/cy8c_tmg_ts.c
 *
 * Copyright (C) 2007-2008 HTC Corporation.
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

#include <linux/cy8c_tmg_ts.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#define CY8C_REG_START_NEW_SCAN 0x0F
#define CY8C_REG_INTR_STATUS    0x3C
#define CY8C_REG_VERSION        0x3E

struct cy8c_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	int use_irq;
	struct hrtimer timer;
	struct work_struct work;
	uint16_t version;
	int (*power) (int on);
	struct early_suspend early_suspend;
};

struct workqueue_struct *cypress_touch_wq;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cy8c_ts_early_suspend(struct early_suspend *h);
static void cy8c_ts_late_resume(struct early_suspend *h);
#endif

uint16_t sample_count, X_mean, Y_mean, first_touch;

static s32 cy8c_read_word_data(struct i2c_client *client,
			       u8 command, uint16_t * data)
{
	s32 ret = i2c_smbus_read_word_data(client, command);
	if (ret != -1) {
		*data = (u16) ((ret << 8) | (ret >> 8));
	}
	return ret;
}

static int cy8c_init_panel(struct cy8c_ts_data *ts)
{
	int ret;
	sample_count = X_mean = Y_mean = first_touch = 0;

	/* clean intr busy */
	ret = i2c_smbus_write_byte_data(ts->client, CY8C_REG_INTR_STATUS,
					0x00);
	if (ret < 0) {
		dev_err(&ts->client->dev,
			"cy8c_init_panel failed for clean intr busy\n");
		goto exit;
	}

	/* start new scan */
	ret = i2c_smbus_write_byte_data(ts->client, CY8C_REG_START_NEW_SCAN,
					0x01);
	if (ret < 0) {
		dev_err(&ts->client->dev,
			"cy8c_init_panel failed for start new scan\n");
		goto exit;
	}

exit:
	return ret;
}

static void cy8c_ts_reset(struct i2c_client *client)
{
	struct cy8c_ts_data *ts = i2c_get_clientdata(client);

	if (ts->power) {
		ts->power(0);
		msleep(10);
		ts->power(1);
		msleep(10);
	}

	cy8c_init_panel(ts);
}

static void cy8c_ts_work_func(struct work_struct *work)
{
	struct cy8c_ts_data *ts = container_of(work, struct cy8c_ts_data, work);
	uint16_t x1, y1, x2, y2;
	uint8_t is_touch, start_reg, force, area, finger2_pressed;
	uint8_t buf[11];
	struct i2c_msg msg[2];
	int ret = 0;

	x2 = y2 = 0;

	/*printk("%s: enter\n",__func__);*/
	is_touch = i2c_smbus_read_byte_data(ts->client, 0x20);
	dev_dbg(&ts->client->dev, "fIsTouch %d,\n", is_touch);
	if (is_touch < 0 || is_touch > 3) {
		pr_err("%s: invalid is_touch = %d\n", __func__, is_touch);
		cy8c_ts_reset(ts->client);
		msleep(10);
		goto done;
	}

	msg[0].addr = ts->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	start_reg = 0x16;
	msg[0].buf = &start_reg;

	msg[1].addr = ts->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = sizeof(buf);
	msg[1].buf = buf;

	ret = i2c_transfer(ts->client->adapter, msg, 2);
	if (ret < 0)
		goto done;

	/* parse data */
	force = buf[0];
	area = buf[1];
	x1 = (buf[2] << 8) | buf[3];
	y1 = (buf[6] << 8) | buf[7];
	is_touch = buf[10];

	if (is_touch == 2) {
		x2 = (buf[4] << 8) | buf[5];
		y2 = (buf[8] << 8) | buf[9];
		finger2_pressed = 1;
	}

	dev_dbg(&ts->client->dev,
		"bFingerForce %d, bFingerArea %d \n", force, area);
	dev_dbg(&ts->client->dev, "x1: %d, y1: %d \n", x1, y1);
	if (finger2_pressed)
		dev_dbg(&ts->client->dev, "x2: %d, y2: %d \n", x2, y2);

	/* drop the first one? */
	if ((is_touch == 1) && (first_touch == 0)) {
		first_touch = 1;
		goto done;
	}

	if (!first_touch)
		goto done;

	if (is_touch == 2)
		finger2_pressed = 1;

	input_report_abs(ts->input_dev, ABS_X, x1);
	input_report_abs(ts->input_dev, ABS_Y, y1);
	input_report_abs(ts->input_dev, ABS_PRESSURE, force);
	input_report_abs(ts->input_dev, ABS_TOOL_WIDTH, area);
	input_report_key(ts->input_dev, BTN_TOUCH, is_touch);
	input_report_key(ts->input_dev, BTN_2, finger2_pressed);

	if (finger2_pressed) {
		input_report_abs(ts->input_dev, ABS_HAT0X, x2);
		input_report_abs(ts->input_dev, ABS_HAT0Y, y2);
	}
	input_sync(ts->input_dev);

done:
	if (is_touch == 0)
		first_touch = sample_count = 0;

	/* prepare for next intr */
	i2c_smbus_write_byte_data(ts->client, CY8C_REG_INTR_STATUS, 0x00);
	if (!ts->use_irq)
		hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	else
		enable_irq(ts->client->irq);
}

static enum hrtimer_restart cy8c_ts_timer_func(struct hrtimer *timer)
{
	struct cy8c_ts_data *ts;

	ts = container_of(timer, struct cy8c_ts_data, timer);
	queue_work(cypress_touch_wq, &ts->work);
	return HRTIMER_NORESTART;
}

static irqreturn_t cy8c_ts_irq_handler(int irq, void *dev_id)
{
	struct cy8c_ts_data *ts = dev_id;

	disable_irq_nosync(ts->client->irq);
	queue_work(cypress_touch_wq, &ts->work);
	return IRQ_HANDLED;
}

static int cy8c_ts_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct cy8c_ts_data *ts;
	struct cy8c_i2c_platform_data *pdata;
	uint16_t panel_version;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "need I2C_FUNC_I2C\n");
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct cy8c_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		dev_err(&client->dev, "allocate cy8c_ts_data failed\n");
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	INIT_WORK(&ts->work, cy8c_ts_work_func);
	ts->client = client;
	i2c_set_clientdata(client, ts);

	pdata = client->dev.platform_data;
	if (pdata) {
		ts->version = pdata->version;
		ts->power = pdata->power;
	}

	if (ts->power) {
		ret = ts->power(1);
		msleep(10);
		if (ret < 0) {
			dev_err(&client->dev, "power on failed\n");
			goto err_power_failed;
		}
	}

	ret = cy8c_read_word_data(ts->client, CY8C_REG_VERSION, &panel_version);
	if (ret < 0) {
		dev_err(&client->dev, "init panel failed\n");
		goto err_detect_failed;
	}
	dev_info(&client->dev, "Panel Version %04X\n", panel_version);
	if (pdata) {
		while (pdata->version > panel_version) {
			dev_info(&client->dev, "old tp detected, "
				 "panel version = %x\n", panel_version);
			pdata++;
		}
	}

	ret = cy8c_init_panel(ts);
	if (ret < 0) {
		dev_err(&client->dev, "init panel failed\n");
		goto err_detect_failed;
	}

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		dev_err(&client->dev, "Failed to allocate input device\n");
		goto err_input_dev_alloc_failed;
	}
	ts->input_dev->name = "cy8c-touchscreen";

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	input_set_capability(ts->input_dev, EV_KEY, BTN_TOUCH);
	input_set_capability(ts->input_dev, EV_KEY, BTN_2);

	input_set_abs_params(ts->input_dev, ABS_X,
			     pdata->abs_x_min, pdata->abs_x_max, 5, 0);
	input_set_abs_params(ts->input_dev, ABS_Y,
			     pdata->abs_y_min, pdata->abs_y_max, 5, 0);
	input_set_abs_params(ts->input_dev, ABS_HAT0X,
			     pdata->abs_x_min, pdata->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_HAT0Y,
			     pdata->abs_y_min, pdata->abs_y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_PRESSURE,
			     pdata->abs_pressure_min, pdata->abs_pressure_max,
			     0, 0);
	input_set_abs_params(ts->input_dev, ABS_TOOL_WIDTH,
			     pdata->abs_width_min, pdata->abs_width_max, 0, 0);

	ret = input_register_device(ts->input_dev);
	if (ret) {
		dev_err(&client->dev,
			"cy8c_ts_probe: Unable to register %s input device\n",
			ts->input_dev->name);
		goto err_input_register_device_failed;
	}

	if (client->irq) {
		ret = request_irq(client->irq, cy8c_ts_irq_handler,
				  IRQF_TRIGGER_LOW, CYPRESS_TMG_NAME, ts);
		if (ret == 0)
			ts->use_irq = 1;
		else
			dev_err(&client->dev, "request_irq failed\n");
	}

	if (!ts->use_irq) {
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = cy8c_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = cy8c_ts_early_suspend;
	ts->early_suspend.resume = cy8c_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	dev_info(&client->dev, "Start touchscreen %s in %s mode\n",
		 ts->input_dev->name, (ts->use_irq ? "interrupt" : "polling"));

	return 0;

err_input_register_device_failed:
	input_free_device(ts->input_dev);

err_input_dev_alloc_failed:
	if (ts->power)
		ts->power(0);

err_detect_failed:
err_power_failed:
	kfree(ts);

err_alloc_data_failed:
err_check_functionality_failed:
	return ret;
}

static int cy8c_ts_remove(struct i2c_client *client)
{
	struct cy8c_ts_data *ts = i2c_get_clientdata(client);

	unregister_early_suspend(&ts->early_suspend);

	if (ts->use_irq)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);

	input_unregister_device(ts->input_dev);
	kfree(ts);

	return 0;
}

static int cy8c_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct cy8c_ts_data *ts = i2c_get_clientdata(client);
	int ret;

	if (ts->use_irq)
		disable_irq_nosync(client->irq);
	else
		hrtimer_cancel(&ts->timer);

	ret = cancel_work_sync(&ts->work);
	if (ret && ts->use_irq)
		enable_irq(client->irq);

	if (ts->power)
		ts->power(0);

	return 0;
}

static int cy8c_ts_resume(struct i2c_client *client)
{
	int ret;
	struct cy8c_ts_data *ts = i2c_get_clientdata(client);

	if (ts->power) {
		ret = ts->power(1);
		if (ret < 0)
			dev_err(&client->dev,
				"cy8c_ts_resume power on failed\n");
		msleep(10);

		cy8c_init_panel(ts);
	}

	if (ts->use_irq)
		enable_irq(client->irq);
	else
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void cy8c_ts_early_suspend(struct early_suspend *h)
{
	struct cy8c_ts_data *ts;
	ts = container_of(h, struct cy8c_ts_data, early_suspend);
	cy8c_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void cy8c_ts_late_resume(struct early_suspend *h)
{
	struct cy8c_ts_data *ts;
	ts = container_of(h, struct cy8c_ts_data, early_suspend);
	cy8c_ts_resume(ts->client);
}
#endif

static const struct i2c_device_id cy8c_ts_i2c_id[] = {
	{CYPRESS_TMG_NAME, 0},
	{}
};

static struct i2c_driver cy8c_ts_driver = {
	.id_table = cy8c_ts_i2c_id,
	.probe = cy8c_ts_probe,
	.remove = cy8c_ts_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend = cy8c_ts_suspend,
	.resume = cy8c_ts_resume,
#endif
	.driver = {
		.name = CYPRESS_TMG_NAME,
		.owner = THIS_MODULE,
	},
};

static int __devinit cy8c_ts_init(void)
{
	cypress_touch_wq = create_singlethread_workqueue("cypress_touch_wq");
	if (!cypress_touch_wq)
		return -ENOMEM;

	return i2c_add_driver(&cy8c_ts_driver);
}

static void __exit cy8c_ts_exit(void)
{
	if (cypress_touch_wq)
		destroy_workqueue(cypress_touch_wq);

	i2c_del_driver(&cy8c_ts_driver);
}

module_init(cy8c_ts_init);
module_exit(cy8c_ts_exit);

MODULE_DESCRIPTION("Cypress TMG Touchscreen Driver");
MODULE_LICENSE("GPL");

