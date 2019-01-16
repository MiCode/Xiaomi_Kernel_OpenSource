/* lge_ts_melfas.c
 *
 * Copyright (C) 2013 LGE.
 *
 * Author: WX-BSP-TS@lge.com
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

/* History :
 *
 */

#include "lge_ts_melfas.h"
//#include <mach/mt_gpio.h>

#define ts_pdata		((ts)->pdata)
#define ts_caps		(ts_pdata->caps)
#define ts_role		(ts_pdata->role)
#define ts_pwr		(ts_pdata->pwr)

/* LPWG Control Value */
#define IDLE_REPORTRATE_CTRL    1
#define ACTIVE_REPORTRATE_CTRL  2
#define SENSITIVITY_CTRL        3

#define TCI_ENABLE_CTRL         11
#define TOUCH_SLOP_CTRL         12
#define TAP_MIN_DISTANCE_CTRL   13
#define TAP_MAX_DISTANCE_CTRL   14
#define MIN_INTERTAP_CTRL       15
#define MAX_INTERTAP_CTRL       16
#define TAP_COUNT_CTRL          17
#define INTERRUPT_DELAY_CTRL    18

#define TCI_ENABLE_CTRL2        21
#define TOUCH_SLOP_CTRL2        22
#define TAP_MIN_DISTANCE_CTRL2  23
#define TAP_MAX_DISTANCE_CTRL2  24
#define MIN_INTERTAP_CTRL2      25
#define MAX_INTERTAP_CTRL2      26
#define TAP_COUNT_CTRL2         27
#define INTERRUPT_DELAY_CTRL2   28

#define LPWG_STORE_INFO_CTRL    31
#define LPWG_START_CTRL         32
#define LPWG_PANEL_DEBUG_CTRL   33
#define LPWG_FAIL_REASON_CTRL   34

int lockscreen_stat = 0;

static int mms_get_packet(struct i2c_client *client);
static int mms_power(struct i2c_client* client, int power_ctrl);


int mms_i2c_read(struct i2c_client *client, u8 reg, char *buf, int len)
{
	TOUCH_TRACE_FUNC();

	int ret = 0;
	u8 cmd[2] = {MIT_REGH_CMD,reg};
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = 0,
			.len = 2,
			.buf = &reg,
		},
		{
			.addr = client->addr,
			.flags = I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	if (reg == MIT_FW_VERSION) {
		cmd[0] = 0x00;
	}
	msgs[0].buf = cmd;
	
#ifdef USE_DMA
	ret = i2c_msg_transfer(client, msgs, 2);
#else
	ret = i2c_transfer(client->adapter, msgs, 2);
#endif

	if (ret < 0) {
		if (printk_ratelimit())
			TOUCH_ERR_MSG("transfer error: %d\n", ret);
		return -EIO;
	} else
		return 0;
}

#if 0
static int mms_i2c_write(struct i2c_client *client, u8 reg, int len, u8 *buf)
{
	unsigned char send_buf[len + 1];
	struct i2c_msg msgs[] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = len+1,
			.buf = send_buf,
		},
	};

	send_buf[0] = (unsigned char)reg;
	memcpy(&send_buf[1], buf, len);

	if (i2c_transfer(client->adapter, msgs, 1) < 0) {
		if (printk_ratelimit())
			TOUCH_ERR_MSG("transfer error\n");
		return -EIO;
	} else
		return 0;
}
#endif

