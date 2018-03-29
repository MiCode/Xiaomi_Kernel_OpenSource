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

#include "ged_base.h"
#include "ged_hashtable.h"
#include <linux/hashtable.h>

typedef struct GED_HASHTABLE_TAG
{
	unsigned int        ui32Bits;
	unsigned int        ui32Length;
	unsigned int        ui32CurrentID;
	unsigned int        ui32Count;
	struct hlist_head*  psHashTable;
} GED_HASHTABLE;

typedef struct GED_HASHNODE_TAG
{
	unsigned int        ui32ID;
	void*               pvoid;
	struct hlist_node   sNode;
} GED_HASHNODE;

#define GED_HASHTABLE_INIT_ID 1234 // 0 = invalid

void* __ged_hashtable_find(struct hlist_head *head, unsigned int ui32ID)
{
	GED_HASHNODE* psHN;
	hlist_for_each_entry_rcu(psHN, head, sNode) 
	{
		if (psHN->ui32ID == ui32ID)
		{
			return psHN;
		}
	}
	return NULL;
}

static int ged_hash(GED_HASHTABLE_HANDLE hHashTable, unsigned int ui32ID)
{
	GED_HASHTABLE* psHT = (GED_HASHTABLE*)hHashTable;
	return hash_32(ui32ID, psHT->ui32Bits);
}

GED_HASHTABLE_HANDLE ged_hashtable_create(unsigned int ui32Bits)
{
	GED_HASHTABLE* psHT;
	unsigned int i;

	if (ui32Bits > 20)
	{
		// 1048576 slots !?
		// Need to check the necessary
		return NULL;
	}

	psHT = (GED_HASHTABLE*)ged_alloc(sizeof(GED_HASHTABLE));
	if (psHT)
	{
		psHT->ui32Bits = ui32Bits;
		psHT->ui32Length = 1 << ui32Bits;
		psHT->ui32CurrentID = GED_HASHTABLE_INIT_ID; // 0 = invalid
		psHT->psHashTable = (struct hlist_head*)ged_alloc(psHT->ui32Length * sizeof(struct hlist_head));
		if (psHT->psHashTable)
		{
			for (i = 0; i < psHT->ui32Length; i++)
			{
				INIT_HLIST_HEAD(&psHT->psHashTable[i]);
			}
			return (GED_HASHTABLE_HANDLE)psHT;
		}
	}

	ged_hashtable_destroy(psHT);
	return NULL;
}

void ged_hashtable_destroy(GED_HASHTABLE_HANDLE hHashTable)
{
	GED_HASHTABLE* psHT = (GED_HASHTABLE*)hHashTable;
	if (psHT)
	{
		int i = 0;
		while(psHT->ui32Count > 0)
		{
			unsigned int ui32ID = 0;
			GED_HASHNODE* psHN;
			// get one to be freed
			for (;i < psHT->ui32Length; i++)
			{
				struct hlist_head *head = &psHT->psHashTable[i];
				hlist_for_each_entry_rcu(psHN, head, sNode) 
				{
					ui32ID = psHN->ui32ID;
					break;
				}
				if (0 < ui32ID)
				{
					break;
				}
			}

			if (i >= psHT->ui32Length)
			{
				break;
			}

			ged_hashtable_remove(psHT, ui32ID);
		}

		/* free the hash table */
		ged_free(psHT->psHashTable, psHT->ui32Length * sizeof(struct hlist_head));
		ged_free(psHT, sizeof(GED_HASHTABLE));
	}
}

GED_ERROR ged_hashtable_insert(GED_HASHTABLE_HANDLE hHashTable, void* pvoid, unsigned int* pui32ID)
{
	GED_HASHTABLE* psHT = (GED_HASHTABLE*)hHashTable;
	GED_HASHNODE* psHN = NULL;
	unsigned int ui32Hash, ui32ID;

	if ((!psHT) || (!pui32ID))
	{
		return GED_ERROR_INVALID_PARAMS;
	}

	ui32ID = psHT->ui32CurrentID + 1;
	while(1)
	{
		ui32Hash = ged_hash(psHT, ui32ID);
		psHN = __ged_hashtable_find(&psHT->psHashTable[ui32Hash], ui32ID);
		if (psHN != NULL)
		{
			ui32ID++;
			if (ui32ID == 0)//skip the value 0
			{
				ui32ID = 1;
			}
			if (ui32ID == psHT->ui32CurrentID)
			{
				return GED_ERROR_FAIL;
			}
		}
		else
		{
			break;
		}
	};

	psHN = (GED_HASHNODE*)ged_alloc(sizeof(GED_HASHNODE));
	if (psHN)
	{
		psHN->pvoid = pvoid;
		psHN->ui32ID = ui32ID;
		psHT->ui32CurrentID = ui32ID;
		*pui32ID = ui32ID;
		hlist_add_head_rcu(&psHN->sNode, &psHT->psHashTable[ui32Hash]);
		psHT->ui32Count += 1;
		return GED_OK;
	}

	return GED_ERROR_OOM;
}

void ged_hashtable_remove(GED_HASHTABLE_HANDLE hHashTable, unsigned int ui32ID)
{
	GED_HASHTABLE* psHT = (GED_HASHTABLE*)hHashTable;
	if (psHT)
	{
		unsigned int ui32Hash = ged_hash(psHT, ui32ID);
		GED_HASHNODE* psHN = __ged_hashtable_find(&psHT->psHashTable[ui32Hash], ui32ID);
		if (psHN)
		{
			hlist_del_rcu(&psHN->sNode);
			synchronize_rcu();
			ged_free(psHN, sizeof(GED_HASHNODE));
			psHT->ui32Count -= 1;
		}
	}
}

void* ged_hashtable_find(GED_HASHTABLE_HANDLE hHashTable, unsigned int ui32ID)
{
	GED_HASHTABLE* psHT = (GED_HASHTABLE*)hHashTable;
	if (psHT)
	{
		unsigned int ui32Hash = ged_hash(psHT, ui32ID);
		GED_HASHNODE* psHN = __ged_hashtable_find(&psHT->psHashTable[ui32Hash], ui32ID);
		if (psHN)
		{
			return psHN->pvoid;
		}
#ifdef GED_DEBUG
		if (ui32ID != 0)
		{
			GED_LOGE("ged_hashtable_find: ui32ID=%u ui32Hash=%u psHN=%p\n", ui32ID, ui32Hash, psHN);
		}
#endif
	}
	return NULL;
}

GED_ERROR ged_hashtable_set(GED_HASHTABLE_HANDLE hHashTable, unsigned int ui32ID, void* pvoid)
{
	GED_HASHTABLE* psHT = (GED_HASHTABLE*)hHashTable;
	if (psHT)
	{
		unsigned int ui32Hash = ged_hash(psHT, ui32ID);
		GED_HASHNODE* psHN = __ged_hashtable_find(&psHT->psHashTable[ui32Hash], ui32ID);
		if (psHN)
		{
			psHN->pvoid = pvoid;
			return GED_OK;
		}
	}

	return GED_ERROR_INVALID_PARAMS;
}

