// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2022 MediaTek Inc.
 */



/**************************************************************
 * camera_dip.c - MT6799 Linux DIP Device Driver
 *
 * DESCRIPTION:
 *     This file provid the other drivers DIP relative functions
 *
 **************************************************************/
/* MET: define to enable MET*/

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

/*#include <mach/irqs.h>*/
/* For clock mgr APIS. enable_clock()/disable_clock(). */
/*#include <mach/mt_clkmgr.h>*/
#include <mt-plat/sync_write.h> /* For reg_sync_writel(). */
/* For spm_enable_sodi()/spm_disable_sodi(). */
/* #include <mach/mt_spm_idle.h> */

#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#ifdef CONFIG_MTK_IOMMU
#include <mtk_iommu.h>
#elif defined(CONFIG_MTK_IOMMU_V2)
#include <mach/mt_iommu.h>
#else
#include <m4u.h>
#endif

#ifndef EP_CODE_MARK_CMDQ
#include <cmdq_core.h>
#endif

#define EP_CODE_MARK_KERNEL414

#ifndef EP_CODE_MARK_SMI
#include <smi_public.h>
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>


/*for kernel log count*/
#define _K_LOG_ADJUST (1)

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
#define DUMMY_INT           (0) /*for early if load dont need to use camera*/

/* Clkmgr is not ready in early porting, en/disable clock  by hardcode */
/* #define EP_NO_CLKMGR */

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
 * Note!!! The order and member of .compatible must be the same with that in
 *  "DIP_DEV_NODE_ENUM" in camera_dip.h
 */
static const struct of_device_id dip_of_ids[] = {
	{ .compatible = "mediatek,imgsys1", },
	{ .compatible = "mediatek,dip1", },
	{}
};

#endif

struct Dip_Init_Array {
	unsigned int ofset;
	unsigned int val;
};

#define DIP_INIT_ARRAY_COUNT  118
const struct Dip_Init_Array DIP_INIT_ARY[DIP_INIT_ARRAY_COUNT] = {
	{0x00A0, 0x1},   //DIP_X_CTL_TOP_DCM_DIS
	{0x0020, 0x0},   //DIP_X_REG_CTL_INT_EN
	{0x0024, 0x0},   //DIP_X_REG_CTL_CQ_INT_EN
	{0x0028, 0x0},   //DIP_X_REG_CTL_CQ_INT2_EN
	{0x002C, 0x0},   //DIP_X_REG_CTL_CQ_INT3_EN
	{0x0204, 0x11},   //DIP_X_CQ_THR0_CTL
	{0x0210, 0x11},   //DIP_X_CQ_THR1_CTL
	{0x021C, 0x11},   //DIP_X_CQ_THR2_CTL
	{0x0228, 0x11},   //DIP_X_CQ_THR3_CTL
	{0x0234, 0x11},   //DIP_X_CQ_THR4_CTL
	{0x0240, 0x11},   //DIP_X_CQ_THR5_CTL
	{0x024C, 0x11},   //DIP_X_CQ_THR6_CTL
	{0x0258, 0x11},   //DIP_X_CQ_THR7_CTL
	{0x0264, 0x11},   //DIP_X_CQ_THR8_CTL
	{0x0270, 0x11},   //DIP_X_CQ_THR9_CTL
	{0x027C, 0x11},   //DIP_X_CQ_THR10_CTL
	{0x0288, 0x11},   //DIP_X_CQ_THR11_CTL
	{0x0294, 0x11},   //DIP_X_CQ_THR12_CTL
	{0x02A0, 0x11},   //DIP_X_CQ_THR13_CTL
	{0x02AC, 0x11},   //DIP_X_CQ_THR14_CTL
	{0x02B8, 0x11},   //DIP_X_CQ_THR15_CTL
	{0x02C4, 0x11},   //DIP_X_CQ_THR16_CTL
	{0x02D0, 0x11},   //DIP_X_CQ_THR17_CTL
	{0x02DC, 0x11},   //DIP_X_CQ_THR18_CTL
	{0x020C, 0x308},    //DIP_X_CQ_THR0_DESC_SIZE
	{0x0218, 0x308},    //DIP_X_CQ_THR1_DESC_SIZE
	{0x0224, 0x308},    //DIP_X_CQ_THR2_DESC_SIZE
	{0x0230, 0x308},    //DIP_X_CQ_THR3_DESC_SIZE
	{0x023C, 0x308},    //DIP_X_CQ_THR4_DESC_SIZE
	{0x0248, 0x308},    //DIP_X_CQ_THR5_DESC_SIZE
	{0x0254, 0x308},    //DIP_X_CQ_THR6_DESC_SIZE
	{0x0260, 0x308},    //DIP_X_CQ_THR7_DESC_SIZE
	{0x026C, 0x308},    //DIP_X_CQ_THR8_DESC_SIZE
	{0x0278, 0x308},    //DIP_X_CQ_THR9_DESC_SIZE
	{0x0284, 0x308},    //DIP_X_CQ_THR10_DESC_SIZE
	{0x0290, 0x308},    //DIP_X_CQ_THR11_DESC_SIZE
	{0x029C, 0x308},    //DIP_X_CQ_THR12_DESC_SIZE
	{0x02A8, 0x308},    //DIP_X_CQ_THR13_DESC_SIZE
	{0x02B4, 0x308},    //DIP_X_CQ_THR14_DESC_SIZE
	{0x02C0, 0x308},    //DIP_X_CQ_THR15_DESC_SIZE
	{0x02CC, 0x308},    //DIP_X_CQ_THR16_DESC_SIZE
	{0x02D8, 0x308},    //DIP_X_CQ_THR17_DESC_SIZE
	{0x02E4, 0x308},    //DIP_X_CQ_THR18_DESC_SIZE
	{0x34C, 0x80000040},    //DIP_X_IMG2O_CON
	{0x350, 0x00400040},    //DIP_X_IMG2O_CON2
	{0x354, 0x00150015},    //DIP_X_IMG2O_CON3
	{0x37C, 0x80000040},    //DIP_X_IMG2BO_CON
	{0x380, 0x00400040},    //DIP_X_IMG2bO_CON2
	{0x384, 0x00150015},    //DIP_X_IMG2bO_CON3
	{0x3AC, 0x80000040},    //DIP_X_IMG3O_CON
	{0x3B0, 0x00400040},    //DIP_X_IMG3O_CON2
	{0x3B4, 0x00150015},    //DIP_X_IMG3O_CON3
	{0x3DC, 0x80000020},    //DIP_X_IMG3BO_CON
	{0x3E0, 0x00200020},    //DIP_X_IMG3BO_CON2
	{0x3E4, 0x000A000A},    //DIP_X_IMG3BO_CON3
	{0x40C, 0x80000020},    //DIP_X_IMG3CO_CON
	{0x410, 0x00200020},    //DIP_X_IMG3CO_CON2
	{0x414, 0x000A000A},    //DIP_X_IMG3CO_CON3
	{0x43C, 0x80000040},    //DIP_X_FEO_CON
	{0x440, 0x00400040},    //DIP_X_FEO_CON2
	{0x444, 0x00150015},    //DIP_X_FEO_CON3
	{0x46C, 0x80000040},    //DIP_X_PAK2O_CON
	{0x470, 0x00400040},    //DIP_X_PAK2O_CON2
	{0x474, 0x00150015},    //DIP_X_PAK2O_CON3
	{0x51C, 0x80000080},    //DIP_X_IMGI_CON
	{0x520, 0x00800080},    //DIP_X_IMGI_CON2
	{0x524, 0x002A002A},    //DIP_X_IMGI_CON3
	{0x54C, 0x80000040},    //DIP_X_IMGBI_CON
	{0x550, 0x00400040},    //DIP_X_IMGBI_CON2
	{0x554, 0x00150015},    //DIP_X_IMGBI_CON3
	{0x57C, 0x80000040},    //DIP_X_IMGCI_CON
	{0x580, 0x00400040},    //DIP_X_IMGCI_CON2
	{0x584, 0x00150015},    //DIP_X_IMGCI_CON3
	{0x5AC, 0x80000080},    //DIP_X_VIPI_CON
	{0x5B0, 0x00800080},    //DIP_X_VIPI_CON2
	{0x5B4, 0x002A002A},    //DIP_X_VIPI_CON3
	{0x5DC, 0x80000040},    //DIP_X_VIP2I_CON
	{0x5E0, 0x00400040},    //DIP_X_VIP2I_CON2
	{0x5E4, 0x00150015},    //DIP_X_VIP2I_CON3
	{0x60C, 0x80000040},    //DIP_X_VIP3I_CON
	{0x610, 0x00400040},    //DIP_X_VIP3I_CON2
	{0x614, 0x00150015},    //DIP_X_VIP3I_CON3
	{0x63C, 0x80000020},    //DIP_X_DMGI_CON
	{0x640, 0x00200020},    //DIP_X_DMGI_CON2
	{0x644, 0x000A000A},    //DIP_X_DMGI_CON3
	{0x66C, 0x80000020},    //DIP_X_DEPI_CON
	{0x670, 0x00200020},    //DIP_X_DEPI_CON2
	{0x674, 0x000A000A},    //DIP_X_DEPI_CON3
	{0x69C, 0x80000020},    //DIP_X_LCEI_CON
	{0x6A0, 0x00200020},    //DIP_X_LCEI_CON2
	{0x6A4, 0x000A000A},    //DIP_X_LCEI_CON3
	{0x6CC, 0x80000020},    //DIP_X_UFDI_CON
	{0x6D0, 0x00200020},    //DIP_X_UFDI_CON2
	{0x6D4, 0x000A000A},    //DIP_X_UFDI_CON3
	{0x7EC, 0x80000040},    //DIP_X_SMX1O_CON
	{0x7F0, 0x00400040},    //DIP_X_SMX1O_CON2
	{0x7F4, 0x00400040},    //DIP_X_SMX1O_CON3
	{0x81C, 0x80000040},    //DIP_X_SMX2O_CON
	{0x820, 0x00400040},    //DIP_X_SMX2O_CON2
	{0x824, 0x00400040},    //DIP_X_SMX2O_CON3
	{0x84C, 0x80000040},    //DIP_X_SMX3O_CON
	{0x850, 0x00400040},    //DIP_X_SMX3O_CON2
	{0x854, 0x00400040},    //DIP_X_SMX3O_CON3
	{0x87C, 0x80000040},    //DIP_X_SMX4O_CON
	{0x880, 0x00400040},    //DIP_X_SMX4O_CON2
	{0x884, 0x00400040},    //DIP_X_SMX4O_CON3
	{0x8AC, 0x80000040},    //DIP_A_SMX1I_CON
	{0x8B0, 0x00400040},    //DIP_A_SMX1I_CON2
	{0x8B4, 0x00400040},    //DIP_A_SMX1I_CON3
	{0x8DC, 0x80000040},    //DIP_A_SMX2I_CON
	{0x8E0, 0x00400040},    //DIP_A_SMX2I_CON2
	{0x8E4, 0x00400040},    //DIP_A_SMX2I_CON3
	{0x90C, 0x80000040},    //DIP_A_SMX3I_CON
	{0x910, 0x00400040},    //DIP_A_SMX3I_CON2
	{0x914, 0x00400040},    //DIP_A_SMX3I_CON3
	{0x93C, 0x80000040},    //DIP_A_SMX4I_CON
	{0x940, 0x00400040},    //DIP_A_SMX4I_CON2
	{0x944, 0x00400040}     //DIP_A_SMX4I_CON3
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
	struct clk *DIP_IMG_LARB5;
	struct clk *DIP_IMG_DIP;
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

#define AEE_DUMP_BY_USING_ION_MEMORY
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
static struct DIP_MEM_INFO_STRUCT g_TpipeBaseAddrInfo = {0x0, 0x0, NULL, 0x0};
static struct DIP_MEM_INFO_STRUCT g_CmdqBaseAddrInfo = {0x0, 0x0, NULL, 0x0};
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

#else
#define DIP_ADDR                        (IMGSYS_BASE + 0x4000)
#define DIP_IMGSYS_BASE                 IMGSYS_BASE
#define DIP_ADDR_CAMINF                 IMGSYS_BASE
#define DIP_MIPI_ANA_ADDR               0x10217000
#define DIP_GPIO_ADDR                   GPIO_BASE

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
		usec = do_div(sec, 1000000);\
	}

