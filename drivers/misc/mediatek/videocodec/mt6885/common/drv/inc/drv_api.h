/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MT6885_DRVBASE_H__
#define __MT6885_DRVBASE_H__

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
	void *pvHandle;	/*  */
	unsigned int u4ThreadNum;	/* Hybrid vcodec thread num */
	unsigned int u4ThreadID[VCODEC_THREAD_MAX_NUM];
		/* hybrid vcodec thread ids */
	unsigned long  ulSize;
};

/* ============================================================== */
/* For Hybrid HW */
#define MULTI_INST_NUM 16
#define MULTI_INST_NUM_x_10 (MULTI_INST_NUM * 10)

/* spinlock : OalHWContextLock */
extern struct VAL_VCODEC_OAL_HW_CONTEXT_T
	oal_hw_context[MULTI_INST_NUM];
/* mutex : NonCacheMemoryListLock */
extern struct VAL_NON_CACHE_MEMORY_LIST_T
	grNCMemoryList[MULTI_INST_NUM_x_10];

/* For both hybrid and pure HW */
extern struct VAL_VCODEC_HW_LOCK_T grVcodecDecHWLock;	/* mutex : VdecHWLock */
extern struct VAL_VCODEC_HW_LOCK_T grVcodecEncHWLock;	/* mutex : VencHWLock */

extern unsigned int gu4LockDecHWCount;	/* spinlock : LockDecHWCountLock */
extern unsigned int gu4LockEncHWCount;	/* spinlock : LockEncHWCountLock */
extern unsigned int gu4DecISRCount;	/* spinlock : DecISRCountLock */
extern unsigned int gu4EncISRCount;	/* spinlock : EncISRCountLock */

int search_HWLockSlot_ByTID(unsigned long ulpa, unsigned int curr_tid);
int search_HWLockSlot_ByHandle(unsigned long ulpa, unsigned long handle);
struct VAL_VCODEC_OAL_HW_CONTEXT_T *setCurr_HWLockSlot(
		unsigned long ulpa, unsigned int tid);
struct VAL_VCODEC_OAL_HW_CONTEXT_T *setCurr_HWLockSlot_Thread_ID(
		struct VAL_VCODEC_THREAD_ID_T a_prThreadID,
		unsigned int *a_prIndex);
struct VAL_VCODEC_OAL_HW_CONTEXT_T *freeCurr_HWLockSlot(unsigned long ulpa);
void Add_NonCacheMemoryList(unsigned long a_ulKVA,
		unsigned long a_ulKPA,
		unsigned long a_ulSize,
		unsigned int a_u4ThreadNum,
		unsigned int *a_pu4ThreadID);
void Free_NonCacheMemoryList(unsigned long a_ulKVA, unsigned long a_ulKPA);
void Force_Free_NonCacheMemoryList(unsigned int a_u4Tid);
unsigned long Search_NonCacheMemoryList_By_KPA(unsigned long a_u4KPA);

#endif
