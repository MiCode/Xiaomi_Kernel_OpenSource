/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/**
 * @file    mtk-srclken-rc.h
 * @brief   Driver for subys request resource control
 *
 */
#ifndef __MTK_SRCLKEN_RC_H__
#define __MTK_SRCLKEN_RC_H__

enum srclken_ret_type {
	SRCLKEN_OK = 0,
	SRCLKEN_NOT_READY = -1,
	SRCLKEN_NOT_SUPPORT = -2,
	SRCLKEN_BRINGUP = -3,
	SRCLKEN_ERR = -4,
	SRCLKEN_READ_FAIL = -5,
};

enum srclken_cfg {
	BT_ONLY_CFG = 0,
	COANT_ONLY_CFG,
	FULL_SET_CFG,
	NOT_SUPPORT_CFG,
};

extern bool srclken_get_debug_cfg(void);
extern int srclken_dump_sta_log(void);
extern int srclken_dump_cfg_log(void);
extern int srclken_dump_last_sta_log(void);

#endif

