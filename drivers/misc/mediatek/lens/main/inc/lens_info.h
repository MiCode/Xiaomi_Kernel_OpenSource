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

#ifndef _MAIN_LENS_H

#define _MAIN_LENS_H

#include <linux/ioctl.h>
#include "lens_list.h"

#define MAX_NUM_OF_LENS 32

#define AF_MAGIC 'A'

/* AFDRV_XXXX be the same as AF_DRVNAME in (*af).c */
#define AFDRV_AD5820AF "AD5820AF"
#define AFDRV_AD5823 "AD5823"
#define AFDRV_AD5823AF "AD5823AF"
#define AFDRV_AK7345AF "AK7345AF"
#define AFDRV_AK7371AF "AK7371AF"
#define AFDRV_BU63165AF "BU63165AF"
#define AFDRV_BU6424AF "BU6424AF"
#define AFDRV_BU6429AF "BU6429AF"
#define AFDRV_BU64745GWZAF "BU64745GWZAF"
#define AFDRV_DW9714A "DW9714A"
#define AFDRV_DW9714AF "DW9714AF"
#define AFDRV_DW9718AF "DW9718AF"
#define AFDRV_DW9719TAF "DW9719TAF"
#define AFDRV_DW9814AF "DW9814AF"
#define AFDRV_FM50AF "FM50AF"
#define AFDRV_GAF001AF "GAF001AF"
#define AFDRV_GAF002AF "GAF002AF"
#define AFDRV_GAF008AF "GAF008AF"
#define AFDRV_LC898122AF "LC898122AF"
#define AFDRV_LC898212AF "LC898212AF"
#define AFDRV_LC898212XDAF "LC898212XDAF"
#define AFDRV_LC898212XDAF_F "LC898212XDAF_F"
#define AFDRV_LC898214AF "LC898214AF"
#define AFDRV_MT9P017AF "MT9P017AF"
#define AFDRV_OV8825AF "OV8825AF"
#define AFDRV_WV511AAF "WV511AAF"

/* Structures */
typedef struct {
/* current position */
	u32 u4CurrentPosition;
/* macro position */
	u32 u4MacroPosition;
/* Infinity position */
	u32 u4InfPosition;
/* Motor Status */
	bool bIsMotorMoving;
/* Motor Open? */
	bool bIsMotorOpen;
/* Support SR? */
	bool bIsSupportSR;
} stAF_MotorInfo;

/* Structures */
typedef struct {
/* macro position */
	u32 u4MacroPos;
/* Infinity position */
	u32 u4InfPos;
} stAF_MotorCalPos;

/* Structures */
typedef struct {
	u8 uMotorName[32];
} stAF_MotorName;

/* Structures */
typedef struct {
	u32 u4CmdID;
	u32 u4Param;
} stAF_MotorCmd;

/* Structures */
typedef struct {
	u8 uEnable;
	u8 uDrvName[32];
	void (*pAF_SetI2Cclient)(struct i2c_client *pstAF_I2Cclient, spinlock_t *pAF_SpinLock,
				  int *pAF_Opened);
	long (*pAF_Ioctl)(struct file *a_pstFile, unsigned int a_u4Command,
			   unsigned long a_u4Param);
	int (*pAF_Release)(struct inode *a_pstInode, struct file *a_pstFile);
} stAF_DrvList;


/* Control commnad */
/* S means "set through a ptr" */
/* T means "tell by a arg value" */
/* G means "get by a ptr" */
/* Q means "get by return a value" */
/* X means "switch G and S atomically" */
/* H means "switch T and Q atomically" */
#define AFIOC_G_MOTORINFO _IOR(AF_MAGIC, 0, stAF_MotorInfo)

#define AFIOC_T_MOVETO _IOW(AF_MAGIC, 1, u32)

#define AFIOC_T_SETINFPOS _IOW(AF_MAGIC, 2, u32)

#define AFIOC_T_SETMACROPOS _IOW(AF_MAGIC, 3, u32)

#define AFIOC_G_MOTORCALPOS _IOR(AF_MAGIC, 4, stAF_MotorCalPos)

#define AFIOC_S_SETPARA _IOW(AF_MAGIC, 5, stAF_MotorCmd)

#define AFIOC_S_SETDRVNAME _IOW(AF_MAGIC, 10, stAF_MotorName)

#define AFIOC_S_SETPOWERDOWN _IOW(AF_MAGIC, 11, u32)

#endif
