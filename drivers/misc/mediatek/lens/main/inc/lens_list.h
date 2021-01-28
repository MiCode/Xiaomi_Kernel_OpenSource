/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */



#ifndef _LENS_LIST_H

#define _LENS_LIST_H

extern void MAIN2AF_PowerDown(void);

#define AK7371AF_SetI2Cclient AK7371AF_SetI2Cclient_Main
#define AK7371AF_Ioctl AK7371AF_Ioctl_Main
#define AK7371AF_Release AK7371AF_Release_Main
#define AK7371AF_PowerDown AK7371AF_PowerDown_Main
#define AK7371AF_GetFileName AK7371AF_GetFileName_Main
extern int AK7371AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long AK7371AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int AK7371AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int AK7371AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int AK7371AF_GetFileName(unsigned char *pFileName);

#define BU6424AF_SetI2Cclient BU6424AF_SetI2Cclient_Main
#define BU6424AF_Ioctl BU6424AF_Ioctl_Main
#define BU6424AF_Release BU6424AF_Release_Main
#define BU6424AF_GetFileName BU6424AF_GetFileName_Main
extern int BU6424AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long BU6424AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int BU6424AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int BU6424AF_GetFileName(unsigned char *pFileName);

extern int bu64748af_SetI2Cclient_Main(struct i2c_client *pstAF_I2Cclient,
				       spinlock_t *pAF_SpinLock,
				       int *pAF_Opened);
extern long bu64748af_Ioctl_Main(struct file *a_pstFile,
				 unsigned int a_u4Command,
				 unsigned long a_u4Param);
extern int bu64748af_Release_Main(struct inode *a_pstInode,
				  struct file *a_pstFile);
extern int bu64748af_PowerDown_Main(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int bu64748af_GetFileName_Main(unsigned char *pFileName);

#define BU6429AF_SetI2Cclient BU6429AF_SetI2Cclient_Main
#define BU6429AF_Ioctl BU6429AF_Ioctl_Main
#define BU6429AF_Release BU6429AF_Release_Main
#define BU6429AF_GetFileName BU6429AF_GetFileName_Main
extern int BU6429AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long BU6429AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int BU6429AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int BU6429AF_GetFileName(unsigned char *pFileName);

#ifdef CONFIG_MTK_LENS_BU63165AF_SUPPORT
#define BU63165AF_SetI2Cclient BU63165AF_SetI2Cclient_Main
#define BU63165AF_Ioctl BU63165AF_Ioctl_Main
#define BU63165AF_Release BU63165AF_Release_Main
#define BU63165AF_GetFileName BU63165AF_GetFileName_Main
extern int BU63165AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				  spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long BU63165AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			    unsigned long a_u4Param);
extern int BU63165AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int BU63165AF_GetFileName(unsigned char *pFileName);
#else
#define BU63169AF_SetI2Cclient BU63169AF_SetI2Cclient_Main
#define BU63169AF_Ioctl BU63169AF_Ioctl_Main
#define BU63169AF_Release BU63169AF_Release_Main
#define BU63169AF_PowerDown BU63169AF_PowerDown_Main
#define BU63169AF_GetFileName BU63169AF_GetFileName_Main
extern int BU63169AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				  spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long BU63169AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			    unsigned long a_u4Param);
extern int BU63169AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int BU63169AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int BU63169AF_GetFileName(unsigned char *pFileName);
#endif

#define DW9714AF_SetI2Cclient DW9714AF_SetI2Cclient_Main
#define DW9714AF_Ioctl DW9714AF_Ioctl_Main
#define DW9714AF_Release DW9714AF_Release_Main
#define DW9714AF_GetFileName DW9714AF_GetFileName_Main
extern int DW9714AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9714AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int DW9714AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9714AF_GetFileName(unsigned char *pFileName);

