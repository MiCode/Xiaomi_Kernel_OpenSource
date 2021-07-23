/* Himax Android Driver Sample Code for common functions
 *
 * Copyright (C) 2017 Himax Corporation.
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

#include "himax_ic.h"

#define SUPPORT_FINGER_DATA_CHECKSUM 0x0F
#define TS_WAKE_LOCK_TIMEOUT (2 * HZ)
#define FRAME_COUNT 5

#if defined(HX_AUTO_UPDATE_FW)
unsigned char i_CTPM_FW[] = {
#include "HX83100_Firmware_Version_FF23.i"
};
#endif

struct himax_ts_data *private_ts;
struct himax_ic_data *ic_data;
struct himax_report_data *hx_touch_data;

static int HX_TOUCH_INFO_POINT_CNT;

unsigned long FW_VER_MAJ_FLASH_ADDR;
unsigned long FW_VER_MIN_FLASH_ADDR;
unsigned long CFG_VER_MAJ_FLASH_ADDR;
unsigned long CFG_VER_MIN_FLASH_ADDR;
unsigned long CID_VER_MAJ_FLASH_ADDR;
unsigned long CID_VER_MIN_FLASH_ADDR;
/* unsigned long	PANEL_VERSION_ADDR; */

unsigned long FW_VER_MAJ_FLASH_LENG;
unsigned long FW_VER_MIN_FLASH_LENG;
unsigned long CFG_VER_MAJ_FLASH_LENG;
unsigned long CFG_VER_MIN_FLASH_LENG;
unsigned long CID_VER_MAJ_FLASH_LENG;
unsigned long CID_VER_MIN_FLASH_LENG;
/* unsigned long	PANEL_VERSION_LENG; */

unsigned long FW_CFG_VER_FLASH_ADDR;

#ifdef HX_AUTO_UPDATE_FW
int g_i_FW_VER;
int g_i_CFG_VER;
int g_i_CID_MAJ; /* GUEST ID */
int g_i_CID_MIN; /* VER for GUEST */
#endif

unsigned char IC_TYPE = 11;
unsigned char IC_CHECKSUM;

#ifdef HX_ESD_RECOVERY
u8 HX_ESD_RESET_ACTIVATE;
int hx_EB_event_flag;
int hx_EC_event_flag;
int hx_ED_event_flag;
#endif
u8 HX_HW_RESET_ACTIVATE;

#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
static uint8_t vk_press;
#endif
static uint8_t AA_press;
static uint8_t EN_NoiseFilter;
static uint8_t Last_EN_NoiseFilter;
static int hx_point_num; /* for himax_ts_work_func use */
static int p_point_num = 0xFFFF;
static int tpd_key;
static int tpd_key_old;
static int probe_fail_flag;
#ifdef HX_USB_DETECT_GLOBAL
bool USB_detect_flag;
#endif

#if defined(CONFIG_FB)
int fb_notifier_callback(struct notifier_block *self, unsigned long event,
			 void *data);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void himax_ts_early_suspend(struct early_suspend *h);
static void himax_ts_late_resume(struct early_suspend *h);
#endif

#ifdef HX_GESTURE_TRACK
static int gest_pt_cnt;
static int gest_pt_x[GEST_PT_MAX_NUM];
static int gest_pt_y[GEST_PT_MAX_NUM];
static int gest_start_x = 0, gest_start_y = 0, gest_end_x = 0, gest_end_y;
static int gest_width = 0, gest_height = 0, gest_mid_x = 0, gest_mid_y;
static int gn_gesture_coor[16];
#endif

#ifdef HX_CHIP_STATUS_MONITOR
struct chip_monitor_data *g_chip_monitor_data;
#endif

int himax_input_register(struct himax_ts_data *ts)
{
	int ret = 0;

	ret = himax_dev_set(ts);
	if (ret < 0)
		goto input_device_fail;

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);

#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
#if defined(HX_PLATFOME_DEFINE_KEY)
	himax_platform_key();
#else
	set_bit(KEY_BACK, ts->input_dev->keybit);
	set_bit(KEY_HOME, ts->input_dev->keybit);
	set_bit(KEY_MENU, ts->input_dev->keybit);
	set_bit(KEY_SEARCH, ts->input_dev->keybit);
	set_bit(KEY_APP_SWITCH, ts->input_dev->keybit);
#endif
#endif

#if defined(HX_SMART_WAKEUP) || defined(HX_PALM_REPORT) ||                     \
	defined(HX_INSPECT_LPWUG_TEST)
	set_bit(KEY_POWER, ts->input_dev->keybit);
#endif
#if defined(HX_SMART_WAKEUP)
	set_bit(KEY_CUST_01, ts->input_dev->keybit);
	set_bit(KEY_CUST_02, ts->input_dev->keybit);
	set_bit(KEY_CUST_03, ts->input_dev->keybit);
	set_bit(KEY_CUST_04, ts->input_dev->keybit);
	set_bit(KEY_CUST_05, ts->input_dev->keybit);
	set_bit(KEY_CUST_06, ts->input_dev->keybit);
	set_bit(KEY_CUST_07, ts->input_dev->keybit);
	set_bit(KEY_CUST_08, ts->input_dev->keybit);
	set_bit(KEY_CUST_09, ts->input_dev->keybit);
	set_bit(KEY_CUST_10, ts->input_dev->keybit);
	set_bit(KEY_CUST_11, ts->input_dev->keybit);
	set_bit(KEY_CUST_12, ts->input_dev->keybit);
	set_bit(KEY_CUST_13, ts->input_dev->keybit);
	set_bit(KEY_CUST_14, ts->input_dev->keybit);
	set_bit(KEY_CUST_15, ts->input_dev->keybit);
#endif
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

#ifdef HX_PROTOCOL_A
	/* ts->input_dev->mtsize = ts->nFinger_support; */
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, 3, 0, 0);
#else
	set_bit(MT_TOOL_FINGER, ts->input_dev->keybit);
#if defined(HX_PROTOCOL_B_3PA)
	input_mt_init_slots(ts->input_dev, ts->nFinger_support,
			    INPUT_MT_DIRECT);
#else
	input_mt_init_slots(ts->input_dev, ts->nFinger_support);
#endif
#endif

	I("input_set_abs_params: mix_x %d, max_x %d, min_y %d, max_y %d\n",
	  ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_y_min,
	  ts->pdata->abs_y_max);

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
			     ts->pdata->abs_x_min, ts->pdata->abs_x_max,
			     ts->pdata->abs_x_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
			     ts->pdata->abs_y_min, ts->pdata->abs_y_max,
			     ts->pdata->abs_y_fuzz, 0);
	input_set_abs_params(
		ts->input_dev, ABS_MT_TOUCH_MAJOR, ts->pdata->abs_pressure_min,
		ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
#ifndef HX_PROTOCOL_A
	input_set_abs_params(
		ts->input_dev, ABS_MT_PRESSURE, ts->pdata->abs_pressure_min,
		ts->pdata->abs_pressure_max, ts->pdata->abs_pressure_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR,
			     ts->pdata->abs_width_min, ts->pdata->abs_width_max,
			     ts->pdata->abs_pressure_fuzz, 0);
#endif
	/* input_set_abs_params(ts->input_dev, ABS_MT_AMPLITUDE, 0, */
	/* ((ts->pdata->abs_pressure_max << 16) | ts->pdata->abs_width_max), 0,
	 */
	/* 0); */
	/* input_set_abs_params(ts->input_dev, ABS_MT_POSITION, 0, (BIT(31) */
	/* | (ts->pdata->abs_x_max << 16) | ts->pdata->abs_y_max), 0, 0); */

	if (himax_input_register_device(ts->input_dev) == 0)
		return NO_ERR;

	ret = INPUT_REGISTER_FAIL;

input_device_fail:
	I("%s, input device register fail!\n", __func__);
	return ret;
}

static void calcDataSize(uint8_t finger_num)
{
	struct himax_ts_data *ts_data = private_ts;

	ts_data->coord_data_size = 4 * finger_num;
	ts_data->area_data_size =
		((finger_num / 4) + (finger_num % 4 ? 1 : 0)) * 4;
	ts_data->coordInfoSize =
		ts_data->coord_data_size + ts_data->area_data_size + 4;
	ts_data->raw_data_frame_size = 128 - ts_data->coord_data_size -
				       ts_data->area_data_size - 4 - 4 - 1;
	if (ts_data->raw_data_frame_size == 0) {
		E("%s: could NOT calculate!\n", __func__);
		return;
	}
	ts_data->raw_data_nframes =
		((uint32_t)ts_data->x_channel * ts_data->y_channel +
		 ts_data->x_channel + ts_data->y_channel) /
					ts_data->raw_data_frame_size +
				(((uint32_t)ts_data->x_channel *
					  ts_data->y_channel +
				  ts_data->x_channel + ts_data->y_channel) %
				 ts_data->raw_data_frame_size)
			? 1
			: 0;
	I("%s: coord: %d, area:%d, raw:%d, raw:%d",
	  __func__, ts_data->coord_data_size, ts_data->area_data_size,
	  ts_data->raw_data_frame_size, ts_data->raw_data_nframes);
}

void calculate_point_number(void)
{
	HX_TOUCH_INFO_POINT_CNT = ic_data->HX_MAX_PT * 4;

	if ((ic_data->HX_MAX_PT % 4) == 0)
		HX_TOUCH_INFO_POINT_CNT += (ic_data->HX_MAX_PT / 4) * 4;
	else
		HX_TOUCH_INFO_POINT_CNT += ((ic_data->HX_MAX_PT / 4) + 1) * 4;
}

