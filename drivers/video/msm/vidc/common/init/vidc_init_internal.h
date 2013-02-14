/* Copyright (c) 2010, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef VIDC_INIT_INTERNAL_H
#define VIDC_INIT_INTERNAL_H

#include <linux/cdev.h>

struct vidc_timer {
	struct list_head list;
	struct timer_list hw_timeout;
	void (*cb_func)(void *);
	void *userdata;
};

struct vidc_dev {
	struct cdev cdev;
	struct device *device;
	resource_size_t phys_base;
	void __iomem *virt_base;
	unsigned int irq;
	unsigned int ref_count;
	unsigned int firmware_refcount;
	unsigned int get_firmware;
	struct mutex lock;
	s32 device_handle;
	struct list_head vidc_timer_queue;
	struct work_struct vidc_timer_worker;
};

#endif
