/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */


#ifndef __DEVFINO_H__
#define __DEVFINO_H__

/*****************************************************************************
 * MODULE DEFINITION
 *****************************************************************************/
#define MODULE_NAME	 "[devinfo]"
#define DEV_NAME		"devmap"
#define MAJOR_DEV_NUM	196
#define DEVINFO_SEGCODE_INDEX 30

/*****************************************************************************
 * IOCTL DEFINITION
 *****************************************************************************/
#define DEV_IOC_MAGIC	   'd'
#define READ_DEV_DATA	   _IOR(DEV_IOC_MAGIC,  1, unsigned int)
#define DEV_IOC_MAXNR	   (10)

/*****************************************************************************
 * HRID DEFINITION
 *****************************************************************************/
#define HRID_SIZE_MAGIC_NUM              0x56AB0000
#define HRID_SIZE_INDEX                  10
#define HRID_DEFAULT_SIZE                2
#define HRID_MIN_ALLOWED_SIZE            2
#define HRID_MAX_ALLOWED_SIZE            8

#ifdef CONFIG_OF
/*device information data*/
struct devinfo_tag {
	/* device information size */
	u32 data_size;
	/* device information (dynamically allocate in v2 driver) */
	u32 data[0];
};
#endif

#endif /* end of __DEVFINO_H__ */

