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

#ifndef __OV7695_H__
#define __OV7695_H__

#include <linux/ioctl.h>  /* For IOCTL macros */

#define OV7695_SENSOR_NAME     "OV7695"
#define OV7695_DEV(x)          "/dev/"x
#define OV7695_SENSOR_PATH     DEV(OV7695_SENSOR_NAME)
#define OV7695_LOG_NAME(x)     "ImagerODM-"x
#define OV7695_LOG_TAG         LOG_NAME(OV7695_SENSOR_NAME)

#define OV7695_SENSOR_WAIT_MS          0
#define OV7695_SENSOR_TABLE_END        1
#define OV7695_SENSOR_BYTE_WRITE       2
#define OV7695_SENSOR_WORD_WRITE       3
#define OV7695_SENSOR_MASK_BYTE_WRITE  4
#define OV7695_SENSOR_MASK_WORD_WRITE  5
#define OV7695_SEQ_WRITE_START         6
#define OV7695_SEQ_WRITE_END           7

#define OV7695_SENSOR_MAX_RETRIES      3 /* max counter for retry I2C access */
#define OV7695_MAX_FACEDETECT_WINDOWS  5
#define OV7695_SENSOR_IOCTL_SET_MODE  _IOW('o', 1, struct ov7695_modeinfo)
#define OV7695_SENSOR_IOCTL_GET_STATUS         _IOR('o', 2, __u8)
#define OV7695_SENSOR_IOCTL_SET_COLOR_EFFECT   _IOW('o', 3, __u16)
#define OV7695_SENSOR_IOCTL_SET_WHITE_BALANCE  _IOW('o', 4, __u8)
#define OV7695_SENSOR_IOCTL_SET_SCENE_MODE     _IOW('o', 5, __u8)
#define OV7695_SENSOR_IOCTL_SET_AF_MODE        _IOW('o', 6, __u8)
#define OV7695_SENSOR_IOCTL_GET_AF_STATUS      _IOR('o', 7, __u8)
#define OV7695_SENSOR_IOCTL_SET_CAMERA         _IOW('o', 8, __u8)
#define OV7695_SENSOR_IOCTL_SET_EV             _IOW('o', 9, __s16)
#define OV7695_SENSOR_IOCTL_GET_EV             _IOR('o', 10, __s16)

struct ov7695_mode {
	int xres;
	int yres;
};

struct ov7695_modeinfo {
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
	OV7695_YUV_ColorEffect = 0,
	OV7695_YUV_Whitebalance,
	OV7695_YUV_SceneMode,
	OV7695_YUV_FlashType
};

enum {
	OV7695_YUV_ColorEffect_Invalid = 0xA000,
	OV7695_YUV_ColorEffect_Aqua,
	OV7695_YUV_ColorEffect_Blackboard,
	OV7695_YUV_ColorEffect_Mono,
	OV7695_YUV_ColorEffect_Negative,
	OV7695_YUV_ColorEffect_None,
	OV7695_YUV_ColorEffect_Posterize,
	OV7695_YUV_ColorEffect_Sepia,
	OV7695_YUV_ColorEffect_Solarize,
	OV7695_YUV_ColorEffect_Whiteboard,
	OV7695_YUV_ColorEffect_Vivid,
	OV7695_YUV_ColorEffect_WaterColor,
	OV7695_YUV_ColorEffect_Vintage,
	OV7695_YUV_ColorEffect_Vintage2,
	OV7695_YUV_ColorEffect_Lomo,
	OV7695_YUV_ColorEffect_Red,
	OV7695_YUV_ColorEffect_Blue,
	OV7695_YUV_ColorEffect_Yellow,
	OV7695_YUV_ColorEffect_Aura,
	OV7695_YUV_ColorEffect_Max
};

enum {
	OV7695_YUV_Whitebalance_Invalid = 0,
	OV7695_YUV_Whitebalance_Auto,
	OV7695_YUV_Whitebalance_Incandescent,
	OV7695_YUV_Whitebalance_Fluorescent,
	OV7695_YUV_Whitebalance_WarmFluorescent,
	OV7695_YUV_Whitebalance_Daylight,
	OV7695_YUV_Whitebalance_CloudyDaylight,
	OV7695_YUV_Whitebalance_Shade,
	OV7695_YUV_Whitebalance_Twilight,
	OV7695_YUV_Whitebalance_Custom
};

