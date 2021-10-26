// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 Google LLC
 * Author: Fuad Tabba <tabba@google.com>
 */

#include <asm/kvm_asm.h>
#include <asm/kvm_emulate.h>
#include <asm/kvm_mmu.h>
#include <asm/memory.h>

#include <linux/kvm_host.h>
#include <linux/mm.h>

#include <nvhe/mem_protect.h>
#include <nvhe/mm.h>
#include <nvhe/pkvm.h>
#include <nvhe/trap_handler.h>

/*
 * Set trap register values based on features in ID_AA64PFR0.
 */
static void pvm_init_traps_aa64pfr0(struct kvm_vcpu *vcpu)
{
	const u64 feature_ids = pvm_read_id_reg(vcpu, SYS_ID_AA64PFR0_EL1);
	u64 hcr_set = HCR_RW;
	u64 hcr_clear = 0;
	u64 cptr_set = 0;

	/* Protected KVM does not support AArch32 guests. */
	BUILD_BUG_ON(FIELD_GET(ARM64_FEATURE_MASK(ID_AA64PFR0_EL0),
		PVM_ID_AA64PFR0_RESTRICT_UNSIGNED) != ID_AA64PFR0_ELx_64BIT_ONLY);
	BUILD_BUG_ON(FIELD_GET(ARM64_FEATURE_MASK(ID_AA64PFR0_EL1),
		PVM_ID_AA64PFR0_RESTRICT_UNSIGNED) != ID_AA64PFR0_ELx_64BIT_ONLY);

	/*
	 * Linux guests assume support for floating-point and Advanced SIMD. Do
	 * not change the trapping behavior for these from the KVM default.
	 */
	BUILD_BUG_ON(!FIELD_GET(ARM64_FEATURE_MASK(ID_AA64PFR0_FP),
				PVM_ID_AA64PFR0_ALLOW));
	BUILD_BUG_ON(!FIELD_GET(ARM64_FEATURE_MASK(ID_AA64PFR0_ASIMD),
				PVM_ID_AA64PFR0_ALLOW));

	/* Trap RAS unless all current versions are supported */
	if (FIELD_GET(ARM64_FEATURE_MASK(ID_AA64PFR0_RAS), feature_ids) <
	    ID_AA64PFR0_RAS_V1P1) {
		hcr_set |= HCR_TERR | HCR_TEA;
		hcr_clear |= HCR_FIEN;
	}

	/* Trap AMU */
	if (!FIELD_GET(ARM64_FEATURE_MASK(ID_AA64PFR0_AMU), feature_ids)) {
		hcr_clear |= HCR_AMVOFFEN;
		cptr_set |= CPTR_EL2_TAM;
	}

	/* Trap SVE */
	if (!FIELD_GET(ARM64_FEATURE_MASK(ID_AA64PFR0_SVE), feature_ids))
		cptr_set |= CPTR_EL2_TZ;

	vcpu->arch.hcr_el2 |= hcr_set;
	vcpu->arch.hcr_el2 &= ~hcr_clear;
	vcpu->arch.cptr_el2 |= cptr_set;
}

/*
 * Set trap register values based on features in ID_AA64PFR1.
 */
static void pvm_init_traps_aa64pfr1(struct kvm_vcpu *vcpu)
{
	const u64 feature_ids = pvm_read_id_reg(vcpu, SYS_ID_AA64PFR1_EL1);
	u64 hcr_set = 0;
	u64 hcr_clear = 0;

	/* Memory Tagging: Trap and Treat as Untagged if not supported. */
	if (!FIELD_GET(ARM64_FEATURE_MASK(ID_AA64PFR1_MTE), feature_ids)) {
		hcr_set |= HCR_TID5;
		hcr_clear |= HCR_DCT | HCR_ATA;
	}

	vcpu->arch.hcr_el2 |= hcr_set;
	vcpu->arch.hcr_el2 &= ~hcr_clear;
}

/*
 * Set trap register values based on features in ID_AA64DFR0.
 */
