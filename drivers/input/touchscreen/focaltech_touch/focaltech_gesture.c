/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2019, Focaltech Ltd. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
* File Name: focaltech_gestrue.c
*
* Author: Focaltech Driver Team
*
* Created: 2016-08-08
*
* Abstract:
*
* Reference:
*
*****************************************************************************/

/*****************************************************************************
* 1.Included header files
*****************************************************************************/
#include "focaltech_core.h"
/******************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define KEY_GESTURE_U                           KEY_WAKEUP
#define KEY_GESTURE_UP                          KEY_UP
#define KEY_GESTURE_DOWN                        KEY_DOWN
#define KEY_GESTURE_LEFT                        KEY_LEFT
#define KEY_GESTURE_RIGHT                       KEY_RIGHT
#define KEY_GESTURE_O                           KEY_O
#define KEY_GESTURE_E                           KEY_E
#define KEY_GESTURE_M                           KEY_M
#define KEY_GESTURE_L                           KEY_L
#define KEY_GESTURE_W                           KEY_W
#define KEY_GESTURE_S                           KEY_S
#define KEY_GESTURE_V                           KEY_V
#define KEY_GESTURE_C                           KEY_C
#define KEY_GESTURE_Z                           KEY_Z

#define KEY_FODLEAVE                            0x153
#define KEY_FOD                                 0x152
#define KEY_FOD_TOUCH                           KEY_FOD
#define KEY_FOD_TOUCH_LEAVE                     KEY_FODLEAVE

#define GESTURE_LEFT                            0x20
#define GESTURE_RIGHT                           0x21
#define GESTURE_UP                              0x22
#define GESTURE_DOWN                            0x23
#define GESTURE_DOUBLECLICK                     0x24
#define GESTURE_CLICK                           0x25
#define GESTURE_O                               0x30
#define GESTURE_W                               0x31
#define GESTURE_M                               0x32
#define GESTURE_E                               0x33
#define GESTURE_L                               0x44
#define GESTURE_S                               0x46
#define GESTURE_V                               0x54
#define GESTURE_Z                               0x41
#define GESTURE_C                               0x34

#define FOD_TOUCH                               0x26
#define FOD_TOUCH_LEAVE                         0x27


/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/
/*
* gesture_id    - mean which gesture is recognised
* point_num     - points number of this gesture
* coordinate_x  - All gesture point x coordinate
* coordinate_y  - All gesture point y coordinate
* mode          - gesture enable/disable, need enable by host
*               - 1:enable gesture function(default)  0:disable
* active        - gesture work flag,
*                 always set 1 when suspend, set 0 when resume
*/
struct fts_gesture_st {
	u8 gesture_id;
	u8 point_num;
	u16 coordinate_x[FTS_GESTURE_POINTS_MAX];
	u16 coordinate_y[FTS_GESTURE_POINTS_MAX];
	u8 mode;
	u8 active;
};

struct fts_gesture_fod_st {
	u8 ucFodPointID;
	u8 ucFodID;
	u8 ucFodArea;
	u8 ucTouchArea;
	u16 ucFodCoordinate_x;
	u16 ucFodCoordinate_y;
	u8 ucFodEvent;
};

/*****************************************************************************
* Static variables
*****************************************************************************/
static struct fts_gesture_st fts_gesture_data;
static struct fts_gesture_fod_st fts_gerture_fod_data;

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static ssize_t fts_gesture_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;
	u8 val = 0;
	struct input_dev *input_dev = fts_data->input_dev;

	mutex_lock(&input_dev->mutex);
	fts_read_reg(FTS_REG_GESTURE_EN, &val);
	count = snprintf(buf, PAGE_SIZE, "Gesture Mode:%s\n",
					 fts_gesture_data.mode ? "On" : "Off");
	count += snprintf(buf + count, PAGE_SIZE, "Reg(0xD0)=%d\n", val);
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_gesture_store(
	struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct input_dev *input_dev = fts_data->input_dev;
	mutex_lock(&input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf)) {
		FTS_DEBUG("enable gesture");
		fts_gesture_data.mode = ENABLE;
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		FTS_DEBUG("disable gesture");
		fts_gesture_data.mode = DISABLE;
	}
	mutex_unlock(&input_dev->mutex);

	return count;
}

