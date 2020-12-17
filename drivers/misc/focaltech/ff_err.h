/**
 * ${ANDROID_BUILD_TOP}/vendor/focaltech/src/base/focaltech/ff_err.h
 *
 * Copyright (C) 2014-2017 FocalTech Systems Co., Ltd. All Rights Reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 *
**/

#ifndef __FF_ERROR_H__
#define __FF_ERROR_H__

#include "ff_log.h" /* FF_LOGE(..) */

//
// All the following macros will return while error.
//

#define FF_CHECK_ERR(expr)                                             \
	do {                                                               \
		int __e = (expr);                                              \
		if (__e) {                                                     \
			FF_LOGE("'%s'.", ff_err_strerror(__e));                    \
			return __e;                                              \
		}                                                              \
	} while (0)
/* End of FF_CHECK_ERR */

#define FF_CHECK_PTR(ptr)                                              \
	do {                                                               \
		if ((ptr) == NULL) {                                           \
			FF_CHECK_ERR(FF_ERR_NULL_PTR);                             \
		}                                                              \
	} while (0)
/* End of FF_CHECK_PTR */

#define FF_FAILURE_RETRY(expr, retry)                                  \
	do {                                                               \
		int __e, i = 0, j = retry;                                     \
		do {                                                           \
			__e = (expr);                                              \
			if (!__e) 												   \
				break;												   \
			if (++i <= j) {                                            \
				FF_LOGW("'"#expr"' failed, try again (%d/%d).", i, j); \
			} else {                                                   \
				FF_LOGE("'%s'.", ff_err_strerror(__e));                \
				return __e;                                          \
			}                                                          \
		} while (true);                                                \
	} while (0)
/* End of FF_FAILURE_RETRY */

typedef enum {
    FF_SUCCESS      =  0, /* No error.      */
    FF_ERR_INTERNAL = -1, /* Generic error. */

    /* Base on unix errno. */
    FF_ERR_NOENT =  -2,
    FF_ERR_INTR  =  -4,
    FF_ERR_IO    =  -5,
    FF_ERR_AGAIN = -11,
    FF_ERR_NOMEM = -12,
    FF_ERR_BUSY  = -16,

    /* Common error. */
    FF_ERR_BAD_PARAMS   = -200,
    FF_ERR_NULL_PTR     = -201,
    FF_ERR_BUF_OVERFLOW = -202,
    FF_ERR_BAD_PROTOCOL = -203,
    FF_ERR_SENSOR_SIZE  = -204,
    FF_ERR_NULL_DEVICE  = -205,
    FF_ERR_DEAD_DEVICE  = -206,
    FF_ERR_REACH_LIMIT  = -207,
    FF_ERR_REE_TEMPLATE = -208,
    FF_ERR_NOT_TRUSTED  = -209,
} ff_err_t;

/*
 * For some functions that don't return an int(ff_err_t) type error
 * code, 'ff_last_error' shall be set if there is an error happened.
 */
extern ff_err_t ff_last_error;

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Return an error description string corresponding to $err.
 *
 * @params
 *  err: Error number.
 *
 * @return
 *  Static error description string.
 */
const char *ff_err_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif /* __FF_ERROR_H__ */