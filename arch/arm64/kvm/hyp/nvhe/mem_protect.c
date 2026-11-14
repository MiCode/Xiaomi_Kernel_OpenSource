// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Google LLC
 * Author: Quentin Perret <qperret@google.com>
 */

#include <linux/kvm_host.h>
#include <asm/kvm_emulate.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_hypevents.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_pgtable.h>
#include <asm/kvm_pkvm.h>
#include <asm/stage2_pgtable.h>

#include <hyp/fault.h>

#include <nvhe/gfp.h>
#include <nvhe/iommu.h>
#include <nvhe/memory.h>
#include <nvhe/mem_protect.h>
#include <nvhe/mm.h>
#include <nvhe/modules.h>

#define KVM_HOST_S2_FLAGS (KVM_PGTABLE_S2_NOFWB | \
			   KVM_PGTABLE_S2_IDMAP | \
			   KVM_PGTABLE_S2_PREFAULT_BLOCK)

struct host_mmu host_mmu;

struct pkvm_moveable_reg pkvm_moveable_regs[PKVM_NR_MOVEABLE_REGS];
unsigned int pkvm_moveable_regs_nr;

static struct hyp_pool host_s2_pool;

static DEFINE_PER_CPU(struct pkvm_hyp_vm *, __current_vm);
#define current_vm (*this_cpu_ptr(&__current_vm))

static struct kvm_pgtable_pte_ops host_s2_pte_ops;
static bool host_stage2_force_pte(u64 addr, u64 end, enum kvm_pgtable_prot prot);
static bool host_stage2_pte_is_counted(kvm_pte_t pte, u32 level);
static bool guest_stage2_force_pte_cb(u64 addr, u64 end,
				      enum kvm_pgtable_prot prot);
static bool guest_stage2_pte_is_counted(kvm_pte_t pte, u32 level);

static struct kvm_pgtable_pte_ops guest_s2_pte_ops = {
	.force_pte_cb = guest_stage2_force_pte_cb,
	.pte_is_counted_cb = guest_stage2_pte_is_counted
};

static void guest_lock_component(struct pkvm_hyp_vm *vm)
{
	hyp_spin_lock(&vm->pgtable_lock);
	current_vm = vm;
}

static void guest_unlock_component(struct pkvm_hyp_vm *vm)
{
	current_vm = NULL;
	hyp_spin_unlock(&vm->pgtable_lock);
}

static void host_lock_component(void)
{
	hyp_spin_lock(&host_mmu.lock);
}

static void host_unlock_component(void)
{
	hyp_spin_unlock(&host_mmu.lock);
}

static void hyp_lock_component(void)
{
	hyp_spin_lock(&pkvm_pgd_lock);
}

static void hyp_unlock_component(void)
{
	hyp_spin_unlock(&pkvm_pgd_lock);
}

static void *host_s2_zalloc_pages_exact(size_t size)
{
	void *addr = hyp_alloc_pages(&host_s2_pool, get_order(size));

	hyp_split_page(hyp_virt_to_page(addr));

	/*
	 * The size of concatenated PGDs is always a power of two of PAGE_SIZE,
	 * so there should be no need to free any of the tail pages to make the
	 * allocation exact.
	 */
	WARN_ON(size != (PAGE_SIZE << get_order(size)));

	return addr;
}

static void *host_s2_zalloc_page(void *pool)
{
	return hyp_alloc_pages(pool, 0);
}

static void host_s2_get_page(void *addr)
{
	hyp_get_page(&host_s2_pool, addr);
}

static void host_s2_put_page(void *addr)
{
	hyp_put_page(&host_s2_pool, addr);
}

static void host_s2_free_unlinked_table(void *addr, s8 level)
{
	kvm_pgtable_stage2_free_unlinked(&host_mmu.mm_ops, host_mmu.pgt.pte_ops,
					 addr, level);
}

static int prepare_s2_pool(void *pgt_pool_base)
{
	unsigned long nr_pages, pfn;
	int ret;

	pfn = hyp_virt_to_pfn(pgt_pool_base);
	nr_pages = host_s2_pgtable_pages();
	ret = hyp_pool_init(&host_s2_pool, pfn, nr_pages, 0);
	if (ret)
		return ret;

	host_mmu.mm_ops = (struct kvm_pgtable_mm_ops) {
		.zalloc_pages_exact = host_s2_zalloc_pages_exact,
		.zalloc_page = host_s2_zalloc_page,
		.free_unlinked_table = host_s2_free_unlinked_table,
		.phys_to_virt = hyp_phys_to_virt,
		.virt_to_phys = hyp_virt_to_phys,
		.page_count = hyp_page_count,
		.get_page = host_s2_get_page,
		.put_page = host_s2_put_page,
	};

	return 0;
}

static void prepare_host_vtcr(void)
{
	u32 parange, phys_shift;

	/* The host stage 2 is id-mapped, so use parange for T0SZ */
	parange = kvm_get_parange(id_aa64mmfr0_el1_sys_val);
	phys_shift = id_aa64mmfr0_parange_to_phys_shift(parange);

	host_mmu.arch.mmu.vtcr = kvm_get_vtcr(id_aa64mmfr0_el1_sys_val,
					      id_aa64mmfr1_el1_sys_val, phys_shift);
}

static int prepopulate_host_stage2(void)
{
	struct memblock_region *reg;
	int i, ret = 0;

	for (i = 0; i < hyp_memblock_nr; i++) {
		reg = &hyp_memory[i];
		ret = host_stage2_idmap_locked(reg->base, reg->size, PKVM_HOST_MEM_PROT, false);
		if (ret)
			return ret;
	}

	return ret;
}

int kvm_host_prepare_stage2(void *pgt_pool_base)
{
	struct kvm_s2_mmu *mmu = &host_mmu.arch.mmu;
	int ret;

	prepare_host_vtcr();
	hyp_spin_lock_init(&host_mmu.lock);
	mmu->arch = &host_mmu.arch;

	ret = prepare_s2_pool(pgt_pool_base);
	if (ret)
		return ret;

	host_s2_pte_ops.force_pte_cb = host_stage2_force_pte;
	host_s2_pte_ops.pte_is_counted_cb = host_stage2_pte_is_counted;

	ret = __kvm_pgtable_stage2_init(&host_mmu.pgt, mmu,
					&host_mmu.mm_ops, KVM_HOST_S2_FLAGS,
					&host_s2_pte_ops);
	if (ret)
		return ret;

	mmu->pgd_phys = __hyp_pa(host_mmu.pgt.pgd);
	mmu->pgt = &host_mmu.pgt;
	atomic64_set(&mmu->vmid.id, 0);

	return prepopulate_host_stage2();
}

static bool guest_stage2_force_pte_cb(u64 addr, u64 end,
				      enum kvm_pgtable_prot prot)
{
	return false;
}

static bool guest_stage2_pte_is_counted(kvm_pte_t pte, u32 level)
{
	/*
	 * The refcount tracks valid entries as well as invalid entries if they
	 * encode ownership of a page to another entity than the page-table
	 * owner, whose id is 0.
	 */
	return !!pte;
}

static void *guest_s2_zalloc_pages_exact(size_t size)
{
	void *addr = hyp_alloc_pages(&current_vm->pool, get_order(size));

	WARN_ON(size != (PAGE_SIZE << get_order(size)));
	hyp_split_page(hyp_virt_to_page(addr));

	return addr;
}

static void guest_s2_free_pages_exact(void *addr, unsigned long size)
{
	u8 order = get_order(size);
	unsigned int i;

	for (i = 0; i < (1 << order); i++)
		hyp_put_page(&current_vm->pool, addr + (i * PAGE_SIZE));
}

static void *guest_s2_zalloc_page(void *mc)
{
	struct hyp_page *p;
	void *addr;
	unsigned long order;

	addr = hyp_alloc_pages(&current_vm->pool, 0);
	if (addr)
		return addr;

	addr = pop_hyp_memcache(mc, hyp_phys_to_virt, &order);
	if (!addr)
		return addr;

	WARN_ON(order);
	memset(addr, 0, PAGE_SIZE);
	p = hyp_virt_to_page(addr);
	hyp_set_page_refcounted(p);
	p->order = 0;

	return addr;
}

static void guest_s2_get_page(void *addr)
{
	hyp_get_page(&current_vm->pool, addr);
}

static void guest_s2_put_page(void *addr)
{
	hyp_put_page(&current_vm->pool, addr);
}

static void *__fixmap_guest_page(void *va, size_t *size)
{
	void *addr;

	if (WARN_ON(!IS_ALIGNED(*size, *size)))
		return NULL;

	if (IS_ALIGNED(*size, PMD_SIZE)) {
		addr = hyp_fixblock_map(__hyp_pa(va));
		if (addr)
			return addr;

		*size = PAGE_SIZE;
	}

	if (IS_ALIGNED(*size, PAGE_SIZE))
		return hyp_fixmap_map(__hyp_pa(va));

	WARN_ON(1);

	return NULL;
}

static void __fixunmap_guest_page(size_t size)
{
	switch (size) {
	case PAGE_SIZE:
		hyp_fixmap_unmap();
		break;
	case PMD_SIZE:
		hyp_fixblock_unmap();
		break;
	default:
		BUG();
	}
}

static void clean_dcache_guest_page(void *va, size_t size)
{
	while (size) {
		size_t __size = size == PMD_SIZE ? size : PAGE_SIZE;
		void *addr = __fixmap_guest_page(va, &__size);

		__clean_dcache_guest_page(addr, __size);
		__fixunmap_guest_page(__size);

		size -= __size;
		va += __size;
	}
}

static void invalidate_icache_guest_page(void *va, size_t size)
{
	while (size) {
		size_t __size = size == PMD_SIZE ? size : PAGE_SIZE;
		void *addr = __fixmap_guest_page(va, &__size);

		__invalidate_icache_guest_page(addr, __size);
		__fixunmap_guest_page(__size);

		size -= __size;
		va += __size;
	}
}

int kvm_guest_prepare_stage2(struct pkvm_hyp_vm *vm, void *pgd)
{
	struct kvm_s2_mmu *mmu = &vm->kvm.arch.mmu;
	unsigned long nr_pages;
	int ret;

	nr_pages = kvm_pgtable_stage2_pgd_size(mmu->vtcr) >> PAGE_SHIFT;
	ret = hyp_pool_init(&vm->pool, hyp_virt_to_pfn(pgd), nr_pages, 0);
	if (ret)
		return ret;

	hyp_spin_lock_init(&vm->pgtable_lock);
	vm->mm_ops = (struct kvm_pgtable_mm_ops) {
		.zalloc_pages_exact	= guest_s2_zalloc_pages_exact,
		.free_pages_exact	= guest_s2_free_pages_exact,
		.zalloc_page		= guest_s2_zalloc_page,
		.phys_to_virt		= hyp_phys_to_virt,
		.virt_to_phys		= hyp_virt_to_phys,
		.page_count		= hyp_page_count,
		.get_page		= guest_s2_get_page,
		.put_page		= guest_s2_put_page,
		.dcache_clean_inval_poc	= clean_dcache_guest_page,
		.icache_inval_pou	= invalidate_icache_guest_page,
	};

	guest_lock_component(vm);
	ret = __kvm_pgtable_stage2_init(mmu->pgt, mmu, &vm->mm_ops,
					KVM_PGTABLE_S2_PREFAULT_BLOCK,
					&guest_s2_pte_ops);
	guest_unlock_component(vm);
	if (ret)
		return ret;

	vm->kvm.arch.mmu.pgd_phys = __hyp_pa(vm->pgt.pgd);

	return 0;
}

static enum pkvm_page_state guest_get_page_state(kvm_pte_t pte, u64 addr)
{
	enum pkvm_page_state state = 0;
	enum kvm_pgtable_prot prot;

	if (!kvm_pte_valid(pte)) {
		state = PKVM_NOPAGE;

		if (pte == KVM_INVALID_PTE_MMIO_NOTE)
			state |= PKVM_MMIO;

		return state;
	}

	prot = kvm_pgtable_stage2_pte_prot(pte);
	if (kvm_pte_valid(pte) && ((prot & KVM_PGTABLE_PROT_RWX) != KVM_PGTABLE_PROT_RWX))
		state = PKVM_PAGE_RESTRICTED_PROT;

	return state | pkvm_getstate(prot);
}

int __pkvm_guest_relinquish_to_host(struct pkvm_hyp_vcpu *vcpu,
				    u64 ipa, u64 *ppa)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	enum pkvm_page_state state;
	u64 phys = 0, addr;
	kvm_pte_t pte;
	s8 level;
	int ret;

	if (!pkvm_hyp_vcpu_is_protected(vcpu))
		return 0;

	host_lock_component();
	guest_lock_component(vm);

	ret = kvm_pgtable_get_leaf(&vm->pgt, ipa, &pte, &level);
	if (ret || !kvm_pte_valid(pte))
		goto end;

	/* We don't support splitting non-leaf mappings */
	if (level != KVM_PGTABLE_LAST_LEVEL) {
		ret = -E2BIG;
		goto end;
	}

	state = guest_get_page_state(pte, ipa);
	if (state != PKVM_PAGE_OWNED) {
		ret = -EPERM;
		goto end;
	}

	addr = ALIGN_DOWN(ipa, kvm_granule_size(level));
	phys = kvm_pte_to_phys(pte);
	phys += ipa - addr;
	/* page might be used for DMA! */
	if (hyp_page_count(hyp_phys_to_virt(phys))) {
		ret = -EBUSY;
		goto end;
	}

	hyp_poison_page(phys, PAGE_SIZE);
	psci_mem_protect_dec(1);

	/* Zap the guest stage2 pte and return ownership to the host */
	ret = kvm_pgtable_stage2_unmap(&vm->pgt, ipa, PAGE_SIZE);
	if (ret)
		goto end;

	WARN_ON(host_stage2_set_owner_locked(phys, PAGE_SIZE, PKVM_ID_HOST));
