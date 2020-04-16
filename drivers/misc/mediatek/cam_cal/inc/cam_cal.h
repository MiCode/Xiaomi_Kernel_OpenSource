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
#ifndef _CAM_CAL_H
#define _CAM_CAL_H

#include <linux/ioctl.h>
#ifdef CONFIG_COMPAT
/*64 bit*/
#include <linux/fs.h>
#include <linux/compat.h>
#endif

#define CAM_CALAGIC 'i'
/*IOCTRL(inode * ,file * ,cmd ,arg )*/
/*S means "set through a ptr"*/
/*T means "tell by a arg value"*/
/*G means "get by a ptr"*/
/*Q means "get by return a value"*/
/*X means "switch G and S atomically"*/
/*H means "switch T and Q atomically"*/

/**********************************************
 *
 **********************************************/

/*CAM_CAL write*/
#define CAM_CALIOC_S_WRITE _IOW(CAM_CALAGIC, 0, struct stCAM_CAL_INFO_STRUCT)
/*CAM_CAL read*/
#define CAM_CALIOC_G_READ _IOWR(CAM_CALAGIC, 5, struct stCAM_CAL_INFO_STRUCT)

#ifdef CONFIG_COMPAT
#define COMPAT_CAM_CALIOC_S_WRITE \
	_IOW(CAM_CALAGIC, 0, struct COMPAT_stCAM_CAL_INFO_STRUCT)
#define COMPAT_CAM_CALIOC_G_READ \
	_IOWR(CAM_CALAGIC, 5, struct COMPAT_stCAM_CAL_INFO_STRUCT)
#endif
#endif /*_CAM_CAL_H*/
