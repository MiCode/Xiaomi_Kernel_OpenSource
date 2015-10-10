/*
 *  Copyright (C) 2010,Imagis Technology Co. Ltd. All Rights Reserved.
 *  Copyright (C) 2015 XiaoMi, Inc.
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
#include <linux/of_gpio.h>
#include <linux/power_supply.h>

#include "ist30xx.h"
#include "ist30xx_update.h"
#include "ist30xx_tracking.h"

#if IST30XX_DEBUG
#include "ist30xx_misc.h"
#endif
#if IST30XX_CMCS_TEST
#include "ist30xx_cmcs.h"
#endif

#define MAX_ERR_CNT             (100)

struct ist30xx_data *ts_data;

#if IST30XX_EVENT_MODE
static struct timespec t_current;               // ns
int timer_period_ms = 500;                      // 0.5sec
#define EVENT_TIMER_INTERVAL     (HZ * timer_period_ms / 1000)
#endif  // IST30XX_EVENT_MODE


void tsp_printk(int level, const char *fmt, ...)
{
	struct va_format vaf;
	va_list args;
	int r;

	if (ts_data->dbg_level < level)
		return;

	va_start(args, fmt);

	vaf.fmt = fmt;
	vaf.va = &args;

	r = printk("%s %pV", IST30XX_DEBUG_TAG, &vaf);

	va_end(args);
}

long get_milli_second(void)
{
#if IST30XX_EVENT_MODE
	ktime_get_ts(&t_current);

	return t_current.tv_sec * 1000 + t_current.tv_nsec / 1000000;
#else
	return 0;
#endif  // IST30XX_EVENT_MODE
}

int ist30xx_intr_wait(struct ist30xx_data *data, long ms)
{
	long start_ms = get_milli_second();
	long curr_ms = 0;

	while (1) {
		if (!data->irq_working)
			break;

		curr_ms = get_milli_second();
		if ((curr_ms < 0) || (start_ms < 0) || (curr_ms - start_ms > ms)) {
			tsp_info("%s() timeout(%dms)\n", __func__, ms);
			return -EPERM;
		}

		msleep(2);
	}
	return 0;
}


void ist30xx_disable_irq(struct ist30xx_data *data)
{
	if (likely(data->irq_enabled)) {
		ist30xx_tracking(data, TRACK_INTR_DISABLE);
		disable_irq(data->client->irq);
		data->irq_enabled = 0;
		data->status.event_mode = false;
	}
}

void ist30xx_enable_irq(struct ist30xx_data *data)
{
	if (likely(!data->irq_enabled)) {
		ist30xx_tracking(data, TRACK_INTR_ENABLE);
		enable_irq(data->client->irq);
		msleep(10);
		data->irq_enabled = 1;
		data->status.event_mode = true;
	}
}


int ist30xx_max_error_cnt = MAX_ERR_CNT;
void ist30xx_scheduled_reset(struct ist30xx_data *data)
{
	if (likely(data->initialized))
		schedule_delayed_work(&data->work_reset_check, 0);
}

static void ist30xx_request_reset(struct ist30xx_data *data)
{
	data->error_cnt++;
	if (data->error_cnt >= MAX_ERR_CNT) {
		tsp_info("%s()\n", __func__);
		schedule_delayed_work(&data->work_reset_check, 0);
		data->error_cnt = 0;
	}
}


#define NOISE_MODE_TA       (0)
#define NOISE_MODE_CALL     (1)
#define NOISE_MODE_COVER    (2)
void ist30xx_start(struct ist30xx_data *data)
{
	if (data->initialized) {
		data->noise_mode &= ~(1 << NOISE_MODE_TA);
		data->noise_mode |= (data->ta_status << NOISE_MODE_TA);

		ist30xx_tracking(data, data->ta_status ?
				 TRACK_CMD_TACON : TRACK_CMD_TADISCON);

#if IST30XX_EVENT_MODE
		mod_timer(&data->event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL * 2);
#endif
	}

	data->noise_mode |= (TSP_LOCAL_CODE << 16);
	ist30xx_write_cmd(data->client, CMD_SET_NOISE_MODE, data->noise_mode);

	tsp_info("%s(), local : %d, mode : 0x%x\n", __func__,
		 (data->noise_mode >> 16) & 0xFFFF, data->noise_mode & 0xFFFF);

	if (data->report_rate >= 0) {
		ist30xx_write_cmd(data->client, CMD_SET_REPORT_RATE, data->report_rate);
		tsp_info(" reporting rate : %dus\n", data->report_rate);
	}

	if (data->idle_rate >= 0) {
		ist30xx_write_cmd(data->client, CMD_SET_IDLE_TIME, data->idle_rate);
		tsp_info(" idle rate : %dus\n", data->idle_rate);
	}

	ist30xx_cmd_start_scan(data->client);
}


int ist30xx_get_ver_info(struct ist30xx_data *data)
{
	int ret;

	data->fw.prev_core_ver = data->fw.core_ver;
	data->fw.prev_param_ver = data->fw.param_ver;
	data->fw.core_ver = data->fw.param_ver = 0;

	ret = ist30xx_read_cmd(data->client, CMD_GET_FW_VER, &data->fw.core_ver);
	if (unlikely(ret))
		return ret;

	ret = ist30xx_read_cmd(data->client, CMD_GET_PARAM_VER, &data->fw.param_ver);
	if (unlikely(ret))
		return ret;

	ret = ist30xx_read_cmd(data->client, CMD_GET_SUB_VER, &data->fw.sub_ver);
	if (unlikely(ret))
		return ret;

	tsp_info("IC version read core: %x, param: %x, sub: %x\n",
		 data->fw.core_ver, data->fw.param_ver, data->fw.sub_ver);

	return 0;
}


int ist30xx_init_touch_driver(struct ist30xx_data *data)
{
	int ret = 0;

	mutex_lock(&data->ist30xx_mutex);
	ist30xx_disable_irq(data);

	ret = ist30xx_cmd_run_device(data->client, true);
	if (unlikely(ret))
		goto init_touch_end;

	ist30xx_get_ver_info(data);

init_touch_end:
	ist30xx_start(data);

	ist30xx_enable_irq(data);
	mutex_unlock(&data->ist30xx_mutex);

	return ret;
}


#if IST30XX_DEBUG
void ist30xx_print_info(struct ist30xx_data *data)
{
	TSP_INFO *tsp = &data->tsp_info;
	TKEY_INFO *tkey = &data->tkey_info;

	tsp_info("*** TSP/TKEY info ***\n");
	tsp_info("tscn finger num : %d\n", tsp->finger_num);
	tsp_info("tscn dir swap: %d, flip x: %d, y: %d\n",
		 tsp->dir.swap_xy, tsp->dir.flip_x, tsp->dir.flip_y);
	tsp_info("tscn ch_num tx: %d, rx: %d\n",
		 tsp->ch_num.tx, tsp->ch_num.rx);
	tsp_info("tscn width: %d, height: %d\n",
		 tsp->width, tsp->height);
	tsp_info("tkey enable: %d, key num: %d, axis rx: %d \n",
		 tkey->enable, tkey->key_num, tkey->axis_rx);
	tsp_info("tkey ch_num[0] %d, [1] %d, [2] %d, [3] %d, [4] %d\n",
		 tkey->ch_num[0], tkey->ch_num[1], tkey->ch_num[2],
		 tkey->ch_num[3], tkey->ch_num[4]);
}
#endif

#define CALIB_MSG_MASK          (0xF0000FFF)
#define CALIB_MSG_VALID         (0x80000CAB)
#define TRACKING_INTR_VALID     (0x127EA597)
u32 tracking_intr_value = TRACKING_INTR_VALID;
int ist30xx_get_info(struct ist30xx_data *data)
{
	int ret;
	u32 calib_msg;
	u32 ms;

	mutex_lock(&data->ist30xx_mutex);
	ist30xx_disable_irq(data);

#if IST30XX_INTERNAL_BIN
#if IST30XX_DEBUG
	ist30xx_get_tsp_info(data);
	ist30xx_get_tkey_info(data);
#endif  // IST30XX_DEBUG
#else
	ret = ist30xx_cmd_run_device(data->client, false);
	if (unlikely(ret))
		goto get_info_end;

	ret = ist30xx_get_ver_info(data);
	if (unlikely(ret))
		goto get_info_end;

#if IST30XX_DEBUG
	ret = ist30xx_tsp_update_info(data);
	if (unlikely(ret))
		goto get_info_end;

	ret = ist30xx_tkey_update_info(data);
	if (unlikely(ret))
		goto get_info_end;
#endif  // IST30XX_DEBUG
#endif  // IST30XX_INTERNAL_BIN

#if IST30XX_DEBUG
	ist30xx_print_info(data);
	data->max_fingers = data->tsp_info.finger_num;
	data->max_keys = data->tkey_info.key_num;
#endif  // IST30XX_DEBUG

	ret = ist30xx_read_cmd(data->client, CMD_GET_CALIB_RESULT, &calib_msg);
	if (likely(ret == 0)) {
		tsp_info("calib status: 0x%08x\n", calib_msg);
		ms = get_milli_second();
		ist30xx_put_track_ms(data, ms);
		ist30xx_put_track(data, &tracking_intr_value, 1);
		ist30xx_put_track(data, &calib_msg, 1);
		if ((calib_msg & CALIB_MSG_MASK) != CALIB_MSG_VALID ||
		    CALIB_TO_STATUS(calib_msg) > 0) {
			ist30xx_calibrate(data, IST30XX_FW_UPDATE_RETRY);

			ist30xx_cmd_run_device(data->client, true);
		}
	}

#if (IST30XX_EVENT_MODE && IST30XX_CHECK_CALIB)
	if (likely(!data->status.update)) {
		ret = ist30xx_cmd_check_calib(data->client);
		if (likely(!ret)) {
			data->status.calib = 1;
			data->status.calib_msg = 0;
			data->event_ms = (u32)get_milli_second();
			data->status.event_mode = true;
		}
	}
#else
	ist30xx_start(data);
#endif

#if !(IST30XX_INTERNAL_BIN)
get_info_end:
#endif
	if (likely(ret == 0))
		ist30xx_enable_irq(data);
	mutex_unlock(&data->ist30xx_mutex);

	return ret;
}


#define PRESS_MSG_MASK          (0x01)
#define MULTI_MSG_MASK          (0x02)
#define PRESS_MSG_KEY           (0x6)

#define TOUCH_DOWN_MESSAGE      ("p")
#define TOUCH_UP_MESSAGE        ("r")
#define TOUCH_MOVE_MESSAGE      (" ")

void print_tsp_event(struct ist30xx_data *data, finger_info *finger)
{
	int idx = finger->bit_field.id - 1;
	bool press;

#if IST30XX_EXTEND_COORD
	press = PRESSED_FINGER(data->t_status, finger->bit_field.id);
#else
	press = (finger->bit_field.udmg & PRESS_MSG_MASK) ? true : false;
#endif

	if (press) {
		if (data->tsp_touched[idx] == false) { // touch down
			tsp_notc("%s%d (%d, %d)\n",
				 TOUCH_DOWN_MESSAGE, finger->bit_field.id,
				 finger->bit_field.x, finger->bit_field.y);
			data->tsp_touched[idx] = true;
		} else {                    // touch move
			tsp_debug("%s%d (%d, %d)\n",
				  TOUCH_MOVE_MESSAGE, finger->bit_field.id,
				  finger->bit_field.x, finger->bit_field.y);
		}
	} else {
		if (data->tsp_touched[idx] == true) { // touch up
			tsp_notc("%s%d\n", TOUCH_UP_MESSAGE, finger->bit_field.id);
			data->tsp_touched[idx] = false;
		}
	}
}

void print_tkey_event(struct ist30xx_data *data, int id)
{
#if IST30XX_EXTEND_COORD
	int idx = id - 1;
	bool press = PRESSED_KEY(data->t_status, id);

	if (press) {
		if (data->tkey_pressed[idx] == false) { // tkey down
			tsp_notc("k %s%d \n", TOUCH_DOWN_MESSAGE, id);
			data->tkey_pressed[idx] = true;
		}
	} else {
		if (data->tkey_pressed[idx] == true) { // tkey up
			tsp_notc("k %s%d \n", TOUCH_UP_MESSAGE, id);
			data->tkey_pressed[idx] = false;
		}
	}
#endif
}


#if IST30XX_EXTEND_COORD
static void release_finger(struct ist30xx_data *data, int id)
{
	input_mt_slot(data->input_dev, id - 1);
	input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);

	ist30xx_tracking(data, TRACK_POS_FINGER + id);
	tsp_info("%s() %d\n", __func__, id);

	data->tsp_touched[id - 1] = false;

	input_sync(data->input_dev);
}
#else
static void release_finger(struct ist30xx_data *data, finger_info *finger)
{
	input_mt_slot(data->input_dev, finger->bit_field.id - 1);
	input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);

	ist30xx_tracking(data, TRACK_POS_FINGER + finger->bit_field.id);
	tsp_info("%s() %d(%d, %d)\n", __func__,
		 finger->bit_field.id, finger->bit_field.x, finger->bit_field.y);

	finger->bit_field.udmg &= ~(PRESS_MSG_MASK);
	print_tsp_event(data, finger);

	finger->bit_field.id = 0;

	input_sync(data->input_dev);
}
#endif


#define CANCEL_KEY  (0xff)
#define RELEASE_KEY (0)
#if IST30XX_EXTEND_COORD
static void release_key(struct ist30xx_data *data, int id, u8 key_status)
{
	int index = data->current_index;

	input_report_key(data->input_dev,
			 data->pdata->config_array[index].key_code[id - 1], key_status);

	ist30xx_tracking(data, TRACK_POS_KEY + id);
	tsp_info("%s() key%d, status: %d\n", __func__, id, key_status);

	data->tkey_pressed[id - 1] = false;

	input_sync(data->input_dev);
}
#else
static void release_key(struct ist30xx_data *data, finger_info *key, u8 key_status)
{
	int id = key->bit_field.id;
	int index = data->current_index;

	input_report_key(data->input_dev,
			 data->pdata->config_array[index].key_code[id], key_status);

	ist30xx_tracking(data, TRACK_POS_KEY + id);
	tsp_info("%s() key%d, event: %d, status: %d\n", __func__,
		  id, key->bit_field.w, key_status);

	key->bit_field.id = 0;

	input_sync(data->input_dev);
}
#endif

static void clear_input_data(struct ist30xx_data *data)
{
#if IST30XX_EXTEND_COORD
	int id = 1;
	u32 status;

	status = PARSE_FINGER_STATUS(data->t_status);
	while (status) {
		if (status & 1)
			release_finger(data, id);
		status >>= 1;
		id++;
	}

	id = 1;
	status = PARSE_KEY_STATUS(data->t_status);
	while (status) {
		if (status & 1)
			release_key(data, id, RELEASE_KEY);
		status >>= 1;
		id++;
	}
	data->t_status = 0;
#else
	int i;
	finger_info *fingers = (finger_info *)data->prev_fingers;
	finger_info *keys = (finger_info *)data->prev_keys;

	for (i = 0; i < data->num_fingers; i++) {
		if (fingers[i].bit_field.id == 0)
			continue;

		if (fingers[i].bit_field.udmg & PRESS_MSG_MASK)
			release_finger(data, &fingers[i]);
	}

	for (i = 0; i < data->num_keys; i++) {
		if (keys[i].bit_field.id == 0)
			continue;

		if (keys[i].bit_field.w == PRESS_MSG_KEY)
			release_key(data, &keys[i], RELEASE_KEY);
	}
#endif
}

static int check_report_fingers(struct ist30xx_data *data, int finger_counts)
{
	int i;
	finger_info *fingers = (finger_info *)data->fingers;

#if IST30XX_EXTEND_COORD
	/* current finger info */
	for (i = 0; i < finger_counts; i++) {
		if (unlikely((fingers[i].bit_field.x > IST30XX_MAX_X) ||
			     (fingers[i].bit_field.y > IST30XX_MAX_Y))) {
			tsp_warn("Invalid touch data - %d: %d(%d, %d), 0x%08x\n", i,
				 fingers[i].bit_field.id,
				 fingers[i].bit_field.x,
				 fingers[i].bit_field.y,
				 fingers[i].full_field);

			fingers[i].bit_field.id = 0;
			ist30xx_tracking(data, TRACK_POS_UNKNOWN);
			return -EPERM;
		}
	}
