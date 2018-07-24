/*
 * Goodix GT9xx touchscreen driver
 *
 * Copyright  (C)  2016 - 2017 Goodix. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/input/mt.h>
#include "gt9xx.h"

#define GOODIX_VTG_MIN_UV	2600000
#define GOODIX_VTG_MAX_UV	3300000
#define GOODIX_I2C_VTG_MIN_UV	1800000
#define GOODIX_I2C_VTG_MAX_UV	1800000

#define DELAY_FOR_DISCHARGING		35
#define GOODIX_COORDS_ARR_SIZE	4
#define PROP_NAME_SIZE		24
#define I2C_MAX_TRANSFER_SIZE   255
#define GTP_PEN_BUTTON1		BTN_STYLUS
#define GTP_PEN_BUTTON2		BTN_STYLUS2

static const char *goodix_ts_name = "goodix-ts";
static const char *goodix_input_phys = "input/ts";
struct i2c_client *i2c_connect_client;
static struct proc_dir_entry *gtp_config_proc;

enum doze {
	DOZE_DISABLED = 0,
	DOZE_ENABLED = 1,
	DOZE_WAKEUP = 2,
};

static enum doze doze_status = DOZE_DISABLED;

static int gtp_i2c_test(struct i2c_client *client);
static int gtp_enter_doze(struct goodix_ts_data *ts);

static int gtp_unregister_powermanager(struct goodix_ts_data *ts);
static int gtp_register_powermanager(struct goodix_ts_data *ts);

static int gtp_esd_init(struct goodix_ts_data *ts);
static void gtp_esd_check_func(struct work_struct *);
static int gtp_init_ext_watchdog(struct i2c_client *client);

/*
 * return: 2 - ok, < 0 - i2c transfer error
 */
int gtp_i2c_read(struct i2c_client *client, u8 *buf, int len)
{
	unsigned int transfer_length = 0;
	unsigned int pos = 0, address = (buf[0] << 8) + buf[1];
	unsigned char get_buf[64], addr_buf[2];
	int retry, r = 2;
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = !I2C_M_RD,
			.buf = &addr_buf[0],
			.len = GTP_ADDR_LENGTH,
		}, {
			.addr = client->addr,
			.flags = I2C_M_RD,
		}
	};

	len -= GTP_ADDR_LENGTH;
	if (likely(len < sizeof(get_buf))) {
		/* code optimize, use stack memory */
		msgs[1].buf = &get_buf[0];
	} else {
		msgs[1].buf = kzalloc(len > I2C_MAX_TRANSFER_SIZE
				? I2C_MAX_TRANSFER_SIZE : len, GFP_KERNEL);
		if (!msgs[1].buf)
			return -ENOMEM;
	}

	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE))
			transfer_length = I2C_MAX_TRANSFER_SIZE;
		else
			transfer_length = len - pos;
		msgs[0].buf[0] = (address >> 8) & 0xFF;
		msgs[0].buf[1] = address & 0xFF;
		msgs[1].len = transfer_length;
		for (retry = 0; retry < RETRY_MAX_TIMES; retry++) {
			if (likely(i2c_transfer(client->adapter,
					msgs, 2) == 2)) {
				memcpy(&buf[2 + pos], msgs[1].buf,
					transfer_length);
				pos += transfer_length;
				address += transfer_length;
				break;
			}
			dev_dbg(&client->dev, "I2c read retry[%d]:0x%x\n",
				retry + 1, address);
			usleep_range(2000, 2100);
		}
		if (unlikely(retry == RETRY_MAX_TIMES)) {
			dev_err(&client->dev,
				"I2c read failed,dev:%02x,reg:%04x,size:%u\n",
				client->addr, address, len);
			r = -EAGAIN;
			goto read_exit;
		}
	}
read_exit:
	if (len >= sizeof(get_buf))
		kfree(msgs[1].buf);
	return r;
}

/*******************************************************
 * Function:
 *	Write data to the i2c slave device.
 * Input:
 *	client: i2c device.
 *	buf[0~1]: write start address.
 *	buf[2~len-1]: data buffer
 *	len: GTP_ADDR_LENGTH + write bytes count
 * Output:
 *	numbers of i2c_msgs to transfer:
 *		1: succeed, otherwise: failed
 *********************************************************/
int gtp_i2c_write(struct i2c_client *client, u8 *buf, int len)

{
	unsigned int pos = 0, transfer_length = 0;
	unsigned int address = (buf[0] << 8) + buf[1];
	unsigned char put_buf[64];
	int retry, r = 1;
	struct i2c_msg msg = {
			.addr = client->addr,
			.flags = !I2C_M_RD,
	};

	if (likely(len < sizeof(put_buf))) {
		/* code optimize,use stack memory*/
		msg.buf = &put_buf[0];
	} else {
		msg.buf = kmalloc(len > I2C_MAX_TRANSFER_SIZE
				  ? I2C_MAX_TRANSFER_SIZE : len, GFP_KERNEL);
		if (!msg.buf)
			return -ENOMEM;
	}

	len -= GTP_ADDR_LENGTH;
	while (pos != len) {
		if (unlikely(len - pos > I2C_MAX_TRANSFER_SIZE
						- GTP_ADDR_LENGTH))
			transfer_length = I2C_MAX_TRANSFER_SIZE
						- GTP_ADDR_LENGTH;
		else
			transfer_length = len - pos;
		msg.buf[0] = (unsigned char)((address >> 8) & 0xFF);
		msg.buf[1] = (unsigned char)(address & 0xFF);
		msg.len = transfer_length + 2;
		memcpy(&msg.buf[2], &buf[2 + pos], transfer_length);
		for (retry = 0; retry < RETRY_MAX_TIMES; retry++) {
			if (likely(i2c_transfer(client->adapter,
					&msg, 1) == 1)) {
				pos += transfer_length;
				address += transfer_length;
				break;
			}
			dev_dbg(&client->dev, "I2C write retry[%d]\n",
					retry + 1);
			usleep_range(2000, 2100);
		}
		if (unlikely(retry == RETRY_MAX_TIMES)) {
			dev_err(&client->dev,
				"I2c write failed,dev:%02x,reg:%04x,size:%u\n",
				client->addr, address, len);
			r = -EAGAIN;
			goto write_exit;
		}
	}
write_exit:
	if (len + GTP_ADDR_LENGTH >= sizeof(put_buf))
		kfree(msg.buf);
	return r;
}

/*******************************************************
 * Function:
 *	i2c read twice, compare the results
 * Input:
 *	client:	i2c device
 *	addr: operate address
 *	rxbuf: read data to store, if compare successful
 *	len: bytes to read
 * Output:
 *	FAIL: read failed
 *	SUCCESS: read successful
 *********************************************************/
s32 gtp_i2c_read_dbl_check(struct i2c_client *client,
			   u16 addr, u8 *rxbuf, int len)
{
	u8 buf[16] = {0};
	u8 confirm_buf[16] = {0};
	u8 retry = 0;

	if (len + 2 > sizeof(buf)) {
		dev_warn(&client->dev,
			 "%s, only support length less then %zu\n",
			 __func__, sizeof(buf) - 2);
		return FAIL;
	}
	while (retry++ < 3) {
		memset(buf, 0xAA, 16);
		buf[0] = (u8)(addr >> 8);
		buf[1] = (u8)(addr & 0xFF);
		gtp_i2c_read(client, buf, len + 2);

		memset(confirm_buf, 0xAB, 16);
		confirm_buf[0] = (u8)(addr >> 8);
		confirm_buf[1] = (u8)(addr & 0xFF);
		gtp_i2c_read(client, confirm_buf, len + 2);

		if (!memcmp(buf, confirm_buf, len + 2)) {
			memcpy(rxbuf, confirm_buf + 2, len);
			return SUCCESS;
		}
	}
	dev_err(&client->dev,
		"I2C read 0x%04X, %d bytes, double check failed!\n",
		addr, len);

	return FAIL;
}

/*******************************************************
 * Function:
 *      Send config.
 * Input:
 *      client: i2c device.
 * Output:
 *      result of i2c write operation.
 *              1: succeed, otherwise
 *              0: Not executed
 *		< 0: failed
 *********************************************************/
