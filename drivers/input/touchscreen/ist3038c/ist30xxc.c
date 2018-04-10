/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2018 XiaoMi, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <asm/unaligned.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/err.h>

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/input/mt.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>
#include <linux/proc_fs.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#endif

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#include <linux/hqsysfs.h>


#include "ist30xxc.h"
#include "ist30xxc_update.h"

#ifdef IST30XX_TRACKING_MODE
#include "ist30xxc_tracking.h"
#endif

#ifdef IST30XX_DEBUG
#include "ist30xxc_misc.h"
#endif

#ifdef IST30XX_CMCS_TEST
#include "ist30xxc_cmcs.h"
#endif

#ifdef IST30XX_USE_KEY
int ist30xx_key_code[] = IST30XX_KEY_CODES;
#ifdef IST30XX_USE_KEY_COORD
int ist30xx_key_x[] = KEY_COORD_X;
#endif
#endif

struct ist30xx_data *ts_data;
extern int set_usb_charge_mode_par;
int ist30xx_log_level = IST30XX_LOG_LEVEL;

void tsp_printk(int level, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	if (ist30xx_log_level < level)
		return;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	printk("%s %pV", IST30XX_LOG_TAG, &vaf);

	va_end(args);
}

long get_milli_second(struct ist30xx_data *data)
{
	ktime_get_ts(&data->t_current);

	return data->t_current.tv_sec * 1000 + data->t_current.tv_nsec / 1000000;
}

void ist30xx_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}

int ist30xx_intr_wait(struct ist30xx_data *data, long ms)
{
	long start_ms = get_milli_second(data);
	long curr_ms = 0;

while (1) {
		if (!data->irq_working)
			break;

		curr_ms = get_milli_second(data);
		if ((curr_ms < 0) || (start_ms < 0) || (curr_ms - start_ms > ms)) {
			tsp_info("%s() timeout(%dms)\n", __func__, ms);
			return -EPERM;
		}

		ist30xx_delay(2);
	}
	return 0;
}

void ist30xx_disable_irq(struct ist30xx_data *data)
{
	if (likely(data->irq_enabled)) {
#ifdef IST30XX_TRACKING_MODE
		ist30xx_tracking(TRACK_INTR_DISABLE);
#endif
		disable_irq(data->client->irq);
		data->irq_enabled = false;
		data->status.event_mode = false;
	}
}

void ist30xx_enable_irq(struct ist30xx_data *data)
{
	if (likely(!data->irq_enabled)) {
#ifdef IST30XX_TRACKING_MODE
		ist30xx_tracking(TRACK_INTR_ENABLE);
#endif
		enable_irq(data->client->irq);
		ist30xx_delay(10);
		data->irq_enabled = true;
		data->status.event_mode = true;
	}
}

void ist30xx_scheduled_reset(struct ist30xx_data *data)
{
	if (likely(data->initialized))
		schedule_delayed_work(&data->work_reset_check, 0);
}

static void ist30xx_request_reset(struct ist30xx_data *data)
{
	data->irq_err_cnt++;
	if (unlikely(data->irq_err_cnt >= data->max_irq_err_cnt)) {
		tsp_info("%s()\n", __func__);
		ist30xx_scheduled_reset(data);
		data->irq_err_cnt = 0;
	}
}

#define NOISE_MODE_TA       (0)
#define NOISE_MODE_CALL     (1)
#define NOISE_MODE_COVER    (2)
#define NOISE_MODE_EDGE     (4)
#define NOISE_MODE_POWER    (8)
void ist30xx_start(struct ist30xx_data *data)
{
	if (data->initialized) {
		data->scan_count = 0;
		data->scan_retry = 0;
		mod_timer(&data->event_timer,
				get_jiffies_64() + EVENT_TIMER_INTERVAL * 2);
	}

	data->ignore_delay = true;

	ist30xx_write_cmd(data, IST30XX_HIB_CMD,
			((eHCOM_SET_MODE_SPECIAL << 16) | (data->noise_mode & 0xFFFF)));

	ist30xx_write_cmd(data, IST30XX_HIB_CMD,
			((eHCOM_SET_LOCAL_MODEL << 16) | (TSP_LOCAL_CODE & 0xFFFF)));

	if (data->report_rate >= 0) {
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				((eHCOM_SET_TIME_ACTIVE << 16) | (data->report_rate & 0xFFFF)));
		tsp_info("%s: active rate : %dus\n", __func__, data->report_rate);
	}

	if (data->idle_rate >= 0) {
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				((eHCOM_SET_TIME_IDLE << 16) | (data->idle_rate & 0xFFFF)));
		tsp_info("%s: idle rate : %dus\n", __func__, data->idle_rate);
	}

	if (data->rec_mode) {
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				(eHCOM_SET_REC_MODE << 16) | (IST30XX_ENABLE & 0xFFFF));
		tsp_info("%s: rec mode start\n", __func__);
	}

	if (data->debugging_mode) {
		data->debugging_scancnt = 0;
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				(eHCOM_SET_DBG_MODE << 16) | (IST30XX_ENABLE & 0xFFFF));
		tsp_info("%s: debugging mode start\n", __func__);
	}

	if (data->debug_mode || data->rec_mode || data->debugging_mode) {
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				(eHCOM_SLEEP_MODE_EN << 16) | (IST30XX_DISABLE & 0xFFFF));
		tsp_info("%s: nosleep mode start\n", __func__);
	}

	ist30xx_cmd_start_scan(data);

	data->ignore_delay = false;

	if (data->rec_mode) {
		ist30xx_delay(100);
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				(eHCOM_SET_REC_MODE << 16) | (IST30XX_START_SCAN & 0xFFFF));
	}

	tsp_info("%s, local : %d, mode : 0x%x\n", __func__, TSP_LOCAL_CODE & 0xFFFF,
			data->noise_mode & 0xFFFF);
}

int ist30xx_get_ver_info(struct ist30xx_data *data)
{
	int ret;

	data->fw.prev.main_ver = data->fw.cur.main_ver;
	data->fw.prev.fw_ver = data->fw.cur.fw_ver;
	data->fw.prev.core_ver = data->fw.cur.core_ver;
	data->fw.prev.test_ver = data->fw.cur.test_ver;
#ifdef XIAOMI_PRODUCT
	data->fw.prev.lockdown[0] = data->fw.cur.lockdown[0];
	data->fw.prev.lockdown[1] = data->fw.cur.lockdown[1];
	data->fw.prev.config[0] = data->fw.cur.config[0];
	data->fw.prev.config[1] = data->fw.cur.config[1];
#endif
	data->fw.cur.main_ver = 0;
	data->fw.cur.fw_ver = 0;
	data->fw.cur.core_ver = 0;
	data->fw.cur.test_ver = 0;
#ifdef XIAOMI_PRODUCT
	data->fw.cur.lockdown[0] = 0;
	data->fw.cur.lockdown[1] = 0;
	data->fw.cur.config[0] = 0;
	data->fw.cur.config[1] = 0;
#endif

	ret = ist30xx_cmd_hold(data, IST30XX_ENABLE);
	if (unlikely(ret))
		return ret;

	ret = ist30xx_read_reg(data->client, IST30XX_DA_ADDR(eHCOM_GET_VER_MAIN),
			&data->fw.cur.main_ver);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client, IST30XX_DA_ADDR(eHCOM_GET_VER_FW),
			&data->fw.cur.fw_ver);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client, IST30XX_DA_ADDR(eHCOM_GET_VER_CORE),
			&data->fw.cur.core_ver);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client, IST30XX_DA_ADDR(eHCOM_GET_VER_TEST),
			&data->fw.cur.test_ver);
	if (unlikely(ret))
		goto err_get_ver;

#ifdef XIAOMI_PRODUCT
	ret = ist30xx_read_reg(data->client, IST30XX_DA_ADDR(eHCOM_GET_LOCKDOWN_1),
			&data->fw.cur.lockdown[0]);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client, IST30XX_DA_ADDR(eHCOM_GET_LOCKDOWN_2),
			&data->fw.cur.lockdown[1]);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client, IST30XX_DA_ADDR(eHCOM_GET_CONFIG_1),
			&data->fw.cur.config[0]);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client, IST30XX_DA_ADDR(eHCOM_GET_CONFIG_2),
			&data->fw.cur.config[1]);
	if (unlikely(ret))
		goto err_get_ver;
#endif

	ret = ist30xx_cmd_hold(data, IST30XX_DISABLE);
	if (unlikely(ret))
		goto err_get_ver;

	tsp_info("IC version main: %x, fw: %x, test: %x, core: %x\n",
			data->fw.cur.main_ver, data->fw.cur.fw_ver, data->fw.cur.test_ver,
			data->fw.cur.core_ver);
#ifdef XIAOMI_PRODUCT
	if (data->lockdown_upper != 0)
		data->fw.cur.lockdown[0] = data->lockdown_upper;

	tsp_info("lockdown: %08X%08X, config: %08X%08X\n",
			data->fw.cur.lockdown[0], data->fw.cur.lockdown[1],
			data->fw.cur.config[0], data->fw.cur.config[1]);
#endif

	return 0;

err_get_ver:
	ist30xx_reset(data, false);

	return ret;
}

#if defined(IST30XX_GESTURE)
#define WAKEUP_OFF 4
#define WAKEUP_ON 5
bool ist30xx_gesture_func_on = true;
static int ist30xx_gesture_switch(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	if (type == EV_SYN && code == SYN_CONFIG) {
		if (value == WAKEUP_OFF) {
			ist30xx_gesture_func_on = false;
		} else if (value == WAKEUP_ON) {
			ist30xx_gesture_func_on = true;
		}
	}
	return 0;
}
#endif

