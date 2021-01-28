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

#define AK7371AF_SetI2Cclient AK7371AF_SetI2Cclient_Main2
#define AK7371AF_Ioctl AK7371AF_Ioctl_Main2
#define AK7371AF_Release AK7371AF_Release_Main2
#define AK7371AF_PowerDown AK7371AF_PowerDown_Main2
#define AK7371AF_GetFileName AK7371AF_GetFileName_Main2
extern int AK7371AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long AK7371AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int AK7371AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int AK7371AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int AK7371AF_GetFileName(unsigned char *pFileName);

#define LC898212XDAF_F_SetI2Cclient LC898212XDAF_F_SetI2Cclient_Main2
#define LC898212XDAF_F_Ioctl LC898212XDAF_F_Ioctl_Main2
#define LC898212XDAF_F_Release LC898212XDAF_F_Release_Main2
#define LC898212XDAF_F_GetFileName LC898212XDAF_F_GetFileName_Main2
extern int LC898212XDAF_F_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				       spinlock_t *pAF_SpinLock,
				       int *pAF_Opened);
extern long LC898212XDAF_F_Ioctl(struct file *a_pstFile,
				 unsigned int a_u4Command,
				 unsigned long a_u4Param);
extern int LC898212XDAF_F_Release(struct inode *a_pstInode,
				  struct file *a_pstFile);
extern int LC898212XDAF_F_GetFileName(unsigned char *pFileName);

#define LC898217AF_SetI2Cclient LC898217AF_SetI2Cclient_Main2
#define LC898217AF_Ioctl LC898217AF_Ioctl_Main2
#define LC898217AF_Release LC898217AF_Release_Main2
#define LC898217AF_PowerDown LC898217AF_PowerDown_Main2
#define LC898217AF_GetFileName LC898217AF_GetFileName_Main2
extern int LC898217AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898217AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898217AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int LC898217AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int LC898217AF_GetFileName(unsigned char *pFileName);

#define LC898217AFA_SetI2Cclient LC898217AFA_SetI2Cclient_Main2
#define LC898217AFA_Ioctl LC898217AFA_Ioctl_Main2
#define LC898217AFA_Release LC898217AFA_Release_Main2
#define LC898217AFA_PowerDown LC898217AFA_PowerDown_Main2
#define LC898217AFA_GetFileName LC898217AFA_GetFileName_Main2
extern int LC898217AFA_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898217AFA_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898217AFA_Release(struct inode *a_pstInode,
			       struct file *a_pstFile);
extern int LC898217AFA_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int LC898217AFA_GetFileName(unsigned char *pFileName);

#define LC898217AFB_SetI2Cclient LC898217AFB_SetI2Cclient_Main2
#define LC898217AFB_Ioctl LC898217AFB_Ioctl_Main2
#define LC898217AFB_Release LC898217AFB_Release_Main2
#define LC898217AFB_PowerDown LC898217AFB_PowerDown_Main2
#define LC898217AFB_GetFileName LC898217AFB_GetFileName_Main2
extern int LC898217AFB_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898217AFB_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898217AFB_Release(struct inode *a_pstInode,
			       struct file *a_pstFile);
extern int LC898217AFB_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int LC898217AFB_GetFileName(unsigned char *pFileName);

#define LC898217AFC_SetI2Cclient LC898217AFC_SetI2Cclient_Main2
#define LC898217AFC_Ioctl LC898217AFC_Ioctl_Main2
#define LC898217AFC_Release LC898217AFC_Release_Main2
#define LC898217AFC_PowerDown LC898217AFC_PowerDown_Main2
#define LC898217AFC_GetFileName LC898217AFC_GetFileName_Main2
extern int LC898217AFC_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898217AFC_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898217AFC_Release(struct inode *a_pstInode,
			       struct file *a_pstFile);
extern int LC898217AFC_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int LC898217AFC_GetFileName(unsigned char *pFileName);

extern int bu64748af_SetI2Cclient_Main2(struct i2c_client *pstAF_I2Cclient,
					spinlock_t *pAF_SpinLock,
					int *pAF_Opened);
extern long bu64748af_Ioctl_Main2(struct file *a_pstFile,
				  unsigned int a_u4Command,
				  unsigned long a_u4Param);
extern int bu64748af_Release_Main2(struct inode *a_pstInode,
				   struct file *a_pstFile);
extern int bu64748af_PowerDown_Main2(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int bu64748af_GetFileName_Main2(unsigned char *pFileName);

extern void AFRegulatorCtrl(int Stage);
#endif
