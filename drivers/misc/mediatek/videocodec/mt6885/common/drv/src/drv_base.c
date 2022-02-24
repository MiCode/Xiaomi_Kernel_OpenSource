/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include "val_types_private.h"
/*#include "val_log.h"*/
#include "drv_api.h"

/* ============================================================== */
/* For Hybrid HW */
/* spinlock : OalHWContextLock */
struct VAL_VCODEC_OAL_HW_CONTEXT_T oal_hw_context[MULTI_INST_NUM];
/* mutex : NonCacheMemoryListLock */
struct VAL_NON_CACHE_MEMORY_LIST_T grNCMemoryList[MULTI_INST_NUM_x_10];

/* For both hybrid and pure HW */
struct VAL_VCODEC_HW_LOCK_T grVcodecDecHWLock;	/* mutex : VdecHWLock */
struct VAL_VCODEC_HW_LOCK_T grVcodecEncHWLock;	/* mutex : VencHWLock */

unsigned int gu4LockDecHWCount;	/* spinlock : LockDecHWCountLock */
unsigned int gu4LockEncHWCount;	/* spinlock : LockEncHWCountLock */
unsigned int gu4DecISRCount;	/* spinlock : DecISRCountLock */
unsigned int gu4EncISRCount;	/* spinlock : EncISRCountLock */


int search_HWLockSlot_ByTID(unsigned long ulpa, unsigned int curr_tid)
{
	int i;
	int j;

	for (i = 0; i < MULTI_INST_NUM; i++) {
		if (oal_hw_context[i].u4ThreadNum !=
			VCODEC_THREAD_MAX_NUM) {
			for (j = 0; j <
				oal_hw_context[i].u4ThreadNum; j++) {
				if (oal_hw_context[i].u4ThreadID[j] ==
					curr_tid) {
					pr_debug("[VCODEC][%s]\n",
						__func__);
					pr_debug("Lookup curr HW Locker is ObjId %d in index%d\n",
						 curr_tid, i);
					return i;
				}
			}
		}
	}

	return -1;
}

int search_HWLockSlot_ByHandle(unsigned long ulpa, unsigned long handle)
{
	int i;

	for (i = 0; i < MULTI_INST_NUM; i++) {
		if (oal_hw_context[i].pvHandle == handle)
			return i;
	}

	/* dump debug info */
	pr_debug("search_HWLockSlot_ByHandle");
	for (i = 0; i < MULTI_INST_NUM / 2; i++)
		pr_debug("[%d] 0x%lx", i, oal_hw_context[i].pvHandle);

	return -1;
}

struct VAL_VCODEC_OAL_HW_CONTEXT_T *setCurr_HWLockSlot(unsigned long ulpa,
	unsigned int tid)
{

	int i, j;

	/* Dump current ObjId */
	for (i = 0; i < MULTI_INST_NUM; i++) {
		pr_debug("[VCODEC] Dump curr slot %d ObjId 0x%lx\n",
			i, oal_hw_context[i].ObjId);
	}

	/* check if current ObjId exist in oal_hw_context[i].ObjId */
	for (i = 0; i < MULTI_INST_NUM; i++) {
		if (oal_hw_context[i].ObjId == ulpa) {
			pr_debug("[VCODEC] Curr Already exist in %d Slot\n", i);
			return &oal_hw_context[i];
		}
	}

	/* if not exist in table,  find a new free slot and put it */
	for (i = 0; i < MULTI_INST_NUM; i++) {
		if (oal_hw_context[i].u4ThreadNum !=
			VCODEC_THREAD_MAX_NUM) {
			for (j = 0; j <
				oal_hw_context[i].u4ThreadNum; j++) {
				if (oal_hw_context[i].u4ThreadID[j] ==
					current->pid) {
					oal_hw_context[i].ObjId = ulpa;
					pr_debug("[VCODEC][%s] setCurr %d Slot\n",
						__func__, i);
					return &oal_hw_context[i];
				}
			}
		}
	}

	pr_debug("[VCODEC][ERROR] %s All %d Slots unavaliable\n",
		__func__, MULTI_INST_NUM);
	oal_hw_context[0].u4ThreadNum = VCODEC_THREAD_MAX_NUM - 1;
	for (i = 0; i < oal_hw_context[0].u4ThreadNum; i++)
		oal_hw_context[0].u4ThreadID[i] = current->pid;

	return &oal_hw_context[0];
}


