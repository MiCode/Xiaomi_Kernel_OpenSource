/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _UAPI_NTAGINFO_H_
#define _UAPI_NTAGINFO_H_

#include <linux/ioctl.h>

#define NTAG_FD_STATE           _IOW(0xE9, 0x01, unsigned int)
#define NTAG_SET_OFFSET         _IOW(0xE9, 0x02, unsigned int)

#endif
