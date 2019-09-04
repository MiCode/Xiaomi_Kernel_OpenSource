/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef _KD_IMGSENSOR_CA_TA_CMD_H_
#define _KD_IMGSENSOR_CA_TA_CMD_H_

enum IMGSENSOR_TEE_CMD {
	IMGSENSOR_TEE_CMD_OPEN = 0x0,
	IMGSENSOR_TEE_CMD_GET_INFO,
	IMGSENSOR_TEE_CMD_GET_RESOLUTION,
	IMGSENSOR_TEE_CMD_FEATURE_CONTROL,
	IMGSENSOR_TEE_CMD_CONTROL,
	IMGSENSOR_TEE_CMD_CLOSE,
	IMGSENSOR_TEE_CMD_DUMP_REG,
	IMGSENSOR_TEE_CMD_GET_I2C_BUS,
	IMGSENSOR_TEE_CMD_SET_SENSOR,
};

#endif