struct VAL_VCODEC_OAL_HW_CONTEXT_T *setCurr_HWLockSlot_Thread_ID(
	struct VAL_VCODEC_THREAD_ID_T a_prThreadID,
	unsigned int *a_prIndex)
{
	int i;
	int j;
	int k;

	/* Dump current tids */
	for (i = 0; i < MULTI_INST_NUM; i++) {
		if (oal_hw_context[i].u4ThreadNum !=
			VCODEC_THREAD_MAX_NUM) {
			for (j = 0; j < oal_hw_context[i].u4ThreadNum;
				j++) {
				pr_debug("[VCODEC][%s]\n", __func__);
				pr_debug("Dump curr slot %d, ThreadID[%d] = %d\n",
					i, j,
					oal_hw_context[i].u4ThreadID[j]);
			}
		}
	}

	for (i = 0; i < a_prThreadID.u4ThreadNum; i++) {
		pr_debug("[VCODEC][%s] VCodecThreadNum = %d, VCodecThreadID = %d\n",
		     __func__, a_prThreadID.u4ThreadNum,
		     a_prThreadID.u4ThreadID[i]);
	}

	/* check if current tids exist in oal_hw_context[i].ObjId */
	for (i = 0; i < MULTI_INST_NUM; i++) {
		if (oal_hw_context[i].u4ThreadNum !=
			VCODEC_THREAD_MAX_NUM) {
			for (j = 0; j < oal_hw_context[i].u4ThreadNum; j++) {
				for (k = 0; k < a_prThreadID.u4ThreadNum; k++) {
					if (oal_hw_context[i].u4ThreadID[j] ==
					    a_prThreadID.u4ThreadID[k]) {
						pr_debug("[VCODEC][%s]\n",
							__func__);
						pr_debug("Curr Already exist in %d Slot\n",
							i);
						*a_prIndex = i;
						return &oal_hw_context[i];
					}
				}
			}
		}
	}

	/* if not exist in table,  find a new free slot and put it */
	for (i = 0; i < MULTI_INST_NUM; i++) {
		if (oal_hw_context[i].u4ThreadNum ==
			VCODEC_THREAD_MAX_NUM) {
			oal_hw_context[i].u4ThreadNum =
				a_prThreadID.u4ThreadNum;
			for (j = 0; j < a_prThreadID.u4ThreadNum; j++) {
				oal_hw_context[i].u4ThreadID[j] =
				    a_prThreadID.u4ThreadID[j];
				pr_debug("[VCODEC][%s] setCurr %d Slot, %d\n",
					__func__, i,
					oal_hw_context[i].u4ThreadID[j]);
			}
			*a_prIndex = i;
			return &oal_hw_context[i];
		}
	}

	{
		pr_debug("[VCODEC][ERROR] %s All %d Slots unavaliable\n",
			__func__, MULTI_INST_NUM);
		oal_hw_context[0].u4ThreadNum =
			a_prThreadID.u4ThreadNum;
		for (i = 0; i < oal_hw_context[0].u4ThreadNum; i++) {
			oal_hw_context[0].u4ThreadID[i] =
			    a_prThreadID.u4ThreadID[i];
		}
		*a_prIndex = 0;
		return &oal_hw_context[0];
	}
}


struct VAL_VCODEC_OAL_HW_CONTEXT_T *freeCurr_HWLockSlot(unsigned long ulpa)
{
	int i;
	int j;