#else
	int j;
	bool valid_id;
	finger_info *prev_fingers = (finger_info *)data->prev_fingers;

	/* current finger info */
	for (i = 0; i < finger_counts; i++) {
		if (unlikely((fingers[i].bit_field.id == 0) ||
			     (fingers[i].bit_field.id > data->max_fingers) ||
			     (fingers[i].bit_field.x > IST30XX_MAX_X) ||
			     (fingers[i].bit_field.y > IST30XX_MAX_Y))) {
			tsp_warn("Invalid touch data - %d: %d(%d, %d), 0x%08x\n", i,
				 fingers[i].bit_field.id,
				 fingers[i].bit_field.x,
				 fingers[i].bit_field.y,
				 fingers[i].full_field);

			fingers[i].bit_field.id = 0;
			ist30xx_tracking(data, TRACK_POS_UNKNOWN);
			return -EPERM;
		}
	}

	/* previous finger info */
	if (data->num_fingers >= finger_counts) {
		for (i = 0; i < data->max_fingers; i++) { // prev_fingers
			if (prev_fingers[i].bit_field.id != 0 &&
			    (prev_fingers[i].bit_field.udmg & PRESS_MSG_MASK)) {
				valid_id = false;
				for (j = 0; j < data->max_fingers; j++) { // fingers
					if ((prev_fingers[i].bit_field.id) ==
					    (fingers[j].bit_field.id)) {
						valid_id = true;
						break;
					}
				}
				if (valid_id == false)
					release_finger(data, &prev_fingers[i]);
			}
		}
	}
