#ifndef _ASM_FIXMAP_H
#define _ASM_FIXMAP_H

#include <linux/kernel.h>
#include <asm/pgtable.h>
#include <asm/kmap_types.h>

#define FIXADDR_START		0xffc00000UL
#define FIXADDR_END		0xfff00000UL
#define FIXADDR_TOP		(FIXADDR_END - PAGE_SIZE)

/*
 * 224 temporary boot-time mappings, used by early_ioremap(),
 * before ioremap() is functional.
 *
 * (P)re-using the FIXADDR region, which is used for highmem
 * later on, and statically aligned to 1MB.
 */
#define NR_FIX_BTMAPS          32
#define FIX_BTMAPS_SLOTS       7
#define TOTAL_FIX_BTMAPS       (NR_FIX_BTMAPS * FIX_BTMAPS_SLOTS)

enum fixed_addresses {
	FIX_EARLYCON_MEM_BASE,
	FIX_SMP_MEM_BASE,
	__end_of_permanent_fixed_addresses,
	FIX_BTMAP_END = __end_of_permanent_fixed_addresses,
	FIX_BTMAP_BEGIN = FIX_BTMAP_END + TOTAL_FIX_BTMAPS - 1,

	FIX_KMAP_BEGIN = __end_of_permanent_fixed_addresses,
	FIX_KMAP_END = FIX_KMAP_BEGIN + (KM_TYPE_NR * NR_CPUS) - 1,
	/* Support writing RO kernel text via kprobes, jump labels, etc. */
	FIX_TEXT_POKE0,
	FIX_TEXT_POKE1,

	__end_of_fixed_addresses = (FIXADDR_END - FIXADDR_START) >> PAGE_SHIFT,
};

#define FIXMAP_PAGE_COMMON (L_PTE_YOUNG | L_PTE_PRESENT | L_PTE_XN)

#define FIXMAP_PAGE_NORMAL (FIXMAP_PAGE_COMMON | L_PTE_MT_WRITEBACK)
#define FIXMAP_PAGE_IO    (FIXMAP_PAGE_COMMON | L_PTE_MT_DEV_SHARED | L_PTE_SHARED | L_PTE_DIRTY)
#define FIXMAP_PAGE_NOCACHE FIXMAP_PAGE_IO

extern void __early_set_fixmap(enum fixed_addresses idx,
					phys_addr_t phys, pgprot_t flags);

void __set_fixmap(enum fixed_addresses idx, phys_addr_t phys, pgprot_t prot);

#include <asm-generic/fixmap.h>

#endif
