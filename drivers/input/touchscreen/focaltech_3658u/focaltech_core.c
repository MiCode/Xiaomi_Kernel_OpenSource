/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
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
/*****************************************************************************
*
* File Name: focaltech_core.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract: entrance for focaltech ts driver
*
* Version: V1.0
*
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/times.h>
#include <linux/time.h>

#if defined(CONFIG_DRM)
#include <drm/mi_disp_notifier.h>
#elif defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define FTS_SUSPEND_LEVEL 1     /* Early-suspend level */
#endif

#include "focaltech_core.h"

#ifdef FTS_XIAOMI_TOUCHFEATURE
#include "../xiaomi/xiaomi_touch.h"
#endif

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_NAME                     "fts_ts"
#define INTERVAL_READ_REG                   200  /* unit:ms */
#define TIMEOUT_READ_REG                    1000 /* unit:ms */
#if FTS_POWER_SOURCE_CUST_EN
#define FTS_VTG_MIN_UV                      3300000
#define FTS_VTG_MAX_UV                      3300000
#define FTS_I2C_VTG_MIN_UV                  1800000
#define FTS_I2C_VTG_MAX_UV                  1800000
#endif

#define SPI_DUMMY_BYTE                      3
#define SPI_CMD_BYTE                        4
#define SPI_CRC_BYTE                        2
#define SPI_HEADER_BYTE ((SPI_CMD_BYTE) + (SPI_DUMMY_BYTE) + (SPI_CRC_BYTE))
#define SPI_RETRY_NUMBER                    3
#define CS_HIGH_DELAY                       150 /* unit: us */
#define DATA_CRC_EN                         0x20
#define WRITE_CMD                           0x00
#define READ_CMD                            (0x80 | DATA_CRC_EN)
#define OLED_JUDGE_ID					(17+307)
#define TS_TA_INT_ID					(10+307)
#define HT_AFE_START						0x50
#define HT_AFE_STOP							0x51

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_ts_data *fts_data;

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static int fts_ts_suspend(struct device *dev);
static int fts_ts_resume(struct device *dev);
#ifdef FTS_XIAOMI_TOUCHFEATURE
static int fts_read_palm_data(void);
#endif

/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*         need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int fts_wait_tp_to_valid(void)
{
	int ret = 0;
	int cnt = 0;
	u8 idh = 0;
	u8 idl = 0;
	u8 chip_idh = fts_data->ic_info.ids.chip_idh;
	u8 chip_idl = fts_data->ic_info.ids.chip_idl;

	do {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &idh);
		ret = fts_read_reg(FTS_REG_CHIP_ID2, &idl);
		if ((ret < 0) || (idh != chip_idh) || (idl != chip_idl)) {
			FTS_DEBUG("TP Not Ready,ReadData:0x%02x%02x", idh, idl);
		} else if ((idh == chip_idh) && (idl == chip_idl)) {
			FTS_INFO("TP Ready,Device ID:0x%02x%02x", idh, idl);
			return 0;
		}
		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	return -EIO;
}

/*****************************************************************************
*  Name: fts_tp_state_recovery
*  Brief: Need execute this function when reset
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_tp_state_recovery(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	/* wait tp stable */
	fts_wait_tp_to_valid();
	/* recover TP charger state 0x8B */
	/* recover TP glove state 0xC0 */
	/* recover TP cover state 0xC1 */
	fts_ex_mode_recovery(ts_data);
	/* recover TP gesture state 0xD0 */
	fts_gesture_recovery(ts_data);
	FTS_FUNC_EXIT();
}

int fts_reset_proc(int hdelayms)
{
	FTS_DEBUG("tp reset");
	gpio_direction_output(fts_data->pdata->reset_gpio, 0);
	msleep(1);
	gpio_direction_output(fts_data->pdata->reset_gpio, 1);
	if (hdelayms) {
		msleep(hdelayms);
	}

	return 0;
}

void fts_irq_disable(void)
{
	unsigned long irqflags;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_data->irq_lock, irqflags);

	if (!fts_data->irq_disabled) {
		disable_irq_nosync(fts_data->irq);
		fts_data->irq_disabled = true;
	}

	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

void fts_irq_enable(void)
{
	unsigned long irqflags = 0;

	FTS_FUNC_ENTER();
	spin_lock_irqsave(&fts_data->irq_lock, irqflags);

	if (fts_data->irq_disabled) {
		enable_irq(fts_data->irq);
		fts_data->irq_disabled = false;
	}

	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);
	FTS_FUNC_EXIT();
}

void fts_hid2std(void)
{
	int ret = 0;
	u8 buf[3] = {0xEB, 0xAA, 0x09};

	if (fts_data->bus_type != BUS_TYPE_I2C)
		return;

	ret = fts_write(buf, 3);
	if (ret < 0) {
		FTS_ERROR("hid2std cmd write fail");
	} else {
		msleep(10);
		buf[0] = buf[1] = buf[2] = 0;
		ret = fts_read(NULL, 0, buf, 3);
		if (ret < 0) {
			FTS_ERROR("hid2std cmd read fail");
		} else if ((0xEB == buf[0]) && (0xAA == buf[1]) && (0x08 == buf[2])) {
			FTS_DEBUG("hidi2c change to stdi2c successful");
		} else {
			FTS_DEBUG("hidi2c change to stdi2c not support or fail");
		}
	}
}

static int fts_get_chip_types(
	struct fts_ts_data *ts_data,
	u8 id_h, u8 id_l, bool fw_valid)
{
	int i = 0;
	struct ft_chip_t ctype[] = FTS_CHIP_TYPE_MAPPING;
	u32 ctype_entries = sizeof(ctype) / sizeof(struct ft_chip_t);

	if ((0x0 == id_h) || (0x0 == id_l)) {
		FTS_ERROR("id_h/id_l is 0");
		return -EINVAL;
	}

	FTS_DEBUG("verify id:0x%02x%02x", id_h, id_l);
	for (i = 0; i < ctype_entries; i++) {
		if (VALID == fw_valid) {
			if ((id_h == ctype[i].chip_idh) && (id_l == ctype[i].chip_idl))
				break;
		} else {
			if (((id_h == ctype[i].rom_idh) && (id_l == ctype[i].rom_idl))
				|| ((id_h == ctype[i].pb_idh) && (id_l == ctype[i].pb_idl))
				|| ((id_h == ctype[i].bl_idh) && (id_l == ctype[i].bl_idl)))
				break;
		}
	}

	if (i >= ctype_entries) {
		return -ENODATA;
	}

	ts_data->ic_info.ids = ctype[i];
	return 0;
}

static int fts_read_bootid(struct fts_ts_data *ts_data, u8 *id)
{
	int ret = 0;
	u8 chip_id[2] = { 0 };
	u8 id_cmd[4] = { 0 };
	u32 id_cmd_len = 0;

	id_cmd[0] = 0xF1;
	id_cmd[1] = 0x50;
	ret = fts_write(id_cmd, 2);
	if (ret < 0) {
		FTS_ERROR("Write 0x50 to F1 fail");
		return ret;
	}

	id_cmd[0] = FTS_CMD_START1;
	id_cmd[1] = FTS_CMD_START2;
	ret = fts_write(id_cmd, 2);
	if (ret < 0) {
		FTS_ERROR("start cmd write fail");
		return ret;
	}

	msleep(FTS_CMD_START_DELAY);
	id_cmd[0] = FTS_CMD_READ_ID;
	id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
	if (ts_data->ic_info.is_incell)
		id_cmd_len = FTS_CMD_READ_ID_LEN_INCELL;
	else
		id_cmd_len = FTS_CMD_READ_ID_LEN;
	ret = fts_read(id_cmd, id_cmd_len, chip_id, 2);
	if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
		FTS_ERROR("read boot id fail,read:0x%02x%02x", chip_id[0], chip_id[1]);
		return -EIO;
	}

	id[0] = chip_id[0];
	id[1] = chip_id[1];
	return 0;
}

/*****************************************************************************
* Name: fts_get_ic_information
* Brief: read chip id to get ic information, after run the function, driver w-
*        ill know which IC is it.
*        If cant get the ic information, maybe not focaltech's touch IC, need
*        unregister the driver
* Input:
* Output:
* Return: return 0 if get correct ic information, otherwise return error code
*****************************************************************************/
static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int cnt = 0;
	u8 chip_id[2] = { 0 };

	ts_data->ic_info.is_incell = FTS_CHIP_IDC;
	ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;


	do {
		ret = fts_read_reg(FTS_REG_CHIP_ID, &chip_id[0]);
		ret = fts_read_reg(FTS_REG_CHIP_ID2, &chip_id[1]);
		if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
			FTS_DEBUG("chip id read invalid, read:0x%02x%02x",
					  chip_id[0], chip_id[1]);
		} else {
			ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], VALID);
			if (!ret)
				break;
			else
				FTS_DEBUG("TP not ready, read:0x%02x%02x",
						  chip_id[0], chip_id[1]);
		}

		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	if ((cnt * INTERVAL_READ_REG) >= TIMEOUT_READ_REG) {
		FTS_INFO("fw is invalid, need read boot id");
		if (ts_data->ic_info.hid_supported) {
			fts_hid2std();
		}


		ret = fts_read_bootid(ts_data, &chip_id[0]);
		if (ret <  0) {
			FTS_ERROR("read boot id fail");
			return ret;
		}

		ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], INVALID);
		if (ret < 0) {
			FTS_ERROR("can't get ic informaton");
			return ret;
		}
	}

	FTS_INFO("get ic information, chip id = 0x%02x%02x",
			 ts_data->ic_info.ids.chip_idh, ts_data->ic_info.ids.chip_idl);

	return 0;
}


