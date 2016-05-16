/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * @file	mstar_drv_ic_fw_porting_layer.h
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

#ifndef __MSTAR_DRV_IC_FW_PORTING_LAYER_H__
#define __MSTAR_DRV_IC_FW_PORTING_LAYER_H__

/*--------------------------------------------------------------------------*/
/* INCLUDE FILE															 */
/*--------------------------------------------------------------------------*/

#include "mstar_drv_common.h"
#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
#include "mstar_drv_mutual_fw_control.h"
#ifdef CONFIG_ENABLE_ITO_MP_TEST
#include "mstar_drv_mutual_mp_test.h"
#endif
#elif defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_SELF_IC)
#include "mstar_drv_self_fw_control.h"
#ifdef CONFIG_ENABLE_ITO_MP_TEST
#include "mstar_drv_self_mp_test.h"
#endif
#endif

/*--------------------------------------------------------------------------*/
/* PREPROCESSOR CONSTANT DEFINITION										 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* GLOBAL FUNCTION DECLARATION											  */
/*--------------------------------------------------------------------------*/

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
extern void DrvIcFwLyrOpenGestureWakeup(u32 *pWakeupMode);
extern void DrvIcFwLyrCloseGestureWakeup(void);

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
extern void DrvIcFwLyrOpenGestureDebugMode(u8 nGestureFlag);
extern void DrvIcFwLyrCloseGestureDebugMode(void);
#endif

#endif

extern u32 DrvIcFwLyrReadDQMemValue(u16 nAddr);
extern void DrvIcFwLyrWriteDQMemValue(u16 nAddr, u32 nData);

extern u16 DrvIcFwLyrChangeFirmwareMode(u16 nMode);
extern void DrvIcFwLyrGetFirmwareInfo(FirmwareInfo_t *pInfo);
#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
extern u16 DrvIcFwLyrGetFirmwareMode(void);
#endif
extern void DrvIcFwLyrRestoreFirmwareModeToLogDataMode(void);

#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
extern void DrvIcFwLyrCheckFirmwareUpdateBySwId(void);
#endif

extern void DrvIcFwLyrVariableInitialize(void);
extern void DrvIcFwLyrOptimizeCurrentConsumption(void);
extern u8 DrvIcFwLyrGetChipType(void);
extern void DrvIcFwLyrGetCustomerFirmwareVersion(u16 *pMajor, u16 *pMinor, u8 **ppVersion);
extern void DrvIcFwLyrGetPlatformFirmwareVersion(u8 **ppVersion);
extern void DrvIcFwLyrHandleFingerTouch(u8 *pPacket, u16 nLength);
extern u32 DrvIcFwLyrIsRegisterFingerTouchInterruptHandler(void);
extern s32 DrvIcFwLyrUpdateFirmware(u8 szFwData[][1024], EmemType_e eEmemType);
extern s32 DrvIcFwLyrUpdateFirmwareBySdCard(const char *pFilePath);

#ifdef CONFIG_ENABLE_ITO_MP_TEST
extern void DrvIcFwLyrCreateMpTestWorkQueue(void);
extern void DrvIcFwLyrScheduleMpTestWork(ItoTestMode_e eItoTestMode);
extern void DrvIcFwLyrGetMpTestDataLog(ItoTestMode_e eItoTestMode, u8 *pDataLog, u32 *pLength);
extern void DrvIcFwLyrGetMpTestFailChannel(ItoTestMode_e eItoTestMode, u8 *pFailChannel, u32 *pFailChannelCount);
extern s32 DrvIcFwLyrGetMpTestResult(void);
#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
extern void DrvIcFwLyrGetMpTestScope(TestScopeInfo_t *pInfo);
#endif
#endif

#ifdef CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA
#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
extern void DrvIcFwLyrGetTouchPacketAddress(u16 *pDataAddress, u16 *pFlagAddress);
#endif
#endif

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
extern s32 DrvIcFwLyrEnableProximity(void);
extern s32 DrvIcFwLyrDisableProximity(void);
#endif

#ifdef CONFIG_ENABLE_GLOVE_MODE
extern void DrvIcFwLyrOpenGloveMode(void);
extern void DrvIcFwLyrCloseGloveMode(void);
extern void DrvIcFwLyrGetGloveInfo(u8 *pGloveMode);
#endif

#endif  /* __MSTAR_DRV_IC_FW_PORTING_LAYER_H__ */
