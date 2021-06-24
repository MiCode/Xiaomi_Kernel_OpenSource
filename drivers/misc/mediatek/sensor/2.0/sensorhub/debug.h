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

#ifndef _SENSOR_DEBUG_H_
#define _SENSOR_DEBUG_H_

int debug_get_debug(uint8_t sensor_type, uint8_t *buffer, unsigned int len);
int debug_init(void);
void debug_exit(void);

#endif
