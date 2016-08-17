/*
 * Copyright 2002 Andi Kleen, SuSE Labs.
 * Thanks to Ben LaHaise for precious feedback.
 */
#include <linux/highmem.h>
#include <linux/bootmem.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/pfn.h>
#include <linux/percpu.h>
#include <linux/gfp.h>
#include <linux/vmalloc.h>
#include <linux/mutex.h>

#include <asm/processor.h>
#include <asm/tlbflush.h>
#include <asm/sections.h>
#include <asm/setup.h>
#include <asm/uaccess.h>
#include <asm/pgalloc.h>

#include "mm.h"

#ifdef CPA_DEBUG
#define cpa_debug(x, ...)  printk(x, __VA_ARGS__)
#else
#define cpa_debug(x, ...)
#endif

extern void v7_flush_kern_cache_all(void *);
extern void __flush_dcache_page(struct address_space *, struct page *);

#if defined(CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS)
static void inner_flush_cache_all(void)
{
#if defined(CONFIG_ARCH_TEGRA_11x_SOC)
	v7_flush_kern_cache_all(NULL);
#else
	on_each_cpu(v7_flush_kern_cache_all, NULL, 1);
#endif
}
#endif

#if defined(CONFIG_CPA)

/*
 * The arm kernel uses different cache policies(CPOLICY_WRITEBACK,
 * CPOLICY_WRITEALLOC, CPOLICY_WRITETHROUGH) based on architecture version
 * and smp mode. Using L_PTE_MT_WRITEALLOC or L_PTE_MT_WRITEBACK or
 * L_PTE_MT_WRITETHROUGH directly in CPA code can result in restoring incorrect
 * PTE attributes.
 * pgprot_kernel would always have PTE attributes based on the cache policy
 * in use for kernel cache memory. Use this to set the correct PTE attributes
 * for kernel cache memory.
 * */
#define L_PTE_MT_KERNEL (pgprot_kernel & L_PTE_MT_MASK)

/*
 * The current flushing context - we pass it instead of 5 arguments:
 */
struct cpa_data {
	unsigned long	*vaddr;
	pgprot_t	mask_set;
	pgprot_t	mask_clr;
	int		numpages;
	int		flags;
	unsigned long	pfn;
	unsigned	force_split:1;
	int		curpage;
	struct page	**pages;
};

/*
 * Serialize cpa() (for !DEBUG_PAGEALLOC which uses large identity mappings)
 * using cpa_lock. So that we don't allow any other cpu, with stale large tlb
 * entries change the page attribute in parallel to some other cpu
 * splitting a large page entry along with changing the attribute.
 */
static DEFINE_MUTEX(cpa_lock);

#define CPA_FLUSHTLB 1
#define CPA_ARRAY 2
#define CPA_PAGES_ARRAY 4

#ifdef CONFIG_PROC_FS
static unsigned long direct_pages_count[PG_LEVEL_NUM];

void update_page_count(int level, unsigned long pages)
{
	unsigned long flags;

	/* Protect against CPA */
	spin_lock_irqsave(&pgd_lock, flags);
	direct_pages_count[level] += pages;
	spin_unlock_irqrestore(&pgd_lock, flags);
}

static void split_page_count(int level)
{
	direct_pages_count[level]--;
	direct_pages_count[level - 1] += PTRS_PER_PTE;
}

void arch_report_meminfo(struct seq_file *m)
{
	seq_printf(m, "DirectMap4k:    %8lu kB\n",
			direct_pages_count[PG_LEVEL_4K] << 2);
	seq_printf(m, "DirectMap2M:    %8lu kB\n",
			direct_pages_count[PG_LEVEL_2M] << 11);
}
#else
static inline void split_page_count(int level) { }
#endif

#ifdef CONFIG_DEBUG_PAGEALLOC
# define debug_pagealloc 1
#else
# define debug_pagealloc 0
#endif

static inline int
within(unsigned long addr, unsigned long start, unsigned long end)
{
	return addr >= start && addr < end;
}

static void cpa_flush_range(unsigned long start, int numpages, int cache)
{
	unsigned int i, level;
	unsigned long addr;

	BUG_ON(irqs_disabled());
	WARN_ON(PAGE_ALIGN(start) != start);

	flush_tlb_kernel_range(start, start + (numpages << PAGE_SHIFT));

	if (!cache)
		return;

	for (i = 0, addr = start; i < numpages; i++, addr += PAGE_SIZE) {
		pte_t *pte = lookup_address(addr, &level);

		/*
		 * Only flush present addresses:
		 */
		if (pte && pte_present(*pte)) {
			__cpuc_flush_dcache_area((void *) addr, PAGE_SIZE);
			outer_flush_range(__pa((void *)addr),
					__pa((void *)addr) + PAGE_SIZE);
		}
	}
}

