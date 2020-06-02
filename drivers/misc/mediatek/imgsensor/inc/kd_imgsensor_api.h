/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _KD_IMGSENSOR_API_H_
#define _KD_IMGSENSOR_API_H_

/* API for termal driver use*/
extern MUINT32 Get_Camera_Temperature(
	enum CAMERA_DUAL_CAMERA_SENSOR_ENUM senDevId,
	MUINT8 *valid,
	MUINT32 *temp);

#endif
