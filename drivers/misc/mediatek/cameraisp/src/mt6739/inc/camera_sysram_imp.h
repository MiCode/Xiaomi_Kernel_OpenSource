/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef CAMERA_SYSRAM_IMP_H
#define CAMERA_SYSRAM_IMP_H
/* ----------------------------------------------------------------------------- */
#define MTRUE               true
#define MFALSE              false
/* ----------------------------------------------------------------------------- */
#define MyTag "SYSRAM"

#define	LOG_MSG(format,	args...)           pr_debug(MyTag format, ##args)
#define LOG_PR_WARN(format, args...)       pr_warn(MyTag format,  ##args)
#define LOG_PR_ERR(format, args...)        pr_err(MyTag format,  ##args)
#define LOG_DMP(format, args...)           pr_debug(MyTag format,  ##args)
/* ----------------------------------------------------------------------------- */
#define SYSRAM_DEBUG_DEFAULT    (0xFFFFFFFF)
#define SYSRAM_JIFFIES_MAX      (0xFFFFFFFF)
#define SYSRAM_PROC_NAME        "Default"
/* ----------------------------------------------------------------------------- */
/* originally define in mt_reg_base.h                                            */
#define SYSRAM_BASE (0xF9000000)
/* ----------------------------------------------------------------------------- */
#define SYSRAM_BASE_PHY_ADDR    ((SYSRAM_BASE&0x0FFFFFFF)|0x10000000)
#define SYSRAM_BASE_SIZE        (81920)	/* 32K+48K */
#define SYSRAM_BASE_ADDR_BANK_0 (SYSRAM_BASE_PHY_ADDR)
#define SYSRAM_BASE_SIZE_BANK_0 (SYSRAM_BASE_SIZE)
/*  */
#define SYSRAM_USER_SIZE_VIDO   (SYSRAM_BASE_SIZE) /* (78408)//(74496) // Always allocate max SYSRAM size*/
							/*because there is no other user. //78408: Max size*/
							/*used when format is RGB565. */
#define SYSRAM_USER_SIZE_GDMA   (46080)
#define SYSRAM_USER_SIZE_SW_FD  (0)	/* TBD */
/*  */
#define SYSRAM_MEM_NODE_AMOUNT_PER_POOL (SYSRAM_USER_AMOUNT*2 + 2)
/* ----------------------------------------------------------------------------- */
struct SYSRAM_USER_STRUCT {
	pid_t pid;		/* thread id */
	pid_t tgid;		/* process id */
	char ProcName[TASK_COMM_LEN];	/* executable name */
	unsigned long long Time64;
	unsigned int TimeS;
	unsigned int TimeUS;
};
/*  */
struct SYSRAM_STRUCT {
	spinlock_t SpinLock;
	unsigned int TotalUserCount;
	unsigned int AllocatedTbl;
	unsigned int AllocatedSize[SYSRAM_USER_AMOUNT];
	struct SYSRAM_USER_STRUCT UserInfo[SYSRAM_USER_AMOUNT];
	wait_queue_head_t WaitQueueHead;
	bool EnableClk;
	unsigned int DebugFlag;
	dev_t DevNo;
	struct cdev *pCharDrv;
	struct class *pClass;
};
/*  */
struct SYSRAM_PROC_STRUCT {
	pid_t Pid;
	pid_t Tgid;
	char ProcName[TASK_COMM_LEN];
	unsigned int Table;
	unsigned long long Time64;
	unsigned int TimeS;
	unsigned int TimeUS;
};

/*  */
enum SYSRAM_MEM_BANK_ENUM {
	SYSRAM_MEM_BANK_0,
	SYSRAM_MEM_BANK_AMOUNT,
	SYSRAM_MEM_BANK_BAD
};
/*  */
struct SYSRAM_MEM_NODE_STRUCT {
	enum SYSRAM_USER_ENUM User;
	unsigned int Offset;
	unsigned int Length;
	unsigned int Index;
	struct SYSRAM_MEM_NODE_STRUCT *pNext;
	struct SYSRAM_MEM_NODE_STRUCT *pPrev;
};
/*  */
struct SYSRAM_MEM_POOL_STRUCT {
	struct SYSRAM_MEM_NODE_STRUCT * const pMemNode;
	unsigned int const UserAmount;
	unsigned int const Addr;
	unsigned int const Size;
	unsigned int IndexTbl;
	unsigned int UserCount;
};
/* ------------------------------------------------------------------------------ */
static struct SYSRAM_MEM_NODE_STRUCT SysramMemNodeBank0Tbl[SYSRAM_MEM_NODE_AMOUNT_PER_POOL];
static struct SYSRAM_MEM_POOL_STRUCT SysramMemPoolInfo[SYSRAM_MEM_BANK_AMOUNT] = {

	[SYSRAM_MEM_BANK_0] = {

			       .pMemNode = &SysramMemNodeBank0Tbl[0],
			       .UserAmount = SYSRAM_MEM_NODE_AMOUNT_PER_POOL,
			       .Addr = SYSRAM_BASE_ADDR_BANK_0,
			       .Size = SYSRAM_BASE_SIZE_BANK_0,
			       .IndexTbl = (~0x1),
			       .UserCount = 0,
			       }
};

/*  */
static inline struct SYSRAM_MEM_POOL_STRUCT *SYSRAM_GetMemPoolInfo(enum SYSRAM_MEM_BANK_ENUM const MemBankNo)
{
	if (MemBankNo < SYSRAM_MEM_BANK_AMOUNT)
		return &SysramMemPoolInfo[MemBankNo];
	/*  */
	return NULL;
}

/*  */
enum {
	SysramMemBank0UserMask = (1 << SYSRAM_USER_VIDO)
	    | (1 << SYSRAM_USER_GDMA)
	    | (1 << SYSRAM_USER_SW_FD)
	    ,
	SysramLogUserMask = (1 << SYSRAM_USER_VIDO)
	    | (1 << SYSRAM_USER_GDMA)
	    | (1 << SYSRAM_USER_SW_FD)
};
/*  */
static enum SYSRAM_MEM_BANK_ENUM SYSRAM_GetMemBankNo(enum SYSRAM_USER_ENUM const User)
{
	unsigned int const UserMask = (1 << User);
	/*  */
	if (UserMask & SysramMemBank0UserMask)
		return SYSRAM_MEM_BANK_0;
	/*  */
	return SYSRAM_MEM_BANK_BAD;
}

/*  */
static char const *const SysramUserName[SYSRAM_USER_AMOUNT] = {

	[SYSRAM_USER_VIDO] = "VIDO",
	[SYSRAM_USER_GDMA] = "GDMA",
	[SYSRAM_USER_SW_FD] = "SW FD"
};

/*  */
static unsigned int const SysramUserSize[SYSRAM_USER_AMOUNT] = {

	[SYSRAM_USER_VIDO] = (3 + SYSRAM_USER_SIZE_VIDO) / 4 * 4,
	[SYSRAM_USER_GDMA] = (3 + SYSRAM_USER_SIZE_GDMA) / 4 * 4,
	[SYSRAM_USER_SW_FD] = (3 + SYSRAM_USER_SIZE_SW_FD) / 4 * 4
};

/* ------------------------------------------------------------------------------ */
#endif