static int mit_get_otp(struct mms_data *ts) {

	uint8_t read_buf[16] = {0};
	uint8_t write_buf[4] = {0};
	struct i2c_msg msg[] = {
		{
			.addr = ts->client->addr,
			.flags = 0,
			.buf = write_buf,
		},{
			.addr = ts->client->addr,
			.flags = 1,
		},
	};
	TOUCH_TRACE_FUNC();

	write_buf[0] = MIT_REGH_CMD;
	write_buf[1] = MIT_REGL_UCMD;
	write_buf[2] = MIT_UNIV_GET_READ_OTP_STATUS;
	msg[0].len = 3;
#ifdef USE_DMA
	if (i2c_msg_transfer(ts->client, &msg[0], 1) < 0 ) {
#else
	if (i2c_transfer(ts->client->adapter, &msg[0], 1) != 1) {
#endif	
		TOUCH_INFO_MSG("%s : i2c transfer failed\n", __func__);
		return -EIO;
	}

	if (mms_i2c_read(ts->client, MIT_REGL_UCMD_RESULT_LENGTH, read_buf, 1) < 0) {
		TOUCH_INFO_MSG("%s : Fail to get MIT_REGL_UCMD_RESULT_LENGTH \n", __func__);
		return -EIO;
	}

	if (mms_i2c_read(ts->client, MIT_REGL_UCMD_RESULT, read_buf, 1) < 0) {
		TOUCH_INFO_MSG("%s : Fail to get MIT_REGL_UCMD_RESULT \n", __func__);
		return -EIO;
	}
	ts->module.otp = read_buf[0];
	return 0;
}

static int mms_get_ic_info(struct mms_data *ts, struct touch_fw_info *fw_info)
{
	struct i2c_client *client = ts->client;
	int i = 0;
	int otp_check_max = 20;

	TOUCH_TRACE_FUNC();

	if (mms_i2c_read(client, MIT_ROW_NUM, &ts->dev.row_num, 1) < 0) {
		TOUCH_INFO_MSG("MIT_ROW_NUM read failed\n");
		return -EIO;
	}

	if (mms_i2c_read(client, MIT_COL_NUM, &ts->dev.col_num, 1) < 0) {
		TOUCH_INFO_MSG("MIT_COL_NUM read failed\n");
		return -EIO;
	}

	if (mms_i2c_read(client, MIT_FW_VERSION,(u8 *) &ts->module.version, 2) < 0) {
		TOUCH_INFO_MSG("MIT_FW_VERSION read failed\n");
		return -EIO;
	}

	if (mms_i2c_read(client, MIT_FW_PRODUCT,(u8 *) &ts->module.product_code, 16) < 0){
		TOUCH_INFO_MSG("MIT_FW_PRODUCT read failed\n");
		return -EIO;
	}

	for (i = 0; i < otp_check_max; i++) { // need to time check for OTP status
		if (mit_get_otp(ts) < 0) {
			TOUCH_INFO_MSG("failed to get the otp-enable\n");
			return 1;
		}

		if (ts->module.otp == OTP_APPLIED)
			break;

		msleep(5);
	}

	if (ts->pdata->panel_on) {
		TOUCH_INFO_MSG("====== LCD  ON  ======\n");
	} else {
		TOUCH_INFO_MSG("====== LCD  OFF ======\n");
	}
	TOUCH_INFO_MSG("======================\n");
	TOUCH_INFO_MSG("F/W Version : %X.%02X \n", ts->module.version[0], ts->module.version[1]);
	TOUCH_INFO_MSG("F/W Product : %s \n", ts->module.product_code);
	TOUCH_INFO_MSG("F/W Row : %d, Col : %d \n", ts->dev.row_num, ts->dev.col_num);
	if (ts->module.otp == OTP_NOT_SUPPORTED) {
		TOUCH_INFO_MSG("OTP : F/W Not support \n");
	} else {
		TOUCH_INFO_MSG("OTP : %s \n", (ts->module.otp == OTP_APPLIED) ? "Applied" : "None");
	}
	TOUCH_INFO_MSG("======================\n");

	return 0;
}

static void write_file(char *filename, char *data, int time)
{
	int fd = 0;
	char time_string[64] = {0};
	struct timespec my_time;
	struct tm my_date;
	mm_segment_t old_fs = get_fs();

	my_time = __current_kernel_time();
	time_to_tm(my_time.tv_sec, sys_tz.tz_minuteswest * 60 * (-1), &my_date);
	snprintf(time_string, 64, "\n%02d-%02d %02d:%02d:%02d.%03lu \n\n\n",
		my_date.tm_mon + 1,my_date.tm_mday,
		my_date.tm_hour, my_date.tm_min, my_date.tm_sec,
		(unsigned long) my_time.tv_nsec / 1000000);

	set_fs(KERNEL_DS);
	fd = sys_open(filename, O_WRONLY|O_CREAT|O_APPEND, 0666);
	if (fd >= 0) {
		if (time > 0)
			sys_write(fd, time_string, strlen(time_string));
		sys_write(fd, data, strlen(data));
		sys_close(fd);
	}
	set_fs(old_fs);
}

int mit_atoi(char *str)
{
	int i = 0;
	int minus = 0;
	int result = 0;
	int check = 0;

	if ( str[0] == '-' ) {
		i++;
		minus = 1;
	}

	while ((str[i] >= '0') && (str[i] <= '9')) {
		result = (10 * result) + (str[i] - '0');
		i++;
		check = 1;
	}

	if (!check) {
		TOUCH_INFO_MSG("atoi fail\n");
		return 0xFFF;
	}
	return (minus) ? ((-1) * result) : result;
}

static int read_file(char *filename, char *data, size_t length)
{
	int fd = 0;
	int len = 0;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	fd = sys_open(filename, O_RDONLY, 0666);
	if (fd >= 0) {
		len = sys_read(fd, data, length );
		sys_close(fd);
		if (len <= 0) {
			TOUCH_INFO_MSG("%s sys_read Err len = %d\n", __func__, len);
			goto SYSFS_ERROR;
		}
	} else {
		goto SYSFS_ERROR;
	}
	set_fs(old_fs);
	return 0;

SYSFS_ERROR :
	TOUCH_INFO_MSG("read file fail \n");
	set_fs(old_fs);
	return -1;
}

static void mit_battery_thermal(struct mms_data *ts, char caller)
{
	char data[32] = {0};
	short is_present = 2;
	short ret = 0xFFF;
	uint8_t write_buf[8] = {0};
	struct i2c_msg msg[] = {
		{
			.addr = ts->client->addr,
			.flags = 0,
			.buf = write_buf,
		},{
			.addr = ts->client->addr,
			.flags = 1,
		},
	};
#ifndef IS_MTK
	if (!gpio_get_value(ts->pdata->reset_pin)) {
		return;
	}
#endif

	if (read_file(BATT_THERMAL, data, sizeof(data) - 1) < 0)
		goto DATA_ERROR;

	ret = (short)mit_atoi(data + 7);

	if (ret >= 2000)
		goto DATA_ERROR;

	if (ret != 0xFFF) {
		if (ret == -300) {
			memset(data, 0, sizeof(data));
			if (read_file(BATT_PRESENT, data, sizeof(data) - 1) < 0)
				goto DATA_ERROR;
			is_present = (short)mit_atoi(data);

			if (is_present == 0) {
				ret = 300;
				TOUCH_INFO_MSG("No Battery\n");
			} else
				TOUCH_INFO_MSG("present battery  = %d\n", is_present);
		}

		TOUCH_INFO_MSG("Thermal value %d %s\n", ret, (caller ? "[IC]" : ""));
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_REGL_UCMD;
		write_buf[2] = MIT_UNIV_SEND_THERMAL_INFO;
		write_buf[3] = (char)((ret & 0xFF00) >> 8);
		write_buf[4] = (char)(ret & 0xFF);
		msg[0].len = 5;
#ifdef USE_DMA
	if (i2c_msg_transfer(ts->client, &msg[0], 1) < 0) {
#else
	if (i2c_transfer(ts->client->adapter, &msg[0], 1) != 1) {
#endif			
			TOUCH_INFO_MSG("%s : i2c transfer failed\n", __func__);
		}
		return;
	}

DATA_ERROR :
	TOUCH_INFO_MSG("%s failed\n", __func__);
	return;

}

#if defined(TOUCH_USE_DSV)
void mms_dsv_control(struct i2c_client *client)
{
	struct mms_data *ts = (struct mms_data *) get_touch_handle_(client);
	int set_value = ts->pdata->use_dsv;

	if (set_value)
		msleep(200);
//	mdss_dsv_ctl(set_value);

	TOUCH_INFO_MSG("dsv_ctrl onoff: %d\n", set_value);
}
#endif

static int set_tci_info(struct i2c_client *client)
{
	struct mms_data *ts = get_touch_handle_(client);
	TOUCH_INFO_MSG("Setting tci info data\n");

	//common
	ts->pdata->tci_info->idle_report_rate = 20;
	ts->pdata->tci_info->active_report_rate = 40;
	ts->pdata->tci_info->sensitivity = 30;

	//double tap only
	ts->pdata->tci_info->touch_slope = 10;
	ts->pdata->tci_info->min_distance = 0;
	ts->pdata->tci_info->max_distance = 10;
	ts->pdata->tci_info->min_intertap = 0;
	ts->pdata->tci_info->max_intertap = 700;
	ts->pdata->tci_info->tap_count = 2;

	//multitap only
	ts->pdata->tci_info->touch_slope_2 = 10;
	ts->pdata->tci_info->min_distance_2 = 0;
	ts->pdata->tci_info->max_distance_2 = 255;
	ts->pdata->tci_info->min_intertap_2 = 0;
	ts->pdata->tci_info->max_intertap_2 = 700;
	ts->pdata->tci_info->interrupt_delay_2 = 0;

	return 0;
}

static int mms_probe(struct i2c_client *client, struct touch_platform_data *pdata)
{
	struct mms_data *ts = NULL;
	int ret = 0;
	int i = 0;
	char gpio_request_name[16] = {0};

	TOUCH_TRACE_FUNC();

	ts = devm_kzalloc(&client->dev, sizeof(struct mms_data), GFP_KERNEL);

	if (ts == NULL) {
		TOUCH_ERR_MSG("Can not allocate memory\n");
		return -ENOMEM;
	}

	ts->client = client;
	ts->pdata = pdata;
	ts->log.data = ts->buf;
	ts->pdata->tap_count = 4; //default tap count set
	ts->pdata->lpwg_prox = 1; //default proxi sensor information

	set_touch_handle_(client, ts);
	set_tci_info(client);

	//power on
#ifndef IS_MTK
	for (i = 0; i < TOUCH_PWR_NUM; ++i) {
		if (ts_pwr[i].type == 1 && gpio_is_valid(ts_pwr[i].value)) {
			snprintf(gpio_request_name, 16, "touch_vdd_%d", i);
			if (!strncmp(ts_pwr[i].name, "low", strlen("low")))
				ret = gpio_request_one(ts_pwr[i].value, GPIOF_OUT_INIT_LOW, gpio_request_name);
			else
				ret = gpio_request_one(ts_pwr[i].value, GPIOF_OUT_INIT_HIGH, gpio_request_name);

			if (ret) {
				ts_pwr[i].value = -1;
				goto err_regulator_get;
			}
		} else if (ts_pwr[i].type == 2) {
			ts->vdd_regulator[i] = regulator_get(&client->dev, ts_pwr[i].name);
			if (IS_ERR(ts->vdd_regulator[i])) {
				ret = PTR_ERR(ts->vdd_regulator[i]);
				TOUCH_ERR_MSG("Can NOT get regulator : %s, ret = %d\n", ts_pwr[i].name, ret);
				goto err_regulator_get;
			}

			if (regulator_count_voltages(ts->vdd_regulator[i]) > 0) {
				ret = regulator_set_voltage(ts->vdd_regulator[i], ts_pwr[i].value, ts_pwr[i].value);
				if (ret) {
					TOUCH_ERR_MSG("Error(ret=%d) set regulator(%s) voltage %d\n", ret, ts_pwr[i].name, ts_pwr[i].value);
					goto err_regulator_get;
				}
			}
		}
	}
#else
//	mt_set_gpio_out(ts_pdata->reset_pin, 0);		
	hwPowerOn ( MT6323_POWER_LDO_VGP1, VOL_1800, "TP" );
	
	mms_power(ts->client,POWER_ON);
#endif

	for (i = 0; i < MAX_ROW; i++) {
		ts->mit_data[i] = kzalloc(sizeof(uint16_t) * MAX_COL, GFP_KERNEL);
		if (ts->mit_data[i] == NULL) {
			TOUCH_ERR_MSG("mit_data kzalloc error\n");
			return -ENOMEM;
		}
		ts->intensity_data[i] = kzalloc(sizeof(uint16_t) * MAX_COL, GFP_KERNEL);
		if (ts->intensity_data[i] == NULL) {
			TOUCH_ERR_MSG("intensity_data kzalloc error\n");
			return -ENOMEM;
		}
	}
	return 0;

err_regulator_get:
	do {
		if (ts_pwr[i].type == 1) {
			if (gpio_is_valid(ts_pwr[i].value))
				gpio_free(ts_pwr[i].value);
		} else if (ts_pwr[i].type == 2) {
			if (ts->vdd_regulator != NULL && !IS_ERR(ts->vdd_regulator[i]))
				regulator_put(ts->vdd_regulator[i]);
		}
	} while(--i >= 0);
	return ret;
}

static void mms_remove(struct i2c_client* client)
{
	struct mms_data *ts = get_touch_handle_(client);
	int i = TOUCH_PWR_NUM-1;

	TOUCH_TRACE_FUNC();

	do {
		if (ts_pwr[i].type == 1) {
			if (!strncmp(ts_pwr[i].name, "low", strlen("low")))
				gpio_direction_output(ts_pwr[i].value, 1);
			else
				gpio_direction_output(ts_pwr[i].value, 0);

			if (gpio_is_valid(ts_pwr[i].value))
				gpio_free(ts_pwr[i].value);
		} else if (ts_pwr[i].type == 2) {
			if (ts->vdd_regulator[i] != NULL && !IS_ERR(ts->vdd_regulator[i])) {
				regulator_put(ts->vdd_regulator[i]);
			}
		}
	} while(--i >= 0);

	for (i = 0; i < MAX_ROW; i++) {
		if (ts->mit_data[i] != NULL) {
			kfree(ts->mit_data[i]);
		}
		if (ts->intensity_data[i] != NULL) {
			kfree(ts->intensity_data[i]);
		}
	}
}

static int mms_init(struct i2c_client* client, struct touch_fw_info* fw_info)
{
	struct mms_data *ts = get_touch_handle_(client);

	TOUCH_TRACE_FUNC();

	ts->probed = true;
	return 0;
}

static int mms_touch_event(struct i2c_client *client, struct touch_data *data, u8 *buf, int sz)
{
	struct mms_data *ts = get_touch_handle_(client);
	u8 *tmp = buf;
	int i = 0;

	u8 touch_count = 0;
	u8 index = 0;
	u8 state = 0;
	u8 palm = 0;

	u8 id = 0;
	u16 x = 0;
	u16 y = 0;
	u8 touch_major = 0;
	u8 pressure = 0;
	int finger_event_sz = 0;

	TOUCH_TRACE_FUNC();

	finger_event_sz = MIT_FINGER_EVENT_SZ;

	data->total_num = touch_count;

	for (i = 0; i < sz; i += finger_event_sz) {
		tmp = buf + i;
		index = (tmp[0] & 0xf) - 1;
		state = (tmp[0] & 0x80) ? 1 : 0;

		if (tmp[0] & MIT_TOUCH_KEY_EVENT) {
			if (index < 0 || index >= ts_caps->number_of_button) {
				TOUCH_ERR_MSG("invalid key index (%d)\n", index);
				return -EIO;
			}
			data->curr_button.key_code = ts_caps->button_name[index];
			data->curr_button.state = state;

			if (unlikely(touch_debug_mask_ & DEBUG_GET_DATA))
				TOUCH_INFO_MSG("key_code=[0x%02x-%d], state=[%d]\n",
						data->curr_button.key_code, data->curr_button.key_code, data->curr_button.state);
		} else {
			if (index < 0 || index >= ts_caps->max_id) {
				TOUCH_ERR_MSG("invalid touch index (%d)\n", index);
				return -EIO;
			}

			id = index;
			palm = (tmp[0] & 0x10) ? 1 : 0;
			x = tmp[2] | ((tmp[1] & 0x0f) << 8);
			y = tmp[3] | ((tmp[1] & 0xf0) << 4);
			touch_major = tmp[4];
			pressure = tmp[5];

			if (palm) {
				if (state) {
					TOUCH_INFO_MSG("Palm detected : %d \n", pressure);
					data->palm = true;
				}
				else {
					TOUCH_INFO_MSG("Palm released : %d \n", pressure);
					data->palm = false;
				}
				return 0;
			}

			if (state) {
				data->curr_data[id].id = id;
				data->curr_data[id].x_position = x;
				data->curr_data[id].y_position = y;
				data->curr_data[id].width_major = touch_major;
				data->curr_data[id].width_minor = 0;
				data->curr_data[id].width_orientation = 0;
				data->curr_data[id].pressure = pressure;
				data->curr_data[id].status = FINGER_PRESSED;
				touch_count++;
			} else {
				data->curr_data[id].status = FINGER_RELEASED;
			}

			if (unlikely(touch_debug_mask_ & DEBUG_GET_DATA)) {
				TOUCH_INFO_MSG("<%d> pos(%4d,%4d) w_m[%2d] w_n[%2d] w_o[%2d] p[%2d] s[%d]\n",
					id, data->curr_data[id].x_position, data->curr_data[id].y_position,
					data->curr_data[id].width_major, data->curr_data[id].width_minor,
					data->curr_data[id].width_orientation, data->curr_data[id].pressure, state);
			}

			if (data->curr_data[id].status == FINGER_PRESSED
				&& data->prev_data[id].status <= FINGER_RELEASED
				&& !data->curr_data[id].point_log_state) {
					data->curr_data[id].touch_conut = 0;
					++data->touch_count_num;
					if (likely(touch_debug_mask_ & DEBUG_ABS_POINT)) {
						if (lockscreen_stat == 1) {
							TOUCH_INFO_MSG("%d finger pressed : <%d> x[XXX] y[XXX] z[XXX]\n",
							data->touch_count_num, id);
						} else {
							TOUCH_INFO_MSG("%d finger pressed : <%d> x[%3d] y[%3d] z[%3d]\n",
							data->touch_count_num, id,
							data->curr_data[id].x_position,
							data->curr_data[id].y_position,
							data->curr_data[id].pressure);
						}
					}
					data->curr_data[id].point_log_state = 1;
			}

			else if (data->curr_data[id].status == FINGER_RELEASED
				&& data->prev_data[id].point_log_state) {
					data->touch_count_num--;

					if (likely(touch_debug_mask_ & DEBUG_ABS_POINT)) {
						if (lockscreen_stat == 1) {
							TOUCH_INFO_MSG("touch_release[%s] : <%d> x[XXX] y[XXX] M:XX\n",
							data->palm?"Palm":" ", id);
						} else {
							TOUCH_INFO_MSG("touch_release[%s] : <%d> x[%3d] y[%3d] M:%d\n",
							data->palm?"Palm":" ", id,
							data->prev_data[id].x_position,
							data->prev_data[id].y_position,
							data->curr_data[id].touch_conut);
						}
					}
					data->curr_data[id].point_log_state = 0;
			} else {
				data->curr_data[id].touch_conut++;
			}
		}
	}

	data->total_num = touch_count;

	if (unlikely(touch_debug_mask_ & DEBUG_GET_DATA))
		TOUCH_INFO_MSG("Total_num: %d\n", data->total_num);

	return 0;
}

static int mms_lpwg_event(struct i2c_client *client, struct touch_data *data, u8 *buf, int sz)
{
	struct mms_data *ts = get_touch_handle_(client);
	int i = 0;
	int id = 0;
	int x = 0;
	int y = 0;
	u8 *tmp = NULL;

	ts->pdata->send_lpwg = 0;
	ts->pdata->lpwg_size = 0;

	if (buf[1] == 0) {
		TOUCH_INFO_MSG("LPWG Password Tap detected \n");
		for (i = 2; i < sz; i += MIT_LPWG_EVENT_SZ) {
			tmp = buf + i;
			id = i;
			x = tmp[1] | ((tmp[0] & 0xf) << 8);
			y = tmp[2] | (((tmp[0] >> 4 ) & 0xf) << 8);
			TOUCH_INFO_MSG("LPWG %d TAP x[%3d] y[%3d] \n", (i+1)/MIT_LPWG_EVENT_SZ, x, y);
			ts->pdata->lpwg_x[((i + 1) / MIT_LPWG_EVENT_SZ) - 1] = x;
			ts->pdata->lpwg_y[((i + 1) / MIT_LPWG_EVENT_SZ) - 1] = y;
			ts->pdata->lpwg_size++;
			ts->pdata->send_lpwg = LPWG_MULTI_TAP;
		}
	} else if (buf[1] == 1) {
		TOUCH_INFO_MSG("LPWG Double Tap detected \n");
		for (i = 2; i < sz; i += MIT_LPWG_EVENT_SZ) {
			tmp = buf + i;
			id = i;
			x = tmp[1] | ((tmp[0] & 0xf) << 8);
			y = tmp[2] | (((tmp[0] >> 4 ) & 0xf) << 8);
			TOUCH_INFO_MSG("LPWG %d TAP x[%3d] y[%3d] \n", (i+1)/MIT_LPWG_EVENT_SZ, x, y);
			ts->pdata->lpwg_x[((i + 1) / MIT_LPWG_EVENT_SZ) - 1] = x;
			ts->pdata->lpwg_y[((i + 1) / MIT_LPWG_EVENT_SZ) - 1] = y;
			ts->pdata->lpwg_size++;
			ts->pdata->send_lpwg = LPWG_DOUBLE_TAP;
		}
	} else {
		TOUCH_INFO_MSG("Unknown Packet Error : %02X %02X %02X %02X %02X \n", buf[0], buf[1], buf[2], buf[3], buf[4]);
	}

	return 0;
}

static int mms_log_event(struct i2c_client *client, struct mms_data *ts)
{
	struct mms_log_pkt *pkt = (struct mms_log_pkt *) ts->buf;
	char *tmp = NULL;
	int len = 0;
	u8 row_num = 0;

	TOUCH_TRACE_FUNC();

	if ((pkt->log_info & 0x7) == 0x1) {
		pkt->element_sz = 0;
		pkt->row_sz = 0;
		return -EIO;
	}

	switch (pkt->log_info >> 4) {
		case LOG_TYPE_U08:
		case LOG_TYPE_S08:
			len = pkt->element_sz;
			break;
		case LOG_TYPE_U16:
		case LOG_TYPE_S16:
			len = pkt->element_sz * 2;
			break;
		case LOG_TYPE_U32:
		case LOG_TYPE_S32:
			len = pkt->element_sz * 4;
			break;
		default:
			dev_err(&client->dev, "invalied log type\n");
			return -EIO;
	}

	tmp = ts->buf + sizeof(struct mms_log_pkt);
	row_num = pkt->row_sz ? pkt->row_sz : 1;

	while (row_num--) {
		mms_i2c_read(client, MIT_REGL_UCMD_RESULT, tmp, len);
		tmp += len;
	}

	return 0;
}

static int mms_get_packet(struct i2c_client *client)
{
	struct mms_data *ts = get_touch_handle_(client);
	u8 sz = 0;

	TOUCH_TRACE_FUNC();

	if (mms_i2c_read(client, MIT_EVENT_PKT_SZ, &sz, 1) < 0)
		return -EIO;

	if (sz == 0) {
		TOUCH_ERR_MSG("mms_get_packet sz=0 \n");
		return 0;
	}

	memset(ts->buf, 0, FINGER_EVENT_SZ * ts->pdata->caps->max_id);

	if (mms_i2c_read(client, MIT_INPUT_EVENT, ts->buf, sz) < 0)
		return -EIO;

	return (int) sz;
}

static void mms_check_lpwg_fail_reason(struct i2c_client *client, int cnt)
{
	struct mms_data *ts = get_touch_handle_(client);
	int i = 0;

	for (i = 2; i < cnt; i++) {
		switch (ts->buf[i]) {
			case FAIL_MULTI_TOUCH:
				TOUCH_INFO_MSG("LPWG FAIL REASON = Multi-Touch\n");
				break;
			case FAIL_TOUCH_SLOP:
				TOUCH_INFO_MSG("LPWG FAIL REASON = Touch Slop\n");
				break;
			case FAIL_TAP_DISTANCE:
				TOUCH_INFO_MSG("LPWG FAIL REASON = Tap Distance\n");
				break;
			case FAIL_TAP_TIME:
				TOUCH_INFO_MSG("LPWG FAIL REASON = Tap Time\n");
				break;
			case FAIL_TOTAL_COUNT:
				TOUCH_INFO_MSG("LPWG FAIL REASON = Total Count\n");
				break;
			case FAIL_DELAY_TIME:
				TOUCH_INFO_MSG("LPWG FAIL REASON = Delay Time\n");
				break;
			case FAIL_PALM:
				TOUCH_INFO_MSG("LPWG FAIL REASON = Palm\n");
				break;
			case FAIL_ACTIVE_AREA:
				TOUCH_INFO_MSG("LPWG FAIL REASON = Active Area\n");
				break;
			default:
				TOUCH_INFO_MSG("LPWG FAIL REASON = Unknown Fail Reason\n");
				break;
		}
	}
}

static int mms_get_data(struct i2c_client *client, struct touch_data *data)
{
	struct mms_data *ts = get_touch_handle_(client);
	int sz = 0;
	u8 event_type;
	uint8_t *dummy_buf = NULL;
	TOUCH_TRACE_FUNC();

	sz = mms_get_packet(client);
	if (sz == 0)
		return 0;
	if ((sz) < 0)
		return -EIO;

	event_type = ts->buf[0] & 0xf;

	if (event_type >= 0x1 && event_type <= 0xa) {
		if (mms_touch_event(client, data, ts->buf, sz) < 0)
			goto err_event_type;
	} else if (event_type == MIT_LPWG_EVENT) {
		if (mms_lpwg_event(client, data, ts->buf, sz) < 0)
			goto err_event_type;
	} else if (event_type == MIT_ERROR_EVENT) {
		if (ts->buf[1] == MIT_REQUEST_THERMAL_INFO) {
			mit_battery_thermal(ts, 1);
			return 0;
		} else if (ts->buf[1] == MIT_ERRORCODE_FAIL_REASON) {
			mms_check_lpwg_fail_reason(client, sz);
			if ((dummy_buf = kzalloc(PAGE_SIZE, GFP_KERNEL)) !=  NULL) {
				mit_delta_show(client, dummy_buf);
				kfree(dummy_buf);
			}
			return 0;
		} else {
			TOUCH_ERR_MSG("Error Event Data Buf[1] : 0x%x\n", ts->buf[1]);
			data->state = ts->buf[1];
			goto mms_error_event;
		}
	} else if (event_type == MIT_LOG_EVENT) {
		if (mms_log_event(client, ts) < 0)
			goto err_event_type;
	} else {
		TOUCH_ERR_MSG("Unkown, event type 0x%x\n", event_type);
		goto err_event_type;
	}

	return 0;

err_event_type:
	TOUCH_ERR_MSG("Unkown, event type 0x%x\n", event_type);
	return -EIO;

mms_error_event:
	TOUCH_ERR_MSG("Unkown, event type 0x%x\n", event_type);
	return -ENXIO;
}

static int mms_sleep(struct i2c_client *client)
{
	return 0;
}

static int mms_wake(struct i2c_client *client)
{
	return 0;
}

static int mms_power(struct i2c_client* client, int power_ctrl)
{
	struct mms_data* ts = get_touch_handle_(client);
	int i = 0;
	int ret = 0;

	TOUCH_POWER_MSG("%s = %d\n", __func__, power_ctrl);

	if (ts->pdata->curr_pwr_state == power_ctrl) {
		TOUCH_INFO_MSG("Ignore Power Control : curr_pwr_state = %d\n", power_ctrl);
		return 0;
	}
	switch (power_ctrl) {
	case POWER_OFF:
#ifndef IS_MTK
		gpio_direction_output(ts_pdata->reset_pin, 0);
		TOUCH_POWER_MSG("power: reset_pin low \n");
		msleep(2);
		i = TOUCH_PWR_NUM-1;
		do {
			if (ts_pwr[i].type == 1) {
				if (!strncmp(ts_pwr[i].name, "low", strlen("low"))) {
					gpio_direction_output(ts_pwr[i].value, 1);
					TOUCH_POWER_MSG("power[%d]: gpio[%d] set 1\n", i, ts_pwr[i].value);
				} else {
					gpio_direction_output(ts_pwr[i].value, 0);
					TOUCH_POWER_MSG("power[%d]: gpio[%d] set 0\n", i, ts_pwr[i].value);
				}
			} else if (ts_pwr[i].type == 2) {

				if (ts->vdd_regulator[i] != NULL && !IS_ERR(ts->vdd_regulator[i])) {
					regulator_disable(ts->vdd_regulator[i]);
					TOUCH_POWER_MSG("power[%d]: regulator disabled\n", i);
				}
			}
                        mdelay(2);
		} while(--i >= 0);
#else
		mt_set_gpio_out(ts_pdata->reset_pin, 0);
		TOUCH_POWER_MSG("power: reset_pin low \n");

		hwPowerDown ( MT6323_POWER_LDO_VGP2, "TP" );
		mdelay(2);
#endif
		TOUCH_INFO_MSG("Power Off \n");
		break;
	case POWER_ON:
		i = 0;
#ifndef IS_MTK
		do {
			if (ts_pwr[i].type == 1) {
				if (!strncmp(ts_pwr[i].name, "low", strlen("low"))) {
					gpio_direction_output(ts_pwr[i].value, 0);
					TOUCH_POWER_MSG("power[%d]: gpio[%d] set 0\n", i, ts_pwr[i].value);
				} else {
					gpio_direction_output(ts_pwr[i].value, 1);
					TOUCH_POWER_MSG("power[%d]: gpio[%d] set 1\n", i, ts_pwr[i].value);
				}
			} else if (ts_pwr[i].type == 2) {
				if (ts->vdd_regulator[i] != NULL && !IS_ERR(ts->vdd_regulator[i])) {
					ret = regulator_enable(ts->vdd_regulator[i]);
					if (ret) {
						TOUCH_INFO_MSG("power[%d]: regulator enable failed ret =%d\n", i, ret );
					} else {
						TOUCH_POWER_MSG("power[%d]: regulator enabled\n", i);
					}
				}
			}
			mdelay(2);
		} while(++i < TOUCH_PWR_NUM);
#else
		hwPowerOn ( MT6323_POWER_LDO_VGP2, VOL_3000, "TP" );
		mdelay(2);
#endif

#ifndef IS_MTK		
		gpio_direction_output(ts_pdata->reset_pin, 1);
#else
		mt_set_gpio_out(ts_pdata->reset_pin, 1);
#endif
		TOUCH_POWER_MSG("power: reset_pin high \n");
		msleep(ts_role->booting_delay);
		TOUCH_INFO_MSG("Power On \n");

		if (!ts->thermal_info_send_block) {
			mit_battery_thermal(ts, 0);
		}

		break;
	case POWER_SLEEP:
		if (mms_sleep(client))
			return -EIO;
		break;

	case POWER_WAKE:
		if (mms_wake(client))
			return -EIO;
		break;
		

	default:
		return -EIO;
		break;
	}

	ts->pdata->curr_pwr_state = power_ctrl;

	return 0;
}

int mms_power_ctrl(struct i2c_client* client, int power_ctrl)
{
	TOUCH_INFO_MSG("%s : %d \n", __func__, power_ctrl);

	return mms_power(client, power_ctrl);
}
EXPORT_SYMBOL(mms_power_ctrl);

int mms_power_reset(struct mms_data *ts)
{
	TOUCH_INFO_MSG("Power Reset \n");

	mms_power(ts->client, POWER_OFF);
	msleep(ts->pdata->role->reset_delay);
	mms_power(ts->client, POWER_ON);
	msleep(ts->pdata->role->reset_delay);

	return 0;
}

static int mms_firmware_img_parse_show(const char *image_bin, char *show_buf, int ret)
{
	struct mms_bin_hdr *fw_hdr = NULL;
	return 0;

	fw_hdr = (struct mms_bin_hdr *) image_bin;

	ret += sprintf(show_buf + ret, "mms_fw_hdr:\n");
	ret += sprintf(show_buf + ret, "\ttag[%c%c%c%c%c%c%c%c]\n",
			fw_hdr->tag[0], fw_hdr->tag[1], fw_hdr->tag[2], fw_hdr->tag[3],
			fw_hdr->tag[4], fw_hdr->tag[5], fw_hdr->tag[6], fw_hdr->tag[7]);
	ret += sprintf(show_buf + ret, "\tcore_version[0x%02x]\n", fw_hdr->core_version);
	ret += sprintf(show_buf + ret, "\tsection_num[%d]\n", fw_hdr->section_num);
	ret += sprintf(show_buf + ret, "\tcontains_full_binary[%d]\n", fw_hdr->contains_full_binary);
	ret += sprintf(show_buf + ret, "\tbinary_offset[%d (0x%04x)]\n", fw_hdr->binary_offset, fw_hdr->binary_offset);
	ret += sprintf(show_buf + ret, "\tbinary_length[%d]\n", fw_hdr->binary_length);

	return ret;
}

static int mms_fw_upgrade(struct i2c_client* client, struct touch_fw_info *info)
{
	struct mms_data *ts = get_touch_handle_(client);
	int ret = 0;

	TOUCH_TRACE_FUNC();

	touch_disable(ts->client->irq);

	if (info->fw)
		ret = mit_isc_fwupdate(ts, info);

	touch_enable(ts->client->irq);

	return ret;
}

static int mms_set_active_area(struct mms_data* ts, u8 mode)
{
	char write_buf[255] = {0};

	if (mode) {
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_ACTIVE_AREA_REG;
		write_buf[2] = ts->pdata->active_area_x1 >> 8;
		write_buf[3] = ts->pdata->active_area_x1 & 0xFF;
		write_buf[4] = ts->pdata->active_area_y1 >> 8;
		write_buf[5] = ts->pdata->active_area_y1 & 0xFF;
		write_buf[6] = ts->pdata->active_area_x2 >> 8;
		write_buf[7] = ts->pdata->active_area_x2 & 0xFF;
		write_buf[8] = ts->pdata->active_area_y2 >> 8;
		write_buf[9] = ts->pdata->active_area_y2 & 0xFF;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 10) != 10) {
#else
		if (i2c_master_send(ts->client, write_buf, 10) != 10) {
#endif //USE_DMA
			TOUCH_INFO_MSG("MIT_LPWG_ACTIVE_AREA write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_ACTIVE_AREA\n");
		}
	} else {
		TOUCH_INFO_MSG("None Active Area \n");
	}

	return 0;
}

static int tci_control(struct mms_data* ts, int type, u16 value)
{
	char write_buf[255] = {0};
	/* Common Reg */
	switch (type) {
	case IDLE_REPORTRATE_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_IDLE_REPORTRATE_REG;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_IDLE_REPORTRATE_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_IDLE_REPORTRATE_REG\n");
		}
		break;
	case ACTIVE_REPORTRATE_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_ACTIVE_REPORTRATE_REG;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_ACTIVE_REPORTRATE_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_ACTIVE_REPORTRATE_REG\n");
		}
		break;
	case SENSITIVITY_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_SENSITIVITY_REG;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_SENSITIVITY_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_SENSITIVITY_REG = %d \n", write_buf[2]);
		}
		break;
	/* TCI1 reg */
	case TCI_ENABLE_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_TCI_ENABLE_REG;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_TCI_ENABLE_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_TCI_ENABLE_REG = %d \n", write_buf[2]);
		}
		break;
	case TOUCH_SLOP_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_TOUCH_SLOP_REG;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_TOUCH_SLOP_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_TOUCH_SLOP_REG\n");
		}
		break;
	case TAP_MIN_DISTANCE_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_TAP_MIN_DISTANCE_REG;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_TAP_MIN_DISTANCE_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_TAP_MIN_DISTANCE_REG\n");
		}
		break;
	case TAP_MAX_DISTANCE_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_TAP_MAX_DISTANCE_REG;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_TAP_MAX_DISTANCE_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_TAP_MAX_DISTANCE_REG\n");
		}
		break;
	case MIN_INTERTAP_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_MIN_INTERTAP_REG;
		write_buf[2] = (value >> 8);
		write_buf[3] = (value & 0xFF);
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 4) != 4) {
#else
		if (i2c_master_send(ts->client, write_buf, 4) != 4) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_MIN_INTERTAP_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_MIN_INTERTAP_REG\n");
		}
		break;
	case MAX_INTERTAP_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_MAX_INTERTAP_REG;
		write_buf[2] = (value >> 8);
		write_buf[3] = (value & 0xFF);
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 4) != 4) {
#else
		if (i2c_master_send(ts->client, write_buf, 4) != 4) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_MAX_INTERTAP_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_MAX_INTERTAP_REG\n");
		}
		break;
	case TAP_COUNT_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_TAP_COUNT_REG;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_TAP_COUNT_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_TAP_COUNT_REG\n");
		}
		break;
	case INTERRUPT_DELAY_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_INTERRUPT_DELAY_REG;
		write_buf[2] = ((value ? KNOCKON_DELAY : 0) >> 8);
		write_buf[3] = ((value ? KNOCKON_DELAY : 0) & 0xFF);
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 4) != 4) {
#else
		if (i2c_master_send(ts->client, write_buf, 4) != 4) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_INTERRUPT_DELAY_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_INTERRUPT_DELAY_REG\n");
		}
		break;
	/* TCI2 reg */
	case TCI_ENABLE_CTRL2:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_TCI_ENABLE_REG2;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_TCI_ENABLE_REG2 write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_TCI_ENABLE_REG2 = %d\n", write_buf[2]);
		}
		break;
	case TOUCH_SLOP_CTRL2:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_TOUCH_SLOP_REG2;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_TOUCH_SLOP_REG2 write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_TOUCH_SLOP_REG2\n");
		}
		break;
	case TAP_MIN_DISTANCE_CTRL2:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_TAP_MIN_DISTANCE_REG2;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_TAP_MIN_DISTANCE_REG2 write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_TAP_MIN_DISTANCE_REG2\n");
		}
		break;
	case TAP_MAX_DISTANCE_CTRL2:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_TAP_MAX_DISTANCE_REG2;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_TAP_MAX_DISTANCE_REG2 write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_TAP_MAX_DISTANCE_REG2\n");
		}
		break;
	case MIN_INTERTAP_CTRL2:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_MIN_INTERTAP_REG2;
		write_buf[2] = (value >> 8);
		write_buf[3] = (value & 0xFF);
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 4) != 4) {
#else
		if (i2c_master_send(ts->client, write_buf, 4) != 4) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_MIN_INTERTAP_REG2 write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_MIN_INTERTAP_REG2\n");
		}
		break;
	case MAX_INTERTAP_CTRL2:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_MAX_INTERTAP_REG2;
		write_buf[2] = (value >> 8);
		write_buf[3] = (value & 0xFF);
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 4) != 4) {
#else
		if (i2c_master_send(ts->client, write_buf, 4) != 4) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_MAX_INTERTAP_REG2 write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_MAX_INTERTAP_REG2\n");
		}
		break;
	case TAP_COUNT_CTRL2:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_TAP_COUNT_REG2;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_TAP_COUNT_REG2 write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_TAP_COUNT_REG2 = %d\n", write_buf[2]);
		}
		break;
	case INTERRUPT_DELAY_CTRL2:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_INTERRUPT_DELAY_REG2;
		write_buf[2] = ((value ? KNOCKON_DELAY : 0) >> 8);
		write_buf[3] = ((value ? KNOCKON_DELAY : 0) & 0xFF);
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 4) != 4) {
#else
		if (i2c_master_send(ts->client, write_buf, 4) != 4) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_INTERRUPT_DELAY_REG2 write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_INTERRUPT_DELAY_REG2\n");
		}
		break;

	case LPWG_STORE_INFO_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_STORE_INFO_REG;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_STORE_INFO_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_STORE_INFO_REG = %d\n", write_buf[2]);
		}
		break;
	case LPWG_START_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_START_REG;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_START_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_START\n");
		}
		break;
	case LPWG_PANEL_DEBUG_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_PANEL_DEBUG_REG;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_PANEL_DEBUG_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_PANEL_DEBUG_REG = %d \n", write_buf[2]);
		}
		break;
	case LPWG_FAIL_REASON_CTRL:
		write_buf[0] = MIT_REGH_CMD;
		write_buf[1] = MIT_LPWG_FAIL_REASON_REG;
		write_buf[2] = value;
