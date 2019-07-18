/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2017, FocalTech Systems, Ltd., all rights reserved.
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
#include "focaltech_core.h"
#if defined(CONFIG_DRM) && defined(DRM_ADD_COMPLETE)
#include <linux/notifier.h>
#include <linux/fb.h>
#include <drm/drm_notifier.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define FTS_SUSPEND_LEVEL 1	/* Early-suspend level */
#endif
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
#include "../xiaomi/xiaomi_touch.h"
#endif
#include <linux/backlight.h>
#include <linux/input/touch_common_info.h>

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_NAME                     "fts_ts"
#define INTERVAL_READ_REG                   100	/* unit:ms */
#define TIMEOUT_READ_REG                    2000	/* unit:ms */
#ifdef FTS_POWER_SOURCE_CUST_EN
#define FTS_VTG_MIN_UV                      2600000
#define FTS_VTG_MAX_UV                      3300000
#define FTS_I2C_VTG_MIN_UV                  1800000
#define FTS_I2C_VTG_MAX_UV                  1800000
#endif

#define INPUT_EVENT_START			0
#define INPUT_EVENT_SENSITIVE_MODE_OFF		0
#define INPUT_EVENT_SENSITIVE_MODE_ON		1
#define INPUT_EVENT_STYLUS_MODE_OFF		2
#define INPUT_EVENT_STYLUS_MODE_ON		3
#define INPUT_EVENT_WAKUP_MODE_OFF		4
#define INPUT_EVENT_WAKUP_MODE_ON		5
#define INPUT_EVENT_COVER_MODE_OFF		6
#define INPUT_EVENT_COVER_MODE_ON		7
#define INPUT_EVENT_SLIDE_FOR_VOLUME		8
#define INPUT_EVENT_DOUBLE_TAP_FOR_VOLUME		9
#define INPUT_EVENT_SINGLE_TAP_FOR_VOLUME		10
#define INPUT_EVENT_LONG_SINGLE_TAP_FOR_VOLUME		11
#define INPUT_EVENT_PALM_OFF		12
#define INPUT_EVENT_PALM_ON		13
#define INPUT_EVENT_END				13

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/
struct fts_ts_data *fts_data;
#if FTS_CHARGER_EN
extern int fts_charger_mode_set(struct i2c_client *client, bool on);
#endif

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
extern void lpm_disable_for_input(bool on);
#ifndef CONFIG_FACTORY_BUILD
static int fts_ts_clear_buffer(void);
#endif
static void fts_release_all_finger(void);
static int fts_ts_suspend(struct device *dev);
static int fts_ts_resume(struct device *dev);
static void fts_resume_work(struct work_struct *work);
static void fts_suspend_work(struct work_struct *work);
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_PALMSENSOR
static int fts_read_palm_data(void);
#endif

struct device *fts_get_dev(void)
{
	if (!fts_data)
		return NULL;
	else
		return &(fts_data->client->dev);
}
/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief: Read chip id until TP FW become valid(Timeout: TIMEOUT_READ_REG),
*         need call when reset/power on/resume...
*  Input:
*  Output:
*  Return: return 0 if tp valid, otherwise return error code
*****************************************************************************/
int fts_wait_tp_to_valid(struct i2c_client *client)
{
	int ret = 0;
	int cnt = 0;
	u8 reg_value = 0;
	u8 chip_id = fts_data->ic_info.ids.chip_idh;

	do {
		ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID, &reg_value);
		if ((ret < 0) || (reg_value != chip_id)) {
			FTS_DEBUG("TP Not Ready, ReadData = 0x%x", reg_value);
		} else if (reg_value == chip_id) {
			FTS_INFO("TP Ready, Device ID = 0x%x", reg_value);
			return 0;
		}
		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	return -EIO;
}

/************************************************************************
* Name: fts_get_chip_types
* Brief: verity chip id and get chip type data
* Input:
* Output:
* Return: return 0 if success, otherwise return error code
***********************************************************************/
static int fts_get_chip_types(struct fts_ts_data *ts_data, u8 id_h, u8 id_l, bool fw_valid)
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

/*****************************************************************************
*  Name: fts_get_ic_information
*  Brief:
*  Input:
*  Output:
*  Return: return 0 if success, otherwise return error code
*****************************************************************************/
static int fts_get_ic_information(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int cnt = 0;
	u8 chip_id[2] = { 0 };
	u8 id_cmd[4] = { 0 };
	u32 id_cmd_len = 0;
	struct i2c_client *client = ts_data->client;

	ts_data->ic_info.is_incell = FTS_CHIP_IDC;
	ts_data->ic_info.hid_supported = FTS_HID_SUPPORTTED;
	do {
		ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID, &chip_id[0]);
		ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID2, &chip_id[1]);
		if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
			FTS_DEBUG("i2c read invalid, read:0x%02x%02x", chip_id[0], chip_id[1]);
		} else {
			ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], VALID);
			if (!ret)
				break;
			else
				FTS_DEBUG("TP not ready, read:0x%02x%02x", chip_id[0], chip_id[1]);
		}

		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	if ((cnt * INTERVAL_READ_REG) >= TIMEOUT_READ_REG) {
		FTS_INFO("fw is invalid, need read boot id");
		if (ts_data->ic_info.hid_supported) {
			fts_i2c_hid2std(client);
		}

		id_cmd[0] = FTS_CMD_START1;
		id_cmd[1] = FTS_CMD_START2;
		ret = fts_i2c_write(client, id_cmd, 2);
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
		ret = fts_i2c_read(client, id_cmd, id_cmd_len, chip_id, 2);
		if ((ret < 0) || (0x0 == chip_id[0]) || (0x0 == chip_id[1])) {
			FTS_ERROR("read boot id fail");
			return -EIO;
		}
		ret = fts_get_chip_types(ts_data, chip_id[0], chip_id[1], INVALID);
		if (ret < 0) {
			FTS_ERROR("can't get ic informaton");
			return ret;
		}
	}
	ts_data->chipid = (short)(chip_id[0] << 8 | chip_id[1]);
	FTS_INFO("get ic information, chip id = 0x%02x%02x", ts_data->ic_info.ids.chip_idh,
		 ts_data->ic_info.ids.chip_idl);

	return 0;
}

/*****************************************************************************
*  Name: fts_tp_state_recovery
*  Brief: Need execute this function when reset
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_tp_state_recovery(struct i2c_client *client)
{
	FTS_FUNC_ENTER();
	/* wait tp stable */
	fts_wait_tp_to_valid(client);
	/* recover TP charger state 0x8B */
	/* recover TP glove state 0xC0 */
	/* recover TP cover state 0xC1 */
	fts_ex_mode_recovery(client);
	/* recover TP gesture state 0xD0 */
#if FTS_GESTURE_EN
	fts_gesture_recovery(client);
#endif
	FTS_FUNC_EXIT();
}

/*****************************************************************************
*  Name: fts_reset_proc
*  Brief: Execute reset operation
*  Input: hdelayms - delay time unit:ms
*  Output:
*  Return:
*****************************************************************************/
int fts_reset_proc(int hdelayms)
{
	FTS_FUNC_ENTER();
	gpio_direction_output(fts_data->pdata->reset_gpio, 0);
	msleep(20);
	gpio_direction_output(fts_data->pdata->reset_gpio, 1);
	if (hdelayms) {
		msleep(hdelayms);
	}
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
	fts_fod_recovery(fts_data->client);
#endif

	FTS_FUNC_EXIT();
	return 0;
}

/*****************************************************************************
*  Name: fts_irq_disable
*  Brief: disable irq
*  Input:
*  Output:
*  Return:
*****************************************************************************/
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

/*****************************************************************************
*  Name: fts_irq_disable sync
*  Brief: disable irq sync
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_irq_disable_sync(void)
{

	if (!fts_data->irq_disabled) {
		disable_irq(fts_data->irq);
		FTS_INFO("irq is disabled\n");
		fts_data->irq_disabled = true;
	}

}


/*****************************************************************************
*  Name: fts_irq_enable
*  Brief: enable irq
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_irq_enable(void)
{
	unsigned long irqflags = 0;

	spin_lock_irqsave(&fts_data->irq_lock, irqflags);

	if (fts_data->irq_disabled) {
		enable_irq(fts_data->irq);
		FTS_INFO("irq is enabled\n");
		fts_data->irq_disabled = false;
	}
	spin_unlock_irqrestore(&fts_data->irq_lock, irqflags);

}

#ifdef FTS_POWER_SOURCE_CUST_EN
/*****************************************************************************
* Power Control
*****************************************************************************/
static int fts_power_source_init(struct fts_ts_data *data)
{
	int ret = 0;

	FTS_FUNC_ENTER();

	data->vddio = regulator_get(&data->client->dev, "vddio");
	if (IS_ERR(data->vddio)) {
		ret = PTR_ERR(data->vddio);
		FTS_ERROR("get vddio regulator failed,ret=%d", ret);
		return ret;
	}

	if (regulator_count_voltages(data->vddio) > 0) {
		ret = regulator_set_voltage(data->vddio, FTS_I2C_VTG_MIN_UV, FTS_I2C_VTG_MAX_UV);
		if (ret) {
			FTS_ERROR("vdd regulator set_vddio failed ret=%d", ret);
			goto err_set_vtg_vdd;
		}
	}

	data->avdd = regulator_get(&data->client->dev, "avdd");
	if (IS_ERR(data->avdd)) {
		ret = PTR_ERR(data->avdd);
		FTS_ERROR("get vddio regulator failed,ret=%d", ret);
		return ret;
	}
	regulator_set_load(data->avdd, data->pdata->avdd_load);

	data->vsp = regulator_get(&data->client->dev, "lab");
	if (IS_ERR(data->vsp)) {
		ret = PTR_ERR(data->vsp);
		FTS_ERROR("ret vsp regulator failed,ret=%d", ret);
		goto err_get_vsp;
	}

	data->vsn = regulator_get(&data->client->dev, "ibb");
	if (IS_ERR(data->vsn)) {
		ret = PTR_ERR(data->vsn);
		FTS_ERROR("ret vsn regulator failed,ret=%d", ret);
		goto err_get_vsn;
	}

	FTS_FUNC_EXIT();
	return 0;

err_get_vsn:
	regulator_put(data->vsp);
err_get_vsp:
err_set_vtg_vdd:
	regulator_put(data->vddio);

	FTS_FUNC_EXIT();
	return ret;
}

