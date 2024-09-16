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
#ifndef _GPS_DL_HW_TYPE_H
#define _GPS_DL_HW_TYPE_H

#include "gps_dl_config.h"

/* to import u32 series types */
#if GPS_DL_ON_LINUX
#include <linux/types.h>
#elif GPS_DL_ON_CTP
#include "kernel_to_ctp.h"
#endif

typedef u32 conn_reg;


#define GPS_DL_HW_INVALID_ADDR (0xFFFFFFFF)

#endif /* _GPS_DL_HW_TYPE_H */

