/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __GED_HASH_TABLE_H__
#define __GED_HASH_TABLE_H__

#include "ged_type.h"

//typedef void *GED_HASHTABLE_HANDLE;
#define GED_HASHTABLE_HANDLE void*


GED_HASHTABLE_HANDLE ged_hashtable_create(unsigned int ui32Bits);

void ged_hashtable_destroy(GED_HASHTABLE_HANDLE hHashTable);

GED_ERROR ged_hashtable_insert(GED_HASHTABLE_HANDLE hHashTable, void *pvoid,
	unsigned long *pulID);

void ged_hashtable_remove(GED_HASHTABLE_HANDLE hHashTable, unsigned long ulID);

void *ged_hashtable_find(GED_HASHTABLE_HANDLE hHashTable, unsigned long ulID);

GED_ERROR ged_hashtable_set(GED_HASHTABLE_HANDLE hHashTable,
	unsigned long ulID, void *pvoid);

void ged_hashtable_iterator(GED_HASHTABLE_HANDLE hHashTable,
	GED_BOOL(*iterator)(unsigned long ulID, void *pvoid, void *pvParam),
	void *pvParam);

void *ged_hashtable_search(GED_HASHTABLE_HANDLE hHashTable,
	void* (*pFunc)(unsigned long ulID, void *pvoid, void *pvParam),
	void *pvParam);

void ged_hashtable_iterator_delete(GED_HASHTABLE_HANDLE hHashTable,
	GED_BOOL(*pFunc)(unsigned long ulID, void *pvoid, void *pvParam,
	GED_BOOL * pbDeleted),
	void *pvParam);

unsigned long ged_hashtable_get_count(GED_HASHTABLE_HANDLE hHashTable);

#endif
