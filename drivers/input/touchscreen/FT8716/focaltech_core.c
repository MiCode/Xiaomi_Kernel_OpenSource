/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2017, FocalTech Systems, Ltd., all rights reserved.
 * Copyright (C) 2018 XiaoMi, Inc.
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
* Abstract:
*
* Reference:
*
*****************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "focaltech_core.h"
#if FTS_GESTURE_EN
extern struct gesture_struct gesture_data;
#define MXT_INPUT_EVENT_START			0
#define MXT_INPUT_EVENT_SENSITIVE_MODE_OFF	0
#define MXT_INPUT_EVENT_SENSITIVE_MODE_ON	1
#define MXT_INPUT_EVENT_STYLUS_MODE_OFF		2
#define MXT_INPUT_EVENT_STYLUS_MODE_ON		3
#define MXT_INPUT_EVENT_WAKUP_MODE_OFF		4
#define MXT_INPUT_EVENT_WAKUP_MODE_ON		5
#define MXT_INPUT_EVENT_EDGE_DISABLE		6
#define MXT_INPUT_EVENT_EDGE_FINGER		7
#define MXT_INPUT_EVENT_EDGE_HANDGRIP		8
#define MXT_INPUT_EVENT_EDGE_FINGER_HANDGRIP	9
#define MXT_INPUT_EVENT_END			9
#endif
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define FTS_SUSPEND_LEVEL 1     /* Early-suspend level */
#endif
#include <linux/hardware_info.h>
/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_NAME                     "fts_ts"
#define INTERVAL_READ_REG                   20
#define TIMEOUT_READ_REG                    300
#if FTS_POWER_SOURCE_CUST_EN
#define FTS_VTG_MIN_UV                      1600000
#define FTS_VTG_MAX_UV                      2000000
#define FTS_I2C_VTG_MIN_UV                  1800000
#define FTS_I2C_VTG_MAX_UV                  1800000
#endif
#define FTS_READ_TOUCH_BUFFER_DIVIDED       0
/*****************************************************************************
* Global variable or extern global variabls/functions
******************************************************************************/
struct i2c_client *fts_i2c_client;
struct fts_ts_data *fts_wq_data;
struct input_dev *fts_input_dev;
extern char Lcm_name[HARDWARE_MAX_ITEM_LONGTH];

#if FTS_DEBUG_EN
int g_show_log = 1;
#else
int g_show_log = 0;
#endif

#if (FTS_DEBUG_EN && (FTS_DEBUG_LEVEL == 2))
char g_sz_debug[1024] = {0};
#endif

/*****************************************************************************
* Static function prototypes
*****************************************************************************/
static void fts_release_all_finger(void);
int fts_ts_suspend(struct device *dev);
static int fts_ts_resume(struct device *dev);

#define PINCTRL_STATE_ACTIVE	"pmx_ts_active"
#define PINCTRL_STATE_SUSPEND	"pmx_ts_suspend"
#define PINCTRL_STATE_RELEASE	"pmx_ts_release"


extern int panel_dead2tp;

/*****************************************************************************
*  Name: fts_wait_tp_to_valid
*  Brief:   Read chip id until TP FW become valid, need call when reset/power on/resume...
*           1. Read Chip ID per INTERVAL_READ_REG(20ms)
*           2. Timeout: TIMEOUT_READ_REG(300ms)
*  Input:
*  Output:
*  Return: 0 - Get correct Device ID
*****************************************************************************/
int fts_wait_tp_to_valid(struct i2c_client *client)
{
	int ret = 0;
	int cnt = 0;
	u8  reg_value = 0;

	do {
		ret = fts_i2c_read_reg(client, FTS_REG_CHIP_ID, &reg_value);
		if ((ret < 0) || (reg_value != chip_types.chip_idh)) {
			FTS_INFO("TP Not Ready, ReadData = 0x%x", reg_value);
		} else if (reg_value == chip_types.chip_idh) {
			FTS_INFO("TP Ready, Device ID = 0x%x", reg_value);
			return 0;
		}
		cnt++;
		msleep(INTERVAL_READ_REG);
	} while ((cnt * INTERVAL_READ_REG) < TIMEOUT_READ_REG);

	/* error: not get correct reg data */
	return -EPERM;
}

/*****************************************************************************
*  Name: fts_recover_state
*  Brief: Need execute this function when reset
*  Input:
*  Output:
*  Return:
*****************************************************************************/
void fts_tp_state_recovery(struct i2c_client *client)
{
	/* wait tp stable */
	fts_wait_tp_to_valid(client);
	/* recover TP charger state 0x8B */
	/* recover TP glove state 0xC0 */
	/* recover TP cover state 0xC1 */
	fts_ex_mode_recovery(client);
	/* recover TP gesture state 0xD0 */
#if FTS_GESTURE_EN
	if (gesture_data.gesture_all_switch)
	fts_gesture_recovery(client);
#endif
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
	gpio_direction_output(fts_wq_data->pdata->reset_gpio, 0);
	msleep(20);
	gpio_direction_output(fts_wq_data->pdata->reset_gpio, 1);
	msleep(hdelayms);

	return 0;
}

/*****************************************************************************
*  Name: fts_irq_disable
*  Brief: disable irq
*  Input:
*   sync:
*  Output:
*  Return:
*****************************************************************************/
void fts_irq_disable(void)
{
	unsigned long irqflags;
	spin_lock_irqsave(&fts_wq_data->irq_lock, irqflags);

	if (!fts_wq_data->irq_disable) {
		disable_irq_nosync(fts_wq_data->client->irq);
		fts_wq_data->irq_disable = 1;
	}

	spin_unlock_irqrestore(&fts_wq_data->irq_lock, irqflags);
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
	spin_lock_irqsave(&fts_wq_data->irq_lock, irqflags);

	if (fts_wq_data->irq_disable) {
		enable_irq(fts_wq_data->client->irq);
		fts_wq_data->irq_disable = 0;
	}

	spin_unlock_irqrestore(&fts_wq_data->irq_lock, irqflags);
}