#ifdef USE_DMA
		if (i2c_dma_write(ts->client, write_buf, 3) != 3) {
#else
		if (i2c_master_send(ts->client, write_buf, 3) != 3) {
#endif
			TOUCH_INFO_MSG("MIT_LPWG_FAIL_REASON_REG write error \n");
		} else {
			TOUCH_INFO_MSG("MIT_LPWG_FAIL_REASON_REG = %d \n", write_buf[2]);
		}
		break;
	default:
		break;
	}

	return 0;
}

static int lpwg_control(struct mms_data* ts, u8 mode)
{
	switch (mode) {
	case LPWG_SIGNATURE:
		break;
	case LPWG_DOUBLE_TAP:
		tci_control(ts, IDLE_REPORTRATE_CTRL, ts->pdata->tci_info->idle_report_rate);
		tci_control(ts, ACTIVE_REPORTRATE_CTRL, ts->pdata->tci_info->active_report_rate);
		tci_control(ts, SENSITIVITY_CTRL, ts->pdata->tci_info->sensitivity);
		tci_control(ts, TCI_ENABLE_CTRL, 1);
		tci_control(ts, TOUCH_SLOP_CTRL, ts->pdata->tci_info->touch_slope);
		tci_control(ts, TAP_MIN_DISTANCE_CTRL, ts->pdata->tci_info->min_distance);
		tci_control(ts, TAP_MAX_DISTANCE_CTRL, ts->pdata->tci_info->max_distance);
		tci_control(ts, MIN_INTERTAP_CTRL, ts->pdata->tci_info->min_intertap);
		tci_control(ts, MAX_INTERTAP_CTRL, ts->pdata->tci_info->max_intertap);
		tci_control(ts, TAP_COUNT_CTRL, ts->pdata->tci_info->tap_count);
		tci_control(ts, INTERRUPT_DELAY_CTRL, 0);
		tci_control(ts, TCI_ENABLE_CTRL2, 0);
		break;
	case LPWG_MULTI_TAP:
		tci_control(ts, IDLE_REPORTRATE_CTRL, ts->pdata->tci_info->idle_report_rate);
		tci_control(ts, ACTIVE_REPORTRATE_CTRL, ts->pdata->tci_info->active_report_rate);
		tci_control(ts, SENSITIVITY_CTRL, ts->pdata->tci_info->sensitivity);
		tci_control(ts, TCI_ENABLE_CTRL, 1);
		tci_control(ts, TOUCH_SLOP_CTRL, ts->pdata->tci_info->touch_slope);
		tci_control(ts, TAP_MIN_DISTANCE_CTRL, ts->pdata->tci_info->min_distance);
		tci_control(ts, TAP_MAX_DISTANCE_CTRL, ts->pdata->tci_info->max_distance);
		tci_control(ts, MIN_INTERTAP_CTRL, ts->pdata->tci_info->min_intertap);
		tci_control(ts, MAX_INTERTAP_CTRL, ts->pdata->tci_info->max_intertap);
		tci_control(ts, TAP_COUNT_CTRL, ts->pdata->tci_info->tap_count);
		tci_control(ts, INTERRUPT_DELAY_CTRL, ts->pdata->double_tap_check);
		tci_control(ts, TCI_ENABLE_CTRL2, 1);
		tci_control(ts, TOUCH_SLOP_CTRL2, ts->pdata->tci_info->touch_slope_2);
		tci_control(ts, TAP_MIN_DISTANCE_CTRL2, ts->pdata->tci_info->min_distance_2);
		tci_control(ts, TAP_MAX_DISTANCE_CTRL2, ts->pdata->tci_info->max_distance_2);
		tci_control(ts, MIN_INTERTAP_CTRL2, ts->pdata->tci_info->min_intertap_2);
		tci_control(ts, MAX_INTERTAP_CTRL2, ts->pdata->tci_info->max_intertap_2);
		tci_control(ts, TAP_COUNT_CTRL2, ts->pdata->tap_count);
		tci_control(ts, INTERRUPT_DELAY_CTRL2, ts->pdata->tci_info->interrupt_delay_2);
		break;
	default:
		tci_control(ts, TCI_ENABLE_CTRL, 0);
		tci_control(ts, TCI_ENABLE_CTRL2, 0);
		break;
	}

	TOUCH_INFO_MSG("%s : lpwg_mode[%d]\n", __func__, mode);
	return 0;
}

