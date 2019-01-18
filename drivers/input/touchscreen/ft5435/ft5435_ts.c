/*
 *
 * FocalTech ft5435 TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
 * Copyright (C) 2019 XiaoMi, Inc.
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/firmware.h>
#include <linux/debugfs.h>
#include <linux/wakelock.h>
#include <linux/hqsysfs.h>
#include "ft5435_ts.h"
#include <linux/proc_fs.h>
#include <asm/uaccess.h>

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>

#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#include <linux/sensors.h>
/* Early-suspend level */
#define FT_SUSPEND_LEVEL 1
#endif

#define FTS_RESUME_EN 1
struct mutex report_mutex;
struct input_dev *ft5435_input_dev;

#define TCT_KEY_BACK  158
#define TCT_KEY_HOME 172
#define TCT_KEY_MENU 139



#define TCT_KEY_BACK_POS_X  100
#define TCT_KEY_BACK_POS_Y  1321

#define TCT_KEY_HOME_POS_X  360
#define TCT_KEY_HOME_POS_Y  1321

#define TCT_KEY_MENU_POS_X  620
#define TCT_KEY_MENU_POS_Y  1321

#define TX_NUM_MAX 50
#define RX_NUM_MAX 50


#define FT_DRIVER_VERSION	0x02

#define TRULY 0x79
#define CHAOSHENG 0x57

#define FT_META_REGS		3
#define FT_ONE_TCH_LEN		6
#define FT_TCH_LEN(x)		(FT_META_REGS + FT_ONE_TCH_LEN * x)

#define FT_PRESS		0x7F
#define FT_MAX_ID		0x0F
#define FT_TOUCH_X_H_POS	3
#define FT_TOUCH_X_L_POS	4
#define FT_TOUCH_Y_H_POS	5
#define FT_TOUCH_Y_L_POS	6
#define FT_TD_STATUS		2
#define FT_TOUCH_EVENT_POS	3
#define FT_TOUCH_ID_POS		5
#define FT_TOUCH_DOWN		0
#define FT_TOUCH_CONTACT	2

#define FT_REG_ID           0xA3
#define FT_STATUS_NUM_TP_MASK	0x0F

#define FT_VTG_MIN_UV		2600000
#define FT_VTG_MAX_UV		3300000
#define FT_I2C_VTG_MIN_UV	1800000
#define FT_I2C_VTG_MAX_UV	1800000

#define FT_FW_NAME_MAX_LEN	50
#define FT_MAX_WR_BUF		10
#define FT_INFO_MAX_LEN		512

#define FT_COORDS_ARR_SIZE     4

static struct i2c_client  *ft_g_client;

#define FT_STORE_TS_INFO(buf, id, name, max_tch, group_id, fw_vkey_support, \
			fw_name, fw_maj, fw_min, fw_sub_min) \
			snprintf(buf, FT_INFO_MAX_LEN, \
				"controller\t= focaltech\n" \
				"model\t\t= 0x%x\n" \
				"name\t\t= %s\n" \
				"max_touches\t= %d\n" \
				"drv_ver\t\t= 0x%x\n" \
				"group_id\t= 0x%x\n" \
				"fw_vkey_support\t= %s\n" \
				"fw_name\t\t= %s\n" \
				"fw_ver\t\t= %d.%d.%d\n", id, name, \
				max_tch, FT_DRIVER_VERSION, group_id, \
				fw_vkey_support, fw_name, fw_maj, fw_min, \
				fw_sub_min)

#define FT_DEBUG_DIR_NAME	"ts_debug"

struct ft5435_ts_data {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct mutex report_mutex;
	const struct ft5435_ts_platform_data *pdata;
	struct regulator *vdd;
	struct regulator *vcc_i2c;
	char fw_name[FT_FW_NAME_MAX_LEN];
	bool loading_fw;
	u8 family_id;
	struct dentry *dir;
	u16 addr;
	bool suspended;
	char *ts_info;
	u8 *tch_data;
	u32 tch_data_len;
	u8 fw_ver[3];
	u8 fw_vendor_id;

#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	struct pinctrl *ts_pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};
bool is_ft5435 = false;
struct wake_lock ft5436_wakelock;

static int ft5435_i2c_read(struct i2c_client *client, char *writebuf,
			   int writelen, char *readbuf, int readlen);

static int ft5435_i2c_write(struct i2c_client *client, char *writebuf, int writelen);

static struct workqueue_struct *ft5435_wq;
static struct ft5435_ts_data *g_ft5435_ts_data;

static int init_ok = 0;


module_param_named(init_ok, init_ok, int, 0644);


#if FTS_RESUME_EN

#define FTS_RESUME_WAIT_TIME             20

static struct delayed_work ft5435_resume_work;
static struct workqueue_struct *ft5435_resume_workqueue = NULL;
static int ft5435_ts_resume(struct device *dev);
static int ft5x0x_write_reg(struct i2c_client *client, u8 addr, const u8 val);



static void ft5435_resume_func(struct work_struct *work)
{
	struct ft5435_ts_data *data = g_ft5435_ts_data;
	printk("Exter %s",__func__);

	msleep(data->pdata->soft_rst_dly);
	mutex_lock(&data->report_mutex);

	ft5x0x_write_reg(data->client, 0x8c, 0x01);
	enable_irq_wake(data->client->irq);
	data->suspended = false;

	mutex_unlock(&data->report_mutex);
}

void ft5435_resume_queue_work(void)
{
	cancel_delayed_work(&ft5435_resume_work);
	queue_delayed_work(ft5435_resume_workqueue, &ft5435_resume_work, msecs_to_jiffies(FTS_RESUME_WAIT_TIME));
}


int ft5435_resume_init(void)
{
	INIT_DELAYED_WORK(&ft5435_resume_work, ft5435_resume_func);
	ft5435_resume_workqueue = create_workqueue("fts_resume_wq");
	if (ft5435_resume_workqueue == NULL)
	{

	}
	else
	{

	}

	return 0;
}

int ft5435_resume_exit(void)
{
	destroy_workqueue(ft5435_resume_workqueue);

	return 0;
}

#endif

static struct mutex g_device_mutex;


static int ft5435_i2c_read(struct i2c_client *client, char *writebuf,
			   int writelen, char *readbuf, int readlen)
{
	int ret;
	if (writelen > 0) {
		struct i2c_msg msgs[] = {
		    {
			.addr = client->addr,
			.flags = 0,
			.len = writelen,
			.buf = writebuf,
		    },
		    {
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = readlen,
			.buf = readbuf,
		    },
		};
	    ret = i2c_transfer(client->adapter, msgs, 2);
	    if (ret < 0)
		    dev_err(&client->dev, "%s: i2c read error.\n", __func__);
	} else {
		    struct i2c_msg msgs[] = {
			{
			    .addr = client->addr,
			    .flags = I2C_M_RD,
			    .len = readlen,
			    .buf = readbuf,
			},
		    };
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
		        dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	    }
	    return ret;
}

static int ft5435_i2c_write(struct i2c_client *client, char *writebuf,
			    int writelen)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
		    .addr = client->addr,
		    .flags = 0,
		    .len = writelen,
		    .buf = writebuf,
		},
	    };
	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s: i2c write error.\n", __func__);
	return ret;
}

