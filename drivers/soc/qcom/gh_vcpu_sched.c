// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright 2018 The Hafnium Authors.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * This driver is based on idea from Hafnium Hypervisor Linux Driver,
 * but modified to work with Gunyah Hypervisor as needed.
 *
 * Copyright (c) 2021-2022, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#define pr_fmt(fmt)	"gh_vcpu_sched: " fmt

#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/sched/rt.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <uapi/linux/sched/types.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>

#include <linux/gunyah/gh_errno.h>
#include <linux/gunyah/gh_rm_drv.h>
#include "gh_vcpu_sched.h"

#define CREATE_TRACE_POINTS
#include "gh_vcpu_sched_trace.h"

#define GH_MAX_VMS 2
#define GH_MAX_VCPUS_PER_VM 2
#define GH_MAX_VCPUS (GH_MAX_VMS * GH_MAX_VCPUS_PER_VM)

#define GH_VCPU_STATE_RUNNING		0
#define GH_VCPU_STATE_EXPECTS_WAKEUP	1
#define GH_VCPU_STATE_POWERED_DOWN	2

#define GH_VCPU_SUSPEND_STATE_STANDBY	0
#define GH_VCPU_SUSPEND_STATE_POWERDOWN	1

#define SVM_STATE_RUNNING		1
#define SVM_STATE_SYSTEM_SUSPENDED	3

struct gh_vcpu {
	struct gh_vm *vm;
	gh_capid_t cap_id;
	gh_label_t idx;
	atomic_t abort_sleep;
	struct task_struct *task;
	int virq;
};

struct gh_vm {
	gh_vmid_t id;
	int vcpu_count;
	struct gh_vcpu vcpu[GH_MAX_VCPUS_PER_VM];
	bool is_vcpu_info_populated;

	gh_capid_t vpmg_cap_id;
	int susp_res_irq;
	bool is_vpm_group_info_populated;
};

static struct gh_vm *gh_vms;
static int nr_vms;
static int nr_vcpus;
static bool init_done;
static DEFINE_MUTEX(gh_vm_mutex);
static DEFINE_SPINLOCK(gh_vm_lock);

/*
 * Wakes up the kernel thread responsible for running the given vcpu.
 *
 * Returns 0 if the thread was already running, 1 otherwise.
 */
static int gh_vcpu_wake_up(struct gh_vcpu *vcpu)
{
	/* Set a flag indicating that the thread should not go to sleep. */
	atomic_set(&vcpu->abort_sleep, 1);

	/* Set the thread to running state. */
	return wake_up_process(vcpu->task);
}

/*
 * Puts the current thread to sleep. The current thread must be responsible for
 * running the given vcpu.
 *
 * Going to sleep will fail if gh_vcpu_wake_up() or kthread_stop() was called on
 * this vcpu/thread since the last time it [re]started running.
 */
static void gh_vcpu_sleep(struct gh_vcpu *vcpu)
{
	int abort;

	set_current_state(TASK_INTERRUPTIBLE);

	/* Check the sleep-abort flag after making thread interruptible. */
	abort = atomic_read(&vcpu->abort_sleep);
	if (!abort && !kthread_should_stop())
		schedule();

	/* Set state back to running on the way out. */
	set_current_state(TASK_RUNNING);
}

/*
 * This is the main loop of each vcpu.
 */
