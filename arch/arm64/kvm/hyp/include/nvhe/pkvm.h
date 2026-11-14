/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2021 Google LLC
 * Author: Fuad Tabba <tabba@google.com>
 */

#ifndef __ARM64_KVM_NVHE_PKVM_H__
#define __ARM64_KVM_NVHE_PKVM_H__

#include <asm/kvm_pkvm.h>
#include <kvm/power_domain.h>

#include <nvhe/gfp.h>
#include <nvhe/spinlock.h>

/*
 * Holds the relevant data for maintaining the vcpu state completely at hyp.
 */
struct pkvm_hyp_vcpu {
	struct kvm_vcpu vcpu;

	/* Backpointer to the host's (untrusted) vCPU instance. */
	struct kvm_vcpu *host_vcpu;

	/*
	 * If this hyp vCPU is loaded, then this is a backpointer to the
	 * per-cpu pointer tracking us. Otherwise, NULL if not loaded.
	 */
	struct pkvm_hyp_vcpu **loaded_hyp_vcpu;

	/* Tracks exit code for the protected guest. */
	u32 exit_code;

	/*
	 * Track the power state transition of a protected vcpu.
	 * Can be in one of three states:
	 * PSCI_0_2_AFFINITY_LEVEL_ON
	 * PSCI_0_2_AFFINITY_LEVEL_OFF
	 * PSCI_0_2_AFFINITY_LEVEL_PENDING
	 */
	int power_state;
};

/* Holds the hyp address of the mapped RX/TX buffers inside the hypervisor */
struct kvm_ffa_buffers {
	void *tx;
	u64 tx_ipa;
	void *rx;
	u64 rx_ipa;
	struct list_head xfer_list;
	u64 vm_avail_bitmap;
	u64 vm_creating_bitmap;
};

/*
 * Holds the relevant data for running a protected vm.
 */
struct pkvm_hyp_vm {
	struct kvm kvm;

	/* Backpointer to the host's (untrusted) KVM instance. */
	struct kvm *host_kvm;

	/* The guest's stage-2 page-table managed by the hypervisor. */
	struct kvm_pgtable pgt;
	struct kvm_pgtable_mm_ops mm_ops;
	struct hyp_pool pool;
	hyp_spinlock_t pgtable_lock;

	/* pvIOMMUs attached. */
	struct list_head pviommus;
	struct hyp_pool iommu_pool;
	struct list_head domains;
	/* Primary vCPU pending entry to the pvmfw */
	struct pkvm_hyp_vcpu *pvmfw_entry_vcpu;

	unsigned short refcount;

	hyp_spinlock_t vcpus_lock;

	/*
	 * True when the guest is being torn down. When in this state, the
	 * guest's vCPUs can't be loaded anymore, but its pages can be
	 * reclaimed by the host.
	 */
	bool is_dying;

	struct kvm_ffa_buffers ffa_buf;
	struct list_head vm_list;

	/* Array of the hyp vCPU structures for this VM. */
	struct pkvm_hyp_vcpu *vcpus[];
};

struct ffa_mem_transfer *__pkvm_get_vm_ffa_transfer(u16 handle);

static inline struct pkvm_hyp_vm *
pkvm_hyp_vcpu_to_hyp_vm(struct pkvm_hyp_vcpu *hyp_vcpu)
{
	return container_of(hyp_vcpu->vcpu.kvm, struct pkvm_hyp_vm, kvm);
}

static inline bool pkvm_hyp_vcpu_is_protected(struct pkvm_hyp_vcpu *hyp_vcpu)
{
	return vcpu_is_protected(&hyp_vcpu->vcpu);
}

static inline bool pkvm_hyp_vm_is_protected(struct pkvm_hyp_vm *hyp_vm)
{
	return kvm_vm_is_protected(&hyp_vm->kvm);
}

extern phys_addr_t pvmfw_base;
extern phys_addr_t pvmfw_size;

void pkvm_hyp_vm_table_init(void *tbl);

struct kvm_hyp_req *
pkvm_hyp_req_reserve(struct pkvm_hyp_vcpu *hyp_vcpu, u8 type);

int __pkvm_init_vm(struct kvm *host_kvm, unsigned long pgd_hva);
int __pkvm_init_vcpu(pkvm_handle_t handle, struct kvm_vcpu *host_vcpu);
int __pkvm_start_teardown_vm(pkvm_handle_t handle);
int __pkvm_finalize_teardown_vm(pkvm_handle_t handle);
int __pkvm_reclaim_dying_guest_page(pkvm_handle_t handle, u64 pfn, u64 gfn, u8 order);
int __pkvm_reclaim_dying_guest_ffa_resources(pkvm_handle_t handle);
int __pkvm_notify_guest_vm_avail(pkvm_handle_t handle);

struct pkvm_hyp_vcpu *pkvm_load_hyp_vcpu(pkvm_handle_t handle,
					 unsigned int vcpu_idx);
void pkvm_put_hyp_vcpu(struct pkvm_hyp_vcpu *hyp_vcpu);
struct pkvm_hyp_vcpu *pkvm_get_loaded_hyp_vcpu(void);

