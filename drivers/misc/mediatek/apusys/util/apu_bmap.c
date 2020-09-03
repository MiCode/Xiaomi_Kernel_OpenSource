// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/bitmap.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "mdw_cmn.h"
#include "apu_bmap.h"

/**
 * Initialize bitmap<br>
 * <b>Inputs</b><br>
 *   ab->start: start address.<br>
 *   ab->end: end address.<br>
 *   ab->au: allocation unit.<br>
 *   ab->align_mask: alignment mask in terms of au, 0 = no alignment.<br>
 * <b>Outputs</b><br>
 *   ab->size: equals to (ab->end - ab->start).<br>
 *   ab->b: allocated bitmap.<br>
 *   ab->nbits: number of bits in the bitmap(ab->b).<br>
 *   ab->lock: spin lock used to protect the bitmap.<br>
 * @param[in,out] ab The apu bitmap object
 * @param[in] name The name of this bitmap object
 * @return 0: Success.<br>
 *   EINVAL: start, end, or size is not aligned to allocation unit.
 */
int apu_bmap_init(struct apu_bmap *ab, const char *name)
{
	if (!ab || ab->start > ab->end)
		return -EINVAL;

	memset(ab->name, 0, APU_BMAP_NAME_LEN);
	strncpy(ab->name, name, APU_BMAP_NAME_LEN - 1);

	/* size must be multiple of allocation unit */
	ab->size = ab->end - ab->start;
	mdw_mem_debug("%s: %s: start: 0x%x, end: 0x%x, size: 0x%x, au: 0x%x\n",
		__func__, ab->name, ab->start, ab->end, ab->size, ab->au);
	if (!is_au_align(ab, ab->size)) {
		mdw_drv_warn("%s: %s: size 0x%x is un-aligned to AU 0x%x\n",
			__func__, ab->name, ab->size, ab->au);
		return -EINVAL;
	}

	ab->nbits = ab->size / ab->au;
	ab->b = bitmap_zalloc(ab->nbits, GFP_KERNEL);
	spin_lock_init(&ab->lock);

	return 0;
}

/**
 * Release bitmap
 * @param[in] ab The apu bitmap object to be released
 */
void apu_bmap_exit(struct apu_bmap *ab)
{
	if (!ab || !ab->b)
		return;

	bitmap_free(ab->b);
	ab->b = NULL;
}

/**
 * Allocate addresses from bitmap
 * @param[in] ab The apu bitmap object to be allocated from.
 * @param[in] size Desired allocation size in bytes.
 * @param[in] given_addr Search begin from the given address,
 *            0 = Searches from ab->start
 * @return 0: Allocation failed.<br>
 *    Others: Allocated address.<br>
 * @remark: Searches free addresses from begin (ab->start).<br>
 *    You have to check the returned address for static iova mapping
 */
uint32_t apu_bmap_alloc(struct apu_bmap *ab, unsigned int size,
	uint32_t given_addr)
{
	uint32_t addr = 0;
	unsigned int nr;
	unsigned long offset;
	unsigned long start = 0;
	unsigned long flags;

	if (!ab)
		return 0;

	if (given_addr) {
		start = given_addr - ab->start;
		if (!is_au_align(ab, start)) {
			mdw_drv_warn("%s: %s: size: 0x%x, given addr: 0x%x, start 0x%x is un-aligned to AU 0x%x\n",
				__func__, ab->name, size, addr, start, ab->au);
			return 0;
		}
		start = start / ab->au;
	}

	spin_lock_irqsave(&ab->lock, flags);
	nr = round_up(size, ab->au) / ab->au;

	offset = bitmap_find_next_zero_area(ab->b, ab->nbits, start,
		nr, ab->align_mask);

	if (offset >= ab->nbits) {
		mdw_drv_warn("%s: %s: Out of memory: size: 0x%x, given addr: 0x%x, offset: %d, nbits: %d\n",
			__func__, ab->name, size, addr, offset, ab->nbits);
		goto out;
	}

	addr = offset * ab->au + ab->start;
	__bitmap_set(ab->b, offset, nr);

out:
	spin_unlock_irqrestore(&ab->lock, flags);

	if (addr)
		mdw_mem_debug("%s: %s: size: 0x%x, given_addr: 0x%x, allocated addr: 0x%x\n",
		__func__, ab->name, size, given_addr, addr);

	return addr;
}

/**
 * Free occupied addresses from bitmap
 * @param[in] ab The apu bitmap object.
 * @param[in] addr Allocated start address returned by apu_bmap_alloc().
 * @param[in] size Allocated size.
 */
void apu_bmap_free(struct apu_bmap *ab, uint32_t addr, unsigned int size)
{
	unsigned int nr;
	unsigned long offset;
	unsigned long flags;

	if (!ab || addr < ab->start || (addr + size) > ab->end)
		return;

	nr = round_up(size, ab->au) / ab->au;
	offset = addr - ab->start;

	mdw_mem_debug("%s: %s: addr: 0x%x, size: 0x%x, nr_bits: %d\n",
		__func__, ab->name, addr, size, nr);

	if (!is_au_align(ab, offset)) {
		mdw_drv_warn("%s: %s: addr 0x%x, offset 0x%x is un-aligned to AU 0x%x\n",
			__func__, ab->name, addr, offset, ab->au);
		return;
	}

	offset = offset / ab->au;
	if (offset >= ab->nbits) {
		mdw_drv_warn("%s: %s: addr 0x%x, offset-bit %d is out of limit %d\n",
			__func__, ab->name, addr, offset, ab->nbits);
		return;
	}

	spin_lock_irqsave(&ab->lock, flags);
	__bitmap_clear(ab->b, offset, nr);
	spin_unlock_irqrestore(&ab->lock, flags);
}

