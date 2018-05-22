/* Himax Android Driver Sample Code for HIMAX chipset
*
* Copyright (C) 2015 Himax Corporation.
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

#include "himax_platform.h"
#include "himax_common.h"

int irq_enable_count = 0;
#ifdef HX_SMART_WAKEUP
#define TS_WAKE_LOCK_TIMEOUT		(2 * HZ)
#endif

#define PINCTRL_STATE_ACTIVE	"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND	"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE	"pmx_ts_release"

extern struct himax_ic_data* ic_data;
extern void himax_ts_work(struct himax_ts_data *ts);
extern enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer);
extern int himax_ts_init(struct himax_ts_data *ts);

extern int tp_rst_gpio;

#ifdef HX_TP_PROC_DIAG
extern uint8_t getDiagCommand(void);
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
		I(" DT-No vk info in DT");
		return;
	} else {
		while ((pp = of_get_next_child(node, pp)))
			cnt++;
		if (!cnt)
			return;

		vk = kzalloc(cnt * (sizeof *vk), GFP_KERNEL);
		if (!vk)
			return;
		pp = NULL;
		while ((pp = of_get_next_child(node, pp))) {
			if (of_property_read_u32(pp, "idx", &data) == 0)
				vk[i].index = data;
			if (of_property_read_u32_array(pp, "range", coords, 4) == 0) {
				vk[i].x_range_min = coords[0], vk[i].x_range_max = coords[1];
				vk[i].y_range_min = coords[2], vk[i].y_range_max = coords[3];
			} else
				I(" range faile");
			i++;
		}
		pdata->virtual_key = vk;
		for (i = 0; i < cnt; i++)
			I(" vk[%d] idx:%d x_min:%d, y_max:%d", i,pdata->virtual_key[i].index,
				pdata->virtual_key[i].x_range_min, pdata->virtual_key[i].y_range_max);
	}
}

int himax_parse_dt(struct himax_ts_data *ts,
				struct himax_i2c_platform_data *pdata)
{
	int rc, coords_size = 0;
	uint32_t coords[4] = {0};
	struct property *prop;
	struct device_node *dt = ts->client->dev.of_node;
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
		I(" DT-%s:panel-coords = %d, %d, %d, %d\n", __func__, pdata->abs_x_min,
				pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);
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
	I(" DT-%s:display-coords = (%d, %d)", __func__, pdata->screenWidth,
		pdata->screenHeight);

	pdata->gpio_irq = of_get_named_gpio(dt, "himax,irq-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_irq)) {
		I(" DT:gpio_irq value is not valid\n");
	}

	pdata->gpio_reset = of_get_named_gpio(dt, "himax,rst-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_reset)) {
		I(" DT:gpio_rst value is not valid\n");
	}
	pdata->gpio_3v3_en = of_get_named_gpio(dt, "himax,3v3-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_3v3_en)) {
		I(" DT:gpio_3v3_en value is not valid\n");
	}
	I(" DT:gpio_irq=%d, gpio_rst=%d, gpio_3v3_en=%d", pdata->gpio_irq, pdata->gpio_reset, pdata->gpio_3v3_en);

	if (of_property_read_u32(dt, "report_type", &data) == 0) {
		pdata->protocol_type = data;
		I(" DT:protocol_type=%d", pdata->protocol_type);
	}

	himax_vk_parser(dt, pdata);

	return 0;
}

int i2c_himax_read(struct i2c_client *client, uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int retry;
	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 1,
			.buf = &command,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = length,
			.buf = data,
		}
	};

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 2) == 2)
			break;
		msleep(20);
	}
	if (retry == toRetry) {
		E("%s: i2c_read_block retry over %d\n",
			__func__, toRetry);
		return -EIO;
	}
	return 0;

}

int i2c_himax_write(struct i2c_client *client, uint8_t command, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int retry/*, loop_i*/;
	uint8_t buf[length + 1];

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length + 1,
			.buf = buf,
		}
	};

	buf[0] = command;
	memcpy(buf+1, data, length);
	
	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(20);
	}

	if (retry == toRetry) {
		E("%s: i2c_write_block retry over %d\n",
			__func__, toRetry);
		return -EIO;
	}
	return 0;

}

int i2c_himax_read_command(struct i2c_client *client, uint8_t length, uint8_t *data, uint8_t *readlength, uint8_t toRetry)
{
	int retry;
	struct i2c_msg msg[] = {
		{
		.addr = client->addr,
		.flags = I2C_M_RD,
		.len = length,
		.buf = data,
		}
	};

	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(20);
	}
	if (retry == toRetry) {
		E("%s: i2c_read_block retry over %d\n",
		       __func__, toRetry);
		return -EIO;
	}
	return 0;
}

