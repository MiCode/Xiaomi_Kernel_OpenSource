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

#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>  /* proc file use */
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include "inc/mt_typedefs.h"
#ifdef CONFIG_MTK_CLKMGR
#include <mach/mt_clkmgr.h>
#endif
#include <mt-plat/sync_write.h>
#include "inc/camera_sysram.h"
#include "inc/camera_sysram_imp.h"

/* ----------------------------------------------------------------------------- */
#define ISP_VALID_REG_RANGE  0x10000
#define IMGSYS_BASE_ADDR     0x15000000
static struct SYSRAM_STRUCT Sysram;
/* ------------------------------------------------------------------------------ */
static void SYSRAM_GetTime(unsigned long long *pUS64, unsigned int *pSec, unsigned int *pUSec)
{
	unsigned long long TimeSec;

	TimeSec = ktime_get(); /* ns */
	do_div(TimeSec, 1000);

	*pUS64 = TimeSec;
	*pUSec = do_div(TimeSec, 1000000);
	*pSec = (unsigned long long)TimeSec;
}
/* ------------------------------------------------------------------------------ */
static void SYSRAM_CheckClock(void)
{
	/* LOG_MSG("AllocatedTbl(0x%08X), EnableClk(%d)", Sysram.AllocatedTbl, Sysram.EnableClk); */
	if (Sysram.AllocatedTbl) {
		if (!(Sysram.EnableClk)) {
			Sysram.EnableClk = true;

		/*	LOG_MSG("AllocatedTbl(0x%08lX), EnableClk(%d)",*/
		/*			Sysram.AllocatedTbl,*/
		/*			Sysram.EnableClk);*/

		}
	} else{
		if (Sysram.EnableClk) {
			Sysram.EnableClk = false;

		/*	LOG_MSG("AllocatedTbl(0x%08lX), EnableClk(%d)",*/
		/*			Sysram.AllocatedTbl,*/
		/*			Sysram.EnableClk);*/

		}
	}
}
/* ------------------------------------------------------------------------------ */
static void SYSRAM_DumpResMgr(void)
{
	unsigned int u4Idx = 0;

	LOG_MSG("TotalUserCount(%d), AllocatedTbl(0x%X)",
			Sysram.TotalUserCount,
			Sysram.AllocatedTbl);

	for (u4Idx = 0; u4Idx < SYSRAM_USER_AMOUNT; u4Idx++) {
		if (Sysram.AllocatedSize[u4Idx] > 0) {
			struct SYSRAM_USER_STRUCT * const pUserInfo = &Sysram.UserInfo[u4Idx];

			LOG_MSG("[id:%u][%s][size:0x%X][pid:%d][tgid:%d][%s][%5u.%06u]",
					u4Idx,
					SysramUserName[u4Idx],
					Sysram.AllocatedSize[u4Idx],
					pUserInfo->pid,
					pUserInfo->tgid,
					pUserInfo->ProcName,
					pUserInfo->TimeS,
					pUserInfo->TimeUS);
		}
	}
	LOG_MSG("End");
}
/* ------------------------------------------------------------------------------ */
static inline bool SYSRAM_IsBadOwner(enum SYSRAM_USER_ENUM const User)
{
	if (User >= SYSRAM_USER_AMOUNT || User < 0)
		return MTRUE;
	else
		return MFALSE;
}
/* ------------------------------------------------------------------------------ */
static void SYSRAM_SetUserTaskInfo(enum SYSRAM_USER_ENUM const User)
{
	if (!SYSRAM_IsBadOwner(User)) {
		struct SYSRAM_USER_STRUCT * const pUserInfo = &Sysram.UserInfo[User];

		pUserInfo->pid = current->pid;
		pUserInfo->tgid = current->tgid;
		memcpy(pUserInfo->ProcName, current->comm, sizeof(pUserInfo->ProcName));

		SYSRAM_GetTime(&(pUserInfo->Time64), &(pUserInfo->TimeS), &(pUserInfo->TimeUS));
	}
}
/* ------------------------------------------------------------------------------ */
static void SYSRAM_ResetUserTaskInfo(enum SYSRAM_USER_ENUM const User)
{
	if (!SYSRAM_IsBadOwner(User)) {
		struct SYSRAM_USER_STRUCT * const pUserInfo = &Sysram.UserInfo[User];

		memset(pUserInfo, 0, sizeof(*pUserInfo));
	}
}
/* ------------------------------------------------------------------------------ */
static inline void SYSRAM_SpinLock(void)
{
	spin_lock(&Sysram.SpinLock);
}
/* ------------------------------------------------------------------------------ */
static inline void SYSRAM_SpinUnlock(void)
{
	spin_unlock(&Sysram.SpinLock);
}
/* ------------------------------------------------------------------------------ */
static inline bool SYSRAM_UserIsLocked(enum SYSRAM_USER_ENUM const User)
{
	if ((1 << User) & Sysram.AllocatedTbl)
		return MTRUE;
	else
		return MFALSE;
}
/* ------------------------------------------------------------------------------ */
static inline bool SYSRAM_UserIsUnlocked(enum SYSRAM_USER_ENUM const User)
{
	if (SYSRAM_UserIsLocked(User) == 0)
		return MTRUE;
	else
		return MFALSE;
}
/* ------------------------------------------------------------------------------ */
static void SYSRAM_LockUser(enum SYSRAM_USER_ENUM const User, unsigned int const Size)
{
	if (SYSRAM_UserIsLocked(User))
		return;

	Sysram.TotalUserCount++;
	Sysram.AllocatedTbl |= (1 << User);
	Sysram.AllocatedSize[User] = Size;
	SYSRAM_SetUserTaskInfo(User);
	/* Debug Log. */
	if ((1<<User) & SysramLogUserMask) {
		struct SYSRAM_USER_STRUCT * const pUserInfo = &Sysram.UserInfo[User];

		LOG_MSG("[%s][%u bytes]OK, Time(%u.%06u)",
				SysramUserName[User],
				Sysram.AllocatedSize[User],
				pUserInfo->TimeS,
				pUserInfo->TimeUS);
	}
}
/* ------------------------------------------------------------------------------ */
static void SYSRAM_UnlockUser(enum SYSRAM_USER_ENUM const User)
{
	if (SYSRAM_UserIsUnlocked(User))
		return;
	/* Debug Log. */
	if ((1<<User) & SysramLogUserMask) {
		struct SYSRAM_USER_STRUCT * const pUserInfo = &Sysram.UserInfo[User];
		unsigned int Sec, USec;
		unsigned long long Time64 = 0;

		SYSRAM_GetTime(&Time64, &Sec, &USec);

		LOG_MSG("[%s][%u bytes]Time(%u.%06u - %u.%06u)(%u.%06u)",
				SysramUserName[User],
				Sysram.AllocatedSize[User],
				pUserInfo->TimeS,
				pUserInfo->TimeUS,
				Sec,
				USec,
				((unsigned int)(Time64-pUserInfo->Time64))/1000,
				((unsigned int)(Time64-pUserInfo->Time64))%1000);
	}

	if (Sysram.TotalUserCount > 0)
		Sysram.TotalUserCount--;

	Sysram.AllocatedTbl &= (~(1 << User));
	Sysram.AllocatedSize[User] = 0;
	SYSRAM_ResetUserTaskInfo(User);
}
/* ------------------------------------------------------------------------------ */
static void SYSRAM_DumpLayout(void)
{
	unsigned int Index = 0;
	struct SYSRAM_MEM_NODE_STRUCT *pCurrNode = NULL;

	LOG_DMP("[SYSRAM_DumpLayout]\n");
	LOG_DMP("AllocatedTbl = 0x%08X\n", Sysram.AllocatedTbl);
	LOG_DMP("=========================================\n");
	for (Index = 0; Index < SYSRAM_MEM_BANK_AMOUNT; Index++) {
		LOG_DMP("\n [Mem Pool %d] (IndexTbl, UserCount)=(%X, %d)\n",
				Index,
				SysramMemPoolInfo[Index].IndexTbl,
				SysramMemPoolInfo[Index].UserCount);
		LOG_DMP(
"[Locked Time][Owner Offset Size Index pCurrent pPrevious pNext][pid tgid][Proc Name / Owner Name]\n");
		pCurrNode = &SysramMemPoolInfo[Index].pMemNode[0];
		while (pCurrNode != NULL) {
			enum SYSRAM_USER_ENUM const User = pCurrNode->User;

			if (SYSRAM_IsBadOwner(User)) {
				LOG_DMP(
				"------------ --------  %2d\t0x%05X 0x%05X  %d    %p %p\t%p\n",
					pCurrNode->User,
					pCurrNode->Offset,
					pCurrNode->Length,
					pCurrNode->Index,
					pCurrNode,
					pCurrNode->pPrev,
					pCurrNode->pNext);
			} else{
				struct SYSRAM_USER_STRUCT * const pUserInfo = &Sysram.UserInfo[User];

				LOG_DMP(
	"%5u.%06u  %2d\t0x%05X 0x%05X  %d    %p %p\t%p   %-4d %-4d \"%s\" / \"%s\"\n",
					pUserInfo->TimeS,
					pUserInfo->TimeUS,
					User,
					pCurrNode->Offset,
					pCurrNode->Length,
					pCurrNode->Index,
					pCurrNode,
					pCurrNode->pPrev,
					pCurrNode->pNext,
					pUserInfo->pid,
					pUserInfo->tgid,
					pUserInfo->ProcName,
					SysramUserName[User]);
			}
			pCurrNode = pCurrNode->pNext;
		};
	}
	LOG_DMP("\n");
	SYSRAM_DumpResMgr();
}
/* ------------------------------------------------------------------------------ */
static struct SYSRAM_MEM_NODE_STRUCT *SYSRAM_AllocNode(struct SYSRAM_MEM_POOL_STRUCT * const pMemPoolInfo)
{
	struct SYSRAM_MEM_NODE_STRUCT *pNode = NULL;
	unsigned int Index = 0;

