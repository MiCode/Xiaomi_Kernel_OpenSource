/*
 * Himax Android Driver Sample Code for QCT platform
 *
 * Copyright (C) 2018 Himax Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include "himax_platform.h"
#include "himax_common.h"
#include "himax_ic_core.h"
#include "linux/moduleparam.h"

int i2c_error_count;
int irq_enable_count;

active_tp_setup(himax);

int himax_dev_set(struct himax_ts_data *ts)
{
	int ret = 0;

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		E("%s: Failed to allocate input device\n", __func__);
		return ret;
	}

	ts->input_dev->name = "himax-touchscreen";
	return ret;
}

int himax_input_register_device(struct input_dev *input_dev)
{
	return input_register_device(input_dev);
}

#if defined(HX_PLATFOME_DEFINE_KEY)
void himax_platform_key(void)
{
	I("Nothing to be done! Plz cancel it!\n");
}
#endif

void himax_vk_parser(struct device_node *dt,
			struct himax_i2c_platform_data *pdata)
{
	u32 data = 0;
	uint8_t cnt = 0, i = 0;
	uint32_t coords[4] = {0};
	struct device_node *node, *pp = NULL;
	struct himax_virtual_key *vk;

	node = of_parse_phandle(dt, "virtualkey", 0);
	if (node == NULL) {
		D(" DT-No vk info in DT");
		return;
	}

	cnt = of_get_child_count(node);
	if (!cnt) {
		of_node_put(node);
		return;
	}

	vk = kcalloc(cnt, sizeof(*vk), GFP_KERNEL);
	if (!vk) {
		E(" %s: allocate memory failed!", __func__);
		of_node_put(node);
		return;
	}

	for_each_child_of_node(node, pp) {
		if (of_property_read_u32(pp, "idx", &data) != 0)
			continue;
		vk[i].index = data;

		if (of_property_read_u32_array(pp, "range", coords, 4) != 0)
			continue;
		vk[i].x_range_min = coords[0], vk[i].x_range_max = coords[1];
		vk[i].y_range_min = coords[2], vk[i].y_range_max = coords[3];

		i++;
	}

	pdata->virtual_key = vk;
	of_node_put(node);

	for (i = 0; i < cnt; i++)
		I(" vk[%d] idx:%d x_min:%d, y_max:%d", i, pdata->virtual_key[i].index,
		  pdata->virtual_key[i].x_range_min, pdata->virtual_key[i].y_range_max);
}

int himax_parse_dt(struct himax_ts_data *ts, struct himax_i2c_platform_data *pdata)
{
	int rc, coords_size = 0;
	uint32_t coords[4] = {0};
	struct property *prop;
	struct device_node *dt = private_ts->client->dev.of_node;
	u32 data = 0;

	prop = of_find_property(dt, "himax,panel-coords", NULL);
	if (prop) {
		coords_size = prop->length / sizeof(u32);

		if (coords_size != 4)
			D(" %s:Invalid panel coords size %d", __func__, coords_size);
	}

	if (of_property_read_u32_array(dt, "himax,panel-coords", coords, coords_size) == 0) {
		pdata->abs_x_min = coords[0], pdata->abs_x_max = coords[1];
		pdata->abs_y_min = coords[2], pdata->abs_y_max = coords[3];
		D(" DT-%s:panel-coords = %d, %d, %d, %d\n", __func__,
			pdata->abs_x_min, pdata->abs_x_max,
			pdata->abs_y_min, pdata->abs_y_max);
	}

	prop = of_find_property(dt, "himax,display-coords", NULL);

	if (prop) {
		coords_size = prop->length / sizeof(u32);

		if (coords_size != 4)
			D(" %s:Invalid display coords size %d", __func__, coords_size);
	}

	rc = of_property_read_u32_array(dt, "himax,display-coords", coords, coords_size);

	if (rc && (rc != -EINVAL)) {
		D(" %s:Fail to read display-coords %d\n", __func__, rc);
		return rc;
	}

	pdata->screenWidth  = coords[1];
	pdata->screenHeight = coords[3];
	D(" DT-%s:display-coords = (%d, %d)", __func__, pdata->screenWidth,
		pdata->screenHeight);
	pdata->gpio_irq = of_get_named_gpio(dt, "himax,irq-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_irq))
		E(" DT:gpio_irq value is not valid\n");

	pdata->gpio_reset = of_get_named_gpio(dt, "himax,rst-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_reset))
		E(" DT:gpio_rst value is not valid\n");

	pdata->gpio_3v3_en = of_get_named_gpio(dt, "himax,3v3-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_3v3_en))
		D(" DT:gpio_3v3_en value is not valid\n");

	D(" DT:gpio_irq=%d, gpio_rst=%d, gpio_3v3_en=%d",
		pdata->gpio_irq, pdata->gpio_reset, pdata->gpio_3v3_en);

	if (of_property_read_u32(dt, "himax,report_type", &data) == 0) {
		pdata->protocol_type = data;
		I(" DT:protocol_type=%d", pdata->protocol_type);
	}

	himax_vk_parser(dt, pdata);
	return 0;
}

int himax_bus_read(uint8_t command, uint8_t *data, uint32_t length, uint8_t toRetry)
{
	int retry;
	bool reallocate = false;
	struct himax_ts_data *ts = private_ts;
	uint8_t *buf = ts->i2c_data;
	struct i2c_client *client = ts->client;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = HX_CMD_BYTE,
			.buf = buf,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = buf + HX_CMD_BYTE,
		}
	};

	if (length > FLASH_RW_MAX_LEN) {
		W("%s: data length too large %d!\n", __func__, length);
		buf = kmalloc(length + HX_CMD_BYTE, GFP_KERNEL);
		if (!buf) {
			E("%s: failed realloc buf %d\n", __func__,
							length + HX_CMD_BYTE);
			return -EIO;
		}
		reallocate = true;
		msg[0].buf = buf;
		msg[1].buf = buf + HX_CMD_BYTE;
	}

	mutex_lock(&ts->rw_lock);
	buf[0] = command;

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 2) == 2)
			break;

		msleep(20);
	}

	if (retry == toRetry) {
		E("%s: i2c_read_block retry over %d\n", __func__, toRetry);
		i2c_error_count = toRetry;
		mutex_unlock(&ts->rw_lock);
		return -EIO;
	}

	memcpy(data, buf + HX_CMD_BYTE, length);
	mutex_unlock(&ts->rw_lock);

	if (reallocate)
		kfree(buf);

	return 0;
}

int himax_bus_write(uint8_t command, uint8_t *data, uint32_t length, uint8_t toRetry)
{
	int retry;
	bool reallocate = false;
	struct himax_ts_data *ts = private_ts;
	uint8_t *buf = ts->i2c_data;
	struct i2c_client *client = ts->client;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length + HX_CMD_BYTE,
			.buf = buf,
		}
	};

	if (length > FLASH_RW_MAX_LEN) {
		W("%s: data length too large %d!\n", __func__, length);
		buf = kmalloc(length + HX_CMD_BYTE, GFP_KERNEL);
		if (!buf) {
			E("%s: failed realloc buf %d\n", __func__,
							length + HX_CMD_BYTE);
			return -EIO;
		}
		reallocate = true;
		msg[0].buf = buf;
	}

	mutex_lock(&ts->rw_lock);
	buf[0] = command;
	memcpy(buf + HX_CMD_BYTE, data, length);

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;

		msleep(20);
	}

	if (retry == toRetry) {
		E("%s: i2c_write_block retry over %d\n", __func__, toRetry);
		i2c_error_count = toRetry;
		mutex_unlock(&ts->rw_lock);
		return -EIO;
	}

	mutex_unlock(&ts->rw_lock);

	if (reallocate)
		kfree(buf);

	return 0;
}

int himax_bus_write_command(uint8_t command, uint8_t toRetry)
{
	return himax_bus_write(command, NULL, 0, toRetry);
}

int himax_bus_master_write(uint8_t *data, uint32_t length, uint8_t toRetry)
{
	int retry;
	bool reallocate = false;
	struct himax_ts_data *ts = private_ts;
	uint8_t *buf = ts->i2c_data;
	struct i2c_client *client = private_ts->client;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length,
			.buf = buf,
		}
	};

	if (length > FLASH_RW_MAX_LEN) {
		W("%s: data length too large %d!\n", __func__, length);
		buf = kmalloc(length, GFP_KERNEL);
		if (!buf) {
			E("%s: failed realloc buf %d\n", __func__, length);
			return -EIO;
		}
		reallocate = true;
		msg[0].buf = buf;
	}

	mutex_lock(&ts->rw_lock);
	memcpy(buf, data, length);

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;

		msleep(20);
	}

	if (retry == toRetry) {
		E("%s: i2c_write_block retry over %d\n", __func__, toRetry);
		i2c_error_count = toRetry;
		mutex_unlock(&ts->rw_lock);
		return -EIO;
	}

	mutex_unlock(&ts->rw_lock);

	if (reallocate)
		kfree(buf);

	return 0;
}

void himax_int_enable(int enable)
{
	int irqnum = 0;

	irqnum = private_ts->client->irq;

	if (enable == 1 && irq_enable_count == 0) {
		enable_irq(irqnum);
		irq_enable_count++;
		private_ts->irq_enabled = 1;
	} else if (enable == 0 && irq_enable_count == 1) {
		disable_irq_nosync(irqnum);
		irq_enable_count--;
		private_ts->irq_enabled = 0;
	}

	D("irq_enable_count = %d", irq_enable_count);
}

#ifdef HX_RST_PIN_FUNC
void himax_rst_gpio_set(int pinnum, uint8_t value)
{
	gpio_direction_output(pinnum, value);
}
#endif

uint8_t himax_int_gpio_read(int pinnum)
{
	return gpio_get_value(pinnum);
}

#if defined(CONFIG_HMX_DB)
static int himax_regulator_configure(struct himax_i2c_platform_data *pdata)
{
	int retval;
	struct i2c_client *client = private_ts->client;

	pdata->vcc_dig = regulator_get(&client->dev, "vdd");
	if (IS_ERR(pdata->vcc_dig)) {
		E("%s: Failed to get regulator vdd\n", __func__);
		retval = PTR_ERR(pdata->vcc_dig);
		return retval;
	}

	pdata->vcc_ana = regulator_get(&client->dev, "avdd");

	if (IS_ERR(pdata->vcc_ana)) {
		E("%s: Failed to get regulator avdd\n", __func__);
		retval = PTR_ERR(pdata->vcc_ana);
		regulator_put(pdata->vcc_ana);
		return retval;
	}

	return 0;
};

static int himax_power_on(struct himax_i2c_platform_data *pdata, bool on)
{
	int retval;

	if (on) {
		retval = regulator_enable(pdata->vcc_dig);

		if (retval) {
			E("%s: Failed to enable regulator vdd\n", __func__);
			return retval;
		}

		msleep(100);
		retval = regulator_enable(pdata->vcc_ana);

		if (retval) {
			E("%s: Failed to enable regulator avdd\n", __func__);
			regulator_disable(pdata->vcc_dig);
			return retval;
		}
	} else {
		regulator_disable(pdata->vcc_dig);
		regulator_disable(pdata->vcc_ana);
	}

	return 0;
}

int himax_gpio_power_config(struct himax_i2c_platform_data *pdata)
{
	int error;
	struct i2c_client *client = private_ts->client;

	error = himax_regulator_configure(pdata);
	if (error) {
		E("Failed to initialize hardware\n");
		goto err_regulator_not_on;
	}

#ifdef HX_RST_PIN_FUNC

	if (gpio_is_valid(pdata->gpio_reset)) {
		/* configure touchscreen reset out gpio */
		error = gpio_request(pdata->gpio_reset, "hmx_reset_gpio");

		if (error) {
			E("unable to request gpio [%d]\n", pdata->gpio_reset);
			goto err_regulator_on;
		}

		error = gpio_direction_output(pdata->gpio_reset, 0);

		if (error) {
			E("unable to set direction for gpio [%d]\n", pdata->gpio_reset);
			goto err_gpio_reset_req;
		}
	}

