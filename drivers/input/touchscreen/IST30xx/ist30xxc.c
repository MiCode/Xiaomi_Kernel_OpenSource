/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2016 XiaoMi, Inc.
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

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/input/mt.h>

#include <linux/regulator/consumer.h>
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
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <asm/unaligned.h>
#include <linux/uaccess.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <linux/err.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>
#endif

#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif

#include "ist30xxc.h"
#include "ist30xxc_update.h"
#include "ist30xxc_tracking.h"

#if IST30XX_DEBUG
#include "ist30xxc_misc.h"
#endif

#if IST30XX_CMCS_TEST
#include "ist30xxc_cmcs.h"
#endif

#if  WT_ADD_CTP_INFO
static char tp_string_version[40];
#endif

#if CTP_LOCKDOWN_INFO
lockinfoH Lockdown_Info_High;
lockinfoL Lockdown_Info_LOW;
extern u8 tp_color;
#endif

#if CTP_CHARGER_DETECT
#include <linux/power_supply.h>
#endif

#define FT_VTG_MIN_UV       2600000
#define FT_VTG_MAX_UV       3300000
#define FT_I2C_VTG_MIN_UV   1800000
#define FT_I2C_VTG_MAX_UV   1800000

#define MAX_ERR_CNT						(100)
#define EVENT_TIMER_INTERVAL			(HZ * timer_period_ms / 1000)
#define CHECK_CHARGER_INTERVAL          (HZ * 100 / 1000)
u32 event_ms = 0, timer_ms = 0;
static struct timer_list event_timer;
static struct timespec t_current;	/* nano seconds */
int timer_period_ms = 500;	/* 500 msec */

#if IST30XX_USE_KEY
int fhd_key_dim_x[] = { 0, FHD_MENU_KEY_X, FHD_HOME_KEY_X, FHD_BACK_KEY_X, };
#endif

struct ist30xx_data *ts_data;
struct ist30xx_data *global_ts_data;

#if CTP_CHARGER_DETECT
extern int power_supply_get_battery_charge_state(struct power_supply *psy);
static struct power_supply *batt_psy;
static u8 is_charger_plug;
static u8 pre_charger_status;
#endif

DEFINE_MUTEX(ist30xx_mutex);

int ist30xx_dbg_level = IST30XX_DEBUG_LEVEL;
void tsp_printk(int level, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;

	if (ist30xx_dbg_level < level)
		return;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	printk("%s %pV", IST30XX_DEBUG_TAG, &vaf);

	va_end(args);
}

long get_milli_second(void)
{
	ktime_get_ts(&t_current);

	return t_current.tv_sec * 1000 + t_current.tv_nsec / 1000000;
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
	long start_ms = get_milli_second();
	long curr_ms = 0;

	while (1) {
		if (!data->irq_working)
			break;

		curr_ms = get_milli_second();
		if ((curr_ms < 0) || (start_ms < 0)
		    || (curr_ms - start_ms > ms)) {
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
		ist30xx_tracking(TRACK_INTR_DISABLE);
		disable_irq(data->client->irq);
		data->irq_enabled = 0;
		data->status.event_mode = false;
	}
}

void ist30xx_enable_irq(struct ist30xx_data *data)
{
	if (likely(!data->irq_enabled)) {
		ist30xx_tracking(TRACK_INTR_ENABLE);
		enable_irq(data->client->irq);
		ist30xx_delay(10);
		data->irq_enabled = 1;
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

#define NOISE_MODE_TA		(0)
#define NOISE_MODE_CALL		(1)
#define NOISE_MODE_COVER	(2)
#define NOISE_MODE_SECURE	(3)
#define NOISE_MODE_EDGE		(4)
#define NOISE_MODE_POWER	(8)
void ist30xx_start(struct ist30xx_data *data)
{
	if (data->initialized) {
		data->scan_count = 0;
		data->scan_retry = 0;
		mod_timer(&event_timer,
			  get_jiffies_64() + EVENT_TIMER_INTERVAL * 2);
	}

	data->ignore_delay = true;

	/* TA mode */
	ist30xx_write_cmd(data, IST30XX_HIB_CMD,
			  ((eHCOM_SET_MODE_SPECIAL << 16) |
			   (data->noise_mode & 0xFFFF)));

	ist30xx_write_cmd(data, IST30XX_HIB_CMD,
			  ((eHCOM_SET_LOCAL_MODEL << 16) |
			   (TSP_LOCAL_CODE & 0xFFFF)));

	if (data->report_rate >= 0) {
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				  ((eHCOM_SET_TIME_ACTIVE << 16) |
				   (data->report_rate & 0xFFFF)));
		tsp_info("%s: active rate : %dus\n", __func__,
			 data->report_rate);
	}

	if (data->idle_rate >= 0) {
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				  ((eHCOM_SET_TIME_IDLE << 16) |
				   (data->idle_rate & 0xFFFF)));
		tsp_info("%s: idle rate : %dus\n", __func__, data->idle_rate);
	}

	if (data->rec_mode) {
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				  (eHCOM_SET_REC_MODE << 16) | (IST30XX_ENABLE &
								0xFFFF));
		tsp_info("%s: rec mode start\n", __func__);
	}

	if (data->debugging_mode) {
		data->debugging_scancnt = 0;
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				  (eHCOM_SET_DBG_MODE << 16) | (IST30XX_ENABLE &
								0xFFFF));
		tsp_info("%s: debugging mode start\n", __func__);
	}

	if (data->debug_mode || data->rec_mode | data->debugging_mode) {
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				  (eHCOM_SLEEP_MODE_EN << 16) | (IST30XX_DISABLE
								 & 0xFFFF));
		tsp_info("%s: debug mode start\n", __func__);
	}

	ist30xx_cmd_start_scan(data);

	data->ignore_delay = false;

	if (data->rec_mode) {
		msleep(100);
		ist30xx_write_cmd(data, IST30XX_HIB_CMD,
				  (eHCOM_SET_REC_MODE << 16) |
				  (IST30XX_START_SCAN & 0xFFFF));
	}

	tsp_info("%s, local : %d, mode : 0x%x\n", __func__,
		 TSP_LOCAL_CODE & 0xFFFF, data->noise_mode & 0xFFFF);
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

	ret = ist30xx_cmd_hold(data, 1);
	if (unlikely(ret))
		return ret;

	ret = ist30xx_read_reg(data->client,
			       IST30XX_DA_ADDR(eHCOM_GET_VER_MAIN),
			       &data->fw.cur.main_ver);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client,
			       IST30XX_DA_ADDR(eHCOM_GET_VER_FW),
			       &data->fw.cur.fw_ver);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client,
			       IST30XX_DA_ADDR(eHCOM_GET_VER_CORE),
			       &data->fw.cur.core_ver);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client,
			       IST30XX_DA_ADDR(eHCOM_GET_VER_TEST),
			       &data->fw.cur.test_ver);
	if (unlikely(ret))
		goto err_get_ver;

#ifdef XIAOMI_PRODUCT
	ret = ist30xx_read_reg(data->client,
			       IST30XX_DA_ADDR(eHCOM_GET_LOCKDOWN_1),
			       &data->fw.cur.lockdown[0]);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client,
			       IST30XX_DA_ADDR(eHCOM_GET_LOCKDOWN_2),
			       &data->fw.cur.lockdown[1]);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client,
			       IST30XX_DA_ADDR(eHCOM_GET_CONFIG_1),
			       &data->fw.cur.config[0]);
	if (unlikely(ret))
		goto err_get_ver;

	ret = ist30xx_read_reg(data->client,
			       IST30XX_DA_ADDR(eHCOM_GET_CONFIG_2),
			       &data->fw.cur.config[1]);
	if (unlikely(ret))
		goto err_get_ver;
#endif

	ret = ist30xx_cmd_hold(data, 0);
	if (unlikely(ret))
		goto err_get_ver;

#ifdef XIAOMI_PRODUCT
	if (data->lockdown_upper != 0)
		data->fw.cur.lockdown[0] = data->lockdown_upper;
#endif

	tsp_info("IC version main: %x, fw: %x, test: %x, core: %x\n",
		 data->fw.cur.main_ver, data->fw.cur.fw_ver,
		 data->fw.cur.test_ver, data->fw.cur.core_ver);

	Lockdown_Info_High.lockdowninfo = data->fw.cur.lockdown[0];
	Lockdown_Info_LOW.lockdowninfo = data->fw.cur.lockdown[1];
	return 0;

err_get_ver:
	ist30xx_reset(data, false);

	return ret;
}

