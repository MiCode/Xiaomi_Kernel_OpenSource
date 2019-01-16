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
