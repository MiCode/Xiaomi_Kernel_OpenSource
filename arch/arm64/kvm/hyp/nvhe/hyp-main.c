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

#include <nvhe/mem_protect.h>
#include <nvhe/mm.h>
#include <nvhe/pkvm.h>
#include <nvhe/trap_handler.h>

#include <linux/irqchip/arm-gic-v3.h>
#include <uapi/linux/psci.h>

DEFINE_PER_CPU(struct kvm_nvhe_init_params, kvm_init_params);

struct kvm_iommu_ops kvm_iommu_ops;

void __kvm_hyp_host_forward_smc(struct kvm_cpu_context *host_ctxt);

typedef void (*shadow_entry_exit_handler_fn)(struct kvm_vcpu *, struct kvm_vcpu *);

static void handle_pvm_entry_wfx(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	shadow_vcpu->arch.flags |= host_vcpu->arch.flags & KVM_ARM64_INCREMENT_PC;
}

static void handle_pvm_entry_hvc64(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	/* HVCs for pvms either don't return or use only one register. */
	vcpu_set_reg(shadow_vcpu, 0, vcpu_get_reg(host_vcpu, 0));
}

static void handle_pvm_entry_sys64(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	u32 esr_el2 = shadow_vcpu->arch.fault.esr_el2;
	bool is_read = (esr_el2 & ESR_ELx_SYS64_ISS_DIR_MASK) == ESR_ELx_SYS64_ISS_DIR_READ;

	shadow_vcpu->arch.flags |= host_vcpu->arch.flags &
		(KVM_ARM64_PENDING_EXCEPTION | KVM_ARM64_INCREMENT_PC);

	if (shadow_vcpu->arch.flags & KVM_ARM64_PENDING_EXCEPTION) {
		/* Exceptions caused by this should be undef exceptions. */
		u32 esr_el1 = (ESR_ELx_EC_UNKNOWN << ESR_ELx_EC_SHIFT);

		__vcpu_sys_reg(shadow_vcpu, ESR_EL1) = esr_el1;
	} else if (is_read) {
		/* r0 as transfer register between the guest and the host. */
		u64 rt_val = vcpu_get_reg(host_vcpu, 0);
		int rt = kvm_vcpu_sys_get_rt(shadow_vcpu);

		vcpu_set_reg(shadow_vcpu, rt, rt_val);
	}
}

static void handle_pvm_entry_abt(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	shadow_vcpu->arch.flags |= host_vcpu->arch.flags &
		(KVM_ARM64_PENDING_EXCEPTION | KVM_ARM64_INCREMENT_PC);

	if (shadow_vcpu->arch.flags & KVM_ARM64_PENDING_EXCEPTION) {
		/* If the host wants to inject an exception, get syndrom and fault address. */
		u32 far_el1 = kvm_vcpu_get_hfar(shadow_vcpu);
		u32 esr_el1;

		esr_el1 = ESR_ELx_EC_IABT_CUR << ESR_ELx_EC_SHIFT;
		esr_el1 |= ESR_ELx_FSC_EXTABT;

		__vcpu_sys_reg(shadow_vcpu, ESR_EL1) = esr_el1;
		__vcpu_sys_reg(shadow_vcpu, FAR_EL1) = far_el1;
	}
}

static void handle_pvm_entry_dabt(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	bool pend_exception;
	bool inc_pc;

	handle_pvm_entry_abt(host_vcpu, shadow_vcpu);

	pend_exception = shadow_vcpu->arch.flags & KVM_ARM64_PENDING_EXCEPTION;
	inc_pc = shadow_vcpu->arch.flags & KVM_ARM64_INCREMENT_PC;

	if (!pend_exception && inc_pc && !kvm_vcpu_dabt_iswrite(shadow_vcpu)) {
		/* r0 as transfer register between the guest and the host. */
		u64 rd_val = vcpu_get_reg(host_vcpu, 0);
		int rd = kvm_vcpu_dabt_get_rd(shadow_vcpu);

		vcpu_set_reg(shadow_vcpu, rd, rd_val);
	}
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
	bool is_write = (esr_el2 & ESR_ELx_SYS64_ISS_DIR_MASK) == ESR_ELx_SYS64_ISS_DIR_WRITE;

	/* r0 as transfer register between the guest and the host. */
	host_vcpu->arch.fault.esr_el2 = esr_el2 & ~ESR_ELx_SYS64_ISS_RT_MASK;

	if (is_write) {
		int rt = kvm_vcpu_sys_get_rt(shadow_vcpu);
		u64 rt_val = vcpu_get_reg(shadow_vcpu, rt);

		vcpu_set_reg(host_vcpu, 0, rt_val);
	}
}