static int ist30xx_set_input_device(struct ist30xx_data *data)
{
	int ret;

	set_bit(EV_ABS, data->input_dev->evbit);
	set_bit(EV_KEY, data->input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, data->input_dev->propbit);

	input_set_abs_params(data->input_dev, ABS_MT_POSITION_X, 0,
			     data->tsp_info.width - 1, 0, 0);
	input_set_abs_params(data->input_dev, ABS_MT_POSITION_Y, 0,
			     data->tsp_info.height - 1, 0, 0);
	input_set_abs_params(data->input_dev, ABS_MT_TOUCH_MAJOR, 0,
			     IST30XX_MAX_W, 0, 0);

#if IST30XX_GESTURE
	input_set_capability(data->input_dev, EV_KEY, KEY_POWER);
	input_set_capability(data->input_dev, EV_KEY, KEY_PLAYPAUSE);
	input_set_capability(data->input_dev, EV_KEY, KEY_NEXTSONG);
	input_set_capability(data->input_dev, EV_KEY, KEY_PREVIOUSSONG);
	input_set_capability(data->input_dev, EV_KEY, KEY_VOLUMEUP);
	input_set_capability(data->input_dev, EV_KEY, KEY_VOLUMEDOWN);
	input_set_capability(data->input_dev, EV_KEY, KEY_MUTE);
	input_set_capability(data->input_dev, EV_KEY, KEY_A);
	input_set_capability(data->input_dev, EV_KEY, KEY_B);
	input_set_capability(data->input_dev, EV_KEY, KEY_C);
	input_set_capability(data->input_dev, EV_KEY, KEY_D);
	input_set_capability(data->input_dev, EV_KEY, KEY_E);
	input_set_capability(data->input_dev, EV_KEY, KEY_F);
	input_set_capability(data->input_dev, EV_KEY, KEY_G);
	input_set_capability(data->input_dev, EV_KEY, KEY_H);
	input_set_capability(data->input_dev, EV_KEY, KEY_I);
	input_set_capability(data->input_dev, EV_KEY, KEY_J);
	input_set_capability(data->input_dev, EV_KEY, KEY_K);
	input_set_capability(data->input_dev, EV_KEY, KEY_L);
	input_set_capability(data->input_dev, EV_KEY, KEY_M);
	input_set_capability(data->input_dev, EV_KEY, KEY_N);
	input_set_capability(data->input_dev, EV_KEY, KEY_O);
	input_set_capability(data->input_dev, EV_KEY, KEY_P);
	input_set_capability(data->input_dev, EV_KEY, KEY_Q);
	input_set_capability(data->input_dev, EV_KEY, KEY_R);
	input_set_capability(data->input_dev, EV_KEY, KEY_S);
	input_set_capability(data->input_dev, EV_KEY, KEY_T);
	input_set_capability(data->input_dev, EV_KEY, KEY_U);
	input_set_capability(data->input_dev, EV_KEY, KEY_V);
	input_set_capability(data->input_dev, EV_KEY, KEY_W);
	input_set_capability(data->input_dev, EV_KEY, KEY_X);
	input_set_capability(data->input_dev, EV_KEY, KEY_Y);
	input_set_capability(data->input_dev, EV_KEY, KEY_Z);
	input_set_capability(data->input_dev, EV_KEY, KEY_KP1);
	input_set_capability(data->input_dev, EV_KEY, KEY_KP2);
	input_set_capability(data->input_dev, EV_KEY, KEY_KP3);
	input_set_capability(data->input_dev, EV_KEY, KEY_KP4);
	input_set_capability(data->input_dev, EV_KEY, KEY_KP5);
	input_set_capability(data->input_dev, EV_KEY, KEY_KP6);
	input_set_capability(data->input_dev, EV_KEY, KEY_KP7);
	input_set_capability(data->input_dev, EV_KEY, KEY_KP8);
	input_set_capability(data->input_dev, EV_KEY, KEY_KP9);
	tsp_info("regist gesture key!\n");
#endif
#if IST30XX_SURFACE_TOUCH
	input_set_capability(data->input_dev, EV_KEY, KEY_MUTE);
#endif
#if IST30XX_BLADE_TOUCH
	input_set_capability(data->input_dev, EV_KEY, KEY_SYSRQ);
#endif

	input_set_drvdata(data->input_dev, data);
	ret = input_register_device(data->input_dev);

	tsp_err("%s: input register device:%d\n", __func__, ret);

	return ret;
}

#define CALIB_MSG_MASK		(0xF0000FFF)
#define CALIB_MSG_VALID		(0x80000CAB)
#define TRACKING_INTR_VALID	(0x127EA597)
u32 tracking_intr_value = TRACKING_INTR_VALID;
int ist30xx_get_info(struct ist30xx_data *data)
{
	int ret;
	u32 calib_msg;
	u32 ms;

	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);

#if IST30XX_INTERNAL_BIN
#if IST30XX_UPDATE_BY_WORKQUEUE
	ist30xx_get_update_info(data, data->fw.buf, data->fw.buf_size);
#endif
	ist30xx_get_tsp_info(data);
#else
	ret = ist30xx_get_ver_info(data);
	if (unlikely(ret))
		goto err_get_info;

	ret = ist30xx_get_tsp_info(data);
	if (unlikely(ret))
		goto err_get_info;
#endif /* IST30XX_INTERNAL_BIN */

	ret = ist30xx_set_input_device(data);
	if (unlikely(ret))
		goto err_get_info;

	ret = ist30xx_read_cmd(data, eHCOM_GET_CAL_RESULT, &calib_msg);
	if (likely(ret == 0)) {
		ms = get_milli_second();
		ist30xx_put_track_ms(ms);
		ist30xx_put_track(&tracking_intr_value, 1);
		ist30xx_put_track(&calib_msg, 1);
		if ((calib_msg & CALIB_MSG_MASK) != CALIB_MSG_VALID ||
		    CALIB_TO_STATUS(calib_msg) > 0) {
			ist30xx_calibrate(data, IST30XX_MAX_RETRY_CNT);
		}
	}
#if IST30XX_CHECK_CALIB
	if (likely(!data->status.update)) {
		ret = ist30xx_cmd_check_calib(data);
		if (likely(!ret)) {
			data->status.calib = 1;
			data->status.calib_msg = 0;
			event_ms = (u32) get_milli_second();
			data->status.event_mode = true;
		}
	}
#else
	ist30xx_start(data);
#endif

	ist30xx_enable_irq(data);
	mutex_unlock(&ist30xx_mutex);

	return 0;

err_get_info:
	mutex_unlock(&ist30xx_mutex);

	return ret;
}

#if (IST30XX_GESTURE || IST30XX_SURFACE_TOUCH || IST30XX_BLADE_TOUCH)
#define SPECIAL_MAGIC_STRING	(0x4170CF00)
#define SPECIAL_MAGIC_MASK		(0xFFFFFF00)
#define SPECIAL_MESSAGE_MASK	(~SPECIAL_MAGIC_MASK)
#define PARSE_SPECIAL_MESSAGE(n)  \
	((n & SPECIAL_MAGIC_MASK) == SPECIAL_MAGIC_STRING ? \
	 (n & SPECIAL_MESSAGE_MASK) : -EINVAL)
#define MAX_LK_KEYCODE_NUM		(256)
static unsigned short lk_keycode[MAX_LK_KEYCODE_NUM] = {
	[0x01] = KEY_POWER,
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
#if IST30XX_GESTURE
	if (((cmd > 0x40) && (cmd < 0x5B)) || ((cmd > 0x90) && (cmd < 0x9A))) {
		tsp_info("Gesture touch: 0x%02X\n", cmd);

		if ((cmd != 0x92) && (cmd != 0x94) && (cmd != 0x96)
		    && (cmd != 0x98)) {
			input_report_key(data->input_dev, KEY_POWER, 1);
			input_sync(data->input_dev);
			input_report_key(data->input_dev, KEY_POWER, 0);
			input_sync(data->input_dev);

			msleep(500);
		}

		input_report_key(data->input_dev, lk_keycode[cmd], 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, lk_keycode[cmd], 0);
		input_sync(data->input_dev);
		return;
	}
#endif
#if IST30XX_SURFACE_TOUCH
	if (cmd == 0x21) {
		tsp_info("Surface touch: 0x%02X\n", cmd);
		input_report_key(data->input_dev, lk_keycode[cmd], 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, lk_keycode[cmd], 0);
		input_sync(data->input_dev);
		return;
	}
#endif
#if IST30XX_BLADE_TOUCH
	if ((cmd >= 0x11) && (cmd <= 0x14)) {
		tsp_info("Blade touch: 0x%02X\n", cmd);
		input_report_key(data->input_dev, lk_keycode[cmd], 1);
		input_sync(data->input_dev);
		input_report_key(data->input_dev, lk_keycode[cmd], 0);
		input_sync(data->input_dev);
		return;
	}
#endif

	tsp_warn("Not support gesture cmd: 0x%02X\n", cmd);
}
#endif

