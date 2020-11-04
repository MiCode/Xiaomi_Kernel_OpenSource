// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/percpu.h>
#include <linux/string.h>

#include <asm/memory.h>
#include <asm/pgtable.h>
#include <asm/sections.h>
#include <asm/smp_plat.h>
#include <asm/stacktrace.h>
#include <asm/system_misc.h>

struct module_sect_attr {
	struct bin_attribute battr;
	unsigned long address;
};

struct module_sect_attrs {
	struct attribute_group grp;
	unsigned int nsections;
	struct module_sect_attr attrs[0];
};

#define KV		kimage_vaddr
#define S_MAX		SZ_32M
#define SM_SIZE		28
#define TT_SIZE		256
#define NAME_LEN	128

static unsigned long *mb_ka;
static int *mb_ko;
static unsigned long *mb_krb;
static unsigned int *mb_kns;
static u8 *mb_kn;
static unsigned int *mb_km;
static u8 *mb_ktt;
static u16 *mb_kti;

static void *mb_abt_addr(void)
{
	void *ssa = (void *)(KV);
	void *pos;
	u8 abt[SM_SIZE];
	int i;
	u32 s_left;

	for (i = 0; i < SM_SIZE; i++) {
		if (i & 0x1)
			abt[i] = 0;
		else
			abt[i] = 0x61 + i / 2;
	}

	pos = ssa;
	s_left = S_MAX;
	while ((u64)pos < (u64)(ssa + S_MAX)) {
		pos = memchr(pos, 'a', s_left);

		if (!pos) {
			pr_info("fail at: 0x%lx @ 0x%x\n", ssa, s_left);
			return NULL;
		}
		s_left = ssa + S_MAX - pos;

		if (!memcmp(pos, (const void *)abt, sizeof(abt)))
			return pos;

		pos += 1;
	}

	pr_info("fail at end: 0x%lx @ 0x%x\n", ssa, s_left);
	return NULL;
}

static unsigned long *mb_krb_addr(void)
{
	void *abt_addr = mb_abt_addr();
	unsigned long *ssa;
	int i;

	if (!abt_addr)
		return NULL;

	ssa = (unsigned long *)round_up((unsigned long)abt_addr, 8);

	for (i = 0; i < SZ_1M ; i++) {
		if (*ssa == KV)
			return ssa;
		ssa--;
	}

	pr_info("krb not found: 0x%lx\n", ssa);
	return NULL;
}

static unsigned int *mb_km_addr(void)
{
	const u8 *name = mb_kn;
	unsigned int loop = *mb_kns;

	while (loop--)
		name = name + (*name) + 1;

	return (unsigned int *)round_up((unsigned long)name, 8);
}

static u16 *mb_kti_addr(void)
{
	const u8 *pch = mb_ktt;
	int loop = TT_SIZE;

	while (loop--) {
		for (; *pch; pch++)
			;
		pch++;
	}

	return (u16 *)round_up((unsigned long)pch, 8);
}

int mb_ka_init(void)
{
	unsigned int kns;

	mb_krb = mb_krb_addr();
	if (!mb_krb)
		return -EINVAL;

	mb_kns = (unsigned int *)(mb_krb + 1);
	mb_kn = (u8 *)(mb_krb + 2);
	kns = *mb_kns;
	mb_ko = (int *)(mb_krb - (round_up(kns, 2) / 2));
	mb_km = mb_km_addr();
	mb_ktt = (u8 *)round_up((unsigned long)(mb_km +
				    (round_up(kns, 256) / 256)), 8);
	mb_kti = mb_kti_addr();

	return 0;
}

static unsigned int mb_checking_names(unsigned int off,
					   char *namebuf, size_t buflen)
{
	int len, skipped_first = 0;
	const u8 *tptr, *data;

	data = mb_kn + off;
	len = *data;
	data++;
	off += len + 1;

	while (len) {
		tptr = mb_ktt + *(mb_kti + *data);
		data++;
		len--;

		while (*tptr) {
			if (skipped_first) {
				if (buflen <= 1)
					goto tail;
				*namebuf = *tptr;
				namebuf++;
				buflen--;
			} else
				skipped_first = 1;
			tptr++;
		}
	}

tail:
	if (buflen)
		*namebuf = '\0';

	return off;
}

static unsigned long mb_idx2addr(int idx)
{
	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE))
		return *(mb_ka + idx);

	if (!IS_ENABLED(CONFIG_KALLSYMS_ABSOLUTE_PERCPU))
		return *mb_krb + (u32)(*(mb_ko + idx));

	if (*(mb_ko + idx) >= 0)
		return *(mb_ko + idx);

	return *mb_krb - 1 - *(mb_ko + idx);
}

static unsigned long mb_addr_find(const char *name)
{
	char strbuf[NAME_LEN];
	unsigned long i;
	unsigned int off;

	for (i = 0, off = 0; i < *mb_kns; i++) {
		off = mb_checking_names(off, strbuf, ARRAY_SIZE(strbuf));

		if (strcmp(strbuf, name) == 0)
			return mb_idx2addr(i);
	}
	return 0;
}

static struct memblock *p_memblock;
static struct memblock *mb_get_memblock(void)
{
	if (p_memblock)
		return p_memblock;

	p_memblock = (void *)(mb_addr_find("memblock"));

	if (!p_memblock) {
		pr_info("%s failed", __func__);
		return NULL;
	}
	return p_memblock;
}

phys_addr_t mb_start_of_DRAM(void)
{
	struct memblock *memblockp = mb_get_memblock();

	if (!memblockp) {
		pr_info("%s failed", __func__);
		return 0;
	}

	return memblockp->memory.regions[0].base;
}
EXPORT_SYMBOL_GPL(mb_start_of_DRAM);

phys_addr_t mb_end_of_DRAM(void)
{
	struct memblock *memblockp = mb_get_memblock();
	int idx;

	if (!memblockp) {
		pr_info("%s failed", __func__);
		return 0;
	}

	idx = memblockp->memory.cnt - 1;

	return (memblockp->memory.regions[idx].base +
		memblockp->memory.regions[idx].size);
}
EXPORT_SYMBOL_GPL(mb_end_of_DRAM);

static int __init mb_init(void)
{
	int ret;

	return 0;
	ret = mb_ka_init();
	if (ret) {
		pr_info("mb_ka_init failed: %d", ret);
		return ret;
	}

	mb_get_memblock();
	return 0;
}

arch_initcall(mb_init);
MODULE_AUTHOR("Zhiyong Wang <zhiyong.wang@mediatek.com>");
MODULE_AUTHOR("Stanley Chu <stanley chu@mediatek.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Memory Block");
