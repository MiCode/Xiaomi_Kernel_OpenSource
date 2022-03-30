/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _KD_IMGSENSOR_API_H_
#define _KD_IMGSENSOR_API_H_

#include "kd_camera_feature.h"

/* API for termal driver use*/
extern unsigned int Get_Camera_Temperature(
	enum CAMERA_DUAL_CAMERA_SENSOR_ENUM senDevId,
	unsigned char *valid,
	unsigned int *temp);
extern unsigned int Switch_Tg_For_Stagger(unsigned int camtg);


#endif
