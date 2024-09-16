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
#ifndef _GPS_DL_LIB_MISC_H
#define _GPS_DL_LIB_MISC_H

#include "gps_dl_config.h"
#include "gps_dl_base.h"
#if GPS_DL_ON_LINUX
#include <linux/types.h> /* for bool */
#endif

void gps_dl_hal_show_buf(unsigned char *tag,
	unsigned char *buf, unsigned int len);

bool gps_dl_hal_comp_buf_match(unsigned char *data_buf, unsigned int data_len,
	unsigned char *golden_buf, unsigned int golden_len, unsigned int data_shift);

#endif /* _GPS_DL_LIB_MISC_H */

