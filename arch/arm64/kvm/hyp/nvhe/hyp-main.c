// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 - Google Inc
 * Author: Andrew Scull <ascull@google.com>
 */

#include <kvm/arm_hypercalls.h>

#include <hyp/adjust_pc.h>

#include <asm/pgtable-types.h>
#include <asm/kvm_asm.h>
#include <asm/kvm_emulate.h>
#include <asm/kvm_host.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_mmu.h>

#include <nvhe/ffa.h>
#include <nvhe/iommu.h>
#include <nvhe/mem_protect.h>
#include <nvhe/mm.h>
#include <nvhe/pkvm.h>
#include <nvhe/trap_handler.h>

#include <linux/irqchip/arm-gic-v3.h>
#include <uapi/linux/psci.h>

#include "../../sys_regs.h"

struct pkvm_loaded_state {
	/* loaded vcpu is HYP VA */
	struct kvm_vcpu			*vcpu;
	bool				is_protected;

	/*
	 * Host FPSIMD state. Written to when the guest accesses its
	 * own FPSIMD state, and read when the guest state is live and
	 * that it needs to be switched back to the host.
	 *
	 * Only valid when the KVM_ARM64_FP_ENABLED flag is set in the
	 * shadow structure.
	 */
	struct user_fpsimd_state	host_fpsimd_state;
};

static DEFINE_PER_CPU(struct pkvm_loaded_state, loaded_state);

DEFINE_PER_CPU(struct kvm_nvhe_init_params, kvm_init_params);

void __kvm_hyp_host_forward_smc(struct kvm_cpu_context *host_ctxt);

typedef void (*shadow_entry_exit_handler_fn)(struct kvm_vcpu *, struct kvm_vcpu *);

static void handle_pvm_entry_wfx(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	shadow_vcpu->arch.flags |= host_vcpu->arch.flags & KVM_ARM64_INCREMENT_PC;
}

static int pkvm_refill_memcache(struct kvm_vcpu *shadow_vcpu,
				struct kvm_vcpu *host_vcpu)
{
	u64 nr_pages;

	nr_pages = VTCR_EL2_LVLS(shadow_vcpu->arch.pkvm.shadow_vm->arch.vtcr) - 1;
	return refill_memcache(&shadow_vcpu->arch.pkvm_memcache, nr_pages,
			       &host_vcpu->arch.pkvm_memcache);
}

static void handle_pvm_entry_psci(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	u32 psci_fn = smccc_get_function(shadow_vcpu);
	u64 ret = vcpu_get_reg(host_vcpu, 0);

	switch (psci_fn) {
	case PSCI_0_2_FN_CPU_ON:
	case PSCI_0_2_FN64_CPU_ON:
		/*
		 * Check whether the cpu_on request to the host was successful.
		 * If not, reset the vcpu state from ON_PENDING to OFF.
		 * This could happen if this vcpu attempted to turn on the other
		 * vcpu while the other one is in the process of turning itself
		 * off.
		 */
		if (ret != PSCI_RET_SUCCESS) {
			struct kvm_shadow_vm *vm = shadow_vcpu->arch.pkvm.shadow_vm;
			unsigned long cpu_id = smccc_get_arg1(shadow_vcpu);
			struct kvm_vcpu *vcpu = pvm_mpidr_to_vcpu(vm, cpu_id);

			if (vcpu && READ_ONCE(vcpu->arch.pkvm.power_state) == PSCI_0_2_AFFINITY_LEVEL_ON_PENDING)
				WRITE_ONCE(vcpu->arch.pkvm.power_state, PSCI_0_2_AFFINITY_LEVEL_OFF);

			ret = PSCI_RET_INTERNAL_FAILURE;
		}
		break;
	default:
		break;
	}

	vcpu_set_reg(shadow_vcpu, 0, ret);
}

static void handle_pvm_entry_hvc64(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	u32 fn = smccc_get_function(shadow_vcpu);

	switch (fn) {
	case ARM_SMCCC_VENDOR_HYP_KVM_MMIO_GUARD_MAP_FUNC_ID:
		pkvm_refill_memcache(shadow_vcpu, host_vcpu);
		break;
	case ARM_SMCCC_VENDOR_HYP_KVM_MEM_SHARE_FUNC_ID:
		fallthrough;
	case ARM_SMCCC_VENDOR_HYP_KVM_MEM_UNSHARE_FUNC_ID:
		vcpu_set_reg(shadow_vcpu, 0, SMCCC_RET_SUCCESS);
		break;
	default:
		handle_pvm_entry_psci(host_vcpu, shadow_vcpu);
		break;
	}
}

static void handle_pvm_entry_sys64(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	unsigned long host_flags;

	host_flags = READ_ONCE(host_vcpu->arch.flags);

	/* Exceptions have priority on anything else */
	if (host_flags & KVM_ARM64_PENDING_EXCEPTION) {
		/* Exceptions caused by this should be undef exceptions. */
		u32 esr = (ESR_ELx_EC_UNKNOWN << ESR_ELx_EC_SHIFT);

		__vcpu_sys_reg(shadow_vcpu, ESR_EL1) = esr;
		shadow_vcpu->arch.flags &= ~(KVM_ARM64_PENDING_EXCEPTION |
					     KVM_ARM64_EXCEPT_MASK);
		shadow_vcpu->arch.flags |= (KVM_ARM64_PENDING_EXCEPTION |
					    KVM_ARM64_EXCEPT_AA64_ELx_SYNC |
					    KVM_ARM64_EXCEPT_AA64_EL1);

		return;
	}


	if (host_flags & KVM_ARM64_INCREMENT_PC) {
		shadow_vcpu->arch.flags &= ~(KVM_ARM64_PENDING_EXCEPTION |
					     KVM_ARM64_EXCEPT_MASK);
		shadow_vcpu->arch.flags |= KVM_ARM64_INCREMENT_PC;
	}

	if (!esr_sys64_to_params(shadow_vcpu->arch.fault.esr_el2).is_write) {
		/* r0 as transfer register between the guest and the host. */
		u64 rt_val = vcpu_get_reg(host_vcpu, 0);
		int rt = kvm_vcpu_sys_get_rt(shadow_vcpu);

		vcpu_set_reg(shadow_vcpu, rt, rt_val);
	}
}