void fts_gesture_enable(bool enable)
{
	struct input_dev *input_dev = fts_data->input_dev;

	mutex_lock(&input_dev->mutex);

	if (enable) {
		FTS_INFO("[GESTURE]enable gesture");
		fts_gesture_data.mode = ENABLE;
	} else {
		FTS_INFO("[GESTURE]disable gesture");
		fts_gesture_data.mode = DISABLE;
	}

	mutex_unlock(&input_dev->mutex);
}

static ssize_t fts_gesture_buf_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;
	int i = 0;
	struct input_dev *input_dev = fts_data->input_dev;
	struct fts_gesture_st *gesture = &fts_gesture_data;

	mutex_lock(&input_dev->mutex);
	count = snprintf(buf, PAGE_SIZE, "Gesture ID:%d\n", gesture->gesture_id);
	count += snprintf(buf + count, PAGE_SIZE, "Gesture PointNum:%d\n",
					  gesture->point_num);
	count += snprintf(buf + count, PAGE_SIZE, "Gesture Points Buffer:\n");

	/* save point data,max:6 */
	for (i = 0; i < FTS_GESTURE_POINTS_MAX; i++) {
		count += snprintf(buf + count, PAGE_SIZE, "%3d(%4d,%4d) ", i,
						  gesture->coordinate_x[i], gesture->coordinate_y[i]);
		if ((i + 1) % 4 == 0)
			count += snprintf(buf + count, PAGE_SIZE, "\n");
	}
	count += snprintf(buf + count, PAGE_SIZE, "\n");
	mutex_unlock(&input_dev->mutex);

	return count;
}

static ssize_t fts_gesture_buf_store(
	struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	return -EPERM;
}


/* sysfs gesture node
 *   read example: cat  fts_gesture_mode       ---read gesture mode
 *   write example:echo 1 > fts_gesture_mode   --- write gesture mode to 1
 *
 */
static DEVICE_ATTR(fts_gesture_mode, S_IRUGO | S_IWUSR, fts_gesture_show,
				   fts_gesture_store);
/*
 *   read example: cat fts_gesture_buf        --- read gesture buf
 */
static DEVICE_ATTR(fts_gesture_buf, S_IRUGO | S_IWUSR,
				   fts_gesture_buf_show, fts_gesture_buf_store);

static struct attribute *fts_gesture_mode_attrs[] = {
	&dev_attr_fts_gesture_mode.attr,
	&dev_attr_fts_gesture_buf.attr,
	NULL,
};

static struct attribute_group fts_gesture_group = {
	.attrs = fts_gesture_mode_attrs,
};

int fts_create_gesture_sysfs(struct device *dev)
{
	int ret = 0;

	ret = sysfs_create_group(&dev->kobj, &fts_gesture_group);
	if (ret) {
		FTS_ERROR("gesture sys node create fail");
		sysfs_remove_group(&dev->kobj, &fts_gesture_group);
		return ret;
	}

	return 0;
}

static void fts_gesture_report(struct input_dev *input_dev, int gesture_id)
{
	int gesture;

	FTS_DEBUG("gesture_id:0x%x", gesture_id);
	switch (gesture_id) {
	case GESTURE_LEFT:
		gesture = KEY_GESTURE_LEFT;
		break;
	case GESTURE_RIGHT:
		gesture = KEY_GESTURE_RIGHT;
		break;
	case GESTURE_UP:
		gesture = KEY_GESTURE_UP;
		break;
	case GESTURE_DOWN:
		gesture = KEY_GESTURE_DOWN;
		break;
	case GESTURE_DOUBLECLICK:
		if (fts_data->lpwg_mode) {
			gesture = KEY_GESTURE_U;
		} else {
			gesture = -1;
		}

		break;
	case GESTURE_CLICK:
		gesture =  KEY_GOTO;
		break;
	case GESTURE_O:
		gesture = KEY_GESTURE_O;
		break;
	case GESTURE_W:
		gesture = KEY_GESTURE_W;
		break;
	case GESTURE_M:
		gesture = KEY_GESTURE_M;
		break;
	case GESTURE_E:
		gesture = KEY_GESTURE_E;
		break;
	case GESTURE_L:
		gesture = KEY_GESTURE_L;
		break;
	case GESTURE_S:
		gesture = KEY_GESTURE_S;
		break;
	case GESTURE_V:
		gesture = KEY_GESTURE_V;
		break;
	case GESTURE_Z:
		gesture = KEY_GESTURE_Z;
		break;
	case  GESTURE_C:
		gesture = KEY_GESTURE_C;
		break;
	case  FOD_TOUCH_LEAVE:
		gesture = KEY_FOD_TOUCH_LEAVE;
		break;

	default:
		gesture = -1;
		break;
	}
	/* report event key */
	if (gesture != -1) {
		FTS_DEBUG("Gesture Code=%d", gesture);
		input_report_key(input_dev, gesture, 1);
		input_sync(input_dev);
		input_report_key(input_dev, gesture, 0);
		input_sync(input_dev);
	}
}