#if FTS_GESTURE_EN
static int fts_input_event(struct input_dev *dev,
		unsigned int type, unsigned int code, int value)
{
	char buffer[16];

	if (type == EV_SYN && code == SYN_CONFIG) {
		sprintf(buffer, "%d", value);

		FTS_INFO("FTS:Gesture on/off : %d", value);
		if (value >= MXT_INPUT_EVENT_START && value <= MXT_INPUT_EVENT_END) {
			if (value == MXT_INPUT_EVENT_WAKUP_MODE_ON) {
				gesture_data.gesture_all_switch = 1;
			} else if (value == MXT_INPUT_EVENT_WAKUP_MODE_OFF) {
				gesture_data.gesture_all_switch = 0;
			} else {
				gesture_data.gesture_all_switch = 0;
				FTS_ERROR("Failed Open/Close Gesture Function!\n");
				return -ENOMEM;
			}
		}
	}

	return 0;
}
#endif

/*****************************************************************************
*  Name: fts_input_dev_init
*  Brief: input dev init
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_input_dev_init(struct i2c_client *client, struct fts_ts_data *data,  struct input_dev *input_dev, struct fts_ts_platform_data *pdata)
{
	int  err, len;

	FTS_FUNC_ENTER();

	/* Init and register Input device */
	input_dev->name = FTS_DRIVER_NAME;
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
#if FTS_GESTURE_EN
	input_dev->event = fts_input_event;
#endif

	input_set_drvdata(input_dev, data);
	i2c_set_clientdata(client, data);

	__set_bit(EV_KEY, input_dev->evbit);
	if (data->pdata->have_key) {
		FTS_DEBUG("set key capabilities");
		for (len = 0; len < data->pdata->key_number; len++) {
			input_set_capability(input_dev, EV_KEY, data->pdata->keys[len]);
		}
	}
	__set_bit(EV_ABS, input_dev->evbit);
	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

#if FTS_MT_PROTOCOL_B_EN
	input_mt_init_slots(input_dev, pdata->max_touch_number, INPUT_MT_DIRECT);
#else
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, 0x0f, 0, 0);
#endif
	input_set_abs_params(input_dev, ABS_MT_POSITION_X, pdata->x_min, pdata->x_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, pdata->y_min, pdata->y_max, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, 0xFF, 0, 0);
#if FTS_REPORT_PRESSURE_EN
	input_set_abs_params(input_dev, ABS_MT_PRESSURE, 0, 0xFF, 0, 0);
#endif

	err = input_register_device(input_dev);
	if (err) {
		FTS_ERROR("Input device registration failed");
		goto free_inputdev;
	}

	FTS_FUNC_EXIT();

	return 0;

free_inputdev:
	input_free_device(input_dev);
	FTS_FUNC_EXIT();
	return err;

}

/*****************************************************************************
* Power Control
*****************************************************************************/
#if FTS_POWER_SOURCE_CUST_EN
static int fts_power_source_init(struct fts_ts_data *data)
{
	int rc;

	FTS_FUNC_ENTER();

	data->vcc_i2c = regulator_get(&data->client->dev, "vcc_i2c");
	if (IS_ERR(data->vcc_i2c)) {
		rc = PTR_ERR(data->vcc_i2c);
		FTS_ERROR("Regulator get failed vcc_i2c rc=%d", rc);
		goto reg_vdd_set_vtg;
	}

	data->lab = regulator_get(&data->client->dev, "lab");
	if (IS_ERR(data->lab)) {
		rc = PTR_ERR(data->lab);
		FTS_ERROR("Regulator get failed lab rc=%d", rc);
	}

	data->ibb = regulator_get(&data->client->dev, "ibb");
	if (IS_ERR(data->ibb)) {
		rc = PTR_ERR(data->ibb);
		FTS_ERROR("Regulator get failed ibb rc=%d", rc);
	}
	data->panel_iovdd = regulator_get(&data->client->dev, "panel_iovdd");
	if (IS_ERR(data->panel_iovdd)) {

		FTS_ERROR("!!! panel_iovdd not present !!!");
	}

	FTS_FUNC_EXIT();
	return 0;



reg_vdd_set_vtg:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, FTS_VTG_MAX_UV);


	FTS_FUNC_EXIT();
	return rc;
}

static int fts_power_source_ctrl(struct fts_ts_data *data, int enable)
{
	int rc;

	FTS_FUNC_ENTER();
	if (enable) {
		rc = regulator_enable(data->vcc_i2c);
		if (rc) {
			FTS_ERROR("Regulator vcc_i2c enable failed rc=%d", rc);
		}
	} else {
		rc = regulator_disable(data->vcc_i2c);
		if (rc) {
			FTS_ERROR("Regulator vcc_i2c disable failed rc=%d", rc);
		}
	}
	FTS_FUNC_EXIT();
	return 0;
}

static int lcd_power_ctrl(struct fts_ts_data *data, int enable)
{
	int rc;

	FTS_FUNC_ENTER();
	if (enable) {
		rc = regulator_enable(data->panel_iovdd);
		if (rc) {
			FTS_ERROR("Regulator panel_iovdd enable failed rc=%d\n", rc);
		}

		rc = regulator_enable(data->lab);
		if (rc) {
			FTS_ERROR("Regulator labenable failed rc=%d\n", rc);
		}

		rc = regulator_enable(data->ibb);
		if (rc) {
			FTS_ERROR("Regulator ibb enable failed rc=%d\n", rc);
		}
	} else {
		rc = regulator_disable(data->lab);
		if (rc) {
			FTS_ERROR("Regulator lab disable failed rc=%d\n", rc);
		}

		rc = regulator_disable(data->ibb);
		if (rc) {
			FTS_ERROR("Regulator ibb disable failed rc=%d\n", rc);
		}

		mdelay(10);
		rc = regulator_disable(data->panel_iovdd);
		if (rc) {
			FTS_ERROR("Regulator panel_iovdd disable failed rc=%d\n", rc);
		}

	}
	FTS_FUNC_EXIT();
	return 0;
}


