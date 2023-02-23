/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#define MAX_EEPROM_SIZE_16K 0x4000
#define MAX_EEPROM_SIZE_32K 0x8000

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */

	{S5KHPXSEMCO_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_32K}, //??
	{IMX766SUNNY_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K}, //??
	{IMX766OFILM_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K}, //??
	{OV16A1QSUNNY_SENSOR_ID, 0xA2, Common_read_region},//??
	{OV16A1QOFILM_SENSOR_ID, 0xA2, Common_read_region},
	{S5K4H7SUNNY_SENSOR_ID, 0xA0, Common_read_region},
	{S5K4H7OFILM_SENSOR_ID, 0xA0, Common_read_region},//??
	{OV02B10AAC_SENSOR_ID, 0xA4, Common_read_region},
	{OV02B10SUNNY_SENSOR_ID, 0xA4, Common_read_region},

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