#endif

	return 0;
}

#if IST30XX_EXTEND_COORD
static int check_valid_coord(u32 *msg, int cnt)
{
	int ret = 0;
	u8 *buf = (u8 *)msg;
	u8 chksum1 = msg[0] >> 24;
	u8 chksum2 = 0;

	msg[0] &= 0x00FFFFFF;

	cnt *= IST30XX_DATA_LEN;

	while (cnt--)
		chksum2 += *buf++;

	if (chksum1 != chksum2) {
		tsp_err("intr chksum: %02x, %02x\n", chksum1, chksum2);
		ret = -EPERM;
	}

	return (chksum1 == chksum2) ? 0 : -EPERM;
}
#endif


static void report_input_data(struct ist30xx_data *data, int finger_counts, int key_counts)
{
	int id;
	bool press = false;
	finger_info *fingers = (finger_info *)data->fingers;
	struct ist30xx_platform_data *pdata = data->pdata;
	int index = data->current_index;

#if IST30XX_EXTEND_COORD
	int idx = 0;
	u32 status;

	status = PARSE_FINGER_STATUS(data->t_status);
	for (id = 0; id < data->max_fingers; id++) {
		press = (status & (1 << id)) ? true : false;

		input_mt_slot(data->input_dev, id);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, press);

		fingers[idx].bit_field.id = id + 1;
		print_tsp_event(data, &fingers[idx]);

		if (press == false)
			continue;

		input_report_abs(data->input_dev, ABS_MT_POSITION_X,
				 fingers[idx].bit_field.x);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
				 fingers[idx].bit_field.y);
		input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
				 fingers[idx].bit_field.area);
		idx++;
	}

#if IST30XX_USE_KEY
	status = PARSE_KEY_STATUS(data->t_status);
	for (id = 0; id < data->max_keys; id++) {
		press = (status & (1 << id)) ? true : false;

		input_report_key(data->input_dev,
				 pdata->config_array[index].key_code[id], press);

		print_tkey_event(data, id + 1);
	}
#endif  // IST30XX_USE_KEY

