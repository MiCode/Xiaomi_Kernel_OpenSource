/*
 *  Silicon Integrated Co., Ltd haptic sih688x haptic header file
 *
 *  Copyright (c) 2021 kugua <canzhen.peng@si-in.com>
 *  Copyright (c) 2021 tianchi <tianchi.zheng@si-in.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation
 */

#ifndef _HAPTIC_MISC_H_
#define _HAPTIC_MISC_H_

#include <linux/regmap.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/hrtimer.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/version.h>
#include <sound/control.h>
#include <sound/soc.h>

#define SIH_DEV_HAPTIC_NAME					"si_haptic"

#define IOCTL_MMAP_PAGE_ORDER				2
#define IOCTL_MMAP_BUF_SUM					16
#define IOCTL_HWINFO						0x0a

#define IOCTL_IOCTL_GROUP					0x52
#define IOCTL_WAIT_BUFF_VALID_MAX_TRY		100
#define IOCTL_GET_HWINFO					_IO(IOCTL_IOCTL_GROUP, 0x03)
#define IOCTL_SET_FREQ						_IO(IOCTL_IOCTL_GROUP, 0x04)
#define IOCTL_SETTING_GAIN					_IO(IOCTL_IOCTL_GROUP, 0x05)
#define IOCTL_OFF_MODE						_IO(IOCTL_IOCTL_GROUP, 0x06)
#define IOCTL_TIMEOUT_MODE					_IO(IOCTL_IOCTL_GROUP, 0x07)
#define IOCTL_RAM_MODE						_IO(IOCTL_IOCTL_GROUP, 0x08)
#define IOCTL_MODE_RTP_MODE					_IO(IOCTL_IOCTL_GROUP, 0x09)
#define IOCTL_STREAM_MODE					_IO(IOCTL_IOCTL_GROUP, 0x0A)
#define IOCTL_UPDATE_RAM					_IO(IOCTL_IOCTL_GROUP, 0x10)
#define IOCTL_GET_F0						_IO(IOCTL_IOCTL_GROUP, 0x11)
#define IOCTL_STOP_MODE						_IO(IOCTL_IOCTL_GROUP, 0x12)
#define IOCTL_F0_UPDATE						_IO(IOCTL_IOCTL_GROUP, 0x13)

typedef enum haptic_buf_status {
	MMAP_BUF_DATA_VALID = 0x55,
	MMAP_BUF_DATA_FINISHED = 0xaa,
	MMAP_BUF_DATA_INVALID = 0xff,
} haptic_buf_status_e;

int sih_add_misc_dev(void);

#endif
