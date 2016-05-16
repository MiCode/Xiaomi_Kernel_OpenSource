/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * @file	mstar_drv_mutual_fw_control.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

/*=============================================================*/

/*=============================================================*/

#include "mstar_drv_mutual_fw_control.h"
#include "mstar_drv_utility_adaption.h"
#include "mstar_drv_platform_porting_layer.h"

#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)

/*=============================================================*/

/*=============================================================*/

extern u32 SLAVE_I2C_ID_DBBUS;
extern u32 SLAVE_I2C_ID_DWI2C;

#ifdef CONFIG_TP_HAVE_KEY
extern const int g_TpVirtualKey[];

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
extern const int g_TpVirtualKeyDimLocal[][4];
#endif
#endif

extern struct input_dev *g_InputDevice;

extern u8 g_FwData[MAX_UPDATE_FIRMWARE_BUFFER_SIZE][1024];
extern u32 g_FwDataCount;

extern struct mutex g_Mutex;

extern u16 FIRMWARE_MODE_UNKNOWN_MODE;
extern u16 FIRMWARE_MODE_DEMO_MODE;
extern u16 FIRMWARE_MODE_DEBUG_MODE;
extern u16 FIRMWARE_MODE_RAW_DATA_MODE;

extern struct kobject *g_TouchKObj;
extern u8 g_IsSwitchModeByAPK;

extern u8 IS_FIRMWARE_DATA_LOG_ENABLED;
extern u8 IS_FORCE_TO_UPDATE_FIRMWARE_ENABLED;

#ifdef CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA
extern u16 g_FwPacketDataAddress;
extern u16 g_FwPacketFlagAddress;

extern u8 g_FwSupportSegment;
#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
extern struct kobject *g_GestureKObj;
#endif
#endif

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
extern u32 g_IsEnableReportRate;
extern u32 g_InterruptCount;
extern u32 g_ValidTouchCount;

extern struct timeval g_StartTime;
#endif

#ifdef CONFIG_ENABLE_HOTKNOT
extern u8 g_HotKnotState;
#endif

/*=============================================================*/

/*=============================================================*/



static u8 _gOneDimenFwData[MSG28XX_FIRMWARE_WHOLE_SIZE*1024] = {0};

#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
/*
 * Note.
 * Please modify the name of the below .h depends on the vendor TP that you are using.
 */
#include "msg26xxm_xxxx_update_bin.h"
#include "msg26xxm_yyyy_update_bin.h"

#include "msg28xx_xxxx_update_bin.h"
#include "msg28xx_yyyy_update_bin.h"

static u32 _gUpdateRetryCount = UPDATE_FIRMWARE_RETRY_COUNT;
static u32 _gIsUpdateInfoBlockFirst;
static struct work_struct _gUpdateFirmwareBySwIdWork;
static struct workqueue_struct *_gUpdateFirmwareBySwIdWorkQueue;
static u8 _gTempData[1024];
#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
static u32 _gGestureWakeupValue[2] = {0};
#endif

#ifdef CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA
static u8 _gTouchPacketFlag[2] = {0};
#endif

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
static u8 _gChargerPlugIn;
#endif

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
static u8 _gPreviousTouch[MAX_TOUCH_NUM] = {0};
static u8 _gCurrentTouch[MAX_TOUCH_NUM] = {0};
#endif

static u8 _gIsDisableFinagerTouch;

static u16 _gSFR_ADDR3_BYTE0_1_VALUE = 0x0000;
static u16 _gSFR_ADDR3_BYTE2_3_VALUE = 0x0000;


/*=============================================================*/

/*=============================================================*/

u8 g_ChipType = 0;
u8 g_DemoModePacket[DEMO_MODE_PACKET_LENGTH] = {0};

#ifdef CONFIG_ENABLE_HOTKNOT
u8 g_DemoModeHotKnotSndRetPacket[DEMO_HOTKNOT_SEND_RET_LEN] = {0};
u8 g_DebugModeHotKnotSndRetPacket[DEBUG_HOTKNOT_SEND_RET_LEN] = {0};
#endif

FirmwareInfo_t g_FirmwareInfo;
u8 g_LogModePacket[DEBUG_MODE_PACKET_LENGTH] = {0};
u16 g_FirmwareMode;

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP

#if defined(CONFIG_ENABLE_GESTURE_DEBUG_MODE)
u8 _gGestureWakeupPacket[GESTURE_DEBUG_MODE_PACKET_LENGTH] = {0};
#elif defined(CONFIG_ENABLE_GESTURE_INFORMATION_MODE)
u8 _gGestureWakeupPacket[GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH] = {0};
#else
u8 _gGestureWakeupPacket[GESTURE_WAKEUP_PACKET_LENGTH] = {0};
#endif

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
u8 g_GestureDebugFlag = 0x00;
u8 g_GestureDebugMode = 0x00;
u8 g_LogGestureDebug[GESTURE_DEBUG_MODE_PACKET_LENGTH] = {0};
#endif

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
u32 g_LogGestureInfor[GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH] = {0};
#endif

#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
u32 g_GestureWakeupMode[2] = {0xFFFFFFFF, 0xFFFFFFFF};
#else
u32 g_GestureWakeupMode[2] = {0x0000FFFF, 0x00000000};
#endif

u8 g_GestureWakeupFlag = 0;
#endif

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
u8 g_EnableTpProximity = 0;
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
u8 g_FaceClosingTp = 0;
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
u8 g_FaceClosingTp = 1;
#endif
#endif

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
u8 g_ForceUpdate = 0;
#endif

u8 g_IsUpdateFirmware = 0x00;

/*=============================================================*/

/*=============================================================*/

#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID
static void _DrvFwCtrlUpdateFirmwareBySwIdDoWork(struct work_struct *pWork);
#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
static void _DrvFwCtrlCoordinate(u8 *pRawData, u32 *pTranX, u32 *pTranY);
#endif
#endif

/*=============================================================*/

/*=============================================================*/