#else   // IST30XX_EXTEND_COORD
	int i, count;

	memset(data->prev_fingers, 0, sizeof(data->prev_fingers));

	for (i = 0; i < finger_counts; i++) {
		press = (fingers[i].bit_field.udmg & PRESS_MSG_MASK) ? true : false;

		print_tsp_event(data, &fingers[i]);

		input_mt_slot(data->input_dev, fingers[i].bit_field.id - 1);
		input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, press);

		if (press) {
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,
					 fingers[i].bit_field.x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
					 fingers[i].bit_field.y);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
					 fingers[i].bit_field.w);
		}

		data->prev_fingers[i] = fingers[i];
	}

#if IST30XX_USE_KEY
	for (i = finger_counts; i < finger_counts + key_counts; i++) {
		id = fingers[i].bit_field.id;
		press = (fingers[i].bit_field.w == PRESS_MSG_KEY) ? true : false;

		tsp_notc("key(%08x) id: %d, press: %d, val: (%d, %d)\n",
			  fingers[i].full_field, id, press,
			  fingers[i].bit_field.x, fingers[i].bit_field.y);

		input_report_key(data->input_dev,
				 pdata->config_array[index].key_code[id], press);

		data->prev_keys[id - 1] = fingers[i];
	}
#endif  // IST30XX_USE_KEY

	data->num_fingers = finger_counts;
	data->num_keys = key_counts;
#endif  // IST30XX_EXTEND_COORD

	data->error_cnt = 0;
	data->scan_retry = 0;

	input_sync(data->input_dev);
}

/*
 * CMD : CMD_GET_COORD
 *
 * IST30XX_EXTEND_COORD == 0 (IST3026, IST3032, IST3026B, IST3032B)
 *
 *               [31:30]  [29:26]  [25:16]  [15:10]  [9:0]
 *   Multi(1st)  UDMG     Rsvd.    NumOfKey Rsvd.    NumOfFinger
 *    Single &   UDMG     ID       X        Area     Y
 *   Multi(2nd)
 *
 *   UDMG [31] 0/1 : single/multi
 *   UDMG [30] 0/1 : unpress/press
 *
 * IST30XX_EXTEND_COORD == 1 (IST3038, IST3044)
 *
 *   1st  [31:24]   [23:21]   [20:16]   [15:12]   [11:10]   [9:0]
 *        '0x71'    KeyCnt    KeyStatus FingerCnt Rsvd.     FingerStatus
 *   2nd  [31:28]   [27:24]   [23:12]   [11:0]
 *        ID        Area      X         Y
 */
static irqreturn_t ist30xx_irq_thread(int irq, void *ptr)
{
	int i, ret;
	int key_cnt, finger_cnt, read_cnt;
	struct ist30xx_data *data = (struct ist30xx_data *)ptr;

#if EXTEND_COORD_CHECKSUM
	u32 msg[IST30XX_MAX_MT_FINGERS + 1];
	int offset = 1;
#else
	u32 msg[IST30XX_MAX_MT_FINGERS];
	int offset = 0;
#endif
	u32 ms;

	data->irq_working = true;

	if (unlikely(!data->irq_enabled))
		goto irq_end;

	ms = get_milli_second();
#if (EXTEND_COORD_CHECKSUM == 0)
	if (unlikely((ms >= data->event_ms) && (ms - data->event_ms < data->noise_ms))) // Noise detect
		goto irq_end;
#endif

	memset(msg, 0, sizeof(msg));

	ret = ist30xx_get_position(data->client, msg, 1);
	if (unlikely(ret))
		goto irq_err;

	tsp_verb("intr msg: 0x%08x\n", *msg);

	if (unlikely(*msg == 0xE11CE970))     // TSP IC Exception
		goto irq_ic_err;

#if (EXTEND_COORD_CHECKSUM == 0)
	if (unlikely(*msg == 0 || *msg == 0xFFFFFFFF || *msg == 0x2FFF03FF ||
		     *msg == 0x30003000 || *msg == 0x300B300B)) // Unknown CMD
		goto irq_err;
#else
	if (unlikely(*msg == 0 || *msg == 0xFFFFFFFF))          // Unknown CMD
		goto irq_err;
#endif

	data->event_ms = ms;
	ist30xx_put_track_ms(data, data->event_ms);
	ist30xx_put_track(data, &tracking_intr_value, 1);
	ist30xx_put_track(data, msg, 1);

	if (unlikely((*msg & CALIB_MSG_MASK) == CALIB_MSG_VALID)) {
		data->status.calib_msg = *msg;
		tsp_info("calib status: 0x%08x\n", data->status.calib_msg);

		goto irq_end;
	}

	memset(data->fingers, 0, sizeof(data->fingers));

#if IST30XX_EXTEND_COORD
	/* Unknown interrupt data for extend coordinate */
#if EXTEND_COORD_CHECKSUM
	if (unlikely(!CHECK_INTR_STATUS3(*msg)))
		goto irq_err;
#else
	if (unlikely(!CHECK_INTR_STATUS1(*msg) || !CHECK_INTR_STATUS2(*msg)))
		goto irq_err;
#endif

	data->t_status = *msg;
	key_cnt = PARSE_KEY_CNT(data->t_status);
	finger_cnt = PARSE_FINGER_CNT(data->t_status);
#else
	data->fingers[0].full_field = *msg;
	key_cnt = 0;
	finger_cnt = 1;
#endif

	read_cnt = finger_cnt;

#if (IST30XX_EXTEND_COORD == 0)
	if (data->fingers[0].bit_field.udmg & MULTI_MSG_MASK) {
		key_cnt = data->fingers[0].bit_field.x;
		finger_cnt = data->fingers[0].bit_field.y;
		read_cnt = finger_cnt + key_cnt;
#endif
	{
		if (unlikely((finger_cnt > data->max_fingers) ||
			     (key_cnt > data->max_keys))) {
			tsp_warn("Invalid touch count - finger: %d(%d), key: %d(%d)\n",
				 finger_cnt, data->max_fingers, key_cnt, data->max_keys);
			goto irq_err;
		}

		//tsp_info("read: %d (f:%d, k:%d)\n", read_cnt, finger_cnt, key_cnt);
		if (read_cnt > 0) {
#if I2C_BURST_MODE
			ret = ist30xx_get_position(data->client, &msg[offset], read_cnt);
			if (unlikely(ret))
				goto irq_err;

			for (i = 0; i < read_cnt; i++)
				data->fingers[i].full_field = msg[i + offset];
#else
			for (i = 0; i < read_cnt; i++) {
				ret = ist30xx_get_position(data->client, &msg[i + offset], 1);
				if (unlikely(ret))
					goto irq_err;

				data->fingers[i].full_field = msg[i + offset];
			}
#endif                  // I2C_BURST_MODE

			ist30xx_put_track(data, msg + offset, read_cnt);

			for (i = 0; i < read_cnt; i++)
				tsp_verb("intr msg(%d): 0x%08x\n", i + offset, msg[i + offset]);
		}
	}
#if (IST30XX_EXTEND_COORD == 0)
}
#endif

	data->event_ms = (u32)get_milli_second();

#if EXTEND_COORD_CHECKSUM
	ret = check_valid_coord(&msg[0], read_cnt + 1);
	if (unlikely(ret < 0))
		goto irq_err;
#endif

	if (unlikely(check_report_fingers(data, finger_cnt)))
		goto irq_end;

#if IST30XX_EXTEND_COORD
	report_input_data(data, finger_cnt, key_cnt);
#else
	if (likely(read_cnt > 0))
		report_input_data(data, finger_cnt, key_cnt);
#endif

	if (data->intr_debug_addr > 0 && data->intr_debug_size > 0) {
		ret = ist30xxb_burst_read(data->client,
					  data->intr_debug_addr, &msg[0], data->intr_debug_size);
		for (i = 0; i < data->intr_debug_size; i++)
			tsp_notc("\t%08x\n", msg[i]);
	}

	goto irq_end;

irq_err:
	data->event_ms = (u32)get_milli_second();
	tsp_err("intr msg: 0x%08x, ret: %d\n", msg[0], ret);
	ist30xx_request_reset(data);
irq_end:
	data->irq_working = false;
	return IRQ_HANDLED;

irq_ic_err:
	tsp_err("Occured IC exception\n");
	ist30xx_scheduled_reset(data);
	data->irq_working = false;
	data->event_ms = (u32)get_milli_second();
	return IRQ_HANDLED;
}


