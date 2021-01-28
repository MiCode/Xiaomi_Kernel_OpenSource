/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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

