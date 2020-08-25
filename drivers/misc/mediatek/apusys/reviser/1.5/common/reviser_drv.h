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
	unsigned int hw_ver;
};
struct reviser_lock {
	struct mutex mutex_tcm;
	struct mutex mutex_ctx;
	struct mutex mutex_ctx_pgt;
	struct mutex mutex_remap;
	struct mutex mutex_power;
	spinlock_t lock_power;
	spinlock_t lock_dump;
	wait_queue_head_t wait_ctx;
	wait_queue_head_t wait_tcm;
};
struct reviser_resource {
	unsigned int addr;
	unsigned int size;
	void *base;
};
struct reviser_power {
	bool power;
	int power_count;
};
struct reviser_resource_mgt {
	struct reviser_resource ctrl;
	struct reviser_resource vlm;
	struct reviser_resource tcm;
	struct reviser_resource isr;
	struct reviser_resource dram;
};
/* reviser driver's private structure */
struct reviser_dev_info {
	bool init_done;
	struct device *dev;
	dev_t reviser_devt;
	struct cdev reviser_cdev;

	struct reviser_resource_mgt rsc;
	struct reviser_power power;
	struct reviser_lock lock;
	struct ctx_pgt *pvlm;
	struct reviser_dump dump;
	struct reviser_platform plat;

};



#endif