static void cpa_flush_array(unsigned long *start, int numpages, int cache,
			    int in_flags, struct page **pages)
{
	unsigned int i, level;
	bool flush_inner = true;
	unsigned long base;

	BUG_ON(irqs_disabled());

#if defined(CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS)
	if (numpages >= (cache_maint_inner_threshold >> PAGE_SHIFT) &&
		cache && in_flags & CPA_PAGES_ARRAY) {
		inner_flush_cache_all();
		flush_inner = false;
	}
#endif

	for (i = 0; i < numpages; i++) {
		unsigned long addr;
		pte_t *pte;

		if (in_flags & CPA_PAGES_ARRAY)
			addr = (unsigned long)page_address(pages[i]);
		else
			addr = start[i];

		flush_tlb_kernel_range(addr, addr + PAGE_SIZE);

		if (cache && in_flags & CPA_PAGES_ARRAY) {
			/* cache flush all pages including high mem pages. */
			if (flush_inner)
				__flush_dcache_page(
					page_mapping(pages[i]), pages[i]);
			base = page_to_phys(pages[i]);
			outer_flush_range(base, base + PAGE_SIZE);
		} else if (cache) {
			pte = lookup_address(addr, &level);

			/*
			 * Only flush present addresses:
			 */
			if (pte && pte_present(*pte)) {
				__cpuc_flush_dcache_area((void *)addr,
					PAGE_SIZE);
				outer_flush_range(__pa((void *)addr),
					__pa((void *)addr) + PAGE_SIZE);
			}
		}
	}
}

/*
 * Certain areas of memory require very specific protection flags,
 * for example the kernel text. Callers don't always get this
 * right so this function checks and fixes these known static
 * required protection bits.
 */
static inline pgprot_t static_protections(pgprot_t prot, unsigned long address,
				   unsigned long pfn)
{
	pgprot_t forbidden = __pgprot(0);

	/*
	 * The kernel text needs to be executable for obvious reasons
	 * Does not cover __inittext since that is gone later on.
	 */
	if (within(address, (unsigned long)_text, (unsigned long)_etext))
		pgprot_val(forbidden) |= L_PTE_XN;

	/*
	 * The .rodata section needs to be read-only. Using the pfn
	 * catches all aliases.
	 */
	if (within(pfn, __pa((unsigned long)__start_rodata) >> PAGE_SHIFT,
		   __pa((unsigned long)__end_rodata) >> PAGE_SHIFT))
		prot |= L_PTE_RDONLY;

	/*
	 * Mask off the forbidden bits and set the bits that are needed
	*/
	prot = __pgprot(pgprot_val(prot) & ~pgprot_val(forbidden));


	return prot;
}

static inline pgprot_t pte_to_pmd_pgprot(unsigned long pte,
				unsigned long ext_prot)
{
	pgprot_t ref_prot;

	ref_prot = PMD_TYPE_SECT | PMD_DOMAIN(DOMAIN_KERNEL) |
		   PMD_SECT_AP_WRITE;

	if (pte & L_PTE_MT_BUFFERABLE)
		ref_prot |= PMD_SECT_BUFFERABLE;

	if (pte & L_PTE_MT_WRITETHROUGH)
		ref_prot |= PMD_SECT_CACHEABLE;

	if (pte & L_PTE_XN)
		ref_prot |= PMD_SECT_XN;

	if (pte & L_PTE_USER)
		ref_prot |= PMD_SECT_AP_READ;

	if (pte & (1 << 4))
		ref_prot |= PMD_SECT_TEX(1);

	if (pte & L_PTE_RDONLY)
		ref_prot |= PMD_SECT_APX;

	if (pte & L_PTE_SHARED)
		ref_prot |= PMD_SECT_S;

	if (pte & PTE_EXT_NG)
		ref_prot |= PMD_SECT_nG;

	return ref_prot;
}