static int mms_ic_ctrl(struct i2c_client *client, u32 code, u32 value)
{
	struct mms_data* ts = (struct mms_data *) get_touch_handle_(client);
	struct ic_ctrl_param *param = (struct ic_ctrl_param *) value;
	int ret = 0;
	char *buf = NULL;

	TOUCH_TRACE_FUNC();

	switch (code) {
	case IC_CTRL_FIRMWARE_IMG_SHOW:
		ret = mms_firmware_img_parse_show((const char *) param->v1, (char *) param->v2, param->v3);
		break;

	case IC_CTRL_INFO_SHOW:
		mms_get_ic_info(ts, NULL);
		if (param) {
			buf = (char *) param->v1;
			if (buf) {
				if (ts->pdata->panel_on) {
					ret += sprintf(buf + ret, "====== LCD  ON  ======\n");
				} else {
					ret += sprintf(buf + ret, "====== LCD  OFF ======\n");
				}
				ret += sprintf(buf + ret, "======================\n");
				ret += sprintf(buf + ret, "F/W Version : %X.%02X \n", ts->module.version[0], ts->module.version[1]);
				ret += sprintf(buf + ret, "F/W Product : %s \n", ts->module.product_code);
				ret += sprintf(buf + ret, "F/W Row : %d, Col : %d\n", ts->dev.row_num, ts->dev.col_num);
				if (ts->module.otp == OTP_NOT_SUPPORTED) {
					ret += sprintf(buf + ret, "OTP : F/W Not support \n");
				} else {
					ret += sprintf(buf + ret, "OTP : %s \n", (ts->module.otp == OTP_APPLIED) ? "Applied" : "None");
				}
				ret += sprintf(buf + ret, "======================\n");
			}
		}
		break;

	case IC_CTRL_TESTMODE_VERSION_SHOW:
		if (ts->module.product_code[0])
			TOUCH_INFO_MSG("F/W : %X.%02X (%s)\n", ts->module.version[0], ts->module.version[1], ts->module.product_code);
		if (param) {
			buf = (char *) param->v1;
			if (buf) {
				ret += sprintf(buf + ret, "%X.%02X(%s)\n", ts->module.version[0], ts->module.version[1], ts->module.product_code);
			}
		}
		break;

	case IC_CTRL_SAVE_IC_INFO:
		mms_get_ic_info(ts, NULL);
		break;

	case IC_CTRL_LPWG:
		tci_control(ts, LPWG_PANEL_DEBUG_CTRL, ts->pdata->lpwg_debug_enable);
		tci_control(ts, LPWG_FAIL_REASON_CTRL, ts->pdata->lpwg_fail_reason);
		lpwg_control(ts, (u8)param->v1);
		mms_set_active_area(ts, (u8)param->v1);

		tci_control(ts, LPWG_START_CTRL, 1);
#if defined(TOUCH_USE_DSV)
		if (ts_pdata->enable_sensor_interlock) {
			if (ts_pdata->sensor_value) {
				ts_pdata->use_dsv = 1;
				mms_dsv_control(client);
			}
		} else {
			if (ts_pdata->use_dsv) {
				mms_dsv_control(client);
			}
		}
#endif
		break;

	case IC_CTRL_ACTIVE_AREA:
		mms_set_active_area(ts, (u8)param->v1);
		break;
	}
	return ret;
}

