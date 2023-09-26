/*
 * Copyright (C) 2021 MediaTek Inc.
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

#ifndef _CUSTOM_CMD_H_
#define _CUSTOM_CMD_H_

#include "hf_sensor_io.h"

int custom_cmd_comm_with(int sensor_type, struct custom_cmd *cust_cmd);
int custom_cmd_init(void);
void custom_cmd_exit(void);

#endif