s32 gtp_send_cfg(struct i2c_client *client)
{
	s32 ret, i;
	u8 check_sum;
	s32 retry = 0;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
	struct goodix_config_data *cfg = &ts->pdata->config;

	if (!cfg->length || !ts->pdata->driver_send_cfg) {
		dev_info(&ts->client->dev,
			 "No config data or error occurred in panel_init\n");
		return 0;
	}

	check_sum = 0;
	for (i = GTP_ADDR_LENGTH; i < cfg->length; i++)
		check_sum += cfg->data[i];
	cfg->data[cfg->length] = (~check_sum) + 1;

	dev_info(&ts->client->dev, "Driver send config\n");
	for (retry = 0; retry < RETRY_MAX_TIMES; retry++) {
		ret = gtp_i2c_write(client, cfg->data,
			GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
		if (ret > 0)
			break;
	}

	return ret;
}

/*******************************************************
 * Function:
 *	Control enable or disable of work thread.
 * Input:
 *      ts: goodix i2c_client private data
 *	enable: enable var.
 *********************************************************/
void gtp_work_control_enable(struct goodix_ts_data *ts, bool enable)
{
	if (enable) {
		set_bit(REPORT_THREAD_ENABLED, &ts->flags);
		dev_dbg(&ts->client->dev, "Input report thread enabled!\n");
	} else {
		clear_bit(REPORT_THREAD_ENABLED, &ts->flags);
		dev_dbg(&ts->client->dev, "Input report thread disabled!\n");
	}
}

static int gtp_gesture_handler(struct goodix_ts_data *ts)
{
	u8 doze_buf[3] = {GTP_REG_DOZE_BUF >> 8, GTP_REG_DOZE_BUF & 0xFF};
	int ret;

	ret = gtp_i2c_read(ts->client, doze_buf, 3);
	if (ret < 0) {
		dev_err(&ts->client->dev, "Failed read doze buf");
		return -EINVAL;
	}

	dev_dbg(&ts->client->dev, "0x814B = 0x%02X", doze_buf[2]);
	if ((doze_buf[2] == 'a') || (doze_buf[2] == 'b') ||
	    (doze_buf[2] == 'c') || (doze_buf[2] == 'd') ||
	    (doze_buf[2] == 'e') || (doze_buf[2] == 'g') ||
	    (doze_buf[2] == 'h') || (doze_buf[2] == 'm') ||
	    (doze_buf[2] == 'o') || (doze_buf[2] == 'q') ||
	    (doze_buf[2] == 's') || (doze_buf[2] == 'v') ||
	    (doze_buf[2] == 'w') || (doze_buf[2] == 'y') ||
	    (doze_buf[2] == 'z') || (doze_buf[2] == 0x5E) ||
	    (doze_buf[2] == 0xAA) || (doze_buf[2] == 0xAB) ||
	    (doze_buf[2] == 0xBA) || (doze_buf[2] == 0xBB) ||
	    (doze_buf[2] == 0xCC)) {
		doze_status = DOZE_WAKEUP;
		input_report_key(ts->input_dev, KEY_POWER, 1);
		input_sync(ts->input_dev);
		input_report_key(ts->input_dev, KEY_POWER, 0);
		input_sync(ts->input_dev);
		/*  clear 0x814B */
		doze_buf[2] = 0x00;
		gtp_i2c_write(ts->client, doze_buf, 3);
	} else {
		/*  clear 0x814B */
		doze_buf[2] = 0x00;
		gtp_i2c_write(ts->client, doze_buf, 3);
		gtp_enter_doze(ts);
	}
	return 0;
}

/*
 * return touch state register value
 * pen event id fixed with 9 and set tool type TOOL_PEN
 *
 */
static u8 gtp_get_points(struct goodix_ts_data *ts,
			 struct goodix_point_t *points,
			 u8 *key_value)
{
	int ret;
	int i;
	u8 *coor_data = NULL;
	u8 finger_state = 0;
	u8 touch_num = 0;
	u8 end_cmd[3] = { GTP_READ_COOR_ADDR >> 8,
			  GTP_READ_COOR_ADDR & 0xFF, 0 };
	u8 point_data[2 + 1 + 8 * GTP_MAX_TOUCH_ID + 1] = {
			GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF };

	ret = gtp_i2c_read(ts->client, point_data, 12);
	if (ret < 0) {
		dev_err(&ts->client->dev,
			"I2C transfer error. errno:%d\n ", ret);
		return 0;
	}
	finger_state = point_data[GTP_ADDR_LENGTH];
	if (finger_state == 0x00)
		return 0;

	touch_num = finger_state & 0x0f;
	if ((finger_state & MASK_BIT_8) == 0 ||
	    touch_num > ts->pdata->max_touch_id) {
		dev_err(&ts->client->dev,
			"Invalid touch state: 0x%x", finger_state);
		finger_state = 0;
		goto exit_get_point;
	}

	if (touch_num > 1) {
		u8 buf[8 * GTP_MAX_TOUCH_ID] = {
					  (GTP_READ_COOR_ADDR + 10) >> 8,
					  (GTP_READ_COOR_ADDR + 10) & 0xff };

		ret = gtp_i2c_read(ts->client, buf, 2 + 8 * (touch_num - 1));
		if (ret < 0) {
			dev_err(&ts->client->dev, "I2C error. %d\n", ret);
			finger_state = 0;
			goto exit_get_point;
		}
		memcpy(&point_data[12], &buf[2], 8 * (touch_num - 1));
	}

	/* panel have touch key */
	/* 0x20_UPKEY 0X10_DOWNKEY 0X40_ALLKEYDOWN */
	*key_value = point_data[3 + 8 * touch_num];

	memset(points, 0, sizeof(*points) * GTP_MAX_TOUCH_ID);
	for (i = 0; i < touch_num; i++) {
		coor_data = &point_data[i * 8 + 3];
		points[i].id = coor_data[0];
		points[i].x = coor_data[1] | (coor_data[2] << 8);
		points[i].y = coor_data[3] | (coor_data[4] << 8);
		points[i].w = coor_data[5] | (coor_data[6] << 8);
		/* if pen hover points[].p must set to zero */
		points[i].p = coor_data[5] | (coor_data[6] << 8);

		if (ts->pdata->swap_x2y)
			GTP_SWAP(points[i].x, points[i].y);

		dev_dbg(&ts->client->dev, "[%d][%d %d %d]\n",
			points[i].id, points[i].x, points[i].y, points[i].p);

		/* pen device coordinate */
		if (points[i].id & 0x80) {
			points[i].tool_type = GTP_TOOL_PEN;
			points[i].id = 10;
			if (ts->pdata->pen_suppress_finger) {
				points[0] = points[i];
				memset(++points, 0, sizeof(*points) *
					(GTP_MAX_TOUCH_ID - 1));
				finger_state &= 0xf0;
				finger_state |= 0x01;
				break;
			}
		} else {
			points[i].tool_type = GTP_TOOL_FINGER;
		}
	}

exit_get_point:
	if (!test_bit(RAW_DATA_MODE, &ts->flags)) {
		ret = gtp_i2c_write(ts->client, end_cmd, 3);
		if (ret < 0)
			dev_info(&ts->client->dev, "I2C write end_cmd error!");
	}
	return finger_state;
}

static void gtp_type_a_report(struct goodix_ts_data *ts, u8 touch_num,
			      struct goodix_point_t *points)
{
	int i;
	u16 cur_touch = 0;
	static u16 pre_touch;
	static u8 pre_pen_id;

	if (touch_num)
		input_report_key(ts->input_dev, BTN_TOUCH, 1);

	for (i = 0; i < ts->pdata->max_touch_id; i++) {
		if (touch_num && i == points->id) {
			input_report_abs(ts->input_dev,
					ABS_MT_TRACKING_ID, points->id);

			if (points->tool_type == GTP_TOOL_PEN) {
				input_report_key(ts->input_dev,
							BTN_TOOL_PEN, true);
				pre_pen_id = points->id;
			} else {
				input_report_key(ts->input_dev,
							BTN_TOOL_FINGER, true);
			}
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
					 points->x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
					 points->y);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
					 points->w);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
					 points->p);
			input_mt_sync(ts->input_dev);

			cur_touch |= 0x01 << points->id;
			points++;
		} else if (pre_touch & 0x01 << i) {
			if (pre_pen_id == i) {
				input_report_key(ts->input_dev,
							BTN_TOOL_PEN, false);
/* valid id will < 10, so id to 0xff to indicate a invalid state */
				pre_pen_id = 0xff;
			} else {
				input_report_key(ts->input_dev,
					BTN_TOOL_FINGER, false);
			}
		}
	}

	pre_touch = cur_touch;
	if (!pre_touch) {
		input_mt_sync(ts->input_dev);
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
	}
	input_sync(ts->input_dev);
}

static void gtp_mt_slot_report(struct goodix_ts_data *ts, u8 touch_num,
			       struct goodix_point_t *points)
{
	int i;
	u16 cur_touch = 0;
	static u16 pre_touch;
	static u8 pre_pen_id;

	for (i = 0; i < ts->pdata->max_touch_id; i++) {
		if (touch_num && i == points->id) {
			input_mt_slot(ts->input_dev, points->id);

			if (points->tool_type == GTP_TOOL_PEN) {
				input_mt_report_slot_state(ts->input_dev,
							   MT_TOOL_PEN, true);
				pre_pen_id = points->id;
			} else {
				input_mt_report_slot_state(ts->input_dev,
						MT_TOOL_FINGER, true);
			}
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X,
					 points->x);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y,
					 points->y);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR,
					 points->w);
			input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
					 points->p);

			cur_touch |= 0x01 << points->id;
			points++;
		} else if (pre_touch & 0x01 << i) {
			input_mt_slot(ts->input_dev, i);
			if (pre_pen_id == i) {
				input_mt_report_slot_state(ts->input_dev,
							   MT_TOOL_PEN, false);
				/* valid id will < 10, so set id to 0xff to
				 * indicate a invalid state
				 */
				pre_pen_id = 0xff;
			} else {
				input_mt_report_slot_state(ts->input_dev,
						MT_TOOL_FINGER, false);
			}
		}
	}

	pre_touch = cur_touch;
	/* report BTN_TOUCH event */
	input_mt_sync_frame(ts->input_dev);
	input_sync(ts->input_dev);
}

/*******************************************************
 * Function:
 *	Goodix touchscreen sensor report function
 * Input:
 *	ts: goodix tp private data
 * Output:
 *	None.
 *********************************************************/
static void gtp_work_func(struct goodix_ts_data *ts)
{
	u8 point_state = 0;
	u8 key_value = 0;
	s32 i = 0;
	s32 ret = -1;
	static u8 pre_key;
	struct goodix_point_t points[GTP_MAX_TOUCH_ID];

	if (test_bit(PANEL_RESETTING, &ts->flags))
		return;
	if (!test_bit(REPORT_THREAD_ENABLED, &ts->flags))
		return;

	/* gesture event */
	if (ts->pdata->slide_wakeup && test_bit(DOZE_MODE, &ts->flags)) {
		ret =  gtp_gesture_handler(ts);
		if (ret)
			dev_err(&ts->client->dev,
				"Failed handler gesture event %d\n", ret);
		return;
	}

	point_state = gtp_get_points(ts, points, &key_value);
	if (!point_state) {
		dev_dbg(&ts->client->dev, "Invalid finger points\n");
		return;
	}

	/* touch key event */
	if (key_value & 0xf0 || pre_key & 0xf0) {
		/* pen button */
		switch (key_value) {
		case 0x40:
			input_report_key(ts->input_dev, GTP_PEN_BUTTON1, 1);
			input_report_key(ts->input_dev, GTP_PEN_BUTTON2, 1);
			break;
		case 0x10:
			input_report_key(ts->input_dev, GTP_PEN_BUTTON1, 1);
			input_report_key(ts->input_dev, GTP_PEN_BUTTON2, 0);
			dev_dbg(&ts->client->dev, "pen button1 down\n");
			break;
		case 0x20:
			input_report_key(ts->input_dev, GTP_PEN_BUTTON1, 0);
			input_report_key(ts->input_dev, GTP_PEN_BUTTON2, 1);
			break;
		default:
			input_report_key(ts->input_dev, GTP_PEN_BUTTON1, 0);
			input_report_key(ts->input_dev, GTP_PEN_BUTTON2, 0);
			dev_dbg(&ts->client->dev, "button1 up\n");
			break;
		}
		input_sync(ts->input_dev);
		pre_key = key_value;
	} else if (key_value & 0x0f || pre_key & 0x0f) {
		/* panel key */
		for (i = 0; i < ts->pdata->key_nums; i++) {
			if ((pre_key | key_value) & (0x01 << i))
				input_report_key(ts->input_dev,
						 ts->pdata->key_map[i],
						 key_value & (0x01 << i));
		}
		input_sync(ts->input_dev);
		pre_key = key_value;
	}

	if (!ts->pdata->type_a_report)
		gtp_mt_slot_report(ts, point_state & 0x0f, points);
	else
		gtp_type_a_report(ts, point_state & 0x0f, points);
}

/*******************************************************
 * Function:
 *	Timer interrupt service routine for polling mode.
 * Input:
 *	timer: timer struct pointer
 * Output:
 *	Timer work mode.
 * HRTIMER_NORESTART:
 *	no restart mode
 *********************************************************/
static enum hrtimer_restart gtp_timer_handler(struct hrtimer *timer)
{
	struct goodix_ts_data *ts =
		container_of(timer, struct goodix_ts_data, timer);

	gtp_work_func(ts);
	hrtimer_start(&ts->timer, ktime_set(0, (GTP_POLL_TIME + 6) * 1000000),
		      HRTIMER_MODE_REL);

	return HRTIMER_NORESTART;
}

static irqreturn_t gtp_irq_handler(int irq, void *dev_id)
{
	struct goodix_ts_data *ts = dev_id;

	gtp_work_func(ts);
	return IRQ_HANDLED;
}