#define DW9763AF_SetI2Cclient DW9763AF_SetI2Cclient_Main
#define DW9763AF_Ioctl DW9763AF_Ioctl_Main
#define DW9763AF_Release DW9763AF_Release_Main
#define DW9763AF_GetFileName DW9763AF_GetFileName_Main
extern int DW9763AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9763AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int DW9763AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9763AF_GetFileName(unsigned char *pFileName);

#define FP5510E2AF_SetI2Cclient FP5510E2AF_SetI2Cclient_Main
#define FP5510E2AF_Ioctl FP5510E2AF_Ioctl_Main
#define FP5510E2AF_Release FP5510E2AF_Release_Main
#define FP5510E2AF_GetFileName FP5510E2AF_GetFileName_Main
extern int FP5510E2AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
	spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long FP5510E2AF_Ioctl(struct file *a_pstFile,
	unsigned int a_u4Command, unsigned long a_u4Param);
extern int FP5510E2AF_Release(struct inode *a_pstInode,
	struct file *a_pstFile);
extern int FP5510E2AF_GetFileName(unsigned char *pFileName);

#define DW9814AF_SetI2Cclient DW9814AF_SetI2Cclient_Main
#define DW9814AF_Ioctl DW9814AF_Ioctl_Main
#define DW9814AF_Release DW9814AF_Release_Main
#define DW9814AF_GetFileName DW9814AF_GetFileName_Main
extern int DW9814AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9814AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int DW9814AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9814AF_GetFileName(unsigned char *pFileName);

#define DW9718AF_SetI2Cclient DW9718AF_SetI2Cclient_Main
#define DW9718AF_Ioctl DW9718AF_Ioctl_Main
#define DW9718AF_Release DW9718AF_Release_Main
#define DW9718AF_GetFileName DW9718AF_GetFileName_Main
extern int DW9718AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9718AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int DW9718AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9718AF_GetFileName(unsigned char *pFileName);

#define DW9718SAF_SetI2Cclient DW9718SAF_SetI2Cclient_Main
#define DW9718SAF_Ioctl DW9718SAF_Ioctl_Main
#define DW9718SAF_Release DW9718SAF_Release_Main
#define DW9718SAF_PowerDown DW9718SAF_PowerDown_Main
#define DW9718SAF_GetFileName DW9718SAF_GetFileName_Main
extern int DW9718SAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				  spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9718SAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			    unsigned long a_u4Param);
extern int DW9718SAF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9718SAF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int DW9718SAF_GetFileName(unsigned char *pFileName);

#define DW9719TAF_SetI2Cclient DW9719TAF_SetI2Cclient_Main
#define DW9719TAF_Ioctl DW9719TAF_Ioctl_Main
#define DW9719TAF_Release DW9719TAF_Release_Main
#define DW9719TAF_GetFileName DW9719TAF_GetFileName_Main
extern int DW9719TAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				  spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long DW9719TAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			    unsigned long a_u4Param);
extern int DW9719TAF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int DW9719TAF_GetFileName(unsigned char *pFileName);


#define LC898122AF_SetI2Cclient LC898122AF_SetI2Cclient_Main
#define LC898122AF_Ioctl LC898122AF_Ioctl_Main
#define LC898122AF_Release LC898122AF_Release_Main
#define LC898122AF_GetFileName LC898122AF_GetFileName_Main
extern int LC898122AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898122AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898122AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int LC898122AF_GetFileName(unsigned char *pFileName);

#define LC898212AF_SetI2Cclient LC898212AF_SetI2Cclient_Main
#define LC898212AF_Ioctl LC898212AF_Ioctl_Main
#define LC898212AF_Release LC898212AF_Release_Main
#define LC898212AF_GetFileName LC898212AF_GetFileName_Main
extern int LC898212AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898212AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898212AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int LC898212AF_GetFileName(unsigned char *pFileName);

#define LC898212XDAF_SetI2Cclient LC898212XDAF_SetI2Cclient_Main
#define LC898212XDAF_Ioctl LC898212XDAF_Ioctl_Main
#define LC898212XDAF_Release LC898212XDAF_Release_Main
#define LC898212XDAF_GetFileName LC898212XDAF_GetFileName_Main
extern int LC898212XDAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				     spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898212XDAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			       unsigned long a_u4Param);
extern int LC898212XDAF_Release(struct inode *a_pstInode,
				struct file *a_pstFile);
