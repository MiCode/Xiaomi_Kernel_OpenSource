/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * @file	mstar_drv_main.c
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */


#include "mstar_drv_main.h"
#include "mstar_drv_utility_adaption.h"
#include "mstar_drv_platform_porting_layer.h"
#include "mstar_drv_ic_fw_porting_layer.h"

#ifdef CONFIG_ENABLE_HOTKNOT
#include "mstar_drv_hotknot.h"
#include "mstar_drv_hotknot_queue.h"
#endif

#ifdef CONFIG_ENABLE_JNI_INTERFACE
#include "mstar_drv_jni_interface.h"
#endif

extern FirmwareInfo_t g_FirmwareInfo;
extern u8 g_LogModePacket[DEBUG_MODE_PACKET_LENGTH];
extern u16 g_FirmwareMode;

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
extern u32 g_GestureWakeupMode[2];

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
extern u8 g_LogGestureDebug[128];
extern u8 g_GestureDebugFlag;
extern u8 g_GestureDebugMode;

extern struct input_dev *g_InputDevice;
#endif

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
extern u32 g_LogGestureInfor[GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH];
#endif
#endif

extern u8 g_ChipType;

#ifdef CONFIG_ENABLE_ITO_MP_TEST
#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
extern TestScopeInfo_t g_TestScopeInfo;
#endif
#endif

static u16 _gDebugReg[MAX_DEBUG_REGISTER_NUM] = {0};
static u16 _gDebugRegValue[MAX_DEBUG_REGISTER_NUM] = {0};
static u32 _gDebugRegCount;

static u8 _gDebugCmdArgu[MAX_DEBUG_COMMAND_ARGUMENT_NUM] = {0};
static u16 _gDebugCmdArguCount;
static u32 _gDebugReadDataSize;

static u8 *_gPlatformFwVersion;

#ifdef CONFIG_ENABLE_ITO_MP_TEST
static ItoTestMode_e _gItoTestMode;
#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
static u32 _gLogGestureCount;
static u8 _gLogGestureInforType;
#endif

#endif

static u32 _gIsUpdateComplete;

static u8 *_gFwVersion;

static u32 _gFeatureSupportStatus;

static struct proc_dir_entry *_gProcClassEntry;
static struct proc_dir_entry *_gProcMsTouchScreenMsg20xxEntry;
static struct proc_dir_entry *_gProcDeviceEntry;
static struct proc_dir_entry *_gProcChipTypeEntry;
static struct proc_dir_entry *_gProcFirmwareDataEntry;
static struct proc_dir_entry *_gProcApkFirmwareUpdateEntry;
static struct proc_dir_entry *_gProcCustomerFirmwareVersionEntry;
static struct proc_dir_entry *_gProcPlatformFirmwareVersionEntry;
static struct proc_dir_entry *_gProcDeviceDriverVersionEntry;
static struct proc_dir_entry *_gProcSdCardFirmwareUpdateEntry;
static struct proc_dir_entry *_gProcFirmwareDebugEntry;
static struct proc_dir_entry *_gProcFirmwareSetDebugValueEntry;
static struct proc_dir_entry *_gProcFirmwareSmBusDebugEntry;
static struct proc_dir_entry *_gProcFirmwareSetDQMemValueEntry;
#ifdef CONFIG_ENABLE_ITO_MP_TEST
static struct proc_dir_entry *_gProcMpTestEntry;
static struct proc_dir_entry *_gProcMpTestLogEntry;
static struct proc_dir_entry *_gProcMpTestFailChannelEntry;
static struct proc_dir_entry *_gProcMpTestScopeEntry;
#endif
static struct proc_dir_entry *_gProcFirmwareModeEntry;
static struct proc_dir_entry *_gProcFirmwareSensorEntry;
static struct proc_dir_entry *_gProcFirmwarePacketHeaderEntry;
static struct proc_dir_entry *_gProcQueryFeatureSupportStatusEntry;
static struct proc_dir_entry *_gProcChangeFeatureSupportStatusEntry;
#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
static struct proc_dir_entry *_gProcGestureWakeupModeEntry;
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
static struct proc_dir_entry *_gProcGestureDebugModeEntry;
#endif
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
static struct proc_dir_entry *_gProcGestureInforModeEntry;
#endif
#endif
#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
static struct proc_dir_entry *_gProcReportRateEntry;
#endif
#ifdef CONFIG_ENABLE_GLOVE_MODE
static struct proc_dir_entry *_gProcGloveModeEntry;
static struct proc_dir_entry *_gProcOpenGloveModeEntry;
static struct proc_dir_entry *_gProcCloseGloveModeEntry;
#endif
#ifdef CONFIG_ENABLE_JNI_INTERFACE
static struct proc_dir_entry *_gProcJniMethodEntry;
#endif
static struct proc_dir_entry *_gProcSeLinuxLimitFirmwareUpdateEntry;


static const struct file_operations _gProcChipType = {
	.read = DrvMainProcfsChipTypeRead,
	.write = DrvMainProcfsChipTypeWrite,
};

static const struct file_operations _gProcFirmwareData = {
	.read = DrvMainProcfsFirmwareDataRead,
	.write = DrvMainProcfsFirmwareDataWrite,
};

static const struct file_operations _gProcApkFirmwareUpdate = {
	.read = DrvMainProcfsFirmwareUpdateRead,
	.write = DrvMainProcfsFirmwareUpdateWrite,
};

static const struct file_operations _gProcCustomerFirmwareVersion = {
	.read = DrvMainProcfsCustomerFirmwareVersionRead,
	.write = DrvMainProcfsCustomerFirmwareVersionWrite,
};

static const struct file_operations _gProcPlatformFirmwareVersion = {
	.read = DrvMainProcfsPlatformFirmwareVersionRead,
	.write = DrvMainProcfsPlatformFirmwareVersionWrite,
};

static const struct file_operations _gProcDeviceDriverVersion = {
	.read = DrvMainProcfsDeviceDriverVersionRead,
	.write = DrvMainProcfsDeviceDriverVersionWrite,
};

static const struct file_operations _gProcSdCardFirmwareUpdate = {
	.read = DrvMainProcfsSdCardFirmwareUpdateRead,
	.write = DrvMainProcfsSdCardFirmwareUpdateWrite,
};

static const struct file_operations _gProcFirmwareDebug = {
	.read = DrvMainProcfsFirmwareDebugRead,
	.write = DrvMainProcfsFirmwareDebugWrite,
};

static const struct file_operations _gProcFirmwareSetDebugValue = {
	.read = DrvMainProcfsFirmwareSetDebugValueRead,
	.write = DrvMainProcfsFirmwareSetDebugValueWrite,
};

static const struct file_operations _gProcFirmwareSmBusDebug = {
	.read = DrvMainProcfsFirmwareSmBusDebugRead,
	.write = DrvMainProcfsFirmwareSmBusDebugWrite,
};

static const struct file_operations _gProcFirmwareSetDQMemValue = {
	.read = DrvMainProcfsFirmwareSetDQMemValueRead,
	.write = DrvMainProcfsFirmwareSetDQMemValueWrite,
};

#ifdef CONFIG_ENABLE_ITO_MP_TEST
static const struct file_operations _gProcMpTest = {
	.read = DrvMainProcfsMpTestRead,
	.write = DrvMainProcfsMpTestWrite,
};

static const struct file_operations _gProcMpTestLog = {
	.read = DrvMainProcfsMpTestLogRead,
	.write = DrvMainProcfsMpTestLogWrite,
};

static const struct file_operations _gProcMpTestFailChannel = {
	.read = DrvMainProcfsMpTestFailChannelRead,
	.write = DrvMainProcfsMpTestFailChannelWrite,
};

static const struct file_operations _gProcMpTestScope = {
	.read = DrvMainProcfsMpTestScopeRead,
	.write = DrvMainProcfsMpTestScopeWrite,
};
#endif


static const struct file_operations _gProcFirmwareMode = {
	.read = DrvMainProcfsFirmwareModeRead,
	.write = DrvMainProcfsFirmwareModeWrite,
};

static const struct file_operations _gProcFirmwareSensor = {
	.read = DrvMainProcfsFirmwareSensorRead,
	.write = DrvMainProcfsFirmwareSensorWrite,
};

static const struct file_operations _gProcFirmwarePacketHeader = {
	.read = DrvMainProcfsFirmwarePacketHeaderRead,
	.write = DrvMainProcfsFirmwarePacketHeaderWrite,
};


static const struct file_operations _gProcQueryFeatureSupportStatus = {
	.read = DrvMainProcfsQueryFeatureSupportStatusRead,
	.write = DrvMainProcfsQueryFeatureSupportStatusWrite,
};

static const struct file_operations _gProcChangeFeatureSupportStatus = {
	.read = DrvMainProcfsChangeFeatureSupportStatusRead,
	.write = DrvMainProcfsChangeFeatureSupportStatusWrite,
};

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
static const struct file_operations _gProcGestureWakeupMode = {
	.read = DrvMainProcfsGestureWakeupModeRead,
	.write = DrvMainProcfsGestureWakeupModeWrite,
};
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
static const struct file_operations _gProcGestureDebugMode = {
	.read = DrvMainProcfsGestureDebugModeRead,
	.write = DrvMainProcfsGestureDebugModeWrite,
};
#endif
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
static const struct file_operations _gProcGestureInforMode = {
	.read = DrvMainProcfsGestureInforModeRead,
	.write = DrvMainProcfsGestureInforModeWrite,
};
#endif
#endif

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
static const struct file_operations _gProcReportRate = {
	.read = DrvMainProcfsReportRateRead,
	.write = DrvMainProcfsReportRateWrite,
};
#endif

#ifdef CONFIG_ENABLE_GLOVE_MODE
static const struct file_operations _gProcGloveMode = {
	.read = DrvMainProcfsGloveModeRead,
	.write = DrvMainProcfsGloveModeWrite,
};

static const struct file_operations _gProcOpenGloveMode = {
	.read = DrvMainProcfsOpenGloveModeRead,
};

static const struct file_operations _gProcCloseGloveMode = {
	.read = DrvMainProcfsCloseGloveModeRead,
};
#endif

#ifdef CONFIG_ENABLE_JNI_INTERFACE
static const struct file_operations _gProcJniMethod = {
	.read = MsgToolRead,
	.write = MsgToolWrite,
	.unlocked_ioctl = MsgToolIoctl,
	.compat_ioctl = MsgToolIoctl,
};
#endif

static const struct file_operations _gProcSeLinuxLimitFirmwareUpdate = {
	.read = DrvMainProcfsSeLinuxLimitFirmwareUpdateRead,
};


#ifdef CONFIG_ENABLE_HOTKNOT
struct mutex g_HKMutex;
extern struct mutex g_QMutex;
#endif

u32 SLAVE_I2C_ID_DBBUS = (0xC4>>1);

u32 SLAVE_I2C_ID_DWI2C = (0x4C>>1);


u16 FIRMWARE_MODE_UNKNOWN_MODE = 0xFFFF;
u16 FIRMWARE_MODE_DEMO_MODE = 0xFFFF;
u16 FIRMWARE_MODE_DEBUG_MODE = 0xFFFF;
u16 FIRMWARE_MODE_RAW_DATA_MODE = 0xFFFF;

struct kset *g_TouchKSet = NULL;
struct kobject *g_TouchKObj = NULL;
u8 g_IsSwitchModeByAPK = 0;