static int get_num_hvc_args(struct kvm_vcpu *vcpu)
{
	u32 psci_fn = smccc_get_function(vcpu);

	switch (psci_fn) {
	case PSCI_0_2_FN_CPU_ON:
	case PSCI_0_2_FN64_CPU_ON:
	case PSCI_0_2_FN_CPU_SUSPEND:
	case PSCI_0_2_FN64_CPU_SUSPEND:
		return 3;
	case PSCI_0_2_FN_AFFINITY_INFO:
	case PSCI_0_2_FN64_AFFINITY_INFO:
	case PSCI_1_1_FN_SYSTEM_RESET2:
	case PSCI_1_1_FN64_SYSTEM_RESET2:
	case PSCI_1_0_FN_SYSTEM_SUSPEND:
	case PSCI_1_0_FN64_SYSTEM_SUSPEND:
		return 2;
	case PSCI_1_0_FN_PSCI_FEATURES:
	case PSCI_0_2_FN_MIGRATE:
	case PSCI_0_2_FN64_MIGRATE:
	case PSCI_1_0_FN_SET_SUSPEND_MODE:
	case ARM_SMCCC_ARCH_FEATURES_FUNC_ID:
	case ARM_SMCCC_TRNG_FEATURES:
	case ARM_SMCCC_TRNG_RND32:
	case ARM_SMCCC_TRNG_RND64:
	case ARM_SMCCC_VENDOR_HYP_KVM_PTP_FUNC_ID:
	case ARM_SMCCC_HV_PV_TIME_FEATURES:
		return 1;
	default:
		return 0;
	}

	return 0;
}

static void handle_pvm_exit_hvc64(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	int i;

	host_vcpu->arch.fault.esr_el2 = shadow_vcpu->arch.fault.esr_el2;

	/* Pass the hvc function id (r0) as well as any potential arguments. */
	for (i = 0; i < get_num_hvc_args(shadow_vcpu) + 1; i++)
		vcpu_set_reg(host_vcpu, i, vcpu_get_reg(shadow_vcpu, i));
}

static void handle_pvm_exit_abt(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	host_vcpu->arch.ctxt.regs.pstate = shadow_vcpu->arch.ctxt.regs.pstate & PSR_MODE_MASK;
	host_vcpu->arch.fault.esr_el2 = shadow_vcpu->arch.fault.esr_el2;
	host_vcpu->arch.fault.far_el2 = shadow_vcpu->arch.fault.far_el2 & FAR_MASK;
	host_vcpu->arch.fault.hpfar_el2 = shadow_vcpu->arch.fault.hpfar_el2;
	__vcpu_sys_reg(host_vcpu, SCTLR_EL1) =
		__vcpu_sys_reg(shadow_vcpu, SCTLR_EL1) & (SCTLR_ELx_EE | SCTLR_EL1_E0E);
}

static void handle_pvm_exit_dabt(struct kvm_vcpu *host_vcpu, struct kvm_vcpu *shadow_vcpu)
{
	handle_pvm_exit_abt(host_vcpu, shadow_vcpu);

	/* r0 as transfer register between the guest and the host. */
	host_vcpu->arch.fault.esr_el2 &= ~ESR_ELx_SRT_MASK;

	/* TODO: don't expose anything if !MMIO (clear ESR_EL2.ISV) */
	if (kvm_vcpu_dabt_iswrite(shadow_vcpu)) {
		int rt = kvm_vcpu_dabt_get_rd(shadow_vcpu);
		u64 rt_val = vcpu_get_reg(shadow_vcpu, rt);

		vcpu_set_reg(host_vcpu, 0, rt_val);
	}
}

static const shadow_entry_exit_handler_fn entry_shadow_handlers[] = {
	[0 ... ESR_ELx_EC_MAX]		= NULL,
	[ESR_ELx_EC_WFx]		= handle_pvm_entry_wfx,
	[ESR_ELx_EC_HVC64]		= handle_pvm_entry_hvc64,
	[ESR_ELx_EC_SYS64]		= handle_pvm_entry_sys64,
	[ESR_ELx_EC_IABT_LOW]		= handle_pvm_entry_abt,
	[ESR_ELx_EC_DABT_LOW]		= handle_pvm_entry_dabt,
};