end:
	guest_unlock_component(vm);
	host_unlock_component();

	*ppa = phys;

	return ret;
}

int __pkvm_prot_finalize(void)
{
	struct kvm_s2_mmu *mmu = &host_mmu.arch.mmu;
	struct kvm_nvhe_init_params *params = this_cpu_ptr(&kvm_init_params);

	if (params->hcr_el2 & HCR_VM)
		return -EPERM;

	params->vttbr = kvm_get_vttbr(mmu);
	params->vtcr = mmu->vtcr;
	params->hcr_el2 |= HCR_VM;

	/*
	 * The CMO below not only cleans the updated params to the
	 * PoC, but also provides the DSB that ensures ongoing
	 * page-table walks that have started before we trapped to EL2
	 * have completed.
	 */
	kvm_flush_dcache_to_poc(params, sizeof(*params));

	write_sysreg(params->hcr_el2, hcr_el2);
	__load_stage2(&host_mmu.arch.mmu, &host_mmu.arch);

	/*
	 * Make sure to have an ISB before the TLB maintenance below but only
	 * when __load_stage2() doesn't include one already.
	 */
	asm(ALTERNATIVE("isb", "nop", ARM64_WORKAROUND_SPECULATIVE_AT));

	/* Invalidate stale HCR bits that may be cached in TLBs */
	__tlbi(vmalls12e1);
	dsb(nsh);
	isb();

	__pkvm_close_module_registration();

	return 0;
}

int host_stage2_unmap_reg_locked(phys_addr_t start, u64 size)
{
	int ret;

	hyp_assert_lock_held(&host_mmu.lock);

	ret = kvm_pgtable_stage2_reclaim_leaves(&host_mmu.pgt, start, size);
	if (ret)
		return ret;

	kvm_iommu_host_stage2_idmap(start, start + size, 0);
	kvm_iommu_host_stage2_idmap_complete(false);

	return 0;
}

static int host_stage2_unmap_unmoveable_regs(void)
{
	struct kvm_pgtable *pgt = &host_mmu.pgt;
	struct pkvm_moveable_reg *reg;
	u64 addr = 0;
	int i, ret;

	/* Unmap all unmoveable regions to recycle the pages */
	for (i = 0; i < pkvm_moveable_regs_nr; i++) {
		reg = &pkvm_moveable_regs[i];
		if (reg->start > addr) {
			ret = host_stage2_unmap_reg_locked(addr, reg->start - addr);
			if (ret)
				return ret;
		}
		addr = max(addr, reg->start + reg->size);
	}
	return host_stage2_unmap_reg_locked(addr, BIT(pgt->ia_bits) - addr);
}

struct kvm_mem_range {
	u64 start;
	u64 end;
};

static struct memblock_region *find_mem_range(phys_addr_t addr, struct kvm_mem_range *range)
{
	int cur, left = 0, right = hyp_memblock_nr;
	struct memblock_region *reg;
	phys_addr_t end;

	range->start = 0;
	range->end = ULONG_MAX;

	/* The list of memblock regions is sorted, binary search it */
	while (left < right) {
		cur = (left + right) >> 1;
		reg = &hyp_memory[cur];
		end = reg->base + reg->size;
		if (addr < reg->base) {
			right = cur;
			range->end = reg->base;
		} else if (addr >= end) {
			left = cur + 1;
			range->start = end;
		} else {
			range->start = reg->base;
			range->end = end;
			return reg;
		}
	}

	return NULL;
}

static enum kvm_pgtable_prot default_host_prot(bool is_memory)
{
	return is_memory ? PKVM_HOST_MEM_PROT : PKVM_HOST_MMIO_PROT;
}

static enum kvm_pgtable_prot default_hyp_prot(phys_addr_t phys)
{
	return addr_is_memory(phys) ? PAGE_HYP : PAGE_HYP_DEVICE;
}

/*
 * Use NORMAL_NC for guest MMIO, when a guest has:
 * No FWB: It will combined with stage-1 attrs where device has precedence over normal.
 * FWB: With MT_S2_FWB_NORMAL_NC encoding, results in device if stage-1 used device attr.
 *      otherwise NC.
 */
static enum kvm_pgtable_prot default_guest_prot(bool is_memory)
{
	return is_memory ? KVM_PGTABLE_PROT_RWX :
		KVM_PGTABLE_PROT_RW | KVM_PGTABLE_PROT_NORMAL_NC;
}

bool addr_is_memory(phys_addr_t phys)
{
	struct kvm_mem_range range;

	return !!find_mem_range(phys, &range);
}

static bool is_in_mem_range(u64 addr, struct kvm_mem_range *range)
{
	return range->start <= addr && addr < range->end;
}

static int check_range_allowed_memory(u64 start, u64 end)
{
	struct memblock_region *reg;
	struct kvm_mem_range range;

	/*
	 * Callers can't check the state of a range that overlaps memory and
	 * MMIO regions, so ensure [start, end[ is in the same kvm_mem_range.
	 */
	reg = find_mem_range(start, &range);
	if (!is_in_mem_range(end - 1, &range))
		return -EINVAL;

	if (!reg || reg->flags & MEMBLOCK_NOMAP)
		return -EPERM;

	return 0;
}

static bool range_is_memory(u64 start, u64 end)
{
	struct kvm_mem_range r;

	if (!find_mem_range(start, &r))
		return false;

	return is_in_mem_range(end - 1, &r);
}

static bool range_is_allowed_memory(u64 start, u64 end)
{
	struct memblock_region *reg;
	struct kvm_mem_range range;

	reg = find_mem_range(start, &range);
	if (!reg)
		return false;

	if (!is_in_mem_range(end - 1, &range))
		return false;

	return !(reg->flags & MEMBLOCK_NOMAP);
}

static inline int __host_stage2_idmap(u64 start, u64 end,
				      enum kvm_pgtable_prot prot,
				      bool update_iommu)
{
	int ret;

	ret = kvm_pgtable_stage2_map(&host_mmu.pgt, start, end - start, start,
				     prot, &host_s2_pool, 0);
	if (ret)
		return ret;

	if (update_iommu) {
		kvm_iommu_host_stage2_idmap(start, end, prot);
		kvm_iommu_host_stage2_idmap_complete(!!prot);
	}

	return 0;
}

/*
 * The pool has been provided with enough pages to cover all of moveable regions
 * with page granularity, but it is difficult to know how much of the
 * non-moveable regions we will need to cover upfront, so we may need to
 * 'recycle' the pages if we run out.
 */
#define host_stage2_try(fn, ...)					\
	({								\
		int __ret;						\
		hyp_assert_lock_held(&host_mmu.lock);			\
		__ret = fn(__VA_ARGS__);				\
		if (__ret == -ENOMEM) {					\
			__ret = host_stage2_unmap_unmoveable_regs();		\
			if (!__ret)					\
				__ret = fn(__VA_ARGS__);		\
		}							\
		__ret;							\
	 })

static inline bool range_included(struct kvm_mem_range *child,
				  struct kvm_mem_range *parent)
{
	return parent->start <= child->start && child->end <= parent->end;
}

static int host_stage2_adjust_range(u64 addr, struct kvm_mem_range *range)
{
	struct kvm_mem_range cur;
	kvm_pte_t pte;
	s8 level;
	int ret;

	hyp_assert_lock_held(&host_mmu.lock);
	ret = kvm_pgtable_get_leaf(&host_mmu.pgt, addr, &pte, &level);
	if (ret)
		return ret;

	if (kvm_pte_valid(pte))
		return -EAGAIN;

	if (pte) {
		WARN_ON(addr_is_memory(addr) &&
			!(hyp_phys_to_page(addr)->host_state & PKVM_NOPAGE));
		return -EPERM;
	}

	do {
		u64 granule = kvm_granule_size(level);
		cur.start = ALIGN_DOWN(addr, granule);
		cur.end = cur.start + granule;
		level++;
	} while ((level <= KVM_PGTABLE_LAST_LEVEL) &&
			!(kvm_level_supports_block_mapping(level) &&
			  range_included(&cur, range)));

	*range = cur;

	return 0;
}

int host_stage2_idmap_locked(phys_addr_t addr, u64 size,
			     enum kvm_pgtable_prot prot,
			     bool update_iommu)
{
	return host_stage2_try(__host_stage2_idmap, addr, addr + size, prot, update_iommu);
}

#define KVM_MAX_OWNER_ID               FIELD_MAX(KVM_INVALID_PTE_OWNER_MASK)

static kvm_pte_t kvm_init_invalid_leaf_owner(u8 owner_id)
{
	return FIELD_PREP(KVM_INVALID_PTE_OWNER_MASK, owner_id);
}

static void __host_update_page_state(phys_addr_t addr, u64 size, enum pkvm_page_state state)
{
	phys_addr_t end = addr + size;

	for (; addr < end; addr += PAGE_SIZE)
		hyp_phys_to_page(addr)->host_state = state;
}

static int __host_stage2_set_owner_locked(phys_addr_t addr, u64 size, u8 owner_id, bool is_memory,
					  enum pkvm_page_state nopage_state, bool update_iommu)
{
	kvm_pte_t annotation;
	enum kvm_pgtable_prot prot;
	int ret;

	if (owner_id > KVM_MAX_OWNER_ID)
		return -EINVAL;

	if (owner_id == PKVM_ID_HOST) {
		prot = default_host_prot(addr_is_memory(addr));
		ret = host_stage2_idmap_locked(addr, size, prot, false);
	} else {
		annotation = kvm_init_invalid_leaf_owner(owner_id);
		ret = host_stage2_try(kvm_pgtable_stage2_annotate,
				      &host_mmu.pgt,
				      addr, size, &host_s2_pool, annotation);
	}
	if (ret)
		return ret;

	if (update_iommu) {
		prot = owner_id == PKVM_ID_HOST ? PKVM_HOST_MEM_PROT : 0;
		kvm_iommu_host_stage2_idmap(addr, addr + size, prot);
		kvm_iommu_host_stage2_idmap_complete(!!prot);
	}

	if (!is_memory)
		return 0;

	/* Don't forget to update the vmemmap tracking for the host */
	if (owner_id == PKVM_ID_HOST)
		__host_update_page_state(addr, size, PKVM_PAGE_OWNED);
	else
		__host_update_page_state(addr, size, PKVM_NOPAGE | nopage_state);

	return 0;
}

int host_stage2_set_owner_locked(phys_addr_t addr, u64 size, u8 owner_id)
{
	return __host_stage2_set_owner_locked(addr, size, owner_id, addr_is_memory(addr), 0, true);
}

static bool host_stage2_force_pte(u64 addr, u64 end, enum kvm_pgtable_prot prot)
{
	/*
	 * Block mappings must be used with care in the host stage-2 as a
	 * kvm_pgtable_stage2_map() operation targeting a page in the range of
	 * an existing block will delete the block under the assumption that
	 * mappings in the rest of the block range can always be rebuilt lazily.
	 * That assumption is correct for the host stage-2 with RWX mappings
	 * targeting memory or RW mappings targeting MMIO ranges (see
	 * host_stage2_idmap() below which implements some of the host memory
	 * abort logic). However, this is not safe for any other mappings where
	 * the host stage-2 page-table is in fact the only place where this
	 * state is stored. In all those cases, it is safer to use page-level
	 * mappings, hence avoiding to lose the state because of side-effects in
	 * kvm_pgtable_stage2_map().
	 */
	return prot != default_host_prot(range_is_memory(addr, end));
}

static bool host_stage2_pte_is_counted(kvm_pte_t pte, u32 level)
{
	u64 phys;

	if (!kvm_pte_valid(pte))
		return !!pte;

	if (kvm_pte_table(pte, level))
		return true;

	phys = kvm_pte_to_phys(pte);
	if (addr_is_memory(phys))
		return (pte & KVM_HOST_S2_DEFAULT_MASK) !=
			KVM_HOST_S2_DEFAULT_MEM_PTE;

	return (pte & KVM_HOST_S2_DEFAULT_MASK) != KVM_HOST_S2_DEFAULT_MMIO_PTE;
}

static int host_stage2_idmap(u64 addr)
{
	struct kvm_mem_range range;
	bool is_memory = !!find_mem_range(addr, &range);
	enum kvm_pgtable_prot prot = default_host_prot(is_memory);
	int ret;
	bool update_iommu = !is_memory;

	host_lock_component();
	ret = host_stage2_adjust_range(addr, &range);
	if (ret)
		goto unlock;

	ret = host_stage2_idmap_locked(range.start, range.end - range.start, prot, update_iommu);
unlock:
	host_unlock_component();

	return ret;
}

static void (*illegal_abt_notifier)(struct user_pt_regs *regs);

int __pkvm_register_illegal_abt_notifier(void (*cb)(struct user_pt_regs *))
{
	return cmpxchg(&illegal_abt_notifier, NULL, cb) ? -EBUSY : 0;
}