/*****************************************************************************
*  Reprot related
*****************************************************************************/
static void fts_show_touch_buffer(u8 *data, int datalen)
{
	int i = 0;
	int count = 0;
	char *tmpbuf = NULL;

	tmpbuf = kzalloc(1024, GFP_KERNEL);
	if (!tmpbuf) {
		FTS_ERROR("tmpbuf zalloc fail");
		return;
	}

	for (i = 0; i < datalen; i++) {
		count += snprintf(tmpbuf + count, 1024 - count, "%02X,", data[i]);
		if (count >= 1024)
			break;
	}
	FTS_DEBUG("point buffer:%s", tmpbuf);

	if (tmpbuf) {
		kfree(tmpbuf);
		tmpbuf = NULL;
	}
}

void fts_release_all_finger(void)
{
	struct input_dev *input_dev = fts_data->input_dev;
#if FTS_MT_PROTOCOL_B_EN
	u32 finger_count = 0;
	u32 max_touches = fts_data->pdata->max_touch_number;
#endif

	FTS_FUNC_ENTER();
	mutex_lock(&fts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
	for (finger_count = 0; finger_count < max_touches; finger_count++) {
		input_mt_slot(input_dev, finger_count);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
	}
#else
	input_mt_sync(input_dev);
#endif
	input_report_key(input_dev, BTN_TOUCH, 0);
	input_sync(input_dev);

	fts_data->touchs = 0;
	fts_data->key_state = 0;
	mutex_unlock(&fts_data->report_mutex);
	FTS_FUNC_EXIT();
}

/*****************************************************************************
* Name: fts_input_report_key
* Brief: process key events,need report key-event if key enable.
*        if point's coordinate is in (x_dim-50,y_dim-50) ~ (x_dim+50,y_dim+50),
*        need report it to key event.
*        x_dim: parse from dts, means key x_coordinate, dimension:+-50
*        y_dim: parse from dts, means key y_coordinate, dimension:+-50
* Input:
* Output:
* Return: return 0 if it's key event, otherwise return error code
*****************************************************************************/
static int fts_input_report_key(struct fts_ts_data *data, int index)
{
	int i = 0;
	int x = data->events[index].x;
	int y = data->events[index].y;
	int *x_dim = &data->pdata->key_x_coords[0];
	int *y_dim = &data->pdata->key_y_coords[0];

	if (!data->pdata->have_key) {
		return -EINVAL;
	}
	for (i = 0; i < data->pdata->key_number; i++) {
		if ((x >= x_dim[i] - FTS_KEY_DIM) && (x <= x_dim[i] + FTS_KEY_DIM) &&
			(y >= y_dim[i] - FTS_KEY_DIM) && (y <= y_dim[i] + FTS_KEY_DIM)) {
			if (EVENT_DOWN(data->events[index].flag)
				&& !(data->key_state & (1 << i))) {
				input_report_key(data->input_dev, data->pdata->keys[i], 1);
				data->key_state |= (1 << i);
				FTS_DEBUG("Key%d(%d,%d) DOWN!", i, x, y);
			} else if (EVENT_UP(data->events[index].flag)
					   && (data->key_state & (1 << i))) {
				input_report_key(data->input_dev, data->pdata->keys[i], 0);
				data->key_state &= ~(1 << i);
				FTS_DEBUG("Key%d(%d,%d) Up!", i, x, y);
			}
			return 0;
		}
	}
	return -EINVAL;
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_report_b(struct fts_ts_data *data)
{
	int i = 0;
	int uppoint = 0;
	int touchs = 0;
	bool va_reported = false;
	u32 max_touch_num = data->pdata->max_touch_number;
	struct ts_event *events = data->events;

	for (i = 0; i < data->touch_point; i++) {
		if (fts_input_report_key(data, i) == 0) {
			continue;
		}

		va_reported = true;
		input_mt_slot(data->input_dev, events[i].id);

		if (EVENT_DOWN(events[i].flag)) {
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);

#if FTS_REPORT_PRESSURE_EN
			if (events[i].p <= 0) {
				events[i].p = 0x3f;
			}
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
			if (events[i].area <= 0) {
				events[i].area = 0x09;
			}
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

			touchs |= BIT(events[i].id);
			data->touchs |= BIT(events[i].id);

			if ((data->log_level >= 2) ||
				((1 == data->log_level) && (FTS_TOUCH_DOWN == events[i].flag))) {
				FTS_DEBUG("[B]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
						  events[i].id,
						  events[i].x, events[i].y,
						  events[i].p, events[i].area);
			}
		} else {
			uppoint++;
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
			data->touchs &= ~BIT(events[i].id);
			if (data->log_level >= 1) {
				FTS_DEBUG("[B]P%d UP!", events[i].id);
			}
		}
	}

	if (unlikely(data->touchs ^ touchs)) {
		for (i = 0; i < max_touch_num; i++)  {
			if (BIT(i) & (data->touchs ^ touchs)) {
				if (data->log_level >= 1) {
					FTS_DEBUG("[B]P%d UP!", i);
				}
				va_reported = true;
				input_mt_slot(data->input_dev, i);
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
			}
		}
	}
	data->touchs = touchs;

	if (va_reported) {
		/* touchs==0, there's no point but key */
		if (EVENT_NO_DOWN(data) || (!touchs)) {
			if (data->log_level >= 1) {
				FTS_DEBUG("[B]Points All Up!");
			}
			input_report_key(data->input_dev, BTN_TOUCH, 0);
		} else {
			input_report_key(data->input_dev, BTN_TOUCH, 1);
		}
	}

	input_sync(data->input_dev);
	return 0;
}

#else
static int fts_input_report_a(struct fts_ts_data *data)
{
	int i = 0;
	int touchs = 0;
	bool va_reported = false;
	struct ts_event *events = data->events;

	for (i = 0; i < data->touch_point; i++) {
		if (fts_input_report_key(data, i) == 0) {
			continue;
		}

		va_reported = true;
		if (EVENT_DOWN(events[i].flag)) {
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, events[i].id);
#if FTS_REPORT_PRESSURE_EN
			if (events[i].p <= 0) {
				events[i].p = 0x3f;
			}
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, events[i].p);
#endif
			if (events[i].area <= 0) {
				events[i].area = 0x09;
			}
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, events[i].area);

			input_report_abs(data->input_dev, ABS_MT_POSITION_X, events[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, events[i].y);

			input_mt_sync(data->input_dev);

			if ((data->log_level >= 2) ||
				((1 == data->log_level) && (FTS_TOUCH_DOWN == events[i].flag))) {
				FTS_DEBUG("[A]P%d(%d, %d)[p:%d,tm:%d] DOWN!",
						  events[i].id,
						  events[i].x, events[i].y,
						  events[i].p, events[i].area);
			}
			touchs++;
		}
	}

	/* last point down, current no point but key */
	if (data->touchs && !touchs) {
		va_reported = true;
	}
	data->touchs = touchs;

	if (va_reported) {
		if (EVENT_NO_DOWN(data)) {
			if (data->log_level >= 1) {
				FTS_DEBUG("[A]Points All Up!");
			}
			input_report_key(data->input_dev, BTN_TOUCH, 0);
			input_mt_sync(data->input_dev);
		} else {
			input_report_key(data->input_dev, BTN_TOUCH, 1);
		}
	}

	input_sync(data->input_dev);
	return 0;
}
#endif

static int fts_read_touchdata(struct fts_ts_data *data)
{
	int ret = 0;
	u8 *buf = data->point_buf;

	memset(buf, 0xFF, data->pnt_buf_size);
	buf[0] = 0x01;

#ifdef FTS_XIAOMI_TOUCHFEATURE
	if (data->palm_sensor_switch)
		fts_read_palm_data();
#endif

	ret = fts_read(buf, 1, buf + 1, data->pnt_buf_size - 1);

	if (ret < 0) {
		FTS_ERROR("touch data(%x) abnormal,ret:%d", buf[1], ret);
		return -EIO;
	}

	if (data->gesture_mode) {
		ret = fts_gesture_readdata(data, buf + FTS_TOUCH_DATA_LEN);
		if (0 == ret) {
			FTS_INFO("succuss to get gesture data in irq handler");
			return 1;
		}
	}

	if (data->log_level >= 3) {
		fts_show_touch_buffer(buf, data->pnt_buf_size);
	}

	return 0;
}