u8 IS_GESTURE_WAKEUP_ENABLED = 0;
u8 IS_GESTURE_DEBUG_MODE_ENABLED = 0;
u8 IS_GESTURE_INFORMATION_MODE_ENABLED = 0;
u8 IS_GESTURE_WAKEUP_MODE_SUPPORT_64_TYPES_ENABLED = 0;

u8 IS_TOUCH_DRIVER_DEBUG_LOG_ENABLED = CONFIG_ENABLE_TOUCH_DRIVER_DEBUG_LOG;
u8 IS_FIRMWARE_DATA_LOG_ENABLED = CONFIG_ENABLE_FIRMWARE_DATA_LOG;
u8 IS_FORCE_TO_UPDATE_FIRMWARE_ENABLED = 0;


#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
struct kset *g_GestureKSet = NULL;
struct kobject *g_GestureKObj = NULL;
#endif
#endif

#ifdef CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA
u16 g_FwPacketDataAddress = 0;
u16 g_FwPacketFlagAddress = 0;
u8 g_FwSupportSegment = 0;
#endif

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
u32 g_IsEnableReportRate = 0;
u32 g_InterruptCount = 0;
u32 g_ValidTouchCount = 0;
u32 g_InterruptReportRate = 0;
u32 g_ValidTouchReportRate = 0;

struct timeval g_StartTime;
#endif

#ifdef CONFIG_ENABLE_GLOVE_MODE
u8 g_IsEnableGloveMode = 0;
#endif

u8 g_FwData[MAX_UPDATE_FIRMWARE_BUFFER_SIZE][1024];
u32 g_FwDataCount = 0;

u8 g_IsHotknotEnabled = 0;
u8 g_IsBypassHotknot = 0;


static s32 _DrvMainCreateProcfsDirEntry(void);
#ifdef CONFIG_ENABLE_HOTKNOT
static s32 _DrvMainHotknotRegistry(void);
#endif


ssize_t DrvMainProcfsChipTypeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	nLength = sprintf(pBuffer, "%d", g_ChipType);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsChipTypeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	DBG("*** %s() ***\n", __func__);



	return nCount;
}

ssize_t DrvMainProcfsFirmwareDataRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	DBG("*** %s() g_FwDataCount = %d ***\n", __func__, g_FwDataCount);


	if (*pPos != 0) {
		return 0;
	}

	*pPos += g_FwDataCount;

	return g_FwDataCount;
}

ssize_t DrvMainProcfsFirmwareDataWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nNum = nCount / 1024;
	u32 nRemainder = nCount % 1024;
	u32 i;

	DBG("*** %s() ***\n", __func__);

	if (nNum > 0) {
		for (i = 0; i < nNum; i++) {
			memcpy(g_FwData[g_FwDataCount], pBuffer+(i*1024), 1024);

			g_FwDataCount++;
		}

		if (nRemainder > 0) {
			DBG("nRemainder = %d\n", nRemainder);

			memcpy(g_FwData[g_FwDataCount], pBuffer+(i*1024), nRemainder);

			g_FwDataCount++;
		}
	} else {
		if (nCount > 0) {
			memcpy(g_FwData[g_FwDataCount], pBuffer, nCount);

			g_FwDataCount++;
		}
	}

	DBG("*** g_FwDataCount = %d ***\n", g_FwDataCount);

	if (pBuffer != NULL) {
		DBG("*** buf[0] = %c ***\n", pBuffer[0]);
	}

	return nCount;
}

ssize_t DrvMainProcfsFirmwareUpdateRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	nLength = sprintf(pBuffer, "%d", _gIsUpdateComplete);

	DBG("*** _gIsUpdateComplete = %d ***\n", _gIsUpdateComplete);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsFirmwareUpdateWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	DrvPlatformLyrDisableFingerTouchReport();

	DBG("*** %s() g_FwDataCount = %d ***\n", __func__, g_FwDataCount);

	if (0 != DrvIcFwLyrUpdateFirmware(g_FwData, EMEM_ALL)) {
		_gIsUpdateComplete = 0;
		DBG("Update FAILED\n");
	} else {
		_gIsUpdateComplete = 1;
		DBG("Update SUCCESS\n");
	}

	DrvPlatformLyrEnableFingerTouchReport();

	return nCount;
}

ssize_t DrvMainProcfsCustomerFirmwareVersionRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() _gFwVersion = %s ***\n", __func__, _gFwVersion);


	if (*pPos != 0) {
		return 0;
	}

	nLength = sprintf(pBuffer, "%s\n", _gFwVersion);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsCustomerFirmwareVersionWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u16 nMajor = 0, nMinor = 0;

	DrvIcFwLyrGetCustomerFirmwareVersion(&nMajor, &nMinor, &_gFwVersion);

	DBG("*** %s() _gFwVersion = %s ***\n", __func__, _gFwVersion);

	return nCount;
}

ssize_t DrvMainProcfsPlatformFirmwareVersionRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() _gPlatformFwVersion = %s ***\n", __func__, _gPlatformFwVersion);


	if (*pPos != 0) {
		return 0;
	}

	nLength = sprintf(pBuffer, "%s\n", _gPlatformFwVersion);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsPlatformFirmwareVersionWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	DrvIcFwLyrGetPlatformFirmwareVersion(&_gPlatformFwVersion);

	DBG("*** %s() _gPlatformFwVersion = %s ***\n", __func__, _gPlatformFwVersion);

	return nCount;
}

ssize_t DrvMainProcfsDeviceDriverVersionRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	nLength = sprintf(pBuffer, "%s", DEVICE_DRIVER_RELEASE_VERSION);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsDeviceDriverVersionWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	DBG("*** %s() ***\n", __func__);

	return nCount;
}

ssize_t DrvMainProcfsSdCardFirmwareUpdateRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u16 nMajor = 0, nMinor = 0;
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	DrvIcFwLyrGetCustomerFirmwareVersion(&nMajor, &nMinor, &_gFwVersion);

	DBG("*** %s() _gFwVersion = %s ***\n", __func__, _gFwVersion);

	nLength = sprintf(pBuffer, "%s\n", _gFwVersion);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsSdCardFirmwareUpdateWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	char *pValid = NULL;
	char *pTmpFilePath = NULL;
	char szFilePath[100] = {0};

	DBG("*** %s() ***\n", __func__);
	DBG("pBuffer = %s\n", pBuffer);

	if (pBuffer != NULL) {
		pValid = strstr(pBuffer, ".bin");

		if (pValid) {
			pTmpFilePath = strsep((char **)&pBuffer, ".");

			DBG("pTmpFilePath = %s\n", pTmpFilePath);

			strcat(szFilePath, pTmpFilePath);
			strcat(szFilePath, ".bin");

			DBG("szFilePath = %s\n", szFilePath);

			if (0 != DrvIcFwLyrUpdateFirmwareBySdCard(szFilePath)) {
				DBG("Update FAILED\n");
			} else {
				DBG("Update SUCCESS\n");
			}
		} else {
			DBG("The file type of the update firmware bin file is not a .bin file.\n");
		}
	} else {
		DBG("The file path of the update firmware bin file is NULL.\n");
	}

	return nCount;
}

ssize_t DrvMainProcfsSeLinuxLimitFirmwareUpdateRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	DBG("FIRMWARE_FILE_PATH_ON_SD_CARD = %s\n", FIRMWARE_FILE_PATH_ON_SD_CARD);

	if (0 != DrvIcFwLyrUpdateFirmwareBySdCard(FIRMWARE_FILE_PATH_ON_SD_CARD)) {
		_gIsUpdateComplete = 0;
		DBG("Update FAILED\n");
	} else {
		_gIsUpdateComplete = 1;
		DBG("Update SUCCESS\n");
	}

	nLength = sprintf(pBuffer, "%d", _gIsUpdateComplete);

	DBG("*** _gIsUpdateComplete = %d ***\n", _gIsUpdateComplete);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsFirmwareDebugRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 i, nLength = 0;
	u8 nBank, nAddr;
	u16 szRegData[MAX_DEBUG_REGISTER_NUM] = {0};
	u8 szOut[MAX_DEBUG_REGISTER_NUM*25] = {0}, szValue[10] = {0};

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	for (i = 0; i < _gDebugRegCount; i++) {
		szRegData[i] = RegGet16BitValue(_gDebugReg[i]);
	}

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	for (i = 0; i < _gDebugRegCount; i++) {
		nBank = (_gDebugReg[i] >> 8) & 0xFF;
		nAddr = _gDebugReg[i] & 0xFF;

		DBG("reg(0x%02X, 0x%02X)=0x%04X\n", nBank, nAddr, szRegData[i]);

		strcat(szOut, "reg(");
		sprintf(szValue, "0x%02X", nBank);
		strcat(szOut, szValue);
		strcat(szOut, ", ");
		sprintf(szValue, "0x%02X", nAddr);
		strcat(szOut, szValue);
		strcat(szOut, ")=");
		sprintf(szValue, "0x%04X", szRegData[i]);
		strcat(szOut, szValue);
		strcat(szOut, "\n");
	}

	nLength = sprintf(pBuffer, "%s\n", szOut);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsFirmwareDebugWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 i;
	char *pCh;
	char *pData = NULL;

	DBG("*** %s() ***\n", __func__);

	if (pBuffer != NULL) {
		DBG("*** pBuffer[0] = %c ***\n", pBuffer[0]);
		DBG("*** pBuffer[1] = %c ***\n", pBuffer[1]);
		DBG("*** pBuffer[2] = %c ***\n", pBuffer[2]);
		DBG("*** pBuffer[3] = %c ***\n", pBuffer[3]);
		DBG("*** pBuffer[4] = %c ***\n", pBuffer[4]);
		DBG("*** pBuffer[5] = %c ***\n", pBuffer[5]);

		DBG("nCount = %d\n", (int)nCount);

		pData = kmalloc(nCount, GFP_KERNEL);
		if (!pData) {
			return -ENOMEM;
		}

		if (copy_from_user(pData, pBuffer, nCount)) {
			DBG("copy_from_user() failed\n");

			kfree(pData);
			return -EFAULT;
		}

		i = 0;

		while ((pCh = strsep((char **)&pData, " , ")) && (i < MAX_DEBUG_REGISTER_NUM)) {
			DBG("pCh = %s\n", pCh);

			_gDebugReg[i] = DrvCommonConvertCharToHexDigit(pCh, strlen(pCh));

			DBG("_gDebugReg[%d] = 0x%04X\n", i, _gDebugReg[i]);
			i++;
		}
		_gDebugRegCount = i;

		DBG("_gDebugRegCount = %d\n", _gDebugRegCount);

		kfree(pData);
	}

	return nCount;
}

