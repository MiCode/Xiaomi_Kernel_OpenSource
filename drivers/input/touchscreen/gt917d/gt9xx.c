/*
 * Goodix GT9xx touchscreen driver
 *
 * Copyright  (C)  2010 - 2016 Goodix. Ltd.
 * Copyright (C) 2018 XiaoMi, Inc.
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
 * Version: 2.4.0.1
 * Release Date: 2016/10/26
 */

#include <linux/irq.h>
#include "gt9xx.h"

#if GTP_ICS_SLOT_REPORT
	#include <linux/input/mt.h>
#endif


#include "../lct_tp_fm_info.h"
#ifdef SUPPORT_READ_TP_VERSION
	char product_id[GTP_PRODUCT_ID_MAXSIZE];
	char fw_version[64];
#endif



char gt9xx_tp_lockdown_info[128];
u16 version_info;
unsigned char get_lockdown_info(struct i2c_client *client, char *pProjectCode);


static const char *goodix_ts_name = "goodix-ts";
static const char *goodix_input_phys = "input/ts";
static struct workqueue_struct *goodix_wq;
struct i2c_client *i2c_connect_client = NULL;
int gtp_rst_gpio;
int gtp_int_gpio;
u8 config[GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH]
				= {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};

#if GTP_HAVE_TOUCH_KEY
	static const u16 touch_key_array[] = GTP_KEY_TAB;
	#define GTP_MAX_KEY_NUM  (sizeof(touch_key_array)/sizeof(touch_key_array[0]))

#if GTP_DEBUG_ON
	static const int  key_codes[] = {KEY_HOME, KEY_BACK, KEY_MENU, KEY_HOMEPAGE, KEY_F1, KEY_F2, KEY_F3};
	static const char *key_names[] = {"Key_Home", "Key_Back", "Key_Menu", "Key_Homepage", "KEY_F1", "KEY_F2", "KEY_F3"};
#endif

#endif

static s8 gtp_i2c_test(struct i2c_client *client);
void gtp_reset_guitar(struct i2c_client *client, s32 ms);
s32 gtp_send_cfg(struct i2c_client *client);

void gtp_int_sync(struct i2c_client *client, s32 ms);

static ssize_t gt91xx_config_read_proc(struct file *, char __user *, size_t, loff_t *);
static ssize_t gt91xx_config_write_proc(struct file *, const char __user *, size_t, loff_t *);

static struct proc_dir_entry *gt91xx_config_proc;
static const struct file_operations config_proc_ops = {
	.owner = THIS_MODULE,
	.read = gt91xx_config_read_proc,
	.write = gt91xx_config_write_proc,
};
static int gtp_register_powermanger(struct goodix_ts_data *ts);
static int gtp_unregister_powermanger(struct goodix_ts_data *ts);
#if RESUME_IN_WORKQUEUE
static void gtp_fb_notifier_resume_work(struct work_struct *work);
#endif

#if GTP_CREATE_WR_NODE
extern s32 init_wr_node(struct i2c_client*);
extern void uninit_wr_node(void);
#endif

#if GTP_AUTO_UPDATE
extern u8 gup_init_update_proc(struct goodix_ts_data *);
#endif

#if GTP_ESD_PROTECT
static struct delayed_work gtp_esd_check_work;
static struct workqueue_struct *gtp_esd_check_workqueue;
static void gtp_esd_check_func(struct work_struct *);
static s32 gtp_init_ext_watchdog(struct i2c_client *client);
void gtp_esd_switch(struct i2c_client *, s32);
#endif


#if GTP_COMPATIBLE_MODE
extern s32 i2c_read_bytes(struct i2c_client *client, u16 addr, u8 *buf, s32 len);
extern s32 i2c_write_bytes(struct i2c_client *client, u16 addr, u8 *buf, s32 len);
extern s32 gup_clk_calibration(void);
extern s32 gup_fw_download_proc(void *dir, u8 dwn_mode);
extern u8 gup_check_fs_mounted(char *path_name);

void gtp_recovery_reset(struct i2c_client *client);
static s32 gtp_esd_recovery(struct i2c_client *client);
s32 gtp_fw_startup(struct i2c_client *client);
static s32 gtp_main_clk_proc(struct goodix_ts_data *ts);
static s32 gtp_bak_ref_proc(struct goodix_ts_data *ts, u8 mode);

#endif

#define PINCTRL_STATE_ACTIVE "pmx_ts_irqrst_active"
#define PINCTRL_STATE_SUSPEND "pmx_ts_irqrst_suspend"
struct pinctrl_data *pindata = NULL;


#if GTP_GESTURE_WAKEUP
typedef enum
{
	DOZE_DISABLED = 0,
	DOZE_ENABLED = 1,
	DOZE_WAKEUP = 2,
}DOZE_T;
static DOZE_T doze_status = DOZE_DISABLED;

static int goodix_select_gesture_mode(struct input_dev *dev, unsigned int type, unsigned int code, int value);

static s8 gtp_enter_doze(struct goodix_ts_data *ts);
#endif

#ifdef GTP_CONFIG_OF
int gtp_parse_dt_cfg(struct device *dev, u8 *cfg, int *cfg_len, u8 sid);
#endif


/*******************************************************
Function:
	Read data from the i2c slave device.
Input:
	client:     i2c device.
	buf[0~1]:   read start address.
	buf[2~len-1]:   read data buffer.
	len:    GTP_ADDR_LENGTH + read bytes count
Output:
	numbers of i2c_msgs to transfer:
	  2: succeed, otherwise: failed
*********************************************************/
s32 gtp_i2c_read(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msgs[2];
	s32 ret = -1;
	s32 retries = 0;
	GTP_DEBUG_FUNC();

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = client->addr;
	msgs[0].len   = GTP_ADDR_LENGTH;
	msgs[0].buf   = &buf[0];


	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = client->addr;
	msgs[1].len   = len - GTP_ADDR_LENGTH;
	msgs[1].buf   = &buf[GTP_ADDR_LENGTH];


	while (retries < 5) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
	}
	if ((retries >= 5)) {
	#if GTP_COMPATIBLE_MODE
		struct goodix_ts_data *ts = i2c_get_clientdata(client);
	#endif

	#if GTP_GESTURE_WAKEUP

		if (DOZE_ENABLED == doze_status) {
			return ret;
		}
	#endif
		GTP_ERROR("I2C Read: 0x%04X, %d bytes failed, errcode: %d! Process reset.", (((u16)(buf[0] << 8)) | buf[1]), len-2, ret);
	#if GTP_COMPATIBLE_MODE
		if (CHIP_TYPE_GT9F == ts->chip_type) {
			gtp_recovery_reset(client);
		} else
	#endif
		{
			gtp_reset_guitar(client, 20);
		}
	}
	return ret;
}



/*******************************************************
Function:
	Write data to the i2c slave device.
Input:
	client:     i2c device.
	buf[0~1]:   write start address.
	buf[2~len-1]:   data buffer
	len:    GTP_ADDR_LENGTH + write bytes count
Output:
	numbers of i2c_msgs to transfer:
		1: succeed, otherwise: failed
*********************************************************/
s32 gtp_i2c_write(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msg;
	s32 ret = -1;
	s32 retries = 0;

	GTP_DEBUG_FUNC();

	msg.flags = !I2C_M_RD;
	msg.addr  = client->addr;
	msg.len   = len;
	msg.buf   = buf;


	while (retries < 5) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)
			break;
		retries++;
	}
	if ((retries >= 5)) {
	#if GTP_COMPATIBLE_MODE
		struct goodix_ts_data *ts = i2c_get_clientdata(client);
	#endif

	#if GTP_GESTURE_WAKEUP
		if (DOZE_ENABLED == doze_status) {
			return ret;
		}
	#endif
		GTP_ERROR("I2C Write: 0x%04X, %d bytes failed, errcode: %d! Process reset.", (((u16)(buf[0] << 8)) | buf[1]), len-2, ret);
	#if GTP_COMPATIBLE_MODE
		if (CHIP_TYPE_GT9F == ts->chip_type) {
			gtp_recovery_reset(client);
		} else
	#endif
		{
			gtp_reset_guitar(client, 20);
		}
	}
	return ret;
}


/*******************************************************
Function:
	i2c read twice, compare the results
Input:
	client:  i2c device
	addr:    operate address
	rxbuf:   read data to store, if compare successful
	len:     bytes to read
Output:
	FAIL:    read failed
	SUCCESS: read successful
*********************************************************/
s32 gtp_i2c_read_dbl_check(struct i2c_client *client, u16 addr, u8 *rxbuf, int len)
{
	u8 buf[16] = {0};
	u8 confirm_buf[16] = {0};
	u8 retry = 0;

	while (retry++ < 3) {
		memset(buf, 0xAA, 16);
		buf[0] = (u8)(addr >> 8);
		buf[1] = (u8)(addr & 0xFF);
		gtp_i2c_read(client, buf, len + 2);

		memset(confirm_buf, 0xAB, 16);
		confirm_buf[0] = (u8)(addr >> 8);
		confirm_buf[1] = (u8)(addr & 0xFF);
		gtp_i2c_read(client, confirm_buf, len + 2);

		if (!memcmp(buf, confirm_buf, len+2)) {
			memcpy(rxbuf, confirm_buf+2, len);
			return SUCCESS;
		}
	}
	GTP_ERROR("I2C read 0x%04X, %d bytes, double check failed!", addr, len);
	return FAIL;
}

/*******************************************************
Function:
	Send config.
Input:
	client: i2c device.
Output:
	result of i2c write operation.
		1: succeed, otherwise: failed
*********************************************************/

s32 gtp_send_cfg(struct i2c_client *client)
{
	s32 ret = 2;

#if GTP_DRIVER_SEND_CFG
	s32 retry = 0;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts->pnl_init_error) {
		GTP_INFO("Error occured in init_panel, no config sent");
		return 0;
	}

	GTP_INFO("Driver send config.");
	for (retry = 0; retry < 5; retry++) {
		ret = gtp_i2c_write(client, config , GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH);
		if (ret > 0) {
			break;
		}
	}
#endif
	return ret;
}
/*******************************************************
Function:
	Disable irq function
Input:
	ts: goodix i2c_client private data
Output:
	None.
*********************************************************/
void gtp_irq_disable(struct goodix_ts_data *ts)
{
	unsigned long irqflags;

	GTP_DEBUG_FUNC();

	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (!ts->irq_is_disable) {
		ts->irq_is_disable = 1;
		disable_irq_nosync(ts->client->irq);
	}
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}

/*******************************************************
Function:
	Enable irq function
Input:
	ts: goodix i2c_client private data
Output:
	None.
*********************************************************/
void gtp_irq_enable(struct goodix_ts_data *ts)
{
	unsigned long irqflags = 0;

	GTP_DEBUG_FUNC();

	spin_lock_irqsave(&ts->irq_lock, irqflags);
	if (ts->irq_is_disable) {
		enable_irq(ts->client->irq);
		ts->irq_is_disable = 0;
	}
	spin_unlock_irqrestore(&ts->irq_lock, irqflags);
}


/*******************************************************
Function:
	Report touch point event
Input:
	ts: goodix i2c_client private data
	id: trackId
	x:  input x coordinate
	y:  input y coordinate
	w:  input pressure
Output:
	None.
*********************************************************/
static void gtp_touch_down(struct goodix_ts_data *ts, s32 id, s32 x, s32 y, s32 w)
{
#if GTP_CHANGE_X2Y
	GTP_SWAP(x, y);
#endif

#if GTP_ICS_SLOT_REPORT


	input_mt_slot(ts->input_dev, id);
	input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
	if (id == 9) {
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_PEN, true);
	} else {
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);
	}
	input_report_abs(ts->input_dev, ABS_MT_PRESSURE, w);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
#else

	if ((id & 0x80)) {
		id = 9;
		input_report_abs(ts->input_dev, ABS_MT_TOOL_TYPE, 1);
	} else {
		id = id & 0x0F;
		input_report_abs(ts->input_dev, ABS_MT_TOOL_TYPE, 0);
	}
	input_report_key(ts->input_dev, BTN_TOUCH, 1);
	input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, id);
	input_report_abs(ts->input_dev, ABS_MT_PRESSURE, w);
	input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, w);
	input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, w);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_X, x);
	input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, y);

	input_mt_sync(ts->input_dev);
#endif

	GTP_DEBUG("ID:%d, X:%d, Y:%d, W:%d", id, x, y, w);
}

/*******************************************************
Function:
	Report touch release event
Input:
	ts: goodix i2c_client private data
Output:
	None.
*********************************************************/
static void gtp_touch_up(struct goodix_ts_data *ts, s32 id)
{
#if GTP_ICS_SLOT_REPORT
	input_mt_slot(ts->input_dev, id);
	input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, -1);
	GTP_DEBUG("Touch id[%2d] release!", id);
#else
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
input_mt_sync(ts->input_dev);

#endif
}

#if GTP_WITH_HOVER

