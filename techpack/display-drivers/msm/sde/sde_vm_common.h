/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef __SDE_VM_COMMON_H__
#define __SDE_VM_COMMON_H__

#include <linux/gunyah/gh_rm_drv.h>
#include "sde_vm.h"

#define SDE_VM_MEM_LABEL 0x11

/**
 * sde_vm_populate_vmid - create and populate the rm vmid desc structure with
 *			  the given vmid
 * @vmid: vmid of the destination vm
 * @return: populated gh_notify_vmid_desc structure
 */
struct gh_notify_vmid_desc *sde_vm_populate_vmid(gh_vmid_t vmid);

/**
 * sde_vm_populate_acl - create and populate the access control list structure
 *			 for the given vm name
 * @vm_name: vm name enum published by the RM driver
 * @return: populated gh_acl_desc structure
 */
struct gh_acl_desc *sde_vm_populate_acl(enum gh_vm_names vm_name);

/**
 * sde_vm_populate_sgl - create and populate the scatter/gather list structure
 *			 with the given io memory list
 * @io_res: io resource list containing the io memory
 * @return: populated gh_sgl_desc structure
 */
struct gh_sgl_desc *sde_vm_populate_sgl(struct msm_io_res *io_res);

/**
 * sde_vm_populate_irq - create and populate the hw irq descriptor structure
 *			 with the given hw irq lines
 * @io_res: io resource list containing the irq numbers
 * @return: populated sde_vm_irq_desc structure
 */
struct sde_vm_irq_desc *sde_vm_populate_irq(struct msm_io_res *io_res);

/**
 * sde_vm_free_irq - free up the irq description structure
 * @irq_desc: handle to irq descriptor
 */
void sde_vm_free_irq(struct sde_vm_irq_desc *irq_desc);

/**
 * sde_vm_get_resources - collect io resource from all the VM clients
 * @sde_kms: handle to sde_kms
 * @io_res: pointer to msm_io_res structure to populate the resources
 * @return: 0 on success.
 */
int sde_vm_get_resources(struct sde_kms *sde_kms, struct msm_io_res *io_res);

/**
 * sde_vm_free_resources - free up the io resource list
 * @io_res: pointer to msm_io_res structure
 */
void sde_vm_free_resources(struct msm_io_res *io_res);

/**
 * sde_vm_post_acquire - handle post_acquire events with all the VM clients
 * @kms: handle to sde_kms
 */
int sde_vm_post_acquire(struct sde_kms *kms);

/**
 * sde_vm_pre_release - handle pre_release events with all the VM clients
 * @kms: handle to sde_kms
 */
int sde_vm_pre_release(struct sde_kms *kms);

/**
 * sde_vm_request_valid - check the validity of state transition request
 * @sde_kms: handle to sde_kms
 * @old_state: old crtc vm req state
 * @new_state: new crtc vm req state
 * @return: 0 on success
 */
int sde_vm_request_valid(struct sde_kms *sde_kms,
			  enum sde_crtc_vm_req old_state,
			  enum sde_crtc_vm_req new_state);

/**
 * sde_vm_msg_send - send display custom message through message queue
 * @sde_vm: handle to sde_vm struct
 * @msg: payload data
 * @msg_size: payload data size
 * @return: 0 on success
 */
int sde_vm_msg_send(struct sde_vm *sde_vm, void *msg, size_t msg_size);

#endif /* __SDE_VM_COMMON_H__ */
