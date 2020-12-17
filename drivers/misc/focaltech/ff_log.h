/**
 * ${ANDROID_BUILD_TOP}/vendor/focaltech/src/base/focaltech/ff_log.h
 *
 * Copyright (C) 2014-2017 FocalTech Systems Co., Ltd. All Rights Reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 *
**/

#ifndef __FF_LOGGING_H__
#define __FF_LOGGING_H__

/*
 * The default LOG_TAG is 'focaltech', using these two lines to define newtag.
 * # undef LOG_TAG
 * #define LOG_TAG "newtag"
 */
#define LOG_TAG "focaltech"

/*
 * Log level can be used in 'logcat <tag>[:priority]', and also be
 * used in output control while '__FF_EARLY_LOG_LEVEL' is defined.
 */
typedef enum {
    FF_LOG_LEVEL_ALL = 0,
    FF_LOG_LEVEL_VBS = 1, /* Verbose */
    FF_LOG_LEVEL_DBG = 2, /* Debug   */
    FF_LOG_LEVEL_INF = 3, /* Info    */
    FF_LOG_LEVEL_WRN = 4, /* Warning */
    FF_LOG_LEVEL_ERR = 5, /* Error   */
} ff_log_level_t;

/*
 * __FF_EARLY_LOG_LEVEL can be defined as compilation option.
 * default level is FF_LOG_LEVEL_ALL(all the logs will be output).
 */
#ifndef __FF_EARLY_LOG_LEVEL
#define __FF_EARLY_LOG_LEVEL FF_LOG_LEVEL_ALL
#endif

/*
 * Log level can be runtime configurable.
 */
extern ff_log_level_t g_log_level; /* = __FF_EARLY_LOG_LEVEL */

/*
 * Using the following five macros for conveniently logging.
 */
#define FF_LOGV(...)                                               \
	do {                                                           \
		if (g_log_level <= FF_LOG_LEVEL_VBS) {                     \
			ff_log_printf(FF_LOG_LEVEL_VBS, LOG_TAG, __VA_ARGS__); \
		};                                                         \
	} while (0)

#define FF_LOGD(...)                                               \
	do {                                                           \
		if (g_log_level <= FF_LOG_LEVEL_DBG) {                     \
			ff_log_printf(FF_LOG_LEVEL_DBG, LOG_TAG, __VA_ARGS__); \
		};                                                         \
	} while (0)

#define FF_LOGI(...)                                               \
	do {                                                           \
		if (g_log_level <= FF_LOG_LEVEL_INF) {                     \
			ff_log_printf(FF_LOG_LEVEL_INF, LOG_TAG, __VA_ARGS__); \
		};                                                         \
	} while (0)

#define FF_LOGW(...)                                               \
	do {                                                           \
		if (g_log_level <= FF_LOG_LEVEL_WRN) {                     \
			ff_log_printf(FF_LOG_LEVEL_WRN, LOG_TAG, __VA_ARGS__); \
		};                                                         \
	} while (0)

#define FF_LOGE(format, ...)                                       \
	do {                                                           \
		if (g_log_level <= FF_LOG_LEVEL_ERR) {                     \
			const char *__fname__ = __FILE__, *s = __fname__;      \
			do { if (*s == '/') __fname__ = s + 1; } while (*s++); \
			ff_log_printf(FF_LOG_LEVEL_ERR, LOG_TAG,               \
					"error at %s[%s:%d]: " format, __FUNCTION__,   \
					__fname__, __LINE__, ##__VA_ARGS__);           \
		};                                                         \
	} while (0)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Logging API. Do NOT use it directly but FF_LOF* instead.
 *
 * @params
 *  level: Logging level for logcat.
 *  tag  : Logging tag for logcat.
 *  fmt  : See POSIX printf(..).
 *  ...  : See POSIX printf(..).
 *
 * @return
 *  The number of characters printed, or a negative value if there
 *  was an output error.
 */
int ff_log_printf(ff_log_level_t level, const char *tag, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* __FF_LOGGING_H__ */