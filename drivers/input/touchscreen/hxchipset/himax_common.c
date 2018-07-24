	/* Himax Android Driver Sample Code for Himax chipset
*
* Copyright (C) 2015 Himax Corporation.
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

#include "himax_common.h"
#include "himax_ic.h"

#define SUPPORT_FINGER_DATA_CHECKSUM 0x0F
#define TS_WAKE_LOCK_TIMEOUT		(2 * HZ)
#define FRAME_COUNT 5

#if defined(HX_AUTO_UPDATE_FW)
	char *i_CTPM_firmware_name = "HX83100_Amber_0B01_030E.bin";
	const struct firmware *i_CTPM_FW = NULL;
#endif

/*static int tpd_keys_local[HX_KEY_MAX_COUNT] = HX_KEY_ARRAY;
// for Virtual key array */

struct himax_ts_data *private_ts;
struct himax_ic_data *ic_data;

static int HX_TOUCH_INFO_POINT_CNT;

static uint8_t vk_press = 0x00;
static uint8_t AA_press = 0x00;
static uint8_t EN_NoiseFilter = 0x00;
static int hx_point_num; /*for himax_ts_work_func use*/
static int p_point_num = 0xFFFF;
static int tpd_key = 0x00;
static int tpd_key_old = 0x00;
static int probe_fail_flag;
static bool config_load;
static struct himax_config *config_selected;

/*static int iref_number = 11;*/
/*static bool iref_found = false;*/


#if defined(CONFIG_FB)
int fb_notifier_callback(struct notifier_block *self,
				 unsigned long event, void *data);
#elif defined(CONFIG_HAS_EARLYSUSPEND)
static void himax_ts_early_suspend(struct early_suspend *h);
static void himax_ts_late_resume(struct early_suspend *h);
#endif

int himax_input_register(struct himax_ts_data *ts)
{
	int ret;

	ts->input_dev = input_allocate_device();
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		E("%s: Failed to allocate input device\n", __func__);
		return ret;
	}
	ts->input_dev->name = "himax-touchscreen";

	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);

	set_bit(KEY_BACK, ts->input_dev->keybit);
	set_bit(KEY_HOME, ts->input_dev->keybit);
	set_bit(KEY_MENU, ts->input_dev->keybit);
	set_bit(KEY_SEARCH, ts->input_dev->keybit);
#if defined(HX_SMART_WAKEUP)
	set_bit(KEY_POWER, ts->input_dev->keybit);
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

	set_bit(KEY_F10, ts->input_dev->keybit);

	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	if (ts->protocol_type == PROTOCOL_TYPE_A) {
		/*ts->input_dev->mtsize = ts->nFinger_support;*/
		input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID,
		0, 3, 0, 0);
	} else {/* PROTOCOL_TYPE_B */
		set_bit(MT_TOOL_FINGER, ts->input_dev->keybit);
		input_mt_init_slots(ts->input_dev, ts->nFinger_support, 0);
	}

	I("input_set_abs_params: mix_x %d, max_x %d, min_y %d, max_y %d\n",
		ts->pdata->abs_x_min, ts->pdata->abs_x_max,
		ts->pdata->abs_y_min, ts->pdata->abs_y_max);

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X,
	ts->pdata->abs_x_min, ts->pdata->abs_x_max, ts->pdata->abs_x_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y,
	ts->pdata->abs_y_min, ts->pdata->abs_y_max, ts->pdata->abs_y_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR,
	ts->pdata->abs_pressure_min, ts->pdata->abs_pressure_max,
	ts->pdata->abs_pressure_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE,
	ts->pdata->abs_pressure_min, ts->pdata->abs_pressure_max,
	ts->pdata->abs_pressure_fuzz, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR,
	ts->pdata->abs_width_min, ts->pdata->abs_width_max,
	ts->pdata->abs_pressure_fuzz, 0);

/*input_set_abs_params(ts->input_dev, ABS_MT_AMPLITUDE, 0,
((ts->pdata->abs_pressure_max << 16) | ts->pdata->abs_width_max), 0, 0);*/
/*input_set_abs_params(ts->input_dev, ABS_MT_POSITION, 0,
(BIT(31) | (ts->pdata->abs_x_max << 16) | ts->pdata->abs_y_max), 0, 0);*/

	return input_register_device(ts->input_dev);
}

static void calcDataSize(uint8_t finger_num)
{
	struct himax_ts_data *ts_data = private_ts;

	ts_data->coord_data_size = 4 * finger_num;
	ts_data->area_data_size = ((finger_num / 4) +
	(finger_num % 4 ? 1 : 0)) * 4;
	ts_data->raw_data_frame_size = 128 -
	ts_data->coord_data_size -
	ts_data->area_data_size - 4 - 4 - 1;

	ts_data->raw_data_nframes =
	((uint32_t)ts_data->x_channel *
	ts_data->y_channel + ts_data->x_channel + ts_data->y_channel) /
	ts_data->raw_data_frame_size + (((uint32_t)ts_data->x_channel *
	ts_data->y_channel + ts_data->x_channel + ts_data->y_channel) %
	ts_data->raw_data_frame_size) ? 1 : 0;

	I("%s: coord_data_size: %d, area_data_size:%d",
	__func__, ts_data->coord_data_size, ts_data->area_data_size);
	I("raw_data_frame_size:%d, raw_data_nframes:%d",
	ts_data->raw_data_frame_size, ts_data->raw_data_nframes);
}

static void calculate_point_number(void)
{
	HX_TOUCH_INFO_POINT_CNT = ic_data->HX_MAX_PT * 4;

	if ((ic_data->HX_MAX_PT % 4) == 0)
		HX_TOUCH_INFO_POINT_CNT += (ic_data->HX_MAX_PT / 4) * 4;
	else
		HX_TOUCH_INFO_POINT_CNT += ((ic_data->HX_MAX_PT / 4) + 1) * 4;
}

/*#if 0*/
#ifdef HX_EN_CHECK_PATCH
static int himax_read_Sensor_ID(struct i2c_client *client)
{
	uint8_t val_high[1], val_low[1], ID0 = 0, ID1 = 0;
	char data[3];
	const int normalRetry = 10;
	int sensor_id;

	data[0] = 0x56; data[1] = 0x02;
	data[2] = 0x02;/*ID pin PULL High*/
	i2c_himax_master_write(client, &data[0], 3, normalRetry);
	usleep(1000);

	/*read id pin high*/
	i2c_himax_read(client, 0x57, val_high, 1, normalRetry);

	data[0] = 0x56; data[1] = 0x01;
	data[2] = 0x01;/*ID pin PULL Low*/
	i2c_himax_master_write(client, &data[0], 3, normalRetry);
	usleep(1000);

	/*read id pin low*/
	i2c_himax_read(client, 0x57, val_low, 1, normalRetry);

	if ((val_high[0] & 0x01) == 0)
		ID0 = 0x02;/*GND*/
	else if ((val_low[0] & 0x01) == 0)
		ID0 = 0x01;/*Floating*/
	else
		ID0 = 0x04;/*VCC*/

	if ((val_high[0] & 0x02) == 0)
		ID1 = 0x02;/*GND*/
	else if ((val_low[0] & 0x02) == 0)
		ID1 = 0x01;/*Floating*/
	else
		ID1 = 0x04;/*VCC*/
	if ((ID0 == 0x04) && (ID1 != 0x04)) {
			data[0] = 0x56; data[1] = 0x02;
			data[2] = 0x01;/*ID pin PULL High,Low*/
			i2c_himax_master_write(client,
			&data[0], 3, normalRetry);
			usleep(1000);

	} else if ((ID0 != 0x04) && (ID1 == 0x04)) {
			data[0] = 0x56; data[1] = 0x01;
			data[2] = 0x02;/*ID pin PULL Low,High*/
			i2c_himax_master_write(client,
			&data[0], 3, normalRetry);
			usleep(1000);

	} else if ((ID0 == 0x04) && (ID1 == 0x04)) {
			data[0] = 0x56; data[1] = 0x02;
			data[2] = 0x02;/*ID pin PULL High,High*/
			i2c_himax_master_write(client,
			&data[0], 3, normalRetry);
			usleep(1000);

	}
	sensor_id = (ID1<<4)|ID0;

	data[0] = 0xE4; data[1] = sensor_id;
	i2c_himax_master_write(client,
	&data[0], 2, normalRetry);/*Write to MCU*/
	usleep(1000);

	return sensor_id;

}
#endif
static void himax_power_on_initCMD(struct i2c_client *client)
{
	I("%s:\n", __func__);
	himax_touch_information(client);
    /*himax_sense_on(private_ts->client, 0x01);//1=Flash, 0=SRAM */
}

