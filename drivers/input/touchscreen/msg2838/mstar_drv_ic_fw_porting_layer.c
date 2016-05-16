/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * @file	mstar_drv_ic_fw_porting_layer.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

#include "mstar_drv_ic_fw_porting_layer.h"

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
extern u32 g_GestureWakeupMode[2];
extern u8 g_GestureWakeupFlag;
#endif
void DrvIcFwLyrVariableInitialize(void)
{


	DrvFwCtrlVariableInitialize();
}

void DrvIcFwLyrOptimizeCurrentConsumption(void)
{


	DrvFwCtrlOptimizeCurrentConsumption();
}

u8 DrvIcFwLyrGetChipType(void)
{


	return DrvFwCtrlGetChipType();
}

void DrvIcFwLyrGetCustomerFirmwareVersion(u16 *pMajor, u16 *pMinor, u8 **ppVersion)
{


	DrvFwCtrlGetCustomerFirmwareVersion(pMajor, pMinor, ppVersion);
}

void DrvIcFwLyrGetPlatformFirmwareVersion(u8 **ppVersion)
{


	DrvFwCtrlGetPlatformFirmwareVersion(ppVersion);
}

s32 DrvIcFwLyrUpdateFirmware(u8 szFwData[][1024], EmemType_e eEmemType)
{


	return DrvFwCtrlUpdateFirmware(szFwData, eEmemType);
}

s32 DrvIcFwLyrUpdateFirmwareBySdCard(const char *pFilePath)
{


	return DrvFwCtrlUpdateFirmwareBySdCard(pFilePath);
}

u32 DrvIcFwLyrIsRegisterFingerTouchInterruptHandler(void)
{
	DBG("*** %s() ***\n", __func__);

	return 1;
}

void DrvIcFwLyrHandleFingerTouch(u8 *pPacket, u16 nLength)
{


	DrvFwCtrlHandleFingerTouch();
}



#ifdef CONFIG_ENABLE_GESTURE_WAKEUP

void DrvIcFwLyrOpenGestureWakeup(u32 *pWakeupMode)
{


	DrvFwCtrlOpenGestureWakeup(pWakeupMode);
}

void DrvIcFwLyrCloseGestureWakeup(void)
{


	DrvFwCtrlCloseGestureWakeup();
}

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
void DrvIcFwLyrOpenGestureDebugMode(u8 nGestureFlag)
{


	DrvFwCtrlOpenGestureDebugMode(nGestureFlag);
}

void DrvIcFwLyrCloseGestureDebugMode(void)
{


	DrvFwCtrlCloseGestureDebugMode();
}
#endif

#endif

u32 DrvIcFwLyrReadDQMemValue(u16 nAddr)
{


	return DrvFwCtrlReadDQMemValue(nAddr);
}

void DrvIcFwLyrWriteDQMemValue(u16 nAddr, u32 nData)
{


	DrvFwCtrlWriteDQMemValue(nAddr, nData);
}



#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
u16 DrvIcFwLyrGetFirmwareMode(void)
{


	return DrvFwCtrlGetFirmwareMode();
}
#endif

u16 DrvIcFwLyrChangeFirmwareMode(u16 nMode)
{


	return DrvFwCtrlChangeFirmwareMode(nMode);
}

void DrvIcFwLyrGetFirmwareInfo(FirmwareInfo_t *pInfo)
{


	DrvFwCtrlGetFirmwareInfo(pInfo);
}

void DrvIcFwLyrRestoreFirmwareModeToLogDataMode(void)
{


	DrvFwCtrlRestoreFirmwareModeToLogDataMode();
}



#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
void DrvIcFwLyrCheckFirmwareUpdateBySwId(void)
{


	DrvFwCtrlCheckFirmwareUpdateBySwId();
}
#endif



#ifdef CONFIG_ENABLE_ITO_MP_TEST

void DrvIcFwLyrCreateMpTestWorkQueue(void)
{


	DrvMpTestCreateMpTestWorkQueue();
}

void DrvIcFwLyrScheduleMpTestWork(ItoTestMode_e eItoTestMode)
{


	DrvMpTestScheduleMpTestWork(eItoTestMode);
}

s32 DrvIcFwLyrGetMpTestResult(void)
{


	return DrvMpTestGetTestResult();
}

void DrvIcFwLyrGetMpTestFailChannel(ItoTestMode_e eItoTestMode, u8 *pFailChannel, u32 *pFailChannelCount)
{


	return DrvMpTestGetTestFailChannel(eItoTestMode, pFailChannel, pFailChannelCount);
}

void DrvIcFwLyrGetMpTestDataLog(ItoTestMode_e eItoTestMode, u8 *pDataLog, u32 *pLength)
{


	return DrvMpTestGetTestDataLog(eItoTestMode, pDataLog, pLength);
}

#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
void DrvIcFwLyrGetMpTestScope(TestScopeInfo_t *pInfo)
{


	return DrvMpTestGetTestScope(pInfo);
}
#endif

#endif



#ifdef CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA

#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
void DrvIcFwLyrGetTouchPacketAddress(u16 *pDataAddress, u16 *pFlagAddress)
{


	return DrvFwCtrlGetTouchPacketAddress(pDataAddress, pFlagAddress);
}
#endif

#endif



#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION

s32 DrvIcFwLyrEnableProximity(void)
{


	return DrvFwCtrlEnableProximity();
}

s32 DrvIcFwLyrDisableProximity(void)
{


	return DrvFwCtrlDisableProximity();
}

#endif



#ifdef CONFIG_ENABLE_GLOVE_MODE
void DrvIcFwLyrOpenGloveMode(void)
{


	DrvIcFwCtrlOpenGloveMode();
}

void DrvIcFwLyrCloseGloveMode(void)
{

	DrvIcFwCtrlCloseGloveMode();
}

void DrvIcFwLyrGetGloveInfo(u8 *pGloveMode)
{


	DrvIcFwCtrlGetGloveInfo(pGloveMode);
}
#endif