static inline pgprot_t pmd_to_pte_pgprot(unsigned long pmd,
				unsigned long *ext_prot)
{
	pgprot_t ref_prot;

	*ext_prot = 0;
	ref_prot = L_PTE_PRESENT | L_PTE_YOUNG | L_PTE_DIRTY;

	if (pmd & PMD_SECT_BUFFERABLE)
		ref_prot |= L_PTE_MT_BUFFERABLE;

	if (pmd & PMD_SECT_CACHEABLE)
		ref_prot |= L_PTE_MT_WRITETHROUGH;

	if (pmd & PMD_SECT_XN)
		ref_prot |= L_PTE_XN;

	if (pmd & PMD_SECT_AP_READ)
		ref_prot |= L_PTE_USER;

	if (pmd & PMD_SECT_TEX(1))
		ref_prot |= (1 << 4);

	if (pmd & PMD_SECT_APX)
		ref_prot |= L_PTE_RDONLY;

	if (pmd & PMD_SECT_S)
		ref_prot |= L_PTE_SHARED;

	if (pmd & PMD_SECT_nG)
		ref_prot |= PTE_EXT_NG;

	return ref_prot;
}

/*
 * Lookup the page table entry for a virtual address. Return a pointer
 * to the entry and the level of the mapping.
 *
 * Note: We return pud and pmd either when the entry is marked large
 * or when the present bit is not set. Otherwise we would return a
 * pointer to a nonexisting mapping.
 */
pte_t *lookup_address(unsigned long address, unsigned int *level)
{
	pgd_t *pgd = pgd_offset_k(address);
	pte_t *pte;
	pmd_t *pmd;

	/* pmds are folded into pgds on ARM */
	*level = PG_LEVEL_NONE;

	if (pgd == NULL || pgd_none(*pgd))
		return NULL;

	pmd = pmd_offset(pud_offset(pgd, address), address);

	if (pmd == NULL || pmd_none(*pmd) || !pmd_present(*pmd))
		return NULL;

	if (((pmd_val(*pmd) & (PMD_TYPE_SECT | PMD_SECT_SUPER))
		== (PMD_TYPE_SECT | PMD_SECT_SUPER)) || !pmd_present(*pmd)) {

		return NULL;
	} else if (pmd_val(*pmd) & PMD_TYPE_SECT) {

		*level = PG_LEVEL_2M;
		return (pte_t *)pmd;
	}

	pte = pte_offset_kernel(pmd, address);

	if ((pte == NULL) || pte_none(*pte))
		return NULL;

	*level = PG_LEVEL_4K;

	return pte;
}
EXPORT_SYMBOL_GPL(lookup_address);

/*
 * Set the new pmd in all the pgds we know about:
 */
static void __set_pmd_pte(pmd_t *pmd, unsigned long address, pte_t *pte)
{
	struct page *page;
	pud_t *pud;

	cpa_debug("__set_pmd_pte %x %x %x\n", pmd, pte, *pte);

	/* enforce pte entry stores ordering to avoid pmd writes
	 * bypassing pte stores.
	 */
	dsb();
	/* change init_mm */
	pmd_populate_kernel(&init_mm, pmd, pte);

	/* change entry in all the pgd's */
	list_for_each_entry(page, &pgd_list, lru) {
		cpa_debug("list %x %x %x\n", (unsigned long)page,
			(unsigned long)pgd_index(address), address);
		pud = pud_offset(((pgd_t *)page_address(page)) +
			pgd_index(address), address);
		pmd = pmd_offset(pud, address);
		pmd_populate_kernel(NULL, pmd, pte);
	}
	/* enforce pmd entry stores ordering to avoid tlb flush bypassing
	 * pmd entry stores.
	 */
	dsb();
}

static int
try_preserve_large_page(pte_t *kpte, unsigned long address,
			struct cpa_data *cpa)
{
	unsigned long nextpage_addr, numpages, pmask, psize, flags, addr, pfn;
	pte_t old_pte, *tmp;
	pgprot_t old_prot, new_prot, req_prot;
	unsigned long ext_prot;
	int i, do_split = 1;
	unsigned int level;

	if (cpa->force_split)
		return 1;

	spin_lock_irqsave(&pgd_lock, flags);
	/*
	 * Check for races, another CPU might have split this page
	 * up already:
	 */
	tmp = lookup_address(address, &level);
	if (tmp != kpte)
		goto out_unlock;

	switch (level) {

	case PG_LEVEL_2M:
		psize = PMD_SIZE;
		pmask = PMD_MASK;
		break;

	default:
		do_split = -EINVAL;
		goto out_unlock;
	}

	/*
	 * Calculate the number of pages, which fit into this large
	 * page starting at address:
	 */
	nextpage_addr = (address + psize) & pmask;
	numpages = (nextpage_addr - address) >> PAGE_SHIFT;
	if (numpages < cpa->numpages)
		cpa->numpages = numpages;