#ifdef HX_AUTO_UPDATE_FW
static int i_update_FW(void)
{
	int upgrade_times = 0;
	unsigned char *ImageBuffer = i_CTPM_FW;
	int fullFileLength = sizeof(i_CTPM_FW);
	uint8_t ret = 0, result = 0;

	I("%s: i_fullFileLength = %d\n", __func__, fullFileLength);

	himax_int_enable(private_ts->client->irq, 0);
update_retry:
	if (fullFileLength == FW_SIZE_32k) {
		ret = fts_ctpm_fw_upgrade_with_sys_fs_32k(
			private_ts->client, ImageBuffer, fullFileLength, false);
	} else if (fullFileLength == FW_SIZE_60k) {
		ret = fts_ctpm_fw_upgrade_with_sys_fs_60k(
			private_ts->client, ImageBuffer, fullFileLength, false);
	} else if (fullFileLength == FW_SIZE_64k) {
		ret = fts_ctpm_fw_upgrade_with_sys_fs_64k(
			private_ts->client, ImageBuffer, fullFileLength, false);
	} else if (fullFileLength == FW_SIZE_124k) {
		ret = fts_ctpm_fw_upgrade_with_sys_fs_124k(
			private_ts->client, ImageBuffer, fullFileLength, false);
	} else if (fullFileLength == FW_SIZE_128k) {
		ret = fts_ctpm_fw_upgrade_with_sys_fs_128k(
			private_ts->client, ImageBuffer, fullFileLength, false);
	}
	if (ret == 0) {
		upgrade_times++;
		E("%s: TP upgrade error, upgrade_times = %d\n", __func__,
		  upgrade_times);
		if (upgrade_times < 3)
			goto update_retry;
		else
			result = -1; /* upgrade fail */
	} else {
		himax_read_FW_ver(private_ts->client);
		himax_touch_information(private_ts->client);
		result = 1; /* upgrade success */
		I("%s: TP upgrade OK\n", __func__);
	}
#ifdef HX_RST_PIN_FUNC
	himax_ic_reset(true, false);
#else
	himax_sense_on(private_ts->client, 0);

#endif
	himax_int_enable(private_ts->client->irq, 1);
	return result;
}
#endif

int himax_loadSensorConfig(struct i2c_client *client,
			   struct himax_i2c_platform_data *pdata)
{

	if (!client) {
		E("%s: Necessary parameters client are null!\n", __func__);
		return -1;
	}

	I("%s: initialization complete\n", __func__);

	return NO_ERR;
}

#ifdef HX_ESD_RECOVERY
void himax_esd_hw_reset(void)
{
	I("START_Himax TP: ESD - Reset\n");
#if defined(HX_TP_PROC_SELF_TEST) || defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
	if (g_self_test_entered == 1) {
		I("In self test ,not  TP: ESD - Reset\n");
		return;
	}
#endif
#if defined(HX_CHIP_STATUS_MONITOR)
	g_chip_monitor_data->HX_CHIP_POLLING_COUNT = 0;
#endif
	himax_esd_ic_reset();

	I("END_Himax TP: ESD - Reset\n");
}
#endif

#ifdef HX_CHIP_STATUS_MONITOR
static void
himax_chip_monitor_function(struct work_struct *work) /* for ESD solution */
{
	int ret = 0;

	I(" %s: POLLING_COUNT=%x, STATUS=%x\n", __func__,
	  g_chip_monitor_data->HX_CHIP_POLLING_COUNT, ret);
	if (g_chip_monitor_data->HX_CHIP_POLLING_COUNT >=
	    (g_chip_monitor_data->HX_POLLING_TIMES - 1)) { /* POLLING TIME */

		g_chip_monitor_data->HX_ON_HAND_SHAKING = 1;
		ret = himax_hand_shaking(
			private_ts->client); /* 0:Running, 1:Stop, 2:I2C Fail */
		g_chip_monitor_data->HX_ON_HAND_SHAKING = 0;
		if (ret == 2) {
			I(" %s: I2C Fail\n", __func__);
			himax_esd_hw_reset();
		} else if (ret == 1) {
			I(" %s: MCU Stop\n", __func__);
			himax_esd_hw_reset();
		}
		g_chip_monitor_data->HX_CHIP_POLLING_COUNT =
			0; /* clear polling counter */
	} else
		g_chip_monitor_data->HX_CHIP_POLLING_COUNT++;

	g_chip_monitor_data->HX_CHIP_MONITOR_EN = 1;
	queue_delayed_work(private_ts->himax_chip_monitor_wq,
			   &private_ts->himax_chip_monitor,
			   g_chip_monitor_data->HX_POLLING_TIMER * HZ);
}
#endif

#ifdef HX_SMART_WAKEUP
#ifdef HX_GESTURE_TRACK
static void gest_pt_log_coordinate(int rx, int tx)
{
	/* driver report x y with range 0 - 255 , we scale it up to x/y pixel */
	gest_pt_x[gest_pt_cnt] = rx * (ic_data->HX_X_RES) / 255;
	gest_pt_y[gest_pt_cnt] = tx * (ic_data->HX_Y_RES) / 255;
}
#endif
static int himax_parse_wake_event(struct himax_ts_data *ts)
{
	uint8_t *buf;
#ifdef HX_GESTURE_TRACK
	int tmp_max_x = 0x00, tmp_min_x = 0xFFFF, tmp_max_y = 0x00,
	    tmp_min_y = 0xFFFF;
	int gest_len;
#endif
	int i = 0, check_FC = 0, gesture_flag = 0;

	buf = kcalloc(hx_touch_data->event_size, sizeof(uint8_t), GFP_KERNEL);
	memcpy(buf, hx_touch_data->hx_event_buf, hx_touch_data->event_size);

	for (i = 0; i < GEST_PTLG_ID_LEN; i++) {
		if (check_FC == 0) {
			if ((buf[0] != 0x00) &&
			    ((buf[0] <= 0x0F) || (buf[0] == 0x80))) {
				check_FC = 1;
				gesture_flag = buf[i];
			} else {
				check_FC = 0;
				I("ID START at %x , value = %x skip\n",
				  i, buf[i]);
				break;
			}
		} else {
			if (buf[i] != gesture_flag) {
				check_FC = 0;
				I("ID NOT the same %x != %x So STOP\n",
				  buf[i], gesture_flag);
				break;
			}
		}

		I("0x%2.2X ", buf[i]);
		if (i % 8 == 7)
			I("\n");
	}
	I("Himax gesture_flag= %x\n", gesture_flag);
	I("Himax check_FC is %d\n", check_FC);

	if (check_FC == 0)
		return 0;
	if (buf[GEST_PTLG_ID_LEN] != GEST_PTLG_HDR_ID1 ||
	    buf[GEST_PTLG_ID_LEN + 1] != GEST_PTLG_HDR_ID2)
		return 0;

#ifdef HX_GESTURE_TRACK
	if (buf[GEST_PTLG_ID_LEN] == GEST_PTLG_HDR_ID1 &&
	    buf[GEST_PTLG_ID_LEN + 1] == GEST_PTLG_HDR_ID2) {
		gest_len = buf[GEST_PTLG_ID_LEN + 2];

		I("gest_len = %d ", gest_len);

		i = 0;
		gest_pt_cnt = 0;
		I("gest doornidate start\n %s", __func__);
		while (i < (gest_len + 1) / 2) {
			gest_pt_log_coordinate(
				buf[GEST_PTLG_ID_LEN + 4 + i * 2],
				buf[GEST_PTLG_ID_LEN + 4 + i * 2 + 1]);
			i++;

			I("gest_pt_x[%d]=%d\n", gest_pt_cnt,
			  gest_pt_x[gest_pt_cnt]);
			I("gest_pt_y[%d]=%d\n", gest_pt_cnt,
			  gest_pt_y[gest_pt_cnt]);

			gest_pt_cnt += 1;
		}
		if (gest_pt_cnt) {
			for (i = 0; i < gest_pt_cnt; i++) {
				if (tmp_max_x < gest_pt_x[i])
					tmp_max_x = gest_pt_x[i];
				if (tmp_min_x > gest_pt_x[i])
					tmp_min_x = gest_pt_x[i];
				if (tmp_max_y < gest_pt_y[i])
					tmp_max_y = gest_pt_y[i];
				if (tmp_min_y > gest_pt_y[i])
					tmp_min_y = gest_pt_y[i];
			}
			I("x_min=%d, x_max=%d, y_min=%d, y_max=%d\n",
			  tmp_min_x, tmp_max_x, tmp_min_y, tmp_max_y);
			gest_start_x = gest_pt_x[0];
			gn_gesture_coor[0] = gest_start_x;
			gest_start_y = gest_pt_y[0];
			gn_gesture_coor[1] = gest_start_y;
			gest_end_x = gest_pt_x[gest_pt_cnt - 1];
			gn_gesture_coor[2] = gest_end_x;
			gest_end_y = gest_pt_y[gest_pt_cnt - 1];
			gn_gesture_coor[3] = gest_end_y;
			gest_width = tmp_max_x - tmp_min_x;
			gn_gesture_coor[4] = gest_width;
			gest_height = tmp_max_y - tmp_min_y;
			gn_gesture_coor[5] = gest_height;
			gest_mid_x = (tmp_max_x + tmp_min_x) / 2;
			gn_gesture_coor[6] = gest_mid_x;
			gest_mid_y = (tmp_max_y + tmp_min_y) / 2;
			gn_gesture_coor[7] = gest_mid_y;
			gn_gesture_coor[8] = gest_mid_x; /* gest_up_x */
			gn_gesture_coor[9] =
				gest_mid_y - gest_height / 2; /* gest_up_y */
			gn_gesture_coor[10] = gest_mid_x;     /* gest_down_x */
			gn_gesture_coor[11] =
				gest_mid_y + gest_height / 2; /* gest_down_y */
			gn_gesture_coor[12] =
				gest_mid_x - gest_width / 2; /* gest_left_x */
			gn_gesture_coor[13] = gest_mid_y;    /* gest_left_y */
			gn_gesture_coor[14] =
				gest_mid_x + gest_width / 2; /* gest_right_x */
			gn_gesture_coor[15] = gest_mid_y;    /* gest_right_y */
		}
	}
#endif
	if (gesture_flag != 0x80) {
		if (!ts->gesture_cust_en[gesture_flag]) {
			I("%s NOT report customer key\n ", __func__);
			return 0; /* NOT report customer key */
		}
	} else {
		if (!ts->gesture_cust_en[0]) {
			I("%s NOT report report double click\n", __func__);
			return 0; /* NOT report power key */
		}
	}

	if (gesture_flag == 0x80)
		return EV_GESTURE_PWR;
	else
		return gesture_flag;
}