ssize_t DrvMainProcfsFirmwareSetDebugValueRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 i, nLength = 0;
	u8 nBank, nAddr;
	u16 szRegData[MAX_DEBUG_REGISTER_NUM] = {0};
	u8 szOut[MAX_DEBUG_REGISTER_NUM*25] = {0}, szValue[10] = {0};

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	DbBusEnterSerialDebugMode();
	DbBusStopMCU();
	DbBusIICUseBus();
	DbBusIICReshape();
	mdelay(100);

	for (i = 0; i < _gDebugRegCount; i++) {
		szRegData[i] = RegGet16BitValue(_gDebugReg[i]);
	}

	DbBusIICNotUseBus();
	DbBusNotStopMCU();
	DbBusExitSerialDebugMode();

	for (i = 0; i < _gDebugRegCount; i++) {
		nBank = (_gDebugReg[i] >> 8) & 0xFF;
		nAddr = _gDebugReg[i] & 0xFF;

		DBG("reg(0x%02X, 0x%02X)=0x%04X\n", nBank, nAddr, szRegData[i]);

		strcat(szOut, "reg(");
		sprintf(szValue, "0x%02X", nBank);
		strcat(szOut, szValue);
		strcat(szOut, ", ");
		sprintf(szValue, "0x%02X", nAddr);
		strcat(szOut, szValue);
		strcat(szOut, ")=");
		sprintf(szValue, "0x%04X", szRegData[i]);
		strcat(szOut, szValue);
		strcat(szOut, "\n");
	}

	nLength = sprintf(pBuffer, "%s\n", szOut);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsFirmwareSetDebugValueWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 i, j, k;
	char *pCh;
	char *pData = NULL;

	DBG("*** %s() ***\n", __func__);

	if (pBuffer != NULL) {
		DBG("*** pBuffer[0] = %c ***\n", pBuffer[0]);
		DBG("*** pBuffer[1] = %c ***\n", pBuffer[1]);

		DBG("nCount = %d\n", (int)nCount);

		pData = kmalloc(nCount, GFP_KERNEL);
		if (!pData) {
			return -ENOMEM;
		}

		if (copy_from_user(pData, pBuffer, nCount)) {
			DBG("copy_from_user() failed\n");

			kfree(pData);
			return -EFAULT;
		}

		i = 0;
		j = 0;
		k = 0;

		while ((pCh = strsep((char **)&pData, " , ")) && (i < 2)) {
			DBG("pCh = %s\n", pCh);

			if ((i%2) == 0) {
				_gDebugReg[j] = DrvCommonConvertCharToHexDigit(pCh, strlen(pCh));
				DBG("_gDebugReg[%d] = 0x%04X\n", j, _gDebugReg[j]);
				j++;
			} else {
				_gDebugRegValue[k] = DrvCommonConvertCharToHexDigit(pCh, strlen(pCh));
				DBG("_gDebugRegValue[%d] = 0x%04X\n", k, _gDebugRegValue[k]);
				k++;
			}

			i++;
		}
		_gDebugRegCount = j;

		DBG("_gDebugRegCount = %d\n", _gDebugRegCount);

		DbBusEnterSerialDebugMode();
		DbBusStopMCU();
		DbBusIICUseBus();
		DbBusIICReshape();
		mdelay(100);

		for (i = 0; i < _gDebugRegCount; i++) {
			RegSet16BitValue(_gDebugReg[i], _gDebugRegValue[i]);
			DBG("_gDebugReg[%d] = 0x%04X, _gDebugRegValue[%d] = 0x%04X\n", i, _gDebugReg[i], i , _gDebugRegValue[i]);
		}

		DbBusIICNotUseBus();
		DbBusNotStopMCU();
		DbBusExitSerialDebugMode();

		kfree(pData);
	}

	return nCount;
}

ssize_t DrvMainProcfsFirmwareSmBusDebugRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 i, nLength = 0;
	u8 szSmBusRxData[MAX_I2C_TRANSACTION_LENGTH_LIMIT] = {0};
	u8 szOut[MAX_I2C_TRANSACTION_LENGTH_LIMIT*2] = {0};
	u8 szValue[10] = {0};

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

#ifdef CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM
#ifdef CONFIG_ENABLE_DMA_IIC
	DmaReset();
#endif
#endif

	if (_gDebugCmdArguCount > 0) {
		DBG("Execute I2C SMBUS write command\n");

		IicWriteData(SLAVE_I2C_ID_DWI2C, &_gDebugCmdArgu[0], _gDebugCmdArguCount);
	}

	if (_gDebugReadDataSize > 0) {
		DBG("Execute I2C SMBUS read command\n");

		IicReadData(SLAVE_I2C_ID_DWI2C, &szSmBusRxData[0], _gDebugReadDataSize);
	}

	for (i = 0; i < _gDebugReadDataSize; i++) {
		DBG("szSmBusRxData[%d] = 0x%x\n", i, szSmBusRxData[i]);

		sprintf(szValue, "0x%02X", szSmBusRxData[i]);
		strcat(szOut, szValue);
		strcat(szOut, "\n");
	}

	nLength = sprintf(pBuffer, "%s\n", szOut);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsFirmwareSmBusDebugWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 i, j;
	char szCmdType[5] = {0};
	char *pCh = NULL;
	char *pData = NULL;

	DBG("*** %s() ***\n", __func__);

	if (pBuffer != NULL) {
		DBG("*** pBuffer[0] = %c ***\n", pBuffer[0]);
		DBG("*** pBuffer[1] = %c ***\n", pBuffer[1]);
		DBG("*** pBuffer[2] = %c ***\n", pBuffer[2]);
		DBG("*** pBuffer[3] = %c ***\n", pBuffer[3]);
		DBG("*** pBuffer[4] = %c ***\n", pBuffer[4]);
		DBG("*** pBuffer[5] = %c ***\n", pBuffer[5]);

		DBG("nCount = %d\n", (int)nCount);

		pData = kmalloc(nCount, GFP_KERNEL);
		if (!pData) {
			return -ENOMEM;
		}

		if (copy_from_user(pData, pBuffer, nCount)) {
			DBG("copy_from_user() failed\n");

			kfree(pData);
			return -EFAULT;
		}


		_gDebugCmdArguCount = 0;
		_gDebugReadDataSize = 0;

		i = 0;
		j = 0;

		while ((pCh = strsep((char **)&pData, " , ")) && (j < MAX_DEBUG_COMMAND_ARGUMENT_NUM)) {
			DBG("pCh = %s\n", pCh);

			if (strcmp(pCh, "w") == 0 || strcmp(pCh, "r") == 0) {
				memcpy(szCmdType, pCh, strlen(pCh));
			} else if (strcmp(szCmdType, "w") == 0) {
				_gDebugCmdArgu[j] = DrvCommonConvertCharToHexDigit(pCh, strlen(pCh));
				DBG("_gDebugCmdArgu[%d] = 0x%02X\n", j, _gDebugCmdArgu[j]);

				j++;

				_gDebugCmdArguCount = j;
				DBG("_gDebugCmdArguCount = %d\n", _gDebugCmdArguCount);
			} else if (strcmp(szCmdType, "r") == 0) {
				sscanf(pCh, "%d", &_gDebugReadDataSize);
				DBG("_gDebugReadDataSize = %d\n", _gDebugReadDataSize);
			} else {
				DBG("Un-supported adb command format!\n");
			}

			i++;
		}

		kfree(pData);
	}

	return nCount;
}

ssize_t DrvMainProcfsFirmwareSetDQMemValueRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 i, nLength = 0;
	u8 nBank, nAddr;
	u32 szRegData[MAX_DEBUG_REGISTER_NUM] = {0};
	u8 szOut[MAX_DEBUG_REGISTER_NUM*25] = {0}, szValue[10] = {0};

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	for (i = 0; i < _gDebugRegCount; i++) {
		szRegData[i] = DrvIcFwLyrReadDQMemValue(_gDebugReg[i]);
	}

	for (i = 0; i < _gDebugRegCount; i++) {
		nBank = (_gDebugReg[i] >> 8) & 0xFF;
		nAddr = _gDebugReg[i] & 0xFF;

		DBG("reg(0x%02X, 0x%02X)=0x%08X\n", nBank, nAddr, szRegData[i]);

		strcat(szOut, "reg(");
		sprintf(szValue, "0x%02X", nBank);
		strcat(szOut, szValue);
		strcat(szOut, ", ");
		sprintf(szValue, "0x%02X", nAddr);
		strcat(szOut, szValue);
		strcat(szOut, ")=");
		sprintf(szValue, "0x%04X", szRegData[i]);
		strcat(szOut, szValue);
		strcat(szOut, "\n");
	}

	nLength = sprintf(pBuffer, "%s\n", szOut);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsFirmwareSetDQMemValueWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 i, j, k;
	char *pCh;
	char *pData = NULL;
	u16 nRealDQMemAddr = 0;
	u32 nRealDQMemValue = 0;

	DBG("*** %s() ***\n", __func__);

	if (pBuffer != NULL) {
		DBG("*** pBuffer[0] = %c ***\n", pBuffer[0]);
		DBG("*** pBuffer[1] = %c ***\n", pBuffer[1]);

		DBG("nCount = %d\n", (int)nCount);

		pData = kmalloc(nCount, GFP_KERNEL);
		if (!pData) {
			return -ENOMEM;
		}

		if (copy_from_user(pData, pBuffer, nCount)) {
			DBG("copy_from_user() failed\n");

			kfree(pData);
			return -EFAULT;
		}

		i = 0;
		j = 0;
		k = 0;

		while ((pCh = strsep((char **)&pData, " , ")) && (i < 2)) {
			DBG("pCh = %s\n", pCh);

			if ((i%2) == 0) {
				_gDebugReg[j] = DrvCommonConvertCharToHexDigit(pCh, strlen(pCh));
				DBG("_gDebugReg[%d] = 0x%04X\n", j, _gDebugReg[j]);
				j++;
			} else {
				_gDebugRegValue[k] = DrvCommonConvertCharToHexDigit(pCh, strlen(pCh));
				DBG("_gDebugRegValue[%d] = 0x%04X\n", k, _gDebugRegValue[k]);
				k++;
			}

			i++;
		}
		_gDebugRegCount = j;

		DBG("_gDebugRegCount = %d\n", _gDebugRegCount);

		if ((_gDebugReg[0] % 4) == 0) {
			nRealDQMemAddr = _gDebugReg[0];
			nRealDQMemValue = DrvIcFwLyrReadDQMemValue(nRealDQMemAddr);
			_gDebugReg[0] = nRealDQMemAddr;
			DBG("nRealDQMemValue Raw = %X\n", nRealDQMemValue);
			nRealDQMemValue &= 0xFFFF0000;
			nRealDQMemValue |= _gDebugRegValue[0];
			DBG("nRealDQMemValue Modify = %X\n", nRealDQMemValue);
			DrvIcFwLyrWriteDQMemValue(nRealDQMemAddr, nRealDQMemValue);
		} else if ((_gDebugReg[0] % 4) == 2) {
			nRealDQMemAddr = _gDebugReg[0] - 2;
			nRealDQMemValue = DrvIcFwLyrReadDQMemValue(nRealDQMemAddr);
			_gDebugReg[0] = nRealDQMemAddr;
			DBG("nRealDQMemValue Raw = %X\n", nRealDQMemValue);

			nRealDQMemValue &= 0x0000FFFF;
			nRealDQMemValue |= (_gDebugRegValue[0] << 16);
			DBG("nRealDQMemValue Modify = %X\n", nRealDQMemValue);
			DrvIcFwLyrWriteDQMemValue(nRealDQMemAddr, nRealDQMemValue);
		}

		kfree(pData);
	}

	return nCount;
}

/*--------------------------------------------------------------------------*/

#ifdef CONFIG_ENABLE_ITO_MP_TEST

ssize_t DrvMainProcfsMpTestRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	DBG("*** ctp mp test status = %d ***\n", DrvIcFwLyrGetMpTestResult());

	nLength = sprintf(pBuffer, "%d", DrvIcFwLyrGetMpTestResult());

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsMpTestWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nMode = 0;

	DBG("*** %s() ***\n", __func__);

	if (pBuffer != NULL) {
		sscanf(pBuffer, "%x", &nMode);

		DBG("Mp Test Mode = 0x%x\n", nMode);

		if (nMode == ITO_TEST_MODE_OPEN_TEST) {
			_gItoTestMode = ITO_TEST_MODE_OPEN_TEST;
			DrvIcFwLyrScheduleMpTestWork(ITO_TEST_MODE_OPEN_TEST);
		} else if (nMode == ITO_TEST_MODE_SHORT_TEST) {
			_gItoTestMode = ITO_TEST_MODE_SHORT_TEST;
			DrvIcFwLyrScheduleMpTestWork(ITO_TEST_MODE_SHORT_TEST);
		} else if (nMode == ITO_TEST_MODE_WATERPROOF_TEST) {
			_gItoTestMode = ITO_TEST_MODE_WATERPROOF_TEST;
			DrvIcFwLyrScheduleMpTestWork(ITO_TEST_MODE_WATERPROOF_TEST);
		} else {
			DBG("*** Undefined MP Test Mode ***\n");
		}
	}

	return nCount;
}

ssize_t DrvMainProcfsMpTestLogRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	DrvIcFwLyrGetMpTestDataLog(_gItoTestMode, pBuffer, &nLength);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsMpTestLogWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	DBG("*** %s() ***\n", __func__);

	return nCount;
}

ssize_t DrvMainProcfsMpTestFailChannelRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	DrvIcFwLyrGetMpTestFailChannel(_gItoTestMode, pBuffer, &nLength);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsMpTestFailChannelWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	DBG("*** %s() ***\n", __func__);

	return nCount;
}

ssize_t DrvMainProcfsMpTestScopeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
	DrvIcFwLyrGetMpTestScope(&g_TestScopeInfo);

	nLength = sprintf(pBuffer, "%d, %d", g_TestScopeInfo.nMx, g_TestScopeInfo.nMy);
#endif

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsMpTestScopeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	DBG("*** %s() ***\n", __func__);

	return nCount;
}

#endif

/*--------------------------------------------------------------------------*/

ssize_t DrvMainProcfsFirmwareModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
	if (g_ChipType == CHIP_TYPE_MSG26XXM) {
		g_FirmwareMode = DrvIcFwLyrGetFirmwareMode();

		DBG("%s() firmware mode = 0x%x\n", __func__, g_FirmwareMode);

		nLength = sprintf(pBuffer, "%x", g_FirmwareMode);
	} else if (g_ChipType == CHIP_TYPE_MSG28XX) {
		DrvIcFwLyrGetFirmwareInfo(&g_FirmwareInfo);
		g_FirmwareMode = g_FirmwareInfo.nFirmwareMode;

		DBG("%s() firmware mode = 0x%x\n", __func__, g_FirmwareInfo.nFirmwareMode);

		nLength = sprintf(pBuffer, "%x", g_FirmwareInfo.nFirmwareMode);
	}
#elif defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_SELF_IC)
	if (g_ChipType == CHIP_TYPE_MSG21XXA || g_ChipType == CHIP_TYPE_MSG22XX) {
		DrvIcFwLyrGetFirmwareInfo(&g_FirmwareInfo);
		g_FirmwareMode = g_FirmwareInfo.nFirmwareMode;

		DBG("%s() firmware mode = 0x%x, can change firmware mode = %d\n", __func__, g_FirmwareInfo.nFirmwareMode, g_FirmwareInfo.nIsCanChangeFirmwareMode);

		nLength = sprintf(pBuffer, "%x, %d", g_FirmwareInfo.nFirmwareMode, g_FirmwareInfo.nIsCanChangeFirmwareMode);
	}
#endif

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsFirmwareModeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nMode;

	DBG("*** %s() ***\n", __func__);

	if (pBuffer != NULL) {
		sscanf(pBuffer, "%x", &nMode);
		DBG("firmware mode = 0x%x\n", nMode);

		g_IsSwitchModeByAPK = 0;

		if (nMode == FIRMWARE_MODE_DEMO_MODE) {
			g_FirmwareMode = DrvIcFwLyrChangeFirmwareMode(FIRMWARE_MODE_DEMO_MODE);
		} else if (nMode == FIRMWARE_MODE_DEBUG_MODE) {
			g_FirmwareMode = DrvIcFwLyrChangeFirmwareMode(FIRMWARE_MODE_DEBUG_MODE);
			g_IsSwitchModeByAPK = 1;
		}
#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_SELF_IC)
		else if (nMode == FIRMWARE_MODE_RAW_DATA_MODE) {
			g_FirmwareMode = DrvIcFwLyrChangeFirmwareMode(FIRMWARE_MODE_RAW_DATA_MODE);
			g_IsSwitchModeByAPK = 1;
		}
#endif
		else {
			DBG("*** Undefined Firmware Mode ***\n");
		}
	}

	DBG("*** g_FirmwareMode = 0x%x ***\n", g_FirmwareMode);

	return nCount;
}

ssize_t DrvMainProcfsFirmwareSensorRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
	if (g_FirmwareInfo.nLogModePacketHeader == 0xA5 || g_FirmwareInfo.nLogModePacketHeader == 0xAB) {
		nLength = sprintf(pBuffer, "%d, %d", g_FirmwareInfo.nMx, g_FirmwareInfo.nMy);
	} else if (g_FirmwareInfo.nLogModePacketHeader == 0xA7) {
		nLength = sprintf(pBuffer, "%d, %d, %d, %d", g_FirmwareInfo.nMx, g_FirmwareInfo.nMy, g_FirmwareInfo.nSs, g_FirmwareInfo.nSd);
	} else {
		DBG("Undefined debug mode packet format : 0x%x\n", g_FirmwareInfo.nLogModePacketHeader);
		nLength = 0;
	}
#elif defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_SELF_IC)
	nLength = sprintf(pBuffer, "%d", g_FirmwareInfo.nLogModePacketLength);
#endif

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsFirmwareSensorWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	DBG("*** %s() ***\n", __func__);

	return nCount;
}

ssize_t DrvMainProcfsFirmwarePacketHeaderRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	nLength = sprintf(pBuffer, "%d", g_FirmwareInfo.nLogModePacketHeader);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsFirmwarePacketHeaderWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	DBG("*** %s() ***\n", __func__);

	return nCount;
}

ssize_t DrvMainKObjectPacketShow(struct kobject *pKObj, struct kobj_attribute *pAttr, char *pBuf)
{
	u32 i = 0;
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);

	if (strcmp(pAttr->attr.name, "packet") == 0) {
		if (g_LogModePacket != NULL) {
			DBG("g_FirmwareMode=%x, g_LogModePacket[0]=%x, g_LogModePacket[1]=%x\n", g_FirmwareMode, g_LogModePacket[0], g_LogModePacket[1]);
			DBG("g_LogModePacket[2]=%x, g_LogModePacket[3]=%x\n", g_LogModePacket[2], g_LogModePacket[3]);
			DBG("g_LogModePacket[4]=%x, g_LogModePacket[5]=%x\n", g_LogModePacket[4], g_LogModePacket[5]);

#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
			if ((g_FirmwareMode == FIRMWARE_MODE_DEBUG_MODE) && (g_LogModePacket[0] == 0xA5 || g_LogModePacket[0] == 0xAB || g_LogModePacket[0] == 0xA7))
#elif defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_SELF_IC)
			if ((g_FirmwareMode == FIRMWARE_MODE_DEBUG_MODE || g_FirmwareMode == FIRMWARE_MODE_RAW_DATA_MODE) && (g_LogModePacket[0] == 0x62)) {
#endif
				for (i = 0; i < g_FirmwareInfo.nLogModePacketLength; i++) {
					pBuf[i] = g_LogModePacket[i];
				}

				nLength = g_FirmwareInfo.nLogModePacketLength;
				DBG("nLength = %d\n", nLength);
			} else {
				DBG("CURRENT MODE IS NOT DEBUG MODE/WRONG DEBUG MODE HEADER\n");
			}
		} else {
			DBG("g_LogModePacket is NULL\n");
		}
	} else {
		DBG("pAttr->attr.name = %s \n", pAttr->attr.name);
	}

	return nLength;
}

ssize_t DrvMainKObjectPacketStore(struct kobject *pKObj, struct kobj_attribute *pAttr, const char *pBuf, size_t nCount)
{
	DBG("*** %s() ***\n", __func__);
	return nCount;
}

static struct kobj_attribute packet_attr = __ATTR(packet, 0666, DrvMainKObjectPacketShow, DrvMainKObjectPacketStore);

