/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SAPU_DRIVER_H_
#define __SAPU_DRIVER_H_

#include <kree/system.h>
#include <kree/mem.h>
#include <linux/refcount.h>
#include <uapi/asm-generic/errno.h>


#define APUSYS_SAPU_IOC_MAGIC 'S'
#define APUSYS_SAPU_DATAMEM	_IOWR(APUSYS_SAPU_IOC_MAGIC, 1, struct dmArg)
#define APUSYS_POWER_CONTROL _IOWR(APUSYS_SAPU_IOC_MAGIC, 2, struct PWRarg)
#define MTEE_SESSION_NAME_LEN 32

struct apusys_sapu_data {
	struct platform_device *pdev;
	struct miscdevice mdev;
	struct kref lock_ref_cnt;
};

struct sapu_lock_rpmsg_device {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;
	struct completion ack;
};

struct dmArg {
	int fd;
	char haSrvName[MTEE_SESSION_NAME_LEN];
	int command;
	uint64_t model_hd_ha;
};

struct PWRarg {
	uint32_t lock;
};

struct haArg {
	uint32_t handle;
	dma_addr_t dma_addr;
	uint64_t model_hd_ha;
};


long apusys_sapu_internal_ioctl(struct file *filep, unsigned int cmd,
				void __user *arg, unsigned int compat);
struct apusys_sapu_data *get_apusys_sapu_data(struct file *filep);
struct sapu_lock_rpmsg_device *get_rpm_dev(void);
struct mutex *get_rpm_mtx(void);
int *get_lock_ref_cnt(void);
int sapu_ha_bridge(struct dmArg *ioDmArg, struct haArg *ioHaArg);

#endif // !__SAPU_DRIVER_H_
