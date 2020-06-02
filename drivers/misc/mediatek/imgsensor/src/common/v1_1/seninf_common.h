/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SENINF_COMMON_H__
#define __SENINF_COMMON_H__

#define PREFIX "[seninf]"

#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG(fmt, arg...)  pr_debug(PREFIX fmt, ##arg)
#define PK_PR_ERR(fmt, arg...)  pr_err(fmt, ##arg)
#define PK_INFO(fmt, arg...) pr_debug(PREFIX fmt, ##arg)
#else
#define PK_DBG(fmt, arg...)
#define PK_PR_ERR(fmt, arg...)  pr_err(fmt, ##arg)
#define PK_INFO(fmt, arg...) pr_debug(PREFIX fmt, ##arg)
#endif

enum SENINF_RETURN {
	SENINF_RETURN_SUCCESS = 0,
	SENINF_RETURN_ERROR = -1,
};

#endif