static int gh_vcpu_thread(void *data)
{
	struct gh_vcpu *vcpu = data;
	int ret;
	ktime_t start_ts, yield_ts;
	struct gh_hcall_vcpu_run_resp resp = {};

	while (!kthread_should_stop()) {
		/*
		 * We're about to run the vcpu, so we can reset the abort-sleep flag.
		 */
		atomic_set(&vcpu->abort_sleep, 0);

		start_ts = ktime_get();
		/* Call into Gunyah to run vcpu. */
		ret = gh_hcall_vcpu_run(vcpu->cap_id, 0, 0, 0, &resp);
		yield_ts = ktime_get() - start_ts;
		trace_gh_hcall_vcpu_run(ret, vcpu->vm->id, vcpu->idx, yield_ts,
					resp.vcpu_state, resp.vcpu_suspend_state);

		if (ret != GH_ERROR_OK) {
			if (!kthread_should_stop())
				schedule();
		} else {
			switch (resp.vcpu_state) {
			/* VCPU is preempted by PVM interrupt. */
			case GH_VCPU_STATE_RUNNING:
				if (need_resched())
					schedule();
				break;

			/* VCPU in WFI. */
			case GH_VCPU_STATE_EXPECTS_WAKEUP:
			case GH_VCPU_STATE_POWERED_DOWN:
				gh_vcpu_sleep(vcpu);
				break;

			/* Unknown VCPU state. */
			default:
				pr_err("Unknown VCPU STATE: state=%d VCPU=%d of VM=%d\n",
					resp.vcpu_state, vcpu->idx, vcpu->vm->id);
				if (!kthread_should_stop())
					schedule();
				break;
			}
		}
	}

	return 0;
}

static void gh_free_vm_resources(struct gh_vm *vm)
{
	gh_label_t j;

	for (j = 0; j < vm->vcpu_count; j++) {
		put_task_struct(vm->vcpu[j].task);
		kthread_stop(vm->vcpu[j].task);
	}
}

static int gh_start_vm_vcpu_threads(struct gh_vm *vm)
{
	int ret = 0, i;

	for (i = 0; i < vm->vcpu_count; i++) {
		struct gh_vcpu *vcpu = &vm->vcpu[i];

		atomic_set(&vcpu->abort_sleep, 0);
		vcpu->task = kthread_run(gh_vcpu_thread, vcpu,
					"vcpu_thread_%u_%u", vm->id, i);
		if (IS_ERR(vcpu->task)) {
			pr_err("Error creating task (vm=%u,vcpu=%u): %ld\n",
					vm->id, i, PTR_ERR(vcpu->task));
			ret = PTR_ERR(vcpu->task);
			goto out;
		}

		get_task_struct(vcpu->task);
	}

out:
	return ret;
}

static inline bool is_tui_vm(gh_vmid_t vmid)
{
	gh_vmid_t tui_vmid;

	if (!gh_rm_get_vmid(GH_TRUSTED_VM, &tui_vmid) && tui_vmid == vmid)
		return true;

	return false;
}

static inline struct gh_vm *gh_get_vm(gh_vmid_t vmid)
{
	int i;
	struct gh_vm *vm = NULL;

	for (i = 0; i < GH_MAX_VMS; i++) {
		vm = &gh_vms[i];
		if (vmid == vm->id || vm->id == GH_VMID_INVAL)
			break;
	}

	return vm;
}

static inline struct gh_vcpu *gh_get_vcpu(struct gh_vm *vm, gh_capid_t cap_id)
{
	int i;
	struct gh_vcpu *vcpu = NULL;

	for (i = 0; i < vm->vcpu_count; i++) {
		if (vm->vcpu[i].cap_id == cap_id) {
			vcpu = &vm->vcpu[i];
			break;
		}
	}

	return vcpu;
}

static inline void gh_reset_vm(struct gh_vm *vm)
{
	int j;

	vm->id = GH_VMID_INVAL;
	vm->vcpu_count = 0;
	vm->is_vcpu_info_populated = false;
	vm->susp_res_irq = U32_MAX;
	vm->is_vpm_group_info_populated = false;
	vm->vpmg_cap_id = GH_CAPID_INVAL;
	for (j = 0; j < GH_MAX_VCPUS_PER_VM; j++) {
		vm->vcpu[j].cap_id = GH_CAPID_INVAL;
		vm->vcpu[j].virq = U32_MAX;
		vm->vcpu[j].idx = U32_MAX;
		vm->vcpu[j].vm = NULL;
	}
}

static void gh_init_vms(void)
{
	struct gh_vm *vm;
	int i;

	for (i = 0; i < GH_MAX_VMS; i++) {
		vm = &gh_vms[i];
		gh_reset_vm(vm);
	}
}