/*****************************************************************************
* Name: fts_gesture_readdata
* Brief: Read information about gesture: enable flag/gesture points..., if ges-
*        ture enable, save gesture points' information, and report to OS.
*        It will be called this function every intrrupt when FTS_GESTURE_EN = 1
*
*        gesture data length: 1(enable) + 1(reserve) + 2(header) + 6 * 4
* Input: ts_data - global struct data
*        data    - gesture data buffer if non-flash, else NULL
* Output:
* Return: 0 - read gesture data successfully, the report data is gesture data
*         1 - tp not in suspend/gesture not enable in TP FW
*         -Exx - error
*****************************************************************************/
int fts_gesture_readdata(struct fts_ts_data *ts_data, u8 *data)
{
	int ret = 0;
	int i = 0;
	int index = 0;
	u8 buf[FTS_GESTURE_DATA_LEN] = { 0 };
	struct input_dev *input_dev = ts_data->input_dev;
	struct fts_gesture_st *gesture = &fts_gesture_data;

	if (!ts_data->suspended || (DISABLE == gesture->mode)) {
		return 1;
	}


	ret = fts_read_reg(FTS_REG_GESTURE_EN, &buf[0]);
	if ((ret < 0) || (buf[0] != ENABLE)) {
		FTS_DEBUG("gesture not enable in fw, don't process gesture");
		return 1;
	}

	buf[2] = FTS_REG_GESTURE_OUTPUT_ADDRESS;
	ret = fts_read(&buf[2], 1, &buf[2], FTS_GESTURE_DATA_LEN - 2);
	if (ret < 0) {
		FTS_ERROR("read gesture header data fail");
		return ret;
	}
	for (index = 0; index < 28; index++) {
		FTS_DEBUG("gesture_buf: buf[%d] = %d    ", index, buf[index]);
	}

	/* init variable before read gesture point */
	memset(gesture->coordinate_x, 0, FTS_GESTURE_POINTS_MAX * sizeof(u16));
	memset(gesture->coordinate_y, 0, FTS_GESTURE_POINTS_MAX * sizeof(u16));
	gesture->gesture_id = buf[2];
	gesture->point_num = buf[3];
	FTS_DEBUG("gesture_id=%d, point_num=%d",
			  gesture->gesture_id, gesture->point_num);

	/* save point data,max:6 */
	for (i = 0; i < FTS_GESTURE_POINTS_MAX; i++) {
		index = 4 * i + 4;
		gesture->coordinate_x[i] = (u16)(((buf[0 + index] & 0x0F) << 8)
						                 + buf[1 + index]);
		gesture->coordinate_y[i] = (u16)(((buf[2 + index] & 0x0F) << 8)
						                 + buf[3 + index]);
	}

	/* report gesture to OS */
	fts_gesture_report(input_dev, gesture->gesture_id);
	return 0;
}

