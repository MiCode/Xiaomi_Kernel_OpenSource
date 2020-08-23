/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
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
struct reviser_platform {
	unsigned int boundary;
	unsigned int bank_size;
	unsigned int vlm_size;
	unsigned int vlm_bank_max;
	unsigned int rmp_max;
	unsigned int ctx_max;
	unsigned int tcm_size;
	unsigned int tcm_bank_max;
	unsigned int mdla_max;
	unsigned int vpu_max;
	unsigned int edma_max;
	unsigned int up_max;
	unsigned int dram_offset;
	unsigned int tcm_addr;
	unsigned int vlm_addr;
	int hw_ver;
};


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
	int power_count;
	dev_t reviser_devt;
	struct cdev reviser_cdev;
	struct dentry *debug_root;

	struct mutex mutex_tcm;
	struct mutex mutex_ctx;
	struct mutex mutex_ctx_pgt;
	struct mutex mutex_remap;
	struct mutex mutex_power;

	spinlock_t lock_power;
	spinlock_t lock_dump;

	struct ctx_pgt *pvlm;

	wait_queue_head_t wait_ctx;
	wait_queue_head_t wait_tcm;

	struct reviser_dump dump;
	struct reviser_platform plat;

};



#endif