static int ft5x0x_write_reg(struct i2c_client *client, u8 addr, const u8 val)
{
	u8 buf[2] = {0};
	buf[0] = addr;
	buf[0] = addr;

	return ft5435_i2c_write(client, buf, sizeof(buf));
}

static int ft5x0x_read_reg(struct i2c_client *client, u8 addr, u8 *val)
{
	return ft5435_i2c_read(client, &addr, 1, val, 1);
}

static irqreturn_t ft5435_ts_interrupt(int irq, void *dev_id)
{
#if 1
	struct ft5435_ts_data *data = dev_id;
	struct input_dev *ip_dev;
	int rc, i,j;
	u32 id, x, y, status, num_touches;
	u8 reg = 0x00, *buf;
	bool update_input = false;

#ifdef CONFIG_TOUCHPANEL_PROXIMITY_SENSOR
	u8 reg_value;
	u8 proximity_status;
#endif
#ifdef FOCALTECH_TP_GESTURE
	int ret = 0;
	u8 state = 0;
#endif
	if (!data) {
		pr_err("%s: Invalid data\n", __func__);
		return IRQ_HANDLED;
	}
#if defined(FOCALTECH_TP_GESTURE)
	if (gesture_func_on) {
		ret = ft5x0x_read_reg(data->client, 0xd0, &state);
		if (ret<0) {
			printk("[FTS]read value fail\n");
		}
		if (state == 1) {
			ft_tp_interrupt(data);
			return IRQ_HANDLED;
		}
	}

#endif

	ip_dev = data->input_dev;
	buf = data->tch_data;
#ifdef CONFIG_TOUCHPANEL_PROXIMITY_SENSOR
	if(vps_ft5436->vps_enabled)
	{
		ft5x0x_read_reg(data->client, 0xB0, &reg_value);
		printk("proxi_fts 0xB0 state value is0x%02X\n", reg_value);
		if(!(reg_value&0x01))
		{
			tp_prox_sensor_enable(data->client, 1);
		}
		ft5x0x_read_reg(data->client, 0x01, &proximity_status);
		printk("FT 0x01 reg proximity_status[0x%x]--%s\n",proximity_status,__FUNCTION__);
		if(proximity_status == 0xC0)
		{
			input_report_abs(vps_ft5436->proximity_dev, ABS_DISTANCE, 0);
			input_sync(vps_ft5436->proximity_dev);
			printk("[Fu]close\n");
		} else if (proximity_status == 0xE0) {
			wake_lock_timeout(&ft5436_wakelock, 1*HZ);
			input_report_abs(vps_ft5436->proximity_dev, ABS_DISTANCE, 1);
			input_sync(vps_ft5436->proximity_dev);
			printk("[Fu]leave\n");
		}
	}
#endif

	rc = ft5435_i2c_read(data->client, &reg, 1,
			buf, data->tch_data_len);
	if (rc < 0) {
		dev_err(&data->client->dev, "%s: read data fail\n", __func__);
		return IRQ_HANDLED;
	}

	mutex_lock(&data->report_mutex);

	for (i = 0; i < data->pdata->num_max_touches; i++) {
		id = (buf[FT_TOUCH_ID_POS + FT_ONE_TCH_LEN * i]) >> 4;
		if (id >= FT_MAX_ID)
			break;

		update_input = true;

		x = (buf[FT_TOUCH_X_H_POS + FT_ONE_TCH_LEN * i] & 0x0F) << 8 |
			(buf[FT_TOUCH_X_L_POS + FT_ONE_TCH_LEN * i]);
		y = (buf[FT_TOUCH_Y_H_POS + FT_ONE_TCH_LEN * i] & 0x0F) << 8 |
		        (buf[FT_TOUCH_Y_L_POS + FT_ONE_TCH_LEN * i]);
	        status = buf[FT_TOUCH_EVENT_POS + FT_ONE_TCH_LEN * i] >> 6;
	        num_touches = buf[FT_TD_STATUS] & FT_STATUS_NUM_TP_MASK;

		/* invalid combination */
		if (!num_touches && !status && !id)
			break;
		input_mt_slot(ip_dev, id);

		if (status==FT_TOUCH_DOWN)
			printk("[FTS]Down pid[%d]:[%d,%d]\n",id,x,y);
		else if (status==1)
			printk("[FTS]Up pid[%d]:[%d,%d]\n",id,x,y);

	if ( x < data->pdata->panel_maxx && y < data->pdata->panel_maxy ) {
		input_mt_slot(ip_dev, id);
			if (status == FT_TOUCH_DOWN || status == FT_TOUCH_CONTACT) {
				input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 1);
				                input_report_abs(ip_dev, ABS_MT_POSITION_X, x);
				                input_report_abs(ip_dev, ABS_MT_POSITION_Y, y);
				                input_report_abs(ip_dev, BTN_TOUCH, 1);
			} else {
				input_mt_report_slot_state(ip_dev, MT_TOOL_FINGER, 0);
	}
		} else {
			if (data->pdata->fw_vkey_support) {
				for(j = 0; j < data->pdata->num_virkey; j++) {
				if (x == data->pdata->vkeys[j].x) {
					if (status == FT_TOUCH_DOWN || status == FT_TOUCH_CONTACT)
						input_report_key(data->input_dev, data->pdata->vkeys[j].keycode, true);
					else {
						input_report_key(data->input_dev, data->pdata->vkeys[j].keycode, false);
					}
						input_sync(ip_dev);
				}
			}

		}
		}
	}
	if (update_input) {
#if defined(FOCALTECH_FAE_MOD)
		if (num_touches == 0) { /* release all touches */
			for (i = 0; i < 10; i++) {
				input_mreg_vdd_set_vtg:t_slot(data->input_dev, i);
		                input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
			}
			input_report_abs(ip_dev, BTN_TOUCH, 0);
		}
		else {
			input_report_abs(ip_dev, BTN_TOUCH, 1);
		}
#endif
		input_mt_report_pointer_emulation(ip_dev, false);
		input_sync(ip_dev);
	}
	mutex_unlock(&data->report_mutex);