#endif
	error = himax_power_on(pdata, true);

	if (error) {
		E("Failed to power on hardware\n");
		goto err_gpio_reset_req;
	}

	if (gpio_is_valid(pdata->gpio_irq)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->gpio_irq, "hmx_gpio_irq");

		if (error) {
			E("unable to request gpio [%d]\n", pdata->gpio_irq);
			goto err_power_on;
		}

		error = gpio_direction_input(pdata->gpio_irq);

		if (error) {
			E("unable to set direction for gpio [%d]\n", pdata->gpio_irq);
			goto err_gpio_irq_req;
		}

		client->irq = gpio_to_irq(pdata->gpio_irq);
	} else {
		E("irq gpio not provided\n");
		goto err_power_on;
	}

	msleep(20);
#ifdef HX_RST_PIN_FUNC

	if (gpio_is_valid(pdata->gpio_reset)) {
		error = gpio_direction_output(pdata->gpio_reset, 1);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
				pdata->gpio_reset);
			goto err_gpio_irq_req;
		}
	}

#endif
	return 0;
err_gpio_irq_req:

	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);

err_power_on:
	himax_power_on(pdata, false);
err_gpio_reset_req:
#ifdef HX_RST_PIN_FUNC

	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);

err_regulator_on:
#endif
err_regulator_not_on:
	return error;
}