static int mms_reg_control_store(struct i2c_client *client, const char *buf)
{
	struct mms_data *ts = (struct mms_data *) get_touch_handle_(client);
	int cmd = 0;
	int ret = 0;
	int reg_addr[2] = {0};
	int value = 0;
	uint8_t write_buf[50] = {0};
	uint8_t read_buf[50] = {0};
	int i = 0;
	int len = 2;

	if ( sscanf(buf, "%d %x %x %d", &cmd, &reg_addr[0], &reg_addr[1], &value) != 4) {
		TOUCH_INFO_MSG("data parsing fail.\n");
		TOUCH_INFO_MSG("%d, 0x%x, 0x%x, %d\n", cmd, reg_addr[0], reg_addr[1], value);
		return -EINVAL;
	}
	TOUCH_INFO_MSG("%d, 0x%x, 0x%x, %d\n", cmd, reg_addr[0], reg_addr[1], value);

	switch (cmd) {
		case 1:
			write_buf[0] = reg_addr[0];
			write_buf[1] = reg_addr[1];
#ifdef USE_DMA
			ret = i2c_dma_write (ts->client, write_buf,len);
#else
			ret = i2c_master_send(ts->client, write_buf,len);
#endif
			if (ret < 0) {
				TOUCH_INFO_MSG("i2c master send fail\n");
				break;
			}
#ifdef USE_DMA
			ret = i2c_dma_read(ts->client, read_buf, value);
#else
			ret = i2c_master_recv(ts->client, read_buf, value);
#endif
			if (ret < 0) {
				TOUCH_INFO_MSG("i2c master recv fail\n");
				break;
			}
			for (i = 0; i < value; i ++) {
				TOUCH_INFO_MSG("read_buf=[%d]\n",read_buf[i]);
			}
			TOUCH_INFO_MSG("register read done\n");
			break;
		case 2:
			write_buf[0] = reg_addr[0];
			write_buf[1] = reg_addr[1];
			if (value >= 256) {
				write_buf[2] = (value >> 8);
				write_buf[3] = (value & 0xFF);
				len = len + 2;
			} else {
				write_buf[2] = value;
				len++;
			}
#ifdef USE_DMA
			ret = i2c_dma_write (ts->client, write_buf,len);
#else
			ret = i2c_master_send(ts->client, write_buf,len);
#endif
			if (ret < 0) {
				TOUCH_INFO_MSG("i2c master send fail\n");
				break;
			}
			TOUCH_INFO_MSG("register write done\n");
			break;
		default:
			TOUCH_INFO_MSG("usage: echo [1(read)|2(write)], [reg address0], [reg address1], [length(read)|value(write)] > reg_control\n");
			TOUCH_INFO_MSG("  - Register Set or Read\n");
			break;
	}
	return ret;
}