#ifdef HX_AUTO_UPDATE_FW
static int i_update_FW(void)
{
	int upgrade_times = 0;
	int fullFileLength = 0;
	int i_FW_VER = 0, i_CFG_VER = 0;
	int ret = -1, result = 0;
	/*uint8_t tmp_addr[4];*/
	/*uint8_t tmp_data[4];*/
	int CRC_from_FW = 0;
	int CRC_Check_result = 0;

	ret = himax_load_CRC_bin_file(private_ts->client);
	if (ret < 0) {
		E("%s: himax_load_CRC_bin_file fail Error Code=%d.\n",
		__func__, ret);
		ret = -1;
		return ret;
	}
	I("file name = %s\n", i_CTPM_firmware_name);
	ret = request_firmware(&i_CTPM_FW,
	i_CTPM_firmware_name, private_ts->dev);
	if (ret < 0) {
		E("%s,fail in line%d error code=%d\n",
		__func__, __LINE__, ret);
		ret = -2;
		return ret;
	}

	if (i_CTPM_FW == NULL) {
		I("%s: i_CTPM_FW = NULL\n", __func__);
		ret = -3;
		return ret;
	}
	fullFileLength = i_CTPM_FW->size;

	i_FW_VER = i_CTPM_FW->data[FW_VER_MAJ_FLASH_ADDR]<<8
	| i_CTPM_FW->data[FW_VER_MIN_FLASH_ADDR];
	i_CFG_VER = i_CTPM_FW->data[CFG_VER_MAJ_FLASH_ADDR]<<8
	| i_CTPM_FW->data[CFG_VER_MIN_FLASH_ADDR];

	I("%s: i_fullFileLength = %d\n", __func__, fullFileLength);

	himax_sense_off(private_ts->client);
	msleep(500);

	CRC_from_FW = himax_check_CRC(private_ts->client, fw_image_64k);
	CRC_Check_result =
	Calculate_CRC_with_AP((unsigned char *)i_CTPM_FW->data,
	CRC_from_FW, fw_image_64k);
	I("%s: Check sum result = %d\n", __func__, CRC_Check_result);
	/*I("%s: ic_data->vendor_fw_ver = %X, i_FW_VER = %X,\n",
	__func__, ic_data->vendor_fw_ver, i_FW_VER);*/
	/*I("%s: ic_data->vendor_config_ver = %X, i_CFG_VER = %X,\n",
	__func__, ic_data->vendor_config_ver, i_CFG_VER);*/

	if ((CRC_Check_result == 0) ||
	(ic_data->vendor_fw_ver < i_FW_VER) ||
	(ic_data->vendor_config_ver < i_CFG_VER)) {
		himax_int_enable(private_ts->client->irq, 0);
update_retry:
		if (fullFileLength == FW_SIZE_60k) {
			ret = fts_ctpm_fw_upgrade_with_sys_fs_60k
			(private_ts->client,
			(unsigned char *)i_CTPM_FW->data,
			fullFileLength, false);
		} else if (fullFileLength == FW_SIZE_64k) {
			ret = fts_ctpm_fw_upgrade_with_sys_fs_64k
			(private_ts->client,
			(unsigned char *)i_CTPM_FW->data,
			fullFileLength, false);
		} else if (fullFileLength == FW_SIZE_124k) {
			ret = fts_ctpm_fw_upgrade_with_sys_fs_124k
			(private_ts->client,
			(unsigned char *)i_CTPM_FW->data,
			fullFileLength, false);
		} else if (fullFileLength == FW_SIZE_128k) {
			ret = fts_ctpm_fw_upgrade_with_sys_fs_128k
			(private_ts->client,
			(unsigned char *)i_CTPM_FW->data,
			fullFileLength, false);
		}
		if (ret == 0) {
			upgrade_times++;
			E("%s: TP upgrade error, upgrade_times = %d\n",
			__func__, upgrade_times);
			if (upgrade_times < 3)
				goto update_retry;
			else {
				himax_sense_on(private_ts->client, 0x01);
				msleep(120);
#ifdef HX_ESD_WORKAROUND
				HX_ESD_RESET_ACTIVATE = 1;
#endif
				result = -1;/*upgrade fail*/
			}
		} else if (ret == 1) {
			/*
			// 1. Set DDREG_Req = 1 (0x9000_0020 = 0x0000_0001)
					(Lock register R/W from driver)
					tmp_addr[3] = 0x90; tmp_addr[2] = 0x00;
					tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
					tmp_data[3] = 0x00; tmp_data[2] = 0x00;
					tmp_data[1] = 0x00; tmp_data[0] = 0x01;
					himax_register_write(private_ts->client,
					tmp_addr, 1, tmp_data);

			// 2. Write driver initial code condition
			//write value from AHB I2C:0x8001_C603 = 0x000000FF
					tmp_addr[3] = 0x80; tmp_addr[2] = 0x01;
					tmp_addr[1] = 0xC6; tmp_addr[0] = 0x03;
					tmp_data[3] = 0x00; tmp_data[2] = 0x00;
					tmp_data[1] = 0x00; tmp_data[0] = 0xFF;
					himax_register_write(private_ts->client,
					tmp_addr, 1, tmp_data);

			// 1. Set DDREG_Req = 0(0x9000_0020 = 0x0000_0001)
					(Lock register R/W from driver)
					tmp_addr[3] = 0x90; tmp_addr[2] = 0x00;
					tmp_addr[1] = 0x00; tmp_addr[0] = 0x20;
					tmp_data[3] = 0x00; tmp_data[2] = 0x00;
					tmp_data[1] = 0x00; tmp_data[0] = 0x00;
					himax_register_write(private_ts->client,
					tmp_addr, 1, tmp_data);
			*/
			himax_sense_on(private_ts->client, 0x01);
			msleep(120);
#ifdef HX_ESD_WORKAROUND
			HX_ESD_RESET_ACTIVATE = 1;
#endif

			ic_data->vendor_fw_ver = i_FW_VER;
			ic_data->vendor_config_ver = i_CFG_VER;
			result = 1;/*upgrade success*/
			I("%s: TP upgrade OK\n", __func__);
		}

		himax_int_enable(private_ts->client->irq, 1);
		return result;

	} else {
		himax_sense_on(private_ts->client, 0x01);
		return 0;/*NO upgrade*/
	}
}
#endif

#ifdef HX_RST_PIN_FUNC
void himax_HW_reset(uint8_t loadconfig, uint8_t int_off)
{
	struct himax_ts_data *ts = private_ts;
	int ret = 0;

	return;
	if (ts->rst_gpio) {
		if (int_off) {
			if (ts->use_irq)
				himax_int_enable(private_ts->client->irq, 0);
			else {
				hrtimer_cancel(&ts->timer);
				ret = cancel_work_sync(&ts->work);
			}
		}

		I("%s: Now reset the Touch chip.\n", __func__);

		himax_rst_gpio_set(ts->rst_gpio, 0);
		msleep(20);
		himax_rst_gpio_set(ts->rst_gpio, 1);
		msleep(20);

		if (loadconfig)
			himax_loadSensorConfig(private_ts->client,
			private_ts->pdata);

		if (int_off) {
			if (ts->use_irq)
				himax_int_enable(private_ts->client->irq, 1);
			else
				hrtimer_start(&ts->timer,
				ktime_set(1, 0), HRTIMER_MODE_REL);
		}
	}
}
#endif