struct pkvm_hyp_vm *get_pkvm_hyp_vm(pkvm_handle_t handle);
void put_pkvm_hyp_vm(struct pkvm_hyp_vm *hyp_vm);
struct pkvm_hyp_vm *get_np_pkvm_hyp_vm(pkvm_handle_t handle);

bool kvm_handle_pvm_sysreg(struct kvm_vcpu *vcpu, u64 *exit_code);
bool kvm_handle_pvm_restricted(struct kvm_vcpu *vcpu, u64 *exit_code);
void kvm_init_pvm_id_regs(struct kvm_vcpu *vcpu);
void kvm_reset_pvm_sys_regs(struct kvm_vcpu *vcpu);
int kvm_check_pvm_sysreg_table(void);

void pkvm_reset_vcpu(struct pkvm_hyp_vcpu *hyp_vcpu);

bool kvm_handle_pvm_hvc64(struct kvm_vcpu *vcpu, u64 *exit_code);
bool kvm_hyp_handle_hvc64(struct kvm_vcpu *vcpu, u64 *exit_code);

bool kvm_handle_pvm_smc64(struct kvm_vcpu *vcpu, u64 *exit_code);

struct pkvm_hyp_vcpu *pkvm_mpidr_to_hyp_vcpu(struct pkvm_hyp_vm *vm, u64 mpidr);

static inline bool pkvm_hyp_vm_has_pvmfw(struct pkvm_hyp_vm *vm)
{
	return vm->kvm.arch.pkvm.pvmfw_load_addr != PVMFW_INVALID_LOAD_ADDR;
}

static inline bool pkvm_ipa_range_has_pvmfw(struct pkvm_hyp_vm *vm,
					    u64 ipa_start, u64 ipa_end)
{
	struct kvm_protected_vm *pkvm = &vm->kvm.arch.pkvm;
	u64 pvmfw_load_end = pkvm->pvmfw_load_addr + pvmfw_size;

	if (!pkvm_hyp_vm_has_pvmfw(vm))
		return false;

	return ipa_end > pkvm->pvmfw_load_addr && ipa_start < pvmfw_load_end;
}

int pkvm_load_pvmfw_pages(struct pkvm_hyp_vm *vm, u64 ipa, phys_addr_t phys,
			  u64 size);
void pkvm_poison_pvmfw_pages(void);

int pkvm_timer_init(void);
void pkvm_udelay(unsigned long usecs);

#define MAX_POWER_DOMAINS		32

struct kvm_power_domain_ops {
	int (*power_on)(struct kvm_power_domain *pd);
	int (*power_off)(struct kvm_power_domain *pd);
};

int pkvm_init_hvc_pd(struct kvm_power_domain *pd,
		     const struct kvm_power_domain_ops *ops);

int pkvm_host_hvc_pd(u64 device_id, u64 on);
int pkvm_init_scmi_pd(struct kvm_power_domain *pd,
		      const struct kvm_power_domain_ops *ops);

bool pkvm_device_request_mmio(struct pkvm_hyp_vcpu *hyp_vcpu, u64 *exit_code);
bool pkvm_device_request_dma(struct pkvm_hyp_vcpu *hyp_vcpu, u64 *exit_code);
void pkvm_devices_teardown(struct pkvm_hyp_vm *vm);
int pkvm_devices_get_context(u64 iommu_id, u32 endpoint_id, struct pkvm_hyp_vm *vm);
void pkvm_devices_put_context(u64 iommu_id, u32 endpoint_id);

/*
 * Register a power domain. When the hypervisor catches power requests from the
 * host for this power domain, it calls the power ops with @pd as argument.
 */
static inline int pkvm_init_power_domain(struct kvm_power_domain *pd,
					 const struct kvm_power_domain_ops *ops)
{
	switch (pd->type) {
	case KVM_POWER_DOMAIN_NONE:
		return 0;
	case KVM_POWER_DOMAIN_HOST_HVC:
		return pkvm_init_hvc_pd(pd, ops);
	case KVM_POWER_DOMAIN_ARM_SCMI:
		return pkvm_init_scmi_pd(pd, ops);
	default:
		return -EOPNOTSUPP;
	}
}

int pkvm_init_devices(void);
int pkvm_device_hyp_assign_mmio(u64 pfn, u64 nr_pages);
int pkvm_device_reclaim_mmio(u64 pfn, u64 nr_pages);
int pkvm_host_map_guest_mmio(struct pkvm_hyp_vcpu *hyp_vcpu, u64 pfn, u64 gfn);
int pkvm_device_register_reset(u64 phys, void *cookie,
			       int (*cb)(void *cookie, bool host_to_guest));
int pkvm_handle_empty_memcache(struct pkvm_hyp_vcpu *hyp_vcpu, u64 *exit_code);
u32 hyp_vcpu_to_ffa_handle(struct pkvm_hyp_vcpu *hyp_vcpu);
u32 vm_handle_to_ffa_handle(pkvm_handle_t vm_handle);

#endif /* __ARM64_KVM_NVHE_PKVM_H__ */
