/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * @file	mstar_drv_jni_interface.c
 *
 * @brief   This file defines the jni interface functions
 *
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <asm/uaccess.h>
#include "mstar_drv_jni_interface.h"
#include "mstar_drv_utility_adaption.h"
#include "mstar_drv_platform_porting_layer.h"


#ifdef CONFIG_ENABLE_JNI_INTERFACE
extern u32 SLAVE_I2C_ID_DBBUS;
extern u32 SLAVE_I2C_ID_DWI2C;
extern u8 g_IsHotknotEnabled;
extern u8 g_IsBypassHotknot;


static MsgToolDrvCmd_t *_gMsgToolCmdIn;
static u8 *_gSndCmdData;
static u8 *_gRtnCmdData;



void _DebugJniShowArray(u8 *pBuf, u16 nLen)
{
	int i;

	for (i = 0; i < nLen; i++) {
		DBG("%02X ", pBuf[i]);

		if (i%16 == 15) {
			DBG("\n");
		}
	}
	DBG("\n");
}


u64 PtrToU64(u8 *pValue)
{
	uintptr_t nValue = (uintptr_t)pValue;
	return (u64)(0xFFFFFFFFFFFFFFFF&nValue);
}

u8 *U64ToPtr(u64 nValue)
{
	uintptr_t pValue = (uintptr_t)nValue;
	return (u8 *)pValue;
}


ssize_t MsgToolRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	long nRet = 0;
	u8 nBusType = 0;
	u16 nReadLen = 0;
	u8 szCmdData[20] = {0};


	DBG("*** %s() ***\n", __func__);

	nBusType = nCount&0xFF;
	nReadLen = (nCount >> 8)&0xFFFF;
	if (nBusType == SLAVE_I2C_ID_DBBUS || nBusType == SLAVE_I2C_ID_DWI2C)
		IicReadData(nBusType, &szCmdData[0], nReadLen);

	nRet = copy_to_user(pBuffer, &szCmdData[0], nReadLen);

	return nRet;
}


ssize_t MsgToolWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	long nRet = 0;
	u8 nBusType = 0;
	u16 nWriteLen = 0;
	u8 szCmdData[20] = {0};


	DBG("*** %s() ***\n", __func__);

	nBusType = nCount&0xFF;
	nWriteLen = (nCount >> 8)&0xFFFF;
	nRet = copy_from_user(szCmdData, &pBuffer[0], nWriteLen);
	if (nBusType == SLAVE_I2C_ID_DBBUS || nBusType == SLAVE_I2C_ID_DWI2C)
		IicWriteData(nBusType, &szCmdData[0], nWriteLen);

	return nRet;
}


void _RegGetXByteData(MsgToolDrvCmd_t *pCmd)
{
	u16 nAddr = 0;

	DBG("*** %s() ***\n", __func__);
	nAddr = (_gSndCmdData[1]<<8)|_gSndCmdData[0];
	RegGetXBitValue(nAddr, _gRtnCmdData, pCmd->nRtnCmdLen, MAX_I2C_TRANSACTION_LENGTH_LIMIT);

}


void _ClearMsgToolMem(void)
{
	DBG("*** %s() ***\n", __func__);

	memset(_gMsgToolCmdIn, 0, sizeof(MsgToolDrvCmd_t));
	memset(_gSndCmdData, 0, 1024);
	memset(_gRtnCmdData, 0, 1024);
}


