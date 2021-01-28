/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



#ifndef _LENS_LIST_H

#define _LENS_LIST_H


#define BU24253AF_SetI2Cclient BU24253AF_SetI2Cclient_Main3
#define BU24253AF_Ioctl BU24253AF_Ioctl_Main3
#define BU24253AF_Release BU24253AF_Release_Main3
#define BU24253AF_PowerDown BU24253AF_PowerDown_Main3
#define BU24253AF_GetFileName BU24253AF_GetFileName_Main3
extern int BU24253AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long BU24253AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int BU24253AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int BU24253AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int BU24253AF_GetFileName(unsigned char *pFileName);

#define GT9772AF_SetI2Cclient GT9772AF_SetI2Cclient_Main3
#define GT9772AF_Ioctl GT9772AF_Ioctl_Main3
#define GT9772AF_Release GT9772AF_Release_Main3
#define GT9772AF_PowerDown GT9772AF_PowerDown_Main3
#define GT9772AF_GetFileName GT9772AF_GetFileName_Main3
extern int GT9772AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long GT9772AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
				unsigned long a_u4Param);
extern int GT9772AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int GT9772AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int GT9772AF_GetFileName(unsigned char *pFileName);

#endif
