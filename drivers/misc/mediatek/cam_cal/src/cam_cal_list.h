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
/*****************************************************************************
 *
 * Filename:
 * ---------
 *   cam_cal_list.h
 *
 * Project:
 * --------
 *   ALPS
 *
 * Description:
 * ------------
 *   Header file of CAM_CAL driver
 *
 *
 * Author:
 * -------
 *   LukeHu (MTK10439)
 *
 *============================================================================*/
#ifndef __CAM_CAL_LIST_H
#define __CAM_CAL_LIST_H
#include <linux/i2c.h>

typedef unsigned int (*cam_cal_cmd_func)(struct i2c_client *client, unsigned int addr,
	unsigned char *data, unsigned int size);

typedef unsigned int (*cam_cal_check_func)(struct i2c_client *client,
	cam_cal_cmd_func readCamCalData);

typedef enum {
	CMD_NONE = 0,
	CMD_AUTO,
	CMD_MAIN,
	CMD_MAIN2,
	CMD_SUB,
	CMD_SUB2,
	CMD_BRCB032GWZ,
	CMD_CAT24C16,
	CMD_GT24C32A,
	CMD_NUM
} CAM_CAL_CMD_TYPE;

typedef CAM_CAL_CMD_TYPE cam_cal_cmd_type;

typedef struct {
	unsigned int sensorID;
	unsigned int slaveID;
	cam_cal_cmd_type cmdType;
	cam_cal_check_func checkFunc;
} stCAM_CAL_LIST_STRUCT, *stPCAM_CAL_LIST_STRUCT;

typedef struct {
	cam_cal_cmd_type cmdType;
	cam_cal_cmd_func readCamCalData;
} stCAM_CAL_FUNC_STRUCT, *stPCAM_CAL_FUNC_STRUCT;


unsigned int cam_cal_get_sensor_list(stCAM_CAL_LIST_STRUCT **ppCamcalList);
unsigned int cam_cal_get_func_list(stCAM_CAL_FUNC_STRUCT **ppCamcalFuncList);
unsigned int cam_cal_check_mtk_cid(struct i2c_client *client, cam_cal_cmd_func readCamCalData);
unsigned int cam_cal_check_double_eeprom(struct i2c_client *client,
	cam_cal_cmd_func readCamCalData);

#endif /* __CAM_CAL_LIST_H */