int himax_loadSensorConfig(struct i2c_client *client,
struct himax_i2c_platform_data *pdata)
{
	int err = -1;

	if (!client) {
		E("%s: Necessary parameters client are null!\n", __func__);
		return err;
	}
	if (config_load == false) {
		config_selected = kzalloc(sizeof(*config_selected), GFP_KERNEL);
		if (config_selected == NULL) {
			E("%s: alloc config_selected fail!\n", __func__);
			return err;
		}
	}
	himax_power_on_initCMD(client);

	himax_int_enable(client->irq, 0);
	himax_read_FW_ver(client);
#ifdef HX_RST_PIN_FUNC
	himax_HW_reset(true, false);
#endif
	himax_int_enable(client->irq, 1);
	I("FW_VER : %X\n", ic_data->vendor_fw_ver);

	ic_data->vendor_sensor_id = 0x2602;
	I("sensor_id=%x.\n", ic_data->vendor_sensor_id);

	himax_sense_on(private_ts->client, 0x01);/*1=Flash, 0=SRAM*/
	msleep(120);
#ifdef HX_ESD_WORKAROUND
	HX_ESD_RESET_ACTIVATE = 1;
#endif
	I("%s: initialization complete\n", __func__);

	return 1;
}

#ifdef HX_ESD_WORKAROUND
void ESD_HW_REST(void)
{
	I("START_Himax TP: ESD - Reset\n");

	HX_report_ESD_event();
	ESD_00_counter = 0;
	ESD_00_Flag = 0;
    /*************************************/
	if (private_ts->protocol_type == PROTOCOL_TYPE_A)
		input_mt_sync(private_ts->input_dev);
	input_report_key(private_ts->input_dev, BTN_TOUCH, 0);
	input_sync(private_ts->input_dev);
	/*************************************/

	I("END_Himax TP: ESD - Reset\n");
}
#endif
#ifdef HX_HIGH_SENSE
void himax_set_HSEN_func(struct i2c_client *client, uint8_t HSEN_enable)
{
	uint8_t tmp_data[4];

	if (HSEN_enable) {
		I(" %s in", __func__);
HSEN_bit_retry:
		himax_set_HSEN_enable(client, HSEN_enable);
		msleep(20);
		himax_get_HSEN_enable(client, tmp_data);
		I("%s: Read HSEN bit data[0]=%x data[1]=%x",
		__func__, tmp_data[0], tmp_data[1]);
		I("data[2]=%x data[3]=%x\n",
		tmp_data[2], tmp_data[3]);

		if (tmp_data[0] != 0x01) {
			I("%s: retry HSEN bit write data[0]=%x\n",
			__func__, tmp_data[0]);
			goto HSEN_bit_retry;
		}
	}
}

static void himax_HSEN_func(struct work_struct *work)
{
	struct himax_ts_data *ts =
	container_of(work, struct himax_ts_data, hsen_work.work);

	himax_set_HSEN_func(ts->client, ts->HSEN_enable);
}

#endif

