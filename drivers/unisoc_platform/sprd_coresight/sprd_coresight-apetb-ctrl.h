// SPDX-License-Identifier: GPL-2.0-only
/*
 * sprd_coresight-apetb-ctrl.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _LINUX_CORESIGHT_APETB_H
#define _LINUX_CORESIGHT_APETB_H

#include <linux/device.h>
#include <linux/perf_event.h>
#include <linux/sched.h>

#define MAX_ETB_SOURCE_NUM			8

struct apetb_device;

struct apetb_ops {
	void (*init)(struct apetb_device *dbg);
	void (*exit)(struct apetb_device *dbg);
};

struct apetb_device {
	struct device dev;
	struct apetb_ops *ops;
	void *apetb_sink;
	void *apetb_source[MAX_ETB_SOURCE_NUM];
	u32 source_num;
	bool activated;
};

struct apetb_device *apetb_device_register(struct device *parent, struct apetb_ops *ops, const char *serdes_name);

#define to_apetb_device(d) container_of(d, struct apetb_device, dev)

#endif
