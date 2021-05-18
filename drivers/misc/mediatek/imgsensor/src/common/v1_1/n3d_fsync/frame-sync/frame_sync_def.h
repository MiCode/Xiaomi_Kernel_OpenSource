/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _FRAME_SYNC_DEF_H
#define _FRAME_SYNC_DEF_H


#define SENSOR_MAX_NUM 5


/* get timestamp by using bellow method */
// #define USING_CCU
#ifdef USING_CCU
/* delay power ON/OFF and operate CCU to fs_set_sync() */
#define DELAY_CCU_OP
#endif // USING_CCU

#define USING_N3D


/* using v4l2_ctrl_request_setup */
#define USING_V4L2_CTRL_REQUEST_SETUP


/* for (FrameSync + Sensor Driver + CCU) Single Cam IT using */
// #define FS_SENSOR_CCU_IT


/******************************************************************************/
// Log message
/******************************************************************************/
#define LOG_BUF_STR_LEN 512


#ifdef FS_UT
#include <stdio.h>
#define LOG_INF(format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_PR_WARN(format, args...) printf(PFX "[%s] " format, __func__, ##args)
#define LOG_PR_ERR(format, args...) printf(PFX "[%s] " format, __func__, ##args)
#else
#include <linux/printk.h>  /* for kernel log reduction */
#define LOG_INF(format, args...) pr_debug(PFX "[%s] " format, __func__, ##args)
#define LOG_PR_WARN(format, args...) pr_warn(PFX "[%s] " format, __func__, ##args)
#define LOG_PR_ERR(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)
#endif
/******************************************************************************/


/******************************************************************************/
// frame_record_st (record shutter and framelength settings)
/******************************************************************************/
#define RECORDER_DEPTH 3
struct frame_record_st {
	unsigned int *framelength_lc;
	unsigned int *shutter_lc;
};

#endif