static s32 _DrvFwCtrlParsePacket(u8 *pPacket, u16 nLength, TouchInfo_t *pInfo)
{
	u32 i;
	u8 nCheckSum = 0;
	u32 nX = 0, nY = 0;

	DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
	if (g_IsEnableReportRate == 1) {
		if (g_InterruptCount == 4294967296) {
			g_InterruptCount = 0;
			DBG("g_InterruptCount reset to 0\n");
		}

		if (g_InterruptCount == 0) {

			do_gettimeofday(&g_StartTime);

			DBG("Start time : %lu sec, %lu msec\n", g_StartTime.tv_sec, g_StartTime.tv_usec);
		}

		g_InterruptCount++;

		DBG("g_InterruptCount = %d\n", g_InterruptCount);
	}
#endif

	DBG("received raw data from touch panel as following:\n");
	DBG("pPacket[0]=%x \n pPacket[1]=%x pPacket[2]=%x pPacket[3]=%x pPacket[4]=%x \n pPacket[5]=%x pPacket[6]=%x pPacket[7]=%x pPacket[8]=%x \n", \
				pPacket[0], pPacket[1], pPacket[2], pPacket[3], pPacket[4], pPacket[5], pPacket[6], pPacket[7], pPacket[8]);

	nCheckSum = DrvCommonCalculateCheckSum(&pPacket[0], (nLength-1));
	DBG("checksum : [%x] == [%x]? \n", pPacket[nLength-1], nCheckSum);

	if (pPacket[nLength-1] != nCheckSum) {
		DBG("WRONG CHECKSUM\n");
		return -EPERM;
	}

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	if (g_GestureWakeupFlag == 1) {
		u8 nWakeupMode = 0;
		u8 bIsCorrectFormat = 0;

		DBG("received raw data from touch panel as following:\n");
		DBG("pPacket[0]=%x \n pPacket[1]=%x pPacket[2]=%x pPacket[3]=%x pPacket[4]=%x pPacket[5]=%x \n", \
				pPacket[0], pPacket[1], pPacket[2], pPacket[3], pPacket[4], pPacket[5]);

		if (pPacket[0] == 0xA7 && pPacket[1] == 0x00 && pPacket[2] == 0x06 && pPacket[3] == PACKET_TYPE_GESTURE_WAKEUP) {
			nWakeupMode = pPacket[4];
			bIsCorrectFormat = 1;
		}
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
		else if (pPacket[0] == 0xA7 && pPacket[1] == 0x00 && pPacket[2] == 0x80 && pPacket[3] == PACKET_TYPE_GESTURE_DEBUG) {
			u32 a = 0;

			nWakeupMode = pPacket[4];
			bIsCorrectFormat = 1;

			for (a = 0; a < 0x80; a++) {
				g_LogGestureDebug[a] = pPacket[a];
			}

			if (!(pPacket[5] >> 7)) {
				nWakeupMode = 0xFE;
				DBG("gesture debug mode LCM flag = 0\n");
			}
		}
#endif

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
		else if (pPacket[0] == 0xA7 && pPacket[1] == 0x00 && pPacket[2] == 0x80 && pPacket[3] == PACKET_TYPE_GESTURE_INFORMATION) {
			u32 a = 0;
			u32 nTmpCount = 0;

			nWakeupMode = pPacket[4];
			bIsCorrectFormat = 1;

			for (a = 0; a < 6; a++) {
				g_LogGestureInfor[nTmpCount] = pPacket[a];
				nTmpCount++;
			}

			for (a = 6; a < 126; a = a+3) {
				u32 nTranX = 0;
				u32 nTranY = 0;

				_DrvFwCtrlCoordinate(&pPacket[a], &nTranX, &nTranY);
				g_LogGestureInfor[nTmpCount] = nTranX;
				nTmpCount++;
				g_LogGestureInfor[nTmpCount] = nTranY;
				nTmpCount++;
			}

			g_LogGestureInfor[nTmpCount] = pPacket[126];
			nTmpCount++;
			g_LogGestureInfor[nTmpCount] = pPacket[127];
			nTmpCount++;
			DBG("gesture information mode Count = %d\n", nTmpCount);
		}
#endif

		if (bIsCorrectFormat) {
			DBG("nWakeupMode = 0x%x\n", nWakeupMode);

			switch (nWakeupMode) {
			case 0x58:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG;
				DBG("Light up screen by DOUBLE_CLICK gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x60:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG;
				DBG("Light up screen by UP_DIRECT gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x61:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG;
				DBG("Light up screen by DOWN_DIRECT gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x62:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG;
				DBG("Light up screen by LEFT_DIRECT gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x63:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG;
				DBG("Light up screen by RIGHT_DIRECT gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x64:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG;
				DBG("Light up screen by m_CHARACTER gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x65:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG;
				DBG("Light up screen by W_CHARACTER gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x66:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG;
				DBG("Light up screen by C_CHARACTER gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x67:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG;
				DBG("Light up screen by e_CHARACTER gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x68:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG;
				DBG("Light up screen by V_CHARACTER gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x69:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG;
				DBG("Light up screen by O_CHARACTER gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x6A:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG;
				DBG("Light up screen by S_CHARACTER gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x6B:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG;
				DBG("Light up screen by Z_CHARACTER gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x6C:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE1_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE1_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x6D:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE2_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE2_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x6E:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE3_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE3_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
			case 0x6F:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE4_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE4_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x70:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE5_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE5_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x71:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE6_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE6_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x72:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE7_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE7_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x73:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE8_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE8_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x74:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE9_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE9_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x75:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE10_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE10_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x76:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE11_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE11_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x77:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE12_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE12_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x78:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE13_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE13_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x79:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE14_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE14_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x7A:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE15_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE15_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x7B:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE16_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE16_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x7C:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE17_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE17_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x7D:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE18_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE18_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x7E:
				_gGestureWakeupValue[0] = GESTURE_WAKEUP_MODE_RESERVE19_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE19_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x7F:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE20_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE20_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x80:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE21_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE21_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x81:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE22_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE22_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x82:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE23_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE23_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x83:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE24_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE24_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x84:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE25_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE25_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x85:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE26_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE26_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x86:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE27_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE27_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x87:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE28_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE28_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x88:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE29_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE29_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x89:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE30_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE30_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x8A:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE31_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE31_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x8B:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE32_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE32_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x8C:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE33_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE33_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x8D:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE34_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE34_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x8E:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE35_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE35_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x8F:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE36_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE36_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x90:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE37_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE37_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x91:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE38_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE38_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x92:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE39_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE39_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x93:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE40_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE40_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x94:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE41_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE41_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x95:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE42_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE42_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x96:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE43_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE43_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x97:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE44_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE44_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x98:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE45_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE45_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x99:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE46_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE46_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x9A:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE47_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE47_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x9B:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE48_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE48_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x9C:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE49_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE49_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x9D:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE50_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE50_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
			case 0x9E:
				_gGestureWakeupValue[1] = GESTURE_WAKEUP_MODE_RESERVE51_FLAG;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_RESERVE51_FLAG gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
#endif

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
			case 0xFF:
				_gGestureWakeupValue[1] = 0xFF;
				DBG("Light up screen by GESTURE_WAKEUP_MODE_FAIL gesture wakeup.\n");
				input_report_key(g_InputDevice, KEY_POWER, 1);
				input_sync(g_InputDevice);
				input_report_key(g_InputDevice, KEY_POWER, 0);
				input_sync(g_InputDevice);
				break;
#endif

			default:
				_gGestureWakeupValue[0] = 0;
				_gGestureWakeupValue[1] = 0;
				DBG("Un-supported gesture wakeup mode. Please check your device driver code.\n");
				break;
			}

			DBG("_gGestureWakeupValue = 0x%x, 0x%x\n", _gGestureWakeupValue[0], _gGestureWakeupValue[1]);
		} else {
			DBG("gesture wakeup packet format is incorrect.\n");
		}

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE

		if (g_GestureKObj != NULL && pPacket[3] == PACKET_TYPE_GESTURE_DEBUG) {
			char *pEnvp[2];
			s32 nRetVal = 0;

			pEnvp[0] = "STATUS=GET_GESTURE_DEBUG";
			pEnvp[1] = NULL;

			nRetVal = kobject_uevent_env(g_GestureKObj, KOBJ_CHANGE, pEnvp);
			DBG("kobject_uevent_env() nRetVal = %d\n", nRetVal);
		}
#endif

		return -EPERM;
	}
#endif


	if (IS_FIRMWARE_DATA_LOG_ENABLED) {
		if (g_FirmwareMode == FIRMWARE_MODE_DEMO_MODE && pPacket[0] != 0x5A) {
#ifdef CONFIG_ENABLE_HOTKNOT
			if (pPacket[3] != HOTKNOT_PACKET_TYPE && pPacket[3] != HOTKNOT_RECEIVE_PACKET_TYPE) {
#endif
				DBG("WRONG DEMO MODE HEADER\n");
				return -EPERM;
			}
		} else if (g_FirmwareMode == FIRMWARE_MODE_DEBUG_MODE && pPacket[0] != 0xA5 && pPacket[0] != 0xAB && (pPacket[0] != 0xA7 && pPacket[3] != PACKET_TYPE_TOOTH_PATTERN)) {
			DBG("WRONG DEBUG MODE HEADER\n");
			return -EPERM;
		}
	} else {
		if (pPacket[0] != 0x5A) {
#ifdef CONFIG_ENABLE_HOTKNOT
			if (pPacket[3] != HOTKNOT_PACKET_TYPE && pPacket[3] != HOTKNOT_RECEIVE_PACKET_TYPE) {
#endif
				DBG("WRONG DEMO MODE HEADER\n");
				return -EPERM;
			}
		}
	}


	if (pPacket[0] == 0x5A) {
#ifdef CONFIG_ENABLE_HOTKNOT
		if (((DemoHotKnotCmdRet_t *)pPacket)->nIdentify == DEMO_PD_PACKET_IDENTIFY) {
			ReportHotKnotCmd(pPacket, nLength);
			return -EPERM;
		} else {
#endif
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				if ((pPacket[(4*i)+1] == 0xFF) && (pPacket[(4*i)+2] == 0xFF) && (pPacket[(4*i)+3] == 0xFF)) {
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
					_gCurrentTouch[i] = 0;
#endif

					continue;
				}

				nX = (((pPacket[(4*i)+1] & 0xF0) << 4) | (pPacket[(4*i)+2]));
				nY = (((pPacket[(4*i)+1] & 0x0F) << 8) | (pPacket[(4*i)+3]));

				pInfo->tPoint[pInfo->nCount].nX = nX * TOUCH_SCREEN_X_MAX / TPD_WIDTH;
				pInfo->tPoint[pInfo->nCount].nY = nY * TOUCH_SCREEN_Y_MAX / TPD_HEIGHT;
				pInfo->tPoint[pInfo->nCount].nP = pPacket[4*(i+1)];
				pInfo->tPoint[pInfo->nCount].nId = i;

				DBG("[x, y]=[%d, %d]\n", nX, nY);
				DBG("point[%d] : (%d, %d) = %d\n", pInfo->tPoint[pInfo->nCount].nId, pInfo->tPoint[pInfo->nCount].nX, pInfo->tPoint[pInfo->nCount].nY, pInfo->tPoint[pInfo->nCount].nP);

				pInfo->nCount++;

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
				_gCurrentTouch[i] = 1;
#endif
			}
		}
	} else if (pPacket[0] == 0xA5 || pPacket[0] == 0xAB) {
		for (i = 0; i < MAX_TOUCH_NUM; i++) {
			if ((pPacket[(3*i)+4] == 0xFF) && (pPacket[(3*i)+5] == 0xFF) && (pPacket[(3*i)+6] == 0xFF)) {
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
				_gCurrentTouch[i] = 0;
#endif

				continue;
			}

			nX = (((pPacket[(3*i)+4] & 0xF0) << 4) | (pPacket[(3*i)+5]));
			nY = (((pPacket[(3*i)+4] & 0x0F) << 8) | (pPacket[(3*i)+6]));

			pInfo->tPoint[pInfo->nCount].nX = nX * TOUCH_SCREEN_X_MAX / TPD_WIDTH;
			pInfo->tPoint[pInfo->nCount].nY = nY * TOUCH_SCREEN_Y_MAX / TPD_HEIGHT;
			pInfo->tPoint[pInfo->nCount].nP = 1;
			pInfo->tPoint[pInfo->nCount].nId = i;

			DBG("[x, y]=[%d, %d]\n", nX, nY);
			DBG("point[%d] : (%d, %d) = %d\n", pInfo->tPoint[pInfo->nCount].nId, pInfo->tPoint[pInfo->nCount].nX, pInfo->tPoint[pInfo->nCount].nY, pInfo->tPoint[pInfo->nCount].nP);

			pInfo->nCount++;

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
			_gCurrentTouch[i] = 1;
#endif
		}


		if (g_TouchKObj != NULL) {
			char *pEnvp[2];
			s32 nRetVal = 0;

			pEnvp[0] = "STATUS=GET_DEBUG_MODE_PACKET";
			pEnvp[1] = NULL;

			nRetVal = kobject_uevent_env(g_TouchKObj, KOBJ_CHANGE, pEnvp);
			DBG("kobject_uevent_env() nRetVal = %d\n", nRetVal);
		}
	} else if (pPacket[0] == 0xA7 && pPacket[3] == PACKET_TYPE_TOOTH_PATTERN) {
		for (i = 0; i < MAX_TOUCH_NUM; i++) {
			if ((pPacket[(3*i)+5] == 0xFF) && (pPacket[(3*i)+6] == 0xFF) && (pPacket[(3*i)+7] == 0xFF)) {
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
				_gCurrentTouch[i] = 0;
#endif

				continue;
			}

			nX = (((pPacket[(3*i)+5] & 0xF0) << 4) | (pPacket[(3*i)+6]));
			nY = (((pPacket[(3*i)+5] & 0x0F) << 8) | (pPacket[(3*i)+7]));

			pInfo->tPoint[pInfo->nCount].nX = nX * TOUCH_SCREEN_X_MAX / TPD_WIDTH;
			pInfo->tPoint[pInfo->nCount].nY = nY * TOUCH_SCREEN_Y_MAX / TPD_HEIGHT;
			pInfo->tPoint[pInfo->nCount].nP = 1;
			pInfo->tPoint[pInfo->nCount].nId = i;

			DBG("[x, y]=[%d, %d]\n", nX, nY);
			DBG("point[%d] : (%d, %d) = %d\n", pInfo->tPoint[pInfo->nCount].nId, pInfo->tPoint[pInfo->nCount].nX, pInfo->tPoint[pInfo->nCount].nY, pInfo->tPoint[pInfo->nCount].nP);

			pInfo->nCount++;

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
			_gCurrentTouch[i] = 1;
#endif
		}


		if (g_TouchKObj != NULL) {
			char *pEnvp[2];
			s32 nRetVal = 0;

			pEnvp[0] = "STATUS=GET_DEBUG_MODE_PACKET";
			pEnvp[1] = NULL;

			nRetVal = kobject_uevent_env(g_TouchKObj, KOBJ_CHANGE, pEnvp);
			DBG("kobject_uevent_env() nRetVal = %d\n", nRetVal);
		}
	}
#ifdef CONFIG_ENABLE_HOTKNOT
	else if (pPacket[0] == 0xA7) {
		if (pPacket[3] == HOTKNOT_PACKET_TYPE || pPacket[3] == HOTKNOT_RECEIVE_PACKET_TYPE) {
			ReportHotKnotCmd(pPacket, nLength);
			return -EPERM;
		}
	}
#endif


#ifdef CONFIG_TP_HAVE_KEY
	if (pPacket[0] == 0x5A) {
		u8 nButton = pPacket[nLength-2];


		if (nButton != 0xFF) {
			DBG("button = %x\n", nButton);

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
			DBG("g_EnableTpProximity = %d, pPacket[nLength-2] = 0x%x\n", g_EnableTpProximity, pPacket[nLength-2]);

			if (g_EnableTpProximity && ((pPacket[nLength-2] == 0x80) || (pPacket[nLength-2] == 0x40))) {
				if (pPacket[nLength-2] == 0x80) {
					g_FaceClosingTp = 1;
				} else if (pPacket[nLength-2] == 0x40) {
					g_FaceClosingTp = 0;
				}

				DBG("g_FaceClosingTp = %d\n", g_FaceClosingTp);

				return -EPERM;
			}
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
			if (g_EnableTpProximity && ((pPacket[nLength-2] == 0x80) || (pPacket[nLength-2] == 0x40))) {
				int nErr;
				hwm_sensor_data tSensorData;

				if (pPacket[nLength-2] == 0x80) {
					g_FaceClosingTp = 0;
				} else if (pPacket[nLength-2] == 0x40) {
					g_FaceClosingTp = 1;
				}

				DBG("g_FaceClosingTp = %d\n", g_FaceClosingTp);


				tSensorData.values[0] = DrvPlatformLyrGetTpPsData();
				tSensorData.value_divide = 1;
				tSensorData.status = SENSOR_STATUS_ACCURACY_MEDIUM;

				if ((nErr = hwmsen_get_interrupt_data(ID_PROXIMITY, &tSensorData))) {
					DBG("call hwmsen_get_interrupt_data() failed = %d\n", nErr);
				}

				return -EPERM;
			}
#endif
#endif

			for (i = 0; i < MAX_KEY_NUM; i++) {
				if ((nButton & (1<<i)) == (1<<i)) {
					if (pInfo->nKeyCode == 0) {
						pInfo->nKeyCode = i;

						DBG("key[%d]=%d ...\n", i, g_TpVirtualKey[i]);

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
						pInfo->nKeyCode = 0xFF;
						pInfo->nCount = 1;
						pInfo->tPoint[0].nX = g_TpVirtualKeyDimLocal[i][0];
						pInfo->tPoint[0].nY = g_TpVirtualKeyDimLocal[i][1];
						pInfo->tPoint[0].nP = 1;
						pInfo->tPoint[0].nId = 0;
#endif
					} else {
						/
						pInfo->nKeyCode = 0xFF;
					}
				}
			}
		} else {
			pInfo->nKeyCode = 0xFF;
		}
	} else if (pPacket[0] == 0xA5 || pPacket[0] == 0xAB || (pPacket[0] == 0xA7 && pPacket[3] == PACKET_TYPE_TOOTH_PATTERN)) {


		if (pPacket[0] == 0xA5) {

		} else if (pPacket[0] == 0xAB || (pPacket[0] == 0xA7 && pPacket[3] == PACKET_TYPE_TOOTH_PATTERN)) {
			u8 nButton = 0xFF;

			if (pPacket[0] == 0xAB) {
				nButton = pPacket[3];
			} else if (pPacket[0] == 0xA7 && pPacket[3] == PACKET_TYPE_TOOTH_PATTERN) {
				nButton = pPacket[4];
			}

			if (nButton != 0xFF) {
				DBG("button = %x\n", nButton);

				for (i = 0; i < MAX_KEY_NUM; i++) {
					if ((nButton & (1<<i)) == (1<<i)) {
						if (pInfo->nKeyCode == 0) {
							pInfo->nKeyCode = i;

							DBG("key[%d]=%d ...\n", i, g_TpVirtualKey[i]);

#ifdef CONFIG_ENABLE_REPORT_KEY_WITH_COORDINATE
							pInfo->nKeyCode = 0xFF;
							pInfo->nCount = 1;
							pInfo->tPoint[0].nX = g_TpVirtualKeyDimLocal[i][0];
							pInfo->tPoint[0].nY = g_TpVirtualKeyDimLocal[i][1];
							pInfo->tPoint[0].nP = 1;
							pInfo->tPoint[0].nId = 0;
#endif
						} else {
							/
							pInfo->nKeyCode = 0xFF;
						}
					}
				}
			} else {
				pInfo->nKeyCode = 0xFF;
			}
		}
	}
#endif

	return 0;
}

static void _DrvFwCtrlStoreFirmwareData(u8 *pBuf, u32 nSize)
{
	u32 nCount = nSize / 1024;
	u32 nRemainder = nSize % 1024;
	u32 i;

	DBG("*** %s() ***\n", __func__);

	if (nCount > 0) {
		for (i = 0; i < nCount; i++) {
			memcpy(g_FwData[g_FwDataCount], pBuf+(i*1024), 1024);

			g_FwDataCount++;
		}

		if (nRemainder > 0) {
			DBG("nRemainder = %d\n", nRemainder);

			memcpy(g_FwData[g_FwDataCount], pBuf+(i*1024), nRemainder);

			g_FwDataCount++;
		}
	} else {
		if (nSize > 0) {
			memcpy(g_FwData[g_FwDataCount], pBuf, nSize);

			g_FwDataCount++;
		}
	}

	DBG("*** g_FwDataCount = %d ***\n", g_FwDataCount);

	if (pBuf != NULL) {
		DBG("*** buf[0] = %c ***\n", pBuf[0]);
	}
}

static u16 _DrvFwCtrlMsg26xxmGetSwId(EmemType_e eEmemType)
{
	u16 nRetVal = 0;
	u16 nRegData = 0;
	u8 szDbBusTxData[5] = {0};
	u8 szDbBusRxData[4] = {0};

	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	RegSetLByteValue(0x0FE6, 0x01);
	RegSet16BitValue(0x3C60, 0xAA55);
	RegSet16BitValue(0x3CE4, 0xA4AB);
	RegSet16BitValue(0x1E04, 0x7d60);
	RegSet16BitValue(0x1E04, 0x829F);
	RegSetLByteValue(0x0FE6, 0x00);

	mdelay(100);


	do {
		nRegData = RegGet16BitValue(0x3CE4);
	} while (nRegData != 0x5B58);

	szDbBusTxData[0] = 0x72;
	if (eEmemType == EMEM_MAIN) {
		szDbBusTxData[1] = 0x00;
		szDbBusTxData[2] = 0x2A;
	} else if (eEmemType == EMEM_INFO) {
		szDbBusTxData[1] = 0x80;
		szDbBusTxData[2] = 0x04;
	}
	szDbBusTxData[3] = 0x00;
	szDbBusTxData[4] = 0x04;

	IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 5);
	IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 4);


	nRetVal = szDbBusRxData[1];
	nRetVal = (nRetVal << 8) | szDbBusRxData[0];

	DBG("SW ID = 0x%x\n", nRetVal);

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	return nRetVal;
}

static s32 _DrvFwCtrlMsg26xxmUpdateFirmware(u8 szFwData[][1024], EmemType_e eEmemType)
{
	u32 i, j;
	u32 nCrcMain, nCrcMainTp;
	u32 nCrcInfo, nCrcInfoTp;
	u16 nRegData = 0;

	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
	DmaReset();
#endif
#endif

	nCrcMain = 0xffffffff;
	nCrcInfo = 0xffffffff;

	DBG("erase 0\n");

	DrvPlatformLyrTouchDeviceResetHw();

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	DBG("erase 1\n");


	RegSetLByteValue(0x0FE6, 0x01);


	RegSet16BitValue(0x3C60, 0xAA55);


	RegSet16BitValue(0x161A, 0xABBA);


	RegSetLByteValue(0x1618, 0x80);

	DBG("erase 2\n");

	RegSetLByteValue(0x1618, 0x40);

	mdelay(10);


	RegSetLByteValue(0x1618, 0x80);

	DBG("erase 3\n");

	if (eEmemType == EMEM_ALL) {
		RegSetLByteValue(0x160E, 0x08);
	} else {
		RegSetLByteValue(0x160E, 0x04);
	}

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	mdelay(1000);
	DBG("erase OK\n");


	DBG("program 0\n");

	DrvPlatformLyrTouchDeviceResetHw();

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	DBG("program 1\n");


	do {
		nRegData = RegGet16BitValue(0x3CE4);
	} while (nRegData != 0x1C70);

	DBG("program 2\n");

	RegSet16BitValue(0x3CE4, 0xE38F);
	mdelay(100);


	do {
		nRegData = RegGet16BitValue(0x3CE4);
	} while (nRegData != 0x2F43);

	DBG("program 3\n");


	DrvCommonCrcInitTable();

	for (i = 0; i < MSG26XXM_FIRMWARE_WHOLE_SIZE; i++) {
		if (i > 31) {
			for (j = 0; j < 1024; j++) {
				nCrcInfo = DrvCommonCrcGetValue(szFwData[i][j], nCrcInfo);
			}
		} else if (i < 31) {
			for (j = 0; j < 1024; j++) {
				nCrcMain = DrvCommonCrcGetValue(szFwData[i][j], nCrcMain);
			}
		} else {
			szFwData[i][1014] = 0x5A;
			szFwData[i][1015] = 0xA5;

			for (j = 0; j < 1016; j++) {
				nCrcMain = DrvCommonCrcGetValue(szFwData[i][j], nCrcMain);
			}
		}

		for (j = 0; j < 8; j++) {
			IicWriteData(SLAVE_I2C_ID_DWI2C, &szFwData[i][j*128], 128);
		}
		mdelay(100);


		do {
			nRegData = RegGet16BitValue(0x3CE4);
		} while (nRegData != 0xD0BC);


		RegSet16BitValue(0x3CE4, 0x2F43);
	}

	DBG("program 4\n");


	RegSet16BitValue(0x3CE4, 0x1380);
	mdelay(100);

	DBG("program 5\n");


	do {
	   nRegData = RegGet16BitValue(0x3CE4);
	} while (nRegData != 0x9432);

	DBG("program 6\n");


	nCrcMain = nCrcMain ^ 0xffffffff;
	nCrcInfo = nCrcInfo ^ 0xffffffff;


	nCrcMainTp = RegGet16BitValue(0x3C80);
	nCrcMainTp = (nCrcMainTp << 16) | RegGet16BitValue(0x3C82);
	nCrcInfoTp = RegGet16BitValue(0x3CA0);
	nCrcInfoTp = (nCrcInfoTp << 16) | RegGet16BitValue(0x3CA2);

	DBG("nCrcMain=0x%x, nCrcInfo=0x%x, nCrcMainTp=0x%x, nCrcInfoTp=0x%x\n",
			   nCrcMain, nCrcInfo, nCrcMainTp, nCrcInfoTp);

	g_FwDataCount = 0;

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	DrvPlatformLyrTouchDeviceResetHw();
	mdelay(300);

	if ((nCrcMainTp != nCrcMain) || (nCrcInfoTp != nCrcInfo)) {
		DBG("Update FAILED\n");

		return -EPERM;
	}

	DBG("Update SUCCESS\n");

	return 0;
}

static void _DrvFwCtrlMsg28xxConvertFwDataTwoDimenToOneDimen(u8 szTwoDimenFwData[][1024], u8 *pOneDimenFwData)
{
	u32 i, j;

	DBG("*** %s() ***\n", __func__);

	for (i = 0; i < MSG28XX_FIRMWARE_WHOLE_SIZE; i++) {
		for (j = 0; j < 1024; j++) {
			pOneDimenFwData[i*1024+j] = szTwoDimenFwData[i][j];
		}
	}
}

static u32 _DrvFwCtrlMsg28xxCalculateCrc(u8 *pFwData, u32 nOffset, u32 nSize)
{
	u32 i;
	u32 nData = 0, nCrc = 0;
	u32 nCrcRule = 0x0C470C06;

	for (i = 0; i < nSize; i += 4) {
		nData = (pFwData[nOffset+i]) | (pFwData[nOffset+i+1] << 8) | (pFwData[nOffset+i+2] << 16) | (pFwData[nOffset+i+3] << 24);
		nCrc = (nCrc >> 1) ^ (nCrc << 1) ^ (nCrc & nCrcRule) ^ nData;
	}

	return nCrc;
}

static void _DrvFwCtrlMsg28xxAccessEFlashInit(void)
{

	RegSetLByteValue(0x3C60, 0x55);
	RegSetLByteValue(0x3C61, 0xAA);


	RegSetLByteValue(0x1606, 0x20);
	RegSetLByteValue(0x1608, 0x20);


	RegSet16BitValue(0x1618, 0xA55A);
}

static void _DrvFwCtrlMsg28xxIspBurstWriteEFlashStart(u16 nStartAddr, u8 *pFirstData, u32 nBlockSize, u16 nPageNum, EmemType_e eEmemType)
{
	u16 nWriteAddr = nStartAddr/4;
	u8  szDbBusTxData[3] = {0};

	DBG("*** %s() nStartAddr = 0x%x, nBlockSize = %d, nPageNum = %d, eEmemType = %d ***\n", __func__, nStartAddr, nBlockSize, nPageNum, eEmemType);


	RegSetLByteValue(0x1608, 0x20);
	RegSetLByteValue(0x1606, 0x20);


	RegSet16BitValue(0x1606, 0x0080);


	RegSetLByteValue(0x1640, 0x01);

	if (eEmemType == EMEM_INFO) {
		RegSetLByteValue(0x1607, 0x08);
	}


	RegSetLByteValue(0x1604, 0x01);


	RegSet16BitValue(0x161A, nPageNum);


	RegSetLByteValue(0x1606, 0x81);


	RegSetLByteValue(0x1602, pFirstData[0]);
	RegSetLByteValue(0x1602, pFirstData[1]);
	RegSetLByteValue(0x1602, pFirstData[2]);
	RegSetLByteValue(0x1602, pFirstData[3]);


	RegSet16BitValue(0x1600, nWriteAddr);


	RegSet16BitValue(0x1600, nWriteAddr);


	RegSetLByteValue(0x1608, 0x21);

	szDbBusTxData[0] = 0x10;
	szDbBusTxData[1] = 0x16;
	szDbBusTxData[2] = 0x02;
	IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3);

	szDbBusTxData[0] = 0x20;


	IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 1);
}

static void _DrvFwCtrlMsg28xxIspBurstWriteEFlashDoWrite(u8 *pBufferData, u32 nLength)
{
	u32 i;
	u8  szDbBusTxData[3+MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE] = {0};

	DBG("*** %s() nLength = %d ***\n", __func__, nLength);

	szDbBusTxData[0] = 0x10;
	szDbBusTxData[1] = 0x16;
	szDbBusTxData[2] = 0x02;

	for (i = 0; i < nLength; i++) {
		szDbBusTxData[3+i] = pBufferData[i];
	}
	IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3+nLength);
}

static void _DrvFwCtrlMsg28xxIspBurstWriteEFlashEnd(void)
{
	u8 szDbBusTxData[1] = {0};

	DBG("*** %s() ***\n", __func__);

	szDbBusTxData[0] = 0x21;
	IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 1);

	szDbBusTxData[0] = 0x7E;
	IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 1);


	RegSetLByteValue(0x1608, 0x20);
}

static void _DrvFwCtrlMsg28xxWriteEFlashStart(u16 nStartAddr, u8 *pFirstData, EmemType_e eEmemType)
{
	u16 nWriteAddr = nStartAddr/4;

	DBG("*** %s() nStartAddr = 0x%x, eEmemType = %d ***\n", __func__, nStartAddr, eEmemType);


	RegSetLByteValue(0x1608, 0x20);
	RegSetLByteValue(0x1606, 0x20);


	RegSet16BitValue(0x1606, 0x0040);


	RegSetLByteValue(0x1640, 0x01);

	if (eEmemType == EMEM_INFO) {
		RegSetLByteValue(0x1607, 0x08);
	}


	RegSetLByteValue(0x1604, 0x01);


	RegSetLByteValue(0x1606, 0x81);


	RegSetLByteValue(0x1602, pFirstData[0]);
	RegSetLByteValue(0x1602, pFirstData[1]);
	RegSetLByteValue(0x1602, pFirstData[2]);
	RegSetLByteValue(0x1602, pFirstData[3]);


	RegSet16BitValue(0x1600, nWriteAddr);


	RegSet16BitValue(0x1600, nWriteAddr);
}

static void _DrvFwCtrlMsg28xxWriteEFlashDoWrite(u16 nStartAddr, u8 *pBufferData)
{
	u16 nWriteAddr = nStartAddr/4;

	DBG("*** %s() nWriteAddr = %d ***\n", __func__, nWriteAddr);


	RegSetLByteValue(0x1602, pBufferData[0]);
	RegSetLByteValue(0x1602, pBufferData[1]);
	RegSetLByteValue(0x1602, pBufferData[2]);
	RegSetLByteValue(0x1602, pBufferData[3]);


	RegSet16BitValue(0x1600, nWriteAddr);
}

static void _DrvFwCtrlMsg28xxWriteEFlashEnd(void)
{
	DBG("*** %s() ***\n", __func__);


}

static void _DrvFwCtrlMsg28xxReadEFlashStart(u16 nStartAddr, EmemType_e eEmemType)
{
	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);


	RegSetLByteValue(0x1608, 0x20);
	RegSetLByteValue(0x1606, 0x20);

	RegSetLByteValue(0x1606, 0x02);

	RegSet16BitValue(0x1600, nStartAddr);

	if (eEmemType == EMEM_MAIN) {

		RegSetLByteValue(0x1607, 0x00);


		RegSetLByteValue(0x1604, 0x01);


		RegSet16BitValue(0x1606, 0x0001);
	} else if (eEmemType == EMEM_INFO) {

		RegSetLByteValue(0x1607, 0x08);


		RegSetLByteValue(0x1604, 0x01);


		RegSet16BitValue(0x1606, 0x0801);
	}
}

static void _DrvFwCtrlMsg28xxReadEFlashDoRead(u16 nReadAddr, u8 *pReadData)
{
	u16 nRegData1 = 0, nRegData2 = 0;

	DBG("*** %s() nReadAddr = 0x%x ***\n", __func__, nReadAddr);


	RegSet16BitValue(0x1600, nReadAddr);


	nRegData1 = RegGet16BitValue(0x160A);
	nRegData2 = RegGet16BitValue(0x160C);

	pReadData[0] = nRegData1 & 0xFF;
	pReadData[1] = (nRegData1 >> 8) & 0xFF;
	pReadData[2] = nRegData2 & 0xFF;
	pReadData[3] = (nRegData2 >> 8) & 0xFF;
}

static void _DrvFwCtrlMsg28xxReadEFlashEnd(void)
{
	DBG("*** %s() ***\n", __func__);


	RegSetLByteValue(0x1606, 0x02);


	RegSetLByteValue(0x1607, 0x00);


	RegSet16BitValue(0x1600, 0x0000);
}


static void _DrvFwCtrlMsg28xxGetSfrAddr3Value(void)
{
	DBG("*** %s() ***\n", __func__);


	RegSetLByteValue(0x1608, 0x20);
	RegSetLByteValue(0x1606, 0x20);


	RegSetLByteValue(0x1606, 0x01);
	RegSetLByteValue(0x1610, 0x01);
	RegSetLByteValue(0x1607, 0x20);


	RegSetLByteValue(0x1600, 0x03);
	RegSetLByteValue(0x1601, 0x00);

	_gSFR_ADDR3_BYTE0_1_VALUE = RegGet16BitValue(0x160A);
	_gSFR_ADDR3_BYTE2_3_VALUE = RegGet16BitValue(0x160C);

	DBG("_gSFR_ADDR3_BYTE0_1_VALUE = 0x%4X, _gSFR_ADDR3_BYTE2_3_VALUE = 0x%4X\n", _gSFR_ADDR3_BYTE0_1_VALUE, _gSFR_ADDR3_BYTE2_3_VALUE);
}

static void _DrvFwCtrlMsg28xxUnsetProtectBit(void)
{
	u8 nB0, nB1, nB2, nB3;

	DBG("*** %s() ***\n", __func__);

	_DrvFwCtrlMsg28xxGetSfrAddr3Value();

	nB0 = _gSFR_ADDR3_BYTE0_1_VALUE & 0xFF;
	nB1 = (_gSFR_ADDR3_BYTE0_1_VALUE & 0xFF00) >> 8;

	nB2 = _gSFR_ADDR3_BYTE2_3_VALUE & 0xFF;
	nB3 = (_gSFR_ADDR3_BYTE2_3_VALUE & 0xFF00) >> 8;

	DBG("nB0 = 0x%2X, nB1 = 0x%2X, nB2 = 0x%2X, nB3 = 0x%2X\n", nB0, nB1, nB2, nB3);

	nB2 = nB2 & 0xBF;
	nB3 = nB3 & 0xFC;

	DBG("nB0 = 0x%2X, nB1 = 0x%2X, nB2 = 0x%2X, nB3 = 0x%2X\n", nB0, nB1, nB2, nB3);


	RegSetLByteValue(0x1608, 0x20);
	RegSetLByteValue(0x1606, 0x20);
	RegSetLByteValue(0x1610, 0x80);
	RegSetLByteValue(0x1607, 0x10);


	RegSetLByteValue(0x1606, 0x01);


	RegSetLByteValue(0x1602, nB0);
	RegSetLByteValue(0x1602, nB1);
	RegSetLByteValue(0x1602, nB2);
	RegSetLByteValue(0x1602, nB3);


	RegSetLByteValue(0x1600, 0x03);
	RegSetLByteValue(0x1601, 0x00);


	RegSetLByteValue(0x1607, 0x00);
}

static void _DrvFwCtrlMsg28xxSetProtectBit(void)
{
	u8 nB0, nB1, nB2, nB3;

	DBG("*** %s() ***\n", __func__);

	nB0 = _gSFR_ADDR3_BYTE0_1_VALUE & 0xFF;
	nB1 = (_gSFR_ADDR3_BYTE0_1_VALUE & 0xFF00) >> 8;

	nB2 = _gSFR_ADDR3_BYTE2_3_VALUE & 0xFF;
	nB3 = (_gSFR_ADDR3_BYTE2_3_VALUE & 0xFF00) >> 8;

	DBG("nB0 = 0x%2X, nB1 = 0x%2X, nB2 = 0x%2X, nB3 = 0x%2X\n", nB0, nB1, nB2, nB3);


	RegSetLByteValue(0x1608, 0x20);
	RegSetLByteValue(0x1606, 0x20);
	RegSetLByteValue(0x1610, 0x80);
	RegSetLByteValue(0x1607, 0x10);


	RegSetLByteValue(0x1606, 0x01);


	RegSetLByteValue(0x1602, nB0);
	RegSetLByteValue(0x1602, nB1);
	RegSetLByteValue(0x1602, nB2);
	RegSetLByteValue(0x1602, nB3);


	RegSetLByteValue(0x1600, 0x03);
	RegSetLByteValue(0x1601, 0x00);
	RegSetLByteValue(0x1606, 0x02);
}

static void _DrvFwCtrlMsg28xxEraseEmem(EmemType_e eEmemType)
{
	u32 nInfoAddr = 0x20;
	u32 nTimeOut = 0;
	u8 nRegData = 0;

	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

	DrvPlatformLyrTouchDeviceResetHw();

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	DBG("Erase start\n");

	_DrvFwCtrlMsg28xxAccessEFlashInit();


	RegSetLByteValue(0x0FE6, 0x01);


	RegSet16BitValue(0x1618, 0x5AA5);

	_DrvFwCtrlMsg28xxUnsetProtectBit();

	if (eEmemType == EMEM_MAIN) {
		DBG("Erase main block\n");


		RegSetLByteValue(0x1607, 0x00);


		RegSetLByteValue(0x1606, 0xC0);


		RegSetLByteValue(0x1607, 0x03);


		RegSetLByteValue(0x1606, 0xC1);

		nTimeOut = 0;
		while (1) {
			nRegData = RegGetLByteValue(0x160E);
			nRegData = (nRegData & BIT3);

			DBG("Wait erase done flag nRegData = 0x%x\n", nRegData);

			if (nRegData == BIT3)
				break;

			mdelay(10);

			if ((nTimeOut++) > 10) {
				DBG("Erase main block failed. Timeout.\n");

				goto EraseEnd;
			}
		}
	} else if (eEmemType == EMEM_INFO) {
		DBG("Erase info block\n");


		RegSetLByteValue(0x1607, 0x08);


		RegSetLByteValue(0x1604, 0x01);


		RegSetLByteValue(0x1606, 0xC0);


		RegSetLByteValue(0x1607, 0x09);

		for (nInfoAddr = 0x20; nInfoAddr <= MSG28XX_EMEM_INFO_MAX_ADDR; nInfoAddr += 0x20) {
			DBG("nInfoAddr = 0x%x\n", nInfoAddr);


			RegSet16BitValue(0x1600, nInfoAddr);


			RegSetLByteValue(0x1606, 0xC1);

			nTimeOut = 0;
			while (1) {
				nRegData = RegGetLByteValue(0x160E);
				nRegData = (nRegData & BIT3);

				DBG("Wait erase done flag nRegData = 0x%x\n", nRegData);

				if (nRegData == BIT3) {
					break;
				}

				mdelay(10);

				if ((nTimeOut++) > 10) {
					DBG("Erase info block failed. Timeout.\n");


					RegSetLByteValue(0x1607, 0x00);

					goto EraseEnd;
				}
			}
		}


		RegSetLByteValue(0x1607, 0x00);
	}

	EraseEnd:

	_DrvFwCtrlMsg28xxSetProtectBit();

	RegSetLByteValue(0x1606, 0x00);
	RegSetLByteValue(0x1607, 0x00);


	RegSet16BitValue(0x1618, 0xA55A);

	DBG("Erase end\n");

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();
}

static void _DrvFwCtrlMsg28xxProgramEmem(EmemType_e eEmemType)
{
	u32 i, j;
	u32 nPageNum = 0, nLength = 0, nIndex = 0, nWordNum = 0;
	u32 nRetryTime = 0;
	u8  nRegData = 0;
	u8  szFirstData[MSG28XX_EMEM_SIZE_BYTES_ONE_WORD] = {0};
	u8  szBufferData[MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE] = {0};

	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
	DmaReset();
#endif
#endif

	DrvPlatformLyrTouchDeviceResetHw();

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	DBG("Program start\n");

	_DrvFwCtrlMsg28xxAccessEFlashInit();


	RegSetLByteValue(0x0FE6, 0x01);


	RegSet16BitValue(0x1618, 0x5AA5);

	_DrvFwCtrlMsg28xxUnsetProtectBit();

	if (eEmemType == EMEM_MAIN) {
		DBG("Program main block\n");

		nPageNum = (MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE * 1024) / MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE;
		nIndex = 0;

		for (i = 0; i < nPageNum; i++) {
			if (i == 0) {

				nLength = MSG28XX_EMEM_SIZE_BYTES_ONE_WORD;

				szFirstData[0] = g_FwData[0][0];
				szFirstData[1] = g_FwData[0][1];
				szFirstData[2] = g_FwData[0][2];
				szFirstData[3] = g_FwData[0][3];

				_DrvFwCtrlMsg28xxIspBurstWriteEFlashStart(nIndex, &szFirstData[0], MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE*1024, nPageNum, EMEM_MAIN);

				nIndex += nLength;

				nLength = MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE - MSG28XX_EMEM_SIZE_BYTES_ONE_WORD;

				for (j = 0; j < nLength; j++) {
					szBufferData[j] = g_FwData[0][4+j];
				}
			} else {
				nLength = MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE;

				for (j = 0; j < nLength; j++) {
					szBufferData[j] = g_FwData[i/8][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE*(i-(8*(i/8)))+j];
				}
			}

			_DrvFwCtrlMsg28xxIspBurstWriteEFlashDoWrite(&szBufferData[0], nLength);

			udelay(2000);

			nIndex += nLength;
		}

		_DrvFwCtrlMsg28xxIspBurstWriteEFlashEnd();


		RegSet16BitValueOn(0x1606, BIT2);


		nRegData = RegGetLByteValue(0x160E);
		nRetryTime = 0;

		while ((nRegData & BIT3) != BIT3) {
			mdelay(10);

			nRegData = RegGetLByteValue(0x160E);

			if (nRetryTime++ > 100) {
				DBG("main block can't wait write to done.\n");

				goto ProgramEnd;
			}
		}
	} else if (eEmemType == EMEM_INFO) {
		DBG("Program info block\n");

		nPageNum = (MSG28XX_FIRMWARE_INFO_BLOCK_SIZE * 1024) / MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE;
		nIndex = 0;
		nIndex += MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE;


		for (i = 1; i < (nPageNum - 1); i++) {
			if (i == 1) {

				nLength = MSG28XX_EMEM_SIZE_BYTES_ONE_WORD;

				szFirstData[0] = g_FwData[MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE];
				szFirstData[1] = g_FwData[MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE+1];
				szFirstData[2] = g_FwData[MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE+2];
				szFirstData[3] = g_FwData[MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE+3];

				_DrvFwCtrlMsg28xxIspBurstWriteEFlashStart(nIndex, &szFirstData[0], MSG28XX_FIRMWARE_INFO_BLOCK_SIZE*1024, nPageNum-1, EMEM_INFO);

				nIndex += nLength;

				nLength = MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE - MSG28XX_EMEM_SIZE_BYTES_ONE_WORD;

				for (j = 0; j < nLength; j++) {
					szBufferData[j] = g_FwData[MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE+4+j];
				}
			} else {
				nLength = MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE;

				if (i < 8) {
					for (j = 0; j < nLength; j++) {
						szBufferData[j] = g_FwData[MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE*i+j];
					}
				} else {
					for (j = 0; j < nLength; j++) {
						szBufferData[j] = g_FwData[MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE+1][MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE*(i-8)+j];
					}
				}
			}

			_DrvFwCtrlMsg28xxIspBurstWriteEFlashDoWrite(&szBufferData[0], nLength);

			udelay(2000);

			nIndex += nLength;
		}

		_DrvFwCtrlMsg28xxIspBurstWriteEFlashEnd();


		RegSet16BitValueOn(0x1606, BIT2);


		nRegData = RegGetLByteValue(0x160E);
		nRetryTime = 0;

		while ((nRegData & BIT3) != BIT3) {
			mdelay(10);

			nRegData = RegGetLByteValue(0x160E);

			if (nRetryTime++ > 100) {
				DBG("Info block page 1~14 can't wait write to done.\n");

				goto ProgramEnd;
			}
		}

		RegSet16BitValueOff(0x1EBE, BIT15);


		nIndex = 15 * MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE;
		nWordNum = MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE / MSG28XX_EMEM_SIZE_BYTES_ONE_WORD;
		nLength = MSG28XX_EMEM_SIZE_BYTES_ONE_WORD;

		for (i = 0; i < nWordNum; i++) {
			if (i == 0) {
				szFirstData[0] = g_FwData[MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE+1][7*MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE];
				szFirstData[1] = g_FwData[MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE+1][7*MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE+1];
				szFirstData[2] = g_FwData[MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE+1][7*MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE+2];
				szFirstData[3] = g_FwData[MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE+1][7*MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE+3];

				_DrvFwCtrlMsg28xxWriteEFlashStart(nIndex, &szFirstData[0], EMEM_INFO);
			} else {
				for (j = 0; j < nLength; j++) {
					szFirstData[j] = g_FwData[MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE+1][7*MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE+(4*i)+j];
				}

				_DrvFwCtrlMsg28xxWriteEFlashDoWrite(nIndex, &szFirstData[0]);
			}

			udelay(2000);

			nIndex += nLength;
		}

		_DrvFwCtrlMsg28xxWriteEFlashEnd();


		RegSet16BitValueOn(0x1606, BIT2);


		nRegData = RegGetLByteValue(0x160E);
		nRetryTime = 0;

		while ((nRegData & BIT3) != BIT3) {
			mdelay(10);

			nRegData = RegGetLByteValue(0x160E);

			if (nRetryTime++ > 100) {
				DBG("Info block page 15 can't wait write to done.\n");

				goto ProgramEnd;
			}
		}
	}

	ProgramEnd:

	_DrvFwCtrlMsg28xxSetProtectBit();


	RegSet16BitValue(0x1618, 0xA55A);

	DBG("Program end\n");

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();
}

static u16 _DrvFwCtrlMsg28xxGetSwId(EmemType_e eEmemType)
{
	u16 nRetVal = 0;
	u16 nReadAddr = 0;
	u8  szTmpData[4] = {0};

	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	RegSetLByteValue(0x0FE6, 0x01);
	RegSet16BitValue(0x3C60, 0xAA55);

	if (eEmemType == EMEM_MAIN) {
		_DrvFwCtrlMsg28xxReadEFlashStart(0x7FFD, EMEM_MAIN);
		nReadAddr = 0x7FFD;
	} else if (eEmemType == EMEM_INFO) {
		_DrvFwCtrlMsg28xxReadEFlashStart(0x81FB, EMEM_INFO);
		nReadAddr = 0x81FB;
	}

	_DrvFwCtrlMsg28xxReadEFlashDoRead(nReadAddr, &szTmpData[0]);

	_DrvFwCtrlMsg28xxReadEFlashEnd();

	/*
	  Ex. SW ID in Main Block :
		  Major low byte at address 0x7FFD

		  SW ID in Info Block :
		  Major low byte at address 0x81FB
	*/

	nRetVal = (szTmpData[1] << 8);
	nRetVal |= szTmpData[0];

	DBG("SW ID = 0x%x\n", nRetVal);

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	return nRetVal;
}

static u32 _DrvFwCtrlMsg28xxGetFirmwareCrcByHardware(EmemType_e eEmemType)
{
	u32 nRetVal = 0;
	u32 nRetryTime = 0;
	u32 nCrcEndAddr = 0;
	u16 nCrcDown = 0;

	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

	DrvPlatformLyrTouchDeviceResetHw();

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	_DrvFwCtrlMsg28xxAccessEFlashInit();

	if (eEmemType == EMEM_MAIN) {

		RegSetLByteValue(0x1608, 0x20);
		RegSetLByteValue(0x1606, 0x20);

		RegSet16BitValue(0x1610, 0x0001);
		RegSet16BitValue(0x1606, 0x0000);
		RegSet16BitValue(0x1620, 0x0002);
		RegSet16BitValue(0x1620, 0x0000);
		RegSet16BitValue(0x1600, 0x0000);

		nCrcEndAddr = (MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE*1024)/4-2;
		RegSet16BitValue(0x1622, nCrcEndAddr);
		RegSet16BitValue(0x1620, 0x0001);
		nCrcDown = RegGet16BitValue(0x1620);

		nRetryTime = 0;
		while ((nCrcDown >> 15) == 0) {
			mdelay(10);

			nCrcDown = RegGet16BitValue(0x1620);
			nRetryTime++;

			if (nRetryTime > 30) {
				DBG("Wait main block nCrcDown failed.\n");
				break;
			}
		}

		nRetVal = RegGet16BitValue(0x1626);
		nRetVal = (nRetVal << 16) | RegGet16BitValue(0x1624);
	} else if (eEmemType == EMEM_INFO) {

		RegSetLByteValue(0x1608, 0x20);
		RegSetLByteValue(0x1606, 0x20);

		RegSet16BitValue(0x1610, 0x0001);
		RegSet16BitValue(0x1606, 0x0800);
		RegSetLByteValue(0x1604, 0x01);
		RegSet16BitValue(0x1620, 0x0002);
		RegSet16BitValue(0x1620, 0x0000);
		RegSet16BitValue(0x1600, 0x0020);
		RegSet16BitValue(0x1622, 0x01FE);
		RegSet16BitValue(0x1620, 0x0001);

		nCrcDown = RegGet16BitValue(0x1620);

		nRetryTime = 0;
		while ((nCrcDown >> 15) == 0) {
			mdelay(10);

			nCrcDown = RegGet16BitValue(0x1620);
			nRetryTime++;

			if (nRetryTime > 30) {
				DBG("Wait info block nCrcDown failed.\n");
				break;
			}
		}

		nRetVal = RegGet16BitValue(0x1626);
		nRetVal = (nRetVal << 16) | RegGet16BitValue(0x1624);
	}

	DBG("Hardware CRC = 0x%x\n", nRetVal);

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	return nRetVal;
}

static s32 _DrvFwCtrlMsg28xxUpdateFirmware(u8 szFwData[][1024], EmemType_e eEmemType)
{
	u32 nCrcMain = 0, nCrcMainTp = 0;
	u32 nCrcInfo = 0, nCrcInfoTp = 0;

	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

	g_IsUpdateFirmware = 0x01;


	if (eEmemType == EMEM_ALL) {
		_DrvFwCtrlMsg28xxEraseEmem(EMEM_MAIN);
		_DrvFwCtrlMsg28xxEraseEmem(EMEM_INFO);
	} else if (eEmemType == EMEM_MAIN) {
		_DrvFwCtrlMsg28xxEraseEmem(EMEM_MAIN);
	} else if (eEmemType == EMEM_INFO) {
		_DrvFwCtrlMsg28xxEraseEmem(EMEM_INFO);
	}

	DBG("erase OK\n");


	if (eEmemType == EMEM_ALL) {
		_DrvFwCtrlMsg28xxProgramEmem(EMEM_MAIN);
		_DrvFwCtrlMsg28xxProgramEmem(EMEM_INFO);
	} else if (eEmemType == EMEM_MAIN) {
		_DrvFwCtrlMsg28xxProgramEmem(EMEM_MAIN);
	} else if (eEmemType == EMEM_INFO) {
		_DrvFwCtrlMsg28xxProgramEmem(EMEM_INFO);
	}

	DBG("program OK\n");

	/* Calculate main block & info block CRC by device driver itself */
	_DrvFwCtrlMsg28xxConvertFwDataTwoDimenToOneDimen(szFwData, _gOneDimenFwData);

	/* Read main block CRC from TP */
	if (eEmemType == EMEM_ALL) {
		nCrcMain = _DrvFwCtrlMsg28xxCalculateCrc(_gOneDimenFwData, 0, MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE*1024-MSG28XX_EMEM_SIZE_BYTES_ONE_WORD);
		nCrcInfo = _DrvFwCtrlMsg28xxCalculateCrc(_gOneDimenFwData, MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE*1024+MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE, MSG28XX_FIRMWARE_INFO_BLOCK_SIZE*1024-MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE-MSG28XX_EMEM_SIZE_BYTES_ONE_WORD);

		nCrcMainTp = _DrvFwCtrlMsg28xxGetFirmwareCrcByHardware(EMEM_MAIN);
		nCrcInfoTp = _DrvFwCtrlMsg28xxGetFirmwareCrcByHardware(EMEM_INFO);
	} else if (eEmemType == EMEM_MAIN) {
		nCrcMain = _DrvFwCtrlMsg28xxCalculateCrc(_gOneDimenFwData, 0, MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE*1024-MSG28XX_EMEM_SIZE_BYTES_ONE_WORD);

		nCrcMainTp = _DrvFwCtrlMsg28xxGetFirmwareCrcByHardware(EMEM_MAIN);
	} else if (eEmemType == EMEM_INFO) {
		nCrcInfo = _DrvFwCtrlMsg28xxCalculateCrc(_gOneDimenFwData, MSG28XX_FIRMWARE_MAIN_BLOCK_SIZE*1024+MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE, MSG28XX_FIRMWARE_INFO_BLOCK_SIZE*1024-MSG28XX_EMEM_SIZE_BYTES_PER_ONE_PAGE-MSG28XX_EMEM_SIZE_BYTES_ONE_WORD);

		nCrcInfoTp = _DrvFwCtrlMsg28xxGetFirmwareCrcByHardware(EMEM_INFO);
	}

	DBG("nCrcMain=0x%x, nCrcInfo=0x%x, nCrcMainTp=0x%x, nCrcInfoTp=0x%x\n",
			   nCrcMain, nCrcInfo, nCrcMainTp, nCrcInfoTp);

	g_FwDataCount = 0;

	g_IsUpdateFirmware = 0x00;

	DrvPlatformLyrTouchDeviceResetHw();
	mdelay(300);

	if (eEmemType == EMEM_ALL) {
		if ((nCrcMainTp != nCrcMain) || (nCrcInfoTp != nCrcInfo)) {
			DBG("Update FAILED\n");

			return -EPERM;
		}
	} else if (eEmemType == EMEM_MAIN) {
		if (nCrcMainTp != nCrcMain) {
			DBG("Update FAILED\n");

			return -EPERM;
		}
	} else if (eEmemType == EMEM_INFO) {
		if (nCrcInfoTp != nCrcInfo) {
			DBG("Update FAILED\n");

			return -EPERM;
		}
	}

	DBG("Update SUCCESS\n");

	return 0;
}

static s32 _DrvFwCtrlUpdateFirmwareCash(u8 szFwData[][1024], EmemType_e eEmemType)
{
	DBG("*** %s() ***\n", __func__);

	DBG("chip type = 0x%x\n", g_ChipType);

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		return _DrvFwCtrlMsg26xxmUpdateFirmware(szFwData, eEmemType);
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		DBG("IS_FORCE_TO_UPDATE_FIRMWARE_ENABLED = %d\n", IS_FORCE_TO_UPDATE_FIRMWARE_ENABLED);

		if (IS_FORCE_TO_UPDATE_FIRMWARE_ENABLED) {
			return _DrvFwCtrlMsg28xxUpdateFirmware(szFwData, EMEM_MAIN);
		} else {
			u16 eSwId = 0x0000;
			u16 eVendorId = 0x0000;

			eVendorId = szFwData[129][1005] << 8 | szFwData[129][1004];
			eSwId = _DrvFwCtrlMsg28xxGetSwId(EMEM_INFO);

			DBG("eVendorId = 0x%x, eSwId = 0x%x\n", eVendorId, eSwId);


			if (eSwId != eVendorId) {
				DrvPlatformLyrTouchDeviceResetHw();

				DBG("The vendor id of the update firmware bin file is different from the vendor id on e-flash. Not allow to update.\n");

				return -EPERM;
			} else {
				return _DrvFwCtrlMsg28xxUpdateFirmware(szFwData, EMEM_MAIN);
			}
		}
	} else {
		DBG("Undefined chip type.\n");
		g_FwDataCount = 0;

		return -EPERM;
	}
}

static s32 _DrvFwCtrlUpdateFirmwareBySdCard(const char *pFilePath)
{
	s32 nRetVal = 0;
	struct file *pfile = NULL;
	struct inode *inode;
	s32 fsize = 0;
	u8 *pbt_buf = NULL;
	mm_segment_t old_fs;
	loff_t pos;
	u16 eSwId = 0x0000;
	u16 eVendorId = 0x0000;

	DBG("*** %s() ***\n", __func__);

	pfile = filp_open(pFilePath, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		DBG("Error occurred while opening file %s.\n", pFilePath);
		return -EPERM;
	}

	inode = pfile->f_dentry->d_inode;
	fsize = inode->i_size;

	DBG("fsize = %d\n", fsize);

	if (fsize <= 0) {
		filp_close(pfile, NULL);
		return -EPERM;
	}


	pbt_buf = kmalloc(fsize, GFP_KERNEL);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	pos = 0;
	vfs_read(pfile, pbt_buf, fsize, &pos);

	filp_close(pfile, NULL);
	set_fs(old_fs);

	_DrvFwCtrlStoreFirmwareData(pbt_buf, fsize);

	kfree(pbt_buf);

	DrvPlatformLyrDisableFingerTouchReport();

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		eVendorId = g_FwData[0][0x2B] << 8 | g_FwData[0][0x2A];
		eSwId = _DrvFwCtrlMsg26xxmGetSwId(EMEM_MAIN);
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		eVendorId = g_FwData[129][1005] << 8 | g_FwData[129][1004];
		eSwId = _DrvFwCtrlMsg28xxGetSwId(EMEM_INFO);
	}

	DBG("eVendorId = 0x%x, eSwId = 0x%x\n", eVendorId, eSwId);
	DBG("IS_FORCE_TO_UPDATE_FIRMWARE_ENABLED = %d\n", IS_FORCE_TO_UPDATE_FIRMWARE_ENABLED);

	if ((eSwId == eVendorId) || (IS_FORCE_TO_UPDATE_FIRMWARE_ENABLED)) {
		if (g_ChipType == CHIP_TYPE_MSG26XXM && fsize == 40960/* 40KB */) {
			  nRetVal = _DrvFwCtrlUpdateFirmwareCash(g_FwData, EMEM_ALL);
		} else if (g_ChipType == CHIP_TYPE_MSG28XX && fsize == 133120/* 130KB */) {
			  nRetVal = _DrvFwCtrlUpdateFirmwareCash(g_FwData, EMEM_MAIN);
		} else {
			DBG("The file size of the update firmware bin file is not supported, fsize = %d\n", fsize);
			nRetVal = -1;
		}
	} else {
		DBG("The vendor id of the update firmware bin file is different from the vendor id on e-flash. Not allow to update.\n");
		nRetVal = -1;
	}

	g_FwDataCount = 0;

	DrvPlatformLyrEnableFingerTouchReport();

	return nRetVal;
}



#ifdef CONFIG_ENABLE_GESTURE_WAKEUP

void DrvFwCtrlOpenGestureWakeup(u32 *pMode)
{
	u8 szDbBusTxData[4] = {0};
	u32 i = 0;
	s32 rc;

	DBG("*** %s() ***\n", __func__);

	DBG("wakeup mode 0 = 0x%x\n", pMode[0]);
	DBG("wakeup mode 1 = 0x%x\n", pMode[1]);

#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
	szDbBusTxData[0] = 0x59;
	szDbBusTxData[1] = 0x00;
	szDbBusTxData[2] = ((pMode[1] & 0xFF000000) >> 24);
	szDbBusTxData[3] = ((pMode[1] & 0x00FF0000) >> 16);

	while (i < 5) {
		mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
		rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 4);
		if (rc > 0) {
			DBG("Enable gesture wakeup index 0 success\n");
			break;
		}

		i++;
	}
	if (i == 5) {
		DBG("Enable gesture wakeup index 0 failed\n");
	}

	szDbBusTxData[0] = 0x59;
	szDbBusTxData[1] = 0x01;
	szDbBusTxData[2] = ((pMode[1] & 0x0000FF00) >> 8);
	szDbBusTxData[3] = ((pMode[1] & 0x000000FF) >> 0);

	while (i < 5) {
		mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
		rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 4);
		if (rc > 0) {
			DBG("Enable gesture wakeup index 1 success\n");
			break;
		}

		i++;
	}
	if (i == 5) {
		DBG("Enable gesture wakeup index 1 failed\n");
	}

	szDbBusTxData[0] = 0x59;
	szDbBusTxData[1] = 0x02;
	szDbBusTxData[2] = ((pMode[0] & 0xFF000000) >> 24);
	szDbBusTxData[3] = ((pMode[0] & 0x00FF0000) >> 16);

	while (i < 5) {
		mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
		rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 4);
		if (rc > 0) {
			DBG("Enable gesture wakeup index 2 success\n");
			break;
		}

		i++;
	}
	if (i == 5) {
		DBG("Enable gesture wakeup index 2 failed\n");
	}

	szDbBusTxData[0] = 0x59;
	szDbBusTxData[1] = 0x03;
	szDbBusTxData[2] = ((pMode[0] & 0x0000FF00) >> 8);
	szDbBusTxData[3] = ((pMode[0] & 0x000000FF) >> 0);

	while (i < 5) {
		mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
		rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 4);
		if (rc > 0) {
			DBG("Enable gesture wakeup index 3 success\n");
			break;
		}

		i++;
	}
	if (i == 5) {
		DBG("Enable gesture wakeup index 3 failed\n");
	}

	g_GestureWakeupFlag = 1;

