// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 * Author: Kuan-Ying Lee <Kuan-Ying.Lee@mediatek.com>
 */

#include "ksym.h"

static unsigned long *mkp_ka;
static int *mkp_ko;
static unsigned long *mkp_krb;
static unsigned int *mkp_kns;
static u8 *mkp_kn;
static unsigned int *mkp_km;
static u8 *mkp_ktt;
static u16 *mkp_kti;

void *mkp_abt_addr(void *ssa)
{
	void *pos;
	u8 abt[SM_SIZE];
	int i;
	unsigned long s_left;

	for (i = 0; i < SM_SIZE; i++) {
		if (i & 0x1)
			abt[i] = 0;
		else
			abt[i] = 0x61 + i / 2;
	}

	if ((unsigned long)ssa >= KV + S_MAX) {
		pr_info("out of range: 0xLx\n", ssa);
		return NULL;
	}

	pos = ssa;
	s_left = KV + S_MAX - (unsigned long)ssa;
	while ((u64)pos < (u64)(KV + S_MAX)) {
		pos = memchr(pos, 'a', s_left);

		if (!pos) {
			pr_info("fail at: 0x%lx @ 0x%x\n", ssa, s_left);
			return NULL;
		}
		s_left = KV + S_MAX - (unsigned long)pos;

		if (!memcmp(pos, (const void *)abt, sizeof(abt)))
			return pos;

		pos += 1;
	}

	pr_info("fail at end: 0x%lx @ 0x%x\n", ssa, s_left);
	return NULL;
}

unsigned long *mkp_krb_addr(void)
{
	void *abt_addr = (void *)KV;
	void *ssa = (void *)KV;
	unsigned long *pos;

	while((u64)ssa < KV + S_MAX) {
		abt_addr = mkp_abt_addr(ssa);
		if (!abt_addr) {
			pr_info("krb not found: 0x%lx\n", ssa);
			return NULL;
		}

		abt_addr = (void *)round_up((unsigned long)abt_addr, 8);
		for (pos = (unsigned long *)abt_addr;
			(u64)pos > (u64)ssa ; pos--) {
			if ((u64)pos == (u64)&kimage_vaddr)
				break;
			if (*pos == KV)
				return pos;
		}
		ssa = abt_addr + 1;
	}

	pr_info("krb not found: 0x%lx\n", ssa);
	return NULL;
}

unsigned int *mkp_km_addr(void)
{
	const u8 *name = mkp_kn;
	unsigned int loop = *mkp_kns;

	while (loop--)
		name = name + (*name) + 1;

	return (unsigned int *)round_up((unsigned long)name, 8);
}

u16 *mkp_kti_addr(void)
{
	const u8 *pch = mkp_ktt;
	int loop = TT_SIZE;

	while (loop--) {
		for (; *pch; pch++)
			;
		pch++;
	}

	return (u16 *)round_up((unsigned long)pch, 8);
}

int mkp_ka_init(void)
{
	unsigned int kns;

	mkp_krb = mkp_krb_addr();
	if (!mkp_krb)
		return -EINVAL;

	mkp_kns = (unsigned int *)(mkp_krb + 1);
	mkp_kn = (u8 *)(mkp_krb + 2);
	kns = *mkp_kns;
	mkp_ko = (int *)(mkp_krb - (round_up(kns, 2) / 2));
	mkp_km = mkp_km_addr();
	mkp_ktt = (u8 *)round_up((unsigned long)(mkp_km +
				    (round_up(kns, 256) / 256)), 8);
	mkp_kti = mkp_kti_addr();

	return 0;
}

unsigned int mkp_checking_names(unsigned int off,
					   char *namebuf, size_t buflen)
{
	int len, skipped_first = 0;
	const u8 *tptr, *data;

	data = mkp_kn + off;
	len = *data;
	data++;
	off += len + 1;

	while (len) {
		tptr = mkp_ktt + *(mkp_kti + *data);
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

unsigned long mkp_idx2addr(int idx)
{
	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE))
		return *(mkp_ka + idx);

	if (!IS_ENABLED(CONFIG_KALLSYMS_ABSOLUTE_PERCPU))
		return *mkp_krb + (u32)(*(mkp_ko + idx));

	if (*(mkp_ko + idx) >= 0)
		return *(mkp_ko + idx);

	return *mkp_krb - 1 - *(mkp_ko + idx);
}

unsigned long mkp_addr_find(const char *name)
{
	char strbuf[NAME_LEN];
	unsigned long i;
	unsigned int off;

	for (i = 0, off = 0; i < *mkp_kns; i++) {
		off = mkp_checking_names(off, strbuf, ARRAY_SIZE(strbuf));

		if (strcmp(strbuf, name) == 0)
			return mkp_idx2addr(i);
	}
	return 0;
}

void mkp_get_krn_code(void **p_stext, void **p_etext)
{
	if (*p_stext && *p_etext)
		return;

	*p_stext = (void *)(mkp_addr_find("_stext"));
	*p_etext = (void *)(mkp_addr_find("_etext"));

	if (!(*p_etext)) {
		pr_info("%s: _stext not found\n", __func__);
		return;
	}
	if (!(*p_etext)) {
		pr_info("%s: _etext not found\n", __func__);
		return;
	}
	pr_info("_stext: %px, _etext: %px\n", *p_stext, *p_etext);
	return;
}

void mkp_get_krn_rodata(void **p_etext, void **p__init_begin)
{
	if (*p_etext && *p__init_begin)
		return;

	*p_etext = (void *)(mkp_addr_find("_etext"));
	*p__init_begin = (void *)(mkp_addr_find("__init_begin"));

	if (!(*p_etext)) {
		pr_info("%s: _etext not found\n", __func__);
		return;
	}
	if (!(*p__init_begin)) {
		pr_info("%s: __init_begin not found\n", __func__);
		return;
	}
	pr_info("_etext: %px, __init_begin: %px\n", *p_etext, *p__init_begin);
	return;
}
