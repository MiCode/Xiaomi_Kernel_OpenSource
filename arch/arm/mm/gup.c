#include <linux/mm.h>
#include <linux/uaccess.h>
#include <linux/pagemap.h>
#include <asm/pgtable.h>

static inline pte_t gup_get_pte(pte_t *ptep)
{
	return READ_ONCE(*ptep);
}

static int gup_pte_range(pmd_t pmd, unsigned long addr, unsigned long end,
			int write, struct page **pages, int *nr)
{
	int ret = 0;
	pte_t *ptep, *ptem;

	ptem = ptep = pte_offset_map(&pmd, addr);
	do {
		pte_t pte = gup_get_pte(ptep);
		struct page *page;

		/*
		 * Similar to the PMD case below, NUMA hinting must take slow
		 * path using the pte_protnone check.
		 */
		if (pte_protnone(pte))
			goto pte_unmap;

		if (!pte_present(pte) || pte_special(pte) ||
			pte_protnone(pte) || (write && !pte_write(pte)))
			goto pte_unmap;

		if (pte_special(pte))
			goto pte_unmap;

		VM_BUG_ON(!pfn_valid(pte_pfn(pte)));
		page = pte_page(pte);

		if (!page_cache_get_speculative(page))
			goto pte_unmap;

		if (unlikely(pte_val(pte) != pte_val(*ptep))) {
			put_page(page);
			goto pte_unmap;
		}

		SetPageReferenced(page);
		pages[*nr] = page;
		(*nr)++;

	} while (ptep++, addr += PAGE_SIZE, addr != end);

	ret = 1;

pte_unmap:
	pte_unmap(ptem);
	return ret;
}

static int gup_pmd_range(pud_t pud, unsigned long addr, unsigned long end,
		int write, struct page **pages, int *nr)
{
	unsigned long next;
	pmd_t *pmdp;

	pmdp = pmd_offset(&pud, addr);
	do {
		pmd_t pmd = READ_ONCE(*pmdp);

		next = pmd_addr_end(addr, end);
		if (!pmd_present(pmd))
			return 0;
		else if (!gup_pte_range(pmd, addr, next, write, pages, nr))
			return 0;
	} while (pmdp++, addr = next, addr != end);

	return 1;
}

static int gup_pud_range(pgd_t *pgd, unsigned long addr, unsigned long end,
		int write, struct page **pages, int *nr)
{
	unsigned long next;
	pud_t *pudp;

	pudp = pud_offset(pgd, addr);
	do {
		pud_t pud = READ_ONCE(*pudp);

		next = pud_addr_end(addr, end);
		if (pud_none(pud))
			return 0;
		else if (!gup_pmd_range(pud, addr, next, write, pages, nr))
			return 0;
	} while (pudp++, addr = next, addr != end);

	return 1;
}

static void gup_pgd_range(unsigned long addr, unsigned long end,
		int write, struct page **pages, int *nr)
{
	unsigned long next;
	pgd_t *pgdp;

	pgdp = pgd_offset(current->mm, addr);
	do {
		next = pgd_addr_end(addr, end);
		if (pgd_none(*pgdp))
			return;
		else if (!gup_pud_range(pgdp, addr, next, write, pages, nr))
			break;
	} while (pgdp++, addr = next, addr != end);
}

bool gup_fast_permitted(unsigned long start, int nr_pages, int write)
{
	unsigned long len, end;

	len = (unsigned long) nr_pages << PAGE_SHIFT;
	end = start + len;
	return end >= start;
}

/*
 * Like get_user_pages_fast() except its IRQ-safe in that it won't fall
 * back to the regular GUP.
 */
int __get_user_pages_fast(unsigned long start, int nr_pages, int write,
			  struct page **pages)
{
	unsigned long addr, len, end;
	unsigned long flags;
	int nr = 0;

	start &= PAGE_MASK;
	addr = start;
	len = (unsigned long) nr_pages << PAGE_SHIFT;
	end = start + len;

	if (unlikely(!access_ok(write ? VERIFY_WRITE : VERIFY_READ,
					(void __user *)start, len)))
		return 0;

	/*
	 * Disable interrupts.  We use the nested form as we can already have
	 * interrupts disabled by get_futex_key.
	 *
	 * With interrupts disabled, we block page table pages from being
	 * freed from under us. See mmu_gather_tlb in asm-generic/tlb.h
	 * for more details.
	 *
	 * We do not adopt an rcu_read_lock(.) here as we also want to
	 * block IPIs that come from THPs splitting.
	 */

	if (gup_fast_permitted(start, nr_pages, write)) {
		local_irq_save(flags);
		gup_pgd_range(addr, end, write, pages, &nr);
		local_irq_restore(flags);
	}

	return nr;
}

int get_user_pages_fast(unsigned long start, int nr_pages, int write,
			struct page **pages)
{
	unsigned long addr, len, end;
	int nr = 0, ret = 0;

	start &= PAGE_MASK;
	addr = start;
	len = (unsigned long) nr_pages << PAGE_SHIFT;
	end = start + len;

	if (nr_pages <= 0)
		return 0;

	if (unlikely(!access_ok(write ? VERIFY_WRITE : VERIFY_READ,
					(void __user *)start, len)))
		return -EFAULT;

	if (gup_fast_permitted(start, nr_pages, write)) {
		local_irq_disable();
		gup_pgd_range(addr, end, write, pages, &nr);
		local_irq_enable();
		ret = nr;
	}

	if (nr < nr_pages) {
		/* Try to get the remaining pages with get_user_pages */
		start += nr << PAGE_SHIFT;
		pages += nr;

		ret = get_user_pages_unlocked(start, nr_pages - nr, pages,
				write ? FOLL_WRITE : 0);

		/* Have to be a bit careful with return values */
		if (nr > 0) {
			if (ret < 0)
				ret = nr;
			else
				ret += nr;
		}
	}

	return ret;
}
