/*
 * Copyright (C) 2015 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/types.h>
#include <linux/printk.h>
#include <gz-trusty/trusty.h>
#include <gz-trusty/smcall.h>

static int get_mem_attr(struct page *page, pgprot_t pgprot)
{
#if IS_ENABLED(CONFIG_ARM64)
	uint64_t mair;
	uint attr_index = (pgprot_val(pgprot) & PTE_ATTRINDX_MASK) >> 2;

	asm ("mrs %0, mair_el1\n" : "=&r" (mair));
	return (mair >> (attr_index * 8)) & 0xff;

#elif IS_ENABLED(CONFIG_ARM_LPAE)
	uint32_t mair;
	uint attr_index = ((pgprot_val(pgprot) & L_PTE_MT_MASK) >> 2);

	if (attr_index >= 4) {
		attr_index -= 4;
		asm volatile("mrc p15, 0, %0, c10, c2, 1\n" : "=&r" (mair));
	} else {
		asm volatile("mrc p15, 0, %0, c10, c2, 0\n" : "=&r" (mair));
	}
	return (mair >> (attr_index * 8)) & 0xff;

#elif IS_ENABLED(CONFIG_ARM)
	/* check memory type */
	switch (pgprot_val(pgprot) & L_PTE_MT_MASK) {
	case L_PTE_MT_WRITEALLOC:
		/* Normal: write back write allocate */
		return 0xFF;

	case L_PTE_MT_BUFFERABLE:
		/* Normal: non-cacheble */
		return 0x44;

	case L_PTE_MT_WRITEBACK:
		/* Normal: writeback, read allocate */
		return 0xEE;

	case L_PTE_MT_WRITETHROUGH:
		/* Normal: write through */
		return 0xAA;

	case L_PTE_MT_UNCACHED:
		/* strongly ordered */
		return 0x00;

	case L_PTE_MT_DEV_SHARED:
	case L_PTE_MT_DEV_NONSHARED:
		/* device */
		return 0x04;

	default:
		return -EINVAL;
	}
#else
	return 0;
#endif
}

int trusty_encode_page_info(struct ns_mem_page_info *inf,
			    struct page *page, pgprot_t pgprot)
{
	int mem_attr;
	uint64_t pte;

	if (!inf || !page)
		return -EINVAL;

	/* get physical address */
	pte = (uint64_t) page_to_phys(page);

	/* get memory attributes */
	mem_attr = get_mem_attr(page, pgprot);
	if (mem_attr < 0)
		return mem_attr;

	/* add other attributes */
#if IS_ENABLED(CONFIG_ARM64) || IS_ENABLED(CONFIG_ARM_LPAE)
	pte |= pgprot_val(pgprot);
#elif IS_ENABLED(CONFIG_ARM)
	if (pgprot_val(pgprot) & L_PTE_USER)
		pte |= (1 << 6);
	if (pgprot_val(pgprot) & L_PTE_RDONLY)
		pte |= (1 << 7);
	if (pgprot_val(pgprot) & L_PTE_SHARED)
		pte |= (3 << 8); /* inner sharable */
#endif

	inf->attr = (pte & 0x0000FFFFFFFFFFFFull) | ((uint64_t)mem_attr << 48);
	return 0;
}

int trusty_call32_mem_buf(struct device *dev, u32 smcnr,
			  struct page *page,  u32 size,
			  pgprot_t pgprot)
{
	int ret;
	struct ns_mem_page_info pg_inf;

	if (!dev || !page)
		return -EINVAL;

	ret = trusty_encode_page_info(&pg_inf, page, pgprot);
	if (ret)
		return ret;

	if (SMC_IS_FASTCALL(smcnr)) {
		return trusty_fast_call32(dev, smcnr,
					  (u32)pg_inf.attr,
					  (u32)(pg_inf.attr >> 32), size);
	} else {
		return trusty_std_call32(dev, smcnr,
					 (u32)pg_inf.attr,
					 (u32)(pg_inf.attr >> 32), size);
	}
}