	for (Index = 0; Index < pMemPoolInfo->UserAmount; Index += 1) {
		if ((pMemPoolInfo->IndexTbl) & (1 << Index)) {
			pMemPoolInfo->IndexTbl &= (~(1 << Index));
			/* A free node is found. */
			pNode = &pMemPoolInfo->pMemNode[Index];
			pNode->User  = SYSRAM_USER_NONE;
			pNode->Offset = 0;
			pNode->Length = 0;
			pNode->pPrev   = NULL;
			pNode->pNext   = NULL;
			pNode->Index = Index;
			break;
		}
	}
	/* Shouldn't happen. */
	if (!pNode)
		LOG_MSG("returns NULL - pMemPoolInfo->IndexTbl(%X)", pMemPoolInfo->IndexTbl);

	return  pNode;
}
/* ------------------------------------------------------------------------------ */
static void SYSRAM_FreeNode(
	struct SYSRAM_MEM_POOL_STRUCT * const pMemPoolInfo,
	struct SYSRAM_MEM_NODE_STRUCT * const pNode)
{
	pMemPoolInfo->IndexTbl |= (1<<pNode->Index);
	pNode->User    = SYSRAM_USER_NONE;
	pNode->Offset  = 0;
	pNode->Length  = 0;
	pNode->pPrev   = NULL;
	pNode->pNext   = NULL;
	pNode->Index   = 0;
}
/* ------------------------------------------------------------------------------ */
static bool SYSRAM_IsLegalSizeToAlloc(
	enum SYSRAM_MEM_BANK_ENUM const MemBankNo,
	enum SYSRAM_USER_ENUM     const User,
	unsigned int              const Size)
{
	unsigned int MaxSize = 0;
	/* (1) Check the memory pool. */
	switch (MemBankNo) {
	case SYSRAM_MEM_BANK_BAD:
	case SYSRAM_MEM_BANK_AMOUNT:
	{
	/* Illegal Memory Pool: return "illegal" */
	/* Shouldn't happen. */
		goto EXIT;
	}
	default:
		break;
	}
	/* (2) */
	/* Here we use the dynamic memory pools. */
	MaxSize = SysramUserSize[User];

EXIT:
	if (MaxSize < Size) {
		LOG_MSG("[User:%s]requested size(0x%X) > max size(0x%X)",
				SysramUserName[User],
				Size,
				MaxSize);
		SYSRAM_DumpLayout();
		return MFALSE;
	}
	return MTRUE;
}
/* ------------------------------------------------------------------------------ */

