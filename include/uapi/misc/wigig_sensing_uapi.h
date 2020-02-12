/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */
#ifndef __WIGIG_SENSING_UAPI_H__
#define __WIGIG_SENSING_UAPI_H__

#if !defined(__KERNEL__)
#define __user
#endif

#include <linux/types.h>
#include <linux/ioctl.h>

enum wigig_sensing_mode {
	WIGIG_SENSING_MODE_SEARCH             = 1,
	WIGIG_SENSING_MODE_FACIAL_RECOGNITION = 2,
	WIGIG_SENSING_MODE_GESTURE_DETECTION  = 3,
	WIGIG_SENSING_MODE_STOP               = 7,
	WIGIG_SENSING_MODE_CUSTOM             = 15
};

struct wigig_sensing_change_mode {
	enum wigig_sensing_mode mode;
	bool has_channel;
	uint32_t channel;
	uint32_t burst_size;
};

enum wigig_sensing_event {
	WIGIG_SENSING_EVENT_MIN,
	WIGIG_SENSING_EVENT_FW_READY,
	WIGIG_SENSING_EVENT_RESET,
	WIGIG_SENSING_EVENT_MAX,
};

#define WIGIG_SENSING_IOC_MAGIC	'r'

#define WIGIG_SENSING_IOCTL_SET_AUTO_RECOVERY      (0)
#define WIGIG_SENSING_IOCTL_GET_MODE               (1)
#define WIGIG_SENSING_IOCTL_CHANGE_MODE            (2)
#define WIGIG_SENSING_IOCTL_CLEAR_DATA             (3)
#define WIGIG_SENSING_IOCTL_GET_NUM_DROPPED_BURSTS (4)
#define WIGIG_SENSING_IOCTL_GET_EVENT              (5)
#define WIGIG_SENSING_IOCTL_GET_NUM_AVAIL_BURSTS   (6)

/**
 * Set auto recovery, which means that the system will go back to search mode
 *  after an error
 */
#define WIGIG_SENSING_IOC_SET_AUTO_RECOVERY \
	_IO(WIGIG_SENSING_IOC_MAGIC, WIGIG_SENSING_IOCTL_SET_AUTO_RECOVERY)

/**
 * Get current system mode of operation *
 *
 * Returns struct wigig_sensing_mode
 */
#define WIGIG_SENSING_IOC_GET_MODE \
	_IOR(WIGIG_SENSING_IOC_MAGIC, WIGIG_SENSING_IOCTL_GET_MODE, \
	     sizeof(enum wigig_sensing_mode))

/**
 * Change mode of operation and optionaly channel
 *
 * Note: Before issuing a CHANGE_MODE operation, the application must stop
 * reading data from the device node and clear any cached data. This comes to
 * prevent loss of burst boundary synchronization in the application.
 *
 * Returns burst size
 */
#define WIGIG_SENSING_IOC_CHANGE_MODE \
	_IOWR(WIGIG_SENSING_IOC_MAGIC, WIGIG_SENSING_IOCTL_CHANGE_MODE, \
	      sizeof(struct wigig_sensing_change_mode))

/**
 * Clear data buffer
 */
#define WIGIG_SENSING_IOC_CLEAR_DATA \
	_IO(WIGIG_SENSING_IOC_MAGIC, WIGIG_SENSING_IOCTL_CLEAR_DATA)

/**
 * Get number of bursts that were dropped due to data buffer overflow
 */
#define WIGIG_SENSING_IOC_GET_NUM_DROPPED_BURSTS \
	_IOR(WIGIG_SENSING_IOC_MAGIC, \
	     WIGIG_SENSING_IOCTL_GET_NUM_DROPPED_BURSTS, sizeof(uint32_t))

/**
 * Get asynchronous event (FW_READY, RESET)
 */
#define WIGIG_SENSING_IOC_GET_EVENT \
	_IOR(WIGIG_SENSING_IOC_MAGIC, WIGIG_SENSING_IOCTL_GET_EVENT, \
	     sizeof(enum wigig_sensing_event))

/**
 * Get number of available bursts in the data buffer
 */
#define WIGIG_SENSING_IOC_GET_NUM_AVAIL_BURSTS \
	_IOR(WIGIG_SENSING_IOC_MAGIC, WIGIG_SENSING_IOCTL_GET_NUM_AVAIL_BURSTS,\
	     sizeof(uint32_t))

#endif /* __WIGIG_SENSING_UAPI_H__ */
