/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 Google LLC
 * Author: Fuad Tabba <tabba@google.com>
 */

#ifndef __ARM64_KVM_NVHE_PKVM_H__
#define __ARM64_KVM_NVHE_PKVM_H__

#include <asm/kvm_pkvm.h>

#include <nvhe/gfp.h>
#include <nvhe/spinlock.h>

/*
 * A container for the vcpu state that hyp needs to maintain for protected VMs.
 */
struct shadow_vcpu_state {
	struct kvm_shadow_vm *vm;
	struct kvm_vcpu vcpu;
};

/*
 * Holds the relevant data for running a protected vm.
 */
struct kvm_shadow_vm {
	/* A unique id to the shadow structs in the hyp shadow area. */
	int shadow_handle;

	/* Number of vcpus for the vm. */
	int created_vcpus;

	/* Pointers to the shadow vcpus of the shadow vm. */
	struct kvm_vcpu *vcpus[KVM_MAX_VCPUS];

	/* Primary vCPU pending entry to the pvmfw */
	struct kvm_vcpu *pvmfw_entry_vcpu;

	/* The host's kvm structure. */
	struct kvm *host_kvm;

	/* The total size of the donated shadow area. */
	size_t shadow_area_size;

	/*
	 * The number of vcpus initialized and ready to run in the shadow vm.
	 * Modifying this is protected by shadow_lock.
	 */
	unsigned int nr_vcpus;

	struct kvm_arch arch;
	struct kvm_pgtable pgt;
	struct kvm_pgtable_mm_ops mm_ops;
	struct hyp_pool pool;
	hyp_spinlock_t lock;

	/* Array of the shadow state pointers per vcpu. */
	struct shadow_vcpu_state *shadow_vcpus[0];
};

static inline bool vcpu_is_protected(struct kvm_vcpu *vcpu)
{
	if (!is_protected_kvm_enabled())
		return false;

	return vcpu->arch.pkvm.shadow_vm->arch.pkvm.enabled;
}

extern phys_addr_t pvmfw_base;
extern phys_addr_t pvmfw_size;

void hyp_shadow_table_init(void *tbl);
int __pkvm_init_shadow(struct kvm *kvm, void *shadow_va, size_t size, void *pgd);
int __pkvm_init_shadow_vcpu(unsigned int shadow_handle,
			    struct kvm_vcpu *host_vcpu,
			    void *shadow_vcpu_hva);
int __pkvm_teardown_shadow(int shadow_handle);
struct kvm_vcpu *get_shadow_vcpu(int shadow_handle, unsigned int vcpu_idx);
void put_shadow_vcpu(struct kvm_vcpu *vcpu);

u64 pvm_read_id_reg(const struct kvm_vcpu *vcpu, u32 id);
bool kvm_handle_pvm_sysreg(struct kvm_vcpu *vcpu, u64 *exit_code);
bool kvm_handle_pvm_restricted(struct kvm_vcpu *vcpu, u64 *exit_code);
void kvm_reset_pvm_sys_regs(struct kvm_vcpu *vcpu);
int kvm_check_pvm_sysreg_table(void);

void pkvm_reset_vcpu(struct kvm_vcpu *vcpu);

bool kvm_handle_pvm_hvc64(struct kvm_vcpu *vcpu, u64 *exit_code);

struct kvm_vcpu *pvm_mpidr_to_vcpu(struct kvm_shadow_vm *vm, unsigned long mpidr);

static inline bool pvm_has_pvmfw(struct kvm_shadow_vm *vm)
{
	return vm->arch.pkvm.pvmfw_load_addr != PVMFW_INVALID_LOAD_ADDR;
}

static inline bool ipa_in_pvmfw_region(struct kvm_shadow_vm *vm, u64 ipa)
{
	struct kvm_protected_vm *pkvm = &vm->arch.pkvm;

	if (!pvm_has_pvmfw(vm))
		return false;

	return ipa - pkvm->pvmfw_load_addr < pvmfw_size;
}

int pkvm_load_pvmfw_pages(struct kvm_shadow_vm *vm, u64 ipa, phys_addr_t phys,
			  u64 size);
void pkvm_clear_pvmfw_pages(void);

#endif /* __ARM64_KVM_NVHE_PKVM_H__ */
