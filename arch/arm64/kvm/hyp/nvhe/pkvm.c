// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2021 Google LLC
 * Author: Fuad Tabba <tabba@google.com>
 */

#include <asm/kvm_arm.h>
#include <asm/kvm_asm.h>
#include <asm/kvm_host.h>
#include <asm/kvm_mmu.h>
#include <asm/memory.h>

#include <linux/kvm_host.h>
#include <linux/mm.h>

#include <kvm/arm_hypercalls.h>
#include <kvm/arm_psci.h>

#include <nvhe/mem_protect.h>
#include <nvhe/mm.h>
#include <nvhe/pkvm.h>
#include <nvhe/trap_handler.h>

/* Used by icache_is_vpipt(). */
unsigned long __icache_flags;

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
 * Track the vcpu most recently loaded on each physical CPU.
 */
static DEFINE_PER_CPU(struct kvm_vcpu *, last_loaded_vcpu);

/*
 * Spinlock for protecting the shadow table related state.
 * Protects writes to shadow_table, num_shadow_entries, and next_shadow_alloc,
 * as well as reads and writes to last_shadow_vcpu_lookup.
 */
static DEFINE_HYP_SPINLOCK(shadow_lock);

/*
 * The table of shadow entries for protected VMs in hyp.
 * Allocated at hyp initialization and setup.
 */
static struct kvm_shadow_vm **shadow_table;

/* Current number of vms in the shadow table. */
static int num_shadow_entries;

/* The next entry index to try to allocate from. */
static int next_shadow_alloc;

void hyp_shadow_table_init(void *tbl)
{
	WARN_ON(shadow_table);
	shadow_table = tbl;
}

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

struct kvm_vcpu *get_shadow_vcpu(int shadow_handle, unsigned int vcpu_idx)
{
	struct kvm_vcpu *vcpu = NULL;
	struct kvm_shadow_vm *vm;
	bool flush_context = false;

	hyp_spin_lock(&shadow_lock);
	vm = find_shadow_by_handle(shadow_handle);
	if (!vm || vm->created_vcpus <= vcpu_idx)
		goto unlock;
	vcpu = &vm->shadow_vcpus[vcpu_idx].vcpu;

	/* Ensure vcpu isn't loaded on more than one cpu simultaneously. */
	if (unlikely(vcpu->arch.pkvm.loaded_on_cpu)) {
		vcpu = NULL;
		goto unlock;
	}

	/*
	 * Guarantee that both TLBs and I-cache are private to each vcpu.
	 * The check below is conservative and could lead to over-invalidation,
	 * because there is no need to nuke the contexts if the vcpu belongs to
	 * a different vm.
	 */
	if (vcpu != __this_cpu_read(last_loaded_vcpu)) {
		flush_context = true;
		__this_cpu_write(last_loaded_vcpu, vcpu);
	}

	vcpu->arch.pkvm.loaded_on_cpu = true;

	hyp_page_ref_inc(hyp_virt_to_page(vm));
unlock:
	hyp_spin_unlock(&shadow_lock);

	/* No need for the lock while flushing the context. */
	if (flush_context)
		__kvm_flush_cpu_context(vcpu->arch.hw_mmu);

	return vcpu;
}

void put_shadow_vcpu(struct kvm_vcpu *vcpu)
{
	struct kvm_shadow_vm *vm = vcpu->arch.pkvm.shadow_vm;

	hyp_spin_lock(&shadow_lock);
	vcpu->arch.pkvm.loaded_on_cpu = false;
	hyp_page_ref_dec(hyp_virt_to_page(vm));
	hyp_spin_unlock(&shadow_lock);
}

/* Check and copy the supported features for the vcpu from the host. */
static int copy_features(struct kvm_vcpu *shadow_vcpu, struct kvm_vcpu *host_vcpu)
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

	/*
	 * Check for system support for address/generic pointer authentication
	 * features if either are enabled.
	 */
	if ((test_bit(KVM_ARM_VCPU_PTRAUTH_ADDRESS, shadow_vcpu->arch.features) ||
	     test_bit(KVM_ARM_VCPU_PTRAUTH_GENERIC, shadow_vcpu->arch.features)) &&
	    !system_has_full_ptr_auth())
		return -EINVAL;

	return 0;
}

static void unpin_host_vcpus(struct shadow_vcpu_state *shadow_vcpus, int nr_vcpus)
{
	int i;

	for (i = 0; i < nr_vcpus; i++) {
		struct kvm_vcpu *host_vcpu = shadow_vcpus[i].vcpu.arch.pkvm.host_vcpu;
		struct kvm_vcpu *shadow_vcpu = &shadow_vcpus[i].vcpu;
		size_t sve_state_size;
		void *sve_state;

		hyp_unpin_shared_mem(host_vcpu, host_vcpu + 1);

		if (!test_bit(KVM_ARM_VCPU_SVE, shadow_vcpu->arch.features))
			continue;

		sve_state = shadow_vcpu->arch.sve_state;
		sve_state = kern_hyp_va(sve_state);
		sve_state_size = vcpu_sve_state_size(shadow_vcpu);
		hyp_unpin_shared_mem(sve_state, sve_state + sve_state_size);
	}
}

static int set_host_vcpus(struct shadow_vcpu_state *shadow_vcpus, int nr_vcpus,
			  struct kvm_vcpu **vcpu_array, size_t vcpu_array_size)
{
	int i;

