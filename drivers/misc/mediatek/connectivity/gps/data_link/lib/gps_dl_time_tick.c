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
#include "gps_dl_config.h"
#include "gps_dl_time_tick.h"

#if GPS_DL_ON_LINUX
#include <linux/delay.h>
#include <linux/jiffies.h>
#elif GPS_DL_ON_CTP
#include "kernel_to_ctp.h"
#endif

void gps_dl_wait_us(unsigned int us)
{
#if GPS_DL_ON_LINUX
	udelay(us);
#elif GPS_DL_ON_CTP
	udelay(us); /* GPT_Delay_us(us); */
#endif
}

unsigned long gps_dl_tick_get(void)
{
#if GPS_DL_ON_LINUX
	return jiffies;
#elif GPS_DL_ON_CTP
	return GPT_GetTickCount(0);
#else
	return 0;
#endif
}

int gps_dl_tick_delta_to_usec(unsigned int tick0, unsigned int tick1)
{
#if GPS_DL_ON_LINUX
	return (int)((tick1 - tick0) * 1000 * 1000 / HZ);
#elif GPS_DL_ON_CTP
	return (int)((tick1 - tick0) / 13);
#else
	return 0;
#endif
}

