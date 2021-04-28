/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __IMGSENSOR_CA_H__
#define __IMGSENSOR_CA_H__
#include "kd_camera_typedef.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_CA_TA_cmd.h"
struct command_params {
	void *param0;
	void *param1;
	void *param2;
};

unsigned int imgsensor_ca_open(void);
unsigned int imgsensor_ca_invoke_command(enum IMGSENSOR_TEE_CMD cmd,
		struct command_params parms, MUINT32 *ret);
void imgsensor_ca_close(void);
void imgsensor_ca_release(void);
extern int i2c_tui_enable_clock(int id);
extern int i2c_tui_disable_clock(int id);
#endif

