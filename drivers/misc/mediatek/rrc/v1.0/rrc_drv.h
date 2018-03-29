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

#include <linux/ioctl.h>

#ifndef __RRC_DRV_H__
#define __RRC_DRV_H__



typedef struct {

	unsigned int scenario;
	unsigned int enable;
	/* unsigned int pid;           */
	/* unsigned int *maxSafeSize;  */
	/* unsigned int *result;       */

} RRC_DRV_DATA;


typedef enum {

	RRC_DRV_TYPE_NONE                      = 0,
	RRC_DRV_TYPE_CAMERA_PREVIEW            ,
	RRC_DRV_TYPE_CAMERA_ZSD                ,
	RRC_DRV_TYPE_CAMERA_CAPTURE            ,
	RRC_DRV_TYPE_CAMERA_ICFP               ,
	RRC_DRV_TYPE_VIDEO_NORMAL              ,
	RRC_DRV_TYPE_VIDEO_SWDEC_PLAYBACK      ,
	RRC_DRV_TYPE_VIDEO_PLAYBACK            ,
	RRC_DRV_TYPE_VIDEO_TELEPHONY           ,
	RRC_DRV_TYPE_VIDEO_RECORD              ,
	RRC_DRV_TYPE_VIDEO_RECORD_CAMERA       ,
	RRC_DRV_TYPE_VIDEO_RECORD_SLOWMOTION   ,
	RRC_DRV_TYPE_VIDEO_SNAPSHOT            ,
	RRC_DRV_TYPE_VIDEO_LIVE_PHOTO          ,
	RRC_DRV_TYPE_VIDEO_WIFI_DISPLAY        ,

	/* touch event */
	RRC_DRV_TYPE_TOUCH_EVENT               ,

	RRC_DRV_TYPE_MAX_SIZE


} RRC_DRV_SCENARIO_TYPE;


typedef enum {
	RRC_DRV_NONE = 0,
	RRC_DRV_60Hz ,
	RRC_DRV_120Hz


} RRC_DRV_REFRESH_RATE;




#define RRC_IOCTL_MAGIC        'x'

/* #define JPEG_DEC_IOCTL_INIT     _IO  (ALMK_IOCTL_MAGIC, 1)                    */
/* #define JPEG_DEC_IOCTL_CONFIG   _IOW (ALMK_IOCTL_MAGIC, 2, JPEG_DEC_DRV_IN)   */
/* #define JPEG_DEC_IOCTL_START    _IO  (ALMK_IOCTL_MAGIC, 3)                    */
/* #define JPEG_DEC_IOCTL_WAIT     _IOWR(ALMK_IOCTL_MAGIC, 6, JPEG_DEC_DRV_OUT)  */
/* #define JPEG_DEC_IOCTL_DEINIT   _IO  (ALMK_IOCTL_MAGIC, 8)                    */

#define RRC_IOCTL_CMD_INIT          _IO(RRC_IOCTL_MAGIC, 11)
#define RRC_IOCTL_CMD_SET_SCENARIO_TYPE  _IOWR(RRC_IOCTL_MAGIC, 12, RRC_DRV_DATA)
#define RRC_IOCTL_CMD_DEINIT        _IO(RRC_IOCTL_MAGIC, 13)





#endif