#else
int himax_gpio_power_config(struct himax_i2c_platform_data *pdata)
{
	int error = 0;
	struct i2c_client *client = private_ts->client;
#ifdef HX_RST_PIN_FUNC

	if (pdata->gpio_reset >= 0) {
		error = gpio_request(pdata->gpio_reset, "himax-reset");

		if (error < 0) {
			E("%s: request reset pin failed\n", __func__);
			return error;
		}

		error = gpio_direction_output(pdata->gpio_reset, 0);

		if (error) {
			E("unable to set direction for gpio [%d]\n", pdata->gpio_reset);
			return error;
		}
	}

#endif

	if (pdata->gpio_3v3_en >= 0) {
		error = gpio_request(pdata->gpio_3v3_en, "himax-3v3_en");

		if (error < 0) {
			E("%s: request 3v3_en pin failed\n", __func__);
			return error;
		}

		gpio_direction_output(pdata->gpio_3v3_en, 1);
		I("3v3_en pin =%d\n", gpio_get_value(pdata->gpio_3v3_en));
	}

	if (gpio_is_valid(pdata->gpio_irq)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->gpio_irq, "himax_gpio_irq");

		if (error) {
			E("unable to request gpio [%d]\n", pdata->gpio_irq);
			return error;
		}

		error = gpio_direction_input(pdata->gpio_irq);

		if (error) {
			E("unable to set direction for gpio [%d]\n", pdata->gpio_irq);
			return error;
		}

		client->irq = gpio_to_irq(pdata->gpio_irq);
	} else {
		E("irq gpio not provided\n");
		return error;
	}

	msleep(20);