#else

	szDbBusTxData[0] = 0x58;
	szDbBusTxData[1] = ((pMode[0] & 0x0000FF00) >> 8);
	szDbBusTxData[2] = ((pMode[0] & 0x000000FF) >> 0);

	while (i < 5) {
		mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
		rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 3);
		if (rc > 0) {
			DBG("Enable gesture wakeup success\n");
			break;
		}

		i++;
	}
	if (i == 5) {
		DBG("Enable gesture wakeup failed\n");
	}

	g_GestureWakeupFlag = 1;
#endif
}

void DrvFwCtrlCloseGestureWakeup(void)
{
	DBG("*** %s() ***\n", __func__);

	g_GestureWakeupFlag = 0;
}

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
void DrvFwCtrlOpenGestureDebugMode(u8 nGestureFlag)
{
	u8 szDbBusTxData[3] = {0};
	s32 rc;

	DBG("*** %s() ***\n", __func__);

	DBG("Gesture Flag = 0x%x\n", nGestureFlag);

	szDbBusTxData[0] = 0x30;
	szDbBusTxData[1] = 0x01;
	szDbBusTxData[2] = nGestureFlag;

	mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
	rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 3);
	if (rc < 0) {
		DBG("Enable gesture debug mode failed\n");
	} else {
		g_GestureDebugMode = 1;

		DBG("Enable gesture debug mode success\n");
	}
}