	old_pte = *kpte;
	old_prot = new_prot = req_prot = pmd_to_pte_pgprot(pmd_val(*kpte),
						&ext_prot);

	pgprot_val(req_prot) &= ~pgprot_val(cpa->mask_clr);
	pgprot_val(req_prot) |= pgprot_val(cpa->mask_set);

	/*
	 * old_pte points to the large page base address. So we need
	 * to add the offset of the virtual address:
	 */
	pfn = pmd_pfn(*kpte) + ((address & (psize - 1)) >> PAGE_SHIFT);
	cpa->pfn = pfn;

	new_prot = static_protections(req_prot, address, pfn);

	/*
	 * We need to check the full range, whether
	 * static_protection() requires a different pgprot for one of
	 * the pages in the range we try to preserve:
	 */
	addr = address & pmask;
	pfn = pmd_pfn(old_pte);
	for (i = 0; i < (psize >> PAGE_SHIFT); i++, addr += PAGE_SIZE, pfn++) {
		pgprot_t chk_prot = static_protections(req_prot, addr, pfn);

		if (pgprot_val(chk_prot) != pgprot_val(new_prot))
			goto out_unlock;
	}

	/*
	 * If there are no changes, return. maxpages has been updated
	 * above:
	 */
	if (pgprot_val(new_prot) == pgprot_val(old_prot)) {
		do_split = 0;
		goto out_unlock;
	}

	/*
	 * convert prot to pmd format
	 */
	new_prot = pte_to_pmd_pgprot(new_prot, ext_prot);

	/*
	 * We need to change the attributes. Check, whether we can
	 * change the large page in one go. We request a split, when
	 * the address is not aligned and the number of pages is
	 * smaller than the number of pages in the large page. Note
	 * that we limited the number of possible pages already to
	 * the number of pages in the large page.
	 */
	if (address == (nextpage_addr - psize) && cpa->numpages == numpages) {
		/*
		 * The address is aligned and the number of pages
		 * covers the full page.
		 */
		phys_addr_t phys = __pfn_to_phys(pmd_pfn(*kpte));
		pmd_t *p = (pmd_t *)kpte;

		*kpte++ = __pmd(phys | new_prot);
		*kpte   = __pmd((phys + SECTION_SIZE) | new_prot);
		flush_pmd_entry(p);
		cpa->flags |= CPA_FLUSHTLB;
		do_split = 0;
		cpa_debug("preserving page at phys %x pmd %x\n", phys, p);
	}

out_unlock:
	spin_unlock_irqrestore(&pgd_lock, flags);

	return do_split;
}

static int split_large_page(pte_t *kpte, unsigned long address)
{
	unsigned long flags, pfn, pfninc = 1;
	unsigned int i, level;
	pte_t *pbase, *tmp;
	pgprot_t ref_prot = 0;
	unsigned long ext_prot = 0;
	int ret = 0;

	BUG_ON((address & PMD_MASK) < __pa(_end));

	if (!debug_pagealloc)
		mutex_unlock(&cpa_lock);
	pbase = pte_alloc_one_kernel(&init_mm, address);
	if (!debug_pagealloc)
		mutex_lock(&cpa_lock);
	if (!pbase)
		return -ENOMEM;

	cpa_debug("split_large_page %x PMD %x new pte @ %x\n", address,
			*kpte, pbase);

	spin_lock_irqsave(&pgd_lock, flags);
	/*
	 * Check for races, another CPU might have split this page
	 * up for us already:
	 */
	tmp = lookup_address(address, &level);
	if (tmp != kpte)
		goto out_unlock;

	/*
	 * we only split 2MB entries for now
	*/
	if (level != PG_LEVEL_2M) {
		ret = -EINVAL;
		goto out_unlock;
	}

	ref_prot = pmd_to_pte_pgprot(pmd_val(*kpte), &ext_prot);

	BUG_ON(ref_prot != pgprot_kernel);
	/*
	 * Get the target pfn from the original entry:
	 */
	pfn = pmd_pfn(*kpte);
	for (i = 0; i < PTRS_PER_PTE; i++, pfn += pfninc)
		set_pte_ext(&pbase[i], pfn_pte(pfn, ref_prot), ext_prot);

	if (address >= (unsigned long)__va(0) &&
		address < (unsigned long)__va(arm_lowmem_limit))
		split_page_count(level);

	/*
	 * Install the new, split up pagetable.
	 */
	__set_pmd_pte((pmd_t *)kpte, address, pbase);

	pbase = NULL;

out_unlock:
	/*
	 * If we dropped out via the lookup_address check under
	 * pgd_lock then stick the page back into the pool:
	 */
	if (pbase)
		pte_free_kernel(&init_mm, pbase);

	spin_unlock_irqrestore(&pgd_lock, flags);

	return ret;
}

