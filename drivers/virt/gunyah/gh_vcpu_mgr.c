// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt)	"gh_vcpu_mgr: " fmt

#include <linux/freezer.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>

#include <linux/gunyah.h>
#include <linux/gunyah/gh_errno.h>
#include <linux/gunyah/gh_rm_drv.h>
#include <linux/gunyah/gh_vm.h>
#include <trace/hooks/gunyah.h>
#include "gh_vcpu_mgr.h"

#define CREATE_TRACE_POINTS
#include "gh_vcpu_mgr_trace.h"

struct gh_proxy_vcpu {
	struct gh_proxy_vm *vm;
	gh_capid_t cap_id;
	gh_label_t idx;
	bool wdog_frozen;
	char ws_name[32];
	struct wakeup_source *ws;
	struct task_struct *vcpu_thread;
	struct gunyah_vcpu *gunyah_vcpu;
};

struct gh_proxy_vm {
	gh_vmid_t id;
	int vcpu_count;
	struct xarray vcpus;
	bool is_vcpu_info_populated;
	bool is_active;
	gh_capid_t wdog_cap_id;
};

static bool trustvm_keep_running = true;
static bool oemvm_keep_running = true;

static struct gh_proxy_vm *gh_vms;
static bool init_done;
static DECLARE_RWSEM(gh_vm_rwsem);

static inline bool is_vm_supports_proxy(gh_vmid_t gh_vmid)
{
	gh_vmid_t vmid;

	if ((!ghd_rm_get_vmid(GH_TRUSTED_VM, &vmid) && vmid == gh_vmid) ||
			(!ghd_rm_get_vmid(GH_OEM_VM, &vmid) && vmid == gh_vmid))
		return true;

	return false;
}

static inline bool is_keep_running_enable(gh_vmid_t gh_vmid)
{
	gh_vmid_t vmid;

	if ((!ghd_rm_get_vmid(GH_TRUSTED_VM, &vmid) && vmid == gh_vmid) &&
	    trustvm_keep_running)
		return true;
	else if ((!ghd_rm_get_vmid(GH_OEM_VM, &vmid) && vmid == gh_vmid) &&
		 oemvm_keep_running)
		return true;
	else
		return false;
}

static inline struct gh_proxy_vm *gh_get_vm(gh_vmid_t vmid)
{
	int ret;
	enum gh_vm_names vm_name;
	struct gh_proxy_vm *vm = NULL;

	ret = gh_rm_get_vm_name(vmid, &vm_name);
	if (ret) {
		pr_err("Failed to get VM name for VMID%d ret=%d\n", vmid, ret);
		return vm;
	}

	vm = &gh_vms[vm_name];

	return vm;
}

static inline void gh_reset_vm(struct gh_proxy_vm *vm)
{
	vm->id = GH_VMID_INVAL;
	vm->vcpu_count = 0;
	vm->is_vcpu_info_populated = false;
	vm->is_active = false;
}

static void gh_init_vms(void)
{
	struct gh_proxy_vm *vm;
	int i;

	for (i = 0; i < GH_VM_MAX; i++) {
		vm = &gh_vms[i];
		gh_reset_vm(vm);
		xa_init(&vm->vcpus);
	}
}

static inline void gh_get_vcpu_prop_name(int vmid, int vcpu_num, char *name)
{
	char extrastr[12];

	scnprintf(extrastr, 12, "_%d_%d", vmid, vcpu_num);
	strlcat(name, extrastr, 32);
}

static int gh_wdog_manage(gh_vmid_t vmid, gh_capid_t cap_id, bool populate)
{
	struct gh_proxy_vm *vm;
	int ret = 0;

	if (!init_done) {
		pr_err("%s: vcpu_mgr not initiatized\n", __func__);
		return -ENXIO;
	}

	if (!is_vm_supports_proxy(vmid)) {
		pr_info("Skip populating VCPU affinity info for VM=%d\n", vmid);
		return -EINVAL;
	}

	down_write(&gh_vm_rwsem);
	vm = gh_get_vm(vmid);
	if (!vm) {
		ret = -ENODEV;
		goto unlock;
	}

	if (populate)
		vm->wdog_cap_id = cap_id;
	else
		vm->wdog_cap_id = GH_CAPID_INVAL;

unlock:
	up_write(&gh_vm_rwsem);
	return ret;
}