#define PRESS_MSG_MASK		(0x01)
#define MULTI_MSG_MASK		(0x02)
#define TOUCH_DOWN_MESSAGE	("p")
#define TOUCH_UP_MESSAGE	("r")
#define TOUCH_MOVE_MESSAGE	(" ")
bool tsp_touched[IST30XX_MAX_MT_FINGERS] = { false, };

void print_tsp_event(struct ist30xx_data *data, int id, finger_info *finger)
{
	int idx = id - 1;
	bool press;

	press = PRESSED_FINGER(data->t_status, id);

	if (press) {
		if (tsp_touched[idx] == false) {
			/* touch down */
			tsp_noti("%s%d (%d, %d)\n", TOUCH_DOWN_MESSAGE, id,
				 finger->bit_field.x, finger->bit_field.y);
			tsp_touched[idx] = true;
		} else {
			/* touch move */
			tsp_debug("%s%d (%d, %d)\n", TOUCH_MOVE_MESSAGE, id,
				  finger->bit_field.x, finger->bit_field.y);
		}
	} else {
		if (tsp_touched[idx] == true) {
			/* touch up */
			tsp_noti("%s%d\n", TOUCH_UP_MESSAGE, id);
			tsp_touched[idx] = false;
		}
	}
}

#if IST30XX_USE_KEY
#define PRESS_MSG_KEY           (0x06)
bool tkey_pressed[IST30XX_MAX_KEYS] = { false, };

void print_tkey_event(struct ist30xx_data *data, int id)
{
	int idx = id - 1;
	bool press = PRESSED_KEY(data->t_status, id);

	if (press) {
		if (tkey_pressed[idx] == false) {
			/* tkey down */
			tsp_noti("k %s%d\n", TOUCH_DOWN_MESSAGE, id);
			tkey_pressed[idx] = true;
		}
	} else {
		if (tkey_pressed[idx] == true) {
			/* tkey up */
			tsp_noti("k %s%d\n", TOUCH_UP_MESSAGE, id);
			tkey_pressed[idx] = false;
		}
	}
}
#endif
static void release_finger(struct ist30xx_data *data, int id)
{
	input_mt_slot(data->input_dev, id - 1);
	input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);

	ist30xx_tracking(TRACK_POS_FINGER + id);
	tsp_info("%s() %d\n", __func__, id);

	tsp_touched[id - 1] = false;

	if (data->debugging_mode && (id < 3))
		data->t_frame[id - 1] = 0;

	input_sync(data->input_dev);
}

#if IST30XX_USE_KEY
#define CANCEL_KEY  (0xFF)
#define RELEASE_KEY (0)
static void release_key(struct ist30xx_data *data, int id)
{
	input_mt_slot(data->input_dev, 0);
	input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
	ist30xx_tracking(TRACK_POS_KEY + id);

	tkey_pressed[id - 1] = false;

	input_sync(data->input_dev);
}
#endif
static void clear_input_data(struct ist30xx_data *data)
{
	int id = 1;
	u32 status;

	status = PARSE_FINGER_STATUS(data->t_status);
	while (status) {
		if (status & 1)
			release_finger(data, id);
		status >>= 1;
		id++;
	}
#if IST30XX_USE_KEY
	id = 1;
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
	u8 *buf = (u8 *) msg;
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
	bool finger_press = false, key_press = false;
	finger_info *fingers = (finger_info *) data->fingers;
	int idx = 0;
	u32 finger_status, key_status;

	memset(data->t_frame, 0, sizeof(data->t_frame));

	finger_status = PARSE_FINGER_STATUS(data->t_status);
	key_status = PARSE_KEY_STATUS(data->t_status);
	for (id = 0; id < IST30XX_MAX_MT_FINGERS; id++) {
		finger_press = (finger_status & (1 << id)) ? true : false;
		if (id <= 2)
			key_press = (key_status & (1 << id)) ? true : false;
		else
			key_press = false;

		if ((finger_press == false) && (key_press == false)) {
			input_mt_slot(data->input_dev, id);
			input_mt_report_slot_state(data->input_dev,
						   MT_TOOL_FINGER, false);
			continue;
		} else {
			if (finger_press == true) {
				if (data->debugging_mode && (id < 2))
					data->t_frame[id] =
					    fingers[idx].full_field;

				tsp_info("finger press id: %d ( %d, %d)\n", id,
					 fingers[idx].bit_field.x,
					 fingers[idx].bit_field.y);
				input_mt_slot(data->input_dev, id);
				input_mt_report_slot_state(data->input_dev,
							   MT_TOOL_FINGER,
							   true);
				input_report_abs(data->input_dev,
						 ABS_MT_POSITION_X,
						 fingers[idx].bit_field.x);
				input_report_abs(data->input_dev,
						 ABS_MT_POSITION_Y,
						 fingers[idx].bit_field.y);
				input_report_abs(data->input_dev,
						 ABS_MT_TOUCH_MAJOR,
						 fingers[idx].bit_field.w);
				idx++;
			}
			if (key_press == true) {
				tsp_info("key press ( %d, %d)\n",
					 fhd_key_dim_x[id + 1], FHD_KEY_Y);
				input_mt_slot(data->input_dev, 0);
				input_mt_report_slot_state(data->input_dev,
							   MT_TOOL_FINGER,
							   true);
				input_report_abs(data->input_dev,
						 ABS_MT_POSITION_X,
						 fhd_key_dim_x[id + 1]);
				input_report_abs(data->input_dev,
						 ABS_MT_POSITION_Y, FHD_KEY_Y);
				input_report_abs(data->input_dev,
						 ABS_MT_TOUCH_MAJOR,
						 IST30XX_MAX_W);
			}
		}
	}


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

	tsp_verb("%s() \n", __func__);

	ist30xx_delay(data->rec_delay);

	buf32 = kzalloc(IST30XX_NODE_TOTAL_NUM * sizeof(u32), GFP_KERNEL);
	buf = kzalloc(IST30XX_NODE_TOTAL_NUM * 20, GFP_KERNEL);

	for (i = data->rec_start_ch.tx; i <= data->rec_stop_ch.tx; i++) {
		addr =
		    ((i * tsp->ch_num.rx +
		      data->rec_start_ch.rx) * IST30XX_DATA_LEN)
		    + IST30XX_DA_ADDR(data->rec_addr);
		len = data->rec_stop_ch.rx - data->rec_start_ch.rx + 1;
		ret = ist30xx_burst_read(data->client, addr,
					 buf32 + (i * tsp->ch_num.rx +
						  data->rec_start_ch.rx), len,
					 true);
		if (ret)
			goto err_rec_fail;
	}

	for (i = 0; i < IST30XX_NODE_TOTAL_NUM; i++) {
		count += snprintf(msg, msg_len, "%08x ", buf32[i]);
		strncat(buf, msg, msg_len);
	}

	for (i = 0; i < IST30XX_NODE_TOTAL_NUM; i++) {
		count += snprintf(msg, msg_len, "%08x ", 0);
		strncat(buf, msg, msg_len);
	}

	count += snprintf(msg, msg_len, "\n");
	strncat(buf, msg, msg_len);

	old_fs = get_fs();
	set_fs(get_ds());

	tsp_verb("buf[5] = %c, count = %d\n", buf[5], count);
	tsp_verb("filepath=%s\n", data->rec_file_name);

	fp = filp_open(data->rec_file_name, O_CREAT | O_WRONLY | O_APPEND, 0);
	if (IS_ERR(fp)) {
		tsp_err("file %s open error:%d\n", data->rec_file_name,
			PTR_ERR(fp));
		goto err_file_open;
	} else {
		tsp_verb("File Open Success\n");
	}

	ret = fp->f_op->write(fp, buf, count, &fp->f_pos);
	fput(fp);

	tsp_verb("size=%d, pos=%d\n", ret, fp->f_pos);

	filp_close(fp, NULL);

	tsp_verb("%s() success\n", __func__);

err_file_open:
	set_fs(old_fs);

err_rec_fail:
	kfree(buf32);
	kfree(buf);

state_idle:
	data->ignore_delay = true;
	ist30xx_write_cmd(data, IST30XX_HIB_CMD,
			  (eHCOM_SET_REC_MODE << 16) | (IST30XX_START_SCAN &
							0xFFFF));
	data->ignore_delay = false;
}