/* should be 2^N, 4/8/2048 bytes alignment only*/
/*First fit algorithm*/

static unsigned int SYSRAM_AllocUserPhy(
	enum SYSRAM_USER_ENUM     const User,
	unsigned int              const Size,
	unsigned int              const Alignment,
	enum SYSRAM_MEM_BANK_ENUM const MemBankNo
)
{
	struct SYSRAM_MEM_NODE_STRUCT *pSplitNode = NULL;
	struct SYSRAM_MEM_NODE_STRUCT *pCurrNode = NULL;
	unsigned int AlingnedAddr = 0;
	unsigned int ActualSize = 0;

	struct SYSRAM_MEM_POOL_STRUCT * const pMemPoolInfo = SYSRAM_GetMemPoolInfo(MemBankNo);

	if (!pMemPoolInfo)
		return  0;

	pCurrNode = &pMemPoolInfo->pMemNode[0];
	for (; pCurrNode && pCurrNode->Offset < pMemPoolInfo->Size; pCurrNode = pCurrNode->pNext) {
		if (pCurrNode->User == SYSRAM_USER_NONE) {
			/* Free space */
			AlingnedAddr = (pCurrNode->Offset + Alignment - 1)&(~(Alignment - 1));
			ActualSize = Size + AlingnedAddr - pCurrNode->Offset;
			if (ActualSize <= pCurrNode->Length) {
				/* Hit!! Split into 2 */
				/* pSplitNode pointers to the next available (free) node. */
				pSplitNode = SYSRAM_AllocNode(pMemPoolInfo);
				pSplitNode->Offset = pCurrNode->Offset + ActualSize;
				pSplitNode->Length = pCurrNode->Length - ActualSize;
				pSplitNode->pPrev  = pCurrNode;
				pSplitNode->pNext  = pCurrNode->pNext;

				pCurrNode->User    = User;
				pCurrNode->Length  = ActualSize;
				pCurrNode->pNext   = pSplitNode;

				if (pSplitNode->pNext != NULL)
					pSplitNode->pNext->pPrev = pSplitNode;

				pMemPoolInfo->UserCount++;
				break;
			}
			/* Not hit */
			ActualSize = 0;
		}
	};

	return ActualSize ? (AlingnedAddr + pMemPoolInfo->Addr) : 0;
}
/* ------------------------------------------------------------------------------ */
static bool SYSRAM_FreeUserPhy(
	enum SYSRAM_USER_ENUM     const User,
	enum SYSRAM_MEM_BANK_ENUM const MemBankNo)
{
	bool Ret = MFALSE;
	struct SYSRAM_MEM_NODE_STRUCT *pPrevOrNextNode = NULL;
	struct SYSRAM_MEM_NODE_STRUCT *pCurrNode = NULL;
	struct SYSRAM_MEM_NODE_STRUCT *pTempNode = NULL;

	struct SYSRAM_MEM_POOL_STRUCT * const pMemPoolInfo = SYSRAM_GetMemPoolInfo(MemBankNo);

	if (!pMemPoolInfo) {
		LOG_MSG("pMemPoolInfo==NULL, User(%d), MemBankNo(%d)", User, MemBankNo);
		return  MFALSE;
	}

	pCurrNode = &pMemPoolInfo->pMemNode[0];
	for (; pCurrNode; pCurrNode = pCurrNode->pNext) {
		if (User == pCurrNode->User) {
			Ret = MTRUE;   /* user is found. */
			if (pMemPoolInfo->UserCount > 0)
				pMemPoolInfo->UserCount--;

			pCurrNode->User = SYSRAM_USER_NONE;
			if (pCurrNode->pPrev != NULL) {
				pPrevOrNextNode = pCurrNode->pPrev;

				if (pPrevOrNextNode->User == SYSRAM_USER_NONE) {
					/* Merge previous: prev(o) + curr(x) */
					pTempNode = pCurrNode;
					pCurrNode = pPrevOrNextNode;
					pCurrNode->Length += pTempNode->Length;
					pCurrNode->pNext = pTempNode->pNext;
					if (pTempNode->pNext != NULL)
						pTempNode->pNext->pPrev = pCurrNode;
					SYSRAM_FreeNode(pMemPoolInfo, pTempNode);
				}
			}

			if (pCurrNode->pNext != NULL) {
				pPrevOrNextNode = pCurrNode->pNext;

				if (pPrevOrNextNode->User == SYSRAM_USER_NONE) {
					/* Merge next: curr(o) + next(x) */
					pTempNode = pPrevOrNextNode;
					pCurrNode->Length += pTempNode->Length;
					pCurrNode->pNext = pTempNode->pNext;
					if (pCurrNode->pNext != NULL)
						pCurrNode->pNext->pPrev = pCurrNode;
					SYSRAM_FreeNode(pMemPoolInfo, pTempNode);
				}
			}
			break;
		}
	}

