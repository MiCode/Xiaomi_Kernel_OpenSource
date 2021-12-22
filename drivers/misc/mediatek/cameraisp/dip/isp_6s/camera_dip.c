// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */
/* MET: define to enable MET*/
//#include <error_test.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h> /* proc file use */
#include <linux/slab.h>
#include <linux/spinlock.h>
/* #include <linux/io.h> */
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/seq_file.h>
#include <linux/dma-mapping.h>

/*#include <mach/irqs.h>*/
/* For clock mgr APIS. enable_clock()/disable_clock(). */
/*#include <mach/mt_clkmgr.h>*/
#include <mt-plat/sync_write.h> /* For reg_sync_writel(). */
/* For spm_enable_sodi()/spm_disable_sodi(). */
/* #include <mach/mt_spm_idle.h> */

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#ifdef CONFIG_MTK_IOMMU_V2
#include <mach/mt_iommu.h>
#include "mach/pseudo_m4u.h"
#else
#ifdef CONFIG_MTK_M4U
#include <m4u.h>
#endif
#endif

#ifndef EP_CODE_MARK_CMDQ
#include <mdp_cmdq_helper_ext.h>
#endif

#include <cmdq-util.h>
#include <smi_public.h>

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

/*for SMI BW debug log*/
/*#include"../../../smi/smi_debug.h" YWclose*/

/*for kernel log count*/
#define _K_LOG_ADJUST (0)

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/*  */
/* #include "smi_common.h" */

#ifdef CONFIG_PM_SLEEP
#include <linux/pm_wakeup.h>
#endif

#ifdef CONFIG_OF
#include <linux/of_platform.h>  /* for device tree */
#include <linux/of_irq.h>       /* for device tree */
#include <linux/of_address.h>   /* for device tree */
#endif

#if defined(DIP_MET_READY)
/*MET:met mmsys profile*/
#include <mt-plat/met_drv.h>
#endif


#define CAMSV_DBG
#ifdef CAMSV_DBG
#define CAM_TAG "CAM:"
#define CAMSV_TAG "SV1:"
#define CAMSV2_TAG "SV2:"
#else
#define CAMSV_TAG ""
#define CAMSV2_TAG ""
#define CAM_TAG ""
#endif

#include "camera_dip.h"

/*  */
#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif

#define DIP_DEV_NAME        "camera-dip"

/*for early if load dont need to use camera*/
#define DUMMY_INT           (0)

/* Clkmgr is not ready in early porting, en/disable clock  by hardcode */
/*#define EP_NO_CLKMGR*/
#ifdef CONFIG_FPGA_EARLY_PORTING
#define EP_NO_CLKMGR
#endif

#define DIP_BOTTOMHALF_WORKQ		(1)

#if (DIP_BOTTOMHALF_WORKQ == 1)
#include <linux/workqueue.h>
#endif

/* ----------------------------------------------------------- */

#define MyTag "[DIP]"
#define IRQTag "KEEPER"

