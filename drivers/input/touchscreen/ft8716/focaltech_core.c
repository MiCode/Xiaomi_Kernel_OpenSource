/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2010-2016, FocalTech Systems, Ltd., all rights reserved.
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



#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#elif defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#define FTS_SUSPEND_LEVEL 1     /* Early-suspend level */
#endif

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define FTS_DRIVER_NAME                     "fts_ts"
#define INTERVAL_READ_REG                   20
#define TIMEOUT_READ_REG                    300
#if FTS_POWER_SOURCE_CUST_EN
#define FTS_VTG_MIN_UV                      2600000
#define FTS_VTG_MAX_UV                      3300000
#define FTS_I2C_VTG_MIN_UV                  1800000
#define FTS_I2C_VTG_MAX_UV                  1800000
#endif
#define FTS_READ_TOUCH_BUFFER_DIVIDED       0



char TP_vendor;
u8 fw_ver;


extern int fts_i2c_read(struct i2c_client *client, char *writebuf, int writelen, char *readbuf, int readlen);

#if FTS_LOCK_DOWN_INFO
char ftp_lockdown_info[128];

#define FTS_PROC_LOCKDOWN_FILE "tp_lockdown_info"
static struct proc_dir_entry *fts_lockdown_status_proc;

static int fts_lockdown_proc_show(struct seq_file *file, void *data)
{
	char temp[40] = {0};

	sprintf(temp, "%s\n", ftp_lockdown_info);
	seq_printf(file, "%s\n", temp);

	return 0;
}

static int fts_lockdown_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, fts_lockdown_proc_show, inode->i_private);
}

static const struct file_operations fts_lockdown_proc_fops = {
	.open = fts_lockdown_proc_open,
	.read = seq_read,
};

#endif





/*****************************************************************************
* Global variable or extern global variabls/functions
******************************************************************************/
struct i2c_client *fts_i2c_client;
struct fts_ts_data *fts_wq_data;
struct input_dev *fts_input_dev;


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
static int fts_ts_suspend(struct device *dev);
static int fts_ts_resume(struct device *dev);


/*****************************************************************************
*  Name: fts_wait_tp_to_valid_sharp
*  Brief:   Read chip id until TP FW become valid, need call when reset/power on/resume...
*           1. Read Chip ID per INTERVAL_READ_REG(20ms)
*           2. Timeout: TIMEOUT_READ_REG(300ms)
*  Input:
*  Output:
*  Return: 0 - Get correct Device ID
*****************************************************************************/
int fts_wait_tp_to_valid_sharp(struct i2c_client *client)
{
	int ret = 0;
	int cnt = 0;
	u8  reg_value = 0;
	u8 id_cmd[4] = {0};
	u8 boot_id[2] = {0};

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

	if ((cnt * INTERVAL_READ_REG) >= TIMEOUT_READ_REG) {
		FTS_INFO("fw is invalid, need read boot id");

		id_cmd[0] = 0x55;
		id_cmd[1] = 0xAA;
		ret = fts_i2c_write(client, id_cmd, 2);
		if (ret < 0) {
			FTS_ERROR("55 AA cmd write fail");
			return ret;
		}

		id_cmd[0] = 0x90;
		id_cmd[1] = id_cmd[2] = id_cmd[3] = 0x00;
		ret = fts_i2c_read(client, id_cmd, 4, boot_id, 2);
		if (ret < 0) {
			FTS_ERROR("read boot id cmd fail");
			return ret;
		}
		FTS_INFO("read boot id:%02x%02x", boot_id[0], boot_id[1]);
		if ((boot_id[0] == chip_types.rom_idh) && (boot_id[1] == chip_types.rom_idl)) {
			return 0;
		}
	}

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
	fts_wait_tp_to_valid_sharp(client);
	/* recover TP charger state 0x8B */
	/* recover TP glove state 0xC0 */
	/* recover TP cover state 0xC1 */
	fts_ex_mode_recovery(client);
	/* recover TP gesture state 0xD0 */
#if FTS_GESTURE_EN
	fts_gesture_recovery(client);
#endif

	fts_release_all_finger();
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

	data->vdd = regulator_get(&data->client->dev, "vdd");
	if (IS_ERR(data->vdd)) {
		rc = PTR_ERR(data->vdd);
		FTS_ERROR("Regulator get failed vdd rc=%d", rc);
	}

	if (regulator_count_voltages(data->vdd) > 0) {
		rc = regulator_set_voltage(data->vdd, FTS_VTG_MIN_UV, FTS_VTG_MAX_UV);
		if (rc) {
			FTS_ERROR("Regulator set_vtg failed vdd rc=%d", rc);
			goto reg_vdd_put;
		}
	}

	data->vcc_i2c = regulator_get(&data->client->dev, "vcc_i2c");
	if (IS_ERR(data->vcc_i2c)) {
		rc = PTR_ERR(data->vcc_i2c);
		FTS_ERROR("Regulator get failed vcc_i2c rc=%d", rc);
		goto reg_vdd_set_vtg;
	}

	if (regulator_count_voltages(data->vcc_i2c) > 0) {
		rc = regulator_set_voltage(data->vcc_i2c, FTS_I2C_VTG_MIN_UV, FTS_I2C_VTG_MAX_UV);
		if (rc) {
			FTS_ERROR("Regulator set_vtg failed vcc_i2c rc=%d", rc);
			goto reg_vcc_i2c_put;
		}
	}

	FTS_FUNC_EXIT();
	return 0;

reg_vcc_i2c_put:
	regulator_put(data->vcc_i2c);
reg_vdd_set_vtg:
	if (regulator_count_voltages(data->vdd) > 0)
		regulator_set_voltage(data->vdd, 0, FTS_VTG_MAX_UV);
reg_vdd_put:
	regulator_put(data->vdd);
	FTS_FUNC_EXIT();
	return rc;
}

