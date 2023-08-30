 /*
  * Goodix Touchscreen Driver
  * Copyright (C) 2020 - 2021 Goodix, Inc.
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
  */
#include "goodix_ts_core.h"

bool debug_log_flag = false;

/*****************************************************************************
* goodix_append_checksum
* @summary
*    Calcualte data checksum with the specified mode.
*
* @param data
*   data need to be calculate
* @param len
*   data length
* @param mode
*   calculate for u8 or u16 checksum
* @return
*   return the data checksum value.
*
*****************************************************************************/
u32 goodix_append_checksum(u8 *data, int len, int mode)
{
	u32 checksum = 0;
	int i;

	checksum = 0;
	if (mode == CHECKSUM_MODE_U8_LE) {
		for (i = 0; i < len; i++)
			checksum += data[i];
	} else {
		for (i = 0; i < len; i+=2)
			checksum += (data[i] + (data[i+1] << 8));
	}

	if (mode == CHECKSUM_MODE_U8_LE) {
		data[len] = checksum & 0xff;
		data[len + 1] = (checksum >> 8) & 0xff;
		return 0xFFFF & checksum;
	}
	data[len] = checksum & 0xff;
	data[len + 1] = (checksum >> 8) & 0xff;
	data[len + 2] = (checksum >> 16) & 0xff;
	data[len + 3] = (checksum >> 24) & 0xff;
	return checksum;
}

/* checksum_cmp: check data valid or not
 * @data: data need to be check
 * @size: data length need to be check(include the checksum bytes)
 * @mode: compare with U8 or U16 mode
 * */
int checksum_cmp(const u8 *data, int size, int mode)
{
	u32 cal_checksum = 0;
	u32 r_checksum = 0;
	u32 i;

	if (mode == CHECKSUM_MODE_U8_LE) {
		if (size < 2)
			return 1;
		for (i = 0; i < size - 2; i++)
			cal_checksum += data[i];

		r_checksum = data[size - 2] + (data[size - 1] << 8);
		return (cal_checksum & 0xFFFF) == r_checksum ? 0 : 1;
	}

	if (size < 4)
		return 1;
	for (i = 0; i < size - 4; i += 2)
		cal_checksum += data[i] + (data[i + 1] << 8);
	r_checksum = data[size - 4] + (data[size - 3] << 8) +
		(data[size - 2] << 16) + (data[size - 1] << 24);
	return cal_checksum == r_checksum ? 0 : 1;
}

/* return 1 if all data is zero or ff
 * else return 0
 */
int is_risk_data(const u8 *data, int size)
{
	int i;
	int zero_count =  0;
	int ff_count = 0;

	for (i = 0; i < size; i++) {
		if (data[i] == 0)
			zero_count++;
		else if (data[i] == 0xff)
			ff_count++;
	}
	if (zero_count == size || ff_count == size) {
		ts_info("warning data is all %s\n",
			zero_count == size ? "zero" : "0xff");
		return 1;
	}

	return 0;
}

/* get config id form config file */
#define CONFIG_ID_OFFSET 		30
u32 goodix_get_file_config_id(u8 *ic_config)
{
	if (!ic_config)
		return 0;
	return le32_to_cpup((__le32 *)&ic_config[CONFIG_ID_OFFSET]);
}

/* matrix transpose */
void goodix_rotate_abcd2cbad(int tx, int rx, s16 *data)
{
	s16 *temp_buf = NULL;
	int size = tx * rx;
	int i;
	int j;
	int col;

	temp_buf = kcalloc(size, sizeof(s16), GFP_KERNEL);
	if (!temp_buf) {
		ts_err("malloc failed");
		return;
	}

	for (i = 0, j = 0, col = 0; i < size; i++) {
		temp_buf[i] = data[j++ * rx + col];
		if (j == tx) {
			j = 0;
			col++;
		}
	}

	memcpy(data, temp_buf, size * sizeof(s16));
	kfree(temp_buf);
}

/* get ic type */
int goodix_get_ic_type(struct device_node *node)
{
	const char *name_tmp;
	int ret;

	ret = of_property_read_string(node, "compatible", &name_tmp);
	if (ret < 0) {
		ts_err("get compatible failed");
		return ret;
	}

	if (strstr(name_tmp, "9897")) {
		ts_info("ic type is BerlinA");
		ret = IC_TYPE_BERLIN_A;
	} else if (strstr(name_tmp, "9966") || strstr(name_tmp, "7986")) {
		ts_info("ic type is BerlinB");
		ret = IC_TYPE_BERLIN_B;
	} else if (strstr(name_tmp, "9916") || strstr(name_tmp, "spi")) {
		ts_info("ic type is BerlinD");
		ret = IC_TYPE_BERLIN_D;
	} else {
		ts_info("can't find valid ic_type");
		ret = -EINVAL;
	}

	return ret;
}