void himax_wake_check_func(void)
{
	int ret_event = 0, KEY_EVENT = 0;

	ret_event = himax_parse_wake_event(private_ts);
	switch (ret_event) {
	case EV_GESTURE_PWR:
		KEY_EVENT = KEY_POWER;
		break;
	case EV_GESTURE_01:
		KEY_EVENT = KEY_CUST_01;
		break;
	case EV_GESTURE_02:
		KEY_EVENT = KEY_CUST_02;
		break;
	case EV_GESTURE_03:
		KEY_EVENT = KEY_CUST_03;
		break;
	case EV_GESTURE_04:
		KEY_EVENT = KEY_CUST_04;
		break;
	case EV_GESTURE_05:
		KEY_EVENT = KEY_CUST_05;
		break;
	case EV_GESTURE_06:
		KEY_EVENT = KEY_CUST_06;
		break;
	case EV_GESTURE_07:
		KEY_EVENT = KEY_CUST_07;
		break;
	case EV_GESTURE_08:
		KEY_EVENT = KEY_CUST_08;
		break;
	case EV_GESTURE_09:
		KEY_EVENT = KEY_CUST_09;
		break;
	case EV_GESTURE_10:
		KEY_EVENT = KEY_CUST_10;
		break;
	case EV_GESTURE_11:
		KEY_EVENT = KEY_CUST_11;
		break;
	case EV_GESTURE_12:
		KEY_EVENT = KEY_CUST_12;
		break;
	case EV_GESTURE_13:
		KEY_EVENT = KEY_CUST_13;
		break;
	case EV_GESTURE_14:
		KEY_EVENT = KEY_CUST_14;
		break;
	case EV_GESTURE_15:
		KEY_EVENT = KEY_CUST_15;
		break;
	}
	if (ret_event) {
		I(" %s SMART WAKEUP KEY event %x press\n", __func__, KEY_EVENT);
		input_report_key(private_ts->input_dev, KEY_EVENT, 1);
		input_sync(private_ts->input_dev);
		/* msleep(100); */
		I(" %s SMART WAKEUP KEY event %x release\n", __func__,
		  KEY_EVENT);
		input_report_key(private_ts->input_dev, KEY_EVENT, 0);
		input_sync(private_ts->input_dev);
		FAKE_POWER_KEY_SEND = true;
#ifdef HX_GESTURE_TRACK
		I("start_x= %d, start_y= %d, end_x= %d, end_y= %d\n",
		  gest_start_x, gest_start_y, gest_end_x, gest_end_y);
		I("width= %d, height= %d, mid_x= %d, mid_y= %d\n",
		  gest_width, gest_height, gest_mid_x, gest_mid_y);
		I("up_x= %d, up_y= %d, down_x= %d, down_y= %d\n",
		  gn_gesture_coor[8], gn_gesture_coor[9], gn_gesture_coor[10],
		  gn_gesture_coor[11]);
		I("left_x= %d, left_y= %d, right_x= %d, right_y= %d\n",
		  gn_gesture_coor[12], gn_gesture_coor[13], gn_gesture_coor[14],
		  gn_gesture_coor[15]);
#endif
	}
}

#endif

#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
static void himax_ts_button_func(int tp_key_index, struct himax_ts_data *ts)
{
	uint16_t x_position = 0, y_position = 0;

	if (tp_key_index != 0x00) {
		I("virtual key index =%x\n", tp_key_index);
		if (tp_key_index == 0x01) {
			vk_press = 1;
			I("back key pressed\n");
			if (ts->pdata->virtual_key) {
				if (ts->button[0].index) {
					x_position =
						(ts->button[0].x_range_min +
						 ts->button[0].x_range_max) /
						2;
					y_position =
						(ts->button[0].y_range_min +
						 ts->button[0].y_range_max) /
						2;
				}
#ifdef HX_PROTOCOL_A
				input_report_abs(ts->input_dev,
						 ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev,
						 ABS_MT_TRACKING_ID, 0);

				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_Y, y_position);
				input_mt_sync(ts->input_dev);
#else
				input_mt_slot(ts->input_dev, 0);
				input_mt_report_slot_state(ts->input_dev,
							   MT_TOOL_FINGER, 1);
				input_report_abs(ts->input_dev,
						 ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev,
						 ABS_MT_WIDTH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
						 100);
				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_Y, y_position);
#endif
			} else
				input_report_key(ts->input_dev, KEY_BACK, 1);
		} else if (tp_key_index == 0x02) {
			vk_press = 1;
			I("home key pressed\n");
			if (ts->pdata->virtual_key) {
				if (ts->button[1].index) {
					x_position =
						(ts->button[1].x_range_min +
						 ts->button[1].x_range_max) /
						2;
					y_position =
						(ts->button[1].y_range_min +
						 ts->button[1].y_range_max) /
						2;
				}
#ifdef HX_PROTOCOL_A
				input_report_abs(ts->input_dev,
						 ABS_MT_TRACKING_ID, 0);
				input_report_abs(ts->input_dev,
						 ABS_MT_TOUCH_MAJOR, 100);

				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_Y, y_position);
				input_mt_sync(ts->input_dev);
#else
				input_mt_slot(ts->input_dev, 0);
				input_mt_report_slot_state(ts->input_dev,
							   MT_TOOL_FINGER, 1);
				input_report_abs(ts->input_dev,
						 ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev,
						 ABS_MT_WIDTH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
						 100);
				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_Y, y_position);
#endif
			} else
				input_report_key(ts->input_dev, KEY_HOME, 1);
		} else if (tp_key_index == 0x04) {
			vk_press = 1;
			I("APP_switch key pressed\n");
			if (ts->pdata->virtual_key) {
				if (ts->button[2].index) {
					x_position =
						(ts->button[2].x_range_min +
						 ts->button[2].x_range_max) /
						2;
					y_position =
						(ts->button[2].y_range_min +
						 ts->button[2].y_range_max) /
						2;
				}
#ifdef HX_PROTOCOL_A
				input_report_abs(ts->input_dev,
						 ABS_MT_TRACKING_ID, 0);

				input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
						 100);
				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_Y, y_position);
				input_mt_sync(ts->input_dev);
#else
				input_mt_slot(ts->input_dev, 0);
				input_mt_report_slot_state(ts->input_dev,
							   MT_TOOL_FINGER, 1);
				input_report_abs(ts->input_dev,
						 ABS_MT_TOUCH_MAJOR, 100);
				input_report_abs(ts->input_dev,
						 ABS_MT_WIDTH_MAJOR, 100);
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
						 100);
				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_X, x_position);
				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_Y, y_position);
#endif
			} else
				input_report_key(ts->input_dev, KEY_APP_SWITCH,
						 1);
		}
		input_sync(ts->input_dev);
	} else { /*tp_key_index =0x00*/

		I("virtual key released\n");
		vk_press = 0;
#ifndef HX_PROTOCOL_A
		input_mt_slot(ts->input_dev, 0);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
#else
		input_mt_sync(ts->input_dev);
#endif
		input_report_key(ts->input_dev, KEY_BACK, 0);
		input_report_key(ts->input_dev, KEY_HOME, 0);
		input_report_key(ts->input_dev, KEY_APP_SWITCH, 0);
#ifndef HX_PROTOCOL_A
		input_sync(ts->input_dev);
#endif
	}
}

void himax_report_key(struct himax_ts_data *ts)
{
	if (hx_point_num != 0) {
		/* Touch KEY */
		if ((tpd_key_old != 0x00) && (tpd_key == 0x00)) {
			/* temp_x[0] = 0xFFFF; */
			/* temp_y[0] = 0xFFFF; */
			/* temp_x[1] = 0xFFFF; */
			/* temp_y[1] = 0xFFFF; */
			hx_touch_data->finger_on = 0;
#ifdef HX_PROTOCOL_A
			input_report_key(ts->input_dev, BTN_TOUCH,
					 hx_touch_data->finger_on);
#endif
			himax_ts_button_func(tpd_key, ts);
		}
#ifndef HX_PROTOCOL_A
		input_report_key(ts->input_dev, BTN_TOUCH,
				 hx_touch_data->finger_on);
#endif
		input_sync(ts->input_dev);
	} else {
		if (tpd_key != 0x00) {
			hx_touch_data->finger_on = 1;
#ifdef HX_PROTOCOL_A
			input_report_key(ts->input_dev, BTN_TOUCH,
					 hx_touch_data->finger_on);
#endif
			himax_ts_button_func(tpd_key, ts);

		} else if ((tpd_key_old != 0x00) && (tpd_key == 0x00)) {
			hx_touch_data->finger_on = 0;
#ifdef HX_PROTOCOL_A
			input_report_key(ts->input_dev, BTN_TOUCH,
					 hx_touch_data->finger_on);
#endif
			himax_ts_button_func(tpd_key, ts);
		}
#ifndef HX_PROTOCOL_A
		input_report_key(ts->input_dev, BTN_TOUCH,
				 hx_touch_data->finger_on);
#endif
		input_sync(ts->input_dev);
	}
	tpd_key_old = tpd_key;
	Last_EN_NoiseFilter = EN_NoiseFilter;
}
#endif