static void handle_pvm_entry_iabt(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	unsigned long cpsr = *vcpu_cpsr(shadow_vcpu);
	unsigned long host_flags;
	u32 esr = ESR_ELx_IL;

	host_flags = READ_ONCE(host_vcpu->arch.flags);

	if (!(host_flags & KVM_ARM64_PENDING_EXCEPTION))
		return;

	/*
	 * If the host wants to inject an exception, get syndrom and
	 * fault address.
	 */
	if ((cpsr & PSR_MODE_MASK) == PSR_MODE_EL0t)
		esr |= (ESR_ELx_EC_IABT_LOW << ESR_ELx_EC_SHIFT);
	else
		esr |= (ESR_ELx_EC_IABT_CUR << ESR_ELx_EC_SHIFT);

	esr |= ESR_ELx_FSC_EXTABT;

	__vcpu_sys_reg(shadow_vcpu, ESR_EL1) = esr;
	__vcpu_sys_reg(shadow_vcpu, FAR_EL1) = kvm_vcpu_get_hfar(shadow_vcpu);

	/* Tell the run loop that we want to inject something */
	shadow_vcpu->arch.flags &= ~(KVM_ARM64_PENDING_EXCEPTION |
				     KVM_ARM64_EXCEPT_MASK);
	shadow_vcpu->arch.flags |= (KVM_ARM64_PENDING_EXCEPTION |
				    KVM_ARM64_EXCEPT_AA64_ELx_SYNC |
				    KVM_ARM64_EXCEPT_AA64_EL1);
}

static void handle_pvm_entry_dabt(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	unsigned long host_flags;
	bool rd_update;

	host_flags = READ_ONCE(host_vcpu->arch.flags);

	/* Exceptions have priority over anything else */
	if (host_flags & KVM_ARM64_PENDING_EXCEPTION) {
		unsigned long cpsr = *vcpu_cpsr(shadow_vcpu);
		u32 esr = ESR_ELx_IL;

		if ((cpsr & PSR_MODE_MASK) == PSR_MODE_EL0t)
			esr |= (ESR_ELx_EC_DABT_LOW << ESR_ELx_EC_SHIFT);
		else
			esr |= (ESR_ELx_EC_DABT_CUR << ESR_ELx_EC_SHIFT);

		esr |= ESR_ELx_FSC_EXTABT;

		__vcpu_sys_reg(shadow_vcpu, ESR_EL1) = esr;
		__vcpu_sys_reg(shadow_vcpu, FAR_EL1) = kvm_vcpu_get_hfar(shadow_vcpu);
		/* Tell the run loop that we want to inject something */
		shadow_vcpu->arch.flags &= ~(KVM_ARM64_PENDING_EXCEPTION |
					     KVM_ARM64_EXCEPT_MASK);
		shadow_vcpu->arch.flags |= (KVM_ARM64_PENDING_EXCEPTION |
					    KVM_ARM64_EXCEPT_AA64_ELx_SYNC |
					    KVM_ARM64_EXCEPT_AA64_EL1);

		/* Cancel potential in-flight MMIO */
		shadow_vcpu->mmio_needed = false;
		return;
	}

	/* Handle PC increment on MMIO */
	if ((host_flags & KVM_ARM64_INCREMENT_PC) && shadow_vcpu->mmio_needed) {
		shadow_vcpu->arch.flags &= ~(KVM_ARM64_PENDING_EXCEPTION |
					     KVM_ARM64_EXCEPT_MASK);
		shadow_vcpu->arch.flags |= KVM_ARM64_INCREMENT_PC;
	}

	/* If we were doing an MMIO read access, update the register*/
	rd_update = (shadow_vcpu->mmio_needed &&
		     (host_flags & KVM_ARM64_INCREMENT_PC));
	rd_update &= !kvm_vcpu_dabt_iswrite(shadow_vcpu);

	if (rd_update) {
		/* r0 as transfer register between the guest and the host. */
		u64 rd_val = vcpu_get_reg(host_vcpu, 0);
		int rd = kvm_vcpu_dabt_get_rd(shadow_vcpu);

		vcpu_set_reg(shadow_vcpu, rd, rd_val);
	}

	shadow_vcpu->mmio_needed = false;
}

static void handle_pvm_exit_wfx(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	host_vcpu->arch.ctxt.regs.pstate = shadow_vcpu->arch.ctxt.regs.pstate &
		PSR_MODE_MASK;
	host_vcpu->arch.fault.esr_el2 = shadow_vcpu->arch.fault.esr_el2;
}

static void handle_pvm_exit_sys64(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	u32 esr_el2 = shadow_vcpu->arch.fault.esr_el2;

	/* r0 as transfer register between the guest and the host. */
	WRITE_ONCE(host_vcpu->arch.fault.esr_el2,
		   esr_el2 & ~ESR_ELx_SYS64_ISS_RT_MASK);

	/* The mode is required for the host to emulate some sysregs */
	WRITE_ONCE(host_vcpu->arch.ctxt.regs.pstate,
		   shadow_vcpu->arch.ctxt.regs.pstate & PSR_MODE_MASK);

	if (esr_sys64_to_params(esr_el2).is_write) {
		int rt = kvm_vcpu_sys_get_rt(shadow_vcpu);
		u64 rt_val = vcpu_get_reg(shadow_vcpu, rt);

		vcpu_set_reg(host_vcpu, 0, rt_val);
	}
}