static int __cpa_process_fault(struct cpa_data *cpa, unsigned long vaddr,
			       int primary)
{
	/*
	 * Ignore all non primary paths.
	 */
	if (!primary)
		return 0;

	/*
	 * Ignore the NULL PTE for kernel identity mapping, as it is expected
	 * to have holes.
	 * Also set numpages to '1' indicating that we processed cpa req for
	 * one virtual address page and its pfn. TBD: numpages can be set based
	 * on the initial value and the level returned by lookup_address().
	 */
	if (within(vaddr, PAGE_OFFSET,
		   PAGE_OFFSET + arm_lowmem_limit)) {
		cpa->numpages = 1;
		cpa->pfn = __pa(vaddr) >> PAGE_SHIFT;
		return 0;
	} else {
		WARN(1, KERN_WARNING "CPA: called for zero pte. "
			"vaddr = %lx cpa->vaddr = %lx\n", vaddr,
			*cpa->vaddr);

		return -EFAULT;
	}
}

static int __change_page_attr(struct cpa_data *cpa, int primary)
{
	unsigned long address;
	int do_split, err;
	unsigned int level;
	pte_t *kpte, old_pte;

	if (cpa->flags & CPA_PAGES_ARRAY) {
		struct page *page = cpa->pages[cpa->curpage];

		if (unlikely(PageHighMem(page)))
			return 0;

		address = (unsigned long)page_address(page);

	} else if (cpa->flags & CPA_ARRAY)
		address = cpa->vaddr[cpa->curpage];
	else
		address = *cpa->vaddr;

repeat:
	kpte = lookup_address(address, &level);
	if (!kpte)
		return __cpa_process_fault(cpa, address, primary);

	old_pte = *kpte;
	if (!pte_val(old_pte))
		return __cpa_process_fault(cpa, address, primary);

	if (level == PG_LEVEL_4K) {
		pte_t new_pte;
		pgprot_t new_prot = pte_pgprot(old_pte);
		unsigned long pfn = pte_pfn(old_pte);

		pgprot_val(new_prot) &= ~pgprot_val(cpa->mask_clr);
		pgprot_val(new_prot) |= pgprot_val(cpa->mask_set);

		new_prot = static_protections(new_prot, address, pfn);

		/*
		 * We need to keep the pfn from the existing PTE,
		 * after all we're only going to change it's attributes
		 * not the memory it points to
		 */
		new_pte = pfn_pte(pfn, new_prot);
		cpa->pfn = pfn;

		/*
		 * Do we really change anything ?
		 */
		if (pte_val(old_pte) != pte_val(new_pte)) {
			set_pte_ext(kpte, new_pte, 0);
			/*
			 * FIXME : is this needed on arm?
			 * set_pte_ext already does a flush
			 */
			cpa->flags |= CPA_FLUSHTLB;
		}
		cpa->numpages = 1;
		return 0;
	}

	/*
	 * Check, whether we can keep the large page intact
	 * and just change the pte:
	 */
	do_split = try_preserve_large_page(kpte, address, cpa);

	/*
	 * When the range fits into the existing large page,
	 * return. cp->numpages and cpa->tlbflush have been updated in
	 * try_large_page:
	 */
	if (do_split <= 0)
		return do_split;

	/*
	 * We have to split the large page:
	 */
	err = split_large_page(kpte, address);

	if (!err) {
		/*
		 * Do a global flush tlb after splitting the large page
		 * and before we do the actual change page attribute in the PTE.
		 *
		 * With out this, we violate the TLB application note, that says
		 * "The TLBs may contain both ordinary and large-page
		 *  translations for a 4-KByte range of linear addresses. This
		 *  may occur if software modifies the paging structures so that
		 *  the page size used for the address range changes. If the two
		 *  translations differ with respect to page frame or attributes
		 *  (e.g., permissions), processor behavior is undefined and may
		 *  be implementation-specific."
		 *
		 * We do this global tlb flush inside the cpa_lock, so that we
		 * don't allow any other cpu, with stale tlb entries change the
		 * page attribute in parallel, that also falls into the
		 * just split large page entry.
		 */
		flush_tlb_all();
		goto repeat;
	}

	return err;
}

static int __change_page_attr_set_clr(struct cpa_data *cpa, int checkalias);