int fts_fod_readdata(struct fts_ts_data *ts_data, u8 *data)
{
	int ret = 0;
	int index = 0;
	u8 buf[FTS_GESTURE_FOD_DATA_LEN] = { 0 };
	struct input_dev *input_dev = ts_data->input_dev;
	struct fts_gesture_fod_st *gesture_fod = &fts_gerture_fod_data;


	ret = fts_read_reg(FTS_REG_FOD_EN, &buf[0]);
	if ((ret < 0) || (buf[0] < 1) || (buf[0] > 3)) {
		return 1;
	}

	buf[0] = FTS_REG_GESTURE_FOD_OUTPUT_ADDRESS;
	ret = fts_read(&buf[0], 1, &buf[0], FTS_GESTURE_FOD_DATA_LEN);
	if (ret < 0) {
		FTS_ERROR("read fod header data fail");
		return ret;
	}
	FTS_DEBUG("gesture_fod: buf[0-8] = %d, %d, %d, %d, %d, %d, %d, %d, %d",
			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);

	gesture_fod->ucFodPointID = buf[0];
	gesture_fod->ucFodID = buf[1];
	gesture_fod->ucFodArea = buf[2];
	gesture_fod->ucTouchArea = buf[3];
	gesture_fod->ucFodCoordinate_x = (u16)(((buf[4 + index] & 0x0F) << 8)
						                 + buf[5 + index]);
	gesture_fod->ucFodCoordinate_y = (u16)(((buf[6 + index] & 0x0F) << 8)
						                 + buf[7 + index]);
	gesture_fod->ucFodEvent = buf[8];

	/* report gesture_fod_id to OS */
	switch (gesture_fod->ucFodID){
	case 0x24:
		input_report_key(input_dev, KEY_WAKEUP, 1);
		input_sync(input_dev);
		input_report_key(input_dev, KEY_WAKEUP, 0);
		input_sync(input_dev);
		break;
	case 0x25:
		input_report_key(input_dev, KEY_GOTO, 1);
		input_sync(input_dev);
		input_report_key(input_dev, KEY_GOTO, 0);
		input_sync(input_dev);
		break;
	case 0x26:
		if (gesture_fod->ucFodEvent == 0) {
			mutex_lock(&ts_data->report_mutex);
			if (!ts_data->fod_finger_skip) {
				input_report_key(input_dev, KEY_INFO, 1);
				input_report_key(input_dev, KEY_FOD_TOUCH, 1);
				input_sync(input_dev);
				ts_data->overlap_area = 100;
			}
			if (ts_data->old_point_id != gesture_fod->ucFodPointID) {
				if (ts_data->old_point_id == 0xff)
					ts_data->old_point_id = gesture_fod->ucFodPointID;
				else {
					ts_data->point_id_changed = true;
				}
			}
			ts_data->finger_in_fod = true;
			if (!ts_data->suspended) {
				pr_info("FTS:touch is not in suspend state or finger report is not enabled, report x,y value by touch nomal report\n");
				mutex_unlock(&ts_data->report_mutex);
				return -EINVAL;
			}

			if (!ts_data->fod_finger_skip) {
				input_mt_slot(input_dev,gesture_fod->ucFodPointID);
				input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 1);
				input_report_key(input_dev, BTN_TOUCH, 1);
				input_report_key(input_dev, BTN_TOOL_FINGER, 1);
				input_report_abs(input_dev, ABS_MT_POSITION_X, gesture_fod->ucFodCoordinate_x);
				input_report_abs(input_dev, ABS_MT_POSITION_Y, gesture_fod->ucFodCoordinate_y);
				input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR, gesture_fod->ucTouchArea);
				input_report_abs(input_dev, ABS_MT_WIDTH_MAJOR, ts_data->overlap_area);
				input_report_abs(input_dev, ABS_MT_WIDTH_MINOR, ts_data->overlap_area);
				input_report_abs(input_dev, ABS_MT_PRESSURE, gesture_fod->ucTouchArea);
				input_sync(input_dev);
			}
			mutex_unlock(&ts_data->report_mutex);
		} else {
			input_report_key(input_dev, KEY_INFO, 0);
			input_report_key(input_dev, KEY_FOD_TOUCH, 0);
			input_sync(input_dev);
			ts_data->overlap_area = 0;
			ts_data->finger_in_fod = false;
			ts_data->fod_finger_skip = false;
			ts_data->old_point_id = 0xff;
			ts_data->point_id_changed = false;
			FTS_INFO("set fod finger skip false, set old_point_id as default value\n");
			if (!ts_data->suspended) {
				pr_info("FTS_UP:touch is not in suspend state or finger report is not enabled, report x,y value by touch nomal report\n");
				return -EINVAL;
			}
			mutex_lock(&ts_data->report_mutex);
			input_mt_slot(input_dev, gesture_fod->ucFodPointID);
			input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
			input_report_key(input_dev, BTN_TOUCH, 0);
			input_report_abs(input_dev, ABS_MT_TRACKING_ID, -1);
			input_sync(input_dev);
			mutex_unlock(&ts_data->report_mutex);
		}
		break;
	default:
		ts_data->overlap_area = 0;
		if (ts_data->suspended)
			return 0;
		else
			return -EINVAL;
		break;
	}
	return 0;
}

