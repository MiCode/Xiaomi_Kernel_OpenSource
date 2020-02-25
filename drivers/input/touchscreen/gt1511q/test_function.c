/* drivers/input/touchscreen/test_function.c
 *
 * 2010 - 2014 Shenzhen Huiding Technology Co.,Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be a reference
 * to you, when you are integrating the GOODiX's CTP IC into your system,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * Version: 1.0
 * Release Date: 2014/11/14
 *
 */

#ifdef __cplusplus
//extern "C" {
#endif

#include <linux/string.h>
#ifdef __cplusplus
#include "tool.h"
#endif

#include "test_function.h"
#include "gt1x_generic.h"

#if !GTP_TEST_PARAMS_FROM_INI
#include "test_params.h"
#endif

#include "test_params.h"

/*----------------------------------- test param variable define-----------------------------------*/
/* sample data frame numbers */
int samping_set_number = 16;

/* raw data max threshold */
int *max_limit_value;
int max_limit_value_tmp;

/* raw data min threshold */
int *min_limit_value;
int min_limit_value_tmp;

/* ratio threshold between adjacent(up down right left) data */
long *accord_limit;
long accord_limit_tmp;
long *accord_line_limit;

/* maximum deviation ratio,|(data - average)|/average */
long offset_limit = 150;

/* uniformity = minimum / maximum */
long uniformity_limit;

/* further judgement for not meet the entire screen offset limit of the channel,
   when the following conditions are satisfied that the legitimate:
   1 beyond the entire screen offset limit but not beyond the limits ,
   2 to meet the condition of 1 point number no more than
   3 3 of these two non adjacent */
long special_limit = 250;

/* maximum jitter the entire screen data value */
int permit_jitter_limit = 20;

/* key raw data max threshold */
int *max_key_limit_value;
int max_key_limit_value_tmp;

/* key raw data min threshold */
int *min_key_limit_value;
int min_key_limit_value_tmp;

int ini_module_type;

unsigned char *ini_version1;

unsigned char *ini_version2;

unsigned short gt900_short_threshold = 10;
unsigned short gt900_short_dbl_threshold = 500;
unsigned short gt900_drv_drv_resistor_threshold = 500;;
unsigned short gt900_drv_sen_resistor_threshold = 500;
unsigned short gt900_sen_sen_resistor_threshold = 500;
unsigned short gt900_resistor_warn_threshold = 1000;
unsigned short gt900_drv_gnd_resistor_threshold = 400;
unsigned short gt900_sen_gnd_resistor_threshold = 400;
unsigned char gt900_short_test_times = 1;

long tri_pattern_r_ratio = 115;
int sys_avdd = 28;

/*----------------------------------- test input variable define-----------------------------------*/
unsigned short *current_data_temp;

unsigned short *channel_max_value;

unsigned short *channel_min_value;

/* average value of channel */
int *channel_average;

/* average current value of each channel of the square */
int *channel_square_average;

/* the test group number */
int current_data_index;

unsigned char *need_check;

unsigned char *channel_key_need_check;

/* max sample data number */
//int samping_num = 64;
int samping_num = 16;

u8 *global_large_buf;

unsigned char *driver_status;
unsigned char *upload_short_data;

/*----------------------------------- test output variable define-----------------------------------*/
int test_error_code;

/* channel status,0 is normal,otherwise is anormaly */
unsigned short *channel_status;

long *channel_max_accord;

long *channel_max_line_accord;

/* maximum number exceeds the set value */
unsigned char *beyond_max_limit_num;

/*  minimum number exceeds the set value */
unsigned char *beyond_min_limit_num;

/* deviation ratio exceeds the set value. */
unsigned char *beyond_accord_limit_num;

/* full screen data maximum deviation ratio exceeds the set value */
unsigned char *beyond_offset_limit_num;

/* maximum frequency jitter full screen data exceeds a set value */
unsigned char *beyond_jitter_limit_num;

/* the number of data consistency over the setting value */
unsigned char beyond_uniformity_limit_num;

/*----------------------------------- other define-----------------------------------*/
char save_result_dir[250] = "/sdcard/";
char ini_find_dir1[250] = "/data/";
char ini_find_dir2[250] = "/data/data/com.goodix.rawdata/files/";
char ini_format[250] = "0";
char save_data_path[250];

u8 *original_cfg;
u8 *module_cfg;

s32 _node_in_key_chn(u16 node, u8 *config, u16 keyoffest)
{
	int ret = -1;
	u8 i, tmp, chn;
	if (node < sys.sc_sensor_num * sys.sc_driver_num) {
		return INVALID;
	}

	chn = node - sys.sc_sensor_num * sys.sc_driver_num;
	for (i = 0; i < 4; i++) {
		tmp = config[keyoffest + i];
		if ((tmp != 0) && (tmp % 8 != 0)) {
			return 1;
		}

		if (tmp == (chn + 1) * 8) {
			ret = 1;
		}
	}

	return ret;
}

static void _get_channel_min_value(void)
{
	int i;
	for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
		if (current_data_temp[i] < channel_min_value[i]) {
			channel_min_value[i] = current_data_temp[i];
		}
	}
}

static void _get_channel_max_value(void)
{
	int i;
	for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
		if (current_data_temp[i] > channel_max_value[i]) {
			channel_max_value[i] = current_data_temp[i];
		}
	}
}

static void _get_channel_average_value(void)
{
	int i;
	if (current_data_index == 0) {
		memset(channel_average, 0, sizeof(channel_average) / sizeof(channel_average[0]));
		memset(channel_average, 0, sizeof(channel_square_average) / sizeof(channel_square_average[0]));
	}

	for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
		channel_average[i] += current_data_temp[i] / samping_set_number;
		channel_square_average[i] += (current_data_temp[i] * current_data_temp[i]) / samping_set_number;
	}
}

static unsigned short _get_average_value(unsigned short *data)
{
	int i;
	int temp = 0;
	int not_check_num = 0;
	unsigned short average_temp = 0;
	for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
		if (need_check[i] == _NEED_NOT_CHECK) {
			not_check_num++;
			continue;
		}
		temp += data[i];
	}

	GTP_ERROR("NOT CHECK NUM:%d\ntmp:%d\n", not_check_num, temp);
	if (not_check_num < sys.sc_sensor_num * sys.sc_driver_num) {
		average_temp = (unsigned short)(temp / (sys.sc_sensor_num * sys.sc_driver_num - not_check_num));
	}
	return average_temp;
}

#ifdef SIX_SIGMA_JITTER
static float _get_six_sigma_value(void)
{
	int i;
	float square_sigma = 0;
	for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
		square_sigma += channel_square_average[i] - (channel_average[i] * channel_average[i]);
	}
	square_sigma /= samping_set_number;
	return sqrt(square_sigma);
}
#endif
static unsigned char _check_channel_min_value(void)
{
	int i;
	unsigned char test_result = 1;
	for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
		if (need_check[i] == _NEED_NOT_CHECK) {
			continue;
		}

		if (current_data_temp[i] < min_limit_value[i]) {
			channel_status[i] |= _BEYOND_MIN_LIMIT;
			test_error_code |= _BEYOND_MIN_LIMIT;
			beyond_min_limit_num[i]++;
			test_result = 0;
			GTP_ERROR("current[%d]%d,min_limit[%d]%d", i, current_data_temp[i], i, min_limit_value[i]);
		}
	}

	return test_result;
}

static unsigned char _check_channel_max_value(void)
{
	int i;
	unsigned char test_result = 1;
	for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
		if (need_check[i] == _NEED_NOT_CHECK) {
			continue;
		}

		if (current_data_temp[i] > max_limit_value[i]) {
			channel_status[i] |= _BEYOND_MAX_LIMIT;
			test_error_code |= _BEYOND_MAX_LIMIT;
			beyond_max_limit_num[i]++;
			test_result = 0;
			GTP_ERROR("current[%d]%d,max_limit[%d]%d", i, current_data_temp[i], i, max_limit_value[i]);
		}
	}

	return test_result;
}

static unsigned char _check_key_min_value(void)
{
	int i, j;
	unsigned char test_result = 1;
	if (sys.key_number == 0) {
		return test_result;
	}

	for (i = sys.sc_sensor_num * sys.sc_driver_num, j = 0; i < sys.sc_sensor_num * sys.sc_driver_num + sys.key_number; i++) {
		if (channel_key_need_check[i - sys.sc_sensor_num * sys.sc_driver_num] == _NEED_NOT_CHECK) {
			continue;
		}

		if (_node_in_key_chn(i, module_cfg, sys.key_offest) < 0) {
			continue;
		}

		if (current_data_temp[i] < min_key_limit_value[j++]) {
			channel_status[i] |= _KEY_BEYOND_MIN_LIMIT;
			test_error_code |= _KEY_BEYOND_MIN_LIMIT;
			beyond_min_limit_num[i]++;
			test_result = 0;
		}
	}

	return test_result;
}

static unsigned char _check_key_max_value(void)
{
	int i, j;
	unsigned char test_result = 1;
	if (sys.key_number == 0) {
		return test_result;
	}

	for (i = sys.sc_sensor_num * sys.sc_driver_num, j = 0; i < sys.sc_sensor_num * sys.sc_driver_num + sys.key_number; i++) {
		if (channel_key_need_check[i - sys.sc_sensor_num * sys.sc_driver_num] == _NEED_NOT_CHECK) {
			continue;
		}

		if (_node_in_key_chn(i, module_cfg, sys.key_offest) < 0) {
			continue;
		}

		if (current_data_temp[i] > max_key_limit_value[j++]) {
			channel_status[i] |= _KEY_BEYOND_MAX_LIMIT;
			test_error_code |= _KEY_BEYOND_MAX_LIMIT;
			beyond_max_limit_num[i]++;
			test_result = 0;
		}
	}

	return test_result;
}

static unsigned char _check_area_accord(void)
{
	int i, j, index;
	long temp;
	long accord_temp;
	unsigned char test_result = 1;
	for (i = 0; i < sys.sc_sensor_num; i++) {
		for (j = 0; j < sys.sc_driver_num; j++) {
			index = i + j * sys.sc_sensor_num;

			accord_temp = 0;
			temp = 0;
			if (need_check[index] == _NEED_NOT_CHECK) {
				continue;
			}

			if (current_data_temp[index] == 0) {
				current_data_temp[index] = 1;
				continue;
			}

			if (j == 0) {
				if (need_check[i + (j + 1) * sys.sc_sensor_num] != _NEED_NOT_CHECK) {
					accord_temp =
					    (FLOAT_AMPLIFIER * abs((s16) (current_data_temp[i + (j + 1) * sys.sc_sensor_num] - current_data_temp[index]))) /
					    current_data_temp[index];
				}
			} else if (j == sys.sc_driver_num - 1) {
				if (need_check[i + (j - 1) * sys.sc_sensor_num] != _NEED_NOT_CHECK) {
					accord_temp =
					    (FLOAT_AMPLIFIER * abs((s16) (current_data_temp[i + (j - 1) * sys.sc_sensor_num] - current_data_temp[index]))) /
					    current_data_temp[index];
				}
			} else {
				if (need_check[i + (j + 1) * sys.sc_sensor_num] != _NEED_NOT_CHECK) {
					accord_temp =
					    (FLOAT_AMPLIFIER * abs((s16) (current_data_temp[i + (j + 1) * sys.sc_sensor_num] - current_data_temp[index]))) /
					    current_data_temp[index];
				}
				if (need_check[i + (j - 1) * sys.sc_sensor_num] != _NEED_NOT_CHECK) {
					temp =
					    (FLOAT_AMPLIFIER * abs((s16) (current_data_temp[i + (j - 1) * sys.sc_sensor_num] - current_data_temp[index]))) /
					    current_data_temp[index];
				}
				if (temp > accord_temp) {
					accord_temp = temp;
				}
			}

			if (i == 0) {
				if (need_check[i + 1 + j * sys.sc_sensor_num] != _NEED_NOT_CHECK) {
					temp =
					    (FLOAT_AMPLIFIER * abs((s16) (current_data_temp[i + 1 + j * sys.sc_sensor_num] - current_data_temp[index]))) / current_data_temp[index];
				}
				if (temp > accord_temp) {
					accord_temp = temp;
				}
			} else if (i == sys.sc_sensor_num - 1) {
				if (need_check[i - 1 + j * sys.sc_sensor_num] != _NEED_NOT_CHECK) {
					temp =
					    (FLOAT_AMPLIFIER * abs((s16) (current_data_temp[i - 1 + j * sys.sc_sensor_num] - current_data_temp[index]))) / current_data_temp[index];
				}
				if (temp > accord_temp) {
					accord_temp = temp;
				}
			} else {
				if (need_check[i + 1 + j * sys.sc_sensor_num] != _NEED_NOT_CHECK) {
					temp =
					    (FLOAT_AMPLIFIER * abs((s16) (current_data_temp[i + 1 + j * sys.sc_sensor_num] - current_data_temp[index]))) / current_data_temp[index];
				}
				if (temp > accord_temp) {
					accord_temp = temp;
				}
				if (need_check[i - 1 + j * sys.sc_sensor_num] != _NEED_NOT_CHECK) {
					temp =
					    (FLOAT_AMPLIFIER * abs((s16) (current_data_temp[i - 1 + j * sys.sc_sensor_num] - current_data_temp[index]))) / current_data_temp[index];
				}
				if (temp > accord_temp) {
					accord_temp = temp;
				}
			}

			channel_max_accord[index] = accord_temp;

			if (accord_temp > accord_limit[index]) {
				channel_status[index] |= _BEYOND_ACCORD_LIMIT;
				test_error_code |= _BEYOND_ACCORD_LIMIT;
				test_result = 0;
				beyond_accord_limit_num[index]++;

			GTP_ERROR("current[%d]%ld,accord_limit[%d]%ld", index, accord_temp, index, accord_limit[index]);
			}
		}
	}

	return test_result;
}