static int cpa_process_alias(struct cpa_data *cpa)
{
	struct cpa_data alias_cpa;
	unsigned long laddr = (unsigned long)__va(cpa->pfn << PAGE_SHIFT);
	unsigned long vaddr;
	int ret;

	if (cpa->pfn >= (arm_lowmem_limit >> PAGE_SHIFT))
		return 0;

	/*
	 * No need to redo, when the primary call touched the direct
	 * mapping already:
	 */
	if (cpa->flags & CPA_PAGES_ARRAY) {
		struct page *page = cpa->pages[cpa->curpage];
		if (unlikely(PageHighMem(page)))
			return 0;
		vaddr = (unsigned long)page_address(page);
	} else if (cpa->flags & CPA_ARRAY)
		vaddr = cpa->vaddr[cpa->curpage];
	else
		vaddr = *cpa->vaddr;

	if (!(within(vaddr, PAGE_OFFSET,
		    PAGE_OFFSET + arm_lowmem_limit))) {

		alias_cpa = *cpa;
		alias_cpa.vaddr = &laddr;
		alias_cpa.flags &= ~(CPA_PAGES_ARRAY | CPA_ARRAY);

		ret = __change_page_attr_set_clr(&alias_cpa, 0);
		if (ret)
			return ret;
	}

	return 0;
}

static int __change_page_attr_set_clr(struct cpa_data *cpa, int checkalias)
{
	int ret, numpages = cpa->numpages;

	while (numpages) {
		/*
		 * Store the remaining nr of pages for the large page
		 * preservation check.
		 */
		cpa->numpages = numpages;
		/* for array changes, we can't use large page */
		if (cpa->flags & (CPA_ARRAY | CPA_PAGES_ARRAY))
			cpa->numpages = 1;

		if (!debug_pagealloc)
			mutex_lock(&cpa_lock);
		ret = __change_page_attr(cpa, checkalias);
		if (!debug_pagealloc)
			mutex_unlock(&cpa_lock);
		if (ret)
			return ret;

		if (checkalias) {
			ret = cpa_process_alias(cpa);
			if (ret)
				return ret;
		}

		/*
		 * Adjust the number of pages with the result of the
		 * CPA operation. Either a large page has been
		 * preserved or a single page update happened.
		 */
		BUG_ON(cpa->numpages > numpages);
		numpages -= cpa->numpages;
		if (cpa->flags & (CPA_PAGES_ARRAY | CPA_ARRAY))
			cpa->curpage++;
		else
			*cpa->vaddr += cpa->numpages * PAGE_SIZE;
	}
	return 0;
}

static inline int cache_attr(pgprot_t attr)
{
	/*
	 * We need to flush the cache for all memory type changes
	 * except when a page is being marked write back cacheable
	 */
	return !((pgprot_val(attr) & L_PTE_MT_MASK) == L_PTE_MT_KERNEL);
}

static int change_page_attr_set_clr(unsigned long *addr, int numpages,
				    pgprot_t mask_set, pgprot_t mask_clr,
				    int force_split, int in_flag,
				    struct page **pages)
{
	struct cpa_data cpa;
	int ret, cache, checkalias;
	unsigned long baddr = 0;

	if (!pgprot_val(mask_set) && !pgprot_val(mask_clr) && !force_split)
		return 0;

	/* Ensure we are PAGE_SIZE aligned */
	if (in_flag & CPA_ARRAY) {
		int i;
		for (i = 0; i < numpages; i++) {
			if (addr[i] & ~PAGE_MASK) {
				addr[i] &= PAGE_MASK;
				WARN_ON_ONCE(1);
			}
		}
	} else if (!(in_flag & CPA_PAGES_ARRAY)) {
		/*
		 * in_flag of CPA_PAGES_ARRAY implies it is aligned.
		 * No need to cehck in that case
		 */
		if (*addr & ~PAGE_MASK) {
			*addr &= PAGE_MASK;
			/*
			 * People should not be passing in unaligned addresses:
			 */
			WARN_ON_ONCE(1);
		}
		/*
		 * Save address for cache flush. *addr is modified in the call
		 * to __change_page_attr_set_clr() below.
		 */
		baddr = *addr;
	}

	/* Must avoid aliasing mappings in the highmem code */
	kmap_flush_unused();

	vm_unmap_aliases();

	cpa.vaddr = addr;
	cpa.pages = pages;
	cpa.numpages = numpages;
	cpa.mask_set = mask_set;
	cpa.mask_clr = mask_clr;
	cpa.flags = 0;
	cpa.curpage = 0;
	cpa.force_split = force_split;

