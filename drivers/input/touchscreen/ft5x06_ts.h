/*
 * Copyright (C) 2011 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef FT5X0X_TS_H
#define FT5X0X_TS_H

#include <linux/types.h>

struct device;
struct ft5x06_data;

struct ft5x06_bus_ops {
	u16 bustype;
	int (*recv)(struct device *dev, void *buf, int len);
	int (*send)(struct device *dev, const void *buf, int len);
	int (*read)(struct device *dev, u8 addr, void *buf, u8 len);
	int (*write)(struct device *dev, u8 addr, const void *buf, u8 len);
};

int ft5x06_suspend(struct ft5x06_data *ft5x0x);
int ft5x06_resume(struct ft5x06_data *ft5x0x);

struct ft5x06_data *ft5x06_probe(struct device *dev,
				const struct ft5x06_bus_ops *bops);
void ft5x06_remove(struct ft5x06_data *ft5x06);

#endif
