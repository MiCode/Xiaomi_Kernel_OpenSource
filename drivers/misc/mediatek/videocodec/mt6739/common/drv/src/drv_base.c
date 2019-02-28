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

#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include "val_types_private.h"
#include "val_log.h"
#include "drv_api.h"

/* #define VCODEC_DEBUG */
#ifdef VCODEC_DEBUG
#undef VCODEC_DEBUG
#define VCODEC_DEBUG pr_debug
#undef MODULE_MFV_PR_DEBUG
#define MODULE_MFV_PR_DEBUG pr_debug
#else
#define VCODEC_DEBUG(...)
#undef MODULE_MFV_PR_DEBUG
#define MODULE_MFV_PR_DEBUG(...)
#endif

/* ============================================================== */
/* For Hybrid HW */
/* spinlock : OalHWContextLock */
VAL_VCODEC_OAL_HW_CONTEXT_T oal_hw_context[VCODEC_MULTIPLE_INSTANCE_NUM];
/* mutex : NonCacheMemoryListLock */
struct VAL_NON_CACHE_MEMORY_LIST_T grNonCacheMemoryList[VCODEC_MULTIPLE_INSTANCE_NUM_x_10];

/* For both hybrid and pure HW */
struct VAL_VCODEC_HW_LOCK_T grVcodecHWLock;	/* mutex : HWLock */

VAL_UINT32_T gu4LockDecHWCount;	/* spinlock : LockDecHWCountLock */
VAL_UINT32_T gu4LockEncHWCount;	/* spinlock : LockEncHWCountLock */
VAL_UINT32_T gu4DecISRCount;	/* spinlock : DecISRCountLock */
VAL_UINT32_T gu4EncISRCount;	/* spinlock : EncISRCountLock */


VAL_INT32_T search_HWLockSlot_ByTID(VAL_ULONG_T ulpa, VAL_UINT32_T curr_tid)
{
	int i;
	int j;

	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_context[i].u4VCodecThreadNum != VCODEC_THREAD_MAX_NUM) {
			for (j = 0; j < oal_hw_context[i].u4VCodecThreadNum; j++) {
				if (oal_hw_context[i].u4VCodecThreadID[j] == curr_tid) {
					MODULE_MFV_PR_DEBUG("[VCODEC][search_HWLockSlot_ByTID]\n");
					MODULE_MFV_PR_DEBUG("Lookup curr HW Locker is ObjId %d in index%d\n",
						 curr_tid, i);
					return i;
				}
			}
		}
	}

	return -1;
}

VAL_INT32_T search_HWLockSlot_ByHandle(VAL_ULONG_T ulpa, VAL_HANDLE_T handle)
{
	VAL_INT32_T i;

	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_context[i].pvHandle == handle) {
			/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
			return i;
		}
	}

	/* dump debug info */
	MODULE_MFV_PR_DEBUG("search_HWLockSlot_ByHandle");
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM / 2; i++) {
		/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
		MODULE_MFV_PR_DEBUG("[%d] 0x%lx", i, oal_hw_context[i].pvHandle);
	}

	return -1;
}

VAL_VCODEC_OAL_HW_CONTEXT_T *setCurr_HWLockSlot(VAL_ULONG_T ulpa, VAL_UINT32_T tid)
{

	int i, j;

	/* Dump current ObjId */
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
		MODULE_MFV_PR_DEBUG("[VCODEC] Dump curr slot %d ObjId 0x%lx\n", i, oal_hw_context[i].ObjId);
	}

	/* check if current ObjId exist in oal_hw_context[i].ObjId */
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_context[i].ObjId == ulpa) {
			MODULE_MFV_PR_DEBUG("[VCODEC] Curr Already exist in %d Slot\n", i);
			return &oal_hw_context[i];
		}
	}

	/* if not exist in table,  find a new free slot and put it */
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_context[i].u4VCodecThreadNum != VCODEC_THREAD_MAX_NUM) {
			for (j = 0; j < oal_hw_context[i].u4VCodecThreadNum; j++) {
				if (oal_hw_context[i].u4VCodecThreadID[j] == current->pid) {
					oal_hw_context[i].ObjId = ulpa;
					MODULE_MFV_PR_DEBUG("[VCODEC][setCurr_HWLockSlot] setCurr %d Slot\n",
						 i);
					return &oal_hw_context[i];
				}
			}
		}
	}

	MODULE_MFV_PR_ERR("[VCODEC][ERROR] setCurr_HWLockSlot All %d Slots unavaliable\n",
		 VCODEC_MULTIPLE_INSTANCE_NUM);
	oal_hw_context[0].u4VCodecThreadNum = VCODEC_THREAD_MAX_NUM - 1;
	for (i = 0; i < oal_hw_context[0].u4VCodecThreadNum; i++) {
		/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
		oal_hw_context[0].u4VCodecThreadID[i] = current->pid;
	}
	return &oal_hw_context[0];
}