	if (in_flag & (CPA_ARRAY | CPA_PAGES_ARRAY))
		cpa.flags |= in_flag;

	/* No alias checking for XN bit modifications */
	checkalias = (pgprot_val(mask_set) |
				pgprot_val(mask_clr)) != L_PTE_XN;

	ret = __change_page_attr_set_clr(&cpa, checkalias);

	cache = cache_attr(mask_set);
	/*
	 * Check whether we really changed something or
	 * cache need to be flushed.
	 */
	if (!(cpa.flags & CPA_FLUSHTLB) && !cache)
		goto out;

	if (cpa.flags & (CPA_PAGES_ARRAY | CPA_ARRAY)) {
		cpa_flush_array(addr, numpages, cache,
				cpa.flags, pages);
	} else
		cpa_flush_range(baddr, numpages, cache);

out:
	return ret;
}

static inline int change_page_attr_set(unsigned long *addr, int numpages,
				       pgprot_t mask, int array)
{
	return change_page_attr_set_clr(addr, numpages, mask, __pgprot(0), 0,
		(array ? CPA_ARRAY : 0), NULL);
}

static inline int change_page_attr_clear(unsigned long *addr, int numpages,
					 pgprot_t mask, int array)
{
	return change_page_attr_set_clr(addr, numpages, __pgprot(0), mask, 0,
		(array ? CPA_ARRAY : 0), NULL);
}

static inline int cpa_set_pages_array(struct page **pages, int numpages,
				       pgprot_t mask)
{
	return change_page_attr_set_clr(NULL, numpages, mask, __pgprot(0), 0,
		CPA_PAGES_ARRAY, pages);
}

static inline int cpa_clear_pages_array(struct page **pages, int numpages,
					 pgprot_t mask)
{
	return change_page_attr_set_clr(NULL, numpages, __pgprot(0), mask, 0,
		CPA_PAGES_ARRAY, pages);
}

int set_memory_uc(unsigned long addr, int numpages)
{
	return change_page_attr_set_clr(&addr, numpages,
		__pgprot(L_PTE_MT_UNCACHED),
			__pgprot(L_PTE_MT_MASK), 0, 0, NULL);
}
EXPORT_SYMBOL(set_memory_uc);

int _set_memory_array(unsigned long *addr, int addrinarray,
		unsigned long set, unsigned long clr)
{
	return change_page_attr_set_clr(addr, addrinarray, __pgprot(set),
		__pgprot(clr), 0, CPA_ARRAY, NULL);
}

int set_memory_array_uc(unsigned long *addr, int addrinarray)
{
	return _set_memory_array(addr, addrinarray,
		L_PTE_MT_UNCACHED, L_PTE_MT_MASK);
}
EXPORT_SYMBOL(set_memory_array_uc);

int set_memory_array_wc(unsigned long *addr, int addrinarray)
{
	return _set_memory_array(addr, addrinarray,
		L_PTE_MT_BUFFERABLE, L_PTE_MT_MASK);
}
EXPORT_SYMBOL(set_memory_array_wc);

int set_memory_wc(unsigned long addr, int numpages)
{
	int ret;

	ret = change_page_attr_set_clr(&addr, numpages,
			__pgprot(L_PTE_MT_BUFFERABLE),
			__pgprot(L_PTE_MT_MASK),
			0, 0, NULL);
	return ret;
}
EXPORT_SYMBOL(set_memory_wc);

int set_memory_wb(unsigned long addr, int numpages)
{
	return change_page_attr_set_clr(&addr, numpages,
			__pgprot(L_PTE_MT_KERNEL),
			__pgprot(L_PTE_MT_MASK),
			0, 0, NULL);
}
EXPORT_SYMBOL(set_memory_wb);

int set_memory_iwb(unsigned long addr, int numpages)
{
	return change_page_attr_set_clr(&addr, numpages,
			__pgprot(L_PTE_MT_INNER_WB),
			__pgprot(L_PTE_MT_MASK),
			0, 0, NULL);
}
EXPORT_SYMBOL(set_memory_iwb);

int set_memory_array_wb(unsigned long *addr, int addrinarray)
{
	return change_page_attr_set_clr(addr, addrinarray,
			__pgprot(L_PTE_MT_KERNEL),
			__pgprot(L_PTE_MT_MASK),
			0, CPA_ARRAY, NULL);

}
EXPORT_SYMBOL(set_memory_array_wb);