int himax_report_data_init(void)
{
	if (hx_touch_data->hx_coord_buf != NULL)
		kfree(hx_touch_data->hx_coord_buf);

#ifdef HX_TP_PROC_DIAG
	if (hx_touch_data->hx_rawdata_buf != NULL)
		kfree(hx_touch_data->hx_rawdata_buf);

#endif

#if defined(HX_SMART_WAKEUP)
	hx_touch_data->event_size = himax_get_touch_data_size();
	if (hx_touch_data->hx_event_buf != NULL)
		kfree(hx_touch_data->hx_event_buf);

#endif

	hx_touch_data->touch_all_size = himax_get_touch_data_size();
	hx_touch_data->raw_cnt_max = ic_data->HX_MAX_PT / 4;
	hx_touch_data->raw_cnt_rmd = ic_data->HX_MAX_PT % 4;

	if (hx_touch_data->raw_cnt_rmd != 0x00) { /* more than 4 fingers */

		hx_touch_data->rawdata_size = cal_data_len(
			hx_touch_data->raw_cnt_rmd, ic_data->HX_MAX_PT,
			hx_touch_data->raw_cnt_max);
		hx_touch_data->touch_info_size =
			(ic_data->HX_MAX_PT + hx_touch_data->raw_cnt_max + 2) *
			4;
	} else { /* less than 4 fingers */

		hx_touch_data->rawdata_size = cal_data_len(
			hx_touch_data->raw_cnt_rmd, ic_data->HX_MAX_PT,
			hx_touch_data->raw_cnt_max);
		hx_touch_data->touch_info_size =
			(ic_data->HX_MAX_PT + hx_touch_data->raw_cnt_max + 1) *
			4;
	}
	if ((ic_data->HX_TX_NUM * ic_data->HX_RX_NUM + ic_data->HX_TX_NUM +
	     ic_data->HX_RX_NUM) %
		    hx_touch_data->rawdata_size ==
	    0)
		hx_touch_data->rawdata_frame_size =
			(ic_data->HX_TX_NUM * ic_data->HX_RX_NUM +
			 ic_data->HX_TX_NUM + ic_data->HX_RX_NUM) /
			hx_touch_data->rawdata_size;
	else
		hx_touch_data->rawdata_frame_size =
			(ic_data->HX_TX_NUM * ic_data->HX_RX_NUM +
			 ic_data->HX_TX_NUM + ic_data->HX_RX_NUM) /
				hx_touch_data->rawdata_size +
			1;
	I("%s: rawdata_frame_size = %d ", __func__,
	  hx_touch_data->rawdata_frame_size);
	I("%s: ic_data->HX_MAX_PT:%d,hx_raw_cnt_max:%d,hx_raw_cnt_rmd:%d,",
	  __func__, ic_data->HX_MAX_PT, hx_touch_data->raw_cnt_max,
	  hx_touch_data->raw_cnt_rmd);
	I("g_hx_rawdata_size:%d,hx_touch_data->touch_info_size:%d\n",
	  hx_touch_data->rawdata_size, hx_touch_data->touch_info_size);

	hx_touch_data->hx_coord_buf = kzalloc(
		sizeof(uint8_t) * (hx_touch_data->touch_info_size), GFP_KERNEL);
	if (hx_touch_data->hx_coord_buf == NULL)
		goto mem_alloc_fail;
#ifdef HX_TP_PROC_DIAG
	hx_touch_data->hx_rawdata_buf =
		kzalloc(sizeof(uint8_t) * (hx_touch_data->touch_all_size -
					   hx_touch_data->touch_info_size),
			GFP_KERNEL);
	if (hx_touch_data->hx_rawdata_buf == NULL)
		goto mem_alloc_fail;
#endif

#if defined(HX_SMART_WAKEUP)
	hx_touch_data->hx_event_buf = kzalloc(
		sizeof(uint8_t) * (hx_touch_data->event_size), GFP_KERNEL);
	if (hx_touch_data->hx_event_buf == NULL)
		goto mem_alloc_fail;
#endif

	return NO_ERR;

mem_alloc_fail:
	kfree(hx_touch_data->hx_coord_buf);
#if defined(HX_TP_PROC_DIAG)
	kfree(hx_touch_data->hx_rawdata_buf);
#endif
#if defined(HX_SMART_WAKEUP)
	kfree(hx_touch_data->hx_event_buf);
#endif

	I("%s: Memory allocate fail!\n", __func__);
	return MEM_ALLOC_FAIL;
}

#if defined(HX_USB_DETECT_GLOBAL)
void himax_cable_detect_func(bool force_renew)
{
	struct himax_ts_data *ts;
	u32 connect_status = 0;

	connect_status = USB_detect_flag; /* upmu_is_chr_det(); */
	ts = private_ts;
	/* I("Touch: cable status=%d, cable_config=%p, usb_connected=%d\n", */
	/* connect_status,ts->cable_config, ts->usb_connected); */
	if (ts->cable_config) {
		if (((!!connect_status) != ts->usb_connected) || force_renew) {

			if (!!connect_status) {
				ts->cable_config[1] = 0x01;
				ts->usb_connected = 0x01;
			} else {
				ts->cable_config[1] = 0x00;
				ts->usb_connected = 0x00;
			}

			himax_usb_detect_set(ts->client, ts->cable_config);

			I("%s: Cable status change: 0x%2.2X\n", __func__,
			  ts->usb_connected);
		}
		/* else */
		/* I("%s: Cable status is the same as previous one, */
		/* ignore.\n", __func__); */
	}
}
#endif

static void himax_report_points(struct himax_ts_data *ts)
{
	int x = 0;
	int y = 0;
	int w = 0;
	int base = 0;
	int32_t loop_i = 0;
	uint16_t old_finger = 0;

	/* I("%s:Entering\n",__func__); */

	/* finger on/press */
	if (hx_point_num != 0) {
		old_finger = ts->pre_finger_mask;
		ts->pre_finger_mask = 0;
		hx_touch_data->finger_num =
			hx_touch_data->hx_coord_buf[ts->coordInfoSize - 4] &
			0x0F;
		hx_touch_data->finger_on = 1;
		AA_press = 1;
		for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
			base = loop_i * 4;
			x = hx_touch_data->hx_coord_buf[base] << 8 |
			    hx_touch_data->hx_coord_buf[base + 1];
			y = (hx_touch_data->hx_coord_buf[base + 2] << 8 |
			     hx_touch_data->hx_coord_buf[base + 3]);
			w = hx_touch_data
				    ->hx_coord_buf[(ts->nFinger_support * 4) +
						   loop_i];
			/* x = ic_data->HX_X_RES - x; */
			/* y = ic_data->HX_Y_RES - y; */
			if (x >= 0 && x <= ts->pdata->abs_x_max && y >= 0 &&
			    y <= ts->pdata->abs_y_max) {
				hx_touch_data->finger_num--;
				if ((ts->debug_log_level & BIT(3)) > 0) {
					himax_log_touch_event_detail(
						ts, x, y, w, loop_i,
						EN_NoiseFilter, HX_FINGER_ON,
						old_finger);
				}
#ifndef HX_PROTOCOL_A

				input_mt_slot(ts->input_dev, loop_i);
#endif
				input_report_key(ts->input_dev, BTN_TOUCH,
						 hx_touch_data->finger_on);
				input_report_abs(ts->input_dev,
						 ABS_MT_TOUCH_MAJOR, w);
				input_report_abs(ts->input_dev,
						 ABS_MT_TRACKING_ID, loop_i);
#ifndef HX_PROTOCOL_A
				input_report_abs(ts->input_dev,
						 ABS_MT_WIDTH_MAJOR, w);
				input_report_abs(ts->input_dev, ABS_MT_PRESSURE,
						 w);
#endif
				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_X, x);
				input_report_abs(ts->input_dev,
						 ABS_MT_POSITION_Y, y);

#ifndef HX_PROTOCOL_A
				ts->last_slot = loop_i;
				input_mt_report_slot_state(ts->input_dev,
							   MT_TOOL_FINGER, 1);
#else
				input_mt_sync(ts->input_dev);
#endif

				if (!ts->first_pressed) {
					ts->first_pressed = 1;
					I("S1@%d, %d\n", x, y);
				}

				ts->pre_finger_data[loop_i][0] = x;
				ts->pre_finger_data[loop_i][1] = y;
				if (ts->debug_log_level & BIT(1))
					himax_log_touch_event(x, y, w, loop_i,
							      EN_NoiseFilter,
							      HX_FINGER_ON);

				ts->pre_finger_mask =
					ts->pre_finger_mask + (1 << loop_i);
			}
			/* report coordinates */
			else {
#ifndef HX_PROTOCOL_A
				input_mt_slot(ts->input_dev, loop_i);
				input_mt_report_slot_state(ts->input_dev,
							   MT_TOOL_FINGER, 0);
#endif

				if (loop_i == 0 && ts->first_pressed == 1) {
					ts->first_pressed = 2;
					I("E1@%d, %d\n",
					  ts->pre_finger_data[0][0],
					  ts->pre_finger_data[0][1]);
				}
				if ((ts->debug_log_level & BIT(3)) > 0) {
					himax_log_touch_event_detail(
						ts, x, y, w, loop_i,
						Last_EN_NoiseFilter,
						HX_FINGER_LEAVE, old_finger);
				}
			}
		}
#ifndef HX_PROTOCOL_A
		input_report_key(ts->input_dev, BTN_TOUCH,
				 hx_touch_data->finger_on);