#ifdef HX_SMART_WAKEUP
#ifdef HX_GESTURE_TRACK
static void gest_pt_log_coordinate(int rx, int tx)
{
	/*driver report x y with range 0 - 255*/
	/* And we scale it up to x/y coordinates*/
	gest_pt_x[gest_pt_cnt] = rx * (ic_data->HX_X_RES) / 255;
	gest_pt_y[gest_pt_cnt] = tx * (ic_data->HX_Y_RES) / 255;
}
#endif
static int himax_parse_wake_event(struct himax_ts_data *ts)
{
	uint8_t buf[64];
	unsigned char check_sum_cal = 0;
#ifdef HX_GESTURE_TRACK
	int tmp_max_x = 0x00, tmp_min_x = 0xFFFF,
	tmp_max_y = 0x00, tmp_min_y = 0xFFFF;
	int gest_len;
#endif
	int i = 0, check_FC = 0, gesture_flag = 0;

	himax_burst_enable(ts->client, 0);
	himax_read_event_stack(ts->client, buf, 56);

	for (i = 0 ; i < GEST_PTLG_ID_LEN ; i++) {
		if (check_FC == 0) {
			if ((buf[0] != 0x00) &&
			((buf[0] <= 0x0F) || (buf[0] == 0x80))) {
				check_FC = 1;
				gesture_flag = buf[i];
			} else {
				check_FC = 0;
				I("ID START at %x,value = %x skip event\n",
				i, buf[i]);
				break;
			}
		} else {
			if (buf[i] != gesture_flag) {
				check_FC = 0;
				I("ID NOT same %x != %x So STOP parse event\n",
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
	if (buf[GEST_PTLG_ID_LEN] != GEST_PTLG_HDR_ID1
	|| buf[GEST_PTLG_ID_LEN+1] != GEST_PTLG_HDR_ID2)
		return 0;
	for (i = 0 ; i < (GEST_PTLG_ID_LEN + GEST_PTLG_HDR_LEN) ; i++) {
		I("P[%x]=0x%2.2X\n", i, buf[i]);
		I("checksum=0x%2.2X\n", check_sum_cal);
		check_sum_cal += buf[i];
	}
	if ((check_sum_cal != 0x00)) {
		I(" %s : check_sum_cal: 0x%02X\n", __func__ , check_sum_cal);
		return 0;
	}
#ifdef HX_GESTURE_TRACK
	if (buf[GEST_PTLG_ID_LEN] == GEST_PTLG_HDR_ID1
	&& buf[GEST_PTLG_ID_LEN+1] == GEST_PTLG_HDR_ID2) {
		gest_len = buf[GEST_PTLG_ID_LEN + 2];
		I("gest_len = %d ", gest_len);
		i = 0;
		gest_pt_cnt = 0;
		I("gest doornidate start\n %s", __func__);
		while (i < (gest_len + 1) / 2) {
			gest_pt_log_coordinate
			(buf[GEST_PTLG_ID_LEN + 4 + i * 2],
			buf[GEST_PTLG_ID_LEN + 4 + i * 2 + 1]);
			i++;

			I("gest_pt_x[%d]=%d\n",
			gest_pt_cnt, gest_pt_x[gest_pt_cnt]);
			I("gest_pt_y[%d]=%d\n",
			gest_pt_cnt, gest_pt_y[gest_pt_cnt]);

			gest_pt_cnt += 1;
		}
		if (gest_pt_cnt) {
			for (i = 0 ; i < gest_pt_cnt ; i++) {
				if (tmp_max_x < gest_pt_x[i])
					tmp_max_x = gest_pt_x[i];
				if (tmp_min_x > gest_pt_x[i])
					tmp_min_x = gest_pt_x[i];
				if (tmp_max_y < gest_pt_y[i])
					tmp_max_y = gest_pt_y[i];
				if (tmp_min_y > gest_pt_y[i])
					tmp_min_y = gest_pt_y[i];
			}
			I("gest_point x_min= %d, x_max= %d\n",
			tmp_min_x, tmp_max_x);
			I("y_min= %d, y_max= %d\n",
			tmp_min_y, tmp_max_y);
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
			/*gest_up_x*/
			gn_gesture_coor[8] = gest_mid_x;
			/*gest_up_y*/
			gn_gesture_coor[9] = gest_mid_y - gest_height / 2;
			/*gest_down_x*/
			gn_gesture_coor[10] = gest_mid_x;
			/*gest_down_y*/
			gn_gesture_coor[11] = gest_mid_y + gest_height / 2;
			/*gest_left_x*/
			gn_gesture_coor[12] = gest_mid_x - gest_width / 2;
			/*gest_left_y*/
			gn_gesture_coor[13] = gest_mid_y;
			/*gest_right_x*/
			gn_gesture_coor[14] = gest_mid_x + gest_width / 2;
			/*gest_right_y*/
			gn_gesture_coor[15] = gest_mid_y;

		}

	}
#endif
	if (gesture_flag != 0x80) {
		if (!ts->gesture_cust_en[gesture_flag]) {
			I("%s NOT report customer key\n ", __func__);
			return 0;/*NOT report customer key*/
		}
	} else {
		if (!ts->gesture_cust_en[0]) {
			I("%s NOT report report double click\n", __func__);
			return 0;/*NOT report power key*/
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
		I(" %s SMART WAKEUP KEY event %x press\n",
		__func__, KEY_EVENT);
		input_report_key(private_ts->input_dev, KEY_EVENT, 1);
		input_sync(private_ts->input_dev);
		/*msleep(100);*/
		I(" %s SMART WAKEUP KEY event %x release\n",
		__func__, KEY_EVENT);
		input_report_key(private_ts->input_dev, KEY_EVENT, 0);
		input_sync(private_ts->input_dev);
		FAKE_POWER_KEY_SEND = true;
#ifdef HX_GESTURE_TRACK
		I("gest_start_x= %d, gest_start_y= %d\n",
		gest_start_x, gest_start_y);
		I("gest_end_x= %d, gest_end_y= %d\n",
		gest_end_x, gest_end_y);
		I("gest_width= %d, gest_height= %d\n",
		gest_width, gest_height);
		I("gest_mid_x= %d, gest_mid_y= %d\n",
		gest_mid_x, gest_mid_y);
		I("gest_up_x= %d, gest_up_y= %d\n",
		gn_gesture_coor[8], gn_gesture_coor[9]);
		I("gest_down_x= %d, gest_down_y= %d\n",
		gn_gesture_coor[10], gn_gesture_coor[11]);
		I("gest_left_x= %d, gest_left_y= %d\n",
		gn_gesture_coor[12], gn_gesture_coor[13]);
		I("gest_right_x= %d, gest_right_y= %d\n",
		gn_gesture_coor[14], gn_gesture_coor[15]);
#endif
	}
}

#endif
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
					x_position = (ts->button[0].x_range_min
					+ ts->button[0].x_range_max) / 2;
					y_position = (ts->button[0].y_range_min
					+ ts->button[0].y_range_max) / 2;
				}
				if (ts->protocol_type == PROTOCOL_TYPE_A) {
					input_report_abs(ts->input_dev,
					ABS_MT_TRACKING_ID, 0);
					input_report_abs(ts->input_dev,
					ABS_MT_TOUCH_MAJOR, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_WIDTH_MAJOR, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_PRESSURE, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_POSITION_X, x_position);
					input_report_abs(ts->input_dev,
					ABS_MT_POSITION_Y, y_position);
					input_mt_sync(ts->input_dev);
				} else if (ts->protocol_type
					== PROTOCOL_TYPE_B) {
					input_mt_slot(ts->input_dev, 0);

					input_mt_report_slot_state
					(ts->input_dev, MT_TOOL_FINGER, 1);

					input_report_abs(ts->input_dev,
					ABS_MT_TOUCH_MAJOR, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_WIDTH_MAJOR, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_PRESSURE, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_POSITION_X, x_position);
					input_report_abs(ts->input_dev,
					ABS_MT_POSITION_Y, y_position);
				}
			} else
				input_report_key(ts->input_dev, KEY_BACK, 1);
		} else if (tp_key_index == 0x02) {
			vk_press = 1;
			I("home key pressed\n");
			if (ts->pdata->virtual_key) {
				if (ts->button[1].index) {
					x_position = (ts->button[1].x_range_min
					+ ts->button[1].x_range_max) / 2;
					y_position = (ts->button[1].y_range_min
					+ ts->button[1].y_range_max) / 2;
				}
				if (ts->protocol_type == PROTOCOL_TYPE_A) {
					input_report_abs(ts->input_dev,
					ABS_MT_TRACKING_ID, 0);
					input_report_abs(ts->input_dev,
					ABS_MT_TOUCH_MAJOR, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_WIDTH_MAJOR, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_PRESSURE, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_POSITION_X, x_position);
					input_report_abs(ts->input_dev,
					ABS_MT_POSITION_Y, y_position);
					input_mt_sync(ts->input_dev);
				} else if (ts->protocol_type
				== PROTOCOL_TYPE_B) {
					input_mt_slot(ts->input_dev, 0);

					input_mt_report_slot_state
					(ts->input_dev, MT_TOOL_FINGER, 1);

					input_report_abs(ts->input_dev,
					ABS_MT_TOUCH_MAJOR, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_WIDTH_MAJOR, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_PRESSURE, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_POSITION_X, x_position);
					input_report_abs(ts->input_dev,
					ABS_MT_POSITION_Y, y_position);
				}
			} else
				input_report_key(ts->input_dev, KEY_HOME, 1);
		} else if (tp_key_index == 0x04) {
			vk_press = 1;
			I("APP_switch key pressed\n");
			if (ts->pdata->virtual_key) {
				if (ts->button[2].index) {
					x_position = (ts->button[2].x_range_min
					+ ts->button[2].x_range_max) / 2;
					y_position = (ts->button[2].y_range_min
					+ ts->button[2].y_range_max) / 2;
				}
				if (ts->protocol_type == PROTOCOL_TYPE_A) {
					input_report_abs(ts->input_dev,
					ABS_MT_TRACKING_ID, 0);
					input_report_abs(ts->input_dev,
					ABS_MT_TOUCH_MAJOR, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_WIDTH_MAJOR, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_PRESSURE, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_POSITION_X, x_position);
					input_report_abs(ts->input_dev,
					ABS_MT_POSITION_Y, y_position);
					input_mt_sync(ts->input_dev);
				} else if (ts->protocol_type ==
				PROTOCOL_TYPE_B) {
					input_mt_slot(ts->input_dev, 0);

					input_mt_report_slot_state
					(ts->input_dev, MT_TOOL_FINGER, 1);

					input_report_abs(ts->input_dev,
					ABS_MT_TOUCH_MAJOR, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_WIDTH_MAJOR, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_PRESSURE, 100);
					input_report_abs(ts->input_dev,
					ABS_MT_POSITION_X, x_position);
					input_report_abs(ts->input_dev,
					ABS_MT_POSITION_Y, y_position);
				}
			} else
				input_report_key(ts->input_dev, KEY_F10, 1);
		}
		input_sync(ts->input_dev);
	} else {/*tp_key_index =0x00*/
		I("virtual key released\n");
		vk_press = 0;
		if (ts->protocol_type == PROTOCOL_TYPE_A) {
			input_mt_sync(ts->input_dev);
		} else if (ts->protocol_type == PROTOCOL_TYPE_B) {
			input_mt_slot(ts->input_dev, 0);
			input_mt_report_slot_state(ts->input_dev,
			MT_TOOL_FINGER, 0);
		}
		input_report_key(ts->input_dev, KEY_BACK, 0);
		input_report_key(ts->input_dev, KEY_HOME, 0);
		input_report_key(ts->input_dev, KEY_F10, 0);
		input_sync(ts->input_dev);
	}
}

