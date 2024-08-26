// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"
/*N19A code for HQ-357411 by xiexinli at 2012/12/14 start*/
#define MAX_EEPROM_SIZE_32K 0x8000
/*N19A code for HQ-357411 by xiexinli at 2012/12/14 end*/
struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
/* N19A code for HQ-357415 by p-huabinchen at 2023/12/13 start */
/*N19A code for HQ-357411 by xiexinli at 2012/12/14 start*/
	{S5KHM6_SUNNY_MAIN_SENSOR_ID, 0xA2, Common_read_region,MAX_EEPROM_SIZE_32K},
	{S5KHM6_AAC_MAIN_SENSOR_ID, 0xA2, Common_read_region,MAX_EEPROM_SIZE_32K},
/*N19A code for HQ-357413 by wangjie at 2024/1/16 start*/
	{S5KHM6_TRULY_MAIN_SENSOR_ID, 0xA2, Common_read_region,MAX_EEPROM_SIZE_32K},
/*N19A code for HQ-357413 by wangjie at 2024/1/16 end*/
/*N19A code for HQ-357411 by xiexinli at 2012/12/14 end*/
	{OV13B10_SUNNY_FRONT_SENSOR_ID, 0xA2, Common_read_region},
	{OV13B10_OFILM_FRONT_SENSOR_ID, 0xA2, Common_read_region},
/* N19A code for HQ-357415 by p-huabinchen at 2023/12/13 end */
/* N19A code for HQ-360492 by p-xuyechen at 2023/12/20 */
	{S5K3L6_AAC_FRONT_SENSOR_ID, 0xA2, Common_read_region},
/* N19A code for HQ-357416 by p-xuyechen at 2023/12/13 */
	{GC02M1_SUNNY_MACRO_SENSOR_ID, 0xA4, Common_read_region},
/* N19A code for HQ-357416 by p-xuyechen at 2023/12/17 start*/
	{GC02M1_AAC_MACRO_SENSOR_ID, 0xA4, Common_read_region},
	{GC02M1_TRULY_MACRO_SENSOR_ID, 0xA4, Common_read_region},
/* N19A code for HQ-357416 by p-xuyechen at 2023/12/17 end*/
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2T7SP_SENSOR_ID, 0xA4, Common_read_region},
	{IMX338_SENSOR_ID, 0xA0, Common_read_region},
	{S5K4E6_SENSOR_ID, 0xA8, Common_read_region},
	{IMX386_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3M3_SENSOR_ID, 0xA0, Common_read_region},
	{S5K2L7_SENSOR_ID, 0xA0, Common_read_region},
	{IMX398_SENSOR_ID, 0xA0, Common_read_region},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{IMX318_SENSOR_ID, 0xA0, Common_read_region},
	{IMX386_MONO_SENSOR_ID, 0xA0, Common_read_region},
	/*B+B. No Cal data for main2 OV8856*/
	{S5K2P7_SENSOR_ID, 0xA0, Common_read_region},
#ifdef SUPPORT_S5K4H7
	{S5K4H7_SENSOR_ID, 0xA0, zte_s5k4h7_read_region},
	{S5K4H7SUB_SENSOR_ID, 0xA0, zte_s5k4h7_sub_read_region},
#endif
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