static void gtp_pen_init(struct goodix_ts_data *ts)
{
	s32 ret = 0;

	GTP_INFO("Request input device for pen/stylus.");

	ts->pen_dev = input_allocate_device();
	if (ts->pen_dev == NULL) {
		GTP_ERROR("Failed to allocate input device for pen/stylus.");
		return;
	}

	ts->pen_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
	ts->pen_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
	set_bit(BTN_TOOL_PEN, ts->pen_dev->keybit);

	input_set_abs_params(ts->pen_dev, ABS_MT_POSITION_X, 0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->pen_dev, ABS_MT_POSITION_Y, 0, ts->abs_y_max, 0, 0);
	input_set_abs_params(ts->pen_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(ts->pen_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->pen_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

	ts->pen_dev->name = "goodix-pen";
	ts->pen_dev->id.bustype = BUS_I2C;

	ret = input_register_device(ts->pen_dev);
	if (ret) {
		GTP_ERROR("Register %s input device failed", ts->pen_dev->name);
		return;
	}
}

static void gtp_pen_down(s32 x, s32 y, s32 w, s32 id)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(i2c_connect_client);

#if GTP_CHANGE_X2Y
	GTP_SWAP(x, y);
#endif

	input_report_key(ts->pen_dev, BTN_TOOL_PEN, 1);
#if GTP_ICS_SLOT_REPORT
	input_mt_slot(ts->pen_dev, id);
	input_report_abs(ts->pen_dev, ABS_MT_POSITION_X, x);
	input_report_abs(ts->pen_dev, ABS_MT_POSITION_Y, y);
	input_report_key(ts->pen_dev, BTN_TOUCH, 0);

#else

	input_report_abs(ts->pen_dev, ABS_MT_POSITION_X, x);
	input_report_abs(ts->pen_dev, ABS_MT_POSITION_Y, y);
	input_report_key(ts->pen_dev, BTN_TOUCH, 0);
	input_mt_sync(ts->pen_dev);
#endif
	GTP_DEBUG("(%d)(%d, %d)[%d]", id, x, y, w);
}

static void gtp_pen_up(s32 id)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(i2c_connect_client);

	input_report_key(ts->pen_dev, BTN_TOOL_PEN, 0);

#if GTP_ICS_SLOT_REPORT
	input_mt_slot(ts->pen_dev, id);

	input_report_key(ts->pen_dev, BTN_TOUCH, 0);
#else

	input_report_key(ts->pen_dev, BTN_TOUCH, 0);
#endif

}
#endif

/*******************************************************
Function:
	Goodix touchscreen work function
Input:
	work: work struct of goodix_workqueue
Output:
	None.
*********************************************************/
static void goodix_ts_work_func(struct work_struct *work)
{
	u8  end_cmd[3] = {GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF, 0};
	u8  point_data[2 + 1 + 8 * GTP_MAX_TOUCH + 1] = {GTP_READ_COOR_ADDR >> 8, GTP_READ_COOR_ADDR & 0xFF};
	u8  touch_num = 0;
	u8  finger = 0;
	static u16 pre_touch;
	static u8 pre_key;
#if GTP_WITH_HOVER
	u8 pen_active = 0;
	static u8 pre_pen;
#endif
	static u8 pre_finger;
	u8 dev_active = 0;
	u8  key_value = 0;
	u8 *coor_data = NULL;
	s32 input_x = 0;
	s32 input_y = 0;
	s32 input_w = 0;
	s32 id = 0;
	s32 i  = 0;
	s32 ret = -1;
	struct goodix_ts_data *ts = NULL;

#if GTP_COMPATIBLE_MODE
	u8 rqst_buf[3] = {0x80, 0x43};
#endif

#if GTP_GESTURE_WAKEUP
	u8 doze_buf[3] = {0x81, 0x4B};
#endif
	GTP_DEBUG_FUNC();
	ts = container_of(work, struct goodix_ts_data, work);
	if (ts->enter_update) {
		return;
	}
#if GTP_GESTURE_WAKEUP
	if (DOZE_ENABLED == doze_status) {
		ret = gtp_i2c_read(i2c_connect_client, doze_buf, 3);
		GTP_DEBUG("0x814B = 0x%02X", doze_buf[2]);
		if (ret > 0) {
			if ((doze_buf[2] == 'a') || (doze_buf[2] == 'b') || (doze_buf[2] == 'c') ||
				(doze_buf[2] == 'd') || (doze_buf[2] == 'e') || (doze_buf[2] == 'g') ||
				(doze_buf[2] == 'h') || (doze_buf[2] == 'm') || (doze_buf[2] == 'o') ||
				(doze_buf[2] == 'q') || (doze_buf[2] == 's') || (doze_buf[2] == 'v') ||
				(doze_buf[2] == 'w') || (doze_buf[2] == 'y') || (doze_buf[2] == 'z') ||
				(doze_buf[2] == 0x5E) || (doze_buf[2] == 0x3E)/* ^ */
				) {
				if (doze_buf[2] != 0x5E) {
					GTP_INFO("Wakeup by gesture(%c), light up the screen!", doze_buf[2]);
				} else {
					GTP_INFO("Wakeup by gesture(^), light up the screen!");
				}
				doze_status = DOZE_WAKEUP;
				input_report_key(ts->input_dev, KEY_POWER, 1);
				input_sync(ts->input_dev);
				input_report_key(ts->input_dev, KEY_POWER, 0);
				input_sync(ts->input_dev);

				doze_buf[2] = 0x00;
				gtp_i2c_write(i2c_connect_client, doze_buf, 3);
			} else if ((doze_buf[2] == 0xAA) || (doze_buf[2] == 0xBB) ||
				(doze_buf[2] == 0xAB) || (doze_buf[2] == 0xBA)) {
				char *direction[4] = {"Right", "Down", "Up", "Left"};
				u8 type = ((doze_buf[2] & 0x0F) - 0x0A) + (((doze_buf[2] >> 4) & 0x0F) - 0x0A) * 2;

				GTP_INFO("%s slide to light up the screen!", direction[type]);
				doze_status = DOZE_WAKEUP;
				input_report_key(ts->input_dev, KEY_POWER, 1);
				input_sync(ts->input_dev);
				input_report_key(ts->input_dev, KEY_POWER, 0);
				input_sync(ts->input_dev);

				doze_buf[2] = 0x00;
				gtp_i2c_write(i2c_connect_client, doze_buf, 3);
			} else if (0xCC == doze_buf[2]) {
				GTP_INFO("Double click to light up the screen!");
				doze_status = DOZE_WAKEUP;

				input_report_key(ts->input_dev, KEY_WAKEUP, 1);
				input_sync(ts->input_dev);
				input_report_key(ts->input_dev, KEY_WAKEUP, 0);

				input_sync(ts->input_dev);

				doze_buf[2] = 0x00;
				gtp_i2c_write(i2c_connect_client, doze_buf, 3);
			} else {

				doze_buf[2] = 0x00;
				gtp_i2c_write(i2c_connect_client, doze_buf, 3);
				gtp_enter_doze(ts);
			}
		}
		if (ts->use_irq) {
			gtp_irq_enable(ts);
		}
		return;
	}
#endif

	ret = gtp_i2c_read(ts->client, point_data, 12);
	if (ret < 0) {
		GTP_ERROR("I2C transfer error. errno:%d\n ", ret);
		if (ts->use_irq) {
			gtp_irq_enable(ts);
		}
		return;
	}

	finger = point_data[GTP_ADDR_LENGTH];

#if GTP_COMPATIBLE_MODE

	if ((finger == 0x00) && (CHIP_TYPE_GT9F == ts->chip_type)) {
		ret = gtp_i2c_read(ts->client, rqst_buf, 3);
		if (ret < 0) {
			GTP_ERROR("Read request status error!");
			goto exit_work_func;
		}

		switch (rqst_buf[2]) {
		case GTP_RQST_CONFIG:
			GTP_INFO("Request for config.");
			ret = gtp_send_cfg(ts->client);
			if (ret < 0) {
				GTP_ERROR("Request for config unresponded!");
			} else {
				rqst_buf[2] = GTP_RQST_RESPONDED;
				gtp_i2c_write(ts->client, rqst_buf, 3);
				GTP_INFO("Request for config responded!");
			}
			break;

		case GTP_RQST_BAK_REF:
			GTP_INFO("Request for backup reference.");
			ts->rqst_processing = 1;
			ret = gtp_bak_ref_proc(ts, GTP_BAK_REF_SEND);
			if (SUCCESS == ret) {
				rqst_buf[2] = GTP_RQST_RESPONDED;
				gtp_i2c_write(ts->client, rqst_buf, 3);
				ts->rqst_processing = 0;
				GTP_INFO("Request for backup reference responded!");
			} else {
				GTP_ERROR("Requeset for backup reference unresponed!");
			}
			break;

		case GTP_RQST_RESET:
			GTP_INFO("Request for reset.");
			gtp_recovery_reset(ts->client);
			break;

		case GTP_RQST_MAIN_CLOCK:
			GTP_INFO("Request for main clock.");
			ts->rqst_processing = 1;
			ret = gtp_main_clk_proc(ts);
			if (FAIL == ret) {
				GTP_ERROR("Request for main clock unresponded!");
			} else {
				GTP_INFO("Request for main clock responded!");
				rqst_buf[2] = GTP_RQST_RESPONDED;
				gtp_i2c_write(ts->client, rqst_buf, 3);
				ts->rqst_processing = 0;
				ts->clk_chk_fs_times = 0;
			}
			break;

		default:
			GTP_INFO("Undefined request: 0x%02X", rqst_buf[2]);
			rqst_buf[2] = GTP_RQST_RESPONDED;
			gtp_i2c_write(ts->client, rqst_buf, 3);
			break;
		}
	}
#endif
	if (finger == 0x00) {
		if (ts->use_irq) {
			gtp_irq_enable(ts);
		}
		return;
	}

	if ((finger & 0x80) == 0) {
		goto exit_work_func;
	}

	touch_num = finger & 0x0f;
	if (touch_num > GTP_MAX_TOUCH) {
		goto exit_work_func;
	}

	if (touch_num > 1) {
		u8 buf[8 * GTP_MAX_TOUCH] = {(GTP_READ_COOR_ADDR + 10) >> 8, (GTP_READ_COOR_ADDR + 10) & 0xff};

		ret = gtp_i2c_read(ts->client, buf, 2 + 8 * (touch_num - 1));
		memcpy(&point_data[12], &buf[2], 8 * (touch_num - 1));
	}

#if GTP_HAVE_TOUCH_KEY
	key_value = point_data[3 + 8 * touch_num];

	if (key_value || pre_key) {
		for (i = 0; i < GTP_MAX_KEY_NUM; i++) {
		#if GTP_DEBUG_ON
			for (ret = 0; ret < GTP_MAX_KEY_NUM; ++ret) {
				if (key_codes[ret] == touch_key_array[i]) {
					GTP_DEBUG("Key: %s %s", key_names[ret], (key_value & (0x01 << i)) ? "Down" : "Up");
					break;
				}
			}
		#endif
			input_report_key(ts->input_dev, touch_key_array[i], key_value & (0x01<<i));
		}


		if ((pre_key == 0x20) || (key_value == 0x20) || (pre_key == 0x10) || (key_value == 0x10) || (key_value == 0x40) || (pre_key == 0x40)) {

		} else {
			touch_num = 0;
		}
		dev_active = 1;
	}
#endif
	pre_key = key_value;

	GTP_DEBUG("pre_touch:%02x, finger:%02x.", pre_touch, finger);

#if GTP_ICS_SLOT_REPORT

	if (pre_touch || touch_num) {
		s32 pos = 0;
		u16 touch_index = 0;
		u8 report_num = 0;
		coor_data = &point_data[3];

		if (touch_num) {
			id = coor_data[pos];

	#if GTP_WITH_HOVER
			id = coor_data[pos];
			input_x  = coor_data[pos + 1] | (coor_data[pos + 2] << 8);
			input_y  = coor_data[pos + 3] | (coor_data[pos + 4] << 8);
			input_w  = coor_data[pos + 5] | (coor_data[pos + 6] << 8);
			if ((id & 0x80)) {
				if (!input_w) {
					GTP_DEBUG("Pen touch DOWN(Slot)!");

					if (pre_finger) {
						dev_active = 1;
						pre_finger = 0;
						for (i = 0; i < GTP_MAX_TOUCH; i++) {
							gtp_touch_up(ts, i);
						}
					}
					if (0)
						gtp_pen_down(input_x, input_y, input_w, 0);
					pre_pen = 1;
					pre_touch = 1;
					pen_active = 1;
				} else {
					if (pre_pen) {
						GTP_DEBUG("Pen touch UP(Slot)!");
						gtp_pen_up(0);
						pen_active = 1;
						pre_pen = 0;
					}
					id = 9;
				}
			}
	#else
		if ((id & 0x80)) {
			id = 9;
		}
	#endif
			touch_index |= (0x01<<id);
		}
	#if GTP_WITH_HOVER
		else {
			if (pre_pen) {
				GTP_DEBUG("Pen touch UP(Slot)!");
				gtp_pen_up(0);
				pen_active = 1;
				pre_pen = 0;
			}
		}
	#endif
	GTP_DEBUG("id = %d, touch_index = 0x%x, pre_touch = 0x%x\n", id, touch_index, pre_touch);
	for (i = 0; i < GTP_MAX_TOUCH; i++) {
	#if GTP_WITH_HOVER
		if (pre_pen) {
			break;
		}
	#endif
		if ((touch_index & (0x01<<i))) {
			GTP_DEBUG("Devices touch Down(Slot)!");
			pre_finger = 1;
			input_x  = coor_data[pos + 1] | (coor_data[pos + 2] << 8);
			input_y  = coor_data[pos + 3] | (coor_data[pos + 4] << 8);
			input_w  = coor_data[pos + 5] | (coor_data[pos + 6] << 8);

			gtp_touch_down(ts, id, input_x, input_y, input_w);
			pre_touch |= 0x01 << i;

			report_num++;
			if (report_num < touch_num) {
				pos += 8;
				id = coor_data[pos] & 0x0F;
				touch_index |= (0x01<<id);
			}
		} else {
			GTP_DEBUG("Devices touch Up(Slot)!");
			gtp_touch_up(ts, i);
			pre_touch &= ~(0x01 << i);
		}
		dev_active = 1;
	}
	pre_touch = touch_num;
	}
#else

	if (touch_num) {
		for (i = 0; i < touch_num; i++) {
			coor_data = &point_data[i * 8 + 3];
			input_x  = coor_data[1] | (coor_data[2] << 8);
			input_y  = coor_data[3] | (coor_data[4] << 8);
			input_w  = coor_data[5] | (coor_data[6] << 8);
			id = coor_data[0];
		#if GTP_WITH_HOVER

			if ((id & 0x80) && !input_w) {
				GTP_DEBUG("Pen touch DOWN:(%d)(%d, %d)[%d]", id, input_x, input_y, input_w);
				gtp_pen_down(input_x, input_y, input_w, 0);
				pre_pen = 1;
				pen_active = 1;
				if (pre_finger) {
					gtp_touch_up(ts, 0);
					dev_active = 1;
					pre_finger = 0;
				}
			} else
		#endif
			{
				#if GTP_WITH_HOVER
					if (pre_pen) {
						GTP_DEBUG("Pen touch UP!");
						gtp_pen_up(0);
						pre_pen = 0;
						pen_active = 1;
					}
				#endif
				GTP_DEBUG(" (%d)(%d, %d)[%d]", id, input_x, input_y, input_w);
				dev_active = 1;
				pre_finger = 1;
				gtp_touch_down(ts, id, input_x, input_y, input_w);
			}
		}
	} else if (pre_touch) {

	#if GTP_WITH_HOVER
	GTP_DEBUG("@@@@ touch UP , pre_pen is %d, pre_finger is %d!", pre_pen, pre_finger);
		if (pre_pen) {
			GTP_DEBUG("Pen touch UP!");
			gtp_pen_up(0);
			pre_pen = 0;
			pen_active = 1;
		}
	#endif
		if (pre_finger) {
			GTP_DEBUG("Touch Release!");
			dev_active = 1;
			pre_finger = 0;
			gtp_touch_up(ts, 0);
		}
	}

	pre_touch = touch_num;
#endif

#if GTP_WITH_HOVER
	if (pen_active) {
		pen_active = 0;
		input_sync(ts->pen_dev);
	}

#endif
	if (dev_active) {
		dev_active = 0;
		input_sync(ts->input_dev);
	}

exit_work_func:
	if (!ts->gtp_rawdiff_mode) {
		ret = gtp_i2c_write(ts->client, end_cmd, 3);
		if (ret < 0) {
			GTP_INFO("I2C write end_cmd error!");
		}
	}
	if (ts->use_irq) {
		gtp_irq_enable(ts);
	}
}

