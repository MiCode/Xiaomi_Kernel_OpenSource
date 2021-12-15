/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __HIMAX_MODULAR_TABLE_H__
#define __HIMAX_MODULAR_TABLE_H__

#include "himax_ic_core.h"

#define TO_STR(VAR)	#VAR

enum modular_table {
	MODULE_NOT_FOUND = -1,
	MODULE_FOUND,
	MODULE_EMPTY,
};

#if defined(HX_USE_KSYM)
#define DECLARE(sym) struct himax_chip_entry sym; \
			EXPORT_SYMBOL(sym)
static const char * const himax_ksym_lookup[] = {
	#if defined(HX_MOD_KSYM_HX852xG)
	TO_STR(HX_MOD_KSYM_HX852xG),
	#endif
	#if defined(HX_MOD_KSYM_HX852xH)
	TO_STR(HX_MOD_KSYM_HX852xH),
	#endif
	#if defined(HX_MOD_KSYM_HX83102)
	TO_STR(HX_MOD_KSYM_HX83102),
	#endif
	#if defined(HX_MOD_KSYM_HX83103)
	TO_STR(HX_MOD_KSYM_HX83103),
	#endif
	#if defined(HX_MOD_KSYM_HX83106)
	TO_STR(HX_MOD_KSYM_HX83106),
	#endif
	#if defined(HX_MOD_KSYM_HX83111)
	TO_STR(HX_MOD_KSYM_HX83111),
	#endif
	#if defined(HX_MOD_KSYM_HX83112)
	TO_STR(HX_MOD_KSYM_HX83112),
	#endif
	#if defined(HX_MOD_KSYM_HX83113)
	TO_STR(HX_MOD_KSYM_HX83113),
	#endif
	#if defined(HX_MOD_KSYM_HX83192)
	TO_STR(HX_MOD_KSYM_HX83192),
	#endif
	#if defined(HX_MOD_KSYM_HX83191)
	TO_STR(HX_MOD_KSYM_HX83191),
	#endif
	NULL
};

static struct himax_chip_entry *get_chip_entry_by_index(int32_t idx)
{
	return  (void *)kallsyms_lookup_name(himax_ksym_lookup[idx]);
}

/*
 * Return 1 when specified entry is empty, 0 when not, -1 when index error
 */
static int32_t isEmpty(int32_t idx)
{
	int32_t size = sizeof(himax_ksym_lookup) / sizeof(char *);
	struct himax_chip_entry *entry;

	if (idx < 0 || idx >= size)
		return MODULE_NOT_FOUND;

	entry = get_chip_entry_by_index(idx);
	if (entry)
		return (entry->core_chip_dt == NULL)?MODULE_EMPTY:MODULE_FOUND;

	return MODULE_NOT_FOUND;
}

/*
 * Search for created entry, if not existed, return 1st
 * Return index of himax_ksym_lookup
 */
static int32_t himax_get_ksym_idx(void)
{
	int32_t i, first = -1;
	int32_t size = sizeof(himax_ksym_lookup) / sizeof(char *);
	struct himax_chip_entry *entry;

	I("%s: symtable size: %d\n", __func__, size);
	for (i = 0; i < size; i++) {
		if (himax_ksym_lookup[i] == NULL)
			break;

		I("%s: %s\n", __func__, himax_ksym_lookup[i]);
		entry = get_chip_entry_by_index(i);
		if (entry) {
			if (first < 0)
				first = i;
			if (isEmpty(i) == 0)
				return i;
		}
	}
	if (first >= 0)
		return first;
	/*incorrect use state, means no ic defined*/
	return MODULE_NOT_FOUND;
}

#else
#define DECLARE(sym)
extern struct himax_chip_entry himax_ksym_lookup;

static struct himax_chip_entry *get_chip_entry_by_index(int32_t idx)
{
	return  &himax_ksym_lookup;
}

static int32_t isEmpty(int32_t idx)
{
	struct himax_chip_entry *entry;

	if (idx < 0)
		return MODULE_NOT_FOUND;

	entry = get_chip_entry_by_index(idx);
	if (entry)
		return (entry->core_chip_dt == NULL)?MODULE_EMPTY:MODULE_FOUND;

	return MODULE_NOT_FOUND;
}

static int32_t himax_get_ksym_idx(void)
{
	int32_t size;

	size = himax_ksym_lookup.hx_ic_dt_num;

	I("%s: symtable size: %d\n", __func__, size);
	return 0;
}

#endif



#endif
