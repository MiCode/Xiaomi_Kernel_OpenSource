/**
 * Copyright MI
 *
 */

#pragma once

#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <mius/mius_data_io.h>

#define MIUS_DEVICENAME "mius"
#define MIUS_NUM_DEVICES 2

#define IOCTL_MIUS_APP	197
#define MIRROR_TAG		0x3D0A4842

#define IOCTL_MIUS_DATA_IO_CANCEL \
	_IO(IOCTL_MIUS_APP, 2)

#define IOCTL_MIUS_ACTIVATE_ENGINE \
	_IOW(IOCTL_MIUS_APP, 3, int)

#define IOCTL_MIUS_SET_RAMP_DOWN \
	_IO(IOCTL_MIUS_APP, 4)

#define IOCTL_MIUS_SYSTEM_CONFIGURATION \
	_IOW(IOCTL_MIUS_APP, 5, int)

#define IOCTL_MIUS_DATA_IO_MIRROR \
	_IOW(IOCTL_MIUS_APP, 117, unsigned char *)

struct mius_device {
	int opened;
	struct cdev cdev;
	struct semaphore sem;
	struct mius_data el_data;
};

extern struct class *mius_class;

#define MI_PRINT_E(string, arg...) \
	pr_err("[MIUS] : (%s) : " string "\n", __func__, ##arg)

#define MI_PRINT_W(string, arg...) \
	pr_warn("[MIUS] : (%s) : " string "\n", __func__, ##arg)

#define MI_PRINT_I(string, arg...) \
	pr_info("[MIUS] : (%s) : " string "\n", __func__, ##arg)

#define MI_PRINT_D(string, arg...) \
	pr_debug("[MIUS] : (%s) : " string "\n", __func__, ##arg)

