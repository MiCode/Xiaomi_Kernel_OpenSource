// SPDX-License-Identifier: GPL-2.0
/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, Focaltech Ltd. All rights reserved.
 *
 * Copyright (C) 2022 XiaoMi, Inc.
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
#define KEY_GESTURE_U                           KEY_U
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

#define GESTURE_LEFT                            0x20
#define GESTURE_RIGHT                           0x21
#define GESTURE_UP                              0x22
#define GESTURE_DOWN                            0x23
#define GESTURE_DOUBLECLICK                     0x24
#define GESTURE_SINGLETAP                       0x25
#define GESTURE_O                               0x30
#define GESTURE_W                               0x31
#define GESTURE_M                               0x32
#define GESTURE_E                               0x33
#define GESTURE_L                               0x44
#define GESTURE_S                               0x46
#define GESTURE_V                               0x54
#define GESTURE_Z                               0x41
#define GESTURE_C                               0x34

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
};

/*****************************************************************************
 * Static variables
 *****************************************************************************/
static struct fts_gesture_st fts_gesture_data;

/*****************************************************************************
 * Global variable or extern global variabls/functions
 *****************************************************************************/

/*****************************************************************************
 * Static function prototypes
 *****************************************************************************/
static ssize_t fts_gesture_mode_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;
	u8 val = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	fts_read_reg(FTS_REG_GESTURE_EN, &val);
	count = snprintf(buf, PAGE_SIZE, "Gesture Mode:%s\n",
					 ts_data->gesture_support ? "On" : "Off");
	count += snprintf(buf + count, PAGE_SIZE, "Reg(0xD0)=%d\n", val);
	mutex_unlock(&ts_data->input_dev->mutex);

	return count;
}

static ssize_t fts_gesture_mode_store(
	struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	if (FTS_SYSFS_ECHO_ON(buf)) {
		FTS_DEBUG("enable gesture");
		ts_data->gesture_support = ENABLE;
	} else if (FTS_SYSFS_ECHO_OFF(buf)) {
		FTS_DEBUG("disable gesture");
		ts_data->gesture_support = DISABLE;
	}
	mutex_unlock(&ts_data->input_dev->mutex);

	return count;
}

static ssize_t fts_gesture_buf_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;
	int i = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	struct input_dev *input_dev = ts_data->input_dev;
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

static ssize_t fts_gesture_bm_show(
	struct device *dev, struct device_attribute *attr, char *buf)
{
	int count = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	mutex_lock(&ts_data->input_dev->mutex);
	count = snprintf(buf, PAGE_SIZE, "gesture bmode:%d\n",
					 ts_data->gesture_bmode);
	mutex_unlock(&ts_data->input_dev->mutex);

	return count;
}

static ssize_t fts_gesture_bm_store(
	struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	int value = 0xFF;

	mutex_lock(&ts_data->input_dev->mutex);
	if (kstrtoint(buf, 10, &value))
		return -EINVAL;
	FTS_DEBUG("gesture bmode:%d->%d", ts_data->gesture_bmode, value);
	ts_data->gesture_bmode = value;
	mutex_unlock(&ts_data->input_dev->mutex);

	return count;
}

/* sysfs gesture node
 *   read example: cat  fts_gesture_mode       ---read gesture mode
 *   write example:echo 1 > fts_gesture_mode   --- write gesture mode to 1
 *
 */
static DEVICE_ATTR_RW(fts_gesture_mode);
/*
 *   read example: cat fts_gesture_buf        --- read gesture buf
 */
static DEVICE_ATTR_RW(fts_gesture_buf);

static DEVICE_ATTR_RW(fts_gesture_bm);

static struct attribute *fts_gesture_mode_attrs[] = {
	&dev_attr_fts_gesture_mode.attr,
	&dev_attr_fts_gesture_buf.attr,
	&dev_attr_fts_gesture_bm.attr,
	NULL,
};

static struct attribute_group fts_gesture_group = {
	.attrs = fts_gesture_mode_attrs,
};

static int fts_create_gesture_sysfs(struct device *dev)
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
		gesture = KEY_WAKEUP;
		break;
	case GESTURE_SINGLETAP:
		gesture = KEY_GOTO;
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
 *        data    - gesture data buffer
 * Output:
 * Return: 0 - read gesture data successfully, the report data is gesture data
 *         1 - tp not in suspend/gesture not enable in TP FW
 *         -Exx - error
 *****************************************************************************/