static void handle_pvm_exit_hvc64(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	int n, i;

	switch (smccc_get_function(shadow_vcpu)) {
	/*
	 * CPU_ON takes 3 arguments, however, to wake up the target vcpu the
	 * host only needs to know the target's cpu_id, which is passed as the
	 * first argument. The processing of the reset state is done at hyp.
	 */
	case PSCI_0_2_FN_CPU_ON:
	case PSCI_0_2_FN64_CPU_ON:
		n = 2;
		break;

	case PSCI_0_2_FN_CPU_OFF:
	case PSCI_0_2_FN_SYSTEM_OFF:
	case PSCI_0_2_FN_SYSTEM_RESET:
	case PSCI_0_2_FN_CPU_SUSPEND:
	case PSCI_0_2_FN64_CPU_SUSPEND:
		n = 1;
		break;

	case ARM_SMCCC_VENDOR_HYP_KVM_MEM_SHARE_FUNC_ID:
		fallthrough;
	case ARM_SMCCC_VENDOR_HYP_KVM_MEM_UNSHARE_FUNC_ID:
		n = 4;
		break;

	case PSCI_1_1_FN_SYSTEM_RESET2:
	case PSCI_1_1_FN64_SYSTEM_RESET2:
	case ARM_SMCCC_VENDOR_HYP_KVM_MMIO_GUARD_MAP_FUNC_ID:
		n = 3;
		break;

	/*
	 * The rest are either blocked or handled by HYP, so we should
	 * really never be here.
	 */
	default:
		BUG();
	}

	host_vcpu->arch.fault.esr_el2 = shadow_vcpu->arch.fault.esr_el2;

	/* Pass the hvc function id (r0) as well as any potential arguments. */
	for (i = 0; i < n; i++)
		vcpu_set_reg(host_vcpu, i, vcpu_get_reg(shadow_vcpu, i));
}

static void handle_pvm_exit_iabt(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	WRITE_ONCE(host_vcpu->arch.fault.esr_el2,
		   shadow_vcpu->arch.fault.esr_el2);
	WRITE_ONCE(host_vcpu->arch.fault.hpfar_el2,
		   shadow_vcpu->arch.fault.hpfar_el2);
}

static void handle_pvm_exit_dabt(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	shadow_vcpu->mmio_needed = __pkvm_check_ioguard_page(shadow_vcpu);

	if (shadow_vcpu->mmio_needed) {
		/* r0 as transfer register between the guest and the host. */
		WRITE_ONCE(host_vcpu->arch.fault.esr_el2,
			   shadow_vcpu->arch.fault.esr_el2 & ~ESR_ELx_SRT_MASK);

		if (kvm_vcpu_dabt_iswrite(shadow_vcpu)) {
			int rt = kvm_vcpu_dabt_get_rd(shadow_vcpu);
			u64 rt_val = vcpu_get_reg(shadow_vcpu, rt);

			vcpu_set_reg(host_vcpu, 0, rt_val);
		}
	} else {
		WRITE_ONCE(host_vcpu->arch.fault.esr_el2,
			   shadow_vcpu->arch.fault.esr_el2 & ~ESR_ELx_ISV);
	}

	WRITE_ONCE(host_vcpu->arch.ctxt.regs.pstate,
		   shadow_vcpu->arch.ctxt.regs.pstate & PSR_MODE_MASK);
	WRITE_ONCE(host_vcpu->arch.fault.far_el2,
		   shadow_vcpu->arch.fault.far_el2 & FAR_MASK);
	WRITE_ONCE(host_vcpu->arch.fault.hpfar_el2,
		   shadow_vcpu->arch.fault.hpfar_el2);
	WRITE_ONCE(__vcpu_sys_reg(host_vcpu, SCTLR_EL1),
		   __vcpu_sys_reg(shadow_vcpu, SCTLR_EL1) & (SCTLR_ELx_EE | SCTLR_EL1_E0E));
}

static void handle_vm_entry_generic(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	unsigned long host_flags = READ_ONCE(host_vcpu->arch.flags);

	shadow_vcpu->arch.flags &= ~(KVM_ARM64_PENDING_EXCEPTION |
				     KVM_ARM64_EXCEPT_MASK);

	if (host_flags & KVM_ARM64_PENDING_EXCEPTION) {
		shadow_vcpu->arch.flags |= KVM_ARM64_PENDING_EXCEPTION;
		shadow_vcpu->arch.flags |= host_flags & KVM_ARM64_EXCEPT_MASK;
	} else if (host_flags & KVM_ARM64_INCREMENT_PC) {
		shadow_vcpu->arch.flags |= KVM_ARM64_INCREMENT_PC;
	}
}

static void handle_vm_exit_generic(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	host_vcpu->arch.fault.esr_el2 = shadow_vcpu->arch.fault.esr_el2;
}

static void handle_vm_exit_abt(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	host_vcpu->arch.fault = shadow_vcpu->arch.fault;
}

static const shadow_entry_exit_handler_fn entry_pvm_shadow_handlers[] = {
	[0 ... ESR_ELx_EC_MAX]		= NULL,
	[ESR_ELx_EC_WFx]		= handle_pvm_entry_wfx,
	[ESR_ELx_EC_HVC64]		= handle_pvm_entry_hvc64,
	[ESR_ELx_EC_SYS64]		= handle_pvm_entry_sys64,
	[ESR_ELx_EC_IABT_LOW]		= handle_pvm_entry_iabt,
	[ESR_ELx_EC_DABT_LOW]		= handle_pvm_entry_dabt,
};

static const shadow_entry_exit_handler_fn exit_pvm_shadow_handlers[] = {
	[0 ... ESR_ELx_EC_MAX]		= NULL,
	[ESR_ELx_EC_WFx]		= handle_pvm_exit_wfx,
	[ESR_ELx_EC_HVC64]		= handle_pvm_exit_hvc64,
	[ESR_ELx_EC_SYS64]		= handle_pvm_exit_sys64,
	[ESR_ELx_EC_IABT_LOW]		= handle_pvm_exit_iabt,
	[ESR_ELx_EC_DABT_LOW]		= handle_pvm_exit_dabt,
};

static const shadow_entry_exit_handler_fn entry_vm_shadow_handlers[] = {
	[0 ... ESR_ELx_EC_MAX]		= handle_vm_entry_generic,
};

