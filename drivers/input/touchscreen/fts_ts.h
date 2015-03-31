/*
 * Copyright (C) 2011-2015 XiaoMi, Inc.
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

#ifndef FTS_TS_H
#define FTS_TS_H

#include <linux/types.h>

struct device;
struct fts_data;

struct fts_bus_ops {
	u16 bustype;
	int (*recv)(struct device *dev, void *wbuf, int wlen, void *rbuf, int rlen);
	int (*send)(struct device *dev, void *buf, int len);
};

struct fts_data *fts_probe(struct device *dev,
				const struct fts_bus_ops *bops, int irq);
void fts_remove(struct fts_data *fts);

int fts_suspend(struct fts_data *fts);
int fts_resume(struct fts_data *fts);

#endif