static unsigned char _check_area_accord_1143(void)
{
	int i, j, index, sign = 1;
	long accord_temp;
	unsigned char test_result = 1;
	GTP_ERROR("%s", __func__);
	for (i = 0; i < sys.sc_sensor_num; i++) {
		for (j = 0; j < sys.sc_driver_num; j++) {
			index = i + j * sys.sc_sensor_num;

			accord_temp = 0;
//                      GTP_ERROR("need_check[%d]%d.",index,need_check[index]);
			if (need_check[index] == _NEED_NOT_CHECK) {
				continue;
			}

			if (current_data_temp[index] == 0) {
//                              current_data_temp[index] = 1;
				continue;
			}

			if (j % 2 == 0) {
				sign = 1;	//right
			} else {
				sign = -1;	//left
			}

			if (i == 0) {
				if ((need_check[i + 1 + j * sys.sc_sensor_num] != _NEED_NOT_CHECK) && (need_check[i + (j + sign) * sys.sc_sensor_num] != _NEED_NOT_CHECK)
				    && (need_check[i + 1 + (j + sign) * sys.sc_sensor_num] != _NEED_NOT_CHECK)) {
					accord_temp =
					    (FLOAT_AMPLIFIER * abs((s16) (current_data_temp[i + 1 + j * sys.sc_sensor_num] - current_data_temp[index])) +
					     FLOAT_AMPLIFIER *
					     abs((s16) (current_data_temp[i + 1 + (j + sign) * sys.sc_sensor_num] - current_data_temp[i + (j + sign) * sys.sc_sensor_num]))) /
					    current_data_temp[index];

//                                      GTP_ERROR("buf[%d]=%d,buf[%d]=%d,buf[%d]=%d,buf[%d]=%d",i + 1 + j * sys.sc_sensor_num,current_data_temp[i + 1 + j * sys.sc_sensor_num],index,current_data_temp[index],
//                                                      i + 1 + (j + sign) * sys.sc_sensor_num,current_data_temp[i + 1 + (j + sign) * sys.sc_sensor_num],i + (j + sign) * sys.sc_sensor_num,current_data_temp[i + (j + sign) * sys.sc_sensor_num]);
				}
			} else if (i == sys.sc_sensor_num - 1) {
				if ((need_check[i - 1 + j * sys.sc_sensor_num] != _NEED_NOT_CHECK) && (need_check[i + (j + sign) * sys.sc_sensor_num] != _NEED_NOT_CHECK)
				    && (need_check[i - 1 + (j + sign) * sys.sc_sensor_num] != _NEED_NOT_CHECK)) {
					accord_temp =
					    (FLOAT_AMPLIFIER * abs((s16) (current_data_temp[i - 1 + j * sys.sc_sensor_num] - current_data_temp[index])) +
					     FLOAT_AMPLIFIER *
					     abs((s16) (current_data_temp[i - 1 + (j + sign) * sys.sc_sensor_num] - current_data_temp[i + (j + sign) * sys.sc_sensor_num]))) /
					    current_data_temp[index];
				}
			} else {
				if ((need_check[i + 1 + j * sys.sc_sensor_num] != _NEED_NOT_CHECK) && (need_check[i - 1 + j * sys.sc_sensor_num] != _NEED_NOT_CHECK)) {
					accord_temp =
					    (FLOAT_AMPLIFIER * abs((s16) (current_data_temp[i + 1 + j * sys.sc_sensor_num] - current_data_temp[index])) +
					     FLOAT_AMPLIFIER * abs((s16) (current_data_temp[i - 1 + j * sys.sc_sensor_num] - current_data_temp[index]))) / current_data_temp[index];
				}
			}

			channel_max_line_accord[index] += accord_temp;

			channel_max_accord[index] = accord_temp;

		}
	}

	return test_result;
}

static unsigned char _check_full_screen_offest(unsigned char special_check)
{
	int average_temp = 0;
	int i, j;
	long offset_temp;
	int special_num = 0;
	int special_channel[_SPECIAL_LIMIT_CHANNEL_NUM];
	unsigned char test_result = 1;
	/* calculate the average value of total screen */
	average_temp = _get_average_value(current_data_temp);
	GTP_ERROR("average:%d\n", average_temp);

	/* caculate the offset between the channel value and the average value */
	for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
		if (need_check[i] == _NEED_NOT_CHECK) {
			continue;
		}
		/* get the max ratio of the total screen,(current_chn_value - screen_average_value)/screen_average_value */
		offset_temp = abs((current_data_temp[i] - average_temp) * FLOAT_AMPLIFIER) / average_temp;

//              /* if area accord test is pass,and then do not do the screen accord test */
//              if ((channel_status[i] & _BEYOND_ACCORD_LIMIT) != _BEYOND_ACCORD_LIMIT) {
//                      continue;
//              }
		/* current channel accord validity detection */
		if (offset_temp > offset_limit) {
			if ((special_check == _SPECIAL_CHECK) && (special_num < _SPECIAL_LIMIT_CHANNEL_NUM) && (offset_temp <= special_limit)) {
				if ((current_data_index == 0 || special_num == 0)) {
					special_channel[special_num] = i;
					special_num++;
				} else {
					for (j = 0; j < special_num; j++) {
						if (special_channel[j] == i) {
							break;
						}
					}
					if (j == special_num) {
						special_channel[special_num] = i;
						special_num++;
					}
				}
			} else {
				channel_status[i] |= _BEYOND_OFFSET_LIMIT;
				test_error_code |= _BEYOND_OFFSET_LIMIT;
				beyond_offset_limit_num[i]++;
				test_result = 0;
			}
		}		/* end of if (offset_temp > offset_limit) */
	}			/* end of for (i = 0; i < sys.sensor_num*sys.driver_num; i++) */
	if (special_check && test_result == 1) {
		for (i = special_num - 1; i > 0; i--) {
			for (j = i - 1; j >= 0; j--) {
				if ((special_channel[i] - special_channel[j] == 1)
				    || (special_channel[i] - special_channel[j] == sys.sc_driver_num)) {
					channel_status[special_channel[j]] |= _BEYOND_OFFSET_LIMIT;
					test_error_code |= _BEYOND_OFFSET_LIMIT;
					beyond_offset_limit_num[special_channel[j]]++;
					test_result = 0;
				}
			}
		}
	}			/* end of if (special_check && test_result == TRUE) */
	return test_result;
}

static unsigned char _check_full_screen_jitter(void)
{
	int j;
	unsigned short max_jitter = 0;
	unsigned char test_result = 1;
	unsigned short *shake_value;
#ifdef SIX_SIGMA_JITTER
	double six_sigma = 0;
#endif
	shake_value = (u16 *) (&global_large_buf[0]);

	for (j = 0; j < sys.sc_sensor_num * sys.sc_driver_num; j++) {
		if (need_check[j] == _NEED_NOT_CHECK) {
			continue;
		}
		shake_value[j] = channel_max_value[j] - channel_min_value[j];

		if (shake_value[j] > max_jitter) {
			max_jitter = shake_value[j];
		}
	}

#ifdef SIX_SIGMA_JITTER
	six_sigma = 6 * _get_six_sigma_value();
	/* if 6sigama>jitter_limit or max_jitter>jitter_limit+10, jitter is not legal */
	if ((six_sigma > permit_jitter_limit) || (max_jitter > permit_jitter_limit + 10))
#endif
	{
		for (j = 0; j < sys.sc_sensor_num * sys.sc_driver_num; j++) {
			if (shake_value[j] >= permit_jitter_limit) {
				channel_status[j] |= _BEYOND_JITTER_LIMIT;
				test_error_code |= _BEYOND_JITTER_LIMIT;
				test_result = 0;
				GTP_ERROR("point %d beyond jitter limit", j);
			}
		}
	}
	return test_result;
}

static unsigned char _check_uniformity(void)
{
	u16 i = 0;
	u16 min_val = 0, max_val = 0;
	long uniformity = 0;
	unsigned char test_result = 1;
	min_val = current_data_temp[0];
	max_val = current_data_temp[0];
	for (i = 1; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
		if (need_check[i] == _NEED_NOT_CHECK) {
			continue;
		}
		if (current_data_temp[i] > max_val) {
			max_val = current_data_temp[i];
		}
		if (current_data_temp[i] < min_val) {
			min_val = current_data_temp[i];
		}
	}

	if (0 == max_val) {
		uniformity = 0;
	} else {
		uniformity = (min_val * FLOAT_AMPLIFIER) / max_val;
	}
	GTP_ERROR("min_val: %d, max_val: %d, tp uniformity(x1000): %lx", min_val, max_val, uniformity);
	if (uniformity < uniformity_limit) {
		beyond_uniformity_limit_num++;
		//channel_status[i] |= _BEYOND_UNIFORMITY_LIMIT;
		test_error_code |= _BEYOND_UNIFORMITY_LIMIT;
		test_result = 0;
	}
	return test_result;
}

static s32 _check_modele_type(int type)
{
	int ic_id = read_sensorid();
	if (ic_id != read_sensorid()) {
		ic_id = read_sensorid();
		if (ic_id != read_sensorid()) {
			WARNING("Read many ID inconsistent");
			return INVALID;
		}
	}

	if ((ic_id | ini_module_type) < 0) {
		return ic_id < ini_module_type ? ic_id : ini_module_type;
	}

	if (ic_id != ini_module_type) {
		test_error_code |= _MODULE_TYPE_ERR;
	}

	return 0;
}

static s32 _check_device_version(int type)
{
	s32 ret = 0;
	if (type & _VER_EQU_CHECK) {
		GTP_ERROR("ini version:%s\n", ini_version1);
		ret = check_version(ini_version1);
		if (ret != 0) {
			test_error_code |= _VERSION_ERR;
		}
	} else if (type & _VER_GREATER_CHECK) {
		GTP_ERROR("ini version:%s\n", ini_version1);
		ret = check_version(ini_version1);
		if (ret == 1 || ret < 0) {
			test_error_code |= _VERSION_ERR;
		}
	} else if (type & _VER_BETWEEN_CHECK) {
		signed int ret1 = 0;
		signed int ret2 = 0;

		GTP_ERROR("ini version1:%s\n", ini_version1);
		GTP_ERROR("ini version2:%s\n", ini_version2);
		ret1 = check_version(ini_version1);
		ret2 = check_version(ini_version2);
		if (ret1 == 1 || ret2 == 2 || ret1 < 0 || ret2 < 0) {
			test_error_code |= _VERSION_ERR;
		}
	}

	return 0;
}

