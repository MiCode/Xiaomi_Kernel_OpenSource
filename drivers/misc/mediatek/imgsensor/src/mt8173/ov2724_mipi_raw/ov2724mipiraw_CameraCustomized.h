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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   sensor.h
 *
 * Project:
 * --------
 *   DUMA
 *
 * Description:
 * ------------
 *   Header file of camera customized parameters.
 *
 *
 * Author:
 * -------
 *   PC Huang (MTK02204)
 *
 *============================================================================
 *             HISTORY
 * Below this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *------------------------------------------------------------------------------
 * $Revision:$
 * $Modtime:$
 * $Log:$
 *
 * Feb 9 2010 mtk80461
 * [DUMA00154355] JPeg Orientation feature check in
 *
 *
 * Oct 27 2009 mtk02204
 * [DUMA00015869] [Camera Driver] Modifiy camera related drivers for dual/backup sensor/lens drivers.
 *
 *
 * Sep 24 2009 mtk02204
 * [DUMA00134387] [LTK][GW616][Camera]4040 Camera:Camera Quality Test failed.
 *
 *
 * Jul 8 2009 mtk02204
 * [DUMA00008051] [Camera Driver] Add drivers for camera high ISO binning mode.
 *
 *
 * Jul 8 2009 mtk02204
 * [DUMA00008051] [Camera Driver] Add drivers for camera high ISO binning mode.
 *
 *
 * Jul 7 2009 mtk02204
 * [DUMA00008051] [Camera Driver] Add drivers for camera high ISO binning mode.
 *
 *
 * Jun 6 2009 mtk02204
 * [DUMA00119628] MTK camera_recorded video only contain audio but no video with Jean effect
 *
 *
 * Apr 10 2009 mtk02204
 * [DUMA00004178] [Camera] Check in for MTK camera integration
 *
 *
 * Mar 13 2009 mtk02204
 * [DUMA00001084] First Check in of MT6516 multimedia drivers
 *
 *
 * Mar 2 2009 mtk02204
 * [DUMA00001084] First Check in of MT6516 multimedia drivers
 *
 *
 * Feb 6 2009 mtk02204
 * [DUMA00001084] First Check in of MT6516 multimedia drivers
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#ifndef __CAMERA_CUSTOMIZED_H
#define __CAMERA_CUSTOMIZED_H

/* the angle between handset and sensor placement in clockwise, should be one of 0, 90, 270 */
#define MAIN_SENSOR_ORIENTATION_ANGLE	90
#define SUB_SENSOR_ORIENTATION_ANGLE	0	/* do not care if the sub sensor does not exist */


/* First, we think you hold the cell phone vertical. */
/* Second, we suppose the direction of upward is 0 */
/* Third, it is 90, 180, 270 in clockwise */
/* here we define the main sensor and sub sensor angles to deal with the jpeg orientation */
#define MAIN_SENSOR_TO_PHONE_ANGLE	90
#define SUB_SENSOR_TO_PHONE_ANGLE	0


#define CAM_SIZE_QVGA_WIDTH	320
#define CAM_SIZE_QVGA_HEIGHT	240
#define CAM_SIZE_VGA_WIDTH		640
#define CAM_SIZE_VGA_HEIGHT	480
#define CAM_SIZE_05M_WIDTH		800
#define CAM_SIZE_05M_HEIGHT	600
#define CAM_SIZE_1M_WIDTH		1280
#define CAM_SIZE_1M_HEIGHT		960
#define CAM_SIZE_2M_WIDTH		1600
#define CAM_SIZE_2M_HEIGHT		1200
#define CAM_SIZE_3M_WIDTH		2048
#define CAM_SIZE_3M_HEIGHT		1536
#define CAM_SIZE_5M_WIDTH		2592
#define CAM_SIZE_5M_HEIGHT		1944

/* for main sensor */
#define MAIN_NUM_OF_PREVIEW_RESOLUTION 3
#define MAIN_NUM_OF_VIDEO_RESOLUTION 4
#define MAIN_NUM_OF_STILL_RESOLUTION 7
#define MAIN_VIDEO_RESOLUTION_PROFILE		{{176, 144}, {320, 240}, {640, 480}, {720, 480} }
#define MAIN_PREVIEW_RESOLUTION_PROFILE	{{232, 174}, {320, 240}, {240, 320} }
#define MAIN_STILL_RESOLUTION_PROFILE		{{CAM_SIZE_QVGA_WIDTH, CAM_SIZE_QVGA_HEIGHT}, \
									 {CAM_SIZE_VGA_WIDTH, CAM_SIZE_VGA_HEIGHT}, \
									 {CAM_SIZE_05M_WIDTH, CAM_SIZE_05M_HEIGHT}, \
									 {CAM_SIZE_1M_WIDTH, CAM_SIZE_1M_HEIGHT}, \
									 {CAM_SIZE_2M_WIDTH, CAM_SIZE_2M_HEIGHT}, \
									 {CAM_SIZE_3M_WIDTH, CAM_SIZE_3M_HEIGHT}, \
									 {CAM_SIZE_5M_WIDTH, CAM_SIZE_5M_HEIGHT} }

/* if sub sensor does not exist, set all the parameters as 0 */
#define SUB_NUM_OF_PREVIEW_RESOLUTION 0
#define SUB_NUM_OF_VIDEO_RESOLUTION 0
#define SUB_NUM_OF_STILL_RESOLUTION 0
#define SUB_VIDEO_RESOLUTION_PROFILE		{{0, 0} }
#define SUB_PREVIEW_RESOLUTION_PROFILE		{{0, 0} }
#define SUB_STILL_RESOLUTION_PROFILE		{{0, 0} }

/* #define NUM_OF_PREVIEW_RESOLUTION     max(MAIN_NUM_OF_PREVIEW_RESOLUTION,SUB_NUM_OF_PREVIEW_RESOLUTION) */
/* #define NUM_OF_VIDEO_RESOLUTION       max(MAIN_NUM_OF_VIDEO_RESOLUTION,SUB_NUM_OF_VIDEO_RESOLUTION) */
/* #define NUM_OF_STILL_RESOLUTION       max(MAIN_NUM_OF_STILL_RESOLUTION,SUB_NUM_OF_STILL_RESOLUTION) */

#define NUM_OF_VIDEO_STREAM_BUFF	8	/* Maximun is 8 */
#endif