void DrvFwCtrlCloseGestureDebugMode(void)
{
	u8 szDbBusTxData[3] = {0};
	s32 rc;

	DBG("*** %s() ***\n", __func__);

	szDbBusTxData[0] = 0x30;
	szDbBusTxData[1] = 0x00;
	szDbBusTxData[2] = 0x00;

	mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
	rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 3);
	if (rc < 0) {
		DBG("Disable gesture debug mode failed\n");
	} else {
		g_GestureDebugMode = 0;

		DBG("Disable gesture debug mode success\n");
	}
}
#endif

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
static void _DrvFwCtrlCoordinate(u8 *pRawData, u32 *pTranX, u32 *pTranY)
{
	u32 nX;
	u32 nY;
#ifdef CONFIG_SWAP_X_Y
	u32 nTempX;
	u32 nTempY;
#endif

	nX = (((pRawData[0] & 0xF0) << 4) | pRawData[1]);
	nY = (((pRawData[0] & 0x0F) << 8) | pRawData[2]);

	DBG("[x, y]=[%d, %d]\n", nX, nY);

#ifdef CONFIG_SWAP_X_Y
	nTempY = nX;
	nTempX = nY;
	nX = nTempX;
	nY = nTempY;
#endif

#ifdef CONFIG_REVERSE_X
	nX = 2047 - nX;
#endif

#ifdef CONFIG_REVERSE_Y
	nY = 2047 - nY;
#endif

	/*
	* pRawData[0]~pRawData[2] : the point abs,
	* pRawData[0]~pRawData[2] all are 0xFF, release touch
	*/
	if ((pRawData[0] == 0xFF) && (pRawData[1] == 0xFF) && (pRawData[2] == 0xFF)) {
		*pTranX = 0;
		*pTranY = 0;
	} else {
	/* one touch point */
		*pTranX = (nX * TOUCH_SCREEN_X_MAX) / TPD_WIDTH;
		*pTranY = (nY * TOUCH_SCREEN_Y_MAX) / TPD_HEIGHT;
		DBG("[%s]: [x, y]=[%d, %d]\n", __func__, nX, nY);
		DBG("[%s]: point[x, y]=[%d, %d]\n", __func__, *pTranX, *pTranY);
	}
}
#endif

#endif

static void _DrvFwCtrlReadReadDQMemStart(void)
{
	u8 nParCmdSelUseCfg = 0x7F;
	u8 nParCmdAdByteEn0 = 0x50;
	u8 nParCmdAdByteEn1 = 0x51;
	u8 nParCmdDaByteEn0 = 0x54;
	u8 nParCmdUSetSelB0 = 0x80;
	u8 nParCmdUSetSelB1 = 0x82;
	u8 nParCmdSetSelB2  = 0x85;
	u8 nParCmdIicUse	= 0x35;


	DBG("*** %s() ***\n", __func__);

	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdSelUseCfg, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdAdByteEn0, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdAdByteEn1, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdDaByteEn0, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdUSetSelB0, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdUSetSelB1, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdSetSelB2, 1);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdIicUse, 1);
}

static void _DrvFwCtrlReadReadDQMemEnd(void)
{
	u8 nParCmdNSelUseCfg = 0x7E;

	IicWriteData(SLAVE_I2C_ID_DBBUS, &nParCmdNSelUseCfg, 1);
}

u32 DrvFwCtrlReadDQMemValue(u16 nAddr)
{
	u8 tx_data[3] = {0x10, (nAddr >> 8) & 0xFF, nAddr & 0xFF};
	u8 rx_data[4] = {0};

	DBG("*** %s() ***\n", __func__);

	DBG("DQMem Addr = 0x%x\n", nAddr);

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	RegSetLByteValue(0x0FE6, 0x01);
	mdelay(100);

	_DrvFwCtrlReadReadDQMemStart();

	IicWriteData(SLAVE_I2C_ID_DBBUS, &tx_data[0], 3);
	IicReadData(SLAVE_I2C_ID_DBBUS, &rx_data[0], 4);

	_DrvFwCtrlReadReadDQMemEnd();


	RegSetLByteValue(0x0FE6, 0x00);
	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	return rx_data[3] << 24 | rx_data[2] << 16 | rx_data[1] << 8 | rx_data[0];
}

void DrvFwCtrlWriteDQMemValue(u16 nAddr, u32 nData)
{
	u8 szDbBusTxData[7] = {0};

	DBG("*** %s() ***\n", __func__);

	DBG("DQMem Addr = 0x%x\n", nAddr);

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	RegSetLByteValue(0x0FE6, 0x01);
	mdelay(100);

	_DrvFwCtrlReadReadDQMemStart();
	szDbBusTxData[0] = 0x10;
	szDbBusTxData[1] = ((nAddr >> 8) & 0xff);
	szDbBusTxData[2] = (nAddr & 0xff);
	szDbBusTxData[3] = nData & 0x000000FF;
	szDbBusTxData[4] = ((nData & 0x0000FF00) >> 8);
	szDbBusTxData[5] = ((nData & 0x00FF0000) >> 16);
	szDbBusTxData[6] = ((nData & 0xFF000000) >> 24);
	IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 7);

	_DrvFwCtrlReadReadDQMemEnd();


	RegSetLByteValue(0x0FE6, 0x00);
	mdelay(100);

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();
}



#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID



static u32 _DrvFwCtrlMsg26xxmCalculateFirmwareCrcFromEFlash(EmemType_e eEmemType, u8 nIsNeedResetHW)
{
	u32 nRetVal = 0;
	u16 nRegData = 0;

	DBG("*** %s() eEmemType = %d, nIsNeedResetHW = %d ***\n", __func__, eEmemType, nIsNeedResetHW);

	if (1 == nIsNeedResetHW) {
		DrvPlatformLyrTouchDeviceResetHw();
	}

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	RegSetLByteValue(0x0FE6, 0x01);
	RegSet16BitValue(0x3C60, 0xAA55);
	RegSet16BitValue(0x3CE4, 0xDF4C);
	RegSet16BitValue(0x1E04, 0x7d60);
	RegSet16BitValue(0x1E04, 0x829F);
	RegSetLByteValue(0x0FE6, 0x00);

	mdelay(100);


	do {
		nRegData = RegGet16BitValue(0x3CE4);
	} while (nRegData != 0x9432);

	if (eEmemType == EMEM_MAIN) {
		nRetVal = RegGet16BitValue(0x3C80);
		nRetVal = (nRetVal << 16) | RegGet16BitValue(0x3C82);

		DBG("Main Block CRC = 0x%x\n", nRetVal);
	} else if (eEmemType == EMEM_INFO) {
		nRetVal = RegGet16BitValue(0x3CA0);
		nRetVal = (nRetVal << 16) | RegGet16BitValue(0x3CA2);

		DBG("Info Block CRC = 0x%x\n", nRetVal);
	}

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	return nRetVal;
}

static u32 _DrvFwCtrlMsg26xxmRetrieveFirmwareCrcFromMainBlock(EmemType_e eEmemType)
{
	u32 nRetVal = 0;
	u16 nRegData = 0;
	u8 szDbBusTxData[5] = {0};
	u8 szDbBusRxData[4] = {0};

	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

	DrvPlatformLyrTouchDeviceResetHw();

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	RegSetLByteValue(0x0FE6, 0x01);
	RegSet16BitValue(0x3C60, 0xAA55);
	RegSet16BitValue(0x3CE4, 0xA4AB);
	RegSet16BitValue(0x1E04, 0x7d60);
	RegSet16BitValue(0x1E04, 0x829F);
	RegSetLByteValue(0x0FE6, 0x00);

	mdelay(100);


	do {
		nRegData = RegGet16BitValue(0x3CE4);
	} while (nRegData != 0x5B58);

	szDbBusTxData[0] = 0x72;
	if (eEmemType == EMEM_MAIN) {
		szDbBusTxData[1] = 0x7F;
		szDbBusTxData[2] = 0xF8;
	} else if (eEmemType == EMEM_INFO) {
		szDbBusTxData[1] = 0x7F;
		szDbBusTxData[2] = 0xFC;
	}
	szDbBusTxData[3] = 0x00;
	szDbBusTxData[4] = 0x04;

	IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 5);
	IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 4);

	nRetVal = szDbBusRxData[3];
	nRetVal = (nRetVal << 8) | szDbBusRxData[2];
	nRetVal = (nRetVal << 8) | szDbBusRxData[1];
	nRetVal = (nRetVal << 8) | szDbBusRxData[0];

	DBG("CRC = 0x%x\n", nRetVal);

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	return nRetVal;
}

static u32 _DrvFwCtrlMsg26xxmRetrieveInfoCrcFromInfoBlock(void)
{
	u32 nRetVal = 0;
	u16 nRegData = 0;
	u8 szDbBusTxData[5] = {0};
	u8 szDbBusRxData[4] = {0};

	DBG("*** %s() ***\n", __func__);

	DrvPlatformLyrTouchDeviceResetHw();

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	RegSetLByteValue(0x0FE6, 0x01);
	RegSet16BitValue(0x3C60, 0xAA55);
	RegSet16BitValue(0x3CE4, 0xA4AB);
	RegSet16BitValue(0x1E04, 0x7d60);
	RegSet16BitValue(0x1E04, 0x829F);
	RegSetLByteValue(0x0FE6, 0x00);

	mdelay(100);

	do {
		nRegData = RegGet16BitValue(0x3CE4);
	} while (nRegData != 0x5B58);

	szDbBusTxData[0] = 0x72;
	szDbBusTxData[1] = 0x80;
	szDbBusTxData[2] = 0x00;
	szDbBusTxData[3] = 0x00;
	szDbBusTxData[4] = 0x04;

	IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 5);
	IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 4);

	nRetVal = szDbBusRxData[3];
	nRetVal = (nRetVal << 8) | szDbBusRxData[2];
	nRetVal = (nRetVal << 8) | szDbBusRxData[1];
	nRetVal = (nRetVal << 8) | szDbBusRxData[0];

	DBG("CRC = 0x%x\n", nRetVal);

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	return nRetVal;
}

static u32 _DrvFwCtrlMsg26xxmRetrieveFrimwareCrcFromBinFile(u8 szTmpBuf[][1024], EmemType_e eEmemType)
{
	u32 nRetVal = 0;

	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

	if (szTmpBuf != NULL) {
		if (eEmemType == EMEM_MAIN) {
			nRetVal = szTmpBuf[31][1019];
			nRetVal = (nRetVal << 8) | szTmpBuf[31][1018];
			nRetVal = (nRetVal << 8) | szTmpBuf[31][1017];
			nRetVal = (nRetVal << 8) | szTmpBuf[31][1016];
		} else if (eEmemType == EMEM_INFO) {
			nRetVal = szTmpBuf[31][1023];
			nRetVal = (nRetVal << 8) | szTmpBuf[31][1022];
			nRetVal = (nRetVal << 8) | szTmpBuf[31][1021];
			nRetVal = (nRetVal << 8) | szTmpBuf[31][1020];
		}
	}

	return nRetVal;
}

static u32 _DrvFwCtrlMsg26xxmCalculateInfoCrcByDeviceDriver(void)
{
	u32 nRetVal = 0xffffffff;
	u32 i, j;
	u16 nRegData = 0;
	u8 szDbBusTxData[5] = {0};

	DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
	DmaReset();
#endif
#endif

	DrvPlatformLyrTouchDeviceResetHw();

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	RegSetLByteValue(0x0FE6, 0x01);
	RegSet16BitValue(0x3C60, 0xAA55);
	RegSet16BitValue(0x3CE4, 0xA4AB);
	RegSet16BitValue(0x1E04, 0x7d60);
	RegSet16BitValue(0x1E04, 0x829F);
	RegSetLByteValue(0x0FE6, 0x00);

	mdelay(100);


	do {
		nRegData = RegGet16BitValue(0x3CE4);
	} while (nRegData != 0x5B58);

	DrvCommonCrcInitTable();


	szDbBusTxData[0] = 0x72;
	szDbBusTxData[3] = 0x00;
	szDbBusTxData[4] = 0x80;

	for (i = 0; i < 8; i++) {
		for (j = 0; j < 8; j++) {
			szDbBusTxData[1] = 0x80 + (i*0x04) + (((j*128)&0xff00)>>8);
			szDbBusTxData[2] = (j*128)&0x00ff;

			IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 5);


			IicReadData(SLAVE_I2C_ID_DWI2C, &_gTempData[j*128], 128);
		}

		if (i == 0) {
			for (j = 4; j < 1024; j++) {
				nRetVal = DrvCommonCrcGetValue(_gTempData[j], nRetVal);
			}
		} else {
			for (j = 0; j < 1024; j++) {
				nRetVal = DrvCommonCrcGetValue(_gTempData[j], nRetVal);
			}
		}
	}

	nRetVal = nRetVal ^ 0xffffffff;

	DBG("Info(8K-4) CRC = 0x%x\n", nRetVal);

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	return nRetVal;
}