int ist30xx_set_input_device(struct ist30xx_data *data)
{
	int ret;
	data->input_dev->name = "ist30xx_ts_input";
	data->input_dev->id.bustype = BUS_I2C;
	data->input_dev->dev.parent = &data->client->dev;

	input_mt_init_slots(data->input_dev, IST30XX_MAX_FINGERS, 0);

	set_bit(EV_ABS, data->input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, data->input_dev->propbit);

	input_set_abs_params(data->input_dev, ABS_MT_POSITION_X, 0,
			data->tsp_info.width - 1, 0, 0);
	input_set_abs_params(data->input_dev, ABS_MT_POSITION_Y, 0,
			data->tsp_info.height - 1, 0, 0);
#ifdef IST30XX_USE_3D_TOUCH
	input_set_abs_params(data->input_dev, ABS_MT_PRESSURE, 0,
			IST30XX_MAX_3D_VALUE, 0, 0);
#else
	input_set_abs_params(data->input_dev, ABS_MT_TOUCH_MAJOR, 0,
			IST30XX_MAX_W, 0, 0);
#endif

#if (defined(IST30XX_USE_KEY) || defined(IST30XX_GESTURE) ||    \
		defined(IST30XX_SURFACE_TOUCH) || defined(IST30XX_BLADE_TOUCH))
	set_bit(EV_KEY, data->input_dev->evbit);
	set_bit(EV_SYN, data->input_dev->evbit);
#endif
#ifdef IST30XX_USE_KEY
	{
		int i;
		for (i = 0; i < ARRAY_SIZE(ist30xx_key_code); i++)
			set_bit(ist30xx_key_code[i], data->input_dev->keybit);
	}
#endif
#ifdef IST30XX_GESTURE
	input_set_capability(data->input_dev, EV_KEY, KEY_WAKEUP);
	tsp_info("regist gesture key!\n");
	data->input_dev->event = ist30xx_gesture_switch;
#endif
#ifdef IST30XX_SURFACE_TOUCH
	input_set_capability(data->input_dev, EV_KEY, KEY_MUTE);
#endif
#ifdef IST30XX_BLADE_TOUCH
	input_set_capability(data->input_dev, EV_KEY, KEY_SYSRQ);
#endif

	input_set_drvdata(data->input_dev, data);
	ret = input_register_device(data->input_dev);

	tsp_info("%s: input register device:%d\n", __func__, ret);

	return ret;
}

#define CALIB_MSG_MASK              (0xF0000FFF)
#define CALIB_MSG_VALID             (0x80000CAB)
#define CAL_REF_MSG_MASK            (0xFF0000FF)
#define CAL_REF_MSG_VALID           (0xAA000055)
#define TRACKING_INTR_VALID         (0x127EA597)
#define TRACKING_INTR_DEBUG1_VALID  (0x127A6E81)
#define TRACKING_INTR_DEBUG2_VALID  (0x127A6E82)
#define TRACKING_INTR_DEBUG3_VALID  (0x127A6E83)
int ist30xx_get_info(struct ist30xx_data *data)
{
	int ret;
	u32 calib_msg;
#ifdef IST30XX_TRACKING_MODE
	u32 tracking_value;
	u32 ms;
#endif

	mutex_lock(&data->lock);
	ist30xx_disable_irq(data);

#ifdef IST30XX_INTERNAL_BIN
	ist30xx_get_tsp_info(data);
#else
	ret = ist30xx_get_ver_info(data);
	if (unlikely(ret))
		goto err_get_info;

	ret = ist30xx_get_tsp_info(data);
	if (unlikely(ret))
		goto err_get_info;
#endif

	ret = ist30xx_set_input_device(data);
	if (unlikely(ret))
		goto err_get_info;

	ist30xx_print_info(data);

#ifndef IST30XX_UPDATE_NO_CAL
	if (data->fw_mode == IST30XX_CAL_MODE) {
		ret = ist30xx_read_cmd(data, eHCOM_GET_CAL_RESULT, &calib_msg);
		if (likely(ret == 0)) {
			tsp_info("calib status: 0x%08x\n", calib_msg);
#ifdef IST30XX_TRACKING_MODE
			ms = get_milli_second(data);
			tracking_value = TRACKING_INTR_VALID;
			ist30xx_put_track_ms(ms);
			ist30xx_put_track(&tracking_value, 1);
			ist30xx_put_track(&calib_msg, 1);
#endif
			if ((calib_msg & CALIB_MSG_MASK) != CALIB_MSG_VALID ||
					CALIB_TO_STATUS(calib_msg) > 0) {
				ist30xx_calibrate(data, IST30XX_MAX_RETRY_CNT);
			}
		}
	}
#endif

#ifdef IST30XX_CHECK_CALIB
	if (data->fw_mode == IST30XX_CAL_MODE) {
		if ((!data->status.update) && (!data->status.calib != 1)) {
			ret = ist30xx_cmd_check_calib(data);
			if (likely(!ret)) {
				data->status.calib = 2;
				data->status.calib_msg = 0;
				data->event_ms = (u32)get_milli_second(data);
				data->status.event_mode = true;
			}
		}
	}
#endif

	ist30xx_enable_irq(data);
	mutex_unlock(&data->lock);

	return 0;

err_get_info:
	mutex_unlock(&data->lock);

	return ret;
}

#if (defined(IST30XX_GESTURE) || defined(IST30XX_SURFACE_TOUCH) ||  \
		defined(IST30XX_BLADE_TOUCH))
#define SPECIAL_MAGIC_STRING        (0x4170CF00)
#define SPECIAL_MAGIC_MASK          (0xFFFFFF00)
#define SPECIAL_MESSAGE_MASK        (~SPECIAL_MAGIC_MASK)
#define PARSE_SPECIAL_MESSAGE(n)  \
		((n & SPECIAL_MAGIC_MASK) == SPECIAL_MAGIC_STRING ? \
		(n & SPECIAL_MESSAGE_MASK) : -EINVAL)
#define SPECIAL_START_MASK          (0x80)
#define SPECIAL_SUCCESS_MASK        (0x0F)
#define MAX_LK_KEYCODE_NUM          (256)
static unsigned short lk_keycode[MAX_LK_KEYCODE_NUM] = {
	[0x01] = KEY_WAKEUP,
	[0x02] = KEY_PLAYPAUSE,
	[0x03] = KEY_NEXTSONG,
	[0x04] = KEY_PREVIOUSSONG,
	[0x05] = KEY_VOLUMEUP,
	[0x06] = KEY_VOLUMEDOWN,
	[0x07] = KEY_MUTE,
	[0x21] = KEY_MUTE,
	[0x11] = KEY_SYSRQ,
	[0x12] = KEY_SYSRQ,
	[0x13] = KEY_SYSRQ,
	[0x14] = KEY_SYSRQ,
	[0x41] = KEY_A,
	[0x42] = KEY_B,
	[0x43] = KEY_C,
	[0x44] = KEY_D,
	[0x45] = KEY_E,
	[0x46] = KEY_F,
	[0x47] = KEY_G,
	[0x48] = KEY_H,
	[0x49] = KEY_I,
	[0x4A] = KEY_J,
	[0x4B] = KEY_K,
	[0x4C] = KEY_L,
	[0x4D] = KEY_M,
	[0x4E] = KEY_N,
	[0x4F] = KEY_O,
	[0x50] = KEY_P,
	[0x51] = KEY_Q,
	[0x52] = KEY_R,
	[0x53] = KEY_S,
	[0x54] = KEY_T,
	[0x55] = KEY_U,
	[0x56] = KEY_V,
	[0x57] = KEY_W,
	[0x58] = KEY_X,
	[0x59] = KEY_Y,
	[0x5A] = KEY_Z,
	[0x91] = KEY_KP1,
	[0x92] = KEY_VOLUMEDOWN,
	[0x93] = KEY_KP3,
	[0x94] = KEY_PREVIOUSSONG,
	[0x95] = KEY_KP5,
	[0x96] = KEY_NEXTSONG,
	[0x97] = KEY_KP7,
	[0x98] = KEY_VOLUMEUP,
	[0x99] = KEY_KP9,
};

