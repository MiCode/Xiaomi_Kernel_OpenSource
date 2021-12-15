/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

/************************************************************************
 * Copyright (C) 2012-2015, Focaltech Systems (R)All Rights Reserved.
 *
 * File Name: Ft_gesture_lib.c
 *
 * Author:
 *
 * Created: 2015-01-01
 *
 * Abstract: function for hand recognition
 *
 ************************************************************************/
#ifndef __LINUX_FT_GESTURE_LIB_H__
#define __LINUX_FT_GESTURE_LIB_H__

/*
 * int fetch_object_sample(unsigned short *datax,unsigned short *datay,
 * unsigned char *dataxy,short pointnum,unsigned long time_stamp);
 */

int fetch_object_sample(unsigned char *buf, short pointnum);

void init_para(int x_pixel, int y_pixel, int time_slot, int cut_x_pixel,
	       int cut_y_pixel);

/* ft_gesture_lib_v1.0_20140820.a */

#endif
