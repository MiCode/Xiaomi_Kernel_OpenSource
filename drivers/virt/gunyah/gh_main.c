// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/anon_inodes.h>
#include <linux/miscdevice.h>
#include <linux/pagemap.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <soc/qcom/secure_buffer.h>
#include <linux/gunyah_deprecated.h>

#include "gh_secure_vm_virtio_backend.h"
#include "gh_secure_vm_loader.h"
#include "gh_proxy_sched.h"
#include "gh_private.h"

#define MAX_VCPU_NAME	20 /* gh-vcpu:u32_max +1 */

SRCU_NOTIFIER_HEAD_STATIC(gh_vm_notifier);

/*
 * Support for RM calls and the wait for change of status
 */
#define gh_rm_call_and_set_status(name) \
static int gh_##name(struct gh_vm *vm, int vm_status)			 \
{									 \
	int ret = 0;							 \
	ret = ghd_rm_##name(vm->vmid);					 \
	if (!ret)							 \
		vm->status.vm_status = vm_status;			 \
	return ret;							 \
}

gh_rm_call_and_set_status(vm_start);

#define gh_wait_for_vm_status(vm, wait_status)				\
	wait_event(vm->vm_status_wait, (vm->status.vm_status == wait_status))

int gh_register_vm_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_register(&gh_vm_notifier, nb);
}
EXPORT_SYMBOL_GPL(gh_register_vm_notifier);

int gh_unregister_vm_notifier(struct notifier_block *nb)
{
	return srcu_notifier_chain_unregister(&gh_vm_notifier, nb);
}
EXPORT_SYMBOL_GPL(gh_unregister_vm_notifier);

static void gh_notify_clients(struct gh_vm *vm, unsigned long val)
{
	srcu_notifier_call_chain(&gh_vm_notifier, val, &vm->vmid);
}

static void gh_notif_vm_status(struct gh_vm *vm,
			struct gh_rm_notif_vm_status_payload *status)
{
	if (vm->vmid != status->vmid)
		return;

	/* Wake up the waiters only if there's a change in any of the states */
	if (status->vm_status != vm->status.vm_status) {
		switch (status->vm_status) {
		case GH_RM_VM_STATUS_RESET:
		case GH_RM_VM_STATUS_READY:
			pr_info("VM: %d status %d complete\n", vm->vmid,
							status->vm_status);
			vm->status.vm_status = status->vm_status;
			wake_up(&vm->vm_status_wait);
			break;
		case GH_RM_VM_STATUS_RESET_FAILED:
			pr_err("VM %d RESET failed with status %d\n",
					vm->vmid, status->vm_status);
			/*
			 * Forcibly set the vm_status to RESET so that
			 * the VM can be destroyed and the next start
			 * of the VM will be unsuccessful and userspace
			 * can make the right decision.
			 */
			vm->status.vm_status = GH_RM_VM_STATUS_RESET;
			wake_up(&vm->vm_status_wait);
			break;
		}
	}
}

static void gh_notif_vm_exited(struct gh_vm *vm,
			struct gh_rm_notif_vm_exited_payload *vm_exited)
{
	if (vm->vmid != vm_exited->vmid)
		return;

	mutex_lock(&vm->vm_lock);
	vm->exit_type = vm_exited->exit_type;
	switch (vm_exited->exit_type) {
	case GH_RM_VM_EXIT_TYPE_WDT_BITE:
	case GH_RM_VM_EXIT_TYPE_HYP_ERROR:
	case GH_RM_VM_EXIT_TYPE_ASYNC_EXT_ABORT:
		gh_notify_clients(vm, GH_VM_CRASH);
		break;
	}

	vm->status.vm_status = GH_RM_VM_STATUS_EXITED;
	gh_wakeup_all_vcpus(vm->vmid);
	wake_up(&vm->vm_status_wait);
	mutex_unlock(&vm->vm_lock);
}

static int gh_vm_rm_notifier_fn(struct notifier_block *nb,
					unsigned long cmd, void *data)
{
	struct gh_vm *vm;

	vm = container_of(nb, struct gh_vm, rm_nb);

	switch (cmd) {
	case GH_RM_NOTIF_VM_STATUS:
		gh_notif_vm_status(vm, data);
		break;
	case GH_RM_NOTIF_VM_EXITED:
		gh_notif_vm_exited(vm, data);
		break;
	}

	return NOTIFY_DONE;
}

