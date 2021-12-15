// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include "mdla_debug.h"
#include "gsm.h"
#include "mdla.h"

#include <linux/bitmap.h>
#include <asm/page.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>

#define GSM_BITMAP_SIZE  (GSM_SIZE/PAGE_SIZE)
DECLARE_BITMAP(gsm_bitmap, GSM_BITMAP_SIZE);
DEFINE_SPINLOCK(gsm_lock);

void *gsm_alloc(size_t size)
{
	unsigned long flags;
	int pageno;
	int order = get_order(size);

	spin_lock_irqsave(&gsm_lock, flags);
	pageno = bitmap_find_free_region(
		gsm_bitmap, GSM_BITMAP_SIZE, order);
	spin_unlock_irqrestore(&gsm_lock, flags);
	if (unlikely(pageno < 0))
		return NULL;
	return apu_mdla_gsm_top + (pageno << PAGE_SHIFT);
}

void *gsm_mva_to_virt(u32 mva)
{
	return (void *)(mva - GSM_MVA_BASE + apu_mdla_gsm_top);
}

#define is_gsm_virt(a) \
	(likely(((a) >= apu_mdla_gsm_top) && \
	((a) < (apu_mdla_gsm_top + SZ_1M))))

#define gsm_virt_offset(a) \
	((a) - apu_mdla_gsm_top)

static u32 gsm_virt_to_mva(void *vaddr)
{
	if (is_gsm_virt(vaddr))
		return (u32)(gsm_virt_offset(vaddr) + GSM_MVA_BASE);

	return GSM_MVA_INVALID;
}

static void *gsm_virt_to_pa(void *vaddr)
{
	if (is_gsm_virt(vaddr))
		return (apu_mdla_gsm_base + gsm_virt_offset(vaddr));

	return NULL;
}

int gsm_release(void *vaddr, size_t size)
{
	unsigned long flags;
	int order = get_order(size);
	int page = (vaddr - apu_mdla_gsm_top) >> PAGE_SHIFT;

	if ((page >= GSM_BITMAP_SIZE) || (page < 0))
		return -EINVAL;

	spin_lock_irqsave(&gsm_lock, flags);
	bitmap_release_region(gsm_bitmap, page, order);
	spin_unlock_irqrestore(&gsm_lock, flags);

	return 0;
}

int mdla_gsm_alloc(struct ioctl_malloc *malloc_data)
{
	int ret = 0;
	int ex = 0;
	void *ptr;
	u32 size = malloc_data->size;

	if (size == GSM_MVA_INVALID) {
		size = GSM_SIZE;
		mdla_mem_debug("%s: <exclusive mode>\n", __func__);
		ex = 1;
	}
	ptr = gsm_alloc(size);

	if (ptr == NULL)
		ret = -ENOMEM;

	malloc_data->size = size;
	malloc_data->kva = (ex) ? apu_mdla_gsm_top : ptr;
	malloc_data->mva = gsm_virt_to_mva(malloc_data->kva);
	malloc_data->pa = gsm_virt_to_pa(malloc_data->kva);
#ifdef CONFIG_FPGA_EARLY_PORTING
	/* avoid bus hang in fpga stage */
	malloc_data->pa = (void *)((long) malloc_data->mva);
#endif
	mdla_mem_debug("%s: ret: %d, kva:%p, mva:%x, pa:%p, size:%x\n",
		__func__, ret,
		malloc_data->kva, malloc_data->mva,
		malloc_data->pa, malloc_data->size);

	return ret;
}

void mdla_gsm_free(struct ioctl_malloc *malloc_data)
{
	if (malloc_data->size > GSM_SIZE)
		return;
	if (malloc_data->kva)
		gsm_release(malloc_data->kva, malloc_data->size);
	mdla_mem_debug("%s: kva:%p, mva:%x, pa:%p, size:%x\n", __func__,
		malloc_data->kva, malloc_data->mva,
		malloc_data->pa, malloc_data->size);
}

