/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2020 Google LLC
 * Author: Quentin Perret <qperret@google.com>
 */

#ifndef __KVM_NVHE_MEM_PROTECT__
#define __KVM_NVHE_MEM_PROTECT__
#include <linux/kvm_host.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_pgtable.h>
#include <asm/kvm_pkvm_module.h>
#include <asm/virt.h>
#include <nvhe/memory.h>
#include <nvhe/pkvm.h>
#include <nvhe/spinlock.h>

struct host_mmu {
	struct kvm_arch arch;
	struct kvm_pgtable pgt;
	struct kvm_pgtable_mm_ops mm_ops;
	hyp_spinlock_t lock;
};
extern struct host_mmu host_mmu;

/* This corresponds to page-table locking order */
enum pkvm_component_id {
	PKVM_ID_HOST,
	PKVM_ID_HYP,
	PKVM_ID_FFA,
	PKVM_ID_GUEST,
	PKVM_ID_PROTECTED,
	PKVM_ID_MAX = PKVM_ID_PROTECTED,
};

extern unsigned long hyp_nr_cpus;

extern struct kvm_hyp_pinned_page *hyp_ppages;

int __pkvm_prot_finalize(void);
int __pkvm_host_share_hyp(u64 pfn);
int __pkvm_host_unshare_hyp(u64 pfn);
int __pkvm_host_reclaim_page(struct pkvm_hyp_vm *vm, u64 pfn, u64 ipa, u8 order);
int __pkvm_host_donate_hyp(u64 pfn, u64 nr_pages);
int ___pkvm_host_donate_hyp(u64 pfn, u64 nr_pages, bool accept_mmio);
int ___pkvm_host_donate_hyp_prot(u64 pfn, u64 nr_pages,
				 bool accept_mmio, enum kvm_pgtable_prot prot);
int __pkvm_host_donate_sglist_hyp(struct pkvm_sglist_page *sglist, size_t nr_pages);
int __pkvm_host_donate_hyp_locked(u64 pfn, u64 nr_pages, enum kvm_pgtable_prot prot);
int __pkvm_hyp_donate_host(u64 pfn, u64 nr_pages);
int __pkvm_guest_share_hyp_page(struct pkvm_hyp_vcpu *vcpu, u64 ipa, u64 *hyp_va);
int __pkvm_guest_unshare_hyp_page(struct pkvm_hyp_vcpu *vcpu, u64 ipa);
int __pkvm_guest_share_ffa_page(struct pkvm_hyp_vcpu *vcpu, u64 ipa, phys_addr_t *phys);
int __pkvm_guest_unshare_ffa_page(struct pkvm_hyp_vcpu *vcpu, u64 ipa);
int __pkvm_host_share_ffa(u64 pfn, u64 nr_pages);
int __pkvm_host_unshare_ffa(u64 pfn, u64 nr_pages);
int __pkvm_host_donate_guest(u64 pfn, u64 gfn, struct pkvm_hyp_vcpu *vcpu, u64 nr_pages);
int __pkvm_host_donate_sglist_guest(struct pkvm_hyp_vcpu *vcpu);
int __pkvm_host_share_guest(u64 pfn, u64 gfn, struct pkvm_hyp_vcpu *vcpu,
			    enum kvm_pgtable_prot prot, u64 nr_pages);
int __pkvm_host_unshare_guest(u64 gfn, struct pkvm_hyp_vm *vm, u64 nr_pages);
int __pkvm_host_relax_perms_guest(u64 gfn, struct pkvm_hyp_vcpu *vcpu, enum kvm_pgtable_prot prot);
int __pkvm_host_wrprotect_guest(u64 gfn, struct pkvm_hyp_vm *hyp_vm, u64 size);
int __pkvm_host_test_clear_young_guest(u64 gfn, u64 size, bool mkold, struct pkvm_hyp_vm *vm);
kvm_pte_t __pkvm_host_mkyoung_guest(u64 gfn, struct pkvm_hyp_vcpu *vcpu);
int __pkvm_host_split_guest(u64 gfn, u64 size, struct pkvm_hyp_vcpu *vcpu);
int __pkvm_host_donate_ffa(u64 pfn, u64 nr_pages);
int __pkvm_host_reclaim_ffa(u64 pfn, u64 nr_pages);
int __pkvm_guest_share_host(struct pkvm_hyp_vcpu *hyp_vcpu, u64 ipa,
			    u64 nr_pages, u64 *nr_shared);