#endif


/*****************************************************************************
*  Reprot related
*****************************************************************************/
/*****************************************************************************
*  Name: fts_release_all_finger
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void fts_release_all_finger(void)
{
#if FTS_MT_PROTOCOL_B_EN
	unsigned int finger_count = 0;
#endif

	mutex_lock(&fts_wq_data->report_mutex);
#if FTS_MT_PROTOCOL_B_EN
	for (finger_count = 0; finger_count < fts_wq_data->pdata->max_touch_number; finger_count++) {
		input_mt_slot(fts_input_dev, finger_count);
		input_mt_report_slot_state(fts_input_dev, MT_TOOL_FINGER, false);
	}
#else
	input_mt_sync(fts_input_dev);
#endif
	input_report_key(fts_input_dev, BTN_TOUCH, 0);
	input_sync(fts_input_dev);
	mutex_unlock(&fts_wq_data->report_mutex);
}


#if (FTS_DEBUG_EN && (FTS_DEBUG_LEVEL == 2))
static void fts_show_touch_buffer(u8 *buf, int point_num)
{
	int len = point_num * FTS_ONE_TCH_LEN;
	int count = 0;
	int i;

	memset(g_sz_debug, 0, 1024);
	if (len > (POINT_READ_BUF-3)) {
		len = POINT_READ_BUF-3;
	} else if (len == 0) {
		len += FTS_ONE_TCH_LEN;
	}
	count += sprintf(g_sz_debug, "%02X,%02X,%02X", buf[0], buf[1], buf[2]);
	for (i = 0; i < len; i++) {
		count += sprintf(g_sz_debug+count, ",%02X", buf[i+3]);
	}

}
#endif

static int fts_input_dev_report_key_event(struct ts_event *event, struct fts_ts_data *data)
{
	int i;

	if (data->pdata->have_key) {
		if ((1 == event->touch_point || 1 == event->point_num) &&
			(event->au16_y[0] == data->pdata->key_y_coord)) {

			if (event->point_num == 0) {
				FTS_DEBUG("Keys All Up!");
				for (i = 0; i < data->pdata->key_number; i++) {
					input_report_key(data->input_dev, data->pdata->keys[i], 0);
				}
			} else {
				for (i = 0; i < data->pdata->key_number; i++) {
					if (event->au16_x[0] > (data->pdata->key_x_coords[i] - FTS_KEY_WIDTH) &&
						event->au16_x[0] < (data->pdata->key_x_coords[i] + FTS_KEY_WIDTH)) {

						if (event->au8_touch_event[i] == 0 ||
							event->au8_touch_event[i] == 2) {
							input_report_key(data->input_dev, data->pdata->keys[i], 1);
							FTS_DEBUG("Key%d(%d, %d) DOWN!", i, event->au16_x[0], event->au16_y[0]);
						} else {
							input_report_key(data->input_dev, data->pdata->keys[i], 0);
							FTS_DEBUG("Key%d(%d, %d) Up!", i, event->au16_x[0], event->au16_y[0]);
						}
						break;
					}
				}
			}
			input_sync(data->input_dev);
			return 0;
		}
	}

	return -EPERM;
}

#if FTS_MT_PROTOCOL_B_EN
static int fts_input_dev_report_b(struct ts_event *event, struct fts_ts_data *data)
{
	int i = 0;
	int uppoint = 0;
	int touchs = 0;
	for (i = 0; i < event->touch_point; i++) {
		if (event->au8_finger_id[i] >= data->pdata->max_touch_number) {
			break;
		}
		input_mt_slot(data->input_dev, event->au8_finger_id[i]);

		if (event->au8_touch_event[i] == FTS_TOUCH_DOWN || event->au8_touch_event[i] == FTS_TOUCH_CONTACT) {
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, true);

#if FTS_REPORT_PRESSURE_EN
#if FTS_FORCE_TOUCH_EN
			if (event->pressure[i] <= 0) {
				FTS_ERROR("[B]Illegal pressure: %d", event->pressure[i]);
				event->pressure[i] = 1;
			}
#else
			event->pressure[i] = 0x3f;
#endif
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, event->pressure[i]);
#endif

			if (event->area[i] <= 0) {
				FTS_ERROR("[B]Illegal touch-major: %d", event->area[i]);
				event->area[i] = 1;
			}
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->area[i]);

			input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->au16_x[i]);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->au16_y[i]);
			touchs |= BIT(event->au8_finger_id[i]);
			data->touchs |= BIT(event->au8_finger_id[i]);

#if FTS_REPORT_PRESSURE_EN


#else

#endif
		} else {
			uppoint++;
			input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
#if FTS_REPORT_PRESSURE_EN
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, 0);
#endif
			data->touchs &= ~BIT(event->au8_finger_id[i]);

		}
	}

	if (unlikely(data->touchs ^ touchs)) {
		for (i = 0; i < data->pdata->max_touch_number; i++) {
			if (BIT(i) & (data->touchs ^ touchs)) {

				input_mt_slot(data->input_dev, i);
				input_mt_report_slot_state(data->input_dev, MT_TOOL_FINGER, false);
#if FTS_REPORT_PRESSURE_EN
				input_report_abs(data->input_dev, ABS_MT_PRESSURE, 0);
#endif
			}
		}
	}

	data->touchs = touchs;
	if (event->touch_point == uppoint) {

		input_report_key(data->input_dev, BTN_TOUCH, 0);
	} else {
		input_report_key(data->input_dev, BTN_TOUCH, event->touch_point > 0);
	}

	input_sync(data->input_dev);

	return 0;

}

#else
static int fts_input_dev_report_a(struct ts_event *event, struct fts_ts_data *data)
{
	int i = 0;
	int uppoint = 0;
	int touchs = 0;
	for (i = 0; i < event->touch_point; i++) {

		if (event->au8_touch_event[i] == FTS_TOUCH_DOWN || event->au8_touch_event[i] == FTS_TOUCH_CONTACT) {
			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, event->au8_finger_id[i]);
#if FTS_REPORT_PRESSURE_EN
#if FTS_FORCE_TOUCH_EN
			if (event->pressure[i] <= 0) {
				FTS_ERROR("[B]Illegal pressure: %d", event->pressure[i]);
				event->pressure[i] = 1;
			}
#else
			event->pressure[i] = 0x3f;
#endif
			input_report_abs(data->input_dev, ABS_MT_PRESSURE, event->pressure[i]);
#endif

			if (event->area[i] <= 0) {
				FTS_ERROR("[B]Illegal touch-major: %d", event->area[i]);
				event->area[i] = 1;
			}
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->area[i]);

			input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->au16_x[i]);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->au16_y[i]);

			input_mt_sync(data->input_dev);

#if FTS_REPORT_PRESSURE_EN


#else
			FTS_DEBUG("[B]P%d(%d, %d)[tm:%d] DOWN!", event->au8_finger_id[i], event->au16_x[i], event->au16_y[i], event->area[i]);
#endif
		} else {
			uppoint++;
		}
	}

	data->touchs = touchs;
	if (event->touch_point == uppoint) {

		input_report_key(data->input_dev, BTN_TOUCH, 0);
		input_mt_sync(data->input_dev);
	} else {
		input_report_key(data->input_dev, BTN_TOUCH, event->touch_point > 0);
	}

	input_sync(data->input_dev);

	return 0;
}
#endif

/*****************************************************************************
*  Name: fts_read_touchdata
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_read_touchdata(struct fts_ts_data *data)
{
	u8 buf[POINT_READ_BUF] = { 0 };
	u8 pointid = FTS_MAX_ID;
	int ret = -1;
	int i;
	struct ts_event *event = &(data->event);

#if FTS_GESTURE_EN
	if (gesture_data.gesture_all_switch) {
		u8 state;
		if (data->suspended) {
			fts_i2c_read_reg(data->client, FTS_REG_GESTURE_EN, &state);
			if (state == 1) {
				fts_gesture_readdata(data->client);
				return 1;
			}
		}
	}
#endif

#if FTS_PSENSOR_EN
	if ((fts_sensor_read_data(data) != 0) && (data->suspended == 1)) {
		return 1;
	}
#endif


#if FTS_READ_TOUCH_BUFFER_DIVIDED
	memset(buf, 0xFF, POINT_READ_BUF);
	memset(event, 0, sizeof(struct ts_event));

	buf[0] = 0x00;
	ret = fts_i2c_read(data->client, buf, 1, buf, (3 + FTS_ONE_TCH_LEN));
	if (ret < 0) {
		FTS_ERROR("%s read touchdata failed.", __func__);
		return ret;
	}
	event->touch_point = 0;
	event->point_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;
	if (event->point_num > data->pdata->max_touch_number)
		event->point_num = data->pdata->max_touch_number;

	if (event->point_num > 1) {
		buf[9] = 0x09;
		fts_i2c_read(data->client, buf+9, 1, buf+9, (event->point_num - 1) * FTS_ONE_TCH_LEN);
	}
#else
	ret = fts_i2c_read(data->client, buf, 1, buf, POINT_READ_BUF);
	if (ret < 0) {
		FTS_ERROR("[B]Read touchdata failed, ret: %d", ret);
		return ret;
	}

#if FTS_POINT_REPORT_CHECK_EN
	fts_point_report_check_queue_work();
#endif

	memset(event, 0, sizeof(struct ts_event));
	event->point_num = buf[FTS_TOUCH_POINT_NUM] & 0x0F;
	if (event->point_num > data->pdata->max_touch_number)
		event->point_num = data->pdata->max_touch_number;
	event->touch_point = 0;
#endif

#if (FTS_DEBUG_EN && (FTS_DEBUG_LEVEL == 2))
	fts_show_touch_buffer(buf, event->point_num);
#endif

	for (i = 0; i < data->pdata->max_touch_number; i++) {
		pointid = (buf[FTS_TOUCH_ID_POS + FTS_ONE_TCH_LEN * i]) >> 4;
		if (pointid >= FTS_MAX_ID)
			break;
		else
			event->touch_point++;

		event->au16_x[i] =
			(s16) (buf[FTS_TOUCH_X_H_POS + FTS_ONE_TCH_LEN * i] & 0x0F) <<
			8 | (s16) buf[FTS_TOUCH_X_L_POS + FTS_ONE_TCH_LEN * i];
		event->au16_y[i] =
			(s16) (buf[FTS_TOUCH_Y_H_POS + FTS_ONE_TCH_LEN * i] & 0x0F) <<
			8 | (s16) buf[FTS_TOUCH_Y_L_POS + FTS_ONE_TCH_LEN * i];
		event->au8_touch_event[i] =
			buf[FTS_TOUCH_EVENT_POS + FTS_ONE_TCH_LEN * i] >> 6;
		event->au8_finger_id[i] =
			(buf[FTS_TOUCH_ID_POS + FTS_ONE_TCH_LEN * i]) >> 4;
		event->area[i] =
			(buf[FTS_TOUCH_AREA_POS + FTS_ONE_TCH_LEN * i]) >> 4;
		event->pressure[i] =
			(s16) buf[FTS_TOUCH_PRE_POS + FTS_ONE_TCH_LEN * i];

		if (0 == event->area[i])
			event->area[i] = 0x09;

		if (0 == event->pressure[i])
			event->pressure[i] = 0x3f;

		if ((event->au8_touch_event[i] == 0 || event->au8_touch_event[i] == 2) && (event->point_num == 0)) {
			FTS_DEBUG("abnormal touch data from fw");
			return -EPERM;
		}
	}
	if (event->touch_point == 0) {
		return -EPERM;
	}
	return 0;
}

/*****************************************************************************
*  Name: fts_report_value
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void fts_report_value(struct fts_ts_data *data)
{
	struct ts_event *event = &data->event;





	if (0 == fts_input_dev_report_key_event(event, data)) {
		return;
	}

#if FTS_MT_PROTOCOL_B_EN
	fts_input_dev_report_b(event, data);
#else
	fts_input_dev_report_a(event, data);
#endif


	return;

}

/*****************************************************************************
*  Name: fts_ts_interrupt
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static irqreturn_t fts_ts_interrupt(int irq, void *dev_id)
{
	struct fts_ts_data *fts_ts = dev_id;
	int ret = -1;

	if (!fts_ts) {
		FTS_ERROR("[INTR]: Invalid fts_ts");
		return IRQ_HANDLED;
	}

#if FTS_ESDCHECK_EN
	fts_esdcheck_set_intr(1);
#endif

	ret = fts_read_touchdata(fts_wq_data);

	if (ret == 0) {
		mutex_lock(&fts_wq_data->report_mutex);
		fts_report_value(fts_wq_data);
		mutex_unlock(&fts_wq_data->report_mutex);
	}

#if FTS_ESDCHECK_EN
	fts_esdcheck_set_intr(0);
#endif

	return IRQ_HANDLED;
}

/*****************************************************************************
*  Name: fts_gpio_configure
*  Brief: Configure IRQ&RESET GPIO
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_gpio_configure(struct fts_ts_data *data)
{
	int err = 0;

	FTS_FUNC_ENTER();
	/* request irq gpio */
	if (gpio_is_valid(data->pdata->irq_gpio)) {
		err = gpio_request(data->pdata->irq_gpio, "fts_irq_gpio");
		if (err) {
			FTS_ERROR("[GPIO]irq gpio request failed");
			goto err_irq_gpio_req;
		}

		err = gpio_direction_input(data->pdata->irq_gpio);
		if (err) {
			FTS_ERROR("[GPIO]set_direction for irq gpio failed");
			goto err_irq_gpio_dir;
		}
	}
	/* request reset gpio */
	if (gpio_is_valid(data->pdata->reset_gpio)) {
		err = gpio_request(data->pdata->reset_gpio, "fts_reset_gpio");
		if (err) {
			FTS_ERROR("[GPIO]reset gpio request failed");
			goto err_irq_gpio_dir;
		}

		err = gpio_direction_output(data->pdata->reset_gpio, 1);
		if (err) {
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
	return err;
}


/*****************************************************************************
*  Name: fts_get_dt_coords
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_get_dt_coords(struct device *dev, char *name,
							struct fts_ts_platform_data *pdata) {
	u32 coords[FTS_COORDS_ARR_SIZE];
	struct property *prop;
	struct device_node *np = dev->of_node;
	int coords_size, rc;

	prop = of_find_property(np, name, NULL);
	if (!prop)
		return -EINVAL;
	if (!prop->value)
		return -ENODATA;


	coords_size = prop->length / sizeof(u32);
	if (coords_size != FTS_COORDS_ARR_SIZE) {
		FTS_ERROR("invalid %s", name);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(np, name, coords, coords_size);
	if (rc && (rc != -EINVAL)) {
		FTS_ERROR("Unable to read %s", name);
		return rc;
	}

	if (!strcmp(name, "focaltech,display-coords")) {
	if (!strcmp(Lcm_name, "ft8613_ebbg_5p5_1080p_video")) {
		pdata->x_min = 0;
		pdata->y_min = 0;
		pdata->x_max = 1080;
		pdata->y_max = 1920;
	} else {
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
	}
	FTS_INFO("Lcdname:%s \n", Lcm_name);
	FTS_INFO("x_min:%d  y_min:%d    x_max:%d    y_max:%d", pdata->x_min, pdata->y_min, pdata->x_max, pdata->y_max);

	} else {
		FTS_ERROR("unsupported property %s", name);
		return -EINVAL;
	}

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
	int rc;
	struct device_node *np = dev->of_node;
	u32 temp_val;

	FTS_FUNC_ENTER();

	rc = fts_get_dt_coords(dev, "focaltech,display-coords", pdata);
	if (rc)
		FTS_ERROR("Unable to get display-coords");

	/* key */
	pdata->have_key = of_property_read_bool(np, "focaltech,have-key");
	if (pdata->have_key) {
		rc = of_property_read_u32(np, "focaltech,key-number", &pdata->key_number);
		if (rc) {
			FTS_ERROR("Key number undefined!");
		}
		rc = of_property_read_u32_array(np, "focaltech,keys",
								pdata->keys, pdata->key_number);
		if (rc) {
			FTS_ERROR("Keys undefined!");
		}
		rc = of_property_read_u32(np, "focaltech,key-y-coord", &pdata->key_y_coord);
		if (rc) {
			FTS_ERROR("Key Y Coord undefined!");
		}
		rc = of_property_read_u32_array(np, "focaltech,key-x-coords",
								pdata->key_x_coords, pdata->key_number);
		if (rc) {
			FTS_ERROR("Key X Coords undefined!");
		}
		FTS_DEBUG("%d: (%d, %d, %d), [%d, %d, %d][%d]",
				pdata->key_number, pdata->keys[0], pdata->keys[1], pdata->keys[2],
				pdata->key_x_coords[0], pdata->key_x_coords[1], pdata->key_x_coords[2],
				pdata->key_y_coord);
	}

	/* reset, irq gpio info */
	pdata->reset_gpio = of_get_named_gpio_flags(np, "focaltech,reset-gpio", 0, &pdata->reset_gpio_flags);
	if (pdata->reset_gpio < 0) {
		FTS_ERROR("Unable to get reset_gpio");
	}

	pdata->irq_gpio = of_get_named_gpio_flags(np, "focaltech,irq-gpio", 0, &pdata->irq_gpio_flags);
	if (pdata->irq_gpio < 0) {
		FTS_ERROR("Unable to get irq_gpio");
	}

	rc = of_property_read_u32(np, "focaltech,max-touch-number", &temp_val);
	if (!rc) {
		pdata->max_touch_number = temp_val;
		FTS_DEBUG("max_touch_number=%d", pdata->max_touch_number);
	} else {
		FTS_ERROR("Unable to get max-touch-number");
		pdata->max_touch_number = FTS_MAX_POINTS;
	}



	FTS_FUNC_EXIT();
	return 0;
}

