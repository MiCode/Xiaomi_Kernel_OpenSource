/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * @file	mstar_drv_platform_interface.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

/*=============================================================*/

/*=============================================================*/

#include "mstar_drv_platform_interface.h"
#include "mstar_drv_main.h"
#include "mstar_drv_ic_fw_porting_layer.h"
#include "mstar_drv_platform_porting_layer.h"
#include "mstar_drv_utility_adaption.h"

#ifdef CONFIG_ENABLE_HOTKNOT
#include "mstar_drv_hotknot.h"
#endif

/*=============================================================*/

/*=============================================================*/

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
extern u32 g_GestureWakeupMode[2];
extern u8 g_GestureWakeupFlag;

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
extern u8 g_GestureDebugFlag;
extern u8 g_GestureDebugMode;
#endif

#endif

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
extern u8 g_EnableTpProximity;
#endif

#ifdef CONFIG_ENABLE_GLOVE_MODE
extern u8 g_IsEnableGloveMode;
#endif

extern u8 g_IsUpdateFirmware;

/*=============================================================*/

/*=============================================================*/

extern struct input_dev *g_InputDevice;

#ifdef CONFIG_ENABLE_HOTKNOT
extern u8 g_HotKnotState;
extern u32 SLAVE_I2C_ID_DWI2C;
#endif

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
extern u8 g_ForceUpdate;
#endif

extern u8 IS_FIRMWARE_DATA_LOG_ENABLED;

/*=============================================================*/

/*=============================================================*/

#ifdef CONFIG_ENABLE_HOTKNOT
static u8 _gAMStartCmd[4] = {HOTKNOT_SEND, ADAPTIVEMOD_BEGIN, 0, 0};
#endif

/*=============================================================*/

/*=============================================================*/