static void host_inject_abort(struct kvm_cpu_context *host_ctxt)
{
	u64 spsr = read_sysreg_el2(SYS_SPSR);
	u64 esr = read_sysreg_el2(SYS_ESR);
	u64 ventry, ec;

	if (READ_ONCE(illegal_abt_notifier))
		illegal_abt_notifier(&host_ctxt->regs);

	/* Repaint the ESR to report a same-level fault if taken from EL1 */
	if ((spsr & PSR_MODE_MASK) != PSR_MODE_EL0t) {
		ec = ESR_ELx_EC(esr);
		if (ec == ESR_ELx_EC_DABT_LOW)
			ec = ESR_ELx_EC_DABT_CUR;
		else if (ec == ESR_ELx_EC_IABT_LOW)
			ec = ESR_ELx_EC_IABT_CUR;
		else
			WARN_ON(1);
		esr &= ~ESR_ELx_EC_MASK;
		esr |= ec << ESR_ELx_EC_SHIFT;
	}

	/*
	 * Since S1PTW should only ever be set for stage-2 faults, we're pretty
	 * much guaranteed that it won't be set in ESR_EL1 by the hardware. So,
	 * let's use that bit to allow the host abort handler to differentiate
	 * this abort from normal userspace faults.
	 *
	 * Note: although S1PTW is RES0 at EL1, it is guaranteed by the
	 * architecture to be backed by flops, so it should be safe to use.
	 */
	esr |= ESR_ELx_S1PTW;

	write_sysreg_el1(esr, SYS_ESR);
	write_sysreg_el1(spsr, SYS_SPSR);
	write_sysreg_el1(read_sysreg_el2(SYS_ELR), SYS_ELR);
	write_sysreg_el1(read_sysreg_el2(SYS_FAR), SYS_FAR);

	ventry = read_sysreg_el1(SYS_VBAR);
	ventry += get_except64_offset(spsr, PSR_MODE_EL1h, except_type_sync);
	write_sysreg_el2(ventry, SYS_ELR);

	spsr = get_except64_cpsr(spsr, system_supports_mte(),
				 read_sysreg_el1(SYS_SCTLR), PSR_MODE_EL1h);
	write_sysreg_el2(spsr, SYS_SPSR);
}

static bool is_dabt(u64 esr)
{
	return ESR_ELx_EC(esr) == ESR_ELx_EC_DABT_LOW;
}

void handle_host_mem_abort(struct kvm_cpu_context *host_ctxt)
{
	struct kvm_vcpu_fault_info fault;
	u64 esr, addr;
	int ret;

	esr = read_sysreg_el2(SYS_ESR);
	if (!__get_fault_info(esr, &fault)) {
		/* Setting the address to an invalid value for use in tracing. */
		addr = (u64)-1;
		/*
		 * We've presumably raced with a page-table change which caused
		 * AT to fail, try again.
		 */
		return;
	}

	addr = (fault.hpfar_el2 & HPFAR_MASK) << 8;
	addr |= fault.far_el2 & FAR_MASK;

	if (is_dabt(esr) && !addr_is_memory(addr) &&
	    kvm_iommu_host_dabt_handler(host_ctxt, esr, addr))
		goto return_to_host;

	switch (esr & ESR_ELx_FSC_TYPE) {
	case ESR_ELx_FSC_FAULT:
		ret = host_stage2_idmap(addr);
		break;
	case ESR_ELx_FSC_PERM:
		ret = module_handle_host_perm_fault(&host_ctxt->regs, esr, addr);
		ret = ret ? 0 /* handled */ : -EPERM;
		break;
	default:
		ret = -EPERM;
		break;
	}

	if (ret == -EPERM)
		host_inject_abort(host_ctxt);
	else
		BUG_ON(ret && ret != -EAGAIN);

return_to_host:
	trace_host_mem_abort(esr, addr);
}

struct check_walk_data {
	enum pkvm_page_state	desired;
	enum pkvm_page_state	(*get_page_state)(kvm_pte_t pte, u64 addr);
};

static int __check_page_state_visitor(const struct kvm_pgtable_visit_ctx *ctx,
				      enum kvm_pgtable_walk_flags visit)
{
	struct check_walk_data *d = ctx->arg;

	return d->get_page_state(ctx->old, ctx->addr) == d->desired ? 0 : -EPERM;
}

static int check_page_state_range(struct kvm_pgtable *pgt, u64 addr, u64 size,
				  struct check_walk_data *data)
{
	struct kvm_pgtable_walker walker = {
		.cb	= __check_page_state_visitor,
		.arg	= data,
		.flags	= KVM_PGTABLE_WALK_LEAF,
	};

	return kvm_pgtable_walk(pgt, addr, size, &walker);
}

static enum pkvm_page_state host_get_mmio_page_state(kvm_pte_t pte, u64 addr)
{
	enum pkvm_page_state state = 0;
	enum kvm_pgtable_prot prot;

	WARN_ON(addr_is_memory(addr));

	if (!kvm_pte_valid(pte) && pte)
		return PKVM_NOPAGE;

	prot = kvm_pgtable_stage2_pte_prot(pte);
	if (kvm_pte_valid(pte)) {
		if ((prot & KVM_PGTABLE_PROT_RWX) != PKVM_HOST_MMIO_PROT)
			state = PKVM_PAGE_RESTRICTED_PROT;
	}

	return state | pkvm_getstate(prot);
}

static int ___host_check_page_state_range(u64 addr, u64 size,
					  enum pkvm_page_state state,
					  struct memblock_region *reg,
					  bool check_null_refcount)
{
	struct check_walk_data d = {
		.desired	= state,
		.get_page_state	= host_get_mmio_page_state,
	};
	u64 end = addr + size;
	struct hyp_page *p;

	hyp_assert_lock_held(&host_mmu.lock);

	/* MMIO state is still in the page-table */
	if (!reg)
		return check_page_state_range(&host_mmu.pgt, addr, size, &d);

	if (reg->flags & MEMBLOCK_NOMAP)
		return -EPERM;

	for (; addr < end; addr += PAGE_SIZE) {
		p = hyp_phys_to_page(addr);
		if (p->host_state != state)
			return -EPERM;
		if (check_null_refcount && hyp_refcount_get(p->refcount))
			return -EINVAL;
	}

	/*
	 * All memory pages with restricted permissions will already be covered
	 * by other states (e.g. PKVM_MODULE_OWNED_PAGE), so no need to retrieve
	 * the PKVM_PAGE_RESTRICTED_PROT state from the PTE.
	 */

	return 0;
}

static int __host_check_page_state_range(u64 addr, u64 size,
					 enum pkvm_page_state state)
{
	struct memblock_region *reg;
	struct kvm_mem_range range;
	u64 end = addr + size;

	/* Can't check the state of both MMIO and memory regions at once */
	reg = find_mem_range(addr, &range);
	if (!is_in_mem_range(end - 1, &range))
		return -EINVAL;

	/* Check the refcount of PAGE_OWNED pages as those may be used for DMA. */
	return ___host_check_page_state_range(addr, size, state, reg, state == PKVM_PAGE_OWNED);
}

static int __host_set_page_state_range(u64 addr, u64 size,
				       enum pkvm_page_state state)
{
	if (hyp_phys_to_page(addr)->host_state & PKVM_NOPAGE) {
		int ret = host_stage2_idmap_locked(addr, size, PKVM_HOST_MEM_PROT, true);

		if (ret)
			return ret;
	}

	__host_update_page_state(addr, size, state);

	return 0;
}

static enum pkvm_page_state hyp_get_page_state(kvm_pte_t pte, u64 addr)
{
	enum pkvm_page_state state = 0;
	enum kvm_pgtable_prot prot;

	if (!kvm_pte_valid(pte))
		return PKVM_NOPAGE;

	prot = kvm_pgtable_hyp_pte_prot(pte);
	if (kvm_pte_valid(pte) && ((prot & KVM_PGTABLE_PROT_RWX) != PAGE_HYP))
		state = PKVM_PAGE_RESTRICTED_PROT;

	return state | pkvm_getstate(prot);
}

static int __hyp_check_page_state_range(u64 addr, u64 size,
					enum pkvm_page_state state)
{
	struct check_walk_data d = {
		.desired	= state,
		.get_page_state	= hyp_get_page_state,
	};

	hyp_assert_lock_held(&pkvm_pgd_lock);
	return check_page_state_range(&pkvm_pgtable, addr, size, &d);
}

int hyp_check_range_owned(u64 phys_addr, u64 size)
{
	int ret;

	hyp_lock_component();
	ret = __hyp_check_page_state_range((u64)hyp_phys_to_virt(phys_addr),
					   size, PKVM_PAGE_OWNED);
	hyp_unlock_component();

	return ret;
}

static int __guest_check_page_state_range(struct pkvm_hyp_vcpu *vcpu, u64 addr,
					  u64 size, enum pkvm_page_state state)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	struct check_walk_data d = {
		.desired	= state,
		.get_page_state	= guest_get_page_state,
	};

	hyp_assert_lock_held(&vm->pgtable_lock);
	return check_page_state_range(&vm->pgt, addr, size, &d);
}

struct guest_request_walker_data {
	unsigned long		ipa_start;
	kvm_pte_t		pte_start;
	u64			size;
	enum pkvm_page_state	desired_state;
	enum pkvm_page_state	desired_mask;
	int			max_ptes;
};

#define GUEST_WALKER_DATA_INIT(__state)			\
{							\
	.size		= 0,				\
	.desired_state	= __state,			\
	.desired_mask	= ~0,				\
	/*						\
	 * Arbitrary limit of walked PTEs to restrict	\
	 * the time spent at EL2			\
	 */						\
	.max_ptes	= 512,				\
}

static int guest_request_walker(const struct kvm_pgtable_visit_ctx *ctx,
				enum kvm_pgtable_walk_flags visit)
{
	struct guest_request_walker_data *data = (struct guest_request_walker_data *)ctx->arg;
	enum pkvm_page_state state;
	kvm_pte_t pte = *ctx->ptep;
	phys_addr_t phys = kvm_pte_to_phys(pte);
	u32 level = ctx->level;

	state = guest_get_page_state(pte, 0);
	if (data->desired_state != (state & data->desired_mask))
		return (state & PKVM_NOPAGE) ? -EFAULT : -EPERM;

	data->max_ptes--;

	if (!data->size) {
		data->pte_start = pte;
		data->size = kvm_granule_size(level);
		data->ipa_start = ctx->addr & ~(kvm_granule_size(level) - 1);

		goto end;
	}

	if (kvm_pgtable_stage2_pte_prot(pte) !=
	    kvm_pgtable_stage2_pte_prot(data->pte_start))
		return -EINVAL;

	/* Can only describe physically contiguous mappings */
	if (kvm_pte_valid(data->pte_start) &&
	    (phys != kvm_pte_to_phys(data->pte_start) + data->size))
			return -E2BIG;

	data->size += kvm_granule_size(level);

end:
	return --data->max_ptes > 0 ? 0 : -E2BIG;
}

static int __guest_request_page_transition(u64 ipa, kvm_pte_t *__pte, u64 *__nr_pages,
					   struct pkvm_hyp_vcpu *vcpu,
					   enum pkvm_page_state desired)
{
	struct guest_request_walker_data data = GUEST_WALKER_DATA_INIT(desired);
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	struct kvm_pgtable_walker walker = {
		.cb     = guest_request_walker,
		.flags  = KVM_PGTABLE_WALK_LEAF,
		.arg    = (void *)&data,
	};
	phys_addr_t phys, phys_offset;
	kvm_pte_t pte;
	int ret = kvm_pgtable_walk(&vm->pgt, ipa, *__nr_pages * PAGE_SIZE, &walker);

	/* Walker reached data.max_ptes or a non physically contiguous block */
	if (ret == -E2BIG)
		ret = 0;
	else if (ret)
		return ret;

	if (WARN_ON(!kvm_pte_valid(data.pte_start)))
		return -EINVAL;

	phys = kvm_pte_to_phys(data.pte_start);
	if (!range_is_allowed_memory(phys, phys + data.size))
		return -EINVAL;

	if (data.ipa_start > ipa)
		return -EINVAL;

	/*
	 * transition not aligned with block memory mapping. They'll be broken
	 * down and memory donation will be needed.
	 */
	phys_offset = ipa - data.ipa_start;
	if (phys_offset || (*__nr_pages * PAGE_SIZE < data.size)) {
		struct pkvm_hyp_vcpu *hyp_vcpu = pkvm_get_loaded_hyp_vcpu();
		int min_pages;

		if (WARN_ON(!hyp_vcpu))
			return -EINVAL;

		min_pages = kvm_mmu_cache_min_pages(&hyp_vcpu->vcpu.kvm->arch.mmu);
		if (hyp_vcpu->vcpu.arch.stage2_mc.nr_pages < min_pages)
			return -ENOMEM;
	}

	phys = kvm_pte_to_phys(data.pte_start) + phys_offset;
	pte = data.pte_start & ~kvm_phys_to_pte(KVM_PHYS_INVALID);
	pte |= kvm_phys_to_pte(phys);

	if (WARN_ON(phys_offset >= data.size))
		return -EINVAL;

	*__pte = pte;
	*__nr_pages = min_t(u64, (data.size - phys_offset) >> PAGE_SHIFT,
			    *__nr_pages);

	return 0;
}

static int __guest_initiate_page_transition(u64 ipa, kvm_pte_t pte, u64 nr_pages,
					    struct pkvm_hyp_vcpu *vcpu,
					    enum pkvm_page_state state)
{
	struct kvm_hyp_memcache *mc = &vcpu->vcpu.arch.stage2_mc;
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	u64 size = nr_pages * PAGE_SIZE;
	enum kvm_pgtable_prot prot;
	u64 phys;
	int ret;

	phys = kvm_pte_to_phys(pte);
	prot = pkvm_mkstate(kvm_pgtable_stage2_pte_prot(pte), state);
	ret = kvm_pgtable_stage2_map(&vm->pgt, ipa, size, phys, prot, mc, 0);
	if (ret)
		return ret;

	return 0;
}