void himax_ts_work(struct himax_ts_data *ts)
{
	int ret = 0;
	uint8_t  finger_num, hw_reset_check[2];
	uint8_t buf[128];
	uint8_t finger_on = 0;
	int32_t loop_i;
	uint16_t check_sum_cal = 0;
	int raw_cnt_max;
	int raw_cnt_rmd;
	int hx_touch_info_size;
	uint8_t coordInfoSize = ts->coord_data_size + ts->area_data_size + 4;

#ifdef HX_TP_PROC_DIAG
	int16_t *mutual_data;
	int16_t *self_data;
	uint8_t diag_cmd;
	int i;
	int mul_num;
	int self_num;
	int RawDataLen = 0;
	/*coordinate dump start*/
	char coordinate_char[15 + (ic_data->HX_MAX_PT + 5) * 2 * 5 + 2];
	struct timeval t;
	struct tm broken;
	/*coordinate dump end*/
#endif

	memset(buf, 0x00, sizeof(buf));
	memset(hw_reset_check, 0x00, sizeof(hw_reset_check));

	raw_cnt_max = ic_data->HX_MAX_PT / 4;
	raw_cnt_rmd = ic_data->HX_MAX_PT % 4;
#if defined(HX_USB_DETECT2)
	himax_cable_detect_func();
#endif

	if (raw_cnt_rmd != 0x00) { /*more than 4 fingers*/
		RawDataLen = cal_data_len(raw_cnt_rmd,
		ic_data->HX_MAX_PT, raw_cnt_max);
		hx_touch_info_size = (ic_data->HX_MAX_PT + raw_cnt_max + 2) * 4;
	} else { /*less than 4 fingers*/
		RawDataLen = cal_data_len(raw_cnt_rmd,
		ic_data->HX_MAX_PT, raw_cnt_max);
		hx_touch_info_size = (ic_data->HX_MAX_PT + raw_cnt_max + 1) * 4;
	}

#ifdef HX_TP_PROC_DIAG
	diag_cmd = getDiagCommand();
	if (diag_cmd) {
		ret = read_event_stack(ts->client, buf, 128);
	} else {
		if (touch_monitor_stop_flag != 0) {
			ret = read_event_stack(ts->client, buf, 128);
			touch_monitor_stop_flag--;
		} else {
			ret = read_event_stack(ts->client,
					buf, hx_touch_info_size);
		}
	}

	if (!ret)
#else
	if (!read_event_stack(ts->client, buf, hx_touch_info_size))
#endif
		{
			E("%s: can't read data from chip!\n", __func__);
			goto err_workqueue_out;
		}
	post_read_event_stack(ts->client);
#ifdef HX_ESD_WORKAROUND
	for (i = 0; i < hx_touch_info_size; i++) {
		if (buf[i] == 0xED)	{ /*case 1 ESD recovery flow*/
			check_sum_cal = 1;

		} else if (buf[i] == 0x00) {
			ESD_00_Flag = 1;
		} else {
			check_sum_cal = 0;
			ESD_00_counter = 0;
			ESD_00_Flag = 0;
			i = hx_touch_info_size;
			break;
		}
	}
	if (ESD_00_Flag == 1)
		ESD_00_counter++;
	if (ESD_00_counter > 1)
		check_sum_cal = 2;
	if (check_sum_cal == 2 && HX_ESD_RESET_ACTIVATE == 0) {
		I("[HIMAX TP MSG]: ESD event checked - ALL Zero.\n");
		ESD_HW_REST();
		return;
	}
	if (check_sum_cal == 1 && HX_ESD_RESET_ACTIVATE == 0) {
		I("[HIMAX TP MSG]: ESD event checked - ALL 0xED.\n");
		ESD_HW_REST();
		return;
	} else if (HX_ESD_RESET_ACTIVATE) {
#ifdef HX_SMART_WAKEUP
		queue_delayed_work(ts->himax_smwp_wq,
		&ts->smwp_work, msecs_to_jiffies(50));
#endif
#ifdef HX_HIGH_SENSE
		queue_delayed_work(ts->himax_hsen_wq,
		&ts->hsen_work, msecs_to_jiffies(50));
#endif
/*drop 1st interrupts after chip reset*/
		HX_ESD_RESET_ACTIVATE = 0;
		I("[HIMAX TP MSG]:%s: Back from reset,ready to serve.\n",
		__func__);
	}
#endif
	for (loop_i = 0, check_sum_cal = 0;
	loop_i < hx_touch_info_size; loop_i++)
		check_sum_cal += buf[loop_i];

	if ((check_sum_cal % 0x100 != 0)) {
		I("[HIMAX TP MSG] checksum fail : check_sum_cal: 0x%02X\n",
		check_sum_cal);
		return;
	}
	if (ts->debug_log_level & BIT(0)) {
		I("%s: raw data:\n", __func__);
		for (loop_i = 0; loop_i < hx_touch_info_size; loop_i++) {
			I("P %d = 0x%2.2X ", loop_i, buf[loop_i]);
			if (loop_i % 8 == 7)
				I("\n");
		}
	}

	/*touch monitor raw data fetch*/
#ifdef HX_TP_PROC_DIAG
	diag_cmd = getDiagCommand();
	if (diag_cmd >= 1 && diag_cmd <= 6) {
		/*Check 124th byte CRC*/
		if (!diag_check_sum(hx_touch_info_size, buf))
			goto bypass_checksum_failed_packet;

#ifdef HX_TP_PROC_2T2R
		if (Is_2T2R && diag_cmd == 4) {
			mutual_data = getMutualBuffer_2();
			self_data = getSelfBuffer();

			/* initiallize the block number of mutual and self*/
			mul_num = getXChannel_2() * getYChannel_2();

#ifdef HX_EN_SEL_BUTTON
			self_num = getXChannel_2() +
					getYChannel_2() + ic_data->HX_BT_NUM;
#else
			self_num = getXChannel_2() + getYChannel_2();
#endif
		} else
#endif
		{
			mutual_data = getMutualBuffer();
			self_data = getSelfBuffer();

			/* initiallize the block number of mutual and self*/
			mul_num = getXChannel() * getYChannel();

#ifdef HX_EN_SEL_BUTTON
			self_num = getXChannel() +
					getYChannel() + ic_data->HX_BT_NUM;
#else
			self_num = getXChannel() + getYChannel();
#endif
		}

		diag_parse_raw_data(hx_touch_info_size,
		RawDataLen, mul_num, self_num, buf,
		diag_cmd, mutual_data, self_data);

	} else if (diag_cmd == 7) {
		memcpy(&(diag_coor[0]), &buf[0], 128);
	}
	/*coordinate dump start*/
	if (coordinate_dump_enable == 1) {
		for (i = 0; i < (15 + (ic_data->
			HX_MAX_PT + 5) * 2 * 5);
		i++) {
			coordinate_char[i] = 0x20;
		}
		coordinate_char[15 +
		(ic_data->HX_MAX_PT + 5) * 2 * 5] = 0xD;
		coordinate_char[15 +
		(ic_data->HX_MAX_PT + 5) * 2 * 5 + 1] = 0xA;
	}
	/*coordinate dump end*/
bypass_checksum_failed_packet:
#endif
	EN_NoiseFilter = (buf[HX_TOUCH_INFO_POINT_CNT + 2] >> 3);
	/*I("EN_NoiseFilter=%d\n",EN_NoiseFilter);*/
	EN_NoiseFilter = EN_NoiseFilter & 0x01;
	/*I("EN_NoiseFilter2=%d\n",EN_NoiseFilter);*/

#if defined(HX_EN_SEL_BUTTON) || defined(HX_EN_MUT_BUTTON)
	tpd_key = (buf[HX_TOUCH_INFO_POINT_CNT + 2] >> 4);
	if (tpd_key == 0x0F) {/*All (VK+AA)leave*/
		tpd_key = 0x00;
	}
	/*I("[DEBUG] tpd_key:  %x\r\n", tpd_key);*/
#else
	tpd_key = 0x00;
#endif

	p_point_num = hx_point_num;

	if (buf[HX_TOUCH_INFO_POINT_CNT] == 0xff)
		hx_point_num = 0;
	else
		hx_point_num = buf[HX_TOUCH_INFO_POINT_CNT] & 0x0f;

	/* Touch Point information*/
	if ((hx_point_num != 0) && (vk_press == 0x00)) {
		uint16_t old_finger = ts->pre_finger_mask;

		ts->pre_finger_mask = 0;
		finger_num = buf[coordInfoSize - 4] & 0x0F;
		finger_on = 1;
		AA_press = 1;
		for (i = 0; i < ts->nFinger_support; i++) {
			int base = i * 4;
			int x = buf[base] << 8 | buf[base + 1];
			int y = (buf[base + 2] << 8 | buf[base + 3]);
			int w = buf[(ts->nFinger_support * 4) + i];

			if (x >= 0 && x <= ts->pdata->abs_x_max
			&& y >= 0 && y <= ts->pdata->abs_y_max) {
				finger_num--;
				if ((((ts->debug_log_level & BIT(3)) > 0)
				&& (old_finger >> i == 0))
				&& (ts->useScreenRes)) {
					I("status:Screen:F:%02d", i + 1);
					I("Down,X:%d,Y:%d,W:%d,N:%d\n",
					x * ts->widthFactor >> SHIFTBITS,
					y * ts->heightFactor >> SHIFTBITS,
					w, EN_NoiseFilter);
				} else if ((((ts->debug_log_level & BIT(3)) > 0)
				&& (old_finger >> i == 0))
				&& !(ts->useScreenRes)) {
					I("status:Raw:F:%02d", i + 1);
					I("Down,X:%d,Y:%d,W:%d,N:%d\n",
					x, y, w, EN_NoiseFilter);
				}

				if (ts->protocol_type == PROTOCOL_TYPE_B)
					input_mt_slot(ts->input_dev, i);

				input_report_abs(ts->input_dev,
				ABS_MT_TOUCH_MAJOR, w);
				input_report_abs(ts->input_dev,
				ABS_MT_WIDTH_MAJOR, w);
				input_report_abs(ts->input_dev,
				ABS_MT_PRESSURE, w);
				input_report_abs(ts->input_dev,
				ABS_MT_POSITION_X, x);
				input_report_abs(ts->input_dev,
				ABS_MT_POSITION_Y, y);

				if (ts->protocol_type == PROTOCOL_TYPE_A) {
					input_report_abs(ts->input_dev,
					ABS_MT_TRACKING_ID, i);
					input_mt_sync(ts->input_dev);
				} else {
					ts->last_slot = i;
					input_mt_report_slot_state
					(ts->input_dev,
					MT_TOOL_FINGER, 1);
				}

				if (!ts->first_pressed) {
					ts->first_pressed = 1;
					I("S1@%d, %d\n", x, y);
				}

				ts->pre_finger_data[i][0] = x;
				ts->pre_finger_data[i][1] = y;

				if (ts->debug_log_level & BIT(1)) {
					I("Finger %d=> X:%d,Y:%d,W:%d,",
					i + 1, x, y, w);
					I("Z:%d,F:%d,N:%d\n",
					w, i + 1, EN_NoiseFilter);
				}
				ts->pre_finger_mask =
				ts->pre_finger_mask + (1 << i);

			} else {
				if (ts->protocol_type == PROTOCOL_TYPE_B) {
					input_mt_slot(ts->input_dev, i);
					input_mt_report_slot_state
					(ts->input_dev, MT_TOOL_FINGER, 0);
				}
				if (i == 0 && ts->first_pressed == 1) {
					ts->first_pressed = 2;
					I("E1@%d, %d\n",
					ts->pre_finger_data[0][0],
					ts->pre_finger_data[0][1]);
				}
				if ((((ts->debug_log_level & BIT(3)) > 0)
				&& (old_finger >> i == 1))
				&& (ts->useScreenRes)) {
					I("status:Screen:F:%02d,Up,X:%d,Y:%d\n",
					i + 1, ts->pre_finger_data[i][0]
					* ts->widthFactor >> SHIFTBITS,
					ts->pre_finger_data[i][1]
					* ts->heightFactor >> SHIFTBITS);
				} else if ((((ts->debug_log_level & BIT(3)) > 0)
				&& (old_finger >> i == 1))
				&& !(ts->useScreenRes)) {
					I("status:Raw:F:%02d,Up,X:%d,Y:%d\n",
					i + 1, ts->pre_finger_data[i][0],
					ts->pre_finger_data[i][1]);
				}
			}
		}
		input_report_key(ts->input_dev, BTN_TOUCH, finger_on);
		input_sync(ts->input_dev);
	} else if ((hx_point_num != 0)
		&& ((tpd_key_old != 0x00) && (tpd_key == 0x00))) {
		/*temp_x[0] = 0xFFFF;*/
		/*temp_y[0] = 0xFFFF;*/
		/*temp_x[1] = 0xFFFF;*/
		/*temp_y[1] = 0xFFFF;*/
		himax_ts_button_func(tpd_key, ts);
		finger_on = 0;
		input_report_key(ts->input_dev, BTN_TOUCH, finger_on);
		input_sync(ts->input_dev);
	} else if (hx_point_num == 0) {
		if (AA_press) {
			/*leave event*/
			finger_on = 0;
			AA_press = 0;
			if (ts->protocol_type == PROTOCOL_TYPE_A)
				input_mt_sync(ts->input_dev);

			for (i = 0 ; i < ts->nFinger_support ; i++) {
				if ((((ts->pre_finger_mask >> i) & 1) == 1)
				&& (ts->protocol_type == PROTOCOL_TYPE_B)) {
					input_mt_slot(ts->input_dev, i);
					input_mt_report_slot_state
					(ts->input_dev, MT_TOOL_FINGER, 0);
				}
			}
			if (ts->pre_finger_mask > 0) {
				for (i = 0; i < ts->nFinger_support
				&& (ts->debug_log_level & BIT(3)) > 0; i++) {
					if ((((ts->pre_finger_mask
					>> i) & 1) == 1)
					&& (ts->useScreenRes)) {
						I("status:%X,", 0);
						I("Screen:F:%02d,", i + 1);
						I("Up,X:%d,Y:%d\n",
						ts->pre_finger_data[i][0]
						* ts->widthFactor >> SHIFTBITS,
						ts->pre_finger_data[i][1]
						* ts->heightFactor >> SHIFTBITS
						);
					} else if ((((ts->pre_finger_mask
					>> i) & 1) == 1)
					&& !(ts->useScreenRes)) {
						I("status:%X,", 0);
						I("Screen:F:%02d,", i + 1);
						I("Up,X:%d,Y:%d\n",
						ts->pre_finger_data[i][0],
						ts->pre_finger_data[i][1]);
					}
				}
				ts->pre_finger_mask = 0;
			}

			if (ts->first_pressed == 1) {
				ts->first_pressed = 2;
				I("E1@%d, %d\n", ts->pre_finger_data[0][0],
				ts->pre_finger_data[0][1]);
			}

			if (ts->debug_log_level & BIT(1))
				I("All Finger leave\n");

#ifdef HX_TP_PROC_DIAG
				/*coordinate dump start*/
				if (coordinate_dump_enable == 1) {
					do_gettimeofday(&t);
					time_to_tm(t.tv_sec, 0, &broken);
					snprintf(&coordinate_char[0], 15,
					"%2d:%2d:%2d:%lu,", broken.tm_hour,
					broken.tm_min, broken.tm_sec,
					t.tv_usec / 1000);

					snprintf(&coordinate_char[15], 10,
					"Touch up!");

					coordinate_fn->f_op->write
					(coordinate_fn, &coordinate_char[0],
					15 + (ic_data->HX_MAX_PT + 5)
					* 2 * sizeof(char) * 5 + 2,
					&coordinate_fn->f_pos);
				}
				/*coordinate dump end*/
#endif
		} else if (tpd_key != 0x00) {
			himax_ts_button_func(tpd_key, ts);
			finger_on = 1;
		} else if ((tpd_key_old != 0x00) && (tpd_key == 0x00)) {
			himax_ts_button_func(tpd_key, ts);
			finger_on = 0;
		}
		input_report_key(ts->input_dev, BTN_TOUCH, finger_on);
		input_sync(ts->input_dev);
	}
	tpd_key_old = tpd_key;

workqueue_out:
	return;

err_workqueue_out:
	I("%s: Now reset the Touch chip.\n", __func__);

#ifdef HX_RST_PIN_FUNC
	himax_HW_reset(true, false);
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

#if defined(HX_USB_DETECT)
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
				HIMAX_I2C_RETRY_TIMES);

				I("%s: Cable status change: 0x%2.2X\n",
				__func__, ts->cable_config[1]);
			} else
				I("%s: Cable status is same, ignore.\n",
				__func__);
		} else {
			if (connect_status)
				ts->usb_connected = 0x01;
			else
				ts->usb_connected = 0x00;
			I("%s: Cable status remembered: 0x%2.2X\n",
			__func__, ts->usb_connected);
		}
	}
}