static void gh_vm_cleanup(struct gh_vm *vm)
{
	gh_vmid_t vmid = vm->vmid;
	int vm_status = vm->status.vm_status;
	int ret;

	gh_notify_clients(vm, GH_VM_EXITED);
	switch (vm_status) {
	case GH_RM_VM_STATUS_EXITED:
	case GH_RM_VM_STATUS_RUNNING:
	case GH_RM_VM_STATUS_READY:
		ret = gh_rm_unpopulate_hyp_res(vmid, vm->fw_name);
		if (ret)
			pr_warn("Failed to unpopulate hyp resources: %d\n", ret);
		fallthrough;
	case GH_RM_VM_STATUS_INIT:
	case GH_RM_VM_STATUS_AUTH:
		ret = ghd_rm_vm_reset(vmid);
		if (!ret) {
			gh_wait_for_vm_status(vm, GH_RM_VM_STATUS_RESET);
		} else
			pr_warn("Reset is unsuccessful for VM:%d\n", vmid);

		gh_notify_clients(vm, GH_VM_EARLY_POWEROFF);
		ret = gh_virtio_mmio_exit(vmid, vm->fw_name);
		if (ret)
			pr_warn("Failed to free virtio resources : %d\n", ret);

		if (vm->is_secure_vm) {
			ret = gh_secure_vm_loader_reclaim_fw(vm);
			if (ret)
				pr_warn("Failed to reclaim mem VMID: %d: %d\n", vmid, ret);
		}
		fallthrough;
	case GH_RM_VM_STATUS_LOAD:
		ret = gh_rm_vm_dealloc_vmid(vmid);
		if (ret)
			pr_warn("Failed to dealloc VMID: %d: %d\n", vmid, ret);
	}

	vm->status.vm_status = GH_RM_VM_STATUS_NO_STATE;
}

static int gh_exit_vm(struct gh_vm *vm, u32 stop_reason, u8 stop_flags)
{
	gh_vmid_t vmid = vm->vmid;
	int ret = -EINVAL;

	if (!vmid)
		return -ENODEV;

	mutex_lock(&vm->vm_lock);
	if (vm->status.vm_status == GH_RM_VM_STATUS_EXITED) {
		pr_info("VM:%d already exited\n", vmid);
		mutex_unlock(&vm->vm_lock);
		return 0;
	} else if (vm->status.vm_status != GH_RM_VM_STATUS_RUNNING) {
		pr_err("VM:%d is not running\n", vmid);
		mutex_unlock(&vm->vm_lock);
		return -ENODEV;
	}

	ret = ghd_rm_vm_stop(vmid, stop_reason, stop_flags);
	if (ret) {
		pr_err("Failed to stop the VM:%d ret %d\n", vmid, ret);
		mutex_unlock(&vm->vm_lock);
		return ret;
	}
	mutex_unlock(&vm->vm_lock);

	gh_wait_for_vm_status(vm, GH_RM_VM_STATUS_EXITED);

	return ret;
}

static int gh_stop_vm(struct gh_vm *vm)
{
	gh_vmid_t vmid = vm->vmid;
	int ret = -EINVAL;

	if (vm->proxy_vm && !(vm->keep_running == true &&
			      vm->status.vm_status == GH_RM_VM_STATUS_RUNNING))
		ret = gh_exit_vm(vm, GH_VM_STOP_RESTART,
					GH_RM_VM_STOP_FLAG_FORCE_STOP);
	else
		ret = gh_exit_vm(vm, GH_VM_STOP_RESTART, 0);

	if (ret && ret != -ENODEV)
		goto err_vm_force_stop;

	return ret;

err_vm_force_stop:
	ret = gh_exit_vm(vm, GH_VM_STOP_FORCE_STOP,
					GH_RM_VM_STOP_FLAG_FORCE_STOP);
	if (ret)
		pr_err("VM:%d force stop has failed\n", vmid);
	return ret;
}

void gh_destroy_vcpu(struct gh_vcpu *vcpu)
{
	struct gh_vm *vm = vcpu->vm;
	u32 id = vcpu->vcpu_id;

	kfree(vcpu);
	vm->vcpus[id] = NULL;
	vm->created_vcpus--;
}