/*
 * Called when vm_status is STATUS_READY, multiple times before status
 * moves to STATUS_RUNNING
 */
static int gh_populate_vm_vcpu_info(gh_vmid_t vmid, gh_label_t cpu_idx,
					gh_capid_t cap_id, int virq_num)
{
	struct gh_proxy_vm *vm;
	struct gh_proxy_vcpu *vcpu;
	int ret = 0;

	if (!init_done) {
		pr_err("%s: vcpu_mgr not initiatized\n", __func__);
		ret = -ENXIO;
		goto out;
	}

	if (!is_vm_supports_proxy(vmid)) {
		pr_info("Skip populating VCPU affinity info for VM=%d\n", vmid);
		goto out;
	}

	down_write(&gh_vm_rwsem);
	vm = gh_get_vm(vmid);
	if (vm && !vm->is_vcpu_info_populated) {
		vcpu = kzalloc(sizeof(*vcpu), GFP_KERNEL);
		if (!vcpu) {
			ret = -ENOMEM;
			goto unlock;
		}

		strscpy(vcpu->ws_name, "gh_vcpu_ws", sizeof(vcpu->ws_name));
		gh_get_vcpu_prop_name(vmid, vm->vcpu_count,
				vcpu->ws_name);
		vcpu->ws = wakeup_source_register(NULL,
					vcpu->ws_name);
		if (!vcpu->ws) {
			pr_err("%s: Wakeup source creation failed\n", __func__);
			kfree(vcpu);
			goto unlock;
		}

		vcpu->cap_id = cap_id;
		vcpu->idx = cpu_idx;
		vm->id = vmid;
		vcpu->vm = vm;
		ret = xa_err(xa_store(&vm->vcpus, cpu_idx, vcpu, GFP_KERNEL));
		if (ret) {
			wakeup_source_unregister(vcpu->ws);
			kfree(vcpu);
			goto unlock;
		}
		vm->vcpu_count++;
	}
unlock:
	up_write(&gh_vm_rwsem);
out:
	return ret;
}

/*
 * The proxy_vcpu structure is allocated during the populate() function.
 * It can be freed through one of two paths:
 *
 * (1) Via unpopulate():
 *     - If vcpu_thread is NULL (i.e., no kernel thread was created),
 *       unpopulate() performs immediate cleanup of proxy_vcpu.
 *
 * (2) Via kthread execution:
 *     - If vcpu_thread is not NULL (i.e., a kernel thread exists),
 *       unpopulate() does not directly free the structure.
 *       Instead, it sets the immediate_exit flag and calls complete_all()
 *       to signal the thread to exit.
 *     - The gh_vcpu_kthread() function then performs its own resource
 *       cleanup before terminating.
 */
static void gh_cleanup_proxy_vcpu(struct gh_proxy_vcpu *vcpu, struct gh_proxy_vm *vm)
{
	lockdep_assert_held_write(&gh_vm_rwsem);

	wakeup_source_unregister(vcpu->ws);
	xa_erase(&vm->vcpus, vcpu->idx);
	vm->vcpu_count--;
	kfree(vcpu);
}

static int gh_unpopulate_vm_vcpu_info(gh_vmid_t vmid, gh_label_t cpu_idx,
					gh_capid_t cap_id, int *irq)
{
	struct gh_proxy_vm *vm;
	struct gh_proxy_vcpu *vcpu;

	if (!init_done) {
		pr_err("%s: vcpu_mgr not initiatized\n", __func__);
		return -ENXIO;
	}

	if (!is_vm_supports_proxy(vmid)) {
		pr_info("Skip unpopulating VCPU affinity info for VM=%d\n", vmid);
		goto out;
	}

	down_write(&gh_vm_rwsem);
	vm = gh_get_vm(vmid);
	if (vm && vm->is_vcpu_info_populated) {
		vcpu = xa_load(&vm->vcpus, cpu_idx);
		if (!vcpu) {
			up_write(&gh_vm_rwsem);
			goto out;
		}
		if (vcpu->vcpu_thread && vcpu->gunyah_vcpu) {
			vcpu->gunyah_vcpu->vcpu_run->immediate_exit = true;
			complete_all(&vcpu->gunyah_vcpu->ready);
		} else {
			gh_cleanup_proxy_vcpu(vcpu, vm);
		}
	}
	up_write(&gh_vm_rwsem);

out:
	return 0;
}

