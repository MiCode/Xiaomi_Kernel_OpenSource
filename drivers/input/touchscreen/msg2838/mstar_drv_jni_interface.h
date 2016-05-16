/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * @file	mstar_drv_jni_interface.h
 *
 * @brief   This file defines the jni interface functions
 *
 *
 */

#ifndef __MSTAR_DRV_JNI_INTERFACE_H__
#define __MSTAR_DRV_JNI_INTERFACE_H__
#include "mstar_drv_common.h"
#ifdef CONFIG_ENABLE_JNI_INTERFACE

typedef struct {
	u64		 nCmdId;
	u64		 nSndCmdDataPtr;
	u64		 nSndCmdLen;
	u64		 nRtnCmdDataPtr;
	u64		 nRtnCmdLen;
} MsgToolDrvCmd_t;


#define MSGTOOL_MAGIC_NUMBER			   96
#define MSGTOOL_IOCTL_RUN_CMD			  _IO(MSGTOOL_MAGIC_NUMBER, 1)


#define MSGTOOL_RESETHW		   0x01
#define MSGTOOL_REGGETXBYTEVALUE  0x02
#define MSGTOOL_HOTKNOTSTATUS	 0x03
#define MSGTOOL_FINGERTOUCH	   0x04
#define MSGTOOL_BYPASSHOTKNOT	 0x05
#define MSGTOOL_DEVICEPOWEROFF	0x06


extern ssize_t MsgToolRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t MsgToolWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern long MsgToolIoctl(struct file *pFile, unsigned int nCmd, unsigned long nArg);
extern void CreateMsgToolMem(void);
extern void DeleteMsgToolMem(void);


#endif
#endif