	/* check if current ObjId exist in oal_hw_context[i].ObjId */

	for (i = 0; i < MULTI_INST_NUM; i++) {
		if (oal_hw_context[i].ObjId == ulpa) {
			oal_hw_context[i].ObjId = -1L;
			for (j = 0; j < oal_hw_context[i].u4ThreadNum; j++)
				oal_hw_context[i].u4ThreadID[j] = -1;

			oal_hw_context[i].u4ThreadNum =
				VCODEC_THREAD_MAX_NUM;
			oal_hw_context[i].Oal_HW_reg =
				(struct VAL_VCODEC_OAL_HW_REGISTER_T  *)0;
			pr_debug("[VCODEC] %s %d Slot\n", __func__, i);
			return &oal_hw_context[i];
		}
	}

	pr_debug("[VCODEC][ERROR] %s can't find pid in HWLockSlot\n", __func__);
	return 0;
}


void Add_NonCacheMemoryList(unsigned long a_ulKVA,
			    unsigned long a_ulKPA,
			    unsigned long a_ulSize,
			    unsigned int a_u4ThreadNum,
			    unsigned int *a_pu4ThreadID)
{
	unsigned int u4I = 0;
	unsigned int u4J = 0;

	pr_debug("[VCODEC] %s +, KVA = 0x%lx, KPA = 0x%lx, Size = 0x%lx\n",
		__func__, a_ulKVA, a_ulKPA, a_ulSize);

	for (u4I = 0; u4I < MULTI_INST_NUM_x_10; u4I++) {
		if ((grNCMemoryList[u4I].ulKVA == -1L)
		    && (grNCMemoryList[u4I].ulKPA == -1L)) {
			pr_debug("[VCODEC] %s index = %d, VCodecThreadNum = %d, curr_tid = %d\n",
				__func__, u4I, a_u4ThreadNum,
				current->pid);

			grNCMemoryList[u4I].u4ThreadNum = a_u4ThreadNum;
			for (u4J = 0; u4J <
				grNCMemoryList[u4I].u4ThreadNum; u4J++) {
				grNCMemoryList[u4I].u4ThreadID[u4J] =
				    *(a_pu4ThreadID + u4J);
				pr_debug("[VCODEC][%s] VCodecThreadNum = %d, VCodecThreadID = %d\n",
				__func__,
				grNCMemoryList[u4I].u4ThreadNum,
				grNCMemoryList[u4I].u4ThreadID[u4J]);
			}

			grNCMemoryList[u4I].ulKVA = a_ulKVA;
			grNCMemoryList[u4I].ulKPA = a_ulKPA;
			grNCMemoryList[u4I].ulSize = a_ulSize;
			break;
		}
	}

	if (u4I == MULTI_INST_NUM_x_10) {
		pr_debug("[VCODEC][ERROR] CAN'T ADD %s, List is FULL!!\n",
			__func__);
	}

	pr_debug("[VCODEC] %s -\n", __func__);
}

void Free_NonCacheMemoryList(unsigned long a_ulKVA, unsigned long a_ulKPA)
{
	unsigned int u4I = 0;
	unsigned int u4J = 0;

	pr_debug("[VCODEC] %s +, KVA = 0x%lx, KPA = 0x%lx\n", __func__,
		 a_ulKVA, a_ulKPA);

	for (u4I = 0; u4I < MULTI_INST_NUM_x_10; u4I++) {
		if ((grNCMemoryList[u4I].ulKVA == a_ulKVA)
		    && (grNCMemoryList[u4I].ulKPA == a_ulKPA)) {
			pr_debug("[VCODEC] Free %s index = %d\n",
				__func__, u4I);
			grNCMemoryList[u4I].u4ThreadNum =
				VCODEC_THREAD_MAX_NUM;
			for (u4J = 0; u4J < VCODEC_THREAD_MAX_NUM; u4J++) {
				/* Add one line comment for
				 * avoid kernel coding style, WARNING:BRACES:
				 */
				grNCMemoryList[u4I].u4ThreadID[u4J]
					= 0xffffffff;
			}

			grNCMemoryList[u4I].ulKVA = -1L;
			grNCMemoryList[u4I].ulKPA = -1L;
			grNCMemoryList[u4I].ulSize = -1L;
			break;
		}
	}

	if (u4I == MULTI_INST_NUM_x_10)
		pr_debug("[VCODEC][ERROR] %s, addr not found\n",
			__func__);

	pr_debug("[VCODEC]%s -\n", __func__);
}