int gh_destroy_vm(struct gh_vm *vm)
{
	int vcpu_id, ret;

	if (vm->status.vm_status == GH_RM_VM_STATUS_NO_STATE)
		goto clean_vm;

	ret = gh_stop_vm(vm);
	if (ret)
		return ret;

	for (vcpu_id = 0; vm->created_vcpus && vcpu_id < GH_MAX_VCPUS; vcpu_id++) {
		if (!vm->vcpus[vcpu_id])
			continue;
		gh_destroy_vcpu(vm->vcpus[vcpu_id]);
	}

	gh_vm_cleanup(vm);
	gh_notify_clients(vm, GH_VM_POWEROFF);

	gh_uevent_notify_change(GH_EVENT_DESTROY_VM, vm);
	memset(vm->fw_name, 0, GH_VM_FW_NAME_MAX);
	vm->vmid = 0;
	kfree(vm->ext_region);

clean_vm:
	gh_rm_unregister_notifier(&vm->rm_nb);
	mutex_destroy(&vm->vm_lock);
	kfree(vm);
	return 0;
}

static void gh_get_vm(struct gh_vm *vm)
{
	refcount_inc(&vm->users_count);
}

static int gh_put_vm(struct gh_vm *vm)
{
	int ret = 0;

	if (refcount_dec_and_test(&vm->users_count)) {
		ret = gh_destroy_vm(vm);
		if (ret)
			pr_err("Failed to destroy VM:%d ret %d\n", vm->vmid,
			       ret);
	}

	return ret;
}

static int gh_vcpu_release(struct inode *inode, struct file *filp)
{
	struct gh_vcpu *vcpu = filp->private_data;
	int ret;

	/* need to create workqueue if critical vm */
	if (vcpu->vm->keep_running &&
	    vcpu->vm->status.vm_status == GH_RM_VM_STATUS_RUNNING)
		gh_vcpu_create_wq(vcpu->vm->vmid, vcpu->vcpu_id);

	ret = gh_put_vm(vcpu->vm);

	return ret;
}

static int gh_vcpu_ioctl_run(struct gh_vcpu *vcpu)
{
	struct gh_hcall_vcpu_run_resp vcpu_run;
	struct gh_vm *vm = vcpu->vm;
	int ret = 0;

	mutex_lock(&vm->vm_lock);

	if (vm->status.vm_status == GH_RM_VM_STATUS_RUNNING) {
		mutex_unlock(&vm->vm_lock);
		goto start_vcpu_run;
	}

	if (vm->vm_run_once &&
		vm->status.vm_status != GH_RM_VM_STATUS_RUNNING) {
		pr_err("VM:%d has failed to run before\n", vm->vmid);
		mutex_unlock(&vm->vm_lock);
		return -EINVAL;
	}

	vm->vm_run_once = true;

	if (vm->is_secure_vm &&
		vm->created_vcpus != vm->allowed_vcpus) {
		pr_err("VCPUs created %d doesn't match with allowed %d for VM %d\n",
			vm->created_vcpus, vm->allowed_vcpus,
							vm->vmid);
		ret = -EINVAL;
		mutex_unlock(&vm->vm_lock);
		return ret;
	}

	if (vm->status.vm_status != GH_RM_VM_STATUS_READY) {
		pr_err("VM:%d not ready to start\n", vm->vmid);
		ret = -EINVAL;
		mutex_unlock(&vm->vm_lock);
		return ret;
	}

	gh_notify_clients(vm, GH_VM_BEFORE_POWERUP);

	ret = gh_rm_vm_set_time_base(vm->vmid);
	if (ret)
		pr_err("Failed to set time base for VM:%d %d\n", vm->vmid, ret);

	ret = gh_vm_start(vm, GH_RM_VM_STATUS_RUNNING);
	if (ret) {
		pr_err("Failed to start VM:%d %d\n", vm->vmid, ret);
		mutex_unlock(&vm->vm_lock);
		goto err_powerup;
	}
	pr_info("VM:%d started running\n", vm->vmid);

	mutex_unlock(&vm->vm_lock);

start_vcpu_run:
	/*
	 * proxy scheduling APIs
	 */
	if (gh_vm_supports_proxy_sched(vm->vmid)) {
		vm->proxy_vm = true;
		ret = gh_vcpu_run(vm->vmid, vcpu->vcpu_id,
						0, 0, 0, &vcpu_run);
		if (ret < 0) {
			pr_err("Failed vcpu_run %d\n", ret);
			return ret;
		}
	} else {
		/*
		 * wait_event is not allowing the process to freez,
		 * results in device suspend failure. Use wait_event_freezable
		 * so that device suspends while wait for event.
		 */
		ret = wait_event_freezable(vm->vm_status_wait,
			(vm->status.vm_status == GH_RM_VM_STATUS_EXITED));
		if (ret < 0)
			return ret;
		ret = vm->exit_type;
	}

	return ret;

err_powerup:
	gh_notify_clients(vm, GH_VM_POWERUP_FAIL);
	return ret;
}

