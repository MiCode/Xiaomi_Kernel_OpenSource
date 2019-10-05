/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MTK_ISP_SWPM_PLATFORM_H__
#define __MTK_ISP_SWPM_PLATFORM_H__

#define ISP_SWPM_RESERVED_SIZE 256

struct isp_swpm_rec_data {
	/* 4(int) * 256 = 1024 bytes */
	unsigned int isp_data[ISP_SWPM_RESERVED_SIZE];
};

#endif

