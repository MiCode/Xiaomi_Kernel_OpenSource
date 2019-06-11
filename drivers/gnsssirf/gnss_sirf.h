/*
 *
 * SiRF GNSS Driver
 *
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _GNSS_SIRF_H_
#define _GNSS_SIRF_H_

#include <linux/ioctl.h>


/* IO Control used to interface with SiRF GNSS receiver */
#define IO_CONTROL_SIRF_MAGIC_CODE    'Q'
#define IO_CONTROL_SIRF_RESET_CLEAR   _IOW(IO_CONTROL_SIRF_MAGIC_CODE, 0, int)
#define IO_CONTROL_SIRF_RESET_SET     _IOW(IO_CONTROL_SIRF_MAGIC_CODE, 1, int)
#define IO_CONTROL_SIRF_ON_OFF_CLEAR  _IOW(IO_CONTROL_SIRF_MAGIC_CODE, 2, int)
#define IO_CONTROL_SIRF_ON_OFF_SET    _IOW(IO_CONTROL_SIRF_MAGIC_CODE, 3, int)

#endif //_GNSS_SIRF_H_