static long gh_vcpu_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	struct gh_vcpu *vcpu = filp->private_data;
	int ret = -EINVAL;

	switch (cmd) {
	case GH_VCPU_RUN:
		ret = gh_vcpu_ioctl_run(vcpu);
		break;
	default:
		pr_err("Invalid gunyah VCPU ioctl 0x%x\n", cmd);
		break;
	}
	return ret;
}

static const struct file_operations gh_vcpu_fops = {
	.unlocked_ioctl = gh_vcpu_ioctl,
	.release = gh_vcpu_release,
	.llseek = noop_llseek,
};

static int gh_vm_ioctl_get_vcpu_count(struct gh_vm *vm)
{
	if (!vm->is_secure_vm)
		return -EINVAL;

	if (vm->status.vm_status != GH_RM_VM_STATUS_READY)
		return -EAGAIN;

	return vm->allowed_vcpus;
}

static long gh_vm_ioctl_create_vcpu(struct gh_vm *vm, u32 id)
{
	struct gh_vcpu *vcpu;
	struct file *file;
	char name[MAX_VCPU_NAME];
	int fd, err = 0;

	if (id >= GH_MAX_VCPUS)
		return -EINVAL;

	mutex_lock(&vm->vm_lock);
	if (vm->vcpus[id]) {
		err = -EEXIST;
		mutex_unlock(&vm->vm_lock);
		return err;
	}

	vcpu = kzalloc(sizeof(*vcpu), GFP_KERNEL);
	if (!vcpu) {
		err = -ENOMEM;
		mutex_unlock(&vm->vm_lock);
		return err;
	}

	vcpu->vcpu_id = id;
	vcpu->vm = vm;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		err = fd;
		goto err_destroy_vcpu;
	}

	snprintf(name, sizeof(name), "gh-vcpu:%d", id);
	file = anon_inode_getfile(name, &gh_vcpu_fops, vcpu, O_RDWR);
	if (IS_ERR(file)) {
		err = PTR_ERR(file);
		goto err_put_fd;
	}

	fd_install(fd, file);
	gh_get_vm(vm);

	vm->vcpus[id] = vcpu;
	vm->created_vcpus++;
	mutex_unlock(&vm->vm_lock);
	return fd;

err_put_fd:
	put_unused_fd(fd);
err_destroy_vcpu:
	kfree(vcpu);
	mutex_unlock(&vm->vm_lock);
	return err;
}

int gh_reclaim_mem(struct gh_vm *vm, phys_addr_t phys,
					ssize_t size, bool is_system_vm)
{
	int vmid = vm->vmid;
	struct qcom_scm_vmperm destVM[1] = {{VMID_HLOS,
				PERM_READ | PERM_WRITE | PERM_EXEC}};
	u64 srcVM = BIT(vmid);
	bool is_ext_region = false;
	int ret = 0;

	if (vm->ext_region)
		is_ext_region = (phys == vm->ext_region->ext_phys);

	if (!is_system_vm) {
		ret = ghd_rm_mem_reclaim(vm->mem_handle, 0);

		if (ret)
			pr_err("Failed to reclaim memory for %d, %d\n",
						vm->vmid, ret);
	}

	ret = qcom_scm_assign_mem(phys, size, &srcVM, destVM, ARRAY_SIZE(destVM));
	if (ret)
		pr_err("failed qcom_assign for %pa address of size %zx - subsys VMid %d rc:%d\n",
			&phys, size, vmid, ret);

	if (is_ext_region) {
		if (!is_system_vm) {
			ret = ghd_rm_mem_reclaim(vm->ext_region->ext_mem_handle, 0);
			if (ret)
				pr_err("Failed to reclaim memory for %d, %d\n",
							vm->vmid, ret);
		}
		ret |= qcom_scm_assign_mem(vm->ext_region->ext_phys,
				vm->ext_region->ext_size,
				&srcVM, destVM, ARRAY_SIZE(destVM));
		if (ret)
			pr_err("failed qcom_assign for %pa address of size %zx - subsys VMid %d rc:%d\n",
					&vm->ext_region->ext_phys,
					vm->ext_region->ext_size, vmid, ret);
	}

	return ret;
}