static unsigned char _rawdata_test_result_analysis(int check_types)
{
	int i;
	int accord_temp = 0;
	int temp;
	int error_code_temp = test_error_code;
	int err = 0;
	int test_end = 0;
	// screen max value check
	GTP_ERROR("[%s] enter,,check_types=[0x%x]\n", __func__, check_types);
	error_code_temp &= ~_BEYOND_MAX_LIMIT;
	if (((check_types & _MAX_CHECK) != 0) && ((test_error_code & _BEYOND_MAX_LIMIT) != 0)) {
		for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
			if ((channel_status[i] & _BEYOND_MAX_LIMIT) == _BEYOND_MAX_LIMIT) {
				if (beyond_max_limit_num[i] >= (samping_set_number * 9 / 10)) {
					//GTP_ERROR("beyond max limit.9/10...[%d]",i);
					error_code_temp |= _BEYOND_MAX_LIMIT;
					test_end |= _BEYOND_MAX_LIMIT;
				} else if (beyond_max_limit_num[i] > samping_set_number / 10) {
					//GTP_ERROR("beyond max limit.1/10...[%d]",i);
					error_code_temp |= _BEYOND_MAX_LIMIT;
					err |= _BEYOND_MAX_LIMIT;
				} else {
					channel_status[i] &= ~_BEYOND_MAX_LIMIT;
				}
			}
		}
	}
	/* touch key max value check */
	error_code_temp &= ~_KEY_BEYOND_MAX_LIMIT;
	if (((check_types & _KEY_MAX_CHECK) != 0) && ((test_error_code & _KEY_BEYOND_MAX_LIMIT) != 0)) {
		for (i = sys.sc_sensor_num * sys.sc_driver_num; i < sys.sc_sensor_num * sys.sc_driver_num + sys.key_number; i++) {
			if ((channel_status[i] & _KEY_BEYOND_MAX_LIMIT) == _KEY_BEYOND_MAX_LIMIT) {
				if (beyond_max_limit_num[i] >= (samping_set_number * 9 / 10)) {
					//GTP_ERROR("key beyond max limit.9/10...[%d]",i);
					error_code_temp |= _KEY_BEYOND_MAX_LIMIT;
					test_end |= _KEY_BEYOND_MAX_LIMIT;
				} else if (beyond_max_limit_num[i] > samping_set_number / 10) {
					//GTP_ERROR("key beyond max limit.1/10...[%d]",i);
					err |= _KEY_BEYOND_MAX_LIMIT;
				} else {
					channel_status[i] &= ~_KEY_BEYOND_MAX_LIMIT;
				}
			}
		}
	}
	/* screen min value check */
	error_code_temp &= ~_BEYOND_MIN_LIMIT;
	if (((check_types & _MIN_CHECK) != 0) && ((test_error_code & _BEYOND_MIN_LIMIT) != 0)) {
		for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
			if ((channel_status[i] & _BEYOND_MIN_LIMIT) == _BEYOND_MIN_LIMIT) {
				if (beyond_min_limit_num[i] >= (samping_set_number * 9 / 10)) {
					//GTP_ERROR("beyond min limit.9/10...[%d]",i);
					error_code_temp |= _BEYOND_MIN_LIMIT;
					test_end |= _BEYOND_MIN_LIMIT;
				} else if (beyond_min_limit_num[i] > samping_set_number / 10) {
					//GTP_ERROR("beyond min limit.1/10...[%d]",i);
					error_code_temp |= _BEYOND_MIN_LIMIT;
					err |= _BEYOND_MIN_LIMIT;
				} else {
					channel_status[i] &= ~_BEYOND_MIN_LIMIT;
				}
			}
		}
	}
	/* touch key min value check */
	error_code_temp &= ~_KEY_BEYOND_MIN_LIMIT;
	if (((check_types & _KEY_MIN_CHECK) != 0) && ((test_error_code & _KEY_BEYOND_MIN_LIMIT) != 0)) {
		for (i = sys.sc_sensor_num * sys.sc_driver_num; i < sys.sc_sensor_num * sys.sc_driver_num + sys.key_number; i++) {
			if ((channel_status[i] & _KEY_BEYOND_MIN_LIMIT) == _KEY_BEYOND_MIN_LIMIT) {
				if (beyond_min_limit_num[i] >= (samping_set_number * 9 / 10)) {
					error_code_temp |= _KEY_BEYOND_MIN_LIMIT;
					test_end |= _KEY_BEYOND_MIN_LIMIT;
				} else if (beyond_min_limit_num[i] > samping_set_number / 10) {
					error_code_temp |= _KEY_BEYOND_MIN_LIMIT;
					err |= _KEY_BEYOND_MIN_LIMIT;
				} else {
					channel_status[i] &= ~_KEY_BEYOND_MIN_LIMIT;
				}
			}
		}
	}
	/* screen uniformity check */
	error_code_temp &= ~_BEYOND_UNIFORMITY_LIMIT;
	if (((check_types & _UNIFORMITY_CHECK) != 0) && ((test_error_code & _BEYOND_UNIFORMITY_LIMIT) != 0)) {
		GTP_ERROR("beyond_uniformity_limit_num:%d", beyond_uniformity_limit_num);
		if (beyond_uniformity_limit_num >= (samping_set_number * 9 / 10)) {
			error_code_temp |= _BEYOND_UNIFORMITY_LIMIT;
			test_end |= _BEYOND_UNIFORMITY_LIMIT;
		} else if (beyond_uniformity_limit_num > samping_set_number / 10) {
			error_code_temp |= _BEYOND_UNIFORMITY_LIMIT;
			err |= _BEYOND_UNIFORMITY_LIMIT;
			GTP_ERROR("beyond_uniformity_limit_num:%d", beyond_uniformity_limit_num);
		}
	}
	/* adjacent data accord check */
	error_code_temp &= ~_BEYOND_ACCORD_LIMIT;
	if (((check_types & _ACCORD_CHECK) != 0) && ((test_error_code & _BEYOND_ACCORD_LIMIT) != 0)) {
		//GTP_ERROR("analysis accord ");
		for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
			if ((channel_status[i] & _BEYOND_ACCORD_LIMIT) == _BEYOND_ACCORD_LIMIT) {
				if (beyond_accord_limit_num[i] >= (samping_set_number * 9 / 10)) {
					//GTP_ERROR("beyond accord limit.9/10...[%d]",i);
					error_code_temp |= _BEYOND_ACCORD_LIMIT;
					test_end |= _BEYOND_ACCORD_LIMIT;
					accord_temp++;
				} else if (beyond_accord_limit_num[i] > samping_set_number / 10) {
					//GTP_ERROR("beyond accord limit.1/10...[%d]",i);
					error_code_temp |= _BEYOND_ACCORD_LIMIT;
					err |= _BEYOND_ACCORD_LIMIT;
					accord_temp++;
				} else {
					channel_status[i] &= ~_BEYOND_ACCORD_LIMIT;
				}
			}
		}
	}
	/* screen max accord check */
	error_code_temp &= ~_BEYOND_OFFSET_LIMIT;
	if (((check_types & _OFFSET_CHECK) != 0) && ((test_error_code & _BEYOND_OFFSET_LIMIT) != 0)) {
		for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
			if ((channel_status[i] & _BEYOND_OFFSET_LIMIT) == _BEYOND_OFFSET_LIMIT) {
				if (beyond_offset_limit_num[i] >= (samping_set_number * 9 / 10)) {
					error_code_temp |= _BEYOND_OFFSET_LIMIT;
					test_end |= _BEYOND_OFFSET_LIMIT;
				} else if (beyond_offset_limit_num[i] > samping_set_number / 10) {
					error_code_temp |= _BEYOND_OFFSET_LIMIT;
					err |= _BEYOND_OFFSET_LIMIT;
				} else {
					channel_status[i] &= ~_BEYOND_OFFSET_LIMIT;
				}
			}
		}
	}

	if (1) {		/* (sys.AccordOrOffsetNG == FALSE) */
		if (((check_types & _ACCORD_CHECK) != 0) && ((check_types & _OFFSET_CHECK) != 0)) {
			for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
				if (((channel_status[i] & _BEYOND_OFFSET_LIMIT) != _BEYOND_OFFSET_LIMIT)
				    && ((channel_status[i] & _BEYOND_ACCORD_LIMIT) == _BEYOND_ACCORD_LIMIT)) {
					channel_status[i] &= ~_BEYOND_ACCORD_LIMIT;
					accord_temp--;
				}
			}
			if (accord_temp == 0) {
				error_code_temp &= ~_BEYOND_ACCORD_LIMIT;
				test_end &= ~_BEYOND_ACCORD_LIMIT;
				err &= ~_BEYOND_ACCORD_LIMIT;
			}
		}
	}

	error_code_temp |= (test_error_code & _BEYOND_JITTER_LIMIT);

	test_error_code = error_code_temp;
	GTP_ERROR("test_end:0x%0x err:0x%0x,test_error_code:%x.\n", test_end, err, test_error_code);
	if (test_end != _CHANNEL_PASS) {
		return 1;
	}

	if (err != _CHANNEL_PASS) {
		if ((check_types & _FAST_TEST_MODE) != 0) {
			if (samping_set_number < samping_num) {
				temp = samping_set_number;
				samping_set_number += (samping_num / 4);
				for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
					channel_average[i] = channel_average[i] * temp / samping_set_number;
					channel_square_average[i] += channel_square_average[i] * temp / samping_set_number;
				}
				return 0;
			}
		}
	}

	return 1;
}

static unsigned char _accord_test_result_analysis(int check_types)
{
	int i, error, error_tmp;
	long accord_tmp;
	if (sys.chip_type != _GT1143) {
		return 1;
	}
	//1143
	test_error_code &= ~_BEYOND_ACCORD_LIMIT;
	error = error_tmp = _CHANNEL_PASS;
	if ((check_types & _ACCORD_CHECK) != 0) {
		for (i = 0; i < sys.sc_sensor_num * sys.sc_driver_num; i++) {
			accord_tmp = channel_max_line_accord[i] / current_data_index;
//                      GTP_ERROR("accord_line_limit[%d]=%ld",i,accord_line_limit[i]);
			if (accord_tmp > accord_line_limit[i]) {
				error |= _BEYOND_ACCORD_LIMIT;
			} else if (accord_tmp > accord_limit[i] && accord_tmp <= accord_line_limit[i]) {
				error_tmp |= _BETWEEN_ACCORD_AND_LINE;
			}
		}
	}

	if (error) {
		test_error_code |= error;
	} else {
		test_error_code |= error_tmp;
	}
	GTP_ERROR("1143 accord test_error_code 0x%x", test_error_code);
	return 1;
}