static int ist30xx_input_disable(struct input_dev *in_dev)
{
	struct ist30xx_data *data = input_get_drvdata(in_dev);

	mutex_lock(&data->ist30xx_mutex);
	ist30xx_disable_irq(data);
	ist30xx_internal_suspend(data);
	clear_input_data(data);
	mutex_unlock(&data->ist30xx_mutex);

	return 0;
}

static int ist30xx_input_enable(struct input_dev *in_dev)
{
	struct ist30xx_data *data = input_get_drvdata(in_dev);

	mutex_lock(&data->ist30xx_mutex);
	ist30xx_internal_resume(data);
	ist30xx_start(data);
	ist30xx_enable_irq(data);
	mutex_unlock(&data->ist30xx_mutex);

	return 0;
}

void ist30xx_set_ta_mode(struct ist30xx_data *data, bool charging)
{
	tsp_info("%s(), charging = %d\n", __func__, charging);

	if (unlikely(charging == data->ta_status))
		return;

	if (unlikely(data->noise_mode == -1)) {
		data->ta_status = charging ? 1 : 0;
		return;
	}

	data->ta_status = charging ? 1 : 0;

	ist30xx_scheduled_reset(data);
}
EXPORT_SYMBOL(ist30xx_set_ta_mode);

void ist30xx_set_call_mode(struct ist30xx_data *data, int mode)
{
	tsp_info("%s(), mode = %d\n", __func__, mode);

	if (unlikely(mode == ((data->noise_mode >> NOISE_MODE_CALL) & 1)))
		return;

	data->noise_mode &= ~(1 << NOISE_MODE_CALL);
	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_CALL);

	ist30xx_scheduled_reset(data);
}
EXPORT_SYMBOL(ist30xx_set_call_mode);

void ist30xx_set_cover_mode(struct ist30xx_data *data, int mode)
{
	tsp_info("%s(), mode = %d\n", __func__, mode);

	if (unlikely(mode == ((data->noise_mode >> NOISE_MODE_COVER) & 1)))
		return;

	data->noise_mode &= ~(1 << NOISE_MODE_COVER);
	if (mode)
		data->noise_mode |= (1 << NOISE_MODE_COVER);

	ist30xx_scheduled_reset(data);
}
EXPORT_SYMBOL(ist30xx_set_cover_mode);
static void reset_work_func(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *ts = container_of(delayed_work, struct ist30xx_data, work_reset_check);

	if (unlikely((ts == NULL) || (ts->client == NULL)))
		return;

	tsp_info("Request reset function\n");

	if (likely((ts->initialized == 1) && (ts->status.power == 1) &&
		   (ts->status.update != 1) && (ts->status.calib != 1))) {
		mutex_lock(&ts->ist30xx_mutex);
		ist30xx_disable_irq(ts);

		clear_input_data(ts);

		ist30xx_cmd_run_device(ts->client, true);

		ist30xx_start(ts);

		ist30xx_enable_irq(ts);
		mutex_unlock(&ts->ist30xx_mutex);
	}
}

static void ist30xx_charger_state_changed(struct ist30xx_data *ts)
{
	int is_usb_exist;

	is_usb_exist = power_supply_is_system_supplied();
	if (is_usb_exist != ts->is_usb_plug_in) {
		ts->is_usb_plug_in = is_usb_exist;
		tsp_info("Power state changed, set ta_mode to 0x%x\n", is_usb_exist);
		mutex_lock(&ts->ist30xx_mutex);
		if (ts->status.power == 1)
			ist30xx_set_ta_mode(ts, is_usb_exist);
		mutex_unlock(&ts->ist30xx_mutex);
	}
}

static int ist30xx_power_supply_event(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct ist30xx_data *ts = container_of(nb, struct ist30xx_data, power_supply_notifier);

	tsp_debug("%s\n", __func__);
	ist30xx_charger_state_changed(ts);

	return 0;
}

#if IST30XX_INTERNAL_BIN && IST30XX_UPDATE_BY_WORKQUEUE
static void fw_update_func(struct work_struct *work)
{
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *ts = container_of(delayed_work, struct ist30xx_data, work_fw_update);

	if (unlikely((ts == NULL) || (ts->client == NULL)))
		return;

	tsp_info("FW update function\n");

	if (likely(ist30xx_auto_bin_update(ts)))
		ist30xx_disable_irq(ts);
}
#endif // IST30XX_INTERNAL_BIN && IST30XX_UPDATE_BY_WORKQUEUE


#if IST30XX_EVENT_MODE
u32 ist30xx_max_scan_retry = 2;
#define SCAN_STATUS_MAGIC   (0x3C000000)
#define SCAN_STATUS_MASK    (0xFF000000)
#define FINGER_CNT_MASK     (0x00F00000)
#define SCAN_CNT_MASK       (0x000FFFFF)
#define GET_FINGER_CNT(k)   ((k & FINGER_CNT_MASK) >> 20)
#define GET_SCAN_CNT(k)     (k & SCAN_CNT_MASK)

