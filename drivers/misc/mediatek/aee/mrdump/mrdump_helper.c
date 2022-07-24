// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/percpu.h>
#include <linux/string.h>
#include <linux/workqueue.h>

#include <asm/memory.h>
#include <asm/pgtable.h>
#include <asm/sections.h>
#include <asm/smp_plat.h>
#include <asm/stacktrace.h>
#include <asm/system_misc.h>

#include <debug_kinfo.h>
#include <mt-plat/aee.h>
#include <mt-plat/mboot_params.h>
#include "mrdump_private.h"

#ifdef MODULE

#define NAME_LEN	128

static unsigned long *mrdump_ka;
static int *mrdump_ko;
static unsigned long _mrdump_krb;
static unsigned int _mrdump_kns;
static u8 *mrdump_kn;
static unsigned int *mrdump_km;
static u8 *mrdump_ktt;
static u16 *mrdump_kti;

#if IS_ENABLED(CONFIG_64BIT)
#define KALLS_ALGN	8
#else
#define KALLS_ALGN	4
#endif

#if IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE)
unsigned long aee_get_kn_off(void)
{
	if (!_mrdump_kns)
		return 0;

	return (unsigned long)mrdump_kn - (unsigned long)mrdump_ko;
}

unsigned long aee_get_kns_off(void)
{
	if (!_mrdump_kns)
		return 0;

	return aee_get_kn_off() - KALLS_ALGN;
}

unsigned long aee_get_km_off(void)
{
	if (!_mrdump_kns)
		return 0;

	return (unsigned long)mrdump_km - (unsigned long)mrdump_ko;
}

unsigned long aee_get_ktt_off(void)
{
	if (!_mrdump_kns)
		return 0;

	return (unsigned long)mrdump_ktt - (unsigned long)mrdump_ko;
}

unsigned long aee_get_kti_off(void)
{
	if (!_mrdump_kns)
		return 0;

	return (unsigned long)mrdump_kti - (unsigned long)mrdump_ko;
}
#endif

static int retry_nm = 100;

static void *kinfo_vaddr;

static void mrdump_ka_work_func(struct work_struct *work);
static void aee_base_addrs_init(void);

static DECLARE_DELAYED_WORK(ka_work, mrdump_ka_work_func);

int mrdump_ka_init(void *vaddr)
{
	kinfo_vaddr = vaddr;
	schedule_delayed_work(&ka_work, 0);
	return 0;
}

static void mrdump_ka_work_func(struct work_struct *work)
{
	struct kernel_all_info *dbg_kinfo;
	struct kernel_info *kinfo;

	dbg_kinfo = (struct kernel_all_info *)kinfo_vaddr;
	kinfo = &(dbg_kinfo->info);
	if (dbg_kinfo->magic_number == DEBUG_KINFO_MAGIC) {
		_mrdump_kns = kinfo->num_syms;
		_mrdump_krb = kinfo->_relative_pa + kimage_voffset;
		mrdump_ko = (void *)(kinfo->_offsets_pa + kimage_voffset);
		mrdump_kn = (void *)(kinfo->_names_pa + kimage_voffset);
		mrdump_ktt = (void *)(kinfo->_token_table_pa + kimage_voffset);
		mrdump_kti = (void *)(kinfo->_token_index_pa + kimage_voffset);
		mrdump_km = (void *)(kinfo->_markers_pa + kimage_voffset);
		aee_base_addrs_init();
		mrdump_cblock_late_init();
		init_ko_addr_list_late();
		mrdump_mini_add_klog();
		mrdump_mini_add_kallsyms();
	} else {
		pr_info("%s: retry in 0.1 second", __func__);
		if (--retry_nm >= 0)
			schedule_delayed_work(&ka_work, HZ / 10);
		else
			pr_info("%s failed\n", __func__);
	}
}

static unsigned int mrdump_checking_names(unsigned int off,
					   char *namebuf, size_t buflen)
{
	int len, skipped_first = 0;
	const u8 *tptr, *data;

	data = mrdump_kn + off;
	len = *data;
	data++;
	off += len + 1;

	while (len) {
		tptr = mrdump_ktt + *(mrdump_kti + *data);
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

static unsigned long mrdump_idx2addr(int idx)
{
	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE))
		return *(mrdump_ka + idx);

	if (!IS_ENABLED(CONFIG_KALLSYMS_ABSOLUTE_PERCPU))
		return _mrdump_krb + (u32)(*(mrdump_ko + idx));

	if (*(mrdump_ko + idx) >= 0)
		return *(mrdump_ko + idx);

	return _mrdump_krb - 1 - *(mrdump_ko + idx);
}

static unsigned long aee_addr_find(const char *name)
{
	char strbuf[NAME_LEN];
	unsigned long i;
	unsigned int off;

	if (!_mrdump_kns)
		return 0;

	for (i = 0, off = 0; i < _mrdump_kns; i++) {
		off = mrdump_checking_names(off, strbuf, ARRAY_SIZE(strbuf));

		if (strcmp(strbuf, name) == 0)
			return mrdump_idx2addr(i);
	}
	return 0;
}