static struct t_cable_status_notifier himax_cable_status_handler = {
	.name = "usb_tp_connected",
	.func = himax_cable_tp_status_handler_func,
};

#endif

#if defined(HX_USB_DETECT2)
void himax_cable_detect_func(void)
{
	uint8_t tmp_addr[4];
	uint8_t tmp_data[128];
	struct himax_ts_data *ts;
	u32 connect_status = 0;

	connect_status = USB_Flag;/*upmu_is_chr_det();*/
	ts = private_ts;
	/*I("Touch: cable status=%d, cable_config=%p,
	usb_connected=%d\n", connect_status,
	ts->cable_config, ts->usb_connected);*/

	if (ts->cable_config) {
		if ((!!connect_status) != ts->usb_connected) {
			/*notify USB plug/unplug*/
			/*0x9008_8060 ==> 0x0000_0000/0001*/
			tmp_addr[3] = 0x90; tmp_addr[2] = 0x08;
			tmp_addr[1] = 0x80; tmp_addr[0] = 0x60;
			tmp_data[3] = 0x00; tmp_data[2] = 0x00;
			tmp_data[1] = 0x00;
			if (!!connect_status) {
				tmp_data[0] = 0x01;
				ts->usb_connected = 0x01;
			} else {
				tmp_data[0] = 0x00;
				ts->usb_connected = 0x00;
			}

			himax_flash_write_burst(ts->client, tmp_addr, tmp_data);

			I("%s: Cable status change: 0x%2.2X\n",
			__func__, ts->usb_connected);
		}
		/*else*/
			/*I("%s: Cable status is the same as previous one,
			ignore.\n", __func__);*/

	}
}
#endif

