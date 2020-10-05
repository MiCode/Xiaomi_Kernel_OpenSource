// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "ged_base.h"
#include "ged_hashtable.h"
#include <linux/hashtable.h>

struct GED_HASHTABLE {
	unsigned int		version;
	unsigned int		ui32Bits;
	unsigned int		ui32Tmp;
	unsigned long		ulLength;
	unsigned long		ulCount;
	unsigned long		ulCurrentID;
	struct list_head	*psHashTable;
};

#define HT_VERSION sizeof(struct GED_HASHTABLE)

struct GED_HASHNODE {
	unsigned long		ulID;
	void				*pvoid;
	struct list_head	sList;
};

#define GED_HASHTABLE_INIT_ID 1234 // 0 = invalid

static struct GED_HASHTABLE
*__ged_hashtable_verify(GED_HASHTABLE_HANDLE hHashTable)
{
	struct GED_HASHTABLE *psHT = (struct GED_HASHTABLE *)hHashTable;

	if (psHT && psHT->version == HT_VERSION)
		return psHT;
	return NULL;
}

static void *__ged_hashtable_find(struct list_head *psList, unsigned long ulID)
{
	struct GED_HASHNODE *psHN;
	struct list_head *psListEntry, *psListEntryTemp;

	list_for_each_safe(psListEntry, psListEntryTemp, psList) {
		psHN = list_entry(psListEntry, struct GED_HASHNODE, sList);
		if (psHN && (psHN->ulID == ulID))
			return psHN;
	}
	return NULL;
}

static unsigned long ged_hash(struct GED_HASHTABLE *psHT, unsigned long ulID)
{
	return hash_long(ulID, psHT->ui32Bits);
}

GED_HASHTABLE_HANDLE ged_hashtable_create(unsigned int ui32Bits)
{
	struct GED_HASHTABLE *psHT;
	unsigned long i;

	if (ui32Bits > 20) {
		// 1048576 slots !?
		// Need to check the necessary
		return NULL;
	}

	psHT = (struct GED_HASHTABLE *)
		ged_alloc_atomic(sizeof(struct GED_HASHTABLE));
	if (psHT) {
		psHT->version = HT_VERSION;
		psHT->ui32Bits = ui32Bits;
		psHT->ulLength = (unsigned long)(1 << ui32Bits);
		psHT->ulCount = 0;
		psHT->ulCurrentID = GED_HASHTABLE_INIT_ID; /* 0 = invalid */
		psHT->psHashTable = (struct list_head *)
		ged_alloc_atomic(psHT->ulLength * sizeof(struct list_head));

		if (psHT->psHashTable) {
			for (i = 0; i < psHT->ulLength; i++)
				INIT_LIST_HEAD(&psHT->psHashTable[i]);
			return (GED_HASHTABLE_HANDLE)psHT;
		}
	}

	ged_hashtable_destroy(psHT);
	return NULL;
}

void ged_hashtable_destroy(GED_HASHTABLE_HANDLE hHashTable)
{
	struct GED_HASHTABLE *psHT = __ged_hashtable_verify(hHashTable);

	if (psHT) {
		unsigned long i = 0;
		struct list_head *psListEntry, *psListEntryTemp;

		for (; i < psHT->ulLength; i++) {
			struct list_head *psList = &psHT->psHashTable[i];

			list_for_each_safe(psListEntry,
				psListEntryTemp, psList) {
				struct GED_HASHNODE *psHN =
					list_entry(psListEntry,
					struct GED_HASHNODE, sList);

				if (psHN) {
					list_del(&psHN->sList);
					ged_free(psHN,
						sizeof(struct GED_HASHNODE));
				}
			}
		}

		psHT->version = 0xf2eef2ee;

		/* free the hash table */
		ged_free(psHT->psHashTable,
			psHT->ulLength * sizeof(struct list_head));
		ged_free(psHT, sizeof(struct GED_HASHTABLE));
	}
}

GED_ERROR ged_hashtable_insert(GED_HASHTABLE_HANDLE hHashTable,
	void *pvoid, unsigned long *pulID)
{
	struct GED_HASHTABLE *psHT = __ged_hashtable_verify(hHashTable);
	struct GED_HASHNODE *psHN = NULL;
	unsigned long ulHash, ulID;
	GED_BOOL bFindSlot = GED_FALSE;

	if ((!psHT) || (!pulID))
		return GED_ERROR_INVALID_PARAMS;

	ulID = psHT->ulCurrentID + 1;

	while (1) {
		ulHash = ged_hash(psHT, ulID);
		psHN = __ged_hashtable_find(&psHT->psHashTable[ulHash], ulID);
		if (psHN != NULL) {
			ulID++;
			if (ulID == 0) /*skip the value 0 */
				ulID = 1;
			if (ulID == psHT->ulCurrentID) {
				bFindSlot = GED_FALSE;
				break;
			}
		} else {
			bFindSlot = GED_TRUE;
			break;
		}
	};

	if (bFindSlot == GED_FALSE)
		return GED_ERROR_FAIL;

	psHN =
	(struct GED_HASHNODE *)ged_alloc_atomic(sizeof(struct GED_HASHNODE));

	if (psHN) {
		psHN->pvoid = pvoid;
		psHN->ulID = ulID;
		*pulID = ulID;
		INIT_LIST_HEAD(&psHN->sList);
		psHT->ulCurrentID = ulID;
		psHT->ulCount += 1;
		list_add(&psHN->sList, &psHT->psHashTable[ulHash]);
		return GED_OK;
	}

	return GED_ERROR_OOM;
}