void fts_gesture_recovery(struct fts_ts_data *ts_data)
{
	if ((ENABLE == fts_gesture_data.mode) && (ENABLE == fts_gesture_data.active)) {
		FTS_DEBUG("gesture recovery...");
		fts_write_reg(0xD2, 0xFF);
		fts_write_reg(0xD5, 0xFF);
		fts_write_reg(0xD6, 0xFF);
		fts_write_reg(0xD7, 0xFF);
		fts_write_reg(0xD8, 0xFF);
		fts_write_reg(FTS_REG_GESTURE_EN, ENABLE);
	}
}

void fts_fod_recovery(struct fts_ts_data *ts_data)
{
	FTS_DEBUG("FOD recovery...");
	if (fts_data->suspended)
		fts_write_reg(FTS_REG_GESTURE_EN, ENABLE);
}

int fts_gesture_suspend(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int i = 0;
	u8 state = 0xFF;
	unsigned char double_val;
	FTS_INFO("gesture suspend...");
	/* gesture not enable, return immediately */
	if (fts_gesture_data.mode == DISABLE) {
		FTS_DEBUG("gesture is disabled");
		return -EINVAL;
	}
	ret = fts_read_reg(FTS_REG_FOD_EN, &double_val);
	if (ret < 0) {
		FTS_ERROR("set double_wakeup fail");
	}
	for (i = 0; i < 5; i++) {
		if (ts_data->lpwg_mode){
			double_val |= (1 << 0);
			FTS_INFO("CF register double_wakeup's bit set 1, CF register = %x", double_val);
			ret = fts_write_reg(FTS_REG_FOD_EN, double_val);
			if (ret < 0) {
				FTS_ERROR("set double_wakeup fail");
			}
		}
		fts_write_reg(FTS_REG_GESTURE_EN, ENABLE);
		msleep(10);
		fts_read_reg(FTS_REG_GESTURE_EN, &state);
		if (state == ENABLE)
			break;
	}

	if (i >= 5) {
		FTS_ERROR("Enter into gesture(suspend) fail");
		fts_gesture_data.active = DISABLE;
		return -EIO;
	}

	ret = enable_irq_wake(ts_data->irq);
	if (ret) {
		FTS_DEBUG("enable_irq_wake(irq:%d) fail", ts_data->irq);
	}

	fts_gesture_data.active = ENABLE;
	FTS_INFO("Enter into gesture(suspend) successfully!");
	return 0;
}

int fts_fod_pay(struct fts_ts_data *ts_data)
{
	int i = 0;

	for (i = 0; i < 5; i++) {
		fts_write_reg(FTS_REG_FOD_EN, 0x02);
	}

	FTS_INFO("Enter into FOD(pay) successfully!");
	return 0;
}

int fts_fod_suspend(struct fts_ts_data *ts_data)
{
	int ret = 0;
	unsigned char fodval;

	FTS_INFO("FOD suspend...");

	ret = fts_read_reg(FTS_REG_FOD_EN, &fodval);
	if (ret < 0) {
		FTS_ERROR("exit FOD(suspend) fail");
		return -EIO;
	}

	fodval |= (1 << 1);
	FTS_INFO("CF register fod's bit set 1,  CF register = %x", fodval);

	ret = fts_write_reg(FTS_REG_FOD_EN, fodval);
	if (ret < 0) {
		FTS_ERROR("exit FOD(suspend) fail");
		return -EIO;
	}

	ret = enable_irq_wake(ts_data->irq);
	if (ret) {
		FTS_DEBUG("enable_irq_wake(irq:%d) fail", ts_data->irq);
	}

	FTS_INFO("Enter into FOD(suspend) successfully!");
	return 0;
}

int fts_gesture_resume(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int i = 0;
	u8 state = 0xFF;

	FTS_INFO("gesture resume...");
	/* gesture not enable, return immediately */
	if (fts_gesture_data.mode == DISABLE) {
		FTS_DEBUG("gesture is disabled");
		return -EINVAL;
	}

	if (fts_gesture_data.active == DISABLE) {
		FTS_DEBUG("gesture active is disable, return immediately");
		return -EINVAL;
	}

	fts_gesture_data.active = DISABLE;
	for (i = 0; i < 5; i++) {
		fts_write_reg(FTS_REG_GESTURE_EN, DISABLE);
		msleep(1);
		fts_read_reg(FTS_REG_GESTURE_EN, &state);
		if (state == DISABLE)
			break;
	}

	if (i >= 5) {
		FTS_ERROR("exit gesture(resume) fail");
		return -EIO;
	}

	ret = disable_irq_wake(ts_data->irq);
	if (ret) {
		FTS_DEBUG("disable_irq_wake(irq:%d) fail", ts_data->irq);
	}

	FTS_INFO("resume from gesture successfully");
	return 0;
}