void ist30xx_special_cmd(struct ist30xx_data *data, int cmd)
{
#ifdef IST30XX_GESTURE
	if (cmd == 0x22) {
		tsp_info("Double Tap Gesture\n");
		input_report_key(data->input_dev, KEY_WAKEUP, 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, KEY_WAKEUP, 0);
		input_sync(data->input_dev);

		return;
	}
	if (((cmd > 0x40) && (cmd < 0x5B)) || ((cmd > 0x90) && (cmd < 0x9A))) {
		tsp_info("Gesture touch: 0x%02X\n", cmd);

		if ((cmd != 0x92) && (cmd != 0x94) && (cmd != 0x96) && (cmd != 0x98)) {
			input_report_key(data->input_dev, KEY_WAKEUP, 1);
			input_sync(data->input_dev);
			input_report_key(data->input_dev, KEY_WAKEUP, 0);
			input_sync(data->input_dev);

			ist30xx_delay(500);
		}

		input_report_key(data->input_dev, lk_keycode[cmd], 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, lk_keycode[cmd], 0);
		input_sync(data->input_dev);

		return;
	}
#endif
#ifdef IST30XX_SURFACE_TOUCH
	if (((cmd & (~SPECIAL_START_MASK)) >= 0x20) &&
			((cmd & (~SPECIAL_START_MASK)) <= 0x21)) {
		if (cmd & SPECIAL_START_MASK) {
			tsp_info("Surface touch start: 0x%02X\n", cmd);
		} else {
			if ((cmd & SPECIAL_SUCCESS_MASK) > 0) {
				tsp_info("Surface touch stop: 0x%02X\n", cmd);
				input_report_key(data->input_dev, lk_keycode[cmd], 1);
				input_sync(data->input_dev);
				input_report_key(data->input_dev, lk_keycode[cmd], 0);
				input_sync(data->input_dev);
			} else {
				tsp_info("Surface touch cancel: 0x%02X\n", cmd);
			}
		}

		return;
	}
#endif
#ifdef IST30XX_BLADE_TOUCH
	if (((cmd & (~SPECIAL_START_MASK)) >= 0x10) &&
			((cmd & (~SPECIAL_START_MASK)) <= 0x14)) {
		if (cmd & SPECIAL_START_MASK) {
			tsp_info("Blade touch start: 0x%02X\n", cmd);
		} else {
			if ((cmd & SPECIAL_SUCCESS_MASK) > 0) {
				tsp_info("Blade touch stop: 0x%02X\n", cmd);
				input_report_key(data->input_dev, lk_keycode[cmd], 1);
				input_sync(data->input_dev);
				input_report_key(data->input_dev, lk_keycode[cmd], 0);
				input_sync(data->input_dev);
			} else {
				tsp_info("Blade touch cancel: 0x%02X\n", cmd);
			}
		}

		return;
	}
#endif

	tsp_warn("Not support gesture cmd: 0x%02X\n", cmd);
}
#endif

#define PRESS_MSG_MASK          (0x01)
#define MULTI_MSG_MASK          (0x02)
#define TOUCH_DOWN_MESSAGE      ("p")
#define TOUCH_UP_MESSAGE        ("r")
#define TOUCH_MOVE_MESSAGE      (" ")
void print_tsp_event(struct ist30xx_data *data, int id, finger_info *finger)
{
	bool press;

	press = PRESSED_FINGER(data->t_status, id);

	if (press) {
		if (data->tsp_touched[id] == false) {
#ifdef IST30XX_USE_3D_TOUCH
			tsp_noti("%s%d (%d, %d)(f:%d)\n", TOUCH_DOWN_MESSAGE, id + 1,
					finger->bit_field.x, finger->bit_field.y,
					finger->bit_field.f);
#else
			tsp_noti("%s%d (%d, %d)\n", TOUCH_DOWN_MESSAGE, id + 1,
					finger->bit_field.x, finger->bit_field.y);
#endif
			data->tsp_touched[id] = true;
		} else {
#ifdef IST30XX_USE_3D_TOUCH
			tsp_debug("%s%d (%d, %d)(f:%d)\n", TOUCH_MOVE_MESSAGE, id + 1,
					finger->bit_field.x, finger->bit_field.y,
					finger->bit_field.f);
#else
			tsp_debug("%s%d (%d, %d)\n", TOUCH_MOVE_MESSAGE, id + 1,
					finger->bit_field.x, finger->bit_field.y);
#endif
		}
	} else {
		if (data->tsp_touched[id] == true) {
			tsp_noti("%s%d\n", TOUCH_UP_MESSAGE, id + 1);
			data->tsp_touched[id] = false;
		}
	}
}
#ifdef IST30XX_USE_KEY
#define PRESS_MSG_KEY       (0x06)
void print_tkey_event(struct ist30xx_data *data, int id)
{
	bool press = PRESSED_KEY(data->t_status, id);

	if (press) {
		if (data->tkey_pressed[id] == false) {
			tsp_noti("k %s%d\n", TOUCH_DOWN_MESSAGE, id + 1);
			data->tkey_pressed[id] = true;
		}
	} else {
		if (data->tkey_pressed[id] == true) {
			tsp_noti("k %s%d\n", TOUCH_UP_MESSAGE, id + 1);
			data->tkey_pressed[id] = false;
		}
	}
}
#endif
static void release_finger(struct ist30xx_data *data, int id)
{
	input_mt_slot(data->input_dev, id);
	input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);

#ifdef IST30XX_TRACKING_MODE
	ist30xx_tracking(TRACK_POS_FINGER + id + 1);
#endif
	tsp_info("forced touch release: %d\n", id + 1);

	data->tsp_touched[id] = false;
	if (data->debugging_mode && (id < 2))
		data->t_frame[id] = 0;

	input_sync(data->input_dev);
}

#ifdef IST30XX_USE_KEY
static void release_key(struct ist30xx_data *data, int id)
{
	input_report_key(data->input_dev, ist30xx_key_code[id], false);

#ifdef IST30XX_TRACKING_MODE
	ist30xx_tracking(TRACK_POS_KEY + id + 1);
#endif
	tsp_info("forced key release: %d\n", id + 1);

	data->tkey_pressed[id] = false;

	input_sync(data->input_dev);
}
#endif
static void clear_input_data(struct ist30xx_data *data)
{
	int id = 0;
	u32 status;

	status = PARSE_FINGER_STATUS(data->t_status);
	while (status) {
		if (status & 1)
			release_finger(data, id);
		status >>= 1;
		id++;
	}
#ifdef IST30XX_USE_KEY
	id = 0;
	status = PARSE_KEY_STATUS(data->t_status);
	while (status) {
		if (status & 1)
			release_key(data, id);
		status >>= 1;
		id++;
	}
#endif

	data->t_status = 0;
}

static int check_valid_coord(u32 *msg, int cnt)
{
	u8 *buf = (u8 *)msg;
	u8 chksum1 = msg[0] >> 24;
	u8 chksum2 = 0;
	u32 tmp = msg[0];

	msg[0] &= 0x00FFFFFF;

	cnt *= IST30XX_DATA_LEN;

	while (cnt--)
		chksum2 += *buf++;

	msg[0] = tmp;

	if (chksum1 != chksum2) {
		tsp_err("intr chksum: %02x, %02x\n", chksum1, chksum2);
		return -EPERM;
	}

	return 0;
}

static void report_input_data(struct ist30xx_data *data, int finger_counts,
		int key_counts)
{
	int id;
	bool press = false;
	finger_info *fingers = (finger_info *)data->fingers;

	int idx = 0;
	u32 status;

	memset(data->t_frame, 0, sizeof(data->t_frame));

	status = PARSE_FINGER_STATUS(data->t_status);
	for (id = 0; id < IST30XX_MAX_FINGERS; id++) {
		press = (status & (1 << id)) ? true : false;

		input_mt_slot(data->input_dev, id);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, press);

		print_tsp_event(data, id, &fingers[idx]);

		if (press == false)
			continue;

		if (data->debugging_mode && (id < 2))
			data->t_frame[id] = fingers[idx].full_field;

		input_report_abs(data->input_dev, ABS_MT_POSITION_X,
				fingers[idx].bit_field.x);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
				fingers[idx].bit_field.y);
#ifdef IST30XX_USE_3D_TOUCH
		input_report_abs(data->input_dev, ABS_MT_PRESSURE,
				fingers[idx].bit_field.f);
#else
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
				fingers[idx].bit_field.w);
#endif
		idx++;
	}

#ifdef IST30XX_USE_KEY
	status = PARSE_KEY_STATUS(data->t_status);
	for (id = 0; id < ARRAY_SIZE(ist30xx_key_code); id++) {
		press = (status & (1 << id)) ? true : false;

		input_report_key(data->input_dev, ist30xx_key_code[id], press);

		print_tkey_event(data, id);
	}
#endif

	data->irq_err_cnt = 0;
	data->scan_retry = 0;

	input_sync(data->input_dev);
}

void recording_data(struct ist30xx_data *data, bool idle)
{
	int ret;
	int i;
	int count = 0;
	mm_segment_t old_fs = { 0 };
	struct file *fp = NULL;
	u32 addr = 0;
	u32 len;
	u32 *buf32;
	const int msg_len = 128;
	char msg[msg_len];
	char *buf;
	TSP_INFO *tsp = &data->tsp_info;

	if (idle && (data->rec_mode > 1))
		goto state_idle;

	ist30xx_delay(data->rec_delay);

	buf32 = kzalloc(IST30XX_MAX_NODE_NUM * sizeof(u32), GFP_KERNEL);
	buf = kzalloc(IST30XX_MAX_NODE_NUM * 20, GFP_KERNEL);

	for (i = data->rec_start_ch.tx; i <= data->rec_stop_ch.tx; i++) {
		addr = ((i * tsp->ch_num.rx + data->rec_start_ch.rx) * IST30XX_DATA_LEN)
				+ IST30XX_DA_ADDR(data->raw_addr);
		len = data->rec_stop_ch.rx - data->rec_start_ch.rx + 1;
		ret = ist30xx_burst_read(data->client, addr,
				buf32 + (i * tsp->ch_num.rx + data->rec_start_ch.rx), len,
				true);
		if (ret)
			goto err_rec_fail;
	}

	for (i = 0; i < IST30XX_MAX_NODE_NUM; i++) {
		count += snprintf(msg, msg_len, "%08x ", buf32[i]);
		strncat(buf, msg, msg_len);
	}

	for (i = 0; i < IST30XX_MAX_NODE_NUM; i++) {
		count += snprintf(msg, msg_len, "%08x ", 0);
		strncat(buf, msg, msg_len);
	}

	count += snprintf(msg, msg_len, "\n");
	strncat(buf, msg, msg_len);

	old_fs = get_fs();
	set_fs(get_ds());

	fp = filp_open(data->rec_file_name, O_CREAT|O_WRONLY|O_APPEND, 0);
	if (IS_ERR(fp)) {
		tsp_err("file %s open error:%d\n", data->rec_file_name, PTR_ERR(fp));
		goto err_file_open;
	}

	fp->f_op->write(fp, buf, count, &fp->f_pos);
	fput(fp);

	filp_close(fp, NULL);

err_file_open:
	set_fs(old_fs);

err_rec_fail:
	kfree(buf32);
	kfree(buf);

state_idle:
	data->ignore_delay = true;
	ist30xx_write_cmd(data, IST30XX_HIB_CMD,
			(eHCOM_SET_REC_MODE << 16) | (IST30XX_START_SCAN & 0xFFFF));
	data->ignore_delay = false;
}