#ifdef CONFIG_ENABLE_NOTIFIER_FB
int MsDrvInterfaceTouchDeviceFbNotifierCallback(struct notifier_block *pSelf, unsigned long nEvent, void *pData)
{
	struct fb_event *pEventData = pData;
	int *pBlank;

	if (pEventData && pEventData->data && nEvent == FB_EVENT_BLANK) {
		pBlank = pEventData->data;

		if (*pBlank == FB_BLANK_UNBLANK) {
			DBG("*** %s() TP Resume ***\n", __func__);

			if (g_IsUpdateFirmware != 0) {
				DBG("Not allow to power on/off touch ic while update firmware.\n");
				return 0;
			}

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
			if (g_EnableTpProximity == 1) {
				DBG("g_EnableTpProximity = %d\n", g_EnableTpProximity);
				return 0;
			}
#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_HOTKNOT
			if (g_HotKnotState != HOTKNOT_BEFORE_TRANS_STATE && g_HotKnotState != HOTKNOT_TRANS_STATE && g_HotKnotState != HOTKNOT_AFTER_TRANS_STATE)
#endif
			{
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
				if (g_GestureDebugMode == 1)
					DrvIcFwLyrCloseGestureDebugMode();
#endif

				if (g_GestureWakeupFlag == 1)
					DrvIcFwLyrCloseGestureWakeup();
				else
					DrvPlatformLyrEnableFingerTouchReport();
			}
#ifdef CONFIG_ENABLE_HOTKNOT
			else
				DrvPlatformLyrEnableFingerTouchReport();
#endif
#endif

#ifdef CONFIG_ENABLE_HOTKNOT
			if (g_HotKnotState != HOTKNOT_BEFORE_TRANS_STATE && g_HotKnotState != HOTKNOT_TRANS_STATE && g_HotKnotState != HOTKNOT_AFTER_TRANS_STATE)
#endif
				DrvPlatformLyrTouchDevicePowerOn();

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
			{
				u8 szChargerStatus[20] = {0};

				DrvCommonReadFile("/sys/class/power_supply/battery/status", szChargerStatus, 20);

				DBG("*** Battery Status : %s ***\n", szChargerStatus);

				g_ForceUpdate = 1;

				if (strstr(szChargerStatus, "Charging") != NULL || strstr(szChargerStatus, "Full") != NULL || strstr(szChargerStatus, "Fully charged") != NULL)
					DrvFwCtrlChargerDetection(1);
				else
					DrvFwCtrlChargerDetection(0);

				g_ForceUpdate = 0;
			}
#endif

#ifdef CONFIG_ENABLE_GLOVE_MODE
			if (g_IsEnableGloveMode == 1)
				DrvIcFwCtrlOpenGloveMode();
#endif

			if (IS_FIRMWARE_DATA_LOG_ENABLED)
				DrvIcFwLyrRestoreFirmwareModeToLogDataMode();

#ifndef CONFIG_ENABLE_GESTURE_WAKEUP
			DrvPlatformLyrEnableFingerTouchReport();
#endif
		} else if (*pBlank == FB_BLANK_POWERDOWN) {
			DBG("*** %s() TP Suspend ***\n", __func__);

			if (g_IsUpdateFirmware != 0) {
				DBG("Not allow to power on/off touch ic while update firmware.\n");
				return 0;
			}

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
			if (g_EnableTpProximity == 1) {
				DBG("g_EnableTpProximity = %d\n", g_EnableTpProximity);
				return 0;
			}
#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_HOTKNOT
			if (g_HotKnotState != HOTKNOT_BEFORE_TRANS_STATE && g_HotKnotState != HOTKNOT_TRANS_STATE && g_HotKnotState != HOTKNOT_AFTER_TRANS_STATE) {
#endif
			{
				if (g_GestureWakeupMode[0] != 0x00000000 || g_GestureWakeupMode[1] != 0x00000000) {
					DrvIcFwLyrOpenGestureWakeup(&g_GestureWakeupMode[0]);
					return 0;
				}
			}
#endif

#ifdef CONFIG_ENABLE_HOTKNOT
			if (g_HotKnotState == HOTKNOT_BEFORE_TRANS_STATE || g_HotKnotState == HOTKNOT_TRANS_STATE || g_HotKnotState == HOTKNOT_AFTER_TRANS_STATE)
				IicWriteData(SLAVE_I2C_ID_DWI2C, &_gAMStartCmd[0], 4);
#endif


			DrvPlatformLyrFingerTouchReleased(0, 0, 0);
			input_sync(g_InputDevice);

			DrvPlatformLyrDisableFingerTouchReport();

#ifdef CONFIG_ENABLE_HOTKNOT
			if (g_HotKnotState != HOTKNOT_BEFORE_TRANS_STATE && g_HotKnotState != HOTKNOT_TRANS_STATE && g_HotKnotState != HOTKNOT_AFTER_TRANS_STATE)
#endif
				DrvPlatformLyrTouchDevicePowerOff();
		}
	}

	return 0;
}

#else

void MsDrvInterfaceTouchDeviceSuspend(struct early_suspend *pSuspend)
{
	DBG("*** %s() ***\n", __func__);

	if (g_IsUpdateFirmware != 0) {
		DBG("Not allow to power on/off touch ic while update firmware.\n");
		return;
	}

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
	if (g_EnableTpProximity == 1) {
		DBG("g_EnableTpProximity = %d\n", g_EnableTpProximity);
		return;
	}
#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_HOTKNOT
	if (g_HotKnotState != HOTKNOT_BEFORE_TRANS_STATE && g_HotKnotState != HOTKNOT_TRANS_STATE && g_HotKnotState != HOTKNOT_AFTER_TRANS_STATE) {
#endif
		if (g_GestureWakeupMode[0] != 0x00000000 || g_GestureWakeupMode[1] != 0x00000000) {
			DrvIcFwLyrOpenGestureWakeup(&g_GestureWakeupMode[0]);
			return;
		}
	}
#endif

#ifdef CONFIG_ENABLE_HOTKNOT
	if (g_HotKnotState == HOTKNOT_BEFORE_TRANS_STATE || g_HotKnotState == HOTKNOT_TRANS_STATE || g_HotKnotState == HOTKNOT_AFTER_TRANS_STATE)
		IicWriteData(SLAVE_I2C_ID_DWI2C, &_gAMStartCmd[0], 4);
#endif


	DrvPlatformLyrFingerTouchReleased(0, 0, 0);
	input_sync(g_InputDevice);

	DrvPlatformLyrDisableFingerTouchReport();

#ifdef CONFIG_ENABLE_HOTKNOT
	if (g_HotKnotState != HOTKNOT_BEFORE_TRANS_STATE && g_HotKnotState != HOTKNOT_TRANS_STATE && g_HotKnotState != HOTKNOT_AFTER_TRANS_STATE)
#endif
		DrvPlatformLyrTouchDevicePowerOff();
}

void MsDrvInterfaceTouchDeviceResume(struct early_suspend *pSuspend)
{
	DBG("*** %s() ***\n", __func__);

	if (g_IsUpdateFirmware != 0) {
		DBG("Not allow to power on/off touch ic while update firmware.\n");
		return;
	}

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
	if (g_EnableTpProximity == 1) {
		DBG("g_EnableTpProximity = %d\n", g_EnableTpProximity);
		return;
	}
#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_HOTKNOT
	if (g_HotKnotState != HOTKNOT_BEFORE_TRANS_STATE && g_HotKnotState != HOTKNOT_TRANS_STATE && g_HotKnotState != HOTKNOT_AFTER_TRANS_STATE) {
#endif
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
		if (g_GestureDebugMode == 1)
			DrvIcFwLyrCloseGestureDebugMode();
#endif

		if (g_GestureWakeupFlag == 1)
			DrvIcFwLyrCloseGestureWakeup();
		else
			DrvPlatformLyrEnableFingerTouchReport();
	}
#ifdef CONFIG_ENABLE_HOTKNOT
	else
		DrvPlatformLyrEnableFingerTouchReport();
#endif
#endif

#ifdef CONFIG_ENABLE_HOTKNOT
	if (g_HotKnotState != HOTKNOT_BEFORE_TRANS_STATE && g_HotKnotState != HOTKNOT_TRANS_STATE && g_HotKnotState != HOTKNOT_AFTER_TRANS_STATE)
#endif
		DrvPlatformLyrTouchDevicePowerOn();

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
	{
		u8 szChargerStatus[20] = {0};

		DrvCommonReadFile("/sys/class/power_supply/battery/status", szChargerStatus, 20);

		DBG("*** Battery Status : %s ***\n", szChargerStatus);

		g_ForceUpdate = 1;

		if (strstr(szChargerStatus, "Charging") != NULL || strstr(szChargerStatus, "Full") != NULL || strstr(szChargerStatus, "Fully charged") != NULL)
			DrvFwCtrlChargerDetection(1);
		else
			DrvFwCtrlChargerDetection(0);

		g_ForceUpdate = 0;
	}
#endif

#ifdef CONFIG_ENABLE_GLOVE_MODE
	if (g_IsEnableGloveMode == 1) {
		DrvIcFwCtrlOpenGloveMode();
	}
#endif

	if (IS_FIRMWARE_DATA_LOG_ENABLED) {
		DrvIcFwLyrRestoreFirmwareModeToLogDataMode();
	}

#ifndef CONFIG_ENABLE_GESTURE_WAKEUP
	DrvPlatformLyrEnableFingerTouchReport();
#endif
}

#endif

/* probe function is used for matching and initializing input device */
s32 /*__devinit*/ MsDrvInterfaceTouchDeviceProbe(struct i2c_client *pClient, const struct i2c_device_id *pDeviceId)
{
	s32 nRetVal = 0;

	DBG("*** %s() ***\n", __func__);
	DBG("MsDrvInterfaceTouchDeviceProbe\n");
	DrvPlatformLyrInputDeviceInitialize(pClient);

	DrvPlatformLyrTouchDeviceRequestGPIO();

#ifdef CONFIG_ENABLE_REGULATOR_POWER_ON
	DrvPlatformLyrTouchDeviceRegulatorPowerOn();
#endif

	DrvPlatformLyrTouchDevicePowerOn();

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
	DmaAlloc();
#endif
#endif

	nRetVal = DrvMainTouchDeviceInitialize();
	if (nRetVal == -ENODEV) {
		DrvPlatformLyrTouchDeviceRemove(pClient);
		return nRetVal;
	}

	DrvPlatformLyrTouchDeviceRegisterFingerTouchInterruptHandler();

	DrvPlatformLyrTouchDeviceRegisterEarlySuspend();

#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
	DrvIcFwLyrCheckFirmwareUpdateBySwId();
#endif

	DBG("*** MStar touch driver registered ***\n");

	return nRetVal;
}

/* remove function is triggered when the input device is removed from input sub-system */
s32 /*__devexit*/ MsDrvInterfaceTouchDeviceRemove(struct i2c_client *pClient)
{
	DBG("*** %s() ***\n", __func__);

	return DrvPlatformLyrTouchDeviceRemove(pClient);
}

void MsDrvInterfaceTouchDeviceSetIicDataRate(struct i2c_client *pClient, u32 nIicDataRate)
{
	DBG("*** %s() ***\n", __func__);

	DrvPlatformLyrSetIicDataRate(pClient, nIicDataRate);
}
