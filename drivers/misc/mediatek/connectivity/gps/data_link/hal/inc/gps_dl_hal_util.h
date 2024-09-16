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
#ifndef _GPS_DL_HAL_UTIL_H
#define _GPS_DL_HAL_UTIL_H



/* bool gps_dl_hal_get_dma_int_status(enum gps_dl_hal_dma_ch_index channel); */


enum GPS_DL_HWCR_OWNER_ENUM {
	GPS_DL_HWCR_OWNER_POS,	/* for power on sequence */
	GPS_DL_HWCR_OWNER_DMA,	/* for dma control */
	GPS_DL_HWCR_OWNER_NUM
};


/* Must claim the owner before access hardware control registers */
enum GDL_RET_STATUS gps_dl_hwcr_access_claim(enum GPS_DL_HWCR_OWNER_ENUM owner);
enum GDL_RET_STATUS gps_dl_hwcr_access_disclaim(enum GPS_DL_HWCR_OWNER_ENUM owner);


#endif /* _GPS_DL_HAL_UTIL_H */

