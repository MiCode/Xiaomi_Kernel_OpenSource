// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/soc/mediatek/gzvm_drv.h>
#include <linux/sched.h>
#include <linux/rcuwait.h>

/**
 * gzvm_handle_guest_exception() - Handle guest exception
 * @vcpu: Pointer to struct gzvm_vcpu_run in userspace
 * Return:
 * * true - This exception has been processed, no need to back to VMM.
 * * false - This exception has not been processed, require userspace.
 */
bool gzvm_handle_guest_exception(struct gzvm_vcpu *vcpu)
{
	int ret;

	for (int i = 0; i < ARRAY_SIZE(vcpu->run->exception.reserved); i++) {
		if (vcpu->run->exception.reserved[i])
			return false;
	}

	switch (vcpu->run->exception.exception) {
	case GZVM_EXCEPTION_PAGE_FAULT:
		ret = gzvm_handle_page_fault(vcpu);
		break;
	case GZVM_EXCEPTION_UNKNOWN:
		fallthrough;
	default:
		ret = -EFAULT;
	}

	if (!ret)
		return true;
	else
		return false;
}

/**
 * gzvm_handle_guest_hvc() - Handle guest hvc
 * @vcpu: Pointer to struct gzvm_vcpu struct
 * Return:
 * * true - This hvc has been processed, no need to back to VMM.
 * * false - This hvc has not been processed, require userspace.
 */
bool gzvm_handle_guest_hvc(struct gzvm_vcpu *vcpu)
{
	unsigned long ipa;
	int ret;

	switch (vcpu->run->hypercall.args[0]) {
	case GZVM_HVC_MEM_RELINQUISH:
		ipa = vcpu->run->hypercall.args[1];
		ret = gzvm_handle_relinquish(vcpu, ipa);
		return (ret == 0) ? true : false;
	default:
		return gzvm_arch_handle_guest_hvc(vcpu);
	}
}

static void vcpu_block_wait(struct gzvm_vcpu *vcpu)
{
	struct rcuwait *wait = &vcpu->wait;

	prepare_to_rcuwait(wait);

	while (true) {
		set_current_state(TASK_INTERRUPTIBLE);
		if (vcpu->idle_events.virtio_irq) {
			vcpu->idle_events.virtio_irq = 0;
			break;
		}
		if (vcpu->idle_events.vtimer_irq) {
			vcpu->idle_events.vtimer_irq = 0;
			break;
		}
		if (signal_pending(current))
			break;
		schedule();
	}
	finish_rcuwait(wait);
}

/**
 * gzvm_handle_guest_idle() - Handle guest vm entering idle
 * @vcpu: Pointer to struct gzvm_vcpu struct
 * Return:
 */
int gzvm_handle_guest_idle(struct gzvm_vcpu *vcpu)
{
	int ret = 0;
	u64 ns = 0;

	ns = gzvm_vcpu_arch_get_timer_delay_ns(vcpu);

	if (ns) {
		gzvm_vtimer_set(vcpu, ns);
		vcpu_block_wait(vcpu);
		gzvm_vtimer_release(vcpu);
	}

	return ret;
}

void gzvm_handle_guest_ipi(struct gzvm_vcpu *vcpu)
{
	gzvm_vcpu_wakeup_all(vcpu->gzvm);
}