void gtp_int_output(struct goodix_ts_data *ts, int level)
{
	if (!ts->pdata->int_sync)
		return;

	if (level == 0) {
		if (ts->pinctrl.pinctrl)
			pinctrl_select_state(ts->pinctrl.pinctrl,
					     ts->pinctrl.int_out_low);
		else if (gpio_is_valid(ts->pdata->irq_gpio))
			gpio_direction_output(ts->pdata->irq_gpio, 0);
		else
			dev_err(&ts->client->dev,
				"Failed set int pin output low\n");
	} else {
		if (ts->pinctrl.pinctrl)
			pinctrl_select_state(ts->pinctrl.pinctrl,
					     ts->pinctrl.int_out_high);
		else if (gpio_is_valid(ts->pdata->irq_gpio))
			gpio_direction_output(ts->pdata->irq_gpio, 1);
		else
			dev_err(&ts->client->dev,
				"Failed set int pin output high\n");
	}
}

void gtp_int_sync(struct goodix_ts_data *ts, s32 ms)
{
	if (!ts->pdata->int_sync)
		return;

	if (ts->pinctrl.pinctrl) {
		gtp_int_output(ts, 0);
		msleep(ms);
		pinctrl_select_state(ts->pinctrl.pinctrl,
				     ts->pinctrl.int_input);
	} else if (gpio_is_valid(ts->pdata->irq_gpio)) {
		gpio_direction_output(ts->pdata->irq_gpio, 0);
		msleep(ms);
		gpio_direction_input(ts->pdata->irq_gpio);
	} else {
		dev_err(&ts->client->dev, "Failed sync int pin\n");
	}
}

void gtp_rst_output(struct goodix_ts_data *ts, int level)
{
	if (level == 0) {
		if (ts->pinctrl.pinctrl)
			pinctrl_select_state(ts->pinctrl.pinctrl,
					     ts->pinctrl.rst_out_low);
		else if (gpio_is_valid(ts->pdata->rst_gpio))
			gpio_direction_output(ts->pdata->rst_gpio, 0);
		else
			dev_err(&ts->client->dev,
				"Failed set rst pin output low\n");
	} else {
		if (ts->pinctrl.pinctrl)
			pinctrl_select_state(ts->pinctrl.pinctrl,
					     ts->pinctrl.rst_out_high);
		else if (gpio_is_valid(ts->pdata->rst_gpio))
			gpio_direction_output(ts->pdata->rst_gpio, 1);
		else
			dev_err(&ts->client->dev,
				"Failed set rst pin output high\n");
	}
}

void gtp_rst_input(struct goodix_ts_data *ts)
{
	if (ts->pinctrl.pinctrl)
		pinctrl_select_state(ts->pinctrl.pinctrl,
					 ts->pinctrl.rst_input);
	else if (gpio_is_valid(ts->pdata->rst_gpio))
		gpio_direction_input(ts->pdata->rst_gpio);
	else
		dev_err(&ts->client->dev,
			"Failed set rst pin input\n");
}

/*******************************************************
 * Function:
 *	Reset chip. Control the reset pin and int-pin(if
 *	defined),
 * Input:
 *	client:	i2c device.
 *	ms: reset time in millisecond
 * Output:
 *	None.
 *******************************************************/
void gtp_reset_guitar(struct i2c_client *client, s32 ms)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	dev_info(&client->dev, "Guitar reset");
	set_bit(PANEL_RESETTING, &ts->flags);
	if (!gpio_is_valid(ts->pdata->rst_gpio)) {
		dev_warn(&client->dev, "reset failed no valid reset gpio");
		return;
	}

	gtp_rst_output(ts, 0);
	usleep_range(ms * 1000, ms * 1000 + 100);	/*  T2: > 10ms */

	gtp_int_output(ts, client->addr == 0x14);

	usleep_range(2000, 3000);		/*  T3: > 100us (2ms)*/
	gtp_rst_output(ts, 1);

	usleep_range(6000, 7000);		/*  T4: > 5ms */
	gtp_rst_input(ts);

	gtp_int_sync(ts, 50);
	if (ts->pdata->esd_protect)
		gtp_init_ext_watchdog(client);

	clear_bit(PANEL_RESETTING, &ts->flags);
}

/*******************************************************
 * Function:
 *	Enter doze mode for sliding wakeup.
 * Input:
 *	ts: goodix tp private data
 * Output:
 *	1: succeed, otherwise failed
 *******************************************************/
static int gtp_enter_doze(struct goodix_ts_data *ts)
{
	int ret = -1;
	int retry = 0;
	u8 i2c_control_buf[3] = { (u8)(GTP_REG_COMMAND >> 8),
				  (u8)GTP_REG_COMMAND, 8 };

	/*  resend doze command
	 * if (test_and_set_bit(DOZE_MODE, &ts->flags)) {
	 *	dev_info(&ts->client->dev, "Already in doze mode\n");
	 *	return SUCCESS;
	 * }
	 */
	set_bit(DOZE_MODE, &ts->flags);
	dev_dbg(&ts->client->dev, "Entering gesture mode.");
	while (retry++ < 5) {
		i2c_control_buf[0] = (u8)(GTP_REG_COMMAND_CHECK >> 8);
		i2c_control_buf[1] = (u8)GTP_REG_COMMAND_CHECK;
		ret = gtp_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret < 0) {
			dev_dbg(&ts->client->dev,
				"failed to set doze flag into 0x8046, %d\n",
				retry);
			continue;
		}
		i2c_control_buf[0] = (u8)(GTP_REG_COMMAND >> 8);
		i2c_control_buf[1] = (u8)GTP_REG_COMMAND;
		ret = gtp_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret > 0) {
			dev_dbg(&ts->client->dev, "Gesture mode enabled\n");
			return ret;
		}
		usleep_range(10000, 11000);
	}

	dev_err(&ts->client->dev, "Failed enter doze mode\n");
	clear_bit(DOZE_MODE, &ts->flags);
	return ret;
}

static s8 gtp_enter_sleep(struct goodix_ts_data *ts)
{
	s8 ret = -1;
	s8 retry = 0;
	u8 i2c_control_buf[3] = { (u8)(GTP_REG_COMMAND >> 8),
				  (u8)GTP_REG_COMMAND, 5 };

	gtp_int_output(ts, 0);
	usleep_range(5000, 6000);

	while (retry++ < 5) {
		ret = gtp_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret > 0) {
			dev_info(&ts->client->dev, "Enter sleep mode\n");

			return ret;
		}
		usleep_range(10000, 11000);
	}
	dev_err(&ts->client->dev, "Failed send sleep cmd\n");

	return ret;
}

static int gtp_wakeup_sleep(struct goodix_ts_data *ts)
{
	u8 retry = 0;
	int ret = -1;

	while (retry++ < 10) {
		gtp_int_output(ts, 1);
		usleep_range(5000, 6000);

		ret = gtp_i2c_test(ts->client);
		if (!ret) {
			dev_dbg(&ts->client->dev, "Success wakeup sleep\n");

			gtp_int_sync(ts, 25);
			if (ts->pdata->esd_protect)
				gtp_init_ext_watchdog(ts->client);

			return ret;
		}
		gtp_reset_guitar(ts->client, 20);
	}

	dev_err(&ts->client->dev, "Failed wakeup from sleep mode\n");
	return -EINVAL;
}