static s32 _DrvFwCtrlMsg26xxmCompare8BytesForCrc(u8 szTmpBuf[][1024])
{
	s32 nRetVal = -1;
	u16 nRegData = 0;
	u8 szDbBusTxData[5] = {0};
	u8 szDbBusRxData[8] = {0};
	u8 crc[8] = {0};

	DBG("*** %s() ***\n", __func__);


	if (szTmpBuf != NULL) {
		crc[0] = szTmpBuf[31][1016];
		crc[1] = szTmpBuf[31][1017];
		crc[2] = szTmpBuf[31][1018];
		crc[3] = szTmpBuf[31][1019];
		crc[4] = szTmpBuf[31][1020];
		crc[5] = szTmpBuf[31][1021];
		crc[6] = szTmpBuf[31][1022];
		crc[7] = szTmpBuf[31][1023];
	}

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	RegSetLByteValue(0x0FE6, 0x01);
	RegSet16BitValue(0x3C60, 0xAA55);
	RegSet16BitValue(0x3CE4, 0xA4AB);
	RegSet16BitValue(0x1E04, 0x7d60);
	RegSet16BitValue(0x1E04, 0x829F);
	RegSetLByteValue(0x0FE6, 0x00);

	mdelay(100);


	do {
		nRegData = RegGet16BitValue(0x3CE4);
	} while (nRegData != 0x5B58);

	szDbBusTxData[0] = 0x72;
	szDbBusTxData[1] = 0x7F;
	szDbBusTxData[2] = 0xF8;
	szDbBusTxData[3] = 0x00;
	szDbBusTxData[4] = 0x08;

	IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 5);
	IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 8);

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	if (crc[0] == szDbBusRxData[0]
		&& crc[1] == szDbBusRxData[1]
		&& crc[2] == szDbBusRxData[2]
		&& crc[3] == szDbBusRxData[3]
		&& crc[4] == szDbBusRxData[4]
		&& crc[5] == szDbBusRxData[5]
		&& crc[6] == szDbBusRxData[6]
		&& crc[7] == szDbBusRxData[7]) {
		nRetVal = 0;
	} else {
		nRetVal = -1;
	}

	DBG("compare 8bytes for CRC = %d\n", nRetVal);

	return nRetVal;
}

static void _DrvFwCtrlMsg26xxmEraseFirmwareOnEFlash(EmemType_e eEmemType)
{
	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

	DBG("erase 0\n");

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	RegSetLByteValue(0x0FE6, 0x01);
	RegSet16BitValue(0x3C60, 0xAA55);
	RegSet16BitValue(0x3CE4, 0x78C5);
	RegSet16BitValue(0x1E04, 0x7d60);
	RegSet16BitValue(0x1E04, 0x829F);
	RegSetLByteValue(0x0FE6, 0x00);

	mdelay(100);
	DBG("erase 1\n");

	RegSetLByteValue(0x0FE6, 0x01);
	RegSet16BitValue(0x3C60, 0xAA55);
	RegSet16BitValue(0x161A, 0xABBA);

	if (eEmemType == EMEM_INFO) {
		RegSet16BitValue(0x1600, 0x8000);
	}

	RegSetLByteValue(0x1618, 0x80);
	DBG("erase 2\n");

	RegSetLByteValue(0x1618, 0x40);
	mdelay(10);

	RegSetLByteValue(0x1618, 0x80);
	DBG("erase 3\n");

	if (eEmemType == EMEM_ALL) {
		RegSetLByteValue(0x160E, 0x08);
	} else if (eEmemType == EMEM_MAIN || eEmemType == EMEM_INFO) {
		RegSetLByteValue(0x160E, 0x04);
	}

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	mdelay(200);

	DBG("erase OK\n");
}

static void _DrvFwCtrlMsg26xxmProgramFirmwareOnEFlash(EmemType_e eEmemType)
{
	u32 nStart = 0, nEnd = 0;
	u32 i, j;
	u16 nRegData = 0;


	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
	DmaReset();
#endif
#endif

	DBG("program 0\n");

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	if (eEmemType == EMEM_INFO || eEmemType == EMEM_MAIN) {

		RegSetLByteValue(0x0FE6, 0x01);
		RegSet16BitValue(0x3C60, 0xAA55);
		RegSet16BitValue(0x3CE4, 0x78C5);
		RegSet16BitValue(0x1E04, 0x7d60);
		RegSet16BitValue(0x1E04, 0x829F);
		nRegData = RegGet16BitValue(0x1618);

		nRegData |= 0x40;
		RegSetLByteValue(0x1618, nRegData);
		RegSetLByteValue(0x0FE6, 0x00);
		mdelay(100);
	}

	DBG("program 1\n");

	RegSet16BitValue(0x0F52, 0xDB00);


	do {
		nRegData = RegGet16BitValue(0x3CE4);

	} while (nRegData != 0x1C70);

	DBG("program 2\n");

	if (eEmemType == EMEM_ALL) {
		RegSet16BitValue(0x3CE4, 0xE38F);

		nStart = 0;
		nEnd = MSG26XXM_FIRMWARE_WHOLE_SIZE;
	} else if (eEmemType == EMEM_MAIN) {
		RegSet16BitValue(0x3CE4, 0x7731);

		nStart = 0;
		nEnd = MSG26XXM_FIRMWARE_MAIN_BLOCK_SIZE;
	} else if (eEmemType == EMEM_INFO) {
		RegSet16BitValue(0x3CE4, 0xB9D6);

		nStart = MSG26XXM_FIRMWARE_MAIN_BLOCK_SIZE;
		nEnd = MSG26XXM_FIRMWARE_MAIN_BLOCK_SIZE + MSG26XXM_FIRMWARE_INFO_BLOCK_SIZE;
	}


	do {
		nRegData = RegGet16BitValue(0x3CE4);
	} while (nRegData != 0x2F43);

	DBG("program 3\n");

	for (i = nStart; i < nEnd; i++) {
		for (j = 0; j < 8; j++) {
			IicWriteData(SLAVE_I2C_ID_DWI2C, &g_FwData[i][j*128], 128);
		}

		mdelay(100);


		do {
			nRegData = RegGet16BitValue(0x3CE4);
		} while (nRegData != 0xD0BC);


		RegSet16BitValue(0x3CE4, 0x2F43);
	}

	DBG("program 4\n");


	RegSet16BitValue(0x3CE4, 0x1380);
	mdelay(100);

	DBG("program 5\n");


	do {
		nRegData = RegGet16BitValue(0x3CE4);
	} while (nRegData != 0x9432);

	DBG("program 6\n");

	g_FwDataCount = 0;

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	mdelay(300);

	DBG("program OK\n");
}

static s32 _DrvFwCtrlMsg26xxmUpdateFirmwareBySwId(void)
{
	s32 nRetVal = -1;
	u32 nCrcInfoA = 0, nCrcInfoB = 0, nCrcMainA = 0, nCrcMainB = 0;

	DBG("*** %s() ***\n", __func__);

	DBG("_gIsUpdateInfoBlockFirst = %d, g_IsUpdateFirmware = 0x%x\n", _gIsUpdateInfoBlockFirst, g_IsUpdateFirmware);

	if (_gIsUpdateInfoBlockFirst == 1) {
		if ((g_IsUpdateFirmware & 0x10) == 0x10) {
			_DrvFwCtrlMsg26xxmEraseFirmwareOnEFlash(EMEM_INFO);
			_DrvFwCtrlMsg26xxmProgramFirmwareOnEFlash(EMEM_INFO);

			nCrcInfoA = _DrvFwCtrlMsg26xxmRetrieveFrimwareCrcFromBinFile(g_FwData, EMEM_INFO);
			nCrcInfoB = _DrvFwCtrlMsg26xxmCalculateFirmwareCrcFromEFlash(EMEM_INFO, 0);

			DBG("nCrcInfoA = 0x%x, nCrcInfoB = 0x%x\n", nCrcInfoA, nCrcInfoB);

			if (nCrcInfoA == nCrcInfoB) {
				_DrvFwCtrlMsg26xxmEraseFirmwareOnEFlash(EMEM_MAIN);
				_DrvFwCtrlMsg26xxmProgramFirmwareOnEFlash(EMEM_MAIN);

				nCrcMainA = _DrvFwCtrlMsg26xxmRetrieveFrimwareCrcFromBinFile(g_FwData, EMEM_MAIN);
				nCrcMainB = _DrvFwCtrlMsg26xxmCalculateFirmwareCrcFromEFlash(EMEM_MAIN, 0);

				DBG("nCrcMainA = 0x%x, nCrcMainB = 0x%x\n", nCrcMainA, nCrcMainB);

				if (nCrcMainA == nCrcMainB) {
					nRetVal = _DrvFwCtrlMsg26xxmCompare8BytesForCrc(g_FwData);

					if (nRetVal == 0) {
						g_IsUpdateFirmware = 0x00;
					} else {
						g_IsUpdateFirmware = 0x11;
					}
				} else {
					g_IsUpdateFirmware = 0x01;
				}
			} else {
				g_IsUpdateFirmware = 0x11;
			}
		} else if ((g_IsUpdateFirmware & 0x01) == 0x01) {
			_DrvFwCtrlMsg26xxmEraseFirmwareOnEFlash(EMEM_MAIN);
			_DrvFwCtrlMsg26xxmProgramFirmwareOnEFlash(EMEM_MAIN);

			nCrcMainA = _DrvFwCtrlMsg26xxmRetrieveFrimwareCrcFromBinFile(g_FwData, EMEM_MAIN);
			nCrcMainB = _DrvFwCtrlMsg26xxmCalculateFirmwareCrcFromEFlash(EMEM_MAIN, 0);

			DBG("nCrcMainA=0x%x, nCrcMainB=0x%x\n", nCrcMainA, nCrcMainB);

			if (nCrcMainA == nCrcMainB) {
				nRetVal = _DrvFwCtrlMsg26xxmCompare8BytesForCrc(g_FwData);

				if (nRetVal == 0) {
					g_IsUpdateFirmware = 0x00;
				} else {
					g_IsUpdateFirmware = 0x11;
				}
			} else {
				g_IsUpdateFirmware = 0x01;
			}
		}
	} else {
		if ((g_IsUpdateFirmware & 0x10) == 0x10) {
			_DrvFwCtrlMsg26xxmEraseFirmwareOnEFlash(EMEM_MAIN);
			_DrvFwCtrlMsg26xxmProgramFirmwareOnEFlash(EMEM_MAIN);

			nCrcMainA = _DrvFwCtrlMsg26xxmRetrieveFrimwareCrcFromBinFile(g_FwData, EMEM_MAIN);
			nCrcMainB = _DrvFwCtrlMsg26xxmCalculateFirmwareCrcFromEFlash(EMEM_MAIN, 0);

			DBG("nCrcMainA=0x%x, nCrcMainB=0x%x\n", nCrcMainA, nCrcMainB);

			if (nCrcMainA == nCrcMainB) {
				_DrvFwCtrlMsg26xxmEraseFirmwareOnEFlash(EMEM_INFO);
				_DrvFwCtrlMsg26xxmProgramFirmwareOnEFlash(EMEM_INFO);

				nCrcInfoA = _DrvFwCtrlMsg26xxmRetrieveFrimwareCrcFromBinFile(g_FwData, EMEM_INFO);
				nCrcInfoB = _DrvFwCtrlMsg26xxmCalculateFirmwareCrcFromEFlash(EMEM_INFO, 0);

				DBG("nCrcInfoA=0x%x, nCrcInfoB=0x%x\n", nCrcInfoA, nCrcInfoB);

				if (nCrcInfoA == nCrcInfoB) {
					nRetVal = _DrvFwCtrlMsg26xxmCompare8BytesForCrc(g_FwData);

					if (nRetVal == 0) {
						g_IsUpdateFirmware = 0x00;
					} else {
						g_IsUpdateFirmware = 0x11;
					}
				} else {
					g_IsUpdateFirmware = 0x01;
				}
			} else {
				g_IsUpdateFirmware = 0x11;
			}
		} else if ((g_IsUpdateFirmware & 0x01) == 0x01) {
			_DrvFwCtrlMsg26xxmEraseFirmwareOnEFlash(EMEM_INFO);
			_DrvFwCtrlMsg26xxmProgramFirmwareOnEFlash(EMEM_INFO);

			nCrcInfoA = _DrvFwCtrlMsg26xxmRetrieveFrimwareCrcFromBinFile(g_FwData, EMEM_INFO);
			nCrcInfoB = _DrvFwCtrlMsg26xxmCalculateFirmwareCrcFromEFlash(EMEM_INFO, 0);

			DBG("nCrcInfoA=0x%x, nCrcInfoB=0x%x\n", nCrcInfoA, nCrcInfoB);

			if (nCrcInfoA == nCrcInfoB) {
				nRetVal = _DrvFwCtrlMsg26xxmCompare8BytesForCrc(g_FwData);

				if (nRetVal == 0) {
					g_IsUpdateFirmware = 0x00;
				} else {
					g_IsUpdateFirmware = 0x11;
				}
			} else {
				g_IsUpdateFirmware = 0x01;
			}
		}
	}

	return nRetVal;
}

void _DrvFwCtrlMsg26xxmCheckFirmwareUpdateBySwId(void)
{
	u32 nCrcMainA, nCrcInfoA, nCrcMainB, nCrcInfoB;
	u32 i;
	u16 nUpdateBinMajor = 0, nUpdateBinMinor = 0;
	u16 nMajor = 0, nMinor = 0;
	u8 *pVersion = NULL;
	Msg26xxmSwId_e eSwId = MSG26XXM_SW_ID_UNDEFINED;

	DBG("*** %s() ***\n", __func__);

	DrvPlatformLyrDisableFingerTouchReport();

	nCrcMainA = _DrvFwCtrlMsg26xxmCalculateFirmwareCrcFromEFlash(EMEM_MAIN, 1);
	nCrcMainB = _DrvFwCtrlMsg26xxmRetrieveFirmwareCrcFromMainBlock(EMEM_MAIN);

	nCrcInfoA = _DrvFwCtrlMsg26xxmCalculateFirmwareCrcFromEFlash(EMEM_INFO, 1);
	nCrcInfoB = _DrvFwCtrlMsg26xxmRetrieveFirmwareCrcFromMainBlock(EMEM_INFO);

	_gUpdateFirmwareBySwIdWorkQueue = create_singlethread_workqueue("update_firmware_by_sw_id");
	INIT_WORK(&_gUpdateFirmwareBySwIdWork, _DrvFwCtrlUpdateFirmwareBySwIdDoWork);

	DBG("nCrcMainA=0x%x, nCrcInfoA=0x%x, nCrcMainB=0x%x, nCrcInfoB=0x%x\n", nCrcMainA, nCrcInfoA, nCrcMainB, nCrcInfoB);

	if (nCrcMainA == nCrcMainB && nCrcInfoA == nCrcInfoB) {
		eSwId = _DrvFwCtrlMsg26xxmGetSwId(EMEM_MAIN);

		DBG("eSwId=0x%x\n", eSwId);

		if (eSwId == MSG26XXM_SW_ID_XXXX) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
			nUpdateBinMajor = msg26xxm_xxxx_update_bin[0][0x2B]<<8 | msg26xxm_xxxx_update_bin[0][0x2A];
			nUpdateBinMinor = msg26xxm_xxxx_update_bin[0][0x2D]<<8 | msg26xxm_xxxx_update_bin[0][0x2C];
#else
			nUpdateBinMajor = msg26xxm_xxxx_update_bin[0x002B]<<8 | msg26xxm_xxxx_update_bin[0x002A];
			nUpdateBinMinor = msg26xxm_xxxx_update_bin[0x002D]<<8 | msg26xxm_xxxx_update_bin[0x002C];
#endif
		} else if (eSwId == MSG26XXM_SW_ID_YYYY) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
			nUpdateBinMajor = msg26xxm_yyyy_update_bin[0][0x2B]<<8 | msg26xxm_yyyy_update_bin[0][0x2A];
			nUpdateBinMinor = msg26xxm_yyyy_update_bin[0][0x2D]<<8 | msg26xxm_yyyy_update_bin[0][0x2C];
#else
			nUpdateBinMajor = msg26xxm_yyyy_update_bin[0x002B]<<8 | msg26xxm_yyyy_update_bin[0x002A];
			nUpdateBinMinor = msg26xxm_yyyy_update_bin[0x002D]<<8 | msg26xxm_yyyy_update_bin[0x002C];
#endif
		} else {
			DBG("eSwId = 0x%x is an undefined SW ID.\n", eSwId);

			eSwId = MSG26XXM_SW_ID_UNDEFINED;
			nUpdateBinMajor = 0;
			nUpdateBinMinor = 0;
		}

		DrvFwCtrlGetCustomerFirmwareVersion(&nMajor, &nMinor, &pVersion);

		DBG("eSwId=0x%x, nMajor=%d, nMinor=%d, nUpdateBinMajor=%d, nUpdateBinMinor=%d\n", eSwId, nMajor, nMinor, nUpdateBinMajor, nUpdateBinMinor);

		if (nUpdateBinMinor > nMinor) {
			if (eSwId == MSG26XXM_SW_ID_XXXX) {
				for (i = 0; i < MSG26XXM_FIRMWARE_WHOLE_SIZE; i++) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
					_DrvFwCtrlStoreFirmwareData(msg26xxm_xxxx_update_bin[i], 1024);
#else
					_DrvFwCtrlStoreFirmwareData(&(msg26xxm_xxxx_update_bin[i*1024]), 1024);
#endif
				}
			} else if (eSwId == MSG26XXM_SW_ID_YYYY) {
				for (i = 0; i < MSG26XXM_FIRMWARE_WHOLE_SIZE; i++) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
					_DrvFwCtrlStoreFirmwareData(msg26xxm_yyyy_update_bin[i], 1024);
#else
					_DrvFwCtrlStoreFirmwareData(&(msg26xxm_yyyy_update_bin[i*1024]), 1024);
#endif
				}
			} else {
				DBG("eSwId = 0x%x is an undefined SW ID.\n", eSwId);

				eSwId = MSG26XXM_SW_ID_UNDEFINED;
			}

			if (eSwId < MSG26XXM_SW_ID_UNDEFINED && eSwId != 0x0000 && eSwId != 0xFFFF) {
				g_FwDataCount = 0;

				_gUpdateRetryCount = UPDATE_FIRMWARE_RETRY_COUNT;
				_gIsUpdateInfoBlockFirst = 1;
				g_IsUpdateFirmware = 0x11;
				queue_work(_gUpdateFirmwareBySwIdWorkQueue, &_gUpdateFirmwareBySwIdWork);
				return;
			} else {
				DBG("The sw id is invalid.\n");
				DBG("Go to normal boot up process.\n");
			}
		} else {
			DBG("The update bin version is older than or equal to the current firmware version on e-flash.\n");
			DBG("Go to normal boot up process.\n");
		}
	} else if (nCrcMainA == nCrcMainB && nCrcInfoA != nCrcInfoB) {
		eSwId = _DrvFwCtrlMsg26xxmGetSwId(EMEM_MAIN);

		DBG("eSwId=0x%x\n", eSwId);

		if (eSwId == MSG26XXM_SW_ID_XXXX) {
			for (i = 0; i < MSG26XXM_FIRMWARE_WHOLE_SIZE; i++) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
				_DrvFwCtrlStoreFirmwareData(msg26xxm_xxxx_update_bin[i], 1024);
#else
				_DrvFwCtrlStoreFirmwareData(&(msg26xxm_xxxx_update_bin[i*1024]), 1024);
#endif
			}
		} else if (eSwId == MSG26XXM_SW_ID_YYYY) {
			for (i = 0; i < MSG26XXM_FIRMWARE_WHOLE_SIZE; i++) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
				_DrvFwCtrlStoreFirmwareData(msg26xxm_yyyy_update_bin[i], 1024);
#else
				_DrvFwCtrlStoreFirmwareData(&(msg26xxm_yyyy_update_bin[i*1024]), 1024);
#endif
			}
		} else {
			DBG("eSwId = 0x%x is an undefined SW ID.\n", eSwId);

			eSwId = MSG26XXM_SW_ID_UNDEFINED;
		}

		if (eSwId < MSG26XXM_SW_ID_UNDEFINED && eSwId != 0x0000 && eSwId != 0xFFFF) {
			g_FwDataCount = 0;

			_gUpdateRetryCount = UPDATE_FIRMWARE_RETRY_COUNT;
			_gIsUpdateInfoBlockFirst = 1;
			g_IsUpdateFirmware = 0x11;
			queue_work(_gUpdateFirmwareBySwIdWorkQueue, &_gUpdateFirmwareBySwIdWork);
			return;
		} else {
			DBG("The sw id is invalid.\n");
			DBG("Go to normal boot up process.\n");
		}
	} else {
		nCrcInfoA = _DrvFwCtrlMsg26xxmRetrieveInfoCrcFromInfoBlock();
		nCrcInfoB = _DrvFwCtrlMsg26xxmCalculateInfoCrcByDeviceDriver();

		DBG("8K-4 : nCrcInfoA=0x%x, nCrcInfoB=0x%x\n", nCrcInfoA, nCrcInfoB);

		if (nCrcInfoA == nCrcInfoB) {
			eSwId = _DrvFwCtrlMsg26xxmGetSwId(EMEM_INFO);

			DBG("eSwId=0x%x\n", eSwId);

			if (eSwId == MSG26XXM_SW_ID_XXXX) {
				for (i = 0; i < MSG26XXM_FIRMWARE_WHOLE_SIZE; i++) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
					_DrvFwCtrlStoreFirmwareData(msg26xxm_xxxx_update_bin[i], 1024);
#else
					_DrvFwCtrlStoreFirmwareData(&(msg26xxm_xxxx_update_bin[i*1024]), 1024);
#endif
				}
			} else if (eSwId == MSG26XXM_SW_ID_YYYY) {
				for (i = 0; i < MSG26XXM_FIRMWARE_WHOLE_SIZE; i++) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
					_DrvFwCtrlStoreFirmwareData(msg26xxm_yyyy_update_bin[i], 1024);
#else
					_DrvFwCtrlStoreFirmwareData(&(msg26xxm_yyyy_update_bin[i*1024]), 1024);
#endif
				}
			} else {
				DBG("eSwId = 0x%x is an undefined SW ID.\n", eSwId);

				eSwId = MSG26XXM_SW_ID_UNDEFINED;
			}
			if (eSwId < MSG26XXM_SW_ID_UNDEFINED && eSwId != 0x0000 && eSwId != 0xFFFF) {
				g_FwDataCount = 0;

				_gUpdateRetryCount = UPDATE_FIRMWARE_RETRY_COUNT;
				_gIsUpdateInfoBlockFirst = 0;
				g_IsUpdateFirmware = 0x11;
				queue_work(_gUpdateFirmwareBySwIdWorkQueue, &_gUpdateFirmwareBySwIdWork);
				return;
			} else {
				DBG("The sw id is invalid.\n");
				DBG("Go to normal boot up process.\n");
			}
		} else {
			DBG("Info block is broken.\n");
		}
	}

	DrvPlatformLyrTouchDeviceResetHw();

	DrvPlatformLyrEnableFingerTouchReport();
}