int gh_reclaim_user_mem(struct gh_vm *vm)
{
	struct gh_vm_user_mem *mapping;
	int src_vmid = vm->vmid;
	int dest_vmid = VMID_HLOS;
	int dest_perms = PERM_READ | PERM_WRITE | PERM_EXEC;
	int ret = 0;

	if (!vm->memory_mapping) {
		ret = -EINVAL;
		goto drop_mm;
	}

	mapping = vm->memory_mapping;
	ret = ghd_rm_mem_reclaim(mapping->mem_handle, 0);
	if (ret)
		pr_err("Failed to reclaim user memory for %d, %d\n",
			vm->vmid, ret);

	ret = hyp_assign_table(mapping->sgt, &src_vmid, 1,
							&dest_vmid, &dest_perms, 1);
	if (ret) {
		pr_err("failed hyp_assign_table for - subsys VMid %d rc:%d\n",
			src_vmid, ret);
		return ret;
	}

	sg_free_table(mapping->sgt);
	kfree(mapping->sgt);
	kfree(mapping->sgl_entries);
	unpin_user_pages(mapping->pages, mapping->npages);
	kfree(mapping->pages);
	account_locked_vm(vm->mm, mapping->npages, false);
	kfree(mapping);

drop_mm:
	mmdrop(vm->mm);

	return ret;
}

int gh_provide_mem(struct gh_vm *vm, phys_addr_t phys,
					ssize_t size, bool is_system_vm)
{
	gh_vmid_t vmid = vm->vmid;
	struct gh_acl_desc *acl_desc;
	struct gh_sgl_desc *sgl_desc;
	struct qcom_scm_vmperm srcVM[1] = {{VMID_HLOS,
				PERM_READ | PERM_WRITE | PERM_EXEC}};
	struct qcom_scm_vmperm destVM[1] = {{vmid,
				PERM_READ | PERM_WRITE | PERM_EXEC}};
	u64 srcvmid = BIT(srcVM[0].vmid);
	u64 dstvmid = BIT(destVM[0].vmid);
	bool is_ext_region = false;
	int ret = 0;

	if (vm->ext_region)
		is_ext_region = (phys == vm->ext_region->ext_phys);

	acl_desc = kzalloc(offsetof(struct gh_acl_desc, acl_entries[1]),
			GFP_KERNEL);
	if (!acl_desc)
		return -ENOMEM;

	acl_desc->n_acl_entries = 1;
	acl_desc->acl_entries[0].vmid = vmid;
	acl_desc->acl_entries[0].perms =
				GH_RM_ACL_X | GH_RM_ACL_R | GH_RM_ACL_W;

	sgl_desc = kzalloc(offsetof(struct gh_sgl_desc, sgl_entries[1]),
			GFP_KERNEL);
	if (!sgl_desc) {
		kfree(acl_desc);
		return -ENOMEM;
	}

	sgl_desc->n_sgl_entries = 1;
	sgl_desc->sgl_entries[0].ipa_base = phys;
	sgl_desc->sgl_entries[0].size = size;

	if (is_ext_region) {
		destVM[0].perm = PERM_READ;
		acl_desc->acl_entries[0].perms = GH_RM_ACL_R;
	}

	ret = qcom_scm_assign_mem(phys, size, &srcvmid, destVM,
					ARRAY_SIZE(destVM));
	if (ret) {
		pr_err("failed qcom_assign for %pa address of size %zx - subsys VMid %d rc:%d\n",
		       &phys, size, vmid, ret);
		goto err_hyp_assign;
	}

	/*
	 * A system VM is deemed critical for the functioning of the
	 * system. The memory donated to this VM can't be reclaimed
	 * by host OS at any point in time after donating it.
	 * Whereas any memory lent to a non system VM, can be reclaimed
	 * when VM terminates.
	 */
	if (is_system_vm) {
		ret = gh_rm_mem_donate(GH_RM_MEM_TYPE_NORMAL, 0, 0,
			acl_desc, sgl_desc, NULL, &vm->mem_handle);
	} else {
		if (is_ext_region)
			ret = ghd_rm_mem_lend(GH_RM_MEM_TYPE_NORMAL, 0,
					vm->ext_region->ext_label, acl_desc,
					sgl_desc, NULL, &vm->ext_region->ext_mem_handle);
		else
			ret = ghd_rm_mem_lend(GH_RM_MEM_TYPE_NORMAL, 0, 0, acl_desc,
					sgl_desc, NULL, &vm->mem_handle);
	}

	if (ret) {
		ret = qcom_scm_assign_mem(phys, size, &dstvmid,
				      srcVM, ARRAY_SIZE(srcVM));
		if (ret)
			pr_err("failed qcom_assign for %pa address of size %zx - subsys VMid %d rc:%d\n",
				&phys, size, srcVM[0].vmid, ret);
	}

err_hyp_assign:
	kfree(acl_desc);
	kfree(sgl_desc);
	return ret;
}

