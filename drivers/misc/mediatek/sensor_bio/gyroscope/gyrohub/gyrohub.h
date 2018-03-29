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

#ifndef GYROHUB_H
#define GYROHUB_H

#include <linux/ioctl.h>

#define GYROHUB_AXIS_X          0
#define GYROHUB_AXIS_Y          1
#define GYROHUB_AXIS_Z          2
#define GYROHUB_AXES_NUM        3
#define GYROHUB_DATA_LEN        6
#define GYROHUB_SUCCESS             0

#define GYROHUB_BUFSIZE 60

/* 1 rad = 180/PI degree, MAX_LSB = 131, */
/* 180*131/PI = 7506 */
#define DEGREE_TO_RAD	7506

#endif