int fts_fod_resume(struct fts_ts_data *ts_data)
{
	int ret = 0;
	unsigned char fodval;
	struct input_dev *input_dev = ts_data->input_dev;

	FTS_INFO("FOD resume...");

	ret = fts_read_reg(FTS_REG_FOD_EN, &fodval);
	if (ret < 0) {
		FTS_ERROR("set FOD(resume) fail");
		return -EIO;
	}

	fodval &= ~(1 << 1);
	FTS_INFO("CF register fod's bit set 0, CF register = %x", fodval);

	ret = fts_write_reg(FTS_REG_FOD_EN, fodval);
	if (ret < 0) {
		FTS_ERROR("set FOD(resume) fail");
		return -EIO;
	}

	ret = disable_irq_wake(ts_data->irq);
	if (ret) {
		FTS_DEBUG("disable_irq_wake(irq:%d) fail", ts_data->irq);
	}

	synchronize_irq(ts_data->irq);
	input_report_key(input_dev, KEY_FOD_TOUCH, 0);
	input_report_key(input_dev, KEY_INFO, 0);
	input_sync(input_dev);
	fts_data->finger_in_fod = false;
	ts_data->fod_finger_skip = false;

	FTS_INFO("resume from FOD successfully");
	return 0;
}

int fts_gesture_init(struct fts_ts_data *ts_data)
{
	struct input_dev *input_dev = ts_data->input_dev;

	FTS_FUNC_ENTER();
	input_set_capability(input_dev, EV_KEY, KEY_POWER);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_U);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_UP);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_DOWN);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_LEFT);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_RIGHT);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_O);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_E);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_M);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_L);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_W);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_S);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_V);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_Z);
	input_set_capability(input_dev, EV_KEY, KEY_GESTURE_C);
	input_set_capability(input_dev, EV_KEY, KEY_FOD_TOUCH);
	input_set_capability(input_dev, EV_KEY, KEY_FOD_TOUCH_LEAVE);
	input_set_capability(input_dev, EV_KEY, KEY_GOTO);
	input_set_capability(input_dev, EV_KEY, KEY_INFO);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, 0xFF, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MINOR, 0, 0xFF, 0, 0);

	__set_bit(KEY_GESTURE_RIGHT, input_dev->keybit);
	__set_bit(KEY_GESTURE_LEFT, input_dev->keybit);
	__set_bit(KEY_GESTURE_UP, input_dev->keybit);
	__set_bit(KEY_GESTURE_DOWN, input_dev->keybit);
	__set_bit(KEY_GESTURE_U, input_dev->keybit);
	__set_bit(KEY_GESTURE_O, input_dev->keybit);
	__set_bit(KEY_GESTURE_E, input_dev->keybit);
	__set_bit(KEY_GESTURE_M, input_dev->keybit);
	__set_bit(KEY_GESTURE_W, input_dev->keybit);
	__set_bit(KEY_GESTURE_L, input_dev->keybit);
	__set_bit(KEY_GESTURE_S, input_dev->keybit);
	__set_bit(KEY_GESTURE_V, input_dev->keybit);
	__set_bit(KEY_GESTURE_C, input_dev->keybit);
	__set_bit(KEY_GESTURE_Z, input_dev->keybit);

	__set_bit(KEY_FOD_TOUCH, input_dev->keybit);
	__set_bit(KEY_FOD_TOUCH_LEAVE, input_dev->keybit);

	fts_create_gesture_sysfs(ts_data->dev);

	memset(&fts_gesture_data, 0, sizeof(struct fts_gesture_st));
	fts_gesture_data.mode = ENABLE;
	fts_gesture_data.active = DISABLE;

	FTS_FUNC_EXIT();
	return 0;
}

int fts_gesture_exit(struct fts_ts_data *ts_data)
{
	FTS_FUNC_ENTER();
	sysfs_remove_group(&ts_data->dev->kobj, &fts_gesture_group);
	FTS_FUNC_EXIT();
	return 0;
}
