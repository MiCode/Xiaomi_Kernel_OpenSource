/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_VM_EVENT_H__
#define __SDE_VM_EVENT_H__

#include <linux/list.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <drm/drm_device.h>
#include <linux/platform_device.h>

/**
 * struct - msm_io_irq_entry - define irq item
 * @label: gh_irq_label for the irq
 * @irq_num: linux mapped irq num
 * @list: list head pointer
 */
struct msm_io_irq_entry {
	u32 label;
	u32 irq_num;
	struct list_head list;
};

/**
 * struct - msm_io_mem_entry - define io memory item
 * @base: reg base
 * @size: size of the reg range
 * @list: list head pointer
 */
struct msm_io_mem_entry {
	phys_addr_t base;
	phys_addr_t size;
	struct list_head list;
};

/**
 * struct - msm_io_res - represents the hw resources for vm sharing
 * @irq: list of IRQ's of all the dislay sub-devices
 * @mem: list of IO memory ranges of all the display sub-devices
 */
struct msm_io_res {
	struct list_head irq;
	struct list_head mem;
};

/**
 * struct msm_vm_ops - hooks for communication with vm clients
 * @vm_pre_hw_release: invoked before releasing the HW
 * @vm_post_hw_acquire: invoked before pushing the first commit
 * @vm_check: invoked to check the readiness of the vm_clients
 *	      before releasing the HW
 * @vm_get_io_resources: invoked to collect HW resources
 */
struct msm_vm_ops {
	int (*vm_pre_hw_release)(void *priv_data);
	int (*vm_post_hw_acquire)(void *priv_data);
	int (*vm_check)(void *priv_data);
	int (*vm_get_io_resources)(struct msm_io_res *io_res, void *priv_data);
};

/**
 * msm_vm_client_entry - defines the vm client info
 * @ops: client vm_ops
 * @dev: clients device id. Used in unregister
 * @data: client custom data
 * @list: linked list entry
 */
struct msm_vm_client_entry {
	struct msm_vm_ops ops;
	struct device *dev;
	void *data;
	struct list_head list;
};

/**
 * msm_register_vm_event - api for display dependent drivers(clients) to
 *                         register for vm events
 * @dev: msm device
 * @client_dev: client device
 * @ops: vm event hooks
 * @priv_data: client custom data
 */
int msm_register_vm_event(struct device *dev, struct device *client_dev,
			  struct msm_vm_ops *ops, void *priv_data);

/**
 * msm_unregister_vm_event - api for display dependent drivers(clients) to
 *                           unregister from vm events
 * @dev: msm device
 * @client_dev: client device
 */
void msm_unregister_vm_event(struct device *dev, struct device *client_dev);

#endif //__SDE_VM_EVENT_H__