static int fts_power_source_release(struct fts_ts_data *data)
{
	if (regulator_count_voltages(data->vddio) > 0)
		regulator_set_voltage(data->vddio, 0, FTS_I2C_VTG_MAX_UV);
	regulator_put(data->vddio);

	regulator_put(data->vsp);

	regulator_put(data->vsn);

	return 0;
}

static int fts_power_source_ctrl(struct fts_ts_data *data, int enable)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	if (enable) {
		if (data->power_disabled) {
			ret = regulator_enable(data->avdd);
			if (ret) {
				FTS_ERROR("enable avdd regulator failed,ret=%d", ret);
			}
			ret = regulator_enable(data->vddio);
			if (ret) {
				FTS_ERROR("enable vddio regulator failed,ret=%d", ret);
			}

			ret = regulator_enable(data->vsp);
			if (ret) {
				FTS_ERROR("enable vsp regulator failed,ret=%d", ret);
			}

			ret = regulator_enable(data->vsn);
			if (ret) {
				FTS_ERROR("enable vsn regulator failed,ret=%d", ret);
			}
			data->power_disabled = false;
		}
	} else {
		if (!data->power_disabled) {
			ret = regulator_disable(data->avdd);
			if (ret) {
				FTS_ERROR("disable avdd regulator failed,ret=%d", ret);
			}
			ret = regulator_disable(data->vddio);
			if (ret) {
				FTS_ERROR("disable vddio regulator failed,ret=%d", ret);
			}

			ret = regulator_disable(data->vsp);
			if (ret) {
				FTS_ERROR("disable vsp regulator failed,ret=%d", ret);
			}

			ret = regulator_disable(data->vsn);
			if (ret) {
				FTS_ERROR("disable vsn regulator failed,ret=%d", ret);
			}

			data->power_disabled = true;
		}
	}

	FTS_FUNC_EXIT();
	return ret;
}

#if FTS_PINCTRL_EN
/*****************************************************************************
*  Name: fts_pinctrl_init
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_pinctrl_init(struct fts_ts_data *ts)
{
	int ret = 0;
	struct i2c_client *client = ts->client;

	ts->pinctrl = devm_pinctrl_get(&client->dev);
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

#endif /* FTS_POWER_SOURCE_CUST_EN */

/*****************************************************************************
*  Reprot related
*****************************************************************************/
#if (FTS_DEBUG_EN && (FTS_DEBUG_LEVEL == 2))
char g_sz_debug[1024] = { 0 };

static void fts_show_touch_buffer(u8 *buf, int point_num)
{
	int len = point_num * FTS_ONE_TCH_LEN;
	int count = 0;
	int i;

	memset(g_sz_debug, 0, 1024);
	if (len > (fts_data->pnt_buf_size - 3)) {
		len = fts_data->pnt_buf_size - 3;
	} else if (len == 0) {
		len += FTS_ONE_TCH_LEN;
	}
	count += snprintf(g_sz_debug, PAGE_SIZE, "%02X,%02X,%02X", buf[0], buf[1], buf[2]);
	for (i = 0; i < len; i++) {
		count += snprintf(g_sz_debug + count, PAGE_SIZE, ",%02X", buf[i + 3]);
	}
	FTS_DEBUG("buffer: %s", g_sz_debug);
}
#endif

/*****************************************************************************
 *  Name: fts_release_all_finger
 *  Brief: report all points' up events, release touch
 *  Input:
 *  Output:
 *  Return:
 *****************************************************************************/
static void fts_release_all_finger(void)
{
	struct input_dev *input_dev = fts_data->input_dev;
#if FTS_MT_PROTOCOL_B_EN
	u32 finger_count = 0;
#endif

	FTS_FUNC_ENTER();
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
	fts_data->finger_in_fod = false;
	fts_data->overlap_area = 0;
#endif
#if FTS_MT_PROTOCOL_B_EN
	for (finger_count = 0; finger_count < fts_data->pdata->max_touch_number; finger_count++) {
		input_mt_slot(input_dev, finger_count);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, false);
	}
#else
	input_mt_sync(input_dev);
#endif
	input_report_key(input_dev, BTN_TOUCH, 0);
	input_sync(input_dev);
	lpm_disable_for_input(false);
	FTS_FUNC_EXIT();
}

/************************************************************************
 * Name: fts_input_report_key
 * Brief: report key event
 * Input: events info
 * Output:
 * Return: return 0 if success
 ***********************************************************************/
