/*
 * Copyright (C) 2013 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __MT9M114_H__
#define __MT9M114_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define SENSOR_NAME     "mt9m114"
#define DEV(x)          "/dev/"x
#define SENSOR_PATH     DEV(SENSOR_NAME)
#define LOG_NAME(x)     "ImagerODM-"x
#define LOG_TAG         LOG_NAME(SENSOR_NAME)

#define MT9M114_SENSOR_WAIT_MS          0
#define MT9M114_SENSOR_TABLE_END        1
#define MT9M114_SENSOR_BYTE_WRITE       2
#define MT9M114_SENSOR_WORD_WRITE       3
#define MT9M114_SENSOR_MASK_BYTE_WRITE  4
#define MT9M114_SENSOR_MASK_WORD_WRITE  5
#define MT9M114_SEQ_WRITE_START         6
#define MT9M114_SEQ_WRITE_END           7

#define MT9M114_SENSOR_MAX_RETRIES      3 /* max counter for retry I2C access */
#define MT9M114_MAX_FACEDETECT_WINDOWS  5
#define MT9M114_SENSOR_IOCTL_SET_MODE  _IOW('o', 1, struct mt9m114_modeinfo)
#define MT9M114_SENSOR_IOCTL_GET_STATUS         _IOR('o', 2, __u8)
#define MT9M114_SENSOR_IOCTL_SET_COLOR_EFFECT   _IOW('o', 3, __u16)
#define MT9M114_SENSOR_IOCTL_SET_WHITE_BALANCE  _IOW('o', 4, __u8)
#define MT9M114_SENSOR_IOCTL_SET_SCENE_MODE     _IOW('o', 5, __u8)
#define MT9M114_SENSOR_IOCTL_SET_AF_MODE        _IOW('o', 6, __u8)
#define MT9M114_SENSOR_IOCTL_GET_AF_STATUS      _IOR('o', 7, __u8)
#define MT9M114_SENSOR_IOCTL_SET_CAMERA         _IOW('o', 8, __u8)

struct mt9m114_mode {
	int xres;
	int yres;
};

struct mt9m114_modeinfo {
	int xres;
	int yres;
};

#define  AF_CMD_START 0
#define  AF_CMD_ABORT 1
#define  AF_CMD_SET_POSITION  2
#define  AF_CMD_SET_WINDOW_POSITION 3
#define  AF_CMD_SET_WINDOW_SIZE 4
#define  AF_CMD_SET_AFMODE  5
#define  AF_CMD_SET_CAF 6
#define  AF_CMD_GET_AF_STATUS 7

enum {
	YUV_ColorEffect = 0,
	YUV_Whitebalance,
	YUV_SceneMode,
	YUV_FlashType
};

enum {
	YUV_ColorEffect_Invalid = 0xA000,
	YUV_ColorEffect_Aqua,
	YUV_ColorEffect_Blackboard,
	YUV_ColorEffect_Mono,
	YUV_ColorEffect_Negative,
	YUV_ColorEffect_None,
	YUV_ColorEffect_Posterize,
	YUV_ColorEffect_Sepia,
	YUV_ColorEffect_Solarize,
	YUV_ColorEffect_Whiteboard,
	YUV_ColorEffect_Vivid,
	YUV_ColorEffect_WaterColor,
	YUV_ColorEffect_Vintage,
	YUV_ColorEffect_Vintage2,
	YUV_ColorEffect_Lomo,
	YUV_ColorEffect_Red,
	YUV_ColorEffect_Blue,
	YUV_ColorEffect_Yellow,
	YUV_ColorEffect_Aura,
	YUV_ColorEffect_Max
};

enum {
	YUV_Whitebalance_Invalid = 0,
	YUV_Whitebalance_Auto,
	YUV_Whitebalance_Incandescent,
	YUV_Whitebalance_Fluorescent,
	YUV_Whitebalance_WarmFluorescent,
	YUV_Whitebalance_Daylight,
	YUV_Whitebalance_CloudyDaylight,
	YUV_Whitebalance_Shade,
	YUV_Whitebalance_Twilight,
	YUV_Whitebalance_Custom
};