#if GTP_SAVE_TEST_DATA
static s32 _save_testing_data(char *save_test_data_dir, int test_types)
{
	FILE *fp = NULL;
	s32 ret;
	s32 tmp = 0;
	u8 *data = NULL;
	s32 i = 0, j;
	s32 bytes = 0;
	int max, min;
	int average;
	GTP_ERROR("[%s] enter.\n", __func__);
	data = (u8 *) malloc(MAX_BUFFER_SIZE);

	if (NULL == data) {
		WARNING("memory error!");
		return MEMORY_ERR;
	}
	fp = fopen(save_test_data_dir, "a+");
	if (NULL == fp) {
		WARNING("open %s failed!", save_test_data_dir);
		free(data);
		return FILE_OPEN_CREATE_ERR;
	}

	if (current_data_index == 0) {
		bytes = (s32) snprintf((char *)data, 20, "Device Type:%s\n", sys.chip_name);
		bytes += (s32) snprintf((char *)&data[bytes], 10, "Config:\n");
		for (i = 0; i < sys.config_length; i++) {
			bytes += (s32) snprintf((char *)&data[bytes], 10, "0x%02X,", module_cfg[i]);
		}
		bytes += (s32) snprintf((char *)&data[bytes], 5, "\n\n");
		ret = fwrite(data, bytes, 1, fp);
		bytes = 0;
		if (ret < 0) {
			WARNING("write to file fail.");
			goto exit_save_testing_data;
		}

		if ((test_types & _MAX_CHECK) != 0) {
			bytes = (s32) snprintf((char *)data, 20, "Channel maximum:\n");
			for (i = 0; i < sys.sc_sensor_num; i++) {
				for (j = 0; j < sys.sc_driver_num; j++) {
					bytes += (s32) snprintf((char *)&data[bytes], 10, "%d,", max_limit_value[i + j * sys.sc_sensor_num]);
				}
				bytes += (s32) snprintf((char *)&data[bytes], 5, "\n");
				ret = fwrite(data, bytes, 1, fp);
				bytes = 0;
				if (ret < 0) {
					WARNING("write to file fail.");
					goto exit_save_testing_data;
				}
			}

			j = 0;
			bytes = 0;
			for (i = 0; i < sys.key_number; i++) {
				if (_node_in_key_chn(i + sys.sc_sensor_num * sys.sc_driver_num, module_cfg, sys.key_offest) < 0) {
					continue;
				}
				bytes += (s32) snprintf((char *)&data[bytes], 10, "%d,", max_key_limit_value[j++]);
				// GTP_ERROR("max[%d]%d",i,max_key_limit_value[i]); */
			}
			bytes += (s32) snprintf((char *)&data[bytes], 5, "\n");
			ret = fwrite(data, bytes, 1, fp);
			bytes = 0;
			if (ret < 0) {
				WARNING("write to file fail.");
				goto exit_save_testing_data;
			}
		}

		if ((test_types & _MIN_CHECK) != 0) {
			bytes = (s32) snprintf((char *)data, 20, "Channel minimum:\n");
			for (i = 0; i < sys.sc_sensor_num; i++) {
				for (j = 0; j < sys.sc_driver_num; j++) {
					bytes += (s32) snprintf((char *)&data[bytes], 10, "%d,", min_limit_value[i + j * sys.sc_sensor_num]);
				}
				bytes += (s32) snprintf((char *)&data[bytes], 5, "\n");
				ret = fwrite(data, bytes, 1, fp);
				bytes = 0;
				if (ret < 0) {
					WARNING("write to file fail.");
					goto exit_save_testing_data;
				}
			}

			j = 0;
			bytes = 0;
			for (i = 0; i < sys.key_number; i++) {
				if (_node_in_key_chn(i + sys.sc_sensor_num * sys.sc_driver_num, module_cfg, sys.key_offest) < 0) {
					continue;
				}
				bytes += (s32) snprintf((char *)&data[bytes], 10, "%d,", min_key_limit_value[j++]);
			}
			bytes += (s32) snprintf((char *)&data[bytes], 5, "\n");
			ret = fwrite(data, bytes, 1, fp);
			if (ret < 0) {
				WARNING("write to file fail.");
				goto exit_save_testing_data;
			}
		}

		if ((test_types & _ACCORD_CHECK) != 0) {
			bytes = (s32) snprintf((char *)data, 25, "Channel average:(%d)\n", FLOAT_AMPLIFIER);
			for (i = 0; i < sys.sc_sensor_num; i++) {
				for (j = 0; j < sys.sc_driver_num; j++) {
					bytes += (s32) snprintf((char *)&data[bytes], 10, "%ld,", accord_limit[i + j * sys.sc_sensor_num]);
				}
				bytes += (s32) snprintf((char *)&data[bytes], 5, "\n");
				ret = fwrite(data, bytes, 1, fp);
				bytes = 0;
				if (ret < 0) {
					WARNING("write to file fail.");
					goto exit_save_testing_data;
				}
			}

			bytes = (s32) snprintf((char *)data, 5, "\n");
			ret = fwrite(data, bytes, 1, fp);
			if (ret < 0) {
				WARNING("write to file fail.");
				goto exit_save_testing_data;
			}

			if (sys.chip_type == _GT1143) {
				bytes = (s32) snprintf((char *)data, 25, "Channel line accord:(%d)\n", FLOAT_AMPLIFIER);
				for (i = 0; i < sys.sc_sensor_num; i++) {
					for (j = 0; j < sys.sc_driver_num; j++) {
						bytes += (s32) snprintf((char *)&data[bytes], 10, "%ld,", accord_line_limit[i + j * sys.sc_sensor_num]);
					}
					bytes += (s32) snprintf((char *)&data[bytes], 5, "\n");
					ret = fwrite(data, bytes, 1, fp);
					bytes = 0;
					if (ret < 0) {
						WARNING("write to file fail.");
						goto exit_save_testing_data;
					}
				}

				bytes = (s32) snprintf((char *)data, 5, "\n");
				ret = fwrite(data, bytes, 1, fp);
				if (ret < 0) {
					WARNING("write to file fail.");
					goto exit_save_testing_data;
				}
			}
		}

		bytes = (s32) snprintf((char *)data, 15, " Rawdata\n");
		ret = fwrite(data, bytes, 1, fp);
		if (ret < 0) {
			WARNING("write to file fail.");
			goto exit_save_testing_data;
		}
	}

	bytes = (s32) snprintf((char *)data, 8, "No.%d\n", current_data_index);
	ret = fwrite(data, bytes, 1, fp);
	if (ret < 0) {
		WARNING("write to file fail.");
		goto exit_save_testing_data;
	}

	max = min = current_data_temp[0];
	average = 0;
	for (i = 0; i < sys.sc_sensor_num; i++) {
		bytes = 0;
		for (j = 0; j < sys.sc_driver_num; j++) {
			tmp = current_data_temp[i + j * sys.sc_sensor_num];
			bytes += (s32) snprintf((char *)&data[bytes], 10, "%d,", tmp);
			if (tmp > max) {
				max = tmp;
			}
			if (tmp < min) {
				min = tmp;
			}
			average += tmp;
		}
		bytes += (s32) snprintf((char *)&data[bytes], 5, "\n");
		ret = fwrite(data, bytes, 1, fp);
		if (ret < 0) {
			WARNING("write to file fail.");
			goto exit_save_testing_data;
		}
	}
	average = average / (sys.sc_sensor_num * sys.sc_driver_num);

	if (sys.key_number > 0) {
		bytes = (s32) snprintf((char *)data, 15, "Key Rawdata:\n");
		for (i = 0; i < sys.key_number; i++) {
			if (_node_in_key_chn(i + sys.sc_sensor_num * sys.sc_driver_num, module_cfg, sys.key_offest) < 0) {
				continue;
			}
			bytes += (s32) snprintf((char *)&data[bytes], 10, "%d,", current_data_temp[i + sys.sc_sensor_num * sys.sc_driver_num]);
		}
		bytes += (s32) snprintf((char *)&data[bytes], 5, "\n");
		ret = fwrite(data, bytes, 1, fp);
		if (ret < 0) {
			WARNING("write to file fail.");
			goto exit_save_testing_data;
		}
	}


	bytes = (s32) snprintf((char *)data, 100, "  Maximum:%d  Minimum:%d  Average:%d\n\n", max, min, average);
	ret = fwrite(data, bytes, 1, fp);
	if (ret < 0) {
		WARNING("write to file fail.");
		goto exit_save_testing_data;
	}

	if ((test_types & _ACCORD_CHECK) != 0) {
		bytes = (s32) snprintf((char *)data, 100, "Channel_Accord :(%d)\n", FLOAT_AMPLIFIER);
		ret = fwrite(data, bytes, 1, fp);
		if (ret < 0) {
			WARNING("write to file fail.");
			goto exit_save_testing_data;
		}
		for (i = 0; i < sys.sc_sensor_num; i++) {
			bytes = 0;
			for (j = 0; j < sys.sc_driver_num; j++) {
				//bytes += (s32) sprintf((char *)&data[bytes], "%ld,", channel_max_accord[i + j * sys.sc_sensor_num]);
				//for save float accord
				if ((channel_max_accord[i + j * sys.sc_sensor_num]/1000 > 0)
					&& (channel_max_accord[i + j * sys.sc_sensor_num]/10000 == 0)) {
					bytes += (s32) snprintf((char *)&data[bytes], 10, "%ld,",
								channel_max_accord[i + j * sys.sc_sensor_num]);
				} else if ((channel_max_accord[i + j * sys.sc_sensor_num]/100 > 0)
					&& (channel_max_accord[i + j * sys.sc_sensor_num]/1000 == 0)) {
					bytes += (s32) snprintf((char *)&data[bytes], 10, "0.%ld,",
								channel_max_accord[i + j * sys.sc_sensor_num]);
				} else if ((channel_max_accord[i + j * sys.sc_sensor_num]/10 > 0)
					&& (channel_max_accord[i + j * sys.sc_sensor_num]/100 == 0)) {
					bytes += (s32) snprintf((char *)&data[bytes], 10, "0.0%ld,",
								channel_max_accord[i + j * sys.sc_sensor_num]);
				} else if ((channel_max_accord[i + j * sys.sc_sensor_num] > 0)
					&& (channel_max_accord[i + j * sys.sc_sensor_num] < 10)) {
					bytes += (s32) snprintf((char *)&data[bytes], 10, "0.00%ld,",
								channel_max_accord[i + j * sys.sc_sensor_num]);
				}
				//end for save float accord
			}
			bytes += (s32) snprintf((char *)&data[bytes], 5, "\n");
			ret = fwrite(data, bytes, 1, fp);
			if (ret < 0) {
				WARNING("write to file fail.");
				goto exit_save_testing_data;
			}
		}
		bytes = (s32) snprintf((char *)data, 5, "\n");
		ret = fwrite(data, bytes, 1, fp);
		if (ret < 0) {
			WARNING("write to file fail.");
			goto exit_save_testing_data;
		}
	}
exit_save_testing_data:
	/* GTP_ERROR("step4"); */
	free(data);
	/* GTP_ERROR("step3"); */
	fclose(fp);
	/* GTP_ERROR("step2"); */
	return ret;
}

static s32 _save_test_result_data(char *save_test_data_dir, int test_types, u8 *shortresult)
{
	FILE *fp = NULL;
	s32 ret, index;
	u8 *data = NULL;
	s32 bytes = 0;
	data = (u8 *) malloc(MAX_BUFFER_SIZE);
	if (NULL == data) {
		WARNING("memory error!");
		return MEMORY_ERR;
	}
	GTP_ERROR("before fopen patch = %s\n", save_test_data_dir);
	fp = fopen(save_test_data_dir, "a+");
	if (NULL == fp) {
		WARNING("open %s failed!", save_test_data_dir);
		free(data);
		return FILE_OPEN_CREATE_ERR;
	}

	bytes = (s32) snprintf((char *)data, 15, "Test Result:");
	if (test_error_code == _CHANNEL_PASS) {
		bytes += (s32) snprintf((char *)&data[bytes], 6, "Pass\n\n");
	} else {
		bytes += (s32) snprintf((char *)&data[bytes], 6, "Fail\n\n");
	}
	bytes += (s32) snprintf((char *)&data[bytes], 15, "Test items:\n");
	if ((test_types & _MAX_CHECK) != 0) {
		bytes += (s32) snprintf((char *)&data[bytes], 16, "Max Rawdata:  ");
		if (test_error_code & _BEYOND_MAX_LIMIT) {
			bytes += (s32) snprintf((char *)&data[bytes], 6, "NG !\n");
		} else {
			bytes += (s32) snprintf((char *)&data[bytes], 6, "pass\n");
		}
	}

	if ((test_types & _MIN_CHECK) != 0) {
		bytes += (s32) snprintf((char *)&data[bytes], 16, "Min Rawdata:  ");
		if (test_error_code & _BEYOND_MIN_LIMIT) {
			bytes += (s32) snprintf((char *)&data[bytes], 6, "NG !\n");
		} else {
			bytes += (s32) snprintf((char *)&data[bytes], 6, "pass\n");
		}
	}

	if ((test_types & _ACCORD_CHECK) != 0) {
		bytes += (s32) snprintf((char *)&data[bytes], 16, "Area Accord:  ");

		if (test_error_code & _BETWEEN_ACCORD_AND_LINE) {
			bytes += (s32) snprintf((char *)&data[bytes], 10, "Fuzzy !\n");
		} else {
			if (test_error_code & _BEYOND_ACCORD_LIMIT) {

				bytes += (s32) snprintf((char *)&data[bytes], 6, "NG !\n");

			} else {

				bytes += (s32) snprintf((char *)&data[bytes], 6, "pass\n");

			}
		}
	}

	if ((test_types & _OFFSET_CHECK) != 0) {

		bytes += (s32) snprintf((char *)&data[bytes], 15, "Max Offest:  ");

		if (test_error_code & _BEYOND_OFFSET_LIMIT) {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "NG !\n");

		} else {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "pass\n");

		}
	}

	if ((test_types & _JITTER_CHECK) != 0) {

		bytes += (s32) snprintf((char *)&data[bytes], 15, "Max Jitier:  ");

		if (test_error_code & _BEYOND_JITTER_LIMIT) {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "NG !\n");

		} else {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "pass\n");

		}
	}

	if (test_types & _UNIFORMITY_CHECK) {

		bytes += (s32) snprintf((char *)&data[bytes], 15, "Uniformity:  ");

		if (test_error_code & _BEYOND_UNIFORMITY_LIMIT) {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "NG !\n");

		} else {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "pass\n");

		}
	}

	if ((test_types & _KEY_MAX_CHECK) != 0) {

		bytes += (s32) snprintf((char *)&data[bytes], 20, "Key Max Rawdata:  ");

		if (test_error_code & _KEY_BEYOND_MAX_LIMIT) {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "NG !\n");

		} else {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "pass\n");

		}
	}

	if ((test_types & _KEY_MIN_CHECK) != 0) {

		bytes += (s32) snprintf((char *)&data[bytes], 20, "Key Min Rawdata:  ");

		if (test_error_code & _KEY_BEYOND_MIN_LIMIT) {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "NG !\n");

		} else {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "pass\n");

		}
	}

	if (test_types & (_VER_EQU_CHECK | _VER_GREATER_CHECK | _VER_BETWEEN_CHECK)) {

		bytes += (s32) snprintf((char *)&data[bytes], 20, "Device Version:  ");

		if (test_error_code & _VERSION_ERR) {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "NG !\n");

		} else {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "pass\n");

		}
	}

	if (test_types & _MODULE_TYPE_CHECK) {

		bytes += (s32) snprintf((char *)&data[bytes], 16, "Module Type:  ");

		if (test_error_code & _MODULE_TYPE_ERR) {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "NG !\n");

		} else {

			bytes += (s32) snprintf((char *)&data[bytes], 6, "pass\n");

		}
	}


	ret = fwrite(data, bytes, 1, fp);

	if (ret < 0) {

		WARNING("write to file fail.");

		free(data);

		fclose(fp);

		return ret;

	}

	if ((test_types & _MODULE_SHORT_CHECK) != 0) {

		bytes = (s32) snprintf((char *)data, 50, "Module short test:  ");

		if (test_error_code & _GT_SHORT) {

			bytes += (s32) snprintf((char *)&data[bytes], 35, "NG !\n\n\nError items:\nShort:\n");

			if (shortresult[0] > _GT9_UPLOAD_SHORT_TOTAL) {

				WARNING("short total over limit, data error!");

				shortresult[0] = 0;

			}

			for (index = 0; index < shortresult[0]; index++) {

				/* GTP_ERROR("bytes=%d shortresult[0]=%d",bytes,shortresult[0]); */
				if (shortresult[1 + index * 4] & 0x80) {

						bytes += (s32) snprintf((char *)&data[bytes], 10, "Drv%d - ", shortresult[1 + index * 4] & 0x7F);

					} else {

						if (shortresult[1 + index * 4] == (sys.max_driver_num + 1)) {

							bytes += (s32) snprintf((char *)&data[bytes], 10, "GND\\VDD%d - ", shortresult[1 + index * 4] & 0x7F);

						} else {

							bytes += (s32) snprintf((char *)&data[bytes], 10, "Sen%d - ", shortresult[1 + index * 4] & 0x7F);

						}
					}
					if (shortresult[2 + index * 4] & 0x80) {

						bytes += (s32) snprintf((char *)&data[bytes], 10, "Drv%d is short", shortresult[2 + index * 4] & 0x7F);

					} else {

						if (shortresult[2 + index * 4] == (sys.max_driver_num + 1)) {

							bytes += (s32) snprintf((char *)&data[bytes], 20, "GND\\VDD is short");


					}

					else {

						bytes += (s32) snprintf((char *)&data[bytes], 15, "Sen%d is short", shortresult[2 + index * 4] & 0x7F);

					}
				}
				bytes += (s32) snprintf((char *)&data[bytes], 15, "(R=%d Kohm)\n", (((shortresult[3 + index * 4] << 8) + shortresult[4 + index * 4])
					& 0xffff) / 10);

				GTP_ERROR("%d&%d:", shortresult[1 + index * 4], shortresult[2 + index * 4]);

				GTP_ERROR("%dK", (((shortresult[3 + index * 4] << 8) + shortresult[4 + index * 4]) & 0xffff) / 10);

			}
		}

		else {

			bytes += (s32) snprintf((char *)&data[bytes], 5, "\n\n");

		}
		ret = fwrite(data, bytes, 1, fp);

		if (ret < 0) {

			WARNING("write to file fail.");

			free(data);

			fclose(fp);

			return ret;

		}

	}

	free(data);

	fclose(fp);

	return 1;

}