static void cal_ref_work_func(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work, struct ist30xx_data,
			work_cal_reference);

	mutex_lock(&data->lock);
	ist30xx_reset(data, false);
	ist30xx_cal_reference(data);
	mutex_unlock(&data->lock);

	data->cal_ref_count++;
}

void ist30xx_scheduled_cal_ref(struct ist30xx_data *data)
{
	if (data->initialized)
		schedule_delayed_work(&data->work_cal_reference, 0);
}

static irqreturn_t ist30xx_irq_thread(int irq, void *ptr)
{
	int i, ret;
	int key_cnt, finger_cnt, read_cnt;
	struct ist30xx_data *data = (struct ist30xx_data *)ptr;
	int offset = 1;
	u32 t_status;
	u32 msg[IST30XX_MAX_FINGERS * 2 + offset];
	u32 *buf32;
	u32 debugBuf32[IST30XX_MAX_DEBUGINFO / IST30XX_DATA_LEN];
#ifdef IST30XX_TRACKING_MODE
	u32 tracking_value;
#endif
	u32 ms;
	u32 touch[2];
	u32 result;
	bool idle = false;

	data->irq_working = true;

	if (unlikely(!data->irq_enabled))
		goto irq_ignore;

	ms = get_milli_second(data);

	if (data->debugging_mode) {
		ist30xx_burst_read(data->client,
				IST30XX_DA_ADDR(data->debugging_addr), debugBuf32,
				data->debugging_size / IST30XX_DATA_LEN, true);
	}

#ifdef IST30XX_DEBUG
	if (data->intr_debug1_size > 0) {
		buf32 = kzalloc(data->intr_debug1_size * sizeof(u32), GFP_KERNEL);
		tsp_debug("Intr_debug1 (addr: 0x%08x)\n", data->intr_debug1_addr);
		ist30xx_burst_read(data->client,
				IST30XX_DA_ADDR(data->intr_debug1_addr), buf32,
				data->intr_debug1_size, true);

		for (i = 0; i < data->intr_debug1_size; i++)
			tsp_debug("\t%08x\n", buf32[i]);
#ifdef IST30XX_TRACKING_MODE
		tracking_value = TRACKING_INTR_DEBUG1_VALID;
		ist30xx_put_track_ms(ms);
		ist30xx_put_track(&tracking_value, 1);
		ist30xx_put_track(buf32, data->intr_debug1_size);
#endif
		kfree(buf32);
	}

	if (data->intr_debug2_size > 0) {
		buf32 = kzalloc(data->intr_debug2_size * sizeof(u32), GFP_KERNEL);
		tsp_debug("Intr_debug2 (addr: 0x%08x)\n", data->intr_debug2_addr);
		ist30xx_burst_read(data->client,
				IST30XX_DA_ADDR(data->intr_debug2_addr), buf32,
				data->intr_debug2_size, true);

		for (i = 0; i < data->intr_debug2_size; i++)
			tsp_debug("\t%08x\n", buf32[i]);
#ifdef IST30XX_TRACKING_MODE
		tracking_value = TRACKING_INTR_DEBUG2_VALID;
		ist30xx_put_track_ms(ms);
		ist30xx_put_track(&tracking_value, 1);
		ist30xx_put_track(buf32, data->intr_debug2_size);
#endif
		kfree(buf32);
	}
#endif
	memset(msg, 0, sizeof(msg));

	ret = ist30xx_read_reg(data->client, IST30XX_HIB_INTR_MSG, msg);
	if (unlikely(ret))
		goto irq_err;

	tsp_verb("intr msg: 0x%08x\n", *msg);

	if (unlikely(*msg == IST30XX_INITIAL_VALUE)) {
		tsp_debug("End Initial\n");
		goto irq_event;
	}

	if (unlikely(*msg == IST30XX_REC_VALUE)) {
		idle = true;
		goto irq_end;
	}

	if (unlikely(*msg == IST30XX_DEBUGGING_VALUE))
		goto irq_end;

	if (unlikely((*msg & IST30XX_EXCEPT_MASK) == IST30XX_EXCEPT_VALUE)) {
		tsp_err("Occured IC exception(0x%02X)\n", *msg & 0xFF);
		ret = ist30xx_burst_read(data->client, IST30XX_HIB_COORD,
				&msg[offset], IST30XX_MAX_EXCEPT_SIZE, true);
		if (unlikely(ret))
			tsp_err(" exception value read error(%d)\n", ret);
		else
			tsp_err(" exception value : 0x%08X, 0x%08X\n", msg[1], msg[2]);

		goto irq_ic_err;
	}

	if (unlikely(*msg == 0 || *msg == 0xFFFFFFFF))
		goto irq_err;

#ifdef IST30XX_TRACKING_MODE
	tracking_value = TRACKING_INTR_VALID;
	ist30xx_put_track_ms(ms);
	ist30xx_put_track(&tracking_value, 1);
	ist30xx_put_track(msg, 1);
#endif

	if (unlikely((*msg & CALIB_MSG_MASK) == CALIB_MSG_VALID)) {
		data->status.calib_msg = *msg;
		tsp_info("calib status: 0x%08X\n", data->status.calib_msg);

		goto irq_event;
	}

	if (unlikely((*msg & CAL_REF_MSG_MASK) == CAL_REF_MSG_VALID)) {
		data->status.cal_ref_msg = *msg;
		tsp_info("cal ref status: 0x%08X\n", data->status.cal_ref_msg);
		if ((data->status.update != 1) && (data->status.calib == 0)) {
			if (data->cal_ref_count < IST30XX_MAX_CAL_REF_CNT) {
				result = CAL_REF_TO_STATUS(data->status.cal_ref_msg);
				if ((result & CAL_REF_NONE) ||
						(result & CAL_REF_FAIL_UNKNOWN) ||
						(result & CAL_REF_FAIL_CRC) ||
						(result & CAL_REF_FAIL_VERIFY))
					ist30xx_scheduled_cal_ref(data);
			}
		}

		goto irq_event;
	}

#ifdef IST30XX_CMCS_TEST
	if (((*msg & CMCS_MSG_MASK) == CM_MSG_VALID) ||
			((*msg & CMCS_MSG_MASK) == CM2_MSG_VALID) ||
			((*msg & CMCS_MSG_MASK) == CS_MSG_VALID) ||
			((*msg & CMCS_MSG_MASK) == CMJIT_MSG_VALID) ||
			((*msg & CMCS_MSG_MASK) == INT_MSG_VALID)) {
		data->status.cmcs = *msg;
		tsp_info("CMCS notify: 0x%08X\n", *msg);

		goto irq_event;
	}
#endif

#if (defined(IST30XX_GESTURE) || defined(IST30XX_SURFACE_TOUCH) ||  \
		defined(IST30XX_BLADE_TOUCH))
	if (ist30xx_gesture_func_on) {
		ret = PARSE_SPECIAL_MESSAGE(*msg);
		if (unlikely(ret > 0)) {
			tsp_verb("special cmd: %d (0x%08X)\n", ret, *msg);
			ist30xx_special_cmd(data, ret);

			goto irq_event;
		}
	}
#endif

	memset(data->fingers, 0, sizeof(data->fingers));

	if (unlikely(!CHECK_INTR_STATUS(*msg)))
		goto irq_err;

	t_status = *msg;
	key_cnt = PARSE_KEY_CNT(t_status);
	finger_cnt = PARSE_FINGER_CNT(t_status);

	if (unlikely((finger_cnt > data->max_fingers) ||
			(key_cnt > data->max_keys))) {
		tsp_warn("Invalid touch count - finger: %d(%d), key: %d(%d)\n",
				finger_cnt, data->max_fingers, key_cnt, data->max_keys);
		goto irq_err;
	}

	if (finger_cnt > 0) {
		ret = ist30xx_burst_read(data->client, IST30XX_HIB_COORD, &msg[offset],
				finger_cnt, true);
		if (unlikely(ret))
			goto irq_err;

		for (i = 0; i < finger_cnt; i++)
			data->fingers[i].full_field = msg[i + offset];

#ifdef IST30XX_TRACKING_MODE
		ist30xx_put_track(msg + offset, finger_cnt);
#endif
		for (i = 0; i < finger_cnt; i++) {
			tsp_verb("intr msg(%d): 0x%08x, %d\n",
					i + offset, msg[i + offset], data->z_values[i]);
		}
	}

	read_cnt = finger_cnt + 1;

	ret = check_valid_coord(&msg[0], read_cnt);
	if (unlikely(ret))
		goto irq_err;

	data->t_status = t_status;
	report_input_data(data, finger_cnt, key_cnt);