static const shadow_entry_exit_handler_fn exit_vm_shadow_handlers[] = {
	[0 ... ESR_ELx_EC_MAX]		= handle_vm_exit_generic,
	[ESR_ELx_EC_IABT_LOW]		= handle_vm_exit_abt,
	[ESR_ELx_EC_DABT_LOW]		= handle_vm_exit_abt,
};

static void flush_vgic_state(struct kvm_vcpu *host_vcpu,
			     struct kvm_vcpu *shadow_vcpu)
{
	struct vgic_v3_cpu_if *host_cpu_if, *shadow_cpu_if;
	unsigned int used_lrs, max_lrs, i;

	host_cpu_if	= &host_vcpu->arch.vgic_cpu.vgic_v3;
	shadow_cpu_if	= &shadow_vcpu->arch.vgic_cpu.vgic_v3;

	max_lrs = (read_gicreg(ICH_VTR_EL2) & 0xf) + 1;
	used_lrs = READ_ONCE(host_cpu_if->used_lrs);
	used_lrs = min(used_lrs, max_lrs);

	shadow_cpu_if->vgic_hcr	= host_cpu_if->vgic_hcr;
	/* Should be a one-off */
	shadow_cpu_if->vgic_sre = (ICC_SRE_EL1_DIB |
				   ICC_SRE_EL1_DFB |
				   ICC_SRE_EL1_SRE);
	shadow_cpu_if->used_lrs	= used_lrs;

	for (i = 0; i < used_lrs; i++)
		shadow_cpu_if->vgic_lr[i] = host_cpu_if->vgic_lr[i];
}

static void sync_vgic_state(struct kvm_vcpu *host_vcpu,
			    struct kvm_vcpu *shadow_vcpu)
{
	struct vgic_v3_cpu_if *host_cpu_if, *shadow_cpu_if;
	unsigned int i;

	host_cpu_if	= &host_vcpu->arch.vgic_cpu.vgic_v3;
	shadow_cpu_if	= &shadow_vcpu->arch.vgic_cpu.vgic_v3;

	host_cpu_if->vgic_hcr	= shadow_cpu_if->vgic_hcr;

	for (i = 0; i < shadow_cpu_if->used_lrs; i++)
		host_cpu_if->vgic_lr[i] = shadow_cpu_if->vgic_lr[i];
}

static void flush_timer_state(struct pkvm_loaded_state *state)
{
	struct kvm_vcpu *shadow_vcpu = state->vcpu;

	if (!state->is_protected)
		return;

	/*
	 * A shadow vcpu has no offset, and sees vtime == ptime. The
	 * ptimer is fully emulated by EL1 and cannot be trusted.
	 */
	write_sysreg(0, cntvoff_el2);
	isb();
	write_sysreg_el0(__vcpu_sys_reg(shadow_vcpu, CNTV_CVAL_EL0), SYS_CNTV_CVAL);
	write_sysreg_el0(__vcpu_sys_reg(shadow_vcpu, CNTV_CTL_EL0), SYS_CNTV_CTL);
}

static void sync_timer_state(struct pkvm_loaded_state *state)
{
	struct kvm_vcpu *shadow_vcpu = state->vcpu;

	if (!state->is_protected)
		return;

	/*
	 * Preserve the vtimer state so that it is always correct,
	 * even if the host tries to make a mess.
	 */
	__vcpu_sys_reg(shadow_vcpu, CNTV_CVAL_EL0) = read_sysreg_el0(SYS_CNTV_CVAL);
	__vcpu_sys_reg(shadow_vcpu, CNTV_CTL_EL0) = read_sysreg_el0(SYS_CNTV_CTL);
}

static void __copy_vcpu_state(const struct kvm_vcpu *from_vcpu,
			      struct kvm_vcpu *to_vcpu)
{
	int i;

	to_vcpu->arch.ctxt.regs		= from_vcpu->arch.ctxt.regs;
	to_vcpu->arch.ctxt.spsr_abt	= from_vcpu->arch.ctxt.spsr_abt;
	to_vcpu->arch.ctxt.spsr_und	= from_vcpu->arch.ctxt.spsr_und;
	to_vcpu->arch.ctxt.spsr_irq	= from_vcpu->arch.ctxt.spsr_irq;
	to_vcpu->arch.ctxt.spsr_fiq	= from_vcpu->arch.ctxt.spsr_fiq;

	/*
	 * Copy the sysregs, but don't mess with the timer state which
	 * is directly handled by EL1 and is expected to be preserved.
	 */
	for (i = 1; i < NR_SYS_REGS; i++) {
		if (i >= CNTVOFF_EL2 && i <= CNTP_CTL_EL0)
			continue;
		to_vcpu->arch.ctxt.sys_regs[i] = from_vcpu->arch.ctxt.sys_regs[i];
	}
}

static void __sync_vcpu_state(struct kvm_vcpu *shadow_vcpu)
{
	struct kvm_vcpu *host_vcpu = shadow_vcpu->arch.pkvm.host_vcpu;

	__copy_vcpu_state(shadow_vcpu, host_vcpu);
}

static void __flush_vcpu_state(struct kvm_vcpu *shadow_vcpu)
{
	struct kvm_vcpu *host_vcpu = shadow_vcpu->arch.pkvm.host_vcpu;

	__copy_vcpu_state(host_vcpu, shadow_vcpu);
}