static MsgToolDrvCmd_t *_TransCmdFromUser(unsigned long nArg)
{
	long nRet;
	MsgToolDrvCmd_t tCmdIn;
	MsgToolDrvCmd_t *pTransCmd;

	DBG("*** %s() ***\n", __func__);
	_ClearMsgToolMem();
	pTransCmd = (MsgToolDrvCmd_t *)_gMsgToolCmdIn;
	nRet = copy_from_user(&tCmdIn, (void *)nArg, sizeof(MsgToolDrvCmd_t));
	pTransCmd->nCmdId = tCmdIn.nCmdId;

	if (tCmdIn.nSndCmdLen > 0) {
		pTransCmd->nSndCmdLen = tCmdIn.nSndCmdLen;
		nRet = copy_from_user(_gSndCmdData, U64ToPtr(tCmdIn.nSndCmdDataPtr), pTransCmd->nSndCmdLen);
	}

	if (tCmdIn.nRtnCmdLen > 0) {
		pTransCmd->nRtnCmdLen = tCmdIn.nRtnCmdLen;
		nRet = copy_from_user(_gRtnCmdData, U64ToPtr(tCmdIn.nRtnCmdDataPtr), pTransCmd->nRtnCmdLen);
	}

	return pTransCmd;
}


static void _TransCmdToUser(MsgToolDrvCmd_t *pTransCmd, unsigned long nArg)
{
	MsgToolDrvCmd_t tCmdOut;
	long nRet;

	DBG("*** %s() ***\n", __func__);
	nRet = copy_from_user(&tCmdOut, (void *)nArg, sizeof(MsgToolDrvCmd_t));

	nRet = copy_to_user(U64ToPtr(tCmdOut.nRtnCmdDataPtr), _gRtnCmdData, tCmdOut.nRtnCmdLen);
}


long MsgToolIoctl(struct file *pFile, unsigned int nCmd, unsigned long nArg)
{
	long nRet = 0;

	DBG("*** %s() ***\n", __func__);
	switch (nCmd) {
	case MSGTOOL_IOCTL_RUN_CMD:
		{
			MsgToolDrvCmd_t *pTransCmd;
			pTransCmd = _TransCmdFromUser(nArg);
			switch (pTransCmd->nCmdId) {
			case MSGTOOL_RESETHW:
				DrvPlatformLyrTouchDeviceResetHw();
				break;
			case MSGTOOL_REGGETXBYTEVALUE:
				_RegGetXByteData(pTransCmd);
				_TransCmdToUser(pTransCmd, nArg);
				break;
			case MSGTOOL_HOTKNOTSTATUS:
				_gRtnCmdData[0] = g_IsHotknotEnabled;
				_TransCmdToUser(pTransCmd, nArg);
				break;
			case MSGTOOL_FINGERTOUCH:
				if (pTransCmd->nSndCmdLen == 1) {
					DBG("*** JNI enable touch ***\n");
					DrvPlatformLyrEnableFingerTouchReport();
				} else if (pTransCmd->nSndCmdLen == 0) {
					DBG("*** JNI disable touch ***\n");
					DrvPlatformLyrDisableFingerTouchReport();
				}
				break;
			case MSGTOOL_BYPASSHOTKNOT:
				if (pTransCmd->nSndCmdLen == 1) {
					DBG("*** JNI enable bypass hotknot ***\n");
					g_IsBypassHotknot = 1;
				} else if (pTransCmd->nSndCmdLen == 0) {
					DBG("*** JNI disable bypass hotknot ***\n");
					g_IsBypassHotknot = 0;
				}
				break;
			case MSGTOOL_DEVICEPOWEROFF:
				DrvPlatformLyrTouchDevicePowerOff();
				break;
			default:
				break;
			}
		}
		break;

	default:
		nRet = -EINVAL;
		break;
	}

	return nRet;
}


void CreateMsgToolMem(void)
{
	DBG("*** %s() ***\n", __func__);

	_gMsgToolCmdIn = (MsgToolDrvCmd_t *)kmalloc(sizeof(MsgToolDrvCmd_t), GFP_KERNEL);
	_gSndCmdData = (u8 *)kmalloc(1024, GFP_KERNEL);
	_gRtnCmdData = (u8 *)kmalloc(1024, GFP_KERNEL);
}


void DeleteMsgToolMem(void)
{
	DBG("*** %s() ***\n", __func__);

	kfree(_gMsgToolCmdIn);
	kfree(_gSndCmdData);
	kfree(_gRtnCmdData);
}

#endif
