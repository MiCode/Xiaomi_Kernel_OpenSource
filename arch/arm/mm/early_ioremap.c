/*
 * early_ioremap() support for ARM
 *
 * Based on existing support in arch/x86/mm/ioremap.c
 *
 * Restrictions: currently only functional before paging_init()
 */

#include <linux/init.h>
#include <linux/io.h>

#include <asm/fixmap.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>

#include <asm/mach/map.h>

static pte_t bm_pte[PTRS_PER_PTE + PTE_HWTABLE_PTRS]
	 __aligned(PTE_HWTABLE_OFF + PTE_HWTABLE_SIZE) __initdata;

static inline pmd_t * __init early_ioremap_pmd(unsigned long addr)
{
	unsigned int index = pgd_index(addr);
	pgd_t *pgd = cpu_get_pgd() + index;
	pud_t *pud = pud_offset(pgd, addr);
	pmd_t *pmd = pmd_offset(pud, addr);

	return pmd;
}

static inline pte_t * __init early_ioremap_pte(unsigned long addr)
{
	return &bm_pte[pte_index(addr)];
}

void __init early_ioremap_init(void)
{
	pmd_t *pmd;

	pmd = early_ioremap_pmd(fix_to_virt(FIX_BTMAP_BEGIN));

	pmd_populate_kernel(NULL, pmd, bm_pte);

	/*
	* Make sure we don't span multiple pmds.
	*/
	BUILD_BUG_ON((__fix_to_virt(FIX_BTMAP_BEGIN) >> PMD_SHIFT)
			!= (__fix_to_virt(FIX_BTMAP_END) >> PMD_SHIFT));

	if (pmd != early_ioremap_pmd(fix_to_virt(FIX_BTMAP_END))) {
		WARN_ON(1);
		pr_warn("pmd %p != %p\n",
			pmd, early_ioremap_pmd(fix_to_virt(FIX_BTMAP_END)));
		pr_warn("fix_to_virt(FIX_BTMAP_BEGIN): %08lx\n",
			fix_to_virt(FIX_BTMAP_BEGIN));
		pr_warn("fix_to_virt(FIX_BTMAP_END):   %08lx\n",
			fix_to_virt(FIX_BTMAP_END));
		pr_warn("FIX_BTMAP_END:       %d\n", FIX_BTMAP_END);
		pr_warn("FIX_BTMAP_BEGIN:     %d\n", FIX_BTMAP_BEGIN);
	}

	early_ioremap_setup();
}

void __init __early_set_fixmap(enum fixed_addresses idx,
				phys_addr_t phys, pgprot_t flags)
{
	unsigned long addr = __fix_to_virt(idx);
	pte_t *pte;
	u64 desc;

	if (idx > FIX_KMAP_END) {
		BUG();
		return;
	}
	pte = early_ioremap_pte(addr);

	if (pgprot_val(flags))
		set_pte_at(NULL, addr, pte, pfn_pte(phys >> PAGE_SHIFT, flags));
	else
		pte_clear(NULL, addr, pte);
	flush_tlb_kernel_range(addr, addr + PAGE_SIZE);
	desc = *pte;
}

void __init early_ioremap_shutdown(void)
{
	int i;
	pmd_clear(early_ioremap_pmd(fix_to_virt(FIX_BTMAP_BEGIN)));

	 /* Create new entries for permanent mappings */
	for (i = 0; i < __end_of_permanent_fixed_addresses; i++) {
		pte_t *pte;
		struct map_desc map;

		map.virtual = fix_to_virt(i);
		pte = early_ioremap_pte(map.virtual);

		/* Only i/o device mappings are supported ATM */
		if (pte_none(*pte) ||
		   (pte_val(*pte) & L_PTE_MT_MASK) != L_PTE_MT_DEV_SHARED)
			continue;

		map.pfn = pte_pfn(*pte);
		map.type = MT_DEVICE;
		map.length = PAGE_SIZE;

		create_mapping(&map);
	}
}