enum {
	YUV_SceneMode_Invalid = 0,
	YUV_SceneMode_Auto,
	YUV_SceneMode_Action,
	YUV_SceneMode_Portrait,
	YUV_SceneMode_Landscape,
	YUV_SceneMode_Beach,
	YUV_SceneMode_Candlelight,
	YUV_SceneMode_Fireworks,
	YUV_SceneMode_Night,
	YUV_SceneMode_NightPortrait,
	YUV_SceneMode_Party,
	YUV_SceneMode_Snow,
	YUV_SceneMode_Sports,
	YUV_SceneMode_SteadyPhoto,
	YUV_SceneMode_Sunset,
	YUV_SceneMode_Theatre,
	YUV_SceneMode_Barcode,
	YUV_SceneMode_BackLight
};

enum {
	YUV_FlashControlOn = 0,
	YUV_FlashControlOff,
	YUV_FlashControlAuto,
	YUV_FlashControlRedEyeReduction,
	YUV_FlashControlFillin,
	YUV_FlashControlTorch
};

enum {
	YUV_ISO_AUTO = 0,
	YUV_ISO_50 = 1,
	YUV_ISO_100 = 2,
	YUV_ISO_200 = 3,
	YUV_ISO_400 = 4,
	YUV_ISO_800 = 5,
	YUV_ISO_1600 = 6,
};

enum {
	YUV_FRAME_RATE_AUTO = 0,
	YUV_FRAME_RATE_30 = 1,
	YUV_FRAME_RATE_15 = 2,
	YUV_FRAME_RATE_7 = 3,
};

enum {
	YUV_ANTIBANGING_OFF = 0,
	YUV_ANTIBANGING_AUTO = 1,
	YUV_ANTIBANGING_50HZ = 2,
	YUV_ANTIBANGING_60HZ = 3,
};

enum {
	INT_STATUS_MODE = 0x01,
	INT_STATUS_AF = 0x02,
	INT_STATUS_ZOOM = 0x04,
	INT_STATUS_CAPTURE = 0x08,
	INT_STATUS_FRAMESYNC = 0x10,
	INT_STATUS_FD = 0x20,
	INT_STATELENS_INIT = 0x40,
	INT_STATUS_SOUND = 0x80,
};

enum {
	TOUCH_STATUS_OFF = 0,
	TOUCH_STATUS_ON,
	TOUCH_STATUS_DONE,
};

enum {
	CALIBRATION_ISP_POWERON_FAIL = 0,
	CALIBRATION_ISP_INIT_FAIL,
	CALIBRATION_ISP_MONITOR_FAIL,
	CALIBRATION_LIGHT_SOURCE_FAIL,
	CALIBRATION_LIGHT_SOURCE_OK,
	CALIBRATION_ISP_CAPTURE_FAIL,
	CALIBRATION_ISP_CHECKSUM_FAIL,
	CALIBRATION_ISP_PGAIN_FAIL,
	CALIBRATION_ISP_GOLDEN_FAIL,
	CALIBRATION_ISP_FAIL,
	CALIBRATION_ISP_OK,
};

enum {
	STREAMING_TYPE_PREVIEW_STREAMING = 0,
	STREAMING_TYPE_SINGLE_SHOT,
	STREAMING_TYPE_HDR_STREAMING,
	STREAMING_TYPE_BURST_STREAMING,
	STREAMING_TYPE_PREVIEW_AFTER_SINGLE_SHOT,
	STREAMING_TYPE_PREVIEW_AFTER_HDR_STREAMING,
	STREAMING_TYPE_PREVIEW_AFTER_BURST_STREAMING,
};

struct mt9m114_sensordata {
	__u32 fuse_id_size;
	__u8  fuse_id[16];
};

#ifdef __KERNEL__
struct mt9m114_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
};

struct mt9m114_platform_data {
	int (*power_on)(struct mt9m114_power_rail *pw);
	int (*power_off)(struct mt9m114_power_rail *pw);
};
#endif /* __KERNEL__ */

#endif  /* __MT9M114_H__ */
