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

#define IMX586_MAX_EEPROM_SIZE 0x24D0
extern unsigned int gc02m1bofilm_mipi_raw_read_otp_info(struct i2c_client *client,
		 unsigned int addr, unsigned char *data, unsigned int size);
extern unsigned int ov02b1b_read_otp_info(struct i2c_client *client,
                 unsigned int addr, unsigned char *data, unsigned int size);

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{S5K2LQSX_SENSOR_ID, 0xA0, Common_read_region},
	{S5K4H7_SENSOR_ID, 0xA2, Common_read_region},
	{S5KHM2SP_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	{IMX586_SENSOR_ID, 0xA0, Common_read_region, IMX586_MAX_EEPROM_SIZE},
	{IMX499_SENSOR_ID, 0xA0, Common_read_region},
	{OV64B40SEMCO_SENSOR_ID, 0xA2, Common_read_region},
	{OV64B40OFILM_SENSOR_ID, 0xA2, Common_read_region},
	{OV13B10SUNNY_SENSOR_ID, 0xA2, Common_read_region},
	{OV13B10OFILM_SENSOR_ID, 0xA2, Common_read_region},
	{IMX355OFILM_SENSOR_ID, 0xA0, Common_read_region},
	{IMX355SUNNY_SENSOR_ID, 0xA0, Common_read_region},
        {GC02M1OFILM_SENSOR_ID, 0xA4, Common_read_region},
        {GC02M1SUNNY_SENSOR_ID, 0xA4, Common_read_region},
        {OV02B1BOFILM_SENSOR_ID, 0x78, ov02b1b_read_otp_info},
        {GC02M1BSUNNY_SENSOR_ID, 0x6E, gc02m1bofilm_mipi_raw_read_otp_info},
	{OV64B40SEMCO_INDIA_SENSOR_ID, 0xA2, Common_read_region},
	{OV64B40OFILM_INDIA_SENSOR_ID, 0xA2, Common_read_region},
	{OV13B10SUNNY_INDIA_SENSOR_ID, 0xA2, Common_read_region},
	{OV13B10OFILM_INDIA_SENSOR_ID, 0xA2, Common_read_region},
	{IMX355OFILM_INDIA_SENSOR_ID, 0xA0, Common_read_region},
	{IMX355SUNNY_INDIA_SENSOR_ID, 0xA0, Common_read_region},
        {GC02M1OFILM_INDIA_SENSOR_ID, 0xA4, Common_read_region},
        {GC02M1SUNNY_INDIA_SENSOR_ID, 0xA4, Common_read_region},
        {OV02B1BOFILM_INDIA_SENSOR_ID, 0x78, ov02b1b_read_otp_info},
        {GC02M1BSUNNY_INDIA_SENSOR_ID, 0x6E, gc02m1bofilm_mipi_raw_read_otp_info},
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


