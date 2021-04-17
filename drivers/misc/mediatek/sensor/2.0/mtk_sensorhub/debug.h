/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _SENSOR_DEBUG_H_
#define _SENSOR_DEBUG_H_

int debug_get_debug(uint8_t sensor_type, uint8_t *buffer, unsigned int len);
int debug_init(void);
void debug_exit(void);

#endif