/* Create a group of attributes so that we can create and destroy them all at once. */
static struct attribute *attrs[] = {
	&packet_attr.attr,
	NULL, 	/* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory. If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
struct attribute_group attr_group = {
	.attrs = attrs,
};




ssize_t DrvMainProcfsQueryFeatureSupportStatusRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	nLength = sprintf(pBuffer, "%d", _gFeatureSupportStatus);

	DBG("*** _gFeatureSupportStatus = %d ***\n", _gFeatureSupportStatus);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsQueryFeatureSupportStatusWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nFeature;

	DBG("*** %s() ***\n", __func__);

	if (pBuffer != NULL) {
		sscanf(pBuffer, "%x", &nFeature);
		DBG("nFeature = 0x%x\n", nFeature);

		if (nFeature == FEATURE_GESTURE_WAKEUP_MODE) {
			_gFeatureSupportStatus = IS_GESTURE_WAKEUP_ENABLED;
		} else if (nFeature == FEATURE_GESTURE_DEBUG_MODE) {
			_gFeatureSupportStatus = IS_GESTURE_DEBUG_MODE_ENABLED;
		} else if (nFeature == FEATURE_GESTURE_INFORMATION_MODE) {
			_gFeatureSupportStatus = IS_GESTURE_INFORMATION_MODE_ENABLED;
		} else if (nFeature == FEATURE_TOUCH_DRIVER_DEBUG_LOG) {
			_gFeatureSupportStatus = IS_TOUCH_DRIVER_DEBUG_LOG_ENABLED;
		} else if (nFeature == FEATURE_FIRMWARE_DATA_LOG) {
			_gFeatureSupportStatus = IS_FIRMWARE_DATA_LOG_ENABLED;

#ifdef CONFIG_ENABLE_SEGMENT_READ_FINGER_TOUCH_DATA
#if defined(CONFIG_ENABLE_TOUCH_DRIVER_FOR_MUTUAL_IC)
			if (_gFeatureSupportStatus == 1) {
				DrvIcFwLyrGetTouchPacketAddress(&g_FwPacketDataAddress, &g_FwPacketFlagAddress);
			}
#endif
#endif
		} else if (nFeature == FEATURE_FORCE_TO_UPDATE_FIRMWARE) {
			_gFeatureSupportStatus = IS_FORCE_TO_UPDATE_FIRMWARE_ENABLED;
		} else {
			DBG("*** Undefined Feature ***\n");
		}
	}

	DBG("*** _gFeatureSupportStatus = %d ***\n", _gFeatureSupportStatus);

	return nCount;
}

ssize_t DrvMainProcfsChangeFeatureSupportStatusRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	nLength = sprintf(pBuffer, "%d", _gFeatureSupportStatus);

	DBG("*** _gFeatureSupportStatus = %d ***\n", _gFeatureSupportStatus);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsChangeFeatureSupportStatusWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 i;
	u32 nFeature = 0, nNewValue = 0;
	char *pCh;
	char *pData = NULL;

	DBG("*** %s() ***\n", __func__);

	if (pBuffer != NULL) {
		DBG("nCount = %d\n", (int)nCount);

		pData = kmalloc(nCount, GFP_KERNEL);
		if (!pData) {
			return -ENOMEM;
		}

		if (copy_from_user(pData, pBuffer, nCount)) {
			DBG("copy_from_user() failed\n");

			kfree(pData);
			return -EFAULT;
		}

		i = 0;

		while ((pCh = strsep((char **)&pData, " , ")) && (i < 3)) {
			DBG("pCh = %s\n", pCh);

			if (i == 0) {
				nFeature = DrvCommonConvertCharToHexDigit(pCh, strlen(pCh));
				DBG("nFeature = 0x%04X\n", nFeature);
			} else if (i == 1) {
				nNewValue = DrvCommonConvertCharToHexDigit(pCh, strlen(pCh));
				DBG("nNewValue = %d\n", nNewValue);
			} else {
				DBG("End of parsing adb command.\n");
			}

			i++;
		}

		if (nFeature == FEATURE_GESTURE_WAKEUP_MODE) {
			IS_GESTURE_WAKEUP_ENABLED = nNewValue;
			_gFeatureSupportStatus = IS_GESTURE_WAKEUP_ENABLED;
		} else if (nFeature == FEATURE_GESTURE_DEBUG_MODE) {
			IS_GESTURE_DEBUG_MODE_ENABLED = nNewValue;
			_gFeatureSupportStatus = IS_GESTURE_DEBUG_MODE_ENABLED;
		} else if (nFeature == FEATURE_GESTURE_INFORMATION_MODE) {
			IS_GESTURE_INFORMATION_MODE_ENABLED = nNewValue;
			_gFeatureSupportStatus = IS_GESTURE_INFORMATION_MODE_ENABLED;
		} else if (nFeature == FEATURE_TOUCH_DRIVER_DEBUG_LOG) {
			IS_TOUCH_DRIVER_DEBUG_LOG_ENABLED = nNewValue;
			_gFeatureSupportStatus = IS_TOUCH_DRIVER_DEBUG_LOG_ENABLED;
		} else if (nFeature == FEATURE_FIRMWARE_DATA_LOG) {
			IS_FIRMWARE_DATA_LOG_ENABLED = nNewValue;
			_gFeatureSupportStatus = IS_FIRMWARE_DATA_LOG_ENABLED;
		} else if (nFeature == FEATURE_FORCE_TO_UPDATE_FIRMWARE) {
			IS_FORCE_TO_UPDATE_FIRMWARE_ENABLED = nNewValue;
			_gFeatureSupportStatus = IS_FORCE_TO_UPDATE_FIRMWARE_ENABLED;
		} else {
			DBG("*** Undefined Feature ***\n");
		}

		DBG("*** _gFeatureSupportStatus = %d ***\n", _gFeatureSupportStatus);

		kfree(pData);
	}

	return nCount;
}



#ifdef CONFIG_ENABLE_GESTURE_WAKEUP

ssize_t DrvMainProcfsGestureWakeupModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
	DBG("g_GestureWakeupMode = 0x%x, 0x%x\n", g_GestureWakeupMode[0], g_GestureWakeupMode[1]);

	nLength = sprintf(pBuffer, "%x, %x", g_GestureWakeupMode[0], g_GestureWakeupMode[1]);
#else
	DBG("g_GestureWakeupMode = 0x%x\n", g_GestureWakeupMode[0]);

	nLength = sprintf(pBuffer, "%x", g_GestureWakeupMode[0]);
#endif

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsGestureWakeupModeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength;
	u32 nWakeupMode[2] = {0};

	DBG("*** %s() ***\n", __func__);

	if (pBuffer != NULL) {
#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
		u32 i;
		char *pCh;

		i = 0;
		while ((pCh = strsep((char **)&pBuffer, " , ")) && (i < 2)) {
			DBG("pCh = %s\n", pCh);

			nWakeupMode[i] = DrvCommonConvertCharToHexDigit(pCh, strlen(pCh));

			DBG("nWakeupMode[%d] = 0x%04X\n", i, nWakeupMode[i]);
			i++;
		}
#else
		sscanf(pBuffer, "%x", &nWakeupMode[0]);
		DBG("nWakeupMode = 0x%x\n", nWakeupMode[0]);
#endif

		nLength = nCount;
		DBG("nLength = %d\n", nLength);

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG) == GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_DOUBLE_CLICK_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG) == GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_UP_DIRECT_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG) == GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_DOWN_DIRECT_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG) == GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_LEFT_DIRECT_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG) == GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RIGHT_DIRECT_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_m_CHARACTER_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_W_CHARACTER_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_C_CHARACTER_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_e_CHARACTER_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_V_CHARACTER_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_O_CHARACTER_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_S_CHARACTER_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG) == GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_Z_CHARACTER_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE1_FLAG) == GESTURE_WAKEUP_MODE_RESERVE1_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE1_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE1_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE2_FLAG) == GESTURE_WAKEUP_MODE_RESERVE2_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE2_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE2_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE3_FLAG) == GESTURE_WAKEUP_MODE_RESERVE3_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE3_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE3_FLAG);
		}

#ifdef CONFIG_SUPPORT_64_TYPES_GESTURE_WAKEUP_MODE
		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE4_FLAG) == GESTURE_WAKEUP_MODE_RESERVE4_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE4_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE4_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE5_FLAG) == GESTURE_WAKEUP_MODE_RESERVE5_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE5_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE5_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE6_FLAG) == GESTURE_WAKEUP_MODE_RESERVE6_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE6_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE6_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE7_FLAG) == GESTURE_WAKEUP_MODE_RESERVE7_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE7_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE7_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE8_FLAG) == GESTURE_WAKEUP_MODE_RESERVE8_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE8_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE8_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE9_FLAG) == GESTURE_WAKEUP_MODE_RESERVE9_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE9_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE9_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE10_FLAG) == GESTURE_WAKEUP_MODE_RESERVE10_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE10_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE10_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE11_FLAG) == GESTURE_WAKEUP_MODE_RESERVE11_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE11_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE11_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE12_FLAG) == GESTURE_WAKEUP_MODE_RESERVE12_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE12_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE12_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE13_FLAG) == GESTURE_WAKEUP_MODE_RESERVE13_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE13_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE13_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE14_FLAG) == GESTURE_WAKEUP_MODE_RESERVE14_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE14_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE14_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE15_FLAG) == GESTURE_WAKEUP_MODE_RESERVE15_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE15_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE15_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE16_FLAG) == GESTURE_WAKEUP_MODE_RESERVE16_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE16_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE16_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE17_FLAG) == GESTURE_WAKEUP_MODE_RESERVE17_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE17_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE17_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE18_FLAG) == GESTURE_WAKEUP_MODE_RESERVE18_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE18_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE18_FLAG);
		}

		if ((nWakeupMode[0] & GESTURE_WAKEUP_MODE_RESERVE19_FLAG) == GESTURE_WAKEUP_MODE_RESERVE19_FLAG) {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] | GESTURE_WAKEUP_MODE_RESERVE19_FLAG;
		} else {
			g_GestureWakeupMode[0] = g_GestureWakeupMode[0] & (~GESTURE_WAKEUP_MODE_RESERVE19_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE20_FLAG) == GESTURE_WAKEUP_MODE_RESERVE20_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE20_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE20_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE21_FLAG) == GESTURE_WAKEUP_MODE_RESERVE21_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE21_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE21_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE22_FLAG) == GESTURE_WAKEUP_MODE_RESERVE22_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE22_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE22_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE23_FLAG) == GESTURE_WAKEUP_MODE_RESERVE23_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE23_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE23_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE24_FLAG) == GESTURE_WAKEUP_MODE_RESERVE24_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE24_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE24_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE25_FLAG) == GESTURE_WAKEUP_MODE_RESERVE25_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE25_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE25_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE26_FLAG) == GESTURE_WAKEUP_MODE_RESERVE26_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE26_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE26_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE27_FLAG) == GESTURE_WAKEUP_MODE_RESERVE27_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE27_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE27_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE28_FLAG) == GESTURE_WAKEUP_MODE_RESERVE28_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE28_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE28_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE29_FLAG) == GESTURE_WAKEUP_MODE_RESERVE29_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE29_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE29_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE30_FLAG) == GESTURE_WAKEUP_MODE_RESERVE30_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE30_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE30_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE31_FLAG) == GESTURE_WAKEUP_MODE_RESERVE31_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE31_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE31_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE32_FLAG) == GESTURE_WAKEUP_MODE_RESERVE32_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE32_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE32_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE33_FLAG) == GESTURE_WAKEUP_MODE_RESERVE33_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE33_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE33_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE34_FLAG) == GESTURE_WAKEUP_MODE_RESERVE34_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE34_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE34_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE35_FLAG) == GESTURE_WAKEUP_MODE_RESERVE35_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE35_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE35_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE36_FLAG) == GESTURE_WAKEUP_MODE_RESERVE36_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE36_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE36_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE37_FLAG) == GESTURE_WAKEUP_MODE_RESERVE37_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE37_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE37_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE38_FLAG) == GESTURE_WAKEUP_MODE_RESERVE38_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE38_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE38_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE39_FLAG) == GESTURE_WAKEUP_MODE_RESERVE39_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE39_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE39_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE40_FLAG) == GESTURE_WAKEUP_MODE_RESERVE40_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE40_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE40_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE41_FLAG) == GESTURE_WAKEUP_MODE_RESERVE41_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE41_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE41_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE42_FLAG) == GESTURE_WAKEUP_MODE_RESERVE42_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE42_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE42_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE43_FLAG) == GESTURE_WAKEUP_MODE_RESERVE43_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE43_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE43_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE44_FLAG) == GESTURE_WAKEUP_MODE_RESERVE44_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE44_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE44_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE45_FLAG) == GESTURE_WAKEUP_MODE_RESERVE45_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE45_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE45_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE46_FLAG) == GESTURE_WAKEUP_MODE_RESERVE46_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE46_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE46_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE47_FLAG) == GESTURE_WAKEUP_MODE_RESERVE47_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE47_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE47_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE48_FLAG) == GESTURE_WAKEUP_MODE_RESERVE48_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE48_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE48_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE49_FLAG) == GESTURE_WAKEUP_MODE_RESERVE49_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE49_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE49_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE50_FLAG) == GESTURE_WAKEUP_MODE_RESERVE50_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE50_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE50_FLAG);
		}

		if ((nWakeupMode[1] & GESTURE_WAKEUP_MODE_RESERVE51_FLAG) == GESTURE_WAKEUP_MODE_RESERVE51_FLAG) {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] | GESTURE_WAKEUP_MODE_RESERVE51_FLAG;
		} else {
			g_GestureWakeupMode[1] = g_GestureWakeupMode[1] & (~GESTURE_WAKEUP_MODE_RESERVE51_FLAG);
		}
#endif

		DBG("g_GestureWakeupMode = 0x%x, 0x%x\n", g_GestureWakeupMode[0], g_GestureWakeupMode[1]);
	}

	return nCount;
}

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE

ssize_t DrvMainProcfsGestureDebugModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	DBG("g_GestureDebugMode = 0x%x\n", g_GestureDebugMode);

	nLength = sprintf(pBuffer, "%d", g_GestureDebugMode);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsGestureDebugModeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u8 ucGestureMode[2];
	u8 i;
	char *pCh;

	if (pBuffer != NULL) {
		i = 0;
		while ((pCh = strsep((char **)&pBuffer, " , ")) && (i < 2)) {
			DBG("pCh = %s\n", pCh);

			ucGestureMode[i] = DrvCommonConvertCharToHexDigit(pCh, strlen(pCh));

			DBG("ucGestureMode[%d] = 0x%04X\n", i, ucGestureMode[i]);
			i++;
		}

		g_GestureDebugMode = ucGestureMode[0];
		g_GestureDebugFlag = ucGestureMode[1];

		DBG("Gesture flag = 0x%x\n", g_GestureDebugFlag);

		if (g_GestureDebugMode == 0x01) {
			DrvIcFwLyrOpenGestureDebugMode(g_GestureDebugFlag);


			input_report_key(g_InputDevice, KEY_POWER, 1);
			input_sync(g_InputDevice);

			input_report_key(g_InputDevice, KEY_POWER, 0);
			input_sync(g_InputDevice);
		} else if (g_GestureDebugMode == 0x00) {
			DrvIcFwLyrCloseGestureDebugMode();
		} else {
			DBG("*** Undefined Gesture Debug Mode ***\n");
		}
	}

	return nCount;
}

ssize_t DrvMainKObjectGestureDebugShow(struct kobject *pKObj, struct kobj_attribute *pAttr, char *pBuf)
{
	u32 i = 0;
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);

	if (strcmp(pAttr->attr.name, "gesture_debug") == 0) {
		if (g_LogGestureDebug != NULL) {
			DBG("g_LogGestureDebug[0]=%x, g_LogGestureDebug[1]=%x\n", g_LogGestureDebug[0], g_LogGestureDebug[1]);
			DBG("g_LogGestureDebug[2]=%x, g_LogGestureDebug[3]=%x\n", g_LogGestureDebug[2], g_LogGestureDebug[3]);
			DBG("g_LogGestureDebug[4]=%x, g_LogGestureDebug[5]=%x\n", g_LogGestureDebug[4], g_LogGestureDebug[5]);

			if (g_LogGestureDebug[0] == 0xA7 && g_LogGestureDebug[3] == 0x51) {
				for (i = 0; i < 0x80; i++) {
					pBuf[i] = g_LogGestureDebug[i];
				}

				nLength = 0x80;
				DBG("nLength = %d\n", nLength);
			} else {
				DBG("CURRENT MODE IS NOT GESTURE DEBUG MODE/WRONG GESTURE DEBUG MODE HEADER\n");
			}
		} else {
			DBG("g_LogGestureDebug is NULL\n");
		}
	} else {
		DBG("pAttr->attr.name = %s \n", pAttr->attr.name);
	}

	return nLength;
}

ssize_t DrvMainKObjectGestureDebugStore(struct kobject *pKObj, struct kobj_attribute *pAttr, const char *pBuf, size_t nCount)
{
	DBG("*** %s() ***\n", __func__);
/*
	if (strcmp(pAttr->attr.name, "packet") == 0) {

	}
*/
	return nCount;
}