static void noise_work_func(struct work_struct *work)
{
#if IST30XX_NOISE_MODE
	int ret;
	u32 scan_status = 0;
#if (IST30XX_EXTEND_COORD == 0)
	int i;
#endif
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *ts = container_of(delayed_work, struct ist30xx_data, work_noise_protect);

	ret = ist30xx_read_cmd(ts->client, IST30XXB_MEM_COUNT, &scan_status);
	if (unlikely(ret)) {
		tsp_warn("Mem scan count read fail!\n");
		goto retry_timer;
	}

	ist30xx_put_track_ms(ts, ts->timer_ms);
	ist30xx_put_track(ts, &scan_status, 1);

	tsp_verb("scan status: 0x%x\n", scan_status);

	/* Check valid scan count */
	if (unlikely((scan_status & SCAN_STATUS_MASK) != SCAN_STATUS_MAGIC)) {
		tsp_warn("Scan status is not corrected! (0x%08x)\n", scan_status);
		goto retry_timer;
	}
	/* Status of IC is idle */
	if (GET_FINGER_CNT(scan_status) == 0) {
#if IST30XX_EXTEND_COORD
		if ((PARSE_FINGER_CNT(ts->t_status) > 0) ||
		    (PARSE_KEY_CNT(ts->t_status) > 0))
			clear_input_data(ts);

#else
		for (i = 0; i < IST30XX_MAX_MT_FINGERS; i++) {
			if (ts->prev_fingers[i].bit_field.id == 0)
				continue;

			if (ts->prev_fingers[i].bit_field.udmg & PRESS_MSG_MASK) {
				tsp_warn("prev_fingers: 0x%08x\n",
					 ts->prev_fingers[i].full_field);
				release_finger(ts, &ts->prev_fingers[i]);
			}
		}

		for (i = 0; i < IST30XX_MAX_MT_FINGERS; i++) {
			if (ts->prev_keys[i].bit_field.id == 0)
				continue;

			if (ts->prev_keys[i].bit_field.w == PRESS_MSG_KEY) {
				tsp_warn("prev_keys: 0x%08x\n",
					 ts->prev_keys[i].full_field);
				release_key(ts, &ts->prev_keys[i], RELEASE_KEY);
			}
		}
#endif
	}

	scan_status &= SCAN_CNT_MASK;

	/* Status of IC is lock-up */
	if (scan_status == ts->scan_count) {
		tsp_warn("TSP IC is not responded!\n");
		goto retry_timer;
	} else {
		ts->scan_retry = 0;
	}

	ts->scan_count = scan_status;

	return;

retry_timer:
	ts->scan_retry++;
	tsp_warn("Retry scan status!(%d)\n", ts->scan_retry);

	if (ts->scan_retry == ist30xx_max_scan_retry) {
		ist30xx_scheduled_reset(ts);
		ts->scan_retry = 0;
	}
#endif  // IST30XX_NOISE_MODE
}

static void debug_work_func(struct work_struct *work)
{
#if IST30XX_ALGORITHM_MODE
	int ret = -EPERM;
	ALGR_INFO algr;
	u32 *buf32 = (u32 *)&algr;
	struct delayed_work *delayed_work = to_delayed_work(work);
	struct ist30xx_data *ts = container_of(delayed_work, struct ist30xx_data, work_debug_algorithm);

	ret = ist30xxb_burst_read(ts->client,
				  ts->algr_addr, (u32 *)&algr, ts->algr_size);
	if (unlikely(ret)) {
		tsp_warn("Algorithm mem addr read fail!\n");
		return;
	}

	ist30xx_put_track(ts, buf32, ts->algr_size);

	tsp_debug(" 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
		  buf32[0], buf32[1], buf32[2], buf32[3], buf32[4]);

	tsp_verb("  Scanstatus: %x\n", algr.scan_status);
	tsp_verb("  TouchCnt: %d\n", algr.touch_cnt);
	tsp_verb("  IntlTouchCnt: %d\n", algr.intl_touch_cnt);
	tsp_verb("  StatusFlag: %d\n", algr.status_flag);
	tsp_verb("  RAWPeakMax: %d\n", algr.raw_peak_max);
	tsp_verb("  RAWPeakMin: %d\n", algr.raw_peak_min);
	tsp_verb("  FLTPeakMax: %d\n", algr.flt_peak_max);
	tsp_verb("  AdptThreshold: %d\n", algr.adpt_threshold);
	tsp_verb("  KeyRawData0: %d\n", algr.key_raw_data[0]);
	tsp_verb("  KeyRawData1: %d\n", algr.key_raw_data[1]);
#endif
}

void timer_handler(unsigned long data)
{
	struct ist30xx_data *ts = (struct ist30xx_data *)data;
	struct ist30xx_status *status = &ts->status;

	if (ts->irq_working)
		goto restart_timer;

	if (status->event_mode) {
		if (likely((status->power == 1) && (status->update != 1))) {
			ts->timer_ms = (u32)get_milli_second();
			if (unlikely(status->calib == 1)) { // Check calibration
				if ((status->calib_msg & CALIB_MSG_MASK) == CALIB_MSG_VALID) {
					tsp_info("Calibration check OK!!\n");
					schedule_delayed_work(&ts->work_reset_check, 0);
					status->calib = 0;
				} else if (ts->timer_ms - ts->event_ms >= 3000) {     // 3second
					tsp_info("calibration timeout over 3sec\n");
					schedule_delayed_work(&ts->work_reset_check, 0);
					status->calib = 0;
				}
			} else if (likely(status->noise_mode)) {
				if (ts->timer_ms - ts->event_ms > 100)     // 0.1ms after last interrupt
					schedule_delayed_work(&ts->work_noise_protect, 0);
			}

#if IST30XX_ALGORITHM_MODE
			if ((ts->algr_addr >= IST30XXB_ACCESS_ADDR) &&
			    (ts->algr_size > 0))
				schedule_delayed_work(&ts->work_debug_algorithm, 0);
#endif
		}
	}
restart_timer:
	mod_timer(&ts->event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL);
}
#endif // IST30XX_EVENT_MODE

#ifdef CONFIG_OF
static void ist30xx_dump_dt(struct device *dev, struct ist30xx_platform_data *pdata)
{
	int i;

	pr_info("### reset gpio = %d ####\n", pdata->reset_gpio);
	pr_info("### irq gpio = %d ####\n", pdata->irq_gpio);

	pr_info("### max x = %d ####\n", pdata->max_x);
	pr_info("### max y = %d ####\n", pdata->max_y);
	pr_info("### max w = %d ####\n", pdata->max_w);

	pr_info("### key num = %d ####\n", pdata->key_num);

	for (i = 0; i < pdata->key_num; i++)
		pr_info("#### key [%d] = %d ####\n", i,
			(int)pdata->config_array[0].key_code[i]);
}