static int fts_read_parse_touchdata(struct fts_ts_data *data)
{
	int ret = 0;
	int i = 0;
	u8 pointid = 0;
	int base = 0;
	struct ts_event *events = data->events;
	int max_touch_num = data->pdata->max_touch_number;
	u8 *buf = data->point_buf;

	ret = fts_read_touchdata(data);
	if (ret) {
		return ret;
	}

	data->point_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;
	data->touch_point = 0;

	if (data->ic_info.is_incell) {
		if ((data->point_num == 0x0F) && (buf[2] == 0xFF) && (buf[3] == 0xFF)
			&& (buf[4] == 0xFF) && (buf[5] == 0xFF) && (buf[6] == 0xFF)) {
			FTS_DEBUG("touch buff is 0xff, need recovery state");
			fts_release_all_finger();
			fts_tp_state_recovery(data);
			return -EIO;
		}
	}

	if (data->point_num > max_touch_num) {
		FTS_INFO("invalid point_num(%d)", data->point_num);
		return -EIO;
	}

	for (i = 0; i < max_touch_num; i++) {
		base = FTS_ONE_TCH_LEN * i;
		pointid = (buf[FTS_TOUCH_ID_POS + base]) >> 4;
		if (pointid >= FTS_MAX_ID)
			break;
		else if (pointid >= max_touch_num) {
			FTS_ERROR("ID(%d) beyond max_touch_number", pointid);
			return -EINVAL;
		}

		data->touch_point++;
		events[i].x = ((buf[FTS_TOUCH_X_H_POS + base] & 0x0F) << 8) +
					  (buf[FTS_TOUCH_X_L_POS + base] & 0xFF);
		events[i].y = ((buf[FTS_TOUCH_Y_H_POS + base] & 0x0F) << 8) +
					  (buf[FTS_TOUCH_Y_L_POS + base] & 0xFF);
		events[i].flag = buf[FTS_TOUCH_EVENT_POS + base] >> 6;
		events[i].id = buf[FTS_TOUCH_ID_POS + base] >> 4;
		events[i].area = buf[FTS_TOUCH_AREA_POS + base] >> 4;
		events[i].p =  buf[FTS_TOUCH_PRE_POS + base];

		if (EVENT_DOWN(events[i].flag) && (data->point_num == 0)) {
			FTS_INFO("abnormal touch data from fw");
			return -EIO;
		}
	}

	if (data->touch_point == 0) {
		FTS_INFO("no touch point information");
		return -EIO;
	}

	return 0;
}

static void fts_irq_read_report(void)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

#if FTS_ESDCHECK_EN
	fts_esdcheck_set_intr(1);
#endif

#if FTS_POINT_REPORT_CHECK_EN
	fts_prc_queue_work(ts_data);
#endif

	ret = fts_read_parse_touchdata(ts_data);
	if (ret == 0) {
		mutex_lock(&ts_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
		fts_input_report_b(ts_data);
#else
		fts_input_report_a(ts_data);
#endif
		mutex_unlock(&ts_data->report_mutex);
	}

	if (ts_data->clicktouch_count && ts_data->touchs) {
		FTS_INFO("%s: update touch data: %d\n", __func__, ts_data->clicktouch_count);
		ts_data->clicktouch_count--;
	} else if (!ts_data->touchs) {
		ts_data->clicktouch_count = ts_data->clicktouch_num;
	}
#if FTS_ESDCHECK_EN
	fts_esdcheck_set_intr(0);
#endif
}

static int fts_read_raw(struct fts_ts_data *ts_data, u8 *data, u32 datalen)
{
	int ret = 0;
	int i = 0;
	u8 *txbuf = ts_data->bus_tx_buf;
	u8 *rxbuf = ts_data->bus_rx_buf;
	u32 txlen_need = datalen + SPI_HEADER_BYTE;
	u32 txlen = 0;
	u8 ctrl = READ_CMD;
	u32 dp = 0;
	u8 cmd = HT_CMD_GET_FRAME;

	if (!cmd  || !data || !datalen || (txlen_need > PAGE_SIZE)) {
		FTS_ERROR("cmd/cmdlen/data/datalen(%d) is invalid", datalen);
		return -EINVAL;
	}
	memset(txbuf, 0x0, txlen_need);
	memset(rxbuf, 0x0, txlen_need);
	txbuf[txlen++] = cmd;
	txbuf[txlen++] = ctrl;
	txbuf[txlen++] = (datalen >> 8) & 0xFF;
	txbuf[txlen++] = datalen & 0xFF;
	dp = txlen + SPI_DUMMY_BYTE;
	txlen = dp + datalen;
	if (ctrl & DATA_CRC_EN) {
		txlen = txlen + SPI_CRC_BYTE;
	}

	for (i = 0; i < SPI_RETRY_NUMBER; i++) {
		ret = fts_spi_transfer(txbuf, rxbuf, txlen);
		if ((0 == ret) && ((rxbuf[3] & 0xA0) == 0)) {
			memcpy(data, &rxbuf[dp], datalen);
			/* crc check */
			if (ctrl & DATA_CRC_EN) {
			    ret = rdata_check(&rxbuf[dp], txlen - dp);
			    if (ret < 0) {
			        FTS_DEBUG("data read(addr:%x) crc abnormal,retry:%d",
			                  cmd, i);
			        udelay(CS_HIGH_DELAY);
			        continue;
			    }
		    }
		    break;
	    } else {
			FTS_INFO("data read(addr:%x) status:%x,retry:%d,ret:%d",
			          cmd, rxbuf[3], i, ret);
			ret = -EIO;
			udelay(CS_HIGH_DELAY);
		}
	}

	if (ret < 0) {
		FTS_ERROR("data read(addr:%x) %s,status:%x,ret:%d", cmd,
		          (i >= SPI_RETRY_NUMBER) ? "crc abnormal" : "fail",
		          rxbuf[3], ret);
	}

	udelay(CS_HIGH_DELAY);
	return ret;
}

static struct timeval get_timeval(const s64 nsec)
{
	struct timespec ts = ns_to_timespec(nsec);
	struct timeval tv;

	tv.tv_sec = ts.tv_sec;
	tv.tv_usec = (suseconds_t) ts.tv_nsec / 1000;

	return tv;
}

static irqreturn_t fts_irq_handler(int irq, void *data)
{
	int read_size = sizeof(struct tp_raw);
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;

	if ((ts_data->suspended) && (ts_data->pm_suspend)) {
		ret = wait_for_completion_timeout(
				  &ts_data->pm_completion,
				  msecs_to_jiffies(FTS_TIMEOUT_COMERR_PM));
		if (!ret) {
			FTS_ERROR("Bus don't resume from pm(deep),timeout,skip irq");
			return IRQ_HANDLED;
		}
	}
#endif
	pm_stay_awake(fts_data->dev);
	if (fts_data->enable_touch_raw) {
		fts_data->tp_frame.tv = get_timeval(ktime_get());
		fts_read_raw(fts_data, fts_data->tp_frame.tp_raw, read_size);
		fts_data->tp_frame.tv0 = get_timeval(ktime_get());
	} else {
		fts_irq_read_report();
	}
	pm_relax(fts_data->dev);
	return IRQ_HANDLED;
}

static int fts_irq_registration(struct fts_ts_data *ts_data)
{
	int ret = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;

	ts_data->irq = gpio_to_irq(pdata->irq_gpio);
	pdata->irq_gpio_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
	FTS_INFO("irq:%d, flag:%x", ts_data->irq, pdata->irq_gpio_flags);
	ret = request_threaded_irq(ts_data->irq, NULL, fts_irq_handler,
							   pdata->irq_gpio_flags,
							   FTS_DRIVER_NAME, ts_data);

	return ret;
}

static int fts_input_init(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int key_num = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;
	struct input_dev *input_dev;

	FTS_FUNC_ENTER();
	input_dev = input_allocate_device();
	if (!input_dev) {
		FTS_ERROR("Failed to allocate memory for input device");
		return -ENOMEM;
	}

	/* Init and register Input device */
	input_dev->name = FTS_DRIVER_NAME;
	if (ts_data->bus_type == BUS_TYPE_I2C)
		input_dev->id.bustype = BUS_I2C;
	else
		input_dev->id.bustype = BUS_SPI;
	input_dev->dev.parent = ts_data->dev;

	input_set_drvdata(input_dev, ts_data);

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_set_capability(input_dev, EV_KEY, KEY_WAKEUP);
	input_set_capability(input_dev, EV_KEY, KEY_GOTO);
	if (pdata->have_key) {
		FTS_INFO("set key capabilities");
		for (key_num = 0; key_num < pdata->key_number; key_num++)
			input_set_capability(input_dev, EV_KEY, pdata->keys[key_num]);
	}

#if FTS_MT_PROTOCOL_B_EN
	input_mt_init_slots(input_dev, pdata->max_touch_number, INPUT_MT_DIRECT);
#else
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 0x0F, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min, pdata->x_max - 1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min, pdata->y_max - 1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
#if FTS_REPORT_PRESSURE_EN
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
#endif

	ret = input_register_device(input_dev);
	if (ret) {
		FTS_ERROR("Input device registration failed");
		input_set_drvdata(input_dev, NULL);
		input_free_device(input_dev);
		input_dev = NULL;
		return ret;
	}

	ts_data->input_dev = input_dev;

	FTS_FUNC_EXIT();
	return 0;
}