#ifdef CONFIG_FB
int himax_fb_register(struct himax_ts_data *ts)
{
	int ret = 0;

	I(" %s in", __func__);
	ts->fb_notif.notifier_call = fb_notifier_callback;
	ret = fb_register_client(&ts->fb_notif);
	if (ret)
		E(" Unable to register fb_notifier: %d\n", ret);

	return ret;
}
#endif

#ifdef HX_SMART_WAKEUP
void himax_set_SMWP_func(struct i2c_client *client, uint8_t SMWP_enable)
{
	uint8_t tmp_data[4];

	if (SMWP_enable) {
SMWP_bit_retry:
		himax_set_SMWP_enable(client, SMWP_enable);
		msleep(20);
		himax_get_SMWP_enable(client, tmp_data);
I("%s: Read SMWP bit data[0]=%x data[1]=%x data[2]=%x data[3]=%x\n",
__func__, tmp_data[0], tmp_data[1], tmp_data[2], tmp_data[3]);

		if (tmp_data[0] != 0x01) {
			I("%s: retry SMWP bit write data[0]=%x\n",
			__func__, tmp_data[0]);
			goto SMWP_bit_retry;
		}
	}
}

static void himax_SMWP_work(struct work_struct *work)
{
	struct himax_ts_data *ts =
	container_of(work, struct himax_ts_data, smwp_work.work);
	I(" %s in", __func__);

	himax_set_SMWP_func(ts->client, ts->SMWP_enable);

}
#endif

#ifdef HX_TP_PROC_FLASH_DUMP
static void himax_ts_flash_work_func(struct work_struct *work)
{
	himax_ts_flash_func();
}
#endif

#ifdef HX_TP_PROC_DIAG
static void himax_ts_diag_work_func(struct work_struct *work)
{
	himax_ts_diag_func();
}
#endif

bool himax_ts_init(struct himax_ts_data *ts)
{
	int ret = 0, err = 0;
	struct himax_i2c_platform_data *pdata;
	struct i2c_client *client;

	client = ts->client;
	pdata = ts->pdata;

	I("%s: Start.\n", __func__);

	/* Set pinctrl in active state */
	if (ts->ts_pinctrl) {
		ret = pinctrl_select_state(ts->ts_pinctrl,
		ts->pinctrl_state_active);
		if (ret < 0)
			E("Failed to set pin in active state %d", ret);
	}

	himax_burst_enable(client, 0);

	/*Get Himax IC Type / FW information / Calculate the point number */
	if (himax_check_chip_version(ts->client) == false) {
		E("Himax chip doesn NOT EXIST");
		goto err_ic_package_failed;
	}
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
		goto err_create_wq_failed;
	}

	INIT_WORK(&ts->flash_work, himax_ts_flash_work_func);

	setSysOperation(0);
	setFlashBuffer();
#endif

#ifdef HX_TP_PROC_DIAG
	ts->himax_diag_wq = create_singlethread_workqueue("himax_diag");
	if (!ts->himax_diag_wq) {
		E("%s: create diag workqueue failed\n", __func__);
		err = -ENOMEM;
		goto err_create_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->himax_diag_delay_wrok, himax_ts_diag_work_func);
#endif

himax_read_FW_ver(client);

#ifdef HX_AUTO_UPDATE_FW
	I(" %s in", __func__);
	if (i_update_FW() <= 0)
		I("FW NOT UPDATE=\n");
	else
		I("Have new FW=UPDATE=\n");
#endif

	/*Himax Power On and Load Config*/
	if (himax_loadSensorConfig(client, pdata) < 0) {
		E("%s: Load Sesnsor config failed,unload driver.\n",
		__func__);
		goto err_detect_failed;
	}

	calculate_point_number();
#ifdef HX_TP_PROC_DIAG
	setXChannel(ic_data->HX_RX_NUM); /*X channel*/
	setYChannel(ic_data->HX_TX_NUM); /*Y channel*/

	setMutualBuffer();
	setMutualNewBuffer();
	setMutualOldBuffer();
	if (getMutualBuffer() == NULL) {
		E("%s: mutual buffer allocate fail failed\n", __func__);
		return false;
	}
