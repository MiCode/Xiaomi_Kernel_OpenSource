/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _GH_PRIVATE_H
#define _GH_PRIVATE_H

#include <linux/gunyah/gh_rm_drv.h>
#include <linux/gunyah/gh_vm.h>
#include <linux/refcount.h>
#include <linux/gunyah_deprecated.h>
#include <linux/wait.h>

#define GH_EVENT_CREATE_VM 0
#define GH_EVENT_DESTROY_VM 1
#define GH_MAX_VCPUS 8

struct gh_vm_user_mem {
	struct sg_table *sgt;
	struct page **pages;
	unsigned long npages;
	unsigned long n_sgl_entries;
	struct gh_sgl_entry *sgl_entries;
	gh_memparcel_handle_t mem_handle;
};

struct gh_vcpu {
	u32 vcpu_id;
	struct gh_vm *vm;
};

struct gh_ext_reg {
	u32 ext_label;
	phys_addr_t ext_phys;
	ssize_t ext_size;
	gh_memparcel_handle_t ext_mem_handle;
};

struct gh_vm {
	bool is_secure_vm; /* is true for Qcom authenticated secure VMs */
	bool vm_run_once;
	bool keep_running;
	bool proxy_vm; /* is true for VMs scheduled by proxy driver*/
	u32 created_vcpus;
	u32 allowed_vcpus;
	gh_vmid_t vmid;
	struct gh_vcpu *vcpus[GH_MAX_VCPUS];
	char fw_name[GH_VM_FW_NAME_MAX];
	struct notifier_block rm_nb;
	struct gh_vm_status status;
	wait_queue_head_t vm_status_wait;
	int exit_type;
	refcount_t users_count;
	gh_memparcel_handle_t mem_handle;
	struct gh_ext_reg *ext_region;
	struct mutex vm_lock;
	struct gh_vm_user_mem *memory_mapping;
	struct mm_struct *mm; /* userspace tied to this vm */
};

/*
 * memory lending/donating and reclaiming APIs
 */
int gh_provide_mem(struct gh_vm *vm, phys_addr_t phys,
					ssize_t size, bool is_system_vm);
int gh_provide_user_mem(gh_vmid_t vmid, struct gh_vm_user_mem *memory_mapping);
int gh_reclaim_mem(struct gh_vm *vm, phys_addr_t phys,
					ssize_t size, bool is_system_vm);
int gh_reclaim_user_mem(struct gh_vm *vm);
long gh_vm_configure(u16 auth_mech, u64 image_offset,
			u64 image_size, u64 dtb_offset, u64 dtb_size,
			u32 pas_id, struct gh_vm *vm);
long gh_vm_init(const char *fw_name, struct gh_vm *vm);
int gh_vm_alloc_ext_region(struct gh_vm *vm);
void gh_uevent_notify_change(unsigned int type, struct gh_vm *vm);
bool gh_firmware_is_legacy(void);

#endif /* _GH_PRIVATE_H */