static int fts_input_report_key(struct fts_ts_data *data, int index)
{
	u32 ik;
	int id = data->events[index].id;
	int x = data->events[index].x;
	int y = data->events[index].y;
	int flag = data->events[index].flag;
	u32 key_num = data->pdata->key_number;

	if (!KEY_EN(data)) {
		return -EINVAL;
	}
	for (ik = 0; ik < key_num; ik++) {
		if (TOUCH_IN_KEY(x, data->pdata->key_x_coords[ik])) {
			if (EVENT_DOWN(flag)) {
				data->key_down = true;
				input_report_key(data->input_dev, data->pdata->keys[ik], 1);
				FTS_DEBUG("Key%d(%d, %d) DOWN!", ik, x, y);
			} else {
				data->key_down = false;
				input_report_key(data->input_dev, data->pdata->keys[ik], 0);
				FTS_DEBUG("Key%d(%d, %d) Up!", ik, x, y);
			}
			return 0;
		}
	}

	FTS_ERROR("invalid touch for key, [%d](%d, %d)", id, x, y);
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
	u32 key_y_coor = data->pdata->key_y_coord;
	struct ts_event *events = data->events;

	if (!data->fod_point_released && data->point_num == 0) {
		fts_release_all_finger();
		FTS_INFO("%s Normal report release all fingers\n", __func__);
		data->fod_point_released = true;
	}
	for (i = 0; i < data->touch_point; i++) {
		if (KEY_EN(data) && TOUCH_IS_KEY(events[i].y, key_y_coor)) {
			fts_input_report_key(data, i);
			continue;
		}

		if (events[i].id >= max_touch_num)
			break;

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
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, data->overlap_area);
			input_report_abs(data->input_dev, ABS_MT_WIDTH_MINOR, data->overlap_area);

			touchs |= BIT(events[i].id);
			data->touchs |= BIT(events[i].id);
			if (data->point_id_changed && data->old_point_id != 0xff
				&& data->fod_status == 0 && data->old_point_id != events[i].id) {
				FTS_INFO("%s FOD finger ID is Changed, release old id\n", __func__);
				input_mt_slot(data->input_dev, data->old_point_id);
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
				input_sync(data->input_dev);
				data->old_point_id = 0xff;
				data->point_id_changed = false;
			}

			/*FTS_DEBUG("[B]P%d(%d, %d)[p:%d,tm:%d] DOWN!", events[i].id, events[i].x,
			   events[i].y, events[i].p, events[i].area); */
		} else {
			uppoint++;
#if FTS_REPORT_PRESSURE_EN
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, 0);
#endif
			input_mt_report_slot_state(data->input_dev,
						MT_TOOL_FINGER, false);
			data->touchs &= ~BIT(events[i].id);
			FTS_DEBUG("[B]P%d UP!", events[i].id);
		}
	}

	if (unlikely(data->touchs ^ touchs)) {
		for (i = 0; i < max_touch_num; i++) {
			if (BIT(i) & (data->touchs ^ touchs)) {
				/*FTS_DEBUG("[B]P%d UP!", i); */
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
			/*FTS_DEBUG("[B]Points All Up!"); */
			input_report_key(data->input_dev, BTN_TOUCH, 0);
			lpm_disable_for_input(false);
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
	u32 key_y_coor = data->pdata->key_y_coord;
	struct ts_event *events = data->events;

	for (i = 0; i < data->touch_point; i++) {
		if (KEY_EN(data) && TOUCH_IS_KEY(events[i].y, key_y_coor)) {
			fts_input_report_key(data, i);
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

			FTS_DEBUG("[A]P%d(%d, %d)[p:%d,tm:%d] DOWN!", events[i].id, events[i].x, events[i].y,
				  events[i].p, events[i].area);
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
			FTS_DEBUG("[A]Points All Up!");
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
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
static int fts_read_and_report_foddata(struct fts_ts_data *data)
{
	u8 buf[10] = { 0 };
	int ret;
	int x, y, z;

	buf[0] = FTS_REG_FOD_OUTPUT_ADDRESS;
	ret = fts_i2c_read(data->client, buf, 1, buf + 1, 9);
	if (ret < 0) {
		FTS_ERROR("read fod failed, ret:%d", ret);
		return ret;
	} else {
		/*
		 * buf[1]: point id
		 * buf[2]:event type， 0x24 is doubletap, 0x25 is single tap, 0x26 is fod pointer event
		 * buf[3]: touch area/fod sensor area
		 * buf[4]: touch area
		 * buf[5-8]: x,y position
		 * buf[9]:pointer up or down, 0 is down, 1 is up
		 * */
		switch (buf[2]) {
		case 0x24:
			FTS_INFO("DoubleClick Gesture detected, Wakeup panel\n");
			input_report_key(data->input_dev, KEY_WAKEUP, 1);
			input_sync(data->input_dev);
			input_report_key(data->input_dev, KEY_WAKEUP, 0);
			input_sync(data->input_dev);
			break;
		case 0x25:
			FTS_INFO("FOD status report KEY_GOTO\n");
			input_report_key(data->input_dev, KEY_GOTO, 1);
			input_sync(data->input_dev);
			input_report_key(data->input_dev, KEY_GOTO, 0);
			input_sync(data->input_dev);
			break;
		case 0x26:
			x = (buf[5] << 8) | buf[6];
			y = (buf[7] << 8) | buf[8];
			z = buf[4];
			pr_info("FTS:read fod data: 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x anxis_x: %d anxis_y: %d\n",
				buf[1], buf[2], buf[3], buf[4], buf[9], x, y);
			if (buf[9] == 0) {
				mutex_lock(&data->report_mutex);
				if (!data->fod_finger_skip) {
					input_report_key(data->input_dev, BTN_INFO, 1);
					input_sync(data->input_dev);
					data->overlap_area = 100;
				}
				if (data->old_point_id != buf[1]) {
					if (data->old_point_id == 0xff)
						data->old_point_id = buf[1];
					else {
						data->point_id_changed = true;
					}
				}
				data->finger_in_fod = true;
				if (!data->suspended) {
					pr_info("FTS:touch is not in suspend state, report x,y value by touch nomal report\n");
					mutex_unlock(&data->report_mutex);
					return -EINVAL;
				}

				if (!data->fod_finger_skip) {
					input_mt_slot(data->input_dev, buf[1]);
					input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 1);
					input_report_key(data->input_dev, BTN_TOUCH, 1);
					input_report_key(data->input_dev, BTN_TOOL_FINGER, 1);
					input_report_abs(data->input_dev, ABS_MT_POSITION_X, x);
					input_report_abs(data->input_dev, ABS_MT_POSITION_Y, y);
					input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, z);
					input_report_abs(data->input_dev, ABS_MT_WIDTH_MAJOR, data->overlap_area);
					input_report_abs(data->input_dev, ABS_MT_WIDTH_MINOR, data->overlap_area);
					input_report_abs(data->input_dev, ABS_MT_PRESSURE, z);
					input_sync(data->input_dev);
				}
				mutex_unlock(&data->report_mutex);
			} else {
				input_report_key(data->input_dev, BTN_INFO, 0);
				input_sync(data->input_dev);
				data->finger_in_fod = false;
				data->fod_finger_skip = false;
				data->old_point_id = 0xff;
				data->point_id_changed = false;
				FTS_INFO("set fod finger skip false, set old_point_id as default value\n");
				data->overlap_area = 0;
				if (!data->suspended) {
					pr_info("FTS:touch is not in suspend state, report x,y value by touch nomal report\n");
					return -EINVAL;
				}
				mutex_lock(&data->report_mutex);
				input_mt_slot(data->input_dev, buf[1]);
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, 0);
				input_report_key(data->input_dev, BTN_TOUCH, 0);
				input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, -1);
				input_sync(data->input_dev);
				mutex_unlock(&data->report_mutex);
			}
			break;
		default:
			data->overlap_area = 0;
			if (data->suspended)
				return 0;
			else
				return -EINVAL;
			break;
		}
		return 0;
	}
}
#endif

/*****************************************************************************
*  Name: fts_read_touchdata
*  Brief:
*  Input:
*  Output:
*  Return: return 0 if succuss
*****************************************************************************/
static int fts_read_touchdata(struct fts_ts_data *data)
{
	int ret = 0;
	int i = 0;
	u8 pointid;
	int base;
	struct ts_event *events = data->events;
	int max_touch_num = data->pdata->max_touch_number;
	u8 *buf = data->point_buf;
	struct i2c_client *client = data->client;
	u8 reg_value;

#if FTS_GESTURE_EN
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
	if (fts_read_and_report_foddata(data) == 0) {
		FTS_INFO("succuss to get gesture data in irq handler");
		ret = fts_i2c_read_reg(fts_data->client, FTS_REG_INT_ACK, &reg_value);
		if (ret < 0)
			FTS_ERROR("read int ack error");
		else
			return 1;
	}
#else
	if (0 == fts_gesture_readdata(data)) {
		FTS_INFO("succuss to get gesture data in irq handler");
		return 1;
	}
#endif
#endif
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_PALMSENSOR
	if (data->palm_sensor_switch)
		fts_read_palm_data();
#endif

#if FTS_POINT_REPORT_CHECK_EN
	fts_prc_queue_work(data);
#endif

	data->point_num = 0;
	data->touch_point = 0;

	memset(buf, 0xFF, data->pnt_buf_size);
	buf[0] = 0x00;

	ret = fts_i2c_read(data->client, buf, 1, buf, data->pnt_buf_size);
	if (ret < 0) {
		FTS_ERROR("read touchdata failed, ret:%d", ret);
		return ret;
	}
	data->point_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;

	if ((data->point_num == 0x0F) && (buf[1] == 0xFF) && (buf[2] == 0xFF)
		&& (buf[3] == 0xFF) && (buf[4] == 0xFF) && (buf[5] == 0xFF) && (buf[6] == 0xFF)) {
		FTS_INFO("touch buff is 0xff, need recovery state");
		fts_tp_state_recovery(client);
		return -EIO;
	}

	if (data->point_num > max_touch_num) {
		FTS_INFO("invalid point_num(%d)", data->point_num);
		data->point_num = max_touch_num;
		return -EIO;
	}
#if (FTS_DEBUG_EN && (FTS_DEBUG_LEVEL == 2))
	fts_show_touch_buffer(buf, data->point_num);
#endif

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
		events[i].x = ((buf[FTS_TOUCH_X_H_POS + base] & 0x0F) << 8) + (buf[FTS_TOUCH_X_L_POS + base] & 0xFF);
		events[i].y = ((buf[FTS_TOUCH_Y_H_POS + base] & 0x0F) << 8) + (buf[FTS_TOUCH_Y_L_POS + base] & 0xFF);
		events[i].flag = buf[FTS_TOUCH_EVENT_POS + base] >> 6;
		events[i].id = buf[FTS_TOUCH_ID_POS + base] >> 4;
		events[i].area = buf[FTS_TOUCH_AREA_POS + base] >> 4;
		events[i].p = buf[FTS_TOUCH_PRE_POS + base];

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

/*****************************************************************************
*  Name: fts_report_event
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void fts_report_event(struct fts_ts_data *data)
{
#if FTS_MT_PROTOCOL_B_EN
	fts_input_report_b(data);
#else
	fts_input_report_a(data);
#endif
}

/*****************************************************************************
*  Name: fts_ts_interrupt
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static irqreturn_t fts_ts_interrupt(int irq, void *data)
{
	int ret = 0;
	struct fts_ts_data *ts_data = (struct fts_ts_data *)data;

	if (!ts_data) {
		FTS_ERROR("[INTR]: Invalid fts_ts_data");
		return IRQ_HANDLED;
	}
#if FTS_ESDCHECK_EN
	fts_esdcheck_set_intr(1);
#endif

	if (ts_data->dev_pm_suspend) {
		ret = wait_for_completion_timeout(&ts_data->dev_pm_suspend_completion, msecs_to_jiffies(50));
		if (!ret) {
			FTS_ERROR("system(i2c) can't finished resuming procedure, skip it");
			return IRQ_HANDLED;
		}
	}
	lpm_disable_for_input(true);
	ret = fts_read_touchdata(ts_data);
	if (ret == 0) {
		mutex_lock(&ts_data->report_mutex);
		fts_report_event(ts_data);
		mutex_unlock(&ts_data->report_mutex);
	}
#if FTS_ESDCHECK_EN
	fts_esdcheck_set_intr(0);
#endif

	return IRQ_HANDLED;
}

/*****************************************************************************
*  Name: fts_irq_registration
*  Brief:
*  Input:
*  Output:
*  Return: return 0 if succuss, otherwise return error code
*****************************************************************************/
static int fts_irq_registration(struct fts_ts_data *ts_data)
{
	int ret = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;

	ts_data->irq = gpio_to_irq(pdata->irq_gpio);
	FTS_INFO("irq in ts_data:%d irq in client:%d", ts_data->irq, ts_data->client->irq);
	if (ts_data->irq != ts_data->client->irq)
		FTS_ERROR("IRQs are inconsistent, please check <interrupts> & <focaltech,irq-gpio> in DTS");

	if (0 == pdata->irq_gpio_flags)
		pdata->irq_gpio_flags = IRQF_TRIGGER_FALLING;
	FTS_INFO("irq flag:%x", pdata->irq_gpio_flags);
	ret =
	    request_threaded_irq(ts_data->irq, NULL, fts_ts_interrupt, pdata->irq_gpio_flags | IRQF_ONESHOT,
				 ts_data->client->name, ts_data);

	return ret;
}

#if FTS_GESTURE_EN
static void fts_switch_mode_work(struct work_struct *work)
{
	struct fts_mode_switch *ms = container_of(work, struct fts_mode_switch, switch_mode_work);
	unsigned char value = ms->mode;
	bool enable = false;

	FTS_INFO("%s mode %d", __func__, value);

	if (value >= INPUT_EVENT_WAKUP_MODE_OFF && value <= INPUT_EVENT_WAKUP_MODE_ON) {
		enable = !!(value - INPUT_EVENT_WAKUP_MODE_OFF);
		fts_gesture_enable(enable);
		ms->ts_data->lpwg_mode = enable;
	}

	if (ms != NULL) {
		kfree(ms);
		ms = NULL;
	}
}

static int fts_input_event(struct input_dev *dev, unsigned int type, unsigned int code, int value)
{
	struct fts_ts_data *ts_data = input_get_drvdata(dev);
	struct fts_mode_switch *ms;

	FTS_INFO("set input event value = %d", value);

	if (!ts_data) {
		FTS_ERROR("fts_ts_data is NULL");
		return 0;
	}

	if (type == EV_SYN && code == SYN_CONFIG) {
		if (value >= INPUT_EVENT_START && value <= INPUT_EVENT_END) {
			ms = (struct fts_mode_switch *)
			    kmalloc(sizeof(struct fts_mode_switch), GFP_ATOMIC);

			if (ms != NULL) {
				ms->ts_data = ts_data;
				ms->mode = (unsigned char)value;
				INIT_WORK(&ms->switch_mode_work, fts_switch_mode_work);
				schedule_work(&ms->switch_mode_work);
			} else {
				FTS_ERROR("failed in allocating memory for switching mode");
				return -ENOMEM;
			}
		} else {
			FTS_ERROR("%s Invalid event value\n", __func__);
			return -EINVAL;
		}
	}

	return 0;
}
#endif

/*****************************************************************************
*  Name: fts_input_init
*  Brief: input device init
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_input_init(struct fts_ts_data *ts_data)
{
	int ret = 0;
	int key_num = 0;
	struct fts_ts_platform_data *pdata = ts_data->pdata;
	struct input_dev *input_dev;
	int point_num;

	FTS_FUNC_ENTER();

	input_dev = input_allocate_device();
	if (!input_dev) {
		FTS_ERROR("Failed to allocate memory for input device");
		return -ENOMEM;
	}

	/* Init and register Input device */
	input_dev->name = FTS_DRIVER_NAME;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &ts_data->client->dev;
#if FTS_GESTURE_EN
	input_dev->event = fts_input_event;
#endif
	input_set_drvdata(input_dev, ts_data);

	__set_bit(EV_SYN, input_dev->evbit);
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(EV_KEY, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	if (pdata->have_key) {
		FTS_INFO("set key capabilities");
		for (key_num = 0; key_num < pdata->key_number; key_num++)
			input_set_capability(input_dev, EV_KEY, pdata->keys[key_num]);
	}
#if FTS_MT_PROTOCOL_B_EN
	input_mt_init_slots(input_dev, pdata->max_touch_number, INPUT_MT_DIRECT);
#else
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 0x0f, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min, pdata->x_max - 1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min, pdata->y_max - 1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
#if FTS_REPORT_PRESSURE_EN
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
#endif
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
	input_set_capability(input_dev, EV_KEY, KEY_GOTO);
	input_set_capability(input_dev, EV_KEY, BTN_INFO);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, pdata->x_min, pdata->x_max - 1, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MINOR, pdata->x_min, pdata->x_max - 1, 0, 0);
#endif
	point_num = pdata->max_touch_number;
	ts_data->pnt_buf_size = point_num * FTS_ONE_TCH_LEN + 3;
	ts_data->point_buf = (u8 *) kzalloc(ts_data->pnt_buf_size, GFP_KERNEL);
	if (!ts_data->point_buf) {
		FTS_ERROR("failed to alloc memory for point buf!");
		ret = -ENOMEM;
		goto err_point_buf;
	}

	ts_data->events = (struct ts_event *)kzalloc(point_num * sizeof(struct ts_event), GFP_KERNEL);
	if (!ts_data->events) {

		FTS_ERROR("failed to alloc memory for point events!");
		ret = -ENOMEM;
		goto err_event_buf;
	}
	ret = input_register_device(input_dev);
	if (ret) {
		FTS_ERROR("Input device registration failed");
		goto err_input_reg;
	}

	ts_data->input_dev = input_dev;

	FTS_FUNC_EXIT();
	return 0;

err_input_reg:
	kfree_safe(ts_data->events);

err_event_buf:
	kfree_safe(ts_data->point_buf);

err_point_buf:
	input_set_drvdata(input_dev, NULL);
	input_free_device(input_dev);
	input_dev = NULL;

	FTS_FUNC_EXIT();
	return ret;
}

/*****************************************************************************
*  Name: fts_gpio_configure
*  Brief: Configure IRQ&RESET GPIO
*  Input:
*  Output:
*  Return: return 0 if succuss
*****************************************************************************/
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

		msleep(20);

		ret = gpio_direction_output(data->pdata->reset_gpio, 1);
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

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
static struct xiaomi_touch_interface xiaomi_touch_interfaces;
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_PALMSENSOR
static int fts_read_palm_data(void)
{
	int ret = 0;
	u8 reg_value;

	if (fts_data == NULL)
		return -EINVAL;
	ret = fts_i2c_read_reg(fts_data->client, FTS_REG_FACE_DEC_MODE_STATUS, &reg_value);
	if (ret < 0) {
		FTS_ERROR("read face detec error");
		return -EINVAL;
	}
	if (reg_value == 0xC0) {
		FTS_INFO("%s ear enter\n", __func__);
		update_palm_sensor_value(1);
	} else if (reg_value == 0XE0) {
		FTS_INFO("%s ear leave\n", __func__);
		update_palm_sensor_value(0);
	}
	return 0;
}

static int fts_finger_report_disable(struct fts_ts_data *data, int value)
{
	int ret;

	if (data == NULL)
		return -EINVAL;
	ret = fts_i2c_write_reg(data->client, FTS_REG_FINGER_REPORT_DISABLE, value);
	if (ret < 0)
		FTS_ERROR("disable finger report error, ret=%d", ret);
	return ret;

}

static int fts_palm_sensor_cmd(int value)
{
	int ret;

	ret = fts_i2c_write_reg(fts_data->client, FTS_REG_FACE_DEC_MODE_EN, value);
	if (ret < 0)
		FTS_ERROR("set face detect enable error, ret=%d", ret);
	return ret;

}

static int fts_palm_sensor_write(int value)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	if (fts_data == NULL)
		return -EINVAL;
	fts_data->palm_sensor_switch = value;

	if (fts_data->suspended) {
		fts_data->palm_sensor_changed = false;
		return 0;
	}
	ret = fts_palm_sensor_cmd(value);

	if (ret >= 0)
		fts_data->palm_sensor_changed = true;
	FTS_FUNC_EXIT();

	return ret;
}
#endif

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
static void fts_init_touchmode_data(void)
{
	int i;
	int ret;
	u8 reg_value;

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

	/* sensivity */
	ret = fts_i2c_read_reg(fts_data->client, FTS_REG_SENSIVITY, &reg_value);
	if (ret < 0) {
		FTS_ERROR("read sensivity reg error");
	}
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MAX_VALUE] = 50;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_MIN_VALUE] = 35;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE] = reg_value;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][SET_CUR_VALUE] = reg_value;
	xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_CUR_VALUE] = reg_value;

	/*  Tolerance */
	ret = fts_i2c_read_reg(fts_data->client, FTS_REG_THDIFF, &reg_value);
	if (ret < 0) {
		FTS_ERROR("read reg thdiff error");
	}
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MAX_VALUE] = 255;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_MIN_VALUE] = 64;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE] = reg_value;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][SET_CUR_VALUE] = reg_value;
	xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_CUR_VALUE] = reg_value;
	/* edge filter orientation*/
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


	for (i = 0; i < Touch_Mode_NUM; i++) {
		FTS_INFO("mode:%d, set cur:%d, get cur:%d, def:%d min:%d max:%d\n",
			i,
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_CUR_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MIN_VALUE],
			xiaomi_touch_interfaces.touch_mode[i][GET_MAX_VALUE]);
	}

	return;
}