/*
 * CMD : CMD_GET_COORD
 *
 *   1st  [31:24]   [23:21]   [20:16]   [15:12]   [11:10]   [9:0]
 *        Checksum  KeyCnt    KeyStatus FingerCnt Rsvd.     FingerStatus
 *   2nd  [31:28]   [27:24]   [23:12]   [11:0]
 *        ID        Area      X         Y
 */
#define TRACKING_INTR_DEBUG1_VALID	(0x127A6E81)
#define TRACKING_INTR_DEBUG2_VALID	(0x127A6E82)
#define TRACKING_INTR_DEBUG3_VALID	(0x127A6E83)
u32 tracking_intr_debug_value = 0;
u32 intr_debug_addr, intr_debug2_addr, intr_debug3_addr = 0;
u32 intr_debug_size, intr_debug2_size, intr_debug3_size = 0;
static irqreturn_t ist30xx_irq_thread(int irq, void *ptr)
{
	int i, ret;
	int key_cnt, finger_cnt, read_cnt;
	struct ist30xx_data *data = (struct ist30xx_data *)ptr;
	int offset = 1;
#if IST30XX_STATUS_DEBUG
	u32 touch_status;
#endif
	u32 t_status;
	u32 msg[IST30XX_MAX_MT_FINGERS * 2 + offset];
	u32 ms;
	u32 debugBuf32[IST30XX_MAX_DEBUGINFO / IST30XX_DATA_LEN];
	u32 touch[2];
	bool idle = false;

	data->irq_working = true;

	if (unlikely(!data->irq_enabled))
		goto irq_ignore;

#if CTP_CHARGER_DETECT
	if (!batt_psy) {
		batt_psy = power_supply_get_by_name("usb");
	} else {
		is_charger_plug =
		    (u8) power_supply_get_battery_charge_state(batt_psy);

		if (is_charger_plug != pre_charger_status) {
			pre_charger_status = is_charger_plug;
			ist30xx_set_ta_mode(is_charger_plug);
		}
	}

#endif

	ms = get_milli_second();

	if (data->debugging_mode) {
		ist30xx_burst_read(data->client,
				   IST30XX_DA_ADDR(data->debugging_addr),
				   debugBuf32,
				   data->debugging_size / IST30XX_DATA_LEN,
				   true);
	}

	if (intr_debug_size > 0) {
		tsp_debug("Intr_debug (addr: 0x%08x)\n", intr_debug_addr);
		ist30xx_burst_read(data->client,
				   IST30XX_DA_ADDR(intr_debug_addr), &msg[0],
				   intr_debug_size, true);

		for (i = 0; i < intr_debug_size; i++)
			tsp_debug("\t%08x\n", msg[i]);
		tracking_intr_debug_value = TRACKING_INTR_DEBUG1_VALID;
		ist30xx_put_track_ms(ms);
		ist30xx_put_track(&tracking_intr_debug_value, 1);
		ist30xx_put_track(msg, intr_debug_size);
	}

	if (intr_debug2_size > 0) {
		tsp_debug("Intr_debug2 (addr: 0x%08x)\n", intr_debug2_addr);
		ist30xx_burst_read(data->client,
				   IST30XX_DA_ADDR(intr_debug2_addr), &msg[0],
				   intr_debug2_size, true);

		for (i = 0; i < intr_debug2_size; i++)
			tsp_debug("\t%08x\n", msg[i]);
		tracking_intr_debug_value = TRACKING_INTR_DEBUG2_VALID;
		ist30xx_put_track_ms(ms);
		ist30xx_put_track(&tracking_intr_debug_value, 1);
		ist30xx_put_track(msg, intr_debug2_size);
	}

	memset(msg, 0, sizeof(msg));

	ret = ist30xx_read_reg(data->client, IST30XX_HIB_INTR_MSG, msg);
	if (unlikely(ret))
		goto irq_err;

	tsp_verb("intr msg: 0x%08x\n", *msg);

	/* TSP End Initial */
	if (unlikely(*msg == IST30XX_INITIAL_VALUE)) {
		tsp_debug("End Initial\n");
		goto irq_event;
	}

	/* TSP Recording */
	if (unlikely(*msg == IST30XX_REC_VALUE)) {
		idle = true;
		goto irq_end;
	}

	/* TSP Debugging */
	if (unlikely(*msg == IST30XX_DEBUGGING_VALUE))
		goto irq_end;

	/* TSP IC Exception */
	if (unlikely((*msg & IST30XX_EXCEPT_MASK) == IST30XX_EXCEPT_VALUE)) {
		tsp_err("Occured IC exception(0x%02X)\n", *msg & 0xFF);
#if I2C_BURST_MODE
		ret = ist30xx_burst_read(data->client,
					 IST30XX_HIB_COORD, &msg[offset],
					 IST30XX_MAX_EXCEPT_SIZE, true);
#else
		for (i = 0; i < IST30XX_MAX_EXCEPT_SIZE; i++) {
			ret = ist30xx_read_reg(data->client,
					       IST30XX_HIB_COORD + (i * 4),
					       &msg[i + offset]);
		}
#endif
		if (unlikely(ret))
			tsp_err(" exception value read error(%d)\n", ret);
		else
			tsp_err(" exception value : 0x%08X, 0x%08X\n", msg[1],
				msg[2]);

		goto irq_ic_err;
	}

	if (unlikely(*msg == 0 || *msg == 0xFFFFFFFF))	/* Unknown CMD */
		goto irq_err;

	event_ms = ms;
	ist30xx_put_track_ms(event_ms);
	ist30xx_put_track(&tracking_intr_value, 1);
	ist30xx_put_track(msg, 1);

	if (unlikely((*msg & CALIB_MSG_MASK) == CALIB_MSG_VALID)) {
		data->status.calib_msg = *msg;
		tsp_info("calib status: 0x%08x\n", data->status.calib_msg);

		goto irq_event;
	}
#if IST30XX_CMCS_TEST
	if (((*msg & CMCS_MSG_MASK) == CM_MSG_VALID) ||
	    ((*msg & CMCS_MSG_MASK) == CM2_MSG_VALID) ||
	    ((*msg & CMCS_MSG_MASK) == CS_MSG_VALID) ||
	    ((*msg & CMCS_MSG_MASK) == CMJIT_MSG_VALID) ||
	    ((*msg & CMCS_MSG_MASK) == INT_MSG_VALID)) {
		data->status.cmcs = *msg;
		tsp_info("cmcs status: 0x%08x\n", *msg);

		goto irq_event;
	}
#endif

#if (IST30XX_GESTURE || IST30XX_SURFACE_TOUCH || IST30XX_BLADE_TOUCH)
	ret = PARSE_SPECIAL_MESSAGE(*msg);
	if (unlikely(ret > 0)) {
		tsp_verb("special cmd: %d (0x%08x)\n", ret, *msg);
		ist30xx_special_cmd(data, ret);

		goto irq_event;
	}
#endif

#if IST30XX_STATUS_DEBUG
	ret = ist30xx_read_reg(data->client,
			       IST30XX_HIB_TOUCH_STATUS, &touch_status);

	if (ret == 0)
		tsp_debug("ALG : 0x%08X\n", touch_status);
#endif

	memset(data->fingers, 0, sizeof(data->fingers));

	/* Unknown interrupt data for extend coordinate */
	if (unlikely(!CHECK_INTR_STATUS(*msg)))
		goto irq_err;

	t_status = *msg;
	key_cnt = PARSE_KEY_CNT(t_status);
	finger_cnt = PARSE_FINGER_CNT(t_status);

	if (unlikely((finger_cnt > data->max_fingers) ||
		     (key_cnt > data->max_keys))) {
		tsp_warn("Invalid touch count - finger: %d(%d), key: %d(%d)\n",
			 finger_cnt, data->max_fingers, key_cnt,
			 data->max_keys);
		goto irq_err;
	}

	if (finger_cnt > 0) {
#if I2C_BURST_MODE
		ret = ist30xx_burst_read(data->client,
					 IST30XX_HIB_COORD, &msg[offset],
					 finger_cnt, true);
		if (unlikely(ret))
			goto irq_err;

		for (i = 0; i < finger_cnt; i++)
			data->fingers[i].full_field = msg[i + offset];
#else
		for (i = 0; i < finger_cnt; i++) {
			ret = ist30xx_read_reg(data->client,
					       IST30XX_HIB_COORD + (i * 4),
					       &msg[i + offset]);
			if (unlikely(ret))
				goto irq_err;

			data->fingers[i].full_field = msg[i + offset];
		}
#endif /* I2C_BURST_MODE */

		ist30xx_put_track(msg + offset, finger_cnt);
		for (i = 0; i < finger_cnt; i++) {
			tsp_verb("intr msg(%d): 0x%08x\n", i + offset,
				 msg[i + offset]);
		}
	}

	read_cnt = finger_cnt + 1;

	ret = check_valid_coord(&msg[0], read_cnt);
	if (unlikely(ret))
		goto irq_err;

	data->t_status = t_status;
	report_input_data(data, finger_cnt, key_cnt);

	if (intr_debug3_size > 0) {
		tsp_debug("Intr_debug3 (addr: 0x%08x)\n", intr_debug3_addr);
		ist30xx_burst_read(data->client,
				   IST30XX_DA_ADDR(intr_debug3_addr), &msg[0],
				   intr_debug3_size, true);

		for (i = 0; i < intr_debug3_size; i++)
			tsp_debug("\t%08x\n", msg[i]);

		tracking_intr_debug_value = TRACKING_INTR_DEBUG3_VALID;
		ist30xx_put_track_ms(ms);
		ist30xx_put_track(&tracking_intr_debug_value, 1);
		ist30xx_put_track(msg, intr_debug3_size);
	}

	goto irq_end;

irq_err:
	tsp_err("intr msg: 0x%08x, ret: %d\n", msg[0], ret);
#if IST30XX_GESTURE
	if (!data->suspend)
#endif
		ist30xx_request_reset(data);
	goto irq_event;
irq_end:
	if (data->rec_mode)
		recording_data(data, idle);
	if (data->debugging_mode) {
		if ((data->debugging_scancnt == debugBuf32[0])
		    && data->debugging_noise)
			goto irq_event;

		memset(touch, 0, sizeof(touch));
		touch[0] = data->t_frame[0];
		touch[1] = data->t_frame[1];

		ist30xx_put_frame(data, ms, touch, debugBuf32,
				  data->debugging_size / IST30XX_DATA_LEN);
		data->debugging_scancnt = debugBuf32[0];
	}
irq_event:
irq_ignore:
	data->irq_working = false;
	event_ms = (u32) get_milli_second();
	if (data->initialized)
		mod_timer(&event_timer,
			  get_jiffies_64() + EVENT_TIMER_INTERVAL);
	return IRQ_HANDLED;

irq_ic_err:
	ist30xx_scheduled_reset(data);
	data->irq_working = false;
	event_ms = (u32) get_milli_second();
	if (data->initialized)
		mod_timer(&event_timer,
			  get_jiffies_64() + EVENT_TIMER_INTERVAL);
	return IRQ_HANDLED;
}

