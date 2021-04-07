/*
 * Copyright (C) 2015-2018 Google, Inc.
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
#include <linux/trusty/trusty.h>
#include <linux/trusty/smcall.h>

#if defined(CONFIG_ARM64)
static int encode_page_inf(struct ns_mem_page_info *inf, uint64_t pte,
			   pgprot_t pgprot, bool writable)
{
	uint64_t mair;
	uint attr_index = (pgprot_val(pgprot) & PTE_ATTRINDX_MASK) >> 2;

	asm ("mrs %0, mair_el1\n" : "=&r" (mair));
	mair = (mair >> (attr_index * 8)) & 0xff;

	/* add other attributes  */
	pte |= pgprot_val(pgprot);

	/* reset RDONLY flag if needed */
	if (writable)
		pte &= ~PTE_RDONLY;

	inf->attr = (pte & 0x0000FFFFFFFFFFFFull) | (mair << 48);
	return 0;
}
#elif defined(CONFIG_ARM_LPAE)
static int encode_page_inf(struct ns_mem_page_info *inf, uint64_t pte,
			   pgprot_t pgprot, bool writable)
{
	u32 mair;
	uint attr_index = ((pgprot_val(pgprot) & L_PTE_MT_MASK) >> 2);

	if (attr_index >= 4) {
		attr_index -= 4;
		asm volatile("mrc p15, 0, %0, c10, c2, 1\n" : "=&r" (mair));
	} else {
		asm volatile("mrc p15, 0, %0, c10, c2, 0\n" : "=&r" (mair));
	}
	mair = (mair >> (attr_index * 8)) & 0xff;

	/* add other attributes  */
	pte |= pgprot_val(pgprot);

	/* reset RDONLY flag if needed */
	if (writable)
		pte &= ~(1 << 7);

	inf->attr = (pte & 0x0000FFFFFFFFFFFFull) | ((uint64_t)mair << 48);
	return 0;
}
#elif defined(CONFIG_ARM)
static int encode_page_inf(struct ns_mem_page_info *inf, uint64_t pte,
			   pgprot_t pgprot, bool writable)
{
	u32 mair;

	/* check memory type */
	switch (pgprot_val(pgprot) & L_PTE_MT_MASK) {
	case L_PTE_MT_WRITEALLOC:
		/* Normal: write back write allocate */
		mair = 0xFF;
		break;

	case L_PTE_MT_BUFFERABLE:
		/* Normal: non-cacheble */
		mair = 0x44;
		break;

	case L_PTE_MT_WRITEBACK:
		/* Normal: writeback, read allocate */
		mair = 0xEE;
		break;

	case L_PTE_MT_WRITETHROUGH:
		/* Normal: write through */
		mair = 0xAA;
		break;

	case L_PTE_MT_UNCACHED:
		/* strongly ordered */
		mair = 0x00;
		break;

	case L_PTE_MT_DEV_SHARED:
	case L_PTE_MT_DEV_NONSHARED:
		/* device */
		mair = 0x04;
		break;

	default:
		return -EINVAL;
	}

	/* add other attributes */
	if (pgprot_val(pgprot) & L_PTE_USER)
		pte |= (1 << 6);
	if (!writable && pgprot_val(pgprot) & L_PTE_RDONLY)
		pte |= (1 << 7);
	if (pgprot_val(pgprot) & L_PTE_SHARED)
		pte |= (3 << 8); /* inner sharable */

	inf->attr = (pte & 0x0000FFFFFFFFFFFFull) | ((uint64_t)mair << 48);
	return 0;
}
#else
static int encode_page_inf(struct ns_mem_page_info *inf, uint64_t pte,
			   pgprot_t pgprot, bool writable)
{
	return -EINVAL;
}
#endif

int trusty_encode_page_info(struct ns_mem_page_info *inf, struct page *page,
			    pgprot_t pgprot, bool writable)
{
	if (!inf || !page)
		return -EINVAL;

	return encode_page_inf(inf, (uint64_t)page_to_phys(page),
			       pgprot, writable);
}

int trusty_call32_mem_buf(struct device *dev, u32 smcnr,
			  struct page *page,  u32 size,
			  pgprot_t pgprot, bool writable)
{
	int ret;
	struct ns_mem_page_info pg_inf;

	if (!dev || !page)
		return -EINVAL;

	ret = trusty_encode_page_info(&pg_inf, page, pgprot, writable);
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

