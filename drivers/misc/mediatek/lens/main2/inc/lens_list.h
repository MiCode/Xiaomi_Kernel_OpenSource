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

#ifndef _LENS_LIST_H

#define _LENS_LIST_H

#ifdef CONFIG_MTK_LENS_LC898212XDAF_SUPPORT
#define LC898212XDAF_F_SetI2Cclient LC898212XDAF_F_SetI2Cclient_Main2
#define LC898212XDAF_F_Ioctl LC898212XDAF_F_Ioctl_Main2
#define LC898212XDAF_F_Release LC898212XDAF_F_Release_Main2
extern void LC898212XDAF_F_SetI2Cclient(struct i2c_client *pstAF_I2Cclient, spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898212XDAF_F_Ioctl(struct file *a_pstFile, unsigned int a_u4Command, unsigned long a_u4Param);
extern int LC898212XDAF_F_Release(struct inode *a_pstInode, struct file *a_pstFile);
#endif

#endif
