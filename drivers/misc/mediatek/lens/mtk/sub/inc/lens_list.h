/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



#ifndef _LENS_LIST_H

#define _LENS_LIST_H

#define AK7371AF_SetI2Cclient AK7371AF_SetI2Cclient_Sub
#define AK7371AF_Ioctl AK7371AF_Ioctl_Sub
#define AK7371AF_Release AK7371AF_Release_Sub
#define AK7371AF_GetFileName AK7371AF_GetFileName_Sub
extern int AK7371AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long AK7371AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int AK7371AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int AK7371AF_GetFileName(unsigned char *pFileName);

#define BU6424AF_SetI2Cclient BU6424AF_SetI2Cclient_Sub
#define BU6424AF_Ioctl BU6424AF_Ioctl_Sub
#define BU6424AF_Release BU6424AF_Release_Sub
#define BU6424AF_GetFileName BU6424AF_GetFileName_Sub
extern int BU6424AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long BU6424AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int BU6424AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int BU6424AF_GetFileName(unsigned char *pFileName);

#define BU6429AF_SetI2Cclient BU6429AF_SetI2Cclient_Sub
#define BU6429AF_Ioctl BU6429AF_Ioctl_Sub
#define BU6429AF_Release BU6429AF_Release_Sub
#define BU6429AF_GetFileName BU6429AF_GetFileName_Sub
extern int BU6429AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long BU6429AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int BU6429AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int BU6429AF_GetFileName(unsigned char *pFileName);

#define DW9714AF_SetI2Cclient DW9714AF_SetI2Cclient_Sub
#define DW9714AF_Ioctl DW9714AF_Ioctl_Sub
#define DW9714AF_Release DW9714AF_Release_Sub
#define DW9714AF_GetFileName DW9714AF_GetFileName_Sub
extern int DW9714AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9714AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int DW9714AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9714AF_GetFileName(unsigned char *pFileName);

#define DW9814AF_SetI2Cclient DW9814AF_SetI2Cclient_Sub
#define DW9814AF_Ioctl DW9814AF_Ioctl_Sub
#define DW9814AF_Release DW9814AF_Release_Sub
extern int DW9814AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9814AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int DW9814AF_Release(struct inode *a_pstInode, struct file *a_pstFile);

#define DW9718AF_SetI2Cclient DW9718AF_SetI2Cclient_Sub
#define DW9718AF_Ioctl DW9718AF_Ioctl_Sub
#define DW9718AF_Release DW9718AF_Release_Sub
#define DW9814AF_GetFileName DW9814AF_GetFileName_Sub
extern int DW9718AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9718AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int DW9718AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9718AF_GetFileName(unsigned char *pFileName);


#define WV511AAF_SetI2Cclient WV511AAF_SetI2Cclient_Sub
#define WV511AAF_Ioctl WV511AAF_Ioctl_Sub
#define WV511AAF_Release WV511AAF_Release_Sub
#define WV511AAF_GetFileName WV511AAF_GetFileName_Sub
extern int WV511AAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long WV511AAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int WV511AAF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int WV511AAF_GetFileName(unsigned char *pFileName);

#endif