#ifdef HX_TP_PROC_2T2R
	if (Is_2T2R) {
		setXChannel_2(ic_data->HX_RX_NUM_2); /*X channel*/
		setYChannel_2(ic_data->HX_TX_NUM_2); /*Y channel*/

		setMutualBuffer_2();

		if (getMutualBuffer_2() == NULL) {
			E("%s: mutual buffer 2 allocate fail failed\n",
			__func__);
			return false;
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
	/*calculate the i2c data size*/
	calcDataSize(ts->nFinger_support);
	I("%s: calcDataSize complete\n", __func__);
#ifdef CONFIG_OF
	ts->pdata->abs_pressure_min = 0;
	ts->pdata->abs_pressure_max = 200;
	ts->pdata->abs_width_min = 0;
	ts->pdata->abs_width_max = 200;
	pdata->cable_config[0] = 0x90;
	pdata->cable_config[1] = 0x00;
#endif
	ts->suspended = false;
#if defined(HX_USB_DETECT) || defined(HX_USB_DETECT2)
	ts->usb_connected = 0x00;
	ts->cable_config = pdata->cable_config;
#endif
	ts->protocol_type = pdata->protocol_type;
	I("%s: Use Protocol Type %c\n", __func__,
	ts->protocol_type == PROTOCOL_TYPE_A ? 'A' : 'B');
	ret = himax_input_register(ts);
	if (ret) {
		E("%s: Unable to register %s input device\n",
			__func__, ts->input_dev->name);
		goto err_input_register_device_failed;
	}
#ifdef HX_SMART_WAKEUP
	ts->SMWP_enable = 0;
	wakeup_source_init(&ts->ts_SMWP_wake_lock,
	WAKE_LOCK_SUSPEND, HIMAX_common_NAME);

	ts->himax_smwp_wq = create_singlethread_workqueue("HMX_SMWP_WORK");
	if (!ts->himax_smwp_wq) {
		E(" allocate himax_smwp_wq failed\n");
		err = -ENOMEM;
		goto err_smwp_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->smwp_work, himax_SMWP_work);
#endif
#ifdef HX_HIGH_SENSE
	ts->HSEN_enable = 0;
	ts->himax_hsen_wq = create_singlethread_workqueue("HMX_HSEN_WORK");
	if (!ts->himax_hsen_wq) {
		E(" allocate himax_hsen_wq failed\n");
		err = -ENOMEM;
		goto err_hsen_wq_failed;
	}
	INIT_DELAYED_WORK(&ts->hsen_work, himax_HSEN_func);
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_touch_proc_init();
#endif

#if defined(HX_USB_DETECT)
	if (ts->cable_config)
		cable_detect_register_notifier(&himax_cable_status_handler);
#endif

	err = himax_ts_register_interrupt(ts->client);
	if (err)
		goto err_register_interrupt_failed;
	return true;

err_register_interrupt_failed:
#ifdef HX_HIGH_SENSE
err_hsen_wq_failed:
#endif
#ifdef HX_SMART_WAKEUP
err_smwp_wq_failed:
	wakeup_source_trash(&ts->ts_SMWP_wake_lock);
#endif
err_input_register_device_failed:
	input_free_device(ts->input_dev);
err_detect_failed:
#ifdef HX_TP_PROC_FLASH_DUMP
err_create_wq_failed:
#endif
err_ic_package_failed:
return false;
}

int himax_chip_common_probe(struct i2c_client *client,
const struct i2c_device_id *id)
{
	int err = 0;
	struct himax_ts_data *ts;
	struct himax_i2c_platform_data *pdata;

	/*Check I2C functionality*/
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

#ifdef CONFIG_OF
	/*DeviceTree Init Platform_data*/
	if (client->dev.of_node) {
		err = himax_parse_dt(ts, pdata);
		if (err < 0) {
			I(" pdata is NULL for DT\n");
			goto err_alloc_dt_pdata_failed;
		}
	}
#endif

#ifdef HX_RST_PIN_FUNC
	ts->rst_gpio = pdata->gpio_reset;
#endif

	himax_gpio_power_config(ts->client, pdata);

	err = himax_ts_pinctrl_init(ts);
	if (err || ts->ts_pinctrl == NULL)
		E(" Pinctrl init failed\n");

#ifndef CONFIG_OF
	if (pdata->power) {
		err = pdata->power(1);
		if (err < 0) {
			E("%s: power on failed\n", __func__);
			goto err_power_failed;
		}
	}
#endif
	ts->pdata = pdata;
	private_ts = ts;

	mutex_init(&ts->fb_mutex);
	/* ts initialization is deferred till FB_UNBLACK event;
	 * probe is considered pending till then.*/
	ts->probe_done = false;
#ifdef CONFIG_FB
	err = himax_fb_register(ts);
	if (err) {
		E("Falied to register fb notifier\n");
		err = -ENOMEM;
		goto err_fb_notif_wq_create;
	}
#endif

	return 0;

#ifdef CONFIG_FB
err_fb_notif_wq_create:
#endif
#ifdef CONFIG_OF
err_alloc_dt_pdata_failed:
#else
err_power_failed:
err_get_platform_data_fail:
#endif
	if (ts->ts_pinctrl) {
		if (IS_ERR_OR_NULL(ts->pinctrl_state_release)) {
			devm_pinctrl_put(ts->ts_pinctrl);
			ts->ts_pinctrl = NULL;
		} else {
			err = pinctrl_select_state(ts->ts_pinctrl,
					ts->pinctrl_state_release);
			if (err)
				E("failed to select relase pinctrl state %d\n",
					err);
		}
	}
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
	int ret;
#if defined(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)
	himax_touch_proc_deinit();
#endif
#ifdef CONFIG_FB
	if (fb_unregister_client(&ts->fb_notif)) {
		dev_err(&client->dev,
		"Error occurred while unregistering fb_notifier.\n");
	}
#endif

	if (!ts->use_irq)
		hrtimer_cancel(&ts->timer);

	destroy_workqueue(ts->himax_wq);

	if (ts->protocol_type == PROTOCOL_TYPE_B)
		input_mt_destroy_slots(ts->input_dev);

	input_unregister_device(ts->input_dev);

	if (ts->ts_pinctrl) {
		if (IS_ERR_OR_NULL(ts->pinctrl_state_release)) {
			devm_pinctrl_put(ts->ts_pinctrl);
			ts->ts_pinctrl = NULL;
		} else {
			ret = pinctrl_select_state(ts->ts_pinctrl,
					ts->pinctrl_state_release);
			if (ret)
				E("failed to select relase pinctrl state %d\n",
					ret);
		}
	}
#ifdef HX_SMART_WAKEUP
		wakeup_source_trash(&ts->ts_SMWP_wake_lock);
#endif
	kfree(ts);

	return 0;

}

int himax_chip_common_suspend(struct himax_ts_data *ts)
{
	int ret;

	if (ts->suspended) {
		I("%s: Already suspended. Skipped.\n", __func__);
		return 0;

	} else {
		ts->suspended = true;
		I("%s: enter\n", __func__);
	}

#ifdef HX_TP_PROC_FLASH_DUMP
	if (getFlashDumpGoing()) {
		I("[himax] %s: Flash dump is going,reject suspend\n",
		__func__);
		return 0;
	}
#endif
#ifdef HX_TP_PROC_HITOUCH
	if (hitouch_is_connect) {
		I("[himax] %s: Hitouch connect,reject suspend\n",
		__func__);
		return 0;
	}
#endif
#ifdef HX_SMART_WAKEUP
	if (ts->SMWP_enable) {
		atomic_set(&ts->suspend_mode, 1);
		ts->pre_finger_mask = 0;
		FAKE_POWER_KEY_SEND = false;
		I("[himax] %s: SMART_WAKEUP enable,reject suspend\n",
		__func__);
		return 0;
	}
#endif
#ifdef HX_ESD_WORKAROUND
	ESD_00_counter = 0;
	ESD_00_Flag = 0;
#endif
	if (!ts->use_irq) {
		ret = cancel_work_sync(&ts->work);
		if (ret)
			himax_int_enable(ts->client->irq, 1);
	}

	/*ts->first_pressed = 0;*/
	atomic_set(&ts->suspend_mode, 1);
	ts->pre_finger_mask = 0;

	if (ts->ts_pinctrl) {
		ret = pinctrl_select_state(ts->ts_pinctrl,
				ts->pinctrl_state_suspend);
		if (ret < 0)
			E("Failed to get idle pinctrl state %d\n", ret);
	}

	if (ts->pdata->powerOff3V3 && ts->pdata->power)
		ts->pdata->power(0);

	return 0;
}

int himax_chip_common_resume(struct himax_ts_data *ts)
{
	int retval;

	I("%s: enter\n", __func__);

	if (ts->pdata->powerOff3V3 && ts->pdata->power)
		ts->pdata->power(1);

		if (ts->protocol_type == PROTOCOL_TYPE_A)
			input_mt_sync(ts->input_dev);
		input_report_key(ts->input_dev, BTN_TOUCH, 0);
		input_sync(ts->input_dev);

	if (ts->ts_pinctrl) {
		retval = pinctrl_select_state(ts->ts_pinctrl,
				ts->pinctrl_state_active);
		if (retval < 0) {
			E("Cannot get default pinctrl state %d\n", retval);
			goto err_pinctrl_select_resume;
		}
	}

	atomic_set(&ts->suspend_mode, 0);

	himax_int_enable(ts->client->irq, 1);

	ts->suspended = false;
#if defined(HX_USB_DETECT2)
	ts->usb_connected = 0x00;
	himax_cable_detect_func();
#endif
#ifdef HX_SMART_WAKEUP
	queue_delayed_work(ts->himax_smwp_wq,
	&ts->smwp_work, msecs_to_jiffies(1000));
#endif
#ifdef HX_HIGH_SENSE
	queue_delayed_work(ts->himax_hsen_wq,
	&ts->hsen_work, msecs_to_jiffies(1000));
#endif
	return 0;
err_pinctrl_select_resume:
	if (ts->pdata->powerOff3V3 && ts->pdata->power)
		ts->pdata->power(0);
	return retval;
}

