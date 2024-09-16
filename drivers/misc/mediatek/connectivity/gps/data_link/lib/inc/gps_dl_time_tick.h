/*
 * Copyright (C) 2019 MediaTek Inc.
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
#ifndef _GPS_DL_TIME_TICK_H
#define _GPS_DL_TIME_TICK_H

#define GPS_DL_RW_NO_TIMEOUT (-1)
void gps_dl_wait_us(unsigned int us);
#define GDL_WAIT_US(Usec) gps_dl_wait_us(Usec)
unsigned long gps_dl_tick_get(void);
int gps_dl_tick_delta_to_usec(unsigned int tick0, unsigned int tick1);

#endif /* _GPS_DL_TIME_TICK_H */