#if defined(CONFIG_FB)
/*****************************************************************************
*  Name: fb_notifier_callback
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fb_notifier_callback(struct notifier_block *self,
								unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int *blank;
	struct fts_ts_data *fts_data =
		container_of(self, struct fts_ts_data, fb_notif);

	if (evdata && evdata->data && event == FB_EVENT_BLANK &&
		fts_data && fts_data->client) {
		blank = evdata->data;
		if (*blank == FB_BLANK_UNBLANK)
			fts_ts_resume(&fts_data->client->dev);
		else if (*blank == FB_BLANK_POWERDOWN)
			fts_ts_suspend(&fts_data->client->dev);
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
#if 1
static int fts_ts_pinctrl_init(struct fts_ts_data *data)
{
	int retval;

	/* Get pinctrl if target uses pinctrl */
	data->ts_pinctrl = devm_pinctrl_get(&(data->client->dev));
	if (IS_ERR_OR_NULL(data->ts_pinctrl)) {
		retval = PTR_ERR(data->ts_pinctrl);
		dev_dbg(&data->client->dev,
			"Target does not use pinctrl %d\n", retval);
		goto err_pinctrl_get;
	}

	data->pinctrl_state_active
		= pinctrl_lookup_state(data->ts_pinctrl,
				PINCTRL_STATE_ACTIVE);
	if (IS_ERR_OR_NULL(data->pinctrl_state_active)) {
		retval = PTR_ERR(data->pinctrl_state_active);
		dev_err(&data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_ACTIVE, retval);
		goto err_pinctrl_lookup;
	}

	data->pinctrl_state_suspend
		= pinctrl_lookup_state(data->ts_pinctrl,
			PINCTRL_STATE_SUSPEND);
	if (IS_ERR_OR_NULL(data->pinctrl_state_suspend)) {
		retval = PTR_ERR(data->pinctrl_state_suspend);
		dev_err(&data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_SUSPEND, retval);
		goto err_pinctrl_lookup;
	}

	data->pinctrl_state_release
		= pinctrl_lookup_state(data->ts_pinctrl,
			PINCTRL_STATE_RELEASE);
	if (IS_ERR_OR_NULL(data->pinctrl_state_release)) {
		retval = PTR_ERR(data->pinctrl_state_release);
		dev_dbg(&data->client->dev,
			"Can not lookup %s pinstate %d\n",
			PINCTRL_STATE_RELEASE, retval);
	}

	return 0;