	return  Ret;
}
/* ------------------------------------------------------------------------------ */
static unsigned int SYSRAM_AllocUser(
	enum SYSRAM_USER_ENUM const User,
	unsigned int                Size,
	unsigned int          const Alignment)
{
	unsigned int Addr = 0;

	enum SYSRAM_MEM_BANK_ENUM const MemBankNo = SYSRAM_GetMemBankNo(User);

	if (SYSRAM_IsBadOwner(User)) {
		LOG_MSG("User(%d) out of range(%d)", User, SYSRAM_USER_AMOUNT);
		return  0;
	}

	if (!SYSRAM_IsLegalSizeToAlloc(MemBankNo, User, Size))
		return 0;

	switch (MemBankNo) {
	case SYSRAM_MEM_BANK_BAD:
	case SYSRAM_MEM_BANK_AMOUNT:
	{
	/* Do nothing. */
		break;
	}
	default:
	{
		Addr = SYSRAM_AllocUserPhy(
					User,
					Size,
					Alignment,
					MemBankNo);
		break;
	}
	}

	if (Addr > 0)
		SYSRAM_LockUser(User, Size);

	return Addr;
}
/* ------------------------------------------------------------------------------ */
static void SYSRAM_FreeUser(enum SYSRAM_USER_ENUM const User)
{
	enum SYSRAM_MEM_BANK_ENUM const MemBankNo = SYSRAM_GetMemBankNo(User);

	switch (MemBankNo) {
	case SYSRAM_MEM_BANK_BAD:
	case SYSRAM_MEM_BANK_AMOUNT:
	{
	/* Do nothing. */
		break;
	}
	default:
	{
		if (SYSRAM_FreeUserPhy(User, MemBankNo)) {
			SYSRAM_UnlockUser(User);
		} else{
			LOG_MSG("Cannot free User(%d)", User);
			SYSRAM_DumpLayout();
		}
		break;
	}
	}
}
/* ------------------------------------------------------------------------------ */
static unsigned int SYSRAM_MsToJiffies(unsigned int TimeMs)
{
	return (TimeMs*HZ + 512) >> 10;
}
/* ------------------------------------------------------------------------------ */
static unsigned int SYSRAM_TryAllocUser(
	enum SYSRAM_USER_ENUM const User,
	unsigned int          const Size,
	unsigned int          const Alignment)
{
	unsigned int Addr = 0;

	SYSRAM_SpinLock();

	if (SYSRAM_UserIsLocked(User)) {
		SYSRAM_SpinUnlock();
		LOG_MSG("[User:%s]has been already allocated!", SysramUserName[User]);
		return 0;
	}

	Addr = SYSRAM_AllocUser(User, Size, Alignment);
	if (Addr != 0)
		SYSRAM_CheckClock();
	SYSRAM_SpinUnlock();

	return  Addr;
}
/* ------------------------------------------------------------------------------ */
static unsigned int SYSRAM_IOC_Alloc(
	enum SYSRAM_USER_ENUM const User,
	unsigned int          const Size,
	unsigned int                Alignment,
	unsigned int          const TimeoutMS)
{
	unsigned int Addr = 0;
	signed int TimeOut = 0;

	if (SYSRAM_IsBadOwner(User)) {
		LOG_MSG("User(%d) out of range(%d)", User, SYSRAM_USER_AMOUNT);
		return  0;
	}

	if (Size == 0) {
		LOG_MSG("[User:%s]allocates 0 size!", SysramUserName[User]);
		return  0;
	}

	Addr = SYSRAM_TryAllocUser(User, Size, Alignment);
	if (Addr != 0       /* success */
	||	TimeoutMS == 0   /* failure without a timeout specified */)
		goto EXIT;

	TimeOut = wait_event_interruptible_timeout(
		Sysram.WaitQueueHead,
	0 != (Addr = SYSRAM_TryAllocUser(User, Size, Alignment)),
		SYSRAM_MsToJiffies(TimeoutMS));

	if (0 == TimeOut && 0 == Addr)
		LOG_MSG("[User:%s]allocate timeout", SysramUserName[User]);

EXIT:
	if (Addr == 0) {   /* Failure */
		LOG_MSG("[User:%s]fails to allocate.Size(%u), Alignment(%u), TimeoutMS(%u)",
				SysramUserName[User],
				Size,
				Alignment,
				TimeoutMS);
		SYSRAM_DumpLayout();
	} else{   /* Success */
		if ((1<<User) & SysramLogUserMask) {
			LOG_MSG("[User:%s]%u bytes OK",
					SysramUserName[User],
					Size);
		}
	}

	return Addr;
}
/* ------------------------------------------------------------------------------ */
static void SYSRAM_IOC_Free(enum SYSRAM_USER_ENUM User)
{
	if (SYSRAM_IsBadOwner(User)) {
		LOG_MSG("User(%d) out of range(%d)", User, SYSRAM_USER_AMOUNT);
		return;
	}

	SYSRAM_SpinLock();
	SYSRAM_FreeUser(User);
	wake_up_interruptible(&Sysram.WaitQueueHead);
	SYSRAM_CheckClock();
	SYSRAM_SpinUnlock();

	if ((1<<User) & SysramLogUserMask)
		LOG_MSG("[User:%s]Done", SysramUserName[User]);
}
/* ------------------------------------------------------------------------------ */
static int SYSRAM_Open(
	struct inode *pInode,
	struct file *pFile)
{
	int Ret = 0;
	unsigned int Sec = 0, USec = 0;
	unsigned long long Time64 = 0;
	struct SYSRAM_PROC_STRUCT *pProc;

