/*
* Copyright (C) 2016 MediaTek Inc.
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
#include "kd_imgsensor.h"

/*Common EEPRom Driver*/
#include "common/BRCB032GWZ_3/BRCB032GWZ_3.h"
#include "common/cat24c16/cat24c16.h"
#include "common/GT24c32a/GT24c32a.h"


#define CAM_CAL_DEBUG
#ifdef CAM_CAL_DEBUG
/*#include <linux/log.h>*/
#include <linux/kern_levels.h>
#define PFX "cam_cal_list"

#define CAM_CALINF(format, args...)     pr_info(PFX "[%s] " format, __func__, ##args)
#define CAM_CALDB(format, args...)      pr_debug(PFX "[%s] " format, __func__, ##args)
#define CAM_CALERR(format, args...)     pr_info(format, ##args)
#else
#define CAM_CALINF(x, ...)
#define CAM_CALDB(x, ...)
#define CAM_CALERR(x, ...)
#endif


#define MTK_MAX_CID_NUM 3
unsigned int mtkCidList[MTK_MAX_CID_NUM] = {
	0x010b00ff,/*Single MTK Format*/
	0x020b00ff,/*Double MTK Format in One OTP/EEPRom - Legacy*/
	0x030b00ff /*Double MTK Format in One OTP/EEPRom*/
};

stCAM_CAL_FUNC_STRUCT g_camCalCMDFunc[] = {
	{CMD_BRCB032GWZ, brcb032gwz_selective_read_region},
	{CMD_CAT24C16, cat24c16_selective_read_region},
	{CMD_GT24C32A, gt24c32a_selective_read_region},

	/*      ADD before this line */
	{0, 0} /*end of list*/
};

stCAM_CAL_LIST_STRUCT g_camCalList[] = {
	{OV23850_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{OV23850_SENSOR_ID, 0xA8, CMD_AUTO, cam_cal_check_mtk_cid},
	{S5K3M2_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{IMX214_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{IMX214_MONO_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{S5K5E2YA_SENSOR_ID, 0x00, CMD_MAIN, cam_cal_check_double_eeprom},
	{S5K2X8_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{IMX377_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},

	{IMX258_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},/*Dream: main1*/
	{IMX258_MONO_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},/*Dream: main2*/
	/*{OV8865_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},Dream: sub1 for 4cam*/
	/*{S5K5E8YX_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},Dream: sub2 for 4cam*/

	{S5K5E2YA_SENSOR_ID, 0x00, CMD_MAIN, cam_cal_check_double_eeprom},

	{IMX338_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},/*O main */
	{OV8858_SENSOR_ID, 0xA8, CMD_AUTO, cam_cal_check_mtk_cid},/*O sub */

	{S5K2P8_SENSOR_ID, 0xA2, CMD_AUTO, cam_cal_check_mtk_cid},/*J main */
	{OV8858_SENSOR_ID, 0xA2, CMD_AUTO, cam_cal_check_mtk_cid},/*J sub */

	/*  ADD before this line */
	{0, 0, CMD_NONE, 0} /*end of list*/
};

unsigned int cam_cal_get_sensor_list(stCAM_CAL_LIST_STRUCT **ppCamcalList)

{
	if (NULL == ppCamcalList)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


unsigned int cam_cal_get_func_list(stCAM_CAL_FUNC_STRUCT **ppCamcalFuncList)
{
	if (NULL == ppCamcalFuncList)
		return 1;

	*ppCamcalFuncList = &g_camCalCMDFunc[0];
	return 0;
}

unsigned int cam_cal_check_mtk_cid(struct i2c_client *client, cam_cal_cmd_func readCamCalData)
{
	unsigned int calibrationID = 0, ret = 0;
	int j = 0;

	if (readCamCalData != NULL) {
		readCamCalData(client, 1, (unsigned char *)&calibrationID, 4);
		CAM_CALDB("calibrationID = %x\n", calibrationID);
	}

	if (calibrationID != 0)
		for (j = 0; j < MTK_MAX_CID_NUM; j++) {
			CAM_CALDB("mtkCidList[%d] == %x\n", j, calibrationID);
			if (mtkCidList[j] == calibrationID) {
				ret = 1;
				break;
			}
		}

	CAM_CALDB("ret=%d\n", ret);
	return ret;
}

unsigned int cam_cal_check_double_eeprom(struct i2c_client *client, cam_cal_cmd_func readCamCalData)
{
	unsigned int calibrationID = 0, ret = 0;

	CAM_CALDB("start cam_cal_check_double_eeprom !\n");
	if (readCamCalData != NULL) {
		CAM_CALDB("readCamCalData != NULL !\n");
		readCamCalData(client, 1, (unsigned char *)&calibrationID, 4);
		CAM_CALDB("calibrationID = %x\n", calibrationID);
	}

	if (calibrationID == 0x020b00ff || calibrationID == 0x030b00ff)
		ret = 1;


	CAM_CALDB("ret=%d\n", ret);
	return ret;
}