static void fts_update_touchmode_data(int mode)
{
	u8 temp_value;
	int ret;

	temp_value = (u8)xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE];
	switch (mode) {
	case Touch_Game_Mode:
		/*enable touch game mode,set tp into active mode, set high report rate*/
		if (temp_value == 1) {
			ret = fts_i2c_write_reg(fts_data->client, FTS_REG_MONITOR_MODE, 0);
			if (ret < 0)
				FTS_ERROR("disable monitor mode error, ret=%d\n", ret);
			fts_data->gamemode_enabled = true;
		} else {
			/*restore touch parameters */
			ret = fts_i2c_write_reg(fts_data->client, FTS_REG_MONITOR_MODE, 1);
			if (ret < 0)
				FTS_ERROR("restore monitor mode error, ret=%d\n", ret);
			ret = fts_i2c_write_reg(fts_data->client, FTS_REG_SENSIVITY, (u8)xiaomi_touch_interfaces.touch_mode[Touch_UP_THRESHOLD][GET_DEF_VALUE]);
			if (ret < 0)
				FTS_ERROR("restore sensitivity error, ret=%d\n", ret);
			ret = fts_i2c_write_reg(fts_data->client, FTS_REG_THDIFF, (u8)xiaomi_touch_interfaces.touch_mode[Touch_Tolerance][GET_DEF_VALUE]);
			if (ret < 0)
				FTS_ERROR("restore touch smooth error, ret=%d\n", ret);
			ret = fts_i2c_write_reg(fts_data->client, FTS_REG_EDGE_FILTER_ORIENTATION, (u8)xiaomi_touch_interfaces.touch_mode[Touch_Panel_Orientation][GET_DEF_VALUE]);
			if (ret < 0)
				FTS_ERROR("restore orientation error, ret=%d\n", ret);
			ret = fts_i2c_write_reg(fts_data->client, FTS_REG_EDGE_FILTER_LEVEL, (u8)xiaomi_touch_interfaces.touch_mode[Touch_Edge_Filter][GET_DEF_VALUE]);
			if (ret < 0)
				FTS_ERROR("restore orientation error, ret=%d\n", ret);
			fts_data->gamemode_enabled = false;
		}
		break;
	case Touch_Active_MODE:
		break;
	case Touch_UP_THRESHOLD:
			if (fts_data->gamemode_enabled) {
				ret = fts_i2c_write_reg(fts_data->client, FTS_REG_SENSIVITY, temp_value);
				if (ret < 0)
					FTS_ERROR("write sensitivity error, ret=%d\n", ret);
			}
		break;
	case Touch_Tolerance:
			if (fts_data->gamemode_enabled) {
				ret = fts_i2c_write_reg(fts_data->client, FTS_REG_THDIFF, temp_value);
				if (ret < 0)
					FTS_ERROR("write touch smooth error, ret=%d\n", ret);
			}
		break;
	case Touch_Panel_Orientation:
			if (temp_value == 0 || temp_value == 2) {
				ret = fts_i2c_write_reg(fts_data->client, FTS_REG_EDGE_FILTER_ORIENTATION, 0);
				if (ret < 0)
					FTS_ERROR("write orientation error, ret=%d\n", ret);
			}
			if (temp_value == 1) {
				ret = fts_i2c_write_reg(fts_data->client, FTS_REG_EDGE_FILTER_ORIENTATION, 1);
				if (ret < 0)
					FTS_ERROR("write orientation error, ret=%d\n", ret);
			}
			if (temp_value == 3) {
				ret = fts_i2c_write_reg(fts_data->client, FTS_REG_EDGE_FILTER_ORIENTATION, 2);
				if (ret < 0)
					FTS_ERROR("write orientation error, ret=%d\n", ret);
			}
		break;
	case Touch_Edge_Filter:
			if (fts_data->gamemode_enabled) {
				/*gamemode apk will set 0 as edge filter off,1 as level min, 2 as level middle,3 as level max
				 * register in touch ic:0 as default, 1 as edge filter off, 2 as level min, 3 as level middle,4 as level max
				 **/
				ret = fts_i2c_write_reg(fts_data->client, FTS_REG_EDGE_FILTER_LEVEL, temp_value + 1);
				if (ret < 0)
					FTS_ERROR("write edge filter level error, ret=%d\n", ret);
			}
		break;
	case Touch_Report_Rate:
		break;
	default:
		break;
	}
}