#ifdef HX_RST_PIN_FUNC

	if (pdata->gpio_reset >= 0) {
		error = gpio_direction_output(pdata->gpio_reset, 1);

		if (error) {
			E("unable to set direction for gpio [%d]\n", pdata->gpio_reset);
			return error;
		}
	}

#endif
	return error;
}

#endif

static void himax_ts_isr_func(struct himax_ts_data *ts)
{
	himax_ts_work(ts);
}

irqreturn_t himax_ts_thread(int irq, void *ptr)
{
	himax_ts_isr_func((struct himax_ts_data *)ptr);

	return IRQ_HANDLED;
}

static void himax_ts_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts;

	ts = container_of(work, struct himax_ts_data, work);
	himax_ts_work(ts);
}

int himax_int_register_trigger(void)
{
	int ret = 0;
	struct himax_ts_data *ts = private_ts;
	struct i2c_client *client = private_ts->client;

	if (ic_data->HX_INT_IS_EDGE) {
		D("%s edge triiger falling\n ", __func__);
		ret = request_threaded_irq(client->irq, NULL, himax_ts_thread, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->name, ts);
	} else {
		D("%s level trigger low\n ", __func__);
		ret = request_threaded_irq(client->irq, NULL, himax_ts_thread, IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->name, ts);
	}

	return ret;
}

