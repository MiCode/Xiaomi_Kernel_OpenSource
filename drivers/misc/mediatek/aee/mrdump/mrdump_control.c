// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <asm/memory.h>
#include <asm/sections.h>
#include <mt-plat/mrdump.h>
#include "mrdump_private.h"

struct mrdump_control_block *mrdump_cblock;
struct mrdump_rsvmem_block mrdump_sram_cb;

/* mrdump_cb info from lk */
static int __init mrdump_get_cb(char *p)
{
	unsigned long cbaddr, cbsize;
	int ret;

	ret = sscanf(p, "0x%lx,0x%lx", &cbaddr, &cbsize);
	if (ret != 2) {
		pr_notice("%s: no mrdump_sram_cb. (ret=%d, p=%s)\n",
			 __func__, ret, p);
	} else {
		mrdump_sram_cb.start_addr = cbaddr;
		mrdump_sram_cb.size = cbsize;
		pr_notice("%s: mrdump_cbaddr=%pa, mrdump_cbsize=%pa\n",
			 __func__,
			 &mrdump_sram_cb.start_addr,
			 &mrdump_sram_cb.size
			 );
	}

	return 0;
}
early_param("mrdump_cb", mrdump_get_cb);

#if defined(CONFIG_KALLSYMS) && !defined(CONFIG_KALLSYMS_BASE_RELATIVE)
static void mrdump_cblock_kallsyms_init(struct mrdump_ksyms_param *kparam)
{
	unsigned long start_addr = (unsigned long) &kallsyms_addresses;

	kparam->tag[0] = 'K';
	kparam->tag[1] = 'S';
	kparam->tag[2] = 'Y';
	kparam->tag[3] = 'M';

	switch (sizeof(unsigned long)) {
	case 4:
		kparam->flag = KSYM_32;
		break;
	case 8:
		kparam->flag = KSYM_64;
		break;
	default:
		BUILD_BUG();
	}
	kparam->start_addr = __pa_symbol(start_addr);
	kparam->size = (unsigned long)&kallsyms_token_index - start_addr + 512;
	kparam->crc = crc32(0, (unsigned char *)start_addr, kparam->size);
	kparam->addresses_off = (unsigned long)&kallsyms_addresses - start_addr;
	kparam->num_syms_off = (unsigned long)&kallsyms_num_syms - start_addr;
	kparam->names_off = (unsigned long)&kallsyms_names - start_addr;
	kparam->markers_off = (unsigned long)&kallsyms_markers - start_addr;
	kparam->token_table_off =
		(unsigned long)&kallsyms_token_table - start_addr;
	kparam->token_index_off =
		(unsigned long)&kallsyms_token_index - start_addr;
}
#else
static void mrdump_cblock_kallsyms_init(struct mrdump_ksyms_param *unused)
{
}

#endif

__init void mrdump_cblock_init(void)
{
	struct mrdump_machdesc *machdesc_p;

	if (!mrdump_sram_cb.start_addr || !mrdump_sram_cb.size) {
		pr_notice("%s: no mrdump_cb\n", __func__);
		goto end;
	}

	if (mrdump_sram_cb.size < sizeof(struct mrdump_control_block)) {
		pr_notice("%s: not enough space for mrdump control block\n",
			  __func__);
		goto end;
	}

	mrdump_cblock = ioremap_wc(mrdump_sram_cb.start_addr,
				   mrdump_sram_cb.size);
	if (!mrdump_cblock) {
		pr_notice("%s: mrdump_cb not mapped\n", __func__);
		goto end;
	}
	memset_io(mrdump_cblock, 0, sizeof(struct mrdump_control_block));
	memcpy_toio(mrdump_cblock->sig, MRDUMP_GO_DUMP,
			sizeof(mrdump_cblock->sig));

	machdesc_p = &mrdump_cblock->machdesc;
	machdesc_p->nr_cpus = AEE_MTK_CPU_NUMS;
	machdesc_p->page_offset = (uint64_t)PAGE_OFFSET;
	machdesc_p->high_memory = (uintptr_t)high_memory;

#if defined(KIMAGE_VADDR)
	machdesc_p->kimage_vaddr = KIMAGE_VADDR;
#endif
#if defined(TEXT_OFFSET)
	machdesc_p->kimage_vaddr += TEXT_OFFSET;
#endif
	machdesc_p->dram_start = (uintptr_t)memblock_start_of_DRAM();
	machdesc_p->dram_end = (uintptr_t)memblock_end_of_DRAM();
	machdesc_p->kimage_stext = (uintptr_t)_text;
	machdesc_p->kimage_etext = (uintptr_t)_etext;
	machdesc_p->kimage_stext_real = (uintptr_t)_stext;
#if defined(CONFIG_ARM64)
	machdesc_p->kimage_voffset = kimage_voffset;
#endif
	machdesc_p->kimage_sdata = (uintptr_t)_sdata;
	machdesc_p->kimage_edata = (uintptr_t)_edata;

	machdesc_p->vmalloc_start = (uint64_t)VMALLOC_START;
	machdesc_p->vmalloc_end = (uint64_t)VMALLOC_END;

	machdesc_p->modules_start = (uint64_t)MODULES_VADDR;
	machdesc_p->modules_end = (uint64_t)MODULES_END;

	machdesc_p->phys_offset = (uint64_t)(phys_addr_t)PHYS_OFFSET;
	if (virt_addr_valid(&swapper_pg_dir)) {
		machdesc_p->master_page_table =
			(uintptr_t)__pa(&swapper_pg_dir);
	} else {
		machdesc_p->master_page_table =
			(uintptr_t)__pa_symbol(&swapper_pg_dir);
	}

#if defined(CONFIG_SPARSEMEM_VMEMMAP)
	machdesc_p->memmap = (uintptr_t)vmemmap;
#endif
	mrdump_cblock_kallsyms_init(&machdesc_p->kallsyms);
	mrdump_cblock->machdesc_crc = crc32(0, machdesc_p,
			sizeof(struct mrdump_machdesc));

	pr_notice("%s: done.\n", __func__);

end:
	__inner_flush_dcache_all();
}