#ifdef IST30XX_DEBUG
	if (data->intr_debug3_size > 0) {
		buf32 = kzalloc(data->intr_debug3_size * sizeof(u32), GFP_KERNEL);
		tsp_debug("Intr_debug3 (addr: 0x%08x)\n", data->intr_debug3_addr);
		ist30xx_burst_read(data->client,
				IST30XX_DA_ADDR(data->intr_debug3_addr), buf32,
				data->intr_debug3_size, true);

		for (i = 0; i < data->intr_debug3_size; i++)
			tsp_debug("\t%08x\n", buf32[i]);
#ifdef IST30XX_TRACKING_MODE
		tracking_value = TRACKING_INTR_DEBUG3_VALID;
		ist30xx_put_track_ms(ms);
		ist30xx_put_track(&tracking_value, 1);
		ist30xx_put_track(buf32, data->intr_debug3_size);
#endif
		kfree(buf32);
	}
#endif

	goto irq_end;

irq_err:
	tsp_err("intr msg: 0x%08x, ret: %d\n", msg[0], ret);
#ifdef IST30XX_GESTURE
	if (!data->suspend)
#endif
		ist30xx_request_reset(data);
	goto irq_event;
irq_end:
	if (data->rec_mode)
		recording_data(data, idle);
	if (data->debugging_mode) {
		if ((data->debugging_scancnt == debugBuf32[0]) && data->debugging_noise)
			goto irq_event;

		memset(touch, 0, sizeof(touch));
		touch[0] = data->t_frame[0];
		touch[1] = data->t_frame[1];

#ifdef IST30XX_DEBUG
		ist30xx_put_frame(data, ms, touch, debugBuf32,
				data->debugging_size / IST30XX_DATA_LEN);
		data->debugging_scancnt = debugBuf32[0];
#endif
	}
irq_event:
irq_ignore:
	data->irq_working = false;
	data->event_ms = (u32)get_milli_second(data);
	if (data->initialized)
		mod_timer(&data->event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL);
	return IRQ_HANDLED;

irq_ic_err:
	ist30xx_scheduled_reset(data);
	data->irq_working = false;
	data->event_ms = (u32)get_milli_second(data);
	if (data->initialized)
		mod_timer(&data->event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL);
	return IRQ_HANDLED;
}

static int ist30xx_power_init(struct ist30xx_data *data)
{
	int ret;

	data->vdd = regulator_get(&data->client->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		ret = PTR_ERR(data->vdd);
		tsp_err("Regulator get failed vdd ret=%d\n", ret);
		return ret;
	}

	ret = regulator_set_voltage(data->vdd, 2850000, 2850000);
	if (ret) {
		tsp_err("Regulator set_vtg failed vdd ret=%d\n", ret);
		return ret;
	}

	data->vddio = regulator_get(&data->client->dev, "vcc_i2c");
	if (IS_ERR(data->vddio)) {
		ret = PTR_ERR(data->vddio);
		tsp_err("Regulator get failed vcc_i2c ret=%d\n", ret);
		return ret;
	}

	ret = regulator_set_voltage(data->vddio, 1800000, 1800000);
	if (ret) {
		tsp_err("Regulator set_vtg failed vcc_i2c ret=%d\n", ret);
		return ret;
	}

   ret = regulator_enable(data->vdd);
	if (ret) {
		tsp_err("Regulator vdd enable failed rc=%d\n", ret);
		return ret;
	}

	ret = regulator_enable(data->vddio);
	if (ret) {
		tsp_err("Regulator vddio enable failed rc=%d\n", ret);
		return ret;
	}

	return 0;
}

static int ist30xx_pinctrl_init(struct ist30xx_data *data)
{
	int ret;

	data->pinctrl = devm_pinctrl_get(&(data->client->dev));
	if (IS_ERR_OR_NULL(data->pinctrl)) {
		ret = PTR_ERR(data->pinctrl);
		tsp_err("target does not use pinctrl %d\n", ret);
		goto err_pinctrl_get;
	}

	data->pinctrl_state_active = pinctrl_lookup_state(data->pinctrl,
			"pmx_ts_active");
	if (IS_ERR_OR_NULL(data->pinctrl_state_active)) {
		ret = PTR_ERR(data->pinctrl_state_active);
		tsp_err("can't lookup %s pinstate %d\n", "pmx_ts_active", ret);
		goto err_pinctrl_lookup;
	}

	data->pinctrl_state_suspend = pinctrl_lookup_state(data->pinctrl,
			"pmx_ts_suspend");
	if (IS_ERR_OR_NULL(data->pinctrl_state_suspend)) {
		ret = PTR_ERR(data->pinctrl_state_suspend);
		tsp_err("can't lookup %s pinstate %d\n", "pmx_ts_suspend", ret);
		goto err_pinctrl_lookup;
	}

	data->pinctrl_state_release = pinctrl_lookup_state(data->pinctrl,
			"pmx_ts_release");
	if (IS_ERR_OR_NULL(data->pinctrl_state_release)) {
		ret = PTR_ERR(data->pinctrl_state_release);
		tsp_err("can't lookup %s pinstate %d\n", "pmx_ts_release", ret);
		goto err_pinctrl_lookup;
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(data->pinctrl);
err_pinctrl_get:
	data->pinctrl = NULL;

	return ret;
}

static int ist30xx_suspend(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct ist30xx_data *data = i2c_get_clientdata(client);

	if (data->debugging_mode)
		return 0;

	del_timer(&data->event_timer);
	cancel_delayed_work_sync(&data->work_reset_check);
#ifdef IST30XX_NOISE_MODE
	cancel_delayed_work_sync(&data->work_noise_protect);
#else
#ifdef IST30XX_FORCE_RELEASE
	cancel_delayed_work_sync(&data->work_force_release);
#endif
#endif
#ifdef IST30XX_ALGORITHM_MODE
	cancel_delayed_work_sync(&data->work_debug_algorithm);
#endif
	cancel_delayed_work_sync(&data->work_cal_reference);
	mutex_lock(&data->lock);
	ist30xx_disable_irq(data);
	ist30xx_internal_suspend(data);
	clear_input_data(data);
#ifdef IST30XX_GESTURE
	if (ist30xx_gesture_func_on) {
		if (data->gesture) {
			ist30xx_enable_irq(data);
			ist30xx_start(data);
			data->status.noise_mode = false;
			if (device_may_wakeup(&data->client->dev))
				enable_irq_wake(data->client->irq);
		}
	}
	else {
#endif

		if (data->pinctrl) {
			ret = pinctrl_select_state(data->pinctrl, data->pinctrl_state_suspend);
			if (ret < 0)
				tsp_err("cannot get suspend pinctrl state\n");
		}
#ifdef IST30XX_GESTURE
	}
#endif
	mutex_unlock(&data->lock);

	return 0;
}

static int ist30xx_resume(struct device *dev)
{
	int ret;
	struct i2c_client *client = to_i2c_client(dev);
	struct ist30xx_data *data = i2c_get_clientdata(client);

	data->noise_mode |= (1 << NOISE_MODE_POWER);

	if (data->debugging_mode && data->status.power)
		return 0;

	mutex_lock(&data->lock);
#ifdef IST30XX_GESTURE
	if (ist30xx_gesture_func_on) {
		if (data->gesture) {
			ist30xx_disable_irq(data);
		}
	}
#endif
	ist30xx_internal_resume(data);
	ist30xx_enable_irq(data);
	ist30xx_start(data);

#ifdef IST30XX_GESTURE

	if (ist30xx_gesture_func_on) {
		if (data->gesture) {
			if (device_may_wakeup(&data->client->dev))
				disable_irq_wake(data->client->irq);

		}
	}
	else {
#endif

		if (data->pinctrl) {
			ret = pinctrl_select_state(data->pinctrl, data->pinctrl_state_active);
			if (ret < 0)
				tsp_err("cannot get active pinctrl state\n");
		}
#ifdef IST30XX_GESTURE
	}
#endif

	mutex_unlock(&data->lock);

	return 0;
}

#ifdef USE_OPEN_CLOSE
static void ist30xx_ts_close(struct input_dev *dev)
{
	struct ist30xx_data *data = input_get_drvdata(dev);

	ist30xx_suspend(&data->client->dev);
}

static int ist30xx_ts_open(struct input_dev *dev)
{
	struct ist30xx_data *data = input_get_drvdata(dev);

	return ist30xx_resume(&data->client->dev);
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void ist30xx_early_suspend(struct early_suspend *h)
{
	struct ist30xx_data *data = container_of(h, struct ist30xx_data,
			early_suspend);

	ist30xx_suspend(&data->client->dev);
}

static void ist30xx_late_resume(struct early_suspend *h)
{
	struct ist30xx_data *data = container_of(h, struct ist30xx_data,
			early_suspend);

	ist30xx_resume(&data->client->dev);
}
#endif

void ist30xx_set_ta_mode(bool mode)
{
	struct ist30xx_data *data = ts_data;

	if (unlikely(mode == ((data->noise_mode >> NOISE_MODE_TA) & 1)))
		return;

	pr_err("%s(), mode = %d\n", __func__, mode);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_TA);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_TA);

#ifdef IST30XX_TA_RESET
	if (data->initialized)
		ist30xx_scheduled_reset(data);
#else
	if (data->initialized && data->status.power)
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				((eHCOM_SET_MODE_SPECIAL << 16) | (data->noise_mode & 0xFFFF)));
#endif
#ifdef IST30XX_TRACKING_MODE
	ist30xx_tracking(mode ? TRACK_CMD_TACON : TRACK_CMD_TADISCON);