err_pinctrl_lookup:
	devm_pinctrl_put(data->ts_pinctrl);
err_pinctrl_get:
	data->ts_pinctrl = NULL;
	return retval;
}
#endif

static void hardwareinfo_set(void *drv_data)
{
	char firmware_ver[HARDWARE_MAX_ITEM_LONGTH];
	char vendor_for_id[HARDWARE_MAX_ITEM_LONGTH];
	char ic_name[HARDWARE_MAX_ITEM_LONGTH];
	int err;

	u8 vendor_id;
	u8 ic_type;
	u8 fw_ver;

	fts_i2c_read_reg(fts_i2c_client, FTS_REG_VENDOR_ID, &vendor_id);
		fts_i2c_read_reg(fts_i2c_client, FTS_REG_FW_VER, &fw_ver);
	fts_i2c_read_reg(fts_i2c_client, FTS_REG_CHIP_ID, &ic_type);

	if (vendor_id == EBBG_VENDOR) {
		snprintf(vendor_for_id, HARDWARE_MAX_ITEM_LONGTH, "EBBG");
	} else if (vendor_id == CSOT_VENDOR) {
		snprintf(vendor_for_id, HARDWARE_MAX_ITEM_LONGTH, "CSOT");
	} else{
		snprintf(vendor_for_id, HARDWARE_MAX_ITEM_LONGTH, "Other vendor");
	}

	if (ic_type == TP_IC_FT8613) {
		snprintf(ic_name, HARDWARE_MAX_ITEM_LONGTH, "FT8613");
	} else{
		snprintf(ic_name, HARDWARE_MAX_ITEM_LONGTH, "Other IC");
	}

	snprintf(firmware_ver, HARDWARE_MAX_ITEM_LONGTH, "%s, %s, FW:0x%x", vendor_for_id, ic_name, fw_ver);
	FTS_INFO("firmware_ver=%s\n", firmware_ver);

	err = hardwareinfo_set_prop(HARDWARE_TP, firmware_ver);
		if (err < 0)
		return ;

	return ;

}


