/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __APUSYS_REVISER_DRV_H__
#define __APUSYS_REVISER_DRV_H__

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>

struct reviser_dump {
	unsigned int err_count;
	unsigned int unknown_count;
};
struct reviser_resource {
	unsigned int iova;
	unsigned int size;
};
/* reviser driver's private structure */
struct reviser_dev_info {
	void *pctrl_top;
	void *vlm_base;
	void *tcm_base;
	struct reviser_resource vlm;
	struct reviser_resource tcm;

	void *dram_base;
	void *int_base;

	bool init_done;
	struct device *dev;
	bool power;
	int power_count;
	dev_t reviser_devt;
	struct cdev reviser_cdev;
	struct dentry *debug_root;

	struct mutex mutex_tcm;
	struct mutex mutex_ctxid;
	struct mutex mutex_vlm_pgtable;
	struct mutex mutex_remap;
	struct mutex mutex_power;

	spinlock_t lock_power;
	spinlock_t lock_dump;

	struct vlm_pgtable *pvlm;

	wait_queue_head_t wait_ctxid;
	wait_queue_head_t wait_tcm;

	struct reviser_dump dump;
};



#endif