static void pvm_init_traps_aa64dfr0(struct kvm_vcpu *vcpu)
{
	const u64 feature_ids = pvm_read_id_reg(vcpu, SYS_ID_AA64DFR0_EL1);
	u64 mdcr_set = 0;
	u64 mdcr_clear = 0;
	u64 cptr_set = 0;

	/* Trap/constrain PMU */
	if (!FIELD_GET(ARM64_FEATURE_MASK(ID_AA64DFR0_PMUVER), feature_ids)) {
		mdcr_set |= MDCR_EL2_TPM | MDCR_EL2_TPMCR;
		mdcr_clear |= MDCR_EL2_HPME | MDCR_EL2_MTPME |
			      MDCR_EL2_HPMN_MASK;
	}

	/* Trap Debug */
	if (!FIELD_GET(ARM64_FEATURE_MASK(ID_AA64DFR0_DEBUGVER), feature_ids))
		mdcr_set |= MDCR_EL2_TDRA | MDCR_EL2_TDA;

	/* Trap OS Double Lock */
	if (!FIELD_GET(ARM64_FEATURE_MASK(ID_AA64DFR0_DOUBLELOCK), feature_ids))
		mdcr_set |= MDCR_EL2_TDOSA;

	/* Trap SPE */
	if (!FIELD_GET(ARM64_FEATURE_MASK(ID_AA64DFR0_PMSVER), feature_ids)) {
		mdcr_set |= MDCR_EL2_TPMS;
		mdcr_clear |= MDCR_EL2_E2PB_MASK << MDCR_EL2_E2PB_SHIFT;
	}

	/* Trap Trace Filter */
	if (!FIELD_GET(ARM64_FEATURE_MASK(ID_AA64DFR0_TRACE_FILT), feature_ids))
		mdcr_set |= MDCR_EL2_TTRF;

	/* Trap Trace */
	if (!FIELD_GET(ARM64_FEATURE_MASK(ID_AA64DFR0_TRACEVER), feature_ids))
		cptr_set |= CPTR_EL2_TTA;

	vcpu->arch.mdcr_el2 |= mdcr_set;
	vcpu->arch.mdcr_el2 &= ~mdcr_clear;
	vcpu->arch.cptr_el2 |= cptr_set;
}

/*
 * Set trap register values based on features in ID_AA64MMFR0.
 */
static void pvm_init_traps_aa64mmfr0(struct kvm_vcpu *vcpu)
{
	const u64 feature_ids = pvm_read_id_reg(vcpu, SYS_ID_AA64MMFR0_EL1);
	u64 mdcr_set = 0;

	/* Trap Debug Communications Channel registers */
	if (!FIELD_GET(ARM64_FEATURE_MASK(ID_AA64MMFR0_FGT), feature_ids))
		mdcr_set |= MDCR_EL2_TDCC;

	vcpu->arch.mdcr_el2 |= mdcr_set;
}

/*
 * Set trap register values based on features in ID_AA64MMFR1.
 */
static void pvm_init_traps_aa64mmfr1(struct kvm_vcpu *vcpu)
{
	const u64 feature_ids = pvm_read_id_reg(vcpu, SYS_ID_AA64MMFR1_EL1);
	u64 hcr_set = 0;

	/* Trap LOR */
	if (!FIELD_GET(ARM64_FEATURE_MASK(ID_AA64MMFR1_LOR), feature_ids))
		hcr_set |= HCR_TLOR;

	vcpu->arch.hcr_el2 |= hcr_set;
}

/*
 * Set baseline trap register values.
 */
static void pvm_init_trap_regs(struct kvm_vcpu *vcpu)
{
	vcpu->arch.cptr_el2 = CPTR_EL2_DEFAULT;
	vcpu->arch.mdcr_el2 = 0;

	/*
	 * Always trap:
	 * - Feature id registers: to control features exposed to guests
	 * - Implementation-defined features
	 */
	vcpu->arch.hcr_el2 = HCR_GUEST_FLAGS |
			     HCR_TID3 | HCR_TACR | HCR_TIDCP | HCR_TID1;

	if (cpus_have_const_cap(ARM64_HAS_RAS_EXTN)) {
		/* route synchronous external abort exceptions to EL2 */
		vcpu->arch.hcr_el2 |= HCR_TEA;
		/* trap error record accesses */
		vcpu->arch.hcr_el2 |= HCR_TERR;
	}

	if (cpus_have_const_cap(ARM64_HAS_STAGE2_FWB))
		vcpu->arch.hcr_el2 |= HCR_FWB;

	if (cpus_have_const_cap(ARM64_MISMATCHED_CACHE_TYPE))
		vcpu->arch.hcr_el2 |= HCR_TID2;
}

/*
 * Initialize trap register values for protected VMs.
 */
static void pkvm_vcpu_init_traps(struct kvm_vcpu *vcpu)
{
	pvm_init_trap_regs(vcpu);
	pvm_init_traps_aa64pfr0(vcpu);
	pvm_init_traps_aa64pfr1(vcpu);
	pvm_init_traps_aa64dfr0(vcpu);
	pvm_init_traps_aa64mmfr0(vcpu);
	pvm_init_traps_aa64mmfr1(vcpu);
}

