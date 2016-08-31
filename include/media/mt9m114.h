/*
 * Copyright (C) 2013-2014 NVIDIA Corporation. All rights reserved.
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
#define MT9M114_SENSOR_IOCTL_SET_EV             _IOW('o', 9, __s16)
#define MT9M114_SENSOR_IOCTL_GET_EV             _IOR('o', 10, __s16)
#define MT9M114_SENSOR_IOCTL_SET_MIN_FPS        _IOW('o', 11, __u16)

struct mt9m114_mode {
	int xres;
	int yres;
};

struct mt9m114_modeinfo {
	int xres;
	int yres;
};

#define AF_CMD_START 0
#define AF_CMD_ABORT 1
#define AF_CMD_SET_POSITION  2
#define AF_CMD_SET_WINDOW_POSITION 3
#define AF_CMD_SET_WINDOW_SIZE 4
#define AF_CMD_SET_AFMODE  5
#define AF_CMD_SET_CAF 6
#define AF_CMD_GET_AF_STATUS 7

enum {
	MT9M114_YUV_ColorEffect_Invalid = 0xA000,
	MT9M114_YUV_ColorEffect_Aqua,
	MT9M114_YUV_ColorEffect_Blackboard,
	MT9M114_YUV_ColorEffect_Mono,
	MT9M114_YUV_ColorEffect_Negative,
	MT9M114_YUV_ColorEffect_None,
	MT9M114_YUV_ColorEffect_Posterize,
	MT9M114_YUV_ColorEffect_Sepia,
	MT9M114_YUV_ColorEffect_Solarize,
	MT9M114_YUV_ColorEffect_Whiteboard,
	MT9M114_YUV_ColorEffect_Vivid,
	MT9M114_YUV_ColorEffect_WaterColor,
	MT9M114_YUV_ColorEffect_Vintage,
	MT9M114_YUV_ColorEffect_Vintage2,
	MT9M114_YUV_ColorEffect_Lomo,
	MT9M114_YUV_ColorEffect_Red,
	MT9M114_YUV_ColorEffect_Blue,
	MT9M114_YUV_ColorEffect_Yellow,
	MT9M114_YUV_ColorEffect_Aura,
	MT9M114_YUV_ColorEffect_Max
};

enum {
	MT9M114_YUV_Whitebalance_Invalid = 0,
	MT9M114_YUV_Whitebalance_Auto,
	MT9M114_YUV_Whitebalance_Incandescent,
	MT9M114_YUV_Whitebalance_Fluorescent,
	MT9M114_YUV_Whitebalance_WarmFluorescent,
	MT9M114_YUV_Whitebalance_Daylight,
	MT9M114_YUV_Whitebalance_CloudyDaylight,
	MT9M114_YUV_Whitebalance_Shade,
	MT9M114_YUV_Whitebalance_Twilight,
	MT9M114_YUV_Whitebalance_Custom
};

enum {
	MT9M114_YUV_SceneMode_Invalid = 0,
	MT9M114_YUV_SceneMode_Auto,
	MT9M114_YUV_SceneMode_Action,
	MT9M114_YUV_SceneMode_Portrait,
	MT9M114_YUV_SceneMode_Landscape,
	MT9M114_YUV_SceneMode_Beach,
	MT9M114_YUV_SceneMode_Candlelight,
	MT9M114_YUV_SceneMode_Fireworks,
	MT9M114_YUV_SceneMode_Night,
	MT9M114_YUV_SceneMode_NightPortrait,
	MT9M114_YUV_SceneMode_Party,
	MT9M114_YUV_SceneMode_Snow,
	MT9M114_YUV_SceneMode_Sports,
	MT9M114_YUV_SceneMode_SteadyPhoto,
	MT9M114_YUV_SceneMode_Sunset,
	MT9M114_YUV_SceneMode_Theatre,
	MT9M114_YUV_SceneMode_Barcode,
	MT9M114_YUV_SceneMode_BackLight
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
	const char *mclk_name;
};
#endif /* __KERNEL__ */

#endif  /* __MT9M114_H__ */