static int fts_power_source_ctrl(struct fts_ts_data *data, int enable)
{
	int rc;

	FTS_FUNC_ENTER();
	if (enable) {
		rc = regulator_enable(data->vdd);
		if (rc) {
			FTS_ERROR("Regulator vdd enable failed rc=%d", rc);
		}

		rc = regulator_enable(data->vcc_i2c);
		if (rc) {
			FTS_ERROR("Regulator vcc_i2c enable failed rc=%d", rc);
		}
	} else {
		rc = regulator_disable(data->vdd);
		if (rc) {
			FTS_ERROR("Regulator vdd disable failed rc=%d", rc);
		}
		rc = regulator_disable(data->vcc_i2c);
		if (rc) {
			FTS_ERROR("Regulator vcc_i2c disable failed rc=%d", rc);
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

	for (finger_count = 0; finger_count < fts_wq_data->pdata->max_touch_number; finger_count++) {
		input_mt_slot(fts_input_dev, finger_count);
		input_mt_report_slot_state(fts_input_dev, MT_TOOL_FINGER, false);
	}
#else
	input_mt_sync(fts_input_dev);
#endif
	input_report_key(fts_input_dev, BTN_TOUCH, 0);
	input_sync(fts_input_dev);
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
	FTS_DEBUG("buffer: %s", g_sz_debug);
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
		input_mt_slot(data->input_dev, event->au8_finger_id[i]);

		if (event->au8_touch_event[i] == FTS_TOUCH_DOWN || event->au8_touch_event[i] == FTS_TOUCH_CONTACT) {
			input_report_key(data->input_dev, BTN_TOUCH, 1);

			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, event->au8_finger_id[i]);

#if FTS_REPORT_PRESSURE_EN
#if FTS_FORCE_TOUCH_EN
			if (event->pressure[i] <= 0) {
				FTS_ERROR("[B]Illegal pressure: %d", event->pressure[i]);
				event->pressure[i] = 1;
			}
#else

#endif

#endif

			if (event->area[i] <= 0) {
				FTS_ERROR("[B]Illegal touch-major: %d", event->area[i]);
				event->area[i] = 1;
			}
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, event->area[i]);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X, event->au16_x[i]);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y, event->au16_y[i]);
			printk("event->au16_x[i] : %d, event->au16_y[i]: %d\n", event->au16_x[i], event->au16_y[i]);
			touchs |= BIT(event->au8_finger_id[i]);
			data->touchs |= BIT(event->au8_finger_id[i]);
		} else {
			uppoint++;
			input_report_key(data->input_dev, BTN_TOUCH, 0);

			input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, -1);
#if FTS_REPORT_PRESSURE_EN

