/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef __SYSINFO_H__
#define __SYSINFO_H__

/**************************************************
 * MTK PVR Log Setting
 **************************************************/
#define MTKPVR_TAG "[GPU/PVR]"
#define MTK_LOGE(fmt, args...) \
	pr_err(MTKPVR_TAG"[ERROR]@%s: "fmt"\n", __func__, ##args)
#define MTK_LOGW(fmt, args...) \
	pr_warn(MTKPVR_TAG"[WARN]@%s: "fmt"\n", __func__, ##args)
#define MTK_LOGI(fmt, args...) \
	pr_info(MTKPVR_TAG"[INFO]@%s: "fmt"\n", __func__, ##args)
#define MTK_LOGD(fmt, args...) \
	pr_debug(MTKPVR_TAG"[DEBUG]@%s: "fmt"\n", __func__, ##args)

/*!< System specific poll/timeout details */
#if defined(PVR_LINUX_USING_WORKQUEUES)
#define MAX_HW_TIME_US                              (1000000)
#define DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT     (10000)
#define DEVICES_WATCHDOG_POWER_OFF_SLEEP_TIMEOUT    (3600000)
#define WAIT_TRY_COUNT                              (20000)
#else
#define MAX_HW_TIME_US                              (5000000)
#define DEVICES_WATCHDOG_POWER_ON_SLEEP_TIMEOUT     (10000)
#define DEVICES_WATCHDOG_POWER_OFF_SLEEP_TIMEOUT    (3600000)
#define WAIT_TRY_COUNT                              (100000)
#endif

/* RGX, DISPLAY (external), BUFFER (external) */
#define SYS_DEVICE_COUNT                            (3)
#define SYS_PHYS_HEAP_COUNT                         (1)

#if defined(__linux__)
/* Use the static bus ID for the platform DRM device. */
#if defined(PVR_DRM_DEV_BUS_ID)
#define SYS_RGX_DEV_DRM_BUS_ID                      PVR_DRM_DEV_BUS_ID
#else
#define SYS_RGX_DEV_DRM_BUS_ID                      "platform:pvrsrvkm"
#endif /* defined(PVR_DRM_DEV_BUS_ID) */
#endif

#endif /* __SYSINFO_H__ */