#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...) do {\
	char *ptr; \
	char *pDes;\
	signed int avaLen;\
	unsigned int *ptr2 = &gSvLog[irq]._cnt[ppb][logT];\
	unsigned int str_leng;\
	unsigned int i;\
	struct SV_LOG_STR *pSrc = &gSvLog[irq];\
	if (logT == _LOG_ERR) {\
		str_leng = NORMAL_STR_LEN*ERR_PAGE; \
	} else if (logT == _LOG_DBG) {\
		str_leng = NORMAL_STR_LEN*DBG_PAGE; \
	} else if (logT == _LOG_INF) {\
		str_leng = NORMAL_STR_LEN*INF_PAGE;\
	} else {\
		str_leng = 0;\
	} \
	ptr = pDes = \
	(char *)&(gSvLog[irq]._str[ppb][logT][gSvLog[irq]._cnt[ppb][logT]]);\
	avaLen = str_leng - 1 - gSvLog[irq]._cnt[ppb][logT];\
	if (avaLen > 1) {\
		snprintf((char *)(pDes), avaLen, "[%d.%06d]" fmt,\
		gSvLog[irq]._lastIrqTime.sec, gSvLog[irq]._lastIrqTime.usec,\
		##__VA_ARGS__);   \
	if ('\0' != gSvLog[irq]._str[ppb][logT][str_leng - 1]) {\
		LOG_ERR("log str over flow(%d)", irq);\
	} \
	while (*ptr++ != '\0') {        \
		(*ptr2)++;\
	}     \
	} else { \
		LOG_INF("(%d)(%d)log str avalible=0, print log\n", irq, logT);\
	ptr = pSrc->_str[ppb][logT];\
	if (pSrc->_cnt[ppb][logT] != 0) {\
		if (logT == _LOG_DBG) {\
			for (i = 0; i < DBG_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else {\
					LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else if (logT == _LOG_INF) {\
			for (i = 0; i < INF_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else {\
					LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else if (logT == _LOG_ERR) {\
			for (i = 0; i < ERR_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else {\
					LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else {\
			LOG_ERR("N.S.%d", logT);\
		} \
		ptr[0] = '\0';\
		pSrc->_cnt[ppb][logT] = 0;\
		avaLen = str_leng - 1;\
		ptr = pDes = \
		(char *)&(pSrc->_str[ppb][logT][pSrc->_cnt[ppb][logT]]);\
		ptr2 = &(pSrc->_cnt[ppb][logT]);\
		snprintf((char *)(pDes), avaLen, fmt, ##__VA_ARGS__);   \
		while (*ptr++ != '\0') {\
			(*ptr2)++;\
		} \
	} \
	} \
} while (0)

#define IRQ_LOG_PRINTER(irq, ppb_in, logT_in) do {\
	struct SV_LOG_STR *pSrc = &gSvLog[irq];\
	char *ptr;\
	unsigned int i;\
	unsigned int ppb = 0;\
	unsigned int logT = 0;\
	if (ppb_in > 1) {\
		ppb = 1;\
	} else {\
		ppb = ppb_in;\
	} \
	if (logT_in > _LOG_ERR) {\
		logT = _LOG_ERR;\
	} else {\
		logT = logT_in;\
	} \
	ptr = pSrc->_str[ppb][logT];\
	if (pSrc->_cnt[ppb][logT] != 0) {\
		if (logT == _LOG_DBG) {\
			for (i = 0; i < DBG_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else {\
					LOG_DBG("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else if (logT == _LOG_INF) {\
			for (i = 0; i < INF_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else {\
					LOG_INF("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else if (logT == _LOG_ERR) {\
			for (i = 0; i < ERR_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else {\
					LOG_ERR("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else {\
			LOG_ERR("N.S.%d", logT);\
		} \
		ptr[0] = '\0';\
		pSrc->_cnt[ppb][logT] = 0;\
	} \
	} while (0)


/*#define CAMSYS_REG_CG_CON               (DIP_CAMSYS_CONFIG_BASE + 0x0)*/
#define IMGSYS_REG_CG_CON               (DIP_IMGSYS_CONFIG_BASE + 0x0)
/*#define CAMSYS_REG_CG_SET               (DIP_CAMSYS_CONFIG_BASE + 0x4)*/
#define IMGSYS_REG_CG_SET               (DIP_IMGSYS_CONFIG_BASE + 0x4)
/*#define CAMSYS_REG_CG_CLR               (DIP_CAMSYS_CONFIG_BASE + 0x8)*/
#define IMGSYS_REG_CG_CLR               (DIP_IMGSYS_CONFIG_BASE + 0x8)

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

static signed int DIP_DumpDIPReg(void)
{
	signed int Ret = 0;
#ifndef EP_CODE_MARK_CMDQ
	unsigned int i, cmdqidx = 0, dipdmacmd = 0;
	unsigned int smidmacmd = 0, dmaidx = 0;
	unsigned int fifodmacmd = 0;
#ifdef AEE_DUMP_REDUCE_MEMORY
	unsigned int offset = 0;
	uintptr_t OffsetAddr = 0;
	unsigned int ctrl_start;
#endif
	/*  */
	CMDQ_ERR("- E.");
	CMDQ_ERR("g_bDumpPhyDIPBuf:(0x%x), g_pPhyDIPBuffer:(0x%p)\n",
		g_bDumpPhyDIPBuf, g_pPhyDIPBuffer);
	CMDQ_ERR("g_bIonBuf:(0x%x)\n", g_bIonBufferAllocated);
#ifdef AEE_DUMP_REDUCE_MEMORY
	if (g_bDumpPhyDIPBuf == MFALSE) {
		ctrl_start = DIP_RD32(DIP_A_BASE + 0x0000);
		if (g_bIonBufferAllocated == MFALSE) {
			if (g_pPhyDIPBuffer != NULL) {
				CMDQ_ERR("g_pPhyDIPBuffer isn't NULL(0x%pK)\n",
					g_pPhyDIPBuffer);
				vfree(g_pPhyDIPBuffer);
				g_pPhyDIPBuffer = NULL;
			}
			g_pPhyDIPBuffer = vmalloc(DIP_REG_RANGE);
			if (g_pPhyDIPBuffer == NULL)
				CMDQ_ERR("g_pPhyDIPBuffer kmalloc failed\n");

			if (g_pKWTpipeBuffer != NULL) {
				CMDQ_ERR("g_pKWTpipeBuffer isn't NULL(0x%pK)\n",
					g_pKWTpipeBuffer);
					vfree(g_pKWTpipeBuffer);
					g_pKWTpipeBuffer = NULL;
			}
			g_pKWTpipeBuffer = vmalloc(MAX_ISP_TILE_TDR_HEX_NO);
			if (g_pKWTpipeBuffer == NULL)
				CMDQ_ERR("g_pKWTpipeBuffer kmalloc failed\n");

			if (g_pKWCmdqBuffer != NULL) {
				CMDQ_ERR("g_KWCmdqBuffer isn't NULL(0x%pK)\n",
					g_pKWCmdqBuffer);
				vfree(g_pKWCmdqBuffer);
				g_pKWCmdqBuffer = NULL;
			}
			g_pKWCmdqBuffer = vmalloc(MAX_DIP_CMDQ_BUFFER_SIZE);
			if (g_pKWCmdqBuffer == NULL)
				CMDQ_ERR("g_KWCmdqBuffer kmalloc failed\n");

			if (g_pKWVirDIPBuffer != NULL) {
				CMDQ_ERR("g_KWVirDIPBuffer isn't NULL(0x%pK)\n",
					g_pKWVirDIPBuffer);
				vfree(g_pKWVirDIPBuffer);
				g_pKWVirDIPBuffer = NULL;
			}
			g_pKWVirDIPBuffer = vmalloc(DIP_REG_RANGE);
			if (g_pKWVirDIPBuffer == NULL)
				CMDQ_ERR("g_KWVirDIPBuffer kmalloc failed\n");
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
		} else {
			CMDQ_ERR("g_pPhyDIPBuffer:(0x%pK)\n", g_pPhyDIPBuffer);
		}
		g_dumpInfo.tdri_baseaddr = DIP_RD32(DIP_A_BASE + 0x304);
		g_dumpInfo.imgi_baseaddr = DIP_RD32(DIP_A_BASE + 0x500);
		g_dumpInfo.dmgi_baseaddr = DIP_RD32(DIP_A_BASE + 0x620);
		g_tdriaddr = g_dumpInfo.tdri_baseaddr;
		/*0x15022208, DIP_X_CQ_THR0_BASEADDR*/
		for (cmdqidx = 0; cmdqidx < 32 ; cmdqidx++) {
			if (ctrl_start & (0x1<<cmdqidx)) {
				g_cmdqaddr = DIP_RD32(DIP_A_BASE + 0x208 +
					(cmdqidx*DIP_CMDQ_BASEADDR_OFFSET));
				break;
			}
		}
		g_dumpInfo.cmdq_baseaddr = g_cmdqaddr;
		if ((g_TpipeBaseAddrInfo.MemPa != 0) &&
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
				CMDQ_ERR("cpy tpipe fail. tdriaddr:0x%x\n",
				g_tdriaddr);
			CMDQ_ERR("MemVa:0x%lx, MemPa:0x%x, offset:0x%x\n",
				(uintptr_t)g_TpipeBaseAddrInfo.MemVa,
				g_TpipeBaseAddrInfo.MemPa,
				offset);
			}
		}
		CMDQ_ERR("tdraddr:0x%x,MemVa:0x%lx,MemPa:0x%x\n",
			g_tdriaddr,
			(uintptr_t)g_TpipeBaseAddrInfo.MemVa,
			g_TpipeBaseAddrInfo.MemPa);
		CMDQ_ERR("MemSizeDiff:0x%x,offset:0x%x\n",
			g_TpipeBaseAddrInfo.MemSizeDiff, offset);
		CMDQ_ERR("g_pKWTpipeBuffer:0x%pK\n", g_pKWTpipeBuffer);

		if ((g_CmdqBaseAddrInfo.MemPa != 0) &&
			(g_CmdqBaseAddrInfo.MemVa != NULL) &&
			(g_pKWCmdqBuffer != NULL) &&
			(g_pKWVirDIPBuffer != NULL)) {
			offset = (g_cmdqaddr-g_CmdqBaseAddrInfo.MemPa);
			OffsetAddr = ((uintptr_t)g_CmdqBaseAddrInfo.MemVa)+
				(g_cmdqaddr-g_CmdqBaseAddrInfo.MemPa);
			if (copy_from_user(g_pKWCmdqBuffer,
				(void __user *)(OffsetAddr),
				MAX_DIP_CMDQ_BUFFER_SIZE) != 0) {
				CMDQ_ERR("cpy cmdq fail. cmdqaddr:0x%x\n",
					g_cmdqaddr);
				CMDQ_ERR("MemVa:0x%lx,MemPa:0x%x,offset:0x%x\n",
					(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
					g_CmdqBaseAddrInfo.MemPa,
					offset);
			}
			CMDQ_ERR("cmdqidx:0x%x, cmdqaddr:0x%x\n",
				cmdqidx,
				g_cmdqaddr);
			CMDQ_ERR("MemVa:0x%lx,MemPa:0x%x, offset:0x%x\n",
				(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
				g_CmdqBaseAddrInfo.MemPa,
				offset);
			offset = offset+g_CmdqBaseAddrInfo.MemSizeDiff;
			OffsetAddr = ((uintptr_t)g_CmdqBaseAddrInfo.MemVa) +
				offset;
			if (copy_from_user(g_pKWVirDIPBuffer,
				(void __user *)(OffsetAddr),
				DIP_REG_RANGE) != 0) {
				CMDQ_ERR("cpy vir dip fail.\n");
				CMDQ_ERR("cmdqaddr:0x%x,MVa:0x%lx,MPa:0x%x\n",
				g_cmdqaddr,
				(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
				g_CmdqBaseAddrInfo.MemPa);
				CMDQ_ERR("MSzDiff:0x%x,offset:0x%x\n",
				g_CmdqBaseAddrInfo.MemSizeDiff,
				offset);

			}
			CMDQ_ERR("cmdqaddr:0x%x,MVa:0x%lx,MPa:0x%x\n",
				g_cmdqaddr,
				(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
				g_CmdqBaseAddrInfo.MemPa);
			CMDQ_ERR("MSzDiff:0x%x\n",
				g_CmdqBaseAddrInfo.MemSizeDiff);
			CMDQ_ERR("ofset:0x%x,KWCmdBuf:0x%pK,KWTdrBuf:0x%pK\n",
				offset, g_pKWCmdqBuffer, g_pKWTpipeBuffer);
		} else {
			CMDQ_ERR("cmdqadd:0x%x,MVa:0x%lx,MPa:0x%x\n",
				g_cmdqaddr,
				(uintptr_t)g_CmdqBaseAddrInfo.MemVa,
				g_CmdqBaseAddrInfo.MemPa);
			CMDQ_ERR("MSzDiff:0x%x,KWCmdBuf:0x%pK,KWTdrBuf:0x%pK\n",
				g_CmdqBaseAddrInfo.MemSizeDiff,
				g_pKWCmdqBuffer,
				g_pKWTpipeBuffer);
		}
		g_bDumpPhyDIPBuf = MTRUE;
	}
#endif

	CMDQ_ERR("direct link: g_bDumpPhyDIPBuf:(0x%x), cmdqidx(0x%x)\n",
		g_bDumpPhyDIPBuf, cmdqidx);
	CMDQ_ERR("direct link: cmdqaddr(0x%x), tdriaddr(0x%x)\n",
		g_cmdqaddr, g_tdriaddr);

    /*top control*/
	CMDQ_ERR("dip: 15022000(0x%x)-15022004(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0000), DIP_RD32(DIP_A_BASE + 0x0004));
	CMDQ_ERR("dip: 15022008(0x%x)-1502200C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0008), DIP_RD32(DIP_A_BASE + 0x000C));
	CMDQ_ERR("dip: 15022010(0x%x)-15022014(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0010), DIP_RD32(DIP_A_BASE + 0x0014));
	CMDQ_ERR("dip: 15022018(0x%x)-1502201C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0018), DIP_RD32(DIP_A_BASE + 0x001C));
	CMDQ_ERR("dip: 150220E8(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x00E8));
    /*mdp crop1 and mdp crop2*/
	CMDQ_ERR("dip: 0x15027500(0x%x)-0x15027504(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x5500), DIP_RD32(DIP_A_BASE + 0x5504));
	CMDQ_ERR("crop2: 0x15024B80(0x%x)-0x15024B84(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x2B80), DIP_RD32(DIP_A_BASE + 0x2B84));
    /*imgi and tdri offset address*/
	CMDQ_ERR("dip: 15022508(0x%x)-15022304(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0508), DIP_RD32(DIP_A_BASE + 0x0304));
	CMDQ_ERR("dip: 15022308(0x%x)-1502230C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0308), DIP_RD32(DIP_A_BASE + 0x030C));
    /*tdr ctrl*/
	CMDQ_ERR("dip: 15022050(0x%x)-15022054(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0050), DIP_RD32(DIP_A_BASE + 0x0054));
	CMDQ_ERR("dip: 15022058(0x%x)-1502205C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0058), DIP_RD32(DIP_A_BASE + 0x005C));
	CMDQ_ERR("dip: 15022060(0x%x)-15022064(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0060), DIP_RD32(DIP_A_BASE + 0x0064));
	CMDQ_ERR("dip: 15022068(0x%x)-150220e0(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0068), DIP_RD32(DIP_A_BASE + 0x00E0));
	CMDQ_ERR("dip: 150220E4(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x00E4));

    /*Request and Ready Signal*/
	CMDQ_ERR("dip: 150220B8(0x%x)-150220BC(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x00B8), DIP_RD32(DIP_A_BASE + 0x00BC));
	CMDQ_ERR("dip: 150220C0(0x%x)-150220C4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x00C0), DIP_RD32(DIP_A_BASE + 0x00C4));
	CMDQ_ERR("dip: 150220C8(0x%x)-150220CC(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x00C8), DIP_RD32(DIP_A_BASE + 0x00CC));
	CMDQ_ERR("dip: 150220D0(0x%x)-150220D4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x00D0), DIP_RD32(DIP_A_BASE + 0x00D4));
	CMDQ_ERR("dip: 150220D8(0x%x)-150220DC(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x00D8), DIP_RD32(DIP_A_BASE + 0x00DC));
	CMDQ_ERR("dip: 150220F4(0x%x)-150220F8(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x00F4), DIP_RD32(DIP_A_BASE + 0x00F8));


	CMDQ_ERR("dcm: 15022090(0x%x)-15022094(0x%x)-15022098(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0090), DIP_RD32(DIP_A_BASE + 0x0094),
		DIP_RD32(DIP_A_BASE + 0x0098));
	CMDQ_ERR("dcm: 1502209C(0x%x)-150220A0(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x009C), DIP_RD32(DIP_A_BASE + 0x00A0));
	CMDQ_ERR("dcm: 0x150220A4(0x%x)-0x150220A8(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x00A4), DIP_RD32(DIP_A_BASE + 0x00A8));
	CMDQ_ERR("dcm: 0x150220AC(0x%x)-0x150220B0(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x00AC), DIP_RD32(DIP_A_BASE + 0x00B0));
	CMDQ_ERR("dcm: 0x150220B4(0x%x)-0x150220F0(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x00B4), DIP_RD32(DIP_A_BASE + 0x00F0));


	/*CQ_THR info*/
	CMDQ_ERR("dip: 15022204(0x%x)-15022208(0x%x)-1502220C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0204), DIP_RD32(DIP_A_BASE + 0x0208),
		DIP_RD32(DIP_A_BASE + 0x020C));
	CMDQ_ERR("dip: 15022210(0x%x)-15022214(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0210), DIP_RD32(DIP_A_BASE + 0x0214));
	CMDQ_ERR("dip: 15022218(0x%x)-0x1502231C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0218), DIP_RD32(DIP_A_BASE + 0x031C));

	CMDQ_ERR("nbc2: 15026800(0x%x)-15026804(0x%x)-15026808(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x4800), DIP_RD32(DIP_A_BASE + 0x4804),
		DIP_RD32(DIP_A_BASE + 0x4808));
	CMDQ_ERR("nbc2: 15026810(0x%x)-15026814(0x%x)-15026818(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x4810), DIP_RD32(DIP_A_BASE + 0x4814),
		DIP_RD32(DIP_A_BASE + 0x4818));
	CMDQ_ERR("nbc2: 15026830(0x%x)-15026834(0x%x)-15026838(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x4830), DIP_RD32(DIP_A_BASE + 0x4834),
		DIP_RD32(DIP_A_BASE + 0x4838));
	CMDQ_ERR("nbc2: 15026840(0x%x)-15026844(0x%x)-15026848(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x4840), DIP_RD32(DIP_A_BASE + 0x4844),
		DIP_RD32(DIP_A_BASE + 0x4848));
	CMDQ_ERR("nbc2: 1502684C(0x%x)-15026850(0x%x)-15026854(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x484C), DIP_RD32(DIP_A_BASE + 0x4850),
		DIP_RD32(DIP_A_BASE + 0x4854));
	CMDQ_ERR("nbc2: 15026858(0x%x)-1502685C(0x%x)-15026874(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x4858), DIP_RD32(DIP_A_BASE + 0x485C),
		DIP_RD32(DIP_A_BASE + 0x4874));
	/* 0080, 0x15022080, DIP_A_CTL_DBG_SET */
	DIP_WR32(DIP_A_BASE + 0x80, 0x0);
	CMDQ_ERR("0x0007BC : dip: 0x150227BC(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x7BC));
	/* 07BC, 0x150227BC, DIP_A_DMA_DEBUG_SEL */
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x000013);
	/* 0084, 0x15022084, DIP_A_CTL_DBG_PORT */
	CMDQ_ERR("0x000013 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x000113);
	CMDQ_ERR("0x000113 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x000213);
	CMDQ_ERR("0x000213 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x000313);
	CMDQ_ERR("0x000313 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	/* IMG2O */
	/* 07BC, 0x150227BC, DIP_A_DMA_DEBUG_SEL */
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x001013);
	/* 0084, 0x15022084, DIP_A_CTL_DBG_PORT */
	CMDQ_ERR("0x001013 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x001113);
	CMDQ_ERR("0x001113 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x001213);
	CMDQ_ERR("0x001213 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x001313);
	CMDQ_ERR("0x001313 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	/*IMG3O */
	/* 07BC, 0x150227BC, DIP_A_DMA_DEBUG_SEL */
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x001813);
	/* 0084, 0x15022084, DIP_A_CTL_DBG_PORT */
	CMDQ_ERR("0x001813 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x001913);
	CMDQ_ERR("0x001913 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x001A13);
	CMDQ_ERR("0x001A13 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x001B13);
	CMDQ_ERR("0x001B13 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	/* UFDI */
	/* 07BC, 0x150227BC, DIP_A_DMA_DEBUG_SEL */
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x01020A);
	/* 0084, 0x15022084, DIP_A_CTL_DBG_PORT */
	CMDQ_ERR("0x01020A : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));

	/* 07BC, 0x150227BC, DIP_A_DMA_DEBUG_SEL */
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x000301);
	/* 0084, 0x15022084, DIP_A_CTL_DBG_PORT */
	CMDQ_ERR("0x000301 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));

	/* 07BC, 0x150227BC, DIP_A_DMA_DEBUG_SEL */
	DIP_WR32(DIP_A_BASE + 0x7BC, 0x000101);
	/* 0084, 0x15022084, DIP_A_CTL_DBG_PORT */
	CMDQ_ERR("0x000101 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));

	DIP_WR32(DIP_A_BASE + 0x80, 0x003016);
	CMDQ_ERR("0x003016 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x80, 0x003017);
	CMDQ_ERR("0x003017 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x80, 0x003018);
	CMDQ_ERR("0x003018 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x80, 0x003019);
	CMDQ_ERR("0x003019 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x80, 0x005100);
	CMDQ_ERR("0x005100 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));

		DIP_WR32(DIP_A_BASE + 0x80, 0x030514);
	CMDQ_ERR("0x030514 : dip: 0x15022084(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x084));

	/* DMA Error */
	CMDQ_ERR("img2o  0x15022744(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x744));
	CMDQ_ERR("img2bo 0x15022748(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x748));
	CMDQ_ERR("img3o  0x1502274C(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x74C));
	CMDQ_ERR("img3bo 0x15022750(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x750));
	CMDQ_ERR("img3Co 0x15022754(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x754));
	CMDQ_ERR("feo    0x15022758(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x758));
	CMDQ_ERR("mfbo/pak2o 0x1502275C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x75C));
	CMDQ_ERR("imgi   0x15022760(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x760));
	CMDQ_ERR("imgbi  0x15022764(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x764));
	CMDQ_ERR("imgci  0x15022768(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x768));
	CMDQ_ERR("vipi   0x1502276c(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x76c));
	CMDQ_ERR("vip2i  0x15022770(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x770));
	CMDQ_ERR("vip3i  0x15022774(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x774));
	CMDQ_ERR("dmgi   0x15022778(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x778));
	CMDQ_ERR("depi   0x1502277c(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x77C));
	CMDQ_ERR("lcei   0x15022780(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x780));
	CMDQ_ERR("ufdi   0x15022784(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x784));
	CMDQ_ERR("smx1o  0x15022788(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x788));
	CMDQ_ERR("smx2o  0x1502278C(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x78C));
	CMDQ_ERR("smx3o  0x15022790(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x790));
	CMDQ_ERR("smx4o  0x15022794(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x794));
	CMDQ_ERR("smx1i  0x15022798(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x798));
	CMDQ_ERR("smx2i  0x1502279C(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x79C));
	CMDQ_ERR("smx3i  0x150227A0(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x7A0));
	CMDQ_ERR("smx4i  0x150227A4(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x7A4));

	/* Interrupt Status */
	CMDQ_ERR("CTL_INT_STATUSX      0x15022040(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x040));
	CMDQ_ERR("CTL_CQ_INT_STATUSX   0x15022044(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x044));
	CMDQ_ERR("CTL_CQ_INT2_STATUSX  0x15022048(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x048));
	CMDQ_ERR("CTL_CQ_INT3_STATUSX  0x1502204C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x04C));

	CMDQ_ERR("hfg crop: 0x150272B0(0x%x)-0x150272B4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x52B0), DIP_RD32(DIP_A_BASE + 0x52B4));

	/* IMG3O DMA*/
	CMDQ_ERR("img3o: 0x15022390(0x%x)-0x15022398(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x390), DIP_RD32(DIP_A_BASE + 0x0398));
	CMDQ_ERR("img3o: 0x150223A0(0x%x)-0x150223A4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x3A0), DIP_RD32(DIP_A_BASE + 0x03A4));
	CMDQ_ERR("img3o: 0x150223A8(0x%x)-0x150223AC(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x03A8), DIP_RD32(DIP_A_BASE + 0x03AC));
	CMDQ_ERR("img3o: 0x150223B0(0x%x)-0x150223B4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x03B0), DIP_RD32(DIP_A_BASE + 0x03B4));
	CMDQ_ERR("img3o: 0x150223B8(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x03B8));
	CMDQ_ERR("img3bo: 0x150223C0(0x%x)-0x150223C8(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x3C0), DIP_RD32(DIP_A_BASE + 0x03C8));
	CMDQ_ERR("img3bo: 0x150223D0(0x%x)-0x150223D4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x3D0), DIP_RD32(DIP_A_BASE + 0x03D4));
	CMDQ_ERR("img3bo: 0x150223D8(0x%x)-0x150223DC(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x03D8), DIP_RD32(DIP_A_BASE + 0x03DC));
	CMDQ_ERR("img3bo: 0x150223E0(0x%x)-0x150223E4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x03E0), DIP_RD32(DIP_A_BASE + 0x03E4));
	CMDQ_ERR("img3bo: 0x150223E8(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x03E8));

	CMDQ_ERR("img3co: 0x150223F0(0x%x)-0x150223F8(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x3F0), DIP_RD32(DIP_A_BASE + 0x03F8));
	CMDQ_ERR("img3co: 0x15022400(0x%x)-0x15022404(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x400), DIP_RD32(DIP_A_BASE + 0x0404));
	CMDQ_ERR("img3co: 0x15022408(0x%x)-0x1502240C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0408), DIP_RD32(DIP_A_BASE + 0x040C));
	CMDQ_ERR("img3co: 0x15022410(0x%x)-0x15022414(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0410), DIP_RD32(DIP_A_BASE + 0x0414));
	CMDQ_ERR("img3co: 0x15022418(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x0418));

	/* IMGI DMA*/
	CMDQ_ERR("imgi: 0x15022500(0x%x)-0x15022508(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x500), DIP_RD32(DIP_A_BASE + 0x508));
	CMDQ_ERR("imgi: 0x15022510(0x%x)-0x15022514(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x510), DIP_RD32(DIP_A_BASE + 0x514));
	CMDQ_ERR("imgi: 0x15022518(0x%x)-0x1502251C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x518), DIP_RD32(DIP_A_BASE + 0x51C));
	CMDQ_ERR("imgi: 0x15022520(0x%x)-0x15022524(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x520), DIP_RD32(DIP_A_BASE + 0x524));

	/* PAK2O DMA*/
	CMDQ_ERR("pak2o: 0x15022450(0x%x)-0x15022458(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x450), DIP_RD32(DIP_A_BASE + 0x458));
	CMDQ_ERR("pak2o: 0x15022460(0x%x)-0x15022464(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x460), DIP_RD32(DIP_A_BASE + 0x464));
	CMDQ_ERR("pak2o: 0x15022468(0x%x)-0x1502246C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x468), DIP_RD32(DIP_A_BASE + 0x46C));
	CMDQ_ERR("pak2o: 0x15022470(0x%x)-0x15022474(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x470), DIP_RD32(DIP_A_BASE + 0x474));
	CMDQ_ERR("pak2o: 0x15022478(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x478));

	/* IMG2O DMA*/
	CMDQ_ERR("img2o: 0x15022330(0x%x)-0x15022338(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x330), DIP_RD32(DIP_A_BASE + 0x338));
	CMDQ_ERR("img2o: 0x15022340(0x%x)-0x15022344(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x340), DIP_RD32(DIP_A_BASE + 0x344));
	CMDQ_ERR("img2o: 0x15022348(0x%x)-0x1502234C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0348), DIP_RD32(DIP_A_BASE + 0x034C));
	CMDQ_ERR("img2o: 0x15022350(0x%x)-0x15022354(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0350), DIP_RD32(DIP_A_BASE + 0x0354));
	CMDQ_ERR("img2o: 0x15022358(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x0358));

	/* VIPI DMA*/
	CMDQ_ERR("vipi: 0x15022590(0x%x)-0x15022598(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x590), DIP_RD32(DIP_A_BASE + 0x0598));
	CMDQ_ERR("vipi: 0x150225A0(0x%x)-0x150225A4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x5A0), DIP_RD32(DIP_A_BASE + 0x05A4));
	CMDQ_ERR("vipi: 0x150225A8(0x%x)-0x150225AC(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x05A8), DIP_RD32(DIP_A_BASE + 0x05AC));
	CMDQ_ERR("vipi: 0x150225B0(0x%x)-0x150225B4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x05B0), DIP_RD32(DIP_A_BASE + 0x05B4));

	   /* VIP2I DMA*/
	CMDQ_ERR("vip2i: 0x150225C0(0x%x)-0x150225C8(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x5C0), DIP_RD32(DIP_A_BASE + 0x05C8));
	CMDQ_ERR("vip2i: 0x150225D0(0x%x)-0x150225D4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x5D0), DIP_RD32(DIP_A_BASE + 0x05D4));
	CMDQ_ERR("vip2i: 0x150225D8(0x%x)-0x150225DC(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x05D8), DIP_RD32(DIP_A_BASE + 0x05DC));
	CMDQ_ERR("vip2i: 0x150225E0(0x%x)-0x150225E4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x05E0), DIP_RD32(DIP_A_BASE + 0x05E4));

	   /* VIP3I DMA*/
	CMDQ_ERR("vip3i: 0x150225F0(0x%x)-0x150225F8(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x5F0), DIP_RD32(DIP_A_BASE + 0x05F8));
	CMDQ_ERR("vip3i: 0x15022600(0x%x)-0x15022604(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x600), DIP_RD32(DIP_A_BASE + 0x0604));
	CMDQ_ERR("vip3i: 0x15022608(0x%x)-0x1502260C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0608), DIP_RD32(DIP_A_BASE + 0x060C));
	CMDQ_ERR("vip3i: 0x15022610(0x%x)-0x15022614(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0610), DIP_RD32(DIP_A_BASE + 0x0614));

	   /* DMGI DMA*/
	CMDQ_ERR("dmgi: 0x15022620(0x%x)-0x15022628(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x620), DIP_RD32(DIP_A_BASE + 0x0628));
	CMDQ_ERR("dmgi: 0x15022630(0x%x)-0x15022634(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x630), DIP_RD32(DIP_A_BASE + 0x0634));
	CMDQ_ERR("dmgi: 0x15022638(0x%x)-0x1502263C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0638), DIP_RD32(DIP_A_BASE + 0x063C));
	CMDQ_ERR("dmgi: 0x15022640(0x%x)-0x15022644(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0640), DIP_RD32(DIP_A_BASE + 0x0644));

	   /* DEPI DMA*/
	CMDQ_ERR("depi: 0x15022650(0x%x)-0x15022658(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x650), DIP_RD32(DIP_A_BASE + 0x0658));
	CMDQ_ERR("depi: 0x15022660(0x%x)-0x15022664(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x660), DIP_RD32(DIP_A_BASE + 0x0664));
	CMDQ_ERR("depi: 0x15022668(0x%x)-0x1502266C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0668), DIP_RD32(DIP_A_BASE + 0x066C));
	CMDQ_ERR("depi: 0x15022670(0x%x)-0x15022674(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0670), DIP_RD32(DIP_A_BASE + 0x0674));

	/* LCEI DMA*/
	CMDQ_ERR("lcei: 0x15022680(0x%x)-0x15022688(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x680), DIP_RD32(DIP_A_BASE + 0x688));
	CMDQ_ERR("lcei: 0x15022690(0x%x)-0x15022694(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x690), DIP_RD32(DIP_A_BASE + 0x694));
	CMDQ_ERR("lcei: 0x15022698(0x%x)-0x1502269C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0698), DIP_RD32(DIP_A_BASE + 0x069C));
	CMDQ_ERR("lcei: 0x150226A0(0x%x)-0x150226A4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x06A0), DIP_RD32(DIP_A_BASE + 0x06A4));

	/* CRZ */
	CMDQ_ERR("crz: 0x15027300(0x%x)-0x15027304(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x5300), DIP_RD32(DIP_A_BASE + 0x5304));
	CMDQ_ERR("crz: 0x15027308(0x%x)-0x1502730C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x5308), DIP_RD32(DIP_A_BASE + 0x530C));
	CMDQ_ERR("crz: 0x15027310(0x%x)-0x15027314(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x5310), DIP_RD32(DIP_A_BASE + 0x5314));
	CMDQ_ERR("crz: 0x15027318(0x%x)-0x1502731C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x5318), DIP_RD32(DIP_A_BASE + 0x531C));
	CMDQ_ERR("crz: 0x15027320(0x%x)-0x15027324(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x5320), DIP_RD32(DIP_A_BASE + 0x5324));
	CMDQ_ERR("crz: 0x15027328(0x%x)-0x1502732C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x5328), DIP_RD32(DIP_A_BASE + 0x532C));
	CMDQ_ERR("crz: 0x15027330(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x5330));

	CMDQ_ERR("imgsys:0x15020000(0x%x)\n", DIP_RD32(DIP_IMGSYS_CONFIG_BASE));
	CMDQ_ERR("0x15020180(0x%x)-0x15020184(0x%x)\n",
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x180),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x184));
	CMDQ_ERR("0x15020188(0x%x)-0x1502018C(0x%x)\n",
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x188),
		DIP_RD32(DIP_IMGSYS_CONFIG_BASE + 0x18C));
	/* NR3D */
	CMDQ_ERR("tnr and color: 0x15027380(0x%x)-0x15027398(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x5380), DIP_RD32(DIP_A_BASE + 0x5398));
	CMDQ_ERR("tnr and color: 0x15027110(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x5110));

   /* IMGBI */
	CMDQ_ERR("imgbi: 0x15022530(0x%x)-0x15022538(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0530), DIP_RD32(DIP_A_BASE + 0x0538));
	CMDQ_ERR("imgbi: 0x15022540(0x%x)-0x15022544(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0540), DIP_RD32(DIP_A_BASE + 0x0544));
	CMDQ_ERR("imgbi: 0x15022548(0x%x)-0x1502254C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0548), DIP_RD32(DIP_A_BASE + 0x054C));
	CMDQ_ERR("imgbi: 0x15022550(0x%x)-0x15022554(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0550), DIP_RD32(DIP_A_BASE + 0x0554));

    /* IMGCI */
	CMDQ_ERR("imgci: 0x15022560(0x%x)-0x15022568(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0560), DIP_RD32(DIP_A_BASE + 0x0568));
	CMDQ_ERR("imgci: 0x15022570(0x%x)-0x15022574(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0570), DIP_RD32(DIP_A_BASE + 0x0574));
	CMDQ_ERR("imgci: 0x15022578(0x%x)-0x1502257C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0578), DIP_RD32(DIP_A_BASE + 0x057C));
	CMDQ_ERR("imgci: 0x15022580(0x%x)-0x15022584(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x0580), DIP_RD32(DIP_A_BASE + 0x0584));

    /* LCE */
	CMDQ_ERR("lce: 0x15024100(0x%x)-0x15024104(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x2100), DIP_RD32(DIP_A_BASE + 0x2104));
	CMDQ_ERR("lce: 0x15024108(0x%x)-0x1502410C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x2108), DIP_RD32(DIP_A_BASE + 0x210C));
	CMDQ_ERR("0x15024110(0x%x)-0x15024114(0x%x)-0x15024120(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x2110), DIP_RD32(DIP_A_BASE + 0x2114),
		DIP_RD32(DIP_A_BASE + 0x2120));
    /* NBC */
	CMDQ_ERR("nbc: 0x15025700(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x3700));
    /* BNR */
	CMDQ_ERR("bnr: 0x15023240(0x%x)-0x15023280(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x1240), DIP_RD32(DIP_A_BASE + 0x1280));


	/* SMX1O DMA*/
	CMDQ_ERR("smx1 ctrl: 0x15023100(0x%x)-0x15023104(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x1100), DIP_RD32(DIP_A_BASE + 0x1104));
	CMDQ_ERR("smx1 ctrl: 0x1502310C(0x%x)-0x15023110(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x110C), DIP_RD32(DIP_A_BASE + 0x1110));
	CMDQ_ERR("smx1 ctrl: 0x15023114(0x%x)-0x15023118(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x1114), DIP_RD32(DIP_A_BASE + 0x1118));
	CMDQ_ERR("smx1 ctrl: 0x1502311C(0x%x)-0x15023120(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x111C), DIP_RD32(DIP_A_BASE + 0x1120));
	CMDQ_ERR("smx1o: 0x150227D0(0x%x)-0x150227D8(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x7D0), DIP_RD32(DIP_A_BASE + 0x7D8));
	CMDQ_ERR("smx1o: 0x150227E0(0x%x)-0x150227E4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x7E0), DIP_RD32(DIP_A_BASE + 0x7E4));
	CMDQ_ERR("smx1o: 0x150227E8(0x%x)-0x150227EC(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x7E8), DIP_RD32(DIP_A_BASE + 0x7EC));
	CMDQ_ERR("smx1o: 0x150227F0(0x%x)-0x150227F4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x7F0), DIP_RD32(DIP_A_BASE + 0x7F4));
	CMDQ_ERR("smx1o: 0x150227F8(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x7F8));

	/* SMX2O DMA*/
	CMDQ_ERR("smx2 ctrl: 0x150267C0(0x%x)-0x150267C4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x47C0), DIP_RD32(DIP_A_BASE + 0x47C4));
	CMDQ_ERR("smx2 ctrl: 0x150267CC(0x%x)-0x150267D0(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x47CC), DIP_RD32(DIP_A_BASE + 0x47D0));
	CMDQ_ERR("smx2 ctrl: 0x150267D4(0x%x)-0x150267D8(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x47D4), DIP_RD32(DIP_A_BASE + 0x47D8));
	CMDQ_ERR("smx2 ctrl: 0x150267DC(0x%x)-0x150267E0(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x47DC), DIP_RD32(DIP_A_BASE + 0x47E0));
	CMDQ_ERR("smx2o: 0x15022800(0x%x)-0x15022808(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x800), DIP_RD32(DIP_A_BASE + 0x808));
	CMDQ_ERR("smx2o: 0x15022810(0x%x)-0x15022814(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x810), DIP_RD32(DIP_A_BASE + 0x814));
	CMDQ_ERR("smx2o: 0x15022818(0x%x)-0x1502281C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x818), DIP_RD32(DIP_A_BASE + 0x81C));
	CMDQ_ERR("smx2o: 0x15022820(0x%x)-0x15022824(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x820), DIP_RD32(DIP_A_BASE + 0x824));
	CMDQ_ERR("smx2o: 0x15022828(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x828));

	/* SMX3O DMA*/
	CMDQ_ERR("smx3 ctrl: 0x150269C0(0x%x)-0x150269C4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x49C0), DIP_RD32(DIP_A_BASE + 0x49C4));
	CMDQ_ERR("smx3 ctrl: 0x150269CC(0x%x)-0x150269D0(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x49CC), DIP_RD32(DIP_A_BASE + 0x49D0));
	CMDQ_ERR("smx3 ctrl: 0x150269D4(0x%x)-0x150269D8(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x49D4), DIP_RD32(DIP_A_BASE + 0x49D8));
	CMDQ_ERR("smx3 ctrl: 0x150269DC(0x%x)-0x150269E0(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x49DC), DIP_RD32(DIP_A_BASE + 0x49E0));
	CMDQ_ERR("smx3o: 0x15022830(0x%x)-0x15022838(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x830), DIP_RD32(DIP_A_BASE + 0x838));
	CMDQ_ERR("smx3o: 0x15022840(0x%x)-0x15022844(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x840), DIP_RD32(DIP_A_BASE + 0x844));
	CMDQ_ERR("smx3o: 0x15022848(0x%x)-0x1502284C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x848), DIP_RD32(DIP_A_BASE + 0x84C));
	CMDQ_ERR("smx3o: 0x15022850(0x%x)-0x15022854(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x850), DIP_RD32(DIP_A_BASE + 0x854));
	CMDQ_ERR("smx3o: 0x15022858(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x858));

	/* SMX4O DMA*/
	CMDQ_ERR("smx4 ctrl: 0x15024BC0(0x%x)-0x15024BC4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x2BC0), DIP_RD32(DIP_A_BASE + 0x2BC4));
	CMDQ_ERR("smx4 ctrl: 0x15024BCC(0x%x)-0x15024BD0(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x2BCC), DIP_RD32(DIP_A_BASE + 0x2BD0));
	CMDQ_ERR("smx4 ctrl: 0x15024BD4(0x%x)-0x15024BD8(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x2BD4), DIP_RD32(DIP_A_BASE + 0x2BD8));
	CMDQ_ERR("smx4 ctrl: 0x15024BDC(0x%x)-0x15024BE0(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x2BDC), DIP_RD32(DIP_A_BASE + 0x2BE0));
	CMDQ_ERR("smx4o: 0x15022860(0x%x)-0x15022868(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x860), DIP_RD32(DIP_A_BASE + 0x868));
	CMDQ_ERR("smx4o: 0x15022870(0x%x)-0x15022874(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x870), DIP_RD32(DIP_A_BASE + 0x874));
	CMDQ_ERR("smx4o: 0x15022878(0x%x)-0x1502287C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x878), DIP_RD32(DIP_A_BASE + 0x87C));
	CMDQ_ERR("smx4o: 0x15022880(0x%x)-0x15022884(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x880), DIP_RD32(DIP_A_BASE + 0x884));
	CMDQ_ERR("smx4o: 0x15022888(0x%x)\n", DIP_RD32(DIP_A_BASE + 0x888));

	/* SMX1I DMA*/
	CMDQ_ERR("smx1i: 0x15022890(0x%x)-0x15022898(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x890), DIP_RD32(DIP_A_BASE + 0x898));
	CMDQ_ERR("smx1i: 0x150228A0(0x%x)-0x150228A4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x8A0), DIP_RD32(DIP_A_BASE + 0x8A4));
	CMDQ_ERR("smx1i: 0x150228A8(0x%x)-0x150228AC(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x8A8), DIP_RD32(DIP_A_BASE + 0x8AC));
	CMDQ_ERR("smx1i: 0x150228B0(0x%x)-0x150228B4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x8B0), DIP_RD32(DIP_A_BASE + 0x8B4));
	/* SMX2I DMA*/
	CMDQ_ERR("smx2i: 0x150228C0(0x%x)-0x150228C8(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x8C0), DIP_RD32(DIP_A_BASE + 0x8C8));
	CMDQ_ERR("smx2i: 0x150228D0(0x%x)-0x150228D4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x8D0), DIP_RD32(DIP_A_BASE + 0x8D4));
	CMDQ_ERR("smx2i: 0x150228D8(0x%x)-0x150228DC(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x8D8), DIP_RD32(DIP_A_BASE + 0x8DC));
	CMDQ_ERR("smx2i: 0x150228E0(0x%x)-0x150228E4(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x8E0), DIP_RD32(DIP_A_BASE + 0x8E4));
	/* SMX3I DMA*/
	CMDQ_ERR("smx3i: 0x150228F0(0x%x)-0x150228F8(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x8F0), DIP_RD32(DIP_A_BASE + 0x8F8));
	CMDQ_ERR("smx3i: 0x15022900(0x%x)-0x15022904(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x900), DIP_RD32(DIP_A_BASE + 0x904));
	CMDQ_ERR("smx3i: 0x15022908(0x%x)-0x1502290C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x908), DIP_RD32(DIP_A_BASE + 0x90C));
	CMDQ_ERR("smx3i: 0x15022910(0x%x)-0x15022914(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x910), DIP_RD32(DIP_A_BASE + 0x914));
	/* SMX4I DMA*/
	CMDQ_ERR("smx4i: 0x15022920(0x%x)-0x15022928(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x920), DIP_RD32(DIP_A_BASE + 0x928));
	CMDQ_ERR("smx4i: 0x15022930(0x%x)-0x15022934(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x930), DIP_RD32(DIP_A_BASE + 0x934));
	CMDQ_ERR("smx4i: 0x15022938(0x%x)-0x1502293C(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x938), DIP_RD32(DIP_A_BASE + 0x93C));
	CMDQ_ERR("smx4i: 0x15022940(0x%x)-0x15022944(0x%x)\n",
		DIP_RD32(DIP_A_BASE + 0x940), DIP_RD32(DIP_A_BASE + 0x944));

	dipdmacmd = 0x000109;
	DIP_WR32(DIP_A_BASE + 0x7BC, dipdmacmd);
	CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
		dipdmacmd, DIP_RD32(DIP_A_BASE + 0x084));
	dipdmacmd = 0x010209;
	DIP_WR32(DIP_A_BASE + 0x7BC, dipdmacmd);
	CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
		dipdmacmd, DIP_RD32(DIP_A_BASE + 0x084));
	dipdmacmd = 0x030209;
	DIP_WR32(DIP_A_BASE + 0x7BC, dipdmacmd);
	CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
		dipdmacmd, DIP_RD32(DIP_A_BASE + 0x084));


	dipdmacmd = 0x00000013;
	for (i = 0; i < 64 ; i++) {
		/* 0x150210A8, DIPDMATOP_REG_D1A_DIPDMATOP_DMA_DEBUG_SEL */
		switch (i) {
		case 0x0:
			dmaidx = 1;
			CMDQ_ERR("imgi dma debug\n");
			break;
		case 0x4:
			dmaidx = 2;
			CMDQ_ERR("imgbi dma debug\n");
			break;
		case 0x8:
			dmaidx = 4;
			CMDQ_ERR("vipi dma debug\n");
			break;
		case 0xc:
			dmaidx = 5;
			CMDQ_ERR("vip2i dma debug\n");
			break;
		case 0x10:
			dmaidx = 11;
			CMDQ_ERR("img2o dma debug\n");
			break;
		case 0x14:
			dmaidx = 12;
			CMDQ_ERR("img2bo dma debug\n");
			break;
		case 0x18:
			dmaidx = 13;
			CMDQ_ERR("img3o dma debug\n");
			break;
		case 0x1c:
			dmaidx = 14;
			CMDQ_ERR("img3bo dma debug\n");
			break;
		case 0x20:
			dmaidx = 20;
			CMDQ_ERR("smx1i dma debug\n");
			break;
		case 0x24:
			dmaidx = 21;
			CMDQ_ERR("smx2i dma debug\n");
			break;
		case 0x28:
			dmaidx = 22;
			CMDQ_ERR("smx3i dma debug\n");
			break;
		case 0x2c:
			dmaidx = 23;
			CMDQ_ERR("smx4i dma debug\n");
			break;
		case 0x30:
			dmaidx = 24;
			CMDQ_ERR("smx1o dma debug\n");
			break;
		case 0x34:
			dmaidx = 25;
			CMDQ_ERR("smx2o dma debug\n");
			break;
		case 0x38:
			dmaidx = 26;
			CMDQ_ERR("smx3o dma debug\n");
			break;
		case 0x3c:
			dmaidx = 27;
			CMDQ_ERR("smx4o dma debug\n");
			break;
		default:
			break;
		}
		dipdmacmd = dipdmacmd & 0xFFFF00FF;
		dipdmacmd = dipdmacmd | (i << 8);
		DIP_WR32(DIP_A_BASE + 0x7BC, dipdmacmd);
		/* 0x15022194, DIPCTL_REG_D1A_DIPCTL_DBG_OUT */
		CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
			dipdmacmd, DIP_RD32(DIP_A_BASE + 0x084));
		i++;
		dipdmacmd = dipdmacmd & 0xFFFF00FF;
		dipdmacmd = dipdmacmd | (i << 8);
		DIP_WR32(DIP_A_BASE + 0x7BC, dipdmacmd);
		CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
			dipdmacmd, DIP_RD32(DIP_A_BASE + 0x084));
		i++;
		dipdmacmd = dipdmacmd & 0xFFFF00FF;
		dipdmacmd = dipdmacmd | (i << 8);
		DIP_WR32(DIP_A_BASE + 0x7BC, dipdmacmd);
		CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
			dipdmacmd, DIP_RD32(DIP_A_BASE + 0x084));
		i++;
		dipdmacmd = dipdmacmd & 0xFFFF00FF;
		dipdmacmd = dipdmacmd | (i << 8);
		DIP_WR32(DIP_A_BASE + 0x7BC, dipdmacmd);
		CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
			dipdmacmd, DIP_RD32(DIP_A_BASE + 0x084));

		if (((dmaidx >= 11) && (dmaidx <= 14)) ||
			((dmaidx >= 24) && (dmaidx <= 27))) {
			smidmacmd = 0x00080400;
			smidmacmd = smidmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, smidmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				smidmacmd, DIP_RD32(DIP_A_BASE + 0x084));

			smidmacmd = 0x00090400;
			smidmacmd = smidmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, smidmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				smidmacmd, DIP_RD32(DIP_A_BASE + 0x084));

			smidmacmd = 0x00000400;
			smidmacmd = smidmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, smidmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				smidmacmd, DIP_RD32(DIP_A_BASE + 0x084));

			smidmacmd = 0x00010400;
			smidmacmd = smidmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, smidmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				smidmacmd, DIP_RD32(DIP_A_BASE + 0x084));

			fifodmacmd = 0x00000300;
			fifodmacmd = fifodmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, DIP_RD32(DIP_A_BASE + 0x084));

			fifodmacmd = 0x00010300;
			fifodmacmd = fifodmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, DIP_RD32(DIP_A_BASE + 0x084));

			fifodmacmd = 0x00020300;
			fifodmacmd = fifodmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, DIP_RD32(DIP_A_BASE + 0x084));

			fifodmacmd = 0x00030300;
			fifodmacmd = fifodmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, DIP_RD32(DIP_A_BASE + 0x084));

		} else {
			smidmacmd = 0x00080100;
			smidmacmd = smidmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, smidmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				smidmacmd, DIP_RD32(DIP_A_BASE + 0x084));

			smidmacmd = 0x00000100;
			smidmacmd = smidmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, smidmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				smidmacmd, DIP_RD32(DIP_A_BASE + 0x084));

			fifodmacmd = 0x00000200;
			fifodmacmd = fifodmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, DIP_RD32(DIP_A_BASE + 0x084));

			fifodmacmd = 0x00010200;
			fifodmacmd = fifodmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, DIP_RD32(DIP_A_BASE + 0x084));

			fifodmacmd = 0x00020200;
			fifodmacmd = fifodmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, DIP_RD32(DIP_A_BASE + 0x084));

			fifodmacmd = 0x00030200;
			fifodmacmd = fifodmacmd | dmaidx;
			DIP_WR32(DIP_A_BASE + 0x7BC, fifodmacmd);
			CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
				fifodmacmd, DIP_RD32(DIP_A_BASE + 0x084));

		}
	}

	CMDQ_ERR("lcei dma debug\n");
	smidmacmd = 0x00080109;
	DIP_WR32(DIP_A_BASE + 0x7BC, smidmacmd);
	CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
	smidmacmd, DIP_RD32(DIP_A_BASE + 0x084));

	smidmacmd = 0x00000109;   /*lcei smi case 0*/
	DIP_WR32(DIP_A_BASE + 0x7BC, smidmacmd);
	CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
	smidmacmd, DIP_RD32(DIP_A_BASE + 0x084));

	fifodmacmd = 0x00010209;
	DIP_WR32(DIP_A_BASE + 0x7BC, fifodmacmd);
	CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
		fifodmacmd, DIP_RD32(DIP_A_BASE + 0x084));

	fifodmacmd = 0x00030209;
	DIP_WR32(DIP_A_BASE + 0x7BC, fifodmacmd);
	CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
		fifodmacmd, DIP_RD32(DIP_A_BASE + 0x084));

	dipdmacmd = 0x6000;
	DIP_WR32(DIP_A_BASE + 0x080, dipdmacmd);
	CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
		dipdmacmd, DIP_RD32(DIP_A_BASE + 0x084));
	dipdmacmd = 0x6100;
	DIP_WR32(DIP_A_BASE + 0x080, dipdmacmd);
	CMDQ_ERR("0x%x : dip: 0x15022084(0x%x)\n",
		dipdmacmd, DIP_RD32(DIP_A_BASE + 0x084));
	DIP_WR32(DIP_A_BASE + 0x80, 0x0);
	CMDQ_ERR("- X.");
#endif
	/*  */
	return Ret;
}

#ifndef CONFIG_MTK_CLKMGR /*CCF*/

static inline void Prepare_Enable_ccf_clock(void)
{
	int ret;
	/* enable through smi API */
#ifndef EP_CODE_MARK_SMI
	smi_bus_prepare_enable(SMI_LARB5, DIP_DEV_NAME);
#endif
	ret = clk_prepare_enable(dip_clk.DIP_IMG_LARB5);
	if (ret)
		LOG_ERR("cannot prepare and enable DIP_IMG_LARB5 clock\n");

	ret = clk_prepare_enable(dip_clk.DIP_IMG_DIP);
	if (ret)
		LOG_ERR("cannot prepare and enable DIP_IMG_DIP clock\n");

}

static inline void Disable_Unprepare_ccf_clock(void)
{
	clk_disable_unprepare(dip_clk.DIP_IMG_DIP);
	clk_disable_unprepare(dip_clk.DIP_IMG_LARB5);
#ifndef EP_CODE_MARK_SMI
	smi_bus_disable_unprepare(SMI_LARB5, DIP_DEV_NAME);
#endif
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

	if (En) {
#if defined(EP_NO_CLKMGR)
		spin_lock(&(IspInfo.SpinLockClock));
		/* LOG_DBG("Camera clock enbled. G_u4DipEnClkCnt: %d.", */
		/* G_u4DipEnClkCnt); */
		switch (G_u4DipEnClkCnt) {
		case 0:
			/* Enable clock by hardcode*/
			setReg = 0xFFFFFFFF;
			/*DIP_WR32(CAMSYS_REG_CG_CLR, setReg);*/
			/*DIP_WR32(IMGSYS_REG_CG_CLR, setReg);*/
			break;
		default:
			break;
		}
		G_u4DipEnClkCnt++;
		spin_unlock(&(IspInfo.SpinLockClock));
#else/*CCF*/
		/*LOG_INF("CCF:prepare_enable clk");*/
		spin_lock(&(IspInfo.SpinLockClock));
		G_u4DipEnClkCnt++;
		spin_unlock(&(IspInfo.SpinLockClock));
		Prepare_Enable_ccf_clock(); /* !!cannot be used in spinlock!! */
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
		spin_lock(&(IspInfo.SpinLockClock));
		G_u4DipEnClkCnt--;
		spin_unlock(&(IspInfo.SpinLockClock));
		/* !!cannot be used in spinlock!! */
		Disable_Unprepare_ccf_clock();
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

	LOG_DBG(" Reset module(%d), IMGSYS clk gate(0x%x)\n",
		module, DIP_RD32(IMGSYS_REG_CG_CON));

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
		LOG_INF("DIP ReadReg pRegIo->pData is NULL, Count:%d!!",
			pRegIo->Count);
		Ret = -EFAULT;
		goto EXIT;
	}
	pData = kmalloc((pRegIo->Count) * sizeof(struct DIP_REG_STRUCT),
		GFP_KERNEL);
	if (pData == NULL) {
		LOG_INF("ERROR: DIP ReadReg kmalloc failed, cnt:%d\n",
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
 *static signed int DIP_WriteRegToHw(
 *	struct DIP_REG_STRUCT *pReg,
 *	unsigned int         Count)
 *{
 *	signed int Ret = 0;
 *	unsigned int i;
 *	bool dbgWriteReg;
 *	unsigned int module;
 *	void __iomem *regBase;
 *
 *	spin_lock(&(IspInfo.SpinLockIsp));
 *	dbgWriteReg = IspInfo.DebugMask & DIP_DBG_WRITE_REG;
 *	spin_unlock(&(IspInfo.SpinLockIsp));
 *
 *	module = pReg->module;
 *
 *
 *	switch (module) {
 *	case DIP_DIP_A_IDX:
 *		regBase = DIP_A_BASE;
 *		break;
 *	default:
 *		LOG_ERR("Unsupported module(%x) !!!\n", module);
 *		return -EFAULT;
 *	}
 *
 *	if (dbgWriteReg)
 *		LOG_DBG("- E.\n");
 *
 *	for (i = 0; i < Count; i++) {
 *		if (dbgWriteReg)
 *			LOG_DBG("mod(%d),base(0x%lx),Addr(0x%lx),Val(0x%x)\n",
 *			module,
 *			(unsigned long)regBase,
 *			(unsigned long)(pReg[i].Addr),
 *			(unsigned int)(pReg[i].Val));
 *		if (((regBase + pReg[i].Addr) < (regBase + PAGE_SIZE))
 *			&& ((pReg[i].Addr & 0x3) == 0))
 *			DIP_WR32(regBase + pReg[i].Addr, pReg[i].Val);
 *		else
 *			LOG_ERR("wrong address(0x%lx)\n",
 *				(unsigned long)(regBase + pReg[i].Addr));
 *
 *	}
 *
 *	return Ret;
 *}
 */

/**************************************************************
 *
 **************************************************************/
/*
 *static signed int DIP_WriteReg(struct DIP_REG_IO_STRUCT *pRegIo)
 *{
 *	signed int Ret = 0;
 *	struct DIP_REG_STRUCT *pData = NULL;
 *
 *	if (IspInfo.DebugMask & DIP_DBG_WRITE_REG)
 *		LOG_DBG("Data(0x%p), Count(%d)\n",
 *			(pRegIo->pData),
 *			(pRegIo->Count));
 *
 *	if ((pRegIo->pData == NULL) ||
 *		(pRegIo->Count == 0) ||
 *		(pRegIo->Count > (DIP_REG_RANGE>>2))) {
 *		LOG_INF("ERROR: pRegIo->pData is NULL or Count:%d\n",
 *			pRegIo->Count);
 *		Ret = -EFAULT;
 *		goto EXIT;
 *	}
 *	pData = kmalloc((pRegIo->Count) *
 *		sizeof(struct DIP_REG_STRUCT),
 *		GFP_KERNEL);
 *	if (pData == NULL) {
 *		LOG_INF("ERROR:kmalloc failed,(process,pid,tgid)=(%s,%d,%d)\n",
 *		current->comm,
 *		current->pid,
 *		current->tgid);
 *	Ret = -ENOMEM;
 *	goto EXIT;
 *	}
 *	if (copy_from_user(pData,
 *		(void __user *)(pRegIo->pData),
 *		pRegIo->Count * sizeof(struct DIP_REG_STRUCT)) != 0) {
 *		LOG_INF("copy_from_user failed\n");
 *		Ret = -EFAULT;
 *		goto EXIT;
 *	}
 *
 *
 *	Ret = DIP_WriteRegToHw(
 *		      pData,
 *		      pRegIo->Count);
 *
 *EXIT:
 *	if (pData != NULL) {
 *		kfree(pData);
 *		pData = NULL;
 *	}
 *	return Ret;
 *}
 */
/**************************************************************
 *
 **************************************************************/
#ifdef AEE_DUMP_BY_USING_ION_MEMORY
static signed int dip_allocbuf(struct dip_imem_memory *pMemInfo)
{
	int ret = 0;
	struct ion_mm_data mm_data;
	struct ion_sys_data sys_data;
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

	mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
	mm_data.config_buffer_param.kernel_handle = handle;
	mm_data.config_buffer_param.module_id = 0;
	mm_data.config_buffer_param.security = 0;
	mm_data.config_buffer_param.coherent = 1;
	ret = ion_kernel_ioctl(dip_p2_ion_client,
		ION_CMD_MULTIMEDIA,
		(unsigned long)&mm_data);
	if (ret) {
		LOG_ERR("fail to config ion buffer, ret=%d\n", ret);
		ret = -ENOMEM;
		goto dip_allocbuf_exit;
	}

	sys_data.sys_cmd = ION_SYS_GET_PHYS;
	sys_data.get_phys_param.kernel_handle = handle;
	ret = ion_kernel_ioctl(dip_p2_ion_client,
		ION_CMD_SYSTEM,
		(unsigned long)&sys_data);
	pMemInfo->pa = sys_data.get_phys_param.phy_addr;

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
			LOG_ERR("TpipeBuf failed,g_bIonBufAllocated:%d\n",
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
	(enum DIP_P2_BUFQUE_PROPERTY property,
	enum DIP_P2_BUFQUE_LIST_TAG listTag)
{
	signed int ret = 0;
	unsigned int tmpIdx = 0;
	signed int cnt = 0;
	bool stop = false;
	int i = 0;
	enum DIP_P2_BUF_STATE_ENUM cIdxSts = DIP_P2_BUF_STATE_NONE;

	if (property < 0)
		return MFALSE;

	switch (listTag) {
	case DIP_P2_BUFQUE_LIST_TAG_UNIT:
		if (P2_FrameUnit_List_Idx[property].curr < 0) {
			LOG_ERR("curr is negative!(%d)\n", P2_FrameUnit_List_Idx[property].curr);
			return MFALSE;
		}
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
	unsigned int tmpIdx = 0;
	unsigned int property = 0;

	if (enum_property >= DIP_P2_BUFQUE_PROPERTY_NUM ||
		enum_property < DIP_P2_BUFQUE_PROPERTY_DIP) {
		LOG_ERR("property err(%d)\n", enum_property);
		return -EINVAL;
	}
	if (idx ==  -1) {
		LOG_ERR("idx can't be negative!!");
		ret =  -EINVAL;
		return ret;
	}
	property = (unsigned int)enum_property;

	switch (listTag) {
	case DIP_P2_BUFQUE_LIST_TAG_PACKAGE:
	tmpIdx = P2_FramePack_List_Idx[property].start;
	if (tmpIdx < 0) {
		LOG_ERR("tmpIdx is negative\n");
		return MFALSE;
	}
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
	if (tmpIdx < 0) {
		LOG_ERR("tmpIdx is negative\n");
		return MFALSE;
	}
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
	unsigned int property;

	if (param.property >= DIP_P2_BUFQUE_PROPERTY_NUM) {
		LOG_ERR("property err(%d)\n", param.property);
		return idx;
	}
	property = param.property;

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
	enum DIP_P2_BUFQUE_PROPERTY property;

	if (param.property >= DIP_P2_BUFQUE_PROPERTY_NUM) {
		LOG_ERR("property err(%d)\n", param.property);
		return ret;
	}
	property = param.property;
	if (property < 0)
		return MFALSE;
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
		if (index < 0) {
			spin_unlock(&(SpinLock_P2FrameList));
			return MFALSE;
		}
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
	unsigned int property;

	if (param.property >= DIP_P2_BUFQUE_PROPERTY_NUM) {
		LOG_ERR("property err(%d)\n", param.property);
		ret = -EFAULT;
		return ret;
	}
	property = param.property;

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
		if (P2_FrameUnit_List_Idx[property].curr < 0) {
			spin_unlock(&(SpinLock_P2FrameList));
			LOG_ERR("p2 frame curr idx is negative(%d)!!",
			P2_FrameUnit_List_Idx[property].curr);
			ret =  -EFAULT;
			return ret;
		}
		if (P2_FrameUnit_List_Idx[property].end < 0) {
			spin_unlock(&(SpinLock_P2FrameList));
			LOG_ERR("p2 frame idx is negative(%d)!!",
			P2_FrameUnit_List_Idx[property].end);
			ret =  -EFAULT;
			return ret;
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
			if (P2_FramePack_List_Idx[property].end < 0) {
				spin_unlock(&(SpinLock_P2FrameList));
				LOG_ERR("p2 package idx is negative(%d)!!",
				P2_FramePack_List_Idx[property].end);
				ret =  -EFAULT;
				return ret;
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
			LOG_ERR("not match buf(pty/pid/cid:%d/0x%x/0x%x)",
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
			LOG_ERR("Dqe fail,idx(%d),pty(%d),pID(0x%x),cID(0x%x)",
			idx,
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
		if (idx2 ==  -1) {
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
		if (idx ==  -1) {
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
		if (idx ==  -1) {
			LOG_ERR("can't find buf idx(pty/pid/cid:%d/0x%x/0x%x)",
			param.property,
			param.processID,
			param.callerID);
			ret =  -EFAULT;
			return ret;
		}
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
				LOG_ERR("Dqe Buf fail,rT(%d),idx(%d),pty(%d)\n",
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
		LOG_WRN("private_data is NULL,(process,pid,tgid)=(%s,%d,%d)\n",
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
/* 2nd layer behavoir of copy from user is implemented in DIP_ReadReg(...) */
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
/* 2nd layer behavoir of copy from user is implemented in DIP_WriteReg(...) */
			/* Ret = DIP_WriteReg(&RegIo);*/
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
				LOG_ERR("usrKey(%d),max(%d),force usrkey = 0\n",
				IrqInfo.EventInfo.UserKey,
				IRQ_USER_NUM_MAX);
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
		LOG_ERR("Fail,Cmd(%d),Pid(%d),(process,pid,tgid)=(%s,%d,%d)\n",
		Cmd, pUserInfo->Pid,
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

	for (i = 0 ; i < DIP_INIT_ARRAY_COUNT ; i++) {
		DIP_WR32(DIP_A_BASE + DIP_INIT_ARY[i].ofset,
				DIP_INIT_ARY[i].val);
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
		LOG_ERR("kmalloc failed,(com,pid,tgid)=(%s,%d,%d)\n",
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
#ifndef EP_CODE_MARK_KERNEL414
		DIP_pr_detect_count = get_detect_count();
		i = DIP_pr_detect_count + 150;
		set_detect_count(i);
		LOG_DBG("Curr UserCount(%d)\n",	IspInfo.UserCount);
		LOG_DBG("(process, pid, tgid)=(%s, %d, %d)\n",
			current->comm,
			current->pid,
			current->tgid);
		LOG_DBG("log_limit_line(%d), first user\n", i);
#endif
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
	g_dip1sterr = DIP_GCE_EVENT_NONE;
	/* mutex_lock(&gDipMutex); */  /* Protect the Multi Process */
	g_bIonBufferAllocated = MFALSE;

#ifdef AEE_DUMP_BY_USING_ION_MEMORY
	g_dip_p2_imem_buf.handle = NULL;
	g_dip_p2_imem_buf.ion_fd = 0;
	g_dip_p2_imem_buf.va = 0;
	g_dip_p2_imem_buf.pa = 0;
	g_dip_p2_imem_buf.length = ((4*DIP_REG_RANGE) +
		(2*MAX_ISP_TILE_TDR_HEX_NO) +
		(2*MAX_DIP_CMDQ_BUFFER_SIZE) +
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
	}
	g_bUserBufIsReady = MFALSE;
	g_bDumpPhyDIPBuf = MFALSE;
	g_dumpInfo.tdri_baseaddr = 0xFFFFFFFF;/* 0x15022304 */
	g_dumpInfo.imgi_baseaddr = 0xFFFFFFFF;/* 0x15022500 */
	g_dumpInfo.dmgi_baseaddr = 0xFFFFFFFF;/* 0x15022620 */
	g_dumpInfo.cmdq_baseaddr = 0xFFFFFFFF;/* 0x15022620 */
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
#ifndef EP_CODE_MARK_KERNEL414
	set_detect_count(DIP_pr_detect_count);
#endif
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
 *static signed int DIP_mmap(
 *	struct file *pFile, struct vm_area_struct *pVma)
 *{
 *	unsigned long length = 0;
 *	unsigned int pfn = 0x0;
 *
 *	length = (pVma->vm_end - pVma->vm_start);
 *
 *	pVma->vm_page_prot = pgprot_noncached(pVma->vm_page_prot);
 *	pfn = pVma->vm_pgoff << PAGE_SHIFT;
 *
 *
 *	switch (pfn) {
 *	case DIP_A_BASE_HW:
 *		if (length > DIP_REG_RANGE) {
 *			LOG_ERR("mmap range error\n");
 *			LOG_ERR("mod(0x%x),len(0x%lx),dip_regsize(0x%x)\n",
 *			pfn, length, DIP_REG_RANGE);
 *			return -EAGAIN;
 *		}
 *		break;
 *	default:
 *		LOG_ERR("Illegal starting HW addr for mmap!\n");
 *		return -EAGAIN;
 *	}
 *	if (remap_pfn_range(pVma,
 *		pVma->vm_start,
 *		pVma->vm_pgoff,
 *		pVma->vm_end - pVma->vm_start,
 *		pVma->vm_page_prot))
 *		return -EAGAIN;
 *
 *	return 0;
 *}
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
	/*  .mmap = DIP_mmap, */
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

	LOG_INF("- E. DIP driver probe.\n");

	/* Get platform_device parameters */
#ifdef CONFIG_OF

	if (pDev == NULL) {
		LOG_INF("pDev is NULL");
		return -ENXIO;
	}

	nr_dip_devs += 1;

	_dipdev = krealloc(dip_devs,
		sizeof(struct dip_device) * nr_dip_devs,
		GFP_KERNEL);
	if (!_dipdev) {
		LOG_INF("Unable to allocate dip_devs\n");
		return -ENOMEM;
	}
	dip_devs = _dipdev;

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
				(const char *)DIP_IRQ_CB_TBL[i].device_name,
				NULL);
			if (Ret) {
				LOG_INF("request_irq fail\n");
				LOG_INF("nrdevs=%d,node(%s),irq=%d,ISR:%s\n",
				nr_dip_devs,
				pDev->dev.of_node->name,
				dip_dev->irq,
				DIP_IRQ_CB_TBL[i].device_name);
				return Ret;
			}

			LOG_INF("nr_dip_devs=%d,devnode(%s),irq=%d,ISR: %s\n",
				nr_dip_devs,
				pDev->dev.of_node->name,
				dip_dev->irq,
				DIP_IRQ_CB_TBL[i].device_name);
			break;
		}
		}

		if (i >= DIP_IRQ_TYPE_AMOUNT)
			LOG_INF("No corresponding ISR!!\n");
		else
			LOG_INF("nr_dip_devs=%d, devnode(%s), irq=%d\n",
			nr_dip_devs,
			pDev->dev.of_node->name,
			dip_dev->irq);


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
		dip_clk.DIP_IMG_LARB5 =
			devm_clk_get(&pDev->dev, "DIP_CG_IMG_LARB5");
		dip_clk.DIP_IMG_DIP =
			devm_clk_get(&pDev->dev, "DIP_CG_IMG_DIP");

		if (IS_ERR(dip_clk.DIP_IMG_LARB5)) {
			LOG_ERR("cannot get DIP_IMG_LARB5 clock\n");
			return PTR_ERR(dip_clk.DIP_IMG_LARB5);
		}
		if (IS_ERR(dip_clk.DIP_IMG_DIP)) {
			LOG_ERR("cannot get DIP_IMG_DIP clock\n");
			return PTR_ERR(dip_clk.DIP_IMG_DIP);
		}

#endif
		/*  */
		for (i = 0 ; i < DIP_IRQ_TYPE_AMOUNT; i++)
			init_waitqueue_head(&IspInfo.WaitQueueHead[i]);

#ifdef CONFIG_PM_SLEEP
		dip_wake_lock = wakeup_source_register(&pDev->dev, "dip_lock_wakelock");
		isp_mdp_wake_lock = wakeup_source_register(&pDev->dev, "isp_mdp_wakelock");
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

	return 0;
}

/**************************************************************
 *
 **************************************************************/
static signed int DIP_resume(struct platform_device *pDev)
{
	if (g_u4DipCnt > 0) {
		DIP_EnableClock(MTRUE);
		if (G_u4DipEnClkCnt == 1)
			DIP_Load_InitialSettings();
		g_u4DipCnt--;
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
	LOG_INF("DIP suspend G_u4DipEnClkCnt: %d, g_u4DipCnt: %d",
		G_u4DipEnClkCnt,
		g_u4DipCnt);

	return DIP_suspend(pdev, PMSG_SUSPEND);
}

int DIP_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	/*pr_debug("calling %s()\n", __func__);*/
	LOG_INF("DIP resume G_u4DipEnClkCnt: %d, g_u4DipCnt: %d",
		G_u4DipEnClkCnt,
		g_u4DipCnt);

	return DIP_resume(pdev);
}

/* extern void mt_irq_set_sens(unsigned int irq, unsigned int sens); */
/* extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity); */
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

LOG_INF("dip p2 ne dump start!g_bUserBufIsReady:%d, g_bIonBufferAllocated:%d\n",
		g_bUserBufIsReady, g_bIonBufferAllocated);
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

	int i;

	spin_lock(&(IspInfo.SpinLockClock));
	if (G_u4DipEnClkCnt == 0) {
		spin_unlock(&(IspInfo.SpinLockClock));
		return 0;
	}
	spin_unlock(&(IspInfo.SpinLockClock));

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

static signed int __init DIP_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
//#if 0
//	struct device_node *node = NULL;
//#endif
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
			   DIP_MDPClockOnCallback,
			   DIP_MDPDumpCallback,
			   DIP_MDPResetCallback,
			   DIP_MDPClockOffCallback);
	/* Register GCE callback for dumping DIP register */
	LOG_DBG("register dip callback for GCE");
	cmdqCoreRegisterDebugRegDumpCB
		(DIP_BeginGCECallback, DIP_EndGCECallback);
#endif
	/* m4u_enable_tf(M4U_PORT_CAM_IMGI, 0);*/

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
	LOG_DBG("DIP_MDPDumpCallback");

	DIP_DumpDIPReg();

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
	/* LOG_DBG("DIP_MDPClockOffCallback"); */
	DIP_EnableClock(MFALSE);
#ifdef CONFIG_PM_SLEEP
	__pm_relax(isp_mdp_wake_lock);
#endif
	/*LOG_DBG("-MDPEn:%d", G_u4DipEnClkCnt);*/
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