int fts_gesture_readdata(struct fts_ts_data *ts_data, u8 *touch_buf)
{
	int ret = 0;
	int i = 0;
	int index = 0;
	u8 buf[FTS_GESTURE_DATA_LEN] = { 0 };
	struct input_dev *input_dev = ts_data->input_dev;
	struct fts_gesture_st *gesture = &fts_gesture_data;

	if (!ts_data->gesture_support) {
		FTS_ERROR("gesture no support");
		return -EINVAL;
	}

	if (ts_data->gesture_bmode == GESTURE_BM_TOUCH) {
		memcpy(buf, touch_buf + FTS_TOUCH_DATA_LEN, FTS_GESTURE_DATA_LEN);
	} else {
		buf[2] = FTS_REG_GESTURE_OUTPUT_ADDRESS;
		ret = fts_read(&buf[2], 1, &buf[2], FTS_GESTURE_DATA_LEN - 2);
		if (ret < 0) {
			FTS_ERROR("read gesture header data fail");
			return ret;
		}
	}

	/* init variable before read gesture point */
	memset(gesture->coordinate_x, 0, FTS_GESTURE_POINTS_MAX * sizeof(u16));
	memset(gesture->coordinate_y, 0, FTS_GESTURE_POINTS_MAX * sizeof(u16));
	gesture->gesture_id = buf[2];
	gesture->point_num = buf[3];
	if (gesture->gesture_id == GESTURE_DOUBLECLICK && !(ts_data->gesture_status & 0x01)) {
		FTS_INFO("double click is not enabled!");
		return 1;
	}
	if (gesture->gesture_id == GESTURE_SINGLETAP && !(ts_data->gesture_status & 0x02)) {
		if (ts_data->fod_status != -1 && ts_data->fod_status != 100 && ts_data->nonui_status == 0) {
			FTS_INFO("FOD on support single tap");
		} else {
			FTS_INFO("single tap is not enabled!");
			return 1;
		}
	}
	FTS_DEBUG("gesture_id=%x; DoubleClick:0x24  SingleTap:0x25", gesture->gesture_id);

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

void fts_gesture_recovery(struct fts_ts_data *ts_data)
{
	if (ts_data->gesture_support && ts_data->suspended) {
		FTS_DEBUG("gesture recovery...");
		fts_write_reg(0xD1, 0xFF);
		fts_write_reg(0xD2, 0xFF);
		fts_write_reg(0xD5, 0xFF);
		fts_write_reg(0xD6, 0xFF);
		fts_write_reg(0xD7, 0xFF);
		fts_write_reg(0xD8, 0xFF);
		fts_write_reg(FTS_REG_GESTURE_EN, ENABLE);
#ifdef FTS_TOUCHSCREEN_FOD
		fts_fod_reg_write(FTS_REG_GESTURE_DOUBLETAP_ON, true);
#endif
	}
}

void fts_fod_recovery(void)
{
	FTS_FUNC_ENTER();
	if (fts_data->suspended) {
		FTS_INFO("%s, tp is in suspend mode, write 0xD0 to 1", __func__);
		fts_gesture_reg_write(FTS_REG_GESTURE_DOUBLETAP_ON, true);
	}
	fts_fod_reg_write(FTS_REG_GESTURE_FOD_ON, true);
}

int fts_fod_reg_write(u8 mask, bool enable)
{
	int i;
	u8 state;
	u8 reg_value;
	u8 reg_value_last_time;


	for (i = 0; i < 5; i++) {
		fts_read_reg(FTS_REG_GESTURE_SUPPORT, &reg_value);
		reg_value_last_time = reg_value;
		if (enable)
			reg_value |= mask;
		else
			reg_value &= ~mask;
		/* If the value in the register is equal to the modified value, skip writing to the register */
		if (reg_value == reg_value_last_time) {
			FTS_INFO("reg 0xCF do not need to be modified, reg_value = %02X", reg_value);
			return 0;
		}
		fts_write_reg(FTS_REG_GESTURE_SUPPORT, reg_value);
		msleep(1);
		fts_read_reg(FTS_REG_GESTURE_SUPPORT, &state);
		if (state == reg_value)
			break;
	}

	if (i >= 5) {
		FTS_ERROR("[GESTURE]Write fod reg failed!\n");
		return -EIO;
	}
	FTS_ERROR("[GESTURE]Write fod reg success!\n");
	return 0;
}

int fts_gesture_reg_write(u8 mask, bool enable)
{
	int i;
	u8 state;
	u8 reg_value;

	for (i = 0; i < 5; i++) {
		fts_read_reg(FTS_REG_GESTURE_EN, &reg_value);
		if (enable)
			reg_value |= mask;
		else
			reg_value &= ~mask;
		fts_write_reg(FTS_REG_GESTURE_EN, reg_value);
		msleep(1);
		fts_read_reg(FTS_REG_GESTURE_EN, &state);
		if (state == reg_value)
			break;
	}

	if (i >= 5) {
		FTS_ERROR("[GESTURE]Write gesture reg failed!\n");
		return -EIO;
	}
	FTS_ERROR("[GESTURE]Write gesture reg success!\n");
	return 0;
}


int fts_gesture_suspend(struct fts_ts_data *ts_data)
{
	int i = 0;
	int ret;
	u8 state = 0xFF;

	FTS_FUNC_ENTER();
	if (enable_irq_wake(ts_data->irq))
		FTS_DEBUG("enable_irq_wake(irq:%d) fail", ts_data->irq);

	for (i = 0; i < 5; i++) {
		fts_write_reg(0xD1, 0xFF);
		fts_write_reg(0xD2, 0xFF);
		fts_write_reg(0xD5, 0xFF);
		fts_write_reg(0xD6, 0xFF);
		fts_write_reg(0xD7, 0xFF);
		fts_write_reg(0xD8, 0xFF);
		fts_write_reg(FTS_REG_GESTURE_EN, ENABLE);
		msleep(1);
		fts_read_reg(FTS_REG_GESTURE_EN, &state);
		if (state == ENABLE)
			break;
	}
#ifdef FTS_TOUCHSCREEN_FOD
	ret = fts_fod_reg_write(FTS_REG_GESTURE_DOUBLETAP_ON, true);
	if (ret) {
		FTS_ERROR("[GESTURE]Enter into gesture(suspend) failed!\n");
		return -EIO;
	}
#endif

	if (i >= 5)
		FTS_ERROR("make IC enter into gesture(suspend) fail,state:%x", state);
	else
		FTS_INFO("Enter into gesture(suspend) successfully");

	FTS_FUNC_EXIT();
	return 0;
}

int fts_gesture_resume(struct fts_ts_data *ts_data)
{
	int i = 0;
	int ret;
	u8 state = 0xFF;

	FTS_FUNC_ENTER();
	if (disable_irq_wake(ts_data->irq))
		FTS_DEBUG("disable_irq_wake(irq:%d) fail", ts_data->irq);

	for (i = 0; i < 5; i++) {
		fts_write_reg(FTS_REG_GESTURE_EN, DISABLE);
		msleep(1);
		fts_read_reg(FTS_REG_GESTURE_EN, &state);
		if (state == DISABLE)
			break;
	}

#ifdef FTS_TOUCHSCREEN_FOD
	ret = fts_fod_reg_write(FTS_REG_GESTURE_DOUBLETAP_ON, false);
	if (ret) {
		FTS_ERROR("[GESTURE]resume from gesture(suspend) failed!\n");
		return -EIO;
	}
#endif

	if (i >= 5)
		FTS_ERROR("make IC exit gesture(resume) fail,state:%x", state);
	else
		FTS_INFO("resume from gesture successfully");

	FTS_FUNC_EXIT();
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

	fts_create_gesture_sysfs(ts_data->dev);

	memset(&fts_gesture_data, 0, sizeof(struct fts_gesture_st));
	ts_data->gesture_bmode = GESTURE_BM_REG;
	ts_data->gesture_support = FTS_GESTURE_EN;

	if ((ts_data->ic_info.ids.type <= 0x25)
		|| (ts_data->ic_info.ids.type == 0x87)
		|| (ts_data->ic_info.ids.type == 0x88)) {
		FTS_INFO("ic type:0x%02x,GESTURE_BM_TOUCH", ts_data->ic_info.ids.type);
		ts_data->touch_size += FTS_GESTURE_DATA_LEN;
		ts_data->gesture_bmode = GESTURE_BM_TOUCH;
	}

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