static int ist30xx_parse_dt(struct device *dev, struct ist30xx_platform_data *pdata)
{
	int ret;
	struct ist30xx_config_info *info;
	struct device_node *temp, *np = dev->of_node;
	u32 temp_val;

	/* reset, irq, power gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "ist30xx,reset-gpio",
						    0, &pdata->reset_gpio_flags);
	pdata->irq_gpio = of_get_named_gpio_flags(np, "ist30xx,irq-gpio",
						  0, &pdata->irq_gpio_flags);
	pdata->power_gpio = of_get_named_gpio_flags(np, "ist30xx,power-gpio",
						    0, &pdata->power_gpio_flags);
	ret = of_property_read_u32(np, "ist30xx,irqflags", &temp_val);
	if (ret) {
		dev_err(dev, "Unable to read irqflags id\n");
		return ret;
	} else {
		pdata->irqflags = temp_val;
	}

	ret = of_property_read_u32(np, "ist30xx,max-x", &pdata->max_x);
	if (ret)
		dev_err(dev, "Unable to read max_x\n");


	ret = of_property_read_u32(np, "ist30xx,max-y", &pdata->max_y);
	if (ret)
		dev_err(dev, "Unable to read max_y\n");

	ret = of_property_read_u32(np, "ist30xx,max-w", &pdata->max_w);
	if (ret)
		dev_err(dev, "Unable to read max_w\n");

	ret = of_property_read_u32(np, "ist30xx,key-num", &pdata->key_num);
	if (ret)
		dev_err(dev, "Unable to read key number\n");

	ret = of_property_read_u32(np, "ist30xx,config-array-size", &pdata->config_array_size);
	if (ret) {
		dev_err(dev, "Unable to get array size\n");
		return ret;
	}

	pdata->config_array = devm_kzalloc(dev, pdata->config_array_size *
					   sizeof(struct ist30xx_config_info), GFP_KERNEL);
	if (!pdata->config_array) {
		dev_err(dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	info = pdata->config_array;

	for_each_child_of_node(np, temp) {
		info->key_code = devm_kzalloc(dev, pdata->key_num *
					      sizeof(int), GFP_KERNEL);
		if (!info->key_code) {
			dev_err(dev, "Unable to allocate memory for key code\n");
			return -ENOMEM;
		}

		ret = of_property_read_u32(temp, "ist30xx,tsp-type", &info->tsp_type);
		if (ret) {
			dev_err(dev, "Failed to get tsp-type\n");
			return ret;
		}

		ret = of_property_read_string(temp, "ist30xx,tsp-name",
						&info->tsp_name);
		if (ret) {
			dev_err(dev, "Failed to get tsp-name\n");
			return ret;
		}

		ret = of_property_read_u32_array(temp, "ist30xx,key-code",
						 info->key_code, pdata->key_num);
		if (ret) {
			dev_err(dev, "Failed to get key-code\n");
			return ret;
		}

		ret = of_property_read_string(temp, "ist30xx,cmcs-name",
						&info->cmcs_name);
		if (ret && (ret != -EINVAL)) {
			dev_err(dev, "Unable to read cmcs name\n");
			return ret;
		}

		ret = of_property_read_string(temp, "ist30xx,fw-name",
						&info->fw_name);
		if (ret && (ret != -EINVAL)) {
			dev_err(dev, "Unable to read cfg name\n");
			return ret;
		}

		info++;
	}

	ist30xx_dump_dt(dev, pdata);

	return 0;
}
#else
static int ist30xx_parse_dt(struct device *dev, struct ist30xx_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int __devinit ist30xx_probe(struct i2c_client *		client,
				   const struct i2c_device_id * id)
{
	int ret;
	int retry = 3;
	int j, index = 0;
	struct ist30xx_data *data;
	struct input_dev *input_dev;
	struct  ist30xx_platform_data *pdata;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	ts_data = data;
	ts_data->dbg_level = IST30XX_DEBUG_LEVEL;
	ts_data->noise_ms = 6;

	tsp_info("%s(), the i2c addr=0x%x", __func__, client->addr);

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				     sizeof(struct ist30xx_platform_data), GFP_KERNEL);
		if (unlikely(!pdata)) {
			tsp_err("Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_alloc_dev;
		}

		ret = ist30xx_parse_dt(&client->dev, pdata);
		if (unlikely(ret)) {
			tsp_err("Failed to parse device tree\n");
			goto err_alloc_dev;
		}
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata)
		return -EINVAL;

	data->pdata = pdata;

	mutex_init(&data->ist30xx_mutex);

	data->ta_status = -1;
	data->noise_mode = -1;
	data->report_rate = -1;
	data->idle_rate = -1;

	input_dev = input_allocate_device();
	if (unlikely(!input_dev)) {
		ret = -ENOMEM;
		tsp_err("%s(), input_allocate_device failed (%d)\n", __func__, ret);
		goto err_alloc_dev;
	}

	data->max_fingers = data->max_keys = IST30XX_MAX_MT_FINGERS;
	data->irq_enabled = 1;
	data->client = client;
	data->input_dev = input_dev;
	i2c_set_clientdata(client, data);

	if (gpio_is_valid(pdata->irq_gpio)) {
		ret = gpio_request(pdata->irq_gpio, "imagis_irq_gpio");
		if (unlikely(ret < 0)) {
			tsp_err("irq gpio request failed!\n");
			goto err_alloc_input_dev;
		}
		ret = gpio_direction_input(pdata->irq_gpio);
		if (unlikely(ret < 0)) {
			tsp_err("set direction for irq gpio failed!\n");
			goto free_irq_gpio;
		}
	}

	if (gpio_is_valid(pdata->reset_gpio)) {
		ret = gpio_request(pdata->reset_gpio, "imagis_reset_gpio");
		if (unlikely(ret < 0)) {
			tsp_err("reset gpio request failed!\n");
			goto free_irq_gpio;
		}
		ret = gpio_direction_output(pdata->reset_gpio, 0);
		if (unlikely(ret < 0)) {
			tsp_err("set direction for reset gpio failed!\n");
			goto free_reset_gpio;
		}
	}

	ret = ist30xx_init_system(data);
	if (unlikely(ret)) {
		tsp_err("chip initialization failed\n");
		goto err_init_drv;
	}

	/*{
	 *      int i;
	 *      for (i = 0; i < 255; i++) {
	 *              data->client->addr = i;
	 *              ret = ist30xx_read_cmd(data->client, IST30XXB_REG_CHIPID, &data->chip_id);
	 *              if (ret == 0) {
	 *                      pr_info("#### addr = %d pass ####\n", (int)data->client->addr);
	 *                      break;
	 *              } else
	 *                      pr_info("#### addr = %d fail ####\n", (int)data->client->addr);
	 *      }
	 * }*/

	ret = request_threaded_irq(client->irq, NULL, ist30xx_irq_thread,
				   pdata->irqflags, "ist30xx_ts", data);
	if (unlikely(ret))
		goto err_req_irq;

	ist30xx_disable_irq(data);

	while (data->chip_id != IST3038_CHIP_ID) {
		ret = ist30xx_read_cmd(data->client, IST30XXB_REG_CHIPID, &data->chip_id);
		if (unlikely(ret))
			tsp_warn("reg chip id read fail!\n");

		if (data->chip_id == 0x3000B)
			data->chip_id = IST30XXB_CHIP_ID;

		if (retry-- == 0) {
			tsp_err("Touch IC might be not ist30xx!\n");
			goto err_read_chip_id;
		}
	}

	retry = 3;
	while (retry-- > 0) {
		ret = ist30xx_read_cmd(data->client, IST30XXB_REG_TSPTYPE,
				       &data->tsp_type);
		if (unlikely(ret)) continue;

		tsp_info("tsptype: %x\n", data->tsp_type);
		data->tsp_type = IST30XXB_PARSE_TSPTYPE(data->tsp_type);

		if (likely(ret == 0))
			break;

		data->tsp_type = TSP_TYPE_UNKNOWN;
	}

	ret = ist30xx_init_update_sysfs(data);
	if (unlikely(ret))
		goto err_init_sysfs;

#if IST30XX_DEBUG
	ret = ist30xx_init_misc_sysfs(data);
	if (unlikely(ret))
		goto err_init_sysfs;
#endif

#if IST30XX_CMCS_TEST
	ret = ist30xx_init_cmcs_sysfs(data);
	if (unlikely(ret))
		goto err_init_sysfs;
#endif

#if IST30XX_TRACKING_MODE
	ret = ist30xx_init_tracking_sysfs(data);
	if (unlikely(ret))
		goto err_init_sysfs;
#endif

	for (j = 0; j < pdata->config_array_size; j++) {
		if (data->tsp_type == pdata->config_array[j].tsp_type) {
			tsp_info("find current_index = %d\n", j);
			data->current_index = j;
			break;
		}
	}

	index = data->current_index;

	tsp_info("TSP IC: %x, TSP Vendor: %x\n", data->chip_id, data->tsp_type);

	data->status.event_mode = false;

	input_mt_init_slots(input_dev, IST30XX_MAX_MT_FINGERS);

	input_dev->name = "ist30xx";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;

	set_bit(EV_ABS, input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, pdata->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, pdata->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, pdata->max_w, 0, 0);

#if IST30XX_USE_KEY
	{
		int i;
		set_bit(EV_KEY, input_dev->evbit);
		set_bit(EV_SYN, input_dev->evbit);
		for (i = 0; i < pdata->key_num; i++)
			set_bit(pdata->config_array[index].key_code[i], input_dev->keybit);
	}
#endif

	input_set_drvdata(input_dev, data);
	ret = input_register_device(input_dev);
	if (unlikely(ret)) {
		//input_free_device(input_dev);
		goto err_input_reg;
	}

	input_dev->enable = ist30xx_input_enable;
	input_dev->disable = ist30xx_input_disable;
	input_dev->enabled = true;


#if IST30XX_INTERNAL_BIN
# if IST30XX_UPDATE_BY_WORKQUEUE
	INIT_DELAYED_WORK(&data->work_fw_update, fw_update_func);
	schedule_delayed_work(&data->work_fw_update, IST30XX_UPDATE_DELAY);
# else
	ret = ist30xx_auto_bin_update(data);
	if (unlikely(ret != 0))
		goto err_firm_update;
# endif
#endif  // IST30XX_INTERNAL_BIN

	if (data->ta_status < 0)
		data->ta_status = 0;

	if (data->noise_mode < 0)
		data->noise_mode = 0;

	ret = ist30xx_get_info(data);
	tsp_info("Get info: %s\n", (ret == 0 ? "success" : "fail"));

	INIT_DELAYED_WORK(&data->work_reset_check, reset_work_func);
#if IST30XX_EVENT_MODE
	INIT_DELAYED_WORK(&data->work_noise_protect, noise_work_func);
	INIT_DELAYED_WORK(&data->work_debug_algorithm, debug_work_func);

	init_timer(&data->event_timer);
	data->event_timer.data = (unsigned long)data;
	data->event_timer.function = timer_handler;
	data->event_timer.expires = jiffies_64 + (EVENT_TIMER_INTERVAL);

	mod_timer(&data->event_timer, get_jiffies_64() + EVENT_TIMER_INTERVAL);
#endif

	data->power_supply_notifier.notifier_call = ist30xx_power_supply_event;
	register_power_supply_notifier(&data->power_supply_notifier);

	data->initialized = 1;

	return 0;

#if IST30XX_INTERNAL_BIN
err_firm_update:
	tsp_info("ChipID: %x\n", data->chip_id);
	input_unregister_device(input_dev);
#endif
err_input_reg:
err_init_sysfs:
err_read_chip_id:
	ist30xx_disable_irq(data);
	free_irq(client->irq, data);
err_req_irq:
err_init_drv:
	data->status.event_mode = false;
	tsp_err("Error, ist30xx init driver\n");
	ist30xx_power_off(data);
free_reset_gpio:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
free_irq_gpio:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_alloc_input_dev:
	input_free_device(input_dev);
err_alloc_dev:
	tsp_err("Error, ist30xx mem free\n");
	kfree(data);
	return ret;
}