#endif
		input_sync(ts->input_dev);
	}

	/* finger leave/release */
	else {
#if defined(HX_PALM_REPORT)
		if (himax_palm_detect(hx_touch_data->hx_coord_buf) == NO_ERR) {
			I(" %s HX_PALM_REPORT KEY power event press\n",
			  __func__);
			input_report_key(ts->input_dev, KEY_POWER, 1);
			input_sync(ts->input_dev);
			msleep(100);
			I(" %s HX_PALM_REPORT KEY power event release\n",
			  __func__);
			input_report_key(ts->input_dev, KEY_POWER, 0);
			input_sync(ts->input_dev);
			return;
		}
#endif
		hx_touch_data->finger_on = 0;
		AA_press = 0;
#ifdef HX_PROTOCOL_A
		input_mt_sync(ts->input_dev);
#endif

		for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
			if (((ts->pre_finger_mask >> loop_i) & 1) == 1) {
#ifndef HX_PROTOCOL_A
				input_mt_slot(ts->input_dev, loop_i);
				input_mt_report_slot_state(ts->input_dev,
							   MT_TOOL_FINGER, 0);
#endif
			}
			if (ts->pre_finger_mask > 0 &&
			    (ts->debug_log_level & BIT(3)) > 0) {
				if (((ts->pre_finger_mask >> loop_i) & 1) ==
				    1) {
					if (ts->useScreenRes) {
						I("%X,%02d,%d,%d,%d\n",
						  0, loop_i + 1,
						  ts->pre_finger_data[loop_i]
								     [0] *
					ts->widthFactor >>
							  SHIFTBITS,
						  ts->pre_finger_data[loop_i]
								     [1] *
					ts->heightFactor >>
							  SHIFTBITS,
						  Last_EN_NoiseFilter);
					} else {
						I("%X,%02d,%d,%d,%d\n",
						  0, loop_i + 1,
						  ts->pre_finger_data[loop_i]
								     [0],
						  ts->pre_finger_data[loop_i]
								     [1],
						  Last_EN_NoiseFilter);
					}
				}
			}
		}
		if (ts->pre_finger_mask > 0)

			ts->pre_finger_mask = 0;

		if (ts->first_pressed == 1) {
			ts->first_pressed = 2;
			I("E1@%d, %d\n", ts->pre_finger_data[0][0],
			  ts->pre_finger_data[0][1]);
		}

		if (ts->debug_log_level & BIT(1))
			himax_log_touch_event(x, y, w, loop_i, EN_NoiseFilter,
					      HX_FINGER_LEAVE);

		input_report_key(ts->input_dev, BTN_TOUCH,
				 hx_touch_data->finger_on);
		input_sync(ts->input_dev);
	}
	Last_EN_NoiseFilter = EN_NoiseFilter;

	/* I("%s:End\n",__func__); */
}

static void himax_report_all_leave(struct himax_ts_data *ts)
{
	int loop_i = 0;

	for (loop_i = 0; loop_i < ts->nFinger_support; loop_i++) {
#ifndef HX_PROTOCOL_A
		input_mt_slot(ts->input_dev, loop_i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);
#endif
	}
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
	input_sync(ts->input_dev);
}

int himax_touch_get(struct himax_ts_data *ts, uint8_t *buf, int ts_status)
{
	int ret = 0;

	switch (ts_status) {
	/*normal*/
	case 1:
#ifdef HX_TP_PROC_DIAG
		hx_touch_data->diag_cmd = getDiagCommand();

		if ((hx_touch_data->diag_cmd) || (HX_HW_RESET_ACTIVATE)
#ifdef HX_ESD_RECOVERY
		    || (HX_ESD_RESET_ACTIVATE)
#endif
			    ) {
			ret = himax_read_event_stack(ts->client, buf, 128);
		} else {
			ret = himax_read_event_stack(
				ts->client, buf,
				hx_touch_data->touch_info_size);
		}

		if (!ret)
#else
		if (!himax_read_event_stack(ts->client, buf,
					    hx_touch_data->touch_info_size))
#endif
		{
			E("%s: can't read data from chip!\n", __func__);
			goto err_workqueue_out;
		}
		break;
#if defined(HX_SMART_WAKEUP)
	/*SMWP*/
	case 2:
		himax_burst_enable(ts->client, 0);
		if (!himax_read_event_stack(ts->client, buf,
					    hx_touch_data->event_size)) {
			E("%s: can't read data from chip!\n", __func__);
			goto err_workqueue_out;
		}
		break;
#endif
	default:
		break;
	}
	return NO_ERR;

err_workqueue_out:
	return I2C_FAIL;
}

int himax_checksum_cal(struct himax_ts_data *ts, uint8_t *buf, int ts_status)
{
#if defined(HX_ESD_RECOVERY)
	int hx_EB_event = 0;
	int hx_EC_event = 0;
	int hx_ED_event = 0;
	int hx_esd_event = 0;
	int hx_zero_event = 0;
	int shaking_ret = 0;
#endif
	uint16_t check_sum_cal = 0;
	int32_t loop_i = 0;
	int length = 0;

	/* Normal */
	if (ts_status == HX_REPORT_COORD)
		length = hx_touch_data->touch_info_size;
#if defined(HX_SMART_WAKEUP)
	/* SMWP */
	else if (ts_status == HX_REPORT_SMWP_EVENT)
		length = (GEST_PTLG_ID_LEN + GEST_PTLG_HDR_LEN);
#endif
	else
		I("%s, Neither Normal Nor SMWP error!\n", __func__);

	/* I("Now status=%d,length=%d\n",ts_status,length); */
	for (loop_i = 0; loop_i < length; loop_i++) {
		check_sum_cal += buf[loop_i];

#ifdef HX_ESD_RECOVERY
		if (ts_status == HX_REPORT_COORD) {
			/* case 1 ESD recovery flow */
			if (buf[loop_i] == 0xEB)
				hx_EB_event++;
			else if (buf[loop_i] == 0xEC)
				hx_EC_event++;
			else if (buf[loop_i] == 0xED)
				hx_ED_event++;
			/* case 2 ESD recovery flow-Disable */
			else if (buf[loop_i] == 0x00)
				hx_zero_event++;
			else {
				hx_EB_event = 0;
				hx_EC_event = 0;
				hx_ED_event = 0;
				hx_zero_event = 0;
				g_zero_event_count = 0;
			}

			if (hx_EB_event == length) {
				hx_esd_event = length;
				hx_EB_event_flag++;
				I("[HIMAX TP MSG]: ALL 0xEB.\n");
			} else if (hx_EC_event == length) {
				hx_esd_event = length;
				hx_EC_event_flag++;
				I("[HIMAX TP MSG]: ALL 0xEC.\n");
			} else if (hx_ED_event == length) {
				hx_esd_event = length;
				hx_ED_event_flag++;
				I("[HIMAX TP MSG]: ALL 0xED.\n");
			} else {
				hx_esd_event = 0;
			}
		}
#endif
	}

	if (ts_status == HX_REPORT_COORD) {
#ifdef HX_ESD_RECOVERY
		if ((hx_esd_event == length || hx_zero_event == length) &&
		    (HX_HW_RESET_ACTIVATE == 0) && (HX_ESD_RESET_ACTIVATE == 0)
#if defined(HX_TP_PROC_DIAG)
		    && (hx_touch_data->diag_cmd == 0)
#endif
#if defined(HX_TP_PROC_SELF_TEST) || defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
		    && (g_self_test_entered == 0)
#endif
			    ) {
			shaking_ret = himax_ic_esd_recovery(
				hx_esd_event, hx_zero_event, length);
			if (shaking_ret == CHECKSUM_FAIL) {
				himax_esd_hw_reset();
				goto checksum_fail;
			} else if (shaking_ret == ERR_WORK_OUT)
				goto err_workqueue_out;
			else {
				/* I("I2C running. Nothing to be done!\n"); */
				goto workqueue_out;
			}
		} else if (HX_ESD_RESET_ACTIVATE) {
#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) ||                      \
	defined(HX_USB_DETECT_GLOBAL)
#ifdef HX_RESUME_SEND_CMD
			himax_rst_cmd_recovery_func(ts->suspended);
#endif
#endif
			/* drop 1st interrupts after chip reset */
			HX_ESD_RESET_ACTIVATE = 0;
			I("[HX_ESD_RESET_ACTIVATE]:%s:\n",
			/* Back from reset, ready to serve */
			  __func__);
			goto checksum_fail;
		}

		else if (HX_HW_RESET_ACTIVATE)
#else
		if (HX_HW_RESET_ACTIVATE)
#endif
		{
#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) ||                      \
	defined(HX_USB_DETECT_GLOBAL)
#ifdef HX_RESUME_SEND_CMD
			himax_rst_cmd_recovery_func(ts->suspended);
#endif
#endif
			/* drop 1st interrupts after chip reset */
			HX_HW_RESET_ACTIVATE = 0;
			I("[HX_HW_RESET_ACTIVATE]:%s:\n",
			/*  Back from reset, ready to serve */
			  __func__);
			goto ready_to_serve;
		}
	}

	if ((check_sum_cal % 0x100 != 0)) {
		I("[HIMAX TP MSG] checksum fail : check_sum_cal: 0x%02X\n",
		  check_sum_cal);
		goto checksum_fail;
	}

	/* I("%s:End\n",__func__); */
	return NO_ERR;

ready_to_serve:
	return READY_TO_SERVE;
checksum_fail:
	return CHECKSUM_FAIL;
#ifdef HX_ESD_RECOVERY
err_workqueue_out:
	return ERR_WORK_OUT;
workqueue_out:
	return WORK_OUT;
#endif
}

int himax_ts_work_status(struct himax_ts_data *ts)
{
	/* 1: normal, 2:SMWP */
	int result = HX_REPORT_COORD;
	uint8_t diag_cmd = 0;

#ifdef HX_TP_PROC_DIAG
	diag_cmd = getDiagCommand();
#endif

#ifdef HX_SMART_WAKEUP
	if (atomic_read(&ts->suspend_mode) && (!FAKE_POWER_KEY_SEND) &&
	    (ts->SMWP_enable) && (!diag_cmd)) {
		result = HX_REPORT_SMWP_EVENT;
	}
#endif
	/* I("Now Status is %d\n",result); */
	return result;
}

void himax_assign_touch_data(uint8_t *buf, int ts_status)
{
	uint8_t hx_state_info_pos = hx_touch_data->touch_info_size - 3;

	if (ts_status == HX_REPORT_COORD) {
		memcpy(hx_touch_data->hx_coord_buf, &buf[0],
		       hx_touch_data->touch_info_size);
		if (buf[hx_state_info_pos] != 0xFF &&
		    buf[hx_state_info_pos + 1] != 0xFF)
			memcpy(hx_touch_data->hx_state_info,
			       &buf[hx_state_info_pos], 2);
		else
			memset(hx_touch_data->hx_state_info, 0x00,
			       sizeof(hx_touch_data->hx_state_info));
	}
#if defined(HX_SMART_WAKEUP)
	else
		memcpy(hx_touch_data->hx_event_buf, buf,
		       hx_touch_data->event_size);
#endif

#ifdef HX_TP_PROC_DIAG
	if ((hx_touch_data->diag_cmd) || (HX_HW_RESET_ACTIVATE)
#ifdef HX_ESD_RECOVERY
	    || (HX_ESD_RESET_ACTIVATE)
#endif
		    ) {
		memcpy(hx_touch_data->hx_rawdata_buf,
		       &buf[hx_touch_data->touch_info_size],
		       hx_touch_data->touch_all_size -
			       hx_touch_data->touch_info_size);
	}
#endif
}