enum {
	OV7695_YUV_SceneMode_Invalid = 0,
	OV7695_YUV_SceneMode_Auto,
	OV7695_YUV_SceneMode_Action,
	OV7695_YUV_SceneMode_Portrait,
	OV7695_YUV_SceneMode_Landscape,
	OV7695_YUV_SceneMode_Beach,
	OV7695_YUV_SceneMode_Candlelight,
	OV7695_YUV_SceneMode_Fireworks,
	OV7695_YUV_SceneMode_Night,
	OV7695_YUV_SceneMode_NightPortrait,
	OV7695_YUV_SceneMode_Party,
	OV7695_YUV_SceneMode_Snow,
	OV7695_YUV_SceneMode_Sports,
	OV7695_YUV_SceneMode_SteadyPhoto,
	OV7695_YUV_SceneMode_Sunset,
	OV7695_YUV_SceneMode_Theatre,
	OV7695_YUV_SceneMode_Barcode,
	OV7695_YUV_SceneMode_BackLight
};

enum {
	OV7695_YUV_FlashControlOn = 0,
	OV7695_YUV_FlashControlOff,
	OV7695_YUV_FlashControlAuto,
	OV7695_YUV_FlashControlRedEyeReduction,
	OV7695_YUV_FlashControlFillin,
	OV7695_YUV_FlashControlTorch
};

enum {
	OV7695_YUV_ISO_AUTO = 0,
	OV7695_YUV_ISO_50 = 1,
	OV7695_YUV_ISO_100 = 2,
	OV7695_YUV_ISO_200 = 3,
	OV7695_YUV_ISO_400 = 4,
	OV7695_YUV_ISO_800 = 5,
	OV7695_YUV_ISO_1600 = 6,
};

enum {
	OV7695_YUV_FRAME_RATE_AUTO = 0,
	OV7695_YUV_FRAME_RATE_30 = 1,
	OV7695_YUV_FRAME_RATE_15 = 2,
	OV7695_YUV_FRAME_RATE_7 = 3,
};

enum {
	OV7695_YUV_ANTIBANGING_OFF = 0,
	OV7695_YUV_ANTIBANGING_AUTO = 1,
	OV7695_YUV_ANTIBANGING_50HZ = 2,
	OV7695_YUV_ANTIBANGING_60HZ = 3,
};

enum {
	OV7695_INT_STATUS_MODE = 0x01,
	OV7695_INT_STATUS_AF = 0x02,
	OV7695_INT_STATUS_ZOOM = 0x04,
	OV7695_INT_STATUS_CAPTURE = 0x08,
	OV7695_INT_STATUS_FRAMESYNC = 0x10,
	OV7695_INT_STATUS_FD = 0x20,
	OV7695_INT_STATELENS_INIT = 0x40,
	OV7695_INT_STATUS_SOUND = 0x80,
};

enum {
	OV7695_TOUCH_STATUS_OFF = 0,
	OV7695_TOUCH_STATUS_ON,
	OV7695_TOUCH_STATUS_DONE,
};

enum {
	OV7695_CALIBRATION_ISP_POWERON_FAIL = 0,
	OV7695_CALIBRATION_ISP_INIT_FAIL,
	OV7695_CALIBRATION_ISP_MONITOR_FAIL,
	OV7695_CALIBRATION_LIGHT_SOURCE_FAIL,
	OV7695_CALIBRATION_LIGHT_SOURCE_OK,
	OV7695_CALIBRATION_ISP_CAPTURE_FAIL,
	OV7695_CALIBRATION_ISP_CHECKSUM_FAIL,
	OV7695_CALIBRATION_ISP_PGAIN_FAIL,
	OV7695_CALIBRATION_ISP_GOLDEN_FAIL,
	OV7695_CALIBRATION_ISP_FAIL,
	OV7695_CALIBRATION_ISP_OK,
};

enum {
	OV7695_STREAMING_TYPE_PREVIEW_STREAMING = 0,
	OV7695_STREAMING_TYPE_SINGLE_SHOT,
	OV7695_STREAMING_TYPE_HDR_STREAMING,
	OV7695_STREAMING_TYPE_BURST_STREAMING,
	OV7695_STREAMING_TYPE_PREVIEW_AFTER_SINGLE_SHOT,
	OV7695_STREAMING_TYPE_PREVIEW_AFTER_HDR_STREAMING,
	OV7695_STREAMING_TYPE_PREVIEW_AFTER_BURST_STREAMING,
};

struct ov7695_sensordata {
	__u32 fuse_id_size;
	__u8  fuse_id[16];
};

#ifdef __KERNEL__
struct ov7695_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
};

struct ov7695_platform_data {
	int (*power_on)(struct ov7695_power_rail *pw);
	int (*power_off)(struct ov7695_power_rail *pw);
	const char *mclk_name;
};
#endif /* __KERNEL__ */

#endif  /* __OV7695_H__ */
