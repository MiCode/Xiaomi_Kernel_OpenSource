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

enum REVISER_POOL_E {
	REVSIER_POOL_TCM,
	REVSIER_POOL_SLBS,
	REVSIER_POOL_EXT,
	REVSIER_POOL_MAX,
};

enum REVISER_DEVICE_E {
	REVISER_DEVICE_NONE,
	REVISER_DEVICE_SECURE_MD32,
	REVISER_DEVICE_NORMAL_MD32,
	REVISER_DEVICE_MDLA,
	REVISER_DEVICE_VPU,
	REVISER_DEVICE_EDMA,
	REVISER_DEVICE_MAX,
};

struct reviser_platform {
	uint32_t boundary;
	uint32_t bank_size;
	uint32_t vlm_size;
	uint32_t vlm_bank_max;
	uint32_t vlm_addr;
	uint32_t dram_max;
	uint32_t pool_max;
	uint32_t pool_type[REVSIER_POOL_MAX];
	uint32_t pool_base[REVSIER_POOL_MAX];
	uint32_t pool_step[REVSIER_POOL_MAX];
	uint32_t pool_size[REVSIER_POOL_MAX];
	uint32_t pool_bank_max[REVSIER_POOL_MAX];
	uint32_t pool_addr[REVSIER_POOL_MAX];
	uint32_t device[REVISER_DEVICE_MAX];
	uint64_t dram[32];
	uint32_t hw_ver;
	uint32_t sw_ver;
	uint32_t fix_dram;

	uint32_t rmp_max;
	uint32_t ctx_max;

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
	struct reviser_resource pool[REVSIER_POOL_MAX];
	struct reviser_resource isr;
	struct reviser_resource dram;
};

/* reviser driver's private structure */
struct reviser_dev_info {
	bool init_done;
	struct device *dev;
	dev_t reviser_devt;
	struct cdev reviser_cdev;
	struct rpmsg_device *rpdev;

	struct reviser_resource_mgt rsc;
	struct reviser_power power;
	struct reviser_lock lock;
	struct ctx_pgt *pvlm;
	struct reviser_dump dump;
	struct reviser_platform plat;
	struct reviser_platform remote;
};



#endif
