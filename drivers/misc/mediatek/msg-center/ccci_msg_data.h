/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __CCCI_MSG_DATA_H__
#define __CCCI_MSG_DATA_H__

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/skbuff.h>






struct ccci_usb_upstream_data {
	int transfer_id;
	const void *buffer;
	unsigned int length;
};

struct ccci_usb_push_data {
	int ch_id;
	void *buf;
	int count;
};

struct ccci_usb_intercept_data {
	int ch_id;
	unsigned int intercept;
};

#endif	/* __CCCI_MSG_DATA_H__ */