/*
 * Start the shadow table handle at the offset defined instead of at 0.
 * Mainly for sanity checking and debugging.
 */
#define HANDLE_OFFSET 0x1000

static int shadow_handle_to_index(int shadow_handle)
{
	return shadow_handle - HANDLE_OFFSET;
}

static int index_to_shadow_handle(int index)
{
	return index + HANDLE_OFFSET;
}

extern unsigned long hyp_nr_cpus;

/*
 * Spinlock for protecting the shadow table related state.
 * Protects writes to shadow_table, num_shadow_entries, and next_shadow_alloc,
 * as well as reads and writes to last_shadow_vcpu_lookup.
 */
DEFINE_HYP_SPINLOCK(shadow_lock);

/*
 * The table of shadow entries for protected VMs in hyp.
 * Allocated at hyp initialization and setup.
 */
struct kvm_shadow_vm **shadow_table;

/* Current number of vms in the shadow table. */
int num_shadow_entries;

/* The next entry index to try to allocate from. */
int next_shadow_alloc;

/*
 * Return the shadow vm corresponding to the handle.
 */
static struct kvm_shadow_vm *find_shadow_by_handle(int shadow_handle)
{
	int shadow_index = shadow_handle_to_index(shadow_handle);

	if (unlikely(shadow_index < 0 || shadow_index >= KVM_MAX_PVMS))
		return NULL;

	return shadow_table[shadow_index];
}

/*
 * Returns the hyp shadow vcpu for the corresponding host vcpu,
 * or NULL if it fails.
 */
struct kvm_vcpu *hyp_get_shadow_vcpu(const struct kvm_vcpu *vcpu)
{
	struct shadow_vcpu_state *shadow_vcpu_state;
	struct kvm_shadow_vm *vm;
	int vcpu_idx;
	int shadow_handle;

	shadow_handle = vcpu->arch.pkvm.shadow_handle;
	vm = find_shadow_by_handle(shadow_handle);
	vcpu_idx = vcpu->vcpu_idx;

	if (unlikely(!vm || vcpu_idx < 0 || vcpu_idx >= vm->created_vcpus))
		return NULL;

	shadow_vcpu_state = &vm->shadow_vcpus[vcpu_idx];

	return &shadow_vcpu_state->vcpu;
}

/* Copy the supported features for the vcpu from the host. */
static void copy_features(struct kvm_vcpu *shadow_vcpu, struct kvm_vcpu *host_vcpu)
{
	DECLARE_BITMAP(allowed_features, KVM_VCPU_MAX_FEATURES);

	bitmap_zero(allowed_features, KVM_VCPU_MAX_FEATURES);

	/*
	 * Always allowed:
	 * - CPU starting in poweroff state
	 * - PSCI v0.2
	 */
	set_bit(KVM_ARM_VCPU_POWER_OFF, allowed_features);
	set_bit(KVM_ARM_VCPU_PSCI_0_2, allowed_features);

	/*
	 * Check if remaining features are allowed:
	 * - Performance Monitoring
	 * - Scalable Vectors
	 * - Pointer Authentication
	 */
	if (FIELD_GET(ARM64_FEATURE_MASK(ID_AA64DFR0_PMUVER), PVM_ID_AA64DFR0_ALLOW))
	        set_bit(KVM_ARM_VCPU_PMU_V3, allowed_features);

	if (FIELD_GET(ARM64_FEATURE_MASK(ID_AA64PFR0_SVE), PVM_ID_AA64PFR0_ALLOW))
	        set_bit(KVM_ARM_VCPU_SVE, allowed_features);

	if (FIELD_GET(ARM64_FEATURE_MASK(ID_AA64ISAR1_API), PVM_ID_AA64ISAR1_ALLOW) &&
	    FIELD_GET(ARM64_FEATURE_MASK(ID_AA64ISAR1_APA), PVM_ID_AA64ISAR1_ALLOW))
	        set_bit(KVM_ARM_VCPU_PTRAUTH_ADDRESS, allowed_features);

	if (FIELD_GET(ARM64_FEATURE_MASK(ID_AA64ISAR1_GPI), PVM_ID_AA64ISAR1_ALLOW) &&
	    FIELD_GET(ARM64_FEATURE_MASK(ID_AA64ISAR1_GPA), PVM_ID_AA64ISAR1_ALLOW))
	        set_bit(KVM_ARM_VCPU_PTRAUTH_GENERIC, allowed_features);

	bitmap_and(shadow_vcpu->arch.features, host_vcpu->arch.features,
		allowed_features, KVM_VCPU_MAX_FEATURES);
}

