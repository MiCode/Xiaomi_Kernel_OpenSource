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

#ifndef _VAL_LOG_H_
#define _VAL_LOG_H_

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "MFV_COMMON"    /* /< LOG_TAG "MFV_COMMON" */
#include <utils/Log.h>
#include <cutils/xlog.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MFV_LOG_ERROR           /* /< error */
#ifdef MFV_LOG_ERROR
#define MODULE_MFV_LOGE(...) xlog_printf(ANDROID_LOG_ERROR, "VDO_LOG", __VA_ARGS__)   /* /< show error log */
#define VDO_LOGE(...) xlog_printf(ANDROID_LOG_ERROR, "VDO_LOG", __VA_ARGS__)   /* /< show error log */
#else
#define MODULE_MFV_LOGE(...)           /* /< NOT show error log */
#define VDO_LOGE(...)           /* /< NOT show error log */
#endif

#define MFV_LOG_WARNING         /* /< warning */
#ifdef MFV_LOG_WARNING
#define MODULE_MFV_LOGW(...) xlog_printf(ANDROID_LOG_WARN, "VDO_LOG", __VA_ARGS__)    /* /< show warning log */
#define VDO_LOGW(...) xlog_printf(ANDROID_LOG_WARN, "VDO_LOG", __VA_ARGS__)    /* /< show warning log */
#else
#define MODULE_MFV_LOGW(...)           /* /< NOT show warning log */
#define VDO_LOGW(...)           /* /< NOT show warning log */
#endif

/* #define MFV_LOG_DEBUG         ///< debug information */
#ifdef MFV_LOG_DEBUG
/* /< show debug information log */
#define MODULE_MFV_LOGD(...) xlog_printf(ANDROID_LOG_DEBUG, "VDO_LOG", __VA_ARGS__)
/* /< show debug information log */
#define VDO_LOGD(...) xlog_printf(ANDROID_LOG_DEBUG, "VDO_LOG", __VA_ARGS__)
#else
#define MODULE_MFV_LOGD(...)           /* /< NOT show debug information log */
#define VDO_LOGD(...)           /* /< NOT show debug information log */
#endif

#define MFV_LOG_INFO            /* /< information */
#ifdef MFV_LOG_INFO
#define MODULE_MFV_LOGI(...) xlog_printf(ANDROID_LOG_INFO, "VDO_LOG", __VA_ARGS__)    /* /< show information log */
#define VDO_LOGI(...) xlog_printf(ANDROID_LOG_INFO, "VDO_LOG", __VA_ARGS__)    /* /< show information log */
#else
#define MODULE_MFV_LOGI(...)           /* /< NOT show information log */
#define VDO_LOGI(...)           /* /< NOT show information log */
#endif

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _VAL_LOG_H_ */
