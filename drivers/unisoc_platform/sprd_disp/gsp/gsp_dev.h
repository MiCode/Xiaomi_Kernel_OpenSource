/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_DEV_H
#define _GSP_DEV_H

#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/fs.h>

struct gsp_core;
struct gsp_dev;
struct gsp_sync_timeline;

#define for_each_gsp_core(core, gsp) \
	list_for_each_entry((core), &(gsp)->cores, list)

#define for_each_gsp_core_safe(core, tmp, gsp) \
	list_for_each_entry_safe((core), (tmp), &(gsp)->cores, list)

#define GSP_MAX_IO_CNT(gsp)	((gsp)->io_cnt)

#define GSP_DEVICE_NAME		"sprd-gsp"

struct gsp_dev {
	char name[32];
	u32 core_cnt;
	u32 io_cnt;

	struct miscdevice mdev;
	struct device *dev;

	struct gsp_interface *interface;

	struct list_head cores;
};

int gsp_dev_verify(struct gsp_dev *gsp);

struct device *gsp_dev_to_device(struct gsp_dev *gsp);
struct gsp_interface *gsp_dev_to_interface(struct gsp_dev *gsp);

int gsp_dev_is_idle(struct gsp_dev *gsp);

struct gsp_core *gsp_dev_to_core(struct gsp_dev *gsp, int index);
#endif