/*******************************************************
Function:
	Timer interrupt service routine for polling mode.
Input:
	timer: timer struct pointer
Output:
	Timer work mode.
		HRTIMER_NORESTART: no restart mode
*********************************************************/
static enum hrtimer_restart goodix_ts_timer_handler(struct hrtimer *timer)
{
	struct goodix_ts_data *ts = container_of(timer, struct goodix_ts_data, timer);

	GTP_DEBUG_FUNC();

	queue_work(goodix_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, (GTP_POLL_TIME+6)*1000000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

/*******************************************************
Function:
	External interrupt service routine for interrupt mode.
Input:
	irq:  interrupt number.
	dev_id: private data pointer
Output:
	Handle Result.
		IRQ_HANDLED: interrupt handled successfully
*********************************************************/
static irqreturn_t goodix_ts_irq_handler(int irq, void *dev_id)
{
	struct goodix_ts_data *ts = dev_id;

	GTP_DEBUG_FUNC();

	gtp_irq_disable(ts);

	goodix_ts_work_func(&ts->work);

	return IRQ_HANDLED;
}
/*******************************************************
Function:
	Synchronization.
Input:
	ms: synchronization time in millisecond.
Output:
	None.
*******************************************************/
void gtp_int_sync(struct i2c_client *client, s32 ms)
{



	int ret = 0;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
	ret = pinctrl_select_state(ts->ts_pinctrl, ts->eint_output_low);
	if (ret < 0) {
		dev_err(&client->dev, "failed to select pin to eint_output_low");
	}
	msleep(ms);
	ret = pinctrl_select_state(ts->ts_pinctrl, ts->eint_as_int);
	if (ret < 0) {
		dev_err(&client->dev, "failed to select pin to eint_as_int");
	}
}


/*******************************************************
Function:
	Reset chip.
Input:
	ms: reset time in millisecond
Output:
	None.
*******************************************************/
void gtp_reset_guitar(struct i2c_client *client, s32 ms)
{
	struct goodix_ts_data *ts;
#if 1
	ts = i2c_get_clientdata(client);
#endif

	GTP_DEBUG_FUNC();
	GTP_INFO("Guitar reset1 GTP I2C Address: 0x%02x", client->addr);

	pinctrl_select_state(ts->ts_pinctrl, ts->erst_output_low);
	msleep(ms);


	pinctrl_select_state(ts->ts_pinctrl, ts->eint_output_low);

	msleep(4);

	pinctrl_select_state(ts->ts_pinctrl, ts->erst_output_high);

	msleep(10);

	GTP_GPIO_AS_INPUT(gtp_rst_gpio);
	GTP_INFO("Guitar reset2 GTP I2C Address: 0x%02x", client->addr);
#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == ts->chip_type) {
		return;
	}
#endif

	gtp_int_sync(client, 50);
#if GTP_ESD_PROTECT
	gtp_init_ext_watchdog(client);
#endif
}

#if GTP_GESTURE_WAKEUP
/*******************************************************
Function:
	Enter doze mode for sliding wakeup.
Input:
	ts: goodix tp private data
Output:
	1: succeed, otherwise failed
*******************************************************/
static s8 gtp_enter_doze(struct goodix_ts_data *ts)
{
	s8 ret = -1;
	s8 retry = 0;
	u8 i2c_control_buf[3] = {(u8)(GTP_REG_SLEEP >> 8), (u8)GTP_REG_SLEEP, 8};

	GTP_DEBUG_FUNC();

	GTP_DEBUG("Entering gesture mode.");
	while (retry++ < 5) {
		i2c_control_buf[0] = 0x80;
		i2c_control_buf[1] = 0x46;
		ret = gtp_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret < 0) {
			GTP_DEBUG("failed to set doze flag into 0x8046, %d", retry);
			continue;
		}
		i2c_control_buf[0] = 0x80;
		i2c_control_buf[1] = 0x40;
		ret = gtp_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret > 0) {
			doze_status = DOZE_ENABLED;
			GTP_INFO("Gesture mode enabled.");
			return ret;
		}
		msleep(10);
	}
	GTP_ERROR("GTP send gesture cmd failed.");
	return ret;
}
#endif
/*******************************************************
Function:
	Enter sleep mode.
Input:
	ts: private data.
Output:
	Executive outcomes.
	   1: succeed, otherwise failed.
*******************************************************/
static s8 gtp_enter_sleep(struct goodix_ts_data *ts)
{
	s8 ret = -1;
	s8 retry = 0;
	u8 i2c_control_buf[3] = {(u8)(GTP_REG_SLEEP >> 8), (u8)GTP_REG_SLEEP, 5};

#if GTP_COMPATIBLE_MODE
	u8 status_buf[3] = {0x80, 0x44};
#endif

	GTP_DEBUG_FUNC();

#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == ts->chip_type) {

		ret = gtp_i2c_read(ts->client, status_buf, 3);
		if (ret < 0) {
			GTP_ERROR("failed to get backup-reference status");
		}

		if (status_buf[2] & 0x80) {
			ret = gtp_bak_ref_proc(ts, GTP_BAK_REF_STORE);
			if (FAIL == ret) {
				GTP_ERROR("failed to store bak_ref");
			}
		}
	}
#endif


	GTP_INFO("gtp_enter_sleep");
	pinctrl_select_state(ts->ts_pinctrl, ts->eint_output_low);
	msleep(5);

	while (retry++ < 5) {
		ret = gtp_i2c_write(ts->client, i2c_control_buf, 3);
		if (ret > 0) {
			GTP_INFO("GTP enter sleep!");

			return ret;
		}
		msleep(10);
	}
	GTP_ERROR("GTP send sleep cmd failed.");
	return ret;
}
/*******************************************************
Function:
	Wakeup from sleep.
Input:
	ts: private data.
Output:
	Executive outcomes.
		>0: succeed, otherwise: failed.
*******************************************************/
static s8 gtp_wakeup_sleep(struct goodix_ts_data *ts)
{
	u8 retry = 0;
	s8 ret = -1;

	GTP_DEBUG_FUNC();

#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == ts->chip_type) {
		u8 opr_buf[3] = {0x41, 0x80};


		if (pinctrl_select_state(ts->ts_pinctrl, ts->eint_output_high) < 0) {
			dev_err(&ts->client->dev, "failed to select pin to eint_output_high");
		}
		msleep(6);

		for (retry = 0; retry < 10; ++retry) {

			opr_buf[2] = 0x0C;
			ret = gtp_i2c_write(ts->client, opr_buf, 3);
			if (FAIL == ret) {
				GTP_ERROR("failed to hold ss51 & dsp!");
				continue;
			}
			opr_buf[2] = 0x00;
			ret = gtp_i2c_read(ts->client, opr_buf, 3);
			if (FAIL == ret) {
				GTP_ERROR("failed to get ss51 & dsp status!");
				continue;
			}
			if (0x0C != opr_buf[2]) {
				GTP_DEBUG("ss51 & dsp not been hold, %d", retry+1);
				continue;
			}
			GTP_DEBUG("ss51 & dsp confirmed hold");

			ret = gtp_fw_startup(ts->client);
			if (FAIL == ret) {
				GTP_ERROR("failed to startup GT9XXF, process recovery");
				gtp_esd_recovery(ts->client);
			}
			break;
		}
		if (retry >= 10) {
			GTP_ERROR("failed to wakeup, processing esd recovery");
			gtp_esd_recovery(ts->client);
		} else {
			GTP_INFO("GT9XXF gtp wakeup success");
		}
		return ret;
	}
#endif

#if GTP_POWER_CTRL_SLEEP
	while (retry++ < 5) {
		gtp_reset_guitar(ts->client, 20);

		GTP_INFO("GTP wakeup sleep.");
		return 1;
	}
#else
	while (retry++ < 10) {
	#if GTP_GESTURE_WAKEUP
		if (DOZE_WAKEUP != doze_status) {
			GTP_INFO("Powerkey wakeup.");
		} else {
			GTP_INFO("Gesture wakeup.");
		}
		doze_status = DOZE_DISABLED;
		gtp_irq_disable(ts);
		gtp_reset_guitar(ts->client, 20);
		gtp_irq_enable(ts);

	#else

		pinctrl_select_state(ts->ts_pinctrl, ts->eint_output_high);
		msleep(6);
	#endif

		ret = gtp_i2c_test(ts->client);
		if (ret > 0) {
			GTP_INFO("GTP wakeup sleep.");

		#if (!GTP_GESTURE_WAKEUP)
		{
				gtp_int_sync(ts->client, 25);
			#if GTP_ESD_PROTECT
				gtp_init_ext_watchdog(ts->client);
			#endif
			}
		#endif

			return ret;
		}
		gtp_reset_guitar(ts->client, 20);
	}
#endif

	GTP_ERROR("GTP wakeup sleep failed.");
	return ret;
}

/*******************************************************
Function:
	Initialize gtp.
Input:
	ts: goodix private data
Output:
	Executive outcomes.
		0: succeed, otherwise: failed
*******************************************************/
static s32 gtp_init_panel(struct goodix_ts_data *ts)
{
	s32 ret = -1;

#if GTP_DRIVER_SEND_CFG
	s32 i = 0;
	u8 check_sum = 0;
	u8 opr_buf[16] = {0};
	u8 sensor_id = 0;
	u8 drv_cfg_version;
	u8 flash_cfg_version;

/* if defined CONFIG_OF, parse config data from dtsi
 *  else parse config data form header file.
 */
#ifndef	GTP_CONFIG_OF
	u8 cfg_info_group0[] = CTP_CFG_GROUP0;
	u8 cfg_info_group1[] = CTP_CFG_GROUP1;
	u8 cfg_info_group2[] = CTP_CFG_GROUP2;
	u8 cfg_info_group3[] = CTP_CFG_GROUP3;
	u8 cfg_info_group4[] = CTP_CFG_GROUP4;
	u8 cfg_info_group5[] = CTP_CFG_GROUP5;

	u8 *send_cfg_buf[] = {cfg_info_group0, cfg_info_group1,
						 cfg_info_group2, cfg_info_group3,
						 cfg_info_group4, cfg_info_group5};
	u8 cfg_info_len[] = { CFG_GROUP_LEN(cfg_info_group0),
						  CFG_GROUP_LEN(cfg_info_group1),
						  CFG_GROUP_LEN(cfg_info_group2),
						  CFG_GROUP_LEN(cfg_info_group3),
						  CFG_GROUP_LEN(cfg_info_group4),
						  CFG_GROUP_LEN(cfg_info_group5)};

	GTP_DEBUG("Config Groups\' Lengths: %d, %d, %d, %d, %d, %d",
		cfg_info_len[0], cfg_info_len[1], cfg_info_len[2], cfg_info_len[3],
		cfg_info_len[4], cfg_info_len[5]);
#endif

#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == ts->chip_type) {
		ts->fw_error = 0;
	} else