static void gh_populate_all_res_info(gh_vmid_t vmid, bool res_populated)
{
	struct gh_proxy_vm *vm;

	if (!init_done) {
		pr_err("%s: vcpu_mgr not initiatized\n", __func__);
		return;
	}

	if (!is_vm_supports_proxy(vmid)) {
		pr_debug("VM %d is not managed by Gunyah vCPU manager vendor module\n", vmid);
		return;
	}

	down_write(&gh_vm_rwsem);
	vm = gh_get_vm(vmid);
	if (!vm)
		goto unlock;

	if (res_populated && !vm->is_vcpu_info_populated) {
		vm->is_vcpu_info_populated = true;
		vm->is_active = true;
	} else if (!res_populated && vm->is_vcpu_info_populated) {
		gh_reset_vm(vm);
	}
unlock:
	up_write(&gh_vm_rwsem);
}

static int gh_get_nr_vcpus(gh_vmid_t vmid)
{
	struct gh_proxy_vm *vm;

	vm = gh_get_vm(vmid);
	if (vm && vm->is_vcpu_info_populated)
		return vm->vcpu_count;

	return 0;
}

int gh_poll_vcpu_run(gh_vmid_t vmid)
{
	struct gh_hcall_vcpu_run_resp resp;
	struct gh_proxy_vcpu *vcpu;
	struct gh_proxy_vm *vm;
	unsigned int vcpu_id;
	int poll_nr_vcpus;
	ktime_t start_ts, yield_ts;
	int ret = -EPERM;

	vm = gh_get_vm(vmid);
	if (!vm || !vm->is_active)
		return ret;

	poll_nr_vcpus = gh_get_nr_vcpus(vmid);
	if (poll_nr_vcpus < 0) {
		printk_deferred("Failed to get vcpu count for VM %d ret %d\n",
						vmid, poll_nr_vcpus);
		ret = poll_nr_vcpus;
		return ret;
	}

	for (vcpu_id = 0; vcpu_id < poll_nr_vcpus; vcpu_id++) {
		vcpu = xa_load(&vm->vcpus, vcpu_id);
		if (!vcpu)
			return -EPERM;

		if (vcpu->cap_id == GH_CAPID_INVAL)
			return -EPERM;

		do {
			start_ts = ktime_get();
			ret = gh_hcall_vcpu_run(vcpu->cap_id, 0, 0, 0, &resp);
			yield_ts = ktime_get() - start_ts;
			trace_gh_hcall_vcpu_run(ret, vcpu->vm->id, vcpu_id, yield_ts,
						resp.vcpu_state, resp.vcpu_suspend_state);
			if (ret == GH_ERROR_OK) {
				if (resp.vcpu_state != GUNYAH_VCPU_STATE_READY &&
					resp.vcpu_state != GUNYAH_VCPU_STATE_EXPECTS_WAKEUP)
					pr_warn_ratelimited("VCPU STATE: state=%llu VCPU=%u of VM=%d\n",
							resp.vcpu_state, vcpu_id, vmid);
				break;
			}
		} while (ret == GH_ERROR_RETRY);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(gh_poll_vcpu_run);

static void android_rvh_gh_before_vcpu_run(void *unused, u16 vmid, u32 vcpu_id)
{
	struct gh_proxy_vcpu *vcpu;
	struct gh_proxy_vm *vm;

	if (vmid > QCOM_SCM_MAX_MANAGED_VMID)
		return;

	/*
	 * No need to release rwsem in this function, as
	 * android_rvh_gh_after_vcpu_run is guaranteed to be called and will
	 * handle the release there.
	 */
	down_read(&gh_vm_rwsem);
	preempt_disable();
	vm = gh_get_vm(vmid);
	if (!vm || !vm->is_active)
		return;

	vcpu = xa_load(&vm->vcpus, vcpu_id);
	if (!vcpu)
		return;

	/* Call into Gunyah to run vcpu. */
	__pm_stay_awake(vcpu->ws);
	if (vcpu->wdog_frozen) {
		gh_hcall_wdog_manage(vm->wdog_cap_id, WATCHDOG_MANAGE_OP_UNFREEZE);
		vcpu->wdog_frozen = false;
	}
}

/*
 * Freeze the VM watchdog when
 * 1. Any vCPU thread is re-scheduled while runnable.
 * 2. Any vCPU thread is blocked on VMMIO.
 * case (2) may involve returning to user space and has unbound latency, so not
 * freezing watchdog can result in false alram.
 */
static inline bool gh_vcpu_should_freeze_wdog(const struct gunyah_hypercall_vcpu_run_resp *resp)
{
	return (resp->state == GUNYAH_VCPU_STATE_READY && need_resched()) ||
		resp->state == GUNYAH_VCPU_ADDRSPACE_VMMIO_READ ||
		resp->state == GUNYAH_VCPU_ADDRSPACE_VMMIO_WRITE;
}

static void android_rvh_gh_after_vcpu_run(void *unused, u16 vmid, u32 vcpu_id, int hcall_ret,
			const struct gunyah_hypercall_vcpu_run_resp *resp)
{
	struct gh_proxy_vcpu *vcpu;
	struct gh_proxy_vm *vm;

	if (vmid > QCOM_SCM_MAX_MANAGED_VMID)
		return;

	/*
	 * VM state may change between vendor hooks.
	 * Ensure preempt operations remain symmetric
	 * even on early exit paths.
	 */
	vm = gh_get_vm(vmid);
	if (!vm || !vm->is_active)
		goto re_enable_preempt;

	vcpu = xa_load(&vm->vcpus, vcpu_id);
	if (!vcpu)
		goto re_enable_preempt;

	if (hcall_ret == GH_ERROR_OK && gh_vcpu_should_freeze_wdog(resp)) {
		gh_hcall_wdog_manage(vm->wdog_cap_id,
				     WATCHDOG_MANAGE_OP_FREEZE);
		vcpu->wdog_frozen = true;
	}
	preempt_enable();

	if (hcall_ret == GH_ERROR_OK) {
		switch (resp->state) {
		case GUNYAH_VCPU_STATE_EXPECTS_WAKEUP:
			if (resp->state_data[1] == GH_VCPU_SYSTEM_SUSPEND || (vmid == 49 && resp->state_data[1] == GH_VCPU_WFI))
				__pm_relax(vcpu->ws);
			break;
		case GUNYAH_VCPU_STATE_POWERED_OFF:
			fallthrough;
		case GUNYAH_VCPU_STATE_SYSTEM_OFF:
			__pm_relax(vcpu->ws);
			break;
		default:
			break;
		}
	}

	if (signal_pending(current)) {
		if (!vcpu->wdog_frozen) {
			gh_hcall_wdog_manage(vm->wdog_cap_id,
					WATCHDOG_MANAGE_OP_FREEZE);
			vcpu->wdog_frozen = true;
		}
	}

	up_read(&gh_vm_rwsem);
	return;

re_enable_preempt:
	preempt_enable();
	up_read(&gh_vm_rwsem);
}

static int gh_vcpu_mgr_reg_rm_cbs(void)
{
	int ret = -EINVAL;

	ret = gh_rm_set_wdog_manage_cb(&gh_wdog_manage);
	if (ret) {
		pr_err("fail to set the WDOG resource callback\n");
		return ret;
	}

	ret = gh_rm_set_vcpu_affinity_cb(&gh_populate_vm_vcpu_info);
	if (ret) {
		pr_err("fail to set the VM VCPU populate callback\n");
		return ret;
	}

	ret = gh_rm_reset_vcpu_affinity_cb(&gh_unpopulate_vm_vcpu_info);
	if (ret) {
		pr_err("fail to set the VM VCPU unpopulate callback\n");
		return ret;
	}

	ret = gh_rm_all_res_populated_cb(&gh_populate_all_res_info);
	if (ret) {
		pr_err("fail to set the all res populate callback\n");
		return ret;
	}

	return 0;
}

static int __maybe_unused gh_vcpu_kthread(void *data)
{
	struct gh_proxy_vcpu *proxy_vcpu = (struct gh_proxy_vcpu *)data;
	struct gh_proxy_vm *vm = proxy_vcpu->vm;
	struct gunyah_vcpu *vcpu = proxy_vcpu->gunyah_vcpu;
	unsigned long resume_data[3] = { 0 };
	struct gunyah_hypercall_vcpu_run_resp vcpu_run_resp;
	enum gunyah_error gunyah_error = GUNYAH_ERROR_OK;
	int ret = 0;

	set_freezable();

	while (!kthread_should_stop() && !ret) {
		mutex_lock(&vcpu->run_lock);
		if (vcpu->vcpu_run->immediate_exit) {
			ret = -EINTR;
			mutex_unlock(&vcpu->run_lock);
			break;
		}
		android_rvh_gh_before_vcpu_run(NULL, proxy_vcpu->vm->id,
					       vcpu->ticket.label);
		gunyah_error = gunyah_hypercall_vcpu_run(
			vcpu->rsc->capid, resume_data, &vcpu_run_resp);
		android_rvh_gh_after_vcpu_run(
			NULL, proxy_vcpu->vm->id, vcpu->ticket.label,
			gunyah_error,
			(const struct gunyah_hypercall_vcpu_run_resp
				 *)&vcpu_run_resp);
		if (gunyah_error == GUNYAH_ERROR_OK) {
			memset(resume_data, 0, sizeof(resume_data));
			switch (vcpu_run_resp.state) {
			case GUNYAH_VCPU_STATE_READY:
				if (need_resched())
					schedule();
				break;
			case GUNYAH_VCPU_STATE_POWERED_OFF:
				fallthrough;
			case GUNYAH_VCPU_STATE_SYSTEM_OFF:
				if (vcpu->vcpu_run->immediate_exit) {
					ret = -EINTR;
					break;
				}
				fallthrough;
			case GUNYAH_VCPU_STATE_EXPECTS_WAKEUP:
				ret = wait_for_completion_interruptible(
					&vcpu->ready);
				reinit_completion(&vcpu->ready);
				break;
			case GUNYAH_VCPU_STATE_BLOCKED:
				schedule();
				break;
			case GUNYAH_VCPU_ADDRSPACE_VMMIO_READ:
			case GUNYAH_VCPU_ADDRSPACE_VMMIO_WRITE:
			case GUNYAH_VCPU_ADDRSPACE_PAGE_FAULT:
				/* These VCPU state cannot be handled without
				 * origin vcpu thread, just skip it and run
				 * vcpu again
				 */
				break;
			default:
				pr_warn_ratelimited(
					"Unknown vCPU state: %llx\n",
					vcpu_run_resp.sized_state);
				schedule();
				break;
			}
		} else if (gunyah_error == GUNYAH_ERROR_RETRY) {
			schedule();
		} else {
			ret = gunyah_error_remap(gunyah_error);
		}
		mutex_unlock(&vcpu->run_lock);

		try_to_freeze();
	}

	/* The cleanup work in vcpu unpopulate is only used to wakeup kthread.
	 * Once kthread is already in the exit flow, cleanup can be skipped.
	 */
	down_write(&gh_vm_rwsem);
	gh_cleanup_proxy_vcpu(proxy_vcpu, vm);
	up_write(&gh_vm_rwsem);

	gunyah_vm_put(vcpu->ghvm);

	return ret;
}

static void android_rvh_gh_before_vcpu_release(void *unused, u16 vmid,
					       struct gunyah_vcpu *vcpu)
{
	int vcpu_id = vcpu->ticket.label;
	struct gh_proxy_vcpu *proxy_vcpu;
	struct gh_proxy_vm *vm;

	if (!is_vm_supports_proxy(vmid)) {
		pr_debug("VM %d is not managed by Gunyah vCPU manager vendor module\n", vmid);
		return;
	}

	if (!is_keep_running_enable(vmid))
		return;

	down_read(&gh_vm_rwsem);
	vm = gh_get_vm(vmid);
	if (!vm || !vm->is_active)
		goto unlock;

	proxy_vcpu = xa_load(&vm->vcpus, vcpu_id);
	if (!proxy_vcpu)
		goto unlock;
	/* Do not need to create kthread if VM is shutdown */
	if (vcpu->vcpu_run->immediate_exit ||
	    vcpu->state == GUNYAH_VCPU_RUN_STATE_SYSTEM_DOWN)
		goto unlock;
	/* VM instance already get a vcpu kref, only need to get VM kref here */
	if (!gunyah_vm_get(vcpu->ghvm))
		goto unlock;

	proxy_vcpu->gunyah_vcpu = vcpu;

	proxy_vcpu->vcpu_thread = kthread_run(gh_vcpu_kthread, proxy_vcpu,
					      "vm%d_vcpu%d_kthread", vmid,
					      vcpu_id);
	if (IS_ERR(proxy_vcpu->vcpu_thread)) {
		proxy_vcpu->vcpu_thread = NULL;
		proxy_vcpu->gunyah_vcpu = NULL;
		pr_err("Failed to create vcpu kthread for VM=%d vcpu=%d\n",
		       vmid, vcpu_id);
		gunyah_vm_put(vcpu->ghvm);
		goto unlock;
	}

unlock:
	up_read(&gh_vm_rwsem);
	return;
}

static void android_rvh_gh_before_vm_release(void *unused, u16 vmid,
					     struct gunyah_vm *ghvm)
{
	struct gh_proxy_vm *vm;

	if (!is_vm_supports_proxy(vmid)) {
		pr_debug("VM %d is not managed by Gunyah vCPU manager vendor module\n", vmid);
		return;
	}

	if (!is_keep_running_enable(vmid))
		return;

	down_read(&gh_vm_rwsem);
	vm = gh_get_vm(vmid);
	if (!vm || !vm->is_active)
		goto unlock;

	up_read(&gh_vm_rwsem);
	ghd_rm_vm_stop(vmid, GH_VM_STOP_SHUTDOWN, 0);
	return;

unlock:
	up_read(&gh_vm_rwsem);
	return;
}

static void gh_register_hooks(void)
{
	register_trace_android_rvh_gh_before_vcpu_run(android_rvh_gh_before_vcpu_run, NULL);
	register_trace_android_rvh_gh_after_vcpu_run(android_rvh_gh_after_vcpu_run, NULL);
	register_trace_android_rvh_gh_vcpu_release(android_rvh_gh_before_vcpu_release, NULL);
	register_trace_android_rvh_gh_vm_release(android_rvh_gh_before_vm_release, NULL);
}

int gh_vcpu_mgr_init(void)
{
	int ret;

	gh_vms = kcalloc(GH_VM_MAX, sizeof(struct gh_proxy_vm), GFP_KERNEL);
	if (!gh_vms) {
		ret = -ENOMEM;
		goto err;
	}

	ret = gh_vcpu_mgr_reg_rm_cbs();
	if (ret)
		goto free_gh_vms;

	gh_init_vms();
	gh_register_hooks();
	init_done = true;
	return 0;

free_gh_vms:
	kfree(gh_vms);
err:
	return ret;
}
module_init(gh_vcpu_mgr_init);

void gh_vcpu_mgr_exit(void)
{
	struct gh_proxy_vm *vm;
	int i;

	for (i = 0; i < GH_VM_MAX; i++) {
		vm = &gh_vms[i];
		xa_destroy(&vm->vcpus);
	}

	kfree(gh_vms);
	init_done = false;
}
module_exit(gh_vcpu_mgr_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Gunyah VCPU management Driver");