VAL_VCODEC_OAL_HW_CONTEXT_T *setCurr_HWLockSlot_Thread_ID(VAL_VCODEC_THREAD_ID_T a_prVcodecThreadID,
							  VAL_UINT32_T *a_prIndex)
{
	int i;
	int j;
	int k;

	/* Dump current tids */
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_context[i].u4VCodecThreadNum != VCODEC_THREAD_MAX_NUM) {
			for (j = 0; j < oal_hw_context[i].u4VCodecThreadNum; j++) {
				MODULE_MFV_PR_DEBUG("[VCODEC][setCurr_HWLockSlot_Thread_ID]\n");
				MODULE_MFV_PR_DEBUG("Dump curr slot %d, ThreadID[%d] = %d\n",
					 i, j, oal_hw_context[i].u4VCodecThreadID[j]);
			}
		}
	}

	for (i = 0; i < a_prVcodecThreadID.u4VCodecThreadNum; i++) {
		MODULE_MFV_PR_DEBUG
		    ("[VCODEC][setCurr_HWLockSlot_Thread_ID] VCodecThreadNum = %d, VCodecThreadID = %d\n",
		     a_prVcodecThreadID.u4VCodecThreadNum, a_prVcodecThreadID.u4VCodecThreadID[i]
			);
	}

	/* check if current tids exist in oal_hw_context[i].ObjId */
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_context[i].u4VCodecThreadNum != VCODEC_THREAD_MAX_NUM) {
			for (j = 0; j < oal_hw_context[i].u4VCodecThreadNum; j++) {
				for (k = 0; k < a_prVcodecThreadID.u4VCodecThreadNum; k++) {
					if (oal_hw_context[i].u4VCodecThreadID[j] ==
					    a_prVcodecThreadID.u4VCodecThreadID[k]) {
						MODULE_MFV_PR_DEBUG
						    ("[VCODEC][setCurr_HWLockSlot_Thread_ID]\n");
						MODULE_MFV_PR_DEBUG("Curr Already exist in %d Slot\n", i);
						*a_prIndex = i;
						return &oal_hw_context[i];
					}
				}
			}
		}
	}

	/* if not exist in table,  find a new free slot and put it */
	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_context[i].u4VCodecThreadNum == VCODEC_THREAD_MAX_NUM) {
			oal_hw_context[i].u4VCodecThreadNum = a_prVcodecThreadID.u4VCodecThreadNum;
			for (j = 0; j < a_prVcodecThreadID.u4VCodecThreadNum; j++) {
				oal_hw_context[i].u4VCodecThreadID[j] =
				    a_prVcodecThreadID.u4VCodecThreadID[j];
				MODULE_MFV_PR_DEBUG
				    ("[VCODEC][setCurr_HWLockSlot_Thread_ID] setCurr %d Slot, %d\n",
				     i, oal_hw_context[i].u4VCodecThreadID[j]);
			}
			*a_prIndex = i;
			return &oal_hw_context[i];
		}
	}

	{
		MODULE_MFV_PR_ERR("[VCODEC][ERROR] setCurr_HWLockSlot_Thread_ID All %d Slots unavaliable\n",
			 VCODEC_MULTIPLE_INSTANCE_NUM);
		oal_hw_context[0].u4VCodecThreadNum = a_prVcodecThreadID.u4VCodecThreadNum;
		for (i = 0; i < oal_hw_context[0].u4VCodecThreadNum; i++) {
			/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
			oal_hw_context[0].u4VCodecThreadID[i] =
			    a_prVcodecThreadID.u4VCodecThreadID[i];
		}
		*a_prIndex = 0;
		return &oal_hw_context[0];
	}
}


