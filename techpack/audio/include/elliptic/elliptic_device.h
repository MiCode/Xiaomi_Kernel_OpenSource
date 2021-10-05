/**
 * Copyright Elliptic Labs
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 */

#pragma once

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <elliptic/elliptic_data_io.h>

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

#define IOCTL_ELLIPTIC_DATA_IO_MIRROR \
	_IOW(IOCTL_ELLIPTIC_APP, 117, unsigned char *)

struct elliptic_device {
	int opened;
	struct cdev cdev;
	struct semaphore sem;
	struct device *device;
	struct elliptic_data el_data;
};

extern struct class *elliptic_class;

#define EL_PRINT_E(string, arg...) \
	pr_err("[ELUS] : (%s) : " string "\n", __func__, ##arg)

#define EL_PRINT_W(string, arg...) \
	pr_warn("[ELUS] : (%s) : " string "\n", __func__, ##arg)

#define EL_PRINT_I(string, arg...) \
	pr_info("[ELUS] : (%s) : " string "\n", __func__, ##arg)

#define EL_PRINT_D(string, arg...) \
	pr_debug("[ELUS] : (%s) : " string "\n", __func__, ##arg)