#endif
	return IRQ_HANDLED;
}

static int ft5435_power_on(struct ft5435_ts_data *data, bool on)
{
	int rc;
	if (!on)
		goto power_off;
	rc = regulator_enable(data->vdd);
	if (rc) {
		dev_err(&data->client->dev,
		        "Regulator vdd enable failed rc=%d\n", rc);
		return rc;
	}

	rc = regulator_enable(data->vcc_i2c);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vcc_i2c enable failed rc=%d\n", rc);
		regulator_disable(data->vdd);
	}
	return rc;
power_off:
	rc = regulator_disable(data->vdd);
	if (rc) {
		dev_err(&data->client->dev,
		    "Regulator vdd disable failed rc=%d\n", rc);
		return rc;
	}
	rc = regulator_disable(data->vcc_i2c);
	if (rc) {
		dev_err(&data->client->dev,
			"Regulator vcc_i2c disable failed rc=%d\n", rc);
		rc = regulator_enable(data->vdd);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator vdd enable failed rc=%d\n", rc);
		}

	}
	return rc;
}

static int ft5435_power_init(struct ft5435_ts_data *data, bool on)
{
	int rc;
	if (!on)
		goto pwr_deinit;
	data->vdd = regulator_get(&data->client->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		rc = PTR_ERR(data->vdd);
		dev_err(&data->client->dev,
		        "Regulator get failed vdd rc=%d\n", rc);
		return rc;
	}

	if (regulator_count_voltages(data->vdd) > 0) {
		rc = regulator_set_voltage(data->vdd, FT_VTG_MIN_UV,
					   FT_VTG_MAX_UV);
		if (rc) {
			dev_err(&data->client->dev,
				"Regulator set_vtg failed vdd rc=%d\n", rc);
			goto reg_vdd_put;
	    }
	}
	data->vcc_i2c = regulator_get(&data->client->dev, "vcc_i2c");
	if (IS_ERR(data->vcc_i2c)) {
		rc = PTR_ERR(data->vcc_i2c);
		dev_err(&data->client->dev,
		        "Regulator get failed vcc_i2c rc=%d\n", rc);
		goto reg_vdd_set_vtg;
	}

	if (regulator_count_voltages(data->vcc_i2c) > 0) {
		rc = regulator_set_voltage(data->vcc_i2c, FT_I2C_VTG_MIN_UV,
				           FT_I2C_VTG_MAX_UV);
		if (rc) {
			dev_err(&data->client->dev,
			"Regulator set_vtg failed vcc_i2c rc=%d\n", rc);
			goto reg_vcc_i2c_put;
		}
	}
	return 0;

reg_vcc_i2c_put:
	regulator_put(data->vcc_i2c);
reg_vdd_set_vtg:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, FT_VTG_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	return rc;
pwr_deinit:
	if (regulator_count_voltages(data->vdd) > 0)
	        regulator_set_voltage(data->vdd, 0, FT_VTG_MAX_UV);
	regulator_put(data->vdd);
	return 0;
}