void ged_hashtable_remove(GED_HASHTABLE_HANDLE hHashTable, unsigned long ulID)
{
	struct GED_HASHTABLE *psHT = __ged_hashtable_verify(hHashTable);

	if (psHT) {
		unsigned long ulHash = ged_hash(psHT, ulID);
		struct GED_HASHNODE *psHN =
			__ged_hashtable_find(&psHT->psHashTable[ulHash], ulID);

		if (psHN) {
			psHN->pvoid = NULL;
			list_del(&psHN->sList);
			ged_free(psHN, sizeof(struct GED_HASHNODE));
			psHT->ulCount -= 1;
		}
	}
}

void *ged_hashtable_find(GED_HASHTABLE_HANDLE hHashTable, unsigned long ulID)
{
	struct GED_HASHTABLE *psHT = __ged_hashtable_verify(hHashTable);

	if (psHT) {
		unsigned long ulHash = ged_hash(psHT, ulID);
		struct GED_HASHNODE *psHN =
			__ged_hashtable_find(&psHT->psHashTable[ulHash], ulID);

		if (psHN)
			return psHN->pvoid;
	}
	return NULL;
}

GED_ERROR ged_hashtable_set(GED_HASHTABLE_HANDLE hHashTable,
	unsigned long ulID, void *pvoid)
{
	struct GED_HASHTABLE *psHT = __ged_hashtable_verify(hHashTable);

	if (psHT) {
		unsigned long ulHash = ged_hash(psHT, ulID);
		struct GED_HASHNODE *psHN =
			__ged_hashtable_find(&psHT->psHashTable[ulHash], ulID);

		if (psHN) {
			psHN->pvoid = pvoid;
		} else {
			psHN =
			(struct GED_HASHNODE *)
			ged_alloc_atomic(sizeof(struct GED_HASHNODE));

			if (psHN == NULL)
				return GED_ERROR_OOM;
			psHN->pvoid = pvoid;
			psHN->ulID = ulID;
			INIT_LIST_HEAD(&psHN->sList);
			list_add(&psHN->sList, &psHT->psHashTable[ulHash]);
			psHT->ulCount += 1;
		}
		return GED_OK;
	}
	return GED_ERROR_INVALID_PARAMS;
}

void ged_hashtable_iterator(GED_HASHTABLE_HANDLE hHashTable,
	GED_BOOL (*iterator)(unsigned long ulID, void *pvoid, void *pvParam),
	void *pvParam)
{
	struct GED_HASHTABLE *psHT = __ged_hashtable_verify(hHashTable);

	if (psHT) {
		struct GED_HASHNODE *psHN;
		unsigned long i;
		struct list_head *psListEntry, *psListEntryTemp;

		for (i = 0; i < psHT->ulLength; ++i) {
			struct list_head *psList = &psHT->psHashTable[i];

			list_for_each_safe(psListEntry,
				psListEntryTemp, psList) {
				psHN = list_entry(psListEntry,
					struct GED_HASHNODE, sList);
				if (psHN) {
					if (!iterator(psHN->ulID,
						psHN->pvoid, pvParam))
						return;
				}
			}
		}
	}
}

void *ged_hashtable_search(GED_HASHTABLE_HANDLE hHashTable,
	void* (*pFunc)(unsigned long ulID, void *pvoid, void *pvParam),
	void *pvParam)
{
	struct GED_HASHTABLE *psHT = __ged_hashtable_verify(hHashTable);
	void *pResult = NULL;

	if (psHT) {
		struct GED_HASHNODE *psHN;
		unsigned long i;
		struct list_head *psListEntry, *psListEntryTemp;

		for (i = 0; i < psHT->ulLength; ++i) {
			struct list_head *psList = &psHT->psHashTable[i];

			list_for_each_safe(psListEntry, psListEntryTemp,
				psList) {
				psHN = list_entry(psListEntry,
					struct GED_HASHNODE,
					sList);

				if (psHN) {
					pResult = pFunc(psHN->ulID,
						psHN->pvoid, pvParam);
					if (pResult)
						return pResult;
				}
			}
		}
	}
	return pResult;
}

void ged_hashtable_iterator_delete(GED_HASHTABLE_HANDLE hHashTable,
	GED_BOOL (*pFunc)(unsigned long ulID, void *pvoid, void *pvParam,
	GED_BOOL *pbDeleted),
	void *pvParam)
{
	struct GED_HASHTABLE *psHT = __ged_hashtable_verify(hHashTable);

	if (psHT) {
		struct GED_HASHNODE *psHN;
		unsigned long i;
		struct list_head *psListEntry, *psListEntryTemp;

		for (i = 0; i < psHT->ulLength; ++i) {
			struct list_head *psList = &psHT->psHashTable[i];

			list_for_each_safe(psListEntry,
				psListEntryTemp, psList) {
				psHN = list_entry(psListEntry,
					struct GED_HASHNODE, sList);
				if (psHN) {
					GED_BOOL bDeleted = GED_FALSE;
					GED_BOOL bContinue = pFunc(psHN->ulID,
					psHN->pvoid, pvParam, &bDeleted);

					if (bDeleted) {
						psHN->pvoid = NULL;
						list_del(&psHN->sList);
						ged_free(psHN,
						sizeof(struct GED_HASHNODE));
						psHT->ulCount -= 1;
					}
					if (!bContinue)
						return;
				}
			}
		}
	}

}

unsigned long ged_hashtable_get_count(GED_HASHTABLE_HANDLE hHashTable)
{
	struct GED_HASHTABLE *psHT = __ged_hashtable_verify(hHashTable);

	if (psHT)
		return psHT->ulCount;
	return 0;
}