int himax_int_en_set(void)
{
	int ret = NO_ERR;

	ret = himax_int_register_trigger();
	return ret;
}

int himax_ts_register_interrupt(void)
{
	struct himax_ts_data *ts = private_ts;
	struct i2c_client *client = private_ts->client;
	int ret = 0;

	ts->irq_enabled = 0;

	/* Work functon */
	if (client->irq) {/* INT mode */
		ts->use_irq = 1;
		ret = himax_int_register_trigger();

		if (ret == 0) {
			ts->irq_enabled = 1;
			irq_enable_count = 1;
			D("%s: irq enabled at qpio: %d\n",
				__func__, client->irq);
#ifdef HX_SMART_WAKEUP
			irq_set_irq_wake(client->irq, 1);
#endif
		} else {
			ts->use_irq = 0;
			E("%s: request_irq failed\n", __func__);
		}
	} else
		I("%s: client->irq is empty, use polling mode.\n", __func__);

	/* if use polling mode need to disable HX_ESD_RECOVERY function */
	if (!ts->use_irq) {
		ts->himax_wq = create_singlethread_workqueue("himax_touch");
		INIT_WORK(&ts->work, himax_ts_work_func);
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = himax_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		I("%s: polling mode enabled\n", __func__);
	}

	return ret;
}

static int himax_common_suspend(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	D("%s: enter\n", __func__);
	if (!ts->initialized)
		return -ECANCELED;
	himax_chip_common_suspend(ts);
	return 0;
}

static int himax_common_resume(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	D("%s: enter\n", __func__);
	if (!ts->initialized) {
		/*
		 * wait until device resume for TDDI
		 * TDDI: Touch and display Driver IC
		 */
		if (himax_chip_common_init())
			return -ECANCELED;
		ts->initialized = true;
	}
	himax_chip_common_resume(ts);
	return 0;
}


#if defined(CONFIG_DRM)

int drm_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct msm_drm_notifier *evdata = data;
	int *blank;
	struct himax_ts_data *ts =
		container_of(self, struct himax_ts_data, fb_notif);

	if (!evdata || (evdata->id != 0))
		return 0;

	D("DRM  %s\n", __func__);

	if (evdata->data && event == MSM_DRM_EARLY_EVENT_BLANK && ts &&
							ts->client) {
		blank = evdata->data;
		switch (*blank) {
		case MSM_DRM_BLANK_POWERDOWN:
			if (!ts->initialized)
				return -ECANCELED;
			himax_common_suspend(&ts->client->dev);
			break;
		}
	}

	if (evdata->data && event == MSM_DRM_EVENT_BLANK && ts && ts->client) {
		blank = evdata->data;
		switch (*blank) {
		case MSM_DRM_BLANK_UNBLANK:
			himax_common_resume(&ts->client->dev);
			break;
		}
	}

	return 0;
}

#elif defined(CONFIG_FB)

int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct himax_ts_data *ts =
		container_of(self, struct himax_ts_data, fb_notif);

	D("FB  %s\n", __func__);

	if (evdata && evdata->data && event == FB_EVENT_BLANK && ts &&
		ts->client) {
		blank = evdata->data;

		switch (*blank) {
		case FB_BLANK_UNBLANK:
			himax_common_resume(&ts->client->dev);
			break;
		case FB_BLANK_POWERDOWN:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
			himax_common_suspend(&ts->client->dev);
			break;
		}
	}

	return 0;
}
#endif