static unsigned long p_stext;
unsigned long aee_get_stext(void)
{
	if (p_stext)
		return p_stext;

	p_stext = aee_addr_find("_stext");

	if (!p_stext)
		pr_info("%s failed", __func__);
	return p_stext;
}

static unsigned long p_etext;
unsigned long aee_get_etext(void)
{
	if (p_etext)
		return p_etext;

	p_etext = aee_addr_find("_etext");

	if (!p_etext)
		pr_info("%s failed", __func__);
	return p_etext;
}

static unsigned long p_text;
unsigned long aee_get_text(void)
{
	if (p_text)
		return p_text;

	p_text = aee_addr_find("_text");

	if (!p_text)
		pr_info("%s failed", __func__);
	return p_text;
}

#ifdef CONFIG_MODULES
static struct list_head *p_modules;
struct list_head *aee_get_modules(void)
{

	if (p_modules)
		return p_modules;

	p_modules = (void *)aee_addr_find("modules");

	if (!p_modules) {
		pr_info("%s failed", __func__);
		return NULL;
	}

	return p_modules;
}
#endif

static void *p_log_ptr;
void *aee_log_buf_addr_get(void)
{
	if (p_log_ptr)
		return p_log_ptr;

	p_log_ptr = (void *)(aee_addr_find("prb"));

	if (p_log_ptr)
		return p_log_ptr;

	pr_info("%s failed", __func__);
	return NULL;
}

unsigned long aee_get_kallsyms_addresses(void)
{
	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE))
		return (unsigned long)mrdump_ka;
	return (unsigned long)mrdump_ko;
}

unsigned long aee_get_kti_addresses(void)
{
	return (unsigned long)mrdump_kti;
}

static raw_spinlock_t *p_die_lock;
void aee_reinit_die_lock(void)
{
	if (!p_die_lock) {
		p_die_lock = (void *)aee_addr_find("die_lock");
		if (!p_die_lock) {
			aee_sram_printk("%s failed to get die_lock\n",
					__func__);
			return;
		}
	}

	/* If a crash is occurring, make sure we can't deadlock */
	raw_spin_lock_init(p_die_lock);
}

/* find the addrs needed during driver init stage */
static void aee_base_addrs_init(void)
{
	char strbuf[NAME_LEN];
	unsigned long i;
	unsigned int off;
	unsigned int search_num = 5;

#ifndef CONFIG_MODULES
	search_num--;
#endif

	for (i = 0, off = 0; i < _mrdump_kns; i++) {
		if (!search_num)
			return;
		off = mrdump_checking_names(off, strbuf, ARRAY_SIZE(strbuf));

#ifdef CONFIG_MODULES
		if (!p_modules && strcmp(strbuf, "modules") == 0) {
			p_modules = (void *)mrdump_idx2addr(i);
			search_num--;
			continue;
		}
#endif

		if (!p_etext && strcmp(strbuf, "_etext") == 0) {
			p_etext = mrdump_idx2addr(i);
			search_num--;
			continue;
		}

		if (!p_stext && strcmp(strbuf, "_stext") == 0) {
			p_stext = mrdump_idx2addr(i);
			search_num--;
			continue;
		}

		if (!p_text && strcmp(strbuf, "_text") == 0) {
			p_text = mrdump_idx2addr(i);
			search_num--;
			continue;
		}

		if (!p_log_ptr && strcmp(strbuf, "prb") == 0) {
			p_log_ptr = (void *)mrdump_idx2addr(i);
			search_num--;
			continue;
		}
	}
	if (search_num)
		pr_info("mrdump addr init incomplete %d\n", search_num);
}
#else /* #ifdef MODULE*/

unsigned long aee_get_stext(void)
{
	return (unsigned long)_stext;
}

unsigned long aee_get_etext(void)
{
	return (unsigned long)_etext;
}

unsigned long aee_get_text(void)
{
	return (unsigned long)_text;
}

#ifdef CONFIG_MODULES
static struct list_head *p_modules;
struct list_head *aee_get_modules(void)
{

	if (p_modules)
		return p_modules;

	p_modules = (void *)kallsyms_lookup_name("modules");

	if (!p_modules) {
		pr_info("%s failed", __func__);
		return NULL;
	}

	return p_modules;
}
#endif

static void *p_log_ptr;
void *aee_log_buf_addr_get(void)
{
	if (p_log_ptr)
		return p_log_ptr;

	p_log_ptr = (void *)(kallsyms_lookup_name("prb"));

	if (p_log_ptr)
		return p_log_ptr;

	pr_info("%s failed", __func__);
	return NULL;
}

unsigned long aee_get_kallsyms_addresses(void)
{
	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE))
		return (unsigned long)kallsyms_addresses;
	return (unsigned long)kallsyms_offsets;
}

unsigned long aee_get_kti_addresses(void)
{
	return (unsigned long)kallsyms_token_index;
}

static raw_spinlock_t *p_die_lock;
void aee_reinit_die_lock(void)
{
	if (!p_die_lock) {
		p_die_lock = (void *)kallsyms_lookup_name("die_lock");
		if (!p_die_lock) {
			aee_sram_printk("%s failed to get die_lock\n",
					__func__);
			return;
		}
	}

	/* If a crash is occurring, make sure we can't deadlock */
	raw_spin_lock_init(p_die_lock);
}

#endif