static void flush_shadow_state(struct pkvm_loaded_state *state)
{
	struct kvm_vcpu *shadow_vcpu = state->vcpu;
	struct kvm_vcpu *host_vcpu = shadow_vcpu->arch.pkvm.host_vcpu;
	u8 esr_ec;
	shadow_entry_exit_handler_fn ec_handler;

	if (READ_ONCE(shadow_vcpu->arch.pkvm.power_state) == PSCI_0_2_AFFINITY_LEVEL_ON_PENDING)
		pkvm_reset_vcpu(shadow_vcpu);

	/*
	 * If we deal with a non-protected guest and that the state is
	 * dirty (from a host perspective), copy the state back into
	 * the shadow.
	 */
	if (!state->is_protected) {
		if (READ_ONCE(host_vcpu->arch.flags) & KVM_ARM64_PKVM_STATE_DIRTY)
			__flush_vcpu_state(shadow_vcpu);

		state->vcpu->arch.hcr_el2 = HCR_GUEST_FLAGS & ~(HCR_RW | HCR_TWI | HCR_TWE);
		state->vcpu->arch.hcr_el2 |= host_vcpu->arch.hcr_el2;
	}

	flush_vgic_state(host_vcpu, shadow_vcpu);
	flush_timer_state(state);

	switch (ARM_EXCEPTION_CODE(shadow_vcpu->arch.pkvm.exit_code)) {
	case ARM_EXCEPTION_IRQ:
	case ARM_EXCEPTION_EL1_SERROR:
	case ARM_EXCEPTION_IL:
		break;
	case ARM_EXCEPTION_TRAP:
		esr_ec = ESR_ELx_EC(kvm_vcpu_get_esr(shadow_vcpu));
		if (state->is_protected)
			ec_handler = entry_pvm_shadow_handlers[esr_ec];
		else
			ec_handler = entry_vm_shadow_handlers[esr_ec];

		if (ec_handler)
			ec_handler(host_vcpu, shadow_vcpu);

		break;
	default:
		BUG();
	}

	shadow_vcpu->arch.pkvm.exit_code = 0;
}

static void sync_shadow_state(struct pkvm_loaded_state *state, u32 exit_reason)
{
	struct kvm_vcpu *shadow_vcpu = state->vcpu;
	struct kvm_vcpu *host_vcpu = shadow_vcpu->arch.pkvm.host_vcpu;
	u8 esr_ec;
	shadow_entry_exit_handler_fn ec_handler;

	/*
	 * Don't sync the vcpu GPR/sysreg state after a run. Instead,
	 * leave it in the shadow until someone actually requires it.
	 */
	sync_vgic_state(host_vcpu, shadow_vcpu);
	sync_timer_state(state);

	switch (ARM_EXCEPTION_CODE(exit_reason)) {
	case ARM_EXCEPTION_IRQ:
		break;
	case ARM_EXCEPTION_TRAP:
		esr_ec = ESR_ELx_EC(kvm_vcpu_get_esr(shadow_vcpu));
		if (state->is_protected)
			ec_handler = exit_pvm_shadow_handlers[esr_ec];
		else
			ec_handler = exit_vm_shadow_handlers[esr_ec];

		if (ec_handler)
			ec_handler(host_vcpu, shadow_vcpu);

		break;
	case ARM_EXCEPTION_EL1_SERROR:
	case ARM_EXCEPTION_IL:
		break;
	default:
		BUG();
	}

	host_vcpu->arch.flags &= ~(KVM_ARM64_PENDING_EXCEPTION | KVM_ARM64_INCREMENT_PC);
	shadow_vcpu->arch.pkvm.exit_code = exit_reason;
}

static void fpsimd_host_restore(void)
{
	sysreg_clear_set(cptr_el2, CPTR_EL2_TZ | CPTR_EL2_TFP, 0);
	isb();

	if (unlikely(is_protected_kvm_enabled())) {
		struct pkvm_loaded_state *state = this_cpu_ptr(&loaded_state);

		__fpsimd_save_state(&state->vcpu->arch.ctxt.fp_regs);
		__fpsimd_restore_state(&state->host_fpsimd_state);

		state->vcpu->arch.flags &= ~KVM_ARM64_FP_ENABLED;
		state->vcpu->arch.flags |= KVM_ARM64_FP_HOST;
	}

	if (system_supports_sve())
		sve_cond_update_zcr_vq(ZCR_ELx_LEN_MASK, SYS_ZCR_EL2);
}

static void handle___pkvm_vcpu_load(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(int, shadow_handle, host_ctxt, 1);
	DECLARE_REG(int, vcpu_idx, host_ctxt, 2);
	DECLARE_REG(u64, hcr_el2, host_ctxt, 3);
	struct pkvm_loaded_state *state;

	/* Why did you bother? */
	if (!is_protected_kvm_enabled())
		return;

	state = this_cpu_ptr(&loaded_state);

	/* Nice try */
	if (state->vcpu)
		return;

	state->vcpu = get_shadow_vcpu(shadow_handle, vcpu_idx);

	if (!state->vcpu)
		return;

	state->is_protected = state->vcpu->arch.pkvm.shadow_vm->arch.pkvm.enabled;

	state->vcpu->arch.host_fpsimd_state = &state->host_fpsimd_state;
	state->vcpu->arch.flags |= KVM_ARM64_FP_HOST;

	if (state->is_protected) {
		/* Propagate WFx trapping flags, trap ptrauth */
		state->vcpu->arch.hcr_el2 &= ~(HCR_TWE | HCR_TWI |
					       HCR_API | HCR_APK);
		state->vcpu->arch.hcr_el2 |= hcr_el2 & (HCR_TWE | HCR_TWI);
	}
}

static void handle___pkvm_vcpu_put(struct kvm_cpu_context *host_ctxt)
{
	if (unlikely(is_protected_kvm_enabled())) {
		struct pkvm_loaded_state *state = this_cpu_ptr(&loaded_state);

		if (state->vcpu) {
			struct kvm_vcpu *host_vcpu = state->vcpu->arch.pkvm.host_vcpu;

			if (state->vcpu->arch.flags & KVM_ARM64_FP_ENABLED)
				fpsimd_host_restore();

			if (!state->is_protected &&
			    !(READ_ONCE(host_vcpu->arch.flags) & KVM_ARM64_PKVM_STATE_DIRTY))
				__sync_vcpu_state(state->vcpu);

			put_shadow_vcpu(state->vcpu);

			/* "It's over and done with..." */
			state->vcpu = NULL;
		}
	}
}

static void handle___pkvm_vcpu_sync_state(struct kvm_cpu_context *host_ctxt)
{
	if (unlikely(is_protected_kvm_enabled())) {
		struct pkvm_loaded_state *state = this_cpu_ptr(&loaded_state);

		if (!state->vcpu || state->is_protected)
			return;

		__sync_vcpu_state(state->vcpu);
	}
}