static int fts_report_buffer_init(struct fts_ts_data *ts_data)
{
	int point_num = 0;
	int events_num = 0;

	point_num = FTS_MAX_POINTS_SUPPORT;
	ts_data->pnt_buf_size = FTS_TOUCH_DATA_LEN + FTS_GESTURE_DATA_LEN + FTS_DIFF_DATA_LEN;

	ts_data->point_buf = (u8 *)kzalloc(ts_data->pnt_buf_size + 1, GFP_KERNEL);
	if (!ts_data->point_buf) {
		FTS_ERROR("failed to alloc memory for point buf");
		return -ENOMEM;
	}

	events_num = point_num * sizeof(struct ts_event);
	ts_data->events = (struct ts_event *)kzalloc(events_num, GFP_KERNEL);
	if (!ts_data->events) {
		FTS_ERROR("failed to alloc memory for point events");
		kfree_safe(ts_data->point_buf);
		return -ENOMEM;
	}

	return 0;
}

#if FTS_POWER_SOURCE_CUST_EN
/*****************************************************************************
* Power Control
*****************************************************************************/
#if FTS_PINCTRL_EN
static int fts_pinctrl_init(struct fts_ts_data *ts)
{
	int ret = 0;

	ts->pinctrl = devm_pinctrl_get(ts->dev);
	if (IS_ERR_OR_NULL(ts->pinctrl)) {
		FTS_ERROR("Failed to get pinctrl, please check dts");
		ret = PTR_ERR(ts->pinctrl);
		goto err_pinctrl_get;
	}

	ts->pins_active = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_active");
	if (IS_ERR_OR_NULL(ts->pins_active)) {
		FTS_ERROR("Pin state[active] not found");
		ret = PTR_ERR(ts->pins_active);
		goto err_pinctrl_lookup;
	}

	ts->pins_suspend = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_suspend");
	if (IS_ERR_OR_NULL(ts->pins_suspend)) {
		FTS_ERROR("Pin state[suspend] not found");
		ret = PTR_ERR(ts->pins_suspend);
		goto err_pinctrl_lookup;
	}

	ts->pins_release = pinctrl_lookup_state(ts->pinctrl, "pmx_ts_release");
	if (IS_ERR_OR_NULL(ts->pins_release)) {
		FTS_ERROR("Pin state[release] not found");
		ret = PTR_ERR(ts->pins_release);
	}

	return 0;
err_pinctrl_lookup:
	if (ts->pinctrl) {
		devm_pinctrl_put(ts->pinctrl);
	}
err_pinctrl_get:
	ts->pinctrl = NULL;
	ts->pins_release = NULL;
	ts->pins_suspend = NULL;
	ts->pins_active = NULL;
	return ret;
}

static int fts_pinctrl_select_normal(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl && ts->pins_active) {
		ret = pinctrl_select_state(ts->pinctrl, ts->pins_active);
		if (ret < 0) {
			FTS_ERROR("Set normal pin state error:%d", ret);
		}
	}

	return ret;
}

static int fts_pinctrl_select_suspend(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl && ts->pins_suspend) {
		ret = pinctrl_select_state(ts->pinctrl, ts->pins_suspend);
		if (ret < 0) {
			FTS_ERROR("Set suspend pin state error:%d", ret);
		}
	}

	return ret;
}

static int fts_pinctrl_select_release(struct fts_ts_data *ts)
{
	int ret = 0;

	if (ts->pinctrl) {
		if (IS_ERR_OR_NULL(ts->pins_release)) {
			devm_pinctrl_put(ts->pinctrl);
			ts->pinctrl = NULL;
		} else {
			ret = pinctrl_select_state(ts->pinctrl, ts->pins_release);
			if (ret < 0)
				FTS_ERROR("Set gesture pin state error:%d", ret);
		}
	}

	return ret;
}
#endif /* FTS_PINCTRL_EN */

static int fts_power_source_ctrl(struct fts_ts_data *ts_data, int enable)
{
	int ret = 0;

	if (IS_ERR_OR_NULL(ts_data->vdd)) {
		FTS_ERROR("vdd is invalid");
		return -EINVAL;
	}

	FTS_FUNC_ENTER();
	if (enable) {
		if (ts_data->power_disabled) {
			FTS_DEBUG("regulator enable !");
			gpio_direction_output(ts_data->pdata->reset_gpio, 0);
			msleep(1);
			if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
				ret = regulator_enable(ts_data->vcc_i2c);
				if (ret) {
					FTS_ERROR("enable vcc_i2c regulator failed,ret=%d", ret);
				}
			}
			msleep(1);

			ret = regulator_enable(ts_data->vdd);
			if (ret) {
				FTS_ERROR("enable vdd regulator failed,ret=%d", ret);
			}

			ts_data->power_disabled = false;
		}
	} else {
		if (!ts_data->power_disabled) {
			FTS_DEBUG("regulator disable !");
			gpio_direction_output(ts_data->pdata->reset_gpio, 0);
			msleep(1);
			ret = regulator_disable(ts_data->vdd);
			if (ret) {
				FTS_ERROR("disable vdd regulator failed,ret=%d", ret);
			}
			if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
				ret = regulator_disable(ts_data->vcc_i2c);
				if (ret) {
					FTS_ERROR("disable vcc_i2c regulator failed,ret=%d", ret);
				}
			}
			ts_data->power_disabled = true;
		}
	}

	FTS_FUNC_EXIT();
	return ret;
}

/*****************************************************************************
* Name: fts_power_source_init
* Brief: Init regulator power:vdd/vcc_io(if have), generally, no vcc_io
*        vdd---->vdd-supply in dts, kernel will auto add "-supply" to parse
*        Must be call after fts_gpio_configure() execute,because this function
*        will operate reset-gpio which request gpio in fts_gpio_configure()
* Input:
* Output:
* Return: return 0 if init power successfully, otherwise return error code
*****************************************************************************/
static int fts_power_source_init(struct fts_ts_data *ts_data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	ts_data->vdd = regulator_get(ts_data->dev, "avdd");
	if (IS_ERR_OR_NULL(ts_data->vdd)) {
		ret = PTR_ERR(ts_data->vdd);
		FTS_ERROR("get vdd regulator failed,ret=%d", ret);
		return ret;
	}

	if (regulator_count_voltages(ts_data->vdd) > 0) {
		ret = regulator_set_voltage(ts_data->vdd, FTS_VTG_MIN_UV,
								FTS_VTG_MAX_UV);
		if (ret) {
			FTS_ERROR("vdd regulator set_vtg failed ret=%d", ret);
			regulator_put(ts_data->vdd);
			return ret;
		}
	}

	ts_data->vcc_i2c = regulator_get(ts_data->dev, "iovdd");
	if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
		if (regulator_count_voltages(ts_data->vcc_i2c) > 0) {
			ret = regulator_set_voltage(ts_data->vcc_i2c,
							FTS_I2C_VTG_MIN_UV,
							FTS_I2C_VTG_MAX_UV);
			if (ret) {
				FTS_ERROR("vcc_i2c regulator set_vtg failed,ret=%d", ret);
				regulator_put(ts_data->vcc_i2c);
			}
		}
	}

#if FTS_PINCTRL_EN
	fts_pinctrl_init(ts_data);
	fts_pinctrl_select_normal(ts_data);
#endif

	ts_data->power_disabled = true;
	ret = fts_power_source_ctrl(ts_data, ENABLE);
	if (ret) {
		FTS_ERROR("fail to enable power(regulator)");
	}

	FTS_FUNC_EXIT();
	return ret;
}

static int fts_power_source_exit(struct fts_ts_data *ts_data)
{
#if FTS_PINCTRL_EN
	fts_pinctrl_select_release(ts_data);
#endif

	fts_power_source_ctrl(ts_data, DISABLE);

	if (!IS_ERR_OR_NULL(ts_data->vdd)) {
		if (regulator_count_voltages(ts_data->vdd) > 0)
			regulator_set_voltage(ts_data->vdd, 0, FTS_VTG_MAX_UV);
		regulator_put(ts_data->vdd);
	}

	if (!IS_ERR_OR_NULL(ts_data->vcc_i2c)) {
		if (regulator_count_voltages(ts_data->vcc_i2c) > 0)
			regulator_set_voltage(ts_data->vcc_i2c, 0, FTS_I2C_VTG_MAX_UV);
		regulator_put(ts_data->vcc_i2c);
	}

	return 0;
}

static int fts_power_source_suspend(struct fts_ts_data *ts_data)
{
	int ret = 0;

	ret = fts_power_source_ctrl(ts_data, DISABLE);
	if (ret < 0) {
		FTS_ERROR("power off fail, ret=%d", ret);
	}

#if FTS_PINCTRL_EN
	fts_pinctrl_select_suspend(ts_data);
#endif

	return ret;
}

static int fts_power_source_resume(struct fts_ts_data *ts_data)
{
	int ret = 0;

#if FTS_PINCTRL_EN
	fts_pinctrl_select_normal(ts_data);
#endif

	ret = fts_power_source_ctrl(ts_data, ENABLE);
	if (ret < 0) {
		FTS_ERROR("power on fail, ret=%d", ret);
	}

	return ret;
}
#endif /* FTS_POWER_SOURCE_CUST_EN */