#endif
	{	/* check firmware */
		ret = gtp_i2c_read_dbl_check(ts->client, 0x41E4, opr_buf, 1);
		if (SUCCESS == ret) {
			if (opr_buf[0] != 0xBE) {
				ts->fw_error = 1;
				GTP_ERROR("Firmware error, no config sent!");
				return -EPERM;
			}
		}
	}

	/* read sensor id */
#if GTP_COMPATIBLE_MODE
	msleep(50);
#endif
	ret = gtp_i2c_read_dbl_check(ts->client, GTP_REG_SENSOR_ID, &sensor_id, 1);
	if (SUCCESS == ret) {
		if (sensor_id >= 0x06) {
			GTP_ERROR("Invalid sensor_id(0x%02X), No Config Sent!", sensor_id);
			ts->pnl_init_error = 1;
			return -EPERM;
		}
	} else {
		GTP_ERROR("Failed to get sensor_id, No config sent!");
		ts->pnl_init_error = 1;
		return -EPERM;
	}
	GTP_INFO("Sensor_ID: %d", sensor_id);

	/* parse config data*/
#ifdef GTP_CONFIG_OF
	GTP_DEBUG("Get config data from device tree.");
	ret = gtp_parse_dt_cfg(&ts->client->dev, &config[GTP_ADDR_LENGTH], &ts->gtp_cfg_len, sensor_id);
	if (ret < 0) {
		GTP_ERROR("Failed to parse config data form device tree.");
		ts->pnl_init_error = 1;
		return -EPERM;
	}
#else
	GTP_DEBUG("Get config data from header file.");
	if ((!cfg_info_len[1]) && (!cfg_info_len[2]) &&
		(!cfg_info_len[3]) && (!cfg_info_len[4]) &&
		(!cfg_info_len[5])) {
		sensor_id = 0;
	}
	ts->gtp_cfg_len = cfg_info_len[sensor_id];
	memset(&config[GTP_ADDR_LENGTH], 0, GTP_CONFIG_MAX_LENGTH);
	memcpy(&config[GTP_ADDR_LENGTH], send_cfg_buf[sensor_id], ts->gtp_cfg_len);
#endif

	GTP_INFO("Config group%d used,length: %d", sensor_id, ts->gtp_cfg_len);

	if (ts->gtp_cfg_len < GTP_CONFIG_MIN_LENGTH) {
		GTP_ERROR("Config Group%d is INVALID CONFIG GROUP(Len: %d)! NO Config Sent! You need to check you header file CFG_GROUP section!", sensor_id, ts->gtp_cfg_len);
		ts->pnl_init_error = 1;
		return -EPERM;
	}

#if GTP_COMPATIBLE_MODE
	if (ts->chip_type != CHIP_TYPE_GT9F)
#endif
	{
		ret = gtp_i2c_read_dbl_check(ts->client, GTP_REG_CONFIG_DATA, &opr_buf[0], 1);
		if (ret == SUCCESS) {
			GTP_DEBUG("Config Version: %d, 0x%02X; IC Config Version: %d, 0x%02X",
			config[GTP_ADDR_LENGTH], config[GTP_ADDR_LENGTH], opr_buf[0], opr_buf[0]);

			flash_cfg_version = opr_buf[0];
			drv_cfg_version = config[GTP_ADDR_LENGTH];

			if (flash_cfg_version < 90 && flash_cfg_version > drv_cfg_version) {
				config[GTP_ADDR_LENGTH] = 0x00;
			}
		} else {
			GTP_ERROR("Failed to get ic config version!No config sent!");
			return -EPERM;
		}
	}

#if GTP_CUSTOM_CFG
	config[RESOLUTION_LOC]     = (u8)GTP_MAX_WIDTH;
	config[RESOLUTION_LOC + 1] = (u8)(GTP_MAX_WIDTH>>8);
	config[RESOLUTION_LOC + 2] = (u8)GTP_MAX_HEIGHT;
	config[RESOLUTION_LOC + 3] = (u8)(GTP_MAX_HEIGHT>>8);

	if (GTP_INT_TRIGGER == 0) {
		config[TRIGGER_LOC] &= 0xfe;
	} else if (GTP_INT_TRIGGER == 1) {
		config[TRIGGER_LOC] |= 0x01;
	}
#endif

	check_sum = 0;
	for (i = GTP_ADDR_LENGTH; i < ts->gtp_cfg_len; i++) {
		check_sum += config[i];
	}
	config[ts->gtp_cfg_len] = (~check_sum) + 1;

#else

	ts->gtp_cfg_len = GTP_CONFIG_MAX_LENGTH;
	ret = gtp_i2c_read(ts->client, config, ts->gtp_cfg_len + GTP_ADDR_LENGTH);
	if (ret < 0) {
		GTP_ERROR("Read Config Failed, Using Default Resolution & INT Trigger!");
		ts->abs_x_max = GTP_MAX_WIDTH;
		ts->abs_y_max = GTP_MAX_HEIGHT;
		ts->int_trigger_type = GTP_INT_TRIGGER;
	}

#endif

	if ((ts->abs_x_max == 0) && (ts->abs_y_max == 0)) {
		ts->abs_x_max = (config[RESOLUTION_LOC + 1] << 8) + config[RESOLUTION_LOC];
		ts->abs_y_max = (config[RESOLUTION_LOC + 3] << 8) + config[RESOLUTION_LOC + 2];
		ts->int_trigger_type = (config[TRIGGER_LOC]) & 0x03;
	}

#if GTP_COMPATIBLE_MODE
	if (CHIP_TYPE_GT9F == ts->chip_type) {
		u8 sensor_num = 0;
		u8 driver_num = 0;
		u8 have_key = 0;

		have_key = (config[GTP_REG_HAVE_KEY - GTP_REG_CONFIG_DATA + 2] & 0x01);

		if (1 == ts->is_950) {
			driver_num = config[GTP_REG_MATRIX_DRVNUM - GTP_REG_CONFIG_DATA + 2];
			sensor_num = config[GTP_REG_MATRIX_SENNUM - GTP_REG_CONFIG_DATA + 2];
			if (have_key) {
				driver_num--;
			}
			ts->bak_ref_len = (driver_num * (sensor_num - 1) + 2) * 2 * 6;
		} else {
			driver_num = (config[CFG_LOC_DRVA_NUM] & 0x1F) + (config[CFG_LOC_DRVB_NUM]&0x1F);
			if (have_key) {
				driver_num--;
			}
			sensor_num = (config[CFG_LOC_SENS_NUM] & 0x0F) + ((config[CFG_LOC_SENS_NUM] >> 4) & 0x0F);
			ts->bak_ref_len = (driver_num * (sensor_num - 2) + 2) * 2;
		}

		GTP_INFO("Drv * Sen: %d * %d(key: %d), X_MAX: %d, Y_MAX: %d, TRIGGER: 0x%02x",
				 driver_num, sensor_num, have_key, ts->abs_x_max, ts->abs_y_max, ts->int_trigger_type);
		return 0;
	} else
#endif
	{
#if GTP_DRIVER_SEND_CFG
		ret = gtp_send_cfg(ts->client);
		if (ret < 0) {
			GTP_ERROR("Send config error.");
		}
#if GTP_COMPATIBLE_MODE
	if (ts->chip_type != CHIP_TYPE_GT9F)
#endif
	{
		if (flash_cfg_version < 90 && flash_cfg_version > drv_cfg_version) {
			check_sum = 0;
			config[GTP_ADDR_LENGTH] = drv_cfg_version;
			for (i = GTP_ADDR_LENGTH; i < ts->gtp_cfg_len; i++) {
				check_sum += config[i];
			}
			config[ts->gtp_cfg_len] = (~check_sum) + 1;
		}
	}

#endif
		GTP_INFO("X_MAX: %d, Y_MAX: %d, TRIGGER: 0x%02x", ts->abs_x_max, ts->abs_y_max, ts->int_trigger_type);
	}

	msleep(10);
	return 0;

}

static ssize_t gt91xx_config_read_proc(struct file *file, char __user *page, size_t size, loff_t *ppos)
{

	int i, len = 0;
	char def_config[64];
	char *ptr_config = def_config;
	ssize_t total_size = 0;
	char temp_data[GTP_CONFIG_MAX_LENGTH + 2] = {0x80, 0x47};


	if (!page || !ppos || *ppos)
	return 0;

	len = sprintf(def_config, "==== GT9XX config init value====\n");


	if ((total_size + len) >= size - 1) {
	pr_info("config size length is too big\n");
		goto copy_err;
	}

	if (copy_to_user((page + total_size), def_config, len)) {
	pr_err("%s: fail to copy default config\n", __func__);
		total_size = -EFAULT;
		goto copy_err;
	}

	*ppos += len;
	total_size += len;

	for (i = 0 ; i < GTP_CONFIG_MAX_LENGTH ; i++) {
	ptr_config += sprintf(ptr_config, "0x%02X ", config[i + 2]);

		if (i % 8 == 7) {
			ptr_config += sprintf(ptr_config, "\n");
			len =  ptr_config - def_config;

			ptr_config = def_config;
			if ((total_size + len) >= size - 1) {
				pr_info("config size length is too big\n");
				goto copy_err;
			}

			if (copy_to_user((page + total_size), def_config, len)) {
				pr_err("%s: fail to copy default config\n", __func__);
				total_size = -EFAULT;
				goto copy_err;
			}

			*ppos += len;
			total_size += len;
		}
	}

	ptr_config += sprintf(ptr_config, "\n==== GT9XX config real value====\n");
	len = ptr_config - def_config;


	if ((total_size + len) >= size - 1) {
	pr_info("config size length is too big\n");
		goto copy_err;
	}

	if (copy_to_user((page + total_size), def_config, len)) {
		pr_err("%s: fail to copy default config\n", __func__);
		total_size = -EFAULT;
		goto copy_err;
	}

	*ppos += len;
	total_size += len;
	ptr_config = def_config;

	gtp_i2c_read(i2c_connect_client, temp_data, GTP_CONFIG_MAX_LENGTH + 2);
	for (i = 0 ; i < GTP_CONFIG_MAX_LENGTH ; i++) {
		ptr_config += sprintf(ptr_config, "0x%02X ", temp_data[i+2]);

		if (i % 8 == 7) {
		ptr_config += sprintf(ptr_config, "\n");
		len =  ptr_config - def_config;

		ptr_config = def_config;
		if ((total_size + len) >= size - 1) {
			pr_info("config size length is too big\n");
			goto copy_err;
		}

		if (copy_to_user((page + total_size), def_config, len)) {
			pr_err("%s: fail to copy default config\n", __func__);
			total_size = -EFAULT;
			goto copy_err;
		}

		*ppos += len;
		total_size += len;
		}
	}

	len = ptr_config - def_config;

	if (len > 0) {
	if ((total_size + len) >= size - 1) {
		pr_info("config size length is too big\n");
		goto copy_err;
	}

	if (copy_to_user((page + total_size), def_config, len)) {
			pr_err("%s: fail to copy default config\n", __func__);
			total_size = -EFAULT;
			goto copy_err;
	}

	*ppos += len;
	total_size += len;
	}
	pr_info("%s::read total size is %d\n", __func__, total_size);

copy_err:
	return total_size;

}


static ssize_t gt91xx_config_write_proc(struct file *filp, const char __user *buffer, size_t count, loff_t *off)
{
	s32 ret = 0;

	GTP_DEBUG("write count %d\n", count);

	if (count > GTP_CONFIG_MAX_LENGTH) {
		GTP_ERROR("size not match [%d:%d]\n", GTP_CONFIG_MAX_LENGTH, count);
		return -EFAULT;
	}

	if (copy_from_user(&config[2], buffer, count)) {
		GTP_ERROR("copy from user fail\n");
		return -EFAULT;
	}

	ret = gtp_send_cfg(i2c_connect_client);

	if (ret < 0) {
		GTP_ERROR("send config failed.");
	}

	return count;
}
/*******************************************************
Function:
	Read chip version.
Input:
	client:  i2c device
	version: buffer to keep ic firmware version
Output:
	read operation return.
		2: succeed, otherwise: failed
*******************************************************/
s32 gtp_read_version(struct i2c_client *client, u16 *version)
{
	s32 ret = -1;
	u8 buf[8] = {GTP_REG_VERSION >> 8, GTP_REG_VERSION & 0xff};

	GTP_DEBUG_FUNC();

	ret = gtp_i2c_read(client, buf, sizeof(buf));
	if (ret < 0) {
		GTP_ERROR("GTP read version failed");
		return ret;
	}

	if (version) {
		*version = (buf[7] << 8) | buf[6];
	}
	if (buf[5] == 0x00) {
		GTP_INFO("IC Version: %c%c%c_%02x%02x", buf[2], buf[3], buf[4], buf[7], buf[6]);

	#ifdef SUPPORT_READ_TP_VERSION
	strlcpy(product_id, &buf[2], GTP_PRODUCT_ID_MAXSIZE - 1);
	#endif

	} else {
		GTP_INFO("IC Version: %c%c%c%c_%02x%02x", buf[2], buf[3], buf[4], buf[5], buf[7], buf[6]);

	#ifdef SUPPORT_READ_TP_VERSION
	strlcpy(product_id, &buf[2], GTP_PRODUCT_ID_MAXSIZE);
	#endif

	}

	return ret;
}