static int __devexit ist30xx_remove(struct i2c_client *client)
{
	struct ist30xx_data *data = i2c_get_clientdata(client);

	ist30xx_disable_irq(data);
	free_irq(client->irq, data);
	ist30xx_power_off(data);

	unregister_power_supply_notifier(&data->power_supply_notifier);
	input_unregister_device(data->input_dev);

	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);

	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);

	input_free_device(data->input_dev);
	kfree(data);

	return 0;
}


static struct i2c_device_id ist30xx_idtable[] = {
	{ IST30XX_DEV_NAME, 0 },
	{},
};


MODULE_DEVICE_TABLE(i2c, ist30xx_idtable);

#ifdef CONFIG_OF
static struct of_device_id ist30xx_match_table[] = {
	{ .compatible = "ist30xx", },
	{ },
};
#else
#define ist30xx_match_table NULL
#endif


static struct i2c_driver ist30xx_i2c_driver = {
	.id_table		= ist30xx_idtable,
	.probe			= ist30xx_probe,
	.remove			= __devexit_p(ist30xx_remove),
	.driver			= {
		.owner		= THIS_MODULE,
		.name		= IST30XX_DEV_NAME,
		.of_match_table = ist30xx_match_table,
	},
};


static int __init ist30xx_init(void)
{
	pr_info("#### init for imagis #####\n");
	return i2c_add_driver(&ist30xx_i2c_driver);
}


static void __exit ist30xx_exit(void)
{
	i2c_del_driver(&ist30xx_i2c_driver);
}

module_init(ist30xx_init);
module_exit(ist30xx_exit);

MODULE_DESCRIPTION("Imagis IST30XX touch driver");
MODULE_LICENSE("GPL");