static int gtp_find_valid_cfg_data(struct goodix_ts_data *ts)
{
	int ret = -1;
	u8 sensor_id = 0;
	struct goodix_config_data *cfg = &ts->pdata->config;

	/* if defined CONFIG_OF, parse config data from dtsi
	 * else parse config data form header file.
	 */
	cfg->length = 0;

#ifndef	CONFIG_OF
	u8 cfg_info_group0[] = CTP_CFG_GROUP0;
	u8 cfg_info_group1[] = CTP_CFG_GROUP1;
	u8 cfg_info_group2[] = CTP_CFG_GROUP2;
	u8 cfg_info_group3[] = CTP_CFG_GROUP3;
	u8 cfg_info_group4[] = CTP_CFG_GROUP4;
	u8 cfg_info_group5[] = CTP_CFG_GROUP5;

	u8 *send_cfg_buf[] = { cfg_info_group0, cfg_info_group1,
			       cfg_info_group2, cfg_info_group3,
			       cfg_info_group4, cfg_info_group5 };
	u8 cfg_info_len[] = { CFG_GROUP_LEN(cfg_info_group0),
			      CFG_GROUP_LEN(cfg_info_group1),
			      CFG_GROUP_LEN(cfg_info_group2),
			      CFG_GROUP_LEN(cfg_info_group3),
			      CFG_GROUP_LEN(cfg_info_group4),
			      CFG_GROUP_LEN(cfg_info_group5)};

	dev_dbg(&ts->client->dev,
		"Config Groups\' Lengths: %d, %d, %d, %d, %d, %d",
		cfg_info_len[0], cfg_info_len[1], cfg_info_len[2],
		cfg_info_len[3], cfg_info_len[4], cfg_info_len[5]);
#endif

	/* read sensor id */
	ret = gtp_i2c_read_dbl_check(ts->client, GTP_REG_SENSOR_ID,
				     &sensor_id, 1);
	if (ret != SUCCESS || sensor_id >= 0x06) {
		dev_err(&ts->client->dev,
			"Failed get valid sensor_id(0x%02X), No Config Sent\n",
			sensor_id);
		return -EINVAL;
	}

	dev_dbg(&ts->client->dev, "Sensor_ID: %d", sensor_id);
	/* parse config data */
#ifdef CONFIG_OF
	dev_dbg(&ts->client->dev, "Get config data from device tree\n");
	ret = gtp_parse_dt_cfg(&ts->client->dev,
			       &cfg->data[GTP_ADDR_LENGTH],
			       &cfg->length, sensor_id);
	if (ret < 0) {
		dev_err(&ts->client->dev,
			"Failed to parse config data form device tree\n");
		cfg->length = 0;
		return -EPERM;
	}
#else
	dev_dbg(&ts->client->dev, "Get config data from header file\n");
	if ((!cfg_info_len[1]) && (!cfg_info_len[2]) &&
	    (!cfg_info_len[3]) && (!cfg_info_len[4]) &&
	    (!cfg_info_len[5])) {
		sensor_id = 0;
	}
	cfg->length = cfg_info_len[sensor_id];
	memset(&cfg->data[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
	memcpy(&cfg->data[GTP_ADDR_LENGTH], send_cfg_buf[sensor_id],
	       cfg->length);
#endif

	if (cfg->length < GTP_CONFIG_MIN_LENGTH) {
		dev_err(&ts->client->dev,
			"Failed get valid config data with sensor id %d\n",
			sensor_id);
		cfg->length = 0;
		return -EPERM;
	}

	dev_info(&ts->client->dev, "Config group%d used,length: %d\n",
		 sensor_id, cfg->length);

	return 0;
}

/*******************************************************
 * Function:
 *	Get valid config data from dts or .h file.
 *	Read firmware version info and judge firmware
 *	working state
 * Input:
 *	ts: goodix private data
 * Output:
 *	Executive outcomes.
 *		0: succeed, otherwise: failed
 *******************************************************/
static s32 gtp_init_panel(struct goodix_ts_data *ts)
{
	s32 ret = -1;
	u8 opr_buf[16] = {0};
	u8 drv_cfg_version = 0;
	u8 flash_cfg_version = 0;
	struct goodix_config_data *cfg = &ts->pdata->config;

	if (!ts->pdata->driver_send_cfg) {
		dev_info(&ts->client->dev, "Driver set not send config\n");
		cfg->length = GTP_CONFIG_MAX_LENGTH;
		ret = gtp_i2c_read(ts->client,
				   cfg->data, cfg->length +
				   GTP_ADDR_LENGTH);
		if (ret < 0)
			dev_err(&ts->client->dev, "Read origin Config Failed\n");

		return 0;
	}

	gtp_find_valid_cfg_data(ts);

	/* check firmware */
	ret = gtp_i2c_read_dbl_check(ts->client, 0x41E4, opr_buf, 1);
	if (ret == SUCCESS) {
		if (opr_buf[0] != 0xBE) {
			set_bit(FW_ERROR, &ts->flags);
			dev_err(&ts->client->dev,
				"Firmware error, no config sent!\n");
			return -EINVAL;
		}
	}

	ret = gtp_i2c_read_dbl_check(ts->client, GTP_REG_CONFIG_DATA,
				     &opr_buf[0], 1);
	if (ret == SUCCESS) {
		dev_dbg(&ts->client->dev,
			"Config Version: %d; IC Config Version: %d\n",
			cfg->data[GTP_ADDR_LENGTH], opr_buf[0]);
		flash_cfg_version = opr_buf[0];
		drv_cfg_version = cfg->data[GTP_ADDR_LENGTH];

		if (flash_cfg_version < 120 &&
		    flash_cfg_version > drv_cfg_version)
			cfg->data[GTP_ADDR_LENGTH] = 0x00;
	} else {
		dev_err(&ts->client->dev,
			"Failed to get ic config version!No config sent\n");
		return -EPERM;
	}

	ret = gtp_send_cfg(ts->client);
	if (ret < 0)
		dev_err(&ts->client->dev, "Send config error\n");
	else
		usleep_range(10000, 11000); /* 10 ms */

	/* restore config version */
	cfg->data[GTP_ADDR_LENGTH] = drv_cfg_version;

	return 0;
}

static ssize_t gtp_config_read_proc(struct file *file, char __user *page,
				    size_t size, loff_t *ppos)
{
	int i, ret;
	char *ptr;
	size_t data_len = 0;
	char temp_data[GTP_CONFIG_MAX_LENGTH + 2] = {
					(u8)(GTP_REG_CONFIG_DATA >> 8),
					(u8)GTP_REG_CONFIG_DATA };
	struct goodix_ts_data *ts = i2c_get_clientdata(i2c_connect_client);
	struct goodix_config_data *cfg = &ts->pdata->config;

	ptr = kzalloc(4096, GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	data_len += snprintf(ptr + data_len, 4096 - data_len,
			     "====init value====\n");
	for (i = 0 ; i < GTP_CONFIG_MAX_LENGTH ; i++) {
		data_len += snprintf(ptr + data_len, 4096 - data_len,
				     "0x%02X ", cfg->data[i + 2]);

		if (i % 8 == 7)
			data_len += snprintf(ptr + data_len,
					     4096 - data_len, "\n");
	}
	data_len += snprintf(ptr + data_len, 4096 - data_len, "\n");

	data_len += snprintf(ptr + data_len, 4096 - data_len,
			     "====real value====\n");
	ret = gtp_i2c_read(i2c_connect_client, temp_data,
			   GTP_CONFIG_MAX_LENGTH + 2);
	if (ret < 0) {
		data_len += snprintf(ptr + data_len, 4096 - data_len,
				     "Failed read real config data\n");
	} else {
		for (i = 0; i < GTP_CONFIG_MAX_LENGTH; i++) {
			data_len += snprintf(ptr + data_len, 4096 - data_len,
					     "0x%02X ", temp_data[i + 2]);

			if (i % 8 == 7)
				data_len += snprintf(ptr + data_len,
						     4096 - data_len, "\n");
		}
	}

	data_len = simple_read_from_buffer(page, size, ppos, ptr, data_len);
	kfree(ptr);
	ptr = NULL;
	return data_len;
}

int gtp_ascii_to_array(const u8 *src_buf, int src_len, u8 *dst_buf)
{
	int i, ret;
	int cfg_len = 0;
	long val;
	char temp_buf[5];

	for (i = 0; i < src_len;) {
		if (src_buf[i] == ' ' || src_buf[i] == '\r' ||
		    src_buf[i] == '\n') {
			i++;
			continue;
		}

		temp_buf[0] = src_buf[i];
		temp_buf[1] = src_buf[i + 1];
		temp_buf[2] = src_buf[i + 2];
		temp_buf[3] = src_buf[i + 3];
		temp_buf[4] = '\0';
		if (!kstrtol(temp_buf, 16, &val)) {
			if (cfg_len < GTP_CONFIG_MAX_LENGTH) {
				dst_buf[cfg_len++] = val & 0xFF;
				i += 5;
			} else {
				ret = -2;
				goto convert_failed;
			}
		} else {
			ret = -3;
			goto convert_failed;
		}
	}
	return cfg_len;

convert_failed:
	return ret;
}

static ssize_t gtp_config_write_proc(struct file *filp,
				     const char __user *buffer,
				     size_t count, loff_t *off)
{
	u8 *temp_buf;
	u8 *file_config;
	int file_cfg_len;
	s32 ret = 0, i;
	struct goodix_ts_data *ts = i2c_get_clientdata(i2c_connect_client);

	dev_dbg(&ts->client->dev, "write count %zu\n", count);

	if (count > PAGE_SIZE) {
		dev_err(&ts->client->dev, "config to long %zu\n", count);
		return -EFAULT;
	}

	temp_buf = kzalloc(count, GFP_KERNEL);
	if (!temp_buf)
		return -ENOMEM;

	file_config = kzalloc(GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH,
			      GFP_KERNEL);
	if (!file_config) {
		kfree(temp_buf);
		return -ENOMEM;
	}
	file_config[0] = GTP_REG_CONFIG_DATA >> 8;
	file_config[1] = GTP_REG_CONFIG_DATA & 0xff;

	if (copy_from_user(temp_buf, buffer, count)) {
		dev_err(&ts->client->dev, "Failed copy from user\n");
		ret = -EFAULT;
		goto send_cfg_err;
	}

	file_cfg_len = gtp_ascii_to_array(temp_buf, (int)count,
					  &file_config[GTP_ADDR_LENGTH]);
	if (file_cfg_len < 0) {
		dev_err(&ts->client->dev, "failed covert ascii to hex");
		ret = -EFAULT;
		goto send_cfg_err;
	}

	GTP_DEBUG_ARRAY(file_config + GTP_ADDR_LENGTH, file_cfg_len);

	i = 0;
	while (i++ < 5) {
		ret = gtp_i2c_write(ts->client, file_config, file_cfg_len + 2);
		if (ret > 0) {
			dev_info(&ts->client->dev, "Send config SUCCESS.");
			break;
		}
		dev_err(&ts->client->dev, "Send config i2c error.");
		ret = -EFAULT;
		goto send_cfg_err;
	}

	ret = count;
send_cfg_err:
	kfree(temp_buf);
	kfree(file_config);
	return ret;
}

static const struct file_operations config_proc_ops = {
	.owner = THIS_MODULE,
	.read = gtp_config_read_proc,
	.write = gtp_config_write_proc,
};

static ssize_t gtp_workmode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	size_t data_len = 0;
	struct goodix_ts_data *data = dev_get_drvdata(dev);

	if (test_bit(DOZE_MODE, &data->flags))
		data_len = scnprintf(buf, PAGE_SIZE, "%s\n",
				     "doze_mode");
	else if (test_bit(SLEEP_MODE, &data->flags))
		data_len = scnprintf(buf, PAGE_SIZE, "%s\n",
				     "sleep_mode");
	else
		data_len = scnprintf(buf, PAGE_SIZE, "%s\n",
				     "normal_mode");

	return data_len;
}
static DEVICE_ATTR(workmode, 0444, gtp_workmode_show, NULL);

#ifdef CONFIG_TOUCHSCREEN_GT9XX_UPDATE
#define FW_NAME_MAX_LEN	80
static ssize_t gtp_dofwupdate_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct goodix_ts_data *ts = dev_get_drvdata(dev);
	char update_file_name[FW_NAME_MAX_LEN];
	int retval;

	if (count > FW_NAME_MAX_LEN) {
		dev_info(&ts->client->dev, "FW filename is too long\n");
		retval = -EINVAL;
		goto exit;
	}

	strlcpy(update_file_name, buf, count);

	ts->force_update = true;
	retval = gup_update_proc(update_file_name);
	if (retval == FAIL)
		dev_err(&ts->client->dev, "Fail to update GTP firmware.\n");
	else
		dev_info(&ts->client->dev, "Update success\n");

	return count;

exit:
	return retval;
}
static DEVICE_ATTR(dofwupdate, 0664, NULL, gtp_dofwupdate_store);
#endif

static ssize_t gtp_productinfo_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_data *data = dev_get_drvdata(dev);
	struct goodix_fw_info *fw_info = &data->fw_info;

	return scnprintf(buf, PAGE_SIZE, "GT%s_%x_%d\n",
			 fw_info->pid, fw_info->version, fw_info->sensor_id);
}
static DEVICE_ATTR(productinfo, 0444, gtp_productinfo_show, NULL);

static ssize_t gtp_drv_irq_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	unsigned long value = 0;
	int err = 0;
	struct goodix_ts_data *data = dev_get_drvdata(dev);

	err = kstrtoul(buf, 10, &value);
	if (err < 0) {
		dev_err(dev, "Failed to convert value\n");
		return -EINVAL;
	}

	switch (value) {
	case 0:
		/* Disable irq */
		gtp_work_control_enable(data, false);
		break;
	case 1:
		/* Enable irq */
		gtp_work_control_enable(data, true);
		break;
	default:
		dev_err(dev, "Invalid value\n");
		return -EINVAL;
	}

	return count;
}

static ssize_t gtp_drv_irq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct goodix_ts_data *data = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n",
			 test_bit(REPORT_THREAD_ENABLED, &data->flags)
			 ? "enabled" : "disabled");
}
static DEVICE_ATTR(drv_irq, 0664, gtp_drv_irq_show, gtp_drv_irq_store);

static ssize_t gtp_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct goodix_ts_data *data = dev_get_drvdata(dev);

	if ('1' != buf[0]) {
		dev_err(dev, "Invalid argument for reset\n");
		return -EINVAL;
	}

	gtp_reset_guitar(data->client, 20);

	return count;
}
static DEVICE_ATTR(reset, 0220, NULL, gtp_reset_store);

static struct attribute *gtp_attrs[] = {
	&dev_attr_workmode.attr,
	&dev_attr_productinfo.attr,

#ifdef CONFIG_TOUCHSCREEN_GT9XX_UPDATE
	&dev_attr_dofwupdate.attr,
#endif

	&dev_attr_drv_irq.attr,
	&dev_attr_reset.attr,
	NULL
};

static const struct attribute_group gtp_attr_group = {
	.attrs = gtp_attrs,
};