/*******************************************************
Function:
	I2c test Function.
Input:
	client:i2c client.
Output:
	Executive outcomes.
		2: succeed, otherwise failed.
*******************************************************/
static s8 gtp_i2c_test(struct i2c_client *client)
{
	u8 test[3] = {GTP_REG_CONFIG_DATA >> 8, GTP_REG_CONFIG_DATA & 0xff};
	u8 retry = 0;
	s8 ret = -1;

	GTP_DEBUG_FUNC();

	while (retry++ < 3) {
		ret = gtp_i2c_read(client, test, 3);
		if (ret > 0) {
			return ret;
		}
		GTP_ERROR("GTP i2c test failed time %d.", retry);
		msleep(10);
	}
	return ret;
}

/*******************************************************
Function:
	Request gpio(INT & RST) ports.
Input:
	ts: private data.
Output:
	Executive outcomes.
		>= 0: succeed, < 0: failed
*******************************************************/
static s8 gtp_request_io_port(struct goodix_ts_data *ts)
{
	s32 ret = 0;

	GTP_DEBUG_FUNC();
	ret = GTP_GPIO_REQUEST(gtp_int_gpio, "GTP INT IRQ");
	if (ret < 0) {
		GTP_ERROR("Failed to request GPIO:%d, ERRNO:%d", (s32)gtp_int_gpio, ret);
		ret = -ENODEV;
	} else {

		if (pinctrl_select_state(ts->ts_pinctrl, ts->eint_as_int) < 0) {
			dev_err(&ts->client->dev, "failed to select pin to eint_as_int");
		}
		ts->client->irq = gpio_to_irq(gtp_int_gpio);
	}

	ret = GTP_GPIO_REQUEST(gtp_rst_gpio, "GTP RST PORT");
	if (ret < 0) {
		GTP_ERROR("Failed to request GPIO:%d, ERRNO:%d", (s32)gtp_rst_gpio, ret);
		ret = -ENODEV;
	}

	GTP_GPIO_AS_INPUT(gtp_rst_gpio);

	gtp_reset_guitar(ts->client, 20);

	if (ret < 0) {
		GTP_GPIO_FREE(gtp_rst_gpio);
		GTP_GPIO_FREE(gtp_int_gpio);
		pinctrl_select_state(ts->ts_pinctrl, ts->eint_as_int);


	}

	return ret;
}

/*******************************************************
Function:
	Request interrupt.
Input:
	ts: private data.
Output:
	Executive outcomes.
		0: succeed, -1: failed.
*******************************************************/
static s8 gtp_request_irq(struct goodix_ts_data *ts)
{
	s32 ret = -1;


	GTP_DEBUG_FUNC();
	GTP_DEBUG("INT trigger type:%x", ts->int_trigger_type);

	GTP_DEBUG("gtp_request_irq modify by likang i2c->irq: %d, i2c_client->name:%s.", ts->client->irq, ts->client->name);
	ret = request_threaded_irq(ts->client->irq, NULL,
			goodix_ts_irq_handler,
			IRQ_TYPE_EDGE_RISING | IRQF_ONESHOT,
			ts->client->name,
			ts);
	if (ret) {
		GTP_ERROR("Request IRQ failed!ERRNO:%d.", ret);

		if (pinctrl_select_state(ts->ts_pinctrl, ts->eint_as_int) < 0) {
			dev_err(&ts->client->dev, "failed to select pin to eint_as_int");
		}

		GTP_GPIO_FREE(gtp_int_gpio);
		pinctrl_select_state(ts->ts_pinctrl, ts->eint_as_int);

		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = goodix_ts_timer_handler;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		return -EPERM;
	} else {
		gtp_irq_disable(ts);
		ts->use_irq = 1;
		return 0;
	}
}

/*******************************************************
Function:
	Request input device Function.
Input:
	ts:private data.
Output:
	Executive outcomes.
		0: succeed, otherwise: failed.
*******************************************************/
static s8 gtp_request_input_dev(struct goodix_ts_data *ts)
{
	s8 ret = -1;
#if GTP_HAVE_TOUCH_KEY
	u8 index = 0;
#endif

	GTP_DEBUG_FUNC();

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		GTP_ERROR("Failed to allocate input device.");
		return -ENOMEM;
	}

	ts->input_dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS) ;
#if GTP_ICS_SLOT_REPORT
	input_mt_init_slots(ts->input_dev, 16, 0);
#else
	ts->input_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);
#endif
	__set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

#if GTP_HAVE_TOUCH_KEY
	for (index = 0; index < GTP_MAX_KEY_NUM; index++) {
		input_set_capability(ts->input_dev, EV_KEY, touch_key_array[index]);
	}
#endif

#if GTP_GESTURE_WAKEUP

	ts->input_dev->event = goodix_select_gesture_mode;
	input_set_capability(ts->input_dev, EV_KEY, KEY_WAKEUP);

#endif

#if GTP_CHANGE_X2Y
	GTP_SWAP(ts->abs_x_max, ts->abs_y_max);
#endif

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->abs_x_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->abs_y_max, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 255, 0, 0);

	input_set_abs_params(ts->input_dev, ABS_MT_TOOL_TYPE, 0, MT_TOOL_MAX, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 1023, 0, 0);

	ts->input_dev->name = goodix_ts_name;
	ts->input_dev->phys = goodix_input_phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->id.vendor = 0xDEAD;
	ts->input_dev->id.product = 0xBEEF;
	ts->input_dev->id.version = 10427;

	ret = input_register_device(ts->input_dev);
	if (ret) {
		GTP_ERROR("Register %s input device failed", ts->input_dev->name);
		return -ENODEV;
	}

#if GTP_WITH_HOVER
	gtp_pen_init(ts);
#endif

	return 0;
}


#if GTP_COMPATIBLE_MODE

s32 gtp_fw_startup(struct i2c_client *client)
{
	u8 opr_buf[4];
	s32 ret = 0;


	opr_buf[0] = 0xAA;
	ret = i2c_write_bytes(client, 0x8041, opr_buf, 1);
	if (ret < 0) {
		return FAIL;
	}


	opr_buf[0] = 0x00;
	ret = i2c_write_bytes(client, 0x4180, opr_buf, 1);
	if (ret < 0) {
		return FAIL;
	}

	gtp_int_sync(client, 25);


	ret = i2c_read_bytes(client, 0x8041, opr_buf, 1);
	if (ret < 0) {
		return FAIL;
	}
	if (0xAA == opr_buf[0]) {
		GTP_ERROR("IC works abnormally,startup failed.");
		return FAIL;
	} else {
		GTP_INFO("IC works normally, Startup success.");
		opr_buf[0] = 0xAA;
		i2c_write_bytes(client, 0x8041, opr_buf, 1);
		return SUCCESS;
	}
}

static s32 gtp_esd_recovery(struct i2c_client *client)
{
	s32 retry = 0;
	s32 ret = 0;
	struct goodix_ts_data *ts;

	ts = i2c_get_clientdata(client);

	gtp_irq_disable(ts);

	GTP_INFO("GT9XXF esd recovery mode");
	for (retry = 0; retry < 5; retry++) {
		ret = gup_fw_download_proc(NULL, GTP_FL_ESD_RECOVERY);
		if (FAIL == ret) {
			GTP_ERROR("esd recovery failed %d", retry+1);
			continue;
		}
		ret = gtp_fw_startup(ts->client);
		if (FAIL == ret) {
			GTP_ERROR("GT9XXF start up failed %d", retry+1);
			continue;
		}
		break;
	}
	gtp_irq_enable(ts);

	if (retry >= 5) {
		GTP_ERROR("failed to esd recovery");
		return FAIL;
	}

	GTP_INFO("Esd recovery successful");
	return SUCCESS;
}

void gtp_recovery_reset(struct i2c_client *client)
{
#if GTP_ESD_PROTECT
	gtp_esd_switch(client, SWITCH_OFF);
#endif
	GTP_DEBUG_FUNC();

	gtp_esd_recovery(client);

#if GTP_ESD_PROTECT
	gtp_esd_switch(client, SWITCH_ON);
#endif
}

static s32 gtp_bak_ref_proc(struct goodix_ts_data *ts, u8 mode)
{
	s32 ret = 0;
	s32 i = 0;
	s32 j = 0;
	u16 ref_sum = 0;
	u16 learn_cnt = 0;
	u16 chksum = 0;
	s32 ref_seg_len = 0;
	s32 ref_grps = 0;
	struct file *ref_filp = NULL;
	u8 *p_bak_ref;

	ret = gup_check_fs_mounted("/data");
	if (FAIL == ret) {
		ts->ref_chk_fs_times++;
		GTP_DEBUG("Ref check /data times/MAX_TIMES: %d / %d", ts->ref_chk_fs_times, GTP_CHK_FS_MNT_MAX);
		if (ts->ref_chk_fs_times < GTP_CHK_FS_MNT_MAX) {
			msleep(50);
			GTP_INFO("/data not mounted.");
			return FAIL;
		}
		GTP_INFO("check /data mount timeout...");
	} else {
		GTP_INFO("/data mounted!!!(%d/%d)", ts->ref_chk_fs_times, GTP_CHK_FS_MNT_MAX);
	}

	p_bak_ref = (u8 *)kzalloc(ts->bak_ref_len, GFP_KERNEL);

	if (NULL == p_bak_ref) {
		GTP_ERROR("Allocate memory for p_bak_ref failed!");
		return FAIL;
	}

	if (ts->is_950) {
		ref_seg_len = ts->bak_ref_len / 6;
		ref_grps = 6;
	} else {
		ref_seg_len = ts->bak_ref_len;
		ref_grps = 1;
	}
	ref_filp = filp_open(GTP_BAK_REF_PATH, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(ref_filp)) {
		GTP_ERROR("Failed to open/create %s.", GTP_BAK_REF_PATH);
		if (GTP_BAK_REF_SEND == mode) {
			goto bak_ref_default;
		} else {
			goto bak_ref_exit;
		}
	}

	switch (mode) {
	case GTP_BAK_REF_SEND:
		GTP_INFO("Send backup-reference");
		ref_filp->f_op->llseek(ref_filp, 0, SEEK_SET);
		ret = ref_filp->f_op->read(ref_filp, (char *)p_bak_ref, ts->bak_ref_len, &ref_filp->f_pos);
		if (ret < 0) {
			GTP_ERROR("failed to read bak_ref info from file, sending defualt bak_ref");
			goto bak_ref_default;
		}
		for (j = 0; j < ref_grps; ++j) {
			ref_sum = 0;
			for (i = 0; i < (ref_seg_len); i += 2) {
				ref_sum += (p_bak_ref[i + j * ref_seg_len] << 8) + p_bak_ref[i + 1 + j * ref_seg_len];
			}
			learn_cnt = (p_bak_ref[j * ref_seg_len + ref_seg_len - 4] << 8) + (p_bak_ref[j * ref_seg_len + ref_seg_len - 3]);
			chksum = (p_bak_ref[j * ref_seg_len + ref_seg_len - 2] << 8) + (p_bak_ref[j * ref_seg_len + ref_seg_len - 1]);
			GTP_DEBUG("learn count = %d", learn_cnt);
			GTP_DEBUG("chksum = %d", chksum);
			GTP_DEBUG("ref_sum = 0x%04X", ref_sum & 0xFFFF);

			if (1 != ref_sum) {
				GTP_INFO("wrong chksum for bak_ref, reset to 0x00 bak_ref");
				memset(&p_bak_ref[j * ref_seg_len], 0, ref_seg_len);
				p_bak_ref[ref_seg_len + j * ref_seg_len - 1] = 0x01;
			} else {
				if (j == (ref_grps - 1)) {
					GTP_INFO("backup-reference data in %s used", GTP_BAK_REF_PATH);
				}
			}
		}
		ret = i2c_write_bytes(ts->client, GTP_REG_BAK_REF, p_bak_ref, ts->bak_ref_len);
		if (FAIL == ret) {
			GTP_ERROR("failed to send bak_ref because of iic comm error");
			goto bak_ref_exit;
		}
		break;

	case GTP_BAK_REF_STORE:
		GTP_INFO("Store backup-reference");
		ret = i2c_read_bytes(ts->client, GTP_REG_BAK_REF, p_bak_ref, ts->bak_ref_len);
		if (ret < 0) {
			GTP_ERROR("failed to read bak_ref info, sending default back-reference");
			goto bak_ref_default;
		}
		ref_filp->f_op->llseek(ref_filp, 0, SEEK_SET);
		ref_filp->f_op->write(ref_filp, (char *)p_bak_ref, ts->bak_ref_len, &ref_filp->f_pos);
		break;

	default:
		GTP_ERROR("invalid backup-reference request");
		break;
	}
	ret = SUCCESS;
	goto bak_ref_exit;

bak_ref_default:

	for (j = 0; j < ref_grps; ++j) {
		memset(&p_bak_ref[j * ref_seg_len], 0, ref_seg_len);
		p_bak_ref[j * ref_seg_len + ref_seg_len - 1] = 0x01;
	}
	ret = i2c_write_bytes(ts->client, GTP_REG_BAK_REF, p_bak_ref, ts->bak_ref_len);
	if (!IS_ERR(ref_filp)) {
		GTP_INFO("write backup-reference data into %s", GTP_BAK_REF_PATH);
		ref_filp->f_op->llseek(ref_filp, 0, SEEK_SET);
		ref_filp->f_op->write(ref_filp, (char *)p_bak_ref, ts->bak_ref_len, &ref_filp->f_pos);
	}
	if (ret == FAIL) {
		GTP_ERROR("failed to load the default backup reference");
	}

bak_ref_exit:

	if (p_bak_ref) {
		kfree(p_bak_ref);
	}
	if (ref_filp && !IS_ERR(ref_filp)) {
		filp_close(ref_filp, NULL);
	}
	return ret;
}