	SYSRAM_GetTime(&Time64, &Sec, &USec);

	LOG_MSG("Cur:Name(%s), pid(%d), tgid(%d), Time(%d.%06d)",
			current->comm,
			current->pid,
			current->tgid,
			Sec,
			USec);

	SYSRAM_SpinLock();

	pFile->private_data = kmalloc(sizeof(struct SYSRAM_PROC_STRUCT), GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		Ret = -ENOMEM;
	} else{
		pProc = (struct SYSRAM_PROC_STRUCT *)(pFile->private_data);
		pProc->Pid = 0;
		pProc->Tgid = 0;
		strncpy(pProc->ProcName, SYSRAM_PROC_NAME, sizeof(pProc->ProcName)-1);
		pProc->ProcName[sizeof(pProc->ProcName)-1] = '\0';
		pProc->Table = 0;
		pProc->Time64 = Time64;
		pProc->TimeS = Sec;
		pProc->TimeUS = USec;
	}

	SYSRAM_SpinUnlock();

	if (Ret == (-ENOMEM)) {
		LOG_MSG("No enough memory");

		/*LOG_MSG("Cur:Name(%s), pid(%d), tgid(%d), Time(%ld.%06ld)",*/
		/*		current->comm,*/
		/*		current->pid,*/
		/*		current->tgid,*/
		/*		Sec,*/
		/*		USec);*/

	}

	return Ret;
}
/* ------------------------------------------------------------------------------ */
static int SYSRAM_Release(
	struct inode *pInode,
	struct file *pFile)
{
	unsigned int Index = 0;
	unsigned int Sec = 0, USec = 0;
	unsigned long long Time64 = 0;
	struct SYSRAM_PROC_STRUCT *pProc;

	SYSRAM_GetTime(&Time64, &Sec, &USec);

	LOG_MSG("Cur:Name(%s), pid(%d), tgid(%d), Time(%d.%06d)",
			current->comm,
			current->pid,
			current->tgid,
			Sec,
			USec);

	if (pFile->private_data != NULL) {
		pProc = (struct SYSRAM_PROC_STRUCT *)(pFile->private_data);

		if (pProc->Pid != 0 ||
			pProc->Tgid != 0 ||
			pProc->Table != 0) {
			/* */
			LOG_MSG("Proc:Name(%s), pid(%d), tgid(%d), Table(0x%08X), Time(%d.%06d)",
					pProc->ProcName,
					pProc->Pid,
					pProc->Tgid,
					pProc->Table,
					pProc->TimeS,
					pProc->TimeUS);

			if (pProc->Table) {
				LOG_MSG("Force to release");

				/*LOG_MSG("Proc:Name(%s), pid(%d), tgid(%d), Table(0x%08lX), Time(%ld.%06ld)",*/
				/*		pProc->ProcName,*/
				/*		pProc->Pid,*/
				/*		pProc->Tgid,*/
				/*		pProc->Table,*/
				/*		pProc->TimeS,*/
				/*		pProc->TimeUS);*/

				SYSRAM_DumpLayout();

				for (Index = 0; Index < SYSRAM_USER_AMOUNT; Index++) {
					if (pProc->Table & (1 << Index))
						SYSRAM_IOC_Free((enum SYSRAM_USER_ENUM)Index);
				}
			}
		}

		kfree(pFile->private_data);
		pFile->private_data = NULL;
	} else{
		LOG_MSG("private_data is NULL");

		/*LOG_MSG("Cur:Name(%s), pid(%d), tgid(%d), Time(%ld.%06ld)",*/
		/*		current->comm,*/
		/*		current->pid,*/
		/*		current->tgid,*/
		/*		Sec,*/
		/*		USec);*/

	}