static struct kobj_attribute gesture_attr = __ATTR(gesture_debug, 0666, DrvMainKObjectGestureDebugShow, DrvMainKObjectGestureDebugStore);

/* Create a group of attributes so that we can create and destroy them all at once. */
static struct attribute *gestureattrs[] = {
	&gesture_attr.attr,
	NULL, 	/* need to NULL terminate the list of attributes */
};

/*
 * An unnamed attribute group will put all of the attributes directly in
 * the kobject directory. If we specify a name, a subdirectory will be
 * created for the attributes with the directory being the name of the
 * attribute group.
 */
struct attribute_group gestureattr_group = {
	.attrs = gestureattrs,
};

#endif

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE

ssize_t DrvMainProcfsGestureInforModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u8 szOut[GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH*5] = {0}, szValue[10] = {0};
	u32 szLogGestureInfo[GESTURE_WAKEUP_INFORMATION_PACKET_LENGTH] = {0};
	u32 i = 0;
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	_gLogGestureCount = 0;
	if (_gLogGestureInforType == FIRMWARE_GESTURE_INFORMATION_MODE_A) {
		for (i = 0; i < 2; i++) {
			szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[4 + i];
			_gLogGestureCount++;
		}

		for (i = 2; i < 8; i++) {
			szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[4 + i];
			_gLogGestureCount++;
		}
	} else if (_gLogGestureInforType == FIRMWARE_GESTURE_INFORMATION_MODE_B) {
		for (i = 0; i < 2; i++) {
			szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[4 + i];
			_gLogGestureCount++;
		}

		for (i = 0; i < g_LogGestureInfor[5]*2 ; i++) {
			szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[12 + i];
			_gLogGestureCount++;
		}
	} else if (_gLogGestureInforType == FIRMWARE_GESTURE_INFORMATION_MODE_C) {
		for (i = 0; i < 6; i++) {
			szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[i];
			_gLogGestureCount++;
		}

		for (i = 6; i < 86; i++) {
			szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[i];
			_gLogGestureCount++;
		}

		szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[86];
		_gLogGestureCount++;
		szLogGestureInfo[_gLogGestureCount] = g_LogGestureInfor[87];
		_gLogGestureCount++;
	} else {
		DBG("*** Undefined GESTURE INFORMATION MODE ***\n");
	}

	for (i = 0; i < _gLogGestureCount; i++) {
		sprintf(szValue, "%d", szLogGestureInfo[i]);
		strcat(szOut, szValue);
		strcat(szOut, ", ");
	}

	nLength = sprintf(pBuffer, "%s\n", szOut);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsGestureInforModeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nMode;

	DBG("*** %s() ***\n", __func__);

	if (pBuffer != NULL) {
		sscanf(pBuffer, "%x", &nMode);
		_gLogGestureInforType = nMode;
	}

	DBG("*** _gLogGestureInforType type = 0x%x ***\n", _gLogGestureInforType);

	return nCount;
}

#endif

#endif

/*--------------------------------------------------------------------------*/

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
ssize_t DrvMainProcfsReportRateRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	struct timeval tEndTime;
	suseconds_t nStartTime, nEndTime, nElapsedTime;
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);

	DBG("g_InterruptCount = %d, g_ValidTouchCount = %d\n", g_InterruptCount, g_ValidTouchCount);


	if (*pPos != 0) {
		return 0;
	}


	do_gettimeofday(&tEndTime);

	nStartTime = g_StartTime.tv_sec + g_StartTime.tv_usec/1000000;
	nEndTime = tEndTime.tv_sec + tEndTime.tv_usec/1000000;

	nElapsedTime = nEndTime - nStartTime;

	DBG("Start time : %lu sec, %lu msec\n", g_StartTime.tv_sec, g_StartTime.tv_usec);
	DBG("End time : %lu sec, %lu msec\n", tEndTime.tv_sec, tEndTime.tv_usec);

	DBG("Elapsed time : %lu sec\n", nElapsedTime);


	if (nElapsedTime != 0) {
		g_InterruptReportRate = g_InterruptCount / nElapsedTime;
		g_ValidTouchReportRate = g_ValidTouchCount / nElapsedTime;
	} else {
		g_InterruptReportRate = 0;
		g_ValidTouchReportRate = 0;
	}

	DBG("g_InterruptReportRate = %d, g_ValidTouchReportRate = %d\n", g_InterruptReportRate, g_ValidTouchReportRate);

	g_InterruptCount = 0;
	g_ValidTouchCount = 0;

	nLength = sprintf(pBuffer, "%d, %d", g_InterruptReportRate, g_ValidTouchReportRate);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsReportRateWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	DBG("*** %s() ***\n", __func__);

	if (pBuffer != NULL) {
		sscanf(pBuffer, "%d", &g_IsEnableReportRate);

		DBG("g_IsEnableReportRate = %d\n", g_IsEnableReportRate);

		g_InterruptCount = 0;
		g_ValidTouchCount = 0;
	}

	return nCount;
}
#endif

/*--------------------------------------------------------------------------*/

#ifdef CONFIG_ENABLE_GLOVE_MODE
ssize_t DrvMainProcfsGloveModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;
	u8 ucGloveMode = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	if (g_ChipType == CHIP_TYPE_MSG28XX) {
		DrvPlatformLyrDisableFingerTouchReport();

		DrvIcFwLyrGetGloveInfo(&ucGloveMode);

		DrvPlatformLyrEnableFingerTouchReport();

		DBG("Glove Mode = 0x%x\n", ucGloveMode);

		nLength = sprintf(pBuffer, "%x", ucGloveMode);
	}

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsGloveModeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nGloveMode = 0;
	u32 i = 0;
	char *pCh = NULL;

	DBG("*** %s() ***\n", __func__);

	if (pBuffer != NULL) {
		i = 0;
		while ((pCh = strsep((char **)&pBuffer, ", ")) && (i < 1)) {
			DBG("pCh = %s\n", pCh);

			nGloveMode = DrvCommonConvertCharToHexDigit(pCh, strlen(pCh));

			i++;
		}

		DBG("Glove Mode = 0x%x\n", nGloveMode);

		DrvPlatformLyrDisableFingerTouchReport();

		if (nGloveMode == 0x01) {
			DrvIcFwLyrOpenGloveMode();
			g_IsEnableGloveMode = 1;
		} else if (nGloveMode == 0x00) {
			DrvIcFwLyrCloseGloveMode();
			g_IsEnableGloveMode = 0;
		} else {
			DBG("*** Undefined Glove Mode ***\n");
		}
		DBG("g_IsEnableGloveMode = 0x%x\n", g_IsEnableGloveMode);

		DrvPlatformLyrEnableFingerTouchReport();
	}

	return nCount;
}

ssize_t DrvMainProcfsOpenGloveModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	if (g_ChipType == CHIP_TYPE_MSG28XX) {
		DrvPlatformLyrDisableFingerTouchReport();

		DrvIcFwLyrOpenGloveMode();
		g_IsEnableGloveMode = 1;

		DrvPlatformLyrEnableFingerTouchReport();
	}
	DBG("g_IsEnableGloveMode = 0x%x\n", g_IsEnableGloveMode);

	*pPos += nLength;

	return nLength;
}

ssize_t DrvMainProcfsCloseGloveModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos)
{
	u32 nLength = 0;

	DBG("*** %s() ***\n", __func__);


	if (*pPos != 0) {
		return 0;
	}

	if (g_ChipType == CHIP_TYPE_MSG28XX) {
		DrvPlatformLyrDisableFingerTouchReport();

		DrvIcFwLyrCloseGloveMode();
		g_IsEnableGloveMode = 0;

		DrvPlatformLyrEnableFingerTouchReport();
	}
	DBG("g_IsEnableGloveMode = 0x%x\n", g_IsEnableGloveMode);

	*pPos += nLength;

	return nLength;
}
#endif


#ifdef CONFIG_ENABLE_HOTKNOT
static long hotknot_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long nRet = 0;

	DBG("*** %s ***\n", __FUNCTION__);
	mutex_lock(&g_HKMutex);
	nRet = HotKnotIoctl(file, cmd, arg);
	mutex_unlock(&g_HKMutex);

	return nRet;
}

static int hotknot_value;

static ssize_t hotknot_value_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", hotknot_value);
}

static ssize_t hotknot_value_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d", &hotknot_value);

	return count;
}

static struct kobj_attribute hotknot_value_attr = __ATTR(hotknot_value, 0666, hotknot_value_show, hotknot_value_store);
static struct attribute *hotknot_attrs[] = {
	&hotknot_value_attr.attr, 0
};

static struct attribute_group hotknot_attr_group = {
	.attrs = hotknot_attrs
};

static const struct file_operations hotknot_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = hotknot_ioctl
};


struct miscdevice hotknot_miscdevice = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hotknot",
	.nodename = "hotknot",
	.mode = 0666,
	.fops = &hotknot_fops
};
#endif



