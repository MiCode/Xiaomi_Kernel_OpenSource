/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: PC Chen <pc.chen@mediatek.com>
 * Tiffany Lin <tiffany.lin@mediatek.com>
 */

#ifndef __MT6758_DRVBASE_H__
#define __MT6758_DRVBASE_H__

#include "val_types_private.h"

/* ============================================================= */
/* For driver */

struct VAL_VCODEC_HW_LOCK_T {
	void *pvHandle;	/* HW vcodec handle */
	struct VAL_TIME_T rLockedTime;
	enum VAL_DRIVER_TYPE_T eDriverType;
};

struct VAL_NON_CACHE_MEMORY_LIST_T {
	unsigned long ulKVA;	/* Kernel virtual address */
	unsigned long ulKPA;	/* Kernel physical address */
	unsigned long pvHandle;	/*  */
	unsigned int u4VCodecThreadNum;	/* Hybrid vcodec thread num */
	/* hybrid vcodec thread ids */
	unsigned int u4VCodecThreadID[VCODEC_THREAD_MAX_NUM];
	unsigned long  ulSize;
};

/* ============================================================== */
/* For Hybrid HW */
#define VCODEC_INST_NUM 16
#define VCODEC_INST_NUM_x_10 (VCODEC_INST_NUM * 10)

/* spinlock : OalHWContextLock */
extern struct VAL_VCODEC_OAL_HW_CONTEXT_T oal_hw_context[VCODEC_INST_NUM];
/* mutex : NonCacheMemoryListLock */
extern struct VAL_NON_CACHE_MEMORY_LIST_T ncache_mem_list[VCODEC_INST_NUM_x_10];

/* For both hybrid and pure HW */
extern struct VAL_VCODEC_HW_LOCK_T CodecHWLock;	/* mutex : VdecHWLock */

extern unsigned int gu4LockDecHWCount;	/* spinlock : LockDecHWCountLock */
extern unsigned int gu4LockEncHWCount;	/* spinlock : LockEncHWCountLock */
extern unsigned int gu4DecISRCount;	/* spinlock : DecISRCountLock */
extern unsigned int gu4EncISRCount;	/* spinlock : EncISRCountLock */

int search_slot_byTID(unsigned long ulpa, unsigned int curr_tid);
int search_slot_byHdl(unsigned long ulpa, unsigned long handle);
struct VAL_VCODEC_OAL_HW_CONTEXT_T *set_slot(unsigned long ulpa,
						unsigned int tid);
struct VAL_VCODEC_OAL_HW_CONTEXT_T
	*set_slot_TID(struct VAL_VCODEC_THREAD_ID_T a_prVcodecThreadID,
			unsigned int *a_prIndex);
struct VAL_VCODEC_OAL_HW_CONTEXT_T *free_slot(unsigned long ulpa);
void add_ncmem(unsigned long a_ulKVA,
			unsigned long a_ulKPA,
			unsigned long a_ulSize,
			unsigned int a_u4VCodecThreadNum,
			unsigned int *a_pu4VCodecThreadID);
void free_ncmem(unsigned long a_ulKVA, unsigned long a_ulKPA);
void ffree_ncmem(unsigned int a_u4Tid);
unsigned long search_ncmem_byKPA(unsigned long a_u4KPA);

#endif