	return 0;
}
/* ------------------------------------------------------------------------------ */
static int SYSRAM_Flush(
	struct file *pFile,
	fl_owner_t      Id)
{
	unsigned int Index = 0;
	unsigned int Sec = 0, USec = 0;
	unsigned long long Time64 = 0;
	struct SYSRAM_PROC_STRUCT *pProc;

	SYSRAM_GetTime(&Time64, &Sec, &USec);

	LOG_MSG("Cur:Name(%s), pid(%d), tgid(%d), Time(%d.%06d)",
			current->comm,
			current->pid,
			current->tgid,
			Sec,
			USec);

	if (pFile->private_data != NULL) {
		pProc = (struct SYSRAM_PROC_STRUCT *)pFile->private_data;

		if (pProc->Pid != 0 ||
			pProc->Tgid != 0 ||
			pProc->Table != 0) {
			/* */
			LOG_MSG("Proc:Name(%s), pid(%d), tgid(%d), Table(0x%08X), Time(%d.%06d)",
					pProc->ProcName,
					pProc->Pid,
					pProc->Tgid,
					pProc->Table,
					pProc->TimeS,
					pProc->TimeUS);

			if (pProc->Tgid == 0 && pProc->Table != 0) {
				LOG_MSG("No Tgid info");

				/*LOG_MSG("Cur:Name(%s), pid(%d), tgid(%d), Time(%ld.%06ld)",*/
				/*		current->comm,*/
				/*		current->pid,*/
				/*		current->tgid,*/
				/*		Sec,*/
				/*		USec);*/
				/*LOG_MSG("Proc:Name(%s), pid(%d), tgid(%d), Table(0x%08lX), Time(%ld.%06ld)",*/
				/*		pProc->ProcName,*/
				/*		pProc->Pid,*/
				/*		pProc->Tgid,*/
				/*		pProc->Table,*/
				/*		pProc->TimeS,*/
				/*		pProc->TimeUS);*/

			} else{
				if ((pProc->Tgid == current->tgid) ||
					((pProc->Tgid != current->tgid) && (strcmp(current->comm, "binder") == 0))) {
					/* */
					if (pProc->Table) {
						LOG_MSG("Force to release");

						/*LOG_MSG("Cur:Name(%s), pid(%d), tgid(%d), Time(%ld.%06ld)",*/
						/*		current->comm,*/
						/*		current->pid,*/
						/*		current->tgid,*/
						/*		Sec,*/
						/*		USec);*/

						SYSRAM_DumpLayout();

					for (Index = 0; Index < SYSRAM_USER_AMOUNT; Index++) {
					if (pProc->Table & (1 << Index))
						SYSRAM_IOC_Free((enum SYSRAM_USER_ENUM)Index);
					}

						pProc->Table = 0;
					}
				}
			}
		}
	} else{
		LOG_MSG("private_data is NULL");

		/*LOG_MSG("Cur:Name(%s), pid(%d), tgid(%d), Time(%ld.%06ld)",*/
		/*		current->comm,*/
		/*		current->pid,*/
		/*		current->tgid,*/
		/*		Sec,*/
		/*		USec);*/

	}

	return 0;
}
/* ------------------------------------------------------------------------------ */
static int SYSRAM_mmap(
	struct file *pFile,
	struct vm_area_struct *pVma)
{
	long length = 0;
	unsigned int pfn = 0x0;
	/* LOG_MSG(""); */
	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);

	length = pVma->vm_end - pVma->vm_start;

	pfn = pVma->vm_pgoff<<PAGE_SHIFT;/* page from number, physical address of kernel memory */
	LOG_MSG("pVma->vm_pgoff(0x%lx), phy(0x%lx), pVmapVma->vm_start(0x%lx), pVma->vm_end(0x%lx), length(%lx)",
			pVma->vm_pgoff, pVma->vm_pgoff<<PAGE_SHIFT, pVma->vm_start, pVma->vm_end, length);
	if ((length > ISP_VALID_REG_RANGE) ||
		(pfn < IMGSYS_BASE_ADDR) ||
		(pfn > (IMGSYS_BASE_ADDR+ISP_VALID_REG_RANGE))) {
		LOG_MSG("mmap range error : vm_start(0x%lx), vm_end(0x%lx), length(%lx), pfn(%u)!",
		pVma->vm_start, pVma->vm_end, length, (unsigned int)pfn);
		return -EAGAIN;
	}
	if (remap_pfn_range(
		pVma,
		pVma->vm_start,
		pVma->vm_pgoff,
		pVma->vm_end - pVma->vm_start,
		pVma->vm_page_prot)) {
		/* */
		LOG_PR_ERR("fail");
		return -EAGAIN;
	}

	return 0;
}
/* ------------------------------------------------------------------------------ */
static long SYSRAM_Ioctl(
	struct file *pFile,
	unsigned int    Cmd,
	unsigned long   Param)
{
	signed int Ret = 0;
	unsigned int Sec = 0, USec = 0;
	unsigned long long Time64 = 0;
	struct SYSRAM_PROC_STRUCT *pProc = (struct SYSRAM_PROC_STRUCT *)pFile->private_data;
	struct SYSRAM_ALLOC_STRUCT Alloc;
	enum SYSRAM_USER_ENUM User;

	SYSRAM_GetTime(&Time64, &Sec, &USec);

	/*LOG_MSG("Cur:Name(%s), pid(%d), tgid(%d), Time(%ld.%06ld)",*/
	/*		current->comm,*/
	/*		current->pid,*/
	/*		current->tgid,*/
	/*		Sec,*/
	/*		USec);*/

	if (pFile->private_data == NULL) {
		LOG_MSG("private_data is NULL.");
		Ret = -EFAULT;
		goto EXIT;
	}

	switch (Cmd) {
	case SYSRAM_ALLOC:
	{
		if (copy_from_user(&Alloc, (void *)Param, sizeof(struct SYSRAM_ALLOC_STRUCT)) == 0) {
			Alloc.Addr = SYSRAM_IOC_Alloc(
							Alloc.User,
							Alloc.Size,
							Alloc.Alignment,
							Alloc.TimeoutMS);
			if (Alloc.Addr != 0) {
				SYSRAM_SpinLock();
				pProc->Table |= (1 << Alloc.User);
				if (pProc->Tgid == 0) {
					pProc->Pid = current->pid;
					pProc->Tgid = current->tgid;
					strncpy(pProc->ProcName, current->comm, sizeof(pProc->ProcName)-1);
					pProc->ProcName[sizeof(pProc->ProcName)-1] = '\0';
					SYSRAM_SpinUnlock();
				} else{
					SYSRAM_SpinUnlock();
					if (pProc->Tgid != current->tgid) {
						LOG_MSG("Tgid is inconsistent");
						Ret = -EFAULT;
					}
				}
			} else{
				Ret = -EFAULT;
			}

			if (copy_to_user((void *)Param, &Alloc, sizeof(struct SYSRAM_ALLOC_STRUCT))) {
				LOG_MSG("copy to user failed");
				Ret = -EFAULT;
			}
		} else{
			LOG_MSG("copy_from_user fail");
			Ret = -EFAULT;
		}
		break;
	}

	case SYSRAM_FREE:
	{
		if (copy_from_user(&User, (void *)Param, sizeof(enum SYSRAM_USER_ENUM)) == 0) {
			if (User >= SYSRAM_USER_AMOUNT) {
				LOG_MSG("invalid User(%d)", User);
				Ret = -EFAULT;
				break;
			}
			SYSRAM_SpinLock();
			if ((pProc->Table) & (1 << User)) {
				SYSRAM_SpinUnlock();
				SYSRAM_IOC_Free(User);
				SYSRAM_SpinLock();

				pProc->Table &= (~(1 << User));
				if (pProc->Table == 0) {
					pProc->Pid = 0;
					pProc->Tgid = 0;
					strncpy(pProc->ProcName, SYSRAM_PROC_NAME, sizeof(pProc->ProcName)-1);
					pProc->ProcName[sizeof(pProc->ProcName)-1] = '\0';
				}
				SYSRAM_SpinUnlock();
			} else{
				SYSRAM_SpinUnlock();
				LOG_MSG("Freeing unallocated buffer user(%d)", User);
				Ret = -EFAULT;
			}
		} else{
			LOG_MSG("copy_from_user fail");
			Ret = -EFAULT;
		}
		break;
	}
	case SYSRAM_DUMP:
	{
		SYSRAM_DumpLayout();
		break;
	}
	default:
	{
		LOG_MSG("No such command");
		Ret = -EINVAL;
		break;
	}
	}

EXIT:
	if (Ret != 0) {
		LOG_MSG("Fail");
		LOG_MSG("Cur:Name(%s), pid(%d), tgid(%d), Time(%d.%06d)",
				current->comm,
				current->pid,
				current->tgid,
				Sec,
				USec);
		if (pFile->private_data != NULL) {
			LOG_MSG("Proc:Name(%s), pid(%d), tgid(%d), Table(0x%08X), Time(%d.%06d)",
					pProc->ProcName,
					pProc->Pid,
					pProc->Tgid,
					pProc->Table,
					Sec,
					USec);
		}
	}

	return Ret;
}
/* ------------------------------------------------------------------------------ */
static const struct file_operations SysramFileOper = {
	.owner         = THIS_MODULE,
	.open          = SYSRAM_Open,
	.release       = SYSRAM_Release,
	.flush         = SYSRAM_Flush,
	.unlocked_ioctl = SYSRAM_Ioctl,
	.mmap          = SYSRAM_mmap,
};
/* ------------------------------------------------------------------------------ */
static inline int SYSRAM_RegCharDrv(void)
{
	LOG_MSG("E");
	if (alloc_chrdev_region(&Sysram.DevNo, 0, 1, SYSRAM_DEV_NAME)) {
		LOG_PR_ERR("allocate device no failed");
		return -EAGAIN;
	}
	/* allocate driver */
	Sysram.pCharDrv = cdev_alloc();
	if (Sysram.pCharDrv == NULL) {
		unregister_chrdev_region(Sysram.DevNo, 1);
		LOG_PR_ERR("allocate mem for kobject failed");
		return -ENOMEM;
	}
	/* Attatch file operation. */
	cdev_init(Sysram.pCharDrv, &SysramFileOper);
	Sysram.pCharDrv->owner = THIS_MODULE;
	/* Add to system */
	if (cdev_add(Sysram.pCharDrv, Sysram.DevNo, 1)) {
		LOG_PR_ERR("Attatch file operation failed");
		unregister_chrdev_region(Sysram.DevNo, 1);
		return -EAGAIN;
	}
	LOG_MSG("X");
	return 0;
}
/* ------------------------------------------------------------------------------ */
static inline void SYSRAM_UnregCharDrv(void)
{
	LOG_MSG("E");
	/* Release char driver */
	cdev_del(Sysram.pCharDrv);
	unregister_chrdev_region(Sysram.DevNo, 1);
	LOG_MSG("X");
}
/* ------------------------------------------------------------------------------ */
static int SYSRAM_Probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	unsigned int Index = 0;
	struct device *sysram_device = NULL;

	LOG_MSG("E");
	/* register char driver */
	/* allocate major no */
	if (SYSRAM_RegCharDrv()) {
		LOG_MSG("register char failed");
		return -EAGAIN;
	}

	Sysram.pClass = class_create(THIS_MODULE, "SysramDrv");
	if (IS_ERR(Sysram.pClass)) {
		Ret = PTR_ERR(Sysram.pClass);
		LOG_PR_ERR("Unable to create class, err(%d)", Ret);
		return Ret;
	}
	sysram_device = device_create(
						Sysram.pClass,
						NULL,
						Sysram.DevNo,
						NULL,
						SYSRAM_DEV_NAME);
	/* Initialize variables */
	spin_lock_init(&Sysram.SpinLock);
	Sysram.TotalUserCount   = 0;
	Sysram.AllocatedTbl     = 0;
	memset(Sysram.AllocatedSize, 0, sizeof(Sysram.AllocatedSize));
	memset(Sysram.UserInfo, 0, sizeof(Sysram.UserInfo));
	init_waitqueue_head(&Sysram.WaitQueueHead);
	Sysram.EnableClk = MFALSE;

	for (Index = 0; Index < SYSRAM_MEM_BANK_AMOUNT; Index++) {
		SysramMemPoolInfo[Index].pMemNode[0].User  = SYSRAM_USER_NONE;
		SysramMemPoolInfo[Index].pMemNode[0].Offset = 0;
		SysramMemPoolInfo[Index].pMemNode[0].Length = SysramMemPoolInfo[Index].Size;
		SysramMemPoolInfo[Index].pMemNode[0].Index = 0;
		SysramMemPoolInfo[Index].pMemNode[0].pNext = NULL;
		SysramMemPoolInfo[Index].pMemNode[0].pPrev = NULL;
		SysramMemPoolInfo[Index].IndexTbl = (~0x1);
		SysramMemPoolInfo[Index].UserCount = 0;
	}

	for (Index = 0; Index < SYSRAM_USER_AMOUNT; Index++)
		Sysram.AllocatedSize[Index] = 0;
	Sysram.DebugFlag = SYSRAM_DEBUG_DEFAULT;

	LOG_MSG("X");
	return Ret;
}
/* ------------------------------------------------------------------------------ */
static int SYSRAM_Remove(struct platform_device *pDev)
{
	LOG_MSG("E");
	/* unregister char driver. */
	SYSRAM_UnregCharDrv();

	device_destroy(Sysram.pClass, Sysram.DevNo);
	class_destroy(Sysram.pClass);

	LOG_MSG("X");

	return 0;
}
/* ------------------------------------------------------------------------------ */
static int SYSRAM_Suspend(
	struct platform_device *pDev,
	pm_message_t            Mesg)
{
	LOG_MSG("");
	return 0;
}
/* ------------------------------------------------------------------------------ */
static int SYSRAM_Resume(struct platform_device *pDev)
{
	LOG_MSG("");
	return 0;
}
/* ------------------------------------------------------------------------------ */
static struct platform_driver SysramPlatformDriver = {
	.probe     = SYSRAM_Probe,
	.remove    = SYSRAM_Remove,
	.suspend   = SYSRAM_Suspend,
	.resume    = SYSRAM_Resume,
	.driver    = {
		.name  = SYSRAM_DEV_NAME,
		.owner = THIS_MODULE,
	}
};
/* ------------------------------------------------------------------------------ */