int gh_provide_user_mem(gh_vmid_t vmid, struct gh_vm_user_mem *mapping)
{
	struct gh_acl_desc *acl_desc;
	struct gh_sgl_desc *sgl_desc;
	int src_vmid = VMID_HLOS;
	int dest_vmid = vmid;
	int dest_perms = PERM_READ | PERM_WRITE | PERM_EXEC;
	int ret = 0, rc;
	int i;

	if (!mapping)
		return -EINVAL;

	mapping->sgt = kzalloc(sizeof(*mapping->sgt), GFP_KERNEL);
	if (!mapping->sgt)
		return -ENOMEM;

	ret = sg_alloc_table_from_pages(mapping->sgt, mapping->pages, mapping->npages,
					0, mapping->npages << PAGE_SHIFT, GFP_KERNEL);
	if (ret) {
		pr_err("failed sg_alloc_table_from_pages for - subsys VMid %d rc:%d\n",
			   dest_vmid, ret);
		goto free_sgt;
	}

	ret = hyp_assign_table(mapping->sgt, &src_vmid, 1, &dest_vmid, &dest_perms, 1);
	if (ret) {
		pr_err("failed hyp_assign_table for - subsys VMid %d rc:%d\n",
			   dest_vmid, ret);
		goto free_table;
	}

	acl_desc = kzalloc(offsetof(struct gh_acl_desc, acl_entries[1]),
			GFP_KERNEL);
	if (!acl_desc) {
		ret = -ENOMEM;
		goto assign_table;
	}

	acl_desc->n_acl_entries = 1;
	acl_desc->acl_entries[0].vmid = vmid;
	acl_desc->acl_entries[0].perms =
				GH_RM_ACL_X | GH_RM_ACL_R | GH_RM_ACL_W;

	sgl_desc = kzalloc(offsetof(struct gh_sgl_desc, sgl_entries[mapping->n_sgl_entries]),
							GFP_KERNEL);
	if (!sgl_desc) {
		ret = -ENOMEM;
		goto free_acl;
	}

	sgl_desc->n_sgl_entries = mapping->n_sgl_entries;
	for (i = 0; i < mapping->n_sgl_entries; i++)  {
		sgl_desc->sgl_entries[i].ipa_base = mapping->sgl_entries[i].ipa_base;
		sgl_desc->sgl_entries[i].size = mapping->sgl_entries[i].size;
	}

	ret = ghd_rm_mem_lend(GH_RM_MEM_TYPE_NORMAL, 0, 0, acl_desc,
			sgl_desc, NULL, &mapping->mem_handle);

	kfree(sgl_desc);
free_acl:
	kfree(acl_desc);
	if (!ret)
		return ret;
assign_table:
	rc = hyp_assign_table(mapping->sgt, &dest_vmid, 1, &src_vmid, &dest_perms, 1);
	if (rc)
		pr_err("failed hyp_assign_table for - subsys VMid %d rc:%d\n",
			src_vmid, rc);
free_table:
	sg_free_table(mapping->sgt);
free_sgt:
	kfree(mapping->sgt);
	return ret;
}