int __pkvm_host_share_hyp(u64 pfn)
{
	u64 phys = hyp_pfn_to_phys(pfn);
	void *virt = __hyp_va(phys);
	enum kvm_pgtable_prot prot;
	u64 size = PAGE_SIZE;
	int ret;

	host_lock_component();
	hyp_lock_component();

	ret = __host_check_page_state_range(phys, size, PKVM_PAGE_OWNED);
	if (ret)
		goto unlock;
	if (IS_ENABLED(CONFIG_PKVM_STRICT_CHECKS)) {
		ret = __hyp_check_page_state_range((u64)virt, size, PKVM_NOPAGE);
		if (ret)
			goto unlock;
	}

	prot = pkvm_mkstate(PAGE_HYP, PKVM_PAGE_SHARED_BORROWED);
	ret = pkvm_create_mappings_locked(virt, virt + size, prot);
	if (ret) {
		WARN_ON(ret != -ENOMEM);
		/* We might have failed halfway through, so remove anything we've installed */
		pkvm_remove_mappings_locked(virt, virt + size);
		goto unlock;
	}

	WARN_ON(__host_set_page_state_range(phys, size, PKVM_PAGE_SHARED_OWNED));

unlock:
	hyp_unlock_component();
	host_unlock_component();
	return ret;
}

int __pkvm_host_unshare_hyp(u64 pfn)
{
	u64 phys = hyp_pfn_to_phys(pfn);
	u64 virt = (u64)__hyp_va(phys);
	u64 size = PAGE_SIZE;
	int ret;

	host_lock_component();
	hyp_lock_component();

	ret = __host_check_page_state_range(phys, size, PKVM_PAGE_SHARED_OWNED);
	if (ret)
		goto unlock;
	ret = __hyp_check_page_state_range(virt, size, PKVM_PAGE_SHARED_BORROWED);
	if (ret)
		goto unlock;
	if (hyp_page_count((void *)virt)) {
		ret = -EBUSY;
		goto unlock;
	}

	WARN_ON(kvm_pgtable_hyp_unmap(&pkvm_pgtable, virt, size) != size);
	WARN_ON(__host_set_page_state_range(phys, size, PKVM_PAGE_OWNED));

unlock:
	hyp_unlock_component();
	host_unlock_component();

	return ret;
}

int __pkvm_guest_share_host(struct pkvm_hyp_vcpu *vcpu, u64 ipa, u64 nr_pages,
			    u64 *nr_shared)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	kvm_pte_t pte;
	size_t size;
	u64 phys;
	int ret;

	host_lock_component();
	guest_lock_component(vm);

	ret = __guest_request_page_transition(ipa, &pte, &nr_pages, vcpu, PKVM_PAGE_OWNED);
	if (ret)
		goto unlock;

	phys = kvm_pte_to_phys(pte);
	if (check_shl_overflow(nr_pages, PAGE_SHIFT, &size)) {
		ret = -EINVAL;
		goto unlock;
	}

	ret = __host_check_page_state_range(phys, size, PKVM_NOPAGE);
	if (ret)
		goto unlock;

	WARN_ON(__guest_initiate_page_transition(ipa, pte, nr_pages, vcpu, PKVM_PAGE_SHARED_OWNED));
	WARN_ON(__host_set_page_state_range(phys, size, PKVM_PAGE_SHARED_BORROWED));
	psci_mem_protect_dec(nr_pages);
	*nr_shared = nr_pages;

unlock:
	guest_unlock_component(vm);
	host_unlock_component();

	return ret;
}

int __pkvm_guest_share_hyp_page(struct pkvm_hyp_vcpu *vcpu, u64 ipa, u64 *hyp_va)
{
	int ret;
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	kvm_pte_t pte;
	u64 phys;
	enum kvm_pgtable_prot prot;
	void *virt;
	u64 nr_pages = 1;

	hyp_lock_component();
	guest_lock_component(vm);

	ret = __guest_request_page_transition(ipa, &pte, &nr_pages, vcpu, PKVM_PAGE_OWNED);
	if (ret)
		goto unlock;

	phys = kvm_pte_to_phys(pte);

	virt = __hyp_va(phys);
	if (IS_ENABLED(CONFIG_NVHE_EL2_DEBUG)) {
		ret = __hyp_check_page_state_range((u64)virt, PAGE_SIZE, PKVM_NOPAGE);
		if (ret)
			goto unlock;
	}

	prot = pkvm_mkstate(PAGE_HYP, PKVM_PAGE_SHARED_BORROWED);
	ret = pkvm_create_mappings_locked(virt, virt + PAGE_SIZE, prot);
	if (ret) {
		/*
		 * Repaint the return code as we need to distinguish between the
		 * no memory from the guest which is recoverable and no memory
		 * from the hypervisor.
		 */
		if (ret == -ENOMEM)
			ret = -EBUSY;
		goto unlock;
	}

	WARN_ON(__guest_initiate_page_transition(ipa, pte, nr_pages, vcpu, PKVM_PAGE_SHARED_OWNED));
	*hyp_va = (u64)virt;
unlock:
	guest_unlock_component(vm);
	hyp_unlock_component();

	return ret;
}

int __pkvm_guest_unshare_hyp_page(struct pkvm_hyp_vcpu *vcpu, u64 ipa)
{
	int ret;
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	kvm_pte_t pte;
	u64 phys, virt, nr_pages = 1;

	hyp_lock_component();
	guest_lock_component(vm);

	ret = __guest_request_page_transition(ipa, &pte, &nr_pages, vcpu, PKVM_PAGE_SHARED_OWNED);
	if (ret)
		goto unlock;

	phys = kvm_pte_to_phys(pte);

	virt = (u64)__hyp_va(phys);
	ret = __hyp_check_page_state_range(virt, PAGE_SIZE, PKVM_PAGE_SHARED_BORROWED);
	if (ret)
		goto unlock;

	WARN_ON(kvm_pgtable_hyp_unmap(&pkvm_pgtable, virt, PAGE_SIZE) != PAGE_SIZE);
	WARN_ON(__guest_initiate_page_transition(ipa, pte, nr_pages, vcpu, PKVM_PAGE_OWNED));
unlock:
	guest_unlock_component(vm);
	hyp_unlock_component();

	return ret;
}

int __pkvm_guest_unshare_host(struct pkvm_hyp_vcpu *vcpu, u64 ipa, u64 nr_pages,
			      u64 *nr_unshared)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	kvm_pte_t pte;
	size_t size;
	u64 phys;
	int ret;

	host_lock_component();
	guest_lock_component(vm);

	ret = __guest_request_page_transition(ipa, &pte, &nr_pages, vcpu, PKVM_PAGE_SHARED_OWNED);
	if (ret)
		goto unlock;

	phys = kvm_pte_to_phys(pte);
	if (check_shl_overflow(nr_pages, PAGE_SHIFT, &size)) {
		ret = -EINVAL;
		goto unlock;
	}

	ret = __host_check_page_state_range(phys, size, PKVM_PAGE_SHARED_BORROWED);
	if (ret)
		goto unlock;

	WARN_ON(__guest_initiate_page_transition(ipa, pte, nr_pages, vcpu, PKVM_PAGE_OWNED));
	psci_mem_protect_inc(nr_pages);
	WARN_ON(host_stage2_set_owner_locked(phys, size, PKVM_ID_GUEST));
	*nr_unshared = nr_pages;

unlock:
	guest_unlock_component(vm);
	host_unlock_component();

	return ret;
}

int __pkvm_guest_share_ffa_page(struct pkvm_hyp_vcpu *vcpu, u64 ipa, phys_addr_t *phys)
{
	int ret;
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	kvm_pte_t pte;
	u64 nr_pages = 1;

	guest_lock_component(vm);
	ret = __guest_request_page_transition(ipa, &pte, &nr_pages, vcpu, PKVM_PAGE_OWNED);
	if (ret)
		goto unlock;

	ret = __guest_initiate_page_transition(ipa, pte, nr_pages, vcpu, PKVM_PAGE_SHARED_OWNED);
	if (!ret)
		*phys = kvm_pte_to_phys(pte);
unlock:
	guest_unlock_component(vm);

	return ret;
}

/*
 * The caller is responsible for tracking the FFA state and this function
 * should only be called for IPAs that have previously been shared with FFA.
 */
int __pkvm_guest_unshare_ffa_page(struct pkvm_hyp_vcpu *vcpu, u64 ipa)
{
	int ret;
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	kvm_pte_t pte;
	u64 nr_pages = 1;

	guest_lock_component(vm);
	ret = __guest_request_page_transition(ipa, &pte, &nr_pages, vcpu, PKVM_PAGE_SHARED_OWNED);
	if (ret)
		goto unlock;

	ret = __guest_initiate_page_transition(ipa, pte, nr_pages, vcpu, PKVM_PAGE_OWNED);
unlock:
	guest_unlock_component(vm);

	return ret;
}

int __pkvm_host_donate_hyp(u64 pfn, u64 nr_pages)
{
	return ___pkvm_host_donate_hyp(pfn, nr_pages, false);
}

/* The swiss knife of memory donation. */
int ___pkvm_host_donate_hyp_prot(u64 pfn, u64 nr_pages,
				 bool accept_mmio, enum kvm_pgtable_prot prot)
{
	phys_addr_t start = hyp_pfn_to_phys(pfn);
	phys_addr_t end = start + (nr_pages << PAGE_SHIFT);
	int ret;

	if (!accept_mmio && !range_is_memory(start, end))
		return -EPERM;

	host_lock_component();
	ret = __pkvm_host_donate_hyp_locked(pfn, nr_pages, prot);
	host_unlock_component();

	return ret;
}

int ___pkvm_host_donate_hyp(u64 pfn, u64 nr_pages, bool accept_mmio)
{
	return ___pkvm_host_donate_hyp_prot(pfn, nr_pages, accept_mmio,
					    default_hyp_prot(hyp_pfn_to_phys(pfn)));
}

static int pkvm_hyp_donate_guest(struct pkvm_hyp_vcpu *vcpu, u64 pfn, u64 gfn)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	u64 phys = hyp_pfn_to_phys(pfn);
	u64 ipa = hyp_pfn_to_phys(gfn);
	u64 hyp_addr = (u64)__hyp_va(phys);
	size_t size = PAGE_SIZE;
	enum kvm_pgtable_prot prot;
	int ret;

	hyp_assert_lock_held(&pkvm_pgd_lock);
	hyp_assert_lock_held(&vm->pgtable_lock);

	ret = __hyp_check_page_state_range(hyp_addr, size, PKVM_PAGE_OWNED);
	if (ret)
		return ret;;
	ret = __guest_check_page_state_range(vcpu, ipa, size, PKVM_NOPAGE);
	if (ret)
		return ret;

	WARN_ON(kvm_pgtable_hyp_unmap(&pkvm_pgtable, hyp_addr, size) != size);
	prot = pkvm_mkstate(default_guest_prot(addr_is_memory(phys)), PKVM_PAGE_OWNED);
	return WARN_ON(kvm_pgtable_stage2_map(&vm->pgt, ipa, size, phys, prot,
					      &vcpu->vcpu.arch.stage2_mc, 0));
}

int __pkvm_host_donate_hyp_locked(u64 pfn, u64 nr_pages, enum kvm_pgtable_prot prot)
{
	u64 size, phys = hyp_pfn_to_phys(pfn);
	void *virt = __hyp_va(phys);
	int ret;

	if (check_shl_overflow(nr_pages, PAGE_SHIFT, &size))
		return -EINVAL;

	hyp_lock_component();

	ret = __host_check_page_state_range(phys, size, PKVM_PAGE_OWNED);
	if (ret)
		goto unlock;
	if (IS_ENABLED(CONFIG_PKVM_STRICT_CHECKS)) {
		ret = __hyp_check_page_state_range((u64)virt, size, PKVM_NOPAGE);
		if (ret)
			goto unlock;
	}

	prot = pkvm_mkstate(prot, PKVM_PAGE_OWNED);
	ret = pkvm_create_mappings_locked(virt, virt + size, prot);
	if (ret) {
		WARN_ON(ret != -ENOMEM);
		/* We might have failed halfway through, so remove anything we've installed */
		pkvm_remove_mappings_locked(virt, virt + size);
		goto unlock;
	}
	WARN_ON(host_stage2_set_owner_locked(phys, size, PKVM_ID_HYP));

unlock:
	hyp_unlock_component();

	return ret;
}

int __pkvm_hyp_donate_host(u64 pfn, u64 nr_pages)
{
	u64 size, phys = hyp_pfn_to_phys(pfn);
	u64 virt = (u64)__hyp_va(phys);
	int ret;

	if (check_shl_overflow(nr_pages, PAGE_SHIFT, &size))
		return -EINVAL;

	host_lock_component();
	hyp_lock_component();

	ret = __hyp_check_page_state_range(virt, size, PKVM_PAGE_OWNED);
	if (ret)
		goto unlock;
	if (IS_ENABLED(CONFIG_PKVM_STRICT_CHECKS)) {
		ret = __host_check_page_state_range(phys, size, PKVM_NOPAGE);
		if (ret)
			goto unlock;
	}

	WARN_ON(kvm_pgtable_hyp_unmap(&pkvm_pgtable, virt, size) != size);
	WARN_ON(host_stage2_set_owner_locked(phys, size, PKVM_ID_HOST));

unlock:
	hyp_unlock_component();
	host_unlock_component();
	return ret;
}

int __pkvm_host_donate_ffa(u64 pfn, u64 nr_pages)
{
	u64 size, phys = hyp_pfn_to_phys(pfn), end;
	struct kvm_mem_range range;
	struct memblock_region *reg;
	int ret;

	if (check_shl_overflow(nr_pages, PAGE_SHIFT, &size) ||
	    check_add_overflow(phys, size, &end))
		return -EINVAL;

	reg = find_mem_range(phys, &range);
	if (!reg || !is_in_mem_range(end - 1, &range))
		return -EPERM;

	host_lock_component();

	ret = __host_check_page_state_range(phys, size, PKVM_PAGE_OWNED);
	if (ret)
		goto unlock;

	WARN_ON(host_stage2_set_owner_locked(phys, size, PKVM_ID_FFA));
unlock:
	host_unlock_component();
	return ret;
}

