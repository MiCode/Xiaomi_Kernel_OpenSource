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

/* reviser driver's private structure */
struct reviser_dev_info {
	void *pctrl_top;
	void *vlm_base;
	void *tcm_base;

	void *dram_base;
	void *int_base;

	bool init_done;
	struct device *dev;
	bool power;
	dev_t reviser_devt;
	struct cdev reviser_cdev;
	struct dentry *debug_root;

	struct mutex mutex_tcm;
	struct mutex mutex_ctxid;
	struct mutex mutex_vlm_pgtable;
	struct mutex mutex_remap;

	spinlock_t power_lock;

	struct vlm_pgtable *pvlm;

	wait_queue_head_t wait_ctxid;
	wait_queue_head_t wait_tcm;
};



#endif