void himax_coord_report(struct himax_ts_data *ts)
{

#if defined(HX_TP_PROC_DIAG)
	/* touch monitor raw data fetch */
	if (himax_set_diag_cmd(ic_data, hx_touch_data))
		I("%s: coordinate dump fail and bypass with checksum err\n",
		  __func__);
#endif
	EN_NoiseFilter =
		(hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT + 2] >> 3);
	/* I("EN_NoiseFilter=%d\n",EN_NoiseFilter); */
	EN_NoiseFilter = EN_NoiseFilter & 0x01;
/* I("EN_NoiseFilter2=%d\n",EN_NoiseFilter); */

#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
	tpd_key =
		(hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT + 2] >> 4);
	/* All (VK+AA)leave */
	if (tpd_key == 0x0F)
		tpd_key = 0x00;

/* I("[DEBUG] tpd_key:  %x\r\n", tpd_key); */
#else
	tpd_key = 0x00;
#endif

	p_point_num = hx_point_num;

	if (hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT] == 0xff)
		hx_point_num = 0;
	else
		hx_point_num =
			hx_touch_data->hx_coord_buf[HX_TOUCH_INFO_POINT_CNT] &
			0x0f;

	/* Touch Point information */
	if (!tpd_key && !tpd_key_old)
		himax_report_points(ts);
#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
	else
		himax_report_key(ts);
#endif
	/* I("%s:END\n",__func__); */
}

void himax_ts_work(struct himax_ts_data *ts)
{
	uint8_t hw_reset_check[2];
	uint8_t buf[128];
	int check_sum_cal = 0;
	/* int loop_i = 0; */
	int ts_status = 0;
#ifdef HX_CHIP_STATUS_MONITOR
	int j = 0;
#endif

#if defined(HX_USB_DETECT_GLOBAL)
	himax_cable_detect_func(false);
#endif

#if defined(HX_CHIP_STATUS_MONITOR)
	g_chip_monitor_data->HX_CHIP_POLLING_COUNT = 0;
	if (g_chip_monitor_data->HX_ON_HAND_SHAKING) /* chip on hand */
						     /* shaking,wait hand */
						     /* shaking */
	{
		for (j = 0; j < 100; j++) {
			if (g_chip_monitor_data->HX_ON_HAND_SHAKING ==
			    0) { /* chip on hand shaking end */

				I("%s:HX_ON_HAND_SHAKING OK check %d times\n",
				  __func__, j);
				break;
			}
			msleep(20);
		}
		if (j == 100) {
			E("%s:HX_ON_HAND_SHAKING timeout reject interrupt\n",
			  __func__);
			return;
		}
	}
#endif

	ts_status = himax_ts_work_status(ts);
	if (ts_status > HX_REPORT_SMWP_EVENT || ts_status < HX_REPORT_COORD)
		goto neither_normal_nor_smwp;

	memset(buf, 0x00, sizeof(buf));
	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));

	/* I("New Method for ts_work\n"); */

	if (himax_touch_get(ts, buf, ts_status))
		goto err_workqueue_out;

	if (ts->debug_log_level & BIT(0))

		himax_log_touch_data(buf, hx_touch_data);

	check_sum_cal = himax_checksum_cal(ts, buf, ts_status);
	if (check_sum_cal == CHECKSUM_FAIL)
		goto checksum_fail;
	else if (check_sum_cal == READY_TO_SERVE)
		goto ready_to_serve;
	else if (check_sum_cal == ERR_WORK_OUT)
		goto err_workqueue_out;
	else if (check_sum_cal == WORK_OUT)
		goto workqueue_out;
	/* checksum calculate pass and assign data to global touch data*/
	else
		himax_assign_touch_data(buf, ts_status);

	if (ts_status == HX_REPORT_COORD)
		himax_coord_report(ts);
#if defined(HX_SMART_WAKEUP)
	else {
		wake_lock_timeout(&ts->ts_SMWP_wake_lock, TS_WAKE_LOCK_TIMEOUT);
		himax_wake_check_func();
	}
#endif

checksum_fail:
workqueue_out:
ready_to_serve:
neither_normal_nor_smwp:
	return;

err_workqueue_out:
	I("%s: Now reset the Touch chip.\n", __func__);

#ifdef HX_RST_PIN_FUNC
	himax_ic_reset(false, true);
#endif

	goto workqueue_out;
}
enum hrtimer_restart himax_ts_timer_func(struct hrtimer *timer)
{
	struct himax_ts_data *ts;

	ts = container_of(timer, struct himax_ts_data, timer);
	queue_work(ts->himax_wq, &ts->work);
	hrtimer_start(&ts->timer, ktime_set(0, 12500000), HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

#if defined(HX_USB_DETECT_CALLBACK)
static void himax_cable_tp_status_handler_func(int connect_status)
{
	struct himax_ts_data *ts;

	I("Touch: cable change to %d\n", connect_status);
	ts = private_ts;
	if (ts->cable_config) {
		if (!atomic_read(&ts->suspend_mode)) {
			if ((!!connect_status) != ts->usb_connected) {
				if (!!connect_status) {
					ts->cable_config[1] = 0x01;
					ts->usb_connected = 0x01;
				} else {
					ts->cable_config[1] = 0x00;
					ts->usb_connected = 0x00;
				}

				i2c_himax_master_write(ts->client,
						       ts->cable_config,
						       sizeof(ts->cable_config),
						       DEFAULT_RETRY_CNT);

				I("%s:change: 0x%2.2X\n",
				  __func__, ts->cable_config[1]);
			} else
				I("%s:same as previous one, ignore.\n",
				  __func__);
		} else {
			if (connect_status)
				ts->usb_connected = 0x01;
			else
				ts->usb_connected = 0x00;
			I("%s: Cable status remembered: 0x%2.2X\n", __func__,
			  ts->usb_connected);
		}
	}
}

static struct t_cable_status_notifier himax_cable_status_handler = {
	.name = "usb_tp_connected", .func = himax_cable_tp_status_handler_func,
};

#endif

#ifdef HX_AUTO_UPDATE_FW
static void himax_update_register(struct work_struct *work)
{
	I(" %s in", __func__);
#if defined(HX_CHIP_STATUS_MONITOR)
	I("Cancel Chip monitor during auto-updating!\n");
	g_chip_monitor_data->HX_CHIP_POLLING_COUNT = 0;
	g_chip_monitor_data->HX_CHIP_MONITOR_EN = 0;
	cancel_delayed_work_sync(&private_ts->himax_chip_monitor);
#endif
	if (i_update_FW() == false)
		I("NOT Have new FW=NOT UPDATE=\n");
	else
		I("Have new FW=UPDATE=\n");
#ifdef HX_CHIP_STATUS_MONITOR
	I("Auto-updating over, now chip monitor working!\n");
	g_chip_monitor_data->HX_CHIP_POLLING_COUNT = 0;
	g_chip_monitor_data->HX_CHIP_MONITOR_EN = 1;
	queue_delayed_work(private_ts->himax_chip_monitor_wq,
			   &private_ts->himax_chip_monitor,
			   g_chip_monitor_data->HX_POLLING_TIMER * HZ);
#endif
}
#endif

#ifdef CONFIG_FB
static void himax_fb_register(struct work_struct *work)
{
	int ret = 0;
	struct himax_ts_data *ts =
		container_of(work, struct himax_ts_data, work_att.work);
	I(" %s in\n", __func__);

	ts->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts->fb_notif);
	if (ret)
		E(" Unable to register fb_notifier: %d\n", ret);
}
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
static void himax_ito_test_work(struct work_struct *work)
{
	I(" %s in\n", __func__);
	himax_ito_test();
}
#endif

#ifdef HX_TP_PROC_FLASH_DUMP
static void himax_ts_flash_work_func(struct work_struct *work)
{
	himax_ts_flash_func();
}
#endif

#ifdef HX_TP_PROC_GUEST_INFO
static void himax_ts_guest_info_work_func(struct work_struct *work)
{

	himax_read_project_id();
}
#endif

#ifdef HX_TP_PROC_DIAG
static void himax_ts_diag_work_func(struct work_struct *work)
{
	himax_ts_diag_func();
}
#endif

int himax_chip_common_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
#ifdef HX_AUTO_UPDATE_FW
	bool auto_update_flag = false;
#endif
	int ret = 0, err = 0;
	struct himax_ts_data *ts;
	struct himax_i2c_platform_data *pdata;

	/* Check I2C functionality */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		E("%s: i2c check functionality error\n", __func__);
		err = -ENODEV;
		goto err_check_functionality_failed;
	}

	ts = kzalloc(sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		E("%s: allocate himax_ts_data failed\n", __func__);
		err = -ENOMEM;
		goto err_alloc_data_failed;
	}

	i2c_set_clientdata(client, ts);
	ts->client = client;
	ts->dev = &client->dev;
	mutex_init(&ts->rw_lock);

	pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
	if (pdata == NULL) { /*Allocate Platform data space*/

		err = -ENOMEM;
		goto err_dt_platform_data_fail;
	}

	ic_data = kzalloc(sizeof(*ic_data), GFP_KERNEL);
	if (ic_data == NULL) { /*Allocate IC data space*/

		err = -ENOMEM;
		goto err_dt_ic_data_fail;
	}

	/* allocate report data */
	hx_touch_data = kzalloc(sizeof(struct himax_report_data), GFP_KERNEL);
	if (hx_touch_data == NULL) {
		err = -ENOMEM;
		goto err_alloc_touch_data_failed;
	}

	if (himax_parse_dt(ts, pdata) < 0) {
		I(" pdata is NULL for DT\n");
		goto err_alloc_dt_pdata_failed;
	}

#ifdef HX_RST_PIN_FUNC
	ts->rst_gpio = pdata->gpio_reset;
#endif

	himax_gpio_power_config(ts->client, pdata);