#endif
}
EXPORT_SYMBOL(ist30xx_set_ta_mode);

void ist30xx_set_edge_mode(int mode)
{
	struct ist30xx_data *data = ts_data;

	tsp_info("%s(), mode = %d\n", __func__, mode);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_EDGE);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_EDGE);

	if (data->status.power)
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				((eHCOM_SET_MODE_SPECIAL << 16) | (data->noise_mode & 0xFFFF)));
}
EXPORT_SYMBOL(ist30xx_set_edge_mode);

void ist30xx_set_call_mode(int mode)
{
	struct ist30xx_data *data = ts_data;

	if (unlikely(mode == ((data->noise_mode >> NOISE_MODE_CALL) & 1)))
		return;

	tsp_info("%s(), mode = %d\n", __func__, mode);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_CALL);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_CALL);

#ifdef IST30XX_TA_RESET
	if (data->initialized)
		ist30xx_scheduled_reset(data);
#else
	if (data->initialized && data->status.power)
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				((eHCOM_SET_MODE_SPECIAL << 16) | (data->noise_mode & 0xFFFF)));
#endif
#ifdef IST30XX_TRACKING_MODE
	ist30xx_tracking(mode ? TRACK_CMD_CALL : TRACK_CMD_NOTCALL);
#endif
}
EXPORT_SYMBOL(ist30xx_set_call_mode);

void ist30xx_set_cover_mode(int mode)
{
	struct ist30xx_data *data = ts_data;

	if (unlikely(mode == ((data->noise_mode >> NOISE_MODE_COVER) & 1)))
		return;

	tsp_info("%s(), mode = %d\n", __func__, mode);

	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_COVER);
	else
		data->noise_mode &= ~(1 << NOISE_MODE_COVER);

#ifdef IST30XX_TA_RESET
	if (data->initialized)
		ist30xx_scheduled_reset(data);
#else
	if (data->initialized && data->status.power)
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				((eHCOM_SET_MODE_SPECIAL << 16) | (data->noise_mode & 0xFFFF)));
#endif
#ifdef IST30XX_TRACKING_MODE
	ist30xx_tracking(mode ? TRACK_CMD_COVER : TRACK_CMD_NOTCOVER);
#endif
}
EXPORT_SYMBOL(ist30xx_set_cover_mode);

#ifdef USE_TSP_TA_CALLBACKS
void charger_enable(struct tsp_callbacks *cb, int enable)
{
	bool charging = enable ? true : false;

	ist30xx_set_ta_mode(charging);
}

static void ist30xx_register_callback(struct tsp_callbacks *cb)
{
	charger_callbacks = cb;
	pr_info("%s\n", __func__);
}
#endif

static void reset_work_func(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work, struct ist30xx_data,
			work_reset_check);

	if (unlikely((data == NULL) || (data->client == NULL)))
		return;

	tsp_info("Request reset function\n");

	if (likely((data->initialized == 1) && (data->status.power == 1) &&
			(data->status.update != 1) && (data->status.calib != 1 &&
			(data->status.calib != 2)))) {
		mutex_lock(&data->lock);
		ist30xx_disable_irq(data);
#ifdef IST30XX_GESTURE
		if (data->suspend)
			ist30xx_internal_suspend(data);
		else
#endif
			ist30xx_reset(data, false);
		clear_input_data(data);
		ist30xx_enable_irq(data);
		ist30xx_start(data);
#ifdef IST30XX_GESTURE
		if (data->gesture && data->suspend)
			data->status.noise_mode = false;
#endif
		mutex_unlock(&data->lock);
	}
}

#ifdef IST30XX_INTERNAL_BIN
#ifdef IST30XX_UPDATE_BY_WORKQUEUE
static void fw_update_func(struct work_struct *work)
{
	int ret;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work,
			struct ist30xx_data, work_fw_update);

	if (unlikely((data == NULL) || (data->client == NULL)))
		return;

	tsp_info("FW update function\n");

	if (likely(ist30xx_auto_bin_update(data))) {
		ist30xx_disable_irq(data);
	} else {
		ret = ist30xx_get_info(data);
		tsp_info("Get info: %s\n", (ret == 0 ? "success" : "fail"));

		ist30xx_start(data);
		data->initialized = true;
	}
}
#endif
#endif

#if (defined(IST30XX_NOISE_MODE) || defined(IST30XX_FORCE_RELEASE))
#define TOUCH_STATUS_MAGIC      (0x00000075)
#define TOUCH_STATUS_MASK       (0x000000FF)
#define FINGER_ENABLE_MASK      (0x00100000)
#define SCAN_CNT_MASK           (0xFFE00000)
#define GET_FINGER_ENABLE(n)    ((n & FINGER_ENABLE_MASK) >> 20)
#define GET_SCAN_CNT(n)         ((n & SCAN_CNT_MASK) >> 21)
#endif
#ifdef IST30XX_NOISE_MODE
static void noise_work_func(struct work_struct *work)
{
	int ret;
	u32 touch_status = 0;
	u32 scan_count = 0;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work, struct ist30xx_data,
			work_noise_protect);

	ret = ist30xx_read_reg(data->client, IST30XX_HIB_TOUCH_STATUS,
			&touch_status);
	if (unlikely(ret)) {
		tsp_warn("Touch status read fail!\n");
		goto retry_timer;
	}

#ifdef IST30XX_TRACKING_MODE
	ist30xx_put_track_ms(data->timer_ms);
	ist30xx_put_track(&touch_status, 1);
#endif
	tsp_verb("Touch Info: 0x%08x\n", touch_status);

	if (unlikely((touch_status & TOUCH_STATUS_MASK) != TOUCH_STATUS_MAGIC)) {
		tsp_warn("Touch status is not corrected! (0x%08x)\n", touch_status);
		goto retry_timer;
	}

	if (GET_FINGER_ENABLE(touch_status) == 0) {
		if ((PARSE_FINGER_CNT(data->t_status) > 0) ||
				(PARSE_KEY_CNT(data->t_status) > 0))
			clear_input_data(data);
	}

	scan_count = GET_SCAN_CNT(touch_status);

	if (unlikely(scan_count == data->scan_count)) {
		tsp_warn("TSP IC is not responded! (0x%08x)\n", scan_count);
		goto retry_timer;
	}

	data->scan_retry = 0;
	data->scan_count = scan_count;
	return;

retry_timer:
	data->scan_retry++;
	tsp_warn("Retry touch status!(%d)\n", data->scan_retry);

	if (unlikely(data->scan_retry == data->max_scan_retry)) {
		ist30xx_scheduled_reset(data);
		data->scan_retry = 0;
	}
}
#else
#ifdef IST30XX_FORCE_RELEASE
static void release_work_func(struct work_struct *work)
{
	int ret;
	u32 touch_status = 0;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work, struct ist30xx_data,
			work_force_release);

	if ((PARSE_FINGER_CNT(data->t_status) == 0) &&
			(PARSE_KEY_CNT(data->t_status) == 0))
		return;

	ret = ist30xx_read_reg(data->client, IST30XX_HIB_TOUCH_STATUS,
			&touch_status);
	if (unlikely(ret)) {
		tsp_warn("Touch status read fail!\n");
		return;
	}

#ifdef IST30XX_TRACKING_MODE
	ist30xx_put_track_ms(data->timer_ms);
	ist30xx_put_track(&touch_status, 1);
#endif
	tsp_verb("Touch Info: 0x%08x\n", touch_status);

	if (unlikely((touch_status & TOUCH_STATUS_MASK) != TOUCH_STATUS_MAGIC)) {
		tsp_warn("Touch status is not corrected! (0x%08x)\n", touch_status);
		return;
	}

	if (GET_FINGER_ENABLE(touch_status) == 0)
		clear_input_data(data);
}
#endif
#endif

#ifdef IST30XX_ALGORITHM_MODE
static void debug_work_func(struct work_struct *work)
{
	int ret = -EPERM;
	int i;
	u32 *buf32;

	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work,
			struct ist30xx_data, work_debug_algorithm);

	buf32 = kzalloc(data->algr_size * sizeof(u32), GFP_KERNEL);
	ret = ist30xx_burst_read(data->client, IST30XX_DA_ADDR(data->algr_addr),
			buf32, data->algr_size, true);
	if (ret) {
		tsp_warn("Algorithm mem addr read fail!\n");
		return;
	}

#ifdef IST30XX_TRACKING_MODE
	ist30xx_put_track(buf32, data->algr_size);
#endif
	tsp_info("algorithm struct\n");
	for (i = 0; i < data->algr_size; i++)
		tsp_info(" 0x%08x\n", buf32[i]);

	kfree(buf32);
}
#endif

