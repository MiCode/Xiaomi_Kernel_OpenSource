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

extern void MAIN2AF_PowerDown(void);

#define AK7371AF_SetI2Cclient AK7371AF_SetI2Cclient_Main
#define AK7371AF_Ioctl AK7371AF_Ioctl_Main
#define AK7371AF_Release AK7371AF_Release_Main
#define AK7371AF_PowerDown AK7371AF_PowerDown_Main
extern int AK7371AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long AK7371AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int AK7371AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int AK7371AF_PowerDown(void);

#define BU6424AF_SetI2Cclient BU6424AF_SetI2Cclient_Main
#define BU6424AF_Ioctl BU6424AF_Ioctl_Main
#define BU6424AF_Release BU6424AF_Release_Main
extern int BU6424AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long BU6424AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int BU6424AF_Release(struct inode *a_pstInode, struct file *a_pstFile);

extern int bu64748af_SetI2Cclient_Main(struct i2c_client *pstAF_I2Cclient,
				       spinlock_t *pAF_SpinLock,
				       int *pAF_Opened);
extern long bu64748af_Ioctl_Main(struct file *a_pstFile,
				 unsigned int a_u4Command,
				 unsigned long a_u4Param);
extern int bu64748af_Release_Main(struct inode *a_pstInode,
				  struct file *a_pstFile);
extern int bu64748af_PowerDown_Main(void);

#define BU6429AF_SetI2Cclient BU6429AF_SetI2Cclient_Main
#define BU6429AF_Ioctl BU6429AF_Ioctl_Main
#define BU6429AF_Release BU6429AF_Release_Main
extern int BU6429AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long BU6429AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int BU6429AF_Release(struct inode *a_pstInode, struct file *a_pstFile);

#ifdef CONFIG_MTK_LENS_BU63165AF_SUPPORT
#define BU63165AF_SetI2Cclient BU63165AF_SetI2Cclient_Main
#define BU63165AF_Ioctl BU63165AF_Ioctl_Main
#define BU63165AF_Release BU63165AF_Release_Main
extern int BU63165AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				  spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long BU63165AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			    unsigned long a_u4Param);
extern int BU63165AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
#else
#define BU63169AF_SetI2Cclient BU63169AF_SetI2Cclient_Main
#define BU63169AF_Ioctl BU63169AF_Ioctl_Main
#define BU63169AF_Release BU63169AF_Release_Main
#define BU63169AF_PowerDown BU63169AF_PowerDown_Main
extern int BU63169AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				  spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long BU63169AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			    unsigned long a_u4Param);
extern int BU63169AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int BU63169AF_PowerDown(void);
#endif

#define DW9714AF_SetI2Cclient DW9714AF_SetI2Cclient_Main
#define DW9714AF_Ioctl DW9714AF_Ioctl_Main
#define DW9714AF_Release DW9714AF_Release_Main
extern int DW9714AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9714AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int DW9714AF_Release(struct inode *a_pstInode, struct file *a_pstFile);

#define FP5510E2AF_SetI2Cclient FP5510E2AF_SetI2Cclient_Main
#define FP5510E2AF_Ioctl FP5510E2AF_Ioctl_Main
#define FP5510E2AF_Release FP5510E2AF_Release_Main
extern int FP5510E2AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
	spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long FP5510E2AF_Ioctl(struct file *a_pstFile,
	unsigned int a_u4Command, unsigned long a_u4Param);
extern int FP5510E2AF_Release(struct inode *a_pstInode,
	struct file *a_pstFile);

#define DW9814AF_SetI2Cclient DW9814AF_SetI2Cclient_Main
#define DW9814AF_Ioctl DW9814AF_Ioctl_Main
#define DW9814AF_Release DW9814AF_Release_Main
extern int DW9814AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9814AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int DW9814AF_Release(struct inode *a_pstInode, struct file *a_pstFile);

#define DW9718AF_SetI2Cclient DW9718AF_SetI2Cclient_Main
#define DW9718AF_Ioctl DW9718AF_Ioctl_Main
#define DW9718AF_Release DW9718AF_Release_Main
extern int DW9718AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9718AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int DW9718AF_Release(struct inode *a_pstInode, struct file *a_pstFile);

#define DW9718SAF_SetI2Cclient DW9718SAF_SetI2Cclient_Main
#define DW9718SAF_Ioctl DW9718SAF_Ioctl_Main
#define DW9718SAF_Release DW9718SAF_Release_Main
#define DW9718SAF_PowerDown DW9718SAF_PowerDown_Main