#ifndef CONFIG_OF
	if (pdata->power) {
		ret = pdata->power(1);
		if (ret < 0) {
			E("%s: power on failed\n", __func__);
			goto err_power_failed;
		}
	}
#endif
	private_ts = ts;

	if (himax_ic_package_check(ts->client) == false) {
		E("Himax chip doesn NOT EXIST");
		goto err_ic_package_failed;
	}

	if (pdata->virtual_key)
		ts->button = pdata->virtual_key;
#ifdef HX_TP_PROC_FLASH_DUMP
	ts->flash_wq = create_singlethread_workqueue("himax_flash_wq");
	if (!ts->flash_wq) {
		E("%s: create flash workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_flash_dump_wq_failed;
	}

	INIT_WORK(&ts->flash_work, himax_ts_flash_work_func);

	setSysOperation(0);
	setFlashBuffer();
#endif

#ifdef HX_TP_PROC_GUEST_INFO
	ts->guest_info_wq =
		create_singlethread_workqueue("himax_guest_info_wq");
	if (!ts->guest_info_wq) {
		E("%s: create guest info workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_guest_info_wq_failed;
	}
	INIT_WORK(&ts->guest_info_work, himax_ts_guest_info_work_func);
#endif

#ifdef HX_TP_PROC_DIAG
	ts->himax_diag_wq = create_singlethread_workqueue("himax_diag");
	if (!ts->himax_diag_wq) {
		E("%s: create diag workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_diag_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->himax_diag_delay_wrok, himax_ts_diag_work_func);
#endif

#ifdef HX_AUTO_UPDATE_FW
	auto_update_flag = himax_calculateChecksum(client, false);
	auto_update_flag |= himax_flash_lastdata_check(client);
	if (auto_update_flag)
		goto FW_force_upgrade;
#endif
	himax_read_FW_ver(client);

#ifdef HX_AUTO_UPDATE_FW
FW_force_upgrade:
	auto_update_flag |= ((ic_data->vendor_fw_ver < g_i_FW_VER) ||
			     (ic_data->vendor_config_ver < g_i_CFG_VER));
	/* Not sure to do */
	/* auto_update_flag |= ((ic_data->vendor_cid_maj_ver != g_i_CID_MAJ) ||
	 */
	/* (ic_data->vendor_cid_min_ver < g_i_CID_MIN)); */
	if (auto_update_flag) {
		ts->himax_update_wq =
			create_singlethread_workqueue("HMX_update_request");
		if (!ts->himax_update_wq) {
			E(" allocate syn_update_wq failed\n");
			err = -ENOMEM;
			goto err_update_wq_failed;
		}
		INIT_DELAYED_WORK(&ts->work_update, himax_update_register);
		queue_delayed_work(ts->himax_update_wq, &ts->work_update,
				   msecs_to_jiffies(2000));
	}
#endif

#ifdef HX_ZERO_FLASH
	ts->himax_0f_update_wq =
		create_singlethread_workqueue("HMX_0f_update_request");
	if (!ts->himax_0f_update_wq) {
		E(" allocate himax_0f_update_wq failed\n");
		err = -ENOMEM;
		goto err_0f_update_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->work_0f_update, himax_0f_operation);
	queue_delayed_work(ts->himax_0f_update_wq, &ts->work_0f_update,
			   msecs_to_jiffies(2000));
#endif
	/* Himax Power On and Load Config */
	if (himax_loadSensorConfig(client, pdata)) {
		E("%s: Load Sesnsor configuration failed, unload driver.\n",
		  __func__);
		goto err_detect_failed;
	}
	himax_power_on_init(client);

	calculate_point_number();
#ifdef HX_TP_PROC_DIAG
	setXChannel(ic_data->HX_RX_NUM); /* X channel */
	setYChannel(ic_data->HX_TX_NUM); /* Y channel */

	setMutualBuffer();
	setMutualNewBuffer();
	setMutualOldBuffer();
	if (getMutualBuffer() == NULL) {
		E("%s: mutual buffer allocate fail failed\n", __func__);
		return -1;
	}
#ifdef HX_TP_PROC_2T2R
	if (Is_2T2R) {
		setXChannel_2(ic_data->HX_RX_NUM_2); /* X channel */
		setYChannel_2(ic_data->HX_TX_NUM_2); /* Y channel */

		setMutualBuffer_2();

		if (getMutualBuffer_2() == NULL) {
			E("%s: mutual buffer 2 allocate fail failed\n",
			  __func__);
			return -1;
		}
	}
#endif
#endif
#ifdef CONFIG_OF
	ts->power = pdata->power;
#endif
	ts->pdata = pdata;

	ts->x_channel = ic_data->HX_RX_NUM;
	ts->y_channel = ic_data->HX_TX_NUM;
	ts->nFinger_support = ic_data->HX_MAX_PT;
	/* calculate the i2c data size */
	calcDataSize(ts->nFinger_support);
	I("%s: calcDataSize complete\n", __func__);
#ifdef CONFIG_OF
	ts->pdata->abs_pressure_min = 0;
	ts->pdata->abs_pressure_max = 200;
	ts->pdata->abs_width_min = 0;
	ts->pdata->abs_width_max = 200;
	pdata->cable_config[0] = 0xF0;
	pdata->cable_config[1] = 0x00;
#endif
	ts->suspended = false;
#if defined(HX_USB_DETECT_CALLBACK) || defined(HX_USB_DETECT_GLOBAL)
	ts->usb_connected = 0x00;
	ts->cable_config = pdata->cable_config;
#endif
#ifdef HX_PROTOCOL_A
	ts->protocol_type = PROTOCOL_TYPE_A;
#else
	ts->protocol_type = PROTOCOL_TYPE_B;
#endif
	I("%s: Use Protocol Type %c\n", __func__,
	  ts->protocol_type == PROTOCOL_TYPE_A ? 'A' : 'B');

	ret = himax_input_register(ts);
	if (ret) {
		E("%s: Unable to register %s input device\n", __func__,
		  ts->input_dev->name);
		goto err_input_register_device_failed;
	}
#ifdef CONFIG_FB
	ts->himax_att_wq = create_singlethread_workqueue("HMX_ATT_request");
	if (!ts->himax_att_wq) {
		E(" allocate syn_att_wq failed\n");
		err = -ENOMEM;
		goto err_get_intr_bit_failed;
	}
	INIT_DELAYED_WORK(&ts->work_att, himax_fb_register);
	queue_delayed_work(ts->himax_att_wq, &ts->work_att,
			   msecs_to_jiffies(15000));
#endif

#if defined(HX_CHIP_STATUS_MONITOR) /* for ESD solution */
	I("Enter HX_CHIP_STATUS_MONITOR!\n");

	g_chip_monitor_data =
		kzalloc(sizeof(struct chip_monitor_data), GFP_KERNEL);
	if (g_chip_monitor_data == NULL) {
		err = -ENOMEM;
		goto err_alloc_monitor_data;
	}
	g_chip_monitor_data->HX_CHIP_POLLING_COUNT = 0;
	g_chip_monitor_data->HX_POLLING_TIMER = 5; /* unit:sec */
	g_chip_monitor_data->HX_POLLING_TIMES =
		2; /* ex:5(timer)x2(times)=10sec(polling time) */
	g_chip_monitor_data->HX_ON_HAND_SHAKING = 0; /*  */
	g_chip_monitor_data->HX_CHIP_MONITOR_EN = 1;

	ts->himax_chip_monitor_wq =
		create_singlethread_workqueue("himax_chip_monitor_wq");
	if (!ts->himax_chip_monitor_wq) {
		E(" %s: create workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_chip_monitor_wq_failed;
	}
	g_chip_monitor_data->HX_CHIP_MONITOR_EN = 1;

	INIT_DELAYED_WORK(&ts->himax_chip_monitor, himax_chip_monitor_function);

	queue_delayed_work(ts->himax_chip_monitor_wq, &ts->himax_chip_monitor,
			   g_chip_monitor_data->HX_POLLING_TIMER * HZ);

#endif

#ifdef HX_SMART_WAKEUP
	ts->SMWP_enable = 0;
	wake_lock_init(&ts->ts_SMWP_wake_lock, WAKE_LOCK_SUSPEND,
		       HIMAX_common_NAME);
#endif
#ifdef HX_HIGH_SENSE
	ts->HSEN_enable = 0;
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
	ts->ito_test_wq = create_singlethread_workqueue("himax_ito_test_wq");
	if (!ts->ito_test_wq) {
		E("%s: ito test workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_ito_test_wq_failed;
	}

	INIT_WORK(&ts->ito_test_work, himax_ito_test_work);
#endif

	/* touch data init */
	err = himax_report_data_init();
	if (err)
		goto err_report_data_init_failed;

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_touch_proc_init();
#endif

#if defined(HX_USB_DETECT_CALLBACK)
	if (ts->cable_config)
		cable_detect_register_notifier(&himax_cable_status_handler);
#endif

	err = himax_ts_register_interrupt(ts->client);
	if (err)
		goto err_register_interrupt_failed;

#if defined(HX_AUTO_UPDATE_FW) || defined(HX_ZERO_FLASH)
	if (auto_update_flag)
		himax_int_enable(client->irq, 0);

#endif

	return 0;

err_register_interrupt_failed:
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_touch_proc_deinit();
#endif
err_report_data_init_failed:

#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
	destroy_workqueue(ts->ito_test_wq);
err_ito_test_wq_failed:
#endif
#ifdef HX_SMART_WAKEUP
	wake_lock_destroy(&ts->ts_SMWP_wake_lock);
#endif
#ifdef HX_CHIP_STATUS_MONITOR
	g_chip_monitor_data->HX_CHIP_MONITOR_EN = 0;
	cancel_delayed_work_sync(&ts->himax_chip_monitor);
	destroy_workqueue(ts->himax_chip_monitor_wq);
err_create_chip_monitor_wq_failed:
	kfree(g_chip_monitor_data);
err_alloc_monitor_data:
#endif
#ifdef CONFIG_FB
	cancel_delayed_work_sync(&ts->work_att);
	destroy_workqueue(ts->himax_att_wq);
err_get_intr_bit_failed:
#endif
err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_detect_failed:
#ifdef HX_ZERO_FLASH
	cancel_delayed_work_sync(&ts->work_0f_update);
	destroy_workqueue(ts->himax_0f_update_wq);
err_0f_update_wq_failed:
#endif
#ifdef HX_AUTO_UPDATE_FW
	if (auto_update_flag) {
		cancel_delayed_work_sync(&ts->work_update);
		destroy_workqueue(ts->himax_update_wq);
	}
err_update_wq_failed:
#endif

#ifdef HX_TP_PROC_DIAG
	cancel_delayed_work_sync(&ts->himax_diag_delay_wrok);
	destroy_workqueue(ts->himax_diag_wq);
err_create_diag_wq_failed:
#endif

#ifdef HX_TP_PROC_GUEST_INFO
	destroy_workqueue(ts->guest_info_wq);
err_create_guest_info_wq_failed:
#endif

#ifdef HX_TP_PROC_FLASH_DUMP
	destroy_workqueue(ts->flash_wq);
err_create_flash_dump_wq_failed:
#endif
err_ic_package_failed:

	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);
#ifdef HX_RST_PIN_FUNC
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);
#endif

#ifndef CONFIG_OF
err_power_failed:
#endif

err_alloc_dt_pdata_failed:
	kfree(hx_touch_data);
err_alloc_touch_data_failed:
	kfree(ic_data);
err_dt_ic_data_fail:
	kfree(pdata);

err_dt_platform_data_fail:
	kfree(ts);

err_alloc_data_failed:
err_check_functionality_failed:
	probe_fail_flag = 1;
	return err;
}

int himax_chip_common_remove(struct i2c_client *client)
{
	struct himax_ts_data *ts = i2c_get_clientdata(client);

	if (!ts->use_irq) {
		hrtimer_cancel(&ts->timer);
		destroy_workqueue(ts->himax_wq);
	}

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_touch_proc_deinit();
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_ITO_TEST)
	destroy_workqueue(ts->ito_test_wq);
#endif
#ifdef HX_SMART_WAKEUP
	wake_lock_destroy(&ts->ts_SMWP_wake_lock);
#endif
#ifdef HX_CHIP_STATUS_MONITOR
	g_chip_monitor_data->HX_CHIP_MONITOR_EN = 0;
	cancel_delayed_work_sync(&ts->himax_chip_monitor);
	destroy_workqueue(ts->himax_chip_monitor_wq);
	kfree(g_chip_monitor_data);
#endif
#ifdef CONFIG_FB
	if (fb_unregister_client(&ts->fb_notif))
		E("Error occurred while unregistering fb_notifier.\n");
	cancel_delayed_work_sync(&ts->work_att);
	destroy_workqueue(ts->himax_att_wq);
#endif
	input_free_device(ts->input_dev);
#ifdef HX_ZERO_FLASH
	cancel_delayed_work_sync(&ts->work_0f_update);
	destroy_workqueue(ts->himax_0f_update_wq);
#endif
#ifdef HX_AUTO_UPDATE_FW
	cancel_delayed_work_sync(&ts->work_update);
	destroy_workqueue(ts->himax_update_wq);
#endif

#ifdef HX_TP_PROC_DIAG
	cancel_delayed_work_sync(&ts->himax_diag_delay_wrok);
	destroy_workqueue(ts->himax_diag_wq);
#endif

#ifdef HX_TP_PROC_GUEST_INFO
	destroy_workqueue(ts->guest_info_wq);
#endif

#ifdef HX_TP_PROC_FLASH_DUMP
	destroy_workqueue(ts->flash_wq);
#endif
	if (gpio_is_valid(ts->pdata->gpio_irq))
		gpio_free(ts->pdata->gpio_irq);
#ifdef HX_RST_PIN_FUNC
	if (gpio_is_valid(ts->pdata->gpio_reset))
		gpio_free(ts->pdata->gpio_reset);
#endif

	kfree(hx_touch_data);
	kfree(ic_data);
	kfree(ts->pdata);
	kfree(ts);
	probe_fail_flag = 0;

	return 0;
}
#ifdef HX_INSPECT_LPWUG_TEST
void himax_press_powerkey(bool key_status)
{
	if (key_status == FAKE_POWER_KEY_SEND) {
		if (key_status == false)
			I("Already suspend!\n");
		else
			I("Already resume!\n");

		return;
	}

	I(" %s POWER KEY event %x press\n", __func__, KEY_POWER);
	input_report_key(private_ts->input_dev, KEY_POWER, 1);
	input_sync(private_ts->input_dev);

	I(" %s POWER KEY event %x release\n", __func__, KEY_POWER);
	input_report_key(private_ts->input_dev, KEY_POWER, 0);
	input_sync(private_ts->input_dev);

	FAKE_POWER_KEY_SEND = key_status;
}
#endif

int himax_chip_common_suspend(struct himax_ts_data *ts)
{
	int ret;
#ifdef HX_CHIP_STATUS_MONITOR
	int t = 0;
#endif

	if (ts->suspended) {
		I("%s: Already suspended. Skipped.\n", __func__);
		return 0;
	}
	ts->suspended = true;
	I("%s: enter\n", __func__);

#ifdef HX_TP_PROC_FLASH_DUMP
	if (getFlashDumpGoing()) {
		I("[himax] %s: Flash dump is going, reject suspend\n",
		  __func__);
		return 0;
	}
#endif

#ifdef HX_TP_PROC_GUEST_INFO
	if (himax_guest_info_get_status()) {
		I("[himax] %s: GUEST INFO dump is going, reject suspend\n",
		  __func__);
		return 0;
	}
#endif

#ifdef HX_CHIP_STATUS_MONITOR
	if (g_chip_monitor_data->HX_ON_HAND_SHAKING) /* chip on hand */
						     /* shaking,wait hand */
						     /* shaking */
	{
		for (t = 0; t < 100; t++) {
			if (g_chip_monitor_data->HX_ON_HAND_SHAKING ==
			    0) { /* chip on hand shaking end */

				I("%s:HX_ON_HAND_SHAKING OK check %d times\n",
				  __func__, t);
				break;
			}
			msleep(20);
		}
		if (t == 100) {
			E("%s:HX_ON_HAND_SHAKING timeout reject suspend\n",
			  __func__);
			return 0;
		}
	}
	g_chip_monitor_data->HX_CHIP_MONITOR_EN = 0;
	g_chip_monitor_data->HX_CHIP_POLLING_COUNT = 0;
	cancel_delayed_work_sync(&ts->himax_chip_monitor);
#endif

#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) ||                      \
	defined(HX_USB_DETECT_GLOBAL)
#ifndef HX_RESUME_SEND_CMD
	himax_resend_cmd_func(ts->suspended);
#endif
#endif

#ifdef HX_INSPECT_LPWUG_TEST
	FAKE_POWER_KEY_SEND = false;
#endif

#ifdef HX_SMART_WAKEUP
	if (ts->SMWP_enable) {
		atomic_set(&ts->suspend_mode, 1);
		ts->pre_finger_mask = 0;
		FAKE_POWER_KEY_SEND = false;
		I("[himax] %s: SMART_WAKEUP enable, reject suspend\n",
		  __func__);
		return 0;
	}
#endif

	himax_int_enable(ts->client->irq, 0);

	himax_suspend_ic_action(ts->client);

	if (!ts->use_irq) {
		ret = cancel_work_sync(&ts->work);
		if (ret)
			himax_int_enable(ts->client->irq, 1);
	}

	/* ts->first_pressed = 0; */
	atomic_set(&ts->suspend_mode, 1);
	ts->pre_finger_mask = 0;
	if (ts->pdata->powerOff3V3 && ts->pdata->power)
		ts->pdata->power(0);
	I("%s: END\n", __func__);
	return 0;
}

int himax_chip_common_resume(struct himax_ts_data *ts)
{

#ifdef HX_CHIP_STATUS_MONITOR
	int t = 0;
#endif

	I("%s: enter\n", __func__);
	if (ts->suspended == false) {
		I("%s: It had entered resume,skip this step\n", __func__);
		return 0;
	}
	ts->suspended = false;

	atomic_set(&ts->suspend_mode, 0);

	if (ts->pdata->powerOff3V3 && ts->pdata->power)
		ts->pdata->power(1);

#ifdef HX_CHIP_STATUS_MONITOR
	if (g_chip_monitor_data->HX_ON_HAND_SHAKING) /* chip on hand */
						     /* shaking,wait hand */
						     /* shaking */
	{
		for (t = 0; t < 100; t++) {
			if (g_chip_monitor_data->HX_ON_HAND_SHAKING ==
			    0) { /* chip on hand shaking end */

				I("%s:HX_ON_HAND_SHAKING OK check %d times\n",
				  __func__, t);
				break;
			}
			msleep(20);
		}
		if (t == 100) {
			E("%s:HX_ON_HAND_SHAKING timeout reject resume\n",
			  __func__);
			return 0;
		}
	}
#endif

#if defined(HX_SMART_WAKEUP) || defined(HX_HIGH_SENSE) ||                      \
	defined(HX_USB_DETECT_GLOBAL)
	himax_resend_cmd_func(ts->suspended);
#elif defined(HX_RESUME_HW_RESET)
	himax_ic_reset(false, false);
#endif

#ifdef HX_INSPECT_LPWUG_TEST
	FAKE_POWER_KEY_SEND = true;
#endif

	himax_report_all_leave(ts);

	himax_resume_ic_action(ts->client);

	himax_int_enable(ts->client->irq, 1);

#ifdef HX_CHIP_STATUS_MONITOR
	g_chip_monitor_data->HX_CHIP_POLLING_COUNT = 0;
	g_chip_monitor_data->HX_CHIP_MONITOR_EN = 1;
	queue_delayed_work(ts->himax_chip_monitor_wq, &ts->himax_chip_monitor,
			   g_chip_monitor_data->HX_POLLING_TIMER *
				   HZ); /* for ESD solution */
#endif
	I("%s: END\n", __func__);
	return 0;
}
