/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2020 XiaoMi, Inc.
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

#if 0
#define AK7371AF_SetI2Cclient AK7371AF_SetI2Cclient_Main3
#define AK7371AF_Ioctl AK7371AF_Ioctl_Main3
#define AK7371AF_Release AK7371AF_Release_Main3
#define AK7371AF_PowerDown AK7371AF_PowerDown_Main3
#define AK7371AF_GetFileName AK7371AF_GetFileName_Main3
extern int AK7371AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long AK7371AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int AK7371AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int AK7371AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int AK7371AF_GetFileName(unsigned char *pFileName);
#endif

#define CN3927AF_SetI2Cclient CN3927AF_SetI2Cclient_Main3
#define CN3927AF_Ioctl CN3927AF_Ioctl_Main3
#define CN3927AF_Release CN3927AF_Release_Main3
#define CN3927AF_GetFileName CN3927AF_GetFileName_Main3
extern int CN3927AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long CN3927AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int CN3927AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int CN3927AF_GetFileName(unsigned char *pFileName);
extern void CN3927AF_WriteReg(struct i2c_client *pstAF_I2Cclient, u16 a_u2Data);

#define DW9714VAF_SetI2Cclient DW9714VAF_SetI2Cclient_Main3
#define DW9714VAF_Ioctl DW9714VAF_Ioctl_Main3
#define DW9714VAF_Release DW9714VAF_Release_Main3
#define DW9714VAF_GetFileName DW9714VAF_GetFileName_Main3
extern int DW9714VAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9714VAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int DW9714VAF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9714VAF_GetFileName(unsigned char *pFileName);
extern void DW9714VAF_SwitchToPowerDown(struct i2c_client *pstAF_I2Cclient, bool disable);

#endif