static struct kvm_vcpu *__get_current_vcpu(struct kvm_vcpu *vcpu,
					   struct pkvm_loaded_state **state)
{
	struct pkvm_loaded_state *sstate = NULL;

	vcpu = kern_hyp_va(vcpu);

	if (unlikely(is_protected_kvm_enabled())) {
		sstate = this_cpu_ptr(&loaded_state);

		if (!sstate || vcpu != sstate->vcpu->arch.pkvm.host_vcpu) {
			sstate = NULL;
			vcpu = NULL;
		}
	}

	*state = sstate;
	return vcpu;
}

#define get_current_vcpu(ctxt, regnr, statepp)				\
	({								\
		DECLARE_REG(struct kvm_vcpu *, __vcpu, ctxt, regnr);	\
		__get_current_vcpu(__vcpu, statepp);			\
	})

#define get_current_vcpu_from_cpu_if(ctxt, regnr, statepp)		\
	({								\
		DECLARE_REG(struct vgic_v3_cpu_if *, cif, ctxt, regnr); \
		struct kvm_vcpu *__vcpu;				\
		__vcpu = container_of(cif,				\
				      struct kvm_vcpu,			\
				      arch.vgic_cpu.vgic_v3);		\
									\
		__get_current_vcpu(__vcpu, statepp);			\
	})

static void handle___kvm_vcpu_run(struct kvm_cpu_context *host_ctxt)
{
	struct pkvm_loaded_state *shadow_state;
	struct kvm_vcpu *vcpu;
	int ret;

	vcpu = get_current_vcpu(host_ctxt, 1, &shadow_state);
	if (!vcpu) {
		cpu_reg(host_ctxt, 1) =  -EINVAL;
		return;
	}

	if (unlikely(shadow_state)) {
		flush_shadow_state(shadow_state);

		ret = __kvm_vcpu_run(shadow_state->vcpu);

		sync_shadow_state(shadow_state, ret);

		if (shadow_state->vcpu->arch.flags & KVM_ARM64_FP_ENABLED) {
			/*
			 * The guest has used the FP, trap all accesses
			 * from the host (both FP and SVE).
			 */
			u64 reg = CPTR_EL2_TFP;
			if (system_supports_sve())
				reg |= CPTR_EL2_TZ;

			sysreg_clear_set(cptr_el2, 0, reg);
		}
	} else {
		ret = __kvm_vcpu_run(vcpu);
	}

	cpu_reg(host_ctxt, 1) =  ret;
}

static void handle___pkvm_host_donate_guest(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(u64, pfn, host_ctxt, 1);
	DECLARE_REG(u64, gfn, host_ctxt, 2);
	struct kvm_vcpu *host_vcpu;
	struct pkvm_loaded_state *state;
	int ret = -EINVAL;

	if (!is_protected_kvm_enabled())
		goto out;

	state = this_cpu_ptr(&loaded_state);
	if (!state->vcpu)
		goto out;

	host_vcpu = state->vcpu->arch.pkvm.host_vcpu;

	/* Topup shadow memcache with the host's */
	ret = pkvm_refill_memcache(state->vcpu, host_vcpu);
	if (!ret) {
		if (state->is_protected)
			ret = __pkvm_host_donate_guest(pfn, gfn, state->vcpu);
		else
			ret = __pkvm_host_share_guest(pfn, gfn, state->vcpu);
	}
out:
	cpu_reg(host_ctxt, 1) =  ret;
}

static void handle___kvm_adjust_pc(struct kvm_cpu_context *host_ctxt)
{
	struct pkvm_loaded_state *shadow_state;
	struct kvm_vcpu *vcpu;

	vcpu = get_current_vcpu(host_ctxt, 1, &shadow_state);
	if (!vcpu)
		return;

	if (shadow_state) {
		/* This only applies to non-protected VMs */
		if (shadow_state->is_protected)
			return;

		vcpu = shadow_state->vcpu;
	}

	__kvm_adjust_pc(vcpu);
}

static void handle___kvm_flush_vm_context(struct kvm_cpu_context *host_ctxt)
{
	__kvm_flush_vm_context();
}

static void handle___kvm_tlb_flush_vmid_ipa(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(struct kvm_s2_mmu *, mmu, host_ctxt, 1);
	DECLARE_REG(phys_addr_t, ipa, host_ctxt, 2);
	DECLARE_REG(int, level, host_ctxt, 3);

	__kvm_tlb_flush_vmid_ipa(kern_hyp_va(mmu), ipa, level);
}

static void handle___kvm_tlb_flush_vmid(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(struct kvm_s2_mmu *, mmu, host_ctxt, 1);

	__kvm_tlb_flush_vmid(kern_hyp_va(mmu));
}

static void handle___kvm_flush_cpu_context(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(struct kvm_s2_mmu *, mmu, host_ctxt, 1);

	__kvm_flush_cpu_context(kern_hyp_va(mmu));
}

static void handle___kvm_timer_set_cntvoff(struct kvm_cpu_context *host_ctxt)
{
	__kvm_timer_set_cntvoff(cpu_reg(host_ctxt, 1));
}

static void handle___kvm_enable_ssbs(struct kvm_cpu_context *host_ctxt)
{
	u64 tmp;

	tmp = read_sysreg_el2(SYS_SCTLR);
	tmp |= SCTLR_ELx_DSSBS;
	write_sysreg_el2(tmp, SYS_SCTLR);
}

static void handle___vgic_v3_get_gic_config(struct kvm_cpu_context *host_ctxt)
{
	cpu_reg(host_ctxt, 1) = __vgic_v3_get_gic_config();
}

static void handle___vgic_v3_init_lrs(struct kvm_cpu_context *host_ctxt)
{
	__vgic_v3_init_lrs();
}

static void handle___kvm_get_mdcr_el2(struct kvm_cpu_context *host_ctxt)
{
	cpu_reg(host_ctxt, 1) = __kvm_get_mdcr_el2();
}

