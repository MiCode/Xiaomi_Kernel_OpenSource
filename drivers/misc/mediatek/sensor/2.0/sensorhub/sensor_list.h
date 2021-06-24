/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef _SENSOR_LIST_H_
#define _SENSOR_LIST_H_

#include "hf_manager.h"

int sensor_list_get_list(struct sensor_info *list, unsigned int size);
int sensor_list_init(void);
void sensor_list_exit(void);

#endif