int set_memory_array_iwb(unsigned long *addr, int addrinarray)
{
	return change_page_attr_set_clr(addr, addrinarray,
			__pgprot(L_PTE_MT_INNER_WB),
			__pgprot(L_PTE_MT_MASK),
			0, CPA_ARRAY, NULL);

}
EXPORT_SYMBOL(set_memory_array_iwb);

int set_memory_x(unsigned long addr, int numpages)
{
	return change_page_attr_clear(&addr, numpages,
		__pgprot(L_PTE_XN), 0);
}
EXPORT_SYMBOL(set_memory_x);

int set_memory_nx(unsigned long addr, int numpages)
{
	return change_page_attr_set(&addr, numpages,
		__pgprot(L_PTE_XN), 0);
}
EXPORT_SYMBOL(set_memory_nx);

int set_memory_ro(unsigned long addr, int numpages)
{
	return change_page_attr_set(&addr, numpages,
		__pgprot(L_PTE_RDONLY), 0);
}
EXPORT_SYMBOL_GPL(set_memory_ro);

int set_memory_rw(unsigned long addr, int numpages)
{
	return change_page_attr_clear(&addr, numpages,
		__pgprot(L_PTE_RDONLY), 0);
}
EXPORT_SYMBOL_GPL(set_memory_rw);

int set_memory_np(unsigned long addr, int numpages)
{
	return change_page_attr_clear(&addr, numpages,
		 __pgprot(L_PTE_PRESENT), 0);
}

int set_memory_4k(unsigned long addr, int numpages)
{
	return change_page_attr_set_clr(&addr, numpages, __pgprot(0),
					__pgprot(0), 1, 0, NULL);
}

static int _set_pages_array(struct page **pages, int addrinarray,
		unsigned long set, unsigned long clr)
{
	return change_page_attr_set_clr(NULL, addrinarray,
			__pgprot(set),
			__pgprot(clr),
			0, CPA_PAGES_ARRAY, pages);
}

int set_pages_array_uc(struct page **pages, int addrinarray)
{
	return _set_pages_array(pages, addrinarray,
			L_PTE_MT_UNCACHED, L_PTE_MT_MASK);
}
EXPORT_SYMBOL(set_pages_array_uc);

int set_pages_array_wc(struct page **pages, int addrinarray)
{
	return _set_pages_array(pages, addrinarray, L_PTE_MT_BUFFERABLE,
			L_PTE_MT_MASK);
}
EXPORT_SYMBOL(set_pages_array_wc);

int set_pages_array_wb(struct page **pages, int addrinarray)
{
	return _set_pages_array(pages, addrinarray,
			L_PTE_MT_KERNEL, L_PTE_MT_MASK);
}
EXPORT_SYMBOL(set_pages_array_wb);

int set_pages_array_iwb(struct page **pages, int addrinarray)
{
	return _set_pages_array(pages, addrinarray,
			L_PTE_MT_INNER_WB, L_PTE_MT_MASK);
}
EXPORT_SYMBOL(set_pages_array_iwb);

#else /* CONFIG_CPA */

void update_page_count(int level, unsigned long pages)
{
}

static void flush_cache(struct page **pages, int numpages)
{
	unsigned int i;
	bool flush_inner = true;
	unsigned long base;

#if defined(CONFIG_NVMAP_CACHE_MAINT_BY_SET_WAYS)
	if (numpages >= (cache_maint_inner_threshold >> PAGE_SHIFT)) {
		inner_flush_cache_all();
		flush_inner = false;
	}
#endif

	for (i = 0; i < numpages; i++) {
		if (flush_inner)
			__flush_dcache_page(page_mapping(pages[i]), pages[i]);
		base = page_to_phys(pages[i]);
		outer_flush_range(base, base + PAGE_SIZE);
	}
}

int set_pages_array_uc(struct page **pages, int addrinarray)
{
	flush_cache(pages, addrinarray);
	return 0;
}
EXPORT_SYMBOL(set_pages_array_uc);

int set_pages_array_wc(struct page **pages, int addrinarray)
{
	flush_cache(pages, addrinarray);
	return 0;
}
EXPORT_SYMBOL(set_pages_array_wc);

int set_pages_array_wb(struct page **pages, int addrinarray)
{
	return 0;
}
EXPORT_SYMBOL(set_pages_array_wb);

int set_pages_array_iwb(struct page **pages, int addrinarray)
{
	flush_cache(pages, addrinarray);
	return 0;
}
EXPORT_SYMBOL(set_pages_array_iwb);

#endif