#endif
#if 0
static void _unzip_nc(unsigned char *s_buf, unsigned char *key_nc_buf, unsigned short s_length, unsigned short key_length)
{

	unsigned short i, point;

	unsigned char m, data;

	int b_size = sys.max_driver_num * sys.max_sensor_num;

	u8 *tmp;

	tmp = (u8 *) (&global_large_buf[s_length + key_length]);

	memset(need_check, 0, b_size);

	memset(tmp, 0, b_size);

	point = 0;

	for (i = 0; i < s_length; i++) {

		data = *s_buf;

		for (m = 0; m < 8; m++) {

			if (point >= b_size) {

				goto KEY_NC_UNZIP;

			}

			tmp[point] &= 0xfe;

			if ((data & 0x80) == 0x80) {

				tmp[point] |= 0x01;

			}

			data <<= 1;

			point++;
		}

		s_buf++;

	}

	/* memcpy(need_check,tmp,sys.sensor_num*sys.sc_driver_num); */
KEY_NC_UNZIP:

	for (i = 0, point = 0; i < sys.sc_driver_num; i++) {

		for (m = 0; m < sys.sc_sensor_num; m++) {

			need_check[point++] = tmp[i + m * sys.sc_driver_num];
//                      GTP_ERROR("need_check[%d]%d",point-1,need_check[point-1]);
		}

	}

	GTP_ERROR("Load key nc\n");

	memset(channel_key_need_check, 0, MAX_KEY_RAWDATA);

	point = 0;

	for (i = 0; i < key_length; i++) {

		data = *key_nc_buf;

		for (m = 0; m < 8; m++) {

			if (point >= MAX_KEY_RAWDATA) {

				return;

			}

			channel_key_need_check[point] &= 0xfe;

			if ((data & 0x80) == 0x80) {

				channel_key_need_check[point] |= 0x01;

			}

			data <<= 1;

			point++;

		}

		key_nc_buf++;

	}

}
#endif
#if GTP_TEST_PARAMS_FROM_INI
static s32 _init_special_node(char *inipath, const char *key, int *max_limit, int *min_limit, long *accord_limit)
{

	FILE *fp = NULL;

	char *buf = NULL;

	char *tmp = NULL;

	size_t bytes = 0;

	s32 i = 0, space_count = 0;

	u16 tmpNode = 0, max, min;

	long accord;

	int b_size = sys.max_sensor_num * sys.max_driver_num;

	if (NULL == inipath) {
		return PARAMETERS_ILLEGL;
	}

	buf = (char *)malloc(b_size * 4 * 6);

	if (NULL == buf) {

		return MEMORY_ERR;

	}
	GTP_ERROR("before fopen patch = %s\n", inipath);
	fp = fopen((const char *)inipath, "r");

	if (fp == NULL) {

		free(buf);

		buf = NULL;

		GTP_ERROR("[%s]open %s fail!", __func__, inipath);

		return INI_FILE_OPEN_ERR;

	}

	/* while(!feof(fp)) */
	while (1) {

		i = 0;

		space_count = 0;

		do {

			bytes = vfs_read(fp, &buf[i], 1, &fp->f_pos);
			if (i >= b_size * 4 * 6 || bytes < 0) {

				fclose(fp);

				free(buf);

				return INI_FILE_READ_ERR;

			}
			/* GTP_ERROR("%c", buf[i]); */

			if (buf[i] == ' ') {

				continue;

			}

		} while (buf[i] != '\r' && buf[i++] != '\n');

		buf[i] = '\0';
		//GTP_ERROR("special node [%s].\n",buf);
		getrid_space(buf, i);

		strtok((char *)buf, "=");

		if (0 == strcmp((const char *)buf, key)) {

			i = 0;

			//GTP_ERROR("Begin get node data.");

			do {

				tmp = (char *)strtok((char *)NULL, ",");

				if (tmp == NULL) {

					break;

				}

				tmpNode = atoi((char const *)tmp);

				 //GTP_ERROR("tmpNode:%d", tmpNode);
				tmp = (char *)strtok((char *)NULL, ",");

				if (tmp == NULL) {

					fclose(fp);

					free(buf);

					return INI_FILE_ILLEGAL;

				}

				max = atoi((char const *)tmp);

				//GTP_ERROR("max:%d", max);

				tmp = (char *)strtok((char *)NULL, ",");

				if (tmp == NULL) {

					fclose(fp);

					free(buf);

					return INI_FILE_ILLEGAL;

				}

				min = atoi((char const *)tmp);

				//GTP_ERROR("min:%d", min);

				tmp = (char *)strtok((char *)NULL, ",");

				if (tmp == NULL) {

					fclose(fp);

					free(buf);

					return INI_FILE_ILLEGAL;

				}

				accord = atof((char const *)tmp);// * FLOAT_AMPLIFIER;

				//GTP_ERROR("accord:%ld", accord);

				if (tmpNode < sys.sc_driver_num * sys.sc_sensor_num) {

					tmpNode = tmpNode / sys.sc_driver_num + (tmpNode % sys.sc_driver_num) * sys.sc_sensor_num;

					if (max_limit != NULL) {
						max_limit[tmpNode] = max;
					}

					if (min_limit != NULL) {
						min_limit[tmpNode] = min;
					}

					if (accord_limit != NULL) {
						accord_limit[tmpNode] = accord;
					}
//                                      max_limit_value[tmpNode] = max;
//
//                                      min_limit_value[tmpNode] = min;
//
//                                      accord_limit[tmpNode] = accord;

				}

				else {

					tmpNode -= sys.sc_driver_num * sys.sc_sensor_num;

					max_key_limit_value[tmpNode] = max;

					min_key_limit_value[tmpNode] = min;
				}

			} while (++i < b_size);

			//GTP_ERROR("get node data end.");
			fclose(fp);

			free(buf);

			return 1;

		}

	}

	fclose(fp);

	free(buf);

	return INI_FILE_ILLEGAL;

}
#else
static s32 _init_special_node_array(void)
{
	s32 i = 0;

	u16 tmpNode = 0;
	const u16 special_node_tmp[] = SEPCIALTESTNODE;
	const u16 special_line_node_tmp[] = SEPCIALLINECHKNODE;
	for (i = 0; i < sizeof(special_node_tmp); i += 4) {
		tmpNode = special_node_tmp[i];
		if (tmpNode < sys.sc_driver_num * sys.sc_sensor_num) {
			tmpNode = tmpNode / sys.sc_driver_num + (tmpNode % sys.sc_driver_num) * sys.sc_sensor_num;

			max_limit_value[tmpNode] = special_node_tmp[i + 1];

			min_limit_value[tmpNode] = special_node_tmp[i + 2];

			accord_limit[tmpNode] = special_node_tmp[i + 3];

		} else {
			tmpNode -= sys.sc_driver_num * sys.sc_sensor_num;

			max_key_limit_value[tmpNode] = special_node_tmp[i + 1];

			min_key_limit_value[tmpNode] = special_node_tmp[i + 2];
		}
	}

	for (i = 0; i < sizeof(special_line_node_tmp); i += 4) {
		tmpNode = special_line_node_tmp[i];
		if (tmpNode < sys.sc_driver_num * sys.sc_sensor_num) {
			tmpNode = tmpNode / sys.sc_driver_num + (tmpNode % sys.sc_driver_num) * sys.sc_sensor_num;

			accord_line_limit[tmpNode] = special_line_node_tmp[i + 3];

		}
	}

	return i / 4;
}
#endif
static s32 _check_rawdata_proc(int check_types, u16 *data, int len, char *save_path)
{
	GTP_ERROR("[%s] enter.\n", __func__);
	if (data == NULL || len < sys.driver_num * sys.sensor_num) {

		return PARAMETERS_ILLEGL;

	}

	memcpy(current_data_temp, data, sys.driver_num * sys.sensor_num * 2);

	_get_channel_max_value();

	_get_channel_min_value();

	_get_channel_average_value();

	if ((check_types & _MAX_CHECK) != 0) {

		_check_channel_max_value();

		DEBUG_DATA(channel_status, sys.driver_num * sys.sensor_num);

	}

	if ((check_types & _MIN_CHECK) != 0) {

		_check_channel_min_value();

		DEBUG_DATA(channel_status, sys.driver_num * sys.sensor_num);

	}

	if ((check_types & _KEY_MAX_CHECK) != 0) {

		_check_key_max_value();

		GTP_ERROR("After key max check\n");

		DEBUG_DATA(channel_status, sys.driver_num * sys.sensor_num);

	}

	if ((check_types & _KEY_MIN_CHECK) != 0) {

		_check_key_min_value();

		GTP_ERROR("After key min check\n");

		DEBUG_DATA(channel_status, sys.driver_num * sys.sensor_num);

	}

	if ((check_types & _ACCORD_CHECK) != 0) {

		if (sys.chip_type == _GT1143) {
			_check_area_accord_1143();
		} else {
			_check_area_accord();
		}

		DEBUG_DATA(channel_status, sys.driver_num * sys.sensor_num);

	}

	if ((check_types & _OFFSET_CHECK) != 0) {

		_check_full_screen_offest(check_types & _SPECIAL_CHECK);

		GTP_ERROR("After offset check:%ld", offset_limit);

		DEBUG_DATA(channel_status, sys.driver_num * sys.sensor_num);

	}

	if ((check_types & _UNIFORMITY_CHECK) != 0) {

		_check_uniformity();

		GTP_ERROR("After uniformity check:%ld\n", uniformity_limit);

	}
#if GTP_SAVE_TEST_DATA
//  if ((check_types & _TEST_RESULT_SAVE) != 0)
	{
		_save_testing_data(save_path, check_types);
	}

#endif

	GTP_ERROR("The %d group rawdata test end!\n", current_data_index);

	current_data_index++;

	if (current_data_index < samping_set_number) {

		return 0;

	}

	if ((check_types & _JITTER_CHECK) != 0) {

		_check_full_screen_jitter();

		GTP_ERROR("After FullScreenJitterCheck\n");

	}

	if (_rawdata_test_result_analysis(check_types) == 0) {

		GTP_ERROR("After TestResultAnalyse\n");

		return 0;

	}

	_accord_test_result_analysis(check_types);

	GTP_ERROR("rawdata test end!\n");

	return 1;
}