#endif
			data->touchs &= ~BIT(event->au8_finger_id[i]);
			FTS_DEBUG("[B]P%d UP!", event->au8_finger_id[i]);
		}
	}

	if (unlikely(data->touchs ^ touchs)) {
		for (i = 0; i < data->pdata->max_touch_number; i++) {
			if (BIT(i) & (data->touchs ^ touchs)) {
				FTS_DEBUG("[B]P%d UP!", i);
				input_mt_slot(data->input_dev, i);
				input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, -1);
#if FTS_REPORT_PRESSURE_EN

#endif
			}
		}
	}

	data->touchs = touchs;


	if (event->point_num == 0) {
		for (i = 0; i < data->pdata->max_touch_number; i++) {
			if (BIT(i) & (data->touchs ^ touchs)) {
				input_mt_slot(data->input_dev, i);
				input_report_abs(data->input_dev, ABS_MT_TRACKING_ID, -1);
			}
		}
	}

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
				event->au16_y[i], event->pressure[i], event->area[i]);
#endif
		} else {
			uppoint++;
		}
	}

	data->touchs = touchs;
	if (event->touch_point == uppoint) {
		FTS_DEBUG("Points All Up!");
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
	{
		u8 state;
		if (data->suspended) {
			fts_i2c_read_reg(data->client, FTS_REG_GESTURE_EN, &state);
			printk("state :%d\n", state);
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

		if ((event->au8_touch_event[i] == 0 || event->au8_touch_event[i] == 2) && (event->point_num == 0))
			break;
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
		fts_report_value(fts_wq_data);
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
		pdata->x_min = coords[0];
		pdata->y_min = coords[1];
		pdata->x_max = coords[2];
		pdata->y_max = coords[3];
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

/*****************************************************************************
*  Name: fts_ts_probe
*  Brief:
*  Input:
*  Output:
*  Return:
*****************************************************************************/
int ft8716_suspend = 0;
static int fts_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct fts_ts_platform_data *pdata;
	struct fts_ts_data *data;
	struct input_dev *input_dev;
	int err;
	unsigned char auc_i2c_write_buf[10];
	u8  r_buf[10] = {0};


	FTS_FUNC_ENTER();

	/* 1. Get Platform data */
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
							sizeof(struct fts_ts_platform_data),
							GFP_KERNEL);
		if (!pdata) {
			FTS_ERROR("[MEMORY]Failed to allocate memory");
			FTS_FUNC_EXIT();
			ft8716_suspend = 1;
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
	ft8716_suspend = 1;
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		FTS_ERROR("I2C not supported");
		FTS_FUNC_EXIT();
		ft8716_suspend = 1;
		return -ENODEV;
	}

	data = devm_kzalloc(&client->dev, sizeof(struct fts_ts_data), GFP_KERNEL);
	if (!data) {
		FTS_ERROR("[MEMORY]Failed to allocate memory");
		FTS_FUNC_EXIT();
		ft8716_suspend = 1;
		return -ENOMEM;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		FTS_ERROR("[INPUT]Failed to allocate input device");
		FTS_FUNC_EXIT();
		ft8716_suspend = 1;
		return -ENOMEM;
	}



	data->input_dev = input_dev;
	data->client = client;
	data->pdata = pdata;

	fts_wq_data = data;
	fts_i2c_client = client;
	fts_input_dev = input_dev;

	spin_lock_init(&fts_wq_data->irq_lock);

	fts_input_dev_init(client, data, input_dev, pdata);


	fts_ctpm_get_upgrade_array();

#if FTS_POWER_SOURCE_CUST_EN
	fts_power_source_init(data);
	fts_power_source_ctrl(data, 1);
#endif

	err = fts_gpio_configure(data);
	if (err < 0) {
		FTS_ERROR("[GPIO]Failed to configure the gpios");
		goto free_gpio;
	}

	fts_reset_proc(200);

#if FTS_POINT_REPORT_CHECK_EN
	fts_point_report_check_init();
#endif

	err = request_threaded_irq(client->irq, NULL, fts_ts_interrupt,
				pdata->irq_gpio_flags | IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
				client->dev.driver->name, data);

	if (err) {
		FTS_ERROR("Request irq failed!");
		goto free_gpio;
	}

	err = fts_wait_tp_to_valid_sharp(client);
	if (err) {
		FTS_ERROR("fts_wait_tp_to_valid_sharp failed!");

		goto free_irq;
	}


	fts_irq_disable();



#ifdef FTS_LOCK_DOWN_INFO

		err = fts_i2c_write_reg(client, 0x90, 0x20);
		if (err < 0)
			printk("[FTS] i2c write 0x90 err\n");

		msleep(5);
		auc_i2c_write_buf[0] = 0x99;
		err = fts_i2c_read(client, auc_i2c_write_buf, 1, r_buf, 8);
		if (err < 0)
			printk("[FTS] i2c read 0x99 err\n");

		sprintf(ftp_lockdown_info, "%02x%02x%02x%02x%02x%02x%02x%02x", \
				r_buf[0], r_buf[1], r_buf[2], r_buf[3], r_buf[4], r_buf[5], r_buf[6], r_buf[7]);

		printk("tpd_probe, ft8716_ctpm_LockDownInfo_get_from_boot, tp_lockdown_info=%s\n", ftp_lockdown_info);

		fts_lockdown_status_proc = proc_create(FTS_PROC_LOCKDOWN_FILE, 0644, NULL, &fts_lockdown_proc_fops);
		if (fts_lockdown_status_proc == NULL) {
			printk("fts, create_proc_entry ctp_lockdown_status_proc failed\n");
		}
#endif

#if FTS_PSENSOR_EN
	if (fts_sensor_init(data) != 0) {
		FTS_ERROR("fts_sensor_init failed!");
		FTS_FUNC_EXIT();
		return 0;
	}
#endif

#if FTS_APK_NODE_EN
	fts_create_apk_debug_channel(client);
#endif

#if FTS_SYSFS_NODE_EN
	fts_create_sysfs(client);
#endif

	fts_ex_mode_init(client);

#if FTS_GESTURE_EN
	fts_gesture_init(input_dev, client);
#endif

#if FTS_ESDCHECK_EN
	fts_esdcheck_init();
#endif

	fts_irq_enable();

#if FTS_AUTO_UPGRADE_EN
	fts_ctpm_upgrade_init();
#endif


#if FTS_TEST_EN
	fts_test_init(client);
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

	FTS_FUNC_EXIT();
	return 0;

free_irq:
	free_irq(client->irq, data);
	printk("%s:free irq\n", __func__);
free_gpio:
	if (gpio_is_valid(pdata->reset_gpio))
		gpio_free(pdata->reset_gpio);
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
	ft8716_suspend = 1;

	printk("%s:free gpio\n", __func__);
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
extern struct mdss_panel_data *panel_data;
extern int panel_suspend_reset_flag;

extern int mdss_dsi_panel_power_off(struct mdss_panel_data *pdata);
#if FTS_GESTURE_EN
	extern int  ft8716_gesture_func_on;
#endif


static int fts_ts_suspend(struct device *dev)
{
	struct fts_ts_data *data = dev_get_drvdata(dev);
	int retval = 0;

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
#endif

#if FTS_PSENSOR_EN
	if (fts_sensor_suspend(data) != 0) {
		enable_irq_wake(data->client->irq);
		data->suspended = true;
		return 0;
	}
#endif

	fts_irq_disable();

	 ft8716_suspend = 1;

	/* TP enter sleep mode */
	retval = fts_i2c_write_reg(data->client, FTS_REG_POWER_MODE, FTS_REG_POWER_MODE_SLEEP_VALUE);
	if (retval < 0) {
		FTS_ERROR("Set TP to sleep mode fail, ret=%d!", retval);
	}
	data->suspended = true;

	mdelay(10);

	if (!ft8716_gesture_func_on) {
		printk("set rst in TP_suspend\n");
		retval = mdss_dsi_panel_power_off(panel_data);
	if (retval < 0)
		printk("Enter %s mdss_dsi_panel_power_off fail\n", __func__);
	}
	ft8716_suspend = 0;
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
	struct fts_ts_data *data = dev_get_drvdata(dev);

	FTS_FUNC_ENTER();

	if (!data->suspended) {
		FTS_DEBUG("Already in awake state");
		FTS_FUNC_EXIT();
		return -EPERM;
	}

#if (!FTS_CHIP_IDC)
	fts_reset_proc(200);
#endif

	fts_tp_state_recovery(data->client);

#if FTS_GESTURE_EN
	if (fts_gesture_resume(data->client) == 0) {
		int err;
		err = disable_irq_wake(data->client->irq);
		if (err)
			FTS_ERROR("%s: disable_irq_wake failed", __func__);
		data->suspended = false;

	#if FTS_ESDCHECK_EN
		fts_esdcheck_resume();
	#endif

		FTS_FUNC_EXIT();
		return 0;
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