#ifdef CONFIG_PM
static int ist30xx_suspend(struct device *dev)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	if (data->debugging_mode)
		return 0;

	del_timer(&event_timer);
	cancel_delayed_work_sync(&data->work_noise_protect);
	cancel_delayed_work_sync(&data->work_reset_check);
	cancel_delayed_work_sync(&data->work_debug_algorithm);
#if CTP_CHARGER_DETECT
	cancel_delayed_work_sync(&data->work_charger_check);
#endif
	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);
	ist30xx_internal_suspend(data);
	clear_input_data(data);
#if IST30XX_GESTURE
	if (data->gesture) {
		data->status.noise_mode = false;
		ist30xx_start(data);
		ist30xx_enable_irq(data);
	}
#endif
	mutex_unlock(&ist30xx_mutex);

	return 0;
}

static int ist30xx_resume(struct device *dev)
{
	struct ist30xx_data *data = dev_get_drvdata(dev);

	data->noise_mode |= (1 << NOISE_MODE_POWER);

	if (data->debugging_mode && data->status.power)
		return 0;

	mutex_lock(&ist30xx_mutex);
	ist30xx_internal_resume(data);

#if CTP_CHARGER_DETECT
	batt_psy = power_supply_get_by_name("usb");
	if (!batt_psy) {
		tsp_info("tp resume battery supply not found\n");
	} else {
		is_charger_plug =
		    (u8) power_supply_get_battery_charge_state(batt_psy);
		if (is_charger_plug) {
			ist30xx_set_ta_mode(1);
		} else {
			ist30xx_set_ta_mode(0);
		}
		pre_charger_status = is_charger_plug;
	}
#endif
	ist30xx_start(data);
	ist30xx_enable_irq(data);
	mutex_unlock(&ist30xx_mutex);

#if CTP_CHARGER_DETECT
	schedule_delayed_work(&data->work_charger_check,
			      CHECK_CHARGER_INTERVAL);
#endif

	return 0;
}

static const struct dev_pm_ops ist30xx_pm_ops = {
#if (!defined(CONFIG_FB) && !defined(CONFIG_HAS_EARLYSUSPEND))
	.suspend = ist30xx_suspend,
	.resume = ist30xx_resume,
#endif
};

#else
static int ist30xx_suspend(struct device *dev)
{
	return 0;
}

static int ist30xx_resume(struct device *dev)
{
	return 0;
}

#endif

#if defined(CONFIG_FB)
static void fb_notify_resume_works(struct work_struct *work)
{
	struct ist30xx_data *data =
	    container_of(work, struct ist30xx_data, fb_notify_work);

	printk("fb_notify_resume_works\n");
	ist30xx_resume(&data->client->dev);
}

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
			schedule_work(&ist_data->fb_notify_work);
		else if (*blank == FB_BLANK_POWERDOWN) {
			flush_work(&ist_data->fb_notify_work);
			ist30xx_suspend(&ist_data->client->dev);
		}
	}
	return 0;
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
	tsp_info("%s(), mode = %d\n", __func__, mode);

	if (unlikely(mode == ((ts_data->noise_mode >> NOISE_MODE_TA) & 1)))
		return;

	if (mode)
		ts_data->noise_mode |= (1 << NOISE_MODE_TA);
	else
		ts_data->noise_mode &= ~(1 << NOISE_MODE_TA);

	if (ts_data->secure_mode)
		return;

#if IST30XX_TA_RESET
	if (unlikely(ts_data->initialized))
		ist30xx_scheduled_reset(ts_data);
#else
	ist30xx_write_cmd(ts_data, IST30XX_HIB_CMD,
			  ((eHCOM_SET_MODE_SPECIAL << 16) |
			   (ts_data->noise_mode & 0xFFFF)));
#endif

	ist30xx_tracking(mode ? TRACK_CMD_TACON : TRACK_CMD_TADISCON);
}

EXPORT_SYMBOL(ist30xx_set_ta_mode);

void ist30xx_set_secure_mode(int mode)
{
	tsp_info("%s(), mode = %d\n", __func__, mode);

	if (unlikely(mode == ((ts_data->noise_mode >> NOISE_MODE_SECURE) & 1)))
		return;

	if (mode)
		ts_data->noise_mode |= (1 << NOISE_MODE_SECURE);
	else
		ts_data->noise_mode &= ~(1 << NOISE_MODE_SECURE);

	ts_data->secure_mode = mode;

#if IST30XX_TA_RESET
	if (unlikely(ts_data->initialized))
		ist30xx_scheduled_reset(ts_data);
#else
	ist30xx_write_cmd(ts_data, IST30XX_HIB_CMD,
			  ((eHCOM_SET_MODE_SPECIAL << 16) |
			   (ts_data->noise_mode & 0xFFFF)));
#endif
}

EXPORT_SYMBOL(ist30xx_set_secure_mode);

void ist30xx_set_edge_mode(int mode)
{
	tsp_info("%s(), mode = %d\n", __func__, mode);

	if (mode)
		ts_data->noise_mode |= (1 << NOISE_MODE_EDGE);
	else
		ts_data->noise_mode &= ~(1 << NOISE_MODE_EDGE);

	ist30xx_write_cmd(ts_data, IST30XX_HIB_CMD,
			  ((eHCOM_SET_MODE_SPECIAL << 16) |
			   (ts_data->noise_mode & 0xFFFF)));
}

EXPORT_SYMBOL(ist30xx_set_edge_mode);

