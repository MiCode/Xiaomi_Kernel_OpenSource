// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_HASH_TABLE_H__
#define __GED_HASH_TABLE_H__

#include "ged_type.h"

typedef void *GED_HASHTABLE_HANDLE;

typedef GED_BOOL (*ged_hashtable_iterator_func_type)(unsigned long ulID, void *pvoid, void *pvParam);

typedef void* (*ged_hashtable_search_func_type)(unsigned long ulID, void *pvoid, void *pvParam);

typedef GED_BOOL (*ged_hashtable_iterator_delete_func_type)(unsigned long ulID, void *pvoid, void *pvParam, GED_BOOL *pbDeleted);

GED_HASHTABLE_HANDLE ged_hashtable_create(unsigned int ui32Bits);

void ged_hashtable_destroy(GED_HASHTABLE_HANDLE hHashTable);

GED_ERROR ged_hashtable_insert(GED_HASHTABLE_HANDLE hHashTable, void *pvoid, unsigned long *pulID);

void ged_hashtable_remove(GED_HASHTABLE_HANDLE hHashTable, unsigned long ulID);

void *ged_hashtable_find(GED_HASHTABLE_HANDLE hHashTable, unsigned long ulID);

GED_ERROR ged_hashtable_set(GED_HASHTABLE_HANDLE hHashTable, unsigned long ulID, void *pvoid);

void ged_hashtable_iterator(GED_HASHTABLE_HANDLE hHashTable, ged_hashtable_iterator_func_type iterator, void *pvParam);

void *ged_hashtable_search(GED_HASHTABLE_HANDLE hHashTable, ged_hashtable_search_func_type pFunc, void *pvParam);

void ged_hashtable_iterator_delete(GED_HASHTABLE_HANDLE hHashTable,
									ged_hashtable_iterator_delete_func_type pFunc,
									void *pvParam);

unsigned long ged_hashtable_get_count(GED_HASHTABLE_HANDLE hHashTable);

#endif