static int mit_tci_store(struct i2c_client *client, const char *buf)
{
	struct mms_data *ts = (struct mms_data *) get_touch_handle_(client);
	int ret = 0;
	int value = 0;
	int type = 0;

	if (sscanf(buf, "%d %d", &type, &value) != 2) {
		TOUCH_INFO_MSG("data parsing fail.\n");
		TOUCH_INFO_MSG("%d, %d\n", type, value);
		return -EINVAL;
	}

	TOUCH_INFO_MSG("%s - TCI reg control type : %d, value : %d\n", __func__, type, value);

	switch(type) {
		case IDLE_REPORTRATE_CTRL:
			ts->pdata->tci_info->idle_report_rate = value;
		break;
	case ACTIVE_REPORTRATE_CTRL:
			ts->pdata->tci_info->active_report_rate =value;
		break;
	case SENSITIVITY_CTRL:
			ts->pdata->tci_info->sensitivity = value;
		break;
	case TCI_ENABLE_CTRL:
		TOUCH_INFO_MSG("You can't control TCI_ENABLE_CTRL register\n");
		return 0;
	case TOUCH_SLOP_CTRL:
		ts->pdata->tci_info->touch_slope = value;
		break;
	case TAP_MIN_DISTANCE_CTRL:
		ts->pdata->tci_info->min_distance = value;
		break;
	case TAP_MAX_DISTANCE_CTRL:
		ts->pdata->tci_info->max_distance = value;
		break;
	case MIN_INTERTAP_CTRL:
		ts->pdata->tci_info->min_intertap = value;
		break;
	case MAX_INTERTAP_CTRL:
		ts->pdata->tci_info->max_intertap = value;
		break;
	case TAP_COUNT_CTRL:
		ts->pdata->tci_info->tap_count = value;
		break;
	case INTERRUPT_DELAY_CTRL:
		TOUCH_INFO_MSG("You can't control INTERRUPT_DELAY_CTRL register\n");
		return 0;
	case TCI_ENABLE_CTRL2:
		TOUCH_INFO_MSG("You can't control TCI_ENABLE_CTRL2 register\n");
		return 0;
	case TOUCH_SLOP_CTRL2:
		ts->pdata->tci_info->touch_slope_2 = value;
		break;
	case TAP_MIN_DISTANCE_CTRL2:
		ts->pdata->tci_info->min_distance_2 = value;
		break;
	case TAP_MAX_DISTANCE_CTRL2:
		ts->pdata->tci_info->max_distance_2 = value;
		break;
	case MIN_INTERTAP_CTRL2:
		ts->pdata->tci_info->min_intertap_2 = value;
		break;
	case MAX_INTERTAP_CTRL2:
		ts->pdata->tci_info->max_intertap_2 = value;
		break;
	case TAP_COUNT_CTRL2:
		TOUCH_INFO_MSG("You can't control TAP_COUNT_CTRL2 register\n");
		return 0;
	case INTERRUPT_DELAY_CTRL2:
		ts->pdata->tci_info->interrupt_delay_2 = value;
		break;
	case LPWG_STORE_INFO_CTRL:
		TOUCH_INFO_MSG("You can't control LPWG_STORE_INFO_CTRL register\n");
		return 0;
	case LPWG_START_CTRL:
		TOUCH_INFO_MSG("You can't control LPWG_START_CTRL register\n");
		return 0;
	default:
		TOUCH_INFO_MSG("incorrect command\n");
		return 0;
	}

	tci_control(ts,type,value);

	return ret;
}

static ssize_t mms_rawdata_show(struct i2c_client *client, char *buf)
{
	struct mms_data *ts = (struct mms_data *) get_touch_handle_(client);
	int ret = 0;
	ts->pdata->selfdiagnostic_state[SD_RAWDATA] = 1;	// rawdata

	TOUCH_TRACE_FUNC();

	ret = mit_get_test_result(client, buf, RAW_DATA_SHOW);
	if (ret < 0) {
		memset(buf, 0, PAGE_SIZE);
		ret = snprintf(buf, PAGE_SIZE, "failed to get raw data\n");
	}
	return ret;
}

static ssize_t mms_rawdata_store(struct i2c_client *client, const char *buf)
{
	int ret = 0;
	char temp_buf[255];
	TOUCH_TRACE_FUNC();
	strcpy(temp_buf,buf);

	ret = mit_get_test_result(client, temp_buf, RAW_DATA_STORE);

	return ret;
}

