/*
 * Copyright (C) 2016 MediaTek Inc.
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