#ifndef WT_COMPILE_FACTORY_VERSION
static int get_boot_mode(struct i2c_client *client)
{
	int ret;
	char *cmdline_tp = NULL;
	char *temp;
	char cmd_line[15] = {'\0'};

	cmdline_tp = strstr(saved_command_line, "androidboot.mode=");
	if (cmdline_tp != NULL) {
		temp = cmdline_tp + strlen("androidboot.mode=");
		ret = strncmp(temp, "ffbm", strlen("ffbm"));
		memcpy(cmd_line, temp, strlen("ffbm"));
		FTS_INFO("cmd_line =%s \n", cmd_line);
		if (ret == 0) {
			FTS_INFO("mode: ffbm\n");
			return 1;/* factory mode*/
		} else {
			FTS_INFO("mode: no ffbm\n");
			return 2;/* not factory mode*/
		}
	}
	FTS_INFO("Normal mode \n");

	return 0;
}
#endif

/*****************************************************************************
*  Name: fts_ts_probe
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static void fts_ts_shutdown(struct i2c_client *client)
{
	lcd_power_ctrl(fts_wq_data, 0);
}

static int fts_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct fts_ts_platform_data *pdata;
	struct fts_ts_data *data;
	struct input_dev *input_dev;
	int err;
	int i;
	u8  reg_addr, reg_value;

	FTS_FUNC_ENTER();
	/* 1. Get Platform data */
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
							sizeof(struct fts_ts_platform_data),
							GFP_KERNEL);
		if (!pdata) {
			FTS_ERROR("[MEMORY]Failed to allocate memory");
			FTS_FUNC_EXIT();
			return -ENOMEM;
		}
		err = fts_parse_dt(&client->dev, pdata);
		if (err) {
			FTS_ERROR("[DTS]DT parsing failed");
		}
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		FTS_ERROR("Invalid pdata");
		FTS_FUNC_EXIT();
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		FTS_ERROR("I2C not supported");
		FTS_FUNC_EXIT();
		goto free_platform_data;
	}

	data = devm_kzalloc(&client->dev, sizeof(struct fts_ts_data), GFP_KERNEL);
	if (!data) {
		FTS_ERROR("[MEMORY]Failed to allocate memory");
		FTS_FUNC_EXIT();
		return -ENOMEM;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		FTS_ERROR("[INPUT]Failed to allocate input device");
		FTS_FUNC_EXIT();
		goto free_ts_data;
	}



	data->input_dev = input_dev;
	data->client = client;
	data->pdata = pdata;

	fts_wq_data = data;
	fts_i2c_client = client;
	fts_input_dev = input_dev;

	spin_lock_init(&fts_wq_data->irq_lock);
	mutex_init(&fts_wq_data->report_mutex);

	fts_input_dev_init(client, data, input_dev, pdata);

	fts_ctpm_get_upgrade_array();

	err = fts_gpio_configure(data);
	if (err < 0) {
		FTS_ERROR("[GPIO]Failed to configure the gpios");
		goto input_destroy;
	}

	msleep(1);