static int ft5435_ts_pinctrl_init(struct ft5435_ts_data *ft5435_data)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	ft5435_data->ts_pinctrl = devm_pinctrl_get(&(ft5435_data->client->dev));
	if (IS_ERR_OR_NULL(ft5435_data->ts_pinctrl)) {
		dev_dbg(&ft5435_data->client->dev,
		   "Target does not use pinctrl\n");
		retval = PTR_ERR(ft5435_data->ts_pinctrl);
		ft5435_data->ts_pinctrl = NULL;
		goto err_pinctrl_get;
	}

	ft5435_data->gpio_state_active
			   = pinctrl_lookup_state(ft5435_data->ts_pinctrl,
			  "pmx_ts_active");
	if (IS_ERR_OR_NULL(ft5435_data->gpio_state_active)) {
		dev_dbg(&ft5435_data->client->dev,
			"Can not get ts default pinstate\n");
		retval = PTR_ERR(ft5435_data->gpio_state_active);
		ft5435_data->ts_pinctrl = NULL;
		goto err_pinctrl_lookup;
	}

	ft5435_data->gpio_state_suspend
	     = pinctrl_lookup_state(ft5435_data->ts_pinctrl,
		     "pmx_ts_suspend");
	if (IS_ERR_OR_NULL(ft5435_data->gpio_state_suspend)) {
		dev_err(&ft5435_data->client->dev,
		        "Can not get ts sleep pinstate\n");
		retval = PTR_ERR(ft5435_data->gpio_state_suspend);
		ft5435_data->ts_pinctrl = NULL;
		goto err_pinctrl_lookup;
	}

	return 0;
err_pinctrl_lookup:
	devm_pinctrl_put(ft5435_data->ts_pinctrl);
err_pinctrl_get:
	ft5435_data->ts_pinctrl = NULL;
	return retval;
}

static int ft5435_ts_pinctrl_select(struct ft5435_ts_data *ft5435_data,
						bool on)
{
	struct pinctrl_state *pins_state;
	int ret;
	pins_state = on ? ft5435_data->gpio_state_active
		: ft5435_data->gpio_state_suspend;
		if (!IS_ERR_OR_NULL(pins_state)) {
			ret = pinctrl_select_state(ft5435_data->ts_pinctrl, pins_state);
			if (ret) {
				dev_err(&ft5435_data->client->dev,
				      "can not set %s pins\n",
				       on ? "pmx_ts_active" : "pmx_ts_suspend");
			return ret;
		}
			} else {
				dev_err(&ft5435_data->client->dev,
				    "not a valid '%s' pinstate\n",
				    on ? "pmx_ts_active" : "pmx_ts_suspend");
			}
		return 0;
}

/******************************************************esd******************************************/

#ifdef CONFIG_PM
static int ft5435_ts_suspend(struct device *dev)
{
	struct ft5435_ts_data *data = g_ft5435_ts_data;


	u8 reg_addr;
	u8 reg_value;

	if (data->loading_fw) {
		dev_info(dev, "Firmware loading in process...\n");
		return 0;
	}

	if (data->suspended) {
	        dev_info(dev, "Already in suspend state\n");
		return 0;
	}

	disable_irq_wake(data->client->irq);

#if defined(FOCALTECH_TP_GESTURE)
	{
		if(gesture_func_on) {
			enable_irq_wake(data->client->irq);
			ft_tp_suspend(data);
			return 0;
		}
	}
#endif

	reg_addr = FT_REG_ID;
	ft5435_i2c_read(data->client, &reg_addr, 1, &reg_value, 1);
	printk("reg_value : %0x\n",reg_value);

	if (reg_value != 0x54) {
		printk("i2c read err , set rst\n");
		if (gpio_is_valid(data->pdata->reset_gpio)) {
			gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
			msleep(300);
		}
	} else {
	    printk("i2c read OK , no rst\n");
	}
	data->suspended = true;

	return 0;
}

static int ft5435_ts_resume(struct device *dev)
{
	struct ft5435_ts_data *data = g_ft5435_ts_data;
	int i = 0;

	if (!data->suspended) {
		dev_dbg(dev, "Already in awake state\n");
		return 0;
	}

	mutex_lock(&data->report_mutex);
	for (i = 0; i < data->pdata->num_max_touches; i++) {
		input_mt_slot(data->input_dev, i);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
	}
	input_report_abs(data->input_dev, BTN_TOUCH, 0);
	input_mt_report_pointer_emulation(data->input_dev, false);
	input_sync(data->input_dev);
	mutex_unlock(&data->report_mutex);

	if (gpio_is_valid(data->pdata->reset_gpio)) {
		gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
		msleep(2);
		gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
	}
#if FTS_RESUME_EN
	ft5435_resume_queue_work();
#else
	msleep(data->pdata->soft_rst_dly);
	ft5x0x_write_reg(data->client, 0x8c, 0x01);
	enable_irq_wake(data->client->irq);
	data->suspended = false;
#endif
	return 0;
}

static const struct dev_pm_ops ft5435_ts_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
	.suspend = ft5435_ts_suspend,
	.resume = ft5435_ts_resume,
#endif
};

#else
static int ft5435_ts_suspend(struct device *dev)
{
	return 0;
}

static int ft5435_ts_resume(struct device *dev)
{
	return 0;
}