static void handle___vgic_v3_save_vmcr_aprs(struct kvm_cpu_context *host_ctxt)
{
	struct pkvm_loaded_state *shadow_state;
	struct kvm_vcpu *vcpu;

	vcpu = get_current_vcpu_from_cpu_if(host_ctxt, 1, &shadow_state);
	if (!vcpu)
		return;

	if (shadow_state) {
		struct vgic_v3_cpu_if *shadow_cpu_if, *cpu_if;
		int i;

		shadow_cpu_if = &shadow_state->vcpu->arch.vgic_cpu.vgic_v3;
		__vgic_v3_save_vmcr_aprs(shadow_cpu_if);

		cpu_if = &vcpu->arch.vgic_cpu.vgic_v3;

		cpu_if->vgic_vmcr = shadow_cpu_if->vgic_vmcr;
		for (i = 0; i < ARRAY_SIZE(cpu_if->vgic_ap0r); i++) {
			cpu_if->vgic_ap0r[i] = shadow_cpu_if->vgic_ap0r[i];
			cpu_if->vgic_ap1r[i] = shadow_cpu_if->vgic_ap1r[i];
		}
	} else {
		__vgic_v3_save_vmcr_aprs(&vcpu->arch.vgic_cpu.vgic_v3);
	}
}

static void handle___vgic_v3_restore_vmcr_aprs(struct kvm_cpu_context *host_ctxt)
{
	struct pkvm_loaded_state *shadow_state;
	struct kvm_vcpu *vcpu;

	vcpu = get_current_vcpu_from_cpu_if(host_ctxt, 1, &shadow_state);
	if (!vcpu)
		return;

	if (shadow_state) {
		struct vgic_v3_cpu_if *shadow_cpu_if, *cpu_if;
		int i;

		shadow_cpu_if = &shadow_state->vcpu->arch.vgic_cpu.vgic_v3;
		cpu_if = &vcpu->arch.vgic_cpu.vgic_v3;

		shadow_cpu_if->vgic_vmcr = cpu_if->vgic_vmcr;
		/* Should be a one-off */
		shadow_cpu_if->vgic_sre = (ICC_SRE_EL1_DIB |
					   ICC_SRE_EL1_DFB |
					   ICC_SRE_EL1_SRE);
		for (i = 0; i < ARRAY_SIZE(cpu_if->vgic_ap0r); i++) {
			shadow_cpu_if->vgic_ap0r[i] = cpu_if->vgic_ap0r[i];
			shadow_cpu_if->vgic_ap1r[i] = cpu_if->vgic_ap1r[i];
		}

		__vgic_v3_restore_vmcr_aprs(shadow_cpu_if);
	} else {
		__vgic_v3_restore_vmcr_aprs(&vcpu->arch.vgic_cpu.vgic_v3);
	}
}

static void handle___pkvm_init(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(phys_addr_t, phys, host_ctxt, 1);
	DECLARE_REG(unsigned long, size, host_ctxt, 2);
	DECLARE_REG(unsigned long, nr_cpus, host_ctxt, 3);
	DECLARE_REG(unsigned long *, per_cpu_base, host_ctxt, 4);
	DECLARE_REG(u32, hyp_va_bits, host_ctxt, 5);

	/*
	 * __pkvm_init() will return only if an error occurred, otherwise it
	 * will tail-call in __pkvm_init_finalise() which will have to deal
	 * with the host context directly.
	 */
	cpu_reg(host_ctxt, 1) = __pkvm_init(phys, size, nr_cpus, per_cpu_base,
					    hyp_va_bits);
}

static void handle___pkvm_cpu_set_vector(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(enum arm64_hyp_spectre_vector, slot, host_ctxt, 1);

	cpu_reg(host_ctxt, 1) = pkvm_cpu_set_vector(slot);
}

static void handle___pkvm_host_share_hyp(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(u64, pfn, host_ctxt, 1);

	cpu_reg(host_ctxt, 1) = __pkvm_host_share_hyp(pfn);
}

static void handle___pkvm_host_unshare_hyp(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(u64, pfn, host_ctxt, 1);

	cpu_reg(host_ctxt, 1) = __pkvm_host_unshare_hyp(pfn);
}

static void handle___pkvm_host_reclaim_page(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(u64, pfn, host_ctxt, 1);

	cpu_reg(host_ctxt, 1) = __pkvm_host_reclaim_page(pfn);
}

static void handle___pkvm_create_private_mapping(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(phys_addr_t, phys, host_ctxt, 1);
	DECLARE_REG(size_t, size, host_ctxt, 2);
	DECLARE_REG(enum kvm_pgtable_prot, prot, host_ctxt, 3);

	cpu_reg(host_ctxt, 1) = __pkvm_create_private_mapping(phys, size, prot);
}

static void handle___pkvm_prot_finalize(struct kvm_cpu_context *host_ctxt)
{
	cpu_reg(host_ctxt, 1) = __pkvm_prot_finalize();
}

static void handle___pkvm_init_shadow(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(struct kvm *, host_kvm, host_ctxt, 1);
	DECLARE_REG(void *, host_shadow_va, host_ctxt, 2);
	DECLARE_REG(size_t, shadow_size, host_ctxt, 3);
	DECLARE_REG(void *, pgd, host_ctxt, 4);

	cpu_reg(host_ctxt, 1) = __pkvm_init_shadow(host_kvm, host_shadow_va,
						   shadow_size, pgd);
}

static void handle___pkvm_teardown_shadow(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(int, shadow_handle, host_ctxt, 1);

	cpu_reg(host_ctxt, 1) = __pkvm_teardown_shadow(shadow_handle);
}

static void handle___pkvm_iommu_driver_init(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(enum pkvm_iommu_driver_id, id, host_ctxt, 1);
	DECLARE_REG(void *, data, host_ctxt, 2);
	DECLARE_REG(size_t, size, host_ctxt, 3);

	cpu_reg(host_ctxt, 1) = __pkvm_iommu_driver_init(id, data, size);
}