static s32 _check_other_options(int type)
{

	int ret = 0;
	if (type & (_VER_EQU_CHECK | _VER_GREATER_CHECK | _VER_BETWEEN_CHECK)) {

		ret = _check_device_version(type);

		if (ret < 0) {

			return ret;

		}

	}

	if (type & _MODULE_TYPE_CHECK) {

		ret = _check_modele_type(type);

		if (ret < 0) {

			return ret;

		}

		GTP_ERROR("module test");

	}

	return ret;

}

static int _hold_ss51_dsp(void)
{

	int ret = -1;

	int retry = 0;

	unsigned char rd_buf[3];
	/* reset cpu */
	reset_guitar();
	msleep(100);
	enter_update_mode();

	while (retry++ < 200) {

		/* Hold ss51 & dsp */
		rd_buf[0] = 0x0C;

		ret = i2c_write_data(_rRW_MISCTL__SWRST_B0_, rd_buf, 1);

		if (ret <= 0) {

			GTP_ERROR("Hold ss51 & dsp I2C error,retry:%d", retry);

			continue;

		}

		usleep(2 * 1000);

		if (retry < 100) {
			continue;
		}
		/* Confirm hold */
		ret = i2c_read_data(_rRW_MISCTL__SWRST_B0_, rd_buf, 1);

		if (ret <= 0) {

			GTP_ERROR("Hold ss51 & dsp I2C error,retry:%d", retry);

			continue;

		}

		if (0x0C == rd_buf[0]) {

			GTP_ERROR("Hold ss51 & dsp confirm SUCCESS");

			break;

		}

		GTP_ERROR("Hold ss51 & dsp confirm 0x4180 failed,value:%d", rd_buf[0]);

	}

	if (retry >= 200) {

		WARNING("Enter update Hold ss51 failed.");

		return INVALID;

	}

	return 1;

}

/**
 * fun: take several tests of average value
 */
static int _ave_resist_analysis(u8 *s, u8 *d)
{
	u32 g_size, offest, ave;
	u8 i, j, k, cnt, s_chn1, s_chn2, upload_cnt, index;
	u16 *tmp;
	if (s == NULL || d == NULL) {
		return PARAMETERS_ILLEGL;
	}

	g_size = _GT9_UPLOAD_SHORT_TOTAL * 4 + 1;
	tmp = (u16 *) malloc(gt900_short_test_times * g_size * sizeof(u16));
	if (tmp == NULL) {
		return MEMORY_ERR;
	}
//      GTP_ERROR("%s",__func__);
	DEBUG_ARRAY(s, gt900_short_test_times * g_size);
	cnt = s[0];
	for (i = 0; i < gt900_short_test_times; i++) {
		if (s[i * g_size] > cnt) {
			cnt = s[i * g_size];
		}
	}

	for (i = 0, upload_cnt = 0; i < cnt; i++) {
		s_chn1 = s[i * 4 + 1];
		s_chn2 = s[i * 4 + 2];
		memset(tmp, 0, gt900_short_test_times * g_size * 2);

		for (j = 0, index = 1; j < gt900_short_test_times; j++) {
			offest = j * g_size;
			for (k = 0; k < s[offest]; k++) {
				if ((s_chn1 == s[offest + k * 4 + 1] && s_chn2 == s[offest + k * 4 + 2]) || (s_chn2 == s[offest + k * 4 + 1] && s_chn1 == s[offest + k * 4 + 2])) {
					tmp[0]++;
					tmp[index++] = (s[offest + k * 4 + 3] << 8) + s[offest + k * 4 + 4];
//                                      GTP_ERROR("tmp[%d]%d,offest %d",index -1,tmp[index -1],offest);
					break;
				}
			}
		}

		/*frequency */
		GTP_ERROR("tmp[0]%d", tmp[0]);
		if (tmp[0] * FLOAT_AMPLIFIER >= gt900_short_test_times * FLOAT_AMPLIFIER * 8 / 10) {
			for (k = 0, ave = 0; k < tmp[0]; k++) {
				ave += tmp[k + 1];
//                              GTP_ERROR("tmp[%d]%d",k+1,tmp[k+1]);
			}

			ave = ave / tmp[0];
//                      GTP_ERROR("ave%d,cnt%d",ave,tmp[0]);
			d[upload_cnt * 4 + 1] = s[i * 4 + 1];
			d[upload_cnt * 4 + 2] = s[i * 4 + 2];
			d[upload_cnt * 4 + 3] = (u8) (ave >> 8);
			d[upload_cnt * 4 + 4] = (u8) (ave);
			upload_cnt++;
			test_error_code |= _GT_SHORT;
		}
	}

	d[0] = upload_cnt;

	free(tmp);
	return upload_cnt;
}

static s32 _check_short_circuit(int test_types, u8 *short_result)
{

	u8 *data, *upload_data_ave, *tmp, short_test_cnt = 0;

	s32 i, ret, g_size;
	GTP_ERROR("short test start.");
	g_size = (_GT9_UPLOAD_SHORT_TOTAL * 4 + 1);
	data = (u8 *) malloc((gt900_short_test_times + 1) * g_size + 10);
	if (data == NULL) {
		return MEMORY_ERR;
	}

	memset(data, 0, (gt900_short_test_times + 1) * g_size + 10);
	tmp = (u8 *) &data[5];
	upload_data_ave = (u8 *) &data[5 + g_size];
gt900_short_again:
	/* select addr & hold ss51_dsp */
	ret = _hold_ss51_dsp();

	if (ret <= 0) {

		GTP_ERROR("hold ss51 & dsp failed.");

		ret = ENTER_UPDATE_MODE_ERR;

		goto gt900_test_exit;

	}

	/******preparation of downloading the DSP code**********/
	enter_update_mode_noreset();

	GTP_ERROR("Loading..\n");

	if (load_dsp_code_check() < 0) {

		ret = SHORT_TEST_ERROR;

		goto gt900_test_exit;

	}

	dsp_fw_startup(short_test_cnt);

	usleep(30 * 1000);

	for (i = 0; i < 100; i++) {

		i2c_read_data(_rRW_MISCTL__SHORT_BOOT_FLAG, data, 1);

		if (data[0] == 0xaa) {

			break;

		}

		GTP_ERROR("buf[0]:0x%x", data[0]);

		usleep(10 * 1000);

	}

	if (i >= 20) {

		WARNING("Didn't get 0xaa at 0x%X\n", _rRW_MISCTL__SHORT_BOOT_FLAG);

		ret = SHORT_TEST_ERROR;

		goto gt900_test_exit;

	}

	data[0] = 0x00;

	i2c_write_data(_bRW_MISCTL__TMR0_EN, data, 1);

	write_test_params(module_cfg);

	/* clr 5095, runing dsp */
	data[0] = 0x00;

	i2c_write_data(_rRW_MISCTL__SHORT_BOOT_FLAG, data, 1);

	/* check whether the test is completed */
	i = 0;

	while (1) {

		i2c_read_data(0x8800, data, 1);

		if (data[0] == 0x88) {

			break;

		}

		usleep(50 * 1000);

		i++;

		if (i > 150) {

			WARNING("Didn't get 0x88 at 0x8800\n");

			ret = SHORT_TEST_ERROR;

			goto gt900_test_exit;

		}

	}

	i = ((sys.max_driver_num + sys.max_sensor_num) * 2 + 20) / 4;
	i += (sys.max_driver_num + sys.max_sensor_num) * 2 / 4;

	i += (_GT9_UPLOAD_SHORT_TOTAL + 1) * 4;

	if (i > 1343) {

		ret = -1;
		goto gt900_test_exit;

	}

	/*
	 * GTP_ERROR("AVDD:%d",sys_avdd);
	 GTP_ERROR("gt900_short_threshold:%d",gt900_short_threshold);
	 GTP_ERROR("gt900_resistor_warn_threshold:%d",gt900_resistor_warn_threshold);
	 GTP_ERROR("gt900_drv_drv_resistor_threshold:%d",gt900_drv_drv_resistor_threshold);
	 GTP_ERROR("gt900_drv_sen_resistor_threshold:%d",gt900_drv_sen_resistor_threshold);
	 GTP_ERROR("gt900_sen_sen_resistor_threshold:%d",gt900_sen_sen_resistor_threshold);
	 GTP_ERROR("gt900_drv_gnd_resistor_threshold:%d",gt900_drv_gnd_resistor_threshold);
	 GTP_ERROR("gt900_sen_gnd_resistor_threshold:%d",gt900_sen_gnd_resistor_threshold);
	 */
	memset(tmp, 0, g_size);
	ret = short_test_analysis(tmp, _GT9_UPLOAD_SHORT_TOTAL);
	if (ret < 0) {
		goto gt900_test_exit;
	}

	memcpy(&upload_data_ave[short_test_cnt * g_size], tmp, tmp[0] * 4 + 1);
	short_test_cnt++;
	if (short_test_cnt < gt900_short_test_times) {
		GTP_ERROR("short test again %d", short_test_cnt);
		goto gt900_short_again;
	} else {
		ret = _ave_resist_analysis(upload_data_ave, short_result);
		if (ret > 0) {

			test_to_show(&short_result[1], short_result[0]);

			ret = short_result[0];
		}
	}

gt900_test_exit:

	short_test_end();
	free(data);
	GTP_ERROR("short upload_cnt:%d", short_result[0]);

	GTP_ERROR("short test end,ret 0x%x", ret);

	return ret;
}

static s32 _alloc_test_memory(int b_size)
{

	int offest = 0;
	if (global_large_buf != NULL) {
		return 1;
	}

	original_cfg = (u8 *) malloc(sizeof(u8) * 400);

	if (original_cfg == NULL) {

		return MEMORY_ERR;

	}

	module_cfg = (u8 *) malloc(sizeof(u8) * 400);

	if (module_cfg == NULL) {

		free(original_cfg);

		return MEMORY_ERR;

	}

	current_data_temp = (u16 *) malloc(b_size * sizeof(short));

	if (current_data_temp == NULL) {

		free(original_cfg);

		free(module_cfg);

		return MEMORY_ERR;

	}

	global_large_buf = (u8 *) malloc(sizeof(u8) * 64 * b_size);

	if (global_large_buf == NULL) {

		free(original_cfg);

		free(module_cfg);

		free(current_data_temp);

		return MEMORY_ERR;

	}

	memset(global_large_buf, 0, 64 * b_size);
	memset(original_cfg, 0, 400);
	memset(module_cfg, 0, 400);
	memset(current_data_temp, 0, b_size * sizeof(short));

	offest = 2 * 1024;

	need_check = &global_large_buf[offest];

	offest += b_size * sizeof(char);

	channel_key_need_check = &global_large_buf[offest];

	offest += b_size * sizeof(char);

	channel_status = (u16 *) (&global_large_buf[offest]);

	offest += b_size * sizeof(short);

	channel_max_value = (u16 *) (&global_large_buf[offest]);

	offest += b_size * sizeof(short);

	channel_min_value = (u16 *) (&global_large_buf[offest]);

	offest += b_size * sizeof(short);

	channel_max_accord = (long *)(&global_large_buf[offest]);

	offest += b_size * sizeof(long);

	channel_max_line_accord = (long *)(&global_large_buf[offest]);

	offest += b_size * sizeof(long);

	channel_average = (int *)(&global_large_buf[offest]);

	offest += b_size * sizeof(int);

	channel_square_average = (int *)(&global_large_buf[offest]);

	offest += b_size * sizeof(int);

	driver_status = &global_large_buf[offest];

	offest += b_size * sizeof(char);

	beyond_max_limit_num = &global_large_buf[offest];

	offest += b_size * sizeof(char);

	beyond_min_limit_num = &global_large_buf[offest];

	offest += b_size * sizeof(char);

	beyond_accord_limit_num = &global_large_buf[offest];

	offest += b_size * sizeof(char);

	beyond_offset_limit_num = &global_large_buf[offest];

	offest += b_size * sizeof(char);

	beyond_jitter_limit_num = &global_large_buf[offest];

	offest += b_size * sizeof(char);

	max_limit_value = (s32 *) (&global_large_buf[offest]);

	offest += b_size * sizeof(int);

	min_limit_value = (s32 *) (&global_large_buf[offest]);

	offest += b_size * sizeof(int);

	accord_limit = (long *)(&global_large_buf[offest]);

	offest += b_size * sizeof(long);

	accord_line_limit = (long *)(&global_large_buf[offest]);

	offest += b_size * sizeof(long);

	ini_version1 = &global_large_buf[offest];

	offest += 50;

	ini_version2 = &global_large_buf[offest];

	offest += 50;

/*	original_cfg = &global_large_buf[offest];

	offest += 350;

	module_cfg = &global_large_buf[offest];

	offest += 350;
*/
	max_key_limit_value = (s32 *) (&global_large_buf[offest]);

	offest += MAX_KEY_RAWDATA * sizeof(int);

	min_key_limit_value = (s32 *) (&global_large_buf[offest]);

	offest += MAX_KEY_RAWDATA * sizeof(int);
/*
	current_data_temp = (u16 *) (&global_large_buf[offest]);

	offest += b_size * sizeof(short);
*/
	return 1;

}

