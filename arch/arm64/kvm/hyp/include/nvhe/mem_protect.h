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
#include <asm/virt.h>
#include <nvhe/ffa.h>
#include <nvhe/spinlock.h>

/*
 * SW bits 0-1 are reserved to track the memory ownership state of each page:
 *   00: The page is owned exclusively by the page-table owner.
 *   01: The page is owned by the page-table owner, but is shared
 *       with another entity.
 *   10: The page is shared with, but not owned by the page-table owner.
 *   11: Reserved for future use (lending).
 */
enum pkvm_page_state {
	PKVM_PAGE_OWNED			= 0ULL,
	PKVM_PAGE_SHARED_OWNED		= KVM_PGTABLE_PROT_SW0,
	PKVM_PAGE_SHARED_BORROWED	= KVM_PGTABLE_PROT_SW1,
	__PKVM_PAGE_RESERVED		= KVM_PGTABLE_PROT_SW0 |
					  KVM_PGTABLE_PROT_SW1,

	/* Meta-states which aren't encoded directly in the PTE's SW bits */
	PKVM_NOPAGE,
};

#define PKVM_PAGE_STATE_PROT_MASK	(KVM_PGTABLE_PROT_SW0 | KVM_PGTABLE_PROT_SW1)
static inline enum kvm_pgtable_prot pkvm_mkstate(enum kvm_pgtable_prot prot,
						 enum pkvm_page_state state)
{
	return (prot & ~PKVM_PAGE_STATE_PROT_MASK) | state;
}

static inline enum pkvm_page_state pkvm_getstate(enum kvm_pgtable_prot prot)
{
	return prot & PKVM_PAGE_STATE_PROT_MASK;
}

struct host_kvm {
	struct kvm_arch arch;
	struct kvm_pgtable pgt;
	struct kvm_pgtable_mm_ops mm_ops;
	struct kvm_ffa_buffers ffa;
	hyp_spinlock_t lock;
};
extern struct host_kvm host_kvm;

typedef u32 pkvm_id;
static const pkvm_id pkvm_host_id	= 0;
static const pkvm_id pkvm_hyp_id	= (1 << 16);
static const pkvm_id pkvm_ffa_id	= pkvm_hyp_id + 1; /* Secure world */

extern unsigned long hyp_nr_cpus;

int __pkvm_prot_finalize(void);
int __pkvm_host_share_hyp(u64 pfn);
int __pkvm_host_unshare_hyp(u64 pfn);
int __pkvm_host_reclaim_page(u64 pfn);
int __pkvm_host_donate_hyp(u64 pfn, u64 nr_pages);
int __pkvm_hyp_donate_host(u64 pfn, u64 nr_pages);
int __pkvm_host_share_guest(u64 pfn, u64 gfn, struct kvm_vcpu *vcpu);
int __pkvm_host_donate_guest(u64 pfn, u64 gfn, struct kvm_vcpu *vcpu);
int __pkvm_guest_share_host(struct kvm_vcpu *vcpu, u64 ipa);
int __pkvm_guest_unshare_host(struct kvm_vcpu *vcpu, u64 ipa);
int __pkvm_host_share_ffa(u64 pfn, u64 nr_pages);
int __pkvm_host_unshare_ffa(u64 pfn, u64 nr_pages);
int __pkvm_install_ioguard_page(struct kvm_vcpu *vcpu, u64 ipa);
int __pkvm_remove_ioguard_page(struct kvm_vcpu *vcpu, u64 ipa);
bool __pkvm_check_ioguard_page(struct kvm_vcpu *vcpu);

bool addr_is_memory(phys_addr_t phys);
int host_stage2_idmap_locked(phys_addr_t addr, u64 size, enum kvm_pgtable_prot prot);
int host_stage2_set_owner_locked(phys_addr_t addr, u64 size, pkvm_id owner_id);
int host_stage2_unmap_dev_locked(phys_addr_t start, u64 size);
int kvm_host_prepare_stage2(void *pgt_pool_base);
int kvm_guest_prepare_stage2(struct kvm_shadow_vm *vm, void *pgd);
void handle_host_mem_abort(struct kvm_cpu_context *host_ctxt);

int hyp_pin_shared_mem(void *from, void *to);
void hyp_unpin_shared_mem(void *from, void *to);
int refill_memcache(struct kvm_hyp_memcache *mc, unsigned long min_pages,
		    struct kvm_hyp_memcache *host_mc);
void reclaim_guest_pages(struct kvm_shadow_vm *vm, struct kvm_hyp_memcache *mc);

void psci_mem_protect_inc(void);
void psci_mem_protect_dec(void);

static __always_inline void __load_host_stage2(void)
{
	if (static_branch_likely(&kvm_protected_mode_initialized))
		__load_stage2(&host_kvm.arch.mmu, &host_kvm.arch);
	else
		write_sysreg(0, vttbr_el2);
}
#endif /* __KVM_NVHE_MEM_PROTECT__ */