static int fts_gpio_configure(struct fts_ts_data *data)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	/* request irq gpio */
	if (gpio_is_valid(data->pdata->irq_gpio)) {
		ret = gpio_request(data->pdata->irq_gpio, "fts_irq_gpio");
		if (ret) {
			FTS_ERROR("[GPIO]irq gpio request failed");
			goto err_irq_gpio_req;
		}

		ret = gpio_direction_input(data->pdata->irq_gpio);
		if (ret) {
			FTS_ERROR("[GPIO]set_direction for irq gpio failed");
			goto err_irq_gpio_dir;
		}
	}

	/* request reset gpio */
	if (gpio_is_valid(data->pdata->reset_gpio)) {
		ret = gpio_request(data->pdata->reset_gpio, "fts_reset_gpio");
		if (ret) {
			FTS_ERROR("[GPIO]reset gpio request failed");
			goto err_irq_gpio_dir;
		}

		ret = gpio_direction_output(data->pdata->reset_gpio, 0);
		if (ret) {
			FTS_ERROR("[GPIO]set_direction for reset gpio failed");
			goto err_reset_gpio_dir;
		}
	}

	FTS_FUNC_EXIT();
	return 0;

err_reset_gpio_dir:
	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);
err_irq_gpio_dir:
	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);
err_irq_gpio_req:
	FTS_FUNC_EXIT();
	return ret;
}

static int fts_get_dt_coords(struct device *dev, char *name,
							 struct fts_ts_platform_data *pdata)
{
	int ret = 0;
	u32 coords[FTS_COORDS_ARR_SIZE] = { 0 };
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;

	coords_size = prop->length / sizeof(u32);
	if (coords_size != FTS_COORDS_ARR_SIZE) {
		FTS_ERROR("invalid:%s, size:%d", name, coords_size);
		return -EINVAL;
	}

	ret = of_property_read_u32_array(np, name, coords, coords_size);
	if (ret < 0) {
		FTS_ERROR("Unable to read %s, please check dts", name);
		pdata->x_min = FTS_X_MIN_DISPLAY_DEFAULT;
		pdata->y_min = FTS_Y_MIN_DISPLAY_DEFAULT;
		pdata->x_max = FTS_X_MAX_DISPLAY_DEFAULT;
		pdata->y_max = FTS_Y_MAX_DISPLAY_DEFAULT;
		return -ENODATA;
	} else {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	}

	FTS_INFO("display x(%d %d) y(%d %d)", pdata->x_min, pdata->x_max,
			 pdata->y_min, pdata->y_max);
	return 0;
}

static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
	int ret = 0;
	struct device_node *np = dev->of_node;
	u32 temp_val = 0;
	const char *name_tmp;

	FTS_FUNC_ENTER();

	ret = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (ret < 0)
		FTS_ERROR("Unable to get display-coords");

	/* key */
	pdata->have_key = of_property_read_bool(np, "focaltech,have-key");
	if (pdata->have_key) {
		ret = of_property_read_u32(np, "focaltech,key-number", &pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key number undefined!");

		ret = of_property_read_u32_array(np, "focaltech,keys",
										 pdata->keys, pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Keys undefined!");
		else if (pdata->key_number > FTS_MAX_KEYS)
			pdata->key_number = FTS_MAX_KEYS;

		ret = of_property_read_u32_array(np, "focaltech,key-x-coords",
										 pdata->key_x_coords,
										 pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key Y Coords undefined!");

		ret = of_property_read_u32_array(np, "focaltech,key-y-coords",
										 pdata->key_y_coords,
										 pdata->key_number);
		if (ret < 0)
			FTS_ERROR("Key X Coords undefined!");

		FTS_INFO("VK Number:%d, key:(%d,%d,%d), "
				 "coords:(%d,%d),(%d,%d),(%d,%d)",
				 pdata->key_number,
				 pdata->keys[0], pdata->keys[1], pdata->keys[2],
				 pdata->key_x_coords[0], pdata->key_y_coords[0],
				 pdata->key_x_coords[1], pdata->key_y_coords[1],
				 pdata->key_x_coords[2], pdata->key_y_coords[2]);
	}

	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio",
						0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
		FTS_ERROR("Unable to get reset_gpio");

	pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio",
					  0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
		FTS_ERROR("Unable to get irq_gpio");

	ret = of_property_read_u32(np, "focaltech,max-touch-number", &temp_val);
	if (ret < 0) {
		FTS_ERROR("Unable to get max-touch-number, please check dts");
		pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
	} else {
		if (temp_val < 2)
			pdata->max_touch_number = 2; /* max_touch_number must >= 2 */
		else if (temp_val > FTS_MAX_POINTS_SUPPORT)
			pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
		else
			pdata->max_touch_number = temp_val;
	}

	ret = of_property_read_u32(np, "focaltech,open-min", &temp_val);
	if (ret < 0) {
		FTS_ERROR("Unable to get open-min, please check dts");
		pdata->open_min = FTS_TEST_OPEN_MIN;
	} else {
		pdata->open_min = temp_val;
	}

	memset(pdata->avdd_name, 0, sizeof(pdata->avdd_name));
	ret = of_property_read_string(np, "goodix,avdd-name", &name_tmp);
	if (!ret) {
		FTS_ERROR("avdd name form dt: %s", name_tmp);
		if (strlen(name_tmp) < sizeof(pdata->avdd_name))
			strncpy(pdata->avdd_name,
				name_tmp, sizeof(pdata->avdd_name));
		else
			FTS_ERROR("invalied avdd name length: %ld > %ld",
				strlen(name_tmp),
				sizeof(pdata->avdd_name));
	}
	
	memset(pdata->iovdd_name, 0, sizeof(pdata->iovdd_name));
	ret = of_property_read_string(np, "goodix,iovdd-name", &name_tmp);
	if (!ret) {
		FTS_ERROR("iovdd name form dt: %s", name_tmp);
		if (strlen(name_tmp) < sizeof(pdata->iovdd_name))
			strncpy(pdata->iovdd_name,
				name_tmp, sizeof(pdata->iovdd_name));
		else
			FTS_ERROR("invalied iovdd name length: %ld > %ld",
				strlen(name_tmp),
				sizeof(pdata->iovdd_name));
	}
	

	FTS_INFO("max touch number:%d, irq gpio:%d, reset gpio:%d",
			 pdata->max_touch_number, pdata->irq_gpio, pdata->reset_gpio);

	FTS_FUNC_EXIT();
	return 0;
}

static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data,
								  resume_work);

	fts_ts_resume(ts_data->dev);
}

#define CHANGE_FPS	  0xF628
/*return 1 means notify was handled by this function*/
static int check_fps(unsigned long event, void *data)
{
	bool legally = true;
	int fps = *(int *)data;
	u8 cmd = 0;
	bool suspend = fts_data->suspended;

	if (CHANGE_FPS == event) {
		switch (fps) {
			case 60:
				cmd = 0x3c;
			break;
			case 90:
				cmd = 0x5a;
			break;
			case 120:
				cmd = 0x78;
			break;
			case 144:
				cmd = 0x90;
			break;
			default :
				legally = false;
				FTS_ERROR("fps value illegal, fps:%d", fps);
			break;
		}
		if (legally) {
			FTS_INFO("config touch work with %d fps", fps);
			if (suspend)
				FTS_INFO("tp in suspend status now, config is put-off to resume");
			else {
				if (!fts_write_reg(0x8a, cmd))
					FTS_INFO("config fps successful");
			}
		}
		fts_data->fps_state = fps;
		fts_data->fps_cmd = cmd;
		return 1;
	} else {
		return 0;
	}
}