static void _exit_test(void)
{
	int ret = 0;

	if (global_large_buf != NULL) {

		free(global_large_buf);

		global_large_buf = NULL;

	}
	if (original_cfg != NULL) {

		free(original_cfg);

		original_cfg = NULL;

	}
	if (module_cfg != NULL) {

		free(module_cfg);

		module_cfg = NULL;

	}
	if (current_data_temp != NULL) {

		free(current_data_temp);

		current_data_temp = NULL;

	}

	ret = gt1x_send_cfg(gt1x_config, gt1x_cfg_length);
	if (ret == 0) {
		GTP_INFO("%s:send cfg success", __func__);
	} else{
		GTP_ERROR("%s:send cfg fail", __func__);
	}

}

#if GTP_TEST_PARAMS_FROM_INI
static s32 _get_test_parameters(char *inipath)
{

	int test_types;
	if (inipath == NULL) {

		WARNING("ini file path is null.");

		return PARAMETERS_ILLEGL;

	}

	test_types = ini_read_hex(inipath, (const char *)"test_types");

	if (test_types < 0) {

		WARNING("get the test types 0x%x is error.", test_types);

		return test_types;

	}

	//GTP_ERROR("test type:%x\n", test_types);

	if (test_types & _MAX_CHECK) {

		max_limit_value_tmp = ini_read_int(inipath, (const char *)"max_limit_value");

		//GTP_ERROR("max_limit_value:%d\n", max_limit_value_tmp);

	}

	if (test_types & _MIN_CHECK) {

		min_limit_value_tmp = ini_read_int(inipath, (const char *)"min_limit_value");

		//GTP_ERROR("min_limit_value:%d\n", min_limit_value_tmp);

	}

	if (test_types & _ACCORD_CHECK) {

		accord_limit_tmp = ini_read_float(inipath, (const char *)"accord_limit");

		//GTP_ERROR("accord_limit:%ld", accord_limit_tmp);

	}

	if (test_types & _OFFSET_CHECK) {

		offset_limit = ini_read_float(inipath, (const char *)"offset_limit");

		//GTP_ERROR("offset_limit:%ld", offset_limit);

	}

	if (test_types & _JITTER_CHECK) {

		permit_jitter_limit = ini_read_int(inipath, (const char *)"permit_jitter_limit");

		//GTP_ERROR("permit_jitter_limit:%d\n", permit_jitter_limit);

	}

	if (test_types & _SPECIAL_CHECK) {

		special_limit = ini_read_float(inipath, (const char *)"special_limit");

		//GTP_ERROR("special_limit:%ld", special_limit);

	}

	if (test_types & _KEY_MAX_CHECK) {

		max_key_limit_value_tmp = ini_read_int(inipath, (const char *)"max_key_limit_value");

		//GTP_ERROR("max_key_limit_value:%d\n", max_key_limit_value_tmp);

	}

	if (test_types & _KEY_MIN_CHECK) {

		min_key_limit_value_tmp = ini_read_int(inipath, (const char *)"min_key_limit_value");

		//GTP_ERROR("min_key_limit_value:%d\n", min_key_limit_value_tmp);

	}

	if (test_types & _MODULE_TYPE_CHECK) {

		ini_module_type = ini_read_int(inipath, (const char *)"module_type");

		//GTP_ERROR("Sensor ID:%d\n", ini_module_type);

	}

	if (test_types & _VER_EQU_CHECK) {

		ini_read(inipath, (const char *)"version_equ", (char *)ini_version1);

		//GTP_ERROR("version_equ:%s", ini_version1);

	}

	else if (test_types & _VER_GREATER_CHECK) {

		ini_read(inipath, (const char *)"version_greater", (char *)ini_version1);

		//GTP_ERROR("version_greater:%s", ini_version1);

	}

	else if (test_types & _VER_BETWEEN_CHECK) {

		ini_read(inipath, (const char *)"version_between1", (char *)ini_version1);

		//GTP_ERROR("version_between1:%s", ini_version1);

		ini_read(inipath, (const char *)"version_between2", (char *)ini_version2);

		//GTP_ERROR("version_between2:%s", ini_version2);

	}

	if (test_types & _MODULE_SHORT_CHECK) {

		long lret;

		int ret = ini_read_int(inipath, (const char *)"gt900_short_threshold");

		if (ret > 0) {

			gt900_short_threshold = ret;

			GTP_ERROR("gt900_short_threshold:%d", gt900_short_threshold);

		}

		ret = ini_read_int(inipath, (const char *)"gt900_drv_drv_resistor_threshold");

		if (ret > 0) {

			gt900_drv_drv_resistor_threshold = ret;

			GTP_ERROR("gt900_drv_drv_resistor_threshold:%d", gt900_drv_drv_resistor_threshold);

		}

		ret = ini_read_int(inipath, (const char *)"gt900_drv_sen_resistor_threshold");

		if (ret > 0) {

			gt900_drv_sen_resistor_threshold = ret;

			GTP_ERROR("gt900_drv_sen_resistor_threshold:%d", gt900_drv_sen_resistor_threshold);

		}

		ret = ini_read_int(inipath, (const char *)"gt900_sen_sen_resistor_threshold");

		if (ret > 0) {

			gt900_sen_sen_resistor_threshold = ret;

			GTP_ERROR("gt900_sen_sen_resistor_threshold:%d", gt900_sen_sen_resistor_threshold);

		}

		ret = ini_read_int(inipath, (const char *)"gt900_resistor_warn_threshold");

		if (ret > 0) {

			gt900_resistor_warn_threshold = ret;

			GTP_ERROR("gt900_resistor_warn_threshold:%d", gt900_resistor_warn_threshold);

		}

		ret = ini_read_int(inipath, (const char *)"gt900_drv_gnd_resistor_threshold");

		if (ret > 0) {

			gt900_drv_gnd_resistor_threshold = ret;

			GTP_ERROR("gt900_drv_gnd_resistor_threshold:%d", gt900_drv_gnd_resistor_threshold);

		}

		ret = ini_read_int(inipath, (const char *)"gt900_sen_gnd_resistor_threshold");

		if (ret > 0) {

			gt900_sen_gnd_resistor_threshold = ret;

			GTP_ERROR("gt900_sen_gnd_resistor_threshold:%d", gt900_sen_gnd_resistor_threshold);

		}

		ret = ini_read_int(inipath, (const char *)"gt900_short_test_times");

		if (ret > 0) {

			gt900_short_test_times = ret;

			GTP_ERROR("gt900_short_test_times:%d", gt900_short_test_times);

		}

		lret = ini_read_float(inipath, (const char *)"tri_pattern_r_ratio");

		if (ret > 0) {

			tri_pattern_r_ratio = lret;

			GTP_ERROR("tri_pattern_r_ratio:%ld", tri_pattern_r_ratio);

		}

		ret = ini_read_float(inipath, (const char *)"AVDD");

		if (ret > 0) {

			sys_avdd = ret * 10 / FLOAT_AMPLIFIER;

			GTP_ERROR("sys_avdd:%d", sys_avdd);

		}

	}

	uniformity_limit = ini_read_float(inipath, (const char *)"rawdata_uniformity");

	if (uniformity_limit > 0) {

		test_types |= _UNIFORMITY_CHECK;

		GTP_ERROR("uniformity_limit:%ld", uniformity_limit);

	}

	GTP_ERROR("_get_test_parameters success!");

	return test_types;

}
#else
static s32 _get_test_parameters_array(void)
{

	int test_types;

	test_types = TEST_TYPES;
	if (test_types < 0) {

		WARNING("get the test types 0x%x is error.", test_types);

		return test_types;

	}

	//GTP_ERROR("test type:%x\n", test_types);

	if (test_types & _MAX_CHECK) {

		max_limit_value_tmp = MAX_LIMIT_VALUE;

		GTP_ERROR("max_limit_value:%d\n", max_limit_value_tmp);

	}

	if (test_types & _MIN_CHECK) {

		min_limit_value_tmp = MIN_LIMIT_VALUE;

		GTP_ERROR("min_limit_value:%d\n", min_limit_value_tmp);

	}

	if (test_types & _ACCORD_CHECK) {

		accord_limit_tmp = ACCORD_LIMIT;

		GTP_ERROR("accord_limit:%ld", accord_limit_tmp);

	}

	if (test_types & _OFFSET_CHECK) {

		offset_limit = OFFEST_LIMIT;

		GTP_ERROR("offset_limit:%ld", offset_limit);

	}

	if (test_types & _JITTER_CHECK) {

		permit_jitter_limit = PERMIT_JITTER_LIMIT;

		GTP_ERROR("permit_jitter_limit:%d\n", permit_jitter_limit);

	}

	if (test_types & _SPECIAL_CHECK) {

		special_limit = SPECIAL_LIMIT;

		GTP_ERROR("special_limit:%ld", special_limit);

	}

	if (test_types & _KEY_MAX_CHECK) {

		max_key_limit_value_tmp = MAX_KEY_LIMIT_VALUE;

		GTP_ERROR("max_key_limit_value:%d\n", max_key_limit_value_tmp);

	}

	if (test_types & _KEY_MIN_CHECK) {

		min_key_limit_value_tmp = MIN_KEY_LIMIT_VALUE;

		GTP_ERROR("min_key_limit_value:%d\n", min_key_limit_value_tmp);

	}

	if (test_types & _MODULE_TYPE_CHECK) {

		ini_module_type = MODULE_TYPE;

		GTP_ERROR("Sensor ID:%d\n", ini_module_type);

	}

	if (test_types & _VER_EQU_CHECK) {
		memcpy((char *)ini_version1, (const char *)VERSION_EQU, sizeof(VERSION_EQU));

		GTP_ERROR("version_equ:%s", ini_version1);

	}

	else if (test_types & _VER_GREATER_CHECK) {

		memcpy(ini_version1, VERSION_GREATER, sizeof(VERSION_GREATER));
		GTP_ERROR("version_greater:%s", ini_version1);

	}

	else if (test_types & _VER_BETWEEN_CHECK) {

		memcpy(ini_version1, VERSION_BETWEEN1, sizeof(VERSION_BETWEEN1));
		GTP_ERROR("version_between1:%s", ini_version1);

		memcpy(ini_version2, VERSION_BETWEEN2, sizeof(VERSION_BETWEEN2));
		GTP_ERROR("version_between2:%s", ini_version2);

	}

	if (test_types & _MODULE_SHORT_CHECK) {

		gt900_short_threshold = GT900_SHORT_THRESHOLD;

		GTP_ERROR("gt900_short_threshold:%d", gt900_short_threshold);

		gt900_drv_drv_resistor_threshold = GT900_DRV_DRV_RESISTOR_THRESHOLD;

		GTP_ERROR("gt900_drv_drv_resistor_threshold:%d", gt900_drv_drv_resistor_threshold);

		gt900_drv_sen_resistor_threshold = GT900_DRV_SEN_RESISTOR_THRESHOLD;

		GTP_ERROR("gt900_drv_sen_resistor_threshold:%d", gt900_drv_sen_resistor_threshold);

		gt900_sen_sen_resistor_threshold = GT900_SEN_SEN_RESISTOR_THRESHOLD;

		GTP_ERROR("gt900_sen_sen_resistor_threshold:%d", gt900_sen_sen_resistor_threshold);

		gt900_resistor_warn_threshold = GT900_RESISTOR_WARN_THRESHOLD;

		GTP_ERROR("gt900_resistor_warn_threshold:%d", gt900_resistor_warn_threshold);

		gt900_drv_gnd_resistor_threshold = GT900_DRV_GND_RESISTOR_THRESHOLD;

		GTP_ERROR("gt900_drv_gnd_resistor_threshold:%d", gt900_drv_gnd_resistor_threshold);

		gt900_sen_gnd_resistor_threshold = GT900_SEN_GND_RESISTOR_THRESHOLD;

		GTP_ERROR("gt900_sen_gnd_resistor_threshold:%d", gt900_sen_gnd_resistor_threshold);

		gt900_short_test_times = GT900_SHORT_TEST_TIMES;

		GTP_ERROR("gt900_short_test_times:%d", gt900_short_test_times);

		tri_pattern_r_ratio = TRI_PATTERN_R_RATIO;

		GTP_ERROR("tri_pattern_r_ratio:%ld", tri_pattern_r_ratio);

		sys_avdd = AVDD * 10 / FLOAT_AMPLIFIER;

		GTP_ERROR("sys_avdd:%d", sys_avdd);

	}

	uniformity_limit = RAWDATA_UNIFORMITY;

	if (uniformity_limit > 0) {

		test_types |= _UNIFORMITY_CHECK;

		GTP_ERROR("uniformity_limit:%ld", uniformity_limit);

	}

	GTP_ERROR("_get_test_parameters success!");

	return test_types;

}
#endif

