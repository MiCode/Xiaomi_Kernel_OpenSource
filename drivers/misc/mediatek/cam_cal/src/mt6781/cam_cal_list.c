/*
 * Copyright (C) 2018 MediaTek Inc.
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


extern unsigned int ov02b1b_sunny_read_otp_info(struct i2c_client *client,
                 unsigned int addr, unsigned char *data, unsigned int size);
extern unsigned int ov02b1b_truly_read_otp_info(struct i2c_client *client,
                 unsigned int addr, unsigned char *data, unsigned int size);

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	{S5KHM2SD_MAIN_SUNNY_SENSOR_ID, 0xA2, Common_read_region},
	{S5KHM2SD_MAIN_OFILM_SENSOR_ID, 0xA2, Common_read_region},
	{HI1634Q_FRONT_OFILM_SENSOR_ID, 0xA2, Common_read_region},
	{HI1634Q_FRONT_QTECH_SENSOR_ID, 0xA2, Common_read_region},
	{OV8856_ULTRA_AAC_SENSOR_ID, 0xA0, Common_read_region},
	{IMX355_ULTRA_SUNNY_SENSOR_ID, 0xA0, Common_read_region},
	{OV02B1B_DEPTH_SUNNY_SENSOR_ID, 0xFF, ov02b1b_sunny_read_otp_info},
	{OV02B1B_DEPTH_TRULY_SENSOR_ID, 0xFF, ov02b1b_truly_read_otp_info},
	{GC02M1_MACRO_OFILM_SENSOR_ID, 0xA4, Common_read_region},
	{GC02M1_MACRO_AAC_SENSOR_ID, 0xA4, Common_read_region},
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


