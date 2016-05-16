/*
 *
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * @file	mstar_drv_main.h
 *
 * @brief   This file defines the interface of touch screen
 *
 *
 */

#ifndef __MSTAR_DRV_MAIN_H__
#define __MSTAR_DRV_MAIN_H__

/*--------------------------------------------------------------------------*/
/* INCLUDE FILE															 */
/*--------------------------------------------------------------------------*/

#include "mstar_drv_common.h"

/*--------------------------------------------------------------------------*/
/* PREPROCESSOR CONSTANT DEFINITION										 */
/*--------------------------------------------------------------------------*/

#define PROC_NODE_CLASS					   "class"
#define PROC_NODE_MS_TOUCHSCREEN_MSG20XX	  "ms-touchscreen-msg20xx"
#define PROC_NODE_DEVICE					  "device"
#define PROC_NODE_CHIP_TYPE				   "chip_type"
#define PROC_NODE_FIRMWARE_DATA			   "data"
#define PROC_NODE_FIRMWARE_UPDATE			 "update"
#define PROC_NODE_CUSTOMER_FIRMWARE_VERSION   "version"
#define PROC_NODE_PLATFORM_FIRMWARE_VERSION   "platform_version"
#define PROC_NODE_DEVICE_DRIVER_VERSION	   "driver_version"
#define PROC_NODE_SD_CARD_FIRMWARE_UPDATE	 "sdcard_update"
#define PROC_NODE_FIRMWARE_DEBUG			  "debug"
#define PROC_NODE_FIRMWARE_SET_DEBUG_VALUE	"set_debug_value"
#define PROC_NODE_FIRMWARE_SMBUS_DEBUG		"smbus_debug"

#define PROC_NODE_FIRMWARE_SET_DQMEM_VALUE	"set_dqmem_value"

#ifdef CONFIG_ENABLE_ITO_MP_TEST
#define PROC_NODE_MP_TEST					 "test"
#define PROC_NODE_MP_TEST_LOG				 "test_log"
#define PROC_NODE_MP_TEST_FAIL_CHANNEL		"test_fail_channel"
#define PROC_NODE_MP_TEST_SCOPE			   "test_scope"
#endif

#define PROC_NODE_FIRMWARE_MODE			   "mode"
#define PROC_NODE_FIRMWARE_SENSOR			 "sensor"
#define PROC_NODE_FIRMWARE_PACKET_HEADER	  "header"

#define PROC_NODE_QUERY_FEATURE_SUPPORT_STATUS   "query_feature_support_status"
#define PROC_NODE_CHANGE_FEATURE_SUPPORT_STATUS  "change_feature_support_status"

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
#define PROC_NODE_GESTURE_WAKEUP_MODE		 "gesture_wakeup_mode"
#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
#define PROC_NODE_GESTURE_DEBUG_MODE		  "gesture_debug"
#endif
#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
#define PROC_NODE_GESTURE_INFORMATION_MODE	"gesture_infor"
#endif
#endif

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
#define PROC_NODE_REPORT_RATE				 "report_rate"
#endif

#ifdef CONFIG_ENABLE_GLOVE_MODE
#define PROC_NODE_GLOVE_MODE				  "glove_mode"
#define PROC_NODE_OPEN_GLOVE_MODE			 "open_glove_mode"
#define PROC_NODE_CLOSE_GLOVE_MODE			"close_glove_mode"
#endif

#ifdef CONFIG_ENABLE_JNI_INTERFACE
#define PROC_NODE_JNI_NODE					"msgtool"
#endif

#define PROC_NODE_SELINUX_LIMIT_FIRMWARE_UPDATE	 "selinux_limit_update"


/*--------------------------------------------------------------------------*/
/* PREPROCESSOR MACRO DEFINITION											*/
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* DATA TYPE DEFINITION													 */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* GLOBAL FUNCTION DECLARATION											  */
/*--------------------------------------------------------------------------*/

extern ssize_t DrvMainProcfsChipTypeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsChipTypeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareDataRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareDataWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareUpdateRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareUpdateWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsCustomerFirmwareVersionRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsCustomerFirmwareVersionWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsPlatformFirmwareVersionRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsPlatformFirmwareVersionWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsDeviceDriverVersionRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsDeviceDriverVersionWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsSdCardFirmwareUpdateRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsSdCardFirmwareUpdateWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareDebugRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareDebugWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareSetDebugValueRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareSetDebugValueWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareSmBusDebugRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareSmBusDebugWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);

extern ssize_t DrvMainProcfsFirmwareSetDQMemValueRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareSetDQMemValueWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);

#ifdef CONFIG_ENABLE_ITO_MP_TEST
extern ssize_t DrvMainProcfsMpTestRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsMpTestWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsMpTestLogRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsMpTestLogWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsMpTestFailChannelRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsMpTestFailChannelWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsMpTestScopeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsMpTestScopeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
#endif

extern ssize_t DrvMainProcfsFirmwareModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareModeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareSensorRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwareSensorWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwarePacketHeaderRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsFirmwarePacketHeaderWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainKObjectPacketShow(struct kobject *pKObj, struct kobj_attribute *pAttr, char *pBuf);
extern ssize_t DrvMainKObjectPacketStore(struct kobject *pKObj, struct kobj_attribute *pAttr, const char *pBuf, size_t nCount);

extern ssize_t DrvMainProcfsQueryFeatureSupportStatusRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsQueryFeatureSupportStatusWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsChangeFeatureSupportStatusRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsChangeFeatureSupportStatusWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);

#ifdef CONFIG_ENABLE_GESTURE_WAKEUP
extern ssize_t DrvMainProcfsGestureWakeupModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsGestureWakeupModeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);

#ifdef CONFIG_ENABLE_GESTURE_DEBUG_MODE
extern ssize_t DrvMainProcfsGestureDebugModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsGestureDebugModeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainKObjectGestureDebugShow(struct kobject *pKObj, struct kobj_attribute *pAttr, char *pBuf);
extern ssize_t DrvMainKObjectGestureDebugStore(struct kobject *pKObj, struct kobj_attribute *pAttr, const char *pBuf, size_t nCount);
#endif

#ifdef CONFIG_ENABLE_GESTURE_INFORMATION_MODE
extern ssize_t DrvMainProcfsGestureInforModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsGestureInforModeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
#endif
#endif

#ifdef CONFIG_ENABLE_COUNT_REPORT_RATE
extern ssize_t DrvMainProcfsReportRateRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsReportRateWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
#endif

#ifdef CONFIG_ENABLE_GLOVE_MODE
extern ssize_t DrvMainProcfsGloveModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsGloveModeWrite(struct file *pFile, const char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsOpenGloveModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
extern ssize_t DrvMainProcfsCloseGloveModeRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);
#endif

extern ssize_t DrvMainProcfsSeLinuxLimitFirmwareUpdateRead(struct file *pFile, char __user *pBuffer, size_t nCount, loff_t *pPos);

extern s32 DrvMainTouchDeviceInitialize(void);
extern void DrvMainRemoveProcfsDirEntry(void);

#endif  /* __MSTAR_DRV_MAIN_H__ */
