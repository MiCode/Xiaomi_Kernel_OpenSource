/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * Definitions for akm09912 compass chip.
 */
#ifndef MAGHUB_H
#define MAGHUB_H

#include <linux/ioctl.h>
#define MAGHUB_AXIS_X          0
#define MAGHUB_AXIS_Y          1
#define MAGHUB_AXIS_Z          2
#define MAGHUB_STATUS          2
#define MAGHUB_AXES_NUM        3
#define MAGHUB_DATA_LEN        6

#define MAGHUB_SUCCESS             0

#define MAGHUB_BUFSIZE 60

#define CONVERT_M_DIV		100

#endif
