#ifndef __MT6589_DRVBASE_H__
#define __MT6589_DRVBASE_H__

#include "val_types_private.h"

//=============================================================
// For driver

typedef struct
{
    VAL_VOID_T *pvHandle;                                   // HW vcodec handle
    VAL_TIME_T rLockedTime;
    VAL_DRIVER_TYPE_T eDriverType;
} VAL_VCODEC_HW_LOCK_T;

typedef struct
{
    VAL_ULONG_T ulKVA;                                     // Kernel virtual address
    VAL_ULONG_T ulKPA;                                     // Kernel physical address
    VAL_HANDLE_T pvHandle;                                  //
    VAL_UINT32_T u4VCodecThreadNum;                         // Hybrid vcodec thread num
    VAL_UINT32_T u4VCodecThreadID[VCODEC_THREAD_MAX_NUM];   // hybrid vcodec thread ids
    VAL_ULONG_T  ulSize;
} VAL_NON_CACHE_MEMORY_LIST_T;

//==============================================================
// For Hybrid HW
#define VCODEC_MULTIPLE_INSTANCE_NUM 16
#define VCODEC_MULTIPLE_INSTANCE_NUM_x_10 (VCODEC_MULTIPLE_INSTANCE_NUM * 10)

extern VAL_VCODEC_OAL_HW_CONTEXT_T  oal_hw_context[VCODEC_MULTIPLE_INSTANCE_NUM];               //spinlock : OalHWContextLock
extern VAL_NON_CACHE_MEMORY_LIST_T  grNonCacheMemoryList[VCODEC_MULTIPLE_INSTANCE_NUM_x_10];    //mutex : NonCacheMemoryListLock

// For both hybrid and pure HW
extern VAL_VCODEC_HW_LOCK_T grVcodecDecHWLock; //mutex : VdecHWLock
extern VAL_VCODEC_HW_LOCK_T grVcodecEncHWLock; //mutex : VencHWLock

extern VAL_UINT32_T gu4LockDecHWCount; //spinlock : LockDecHWCountLock
extern VAL_UINT32_T gu4LockEncHWCount; //spinlock : LockEncHWCountLock
extern VAL_UINT32_T gu4DecISRCount;    //spinlock : DecISRCountLock
extern VAL_UINT32_T gu4EncISRCount;    //spinlock : EncISRCountLock

VAL_INT32_T search_HWLockSlot_ByTID(VAL_ULONG_T ulpa, VAL_UINT32_T curr_tid);
VAL_INT32_T search_HWLockSlot_ByHandle(VAL_ULONG_T ulpa, VAL_HANDLE_T handle);
VAL_VCODEC_OAL_HW_CONTEXT_T *setCurr_HWLockSlot(VAL_ULONG_T ulpa, VAL_UINT32_T tid);
VAL_VCODEC_OAL_HW_CONTEXT_T *setCurr_HWLockSlot_Thread_ID(VAL_VCODEC_THREAD_ID_T a_prVcodecThreadID, VAL_UINT32_T *a_prIndex);
VAL_VCODEC_OAL_HW_CONTEXT_T *freeCurr_HWLockSlot(VAL_ULONG_T ulpa);
void Add_NonCacheMemoryList(VAL_ULONG_T a_ulKVA, VAL_ULONG_T a_ulKPA, VAL_ULONG_T a_ulSize, VAL_UINT32_T a_u4VCodecThreadNum, VAL_UINT32_T* a_pu4VCodecThreadID);
void Free_NonCacheMemoryList(VAL_ULONG_T a_ulKVA, VAL_ULONG_T a_ulKPA);
void Force_Free_NonCacheMemoryList(VAL_UINT32_T a_u4Tid);
VAL_UINT32_T Search_NonCacheMemoryList_By_KPA(VAL_ULONG_T a_u4KPA);

#endif