static irqreturn_t gh_vcpu_irq_handler(int irq, void *data)
{
	struct gh_vcpu *vcpu;

	spin_lock(&gh_vm_lock);
	vcpu = data;
	if (!vcpu || !vcpu->vm || !vcpu->vm->is_vcpu_info_populated)
		goto unlock;

	gh_vcpu_wake_up(vcpu);

unlock:
	spin_unlock(&gh_vm_lock);
	return IRQ_HANDLED;
}

static inline void gh_get_irq_name(int vmid, int vcpu_num, char *irq_name)
{
	char extrastr[12];

	scnprintf(extrastr, 12, "_%d_%d", vmid, vcpu_num);
	strlcat(irq_name, extrastr, 32);
}

/*
 * Called when vm_status is STATUS_READY, multiple times before status
 * moves to STATUS_RUNNING
 */
static int gh_populate_vm_vcpu_info(gh_vmid_t vmid, gh_label_t cpu_idx,
					gh_capid_t cap_id, int virq_num)
{
	struct gh_vm *vm;
	int ret = 0;
	char irq_name[32] = "gh_vcpu_irq";

	if (!init_done) {
		pr_err("Driver probe failed\n");
		ret = -ENXIO;
		goto out;
	}

	if (!is_tui_vm(vmid)) {
		pr_info("Skip populating VCPU affinity info for VM=%d\n", vmid);
		goto out;
	}

	if (nr_vcpus >= GH_MAX_VCPUS) {
		pr_err("Exceeded max vcpus in the system %d\n", nr_vcpus);
		ret = -ENXIO;
		goto out;
	}

	if (!virq_num || virq_num == U32_MAX) {
		pr_err("Invalid VIRQ, proxy scheduling isn't supported\n");
		goto out;
	}

	mutex_lock(&gh_vm_mutex);
	vm = gh_get_vm(vmid);
	if (!vm->is_vcpu_info_populated) {
		if (vm->vcpu_count >= GH_MAX_VCPUS_PER_VM) {
			pr_err("Exceeded max vcpus per VM %d\n", vm->vcpu_count);
			ret = -ENXIO;
			goto unlock;
		}

		gh_get_irq_name(vmid, vm->vcpu_count, irq_name);
		ret = request_irq(virq_num, gh_vcpu_irq_handler, 0, irq_name,
							&vm->vcpu[vm->vcpu_count]);
		if (ret < 0) {
			pr_err("%s: IRQ registration failed ret=%d\n", __func__, ret);
			goto unlock;
		}

		vm->id = vmid;
		vm->vcpu[vm->vcpu_count].cap_id = cap_id;
		vm->vcpu[vm->vcpu_count].virq = virq_num;
		vm->vcpu[vm->vcpu_count].idx = cpu_idx;
		vm->vcpu[vm->vcpu_count].vm = vm;
		vm->vcpu_count++;

		nr_vcpus++;
		pr_info("vmid=%d cpu_index:%u vcpu_cap_id:%llu virq_num=%d irq_name=%s nr_vcpus:%d\n",
				vmid, cpu_idx, cap_id, virq_num, irq_name, nr_vcpus);
	}

unlock:
	mutex_unlock(&gh_vm_mutex);
out:
	return ret;
}

static int gh_unpopulate_vm_vcpu_info(gh_vmid_t vmid, gh_label_t cpu_idx,
					gh_capid_t cap_id, int *irq)
{
	struct gh_vm *vm;
	struct gh_vcpu *vcpu;

	if (!init_done) {
		pr_err("Driver probe failed\n");
		return -ENXIO;
	}

	if (!is_tui_vm(vmid)) {
		pr_info("Skip unpopulating VCPU affinity info for VM=%d\n", vmid);
		goto out;
	}

	mutex_lock(&gh_vm_mutex);
	vm = gh_get_vm(vmid);
	if (vm && vm->is_vcpu_info_populated) {
		vcpu = gh_get_vcpu(vm, cap_id);
		if (vcpu) {
			*irq = vcpu->virq;
			free_irq(vcpu->virq, NULL);
			vcpu->virq = U32_MAX;

			if (nr_vcpus)
				nr_vcpus--;
		}
	}
	mutex_unlock(&gh_vm_mutex);

out:
	return 0;
}