static int fts_set_cur_value(int mode, int value)
{

	FTS_INFO("%s, mode:%d, value:%d\n", __func__, mode, value);
	if (mode == Touch_Fod_Enable && fts_data && value >= 0) {
		fts_data->fod_status = value;
		return 0;
	}
	if (mode == Touch_Aod_Enable && fts_data && value >= 0) {
		fts_data->aod_status = value;
		return 0;
	}

	if (mode < Touch_Mode_NUM && mode >= 0) {

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
	} else {
		FTS_ERROR("%s, don't support\n",  __func__);
	}
	fts_update_touchmode_data(mode);

	return 0;
}

static int fts_get_mode_value(int mode, int value_type)
{
	int value = -1;

	if (mode < Touch_Mode_NUM && mode >= 0)
		value = xiaomi_touch_interfaces.touch_mode[mode][value_type];
	else
		FTS_ERROR("%s, don't support\n", __func__);

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
		FTS_ERROR("%s, don't support\n",  __func__);
	}
	FTS_INFO("%s, mode:%d, value:%d:%d:%d:%d\n", __func__, mode, value[0],
					value[1], value[2], value[3]);

	return 0;
}

static int fts_reset_mode(int mode)
{
	int i = 0;

	if (mode < Touch_Mode_NUM && mode > 0) {
		xiaomi_touch_interfaces.touch_mode[mode][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[mode][GET_DEF_VALUE];
	} else if (mode == 0) {
		for (i = 0; i < Touch_Mode_NUM; i++) {
			xiaomi_touch_interfaces.touch_mode[i][SET_CUR_VALUE] =
			xiaomi_touch_interfaces.touch_mode[i][GET_DEF_VALUE];
		}
	} else {
		FTS_ERROR("%s, don't support\n",  __func__);
	}

	FTS_ERROR("%s, mode:%d\n",  __func__, mode);
	fts_update_touchmode_data(mode);

	return 0;
}
#endif
#endif

/*****************************************************************************
*  Name: fts_get_dt_coords
*  Brief:
*  Input:
*  Output:
*  Return: return 0 if succuss, otherwise return error code
*****************************************************************************/
static int fts_get_dt_coords(struct device *dev, char *name, struct fts_ts_platform_data *pdata)
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
	ret = of_property_read_u32(np, "focaltech,lockdown-info-addr", &pdata->lockdown_info_addr);
	if (!ret)
		FTS_ERROR("Unable to get lockdown-info-addr");

	ret = of_property_read_u32_array(np, name, coords, coords_size);
	if (ret && (ret != -EINVAL)) {
		FTS_ERROR("Unable to read %s", name);
		return -ENODATA;
	}

	if (!strcmp(name, "focaltech,display-coords")) {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	} else {
		FTS_ERROR("unsupported property %s", name);
		return -EINVAL;
	}

	FTS_INFO("display x(%d %d) y(%d %d)", pdata->x_min, pdata->x_max, pdata->y_min, pdata->y_max);
	return 0;
}

/*****************************************************************************
*  Name: fts_parse_dt
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_parse_dt(struct device *dev, struct fts_ts_platform_data *pdata)
{
	int ret = 0;
	struct device_node *np = dev->of_node;
	u32 temp_val;

	FTS_FUNC_ENTER();

	ret = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (ret < 0)
		FTS_ERROR("Unable to get display-coords");

	/* key */
	pdata->have_key = of_property_read_bool(np, "focaltech,have-key");
	if (pdata->have_key) {
		ret = of_property_read_u32(np, "focaltech,key-number", &pdata->key_number);
		if (ret)
			FTS_ERROR("Key number undefined!");

		ret = of_property_read_u32_array(np, "focaltech,keys", pdata->keys, pdata->key_number);
		if (ret)
			FTS_ERROR("Keys undefined!");
		else if (pdata->key_number > FTS_MAX_KEYS)
			pdata->key_number = FTS_MAX_KEYS;

		ret = of_property_read_u32(np, "focaltech,key-y-coord", &pdata->key_y_coord);
		if (ret)
			FTS_ERROR("Key Y Coord undefined!");

		ret = of_property_read_u32_array(np, "focaltech,key-x-coords", pdata->key_x_coords, pdata->key_number);
		if (ret)
			FTS_ERROR("Key X Coords undefined!");

		FTS_INFO("VK(%d): (%d, %d, %d), [%d, %d, %d][%d]",
			 pdata->key_number, pdata->keys[0], pdata->keys[1], pdata->keys[2], pdata->key_x_coords[0],
			 pdata->key_x_coords[1], pdata->key_x_coords[2], pdata->key_y_coord);
	}

	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio", 0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0)
		FTS_ERROR("Unable to get reset_gpio");

	pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio", 0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0)
		FTS_ERROR("Unable to get irq_gpio");

	ret = of_property_read_u32(np, "focaltech,max-touch-number", &temp_val);
	if (0 == ret) {
		if (temp_val < 2)
			pdata->max_touch_number = 2;
		else if (temp_val > FTS_MAX_POINTS_SUPPORT)
			pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
		else
			pdata->max_touch_number = temp_val;
	} else {
		FTS_ERROR("Unable to get max-touch-number");
		pdata->max_touch_number = FTS_MAX_POINTS_SUPPORT;
	}

	FTS_INFO("max touch number:%d, irq gpio:%d, reset gpio:%d", pdata->max_touch_number, pdata->irq_gpio,
		 pdata->reset_gpio);
	ret = of_property_read_u32(np, "focaltech,avdd-load", &temp_val);
	if (ret == 0) {
		pdata->avdd_load = temp_val;
	} else {
		FTS_ERROR("Unable to get avdd load");
	}
	ret = of_property_read_u32(np, "focaltech,open-min", &pdata->open_min);
	if (ret)
		FTS_ERROR("selftest open min undefined!");

	pdata->reset_when_resume = of_property_read_bool(np, "focaltech,reset-when-resume");

	FTS_FUNC_EXIT();
	return 0;
}

static ssize_t fts_lockdown_info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	if (!ts_data)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
			(int)ts_data->lockdown_info[0], (int)ts_data->lockdown_info[1],
			(int)ts_data->lockdown_info[2], (int)ts_data->lockdown_info[3],
			(int)ts_data->lockdown_info[4], (int)ts_data->lockdown_info[5], (int)ts_data->lockdown_info[6],
			(int)ts_data->lockdown_info[7]);
}

static ssize_t fts_panel_color_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	if (!ts_data)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%c\n", ts_data->lockdown_info[2]);
}

static ssize_t fts_panel_vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	if (!ts_data)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%c\n", ts_data->lockdown_info[0]);
}

static ssize_t fts_panel_display_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	if (!ts_data)
		return -EINVAL;

	return snprintf(buf, PAGE_SIZE, "%c\n", ts_data->lockdown_info[1]);
}

#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
static ssize_t fts_gamemode_test_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	u8 monitor_mode, filter_ori, thdiff, sensitivity, report_rate, filter_level;

	fts_i2c_read_reg(fts_data->client, FTS_REG_MONITOR_MODE, &monitor_mode);
	fts_i2c_read_reg(fts_data->client, FTS_REG_REPORT_RATE, &report_rate);
	fts_i2c_read_reg(fts_data->client, FTS_REG_SENSIVITY, &sensitivity);
	fts_i2c_read_reg(fts_data->client, FTS_REG_THDIFF, &thdiff);
	fts_i2c_read_reg(fts_data->client, FTS_REG_EDGE_FILTER_ORIENTATION, &filter_ori);
	fts_i2c_read_reg(fts_data->client, FTS_REG_EDGE_FILTER_LEVEL, &filter_level);
	return snprintf(buf, PAGE_SIZE, "monitor_mode:0x%x\nreport_rate:0x%x\nsensitivity:0x%x\nfollowing performance:0x%x\nedge_filter_orieatation:0x%x\nedge_filter_level:0x%x\n",
			monitor_mode, report_rate, sensitivity, thdiff, filter_ori, filter_level);
}

static ssize_t fts_gamemode_test_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int mode, value;

	FTS_INFO("%s,buf:%s,count:%zu\n", __func__, buf, count);
	sscanf(buf, "%d %d", &mode, &value);
	fts_set_cur_value(mode, value);
	return count;
}
#endif

static DEVICE_ATTR(panel_vendor, 0644, fts_panel_vendor_show, NULL);
static DEVICE_ATTR(panel_color, 0644, fts_panel_color_show, NULL);
static DEVICE_ATTR(panel_display, 0644, fts_panel_display_show, NULL);
static DEVICE_ATTR(lockdown_info, 0644, fts_lockdown_info_show, NULL);
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
static DEVICE_ATTR(gamemode_test, 0644, fts_gamemode_test_show, fts_gamemode_test_store);
#endif

static struct attribute *fts_attrs[] = {
	&dev_attr_lockdown_info.attr,
	&dev_attr_panel_color.attr,
	&dev_attr_panel_vendor.attr,
	&dev_attr_panel_display.attr,
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
	&dev_attr_gamemode_test.attr,
#endif
	NULL
};

static const struct attribute_group fts_attr_group = {
	.attrs = fts_attrs
};

#define TP_INFO_MAX_LENGTH 50
static ssize_t fts_lockdown_info_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	int cnt = 0, ret = 0;
	char tmp[TP_INFO_MAX_LENGTH];

	if (*pos != 0)
		return 0;

	cnt =
	    snprintf(tmp, TP_INFO_MAX_LENGTH,
		     "0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
		     fts_data->lockdown_info[0], fts_data->lockdown_info[1],
		     fts_data->lockdown_info[2], fts_data->lockdown_info[3], fts_data->lockdown_info[4],
		     fts_data->lockdown_info[5], fts_data->lockdown_info[6], fts_data->lockdown_info[7]);
	ret = copy_to_user(buf, tmp, cnt);

	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}