VAL_VCODEC_OAL_HW_CONTEXT_T *freeCurr_HWLockSlot(VAL_ULONG_T ulpa)
{
	int i;
	int j;

	/* check if current ObjId exist in oal_hw_context[i].ObjId */

	for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++) {
		if (oal_hw_context[i].ObjId == ulpa) {
			oal_hw_context[i].ObjId = -1L;
			for (j = 0; j < oal_hw_context[i].u4VCodecThreadNum; j++) {
				/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
				oal_hw_context[i].u4VCodecThreadID[j] = -1;
			}
			oal_hw_context[i].u4VCodecThreadNum = VCODEC_THREAD_MAX_NUM;
			oal_hw_context[i].Oal_HW_reg = (VAL_VCODEC_OAL_HW_REGISTER_T  *)0;
			MODULE_MFV_PR_DEBUG("[VCODEC] freeCurr_HWLockSlot %d Slot\n", i);
			return &oal_hw_context[i];
		}
	}

	MODULE_MFV_PR_ERR("[VCODEC][ERROR] freeCurr_HWLockSlot can't find pid in HWLockSlot\n");
	return 0;
}


void Add_NonCacheMemoryList(VAL_ULONG_T a_ulKVA,
			    VAL_ULONG_T a_ulKPA,
			    VAL_ULONG_T a_ulSize,
			    VAL_UINT32_T a_u4VCodecThreadNum, VAL_UINT32_T *a_pu4VCodecThreadID)
{
	VAL_UINT32_T u4I = 0;
	VAL_UINT32_T u4J = 0;

	MODULE_MFV_PR_DEBUG("[VCODEC] Add_NonCacheMemoryList +, KVA = 0x%lx, KPA = 0x%lx, Size = 0x%lx\n",
		 a_ulKVA, a_ulKPA, a_ulSize);

	for (u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++) {
		if ((grNonCacheMemoryList[u4I].ulKVA == -1L)
		    && (grNonCacheMemoryList[u4I].ulKPA == -1L)) {
			MODULE_MFV_PR_DEBUG
			    ("[VCODEC] Add_NonCacheMemoryList index = %d, VCodecThreadNum = %d, curr_tid = %d\n",
				u4I, a_u4VCodecThreadNum, current->pid);

			grNonCacheMemoryList[u4I].u4VCodecThreadNum = a_u4VCodecThreadNum;
			for (u4J = 0; u4J < grNonCacheMemoryList[u4I].u4VCodecThreadNum; u4J++) {
				grNonCacheMemoryList[u4I].u4VCodecThreadID[u4J] =
				    *(a_pu4VCodecThreadID + u4J);
				MODULE_MFV_PR_DEBUG
				    ("[VCODEC][Add_NonCacheMemoryList] VCodecThreadNum = %d, VCodecThreadID = %d\n",
				     grNonCacheMemoryList[u4I].u4VCodecThreadNum,
				     grNonCacheMemoryList[u4I].u4VCodecThreadID[u4J]);
			}

			grNonCacheMemoryList[u4I].ulKVA = a_ulKVA;
			grNonCacheMemoryList[u4I].ulKPA = a_ulKPA;
			grNonCacheMemoryList[u4I].ulSize = a_ulSize;
			break;
		}
	}

	if (u4I == VCODEC_MULTIPLE_INSTANCE_NUM_x_10) {
		/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
		MODULE_MFV_PR_ERR("[VCODEC][ERROR] CAN'T ADD Add_NonCacheMemoryList, List is FULL!!\n");
	}

	MODULE_MFV_PR_DEBUG("[VCODEC] Add_NonCacheMemoryList -\n");
}

void Free_NonCacheMemoryList(VAL_ULONG_T a_ulKVA, VAL_ULONG_T a_ulKPA)
{
	VAL_UINT32_T u4I = 0;
	VAL_UINT32_T u4J = 0;

	MODULE_MFV_PR_DEBUG("[VCODEC] Free_NonCacheMemoryList +, KVA = 0x%lx, KPA = 0x%lx\n", a_ulKVA,
		 a_ulKPA);

	for (u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++) {
		if ((grNonCacheMemoryList[u4I].ulKVA == a_ulKVA)
		    && (grNonCacheMemoryList[u4I].ulKPA == a_ulKPA)) {
			MODULE_MFV_PR_DEBUG("[VCODEC] Free Free_NonCacheMemoryList index = %d\n", u4I);
			grNonCacheMemoryList[u4I].u4VCodecThreadNum = VCODEC_THREAD_MAX_NUM;
			for (u4J = 0; u4J < VCODEC_THREAD_MAX_NUM; u4J++) {
				/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
				grNonCacheMemoryList[u4I].u4VCodecThreadID[u4J] = 0xffffffff;
			}

			grNonCacheMemoryList[u4I].ulKVA = -1L;
			grNonCacheMemoryList[u4I].ulKPA = -1L;
			grNonCacheMemoryList[u4I].ulSize = -1L;
			break;
		}
	}

	if (u4I == VCODEC_MULTIPLE_INSTANCE_NUM_x_10) {
		/* Add one line comment for avoid kernel coding style, WARNING:BRACES: */
		MODULE_MFV_PR_ERR
		    ("[VCODEC][ERROR] CAN'T Free Free_NonCacheMemoryList, Address is not find!!\n");
	}

	MODULE_MFV_PR_DEBUG("[VCODEC]Free_NonCacheMemoryList -\n");
}