#if defined(CONFIG_DRM)
static int drm_notifier_callback(struct notifier_block *self,
		unsigned long event, void *data)
{
	struct fts_ts_data *ts_data = container_of(self, struct fts_ts_data,
					fb_notif);
	struct mi_disp_notifier *evdata = data;
	int blank;

	if (check_fps(event, evdata->data))
	return 0;

	if (!ts_data || !evdata || !evdata->data) {
		FTS_ERROR("evdata is null");
		goto exit;
	}

	if (evdata->disp_id != MI_DISPLAY_PRIMARY) {
		FTS_INFO("not primary display\n");
		return NOTIFY_OK;
	}

	blank = *(int *)(evdata->data);
	FTS_INFO("notifier tp event:%d, code:%d\n", event, blank);

	if (event == MI_DISP_DPMS_EVENT && blank == MI_DISP_DPMS_ON) {
		FTS_INFO("FB_BLANK_UNBLANK!\n");
		queue_work(fts_data->ts_workqueue, &fts_data->resume_work);

	} else if (event == MI_DISP_DPMS_EARLY_EVENT &&
			(blank == MI_DISP_DPMS_POWERDOWN ||
			 blank == MI_DISP_DPMS_LP1 ||
			 blank == MI_DISP_DPMS_LP2)) {
		FTS_INFO("FB_BLANK %s\n",
				blank == MI_DISP_DPMS_POWERDOWN ? "POWER DOWN" : "LP");

		cancel_work_sync(&fts_data->resume_work);
		fts_ts_suspend(ts_data->dev);
	}

exit:
	return NOTIFY_OK;
}
#elif defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
								unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank = NULL;
	struct fts_ts_data *ts_data = container_of(self, struct fts_ts_data,
								fb_notif);

	if (!evdata) {
		FTS_ERROR("evdata is null");
		return 0;
	}

	if (!(event == FB_EARLY_EVENT_BLANK || event == FB_EVENT_BLANK)) {
		FTS_INFO("event(%lu) do not need process\n", event);
		return 0;
	}

	blank = evdata->data;
	FTS_INFO("FB event:%lu,blank:%d", event, *blank);
	switch (*blank) {
	case FB_BLANK_UNBLANK:
		if (FB_EARLY_EVENT_BLANK == event) {
			FTS_INFO("resume: event = %lu, not care\n", event);
		} else if (FB_EVENT_BLANK == event) {
			queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
		}
		break;
	case FB_BLANK_POWERDOWN:
		if (FB_EARLY_EVENT_BLANK == event) {
			cancel_work_sync(&fts_data->resume_work);
			fts_ts_suspend(ts_data->dev);
		} else if (FB_EVENT_BLANK == event) {
			FTS_INFO("suspend: event = %lu, not care\n", event);
		}
		break;
	default:
		FTS_INFO("FB BLANK(%d) do not need process\n", *blank);
		break;
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void fts_ts_early_suspend(struct early_suspend *handler)
{
	struct fts_ts_data *ts_data = container_of(handler, struct fts_ts_data,
								  early_suspend);

	cancel_work_sync(&fts_data->resume_work);
	fts_ts_suspend(ts_data->dev);
}

static void fts_ts_late_resume(struct early_suspend *handler)
{
	struct fts_ts_data *ts_data = container_of(handler, struct fts_ts_data,
								  early_suspend);

	queue_work(fts_data->ts_workqueue, &fts_data->resume_work);
}
#endif


static int fts_power_supply_event(struct notifier_block *nb,
			unsigned long event, void *ptr)
{
	struct fts_ts_data *ts_data =
		container_of(nb, struct fts_ts_data, power_supply_notifier);
	if (ts_data)
		queue_work(ts_data->ts_workqueue, &ts_data->power_supply_work);

	return 0;
}

static void fts_power_supply_work(struct work_struct *work)
{
	int ret = 0;
	bool is_usb_exist;
	struct fts_ts_data *ts_data =
		container_of(work, struct fts_ts_data, power_supply_work);

	if (ts_data == NULL)
		return;
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	if (ts_data->pm_suspend) {
		FTS_ERROR("TP is in suspend mode, don't set usb status!");
		return;
	}
#endif
	pm_stay_awake(ts_data->dev);
#ifdef CONFIG_QGKI_SYSTEM
	is_usb_exist = !!power_supply_is_system_supplied();
#else
	is_usb_exist = false;
#endif
	if (is_usb_exist == ts_data->is_usb_exist)
		goto out;
	ret = fts_write_reg(FTS_REG_CHARGER_MODE_EN, is_usb_exist);
	if (ret < 0) {
		FTS_ERROR("failed to set power supply status:%d", is_usb_exist);
	} else {
		ts_data->is_usb_exist = is_usb_exist;
		FTS_INFO("succes to set power supply status:%d", is_usb_exist);
	}
out:
	pm_relax(ts_data->dev);
}

#ifdef FTS_XIAOMI_TOUCHFEATURE
static struct xiaomi_touch_interface xiaomi_touch_interfaces;

static void fts_restore_mode_value(int mode, int value_type)
{
	xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
		xiaomi_touch_interfaces.touch_mode[mode][value_type];
}

static void fts_restore_normal_mode()
{
	int i;

	for (i = 0; i < Touch_Panel_Orientation; i++)
		fts_restore_mode_value(i, GET_DEF_VALUE);
}

static void fts_write_touchfeature_reg(int mode)
{
	int ret = 0;
	u8 temp_value = (u8)xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];

	switch(mode) {
	case Touch_Game_Mode:
		ret = fts_write_reg(FTS_REG_GAME_MODE_EN, temp_value);
		if (!temp_value)
			fts_restore_normal_mode();
		break;
	case Touch_UP_THRESHOLD:
		ret = fts_write_reg(FTS_REG_SENSIVITY, temp_value);
		break;
	case Touch_Tolerance:
		ret = fts_write_reg(FTS_REG_THDIFF, 3 - temp_value);
		break;
	case Touch_Edge_Filter:
		ret = fts_write_reg(FTS_REG_EDGE_FILTER_LEVEL, temp_value);
		break;
	case Touch_Panel_Orientation:
		if (PANEL_ORIENTATION_DEGREE_90 == temp_value)
			temp_value = 1;
		else if (PANEL_ORIENTATION_DEGREE_270 == temp_value)
			temp_value = 2;
		else
			temp_value = 0;
		ret = fts_write_reg(FTS_REG_ORIENTATION, temp_value);
		break;
	case Touch_Active_MODE:
		break;
	default:
		ret = -1;
		break;
	}
	if (ret < 0) {
		FTS_ERROR("write mode:%d reg failed", mode);
	} else {
		FTS_INFO("write mode:%d value:%d success", mode, temp_value);
		xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE] = temp_value;
	}
}

static void fts_update_touchmode_data(struct fts_ts_data *ts_data)
{
	int mode = 0;
	u8 temp_value;

#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	if (ts_data && ts_data->pm_suspend) {
		FTS_ERROR("SYSTEM is in suspend mode, don't set touch mode data");
		return;
	}
#endif
	pm_stay_awake(ts_data->dev);
	mutex_lock(&ts_data->cmd_update_mutex);
	for (mode = 0; mode <= Touch_Panel_Orientation; mode++) {
		temp_value = xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
		if (temp_value == xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE]) {
			/* FTS_INFO("cur value has been set, mode = %d value: %d", mode, temp_value);*/
			continue;
		}
		fts_write_touchfeature_reg(mode);
	}
	mutex_unlock(&ts_data->cmd_update_mutex);
	pm_relax(ts_data->dev);
}

static void fts_update_gesture_state(struct fts_ts_data *ts_data, int bit, bool enable)
{
	if (ts_data->suspended) {
		FTS_ERROR("TP is suspended, do not update gesture state");
		return;
	}
	mutex_lock(&ts_data->input_dev->mutex);
	if (enable)
		ts_data->gesture_status |= 1 << bit;
	else
		ts_data->gesture_status &= ~(1 << bit);
	FTS_INFO("gesture state:0x%02X", ts_data->gesture_status);
	ts_data->gesture_mode = ts_data->gesture_status != 0 ? ENABLE : DISABLE;
	mutex_unlock(&ts_data->input_dev->mutex);
}

static void fts_power_status_handler(struct fts_ts_data *ts_data, int value)
{
	if (value) {
		queue_work(ts_data->ts_workqueue, &ts_data->resume_work);
	} else {
		cancel_work_sync(&ts_data->resume_work);
		fts_ts_suspend(ts_data->dev);
	}
}

static int fts_set_cur_value(int mode, int value)
{
	if (!fts_data || mode < 0) {
		FTS_ERROR("Error, fts_data is NULL or the parameter is incorrect");
		return -1;
	}
	FTS_INFO("mode:%d, value:%d", mode, value);
	if (mode == Touch_Doubletap_Mode && value >= 0) {
		fts_update_gesture_state(fts_data, GESTURE_DOUBLETAP, value != 0 ? true : false);
		return 0;
	}
	if (mode == Touch_Aod_Enable && value >= 0) {
		fts_update_gesture_state(fts_data, GESTURE_AOD, value != 0 ? true : false);
		return 0;
	}
	if (mode == Touch_Power_Status && value >= 0) {
		fts_power_status_handler(fts_data, value);
		return 0;
	}
	/* orientation for IC:
	 * 0: vertival
	 * 1: left horizontal at normal mode
	 * 2: right horizontal at normal mode
	 * 3: left horizontal at game mode
	 * 4: right horizontal at game mode
	 */


	if (mode > Touch_Panel_Orientation) {
		FTS_ERROR("game mode:%d don't support", mode);
		return -EINVAL;
	}

	xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] = value;
	if (xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] >
			xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE]) {

		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];

	} else if (xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] <
			xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE]) {

		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
				xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
	}

	fts_update_touchmode_data(fts_data);
	return 0;
}

static int fts_reset_mode(int mode)
{
	if (mode == Touch_Game_Mode) {
		fts_restore_normal_mode();
	} else if (mode < Touch_Report_Rate) {
		fts_restore_mode_value(mode, GET_DEF_VALUE);
	} else {
		FTS_ERROR("mode:%d don't support");
	}

	FTS_INFO("mode:%d reset", mode);

	fts_update_touchmode_data(fts_data);

	return 0;
}

static int fts_get_mode_value(int mode, int value_type)
{
	int value = -1;

	if (mode < Touch_Mode_NUM && mode >= 0) {
		value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
		/* FTS_INFO("mode:%d, value_type:%d, value:%d", mode, value_type, value); */
	} else {
		FTS_ERROR("mode:%d don't support");
	}

	return value;
}

static int fts_get_mode_all(int mode, int *value)
{
	if (mode < Touch_Mode_NUM && mode >= 0) {
		value[0] = xiaomi_touch_interfaces.touch_mode[mode][GET_CUR_VALUE];
		value[1] = xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
		value[2] = xiaomi_touch_interfaces.touch_mode[mode][GET_MIN_VALUE];
		value[3] = xiaomi_touch_interfaces.touch_mode[mode][GET_MAX_VALUE];
	} else {
		FTS_ERROR("mode:%d don't support", mode);
	}
	FTS_INFO("mode:%d, value:%d:%d:%d:%d", mode,
				value[0], value[1], value[2], value[3]);
	return 0;
}