static void unpin_host_vcpus(struct kvm_shadow_vm *vm)
{
	int i;

	for (i = 0; i < vm->created_vcpus; i++) {
		struct kvm_vcpu *vcpu = vm->vcpus[i]->arch.pkvm.host_vcpu;
		hyp_unpin_shared_mem(vcpu, vcpu + 1);
	}
}

static int init_shadow_structs(struct kvm *kvm, struct kvm_shadow_vm *vm, int nr_vcpus)
{
	int i;
	int ret;

	/* TODO: initialize the protected MMU. For now, use the host's. */
	vm->mmu = &kvm->arch.mmu;
	vm->host_kvm = kvm;
	vm->created_vcpus = 0;

	for (i = 0; i < nr_vcpus; i++) {
		struct kvm_vcpu *host_vcpu = kern_hyp_va(kvm->vcpus[i]);
		struct shadow_vcpu_state *shadow_state = &vm->shadow_vcpus[i];
		struct kvm_vcpu *shadow_vcpu = &shadow_state->vcpu;

		ret = hyp_pin_shared_mem(host_vcpu, host_vcpu + 1);
		if (ret)
			return -EBUSY;

		vm->created_vcpus++;

		shadow_vcpu->kvm = kvm;
		shadow_vcpu->vcpu_id = host_vcpu->vcpu_id;
		shadow_vcpu->vcpu_idx = i;

		vcpu_gp_regs(shadow_vcpu)->pstate = VCPU_RESET_PSTATE_EL1;
		*vcpu_pc(shadow_vcpu) = *vcpu_pc(host_vcpu);
		vcpu_set_reg(shadow_vcpu, 0, vcpu_get_reg(host_vcpu, 0));

		kvm_reset_pvm_sys_regs(shadow_vcpu);

		copy_features(shadow_vcpu, host_vcpu);
		pkvm_vcpu_init_traps(shadow_vcpu);

		vm->vcpus[i] = shadow_vcpu;
		shadow_state->vm = vm;

		/* TODO - use &vm->arch.mmu when setup properly */
		shadow_vcpu->arch.hw_mmu = host_vcpu->arch.hw_mmu;
		shadow_vcpu->arch.pkvm.shadow_handle = vm->shadow_handle;
		shadow_vcpu->arch.pkvm.host_vcpu = host_vcpu;
		shadow_vcpu->arch.pkvm.shadow_vm = vm;
	}

	return 0;
}

static bool exists_shadow(struct kvm *host_kvm)
{
	int i;
	int num_checked = 0;

	for (i = 0; i < KVM_MAX_PVMS && num_checked < num_shadow_entries; i++) {
		if (!shadow_table[i])
			continue;

		if (unlikely(shadow_table[i]->host_kvm == host_kvm))
			return true;

		num_checked++;
	}

	return false;
}

/*
 * Allocate a shadow table entry and insert a pointer to the shadow vm.
 *
 * Return a unique handle to the protected VM on success,
 * negative error code on failure.
 */
static int __insert_shadow_table(struct kvm *kvm, struct kvm_shadow_vm *vm,
			         size_t shadow_size)
{
	int shadow_handle;

	if (unlikely(num_shadow_entries >= KVM_MAX_PVMS))
		return -ENOMEM;

	/*
	 * Initializing protected state might have failed, yet a malicious host
	 * could trigger this function. Thus, ensure that shadow_table exists.
	 */
	if (unlikely(!shadow_table))
		return -EINVAL;

	/* Check that a shadow hasn't been created before for this host KVM. */
	if (unlikely(exists_shadow(kvm)))
		return -EEXIST;

	/* Find the next free entry in the shadow table. */
	while (shadow_table[next_shadow_alloc])
		next_shadow_alloc = (next_shadow_alloc + 1) % KVM_MAX_PVMS;
	shadow_handle = index_to_shadow_handle(next_shadow_alloc);

	vm->shadow_handle = shadow_handle;
	vm->shadow_area_size = shadow_size;

	shadow_table[next_shadow_alloc] = vm;
	next_shadow_alloc = (next_shadow_alloc + 1) % KVM_MAX_PVMS;
	num_shadow_entries++;

	return shadow_handle;
}

static int insert_shadow_table(struct kvm *kvm, struct kvm_shadow_vm *vm,
			       size_t shadow_size)
{
	int ret;

	hyp_spin_lock(&shadow_lock);
	ret = __insert_shadow_table(kvm, vm, shadow_size);
	hyp_spin_unlock(&shadow_lock);

	return ret;
}

