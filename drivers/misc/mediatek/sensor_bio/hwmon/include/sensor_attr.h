/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef _SENSOR_ATTR_H
#define _SENSOR_ATTR_H
#include <linux/major.h>
#include <linux/types.h>

struct sensor_attr_t  {
	unsigned char minor;
	const char *name;
	const struct file_operations *fops;
	struct list_head list;
	struct device *parent;
	struct device *this_device;
};
struct sensor_attr_dev {
	struct device *dev;
};
extern int sensor_attr_register(struct sensor_attr_t *misc);
extern int sensor_attr_deregister(struct sensor_attr_t *misc);

#endif
