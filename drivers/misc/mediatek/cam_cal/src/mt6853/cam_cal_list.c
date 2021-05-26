/*
 * Copyright (C) 2018 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#define MAX_EEPROM_SIZE_16K 0x4000

extern unsigned int gc02m1b_read_otp_info(struct i2c_client *client,
	unsigned int addr,
	unsigned char *data,
	unsigned int size);

extern unsigned int gc02m1b_aac_read_otp_info(struct i2c_client *client,
	unsigned int addr,
	unsigned char *data,
	unsigned int size);

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	{OV13B10_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX355_SENSOR_ID, 0xA8, Common_read_region},
	{OV02B10_SENSOR_ID, 0xA4, Common_read_region},
	/*Below is commom sensor */
	{OV48B_QTECH_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV48B_AAC_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV8856_OFILM_FRONT_SENSOR_ID, 0xA2, Common_read_region},
	{OV8856_AAC_FRONT_SENSOR_ID, 0xA2, Common_read_region},
	{HI259H_QTECH_MACRO_SENSOR_ID, 0xA4, Common_read_region},
	{HI259H_AAC_MACRO_SENSOR_ID, 0xA4, Common_read_region},
	{GC02M1B_QTECH_DEPTH_SENSOR_ID, 0x37, gc02m1b_read_otp_info},
	{GC02M1B_AAC_DEPTH_SENSOR_ID, 0x37, gc02m1b_aac_read_otp_info},
	/*  ADD before this line */
	{0, 0, 0}       /*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


