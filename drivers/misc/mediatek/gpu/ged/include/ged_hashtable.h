/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __GED_HASH_TABLE_H__
#define __GED_HASH_TABLE_H__

typedef void* GED_HASHTABLE_HANDLE;

GED_HASHTABLE_HANDLE ged_hashtable_create(unsigned int ui32Bits);

void ged_hashtable_destroy(GED_HASHTABLE_HANDLE hHashTable);

GED_ERROR ged_hashtable_insert(GED_HASHTABLE_HANDLE hHashTable, void* pvoid, unsigned int* pui32ID);

void ged_hashtable_remove(GED_HASHTABLE_HANDLE hHashTable, unsigned int ui32ID);

void* ged_hashtable_find(GED_HASHTABLE_HANDLE hHashTable, unsigned int ui32ID);

GED_ERROR ged_hashtable_set(GED_HASHTABLE_HANDLE hHashTable, unsigned int ui32ID, void* pvoid);

#endif
