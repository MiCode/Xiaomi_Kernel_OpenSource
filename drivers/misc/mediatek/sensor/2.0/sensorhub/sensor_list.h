/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _SENSOR_LIST_H_
#define _SENSOR_LIST_H_

#include "hf_manager.h"

int sensor_list_get_list(struct sensor_info *list, unsigned int size);
int sensor_list_init(void);
void sensor_list_exit(void);

#endif
