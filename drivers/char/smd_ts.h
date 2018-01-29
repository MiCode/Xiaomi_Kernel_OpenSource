/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#ifndef SMD_TS_H
#define SMD_TS_H

#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/list.h>
#include <linux/hash.h>
#include <linux/msm_ion.h>
#include <soc/qcom/smd.h>
#include <soc/qcom/subsystem_notif.h>
#include <linux/msm_iommu_domains.h>
#include <linux/scatterlist.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/dma-contiguous.h>
#include <linux/iommu.h>
#include <linux/kref.h>
#include <linux/debugfs.h>
#include <linux/list.h>

#define TS_FIFO_SIZE		(1024)
#define TS_DIFF_BUF_NUM		33

struct smd_ts_apps {
	spinlock_t lock;
	struct completion work;

	/* for dev node */
	struct cdev cdev;
	struct device *dev;
	dev_t dev_no;
	struct class *class;
	const struct file_operations *fops;

	/* handler for smd event */
	void *event_handler;

	/* point to the head of buf, const */
	uint64_t *ts_buf;
	/* point to the buf to write, always changed */
	uint64_t *buf_ptr;
	/* the total buf length */
	unsigned int buf_len;
	/* the length of ready buf */
	unsigned int ready_buf_len;

	smd_channel_t *chan;
	/* the name of smd channel */
	const char *ch_name;
	/* the type of smd channel */
	unsigned int ch_type;
};

#endif