#define LOG_VRB(format, args...) \
pr_debug(MyTag "[%s] " format, __func__, ##args)

#define DIP_DEBUG
#ifdef DIP_DEBUG
#define LOG_DBG(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#else
#define LOG_DBG(format, args...)
#endif

#define LOG_INF(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#define LOG_NOTICE(format, args...) \
pr_notice(MyTag "[%s] " format, __func__, ##args)
#define LOG_WRN(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#define LOG_ERR(format, args...) \
pr_info(MyTag "[%s] " format, __func__, ##args)
#define LOG_AST(format, args...) \
pr_debug(MyTag "[%s] " format, __func__, ##args)

bool g_DIP_PMState;
/**************************************************************
 *
 **************************************************************/
/* #define DIP_WR32(addr, data)    iowrite32(data, addr) */
#define DIP_WR32(addr, data)    mt_reg_sync_writel(data, addr)
#define DIP_RD32(addr)                  ioread32((void *)addr)
/**************************************************************
 *
 **************************************************************/
/* dynamic log level */
#define DIP_DBG_INT                 (0x00000001)
#define DIP_DBG_READ_REG            (0x00000004)
#define DIP_DBG_WRITE_REG           (0x00000008)
#define DIP_DBG_CLK                 (0x00000010)
#define DIP_DBG_TASKLET             (0x00000020)
#define DIP_DBG_SCHEDULE_WORK       (0x00000040)
#define DIP_DBG_BUF_WRITE           (0x00000080)
#define DIP_DBG_BUF_CTRL            (0x00000100)
#define DIP_DBG_REF_CNT_CTRL        (0x00000200)
#define DIP_DBG_INT_2               (0x00000400)
#define DIP_DBG_INT_3               (0x00000800)
#define DIP_DBG_HW_DON              (0x00001000)
#define DIP_DBG_ION_CTRL            (0x00002000)
/**************************************************************
 *
 **************************************************************/
#define DUMP_GCE_TPIPE  0

static irqreturn_t DIP_Irq_DIP_A(signed int  Irq, void *DeviceId);


typedef irqreturn_t (*IRQ_CB)(signed int, void *);

struct ISR_TABLE {
	IRQ_CB          isr_fp;
	unsigned int    int_number;
	char            device_name[16];
};

struct Dip_Init_Array {
	unsigned int    ofset;
	unsigned int    val;
};

#if (MTK_DIP_COUNT == 2)
#define DIP_COUNT_IS_2
#endif

#ifndef CONFIG_OF
const struct ISR_TABLE DIP_IRQ_CB_TBL[DIP_IRQ_TYPE_AMOUNT] = {
	{NULL,              0,    "DIP_A"}
};

#else
/* int number is got from kernel api */

const struct ISR_TABLE DIP_IRQ_CB_TBL[DIP_IRQ_TYPE_AMOUNT] = {
	{DIP_Irq_DIP_A,     0,  "dip"}
};

/*
 * Note!!! The order and member of .compatible
 # must be the same with that in
 *  "DIP_DEV_NODE_ENUM" in camera_dip.h
 */
static const struct of_device_id dip_of_ids[] = {
	{ .compatible = "mediatek,imgsys", },
	{ .compatible = "mediatek,dip1", },
	{ .compatible = "mediatek,mssdl", },
	{ .compatible = "mediatek,msfdl", },
	{ .compatible = "mediatek,imgsys2", },
#ifdef DIP_COUNT_IS_2
	{ .compatible = "mediatek,dip2", },
#endif
	{}
};

#endif
#define DIP_INIT_ARRAY_COUNT  142
const struct Dip_Init_Array DIP_INIT_ARY[DIP_INIT_ARRAY_COUNT] = {
	{0x1110, 0xffffffff},
	{0x1114, 0xffffffff},
	{0x1118, 0xffffffff},
	{0x111C, 0xffffffff},
	{0x1120, 0xffffffff},
	{0x1124, 0xffffffff},
	{0x1128, 0xffffffff},
	{0x112C, 0x1},
	{0x10A0, 0x80000000},
	{0x10B0, 0x0},
	{0x10C0, 0x0},
	{0x10D0, 0x0},
	{0x10E0, 0x0},
	{0x10F0, 0x0},
	{0x1204, 0x11},
	{0x121C, 0x11},
	{0x1228, 0x11},
	{0x1234, 0x11},
	{0x1240, 0x11},
	{0x124C, 0x11},
	{0x1258, 0x11},
	{0x1264, 0x11},
	{0x1270, 0x11},
	{0x127C, 0x11},
	{0x1288, 0x11},
	{0x1294, 0x11},
	{0x12A0, 0x11},
	{0x12AC, 0x11},
	{0x12B8, 0x11},
	{0x12C4, 0x11},
	{0x12D0, 0x11},
	{0x12DC, 0x11},
	{0x12E8, 0x11},
	{0x1210, 0x4b8},
	{0x1224, 0x4b8},
	{0x1230, 0x4b8},
	{0x123C, 0x4b8},
	{0x1248, 0x4b8},
	{0x1254, 0x4b8},
	{0x1260, 0x4b8},
	{0x126C, 0x4b8},
	{0x1278, 0x4b8},
	{0x1284, 0x4b8},
	{0x1290, 0x4b8},
	{0x129C, 0x4b8},
	{0x12A8, 0x4b8},
	{0x12B4, 0x4b8},
	{0x12C0, 0x4b8},
	{0x12CC, 0x4b8},
	{0x12D8, 0x4b8},
	{0x12E4, 0x4b8},
	{0x12F0, 0x4b8},
	{0x218, 0x80000100},
	{0x21C, 0x01000100},
	{0x220, 0x00AB008B},
	{0x248, 0x80000040},
	{0x24C, 0x00400040},
	{0x250, 0x002B0023},
	{0x278, 0x80000040},
	{0x27C, 0x00400040},
	{0x280, 0x002B0023},
	{0x2A8, 0x80000080},
	{0x2AC, 0x00800080},
	{0x2B0, 0x00550045},
	{0x318, 0x80000080},
	{0x31C, 0x00800080},
	{0x320, 0x00550045},
	{0x348, 0x80000080},
	{0x34C, 0x00800080},
	{0x350, 0x00550045},
	{0x378, 0x80000080},
	{0x37C, 0x00800080},
	{0x380, 0x00550045},
	{0x3E8, 0x80000040},
	{0x3EC, 0x00400040},
	{0x3F0, 0x002B0023},
	{0x418, 0x80000040},
	{0x41C, 0x00400040},
	{0x420, 0x002B0023},
	{0x488, 0x80000100},
	{0x48C, 0x01000100},
	{0x490, 0x00500050},
	{0x4B8, 0x800000C0},
	{0x4BC, 0x00C000C0},
	{0x4C0, 0x00400040},
	{0x4E8, 0x80000080},
	{0x4EC, 0x00800080},
	{0x4F0, 0x00550045},
	{0x518, 0x80000080},
	{0x51C, 0x00800080},
	{0x520, 0x00550045},
	{0x588, 0x80000040},
	{0x58C, 0x00400040},
	{0x590, 0x002B0023},
	{0x5B8, 0x80000040},
	{0x5BC, 0x00400040},
	{0x5C0, 0x002B0023},
	{0x628, 0x80000040},
	{0x62C, 0x00400040},
	{0x630, 0x002B0023},
	{0x658, 0x80000040},
	{0x65C, 0x00400040},
	{0x660, 0x002B0023},
	{0x6C8, 0x80000080},
	{0x6CC, 0x00800080},
	{0x6D0, 0x00550045},
	{0x738, 0x80000040},
	{0x73C, 0x00400040},
	{0x740, 0x002B0023},
	{0x7A8, 0x80000040},
	{0x7AC, 0x00400040},
	{0x7B0, 0x002B0023},
	{0x818, 0x80000080},
	{0x81C, 0x00800080},
	{0x820, 0x00550045},
	{0x848, 0x80000040},
	{0x84C, 0x00400040},
	{0x850, 0x002B0023},
	{0x878, 0x80000040},
	{0x87C, 0x00400040},
	{0x880, 0x002B0023},
	{0x8A8, 0x80000080},
	{0x8AC, 0x00800080},
	{0x8B0, 0x00550045},
	{0x918, 0x80000040},
	{0x91C, 0x00400040},
	{0x920, 0x002B0023},
	{0x988, 0x80000040},
	{0x98C, 0x00400040},
	{0x990, 0x002B0023},
	{0x9F8, 0x80000080},
	{0x9FC, 0x00800080},
	{0xA00, 0x00550045},
	{0xA28, 0x80000040},
	{0xA2C, 0x00400040},
	{0xA30, 0x002B0023},
	{0xA98, 0x80000080},
	{0xA9C, 0x00800080},
	{0xAA0, 0x00550045},
	{0xAC8, 0x80000080},
	{0xACC, 0x00800080},
	{0xAD0, 0x00550045}
};

/**************************************************************
 *
 **************************************************************/
typedef void (*tasklet_cb)(unsigned long);
struct Tasklet_table {
	tasklet_cb tkt_cb;
	struct tasklet_struct  *pIsp_tkt;
};

struct tasklet_struct DIP_tkt[DIP_IRQ_TYPE_AMOUNT];

static struct Tasklet_table dip_tasklet[DIP_IRQ_TYPE_AMOUNT] = {
	{NULL,                  &DIP_tkt[DIP_IRQ_TYPE_INT_DIP_A_ST]},
};

#if (DIP_BOTTOMHALF_WORKQ == 1)
struct IspWorkqueTable {
	enum DIP_IRQ_TYPE_ENUM	module;
	struct work_struct  dip_bh_work;
};

static void DIP_BH_Workqueue(struct work_struct *pWork);

static struct IspWorkqueTable dip_workque[DIP_IRQ_TYPE_AMOUNT] = {
	{DIP_IRQ_TYPE_INT_DIP_A_ST},
};
#endif

static DEFINE_MUTEX(gDipMutex);

#ifdef CONFIG_OF

#ifndef CONFIG_MTK_CLKMGR /*CCF*/
#include <linux/clk.h>
struct DIP_CLK_STRUCT {
	struct clk *DIP_IMG_LARB9;
	struct clk *DIP_IMG_DIP;
	struct clk *DIP_IMG_DIP_MSS;
	struct clk *DIP_IMG_MFB_DIP;
#if ((MTK_DIP_COUNT == 2) || (MTK_MSF_OFFSET == 1))
	struct clk *DIP_IMG_LARB11;
#endif
#if (MTK_DIP_COUNT == 2)
	struct clk *DIP_IMG_DIP2;
#endif
};
struct DIP_CLK_STRUCT dip_clk;
#endif


#ifdef CONFIG_OF
struct dip_device {
	void __iomem *regs;
	struct device *dev;
	int irq;
};

static struct dip_device *dip_devs;
static int nr_dip_devs;
#endif


#ifndef CONFIG_FPGA_EARLY_PORTING
/*#define AEE_DUMP_BY_USING_ION_MEMORY MH close for IOMMU*/
#endif


#define AEE_DUMP_REDUCE_MEMORY
#ifdef AEE_DUMP_REDUCE_MEMORY
/* ion */

#ifdef AEE_DUMP_BY_USING_ION_MEMORY
#include <ion.h>
#include <mtk/ion_drv.h>
#include <mtk/mtk_ion.h>

struct dip_imem_memory {
	void *handle;
	int ion_fd;
	uint64_t va;
	uint32_t pa;
	uint32_t length;
};

static struct ion_client *dip_p2_ion_client;
static struct dip_imem_memory g_dip_p2_imem_buf;
#endif
static bool g_bIonBufferAllocated;
static unsigned int *g_pPhyDIPBuffer;
static unsigned int *g_pPhyMFBBuffer;
static unsigned int *g_pPhyMSSBuffer;
/* Kernel Warning */
static unsigned int *g_pKWTpipeBuffer;
static unsigned int *g_pKWCmdqBuffer;
static unsigned int *g_pKWVirDIPBuffer;
/* Navtive Exception */
static unsigned int *g_pTuningBuffer;
static unsigned int *g_pTpipeBuffer;
static unsigned int *g_pVirDIPBuffer;
static unsigned int *g_pCmdqBuffer;
#endif
static bool g_bUserBufIsReady = MFALSE;
static unsigned int DumpBufferField;
static bool g_bDumpPhyDIPBuf = MFALSE;
static unsigned int g_tdriaddr = 0xffffffff;
static unsigned int g_cmdqaddr = 0xffffffff;
static struct DIP_GET_DUMP_INFO_STRUCT g_dumpInfo =	{
	0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
static struct DIP_MEM_INFO_STRUCT g_TpipeBaseAddrInfo = {
	0x0, 0x0, NULL, 0x0};
static struct DIP_MEM_INFO_STRUCT g_CmdqBaseAddrInfo = {
	0x0, 0x0, NULL, 0x0};
static unsigned int m_CurrentPPB;

#ifdef CONFIG_PM_SLEEP
struct wakeup_source *dip_wake_lock;
struct wakeup_source *isp_mdp_wake_lock;
#endif

static int g_bWaitLock;
static unsigned int g_dip1sterr = DIP_GCE_EVENT_NONE;

/* Get HW modules' base address from device nodes */
#define DIP_IMGSYS_CONFIG_BASE      (dip_devs[DIP_IMGSYS_CONFIG_IDX].regs)
#define DIP_A_BASE                  (dip_devs[DIP_DIP_A_IDX].regs)
#define DIP_A_ADDR                  0x15022000
#define MSS_BASE                    (dip_devs[DIP_MSS_IDX].regs)
#define MSF_BASE                    (dip_devs[DIP_MSF_IDX].regs)
#define DIP_IMGSYS2_CONFIG_BASE     (dip_devs[DIP_IMGSYS2_CONFIG_IDX].regs)
#if (MTK_DIP_COUNT == 2)
#define DIP_B_BASE                  (dip_devs[DIP_DIP_B_IDX].regs)
#endif

#else
#define DIP_ADDR                        (IMGSYS_BASE + 0x4000)
#define DIP_IMGSYS_BASE                 IMGSYS_BASE
#define DIP_ADDR_CAMINF                 IMGSYS_BASE
#define DIP_MIPI_ANA_ADDR               0x10217000
#define DIP_GPIO_ADDR                   GPIO_BASE
#if (MTK_DIP_COUNT == 2)
#define DIP_B_ADDR                       (IMGSYS2_BASE + 0x4000)
#endif
#define DIP_IMGSYS2_BASE                 IMGSYS2_BASE
#endif
/* TODO: Remove end, Jessy */


#define DIP_REG_SW_CTL_RST_CAM_P1       (1)
#define DIP_REG_SW_CTL_RST_CAM_P2       (2)
#define DIP_REG_SW_CTL_RST_CAMSV        (3)
#define DIP_REG_SW_CTL_RST_CAMSV2       (4)

struct S_START_T {
	unsigned int sec;
	unsigned int usec;
};

struct DIP_DEV_NODE_MAPPING {
	enum DIP_DEV_NODE_ENUM idx;
	unsigned int region;
};

/* QQ, remove later */
/* record remain node count(success/fail) */
/* excludes head when enque/deque control */
static unsigned int g_regScen = 0xa5a5a5a5; /* remove later */


static /*volatile*/ wait_queue_head_t P2WaitQueueHead_WaitDeque;
static /*volatile*/ wait_queue_head_t P2WaitQueueHead_WaitFrame;
static /*volatile*/ wait_queue_head_t P2WaitQueueHead_WaitFrameEQDforDQ;
static spinlock_t      SpinLock_P2FrameList;
#define _MAX_SUPPORT_P2_FRAME_NUM_ 512
#define _MAX_SUPPORT_P2_BURSTQ_NUM_ 8
#define _MAX_SUPPORT_P2_PACKAGE_NUM_ \
(_MAX_SUPPORT_P2_FRAME_NUM_/_MAX_SUPPORT_P2_BURSTQ_NUM_)
struct DIP_P2_BUFQUE_IDX_STRUCT {
	signed int start; /* starting index for frames in the ring list */
	signed int curr; /* current index for running frame in the ring list */
	signed int end; /* ending index for frames in the ring list */
};

struct DIP_P2_FRAME_UNIT_STRUCT {
	unsigned int               processID; /* caller process ID */
	unsigned int               callerID; /* caller thread ID */
	unsigned int               cqMask;  /*Judge cq combination*/

	enum DIP_P2_BUF_STATE_ENUM  bufSts; /* buffer status */
};

static struct DIP_P2_BUFQUE_IDX_STRUCT
	P2_FrameUnit_List_Idx[DIP_P2_BUFQUE_PROPERTY_NUM];
static struct DIP_P2_FRAME_UNIT_STRUCT
	P2_FrameUnit_List[DIP_P2_BUFQUE_PROPERTY_NUM]
		[_MAX_SUPPORT_P2_FRAME_NUM_];

struct DIP_P2_FRAME_PACKAGE_STRUCT {
	unsigned int                processID;  /* caller process ID */
	unsigned int                callerID;   /* caller thread ID */
	unsigned int                dupCQIdx;
	signed int                   frameNum;
	/* number of dequed buffer no matter deque success or fail */
	signed int                   dequedNum;
};
static struct DIP_P2_BUFQUE_IDX_STRUCT
	P2_FramePack_List_Idx[DIP_P2_BUFQUE_PROPERTY_NUM];
static struct DIP_P2_FRAME_PACKAGE_STRUCT
	P2_FramePackage_List[DIP_P2_BUFQUE_PROPERTY_NUM]
		[_MAX_SUPPORT_P2_PACKAGE_NUM_];




static  spinlock_t      SpinLockRegScen;

/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32
static  spinlock_t      SpinLock_UserKey;


/**************************************************************
 *
 **************************************************************/
/* internal data */
/* pointer to the kmalloc'd area, rounded up to a page boundary */
static int *pTbl_RTBuf[DIP_IRQ_TYPE_AMOUNT];
static int Tbl_RTBuf_MMPSize[DIP_IRQ_TYPE_AMOUNT];

/* original pointer for kmalloc'd area as returned by kmalloc */
static void *pBuf_kmalloc[DIP_IRQ_TYPE_AMOUNT];


static unsigned int G_u4DipEnClkCnt;
static unsigned int g_u4DipCnt;

#ifdef CONFIG_MTK_IOMMU_V2
static int DIP_MEM_USE_VIRTUL = 1;
#endif

int DIP_pr_detect_count;

/**************************************************************
 *
 **************************************************************/
struct DIP_USER_INFO_STRUCT {
	pid_t   Pid;
	pid_t   Tid;
};

/**************************************************************
 *
 **************************************************************/
#define DIP_BUF_SIZE            (4096)
#define DIP_BUF_SIZE_WRITE      1024
#define DIP_BUF_WRITE_AMOUNT    6

enum DIP_BUF_STATUS_ENUM {
	DIP_BUF_STATUS_EMPTY,
	DIP_BUF_STATUS_HOLD,
	DIP_BUF_STATUS_READY
};

struct DIP_BUF_STRUCT {
	enum DIP_BUF_STATUS_ENUM Status;
	unsigned int                Size;
	unsigned char *pData;
};

struct DIP_BUF_INFO_STRUCT {
	struct DIP_BUF_STRUCT      Read;
	struct DIP_BUF_STRUCT      Write[DIP_BUF_WRITE_AMOUNT];
};


/**************************************************************
 *
 **************************************************************/
#define DIP_ISR_MAX_NUM 32
#define INT_ERR_WARN_TIMER_THREAS 1000
#define INT_ERR_WARN_MAX_TIME 1

struct DIP_IRQ_ERR_WAN_CNT_STRUCT {
	/* cnt for each err int # */
	unsigned int m_err_int_cnt[DIP_IRQ_TYPE_AMOUNT][DIP_ISR_MAX_NUM];
	/* cnt for each warning int # */
	unsigned int m_warn_int_cnt[DIP_IRQ_TYPE_AMOUNT][DIP_ISR_MAX_NUM];
	/* mark for err int, where its cnt > threshold */
	unsigned int m_err_int_mark[DIP_IRQ_TYPE_AMOUNT];
	/* mark for warn int, where its cnt > threshold */
	unsigned int m_warn_int_mark[DIP_IRQ_TYPE_AMOUNT];
	unsigned long m_int_usec[DIP_IRQ_TYPE_AMOUNT];
};

static signed int FirstUnusedIrqUserKey = 1;
#define USERKEY_STR_LEN 128

struct UserKeyInfo {
	char userName[USERKEY_STR_LEN];
	int userKey;
};
/* array for recording the user name for a specific user key */
static struct UserKeyInfo IrqUserKey_UserInfo[IRQ_USER_NUM_MAX];

struct DIP_IRQ_INFO_STRUCT {
	unsigned int    Status[DIP_IRQ_TYPE_AMOUNT][IRQ_USER_NUM_MAX];
	unsigned int    Mask[DIP_IRQ_TYPE_AMOUNT];
};

struct DIP_TIME_LOG_STRUCT {
	unsigned int     Vd;
	unsigned int     Expdone;
	unsigned int     WorkQueueVd;
	unsigned int     WorkQueueExpdone;
	unsigned int     TaskletVd;
	unsigned int     TaskletExpdone;
};

/**************************************************************/
#define my_get_pow_idx(value)      \
	({                                                  \
		int i = 0, cnt = 0;                         \
		for (i = 0; i < 32; i++) {                  \
			if ((value>>i) & (0x00000001)) {   \
				break;                       \
			} else {                             \
				cnt++;  \
			}                                    \
		}                                            \
		cnt;                                         \
	})


#define SUPPORT_MAX_IRQ 32
struct DIP_INFO_STRUCT {
	spinlock_t			SpinLockIspRef;
	spinlock_t			SpinLockIsp;
	spinlock_t			SpinLockIrq[DIP_IRQ_TYPE_AMOUNT];
	spinlock_t			SpinLockIrqCnt[DIP_IRQ_TYPE_AMOUNT];
	spinlock_t			SpinLockRTBC;
	spinlock_t			SpinLockClock;
	wait_queue_head_t		WaitQueueHead[DIP_IRQ_TYPE_AMOUNT];
	/* wait_queue_head_t*		WaitQHeadList; */
	wait_queue_head_t		WaitQHeadList[SUPPORT_MAX_IRQ];
	unsigned int			UserCount;
	unsigned int			DebugMask;
	signed int			IrqNum;
	struct DIP_IRQ_INFO_STRUCT	IrqInfo;
	struct DIP_IRQ_ERR_WAN_CNT_STRUCT	IrqCntInfo;
	struct DIP_TIME_LOG_STRUCT	TimeLog;
};



static struct DIP_INFO_STRUCT IspInfo;

enum _eLOG_TYPE {
	_LOG_DBG = 0,
	_LOG_INF = 1,
	_LOG_ERR = 2,
	_LOG_MAX = 3,
} eLOG_TYPE;

enum _eLOG_OP {
	_LOG_INIT = 0,
	_LOG_RST = 1,
	_LOG_ADD = 2,
	_LOG_PRT = 3,
	_LOG_GETCNT = 4,
	_LOG_OP_MAX = 5
} eLOG_OP;

#define NORMAL_STR_LEN (512)
#define ERR_PAGE 2
#define DBG_PAGE 2
#define INF_PAGE 4
/* #define SV_LOG_STR_LEN NORMAL_STR_LEN */

#define LOG_PPNUM 2
struct SV_LOG_STR {
	unsigned int _cnt[LOG_PPNUM][_LOG_MAX];
	/* char   _str[_LOG_MAX][SV_LOG_STR_LEN]; */
	char *_str[LOG_PPNUM][_LOG_MAX];
	struct S_START_T   _lastIrqTime;
};

static void *pLog_kmalloc;
static struct SV_LOG_STR gSvLog[DIP_IRQ_TYPE_AMOUNT];

/**
 *   for irq used,keep log until IRQ_LOG_PRINTER being involked,
 *   limited:
 *   each log must shorter than 512 bytes
 *   total log length in each irq/logtype can't over 1024 bytes
 */
#define IRQ_LOG_KEEPER_T(sec, usec) {\
		ktime_t time;           \
		time = ktime_get();     \
		sec = time.tv64;        \
		do_div(sec, 1000);    \
		usec = do_div(sec, 1000000); \
	}
#if 1
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...) do {\
	char *ptr; \
	char *pDes; \
	signed int avaLen; \
	unsigned int *ptr2 = &gSvLog[irq]._cnt[ppb][logT]; \
	unsigned int str_leng; \
	unsigned int i; \
	struct SV_LOG_STR *pSrc = &gSvLog[irq]; \
	if (logT == _LOG_ERR) {\
		str_leng = NORMAL_STR_LEN*ERR_PAGE; \
	} else if (logT == _LOG_DBG) {\
		str_leng = NORMAL_STR_LEN*DBG_PAGE; \
	} else if (logT == _LOG_INF) {\
		str_leng = NORMAL_STR_LEN*INF_PAGE; \
	} else {\
		str_leng = 0; \
	} \
	ptr = pDes = \
	(char *)&(gSvLog[irq]._str[ppb][logT][gSvLog[irq]._cnt[ppb][logT]]); \
	avaLen = str_leng - 1 - gSvLog[irq]._cnt[ppb][logT]; \
	if (avaLen > 1) {\
		snprintf((char *)(pDes), avaLen, "[%d.%06d]" fmt,\
		gSvLog[irq]._lastIrqTime.sec, gSvLog[irq]._lastIrqTime.usec,\
		##__VA_ARGS__);   \
	if ('\0' != gSvLog[irq]._str[ppb][logT][str_leng - 1]) {\
		LOG_ERR("log str over flow(%d)", irq); \
	} \
	while (*ptr++ != '\0') {        \
		(*ptr2)++; \
	}     \
	} else { \
		LOG_INF("(%d)(%d)log str avalible=0, print log\n", \
			 irq, logT); \
		ptr = pSrc->_str[ppb][logT]; \
	if (pSrc->_cnt[ppb][logT] != 0) {\
		if (logT == _LOG_DBG) {\
			for (i = 0; i < DBG_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';  \
					LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]); \
				} else{\
					LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]); \
					break; \
				} \
			} \
		} \
		else if (logT == _LOG_INF) {\
			for (i = 0; i < INF_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0'; \
					LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]); \
				} else{\
					LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]); \
					break; \
				} \
			} \
		} \
		else if (logT == _LOG_ERR) {\
			for (i = 0; i < ERR_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0'; \
					LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]); \
				} else{\
					LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]); \
					break; \
				} \
			} \
		} \
		else {\
			LOG_ERR("N.S.%d", logT); \
		} \
		ptr[0] = '\0'; \
		pSrc->_cnt[ppb][logT] = 0; \
		avaLen = str_leng - 1; \
		ptr = pDes = \
		(char *)&(pSrc->_str[ppb][logT][pSrc->_cnt[ppb][logT]]); \
		ptr2 = &(pSrc->_cnt[ppb][logT]); \
		snprintf((char *)(pDes), avaLen, fmt, ##__VA_ARGS__); \
		while (*ptr++ != '\0') {\
			(*ptr2)++; \
		} \
	} \
	} \
} while (0)
#else
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, args...) \
pr_debug(IRQTag fmt,  ##args)
#endif

#if 1
#define IRQ_LOG_PRINTER(irq, ppb_in, logT_in) do {\
	struct SV_LOG_STR *pSrc = &gSvLog[irq]; \
	char *ptr; \
	unsigned int i; \
	unsigned int ppb = 0; \
	unsigned int logT = 0; \
	if (ppb_in > 1) {\
		ppb = 1; \
	} else{\
		ppb = ppb_in; \
	} \
	if (logT_in > _LOG_ERR) {\
		logT = _LOG_ERR; \
	} else{\
		logT = logT_in; \
	} \
	ptr = pSrc->_str[ppb][logT]; \
	if (pSrc->_cnt[ppb][logT] != 0) {\
		if (logT == _LOG_DBG) {\
			for (i = 0; i < DBG_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0'; \
					LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]); \
				} else{\
					LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]); \
					break; \
				} \
			} \
		} \
		else if (logT == _LOG_INF) {\
			for (i = 0; i < INF_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0'; \
					LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]); \
				} else{\
					LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]); \
					break; \
				} \
			} \
		} \
		else if (logT == _LOG_ERR) {\
			for (i = 0; i < ERR_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0'; \
					LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]); \
				} else{\
					LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]); \
					break; \
				} \
			} \
		} \
		else {\
			LOG_ERR("N.S.%d", logT); \
		} \
		ptr[0] = '\0'; \
		pSrc->_cnt[ppb][logT] = 0; \
	} \
	} while (0)


#else

/*#define CAMSYS_REG_CG_CON               (DIP_CAMSYS_CONFIG_BASE + 0x0)*/
#define IMGSYS_REG_CG_CON               (DIP_IMGSYS_CONFIG_BASE + 0x0)
/*#define CAMSYS_REG_CG_SET               (DIP_CAMSYS_CONFIG_BASE + 0x4)*/
#define IMGSYS_REG_CG_SET               (DIP_IMGSYS_CONFIG_BASE + 0x4)
#define IRQ_LOG_PRINTER(irq, ppb, logT)
#endif
#define IMGSYS_REG_CG_SET               (DIP_IMGSYS_CONFIG_BASE + 0x4)
#define IMGSYS_REG_CG_CLR               (DIP_IMGSYS_CONFIG_BASE + 0x8)
#if (MTK_DIP_COUNT == 2)
#define IMGSYS2_REG_CG_SET              (DIP_IMGSYS2_CONFIG_BASE + 0x4)
#define IMGSYS2_REG_CG_CLR              (DIP_IMGSYS2_CONFIG_BASE + 0x8)
#endif
/**************************************************************
 *
 **************************************************************/
static inline unsigned int DIP_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

/**************************************************************
 *
 **************************************************************/
static inline unsigned int DIP_UsToJiffies(unsigned int Us)
{
	return (((Us / 1000) * HZ + 512) >> 10);
}

/**************************************************************
 *
 **************************************************************/
static inline unsigned int DIP_JiffiesToMs(unsigned int Jiffies)
{
	return ((Jiffies * 1000) / HZ);
}

static signed int DIP_Dump_IMGSYS_DIP_Reg(void)
{
	signed int Ret = 0;
	unsigned int DIPNo = 0, i = 0;
	unsigned int dipdmacmd = 0;
	unsigned int dbg_rgb = 0x1, dbg_yuv = 0x2;
	unsigned int dbg_sel = 0, dbg_out = 0, dd = 0;
	unsigned int smidmacmd = 0, dmaidx = 0;
	unsigned int fifodmacmd = 0;
	unsigned int cmdqdebugcmd = 0, cmdqdebugidx = 0;
	unsigned int d1a_cq_en = 0;
	void __iomem *dipRegBasAddr;

	static struct DIP_DEV_NODE_MAPPING DipDumpTL[MTK_DIP_COUNT + 1]
	= {		{DIP_DIP_A_IDX, 0x1502},
	#ifdef DIP_COUNT_IS_2
			{DIP_DIP_B_IDX, 0x1582},
	#endif
			{DIP_DEV_NODE_NUM, 0xFFFF} };

	for (DIPNo = 0; DIPNo < MTK_DIP_COUNT; DIPNo++) {
		/* DIP REG_CONFIG_BASE */
		dipRegBasAddr = dip_devs[DipDumpTL[DIPNo].idx].regs;
		cmdq_util_err("***** DIP %d *****", DIPNo);

		/*top control*/
		cmdq_util_err("dip: 0x%x2000(0x%x)-0x%x2004(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1000),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1004));
		cmdq_util_err("dip: 0x%x2010(0x%x)-0x%x2014(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1010),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1014));
		cmdq_util_err("dip: 0x%x2018(0x%x)-0x%x201C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1018),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x101C));
		cmdq_util_err("dip: 0x%x2020(0x%x)-0x%x2024(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1020),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1024));
		cmdq_util_err("dip: 0x%x2028(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1028));
		cmdq_util_err("dip: 0x%x2040(0x%x)-0x%x2044(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1040),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1044));
		cmdq_util_err("dip: 0x%x2050(0x%x)-0x%x2054(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1050),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1054));
		cmdq_util_err("dip: 0x%x2058(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1058));

		/*mdp crop1 and mdp crop2*/
		cmdq_util_err("dip: 0x%x86C0(0x%x)-0x%x86C4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x76C0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x76C4));
		cmdq_util_err("crop2: 0x%x4B80(0x%x)-0x%x4B84(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x3B80),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x3B84));

		/*imgi and tdri offset address*/
		cmdq_util_err("dip: 0x%x1104(0x%x)-0x%x1004(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0104),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0004));
		cmdq_util_err("dip: 0x%x1008(0x%x)-0x%x100C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0008),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x000C));
		/*tdr ctrl*/
		cmdq_util_err("dip: 0x%x2060(0x%x)-0x%x2064(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1060),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1064));
		cmdq_util_err("dip: 0x%x2068(0x%x)-0x%x206C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1068),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x106C));
		cmdq_util_err("dip: 0x%x2070(0x%x)-0x%x2074(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1070),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1074));
		cmdq_util_err("dip: 0x%x2078(0x%x)-0x%x207C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1078),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x107C));
		cmdq_util_err("dip: 0x%x2080(0x%x)-0x%x2084(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1080),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1084));
		cmdq_util_err("dip: 0x%x2088(0x%x)-0x%x208C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1088),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x108C));
		cmdq_util_err("dip: 0x%x2090(0x%x)-0x%x2094(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1090),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1094));

		/*Request and Ready Signal*/
		/*0x%x216c - reserve bit*/
		cmdq_util_err("dip: 0x%x2150(0x%x)-0x%x2154(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1150),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1154));
		cmdq_util_err("dip: 0x%x2158(0x%x)-0x%x215C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1158),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x115C));
		cmdq_util_err("dip: 0x%x2160(0x%x)-0x%x2164(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1160),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1164));
		cmdq_util_err("dip: 0x%x2168(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1168));
		cmdq_util_err("dip: 0x%x2170(0x%x)-0x%x2174(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1170),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1174));
		cmdq_util_err("dip: 0x%x2178(0x%x)-0x%x217C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1178),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x117C));
		cmdq_util_err("dip: 0x%x2180(0x%x)-0x%x2184(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1180),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1184));
		cmdq_util_err("dip: 0x%x2188(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1188));
		cmdq_util_err("dip: 0x%x218C(0x%x)-0x%x2190(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x118C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1190));

		/*CQ_THR info*/
		cmdq_util_err("dip: 0x%x2204(0x%x)-0x%x2208(0x%x)-0x%x2210(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1204),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1208),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1210));
		cmdq_util_err("dip: 0x%x221C(0x%x)-0x%x2220(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x121C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1220));
		cmdq_util_err("dip: 0x%x2224(0x%x)-0x%x101C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1224),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x001C));

		/*FM register dump*/
		cmdq_util_err("dip: 0x%x8800(0x%x)-0x%x8804(0x%x)-0x%x8808(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7800),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7804),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7808));
		cmdqdebugcmd = 0x3c00;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("FM debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		/*FE register dump*/
		cmdq_util_err("dip: 0x%x8A40(0x%x)-0x%x8A44(0x%x)-0x%x8A48(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7A40),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7A44),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7A48));
		cmdq_util_err("dip: 0x%x8A4C(0x%x)-0x%x8A50(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7A4C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7A50));

		cmdq_util_err("crp_d4: 0x%x8CC0(0x%x)-0x%x8CC4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7CC0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7CC4));

		/*CNR register dump*/
		cmdq_util_err("dip: 0x%x73c0(0x%x)-0x%x73c4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x63c0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x63c4));
		cmdq_util_err("dip: 0x%x7430(0x%x)-0x%x7440(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6430),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6440));

		cmdqdebugcmd = 0x1502;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("CNR debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x11502;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("CNR debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x21502;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("CNR debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x31502;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("CNR debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x41502;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("CNR debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x51502;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("CNR debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x61502;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("CNR debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x71502;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("CNR debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0x502;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("MIX_D1 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0x602;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("C24_D3 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0x702;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("C2G_D1 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x10702;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("C2G_D1 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0x802;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("IGGM_D1 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x10802;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("IGGM_D1 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x20802;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("IGGM_D1 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x30802;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("IGGM_D1 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x40802;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("IGGM_D1 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0x902;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("CCM_D3 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0xA02;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("LCE debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0xB02;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("GGM_D3 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x10B02;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("GGM_D3 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x20B02;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("GGM_D3 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x30B02;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("GGM_D3 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x40B02;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("GGM_D1 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0xC02;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("DCE debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0xE02;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("G2C_D1 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x10E02;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("G2C_D1 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0xF02;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("C42_D2 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0x11002;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("EE debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x21002;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("EE debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x31002;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("EE debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x41002;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("EE debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x51002;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("EE debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0x1102;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("AKS debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x11102;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("AKS debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x21102;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("AKS debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x31102;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("AKS debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0x1202;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("SMT_D2 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x11202;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("SMT_D2 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x21202;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("SMT_D2 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x31202;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("SMT_D2 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x51202;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("SMT_D2 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0x1702;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("SMT_D3 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x11702;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("SMT_D3 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x21702;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("SMT_D3 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0x1B02;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("MIX_D2 debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0x2002;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("NR3D debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x12002;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("NR3D debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x22002;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("NR3D debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0x5;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("TDR debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x10005;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("TDR debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
		cmdqdebugcmd = 0x20005;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("TDR debug:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdq_util_err("dip:TDR rsv csr 0x%x21ac(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x11ac));
		cmdq_util_err("dip:bpc+tile edge 0x%x31c4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x21c4));
		cmdq_util_err("dip:bpc x/y offset 0x%x31c8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x21c8));
		cmdq_util_err("dip:bpc x/y size 0x%x31cc(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x21cc));


		cmdq_util_err("dip(DCM): 0x%x2110(0x%x)-0x%x2114(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1110),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1114));
		cmdq_util_err("dip(DCM): 0x%x2118(0x%x)-0x%x211C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1118),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x111C));
		cmdq_util_err("dip(DCM): 0x%x2120(0x%x)-0x%x2124(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1120),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1124));
		cmdq_util_err("dip(DCM): 0x%x2128(0x%x)-0x%x212C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1128),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x112C));

		cmdq_util_err("dip(DCM): 0x%x2130(0x%x)-0x%x2134(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1130),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1134));
		cmdq_util_err("dip(DCM): 0x%x2138(0x%x)-0x%x213C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1138),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x113C));
		cmdq_util_err("dip(DCM): 0x%x2140(0x%x)-0x%x2144(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1140),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1144));
		cmdq_util_err("dip(DCM): 0x%x2148(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1148));
		cmdq_util_err("dip(Done SEL): 0x%x2030(0x%x)-0x%x2034(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1030),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1034));
		cmdq_util_err("dip(Status En): 0x%x20A0(0x%x)-0x%x20B0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x10A0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x10B0));
		cmdq_util_err("dip(Status En): 0x%x20C0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x10C0));


		/*C02_D1 dump*/
		cmdq_util_err("dip: 0x%x5904(0x%x)-0x%x5908(0x%x)-0x%x590c(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4904),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4908),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x490c));
		/*C02_D2 dump*/
		cmdq_util_err("dip: 0x%x5104(0x%x)-0x%x5108(0x%x)-0x%x510c(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4104),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4108),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x410c));
		/*C24_D2, C24_D3 dump*/
		cmdq_util_err("dip: 0x%x8640(0x%x),0x%x6040(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7640),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5040));

		/*SRZ3 register dump*/
		cmdq_util_err("dip: 0x%x7540(0x%x)-0x%x7544(0x%x)-0x%x7548(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6540),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6544),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6548));
		cmdq_util_err("dip: 0x%x754C(0x%x)-0x%x7550(0x%x)-0x%x7554(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x654C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6550),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6554));
		cmdq_util_err("dip: 0x%x7558(0x%x)-0x%x755C(00x%x)-0x%x7560(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6558),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x655C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6560));

		/*SRZ4 register dump*/
		cmdq_util_err("dip: 0x%x52C0(0x%x)-0x%x52C4(0x%x)-0x%x52C8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x42C0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x42C4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x42C8));
		cmdq_util_err("dip: 0x%x52CC(0x%x)-0x%x52D0(0x%x)-0x%x52D4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x42CC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x42D0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x42D4));
		cmdq_util_err("dip: 0x%x52D8(0x%x)-0x%x52DC(0x%x)-0x%x52E0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x42D8),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x42DC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x42E0));

		/*SLK2 register dump*/
		cmdq_util_err("dip: 0x%x5240(0x%x)-0x%x5244(0x%x)-0x%x5248(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4240),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4244),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4248));
		cmdq_util_err("dip: 0x%x524C(0x%x)-0x%x5250(0x%x)-0x%x5254(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x424C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4250),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4254));
		cmdq_util_err("dip: 0x%x5258(0x%x)-0x%x525C(0x%x)-0x%x5260(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4258),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x425C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4260));
		cmdq_util_err("dip: 0x%x5264(0x%x)-0x%x5268(0x%x)-0x%x526C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4264),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4268),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x426C));

		/*SLK3 register dump*/
		cmdq_util_err("dip: 0x%x74C0(0x%x)-0x%x74C4(0x%x)-0x%x74C8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x64C0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x64C4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x64C8));
		cmdq_util_err("dip: 0x%x74CC(0x%x)-0x%x74D0(0x%x)-0x%x74D4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x64CC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x64D0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x64D4));
		cmdq_util_err("dip: 0x%x74D8(0x%x)-0x%x74DC(0x%x)-0x%x74E0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x64D8),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x64DC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x64E0));
		cmdq_util_err("dip: 0x%x74E4(0x%x)-0x%x74E8(0x%x)-0x%x74EC(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x64E4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x64E8),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x64EC));

		/*AKS register dump*/
		cmdq_util_err("dip: 0x%x7BC0(0x%x)-0x%x7BC8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6BC0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6BC8));

		/*SLK4 register dump*/
		cmdq_util_err("dip: 0x%x72C0(0x%x)-0x%x72C4(0x%x)-0x%x72C8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x62C0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x62C4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x62C8));
		cmdq_util_err("dip: 0x%x72CC(0x%x)-0x%x72D0(0x%x)-0x%x72D4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x62CC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x62D0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x62D4));
		cmdq_util_err("dip: 0x%x72D8(0x%x)-0x%x72DC(0x%x)-0x%x72E0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x62D8),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x62DC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x62E0));
		cmdq_util_err("dip: 0x%x72E4(0x%x)-0x%x72E8(0x%x)-0x%x72EC(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x62E4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x62E8),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x62EC));

		/*YNR register dump*/
		cmdq_util_err("dip: 0x%x5700(0x%x)-0x%x5704(0x%x)-0x%x5708(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4700),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4704),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4708));
		cmdq_util_err("dip: 0x%x570C(0x%x)-0x%x5710(0x%x)-0x%x5714(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x470C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4710),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4714));
		cmdq_util_err("dip: 0x%x5718(0x%x)-0x%x571C(0x%x)-0x%x5720(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4718),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x471C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4720));
		cmdq_util_err("dip: 0x%x5724(0x%x)-0x%x5728(0x%x)-0x%x572C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4724),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4728),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x472C));
		cmdq_util_err("dip: 0x%x5730(0x%x)-0x%x5734(0x%x)-0x%x5738(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4730),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4734),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4738));
		cmdq_util_err("dip: 0x%x573C(0x%x)-0x%x5740(0x%x)-0x%x5744(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x473C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4740),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4744));
		cmdq_util_err("dip: 0x%x5748(0x%x)-0x%x574C(0x%x)-0x%x5750(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4748),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x474C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4750));
		cmdq_util_err("dip: 0x%x5754(0x%x)-0x%x5758(0x%x)-0x%x575C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4754),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4758),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x475C));
		cmdq_util_err("dip: 0x%x5760(0x%x)-0x%x5764(0x%x)-0x%x5768(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4760),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4764),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4768));
		cmdq_util_err("dip: 0x%x576C(0x%x)-0x%x5770(0x%x)-0x%x5774(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x476C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4770),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4774));
		cmdq_util_err("dip: 0x%x5778(0x%x)-0x%x577C(0x%x)-0x%x5780(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4778),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x477C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4780));
		cmdq_util_err("dip: 0x%x5784(0x%x)-0x%x5788(0x%x)-0x%x578C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4784),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4788),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x478C));
		cmdq_util_err("dip: 0x%x5790(0x%x)-0x%x5794(0x%x)-0x%x5798(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4790),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4794),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4798));
		cmdq_util_err("dip: 0x%x579C(0x%x)-0x%x57A0(0x%x)-0x%x57A4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x479C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47A0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47A4));
		cmdq_util_err("dip: 0x%x57A8(0x%x)-0x%x57AC(0x%x)-0x%x57B0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47A8),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47AC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47B0));
		cmdq_util_err("dip: 0x%x57B4(0x%x)-0x%x57B8(0x%x)-0x%x57BC(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47B4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47B8),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47BC));
		cmdq_util_err("dip: 0x%x57C0(0x%x)-0x%x57C4(0x%x)-0x%x57C8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47C0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47C4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47C8));
		cmdq_util_err("dip: 0x%x57CC(0x%x)-0x%x57D0(0x%x)-0x%x57D4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47CC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47D0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47D4));
		cmdq_util_err("dip: 0x%x57D8(0x%x)-0x%x57DC(0x%x)-0x%x57E0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47D8),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47DC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47E0));
		cmdq_util_err("dip: 0x%x57E4(0x%x)-0x%x57E8(0x%x)-0x%x57EC(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47E4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47E8),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47EC));
		cmdq_util_err("dip: 0x%x57F0(0x%x)-0x%x57F4(0x%x)-0x%x57F8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47F0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47F4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47F8));
		cmdq_util_err("dip: 0x%x57FC(0x%x)-0x%x5800(0x%x)-0x%x5804(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x47FC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4800),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4804));
		cmdq_util_err("dip: 0x%x5808(0x%x)-0x%x580C(0x%x)-0x%x5810(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4808),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x480C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4810));
		cmdq_util_err("dip: 0x%x5814(0x%x)-0x%x5818(0x%x)-0x%x581C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4814),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4818),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x481C));
		cmdq_util_err("dip: 0x%x5820(0x%x)-0x%x5824(0x%x)-0x%x5828(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4820),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4824),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4828));
		cmdq_util_err("dip: 0x%x582C(0x%x)-0x%x5830(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x482C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4830));

		d1a_cq_en = DIP_RD32(dipRegBasAddr + 0x200);
		d1a_cq_en = d1a_cq_en & 0xEFFFFFFF;
		DIP_WR32(dipRegBasAddr + 0x200, d1a_cq_en);

		cmdqdebugcmd = 0x6;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("thread state:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		cmdqdebugcmd = 0x10006;
		DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
		cmdq_util_err("cq state:0x%x : dip: 0x%x2194(0x%x)",
			cmdqdebugcmd,
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));

		d1a_cq_en = DIP_RD32(dipRegBasAddr + 0x200);
		d1a_cq_en = d1a_cq_en | 0x10000000;
		DIP_WR32(dipRegBasAddr + 0x200, d1a_cq_en);

		for (cmdqdebugidx = 0; cmdqdebugidx < 16; cmdqdebugidx++) {
			cmdqdebugcmd = 0x6;
			cmdqdebugcmd = cmdqdebugcmd | (cmdqdebugidx << 16);
			DIP_WR32(dipRegBasAddr + 0x1190, cmdqdebugcmd);
			cmdq_util_err("cq checksum:0x%x : dip: 0x%x2194(0x%x)",
				cmdqdebugcmd,
				DipDumpTL[DIPNo].region,
				DIP_RD32(dipRegBasAddr + 0x1194));
		}

		/* 0x%x2190, DIPCTL_D1A_DIPCTL_DBG_SEL*/
		/* HUNG-WEN */
		DIP_WR32(dipRegBasAddr + 0x1190, 0x3);
		dipdmacmd = 0x00000014;
		for (i = 0; i < 104 ; i++) {
	    /* 0x%x10A8, DIPDMATOP_REG_D1A_DIPDMATOP_DMA_DEBUG_SEL */
			switch (i) {
			case 0x0:
				dmaidx = 1;
				cmdq_util_err("imgi dma debug");
				break;
			case 0x4:
				dmaidx = 3;
				cmdq_util_err("imgbi dma debug");
				break;
			case 0x8:
				dmaidx = 2;
				cmdq_util_err("imgci dma debug");
				break;
			case 0xc:
				dmaidx = 4;
				cmdq_util_err("vipi dma debug");
				break;
			case 0x10:
				dmaidx = 5;
				cmdq_util_err("vip2i dma debug");
				break;
			case 0x14:
				dmaidx = 6;
				cmdq_util_err("vip3i dma debug");
				break;
			case 0x18:
				dmaidx = 21;
				cmdq_util_err("smti_d1 dma debug");
				break;
			case 0x1c:
				dmaidx = 22;
				cmdq_util_err("smti_d2 dma debug");
				break;
			case 0x20:
				dmaidx = 23;
				cmdq_util_err("smti_d3 dma debug");
				break;
			case 0x24:
				dmaidx = 24;
				cmdq_util_err("smti_d4 dma debug");
				break;
			case 0x28:
				dmaidx = 9;
				cmdq_util_err("lcei_d1 dma debug");
				break;
			case 0x2c:
				dmaidx = 7;
				cmdq_util_err("dmgi_d1 dma debug");
				break;
			case 0x30:
				dmaidx = 8;
				cmdq_util_err("depi_d1 dma debug");
				break;
			case 0x34:
				dmaidx = 10;
				cmdq_util_err("ufdi_d1 dma debug");
				break;
			case 0x38:
				dmaidx = 14;
				cmdq_util_err("img3o_d1 dma debug");
				break;
			case 0x3c:
				dmaidx = 15;
				cmdq_util_err("img3bo_d1 dma debug");
				break;
			case 0x40:
				dmaidx = 16;
				cmdq_util_err("img3co_d1 dma debug");
				break;
			case 0x44:
				dmaidx = 11;
				cmdq_util_err("crzo_d1 dma debug");
				break;
			case 0x48:
				dmaidx = 12;
				cmdq_util_err("crzbo_d1 dma debug");
				break;
			case 0x4c:
				dmaidx = 13;
				cmdq_util_err("dceso_d1 dma debug");
				break;
			case 0x50:
				dmaidx = 18;
				cmdq_util_err("timgo_d1 dma debug");
				break;
			case 0x54:
				dmaidx = 25;
				cmdq_util_err("smto_d1 dma debug");
				break;
			case 0x58:
				dmaidx = 26;
				cmdq_util_err("smto_d2 dma debug");
				break;
			case 0x5c:
				dmaidx = 27;
				cmdq_util_err("smto_d3 dma debug");
				break;
			case 0x60:
				dmaidx = 28;
				cmdq_util_err("smto_d4 dma debug");
				break;
			case 0x64:
				dmaidx = 17;
				cmdq_util_err("feo_d1 dma debug");
				break;
			default:
				break;
			}
			dipdmacmd = dipdmacmd & 0xFFFF00FF;
			dipdmacmd = dipdmacmd | (i << 8);
			DIP_WR32(dipRegBasAddr + 0xA8, dipdmacmd);
			/* 0x%x2194, DIPCTL_REG_D1A_DIPCTL_DBG_OUT */
			cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
				dipdmacmd, DipDumpTL[DIPNo].region,
				DIP_RD32(dipRegBasAddr + 0x1194));
			i++;
			dipdmacmd = dipdmacmd & 0xFFFF00FF;
			dipdmacmd = dipdmacmd | (i << 8);
			DIP_WR32(dipRegBasAddr + 0xA8, dipdmacmd);
			cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
				dipdmacmd, DipDumpTL[DIPNo].region,
				DIP_RD32(dipRegBasAddr + 0x1194));
			i++;
			dipdmacmd = dipdmacmd & 0xFFFF00FF;
			dipdmacmd = dipdmacmd | (i << 8);
			DIP_WR32(dipRegBasAddr + 0xA8, dipdmacmd);
			cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
				dipdmacmd, DipDumpTL[DIPNo].region,
				DIP_RD32(dipRegBasAddr + 0x1194));
			i++;
			dipdmacmd = dipdmacmd & 0xFFFF00FF;
			dipdmacmd = dipdmacmd | (i << 8);
			DIP_WR32(dipRegBasAddr + 0xA8, dipdmacmd);
			cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
				dipdmacmd, DipDumpTL[DIPNo].region,
				DIP_RD32(dipRegBasAddr + 0x1194));

			if (((dmaidx >= 10) && (dmaidx <= 18)) ||
				((dmaidx >= 25) && (dmaidx <= 28))) {
				smidmacmd = 0x00080400;
				smidmacmd = smidmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, smidmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					smidmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));

				smidmacmd = 0x00090400;
				smidmacmd = smidmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, smidmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					smidmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));

				smidmacmd = 0x00000400;
				smidmacmd = smidmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, smidmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					smidmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));

				smidmacmd = 0x00010400;
				smidmacmd = smidmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, smidmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					smidmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));

				fifodmacmd = 0x00000300;
				fifodmacmd = fifodmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, fifodmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					fifodmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));

				fifodmacmd = 0x00010300;
				fifodmacmd = fifodmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, fifodmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					fifodmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));

				fifodmacmd = 0x00020300;
				fifodmacmd = fifodmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, fifodmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					fifodmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));

				fifodmacmd = 0x00030300;
				fifodmacmd = fifodmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, fifodmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					fifodmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));

			} else {
				smidmacmd = 0x00080100;
				smidmacmd = smidmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, smidmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					smidmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));

				smidmacmd = 0x00000100;
				smidmacmd = smidmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, smidmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					smidmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));

				fifodmacmd = 0x00000200;
				fifodmacmd = fifodmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, fifodmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					fifodmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));

				fifodmacmd = 0x00010200;
				fifodmacmd = fifodmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, fifodmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					fifodmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));

				fifodmacmd = 0x00020200;
				fifodmacmd = fifodmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, fifodmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					fifodmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));

				fifodmacmd = 0x00030200;
				fifodmacmd = fifodmacmd | dmaidx;
				DIP_WR32(dipRegBasAddr + 0xA8, fifodmacmd);
				cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
					fifodmacmd, DipDumpTL[DIPNo].region,
					DIP_RD32(dipRegBasAddr + 0x1194));
			}
		}

		// module checksum
		for (dd = 0; dd < 0x80; dd++) {
			switch (dd) {
			case 0x17:
				dd = 0x20;
				break;
			case 0x2E:
				dd = 0x70;
				break;
			default:
				break;
			}
			dbg_sel = ((dd << 8) + dbg_rgb) & 0xFF0F;
			DIP_WR32(dipRegBasAddr + 0x1190, dbg_sel);
			dbg_out = DIP_RD32(dipRegBasAddr + 0x1194);
			cmdq_util_err("YW DBG rgb dbg_sel: 0x%08x dbg_out: 0x%08x",
						dbg_sel, dbg_out);
		}

		for (dd = 0; dd < 0x7C; dd++) {
			switch (dd) {
			case 0x1D:
				dd = 0x20;
				break;
			case 0x40:
				dd = 0x70;
				break;
			default:
				break;
			}
			dbg_sel = ((dd << 8)  + dbg_yuv) & 0xFF0F;
			DIP_WR32(dipRegBasAddr + 0x1190, dbg_sel);
			dbg_out = DIP_RD32(dipRegBasAddr + 0x1194);
			cmdq_util_err("YW DBG yuv dbg_sel: 0x%08x dbg_out: 0x%08x",
						dbg_sel, dbg_out);
		}

		for (dd = 0; dd < 9; dd++) {
			dbg_sel = 0x00000302 | (dd << 16);
			DIP_WR32(dipRegBasAddr + 0x1190, dbg_sel);
			dbg_out = DIP_RD32(dipRegBasAddr + 0x1194);
			cmdq_util_err("YNR dbg_sel: 0x%08x dbg_out: 0x%08x",
					dbg_sel, dbg_out);
		}

		DIP_WR32(dipRegBasAddr + 0x1190, 0x00000202);
		dbg_out = DIP_RD32(dipRegBasAddr + 0x1194);
		cmdq_util_err("mix_d3 dbg_sel: 0x%08x dbg_out: 0x%08x\n",
					dbg_sel, dbg_out);
		DIP_WR32(dipRegBasAddr + 0x1190, 0x00000402);
		dbg_out = DIP_RD32(dipRegBasAddr + 0x1194);
		cmdq_util_err("ndg_d1 dbg_sel: 0x%08x dbg_out: 0x%08x\n",
					dbg_sel, dbg_out);
		DIP_WR32(dipRegBasAddr + 0x1190, 0x00000502);
		dbg_out = DIP_RD32(dipRegBasAddr + 0x1194);
		cmdq_util_err("mix_d1 dbg_sel: 0x%08x dbg_out: 0x%08x\n",
					dbg_sel, dbg_out);
		DIP_WR32(dipRegBasAddr + 0x1190, 0x00000602);
		dbg_out = DIP_RD32(dipRegBasAddr + 0x1194);
		cmdq_util_err("c24_d3 dbg_sel: 0x%08x dbg_out: 0x%08x\n",
					dbg_sel, dbg_out);
		DIP_WR32(dipRegBasAddr + 0x1190, 0x00002002);
		dbg_out = DIP_RD32(dipRegBasAddr + 0x1194);
		cmdq_util_err("nr3d dbg_sel: 0x%08x dbg_out: 0x%08x\n",
					dbg_sel, dbg_out);
		DIP_WR32(dipRegBasAddr + 0x1190, 0x00012002);
		dbg_out = DIP_RD32(dipRegBasAddr + 0x1194);
		cmdq_util_err("nr3d dbg_sel: 0x%08x dbg_out: 0x%08x\n",
					dbg_sel, dbg_out);
		DIP_WR32(dipRegBasAddr + 0x1190, 0x00022002);
		dbg_out = DIP_RD32(dipRegBasAddr + 0x1194);
		cmdq_util_err("nr3d dbg_sel: 0x%08x dbg_out: 0x%08x\n",
					dbg_sel, dbg_out);
		DIP_WR32(dipRegBasAddr + 0x1190, 0x00032002);
		dbg_out = DIP_RD32(dipRegBasAddr + 0x1194);
		cmdq_util_err("nr3d dbg_sel: 0x%08x dbg_out: 0x%08x\n",
					dbg_sel, dbg_out);

		/* DMA Error */
		cmdq_util_err("DMA_ERR_EN  0x%x1020(0x%x)",
		DipDumpTL[DIPNo].region,
		DIP_RD32(dipRegBasAddr + 0x20));
		cmdq_util_err("img2o  0x%x1068(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x68));
		cmdq_util_err("img2bo 0x%x106C(0x%x)"
			, DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6C));
		cmdq_util_err("img3o  0x%x1080(0x%x)"
			, DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x80));
		cmdq_util_err("img3bo 0x%x1084(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x84));
		cmdq_util_err("img3Co 0x%x1088(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x88));
		cmdq_util_err("feo	 0x%x1070(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x70));
		cmdq_util_err("dceso  0x%x1054(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x54));
		cmdq_util_err("timgo  0x%x103C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x3C));
		cmdq_util_err("imgi  0x%x1024(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x24));
		cmdq_util_err("imgbi  0x%x1034(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x34));
		cmdq_util_err("imgci  0x%x1038(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x38));
		cmdq_util_err("vipi  0x%x1074(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x74));
		cmdq_util_err("vip2i  0x%x1078(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x78));
		cmdq_util_err("vip3i  0x%x107C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7C));
		cmdq_util_err("dmgi  0x%x1048(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x48));
		cmdq_util_err("depi  0x%x104C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4C));
		cmdq_util_err("lcei  0x%x1050(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x50));
		cmdq_util_err("ufdi  0x%x1028(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x28));
		cmdq_util_err("smx1o  0x%x1030(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x30));
		cmdq_util_err("smx2o  0x%x105C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5C));
		cmdq_util_err("smx3o  0x%x1064(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x64));
		cmdq_util_err("smx4o  0x%x1044(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x44));
		cmdq_util_err("smx5o  0x%x11D4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1D4));
		cmdq_util_err("smx6o  0x%x11DC(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1DC));
		cmdq_util_err("smx1i  0x%x102C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x2C));
		cmdq_util_err("smx2i  0x%x1058(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x58));
		cmdq_util_err("smx3i  0x%x1060(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x60));
		cmdq_util_err("smx4i  0x%x1040(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x40));
		cmdq_util_err("smx5i  0x%x11D0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1D0));
		cmdq_util_err("smx6i  0x%x11D8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1D8));

		/* Interrupt Status */
		cmdq_util_err("DIPCTL_INT1_STATUSX	0x%x20A8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x10A8));
		cmdq_util_err("DIPCTL_INT2_STATUSX	0x%x20B8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x10B8));
		cmdq_util_err("DIPCTL_INT3_STATUSX	0x%x20C8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x10C8));
		cmdq_util_err("CQ_INT1_STATUSX	0x%x20D8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x10D8));
		cmdq_util_err("CQ_INT2_STATUSX	0x%x20E8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x10E8));
		cmdq_util_err("CQ_INT3_STATUSX	0x%x20F8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x10F8));

			/* IMGI DMA*/
		cmdq_util_err("imgi: 0x%x1200(0x%x)-0x%x1204(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x200),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x204));
		cmdq_util_err("imgi: 0x%x1208(0x%x)-0x%x120C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x208),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x20C));
		cmdq_util_err("imgi: 0x%x1210(0x%x)-0x%x1114(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x210),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x214));
		cmdq_util_err("imgi: 0x%x1218(0x%x)-0x%x121C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x218),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x21C));
		cmdq_util_err("imgi: 0x%x1220(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x220));

		/* TIMGO DMA*/
		cmdq_util_err("timgo: 0x%x1360(0x%x)-0x%x1364(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x360),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x364));
		cmdq_util_err("timgo: 0x%x136C(0x%x)-0x%x1370(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x36C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x370));
		cmdq_util_err("timgo: 0x%x1374(0x%x)-0x%x1378(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x374),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x378));
		cmdq_util_err("timgo: 0x%x137C(0x%x)-0x%x1380(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x37C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x380));
		cmdq_util_err("timgo: 0x%x1384(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x284));

		/* CRZO DMA*/
		cmdq_util_err("crzo: 0x%x16B0(0x%x)-0x%x16B4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6B0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6B4));
		cmdq_util_err("crzo: 0x%x16BC(0x%x)-0x%x16C0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6BC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6C0));
		cmdq_util_err("crzo: 0x%x16C4(0x%x)-0x%x16C8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6C4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6C8));
		cmdq_util_err("crzo: 0x%x16CC(0x%x)-0x%x16D0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6CC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6D0));
		cmdq_util_err("crzo: 0x%x16D4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6D4));

		/* CRZBO DMA*/
		cmdq_util_err("crzbo: 0x%x1720(0x%x)-0x%x1724(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x720),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x724));
		cmdq_util_err("crzbo: 0x%x172C(0x%x)-0x%x1730(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x72C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x730));
		cmdq_util_err("crzbo: 0x%x1734(0x%x)-0x%x1738(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x734),
			DipDumpTL[DIPNo].region
			, DIP_RD32(dipRegBasAddr + 0x738));
		cmdq_util_err("crzbo: 0x%x173C(0x%x)-0x%x1740(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x73C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x740));
		cmdq_util_err("crzbo: 0x%x1744(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x744));

		/* DCES DMA*/
		cmdq_util_err("dceso: 0x%x1500(0x%x)-0x%x1504(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x500),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x504));
		cmdq_util_err("dceso: 0x%x150C(0x%x)-0x%x1510(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x50C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x510));
		cmdq_util_err("dceso: 0x%x1514(0x%x)-0x%x1518(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x514),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x518));
		cmdq_util_err("dceso: 0x%x151C(0x%x)-0x%x1520(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x51C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x520));
		cmdq_util_err("dceso: 0x%x1524(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x524));

		/* FEO DMA*/
		cmdq_util_err("feo: 0x%x1790(0x%x)-0x%x1794(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x790),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x794));
		cmdq_util_err("feo: 0x%x179C(0x%x)-0x%x17A0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x79C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7A0));
		cmdq_util_err("feo: 0x%x17A4(0x%x)-0x%x17A8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7A4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7A8));
		cmdq_util_err("feo: 0x%x17AC(0x%x)-0x%x17B0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7AC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7B0));
		cmdq_util_err("feo: 0x%x17B4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7B4));

		/* IMG3O DMA*/
		cmdq_util_err("img3o: 0x%x1890(0x%x)-0x%x1894(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x890),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x894));
		cmdq_util_err("img3o: 0x%x189C(0x%x)-0x%x18A0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x89C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x8A0));
		cmdq_util_err("img3o: 0x%x18A4(0x%x)-0x%x18A8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x8A4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x8A8));
		cmdq_util_err("img3o: 0x%x18AC(0x%x)-0x%x18B0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x8AC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x8B0));
		cmdq_util_err("img3o: 0x%x18B4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x8B4));

		/* IMG3BO DMA*/
		cmdq_util_err("img3bo: 0x%x1900(0x%x)-0x%x1904(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x900),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x904));
		cmdq_util_err("img3bo: 0x%x190C(0x%x)-0x%x1910(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x90C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x910));
		cmdq_util_err("img3bo: 0x%x1914(0x%x)-0x%x1918(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x914),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x918));
		cmdq_util_err("img3bo: 0x%x191C(0x%x)-0x%x1920(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x91C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x920));
		cmdq_util_err("img3bo: 0x%x1924(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x924));

		/* IMG3CO DMA*/
		cmdq_util_err("img3co: 0x%x1970(0x%x)-0x%x1974(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x970),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x974));
		cmdq_util_err("img3co: 0x%x197C(0x%x)-0x%x1980(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x97C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x980));
		cmdq_util_err("img3co: 0x%x1984(0x%x)-0x%x1988(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x984),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x988));
		cmdq_util_err("img3co: 0x%x198C(0x%x)-0x%x1990(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x98C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x990));
		cmdq_util_err("img3co: 0x%x1994(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x994));

		/* LCEI DMA*/
		cmdq_util_err("lcei: 0x%x14D0(0x%x)-0x%x14D4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04D0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04D4));
		cmdq_util_err("lcei: 0x%x14DC(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04DC));
		cmdq_util_err("lcei: 0x%x14E0(0x%x)-0x%x14E4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04E0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04E4));
		cmdq_util_err("lcei: 0x%x14E8(0x%x)-0x%x14EC(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04E8),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04EC));
		cmdq_util_err("lcei: 0x%x14F0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04F0));

		/* VIPI DMA*/
		cmdq_util_err("vipi: 0x%x1800(0x%x)-0x%x1804(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x800),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x804));
		cmdq_util_err("vipi: 0x%x180C(0x%x)-0x%x1810(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x80C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x810));
		cmdq_util_err("vipi: 0x%x1814(0x%x)-0x%x1818(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x814),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x818));
		cmdq_util_err("vipi: 0x%x181C(0x%x)-0x%x1820(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x81C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x820));

		/* VIPBI DMA*/
		cmdq_util_err("vipbi: 0x%x1830(0x%x)-0x%x1834(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x830),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x834));
		cmdq_util_err("vipbi: 0x%x183C(0x%x)-0x%x1840(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x83C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x840));
		cmdq_util_err("vipbi: 0x%x1844(0x%x)-0x%x1848(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x844),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x848));
		cmdq_util_err("vipbi: 0x%x184C(0x%x)-0x%x1850(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x84C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x850));

		/* VIPCI DMA*/
		cmdq_util_err("vipci: 0x%x1860(0x%x)-0x%x1864(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x860),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x864));
		cmdq_util_err("vipci: 0x%x186C(0x%x)-0x%x1870(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x86C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x870));
		cmdq_util_err("vipci: 0x%x1874(0x%x)-0x%x1878(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x874),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x878));
		cmdq_util_err("vipci: 0x%x187C(0x%x)-0x%x1880(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x87C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x880));


		cmdq_util_err("nr3d: 0x%x8000(0x%x)-0x%x8004(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7000),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7004));
		cmdq_util_err("nr3d: 0x%x8008(0x%x)-0x%x800c(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7008),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x700c));
		cmdq_util_err("nr3d: 0x%x8010(0x%x)-0x%x8014(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7010),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7014));
		cmdq_util_err("nr3d: 0x%x8218(0x%x)-0x%x821c(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7218),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x721c));
		cmdq_util_err("nr3d: 0x%x8220(0x%x)-0x%x8224(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7220),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7224));
		cmdq_util_err("nr3d: 0x%x8228(0x%x)-0x%x822c(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7228),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x722c));
		cmdq_util_err("mix_d2: 0x%x7b40(0x%x)-0x%x7b44(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6b40),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6b44));


		cmdq_util_err("dce: 0x%x7000(0x%x)-0x%x7088(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6000),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6088));
		cmdq_util_err("dce: 0x%x708C(0x%x)-0x%x7090(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x608C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6090));

		/* CRZ */
		cmdq_util_err("crz: 0x%x8700(0x%x)-0x%x8704(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7700),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7704));
		cmdq_util_err("crz: 0x%x8708(0x%x)-0x%x870C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7708),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x770C));
		cmdq_util_err("crz: 0x%x8710(0x%x)-0x%x8714(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7710),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7714));
		cmdq_util_err("crz: 0x%x8718(0x%x)-0x%x871C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7718),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x771C));
		cmdq_util_err("crz: 0x%x8720(0x%x)-0x%x8724(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7720),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7724));
		cmdq_util_err("crz: 0x%x8728(0x%x)-0x%x872C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7728),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x772C));
		cmdq_util_err("crz: 0x%x8730(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7730));
		cmdq_util_err("crz: 0x%x8734(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7734));

		/* IMGBI */
		cmdq_util_err("imgbi: 0x%x1300(0x%x)-0x%x1304(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0300),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0304));
		cmdq_util_err("imgbi: 0x%x130C(0x%x)-0x%x1310(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x030C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0310));
		cmdq_util_err("imgbi: 0x%x1314(0x%x)-0x%x1318(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0314),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0318));
		cmdq_util_err("imgbi: 0x%x131C(0x%x)-0x%x1320(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x031C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0320));

		/* IMGCI */
		cmdq_util_err("imgci: 0x%x1330(0x%x)-0x%x1334(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0330),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0334));
		cmdq_util_err("imgci: 0x%x133C(0x%x)-0x%x1340(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x033C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0340));
		cmdq_util_err("imgci: 0x%x1344(0x%x)-0x%x1348(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0344),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0348));
		cmdq_util_err("imgci: 0x%x134C(0x%x)-0x%x1350(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x034C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0350));

		/* DEPI */
		cmdq_util_err("depi: 0x%x14A0(0x%x)-0x%x14A4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04A0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04A4));
		cmdq_util_err("depi: 0x%x14AC(0x%x)-0x%x14B0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04AC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04B0));
		cmdq_util_err("depi: 0x%x14B4(0x%x)-0x%x14B8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04B4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04B8));
		cmdq_util_err("depi: 0x%x14BC(0x%x)-0x%x14D0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04BC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x04D0));

		/* DMGI */
		cmdq_util_err("dmgi: 0x%x1470(0x%x)-0x%x1474(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0470),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0474));
		cmdq_util_err("dmgi: 0x%x147C(0x%x)-0x%x1480(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x047C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0480));
		cmdq_util_err("dmgi: 0x%x1484(0x%x)-0x%x1488(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0484),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0488));
		cmdq_util_err("dmgi: 0x%x148C(0x%x)-0x%x1490(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x048C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0490));

		/* LCE */
		cmdq_util_err("lce: 0x%x6A00(0x%x)-0x%x6A04(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5A00),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5A04));
		cmdq_util_err("lce: 0x%x6A08(0x%x)-0x%x6A0C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5A08),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5A0C));
		cmdq_util_err("lce: 0x%x6A10(0x%x)-0x%x6A14(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5A10),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5A14));
		cmdq_util_err("lce: 0x%x6A18(0x%x)-0x%x6A1C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5A18),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5A1C));
		cmdq_util_err("lce: 0x%x6A20(0x%x)-0x%x6A24(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5A20),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5A24));
		cmdq_util_err("lce: 0x%x6A28(0x%x)-0x%x6A2C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5A28),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5A2C));
		cmdq_util_err("0x%x6A30(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x5A30));

		/* YNR */
		cmdq_util_err("ynr: 0x%x5700(0x%x)-0x%x5704(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4700),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4704));

		/* BNR */
		cmdq_util_err("bnr: 0x%x3100(0x%x)-0x%x315C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x2100),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x215C));
		cmdq_util_err("bnr: 0x%x3124(0x%x)-0x%x3148(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x2124),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x2148));

		/* NR3D */
		cmdq_util_err("tnr: 0x%x8000(0x%x)-0x%x800C(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x7000),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x700c));
		cmdq_util_err("color: 0x%x7640(0x%x)-0x%x7750(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6640),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6750));

		/* SMT1O DMA*/
		cmdq_util_err("smt1 ctrl: 0x%x3080(0x%x)-0x%x3084(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x2080),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x2084));
		cmdq_util_err("smt1 ctrl: 0x%x308C(0x%x)-0x%x3090(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x208C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x2090));
		cmdq_util_err("smt1 ctrl: 0x%x3094(0x%x)-0x%x3098(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x2094),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x2098));
		cmdq_util_err("smt1 ctrl: 0x%x309C(0x%x)-0x%x30A0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x209C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x20A0));
		cmdq_util_err("smto_d1a: 0x%x1290(0x%x)-0x%x1294(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0290),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0294));
		cmdq_util_err("smto_d1a: 0x%x129C(0x%x)-0x%x12A0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x029C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x02A0));
		cmdq_util_err("smto_d1a: 0x%x12A4(0x%x)-0x%x12A8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x02A4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x02A8));
		cmdq_util_err("smto_d1a: 0x%x12AC(0x%x)-0x%x12B0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x02AC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x02B0));
		cmdq_util_err("smto_d1a: 0x%x12B4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x02B4));

		/* SMX2O DMA*/
		cmdq_util_err("smt2 ctrl: 0x%x7300(0x%x)-0x%x7304(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6300),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6304));
		cmdq_util_err("smt2 ctrl: 0x%x730C(0x%x)-0x%x7310(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x630C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6310));
		cmdq_util_err("smt2 ctrl: 0x%x7314(0x%x)-0x%x7318(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6314),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6318));
		cmdq_util_err("smt2 ctrl: 0x%x731C(0x%x)-0x%x7320(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x631C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6320));
		cmdq_util_err("smto_d2a: 0x%x15A0(0x%x)-0x%x15A4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x05A0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x05A4));
		cmdq_util_err("smto_d2a: 0x%x15AC(0x%x)-0x%x15B0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x05AC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x05B0));
		cmdq_util_err("smto_d2a: 0x%x15B4(0x%x)-0x%x15B8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x05B4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x05B8));
		cmdq_util_err("smto_d2a: 0x%x15BC(0x%x)-0x%x15C0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x05BC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x05C0));
		cmdq_util_err("smto_d2a: 0x%x15C4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x05C4));

		/* SMX3O DMA*/
		cmdq_util_err("smt3 ctrl: 0x%x7580(0x%x)-0x%x7584(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6580),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6584));
		cmdq_util_err("smt3 ctrl: 0x%x758C(0x%x)-0x%x7590(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x658C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6590));
		cmdq_util_err("smt3 ctrl: 0x%x7594(0x%x)-0x%x7598(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6594),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x6598));
		cmdq_util_err("smt3 ctrl: 0x%x759C(0x%x)-0x%x75A0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x659C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x65A0));
		cmdq_util_err("smt3o: 0x%x1640(0x%x)-0x%x1644(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0640),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0644));
		cmdq_util_err("smt3o: 0x%x164C(0x%x)-0x%x1650(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x064C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0650));
		cmdq_util_err("smt3o: 0x%x1654(0x%x)-0x%x1658(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0654),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0658));
		cmdq_util_err("smt3o: 0x%x165C(0x%x)-0x%x1660(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x065C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0660));
		cmdq_util_err("smt3o: 0x%x1664(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0664));

		/* SMX4O DMA*/
		cmdq_util_err("smt4 ctrl: 0x%x5B00(0x%x)-0x%x5B04(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4B00),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4B04));
		cmdq_util_err("smt4 ctrl: 0x%x5B0C(0x%x)-0x%x5B10(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4B0C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4B10));
		cmdq_util_err("smt4 ctrl: 0x%x5B14(0x%x)-0x%x5B18(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4B14),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4B18));
		cmdq_util_err("smt4 ctrl: 0x%x5B1C(0x%x)-0x%x5B20(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4B1C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4B20));
		cmdq_util_err("smt4o: 0x%x1400(0x%x)-0x%x1404(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0400),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0404));
		cmdq_util_err("smt4o: 0x%x140C(0x%x)-0x%x1410(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x040C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0410));
		cmdq_util_err("smt4o: 0x%x1414(0x%x)-0x%x1418(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0414),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0418));
		cmdq_util_err("smt4o: 0x%x141C(0x%x)-0x%x1420(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x041C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0420));
		cmdq_util_err("smt4o: 0x%x1424(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0424));

		/* SMX5O DMA*/
		cmdq_util_err("smt5 ctrl: 0x%x5BC0(0x%x)-0x%x5BC4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4BC0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4BC4));
		cmdq_util_err("smt5 ctrl: 0x%x5BCC(0x%x)-0x%x5BD0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4BCC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4BD0));
		cmdq_util_err("smt5 ctrl: 0x%x5BD4(0x%x)-0x%x5BD8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4BD4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4BD8));
		cmdq_util_err("smt5 ctrl: 0x%x5BDC(0x%x)-0x%x5BE0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4BDC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4BE0));
		cmdq_util_err("smt5o: 0x%x1A10(0x%x)-0x%x1A14(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A10),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A14));
		cmdq_util_err("smt5o: 0x%x1A1C(0x%x)-0x%x1A20(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A1C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A20));
		cmdq_util_err("smt5o: 0x%x1A24(0x%x)-0x%x1A28(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A24),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A28));
		cmdq_util_err("smt5o: 0x%x1A2C(0x%x)-0x%x1A30(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A2C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A30));
		cmdq_util_err("smt5o: 0x%x1A34(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A34));

		/* SMX6O DMA*/
		cmdq_util_err("smt6 ctrl: 0x%x5CC0(0x%x)-0x%x5CC4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4CC0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4CC4));
		cmdq_util_err("smt6 ctrl: 0x%x5CCC(0x%x)-0x%x5CD0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4CCC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4CD0));
		cmdq_util_err("smt6 ctrl: 0x%x5CD4(0x%x)-0x%x5CD8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4CD4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4CD8));
		cmdq_util_err("smt6 ctrl: 0x%x5CDC(0x%x)-0x%x5CE0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4CDC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x4CE0));
		cmdq_util_err("smt6o: 0x%x1AB0(0x%x)-0x%x1AB4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0AB0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0AB4));
		cmdq_util_err("smt6o: 0x%x1ABC(0x%x)-0x%x1AC0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0ABC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0AC0));
		cmdq_util_err("smt6o: 0x%x1AC4(0x%x)-0x%x1AC8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0AC4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0AC8));
		cmdq_util_err("smt6o: 0x%x1ACC(0x%x)-0x%x1AD0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0ACC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0AD0));
		cmdq_util_err("smt6o: 0x%x1AD4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0AD4));

		/* SMX1I DMA*/
		cmdq_util_err("smt1i: 0x%x1260(0x%x)-0x%x1264(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0260),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0264));
		cmdq_util_err("smt1i: 0x%x126C(0x%x)-0x%x1270(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x026C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0270));
		cmdq_util_err("smt1i: 0x%x1274(0x%x)-0x%x1278(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0274),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0278));
		cmdq_util_err("smt1i: 0x%x127C(0x%x)-0x%x1280(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x027C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0280));

		/* SMX2I DMA*/
		cmdq_util_err("smt2i: 0x%x1570(0x%x)-0x%x1574(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0570),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0574));
		cmdq_util_err("smt2i: 0x%x157C(0x%x)-0x%x1580(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x057C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0580));
		cmdq_util_err("smt2i: 0x%x1584(0x%x)-0x%x1588(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0584),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0588));
		cmdq_util_err("smt2i: 0x%x158C(0x%x)-0x%x1590(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x058C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0590));

		/* SMX3I DMA*/
		cmdq_util_err("smt3i: 0x%x1610(0x%x)-0x%x1614(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0610),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0614));
		cmdq_util_err("smt3i: 0x%x161C(0x%x)-0x%x1620(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x061C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0620));
		cmdq_util_err("smt3i: 0x%x1624(0x%x)-0x%x1628(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0624),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0628));
		cmdq_util_err("smt3i: 0x%x162C(0x%x)-0x%x1630(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x062C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0630));

		/* SMX4I DMA*/
		cmdq_util_err("smt4i: 0x%x13D0(0x%x)-0x%x13D4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x03D0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x03D4));
		cmdq_util_err("smt4i: 0x%x13DC(0x%x)-0x%x13E0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x03DC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x03E0));
		cmdq_util_err("smt4i: 0x%x13E4(0x%x)-0x%x13E8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x03E4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x03E8));
		cmdq_util_err("smt4i: 0x%x13EC(0x%x)-0x%x13F0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x03EC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x03F0));

		/* SMX5I DMA*/
		cmdq_util_err("smt5i: 0x%x19E0(0x%x)-0x%x19E4(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x09E0),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x09E4));
		cmdq_util_err("smt5i: 0x%x19EC(0x%x)-0x%x19F0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x09EC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x09F0));
		cmdq_util_err("smt5i: 0x%x19F4(0x%x)-0x%x19F8(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x09F4),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x09F8));
		cmdq_util_err("smt5i: 0x%x19FC(0x%x)-0x%x1A00(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x09FC),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A00));

		/* SMX6I DMA*/
		cmdq_util_err("smt6i: 0x%x1A80(0x%x)-0x%x1A84(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A80),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A84));
		cmdq_util_err("smt6i: 0x%x1A8C(0x%x)-0x%x1A90(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A8C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A90));
		cmdq_util_err("smt6i: 0x%x1A94(0x%x)-0x%x1A98(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A94),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A98));
		cmdq_util_err("smt6i: 0x%x1A9C(0x%x)-0x%x1AA0(0x%x)",
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0A9C),
			DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x0AA0));

		DIP_WR32(dipRegBasAddr + 0xA8, dipdmacmd);
		cmdq_util_err("0x%x : dip: 0x%x2194(0x%x)",
			dipdmacmd, DipDumpTL[DIPNo].region,
			DIP_RD32(dipRegBasAddr + 0x1194));
	}
	cmdq_util_err("***** DIP DUMP E.*****");

	return Ret;
}

static signed int DIP_DumpDIPReg(void)
{
	signed int Ret = 0;
	unsigned int loop = 0;
#if 1 /*YW TODO*/
	unsigned int i, cmdqidx = 0, mfbcmd = 0;
#ifdef AEE_DUMP_REDUCE_MEMORY
	unsigned int offset = 0;
	uintptr_t OffsetAddr = 0;
	unsigned int ctrl_start;
#endif
	/*  */
	cmdq_util_err("- E.");
	cmdq_util_err("g_bDumpPhyDIPBuf:(0x%x), g_pPhyDIPBuffer:(0x%p)",
		g_bDumpPhyDIPBuf, g_pPhyDIPBuffer);
	cmdq_util_err("g_pPhyMFBBuffer:(0x%p), g_pPhyMSSBuffer:(0x%p)",
		g_pPhyMFBBuffer, g_pPhyMSSBuffer);
	cmdq_util_err("g_bIonBuf:(0x%x)", g_bIonBufferAllocated);

	cmdq_util_err("imgsys: 0x%x(0x%x)-0x%x(0x%x)",
		(DIP_IMGSYS_BASE_HW),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE),
		(DIP_IMGSYS_BASE_HW + 0x4C),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x4C));
	cmdq_util_err("imgsys: 0x%x(0x%x)-0x%x(0x%x)",
		(DIP_IMGSYS_BASE_HW + 0x200),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x200),
		(DIP_IMGSYS_BASE_HW + 0x204),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x204));
	cmdq_util_err("imgsys: 0x%x(0x%x)-0x%x(0x%x)",
		(DIP_IMGSYS_BASE_HW + 0x208),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x208),
		(DIP_IMGSYS_BASE_HW + 0x20C),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x20C));
	cmdq_util_err("imgsys: 0x%x(0x%x)-0x%x(0x%x)",
		(DIP_IMGSYS_BASE_HW + 0x220),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x220),
		(DIP_IMGSYS_BASE_HW + 0x224),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x224));
	cmdq_util_err("imgsys: 0x%x(0x%x)-0x%x(0x%x)",
		(DIP_IMGSYS_BASE_HW + 0x228),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x228),
		(DIP_IMGSYS_BASE_HW + 0x230),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x230));
	cmdq_util_err("imgsys: 0x%x(0x%x)-0x%x(0x%x)",
		(DIP_IMGSYS_BASE_HW + 0x234),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x234),
		(DIP_IMGSYS_BASE_HW + 0x238),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x238));

	cmdq_util_err("imgsys2: 0x%x(0x%x)-0x%x(0x%x)",
		(DIP_IMGSYS2_BASE_HW),
		DIP_RD32(DIP_IMGSYS2_CONFIG_BASE),
		(DIP_IMGSYS2_BASE_HW + 0x4C),
		DIP_RD32(DIP_IMGSYS2_CONFIG_BASE + 0x4C));
	cmdq_util_err("imgsys2: 0x%x(0x%x)-0x%x(0x%x)",
		(DIP_IMGSYS2_BASE_HW + 0x200),
		DIP_RD32(DIP_IMGSYS2_CONFIG_BASE + 0x200),
		(DIP_IMGSYS2_BASE_HW + 0x204),
		DIP_RD32(DIP_IMGSYS2_CONFIG_BASE + 0x204));
	cmdq_util_err("imgsys2: 0x%x(0x%x)-0x%x(0x%x)",
		(DIP_IMGSYS2_BASE_HW + 0x208),
		DIP_RD32(DIP_IMGSYS2_CONFIG_BASE + 0x208),
		(DIP_IMGSYS2_BASE_HW + 0x20C),
		DIP_RD32(DIP_IMGSYS2_CONFIG_BASE + 0x20C));
	cmdq_util_err("imgsys2: 0x%x(0x%x)-0x%x(0x%x)",
		(DIP_IMGSYS2_BASE_HW + 0x220),
		DIP_RD32(DIP_IMGSYS2_CONFIG_BASE + 0x220),
		(DIP_IMGSYS2_BASE_HW + 0x224),
		DIP_RD32(DIP_IMGSYS2_CONFIG_BASE + 0x224));
	cmdq_util_err("imgsys2: 0x%x(0x%x)-0x%x(0x%x)",
		(DIP_IMGSYS2_BASE_HW + 0x228),
		DIP_RD32(DIP_IMGSYS2_CONFIG_BASE + 0x228),
		(DIP_IMGSYS2_BASE_HW + 0x230),
		DIP_RD32(DIP_IMGSYS2_CONFIG_BASE + 0x230));
	cmdq_util_err("imgsys2: 0x%x(0x%x)-0x%x(0x%x)",
		(DIP_IMGSYS2_BASE_HW + 0x234),
		DIP_RD32(DIP_IMGSYS2_CONFIG_BASE + 0x234),
		(DIP_IMGSYS2_BASE_HW + 0x238),
		DIP_RD32(DIP_IMGSYS2_CONFIG_BASE + 0x238));

	DIP_Dump_IMGSYS_DIP_Reg();

	cmdq_util_err("MSS Config Info");
	cmdq_util_err("MSSTOP_DBG: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x438), DIP_RD32(MSS_BASE + 0x438),
		(MSS_BASE_HW + 0x448), DIP_RD32(MSS_BASE + 0x448));

	DIP_WR32(MSS_BASE + 0x888, 0x8);

	for (i = 0; i < 24; i++) {
		mfbcmd = i << 8;
		DIP_WR32(MSS_BASE + 0x888, (mfbcmd | 0x8));
		cmdq_util_err("idx:%d cmd:0x%x debug0(0x%x)",
				i, mfbcmd, DIP_RD32(MSS_BASE + 0x440));
	}
	cmdq_util_err("CRSP: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x200), DIP_RD32(MSS_BASE + 0x200),
		(MSS_BASE_HW + 0x204), DIP_RD32(MSS_BASE + 0x204));
	cmdq_util_err("CRSP: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x208), DIP_RD32(MSS_BASE + 0x208),
		(MSS_BASE_HW + 0x20C), DIP_RD32(MSS_BASE + 0x20C));
	cmdq_util_err("CRSP: 0x%x(0x%x)",
		(MSS_BASE_HW + 0x210), DIP_RD32(MSS_BASE + 0x210));
	cmdq_util_err("UNP: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x240), DIP_RD32(MSS_BASE + 0x240),
		(MSS_BASE_HW + 0x244), DIP_RD32(MSS_BASE + 0x244));
	cmdq_util_err("UNP: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x250), DIP_RD32(MSS_BASE + 0x250),
		(MSS_BASE_HW + 0x254), DIP_RD32(MSS_BASE + 0x254));

	cmdq_util_err("YDRZ_0: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x280), DIP_RD32(MSS_BASE + 0x280),
		(MSS_BASE_HW + 0x284), DIP_RD32(MSS_BASE + 0x284));
	cmdq_util_err("YDRZ_0: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x288), DIP_RD32(MSS_BASE + 0x288),
		(MSS_BASE_HW + 0x28C), DIP_RD32(MSS_BASE + 0x28C));
	cmdq_util_err("YDRZ_0: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x290), DIP_RD32(MSS_BASE + 0x290),
		(MSS_BASE_HW + 0x2A4), DIP_RD32(MSS_BASE + 0x2A4));

	cmdq_util_err("YDRZ_1: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x2C0), DIP_RD32(MSS_BASE + 0x2C0),
		(MSS_BASE_HW + 0x2C4), DIP_RD32(MSS_BASE + 0x2C4));
	cmdq_util_err("YDRZ_1: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x2C8), DIP_RD32(MSS_BASE + 0x2C8),
		(MSS_BASE_HW + 0x2CC), DIP_RD32(MSS_BASE + 0x2CC));
	cmdq_util_err("YDRZ_1: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x2D0), DIP_RD32(MSS_BASE + 0x2D0),
		(MSS_BASE_HW + 0x2E4), DIP_RD32(MSS_BASE + 0x2E4));

	cmdq_util_err("CDRZ: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x300), DIP_RD32(MSS_BASE + 0x300),
		(MSS_BASE_HW + 0x304), DIP_RD32(MSS_BASE + 0x304));
	cmdq_util_err("CDRZ: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x308), DIP_RD32(MSS_BASE + 0x308),
		(MSS_BASE_HW + 0x30C), DIP_RD32(MSS_BASE + 0x30C));
	cmdq_util_err("CDRZ: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x310), DIP_RD32(MSS_BASE + 0x310),
		(MSS_BASE_HW + 0x324), DIP_RD32(MSS_BASE + 0x324));

	cmdq_util_err("CRP_0: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x340), DIP_RD32(MSS_BASE + 0x340),
		(MSS_BASE_HW + 0x344), DIP_RD32(MSS_BASE + 0x344));
	cmdq_util_err("CRP_1: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x360), DIP_RD32(MSS_BASE + 0x360),
		(MSS_BASE_HW + 0x364), DIP_RD32(MSS_BASE + 0x364));
	cmdq_util_err("CRP_2: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x380), DIP_RD32(MSS_BASE + 0x380),
		(MSS_BASE_HW + 0x384), DIP_RD32(MSS_BASE + 0x384));
	cmdq_util_err("CRP_3: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x3A0), DIP_RD32(MSS_BASE + 0x3A0),
		(MSS_BASE_HW + 0x3A4), DIP_RD32(MSS_BASE + 0x3A4));
	cmdq_util_err("PAK: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x3C0), DIP_RD32(MSS_BASE + 0x3C0),
		(MSS_BASE_HW + 0x3C8), DIP_RD32(MSS_BASE + 0x3C8));
	cmdq_util_err("PAK: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x03D0), DIP_RD32(MSS_BASE + 0x03D0),
		(MSS_BASE_HW + 0x3D8), DIP_RD32(MSS_BASE + 0x3D8));

	for (loop = 0; loop < (0x64/0x4); loop++) {
		cmdq_util_err("MSSREG: 0x%08X 0x%08X",
			(MSS_BASE_HW + 0x0400) + (loop * 0x4),
			DIP_RD32(MSS_BASE + 0x0400 + (loop * 0x4)));
	}
	cmdq_util_err("MSSCMDQ: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x500), DIP_RD32(MSS_BASE + 0x500),
		(MSS_BASE_HW + 0x504), DIP_RD32(MSS_BASE + 0x504));
	cmdq_util_err("MSSCMDQ: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x508), DIP_RD32(MSS_BASE + 0x508),
		(MSS_BASE_HW + 0x50C), DIP_RD32(MSS_BASE + 0x50C));
	cmdq_util_err("MSSCMDQ: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x510), DIP_RD32(MSS_BASE + 0x510),
		(MSS_BASE_HW + 0x514), DIP_RD32(MSS_BASE + 0x514));
	cmdq_util_err("MSSCQLP: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x600), DIP_RD32(MSS_BASE + 0x600),
		(MSS_BASE_HW + 0x604), DIP_RD32(MSS_BASE + 0x604));
	cmdq_util_err("MSSCQLP: 0x%x(0x%x)\n",
		(MSS_BASE_HW + 0x608), DIP_RD32(MSS_BASE + 0x608));

	cmdq_util_err("MSSDMT: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x800), DIP_RD32(MSS_BASE + 0x800),
		(MSS_BASE_HW + 0x804), DIP_RD32(MSS_BASE + 0x804));
	cmdq_util_err("MSSDMT: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x808), DIP_RD32(MSS_BASE + 0x808),
		(MSS_BASE_HW + 0x80C), DIP_RD32(MSS_BASE + 0x80C));
	cmdq_util_err("MSSDMT: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x810), DIP_RD32(MSS_BASE + 0x810),
		(MSS_BASE_HW + 0x814), DIP_RD32(MSS_BASE + 0x814));
	cmdq_util_err("MSSDMT: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x81C), DIP_RD32(MSS_BASE + 0x81C),
		(MSS_BASE_HW + 0x834), DIP_RD32(MSS_BASE + 0x834));
	cmdq_util_err("MSSDMT: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x838), DIP_RD32(MSS_BASE + 0x838),
		(MSS_BASE_HW + 0x83C), DIP_RD32(MSS_BASE + 0x83C));
	cmdq_util_err("MSSDMT: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x840), DIP_RD32(MSS_BASE + 0x840),
		(MSS_BASE_HW + 0x844), DIP_RD32(MSS_BASE + 0x844));
	cmdq_util_err("MSSDMT: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x850), DIP_RD32(MSS_BASE + 0x850),
		(MSS_BASE_HW + 0x854), DIP_RD32(MSS_BASE + 0x854));
	cmdq_util_err("MSSDMT: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x858), DIP_RD32(MSS_BASE + 0x858),
		(MSS_BASE_HW + 0x85C), DIP_RD32(MSS_BASE + 0x85C));
	cmdq_util_err("MSSDMT: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x884), DIP_RD32(MSS_BASE + 0x884),
		(MSS_BASE_HW + 0x888), DIP_RD32(MSS_BASE + 0x888));
	cmdq_util_err("MSSDMT: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x88C), DIP_RD32(MSS_BASE + 0x88C),
		(MSS_BASE_HW + 0x890), DIP_RD32(MSS_BASE + 0x890));

	cmdq_util_err("MSSDMW_0: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x900), DIP_RD32(MSS_BASE + 0x900),
		(MSS_BASE_HW + 0x904), DIP_RD32(MSS_BASE + 0x904));
	cmdq_util_err("MSSDMW_0: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x908), DIP_RD32(MSS_BASE + 0x908),
		(MSS_BASE_HW + 0x90C), DIP_RD32(MSS_BASE + 0x90C));
	cmdq_util_err("MSSDMW_0: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x910), DIP_RD32(MSS_BASE + 0x910),
		(MSS_BASE_HW + 0x914), DIP_RD32(MSS_BASE + 0x914));
	cmdq_util_err("MSSDMW_0: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x918), DIP_RD32(MSS_BASE + 0x918),
		(MSS_BASE_HW + 0x91C), DIP_RD32(MSS_BASE + 0x91C));

	cmdq_util_err("MSSDMW_1: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x940), DIP_RD32(MSS_BASE + 0x940),
		(MSS_BASE_HW + 0x944), DIP_RD32(MSS_BASE + 0x944));
	cmdq_util_err("MSSDMW_1: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x948), DIP_RD32(MSS_BASE + 0x948),
		(MSS_BASE_HW + 0x94C), DIP_RD32(MSS_BASE + 0x94C));
	cmdq_util_err("MSSDMW_1: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x950), DIP_RD32(MSS_BASE + 0x950),
		(MSS_BASE_HW + 0x954), DIP_RD32(MSS_BASE + 0x954));
	cmdq_util_err("MSSDMW_1: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x958), DIP_RD32(MSS_BASE + 0x958),
		(MSS_BASE_HW + 0x95C), DIP_RD32(MSS_BASE + 0x95C));

	cmdq_util_err("MSSDMW_2: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x980), DIP_RD32(MSS_BASE + 0x980),
		(MSS_BASE_HW + 0x984), DIP_RD32(MSS_BASE + 0x984));
	cmdq_util_err("MSSDMW_2: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x988), DIP_RD32(MSS_BASE + 0x988),
		(MSS_BASE_HW + 0x98C), DIP_RD32(MSS_BASE + 0x98C));
	cmdq_util_err("MSSDMW_2: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x990), DIP_RD32(MSS_BASE + 0x990),
		(MSS_BASE_HW + 0x994), DIP_RD32(MSS_BASE + 0x994));
	cmdq_util_err("MSSDMW_2: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x998), DIP_RD32(MSS_BASE + 0x998),
		(MSS_BASE_HW + 0x99C), DIP_RD32(MSS_BASE + 0x99C));

	cmdq_util_err("MSSDMW_3: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x9C0), DIP_RD32(MSS_BASE + 0x9C0),
		(MSS_BASE_HW + 0x9C4), DIP_RD32(MSS_BASE + 0x9C4));
	cmdq_util_err("MSSDMW_3: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x9C8), DIP_RD32(MSS_BASE + 0x9C8),
		(MSS_BASE_HW + 0x9CC), DIP_RD32(MSS_BASE + 0x9CC));
	cmdq_util_err("MSSDMW_3: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x9D0), DIP_RD32(MSS_BASE + 0x9D0),
		(MSS_BASE_HW + 0x9D4), DIP_RD32(MSS_BASE + 0x9D4));
	cmdq_util_err("MSSDMW_3: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0x9D8), DIP_RD32(MSS_BASE + 0x9D8),
		(MSS_BASE_HW + 0x9DC), DIP_RD32(MSS_BASE + 0x9DC));

	cmdq_util_err("MSSDMR_0: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0xA00), DIP_RD32(MSS_BASE + 0xA00),
		(MSS_BASE_HW + 0xA04), DIP_RD32(MSS_BASE + 0xA04));
	cmdq_util_err("MSSDMR_0: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0xA08), DIP_RD32(MSS_BASE + 0xA08),
		(MSS_BASE_HW + 0xA0C), DIP_RD32(MSS_BASE + 0xA0C));
	cmdq_util_err("MSSDMR_0: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0xA10), DIP_RD32(MSS_BASE + 0xA10),
		(MSS_BASE_HW + 0xA14), DIP_RD32(MSS_BASE + 0xA14));
	cmdq_util_err("MSSDMR_0: 0x%x(0x%x)",
		(MSS_BASE_HW + 0xA18), DIP_RD32(MSS_BASE + 0xA18));

	cmdq_util_err("MSSDMR_1: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0xA40), DIP_RD32(MSS_BASE + 0xA40),
		(MSS_BASE_HW + 0xA44), DIP_RD32(MSS_BASE + 0xA44));
	cmdq_util_err("MSSDMR_1: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0xA48), DIP_RD32(MSS_BASE + 0xA48),
		(MSS_BASE_HW + 0xA4C), DIP_RD32(MSS_BASE + 0xA4C));
	cmdq_util_err("MSSDMR_1: 0x%x(0x%x)-0x%x(0x%x)",
		(MSS_BASE_HW + 0xA50), DIP_RD32(MSS_BASE + 0xA50),
		(MSS_BASE_HW + 0xA54), DIP_RD32(MSS_BASE + 0xA54));
	cmdq_util_err("MSSDMR_1: 0x%x(0x%x)",
		(MSS_BASE_HW + 0xA58), DIP_RD32(MSS_BASE + 0xA58));
	DIP_WR32(MSS_BASE + 0x888, 0x8);

	for (i = 0; i < 3 ; i++) {
		mfbcmd = 0x0;
		mfbcmd |= i << 8;
		DIP_WR32(MSS_BASE + 0x888, mfbcmd);
		cmdq_util_err("mss1 idx:%d cmd:0x%x debug_data(0x%x)",
				i, mfbcmd, DIP_RD32(MSS_BASE + 0x440));
	}
	for (i = 0; i < 6 ; i++) {
		mfbcmd = 0x1;
		mfbcmd |= i << 8;
		DIP_WR32(MSS_BASE + 0x888, mfbcmd);
		cmdq_util_err("mss2 idx:%d cmd:0x%x debug_data(0x%x)",
				i, mfbcmd, DIP_RD32(MSS_BASE + 0x440));
		mfbcmd = 0x2;
		mfbcmd |= i << 8;
		DIP_WR32(MSS_BASE + 0x888, mfbcmd);
		cmdq_util_err("mss2 idx:%d cmd:0x%x debug_data(0x%x)",
				i, mfbcmd, DIP_RD32(MSS_BASE + 0x440));
	}
	for (i = 0; i < 7 ; i++) {
		mfbcmd = 0x3;
		mfbcmd |= i << 8;
		DIP_WR32(MSS_BASE + 0x888, mfbcmd);
		cmdq_util_err("mss3 idx:%d cmd:0x%x debug_data(0x%x)",
				i, mfbcmd, DIP_RD32(MSS_BASE + 0x440));
		mfbcmd = 0x4;
		mfbcmd |= i << 8;
		DIP_WR32(MSS_BASE + 0x888, mfbcmd);
		cmdq_util_err("mss3 idx:%d cmd:0x%x debug_data(0x%x)",
				i, mfbcmd, DIP_RD32(MSS_BASE + 0x440));
		mfbcmd = 0x5;
		mfbcmd |= i << 8;
		DIP_WR32(MSS_BASE + 0x888, mfbcmd);
		cmdq_util_err("mss3 idx:%d cmd:0x%x debug_data(0x%x)",
				i, mfbcmd, DIP_RD32(MSS_BASE + 0x440));
		mfbcmd = 0x6;
		mfbcmd |= i << 8;
		DIP_WR32(MSS_BASE + 0x888, mfbcmd);
		cmdq_util_err("mss3 idx:%d cmd:0x%x debug_data(0x%x)",
				i, mfbcmd, DIP_RD32(MSS_BASE + 0x440));
	}

	for (i = 0; i < 24 ; i++) {
		mfbcmd = 0x8;
		mfbcmd |= i << 8;
		DIP_WR32(MSS_BASE + 0x888, mfbcmd);
		cmdq_util_err("mss4 mod idx:%d cmd:0x%x debug_data(0x%x)",
				i, mfbcmd, DIP_RD32(MSS_BASE + 0x440));
	}

	for (i = 0; i < 4 ; i++) {
		mfbcmd = 0x20;
		mfbcmd |= i;
		DIP_WR32(MSS_BASE + 0x434, mfbcmd);
		cmdq_util_err("mss5 mod idx:%d cmd:0x%x debug_data(0x%x)",
				i, mfbcmd, DIP_RD32(MSS_BASE + 0x440));
		mfbcmd = 0x30;
		mfbcmd |= i;
		DIP_WR32(MSS_BASE + 0x434, mfbcmd);
		cmdq_util_err("mss5 mod idx:%d cmd:0x%x debug_data(0x%x)",
				i, mfbcmd, DIP_RD32(MSS_BASE + 0x440));
	}


	for (i = 0; i < 15 ; i++) {
		mfbcmd = 0x0;
		mfbcmd |= i << 4;
		DIP_WR32(MSS_BASE + 0x434, mfbcmd);
		cmdq_util_err("mss6 idx:%d cmd:0x%x debug_data(0x%x)",
				i, mfbcmd, DIP_RD32(MSS_BASE + 0x444));
	}

	cmdq_util_err("MSS Config Info End");

	cmdq_util_err("MSF Config Info");
	for (i = 0; i < 32 ; i++) {
		mfbcmd = i << 24;
		DIP_WR32(MSF_BASE + 0x4d0, mfbcmd);
		cmdq_util_err("idx:%d cmd:0x%x debug0(0x%x)",
				i, mfbcmd, DIP_RD32(MSF_BASE + 0x4d4));
	}

	DIP_WR32(MSF_BASE + 0x4d0, 0x0);
	for (i = 0; i < 59 ; i++) {
		mfbcmd = 0x11;
		mfbcmd |= (i << 8);
		DIP_WR32(MSF_BASE + 0x888, mfbcmd);
		cmdq_util_err("idx:%d cmd:0x%x debug0(0x%x)",
				i, mfbcmd, DIP_RD32(MSF_BASE + 0x4d4));
	}

	for (i = 0; i < 112 ; i++) {
		mfbcmd = 0x3000000;
		mfbcmd |= (i << 16);
		DIP_WR32(MSF_BASE + 0x4d0, mfbcmd);
		cmdq_util_err("idx:%d cmd:0x%x debug0(0x%x)",
				i, mfbcmd, DIP_RD32(MSF_BASE + 0x4cc));
	}


	for (loop = 0; loop < (0x73C/0x4); loop++) {
		cmdq_util_err("MSFREG: 0x%08X 0x%08X",
			MSF_BASE_HW + (loop * 0x4),
			DIP_RD32(MSF_BASE + (loop * 0x4)));
	}


	for (loop = 0; loop < (0xD0/0x4); loop++) {
		cmdq_util_err("MSFREG: 0x%08X 0x%08X",
			MSF_BASE_HW + 0x7C0 + (loop * 0x4),
			DIP_RD32(MSF_BASE + 0x7C0 + (loop * 0x4)));
	}


	for (loop = 0; loop < (0x398/0x4); loop++) {
		cmdq_util_err("MSFREG: 0x%08X 0x%08X",
			MSF_BASE_HW + 0x900 + (loop * 0x4),
			DIP_RD32(MSF_BASE + 0x900 + (loop * 0x4)));
	}

	for (loop = 0; loop < (0x58/0x4); loop++) {
		cmdq_util_err("MSFREG: 0x%08X 0x%08X",
			MSF_BASE_HW + 0xD00 + (loop * 0x4),
			DIP_RD32(MSF_BASE + 0xD00 + (loop * 0x4)));
	}


	cmdq_util_err("MSF Config Info End\n");

#ifdef AEE_DUMP_REDUCE_MEMORY
	if (g_bDumpPhyDIPBuf == MFALSE) {
		ctrl_start = DIP_RD32(DIP_A_BASE + 0x1000);
		if (g_bIonBufferAllocated == MFALSE) {
			if (g_pPhyDIPBuffer != NULL) {
				cmdq_util_err("g_pPhyDIPBuffer isn't NULL(0x%pK)",
					g_pPhyDIPBuffer);
				vfree(g_pPhyDIPBuffer);
				g_pPhyDIPBuffer = NULL;
			}
			g_pPhyDIPBuffer =
				vmalloc(DIP_REG_RANGE * MTK_DIP_COUNT);
			if (g_pPhyDIPBuffer == NULL)
				CMDQ_ERR("g_pPhyDIPBuffer kmalloc failed");

			if (g_pPhyMFBBuffer != NULL) {
				cmdq_util_err("g_pPhyMFBBuffer isn't NULL(0x%pK)",
					g_pPhyMFBBuffer);
				vfree(g_pPhyMFBBuffer);
				g_pPhyMFBBuffer = NULL;
			}
			g_pPhyMFBBuffer = vmalloc(MFB_REG_RANGE);
			if (g_pPhyMFBBuffer == NULL)
				CMDQ_ERR("g_pPhyMFBBuffer kmalloc failed");

			if (g_pPhyMSSBuffer != NULL) {
				cmdq_util_err("g_pPhyMFBBuffer isn't NULL(0x%pK)",
					g_pPhyMSSBuffer);
				vfree(g_pPhyMSSBuffer);
				g_pPhyMSSBuffer = NULL;
			}
			g_pPhyMSSBuffer = vmalloc(MFB_REG_RANGE);
			if (g_pPhyMSSBuffer == NULL)
				CMDQ_ERR("g_pPhyMSSBuffer kmalloc failed");

			if (g_pKWTpipeBuffer != NULL) {
				cmdq_util_err("g_pKWTpipeBuffer isn't NULL(0x%pK)",
					g_pKWTpipeBuffer);
					vfree(g_pKWTpipeBuffer);
					g_pKWTpipeBuffer = NULL;
			}
			g_pKWTpipeBuffer = vmalloc(MAX_ISP_TILE_TDR_HEX_NO);
			if (g_pKWTpipeBuffer == NULL)
				CMDQ_ERR("g_pKWTpipeBuffer kmalloc failed");

			if (g_pKWCmdqBuffer != NULL) {
				cmdq_util_err("g_KWCmdqBuffer isn't NULL(0x%pK)",
					g_pKWCmdqBuffer);
				vfree(g_pKWCmdqBuffer);
				g_pKWCmdqBuffer = NULL;
			}
			g_pKWCmdqBuffer = vmalloc(MAX_DIP_CMDQ_BUFFER_SIZE);
			if (g_pKWCmdqBuffer == NULL)
				CMDQ_ERR("g_KWCmdqBuffer kmalloc failed");

			if (g_pKWVirDIPBuffer != NULL) {
				cmdq_util_err("g_KWVirDIPBuffer isn't NULL(0x%pK)",
					g_pKWVirDIPBuffer);
				vfree(g_pKWVirDIPBuffer);
				g_pKWVirDIPBuffer = NULL;
			}
			g_pKWVirDIPBuffer = vmalloc(DIP_REG_RANGE);
			if (g_pKWVirDIPBuffer == NULL)
				CMDQ_ERR("g_KWVirDIPBuffer kmalloc failed");
		}

		if (g_pPhyDIPBuffer != NULL) {
			for (i = 0; i < (DIP_REG_RANGE >> 2); i = i + 4) {
				g_pPhyDIPBuffer[i] =
					DIP_RD32(DIP_A_BASE + (i*4));
				g_pPhyDIPBuffer[i+1] =
					DIP_RD32(DIP_A_BASE + ((i+1)*4));
				g_pPhyDIPBuffer[i+2] =
					DIP_RD32(DIP_A_BASE + ((i+2)*4));
				g_pPhyDIPBuffer[i+3] =
					DIP_RD32(DIP_A_BASE + ((i+3)*4));
			}
#if (MTK_DIP_COUNT == 2)
			for (i = 0; i < (DIP_REG_RANGE >> 2); i = i + 4) {
				g_pPhyDIPBuffer[i] =
					DIP_RD32(DIP_B_BASE + (i*4));
				g_pPhyDIPBuffer[i+1] =
					DIP_RD32(DIP_B_BASE + ((i+1)*4));
				g_pPhyDIPBuffer[i+2] =
					DIP_RD32(DIP_B_BASE + ((i+2)*4));
				g_pPhyDIPBuffer[i+3] =
					DIP_RD32(DIP_B_BASE + ((i+3)*4));
			}
#endif
		} else {
			CMDQ_ERR("g_pPhyDIPBuffer:(0x%pK)",
			g_pPhyDIPBuffer);
		}
		if (g_pPhyMFBBuffer != NULL) {
			for (i = 0; i < (MFB_REG_RANGE >> 2); i = i + 4) {
				g_pPhyMFBBuffer[i] =
					DIP_RD32(MSF_BASE + (i*4));
				g_pPhyMFBBuffer[i+1] =
					DIP_RD32(MSF_BASE + ((i+1)*4));
				g_pPhyMFBBuffer[i+2] =
					DIP_RD32(MSF_BASE + ((i+2)*4));
				g_pPhyMFBBuffer[i+3] =
					DIP_RD32(MSF_BASE + ((i+3)*4));
			}
		} else {
			cmdq_util_err("g_pPhyMFBBuffer:(0x%pK)",
			g_pPhyMFBBuffer);
		}
		if (g_pPhyMSSBuffer != NULL) {
			for (i = 0; i < (MSS_REG_RANGE >> 2); i = i + 4) {
				g_pPhyMSSBuffer[i] =
					DIP_RD32(MSS_BASE + (i*4));
				g_pPhyMSSBuffer[i+1] =
					DIP_RD32(MSS_BASE + ((i+1)*4));
				g_pPhyMSSBuffer[i+2] =
					DIP_RD32(MSS_BASE + ((i+2)*4));
				g_pPhyMSSBuffer[i+3] =
					DIP_RD32(MSS_BASE + ((i+3)*4));
			}
		} else {
			cmdq_util_err("g_pPhyMSSBuffer:(0x%pK)",
			g_pPhyMSSBuffer);
		}
		g_dumpInfo.tdri_baseaddr = DIP_RD32(DIP_A_BASE + 0x4);
		g_dumpInfo.imgi_baseaddr = DIP_RD32(DIP_A_BASE + 0x200);
		g_dumpInfo.dmgi_baseaddr = DIP_RD32(DIP_A_BASE + 0x470);
		g_tdriaddr = g_dumpInfo.tdri_baseaddr;
		/*0x15022208, CQ_D1A_CQ_THR0_BASEADDR*/
		/*0x15022220, CQ_D1A_CQ_THR1_BASEADDR*/
		/*0x1502222C, CQ_D1A_CQ_THR2_BASEADDR*/
		if (ctrl_start == 0x1) {
			g_cmdqaddr = DIP_RD32(DIP_A_BASE + 0x1208);
		} else if (ctrl_start == 0x02) {
			g_cmdqaddr = DIP_RD32(DIP_A_BASE + 0x1208 +
					DIP_CMDQ1_TO_CMDQ0_BASEADDR_OFFSET);
		} else {
			for (cmdqidx = 2; cmdqidx < 18; cmdqidx++) {
				if (ctrl_start & (0x1<<cmdqidx)) {
					g_cmdqaddr = DIP_RD32(DIP_A_BASE
						+ 0x1220 +
						((cmdqidx-1)*
						DIP_CMDQ_BASEADDR_OFFSET));
					break;
				}
			}
		}
		g_dumpInfo.cmdq_baseaddr = g_cmdqaddr;
		if ((ctrl_start  != 0) &&
			(g_tdriaddr  != 0) &&
			(g_TpipeBaseAddrInfo.MemPa != 0) &&
			(g_TpipeBaseAddrInfo.MemVa != NULL) &&
			(g_pKWTpipeBuffer != NULL)) {
			/* to get frame tdri baseaddress, */
			/* otherwise you may get one of the tdr bade addr*/
			offset = ((g_tdriaddr &
				(~(g_TpipeBaseAddrInfo.MemSizeDiff-1)))-
				g_TpipeBaseAddrInfo.MemPa);
			OffsetAddr = ((uintptr_t)g_TpipeBaseAddrInfo.MemVa)+
				offset;
			if (copy_from_user(g_pKWTpipeBuffer,
				(void __user *)(OffsetAddr),
				MAX_ISP_TILE_TDR_HEX_NO) != 0) {
				cmdq_util_err("cpy tpipe fail. tdriaddr:0x%x",
				g_tdriaddr);
		cmdq_util_err("MemVa:0x%lx, MemPa:0x%x,offset:0x%x",
				(uintptr_t)g_TpipeBaseAddrInfo.MemVa,
				g_TpipeBaseAddrInfo.MemPa,
				offset);
			}
		}
		cmdq_util_err("tdraddr:0x%x,MemVa:0x%lx,MemPa:0x%x",
			g_tdriaddr,
			(uintptr_t)g_TpipeBaseAddrInfo.MemVa,
			g_TpipeBaseAddrInfo.MemPa);
		cmdq_util_err("MemSizeDiff:0x%x,offset:0x%x",
			g_TpipeBaseAddrInfo.MemSizeDiff, offset);
		cmdq_util_err("g_pKWTpipeBuffer:0x%pK", g_pKWTpipeBuffer);

		if ((ctrl_start  != 0) &&
			(g_cmdqaddr  != 0) &&
			(g_CmdqBaseAddrInfo.MemPa != 0) &&
			(g_CmdqBaseAddrInfo.MemVa != NULL) &&
			(g_pKWCmdqBuffer != NULL) &&
			(g_pKWVirDIPBuffer != NULL)) {
			offset = (g_cmdqaddr-g_CmdqBaseAddrInfo.MemPa);
			OffsetAddr = ((uintptr_t)g_CmdqBaseAddrInfo.MemVa)+
				(g_cmdqaddr-g_CmdqBaseAddrInfo.MemPa);
			if (copy_from_user(g_pKWCmdqBuffer,
				(void __user *)(OffsetAddr),
				MAX_DIP_CMDQ_BUFFER_SIZE) != 0) {
				cmdq_util_err("cpy cmdq fail. cmdqaddr:0x%x",
					g_cmdqaddr);
				cmdq_util_err("MemVa:0x%lx,MemPa:0x%x,offset:0x%x",
					(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
					g_CmdqBaseAddrInfo.MemPa,
					offset);
			}
			cmdq_util_err("cmdqidx:0x%x, cmdqaddr:0x%x",
				cmdqidx,
				g_cmdqaddr);
			cmdq_util_err("MemVa:0x%lx,MemPa:0x%x, offset:0x%x",
				(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
				g_CmdqBaseAddrInfo.MemPa,
				offset);
			offset = offset+g_CmdqBaseAddrInfo.MemSizeDiff;
			OffsetAddr = ((uintptr_t)g_CmdqBaseAddrInfo.MemVa) +
				offset;
			if (copy_from_user(g_pKWVirDIPBuffer,
				(void __user *)(OffsetAddr),
				DIP_REG_RANGE) != 0) {
				cmdq_util_err("cpy vir dip fail.\n");
				cmdq_util_err("cmdqaddr:0x%x,MVa:0x%lx,MPa:0x%x",
				g_cmdqaddr,
				(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
				g_CmdqBaseAddrInfo.MemPa);
				cmdq_util_err("MSzDiff:0x%x,offset:0x%x",
				g_CmdqBaseAddrInfo.MemSizeDiff,
				offset);

			}
			cmdq_util_err("cmdqaddr:0x%x,MVa:0x%lx,MPa:0x%x",
				g_cmdqaddr,
				(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
				g_CmdqBaseAddrInfo.MemPa);
			cmdq_util_err("MSzDiff:0x%x\n",
				g_CmdqBaseAddrInfo.MemSizeDiff);
			cmdq_util_err("ofset:0x%x,KWCmdBuf:0x%pK,KWTdrBuf:0x%pK",
				offset, g_pKWCmdqBuffer, g_pKWTpipeBuffer);
		} else {
			cmdq_util_err("cmdqadd:0x%x,MVa:0x%lx,MPa:0x%x",
				g_cmdqaddr,
				(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
				g_CmdqBaseAddrInfo.MemPa);
			cmdq_util_err("MSzDiff:0x%x,KWCmdBuf:0x%pK,KWTdrBuf:0x%pK",
				g_CmdqBaseAddrInfo.MemSizeDiff,
				g_pKWCmdqBuffer,
				g_pKWTpipeBuffer);
		}
		g_bDumpPhyDIPBuf = MTRUE;
	}
#endif

	cmdq_util_err("direct link: g_bDumpPhyDIPBuf:(0x%x), cmdqidx(0x%x)",
		g_bDumpPhyDIPBuf, cmdqidx);
	cmdq_util_err("direct link: cmdqaddr(0x%x), tdriaddr(0x%x)",
		g_cmdqaddr, g_tdriaddr);
	cmdq_util_err("- X.\n");
#endif
	/*  */
	return Ret;
}

#ifndef CONFIG_MTK_CLKMGR /*CCF*/

static inline void Prepare_Enable_ccf_clock(void)
{
	int ret;
	/* enable through smi API */
	ret = smi_bus_prepare_enable(SMI_LARB9, DIP_DEV_NAME);
	if (ret)
		LOG_ERR("cannot prepare and enable SMI_LARB9! Ret: %d\n",
					(ret));

	ret = clk_prepare_enable(dip_clk.DIP_IMG_LARB9);
	if (ret)
		LOG_ERR("cannot prepare and enable DIP_IMG_LARB9 clock\n");

	ret = clk_prepare_enable(dip_clk.DIP_IMG_DIP);
	if (ret)
		LOG_ERR("cannot prepare and enable DIP_IMG_DIP clock\n");


#if ((MTK_DIP_COUNT == 2) || (MTK_MSF_OFFSET == 1))
	ret = smi_bus_prepare_enable(SMI_LARB11, DIP_DEV_NAME);
	if (ret)
		LOG_ERR("cannot prepare and enable SMI_LARB11! Ret: %d\n",
					(ret));
	LOG_INF("smi_bus_prepare_enable SMI LARB9 LARB11\n");

	ret = clk_prepare_enable(dip_clk.DIP_IMG_LARB11);
	if (ret)
		LOG_ERR("cannot prepare and enable DIP_IMG_LARB11 clock\n");
#endif

#if (MTK_DIP_COUNT == 2)
	ret = clk_prepare_enable(dip_clk.DIP_IMG_DIP2);
	if (ret)
		LOG_ERR("cannot prepare and enable DIP_IMG_DIP clock\n");
#endif

	ret = clk_prepare_enable(dip_clk.DIP_IMG_DIP_MSS);
	if (ret)
		LOG_ERR("cannot prepare and enable DIP_IMG_DIP_MSS clock\n");

	ret = clk_prepare_enable(dip_clk.DIP_IMG_MFB_DIP);
	if (ret)
		LOG_ERR("cannot prepare and enable DIP_IMG_MFB_DIP clock\n");


}

static inline void Disable_Unprepare_ccf_clock(void)
{
	clk_disable_unprepare(dip_clk.DIP_IMG_MFB_DIP);
	clk_disable_unprepare(dip_clk.DIP_IMG_DIP_MSS);
	clk_disable_unprepare(dip_clk.DIP_IMG_DIP);
	clk_disable_unprepare(dip_clk.DIP_IMG_LARB9);

	smi_bus_disable_unprepare(SMI_LARB9, DIP_DEV_NAME);

#if (MTK_DIP_COUNT == 2)
	clk_disable_unprepare(dip_clk.DIP_IMG_DIP2);
#endif

#if ((MTK_DIP_COUNT == 2) || (MTK_MSF_OFFSET == 1))

	clk_disable_unprepare(dip_clk.DIP_IMG_LARB11);

	smi_bus_disable_unprepare(SMI_LARB11, DIP_DEV_NAME);
	LOG_INF("smi_bus_disable_unprepare SMI LARB 9 LARB11\n");
#endif

}

#endif

#ifdef CONFIG_MTK_IOMMU_V2
static inline int m4u_control_iommu_port(void)
{
	struct M4U_PORT_STRUCT sPort;
	int ret = 0;

	/* LARB9 */
	int count_of_ports = 0;
	int i = 0;

#if (MTK_MSF_OFFSET == 1)

	/* LARB13 config all ports */
	count_of_ports = M4U_PORT_L9_IMG_UFBC_R0 -
		M4U_PORT_L9_IMG_IMGI_D1 + 1;
	for (i = 0; i < count_of_ports; i++) {
		sPort.ePortID = M4U_PORT_L9_IMG_IMGI_D1+i;
		sPort.Virtuality = DIP_MEM_USE_VIRTUL;
		//LOG_DBG("config M4U Port ePortID=%d\n", sPort.ePortID);
	#if defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U)
		ret = m4u_config_port(&sPort);

		if (ret == 0) {
			//LOG_DBG("config M4U Port %s to %s SUCCESS\n",
			//iommu_get_port_name(M4U_PORT_L9_IMG_IMGI_D1+i),
			//DIP_MEM_USE_VIRTUL ? "virtual" : "physical");
		} else {
			LOG_INF("config M4U Port %s to %s FAIL(ret=%d)\n",
			iommu_get_port_name(M4U_PORT_L9_IMG_IMGI_D1+i),
			DIP_MEM_USE_VIRTUL ? "virtual" : "physical", ret);
			ret = -1;
		}
	#endif


	}

	count_of_ports = M4U_PORT_L11_IMG_MFB_WDMA1 -
		M4U_PORT_L11_IMG_MFB_RDMA0 + 1;
	for (i = 0; i < count_of_ports; i++) {
		sPort.ePortID = M4U_PORT_L11_IMG_MFB_RDMA0+i;
		sPort.Virtuality = DIP_MEM_USE_VIRTUL;
		//LOG_DBG("config M4U Port ePortID=%d\n", sPort.ePortID);
#if defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U)
		ret = m4u_config_port(&sPort);

		if (ret == 0) {
			//LOG_DBG("config M4U Port %s to %s SUCCESS\n",
			//iommu_get_port_name(M4U_PORT_L11_IMG_MFB_RDMA0+i),
			//DIP_MEM_USE_VIRTUL ? "virtual" : "physical");
		} else {
			LOG_INF("config M4U Port %s to %s FAIL(ret=%d)\n",
			iommu_get_port_name(M4U_PORT_L11_IMG_MFB_RDMA0+i),
			DIP_MEM_USE_VIRTUL ? "virtual" : "physical", ret);
			ret = -1;
		}
#endif
	}

	/* LARB11 */
		count_of_ports = M4U_PORT_L11_IMG_UFBC_R0 -
		M4U_PORT_L11_IMG_IMGI_D1 + 1;
	for (i = 0; i < count_of_ports; i++) {
		sPort.ePortID = M4U_PORT_L11_IMG_IMGI_D1+i;
		sPort.Virtuality = DIP_MEM_USE_VIRTUL;
		//LOG_DBG("config M4U Port ePortID=%d\n", sPort.ePortID);
	#if defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U)
		ret = m4u_config_port(&sPort);

		if (ret == 0) {
			//LOG_DBG("config M4U Port %s to %s SUCCESS\n",
			//iommu_get_port_name(M4U_PORT_L11_IMG_IMGI_D1+i),
			//DIP_MEM_USE_VIRTUL ? "virtual" : "physical");
		} else {
			LOG_INF("config M4U Port %s to %s FAIL(ret=%d)\n",
			iommu_get_port_name(M4U_PORT_L11_IMG_IMGI_D1+i),
			DIP_MEM_USE_VIRTUL ? "virtual" : "physical", ret);
			ret = -1;
		}
	#endif

	}

#else
	/* LARB13 config all ports */
	count_of_ports = M4U_PORT_L9_IMG_UFBC_R0_MDP -
		M4U_PORT_L9_IMG_IMGI_D1_MDP + 1;
	for (i = 0; i < count_of_ports; i++) {
		sPort.ePortID = M4U_PORT_L9_IMG_IMGI_D1_MDP+i;
		sPort.Virtuality = DIP_MEM_USE_VIRTUL;
		//LOG_DBG("config M4U Port ePortID=%d\n", sPort.ePortID);
	#if defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U)
		ret = m4u_config_port(&sPort);

		if (ret == 0) {
			//LOG_DBG("config M4U Port %s to %s SUCCESS\n",
			//iommu_get_port_name(M4U_PORT_L9_IMG_IMGI_D1_MDP+i),
			//DIP_MEM_USE_VIRTUL ? "virtual" : "physical");
		} else {
			LOG_INF("config M4U Port %s to %s FAIL(ret=%d)\n",
			iommu_get_port_name(M4U_PORT_L9_IMG_IMGI_D1_MDP+i),
			DIP_MEM_USE_VIRTUL ? "virtual" : "physical", ret);
			ret = -1;
		}
	#endif


	}
	count_of_ports = M4U_PORT_L9_IMG_MFB_WDMA1_MDP -
		M4U_PORT_L9_IMG_MFB_RDMA0_MDP + 1;
	for (i = 0; i < count_of_ports; i++) {
		sPort.ePortID = M4U_PORT_L9_IMG_MFB_RDMA0_MDP+i;
		sPort.Virtuality = DIP_MEM_USE_VIRTUL;
		//LOG_DBG("config M4U Port ePortID=%d\n", sPort.ePortID);
#if defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U)
		ret = m4u_config_port(&sPort);

		if (ret == 0) {
			//LOG_DBG("config M4U Port %s to %s SUCCESS\n",
			//iommu_get_port_name(M4U_PORT_L9_IMG_MFB_RDMA0_MDP+i),
			//DIP_MEM_USE_VIRTUL ? "virtual" : "physical");
		} else {
			LOG_INF("config M4U Port %s to %s FAIL(ret=%d)\n",
			iommu_get_port_name(M4U_PORT_L9_IMG_MFB_RDMA0_MDP+i),
			DIP_MEM_USE_VIRTUL ? "virtual" : "physical", ret);
			ret = -1;
		}
#endif
	}
	/* LARB11 */
		count_of_ports = M4U_PORT_L11_IMG_UFBC_R0_DISP -
		M4U_PORT_L11_IMG_IMGI_D1_DISP + 1;
	for (i = 0; i < count_of_ports; i++) {
		sPort.ePortID = M4U_PORT_L11_IMG_IMGI_D1_DISP+i;
		sPort.Virtuality = DIP_MEM_USE_VIRTUL;
		//LOG_DBG("config M4U Port ePortID=%d\n", sPort.ePortID);
	#if defined(CONFIG_MTK_M4U) || defined(CONFIG_MTK_PSEUDO_M4U)
		ret = m4u_config_port(&sPort);

		if (ret == 0) {
			//LOG_DBG("config M4U Port %s to %s SUCCESS\n",
			//iommu_get_port_name(M4U_PORT_L11_IMG_IMGI_D1_DISP+i),
			//DIP_MEM_USE_VIRTUL ? "virtual" : "physical");
		} else {
			LOG_INF("config M4U Port %s to %s FAIL(ret=%d)\n",
			iommu_get_port_name(M4U_PORT_L11_IMG_IMGI_D1_DISP+i),
			DIP_MEM_USE_VIRTUL ? "virtual" : "physical", ret);
			ret = -1;
		}
	#endif

	}


#endif

	return ret;
}
#endif


/**************************************************************
 *
 **************************************************************/
static void DIP_EnableClock(bool En)
{
#if defined(EP_NO_CLKMGR)
	unsigned int setReg;
#endif

#ifdef CONFIG_MTK_IOMMU_V2
	int ret = 0;
#endif

	if (En) {
#if defined(EP_NO_CLKMGR)
		LOG_ERR("[Debug] It's LDVT load, EP_NO_CLKMGR");
		spin_lock(&(IspInfo.SpinLockClock));
		/* LOG_DBG("Camera clock enbled. G_u4DipEnClkCnt: %d.", */
		/* G_u4DipEnClkCnt); */
		switch (G_u4DipEnClkCnt) {
		case 0:
			/* Enable clock by hardcode*/
			setReg = 0xFFFFFFFF;
			/*DIP_WR32(CAMSYS_REG_CG_CLR, setReg);*/
			DIP_WR32(IMGSYS_REG_CG_CLR, setReg);
#if (MTK_DIP_COUNT == 2)
			DIP_WR32(IMGSYS2_REG_CG_CLR, setReg);
#endif

			break;
		default:
			break;
		}
		G_u4DipEnClkCnt++;
		spin_unlock(&(IspInfo.SpinLockClock));

#ifdef CONFIG_MTK_IOMMU_V2
		if (G_u4DipEnClkCnt == 1) {
			ret = m4u_control_iommu_port();
			if (ret)
				LOG_ERR("cannot config M4U IOMMU PORTS\n");
		}
#endif

#else/*CCF*/
		/*LOG_INF("CCF:prepare_enable clk");*/
		Prepare_Enable_ccf_clock(); /* !!cannot be used in spinlock!! */
		spin_lock(&(IspInfo.SpinLockClock));
		G_u4DipEnClkCnt++;
		spin_unlock(&(IspInfo.SpinLockClock));

#ifdef CONFIG_MTK_IOMMU_V2
		if (G_u4DipEnClkCnt == 1) {
			ret = m4u_control_iommu_port();
			if (ret)
				LOG_ERR("cannot config M4U IOMMU PORTS\n");
		}
#endif

#endif
	} else {                /* Disable clock. */
#if defined(EP_NO_CLKMGR)
		spin_lock(&(IspInfo.SpinLockClock));
		/* LOG_DBG("Camera clock disabled. G_u4DipEnClkCnt: %d.", */
		/* G_u4DipEnClkCnt); */
		G_u4DipEnClkCnt--;
		switch (G_u4DipEnClkCnt) {
		case 0:
			/* Disable clock by hardcode:
			 * 1. CAMSYS_CG_SET (0x1A000004) = 0xffffffff;
			 * 2. IMG_CG_SET (0x15000004) = 0xffffffff;
			 */
			setReg = 0xFFFFFFFF;
			/*DIP_WR32(CAMSYS_REG_CG_SET, setReg);*/
			/*DIP_WR32(IMGSYS_REG_CG_SET, setReg);*/
			break;
		default:
			break;
		}
		spin_unlock(&(IspInfo.SpinLockClock));
#else
		/*LOG_INF("CCF:disable_unprepare clk\n");*/
		Disable_Unprepare_ccf_clock();
		spin_lock(&(IspInfo.SpinLockClock));
		G_u4DipEnClkCnt--;
		spin_unlock(&(IspInfo.SpinLockClock));
		/* !!cannot be used in spinlock!! */
#endif
	}
}



/**************************************************************
 *
 **************************************************************/
static inline void DIP_Reset(signed int module)
{
	/*    unsigned int Reg;*/
	/*    unsigned int setReg;*/

	LOG_DBG("- E.\n");

	//LOG_DBG(" Reset module(%d), IMGSYS clk gate(0x%x)\n",
	//	module, DIP_RD32(IMGSYS_REG_CG_CON));

	switch (module) {
	case DIP_DIP_A_IDX: {

		/* Reset DIP flow */

		break;
	}
	default:
		LOG_ERR("Not support reset module:%d\n", module);
		break;
	}
}

/**************************************************************
 *
 **************************************************************/
static signed int DIP_ReadReg(struct DIP_REG_IO_STRUCT *pRegIo)
{
	unsigned int i;
	signed int Ret = 0;

	struct DIP_REG_STRUCT *pData = NULL;
	struct DIP_REG_STRUCT *pTmpData = NULL;

	if ((pRegIo->pData == NULL) ||
		(pRegIo->Count == 0) ||
		(pRegIo->Count > (DIP_REG_RANGE>>2))) {
		LOG_INF("%s pRegIo->pData is NULL, Count:%d!!", __func__,
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	pData = kmalloc((pRegIo->Count) * sizeof(struct DIP_REG_STRUCT),
		GFP_KERNEL);
	if (pData == NULL) {
		LOG_INF("ERROR: %s kmalloc failed, cnt:%d\n", __func__,
			pRegIo->Count);
		Ret = -ENOMEM;
		goto EXIT;
	}
	pTmpData = pData;

	if (copy_from_user(pData,
		(void *)pRegIo->pData,
		(pRegIo->Count) * sizeof(struct DIP_REG_STRUCT)) == 0) {
		for (i = 0; i < pRegIo->Count; i++) {
			if ((DIP_A_BASE + pData->Addr >= DIP_A_BASE)
			    && (pData->Addr < DIP_REG_RANGE)
				&& ((pData->Addr & 0x3) == 0)) {
				pData->Val = DIP_RD32(DIP_A_BASE + pData->Addr);
			} else {
				LOG_INF("Wrong address(0x%p)\n",
					DIP_A_BASE + pData->Addr);
				LOG_INF("DIP_BASE(0x%p), Addr(0x%lx)\n",
					DIP_A_BASE,
					(unsigned long)pData->Addr);
				pData->Val = 0;
			}
			pData++;
		}
		pData = pTmpData;
		if (copy_to_user((void *)pRegIo->pData,
			pData,
			(pRegIo->Count)*sizeof(struct DIP_REG_STRUCT)) != 0) {
			LOG_INF("copy_to_user failed\n");
			Ret = -EFAULT;
			goto EXIT;
		}
	} else {
		LOG_INF("DIP_READ_REGISTER copy_from_user failed");
		Ret = -EFAULT;
		goto EXIT;
	}
EXIT:
	if (pData != NULL) {
		kfree(pData);
		pData = NULL;
	}

	return Ret;
}


/**************************************************************
 *
 **************************************************************/
/*
static signed int DIP_WriteRegToHw(
	struct DIP_REG_STRUCT *pReg,
	unsigned int         Count)
{
	signed int Ret = 0;
	unsigned int i;
	bool dbgWriteReg;
	unsigned int module;
	void __iomem *regBase;

	// Use local variable to store IspInfo.DebugMask &
	// DIP_DBG_WRITE_REG for saving lock time
	spin_lock(&(IspInfo.SpinLockIsp));
	dbgWriteReg = IspInfo.DebugMask & DIP_DBG_WRITE_REG;
	spin_unlock(&(IspInfo.SpinLockIsp));

	module = pReg->module;


	switch (module) {
	case DIP_DIP_A_IDX:
		regBase = DIP_A_BASE;
		break;
	default:
		LOG_ERR("Unsupported module(%x) !!!\n", module);
		return -EFAULT;
	}

	if (dbgWriteReg)
		LOG_DBG("- E.\n");


	for (i = 0; i < Count; i++) {
		if (dbgWriteReg)
			LOG_DBG("module(%d), base(0x%lx)",
			module,
			(unsigned long)regBase);
			LOG_DBG("Addr(0x%lx), Val(0x%x)\n",
			(unsigned long)(pReg[i].Addr),
			(unsigned int)(pReg[i].Val));
		if (((regBase + pReg[i].Addr) < (regBase + PAGE_SIZE))
			&& ((pReg[i].Addr & 0x3) == 0))
			DIP_WR32(regBase + pReg[i].Addr, pReg[i].Val);
		else
			LOG_ERR("wrong address(0x%lx)\n",
				(unsigned long)(regBase + pReg[i].Addr));

	}


	return Ret;
}
*/


/**************************************************************
 *
 **************************************************************/
/*
static signed int DIP_WriteReg(struct DIP_REG_IO_STRUCT *pRegIo)
{
	signed int Ret = 0;

	struct DIP_REG_STRUCT *pData = NULL;


	if (IspInfo.DebugMask & DIP_DBG_WRITE_REG)
		LOG_DBG("Data(0x%p), Count(%d)\n",
			(pRegIo->pData),
			(pRegIo->Count));

	if ((pRegIo->pData == NULL) ||
		(pRegIo->Count == 0) ||
		(pRegIo->Count > (DIP_REG_RANGE>>2))) {
		LOG_INF("ERROR: pRegIo->pData is NULL or Count:%d\n",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	pData = kmalloc((pRegIo->Count) *
		sizeof(struct DIP_REG_STRUCT),
		GFP_KERNEL);
	if (pData == NULL) {
		LOG_INF("ERROR: kmalloc failed");
		LOG_INF("(process, pid, tgid)=(%s, %d, %d)\n",
		current->comm,
		current->pid,
		current->tgid);
	Ret = -ENOMEM;
	goto EXIT;
	}

	if (copy_from_user(pData,
		(void __user *)(pRegIo->pData),
		pRegIo->Count * sizeof(struct DIP_REG_STRUCT)) != 0) {
		LOG_INF("copy_from_user failed\n");
		Ret = -EFAULT;
		goto EXIT;
	}

	Ret = DIP_WriteRegToHw(
		      pData,
		      pRegIo->Count);

EXIT:
	if (pData != NULL) {
		kfree(pData);
		pData = NULL;
	}
	return Ret;
}
*/

/**************************************************************
 *
 **************************************************************/
#ifdef AEE_DUMP_BY_USING_ION_MEMORY
static signed int dip_allocbuf(struct dip_imem_memory *pMemInfo)
{
	int ret = 0;
	struct ion_mm_data mm_data;
	struct ion_handle *handle = NULL;

	if (pMemInfo == NULL) {
		LOG_ERR("pMemInfo is NULL!!\n");
		ret = -ENOMEM;
		goto dip_allocbuf_exit;
	}
	if (dip_p2_ion_client == NULL) {
		LOG_ERR("dip_p2_ion_client is NULL!!\n");
		ret = -ENOMEM;
		goto dip_allocbuf_exit;
	}
	handle = ion_alloc(dip_p2_ion_client,
		pMemInfo->length,
		0,
		ION_HEAP_MULTIMEDIA_MASK,
		0);

	if (handle == NULL) {
		LOG_ERR("fail to alloc ion buffer, ret=%d\n", ret);
		ret = -ENOMEM;
		goto dip_allocbuf_exit;
	}
	pMemInfo->handle = (void *) handle;
	pMemInfo->va = (uintptr_t) ion_map_kernel(dip_p2_ion_client, handle);
	if (pMemInfo->va == 0) {
		LOG_ERR("fail to map va of buffer!\n");
		ret = -ENOMEM;
		goto dip_allocbuf_exit;
	}

	/* use get_iova replace config_buffer & get_phys*/
	memset((void *)&mm_data, 0, sizeof(struct ion_mm_data));
	mm_data.mm_cmd = ION_MM_GET_IOVA;
	mm_data.get_phys_param.kernel_handle = handle;
    /* should use Your HW port id, please don't use other's port id */
	mm_data.get_phys_param.module_id = 0;
	mm_data.get_phys_param.coherent = 1;
	ret = ion_kernel_ioctl(dip_p2_ion_client,
		ION_CMD_MULTIMEDIA,
		(unsigned long)&mm_data);

	if (ret) {
		LOG_ERR("fail to get mva, ret=%d\n", ret);
		ret = -ENOMEM;
		goto dip_allocbuf_exit;
	}
	pMemInfo->pa = mm_data.get_phys_param.phy_addr;
dip_allocbuf_exit:
	if (ret < 0) {
		if (handle)
			ion_free(dip_p2_ion_client, handle);
	}
	return ret;
}

/**************************************************************
 *
 **************************************************************/
static void dip_freebuf(struct dip_imem_memory *pMemInfo)
{
	struct ion_handle *handle;

	if (pMemInfo == NULL) {
		LOG_ERR("pMemInfo is NULL!!\n");
		return;
	}
	handle = (struct ion_handle *) pMemInfo->handle;
	if (handle != NULL) {
		ion_unmap_kernel(dip_p2_ion_client, handle);
		ion_free(dip_p2_ion_client, handle);
	}
}
#endif

/**************************************************************
 *
 **************************************************************/
static signed int DIP_DumpBuffer
	(struct DIP_DUMP_BUFFER_STRUCT *pDumpBufStruct)
{
	signed int Ret = 0;

	if (pDumpBufStruct->BytesofBufferSize > 0xFFFFFFFF) {
		LOG_ERR("pDumpTuningBufStruct->BytesofBufferSize error");
		Ret = -EFAULT;
		goto EXIT;
	}
	/*  */
	if ((void __user *)(pDumpBufStruct->pBuffer) == NULL) {
		LOG_ERR("NULL pDumpBufStruct->pBuffer");
		Ret = -EFAULT;
		goto EXIT;
	}
    /* Native Exception */
	switch (pDumpBufStruct->DumpCmd) {
	case DIP_DUMP_TPIPEBUF_CMD:
		if (pDumpBufStruct->BytesofBufferSize >
			MAX_ISP_TILE_TDR_HEX_NO) {
			LOG_ERR("tpipe size error");
			Ret = -EFAULT;
			goto EXIT;
		}
#ifdef AEE_DUMP_REDUCE_MEMORY
		if (g_bIonBufferAllocated == MFALSE) {
			if (g_pTpipeBuffer == NULL)
				g_pTpipeBuffer =
				vmalloc(MAX_ISP_TILE_TDR_HEX_NO);
			else
				LOG_ERR("g_pTpipeBuffer:0x%pK is not NULL!!",
				g_pTpipeBuffer);
		}
		if (g_pTpipeBuffer != NULL) {
			if (copy_from_user(g_pTpipeBuffer,
				(void __user *)(pDumpBufStruct->pBuffer),
				pDumpBufStruct->BytesofBufferSize) != 0) {
				LOG_ERR("copy g_pTpipeBuffer failed\n");
				Ret = -EFAULT;
				goto EXIT;
			}
		} else {
			LOG_ERR("g_pTpipeBuffer kmalloc failed,");
			LOG_ERR("g_bIonBufAllocated:%d\n",
			g_bIonBufferAllocated);
		}
#endif
		LOG_INF("copy dumpbuf::0x%p tpipebuf:0x%p is done!!\n",
			pDumpBufStruct->pBuffer,
			g_pTpipeBuffer);
		DumpBufferField = DumpBufferField | 0x1;
		break;
	case DIP_DUMP_TUNINGBUF_CMD:
		if (pDumpBufStruct->BytesofBufferSize > DIP_REG_RANGE) {
			LOG_ERR("tuning buf size error, size:0x%x",
				pDumpBufStruct->BytesofBufferSize);
			Ret = -EFAULT;
			goto EXIT;
		}
#ifdef AEE_DUMP_REDUCE_MEMORY
		if (g_bIonBufferAllocated == MFALSE) {
			if (g_pTuningBuffer == NULL)
				g_pTuningBuffer = vmalloc(DIP_REG_RANGE);
			else
				LOG_ERR("g_TuningBuffer:0x%pK is not NULL!!",
				g_pTuningBuffer);
		}
		if (g_pTuningBuffer != NULL) {
			if (copy_from_user(g_pTuningBuffer,
				(void __user *)(pDumpBufStruct->pBuffer),
				pDumpBufStruct->BytesofBufferSize) != 0) {
				LOG_ERR("copy g_pTuningBuffer failed\n");
				Ret = -EFAULT;
				goto EXIT;
			}
		} else {
			LOG_ERR("ERROR: g_TuningBuffer kmalloc failed\n");
		}
#endif
		LOG_INF("copy dumpbuf::0x%p tuningbuf:0x%p is done!!\n",
			pDumpBufStruct->pBuffer,
			g_pTuningBuffer);
		DumpBufferField = DumpBufferField | 0x2;
		break;
	case DIP_DUMP_DIPVIRBUF_CMD:
		if (pDumpBufStruct->BytesofBufferSize > DIP_REG_RANGE) {
			LOG_ERR("vir dip buffer size error, size:0x%x",
				pDumpBufStruct->BytesofBufferSize);
			Ret = -EFAULT;
			goto EXIT;
		}
#ifdef AEE_DUMP_REDUCE_MEMORY
		if (g_bIonBufferAllocated == MFALSE) {
			if (g_pVirDIPBuffer == NULL)
				g_pVirDIPBuffer = vmalloc(DIP_REG_RANGE);
			else
				LOG_ERR("g_pVirDIPBuffer:0x%pK is not NULL!!",
				g_pVirDIPBuffer);
		}
		if (g_pVirDIPBuffer != NULL) {
			if (copy_from_user(g_pVirDIPBuffer,
				(void __user *)(pDumpBufStruct->pBuffer),
				pDumpBufStruct->BytesofBufferSize) != 0) {
				LOG_ERR("copy g_pVirDIPBuffer failed\n");
				Ret = -EFAULT;
				goto EXIT;
			}
		} else {
			LOG_ERR("ERROR: g_pVirDIPBuffer kmalloc failed\n");
		}
#endif
		LOG_INF("copy dumpbuf::0x%p virdipbuf:0x%p is done!!\n",
			pDumpBufStruct->pBuffer,
			g_pVirDIPBuffer);
		DumpBufferField = DumpBufferField | 0x4;
		break;
	case DIP_DUMP_CMDQVIRBUF_CMD:
		if (pDumpBufStruct->BytesofBufferSize >
			MAX_DIP_CMDQ_BUFFER_SIZE) {
			LOG_ERR("cmdq buffer size error, size:0x%x",
				pDumpBufStruct->BytesofBufferSize);
			Ret = -EFAULT;
			goto EXIT;
		}
#ifdef AEE_DUMP_REDUCE_MEMORY
		if (g_bIonBufferAllocated == MFALSE) {
			if (g_pCmdqBuffer == NULL)
				g_pCmdqBuffer =
				vmalloc(MAX_DIP_CMDQ_BUFFER_SIZE);
			else
				LOG_ERR("g_pCmdqBuffer:0x%pK is not NULL!!",
					g_pCmdqBuffer);
		}
		if (g_pCmdqBuffer != NULL) {
			if (copy_from_user(g_pCmdqBuffer,
				(void __user *)(pDumpBufStruct->pBuffer),
				pDumpBufStruct->BytesofBufferSize) != 0) {
				LOG_ERR("copy g_pCmdqBuffer failed\n");
				Ret = -EFAULT;
				goto EXIT;
			}
		} else {
			LOG_ERR("ERROR: g_pCmdqBuffer kmalloc failed\n");
		}
#endif
		LOG_INF("copy dumpbuf::0x%p cmdqbuf:0x%p is done!!\n",
			pDumpBufStruct->pBuffer,
			g_pCmdqBuffer);
		DumpBufferField = DumpBufferField | 0x8;
		break;
	default:
		LOG_ERR("error dump buffer cmd:%d", pDumpBufStruct->DumpCmd);
		break;
	}
	if (g_bUserBufIsReady == MFALSE) {
		if ((DumpBufferField & 0xf) == 0xf) {
			g_bUserBufIsReady = MTRUE;
			DumpBufferField = 0;
			LOG_INF("DumpBufferField:0x%x,g_bUserBufIsReady:%d!!\n",
				DumpBufferField, g_bUserBufIsReady);
		}
	}
	/*  */
EXIT:
	return Ret;
}

/**************************************************************
 *
 **************************************************************/
static signed int DIP_SetMemInfo
	(struct DIP_MEM_INFO_STRUCT *pMemInfoStruct)
{
	signed int Ret = 0;
	/*  */
	if ((void __user *)(pMemInfoStruct->MemVa) == NULL) {
		LOG_ERR("NULL pMemInfoStruct->MemVa");
		Ret = -EFAULT;
		goto EXIT;
	}
	switch (pMemInfoStruct->MemInfoCmd) {
	case DIP_MEMORY_INFO_TPIPE_CMD:
		memcpy(&g_TpipeBaseAddrInfo,
			pMemInfoStruct,
			sizeof(struct DIP_MEM_INFO_STRUCT));
		LOG_INF("set tpipe memory info is done!!\n");
		break;
	case DIP_MEMORY_INFO_CMDQ_CMD:
		memcpy(&g_CmdqBaseAddrInfo,
			pMemInfoStruct,
			sizeof(struct DIP_MEM_INFO_STRUCT));
		LOG_INF("set comq memory info is done!!\n");
		break;
	default:
		LOG_ERR("error set memory info cmd:%d",
			pMemInfoStruct->MemInfoCmd);
		break;
	}
	/*  */
EXIT:

	return Ret;
}

/**************************************************************
 *
 **************************************************************/

/*  */
/* isr dbg log , sw isr response counter , +1 when sw receive 1 sof isr. */
int DIP_Vsync_cnt[2] = {0, 0};

/**************************************************************
 * update current idnex to working frame
 **************************************************************/
static signed int DIP_P2_BufQue_Update_ListCIdx
	(enum DIP_P2_BUFQUE_PROPERTY enum_property,
	enum DIP_P2_BUFQUE_LIST_TAG listTag)
{
	signed int ret = 0;
	signed int tmpIdx = 0;
	signed int cnt = 0;
	bool stop = false;
	int i = 0;
	unsigned int property = 0;
	enum DIP_P2_BUF_STATE_ENUM cIdxSts = DIP_P2_BUF_STATE_NONE;

	if (enum_property >= DIP_P2_BUFQUE_PROPERTY_NUM ||
		enum_property < DIP_P2_BUFQUE_PROPERTY_DIP) {
		LOG_ERR("property err(%d)\n", enum_property);
		return -EINVAL;
	}

	property = (unsigned int)enum_property;

	switch (listTag) {
	case DIP_P2_BUFQUE_LIST_TAG_UNIT:
		/* [1] check global pointer current sts */
		cIdxSts = P2_FrameUnit_List[property]
			[P2_FrameUnit_List_Idx[property].curr].bufSts;
/* /////////////////////////////////////////////////////////// */
/* Assume we have the buffer list in the following situation */
/* ++++++         ++++++         ++++++ */
/* +  vss +         +  prv +         +  prv + */
/* ++++++         ++++++         ++++++ */
/* not deque         erased           enqued */
/* done */
/*  */
/* if the vss deque is done, we should update the CurBufIdx */
/* to the next enqued buffer node instead of */
/* moving to the next buffer node */
/* /////////////////////////////////////////////////////////// */
/* [2]traverse count needed */
		if (P2_FrameUnit_List_Idx[property].start <=
			P2_FrameUnit_List_Idx[property].end) {
			cnt = P2_FrameUnit_List_Idx[property].end -
				P2_FrameUnit_List_Idx[property].start;
		} else {
			cnt = _MAX_SUPPORT_P2_FRAME_NUM_ -
				P2_FrameUnit_List_Idx[property].start;
			cnt += P2_FrameUnit_List_Idx[property].end;
		}

		/* [3] update current index for frame unit list */
		tmpIdx = P2_FrameUnit_List_Idx[property].curr;
		switch (cIdxSts) {
		case DIP_P2_BUF_STATE_ENQUE:
			P2_FrameUnit_List[property]
				[P2_FrameUnit_List_Idx[property].curr].bufSts =
				DIP_P2_BUF_STATE_RUNNING;
			break;
		case DIP_P2_BUF_STATE_DEQUE_SUCCESS:
		do { /* to find the newest cur index */
			tmpIdx = (tmpIdx + 1) % _MAX_SUPPORT_P2_FRAME_NUM_;
			switch (P2_FrameUnit_List[property][tmpIdx].bufSts) {
			case DIP_P2_BUF_STATE_ENQUE:
			case DIP_P2_BUF_STATE_RUNNING:
				P2_FrameUnit_List[property][tmpIdx].bufSts =
					DIP_P2_BUF_STATE_RUNNING;
				P2_FrameUnit_List_Idx[property].curr = tmpIdx;
				stop = true;
				break;
			case DIP_P2_BUF_STATE_WAIT_DEQUE_FAIL:
			case DIP_P2_BUF_STATE_DEQUE_SUCCESS:
			case DIP_P2_BUF_STATE_DEQUE_FAIL:
			case DIP_P2_BUF_STATE_NONE:
			default:
				break;
			}
			i++;
		} while ((i < cnt) && (!stop));
/* /////////////////////////////////////////////////////////// */
/* Assume we have the buffer list in the following situation */
/* ++++++         ++++++         ++++++ */
/* +  vss +         +  prv +         +  prv + */
/* ++++++         ++++++         ++++++ */
/* not deque         erased           erased */
/* done */
/*  */
/* all the buffer node are deque done in the current moment, */
/* should update current index to the last node  */
/* if the vss deque is done, we should update the CurBufIdx */
/* to the last buffer node  */
/* /////////////////////////////////////////////////////////// */
		if ((!stop) && (i == (cnt)))
			P2_FrameUnit_List_Idx[property].curr =
			P2_FrameUnit_List_Idx[property].end;

		break;
		case DIP_P2_BUF_STATE_WAIT_DEQUE_FAIL:
		case DIP_P2_BUF_STATE_DEQUE_FAIL:
			/* QQ. ADD ASSERT */
			break;
		case DIP_P2_BUF_STATE_NONE:
		case DIP_P2_BUF_STATE_RUNNING:
		default:
			break;
		}
		break;
	case DIP_P2_BUFQUE_LIST_TAG_PACKAGE:
	default:
		LOG_ERR("Wrong List tag(%d)\n", listTag);
		break;
	}
	return ret;
}
/**************************************************************
 *
 **************************************************************/
static signed int DIP_P2_BufQue_Erase
	(enum DIP_P2_BUFQUE_PROPERTY enum_property,
	enum DIP_P2_BUFQUE_LIST_TAG listTag,
	signed int idx)
{
	signed int ret =  -1;
	bool stop = false;
	int i = 0;
	signed int cnt = 0;
	int tmpIdx = 0;
	unsigned int property = 0;

	if (enum_property >= DIP_P2_BUFQUE_PROPERTY_NUM ||
		enum_property < DIP_P2_BUFQUE_PROPERTY_DIP) {
		LOG_ERR("property err(%d)\n", enum_property);
		return -EINVAL;
	}

	property = (unsigned int)enum_property;

	switch (listTag) {
	case DIP_P2_BUFQUE_LIST_TAG_PACKAGE:
	tmpIdx = P2_FramePack_List_Idx[property].start;
	/* [1] clear buffer status */
	P2_FramePackage_List[property][idx].processID = 0x0;
	P2_FramePackage_List[property][idx].callerID = 0x0;
	P2_FramePackage_List[property][idx].dupCQIdx =  -1;
	P2_FramePackage_List[property][idx].frameNum = 0;
	P2_FramePackage_List[property][idx].dequedNum = 0;
	/* [2] update first index */
	if (P2_FramePackage_List[property][tmpIdx].dupCQIdx == -1) {
		/* traverse count needed, cuz user may erase the element */
		/* but not the one at first idx */
		/* at first idx(pip or vss scenario) */
		if (P2_FramePack_List_Idx[property].start <=
			P2_FramePack_List_Idx[property].end) {
			cnt = P2_FramePack_List_Idx[property].end -
				P2_FramePack_List_Idx[property].start;
		} else {
			cnt = _MAX_SUPPORT_P2_PACKAGE_NUM_ -
				P2_FramePack_List_Idx[property].start;
			cnt += P2_FramePack_List_Idx[property].end;
		}
		do { /* to find the newest first lindex */
			tmpIdx = (tmpIdx + 1) % _MAX_SUPPORT_P2_PACKAGE_NUM_;
			switch (P2_FramePackage_List[property][tmpIdx]
				.dupCQIdx) {
			case (-1):
				break;
			default:
				stop = true;
				P2_FramePack_List_Idx[property].start = tmpIdx;
				break;
			}
			i++;
		} while ((i < cnt) && (!stop));
	/* current last erased element in list */
	/* is the one firstBufindex point at */
	/* and all the buffer node are deque done */
	/* in the current moment */
	/* should update first index to the last node */
		if ((!stop) && (i == cnt))
			P2_FramePack_List_Idx[property].start =
			P2_FramePack_List_Idx[property].end;

	}
	break;
	case DIP_P2_BUFQUE_LIST_TAG_UNIT:
	tmpIdx = P2_FrameUnit_List_Idx[property].start;
	/* [1] clear buffer status */
	P2_FrameUnit_List[property][idx].processID = 0x0;
	P2_FrameUnit_List[property][idx].callerID = 0x0;
	P2_FrameUnit_List[property][idx].cqMask =  0x0;
	P2_FrameUnit_List[property][idx].bufSts = DIP_P2_BUF_STATE_NONE;
	/* [2]update first index */
	if (P2_FrameUnit_List[property][tmpIdx].bufSts ==
		DIP_P2_BUF_STATE_NONE) {
		/* traverse count needed, cuz user may erase the element */
		/* but not the one at first idx */
		if (P2_FrameUnit_List_Idx[property].start <=
			P2_FrameUnit_List_Idx[property].end) {
			cnt = P2_FrameUnit_List_Idx[property].end -
			P2_FrameUnit_List_Idx[property].start;
		} else {
			cnt = _MAX_SUPPORT_P2_FRAME_NUM_ -
				P2_FrameUnit_List_Idx[property].start;
			cnt += P2_FrameUnit_List_Idx[property].end;
		}
		/* to find the newest first lindex */
		do {
			tmpIdx = (tmpIdx + 1) % _MAX_SUPPORT_P2_FRAME_NUM_;
			switch (P2_FrameUnit_List[property][tmpIdx].bufSts) {
			case DIP_P2_BUF_STATE_ENQUE:
			case DIP_P2_BUF_STATE_RUNNING:
			case DIP_P2_BUF_STATE_DEQUE_SUCCESS:
				stop = true;
				P2_FrameUnit_List_Idx[property].start = tmpIdx;
				break;
			case DIP_P2_BUF_STATE_WAIT_DEQUE_FAIL:
			case DIP_P2_BUF_STATE_DEQUE_FAIL:
				/* ASSERT */
				break;
			case DIP_P2_BUF_STATE_NONE:
			default:
				break;
			}
			i++;
		} while ((i < cnt) && (!stop));
	/* current last erased element in list is the */
	/* one firstBufindex point at */
	/* and all the buffer node are deque done */
	/* in the current moment */
	/* should update first index to the last node */
		if ((!stop) && (i == (cnt)))
			P2_FrameUnit_List_Idx[property].start =
			P2_FrameUnit_List_Idx[property].end;

	}
	break;
	default:
		break;
	}
	return ret;
}

/**************************************************************
 * get first matched element idnex
 **************************************************************/
static signed int DIP_P2_BufQue_GetMatchIdx
	(struct  DIP_P2_BUFQUE_STRUCT param,
	enum DIP_P2_BUFQUE_MATCH_TYPE matchType,
	enum DIP_P2_BUFQUE_LIST_TAG listTag)
{
	int idx = -1;
	int i = 0;
	unsigned int property = 0;

	if (param.property >= DIP_P2_BUFQUE_PROPERTY_NUM ||
		param.property < DIP_P2_BUFQUE_PROPERTY_DIP) {
		LOG_ERR("property err(%d)\n", param.property);
		return -EINVAL;
	}
	property = (unsigned int)param.property;

	switch (matchType) {
	case DIP_P2_BUFQUE_MATCH_TYPE_WAITDQ:
	/* traverse for finding the frame unit */
	/* which had not beed dequeued of the process */
	if (P2_FrameUnit_List_Idx[property].start <=
		P2_FrameUnit_List_Idx[property].end) {
		for (i = P2_FrameUnit_List_Idx[property].start; i <=
			P2_FrameUnit_List_Idx[property].end; i++) {
			if ((P2_FrameUnit_List[property][i].processID ==
				param.processID) &&
				((P2_FrameUnit_List[property][i].bufSts ==
				DIP_P2_BUF_STATE_ENQUE) ||
				(P2_FrameUnit_List[property][i].bufSts ==
				DIP_P2_BUF_STATE_RUNNING))) {
				idx = i;
				break;
			}
		}
	} else {
		for (i = P2_FrameUnit_List_Idx[property].start; i <
			_MAX_SUPPORT_P2_FRAME_NUM_; i++) {
			if ((P2_FrameUnit_List[property][i].processID ==
				param.processID) &&
				((P2_FrameUnit_List[property][i].bufSts ==
				DIP_P2_BUF_STATE_ENQUE) ||
				(P2_FrameUnit_List[property][i].bufSts ==
				DIP_P2_BUF_STATE_RUNNING))) {
				idx = i;
				break;
			}
		}
		if (idx !=  -1) {
			/*get in the first for loop*/
		} else {
			for (i = 0; i <=
				P2_FrameUnit_List_Idx[property].end; i++) {
				if ((P2_FrameUnit_List[property][i]
					.processID == param.processID) &&
					((P2_FrameUnit_List[property][i]
					.bufSts == DIP_P2_BUF_STATE_ENQUE) ||
					(P2_FrameUnit_List[property][i]
					.bufSts == DIP_P2_BUF_STATE_RUNNING))){
					idx = i;
					break;
				}
			}
		}
	}
	break;
	case DIP_P2_BUFQUE_MATCH_TYPE_WAITFM:
	if (P2_FramePack_List_Idx[property].start <=
		P2_FramePack_List_Idx[property].end) {
		for (i = P2_FramePack_List_Idx[property].start;
			i <= P2_FramePack_List_Idx[property].end; i++) {
			if ((P2_FramePackage_List[property][i].processID ==
				param.processID) &&
				(P2_FramePackage_List[property][i].callerID ==
				param.callerID)) {
				idx = i;
				break;
			}
		}
	} else {
		for (i = P2_FramePack_List_Idx[property].start;
			i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
			if ((P2_FramePackage_List[property][i]
				.processID == param.processID) &&
			(P2_FramePackage_List[property][i]
				.callerID == param.callerID)) {
				idx = i;
				break;
			}
		}
		if (idx !=  -1) {
			/*get in the first for loop*/
		} else {
			for (i = 0; i <=
				P2_FramePack_List_Idx[property].end; i++) {
				if ((P2_FramePackage_List[property][i]
					.processID == param.processID) &&
					(P2_FramePackage_List[property][i]
					.callerID == param.callerID)) {
					idx = i;
					break;
				}
			}
		}
	}
	break;
	case DIP_P2_BUFQUE_MATCH_TYPE_FRAMEOP:
	/* deque done notify */
	if (listTag == DIP_P2_BUFQUE_LIST_TAG_PACKAGE) {
		if (P2_FramePack_List_Idx[property].start <=
			P2_FramePack_List_Idx[property].end) {
			for (i = P2_FramePack_List_Idx[property].start;
				i <= P2_FramePack_List_Idx[property].end; i++) {
				if ((P2_FramePackage_List[property][i]
					.processID == param.processID) &&
				(P2_FramePackage_List[property][i]
					.callerID == param.callerID) &&
				(P2_FramePackage_List[property][i]
					.dupCQIdx == param.dupCQIdx) &&
				(P2_FramePackage_List[property][i].dequedNum <
				P2_FramePackage_List[property][i].frameNum)) {
		/* avoid race that dupCQ_1 of buffer2 enqued while dupCQ_1 */
		/* of buffer1 have beend deque done but not been erased yet */
					idx = i;
					break;
				}
			}
	} else {
		for (i = P2_FramePack_List_Idx[property].start; i <
			_MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
			if ((P2_FramePackage_List[property][i]
				.processID == param.processID) &&
			(P2_FramePackage_List[property][i]
				.callerID == param.callerID) &&
			(P2_FramePackage_List[property][i]
				.dupCQIdx == param.dupCQIdx) &&
			(P2_FramePackage_List[property][i].dequedNum <
				P2_FramePackage_List[property][i].frameNum)) {
				idx = i;
				break;
			}
		}
		if (idx !=  -1) {
			/*get in the first for loop*/
			break;
		}
		for (i = 0; i <= P2_FramePack_List_Idx[property].end; i++) {
			if ((P2_FramePackage_List[property][i]
				.processID == param.processID) &&
			(P2_FramePackage_List[property][i]
				.callerID == param.callerID) &&
			(P2_FramePackage_List[property][i]
				.dupCQIdx == param.dupCQIdx) &&
			(P2_FramePackage_List[property][i].dequedNum <
				P2_FramePackage_List[property][i].frameNum)) {
				idx = i;
				break;
			}
		}
	}
	} else {
		if (P2_FrameUnit_List_Idx[property].start <=
			P2_FrameUnit_List_Idx[property].end) {
			for (i = P2_FrameUnit_List_Idx[property].start;
			i <= P2_FrameUnit_List_Idx[property].end; i++) {
				if ((P2_FrameUnit_List[property][i]
					.processID == param.processID) &&
				(P2_FrameUnit_List[property][i]
					.callerID == param.callerID)) {
					idx = i;
					break;
				}
			}
	} else {
		for (i = P2_FrameUnit_List_Idx[property].start; i <
			_MAX_SUPPORT_P2_FRAME_NUM_; i++) {
			if ((P2_FrameUnit_List[property][i]
				.processID == param.processID) &&
			(P2_FrameUnit_List[property][i]
				.callerID == param.callerID)) {
				idx = i;
				break;
			}
		}
		if (idx !=  -1) {
			/*get in the first for loop*/
			break;
		}
		for (i = 0; i <= P2_FrameUnit_List_Idx[property].end; i++) {
			if ((P2_FrameUnit_List[property][i]
				.processID == param.processID) &&
			(P2_FrameUnit_List[property][i]
				.callerID == param.callerID)) {
				idx = i;
				break;
			}
		}
	}
	}
	break;
	default:
		break;
	}

	return idx;
}

/**************************************************************
 *
 **************************************************************/
static inline unsigned int DIP_P2_BufQue_WaitEventState(
	struct DIP_P2_BUFQUE_STRUCT param,
	enum DIP_P2_BUFQUE_MATCH_TYPE type,
	signed int *idx)
{
	unsigned int ret = MFALSE;
	signed int index = -1;
	unsigned int property = 0;

	if (param.property >= DIP_P2_BUFQUE_PROPERTY_NUM ||
		param.property < DIP_P2_BUFQUE_PROPERTY_DIP) {
		LOG_ERR("property err(%d)\n", param.property);
		return -EINVAL;
	}

	if ((*idx) < 0) {
		LOG_ERR("idx err(%d)\n", *idx);
		return -EINVAL;
	}

	property = (unsigned int)param.property;
	/*  */
	switch (type) {
	case DIP_P2_BUFQUE_MATCH_TYPE_WAITDQ:
		spin_lock(&(SpinLock_P2FrameList));
		index = *idx;
		if (P2_FrameUnit_List[property][index].bufSts ==
			DIP_P2_BUF_STATE_RUNNING)
			ret = MTRUE;

		spin_unlock(&(SpinLock_P2FrameList));
		break;
	case DIP_P2_BUFQUE_MATCH_TYPE_WAITFM:
		spin_lock(&(SpinLock_P2FrameList));
		index = *idx;
		if (P2_FramePackage_List[property][index].dequedNum ==
			P2_FramePackage_List[property][index].frameNum)
			ret = MTRUE;

		spin_unlock(&(SpinLock_P2FrameList));
		break;
	case DIP_P2_BUFQUE_MATCH_TYPE_WAITFMEQD:
		spin_lock(&(SpinLock_P2FrameList));
		/* LOG_INF("check bf(%d_0x%x_0x%x/%d_%d)", */
		/* param.property, */
		/* param.processID, */
		/* param.callerID, */
		/* index, */
		/*idx); */
		index = DIP_P2_BufQue_GetMatchIdx(param,
			DIP_P2_BUFQUE_MATCH_TYPE_WAITFM,
			DIP_P2_BUFQUE_LIST_TAG_PACKAGE);
		if (index == -1) {
			/* LOG_INF("check bf(%d_0x%x_0x%x / %d_%d) ", */
				/* param.property, */
				/* *param.processID, */
				/* param.callerID, */
				/* index, */
				/* *idx); */
			spin_unlock(&(SpinLock_P2FrameList));
			ret = MFALSE;
		} else {
			*idx = index;
			/* LOG_INF("check bf(%d_0x%x_0x%x / %d_%d) ", */
			/* param.property, */
			/* param.processID, */
			/* param.callerID, */
			/* index, */
			/* idx); */
			spin_unlock(&(SpinLock_P2FrameList));
			ret = MTRUE;
		}
		break;
	default:
		break;
	}
	/*  */
	return ret;
}


/**************************************************************
 *
 **************************************************************/
static signed int DIP_P2_BufQue_CTRL_FUNC(
	struct DIP_P2_BUFQUE_STRUCT param)
{
	signed int ret = 0;
	int i = 0, q = 0;
	int idx =  -1, idx2 =  -1;
	signed int restTime = 0;
	unsigned int property = 0;

	if (param.property >= DIP_P2_BUFQUE_PROPERTY_NUM ||
		param.property < DIP_P2_BUFQUE_PROPERTY_DIP) {
		LOG_ERR("property err(%d)\n", param.property);
		return -EINVAL;
	}

	property = (unsigned int)param.property;

	switch (param.ctrl) {
	/* signal that a specific buffer is enqueued */
	case DIP_P2_BUFQUE_CTRL_ENQUE_FRAME:
		spin_lock(&(SpinLock_P2FrameList));
		/* (1) check the ring buffer list is full or not */
		if (((P2_FramePack_List_Idx[property].end + 1) %
			_MAX_SUPPORT_P2_PACKAGE_NUM_) ==
			P2_FramePack_List_Idx[property].start &&
			(P2_FramePack_List_Idx[property].end != -1)) {
			LOG_ERR("pty(%d), F/L(%d_%d,%d), dCQ(%d,%d)\n",
			property,
			param.frameNum,
			P2_FramePack_List_Idx[property].start,
			P2_FramePack_List_Idx[property].end,
			P2_FramePackage_List[property]
			[P2_FramePack_List_Idx[property].start].dupCQIdx,
			P2_FramePackage_List[property]
			[P2_FramePack_List_Idx[property].end].dupCQIdx);
			LOG_ERR("RF/C/L(%d,%d,%d), sts(%d,%d,%d)\n",
			P2_FrameUnit_List_Idx[property].start,
			P2_FrameUnit_List_Idx[property].curr,
			P2_FrameUnit_List_Idx[property].end,
			P2_FrameUnit_List[property]
			[P2_FrameUnit_List_Idx[property].start].bufSts,
			P2_FrameUnit_List[property]
			[P2_FrameUnit_List_Idx[property].curr].bufSts,
			P2_FrameUnit_List[property]
			[P2_FrameUnit_List_Idx[property].end].bufSts);
		spin_unlock(&(SpinLock_P2FrameList));
		LOG_ERR("p2 frame package list is full, enque Fail.");
		ret =  -EFAULT;
		return ret;
		}
		{
		/*(2) add new to the last of the frame unit list */
		unsigned int cqmask = (param.dupCQIdx << 2) |
			(param.cQIdx << 1) |
			(param.burstQIdx);

		if (P2_FramePack_List_Idx[property].end < 0 ||
			P2_FrameUnit_List_Idx[property].end < 0) {
#ifdef P2_DBG_LOG
			IRQ_LOG_KEEPER(DIP_IRQ_TYPE_INT_DIP_A_ST, 0, _LOG_DBG,
			"pty(%d) pD(0x%x_0x%x)MF/L(%d_%d %d) (%d %d) RF/C/L(%d %d %d) (%d %d %d) cqmsk(0x%x)\n",
			property,
			param.processID,
			param.callerID,
			param.frameNum,
			P2_FramePack_List_Idx[property].start,
			P2_FramePack_List_Idx[property].end,
			P2_FramePackage_List[property]
			[P2_FramePack_List_Idx[property].start].dupCQIdx,
			P2_FramePackage_List[property][0].dupCQIdx,
			P2_FrameUnit_List_Idx[property].start,
			P2_FrameUnit_List_Idx[property].curr,
			P2_FrameUnit_List_Idx[property].end,
			P2_FrameUnit_List[property]
			[P2_FrameUnit_List_Idx[property].start].bufSts,
			P2_FrameUnit_List[property]
			[P2_FrameUnit_List_Idx[property].curr].bufSts,
			P2_FrameUnit_List[property][0].bufSts, cqmask);
#endif
		} else {
#ifdef P2_DBG_LOG
			IRQ_LOG_KEEPER(DIP_IRQ_TYPE_INT_DIP_A_ST, 0, _LOG_DBG,
			"pty(%d) pD(0x%x_0x%x) MF/L(%d_%d %d)(%d %d) RF/C/L(%d %d %d) (%d %d %d) cqmsk(0x%x)\n",
			property,
			param.processID,
			param.callerID,
			param.frameNum,
			P2_FramePack_List_Idx[property].start,
			P2_FramePack_List_Idx[property].end,
			P2_FramePackage_List[property]
			[P2_FramePack_List_Idx[property].start].dupCQIdx,
			P2_FramePackage_List[property]
			[P2_FramePack_List_Idx[property].end].dupCQIdx,
			P2_FrameUnit_List_Idx[property].start,
			P2_FrameUnit_List_Idx[property].curr,
			P2_FrameUnit_List_Idx[property].end,
			P2_FrameUnit_List[property]
			[P2_FrameUnit_List_Idx[property].start].bufSts,
			P2_FrameUnit_List[property]
			[P2_FrameUnit_List_Idx[property].curr].bufSts,
			P2_FrameUnit_List[property]
			[P2_FrameUnit_List_Idx[property].end].bufSts, cqmask);
#endif
		}
		if (P2_FrameUnit_List_Idx[property].start ==
			P2_FrameUnit_List_Idx[property].end &&
			P2_FrameUnit_List[property]
			[P2_FrameUnit_List_Idx[property]
			.start].bufSts == DIP_P2_BUF_STATE_NONE) {
			/* frame unit list is empty */
			P2_FrameUnit_List_Idx[property].end =
				(P2_FrameUnit_List_Idx[property].end + 1) %
				_MAX_SUPPORT_P2_FRAME_NUM_;
			P2_FrameUnit_List_Idx[property].start =
				P2_FrameUnit_List_Idx[property].end;
			P2_FrameUnit_List_Idx[property].curr =
				P2_FrameUnit_List_Idx[property].end;
		} else if (P2_FrameUnit_List_Idx[property].curr ==
			P2_FrameUnit_List_Idx[property].end &&
			P2_FrameUnit_List[property]
			[P2_FrameUnit_List_Idx[property]
			.curr].bufSts ==
			DIP_P2_BUF_STATE_NONE) {
	/* frame unit list is not empty, but current/last is empty. */
	/* (all the enqueued frame is done but user have not called dequeue) */
			P2_FrameUnit_List_Idx[property].end =
				(P2_FrameUnit_List_Idx[property].end + 1) %
				_MAX_SUPPORT_P2_FRAME_NUM_;
			P2_FrameUnit_List_Idx[property].curr =
				P2_FrameUnit_List_Idx[property].end;
		} else {
			P2_FrameUnit_List_Idx[property].end =
				(P2_FrameUnit_List_Idx[property].end + 1) %
				_MAX_SUPPORT_P2_FRAME_NUM_;
		}
		P2_FrameUnit_List[property][P2_FrameUnit_List_Idx[property]
			.end].processID = param.processID;
		P2_FrameUnit_List[property][P2_FrameUnit_List_Idx[property]
			.end].callerID = param.callerID;
		P2_FrameUnit_List[property][P2_FrameUnit_List_Idx[property]
			.end].cqMask = cqmask;
		P2_FrameUnit_List[property][P2_FrameUnit_List_Idx[property]
			.end].bufSts =
			DIP_P2_BUF_STATE_ENQUE;

		/* [3] add new frame package in list */
		if (param.burstQIdx == 0) {
			if (P2_FramePack_List_Idx[property].start ==
				P2_FramePack_List_Idx[property].end &&
				P2_FramePackage_List[property]
				[P2_FramePack_List_Idx[property]
				.start].dupCQIdx == -1) {
				/* all managed buffer node is empty */
				P2_FramePack_List_Idx[property].end =
				(P2_FramePack_List_Idx[property].end + 1) %
				_MAX_SUPPORT_P2_PACKAGE_NUM_;
				P2_FramePack_List_Idx[property].start =
					P2_FramePack_List_Idx[property].end;
			} else {
				P2_FramePack_List_Idx[property].end =
				(P2_FramePack_List_Idx[property].end + 1) %
				_MAX_SUPPORT_P2_PACKAGE_NUM_;
			}
			P2_FramePackage_List[property]
				[P2_FramePack_List_Idx[property].end]
				.processID = param.processID;
			P2_FramePackage_List[property]
				[P2_FramePack_List_Idx[property].end]
				.callerID = param.callerID;
			P2_FramePackage_List[property]
				[P2_FramePack_List_Idx[property].end]
				.dupCQIdx = param.dupCQIdx;
			P2_FramePackage_List[property]
				[P2_FramePack_List_Idx[property].end]
				.frameNum = param.frameNum;
			P2_FramePackage_List[property]
				[P2_FramePack_List_Idx[property].end]
				.dequedNum = 0;
		}
		}
		/* [4]update global index */
		DIP_P2_BufQue_Update_ListCIdx(property,
			DIP_P2_BUFQUE_LIST_TAG_UNIT);
		spin_unlock(&(SpinLock_P2FrameList));
		IRQ_LOG_PRINTER(DIP_IRQ_TYPE_INT_DIP_A_ST, 0, _LOG_DBG);
		/* [5] wake up thread that wait for deque */
		wake_up_interruptible_all(&P2WaitQueueHead_WaitDeque);
		wake_up_interruptible_all(&P2WaitQueueHead_WaitFrameEQDforDQ);
		break;
	/* a dequeue thread is waiting to do dequeue */
	case DIP_P2_BUFQUE_CTRL_WAIT_DEQUE:
		spin_lock(&(SpinLock_P2FrameList));
		idx = DIP_P2_BufQue_GetMatchIdx(param,
			DIP_P2_BUFQUE_MATCH_TYPE_WAITDQ,
			DIP_P2_BUFQUE_LIST_TAG_UNIT);
		spin_unlock(&(SpinLock_P2FrameList));
		if (idx ==  -1) {
			LOG_ERR("Do not find match buffer");
			LOG_ERR("(pty/pid/cid: %d/0x%x/0x%x)",
				param.property,
				param.processID,
				param.callerID);
			ret =  -EFAULT;
			return ret;
		}
		{
		restTime = wait_event_interruptible_timeout(
			P2WaitQueueHead_WaitDeque,
			DIP_P2_BufQue_WaitEventState(param,
				DIP_P2_BUFQUE_MATCH_TYPE_WAITDQ,
				&idx),
			DIP_UsToJiffies(15 * 1000000)); /* 15s */
		if (restTime == 0) {
			LOG_ERR("Wait Deque fail, idx(%d),",
				idx);
			LOG_ERR("pty(%d),pID(0x%x),cID(0x%x)",
				param.property,
				param.processID,
				param.callerID);
			ret =  -EFAULT;
		} else if (restTime == -512) {
			LOG_ERR("be stopped, restime(%d)", restTime);
			ret =  -EFAULT;
			break;
		}
		}
		break;
	/* signal that a buffer is dequeued(success) */
	case DIP_P2_BUFQUE_CTRL_DEQUE_SUCCESS:
		if (IspInfo.DebugMask & DIP_DBG_BUF_CTRL)
			LOG_DBG("dq cm(%d),pID(0x%x),cID(0x%x)\n",
				param.ctrl,
				param.processID,
				param.callerID);

		spin_lock(&(SpinLock_P2FrameList));
/* [1]update buffer status for the current buffer */
/* /////////////////////////////////////////////////////////// */
/* Assume we have the buffer list in the following situation  */
/* ++++++    ++++++  */
/* +  vss +      +  prv +    */
/* ++++++    ++++++   */
/*  */
/* if the vss deque is not done(not blocking deque), */
/* dequeThread in userspace  would change to deque */
/* prv buffer(block deque)immediately to decrease ioctl cnt. */
/* -> vss buffer would be deque at next turn, */
/* so curBuf is still at vss buffer node */
/* -> we should use param to find the current buffer index in Rlikst */
/* to update the buffer status */
/*cuz deque success/fail may not be the first buffer in Rlist */
/* /////////////////////////////////////////////////////////// */
		idx2 = DIP_P2_BufQue_GetMatchIdx(param,
			DIP_P2_BUFQUE_MATCH_TYPE_FRAMEOP,
			DIP_P2_BUFQUE_LIST_TAG_UNIT);
		if ((idx2 < 0) || (idx2 >= _MAX_SUPPORT_P2_FRAME_NUM_)) {
			spin_unlock(&(SpinLock_P2FrameList));
			LOG_ERR("Match index 2 fail(%d_0x%x_0x%x_%d, %d_%d)",
				param.property,
				param.processID,
				param.callerID,
				param.frameNum,
				param.cQIdx,
				param.dupCQIdx);
			ret =  -EFAULT;
			return ret;
		}
		if (param.ctrl == DIP_P2_BUFQUE_CTRL_DEQUE_SUCCESS)
			P2_FrameUnit_List[property][idx2].bufSts =
				DIP_P2_BUF_STATE_DEQUE_SUCCESS;
		else
			P2_FrameUnit_List[property][idx2].bufSts =
				DIP_P2_BUF_STATE_DEQUE_FAIL;

		/* [2]update dequeued num in managed buffer list */
		idx = DIP_P2_BufQue_GetMatchIdx(param,
			DIP_P2_BUFQUE_MATCH_TYPE_FRAMEOP,
			DIP_P2_BUFQUE_LIST_TAG_PACKAGE);
		if ((idx < 0) || (idx >= _MAX_SUPPORT_P2_PACKAGE_NUM_)) {
			spin_unlock(&(SpinLock_P2FrameList));
			LOG_ERR("Match index 1 fail(%d_0x%x_0x%x_%d, %d_%d)",
				param.property,
				param.processID,
				param.callerID,
				param.frameNum,
				param.cQIdx,
				param.dupCQIdx);
			ret =  -EFAULT;
			return ret;
		}
		P2_FramePackage_List[property][idx].dequedNum++;
		/* [3]update global pointer */
		DIP_P2_BufQue_Update_ListCIdx(property,
			DIP_P2_BUFQUE_LIST_TAG_UNIT);
		/* [4]erase node in ring buffer list */
		DIP_P2_BufQue_Erase(property,
			DIP_P2_BUFQUE_LIST_TAG_UNIT,
			idx2);
		spin_unlock(&(SpinLock_P2FrameList));
		/* [5]wake up thread user that wait for a specific buffer */
		/* and the thread that wait for deque */
		wake_up_interruptible_all(&P2WaitQueueHead_WaitFrame);
		wake_up_interruptible_all(&P2WaitQueueHead_WaitDeque);
		break;
	/* signal that a buffer is dequeued(fail) */
	case DIP_P2_BUFQUE_CTRL_DEQUE_FAIL:
		break;
	/* wait for a specific buffer */
	case DIP_P2_BUFQUE_CTRL_WAIT_FRAME:
	/* [1]find first match buffer */
	/*LOG_INF("DIP_P2_BUFQUE_CTRL_WAIT_FRAME, */
	/*	before pty/pID/cID (%d/0x%x/0x%x),idx(%d)", */
	/*	property, */
	/*	param.processID, */
	/*	param.callerID, */
	/*	idx); */

	/* wait for frame enqued due to user might call deque api */
	/* before the frame is enqued to kernel */
		restTime = wait_event_interruptible_timeout(
			P2WaitQueueHead_WaitFrameEQDforDQ,
			DIP_P2_BufQue_WaitEventState(param,
			DIP_P2_BUFQUE_MATCH_TYPE_WAITFMEQD,
			&idx),
			DIP_UsToJiffies(15 * 1000000));
		if (restTime == 0) {
			LOG_ERR("could not find match buffer restTime(%d)",
				restTime);
			LOG_ERR("pty/pID/cID (%d/0x%x/0x%x),idx(%d)",
				property,
				param.processID,
				param.callerID,
				idx);
			ret =  -EFAULT;
			return ret;
		} else if (restTime == -512) {
			LOG_ERR("be stopped, restime(%d)", restTime);
			ret =  -EFAULT;
			return ret;
		}

#ifdef P2_DBG_LOG
		LOG_INF("DIP_P2_BUFQUE_CTRL_WAIT_FRAME\n");
		LOG_INF("after pty/pID/cID (%d/0x%x/0x%x),idx(%d)\n",
			property, param.processID, param.callerID, idx);
#endif
		if (idx ==  -1) {
			LOG_ERR("index is a negative value!");
			ret =  -EFAULT;
			return ret;
		}
		spin_lock(&(SpinLock_P2FrameList));
		/* [2]check the buffer is dequeued or not */
		if (P2_FramePackage_List[property][idx].dequedNum ==
			P2_FramePackage_List[property][idx].frameNum) {
			DIP_P2_BufQue_Erase(property,
				DIP_P2_BUFQUE_LIST_TAG_PACKAGE,
				idx);
			spin_unlock(&(SpinLock_P2FrameList));
			ret = 0;
#ifdef P2_DBG_LOG
			LOG_DBG("Frame is alreay dequeued, return user\n");
			LOG_DBG("pd(%d/0x%x/0x%x), idx(%d)\n",
				property,
				param.processID,
				param.callerID,
				idx);
#endif
			return ret;
		}
		{
			spin_unlock(&(SpinLock_P2FrameList));
			if (IspInfo.DebugMask & DIP_DBG_BUF_CTRL)
				LOG_DBG("=pd(%d/0x%x/0x%x_%d)wait(%d s)=\n",
					property,
					param.processID,
					param.callerID,
					idx, param.timeoutIns);

		/* [3]if not, goto wait event and wait for a signal to check */
			restTime = wait_event_interruptible_timeout(
				P2WaitQueueHead_WaitFrame,
				DIP_P2_BufQue_WaitEventState(param,
					DIP_P2_BUFQUE_MATCH_TYPE_WAITFM,
					&idx),
				DIP_UsToJiffies(param.timeoutIns * 1000000));
			if (restTime == 0) {
				LOG_ERR("Dequeue Buffer fail,");
				LOG_ERR("rT(%d),idx(%d),pty(%d)\n",
					restTime,
					idx,
					property);
				LOG_ERR("pID(0x%x),cID(0x%x)\n",
					param.processID,
					param.callerID);
				ret =  -EFAULT;
				break;
			}
			if (restTime == -512) {
				LOG_ERR("be stopped, restime(%d)", restTime);
				ret =  -EFAULT;
				break;
			}
			{
			LOG_DBG("Dequeue Buffer ok, rT(%d),idx(%d), pty(%d)\n",
				restTime,
				idx,
				property);
			LOG_DBG("pID(0x%x), cID(0x%x)\n",
				param.processID,
				param.callerID);
			spin_lock(&(SpinLock_P2FrameList));
			DIP_P2_BufQue_Erase(property,
				DIP_P2_BUFQUE_LIST_TAG_PACKAGE,
				idx);
			spin_unlock(&(SpinLock_P2FrameList));
			}
		}
		break;
	/* wake all slept users to check buffer is dequeued or not */
	case DIP_P2_BUFQUE_CTRL_WAKE_WAITFRAME:
		wake_up_interruptible_all(&P2WaitQueueHead_WaitFrame);
		break;
	/* free all recored dequeued buffer */
	case DIP_P2_BUFQUE_CTRL_CLAER_ALL:
		spin_lock(&(SpinLock_P2FrameList));
		for (q = 0; q < DIP_P2_BUFQUE_PROPERTY_NUM; q++) {
			for (i = 0; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
				P2_FrameUnit_List[q][i].processID = 0x0;
				P2_FrameUnit_List[q][i].callerID = 0x0;
				P2_FrameUnit_List[q][i].cqMask = 0x0;
				P2_FrameUnit_List[q][i].bufSts =
					DIP_P2_BUF_STATE_NONE;
			}
			P2_FrameUnit_List_Idx[q].start = 0;
			P2_FrameUnit_List_Idx[q].curr = 0;
			P2_FrameUnit_List_Idx[q].end =  -1;
			/*  */
			for (i = 0; i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
				P2_FramePackage_List[q][i].processID = 0x0;
				P2_FramePackage_List[q][i].callerID = 0x0;
				P2_FramePackage_List[q][i].dupCQIdx =  -1;
				P2_FramePackage_List[q][i].frameNum = 0;
				P2_FramePackage_List[q][i].dequedNum = 0;
			}
			P2_FramePack_List_Idx[q].start = 0;
			P2_FramePack_List_Idx[q].curr = 0;
			P2_FramePack_List_Idx[q].end =  -1;
		}
		spin_unlock(&(SpinLock_P2FrameList));
		break;
	default:
		LOG_ERR("do not support this ctrl cmd(%d)", param.ctrl);
		break;
	}
	return ret;
}

/**************************************************************
 *
 **************************************************************/
static signed int DIP_FLUSH_IRQ(struct DIP_WAIT_IRQ_STRUCT *irqinfo)
{
	unsigned long flags;

	LOG_INF("type(%d)userKey(%d)St(0x%x)",
		irqinfo->Type,
		irqinfo->EventInfo.UserKey,
		irqinfo->EventInfo.Status);

	if (irqinfo->Type >= DIP_IRQ_TYPE_AMOUNT) {
		LOG_ERR("FLUSH_IRQ: type error(%d)", irqinfo->Type);
		return -EFAULT;
	}

	if (irqinfo->EventInfo.UserKey >= IRQ_USER_NUM_MAX ||
		irqinfo->EventInfo.UserKey < 0) {
		LOG_ERR("FLUSH_IRQ: userkey error(%d)",
			irqinfo->EventInfo.UserKey);
		return -EFAULT;
	}

	/* 1. enable signal */
	spin_lock_irqsave(&(IspInfo.SpinLockIrq[irqinfo->Type]), flags);
	IspInfo.IrqInfo.Status[irqinfo->Type][irqinfo->EventInfo.UserKey] |=
	irqinfo->EventInfo.Status;
	spin_unlock_irqrestore(&(IspInfo.SpinLockIrq[irqinfo->Type]), flags);

	/* 2. force to wake up the user that are waiting for that signal */
	wake_up_interruptible(&IspInfo.WaitQueueHead[irqinfo->Type]);

	return 0;
}


/**************************************************************
 *
 **************************************************************/
static signed int DIP_WaitIrq(struct DIP_WAIT_IRQ_STRUCT *WaitIrq)
{

	signed int Ret = 0;

	return Ret;
}

/**************************************************************
 *
 **************************************************************/
static long DIP_ioctl(
	struct file *pFile, unsigned int Cmd, unsigned long Param)
{
	signed int Ret = 0;
	/*  */
	/*    bool   HoldEnable = MFALSE;*/
	unsigned int DebugFlag[3] = {0};
	/*    unsigned int pid = 0;*/
	struct DIP_REG_IO_STRUCT       RegIo;
	struct DIP_DUMP_BUFFER_STRUCT DumpBufStruct;
	struct DIP_MEM_INFO_STRUCT MemInfoStruct;
	struct DIP_WAIT_IRQ_STRUCT     IrqInfo;
	struct DIP_CLEAR_IRQ_STRUCT    ClearIrq;
	struct DIP_USER_INFO_STRUCT *pUserInfo;
	struct  DIP_P2_BUFQUE_STRUCT    p2QueBuf;
	unsigned int                 wakelock_ctrl;
	unsigned int                 module;
	unsigned long flags;
	int i;

	/*  */
	if (pFile->private_data == NULL) {
		LOG_WRN("private_data is NULL,");
		LOG_WRN("(process, pid, tgid) = (%s, %d, %d)\n",
			current->comm, current->pid, current->tgid);
		return -EFAULT;
	}
	/*  */
	pUserInfo = (struct DIP_USER_INFO_STRUCT *)(pFile->private_data);
	/*  */
	switch (Cmd) {
	case DIP_WAKELOCK_CTRL:
		if (copy_from_user(&wakelock_ctrl,
			(void *)Param,
			sizeof(unsigned int)) != 0) {
			LOG_ERR("get DIP_WAKELOCK_CTRL from user fail\n");
			Ret = -EFAULT;
		} else {
			if (wakelock_ctrl == 1) {    /* Enable     wakelock */
				if (g_bWaitLock == 0) {
#ifdef CONFIG_PM_SLEEP
					__pm_stay_awake(dip_wake_lock);
#endif

					g_bWaitLock = 1;
					LOG_DBG("wakelock enable!!\n");
				}
			} else {        /* Disable wakelock */
				if (g_bWaitLock == 1) {
#ifdef CONFIG_PM_SLEEP
					__pm_relax(dip_wake_lock);
#endif

					g_bWaitLock = 0;
					LOG_DBG("wakelock disable!!\n");
				}
			}
		}
		break;
	case DIP_RESET_BY_HWMODULE: {
		if (copy_from_user(&module,
			(void *)Param,
			sizeof(unsigned int)) != 0) {
			LOG_ERR("get hwmodule from user fail\n");
			Ret = -EFAULT;
		} else {
			DIP_Reset(module);
		}
		break;
	}
	case DIP_READ_REGISTER: {
		if (copy_from_user(&RegIo,
			(void *)Param,
			sizeof(struct DIP_REG_IO_STRUCT)) == 0) {
/* 2nd layer behavoir of copy from user */
/*  is implemented in DIP_ReadReg(...) */
			Ret = DIP_ReadReg(&RegIo);
		} else {
			LOG_ERR("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	}
	case DIP_WRITE_REGISTER: {
		if (copy_from_user(&RegIo,
			(void *)Param,
			sizeof(struct DIP_REG_IO_STRUCT)) == 0) {
/* 2nd layer behavoir of copy from user */
/*  is implemented in DIP_WriteReg(...) */
			/*Ret = DIP_WriteReg(&RegIo);*/
			LOG_ERR("Not Support Wrire Reg.\n");
			Ret = -EFAULT;
		} else {
			LOG_ERR("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	}
	case DIP_WAIT_IRQ: {
		if (copy_from_user(&IrqInfo,
			(void *)Param,
			sizeof(struct DIP_WAIT_IRQ_STRUCT)) == 0) {
			/*  */
			if ((IrqInfo.Type >= DIP_IRQ_TYPE_AMOUNT) ||
				(IrqInfo.Type < 0)) {
				Ret = -EFAULT;
				LOG_ERR("invalid type(%d)\n", IrqInfo.Type);
				goto EXIT;
			}

			if ((IrqInfo.EventInfo.UserKey >= IRQ_USER_NUM_MAX) ||
				(IrqInfo.EventInfo.UserKey < 0)) {
				LOG_ERR("invalid usrKey(%d),max(%d),",
					IrqInfo.EventInfo.UserKey,
					IRQ_USER_NUM_MAX);
				LOG_ERR("force usrkey = 0\n");
				IrqInfo.EventInfo.UserKey = 0;
			}
#ifdef ENABLE_WAITIRQ_LOG
			LOG_INF("IRQ type(%d), userKey(%d), timeout(%d)\n",
				IrqInfo.Type,
				IrqInfo.EventInfo.UserKey,
				IrqInfo.EventInfo.Timeout);
			LOG_INF("userkey(%d), status(%d)\n",
				IrqInfo.EventInfo.UserKey,
				IrqInfo.EventInfo.Status);
#endif
			Ret = DIP_WaitIrq(&IrqInfo);
		} else {
			LOG_ERR("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	}
	case DIP_CLEAR_IRQ: {
	if (copy_from_user(&ClearIrq,
		(void *)Param,
		sizeof(struct DIP_CLEAR_IRQ_STRUCT)) == 0) {
		LOG_DBG("DIP_CLEAR_IRQ Type(%d)\n", ClearIrq.Type);

		if ((ClearIrq.Type >= DIP_IRQ_TYPE_AMOUNT) ||
			(ClearIrq.Type < 0)) {
			Ret = -EFAULT;
			LOG_ERR("invalid type(%d)\n", ClearIrq.Type);
			goto EXIT;
		}

		/*  */
		if ((ClearIrq.EventInfo.UserKey >= IRQ_USER_NUM_MAX) ||
			(ClearIrq.EventInfo.UserKey < 0)) {
			LOG_ERR("errUserEnum(%d)", ClearIrq.EventInfo.UserKey);
			Ret = -EFAULT;
			goto EXIT;
		}

		i = ClearIrq.EventInfo.UserKey;
		LOG_DBG("DIP_CLEAR_IRQ:Type(%d),Status(0x%x),IrqStatus(0x%x)\n",
			ClearIrq.Type, ClearIrq.EventInfo.Status,
			IspInfo.IrqInfo.Status[ClearIrq.Type][i]);
		spin_lock_irqsave(&(IspInfo.SpinLockIrq[ClearIrq.Type]),
			flags);
		IspInfo.IrqInfo
			.Status[ClearIrq.Type][ClearIrq.EventInfo.UserKey] &=
			(~ClearIrq.EventInfo.Status);
		spin_unlock_irqrestore(
			&(IspInfo.SpinLockIrq[ClearIrq.Type]),
			flags);
	} else {
		LOG_ERR("copy_from_user failed\n");
		Ret = -EFAULT;
	}
	break;
	}

	break;
	/*  */
	case DIP_FLUSH_IRQ_REQUEST:
	if (copy_from_user(&IrqInfo,
		(void *)Param,
		sizeof(struct DIP_WAIT_IRQ_STRUCT)) == 0) {
		if ((IrqInfo.EventInfo.UserKey >= IRQ_USER_NUM_MAX) ||
			(IrqInfo.EventInfo.UserKey < 0)) {
			LOG_ERR("invalid userKey(%d), max(%d)\n",
				IrqInfo.EventInfo.UserKey,
				IRQ_USER_NUM_MAX);
			Ret = -EFAULT;
			break;
		}
		if ((IrqInfo.Type >= DIP_IRQ_TYPE_AMOUNT) ||
			(IrqInfo.Type < 0)) {
			LOG_ERR("invalid type(%d), max(%d)\n",
				IrqInfo.Type,
				DIP_IRQ_TYPE_AMOUNT);
			Ret = -EFAULT;
			break;
		}

		Ret = DIP_FLUSH_IRQ(&IrqInfo);
	}
	break;
	/*  */
	case DIP_P2_BUFQUE_CTRL:
		if (copy_from_user(&p2QueBuf,
			(void *)Param,
			sizeof(struct DIP_P2_BUFQUE_STRUCT)) == 0) {
			p2QueBuf.processID = pUserInfo->Pid;
			Ret = DIP_P2_BufQue_CTRL_FUNC(p2QueBuf);
		} else {
			LOG_ERR("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	case DIP_DEBUG_FLAG:
		if (copy_from_user(DebugFlag,
			(void *)Param,
			sizeof(unsigned int)) == 0) {

			IspInfo.DebugMask = DebugFlag[0];

			/* LOG_DBG("FBC kernel debug level = %x\n", */
			/* IspInfo.DebugMask); */
		} else {
			LOG_ERR("copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;

		break;
	case DIP_GET_DUMP_INFO: {
		if (copy_to_user((void *)Param,
			&g_dumpInfo,
			sizeof(struct DIP_GET_DUMP_INFO_STRUCT)) != 0) {
			LOG_ERR("DIP_GET_DUMP_INFO copy to user fail");
			Ret = -EFAULT;
		}
		break;
	}
	case DIP_DUMP_BUFFER: {
		if (copy_from_user(&DumpBufStruct,
			(void *)Param,
			sizeof(struct DIP_DUMP_BUFFER_STRUCT)) == 0) {
		/* 2nd layer behavoir of copy from user */
		/* is implemented in DIP_DumpTuningBuffer */
			Ret = DIP_DumpBuffer(&DumpBufStruct);
		} else {
			LOG_ERR("DIP_DUMP_BUFFER copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	}
	case DIP_SET_MEM_INFO: {
		if (copy_from_user(&MemInfoStruct,
			(void *)Param,
			sizeof(struct DIP_MEM_INFO_STRUCT)) == 0) {
		/* 2nd layer behavoir of copy from user */
		/* is implemented in DIP_SetMemInfo */
			Ret = DIP_SetMemInfo(&MemInfoStruct);
		} else {
			LOG_ERR("DIP_SET_MEM_INFO copy_from_user failed\n");
			Ret = -EFAULT;
		}
		break;
	}
	case DIP_GET_GCE_FIRST_ERR: {
		if (copy_from_user(&g_dip1sterr,
			(void *)Param,
			sizeof(unsigned int)) == 0) {
		} else {
			LOG_ERR("DIP_GET_GCE_FIRST_ERR failed\n");
			Ret = -EFAULT;
		}
		break;
	}
	default:
	{
		LOG_ERR("Unknown Cmd(%d)\n", Cmd);
		Ret = -EPERM;
		break;
	}
	}
	/*  */
EXIT:
	if (Ret != 0)
		LOG_ERR("Fail, Cmd(%d), Pid(%d),",
			Cmd, pUserInfo->Pid);
		LOG_ERR("(process, pid, tgid) = (%s, %d, %d)\n",
			current->comm,
			current->pid,
			current->tgid);
	/*  */
	return Ret;
}

#ifdef CONFIG_COMPAT

/**************************************************************
 *
 **************************************************************/
static int compat_get_dip_read_register_data(
	struct compat_DIP_REG_IO_STRUCT __user *data32,
	struct DIP_REG_IO_STRUCT __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->pData);
	err |= put_user(compat_ptr(uptr), &data->pData);
	err |= get_user(count, &data32->Count);
	err |= put_user(count, &data->Count);
	return err;
}

static int compat_put_dip_read_register_data(
	struct compat_DIP_REG_IO_STRUCT __user *data32,
	struct DIP_REG_IO_STRUCT __user *data)
{
	compat_uint_t count;
	/*      compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr),     &data->pData); */
	/* err |= put_user(uptr, &data32->pData); */
	err |= get_user(count, &data->Count);
	err |= put_user(count, &data32->Count);
	return err;
}

static int compat_get_dip_dump_buffer(
	struct compat_DIP_DUMP_BUFFER_STRUCT __user *data32,
	struct DIP_DUMP_BUFFER_STRUCT __user *data)
{
	compat_uint_t count;
	compat_uint_t cmd;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(cmd, &data32->DumpCmd);
	err |= put_user(cmd, &data->DumpCmd);
	err |= get_user(uptr, &data32->pBuffer);
	err |= put_user(compat_ptr(uptr), &data->pBuffer);
	err |= get_user(count, &data32->BytesofBufferSize);
	err |= put_user(count, &data->BytesofBufferSize);
	return err;
}

static int compat_get_dip_mem_info(
	struct compat_DIP_MEM_INFO_STRUCT __user *data32,
	struct DIP_MEM_INFO_STRUCT __user *data)
{
	compat_uint_t cmd;
	compat_uint_t mempa;
	compat_uptr_t uptr;
	compat_uint_t size;
	int err = 0;

	err = get_user(cmd, &data32->MemInfoCmd);
	err |= put_user(cmd, &data->MemInfoCmd);
	err |= get_user(mempa, &data32->MemPa);
	err |= put_user(mempa, &data->MemPa);
	err |= get_user(uptr, &data32->MemVa);
	err |= put_user(compat_ptr(uptr), &data->MemVa);
	err |= get_user(size, &data32->MemSizeDiff);
	err |= put_user(size, &data->MemSizeDiff);
	return err;
}

static long DIP_ioctl_compat(
	struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
		return -ENOTTY;

	switch (cmd) {
	case COMPAT_DIP_READ_REGISTER: {
		struct compat_DIP_REG_IO_STRUCT __user *data32;
		struct DIP_REG_IO_STRUCT __user *data;

		int err = 0;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_dip_read_register_data(data32, data);
		if (err) {
			LOG_INF("compat_get_dip_read_register_data error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, DIP_READ_REGISTER,
			(unsigned long)data);
		err = compat_put_dip_read_register_data(data32, data);
		if (err) {
			LOG_INF("compat_put_dip_read_register_data error!!!\n");
			return err;
		}
		return ret;
	}
	case COMPAT_DIP_WRITE_REGISTER: {
		struct compat_DIP_REG_IO_STRUCT __user *data32;
		struct DIP_REG_IO_STRUCT __user *data;

		int err = 0;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_dip_read_register_data(data32, data);
		if (err) {
			LOG_INF("COMPAT_DIP_WRITE_REGISTER error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp, DIP_WRITE_REGISTER,
			(unsigned long)data);
		return ret;
	}

	case COMPAT_DIP_DEBUG_FLAG: {
		/* compat_ptr(arg) will convert the arg */
		ret = filp->f_op->unlocked_ioctl(filp, DIP_DEBUG_FLAG,
			(unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_DIP_WAKELOCK_CTRL: {
		ret = filp->f_op->unlocked_ioctl(filp, DIP_WAKELOCK_CTRL,
			(unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_DIP_RESET_BY_HWMODULE: {
		ret = filp->f_op->unlocked_ioctl(filp, DIP_RESET_BY_HWMODULE,
			(unsigned long)compat_ptr(arg));
		return ret;
	}
	case COMPAT_DIP_DUMP_BUFFER: {
		struct compat_DIP_DUMP_BUFFER_STRUCT __user *data32;
		struct DIP_DUMP_BUFFER_STRUCT __user *data;

		int err = 0;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_dip_dump_buffer(data32, data);
		if (err) {
			LOG_INF("COMPAT_DIP_DUMP_BUFFER error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp,
			DIP_DUMP_BUFFER,
			(unsigned long)data);
		return ret;
	}
	case COMPAT_DIP_SET_MEM_INFO: {
		struct compat_DIP_MEM_INFO_STRUCT __user *data32;
		struct DIP_MEM_INFO_STRUCT __user *data;
		int err = 0;

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;
		err = compat_get_dip_mem_info(data32, data);
		if (err) {
			LOG_INF("COMPAT_DIP_SET_MEM_INFO error!!!\n");
			return err;
		}
		ret = filp->f_op->unlocked_ioctl(filp,
			DIP_SET_MEM_INFO,
			(unsigned long)data);
		return ret;
	}
	case DIP_GET_GCE_FIRST_ERR:
	case DIP_GET_DUMP_INFO:
	case DIP_WAIT_IRQ:
	case DIP_CLEAR_IRQ: /* structure (no pointer) */
	case DIP_FLUSH_IRQ_REQUEST:
	case DIP_P2_BUFQUE_CTRL:/* structure (no pointer) */
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
		/* return DIP_ioctl(filep, cmd, arg); */
	}
}

#endif

/**************************************************************
 *
 **************************************************************/
static inline void DIP_Load_InitialSettings(void)
{
	unsigned int i = 0;

	LOG_DBG("- E.\n");

	for (i = 0 ; i < DIP_INIT_ARRAY_COUNT ; i++) {
		//ofset = DIP_A_BASE + DIP_INIT_ARY[i].ofset;
		DIP_WR32(DIP_A_BASE + DIP_INIT_ARY[i].ofset,
				DIP_INIT_ARY[i].val);
#if (MTK_DIP_COUNT == 2)
		DIP_WR32(DIP_B_BASE + DIP_INIT_ARY[i].ofset,
				DIP_INIT_ARY[i].val);
#endif
	}

}


/**************************************************************
 *
 **************************************************************/
static signed int DIP_open(
	struct inode *pInode, struct file *pFile)
{
	signed int Ret = 0;
	unsigned int i;
	int q = 0;
	struct DIP_USER_INFO_STRUCT *pUserInfo;

	LOG_DBG("- E. UserCount: %d.\n", IspInfo.UserCount);

	mutex_lock(&gDipMutex);  /* Protect the Multi Process */

	/*  */
	spin_lock(&(IspInfo.SpinLockIspRef));

	pFile->private_data = NULL;
	pFile->private_data = kmalloc(
		sizeof(struct DIP_USER_INFO_STRUCT),
		GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		LOG_ERR("ERROR: kmalloc failed,");
		LOG_ERR("(process, pid, tgid) = (%s, %d, %d)\n",
			current->comm, current->pid, current->tgid);
		Ret = -ENOMEM;
	} else {
		pUserInfo = (struct DIP_USER_INFO_STRUCT *)pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}
	/*  */
	if (IspInfo.UserCount > 0) {
		IspInfo.UserCount++;
		spin_unlock(&(IspInfo.SpinLockIspRef));
		LOG_DBG("Curr UserCount(%d)\n", IspInfo.UserCount);
		LOG_DBG("(process, pid, tgid) = (%s, %d, %d), users exist\n",
			current->comm,
			current->pid,
			current->tgid);
		goto EXIT;
	} else {
		IspInfo.UserCount++;
		spin_unlock(&(IspInfo.SpinLockIspRef));

		/* kernel log limit to (current+150) lines per second */
	#if (_K_LOG_ADJUST == 1)
		DIP_pr_detect_count = get_detect_count();
		i = DIP_pr_detect_count + 150;
		set_detect_count(i);
		LOG_DBG("Curr UserCount(%d)\n",	IspInfo.UserCount);
		LOG_DBG("(process, pid, tgid)=(%s, %d, %d)\n",
			current->comm,
			current->pid,
			current->tgid);
		LOG_DBG("log_limit_line(%d), first user Jtest\n", i);
	#else
		LOG_DBG("Curr UserCount(%d)\n", IspInfo.UserCount);
		LOG_DBG("(process, pid, tgid)=(%s, %d, %d), first user\n",
			current->comm,
			current->pid,
			current->tgid);

	#endif
	}
	/* do wait queue head init when re-enter in camera */
	/*  */
	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		FirstUnusedIrqUserKey = 1;
		strncpy((void *)IrqUserKey_UserInfo[i].userName,
			"DefaultUserNametoAllocMem",
			USERKEY_STR_LEN);
		IrqUserKey_UserInfo[i].userKey = -1;
	}
	/*  */
	spin_lock(&(SpinLock_P2FrameList));
	for (q = 0; q < DIP_P2_BUFQUE_PROPERTY_NUM; q++) {
		for (i = 0; i < _MAX_SUPPORT_P2_FRAME_NUM_; i++) {
			P2_FrameUnit_List[q][i].processID = 0x0;
			P2_FrameUnit_List[q][i].callerID = 0x0;
			P2_FrameUnit_List[q][i].cqMask =  0x0;
			P2_FrameUnit_List[q][i].bufSts = DIP_P2_BUF_STATE_NONE;
		}
		P2_FrameUnit_List_Idx[q].start = 0;
		P2_FrameUnit_List_Idx[q].curr = 0;
		P2_FrameUnit_List_Idx[q].end =  -1;
		/*  */
		for (i = 0; i < _MAX_SUPPORT_P2_PACKAGE_NUM_; i++) {
			P2_FramePackage_List[q][i].processID = 0x0;
			P2_FramePackage_List[q][i].callerID = 0x0;
			P2_FramePackage_List[q][i].dupCQIdx =  -1;
			P2_FramePackage_List[q][i].frameNum = 0;
			P2_FramePackage_List[q][i].dequedNum = 0;
		}
		P2_FramePack_List_Idx[q].start = 0;
		P2_FramePack_List_Idx[q].curr = 0;
		P2_FramePack_List_Idx[q].end =  -1;
	}
	spin_unlock(&(SpinLock_P2FrameList));
	/*  */
	spin_lock((spinlock_t *)(&SpinLockRegScen));
	g_regScen = 0xa5a5a5a5;
	spin_unlock((spinlock_t *)(&SpinLockRegScen));
	/*  */
	/* mutex_lock(&gDipMutex); */  /* Protect the Multi Process */
	g_bIonBufferAllocated = MFALSE;
	g_dip1sterr = DIP_GCE_EVENT_NONE;
#ifdef AEE_DUMP_BY_USING_ION_MEMORY
	g_dip_p2_imem_buf.handle = NULL;
	g_dip_p2_imem_buf.ion_fd = 0;
	g_dip_p2_imem_buf.va = 0;
	g_dip_p2_imem_buf.pa = 0;
	g_dip_p2_imem_buf.length = ((4*DIP_REG_RANGE) +
		(2*MAX_ISP_TILE_TDR_HEX_NO) +
		(2*MAX_DIP_CMDQ_BUFFER_SIZE) +
		(MFB_REG_RANGE + MSS_REG_RANGE)
		(8*0x400));
	dip_p2_ion_client = NULL;
	if ((dip_p2_ion_client == NULL) && (g_ion_device))
		dip_p2_ion_client = ion_client_create(g_ion_device, "dip_p2");
	if (dip_p2_ion_client == NULL) {
		LOG_ERR("invalid dip_p2_ion_client client!\n");
	} else {
		if (dip_allocbuf(&g_dip_p2_imem_buf) >= 0)
			g_bIonBufferAllocated = MTRUE;
	}
#else
	LOG_ERR("[Debug] It's LDVT load,  no AEE DUMP\n");
#endif
	if (g_bIonBufferAllocated == MTRUE) {

#ifdef AEE_DUMP_BY_USING_ION_MEMORY
		g_pPhyDIPBuffer =
			(unsigned int *)(uintptr_t)(g_dip_p2_imem_buf.va);
		g_pTuningBuffer =
			(unsigned int *)(((uintptr_t)g_pPhyDIPBuffer) +
			DIP_REG_RANGE);
		g_pTpipeBuffer =
			(unsigned int *)(((uintptr_t)g_pTuningBuffer) +
			DIP_REG_RANGE);
		g_pVirDIPBuffer =
			(unsigned int *)(((uintptr_t)g_pTpipeBuffer) +
			MAX_ISP_TILE_TDR_HEX_NO);
		g_pCmdqBuffer =
			(unsigned int *)(((uintptr_t)g_pVirDIPBuffer) +
			DIP_REG_RANGE);
		/* Kernel Exception */
		g_pKWTpipeBuffer =
			(unsigned int *)(((uintptr_t)g_pCmdqBuffer) +
			MAX_DIP_CMDQ_BUFFER_SIZE);
		g_pKWCmdqBuffer =
			(unsigned int *)(((uintptr_t)g_pKWTpipeBuffer) +
			MAX_ISP_TILE_TDR_HEX_NO);
		g_pKWVirDIPBuffer =
			(unsigned int *)(((uintptr_t)g_pKWCmdqBuffer) +
			MAX_DIP_CMDQ_BUFFER_SIZE);
		g_pPhyMFBBuffer =
			(unsigned int *)(((uintptr_t)g_pKWVirDIPBuffer) +
			DIP_REG_RANGE);
		g_pPhyMSSBuffer =
			(unsigned int *)(((uintptr_t)g_pPhyMFBBuffer) +
			MFB_REG_RANGE);

#endif
	} else {

		/* Navtive Exception */
		g_pPhyDIPBuffer = NULL;
		g_pTuningBuffer = NULL;
		g_pTpipeBuffer = NULL;
		g_pVirDIPBuffer = NULL;
		g_pCmdqBuffer = NULL;
		/* Kernel Exception */
		g_pKWTpipeBuffer = NULL;
		g_pKWCmdqBuffer = NULL;
		g_pKWVirDIPBuffer = NULL;
		g_pPhyMFBBuffer = NULL;
		g_pPhyMSSBuffer = NULL;
	}
	g_bUserBufIsReady = MFALSE;
	g_bDumpPhyDIPBuf = MFALSE;
	g_dumpInfo.tdri_baseaddr = 0xFFFFFFFF;/* 0x15022304 */
	g_dumpInfo.imgi_baseaddr = 0xFFFFFFFF;/* 0x15022500 */
	g_dumpInfo.dmgi_baseaddr = 0xFFFFFFFF;/* 0x15022620 */
	g_dumpInfo.cmdq_baseaddr = 0xFFFFFFFF;
	g_tdriaddr = 0xffffffff;
	g_cmdqaddr = 0xffffffff;
	DumpBufferField = 0;
	g_TpipeBaseAddrInfo.MemInfoCmd = 0x0;
	g_TpipeBaseAddrInfo.MemPa = 0x0;
	g_TpipeBaseAddrInfo.MemVa = NULL;
	g_TpipeBaseAddrInfo.MemSizeDiff = 0x0;
	g_CmdqBaseAddrInfo.MemInfoCmd = 0x0;
	g_CmdqBaseAddrInfo.MemPa = 0x0;
	g_CmdqBaseAddrInfo.MemVa = NULL;
	g_CmdqBaseAddrInfo.MemSizeDiff = 0x0;

	/* mutex_unlock(&gDipMutex); */
	/*  */
	for (i = 0; i < DIP_IRQ_TYPE_AMOUNT; i++) {
		for (q = 0; q < IRQ_USER_NUM_MAX; q++)
			IspInfo.IrqInfo.Status[i][q] = 0;
	}
	/* Enable clock */
#ifdef CONFIG_PM_SLEEP
	__pm_stay_awake(dip_wake_lock);
#endif

	DIP_EnableClock(MTRUE);
	/* Initial HW default value */
	if (G_u4DipEnClkCnt == 1)
		DIP_Load_InitialSettings();

	g_u4DipCnt = 0;
#ifdef CONFIG_PM_SLEEP
	__pm_relax(dip_wake_lock);
#endif

	LOG_DBG("dip open G_u4DipEnClkCnt: %d\n", G_u4DipEnClkCnt);
#ifdef KERNEL_LOG
	IspInfo.DebugMask = (DIP_DBG_INT);
#endif
	/*  */
EXIT:
	mutex_unlock(&gDipMutex);

	LOG_INF("- X. Ret: %d. UserCount: %d, G_u4DipEnClkCnt: %d.\n",
		Ret,
		IspInfo.UserCount,
		G_u4DipEnClkCnt);
	return Ret;

}

/**************************************************************
 *
 **************************************************************/
static signed int DIP_release(
	struct inode *pInode, struct file *pFile)
{
	struct DIP_USER_INFO_STRUCT *pUserInfo;
	unsigned int i = 0;

	LOG_DBG("- E. UserCount: %d.\n", IspInfo.UserCount);

	/*  */

	/*  */
	/* LOG_DBG("UserCount(%d)",IspInfo.UserCount); */
	/*  */
	if (pFile->private_data != NULL) {
		pUserInfo = (struct DIP_USER_INFO_STRUCT *)pFile->private_data;
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	mutex_lock(&gDipMutex);  /* Protect the Multi Process */
	/*      */
	spin_lock(&(IspInfo.SpinLockIspRef));
	IspInfo.UserCount--;
	if (IspInfo.UserCount > 0) {
		spin_unlock(&(IspInfo.SpinLockIspRef));
		LOG_DBG("Curr UserCount(%d)\n", IspInfo.UserCount);
		LOG_DBG("(process, pid, tgid)=(%s, %d, %d), users exist\n",
			current->comm,
			current->pid,
			current->tgid);
		goto EXIT;
	} else {
		spin_unlock(&(IspInfo.SpinLockIspRef));
	}

	/* kernel log limit back to default */
#if (_K_LOG_ADJUST == 1)
	set_detect_count(DIP_pr_detect_count);
#endif
	/*      */
	LOG_DBG("Curr UserCount(%d), (process, pid, tgid) = (%s, %d, %d)\n",
		IspInfo.UserCount,
		current->comm,
		current->pid,
		current->tgid);
	LOG_DBG("log_limit_line(%d), last user\n",
		DIP_pr_detect_count);

	if (g_bWaitLock == 1) {
#ifdef CONFIG_PM_SLEEP
		__pm_relax(dip_wake_lock);
#endif

		g_bWaitLock = 0;
	}
	/* reset */
	/*      */
	for (i = 0; i < IRQ_USER_NUM_MAX; i++) {
		FirstUnusedIrqUserKey = 1;
		strncpy((void *)IrqUserKey_UserInfo[i].userName,
			"DefaultUserNametoAllocMem",
			USERKEY_STR_LEN);
		IrqUserKey_UserInfo[i].userKey = -1;
	}
	/* mutex_lock(&gDipMutex); */  /* Protect the Multi Process */
	if (g_bIonBufferAllocated == MFALSE) {
		/* Native Exception */
		if (g_pPhyDIPBuffer != NULL) {
			vfree(g_pPhyDIPBuffer);
			g_pPhyDIPBuffer = NULL;
		}
		if (g_pPhyMFBBuffer != NULL) {
			vfree(g_pPhyMFBBuffer);
			g_pPhyMFBBuffer = NULL;
		}
		if (g_pPhyMSSBuffer != NULL) {
			vfree(g_pPhyMSSBuffer);
			g_pPhyMSSBuffer = NULL;
		}
		if (g_pTuningBuffer != NULL) {
			vfree(g_pTuningBuffer);
			g_pTuningBuffer = NULL;
		}
		if (g_pTpipeBuffer != NULL) {
			vfree(g_pTpipeBuffer);
			g_pTpipeBuffer = NULL;
		}
		if (g_pVirDIPBuffer != NULL) {
			vfree(g_pVirDIPBuffer);
			g_pVirDIPBuffer = NULL;
		}
		if (g_pCmdqBuffer != NULL) {
			vfree(g_pCmdqBuffer);
			g_pCmdqBuffer = NULL;
		}
		/* Kernel Exception */
		if (g_pKWTpipeBuffer != NULL) {
			vfree(g_pKWTpipeBuffer);
			g_pKWTpipeBuffer = NULL;
		}
		if (g_pKWCmdqBuffer != NULL) {
			vfree(g_pKWCmdqBuffer);
			g_pKWCmdqBuffer = NULL;
		}
		if (g_pKWVirDIPBuffer != NULL) {
			vfree(g_pKWVirDIPBuffer);
			g_pKWVirDIPBuffer = NULL;
		}
	} else {
#ifdef AEE_DUMP_BY_USING_ION_MEMORY
		dip_freebuf(&g_dip_p2_imem_buf);
		g_dip_p2_imem_buf.handle = NULL;
		g_dip_p2_imem_buf.ion_fd = 0;
		g_dip_p2_imem_buf.va = 0;
		g_dip_p2_imem_buf.pa = 0;
		g_bIonBufferAllocated = MFALSE;
		/* Navtive Exception */
		g_pPhyDIPBuffer = NULL;
		g_pTuningBuffer = NULL;
		g_pTpipeBuffer = NULL;
		g_pVirDIPBuffer = NULL;
		g_pCmdqBuffer = NULL;
		/* Kernel Exception */
		g_pKWTpipeBuffer = NULL;
		g_pKWCmdqBuffer = NULL;
		g_pKWVirDIPBuffer = NULL;
		g_pPhyMFBBuffer = NULL;
		g_pPhyMSSBuffer = NULL;
#endif
	}
	/* mutex_unlock(&gDipMutex); */

#ifdef AEE_DUMP_BY_USING_ION_MEMORY
	if (dip_p2_ion_client != NULL) {
		ion_client_destroy(dip_p2_ion_client);
		dip_p2_ion_client = NULL;
	} else {
		LOG_ERR("dip_p2_ion_client is NULL!!\n");
	}
#endif

#ifdef CONFIG_PM_SLEEP
	__pm_stay_awake(dip_wake_lock);
#endif

	DIP_EnableClock(MFALSE);
#ifdef CONFIG_PM_SLEEP
	__pm_relax(dip_wake_lock);
#endif

	LOG_DBG("dip release G_u4DipEnClkCnt: %d", G_u4DipEnClkCnt);
EXIT:
	mutex_unlock(&gDipMutex);
	LOG_INF("- X. UserCount: %d. G_u4DipEnClkCnt: %d",
		IspInfo.UserCount,
		G_u4DipEnClkCnt);
	return 0;
}


/**************************************************************
 *
 **************************************************************/

/*
static signed int DIP_mmap(
	struct file *pFile, struct vm_area_struct *pVma)
{
	unsigned long length = 0;
	unsigned int pfn = 0x0;


	length = (pVma->vm_end - pVma->vm_start);

	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
	pfn = pVma->vm_pgoff << PAGE_SHIFT;

	switch (pfn) {
	case DIP_A_BASE_HW:
		if (length > DIP_REG_RANGE) {
			LOG_ERR("mmap range error\n");
			LOG_ERR("module(0x%x),length(0x%lx),",
				pfn, length);
			LOG_ERR("DIP_REG_RANGE(0x%x)!\n",
				DIP_REG_RANGE);
				return -EAGAIN;
		}
		break;
#if (MTK_DIP_COUNT == 2)
	case DIP_B_BASE_HW:
		if (length > DIP_REG_RANGE) {
			LOG_ERR("mmap range error\n");
			LOG_ERR("module(0x%x),length(0x%lx),",
				pfn, length);
			LOG_ERR("DIP_REG_RANGE(0x%x)!\n",
				DIP_REG_RANGE);
				return -EAGAIN;
		}
		break;
#endif
	default:
		LOG_ERR("Illegal starting HW addr for mmap!\n");
		return -EAGAIN;
	}
	if (remap_pfn_range(pVma,
		pVma->vm_start,
		pVma->vm_pgoff,
		pVma->vm_end - pVma->vm_start,
		pVma->vm_page_prot))
		return -EAGAIN;


	return 0;
}
*/

/**************************************************************
 *
 **************************************************************/

static dev_t IspDevNo;
static struct cdev *pIspCharDrv;
static struct class *pIspClass;

static const struct file_operations IspFileOper = {
	.owner = THIS_MODULE,
	.open = DIP_open,
	.release = DIP_release,
	/* .flush       = mt_dip_flush, */
	/* .mmap = DIP_mmap, */
	.unlocked_ioctl = DIP_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = DIP_ioctl_compat,
#endif
};

/**************************************************************
 *
 **************************************************************/
static inline void DIP_UnregCharDev(void)
{
	LOG_DBG("- E.");
	/*      */
	/* Release char driver */
	if (pIspCharDrv != NULL) {
		cdev_del(pIspCharDrv);
		pIspCharDrv = NULL;
	}
	/*      */
	unregister_chrdev_region(IspDevNo, 1);
}

/**************************************************************
 *
 **************************************************************/
static inline signed int DIP_RegCharDev(void)
{
	signed int Ret = 0;
	/*  */
	LOG_DBG("- E.\n");
	/*  */
	Ret = alloc_chrdev_region(&IspDevNo, 0, 1, DIP_DEV_NAME);
	if ((Ret) < 0) {
		LOG_ERR("alloc_chrdev_region failed, %d\n", Ret);
		return Ret;
	}
	/* Allocate driver */
	pIspCharDrv = cdev_alloc();
	if (pIspCharDrv == NULL) {
		LOG_ERR("cdev_alloc failed\n");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pIspCharDrv, &IspFileOper);
	/*  */
	pIspCharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pIspCharDrv, IspDevNo, 1);
	if ((Ret) < 0) {
		LOG_ERR("Attatch file operation failed, %d\n", Ret);
		goto EXIT;
	}
	/*  */
EXIT:
	if (Ret < 0)
		DIP_UnregCharDev();


	/*      */

	LOG_DBG("- X.\n");
	return Ret;
}

/**************************************************************
 *
 **************************************************************/
static signed int DIP_probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	/*    struct resource *pRes = NULL;*/
	signed int i = 0, j = 0;
	unsigned char n;
	unsigned int irq_info[3]; /* Record interrupts info from device tree */
	struct dip_device *_dipdev = NULL;

#ifdef CONFIG_OF
	struct dip_device *dip_dev;
	struct device *dev = NULL;
#endif

	LOG_INF("- E. DIP driver probe. nr_dip_devs : %d.\n", nr_dip_devs);

	/* Get platform_device parameters */
#ifdef CONFIG_OF

	if (pDev == NULL) {
		LOG_INF("pDev is NULL");
		return -ENXIO;
	}

	nr_dip_devs += 1;
#if 1
	_dipdev = krealloc(dip_devs,
		sizeof(struct dip_device) * nr_dip_devs,
		GFP_KERNEL);
	if (!_dipdev) {
		LOG_INF("Unable to allocate dip_devs\n");
		return -ENOMEM;
	}
	dip_devs = _dipdev;

#else
	/* WARNING: Reusing the krealloc arg is almost always a bug */
	dip_devs = KREALLOC(dip_devs,
		sizeof(struct dip_device) * nr_dip_devs, GFP_KERNEL);
	if (!dip_devs) {
		LOG_INF("Unable to allocate dip_devs\n");
		return -ENOMEM;
	}
#endif

	dip_dev = &(dip_devs[nr_dip_devs - 1]);
	dip_dev->dev = &pDev->dev;

	/* iomap registers */
	dip_dev->regs = of_iomap(pDev->dev.of_node, 0);
	if (!dip_dev->regs) {
		LOG_INF("Unable to ioremap registers\n");
		LOG_INF("of_iomap fail, nr_dip_devs=%d, devnode(%s).\n",
			nr_dip_devs, pDev->dev.of_node->name);
		return -ENOMEM;
	}

#if defined(CONFIG_MTK_IOMMU_PGTABLE_EXT) && \
	(CONFIG_MTK_IOMMU_PGTABLE_EXT > 32)
		*(dip_dev->dev->dma_mask) =
			(u64)DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT);
		dip_dev->dev->coherent_dma_mask =
			(u64)DMA_BIT_MASK(CONFIG_MTK_IOMMU_PGTABLE_EXT);
#endif
	LOG_INF("nr_dip_devs=%d, devnode(%s), map_addr=0x%lx\n",
		nr_dip_devs,
		pDev->dev.of_node->name,
		(unsigned long)dip_dev->regs);

	/* get IRQ ID and request IRQ */
	dip_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);

	if (dip_dev->irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array(
			pDev->dev.of_node,
			"interrupts",
			irq_info,
			ARRAY_SIZE(irq_info))) {
			LOG_INF("get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		for (i = 0; i < DIP_IRQ_TYPE_AMOUNT; i++) {
			if ((strcmp(pDev->dev.of_node->name,
				DIP_IRQ_CB_TBL[i].device_name) == 0) &&
				(DIP_IRQ_CB_TBL[i].isr_fp != NULL)) {
				Ret = request_irq(dip_dev->irq,
					(irq_handler_t)DIP_IRQ_CB_TBL[i].isr_fp,
					irq_info[2],
					(const char *)
					DIP_IRQ_CB_TBL[i].device_name,
					NULL);
				if (Ret) {
					LOG_INF("request_irq fail\n");
					LOG_INF("nr_dip_devs=%d,devnode(%s),",
						nr_dip_devs,
						pDev->dev.of_node->name);
					LOG_INF("irq=%d,ISR: %s\n",
						dip_dev->irq,
						DIP_IRQ_CB_TBL[i].device_name);
						return Ret;
				}

				LOG_INF("nr_dip_devs=%d,devnode(%s),irq=%d,",
					nr_dip_devs,
					pDev->dev.of_node->name,
					dip_dev->irq);
				LOG_INF("ISR: %s\n",
					DIP_IRQ_CB_TBL[i].device_name);
				break;
			}
		}

		if (i >= DIP_IRQ_TYPE_AMOUNT) {
			LOG_INF("No corresponding ISR!!\n");
			LOG_INF("nr_dip_devs=%d, devnode(%s), irq=%d\n",
				nr_dip_devs,
				pDev->dev.of_node->name,
				dip_dev->irq);
		}

	} else {
		LOG_INF("No IRQ!!: nr_dip_devs=%d, devnode(%s), irq=%d\n",
			nr_dip_devs, pDev->dev.of_node->name, dip_dev->irq);
	}



	/* Only register char driver in the 1st time */
	if (nr_dip_devs == 1) {
		/* Register char driver */
		Ret = DIP_RegCharDev();
		if ((Ret)) {
			LOG_INF("register char failed");
			return Ret;
		}

		/* Create class register */
		pIspClass = class_create(THIS_MODULE, "dipdrv");
		if (IS_ERR(pIspClass)) {
			Ret = PTR_ERR(pIspClass);
			LOG_ERR("Unable to create class, err = %d\n", Ret);
			goto EXIT;
		}
		dev = device_create(pIspClass,
			NULL,
			IspDevNo,
			NULL,
			DIP_DEV_NAME);

		if (IS_ERR(dev)) {
			Ret = PTR_ERR(dev);
			LOG_INF("Failed to create device: /dev/%s, err = %d",
				DIP_DEV_NAME, Ret);
			goto EXIT;
		}

#endif

		/* Init spinlocks */
		spin_lock_init(&(IspInfo.SpinLockIspRef));
		spin_lock_init(&(IspInfo.SpinLockIsp));
		for (n = 0; n < DIP_IRQ_TYPE_AMOUNT; n++) {
			spin_lock_init(&(IspInfo.SpinLockIrq[n]));
			spin_lock_init(&(IspInfo.SpinLockIrqCnt[n]));
		}
		spin_lock_init(&(IspInfo.SpinLockRTBC));
		spin_lock_init(&(IspInfo.SpinLockClock));

		spin_lock_init(&(SpinLock_P2FrameList));
		spin_lock_init(&(SpinLockRegScen));
		spin_lock_init(&(SpinLock_UserKey));

#ifdef EP_NO_CLKMGR


#else
		/*CCF: Grab clock pointer (struct clk*) */
		dip_clk.DIP_IMG_LARB9 =
			devm_clk_get(&pDev->dev, "DIP_CG_IMG_LARB9");
		dip_clk.DIP_IMG_DIP =
			devm_clk_get(&pDev->dev, "DIP_CG_IMG_DIP");
		dip_clk.DIP_IMG_DIP_MSS =
			devm_clk_get(&pDev->dev, "DIP_CG_IMG_DIP_MSS");
		dip_clk.DIP_IMG_MFB_DIP =
			devm_clk_get(&pDev->dev, "DIP_CG_IMG_MFB_DIP");

		if (IS_ERR(dip_clk.DIP_IMG_LARB9)) {
			LOG_ERR("cannot get DIP_IMG_LARB9 clock\n");
			return PTR_ERR(dip_clk.DIP_IMG_LARB9);
		}
		if (IS_ERR(dip_clk.DIP_IMG_DIP)) {
			LOG_ERR("cannot get DIP_IMG_DIP clock\n");
			return PTR_ERR(dip_clk.DIP_IMG_DIP);
		}
		if (IS_ERR(dip_clk.DIP_IMG_DIP_MSS)) {
			LOG_ERR("cannot get DIP_IMG_DIP_MSS clock\n");
			return PTR_ERR(dip_clk.DIP_IMG_DIP_MSS);
		}
		if (IS_ERR(dip_clk.DIP_IMG_MFB_DIP)) {
			LOG_ERR("cannot get DIP_IMG_MFB_DIP clock\n");
			return PTR_ERR(dip_clk.DIP_IMG_MFB_DIP);
		}

#if ((MTK_DIP_COUNT == 2) || (MTK_MSF_OFFSET == 1))
		dip_clk.DIP_IMG_LARB11 =
			devm_clk_get(&pDev->dev, "DIP_CG_IMG_LARB11");
		if (IS_ERR(dip_clk.DIP_IMG_LARB11)) {
			LOG_ERR("cannot get DIP_IMG_LARB11 clock\n");
			return PTR_ERR(dip_clk.DIP_IMG_LARB11);
		}
#endif
#if (MTK_DIP_COUNT == 2)
		dip_clk.DIP_IMG_DIP2 =
			devm_clk_get(&pDev->dev, "DIP_CG_IMG_DIP2");
		if (IS_ERR(dip_clk.DIP_IMG_DIP2)) {
			LOG_ERR("cannot get DIP_IMG_DIP2 clock\n");
			return PTR_ERR(dip_clk.DIP_IMG_DIP2);
		}
#endif
#endif
		/*  */
		for (i = 0 ; i < DIP_IRQ_TYPE_AMOUNT; i++)
			init_waitqueue_head(&IspInfo.WaitQueueHead[i]);

#ifdef CONFIG_PM_WAKELOCKS
		dip_wake_lock = wakeup_source_register(&pDev->dev, "dip_lock_wakelock");
		isp_mdp_wake_lock = wakeup_source_register(&pDev->dev,
							"isp_mdp_wakelock");
#endif

		/* enqueue/dequeue control in ihalpipe wrapper */
		init_waitqueue_head(&P2WaitQueueHead_WaitDeque);
		init_waitqueue_head(&P2WaitQueueHead_WaitFrame);
		init_waitqueue_head(&P2WaitQueueHead_WaitFrameEQDforDQ);

		for (i = 0; i < DIP_IRQ_TYPE_AMOUNT; i++)
			tasklet_init(dip_tasklet[i].pIsp_tkt,
				dip_tasklet[i].tkt_cb, 0);

#if (DIP_BOTTOMHALF_WORKQ == 1)
		for (i = 0 ; i < DIP_IRQ_TYPE_AMOUNT; i++) {
			dip_workque[i].module = i;
			memset((void *)&(dip_workque[i].dip_bh_work), 0,
				sizeof(dip_workque[i].dip_bh_work));
			INIT_WORK(&(dip_workque[i].dip_bh_work),
				DIP_BH_Workqueue);
		}
#endif


		/* Init IspInfo*/
		spin_lock(&(IspInfo.SpinLockIspRef));
		IspInfo.UserCount = 0;
		spin_unlock(&(IspInfo.SpinLockIspRef));
		/*  */
		/* Init IrqCntInfo */
		for (i = 0; i < DIP_IRQ_TYPE_AMOUNT; i++) {
			for (j = 0; j < DIP_ISR_MAX_NUM; j++) {
				IspInfo.IrqCntInfo.m_err_int_cnt[i][j] = 0;
				IspInfo.IrqCntInfo.m_warn_int_cnt[i][j] = 0;
			}
			IspInfo.IrqCntInfo.m_err_int_mark[i] = 0;
			IspInfo.IrqCntInfo.m_warn_int_mark[i] = 0;

			IspInfo.IrqCntInfo.m_int_usec[i] = 0;
		}

		g_DIP_PMState = 0;
EXIT:
		if (Ret < 0)
			DIP_UnregCharDev();

	}

	LOG_INF("- X. DIP driver probe.\n");

	return Ret;
}

/**************************************************************
 * Called when the device is being detached from the driver
 **************************************************************/
static signed int DIP_remove(struct platform_device *pDev)
{
	/*    struct resource *pRes;*/
	signed int IrqNum;
	int i;
	/*  */
	LOG_DBG("- E.");
	/* unregister char driver. */
	DIP_UnregCharDev();

	/* Release IRQ */
	disable_irq(IspInfo.IrqNum);
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);

	/* kill tasklet */
	for (i = 0; i < DIP_IRQ_TYPE_AMOUNT; i++)
		tasklet_kill(dip_tasklet[i].pIsp_tkt);

#if 0
	/* free all registered irq(child nodes) */
	DIP_UnRegister_AllregIrq();
	/* free father nodes of irq user list */
	struct my_list_head *head;
	struct my_list_head *father;

	head = ((struct my_list_head *)(&SupIrqUserListHead.list));
	while (1) {
		father = head;
		if (father->nextirq != father) {
			father = father->nextirq;
			REG_IRQ_NODE *accessNode;

			typeof(((REG_IRQ_NODE *)0)->list) * __mptr = (father);
			accessNode = ((REG_IRQ_NODE *)((char *)__mptr -
				offsetof(REG_IRQ_NODE, list)));
			LOG_INF("free father,reg_T(%d)\n", accessNode->reg_T);
			if (father->nextirq != father) {
				head->nextirq = father->nextirq;
				father->nextirq = father;
			} else {
				/* last father node */
				head->nextirq = head;
				LOG_INF("break\n");
				break;
			}
			kfree(accessNode);
		}
	}
#endif
	/*  */
	device_destroy(pIspClass, IspDevNo);
	/*  */
	class_destroy(pIspClass);
	pIspClass = NULL;
	/*  */
	return 0;
}

static signed int DIP_suspend(
	struct platform_device *pDev,
	pm_message_t            Mesg
)
{
	if (G_u4DipEnClkCnt > 0) {
		DIP_EnableClock(MFALSE);
		g_u4DipCnt++;
	}

	if (g_DIP_PMState == 0) {
		LOG_INF("DIP suspend G_u4DipEnClkCnt: %d, g_u4DipCnt: %d",
			G_u4DipEnClkCnt,
			g_u4DipCnt);
		g_DIP_PMState = 1;
	}
	return 0;
}

/**************************************************************
 *
 **************************************************************/
static signed int DIP_resume(struct platform_device *pDev)
{
	//unsigned int ofset = 0;
	if (g_u4DipCnt > 0) {
		DIP_EnableClock(MTRUE);

		if (G_u4DipEnClkCnt == 1)
			DIP_Load_InitialSettings();

		g_u4DipCnt--;
	}

	if (g_DIP_PMState == 1) {
		LOG_INF("DIP resume G_u4DipEnClkCnt: %d, g_u4DipCnt: %d",
			G_u4DipEnClkCnt,
			g_u4DipCnt);
		g_DIP_PMState = 0;
	}
	return 0;
}

/*------------------------------------------------------------*/
#ifdef CONFIG_PM
/*------------------------------------------------------------*/
int DIP_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	/*pr_debug("calling %s()\n", __func__);*/

	return DIP_suspend(pdev, PMSG_SUSPEND);
}

int DIP_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	/*pr_debug("calling %s()\n", __func__);*/

	return DIP_resume(pdev);
}

/* extern void mt_irq_set_sens(unsigned int irq, unsigned int sens); */
/* extern void mt_irq_set_polarity(unsigned int irq,                 */
/*  unsigned int polarity); */
int DIP_pm_restore_noirq(struct device *device)
{
	/*pr_debug("calling %s()\n", __func__);*/
	return 0;

}
/*------------------------------------------------------------*/
#else /*CONFIG_PM*/
/*------------------------------------------------------------*/
#define DIP_pm_suspend NULL
#define DIP_pm_resume  NULL
#define DIP_pm_restore_noirq NULL
/*------------------------------------------------------------*/
#endif /*CONFIG_PM*/
/*------------------------------------------------------------*/

const struct dev_pm_ops DIP_pm_ops = {
	.suspend = DIP_pm_suspend,
	.resume = DIP_pm_resume,
	.freeze = DIP_pm_suspend,
	.thaw = DIP_pm_resume,
	.poweroff = DIP_pm_suspend,
	.restore = DIP_pm_resume,
	.restore_noirq = DIP_pm_restore_noirq,
};


/**************************************************************
 *
 **************************************************************/
static struct platform_driver DipDriver = {
	.probe   = DIP_probe,
	.remove  = DIP_remove,
	.suspend = DIP_suspend,
	.resume  = DIP_resume,
	.driver  = {
		.name  = DIP_DEV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = dip_of_ids,
#endif
#ifdef CONFIG_PM
		.pm     = &DIP_pm_ops,
#endif
	}
};

/**************************************************************
 *
 **************************************************************/
static int dip_p2_ke_dump_read(struct seq_file *m, void *v)
{
#ifdef AEE_DUMP_REDUCE_MEMORY
	int i;

	if (G_u4DipEnClkCnt <= 0)
		return 0;

	LOG_INF("dip p2 ke dump start!! g_bDumpPhyDIPBuf:%d\n",
		g_bDumpPhyDIPBuf);
	LOG_INF("g_bDumpPhyDIPBuf:%d, g_tdriaddr:0x%x, g_cmdqaddr:0x%x\n",
		g_bDumpPhyDIPBuf, g_tdriaddr, g_cmdqaddr);
	seq_puts(m, "============ dip p2 ke dump register============\n");
	seq_printf(m, "dip p2 you can trust below info: g_bDumpPhyDIPBuf:%d\n",
		g_bDumpPhyDIPBuf);
	seq_printf(m,
		"dip p2 g_bDumpPhyDIPBuf:%d,g_tdriaddr:0x%x, g_cmdqaddr:0x%x\n",
		g_bDumpPhyDIPBuf,
		g_tdriaddr,
		g_cmdqaddr);
	seq_puts(m, "===dip p2 hw physical register===\n");
	if (g_bDumpPhyDIPBuf == MFALSE)
		return 0;
	mutex_lock(&gDipMutex);  /* Protect the Multi Process */
	if (g_pPhyDIPBuffer != NULL) {
		for (i = 0; i < (DIP_REG_RANGE >> 2); i = i + 4) {
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				DIP_A_BASE_HW+4*i,
				(unsigned int)g_pPhyDIPBuffer[i],
				DIP_A_BASE_HW+4*(i+1),
				(unsigned int)g_pPhyDIPBuffer[i+1]);
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				DIP_A_BASE_HW+4*(i+2),
				(unsigned int)g_pPhyDIPBuffer[i+2],
				DIP_A_BASE_HW+4*(i+3),
				(unsigned int)g_pPhyDIPBuffer[i+3]);
			seq_puts(m, "\n");
		}
	} else {
		LOG_INF("g_pPhyDIPBuffer:(0x%pK)\n", g_pPhyDIPBuffer);
	}
	seq_puts(m, "===msf hw physical register===\n");
	if (g_pPhyMFBBuffer != NULL) {
		for (i = 0; i < (MFB_REG_RANGE >> 2); i = i + 4) {
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				MFB_BASE_HW+4*i,
				(unsigned int)g_pPhyMFBBuffer[i],
				MFB_BASE_HW+4*(i+1),
				(unsigned int)g_pPhyMFBBuffer[i+1]);
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				MFB_BASE_HW+4*(i+2),
				(unsigned int)g_pPhyMFBBuffer[i+2],
				MFB_BASE_HW+4*(i+3),
				(unsigned int)g_pPhyMFBBuffer[i+3]);
			seq_puts(m, "\n");
		}
	} else {
		LOG_INF("g_pPhyMFBBuffer:(0x%pK)\n", g_pPhyMFBBuffer);
	}
	seq_puts(m, "===mss hw physical register===\n");
	if (g_pPhyMSSBuffer != NULL) {
		for (i = 0; i < (MSS_REG_RANGE >> 2); i = i + 4) {
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				MSS_BASE_HW+4*i,
				(unsigned int)g_pPhyMSSBuffer[i],
				MSS_BASE_HW+4*(i+1),
				(unsigned int)g_pPhyMSSBuffer[i+1]);
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				MSS_BASE_HW+4*(i+2),
				(unsigned int)g_pPhyMSSBuffer[i+2],
				MSS_BASE_HW+4*(i+3),
				(unsigned int)g_pPhyMSSBuffer[i+3]);
			seq_puts(m, "\n");
		}
	} else {
		LOG_INF("g_pPhyMSSBuffer:(0x%pK)\n", g_pPhyMSSBuffer);
	}
	seq_puts(m, "===dip p2 tpipe buffer Info===\n");
	if (g_pKWTpipeBuffer != NULL) {
		for (i = 0; i < (MAX_ISP_TILE_TDR_HEX_NO >> 2); i = i + 4) {
			seq_printf(m, "0x%08X\n0x%08X\n0x%08X\n0x%08X\n",
				(unsigned int)g_pKWTpipeBuffer[i],
				(unsigned int)g_pKWTpipeBuffer[i+1],
				(unsigned int)g_pKWTpipeBuffer[i+2],
				(unsigned int)g_pKWTpipeBuffer[i+3]);
		}
	}
	seq_puts(m, "===dip p2 cmdq buffer Info===\n");
	if (g_pKWCmdqBuffer != NULL) {
		for (i = 0; i < (MAX_DIP_CMDQ_BUFFER_SIZE >> 2); i = i + 4) {
			seq_printf(m, "[0x%08X 0x%08X 0x%08X 0x%08X]\n",
				(unsigned int)g_pKWCmdqBuffer[i],
				(unsigned int)g_pKWCmdqBuffer[i+1],
				(unsigned int)g_pKWCmdqBuffer[i+2],
				(unsigned int)g_pKWCmdqBuffer[i+3]);
		}
	}
	seq_puts(m, "===dip p2 vir dip buffer Info===\n");
	if (g_pKWVirDIPBuffer != NULL) {
		for (i = 0; i < (DIP_REG_RANGE >> 2); i = i + 4) {
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				DIP_A_BASE_HW+4*i,
				(unsigned int)g_pKWVirDIPBuffer[i],
				DIP_A_BASE_HW+4*(i+1),
				(unsigned int)g_pKWVirDIPBuffer[i+1]);
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				DIP_A_BASE_HW+4*(i+2),
				(unsigned int)g_pKWVirDIPBuffer[i+2],
				DIP_A_BASE_HW+4*(i+3),
				(unsigned int)g_pKWVirDIPBuffer[i+3]);
			seq_puts(m, "\n");
		}
	}
	mutex_unlock(&gDipMutex);
	seq_puts(m, "============ dip p2 ke dump debug ============\n");
	LOG_INF("dip p2 ke dump end\n");
#endif
	return 0;
}
static int proc_dip_p2_ke_dump_open(
	struct inode *inode, struct file *file)
{
	return single_open(file, dip_p2_ke_dump_read, NULL);
}
static const struct file_operations dip_p2_ke_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_dip_p2_ke_dump_open,
	.read = seq_read,
	.release = single_release,
};

/**************************************************************
 *
 **************************************************************/
static int dip_p2_dump_read(struct seq_file *m, void *v)
{
#ifdef AEE_DUMP_REDUCE_MEMORY
	int i;

	if (G_u4DipEnClkCnt <= 0)
		return 0;

LOG_INF("dip p2 ne dump start!g_bUserBufIsReady:%d,",
		g_bUserBufIsReady);
LOG_INF("g_bIonBufferAllocated:%d\n",
		g_bIonBufferAllocated);
LOG_INF("dip p2 g_bDumpPhyB:%d, tdriadd:0x%x, imgiadd:0x%x,dmgiadd:0x%x\n",
		g_bDumpPhyDIPBuf,
		g_dumpInfo.tdri_baseaddr,
		g_dumpInfo.imgi_baseaddr,
		g_dumpInfo.dmgi_baseaddr);
	seq_puts(m, "============ dip p2 ne dump register============\n");
	seq_printf(m, "dip p2 you can trust below info:UserBufIsReady:%d\n",
		g_bUserBufIsReady);
	seq_printf(m,
	"dip p2 g_bDumpPhyB:%d,tdriadd:0x%x,imgiadd:0x%x,dmgiadd:0x%x\n",
		g_bDumpPhyDIPBuf,
		g_dumpInfo.tdri_baseaddr,
		g_dumpInfo.imgi_baseaddr,
		g_dumpInfo.dmgi_baseaddr);
	seq_puts(m, "===dip p2 hw physical register===\n");
	if (g_bUserBufIsReady == MFALSE)
		return 0;
	mutex_lock(&gDipMutex);  /* Protect the Multi Process */
	if (g_pPhyDIPBuffer != NULL) {
		for (i = 0; i < (DIP_REG_RANGE >> 2); i = i + 4) {
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				DIP_A_BASE_HW+4*i,
				(unsigned int)g_pPhyDIPBuffer[i],
				DIP_A_BASE_HW+4*(i+1),
				(unsigned int)g_pPhyDIPBuffer[i+1]);
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				DIP_A_BASE_HW+4*(i+2),
				(unsigned int)g_pPhyDIPBuffer[i+2],
				DIP_A_BASE_HW+4*(i+3),
				(unsigned int)g_pPhyDIPBuffer[i+3]);
			seq_puts(m, "\n");
		}
	} else {
		LOG_INF("g_pPhyDIPBuffer:(0x%pK)\n", g_pPhyDIPBuffer);
	}
	seq_puts(m, "===mfb hw physical register===\n");
	if (g_pPhyMFBBuffer != NULL) {
		for (i = 0; i < (MFB_REG_RANGE >> 2); i = i + 4) {
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				MFB_BASE_HW+4*i,
				(unsigned int)g_pPhyMFBBuffer[i],
				MFB_BASE_HW+4*(i+1),
				(unsigned int)g_pPhyMFBBuffer[i+1]);
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				MFB_BASE_HW+4*(i+2),
				(unsigned int)g_pPhyMFBBuffer[i+2],
				MFB_BASE_HW+4*(i+3),
				(unsigned int)g_pPhyMFBBuffer[i+3]);
			seq_puts(m, "\n");
		}
	} else {
		LOG_INF("g_pPhyMFBBuffer:(0x%pK)\n", g_pPhyMFBBuffer);
	}
	seq_puts(m, "===mss hw physical register===\n");
	if (g_pPhyMSSBuffer != NULL) {
		for (i = 0; i < (MSS_REG_RANGE >> 2); i = i + 4) {
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				MSS_BASE_HW+4*i,
				(unsigned int)g_pPhyMSSBuffer[i],
				MSS_BASE_HW+4*(i+1),
				(unsigned int)g_pPhyMSSBuffer[i+1]);
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				MSS_BASE_HW+4*(i+2),
				(unsigned int)g_pPhyMSSBuffer[i+2],
				MSS_BASE_HW+4*(i+3),
				(unsigned int)g_pPhyMSSBuffer[i+3]);
			seq_puts(m, "\n");
		}
	} else {
		LOG_INF("g_pPhyMSSBuffer:(0x%pK)\n", g_pPhyMSSBuffer);
	}

	seq_puts(m, "===dip p2 tpipe buffer Info===\n");
	if (g_pTpipeBuffer != NULL) {
		for (i = 0; i < (MAX_ISP_TILE_TDR_HEX_NO >> 2); i = i + 4) {
			seq_printf(m, "0x%08X\n0x%08X\n0x%08X\n0x%08X\n",
				(unsigned int)g_pTpipeBuffer[i],
				(unsigned int)g_pTpipeBuffer[i+1],
				(unsigned int)g_pTpipeBuffer[i+2],
				(unsigned int)g_pTpipeBuffer[i+3]);
		}
	}
	seq_puts(m, "===dip p2 cmdq buffer Info===\n");
	if (g_pCmdqBuffer != NULL) {
		for (i = 0; i < (MAX_DIP_CMDQ_BUFFER_SIZE >> 2); i = i + 4) {
			seq_printf(m, "[0x%08X 0x%08X 0x%08X 0x%08X]\n",
				(unsigned int)g_pCmdqBuffer[i],
				(unsigned int)g_pCmdqBuffer[i+1],
				(unsigned int)g_pCmdqBuffer[i+2],
				(unsigned int)g_pCmdqBuffer[i+3]);
		}
	}
	seq_puts(m, "===dip p2 vir dip buffer Info===\n");
	if (g_pVirDIPBuffer != NULL) {
		for (i = 0; i < (DIP_REG_RANGE >> 2); i = i + 4) {
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				DIP_A_BASE_HW+4*i,
				(unsigned int)g_pVirDIPBuffer[i],
				DIP_A_BASE_HW+4*(i+1),
				(unsigned int)g_pVirDIPBuffer[i+1]);
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				DIP_A_BASE_HW+4*(i+2),
				(unsigned int)g_pVirDIPBuffer[i+2],
				DIP_A_BASE_HW+4*(i+3),
				(unsigned int)g_pVirDIPBuffer[i+3]);
			seq_puts(m, "\n");
		}
	}
	seq_puts(m, "===dip p2 tuning buffer Info===\n");
	if (g_pTuningBuffer != NULL) {
		for (i = 0; i < (DIP_REG_RANGE >> 2); i = i + 4) {
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				DIP_A_BASE_HW+4*i,
				(unsigned int)g_pTuningBuffer[i],
				DIP_A_BASE_HW+4*(i+1),
				(unsigned int)g_pTuningBuffer[i+1]);
			seq_printf(m, "(0x%08X,0x%08X)(0x%08X,0x%08X)",
				DIP_A_BASE_HW+4*(i+2),
				(unsigned int)g_pTuningBuffer[i+2],
				DIP_A_BASE_HW+4*(i+3),
				(unsigned int)g_pTuningBuffer[i+3]);
			seq_puts(m, "\n");
		}
	}
	mutex_unlock(&gDipMutex);
	seq_puts(m, "============ dip p2 ne dump debug ============\n");
	LOG_INF("dip p2 ne dump end\n");
#endif
	return 0;
}

static int proc_dip_p2_dump_open(
	struct inode *inode, struct file *file)
{
	return single_open(file, dip_p2_dump_read, NULL);
}

static const struct file_operations dip_p2_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_dip_p2_dump_open,
	.read = seq_read,
	.release = single_release,
};
/**************************************************************
 *
 **************************************************************/
static int dip_dump_read(struct seq_file *m, void *v)
{
/* fix unexpected close clock issue */
#if 0
	int i;

	if (G_u4DipEnClkCnt <= 0)
		return 0;

	seq_puts(m, "\n============ dip dump register============\n");
	seq_puts(m, "dip top control\n");
	for (i = 0; i < 0xFC; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(DIP_A_BASE_HW+i),
			(unsigned int)DIP_RD32(DIP_A_BASE + i));
	}

	seq_puts(m, "dma error\n");
	for (i = 0x744; i < 0x7A4; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(DIP_A_BASE_HW+i),
			(unsigned int)DIP_RD32(DIP_A_BASE + i));
	}

	seq_puts(m, "dma setting\n");
	for (i = 0x304; i < 0x6D8; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(DIP_A_BASE_HW+i),
			(unsigned int)DIP_RD32(DIP_A_BASE + i));
	}

	seq_puts(m, "cq info\n");
	for (i = 0x204; i < 0x218; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(DIP_A_BASE_HW+i),
			(unsigned int)DIP_RD32(DIP_A_BASE + i));
	}

	seq_puts(m, "crz setting\n");
	for (i = 0x5300; i < 0x5334; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(DIP_A_BASE_HW+i),
			(unsigned int)DIP_RD32(DIP_A_BASE + i));
	}

	seq_puts(m, "mdp crop1\n");
	for (i = 0x5500; i < 0x5508; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(DIP_A_BASE_HW+i),
			(unsigned int)DIP_RD32(DIP_A_BASE + i));
	}

	seq_puts(m, "mdp crop2\n");
	for (i = 0x2B80; i < 0x2B88; i = i + 4) {
		seq_printf(m, "[0x%08X %08X]\n",
			(unsigned int)(DIP_A_BASE_HW+i),
			(unsigned int)DIP_RD32(DIP_A_BASE + i));
	}

	seq_printf(m, "[0x%08X %08X]\n",
		(unsigned int)(DIP_IMGSYS_BASE_HW),
		(unsigned int)DIP_RD32(DIP_IMGSYS_CONFIG_BASE));

	seq_puts(m, "\n============ dip dump debug ============\n");
#endif
	return 0;
}
static int proc_dip_dump_open(
	struct inode *inode, struct file *file)
{
	return single_open(file, dip_dump_read, NULL);
}

static const struct file_operations dip_dump_proc_fops = {
	.owner = THIS_MODULE,
	.open = proc_dip_dump_open,
	.read = seq_read,
	.release = single_release,
};
/**************************************************************
 *
 **************************************************************/
#ifdef CONFIG_MTK_IOMMU_V2
enum mtk_iommu_callback_ret_t ISP_M4U_TranslationFault_callback(int port,
	unsigned long mva, void *data)
#else
enum m4u_callback_ret_t ISP_M4U_TranslationFault_callback(int port,
	unsigned int mva, void *data)
#endif
{

	pr_info("[ISP_M4U]fault call port=%d, mva=0x%lx", port, mva);

	switch (port) {
#if 0
	case M4U_PORT_IMGI_D1:
	case M4U_PORT_IMGBI_D1:
	case M4U_PORT_DMGI_D1:
	case M4U_PORT_DEPI_D1:
	case M4U_PORT_LCEI_D1:
	case M4U_PORT_SMTI_D1:
	case M4U_PORT_SMTO_D1:
	case M4U_PORT_SMTO_D2:
	case M4U_PORT_CRZO_D1:
	case M4U_PORT_IMG3O_D1:
	case M4U_PORT_VIPI_D1:
	case M4U_PORT_TIMGO_D1:
#endif
	default:  //DIP_A_BASE = 0x15021000
		pr_info("imgi:0x%08x, imgbi:0x%08x, imgci:0x%08x, vipi:0x%08x,",
			DIP_RD32(DIP_A_BASE + 0x200),
			DIP_RD32(DIP_A_BASE + 0x300),
			DIP_RD32(DIP_A_BASE + 0x330),
			DIP_RD32(DIP_A_BASE + 0x800));
		pr_info("vipbi:0x%08x, vipci:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x830),
			DIP_RD32(DIP_A_BASE + 0x860));

		pr_info("imgi offset:0x%08x, xsize:0x%08x, ysize:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x204),
			DIP_RD32(DIP_A_BASE + 0x20c),
			DIP_RD32(DIP_A_BASE + 0x210));
		pr_info("imgbi offset:0x%08x, xsize:0x%08x, ysize:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x304),
			DIP_RD32(DIP_A_BASE + 0x30c),
			DIP_RD32(DIP_A_BASE + 0x310));
		pr_info("imgci offset:0x%08x, xsize:0x%08x, ysize:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x334),
			DIP_RD32(DIP_A_BASE + 0x33c),
			DIP_RD32(DIP_A_BASE + 0x340));
		pr_info("vipi offset:0x%08x, xsize:0x%08x, ysize:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x804),
			DIP_RD32(DIP_A_BASE + 0x80c),
			DIP_RD32(DIP_A_BASE + 0x810));
		pr_info("vipbi offset:0x%08x, xsize:0x%08x, ysize:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x834),
			DIP_RD32(DIP_A_BASE + 0x83c),
			DIP_RD32(DIP_A_BASE + 0x840));
		pr_info("vipci offset:0x%08x, xsize:0x%08x, ysize:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x864),
			DIP_RD32(DIP_A_BASE + 0x86c),
			DIP_RD32(DIP_A_BASE + 0x870));
		pr_info("nr3d con:0x%08x, size:0x%08x, tile_xy:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x7000),
			DIP_RD32(DIP_A_BASE + 0x7004),
			DIP_RD32(DIP_A_BASE + 0x7008));
		pr_info("nr3d on_con:0x%08x, on_off:0x%08x, on_size:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x700c),
			DIP_RD32(DIP_A_BASE + 0x7010),
			DIP_RD32(DIP_A_BASE + 0x7014));
		pr_info("nr3d int1:0x%08x, int2:0x%08x, int3:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x7218),
			DIP_RD32(DIP_A_BASE + 0x721c),
			DIP_RD32(DIP_A_BASE + 0x7220));
		pr_info("nr3d out_cnt:0x%08x, status:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x7224),
			DIP_RD32(DIP_A_BASE + 0x7228));
		pr_info("mix_d2 ctl0:0x%08x, cltl1:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x6b40),
			DIP_RD32(DIP_A_BASE + 0x6b44));



		pr_info("TDRI:0x%08x, CQ0_EN(0x%08x)_BA(0x%08x),",
			DIP_RD32(DIP_A_BASE + 0x004),
			DIP_RD32(DIP_A_BASE + 0x1204),
			DIP_RD32(DIP_A_BASE + 0x1208));
		pr_info("CQ1_EN(0x%08x)_BA(0x%08x), ufdi:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x121c),
			DIP_RD32(DIP_A_BASE + 0x1220),
			DIP_RD32(DIP_A_BASE + 0x230));
		pr_info("smti_d1:0x%08x, smto_d1:0x%08x, timgo:0x%08x,",
			DIP_RD32(DIP_A_BASE + 0x260),
			DIP_RD32(DIP_A_BASE + 0x290),
			DIP_RD32(DIP_A_BASE + 0x360));
		pr_info("smti_d4:0x%08x, smto_d4:0x%08x,\n",
			DIP_RD32(DIP_A_BASE + 0x3d0),
			DIP_RD32(DIP_A_BASE + 0x400));
		pr_info("dmgi:0x%08x, depi:0x%08x, lcei:0x%08x,",
			DIP_RD32(DIP_A_BASE + 0x470),
			DIP_RD32(DIP_A_BASE + 0x4a0),
			DIP_RD32(DIP_A_BASE + 0x4d0));
		pr_info("decso:0x%08x, smti_d2:0x%08x,\n",
			DIP_RD32(DIP_A_BASE + 0x500),
			DIP_RD32(DIP_A_BASE + 0x570));
		pr_info("smto_d2:0x%08x, smti_d3:0x%08x, smto_d3:0x%08x,",
			DIP_RD32(DIP_A_BASE + 0x5a0),
			DIP_RD32(DIP_A_BASE + 0x610),
			DIP_RD32(DIP_A_BASE + 0x640));
		pr_info("crzo:0x%08x, crzbo:0x%08x,\n",
			DIP_RD32(DIP_A_BASE + 0x6b0),
			DIP_RD32(DIP_A_BASE + 0x720));
		pr_info("feo:0x%08x, img3o:0x%08x, img3bo:0x%08x, img3co:0x%08x\n",
			DIP_RD32(DIP_A_BASE + 0x790),
			DIP_RD32(DIP_A_BASE + 0x890),
			DIP_RD32(DIP_A_BASE + 0x900),
			DIP_RD32(DIP_A_BASE + 0x970));
		pr_info("start: 0x%08x, top_en: 0x%08x, 0x%08x, 0x%08x,",
			DIP_RD32(DIP_A_BASE + 0x1000),
			DIP_RD32(DIP_A_BASE + 0x1010),
			DIP_RD32(DIP_A_BASE + 0x1014),
			DIP_RD32(DIP_A_BASE + 0x1018));
		pr_info("top_en: 0x%08x, 0x%08x, 0x%08x)\n",
			DIP_RD32(DIP_A_BASE + 0x101c),
			DIP_RD32(DIP_A_BASE + 0x1020),
			DIP_RD32(DIP_A_BASE + 0x1024));
	break;
	}
#ifdef CONFIG_MTK_IOMMU_V2
	return MTK_IOMMU_CALLBACK_HANDLED;
#else
	return M4U_CALLBACK_HANDLED;
#endif
}

/**************************************************************
 *
 **************************************************************/
#ifdef CONFIG_MTK_IOMMU_V2
enum mtk_iommu_callback_ret_t MFB_M4U_TranslationFault_callback(int port,
	unsigned long mva, void *data)
#else
enum m4u_callback_ret_t MFB_M4U_TranslationFault_callback(int port,
	unsigned int mva, void *data)
#endif
{
	unsigned int loop = 0;

	pr_info("[MFB_M4U]fault call port=%d, mva=0x%lx", port, mva);
	switch (port) {
#if (MTK_MSF_OFFSET == 1)
	case M4U_PORT_L11_IMG_MFB_RDMA0:
#else
	case M4U_PORT_L9_IMG_MFB_RDMA0_MDP:
#endif
	pr_info("msftdr:0x%08x, msstdr:0x%08x diptdr:0x%08x",
		DIP_RD32(MSF_BASE + 0x804),
		DIP_RD32(MSS_BASE + 0x804),
		DIP_RD32(DIP_A_BASE + 0x4));
	pr_info("msfcmdq:0x%08x, msscmdq:0x%08x",
		DIP_RD32(MSF_BASE + 0x7C0),
		DIP_RD32(MSS_BASE + 0x500));
	pr_info("mss top_cfg:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSS_BASE + 0x400),
		DIP_RD32(MSS_BASE + 0x404),
		DIP_RD32(MSS_BASE + 0x408));
	pr_info("msf top_ctl:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSF_BASE + 0x480),
		DIP_RD32(MSF_BASE + 0x49C),
		DIP_RD32(MSF_BASE + 0x4A0));
	pr_info("baseiy:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0xA00),
		DIP_RD32(MSF_BASE + 0xA04),
		DIP_RD32(MSF_BASE + 0xA08),
		DIP_RD32(MSF_BASE + 0xA0C));
	pr_info("baseic:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0xA40),
		DIP_RD32(MSF_BASE + 0xA44),
		DIP_RD32(MSF_BASE + 0xA48),
		DIP_RD32(MSF_BASE + 0xA4C));
	pr_info("confi:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0xC80),
		DIP_RD32(MSF_BASE + 0xC84),
		DIP_RD32(MSF_BASE + 0xC88),
		DIP_RD32(MSF_BASE + 0xC8C));
		break;
#if (MTK_MSF_OFFSET == 1)
	case M4U_PORT_L11_IMG_MFB_RDMA1:
#else
	case M4U_PORT_L9_IMG_MFB_RDMA1_MDP:
#endif
	pr_info("msftdr:0x%08x, msstdr:0x%08x diptdr:0x%08x",
		DIP_RD32(MSF_BASE + 0x804),
		DIP_RD32(MSS_BASE + 0x804),
		DIP_RD32(DIP_A_BASE + 0x4));
	pr_info("msfcmdq:0x%08x, msscmdq:0x%08x",
		DIP_RD32(MSF_BASE + 0x7C0),
		DIP_RD32(MSS_BASE + 0x500));
	pr_info("mss top_cfg:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSS_BASE + 0x400),
		DIP_RD32(MSS_BASE + 0x404),
		DIP_RD32(MSS_BASE + 0x408));
	pr_info("msf top_ctl:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSF_BASE + 0x480),
		DIP_RD32(MSF_BASE + 0x49C),
		DIP_RD32(MSF_BASE + 0x4A0));
	pr_info("wei:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0xC00),
		DIP_RD32(MSF_BASE + 0xC04),
		DIP_RD32(MSF_BASE + 0xC08),
		DIP_RD32(MSF_BASE + 0xC0C));
	pr_info("refic:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0xAC0),
		DIP_RD32(MSF_BASE + 0xAC4),
		DIP_RD32(MSF_BASE + 0xAC8),
		DIP_RD32(MSF_BASE + 0xACC));
	pr_info("dswi:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0xC40),
		DIP_RD32(MSF_BASE + 0xC44),
		DIP_RD32(MSF_BASE + 0xC48),
		DIP_RD32(MSF_BASE + 0xC4C));
	pr_info("idiy:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0xB80),
		DIP_RD32(MSF_BASE + 0xB84),
		DIP_RD32(MSF_BASE + 0xB88),
		DIP_RD32(MSF_BASE + 0xB8C));
		break;
#if (MTK_MSF_OFFSET == 1)
	case M4U_PORT_L11_IMG_MFB_RDMA2:
#else
	case M4U_PORT_L9_IMG_MFB_RDMA2_MDP:
#endif
	pr_info("msftdr:0x%08x, msstdr:0x%08x diptdr:0x%08x",
		DIP_RD32(MSF_BASE + 0x804),
		DIP_RD32(MSS_BASE + 0x804),
		DIP_RD32(DIP_A_BASE + 0x4));
	pr_info("msfcmdq:0x%08x, msscmdq:0x%08x",
		DIP_RD32(MSF_BASE + 0x7C0),
		DIP_RD32(MSS_BASE + 0x500));
	pr_info("mss top_cfg:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSS_BASE + 0x400),
		DIP_RD32(MSS_BASE + 0x404),
		DIP_RD32(MSS_BASE + 0x408));
	pr_info("msf top_ctl:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSF_BASE + 0x480),
		DIP_RD32(MSF_BASE + 0x49C),
		DIP_RD32(MSF_BASE + 0x4A0));
	pr_info("refiy:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0xA80),
		DIP_RD32(MSF_BASE + 0xA84),
		DIP_RD32(MSF_BASE + 0xA88),
		DIP_RD32(MSF_BASE + 0xA8C));
	pr_info("dsiy:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0xB00),
		DIP_RD32(MSF_BASE + 0xB04),
		DIP_RD32(MSF_BASE + 0xB08),
		DIP_RD32(MSF_BASE + 0xB0C));
	pr_info("dsic:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0xB40),
		DIP_RD32(MSF_BASE + 0xB44),
		DIP_RD32(MSF_BASE + 0xB48),
		DIP_RD32(MSF_BASE + 0xB4C));
	pr_info("idic:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0xBC0),
		DIP_RD32(MSF_BASE + 0xBC4),
		DIP_RD32(MSF_BASE + 0xBC8),
		DIP_RD32(MSF_BASE + 0xBCC));
		break;
#if (MTK_MSF_OFFSET == 1)
	case M4U_PORT_L11_IMG_MFB_RDMA3:
#else
	case M4U_PORT_L9_IMG_MFB_RDMA3_MDP:
#endif
	pr_info("msftdr:0x%08x, msstdr:0x%08x diptdr:0x%08x",
		DIP_RD32(MSF_BASE + 0x804),
		DIP_RD32(MSS_BASE + 0x804),
		DIP_RD32(DIP_A_BASE + 0x4));
	pr_info("msfcmdq:0x%08x, msscmdq:0x%08x",
		DIP_RD32(MSF_BASE + 0x7C0),
		DIP_RD32(MSS_BASE + 0x500));
	pr_info("mss top_cfg:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSS_BASE + 0x400),
		DIP_RD32(MSS_BASE + 0x404),
		DIP_RD32(MSS_BASE + 0x408));
	pr_info("msf top_ctl:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSF_BASE + 0x480),
		DIP_RD32(MSF_BASE + 0x49C),
		DIP_RD32(MSF_BASE + 0x4A0));
	pr_info("omcmv:0x%08x, stride:0x%08x,",
		DIP_RD32(MSS_BASE + 0x120),
		DIP_RD32(MSS_BASE + 0x124));
		break;
#if (MTK_MSF_OFFSET == 1)
	case M4U_PORT_L11_IMG_MFB_RDMA4:
#else
	case M4U_PORT_L9_IMG_MFB_RDMA4_MDP:
#endif
	pr_info("msftdr:0x%08x, msstdr:0x%08x diptdr:0x%08x",
		DIP_RD32(MSF_BASE + 0x804),
		DIP_RD32(MSS_BASE + 0x804),
		DIP_RD32(DIP_A_BASE + 0x4));
	pr_info("msfcmdq:0x%08x, msscmdq:0x%08x",
		DIP_RD32(MSF_BASE + 0x7C0),
		DIP_RD32(MSS_BASE + 0x500));
	pr_info("mss top_cfg:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSS_BASE + 0x400),
		DIP_RD32(MSS_BASE + 0x404),
		DIP_RD32(MSS_BASE + 0x408));
	pr_info("msf top_ctl:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSF_BASE + 0x480),
		DIP_RD32(MSF_BASE + 0x49C),
		DIP_RD32(MSF_BASE + 0x4A0));
	pr_info("omciy:0x%08x, ofset:0x%08x, stride:0x%08x,",
		DIP_RD32(MSS_BASE + 0x18C),
		DIP_RD32(MSS_BASE + 0x190),
		DIP_RD32(MSS_BASE + 0x194));
	pr_info("omcic:0x%08x, ofset:0x%08x, stride:0x%08x,",
		DIP_RD32(MSS_BASE + 0x198),
		DIP_RD32(MSS_BASE + 0x19C),
		DIP_RD32(MSS_BASE + 0x1A0));
		break;
#if (MTK_MSF_OFFSET == 1)
	case M4U_PORT_L11_IMG_MFB_RDMA5:
#else
	case M4U_PORT_L9_IMG_MFB_RDMA5_MDP:
#endif
	pr_info("msftdr:0x%08x, msstdr:0x%08x diptdr:0x%08x",
		DIP_RD32(MSF_BASE + 0x804),
		DIP_RD32(MSS_BASE + 0x804),
		DIP_RD32(DIP_A_BASE + 0x4));
	pr_info("msfcmdq:0x%08x, msscmdq:0x%08x",
		DIP_RD32(MSF_BASE + 0x7C0),
		DIP_RD32(MSS_BASE + 0x500));
	pr_info("mss top_cfg:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSS_BASE + 0x400),
		DIP_RD32(MSS_BASE + 0x404),
		DIP_RD32(MSS_BASE + 0x408));
	pr_info("msf top_ctl:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSF_BASE + 0x480),
		DIP_RD32(MSF_BASE + 0x49C),
		DIP_RD32(MSF_BASE + 0x4A0));
	pr_info("mssiy:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSS_BASE + 0xA00),
		DIP_RD32(MSS_BASE + 0xA04),
		DIP_RD32(MSS_BASE + 0xA08),
		DIP_RD32(MSS_BASE + 0xA0C));
	pr_info("mssic:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSS_BASE + 0xA40),
		DIP_RD32(MSS_BASE + 0xA44),
		DIP_RD32(MSS_BASE + 0xA48),
		DIP_RD32(MSS_BASE + 0xA4C));
		break;
#if (MTK_MSF_OFFSET == 1)
	case M4U_PORT_L11_IMG_MFB_WDMA0:
#else
	case M4U_PORT_L9_IMG_MFB_WDMA0_MDP:
#endif
	pr_info("msftdr:0x%08x, msstdr:0x%08x diptdr:0x%08x",
		DIP_RD32(MSF_BASE + 0x804),
		DIP_RD32(MSS_BASE + 0x804),
		DIP_RD32(DIP_A_BASE + 0x4));
	pr_info("msfcmdq:0x%08x, msscmdq:0x%08x",
		DIP_RD32(MSF_BASE + 0x7C0),
		DIP_RD32(MSS_BASE + 0x500));
	pr_info("mss top_cfg:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSS_BASE + 0x400),
		DIP_RD32(MSS_BASE + 0x404),
		DIP_RD32(MSS_BASE + 0x408));
	pr_info("msf top_ctl:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSF_BASE + 0x480),
		DIP_RD32(MSF_BASE + 0x49C),
		DIP_RD32(MSF_BASE + 0x4A0));
	pr_info("fsoy:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0x900),
		DIP_RD32(MSF_BASE + 0x904),
		DIP_RD32(MSF_BASE + 0x908),
		DIP_RD32(MSF_BASE + 0x90C));
	pr_info("fsoc:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0x940),
		DIP_RD32(MSF_BASE + 0x944),
		DIP_RD32(MSF_BASE + 0x948),
		DIP_RD32(MSF_BASE + 0x94C));
	pr_info("weo:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0x980),
		DIP_RD32(MSF_BASE + 0x984),
		DIP_RD32(MSF_BASE + 0x988),
		DIP_RD32(MSF_BASE + 0x98C));
	pr_info("dswo:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSF_BASE + 0x9C0),
		DIP_RD32(MSF_BASE + 0x9C4),
		DIP_RD32(MSF_BASE + 0x9C8),
		DIP_RD32(MSF_BASE + 0x9CC));
		break;
#if (MTK_MSF_OFFSET == 1)
	case M4U_PORT_L11_IMG_MFB_WDMA1:
#else
	case M4U_PORT_L9_IMG_MFB_WDMA1_MDP:
#endif
	pr_info("msftdr:0x%08x, msstdr:0x%08x diptdr:0x%08x",
		DIP_RD32(MSF_BASE + 0x804),
		DIP_RD32(MSS_BASE + 0x804),
		DIP_RD32(DIP_A_BASE + 0x4));
	pr_info("msfcmdq:0x%08x, msscmdq:0x%08x",
		DIP_RD32(MSF_BASE + 0x7C0),
		DIP_RD32(MSS_BASE + 0x500));
	pr_info("mss top_cfg:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSS_BASE + 0x400),
		DIP_RD32(MSS_BASE + 0x404),
		DIP_RD32(MSS_BASE + 0x408));
	pr_info("msf top_ctl:0x%08x, dma_en:0x%08x, eng_en:0x%08x,",
		DIP_RD32(MSF_BASE + 0x480),
		DIP_RD32(MSF_BASE + 0x49C),
		DIP_RD32(MSF_BASE + 0x4A0));
	pr_info("mssoy:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSS_BASE + 0x900),
		DIP_RD32(MSS_BASE + 0x904),
		DIP_RD32(MSS_BASE + 0x908),
		DIP_RD32(MSS_BASE + 0x90C));
	pr_info("mssoc:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSS_BASE + 0x940),
		DIP_RD32(MSS_BASE + 0x944),
		DIP_RD32(MSS_BASE + 0x948),
		DIP_RD32(MSS_BASE + 0x94C));
	pr_info("omcoy:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSS_BASE + 0x980),
		DIP_RD32(MSS_BASE + 0x984),
		DIP_RD32(MSS_BASE + 0x988),
		DIP_RD32(MSS_BASE + 0x98C));
	pr_info("omcoc:0x%08x, ofset:0x%08x, size:0x%08x, stride:0x%08x,",
		DIP_RD32(MSS_BASE + 0x9C0),
		DIP_RD32(MSS_BASE + 0x9C4),
		DIP_RD32(MSS_BASE + 0x9C8),
		DIP_RD32(MSS_BASE + 0x9CC));
		break;
	default:  //
		for (loop = 0; loop < (0x39C/0x4); loop++) {
			pr_info("MSFREG: 0x%08X 0x%08X\n",
				MSF_BASE_HW + 0x900 + (loop * 0x4),
				DIP_RD32(MSF_BASE + 0x900 + (loop * 0x4)));
		}
		for (loop = 0; loop < (0x42C/0x4); loop++) {
			pr_info("MSFREG: 0x%08X 0x%08X\n",
				MSF_BASE_HW + 0x40 + (loop * 0x4),
				DIP_RD32(MSF_BASE + 0x40 + (loop * 0x4)));
		}
		for (loop = 0; loop < (0x19C/0x4); loop++) {
			pr_info("MSSREG: 0x%08X 0x%08X\n",
				MSS_BASE_HW + 0x240 + (loop * 0x4),
				DIP_RD32(MSS_BASE + 0x0240 + (loop * 0x4)));
		}
		for (loop = 0; loop < (0x15c/0x4); loop++) {
			pr_info("MSSREG: 0x%08X 0x%08X\n",
				MSS_BASE_HW + 0x900 + (loop * 0x4),
				DIP_RD32(MSS_BASE + 0x0900 + (loop * 0x4)));
		}
		break;
	}
#ifdef CONFIG_MTK_IOMMU_V2
	return MTK_IOMMU_CALLBACK_HANDLED;
#else
	return M4U_CALLBACK_HANDLED;
#endif
}

static signed int __init DIP_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
#if 0
	struct device_node *node = NULL;
#endif
	struct proc_dir_entry *proc_entry;
	struct proc_dir_entry *dip_p2_dir;

	int i;
	/*  */
	LOG_DBG("- E. Magic: %d", DIP_MAGIC);
	/*  */
	Ret = platform_driver_register(&DipDriver);
	if ((Ret) < 0) {
		LOG_ERR("platform_driver_register fail");
		return Ret;
	}
	/*  */

#if 0
	node = of_find_compatible_node(NULL, NULL, "mediatek,mmsys_config");
	if (!node) {
		LOG_ERR("find mmsys_config node failed!!!\n");
		return -ENODEV;
	}
	DIP_MMSYS_CONFIG_BASE = of_iomap(node, 0);
	if (!DIP_MMSYS_CONFIG_BASE) {
		LOG_ERR("unable to map DIP_MMSYS_CONFIG_BASE registers!!!\n");
		return -ENODEV;
	}
	LOG_DBG("DIP_MMSYS_CONFIG_BASE: %p\n", DIP_MMSYS_CONFIG_BASE);
#endif

	/* FIX-ME: linux-3.10 procfs API changed */
	dip_p2_dir = proc_mkdir("isp_p2", NULL);
	if (!dip_p2_dir) {
		LOG_ERR("[%s]: fail to mkdir /proc/isp_p2\n", __func__);
		return 0;
	}
	proc_entry = proc_create("dip_dump",
		0440, dip_p2_dir, &dip_dump_proc_fops);
	proc_entry = proc_create("isp_p2_dump",
		0440, dip_p2_dir, &dip_p2_dump_proc_fops);
	proc_entry = proc_create("isp_p2_kedump",
		0440, dip_p2_dir, &dip_p2_ke_dump_proc_fops);
	for (j = 0; j < DIP_IRQ_TYPE_AMOUNT; j++) {
		switch (j) {
		default:
			pBuf_kmalloc[j] = NULL;
			pTbl_RTBuf[j] = NULL;
			Tbl_RTBuf_MMPSize[j] = 0;
			break;
		}
	}


	/* isr log */
	if (PAGE_SIZE < ((DIP_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
		((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM)) {
		i = 0;
		while (i < ((DIP_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
			((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM))
			i += PAGE_SIZE;

	} else {
		i = PAGE_SIZE;
	}
	pLog_kmalloc = kmalloc(i, GFP_KERNEL);
	if ((pLog_kmalloc) == NULL) {
		LOG_ERR("mem not enough\n");
		return -ENOMEM;
	}
	memset(pLog_kmalloc, 0x00, i);
	tmp = pLog_kmalloc;
	for (i = 0; i < LOG_PPNUM; i++) {
		for (j = 0; j < DIP_IRQ_TYPE_AMOUNT; j++) {
			gSvLog[j]._str[i][_LOG_DBG] = (char *)tmp;
			tmp = (void *)((char *)tmp + (NORMAL_STR_LEN*DBG_PAGE));
			gSvLog[j]._str[i][_LOG_INF] = (char *)tmp;
			tmp = (void *)((char *)tmp + (NORMAL_STR_LEN*INF_PAGE));
			gSvLog[j]._str[i][_LOG_ERR] = (char *)tmp;
			tmp = (void *)((char *)tmp + (NORMAL_STR_LEN*ERR_PAGE));
		}
		/* log buffer,in case of overflow */
		tmp = (void *)((char *)tmp + NORMAL_STR_LEN);
	}
	/* mark the pages reserved , FOR MMAP*/
	for (j = 0; j < DIP_IRQ_TYPE_AMOUNT; j++) {
		if (pTbl_RTBuf[j] != NULL) {
			for (i = 0; i < Tbl_RTBuf_MMPSize[j]*PAGE_SIZE;
				i += PAGE_SIZE)
				SetPageReserved(
				virt_to_page(
				((unsigned long)pTbl_RTBuf[j]) + i));
		}
	}

#ifndef EP_CODE_MARK_CMDQ
	/* Register DIP callback */
	LOG_DBG("register dip callback for MDP");
	cmdqCoreRegisterCB(CMDQ_GROUP_ISP,
			   NULL,
			   DIP_MDPDumpCallback,
			   DIP_MDPResetCallback,
			   NULL);
	/* Register GCE callback for dumping DIP register */
	LOG_DBG("register dip callback for GCE");
	cmdqCoreRegisterDebugRegDumpCB
		(DIP_BeginGCECallback, DIP_EndGCECallback);
#endif
	/* m4u_enable_tf(M4U_PORT_CAM_IMGI, 0);*/
#ifdef CONFIG_MTK_IOMMU_V2

#if (MTK_MSF_OFFSET == 1)
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_IMGI_D1,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_IMGBI_D1,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_DMGI_D1,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_DEPI_D1,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_ICE_D1,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_SMTI_D1,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_SMTO_D1,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_SMTO_D2,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_CRZO_D1,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_IMG3O_D1,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_VIPI_D1,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_SMTI_D5,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_TIMGO_D1,
					  ISP_M4U_TranslationFault_callback,
					  NULL);

	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_MFB_RDMA0,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_MFB_RDMA1,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_MFB_RDMA2,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_MFB_RDMA3,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_MFB_RDMA4,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_MFB_RDMA5,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_MFB_WDMA0,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L11_IMG_MFB_WDMA1,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
#else
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_IMGI_D1_MDP,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_IMGBI_D1_MDP,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_DMGI_D1_MDP,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_DEPI_D1_MDP,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_ICE_D1_MDP,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_SMTI_D1_MDP,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_SMTO_D1_MDP,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_SMTO_D2_MDP,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_CRZO_D1_MDP,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_IMG3O_D1_MDP,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_VIPI_D1_MDP,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_SMTI_D5_MDP,
					  ISP_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_TIMGO_D1_MDP,
					  ISP_M4U_TranslationFault_callback,
					  NULL);

	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_MFB_RDMA0_MDP,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_MFB_RDMA1_MDP,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_MFB_RDMA2_MDP,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_MFB_RDMA3_MDP,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_MFB_RDMA4_MDP,
					   MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_MFB_RDMA5_MDP,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_MFB_WDMA0_MDP,
					  MFB_M4U_TranslationFault_callback,
					  NULL);
	mtk_iommu_register_fault_callback(M4U_PORT_L9_IMG_MFB_WDMA1_MDP,
					  MFB_M4U_TranslationFault_callback,
					  NULL);

#endif
#else
  //#ifndef CONFIG_FPGA_EARLY_PORTING   //Todo: Justin EP, mt6789 porting
	m4u_register_fault_callback(M4U_PORT_IMGI_D1,
			ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_IMGBI_D1,
			ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_DMGI_D1,
			ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_DEPI_D1,
			ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_LCEI_D1,
			ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_SMTI_D1,
			ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_SMTO_D1,
			ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_SMTO_D2,
			ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_CRZO_D1,
			ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_IMG3O_D1,
			ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_VIPI_D1,
			ISP_M4U_TranslationFault_callback, NULL);
	m4u_register_fault_callback(M4U_PORT_TIMGO_D1,
			ISP_M4U_TranslationFault_callback, NULL);
 //#endif
#endif
	LOG_DBG("- X. Ret: %d.", Ret);
	return Ret;
}

/**************************************************************
 *
 **************************************************************/
static void __exit DIP_Exit(void)
{
	int i, j;

	LOG_DBG("- E.");
	/*  */
	platform_driver_unregister(&DipDriver);
	/*  */
#ifndef EP_CODE_MARK_CMDQ
	/* Unregister DIP callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_ISP,
			   NULL,
			   NULL,
			   NULL,
			   NULL);
	/* Un-Register GCE callback */
	LOG_DBG("Un-register dip callback for GCE");
	cmdqCoreRegisterDebugRegDumpCB(NULL, NULL);
#endif


	for (j = 0; j < DIP_IRQ_TYPE_AMOUNT; j++) {
		if (pTbl_RTBuf[j] != NULL) {
			/* unreserve the pages */
			for (i = 0; i < Tbl_RTBuf_MMPSize[j]*PAGE_SIZE;
				i += PAGE_SIZE)
				ClearPageReserved(
					virt_to_page(
					((unsigned long)pTbl_RTBuf[j]) + i));

			/* free the memory areas */
			kfree(pBuf_kmalloc[j]);
		}
	}

	/* free the memory areas */
	kfree(pLog_kmalloc);

	/*  */
}

int32_t DIP_MDPClockOnCallback(uint64_t engineFlag)
{
	/* LOG_DBG("DIP_MDPClockOnCallback"); */
	/*LOG_DBG("+MDPEn:%d", G_u4DipEnClkCnt);*/
#ifdef CONFIG_PM_SLEEP
	__pm_stay_awake(isp_mdp_wake_lock);
#endif
	DIP_EnableClock(MTRUE);

	return 0;
}

int32_t DIP_MDPDumpCallback(uint64_t engineFlag, int level)
{
// Justin Todo, check mt6885 support cmdq_core_query or not support
	const char *pCmdq1stErrCmd;

	LOG_DBG("DIP_MDPDumpCallback");
#if 1

	pCmdq1stErrCmd = cmdq_core_query_first_err_mod();
	if (pCmdq1stErrCmd != NULL) {
		cmdq_util_err("Cmdq 1st Error:%s", pCmdq1stErrCmd);
		if (strncmp(pCmdq1stErrCmd, "DIP", 3) == 0) {
			cmdq_util_err("DIP is 1st Error!!");
			g_dip1sterr = DIP_GCE_EVENT_DIP;
		} else if (strncmp(pCmdq1stErrCmd, "DPE", 3) == 0) {
			cmdq_util_err("DPE is 1st Error!!");
			g_dip1sterr = DIP_GCE_EVENT_DPE;
		} else if (strncmp(pCmdq1stErrCmd, "RSC", 3) == 0) {
			cmdq_util_err("RSC is 1st Error!!");
			g_dip1sterr = DIP_GCE_EVENT_RSC;
		} else if (strncmp(pCmdq1stErrCmd, "WPE", 3) == 0) {
			cmdq_util_err("WPE is 1st Error!!");
			g_dip1sterr = DIP_GCE_EVENT_WPE;
		} else if (strncmp(pCmdq1stErrCmd, "MFB", 3) == 0) {
			cmdq_util_err("MFB is 1st Error!!");
			g_dip1sterr = DIP_GCE_EVENT_MFB;
		} else if (strncmp(pCmdq1stErrCmd, "FDVT", 4) == 0) {
			cmdq_util_err("FDVT is 1st Error!!");
			g_dip1sterr = DIP_GCE_EVENT_FDVT;
		} else if (strncmp(pCmdq1stErrCmd, "DISP", 4) == 0) {
			cmdq_util_err("DISP is 1st Error!!");
			g_dip1sterr = DIP_GCE_EVENT_DIP;
		} else if (strncmp(pCmdq1stErrCmd, "JPGE", 4) == 0) {
			cmdq_util_err("JPGE is 1st Error!!");
			g_dip1sterr = DIP_GCE_EVENT_JPGE;
		} else if (strncmp(pCmdq1stErrCmd, "VENC", 4) == 0) {
			cmdq_util_err("VENC is 1st Error!!");
			g_dip1sterr = DIP_GCE_EVENT_VENC;
		} else if (strncmp(pCmdq1stErrCmd, "CMDQ", 4) == 0) {
			cmdq_util_err("CMDQ is 1st Error!!");
			g_dip1sterr = DIP_GCE_EVENT_CMDQ;
		} else {
			cmdq_util_err("the others is 1st Error!!");
			g_dip1sterr = DIP_GCE_EVENT_THEOTHERS;
		}
	}
#endif
	if (G_u4DipEnClkCnt > 0)
		DIP_DumpDIPReg();
	else
		LOG_DBG("G_u4DipEnClkCnt(%d) <= 0\n", G_u4DipEnClkCnt);

	return 0;
}
int32_t DIP_MDPResetCallback(uint64_t engineFlag)
{
	LOG_DBG("DIP_MDPResetCallback");

	DIP_Reset(DIP_REG_SW_CTL_RST_CAM_P2);

	return 0;
}

int32_t DIP_MDPClockOffCallback(uint64_t engineFlag)
{
	/*LOG_INF("DIP_MDPClockOffCallback");*/
	DIP_EnableClock(MFALSE);
#ifdef CONFIG_PM_SLEEP
	__pm_relax(isp_mdp_wake_lock);
#endif
	/*LOG_INF("-MDPEn:%d", G_u4DipEnClkCnt);*/
	return 0;
}


#if DUMP_GCE_TPIPE
#define DIP_IMGSYS_BASE_PHY_KK 0x15022000

static uint32_t addressToDump[] = {
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x000),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x004),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x008),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x00C),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x010),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x014),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x018),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x01C),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x204),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x208),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x20C),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x400),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x408),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x410),
(uint32_t)(DIP_IMGSYS_BASE_PHY_KK + 0x414),
};

#endif

int32_t DIP_BeginGCECallback(
	uint32_t taskID, uint32_t *regCount, uint32_t **regAddress)
{
#if DUMP_GCE_TPIPE
	LOG_DBG("+,taskID(%d)", taskID);

	*regCount = sizeof(addressToDump) / sizeof(uint32_t);
	*regAddress = (uint32_t *)addressToDump;

	LOG_DBG("-,*regCount(%d)", *regCount);
#endif
	return 0;
}

int32_t DIP_EndGCECallback(
	uint32_t taskID, uint32_t regCount, uint32_t *regValues)
{
#if DUMP_GCE_TPIPE
	#define PER_LINE_LOG_SIZE   10
	int32_t i, j, pos;
	/* uint32_t add[PER_LINE_LOG_SIZE]; */
	uint32_t add[PER_LINE_LOG_SIZE];
	uint32_t val[PER_LINE_LOG_SIZE];

#if DUMP_GCE_TPIPE
	int32_t tpipePA;
	int32_t ctlStart;
	unsigned long map_va = 0;
	uint32_t map_size;
	int32_t *pMapVa;
	#define TPIPE_DUMP_SIZE    200
#endif

	LOG_DBG("End taskID(%d),regCount(%d)", taskID, regCount);

	for (i = 0; i < regCount; i += PER_LINE_LOG_SIZE) {
		for (j = 0; j < PER_LINE_LOG_SIZE; j++) {
			pos = i + j;
			if (pos < regCount) {
				add[j] = addressToDump[pos];
				val[j] = regValues[pos];
			}
		}
		LOG_DBG("[0x%08x,0x%08x][0x%08x,0x%08x][0x%08x,0x%08x]\n",
			add[0], val[0], add[1], val[1], add[2], val[2]);
		LOG_DBG("[0x%08x,0x%08x][0x%08x,0x%08x]\n",
			add[3], val[3], add[4], val[4]);
		LOG_DBG("[0x%08x,0x%08x][0x%08x,0x%08x][0x%08x,0x%08x]\n",
			add[5], val[5], add[6], val[6], add[7], val[7]);
		LOG_DBG("[0x%08x,0x%08x][0x%08x,0x%08x]\n",
			add[8], val[8], add[9], val[9]);
	}


	/* tpipePA = DIP_RD32(DIP_IMGSYS_BASE_PHY_KK + 0x204); */
	tpipePA = val[7];
	/* ctlStart = DIP_RD32(DIP_IMGSYS_BASE_PHY_KK + 0x000); */
	ctlStart = val[0];

	LOG_DBG("kk:tpipePA(0x%x), ctlStart(0x%x)", tpipePA, ctlStart);

	if ((tpipePA)) {
#ifdef AEE_DUMP_BY_USING_ION_MEMORY
		tpipePA = tpipePA&0xfffff000;
		struct dip_imem_memory dip_p2GCEdump_imem_buf;

		struct ion_client *dip_p2GCEdump_ion_client;

		dip_p2GCEdump_imem_buf.handle = NULL;
		dip_p2GCEdump_imem_buf.ion_fd = 0;
		dip_p2GCEdump_imem_buf.va = 0;
		dip_p2GCEdump_imem_buf.pa = 0;
		dip_p2GCEdump_imem_buf.length = TPIPE_DUMP_SIZE;
		if ((dip_p2_ion_client == NULL) && (g_ion_device))
			dip_p2_ion_client =
				ion_client_create(g_ion_device, "dip_p2");
		if (dip_p2_ion_client == NULL)
			LOG_ERR("invalid dip_p2_ion_client client!\n");
		if (dip_allocbuf(&dip_p2GCEdump_imem_buf) >= 0) {
			pMapVa = (int *)dip_p2GCEdump_imem_buf.va;
		LOG_DBG("ctlStart(0x%x),tpipePA(0x%x)", ctlStart, tpipePA);

		if (pMapVa) {
			for (i = 0; i < TPIPE_DUMP_SIZE; i += 10) {
				LOG_DBG("[idx(%d)]%08X-%08X-%08X-%08X-%08X",
				i,
				pMapVa[i], pMapVa[i+1], pMapVa[i+2],
				pMapVa[i+3], pMapVa[i+4]);
				LOG_DBG("%08X-%08X-%08X-%08X-%08X\n",
				pMapVa[i+5], pMapVa[i+6], pMapVa[i+7],
				pMapVa[i+8], pMapVa[i+9]);
			}
		}
			dip_freebuf(&dip_p2GCEdump_imem_buf);
			dip_p2GCEdump_imem_buf.handle = NULL;
			dip_p2GCEdump_imem_buf.ion_fd = 0;
			dip_p2GCEdump_imem_buf.va = 0;
			dip_p2GCEdump_imem_buf.pa = 0;
		}
#endif
	}
#endif

	return 0;
}

irqreturn_t DIP_Irq_DIP_A(signed int  Irq, void *DeviceId)
{
	int i = 0;
	unsigned int IrqINTStatus = 0x0;
	unsigned int IrqCQStatus = 0x0;
	unsigned int IrqCQLDStatus = 0x0;

	/*LOG_DBG("DIP_Irq_DIP_A:%d\n", Irq);*/

	/* Avoid touch hwmodule when clock is disable. */
	/* DEVAPC will moniter this kind of err */
	if (G_u4DipEnClkCnt == 0)
		return IRQ_HANDLED;

	spin_lock(&(IspInfo.SpinLockIrq[DIP_IRQ_TYPE_INT_DIP_A_ST]));
	/* DIP_A_REG_CTL_INT_STATUS */
	IrqINTStatus = DIP_RD32(DIP_A_BASE + 0x030);
	/* DIP_A_REG_CTL_CQ_INT_STATUS */
	IrqCQStatus = DIP_RD32(DIP_A_BASE + 0x034);
	IrqCQLDStatus = DIP_RD32(DIP_A_BASE + 0x038);

	for (i = 0; i < IRQ_USER_NUM_MAX; i++)
		IspInfo.IrqInfo.Status[DIP_IRQ_TYPE_INT_DIP_A_ST][i]
		|= IrqINTStatus;

	spin_unlock(&(IspInfo.SpinLockIrq[DIP_IRQ_TYPE_INT_DIP_A_ST]));

	/* LOG_DBG("DIP_Irq_DIP_A:%d, reg 0x%p : 0x%x, */
	/* reg 0x%p : 0x%x\n", */
	/* Irq, */
	/* (DIP_A_BASE + 0x030), IrqINTStatus, */
	/* (DIP_A_BASE + 0x034), IrqCQStatus); */

	/*  */
	wake_up_interruptible
		(&IspInfo.WaitQueueHead[DIP_IRQ_TYPE_INT_DIP_A_ST]);

	return IRQ_HANDLED;

}

/**************************************************************
 *
 **************************************************************/

#if (DIP_BOTTOMHALF_WORKQ == 1)
static void DIP_BH_Workqueue(struct work_struct *pWork)
{
	struct IspWorkqueTable *pWorkTable =
		container_of(pWork, struct IspWorkqueTable, dip_bh_work);

	IRQ_LOG_PRINTER(pWorkTable->module, m_CurrentPPB, _LOG_ERR);
	IRQ_LOG_PRINTER(pWorkTable->module, m_CurrentPPB, _LOG_INF);
}
#endif

/**************************************************************
 *
 **************************************************************/
module_init(DIP_Init);
module_exit(DIP_Exit);
MODULE_DESCRIPTION("Camera DIP driver");
MODULE_AUTHOR("MM3/SW5");
MODULE_LICENSE("GPL");