int __pkvm_host_reclaim_ffa(u64 pfn, u64 nr_pages)
{
	u64 size, phys = hyp_pfn_to_phys(pfn), end;
	struct memblock_region *reg;
	struct kvm_mem_range range;
	int ret;

	if (check_shl_overflow(nr_pages, PAGE_SHIFT, &size) ||
	    check_add_overflow(phys, size, &end))
		return -EINVAL;

	reg = find_mem_range(phys, &range);
	if (!reg || !is_in_mem_range(end - 1, &range))
		return -EPERM;

	host_lock_component();

	ret = __host_check_page_state_range(phys, size, PKVM_NOPAGE);
	if (ret)
		goto unlock;

	WARN_ON(host_stage2_set_owner_locked(phys, size, PKVM_ID_HOST));
unlock:
	host_unlock_component();
	return ret;
}

#define MODULE_PROT_ALLOWLIST (KVM_PGTABLE_PROT_RWX |		\
			       KVM_PGTABLE_PROT_DEVICE |	\
			       KVM_PGTABLE_PROT_NORMAL_NC |	\
			       KVM_PGTABLE_PROT_PXN |		\
			       KVM_PGTABLE_PROT_UXN)

int module_change_host_page_prot(u64 pfn, enum kvm_pgtable_prot prot, u64 nr_pages,
				 bool update_iommu)
{
	u64 i, end, addr = hyp_pfn_to_phys(pfn);
	struct hyp_page *page = NULL;
	struct kvm_mem_range range;
	struct memblock_region *reg;
	int ret;

	if ((prot & MODULE_PROT_ALLOWLIST) != prot)
		return -EINVAL;

	if (check_shl_overflow(nr_pages, PAGE_SHIFT, &end) ||
	    check_add_overflow(addr, end, &end))
		return -EINVAL;

	reg = find_mem_range(addr, &range);
	if (end > range.end) {
		/* Specified range not in a single mmio or memory block. */
		return -EPERM;
	}

	host_lock_component();

	/*
	 * There is no hyp_vmemmap covering MMIO regions, which makes tracking
	 * of module-owned MMIO regions hard, so we trust the modules not to
	 * mess things up.
	 */
	if (!reg)
		goto update;

	/* Range is memory: we can track module ownership. */
	page = hyp_phys_to_page(addr);

	/*
	 * Modules can only modify pages they already own, and pristine host
	 * pages. The entire range must be consistently one or the other.
	 */
	if (page->host_state & PKVM_MODULE_OWNED_PAGE) {
		/* The entire range must be module-owned. */
		ret = -EPERM;
		for (i = 1; i < nr_pages; i++) {
			if (!(page[i].host_state & PKVM_MODULE_OWNED_PAGE))
				goto unlock;
		}
	} else {
		/* The entire range must be pristine. */
		ret = ___host_check_page_state_range(
				addr, nr_pages << PAGE_SHIFT, PKVM_PAGE_OWNED, reg, true);
		if (ret)
			goto unlock;
	}

update:
	if (!prot) {
		ret = __host_stage2_set_owner_locked(addr, nr_pages << PAGE_SHIFT,
						     PKVM_ID_PROTECTED, !!reg,
						     PKVM_MODULE_OWNED_PAGE, update_iommu);
	} else {
		ret = host_stage2_idmap_locked(
			addr, nr_pages << PAGE_SHIFT, prot, update_iommu);
	}

	if (WARN_ON(ret) || !page || !prot)
		goto unlock;

	for (i = 0; i < nr_pages; i++) {
		if (prot != KVM_PGTABLE_PROT_RWX) {
			page[i].host_state = PKVM_MODULE_OWNED_PAGE;
		} else {
			page[i].host_state = PKVM_PAGE_OWNED;
		}
	}

unlock:
	host_unlock_component();

	return ret;
}

int hyp_pin_shared_mem(void *from, void *to)
{
	u64 cur, start = ALIGN_DOWN((u64)from, PAGE_SIZE);
	u64 end = PAGE_ALIGN((u64)to);
	u64 size = end - start;
	int ret;

	host_lock_component();
	hyp_lock_component();

	ret = __host_check_page_state_range(__hyp_pa(start), size,
					    PKVM_PAGE_SHARED_OWNED);
	if (ret)
		goto unlock;

	ret = __hyp_check_page_state_range(start, size,
					   PKVM_PAGE_SHARED_BORROWED);
	if (ret)
		goto unlock;

	for (cur = start; cur < end; cur += PAGE_SIZE)
		hyp_page_ref_inc(hyp_virt_to_page(cur));

unlock:
	hyp_unlock_component();
	host_unlock_component();

	return ret;
}

void hyp_unpin_shared_mem(void *from, void *to)
{
	u64 cur, start = ALIGN_DOWN((u64)from, PAGE_SIZE);
	u64 end = PAGE_ALIGN((u64)to);

	host_lock_component();
	hyp_lock_component();

	for (cur = start; cur < end; cur += PAGE_SIZE)
		hyp_page_ref_dec(hyp_virt_to_page(cur));

	hyp_unlock_component();
	host_unlock_component();
}

int __pkvm_host_share_ffa(u64 pfn, u64 nr_pages)
{

	u64 size, phys = hyp_pfn_to_phys(pfn);
	int ret;

	if (check_shl_overflow(nr_pages, PAGE_SHIFT, &size))
		return -EINVAL;

	host_lock_component();

	ret = __host_check_page_state_range(phys, size, PKVM_PAGE_OWNED);
	if (!ret)
		ret = __host_set_page_state_range(phys, size, PKVM_PAGE_SHARED_OWNED);

	host_unlock_component();

	return ret;
}

int __pkvm_host_unshare_ffa(u64 pfn, u64 nr_pages)
{
	u64 size, phys = hyp_pfn_to_phys(pfn);
	int ret;

	if (check_shl_overflow(nr_pages, PAGE_SHIFT, &size))
		return -EINVAL;

	host_lock_component();

	ret = __host_check_page_state_range(phys, size, PKVM_PAGE_SHARED_OWNED);
	if (!ret)
		ret = __host_set_page_state_range(phys, size, PKVM_PAGE_OWNED);

	host_unlock_component();

	return ret;
}

static void __pkvm_use_dma_page(phys_addr_t phys_addr)
{
	struct hyp_page *p = hyp_phys_to_page(phys_addr);

	hyp_page_ref_inc(p);
}

static void __pkvm_unuse_dma_page(phys_addr_t phys_addr)
{
	struct hyp_page *p = hyp_phys_to_page(phys_addr);

	hyp_page_ref_dec(p);
}

static int __pkvm_use_dma_locked(phys_addr_t phys_addr, size_t size,
				 struct pkvm_hyp_vcpu *hyp_vcpu)
{
	int i;
	int ret = 0;
	struct kvm_mem_range r;
	size_t nr_pages = size >> PAGE_SHIFT;
	struct memblock_region *reg = find_mem_range(phys_addr, &r);

	if (WARN_ON(!PAGE_ALIGNED(phys_addr | size)) || !is_in_mem_range(phys_addr + size - 1, &r))
		return -EINVAL;

	/*
	 * Some differences between handling of RAM and device memory:
	 * - The hyp vmemmap area for device memory is not backed by physical
	 *   pages in the hyp page tables.
	 * - However, in some cases modules can donate MMIO, as they can't be
	 *   refcounted, taint them by marking them as shared PKVM_PAGE_TAINTED, and that
	 *   will prevent any future transition.
	 */
	if (!reg) {
		enum kvm_pgtable_prot prot;

		if (hyp_vcpu)
			return -EINVAL;

		for (i = 0; i < nr_pages; i++) {
			u64 addr = phys_addr + i * PAGE_SIZE;

			ret = ___host_check_page_state_range(addr, PAGE_SIZE,
							     PKVM_PAGE_TAINTED,
							     reg, false);
			/* Page already tainted */
			if (!ret)
				continue;
			ret = ___host_check_page_state_range(addr, PAGE_SIZE,
							     PKVM_PAGE_OWNED,
							     reg, false);
			if (ret)
				return ret;
		}
		prot = pkvm_mkstate(PKVM_HOST_MMIO_PROT, PKVM_PAGE_TAINTED);
		WARN_ON(host_stage2_idmap_locked(phys_addr, size, prot, false));
	} else {
		/* For VMs, we know if we reach this point the VM has access to the page. */
		if (!hyp_vcpu) {
			for (i = 0; i < nr_pages; i++) {
				enum pkvm_page_state state;
				phys_addr_t this_addr = phys_addr + i * PAGE_SIZE;

				state = hyp_phys_to_page(this_addr)->host_state;
				if (state != PKVM_PAGE_OWNED) {
					ret = -EPERM;
					break;
				}
			}
			if (ret)
				return ret;
		}

		for (i = 0; i < nr_pages; i++)
			__pkvm_use_dma_page(phys_addr + i * PAGE_SIZE);
	}

	return ret;
}

/*
 * __pkvm_use_dma - Mark memory as used for DMA
 * @phys_addr:	physical address of the DMA region
 * @size:	size of the DMA region
 * When a page is mapped in an IOMMU page table for DMA, it must
 * not be donated to a guest or the hypervisor we ensure this with:
 * - Host can only map pages that are OWNED
 * - Any page that is mapped is refcounted
 * - Donation/Sharing is prevented from refcount check in
 *   ___host_check_page_state_range()
 * - No MMIO transtion is allowed beyond IOMMU MMIO which
 *   happens during de-privilege.
 * In case in the future shared pages are allowed to be mapped,
 * similar checks are needed in host_request_unshare() and
 * host_ack_unshare()
 */
int __pkvm_use_dma(phys_addr_t phys_addr, size_t size, struct pkvm_hyp_vcpu *hyp_vcpu)
{
	int ret;

	host_lock_component();
	ret = __pkvm_use_dma_locked(phys_addr, size, hyp_vcpu);
	host_unlock_component();
	return ret;
}

int __pkvm_unuse_dma(phys_addr_t phys_addr, size_t size, struct pkvm_hyp_vcpu *hyp_vcpu)
{
	int i;
	size_t nr_pages = size >> PAGE_SHIFT;

	if (WARN_ON(!PAGE_ALIGNED(phys_addr | size)))
		return -EINVAL;
	if (!range_is_memory(phys_addr, phys_addr + size)) {
		WARN_ON(hyp_vcpu);
		return 0;
	}

	host_lock_component();
	/*
	 * We end up here after the caller successfully unmapped the page from
	 * the IOMMU table. Which means that a ref is held, the page is shared
	 * in the host s2, there can be no failure.
	 */
	for (i = 0; i < nr_pages; i++)
		__pkvm_unuse_dma_page(phys_addr + i * PAGE_SIZE);

	host_unlock_component();
	return 0;
}

int __pkvm_host_share_guest(u64 pfn, u64 gfn, struct pkvm_hyp_vcpu *vcpu,
			    enum kvm_pgtable_prot prot, u64 nr_pages)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	u64 phys = hyp_pfn_to_phys(pfn);
	u64 ipa = hyp_pfn_to_phys(gfn);
	struct hyp_page *page;
	size_t size;
	u64 end;
	int ret;

	if (prot & ~KVM_PGTABLE_PROT_RWX)
		return -EINVAL;

	if (check_shl_overflow(nr_pages, PAGE_SHIFT, &size))
		return -EINVAL;

	end = phys + size;
	ret = check_range_allowed_memory(phys, end);
	if (ret)
		return ret;

	host_lock_component();
	guest_lock_component(vm);

	ret = __guest_check_page_state_range(vcpu, ipa, size, PKVM_NOPAGE);
	if (ret)
		goto unlock;

	for (; phys < end; phys += PAGE_SIZE) {
		page = hyp_phys_to_page(phys);
		if (page->host_state == PKVM_PAGE_OWNED && !hyp_refcount_get(page->refcount))
			continue;
		else if (page->host_state == PKVM_PAGE_SHARED_OWNED && page->host_share_guest_count)
			continue;
		ret = -EPERM;
		goto unlock;
	}

	phys = hyp_pfn_to_phys(pfn) ;
	WARN_ON(kvm_pgtable_stage2_map(&vm->pgt, ipa, size, phys,
				       pkvm_mkstate(prot, PKVM_PAGE_SHARED_BORROWED),
				       &vcpu->vcpu.arch.stage2_mc, 0));
	for (; phys < end; phys += PAGE_SIZE) {
		page = hyp_phys_to_page(phys);
		page->host_state = PKVM_PAGE_SHARED_OWNED;
		page->host_share_guest_count++;
	}

unlock:
	guest_unlock_component(vm);
	host_unlock_component();

	return ret;
}

static int __check_host_shared_guest(struct pkvm_hyp_vm *vm, u64 *__phys, u64 ipa, size_t size)
{
	enum pkvm_page_state state;
	struct hyp_page *page;
	kvm_pte_t pte;
	u64 phys, end;
	s8 level;
	int ret;

	if (size != PAGE_SIZE && size != PMD_SIZE)
		return -EINVAL;
	ret = kvm_pgtable_get_leaf(&vm->pgt, ipa, &pte, &level);
	if (ret)
		return ret;
	if (!kvm_pte_valid(pte))
		return -ENOENT;
	if (kvm_granule_size(level) != size)
		return -E2BIG;

	state = guest_get_page_state(pte, ipa) & ~PKVM_PAGE_RESTRICTED_PROT;
	if (state != PKVM_PAGE_SHARED_BORROWED)
		return -EPERM;

	phys = kvm_pte_to_phys(pte);
	end = phys + size;
	ret = check_range_allowed_memory(phys, end);
	if (WARN_ON(ret))
		return ret;

	for (; phys < end; phys += PAGE_SIZE) {
		page = hyp_phys_to_page(phys);
		if (page->host_state != PKVM_PAGE_SHARED_OWNED)
			return -EPERM;
		if (WARN_ON(!page->host_share_guest_count))
			return -EINVAL;
	}

	*__phys = kvm_pte_to_phys(pte);

	return 0;
}

