/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/bug.h>
#include <linux/crc32.h>
#include <linux/delay.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/io.h>
#include <asm/sections.h>
#include <mt-plat/mrdump.h>
#include "mrdump_private.h"

struct mrdump_control_block *mrdump_cblock;
struct mrdump_rsvmem_block mrdump_sram_cb;

int mrdump_rsv_conflict;
struct mrdump_rsvmem_block __initdata rsvmem_block[4];

static __init char *find_next_mrdump_rsvmem(char *p, int len)
{
	char *tmp_p;

	tmp_p = memchr(p, ',', len);
	if (!tmp_p)
		return NULL;
	if (*(tmp_p+1) != 0) {
		tmp_p = memchr(tmp_p+1, ',', strlen(tmp_p));
		if (!tmp_p)
			return NULL;
	} else{
		return NULL;
	}
	return tmp_p + 1;
}
static int __init early_mrdump_rsvmem(char *p)
{
	unsigned long start_addr, size;
	int ret;
	char *tmp_p = p;
	int i;

	for (i = 0; i < 4; i++) {
		ret = sscanf(tmp_p, "0x%lx,0x%lx", &start_addr, &size);
		if (ret != 2) {
			pr_notice("%s:%s reserve failed ret=%d\n", __func__,
					p, ret);
			return 0;
		}
		rsvmem_block[i].start_addr = start_addr;
		rsvmem_block[i].size = size;
		tmp_p = find_next_mrdump_rsvmem(tmp_p, strlen(tmp_p));
		if (!tmp_p)
			break;
	}

	return 0;
}

__init void mrdump_rsvmem(void)
{
	int i;

	for (i = 0; i < 4; i++) {
		if (rsvmem_block[i].start_addr) {
			if (!memblock_is_region_reserved(
					rsvmem_block[i].start_addr,
					rsvmem_block[i].size))
				memblock_reserve(rsvmem_block[i].start_addr,
					rsvmem_block[i].size);
			else {
				/*
				 * even conflict , we still enable MRDUMP for
				 * temp because MINI DUMP will reserve the
				 * memory in DTSI now
				 */
#if 0
				mrdump_rsv_conflict = 1;
				mrdump_enable = 0;
#endif
				pr_notice(" mrdump region start = %pa size =%pa is reserved already\n",
						&rsvmem_block[i].start_addr,
						&rsvmem_block[i].size);
			}
		}
	}
}

early_param("mrdump_rsvmem", early_mrdump_rsvmem);

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
#if defined(KIMAGE_VADDR)
	kparam->start_addr = start_addr - KIMAGE_VADDR + PHYS_OFFSET;
#else
	kparam->start_addr = __pa(start_addr);
#endif
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

__init void mrdump_cblock_init(void)
{
	struct mrdump_machdesc *machdesc_p;

	if ((mrdump_sram_cb.start_addr == 0) || (mrdump_sram_cb.size == 0)) {
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
	if (mrdump_cblock == NULL) {
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
	machdesc_p->kimage_init_begin = (uintptr_t)__init_begin;
	machdesc_p->kimage_init_end = (uintptr_t)__init_end;
	machdesc_p->kimage_stext = (uintptr_t)_text;
	machdesc_p->kimage_etext = (uintptr_t)_etext;
	machdesc_p->kimage_srodata = (uintptr_t)__start_rodata;
	machdesc_p->kimage_erodata = (uintptr_t)__init_begin;
	machdesc_p->kimage_sdata = (uintptr_t)_sdata;
	machdesc_p->kimage_edata = (uintptr_t)_edata;

	machdesc_p->vmalloc_start = (uint64_t)VMALLOC_START;
	machdesc_p->vmalloc_end = (uint64_t)VMALLOC_END;

	machdesc_p->modules_start = (uint64_t)MODULES_VADDR;
	machdesc_p->modules_end = (uint64_t)MODULES_END;

	machdesc_p->phys_offset = (uint64_t)(phys_addr_t)PHYS_OFFSET;
	machdesc_p->master_page_table = (uintptr_t)__pa(&swapper_pg_dir);

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

#if !defined(CONFIG_MTK_AEE_MRDUMP)

int __init mrdump_init(void)
{
	mrdump_cblock_init();
	return 0;
}

static atomic_t waiting_for_crash_ipi;

static void mrdump_stop_noncore_cpu(void *unused)
{
	local_irq_disable();
	dis_D_inner_fL1L2();
	while (1)
		cpu_relax();
}

static void __mrdump_reboot_stop_all(void)
{
	unsigned long msecs;

	atomic_set(&waiting_for_crash_ipi, num_online_cpus() - 1);
	smp_call_function(mrdump_stop_noncore_cpu, NULL, false);

	msecs = 1000; /* Wait at most a second for the other cpus to stop */
	while ((atomic_read(&waiting_for_crash_ipi) > 0) && msecs) {
		mdelay(1);
		msecs--;
	}
	if (atomic_read(&waiting_for_crash_ipi) > 0)
		pr_notice("Non-crashing CPUs did not react to IPI\n");
}

void __mrdump_create_oops_dump(enum AEE_REBOOT_MODE reboot_mode,
		struct pt_regs *regs, const char *msg, ...)
{
	local_irq_disable();

#if defined(CONFIG_SMP)
	__mrdump_reboot_stop_all();
#endif
}

#endif