/*
 * Deallocate and remove the shadow table entry corresponding to the handle.
 */
static void __remove_shadow_table(int shadow_handle)
{
	shadow_table[shadow_handle_to_index(shadow_handle)] = NULL;
	num_shadow_entries--;
}

static void remove_shadow_table(int shadow_handle)
{
	hyp_spin_lock(&shadow_lock);
	__remove_shadow_table(shadow_handle);
	hyp_spin_unlock(&shadow_lock);
}

static size_t pkvm_get_shadow_size(int num_vcpus)
{
	/* Shadow space for the vm struct and all of its vcpu states. */
	return sizeof(struct kvm_shadow_vm) +
	       sizeof(struct shadow_vcpu_state) * num_vcpus;
}

/*
 * Check whether the size of the area donated by the host is sufficient for
 * the shadow structues required for nr_vcpus as well as the shadow vm.
 */
static int check_shadow_size(int nr_vcpus, size_t shadow_size)
{
	if (nr_vcpus < 1 || nr_vcpus > KVM_MAX_VCPUS)
		return -EINVAL;

	/*
	 * Shadow size is rounded up when allocated and donated by the host,
	 * so it's likely to be larger than the sum of the struct sizes.
	 */
	if (shadow_size < pkvm_get_shadow_size(nr_vcpus))
		return -EINVAL;

	return 0;
}

/*
 * Initialize the shadow copy of the protected VM state using the memory
 * donated by the host.
 *
 * Unmaps the donated memory from the host at stage 2.
 *
 * Return a unique handle to the protected VM on success,
 * negative error code on failure.
 */
int __pkvm_init_shadow(struct kvm *kvm,
		       void *shadow_va,
		       size_t shadow_size)
{
	struct kvm_shadow_vm *vm = kern_hyp_va(shadow_va);
	phys_addr_t shadow_pa = hyp_virt_to_phys(vm);
	u64 pfn = hyp_phys_to_pfn(shadow_pa);
	u64 nr_pages = shadow_size >> PAGE_SHIFT;
	int nr_vcpus = 0;
	int ret = 0;

	kvm = kern_hyp_va(kvm);

	ret = hyp_pin_shared_mem(kvm, kvm + 1);
	if (ret)
		return ret;

	/* Ensure the host has donated enough memory for the shadow structs. */
	nr_vcpus = kvm->created_vcpus;
	ret = check_shadow_size(nr_vcpus, shadow_size);
	if (ret)
		goto err;

	ret = __pkvm_host_donate_hyp(pfn, nr_pages);
	if (ret)
		goto err;

	/* Ensure we're working with a clean slate. */
	memset(vm, 0, shadow_size);

	/* Add the entry to the shadow table. */
	ret = insert_shadow_table(kvm, vm, shadow_size);
	if (ret < 0)
		goto err_clear_shadow;

	ret = init_shadow_structs(kvm, vm, nr_vcpus);
	if (ret < 0)
		goto err_clear_shadow;

	return vm->shadow_handle;

err_clear_shadow:
	unpin_host_vcpus(vm);
	/* Clear the donated shadow memory on failure to avoid data leaks. */
	memset(vm, 0, shadow_size);
	WARN_ON(__pkvm_hyp_donate_host(pfn, nr_pages));

err:
	hyp_unpin_shared_mem(kvm, kvm + 1);
	return ret;
}

int __pkvm_teardown_shadow(struct kvm *kvm)
{
	struct kvm_shadow_vm *vm;
	size_t shadow_size;
	int shadow_handle;
	u64 pfn;
	u64 nr_pages;

	kvm = kern_hyp_va(kvm);

	shadow_handle = kvm->arch.pkvm.shadow_handle;

	/* Lookup then remove entry from the shadow table. */
	vm = find_shadow_by_handle(shadow_handle);
	if (!vm)
		return -ENOENT;

	shadow_size = vm->shadow_area_size;

	unpin_host_vcpus(vm);
	hyp_unpin_shared_mem(vm->host_kvm, vm->host_kvm + 1);
	remove_shadow_table(shadow_handle);

	/* Clear the shadow memory since hyp is releasing it back to host. */
	memset(vm, 0, shadow_size);

	pfn = hyp_phys_to_pfn(__hyp_pa(vm));
	nr_pages = shadow_size >> PAGE_SHIFT;
	WARN_ON(__pkvm_hyp_donate_host(pfn, nr_pages));
	return 0;
}