static int gh_vm_vcpu_done_populate_info(struct notifier_block *nb,
						unsigned long cmd, void *data)
{
	struct gh_rm_notif_vm_status_payload *vm_status_payload = data;
	u8 vm_status = vm_status_payload->vm_status;
	gh_vmid_t vmid = vm_status_payload->vmid;
	struct gh_vm *vm;
	int ret;

	if (!is_tui_vm(vmid)) {
		pr_info("Proxy Scheduling isn't supported for VM=%d\n", vmid);
		goto out;
	}

	if (nr_vms >= GH_MAX_VMS) {
		pr_err("Exceeded max VMs in the system %d\n", nr_vms);
		return -ENXIO;
	}

	mutex_lock(&gh_vm_mutex);
	vm = gh_get_vm(vmid);

	if (cmd == GH_RM_NOTIF_VM_STATUS &&
			vm_status == GH_RM_VM_STATUS_RUNNING &&
			vm && vm->vcpu_count && !vm->is_vcpu_info_populated) {
		ret = gh_start_vm_vcpu_threads(vm);
		if (ret) {
			gh_free_vm_resources(vm);
			gh_reset_vm(vm);
			goto unlock;
		}

		nr_vms++;
		vm->is_vcpu_info_populated = true;
	} else if (cmd == GH_RM_NOTIF_VM_STATUS &&
			vm_status == GH_RM_VM_STATUS_RESET &&
			vm && vm->is_vcpu_info_populated) {
		gh_free_vm_resources(vm);
		gh_reset_vm(vm);
		if (nr_vms)
			nr_vms--;
	}

unlock:
	mutex_unlock(&gh_vm_mutex);
out:
	return NOTIFY_DONE;
}

static struct notifier_block gh_vm_vcpu_nb = {
	.notifier_call = gh_vm_vcpu_done_populate_info,
};

static inline void gh_get_vpmg_cap_id(int irq, gh_capid_t *vpmg_cap_id)
{
	int i;
	struct gh_vm *vm;

	for (i = 0; i < GH_MAX_VMS; i++) {
		vm = &gh_vms[i];
		if (vm->susp_res_irq == irq)
			*vpmg_cap_id = vm->vpmg_cap_id;
	}
}

static irqreturn_t gh_susp_res_irq_handler(int irq, void *data)
{
	int err;
	uint64_t vpmg_state;
	gh_capid_t vpmg_cap_id;

	gh_get_vpmg_cap_id(irq, &vpmg_cap_id);
	err = gh_hcall_vpm_group_get_state(vpmg_cap_id, &vpmg_state);

	if (err != GH_ERROR_OK) {
		pr_err("Failed to get VPM Group state for cap_id=%llu err=%d\n",
			vpmg_cap_id, err);
		return IRQ_HANDLED;
	}

	if (vpmg_state == SVM_STATE_RUNNING)
		pr_debug("SVM is in running state\n");
	else if (vpmg_state == SVM_STATE_SYSTEM_SUSPENDED)
		pr_debug("SVM is in system suspend state\n");
	else
		pr_err("VPM Group state invalid/non-existent\n");

	trace_gh_susp_res_irq_handler(vpmg_state);

	return IRQ_HANDLED;
}

