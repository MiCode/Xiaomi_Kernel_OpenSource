/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Gene Chen <gene_chen@richtek.com>
 */

#ifndef _EXTDEV_IO_CLASS_H
#define _EXTDEV_IO_CLASS_H

#include <linux/regmap.h>

struct extdev_desc {
	const char *dirname;
	const char *devname;
	const char *typestr;
	int (*io_read)(void *drvdata, u16 reg, void *val, u16 size);
	int (*io_write)(void *drvdata, u16 reg, const void *val, u16 size);
	struct regmap *rmap;
};

struct extdev_io_device {
	struct device *dev;
	struct mutex io_lock;
	struct extdev_desc *desc;
	u16 reg;
	u16 size;
	bool access_lock;
	void *data_buffer;
	u16 data_buffer_size;
};

extern struct extdev_io_device * extdev_io_device_register(struct device *parent,
							   struct extdev_desc *desc);
extern void extdev_io_device_unregister(struct extdev_io_device *extdev);
extern struct extdev_io_device * devm_extdev_io_device_register(struct device *parent,
								struct extdev_desc *desc);

#endif /* _EXTDEV_IO_CLASS_H */