#endif
#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct ft5435_ts_data *ft5435_data =
		container_of(self, struct ft5435_ts_data, fb_notif);
	if (evdata && evdata->data && event == FB_EVENT_BLANK &&
		    ft5435_data && ft5435_data->client) {
	    blank = evdata->data;
	    if (*blank == FB_BLANK_UNBLANK)
		    ft5435_ts_resume(&ft5435_data->client->dev);
	    else if (*blank == FB_BLANK_POWERDOWN)
		    ft5435_ts_suspend(&ft5435_data->client->dev);
	}
	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void ft5435_ts_early_suspend(struct early_suspend *handler)
{
	struct ft5435_ts_data *data = container_of(handler,
						struct ft5435_ts_data,
						early_suspend);

	ft5435_ts_suspend(&data->client->dev);
}

static void ft5435_ts_late_resume(struct early_suspend *handler)
{
	struct ft5435_ts_data *data = container_of(handler,
						struct ft5435_ts_data,
						early_suspend);

	ft5435_ts_resume(&data->client->dev);
}
#endif

static unsigned int booting_into_recovery = 0;
static int __init get_boot_mode(char *str)
{
	if (strcmp("boot_with_recovery", str) == 0) {
		booting_into_recovery = 1;
	}

	printk("zakk: booting_into_recovery=%d\n", booting_into_recovery);
	return 0;
}
__setup("androidboot.boot_reason=", get_boot_mode);

static bool ft5435_debug_addr_is_valid(int addr)
{
	if (addr < 0 || addr > 0xFF) {
		pr_err("FT reg address is invalid: 0x%x\n", addr);
		return false;
	}

	return true;
}