int himax_chip_common_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct himax_ts_data *ts;
	struct device_node *dt = client->dev.of_node;
	struct himax_i2c_platform_data *pdata;

	D("%s:Enter\n", __func__);

	if (himax_check_assigned_tp(dt, "compatible",
		"qcom,i2c-touch-active") < 0)
		goto err_dt_not_match;
	/* Check I2C functionality */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		E("%s: i2c check functionality error\n", __func__);
		ret = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		E("%s: allocate himax_ts_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	i2c_set_clientdata(client, ts);
	ts->client = client;
	ts->dev = &client->dev;
	mutex_init(&ts->rw_lock);
	private_ts = ts;

	ts->i2c_data = kmalloc(FLASH_RW_MAX_LEN + HX_CMD_BYTE, GFP_KERNEL);
	if (ts->i2c_data == NULL) {
		E("%s: allocate i2c_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_i2c_data;
	}

	/*
	 * ts chip initialization is deferred till FB_UNBLACK event;
	 * probe is considered pending till then.
	 */
	ts->initialized = false;
#if defined(CONFIG_FB) || defined(CONFIG_DRM)
	ret = himax_fb_register(ts);
	if (ret)
		goto err_fb_notify_reg_failed;
#endif

#ifdef HX_AUTO_UPDATE_FW
	ts->himax_update_wq =
		create_singlethread_workqueue("HMX_update_request");
	if (!ts->himax_update_wq) {
		E(" allocate syn_update_wq failed\n");
		goto err_fb_notify_reg_failed;
	}
	INIT_DELAYED_WORK(&ts->work_update, himax_update_register);
#endif

	D("PDATA START\n");
	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) { /* Allocate Platform data space */
		ret = -ENOMEM;
		goto err_fb_notify_reg_failed;
	}

	if (himax_parse_dt(ts, pdata) < 0) {
		E(" pdata is NULL for DT\n");
		ret = -ECANCELED;
		goto err_alloc_dt_pdata_failed;
	}

	if (pdata->virtual_key)
		ts->button = pdata->virtual_key;

	ts->pdata = pdata;

#ifdef CONFIG_OF
	pdata->cable_config[0] = 0xF0;
	pdata->cable_config[1] = 0x00;

	ts->pdata->abs_pressure_min = 0;
	ts->pdata->abs_pressure_max = 200;
	ts->pdata->abs_width_min = 0;
	ts->pdata->abs_width_max = 200;
#endif

	ts->suspended = false;
#if defined(HX_USB_DETECT_CALLBACK) || defined(HX_USB_DETECT_GLOBAL)
	ts->usb_connected = 0x00;
	ts->cable_config = pdata->cable_config;
#endif
#ifdef	HX_PROTOCOL_A
	ts->protocol_type = PROTOCOL_TYPE_A;
#else
	ts->protocol_type = PROTOCOL_TYPE_B;
#endif
	D("%s: Use Protocol Type %c\n", __func__,
	  ts->protocol_type == PROTOCOL_TYPE_A ? 'A' : 'B');

	ret = himax_input_register(ts);
	if (ret) {
		E("%s: Unable to register %s input device\n",
		  __func__, ts->input_dev->name);
	}
	return ret;

err_alloc_dt_pdata_failed:
	kfree(pdata);
err_fb_notify_reg_failed:
	kfree(ts->i2c_data);
err_alloc_i2c_data:
	kfree(ts);
err_alloc_data_failed:
err_check_functionality_failed:
err_dt_not_match:

	return ret;
}

int himax_chip_common_remove(struct i2c_client *client)
{
	himax_chip_common_deinit();
	return 0;
}

static const struct i2c_device_id himax_common_ts_id[] = {
	{HIMAX_common_NAME, 0 },
	{}
};

static const struct dev_pm_ops himax_common_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_DRM))
	.suspend = himax_common_suspend,
	.resume  = himax_common_resume,
#endif
};

#ifdef CONFIG_OF
static const struct of_device_id himax_match_table[] = {
	{.compatible = "himax,hxcommon" },
	{},
};
#else
#define himax_match_table NULL
#endif

static struct i2c_driver himax_common_driver = {
	.id_table = himax_common_ts_id,
	.probe = himax_chip_common_probe,
	.remove = himax_chip_common_remove,
	.driver	= {
	.name = HIMAX_common_NAME,
	.owner = THIS_MODULE,
	.of_match_table = himax_match_table,
#ifdef CONFIG_PM
	.pm = &himax_common_pm_ops,
#endif
	},
};

static int __init himax_common_init(void)
{
	D("Himax common touch panel driver init\n");
	i2c_add_driver(&himax_common_driver);

	return 0;
}

static void __exit himax_common_exit(void)
{
	i2c_del_driver(&himax_common_driver);
}

module_init(himax_common_init);
module_exit(himax_common_exit);

MODULE_DESCRIPTION("Himax_common driver");
MODULE_LICENSE("GPL v2");