extern int DW9718SAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				  spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9718SAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			    unsigned long a_u4Param);
extern int DW9718SAF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9718SAF_PowerDown(void);

#define DW9719TAF_SetI2Cclient DW9719TAF_SetI2Cclient_Main
#define DW9719TAF_Ioctl DW9719TAF_Ioctl_Main
#define DW9719TAF_Release DW9719TAF_Release_Main
extern int DW9719TAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				  spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9719TAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			    unsigned long a_u4Param);
extern int DW9719TAF_Release(struct inode *a_pstInode, struct file *a_pstFile);

#define FM50AF_SetI2Cclient FM50AF_SetI2Cclient_Main
#define FM50AF_Ioctl FM50AF_Ioctl_Main
#define FM50AF_Release FM50AF_Release_Main
extern int FM50AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
			       spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long FM50AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			 unsigned long a_u4Param);
extern int FM50AF_Release(struct inode *a_pstInode, struct file *a_pstFile);

#define LC898122AF_SetI2Cclient LC898122AF_SetI2Cclient_Main
#define LC898122AF_Ioctl LC898122AF_Ioctl_Main
#define LC898122AF_Release LC898122AF_Release_Main
extern int LC898122AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898122AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898122AF_Release(struct inode *a_pstInode, struct file *a_pstFile);

#define LC898212AF_SetI2Cclient LC898212AF_SetI2Cclient_Main
#define LC898212AF_Ioctl LC898212AF_Ioctl_Main
#define LC898212AF_Release LC898212AF_Release_Main
extern int LC898212AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898212AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898212AF_Release(struct inode *a_pstInode, struct file *a_pstFile);

#define LC898212XDAF_SetI2Cclient LC898212XDAF_SetI2Cclient_Main
#define LC898212XDAF_Ioctl LC898212XDAF_Ioctl_Main
#define LC898212XDAF_Release LC898212XDAF_Release_Main
extern int LC898212XDAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				     spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898212XDAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			       unsigned long a_u4Param);
extern int LC898212XDAF_Release(struct inode *a_pstInode,
				struct file *a_pstFile);

#define LC898212XD_TVC700_SetI2Cclient LC898212XD_TVC700_SetI2Cclient_Main
#define LC898212XD_TVC700_Ioctl LC898212XD_TVC700_Ioctl_Main
#define LC898212XD_TVC700_Release LC898212XD_TVC700_Release_Main
extern int LC898212XD_TVC700_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
					  spinlock_t *pAF_SpinLock,
					  int *pAF_Opened);
extern long LC898212XD_TVC700_Ioctl(struct file *a_pstFile,
				    unsigned int a_u4Command,
				    unsigned long a_u4Param);
extern int LC898212XD_TVC700_Release(struct inode *a_pstInode,
				     struct file *a_pstFile);

#define LC898214AF_SetI2Cclient LC898214AF_SetI2Cclient_Main
#define LC898214AF_Ioctl LC898214AF_Ioctl_Main
#define LC898214AF_Release LC898214AF_Release_Main
extern int LC898214AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898214AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898214AF_Release(struct inode *a_pstInode, struct file *a_pstFile);

#define LC898217AF_SetI2Cclient LC898217AF_SetI2Cclient_Main
#define LC898217AFA_SetI2Cclient LC898217AFA_SetI2Cclient_Main
#define LC898217AFB_SetI2Cclient LC898217AFB_SetI2Cclient_Main
#define LC898217AFC_SetI2Cclient LC898217AFC_SetI2Cclient_Main
#define LC898217AF_Ioctl LC898217AF_Ioctl_Main
#define LC898217AF_Release LC898217AF_Release_Main
#define LC898217AF_PowerDown LC898217AF_PowerDown_Main
extern int LC898217AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern int LC898217AFA_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				    spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern int LC898217AFB_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				    spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern int LC898217AFC_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				    spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898217AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898217AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int LC898217AF_PowerDown(void);

#define AD5820AF_SetI2Cclient AD5820AF_SetI2Cclient_Main
#define AD5820AF_Ioctl AD5820AF_Ioctl_Main
#define AD5820AF_Release AD5820AF_Release_Main
extern int AD5820AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long AD5820AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int AD5820AF_Release(struct inode *a_pstInode, struct file *a_pstFile);

#define WV511AAF_SetI2Cclient WV511AAF_SetI2Cclient_Main
#define WV511AAF_Ioctl WV511AAF_Ioctl_Main
#define WV511AAF_Release WV511AAF_Release_Main
extern int WV511AAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long WV511AAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int WV511AAF_Release(struct inode *a_pstInode, struct file *a_pstFile);

#endif