	if (vcpu_array_size < sizeof(*vcpu_array) * nr_vcpus)
		return -EINVAL;

	for (i = 0; i < nr_vcpus; i++) {
		struct kvm_vcpu *host_vcpu = kern_hyp_va(vcpu_array[i]);

		if (hyp_pin_shared_mem(host_vcpu, host_vcpu + 1)) {
			unpin_host_vcpus(shadow_vcpus, i);
			return -EBUSY;
		}

		shadow_vcpus[i].vcpu.arch.pkvm.host_vcpu = host_vcpu;
	}

	return 0;
}

static int init_ptrauth(struct kvm_vcpu *shadow_vcpu)
{
	int ret = 0;
	if (test_bit(KVM_ARM_VCPU_PTRAUTH_ADDRESS, shadow_vcpu->arch.features) ||
	    test_bit(KVM_ARM_VCPU_PTRAUTH_GENERIC, shadow_vcpu->arch.features))
		ret = kvm_vcpu_enable_ptrauth(shadow_vcpu);
	return ret;
}

static int init_shadow_structs(struct kvm *kvm, struct kvm_shadow_vm *vm,
			       struct kvm_vcpu **vcpu_array, int nr_vcpus)
{
	int i;
	int ret;

	vm->host_kvm = kvm;
	vm->created_vcpus = nr_vcpus;
	vm->arch.pkvm.pvmfw_load_addr = kvm->arch.pkvm.pvmfw_load_addr;
	vm->arch.pkvm.enabled = READ_ONCE(kvm->arch.pkvm.enabled);

	for (i = 0; i < nr_vcpus; i++) {
		struct shadow_vcpu_state *shadow_state = &vm->shadow_vcpus[i];
		struct kvm_vcpu *shadow_vcpu = &shadow_state->vcpu;
		struct kvm_vcpu *host_vcpu = shadow_vcpu->arch.pkvm.host_vcpu;

		shadow_vcpu->kvm = kvm;
		shadow_vcpu->vcpu_id = host_vcpu->vcpu_id;
		shadow_vcpu->vcpu_idx = i;

		ret = copy_features(shadow_vcpu, host_vcpu);
		if (ret)
			return ret;

		ret = init_ptrauth(shadow_vcpu);
		if (ret)
			return ret;

		if (test_bit(KVM_ARM_VCPU_SVE, shadow_vcpu->arch.features)) {
			size_t sve_state_size;
			void *sve_state;

			shadow_vcpu->arch.sve_state = READ_ONCE(host_vcpu->arch.sve_state);
			shadow_vcpu->arch.sve_max_vl = READ_ONCE(host_vcpu->arch.sve_max_vl);

			sve_state = kern_hyp_va(shadow_vcpu->arch.sve_state);
			sve_state_size = vcpu_sve_state_size(shadow_vcpu);

			if (!shadow_vcpu->arch.sve_state || !sve_state_size ||
			    hyp_pin_shared_mem(sve_state,
					       sve_state + sve_state_size)) {
				clear_bit(KVM_ARM_VCPU_SVE,
					  shadow_vcpu->arch.features);
				shadow_vcpu->arch.sve_state = NULL;
				shadow_vcpu->arch.sve_max_vl = 0;
				return -EINVAL;
			}
		}

		if (vm->arch.pkvm.enabled)
			pkvm_vcpu_init_traps(shadow_vcpu);
		kvm_reset_pvm_sys_regs(shadow_vcpu);

		vm->vcpus[i] = shadow_vcpu;
		shadow_state->vm = vm;

		shadow_vcpu->arch.hw_mmu = &vm->arch.mmu;
		shadow_vcpu->arch.pkvm.shadow_vm = vm;
		shadow_vcpu->arch.power_off = true;

		if (test_bit(KVM_ARM_VCPU_POWER_OFF, shadow_vcpu->arch.features)) {
			shadow_vcpu->arch.pkvm.power_state = PSCI_0_2_AFFINITY_LEVEL_OFF;
		} else if (pvm_has_pvmfw(vm)) {
			if (vm->pvmfw_entry_vcpu)
				return -EINVAL;

			vm->pvmfw_entry_vcpu = shadow_vcpu;
			shadow_vcpu->arch.reset_state.reset = true;
			shadow_vcpu->arch.pkvm.power_state = PSCI_0_2_AFFINITY_LEVEL_ON_PENDING;
		} else {
			struct vcpu_reset_state *reset_state = &shadow_vcpu->arch.reset_state;

			reset_state->pc = *vcpu_pc(host_vcpu);
			reset_state->r0 = vcpu_get_reg(host_vcpu, 0);
			reset_state->reset = true;
			shadow_vcpu->arch.pkvm.power_state = PSCI_0_2_AFFINITY_LEVEL_ON_PENDING;
		}
	}

	return 0;
}

