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
extern unsigned int ov02b1b_sunny2_read_otp_info(struct i2c_client *client,
		unsigned int addr, unsigned char *data, unsigned int size);

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
// K7S Start
	{S5KHM2SD_MAIN_OFILM_SENSOR_ID, 0xA2, Common_read_region},
	{S5KHM2SD_MAIN_SEMCO_SENSOR_ID, 0xA2, Common_read_region},
	{IMX471_FRONT_SUNNY_SENSOR_ID,  0xA2, Common_read_region},
	{IMX471_FRONT_OFILM_SENSOR_ID,  0xA2, Common_read_region},
	{IMX355_ULTRA_OFILM_SENSOR_ID,  0xA0, Common_read_region},
	{S5K4H7_ULTRA_SUNNY_SENSOR_ID,  0xA0, Common_read_region},
	{OV02B1B_DEPTH_SUNNY_SENSOR_ID, 0x78, ov02b1b_sunny_read_otp_info},
	{OV02B1B_DEPTH_TRULY_SENSOR_ID, 0x78, ov02b1b_truly_read_otp_info},
	{OV02B1B_DEPTH_SUNNY2_SENSOR_ID, 0x78, ov02b1b_sunny2_read_otp_info},
	{GC02M1_MACRO_OFILM_SENSOR_ID,  0xA4, Common_read_region},
	{GC02M1_MACRO_AAC_SENSOR_ID,    0xA4, Common_read_region},
// K7S End
// K7P Start
	{OV64B40_MAIN_SUNNY_SENSOR_ID,  0xA2, Common_read_region},
	{OV64B40_MAIN_AAC_SENSOR_ID,    0xA2, Common_read_region},
	{OV64B40_MAIN_OFILM_SENSOR_ID,  0xA2, Common_read_region},
// K7P End
	{OV48B_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3P9SP_SENSOR_ID, 0xA0, Common_read_region},
	{OV13B10_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX355_SENSOR_ID, 0xA8, Common_read_region},
	{OV02B10_SENSOR_ID, 0xA4, Common_read_region},
	/*Below is commom sensor */
	{IMX586_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K,
		BL24SA64_write_region},
	{IMX576_SENSOR_ID, 0xA2, Common_read_region},
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{IMX319_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3M5SX_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K,
		BL24SA64_write_region},
	{IMX686_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{HI846_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5KGD1SP_SENSOR_ID, 0xA8, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	{IMX499_SENSOR_ID, 0xA0, Common_read_region},
	{IMX481_SENSOR_ID, 0xA4, Common_read_region, DEFAULT_MAX_EEPROM_SIZE_8K,
		BL24SA64_write_region},
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