static const struct file_operations fts_lockdown_info_ops = {
	.read = fts_lockdown_info_read,
};

static ssize_t fts_fw_version_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	int cnt = 0, ret = 0;
	char tmp[TP_INFO_MAX_LENGTH];
	u8 major_version, minor_version;

	if (*pos != 0)
		return 0;

	fts_i2c_read_reg(fts_data->client, FTS_REG_FW_VER, &major_version);
	fts_i2c_read_reg(fts_data->client, 0xAD, &minor_version);

	cnt = snprintf(tmp, TP_INFO_MAX_LENGTH, "0x%x:0x%x\n", major_version, minor_version);
	ret = copy_to_user(buf, tmp, cnt);
	*pos += cnt;
	if (ret != 0)
		return 0;
	else
		return cnt;
}

static const struct file_operations fts_fw_version_ops = {
	.read = fts_fw_version_read,
};

static void tpdbg_suspend(struct fts_ts_data *ts_data, bool enable)
{
	if (enable)
		fts_ts_suspend(&ts_data->client->dev);
	else
		fts_ts_resume(&ts_data->client->dev);
}

static int tpdbg_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t tpdbg_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{

	const char *str = "cmd support as below:\n \
				\necho \"irq-disable\" or \"irq-enable\" to ctrl irq\n \
				\necho \"tp-suspend-en\" or \"tp-suspend-off\" to ctrl panel in or off suspend status\n \
				\necho \"tp-sd-en\" or \"tp-sd-off\" to ctrl panel in or off sleep status\n";

	loff_t pos = *ppos;
	int len = strlen(str);

	if (pos < 0)
		return -EINVAL;
	if (pos >= len)
		return 0;

	if (copy_to_user(buf, str, len))
		return -EFAULT;

	*ppos = pos + len;

	return len;
}

static ssize_t tpdbg_write(struct file *file, const char __user *buf, size_t size, loff_t *ppos)
{
	struct fts_ts_data *ts_data = file->private_data;
	char *cmd = kzalloc(size, GFP_KERNEL);
	int ret = size;

	if (!cmd)
		return -ENOMEM;
	if (copy_from_user(cmd, buf, size)) {
		ret = -EFAULT;
		goto out;
	}

	if (!strncmp(cmd, "irq-disable", 11))
		fts_irq_disable();
	else if (!strncmp(cmd, "irq-enable", 10))
		fts_irq_enable();
	else if (!strncmp(cmd, "tp-suspend-en", 13))
		tpdbg_suspend(ts_data, true);
	else if (!strncmp(cmd, "tp-suspend-off", 14))
		tpdbg_suspend(ts_data, false);
	else if (!strncmp(cmd, "tp-sd-en", 8))
		tpdbg_suspend(ts_data, true);
	else if (!strncmp(cmd, "tp-sd-off", 9)) {
		tpdbg_suspend(ts_data, false);
	}
out:
	kfree(cmd);

	return ret;
}

static int tpdbg_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;

	return 0;
}

static const struct file_operations tpdbg_operations = {
	.owner = THIS_MODULE,
	.open = tpdbg_open,
	.read = tpdbg_read,
	.write = tpdbg_write,
	.release = tpdbg_release,
};

static void fts_power_supply_work(struct work_struct *work)
{
	struct fts_ts_data *ts_data = container_of(work, struct fts_ts_data, power_supply_work);
	int is_usb_exist;

	is_usb_exist = !!power_supply_is_system_supplied();
	if (is_usb_exist != ts_data->is_usb_exist || ts_data->is_usb_exist < 0) {
		ts_data->is_usb_exist = is_usb_exist;
		FTS_INFO("%s %d\n", __func__, is_usb_exist);
		if (is_usb_exist) {
			FTS_INFO("%s USB is exist\n", __func__);
			fts_charger_mode_set(ts_data->client, true);
		} else {
			FTS_INFO("%s USB is not exist\n", __func__);
			fts_charger_mode_set(ts_data->client, false);
		}
	}
}

static int fts_power_supply_event(struct notifier_block *nb, unsigned long event, void *ptr)
{
	struct fts_ts_data *ts_data = container_of(nb, struct fts_ts_data, power_supply_notifier);

	if (!ts_data)
		return 0;
	queue_work(ts_data->event_wq, &ts_data->power_supply_work);
	return 0;
}

static int fts_bl_state_chg_callback(struct notifier_block *nb,
				      unsigned long val, void *data)
{
	struct fts_ts_data *ts_data = container_of(nb, struct fts_ts_data, bl_notif);
	unsigned int blank;

	if (val != BACKLIGHT_UPDATED)
		return NOTIFY_OK;
	if (data && ts_data) {
		blank = *(int *)(data);
		FTS_INFO("%s: val:%lu,blank:%u\n", __func__, val, blank);
		if (!ts_data->suspended && blank == BACKLIGHT_OFF) {
#if defined(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE) && defined(CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_PALMSENSOR)
			if (ts_data->palm_sensor_switch) {
				FTS_ERROR("%seardet enabled, skip disable irq\n", __func__);
				return NOTIFY_OK;
			}
#endif
			fts_irq_disable_sync();
		} else {
			fts_irq_enable();
		}
	}
	return NOTIFY_OK;
}


