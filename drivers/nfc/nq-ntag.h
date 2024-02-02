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

#ifndef __NQ_NTAG_H
#define __NQ_NTAG_H

#include <uapi/linux/nfc/ntaginfo.h>

#define DEV_COUNT   1
#define DEVICE_NAME "nq-ntag"
#define CLASS_NAME "nqntag"
#define FD_DISABLE  1
#define FD_ENABLE   0
#define MAX_BUFFER_SIZE    (320)
#define WAKEUP_SRC_TIMEOUT (2000)
#define NTAG_MIN_OFFSET     0
#define NTAG_USER_MEM_SPACE_MAX_OFFSET 56

#endif