static const shadow_entry_exit_handler_fn exit_shadow_handlers[] = {
	[0 ... ESR_ELx_EC_MAX]		= NULL,
	[ESR_ELx_EC_WFx]		= handle_pvm_exit_wfx,
	[ESR_ELx_EC_HVC64]		= handle_pvm_exit_hvc64,
	[ESR_ELx_EC_SYS64]		= handle_pvm_exit_sys64,
	[ESR_ELx_EC_IABT_LOW]		= handle_pvm_exit_abt,
	[ESR_ELx_EC_DABT_LOW]		= handle_pvm_exit_dabt,
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

static bool handle_shadow_entry(struct kvm_vcpu *shadow_vcpu)
{
	struct kvm_vcpu *host_vcpu = shadow_vcpu->arch.pkvm.host_vcpu;
	u8 esr_ec;
	shadow_entry_exit_handler_fn ec_handler;

	flush_vgic_state(host_vcpu, shadow_vcpu);

	switch (ARM_EXCEPTION_CODE(shadow_vcpu->arch.pkvm.exit_code)) {
	case ARM_EXCEPTION_IRQ:
		break;
	case ARM_EXCEPTION_TRAP:
		esr_ec = ESR_ELx_EC(kvm_vcpu_get_esr(shadow_vcpu));
		ec_handler = entry_shadow_handlers[esr_ec];

		if (ec_handler)
			ec_handler(host_vcpu, shadow_vcpu);

		break;
	default:
		return false;
	}

	return true;
}

static void handle_shadow_exit(struct kvm_vcpu *shadow_vcpu)
{
	struct kvm_vcpu *host_vcpu = shadow_vcpu->arch.pkvm.host_vcpu;
	u8 esr_ec;
	shadow_entry_exit_handler_fn ec_handler;

	sync_vgic_state(host_vcpu, shadow_vcpu);

	switch (shadow_vcpu->arch.pkvm.exit_code) {
	case ARM_EXCEPTION_IRQ:
		break;
	case ARM_EXCEPTION_TRAP:
		esr_ec = ESR_ELx_EC(kvm_vcpu_get_esr(shadow_vcpu));
		ec_handler = exit_shadow_handlers[esr_ec];

		if (ec_handler)
			ec_handler(host_vcpu, shadow_vcpu);

		break;
	default:
		break;
	}
}

static struct kvm_vcpu *get_shadow_vcpu(struct kvm_vcpu *host_vcpu)
{
	struct kvm_vcpu *shadow_vcpu;

	host_vcpu = kern_hyp_va(host_vcpu);
	shadow_vcpu = hyp_get_shadow_vcpu(host_vcpu);

	if (shadow_vcpu) {
		if (!handle_shadow_entry(shadow_vcpu))
			return NULL;

		shadow_vcpu->arch.pkvm.exit_code = 0;
		return shadow_vcpu;
	}

	return host_vcpu;
}

static void put_shadow_vcpu(struct kvm_vcpu *shadow_vcpu, int exit_code)
{
	shadow_vcpu->arch.pkvm.exit_code = exit_code;
	handle_shadow_exit(shadow_vcpu);
}

static void handle___kvm_vcpu_run(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(struct kvm_vcpu *, vcpu, host_ctxt, 1);
	struct kvm_vcpu *shadow_vcpu;
	int ret;

	shadow_vcpu = get_shadow_vcpu(vcpu);
	ret = __kvm_vcpu_run(shadow_vcpu);

	if (shadow_vcpu != kern_hyp_va(vcpu))
		put_shadow_vcpu(shadow_vcpu, ret);

	cpu_reg(host_ctxt, 1) =  ret;
}

static void handle___kvm_adjust_pc(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(struct kvm_vcpu *, vcpu, host_ctxt, 1);

	/*
	 * This get_shadow_vcpu() shouldn't exist, as we would never
	 * commit a pending update before returning to userspace, and
	 * this is an actual attack vector (it leaves EL1 in full
	 * control of PC).
	 */
	vcpu = get_shadow_vcpu(vcpu);

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

static struct vgic_v3_cpu_if *get_shadow_vgic_v3_cpu_if(struct vgic_v3_cpu_if *cpu_if)
{
	struct kvm_vcpu *vcpu, *shadow_vcpu;

	vcpu = container_of(cpu_if, struct kvm_vcpu, arch.vgic_cpu.vgic_v3);
	shadow_vcpu = hyp_get_shadow_vcpu(vcpu);
	if (!shadow_vcpu)
		return cpu_if;
	return &shadow_vcpu->arch.vgic_cpu.vgic_v3;
}

static void handle___vgic_v3_save_vmcr_aprs(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(struct vgic_v3_cpu_if *, cpu_if, host_ctxt, 1);
	struct vgic_v3_cpu_if *shadow_cpu_if;

	cpu_if = kern_hyp_va(cpu_if);
	shadow_cpu_if = get_shadow_vgic_v3_cpu_if(cpu_if);

	__vgic_v3_save_vmcr_aprs(shadow_cpu_if);

	if (cpu_if != shadow_cpu_if) {
		int i;

		cpu_if->vgic_vmcr = shadow_cpu_if->vgic_vmcr;
		for (i = 0; i < ARRAY_SIZE(cpu_if->vgic_ap0r); i++) {
			cpu_if->vgic_ap0r[i] = shadow_cpu_if->vgic_ap0r[i];
			cpu_if->vgic_ap1r[i] = shadow_cpu_if->vgic_ap1r[i];
		}
	}
}

static void handle___vgic_v3_restore_vmcr_aprs(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(struct vgic_v3_cpu_if *, cpu_if, host_ctxt, 1);
	struct vgic_v3_cpu_if *shadow_cpu_if;

	cpu_if = kern_hyp_va(cpu_if);
	shadow_cpu_if = get_shadow_vgic_v3_cpu_if(cpu_if);

	if (cpu_if != shadow_cpu_if) {
		int i;

		shadow_cpu_if->vgic_vmcr = cpu_if->vgic_vmcr;
		/* Should be a one-off */
		shadow_cpu_if->vgic_sre = (ICC_SRE_EL1_DIB |
					   ICC_SRE_EL1_DFB |
					   ICC_SRE_EL1_SRE);
		for (i = 0; i < ARRAY_SIZE(cpu_if->vgic_ap0r); i++) {
			shadow_cpu_if->vgic_ap0r[i] = cpu_if->vgic_ap0r[i];
			shadow_cpu_if->vgic_ap1r[i] = cpu_if->vgic_ap1r[i];
		}
	}

	__vgic_v3_restore_vmcr_aprs(shadow_cpu_if);
}

static void handle___pkvm_init(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(phys_addr_t, phys, host_ctxt, 1);
	DECLARE_REG(unsigned long, size, host_ctxt, 2);
	DECLARE_REG(unsigned long, nr_cpus, host_ctxt, 3);
	DECLARE_REG(unsigned long *, per_cpu_base, host_ctxt, 4);
	DECLARE_REG(u32, hyp_va_bits, host_ctxt, 5);
	DECLARE_REG(enum kvm_iommu_driver, iommu_driver, host_ctxt, 6);

	/*
	 * __pkvm_init() will return only if an error occurred, otherwise it
	 * will tail-call in __pkvm_init_finalise() which will have to deal
	 * with the host context directly.
	 */
	cpu_reg(host_ctxt, 1) = __pkvm_init(phys, size, nr_cpus, per_cpu_base,
					    hyp_va_bits, iommu_driver);
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

	cpu_reg(host_ctxt, 1) = __pkvm_init_shadow(host_kvm, host_shadow_va,
						       shadow_size);
}

static void handle___pkvm_teardown_shadow(struct kvm_cpu_context *host_ctxt)
{
	DECLARE_REG(struct kvm *, host_kvm, host_ctxt, 1);

	cpu_reg(host_ctxt, 1) = __pkvm_teardown_shadow(host_kvm);
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
	HANDLE_FUNC(__pkvm_prot_finalize),

	HANDLE_FUNC(__pkvm_host_share_hyp),
	HANDLE_FUNC(__pkvm_host_unshare_hyp),
	HANDLE_FUNC(__kvm_adjust_pc),
	HANDLE_FUNC(__kvm_vcpu_run),
	HANDLE_FUNC(__kvm_flush_vm_context),
	HANDLE_FUNC(__kvm_tlb_flush_vmid_ipa),
	HANDLE_FUNC(__kvm_tlb_flush_vmid),
	HANDLE_FUNC(__kvm_flush_cpu_context),
	HANDLE_FUNC(__kvm_timer_set_cntvoff),
	HANDLE_FUNC(__vgic_v3_save_vmcr_aprs),
	HANDLE_FUNC(__vgic_v3_restore_vmcr_aprs),
	HANDLE_FUNC(__pkvm_init_shadow),
	HANDLE_FUNC(__pkvm_teardown_shadow),
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
	if (!handled && kvm_iommu_ops.host_smc_handler)
		handled = kvm_iommu_ops.host_smc_handler(host_ctxt);
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
	case ESR_ELx_EC_SVE:
		sysreg_clear_set(cptr_el2, CPTR_EL2_TZ, 0);
		isb();
		sve_cond_update_zcr_vq(ZCR_ELx_LEN_MASK, SYS_ZCR_EL2);
		break;
	case ESR_ELx_EC_IABT_LOW:
	case ESR_ELx_EC_DABT_LOW:
		handle_host_mem_abort(host_ctxt);
		break;
	default:
		BUG();
	}
}