int i2c_himax_write_command(struct i2c_client *client, uint8_t command, uint8_t toRetry)
{
	return i2c_himax_write(client, command, NULL, 0, toRetry);
}

int i2c_himax_master_write(struct i2c_client *client, uint8_t *data, uint8_t length, uint8_t toRetry)
{
	int retry/*, loop_i*/;
	uint8_t buf[length];

	struct i2c_msg msg[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = length,
			.buf = buf,
		}
	};

	memcpy(buf, data, length);
	
	for (retry = 0; retry < toRetry; retry++) {
		if (i2c_transfer(client->adapter, msg, 1) == 1)
			break;
		msleep(20);
	}

	if (retry == toRetry) {
		E("%s: i2c_write_block retry over %d\n",
		       __func__, toRetry);
		return -EIO;
	}
	return 0;
}

void himax_int_enable(int irqnum, int enable)
{
	if (enable == 1 && irq_enable_count == 0) {
		enable_irq(irqnum);
		irq_enable_count++;
	} else if (enable == 0 && irq_enable_count == 1) {
		disable_irq_nosync(irqnum);
		irq_enable_count--;
	}
	I("irq_enable_count = %d", irq_enable_count);
}

void himax_rst_gpio_set(int pinnum, uint8_t value)
{
	gpio_direction_output(pinnum, value);
}

uint8_t himax_int_gpio_read(int pinnum)
{
	return gpio_get_value(pinnum);
}

#if defined(CONFIG_HMX_DB)
static int himax_regulator_configure(struct i2c_client *client,struct himax_i2c_platform_data *pdata)
{
    int retval;
    pdata->vcc_dig = regulator_get(&client->dev,
                                   "vdd");
    if (IS_ERR(pdata->vcc_dig))
    {
        E("%s: Failed to get regulator vdd\n",
          __func__);
        retval = PTR_ERR(pdata->vcc_dig);
        return retval;
    }
    pdata->vcc_ana = regulator_get(&client->dev,
                                   "avdd");
    if (IS_ERR(pdata->vcc_ana))
    {
        E("%s: Failed to get regulator avdd\n",
          __func__);
        retval = PTR_ERR(pdata->vcc_ana);
        regulator_put(pdata->vcc_ana);
        return retval;
    }

    return 0;
};

static int himax_power_on(struct himax_i2c_platform_data *pdata, bool on)
{
    int retval;

    if (on)
    {
        retval = regulator_enable(pdata->vcc_dig);
        if (retval)
        {
            E("%s: Failed to enable regulator vdd\n",
              __func__);
            return retval;
        }
        msleep(100);
        retval = regulator_enable(pdata->vcc_ana);
        if (retval)
        {
            E("%s: Failed to enable regulator avdd\n",
              __func__);
            regulator_disable(pdata->vcc_dig);
            return retval;
        }
    }
    else
    {
        regulator_disable(pdata->vcc_dig);
        regulator_disable(pdata->vcc_ana);
    }

    return 0;
}