static s32 _init_test_paramters(char *inipath)
{
	int b_size = sys.max_driver_num * sys.max_sensor_num + 10;

	int i;

	int check_types;

	int ret = -1;

	u8 *s_nc_buf;
	u8 *key_nc_buf;
#if !GTP_TEST_PARAMS_FROM_INI
	const u8 nc_tmp[] = NC;
	const u8 key_nc_tmp[] = KEY_NC;
	const u8 module_cfg_tmp[] = MODULE_CFG;
#endif

	GTP_ERROR("%s", __func__);
	if (_alloc_test_memory(b_size) < 0) {

		return MEMORY_ERR;

	}

	GTP_ERROR("begin _get_test_parameters");

#if GTP_TEST_PARAMS_FROM_INI
	check_types = _get_test_parameters(inipath);
#else
	check_types = _get_test_parameters_array();
#endif
	if (check_types < 0) {

		return check_types;

	}

	current_data_index = 0;

	test_error_code = _CHANNEL_PASS;

	if ((check_types & _FAST_TEST_MODE) != 0) {

		samping_set_number = (samping_num / 4) + (samping_num % 4);

	}

	else {

		samping_set_number = samping_num;

	}

	beyond_uniformity_limit_num = 0;

	for (i = 0; i < b_size; i++) {

		channel_status[i] = _CHANNEL_PASS;

		channel_max_value[i] = 0;

		channel_min_value[i] = 0xFFFF;

		channel_max_accord[i] = 0;

		channel_max_line_accord[i] = 0;

		channel_average[i] = 0;

		channel_square_average[i] = 0;

		beyond_max_limit_num[i] = 0;

		beyond_min_limit_num[i] = 0;

		beyond_accord_limit_num[i] = 0;

		beyond_offset_limit_num[i] = 0;

		max_limit_value[i] = max_limit_value_tmp;

		min_limit_value[i] = min_limit_value_tmp;

		accord_limit[i] = accord_limit_tmp;

	}

	/*for (i = 0; i < MAX_KEY_RAWDATA; i++) {

		max_key_limit_value[i] = max_key_limit_value_tmp;

		min_key_limit_value[i] = min_key_limit_value_tmp;

	}*/

	read_config(original_cfg, sys.config_length);

#if GTP_TEST_PARAMS_FROM_INI
	ret = ini_read_cfg(inipath, (const char *)"module_cfg", module_cfg);
#else
	memcpy(module_cfg, module_cfg_tmp, sizeof(module_cfg_tmp));
	ret = sizeof(module_cfg_tmp);
#endif

#if 0
	ret = sizeof(module_cfg_tmp);
	memcpy(module_cfg, module_cfg_tmp, ret);
#endif

	GTP_ERROR("module_cfg len %d ,config len %d", ret, sys.config_length);

	if (ret < 0 || ret != sys.config_length) {

		GTP_ERROR("switch the original cfg.");

		memcpy(module_cfg, original_cfg, sys.config_length);

		disable_hopping(module_cfg, sys.config_length, 1);

	} else {

		GTP_ERROR("switch the module cfg.");
		module_cfg[0] = original_cfg[0];
		if (disable_hopping(module_cfg, sys.config_length, 0) < 0) {
			memcpy(module_cfg, original_cfg, sys.config_length);
		}
	}
	s_nc_buf = (unsigned char *)(&global_large_buf[0]);
	key_nc_buf = (unsigned char *)(&global_large_buf[2 * (b_size / 8) + 10]);

	memset(s_nc_buf, 0, 2 * (b_size / 8) + 10);

	memset(key_nc_buf, 0, sys.max_sensor_num / 8 + 10);

	GTP_ERROR("here\n");
#if 0
#if GTP_TEST_PARAMS_FROM_INI
	ini_read_text(inipath, (const char *)"NC", s_nc_buf);
	ret = ini_read_text(inipath, (const char *)"KEY_NC", key_nc_buf);
#else
	memcpy(s_nc_buf, nc_tmp, sizeof(nc_tmp));
	memcpy(key_nc_buf, key_nc_tmp, sizeof(key_nc_tmp));
	ret = sizeof(key_nc_tmp);
#endif

	//GTP_ERROR("WITH KEY?:0x%x\n", ret);

	if (ret <= 0) {
		_unzip_nc(s_nc_buf, key_nc_buf, b_size / 8 + 8, 0);
	} else {
		_unzip_nc(s_nc_buf, key_nc_buf, b_size / 8 + 8, sys.max_sensor_num);
	}

	//GTP_ERROR("need check array:\n");

	//DEBUG_ARRAY(need_check, sys.sc_driver_num * sys.sc_sensor_num);

	//GTP_ERROR("key need check array:\n");

	/* DEBUG_ARRAY(channel_key_need_check, b_size); */

	//GTP_ERROR("key_nc_buf:\n");

	/* DEBUG_ARRAY(key_nc_buf, strlen((const char*)key_nc_buf)); */
#endif
#if GTP_TEST_PARAMS_FROM_INI
	ret = _init_special_node(inipath, "SepcialTestNode", max_limit_value, min_limit_value, accord_limit);
	if (ret < 0) {

		WARNING("set special node fail, ret 0x%x", ret);

	}

	for (i = 0; i < b_size; i++) {
		accord_line_limit[i] = accord_limit[i];
	}
#if 0
	ret = _init_special_node(inipath, "SepcialLineChkNode", NULL, NULL, accord_line_limit);
	if (ret < 0) {

		WARNING("set line special node fail, ret 0x%x", ret);

	}
#endif

#else
	_init_special_node_array();
#endif
	GTP_ERROR("[%s]exit.\n", __func__);
	return check_types;
}

static int _check_config(void)
{
	u8 *config;
	int ret = -1, i;
	if (module_cfg == NULL) {
		return INVALID;
	}

	config = (u8 *)malloc(sys.config_length);
	if (config == NULL) {
		return MEMORY_ERR;
	}

	memset(config, 0, sys.config_length);

	read_config(config, sys.config_length);
	if (memcmp (&module_cfg[1], &config[1], sys.config_length - 2) == 0) {
		ret = 1;
	} else {
		for (i = 0; i < sys.config_length; i++) {
			GTP_ERROR("module[%d]0x%02X, config[%d]0x%02X", i, module_cfg[i], i, config[i]);
		}
	}

	free(config);
	return ret;
}

s32 open_short_test(unsigned char *short_result_data)
{
	s32 ret = -1;

	int test_types;

	char *ini_path, times = 0, timeouts = 0;

	u16 *rawdata;

	u8 *short_result;

	char *save_path;

	u8 *largebuf;

	char check_cfg = 0;

	GTP_ERROR("[%s] enter\n", __func__);
	largebuf = (unsigned char *)malloc(600 + sys.sensor_num * sys.driver_num * 2);

	if (disable_irq_esd() < 0) {
		WARNING("disable irq and esd fail.");
		goto TEST_END;
	}

TEST_START:
	memset(largebuf, 0, 600 + sys.sensor_num * sys.driver_num * 2);

	ini_path = (char *)(&largebuf[0]);
	//ini_path = "/vendor/etc/test_sensor_0.ini";
	short_result = (u8 *) (&largebuf[250]);

	save_path = (char *)(&largebuf[350]);

	rawdata = (u16 *) (&largebuf[600]);

	GTP_ERROR("sen*drv:%d*%d", sys.sensor_num, sys.driver_num);
#if GTP_TEST_PARAMS_FROM_INI
	if (auto_find_ini(ini_find_dir1, ini_format, ini_path) < 0) {

		if (auto_find_ini(ini_find_dir2, ini_format, ini_path) < 0) {

			WARNING("Not find the ini file.");

			free(largebuf);

			return INI_FILE_ILLEGAL;

		}
	}

	GTP_ERROR("find ini path:%s", ini_path);
	GTP_ERROR("This params is short_result_data = %s\n", short_result_data);

#endif
	//ini_path = "/data/test_sensor_F.ini";
	test_types = _init_test_paramters(ini_path);

	GTP_ERROR("test type=0x%x\n", test_types);
	if (test_types < 0) {
		WARNING("get test params failed.");
		free(largebuf);
		return test_types;
	}

	//FORMAT_PATH(save_path, save_result_dir, "test_data");
	snprintf(save_path, 50, "%s%s.csv", save_result_dir, "test_data");
	GTP_ERROR("save path is %s", save_path);

	memset(save_data_path, 0, sizeof(save_data_path));
	memcpy(save_data_path, save_path, strlen(save_path));

	memset(short_result, 0, 100);

	if (test_types & _MODULE_SHORT_CHECK || (sys.chip_type == _GT1143 && test_types & _ACCORD_CHECK)) {


		usleep(20 * 1000);

		if (sys.chip_type == _GT1143) {
			gt900_drv_gnd_resistor_threshold = gt900_sen_gnd_resistor_threshold;
			gt900_drv_drv_resistor_threshold = gt900_sen_sen_resistor_threshold;
			gt900_drv_sen_resistor_threshold = gt900_sen_sen_resistor_threshold;

		}

		ret = _check_short_circuit(test_types, short_result);
		if (sys.chip_type != _GT1143) {
			reset_guitar();

			usleep(2 * 1000);

			disable_hopping(module_cfg, sys.config_length, 0);

		}

		if (ret < 0) {

			WARNING("Short Test Fail.");

			goto TEST_END;

		}

		GTP_ERROR("cnt %d", short_result[0]);

		if (short_result_data != NULL) {

			memcpy(short_result_data, short_result, short_result[0] * 4 + 1);
		}
		if (sys.chip_type == _GT1143 && test_error_code & _GT_SHORT) {
			goto TEST_COMPLETE;
		}
	}

	ret = _check_other_options(test_types);

	if (ret < 0) {

		WARNING("DeviceVersion or ModuleType test failed.");

		goto TEST_END;

	}

	times = 0;
	timeouts = 0;
	if (test_types & (_MAX_CHECK | _MIN_CHECK | _ACCORD_CHECK | _OFFSET_CHECK | _JITTER_CHECK | _KEY_MAX_CHECK | _KEY_MIN_CHECK | _UNIFORMITY_CHECK)) {

		while (times < 16) {

			//GTP_ERROR("read_rawdata:time[%d].\n",times);
			ret = read_raw_data(rawdata, sys.sensor_num * sys.driver_num);

			if (ret < 0) {

				GTP_ERROR("read rawdata timeout %d.", timeouts);

				if (++timeouts > 5) {

					WARNING("read rawdata timeout.");

					break;

				}

				be_normal();

				continue;

			}

			ret = _check_rawdata_proc(test_types, rawdata, sys.sensor_num * sys.driver_num, save_path);

			if (ret < 0) {

				WARNING("raw data test proc error.");

				break;

			}

			else if (ret == 1) {
				break;

			}

			times++;

		}

		be_normal();

		if (ret < 0) {

			WARNING("rawdata check fail.");

			goto TEST_END;

		}
		if (check_cfg == 1) {
			check_cfg++;
			if (_check_config() < 0) {
				WARNING("The configuration is accidental changes.");
				disable_hopping(original_cfg, sys.config_length, 0);
				goto TEST_START;
			}
		}
	}

TEST_COMPLETE:

	ret = test_error_code;

#if GTP_SAVE_TEST_DATA
//  if ((check_types & _TEST_RESULT_SAVE) != 0)
	{

		_save_test_result_data(save_path, test_types, short_result);

	}

#endif

TEST_END:

	if (sys.chip_type == _GT1143) {
		reset_guitar();

		usleep(2 * 1000);
	}

	enable_irq_esd();

	_exit_test();

	GTP_ERROR("test result 0x%X", ret);

	free(largebuf);

	return ret;
}

#ifdef __cplusplus
//}
#endif