static int gtp_create_file(struct goodix_ts_data *ts)
{
	int ret;
	struct i2c_client *client = ts->client;

	/*  Create proc file system */
	gtp_config_proc = NULL;
	gtp_config_proc = proc_create(GT91XX_CONFIG_PROC_FILE, 0664,
				      NULL, &config_proc_ops);
	if (!gtp_config_proc)
		dev_err(&client->dev, "create_proc_entry %s failed\n",
			GT91XX_CONFIG_PROC_FILE);
	else
		dev_info(&client->dev, "create proc entry %s success\n",
			 GT91XX_CONFIG_PROC_FILE);

	ret = sysfs_create_group(&client->dev.kobj, &gtp_attr_group);
	if (ret) {
		dev_err(&client->dev, "Failure create sysfs group %d\n", ret);
		/*TODO: debug change */
		goto exit_free_config_proc;
	}
	return 0;

exit_free_config_proc:
	remove_proc_entry(GT91XX_CONFIG_PROC_FILE, gtp_config_proc);
	return -ENODEV;
}

s32 gtp_get_fw_info(struct i2c_client *client, struct goodix_fw_info *fw_info)
{
	s32 ret = -1;
	u8 buf[8] = {GTP_REG_VERSION >> 8, GTP_REG_VERSION & 0xff};

	ret = gtp_i2c_read(client, buf, sizeof(buf));
	if (ret < 0) {
		dev_err(&client->dev, "Failed read fw_info\n");
		return ret;
	}

	/* product id */
	memset(fw_info, 0, sizeof(*fw_info));

	if (buf[5] == 0x00) {
		memcpy(fw_info->pid, buf + GTP_ADDR_LENGTH, 3);
		dev_info(&client->dev, "IC Version: %c%c%c_%02X%02X\n",
			 buf[2], buf[3], buf[4], buf[7], buf[6]);
	} else {
		memcpy(fw_info->pid, buf + GTP_ADDR_LENGTH, 4);
		dev_info(&client->dev, "IC Version: %c%c%c%c_%02X%02X\n",
			 buf[2], buf[3], buf[4], buf[5], buf[7], buf[6]);
	}

	/* current firmware version */
	fw_info->version = (buf[7] << 8) | buf[6];

	/* read sensor id */
	fw_info->sensor_id = 0xff;
	ret = gtp_i2c_read_dbl_check(client, GTP_REG_SENSOR_ID,
				     &fw_info->sensor_id, 1);
	if (ret != SUCCESS || fw_info->sensor_id >= 0x06) {
		dev_err(&client->dev,
			"Failed get valid sensor_id(0x%02X), No Config Sent\n",
			fw_info->sensor_id);

		fw_info->sensor_id = 0xff;
	}

	return ret;
}

static int gtp_i2c_test(struct i2c_client *client)
{
	u8 test[3] = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};
	u8 retry = 0;
	int ret = -1;

	while (retry++ < 3) {
		ret = gtp_i2c_read(client, test, 3);
		if (ret == 2)
			return 0;

		dev_err(&client->dev, "GTP i2c test failed time %d\n", retry);
		usleep_range(10000, 11000); /* 10 ms */
	}

	return -EAGAIN;
}

static int gtp_pinctrl_init(struct goodix_ts_data *ts)
{
	struct goodix_pinctrl *pinctrl = &ts->pinctrl;

	pinctrl->pinctrl = devm_pinctrl_get(&ts->client->dev);
	if (IS_ERR_OR_NULL(pinctrl->pinctrl)) {
		dev_info(&ts->client->dev, "No pinctrl found\n");
		pinctrl->pinctrl = NULL;
		return 0;
	}

	/* INT pinctrl */
	pinctrl->int_default = pinctrl_lookup_state(pinctrl->pinctrl,
						    "gdix_ts_int_default");
	if (IS_ERR_OR_NULL(pinctrl->int_default)) {
		dev_info(&ts->client->dev,
			 "Failed get pinctrl state:INT default state\n");
		goto exit_pinctrl_init;
	}

	pinctrl->int_out_high = pinctrl_lookup_state(pinctrl->pinctrl,
						     "gdix_ts_int_output_high");
	if (IS_ERR_OR_NULL(pinctrl->int_out_high)) {
		dev_info(&ts->client->dev,
			 "Failed get pinctrl state:INT output_high\n");
		goto exit_pinctrl_init;
	}

	pinctrl->int_out_low = pinctrl_lookup_state(pinctrl->pinctrl,
						    "gdix_ts_int_output_low");
	if (IS_ERR_OR_NULL(pinctrl->int_out_low)) {
		dev_info(&ts->client->dev,
			 "Failed get pinctrl state:INT output_low\n");
		goto exit_pinctrl_init;
	}

	pinctrl->int_input = pinctrl_lookup_state(pinctrl->pinctrl,
						  "gdix_ts_int_input");
	if (IS_ERR_OR_NULL(pinctrl->int_input)) {
		dev_info(&ts->client->dev,
			 "Failed get pinctrl state:int-input\n");
		goto exit_pinctrl_init;
	}
	dev_info(&ts->client->dev, "Success init INT pinctrl\n");

	/* RST pinctrl */
	pinctrl->rst_default = pinctrl_lookup_state(pinctrl->pinctrl,
						    "gdix_ts_rst_default");
	if (IS_ERR_OR_NULL(pinctrl->rst_default)) {
		dev_info(&ts->client->dev,
			 "Failed get pinctrl state:RST default state\n");
		goto exit_pinctrl_init;
	}

	pinctrl->rst_out_high = pinctrl_lookup_state(pinctrl->pinctrl,
						     "gdix_ts_rst_output_high");
	if (IS_ERR_OR_NULL(pinctrl->rst_out_high)) {
		dev_info(&ts->client->dev,
			 "Failed get pinctrl state:RST output_high\n");
		goto exit_pinctrl_init;
	}

	pinctrl->rst_out_low = pinctrl_lookup_state(pinctrl->pinctrl,
						    "gdix_ts_rst_output_low");
	if (IS_ERR_OR_NULL(pinctrl->rst_out_low)) {
		dev_info(&ts->client->dev,
			 "Failed get pinctrl state:RST output_low\n");
		goto exit_pinctrl_init;
	}

	pinctrl->rst_input = pinctrl_lookup_state(pinctrl->pinctrl,
						  "gdix_ts_rst_input");
	if (IS_ERR_OR_NULL(pinctrl->rst_input)) {
		dev_info(&ts->client->dev,
			 "Failed get pinctrl state:rst-input\n");
		goto exit_pinctrl_init;
	}
	dev_info(&ts->client->dev, "Success init RST pinctrl\n");

	return 0;
exit_pinctrl_init:
	devm_pinctrl_put(pinctrl->pinctrl);
	pinctrl->pinctrl = NULL;
	pinctrl->int_default = NULL;
	pinctrl->int_out_high = NULL;
	pinctrl->int_out_low = NULL;
	pinctrl->int_input = NULL;
	pinctrl->rst_default = NULL;
	pinctrl->rst_out_high = NULL;
	pinctrl->rst_out_low = NULL;
	pinctrl->rst_input = NULL;
	return -EINVAL;
}

static void gtp_pinctrl_deinit(struct goodix_ts_data *ts)
{
	if (ts->pinctrl.pinctrl)
		devm_pinctrl_put(ts->pinctrl.pinctrl);
}

static int gtp_request_io_port(struct goodix_ts_data *ts)
{
	int ret = 0;

	if (gpio_is_valid(ts->pdata->irq_gpio)) {
		ret = gpio_request(ts->pdata->irq_gpio, "goodix_ts_int");
		if (ret < 0) {
			dev_err(&ts->client->dev,
				"Failed to request GPIO:%d, ERRNO:%d\n",
				(s32)ts->pdata->irq_gpio, ret);
			return -ENODEV;
		}

		gpio_direction_input(ts->pdata->irq_gpio);
		dev_info(&ts->client->dev, "Success request irq-gpio\n");
	}

	if (gpio_is_valid(ts->pdata->rst_gpio)) {
		ret = gpio_request(ts->pdata->rst_gpio, "goodix_ts_rst");
		if (ret < 0) {
			dev_err(&ts->client->dev,
				"Failed to request GPIO:%d, ERRNO:%d\n",
				(s32)ts->pdata->rst_gpio, ret);

			if (gpio_is_valid(ts->pdata->irq_gpio))
				gpio_free(ts->pdata->irq_gpio);

			return -ENODEV;
		}

		gpio_direction_input(ts->pdata->rst_gpio);
		dev_info(&ts->client->dev,  "Success request rst-gpio\n");
	}

	return 0;
}

/*******************************************************
 * Function:
 *	Request interrupt if define irq pin, else use hrtimer
 *	as interrupt source
 * Input:
 *	ts: private data.
 * Output:
 *	Executive outcomes.
 *		0: succeed, -1: failed.
 *******************************************************/
static int gtp_request_irq(struct goodix_ts_data *ts)
{
	int ret = -1;

	/* use irq */
	if (gpio_is_valid(ts->pdata->irq_gpio) || ts->client->irq > 0) {
		if (gpio_is_valid(ts->pdata->irq_gpio))
			ts->client->irq = gpio_to_irq(ts->pdata->irq_gpio);

		dev_info(&ts->client->dev, "INT num %d, trigger type:%d\n",
			 ts->client->irq, ts->pdata->irq_flags);
		ret = request_threaded_irq(ts->client->irq, NULL,
				gtp_irq_handler,
				ts->pdata->irq_flags | IRQF_ONESHOT,
				ts->client->name,
				ts);
		if (ret < 0) {
			dev_err(&ts->client->dev,
				"Failed to request irq %d\n", ts->client->irq);
			return ret;
		}
	} else { /* use hrtimer */
		dev_info(&ts->client->dev, "No hardware irq, use hrtimer\n");
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = gtp_timer_handler;
		hrtimer_start(&ts->timer,
			      ktime_set(0, (GTP_POLL_TIME + 6) * 1000000),
			      HRTIMER_MODE_REL);
		set_bit(HRTIMER_USED, &ts->flags);
		ret = 0;
	}
	return ret;
}

static s8 gtp_request_input_dev(struct goodix_ts_data *ts)
{
	s8 ret = -1;
	u8 index = 0;

	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		dev_err(&ts->client->dev, "Failed to allocate input device\n");
		return -ENOMEM;
	}

	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY)
		| BIT_MASK(EV_ABS);
	if (!ts->pdata->type_a_report) {
		input_mt_init_slots(ts->input_dev, 16, INPUT_MT_DIRECT);
		dev_info(&ts->client->dev, "Use slot report protocol\n");
	} else {
		__set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
		__set_bit(BTN_TOUCH, ts->input_dev->keybit);
		dev_info(&ts->client->dev, "Use type A report protocol\n");
	}

	input_set_capability(ts->input_dev, EV_KEY, GTP_PEN_BUTTON1);
	input_set_capability(ts->input_dev, EV_KEY, GTP_PEN_BUTTON2);

	/* touch key register */
	for (index = 0; index < ts->pdata->key_nums; index++)
		input_set_capability(ts->input_dev, EV_KEY,
				     ts->pdata->key_map[index]);

	if (ts->pdata->slide_wakeup)
		input_set_capability(ts->input_dev, EV_KEY, KEY_POWER);

	if (ts->pdata->swap_x2y)
		GTP_SWAP(ts->pdata->abs_size_x, ts->pdata->abs_size_y);

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0,
			     ts->pdata->abs_size_x, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0,
			     ts->pdata->abs_size_y, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0,
			     ts->pdata->max_touch_width, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0,
			     ts->pdata->max_touch_pressure, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0,
			     ts->pdata->max_touch_id, 0, 0);
	if (!ts->pdata->type_a_report) {
		input_set_abs_params(ts->input_dev, ABS_MT_TOOL_TYPE,
				     0, MT_TOOL_MAX, 0, 0);
	} else {
		__set_bit(BTN_TOOL_PEN, ts->input_dev->keybit);
		__set_bit(BTN_TOOL_FINGER, ts->input_dev->keybit);
	}

	ts->input_dev->name = goodix_ts_name;
	ts->input_dev->phys = goodix_input_phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0xDEAD;
	ts->input_dev->id.product = 0xBEEF;
	ts->input_dev->id.version = 10427;

	ret = input_register_device(ts->input_dev);
	if (ret) {
		dev_err(&ts->client->dev, "Register %s input device failed\n",
			ts->input_dev->name);
		input_free_device(ts->input_dev);
		return -ENODEV;
	}

	return 0;
}

