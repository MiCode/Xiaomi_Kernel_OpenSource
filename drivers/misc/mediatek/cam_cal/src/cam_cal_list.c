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

#define CAM_CAL_DEBUG
#ifdef CAM_CAL_DEBUG
/*#include <linux/log.h>*/
#include <linux/kern_levels.h>
#define PFX "cam_cal_list"

#define CAM_CALINF(format, args...) \
	pr_info(PFX "[%s] " format, __func__, ##args)
#define CAM_CALDB(format, args...) \
	pr_debug(PFX "[%s] " format, __func__, ##args)
#define CAM_CALERR(format, args...) \
	pr_info(format, ##args)
#else
#define CAM_CALINF(x, ...)
#define CAM_CALDB(x, ...)
#define CAM_CALERR(x, ...)
#endif


#define MTK_MAX_CID_NUM 4
unsigned int mtkCidList[MTK_MAX_CID_NUM] = {
	0x010b00ff,		/*Single MTK Format */
	0x020b00ff,		/*Double MTK Format in One - Legacy */
	0x030b00ff,		/*Double MTK Format in One */
	0x040b00ff		/*Double MTK Format in One V1.4 */
};

struct stCAM_CAL_LIST_STRUCT g_camCalList[] = {
    #if 0
/*Below is commom sensor */
	{IMX230_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{S5K2T7SP_SENSOR_ID, 0xA4, CMD_AUTO, cam_cal_check_double_eeprom},
	{IMX338_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{S5K4E6_SENSOR_ID, 0xA8, CMD_AUTO, cam_cal_check_mtk_cid},
	{IMX386_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{S5K3M3_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{S5K2L7_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{IMX398_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_double_eeprom},
	{IMX318_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{OV8858_SENSOR_ID, 0xA8, CMD_AUTO, cam_cal_check_mtk_cid},
	{IMX386_MONO_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
/*B+B*/
	{S5K2P7_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_double_eeprom},
	{OV8856_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
/*99 */
	{IMX258_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{IMX258_MONO_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
/*97*/
	{OV23850_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{OV23850_SENSOR_ID, 0xA8, CMD_AUTO, cam_cal_check_mtk_cid},
	{S5K3M2_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
/*55*/
	{S5K2P8_SENSOR_ID, 0xA2, CMD_AUTO, cam_cal_check_mtk_cid},
	{S5K2P8_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{OV8858_SENSOR_ID, 0xA2, CMD_AUTO, cam_cal_check_mtk_cid},
/* Others */
	{S5K2X8_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{IMX377_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{IMX214_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{IMX214_MONO_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{OV13855_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{S5K3L8_SENSOR_ID, 0xA0, CMD_AUTO, cam_cal_check_mtk_cid},
	{HI556_SENSOR_ID, 0x51, CMD_AUTO, cam_cal_check_mtk_cid},
	{S5K5E8YX_SENSOR_ID, 0x5a, CMD_AUTO, cam_cal_check_mtk_cid},
	{S5K5E8YXREAR2_SENSOR_ID, 0x5a, CMD_AUTO, cam_cal_check_mtk_cid},
	#endif
	{LOTUS_IMX486_OFILM_SENSOR_ID, 0xa2, CMD_MAIN, cam_cal_check_mtk_cid},
	{LOTUS_OV12A10_SUNNY_SENSOR_ID, 0xa8, CMD_MAIN, cam_cal_check_mtk_cid},
    {LOTUS_OV02A10_OFILM_SENSOR_ID, 0xa8, CMD_AUTO, cam_cal_check_mtk_cid},
    {LOTUS_OV02A10_SUNNY_SENSOR_ID, 0xa2, CMD_AUTO, cam_cal_check_mtk_cid},
    {LOTUS_S5K4H7YX_OFILM_SENSOR_ID, 0x5a, CMD_AUTO, cam_cal_check_mtk_cid},
    {LOTUS_S5K4H7YX_QTECH_SENSOR_ID, 0x5a, CMD_AUTO, cam_cal_check_mtk_cid},
    /*  ADD before this line */
	{0, 0, CMD_NONE, 0}	/*end of list */
};

unsigned int cam_cal_get_sensor_list(
	struct stCAM_CAL_LIST_STRUCT **ppCamcalList)
{
	if (ppCamcalList == NULL)
		return 1;

	*ppCamcalList = &g_camCalList[0];
	return 0;
}


unsigned int cam_cal_check_mtk_cid(
	struct i2c_client *client, cam_cal_cmd_func readCamCalData)
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

	CAM_CALDB("ret =%d\n", ret);
	return ret;
}

unsigned int cam_cal_check_double_eeprom(
	struct i2c_client *client, cam_cal_cmd_func readCamCalData)
{
	unsigned int calibrationID = 0, ret = 1;

	CAM_CALDB("start cam_cal_check_double_eeprom !\n");
	if (readCamCalData != NULL) {
		CAM_CALDB("readCamCalData != NULL !\n");
		readCamCalData(client, 1, (unsigned char *)&calibrationID, 4);
		CAM_CALDB("calibrationID = %x\n", calibrationID);
	}

	return ret;
}
