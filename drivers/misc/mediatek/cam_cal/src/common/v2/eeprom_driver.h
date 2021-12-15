/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __EEPROM_DRIVER_H
#define __EEPROM_DRIVER_H

#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/mutex.h>

#include "cam_cal_define.h"

// TODO: Move to Kconfig
#define MAX_EEPROM_NUMBER 5

#define DEV_NAME_STR_LEN_MAX 50

struct EEPROM_DRV {
	dev_t dev_no;
	struct cdev cdev;
	struct class *pclass;
	char class_name[DEV_NAME_STR_LEN_MAX];

	struct i2c_client *pi2c_client;
	struct mutex eeprom_mutex;
};

struct EEPROM_DRV_FD_DATA {
	struct EEPROM_DRV *pdrv;
	struct CAM_CAL_SENSOR_INFO sensor_info;
};

#endif