/*
 * Devices Tree support
 */
#ifdef CONFIG_OF
static void gtp_parse_dt_coords(struct device *dev,
				struct goodix_ts_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret;

	ret = of_property_read_u32(np, "touchscreen-max-id",
				   &pdata->max_touch_id);
	if (ret || pdata->max_touch_id > GTP_MAX_TOUCH_ID) {
		dev_info(dev, "Unset touchscreen-max-id, use default\n");
		pdata->max_touch_id = GTP_MAX_TOUCH_ID;
	}

	ret = of_property_read_u32(np, "touchscreen-size-x",
				   &pdata->abs_size_x);
	if (ret) {
		dev_info(dev, "Unset touchscreen-size-x, use default\n");
		pdata->abs_size_x = GTP_DEFAULT_MAX_X;
	}

	ret = of_property_read_u32(np, "touchscreen-size-y",
				   &pdata->abs_size_y);
	if (ret) {
		dev_info(dev, "Unset touchscreen-size-y, use default\n");
		pdata->abs_size_y = GTP_DEFAULT_MAX_Y;
	}

	ret = of_property_read_u32(np, "touchscreen-max-w",
				   &pdata->max_touch_width);
	if (ret) {
		dev_info(dev, "Unset touchscreen-max-w, use default\n");
		pdata->max_touch_width = GTP_DEFAULT_MAX_WIDTH;
	}

	ret = of_property_read_u32(np, "touchscreen-max-p",
				   &pdata->max_touch_pressure);
	if (ret) {
		dev_info(dev, "Unset touchscreen-max-p, use default\n");
		pdata->max_touch_pressure = GTP_DEFAULT_MAX_PRESSURE;
	}
	dev_info(dev, "touch input parameters is [id x y w p]<%d %d %d %d %d>\n",
		 pdata->max_touch_id, pdata->abs_size_x, pdata->abs_size_y,
		 pdata->max_touch_width, pdata->max_touch_pressure);
}

static int gtp_parse_dt(struct device *dev,
			struct goodix_ts_platform_data *pdata)
{
	int ret;
	u32  key_nums;
	struct property *prop;
	u32 key_map[MAX_KEY_NUMS];
	struct device_node *np = dev->of_node;

	gtp_parse_dt_coords(dev, pdata);

	ret = of_property_read_u32(np, "irq-flags",
				   &pdata->irq_flags);
	if (ret) {
		dev_info(dev,
			 "Failed get int-trigger-type from dts,set default\n");
		pdata->irq_flags = GTP_DEFAULT_INT_TRIGGER;
	}
	of_property_read_u32(np, "goodix,int-sync", &pdata->int_sync);
	if (pdata->int_sync)
		dev_info(dev, "int-sync enabled\n");

	of_property_read_u32(np, "goodix,driver-send-cfg",
			     &pdata->driver_send_cfg);
	if (pdata->driver_send_cfg)
		dev_info(dev, "driver-send-cfg enabled\n");

	of_property_read_u32(np, "goodix,swap-x2y", &pdata->swap_x2y);
	if (pdata->swap_x2y)
		dev_info(dev, "swap-x2y enabled\n");

	of_property_read_u32(np, "goodix,slide-wakeup", &pdata->slide_wakeup);
	if (pdata->slide_wakeup)
		dev_info(dev, "slide-wakeup enabled\n");

	of_property_read_u32(np, "goodix,auto-update", &pdata->auto_update);
	if (pdata->auto_update)
		dev_info(dev, "auto-update enabled\n");

	of_property_read_u32(np, "goodix,auto-update-cfg",
			     &pdata->auto_update_cfg);
	if (pdata->auto_update_cfg)
		dev_info(dev, "auto-update-cfg enabled\n");

	of_property_read_u32(np, "goodix,esd-protect", &pdata->esd_protect);
	if (pdata->esd_protect)
		dev_info(dev, "esd-protect enabled\n");

	of_property_read_u32(np, "goodix,type-a-report",
			     &pdata->type_a_report);
	if (pdata->type_a_report)
		dev_info(dev, "type-a-report enabled\n");

	of_property_read_u32(np, "goodix,resume-in-workqueue",
			     &pdata->resume_in_workqueue);
	if (pdata->resume_in_workqueue)
		dev_info(dev, "resume-in-workqueue enabled\n");

	of_property_read_u32(np, "goodix,power-off-sleep",
			     &pdata->power_off_sleep);
	if (pdata->power_off_sleep)
		dev_info(dev, "power-off-sleep enabled\n");

	of_property_read_u32(np, "goodix,pen-suppress-finger",
			     &pdata->pen_suppress_finger);
	if (pdata->pen_suppress_finger)
		dev_info(dev, "pen-suppress-finger enabled\n");

	prop = of_find_property(np, "touchscreen-key-map", NULL);
	if (prop) {
		key_nums = prop->length / sizeof(key_map[0]);
		key_nums = key_nums > MAX_KEY_NUMS ? MAX_KEY_NUMS : key_nums;

		dev_dbg(dev, "key nums %d\n", key_nums);
		ret = of_property_read_u32_array(np,
				"touchscreen-key-map", key_map,
				key_nums);
		if (ret) {
			dev_err(dev, "Unable to read key codes\n");
			pdata->key_nums = 0;
			memset(pdata->key_map, 0,
			       MAX_KEY_NUMS * sizeof(pdata->key_map[0]));
		}
		pdata->key_nums = key_nums;
		memcpy(pdata->key_map, key_map,
		       key_nums * sizeof(pdata->key_map[0]));
		dev_info(dev, "key-map is [%x %x %x %x]\n",
			 pdata->key_map[0], pdata->key_map[1],
			 pdata->key_map[2], pdata->key_map[3]);
	}

	pdata->irq_gpio = of_get_named_gpio(np, "irq-gpios", 0);
	if (!gpio_is_valid(pdata->irq_gpio))
		dev_err(dev, "No valid irq gpio");

	pdata->rst_gpio = of_get_named_gpio(np, "reset-gpios", 0);
	if (!gpio_is_valid(pdata->rst_gpio))
		dev_err(dev, "No valid rst gpio");

	return 0;
}

/*******************************************************
 * Function:
 *	parse config data from devices tree.
 * Input:
 *	dev: device that this driver attached.
 *	cfg: pointer of the config array.
 *	cfg_len: pointer of the config length.
 *	sid: sensor id.
 * Output:
 *	Executive outcomes.
 *		0-succeed, -1-faileds.
 *******************************************************/
int gtp_parse_dt_cfg(struct device *dev, u8 *cfg, int *cfg_len, u8 sid)
{
	struct device_node *np = dev->of_node;
	struct property *prop;
	char cfg_name[18];
	int ret;

	snprintf(cfg_name, sizeof(cfg_name), "goodix,cfg-group%d", sid);
	prop = of_find_property(np, cfg_name, cfg_len);
	if (!prop || !prop->value || *cfg_len == 0 ||
	    *cfg_len > GTP_CONFIG_MAX_LENGTH) {
		*cfg_len = 0;
		ret = -EPERM;/* failed */
	} else {
		memcpy(cfg, prop->value, *cfg_len);
		ret = 0;
	}

	return ret;
}

#endif

static int gtp_power_on(struct goodix_ts_data *ts)
{
	int ret = 0;

	if (ts->vdd_ana) {
		ret = regulator_set_voltage(ts->vdd_ana, GOODIX_VTG_MIN_UV,
					    GOODIX_VTG_MAX_UV);
		if (ret) {
			dev_err(&ts->client->dev,
				"Regulator set_vtg failed vdd ret=%d\n",
				ret);
			goto err_set_vtg_vdd_ana;
		}
		ret = regulator_enable(ts->vdd_ana);
		if (ret) {
			dev_err(&ts->client->dev,
				"Regulator vdd enable failed ret=%d\n",
				ret);
			goto err_enable_vdd_ana;
		}
	}

	if (ts->vcc_i2c) {
		ret = regulator_set_voltage(ts->vcc_i2c, GOODIX_I2C_VTG_MIN_UV,
					    GOODIX_I2C_VTG_MAX_UV);
		if (ret) {
			dev_err(&ts->client->dev,
				"Regulator set_vtg failed vcc_i2c ret=%d\n",
				ret);
			goto err_set_vtg_vcc_i2c;
		}
		ret = regulator_enable(ts->vcc_i2c);
		if (ret) {
			dev_err(&ts->client->dev,
				"Regulator vcc_i2c enable failed ret=%d\n",
				ret);
			goto err_enable_vcc_i2c;
		}
	}
	clear_bit(POWER_OFF_MODE, &ts->flags);
	return 0;

err_enable_vcc_i2c:
	if (ts->vcc_i2c)
		regulator_set_voltage(ts->vcc_i2c, 0, GOODIX_I2C_VTG_MAX_UV);
err_set_vtg_vcc_i2c:
	if (ts->vdd_ana)
		regulator_disable(ts->vdd_ana);
err_enable_vdd_ana:
	if (ts->vdd_ana)
		regulator_set_voltage(ts->vdd_ana, 0, GOODIX_VTG_MAX_UV);
err_set_vtg_vdd_ana:
	set_bit(POWER_OFF_MODE, &ts->flags);
	return ret;
}

static int gtp_power_off(struct goodix_ts_data *ts)
{
	int ret = 0;

	if (ts->vcc_i2c) {
		set_bit(POWER_OFF_MODE, &ts->flags);
		ret = regulator_set_voltage(ts->vcc_i2c, 0,
					    GOODIX_I2C_VTG_MAX_UV);
		if (ret < 0) {
			dev_err(&ts->client->dev,
				"Regulator vcc_i2c set_vtg failed ret=%d\n",
				ret);
			goto err_set_vtg_vcc_i2c;
		}
		ret = regulator_disable(ts->vcc_i2c);
		if (ret) {
			dev_err(&ts->client->dev,
				"Regulator vcc_i2c disable failed ret=%d\n",
				ret);
			goto err_disable_vcc_i2c;
		}
		dev_info(&ts->client->dev,
			 "Regulator vcc_i2c disabled\n");
	}

	if (ts->vdd_ana) {
		set_bit(POWER_OFF_MODE, &ts->flags);
		ret = regulator_set_voltage(ts->vdd_ana, 0, GOODIX_VTG_MAX_UV);
		if (ret < 0) {
			dev_err(&ts->client->dev,
					"Regulator vdd set_vtg failed ret=%d\n",
					ret);
			goto err_set_vtg_vdd_ana;
		}
		ret = regulator_disable(ts->vdd_ana);
		if (ret) {
			dev_err(&ts->client->dev,
					"Regulator vdd disable failed ret=%d\n",
					ret);
			goto err_disable_vdd_ana;
		}
		dev_info(&ts->client->dev,
			 "Regulator vdd_ana disabled\n");
	}
	return ret;

err_disable_vdd_ana:
	if (ts->vdd_ana)
		regulator_set_voltage(ts->vdd_ana, GOODIX_VTG_MIN_UV,
				      GOODIX_VTG_MAX_UV);
err_set_vtg_vdd_ana:
	if (ts->vcc_i2c)
		ret = regulator_enable(ts->vcc_i2c);
err_disable_vcc_i2c:
	if (ts->vcc_i2c)
		regulator_set_voltage(ts->vcc_i2c, GOODIX_I2C_VTG_MIN_UV,
				      GOODIX_I2C_VTG_MAX_UV);
err_set_vtg_vcc_i2c:
	clear_bit(POWER_OFF_MODE, &ts->flags);
	return ret;
}