#if defined(CONFIG_DRM) && defined(DRM_ADD_COMPLETE)
/*****************************************************************************
*  Name: fb_notifier_callback
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fb_notifier_callback(struct notifier_block *self, unsigned long event, void *data)
{
	struct drm_notify_data *evdata = data;
	int *blank;
	struct fts_ts_data *fts_data = container_of(self, struct fts_ts_data, fb_notif);

	FTS_FUNC_ENTER();

	if (evdata && evdata->data && event == DRM_EVENT_BLANK && fts_data && fts_data->client) {
		blank = evdata->data;

		flush_workqueue(fts_data->event_wq);

		if (*blank == DRM_BLANK_UNBLANK) {
			FTS_INFO("FTS do resume work\n");
			queue_work(fts_data->event_wq, &fts_data->resume_work);
		} else if (*blank == DRM_BLANK_POWERDOWN || *blank == DRM_BLANK_LP1 || *blank == DRM_BLANK_LP2) {
			FTS_INFO("FTS do suspend work by event %s\n", *blank == DRM_BLANK_POWERDOWN ? "POWER DOWN" : "LP");
			if (*blank == DRM_BLANK_POWERDOWN && fts_data->finger_in_fod) {
				FTS_INFO("set fod finger skip\n");
				fts_data->fod_finger_skip = true;
			}
			queue_work(fts_data->event_wq, &fts_data->suspend_work);
		}
	}

	return 0;
}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
/*****************************************************************************
*  Name: fts_ts_early_suspend
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void fts_ts_early_suspend(struct early_suspend *handler)
{
	struct fts_ts_data *data = container_of(handler,
						struct fts_ts_data,
						early_suspend);

	fts_ts_suspend(&data->client->dev);
}

/*****************************************************************************
*  Name: fts_ts_late_resume
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void fts_ts_late_resume(struct early_suspend *handler)
{
	struct fts_ts_data *data = container_of(handler,
						struct fts_ts_data,
						early_suspend);

	fts_ts_resume(&data->client->dev);
}
#endif

static int check_is_focal_touch(struct fts_ts_data *ts_data)
{
	int ret = false;
	u8 cmd[4] = { 0 };
	u32 cmd_len = 0;
	u8 val[2] = { 0 };

	fts_reset_proc(10);
	cmd[0] = FTS_CMD_START1;
	cmd[1] = FTS_CMD_START2;
	ret = fts_i2c_write(ts_data->client, cmd, 2);

	if (ret < 0) {
		FTS_ERROR("write 55 aa cmd fail");
		return false;
	}

	msleep(FTS_CMD_START_DELAY);
	cmd[0] = FTS_CMD_READ_ID;
	cmd[1] = cmd[2] = cmd[3] = 0x00;

	cmd_len = FTS_CMD_READ_ID_LEN_INCELL;

	ret = fts_i2c_read(ts_data->client, cmd, cmd_len, val, 2);
	if (ret < 0) {
		FTS_ERROR("write 90 cmd fail");
		return false;
	}

	FTS_INFO("read boot id:0x%02x%02x", val[0], val[1]);

	return true;
}

/*****************************************************************************
*  Name: fts_ts_probe
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = 0;
	struct fts_ts_platform_data *pdata;
	struct fts_ts_data *ts_data;
	struct dentry *tp_debugfs;

	FTS_FUNC_ENTER();
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		FTS_ERROR("I2C not supported");
		return -ENODEV;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			FTS_ERROR("Failed to allocate memory for platform data");
			return -ENOMEM;
		}
		ret = fts_parse_dt(&client->dev, pdata);
		if (ret)
			FTS_ERROR("[DTS]DT parsing failed");
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		FTS_ERROR("no ts platform data found");
		return -EINVAL;
	}

	ts_data = devm_kzalloc(&client->dev, sizeof(*ts_data), GFP_KERNEL);
	if (!ts_data) {
		FTS_ERROR("Failed to allocate memory for fts_data");
		return -ENOMEM;
	}

	fts_data = ts_data;
	ts_data->client = client;
	ts_data->pdata = pdata;
	ts_data->old_point_id = 0xff;
	ts_data->point_id_changed = false;
	i2c_set_clientdata(client, ts_data);

	ts_data->ts_workqueue = create_singlethread_workqueue("fts_wq");
	if (NULL == ts_data->ts_workqueue) {
		FTS_ERROR("failed to create fts workqueue");
	}

	spin_lock_init(&ts_data->irq_lock);
	mutex_init(&ts_data->report_mutex);
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
	mutex_init(&ts_data->fod_mutex);
	ts_data->fod_status = -1;
#endif

	ret = fts_input_init(ts_data);
	if (ret) {
		FTS_ERROR("fts input initialize fail");
		goto err_input_init;
	}
#ifdef FTS_POWER_SOURCE_CUST_EN
	ret = fts_power_source_init(ts_data);
	if (ret) {
		FTS_ERROR("fail to get vdd/vcc_i2c regulator");
		goto err_power_init;
	}

	ts_data->power_disabled = true;
	ret = fts_power_source_ctrl(ts_data, ENABLE);
	if (ret) {
		FTS_ERROR("fail to enable vdd/vcc_i2c regulator");
		goto err_power_ctrl;
	}
#if FTS_PINCTRL_EN
	ret = fts_pinctrl_init(ts_data);
	if (0 == ret) {
		fts_pinctrl_select_normal(ts_data);
	}
#endif
#endif

	ret = fts_gpio_configure(ts_data);
	if (ret) {
		FTS_ERROR("[GPIO]Failed to configure the gpios");
		goto err_gpio_config;
	}
#if (!FTS_CHIP_IDC)
	fts_reset_proc(200);
#endif

	ret = fts_get_ic_information(ts_data);
	if (ret) {
		FTS_ERROR("can't get ic information");
		ret = check_is_focal_touch(ts_data);
		if (ret)
			ts_data->fw_forceupdate = true;
		else {
			FTS_ERROR("No focal touch found");
			ret = -ENODEV;
			goto err_irq_req;
		}
	}

	ret = sysfs_create_group(&client->dev.kobj, &fts_attr_group);
	if (ret) {
		FTS_ERROR("fail to export sysfs entires\n");
		goto err_irq_req;
	}

	ts_data->tp_lockdown_info_proc = proc_create("tp_lockdown_info", 0644, NULL, &fts_lockdown_info_ops);
	ts_data->tp_fw_version_proc = proc_create("tp_fw_version", 0644, NULL, &fts_fw_version_ops);

	ts_data->debugfs = debugfs_create_dir("tp_debug", NULL);
	if (!ts_data->debugfs) {
		FTS_ERROR("create tp_debug fail");
		goto err_sysfs_create_group;
	}
	tp_debugfs = debugfs_create_file("switch_state", 0660, ts_data->debugfs, ts_data, &tpdbg_operations);
	if (!tp_debugfs) {
		FTS_ERROR("create debugfs fail");
		goto err_sysfs_create_group;
	}
#if FTS_APK_NODE_EN
	ret = fts_create_apk_debug_channel(ts_data);
	if (ret) {
		FTS_ERROR("create apk debug node fail");
		goto err_debugfs_create;
	}
#endif

#if FTS_SYSFS_NODE_EN
	ret = fts_create_sysfs(client);
	if (ret) {
		FTS_ERROR("create sysfs node fail");
		goto err_debugfs_create;
	}
#endif

#if FTS_POINT_REPORT_CHECK_EN
	ret = fts_point_report_check_init(ts_data);
	if (ret) {
		FTS_ERROR("init point report check fail");
		goto err_debugfs_create;
	}
#endif

	ret = fts_ex_mode_init(client);
	if (ret) {
		FTS_ERROR("init glove/cover/charger fail");
		goto err_debugfs_create;
	}
#if FTS_GESTURE_EN
	ret = fts_gesture_init(ts_data);
	if (ret) {
		FTS_ERROR("init gesture fail");
		goto err_debugfs_create;
	}
#endif

#if FTS_TEST_EN
	ret = fts_test_init(client);
	if (ret) {
		FTS_ERROR("init production test fail");
		goto err_debugfs_create;
	}
#endif

#if FTS_ESDCHECK_EN
	ret = fts_esdcheck_init(ts_data);
	if (ret) {
		FTS_ERROR("init esd check fail");
		goto err_debugfs_create;
	}
#endif

	ts_data->event_wq =
	    alloc_workqueue("fts-event-queue",
			    WQ_UNBOUND | WQ_HIGHPRI | WQ_CPU_INTENSIVE, 1);
	if (!ts_data->event_wq) {
		FTS_ERROR("Cannot create work thread\n");
		goto err_debugfs_create;
	}
	INIT_WORK(&ts_data->resume_work, fts_resume_work);
	INIT_WORK(&ts_data->suspend_work, fts_suspend_work);
	INIT_WORK(&ts_data->power_supply_work, fts_power_supply_work);
	ts_data->is_usb_exist = -1;

	ret = fts_irq_registration(ts_data);
	if (ret) {
		FTS_ERROR("request irq failed");
		goto err_event_wq;
	}
	device_init_wakeup(&client->dev, 1);
	ts_data->dev_pm_suspend = false;
	init_completion(&ts_data->dev_pm_suspend_completion);

#if  defined(CONFIG_DRM) && defined(DRM_ADD_COMPLETE)
	ts_data->fb_notif.notifier_call = fb_notifier_callback;
	ret = drm_register_client(&ts_data->fb_notif);
	if (ret) {
		FTS_ERROR("[FB]Unable to register fb_notifier: %d", ret);
	}
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	ts_data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + FTS_SUSPEND_LEVEL;
	ts_data->early_suspend.suspend = fts_ts_early_suspend;
	ts_data->early_suspend.resume = fts_ts_late_resume;
	register_early_suspend(&ts_data->early_suspend);
#endif
	ts_data->bl_notif.notifier_call = fts_bl_state_chg_callback;
	if (backlight_register_notifier(&ts_data->bl_notif) < 0) {
		FTS_ERROR("register bl_notifier failed!\n");
	}
	ts_data->power_supply_notifier.notifier_call = fts_power_supply_event;
	power_supply_reg_notifier(&ts_data->power_supply_notifier);
	if (ts_data->fts_tp_class == NULL) {
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
		ts_data->fts_tp_class = get_xiaomi_touch_class();
#else
		ts_data->fts_tp_class = class_create(THIS_MODULE, "touch");
#endif
		if (ts_data->fts_tp_class) {
			ts_data->fts_touch_dev = device_create(ts_data->fts_tp_class, NULL, 0x38, ts_data, "tp_dev");
			if (IS_ERR(ts_data->fts_touch_dev)) {
				FTS_ERROR("Failed to create device !\n");
				goto err_class_create;
			}
			dev_set_drvdata(ts_data->fts_touch_dev, ts_data);
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE
			memset(&xiaomi_touch_interfaces, 0x00, sizeof(struct xiaomi_touch_interface));
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_GAMEMODE
			xiaomi_touch_interfaces.getModeValue = fts_get_mode_value;
			xiaomi_touch_interfaces.setModeValue = fts_set_cur_value;
			xiaomi_touch_interfaces.resetMode = fts_reset_mode;
			xiaomi_touch_interfaces.getModeAll = fts_get_mode_all;
			fts_init_touchmode_data();
#endif
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_PALMSENSOR
			xiaomi_touch_interfaces.palm_sensor_write = fts_palm_sensor_write;
#endif
			xiaomitouch_register_modedata(&xiaomi_touch_interfaces);
#endif
		}
	}
#if FTS_AUTO_UPGRADE_EN
	ret = fts_fwupg_init(ts_data);
	if (ret) {
		FTS_ERROR("init fw upgrade fail");
	}
#endif

	FTS_FUNC_EXIT();
	return 0;
err_class_create:
	class_destroy(ts_data->fts_tp_class);
	ts_data->fts_tp_class = NULL;
err_event_wq:
	if (ts_data->event_wq)
		destroy_workqueue(ts_data->event_wq);
err_debugfs_create:
	if (tp_debugfs)
		debugfs_remove(tp_debugfs);
err_sysfs_create_group:
	sysfs_remove_group(&client->dev.kobj, &fts_attr_group);
err_irq_req:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_gpio_config:
#ifdef FTS_POWER_SOURCE_CUST_EN
#if FTS_PINCTRL_EN
	fts_pinctrl_select_release(ts_data);
#endif
	fts_power_source_ctrl(ts_data, DISABLE);
err_power_ctrl:
	fts_power_source_release(ts_data);
err_power_init:
#endif
	kfree_safe(ts_data->point_buf);
	kfree_safe(ts_data->events);
	input_unregister_device(ts_data->input_dev);
err_input_init:
	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);
	devm_kfree(&client->dev, ts_data);

	FTS_FUNC_EXIT();
	return ret;
}

/*****************************************************************************
*  Name: fts_ts_remove
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_ts_remove(struct i2c_client *client)
{
	struct fts_ts_data *ts_data = i2c_get_clientdata(client);

	FTS_FUNC_ENTER();
	device_destroy(ts_data->fts_tp_class, 0x38);
	class_destroy(ts_data->fts_tp_class);
	ts_data->fts_tp_class = NULL;

	sysfs_remove_group(&client->dev.kobj, &fts_attr_group);

#if FTS_POINT_REPORT_CHECK_EN
	fts_point_report_check_exit(ts_data);
#endif

#if FTS_APK_NODE_EN
	fts_release_apk_debug_channel(ts_data);
#endif

#if FTS_SYSFS_NODE_EN
	fts_remove_sysfs(client);
#endif
	destroy_workqueue(ts_data->event_wq);

	fts_ex_mode_exit(client);

#if FTS_AUTO_UPGRADE_EN
	fts_fwupg_exit(ts_data);
#endif

#if FTS_TEST_EN
	fts_test_exit(client);
#endif

#if FTS_ESDCHECK_EN
	fts_esdcheck_exit(ts_data);
#endif

	backlight_unregister_notifier(&ts_data->bl_notif);
	power_supply_unreg_notifier(&ts_data->power_supply_notifier);
#if defined(CONFIG_DRM) && defined(DRM_ADD_COMPLETE)
	if (drm_unregister_client(&ts_data->fb_notif))
		FTS_ERROR("Error occurred while unregistering fb_notifier.");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&ts_data->early_suspend);
#endif

	free_irq(client->irq, ts_data);
	input_unregister_device(ts_data->input_dev);

	if (gpio_is_valid(ts_data->pdata->reset_gpio))
		gpio_free(ts_data->pdata->reset_gpio);

	if (gpio_is_valid(ts_data->pdata->irq_gpio))
		gpio_free(ts_data->pdata->irq_gpio);

	if (ts_data->ts_workqueue)
		destroy_workqueue(ts_data->ts_workqueue);

#ifdef FTS_POWER_SOURCE_CUST_EN
#if FTS_PINCTRL_EN
	fts_pinctrl_select_release(ts_data);
#endif
	fts_power_source_ctrl(ts_data, DISABLE);
	fts_power_source_release(ts_data);
#endif

	kfree_safe(ts_data->point_buf);
	kfree_safe(ts_data->events);

	devm_kfree(&client->dev, ts_data);

	FTS_FUNC_EXIT();
	return 0;
}
#ifndef CONFIG_FACTORY_BUILD
/*****************************************************************************
* Name: fts_ts_clear_buffer
* Brief: during irq disabled, FTS_REG_INT_ACK(0x3E) register will store three frames
* touchevent data. This register need to be cleared in case some points lost Tracking ID
* Input:
* Output:
* Return:
*****************************************************************************/
static int fts_ts_clear_buffer(void)
{
	int ret = 0;
	int retry = 0;
	u8 reg_value;
	for (retry = 0; retry < 3; retry++) {
		ret = fts_i2c_read_reg(fts_data->client, FTS_REG_INT_ACK, &reg_value);
		if (ret < 0) {
			FTS_ERROR("Read int ack error, retry %d", retry);
			break;
		}
	}
	return ret;
}
#endif