s32 DrvMainTouchDeviceInitialize(void)
{
	s32 nRetVal = 0;
#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
	int nErr;
	struct hwmsen_object tObjPs;
#endif
#endif

	DBG("*** %s() ***\n", __func__);

	_DrvMainCreateProcfsDirEntry();

#ifdef CONFIG_ENABLE_HOTKNOT
	_DrvMainHotknotRegistry();
#endif

#ifdef CONFIG_ENABLE_JNI_INTERFACE
	CreateMsgToolMem();
#endif

#ifdef CONFIG_ENABLE_ITO_MP_TEST
	DrvIcFwLyrCreateMpTestWorkQueue();
#endif

	g_ChipType = DrvIcFwLyrGetChipType();

	if (g_ChipType == 0) {
		SLAVE_I2C_ID_DBBUS = (0xB2>>1);

		g_ChipType = DrvIcFwLyrGetChipType();
	}

	DrvPlatformLyrTouchDeviceResetHw();

	if (g_ChipType != 0) {
		memset(&g_FirmwareInfo, 0x0, sizeof(FirmwareInfo_t));

		DrvIcFwLyrVariableInitialize();

#ifdef CONFIG_ENABLE_CHARGER_DETECTION
		{
			u8 szChargerStatus[20] = {0};

			DrvCommonReadFile("/sys/class/power_supply/battery/status", szChargerStatus, 20);

			DBG("*** Battery Status : %s ***\n", szChargerStatus);

			if (strstr(szChargerStatus, "Charging") != NULL || strstr(szChargerStatus, "Full") != NULL || strstr(szChargerStatus, "Fully charged") != NULL) {
				DrvFwCtrlChargerDetection(1);
			} else {
				DrvFwCtrlChargerDetection(0);
			}
		}
#endif

#ifdef CONFIG_ENABLE_PROXIMITY_DETECTION
#if defined(CONFIG_TOUCH_DRIVER_RUN_ON_SPRD_PLATFORM) || defined(CONFIG_TOUCH_DRIVER_RUN_ON_QCOM_PLATFORM)
		tsps_assist_register_callback("msg2xxx", &DrvPlatformLyrTpPsEnable, &DrvPlatformLyrGetTpPsData);
#elif defined(CONFIG_TOUCH_DRIVER_RUN_ON_MTK_PLATFORM)
		tObjPs.polling = 0;
		tObjPs.sensor_operate = DrvPlatformLyrTpPsOperate;

		if ((nErr = hwmsen_attach(ID_PROXIMITY, &tObjPs)))
			DBG("call hwmsen_attach() failed = %d\n", nErr);
#endif
#endif
	} else {
		nRetVal = -ENODEV;
	}

	return nRetVal;
}

void DrvMainRemoveProcfsDirEntry(void)
{
	DBG("*** %s() ***\n", __func__);

#ifdef CONFIG_ENABLE_GLOVE_MODE
	if (_gProcGloveModeEntry != NULL) {
		remove_proc_entry(PROC_NODE_GLOVE_MODE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_GLOVE_MODE);
	}

	if (_gProcOpenGloveModeEntry != NULL) {
		remove_proc_entry(PROC_NODE_OPEN_GLOVE_MODE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_OPEN_GLOVE_MODE);
	}

	if (_gProcCloseGloveModeEntry != NULL) {
		remove_proc_entry(PROC_NODE_CLOSE_GLOVE_MODE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_CLOSE_GLOVE_MODE);
	}
#endif

#ifdef CONFIG_ENABLE_JNI_INTERFACE
	if (_gProcJniMethodEntry != NULL) {
		remove_proc_entry(PROC_NODE_JNI_NODE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_JNI_NODE);
	}
#endif

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
	if (_gProcReportRateEntry != NULL) {
		remove_proc_entry(PROC_NODE_REPORT_RATE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_REPORT_RATE);
	}
#endif

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	if (_gProcGestureWakeupModeEntry != NULL) {
		remove_proc_entry(PROC_NODE_GESTURE_WAKEUP_MODE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_GESTURE_WAKEUP_MODE);
	}
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
	if (_gProcGestureDebugModeEntry != NULL) {
		remove_proc_entry(PROC_NODE_GESTURE_DEBUG_MODE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_GESTURE_DEBUG_MODE);
	}
#endif
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
	if (_gProcGestureInforModeEntry != NULL) {
		remove_proc_entry(PROC_NODE_GESTURE_INFORMATION_MODE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_GESTURE_INFORMATION_MODE);
	}
#endif
#endif

	if (_gProcFirmwareModeEntry != NULL) {
		remove_proc_entry(PROC_NODE_FIRMWARE_MODE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_MODE);
	}

	if (_gProcFirmwareSensorEntry != NULL) {
		remove_proc_entry(PROC_NODE_FIRMWARE_SENSOR, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_SENSOR);
	}

	if (_gProcFirmwarePacketHeaderEntry != NULL) {
		remove_proc_entry(PROC_NODE_FIRMWARE_PACKET_HEADER, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_PACKET_HEADER);
	}

	if (_gProcQueryFeatureSupportStatusEntry != NULL) {
		remove_proc_entry(PROC_NODE_QUERY_FEATURE_SUPPORT_STATUS, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_QUERY_FEATURE_SUPPORT_STATUS);
	}

	if (_gProcChangeFeatureSupportStatusEntry != NULL) {
		remove_proc_entry(PROC_NODE_CHANGE_FEATURE_SUPPORT_STATUS, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_CHANGE_FEATURE_SUPPORT_STATUS);
	}

#ifdef CONFIG_ENABLE_ITO_MP_TEST
	if (_gProcMpTestEntry != NULL) {
		remove_proc_entry(PROC_NODE_MP_TEST, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_MP_TEST);
	}

	if (_gProcMpTestLogEntry != NULL) {
		remove_proc_entry(PROC_NODE_MP_TEST_LOG, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_MP_TEST_LOG);
	}

	if (_gProcMpTestFailChannelEntry != NULL) {
		remove_proc_entry(PROC_NODE_MP_TEST_FAIL_CHANNEL, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_MP_TEST_FAIL_CHANNEL);
	}

	if (_gProcMpTestScopeEntry != NULL) {
		remove_proc_entry(PROC_NODE_MP_TEST_SCOPE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_MP_TEST_SCOPE);
	}
#endif

	if (_gProcFirmwareSetDQMemValueEntry != NULL) {
		remove_proc_entry(PROC_NODE_FIRMWARE_SET_DQMEM_VALUE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_SET_DQMEM_VALUE);
	}

	if (_gProcFirmwareSmBusDebugEntry != NULL) {
		remove_proc_entry(PROC_NODE_FIRMWARE_SMBUS_DEBUG, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_SMBUS_DEBUG);
	}

	if (_gProcFirmwareSetDebugValueEntry != NULL) {
		remove_proc_entry(PROC_NODE_FIRMWARE_SET_DEBUG_VALUE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_SET_DEBUG_VALUE);
	}

	if (_gProcFirmwareDebugEntry != NULL) {
		remove_proc_entry(PROC_NODE_FIRMWARE_DEBUG, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_DEBUG);
	}

	if (_gProcSdCardFirmwareUpdateEntry != NULL) {
		remove_proc_entry(PROC_NODE_SD_CARD_FIRMWARE_UPDATE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_SD_CARD_FIRMWARE_UPDATE);
	}

	if (_gProcSeLinuxLimitFirmwareUpdateEntry != NULL) {
		remove_proc_entry(PROC_NODE_SELINUX_LIMIT_FIRMWARE_UPDATE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_SELINUX_LIMIT_FIRMWARE_UPDATE);
	}

	if (_gProcDeviceDriverVersionEntry != NULL) {
		remove_proc_entry(PROC_NODE_DEVICE_DRIVER_VERSION, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_DEVICE_DRIVER_VERSION);
	}

	if (_gProcPlatformFirmwareVersionEntry != NULL) {
		remove_proc_entry(PROC_NODE_PLATFORM_FIRMWARE_VERSION, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_PLATFORM_FIRMWARE_VERSION);
	}

	if (_gProcCustomerFirmwareVersionEntry != NULL) {
		remove_proc_entry(PROC_NODE_CUSTOMER_FIRMWARE_VERSION, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_CUSTOMER_FIRMWARE_VERSION);
	}

	if (_gProcApkFirmwareUpdateEntry != NULL) {
		remove_proc_entry(PROC_NODE_FIRMWARE_UPDATE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_UPDATE);
	}

	if (_gProcFirmwareDataEntry != NULL) {
		remove_proc_entry(PROC_NODE_FIRMWARE_DATA, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_DATA);
	}

	if (_gProcChipTypeEntry != NULL) {
		remove_proc_entry(PROC_NODE_CHIP_TYPE, _gProcDeviceEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_CHIP_TYPE);
	}

	if (_gProcDeviceEntry != NULL) {
		remove_proc_entry(PROC_NODE_DEVICE, _gProcMsTouchScreenMsg20xxEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_DEVICE);
	}

	if (_gProcMsTouchScreenMsg20xxEntry != NULL) {
		remove_proc_entry(PROC_NODE_MS_TOUCHSCREEN_MSG20XX, _gProcClassEntry);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_MS_TOUCHSCREEN_MSG20XX);
	}

	if (_gProcClassEntry != NULL) {
		remove_proc_entry(PROC_NODE_CLASS, NULL);
		DBG("Remove procfs file node(%s) OK!\n", PROC_NODE_CLASS);
	}
}

static s32 _DrvMainCreateProcfsDirEntry(void)
{
	s32 nRetVal = 0;

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
	u8 *pGesturePath = NULL;
#endif
#endif
	u8 *pDevicePath = NULL;

	DBG("*** %s() ***\n", __func__);

	_gProcClassEntry = proc_mkdir(PROC_NODE_CLASS, NULL);

	_gProcMsTouchScreenMsg20xxEntry = proc_mkdir(PROC_NODE_MS_TOUCHSCREEN_MSG20XX, _gProcClassEntry);

	_gProcDeviceEntry = proc_mkdir(PROC_NODE_DEVICE, _gProcMsTouchScreenMsg20xxEntry);

	_gProcChipTypeEntry = proc_create(PROC_NODE_CHIP_TYPE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcChipType);
	if (NULL == _gProcChipTypeEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_CHIP_TYPE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_CHIP_TYPE);
	}

	_gProcFirmwareDataEntry = proc_create(PROC_NODE_FIRMWARE_DATA, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcFirmwareData);
	if (NULL == _gProcFirmwareDataEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_FIRMWARE_DATA);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_DATA);
	}

	_gProcApkFirmwareUpdateEntry = proc_create(PROC_NODE_FIRMWARE_UPDATE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcApkFirmwareUpdate);
	if (NULL == _gProcApkFirmwareUpdateEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_FIRMWARE_UPDATE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_UPDATE);
	}

	_gProcCustomerFirmwareVersionEntry = proc_create(PROC_NODE_CUSTOMER_FIRMWARE_VERSION, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcCustomerFirmwareVersion);
	if (NULL == _gProcCustomerFirmwareVersionEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_CUSTOMER_FIRMWARE_VERSION);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_CUSTOMER_FIRMWARE_VERSION);
	}

	_gProcPlatformFirmwareVersionEntry = proc_create(PROC_NODE_PLATFORM_FIRMWARE_VERSION, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcPlatformFirmwareVersion);
	if (NULL == _gProcPlatformFirmwareVersionEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_PLATFORM_FIRMWARE_VERSION);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_PLATFORM_FIRMWARE_VERSION);
	}

	_gProcDeviceDriverVersionEntry = proc_create(PROC_NODE_DEVICE_DRIVER_VERSION, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcDeviceDriverVersion);
	if (NULL == _gProcDeviceDriverVersionEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_DEVICE_DRIVER_VERSION);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_DEVICE_DRIVER_VERSION);
	}

	_gProcSdCardFirmwareUpdateEntry = proc_create(PROC_NODE_SD_CARD_FIRMWARE_UPDATE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcSdCardFirmwareUpdate);
	if (NULL == _gProcSdCardFirmwareUpdateEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_SD_CARD_FIRMWARE_UPDATE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_SD_CARD_FIRMWARE_UPDATE);
	}

	_gProcFirmwareDebugEntry = proc_create(PROC_NODE_FIRMWARE_DEBUG, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcFirmwareDebug);
	if (NULL == _gProcFirmwareDebugEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_FIRMWARE_DEBUG);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_DEBUG);
	}

	_gProcFirmwareSetDebugValueEntry = proc_create(PROC_NODE_FIRMWARE_SET_DEBUG_VALUE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcFirmwareSetDebugValue);
	if (NULL == _gProcFirmwareSetDebugValueEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_FIRMWARE_SET_DEBUG_VALUE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_SET_DEBUG_VALUE);
	}

	_gProcFirmwareSmBusDebugEntry = proc_create(PROC_NODE_FIRMWARE_SMBUS_DEBUG, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcFirmwareSmBusDebug);
	if (NULL == _gProcFirmwareSmBusDebugEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_FIRMWARE_SMBUS_DEBUG);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_SMBUS_DEBUG);
	}

	_gProcFirmwareSetDQMemValueEntry = proc_create(PROC_NODE_FIRMWARE_SET_DQMEM_VALUE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcFirmwareSetDQMemValue);
	if (NULL == _gProcFirmwareSetDQMemValueEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_FIRMWARE_SET_DQMEM_VALUE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_SET_DQMEM_VALUE);
	}