static int gtp_power_init(struct goodix_ts_data *ts)
{
	int ret;

	ts->vdd_ana = regulator_get(&ts->client->dev, "vdd_ana");
	if (IS_ERR(ts->vdd_ana)) {
		ts->vdd_ana = NULL;
		ret = PTR_ERR(ts->vdd_ana);
		dev_info(&ts->client->dev,
			 "Regulator get failed vdd ret=%d\n", ret);
	}

	ts->vcc_i2c = regulator_get(&ts->client->dev, "vcc_i2c");
	if (IS_ERR(ts->vcc_i2c)) {
		ts->vcc_i2c = NULL;
		ret = PTR_ERR(ts->vcc_i2c);
		dev_info(&ts->client->dev,
			 "Regulator get failed vcc_i2c ret=%d\n", ret);
	}
	return 0;
}

static int gtp_power_deinit(struct goodix_ts_data *ts)
{
	if (ts->vdd_ana)
		regulator_put(ts->vdd_ana);
	if (ts->vcc_i2c)
		regulator_put(ts->vcc_i2c);

	return 0;
}

static void gtp_shutdown(struct i2c_client *client)
{
	struct goodix_ts_data *data = i2c_get_clientdata(client);

	if (!data->init_done)
		return;

	gtp_work_control_enable(data, false);
	gtp_power_off(data);
}

static int gtp_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = -1;
	struct goodix_ts_data *ts;
	struct goodix_ts_platform_data *pdata;

	/* do NOT remove these logs */
	dev_info(&client->dev, "GTP Driver Version: %s\n", GTP_DRIVER_VERSION);
	dev_info(&client->dev, "GTP I2C Address: 0x%02x\n", client->addr);

	i2c_connect_client = client;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "Failed check I2C functionality");
		return -ENODEV;
	}

	ts = devm_kzalloc(&client->dev, sizeof(*ts), GFP_KERNEL);
	if (!ts)
		return -ENOMEM;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		devm_kfree(&client->dev, ts);
		return -EINVAL;
	}

	ts->init_done = false;

#ifdef CONFIG_OF
	if (client->dev.of_node) {
		ret = gtp_parse_dt(&client->dev, pdata);
		if (ret) {
			dev_err(&client->dev, "Failed parse dts\n");
			goto exit_free_client_data;
		}
	}
#else
	/* set parameters at here if you platform doesn't DTS */
	pdata->rst_gpio = GTP_RST_PORT;
	pdata->irq_gpio = GTP_INT_PORT;
	pdata->slide_wakeup = false;
	pdata->auto_update = true;
	pdata->auto_update_cfg = false;
	pdata->type_a_report = false;
	pdata->esd_protect = false;
	pdata->max_touch_id = GTP_MAX_TOUCH_ID;
	pdata->abs_size_x = GTP_DEFAULT_MAX_X;
	pdata->abs_size_y = GTP_DEFAULT_MAX_Y;
	pdata->max_touch_width = GTP_DEFAULT_MAX_WIDTH;
	pdata->max_touch_pressure = GTP_DEFAULT_MAX_PRESSURE;
#endif

	ts->client = client;
	ts->pdata = pdata;

	i2c_set_clientdata(client, ts);

	ret = gtp_power_init(ts);
	if (ret) {
		dev_err(&client->dev, "Failed get regulator\n");
		ret = -EINVAL;
		goto exit_free_client_data;
	}

	ret = gtp_pinctrl_init(ts);
	if (ret < 0) {
		/* if define pinctrl must define the following state
		 * to let int-pin work normally: default, int_output_high,
		 * int_output_low, int_input
		 */
		dev_err(&client->dev, "Failed get wanted pinctrl state\n");
		goto exit_deinit_power;
	}

	ret = gtp_request_io_port(ts);
	if (ret < 0) {
		dev_err(&client->dev, "Failed request IO port\n");
		goto exit_pinctrl;
	}

	/*wait for discharging power, which from i2c pull-up flow backward*/
	gtp_rst_output(ts, 0);
	msleep(DELAY_FOR_DISCHARGING);

	ret = gtp_power_on(ts);
	if (ret) {
		dev_err(&client->dev, "Failed power on device\n");
		ret = -EINVAL;
		goto exit_free_io_port;
	}

	gtp_reset_guitar(ts->client, 20);

	ret = gtp_i2c_test(client);
	if (ret) {
		dev_err(&client->dev, "Failed communicate with IC use I2C\n");
		goto exit_power_off;
	}

	dev_info(&client->dev, "I2C Addr is %x\n", client->addr);

	ret = gtp_get_fw_info(client, &ts->fw_info);
	if (ret < 0) {
		dev_err(&client->dev, "Failed read FW version\n");
		goto exit_power_off;
	}

	pdata->config.data[0] = GTP_REG_CONFIG_DATA >> 8;
	pdata->config.data[1] = GTP_REG_CONFIG_DATA & 0xff;
	ret = gtp_init_panel(ts);
	if (ret < 0)
		dev_info(&client->dev, "Panel un-initialize\n");

#ifdef CONFIG_TOUCHSCREEN_GT9XX_UPDATE
	if (ts->pdata->auto_update) {
		ret = gup_init_update_proc(ts);
		if (ret < 0)
			dev_err(&client->dev, "Failed create update thread\n");
	}
#endif

	ret = gtp_request_input_dev(ts);
	if (ret < 0) {
		dev_err(&client->dev, "Failed request input device\n");
		goto exit_power_off;
	}

	mutex_init(&ts->lock);

	ret = gtp_request_irq(ts);
	if (ret < 0) {
		dev_err(&client->dev, "Failed create work thread");
		goto exit_unreg_input_dev;
	}
	gtp_work_control_enable(ts, false);
	if (ts->pdata->slide_wakeup) {
		dev_info(&client->dev, "slide wakeup enabled\n");
		ret = enable_irq_wake(client->irq);
		if (ret < 0)
			dev_err(&client->dev, "Failed set irq wake\n");
	}

	gtp_register_powermanager(ts);

	ret = gtp_create_file(ts);
	if (ret) {
		dev_info(&client->dev, "Failed create attributes file");
		goto exit_powermanager;
	}

#ifdef CONFIG_TOUCHSCREEN_GT9XX_TOOL
	init_wr_node(client);/*TODO judge return value */
#endif

	gtp_esd_init(ts);
	gtp_esd_on(ts);
	/* probe init finished */
	ts->init_done = true;
	gtp_work_control_enable(ts, true);

	return 0;

exit_powermanager:
	gtp_unregister_powermanager(ts);
exit_unreg_input_dev:
	input_unregister_device(ts->input_dev);
exit_power_off:
	gtp_power_off(ts);
exit_free_io_port:
	if (gpio_is_valid(ts->pdata->rst_gpio))
		gpio_free(ts->pdata->rst_gpio);
	if (gpio_is_valid(ts->pdata->irq_gpio))
		gpio_free(ts->pdata->irq_gpio);
exit_pinctrl:
	gtp_pinctrl_deinit(ts);
exit_deinit_power:
	gtp_power_deinit(ts);
exit_free_client_data:
	devm_kfree(&client->dev, pdata);
	devm_kfree(&client->dev, ts);
	i2c_set_clientdata(client, NULL);

	return ret;
}

static int gtp_drv_remove(struct i2c_client *client)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	gtp_work_control_enable(ts, false);
	gtp_unregister_powermanager(ts);

	remove_proc_entry(GT91XX_CONFIG_PROC_FILE, gtp_config_proc);

	sysfs_remove_group(&client->dev.kobj, &gtp_attr_group);

#ifdef CONFIG_TOUCHSCREEN_GT9XX_TOOL
	uninit_wr_node();
#endif

	if (ts->pdata->esd_protect)
		gtp_esd_off(ts);

	/* TODO: how to judge a irq numbers validity */
	if (ts->client->irq)
		free_irq(client->irq, ts);
	else
		hrtimer_cancel(&ts->timer);

	if (gpio_is_valid(ts->pdata->rst_gpio))
		gpio_free(ts->pdata->rst_gpio);

	if (gpio_is_valid(ts->pdata->irq_gpio))
		gpio_free(ts->pdata->irq_gpio);

	gtp_power_off(ts);
	gtp_power_deinit(ts);
	gtp_pinctrl_deinit(ts);
	dev_info(&client->dev, "goodix ts driver removed");
	i2c_set_clientdata(client, NULL);
	input_unregister_device(ts->input_dev);
	mutex_destroy(&ts->lock);

	devm_kfree(&client->dev, ts->pdata);
	devm_kfree(&client->dev, ts);

	return 0;
}

static void gtp_suspend(struct goodix_ts_data *ts)
{
	int ret = -1;

	if (test_bit(FW_UPDATE_RUNNING, &ts->flags)) {
		dev_warn(&ts->client->dev,
			 "Fw upgrade in progress, can't go to suspend\n");
		return;
	}

	if (test_and_set_bit(SLEEP_MODE, &ts->flags)) {
		dev_info(&ts->client->dev, "Already in suspend state\n");
		return;
	}

	dev_dbg(&ts->client->dev, "Try enter suspend mode\n");

	gtp_esd_off(ts);
	gtp_work_control_enable(ts, false);
	if (ts->pdata->slide_wakeup) {
		ret = gtp_enter_doze(ts);
		gtp_work_control_enable(ts, true);
	} else if (ts->pdata->power_off_sleep) {
		/*TODO: power off routine */
		gtp_power_off(ts);
		ret = SUCCESS;
	} else {
		ret = gtp_enter_sleep(ts);
	}

	if (ret < 0)
		dev_err(&ts->client->dev, "Failed enter suspend\n");

	/*  to avoid waking up while not sleeping */
	/*	delay 48 + 10ms to ensure reliability */
	msleep(GTP_58_DLY_MS);
}

static int gtp_gesture_wakeup(struct goodix_ts_data *ts)
{
	int ret;
	int retry = 10;

	do {
		gtp_reset_guitar(ts->client, 10);
		ret = gtp_i2c_test(ts->client);
		if (!ret)
			break;
	} while (--retry);

	if (!retry)
		ret = -EIO;

	clear_bit(DOZE_MODE, &ts->flags);
	return ret;
}