/*****************************************************************************
*  Name: fts_ts_suspend
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_ts_suspend(struct device *dev)
{
	int ret = 0;
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);

	FTS_FUNC_ENTER();
	if (ts_data->suspended) {
		FTS_INFO("Already in suspend state");
		return 0;
	}

	if (ts_data->fw_loading) {
		FTS_INFO("fw upgrade in process, can't suspend");
		return 0;
	}
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
	mutex_lock(&ts_data->fod_mutex);
#endif
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_PALMSENSOR
	if (ts_data->palm_sensor_switch) {
		ret = fts_finger_report_disable(ts_data, true);
		if (ret < 0)
				FTS_ERROR("%s: disable finger report error\n", __func__);
		else
				FTS_INFO("%s: disable finger report\n", __func__);
		FTS_INFO("%s: eardet enabled, disable finger report and skip suspend\n", __func__);
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
		mutex_unlock(&ts_data->fod_mutex);
#endif
		return 0;
	}
#endif
#if FTS_ESDCHECK_EN
	fts_esdcheck_suspend();
#endif
	fts_irq_disable_sync();
	mutex_lock(&fts_data->report_mutex);
	fts_release_all_finger();
	mutex_unlock(&fts_data->report_mutex);
#ifndef CONFIG_FACTORY_BUILD
#if FTS_GESTURE_EN
	if (fts_gesture_suspend(ts_data->client) == 0) {
		ts_data->suspended = true;
		ts_data->old_point_id = 0xff;
		ts_data->point_id_changed = false;
		FTS_INFO("%s Tp enter suspend, set old_point_id as default value\n", __func__);
		fts_irq_enable();
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
	mutex_unlock(&ts_data->fod_mutex);
#endif
		return 0;
	}
#endif
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
/* fod status = -1 as default value, means fingerprint is not enabled*
 * fod_status = 100 as all fingers in the system is deleted
 * aod_status != 0 means single tap in aod is supported
 */
	if (ts_data->aod_status || (ts_data->fod_status != -1 && ts_data->fod_status != 100)) {
		fts_fod_reg_write(ts_data->client, FTS_REG_GESTURE_FOD_ON, true);
		fts_gesture_reg_write(ts_data->client, FTS_REG_GESTURE_DOUBLETAP_ON, true);
		fts_ts_clear_buffer();
		fts_irq_enable();
		ts_data->suspended = true;
		ts_data->old_point_id = 0xff;
		ts_data->point_id_changed = false;
		FTS_INFO("%s Tp enter suspend, set old_point_id as default value\n", __func__);
		mutex_unlock(&ts_data->fod_mutex);
		return 0;
	}
#endif
#endif
#if FTS_PINCTRL_EN
	fts_pinctrl_select_suspend(ts_data);
#endif
#if defined(FTS_POWER_SOURCE_CUST_EN) && defined(CONFIG_FACTORY_BUILD)
	ret = fts_power_source_ctrl(ts_data, DISABLE);
	if (ret < 0) {
		FTS_ERROR("power off fail, ret=%d", ret);
	}
#else
	/* TP enter sleep mode */
	ret = fts_i2c_write_reg(ts_data->client, FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP_VALUE);
	if (ret < 0)
		FTS_ERROR("set TP to sleep mode fail, ret=%d", ret);
#endif
	ts_data->suspended = true;
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
	mutex_unlock(&ts_data->fod_mutex);
#endif
	FTS_FUNC_EXIT();
	return 0;
}

/*****************************************************************************
*  Name: fts_ts_resume
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_ts_resume(struct device *dev)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_PALMSENSOR
	int res;
#endif

	FTS_FUNC_ENTER();
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_PALMSENSOR
		res = fts_finger_report_disable(ts_data, false);
		if (res < 0)
				FTS_ERROR("%s: enable finger report error\n",  __func__);
		else
				FTS_INFO("%s: enable finger report\n", __func__);
#endif
	if (!ts_data->suspended) {
		FTS_DEBUG("Already in awake state");
		return 0;
	}
#if defined(FTS_POWER_SOURCE_CUST_EN) && defined(CONFIG_FACTORY_BUILD)
	fts_power_source_ctrl(ts_data, ENABLE);
#endif
#if FTS_PINCTRL_EN
	fts_pinctrl_select_normal(ts_data);
#endif
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
	FTS_INFO("%s finger_in_fod:%d\n", __func__, ts_data->finger_in_fod);
	if (ts_data->pdata->reset_when_resume && !ts_data->finger_in_fod) {
#else
	if (ts_data->pdata->reset_when_resume) {
#endif
		FTS_INFO("reset when resume");
		fts_reset_proc(200);
		mutex_lock(&fts_data->report_mutex);
		fts_release_all_finger();
		mutex_unlock(&fts_data->report_mutex);
	}
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
	if (fts_data->finger_in_fod) {
		fts_fod_reg_write(ts_data->client, FTS_REG_GESTURE_FOD_NO_CAL, true);
	}
#endif

	fts_tp_state_recovery(ts_data->client);

#if FTS_ESDCHECK_EN
	fts_esdcheck_resume();
#endif
#if FTS_GESTURE_EN
	if (fts_gesture_resume(ts_data->client) == 0) {
		ts_data->suspended = false;
		ts_data->fod_point_released = false;
		return 0;
	}
#endif
#ifdef CONFIG_TOUCHSCREEN_FTS_FOD
	fts_gesture_reg_write(ts_data->client, FTS_REG_GESTURE_DOUBLETAP_ON, false);
#endif
	ts_data->suspended = false;
	fts_irq_enable();
#ifdef CONFIG_TOUCHSCREEN_XIAOMI_TOUCHFEATURE_PALMSENSOR
	if (ts_data->palm_sensor_switch && !ts_data->palm_sensor_changed) {
		fts_palm_sensor_cmd(ts_data->palm_sensor_switch);
		ts_data->palm_sensor_changed = true;
	}
#endif

	FTS_FUNC_EXIT();
	return 0;
}

static int fts_pm_suspend(struct device *dev)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	int ret = 0;

	ts_data->dev_pm_suspend = true;

#ifndef CONFIG_TOUCHSCREEN_FTS_FOD
	if (ts_data->lpwg_mode) {
#endif
		ret = enable_irq_wake(ts_data->irq);
		if (ret) {
			FTS_INFO("enable_irq_wake(irq:%d) failed", ts_data->irq);
		}
#ifndef CONFIG_TOUCHSCREEN_FTS_FOD
	}
#endif

	reinit_completion(&ts_data->dev_pm_suspend_completion);

	return ret;
}

static int fts_pm_resume(struct device *dev)
{
	struct fts_ts_data *ts_data = dev_get_drvdata(dev);
	int ret = 0;

	ts_data->dev_pm_suspend = false;

#ifndef CONFIG_TOUCHSCREEN_FTS_FOD
	if (ts_data->lpwg_mode) {
#endif
		ret = disable_irq_wake(ts_data->irq);
		if (ret) {
			FTS_INFO("disable_irq_wake(irq:%d) failed", ts_data->irq);
		}
#ifndef CONFIG_TOUCHSCREEN_FTS_FOD
	}
#endif

	complete(&ts_data->dev_pm_suspend_completion);

	return 0;
}

static const struct dev_pm_ops fts_dev_pm_ops = {
	.suspend = fts_pm_suspend,
	.resume = fts_pm_resume,
};

static void fts_resume_work(struct work_struct *work)
{
	struct fts_ts_data *ts;
	ts = container_of(work, struct fts_ts_data, resume_work);
	fts_ts_resume(&ts->client->dev);
}

static void fts_suspend_work(struct work_struct *work)
{
	struct fts_ts_data *ts;
	ts = container_of(work, struct fts_ts_data, suspend_work);
	fts_ts_suspend(&ts->client->dev);
}


/*****************************************************************************
* I2C Driver
*****************************************************************************/
static const struct i2c_device_id fts_ts_id[] = {
	{FTS_DRIVER_NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, fts_ts_id);

static struct of_device_id fts_match_table[] = {
	{.compatible = "focaltech,focal",},
	{},
};

static struct i2c_driver fts_ts_driver = {
	.probe = fts_ts_probe,
	.remove = fts_ts_remove,
	.driver = {
		   .name = FTS_DRIVER_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_PM
		   .pm = &fts_dev_pm_ops,
#endif
		   .of_match_table = fts_match_table,
		   },
	.id_table = fts_ts_id,
};

/*****************************************************************************
*  Name: fts_ts_init
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int __init fts_ts_init(void)
{
	int ret = 0;

	FTS_FUNC_ENTER();
	ret = i2c_add_driver(&fts_ts_driver);
	if (ret != 0) {
		FTS_ERROR("Focaltech touch screen driver init failed!");
	}
	FTS_FUNC_EXIT();
	return ret;
}

/*****************************************************************************
*  Name: fts_ts_exit
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void __exit fts_ts_exit(void)
{
	i2c_del_driver(&fts_ts_driver);
}

module_init(fts_ts_init);
module_exit(fts_ts_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver");
MODULE_LICENSE("GPL v2");