int himax_ts_pinctrl_init(struct himax_ts_data *ts)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	ts->ts_pinctrl = devm_pinctrl_get(&(ts->client->dev));
	if (IS_ERR_OR_NULL(ts->ts_pinctrl)) {
		retval = PTR_ERR(ts->ts_pinctrl);
		dev_dbg(&ts->client->dev,
			"Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	ts->pinctrl_state_active
		= pinctrl_lookup_state(ts->ts_pinctrl,
				PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(ts->pinctrl_state_active)) {
		retval = PTR_ERR(ts->pinctrl_state_active);
		dev_err(&ts->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	ts->pinctrl_state_suspend
		= pinctrl_lookup_state(ts->ts_pinctrl,
			PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(ts->pinctrl_state_suspend)) {
		retval = PTR_ERR(ts->pinctrl_state_suspend);
		dev_err(&ts->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	ts->pinctrl_state_release
		= pinctrl_lookup_state(ts->ts_pinctrl,
			PINCTRL_STATE_RELEASE);
	if (IS_ERR_OR_NULL(ts->pinctrl_state_release)) {
		retval = PTR_ERR(ts->pinctrl_state_release);
		dev_dbg(&ts->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_RELEASE, retval);
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(ts->ts_pinctrl);
err_pinctrl_get:
	ts->ts_pinctrl = NULL;
	return retval;
}

int himax_gpio_power_config(struct i2c_client *client,struct himax_i2c_platform_data *pdata)
{
    int error;

    error = himax_regulator_configure(client, pdata);
    if (error)
    {
        E("Failed to intialize hardware\n");
        goto err_regulator_not_on;
    }

#ifdef HX_RST_PIN_FUNC
    if (gpio_is_valid(pdata->gpio_reset))
    {
        /* configure touchscreen reset out gpio */
        error = gpio_request(pdata->gpio_reset, "hmx_reset_gpio");
        if (error)
        {
            E("unable to request gpio [%d]\n",
              pdata->gpio_reset);
            goto err_regulator_on;
        }

        error = gpio_direction_output(pdata->gpio_reset, 0);
        if (error)
        {
            E("unable to set direction for gpio [%d]\n",
              pdata->gpio_reset);
            goto err_gpio_reset_req;
        }
    }
#endif

    error = himax_power_on(pdata, true);
    if (error)
    {
        E("Failed to power on hardware\n");
        goto err_gpio_reset_req;
    }
#ifdef HX_IRQ_PIN_FUNC
    if (gpio_is_valid(pdata->gpio_irq))
    {
        /* configure touchscreen irq gpio */
        error = gpio_request(pdata->gpio_irq, "hmx_gpio_irq");
        if (error)
        {
            E("unable to request gpio [%d]\n",
              pdata->gpio_irq);
            goto err_power_on;
        }
        error = gpio_direction_input(pdata->gpio_irq);
        if (error)
        {
            E("unable to set direction for gpio [%d]\n",
              pdata->gpio_irq);
            goto err_gpio_irq_req;
        }
        client->irq = gpio_to_irq(pdata->gpio_irq);
    }
    else
    {
        E("irq gpio not provided\n");
        goto err_power_on;
    }
#endif

    msleep(20);

#ifdef HX_RST_PIN_FUNC
    if (gpio_is_valid(pdata->gpio_reset))
    {
        error = gpio_direction_output(pdata->gpio_reset, 1);
        if (error)
        {
            E("unable to set direction for gpio [%d]\n",
              pdata->gpio_reset);
            goto err_gpio_irq_req;
        }
    }
#endif
    return 0;
#ifdef HX_RST_PIN_FUNC
	err_gpio_irq_req:
#endif
#ifdef HX_IRQ_PIN_FUNC
    if (gpio_is_valid(pdata->gpio_irq))
        gpio_free(pdata->gpio_irq);
	err_power_on:
#endif
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
int himax_gpio_power_config(struct i2c_client *client,struct himax_i2c_platform_data *pdata)
{
		int error=0;
	
#ifdef HX_RST_PIN_FUNC
		if (pdata->gpio_reset >= 0)
		{
			error = gpio_request(pdata->gpio_reset, "himax-reset");
			if (error < 0)
			{
				E("%s: request reset pin failed\n", __func__);
				return error;
			}
			error = gpio_direction_output(pdata->gpio_reset, 0);
			if (error)
			{
				E("unable to set direction for gpio [%d]\n",
				  pdata->gpio_reset);
				return error;
			}
		}
#endif
		if (pdata->gpio_3v3_en >= 0)
		{
			error = gpio_request(pdata->gpio_3v3_en, "himax-3v3_en");
			if (error < 0)
			{
				E("%s: request 3v3_en pin failed\n", __func__);
				return error;
			}
			gpio_direction_output(pdata->gpio_3v3_en, 1);
			I("3v3_en pin =%d\n", gpio_get_value(pdata->gpio_3v3_en));
		}

#ifdef HX_IRQ_PIN_FUNC
		if (gpio_is_valid(pdata->gpio_irq))
		{
			/* configure touchscreen irq gpio */
			error = gpio_request(pdata->gpio_irq, "himax_gpio_irq");
			if (error)
			{
				E("unable to request gpio [%d]\n",pdata->gpio_irq);
				return error;
			}
			error = gpio_direction_input(pdata->gpio_irq);
			if (error)
			{
				E("unable to set direction for gpio [%d]\n",pdata->gpio_irq);
				return error;
			}
			client->irq = gpio_to_irq(pdata->gpio_irq);
		}
		else
		{
			E("irq gpio not provided\n");
			return error;
		}
#endif

		msleep(20);
	
#ifdef HX_RST_PIN_FUNC
		if (pdata->gpio_reset >= 0)
		{
			error = gpio_direction_output(pdata->gpio_reset, 1);
			if (error)
			{
				E("unable to set direction for gpio [%d]\n",
				  pdata->gpio_reset);
				return error;
			}
		}
		msleep(20);
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
	uint8_t diag_cmd;
	struct himax_ts_data *ts = ptr;
	struct timespec timeStart, timeEnd, timeDelta;

	diag_cmd = getDiagCommand();

	if (ts->debug_log_level & BIT(2)) {
			getnstimeofday(&timeStart);
			usleep_range(5000, 7000);
			//I(" Irq start time = %ld.%06ld s\n",
			//	timeStart.tv_sec, timeStart.tv_nsec/1000);
	}

#ifdef HX_SMART_WAKEUP
	if (atomic_read(&ts->suspend_mode)&&(!FAKE_POWER_KEY_SEND)&&(ts->SMWP_enable)&&(!diag_cmd)) {
		wake_lock_timeout(&ts->ts_SMWP_wake_lock, TS_WAKE_LOCK_TIMEOUT);
		msleep(200);
		himax_wake_check_func();
		return IRQ_HANDLED;
	}
#endif
	himax_ts_isr_func((struct himax_ts_data *)ptr);
	if(ts->debug_log_level & BIT(2)) {
			getnstimeofday(&timeEnd);
				timeDelta.tv_nsec = (timeEnd.tv_sec*1000000000+timeEnd.tv_nsec)
				-(timeStart.tv_sec*1000000000+timeStart.tv_nsec);
			//I("Irq finish time = %ld.%06ld s\n",
			//	timeEnd.tv_sec, timeEnd.tv_nsec/1000);
			//I("Touch latency = %ld us\n", timeDelta.tv_nsec/1000);
	}
	return IRQ_HANDLED;
}

static void himax_ts_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, work);
	himax_ts_work(ts);
}

int tp_irq = -1;

int himax_ts_register_interrupt(struct i2c_client *client)
{
	struct himax_ts_data *ts = i2c_get_clientdata(client);
	int ret = 0;

	ts->irq_enabled = 0;
	//Work functon
	if (client->irq) {/*INT mode*/
		ts->use_irq = 1;
		if(ic_data->HX_INT_IS_EDGE)
			{
				I("%s edge triiger falling\n ",__func__);
				ret = request_threaded_irq(client->irq, NULL, himax_ts_thread,IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->name, ts);
			}
		else
			{
				I("%s level trigger low\n ",__func__);
				ret = request_threaded_irq(client->irq, NULL, himax_ts_thread,IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->name, ts);
			}
		if (ret == 0) {
			ts->irq_enabled = 1;
			irq_enable_count = 1;
			tp_irq = client->irq;
			I("%s: irq enabled at qpio: %d\n", __func__, client->irq);
#ifdef HX_SMART_WAKEUP
			irq_set_irq_wake(client->irq, 1);
#endif
		} else {
			ts->use_irq = 0;
			E("%s: request_irq failed\n", __func__);
		}
	} else {
		I("%s: client->irq is empty, use polling mode.\n", __func__);
	}

	if (!ts->use_irq) {/*if use polling mode need to disable HX_ESD_WORKAROUND function*/
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

	I("%s: enter \n", __func__);

	himax_chip_common_suspend(ts);
	return 0;
}

static int himax_common_resume(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter \n", __func__);

	himax_chip_common_resume(ts);
	return 0;
}

#if defined(CONFIG_FB)
int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct himax_ts_data *ts=
		container_of(self, struct himax_ts_data, fb_notif);

	I(" %s\n", __func__);
	if (evdata && evdata->data && event == FB_EVENT_BLANK && ts &&
			ts->client) {
		blank = evdata->data;

		mutex_lock(&ts->fb_mutex);
		switch (*blank) {
		case FB_BLANK_UNBLANK:
			if (!ts->probe_done) {
				himax_ts_init(ts);
				ts->probe_done = true;
			} else {
				himax_common_resume(&ts->client->dev);
			}
		break;

		case FB_BLANK_POWERDOWN:
		case FB_BLANK_HSYNC_SUSPEND:
		case FB_BLANK_VSYNC_SUSPEND:
		case FB_BLANK_NORMAL:
			himax_common_suspend(&ts->client->dev);
		break;
		}
		mutex_unlock(&ts->fb_mutex);
	}

	return 0;
}
#endif

static const struct i2c_device_id himax_common_ts_id[] = {
	{HIMAX_common_NAME, 0 },
	{}
};

static const struct dev_pm_ops himax_common_pm_ops = {
#if (!defined(CONFIG_FB))
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
	.id_table	= himax_common_ts_id,
	.probe		= himax_chip_common_probe,
	.remove		= himax_chip_common_remove,
	.driver		= {
	.name = HIMAX_common_NAME,
	.owner = THIS_MODULE,
	.of_match_table = himax_match_table,
#ifdef CONFIG_PM
	.pm				= &himax_common_pm_ops,
#endif
	},
};

static void __init himax_common_init_async(void *unused, async_cookie_t cookie)
{
	I("%s:Enter \n", __func__);
	i2c_add_driver(&himax_common_driver);
}

static int __init himax_common_init(void)
{
	I("Himax common touch panel driver init\n");
	async_schedule(himax_common_init_async, NULL);
	return 0;
}

static void __exit himax_common_exit(void)
{
	i2c_del_driver(&himax_common_driver);
}

module_init(himax_common_init);
module_exit(himax_common_exit);

MODULE_DESCRIPTION("Himax_common driver");
MODULE_LICENSE("GPL");

