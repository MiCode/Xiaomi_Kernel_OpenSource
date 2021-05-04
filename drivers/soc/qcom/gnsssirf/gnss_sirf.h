/* SPDX-License-Identifier: GPL-2.0-only */
// Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
/*
 *
 * SiRF GNSS Driver
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
#define IO_CONTROL_SIRF_BOOT_CLEAR    _IOW(IO_CONTROL_SIRF_MAGIC_CODE, 4, int)
#define IO_CONTROL_SIRF_BOOT_SET      _IOW(IO_CONTROL_SIRF_MAGIC_CODE, 5, int)

#endif //_GNSS_SIRF_H_
