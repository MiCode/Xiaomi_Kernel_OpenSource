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

#ifndef _FLASHLIGHT_H
#define _FLASHLIGHT_H

#include <linux/types.h>
#include <linux/ioctl.h>

/* device node and sysfs */
#define FLASHLIGHT_CORE             "flashlight_core"
#define FLASHLIGHT_DEVNAME          "flashlight"
#define FLASHLIGHT_SYSFS_STROBE     "flashlight_strobe"
#define FLASHLIGHT_SYSFS_PT         "flashlight_pt"
#define FLASHLIGHT_SYSFS_CHARGER    "flashlight_charger"
#define FLASHLIGHT_SYSFS_CURRENT    "flashlight_current"
#define FLASHLIGHT_SYSFS_CAPABILITY "flashlight_capability"
#define FLASHLIGHT_SYSFS_FAULT      "flashlight_fault"
#define FLASHLIGHT_SYSFS_SW_DISABLE "flashlight_sw_disable"

/* scenario */
#define FLASHLIGHT_SCENARIO_CAMERA_MASK 1
#define FLASHLIGHT_SCENARIO_DECOUPLE_MASK 2
#define FLASHLIGHT_SCENARIO_FLASHLIGHT (0 << 0)
#define FLASHLIGHT_SCENARIO_CAMERA     (1 << 0)
#define FLASHLIGHT_SCENARIO_COUPLE     (0 << 1)
#define FLASHLIGHT_SCENARIO_DECOUPLE   (1 << 1)

/* charger status */
#define FLASHLIGHT_CHARGER_NOT_READY 0
#define FLASHLIGHT_CHARGER_READY     1

/* sw disable status*/
#define FLASHLIGHT_SW_DISABLE_ON	1
#define FLASHLIGHT_SW_DISABLE_OFF	0

/* max duty number */
#define FLASHLIGHT_MAX_DUTY_NUM 40

/* flashlight arguments */
#define FLASHLIGHT_TYPE_MAX 2
#define FLASHLIGHT_CT_MAX 3
#define FLASHLIGHT_PART_MAX 2
struct flashlight_user_arg {
	int type_id;
	int ct_id;
	int arg;
};

/* ioctl magic number */
#define FLASHLIGHT_MAGIC 'S'

/* ioctl protocol version 0. */
#define FLASHLIGHTIOC_T_ENABLE             _IOW(FLASHLIGHT_MAGIC, 5, int)
#define FLASHLIGHTIOC_T_LEVEL              _IOW(FLASHLIGHT_MAGIC, 10, int)
#define FLASHLIGHTIOC_T_FLASHTIME          _IOW(FLASHLIGHT_MAGIC, 15, int)
#define FLASHLIGHTIOC_T_STATE              _IOW(FLASHLIGHT_MAGIC, 20, int)
#define FLASHLIGHTIOC_G_FLASHTYPE          _IOR(FLASHLIGHT_MAGIC, 25, int)
#define FLASHLIGHTIOC_X_SET_DRIVER         _IOWR(FLASHLIGHT_MAGIC, 30, int)
#define FLASHLIGHTIOC_T_DELAY              _IOW(FLASHLIGHT_MAGIC, 35, int)

/* ioctl protocol version 1. */
#define FLASH_IOC_SET_TIME_OUT_TIME_MS     _IOR(FLASHLIGHT_MAGIC, 100, int)
#define FLASH_IOC_SET_STEP                 _IOR(FLASHLIGHT_MAGIC, 105, int)
#define FLASH_IOC_SET_DUTY                 _IOR(FLASHLIGHT_MAGIC, 110, int)
#define FLASH_IOC_SET_ONOFF                _IOR(FLASHLIGHT_MAGIC, 115, int)
#define FLASH_IOC_UNINIT                   _IOR(FLASHLIGHT_MAGIC, 120, int)

#define FLASH_IOC_PRE_ON                   _IOR(FLASHLIGHT_MAGIC, 125, int)
#define FLASH_IOC_GET_PRE_ON_TIME_MS       _IOR(FLASHLIGHT_MAGIC, 130, int)
#define FLASH_IOC_GET_PRE_ON_TIME_MS_DUTY  _IOR(FLASHLIGHT_MAGIC, 131, int)

#define FLASH_IOC_SET_REG_ADR              _IOR(FLASHLIGHT_MAGIC, 135, int)
#define FLASH_IOC_SET_REG_VAL              _IOR(FLASHLIGHT_MAGIC, 140, int)
#define FLASH_IOC_SET_REG                  _IOR(FLASHLIGHT_MAGIC, 145, int)
#define FLASH_IOC_GET_REG                  _IOR(FLASHLIGHT_MAGIC, 150, int)

#define FLASH_IOC_GET_MAIN_PART_ID         _IOR(FLASHLIGHT_MAGIC, 155, int)
#define FLASH_IOC_GET_SUB_PART_ID          _IOR(FLASHLIGHT_MAGIC, 160, int)
#define FLASH_IOC_GET_MAIN2_PART_ID        _IOR(FLASHLIGHT_MAGIC, 165, int)
#define FLASH_IOC_GET_PART_ID              _IOR(FLASHLIGHT_MAGIC, 166, int)

#define FLASH_IOC_HAS_LOW_POWER_DETECT     _IOR(FLASHLIGHT_MAGIC, 170, int)
#define FLASH_IOC_LOW_POWER_DETECT_START   _IOR(FLASHLIGHT_MAGIC, 175, int)
#define FLASH_IOC_LOW_POWER_DETECT_END     _IOR(FLASHLIGHT_MAGIC, 180, int)
#define FLASH_IOC_IS_LOW_POWER             _IOR(FLASHLIGHT_MAGIC, 182, int)

#define FLASH_IOC_GET_ERR                  _IOR(FLASHLIGHT_MAGIC, 185, int)
#define FLASH_IOC_GET_PROTOCOL_VERSION     _IOR(FLASHLIGHT_MAGIC, 190, int)

#define FLASH_IOC_IS_CHARGER_IN            _IOR(FLASHLIGHT_MAGIC, 195, int)
#define FLASH_IOC_IS_OTG_USE               _IOR(FLASHLIGHT_MAGIC, 200, int)
#define FLASH_IOC_GET_FLASH_DRIVER_NAME_ID _IOR(FLASHLIGHT_MAGIC, 205, int)

/* ioctl protocol version 2 */
#define FLASH_IOC_IS_CHARGER_READY         _IOR(FLASHLIGHT_MAGIC, 210, int)
#define FLASH_IOC_SET_SCENARIO             _IOWR(FLASHLIGHT_MAGIC, 215, int)
#define FLASH_IOC_IS_HARDWARE_READY        _IOR(FLASHLIGHT_MAGIC, 220, int)
#define FLASH_IOC_GET_DUTY_NUMBER          _IOWR(FLASHLIGHT_MAGIC, 225, int)
#define FLASH_IOC_GET_MAX_TORCH_DUTY       _IOWR(FLASHLIGHT_MAGIC, 230, int)
#define FLASH_IOC_GET_DUTY_CURRENT         _IOWR(FLASHLIGHT_MAGIC, 235, int)
#define FLASH_IOC_GET_HW_TIMEOUT           _IOWR(FLASHLIGHT_MAGIC, 240, int)
#define FLASH_IOC_GET_HW_FAULT             _IOR(FLASHLIGHT_MAGIC, 250, int)
#define FLASH_IOC_GET_HW_FAULT2            _IOR(FLASHLIGHT_MAGIC, 251, int)

#endif /* _FLASHLIGHT_H */

