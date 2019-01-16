#ifndef CAMERA_SYSRAM_IMP_H
#define CAMERA_SYSRAM_IMP_H
//-----------------------------------------------------------------------------
typedef unsigned long long  MUINT64;
typedef long long           MINT64;
typedef unsigned long       MUINT32;
typedef long                MINT32;
typedef unsigned char       MUINT8;
typedef char                MINT8;
typedef bool                MBOOL;
#define MTRUE               true
#define MFALSE              false
//-----------------------------------------------------------------------------
#define LOG_TAG "SYSRAM"
#define LOG_MSG(fmt, arg...)    xlog_printk(ANDROID_LOG_VERBOSE, LOG_TAG, "[%s]"          fmt "\r\n", __FUNCTION__,           ##arg)
#define LOG_WRN(fmt, arg...)    xlog_printk(ANDROID_LOG_VERBOSE, LOG_TAG, "[%s]WRN(%5d):" fmt "\r\n", __FUNCTION__, __LINE__, ##arg)
#define LOG_ERR(fmt, arg...)    xlog_printk(ANDROID_LOG_ERROR,   LOG_TAG, "[%s]ERR(%5d):" fmt "\r\n", __FUNCTION__, __LINE__, ##arg)
#define LOG_DMP(fmt, arg...)    xlog_printk(ANDROID_LOG_ERROR,   LOG_TAG, ""              fmt,                                ##arg)
//-----------------------------------------------------------------------------
#define SYSRAM_DEBUG_DEFAULT    (0xFFFFFFFF)
#define SYSRAM_JIFFIES_MAX      (0xFFFFFFFF)
#define SYSRAM_PROC_NAME        "Default"
//-----------------------------------------------------------------------------
#define SYSRAM_BASE_PHY_ADDR    ((SYSRAM_BASE&0x0FFFFFFF)|0x10000000)
#define SYSRAM_BASE_SIZE        (81920) //32K+48K
#define SYSRAM_BASE_ADDR_BANK_0 (SYSRAM_BASE_PHY_ADDR)
#define SYSRAM_BASE_SIZE_BANK_0 (SYSRAM_BASE_SIZE)
//
#define SYSRAM_USER_SIZE_VIDO   (SYSRAM_BASE_SIZE)//(78408)//(74496)    // Always allocate max SYSRAM size because there is no other user. //78408: Max size used when format is RGB565.
#define SYSRAM_USER_SIZE_GDMA   (46080)
#define SYSRAM_USER_SIZE_SW_FD  (0) //TBD
//
#define SYSRAM_MEM_NODE_AMOUNT_PER_POOL (SYSRAM_USER_AMOUNT*2 + 2)
//-----------------------------------------------------------------------------
typedef struct
{
    pid_t   pid;                    // thread id
    pid_t   tgid;                   // process id
    char    ProcName[TASK_COMM_LEN];    // executable name
    MUINT64 Time64;
    MUINT32 TimeS;
    MUINT32 TimeUS;
}SYSRAM_USER_STRUCT;
//
typedef struct
{
    spinlock_t              SpinLock;
    MUINT32                 TotalUserCount;
    MUINT32                 AllocatedTbl;
    MUINT32                 AllocatedSize[SYSRAM_USER_AMOUNT];
    SYSRAM_USER_STRUCT      UserInfo[SYSRAM_USER_AMOUNT];
    wait_queue_head_t       WaitQueueHead;
    MBOOL                   EnableClk;
    MUINT32                 DebugFlag;
    dev_t                   DevNo;
    struct cdev*            pCharDrv;
    struct class*           pClass;
}SYSRAM_STRUCT;
//
typedef struct
{
    pid_t   Pid;
    pid_t   Tgid;
    char    ProcName[TASK_COMM_LEN];
    MUINT32 Table;
    MUINT64 Time64;
    MUINT32 TimeS;
    MUINT32 TimeUS;
}SYSRAM_PROC_STRUCT;