static int ft5435_debug_data_set(void *_data, u64 val)
{
	struct ft5435_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (ft5435_debug_addr_is_valid(data->addr))
		dev_info(&data->client->dev,
			"Writing into FT registers not supported\n");

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

static int ft5435_debug_data_get(void *_data, u64 *val)
{
	struct ft5435_ts_data *data = _data;
	int rc;
	u8 reg;

	mutex_lock(&data->input_dev->mutex);

	if (ft5435_debug_addr_is_valid(data->addr)) {
		rc = ft5x0x_read_reg(data->client, data->addr, &reg);
		if (rc < 0)
			dev_err(&data->client->dev,
				"FT read register 0x%x failed (%d)\n",
				data->addr, rc);
		else
			*val = reg;
	}

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_data_fops, ft5435_debug_data_get,
			ft5435_debug_data_set, "0x%02llX\n");

static int ft5435_debug_addr_set(void *_data, u64 val)
{
	struct ft5435_ts_data *data = _data;

	if (ft5435_debug_addr_is_valid(val)) {
		mutex_lock(&data->input_dev->mutex);
		data->addr = val;
		mutex_unlock(&data->input_dev->mutex);
	}

	return 0;
}

static int ft5435_debug_addr_get(void *_data, u64 *val)
{
	struct ft5435_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (ft5435_debug_addr_is_valid(data->addr))
		*val = data->addr;

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_addr_fops, ft5435_debug_addr_get,
			ft5435_debug_addr_set, "0x%02llX\n");

static int ft5435_debug_suspend_set(void *_data, u64 val)
{
	struct ft5435_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);

	if (val)
		ft5435_ts_suspend(&data->client->dev);
	else
		ft5435_ts_resume(&data->client->dev);

	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

static int ft5435_debug_suspend_get(void *_data, u64 *val)
{
	struct ft5435_ts_data *data = _data;

	mutex_lock(&data->input_dev->mutex);
	*val = data->suspended;
	mutex_unlock(&data->input_dev->mutex);

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(debug_suspend_fops, ft5435_debug_suspend_get,
			ft5435_debug_suspend_set, "%lld\n");

static int ft5435_debug_dump_info(struct seq_file *m, void *v)
{
	struct ft5435_ts_data *data = m->private;

	seq_printf(m, "%s\n", data->ts_info);

	return 0;
}

static int debugfs_dump_info_open(struct inode *inode, struct file *file)
{
	return single_open(file, ft5435_debug_dump_info, inode->i_private);
}

static const struct file_operations debug_dump_info_fops = {
	.owner		= THIS_MODULE,
	.open		= debugfs_dump_info_open,
	.read		= seq_read,
	.release	= single_release,
};

#ifdef CONFIG_OF
static int ft5435_get_dt_coords(struct device *dev, char *name,
				struct ft5435_ts_platform_data *pdata)
{
	u32 coords[FT_COORDS_ARR_SIZE];
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size, rc;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != FT_COORDS_ARR_SIZE) {
		dev_err(dev, "invalid %s\n", name);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(np, name, coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read %s\n", name);
		return rc;
	}

	if (!strcmp(name, "focaltech,panel-coords")) {
		pdata->panel_minx = coords[0];
		pdata->panel_miny = coords[1];
		pdata->panel_maxx = coords[2];
		pdata->panel_maxy = coords[3];
	} else if (!strcmp(name, "focaltech,display-coords")) {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	} else {
		dev_err(dev, "unsupported property %s\n", name);
		return -EINVAL;
	}

	return 0;
}
static int ft5435_get_dt_vkey(struct device *dev, struct ft5435_ts_platform_data *pdata)
{
	u32 coords[FOCALTECH_MAX_VKEY_NUM];
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size, rc, i;
	char name[128];
	memset(name, 0, sizeof(name));

	sprintf(name, "focal,virtual_key_1");
	prop = of_find_property(np, name, NULL);
	printk("[%s]000\n",__FUNCTION__);
	if (!prop)
		return -EINVAL;
	printk("[%s]111\n",__FUNCTION__);
	if (!prop->value)
		return -ENODATA;
	printk("[%s]222\n",__FUNCTION__);
	coords_size = prop->length / sizeof(u32);
	if (coords_size != pdata->num_virkey) {
		printk("[%s]invalid %s\n",__FUNCTION__, name);
		return -EINVAL;
	}
	printk("[%s]333\n",__FUNCTION__);
	for(i = 0; i < pdata->num_virkey; i++) {
		sprintf(name, "focal,virtual_key_%d", i+1);
		rc = of_property_read_u32_array(np, name, coords, coords_size);
		if (rc && (rc != -EINVAL)) {
			printk("[%s]Unable to read %s\n", __FUNCTION__,name);
			return rc;
		}

		pdata->vkeys[i].keycode = coords[0];
		pdata->vkeys[i].x = coords[1];
		pdata->vkeys[i].y = coords[2];
		printk("[FTS]keycode = %d, x= %d, y=%d \n", pdata->vkeys[i].keycode,
			pdata->vkeys[i].x,pdata->vkeys[i].y);
	}
	printk("[%s]5555\n",__FUNCTION__);
	return 0;
}

static int ft5435_parse_dt(struct device *dev,
			struct ft5435_ts_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	u32 temp_val;

	pdata->name = "focaltech";
	rc = of_property_read_string(np, "focaltech,name", &pdata->name);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read name\n");
		return rc;
	}

	rc = ft5435_get_dt_coords(dev, "focaltech,panel-coords", pdata);
	if (rc && (rc != -EINVAL))
		return rc;

	rc = ft5435_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (rc)
		return rc;

	pdata->i2c_pull_up = of_property_read_bool(np,
						"focaltech,i2c-pull-up");

	pdata->no_force_update = of_property_read_bool(np,
						"focaltech,no-force-update");
	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio",
				0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
		return pdata->reset_gpio;

	pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio",
				0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
		return pdata->irq_gpio;

	rc = of_property_read_u32(np, "focaltech,group-id", &temp_val);
	if (!rc)
		pdata->group_id = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "focaltech,hard-reset-delay-ms",
							&temp_val);
	if (!rc)
		pdata->hard_rst_dly = temp_val;
	else
		 return rc;

	rc = of_property_read_u32(np, "focaltech,soft-reset-delay-ms",
							&temp_val);
	if (!rc)
		pdata->soft_rst_dly = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "focaltech,num-max-touches", &temp_val);
	if (!rc)
		pdata->num_max_touches = temp_val;
	else
		return rc;

	rc = of_property_read_u32(np, "focaltech,fw-delay-readid-ms",
							&temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay read id\n");
		return rc;
	} else if (rc != -EINVAL)
	rc = of_property_read_u32(np, "focaltech,fw-delay-era-flsh-ms",
							&temp_val);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read fw delay erase flash\n");
		return rc;
	} else if (rc != -EINVAL)

	pdata->resume_in_workqueue = of_property_read_bool(np,
						"focaltech,resume-in-workqueue");

	pdata->fw_vkey_support = of_property_read_bool(np,
						"focaltech,fw-vkey-support");

	pdata->ignore_id_check = of_property_read_bool(np,
					  "focaltech,ignore-id-check");

	rc = of_property_read_u32(np,"focaltech,num-virtual-key",&temp_val);
	if (rc && (rc != -EINVAL)) {
		printk("[%s]focaltech,num-virtual-key,dts parase failed\n",__FUNCTION__);
		return rc;
	} else if (rc != -EINVAL) {
		pdata->num_virkey = temp_val;
	}
	rc = ft5435_get_dt_vkey(dev, pdata);
	if (rc) {
		printk("[%s]focaltech,ft5435_get_dt_vkey failed\n",__FUNCTION__);
		return rc;
	}

	rc = of_property_read_u32(np, "focaltech,family-id", &temp_val);
	if (!rc)
		pdata->family_id = temp_val;
	else
		return rc;

	return 0;
}
#else
static int ft5435_parse_dt(struct device *dev,
			struct ft5435_ts_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static char tp_info_summary[80] = "";

static int ft5435_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct ft5435_ts_platform_data *pdata;
	struct ft5435_ts_data *data;
	struct input_dev *input_dev;
	struct dentry *temp;
	u8 reg_value;
	u8 reg_addr;
	int err, len;
	int i;
	int retry = 3;
	char tp_temp_info[80];
	printk("~~~~~ ft5435_ts_probe start\n");

#if defined(CONFIG_FB)
	printk("[%s]CONFIG_FB is defined\n",__FUNCTION__);
#endif
#if defined(CONFIG_PM)
	printk("[%s]CONFIG_PM is defined\n",__FUNCTION__);
#endif

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct ft5435_ts_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		err = ft5435_parse_dt(&client->dev, pdata);
		if (err) {
			dev_err(&client->dev, "DT parsing failed\n");
			return err;
		}
	} else
		pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "Invalid pdata\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "I2C not supported\n");
		return -ENODEV;
	}

	data = devm_kzalloc(&client->dev,
			sizeof(struct ft5435_ts_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Not enough memory\n");
		return -ENOMEM;
	}

	if (pdata->fw_name) {
		len = strlen(pdata->fw_name);
		if (len > FT_FW_NAME_MAX_LEN - 1) {
			dev_err(&client->dev, "Invalid firmware name\n");
			return -EINVAL;
		}

		strlcpy(data->fw_name, pdata->fw_name, len + 1);
	}

	data->tch_data_len = FT_TCH_LEN(pdata->num_max_touches);
	data->tch_data = devm_kzalloc(&client->dev,
				data->tch_data_len, GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Not enough memory\n");
		return -ENOMEM;
	}

	ft5435_input_dev = input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "failed to allocate input device\n");
		return -ENOMEM;
	}

	ft_g_client = client;
	data->input_dev = input_dev;
	data->client = client;
	data->pdata = pdata;

	input_dev->name = "ft5435_ts";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	input_set_drvdata(input_dev, data);
	i2c_set_clientdata(client, data);

	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	for (i = 0; i < pdata->num_virkey; i++) {
		input_set_capability(input_dev, EV_KEY,
							pdata->vkeys[i].keycode);
	}
	input_mt_init_slots(input_dev, pdata->num_max_touches, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min,
			     pdata->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min,
			     pdata->y_max, 0, 0);

	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev, "Input device registration failed\n");
		goto free_inputdev;
	}

	if (gpio_is_valid(pdata->reset_gpio)) {
		err = gpio_request(pdata->reset_gpio, "ft5435_reset_gpio");
		if (err) {
			dev_err(&client->dev, "reset gpio request failed");
			goto unreg_inputdev;
		}

		err = gpio_direction_output(pdata->reset_gpio, 0);
		if (err) {
			dev_err(&client->dev,
				"set_direction for reset gpio failed\n");
			goto free_reset_gpio;
		}
		msleep(2);
	}

	if (pdata->power_init) {
		err = pdata->power_init(true);
		if (err) {
			dev_err(&client->dev, "power init failed");
			goto free_reset_gpio;
		}
	} else {
		err = ft5435_power_init(data, true);
		if (err) {
			dev_err(&client->dev, "power init failed");
			goto free_reset_gpio;
		}
	}

	if (pdata->power_on) {
		err = pdata->power_on(true);
		if (err) {
			dev_err(&client->dev, "power on failed");
			goto pwr_deinit;
		}
	} else {
		err = ft5435_power_on(data, true);
		if (err) {
			dev_err(&client->dev, "power on failed");
			goto pwr_deinit;
		}
	}

	msleep(2);

	err = ft5435_ts_pinctrl_init(data);
	if (!err && data->ts_pinctrl) {
		err = ft5435_ts_pinctrl_select(data, true);
		if (err < 0)
			goto pwr_off;
	}

	if (gpio_is_valid(pdata->irq_gpio)) {
		err = gpio_request(pdata->irq_gpio, "ft5435_irq_gpio");
		if (err) {
			dev_err(&client->dev, "irq gpio request failed");
			goto pwr_off;
		}
		err = gpio_direction_input(pdata->irq_gpio);
		if (err) {
			dev_err(&client->dev,
				"set_direction for irq gpio failed\n");
			goto free_irq_gpio;
		}
	}

	if (gpio_is_valid(pdata->reset_gpio)) {
		gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
	}

	/* make sure CTP already finish startup process */
	msleep(data->pdata->soft_rst_dly);

	/* check the controller id */
	reg_addr = FT_REG_ID;
	while(retry--){
		err = ft5435_i2c_read(client, &reg_addr, 1, &reg_value, 1);
		if(!(err<0)){

			dev_info(&client->dev, "Device ID = 0x%x\n", reg_value);
			break;
		}
		if (gpio_is_valid(pdata->reset_gpio)) {
			gpio_set_value_cansleep(data->pdata->reset_gpio, 0);
			msleep(10);
			gpio_set_value_cansleep(data->pdata->reset_gpio, 1);
			msleep(200);
		}
		if (retry==0) {
			dev_err(&client->dev, "version read failed");
			goto free_irq_gpio;
		}
	}
	data->family_id = pdata->family_id;

	mutex_init(&g_device_mutex);

	mutex_init(&data->report_mutex);
	err = request_threaded_irq(client->irq,NULL,
				ft5435_ts_interrupt,
				pdata->irqflags | IRQF_ONESHOT|IRQF_TRIGGER_FALLING,
				client->dev.driver->name, data);
	if (err) {
		dev_err(&client->dev, "request irq failed\n");
		goto free_reset_gpio;
	}

