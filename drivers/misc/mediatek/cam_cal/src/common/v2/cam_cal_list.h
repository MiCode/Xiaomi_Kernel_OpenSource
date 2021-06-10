/*
 * Copyright (C) 2019 MediaTek Inc.
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
#ifndef __CAM_CAL_LIST_H
#define __CAM_CAL_LIST_H
#include <linux/i2c.h>

#define DEFAULT_MAX_EEPROM_SIZE_8K 0x2000

typedef unsigned int (*cam_cal_cmd_func) (struct i2c_client *client,
	unsigned int addr, unsigned char *data, unsigned int size);

struct stCAM_CAL_LIST_STRUCT {
	unsigned int sensorID;
	unsigned int slaveID;
	cam_cal_cmd_func readCamCalData;
	unsigned int maxEepromSize;
	cam_cal_cmd_func writeCamCalData;
};


unsigned int cam_cal_get_sensor_list
		(struct stCAM_CAL_LIST_STRUCT **ppCamcalList);

#endif				/* __CAM_CAL_LIST_H */