static u32 _DrvFwCtrlMsg28xxRetrieveFirmwareCrcFromEFlash(EmemType_e eEmemType)
{
	u32 nRetVal = 0;
	u16 nReadAddr = 0;
	u8  szTmpData[4] = {0};

	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);


	RegSetLByteValue(0x0FE6, 0x01);


	RegSet16BitValue(0x3C60, 0xAA55);

	if (eEmemType == EMEM_MAIN) {
		_DrvFwCtrlMsg28xxReadEFlashStart(0x7FFF, EMEM_MAIN);
		nReadAddr = 0x7FFF;
	} else if (eEmemType == EMEM_INFO) {
		_DrvFwCtrlMsg28xxReadEFlashStart(0x81FF, EMEM_INFO);
		nReadAddr = 0x81FF;
	}

	_DrvFwCtrlMsg28xxReadEFlashDoRead(nReadAddr, &szTmpData[0]);

	DBG("szTmpData[0] = 0x%x\n", szTmpData[0]);
	DBG("szTmpData[1] = 0x%x\n", szTmpData[1]);
	DBG("szTmpData[2] = 0x%x\n", szTmpData[2]);
	DBG("szTmpData[3] = 0x%x\n", szTmpData[3]);

	_DrvFwCtrlMsg28xxReadEFlashEnd();

	nRetVal = (szTmpData[3] << 24);
	nRetVal |= (szTmpData[2] << 16);
	nRetVal |= (szTmpData[1] << 8);
	nRetVal |= szTmpData[0];

	DBG("CRC = 0x%x\n", nRetVal);

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	return nRetVal;
}

static u32 _DrvFwCtrlMsg28xxRetrieveFrimwareCrcFromBinFile(u8 szTmpBuf[][1024], EmemType_e eEmemType)
{
	u32 nRetVal = 0;

	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

	if (szTmpBuf != NULL) {
		if (eEmemType == EMEM_MAIN) {
			nRetVal = szTmpBuf[127][1023];
			nRetVal = (nRetVal << 8) | szTmpBuf[127][1022];
			nRetVal = (nRetVal << 8) | szTmpBuf[127][1021];
			nRetVal = (nRetVal << 8) | szTmpBuf[127][1020];
		} else if (eEmemType == EMEM_INFO) {
			nRetVal = szTmpBuf[129][1023];
			nRetVal = (nRetVal << 8) | szTmpBuf[129][1022];
			nRetVal = (nRetVal << 8) | szTmpBuf[129][1021];
			nRetVal = (nRetVal << 8) | szTmpBuf[129][1020];
		}
	}

	return nRetVal;
}


void _DrvFwCtrlMsg28xxCheckFirmwareUpdateBySwId(void)
{
	u32 nCrcMainA, nCrcMainB;
	u32 i;
	u16 nUpdateBinMajor = 0, nUpdateBinMinor = 0;
	u16 nMajor = 0, nMinor = 0;
	u8 *pVersion = NULL;
	Msg28xxSwId_e eMainSwId = MSG28XX_SW_ID_UNDEFINED, eInfoSwId = MSG28XX_SW_ID_UNDEFINED, eSwId = MSG28XX_SW_ID_UNDEFINED;

	DBG("*** %s() ***\n", __func__);

	DrvPlatformLyrDisableFingerTouchReport();

	nCrcMainA = _DrvFwCtrlMsg28xxGetFirmwareCrcByHardware(EMEM_MAIN);
	nCrcMainB = _DrvFwCtrlMsg28xxRetrieveFirmwareCrcFromEFlash(EMEM_MAIN);

	DBG("nCrcMainA=0x%x, nCrcMainB=0x%x\n", nCrcMainA, nCrcMainB);

#ifdef CONFIG_ENABLE_CODE_FOR_DEBUG
	if (nCrcMainA != nCrcMainB) {
		for (i = 0; i < 5; i++) {
			nCrcMainA = _DrvFwCtrlMsg28xxGetFirmwareCrcByHardware(EMEM_MAIN);
			nCrcMainB = _DrvFwCtrlMsg28xxRetrieveFirmwareCrcFromEFlash(EMEM_MAIN);

			DBG("*** Retry[%d] : nCrcMainA=0x%x, nCrcMainB=0x%x ***\n", i, nCrcMainA, nCrcMainB);

			if (nCrcMainA == nCrcMainB) {
				break;
			}

			mdelay(50);
		}
	}
#endif

	_gUpdateFirmwareBySwIdWorkQueue = create_singlethread_workqueue("update_firmware_by_sw_id");
	INIT_WORK(&_gUpdateFirmwareBySwIdWork, _DrvFwCtrlUpdateFirmwareBySwIdDoWork);

	if (nCrcMainA == nCrcMainB) {
		eMainSwId = _DrvFwCtrlMsg28xxGetSwId(EMEM_MAIN);

		DBG("eMainSwId=0x%x\n", eMainSwId);

		eSwId = eMainSwId;

		if (eSwId == MSG28XX_SW_ID_XXXX) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
			nUpdateBinMajor = msg28xx_xxxx_update_bin[127][1013]<<8 | msg28xx_xxxx_update_bin[127][1012];
			nUpdateBinMinor = msg28xx_xxxx_update_bin[127][1015]<<8 | msg28xx_xxxx_update_bin[127][1014];
#else
			nUpdateBinMajor = msg28xx_xxxx_update_bin[0x1FFF5]<<8 | msg28xx_xxxx_update_bin[0x1FFF4];
			nUpdateBinMinor = msg28xx_xxxx_update_bin[0x1FFF7]<<8 | msg28xx_xxxx_update_bin[0x1FFF6];
#endif
		} else if (eSwId == MSG28XX_SW_ID_YYYY) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
			nUpdateBinMajor = msg28xx_yyyy_update_bin[127][1013]<<8 | msg28xx_yyyy_update_bin[127][1012];
			nUpdateBinMinor = msg28xx_yyyy_update_bin[127][1015]<<8 | msg28xx_yyyy_update_bin[127][1014];
#else
			nUpdateBinMajor = msg28xx_yyyy_update_bin[0x1FFF5]<<8 | msg28xx_yyyy_update_bin[0x1FFF4];
			nUpdateBinMinor = msg28xx_yyyy_update_bin[0x1FFF7]<<8 | msg28xx_yyyy_update_bin[0x1FFF6];
#endif
		} else {
			DBG("eSwId = 0x%x is an undefined SW ID.\n", eSwId);

			eSwId = MSG28XX_SW_ID_UNDEFINED;
			nUpdateBinMajor = 0;
			nUpdateBinMinor = 0;
		}

		DrvFwCtrlGetCustomerFirmwareVersionByDbBus(EMEM_MAIN, &nMajor, &nMinor, &pVersion);

		DBG("eSwId=0x%x, nMajor=%d, nMinor=%d, nUpdateBinMajor=%d, nUpdateBinMinor=%d\n", eSwId, nMajor, nMinor, nUpdateBinMajor, nUpdateBinMinor);

		if (nUpdateBinMinor > nMinor) {
			if (eSwId == MSG28XX_SW_ID_XXXX) {
				for (i = 0; i < MSG28XX_FIRMWARE_WHOLE_SIZE; i++) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
					_DrvFwCtrlStoreFirmwareData(msg28xx_xxxx_update_bin[i], 1024);
#else
					_DrvFwCtrlStoreFirmwareData(&(msg28xx_xxxx_update_bin[i*1024]), 1024);
#endif
				}
			} else if (eSwId == MSG28XX_SW_ID_YYYY) {
				for (i = 0; i < MSG28XX_FIRMWARE_WHOLE_SIZE; i++) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
					_DrvFwCtrlStoreFirmwareData(msg28xx_yyyy_update_bin[i], 1024);
#else
					_DrvFwCtrlStoreFirmwareData(&(msg28xx_yyyy_update_bin[i*1024]), 1024);
#endif
				}
			} else {
				DBG("eSwId = 0x%x is an undefined SW ID.\n", eSwId);

				eSwId = MSG28XX_SW_ID_UNDEFINED;
			}

			if (eSwId < MSG28XX_SW_ID_UNDEFINED && eSwId != 0x0000 && eSwId != 0xFFFF) {
				g_FwDataCount = 0;

				_gUpdateRetryCount = UPDATE_FIRMWARE_RETRY_COUNT;
				_gIsUpdateInfoBlockFirst = 1;
				g_IsUpdateFirmware = 0x11;
				queue_work(_gUpdateFirmwareBySwIdWorkQueue, &_gUpdateFirmwareBySwIdWork);
				return;
			} else {
				DBG("The sw id is invalid.\n");
				DBG("Go to normal boot up process.\n");
			}
		} else {
			DBG("The update bin version is older than or equal to the current firmware version on e-flash.\n");
			DBG("Go to normal boot up process.\n");
		}
	} else {
		eSwId = _DrvFwCtrlMsg28xxGetSwId(EMEM_INFO);

		DBG("eSwId=0x%x\n", eSwId);

		if (eSwId == MSG28XX_SW_ID_XXXX) {
			for (i = 0; i < MSG28XX_FIRMWARE_WHOLE_SIZE; i++) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
				_DrvFwCtrlStoreFirmwareData(msg28xx_xxxx_update_bin[i], 1024);
#else
				_DrvFwCtrlStoreFirmwareData(&(msg28xx_xxxx_update_bin[i*1024]), 1024);
#endif
			}
		} else if (eSwId == MSG28XX_SW_ID_YYYY) {
			for (i = 0; i < MSG28XX_FIRMWARE_WHOLE_SIZE; i++) {
#ifdef CONFIG_UPDATE_FIRMWARE_BY_TWO_DIMENSIONAL_ARRAY
				_DrvFwCtrlStoreFirmwareData(msg28xx_yyyy_update_bin[i], 1024);
#else
				_DrvFwCtrlStoreFirmwareData(&(msg28xx_yyyy_update_bin[i*1024]), 1024);
#endif
			}
		} else {
			DBG("eSwId = 0x%x is an undefined SW ID.\n", eSwId);

			eSwId = MSG28XX_SW_ID_UNDEFINED;
		}

		if (eSwId < MSG28XX_SW_ID_UNDEFINED && eSwId != 0x0000 && eSwId != 0xFFFF) {
			g_FwDataCount = 0;

			_gUpdateRetryCount = UPDATE_FIRMWARE_RETRY_COUNT;
			_gIsUpdateInfoBlockFirst = 0;
			g_IsUpdateFirmware = 0x11;
			queue_work(_gUpdateFirmwareBySwIdWorkQueue, &_gUpdateFirmwareBySwIdWork);
			return;
		} else {
			DBG("The sw id is invalid.\n");
			DBG("Go to normal boot up process.\n");
		}
	}

	DrvPlatformLyrTouchDeviceResetHw();

	DrvPlatformLyrEnableFingerTouchReport();
}



static void _DrvFwCtrlUpdateFirmwareBySwIdDoWork(struct work_struct *pWork)
{
	s32 nRetVal = 0;

	DBG("*** %s() _gUpdateRetryCount = %d ***\n", __func__, _gUpdateRetryCount);

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		nRetVal = _DrvFwCtrlMsg26xxmUpdateFirmwareBySwId();
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		nRetVal = _DrvFwCtrlMsg28xxUpdateFirmware(g_FwData, EMEM_MAIN);
	} else {
		DBG("This chip type (0x%x) does not support update firmware by sw id\n", g_ChipType);

		DrvPlatformLyrTouchDeviceResetHw();

		DrvPlatformLyrEnableFingerTouchReport();

		nRetVal = -1;
		return;
	}

	DBG("*** update firmware by sw id result = %d ***\n", nRetVal);

	if (nRetVal == 0) {
		DBG("update firmware by sw id success\n");

		_gIsUpdateInfoBlockFirst = 0;
		g_IsUpdateFirmware = 0x00;

		DrvPlatformLyrTouchDeviceResetHw();

		DrvPlatformLyrEnableFingerTouchReport();
	} else {
		_gUpdateRetryCount--;
		if (_gUpdateRetryCount > 0) {
			DBG("_gUpdateRetryCount = %d\n", _gUpdateRetryCount);
			queue_work(_gUpdateFirmwareBySwIdWorkQueue, &_gUpdateFirmwareBySwIdWork);
		} else {
			DBG("update firmware by sw id failed\n");

			_gIsUpdateInfoBlockFirst = 0;
			g_IsUpdateFirmware = 0x00;

			DrvPlatformLyrTouchDeviceResetHw();

			DrvPlatformLyrEnableFingerTouchReport();
		}
	}
}

#endif

void DrvFwCtrlVariableInitialize(void)
{
	DBG("*** %s() ***\n", __func__);

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		FIRMWARE_MODE_UNKNOWN_MODE = MSG26XXM_FIRMWARE_MODE_UNKNOWN_MODE;
		FIRMWARE_MODE_DEMO_MODE = MSG26XXM_FIRMWARE_MODE_DEMO_MODE;
		FIRMWARE_MODE_DEBUG_MODE = MSG26XXM_FIRMWARE_MODE_DEBUG_MODE;


		g_FirmwareMode = FIRMWARE_MODE_DEMO_MODE;
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		FIRMWARE_MODE_UNKNOWN_MODE = MSG28XX_FIRMWARE_MODE_UNKNOWN_MODE;
		FIRMWARE_MODE_DEMO_MODE = MSG28XX_FIRMWARE_MODE_DEMO_MODE;
		FIRMWARE_MODE_DEBUG_MODE = MSG28XX_FIRMWARE_MODE_DEBUG_MODE;


		g_FirmwareMode = FIRMWARE_MODE_DEMO_MODE;
	}
}

void DrvFwCtrlOptimizeCurrentConsumption(void)
{
	u32 i;
	u8 szDbBusTxData[35] = {0};

	DBG("g_ChipType = 0x%x\n", g_ChipType);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	if (g_GestureWakeupFlag == 1) {
		return;
	}
#endif

	if (g_ChipType == CHIP_TYPE_MSG28XX) {
		DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
		DmaReset();
#endif
#endif

		DrvPlatformLyrTouchDeviceResetHw();

		DbBusEnterSerialDebugMode();
		DbBusStopMCU();
		DbBusIICUseBus();
		DbBusIICReshape();

		RegSetLByteValue(0x1608, 0x21);

		szDbBusTxData[0] = 0x10;
		szDbBusTxData[1] = 0x15;
		szDbBusTxData[2] = 0x20;

		for (i = 0; i < 8; i++) {
			szDbBusTxData[i+3] = 0xFF;
		}

		IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3+8);

		szDbBusTxData[0] = 0x10;
		szDbBusTxData[1] = 0x15;
		szDbBusTxData[2] = 0x28;

		for (i = 0; i < 16; i++) {
			szDbBusTxData[i+3] = 0x00;
		}

		IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3+16);

		szDbBusTxData[0] = 0x10;
		szDbBusTxData[1] = 0x21;
		szDbBusTxData[2] = 0x40;

		for (i = 0; i < 8; i++) {
			szDbBusTxData[i+3] = 0xFF;
		}

		IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3+8);

		szDbBusTxData[0] = 0x10;
		szDbBusTxData[1] = 0x21;
		szDbBusTxData[2] = 0x20;

		for (i = 0; i < 32; i++) {
			szDbBusTxData[i+3] = 0xFF;
		}

		IicWriteData(SLAVE_I2C_ID_DBBUS, &szDbBusTxData[0], 3+32);
		RegSetLByteValue(0x1608, 0x20);

		DbBusIICNotUseBus();
		DbBusNotStopMCU();
		DbBusExitSerialDebugMode();
	}
}

u8 DrvFwCtrlGetChipType(void)
{
	s32 rc = 0;
	u8 nChipType = 0;

	DBG("*** %s() ***\n", __func__);


	rc = DbBusEnterSerialDebugMode();
	if (rc < 0) {
		DBG("*** DbBusEnterSerialDebugMode() failed, rc = %d ***\n", rc);
		return nChipType;
	}
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();

	RegSetLByteValue(0x0FE6, 0x01);
	RegSet16BitValue(0x3C60, 0xAA55);
	nChipType = RegGet16BitValue(0x1ECC) & 0xFF;

	if (nChipType != CHIP_TYPE_MSG21XX &&
		nChipType != CHIP_TYPE_MSG21XXA &&
		nChipType != CHIP_TYPE_MSG26XXM &&
		nChipType != CHIP_TYPE_MSG22XX &&
		nChipType != CHIP_TYPE_MSG28XX) {
		nChipType = 0;
	}

	DBG("*** Chip Type = 0x%x ***\n", nChipType);

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	return nChipType;
}

void DrvFwCtrlGetCustomerFirmwareVersionByDbBus(EmemType_e eEmemType, u16 *pMajor, u16 *pMinor, u8 **ppVersion)
{
	u16 nReadAddr = 0;
	u8  szTmpData[4] = {0};

	DBG("*** %s() eEmemType = %d ***\n", __func__, eEmemType);

	if (g_ChipType == CHIP_TYPE_MSG28XX) {
		DbBusEnterSerialDebugMode();
		DbBusStopMCU();
		DbBusIICUseBus();
		DbBusIICReshape();
		mdelay(100);

		RegSetLByteValue(0x0FE6, 0x01);
		RegSet16BitValue(0x3C60, 0xAA55);

		if (eEmemType == EMEM_MAIN) {
			_DrvFwCtrlMsg28xxReadEFlashStart(0x7FFD, EMEM_MAIN);
			nReadAddr = 0x7FFD;
		} else if (eEmemType == EMEM_INFO) {
			_DrvFwCtrlMsg28xxReadEFlashStart(0x81FB, EMEM_INFO);
			nReadAddr = 0x81FB;
		}

		_DrvFwCtrlMsg28xxReadEFlashDoRead(nReadAddr, &szTmpData[0]);
		_DrvFwCtrlMsg28xxReadEFlashEnd();

		*pMajor = (szTmpData[1] << 8);
		*pMajor |= szTmpData[0];
		*pMinor = (szTmpData[3] << 8);
		*pMinor |= szTmpData[2];

		DBG("*** major = %d ***\n", *pMajor);
		DBG("*** minor = %d ***\n", *pMinor);

		if (*ppVersion == NULL) {
			*ppVersion = kzalloc(sizeof(u8)*6, GFP_KERNEL);
		}

		sprintf(*ppVersion, "%03d%03d", *pMajor, *pMinor);

		DbBusIICNotUseBus();
		DbBusNotStopMCU();
		DbBusExitSerialDebugMode();
	}
}