int __pkvm_host_unshare_guest(u64 gfn, struct pkvm_hyp_vm *vm, u64 nr_pages)
{
	size_t size = PAGE_SIZE * nr_pages;
	u64 ipa = hyp_pfn_to_phys(gfn);
	struct hyp_page *page;
	u64 phys, end;
	int ret;

	host_lock_component();
	guest_lock_component(vm);

	ret = __check_host_shared_guest(vm, &phys, ipa, size);
	if (ret)
		goto unlock;

	ret = kvm_pgtable_stage2_unmap(&vm->pgt, ipa, size);
	if (ret)
		goto unlock;

	end = phys + size;
	for (; phys < end; phys += PAGE_SIZE) {
		page = hyp_phys_to_page(phys);
		page->host_share_guest_count--;
		if (!page->host_share_guest_count)
			page->host_state = PKVM_PAGE_OWNED;
	}

unlock:
	guest_unlock_component(vm);
	host_unlock_component();

	return ret;
}

static int guest_get_valid_pte(struct pkvm_hyp_vm *vm, u64 *phys, u64 ipa, u8 order, kvm_pte_t *pte)
{
	size_t size = PAGE_SIZE << order;
	s8 level;

	if (order && size != PMD_SIZE)
		return -EINVAL;

	WARN_ON(kvm_pgtable_get_leaf(&vm->pgt, ipa, pte, &level));

	if (kvm_granule_size(level) != size)
		return -E2BIG;

	if (!kvm_pte_valid(*pte))
		return -ENOENT;

	*phys = kvm_pte_to_phys(*pte);

	return 0;
}

int __pkvm_host_relax_perms_guest(u64 gfn, struct pkvm_hyp_vcpu *vcpu, enum kvm_pgtable_prot prot)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	u64 ipa = hyp_pfn_to_phys(gfn);
	int ret;

	if (WARN_ON(kvm_vm_is_protected(&vm->kvm)))
		return -EPERM;

	if (prot & ~KVM_PGTABLE_PROT_RWX)
		return -EINVAL;

	guest_lock_component(vm);
	ret = kvm_pgtable_stage2_relax_perms(&vm->pgt, ipa, prot, 0);
	guest_unlock_component(vm);

	return ret;
}

int __pkvm_host_wrprotect_guest(u64 gfn, struct pkvm_hyp_vm *vm, u64 size)
{
	u64 ipa = hyp_pfn_to_phys(gfn);
	int ret;

	if (WARN_ON(kvm_vm_is_protected(&vm->kvm)))
		return -EPERM;

	guest_lock_component(vm);
	ret = kvm_pgtable_stage2_wrprotect(&vm->pgt, ipa, size);
	guest_unlock_component(vm);

	return ret;
}

int __pkvm_host_test_clear_young_guest(u64 gfn, u64 size, bool mkold, struct pkvm_hyp_vm *vm)
{
	u64 ipa = hyp_pfn_to_phys(gfn);
	int ret;

	if (WARN_ON(kvm_vm_is_protected(&vm->kvm)))
		return -EPERM;

	guest_lock_component(vm);
	ret = kvm_pgtable_stage2_test_clear_young(&vm->pgt, ipa, size, mkold);
	guest_unlock_component(vm);

	return ret;
}

kvm_pte_t __pkvm_host_mkyoung_guest(u64 gfn, struct pkvm_hyp_vcpu *vcpu)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	u64 ipa = hyp_pfn_to_phys(gfn);
	kvm_pte_t pte;

	if (WARN_ON(kvm_vm_is_protected(&vm->kvm)))
		return 0;

	guest_lock_component(vm);
	pte = kvm_pgtable_stage2_mkyoung(&vm->pgt, ipa, 0);
	guest_unlock_component(vm);

	return pte;
}

int __pkvm_host_split_guest(u64 gfn, u64 size, struct pkvm_hyp_vcpu *vcpu)
{
	struct kvm_hyp_memcache *mc = &vcpu->vcpu.arch.stage2_mc;
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	u64 ipa = hyp_pfn_to_phys(gfn);
	int ret;

	if (size != PMD_SIZE)
		return -EINVAL;

	guest_lock_component(vm);

	/*
	 * stage2_split() already checks the existing mapping is valid and PMD-level.
	 * No other check is necessary.
	 */

	ret = kvm_pgtable_stage2_split(&vm->pgt, ipa, size, mc);

	guest_unlock_component(vm);

	return ret;
}

static int __host_set_owner_guest(struct pkvm_hyp_vcpu *vcpu, u64 phys, u64 ipa,
				  size_t size, bool is_memory)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	u64 nr_pages = size >> PAGE_SHIFT;
	int ret;

	/*
	 * update_iommu=false, the caller must do the update _before_ this function is called. This
	 * intends to protect pvmfw loading.
	 */
	WARN_ON(__host_stage2_set_owner_locked(phys, size, PKVM_ID_GUEST,
					       is_memory, 0, false));
	psci_mem_protect_inc(nr_pages);
	if (pkvm_ipa_range_has_pvmfw(vm, ipa, ipa + size)) {
		ret = pkvm_load_pvmfw_pages(vm, ipa, phys, size);
		if (WARN_ON(ret)) {
			psci_mem_protect_dec(nr_pages);
			return ret;
		}
	}

	return 0;
}

int __pkvm_host_donate_guest(u64 pfn, u64 gfn, struct pkvm_hyp_vcpu *vcpu, u64 nr_pages)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	u64 phys = hyp_pfn_to_phys(pfn);
	u64 ipa = hyp_pfn_to_phys(gfn);
	enum kvm_pgtable_prot prot;
	bool is_memory;
	size_t size;
	int ret;

	if (check_mul_overflow(nr_pages, PAGE_SIZE, &size))
		return -EINVAL;

	host_lock_component();
	guest_lock_component(vm);

	ret = __host_check_page_state_range(phys, size, PKVM_PAGE_OWNED);
	if (ret)
		goto unlock;
	ret = __guest_check_page_state_range(vcpu, ipa, size, PKVM_NOPAGE);
	if (ret)
		goto unlock;

	is_memory = addr_is_memory(phys);
	if (is_memory) {
		kvm_iommu_host_stage2_idmap(phys, phys + size, 0);
		kvm_iommu_host_stage2_idmap_complete(false);
	}
	WARN_ON(__host_set_owner_guest(vcpu, phys, ipa, size, is_memory));

	prot = pkvm_mkstate(KVM_PGTABLE_PROT_RWX, PKVM_PAGE_OWNED);
	WARN_ON(kvm_pgtable_stage2_map(&vm->pgt, ipa, size, phys, prot,
				       &vcpu->vcpu.arch.stage2_mc, 0));

unlock:
	guest_unlock_component(vm);
	host_unlock_component();

	return ret;
}

struct kvm_hyp_pinned_page *hyp_ppages;

static int __copy_hyp_ppages(struct pkvm_hyp_vcpu *vcpu)
{
	struct kvm_hyp_pinned_page *ppage, *hyp_ppage;

	WARN_ON(!hyp_ppages);

	ppage = next_kvm_hyp_pinned_page(vcpu->vcpu.arch.hyp_reqs, NULL, true);
	if (!ppage)
		return -EINVAL;

	hyp_ppage = hyp_ppages;

	do {
		memcpy(hyp_ppage, ppage, sizeof(*ppage));
		ppage = next_kvm_hyp_pinned_page(vcpu->vcpu.arch.hyp_reqs, ppage, true);
		hyp_ppage++; /* No risk to overflow hyp_ppages */
	} while (ppage);

	hyp_ppage->order = 0xFF;

	return 0;
}

#define for_each_hyp_ppage(hyp_ppage)						\
	for (hyp_ppage = hyp_ppages; (hyp_ppage)->order != 0xFF; (hyp_ppage)++)

int __pkvm_host_donate_sglist_guest(struct pkvm_hyp_vcpu *vcpu)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(vcpu);
	struct kvm_hyp_pinned_page *ppage = hyp_ppages;
	bool is_memory;
	int ret;

	host_lock_component();
	guest_lock_component(vm);

	ret = __copy_hyp_ppages(vcpu);
	if (ret)
		goto unlock;

	is_memory = addr_is_memory(hyp_pfn_to_phys(ppage->pfn));

	for_each_hyp_ppage(ppage) {
		u64 phys = hyp_pfn_to_phys(ppage->pfn);
		u64 ipa = hyp_pfn_to_phys(ppage->gfn);
		size_t size;

		if (check_shl_overflow(PAGE_SIZE, ppage->order, &size)) {
			ret = -EINVAL;
			goto unlock;
		}

		if (addr_is_memory(phys) != is_memory) {
			ret = -EINVAL;
			goto unlock;
		}

		ret = __host_check_page_state_range(phys, size, PKVM_PAGE_OWNED);
		if (ret)
			goto unlock;

		ret = __guest_check_page_state_range(vcpu, ipa, size, PKVM_NOPAGE);
		if (ret)
			goto unlock;
	}

	if (is_memory) {
		for_each_hyp_ppage(ppage) {
			size_t size = PAGE_SIZE << ppage->order;
			u64 phys = hyp_pfn_to_phys(ppage->pfn);

			kvm_iommu_host_stage2_idmap(phys, phys + size, 0);
		}

		kvm_iommu_host_stage2_idmap_complete(false);
	}

	for_each_hyp_ppage(ppage) {
		size_t size = PAGE_SIZE << ppage->order;
		u64 phys = hyp_pfn_to_phys(ppage->pfn);
		u64 ipa = hyp_pfn_to_phys(ppage->gfn);
		enum kvm_pgtable_prot prot;

		/* Now the sglist is unmapped from the IOMMUs, we can load pvmfw */
		WARN_ON(__host_set_owner_guest(vcpu, phys, ipa, size, is_memory));

		prot = pkvm_mkstate(KVM_PGTABLE_PROT_RWX, PKVM_PAGE_OWNED);
		WARN_ON(kvm_pgtable_stage2_map(&vm->pgt, ipa, size, phys, prot,
				       &vcpu->vcpu.arch.stage2_mc, 0));
	}

unlock:
	guest_unlock_component(vm);
	host_unlock_component();

	return ret;
}

int __pkvm_host_donate_sglist_hyp(struct pkvm_sglist_page *sglist, size_t nr_pages)
{
	int p, ret;

	host_lock_component();
	hyp_lock_component();

	/* Checking we are reading hyp private memory */
	if (IS_ENABLED(CONFIG_NVHE_EL2_DEBUG))
		WARN_ON(__hyp_check_page_state_range((u64)sglist, nr_pages * sizeof(*sglist),
						     PKVM_PAGE_OWNED));

	for (p = 0; p < nr_pages; p++) {
		u64 phys = hyp_pfn_to_phys(sglist[p].pfn);
		size_t size;

		if (check_shl_overflow(PAGE_SIZE, sglist[p].order, &size)) {
			ret = -EINVAL;
			goto unlock;
		}

		if (!addr_is_memory(phys)) {
			ret = -EINVAL;
			goto unlock;
		}

		ret = __host_check_page_state_range(phys, size, PKVM_PAGE_OWNED);
		if (ret)
			goto unlock;

		if (IS_ENABLED(CONFIG_NVHE_EL2_DEBUG)) {
			ret = __hyp_check_page_state_range((u64)__hyp_va(phys), size, PKVM_NOPAGE);
			if (ret)
				goto unlock;
		}
	}

	for (p = 0; p < nr_pages; p++) {
		size_t size = PAGE_SIZE << sglist[p].order;
		u64 phys = hyp_pfn_to_phys(sglist[p].pfn);
		enum kvm_pgtable_prot prot;

		prot = pkvm_mkstate(PAGE_HYP, PKVM_PAGE_OWNED);
		ret = pkvm_create_mappings_locked(__hyp_va(phys), __hyp_va(phys) + size, prot);
		if (ret) {
			WARN_ON(ret != -ENOMEM);

			kvm_iommu_host_stage2_idmap_complete(false);

			/* Rollback */
			for (; p >= 0; p--) {
				phys = hyp_pfn_to_phys(sglist[p].pfn);
				size = PAGE_SIZE << sglist[p].order;

				WARN_ON(host_stage2_idmap_locked(phys, size,
								 PKVM_HOST_MEM_PROT, false));
				kvm_iommu_host_stage2_idmap(phys, phys + size, PKVM_HOST_MEM_PROT);
				pkvm_remove_mappings_locked(__hyp_va(phys), __hyp_va(phys) + size);
			}
			kvm_iommu_host_stage2_idmap_complete(true);

			break;
		}

		WARN_ON(__host_stage2_set_owner_locked(phys, size, PKVM_ID_HYP, true, 0, false));
		kvm_iommu_host_stage2_idmap(phys, phys + size, 0);
	}

	kvm_iommu_host_stage2_idmap_complete(false);

unlock:
	hyp_unlock_component();
	host_unlock_component();

	return ret;
}