static s32 gtp_verify_main_clk(u8 *p_main_clk)
{
	u8 chksum = 0;
	u8 main_clock = p_main_clk[0];
	s32 i = 0;

	if (main_clock < 50 || main_clock > 120) {
		return FAIL;
	}

	for (i = 0; i < 5; ++i) {
		if (main_clock != p_main_clk[i]) {
			return FAIL;
		}
		chksum += p_main_clk[i];
	}
	chksum += p_main_clk[5];
	if ((chksum) == 0) {
		return SUCCESS;
	} else {
		return FAIL;
	}
}

static s32 gtp_main_clk_proc(struct goodix_ts_data *ts)
{
	s32 ret = 0;
	s32 i = 0;
	s32 clk_chksum = 0;
	struct file *clk_filp = NULL;
	u8 p_main_clk[6] = {0};

	ret = gup_check_fs_mounted("/data");
	if (FAIL == ret) {
		ts->clk_chk_fs_times++;
		GTP_DEBUG("Clock check /data times/MAX_TIMES: %d / %d", ts->clk_chk_fs_times, GTP_CHK_FS_MNT_MAX);
		if (ts->clk_chk_fs_times < GTP_CHK_FS_MNT_MAX) {
			msleep(50);
			GTP_INFO("/data not mounted.");
			return FAIL;
		}
		GTP_INFO("Check /data mount timeout!");
	} else {
		GTP_INFO("/data mounted!!!(%d/%d)", ts->clk_chk_fs_times, GTP_CHK_FS_MNT_MAX);
	}

	clk_filp = filp_open(GTP_MAIN_CLK_PATH, O_RDWR | O_CREAT, 0666);
	if (IS_ERR(clk_filp)) {
		GTP_ERROR("%s is unavailable, calculate main clock", GTP_MAIN_CLK_PATH);
	} else {
		clk_filp->f_op->llseek(clk_filp, 0, SEEK_SET);
		clk_filp->f_op->read(clk_filp, (char *)p_main_clk, 6, &clk_filp->f_pos);

		ret = gtp_verify_main_clk(p_main_clk);
		if (FAIL == ret) {

			GTP_ERROR("main clock data in %s is wrong, recalculate main clock", GTP_MAIN_CLK_PATH);
		} else {
			GTP_INFO("main clock data in %s used, main clock freq: %d", GTP_MAIN_CLK_PATH, p_main_clk[0]);
			filp_close(clk_filp, NULL);
			goto update_main_clk;
		}
	}

#if GTP_ESD_PROTECT
	gtp_esd_switch(ts->client, SWITCH_OFF);
#endif
	ret = gup_clk_calibration();
	gtp_esd_recovery(ts->client);

#if GTP_ESD_PROTECT
	gtp_esd_switch(ts->client, SWITCH_ON);
#endif

	GTP_INFO("calibrate main clock: %d", ret);
	if (ret < 50 || ret > 120) {
		GTP_ERROR("wrong main clock: %d", ret);
		goto exit_main_clk;
	}


	for (i = 0; i < 5; ++i) {
		p_main_clk[i] = ret;
		clk_chksum += p_main_clk[i];
	}
	p_main_clk[5] = 0 - clk_chksum;

	if (!IS_ERR(clk_filp)) {
		GTP_DEBUG("write main clock data into %s", GTP_MAIN_CLK_PATH);
		clk_filp->f_op->llseek(clk_filp, 0, SEEK_SET);
		clk_filp->f_op->write(clk_filp, (char *)p_main_clk, 6, &clk_filp->f_pos);
		filp_close(clk_filp, NULL);
	}

update_main_clk:
	ret = i2c_write_bytes(ts->client, GTP_REG_MAIN_CLK, p_main_clk, 6);
	if (FAIL == ret) {
		GTP_ERROR("update main clock failed!");
		return FAIL;
	}
	return SUCCESS;

exit_main_clk:
	if (!IS_ERR(clk_filp)) {
		filp_close(clk_filp, NULL);
	}
	return FAIL;
}


s32 gtp_gt9xxf_init(struct i2c_client *client)
{
	s32 ret = 0;

	ret = gup_fw_download_proc(NULL, GTP_FL_FW_BURN);
	if (FAIL == ret) {
		return FAIL;
	}

	ret = gtp_fw_startup(client);
	if (FAIL == ret) {
		return FAIL;
	}
	return SUCCESS;
}

void gtp_get_chip_type(struct goodix_ts_data *ts)
{
	u8 opr_buf[10] = {0x00};
	s32 ret = 0;

	msleep(10);

	ret = gtp_i2c_read_dbl_check(ts->client, GTP_REG_CHIP_TYPE, opr_buf, 10);

	if (FAIL == ret) {
		GTP_ERROR("Failed to get chip-type, set chip type default: GOODIX_GT9");
		ts->chip_type = CHIP_TYPE_GT9;
		return;
	}

	if (!memcmp(opr_buf, "GOODIX_GT9", 10)) {
		ts->chip_type = CHIP_TYPE_GT9;
	} else {
		ts->chip_type = CHIP_TYPE_GT9F;
	}
	GTP_INFO("Chip Type: %s", (ts->chip_type == CHIP_TYPE_GT9) ? "GOODIX_GT9" : "GOODIX_GT9F");
}
#endif

/*
 * Devices Tree support,
*/
#ifdef GTP_CONFIG_OF
/**
 * gtp_parse_dt - parse platform infomation form devices tree.
 */
static void gtp_parse_dt(struct device *dev)
{
	struct device_node *np = dev->of_node;

	gtp_int_gpio = of_get_named_gpio(np, "goodix,irq-gpio", 0);
	gtp_rst_gpio = of_get_named_gpio(np, "goodix,reset-gpio", 0);

}

/**
 * gtp_parse_dt_cfg - parse config data from devices tree.
 * @dev: device that this driver attached.
 * @cfg: pointer of the config array.
 * @cfg_len: pointer of the config length.
 * @sid: sensor id.
 * Return: 0-succeed, -1-faileds
 */
int gtp_parse_dt_cfg(struct device *dev, u8 *cfg, int *cfg_len, u8 sid)
{
	struct device_node *np = dev->of_node;
	struct property *prop;
	char cfg_name[18];

	snprintf(cfg_name, sizeof(cfg_name), "goodix,cfg-group%d", sid);
	prop = of_find_property(np, cfg_name, cfg_len);
	if (!prop || !prop->value || *cfg_len == 0 || *cfg_len > GTP_CONFIG_MAX_LENGTH) {
		return -EPERM;/* failed */
	} else {
		memcpy(cfg, prop->value, *cfg_len);
		return 0;
	}
}

/**
 * gtp_power_switch - power switch .
 * @on: 1-switch on, 0-switch off.
 * return: 0-succeed, -1-faileds
 */
static int gtp_power_switch(struct i2c_client *client, int on)
{
	static struct regulator *vdd_ana;
	static struct regulator *vcc_i2c;
	int ret;

	if (!vdd_ana) {
		vdd_ana = regulator_get(&client->dev, "vdd_ana");
		if (IS_ERR(vdd_ana)) {
			GTP_ERROR("regulator get of vdd_ana failed");
			ret = PTR_ERR(vdd_ana);
			vdd_ana = NULL;
			return ret;
		}
	}

	if (!vcc_i2c) {
		vcc_i2c = regulator_get(&client->dev, "vcc_i2c");
		if (IS_ERR(vcc_i2c)) {
			GTP_ERROR("regulator get of vcc_i2c failed");
			ret = PTR_ERR(vcc_i2c);
			vcc_i2c = NULL;
			goto ERR_GET_VCC;
		}
	}

	if (on) {

		struct goodix_ts_data *ts = i2c_get_clientdata(client);
		GTP_DEBUG("GTP power on.");
		pinctrl_select_state(ts->ts_pinctrl, ts->erst_output_low);
		ret = regulator_enable(vdd_ana);
		udelay(2);
		ret = regulator_enable(vcc_i2c);

	} else {
		GTP_DEBUG("GTP power off.");
		ret = regulator_disable(vcc_i2c);
		udelay(2);
		ret = regulator_disable(vdd_ana);
	}

	return ret;

ERR_GET_VCC:
	regulator_put(vdd_ana);
	return ret;
}
#endif


#if GTP_GESTURE_WAKEUP
struct kobject *goodix_gesture_kobj = NULL;
static int gesture_enable_flag = 0, gesture_suspend_resume_flag = 0;

static int goodix_select_gesture_mode(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	printk("[GESTURE]zhanghao enter the gt9xx gesture select!!!!\n");
	if ((type == EV_SYN) && (code == SYN_CONFIG)) {
		if (value == 5) {
			printk("[GESTURE]enable gesture");
			gesture_enable_flag = 1;
		} else {
			printk("[GESTURE]disable gesture");
			gesture_enable_flag = 0;
		}
	}
	return 0;
}

static ssize_t gtp_gesture_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	ssize_t len;

	len = sprintf(buf, "gesture_enable_flag = %d\n", gesture_enable_flag);
	return len;
}

static ssize_t gtp_gesture_enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int flag = 0;

	if (!kstrtoint(buf, 10, &flag)) {
		gesture_enable_flag = flag;
	}

	return count;
}
static DEVICE_ATTR(gesture_enable, S_IRUSR|S_IRGRP|S_IWUSR|S_IWGRP, gtp_gesture_enable_show, gtp_gesture_enable_store);
#endif


unsigned char get_lockdown_info(struct i2c_client *client, char *pProjectCode)
{

		u8 reg_val[10] = {GTP_REG_LOCKDOWN >> 8, GTP_REG_LOCKDOWN & 0xff};
		int ret;
		ret = gtp_i2c_read(client, reg_val, 10);
		if (ret < 0) {
				GTP_ERROR("i2c read lockdown info failed!!!\n");
		}

		sprintf(pProjectCode, "%02x%02x%02x%02x%02x%02x%02x%02x", \
				reg_val[2], reg_val[3], reg_val[4], reg_val[5], reg_val[6], reg_val[7], reg_val[8], reg_val[9]);
		return 0;
}

static ssize_t goodix_lockdown_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
		return snprintf(buf, 127, "%s\n", gt9xx_tp_lockdown_info);
}

static ssize_t goodix_lockdown_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
		if (size > 49)
				return -EINVAL;
		strlcpy(gt9xx_tp_lockdown_info, buf, size);
		if (gt9xx_tp_lockdown_info[size-1] == '\n')
				gt9xx_tp_lockdown_info[size-1] = 0;
		return size;
}

static ssize_t goodix_fwversion_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
		int count = 0;
		count += sprintf(buf, "ID:gt%s FW:%08X\n", product_id, version_info);
		return count;
}

static ssize_t goodix_fwversion_store(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t size)
{
		return -EPERM;
}

static DEVICE_ATTR(tp_lock_down_info, S_IWUSR|S_IRUGO, goodix_lockdown_show, goodix_lockdown_store);
static DEVICE_ATTR(fw_version, S_IWUSR|S_IRUGO, goodix_fwversion_show, goodix_fwversion_store);

/* add your attr in here*/
static struct attribute *goodix_attributes[] =
{
	&dev_attr_tp_lock_down_info.attr,
	&dev_attr_fw_version.attr,
	NULL
};

static struct attribute_group goodix_attribute_group =
{
	.attrs = goodix_attributes
};

/************************************************************************
* Name: goodix_create_sysfs
* Brief:  create sysfs for debug
* Input: i2c info
* Output: no
* Return: success =0
***********************************************************************/
int goodix_create_sysfs(struct i2c_client *client)
{
	int err;
	err = sysfs_create_group(&client->dev.kobj, &goodix_attribute_group);
	if (0 != err) {
		printk("[EX]: sysfs_create_group() failed!!");
		sysfs_remove_group(&client->dev.kobj, &goodix_attribute_group);
		return -EIO;
	} else {
		printk("[EX]: sysfs_create_group() succeeded!!");
	}
	return err;
}

/************************************************************************
* Name: goodix_remove_sysfs
* Brief:  remove sys
* Input: i2c info
* Output: no
* Return: no
***********************************************************************/

int goodix_remove_sysfs(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &goodix_attribute_group);
	return 0;
}