void timer_handler(unsigned long timer_data)
{
	struct ist30xx_data *data = (struct ist30xx_data *)timer_data;
	struct ist30xx_status *status = &data->status;

	if (data->irq_working || !data->initialized || data->rec_mode)
		goto restart_timer;

	if (status->event_mode) {
		if (likely((status->power == 1) && (status->update != 1) &&
				(status->calib != 1))) {
			data->timer_ms = (u32)get_milli_second(data);
			if (unlikely(status->calib == 2)) {
				if ((status->calib_msg & CALIB_MSG_MASK) == CALIB_MSG_VALID) {
					tsp_info("Calibration check OK!!\n");
					schedule_delayed_work(&data->work_reset_check, 0);
					status->calib = 0;
				} else if (data->timer_ms - data->event_ms >= 3000) {
					tsp_info("calibration timeout over 3sec\n");
					schedule_delayed_work(&data->work_reset_check, 0);
					status->calib = 0;
				}
#ifdef IST30XX_NOISE_MODE
			} else if (likely(status->noise_mode)) {
				if (data->timer_ms - data->event_ms > 100)
					schedule_delayed_work(&data->work_noise_protect, 0);
#else
#ifdef IST30XX_FORCE_RELEASE
			} else if (likely(status->noise_mode)) {
				if (data->timer_ms - data->event_ms > 100)
					schedule_delayed_work(&data->work_force_release, 0);
#endif
#endif
			}
#ifdef IST30XX_ALGORITHM_MODE
			if (data->algr_size > 0) {
				if (data->timer_ms - data->event_ms > 100)
					schedule_delayed_work(&data->work_debug_algorithm, 0);
			}
#endif
		}
	}

restart_timer:
	mod_timer(&data->event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL);
}

#ifdef CONFIG_OF
static int ist30xx_request_gpio(struct ist30xx_data *data)
{
	int ret;

	tsp_info("%s\n", __func__);

	if (gpio_is_valid(data->dt_data->reset_gpio)) {
		ret = gpio_request(data->dt_data->reset_gpio, "imagis,reset_gpio");
		if (ret) {
			tsp_err("unable to request reset gpio: %d\n",
					data->dt_data->reset_gpio);
			return ret;
		}

		ret = gpio_direction_output(data->dt_data->reset_gpio, 0);
		if (ret)
			tsp_err("unable to set direction for reset gpio: %d\n",
					data->dt_data->reset_gpio);

		gpio_set_value_cansleep(data->dt_data->reset_gpio, 0);
	}

	if (gpio_is_valid(data->dt_data->irq_gpio)) {
		ret = gpio_request(data->dt_data->irq_gpio, "imagis,irq_gpio");
		if (ret) {
			tsp_err("unable to request irq gpio: %d\n",
					data->dt_data->irq_gpio);
			return ret;
		}

		ret = gpio_direction_input(data->dt_data->irq_gpio);
		if (ret)
			tsp_err("unable to set direction for irq gpio: %d\n",
					data->dt_data->irq_gpio);

		gpio_set_value_cansleep(data->dt_data->reset_gpio, 0);

		data->client->irq = gpio_to_irq(data->dt_data->irq_gpio);
	}
	return 0;
}

static void ist30xx_free_gpio(struct ist30xx_data *data)
{
	tsp_info("%s\n", __func__);

	if (gpio_is_valid(data->dt_data->reset_gpio))
		gpio_free(data->dt_data->reset_gpio);

	if (gpio_is_valid(data->dt_data->irq_gpio))
		gpio_free(data->dt_data->irq_gpio);
}

static int ist30xx_parse_dt(struct device *dev, struct ist30xx_data *data)
{
	struct device_node *np = dev->of_node;

	data->dt_data->irq_gpio = of_get_named_gpio(np, "imagis,irq-gpio", 0);
	data->dt_data->reset_gpio = of_get_named_gpio(np, "imagis,reset-gpio", 0);

	tsp_info("##### Device tree #####\n");
	tsp_info(" reset gpio: %d\n", data->dt_data->reset_gpio);
	tsp_info(" irq gpio: %d\n", data->dt_data->irq_gpio);

	return 0;
}
#endif
#ifdef XIAOMI_PRODUCT
#define CTP_PROC_LOCKDOWN_FILE "tp_lockdown_info"
static struct proc_dir_entry *ctp_lockdown_status_proc;
static char tp_lockdown_info[128];

static int ctp_lockdown_proc_show(struct seq_file *file, void *data)
{
	char temp[40] = {0};

	sprintf(temp, "%s\n", tp_lockdown_info);
	seq_printf(file, "%s\n", temp);
	return 0;
}

static int ctp_lockdown_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, ctp_lockdown_proc_show, inode->i_private);
}

static const struct file_operations ctp_lockdown_proc_fops = {
	.open = ctp_lockdown_proc_open,
	.read = seq_read,
};
#endif
#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
		 unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct ist30xx_data *ist_data =
			container_of(self, struct ist30xx_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK &&
			ist_data && ist_data->client) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK)
			ist30xx_resume(&ist_data->client->dev);
		else if (*blank == FB_BLANK_POWERDOWN)
			ist30xx_suspend(&ist_data->client->dev);
	}
	return 0;
}
#endif

static char tp_info_summary[80] = "";

static int ist30xx_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret;
	int i;
	int retry = 3;
	struct ist30xx_data *data;
	struct input_dev *input_dev;
	char tp_temp_info[80];
#ifdef XIAOMI_PRODUCT
	u32 info_data[2];