/*ssize_t (*read) (struct file *, char __user *, size_t, loff_t *)*/

static ssize_t SYSRAM_DumpLayoutToProc(
	struct file *pFile,
	char *pStart,
	size_t Off,
	loff_t *Count)
{
	LOG_MSG("SYSRAM_DumpLayoutToProc: Not implement");
	return 0;
}
/* ------------------------------------------------------------------------------ */

/*ssize_t (*read) (struct file *, char __user *, size_t, loff_t *)*/

static ssize_t SYSRAM_ReadFlag(
	struct file *pFile,
	char *pStart,
	size_t  Off,
	loff_t *Count)
{
	LOG_MSG("SYSRAM_ReadFlag: Not implement");
	return 0;
}
/* ------------------------------------------------------------------------------ */

/*ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *)*/

static ssize_t SYSRAM_WriteFlag(
	struct file *pFile,
	const char *pBuffer,
	size_t Count,
	loff_t *pData)
{
	LOG_MSG("SYSRAM_WriteFlag: Not implement");
	return 0;
}
/*******************************************************************************
*
********************************************************************************/
static const struct file_operations fsysram_proc_fops = {
	.read = SYSRAM_DumpLayoutToProc,
	.write = NULL,
};
static const struct file_operations fsysram_flag_proc_fops = {
	.read = SYSRAM_ReadFlag,
	.write = SYSRAM_WriteFlag,
};
/* ----------------------------------------------------------------------------- */
static int __init SYSRAM_Init(void)
{
#if 0
	struct proc_dir_entry *pEntry;
#endif

	LOG_MSG("E");

	if (platform_driver_register(&SysramPlatformDriver)) {
		LOG_PR_ERR("failed to register sysram driver");
		return -ENODEV;
	}

	/* FIX-ME: linux-3.10 procfs API changed */
#if 1
	proc_create("sysram", 0, NULL, &fsysram_proc_fops);
	proc_create("sysram_flag", 0, NULL, &fsysram_flag_proc_fops);
#else
	pEntry = create_proc_entry("sysram", 0, NULL);
	if (pEntry) {
		pEntry->read_proc = SYSRAM_DumpLayoutToProc;
		pEntry->write_proc = NULL;
	} else{
		LOG_MSG("add /proc/sysram entry fail.");
	}

	pEntry = create_proc_entry("sysram_flag", 0, NULL);
	if (pEntry) {
		pEntry->read_proc = SYSRAM_ReadFlag;
		pEntry->write_proc = SYSRAM_WriteFlag;
	} else{
		LOG_MSG("add /proc/sysram_flag entry fail");
	}
#endif
	LOG_MSG("X");

	return 0;
}
/* ------------------------------------------------------------------------------ */
static void __exit SYSRAM_Exit(void)
{
	LOG_MSG("E");
	platform_driver_unregister(&SysramPlatformDriver);
	LOG_MSG("X");
}
/* ------------------------------------------------------------------------------ */
module_init(SYSRAM_Init);
module_exit(SYSRAM_Exit);
MODULE_DESCRIPTION("Camera sysram driver");
MODULE_AUTHOR("Marx <marx.chiu@mediatek.com>");
MODULE_LICENSE("GPL");
/* ------------------------------------------------------------------------------ */
