#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include "val_types_private.h"
#include "val_log.h"
#include "drv_api.h"

//==============================================================
// For Hybrid HW
VAL_VCODEC_OAL_HW_CONTEXT_T  oal_hw_context[VCODEC_MULTIPLE_INSTANCE_NUM];               //spinlock : OalHWContextLock
VAL_NON_CACHE_MEMORY_LIST_T  grNonCacheMemoryList[VCODEC_MULTIPLE_INSTANCE_NUM_x_10];    //mutex : NonCacheMemoryListLock

// For both hybrid and pure HW
VAL_VCODEC_HW_LOCK_T grVcodecDecHWLock; //mutex : VdecHWLock
VAL_VCODEC_HW_LOCK_T grVcodecEncHWLock; //mutex : VencHWLock

VAL_UINT32_T gu4LockDecHWCount; //spinlock : LockDecHWCountLock
VAL_UINT32_T gu4LockEncHWCount; //spinlock : LockEncHWCountLock
VAL_UINT32_T gu4DecISRCount;    //spinlock : DecISRCountLock
VAL_UINT32_T gu4EncISRCount;    //spinlock : EncISRCountLock


VAL_INT32_T search_HWLockSlot_ByTID(VAL_ULONG_T ulpa, VAL_UINT32_T curr_tid)
{
    int i;
    int j;

    for(i=0; i<VCODEC_MULTIPLE_INSTANCE_NUM; i++)
    {
        if (oal_hw_context[i].u4VCodecThreadNum != VCODEC_THREAD_MAX_NUM)
        {
            for(j = 0; j < oal_hw_context[i].u4VCodecThreadNum; j++)
            {
                if (oal_hw_context[i].u4VCodecThreadID[j] == curr_tid)
                {
                    MFV_LOGD("[search_HWLockSlot_ByTID] Lookup curr HW Locker is ObjId %d in index%d\n", curr_tid, i);
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
        if (oal_hw_context[i].pvHandle == handle)
        {
            return i;
        }
    }

    return -1;
}

VAL_VCODEC_OAL_HW_CONTEXT_T *setCurr_HWLockSlot(VAL_ULONG_T ulpa, VAL_UINT32_T tid){

    int i, j;

    // Dump current ObjId
    for(i=0; i<VCODEC_MULTIPLE_INSTANCE_NUM; i++)
    {
        MFV_LOGD("Dump curr slot %d ObjId %lu \n", i, oal_hw_context[i].ObjId);
    }

    // check if current ObjId exist in oal_hw_context[i].ObjId
    for(i=0; i<VCODEC_MULTIPLE_INSTANCE_NUM; i++)
    {
        if( oal_hw_context[i].ObjId == ulpa)
        {
            MFV_LOGD("Curr Already exist in %d Slot\n", i);
            return &oal_hw_context[i];
        }
    }

    // if not exist in table,  find a new free slot and put it
    for(i=0; i<VCODEC_MULTIPLE_INSTANCE_NUM; i++)
    {
        if (oal_hw_context[i].u4VCodecThreadNum != VCODEC_THREAD_MAX_NUM)
        {
            for (j = 0; j < oal_hw_context[i].u4VCodecThreadNum; j++)
            {
                if (oal_hw_context[i].u4VCodecThreadID[j] == current->pid)
                {
                    oal_hw_context[i].ObjId = ulpa;
                    MFV_LOGD("[setCurr_HWLockSlot] setCurr %d Slot\n", i);
                    return &oal_hw_context[i];
                }
            }
        }
    }

    MFV_LOGE("[VCodec Error][ERROR] setCurr_HWLockSlot All %d Slots unavaliable\n", VCODEC_MULTIPLE_INSTANCE_NUM);
    oal_hw_context[0].u4VCodecThreadNum = VCODEC_THREAD_MAX_NUM - 1;
    for(i = 0; i < oal_hw_context[0].u4VCodecThreadNum; i++)
    {
        oal_hw_context[0].u4VCodecThreadID[i] = current->pid;
    }
	return &oal_hw_context[0];
}


VAL_VCODEC_OAL_HW_CONTEXT_T *setCurr_HWLockSlot_Thread_ID(VAL_VCODEC_THREAD_ID_T a_prVcodecThreadID, VAL_UINT32_T *a_prIndex)
{
    int i;
    int j;
    int k;

    // Dump current tids
    for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++)
    {
        if (oal_hw_context[i].u4VCodecThreadNum != VCODEC_THREAD_MAX_NUM)
        {
            for(j = 0;j < oal_hw_context[i].u4VCodecThreadNum; j++)
            {
                MFV_LOGD("[setCurr_HWLockSlot_Thread_ID] Dump curr slot %d, ThreadID[%d] = %d\n", i, j, oal_hw_context[i].u4VCodecThreadID[j]);
            }
        }
    }

    for (i = 0; i < a_prVcodecThreadID.u4VCodecThreadNum; i++)
    {
        MFV_LOGD("[setCurr_HWLockSlot_Thread_ID] VCodecThreadNum = %d, VCodecThreadID = %d\n",
            a_prVcodecThreadID.u4VCodecThreadNum,
            a_prVcodecThreadID.u4VCodecThreadID[i]
            );
    }

    // check if current tids exist in oal_hw_context[i].ObjId
    for (i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++)
    {
        if (oal_hw_context[i].u4VCodecThreadNum != VCODEC_THREAD_MAX_NUM)
        {
            for (j = 0; j < oal_hw_context[i].u4VCodecThreadNum; j++)
            {
                for (k = 0; k < a_prVcodecThreadID.u4VCodecThreadNum; k++)
                {
                    if (oal_hw_context[i].u4VCodecThreadID[j] == a_prVcodecThreadID.u4VCodecThreadID[k])
                    {
                        MFV_LOGE("[setCurr_HWLockSlot_Thread_ID] Curr Already exist in %d Slot\n", i);
                        *a_prIndex = i;
                        return &oal_hw_context[i];
                    }
                }
            }
        }
    }

    // if not exist in table,  find a new free slot and put it
    for(i = 0; i < VCODEC_MULTIPLE_INSTANCE_NUM; i++)
    {
        if (oal_hw_context[i].u4VCodecThreadNum == VCODEC_THREAD_MAX_NUM)
        {
            oal_hw_context[i].u4VCodecThreadNum = a_prVcodecThreadID.u4VCodecThreadNum;
            for(j = 0; j < a_prVcodecThreadID.u4VCodecThreadNum; j++)
            {
                oal_hw_context[i].u4VCodecThreadID[j] = a_prVcodecThreadID.u4VCodecThreadID[j];
                MFV_LOGD("[setCurr_HWLockSlot_Thread_ID] setCurr %d Slot, %d\n", i, oal_hw_context[i].u4VCodecThreadID[j]);
            }
            *a_prIndex = i;
            return &oal_hw_context[i];
        }
    }

    {
        MFV_LOGE("[VCodec Error][ERROR] setCurr_HWLockSlot_Thread_ID All %d Slots unavaliable\n", VCODEC_MULTIPLE_INSTANCE_NUM);
        oal_hw_context[0].u4VCodecThreadNum = a_prVcodecThreadID.u4VCodecThreadNum;
        for(i = 0; i < oal_hw_context[0].u4VCodecThreadNum; i++)
        {
            oal_hw_context[0].u4VCodecThreadID[i] = a_prVcodecThreadID.u4VCodecThreadID[i];
        }
        *a_prIndex = 0;
        return &oal_hw_context[0];
    }
}


VAL_VCODEC_OAL_HW_CONTEXT_T *freeCurr_HWLockSlot(VAL_ULONG_T ulpa)
{
    int i;
    int j;

    // check if current ObjId exist in oal_hw_context[i].ObjId

    for(i=0; i<VCODEC_MULTIPLE_INSTANCE_NUM; i++)
    {
        if( oal_hw_context[i].ObjId == ulpa)
        {
            oal_hw_context[i].ObjId = -1;
            for(j = 0; j < oal_hw_context[i].u4VCodecThreadNum; j++)
            {
                oal_hw_context[i].u4VCodecThreadID[j] = -1;
            }
            oal_hw_context[i].u4VCodecThreadNum = VCODEC_THREAD_MAX_NUM;
            oal_hw_context[i].Oal_HW_reg = (VAL_VCODEC_OAL_HW_REGISTER_T  *)0;
            MFV_LOGD("freeCurr_HWLockSlot %d Slot\n", i);
            return &oal_hw_context[i];
        }
    }

    MFV_LOGE("[VCodec Error][ERROR] freeCurr_HWLockSlot can't find pid in HWLockSlot\n");
    return 0;
}


void Add_NonCacheMemoryList(VAL_ULONG_T a_ulKVA, VAL_ULONG_T a_ulKPA, VAL_ULONG_T a_ulSize, VAL_UINT32_T a_u4VCodecThreadNum, VAL_UINT32_T* a_pu4VCodecThreadID)
{
    VAL_UINT32_T u4I = 0;
    VAL_UINT32_T u4J = 0;

    MFV_LOGD("Add_NonCacheMemoryList +, KVA = 0x%lx, KPA = 0x%lx, Size = 0x%lx\n", a_ulKVA, a_ulKPA, a_ulSize);

     for(u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++)
    {
        if ((grNonCacheMemoryList[u4I].ulKVA == 0xffffffff) && (grNonCacheMemoryList[u4I].ulKPA == 0xffffffff))
        {
            MFV_LOGD("ADD Add_NonCacheMemoryList index = %d, VCodecThreadNum = %d, curr_tid = %d\n",
                u4I, a_u4VCodecThreadNum, current->pid);

            grNonCacheMemoryList[u4I].u4VCodecThreadNum = a_u4VCodecThreadNum;
            for (u4J = 0; u4J < grNonCacheMemoryList[u4I].u4VCodecThreadNum; u4J++)
            {
                grNonCacheMemoryList[u4I].u4VCodecThreadID[u4J] = *(a_pu4VCodecThreadID + u4J);
                MFV_LOGD("[Add_NonCacheMemoryList] VCodecThreadNum = %d, VCodecThreadID = %d\n",
                    grNonCacheMemoryList[u4I].u4VCodecThreadNum, grNonCacheMemoryList[u4I].u4VCodecThreadID[u4J]);
            }

            grNonCacheMemoryList[u4I].ulKVA = a_ulKVA;
            grNonCacheMemoryList[u4I].ulKPA = a_ulKPA;
            grNonCacheMemoryList[u4I].ulSize = a_ulSize;
            break;
        }
    }

    if (u4I == VCODEC_MULTIPLE_INSTANCE_NUM_x_10)
    {
        MFV_LOGE("[ERROR] CAN'T ADD Add_NonCacheMemoryList, List is FULL!!\n");
    }

    MFV_LOGD("Add_NonCacheMemoryList -\n");
}

void Free_NonCacheMemoryList(VAL_ULONG_T a_ulKVA, VAL_ULONG_T a_ulKPA)
{
    VAL_UINT32_T u4I = 0;
    VAL_UINT32_T u4J = 0;

    MFV_LOGD("Free_NonCacheMemoryList +, KVA = 0x%lx, KPA = 0x%lx\n", a_ulKVA, a_ulKPA);

    for(u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++)
    {
        if ((grNonCacheMemoryList[u4I].ulKVA == a_ulKVA) && (grNonCacheMemoryList[u4I].ulKPA == a_ulKPA))
        {
            MFV_LOGD("Free Free_NonCacheMemoryList index = %d\n", u4I);
            grNonCacheMemoryList[u4I].u4VCodecThreadNum = VCODEC_THREAD_MAX_NUM;
            for (u4J = 0; u4J <VCODEC_THREAD_MAX_NUM; u4J++)
            {
                grNonCacheMemoryList[u4I].u4VCodecThreadID[u4J] = 0xffffffff;
            }

            grNonCacheMemoryList[u4I].ulKVA = -1L;
            grNonCacheMemoryList[u4I].ulKPA = -1L;
            grNonCacheMemoryList[u4I].ulSize = -1L;
            break;
        }
    }

    if (u4I == VCODEC_MULTIPLE_INSTANCE_NUM_x_10)
    {
        MFV_LOGE("[ERROR] CAN'T Free Free_NonCacheMemoryList, Address is not find!!\n");
    }

    MFV_LOGD("Free_NonCacheMemoryList -\n");
}


void Force_Free_NonCacheMemoryList(VAL_UINT32_T a_u4Tid)
{
    VAL_UINT32_T u4I = 0;
    VAL_UINT32_T u4J = 0;
    VAL_UINT32_T u4K = 0;

    MFV_LOGD("Force_Free_NonCacheMemoryList +, curr_id = %d", a_u4Tid);

    for (u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++) {
        if (grNonCacheMemoryList[u4I].u4VCodecThreadNum != VCODEC_THREAD_MAX_NUM)
        {
            for (u4J = 0; u4J < grNonCacheMemoryList[u4I].u4VCodecThreadNum; u4J++)
            {
                if (grNonCacheMemoryList[u4I].u4VCodecThreadID[u4J] == a_u4Tid)
                {
                    MFV_LOGE("[WARNING] Force_Free_NonCacheMemoryList index = %d, tid = %d, KVA = 0x%lx, KPA = 0x%lx, Size = %lu\n",
                        u4I, a_u4Tid, grNonCacheMemoryList[u4I].ulKVA, grNonCacheMemoryList[u4I].ulKPA, grNonCacheMemoryList[u4I].ulSize);

                    dma_free_coherent(0, grNonCacheMemoryList[u4I].ulSize, (void *)grNonCacheMemoryList[u4I].ulKVA, (dma_addr_t)grNonCacheMemoryList[u4I].ulKPA);

                    grNonCacheMemoryList[u4I].u4VCodecThreadNum = VCODEC_THREAD_MAX_NUM;
                    for (u4K = 0; u4K <VCODEC_THREAD_MAX_NUM; u4K++)
                    {
                        grNonCacheMemoryList[u4I].u4VCodecThreadID[u4K] = 0xffffffff;
                    }
                    grNonCacheMemoryList[u4I].ulKVA = -1L;
                    grNonCacheMemoryList[u4I].ulKPA = -1L;
                    grNonCacheMemoryList[u4I].ulSize = -1L;
                    break;
                }
            }
        }
    }

    MFV_LOGD("Force_Free_NonCacheMemoryList -, curr_id = %d", a_u4Tid);
}

VAL_UINT32_T Search_NonCacheMemoryList_By_KPA(VAL_ULONG_T a_ulKPA)
{
    VAL_UINT32_T u4I = 0;
    VAL_ULONG_T ulVA_Offset = 0;

    ulVA_Offset = a_ulKPA & 0x00000fff;

    MFV_LOGD("Search_NonCacheMemoryList_By_KPA +, KPA = 0x%lx, ulVA_Offset = 0x%lx\n", a_ulKPA, ulVA_Offset);

    for (u4I = 0; u4I < VCODEC_MULTIPLE_INSTANCE_NUM_x_10; u4I++) {
        if (grNonCacheMemoryList[u4I].ulKPA == (a_ulKPA - ulVA_Offset)) {
            MFV_LOGD("Find Search_NonCacheMemoryList_By_KPA index = %d\n", u4I);
            break;
        }
    }

    if (u4I == VCODEC_MULTIPLE_INSTANCE_NUM_x_10) {
        MFV_LOGE("[ERROR] CAN'T Find Search_NonCacheMemoryList_By_KPA, Address is not find!!\n");
        return (grNonCacheMemoryList[0].ulKVA + ulVA_Offset);
    }

    MFV_LOGD("Search_NonCacheMemoryList_By_KPA, ulVA = 0x%lx -\n", (grNonCacheMemoryList[u4I].ulKVA + ulVA_Offset));

    return (grNonCacheMemoryList[u4I].ulKVA + ulVA_Offset);
}