static void gtp_resume(struct goodix_ts_data *ts)
{
	int ret = 0;

	if (test_bit(FW_UPDATE_RUNNING, &ts->flags)) {
		dev_info(&ts->client->dev,
			 "Fw upgrade in progress, can't do resume\n");
		return;
	}

	if (!test_bit(SLEEP_MODE, &ts->flags)) {
		dev_dbg(&ts->client->dev, "Already in awake state\n");
		return;
	}

	dev_info(&ts->client->dev, "Try resume from sleep mode\n");

	gtp_work_control_enable(ts, false);

	if (ts->pdata->slide_wakeup && test_bit(DOZE_MODE, &ts->flags)) {
		ret = gtp_gesture_wakeup(ts);
		if (ret)
			dev_warn(&ts->client->dev, "Failed wake up from gesture mode\n");
	} else if (ts->pdata->power_off_sleep) {
		ret = gtp_power_on(ts);
		if (ret) {
			dev_warn(&ts->client->dev, "Failed wake up from gesture mode\n");
		} else {
			gtp_reset_guitar(ts->client, 20);
			ret = gtp_i2c_test(ts->client);
			if (ret)
				dev_warn(&ts->client->dev,
					 "I2C communicate failed after power on\n");
		}
	} else {
		ret = gtp_wakeup_sleep(ts);
		if (ret)
			dev_warn(&ts->client->dev,
				 "Failed wakeup from sleep mode\n");
	}

	if (ret)
		dev_warn(&ts->client->dev, "Later resume failed\n");
	else
		gtp_esd_on(ts);

	clear_bit(SLEEP_MODE, &ts->flags);
	gtp_work_control_enable(ts, true);
}

#if defined(CONFIG_FB)
static void fb_notify_resume_work(struct work_struct *work)
{
	struct goodix_ts_data *ts =
		container_of(work, struct goodix_ts_data, fb_notify_work);
	dev_info(&ts->client->dev, "try resume in workqueue\n");
	gtp_resume(ts);
}

/* frame buffer notifier block control the suspend/resume procedure */
static int gtp_fb_notifier_callback(struct notifier_block *noti,
				    unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	struct goodix_ts_data *ts = container_of(noti,
			struct goodix_ts_data, notifier);
	int *blank;

	if (ev_data && ev_data->data && event == FB_EVENT_BLANK && ts) {
		blank = ev_data->data;
		if (*blank == FB_BLANK_UNBLANK ||
		    *blank == FB_BLANK_NORMAL) {
			dev_dbg(&ts->client->dev, "ts_resume");
			if (ts->pdata->resume_in_workqueue)
				schedule_work(&ts->fb_notify_work);
			else
				gtp_resume(ts);
		} else if (*blank == FB_BLANK_POWERDOWN) {
			dev_dbg(&ts->client->dev, "ts_suspend");
			if (ts->pdata->resume_in_workqueue)
				flush_work(&ts->fb_notify_work);
			gtp_suspend(ts);
		}
	}

	return 0;
}

#elif defined(CONFIG_PM)
static int gtp_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts) {
		dev_dbg(&ts->client->dev, "Suspend by i2c pm.");
		gtp_suspend(ts);
	}

	return 0;
}

static int gtp_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts) {
		dev_dbg(&ts->client->dev, "Resume by i2c pm.");
		gtp_resume(ts);
	}

	return 0;
}

static const struct dev_pm_ops gtp_pm_ops = {
	.suspend = gtp_pm_suspend,
	.resume  = gtp_pm_resume,
};

#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void gtp_early_suspend(struct early_suspend *h)
{
	struct goodix_ts_data *ts = container_of(h,
			struct goodix_ts_data, early_suspend);

	if (ts) {
		dev_dbg(&ts->client->dev, "Suspend by earlysuspend module.");
		gtp_suspend(ts);
	}
}

static void gtp_late_resume(struct early_suspend *h)
{
	struct goodix_ts_data *ts = container_of(h,
			struct goodix_ts_data, early_suspend);

	if (ts) {
		dev_dbg(&ts->client->dev, "Resume by earlysuspend module.");
		gtp_resume(ts);
	}
}
#endif

static int gtp_register_powermanager(struct goodix_ts_data *ts)
{
	int ret;
#if defined(CONFIG_FB)
	INIT_WORK(&ts->fb_notify_work, fb_notify_resume_work);
	ts->notifier.notifier_call = gtp_fb_notifier_callback;
	ret = fb_register_client(&ts->notifier);
	if (ret)
		dev_err(&ts->client->dev,
			"Unable to register fb_notifier: %d\n", ret);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = goodix_ts_early_suspend;
	ts->early_suspend.resume = goodix_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	return ret;
}

static int gtp_unregister_powermanager(struct goodix_ts_data *ts)
{
#if defined(CONFIG_FB)
		fb_unregister_client(&ts->notifier);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
		unregister_early_suspend(&ts->early_suspend);
#endif

	return 0;
}

/*******************************************************
 * Function:
 *	Initialize external watchdog for esd protect
 * Input:
 *	client: i2c device.
 * Output:
 *	result of i2c write operation.
 *		0: succeed, otherwise: failed
 ********************************************************/
static int gtp_init_ext_watchdog(struct i2c_client *client)
{
	int ret;
	u8 opr_buffer[3] = { (u8)(GTP_REG_ESD_CHECK >> 8),
			     (u8)GTP_REG_ESD_CHECK,
			     (u8)GTP_ESD_CHECK_VALUE };

	dev_dbg(&client->dev, "[Esd]Init external watchdog\n");
	ret = gtp_i2c_write(client, opr_buffer, 3);
	if (ret == 1)
		return 0;

	dev_err(&client->dev, "Failed init ext watchdog\n");
	return -EINVAL;
}

static void gtp_esd_check_func(struct work_struct *work)
{
	s32 i;
	s32 ret = -1;
	u8 esd_buf[5] = { (u8)(GTP_REG_COMMAND >> 8), (u8)GTP_REG_COMMAND };
	struct delayed_work *dwork = to_delayed_work(work);
	struct goodix_ts_esd *ts_esd = container_of(dwork, struct goodix_ts_esd,
						    delayed_work);
	struct goodix_ts_data *ts = container_of(ts_esd, struct goodix_ts_data,
						 ts_esd);

	if (test_bit(SLEEP_MODE, &ts->flags) ||
	    test_bit(FW_UPDATE_RUNNING, &ts->flags)) {
		dev_dbg(&ts->client->dev,
			"Esd cancled by power_suspend or fw_update!");
		return;
	}

	if (ts_esd->esd_on == false)
		return;

	for (i = 0; i < 3; i++) {
		ret = gtp_i2c_read(ts->client, esd_buf, 4);
		if (ret < 0)
			continue;

		dev_dbg(&ts->client->dev,
			"[Esd]0x8040 = 0x%02X, 0x8041 = 0x%02X",
			esd_buf[2], esd_buf[3]);
		if (esd_buf[2] == (u8)GTP_ESD_CHECK_VALUE ||
		    esd_buf[3] != (u8)GTP_ESD_CHECK_VALUE) {
			gtp_i2c_read(ts->client, esd_buf, 4);
			if (ret < 0)
				continue;

			if (esd_buf[2] == (u8)GTP_ESD_CHECK_VALUE ||
			    esd_buf[3] != (u8)GTP_ESD_CHECK_VALUE) {
				i = 3;
				break;
			}
		} else {
			/* IC works normally, Write 0x8040 0xAA, feed the dog */
			esd_buf[2] = (u8)GTP_ESD_CHECK_VALUE;
			gtp_i2c_write(ts->client, esd_buf, 3);
			break;
		}
	}
	if (i >= 3) {
		dev_err(&ts->client->dev, "IC working abnormally! Reset IC\n");
		esd_buf[0] = 0x42;
		esd_buf[1] = 0x26;
		esd_buf[2] = 0x01;
		esd_buf[3] = 0x01;
		esd_buf[4] = 0x01;
		gtp_i2c_write(ts->client, esd_buf, 5);
		/* TODO: Is power off really need? */
		msleep(GTP_50_DLY_MS);
		gtp_power_off(ts);
		msleep(GTP_20_DLY_MS);
		gtp_power_on(ts);
		msleep(GTP_20_DLY_MS);

		gtp_reset_guitar(ts->client, 50);
		msleep(GTP_50_DLY_MS);
		gtp_send_cfg(ts->client);
	}

	if (ts_esd->esd_on == true && !test_bit(SLEEP_MODE, &ts->flags)) {
		schedule_delayed_work(&ts_esd->delayed_work, 2 * HZ);
		dev_dbg(&ts->client->dev, "ESD work rescheduled\n");
	}
}

static int gtp_esd_init(struct goodix_ts_data *ts)
{
	struct goodix_ts_esd *ts_esd = &ts->ts_esd;

	INIT_DELAYED_WORK(&ts_esd->delayed_work, gtp_esd_check_func);
	mutex_init(&ts_esd->mutex);
	ts_esd->esd_on = false;

	return 0;
}

void gtp_esd_on(struct goodix_ts_data *ts)
{
	struct goodix_ts_esd *ts_esd = &ts->ts_esd;

	if (!ts->pdata->esd_protect)
		return;
	mutex_lock(&ts_esd->mutex);
	if (ts_esd->esd_on == false) {
		ts_esd->esd_on = true;
		schedule_delayed_work(&ts_esd->delayed_work, 2 * HZ);
		dev_info(&ts->client->dev, "ESD on");
	}
	mutex_unlock(&ts_esd->mutex);
}

void gtp_esd_off(struct goodix_ts_data *ts)
{
	struct goodix_ts_esd *ts_esd = &ts->ts_esd;

	if (!ts->pdata->esd_protect)
		return;
	mutex_lock(&ts_esd->mutex);
	if (ts_esd->esd_on == true) {
		ts_esd->esd_on = false;
		cancel_delayed_work_sync(&ts_esd->delayed_work);
		dev_info(&ts->client->dev, "ESD off");
	}
	mutex_unlock(&ts_esd->mutex);
}

#ifdef CONFIG_OF
static const struct of_device_id gtp_match_table[] = {
	{.compatible = "goodix,gt9xx",},
	{ },
};
#endif

static const struct i2c_device_id gtp_device_id[] = {
	{ GTP_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver goodix_ts_driver = {
	.probe		= gtp_probe,
	.remove		= gtp_drv_remove,
	.id_table	= gtp_device_id,
	.shutdown	= gtp_shutdown,
	.driver = {
		.name	  = GTP_I2C_NAME,
		.owner	  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = gtp_match_table,
#endif
#if !defined(CONFIG_FB) && defined(CONFIG_PM)
		.pm		  = &gtp_pm_ops,
#endif
	},
};

static int __init gtp_init(void)
{
	s32 ret;

	pr_info("Gt9xx driver installing..\n");
	ret = i2c_add_driver(&goodix_ts_driver);

	return ret;
}

static void __exit gtp_exit(void)
{
	pr_info("Gt9xx driver exited\n");
	i2c_del_driver(&goodix_ts_driver);
}

module_init(gtp_init);
module_exit(gtp_exit);

MODULE_DESCRIPTION("GT9 serials touch controller Driver");
MODULE_LICENSE("GPL v2");