void DrvFwCtrlGetCustomerFirmwareVersion(u16 *pMajor, u16 *pMinor, u8 **ppVersion)
{
	u8 szDbBusTxData[3] = {0};
	u8 szDbBusRxData[4] = {0};

	DBG("*** %s() ***\n", __func__);

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		szDbBusTxData[0] = 0x53;
		szDbBusTxData[1] = 0x00;
		szDbBusTxData[2] = 0x2A;

		mutex_lock(&g_Mutex);

		DrvPlatformLyrTouchDeviceResetHw();

		IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 3);
		IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 4);

		mutex_unlock(&g_Mutex);
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		szDbBusTxData[0] = 0x03;

		mutex_lock(&g_Mutex);

		DrvPlatformLyrTouchDeviceResetHw();

		IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 1);
		IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 4);

		mutex_unlock(&g_Mutex);
	}

	DBG("szDbBusRxData[0] = 0x%x\n", szDbBusRxData[0]);
	DBG("szDbBusRxData[1] = 0x%x\n", szDbBusRxData[1]);
	DBG("szDbBusRxData[2] = 0x%x\n", szDbBusRxData[2]);
	DBG("szDbBusRxData[3] = 0x%x\n", szDbBusRxData[3]);

	*pMajor = (szDbBusRxData[1]<<8) + szDbBusRxData[0];
	*pMinor = (szDbBusRxData[3]<<8) + szDbBusRxData[2];

	DBG("*** major = %d ***\n", *pMajor);
	DBG("*** minor = %d ***\n", *pMinor);

	if (*ppVersion == NULL) {
		*ppVersion = kzalloc(sizeof(u8)*6, GFP_KERNEL);
	}

	sprintf(*ppVersion, "%03d%03d", *pMajor, *pMinor);
}

void DrvFwCtrlGetPlatformFirmwareVersion(u8 **ppVersion)
{
	DBG("*** %s() ***\n", __func__);

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		u32 i;
		u16 nRegData = 0;
		u8 szDbBusTxData[5] = {0};
		u8 szDbBusRxData[16] = {0};

		mutex_lock(&g_Mutex);

		DrvPlatformLyrTouchDeviceResetHw();

		DbBusEnterSerialDebugMode();
		DbBusStopMCU();
		DbBusIICUseBus();
		DbBusIICReshape();
		mdelay(100);


		RegSetLByteValue(0x0FE6, 0x01);


		RegSet16BitValue(0x3C60, 0xAA55);


		RegSet16BitValue(0x3CE4, 0xA4AB);


		RegSet16BitValue(0x1E04, 0x7d60);
		RegSet16BitValue(0x1E04, 0x829F);


		RegSetLByteValue(0x0FE6, 0x00);

		mdelay(100);


		do {
			nRegData = RegGet16BitValue(0x3CE4);
		} while (nRegData != 0x5B58);


		szDbBusTxData[0] = 0x72;
		szDbBusTxData[3] = 0x00;
		szDbBusTxData[4] = 0x08;

		for (i = 0; i < 2; i++) {
			szDbBusTxData[1] = 0x80;
			szDbBusTxData[2] = 0x10 + ((i*8)&0x00ff);

			IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 5);

			mdelay(50);

			IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[i*8], 8);
		}

		if (*ppVersion == NULL) {
			*ppVersion = kzalloc(sizeof(u8)*16, GFP_KERNEL);
		}

		sprintf(*ppVersion, "%.16s", szDbBusRxData);

		DbBusIICNotUseBus();
		DbBusNotStopMCU();
		DbBusExitSerialDebugMode();

		DrvPlatformLyrTouchDeviceResetHw();
		mdelay(100);

		mutex_unlock(&g_Mutex);
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		u8 szDbBusTxData[1] = {0};
		u8 szDbBusRxData[10] = {0};

		szDbBusTxData[0] = 0x04;

		mutex_lock(&g_Mutex);

		DrvPlatformLyrTouchDeviceResetHw();

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
		DmaReset();
#endif
#endif

		IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 1);
		IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 10);

		mutex_unlock(&g_Mutex);

		DBG("szDbBusRxData[0] = 0x%x , %c \n", szDbBusRxData[0], szDbBusRxData[0]);
		DBG("szDbBusRxData[1] = 0x%x , %c \n", szDbBusRxData[1], szDbBusRxData[1]);
		DBG("szDbBusRxData[2] = 0x%x , %c \n", szDbBusRxData[2], szDbBusRxData[2]);
		DBG("szDbBusRxData[3] = 0x%x , %c \n", szDbBusRxData[3], szDbBusRxData[3]);
		DBG("szDbBusRxData[4] = 0x%x , %c \n", szDbBusRxData[4], szDbBusRxData[4]);
		DBG("szDbBusRxData[5] = 0x%x , %c \n", szDbBusRxData[5], szDbBusRxData[5]);
		DBG("szDbBusRxData[6] = 0x%x , %c \n", szDbBusRxData[6], szDbBusRxData[6]);
		DBG("szDbBusRxData[7] = 0x%x , %c \n", szDbBusRxData[7], szDbBusRxData[7]);
		DBG("szDbBusRxData[8] = 0x%x , %c \n", szDbBusRxData[8], szDbBusRxData[8]);
		DBG("szDbBusRxData[9] = 0x%x , %c \n", szDbBusRxData[9], szDbBusRxData[9]);

		if (*ppVersion == NULL) {
			*ppVersion = kzalloc(sizeof(u8)*10, GFP_KERNEL);
		}

		sprintf(*ppVersion, "%.10s", szDbBusRxData);
	}

	DBG("*** platform firmware version = %s ***\n", *ppVersion);
}

s32 DrvFwCtrlUpdateFirmware(u8 szFwData[][1024], EmemType_e eEmemType)
{
	DBG("*** %s() ***\n", __func__);

	return _DrvFwCtrlUpdateFirmwareCash(szFwData, eEmemType);
}

s32 DrvFwCtrlUpdateFirmwareBySdCard(const char *pFilePath)
{
	s32 nRetVal = -1;

	DBG("*** %s() ***\n", __func__);

	if (g_ChipType == CHIP_TYPE_MSG26XXM || g_ChipType == CHIP_TYPE_MSG28XX) {
		nRetVal = _DrvFwCtrlUpdateFirmwareBySdCard(pFilePath);
	} else {
		DBG("This chip type (0x%x) does not support update firmware by sd card\n", g_ChipType);
	}

	return nRetVal;
}



u16 DrvFwCtrlGetFirmwareMode(void)
{
	u16 nMode = 0;

	DBG("*** %s() ***\n", __func__);

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		DbBusEnterSerialDebugMode();
		DbBusStopMCU();
		DbBusIICUseBus();
		DbBusIICReshape();
		mdelay(100);

		nMode = RegGet16BitValue(0x3CF4);

		DBG("firmware mode = 0x%x\n", nMode);

		DbBusIICNotUseBus();
		DbBusNotStopMCU();
		DbBusExitSerialDebugMode();
		mdelay(100);
	}

	return nMode;
}

u16 DrvFwCtrlChangeFirmwareMode(u16 nMode)
{
	DBG("*** %s() *** nMode = 0x%x\n", __func__, nMode);

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		DrvPlatformLyrTouchDeviceResetHw();

		DbBusEnterSerialDebugMode();
		DbBusStopMCU();
		DbBusIICUseBus();
		DbBusIICReshape();

		RegSet16BitValue(0x3CF4, nMode);
		nMode = RegGet16BitValue(0x3CF4);

		DBG("firmware mode = 0x%x\n", nMode);

		DbBusIICNotUseBus();
		DbBusNotStopMCU();
		DbBusExitSerialDebugMode();
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		u8 szDbBusTxData[2] = {0};
		u32 i = 0;
		s32 rc;

		_gIsDisableFinagerTouch = 1;

		szDbBusTxData[0] = 0x02;
		szDbBusTxData[1] = (u8)nMode;

		mutex_lock(&g_Mutex);
		DBG("*** %s() *** mutex_lock(&g_Mutex)\n", __func__);

		while (i < 5) {
			mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
			rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 2);
			if (rc > 0) {
				DBG("Change firmware mode success\n");
				break;
			}

			i++;
		}
		if (i == 5) {
			DBG("Change firmware mode failed, rc = %d\n", rc);
		}

		DBG("*** %s() *** mutex_unlock(&g_Mutex)\n", __func__);
		mutex_unlock(&g_Mutex);

		_gIsDisableFinagerTouch = 0;
	}

	return nMode;
}

void DrvFwCtrlGetFirmwareInfo(FirmwareInfo_t *pInfo)
{
	DBG("*** %s() ***\n", __func__);

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		u8 szDbBusTxData[3] = {0};
		u8 szDbBusRxData[8] = {0};
		u32 i = 0;
		s32 rc;

		_gIsDisableFinagerTouch = 1;

		szDbBusTxData[0] = 0x53;
		szDbBusTxData[1] = 0x00;
		szDbBusTxData[2] = 0x48;

		mutex_lock(&g_Mutex);

		while (i < 5) {
			mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
			rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 3);
			if (rc > 0) {
				DBG("Get firmware info IicWriteData() success\n");
			}

			mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
			rc = IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 8);
			if (rc > 0) {
				DBG("Get firmware info IicReadData() success\n");
				break;
			}

			i++;
		}
		if (i == 5) {
			DBG("Get firmware info failed, rc = %d\n", rc);
		}

		mutex_unlock(&g_Mutex);

		if (szDbBusRxData[0] == 8 && szDbBusRxData[1] == 0 && szDbBusRxData[2] == 9 && szDbBusRxData[3] == 0) {
			DBG("*** Debug Mode Packet Header is 0xA5 ***\n");
			pInfo->nLogModePacketHeader = 0xA5;
			pInfo->nMy = 0;
			pInfo->nMx = 0;

		} else if (szDbBusRxData[0] == 0xAB) {
			DBG("*** Debug Mode Packet Header is 0xAB ***\n");

			pInfo->nLogModePacketHeader = szDbBusRxData[0];
			pInfo->nMy = szDbBusRxData[1];
			pInfo->nMx = szDbBusRxData[2];


		} else if (szDbBusRxData[0] == 0xA7 && szDbBusRxData[3] == PACKET_TYPE_TOOTH_PATTERN) {
			DBG("*** Debug Packet Header is 0xA7 ***\n");

			pInfo->nLogModePacketHeader = szDbBusRxData[0];
			pInfo->nType = szDbBusRxData[3];
			pInfo->nMy = szDbBusRxData[4];
			pInfo->nMx = szDbBusRxData[5];
			pInfo->nSd = szDbBusRxData[6];
			pInfo->nSs = szDbBusRxData[7];
		}

		if (pInfo->nLogModePacketHeader == 0xA5) {
			if (pInfo->nMy != 0 && pInfo->nMx != 0) {

				pInfo->nLogModePacketLength = 1+1+1+1+10*3+pInfo->nMx*pInfo->nMy*2+1;
			} else {
				DBG("Failed to retrieve channel number or subframe number for debug mode packet 0xA5.\n");
			}
		} else if (pInfo->nLogModePacketHeader == 0xAB) {
			if (pInfo->nMy != 0 && pInfo->nMx != 0) {

				pInfo->nLogModePacketLength = 1+1+1+1+10*3+pInfo->nMy*pInfo->nMx*2+pInfo->nMy*2+pInfo->nMx*2+2*2+8*2+1;
			} else {
				DBG("Failed to retrieve channel number or subframe number for debug mode packet 0xAB.\n");
			}
		} else if (pInfo->nLogModePacketHeader == 0xA7 && pInfo->nType == PACKET_TYPE_TOOTH_PATTERN) {
			if (pInfo->nMy != 0 && pInfo->nMx != 0 && pInfo->nSd != 0 && pInfo->nSs != 0) {

				pInfo->nLogModePacketLength = 1+1+1+1+1+10*3+pInfo->nMy*pInfo->nMx*2+pInfo->nSd*2+pInfo->nSs*2+10*2+1;
			} else {
				DBG("Failed to retrieve channel number or subframe number for debug mode packet 0xA7.\n");
			}
		} else {
			DBG("Undefined debug mode packet header = 0x%x\n", pInfo->nLogModePacketHeader);
		}

		DBG("*** debug mode packet header = 0x%x ***\n", pInfo->nLogModePacketHeader);
		DBG("*** debug mode packet length = %d ***\n", pInfo->nLogModePacketLength);
		DBG("*** Type = 0x%x ***\n", pInfo->nType);
		DBG("*** My = %d ***\n", pInfo->nMy);
		DBG("*** Mx = %d ***\n", pInfo->nMx);
		DBG("*** Sd = %d ***\n", pInfo->nSd);
		DBG("*** Ss = %d ***\n", pInfo->nSs);

		_gIsDisableFinagerTouch = 0;
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		u8 szDbBusTxData[1] = {0};
		u8 szDbBusRxData[10] = {0};
		u32 i = 0;
		s32 rc;

		_gIsDisableFinagerTouch = 1;

		szDbBusTxData[0] = 0x01;

		mutex_lock(&g_Mutex);
		DBG("*** %s() *** mutex_lock(&g_Mutex)\n", __func__);

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
		DmaReset();
#endif
#endif

		while (i < 5) {
			mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
			rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 1);
			if (rc > 0) {
				DBG("Get firmware info IicWriteData() success\n");
			}

			mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
			rc = IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 10);
			if (rc > 0) {
				DBG("Get firmware info IicReadData() success\n");

				if (szDbBusRxData[1] == FIRMWARE_MODE_DEMO_MODE || szDbBusRxData[1] == FIRMWARE_MODE_DEBUG_MODE) {
					break;
				} else {
					i = 0;
				}
			}

			i++;
		}
		if (i == 5) {
			DBG("Get firmware info failed, rc = %d\n", rc);
		}

		DBG("*** %s() *** mutex_unlock(&g_Mutex)\n", __func__);
		mutex_unlock(&g_Mutex);


		if ((szDbBusRxData[1] == FIRMWARE_MODE_DEBUG_MODE && szDbBusRxData[2] == 0xA7 && szDbBusRxData[5] == PACKET_TYPE_TOOTH_PATTERN) || (szDbBusRxData[1] == FIRMWARE_MODE_DEMO_MODE && szDbBusRxData[2] == 0x5A)) {
			pInfo->nFirmwareMode = szDbBusRxData[1];
			DBG("pInfo->nFirmwareMode = 0x%x\n", pInfo->nFirmwareMode);

			pInfo->nLogModePacketHeader = szDbBusRxData[2];
			pInfo->nLogModePacketLength = (szDbBusRxData[3]<<8) + szDbBusRxData[4];
			pInfo->nType = szDbBusRxData[5];
			pInfo->nMy = szDbBusRxData[6];
			pInfo->nMx = szDbBusRxData[7];
			pInfo->nSd = szDbBusRxData[8];
			pInfo->nSs = szDbBusRxData[9];

			DBG("pInfo->nLogModePacketHeader = 0x%x\n", pInfo->nLogModePacketHeader);
			DBG("pInfo->nLogModePacketLength = %d\n", pInfo->nLogModePacketLength);
			DBG("pInfo->nType = 0x%x\n", pInfo->nType);
			DBG("pInfo->nMy = %d\n", pInfo->nMy);
			DBG("pInfo->nMx = %d\n", pInfo->nMx);
			DBG("pInfo->nSd = %d\n", pInfo->nSd);
			DBG("pInfo->nSs = %d\n", pInfo->nSs);
		} else {
			DBG("Firmware info before correcting :\n");

			DBG("FirmwareMode = 0x%x\n", szDbBusRxData[1]);
			DBG("LogModePacketHeader = 0x%x\n", szDbBusRxData[2]);
			DBG("LogModePacketLength = %d\n", (szDbBusRxData[3]<<8) + szDbBusRxData[4]);
			DBG("Type = 0x%x\n", szDbBusRxData[5]);
			DBG("My = %d\n", szDbBusRxData[6]);
			DBG("Mx = %d\n", szDbBusRxData[7]);
			DBG("Sd = %d\n", szDbBusRxData[8]);
			DBG("Ss = %d\n", szDbBusRxData[9]);


			pInfo->nFirmwareMode = FIRMWARE_MODE_DEMO_MODE;
			pInfo->nLogModePacketHeader = 0x5A;
			pInfo->nLogModePacketLength = DEMO_MODE_PACKET_LENGTH;
			pInfo->nType = 0;
			pInfo->nMy = 0;
			pInfo->nMx = 0;
			pInfo->nSd = 0;
			pInfo->nSs = 0;

			DBG("Firmware info after correcting :\n");

			DBG("pInfo->nFirmwareMode = 0x%x\n", pInfo->nFirmwareMode);
			DBG("pInfo->nLogModePacketHeader = 0x%x\n", pInfo->nLogModePacketHeader);
			DBG("pInfo->nLogModePacketLength = %d\n", pInfo->nLogModePacketLength);
			DBG("pInfo->nType = 0x%x\n", pInfo->nType);
			DBG("pInfo->nMy = %d\n", pInfo->nMy);
			DBG("pInfo->nMx = %d\n", pInfo->nMx);
			DBG("pInfo->nSd = %d\n", pInfo->nSd);
			DBG("pInfo->nSs = %d\n", pInfo->nSs);
		}

		_gIsDisableFinagerTouch = 0;
	}
}

void DrvFwCtrlRestoreFirmwareModeToLogDataMode(void)
{
	DBG("*** %s() g_IsSwitchModeByAPK = %d ***\n", __func__, g_IsSwitchModeByAPK);

	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		if (g_IsSwitchModeByAPK == 1) {
			DBG("g_FirmwareMode = 0x%x\n", g_FirmwareMode);


			if (g_FirmwareMode == FIRMWARE_MODE_DEBUG_MODE && FIRMWARE_MODE_DEMO_MODE == DrvFwCtrlGetFirmwareMode()) {
				g_FirmwareMode = DrvFwCtrlChangeFirmwareMode(FIRMWARE_MODE_DEBUG_MODE);
			} else {
				DBG("firmware mode is not restored\n");
			}
		}
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		if (g_IsSwitchModeByAPK == 1) {
			FirmwareInfo_t tInfo;

			memset(&tInfo, 0x0, sizeof(FirmwareInfo_t));

			DrvFwCtrlGetFirmwareInfo(&tInfo);

			DBG("g_FirmwareMode = 0x%x, tInfo.nFirmwareMode = 0x%x\n", g_FirmwareMode, tInfo.nFirmwareMode);


			if (g_FirmwareMode == FIRMWARE_MODE_DEBUG_MODE && FIRMWARE_MODE_DEBUG_MODE != tInfo.nFirmwareMode) {
				g_FirmwareMode = DrvFwCtrlChangeFirmwareMode(FIRMWARE_MODE_DEBUG_MODE);
			} else {
				DBG("firmware mode is not restored\n");
			}
		}
	}
}



#ifdef CONFIG_UPDATE_FIRMWARE_BY_SW_ID

void DrvFwCtrlCheckFirmwareUpdateBySwId(void)
{
	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		_DrvFwCtrlMsg26xxmCheckFirmwareUpdateBySwId();
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		_DrvFwCtrlMsg28xxCheckFirmwareUpdateBySwId();
	} else {
		DBG("This chip type (0x%x) does not support update firmware by sw id\n", g_ChipType);
	}
}

#endif