#if 1
	err = fts_ts_pinctrl_init(data);
	if (!err && data->ts_pinctrl) {
		/*
		* Pinctrl handle is optional. If pinctrl handle is found
		* let pins to be configured in active state. If not
		* found continue further without error.
		*/
		err = pinctrl_select_state(data->ts_pinctrl,
					data->pinctrl_state_active);
		if (err < 0) {
			dev_err(&client->dev,
				"failed to select pin to active state");
		}
	}
#endif


#if FTS_POWER_SOURCE_CUST_EN
		fts_power_source_init(data);
		fts_power_source_ctrl(data, 1);
		lcd_power_ctrl(data, 1);
#endif
	fts_reset_proc(200);


	/* check the controller id */
	reg_addr = 0xA3;
	for (i = 0; i < 5; i++) {
		err = fts_i2c_read(client, &reg_addr, 1, &reg_value, 1);
		if (err < 0)
			msleep(5);
		else
			break;

	}
	if (i >= 5) {
		dev_err(&client->dev, "version read failed");
		goto free_gpio;
	}

	fts_wait_tp_to_valid(client);

	err = request_threaded_irq(client->irq, NULL, fts_ts_interrupt,
							 /*pdata->irq_gpio_flags | */IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
							 client->dev.driver->name, data);
	if (err) {
		FTS_ERROR("Request irq failed!");
		goto free_gpio;
	}

	fts_irq_disable();

#if FTS_PSENSOR_EN
	if (fts_sensor_init(data) != 0) {
		FTS_ERROR("fts_sensor_init failed!");
		FTS_FUNC_EXIT();
		goto irq_free;
	}
#endif

#if FTS_APK_NODE_EN
	fts_create_apk_debug_channel(client);
#endif

#if FTS_SYSFS_NODE_EN
	fts_create_sysfs(client);
#endif

#if FTS_POINT_REPORT_CHECK_EN
	fts_point_report_check_init();
#endif

	fts_ex_mode_init(client);

#if FTS_GESTURE_EN
	fts_gesture_init(input_dev, client);
#endif

#if FTS_ESDCHECK_EN
	fts_esdcheck_init();
#endif

	fts_irq_enable();
	fts_i2c_read_reg(client, FTS_REG_VENDOR_ID, &data->fw_vendor_id);
	fts_i2c_read_reg(client, FTS_REG_FW_VER, data->fw_ver);
	FTS_INFO("vendor_id=0x%x\n", data->fw_vendor_id);
	FTS_INFO("tp_fw=0x%x\n", data->fw_ver[0]);
#if FTS_TEST_EN
	fts_test_init(client);
#endif

#if FTS_LOCK_DOWN_INFO
	fts_lockdown_init(client);
#endif

#if FTS_CAT_RAWDATA
	fts_rawdata_init(client);
#endif


#if FTS_AUTO_UPGRADE_EN
	err = get_boot_mode(client);
	if (err == 0) {
	fts_ctpm_upgrade_init();
	} else {
		FTS_INFO("Not in normal mode!\n");
	}

#endif


#if defined(CONFIG_FB)
	data->fb_notif.notifier_call = fb_notifier_callback;
	err = fb_register_client(&data->fb_notif);
	if (err)
		FTS_ERROR("[FB]Unable to register fb_notifier: %d", err);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + FTS_SUSPEND_LEVEL;
	data->early_suspend.suspend = fts_ts_early_suspend;
	data->early_suspend.resume = fts_ts_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	hardwareinfo_tp_register(hardwareinfo_set, data);

	FTS_FUNC_EXIT();
	return 0;

