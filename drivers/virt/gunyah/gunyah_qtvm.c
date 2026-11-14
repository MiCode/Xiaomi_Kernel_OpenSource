// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/of_platform.h>
#include <linux/gunyah_qtvm.h>
#include "vm_mgr.h"

#define PAS_VM_METADATA_SZ 8192

static DEFINE_MUTEX(gunyah_qtvm_lock);
static LIST_HEAD(gunyah_qtvm_list);
SRCU_NOTIFIER_HEAD_STATIC(gunyah_qtvm_notifier);

struct gunyah_qtvm {
	struct gunyah_vm *ghvm;
	struct gunyah_vm_parcel *parcel_list;
	struct list_head list;
	u64 vm_image_addr;
	u64 vm_image_size;
	u32 pas_id;
	u16 vmid;
};

int gunyah_qtvm_register_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&gunyah_qtvm_notifier, nb);
}
EXPORT_SYMBOL_GPL(gunyah_qtvm_register_notifier);

int gunyah_qtvm_unregister_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&gunyah_qtvm_notifier, nb);
}
EXPORT_SYMBOL_GPL(gunyah_qtvm_unregister_notifier);

static void gunyah_notify_clients(struct gunyah_qtvm *vm,
					enum gunyah_qtvm_state state)
{
	srcu_notifier_call_chain(&gunyah_qtvm_notifier, state, &vm->vmid);
}

static u16 gunyah_qtvm_pre_alloc_vmid(struct gunyah_vm *ghvm)
{
	struct gunyah_qtvm *vm = ghvm->auth_vm_mgr_data;

	return vm->vmid;
}

static int gunyah_qtvm_pre_vm_configure(struct gunyah_vm *ghvm)
{
	struct gunyah_qtvm *vm = ghvm->auth_vm_mgr_data;
	u64 start_gfn;
	int ret;

	/*
	 * For QTVMs, the metadata is always placed at the beginning of the
	 * main VM memory and will always be of fixed size decided at the
	 * build time while signing the VM image. The metadata contains the
	 * signing information needed by firmware to authenticate the VM image.
	 * VM image once loaded into the memory looks like this:
	 *
	 *           start |----------------------|
	 *                 | MDT header + hashes  |
	 *                 |----------------------|
	 *                 |       Kernel         |
	 *                 |----------------------|
	 *                 |         DTB          |
	 *                 |----------------------|
	 *                 |      CPIO/Ramdisk    |
	 *                 |----------------------|
	 */

	ghvm->config_image.parcel.start = gunyah_gpa_to_gfn(vm->vm_image_addr);
	ghvm->config_image.parcel.pages = gunyah_gpa_to_gfn(vm->vm_image_size);

	ghvm->config_image.image_offset = 0;
	ghvm->config_image.image_size = PAS_VM_METADATA_SZ;

	if (ghvm->dtb.config.size > 0) {
		ghvm->config_image.dtb_offset = ghvm->dtb.config.guest_phys_addr -
					gunyah_gfn_to_gpa(ghvm->config_image.parcel.start);
		ghvm->config_image.dtb_size = ghvm->dtb.config.size;

		if ((ghvm->dtb.config.guest_phys_addr + ghvm->config_image.dtb_size) >
		    (gunyah_gfn_to_gpa(ghvm->config_image.parcel.start) +
			gunyah_gfn_to_gpa(ghvm->config_image.parcel.pages))) {
			/*
			 * DTB is out of the config image bounds.
			 * This is should not happen!
			 */
			dev_err(ghvm->parent, "DTB is outside the image parcel\n");
			return -EINVAL;
		}
	}

	/*
	 * RM would expect to have all the memory mentioned
	 * in the VM DT to be shared/lent before the VM starts.
	 * We will lend the primary memory parcel as
	 * part of the vm_configure operation. So, share the rest
	 * of the VM memory here.
	 */
	start_gfn = gunyah_gpa_to_gfn(vm->vm_image_addr + vm->vm_image_size);
	ret = gunyah_share_range_as_parcels(ghvm, start_gfn, ULONG_MAX, &vm->parcel_list);
	if (ret) {
		dev_err(ghvm->parent, "Failed to share non primary parcel(s) before VM start\n");
		return ret;
	}

	return 0;
}

static int gunyah_qtvm_authenticate(struct gunyah_vm *ghvm)
{
	struct gunyah_qtvm *vm = ghvm->auth_vm_mgr_data;
	struct gunyah_rm_vm_authenticate_param_entry entry;
	int ret;

	entry.param_type = GUNYAH_VM_AUTH_PARAM_PAS_ID;
	entry.param = vm->pas_id;

	ret = gunyah_rm_vm_authenticate(ghvm->rm, vm->vmid, 1, &entry);
	if (ret) {
		dev_err(ghvm->parent, "Failed to Authenticate VM: %d\n", ret);
		return ret;
	}

	return 0;
}