static int gt9xx_ts_pinctrl_init(struct goodix_ts_data *gt9xx_data, struct i2c_client *client)
{
	int retval;

	gt9xx_data->ts_pinctrl = devm_pinctrl_get(&(client->dev));
	if (IS_ERR_OR_NULL(gt9xx_data->ts_pinctrl)) {
		retval = PTR_ERR(gt9xx_data->ts_pinctrl);
		dev_dbg(&client->dev,
			"Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	gt9xx_data->eint_as_int
		= pinctrl_lookup_state(gt9xx_data->ts_pinctrl,
				"eint_as_int");
	if (IS_ERR_OR_NULL(gt9xx_data->eint_as_int)) {
		retval = PTR_ERR(gt9xx_data->eint_as_int);
		dev_err(&client->dev,
			"Can not lookup %s pinstate %d\n",
			"eint_as_int", retval);
		goto err_pinctrl_lookup;
	}
	gt9xx_data->eint_output_low
		= pinctrl_lookup_state(gt9xx_data->ts_pinctrl,
			"eint_output_low");
	if (IS_ERR_OR_NULL(gt9xx_data->eint_output_low)) {
		retval = PTR_ERR(gt9xx_data->eint_output_low);
		dev_err(&client->dev,
			"Can not lookup %s pinstate %d\n",
			"eint_output_low", retval);
		goto err_pinctrl_lookup;
	}
	gt9xx_data->eint_output_high
		= pinctrl_lookup_state(gt9xx_data->ts_pinctrl,
			"eint_output_high");
	if (IS_ERR_OR_NULL(gt9xx_data->eint_output_high)) {
		retval = PTR_ERR(gt9xx_data->eint_output_high);
		dev_dbg(&client->dev,
			"Can not lookup %s pinstate %d\n",
			"eint_output_high", retval);
	}
	gt9xx_data->erst_as_default
			= pinctrl_lookup_state(gt9xx_data->ts_pinctrl,
					"erst_as_default");
		if (IS_ERR_OR_NULL(gt9xx_data->erst_as_default)) {
			retval = PTR_ERR(gt9xx_data->erst_as_default);
			dev_err(&client->dev,
				"Can not lookup %s pinstate %d\n",
				"erst_as_default", retval);
			goto err_pinctrl_lookup;
		}
		gt9xx_data->erst_output_low
			= pinctrl_lookup_state(gt9xx_data->ts_pinctrl,
				"erst_output_low");
		if (IS_ERR_OR_NULL(gt9xx_data->erst_output_low)) {
			retval = PTR_ERR(gt9xx_data->erst_output_low);
			dev_err(&client->dev,
				"Can not lookup %s pinstate %d\n",
				"erst_output_low", retval);
			goto err_pinctrl_lookup;
		}
		gt9xx_data->erst_output_high
			= pinctrl_lookup_state(gt9xx_data->ts_pinctrl,
				"erst_output_high");
		if (IS_ERR_OR_NULL(gt9xx_data->erst_output_high)) {
			retval = PTR_ERR(gt9xx_data->erst_output_high);
			dev_dbg(&client->dev,
				"Can not lookup %s pinstate %d\n",
				"erst_output_high", retval);
		}
	return 0;
err_pinctrl_lookup:
	devm_pinctrl_put(gt9xx_data->ts_pinctrl);
err_pinctrl_get:
	gt9xx_data->ts_pinctrl = NULL;
	return retval;
}


/*******************************************************
Function:
	I2c probe.
Input:
	client: i2c device struct.
	id: device id.
Output:
	Executive outcomes.
		0: succeed.
*******************************************************/
static int goodix_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	s32 ret = -1;
	struct goodix_ts_data *ts;

	GTP_DEBUG_FUNC();


	if (1 == lct_tp_register_flag) {
		lct_tp_register_flag = 0;
		GTP_ERROR("the right tp has been registered. exit probe");
		return -3;
	}



	GTP_INFO("GTP Driver Version: %s, I2C Address: 0x%02x", GTP_DRIVER_VERSION, client->addr);
	i2c_connect_client = client;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		GTP_ERROR("I2C check functionality failed.");
		return -ENODEV;
	}
	ts = kzalloc(sizeof(*ts), GFP_KERNEL);
	if (ts == NULL) {
		GTP_ERROR("Alloc GFP_KERNEL memory failed.");
		return -ENOMEM;
	}

#ifdef GTP_CONFIG_OF	/* device tree support */
	if (client->dev.of_node) {
		gtp_parse_dt(&client->dev);
	}
	i2c_set_clientdata(client, ts);
	ts->gtp_rawdiff_mode = 0;

	ret = gt9xx_ts_pinctrl_init(ts, client);
	if (!ret && ts->ts_pinctrl) {
		ret = pinctrl_select_state(ts->ts_pinctrl, ts->eint_output_low);
		if (ret < 0) {
			dev_err(&client->dev, "failed to select pin to eint_output_low");
		}
		ret = pinctrl_select_state(ts->ts_pinctrl, ts->erst_output_low);
		if (ret < 0) {
			dev_err(&client->dev, "failed to select pin to erst_as_default");
		}
		msleep(10);
	}
	ret = gtp_power_switch(client, 1);
	if (ret) {
		GTP_ERROR("GTP power on failed.");
		return -EINVAL;
	}
	msleep(50);

#else			/* use gpio defined in gt9xx.h */
	gtp_rst_gpio = GTP_RST_PORT;
	gtp_int_gpio = GTP_INT_PORT;
#endif
	INIT_WORK(&ts->work, goodix_ts_work_func);
	ts->client = client;
	spin_lock_init(&ts->irq_lock);

#if GTP_ESD_PROTECT
	ts->clk_tick_cnt = 2 * HZ;
	GTP_DEBUG("Clock ticks for an esd cycle: %d", ts->clk_tick_cnt);
	spin_lock_init(&ts->esd_lock);

#endif

	ret = gtp_request_io_port(ts);
	if (ret < 0) {
		GTP_ERROR("GTP request IO port failed.");
		kfree(ts);
		return ret;
	}

#if GTP_COMPATIBLE_MODE
	gtp_get_chip_type(ts);
	if (CHIP_TYPE_GT9F == ts->chip_type) {
		ret = gtp_gt9xxf_init(ts->client);
		if (FAIL == ret) {
			GTP_INFO("Failed to init GT9XXF.");
		}
	}
#endif

	ret = gtp_i2c_test(client);
	if (ret < 0) {
		GTP_ERROR("I2C communication ERROR!");
		GTP_GPIO_FREE(gtp_rst_gpio);
		GTP_GPIO_FREE(gtp_int_gpio);
		pinctrl_select_state(ts->ts_pinctrl, ts->eint_as_int);

		gtp_power_switch(client, 0);
		return ret;
	}

	ret = gtp_read_version(client, &version_info);
	if (ret < 0) {
		GTP_ERROR("Read version failed.");
	}

	get_lockdown_info(client, gt9xx_tp_lockdown_info);

	ret = gtp_init_panel(ts);
	if (ret < 0) {
		GTP_ERROR("GTP init panel failed.");
		ts->abs_x_max = GTP_MAX_WIDTH;
		ts->abs_y_max = GTP_MAX_HEIGHT;
		ts->int_trigger_type = GTP_INT_TRIGGER;
	}


	gt91xx_config_proc = proc_create(GT91XX_CONFIG_PROC_FILE, 0666, NULL, &config_proc_ops);
	if (gt91xx_config_proc == NULL) {
		GTP_ERROR("create_proc_entry %s failed\n", GT91XX_CONFIG_PROC_FILE);
	} else {
		GTP_INFO("create proc entry %s success", GT91XX_CONFIG_PROC_FILE);
	}

 #if GTP_GESTURE_WAKEUP
	goodix_gesture_kobj = kobject_create_and_add("goodix_gesture", NULL) ;
	if (goodix_gesture_kobj == NULL) {
		GTP_ERROR("%s: subsystem_register failed\n", __func__);
	}

	ret = sysfs_create_file(goodix_gesture_kobj, &dev_attr_gesture_enable.attr);
	if (ret) {
		GTP_ERROR("%s: sysfs_create_version_file failed\n", __func__);
	}
#endif

#if GTP_ESD_PROTECT
	gtp_esd_switch(client, SWITCH_ON);
#endif

#if GTP_AUTO_UPDATE
	ret = gup_init_update_proc(ts);
	if (ret < 0) {
		GTP_ERROR("Create update thread error.");
	}
#endif

	ret = gtp_request_input_dev(ts);
	if (ret < 0) {
		GTP_ERROR("GTP request input dev failed");
	}

	ret = gtp_request_irq(ts);
	if (ret < 0) {
		GTP_INFO("GTP works in polling mode.");
	} else {
		GTP_INFO("GTP works in interrupt mode.");
	}

	if (ts->use_irq) {
		gtp_irq_enable(ts);
#if GTP_GESTURE_WAKEUP
	enable_irq_wake(client->irq);
#endif
	}

#if RESUME_IN_WORKQUEUE
	INIT_WORK(&ts->fb_notify_work, gtp_fb_notifier_resume_work);
#endif
	/* register suspend and resume fucntion*/
	gtp_register_powermanger(ts);

#if GTP_CREATE_WR_NODE
	init_wr_node(client);
#endif


#ifdef SUPPORT_READ_TP_VERSION
	memset(fw_version, 0, sizeof(fw_version));
	sprintf(fw_version, "[FW]0x%02x,[IC]GT%s", version_info, product_id);
	init_tp_fm_info(0, fw_version, "BIEL");
#endif



	goodix_create_sysfs(client);


	if (!ret && ts->ts_pinctrl) {
		ret = pinctrl_select_state(ts->ts_pinctrl, ts->eint_output_low);
		if (ret < 0) {
			dev_err(&ts->client->dev, "failed to select pin to eint_output_low");
		}
	}

	lct_tp_register_flag = 1;
	return 0;
}


/*******************************************************
Function:
	Goodix touchscreen driver release function.
Input:
	client: i2c device struct.
Output:
	Executive outcomes. 0---succeed.
*******************************************************/
static int goodix_ts_remove(struct i2c_client *client)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	GTP_DEBUG_FUNC();

	gtp_unregister_powermanger(ts);

#if GTP_CREATE_WR_NODE
	uninit_wr_node();
#endif

#if GTP_ESD_PROTECT
	destroy_workqueue(gtp_esd_check_workqueue);
#endif

	if (ts) {
		if (ts->use_irq) {

			pinctrl_select_state(ts->ts_pinctrl, ts->eint_as_int);
			GTP_GPIO_FREE(gtp_int_gpio);
			GTP_GPIO_FREE(gtp_rst_gpio);

			free_irq(client->irq, ts);
		} else {
			hrtimer_cancel(&ts->timer);
		}
	}


	goodix_remove_sysfs(client);


	GTP_INFO("GTP driver removing...");
	i2c_set_clientdata(client, NULL);
	devm_kfree(&(client->dev), pindata);
	input_unregister_device(ts->input_dev);
	kfree(ts);

	lct_tp_register_flag = 0;

	return 0;
}


/*******************************************************
Function:
	Early suspend function.
Input:
	h: early_suspend struct.
Output:
	None.
*******************************************************/
static void goodix_ts_suspend(struct goodix_ts_data *ts)
{
	s8 ret = -1;
	GTP_DEBUG_FUNC();
	if (ts->enter_update) {
		return;
	}
	GTP_INFO("System suspend.");

	ts->gtp_is_suspend = 1;
#if GTP_ESD_PROTECT
	gtp_esd_switch(ts->client, SWITCH_OFF);
#endif

#if GTP_GESTURE_WAKEUP


	if (gesture_enable_flag) {
		ret = gtp_enter_doze(ts);
		gesture_suspend_resume_flag = 1;
	} else {
		if (ts->use_irq) {
			gtp_irq_disable(ts);
		} else {
			hrtimer_cancel(&ts->timer);
		}

		ret = gtp_enter_sleep(ts);
		if (ret < 0) {
			GTP_ERROR("GTP early suspend failed.");
		}
		gesture_suspend_resume_flag = 0;
	}

#else
	if (ts->use_irq) {
		gtp_irq_disable(ts);
	} else {
		hrtimer_cancel(&ts->timer);
	}
	ret = gtp_enter_sleep(ts);
#endif
	if (ret < 0) {
		GTP_ERROR("GTP early suspend failed.");
	}


	msleep(58);
}