void hyp_poison_page(phys_addr_t phys, size_t size)
{
	WARN_ON(!PAGE_ALIGNED(size));

	while (size) {
		size_t __size = size == PMD_SIZE ? size : PAGE_SIZE;
		void *addr = __fixmap_guest_page(__hyp_va(phys), &__size);

		memset(addr, 0, __size);

		/*
		 * Prefer kvm_flush_dcache_to_poc() over __clean_dcache_guest_page()
		 * here as the latter may elide the CMO under the assumption that FWB
		 * will be enabled on CPUs that support it. This is incorrect for the
		 * host stage-2 and would otherwise lead to a malicious host potentially
		 * being able to read the contents of newly reclaimed guest pages.
		 */
		kvm_flush_dcache_to_poc(addr, __size);
		__fixunmap_guest_page(__size);

		size -= __size;
		phys += __size;
	}
}

void destroy_hyp_vm_pgt(struct pkvm_hyp_vm *vm)
{
	guest_lock_component(vm);
	kvm_pgtable_stage2_destroy(&vm->pgt);
	guest_unlock_component(vm);
}

void drain_hyp_pool(struct hyp_pool *pool, struct kvm_hyp_memcache *mc)
{
	WARN_ON(reclaim_hyp_pool(pool, mc, INT_MAX) != -ENOMEM);
}

int __pkvm_host_reclaim_page(struct pkvm_hyp_vm *vm, u64 pfn, u64 ipa, u8 order)
{
	phys_addr_t __phys, phys = hyp_pfn_to_phys(pfn);
	size_t page_size;
	kvm_pte_t pte;
	int ret = 0;

	if (check_shl_overflow(PAGE_SIZE, order, &page_size))
		return -EINVAL;

	host_lock_component();
	guest_lock_component(vm);

	ret = guest_get_valid_pte(vm, &__phys, ipa, order, &pte);
	if (ret)
		goto unlock;

	if (phys != __phys) {
		ret = -EINVAL;
		goto unlock;
	}

	switch ((int)guest_get_page_state(pte, ipa)) {
	case PKVM_PAGE_OWNED:
		WARN_ON(__host_check_page_state_range(phys, page_size, PKVM_NOPAGE));
		/* No vCPUs of the guest can run, doing this prior to stage-2 unmap is OK */
		hyp_poison_page(phys, page_size);
		psci_mem_protect_dec(1 << order);
		break;
	case PKVM_PAGE_SHARED_BORROWED:
	case PKVM_PAGE_SHARED_BORROWED | PKVM_PAGE_RESTRICTED_PROT:
		WARN_ON(__host_check_page_state_range(phys, page_size, PKVM_PAGE_SHARED_OWNED));
		break;
	case PKVM_PAGE_SHARED_OWNED:
		if (__host_check_page_state_range(phys, page_size, PKVM_PAGE_SHARED_BORROWED)) {
			/* Presumably a page shared via FF-A, will be handled separately */
			ret = -EBUSY;
			goto unlock;
		}
		break;
	default:
		BUG_ON(1);
	}

	/* We could avoid TLB inval, it is done per VMID on the finalize path */
	WARN_ON(kvm_pgtable_stage2_unmap(&vm->pgt, ipa, page_size));
	WARN_ON(host_stage2_set_owner_locked(phys, page_size, PKVM_ID_HOST));

unlock:
	guest_unlock_component(vm);
	host_unlock_component();

	return ret;
}

static bool __check_ioguard_page(struct pkvm_hyp_vcpu *hyp_vcpu, u64 ipa)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(hyp_vcpu);
	kvm_pte_t pte;
	s8 level;
	int ret;

	ret = kvm_pgtable_get_leaf(&vm->pgt, ipa, &pte, &level);
	if (ret)
		return false;

	/* Must be a PAGE_SIZE mapping with our annotation */
	return (BIT(ARM64_HW_PGTABLE_LEVEL_SHIFT(level)) == PAGE_SIZE &&
		pte == KVM_INVALID_PTE_MMIO_NOTE);
}

int __pkvm_install_ioguard_page(struct pkvm_hyp_vcpu *hyp_vcpu, u64 ipa,
				u64 nr_pages, u64 *nr_guarded)
{
	struct guest_request_walker_data data = GUEST_WALKER_DATA_INIT(PKVM_NOPAGE);
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(hyp_vcpu);
	struct kvm_pgtable_walker walker = {
		.cb     = guest_request_walker,
		.flags  = KVM_PGTABLE_WALK_LEAF,
		.arg    = (void *)&data,
	};
	int ret;

	if (!test_bit(KVM_ARCH_FLAG_MMIO_GUARD, &vm->kvm.arch.flags))
		return -EINVAL;

	if (!PAGE_ALIGNED(ipa))
		return -EINVAL;

	guest_lock_component(vm);

	/* Check we either have NOMAP or NOMAP|MMIO in this range */
	data.desired_mask = ~PKVM_MMIO;

	ret = kvm_pgtable_walk(&vm->pgt, ipa, nr_pages << PAGE_SHIFT, &walker);
	/* Walker reached data.max_ptes */
	if (ret == -E2BIG)
		ret = 0;
	else if (ret)
		goto unlock;

	/*
	 * Intersection between the requested region and what has been verified
	 */
	*nr_guarded = nr_pages = min_t(u64, data.size >> PAGE_SHIFT, nr_pages);
	ret = kvm_pgtable_stage2_annotate(&vm->pgt, ipa, nr_pages << PAGE_SHIFT,
					  &hyp_vcpu->vcpu.arch.stage2_mc,
					  KVM_INVALID_PTE_MMIO_NOTE);

unlock:
	guest_unlock_component(vm);
	return ret;
}

bool __pkvm_check_ioguard_page(struct pkvm_hyp_vcpu *hyp_vcpu)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(hyp_vcpu);
	u64 ipa, end;
	bool ret;

	if (!kvm_vcpu_dabt_isvalid(&hyp_vcpu->vcpu))
		return false;

	if (!test_bit(KVM_ARCH_FLAG_MMIO_GUARD, &vm->kvm.arch.flags))
		return true;

	ipa  = kvm_vcpu_get_fault_ipa(&hyp_vcpu->vcpu);
	ipa |= kvm_vcpu_get_hfar(&hyp_vcpu->vcpu) & FAR_MASK;
	end = ipa + kvm_vcpu_dabt_get_as(&hyp_vcpu->vcpu) - 1;

	guest_lock_component(vm);
	ret = __check_ioguard_page(hyp_vcpu, ipa);
	if ((end & PAGE_MASK) != (ipa & PAGE_MASK))
		ret &= __check_ioguard_page(hyp_vcpu, end);
	guest_unlock_component(vm);

	return ret;
}

static int __pkvm_remove_ioguard_page(struct pkvm_hyp_vm *vm, u64 ipa)
{
	int ret;
	kvm_pte_t pte;
	s8 level;

	hyp_assert_lock_held(&vm->pgtable_lock);

	if (!test_bit(KVM_ARCH_FLAG_MMIO_GUARD, &vm->kvm.arch.flags))
		return -EINVAL;

	if (!PAGE_ALIGNED(ipa))
		return -EINVAL;

	ret = kvm_pgtable_get_leaf(&vm->pgt, ipa, &pte, &level);
	if (ret)
		return ret;

	if (BIT(ARM64_HW_PGTABLE_LEVEL_SHIFT(level)) == PAGE_SIZE &&
	    pte == KVM_INVALID_PTE_MMIO_NOTE)
		return kvm_pgtable_stage2_unmap(&vm->pgt, ipa, PAGE_SIZE);

	return kvm_pte_valid(pte) ? -EEXIST : -EINVAL;
}

int __pkvm_install_guest_mmio(struct pkvm_hyp_vcpu *hyp_vcpu, u64 pfn, u64 gfn)
{
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(hyp_vcpu);
	u64 ipa = gfn << PAGE_SHIFT;
	int ret;

	hyp_lock_component();
	guest_lock_component(vm);
	ret = __pkvm_remove_ioguard_page(vm, ipa);
	if (ret)
		goto out_unlock;
	ret = pkvm_hyp_donate_guest(hyp_vcpu, pfn, gfn);
out_unlock:
	guest_unlock_component(vm);
	hyp_unlock_component();
	return ret;
}

int host_stage2_get_leaf(phys_addr_t phys, kvm_pte_t *ptep, s8 *level)
{
	int ret;

	host_lock_component();
	ret = kvm_pgtable_get_leaf(&host_mmu.pgt, phys, ptep, level);
	host_unlock_component();

	return ret;
}

static u64 __pkvm_ptdump_get_host_config(enum pkvm_ptdump_ops op)
{
	u64 ret = 0;

	host_lock_component();
	if (op == PKVM_PTDUMP_GET_LEVEL)
		ret = host_mmu.pgt.start_level;
	else
		ret = host_mmu.pgt.ia_bits;
	host_unlock_component();

	return ret;
}

static u64 __pkvm_ptdump_get_guest_config(pkvm_handle_t handle, enum pkvm_ptdump_ops op)
{
	struct pkvm_hyp_vm *vm;
	u64 ret = 0;

	vm = get_pkvm_hyp_vm(handle);
	if (!vm)
		return -EINVAL;

	if (op == PKVM_PTDUMP_GET_LEVEL)
		ret = vm->pgt.start_level;
	else
		ret = vm->pgt.ia_bits;

	put_pkvm_hyp_vm(vm);
	return ret;
}

u64 __pkvm_ptdump_get_config(pkvm_handle_t handle, enum pkvm_ptdump_ops op)
{
	if (!handle)
		return __pkvm_ptdump_get_host_config(op);

	return __pkvm_ptdump_get_guest_config(handle, op);
}

static int pkvm_ptdump_walker(const struct kvm_pgtable_visit_ctx *ctx,
			      enum kvm_pgtable_walk_flags visit)
{
	struct pkvm_ptdump_log_hdr **log_hdr = ctx->arg;
	ssize_t avail_space = PAGE_SIZE - (*log_hdr)->w_index - sizeof(struct pkvm_ptdump_log_hdr);
	struct pkvm_ptdump_log *log;

	if (avail_space < sizeof(struct pkvm_ptdump_log)) {
		if ((*log_hdr)->pfn_next == INVALID_PTDUMP_PFN)
			return -ENOMEM;

		*log_hdr = hyp_phys_to_virt(hyp_pfn_to_phys((*log_hdr)->pfn_next));
		WARN_ON((*log_hdr)->w_index);
	}

	log = (struct pkvm_ptdump_log *)((void *)*log_hdr + (*log_hdr)->w_index +
					 sizeof(struct pkvm_ptdump_log_hdr));
	log->pfn = ctx->addr >> PAGE_SHIFT;
	log->valid = ctx->old & PTE_VALID;
	log->r = FIELD_GET(KVM_PTE_LEAF_ATTR_LO_S2_S2AP_R, ctx->old);
	log->w = FIELD_GET(KVM_PTE_LEAF_ATTR_LO_S2_S2AP_W, ctx->old);
	log->xn = FIELD_GET(KVM_PTE_LEAF_ATTR_HI_S2_XN, ctx->old);
	log->table = FIELD_GET(KVM_PTE_TYPE, ctx->old);
	log->level = ctx->level;
	log->page_state = FIELD_GET(PKVM_PAGE_STATE_PROT_MASK, ctx->old);

	(*log_hdr)->w_index += sizeof(struct pkvm_ptdump_log);
	return 0;
}

static void pkvm_ptdump_teardown_log(struct pkvm_ptdump_log_hdr *log_hva,
				     struct pkvm_ptdump_log_hdr *cur)
{
	struct pkvm_ptdump_log_hdr *tmp, *log = (void *)kern_hyp_va(log_hva);
	bool next_log_invalid = false;

	while (log != cur && !next_log_invalid) {
		next_log_invalid = log->pfn_next == INVALID_PTDUMP_PFN;
		tmp = hyp_phys_to_virt(hyp_pfn_to_phys(log->pfn_next));
		WARN_ON(__pkvm_hyp_donate_host(hyp_virt_to_pfn(log), 1));
		log = tmp;
	}
}

static int pkvm_ptdump_setup_log(struct pkvm_ptdump_log_hdr *log_hva)
{
	int ret;
	struct pkvm_ptdump_log_hdr *log = (void *)kern_hyp_va(log_hva);

	if (!PAGE_ALIGNED(log))
		return -EINVAL;

	for (;;) {
		ret = __pkvm_host_donate_hyp(hyp_virt_to_pfn(log), 1);
		if (ret) {
			pkvm_ptdump_teardown_log(log_hva, log);
			return ret;
		}

		log->w_index = 0;
		if (log->pfn_next == INVALID_PTDUMP_PFN)
			break;

		log = hyp_phys_to_virt(hyp_pfn_to_phys(log->pfn_next));
	}

	return 0;
}

static int pkvm_ptdump_walk_host(struct kvm_pgtable_walker *walker)
{
	int ret;

	host_lock_component();
	ret = kvm_pgtable_walk(&host_mmu.pgt, 0, BIT(host_mmu.pgt.ia_bits), walker);
	host_unlock_component();

	return ret;
}

static int pkvm_ptdump_walk_guest(struct pkvm_hyp_vm *vm, struct kvm_pgtable_walker *walker)
{
	int ret;

	guest_lock_component(vm);

	ret = kvm_pgtable_walk(&vm->pgt, 0, BIT(vm->pgt.ia_bits), walker);

	guest_unlock_component(vm);

	return ret;
}