long gh_vm_configure(u16 auth_mech, u64 image_offset,
			u64 image_size, u64 dtb_offset, u64 dtb_size,
			u32 pas_id, struct gh_vm *vm)
{
	struct gh_vm_auth_param_entry entry;
	long ret = -EINVAL;

	switch (auth_mech) {
	case GH_VM_AUTH_PIL_ELF:
		ret = gh_rm_vm_config_image(vm->vmid, auth_mech,
				vm->mem_handle, image_offset,
				image_size, dtb_offset, dtb_size);
		if (ret) {
			pr_err("VM_CONFIG failed for VM:%d %ld\n",
						vm->vmid, ret);
			return ret;
		}
		vm->status.vm_status = GH_RM_VM_STATUS_AUTH;
		if (!pas_id) {
			pr_err("Incorrect pas_id %d for VM:%d\n", pas_id,
						vm->vmid);
			return -EINVAL;
		}
		entry.auth_param_type = GH_VM_AUTH_PARAM_PAS_ID;
		entry.auth_param = pas_id;

		ret = gh_rm_vm_auth_image(vm->vmid, 1, &entry);
		if (ret) {
			pr_err("VM_AUTH_IMAGE failed for VM:%d %ld\n",
						vm->vmid, ret);
			return ret;
		}
		vm->status.vm_status = GH_RM_VM_STATUS_INIT;
		break;
	default:
		pr_err("Invalid auth mechanism for VM\n");
	}

	return ret;
}

long gh_vm_init(const char *fw_name, struct gh_vm *vm)
{
	long ret = -EINVAL;
	int nr_vcpus = 0;

	if (!gh_firmware_is_legacy()) {
		ret = ghd_rm_vm_init(vm->vmid);
		if (ret) {
			pr_err("VM_INIT_IMAGE failed for VM:%d %ld\n",
							vm->vmid, ret);
			return ret;
		}
	}

	gh_wait_for_vm_status(vm, GH_RM_VM_STATUS_READY);

	ret = gh_rm_populate_hyp_res(vm->vmid, fw_name);
	if (ret < 0) {
		pr_err("Failed to populate resources %ld\n", ret);
		return ret;
	}

	if (vm->is_secure_vm) {
		nr_vcpus = gh_get_nr_vcpus(vm->vmid);

		if (nr_vcpus < 0) {
			pr_err("Failed to get vcpu count for vm %d ret%d\n",
				vm->vmid, nr_vcpus);
			ret = nr_vcpus;
			return ret;
		} else if (!nr_vcpus) /* Hypervisor scheduled case when at least 1 vcpu is needed */
			nr_vcpus = 1;

		vm->allowed_vcpus = nr_vcpus;
	}

	return ret;
}

int gh_vm_alloc_ext_region(struct gh_vm *vm)
{
	struct gh_ext_reg *ext_region;

	ext_region = kzalloc(sizeof(*ext_region), GFP_KERNEL);
	if (!ext_region)
		return -ENOMEM;

	vm->ext_region = ext_region;

	return 0;
}

static long gh_vm_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	struct gh_vm *vm = filp->private_data;
	long ret = -EINVAL;

	switch (cmd) {
	case GH_CREATE_VCPU:
		ret = gh_vm_ioctl_create_vcpu(vm, arg);
		break;
	case GH_VM_SET_FW_NAME:
		ret = gh_vm_ioctl_set_fw_name(vm, arg);
		break;
	case GH_VM_GET_FW_NAME:
		ret = gh_vm_ioctl_get_fw_name(vm, arg);
		break;
	case GH_VM_GET_VCPU_COUNT:
		ret = gh_vm_ioctl_get_vcpu_count(vm);
		break;
	case GH_VM_GET_FW_RESV_MEM_SIZE:
		ret = gh_vm_ioctl_get_fw_resv_mem_size(vm, arg);
		break;
	case GH_VM_SET_FW_USER_MEM_REGION:
		ret = gh_vm_ioctl_set_fw_user_mem_region(vm, arg);
		break;
	default:
		ret = gh_virtio_backend_ioctl(vm->fw_name, cmd, arg);
		break;
	}
	return ret;
}

static int gh_vm_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct gh_vm *vm = file->private_data;
	int ret = -EINVAL;

	ret = gh_virtio_backend_mmap(vm->fw_name, vma);

	return ret;
}

static int gh_vm_release(struct inode *inode, struct file *filp)
{
	struct gh_vm *vm = filp->private_data;
	int ret;

	ret = gh_put_vm(vm);

	return ret;
}

