// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2020 Google LLC
 * Author: Quentin Perret <qperret@google.com>
 */

#include <linux/kvm_host.h>
#include <asm/kvm_hyp.h>
#include <asm/kvm_mmu.h>
#include <asm/kvm_pgtable.h>
#include <asm/kvm_pkvm.h>
#include <asm/spectre.h>

#include <nvhe/early_alloc.h>
#include <nvhe/gfp.h>
#include <nvhe/memory.h>
#include <nvhe/mem_protect.h>
#include <nvhe/mm.h>
#include <nvhe/spinlock.h>

struct kvm_pgtable pkvm_pgtable;
hyp_spinlock_t pkvm_pgd_lock;

struct memblock_region hyp_memory[HYP_MEMBLOCK_REGIONS];
unsigned int hyp_memblock_nr;

static u64 __io_map_base;
static DEFINE_PER_CPU(void *, hyp_fixmap_base);

static int __pkvm_create_mappings(unsigned long start, unsigned long size,
				  unsigned long phys, enum kvm_pgtable_prot prot)
{
	int err;

	hyp_spin_lock(&pkvm_pgd_lock);
	err = kvm_pgtable_hyp_map(&pkvm_pgtable, start, size, phys, prot);
	hyp_spin_unlock(&pkvm_pgd_lock);

	return err;
}

static unsigned long hyp_alloc_private_va_range(size_t size)
{
	unsigned long addr = __io_map_base;

	hyp_assert_lock_held(&pkvm_pgd_lock);
	__io_map_base += PAGE_ALIGN(size);

	/* Are we overflowing on the vmemmap ? */
	if (__io_map_base > __hyp_vmemmap) {
		__io_map_base = addr;
		addr = (unsigned long)ERR_PTR(-ENOMEM);
	}

	return addr;
}

unsigned long __pkvm_create_private_mapping(phys_addr_t phys, size_t size,
					    enum kvm_pgtable_prot prot)
{
	unsigned long addr;
	int err;

	hyp_spin_lock(&pkvm_pgd_lock);

	size = size + offset_in_page(phys);
	addr = hyp_alloc_private_va_range(size);
	if (IS_ERR((void *)addr))
		goto out;

	err = kvm_pgtable_hyp_map(&pkvm_pgtable, addr, size, phys, prot);
	if (err) {
		addr = (unsigned long)ERR_PTR(err);
		goto out;
	}

	addr = addr + offset_in_page(phys);
out:
	hyp_spin_unlock(&pkvm_pgd_lock);

	return addr;
}

int pkvm_create_mappings_locked(void *from, void *to, enum kvm_pgtable_prot prot)
{
	unsigned long start = (unsigned long)from;
	unsigned long end = (unsigned long)to;
	unsigned long virt_addr;
	phys_addr_t phys;

	hyp_assert_lock_held(&pkvm_pgd_lock);

	start = start & PAGE_MASK;
	end = PAGE_ALIGN(end);

	for (virt_addr = start; virt_addr < end; virt_addr += PAGE_SIZE) {
		int err;

		phys = hyp_virt_to_phys((void *)virt_addr);
		err = kvm_pgtable_hyp_map(&pkvm_pgtable, virt_addr, PAGE_SIZE,
					  phys, prot);
		if (err)
			return err;
	}

	return 0;
}

int pkvm_create_mappings(void *from, void *to, enum kvm_pgtable_prot prot)
{
	int ret;

	hyp_spin_lock(&pkvm_pgd_lock);
	ret = pkvm_create_mappings_locked(from, to, prot);
	hyp_spin_unlock(&pkvm_pgd_lock);

	return ret;
}

int hyp_back_vmemmap(phys_addr_t back)
{
	unsigned long i, start, size, end = 0;
	int ret;

	for (i = 0; i < hyp_memblock_nr; i++) {
		start = hyp_memory[i].base;
		start = ALIGN_DOWN((u64)hyp_phys_to_page(start), PAGE_SIZE);
		/*
		 * The begining of the hyp_vmemmap region for the current
		 * memblock may already be backed by the page backing the end
		 * the previous region, so avoid mapping it twice.
		 */
		start = max(start, end);

		end = hyp_memory[i].base + hyp_memory[i].size;
		end = PAGE_ALIGN((u64)hyp_phys_to_page(end));
		if (start >= end)
			continue;

		size = end - start;
		ret = __pkvm_create_mappings(start, size, back, PAGE_HYP);
		if (ret)
			return ret;

		memset(hyp_phys_to_virt(back), 0, size);
		back += size;
	}

	return 0;
}

static void *__hyp_bp_vect_base;
int pkvm_cpu_set_vector(enum arm64_hyp_spectre_vector slot)
{
	void *vector;

	switch (slot) {
	case HYP_VECTOR_DIRECT: {
		vector = __kvm_hyp_vector;
		break;
	}
	case HYP_VECTOR_SPECTRE_DIRECT: {
		vector = __bp_harden_hyp_vecs;
		break;
	}
	case HYP_VECTOR_INDIRECT:
	case HYP_VECTOR_SPECTRE_INDIRECT: {
		vector = (void *)__hyp_bp_vect_base;
		break;
	}
	default:
		return -EINVAL;
	}

	vector = __kvm_vector_slot2addr(vector, slot);
	*this_cpu_ptr(&kvm_hyp_vector) = (unsigned long)vector;

	return 0;
}