static bool __exists_shadow(struct kvm *host_kvm)
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
static int insert_shadow_table(struct kvm *kvm, struct kvm_shadow_vm *vm,
			       size_t shadow_size)
{
	struct kvm_s2_mmu *mmu = &vm->arch.mmu;
	int shadow_handle;
	int vmid;

	hyp_assert_lock_held(&shadow_lock);

	if (unlikely(num_shadow_entries >= KVM_MAX_PVMS))
		return -ENOMEM;

	/*
	 * Initializing protected state might have failed, yet a malicious host
	 * could trigger this function. Thus, ensure that shadow_table exists.
	 */
	if (unlikely(!shadow_table))
		return -EINVAL;

	/* Check that a shadow hasn't been created before for this host KVM. */
	if (unlikely(__exists_shadow(kvm)))
		return -EEXIST;

	/* Find the next free entry in the shadow table. */
	while (shadow_table[next_shadow_alloc])
		next_shadow_alloc = (next_shadow_alloc + 1) % KVM_MAX_PVMS;
	shadow_handle = index_to_shadow_handle(next_shadow_alloc);

	vm->shadow_handle = shadow_handle;
	vm->shadow_area_size = shadow_size;

	/* VMID 0 is reserved for the host */
	vmid = next_shadow_alloc + 1;
	if (vmid > 0xff)
		return -ENOMEM;

	mmu->vmid.vmid = vmid;
	mmu->vmid.vmid_gen = 0;
	mmu->arch = &vm->arch;
	mmu->pgt = &vm->pgt;

	shadow_table[next_shadow_alloc] = vm;
	next_shadow_alloc = (next_shadow_alloc + 1) % KVM_MAX_PVMS;
	num_shadow_entries++;

	return shadow_handle;
}

/*
 * Deallocate and remove the shadow table entry corresponding to the handle.
 */