#ifdef CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA
void DrvFwCtrlGetTouchPacketAddress(u16 *pDataAddress, u16 *pFlagAddress)
{
	s32 rc = 0;
	u32 i = 0;
	u8 szDbBusTxData[1] = {0};
	u8 szDbBusRxData[4] = {0};

	DBG("*** %s() ***\n", __func__);

	szDbBusTxData[0] = 0x05;

	mutex_lock(&g_Mutex);
	DBG("*** %s() *** mutex_lock(&g_Mutex)\n", __func__);

	while (i < 5) {
		mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
		rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 1);
		if (rc > 0) {
			DBG("Get touch packet address IicWriteData() success\n");
		}

		mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
		rc = IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 4);
		if (rc > 0) {
			DBG("Get touch packet address IicReadData() success\n");
			break;
		}

		i++;
	}
	if (i == 5) {
		DBG("Get touch packet address failed, rc = %d\n", rc);
	}

	if (rc < 0) {
		g_FwSupportSegment = 0;
	} else {
		*pDataAddress = (szDbBusRxData[0]<<8) + szDbBusRxData[1];
		*pFlagAddress = (szDbBusRxData[2]<<8) + szDbBusRxData[3];

		g_FwSupportSegment = 1;

		DBG("*** *pDataAddress = 0x%2X ***\n", *pDataAddress);
		DBG("*** *pFlagAddress = 0x%2X ***\n", *pFlagAddress);
	}

	DBG("*** %s() *** mutex_unlock(&g_Mutex)\n", __func__);
	mutex_unlock(&g_Mutex);
}

static int _DrvFwCtrlCheckFingerTouchPacketFlagBit1(void)
{
	u8 szDbBusTxData[3] = {0};
	s32 nRetVal;

	DBG("*** %s() ***\n", __func__);

	szDbBusTxData[0] = 0x53;
	szDbBusTxData[1] = (g_FwPacketFlagAddress >> 8) & 0xFF;
	szDbBusTxData[2] = g_FwPacketFlagAddress & 0xFF;

	IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 3);
	IicReadData(SLAVE_I2C_ID_DWI2C, &_gTouchPacketFlag[0], 2);

	if ((_gTouchPacketFlag[0] & BIT1) == 0x00) {
		nRetVal = 0;
	} else {
		nRetVal = 1;
	}
	DBG("Bit1 = %d\n", nRetVal);

	return nRetVal;
}

static void _DrvFwCtrlResetFingerTouchPacketFlagBit1(void)
{
	u8 szDbBusTxData[4] = {0};

	DBG("*** %s() ***\n", __func__);

	szDbBusTxData[0] = 0x52;
	szDbBusTxData[1] = (g_FwPacketFlagAddress >> 8) & 0xFF;
	szDbBusTxData[2] = g_FwPacketFlagAddress & 0xFF;
	szDbBusTxData[3] = _gTouchPacketFlag[0] | BIT1;

	IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 4);
}
#endif



#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION

s32 DrvFwCtrlEnableProximity(void)
{
	u8 szDbBusTxData[4] = {0};
	s32 rc;

	DBG("*** %s() ***\n", __func__);

	szDbBusTxData[0] = 0x52;
	szDbBusTxData[1] = 0x00;

	if (g_ChipType == CHIP_TYPE_MSG26XXM || g_ChipType == CHIP_TYPE_MSG28XX) {
		szDbBusTxData[2] = 0x47;
	} else {
		DBG("*** Un-recognized chip type = 0x%x ***\n", g_ChipType);
		return -EPERM;
	}

	szDbBusTxData[3] = 0xa0;

	mutex_lock(&g_Mutex);

	mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
	rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 4);
	if (rc > 0) {
		g_EnableTpProximity = 1;

		DBG("Enable proximity detection success\n");
	} else {
		DBG("Enable proximity detection failed\n");
	}

	mutex_unlock(&g_Mutex);

	return rc;
}

s32 DrvFwCtrlDisableProximity(void)
{
	u8 szDbBusTxData[4] = {0};
	s32 rc;

	DBG("*** %s() ***\n", __func__);

	szDbBusTxData[0] = 0x52;
	szDbBusTxData[1] = 0x00;

	if (g_ChipType == CHIP_TYPE_MSG26XXM || g_ChipType == CHIP_TYPE_MSG28XX) {
		szDbBusTxData[2] = 0x47;
	} else {
		DBG("*** Un-recognized chip type = 0x%x ***\n", g_ChipType);
		return -EPERM;
	}

	szDbBusTxData[3] = 0xa1;

	mutex_lock(&g_Mutex);

	mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
	rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 4);
	if (rc > 0) {
		g_EnableTpProximity = 0;

		DBG("Disable proximity detection success\n");
	} else {
		DBG("Disable proximity detection failed\n");
	}

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	g_FaceClosingTp = 0;
#endif

	mutex_unlock(&g_Mutex);

	return rc;
}

#endif



#ifdef CONFIG_ENABLE_GLOVE_MODE
void DrvIcFwCtrlOpenGloveMode(void)
{
	s32 rc = 0;
	u8 szDbBusTxData[3] = {0};
	u32 i = 0;

	DBG("*** %s() ***\n", __func__);

	_gIsDisableFinagerTouch = 1;

	szDbBusTxData[0] = 0x06;
	szDbBusTxData[1] = 0x01;
	szDbBusTxData[2] = 0x01;

	mutex_lock(&g_Mutex);

	while (i < 5) {
	   mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
	   rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 3);
	   if (rc > 0) {
		   DBG("Open glove mode success\n");
		   break;
	  }

	   i++;
	}
	if (i == 5) {
		DBG("Open glove mode failed, rc = %d\n", rc);
	}

	mutex_unlock(&g_Mutex);

	_gIsDisableFinagerTouch = 0;
}

void DrvIcFwCtrlCloseGloveMode(void)
{
	s32 rc = 0;
	u8 szDbBusTxData[3] = {0};
	u32 i = 0;

	DBG("*** %s() ***\n", __func__);

	_gIsDisableFinagerTouch = 1;

	szDbBusTxData[0] = 0x06;
	szDbBusTxData[1] = 0x01;
	szDbBusTxData[2] = 0x00;

	mutex_lock(&g_Mutex);

	while (i < 5) {
	   mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
	   rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 3);
	   if (rc > 0) {
		   DBG("Close glove mode success\n");
		   break;
	  }

	   i++;
	}
	if (i == 5) {
		DBG("Close glove mode failed, rc = %d\n", rc);
	}

	mutex_unlock(&g_Mutex);

	_gIsDisableFinagerTouch = 0;
}

void DrvIcFwCtrlGetGloveInfo(u8 *pGloveMode)
{
	u8 szDbBusTxData[3] = {0};
	u8 szDbBusRxData[2] = {0};
	u32 i = 0;
	s32 rc;

	_gIsDisableFinagerTouch = 1;

	szDbBusTxData[0] = 0x06;
	szDbBusTxData[1] = 0x01;
	szDbBusTxData[2] = 0x02;

	mutex_lock(&g_Mutex);

	while (i < 5) {
		mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
		rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 3);
		if (rc > 0) {
			DBG("Get glove info IicWriteData() success\n");
		}

		mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
		rc = IicReadData(SLAVE_I2C_ID_DWI2C, &szDbBusRxData[0], 1);
		if (rc > 0) {
			DBG("Get glove info IicReadData() success\n");

			if (szDbBusRxData[0] == 0x00 || szDbBusRxData[0] == 0x01) {
				break;
			} else {
				i = 0;
			}
		}

		i++;
	}
	if (i == 5) {
		DBG("Get glove info failed, rc = %d\n", rc);
	}

	mutex_unlock(&g_Mutex);

	*pGloveMode = szDbBusRxData[0];

	DBG("pGloveMode = 0x%x\n", *pGloveMode);

	_gIsDisableFinagerTouch = 0;
}

#endif


#ifdef CONFIG_ENABLE_CHARGER_DETECTION

void DrvFwCtrlChargerDetection(u8 nChargerStatus)
{
	u32 i = 0;
	u8 szDbBusTxData[2] = {0};
	s32 rc = 0;

	DBG("*** %s() ***\n", __func__);

	DBG("_gChargerPlugIn = %d, nChargerStatus = %d, g_ForceUpdate = %d\n", _gChargerPlugIn, nChargerStatus, g_ForceUpdate);

	mutex_lock(&g_Mutex);

	szDbBusTxData[0] = 0x09;

	if (nChargerStatus) {
		if (_gChargerPlugIn == 0 || g_ForceUpdate == 1) {
			szDbBusTxData[1] = 0xA5;

			while (i < 5) {
				mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
				rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 2);
				if (rc > 0) {
					_gChargerPlugIn = 1;

					DBG("Update status for charger plug in success.\n");
					break;
				}

				i++;
			}
			if (i == 5) {
				DBG("Update status for charger plug in failed, rc = %d\n", rc);
			}

			g_ForceUpdate = 0;
		}
	} else {
		if (_gChargerPlugIn == 1 || g_ForceUpdate == 1) {
			szDbBusTxData[1] = 0x5A;

			while (i < 5) {
				mdelay(I2C_WRITE_COMMAND_DELAY_FOR_FIRMWARE);
				rc = IicWriteData(SLAVE_I2C_ID_DWI2C, &szDbBusTxData[0], 2);
				if (rc > 0) {
					_gChargerPlugIn = 0;

					DBG("Update status for charger plug out success.\n");
					break;
				}

				i++;
			}
			if (i == 5) {
				DBG("Update status for charger plug out failed, rc = %d\n", rc);
			}

			g_ForceUpdate = 0;
		}
	}

	mutex_unlock(&g_Mutex);
}

#endif



static s32 _DrvFwCtrlReadFingerTouchData(u8 *pPacket, u16 nReportPacketLength)
{
	s32 rc;

	if (IS_FIRMWARE_DATA_LOG_ENABLED) {
		if (g_FirmwareMode == FIRMWARE_MODE_DEMO_MODE) {
			rc = IicReadData(SLAVE_I2C_ID_DWI2C, &pPacket[0], nReportPacketLength);
			if (rc < 0) {
				DBG("I2C read packet data failed, rc = %d\n", rc);
				return -EPERM;
			}
		} else if (g_FirmwareMode == FIRMWARE_MODE_DEBUG_MODE) {
#ifdef CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA
			DBG("*** g_FwPacketDataAddress = 0x%2X ***\n", g_FwPacketDataAddress);
			DBG("*** g_FwPacketFlagAddress = 0x%2X ***\n", g_FwPacketFlagAddress);

			if (g_FwSupportSegment == 0) {
				DBG("g_FwPacketDataAddress & g_FwPacketFlagAddress is un-initialized\n");
				return -EPERM;
			}

			if (_gIsDisableFinagerTouch == 1) {
				DBG("Skip finger touch for handling get firmware info or change firmware mode\n");
				return -EPERM;
			}

			rc = IicSegmentReadDataBySmBus(g_FwPacketDataAddress, &pPacket[0], nReportPacketLength, MAX_I2C_TRANSACTION_LENGTH_LIMIT);

			_DrvFwCtrlCheckFingerTouchPacketFlagBit1();
			_DrvFwCtrlResetFingerTouchPacketFlagBit1();

			if (rc < 0) {
				DBG("I2C read debug mode packet data failed, rc = %d\n", rc);
				return -EPERM;
			}

#else
			rc = IicReadData(SLAVE_I2C_ID_DWI2C, &pPacket[0], nReportPacketLength);
			if (rc < 0) {
				DBG("I2C read packet data failed, rc = %d\n", rc);
				return -EPERM;
			}
#endif
		} else {
			DBG("WRONG FIRMWARE MODE : 0x%x\n", g_FirmwareMode);
			return -EPERM;
		}
	} else {
		rc = IicReadData(SLAVE_I2C_ID_DWI2C, &pPacket[0], nReportPacketLength);
		if (rc < 0) {
			DBG("I2C read packet data failed, rc = %d\n", rc);
			return -EPERM;
		}
	}

	return 0;
}



void DrvFwCtrlHandleFingerTouch(void)
{
	TouchInfo_t tInfo;
	u32 i = 0;
	static u32 nLastKeyCode = 0xFF;
	static u32 nLastCount;
	u8 *pPacket = NULL;
	u16 nReportPacketLength = 0;



	if (_gIsDisableFinagerTouch == 1) {
		DBG("Skip finger touch for handling get firmware info or change firmware mode\n");
		return;
	}

	mutex_lock(&g_Mutex);
	DBG("*** %s() *** mutex_lock(&g_Mutex)\n", __func__);

	memset(&tInfo, 0x0, sizeof(TouchInfo_t));

	if (IS_FIRMWARE_DATA_LOG_ENABLED) {
		if (g_FirmwareMode == FIRMWARE_MODE_DEMO_MODE) {
#ifdef CONFIG_ENABLE_HOTKNOT
			if (g_HotKnotState == HOTKNOT_TRANS_STATE) {
				nReportPacketLength = DEMO_HOTKNOT_SEND_RET_LEN;
				pPacket = g_DemoModeHotKnotSndRetPacket;
			} else {
#endif
				DBG("FIRMWARE_MODE_DEMO_MODE\n");

				nReportPacketLength = DEMO_MODE_PACKET_LENGTH;
				pPacket = g_DemoModePacket;
			}
		} else if (g_FirmwareMode == FIRMWARE_MODE_DEBUG_MODE) {
			DBG("FIRMWARE_MODE_DEBUG_MODE\n");

			if (g_FirmwareInfo.nLogModePacketHeader != 0xA5 && g_FirmwareInfo.nLogModePacketHeader != 0xAB && g_FirmwareInfo.nLogModePacketHeader != 0xA7) {
				DBG("WRONG DEBUG MODE HEADER : 0x%x\n", g_FirmwareInfo.nLogModePacketHeader);
				goto TouchHandleEnd;
			}

#ifdef CONFIG_ENABLE_HOTKNOT
			if (g_HotKnotState == HOTKNOT_TRANS_STATE) {
				nReportPacketLength = DEBUG_HOTKNOT_SEND_RET_LEN;
				pPacket = g_DebugModeHotKnotSndRetPacket;
			} else {
#endif
				nReportPacketLength = g_FirmwareInfo.nLogModePacketLength;
				pPacket = g_LogModePacket;
			}
		} else {
			DBG("WRONG FIRMWARE MODE : 0x%x\n", g_FirmwareMode);
			goto TouchHandleEnd;
		}
	} else {
#ifdef CONFIG_ENABLE_HOTKNOT
		if (g_HotKnotState == HOTKNOT_TRANS_STATE) {
			nReportPacketLength = DEMO_HOTKNOT_SEND_RET_LEN;
			pPacket = g_DemoModeHotKnotSndRetPacket;
		} else {
#endif
			DBG("FIRMWARE_MODE_DEMO_MODE\n");

			nReportPacketLength = DEMO_MODE_PACKET_LENGTH;
			pPacket = g_DemoModePacket;
		}
	}

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
	if (g_GestureDebugMode == 1 && g_GestureWakeupFlag == 1) {
		DBG("Set gesture debug mode packet length, g_ChipType=%d\n", g_ChipType);

		if (g_ChipType == CHIP_TYPE_MSG26XXM || g_ChipType == CHIP_TYPE_MSG28XX) {
			nReportPacketLength = GESTURE_DEBUG_MODE_PACKET_LENGTH;
			pPacket = _gGestureWakeupPacket;
		} else {
			DBG("This chip type does not support gesture debug mode.\n");
			goto TouchHandleEnd;
		}
	} else if (g_GestureWakeupFlag == 1) {
		DBG("Set gesture wakeup packet length\n");

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
		nReportPacketLength = GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH;
#else
		nReportPacketLength = GESTURE_WAKEUP_PACKET_LENGTH;
#endif

		pPacket = _gGestureWakeupPacket;
	}

#else

	if (g_GestureWakeupFlag == 1) {
		DBG("Set gesture wakeup packet length\n");

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
		nReportPacketLength = GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH;
#else
		nReportPacketLength = GESTURE_WAKEUP_PACKET_LENGTH;
#endif

		pPacket = _gGestureWakeupPacket;
	}
#endif

#endif

#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM)
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	if (g_GestureWakeupFlag == 1) {
		s32 rc;

		while (i < 5) {
			mdelay(50);

			rc = IicReadData(SLAVE_I2C_ID_DWI2C, &pPacket[0], nReportPacketLength);
			if (rc > 0) {
				break;
			}

			i++;
		}
		if (i == 5) {
			DBG("I2C read packet data failed, rc = %d\n", rc);
			goto TouchHandleEnd;
		}
	} else {
		if (0 != _DrvFwCtrlReadFingerTouchData(&pPacket[0], nReportPacketLength)) {
			goto TouchHandleEnd;
		}
	}
#else
	if (0 != _DrvFwCtrlReadFingerTouchData(&pPacket[0], nReportPacketLength)) {
		 goto TouchHandleEnd;
	}
#endif
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
	if (0 != _DrvFwCtrlReadFingerTouchData(&pPacket[0], nReportPacketLength)) {
		goto TouchHandleEnd;
	}
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	{
		s32 nRetVal = 0;

#ifdef CONFIG_ENABLE_DMA_IIC
		DmaReset();
#endif

		nRetVal = _DrvFwCtrlReadFingerTouchData(&pPacket[0], nReportPacketLength);
		if (0 != nRetVal) {
			goto TouchHandleEnd;
		}
	}
#endif

	if (0 == _DrvFwCtrlParsePacket(pPacket, nReportPacketLength, &tInfo)) {
#ifdef CONFIG_TP_HAVE_KEY
		if (tInfo.nKeyCode != 0xFF) {
			DBG("tInfo.nKeyCode=%x, nLastKeyCode=%x, g_TpVirtualKey[%d]=%d\n", tInfo.nKeyCode, nLastKeyCode, tInfo.nKeyCode, g_TpVirtualKey[tInfo.nKeyCode]);

			if (tInfo.nKeyCode < MAX_KEY_NUM) {
				if (tInfo.nKeyCode != nLastKeyCode) {
					DBG("key touch pressed\n");

					input_report_key(g_InputDevice, BTN_TOUCH, 1);
					input_report_key(g_InputDevice, g_TpVirtualKey[tInfo.nKeyCode], 1);

					input_sync(g_InputDevice);

					nLastKeyCode = tInfo.nKeyCode;
				} else {
					/
					DBG("REPEATED KEY\n");
				}
			} else {
				DBG("WRONG KEY\n");
			}
		} else {
			if (nLastKeyCode != 0xFF) {
				DBG("key touch released\n");

				input_report_key(g_InputDevice, BTN_TOUCH, 0);
				input_report_key(g_InputDevice, g_TpVirtualKey[nLastKeyCode], 0);

				input_sync(g_InputDevice);

				nLastKeyCode = 0xFF;
			}
		}
#endif

		DBG("tInfo.nCount = %d, nLastCount = %d\n", tInfo.nCount, nLastCount);

		if (tInfo.nCount > 0) {
			for (i = 0; i < tInfo.nCount; i++) {
				DrvPlatformLyrFingerTouchPressed(tInfo.tPoint[i].nX, tInfo.tPoint[i].nY, tInfo.tPoint[i].nP, tInfo.tPoint[i].nId);
			}

#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
			for (i = 0; i < MAX_TOUCH_NUM; i++) {
				DBG("_gPreviousTouch[%d]=%d, _gCurrentTouch[%d]=%d\n", i, _gPreviousTouch[i], i, _gCurrentTouch[i]);

				if (_gCurrentTouch[i] == 0 && _gPreviousTouch[i] == 1) {
					DrvPlatformLyrFingerTouchReleased(0, 0, i);
				}
				_gPreviousTouch[i] = _gCurrentTouch[i];
			}
			input_mt_sync_frame(g_InputDevice);
#endif

			input_sync(g_InputDevice);

			nLastCount = tInfo.nCount;
		} else {
			if (nLastCount > 0) {
#ifdef CONFIG_ENABLE_TYPE_B_PROTOCOL
				for (i = 0; i < MAX_TOUCH_NUM; i++) {
					DBG("_gPreviousTouch[%d]=%d, _gCurrentTouch[%d]=%d\n", i, _gPreviousTouch[i], i, _gCurrentTouch[i]);

					if (_gCurrentTouch[i] == 0 && _gPreviousTouch[i] == 1) {
						DrvPlatformLyrFingerTouchReleased(0, 0, i);
					}
					_gPreviousTouch[i] = _gCurrentTouch[i];
				}
				input_mt_sync_frame(g_InputDevice);
#else
				DrvPlatformLyrFingerTouchReleased(0, 0, 0);
#endif

				input_sync(g_InputDevice);

				nLastCount = 0;
			}
		}

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
		if (g_IsEnableReportRate == 1) {
			if (g_ValidTouchCount == 4294967296) {
				g_ValidTouchCount = 0;
				DBG("g_ValidTouchCount reset to 0\n");
			}

			g_ValidTouchCount++;

			DBG("g_ValidTouchCount = %d\n", g_ValidTouchCount);
		}
#endif
	}

	TouchHandleEnd:

	DBG("*** %s() *** mutex_unlock(&g_Mutex)\n", __func__);
	mutex_unlock(&g_Mutex);
}

#endif