static ssize_t mit_chstatus_show(struct i2c_client *client, char *buf)
{
	int ret = 0;
	int len = 0;
	struct mms_data *ts = (struct mms_data *) get_touch_handle_(client);
	ts->pdata->selfdiagnostic_state[SD_OPENSHORT] = 1;	// openshort
	ts->pdata->selfdiagnostic_state[SD_SLOPE] = 1;	// slope

	TOUCH_TRACE_FUNC();
	TOUCH_INFO_MSG("mit_chstatus_show\n");

	ret = mit_get_test_result(client, buf, OPENSHORT);
	memset(buf, 0, PAGE_SIZE);
	if (ret < 0) {
		TOUCH_INFO_MSG("Failed to get OPEN SHORT Test result. \n");
		ret = snprintf(buf, PAGE_SIZE, "failed to OPEN SHORT data\n");
		goto error;
	}

	ret = mit_get_test_result(client, buf, SLOPE);
	memset(buf, 0, PAGE_SIZE);
	if (ret < 0) {
		TOUCH_INFO_MSG("Failed to get SLOPE Test result. \n");
		ret = snprintf(buf, PAGE_SIZE, "failed to SLOPE data\n");
		goto error;
	}

	len = snprintf(buf, PAGE_SIZE - len, "Firmware Version : %X.%02X \n", ts->module.version[0], ts->module.version[1]);
	len += snprintf(buf + len, PAGE_SIZE - len, "FW Product : %s \n", ts->module.product_code);
	len += snprintf(buf + len, PAGE_SIZE - len, "=======RESULT========\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "OPEN /  SHORT Test : %s\n", ts->pdata->selfdiagnostic_state[SD_OPENSHORT]==1 ? "PASS" : "FAIL");
	len += snprintf(buf + len, PAGE_SIZE - len, "SLOPE Test : %s\n", ts->pdata->selfdiagnostic_state[SD_SLOPE] == 1 ? "PASS" : "FAIL");

	return len;

error:
	return ret;
}

static ssize_t mit_chstatus_store(struct i2c_client *client, const char *buf)
{
	int ret = 0;
	char temp_buf[255];
	TOUCH_TRACE_FUNC();
	strcpy(temp_buf,buf);

	ret = mit_get_test_result(client, temp_buf, OPENSHORT_STORE);

	return ret;
}

static int melfas_delta_show(struct i2c_client* client, char *buf)
{
	int ret = 0;

	TOUCH_TRACE_FUNC();

	ret = mit_delta_show(client, buf);

	return ret;
}

static ssize_t mms_self_diagnostic_show(struct i2c_client *client, char *buf)
{
	struct mms_data *ts = (struct mms_data *) get_touch_handle_(client);
	int len = 0;
	int ret = 0;
	int row = 0;
	int col = 0;
	u32 limit_upper = 0;
	u32 limit_lower = 0;
	char *sd_path = "/sdcard/touch_self_test.txt";
	ts->pdata->selfdiagnostic_state[SD_RAWDATA] = 1;	// rawdata
	ts->pdata->selfdiagnostic_state[SD_OPENSHORT] = 1;	// openshort
	ts->pdata->selfdiagnostic_state[SD_SLOPE] = 1;	// slope

	mit_get_otp(ts);

	write_file(sd_path, buf, 1);
	msleep(30);

	ret = mit_get_test_result(client, buf, OPENSHORT);
	if (ret < 0) {
		TOUCH_ERR_MSG("failed to get open short data\n");
		memset(buf, 0, PAGE_SIZE);
		ts->o_max = 0;
		ts->o_min = 0;
		ts->pdata->selfdiagnostic_state[SD_OPENSHORT] = 0;
		len += snprintf(buf, PAGE_SIZE, "failed to get open short data\n\n");
	}
	write_file(sd_path, buf, 0);
	msleep(30);

	memset(buf, 0, PAGE_SIZE);
	ret = mit_get_test_result(client, buf, SLOPE);
	if (ret < 0) {
		TOUCH_ERR_MSG("failed to get slope data\n");
		memset(buf, 0, PAGE_SIZE);
		ts->s_max = 0;
		ts->s_min = 0;
		ts->pdata->selfdiagnostic_state[SD_SLOPE] = 0;
		len = snprintf(buf, PAGE_SIZE, "failed to get slope data\n\n");
	}
	write_file(sd_path, buf, 0);
	msleep(30);

	memset(buf, 0, PAGE_SIZE);
	ret = mit_get_test_result(client, buf, RAW_DATA_SHOW);
	if (ret < 0) {
		TOUCH_ERR_MSG("failed to get raw data\n");
		memset(buf, 0, PAGE_SIZE);
		ts->r_max = 0;
		ts->r_min = 0;
		ts->pdata->selfdiagnostic_state[SD_RAWDATA] = 0;
		ret = snprintf(buf, PAGE_SIZE, "failed to get raw data\n\n");
	}

	if (ts->module.otp == OTP_APPLIED) {
		limit_upper = ts->pdata->limit->raw_data_otp_max + ts->pdata->limit->raw_data_margin;
		limit_lower = ts->pdata->limit->raw_data_otp_min - ts->pdata->limit->raw_data_margin;
		ret += sprintf(buf+ret,"RAW DATA SPEC (UPPER : %d  LOWER : %d  MARGIN : %d)\n",
			ts->pdata->limit->raw_data_otp_max , ts->pdata->limit->raw_data_otp_min, ts->pdata->limit->raw_data_margin);
		TOUCH_INFO_MSG("RAW DATA SPEC (UPPER : %d  LOWER : %d MARGIN : %d)\n",
			ts->pdata->limit->raw_data_otp_max , ts->pdata->limit->raw_data_otp_min, ts->pdata->limit->raw_data_margin);
	} else {
		limit_upper = ts->pdata->limit->raw_data_max + ts->pdata->limit->raw_data_margin;
		limit_lower = ts->pdata->limit->raw_data_min - ts->pdata->limit->raw_data_margin;
		ret += sprintf(buf+ret,"RAW DATA SPEC (UPPER : %d  LOWER : %d MARGIN : %d)\n",
			ts->pdata->limit->raw_data_max , ts->pdata->limit->raw_data_min, ts->pdata->limit->raw_data_margin);
		TOUCH_INFO_MSG("RAW DATA SPEC (UPPER : %d  LOWER : %d MARGIN : %d)\n",
			ts->pdata->limit->raw_data_max , ts->pdata->limit->raw_data_min, ts->pdata->limit->raw_data_margin);
	}

	if (ts->pdata->selfdiagnostic_state[SD_RAWDATA] == 0) {

		for (row = 0 ; row < MAX_ROW; row++) {
				ret += sprintf(buf+ret,"[%2d]  ",row);
				printk("[Touch] [%2d]  ",row);

			for (col = 0 ; col < MAX_COL ; col++) {

					if (ts->mit_data[row][col] <= limit_upper && ts->mit_data[row][col] >= limit_lower ){
							ret += sprintf(buf+ret," ,");
							printk(" ,");
						}else{
							ret += sprintf(buf+ret,"X,");
							printk("X,");
						}
			}
				printk("\n");
				ret += sprintf(buf+ret,"\n");
		}
		ret += sprintf(buf+ret,"RawData : FAIL\n\n");
		TOUCH_INFO_MSG("RawData : FAIL\n\n");
	}else {
		ret += sprintf(buf+ret,"RawData : PASS\n\n");
		TOUCH_INFO_MSG("RawData : PASS\n\n");
	}
	write_file(sd_path, buf, 0);
	msleep(30);

	TOUCH_INFO_MSG("Firmware Version : %X.%02X \n", ts->module.version[0], ts->module.version[1]);
	TOUCH_INFO_MSG("FW Product : %s \n", ts->module.product_code);


	if (ts->module.otp == OTP_NOT_SUPPORTED) {
		TOUCH_INFO_MSG("OTP : F/W Not support \n");
	} else {
		TOUCH_INFO_MSG("OTP : %s \n", (ts->module.otp == OTP_APPLIED) ? "Applied" : "None");
	}
	TOUCH_INFO_MSG("=====================\n");
	if (ts->pdata->check_openshort)
		TOUCH_INFO_MSG("OpenShort : %5d , %5d\n", ts->o_max, ts->o_min);
	TOUCH_INFO_MSG("Slope     : %5d , %5d\n", ts->s_max, ts->s_min);
	TOUCH_INFO_MSG("Rawdata   : %5d , %5d\n", ts->r_max, ts->r_min);
	TOUCH_INFO_MSG("=======RESULT========\n");
	TOUCH_INFO_MSG("Channel Status : %s\n", (ts->pdata->selfdiagnostic_state[SD_OPENSHORT] * ts->pdata->selfdiagnostic_state[SD_SLOPE]) == 1 ? "PASS" : "FAIL");
	TOUCH_INFO_MSG("Raw Data : %s\n", ts->pdata->selfdiagnostic_state[SD_RAWDATA] == 1 ? "PASS" : "FAIL");

	memset(buf, 0, PAGE_SIZE);
	len = snprintf(buf, PAGE_SIZE , "Firmware Version : %X.%02X \n", ts->module.version[0], ts->module.version[1]);
	len += snprintf(buf + len, PAGE_SIZE - len, "FW Product : %s \n", ts->module.product_code);
	if (ts->module.otp == OTP_NOT_SUPPORTED) {
		len += snprintf(buf + len, PAGE_SIZE - len, "OTP : F/W Not support \n");
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "OTP : %s \n", (ts->module.otp == OTP_APPLIED) ? "Applied" : "None");
	}
	len += snprintf(buf + len, PAGE_SIZE - len, "=====================\n");
	if (ts->pdata->check_openshort)
		len += snprintf(buf + len, PAGE_SIZE - len, "OpenShort : %5d , %5d\n", ts->o_max, ts->o_min);
	len += snprintf(buf + len, PAGE_SIZE - len, "Slope     : %5d , %5d\n", ts->s_max, ts->s_min);
	len += snprintf(buf + len, PAGE_SIZE - len, "Rawdata   : %5d , %5d\n", ts->r_max, ts->r_min);
	len += snprintf(buf + len, PAGE_SIZE - len, "=======RESULT========\n");
	len += snprintf(buf + len, PAGE_SIZE - len, "Channel Status : %s\n", (ts->pdata->selfdiagnostic_state[SD_OPENSHORT] * ts->pdata->selfdiagnostic_state[SD_SLOPE]) == 1 ? "PASS" : "FAIL");
	len += snprintf(buf + len, PAGE_SIZE - len, "Raw Data : %s\n", ts->pdata->selfdiagnostic_state[SD_RAWDATA] == 1 ? "PASS" : "FAIL");
	write_file(sd_path, buf, 0);
	return len;
}

static int mms_sensing_block_control(struct i2c_client *client, u8 type, u8 onoff)
{
	struct mms_data* ts = get_touch_handle_(client);
	u8 wbuf[6] = {32, 0, 81, 1, 0, 0};
	int i = 0;

	switch(type) {
		case 0 :
			break;
		case 81 :
		case 82 :
		case 83 :
			wbuf[2] = (u8)type;
			break;
		default :
			TOUCH_INFO_MSG("not support %d \n", type);
			return 0;
	}

	if (onoff)
		wbuf[4] = 1;
	else
		wbuf[4] = 0;

	if (type == 0) {
		for (i = 81; i <= 83; i++) {
			wbuf[2] = i;
#ifdef USE_DMA
			i2c_master_send(ts->client, wbuf, 6);
#else
			i2c_master_send(ts->client, wbuf, 6);
#endif
			TOUCH_INFO_MSG("Sensing Block (%d) : %s \n", wbuf[2], onoff ? "On" : "Off");
		}
	} else {
#ifdef USE_DMA
			i2c_master_send(ts->client, wbuf, 6);
#else
			i2c_master_send(ts->client, wbuf, 6);
#endif
		TOUCH_INFO_MSG("Sensing Block (%d) : %s \n", wbuf[2], onoff ? "On" : "Off");
	}

	wbuf[0] = 0x1F;
	wbuf[1] = 0xFF;
	wbuf[2] = 1;
#ifdef USE_DMA
			i2c_master_send(ts->client, wbuf, 3);
#else
			i2c_master_send(ts->client, wbuf, 3);
#endif


	if (onoff) {
		touch_disable(ts->client->irq);
		mms_power_reset(ts);
		touch_enable(ts->client->irq);
	}

	return 0;
}

static ssize_t mms_fw_dump_show(struct i2c_client *client, char *buf)
{
	struct mms_data *ts = (struct mms_data *) get_touch_handle_(client);
	int len = 0;
	u8 *pDump = NULL;
	int readsize = 0;
	int addr = 0;
	int retrycnt = 0;
	int fd = 0;
	char *dump_path = "/sdcard/touch_dump.fw";
	mm_segment_t old_fs = get_fs();

	TOUCH_INFO_MSG("F/W Dumping... \n");

	touch_disable(ts->client->irq);

	pDump = kzalloc(FW_MAX_SIZE, GFP_KERNEL);

RETRY :
	readsize = 0;
	retrycnt++;
	mms_power_reset(ts);
	msleep(50);

	for (addr = 0; addr < FW_MAX_SIZE; addr += FW_BLOCK_SIZE ) {
		if ( mit_isc_page_read(ts, &pDump[addr], addr) ) {
			TOUCH_INFO_MSG("F/W Read failed \n");
			if (retrycnt > 10) {
				len += snprintf(buf + len, PAGE_SIZE - len, "dump failed \n");
				goto EXIT;
			}
			else
				goto RETRY;
		}

		readsize += FW_BLOCK_SIZE;
		if (readsize % (FW_BLOCK_SIZE * 20) == 0) {
			TOUCH_INFO_MSG("\t Dump %5d / %5d bytes\n", readsize, FW_MAX_SIZE);
		}
	}

	TOUCH_INFO_MSG("\t Dump %5d / %5d bytes\n", readsize, FW_MAX_SIZE);

	set_fs(KERNEL_DS);
	fd = sys_open(dump_path, O_WRONLY|O_CREAT, 0666);
	if (fd >= 0) {
		sys_write(fd, pDump, FW_MAX_SIZE);
		sys_close(fd);
		len += snprintf(buf + len, PAGE_SIZE - len, "%s saved \n", dump_path);
	} else {
		len += snprintf(buf + len, PAGE_SIZE - len, "%s open failed \n", dump_path);
	}

	set_fs(old_fs);

EXIT :
	kfree(pDump);

	mit_isc_exit(ts);

	mms_power_reset(ts);

	touch_enable(ts->client->irq);

	return len;
}

static ssize_t mms_lpwg_show(struct i2c_client *client, char *buf)
{
	struct mms_data *ts = (struct mms_data *) get_touch_handle_(client);
	int len = 0;

	len += snprintf(buf + len, PAGE_SIZE - len, "LPWG Mode : %X (0:None, 1:Knock-On, 10, Knock-Code) \n", ts->pdata->lpwg_mode);

	return len;
}