static int gh_populate_vm_vpm_grp_info(gh_vmid_t vmid, gh_capid_t cap_id, int virq_num)
{
	int ret = 0;
	struct gh_vm *vm;

	if (!init_done) {
		pr_err("%s: Driver probe failed\n", __func__);
		ret = -ENXIO;
		goto out;
	}

	if (!is_tui_vm(vmid)) {
		pr_info("Skip populating VPM GRP info for VM=%d\n", vmid);
		goto out;
	}

	if (virq_num < 0) {
		pr_err("%s: Invalid IRQ number\n", __func__);
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&gh_vm_mutex);
	vm = gh_get_vm(vmid);
	if (!vm->is_vpm_group_info_populated) {
		ret = request_irq(virq_num, gh_susp_res_irq_handler, 0,
			"gh_susp_res_irq", NULL);
		if (ret < 0) {
			pr_err("%s: IRQ registration failed ret=%d\n", __func__, ret);
			goto unlock;
		}

		vm->vpmg_cap_id = cap_id;
		vm->susp_res_irq = virq_num;
		vm->is_vpm_group_info_populated = true;
	}

unlock:
	mutex_unlock(&gh_vm_mutex);
out:
	return ret;
}

static int gh_unpopulate_vm_vpm_grp_info(gh_vmid_t vmid, int *irq)
{
	struct gh_vm *vm;

	if (!init_done) {
		pr_err("%s: Driver probe failed\n", __func__);
		return -ENXIO;
	}

	if (!is_tui_vm(vmid)) {
		pr_info("Skip unpopulating VPM GRP info for VM=%d\n", vmid);
		goto out;
	}

	mutex_lock(&gh_vm_mutex);
	vm = gh_get_vm(vmid);
	if (vm && vm->is_vpm_group_info_populated) {
		*irq = vm->susp_res_irq;
		free_irq(vm->susp_res_irq, NULL);
		vm->susp_res_irq = U32_MAX;
		vm->is_vpm_group_info_populated = false;
	}
	mutex_unlock(&gh_vm_mutex);

out:
	return 0;
}

static int gh_vcpu_sched_reg_rm_cbs(void)
{
	int ret = -EINVAL;

	ret = gh_rm_set_vcpu_affinity_cb(GH_TRUSTED_VM, &gh_populate_vm_vcpu_info);
	if (ret) {
		pr_err("fail to set the VM VCPU populate callback\n");
		return ret;
	}

	ret = gh_rm_reset_vcpu_affinity_cb(GH_TRUSTED_VM, &gh_unpopulate_vm_vcpu_info);
	if (ret) {
		pr_err("fail to set the VM VCPU unpopulate callback\n");
		return ret;
	}

	ret = gh_rm_set_vpm_grp_cb(GH_TRUSTED_VM, &gh_populate_vm_vpm_grp_info);
	if (ret) {
		pr_err("fail to set the VM VPM GRP populate callback\n");
		return ret;
	}

	ret = gh_rm_reset_vpm_grp_cb(GH_TRUSTED_VM, &gh_unpopulate_vm_vpm_grp_info);
	if (ret) {
		pr_err("fail to set the VM VPM GRP unpopulate callback\n");
		return ret;
	}

	return 0;
}

static int gh_vcpu_sched_probe(struct platform_device *pdev)
{
	int ret;

	ret = gh_rm_register_notifier(&gh_vm_vcpu_nb);
	if (ret)
		return ret;

	gh_vms = kcalloc(GH_MAX_VMS, sizeof(struct gh_vm), GFP_KERNEL);
	if (!gh_vms) {
		ret = -ENOMEM;
		goto unregister_rm_notifier;
	}

	ret = gh_vcpu_sched_reg_rm_cbs();
	if (ret)
		goto free_gh_vms;

	gh_init_vms();

	init_done = true;
	return 0;

free_gh_vms:
	kfree(gh_vms);
unregister_rm_notifier:
	gh_rm_unregister_notifier(&gh_vm_vcpu_nb);

	return ret;
}

static const struct of_device_id gh_vcpu_sched_match_table[] = {
	{ .compatible = "qcom,gh_vcpu_sched" },
	{},
};

static struct platform_driver gh_vcpu_sched_driver = {
	.probe = gh_vcpu_sched_probe,
	.driver = {
		.name = "gh_vcpu_sched",
		.owner = THIS_MODULE,
		.of_match_table = gh_vcpu_sched_match_table,
	 },
};

builtin_platform_driver(gh_vcpu_sched_driver);
MODULE_LICENSE("GPL v2");
