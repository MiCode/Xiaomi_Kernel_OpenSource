// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "kd_imgsensor.h"

#define MAX_EEPROM_SIZE_32K 0x8000
#define MAX_EEPROM_SIZE_16K 0x4000
//N17 code for HQ-293329 by wuzhenyue at 2023/04/28 start
extern unsigned int sc202cs_aac_read_otp_info(struct i2c_client *client,unsigned int addr, unsigned char *data, unsigned int size);
extern unsigned int sc202cs_sunny_read_otp_info(struct i2c_client *client,unsigned int addr, unsigned char *data, unsigned int size);
//N17 code for HQ-293329 by wuzhenyue at 2023/05/22 start
extern unsigned int sc202cs_sunny2_read_otp_info(struct i2c_client *client,unsigned int addr, unsigned char *data, unsigned int size);
//N17 code for HQ-293329 by wuzhenyue at 2023/05/22 end
//N17 code for HQ-293329 by wuzhenyue at 2023/04/28 end
struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	/*Below is commom sensor */
/* N17 code for HQ-296243 by changqi at 2023/05/15 start */
/* N17 code for HQ-293326 by zhaoyue start*/
	{OV16A1Q_AAC_FRONT_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV16A1Q_SUNNY_FRONT_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
/* N17 code for HQ-293326 by zhaoyue end*/
/* N17 code for HQ-293325 by chenxiaoyong at 2023/04/21 start */
	{OV64B40_OFILM_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
/* N17 code for HQ-293325 by chenxiaoyong at 2023/04/21 end */
/* N17 code for HQ-293327 by changqi start*/
	{OV08D10_AAC_ULTRA_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{OV08D10_SUNNY_ULTRA_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
/* N17 code for HQ-293327 by changqi end*/
//N17 code for HQ-293325 by wangqiang at 2023/04/27 start
	{S5KHM6_SEMCO_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
//N17 code for HQ-293325 by wangqiang at 2023/04/27 end
//N17 code for HQ-293325 by wangqiang at 2023/04/28 start
	{OV50D40_SUNNY_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
//N17 code for HQ-293325 by wangqiang at 2023/04/28 end
//N17 code for HQ-293329 by wuzhenyue at 2023/04/28 start
	{SC202CS_AAC_DEPTH_SENSOR_ID, 0x6C, sc202cs_aac_read_otp_info},
	{SC202CS_SUNNY_DEPTH_SENSOR_ID, 0x6C, sc202cs_sunny_read_otp_info},
//N17 code for HQ-293329 by wuzhenyue at 2023/05/22 start
	{SC202CS_SUNNY2_DEPTH_SENSOR_ID, 0x6C, sc202cs_sunny2_read_otp_info},
//N17 code for HQ-293329 by wuzhenyue at 2023/05/22 end
//N17 code for HQ-293329 by wuzhenyue at 2023/04/28 end
//N17 code for HQ-293328 by yinrong at 2023/05/05 start
	{SC202PCS_AAC_MACRO_SENSOR_ID, 0xA4, Common_read_region, MAX_EEPROM_SIZE_16K},
//N17 code for HQ-293328 by yinrong at 2023/05/05 end
//N17 code for HQ-293325 by wuzhenyue at 2023/05/5 start
	{S5KHM6_AAC_MAIN_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
//N17 code for HQ-293325 by wuzhenyue at 2023/05/5 end
/* N17 code for HQ-293328 by yinrong at 2023/05/08 start */
	{SC202PCS_SUNNY_MACRO_SENSOR_ID, 0xA4, Common_read_region, MAX_EEPROM_SIZE_16K},
/* N17 code for HQ-293328 by yinrong at 2023/05/08 end */
/* N17 code for HQ-296243 by changqi at 2023/05/15 end */
	{HI1339_SENSOR_ID, 0xB0, Common_read_region},
	{OV13B10LZ_SENSOR_ID, 0xB0, Common_read_region},
	{GC5035_SENSOR_ID,  0x7E, Common_read_region},
	{HI1339SUBOFILM_SENSOR_ID, 0xA2, Common_read_region},
	{HI1339SUBTXD_SENSOR_ID, 0xA2, Common_read_region},
	{OV48B_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX766_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_32K},
	{IMX766DUAL_SENSOR_ID, 0xA2, Common_read_region, MAX_EEPROM_SIZE_16K},
	{S5K3P9SP_SENSOR_ID, 0xA0, Common_read_region},
	{IMX481_SENSOR_ID, 0xA2, Common_read_region},
	{IMX586_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX576_SENSOR_ID, 0xA2, Common_read_region},
	{IMX519_SENSOR_ID, 0xA0, Common_read_region},
	{S5K3M5SX_SENSOR_ID, 0xA0, Common_read_region, MAX_EEPROM_SIZE_16K},
	{IMX350_SENSOR_ID, 0xA0, Common_read_region},
	{IMX499_SENSOR_ID, 0xA0, Common_read_region},
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