static void handle___pkvm_iommu_register(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(unsigned long, dev_id, host_ctxt, 1);
	DECLARE_REG(enum pkvm_iommu_driver_id, drv_id, host_ctxt, 2);
	DECLARE_REG(phys_addr_t, dev_pa, host_ctxt, 3);
	DECLARE_REG(size_t, dev_size, host_ctxt, 4);
	DECLARE_REG(unsigned long, parent_id, host_ctxt, 5);
	DECLARE_REG(void *, mem, host_ctxt, 6);
	DECLARE_REG(size_t, mem_size, host_ctxt, 7);

	cpu_reg(host_ctxt, 1) = __pkvm_iommu_register(dev_id, drv_id, dev_pa,
						      dev_size, parent_id,
						      mem, mem_size);
}

static void handle___pkvm_iommu_pm_notify(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(unsigned long, dev_id, host_ctxt, 1);
	DECLARE_REG(enum pkvm_iommu_pm_event, event, host_ctxt, 2);

	cpu_reg(host_ctxt, 1) = __pkvm_iommu_pm_notify(dev_id, event);
}

static void handle___pkvm_iommu_finalize(struct kvm_cpu_context *host_ctxt)
{
	cpu_reg(host_ctxt, 1) = __pkvm_iommu_finalize();
}

typedef void (*hcall_t)(struct kvm_cpu_context *);

#define HANDLE_FUNC(x)	[__KVM_HOST_SMCCC_FUNC_##x] = (hcall_t)handle_##x

static const hcall_t host_hcall[] = {
	/* ___kvm_hyp_init */
	HANDLE_FUNC(__kvm_get_mdcr_el2),
	HANDLE_FUNC(__pkvm_init),
	HANDLE_FUNC(__pkvm_create_private_mapping),
	HANDLE_FUNC(__pkvm_cpu_set_vector),
	HANDLE_FUNC(__kvm_enable_ssbs),
	HANDLE_FUNC(__vgic_v3_init_lrs),
	HANDLE_FUNC(__vgic_v3_get_gic_config),
	HANDLE_FUNC(__kvm_flush_vm_context),
	HANDLE_FUNC(__kvm_tlb_flush_vmid_ipa),
	HANDLE_FUNC(__kvm_tlb_flush_vmid),
	HANDLE_FUNC(__kvm_flush_cpu_context),
	HANDLE_FUNC(__pkvm_prot_finalize),

	HANDLE_FUNC(__pkvm_host_share_hyp),
	HANDLE_FUNC(__pkvm_host_unshare_hyp),
	HANDLE_FUNC(__pkvm_host_reclaim_page),
	HANDLE_FUNC(__pkvm_host_donate_guest),
	HANDLE_FUNC(__kvm_adjust_pc),
	HANDLE_FUNC(__kvm_vcpu_run),
	HANDLE_FUNC(__kvm_timer_set_cntvoff),
	HANDLE_FUNC(__vgic_v3_save_vmcr_aprs),
	HANDLE_FUNC(__vgic_v3_restore_vmcr_aprs),
	HANDLE_FUNC(__pkvm_init_shadow),
	HANDLE_FUNC(__pkvm_teardown_shadow),
	HANDLE_FUNC(__pkvm_vcpu_load),
	HANDLE_FUNC(__pkvm_vcpu_put),
	HANDLE_FUNC(__pkvm_vcpu_sync_state),
	HANDLE_FUNC(__pkvm_iommu_driver_init),
	HANDLE_FUNC(__pkvm_iommu_register),
	HANDLE_FUNC(__pkvm_iommu_pm_notify),
	HANDLE_FUNC(__pkvm_iommu_finalize),
};

static void handle_host_hcall(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(unsigned long, id, host_ctxt, 0);
	unsigned long hcall_min = 0;
	hcall_t hfn;

	/*
	 * If pKVM has been initialised then reject any calls to the
	 * early "privileged" hypercalls. Note that we cannot reject
	 * calls to __pkvm_prot_finalize for two reasons: (1) The static
	 * key used to determine initialisation must be toggled prior to
	 * finalisation and (2) finalisation is performed on a per-CPU
	 * basis. This is all fine, however, since __pkvm_prot_finalize
	 * returns -EPERM after the first call for a given CPU.
	 */
	if (static_branch_unlikely(&kvm_protected_mode_initialized))
		hcall_min = __KVM_HOST_SMCCC_FUNC___pkvm_prot_finalize;

	id -= KVM_HOST_SMCCC_ID(0);

	if (unlikely(id < hcall_min || id >= ARRAY_SIZE(host_hcall)))
		goto inval;

	hfn = host_hcall[id];
	if (unlikely(!hfn))
		goto inval;

	cpu_reg(host_ctxt, 0) = SMCCC_RET_SUCCESS;
	hfn(host_ctxt);

	return;
inval:
	cpu_reg(host_ctxt, 0) = SMCCC_RET_NOT_SUPPORTED;
}

static void default_host_smc_handler(struct kvm_cpu_context *host_ctxt)
{
	__kvm_hyp_host_forward_smc(host_ctxt);
}

static void handle_host_smc(struct kvm_cpu_context *host_ctxt)
{
	bool handled;

	handled = kvm_host_psci_handler(host_ctxt);
	if (!handled)
		handled = kvm_host_ffa_handler(host_ctxt);
	if (!handled)
		default_host_smc_handler(host_ctxt);

	/* SMC was trapped, move ELR past the current PC. */
	kvm_skip_host_instr();
}

void handle_trap(struct kvm_cpu_context *host_ctxt)
{
	u64 esr = read_sysreg_el2(SYS_ESR);

	switch (ESR_ELx_EC(esr)) {
	case ESR_ELx_EC_HVC64:
		handle_host_hcall(host_ctxt);
		break;
	case ESR_ELx_EC_SMC64:
		handle_host_smc(host_ctxt);
		break;
	case ESR_ELx_EC_FP_ASIMD:
	case ESR_ELx_EC_SVE:
		fpsimd_host_restore();
		break;
	case ESR_ELx_EC_IABT_LOW:
	case ESR_ELx_EC_DABT_LOW:
		handle_host_mem_abort(host_ctxt);
		break;
	default:
		BUG();
	}
}