static const struct file_operations gh_vm_fops = {
	.unlocked_ioctl = gh_vm_ioctl,
	.mmap = gh_vm_mmap,
	.release = gh_vm_release,
	.llseek = noop_llseek,
};

static struct gh_vm *gh_create_vm(void)
{
	struct gh_vm *vm;
	int ret;

	vm = kzalloc(sizeof(*vm), GFP_KERNEL);
	if (!vm)
		return ERR_PTR(-ENOMEM);

	mutex_init(&vm->vm_lock);
	vm->rm_nb.priority = 1;
	vm->rm_nb.notifier_call = gh_vm_rm_notifier_fn;
	ret = gh_rm_register_notifier(&vm->rm_nb);
	if (ret) {
		mutex_destroy(&vm->vm_lock);
		kfree(vm);
		return ERR_PTR(ret);
	}

	mmgrab(current->mm);
	vm->mm = current->mm;
	refcount_set(&vm->users_count, 1);
	init_waitqueue_head(&vm->vm_status_wait);
	vm->status.vm_status = GH_RM_VM_STATUS_NO_STATE;
	vm->exit_type = -EINVAL;

	return vm;
}

static long gh_dev_ioctl_create_vm(unsigned long arg)
{
	struct gh_vm *vm;
	struct file *file;
	int fd, err;

	vm = gh_create_vm();
	if (IS_ERR_OR_NULL(vm))
		return PTR_ERR(vm);

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		err = fd;
		goto err_destroy_vm;
	}

	file = anon_inode_getfile("gunyah-vm", &gh_vm_fops, vm, O_RDWR);
	if (IS_ERR(file)) {
		err = PTR_ERR(file);
		goto err_put_fd;
	}

	fd_install(fd, file);

	return fd;

err_put_fd:
	put_unused_fd(fd);
err_destroy_vm:
	gh_put_vm(vm);
	return err;
}

static long gh_dev_ioctl(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	long ret = -EINVAL;

	switch (cmd) {
	case GH_CREATE_VM:
		ret = gh_dev_ioctl_create_vm(arg);
		break;
	default:
		pr_err("Invalid gunyah dev ioctl 0x%x\n", cmd);
		break;
	}

	return ret;
}

static const struct file_operations gh_dev_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = gh_dev_ioctl,
	.llseek = noop_llseek,
};

static struct miscdevice gh_dev = {
	.name = "qgunyah",
	.minor = MISC_DYNAMIC_MINOR,
	.fops = &gh_dev_fops,
};

void gh_uevent_notify_change(unsigned int type, struct gh_vm *vm)
{
	struct kobj_uevent_env *env;

	env = kzalloc(sizeof(*env), GFP_KERNEL_ACCOUNT);
	if (!env)
		return;

	if (type == GH_EVENT_CREATE_VM)
		add_uevent_var(env, "EVENT=create");
	else if (type == GH_EVENT_DESTROY_VM) {
		add_uevent_var(env, "EVENT=destroy");
		add_uevent_var(env, "vm_exit=%d", vm->exit_type);
	}

	add_uevent_var(env, "vm_name=%s", vm->fw_name);
	env->envp[env->envp_idx++] = NULL;
	kobject_uevent_env(&gh_dev.this_device->kobj, KOBJ_CHANGE, env->envp);
	kfree(env);
}

static int __init gh_init(void)
{
	int ret;

	ret = gh_secure_vm_loader_init();
	if (ret)
		pr_err("gunyah: secure loader init failed %d\n", ret);

	ret = gh_proxy_sched_init();
	if (ret)
		pr_err("gunyah: proxy scheduler init failed %d\n", ret);

	ret = misc_register(&gh_dev);
	if (ret) {
		pr_err("gunyah: misc device register failed %d\n", ret);
		goto err_gh_init;
	}

	ret = gh_virtio_backend_init();
	if (ret)
		pr_err("gunyah: virtio backend init failed %d\n", ret);

	return ret;

err_gh_init:
	gh_proxy_sched_exit();
	gh_secure_vm_loader_exit();
	return 0;
}
module_init(gh_init);

static void __exit gh_exit(void)
{
	misc_deregister(&gh_dev);
	gh_proxy_sched_exit();
	gh_secure_vm_loader_exit();
	gh_virtio_backend_exit();
}
module_exit(gh_exit);

MODULE_LICENSE("GPL");