static int fts_read_palm_data(void)
{
	int ret = 0;
	u8 reg_value;

	if (fts_data == NULL)
		return -EINVAL;

	ret = fts_read_reg(FTS_PALM_DATA, &reg_value);
	if (ret < 0) {
		FTS_ERROR("read palm data error\n");
		return -EINVAL;
	}
	if (reg_value == 0x40)
		update_palm_sensor_value(1);
	else if (reg_value == 0x80)
		update_palm_sensor_value(0);
	if (reg_value == 0x40 || reg_value == 0x80)
		FTS_INFO("update palm data:0x%02X", reg_value);
	return 0;
}

static int fts_palm_sensor_cmd(int value)
{
	int ret = 0;

	ret = fts_write_reg(FTS_PALM_EN, value ? FTS_PALM_ON : FTS_PALM_OFF);

	if (ret < 0)
		FTS_ERROR("Set palm sensor switch failed!\n");
	else
		FTS_INFO("Set palm sensor switch: %d\n", value);

	return ret;
}

static int fts_palm_sensor_write(int value)
{
	int ret = 0;

	if (fts_data == NULL)
		return -EINVAL;

	fts_data->palm_sensor_switch = value;

	if (fts_data->suspended)
		return 0;

	ret = fts_palm_sensor_cmd(value);
	if (ret < 0)
		FTS_ERROR("set palm sensor cmd failed: %d\n", value);
	return ret;
}

static u8 fts_panel_vendor_read(void)
{
	if (fts_data)
		return fts_data->lockdown_info[0];
	else
		return 0;
}

static u8 fts_panel_color_read(void)
{
	if (fts_data)
		return fts_data->lockdown_info[2];
	else
		return 0;
}

static u8 fts_panel_display_read(void)
{
	if (fts_data)
		return fts_data->lockdown_info[1];
	else
		return 0;
}

static char fts_touch_vendor_read(void)
{
	return '3';
}

static void fts_init_touchmode_data(void)
{
	/* Touch Game Mode Switch */
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Game_Mode][GET_CUR_VALUE] = 0;

	/* Acitve Mode */
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MAX_VALUE] = 1;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Active_MODE][GET_CUR_VALUE] = 0;

	/* UP_THRESHOLD */
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = 0;

	/*  Tolerance */
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = 0;

	/* edge orientation*/
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][SET_CUR_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_CUR_VALUE] = 0;

	/* edge filter area*/
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MAX_VALUE] = 3;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_MIN_VALUE] = 0;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][SET_CUR_VALUE] = 2;
	xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_CUR_VALUE] = 2;

	FTS_INFO("touchfeature value init done");

	return;
}

static void fts_init_xiaomi_touchfeature(struct fts_ts_data *ts_data)
{
	mutex_init(&ts_data->cmd_update_mutex);
	memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
	xiaomi_touch_interfaces.getModeValue = fts_get_mode_value;
	xiaomi_touch_interfaces.setModeValue = fts_set_cur_value;
	xiaomi_touch_interfaces.resetMode = fts_reset_mode;
	xiaomi_touch_interfaces.getModeAll = fts_get_mode_all;
	xiaomi_touch_interfaces.panel_vendor_read = fts_panel_vendor_read;
	xiaomi_touch_interfaces.panel_color_read = fts_panel_color_read;
	xiaomi_touch_interfaces.panel_display_read = fts_panel_display_read;
	xiaomi_touch_interfaces.touch_vendor_read = fts_touch_vendor_read;
	xiaomi_touch_interfaces.palm_sensor_write = fts_palm_sensor_write;
	/*xiaomi_touch_interfaces.enable_touch_delta = fts_enable_touch_delta;*/
	fts_init_touchmode_data();
	xiaomitouch_register_modedata(0, &xiaomi_touch_interfaces);
}
#endif

static int fts_ts_probe_entry(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int pdata_size = sizeof(struct fts_ts_platform_data);

	FTS_FUNC_ENTER();
	FTS_INFO("%s", FTS_DRIVER_VERSION);
	ts_data->pdata = kzalloc(pdata_size, GFP_KERNEL);
	if (!ts_data->pdata) {
		FTS_ERROR("allocate memory for platform_data fail");
		return -ENOMEM;
	}

	if (ts_data->dev->of_node) {
		ret = fts_parse_dt(ts_data->dev, ts_data->pdata);
		if (ret)
			FTS_ERROR("device-tree parse fail");
	} else {
		if (ts_data->dev->platform_data) {
			memcpy(ts_data->pdata, ts_data->dev->platform_data, pdata_size);
		} else {
			FTS_ERROR("platform_data is null");
			return -ENODEV;
		}
	}

	ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
	if (!ts_data->ts_workqueue) {
		FTS_ERROR("create fts workqueue fail");
	}

	spin_lock_init(&ts_data->irq_lock);
	mutex_init(&ts_data->report_mutex);
	mutex_init(&ts_data->bus_lock);
	ts_data->fps_state = 60;
	ts_data->fps_cmd = 0x3c;

	/* Init communication interface */
	ret = fts_bus_init(ts_data);
	if (ret) {
		FTS_ERROR("bus initialize fail");
		goto err_bus_init;
	}

	ret = fts_input_init(ts_data);
	if (ret) {
		FTS_ERROR("input initialize fail");
		goto err_input_init;
	}

	ret = fts_report_buffer_init(ts_data);
	if (ret) {
		FTS_ERROR("report buffer init fail");
		goto err_report_buffer;
	}

	ret = fts_gpio_configure(ts_data);
	if (ret) {
		FTS_ERROR("configure the gpios fail");
		goto err_gpio_config;
	}

#if FTS_POWER_SOURCE_CUST_EN
	ret = fts_power_source_init(ts_data);
	if (ret) {
		FTS_ERROR("fail to get power(regulator)");
		goto err_power_init;
	}
#endif

#if (!FTS_CHIP_IDC)
	fts_reset_proc(200);
#endif

	ret = fts_get_ic_information(ts_data);
	if (ret) {
		FTS_ERROR("not focal IC, unregister driver");
		goto err_irq_req;
	}

	ret = fts_create_proc(ts_data);
	if (ret) {
		FTS_ERROR("create proc node fail");
	}

	ret = fts_create_sysfs(ts_data);
	if (ret) {
		FTS_ERROR("create sysfs node fail");
	}


#if FTS_POINT_REPORT_CHECK_EN
	ret = fts_point_report_check_init(ts_data);
	if (ret) {
		FTS_ERROR("init point report check fail");
	}
#endif

	ret = fts_ex_mode_init(ts_data);
	if (ret) {
		FTS_ERROR("init glove/cover/charger fail");
	}

	ret = fts_gesture_init(ts_data);
	if (ret) {
		FTS_ERROR("init gesture fail");
	}

#if FTS_TEST_EN
	ret = fts_test_init(ts_data);
	if (ret) {
		FTS_ERROR("init production test fail");
	}
#endif

#if FTS_ESDCHECK_EN
	ret = fts_esdcheck_init(ts_data);
	if (ret) {
		FTS_ERROR("init esd check fail");
	}
#endif

	ret = fts_irq_registration(ts_data);
	if (ret) {
		FTS_ERROR("request irq failed");
		goto err_irq_req;
	}

	ret = fts_fwupg_init(ts_data);
	if (ret) {
		FTS_ERROR("init fw upgrade fail");
	}

	if (ts_data->ts_workqueue) {
		INIT_WORK(&ts_data->resume_work, fts_resume_work);
	}

	device_init_wakeup(ts_data->dev, true);
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
	init_completion(&ts_data->pm_completion);
	ts_data->pm_suspend = false;
#endif

#if defined(CONFIG_DRM)
	ts_data->fb_notif.notifier_call = drm_notifier_callback;
	ret = mi_disp_register_client(&ts_data->fb_notif);
	if (ret) {
		FTS_ERROR("[DRM]Unable to register fb_notifier: %d\n", ret);
	}
#elif defined(CONFIG_FB)
	ts_data->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts_data->fb_notif);
	if (ret) {
		FTS_ERROR("[FB]Unable to register fb_notifier: %d", ret);
	}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + FTS_SUSPEND_LEVEL;
	ts_data->early_suspend.suspend = fts_ts_early_suspend;
	ts_data->early_suspend.resume = fts_ts_late_resume;
	register_early_suspend(&ts_data->early_suspend);
#endif
	INIT_WORK(&ts_data->power_supply_work, fts_power_supply_work);
	ts_data->power_supply_notifier.notifier_call = fts_power_supply_event;
	power_supply_reg_notifier(&ts_data->power_supply_notifier);

#ifdef FTS_XIAOMI_TOUCHFEATURE
	fts_init_xiaomi_touchfeature(ts_data);
#endif
	FTS_FUNC_EXIT();
	return 0;

err_irq_req:
#if FTS_POWER_SOURCE_CUST_EN
err_power_init:
	fts_power_source_exit(ts_data);
#endif
	if (gpio_is_valid(ts_data->pdata->reset_gpio))
		gpio_free(ts_data->pdata->reset_gpio);
	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);
err_gpio_config:
	kfree_safe(ts_data->point_buf);
	kfree_safe(ts_data->events);