#endif

	tsp_info("### IMAGIS probe(ver:%s, addr:0x%02X) ###\n",
			IMAGIS_TSP_DD_VERSION, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_INFO "i2c_check_functionality error\n");
		return -EIO;
	}

	data = kzalloc(sizeof(struct ist30xx_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	data->client = client;
#ifdef CONFIG_OF
	data->dt_data = NULL;
	if (client->dev.of_node) {
		data->dt_data = kzalloc(sizeof(struct ist30xx_dt_data), GFP_KERNEL);
		if (unlikely(!data->dt_data)) {
			tsp_err("failed to allocate dt data\n");
			goto err_alloc_dev;
		}

		ret = ist30xx_parse_dt(&client->dev, data);
		if (unlikely(ret))
			goto err_alloc_dev;
	} else {
		data->dt_data = NULL;
		tsp_err("don't exist of_node\n");
		goto err_alloc_dev;
	}

	if (data->dt_data)
		ret = ist30xx_request_gpio(data);
	if (ret) {
		goto err_alloc_dev;
	}
#endif

	ret = ist30xx_pinctrl_init(data);
	if (!ret && data->pinctrl) {
		ret = pinctrl_select_state(data->pinctrl, data->pinctrl_state_active);
		if (ret < 0)
			tsp_err("failed to select %s pinatate %d\n", "pmx_ts_active", ret);
	}

	input_dev = input_allocate_device();
	if (unlikely(!input_dev)) {
		tsp_err("input_allocate_device failed\n");
		goto err_alloc_dt;
	}

	tsp_info("client->irq : %d\n", client->irq);

	data->input_dev = input_dev;
	i2c_set_clientdata(client, data);

#ifdef USE_OPEN_CLOSE
	data->input_dev->open = ist30xx_ts_open;
	data->input_dev->close = ist30xx_ts_close;
#else
#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = ist30xx_early_suspend;
	data->early_suspend.resume = ist30xx_late_resume;
	register_early_suspend(&data->early_suspend);
#endif
#endif

	data->max_fingers = IST30XX_MAX_FINGERS;
	data->max_keys = IST30XX_MAX_KEYS;
	data->irq_enabled = false;
	data->status.event_mode = false;
	mutex_init(&data->lock);
	ts_data = data;

	data->ignore_delay = false;
	data->irq_working = false;
	data->cal_ref_count = 0;
	data->max_scan_retry = 2;
	data->max_irq_err_cnt = IST30XX_MAX_ERR_CNT;
	data->report_rate = -1;
	data->idle_rate = -1;
	data->timer_period_ms = 500;
#ifdef IST30XX_GESTURE
	data->suspend = false;
	data->gesture = true;
#endif
	data->fw_mode = IST30XX_CAL_MODE;
	data->rec_mode = 0;
	data->rec_file_name = kzalloc(IST30XX_REC_FILENAME_SIZE, GFP_KERNEL);
	data->debug_mode = 0;
	data->debugging_mode = 0;
	data->debugging_noise = 1;
	for (i = 0; i < IST30XX_MAX_FINGERS; i++)
		data->tsp_touched[i] = false;
#ifdef IST30XX_USE_KEY
	for (i = 0; i < IST30XX_MAX_KEYS; i++)
		data->tkey_pressed[i] = false;
#endif

	ret = ist30xx_power_init(data);
	if (ret)
		goto err_init_drv;

	ret = ist30xx_init_system(data);
	if (unlikely(ret)) {
		tsp_err("chip initialization failed\n");
		goto err_init_drv;
	}

#ifdef XIAOMI_PRODUCT
	retry = 1;
	while (retry-- > 0) {
		ret = ist30xxc_isp_info_read(data, XIAOMI_INFO_ADDR, info_data,
				XIAOMI_INFO_SIZE);
		if (ret == 0) {
			if ((info_data[XIAOMI_PID_INDEX] & XIAOMI_INFO_MASK)
					== XIAOMI_INFO_VALUE) {
				data->pid = XIAOMI_GET_PID(info_data[XIAOMI_PID_INDEX]);
				data->lockdown_upper = info_data[XIAOMI_LOCKDOWN_INDEX];
				break;
			}
		}

		if (retry == 0) {
			data->pid = 0;
			tsp_err("read isp info failed\n");
		}
	}

#endif

	retry = IST30XX_MAX_RETRY_CNT;
	while (retry-- > 0) {
		ret = ist30xx_read_reg(data->client, IST30XX_REG_CHIPID,
				&data->chip_id);
		data->chip_id &= 0xFFFF;
		if (unlikely(ret == 0)) {
			set_usb_charge_mode_par = 1;
			if ((data->chip_id == IST30XX_CHIP_ID) ||
					(data->chip_id == IST30XXC_DEFAULT_CHIP_ID) ||
					(data->chip_id == IST3048C_DEFAULT_CHIP_ID)) {
				break;
			}
		} else if (unlikely(retry == 0)) {
			goto err_init_drv;
		}

		ist30xx_reset(data, false);
	}
	tsp_info("TSP IC: %x\n", data->chip_id);

	ret = request_threaded_irq(client->irq, NULL, ist30xx_irq_thread,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT | IRQF_DISABLED , "ist30xx_ts",
			data);
	if (unlikely(ret))
		goto err_init_drv;

#ifdef IST30XX_INTERNAL_BIN
#ifdef XIAOMI_PRODUCT
	if (data->pid != 0) {
#endif
#ifdef IST30XX_UPDATE_BY_WORKQUEUE
	INIT_DELAYED_WORK(&data->work_fw_update, fw_update_func);
	schedule_delayed_work(&data->work_fw_update, IST30XX_UPDATE_DELAY);
#else

	ret = ist30xx_auto_bin_update(data);
	if (unlikely(ret != 0))
		goto err_irq;
#endif
#ifdef XIAOMI_PRODUCT
	}
#endif
#endif

	ret = ist30xx_init_update_sysfs(data);
	if (unlikely(ret))
		goto err_sysfs;

#ifdef IST30XX_DEBUG
	ret = ist30xx_init_misc_sysfs(data);
	if (unlikely(ret))
		goto err_sysfs;
#endif

#ifdef IST30XX_CMCS_TEST
	ret = ist30xx_init_cmcs_sysfs(data);
	if (unlikely(ret))
		goto err_sysfs;
#endif

#ifdef IST30XX_TRACKING_MODE
	ret = ist30xx_init_tracking_sysfs(data);
	if (unlikely(ret))
		goto err_sysfs;
#endif

	tsp_info("Create sysfs!!\n");

#if defined(CONFIG_FB)
	data->fb_notif.notifier_call = fb_notifier_callback;

	ret = fb_register_client(&data->fb_notif);

	if (ret) {
		goto err_sysfs;
		tsp_err("Unable to register fb_notifier\n");
	}
#endif
#ifndef IST30XX_UPDATE_BY_WORKQUEUE
	ret = ist30xx_get_info(data);
	tsp_info("Get info: %s\n", (ret == 0 ? "success" : "fail"));
	if (unlikely(ret))
		goto err_read_info;
#endif

#ifdef XIAOMI_PRODUCT
	memset(tp_lockdown_info, 0, sizeof(tp_lockdown_info));
	sprintf(tp_lockdown_info, "%08X%08X", data->fw.cur.lockdown[0], data->fw.cur.lockdown[1]);
	tsp_info("[%s]lockdowninfo:%s\n", __FUNCTION__, tp_lockdown_info);
	ctp_lockdown_status_proc = proc_create(CTP_PROC_LOCKDOWN_FILE, 0644, NULL, &ctp_lockdown_proc_fops);
	if (ctp_lockdown_status_proc == NULL) {
		tsp_err("tpd, create_proc_entry ctp_lockdown_status_proc failed\n");
	}
#endif

	INIT_DELAYED_WORK(&data->work_reset_check, reset_work_func);
#ifdef IST30XX_NOISE_MODE
	INIT_DELAYED_WORK(&data->work_noise_protect, noise_work_func);
#else
#ifdef IST30XX_FORCE_RELEASE
	INIT_DELAYED_WORK(&data->work_force_release, release_work_func);
#endif
#endif
#ifdef IST30XX_ALGORITHM_MODE
	INIT_DELAYED_WORK(&data->work_debug_algorithm, debug_work_func);
#endif
	INIT_DELAYED_WORK(&data->work_cal_reference, cal_ref_work_func);

	init_timer(&data->event_timer);
	data->event_timer.data = (unsigned long)data;
	data->event_timer.function = timer_handler;
	data->event_timer.expires = jiffies_64 + EVENT_TIMER_INTERVAL;
	mod_timer(&data->event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL * 2);

#ifdef USE_TSP_TA_CALLBACKS
	data->callbacks.inform_charger = charger_enable;
	ist30xx_register_callback(&data->callbacks);
#endif

#ifndef IST30XX_UPDATE_BY_WORKQUEUE
	ist30xx_start(data);
	data->initialized = true;
#endif
	strcpy(tp_info_summary, "[Vendor]Dongshan, [IC]IST3038C, [FW]Ver");
	sprintf(tp_temp_info, "%x", data->fw.bin.fw_ver);
	strcat(tp_info_summary, tp_temp_info);
	strcat(tp_info_summary, "\0");
	hq_regiser_hw_info(HWID_CTP, tp_info_summary);
	tsp_info("### IMAGIS probe success ###\n");

	return 0;

#ifndef IST30XX_UPDATE_BY_WORKQUEUE
err_read_info:
#endif
err_sysfs:
	class_destroy(ist30xx_class);
#ifndef IST30XX_UPDATE_BY_WORKQUEUE
err_irq:
#endif
	ist30xx_disable_irq(data);
	free_irq(client->irq, data);
err_init_drv:
	input_free_device(input_dev);
	data->status.event_mode = false;
	ist30xx_power_off(data);
#if (defined(CONFIG_HAS_EARLYSUSPEND) && !defined(USE_OPEN_CLOSE))
	unregister_early_suspend(&data->early_suspend);
#endif
err_alloc_dt:
	if (data->pinctrl) {
		if (IS_ERR_OR_NULL(data->pinctrl_state_release)) {
			devm_pinctrl_put(data->pinctrl);
			data->pinctrl = NULL;
		} else {
			ret = pinctrl_select_state(data->pinctrl,
					data->pinctrl_state_release);
			if (ret < 0)
				tsp_err("cannot get release pinctrl state\n");
		}
	}
	if (data->dt_data) {
		tsp_err("Error, ist30xx mem free\n");
		ist30xx_free_gpio(data);
		kfree(data->dt_data);
	}
#ifdef CONFIG_OF
err_alloc_dev:
#endif
	kfree(data);
	tsp_err("Error, ist30xx init driver\n");
	return -ENODEV;
}

static int ist30xx_remove(struct i2c_client *client)
{
	int ret;
	struct ist30xx_data *data = i2c_get_clientdata(client);

#if (defined(CONFIG_HAS_EARLYSUSPEND) && !defined(USE_OPEN_CLOSE))
	unregister_early_suspend(&data->early_suspend);
#endif

	ist30xx_disable_irq(data);
	free_irq(client->irq, data);
	ist30xx_power_off(data);
	if (data->dt_data) {
		ist30xx_free_gpio(data);
		kfree(data->dt_data);
	}

#if defined(CONFIG_FB)
	fb_unregister_client(&data->fb_notif);
#endif
	if (data->pinctrl) {
		if (IS_ERR_OR_NULL(data->pinctrl_state_release)) {
			devm_pinctrl_put(data->pinctrl);
			data->pinctrl = NULL;
		} else {
			ret = pinctrl_select_state(data->pinctrl,
					data->pinctrl_state_release);
			if (ret < 0)
				tsp_err("cannot get release pinctrl state\n");
		}
	}
	input_unregister_device(data->input_dev);
	input_free_device(data->input_dev);
	kfree(data);

	return 0;
}

static void ist30xx_shutdown(struct i2c_client *client)
{
	struct ist30xx_data *data = i2c_get_clientdata(client);

	del_timer(&data->event_timer);
	cancel_delayed_work_sync(&data->work_reset_check);
#ifdef IST30XX_NOISE_MODE
	cancel_delayed_work_sync(&data->work_noise_protect);
#else
#ifdef IST30XX_FORCE_RELEASE
	cancel_delayed_work_sync(&data->work_force_release);
#endif
#endif
#ifdef IST30XX_ALGORITHM_MODE
	cancel_delayed_work_sync(&data->work_debug_algorithm);
#endif
	cancel_delayed_work_sync(&data->work_cal_reference);
	mutex_lock(&data->lock);
	ist30xx_disable_irq(data);
	ist30xx_internal_suspend(data);
	clear_input_data(data);
	mutex_unlock(&data->lock);
}

static struct i2c_device_id ist30xx_idtable[] = {
	{ IST30XX_DEV_NAME, 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, ist30xx_idtable);

#ifdef CONFIG_OF
static struct of_device_id ist30xx_match_table[] = {
	{ .compatible = "imagis,ist30xx-ts", },
	{ },
};
#else
#define ist30xx_match_table NULL
#endif

static struct i2c_driver ist30xx_i2c_driver = {
	.id_table   = ist30xx_idtable,
	.probe      = ist30xx_probe,
	.remove     = ist30xx_remove,
	.shutdown   = ist30xx_shutdown,
	.driver     = {
		.owner          = THIS_MODULE,
		.name           = IST30XX_DEV_NAME,
		.of_match_table = ist30xx_match_table,
	},
};

static int __init ist30xx_init(void)
{
	tsp_info("%s()\n", __func__);
	return i2c_add_driver(&ist30xx_i2c_driver);
}

static void __exit ist30xx_exit(void)
{
	tsp_info("%s()\n", __func__);
	i2c_del_driver(&ist30xx_i2c_driver);
}

module_init(ist30xx_init);
module_exit(ist30xx_exit);

MODULE_DESCRIPTION("Imagis IST30XX touch driver");
MODULE_LICENSE("GPL");