static int gunyah_qtvm_pre_vm_start(struct gunyah_vm *ghvm)
{
	struct gunyah_qtvm *vm = ghvm->auth_vm_mgr_data;

	gunyah_notify_clients(vm, GUNYAH_QTVM_BEFORE_POWERUP);
	return 0;
}

static void gunyah_qtvm_vm_start_fail(struct gunyah_vm *ghvm)
{
	struct gunyah_qtvm *vm = ghvm->auth_vm_mgr_data;

	gunyah_notify_clients(vm, GUNYAH_QTVM_POWERUP_FAIL);
}

static int gunyah_qtvm_pre_vm_reset(struct gunyah_vm *ghvm)
{
	struct gunyah_qtvm *vm = ghvm->auth_vm_mgr_data;

	gunyah_notify_clients(vm, GUNYAH_QTVM_EXITED);
	return 0;
}

static int gunyah_qtvm_post_vm_reset(struct gunyah_vm *ghvm)
{
	struct gunyah_qtvm *vm = ghvm->auth_vm_mgr_data;

	gunyah_notify_clients(vm, GUNYAH_QTVM_EARLY_POWEROFF);
	return 0;
}

static struct gunyah_auth_vm_mgr_ops vm_ops = {
	.pre_alloc_vmid = gunyah_qtvm_pre_alloc_vmid,
	.pre_vm_configure = gunyah_qtvm_pre_vm_configure,
	.vm_authenticate = gunyah_qtvm_authenticate,
	.pre_vm_start = gunyah_qtvm_pre_vm_start,
	.vm_start_fail = gunyah_qtvm_vm_start_fail,
	.pre_vm_reset = gunyah_qtvm_pre_vm_reset,
	.post_vm_reset = gunyah_qtvm_post_vm_reset,
};

static long gunyah_qtvm_attach(struct gunyah_vm *ghvm, struct gunyah_auth_desc *desc)
{
	struct gunyah_qtvm_auth_arg arg;
	struct gunyah_qtvm *vm;
	void __user *argp;

	if (desc->arg_size > sizeof(struct gunyah_qtvm_auth_arg))
		return -EINVAL;

	argp = u64_to_user_ptr(desc->arg);
	if (copy_from_user(&arg, argp, desc->arg_size))
		return -EFAULT;

	if (overflows_type(arg.guest_phys_addr + arg.size,
				   u64))
		return -EOVERFLOW;

	mutex_lock(&gunyah_qtvm_lock);
	vm = kzalloc(sizeof(*vm), GFP_KERNEL_ACCOUNT);
	if (!vm) {
		mutex_unlock(&gunyah_qtvm_lock);
		return -ENOMEM;
	}

	vm->vmid = arg.vm_id;
	vm->pas_id = arg.peripheral_id;

	/* This would be the primary Image parcel */
	vm->vm_image_addr = arg.guest_phys_addr;
	vm->vm_image_size = arg.size;
	vm->ghvm = ghvm;

	ghvm->auth = GUNYAH_RM_VM_AUTH_QCOM_TRUSTED_VM;
	ghvm->auth_vm_mgr_ops = &vm_ops;
	ghvm->auth_vm_mgr_data = vm;

	list_add(&vm->list, &gunyah_qtvm_list);
	mutex_unlock(&gunyah_qtvm_lock);
	return 0;
}

static void gunyah_qtvm_detach(struct gunyah_vm *ghvm)
{
	struct gunyah_qtvm *vm = ghvm->auth_vm_mgr_data;

	kfree(vm->parcel_list);
	gunyah_notify_clients(vm, GUNYAH_QTVM_POWEROFF);
	list_del(&vm->list);
	kfree(vm);
	ghvm->auth_vm_mgr_ops = NULL;
	ghvm->auth_vm_mgr_data = NULL;
}

static struct gunyah_auth_vm_mgr auth_vm = {
	.type = GUNYAH_QCOM_TRUSTED_VM_TYPE,
	.name = "gunyah_qtvm",
	.mod = THIS_MODULE,
	.vm_attach = gunyah_qtvm_attach,
	.vm_detach = gunyah_qtvm_detach,
};

static int __init gunyah_qtvm_init(void)
{
	mutex_init(&gunyah_qtvm_lock);
	return gunyah_auth_vm_mgr_register(&auth_vm);
}

static void __exit gunyah_qtvm_exit(void)
{
	gunyah_auth_vm_mgr_unregister(&auth_vm);
}

module_init(gunyah_qtvm_init);
module_exit(gunyah_qtvm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Gunyah Qualcomm Trusted VM Driver");