#ifdef CONFIG_ENABLE_ITO_MP_TEST
	_gProcMpTestEntry = proc_create(PROC_NODE_MP_TEST, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcMpTest);
	if (NULL == _gProcMpTestEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_MP_TEST);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_MP_TEST);
	}

	_gProcMpTestLogEntry = proc_create(PROC_NODE_MP_TEST_LOG, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcMpTestLog);
	if (NULL == _gProcMpTestLogEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_MP_TEST_LOG);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_MP_TEST_LOG);
	}

	_gProcMpTestFailChannelEntry = proc_create(PROC_NODE_MP_TEST_FAIL_CHANNEL, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcMpTestFailChannel);
	if (NULL == _gProcMpTestFailChannelEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_MP_TEST_FAIL_CHANNEL);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_MP_TEST_FAIL_CHANNEL);
	}

	_gProcMpTestScopeEntry = proc_create(PROC_NODE_MP_TEST_SCOPE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcMpTestScope);
	if (NULL == _gProcMpTestScopeEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_MP_TEST_SCOPE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_MP_TEST_SCOPE);
	}
#endif


	_gProcFirmwareModeEntry = proc_create(PROC_NODE_FIRMWARE_MODE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcFirmwareMode);
	if (NULL == _gProcFirmwareModeEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_FIRMWARE_MODE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_MODE);
	}

	_gProcFirmwareSensorEntry = proc_create(PROC_NODE_FIRMWARE_SENSOR, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcFirmwareSensor);
	if (NULL == _gProcFirmwareSensorEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_FIRMWARE_SENSOR);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_SENSOR);
	}

	_gProcFirmwarePacketHeaderEntry = proc_create(PROC_NODE_FIRMWARE_PACKET_HEADER, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcFirmwarePacketHeader);
	if (NULL == _gProcFirmwarePacketHeaderEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_FIRMWARE_PACKET_HEADER);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_FIRMWARE_PACKET_HEADER);
	}

	/* create a kset with the name of "kset_example" which is located under /sys/kernel/ */
	g_TouchKSet = kset_create_and_add("kset_example", NULL, kernel_kobj);
	if (!g_TouchKSet) {
		DBG("*** kset_create_and_add() failed, nRetVal = %d ***\n", nRetVal);

		nRetVal = -ENOMEM;
	}

	g_TouchKObj = kobject_create();
	if (!g_TouchKObj) {
		DBG("*** kobject_create() failed, nRetVal = %d ***\n", nRetVal);

		nRetVal = -ENOMEM;
		kset_unregister(g_TouchKSet);
		g_TouchKSet = NULL;
	}

	g_TouchKObj->kset = g_TouchKSet;

	nRetVal = kobject_add(g_TouchKObj, NULL, "%s", "kobject_example");
	if (nRetVal != 0) {
		DBG("*** kobject_add() failed, nRetVal = %d ***\n", nRetVal);

		kobject_put(g_TouchKObj);
		g_TouchKObj = NULL;
		kset_unregister(g_TouchKSet);
		g_TouchKSet = NULL;
	}

	/* create the files associated with this kobject */
	nRetVal = sysfs_create_group(g_TouchKObj, &attr_group);
	if (nRetVal != 0) {
		DBG("*** sysfs_create_file() failed, nRetVal = %d ***\n", nRetVal);

		kobject_put(g_TouchKObj);
		g_TouchKObj = NULL;
		kset_unregister(g_TouchKSet);
		g_TouchKSet = NULL;
	}

	pDevicePath = kobject_get_path(g_TouchKObj, GFP_KERNEL);
	DBG("DEVPATH = %s\n", pDevicePath);


	_gProcQueryFeatureSupportStatusEntry = proc_create(PROC_NODE_QUERY_FEATURE_SUPPORT_STATUS, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcQueryFeatureSupportStatus);
	if (NULL == _gProcQueryFeatureSupportStatusEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_QUERY_FEATURE_SUPPORT_STATUS);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_QUERY_FEATURE_SUPPORT_STATUS);
	}

	_gProcChangeFeatureSupportStatusEntry = proc_create(PROC_NODE_CHANGE_FEATURE_SUPPORT_STATUS, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcChangeFeatureSupportStatus);
	if (NULL == _gProcChangeFeatureSupportStatusEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_CHANGE_FEATURE_SUPPORT_STATUS);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_CHANGE_FEATURE_SUPPORT_STATUS);
	}

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
	_gProcGestureWakeupModeEntry = proc_create(PROC_NODE_GESTURE_WAKEUP_MODE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcGestureWakeupMode);
	if (NULL == _gProcGestureWakeupModeEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_GESTURE_WAKEUP_MODE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_GESTURE_WAKEUP_MODE);
	}

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
	_gProcGestureDebugModeEntry = proc_create(PROC_NODE_GESTURE_DEBUG_MODE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcGestureDebugMode);
	if (NULL == _gProcGestureDebugModeEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_GESTURE_DEBUG_MODE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_GESTURE_DEBUG_MODE);
	}

	/* create a kset with the name of "kset_gesture" which is located under /sys/kernel/ */
	g_GestureKSet = kset_create_and_add("kset_gesture", NULL, kernel_kobj);
	if (!g_GestureKSet) {
		DBG("*** kset_create_and_add() failed, nRetVal = %d ***\n", nRetVal);
		nRetVal = -ENOMEM;
	}

	g_GestureKObj = kobject_create();
	if (!g_GestureKObj) {
		DBG("*** kobject_create() failed, nRetVal = %d ***\n", nRetVal);

		nRetVal = -ENOMEM;
		kset_unregister(g_GestureKSet);
		g_GestureKSet = NULL;
	}

	g_GestureKObj->kset = g_GestureKSet;

	nRetVal = kobject_add(g_GestureKObj, NULL, "%s", "kobject_gesture");
	if (nRetVal != 0) {
		DBG("*** kobject_add() failed, nRetVal = %d ***\n", nRetVal);

		kobject_put(g_GestureKObj);
		g_GestureKObj = NULL;
		kset_unregister(g_GestureKSet);
		g_GestureKSet = NULL;
	}

	/* create the files associated with this g_GestureKObj */
	nRetVal = sysfs_create_group(g_GestureKObj, &gestureattr_group);
	if (nRetVal != 0) {
		DBG("*** sysfs_create_file() failed, nRetVal = %d ***\n", nRetVal);

		kobject_put(g_GestureKObj);
		g_GestureKObj = NULL;
		kset_unregister(g_GestureKSet);
		g_GestureKSet = NULL;
	}

	pGesturePath = kobject_get_path(g_GestureKObj, GFP_KERNEL);
	DBG("DEVPATH = %s\n", pGesturePath);
#endif

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
	_gProcGestureInforModeEntry = proc_create(PROC_NODE_GESTURE_INFORMATION_MODE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcGestureInforMode);
	if (NULL == _gProcGestureInforModeEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_GESTURE_INFORMATION_MODE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_GESTURE_INFORMATION_MODE);
	}
#endif
#endif

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
	_gProcReportRateEntry = proc_create(PROC_NODE_REPORT_RATE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcReportRate);
	if (NULL == _gProcReportRateEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_REPORT_RATE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_REPORT_RATE);
	}
#endif

#ifdef CONFIG_ENABLE_GLOVE_MODE
	_gProcGloveModeEntry = proc_create(PROC_NODE_GLOVE_MODE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcGloveMode);
	if (NULL == _gProcGloveModeEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_GLOVE_MODE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_GLOVE_MODE);
	}

	_gProcOpenGloveModeEntry = proc_create(PROC_NODE_OPEN_GLOVE_MODE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcOpenGloveMode);
	if (NULL == _gProcOpenGloveModeEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_OPEN_GLOVE_MODE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_OPEN_GLOVE_MODE);
	}

	_gProcCloseGloveModeEntry = proc_create(PROC_NODE_CLOSE_GLOVE_MODE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcCloseGloveMode);
	if (NULL == _gProcCloseGloveModeEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_CLOSE_GLOVE_MODE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_CLOSE_GLOVE_MODE);
	}
#endif

#ifdef CONFIG_ENABLE_JNI_INTERFACE
	_gProcJniMethodEntry = proc_create(PROC_NODE_JNI_NODE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcJniMethod);
	if (NULL == _gProcJniMethodEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_JNI_NODE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_JNI_NODE);
	}
#endif

	_gProcSeLinuxLimitFirmwareUpdateEntry = proc_create(PROC_NODE_SELINUX_LIMIT_FIRMWARE_UPDATE, PROCFS_AUTHORITY, _gProcDeviceEntry, &_gProcSeLinuxLimitFirmwareUpdate);
	if (NULL == _gProcSeLinuxLimitFirmwareUpdateEntry) {
		DBG("Failed to create procfs file node(%s)!\n", PROC_NODE_SELINUX_LIMIT_FIRMWARE_UPDATE);
	} else {
		DBG("Create procfs file node(%s) OK!\n", PROC_NODE_SELINUX_LIMIT_FIRMWARE_UPDATE);
	}

	return nRetVal;
}

ifdef CONFIG_ENABLE_HOTKNOT

static s32 _DrvMainHotknotRegistry(void)
{
	s32 nRetVal = 0;

	nRetVal = misc_register(&hotknot_miscdevice);
	if (nRetVal < 0) {
		DBG("Failed to register misc device. Err:%d\n", nRetVal);
	}
	DBG("*** Misc device registered ***\n");

	nRetVal = sysfs_create_group(&hotknot_miscdevice.this_device->kobj, &hotknot_attr_group);
	if (nRetVal < 0) {
		DBG("Failed to create attribute group. Err:%d\n", nRetVal);

	}
	DBG("*** Attribute group created ***\n");

	mutex_init(&g_HKMutex);
	mutex_init(&g_QMutex);
	CreateQueue();
	CreateHotKnotMem();

	return nRetVal;
}
#endif