void Force_Free_NonCacheMemoryList(VAL_UINT32_T a_u4Tid)
{
	VAL_UINT32_T u4I = 0;
	VAL_UINT32_T u4J = 0;
	VAL_UINT32_T u4K = 0;

	MODULE_MFV_PR_DEBUG("[VCODEC] Force_Free_NonCacheMemoryList +, curr_id = %d", a_u4Tid);

	for (u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++) {
		if (grNonCacheMemoryList[u4I].u4VCodecThreadNum != VCODEC_THREAD_MAX_NUM) {
			for (u4J = 0; u4J < grNonCacheMemoryList[u4I].u4VCodecThreadNum; u4J++) {
				if (grNonCacheMemoryList[u4I].u4VCodecThreadID[u4J] == a_u4Tid) {
					MODULE_MFV_PR_DEBUG
					    ("[VCODEC][WARNING] Force_Free_NonCacheMemoryList\n");
					MODULE_MFV_PR_DEBUG
					    ("index = %d, tid = %d, KVA = 0x%lx, KPA = 0x%lx, Size = %lu\n",
					     u4I, a_u4Tid, grNonCacheMemoryList[u4I].ulKVA,
					     grNonCacheMemoryList[u4I].ulKPA,
					     grNonCacheMemoryList[u4I].ulSize);

					dma_free_coherent(0,
							  grNonCacheMemoryList[u4I].ulSize,
							  (void *)grNonCacheMemoryList[u4I].ulKVA,
							  (dma_addr_t) grNonCacheMemoryList[u4I].
							  ulKPA);

					grNonCacheMemoryList[u4I].u4VCodecThreadNum =
					    VCODEC_THREAD_MAX_NUM;
					for (u4K = 0; u4K < VCODEC_THREAD_MAX_NUM; u4K++) {
						/* Add one line comment for avoid kernel coding style,
						 * WARNING:BRACES:
						 */
						grNonCacheMemoryList[u4I].u4VCodecThreadID[u4K] =
						    0xffffffff;
					}
					grNonCacheMemoryList[u4I].ulKVA = -1L;
					grNonCacheMemoryList[u4I].ulKPA = -1L;
					grNonCacheMemoryList[u4I].ulSize = -1L;
					break;
				}
			}
		}
	}

	MODULE_MFV_PR_DEBUG("[VCODEC] Force_Free_NonCacheMemoryList -, curr_id = %d", a_u4Tid);
}

VAL_ULONG_T Search_NonCacheMemoryList_By_KPA(VAL_ULONG_T a_ulKPA)
{
	VAL_UINT32_T u4I = 0;
	VAL_ULONG_T ulVA_Offset = 0;

	ulVA_Offset = a_ulKPA & 0x0000000000000fff;

	MODULE_MFV_PR_DEBUG("[VCODEC] Search_NonCacheMemoryList_By_KPA +, KPA = 0x%lx, ulVA_Offset = 0x%lx\n",
		 a_ulKPA, ulVA_Offset);

	for (u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++) {
		if (grNonCacheMemoryList[u4I].ulKPA == (a_ulKPA - ulVA_Offset)) {
			MODULE_MFV_PR_DEBUG("[VCODEC] Find Search_NonCacheMemoryList_By_KPA index = %d\n",
				 u4I);
			break;
		}
	}

	if (u4I == VCODEC_MULTIPLE_INSTANCE_NUM_x_10) {
		MODULE_MFV_PR_ERR
		    ("[VCODEC][ERROR] CAN'T Find Search_NonCacheMemoryList_By_KPA, Address is not find!!\n");
		return grNonCacheMemoryList[0].ulKVA + ulVA_Offset;
	}

	MODULE_MFV_PR_DEBUG("[VCODEC] Search_NonCacheMemoryList_By_KPA, ulVA = 0x%lx -\n",
		 (grNonCacheMemoryList[u4I].ulKVA + ulVA_Offset));

	return grNonCacheMemoryList[u4I].ulKVA + ulVA_Offset;
}
