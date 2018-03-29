/*
 * Copyright (C) 2015 MediaTek Inc.
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

/*****************************************************************************
*
* Filename:
* ---------
*   sn65dsi83.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   sn65dsi83 header file
*
* Author:
* -------
*
****************************************************************************/

#ifndef _sn65dsi83_SW_H_
#define _sn65dsi83_SW_H_

#ifndef BUILD_LK
typedef unsigned char kal_uint8;

int sn65dsi83_read_byte(kal_uint8 cmd, kal_uint8 *returnData);
int sn65dsi83_write_byte(kal_uint8 cmd, kal_uint8 writeData);
#endif

#endif				/* _fan5405_SW_H_ */