//
typedef enum 
{
    SYSRAM_MEM_BANK_0,
    SYSRAM_MEM_BANK_AMOUNT,
    SYSRAM_MEM_BANK_BAD
}SYSRAM_MEM_BANK_ENUM;
//
typedef struct SYSRAM_MEM_NODE
{
    SYSRAM_USER_ENUM        User;
    MUINT32                 Offset;
    MUINT32                 Length;
    MUINT32                 Index;
    struct SYSRAM_MEM_NODE* pNext;
    struct SYSRAM_MEM_NODE* pPrev;
}SYSRAM_MEM_NODE_STRUCT;
//
typedef struct
{
    SYSRAM_MEM_NODE_STRUCT* const   pMemNode;
    MUINT32 const                   UserAmount;
    MUINT32 const                   Addr;
    MUINT32 const                   Size;
    MUINT32                         IndexTbl;
    MUINT32                         UserCount;
}SYSRAM_MEM_POOL_STRUCT;
//------------------------------------------------------------------------------
static SYSRAM_MEM_NODE_STRUCT SysramMemNodeBank0Tbl[SYSRAM_MEM_NODE_AMOUNT_PER_POOL];
static SYSRAM_MEM_POOL_STRUCT SysramMemPoolInfo[SYSRAM_MEM_BANK_AMOUNT] = 
{
    [SYSRAM_MEM_BANK_0] = 
    {
        .pMemNode   = &SysramMemNodeBank0Tbl[0], 
        .UserAmount = SYSRAM_MEM_NODE_AMOUNT_PER_POOL, 
        .Addr       = SYSRAM_BASE_ADDR_BANK_0, 
        .Size       = SYSRAM_BASE_SIZE_BANK_0, 
        .IndexTbl   = (~0x1), 
        .UserCount  = 0, 
    }
};
//
static inline SYSRAM_MEM_POOL_STRUCT* SYSRAM_GetMemPoolInfo(SYSRAM_MEM_BANK_ENUM const MemBankNo)
{
    if(SYSRAM_MEM_BANK_AMOUNT > MemBankNo)
    {
        return &SysramMemPoolInfo[MemBankNo];
    }
    return  NULL;
}
//
enum
{
    SysramMemBank0UserMask =
         (1<<SYSRAM_USER_VIDO)
        |(1<<SYSRAM_USER_GDMA)
        |(1<<SYSRAM_USER_SW_FD)
        , 
    SysramLogUserMask =
         (1<<SYSRAM_USER_VIDO)
        |(1<<SYSRAM_USER_GDMA)
        |(1<<SYSRAM_USER_SW_FD)
};
//
static SYSRAM_MEM_BANK_ENUM SYSRAM_GetMemBankNo(SYSRAM_USER_ENUM const User)
{
    MUINT32 const UserMask = (1<<User);
    //
    if(UserMask & SysramMemBank0UserMask)
    {
        return  SYSRAM_MEM_BANK_0;
    }
    //
    return  SYSRAM_MEM_BANK_BAD;
}
//
static char const*const SysramUserName[SYSRAM_USER_AMOUNT] = 
{
    [SYSRAM_USER_VIDO]  = "VIDO", 
    [SYSRAM_USER_GDMA]  = "GDMA",
    [SYSRAM_USER_SW_FD] = "SW FD"
};
//
static MUINT32 const SysramUserSize[SYSRAM_USER_AMOUNT] = 
{
    [SYSRAM_USER_VIDO]  = (3 + SYSRAM_USER_SIZE_VIDO) / 4 * 4,
    [SYSRAM_USER_GDMA]  = (3 + SYSRAM_USER_SIZE_GDMA) / 4 * 4,
    [SYSRAM_USER_SW_FD] = (3 + SYSRAM_USER_SIZE_SW_FD) / 4 * 4
};
//------------------------------------------------------------------------------
#endif