void ist30xx_set_call_mode(int mode)
{
	tsp_info("%s(), mode = %d\n", __func__, mode);

	if (unlikely(mode == ((ts_data->noise_mode >> NOISE_MODE_CALL) & 1)))
		return;

	if (mode)
		ts_data->noise_mode |= (1 << NOISE_MODE_CALL);
	else
		ts_data->noise_mode &= ~(1 << NOISE_MODE_CALL);

#if IST30XX_TA_RESET
	if (unlikely(ts_data->initialized))
		ist30xx_scheduled_reset(ts_data);
#else
	ist30xx_write_cmd(ts_data, IST30XX_HIB_CMD,
			  ((eHCOM_SET_MODE_SPECIAL << 16) |
			   (ts_data->noise_mode & 0xFFFF)));
#endif

	ist30xx_tracking(mode ? TRACK_CMD_CALL : TRACK_CMD_NOTCALL);
}

EXPORT_SYMBOL(ist30xx_set_call_mode);

void ist30xx_set_cover_mode(int mode)
{
	tsp_info("%s(), mode = %d\n", __func__, mode);

	if (unlikely(mode == ((ts_data->noise_mode >> NOISE_MODE_COVER) & 1)))
		return;

	if (mode)
		ts_data->noise_mode |= (1 << NOISE_MODE_COVER);
	else
		ts_data->noise_mode &= ~(1 << NOISE_MODE_COVER);

#if IST30XX_TA_RESET
	if (unlikely(ts_data->initialized))
		ist30xx_scheduled_reset(ts_data);
#else
	ist30xx_write_cmd(ts_data, IST30XX_HIB_CMD,
			  ((eHCOM_SET_MODE_SPECIAL << 16) |
			   (ts_data->noise_mode & 0xFFFF)));
#endif

	ist30xx_tracking(mode ? TRACK_CMD_COVER : TRACK_CMD_NOTCOVER);
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

#if CTP_CHARGER_DETECT
static void charger_work_func(struct work_struct *work)
{
	if (!batt_psy) {
		batt_psy = power_supply_get_by_name("usb");
	} else {
		is_charger_plug =
		    (u8) power_supply_get_battery_charge_state(batt_psy);

		if (is_charger_plug != pre_charger_status) {
			pre_charger_status = is_charger_plug;
			ist30xx_set_ta_mode(is_charger_plug);
		}
	}

	schedule_delayed_work(&ts_data->work_charger_check,
			      CHECK_CHARGER_INTERVAL);
}
#endif

static void reset_work_func(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work,
						 struct ist30xx_data,
						 work_reset_check);

	if (unlikely((data == NULL) || (data->client == NULL)))
		return;

	tsp_info("Request reset function\n");

	if (likely((data->initialized == 1) && (data->status.power == 1) &&
		   (data->status.update != 1) && (data->status.calib != 1))) {
		mutex_lock(&ist30xx_mutex);
		ist30xx_disable_irq(data);
#if IST30XX_GESTURE
		if (data->suspend)
			ist30xx_internal_suspend(data);
		else
#endif
			ist30xx_reset(data, false);
		clear_input_data(data);
#if IST30XX_GESTURE
		if (data->gesture && data->suspend)
			data->status.noise_mode = false;
#endif
		ist30xx_start(data);
		ist30xx_enable_irq(data);
		mutex_unlock(&ist30xx_mutex);
	}
}

#if IST30XX_INTERNAL_BIN
#if IST30XX_UPDATE_BY_WORKQUEUE
static void fw_update_func(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work,
						 struct ist30xx_data,
						 work_fw_update);

	if (unlikely((data == NULL) || (data->client == NULL)))
		return;

	tsp_info("FW update function\n");

	if (likely(ist30xx_auto_bin_update(data)))
		ist30xx_disable_irq(data);
}
#endif /* IST30XX_UPDATE_BY_WORKQUEUE */
#endif /* IST30XX_INTERNAL_BIN */

#define TOUCH_STATUS_MAGIC		(0x00000075)
#define TOUCH_STATUS_MASK		(0x000000FF)
#define FINGER_ENABLE_MASK		(0x00100000)
#define SCAN_CNT_MASK			(0xFFE00000)
#define GET_FINGER_ENABLE(n)	((n & FINGER_ENABLE_MASK) >> 20)
#define GET_SCAN_CNT(n)			((n & SCAN_CNT_MASK) >> 21)
u32 ist30xx_algr_addr = 0, ist30xx_algr_size = 0;
static void noise_work_func(struct work_struct *work)
{
	int ret;
	u32 touch_status = 0;
	u32 scan_count = 0;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work,
						 struct ist30xx_data,
						 work_noise_protect);

	ret = ist30xx_read_reg(data->client,
			       IST30XX_HIB_TOUCH_STATUS, &touch_status);
	if (unlikely(ret)) {
		tsp_warn("Touch status read fail!\n");
		goto retry_timer;
	}

	ist30xx_put_track_ms(timer_ms);
	ist30xx_put_track(&touch_status, 1);

	tsp_verb("Touch Info: 0x%08x\n", touch_status);

	/* Check valid scan count */
	if (unlikely((touch_status & TOUCH_STATUS_MASK) != TOUCH_STATUS_MAGIC)) {
		tsp_warn("Touch status is not corrected! (0x%08x)\n",
			 touch_status);
		goto retry_timer;
	}

	/* Status of IC is idle */
	if (GET_FINGER_ENABLE(touch_status) == 0) {
		if ((PARSE_FINGER_CNT(data->t_status) > 0) ||
		    (PARSE_KEY_CNT(data->t_status) > 0))
			clear_input_data(data);
	}

	scan_count = GET_SCAN_CNT(touch_status);

	/* Status of IC is lock-up */
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

static void debug_work_func(struct work_struct *work)
{
#if IST30XX_ALGORITHM_MODE
	int ret = -EPERM;
	int i;
	u32 *buf32;

	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *data = container_of(delayed_work,
						 struct ist30xx_data,
						 work_debug_algorithm);

	buf32 = kzalloc(ist30xx_algr_size, GFP_KERNEL);

	for (i = 0; i < ist30xx_algr_size; i++) {
		ret = ist30xx_read_buf(data->client,
				       ist30xx_algr_addr + IST30XX_DATA_LEN * i,
				       &buf32[i], 1);
		if (ret) {
			tsp_warn("Algorithm mem addr read fail!\n");
			return;
		}
	}

	ist30xx_put_track(buf32, ist30xx_algr_size);

	tsp_debug(" 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
		  buf32[0], buf32[1], buf32[2], buf32[3], buf32[4]);
#endif
}

void timer_handler(unsigned long timer_data)
{
	struct ist30xx_data *data = (struct ist30xx_data *)timer_data;
	struct ist30xx_status *status = &data->status;

	if (data->irq_working || !data->initialized || data->rec_mode)
		goto restart_timer;

	if (status->event_mode) {
		if (likely((status->power == 1) && (status->update != 1))) {
			timer_ms = (u32) get_milli_second();
			if (unlikely(status->calib == 1)) {
				/* Check calibration */
				if ((status->calib_msg & CALIB_MSG_MASK) ==
				    CALIB_MSG_VALID) {
					tsp_info("Calibration check OK!!\n");
					schedule_delayed_work(&data->
							      work_reset_check,
							      0);
					status->calib = 0;
				} else if (timer_ms - event_ms >= 3000) {
					/* over 3 second */
					tsp_info
					    ("calibration timeout over 3sec\n");
					schedule_delayed_work(&data->
							      work_reset_check,
							      0);
					status->calib = 0;
				}
			} else if (likely(status->noise_mode)) {
				/* 100ms after last interrupt */
				if (timer_ms - event_ms > 100)
					schedule_delayed_work(&data->
							      work_noise_protect,
							      0);
			}
#if IST30XX_ALGORITHM_MODE
			if ((ist30xx_algr_addr >= IST30XX_DIRECT_ACCESS) &&
			    (ist30xx_algr_size > 0)) {
				/* 100ms after last interrupt */
				if (timer_ms - event_ms > 100)
					schedule_delayed_work(&data->
							      work_debug_algorithm,
							      0);
			}
#endif
		}
	}

restart_timer:
	mod_timer(&event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL);
}