#if FTS_PSENSOR_EN
irq_free:
		free_irq(client->irq, data);
#endif
free_gpio:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);

	lcd_power_ctrl(data, 0);
input_destroy:
		input_unregister_device(input_dev);
		input_dev = NULL;
		input_free_device(input_dev);
free_ts_data:
		devm_kfree(&client->dev, data);
free_platform_data:
		devm_kfree(&client->dev, pdata);

	return err;

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
	struct fts_ts_data *data = i2c_get_clientdata(client);

	FTS_FUNC_ENTER();
	cancel_work_sync(&data->touch_event_work);

#if FTS_PSENSOR_EN
	fts_sensor_remove(data);
#endif

#if FTS_POINT_REPORT_CHECK_EN
	fts_point_report_check_exit();
#endif

#if FTS_APK_NODE_EN
	fts_release_apk_debug_channel();
#endif

#if FTS_SYSFS_NODE_EN
	fts_remove_sysfs(client);
#endif

	fts_ex_mode_exit(client);

#if FTS_AUTO_UPGRADE_EN
	cancel_work_sync(&fw_update_work);
#endif

#if defined(CONFIG_FB)
	if (fb_unregister_client(&data->fb_notif))
		FTS_ERROR("Error occurred while unregistering fb_notifier.");
#elif defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&data->early_suspend);
#endif
	free_irq(client->irq, data);

	if (gpio_is_valid(data->pdata->reset_gpio))
		gpio_free(data->pdata->reset_gpio);

	if (gpio_is_valid(data->pdata->irq_gpio))
		gpio_free(data->pdata->irq_gpio);

	input_unregister_device(data->input_dev);

#if FTS_TEST_EN
	fts_test_exit(client);
#endif

#if FTS_ESDCHECK_EN
	fts_esdcheck_exit();
#endif

	FTS_FUNC_EXIT();
	return 0;
}

/*****************************************************************************
*  Name: fts_ts_suspend
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
int fts_ts_suspend(struct device *dev)
{
	struct fts_ts_data *data = fts_wq_data;
	int retval = 0, i = 0;

	FTS_FUNC_ENTER();
	if (data->suspended) {
		FTS_INFO("Already in suspend state");
		FTS_FUNC_EXIT();
		return -EPERM;
	}
#if FTS_ESDCHECK_EN
	fts_esdcheck_suspend();
#endif

#if FTS_GESTURE_EN
	if (gesture_data.gesture_all_switch) {

		if (panel_dead2tp) {
			FTS_ERROR("%s: panel_dead2tp=%d", __func__, panel_dead2tp);
			lcd_power_ctrl(data, 0);
			data->suspended = true;
			return 0;
		}

		retval = fts_gesture_suspend(data->client);
		if (retval == 0) {
			/* Enter into gesture mode(suspend) */
			retval = enable_irq_wake(fts_wq_data->client->irq);
			if (retval)
				FTS_ERROR("%s: set_irq_wake failed", __func__);
			data->suspended = true;
			FTS_FUNC_EXIT();
			return 0;
		}
	}

#endif

#if FTS_PSENSOR_EN
	if (fts_sensor_suspend(data) != 0) {
		enable_irq_wake(data->client->irq);
		data->suspended = true;
		return 0;
	}
#endif

	fts_irq_disable();

	for (; i < 5; i++) {
	/* TP enter sleep mode */
	retval = fts_i2c_write_reg(data->client, FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP_VALUE);
	if (retval < 0) {
		FTS_ERROR("Set TP to sleep mode fail, ret=%d!", retval);
	} else {
		FTS_INFO("go into sleep mode successfully\n");
	break;
	}
	msleep(20);
	}


if (!(gesture_data.gesture_all_switch)) {
		lcd_power_ctrl(data, 0);
	}

	data->suspended = true;

	FTS_FUNC_EXIT();

	return 0;
}
EXPORT_SYMBOL(fts_ts_suspend);
/*****************************************************************************
*  Name: fts_ts_resume
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
static int fts_ts_resume(struct device *dev)
{
	struct fts_ts_data *data = dev_get_drvdata(dev);

	FTS_FUNC_ENTER();
	if (!data->suspended) {
		FTS_DEBUG("Already in awake state");
		FTS_FUNC_EXIT();
		return -EPERM;
	}
	fts_release_all_finger();



	if ((!(gesture_data.gesture_all_switch)) || panel_dead2tp) {
		FTS_ERROR("%s:panel_dead2tp=%d", __func__, panel_dead2tp);
		panel_dead2tp = 0;
		lcd_power_ctrl(data, 1);
	}

#if (!FTS_CHIP_IDC)
	fts_reset_proc(200);
#endif

	fts_tp_state_recovery(data->client);

#if FTS_ESDCHECK_EN
	fts_esdcheck_resume();
#endif

#if FTS_GESTURE_EN
	if (gesture_data.gesture_all_switch) {
	if (fts_gesture_resume(data->client) == 0) {
		int err;
		err = disable_irq_wake(data->client->irq);
		if (err)
			FTS_ERROR("%s: disable_irq_wake failed", __func__);
		data->suspended = false;
		FTS_FUNC_EXIT();
		return 0;
	}
	}

#endif

#if FTS_PSENSOR_EN
	if (fts_sensor_resume(data) != 0) {
		disable_irq_wake(data->client->irq);
		data->suspended = false;
		FTS_FUNC_EXIT();
		return 0;
	}
#endif

	data->suspended = false;

	fts_irq_enable();

	FTS_FUNC_EXIT();
	return 0;
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
	{ .compatible = "focaltech,fts", },
	{ },
};

static struct i2c_driver fts_ts_driver = {
	.probe = fts_ts_probe,
	.remove = fts_ts_remove,
	.shutdown = fts_ts_shutdown,
	.driver = {
		.name = FTS_DRIVER_NAME,
		.owner = THIS_MODULE,
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