void Force_Free_NonCacheMemoryList(unsigned int a_u4Tid)
{
	unsigned int u4I = 0;
	unsigned int u4J = 0;
	unsigned int u4K = 0;

	pr_debug("[VCODEC] %s +, curr_id = %d", __func__, a_u4Tid);

	for (u4I = 0; u4I < MULTI_INST_NUM_x_10; u4I++) {
		if (grNCMemoryList[u4I].u4ThreadNum
			== VCODEC_THREAD_MAX_NUM)
			continue;
		for (u4J = 0; u4J <
			grNCMemoryList[u4I].u4ThreadNum; u4J++) {
			if (grNCMemoryList[u4I].u4ThreadID[u4J]
				!= a_u4Tid)
				continue;
			pr_debug
				("[VCODEC][WARNING] %s\n", __func__);
			pr_debug
				("idx=%d tid=%d KVA=0x%lx KPA=0x%lx Sz=%lu\n",
				 u4I, a_u4Tid, grNCMemoryList[u4I].ulKVA,
				 grNCMemoryList[u4I].ulKPA,
				 grNCMemoryList[u4I].ulSize);

			dma_free_coherent(0,
				grNCMemoryList[u4I].ulSize,
				(void *)grNCMemoryList[u4I].ulKVA,
				(dma_addr_t)grNCMemoryList[u4I].ulKPA);

			grNCMemoryList[u4I].u4ThreadNum =
				VCODEC_THREAD_MAX_NUM;
			for (u4K = 0; u4K <
				VCODEC_THREAD_MAX_NUM; u4K++) {
				grNCMemoryList[u4I].u4ThreadID[u4K] =
					0xffffffff;
			}
			grNCMemoryList[u4I].ulKVA = -1L;
			grNCMemoryList[u4I].ulKPA = -1L;
			grNCMemoryList[u4I].ulSize = -1L;
			break;
		}
	}

	pr_debug("[VCODEC] %s -, curr_id = %d", __func__, a_u4Tid);
}

unsigned long Search_NonCacheMemoryList_By_KPA(unsigned long a_ulKPA)
{
	unsigned int u4I = 0;
	unsigned long ulVA_Offset = 0;

	ulVA_Offset = a_ulKPA & 0x0000000000000fff;

	pr_debug("[VCODEC] %s +, KPA = 0x%lx, ulVA_Offset = 0x%lx\n",
		 __func__, a_ulKPA, ulVA_Offset);

	for (u4I = 0; u4I < MULTI_INST_NUM_x_10; u4I++) {
		if (grNCMemoryList[u4I].ulKPA ==
			(a_ulKPA - ulVA_Offset)) {
			pr_debug("[VCODEC] Find %s index = %d\n",
				 __func__, u4I);
			break;
		}
	}

	if (u4I == MULTI_INST_NUM_x_10) {
		pr_debug
		    ("[VCODEC][ERROR] %s, Address is not find!!\n", __func__);
		return grNCMemoryList[0].ulKVA + ulVA_Offset;
	}

	pr_debug("[VCODEC] %s, ulVA = 0x%lx -\n", __func__,
		 (grNCMemoryList[u4I].ulKVA + ulVA_Offset));

	return grNCMemoryList[u4I].ulKVA + ulVA_Offset;
}