#if FTS_RESUME_EN
	ft5435_resume_init();
#endif

	data->dir = debugfs_create_dir(FT_DEBUG_DIR_NAME, NULL);
	if (data->dir == NULL || IS_ERR(data->dir)) {
		pr_err("debugfs_create_dir failed(%ld)\n", PTR_ERR(data->dir));
		err = PTR_ERR(data->dir);
	}

	temp = debugfs_create_file("addr", S_IRUSR | S_IWUSR, data->dir, data,
				   &debug_addr_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	temp = debugfs_create_file("data", S_IRUSR | S_IWUSR, data->dir, data,
				   &debug_data_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	temp = debugfs_create_file("suspend", S_IRUSR | S_IWUSR, data->dir,
					data, &debug_suspend_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	temp = debugfs_create_file("dump_info", S_IRUSR | S_IWUSR, data->dir,
					data, &debug_dump_info_fops);
	if (temp == NULL || IS_ERR(temp)) {
		pr_err("debugfs_create_file failed: rc=%ld\n", PTR_ERR(temp));
		err = PTR_ERR(temp);
		goto free_debug_dir;
	}

	data->ts_info = devm_kzalloc(&client->dev,
				FT_INFO_MAX_LEN, GFP_KERNEL);
	if (!data->ts_info) {
		dev_err(&client->dev, "Not enough memory\n");
		goto free_debug_dir;
	}

	FT_STORE_TS_INFO(data->ts_info, data->family_id, data->pdata->name,
			data->pdata->num_max_touches, data->pdata->group_id,
			data->pdata->fw_vkey_support ? "yes" : "no",
			data->pdata->fw_name, data->fw_ver[0],
			data->fw_ver[1], data->fw_ver[2]);

#if defined(CONFIG_FB)

	data->fb_notif.notifier_call = fb_notifier_callback;

	err = fb_register_client(&data->fb_notif);

	if (err)
		dev_err(&client->dev, "Unable to register fb_notifier: %d\n",
			err);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN +
						    FT_SUSPEND_LEVEL;
	data->early_suspend.suspend = ft5435_ts_early_suspend;
	data->early_suspend.resume = ft5435_ts_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	g_ft5435_ts_data = data;
	sprintf(tp_temp_info, "%d",data->fw_ver[0]);
	strcat(tp_info_summary,tp_temp_info);
	strcat(tp_info_summary,"\0");
	hq_regiser_hw_info(HWID_CTP,tp_info_summary);
	printk("~~~~~ ft5435_ts_probe end\n");
	return 0;

free_debug_dir:
	debugfs_remove_recursive(data->dir);

free_irq_gpio:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
	if (data->ts_pinctrl) {
		err = ft5435_ts_pinctrl_select(data, false);
		if (err < 0)
			pr_err("Cannot get idle pinctrl state\n");
	}
	if (data->ts_pinctrl) {
			devm_pinctrl_put(data->ts_pinctrl);
			data->ts_pinctrl = NULL;
	}
	if (gpio_is_valid(pdata->reset_gpio)) {
		gpio_direction_output(pdata->reset_gpio, 1);
		mdelay(2);
	}
pwr_off:
	if (pdata->power_on)
		pdata->power_on(false);
	else
		ft5435_power_on(data, false);
pwr_deinit:
	if (pdata->power_init)
		pdata->power_init(false);
	else
		ft5435_power_init(data, false);
free_reset_gpio:
	if (gpio_is_valid(pdata->reset_gpio)) {
		gpio_direction_output(pdata->reset_gpio, 1);
		mdelay(5);
		gpio_free(pdata->reset_gpio);
	}
unreg_inputdev:
	input_unregister_device(input_dev);
	input_dev = NULL;
free_inputdev:
	input_free_device(input_dev);
	return err;
}

static int ft5435_ts_remove(struct i2c_client *client)
{
	struct ft5435_ts_data *data = i2c_get_clientdata(client);
	int retval;

	debugfs_remove_recursive(data->dir);

#if defined(CONFIG_FB)
	if (fb_unregister_client(&data->fb_notif))
		dev_err(&client->dev, "Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&data->early_suspend);
#endif
	free_irq(client->irq, data);

	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);

	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);

	if (data->ts_pinctrl) {
		retval = ft5435_ts_pinctrl_select(data, false);
		if (retval < 0)
			pr_err("Cannot get idle pinctrl state\n");
		devm_pinctrl_put(data->ts_pinctrl);
		data->ts_pinctrl = NULL;
	}

	if (data->pdata->power_on)
		data->pdata->power_on(false);
	else
		ft5435_power_on(data, false);

	if (data->pdata->power_init)
		data->pdata->power_init(false);
	else
		ft5435_power_init(data, false);

	input_unregister_device(data->input_dev);
	wake_lock_destroy(&ft5436_wakelock);

	return 0;
}

static const struct i2c_device_id ft5435_ts_id[] = {
	{"ft5435_ts", 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ft5435_ts_id);

#ifdef CONFIG_OF
static struct of_device_id ft5435_match_table[] = {
	{ .compatible = "focaltech,5435",},
	{ },
};
#else
#define ft5435_match_table NULL
#endif

static struct i2c_driver ft5435_ts_driver = {
	.probe = ft5435_ts_probe,
	.remove = ft5435_ts_remove,
	.driver = {
		   .name = "ft5435_ts",
		   .owner = THIS_MODULE,
		.of_match_table = ft5435_match_table,
#ifdef CONFIG_PM
		   .pm = &ft5435_ts_pm_ops,
#endif
		   },
	.id_table = ft5435_ts_id,
};

static int __init ft5435_ts_init(void)
{
	printk("tony_test:[%s]\n",__FUNCTION__);
	ft5435_wq = create_singlethread_workqueue("ft5435_wq");
	if (!ft5435_wq) {
		printk("Creat ft5435 workqueue failed. \n");
		return -ENOMEM;
	}
	return i2c_add_driver(&ft5435_ts_driver);
}
module_init(ft5435_ts_init);

static void __exit ft5435_ts_exit(void)
{
	if (ft5435_wq) {
		destroy_workqueue(ft5435_wq);
	}
	i2c_del_driver(&ft5435_ts_driver);
}
module_exit(ft5435_ts_exit);

MODULE_DESCRIPTION("FocalTech ft5435 TouchScreen driver");
MODULE_LICENSE("GPL v2");