static int ist30xx_parse_dt(struct device *dev,
			    struct ist30xx_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;

	pdata->name = "ist30xx";
	rc = of_property_read_string(np, "ist30xx,name", &pdata->name);
	if (rc && (rc != -EINVAL)) {
		dev_err(dev, "Unable to read name\n");
		return rc;
	}

	pdata->reset_gpio = of_get_named_gpio_flags(np, "ist30xx,reset-gpio",
						    0,
						    &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
		return pdata->reset_gpio;

	pdata->irq_gpio = of_get_named_gpio_flags(np, "ist30xx,irq-gpio",
						  0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
		return pdata->irq_gpio;

	return 0;
}

static int ist30xx_ts_pinctrl_init(struct ist30xx_data *ist30xx_data)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	ist30xx_data->ts_pinctrl =
	    devm_pinctrl_get(&(ist30xx_data->client->dev));
	if (IS_ERR_OR_NULL(ist30xx_data->ts_pinctrl)) {
		dev_dbg(&ist30xx_data->client->dev,
			"Target does not use pinctrl\n");
		retval = PTR_ERR(ist30xx_data->ts_pinctrl);
		ist30xx_data->ts_pinctrl = NULL;
		return retval;
	}

	ist30xx_data->gpio_state_active
	    = pinctrl_lookup_state(ist30xx_data->ts_pinctrl, "pmx_ts_active");
	if (IS_ERR_OR_NULL(ist30xx_data->gpio_state_active)) {
		dev_dbg(&ist30xx_data->client->dev,
			"Can not get ts default pinstate\n");
		retval = PTR_ERR(ist30xx_data->gpio_state_active);
		ist30xx_data->ts_pinctrl = NULL;
		return retval;
	}

	ist30xx_data->gpio_state_suspend
	    = pinctrl_lookup_state(ist30xx_data->ts_pinctrl, "pmx_ts_suspend");
	if (IS_ERR_OR_NULL(ist30xx_data->gpio_state_suspend)) {
		dev_err(&ist30xx_data->client->dev,
			"Can not get ts sleep pinstate\n");
		retval = PTR_ERR(ist30xx_data->gpio_state_suspend);
		ist30xx_data->ts_pinctrl = NULL;
		return retval;
	}

	return 0;
}

static int ist30xx_ts_pinctrl_select(struct ist30xx_data *ist30xx_data, bool on)
{
	struct pinctrl_state *pins_state;
	int ret;

	pins_state = on ? ist30xx_data->gpio_state_active
	    : ist30xx_data->gpio_state_suspend;
	if (!IS_ERR_OR_NULL(pins_state)) {
		ret =
		    pinctrl_select_state(ist30xx_data->ts_pinctrl, pins_state);
		if (ret) {
			dev_err(&ist30xx_data->client->dev,
				"can not set %s pins\n",
				on ? "pmx_ts_active" : "pmx_ts_suspend");
			return ret;
		}
	} else {
		dev_err(&ist30xx_data->client->dev,
			"not a valid '%s' pinstate\n",
			on ? "pmx_ts_active" : "pmx_ts_suspend");
	}

	return 0;
}

#if IST30XX_INTERNAL_BIN
static int get_boot_mode(struct i2c_client *client)
{
	int ret;
	char *cmdline_tp = NULL;
	char *temp;
	char cmd_line[15] = { '\0' };

	cmdline_tp = strstr(saved_command_line, "androidboot.mode=");
	if (cmdline_tp != NULL) {
		temp = cmdline_tp + strlen("androidboot.mode=");
		ret = strncmp(temp, "ffbm", strlen("ffbm"));
		memcpy(cmd_line, temp, 10);
		dev_err(&client->dev, "cmd_line =%s \n", cmd_line);
		if (ret == 0) {
			dev_err(&client->dev, "mode: ffbm\n");
			return 1;	/* factory mode */
		} else {
			dev_err(&client->dev, "mode: no ffbm\n");
			return 2;	/* no factory mode */
		}
	}
	dev_err(&client->dev, "has no androidboot.mode \n");
	return 0;
}
#endif

static int ist30xx_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int ret;
	int err;
	int retry = 3;
	u32 info_data[2];
	struct ist30xx_data *data;
	struct ist30xx_platform_data *pdata;
	struct input_dev *input_dev;
#if  WT_ADD_CTP_INFO
	char ic_color[64];
#endif

	tsp_info("### IMAGIS probe(ver:%s, protocol:%X, addr:0x%02X) ###\n",
		 IMAGIS_DD_VERSION, IMAGIS_PROTOCOL_TYPE, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_INFO "i2c_check_functionality error\n");
		return -EIO;
	}

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

#ifdef CONFIG_OF
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				     sizeof(struct ist30xx_platform_data),
				     GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		}

		ret = ist30xx_parse_dt(&client->dev, pdata);
		if (ret) {
			dev_err(&client->dev, "DT parsing failed\n");
			return ret;
		}
	} else
		pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "Invalid pdata\n");
		return -EINVAL;
	}
#endif


	input_dev = input_allocate_device();
	if (unlikely(!input_dev)) {
		tsp_err("input_allocate_device failed\n");
		goto err_alloc_dev;
	}

	data->max_fingers = IST30XX_MAX_MT_FINGERS;
	data->max_keys = IST30XX_MAX_KEYS;
	data->irq_enabled = 1;
	data->client = client;
	data->input_dev = input_dev;
	i2c_set_clientdata(client, data);

	input_mt_init_slots(input_dev, IST30XX_MAX_MT_FINGERS, 0);

	input_dev->name = "ist30xx_ts_input";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	data->pdata = pdata;
	ts_data = data;

	err = ist30xx_ts_pinctrl_init(data);
	if (!err && data->ts_pinctrl) {
		err = ist30xx_ts_pinctrl_select(data, 1);
		if (err < 0)
			tsp_info("here i am\n");
	}

	if (gpio_is_valid(pdata->irq_gpio)) {
		err = gpio_request(pdata->irq_gpio, "ist30xx_irq_gpio");
		if (err) {
			dev_err(&client->dev, "irq gpio request failed");
			goto err_init_drv;
		}
		err = gpio_direction_output(pdata->irq_gpio, 1);
		msleep(10);
		err = gpio_direction_input(pdata->irq_gpio);
		if (err) {
			dev_err(&client->dev,
				"set_direction for irq gpio failed\n");
		}
	} else {
		dev_err(&client->dev, "--------\n");
	}

	if (gpio_is_valid(pdata->reset_gpio)) {
		err = gpio_request(pdata->reset_gpio, "ist30xx_reset_gpio");
		if (err) {
			dev_err(&client->dev, "reset gpio request failed");
			goto err_init_drv;
		}

		err = gpio_direction_output(pdata->reset_gpio, 1);
	} else {
		dev_err(&client->dev, "==========\n");
	}

	/* PID info read */
	retry = IST30XX_MAX_RETRY_CNT;
	while (retry-- > 0) {
		ret = ist30xxc_isp_info_read(data, 0, info_data, 2);
		if (ret == 0) {
			if ((info_data[1] & XIAOMI_INFO_MASK) ==
			    XIAOMI_INFO_VALUE) {
				data->product_id = XIAOMI_GET_PID(info_data[1]);
				data->lockdown_upper = info_data[0];
				break;
			} else if ((info_data[0] & XIAOMI_INFO_MASK) ==
				   XIAOMI_INFO_VALUE) {
				data->product_id = XIAOMI_GET_PID(info_data[0]);
				data->lockdown_upper = 0;
				break;
			}
		}

		if (retry == 0)
			tsp_err("read isp info failed\n");
	}
	tsp_info("TSP PID: %x\n", data->product_id);

	/* FW do not enter sleep mode in probe */
	ist30xx_write_cmd(data, IST30XX_HIB_CMD,
			  (eHCOM_FW_HOLD << 16) | (1 & 0xFFFF));

	retry = IST30XX_MAX_RETRY_CNT;
	while (retry-- > 0) {
		ret =
		    ist30xx_read_reg(data->client, IST30XX_REG_CHIPID,
				     &data->chip_id);
		data->chip_id &= 0xFFFF;
		if (unlikely(ret == 0)) {
			if ((data->chip_id == IST30XX_CHIP_ID)
			    || (data->chip_id == IST30XXC_DEFAULT_CHIP_ID)
			    || (data->chip_id == IST3048C_DEFAULT_CHIP_ID)) {
				break;
			}
		} else if (unlikely(retry == 0)) {
			goto err_init_drv;
		}
	}

#if  defined(CONFIG_FB)
	INIT_WORK(&data->fb_notify_work, fb_notify_resume_works);
	data->fb_notif.notifier_call = fb_notifier_callback;

	err = fb_register_client(&data->fb_notif);
	if (err)
		dev_err(&client->dev, "Unable to register fb_notifier: %d\n",
			err);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = ist30xx_early_suspend;
	data->early_suspend.resume = ist30xx_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