err_report_buffer:
	input_unregister_device(ts_data->input_dev);
	if (ts_data->hw_info)
		kfree_safe(ts_data->hw_info);
err_input_init:
	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);

err_bus_init:
	kfree_safe(ts_data->bus_tx_buf);
	kfree_safe(ts_data->bus_rx_buf);
	kfree_safe(ts_data->pdata);

	FTS_FUNC_EXIT();
	return ret;
}

static int fts_ts_remove_entry(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();

#if FTS_POINT_REPORT_CHECK_EN
	fts_point_report_check_exit(ts_data);
#endif

	fts_remove_proc(ts_data);
	fts_remove_sysfs(ts_data);
	fts_ex_mode_exit(ts_data);

	fts_fwupg_exit(ts_data);

#if FTS_TEST_EN
	fts_test_exit(ts_data);
#endif

#if FTS_ESDCHECK_EN
	fts_esdcheck_exit(ts_data);
#endif

	fts_gesture_exit(ts_data);
	fts_bus_exit(ts_data);

	free_irq(ts_data->irq, ts_data);
	input_unregister_device(ts_data->input_dev);

	power_supply_unreg_notifier(&ts_data->power_supply_notifier);

	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);

#if defined(CONFIG_DRM)
	if (mi_disp_unregister_client(&ts_data->fb_notif))
		FTS_ERROR("[DRM]Error occurred while unregistering fb_notifier.\n");
#elif defined(CONFIG_FB)
	if (fb_unregister_client(&ts_data->fb_notif))
		FTS_ERROR("[FB]Error occurred while unregistering fb_notifier.");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts_data->early_suspend);
#endif

	if (gpio_is_valid(ts_data->pdata->reset_gpio))
		gpio_free(ts_data->pdata->reset_gpio);

	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);

#if FTS_POWER_SOURCE_CUST_EN
	fts_power_source_exit(ts_data);
#endif

	kfree_safe(ts_data->point_buf);
	kfree_safe(ts_data->events);

	if (ts_data->hw_info)
		kfree_safe(ts_data->hw_info);

	kfree_safe(ts_data->pdata);
	kfree_safe(ts_data);

	FTS_FUNC_EXIT();

	return 0;
}

static int fts_ts_suspend(struct device *dev)
{
	int ret = 0;
	struct fts_ts_data *ts_data = fts_data;
	char cmd = HT_AFE_STOP;;

	FTS_FUNC_ENTER();
	if (ts_data->suspended) {
		FTS_INFO("Already in suspend state");
		return 0;
	}

	if (ts_data->fw_loading) {
		FTS_INFO("fw upgrade in process, can't suspend");
		return 0;
	}

#ifdef FTS_XIAOMI_TOUCHFEATURE
	if (ts_data->palm_sensor_switch) {
		FTS_INFO("palm sensor ON, switch to OFF");
		update_palm_sensor_value(0);
		fts_palm_sensor_cmd(0);
	}
#endif

#if FTS_ESDCHECK_EN
	fts_esdcheck_suspend();
#endif

#ifdef CONFIG_FACTORY_BUILD
	ts_data->poweroff_on_sleep = true;
#endif
	if (ts_data->gesture_mode && !ts_data->poweroff_on_sleep) {
		fts_gesture_suspend(ts_data);
	} else {
		fts_irq_disable();

		FTS_INFO("make TP enter into sleep mode");
		if (ts_data->enable_touch_raw)
			ret = fts_write(&cmd, 1);
		else
			ret = fts_write_reg(FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP);
		if (ret < 0)
			FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);

		if (!ts_data->ic_info.is_incell && ts_data->poweroff_on_sleep) {
#if FTS_POWER_SOURCE_CUST_EN
			ret = fts_power_source_suspend(ts_data);
			if (ret < 0) {
				FTS_ERROR("power enter suspend fail");
			}
#endif
		}
	}

	fts_release_all_finger();
	ts_data->suspended = true;
	FTS_FUNC_EXIT();
	return 0;
}

static int fts_ts_resume(struct device *dev)
{
	struct fts_ts_data *ts_data = fts_data;
	char cmd = HT_AFE_START;

	FTS_FUNC_ENTER();
	if (!ts_data->suspended) {
		FTS_DEBUG("Already in awake state");
		return 0;
	}

	fts_release_all_finger();

	if (!ts_data->ic_info.is_incell) {
#if FTS_POWER_SOURCE_CUST_EN
		fts_power_source_resume(ts_data);
#endif
		fts_reset_proc(200);
	}

	fts_wait_tp_to_valid();
	fts_ex_mode_recovery(ts_data);

#if FTS_ESDCHECK_EN
	fts_esdcheck_resume();
#endif

#ifdef FTS_XIAOMI_TOUCHFEATURE
	if (ts_data->palm_sensor_switch) {
		fts_palm_sensor_cmd(1);
		FTS_INFO("palm sensor OFF, switch to ON");
	}
#endif
	if (ts_data->gesture_mode && !ts_data->poweroff_on_sleep) {
		fts_gesture_resume(ts_data);
	} else {
		fts_irq_enable();
	}
	if (ts_data->enable_touch_raw)
		fts_write(&cmd, 1);

	if (ts_data->fps_state != 60) {
		FTS_INFO("config touch work with %d fps", ts_data->fps_state);
		if (!fts_write_reg(0x8a, ts_data->fps_cmd))
			FTS_INFO("config fps successful");
	}

	ts_data->poweroff_on_sleep = false;
	ts_data->suspended = false;
	FTS_FUNC_EXIT();
	return 0;
}

#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
static int fts_pm_suspend(struct device *dev)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	FTS_INFO("system enters into pm_suspend");
	ts_data->pm_suspend = true;
	reinit_completion(&ts_data->pm_completion);
	return 0;
}

static int fts_pm_resume(struct device *dev)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	FTS_INFO("system resumes from pm_suspend");
	ts_data->pm_suspend = false;
	complete(&ts_data->pm_completion);
	return 0;
}

static const struct dev_pm_ops fts_dev_pm_ops = {
	.suspend = fts_pm_suspend,
	.resume = fts_pm_resume,
};
#endif

/*****************************************************************************
* TP Driver
*****************************************************************************/
static int fts_ts_probe(struct spi_device *spi)
{
	int ret = 0;
	struct fts_ts_data *ts_data = NULL;

	FTS_INFO("Touch Screen(SPI BUS) driver prboe...");
	spi->mode = SPI_MODE_0;
	spi->max_speed_hz = 12000000;
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret) {
		FTS_ERROR("spi setup fail");
		return ret;
	}

	/* malloc memory for global struct variable */
	ts_data = (struct fts_ts_data *)kzalloc(sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data) {
		FTS_ERROR("allocate memory for fts_data fail");
		return -ENOMEM;
	}

	fts_data = ts_data;
	ts_data->spi = spi;
	ts_data->dev = &spi->dev;
	ts_data->log_level = 1;
	ts_data->poweroff_on_sleep = false;

	ts_data->bus_type = BUS_TYPE_SPI_V2;
	spi_set_drvdata(spi, ts_data);

	ret = fts_ts_probe_entry(ts_data);
	if (ret) {
		FTS_ERROR("Touch Screen(SPI BUS) driver probe fail");
		kfree_safe(ts_data);
		return ret;
	}

	FTS_INFO("Touch Screen(SPI BUS) driver prboe successfully");
	return 0;
}

static int fts_ts_remove(struct spi_device *spi)
{
	return fts_ts_remove_entry(spi_get_drvdata(spi));
}

static const struct spi_device_id fts_ts_id[] = {
	{FTS_DRIVER_NAME, 0},
	{},
};
static const struct of_device_id fts_dt_match[] = {
	{.compatible = "xiaomi,k9-spi", },
	{},
};
MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct spi_driver fts_ts_driver = {
	.probe = fts_ts_probe,
	.remove = fts_ts_remove,
	.driver = {
		.name = FTS_DRIVER_NAME,
		.owner = THIS_MODULE,
#if defined(CONFIG_PM) && FTS_PATCH_COMERR_PM
		.pm = &fts_dev_pm_ops,
#endif
		.of_match_table = of_match_ptr(fts_dt_match),
	},
	.id_table = fts_ts_id,
};

static int __init fts_ts_init(void)
{
	int ret = 0;
	int gpio_10;
	int gpio_17;

	FTS_FUNC_ENTER();

	gpio_direction_input(OLED_JUDGE_ID);
	gpio_17 = gpio_get_value(OLED_JUDGE_ID);
	gpio_direction_input(TS_TA_INT_ID);
	gpio_10 = gpio_get_value(TS_TA_INT_ID);
	FTS_ERROR("panel_id_gpio_17 = %d\n",gpio_17);
	FTS_ERROR("panel_id_gpio_10 = %d\n",gpio_10);
	if(gpio_17){
		FTS_ERROR("TP is goodix");
		return 0;
	}else{
		FTS_ERROR("TP is focal");
	}
	ret = spi_register_driver(&fts_ts_driver);
	if (ret != 0) {
		FTS_ERROR("Focaltech touch screen driver init failed!");
	}
	FTS_FUNC_EXIT();
	return ret;
}

static void __exit fts_ts_exit(void)
{
	spi_unregister_driver(&fts_ts_driver);
}

module_init(fts_ts_init);
module_exit(fts_ts_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