static void remove_shadow_table(int shadow_handle)
{
	hyp_assert_lock_held(&shadow_lock);
	shadow_table[shadow_handle_to_index(shadow_handle)] = NULL;
	num_shadow_entries--;
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

static void drain_shadow_vcpus(struct shadow_vcpu_state *shadow_vcpus,
			       unsigned int nr_vcpus,
			       struct kvm_hyp_memcache *mc)
{
	int i;

	for (i = 0; i < nr_vcpus; i++) {
		struct kvm_vcpu *shadow_vcpu = &shadow_vcpus[i].vcpu;
		struct kvm_hyp_memcache *vcpu_mc = &shadow_vcpu->arch.pkvm_memcache;
		void *addr;

		while (vcpu_mc->nr_pages) {
			addr = pop_hyp_memcache(vcpu_mc, hyp_phys_to_virt);
			push_hyp_memcache(mc, addr, hyp_virt_to_phys);
			WARN_ON(__pkvm_hyp_donate_host(hyp_virt_to_pfn(addr), 1));
		}
	}
}

/*
 * Initialize the shadow copy of the protected VM state using the memory
 * donated by the host.
 *
 * Unmaps the donated memory from the host at stage 2.
 *
 * kvm: A pointer to the host's struct kvm (host va).
 * shadow_va: The host va of the area being donated for the shadow state.
 * 	      Must be page aligned.
 * shadow_size: The size of the area being donated for the shadow state.
 * 		Must be a multiple of the page size.
 * pgd: The host va of the area being donated for the stage-2 PGD for the VM.
 * 	Must be page aligned. Its size is implied by the VM's VTCR.
 * Note: An array to the host KVM VCPUs (host VA) is passed via the pgd, as to
 * 	 not to be dependent on how the VCPU's are layed out in struct kvm.
 *
 * Return a unique handle to the protected VM on success,
 * negative error code on failure.
 */
int __pkvm_init_shadow(struct kvm *kvm,
		       void *shadow_va,
		       size_t shadow_size,
		       void *pgd)
{
	struct kvm_shadow_vm *vm = kern_hyp_va(shadow_va);
	phys_addr_t shadow_pa = hyp_virt_to_phys(vm);
	u64 pfn = hyp_phys_to_pfn(shadow_pa);
	u64 nr_shadow_pages = shadow_size >> PAGE_SHIFT;
	u64 nr_pgd_pages;
	size_t pgd_size;
	int nr_vcpus = 0;
	int ret = 0;

	/* Check that the donated memory is aligned to page boundaries. */
	if (!PAGE_ALIGNED(shadow_va) ||
	    !PAGE_ALIGNED(shadow_size) ||
	    !PAGE_ALIGNED(pgd))
		return -EINVAL;

	kvm = kern_hyp_va(kvm);
	pgd = kern_hyp_va(pgd);

	ret = hyp_pin_shared_mem(kvm, kvm + 1);
	if (ret)
		return ret;

	/* Ensure the host has donated enough memory for the shadow structs. */
	nr_vcpus = kvm->created_vcpus;
	ret = check_shadow_size(nr_vcpus, shadow_size);
	if (ret)
		goto err;

	ret = __pkvm_host_donate_hyp(pfn, nr_shadow_pages);
	if (ret)
		goto err;

	/* Ensure we're working with a clean slate. */
	memset(vm, 0, shadow_size);

	vm->arch.vtcr = host_kvm.arch.vtcr;
	pgd_size = kvm_pgtable_stage2_pgd_size(host_kvm.arch.vtcr);
	nr_pgd_pages = pgd_size >> PAGE_SHIFT;
	ret = __pkvm_host_donate_hyp(hyp_virt_to_pfn(pgd), nr_pgd_pages);
	if (ret)
		goto err_remove_mappings;

	ret = set_host_vcpus(vm->shadow_vcpus, nr_vcpus, pgd, pgd_size);
	if (ret)
		goto err_remove_pgd;

	ret = init_shadow_structs(kvm, vm, pgd, nr_vcpus);
	if (ret < 0)
		goto err_unpin_host_vcpus;

	/* Add the entry to the shadow table. */
	hyp_spin_lock(&shadow_lock);
	ret = insert_shadow_table(kvm, vm, shadow_size);
	if (ret < 0)
		goto err_unlock_unpin_host_vcpus;

	ret = kvm_guest_prepare_stage2(vm, pgd);
	if (ret)
		goto err_remove_shadow_table;

	hyp_spin_unlock(&shadow_lock);
	return vm->shadow_handle;

err_remove_shadow_table:
	remove_shadow_table(vm->shadow_handle);
err_unlock_unpin_host_vcpus:
	hyp_spin_unlock(&shadow_lock);
err_unpin_host_vcpus:
	unpin_host_vcpus(vm->shadow_vcpus, nr_vcpus);
err_remove_pgd:
	WARN_ON(__pkvm_hyp_donate_host(hyp_virt_to_pfn(pgd), nr_pgd_pages));

err_remove_mappings:
	/* Clear the donated shadow memory on failure to avoid data leaks. */
	memset(vm, 0, shadow_size);
	WARN_ON(__pkvm_hyp_donate_host(hyp_phys_to_pfn(shadow_pa),
				       shadow_size >> PAGE_SHIFT));

err:
	hyp_unpin_shared_mem(kvm, kvm + 1);
	return ret;
}

int __pkvm_teardown_shadow(int shadow_handle)
{
	struct kvm_hyp_memcache *mc;
	struct kvm_shadow_vm *vm;
	struct kvm *host_kvm;
	size_t shadow_size;
	int err;
	u64 pfn;
	u64 nr_pages;
	void *addr;
	int i;

	/* Lookup then remove entry from the shadow table. */
	hyp_spin_lock(&shadow_lock);
	vm = find_shadow_by_handle(shadow_handle);
	if (!vm) {
		err = -ENOENT;
		goto err_unlock;
	}

	if (WARN_ON(hyp_page_count(vm))) {
		err = -EBUSY;
		goto err_unlock;
	}

	/*
	 * Clear the tracking for last_loaded_vcpu for all cpus for this vm in
	 * case the same addresses for those vcpus are reused for future vms.
	 */
	for (i = 0; i < hyp_nr_cpus; i++) {
		struct kvm_vcpu **last_loaded_vcpu_ptr =
			per_cpu_ptr(&last_loaded_vcpu, i);
		struct kvm_vcpu *vcpu = *last_loaded_vcpu_ptr;

		if (vcpu && vcpu->arch.pkvm.shadow_vm == vm)
			*last_loaded_vcpu_ptr = NULL;
	}

	/* Ensure the VMID is clean before it can be reallocated */
	__kvm_tlb_flush_vmid(&vm->arch.mmu);
	remove_shadow_table(shadow_handle);
	hyp_spin_unlock(&shadow_lock);

	/* Reclaim guest pages, and page-table pages */
	mc = &vm->host_kvm->arch.pkvm.teardown_mc;
	reclaim_guest_pages(vm, mc);
	drain_shadow_vcpus(vm->shadow_vcpus, vm->created_vcpus, mc);
	unpin_host_vcpus(vm->shadow_vcpus, vm->created_vcpus);

	/* Push the metadata pages to the teardown memcache */
	shadow_size = vm->shadow_area_size;
	host_kvm = vm->host_kvm;
	memset(vm, 0, shadow_size);
	for (addr = vm; addr < ((void *)vm + shadow_size); addr += PAGE_SIZE)
		push_hyp_memcache(mc, addr, hyp_virt_to_phys);
	hyp_unpin_shared_mem(host_kvm, host_kvm + 1);

	pfn = hyp_phys_to_pfn(__hyp_pa(vm));
	nr_pages = shadow_size >> PAGE_SHIFT;
	WARN_ON(__pkvm_hyp_donate_host(pfn, nr_pages));
	return 0;

err_unlock:
	hyp_spin_unlock(&shadow_lock);
	return err;
}

int pkvm_load_pvmfw_pages(struct kvm_shadow_vm *vm, u64 ipa, phys_addr_t phys,
			  u64 size)
{
	struct kvm_protected_vm *pkvm = &vm->arch.pkvm;
	u64 npages, offset = ipa - pkvm->pvmfw_load_addr;
	void *src = hyp_phys_to_virt(pvmfw_base) + offset;

	if (offset >= pvmfw_size)
		return -EINVAL;

	size = min(size, pvmfw_size - offset);
	if (!PAGE_ALIGNED(size) || !PAGE_ALIGNED(src))
		return -EINVAL;

	npages = size >> PAGE_SHIFT;
	while (npages--) {
		void *dst;

		dst = hyp_fixmap_map(phys);
		if (!dst)
			return -EINVAL;

		/*
		 * No need for cache maintenance here, as the pgtable code will
		 * take care of this when installing the pte in the guest's
		 * stage-2 page table.
		 */
		memcpy(dst, src, PAGE_SIZE);

		hyp_fixmap_unmap();
		src += PAGE_SIZE;
		phys += PAGE_SIZE;
	}

	return 0;
}

void pkvm_clear_pvmfw_pages(void)
{
	void *addr = hyp_phys_to_virt(pvmfw_base);

	memset(addr, 0, pvmfw_size);
	kvm_flush_dcache_to_poc(addr, pvmfw_size);
}

/*
 * This function sets the registers on the vcpu to their architecturally defined
 * reset values.
 *
 * Note: Can only be called by the vcpu on itself, after it has been turned on.
 */
void pkvm_reset_vcpu(struct kvm_vcpu *vcpu)
{
	struct vcpu_reset_state *reset_state = &vcpu->arch.reset_state;
	struct kvm_shadow_vm *vm = vcpu->arch.pkvm.shadow_vm;

	WARN_ON(!reset_state->reset);

	init_ptrauth(vcpu);

	/* Reset core registers */
	memset(vcpu_gp_regs(vcpu), 0, sizeof(*vcpu_gp_regs(vcpu)));
	memset(&vcpu->arch.ctxt.fp_regs, 0, sizeof(vcpu->arch.ctxt.fp_regs));
	vcpu_gp_regs(vcpu)->pstate = VCPU_RESET_PSTATE_EL1;

	/* Reset system registers */
	kvm_reset_pvm_sys_regs(vcpu);

	/* Propagate initiator's endianness, after kvm_reset_pvm_sys_regs. */
	if (reset_state->be)
		kvm_vcpu_set_be(vcpu);

	if (vm->pvmfw_entry_vcpu == vcpu) {
		struct kvm_vcpu *host_vcpu = vcpu->arch.pkvm.host_vcpu;
		u64 entry = vm->arch.pkvm.pvmfw_load_addr;
		int i;

		/* X0 - X14 provided by the VMM (preserved) */
		for (i = 0; i <= 14; ++i)
			vcpu_set_reg(vcpu, i, vcpu_get_reg(host_vcpu, i));

		/* X15: Boot protocol version */
		vcpu_set_reg(vcpu, 15, 0);

		/* PC: IPA of pvmfw base */
		*vcpu_pc(vcpu) = entry;

		vm->pvmfw_entry_vcpu = NULL;

		/* Auto enroll MMIO guard */
		set_bit(KVM_ARCH_FLAG_MMIO_GUARD,
			&vcpu->arch.pkvm.shadow_vm->arch.flags);
	} else {
		*vcpu_pc(vcpu) = reset_state->pc;
		vcpu_set_reg(vcpu, 0, reset_state->r0);
	}

	reset_state->reset = false;

	vcpu->arch.pkvm.exit_code = 0;

	WARN_ON(vcpu->arch.pkvm.power_state != PSCI_0_2_AFFINITY_LEVEL_ON_PENDING);
	WRITE_ONCE(vcpu->arch.power_off, false);
	WRITE_ONCE(vcpu->arch.pkvm.power_state, PSCI_0_2_AFFINITY_LEVEL_ON);
}

struct kvm_vcpu *pvm_mpidr_to_vcpu(struct kvm_shadow_vm *vm, unsigned long mpidr)
{
	struct kvm_vcpu *vcpu;
	int i;

	mpidr &= MPIDR_HWID_BITMASK;

	for (i = 0; i < vm->created_vcpus; i++) {
		vcpu = vm->vcpus[i];

		if (mpidr == kvm_vcpu_get_mpidr_aff(vcpu))
			return vcpu;
	}

	return NULL;
}

/*
 * Returns true if the hypervisor handled PSCI call, and control should go back
 * to the guest, or false if the host needs to do some additional work (i.e.,
 * wake up the vcpu).
 */
static bool pvm_psci_vcpu_on(struct kvm_vcpu *source_vcpu)
{
	struct kvm_shadow_vm *vm = source_vcpu->arch.pkvm.shadow_vm;
	struct kvm_vcpu *vcpu;
	struct vcpu_reset_state *reset_state;
	unsigned long cpu_id;
	unsigned long hvc_ret_val;
	int power_state;

	cpu_id = smccc_get_arg1(source_vcpu);
	if (!kvm_psci_valid_affinity(source_vcpu, cpu_id)) {
		hvc_ret_val = PSCI_RET_INVALID_PARAMS;
		goto error;
	}

	vcpu = pvm_mpidr_to_vcpu(vm, cpu_id);

	/* Make sure the caller requested a valid vcpu. */
	if (!vcpu) {
		hvc_ret_val = PSCI_RET_INVALID_PARAMS;
		goto error;
	}

	/*
	 * Make sure the requested vcpu is not on to begin with.
	 * Atomic to avoid race between vcpus trying to power on the same vcpu.
	 */
	power_state = cmpxchg(&vcpu->arch.pkvm.power_state,
		PSCI_0_2_AFFINITY_LEVEL_OFF,
		PSCI_0_2_AFFINITY_LEVEL_ON_PENDING);
	switch (power_state) {
	case PSCI_0_2_AFFINITY_LEVEL_ON_PENDING:
		hvc_ret_val = PSCI_RET_ON_PENDING;
		goto error;
	case PSCI_0_2_AFFINITY_LEVEL_ON:
		hvc_ret_val = PSCI_RET_ALREADY_ON;
		goto error;
	case PSCI_0_2_AFFINITY_LEVEL_OFF:
		break;
	default:
		hvc_ret_val = PSCI_RET_INTERNAL_FAILURE;
		goto error;
	}

	reset_state = &vcpu->arch.reset_state;

	reset_state->pc = smccc_get_arg2(source_vcpu);
	reset_state->r0 = smccc_get_arg3(source_vcpu);

	/* Propagate caller endianness */
	reset_state->be = kvm_vcpu_is_be(source_vcpu);

	reset_state->reset = true;

	/*
	 * Return to the host, which should make the KVM_REQ_VCPU_RESET request
	 * as well as kvm_vcpu_wake_up() to schedule the vcpu.
	 */
	return false;

error:
	/* If there's an error go back straight to the guest. */
	smccc_set_retval(source_vcpu, hvc_ret_val, 0, 0, 0);
	return true;
}

static bool pvm_psci_vcpu_affinity_info(struct kvm_vcpu *vcpu)
{
	int i, matching_cpus = 0;
	unsigned long mpidr;
	unsigned long target_affinity;
	unsigned long target_affinity_mask;
	unsigned long lowest_affinity_level;
	struct kvm_shadow_vm *vm = vcpu->arch.pkvm.shadow_vm;
	struct kvm_vcpu *tmp;
	unsigned long hvc_ret_val;

	target_affinity = smccc_get_arg1(vcpu);
	lowest_affinity_level = smccc_get_arg2(vcpu);

	if (!kvm_psci_valid_affinity(vcpu, target_affinity)) {
		hvc_ret_val = PSCI_RET_INVALID_PARAMS;
		goto done;
	}

	/* Determine target affinity mask */
	target_affinity_mask = psci_affinity_mask(lowest_affinity_level);
	if (!target_affinity_mask) {
		hvc_ret_val = PSCI_RET_INVALID_PARAMS;
		goto done;
	}

	/* Ignore other bits of target affinity */
	target_affinity &= target_affinity_mask;

	hvc_ret_val = PSCI_0_2_AFFINITY_LEVEL_OFF;

	/*
	 * If at least one vcpu matching target affinity is ON then return ON,
	 * then if at least one is PENDING_ON then return PENDING_ON.
	 * Otherwise, return OFF.
	 */
	for (i = 0; i < vm->created_vcpus; i++) {
		tmp = vm->vcpus[i];
		mpidr = kvm_vcpu_get_mpidr_aff(tmp);

		if ((mpidr & target_affinity_mask) == target_affinity) {
			int power_state;

			matching_cpus++;
			power_state = READ_ONCE(tmp->arch.pkvm.power_state);
			switch (power_state) {
			case PSCI_0_2_AFFINITY_LEVEL_ON_PENDING:
				hvc_ret_val = PSCI_0_2_AFFINITY_LEVEL_ON_PENDING;
				break;
			case PSCI_0_2_AFFINITY_LEVEL_ON:
				hvc_ret_val = PSCI_0_2_AFFINITY_LEVEL_ON;
				goto done;
			case PSCI_0_2_AFFINITY_LEVEL_OFF:
				break;
			default:
				hvc_ret_val = PSCI_RET_INTERNAL_FAILURE;
				goto done;
			}
		}
	}

	if (!matching_cpus)
		hvc_ret_val = PSCI_RET_INVALID_PARAMS;

done:
	/* Nothing to be handled by the host. Go back to the guest. */
	smccc_set_retval(vcpu, hvc_ret_val, 0, 0, 0);
	return true;
}

/*
 * Returns true if the hypervisor has handled the PSCI call, and control should
 * go back to the guest, or false if the host needs to do some additional work
 * (e.g., turn off and update vcpu scheduling status).
 */
static bool pvm_psci_vcpu_off(struct kvm_vcpu *vcpu)
{
	WARN_ON(vcpu->arch.power_off);
	WARN_ON(vcpu->arch.pkvm.power_state != PSCI_0_2_AFFINITY_LEVEL_ON);

	WRITE_ONCE(vcpu->arch.power_off, true);
	WRITE_ONCE(vcpu->arch.pkvm.power_state, PSCI_0_2_AFFINITY_LEVEL_OFF);

	/* Return to the host so that it can finish powering off the vcpu. */
	return false;
}

static bool pvm_psci_version(struct kvm_vcpu *vcpu)
{
	/* Nothing to be handled by the host. Go back to the guest. */
	smccc_set_retval(vcpu, KVM_ARM_PSCI_1_1, 0, 0, 0);
	return true;
}

static bool pvm_psci_not_supported(struct kvm_vcpu *vcpu)
{
	/* Nothing to be handled by the host. Go back to the guest. */
	smccc_set_retval(vcpu, PSCI_RET_NOT_SUPPORTED, 0, 0, 0);
	return true;
}

static bool pvm_psci_features(struct kvm_vcpu *vcpu)
{
	u32 feature = smccc_get_arg1(vcpu);
	unsigned long val;

	switch (feature) {
	case PSCI_0_2_FN_PSCI_VERSION:
	case PSCI_0_2_FN_CPU_SUSPEND:
	case PSCI_0_2_FN64_CPU_SUSPEND:
	case PSCI_0_2_FN_CPU_OFF:
	case PSCI_0_2_FN_CPU_ON:
	case PSCI_0_2_FN64_CPU_ON:
	case PSCI_0_2_FN_AFFINITY_INFO:
	case PSCI_0_2_FN64_AFFINITY_INFO:
	case PSCI_0_2_FN_SYSTEM_OFF:
	case PSCI_0_2_FN_SYSTEM_RESET:
	case PSCI_1_0_FN_PSCI_FEATURES:
	case PSCI_1_1_FN_SYSTEM_RESET2:
	case PSCI_1_1_FN64_SYSTEM_RESET2:
	case ARM_SMCCC_VERSION_FUNC_ID:
		val = PSCI_RET_SUCCESS;
		break;
	default:
		val = PSCI_RET_NOT_SUPPORTED;
		break;
	}

	/* Nothing to be handled by the host. Go back to the guest. */
	smccc_set_retval(vcpu, val, 0, 0, 0);
	return true;
}

static bool pkvm_handle_psci(struct kvm_vcpu *vcpu)
{
	u32 psci_fn = smccc_get_function(vcpu);

	switch (psci_fn) {
	case PSCI_0_2_FN_CPU_ON:
		kvm_psci_narrow_to_32bit(vcpu);
		fallthrough;
	case PSCI_0_2_FN64_CPU_ON:
		return pvm_psci_vcpu_on(vcpu);
	case PSCI_0_2_FN_CPU_OFF:
		return pvm_psci_vcpu_off(vcpu);
	case PSCI_0_2_FN_AFFINITY_INFO:
		kvm_psci_narrow_to_32bit(vcpu);
		fallthrough;
	case PSCI_0_2_FN64_AFFINITY_INFO:
		return pvm_psci_vcpu_affinity_info(vcpu);
	case PSCI_0_2_FN_PSCI_VERSION:
		return pvm_psci_version(vcpu);
	case PSCI_1_0_FN_PSCI_FEATURES:
		return pvm_psci_features(vcpu);
	case PSCI_0_2_FN_SYSTEM_RESET:
	case PSCI_0_2_FN_CPU_SUSPEND:
	case PSCI_0_2_FN64_CPU_SUSPEND:
	case PSCI_0_2_FN_SYSTEM_OFF:
	case PSCI_1_1_FN_SYSTEM_RESET2:
	case PSCI_1_1_FN64_SYSTEM_RESET2:
		return false; /* Handled by the host. */
	default:
		break;
	}

	return pvm_psci_not_supported(vcpu);
}

static u64 __pkvm_memshare_page_req(struct kvm_vcpu *vcpu, u64 ipa)
{
	u64 elr;

	/* Fake up a data abort (Level 3 translation fault on write) */
	vcpu->arch.fault.esr_el2 = (u32)ESR_ELx_EC_DABT_LOW << ESR_ELx_EC_SHIFT |
				   ESR_ELx_WNR | ESR_ELx_FSC_FAULT |
				   FIELD_PREP(ESR_ELx_FSC_LEVEL, 3);

	/* Shuffle the IPA around into the HPFAR */
	vcpu->arch.fault.hpfar_el2 = (ipa >> 8) & HPFAR_MASK;

	/* This is a virtual address. 0's good. Let's go with 0. */
	vcpu->arch.fault.far_el2 = 0;

	/* Rewind the ELR so we return to the HVC once the IPA is mapped */
	elr = read_sysreg(elr_el2);
	elr -=4;
	write_sysreg(elr, elr_el2);

	return ARM_EXCEPTION_TRAP;
}

static bool pkvm_memshare_call(struct kvm_vcpu *vcpu, u64 *exit_code)
{
	u64 ipa = smccc_get_arg1(vcpu);
	u64 arg2 = smccc_get_arg2(vcpu);
	u64 arg3 = smccc_get_arg3(vcpu);
	int err;

	if (arg2 || arg3)
		goto out_guest_err;

	err = __pkvm_guest_share_host(vcpu, ipa);
	switch (err) {
	case 0:
		/* Success! Now tell the host. */
		goto out_host;
	case -EFAULT:
		/*
		 * Convert the exception into a data abort so that the page
		 * being shared is mapped into the guest next time.
		 */
		*exit_code = __pkvm_memshare_page_req(vcpu, ipa);
		goto out_host;
	}

out_guest_err:
	smccc_set_retval(vcpu, SMCCC_RET_INVALID_PARAMETER, 0, 0, 0);
	return true;

out_host:
	return false;
}

static bool pkvm_memunshare_call(struct kvm_vcpu *vcpu)
{
	u64 ipa = smccc_get_arg1(vcpu);
	u64 arg2 = smccc_get_arg2(vcpu);
	u64 arg3 = smccc_get_arg3(vcpu);
	int err;

	if (arg2 || arg3)
		goto out_guest_err;

	err = __pkvm_guest_unshare_host(vcpu, ipa);
	if (err)
		goto out_guest_err;

	return false;

out_guest_err:
	smccc_set_retval(vcpu, SMCCC_RET_INVALID_PARAMETER, 0, 0, 0);
	return true;
}

static bool pkvm_install_ioguard_page(struct kvm_vcpu *vcpu, u64 *exit_code)
{
	u32 retval = SMCCC_RET_SUCCESS;
	u64 ipa = smccc_get_arg1(vcpu);
	int ret;

	ret = __pkvm_install_ioguard_page(vcpu, ipa);
	if (ret == -ENOMEM) {
		/*
		 * We ran out of memcache, let's ask for more. Cancel
		 * the effects of the HVC that took us here, and
		 * forward the hypercall to the host for page donation
		 * purposes.
		 */
		write_sysreg_el2(read_sysreg_el2(SYS_ELR) - 4, SYS_ELR);
		return false;
	}

	if (ret)
		retval = SMCCC_RET_INVALID_PARAMETER;

	smccc_set_retval(vcpu, retval, 0, 0, 0);
	return true;
}

bool smccc_trng_available;

static bool pkvm_forward_trng(struct kvm_vcpu *vcpu)
{
	u32 fn = smccc_get_function(vcpu);
	struct arm_smccc_res res;
	unsigned long arg1 = 0;

	/*
	 * Forward TRNG calls to EL3, as we can't trust the host to handle
	 * these for us.
	 */
	switch (fn) {
	case ARM_SMCCC_TRNG_FEATURES:
	case ARM_SMCCC_TRNG_RND32:
	case ARM_SMCCC_TRNG_RND64:
		arg1 = smccc_get_arg1(vcpu);
		fallthrough;
	case ARM_SMCCC_TRNG_VERSION:
	case ARM_SMCCC_TRNG_GET_UUID:
		arm_smccc_1_1_smc(fn, arg1, &res);
		smccc_set_retval(vcpu, res.a0, res.a1, res.a2, res.a3);
		memzero_explicit(&res, sizeof(res));
		break;
	}

	return true;
}

/*
 * Handler for protected VM HVC calls.
 *
 * Returns true if the hypervisor has handled the exit, and control should go
 * back to the guest, or false if it hasn't.
 */
bool kvm_handle_pvm_hvc64(struct kvm_vcpu *vcpu, u64 *exit_code)
{
	u32 fn = smccc_get_function(vcpu);
	u64 val[4] = { SMCCC_RET_NOT_SUPPORTED };

	switch (fn) {
	case ARM_SMCCC_VERSION_FUNC_ID:
		/* Nothing to be handled by the host. Go back to the guest. */
		val[0] = ARM_SMCCC_VERSION_1_1;
		break;
	case ARM_SMCCC_VENDOR_HYP_CALL_UID_FUNC_ID:
		val[0] = ARM_SMCCC_VENDOR_HYP_UID_KVM_REG_0;
		val[1] = ARM_SMCCC_VENDOR_HYP_UID_KVM_REG_1;
		val[2] = ARM_SMCCC_VENDOR_HYP_UID_KVM_REG_2;
		val[3] = ARM_SMCCC_VENDOR_HYP_UID_KVM_REG_3;
		break;
	case ARM_SMCCC_VENDOR_HYP_KVM_FEATURES_FUNC_ID:
		val[0] = BIT(ARM_SMCCC_KVM_FUNC_FEATURES);
		val[0] |= BIT(ARM_SMCCC_KVM_FUNC_HYP_MEMINFO);
		val[0] |= BIT(ARM_SMCCC_KVM_FUNC_MEM_SHARE);
		val[0] |= BIT(ARM_SMCCC_KVM_FUNC_MEM_UNSHARE);
		val[0] |= BIT(ARM_SMCCC_KVM_FUNC_MMIO_GUARD_INFO);
		val[0] |= BIT(ARM_SMCCC_KVM_FUNC_MMIO_GUARD_ENROLL);
		val[0] |= BIT(ARM_SMCCC_KVM_FUNC_MMIO_GUARD_MAP);
		val[0] |= BIT(ARM_SMCCC_KVM_FUNC_MMIO_GUARD_UNMAP);
		break;
	case ARM_SMCCC_VENDOR_HYP_KVM_MMIO_GUARD_ENROLL_FUNC_ID:
		set_bit(KVM_ARCH_FLAG_MMIO_GUARD, &vcpu->arch.pkvm.shadow_vm->arch.flags);
		val[0] = SMCCC_RET_SUCCESS;
		break;
	case ARM_SMCCC_VENDOR_HYP_KVM_MMIO_GUARD_MAP_FUNC_ID:
		return pkvm_install_ioguard_page(vcpu, exit_code);
	case ARM_SMCCC_VENDOR_HYP_KVM_MMIO_GUARD_UNMAP_FUNC_ID:
		if (__pkvm_remove_ioguard_page(vcpu, vcpu_get_reg(vcpu, 1)))
			val[0] = SMCCC_RET_SUCCESS;
		break;
	case ARM_SMCCC_VENDOR_HYP_KVM_MMIO_GUARD_INFO_FUNC_ID:
	case ARM_SMCCC_VENDOR_HYP_KVM_HYP_MEMINFO_FUNC_ID:
		if (smccc_get_arg1(vcpu) ||
		    smccc_get_arg2(vcpu) ||
		    smccc_get_arg3(vcpu)) {
			val[0] = SMCCC_RET_INVALID_PARAMETER;
		} else {
			val[0] = PAGE_SIZE;
		}
		break;
	case ARM_SMCCC_VENDOR_HYP_KVM_MEM_SHARE_FUNC_ID:
		return pkvm_memshare_call(vcpu, exit_code);
	case ARM_SMCCC_VENDOR_HYP_KVM_MEM_UNSHARE_FUNC_ID:
		return pkvm_memunshare_call(vcpu);
	case ARM_SMCCC_TRNG_VERSION ... ARM_SMCCC_TRNG_RND32:
	case ARM_SMCCC_TRNG_RND64:
		if (smccc_trng_available)
			return pkvm_forward_trng(vcpu);
		break;
	default:
		return pkvm_handle_psci(vcpu);
	}

	smccc_set_retval(vcpu, val[0], val[1], val[2], val[3]);
	return true;
}