extern int LC898212XDAF_GetFileName(unsigned char *pFileName);

#define LC898214AF_SetI2Cclient LC898214AF_SetI2Cclient_Main
#define LC898214AF_Ioctl LC898214AF_Ioctl_Main
#define LC898214AF_Release LC898214AF_Release_Main
#define LC898214AF_GetFileName LC898214AF_GetFileName_Main
extern int LC898214AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898214AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898214AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int LC898214AF_GetFileName(unsigned char *pFileName);

#define LC898217AF_SetI2Cclient LC898217AF_SetI2Cclient_Main
#define LC898217AF_Ioctl LC898217AF_Ioctl_Main
#define LC898217AF_Release LC898217AF_Release_Main
#define LC898217AF_PowerDown LC898217AF_PowerDown_Main
#define LC898217AF_GetFileName LC898217AF_GetFileName_Main
extern int LC898217AF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898217AF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898217AF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int LC898217AF_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int LC898217AF_GetFileName(unsigned char *pFileName);

#define LC898217AFA_SetI2Cclient LC898217AFA_SetI2Cclient_Main
#define LC898217AFA_Ioctl LC898217AFA_Ioctl_Main
#define LC898217AFA_Release LC898217AFA_Release_Main
#define LC898217AFA_PowerDown LC898217AFA_PowerDown_Main
#define LC898217AFA_GetFileName LC898217AFA_GetFileName_Main
extern int LC898217AFA_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898217AFA_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898217AFA_Release(struct inode *a_pstInode,
			       struct file *a_pstFile);
extern int LC898217AFA_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int LC898217AFA_GetFileName(unsigned char *pFileName);

#define LC898217AFB_SetI2Cclient LC898217AFB_SetI2Cclient_Main
#define LC898217AFB_Ioctl LC898217AFB_Ioctl_Main
#define LC898217AFB_Release LC898217AFB_Release_Main
#define LC898217AFB_PowerDown LC898217AFB_PowerDown_Main
#define LC898217AFB_GetFileName LC898217AFB_GetFileName_Main
extern int LC898217AFB_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898217AFB_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898217AFB_Release(struct inode *a_pstInode,
			       struct file *a_pstFile);
extern int LC898217AFB_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int LC898217AFB_GetFileName(unsigned char *pFileName);

#define LC898217AFC_SetI2Cclient LC898217AFC_SetI2Cclient_Main
#define LC898217AFC_Ioctl LC898217AFC_Ioctl_Main
#define LC898217AFC_Release LC898217AFC_Release_Main
#define LC898217AFC_PowerDown LC898217AFC_PowerDown_Main
#define LC898217AFC_GetFileName LC898217AFC_GetFileName_Main
extern int LC898217AFC_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				   spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long LC898217AFC_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			     unsigned long a_u4Param);
extern int LC898217AFC_Release(struct inode *a_pstInode,
			       struct file *a_pstFile);
extern int LC898217AFC_PowerDown(struct i2c_client *pstAF_I2Cclient,
				int *pAF_Opened);
extern int LC898217AFC_GetFileName(unsigned char *pFileName);

#define WV511AAF_SetI2Cclient WV511AAF_SetI2Cclient_Main
#define WV511AAF_Ioctl WV511AAF_Ioctl_Main
#define WV511AAF_Release WV511AAF_Release_Main
#define WV511AAF_GetFileName WV511AAF_GetFileName_Main
extern int WV511AAF_SetI2Cclient(struct i2c_client *pstAF_I2Cclient,
				 spinlock_t *pAF_SpinLock, int *pAF_Opened);
extern long WV511AAF_Ioctl(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
extern int WV511AAF_Release(struct inode *a_pstInode, struct file *a_pstFile);
extern int WV511AAF_GetFileName(unsigned char *pFileName);

#endif