#if (IMAGIS_TSP_IC < IMAGIS_IST3038C)
	retry = IST30XX_MAX_RETRY_CNT;
	data->tsp_type = TSP_TYPE_UNKNOWN;
	while (retry-- > 0) {
		ret = ist30xx_read_reg(data->client, IST30XX_REG_TSPTYPE,
				       &data->tsp_type);
		if (likely(ret == 0)) {
			data->tsp_type = IST30XX_PARSE_TSPTYPE(data->tsp_type);
			tsp_info("tsptype: %x\n", data->tsp_type);
			break;
		}

		if (unlikely(retry == 0))
			goto err_init_drv;
	}

	tsp_info("TSP IC: %x, TSP Vendor: %x\n", data->chip_id, data->tsp_type);
#else
	tsp_info("TSP IC: %x\n", data->chip_id);
#endif

	ret = request_threaded_irq(client->irq, NULL, ist30xx_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				   "ist30xx_ts", data);
	if (unlikely(ret))
		goto err_init_drv;

	ist30xx_disable_irq(data);
	data->status.event_mode = false;

#if IST30XX_INTERNAL_BIN
#if IST30XX_UPDATE_BY_WORKQUEUE
	INIT_DELAYED_WORK(&data->work_fw_update, fw_update_func);
	schedule_delayed_work(&data->work_fw_update, IST30XX_UPDATE_DELAY);
#else
	if (data->product_id != 0) {
		err = get_boot_mode(client);
		dev_err(&client->dev, "wdb: err: %d\n", err);
		if (err == 0) {
			dev_err(&client->dev, "wdb: start upgrade,  ret: %d\n",
				ret);
			ret = ist30xx_auto_bin_update(data);
			if (unlikely(ret != 0))
				goto err_irq;
		} else {
			dev_err(&client->dev, "wdb: no upgrade \n");
		}
	}
	{
		ist30xx_get_ver_info(data);
	}
#endif /* IST30XX_UPDATE_BY_WORKQUEUE */
#endif /* IST30XX_INTERNAL_BIN */

	ret = ist30xx_init_update_sysfs(data);
	if (unlikely(ret))
		goto err_sysfs;

#if IST30XX_DEBUG
	ret = ist30xx_init_misc_sysfs(data);
	if (unlikely(ret))
		goto err_sysfs;
#endif

#if IST30XX_CMCS_TEST
	ret = ist30xx_init_cmcs_sysfs(data);
	if (unlikely(ret))
		goto err_sysfs;
#endif

#if IST30XX_TRACKING_MODE
	ret = ist30xx_init_tracking_sysfs(data);
	if (unlikely(ret))
		goto err_sysfs;
#endif

	/* initialize data variable */
#if IST30XX_GESTURE
	data->suspend = false;
	data->gesture = false;
#endif
	data->ignore_delay = false;
	data->secure_mode = false;
	data->rec_mode = 0;
	data->debugging_mode = 0;
	data->irq_working = false;
	data->max_scan_retry = 2;
	data->max_irq_err_cnt = MAX_ERR_CNT;
	data->report_rate = -1;
	data->idle_rate = -1;
#if CTP_CHARGER_DETECT
	INIT_DELAYED_WORK(&data->work_charger_check, charger_work_func);
#endif
	INIT_DELAYED_WORK(&data->work_reset_check, reset_work_func);
	INIT_DELAYED_WORK(&data->work_noise_protect, noise_work_func);
	INIT_DELAYED_WORK(&data->work_debug_algorithm, debug_work_func);

	init_timer(&event_timer);
	event_timer.data = (unsigned long)data;
	event_timer.function = timer_handler;
	event_timer.expires = jiffies_64 + (EVENT_TIMER_INTERVAL);
	mod_timer(&event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL * 2);

	ret = ist30xx_get_info(data);
	tp_color = Lockdown_Info_High.lockinfo[1];
	dev_info(&client->dev,
		 "Lockdown info: %02X %02X %02X %02X %02X %02X %02X %02X",
		 Lockdown_Info_High.lockinfo[3], Lockdown_Info_High.lockinfo[2],
		 Lockdown_Info_High.lockinfo[1], Lockdown_Info_High.lockinfo[0],
		 Lockdown_Info_LOW.lockinfo[3], Lockdown_Info_LOW.lockinfo[2],
		 Lockdown_Info_LOW.lockinfo[1], Lockdown_Info_LOW.lockinfo[0]);

#if  WT_ADD_CTP_INFO
	switch (Lockdown_Info_High.lockinfo[1]) {
	case TP_White:
		sprintf(ic_color, "%s", "White");
		break;
	case TP_Black:
		sprintf(ic_color, "%s", "Black");
		break;
	case TP_Golden:
		sprintf(ic_color, "%s", "Golden");
		break;
	default:
		sprintf(ic_color, "%s", "Other Color");
		break;
	}

	if (data->chip_id == IST30XX_CHIP_ID) {
		switch (data->product_id) {
		case A12HD_PID:
			sprintf(tp_string_version, "BIEL,ist3038,FW:%2x,%s",
				data->fw.cur.fw_ver, ic_color);
			break;
		default:
			sprintf(tp_string_version, "BIEL,unknwon,FW:%2x,%s",
				data->fw.cur.fw_ver, ic_color);
			break;
		}
	} else {
		sprintf(tp_string_version, "BIEL,unknwon,FW:%2x,%s",
			data->fw.cur.fw_ver, ic_color);
	}

#endif

#if  WT_CTP_OPEN_SHORT_TEST
	global_ts_data = data;
	create_tp_proc();
#endif

#ifdef USE_TSP_TA_CALLBACKS
	data->callbacks.inform_charger = charger_enable;
	ist30xx_register_callback(&data->callbacks);
#endif

	/* release Firmware hold mode(forced active mode) */
	ist30xx_write_cmd(data, IST30XX_HIB_CMD,
			  (eHCOM_FW_HOLD << 16) | (0 & 0xFFFF));

	data->initialized = true;

	tsp_info("### IMAGIS probe success ###\n");

	ist30xx_enable_irq(data);

#if CTP_CHARGER_DETECT
	batt_psy = power_supply_get_by_name("usb");
	if (!batt_psy)
		tsp_info("tp battery supply not found\n");

	schedule_delayed_work(&ts_data->work_charger_check,
			      CHECK_CHARGER_INTERVAL);
#endif

	return 0;

err_sysfs:
	class_destroy(ist30xx_class);
	input_unregister_device(input_dev);

err_irq:
	ist30xx_disable_irq(data);
	free_irq(client->irq, data);

err_init_drv:
	input_free_device(input_dev);
	data->status.event_mode = false;
	gpio_free(pdata->reset_gpio);
	gpio_free(pdata->irq_gpio);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
err_alloc_dev:
	kfree(data);
	tsp_err("Error, ist30xx init driver\n");
	return -ENODEV;
}

static int ist30xx_remove(struct i2c_client *client)
{
	struct ist30xx_data *data = i2c_get_clientdata(client);

#if defined(CONFIG_FB)
	if (fb_unregister_client(&data->fb_notif))
		dev_err(&client->dev,
			"Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&data->early_suspend);
#endif

	ist30xx_disable_irq(data);
	free_irq(client->irq, data);
	ist30xx_power_off(data);
	input_unregister_device(data->input_dev);
	input_free_device(data->input_dev);
	kfree(data);

	return 0;
}

static void ist30xx_shutdown(struct i2c_client *client)
{
	struct ist30xx_data *data = i2c_get_clientdata(client);

	del_timer(&event_timer);
#if CTP_CHARGER_DETECT
	cancel_delayed_work_sync(&data->work_charger_check);
#endif
	cancel_delayed_work_sync(&data->work_noise_protect);
	cancel_delayed_work_sync(&data->work_reset_check);
	cancel_delayed_work_sync(&data->work_debug_algorithm);
	mutex_lock(&ist30xx_mutex);
	ist30xx_disable_irq(data);
	ist30xx_internal_suspend(data);
	clear_input_data(data);
	mutex_unlock(&ist30xx_mutex);
}

static struct i2c_device_id ist30xx_idtable[] = {
	{IST30XX_DEV_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, ist30xx_idtable);

#ifdef CONFIG_OF
static struct of_device_id ist30xx_match_table[] = {
	{.compatible = "imagis,ist30xx-ts",},
	{},
};
#else
#define ist30xx_match_table NULL
#endif

static struct i2c_driver ist30xx_i2c_driver = {
	.id_table = ist30xx_idtable,
	.probe = ist30xx_probe,
	.remove = ist30xx_remove,
	.shutdown = ist30xx_shutdown,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = IST30XX_DEV_NAME,
		   .of_match_table = ist30xx_match_table,
#ifdef CONFIG_PM
		   .pm = &ist30xx_pm_ops,
#endif
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