/*******************************************************
Function:
	Late resume function.
Input:
	h: early_suspend struct.
Output:
	None.
*******************************************************/
static void goodix_ts_resume(struct goodix_ts_data *ts)
{
	s8 ret = -1;
	s8 finger_count = 0;
	GTP_DEBUG_FUNC();
	if (ts->enter_update) {
		return;
	}
	GTP_INFO("System resume.");



	for (finger_count = 0; finger_count < 10; finger_count++) {
		gtp_touch_up(ts, finger_count);

	}
	input_sync(ts->input_dev);

#if GTP_GESTURE_WAKEUP
	doze_status = DOZE_DISABLED;
	if (gesture_suspend_resume_flag) {
		gtp_reset_guitar(ts->client, 20);
	} else {
		ret = gtp_wakeup_sleep(ts);
		if (ret < 0) {
			GTP_ERROR("GTP later resume failed.");
		}
#if (GTP_COMPATIBLE_MODE)
		if (CHIP_TYPE_GT9F == ts->chip_type) {

		} else
#endif
		{
			gtp_send_cfg(ts->client);
		}
		/* mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
		if (ts->use_irq) {
			gtp_irq_enable(ts);
		} else {
			hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		}
	}
#else
	ret = gtp_wakeup_sleep(ts);
	if (ret < 0) {
		GTP_ERROR("GTP later resume failed.");
	}
#if (GTP_COMPATIBLE_MODE)
	if (CHIP_TYPE_GT9F == ts->chip_type) {

	} else
#endif
	{
		gtp_send_cfg(ts->client);
	}
	/* mt_eint_unmask(CUST_EINT_TOUCH_PANEL_NUM); */
	if (ts->use_irq) {
		gtp_irq_enable(ts);
	} else {
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}
#endif



	if (ts->use_irq) {
		gtp_irq_enable(ts);
	} else {
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}

	ts->gtp_is_suspend = 0;
#if GTP_ESD_PROTECT
	gtp_esd_switch(ts->client, SWITCH_ON);
#endif
}


#if   defined(CONFIG_FB)
#if RESUME_IN_WORKQUEUE
static void gtp_fb_notifier_resume_work(struct work_struct *work)
{
	struct goodix_ts_data *ts = container_of(work, struct goodix_ts_data, fb_notify_work);
	goodix_ts_resume(ts);
}
#endif
/* frame buffer notifier block control the suspend/resume procedure */
static int gtp_fb_notifier_callback(struct notifier_block *noti, unsigned long event, void *data)
{
	struct fb_event *ev_data = data;
	struct goodix_ts_data *ts = container_of(noti, struct goodix_ts_data, notifier);
	int *blank;

	#if RESUME_IN_WORKQUEUE
		if (ev_data && ev_data->data && ts) {
			blank = ev_data->data;
			if (event == FB_EARLY_EVENT_BLANK && *blank == FB_BLANK_UNBLANK) {
				GTP_DEBUG("Resume by fb notifier in workqueue.");
				schedule_work(&ts->fb_notify_work);
			} else if (event == FB_EVENT_BLANK && *blank == FB_BLANK_POWERDOWN) {
				flush_work(&ts->fb_notify_work);
				GTP_DEBUG("Flush workqueue & Suspend by fb notifier.");
				goodix_ts_suspend(ts);
			}
		}
	#else
		if (ev_data && ev_data->data && event == FB_EVENT_BLANK && ts) {
			blank = ev_data->data;
			if (*blank == FB_BLANK_UNBLANK) {
				GTP_DEBUG("Resume by fb notifier.");
				goodix_ts_resume(ts);

			} else if (*blank == FB_BLANK_POWERDOWN) {
				GTP_DEBUG("Suspend by fb notifier.");
				goodix_ts_suspend(ts);
			}
		}
	#endif

	return 0;
}
#elif defined(CONFIG_PM)
/* bus control the suspend/resume procedure */
static int gtp_pm_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts) {
		GTP_DEBUG("Suspend by i2c pm.");
		goodix_ts_suspend(ts);
	}

	return 0;
}
static int gtp_pm_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts) {
		GTP_DEBUG("Resume by i2c pm.");
		goodix_ts_resume(ts);
	}

	return 0;
}

static struct dev_pm_ops gtp_pm_ops = {
	.suspend = gtp_pm_suspend,
	.resume  = gtp_pm_resume,
};

#elif defined(CONFIG_HAS_EARLYSUSPEND)
/* earlysuspend module the suspend/resume procedure */
static void gtp_early_suspend(struct early_suspend *h)
{
	struct goodix_ts_data *ts = container_of(h, struct goodix_ts_data, early_suspend);

	if (ts) {
		GTP_DEBUG("Suspend by earlysuspend module.");
		goodix_ts_suspend(ts);
	}
}
static void gtp_early_resume(struct early_suspend *h)
{
	struct goodix_ts_data *ts = container_of(h, struct goodix_ts_data, early_suspend);

	if (ts) {
		GTP_DEBUG("Resume by earlysuspend module.");
		goodix_ts_resume(ts);
	}
}
#endif

static int gtp_register_powermanger(struct goodix_ts_data *ts)
{
#if   defined(CONFIG_FB)
	ts->notifier.notifier_call = gtp_fb_notifier_callback;
	fb_register_client(&ts->notifier);

#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = goodix_ts_early_suspend;
	ts->early_suspend.resume = goodix_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

	return 0;
}

static int gtp_unregister_powermanger(struct goodix_ts_data *ts)
{
#if   defined(CONFIG_FB)
		fb_unregister_client(&ts->notifier);

#elif defined(CONFIG_HAS_EARLYSUSPEND)
		unregister_early_suspend(&ts->early_suspend);
#endif
	return 0;
}

/* end */

#if GTP_ESD_PROTECT
s32 gtp_i2c_read_no_rst(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msgs[2];
	s32 ret = -1;
	s32 retries = 0;

	GTP_DEBUG_FUNC();

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = client->addr;
	msgs[0].len   = GTP_ADDR_LENGTH;
	msgs[0].buf   = &buf[0];


	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = client->addr;
	msgs[1].len   = len - GTP_ADDR_LENGTH;
	msgs[1].buf   = &buf[GTP_ADDR_LENGTH];


	while (retries < 5) {
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret == 2)
			break;
		retries++;
	}
	if ((retries >= 5)) {
		GTP_ERROR("I2C Read: 0x%04X, %d bytes failed, errcode: %d!", (((u16)(buf[0] << 8)) | buf[1]), len-2, ret);
	}
	return ret;
}

s32 gtp_i2c_write_no_rst(struct i2c_client *client, u8 *buf, s32 len)
{
	struct i2c_msg msg;
	s32 ret = -1;
	s32 retries = 0;

	GTP_DEBUG_FUNC();

	msg.flags = !I2C_M_RD;
	msg.addr  = client->addr;
	msg.len   = len;
	msg.buf   = buf;


	while (retries < 5) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret == 1)break;
		retries++;
	}
	if ((retries >= 5)) {
		GTP_ERROR("I2C Write: 0x%04X, %d bytes failed, errcode: %d!", (((u16)(buf[0] << 8)) | buf[1]), len-2, ret);
	}
	return ret;
}
/*******************************************************
Function:
	switch on & off esd delayed work
Input:
	client:  i2c device
	on:      SWITCH_ON / SWITCH_OFF
Output:
	void
*********************************************************/
void gtp_esd_switch(struct i2c_client *client, s32 on)
{
	struct goodix_ts_data *ts;

	ts = i2c_get_clientdata(client);
	spin_lock(&ts->esd_lock);

	if (SWITCH_ON == on) {
		if (!ts->esd_running) {
			ts->esd_running = 1;
			spin_unlock(&ts->esd_lock);
			GTP_INFO("Esd started");
			queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, ts->clk_tick_cnt);
		} else {
			spin_unlock(&ts->esd_lock);
		}
	} else {
		if (ts->esd_running) {
			ts->esd_running = 0;
			spin_unlock(&ts->esd_lock);
			GTP_INFO("Esd cancelled");
			cancel_delayed_work_sync(&gtp_esd_check_work);
		} else {
			spin_unlock(&ts->esd_lock);
		}
	}
}

/*******************************************************
Function:
	Initialize external watchdog for esd protect
Input:
	client:  i2c device.
Output:
	result of i2c write operation.
		1: succeed, otherwise: failed
*********************************************************/
static s32 gtp_init_ext_watchdog(struct i2c_client *client)
{
	u8 opr_buffer[3] = {0x80, 0x41, 0xAA};
	GTP_DEBUG("[Esd]Init external watchdog");
	return gtp_i2c_write_no_rst(client, opr_buffer, 3);
}

/*******************************************************
Function:
	Esd protect function.
	External watchdog added by meta, 2013/03/07
Input:
	work: delayed work
Output:
	None.
*******************************************************/
static void gtp_esd_check_func(struct work_struct *work)
{
	s32 i;
	s32 ret = -1;
	struct goodix_ts_data *ts = NULL;
	u8 esd_buf[5] = {0x80, 0x40};

	GTP_DEBUG_FUNC();

	ts = i2c_get_clientdata(i2c_connect_client);

	if (ts->gtp_is_suspend || ts->enter_update) {
		GTP_INFO("Esd suspended!");
		return;
	}

	for (i = 0; i < 3; i++) {
		ret = gtp_i2c_read_no_rst(ts->client, esd_buf, 4);

		GTP_DEBUG("[Esd]0x8040 = 0x%02X, 0x8041 = 0x%02X", esd_buf[2], esd_buf[3]);
		if ((ret < 0)) {

			continue;
		} else {
			if ((esd_buf[2] == 0xAA) || (esd_buf[3] != 0xAA)) {

				u8 chk_buf[4] = {0x80, 0x40};

				gtp_i2c_read_no_rst(ts->client, chk_buf, 4);
				GTP_DEBUG("[Check]0x8040 = 0x%02X, 0x8041 = 0x%02X", chk_buf[2], chk_buf[3]);
				if ((chk_buf[2] == 0xAA) || (chk_buf[3] != 0xAA)) {
					i = 3;
					break;
				} else {
					continue;
				}
			} else {

				esd_buf[2] = 0xAA;
				gtp_i2c_write_no_rst(ts->client, esd_buf, 3);
				break;
			}
		}
	}
	if (i >= 3) {
	#if GTP_COMPATIBLE_MODE
		if (CHIP_TYPE_GT9F == ts->chip_type) {
			if (ts->rqst_processing) {
				GTP_INFO("Request processing, no esd recovery");
			} else {
				GTP_ERROR("IC working abnormally! Process esd recovery.");
				esd_buf[0] = 0x42;
				esd_buf[1] = 0x26;
				esd_buf[2] = 0x01;
				esd_buf[3] = 0x01;
				esd_buf[4] = 0x01;
				gtp_i2c_write_no_rst(ts->client, esd_buf, 5);
				msleep(50);
		#ifdef GTP_CONFIG_OF
				gtp_power_switch(ts->client, 0);
				msleep(20);
				gtp_power_switch(ts->client, 1);
				msleep(20);
		#endif
				gtp_esd_recovery(ts->client);
			}
		} else
	#endif
		{
			GTP_ERROR("IC working abnormally! Process reset guitar.");
			esd_buf[0] = 0x42;
			esd_buf[1] = 0x26;
			esd_buf[2] = 0x01;
			esd_buf[3] = 0x01;
			esd_buf[4] = 0x01;
			gtp_i2c_write_no_rst(ts->client, esd_buf, 5);
			msleep(50);
		#ifdef GTP_CONFIG_OF
			GTP_GPIO_FREE(gtp_rst_gpio);
			GTP_GPIO_FREE(gtp_int_gpio);
			pinctrl_select_state(ts->ts_pinctrl, ts->eint_as_int);

			gtp_power_switch(ts->client, 0);
			ret = gt9xx_ts_pinctrl_init(ts, ts->client);
			if (!ret && ts->ts_pinctrl) {
				ret = pinctrl_select_state(ts->ts_pinctrl, ts->eint_output_low);
				if (ret < 0) {
					dev_err(&ts->client->dev, "failed to select pin to eint_output_low");
				}
				/*ret = pinctrl_select_state(ts->ts_pinctrl,ts->erst_as_default);
				if (ret < 0) {
					dev_err(&ts->client->dev,"failed to select pin to erst_as_default");
				}*/
			}
			msleep(20);
			gtp_power_switch(ts->client, 1);
			msleep(20);
		#endif
			gtp_reset_guitar(ts->client, 50);
			msleep(50);
			gtp_send_cfg(ts->client);
		}
	}

	if (!ts->gtp_is_suspend) {
		queue_delayed_work(gtp_esd_check_workqueue, &gtp_esd_check_work, ts->clk_tick_cnt);
	} else {
		GTP_INFO("Esd suspended!");
	}
	return;
}
#endif

#ifdef GTP_CONFIG_OF
static const struct of_device_id goodix_match_table[] = {
		{.compatible = "goodix,gt9xx",},
		{ },
};
#endif

static const struct i2c_device_id goodix_ts_id[] = {
	{ GTP_I2C_NAME, 0 },
	{ }
};

static struct i2c_driver goodix_ts_driver = {
	.probe      = goodix_ts_probe,
	.remove     = goodix_ts_remove,
	.id_table   = goodix_ts_id,
	.driver = {
		.name     = GTP_I2C_NAME,
		.owner    = THIS_MODULE,
#ifdef GTP_CONFIG_OF
		.of_match_table = goodix_match_table,
#endif
#if !defined(CONFIG_FB) && defined(CONFIG_PM)
		.pm		  = &gtp_pm_ops,
#endif
	},
};

/*******************************************************
Function:
	Driver Install function.
Input:
	None.
Output:
	Executive Outcomes. 0---succeed.
********************************************************/
static int __init goodix_ts_init(void)
{
	s32 ret;
	extern int tpselect;
	GTP_DEBUG_FUNC();
	GTP_INFO("GTP driver installing....");
	goodix_wq = create_singlethread_workqueue("goodix_wq");
	if (!goodix_wq) {
		GTP_ERROR("Creat workqueue failed.");
		return -ENOMEM;
	}
#if GTP_ESD_PROTECT
	INIT_DELAYED_WORK(&gtp_esd_check_work, gtp_esd_check_func);
	gtp_esd_check_workqueue = create_workqueue("gtp_esd_check");
#endif

	if (tpselect == 1) {
		lct_tp_register_flag = 1;
	} else {
		lct_tp_register_flag = 0;
	}
	ret = i2c_add_driver(&goodix_ts_driver);
	return ret;
}

/*******************************************************
Function:
	Driver uninstall function.
Input:
	None.
Output:
	Executive Outcomes. 0---succeed.
********************************************************/
static void __exit goodix_ts_exit(void)
{
	GTP_DEBUG_FUNC();
	GTP_INFO("GTP driver exited.");
	i2c_del_driver(&goodix_ts_driver);
	if (goodix_wq) {
		destroy_workqueue(goodix_wq);
	}
}

module_init(goodix_ts_init);
module_exit(goodix_ts_exit);

MODULE_DESCRIPTION("GTP Series Driver");
MODULE_LICENSE("GPL");