u64 __pkvm_ptdump_walk_range(pkvm_handle_t handle, struct pkvm_ptdump_log_hdr *log)
{
	struct pkvm_hyp_vm *vm;
	int ret;
	struct pkvm_ptdump_log_hdr *log_hyp = kern_hyp_va(log);
	struct kvm_pgtable_walker walker = {
		.cb     = pkvm_ptdump_walker,
		.flags  = KVM_PGTABLE_WALK_LEAF,
		.arg    = &log_hyp,
	};

	ret = pkvm_ptdump_setup_log(log);
	if (ret)
		return ret;

	if (!handle)
		ret = pkvm_ptdump_walk_host(&walker);
	else {
		vm = get_pkvm_hyp_vm(handle);
		if (!vm) {
			ret = -EINVAL;
			goto teardown;
		}

		ret = pkvm_ptdump_walk_guest(vm, &walker);
		put_pkvm_hyp_vm(vm);
	}
teardown:
	pkvm_ptdump_teardown_log(log, NULL);
	return ret;
}

/* Return PA for an owned guest IPA or request it, and repeat the guest HVC */
int pkvm_get_guest_pa_request(struct pkvm_hyp_vcpu *hyp_vcpu, u64 ipa,
			      size_t ipa_size_request, u64 *out_pa, s8 *out_level)
{
	struct kvm_hyp_req *req;
	kvm_pte_t pte;
	enum pkvm_page_state state;
	struct pkvm_hyp_vm *vm = pkvm_hyp_vcpu_to_hyp_vm(hyp_vcpu);

	guest_lock_component(vm);
	WARN_ON(kvm_pgtable_get_leaf(&vm->pgt, ipa, &pte, out_level));
	guest_unlock_component(vm);
	if (!kvm_pte_valid(pte)) {
		/* Page not mapped, create a request*/
		req = pkvm_hyp_req_reserve(hyp_vcpu, KVM_HYP_REQ_TYPE_MAP);
		if (!req)
			return -ENOMEM;

		req->map.guest_ipa = ipa;
		req->map.size = ipa_size_request;
		return -ENOENT;
	}

	state = pkvm_getstate(kvm_pgtable_stage2_pte_prot(pte));
	if (state != PKVM_PAGE_OWNED)
		return -EPERM;

	*out_pa = kvm_pte_to_phys(pte);
	*out_pa |= ipa & (kvm_granule_size(*out_level) - 1) & PAGE_MASK;
	return 0;
}

/* Get a PA and use the page for DMA */
int pkvm_get_guest_pa_request_use_dma(struct pkvm_hyp_vcpu *hyp_vcpu, u64 ipa,
				      size_t ipa_size_request, u64 *out_pa, s8 *level)
{
	int ret;

	host_lock_component();
	ret = pkvm_get_guest_pa_request(hyp_vcpu, ipa, ipa_size_request,
					out_pa, level);
	if (ret)
		goto out_ret;
	WARN_ON(__pkvm_use_dma_locked(*out_pa, kvm_granule_size(*level), hyp_vcpu));
out_ret:
	host_unlock_component();
	return ret;
}

#ifdef CONFIG_PKVM_SELFTESTS
struct pkvm_expected_state {
	enum pkvm_page_state host;
	enum pkvm_page_state hyp;
	enum pkvm_page_state guest[2]; /* [ gfn, gfn + 1 ] */
};

static struct pkvm_expected_state selftest_state;
static struct hyp_page *selftest_page;

static struct pkvm_hyp_vm selftest_vm = {
	.kvm = {
		.arch = {
			.mmu = {
				.arch = &selftest_vm.kvm.arch,
				.pgt = &selftest_vm.pgt,
			},
		},
	},
};

static struct pkvm_hyp_vcpu selftest_vcpu = {
	.vcpu = {
		.arch = {
			.hw_mmu = &selftest_vm.kvm.arch.mmu,
		},
		.kvm = &selftest_vm.kvm,
	},
};

static void init_selftest_vm(void *virt)
{
	struct hyp_page *p = hyp_virt_to_page(virt);
	int i;

	selftest_vm.kvm.arch.mmu.vtcr = host_mmu.arch.mmu.vtcr;
	WARN_ON(kvm_guest_prepare_stage2(&selftest_vm, virt));

	for (i = 0; i < pkvm_selftest_pages(); i++) {
		if (p[i].refcount)
			continue;
		p[i].refcount = 1;
		hyp_put_page(&selftest_vm.pool, hyp_page_to_virt(&p[i]));
	}
}

static void teardown_selftest_vm(void)
{
	destroy_hyp_vm_pgt(&selftest_vm);
}

static u64 selftest_ipa(void)
{
	return BIT(selftest_vm.pgt.ia_bits - 1);
}

static void assert_page_state(void)
{
	void *virt = hyp_page_to_virt(selftest_page);
	u64 size = PAGE_SIZE << selftest_page->order;
	struct pkvm_hyp_vcpu *vcpu = &selftest_vcpu;
	u64 phys = hyp_virt_to_phys(virt);
	u64 ipa[2] = { selftest_ipa(), selftest_ipa() + PAGE_SIZE };

	host_lock_component();
	WARN_ON(__host_check_page_state_range(phys, size, selftest_state.host));
	host_unlock_component();

	hyp_lock_component();
	WARN_ON(__hyp_check_page_state_range((u64)virt, size, selftest_state.hyp));
	hyp_unlock_component();

	guest_lock_component(&selftest_vm);
	WARN_ON(__guest_check_page_state_range(vcpu, ipa[0], size, selftest_state.guest[0]));
	WARN_ON(__guest_check_page_state_range(vcpu, ipa[1], size, selftest_state.guest[1]));
	guest_unlock_component(&selftest_vm);
}

#define assert_transition_res(res, fn, ...)		\
	do {						\
		WARN_ON(fn(__VA_ARGS__) != res);	\
		assert_page_state();			\
	} while (0)

void pkvm_ownership_selftest(void *base)
{
	enum kvm_pgtable_prot prot = KVM_PGTABLE_PROT_RWX;
	void *virt = hyp_alloc_pages(&host_s2_pool, 0);
	struct pkvm_hyp_vcpu *vcpu = &selftest_vcpu;
	struct pkvm_hyp_vm *vm = &selftest_vm;
	u64 phys, size, pfn, gfn, pa;

	WARN_ON(!virt);
	selftest_page = hyp_virt_to_page(virt);
	selftest_page->refcount = 0;
	init_selftest_vm(base);

	size = PAGE_SIZE << selftest_page->order;
	phys = hyp_virt_to_phys(virt);
	pfn = hyp_phys_to_pfn(phys);
	gfn = hyp_phys_to_pfn(selftest_ipa());

	selftest_state.host = PKVM_NOPAGE;
	selftest_state.hyp = PKVM_PAGE_OWNED;
	selftest_state.guest[0] = selftest_state.guest[1] = PKVM_NOPAGE;
	assert_page_state();
	assert_transition_res(-EPERM,	__pkvm_host_donate_hyp, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_hyp, pfn);
	assert_transition_res(-EPERM,	__pkvm_host_unshare_hyp, pfn);
	assert_transition_res(-EPERM,	__pkvm_host_share_ffa, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_unshare_ffa, pfn, 1);
	assert_transition_res(-EPERM,	hyp_pin_shared_mem, virt, virt + size);
	assert_transition_res(-EPERM,	__pkvm_host_share_guest, pfn, gfn, vcpu, prot, 1);
	assert_transition_res(-ENOENT,	__pkvm_host_unshare_guest, gfn, vm, 1);
	assert_transition_res(-EPERM,	__pkvm_host_donate_guest, pfn, gfn, vcpu, 1);

	selftest_state.host = PKVM_PAGE_OWNED;
	selftest_state.hyp = PKVM_NOPAGE;
	assert_transition_res(0,	__pkvm_hyp_donate_host, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_hyp_donate_host, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_unshare_hyp, pfn);
	assert_transition_res(-EPERM,	__pkvm_host_unshare_ffa, pfn, 1);
	assert_transition_res(-ENOENT,	__pkvm_host_unshare_guest, gfn, vm, 1);
	assert_transition_res(-EPERM,	hyp_pin_shared_mem, virt, virt + size);

	selftest_state.host = PKVM_PAGE_SHARED_OWNED;
	selftest_state.hyp = PKVM_PAGE_SHARED_BORROWED;
	assert_transition_res(0,	__pkvm_host_share_hyp, pfn);
	assert_transition_res(-EPERM,	__pkvm_host_share_hyp, pfn);
	assert_transition_res(-EPERM,	__pkvm_host_donate_hyp, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_ffa, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_hyp_donate_host, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_guest, pfn, gfn, vcpu, prot, 1);
	assert_transition_res(-ENOENT,	__pkvm_host_unshare_guest, gfn, vm, 1);
	assert_transition_res(-EPERM,	__pkvm_host_donate_guest, pfn, gfn, vcpu, 1);

	assert_transition_res(0,	hyp_pin_shared_mem, virt, virt + size);
	assert_transition_res(0,	hyp_pin_shared_mem, virt, virt + size);
	hyp_unpin_shared_mem(virt, virt + size);
	WARN_ON(hyp_page_count(virt) != 1);
	assert_transition_res(-EBUSY,	__pkvm_host_unshare_hyp, pfn);
	assert_transition_res(-EPERM,	__pkvm_host_share_hyp, pfn);
	assert_transition_res(-EPERM,	__pkvm_host_donate_hyp, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_ffa, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_hyp_donate_host, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_guest, pfn, gfn, vcpu, prot, 1);
	assert_transition_res(-ENOENT,	__pkvm_host_unshare_guest, gfn, vm, 1);
	assert_transition_res(-EPERM,	__pkvm_host_donate_guest, pfn, gfn, vcpu, 1);

	hyp_unpin_shared_mem(virt, virt + size);
	assert_page_state();
	WARN_ON(hyp_page_count(virt));

	selftest_state.host = PKVM_PAGE_OWNED;
	selftest_state.hyp = PKVM_NOPAGE;
	assert_transition_res(0,	__pkvm_host_unshare_hyp, pfn);

	selftest_state.host = PKVM_PAGE_SHARED_OWNED;
	selftest_state.hyp = PKVM_NOPAGE;
	assert_transition_res(0,	__pkvm_host_share_ffa, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_ffa, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_donate_hyp, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_hyp, pfn);
	assert_transition_res(-EPERM,	__pkvm_host_unshare_hyp, pfn);
	assert_transition_res(-EPERM,	__pkvm_hyp_donate_host, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_guest, pfn, gfn, vcpu, prot, 1);
	assert_transition_res(-ENOENT,	__pkvm_host_unshare_guest, gfn, vm, 1);
	assert_transition_res(-EPERM,	__pkvm_host_donate_guest, pfn, gfn, vcpu, 1);
	assert_transition_res(-EPERM,	hyp_pin_shared_mem, virt, virt + size);

	selftest_state.host = PKVM_PAGE_OWNED;
	selftest_state.hyp = PKVM_NOPAGE;
	assert_transition_res(0,	__pkvm_host_unshare_ffa, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_unshare_ffa, pfn, 1);

	selftest_state.host = PKVM_PAGE_SHARED_OWNED;
	selftest_state.guest[0] = PKVM_PAGE_SHARED_BORROWED;
	assert_transition_res(0,	__pkvm_host_share_guest, pfn, gfn, vcpu, prot, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_guest, pfn, gfn, vcpu, prot, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_ffa, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_donate_hyp, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_hyp, pfn);
	assert_transition_res(-EPERM,	__pkvm_host_unshare_hyp, pfn);
	assert_transition_res(-EPERM,	__pkvm_hyp_donate_host, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_donate_guest, pfn, gfn, vcpu, 1);
	assert_transition_res(-EPERM,	hyp_pin_shared_mem, virt, virt + size);

	selftest_state.guest[1] = PKVM_PAGE_SHARED_BORROWED;
	assert_transition_res(0,	__pkvm_host_share_guest, pfn, gfn + 1, vcpu, prot, 1);
	WARN_ON(hyp_virt_to_page(virt)->host_share_guest_count != 2);

	selftest_state.guest[0] = PKVM_NOPAGE;
	assert_transition_res(0,	__pkvm_host_unshare_guest, gfn, vm, 1);

	selftest_state.guest[1] = PKVM_NOPAGE;
	selftest_state.host = PKVM_PAGE_OWNED;
	assert_transition_res(0,	__pkvm_host_unshare_guest, gfn + 1, vm, 1);

	selftest_vm.kvm.arch.pkvm.enabled = true;
	selftest_state.host = PKVM_NOPAGE;
	selftest_state.guest[0] = PKVM_PAGE_OWNED;
	assert_transition_res(0,	__pkvm_host_donate_guest, pfn, gfn, vcpu, 1);
	assert_transition_res(-EPERM,	__pkvm_host_donate_guest, pfn, gfn, vcpu, 1);
	assert_transition_res(-EPERM,	__pkvm_host_donate_guest, pfn, gfn + 1, vcpu, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_guest, pfn, gfn, vcpu, prot, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_guest, pfn, gfn + 1, vcpu, prot, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_ffa, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_donate_hyp, pfn, 1);
	assert_transition_res(-EPERM,	__pkvm_host_share_hyp, pfn);
	assert_transition_res(-EPERM,	__pkvm_host_unshare_hyp, pfn);
	assert_transition_res(-EPERM,	__pkvm_hyp_donate_host, pfn, 1);

	selftest_state.host = PKVM_PAGE_OWNED;
	selftest_state.guest[0] = PKVM_NOPAGE;
	assert_transition_res(0,	__pkvm_guest_relinquish_to_host, vcpu, gfn * PAGE_SIZE, &pa);
	WARN_ON(pa != phys);

	selftest_state.host = PKVM_NOPAGE;
	selftest_state.hyp = PKVM_PAGE_OWNED;
	assert_transition_res(0,	__pkvm_host_donate_hyp, pfn, 1);

	teardown_selftest_vm();
	selftest_page->refcount = 1;
	hyp_put_page(&host_s2_pool, virt);
}
#endif