static ssize_t mms_lpwg_store(struct i2c_client *client, char* buf1, const char *buf2 )
{
	struct mms_data *ts = (struct mms_data *) get_touch_handle_(client);
	int mode = LPWG_NONE;
	int tap_count = 0;
	int tap_check = 0;

	/* set tap_count */
	if (buf1 != NULL && !strcmp(buf1,"tap_count")) {
		sscanf(buf2, "%d" ,&tap_count);
		ts->pdata->tap_count = tap_count;
		TOUCH_INFO_MSG("Set Touch Tap Count  = %d \n", ts->pdata->tap_count);
		return 0;
	}

	/* set active area */
	if (buf1 != NULL && !strcmp(buf1,"area")) {
		ts->pdata->active_area_x1 = ts->pdata->active_area_gap;
		ts->pdata->active_area_x2 = ts->pdata->caps->lcd_x - ts->pdata->active_area_gap;
		ts->pdata->active_area_y1 = ts->pdata->active_area_gap;
		ts->pdata->active_area_y2 = ts->pdata->caps->lcd_y - ts->pdata->active_area_gap;
		TOUCH_DEBUG_MSG("Active Area - X1:%d, X2:%d, Y1:%d, Y2:%d\n",ts->pdata->active_area_x1, ts->pdata->active_area_x2, ts->pdata->active_area_y1, ts->pdata->active_area_y2);
		return 0;
	}

	/* set double tap check */
	if (buf1 != NULL && !strcmp(buf1,"tap_check")) {
		sscanf(buf2, "%d" ,&tap_check);
		ts->pdata->double_tap_check = tap_check;
		TOUCH_INFO_MSG("Double Tap Check  = %d \n", ts->pdata->double_tap_check);
		tci_control(ts, INTERRUPT_DELAY_CTRL, ts->pdata->double_tap_check);
		return 0;
	}

	if (buf1 != NULL && !strcmp(buf1,"update_all")) {
		/* set lpwg mode */
		if (buf2 == NULL) {
			TOUCH_INFO_MSG(" mode is NULL, Can't not set LPWG\n");
			return 0;
		}
		sscanf(buf2, "%X", &mode);
		ts->pdata->lpwg_mode = (u8)mode;

		/* Proximity Sensor on/off */
		if (ts->pdata->panel_on == 0 && ts->pdata->lpwg_panel_on == 0) {
			TOUCH_INFO_MSG("SUSPEND AND SET\n");
			if (!ts->pdata->lpwg_mode && !ts->pdata->lpwg_prox) {
				touch_disable_wake(ts->client->irq);
				touch_disable(ts->client->irq);
				if (wake_lock_active(&touch_wake_lock))
					wake_unlock(&touch_wake_lock);
				mms_power_ctrl(client, ts_role->suspend_pwr);
				atomic_set(&dev_state,DEV_SUSPEND);
				TOUCH_INFO_MSG("SUSPEND AND SET power off\n");
#if defined(TOUCH_USE_DSV)
				if (ts_pdata->enable_sensor_interlock) {
					ts->pdata->use_dsv = 0;
					mms_dsv_control(ts->client);
				}
#endif
			} else {
				mms_power_ctrl(client, ts_role->resume_pwr);
				wake_lock_timeout(&touch_wake_lock, msecs_to_jiffies(1000));
				mms_ic_ctrl(client, IC_CTRL_LPWG, (u32)&(ts->pdata->lpwg_mode));
				atomic_set(&dev_state,DEV_RESUME_ENABLE);
				touch_enable(ts->client->irq);
				touch_enable_wake(ts->client->irq);
				TOUCH_INFO_MSG("SUSPEND AND SET power on\n");
			}
		} else {
			TOUCH_INFO_MSG("PANEL ON \n");
		}
	}
	TOUCH_INFO_MSG("%s %X \n", __func__, ts->pdata->lpwg_mode);

	return 0;
}

static ssize_t mit_keyguard_info_store(struct i2c_client *client, const char *buf )
{
#if defined(TOUCH_USE_DSV)
	struct mms_data *ts = (struct mms_data *) get_touch_handle_(client);
#endif
	int value;

	if (sscanf(buf, "%d", &value) != 1)
		return -EINVAL;

	switch(value) {
		case KEYGUARD_RESERVED:
			lockscreen_stat = 0;
			TOUCH_INFO_MSG("%s : Lockscreen unlocked, lockscreen_stat = %d\n", __func__, lockscreen_stat);

#if defined(TOUCH_USE_DSV)
			ts->pdata->sensor_value = 0;
			if (ts->pdata->enable_sensor_interlock) {
				ts->pdata->use_dsv = 0;
			}
#endif
			break;
		case KEYGUARD_ENABLE:
			lockscreen_stat = 1;
			TOUCH_INFO_MSG("%s : Lockscreen locked, lockscreen_stat = %d\n", __func__, lockscreen_stat);
			break;
		default:
			break;
	}

	return 0;
}

static int mms_sysfs(struct i2c_client *client, char *buf1, const char *buf2, u32 code)
{
	int ret = 0;
	struct ic_ctrl_param param;
	struct mms_data *ts = (struct mms_data *) get_touch_handle_(client);
	int type = 0;
	int onoff = 0;

	TOUCH_TRACE_FUNC();

	if (code != SYSFS_TESTMODE_VERSION_SHOW && code != SYSFS_KEYGUARD_STORE) {
		power_lock(POWER_SYSFS_LOCK);
		mms_power(client, POWER_ON);
		msleep(30);
	} else {
		TOUCH_INFO_MSG("[%s] Ignore power on.\n", __func__);
	}

	switch (code) {
	case SYSFS_VERSION_SHOW :
		param.v1 = (u32) buf1;
		ret = mms_ic_ctrl(client, IC_CTRL_INFO_SHOW, (u32) &param);
		break;
	case SYSFS_TESTMODE_VERSION_SHOW :
		param.v1 = (u32) buf1;
		ret = mms_ic_ctrl(client, IC_CTRL_TESTMODE_VERSION_SHOW, (u32) &param);
		break;
	case SYSFS_REG_CONTROL_STORE:
		ret = mms_reg_control_store(client, buf2);
		break;
	case SYSFS_LPWG_TCI_STORE:
		ret = mit_tci_store(client, buf2);
		break;
	case SYSFS_CHSTATUS_SHOW:
		touch_disable(ts->client->irq);
		ret = mit_chstatus_show(client, buf1);
		touch_enable(ts->client->irq);
		break;
	case SYSFS_CHSTATUS_STORE:
		touch_disable(ts->client->irq);
		ret = mit_chstatus_store(client, buf2);
		touch_enable(ts->client->irq);
		break;
	case SYSFS_RAWDATA_SHOW:
		touch_disable(ts->client->irq);
		ret = mms_rawdata_show(client, buf1);
		touch_enable(ts->client->irq);
		break;
	case SYSFS_RAWDATA_STORE:
		touch_disable(ts->client->irq);
		ret = mms_rawdata_store(client, buf2);
		touch_enable(ts->client->irq);
		break;
	case SYSFS_DELTA_SHOW:
		ret = melfas_delta_show(client, buf1);
		break;
	case SYSFS_SELF_DIAGNOSTIC_SHOW:
		touch_disable(ts->client->irq);
		ret = mms_self_diagnostic_show(client, buf1);
		touch_enable(ts->client->irq);
		break;
	case SYSFS_SENSING_ALL_BLOCK_CONTROL :
		sscanf(buf1, "%d", &onoff);
		type = 0;
		ret = mms_sensing_block_control(client, (u8)type, (u8)onoff);
		break;
	case SYSFS_SENSING_BLOCK_CONTROL :
		sscanf(buf1, "%d", &onoff);
		sscanf(buf2, "%d", &type);
		ret = mms_sensing_block_control(client, (u8)type, (u8)onoff);
		break;
	case SYSFS_FW_DUMP :
		ret = mms_fw_dump_show(client, buf1);
		break;
	case SYSFS_LPWG_SHOW :
		ret = mms_lpwg_show(client, buf1);
		break;
	case SYSFS_LPWG_STORE :
		ret = mms_lpwg_store(client, buf1 ,buf2);
		break;
	case SYSFS_KEYGUARD_STORE :
		ret = mit_keyguard_info_store(client, buf2);
		break;
	case SYSFS_LPWG_DEBUG_STORE:
		tci_control(ts, LPWG_PANEL_DEBUG_CTRL, ts->pdata->lpwg_debug_enable);
		break;
	case SYSFS_LPWG_REASON_STORE:
		tci_control(ts, LPWG_FAIL_REASON_CTRL, ts->pdata->lpwg_fail_reason);
		break;
	}

	if (code != SYSFS_TESTMODE_VERSION_SHOW && code != SYSFS_KEYGUARD_STORE) {
		power_unlock(POWER_SYSFS_LOCK);
	}
	return ret;
}

enum window_status mms_check_crack(struct i2c_client *client)
{
	int ret = NO_CRACK;
	int result = 0;
	char buf[4] = {0,};
	struct mms_data *ts = (struct mms_data *) get_touch_handle_(client);

	TOUCH_TRACE_FUNC();

	touch_disable(ts->client->irq);
	ts->count_short = 0;
	result = mit_get_test_result(client, buf, CRACK_CHECK);
	touch_enable(ts->client->irq);

	if (result < 0 || ts->count_short > CRACK_SPEC) {
		ret = CRACK;
		TOUCH_INFO_MSG("%s crack_result = %d, count_short = %d\n", __func__, result, ts->count_short);
	}
	return ret;
}

struct touch_device_driver mms_driver = {
	.probe = mms_probe,
	.remove = mms_remove,
	.init = mms_init,
	.data = mms_get_data,
	.power = mms_power,
	.fw_upgrade = mms_fw_upgrade,
	.ic_ctrl = mms_ic_ctrl,
	.sysfs = mms_sysfs,
	.inspection_crack = mms_check_crack,
#if defined(TOUCH_USE_DSV)
	.dsv_control = mms_dsv_control,
#endif
};

static void async_touch_init(void *data, async_cookie_t cookie)
{
	touch_driver_register_(&mms_driver);
	return;
}

static int __init touch_init(void)
{
	TOUCH_TRACE_FUNC();
	
	async_schedule(async_touch_init, NULL);

	return 0;
}

static void __exit touch_exit(void)
{
	TOUCH_TRACE_FUNC();
	touch_driver_unregister_();
}

module_init(touch_init);
module_exit(touch_exit);

MODULE_AUTHOR("WX-BSP-TS@lge.com");
MODULE_DESCRIPTION("LGE Touch Driver");
MODULE_LICENSE("GPL");