int __pkvm_guest_unshare_host(struct pkvm_hyp_vcpu *hyp_vcpu, u64 ipa,
			      u64 nr_pages, u64 *nr_unshared);
int __pkvm_install_ioguard_page(struct pkvm_hyp_vcpu *hyp_vcpu, u64 ipa,
				u64 nr_pages, u64 *nr_guarded);
bool __pkvm_check_ioguard_page(struct pkvm_hyp_vcpu *hyp_vcpu);
int __pkvm_guest_relinquish_to_host(struct pkvm_hyp_vcpu *vcpu,
				    u64 ipa, u64 *ppa);
int __pkvm_use_dma(u64 phys_addr, size_t size, struct pkvm_hyp_vcpu *hyp_vcpu);
int __pkvm_unuse_dma(u64 phys_addr, size_t size, struct pkvm_hyp_vcpu *hyp_vcpu);
u64 __pkvm_ptdump_get_config(pkvm_handle_t handle, enum pkvm_ptdump_ops op);
u64 __pkvm_ptdump_walk_range(pkvm_handle_t handle, struct pkvm_ptdump_log_hdr *log_hva);

int hyp_check_range_owned(u64 addr, u64 size);
int __pkvm_install_guest_mmio(struct pkvm_hyp_vcpu *hyp_vcpu, u64 pfn, u64 gfn);

int pkvm_get_guest_pa_request(struct pkvm_hyp_vcpu *hyp_vcpu, u64 ipa,
			      size_t ipa_size_request, u64 *out_pa, s8 *out_level);
int pkvm_get_guest_pa_request_use_dma(struct pkvm_hyp_vcpu *hyp_vcpu, u64 ipa,
				      size_t ipa_size_request, u64 *out_pa, s8 *level);
bool addr_is_memory(phys_addr_t phys);
int host_stage2_idmap_locked(phys_addr_t addr, u64 size,
			     enum kvm_pgtable_prot prot,
			     bool update_iommu);
int host_stage2_set_owner_locked(phys_addr_t addr, u64 size, u8 owner_id);
int host_stage2_unmap_reg_locked(phys_addr_t start, u64 size);
int kvm_host_prepare_stage2(void *pgt_pool_base);
int kvm_guest_prepare_stage2(struct pkvm_hyp_vm *vm, void *pgd);
void handle_host_mem_abort(struct kvm_cpu_context *host_ctxt);

int hyp_pin_shared_mem(void *from, void *to);
void hyp_unpin_shared_mem(void *from, void *to);
int host_stage2_get_leaf(phys_addr_t phys, kvm_pte_t *ptep, s8 *level);
int refill_memcache(struct kvm_hyp_memcache *mc, unsigned long min_pages,
		    struct kvm_hyp_memcache *host_mc);

int refill_hyp_pool(struct hyp_pool *pool, struct kvm_hyp_memcache *host_mc);
int reclaim_hyp_pool(struct hyp_pool *pool, struct kvm_hyp_memcache *host_mc,
		     int nr_pages);

void destroy_hyp_vm_pgt(struct pkvm_hyp_vm *vm);
void drain_hyp_pool(struct hyp_pool *pool, struct kvm_hyp_memcache *mc);

int module_change_host_page_prot(u64 pfn, enum kvm_pgtable_prot prot, u64 nr_pages, bool update_iommu);

void psci_mem_protect_inc(u64 n);
void psci_mem_protect_dec(u64 n);

static __always_inline void __load_host_stage2(void)
{
	if (static_branch_likely(&kvm_protected_mode_initialized))
		__load_stage2(&host_mmu.arch.mmu, &host_mmu.arch);
	else
		write_sysreg(0, vttbr_el2);
}

#ifdef CONFIG_PKVM_SELFTESTS
void pkvm_ownership_selftest(void *base);
#else
static inline void pkvm_ownership_selftest(void *base) { }
#endif
#endif /* __KVM_NVHE_MEM_PROTECT__ */