int hyp_map_vectors(void)
{
	phys_addr_t phys;
	void *bp_base;

	if (!kvm_system_needs_idmapped_vectors()) {
		__hyp_bp_vect_base = __bp_harden_hyp_vecs;
		return 0;
	}

	phys = __hyp_pa(__bp_harden_hyp_vecs);
	bp_base = (void *)__pkvm_create_private_mapping(phys,
							__BP_HARDEN_HYP_VECS_SZ,
							PAGE_HYP_EXEC);
	if (IS_ERR_OR_NULL(bp_base))
		return PTR_ERR(bp_base);

	__hyp_bp_vect_base = bp_base;

	return 0;
}

void *hyp_fixmap_map(phys_addr_t phys)
{
	void *addr = *this_cpu_ptr(&hyp_fixmap_base);
	int ret = kvm_pgtable_hyp_map(&pkvm_pgtable, (u64)addr, PAGE_SIZE,
				      phys, PAGE_HYP);
	return ret ? NULL : addr;
}

int hyp_fixmap_unmap(void)
{
	void *addr = *this_cpu_ptr(&hyp_fixmap_base);
	int ret = kvm_pgtable_hyp_unmap(&pkvm_pgtable, (u64)addr, PAGE_SIZE);

	return (ret != PAGE_SIZE) ? -EINVAL : 0;
}

static int __pin_pgtable_cb(u64 addr, u64 end, u32 level, kvm_pte_t *ptep,
			    enum kvm_pgtable_walk_flags flag, void * const arg)
{
	if (!kvm_pte_valid(*ptep) || level != KVM_PGTABLE_MAX_LEVELS - 1)
		return -EINVAL;
	hyp_page_ref_inc(hyp_virt_to_page(ptep));

	return 0;
}

static int hyp_pin_pgtable_pages(u64 addr)
{
	struct kvm_pgtable_walker walker = {
		.cb	= __pin_pgtable_cb,
		.flags	= KVM_PGTABLE_WALK_LEAF,
	};

	return kvm_pgtable_walk(&pkvm_pgtable, addr, PAGE_SIZE, &walker);
}

int hyp_create_pcpu_fixmap(void)
{
	unsigned long i;
	int ret = 0;
	u64 addr;

	hyp_spin_lock(&pkvm_pgd_lock);

	for (i = 0; i < hyp_nr_cpus; i++) {
		addr = hyp_alloc_private_va_range(PAGE_SIZE);
		if (IS_ERR((void *)addr)) {
			ret = -ENOMEM;
			goto unlock;
		}

		/*
		 * Create a dummy mapping, to get the intermediate page-table
		 * pages allocated, then take a reference on the last level
		 * page to keep it around at all times.
		 */
		ret = kvm_pgtable_hyp_map(&pkvm_pgtable, addr, PAGE_SIZE,
					  __hyp_pa(__hyp_bss_start), PAGE_HYP);
		if (ret) {
			ret = -EINVAL;
			goto unlock;
		}

		ret = hyp_pin_pgtable_pages(addr);
		if (ret)
			goto unlock;

		ret = kvm_pgtable_hyp_unmap(&pkvm_pgtable, addr, PAGE_SIZE);
		if (ret != PAGE_SIZE) {
			ret = -EINVAL;
			goto unlock;
		} else {
			ret = 0;
		}

		*per_cpu_ptr(&hyp_fixmap_base, i) = (void *)addr;
	}
unlock:
	hyp_spin_unlock(&pkvm_pgd_lock);

	return ret;
}

int hyp_create_idmap(u32 hyp_va_bits)
{
	unsigned long start, end;

	start = hyp_virt_to_phys((void *)__hyp_idmap_text_start);
	start = ALIGN_DOWN(start, PAGE_SIZE);

	end = hyp_virt_to_phys((void *)__hyp_idmap_text_end);
	end = ALIGN(end, PAGE_SIZE);

	/*
	 * One half of the VA space is reserved to linearly map portions of
	 * memory -- see va_layout.c for more details. The other half of the VA
	 * space contains the trampoline page, and needs some care. Split that
	 * second half in two and find the quarter of VA space not conflicting
	 * with the idmap to place the IOs and the vmemmap. IOs use the lower
	 * half of the quarter and the vmemmap the upper half.
	 */
	__io_map_base = start & BIT(hyp_va_bits - 2);
	__io_map_base ^= BIT(hyp_va_bits - 2);
	__hyp_vmemmap = __io_map_base | BIT(hyp_va_bits - 3);

	return __pkvm_create_mappings(start, end - start, start, PAGE_HYP_EXEC);
}

static void *admit_host_page(void *arg)
{
	struct kvm_hyp_memcache *host_mc = arg;

	if (!host_mc->nr_pages)
		return NULL;

	/*
	 * The host still owns the pages in its memcache, so we need to go
	 * through a full host-to-hyp donation cycle to change it. Fortunately,
	 * __pkvm_host_donate_hyp() takes care of races for us, so if it
	 * succeeds we're good to go.
	 */
	if (__pkvm_host_donate_hyp(hyp_phys_to_pfn(host_mc->head), 1))
		return NULL;

	return pop_hyp_memcache(host_mc, hyp_phys_to_virt);
}

/* Refill our local memcache by poping pages from the one provided by the host. */
int refill_memcache(struct kvm_hyp_memcache *mc, unsigned long min_pages,
		    struct kvm_hyp_memcache *host_mc)
{
	struct kvm_hyp_memcache tmp = *host_mc;
	int ret;

	ret =  __topup_hyp_memcache(mc, min_pages, admit_host_page,
				    hyp_virt_to_phys, &tmp);
	*host_mc = tmp;

	return ret;
}
