/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2014-2020, Elliptic Laboratories AS. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 * Elliptic Labs Linux driver
 */

#pragma once

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <elliptic_data_io.h>

#define ELLIPTIC_DEVICENAME "elliptic"
#define ELLIPTIC_NUM_DEVICES 2

#define IOCTL_ELLIPTIC_APP	197
#define MIRROR_TAG		0x3D0A4842

#define IOCTL_ELLIPTIC_DATA_IO_CANCEL \
	_IO(IOCTL_ELLIPTIC_APP, 2)

#define IOCTL_ELLIPTIC_ACTIVATE_ENGINE \
	_IOW(IOCTL_ELLIPTIC_APP, 3, int)

#define IOCTL_ELLIPTIC_SET_RAMP_DOWN \
	_IO(IOCTL_ELLIPTIC_APP, 4)

#define IOCTL_ELLIPTIC_SYSTEM_CONFIGURATION \
	_IOW(IOCTL_ELLIPTIC_APP, 5, int)

struct elliptic_device {
	int opened;
	struct cdev cdev;
	struct semaphore sem;
	struct elliptic_data el_data;
};

extern struct class *elliptic_class;


