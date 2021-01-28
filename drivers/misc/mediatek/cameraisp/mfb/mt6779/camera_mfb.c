// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include <linux/sched/clock.h>

/*  */
#include "camera_mfb.h"


#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#ifdef COFNIG_MTK_IOMMU
#include "mtk_iommu.h"
#elif defined(CONFIG_MTK_M4U)
#include <m4u.h>
#endif
#include <cmdq_core.h>
#include <cmdq_record.h>
#include <smi_public.h>

/* #define MFB_PMQOS */
#ifdef MFB_PMQOS
#include <linux/pm_qos.h>
#include <mmdvfs_pmqos.h>
#endif

/* Measure the kernel performance
 * #define __MFB_KERNEL_PERFORMANCE_MEASURE__
 */
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
#include <linux/met_drv.h>
#include <linux/mtk_ftrace.h>
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/*  #include "smi_common.h" */
#ifdef MFB_USE_WAKELOCK
#include <linux/wakelock.h>
#endif
/* CCF */
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#include <linux/clk.h>
struct MFB_CLK_STRUCT {
	struct clk *CG_IMGSYS_MFB;
};
struct MFB_CLK_STRUCT mfb_clk;
#endif
/* !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK)  */

#ifndef MTRUE
#define MTRUE               1
#endif
#ifndef MFALSE
#define MFALSE              0
#endif

#define MFB_DEV_NAME                "camera-mfb"
/* #define EP_NO_CLKMGR */
#define BYPASS_REG         (0)
/* #define MFB_WAITIRQ_LOG */
#define MFB_USE_GCE
/* #define MFB_DEBUG_USE */
#define DUMMY_MFB	   (0)
/* #define MFB_MULTIPROCESS_TIMING_ISSUE  */
/*I can' test the situation in FPGA due to slow FPGA. */
#define MyTag "[MFB]"
#define IRQTag "KEEPER"

#define log_vrb(format,	args...)    pr_debug(MyTag format, ##args)

#ifdef MFB_DEBUG_USE
#define log_dbg(format, args...)    pr_info(MyTag format, ##args)
#else
#define log_dbg(format, args...)
#endif

#define log_inf(format, args...)    pr_info(MyTag format,  ##args)
#define log_notice(format, args...) pr_notice(MyTag format,  ##args)
#define log_wrn(format, args...)    pr_debug(MyTag format,  ##args)
#define log_err(format, args...)    pr_info(MyTag format,  ##args)
#define log_ast(format, args...)    pr_debug(MyTag format, ##args)


/**************************************************************
 *
 **************************************************************/
/* #define MFB_WR32(addr, data)    iowrite32(data, addr) */
#define MFB_WR32(addr, data)    writel(data, addr)
#define MFB_RD32(addr)          readl(addr)
/**************************************************************
 *
 **************************************************************/
/* dynamic log level */
#define MFB_DBG_DBGLOG              (0x00000001)
#define MFB_DBG_INFLOG              (0x00000002)
#define MFB_DBG_INT                 (0x00000004)
#define MFB_DBG_READ_REG            (0x00000008)
#define MFB_DBG_WRITE_REG           (0x00000010)
#define MFB_DBG_TASKLET             (0x00000020)

/*
 *   IRQ signal mask
 */

#define INT_ST_MASK_MFB     ( \
			MFB_INT_ST)

#define CMDQ_REG_MASK 0xffffffff
#define MFB_START      0x1
#define MFB_ENABLE     0x1
#define MFB_IS_BUSY    0x2

static irqreturn_t ISP_Irq_MFB(signed int Irq, void *DeviceId);
static bool ConfigMFB(void);
static signed int ConfigMFBHW(MFB_Config *pMfbConfig);
static void MFB_ScheduleWork(struct work_struct *data);

typedef irqreturn_t(*IRQ_CB) (signed int, void *);

struct ISR_TABLE {
	IRQ_CB isr_fp;
	unsigned int int_number;
	char device_name[16];
};

#ifndef CONFIG_OF
const struct ISR_TABLE MFB_IRQ_CB_TBL[MFB_IRQ_TYPE_AMOUNT] = {
	{ISP_Irq_MFB, MFB_IRQ_BIT_ID, "mfb"},
};

#else
/* int number is got from kernel api */
const struct ISR_TABLE MFB_IRQ_CB_TBL[MFB_IRQ_TYPE_AMOUNT] = {
#if DUMMY_MFB
	{ISP_Irq_MFB, 0, "mfb-dummy"},
#else
	{ISP_Irq_MFB, 0, "mfb"},
#endif
};
#endif
/* /////////////////////////////////////////////////////////// */
/*  */
typedef void (*tasklet_cb) (unsigned long);
struct Tasklet_table {
	tasklet_cb tkt_cb;
	struct tasklet_struct *pMFB_tkt;
};

struct tasklet_struct Mfbtkt[MFB_IRQ_TYPE_AMOUNT];

static void ISP_TaskletFunc_MFB(unsigned long data);

static struct Tasklet_table MFB_tasklet[MFB_IRQ_TYPE_AMOUNT] = {
	{ISP_TaskletFunc_MFB, &Mfbtkt[MFB_IRQ_TYPE_INT_MFB_ST]},
};

#ifdef MFB_USE_WAKELOCK
struct wake_lock MFB_wake_lock;
#endif

static DEFINE_MUTEX(gMfbMutex);
static DEFINE_MUTEX(gMfbDequeMutex);

#ifdef CONFIG_OF

struct MFB_device {
	void __iomem *regs;
	struct device *dev;
	int irq;
};

static struct MFB_device *MFB_devs;
static int nr_MFB_devs;

/* Get HW modules' base address from device nodes */
#define MFB_DEV_NODE_IDX 0
#define IMGSYS_DEV_MODE_IDX 1
/* static unsigned long gISPSYS_Reg[MFB_IRQ_TYPE_AMOUNT]; */

#define ISP_MFB_BASE		(MFB_devs[MFB_DEV_NODE_IDX].regs)
#define ISP_IMGSYS_BASE		(MFB_devs[IMGSYS_DEV_MODE_IDX].regs)

/* #define ISP_MFB_BASE	(gISPSYS_Reg[MFB_DEV_NODE_IDX]) */

#else
#define ISP_MFB_BASE		(IMGSYS_BASE + 0xe000)

#endif

static unsigned int g_u4EnableClockCount;
static unsigned int g_u4MfbCnt;

/* maximum number for supporting user to do interrupt operation */
/* index 0 is for all the user that do not do register irq first */
#define IRQ_USER_NUM_MAX 32

#ifdef MFB_PMQOS
static struct pm_qos_request mfb_qos_request;
static u64 max_img_freq;
#endif

enum MFB_FRAME_STATUS_ENUM {
	MFB_FRAME_STATUS_EMPTY,		/* 0 */
	MFB_FRAME_STATUS_ENQUE,		/* 1 */
	MFB_FRAME_STATUS_RUNNING,	/* 2 */
	MFB_FRAME_STATUS_FINISHED,	/* 3 */
	MFB_FRAME_STATUS_TOTAL
};


enum MFB_REQUEST_STATE_ENUM {
	MFB_REQUEST_STATE_EMPTY,	/* 0 */
	MFB_REQUEST_STATE_PENDING,	/* 1 */
	MFB_REQUEST_STATE_RUNNING,	/* 2 */
	MFB_REQUEST_STATE_FINISHED,	/* 3 */
	MFB_REQUEST_STATE_TOTAL
};

struct MFB_REQUEST_STRUCT {
	enum MFB_REQUEST_STATE_ENUM State;
	pid_t processID;	/* caller process ID */
	unsigned int callerID;	/* caller thread ID */
	unsigned int enqueReqNum; /* judge it belongs to which frame package */
	unsigned int FrameWRIdx;	/* Frame write Index */
	unsigned int RrameRDIdx;	/* Frame read Index */
	enum MFB_FRAME_STATUS_ENUM
		MfbFrameStatus[_SUPPORT_MAX_MFB_FRAME_REQUEST_];
	MFB_Config MfbFrameConfig[_SUPPORT_MAX_MFB_FRAME_REQUEST_];
};

struct MFB_REQUEST_RING_STRUCT {
	unsigned int WriteIdx;	/* enque how many request  */
	unsigned int ReadIdx;		/* read which request index */
	unsigned int HWProcessIdx;	/* HWWriteIdx */
	struct MFB_REQUEST_STRUCT
		MFBReq_Struct[_SUPPORT_MAX_MFB_REQUEST_RING_SIZE_];
};

struct  MFB_CONFIG_STRUCT {
	MFB_Config MfbFrameConfig[_SUPPORT_MAX_MFB_FRAME_REQUEST_];
};

struct S_START_T {
	unsigned int sec;
	unsigned int usec;
};

static struct MFB_REQUEST_RING_STRUCT g_MFB_ReqRing;
static struct MFB_CONFIG_STRUCT g_MfbEnqueReq_Struct;
static struct MFB_CONFIG_STRUCT g_MfbDequeReq_Struct;


/**************************************************************
 *
 **************************************************************/
struct  MFB_USER_INFO_STRUCT {
	pid_t Pid;
	pid_t Tid;
};
enum MFB_PROCESS_ID_ENUM {
	MFB_PROCESS_ID_NONE,
	MFB_PROCESS_ID_MFB,
	MFB_PROCESS_ID_AMOUNT
};

/**************************************************************
 *
 **************************************************************/
struct MFB_IRQ_INFO_STRUCT {
	unsigned int Status[MFB_IRQ_TYPE_AMOUNT];
	signed int MfbIrqCnt;
	pid_t ProcessID[MFB_PROCESS_ID_AMOUNT];
	unsigned int Mask[MFB_IRQ_TYPE_AMOUNT];
};

struct MFB_INFO_STRUCT {
	spinlock_t SpinLockMFBRef;
	spinlock_t SpinLockMFB;
	spinlock_t SpinLockIrq[MFB_IRQ_TYPE_AMOUNT];
	wait_queue_head_t WaitQueueHead;
	struct work_struct ScheduleMfbWork;
	unsigned int UserCount;	/* User Count */
	unsigned int DebugMask;	/* Debug Mask */
	signed int IrqNum;
	struct MFB_IRQ_INFO_STRUCT IrqInfo;
	signed int WriteReqIdx;
	signed int ReadReqIdx;
	pid_t ProcessID[_SUPPORT_MAX_MFB_FRAME_REQUEST_];
};

static struct MFB_INFO_STRUCT MFBInfo;

enum eLOG_TYPE {
	/* currently, only used at ipl_buf_ctrl for critical section */
	_LOG_DBG = 0,
	_LOG_INF = 1,
	_LOG_ERR = 2,
	_LOG_MAX = 3,
};

#define NORMAL_STR_LEN (512)
#define ERR_PAGE 2
#define DBG_PAGE 2
#define INF_PAGE 4
/* #define SV_LOG_STR_LEN NORMAL_STR_LEN */

#define LOG_PPNUM 2
static unsigned int m_CurrentPPB;
struct SV_LOG_STR {
	unsigned int _cnt[LOG_PPNUM][_LOG_MAX];
	/* char   _str[_LOG_MAX][SV_LOG_STR_LEN]; */
	char *_str[LOG_PPNUM][_LOG_MAX];
	struct S_START_T   _lastIrqTime;
} *MFB_PSV_LOG_STR;

static void *pLog_kmalloc;
static struct SV_LOG_STR gSvLog[MFB_IRQ_TYPE_AMOUNT];

/*
 *   for irq used,keep log until IRQ_LOG_PRINTER being involked,
 *   limited:
 *   each log must shorter than 512 bytes
 *  total log length in each irq/logtype can't over 1024 bytes
 */
#define IRQ_LOG_KEEPER(irq, ppb, logT, fmt, ...) do {\
	char *ptr; \
	char *pDes;\
	signed int avaLen;\
	unsigned int *ptr2 = &gSvLog[irq]._cnt[ppb][logT];\
	unsigned int str_leng;\
	unsigned int i = 0;\
	int ret; \
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
	(char *)&(gSvLog[irq]._str[ppb][logT][gSvLog[irq]._cnt[ppb][logT]]); \
	avaLen = str_leng - 1 - gSvLog[irq]._cnt[ppb][logT]; \
	if (avaLen > 1) { \
		ret = snprintf((char *)(pDes), avaLen, "[%d.%06d]" fmt, \
		gSvLog[irq]._lastIrqTime.sec, gSvLog[irq]._lastIrqTime.usec, \
		##__VA_ARGS__); \
		if (ret < 0) { \
			log_err("snprintf fail(%d)\n", ret); \
		} \
		if ('\0' != gSvLog[irq]._str[ppb][logT][str_leng - 1]) { \
			log_err("log str over flow(%d)\n", irq); \
		} \
		while (*ptr++ != '\0') { \
			(*ptr2)++; \
		} \
	} else { \
		log_inf("(%d)(%d)log str avalible=0, print log\n", irq, logT);\
	ptr = pSrc->_str[ppb][logT];\
	if (pSrc->_cnt[ppb][logT] != 0) {\
		if (logT == _LOG_DBG) {\
			for (i = 0; i < DBG_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					log_dbg("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else {\
					log_dbg("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else if (logT == _LOG_INF) {\
			for (i = 0; i < INF_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					log_inf("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else {\
					log_inf("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else if (logT == _LOG_ERR) {\
			for (i = 0; i < ERR_PAGE; i++) {\
				if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
					ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
					log_err("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else {\
					log_err("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
		else {\
			log_err("N.S.%d", logT);\
		} \
		ptr[0] = '\0';\
		pSrc->_cnt[ppb][logT] = 0;\
		avaLen = str_leng - 1;\
		ptr = pDes = \
		(char *)&(pSrc->_str[ppb][logT][pSrc->_cnt[ppb][logT]]);\
		ptr2 = &(pSrc->_cnt[ppb][logT]);\
		ret = snprintf((char *)(pDes), avaLen, fmt, ##__VA_ARGS__);   \
		if (ret < 0) { \
			log_err("snprintf fail(%d)\n", ret); \
		} \
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
					log_dbg("%s", &ptr[NORMAL_STR_LEN*i]);\
				} else {\
					log_dbg("%s", &ptr[NORMAL_STR_LEN*i]);\
					break;\
				} \
			} \
		} \
	else if (logT == _LOG_INF) {\
		for (i = 0; i < INF_PAGE; i++) {\
			if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
				ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
				log_inf("%s", &ptr[NORMAL_STR_LEN*i]);\
			} else {\
				log_inf("%s", &ptr[NORMAL_STR_LEN*i]);\
				break;\
			} \
		} \
	} \
	else if (logT == _LOG_ERR) {\
		for (i = 0; i < ERR_PAGE; i++) {\
			if (ptr[NORMAL_STR_LEN*(i+1) - 1] != '\0') {\
				ptr[NORMAL_STR_LEN*(i+1) - 1] = '\0';\
				log_err("%s", &ptr[NORMAL_STR_LEN*i]);\
			} else {\
				log_err("%s", &ptr[NORMAL_STR_LEN*i]);\
				break;\
			} \
		} \
	} \
	else {\
		log_err("N.S.%d", logT);\
	} \
		ptr[0] = '\0';\
		pSrc->_cnt[ppb][logT] = 0;\
	} \
} while (0)


#define IMGSYS_REG_CG_CON                      (ISP_IMGSYS_BASE + 0x0)
#define IMGSYS_REG_CG_SET                      (ISP_IMGSYS_BASE + 0x4)
#define IMGSYS_REG_CG_CLR                      (ISP_IMGSYS_BASE + 0x8)

/* MFB unmapped base address macro for GCE to access */
#define C02_CON_HW                             (MFB_BASE_HW)
#define C02_CROP_CON1_HW                       (MFB_BASE_HW + 0x04)
#define C02_CROP_CON2_HW                       (MFB_BASE_HW + 0x08)

#define SRZ_CONTROL_HW                         (MFB_BASE_HW + 0x40)
#define SRZ_IN_IMG_HW                          (MFB_BASE_HW + 0x44)
#define SRZ_OUT_IMG_HW                         (MFB_BASE_HW + 0x48)
#define SRZ_HORI_STEP_HW                       (MFB_BASE_HW + 0x4C)
#define SRZ_VERT_STEP_HW                       (MFB_BASE_HW + 0x50)
#define SRZ_HORI_INT_OFST_HW                   (MFB_BASE_HW + 0x54)
#define SRZ_HORI_SUB_OFST_HW                   (MFB_BASE_HW + 0x58)
#define SRZ_VERT_INT_OFST_HW                   (MFB_BASE_HW + 0x5C)
#define SRZ_VERT_SUB_OFST_HW                   (MFB_BASE_HW + 0x60)

#define CRSP_CTRL_HW                           (MFB_BASE_HW + 0x80)
#define CRSP_OUT_IMG_HW                        (MFB_BASE_HW + 0x84)
#define CRSP_STEP_OFST_HW                      (MFB_BASE_HW + 0x88)
#define CRSP_CROP_X_HW                         (MFB_BASE_HW + 0x8C)
#define CRSP_CROP_Y_HW                         (MFB_BASE_HW + 0x90)

#define OMC_TOP_HW                             (MFB_BASE_HW + 0x100)
#define OMC_ATPG_HW                            (MFB_BASE_HW + 0x104)
#define OMC_FRAME_SIZE_HW                      (MFB_BASE_HW + 0x108)
#define OMC_TILE_EDGE_HW                       (MFB_BASE_HW + 0x10C)
#define OMC_TILE_OFS_HW                        (MFB_BASE_HW + 0x110)
#define OMC_TILE_SIZE_HW                       (MFB_BASE_HW + 0x114)
#define OMC_TILE_CROP_X_HW                     (MFB_BASE_HW + 0x118)
#define OMC_TILE_CROP_Y_HW                     (MFB_BASE_HW + 0x11C)
#define OMC_MV_RDMA_BASE_ADDR_HW               (MFB_BASE_HW + 0x120)
#define OMC_MV_RDMA_STRIDE_HW                  (MFB_BASE_HW + 0x124)

#define OMCC_OMC_C_CFIFO_CTL_HW                (MFB_BASE_HW + 0x180)
#define OMCC_OMC_C_RWCTL_CTL_HW                (MFB_BASE_HW + 0x184)
#define OMCC_OMC_C_CACHI_SPECIAL_FUN_EN_HW     (MFB_BASE_HW + 0x188)
#define OMCC_OMC_C_ADDR_GEN_BASE_ADDR_0_HW     (MFB_BASE_HW + 0x18C)
#define OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_0_HW   (MFB_BASE_HW + 0x190)
#define OMCC_OMC_C_ADDR_GEN_STRIDE_0_HW        (MFB_BASE_HW + 0x194)
#define OMCC_OMC_C_ADDR_GEN_BASE_ADDR_1_HW     (MFB_BASE_HW + 0x198)
#define OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_1_HW   (MFB_BASE_HW + 0x19C)
#define OMCC_OMC_C_ADDR_GEN_STRIDE_1_HW        (MFB_BASE_HW + 0x1A0)
#define OMCC_OMC_C_CACHI_CON2_0_HW             (MFB_BASE_HW + 0x1A4)
#define OMCC_OMC_C_CACHI_CON3_0_HW             (MFB_BASE_HW + 0x1A8)
#define OMCC_OMC_C_CTL_SW_CTL_HW               (MFB_BASE_HW + 0x1AC)
#define OMCC_OMC_C_CTL_CFG_HW                  (MFB_BASE_HW + 0x1B0)
#define OMCC_OMC_C_CTL_FMT_SEL_HW              (MFB_BASE_HW + 0x1B4)
#define OMCC_OMC_C_CTL_RSV0_HW                 (MFB_BASE_HW + 0x1B8)

#define MFB_CON_HW                             (MFB_BASE_HW + 0x200)
#define MFB_LL_CON1_HW                         (MFB_BASE_HW + 0x204)
#define MFB_LL_CON2_HW                         (MFB_BASE_HW + 0x208)
#define MFB_EDGE_HW                            (MFB_BASE_HW + 0x214)
#define MFB_LL_CON5_HW                         (MFB_BASE_HW + 0x218)
#define MFB_LL_CON6_HW                         (MFB_BASE_HW + 0x21C)
#define MFB_LL_CON7_HW                         (MFB_BASE_HW + 0x220)
#define MFB_LL_CON8_HW                         (MFB_BASE_HW + 0x224)
#define MFB_LL_CON9_HW                         (MFB_BASE_HW + 0x228)
#define MFB_LL_CON10_HW                        (MFB_BASE_HW + 0x22C)
#define MFB_MBD_CON0_HW                        (MFB_BASE_HW + 0x230)
#define MFB_MBD_CON1_HW                        (MFB_BASE_HW + 0x234)
#define MFB_MBD_CON2_HW                        (MFB_BASE_HW + 0x238)
#define MFB_MBD_CON3_HW                        (MFB_BASE_HW + 0x23C)
#define MFB_MBD_CON4_HW                        (MFB_BASE_HW + 0x240)
#define MFB_MBD_CON5_HW                        (MFB_BASE_HW + 0x244)
#define MFB_MBD_CON6_HW                        (MFB_BASE_HW + 0x248)
#define MFB_MBD_CON7_HW                        (MFB_BASE_HW + 0x24C)
#define MFB_MBD_CON8_HW                        (MFB_BASE_HW + 0x250)
#define MFB_MBD_CON9_HW                        (MFB_BASE_HW + 0x254)
#define MFB_MBD_CON10_HW                       (MFB_BASE_HW + 0x258)

#define MFB_MFB_TOP_CFG0_HW                    (MFB_BASE_HW + 0x25C)
#define MFB_MFB_TOP_CFG1_HW                    (MFB_BASE_HW + 0x260)
#define MFB_MFB_TOP_CFG2_HW                    (MFB_BASE_HW + 0x264)
#define MFB_MFB_INT_CTL_HW                     (MFB_BASE_HW + 0x268)
#define MFB_MFB_INT_STATUS_HW                  (MFB_BASE_HW + 0x26C)
#define MFB_MFB_SW_RST_HW                      (MFB_BASE_HW + 0x270)
#define MFB_MFB_MAIN_DCM_ST_HW                 (MFB_BASE_HW + 0x274)
#define MFB_MFB_DMA_DCM_ST_HW                  (MFB_BASE_HW + 0x278)
#define MFB_MFB_MAIN_DCM_DIS_HW                (MFB_BASE_HW + 0x27C)
#define MFB_MFB_DBG_CTL0_HW                    (MFB_BASE_HW + 0x284)
#define MFB_MFB_DBG_CTL1_HW                    (MFB_BASE_HW + 0x288)
#define MFB_MFB_DBG_CTL2_HW                    (MFB_BASE_HW + 0x28C)
#define MFB_MFB_DBG_OUT0_HW                    (MFB_BASE_HW + 0x290)
#define MFB_MFB_DBG_OUT1_HW                    (MFB_BASE_HW + 0x294)
#define MFB_MFB_DBG_OUT2_HW                    (MFB_BASE_HW + 0x298)
#define MFB_MFB_DBG_OUT3_HW                    (MFB_BASE_HW + 0x29C)
#define MFB_MFB_DBG_OUT4_HW                    (MFB_BASE_HW + 0x2A0)
#define MFB_MFB_DBG_OUT5_HW                    (MFB_BASE_HW + 0x2A4)
#define MFB_DFTC_HW                            (MFB_BASE_HW + 0x2AC)

#define MFBDMA_DMA_SOFT_RSTSTAT_HW             (MFB_BASE_HW + 0x400)
#define MFBDMA_TDRI_BASE_ADDR_HW               (MFB_BASE_HW + 0x404)
#define MFBDMA_TDRI_OFST_ADDR_HW               (MFB_BASE_HW + 0x408)
#define MFBDMA_TDRI_XSIZE_HW                   (MFB_BASE_HW + 0x40C)
#define MFBDMA_VERTICAL_FLIP_EN_HW             (MFB_BASE_HW + 0x410)
#define MFBDMA_DMA_SOFT_RESET_HW               (MFB_BASE_HW + 0x414)
#define MFBDMA_LAST_ULTRA_EN_HW                (MFB_BASE_HW + 0x418)
#define MFBDMA_SPECIAL_FUN_EN_HW               (MFB_BASE_HW + 0x41C)
#define MFBDMA_MFBO_BASE_ADDR_HW               (MFB_BASE_HW + 0x420)
#define MFBDMA_MFBO_OFST_ADDR_HW               (MFB_BASE_HW + 0x424)
#define MFBDMA_MFBO_XSIZE_HW                   (MFB_BASE_HW + 0x428)
#define MFBDMA_MFBO_YSIZE_HW                   (MFB_BASE_HW + 0x42C)
#define MFBDMA_MFBO_STRIDE_HW                  (MFB_BASE_HW + 0x430)
#define MFBDMA_MFBO_CON_HW                     (MFB_BASE_HW + 0x434)
#define MFBDMA_MFBO_CON2_HW                    (MFB_BASE_HW + 0x438)
#define MFBDMA_MFBO_CON3_HW                    (MFB_BASE_HW + 0x43C)
#define MFBDMA_MFBO_CROP_HW                    (MFB_BASE_HW + 0x440)
#define MFBDMA_MFB2O_BASE_ADDR_HW              (MFB_BASE_HW + 0x444)
#define MFBDMA_MFB2O_OFST_ADDR_HW              (MFB_BASE_HW + 0x448)
#define MFBDMA_MFB2O_XSIZE_HW                  (MFB_BASE_HW + 0x44C)
#define MFBDMA_MFB2O_YSIZE_HW                  (MFB_BASE_HW + 0x450)
#define MFBDMA_MFB2O_STRIDE_HW                 (MFB_BASE_HW + 0x454)
#define MFBDMA_MFB2O_CON_HW                    (MFB_BASE_HW + 0x458)
#define MFBDMA_MFB2O_CON2_HW                   (MFB_BASE_HW + 0x45C)
#define MFBDMA_MFB2O_CON3_HW                   (MFB_BASE_HW + 0x460)
#define MFBDMA_MFB2O_CROP_HW                   (MFB_BASE_HW + 0x464)
#define MFBDMA_MFBI_BASE_ADDR_HW               (MFB_BASE_HW + 0x468)
#define MFBDMA_MFBI_OFST_ADDR_HW               (MFB_BASE_HW + 0x46C)
#define MFBDMA_MFBI_XSIZE_HW                   (MFB_BASE_HW + 0x470)
#define MFBDMA_MFBI_YSIZE_HW                   (MFB_BASE_HW + 0x474)
#define MFBDMA_MFBI_STRIDE_HW                  (MFB_BASE_HW + 0x478)
#define MFBDMA_MFBI_CON_HW                     (MFB_BASE_HW + 0x47C)
#define MFBDMA_MFBI_CON2_HW                    (MFB_BASE_HW + 0x480)
#define MFBDMA_MFBI_CON3_HW                    (MFB_BASE_HW + 0x484)
#define MFBDMA_MFB2I_BASE_ADDR_HW              (MFB_BASE_HW + 0x488)
#define MFBDMA_MFB2I_OFST_ADDR_HW              (MFB_BASE_HW + 0x48C)
#define MFBDMA_MFB2I_XSIZE_HW                  (MFB_BASE_HW + 0x490)
#define MFBDMA_MFB2I_YSIZE_HW                  (MFB_BASE_HW + 0x494)
#define MFBDMA_MFB2I_STRIDE_HW                 (MFB_BASE_HW + 0x498)
#define MFBDMA_MFB2I_CON_HW                    (MFB_BASE_HW + 0x49C)
#define MFBDMA_MFB2I_CON2_HW                   (MFB_BASE_HW + 0x4A0)
#define MFBDMA_MFB2I_CON3_HW                   (MFB_BASE_HW + 0x4A4)
#define MFBDMA_MFB3I_BASE_ADDR_HW              (MFB_BASE_HW + 0x4A8)
#define MFBDMA_MFB3I_OFST_ADDR_HW              (MFB_BASE_HW + 0x4AC)
#define MFBDMA_MFB3I_XSIZE_HW                  (MFB_BASE_HW + 0x4B0)
#define MFBDMA_MFB3I_YSIZE_HW                  (MFB_BASE_HW + 0x4B4)
#define MFBDMA_MFB3I_STRIDE_HW                 (MFB_BASE_HW + 0x4B8)
#define MFBDMA_MFB3I_CON_HW                    (MFB_BASE_HW + 0x4BC)
#define MFBDMA_MFB3I_CON2_HW                   (MFB_BASE_HW + 0x4C0)
#define MFBDMA_MFB3I_CON3_HW                   (MFB_BASE_HW + 0x4C4)
#define MFBDMA_MFB4I_BASE_ADDR_HW              (MFB_BASE_HW + 0x4C8)
#define MFBDMA_MFB4I_OFST_ADDR_HW              (MFB_BASE_HW + 0x4CC)
#define MFBDMA_MFB4I_XSIZE_HW                  (MFB_BASE_HW + 0x4D0)
#define MFBDMA_MFB4I_YSIZE_HW                  (MFB_BASE_HW + 0x4D4)
#define MFBDMA_MFB4I_STRIDE_HW                 (MFB_BASE_HW + 0x4D8)
#define MFBDMA_MFB4I_CON_HW                    (MFB_BASE_HW + 0x4DC)
#define MFBDMA_MFB4I_CON2_HW                   (MFB_BASE_HW + 0x4E0)
#define MFBDMA_MFB4I_CON3_HW                   (MFB_BASE_HW + 0x4E4)
#define MFBDMA_DMA_ERR_CTRL_HW                 (MFB_BASE_HW + 0x4E8)
#define MFBDMA_MFBO_ERR_STAT_HW                (MFB_BASE_HW + 0x4EC)
#define MFBDMA_MFB2O_ERR_STAT_HW               (MFB_BASE_HW + 0x4F0)
#define MFBDMA_MFBO_B_ERR_STAT_HW              (MFB_BASE_HW + 0x4F4)
#define MFBDMA_MFBI_ERR_STAT_HW                (MFB_BASE_HW + 0x4F8)
#define MFBDMA_MFB2I_ERR_STAT_HW               (MFB_BASE_HW + 0x4FC)
#define MFBDMA_MFB3I_ERR_STAT_HW               (MFB_BASE_HW + 0x500)
#define MFBDMA_MFB4I_ERR_STAT_HW               (MFB_BASE_HW + 0x504)
#define MFBDMA_MFBI_B_ERR_STAT_HW              (MFB_BASE_HW + 0x508)
#define MFBDMA_MFB2I_B_ERR_STAT_HW             (MFB_BASE_HW + 0x50C)
#define MFBDMA_DMA_DEBUG_ADDR_HW               (MFB_BASE_HW + 0x510)
#define MFBDMA_DMA_RSV1_HW                     (MFB_BASE_HW + 0x514)
#define MFBDMA_DMA_RSV2_HW                     (MFB_BASE_HW + 0x518)
#define MFBDMA_DMA_RSV3_HW                     (MFB_BASE_HW + 0x51C)
#define MFBDMA_DMA_RSV4_HW                     (MFB_BASE_HW + 0x520)
#define MFBDMA_DMA_DEBUG_SEL_HW                (MFB_BASE_HW + 0x524)
#define MFBDMA_DMA_BW_SELF_TEST_HW             (MFB_BASE_HW + 0x528)
#define MFBDMA_MFBO_B_BASE_ADDR_HW             (MFB_BASE_HW + 0x52C)
#define MFBDMA_MFBO_B_OFST_ADDR_HW             (MFB_BASE_HW + 0x530)
#define MFBDMA_MFBO_B_XSIZE_HW                 (MFB_BASE_HW + 0x534)
#define MFBDMA_MFBO_B_YSIZE_HW                 (MFB_BASE_HW + 0x538)
#define MFBDMA_MFBO_B_STRIDE_HW                (MFB_BASE_HW + 0x53C)
#define MFBDMA_MFBO_B_CON_HW                   (MFB_BASE_HW + 0x540)
#define MFBDMA_MFBO_B_CON2_HW                  (MFB_BASE_HW + 0x544)
#define MFBDMA_MFBO_B_CON3_HW                  (MFB_BASE_HW + 0x548)
#define MFBDMA_MFBO_B_CROP_HW                  (MFB_BASE_HW + 0x54C)
#define MFBDMA_MFBI_B_BASE_ADDR_HW             (MFB_BASE_HW + 0x550)
#define MFBDMA_MFBI_B_OFST_ADDR_HW             (MFB_BASE_HW + 0x554)
#define MFBDMA_MFBI_B_XSIZE_HW                 (MFB_BASE_HW + 0x558)
#define MFBDMA_MFBI_B_YSIZE_HW                 (MFB_BASE_HW + 0x55C)
#define MFBDMA_MFBI_B_STRIDE_HW                (MFB_BASE_HW + 0x560)
#define MFBDMA_MFBI_B_CON_HW                   (MFB_BASE_HW + 0x564)
#define MFBDMA_MFBI_B_CON2_HW                  (MFB_BASE_HW + 0x568)
#define MFBDMA_MFBI_B_CON3_HW                  (MFB_BASE_HW + 0x56C)
#define MFBDMA_MFB2I_B_BASE_ADDR_HW            (MFB_BASE_HW + 0x570)
#define MFBDMA_MFB2I_B_OFST_ADDR_HW            (MFB_BASE_HW + 0x574)
#define MFBDMA_MFB2I_B_XSIZE_HW                (MFB_BASE_HW + 0x578)
#define MFBDMA_MFB2I_B_YSIZE_HW                (MFB_BASE_HW + 0x57C)
#define MFBDMA_MFB2I_B_STRIDE_HW               (MFB_BASE_HW + 0x580)
#define MFBDMA_MFB2I_B_CON_HW                  (MFB_BASE_HW + 0x584)
#define MFBDMA_MFB2I_B_CON2_HW                 (MFB_BASE_HW + 0x588)
#define MFBDMA_MFB2I_B_CON3_HW                 (MFB_BASE_HW + 0x58C)

#define PAK_CONT_Y_HW                          (MFB_BASE_HW + 0x800)
#define PAK_CONT_C_HW                          (MFB_BASE_HW + 0x810)
#define UNP_OFST_Y_HW                          (MFB_BASE_HW + 0x820)
#define UNP_CONT_Y_HW                          (MFB_BASE_HW + 0x824)
#define UNP_OFST_C_HW                          (MFB_BASE_HW + 0x830)
#define UNP_CONT_C_HW                          (MFB_BASE_HW + 0x834)

/*SW Access Registers : using mapped base address from DTS*/
#define C02_CON_REG                            (ISP_MFB_BASE)
#define C02_CROP_CON1_REG                      (ISP_MFB_BASE + 0x04)
#define C02_CROP_CON2_REG                      (ISP_MFB_BASE + 0x08)

#define SRZ_CONTROL_REG                        (ISP_MFB_BASE + 0x40)
#define SRZ_IN_IMG_REG                         (ISP_MFB_BASE + 0x44)
#define SRZ_OUT_IMG_REG                        (ISP_MFB_BASE + 0x48)
#define SRZ_HORI_STEP_REG                      (ISP_MFB_BASE + 0x4C)
#define SRZ_VERT_STEP_REG                      (ISP_MFB_BASE + 0x50)
#define SRZ_HORI_INT_OFST_REG                  (ISP_MFB_BASE + 0x54)
#define SRZ_HORI_SUB_OFST_REG                  (ISP_MFB_BASE + 0x58)
#define SRZ_VERT_INT_OFST_REG                  (ISP_MFB_BASE + 0x5C)
#define SRZ_VERT_SUB_OFST_REG                  (ISP_MFB_BASE + 0x60)

#define CRSP_CTRL_REG                          (ISP_MFB_BASE + 0x80)
#define CRSP_OUT_IMG_REG                       (ISP_MFB_BASE + 0x84)
#define CRSP_STEP_OFST_REG                     (ISP_MFB_BASE + 0x88)
#define CRSP_CROP_X_REG                        (ISP_MFB_BASE + 0x8C)
#define CRSP_CROP_Y_REG                        (ISP_MFB_BASE + 0x90)

#define OMC_TOP_REG                            (ISP_MFB_BASE + 0x100)
#define OMC_ATPG_REG                           (ISP_MFB_BASE + 0x104)
#define OMC_FRAME_SIZE_REG                     (ISP_MFB_BASE + 0x108)
#define OMC_TILE_EDGE_REG                      (ISP_MFB_BASE + 0x10C)
#define OMC_TILE_OFS_REG                       (ISP_MFB_BASE + 0x110)
#define OMC_TILE_SIZE_REG                      (ISP_MFB_BASE + 0x114)
#define OMC_TILE_CROP_X_REG                    (ISP_MFB_BASE + 0x118)
#define OMC_TILE_CROP_Y_REG                    (ISP_MFB_BASE + 0x11C)
#define OMC_MV_RDMA_BASE_ADDR_REG              (ISP_MFB_BASE + 0x120)
#define OMC_MV_RDMA_STRIDE_REG                 (ISP_MFB_BASE + 0x124)

#define OMCC_OMC_C_CFIFO_CTL_REG               (ISP_MFB_BASE + 0x180)
#define OMCC_OMC_C_RWCTL_CTL_REG               (ISP_MFB_BASE + 0x184)
#define OMCC_OMC_C_CACHI_SPECIAL_FUN_EN_REG    (ISP_MFB_BASE + 0x188)
#define OMCC_OMC_C_ADDR_GEN_BASE_ADDR_0_REG    (ISP_MFB_BASE + 0x18C)
#define OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_0_REG  (ISP_MFB_BASE + 0x190)
#define OMCC_OMC_C_ADDR_GEN_STRIDE_0_REG       (ISP_MFB_BASE + 0x194)
#define OMCC_OMC_C_ADDR_GEN_BASE_ADDR_1_REG    (ISP_MFB_BASE + 0x198)
#define OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_1_REG  (ISP_MFB_BASE + 0x19C)
#define OMCC_OMC_C_ADDR_GEN_STRIDE_1_REG       (ISP_MFB_BASE + 0x1A0)
#define OMCC_OMC_C_CACHI_CON2_0_REG            (ISP_MFB_BASE + 0x1A4)
#define OMCC_OMC_C_CACHI_CON3_0_REG            (ISP_MFB_BASE + 0x1A8)
#define OMCC_OMC_C_CTL_SW_CTL_REG              (ISP_MFB_BASE + 0x1AC)
#define OMCC_OMC_C_CTL_CFG_REG                 (ISP_MFB_BASE + 0x1B0)
#define OMCC_OMC_C_CTL_FMT_SEL_REG             (ISP_MFB_BASE + 0x1B4)
#define OMCC_OMC_C_CTL_RSV0_REG                (ISP_MFB_BASE + 0x1B8)

#define MFB_CON_REG                            (ISP_MFB_BASE + 0x200)
#define MFB_LL_CON1_REG                        (ISP_MFB_BASE + 0x204)
#define MFB_LL_CON2_REG                        (ISP_MFB_BASE + 0x208)
#define MFB_EDGE_REG                           (ISP_MFB_BASE + 0x214)
#define MFB_LL_CON5_REG                        (ISP_MFB_BASE + 0x218)
#define MFB_LL_CON6_REG                        (ISP_MFB_BASE + 0x21C)
#define MFB_LL_CON7_REG                        (ISP_MFB_BASE + 0x220)
#define MFB_LL_CON8_REG                        (ISP_MFB_BASE + 0x224)
#define MFB_LL_CON9_REG                        (ISP_MFB_BASE + 0x228)
#define MFB_LL_CON10_REG                       (ISP_MFB_BASE + 0x22C)
#define MFB_MBD_CON0_REG                       (ISP_MFB_BASE + 0x230)
#define MFB_MBD_CON1_REG                       (ISP_MFB_BASE + 0x234)
#define MFB_MBD_CON2_REG                       (ISP_MFB_BASE + 0x238)
#define MFB_MBD_CON3_REG                       (ISP_MFB_BASE + 0x23C)
#define MFB_MBD_CON4_REG                       (ISP_MFB_BASE + 0x240)
#define MFB_MBD_CON5_REG                       (ISP_MFB_BASE + 0x244)
#define MFB_MBD_CON6_REG                       (ISP_MFB_BASE + 0x248)
#define MFB_MBD_CON7_REG                       (ISP_MFB_BASE + 0x24C)
#define MFB_MBD_CON8_REG                       (ISP_MFB_BASE + 0x250)
#define MFB_MBD_CON9_REG                       (ISP_MFB_BASE + 0x254)
#define MFB_MBD_CON10_REG                      (ISP_MFB_BASE + 0x258)

#define MFB_MFB_TOP_CFG0_REG                   (ISP_MFB_BASE + 0x25C)
#define MFB_MFB_TOP_CFG1_REG                   (ISP_MFB_BASE + 0x260)
#define MFB_MFB_TOP_CFG2_REG                   (ISP_MFB_BASE + 0x264)
#define MFB_MFB_INT_CTL_REG                    (ISP_MFB_BASE + 0x268)
#define MFB_MFB_INT_STATUS_REG                 (ISP_MFB_BASE + 0x26C)
#define MFB_MFB_SW_RST_REG                     (ISP_MFB_BASE + 0x270)
#define MFB_MFB_MAIN_DCM_ST_REG                (ISP_MFB_BASE + 0x274)
#define MFB_MFB_DMA_DCM_ST_REG                 (ISP_MFB_BASE + 0x278)
#define MFB_MFB_MAIN_DCM_DIS_REG               (ISP_MFB_BASE + 0x27C)
#define MFB_MFB_DBG_CTL0_REG                   (ISP_MFB_BASE + 0x284)
#define MFB_MFB_DBG_CTL1_REG                   (ISP_MFB_BASE + 0x288)
#define MFB_MFB_DBG_CTL2_REG                   (ISP_MFB_BASE + 0x28C)
#define MFB_MFB_DBG_OUT0_REG                   (ISP_MFB_BASE + 0x290)
#define MFB_MFB_DBG_OUT1_REG                   (ISP_MFB_BASE + 0x294)
#define MFB_MFB_DBG_OUT2_REG                   (ISP_MFB_BASE + 0x298)
#define MFB_MFB_DBG_OUT3_REG                   (ISP_MFB_BASE + 0x29C)
#define MFB_MFB_DBG_OUT4_REG                   (ISP_MFB_BASE + 0x2A0)
#define MFB_MFB_DBG_OUT5_REG                   (ISP_MFB_BASE + 0x2A4)
#define MFB_DFTC_REG                           (ISP_MFB_BASE + 0x2AC)

#define MFBDMA_DMA_SOFT_RSTSTAT_REG            (ISP_MFB_BASE + 0x400)
#define MFBDMA_TDRI_BASE_ADDR_REG              (ISP_MFB_BASE + 0x404)
#define MFBDMA_TDRI_OFST_ADDR_REG              (ISP_MFB_BASE + 0x408)
#define MFBDMA_TDRI_XSIZE_REG                  (ISP_MFB_BASE + 0x40C)
#define MFBDMA_VERTICAL_FLIP_EN_REG            (ISP_MFB_BASE + 0x410)
#define MFBDMA_DMA_SOFT_RESET_REG              (ISP_MFB_BASE + 0x414)
#define MFBDMA_LAST_ULTRA_EN_REG               (ISP_MFB_BASE + 0x418)
#define MFBDMA_SPECIAL_FUN_EN_REG              (ISP_MFB_BASE + 0x41C)
#define MFBDMA_MFBO_BASE_ADDR_REG              (ISP_MFB_BASE + 0x420)
#define MFBDMA_MFBO_OFST_ADDR_REG              (ISP_MFB_BASE + 0x424)
#define MFBDMA_MFBO_XSIZE_REG                  (ISP_MFB_BASE + 0x428)
#define MFBDMA_MFBO_YSIZE_REG                  (ISP_MFB_BASE + 0x42C)
#define MFBDMA_MFBO_STRIDE_REG                 (ISP_MFB_BASE + 0x430)
#define MFBDMA_MFBO_CON_REG                    (ISP_MFB_BASE + 0x434)
#define MFBDMA_MFBO_CON2_REG                   (ISP_MFB_BASE + 0x438)
#define MFBDMA_MFBO_CON3_REG                   (ISP_MFB_BASE + 0x43C)
#define MFBDMA_MFBO_CROP_REG                   (ISP_MFB_BASE + 0x440)
#define MFBDMA_MFB2O_BASE_ADDR_REG             (ISP_MFB_BASE + 0x444)
#define MFBDMA_MFB2O_OFST_ADDR_REG             (ISP_MFB_BASE + 0x448)
#define MFBDMA_MFB2O_XSIZE_REG                 (ISP_MFB_BASE + 0x44C)
#define MFBDMA_MFB2O_YSIZE_REG                 (ISP_MFB_BASE + 0x450)
#define MFBDMA_MFB2O_STRIDE_REG                (ISP_MFB_BASE + 0x454)
#define MFBDMA_MFB2O_CON_REG                   (ISP_MFB_BASE + 0x458)
#define MFBDMA_MFB2O_CON2_REG                  (ISP_MFB_BASE + 0x45C)
#define MFBDMA_MFB2O_CON3_REG                  (ISP_MFB_BASE + 0x460)
#define MFBDMA_MFB2O_CROP_REG                  (ISP_MFB_BASE + 0x464)
#define MFBDMA_MFBI_BASE_ADDR_REG              (ISP_MFB_BASE + 0x468)
#define MFBDMA_MFBI_OFST_ADDR_REG              (ISP_MFB_BASE + 0x46C)
#define MFBDMA_MFBI_XSIZE_REG                  (ISP_MFB_BASE + 0x470)
#define MFBDMA_MFBI_YSIZE_REG                  (ISP_MFB_BASE + 0x474)
#define MFBDMA_MFBI_STRIDE_REG                 (ISP_MFB_BASE + 0x478)
#define MFBDMA_MFBI_CON_REG                    (ISP_MFB_BASE + 0x47C)
#define MFBDMA_MFBI_CON2_REG                   (ISP_MFB_BASE + 0x480)
#define MFBDMA_MFBI_CON3_REG                   (ISP_MFB_BASE + 0x484)
#define MFBDMA_MFB2I_BASE_ADDR_REG             (ISP_MFB_BASE + 0x488)
#define MFBDMA_MFB2I_OFST_ADDR_REG             (ISP_MFB_BASE + 0x48C)
#define MFBDMA_MFB2I_XSIZE_REG                 (ISP_MFB_BASE + 0x490)
#define MFBDMA_MFB2I_YSIZE_REG                 (ISP_MFB_BASE + 0x494)
#define MFBDMA_MFB2I_STRIDE_REG                (ISP_MFB_BASE + 0x498)
#define MFBDMA_MFB2I_CON_REG                   (ISP_MFB_BASE + 0x49C)
#define MFBDMA_MFB2I_CON2_REG                  (ISP_MFB_BASE + 0x4A0)
#define MFBDMA_MFB2I_CON3_REG                  (ISP_MFB_BASE + 0x4A4)
#define MFBDMA_MFB3I_BASE_ADDR_REG             (ISP_MFB_BASE + 0x4A8)
#define MFBDMA_MFB3I_OFST_ADDR_REG             (ISP_MFB_BASE + 0x4AC)
#define MFBDMA_MFB3I_XSIZE_REG                 (ISP_MFB_BASE + 0x4B0)
#define MFBDMA_MFB3I_YSIZE_REG                 (ISP_MFB_BASE + 0x4B4)
#define MFBDMA_MFB3I_STRIDE_REG                (ISP_MFB_BASE + 0x4B8)
#define MFBDMA_MFB3I_CON_REG                   (ISP_MFB_BASE + 0x4BC)
#define MFBDMA_MFB3I_CON2_REG                  (ISP_MFB_BASE + 0x4C0)
#define MFBDMA_MFB3I_CON3_REG                  (ISP_MFB_BASE + 0x4C4)
#define MFBDMA_MFB4I_BASE_ADDR_REG             (ISP_MFB_BASE + 0x4C8)
#define MFBDMA_MFB4I_OFST_ADDR_REG             (ISP_MFB_BASE + 0x4CC)
#define MFBDMA_MFB4I_XSIZE_REG                 (ISP_MFB_BASE + 0x4D0)
#define MFBDMA_MFB4I_YSIZE_REG                 (ISP_MFB_BASE + 0x4D4)
#define MFBDMA_MFB4I_STRIDE_REG                (ISP_MFB_BASE + 0x4D8)
#define MFBDMA_MFB4I_CON_REG                   (ISP_MFB_BASE + 0x4DC)
#define MFBDMA_MFB4I_CON2_REG                  (ISP_MFB_BASE + 0x4E0)
#define MFBDMA_MFB4I_CON3_REG                  (ISP_MFB_BASE + 0x4E4)
#define MFBDMA_DMA_ERR_CTRL_REG                (ISP_MFB_BASE + 0x4E8)
#define MFBDMA_MFBO_ERR_STAT_REG               (ISP_MFB_BASE + 0x4EC)
#define MFBDMA_MFB2O_ERR_STAT_REG              (ISP_MFB_BASE + 0x4F0)
#define MFBDMA_MFBO_B_ERR_STAT_REG             (ISP_MFB_BASE + 0x4F4)
#define MFBDMA_MFBI_ERR_STAT_REG               (ISP_MFB_BASE + 0x4F8)
#define MFBDMA_MFB2I_ERR_STAT_REG              (ISP_MFB_BASE + 0x4FC)
#define MFBDMA_MFB3I_ERR_STAT_REG              (ISP_MFB_BASE + 0x500)
#define MFBDMA_MFB4I_ERR_STAT_REG              (ISP_MFB_BASE + 0x504)
#define MFBDMA_MFBI_B_ERR_STAT_REG             (ISP_MFB_BASE + 0x508)
#define MFBDMA_MFB2I_B_ERR_STAT_REG            (ISP_MFB_BASE + 0x50C)
#define MFBDMA_DMA_DEBUG_ADDR_REG              (ISP_MFB_BASE + 0x510)
#define MFBDMA_DMA_RSV1_REG                    (ISP_MFB_BASE + 0x514)
#define MFBDMA_DMA_RSV2_REG                    (ISP_MFB_BASE + 0x518)
#define MFBDMA_DMA_RSV3_REG                    (ISP_MFB_BASE + 0x51C)
#define MFBDMA_DMA_RSV4_REG                    (ISP_MFB_BASE + 0x520)
#define MFBDMA_DMA_DEBUG_SEL_REG               (ISP_MFB_BASE + 0x524)
#define MFBDMA_DMA_BW_SELF_TEST_REG            (ISP_MFB_BASE + 0x528)
#define MFBDMA_MFBO_B_BASE_ADDR_REG            (ISP_MFB_BASE + 0x52C)
#define MFBDMA_MFBO_B_OFST_ADDR_REG            (ISP_MFB_BASE + 0x530)
#define MFBDMA_MFBO_B_XSIZE_REG                (ISP_MFB_BASE + 0x534)
#define MFBDMA_MFBO_B_YSIZE_REG                (ISP_MFB_BASE + 0x538)
#define MFBDMA_MFBO_B_STRIDE_REG               (ISP_MFB_BASE + 0x53C)
#define MFBDMA_MFBO_B_CON_REG                  (ISP_MFB_BASE + 0x540)
#define MFBDMA_MFBO_B_CON2_REG                 (ISP_MFB_BASE + 0x544)
#define MFBDMA_MFBO_B_CON3_REG                 (ISP_MFB_BASE + 0x548)
#define MFBDMA_MFBO_B_CROP_REG                 (ISP_MFB_BASE + 0x54C)
#define MFBDMA_MFBI_B_BASE_ADDR_REG            (ISP_MFB_BASE + 0x550)
#define MFBDMA_MFBI_B_OFST_ADDR_REG            (ISP_MFB_BASE + 0x554)
#define MFBDMA_MFBI_B_XSIZE_REG                (ISP_MFB_BASE + 0x558)
#define MFBDMA_MFBI_B_YSIZE_REG                (ISP_MFB_BASE + 0x55C)
#define MFBDMA_MFBI_B_STRIDE_REG               (ISP_MFB_BASE + 0x560)
#define MFBDMA_MFBI_B_CON_REG                  (ISP_MFB_BASE + 0x564)
#define MFBDMA_MFBI_B_CON2_REG                 (ISP_MFB_BASE + 0x568)
#define MFBDMA_MFBI_B_CON3_REG                 (ISP_MFB_BASE + 0x56C)
#define MFBDMA_MFB2I_B_BASE_ADDR_REG           (ISP_MFB_BASE + 0x570)
#define MFBDMA_MFB2I_B_OFST_ADDR_REG           (ISP_MFB_BASE + 0x574)
#define MFBDMA_MFB2I_B_XSIZE_REG               (ISP_MFB_BASE + 0x578)
#define MFBDMA_MFB2I_B_YSIZE_REG               (ISP_MFB_BASE + 0x57C)
#define MFBDMA_MFB2I_B_STRIDE_REG              (ISP_MFB_BASE + 0x580)
#define MFBDMA_MFB2I_B_CON_REG                 (ISP_MFB_BASE + 0x584)
#define MFBDMA_MFB2I_B_CON2_REG                (ISP_MFB_BASE + 0x588)
#define MFBDMA_MFB2I_B_CON3_REG                (ISP_MFB_BASE + 0x58C)

#define PAK_CONT_Y_REG                         (ISP_MFB_BASE + 0x800)
#define PAK_CONT_C_REG                         (ISP_MFB_BASE + 0x810)
#define UNP_OFST_Y_REG                         (ISP_MFB_BASE + 0x820)
#define UNP_CONT_Y_REG                         (ISP_MFB_BASE + 0x824)
#define UNP_OFST_C_REG                         (ISP_MFB_BASE + 0x830)
#define UNP_CONT_C_REG                         (ISP_MFB_BASE + 0x834)


/**************************************************************
 *
 **************************************************************/
static inline unsigned int MFB_MsToJiffies(unsigned int Ms)
{
	return ((Ms * HZ + 512) >> 10);
}

/**************************************************************
 *
 **************************************************************/
static inline unsigned int MFB_UsToJiffies(unsigned int Us)
{
	return (((Us / 1000) * HZ + 512) >> 10);
}

/**************************************************************
 *
 **************************************************************/
static inline unsigned int MFB_GetIRQState(
	unsigned int type,
	unsigned int userNumber,
	unsigned int stus,
	enum MFB_PROCESS_ID_ENUM whichReq, int ProcessID)
{
	unsigned int ret = 0;
	unsigned long flags; /* old: unsigned int flags;*/

	/*  */
	spin_lock_irqsave(&(MFBInfo.SpinLockIrq[type]), flags);
#ifdef MFB_USE_GCE

#ifdef MFB_MULTIPROCESS_TIMING_ISSUE
	if (stus & MFB_INT_ST) {
		ret = ((MFBInfo.IrqInfo.MfbIrqCnt > 0)
		&& (MFBInfo.ProcessID[MFBInfo.ReadReqIdx] == ProcessID));
	} else {
		log_err(
"WaitIRQ Err,type:%d,usrNum:%d,sta:%d,Req:%d,ProcID:0x%x,RdIdx:%d\n",
		type, userNumber, stus,
		whichReq, ProcessID, MFBInfo.ReadReqIdx);
	}

#else
	if (stus & MFB_INT_ST) {
		ret = ((MFBInfo.IrqInfo.MfbIrqCnt > 0)
		&& (MFBInfo.IrqInfo.ProcessID[whichReq] == ProcessID));
	} else {
		log_err
		("WaitIRQ Err,type:%d,usrNum:%d,sta:%d,Req:%d,ProcID:0x%x\n",
		type,
		userNumber,
		stus,
		whichReq,
		ProcessID);
	}
#endif
#else
	ret = ((MFBInfo.IrqInfo.Status[type] & stus)
		   && (MFBInfo.IrqInfo.ProcessID[whichReq] == ProcessID));
#endif
	spin_unlock_irqrestore(&(MFBInfo.SpinLockIrq[type]), flags);
	/*  */
	return ret;
}


/**************************************************************
 *
 **************************************************************/
static inline unsigned int MFB_JiffiesToMs(unsigned int Jiffies)
{
	return ((Jiffies * 1000) / HZ);
}


#define RegDump(start, end) {\
	unsigned int i;\
	for (i = start; i <= end; i += 0x10) {\
		log_dbg(\
		"[0x%08X %08X],[0x%08X %08X],[0x%08X %08X],[0x%08X %08X]",\
		(unsigned int)(ISP_MFB_BASE + i),\
		(unsigned int)MFB_RD32(ISP_MFB_BASE + i),\
		(unsigned int)(ISP_MFB_BASE + i+0x4),\
		(unsigned int)MFB_RD32(ISP_MFB_BASE + i+0x4),\
		(unsigned int)(ISP_MFB_BASE + i+0x8),\
		(unsigned int)MFB_RD32(ISP_MFB_BASE + i+0x8),\
		(unsigned int)(ISP_MFB_BASE + i+0xc),\
		(unsigned int)MFB_RD32(ISP_MFB_BASE + i+0xc));\
	} \
}

#ifdef MFB_PMQOS
void MFBQOS_Init(void)
{
	s32 result = 0;
	u64 img_freq_steps[MAX_FREQ_STEP];
	u32 step_size;

	/* Call pm_qos_add_request when initialize module or driver prob */
	pm_qos_add_request(
		&mfb_qos_request,
		PM_QOS_IMG_FREQ,
		PM_QOS_MM_FREQ_DEFAULT_VALUE);

	/* Call mmdvfs_qos_get_freq_steps to get supported frequency */
	result = mmdvfs_qos_get_freq_steps(
		PM_QOS_IMG_FREQ,
		img_freq_steps,
		&step_size);

	if (result < 0 || step_size == 0)
		log_inf("get MMDVFS freq steps failed, result: %d\n", result);
	else
		max_img_freq = img_freq_steps[0];
}

void MFBQOS_Uninit(void)
{
	pm_qos_remove_request(&mfb_qos_request);
}

void MFBQOS_UpdateImgFreq(bool start)
{
	if (start) /* start MFB, configure MMDVFS to highest CLK */
		pm_qos_update_request(&mfb_qos_request, max_img_freq);
	else /* finish MFB, config MMDVFS to lowest CLK */
		pm_qos_update_request(&mfb_qos_request, 0);
}
#endif

void MFB_DumpUserSpaceReg(MFB_Config *pMfbConfig)
{
	log_inf("MFB Config From User Space\n");
	log_inf("C02_CON = 0x%08X\n", pMfbConfig->C02_CON);
	log_inf("C02_CROP_CON1 = 0x%08X\n", pMfbConfig->C02_CROP_CON1);
	log_inf("C02_CROP_CON2 = 0x%08X\n\n", pMfbConfig->C02_CROP_CON2);

	log_inf("SRZ_CONTROL = 0x%08X\n",
		pMfbConfig->SRZ_CONTROL);
	log_inf("SRZ_IN_IMG = 0x%08X\n",
		pMfbConfig->SRZ_IN_IMG);
	log_inf("SRZ_OUT_IMG = 0x%08X\n",
		pMfbConfig->SRZ_OUT_IMG);
	log_inf("SRZ_HORI_STEP = 0x%08X\n",
		pMfbConfig->SRZ_HORI_STEP);
	log_inf("SRZ_VERT_STEP = 0x%08X\n",
		pMfbConfig->SRZ_VERT_STEP);
	log_inf("SRZ_HORI_INT_OFST = 0x%08X\n",
		pMfbConfig->SRZ_HORI_INT_OFST);
	log_inf("SRZ_HORI_SUB_OFST = 0x%08X\n",
		pMfbConfig->SRZ_HORI_SUB_OFST);
	log_inf("SRZ_VERT_INT_OFST = 0x%08X\n",
		pMfbConfig->SRZ_VERT_INT_OFST);
	log_inf("SRZ_VERT_SUB_OFST = 0x%08X\n\n",
		pMfbConfig->SRZ_VERT_SUB_OFST);

	log_inf("CRSP_CTRL = 0x%08X\n",
		pMfbConfig->CRSP_CTRL);
	log_inf("CRSP_OUT_IMG = 0x%08X\n",
		pMfbConfig->CRSP_OUT_IMG);
	log_inf("CRSP_STEP_OFST = 0x%08X\n",
		pMfbConfig->CRSP_STEP_OFST);
	log_inf("CRSP_CROP_X = 0x%08X\n",
		pMfbConfig->CRSP_CROP_X);
	log_inf("CRSP_CROP_Y = 0x%08X\n\n",
		pMfbConfig->CRSP_CROP_Y);

	log_inf("OMC_TOP = 0x%08X\n",
		pMfbConfig->OMC_TOP);
	log_inf("OMC_ATPG = 0x%08X\n",
		pMfbConfig->OMC_ATPG);
	log_inf("OMC_FRAME_SIZE = 0x%08X\n",
		pMfbConfig->OMC_FRAME_SIZE);
	log_inf("OMC_TILE_EDGE = 0x%08X\n",
		pMfbConfig->OMC_TILE_EDGE);
	log_inf("OMC_TILE_OFS = 0x%08X\n",
		pMfbConfig->OMC_TILE_OFS);
	log_inf("OMC_TILE_SIZE = 0x%08X\n",
		pMfbConfig->OMC_TILE_SIZE);
	log_inf("OMC_TILE_CROP_X = 0x%08X\n",
		pMfbConfig->OMC_TILE_CROP_X);
	log_inf("OMC_TILE_CROP_Y = 0x%08X\n",
		pMfbConfig->OMC_TILE_CROP_Y);
	log_inf("OMC_MV_RDMA_BASE_ADDR = 0x%08X\n",
		pMfbConfig->OMC_MV_RDMA_BASE_ADDR);
	log_inf("OMC_MV_RDMA_STRIDE = 0x%08X\n\n",
		pMfbConfig->OMC_MV_RDMA_STRIDE);

	log_inf("OMCC_OMC_C_CFIFO_CTL = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_CFIFO_CTL);
	log_inf("OMCC_OMC_C_RWCTL_CTL = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_RWCTL_CTL);
	log_inf("OMCC_OMC_C_CACHI_SPECIAL_FUN_EN = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_CACHI_SPECIAL_FUN_EN);
	log_inf("OMCC_OMC_C_ADDR_GEN_BASE_ADDR_0 = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_ADDR_GEN_BASE_ADDR_0);
	log_inf("OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_0 = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_0);
	log_inf("OMCC_OMC_C_ADDR_GEN_STRIDE_0 = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_ADDR_GEN_STRIDE_0);
	log_inf("OMCC_OMC_C_ADDR_GEN_BASE_ADDR_1 = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_ADDR_GEN_BASE_ADDR_1);
	log_inf("OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_1 = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_1);
	log_inf("OMCC_OMC_C_ADDR_GEN_STRIDE_1 = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_ADDR_GEN_STRIDE_1);
	log_inf("OMCC_OMC_C_CACHI_CON2_0 = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_CACHI_CON2_0);
	log_inf("OMCC_OMC_C_CACHI_CON3_0 = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_CACHI_CON3_0);
	log_inf("OMCC_OMC_C_CTL_SW_CTL = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_CTL_SW_CTL);
	log_inf("OMCC_OMC_C_CTL_CFG = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_CTL_CFG);
	log_inf("OMCC_OMC_C_CTL_FMT_SEL = 0x%08X\n",
		pMfbConfig->OMCC_OMC_C_CTL_FMT_SEL);
	log_inf("OMCC_OMC_C_CTL_RSV0 = 0x%08X\n\n",
		pMfbConfig->OMCC_OMC_C_CTL_RSV0);

	log_inf("MFB_CON = 0x%08X\n", pMfbConfig->MFB_CON);
	log_inf("MFB_LL_CON1 = 0x%08X\n", pMfbConfig->MFB_LL_CON1);
	log_inf("MFB_LL_CON2 = 0x%08X\n", pMfbConfig->MFB_LL_CON2);
	log_inf("MFB_EDGE = 0x%08X\n", pMfbConfig->MFB_EDGE);
	log_inf("MFB_LL_CON5 = 0x%08X\n", pMfbConfig->MFB_LL_CON5);
	log_inf("MFB_LL_CON6 = 0x%08X\n", pMfbConfig->MFB_LL_CON6);
	log_inf("MFB_LL_CON7 = 0x%08X\n", pMfbConfig->MFB_LL_CON7);
	log_inf("MFB_LL_CON8 = 0x%08X\n", pMfbConfig->MFB_LL_CON8);
	log_inf("MFB_LL_CON9 = 0x%08X\n", pMfbConfig->MFB_LL_CON9);
	log_inf("MFB_LL_CON10 = 0x%08X\n", pMfbConfig->MFB_LL_CON10);
	log_inf("MFB_MBD_CON0 = 0x%08X\n", pMfbConfig->MFB_MBD_CON0);
	log_inf("MFB_MBD_CON1 = 0x%08X\n", pMfbConfig->MFB_MBD_CON1);
	log_inf("MFB_MBD_CON2 = 0x%08X\n", pMfbConfig->MFB_MBD_CON2);
	log_inf("MFB_MBD_CON3 = 0x%08X\n", pMfbConfig->MFB_MBD_CON3);
	log_inf("MFB_MBD_CON4 = 0x%08X\n", pMfbConfig->MFB_MBD_CON4);
	log_inf("MFB_MBD_CON5 = 0x%08X\n", pMfbConfig->MFB_MBD_CON5);
	log_inf("MFB_MBD_CON6 = 0x%08X\n", pMfbConfig->MFB_MBD_CON6);
	log_inf("MFB_MBD_CON7 = 0x%08X\n", pMfbConfig->MFB_MBD_CON7);
	log_inf("MFB_MBD_CON8 = 0x%08X\n", pMfbConfig->MFB_MBD_CON8);
	log_inf("MFB_MBD_CON9 = 0x%08X\n", pMfbConfig->MFB_MBD_CON9);
	log_inf("MFB_MBD_CON10 = 0x%08X\n\n", pMfbConfig->MFB_MBD_CON10);

	log_inf("MFB_MFB_TOP_CFG0 = 0x%08X\n", pMfbConfig->MFB_MFB_TOP_CFG0);
	log_inf("MFB_MFB_TOP_CFG1 = 0x%08X\n", pMfbConfig->MFB_MFB_TOP_CFG1);
	log_inf("MFB_MFB_TOP_CFG2 = 0x%08X\n\n", pMfbConfig->MFB_MFB_TOP_CFG2);

	log_inf("MFB_MFB_INT_CTL = 0x%08X\n",
		pMfbConfig->MFB_MFB_INT_CTL);
	log_inf("MFB_MFB_INT_STATUS = 0x%08X\n",
		pMfbConfig->MFB_MFB_INT_STATUS);
	log_inf("MFB_MFB_SW_RST = 0x%08X\n",
		pMfbConfig->MFB_MFB_SW_RST);
	log_inf("MFB_MFB_MAIN_DCM_ST = 0x%08X\n",
		pMfbConfig->MFB_MFB_MAIN_DCM_ST);
	log_inf("MFB_MFB_DMA_DCM_ST = 0x%08X\n",
		pMfbConfig->MFB_MFB_DMA_DCM_ST);
	log_inf("MFB_MFB_MAIN_DCM_DIS = 0x%08X\n",
		pMfbConfig->MFB_MFB_MAIN_DCM_DIS);
	log_inf("MFB_MFB_DBG_CTL0 = 0x%08X\n", pMfbConfig->MFB_MFB_DBG_CTL0);
	log_inf("MFB_MFB_DBG_CTL1 = 0x%08X\n", pMfbConfig->MFB_MFB_DBG_CTL1);
	log_inf("MFB_MFB_DBG_CTL2 = 0x%08X\n", pMfbConfig->MFB_MFB_DBG_CTL2);
	log_inf("MFB_MFB_DBG_OUT0 = 0x%08X\n", pMfbConfig->MFB_MFB_DBG_OUT0);
	log_inf("MFB_MFB_DBG_OUT1 = 0x%08X\n", pMfbConfig->MFB_MFB_DBG_OUT1);
	log_inf("MFB_MFB_DBG_OUT2 = 0x%08X\n", pMfbConfig->MFB_MFB_DBG_OUT2);
	log_inf("MFB_MFB_DBG_OUT3 = 0x%08X\n", pMfbConfig->MFB_MFB_DBG_OUT3);
	log_inf("MFB_MFB_DBG_OUT4 = 0x%08X\n", pMfbConfig->MFB_MFB_DBG_OUT4);
	log_inf("MFB_MFB_DBG_OUT5 = 0x%08X\n", pMfbConfig->MFB_MFB_DBG_OUT5);
	log_inf("MFB_DFTC = 0x%08X\n\n", pMfbConfig->MFB_DFTC);

	log_inf("MFBDMA_DMA_SOFT_RSTSTAT = 0x%08X\n",
		pMfbConfig->MFBDMA_DMA_SOFT_RSTSTAT);
	log_inf("MFBDMA_TDRI_BASE_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_TDRI_BASE_ADDR);
	log_inf("MFBDMA_TDRI_OFST_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_TDRI_OFST_ADDR);
	log_inf("MFBDMA_TDRI_XSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_TDRI_XSIZE);
	log_inf("MFBDMA_VERTICAL_FLIP_EN = 0x%08X\n",
		pMfbConfig->MFBDMA_VERTICAL_FLIP_EN);
	log_inf("MFBDMA_DMA_SOFT_RESET = 0x%08X\n",
		pMfbConfig->MFBDMA_DMA_SOFT_RESET);
	log_inf("MFBDMA_LAST_ULTRA_EN = 0x%08X\n",
		pMfbConfig->MFBDMA_LAST_ULTRA_EN);
	log_inf("MFBDMA_SPECIAL_FUN_EN = 0x%08X\n\n",
		pMfbConfig->MFBDMA_SPECIAL_FUN_EN);
	log_inf("MFBDMA_MFBO_BASE_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_BASE_ADDR);
	log_inf("MFBDMA_MFBO_OFST_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_OFST_ADDR);
	log_inf("MFBDMA_MFBO_XSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_XSIZE);
	log_inf("MFBDMA_MFBO_YSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_YSIZE);
	log_inf("MFBDMA_MFBO_STRIDE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_STRIDE);
	log_inf("MFBDMA_MFBO_CON = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_CON);
	log_inf("MFBDMA_MFBO_CON2 = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_CON2);
	log_inf("MFBDMA_MFBO_CON3 = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_CON3);
	log_inf("MFBDMA_MFBO_CROP = 0x%08X\n\n",
		pMfbConfig->MFBDMA_MFBO_CROP);
	log_inf("MFBDMA_MFB2O_BASE_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2O_BASE_ADDR);
	log_inf("MFBDMA_MFB2O_OFST_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2O_OFST_ADDR);
	log_inf("MFBDMA_MFB2O_XSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2O_XSIZE);
	log_inf("MFBDMA_MFB2O_YSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2O_YSIZE);
	log_inf("MFBDMA_MFB2O_STRIDE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2O_STRIDE);
	log_inf("MFBDMA_MFB2O_CON = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2O_CON);
	log_inf("MFBDMA_MFB2O_CON2 = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2O_CON2);
	log_inf("MFBDMA_MFB2O_CON3 = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2O_CON3);
	log_inf("MFBDMA_MFB2O_CROP = 0x%08X\n\n",
		pMfbConfig->MFBDMA_MFB2O_CROP);
	log_inf("MFBDMA_MFBI_BASE_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_BASE_ADDR);
	log_inf("MFBDMA_MFBI_OFST_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_OFST_ADDR);
	log_inf("MFBDMA_MFBI_XSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_XSIZE);
	log_inf("MFBDMA_MFBI_YSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_YSIZE);
	log_inf("MFBDMA_MFBI_STRIDE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_STRIDE);
	log_inf("MFBDMA_MFBI_CON = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_CON);
	log_inf("MFBDMA_MFBI_CON2 = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_CON2);
	log_inf("MFBDMA_MFBI_CON3 = 0x%08X\n\n",
		pMfbConfig->MFBDMA_MFBI_CON3);
	log_inf("MFBDMA_MFB2I_BASE_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_BASE_ADDR);
	log_inf("MFBDMA_MFB2I_OFST_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_OFST_ADDR);
	log_inf("MFBDMA_MFB2I_XSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_XSIZE);
	log_inf("MFBDMA_MFB2I_YSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_YSIZE);
	log_inf("MFBDMA_MFB2I_STRIDE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_STRIDE);
	log_inf("MFBDMA_MFB2I_CON = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_CON);
	log_inf("MFBDMA_MFB2I_CON2 = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_CON2);
	log_inf("MFBDMA_MFB2I_CON3 = 0x%08X\n\n",
		pMfbConfig->MFBDMA_MFB2I_CON3);
	log_inf("MFBDMA_MFB3I_BASE_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB3I_BASE_ADDR);
	log_inf("MFBDMA_MFB3I_OFST_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB3I_OFST_ADDR);
	log_inf("MFBDMA_MFB3I_XSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB3I_XSIZE);
	log_inf("MFBDMA_MFB3I_YSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB3I_YSIZE);
	log_inf("MFBDMA_MFB3I_STRIDE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB3I_STRIDE);
	log_inf("MFBDMA_MFB3I_CON = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB3I_CON);
	log_inf("MFBDMA_MFB3I_CON2 = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB3I_CON2);
	log_inf("MFBDMA_MFB3I_CON3 = 0x%08X\n\n",
		pMfbConfig->MFBDMA_MFB3I_CON3);
	log_inf("MFBDMA_MFB4I_BASE_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB4I_BASE_ADDR);
	log_inf("MFBDMA_MFB4I_OFST_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB4I_OFST_ADDR);
	log_inf("MFBDMA_MFB4I_XSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB4I_XSIZE);
	log_inf("MFBDMA_MFB4I_YSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB4I_YSIZE);
	log_inf("MFBDMA_MFB4I_STRIDE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB4I_STRIDE);
	log_inf("MFBDMA_MFB4I_CON = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB4I_CON);
	log_inf("MFBDMA_MFB4I_CON2 = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB4I_CON2);
	log_inf("MFBDMA_MFB4I_CON3 = 0x%08X\n\n",
		pMfbConfig->MFBDMA_MFB4I_CON3);
	log_inf("MFBDMA_DMA_ERR_CTRL = 0x%08X\n",
		pMfbConfig->MFBDMA_DMA_ERR_CTRL);
	log_inf("MFBDMA_MFBO_ERR_STAT = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_ERR_STAT);
	log_inf("MFBDMA_MFB2O_ERR_STAT = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2O_ERR_STAT);
	log_inf("MFBDMA_MFBO_B_ERR_STAT = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_B_ERR_STAT);
	log_inf("MFBDMA_MFBI_ERR_STAT = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_ERR_STAT);
	log_inf("MFBDMA_MFB2I_ERR_STAT = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_ERR_STAT);
	log_inf("MFBDMA_MFB3I_ERR_STAT = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB3I_ERR_STAT);
	log_inf("MFBDMA_MFB4I_ERR_STAT = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB4I_ERR_STAT);
	log_inf("MFBDMA_MFBI_B_ERR_STAT = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_B_ERR_STAT);
	log_inf("MFBDMA_MFB2I_B_ERR_STAT = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_B_ERR_STAT);
	log_inf("MFBDMA_DMA_DEBUG_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_DMA_DEBUG_ADDR);
	log_inf("MFBDMA_DMA_RSV1 = 0x%08X\n",
		pMfbConfig->MFBDMA_DMA_RSV1);
	log_inf("MFBDMA_DMA_RSV2 = 0x%08X\n",
		pMfbConfig->MFBDMA_DMA_RSV2);
	log_inf("MFBDMA_DMA_RSV3 = 0x%08X\n",
		pMfbConfig->MFBDMA_DMA_RSV3);
	log_inf("MFBDMA_DMA_RSV4 = 0x%08X\n",
		pMfbConfig->MFBDMA_DMA_RSV4);
	log_inf("MFBDMA_DMA_DEBUG_SEL = 0x%08X\n",
		pMfbConfig->MFBDMA_DMA_DEBUG_SEL);
	log_inf("MFBDMA_DMA_BW_SELF_TEST = 0x%08X\n\n",
		pMfbConfig->MFBDMA_DMA_BW_SELF_TEST);
	log_inf("MFBDMA_MFBO_B_BASE_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_B_BASE_ADDR);
	log_inf("MFBDMA_MFBO_B_OFST_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_B_OFST_ADDR);
	log_inf("MFBDMA_MFBO_B_XSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_B_XSIZE);
	log_inf("MFBDMA_MFBO_B_YSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_B_YSIZE);
	log_inf("MFBDMA_MFBO_B_STRIDE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_B_STRIDE);
	log_inf("MFBDMA_MFBO_B_CON = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_B_CON);
	log_inf("MFBDMA_MFBO_B_CON2 = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_B_CON2);
	log_inf("MFBDMA_MFBO_B_CON3 = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBO_B_CON3);
	log_inf("MFBDMA_MFBO_B_CROP = 0x%08X\n\n",
		pMfbConfig->MFBDMA_MFBO_B_CROP);
	log_inf("MFBDMA_MFBI_B_BASE_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_B_BASE_ADDR);
	log_inf("MFBDMA_MFBI_B_OFST_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_B_OFST_ADDR);
	log_inf("MFBDMA_MFBI_B_XSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_B_XSIZE);
	log_inf("MFBDMA_MFBI_B_YSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_B_YSIZE);
	log_inf("MFBDMA_MFBI_B_STRIDE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_B_STRIDE);
	log_inf("MFBDMA_MFBI_B_CON = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_B_CON);
	log_inf("MFBDMA_MFBI_B_CON2 = 0x%08X\n",
		pMfbConfig->MFBDMA_MFBI_B_CON2);
	log_inf("MFBDMA_MFBI_B_CON3 = 0x%08X\n\n",
		pMfbConfig->MFBDMA_MFBI_B_CON3);
	log_inf("MFBDMA_MFB2I_B_BASE_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_B_BASE_ADDR);
	log_inf("MFBDMA_MFB2I_B_OFST_ADDR = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_B_OFST_ADDR);
	log_inf("MFBDMA_MFB2I_B_XSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_B_XSIZE);
	log_inf("MFBDMA_MFB2I_B_YSIZE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_B_YSIZE);
	log_inf("MFBDMA_MFB2I_B_STRIDE = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_B_STRIDE);
	log_inf("MFBDMA_MFB2I_B_CON = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_B_CON);
	log_inf("MFBDMA_MFB2I_B_CON2 = 0x%08X\n",
		pMfbConfig->MFBDMA_MFB2I_B_CON2);
	log_inf("MFBDMA_MFB2I_B_CON3 = 0x%08X\n\n",
		pMfbConfig->MFBDMA_MFB2I_B_CON3);
	log_inf("PAK_CONT_Y = 0x%08X\n", pMfbConfig->PAK_CONT_Y);
	log_inf("PAK_CONT_C = 0x%08X\n", pMfbConfig->PAK_CONT_C);
	log_inf("UNP_OFST_Y = 0x%08X\n", pMfbConfig->UNP_OFST_Y);
	log_inf("UNP_CONT_Y = 0x%08X\n", pMfbConfig->UNP_CONT_Y);
	log_inf("UNP_OFST_C = 0x%08X\n", pMfbConfig->UNP_OFST_C);
	log_inf("UNP_CONT_C = 0x%08X\n", pMfbConfig->UNP_CONT_C);
}

static bool ConfigMFBRequest(unsigned int ReqIdx)
{
#ifdef MFB_USE_GCE
	unsigned int j;
	unsigned long flags; /* old: unsigned int flags;*/

	spin_lock_irqsave(
		&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
		flags);
	if (g_MFB_ReqRing.MFBReq_Struct[ReqIdx].State ==
		MFB_REQUEST_STATE_PENDING) {
		g_MFB_ReqRing.MFBReq_Struct[ReqIdx].State =
			MFB_REQUEST_STATE_RUNNING;
		for (j = 0; j < _SUPPORT_MAX_MFB_FRAME_REQUEST_; j++) {
			if (MFB_FRAME_STATUS_ENQUE ==
			g_MFB_ReqRing
				.MFBReq_Struct[ReqIdx].MfbFrameStatus[j]) {
				g_MFB_ReqRing
				.MFBReq_Struct[ReqIdx].MfbFrameStatus[j] =
				MFB_FRAME_STATUS_RUNNING;
				spin_unlock_irqrestore(&
				(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
				flags);
				ConfigMFBHW(&g_MFB_ReqRing.MFBReq_Struct[ReqIdx]
				.MfbFrameConfig[j]);
			spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
			flags);
			}
		}
	} else {
		log_err("Config Request state machine error!!, ReqIdx:%d, State:%d\n",
		ReqIdx, g_MFB_ReqRing.MFBReq_Struct[ReqIdx].State);
	}
	spin_unlock_irqrestore(
		&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
		flags);

	return MTRUE;
#else
	log_err("Config Request don't support this mode.!!\n");
	return MFALSE;
#endif
}


static bool ConfigMFB(void)
{
#ifdef MFB_USE_GCE

	unsigned int i, j, k;
	unsigned long flags; /* old: unsigned int flags;*/

	spin_lock_irqsave(
		&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
		flags);
	for (k = 0; k < _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_; k++) {
		i = (g_MFB_ReqRing.HWProcessIdx + k) %
			_SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
		if (g_MFB_ReqRing.MFBReq_Struct[i].State ==
			MFB_REQUEST_STATE_PENDING) {
			g_MFB_ReqRing.MFBReq_Struct[i].State =
			MFB_REQUEST_STATE_RUNNING;
			for (j = 0; j < _SUPPORT_MAX_MFB_FRAME_REQUEST_; j++) {
				if (MFB_FRAME_STATUS_ENQUE ==
				g_MFB_ReqRing.MFBReq_Struct[i]
				.MfbFrameStatus[j]) {
			/* break; */
					g_MFB_ReqRing.MFBReq_Struct[i]
					.MfbFrameStatus[j] =
					MFB_FRAME_STATUS_RUNNING;
					spin_unlock_irqrestore(
					&(MFBInfo.SpinLockIrq
					[MFB_IRQ_TYPE_INT_MFB_ST]),
					flags);
					ConfigMFBHW(&g_MFB_ReqRing
					.MFBReq_Struct[i]
					.MfbFrameConfig[j]);
					spin_lock_irqsave(
					&(MFBInfo.SpinLockIrq
					[MFB_IRQ_TYPE_INT_MFB_ST]),
					flags);
				}
			}
			/* log_dbg("ConfigMFB idx j:%d\n",j); */
			if (j != _SUPPORT_MAX_MFB_FRAME_REQUEST_) {
				log_err
			("MFB Config Sta wrong!idx j(%d),HWProc(%d),Sta(%d)\n",
				j,
				g_MFB_ReqRing.HWProcessIdx,
				g_MFB_ReqRing.MFBReq_Struct[i].State);
				return MFALSE;
			}
		}
	}
	spin_unlock_irqrestore(
		&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
		flags);
	if (k == _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_)
		log_dbg("No any MFB Request in Ring!!\n");

	return MTRUE;


#else				/* #ifdef MFB_USE_GCE */

	unsigned int i, j, k;
	unsigned int flags;

	for (k = 0; k < _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_; k++) {
		i = (g_MFB_ReqRing.HWProcessIdx + k) %
			_SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
		if (g_MFB_ReqRing.MFBReq_Struct[i].State ==
			MFB_REQUEST_STATE_PENDING) {
			for (j = 0; j < _SUPPORT_MAX_MFB_FRAME_REQUEST_; j++) {
				if (MFB_FRAME_STATUS_ENQUE ==
				g_MFB_ReqRing.MFBReq_Struct[i]
				.MfbFrameStatus[j]) {
					break;
			}
			}
			log_dbg("Config MFB idx j:%d\n", j);
			if (j != _SUPPORT_MAX_MFB_FRAME_REQUEST_) {
				g_MFB_ReqRing.MFBReq_Struct[i]
				.MfbFrameStatus[j] =
				MFB_FRAME_STATUS_RUNNING;
				ConfigMFBHW(&g_MFB_ReqRing.MFBReq_Struct[i]
				.MfbFrameConfig[j]);
				return MTRUE;
			}
			/*else {*/
			log_err
			("MFB Config Sta wrong! HWProc(%d), Sta(%d)\n",
			g_MFB_ReqRing.HWProcessIdx,
			g_MFB_ReqRing.MFBReq_Struct[i].State);
			g_MFB_ReqRing.HWProcessIdx =
				(g_MFB_ReqRing.HWProcessIdx +
				 1) % _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
			/*}*/
		}
	}
	if (k == _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_)
		log_dbg("No any MFB Request in Ring!!\n");

	return MFALSE;

#endif	/* #ifdef MFB_USE_GCE */

}


static bool UpdateMFB(pid_t *ProcessID)
{
#ifdef MFB_USE_GCE
	unsigned int i, j, next_idx;
	bool bFinishRequest = MFALSE;

	for (i = g_MFB_ReqRing.HWProcessIdx;
		i < _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
		i++) {
		if (g_MFB_ReqRing.MFBReq_Struct[i].State ==
			MFB_REQUEST_STATE_RUNNING) {
			for (j = 0; j < _SUPPORT_MAX_MFB_FRAME_REQUEST_; j++) {
				if (MFB_FRAME_STATUS_RUNNING ==
				g_MFB_ReqRing.MFBReq_Struct[i]
				.MfbFrameStatus[j]) {
					break;
			}
			}
			log_dbg("Update MFB idx j:%d\n", j);
			if (j != _SUPPORT_MAX_MFB_FRAME_REQUEST_) {
				next_idx = j + 1;
			g_MFB_ReqRing.MFBReq_Struct[i].MfbFrameStatus[j] =
				MFB_FRAME_STATUS_FINISHED;
			if ((_SUPPORT_MAX_MFB_FRAME_REQUEST_ == (next_idx))
			|| ((_SUPPORT_MAX_MFB_FRAME_REQUEST_ > (next_idx))
				&& (MFB_FRAME_STATUS_EMPTY ==
					g_MFB_ReqRing.MFBReq_Struct[i]
					.MfbFrameStatus[next_idx]))) {
				bFinishRequest = MTRUE;
				(*ProcessID) =
				g_MFB_ReqRing.MFBReq_Struct[i].processID;
				g_MFB_ReqRing.MFBReq_Struct[i].State =
					MFB_REQUEST_STATE_FINISHED;
				g_MFB_ReqRing.HWProcessIdx =
				(g_MFB_ReqRing.HWProcessIdx +
				1) % _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
			log_dbg("Finish MFB Req i:%d, j:%d, HWProcIdx:%d\n",
				i, j, g_MFB_ReqRing.HWProcessIdx);
			} else {
				log_dbg("Finish MFB Frame i:%d, j:%d\n",
					i, j);
				log_dbg("HWProcIdx:%d\n",
					g_MFB_ReqRing.HWProcessIdx);
			}
			break;
			}
			/*else {*/
			IRQ_LOG_KEEPER(
			MFB_IRQ_TYPE_INT_MFB_ST,
			m_CurrentPPB,
			_LOG_ERR,
			"MFB Sta Machine wrong! HWProcessIdx(%d), State(%d)\n",
			g_MFB_ReqRing.HWProcessIdx,
			g_MFB_ReqRing.MFBReq_Struct[i].State);
			g_MFB_ReqRing.MFBReq_Struct[i].State =
				MFB_REQUEST_STATE_FINISHED;
			g_MFB_ReqRing.HWProcessIdx =
				(g_MFB_ReqRing.HWProcessIdx +
				1) % _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
			break;
			/*}*/
		}
	}

	return bFinishRequest;


#else				/* #ifdef MFB_USE_GCE */
	unsigned int i, j, next_idx;
	bool bFinishRequest = MFALSE;

	for (i = g_MFB_ReqRing.HWProcessIdx;
		i < _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
		i++) {
		if (g_MFB_ReqRing.MFBReq_Struct[i].State
				== MFB_REQUEST_STATE_PENDING) {
			for (j = 0;
				j < _SUPPORT_MAX_MFB_FRAME_REQUEST_;
				j++) {
				if (MFB_FRAME_STATUS_RUNNING ==
				g_MFB_ReqRing.MFBReq_Struct[i]
				.MfbFrameStatus[j]) {
					break;
			}
		}
		log_dbg("Update MFB idx j:%d\n", j);
		if (j != _SUPPORT_MAX_MFB_FRAME_REQUEST_) {
			next_idx = j + 1;
			g_MFB_ReqRing.MFBReq_Struct[i].MfbFrameStatus[j] =
				MFB_FRAME_STATUS_FINISHED;
			if ((_SUPPORT_MAX_MFB_FRAME_REQUEST_ == (next_idx))
			|| ((_SUPPORT_MAX_MFB_FRAME_REQUEST_ > (next_idx))
			&& (MFB_FRAME_STATUS_EMPTY ==
			g_MFB_ReqRing.MFBReq_Struct[i]
				.MfbFrameStatus[next_idx]))) {
				bFinishRequest = MTRUE;
				(*ProcessID) =
				g_MFB_ReqRing.MFBReq_Struct[i].processID;
				g_MFB_ReqRing.MFBReq_Struct[i].State =
					MFB_REQUEST_STATE_FINISHED;
				g_MFB_ReqRing.HWProcessIdx =
				(g_MFB_ReqRing.HWProcessIdx + 1) %
					_SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
		log_dbg("Finish MFB Request i:%d, j:%d, HWProcessIdx:%d\n",
			i, j, g_MFB_ReqRing.HWProcessIdx);
			} else {
				log_dbg("Finish MFB Frame i:%d, j:%d\n",
					i, j);
				log_dbg("HWProcIdx:%d\n",
					g_MFB_ReqRing.HWProcessIdx);
			}
			break;
		}
		/*else {*/
		IRQ_LOG_KEEPER(
		MFB_IRQ_TYPE_INT_MFB_ST,
		m_CurrentPPB,
		_LOG_ERR,
		"MFB State Machine is wrong! HWProcessIdx(%d), State(%d)\n",
		g_MFB_ReqRing.HWProcessIdx,
		g_MFB_ReqRing.MFBReq_Struct[i].State);
		g_MFB_ReqRing.MFBReq_Struct[i].State =
			MFB_REQUEST_STATE_FINISHED;
		g_MFB_ReqRing.HWProcessIdx =
		(g_MFB_ReqRing.HWProcessIdx + 1) %
			_SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
		break;
		/*}*/
	}
	}

	return bFinishRequest;

#endif				/* #ifdef MFB_USE_GCE */


}

static signed int ConfigMFBHW(MFB_Config *pMfbConfig)
#if !BYPASS_REG
{
#ifdef MFB_USE_GCE
		struct cmdqRecStruct *handle;
		uint64_t engineFlag = (uint64_t)(1LL << CMDQ_ENG_MFB);
#endif
	/* unsigned int tpipe_index; */

	if (MFB_DBG_DBGLOG == (MFB_DBG_DBGLOG & MFBInfo.DebugMask)) {
		log_dbg("ConfigMFBHW Start!\n");
		log_dbg("MFB_TOP_CFG0:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFB_TOP_CFG0);
		log_dbg("MFB_TOP_CFG2:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_MFB_TOP_CFG2);
		log_dbg("MFB_MFBI_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFBDMA_MFBI_BASE_ADDR);
		log_dbg("MFB_MFBI_B_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFBDMA_MFBI_B_BASE_ADDR);
		log_dbg("MFB_MFB2I_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFBDMA_MFB2I_BASE_ADDR);
		log_dbg("MFB_MFB2I_B_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFBDMA_MFB2I_B_BASE_ADDR);
		log_dbg("MFB_MFB3I_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFBDMA_MFB3I_BASE_ADDR);
		log_dbg("MFB_MFB4I_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFBDMA_MFB4I_BASE_ADDR);
		log_dbg("MFB_MFBO_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFBDMA_MFBO_BASE_ADDR);
		log_dbg("MFB_MFBO_B_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFBDMA_MFBO_B_BASE_ADDR);
		log_dbg("MFB_MFB2O_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFBDMA_MFB2O_BASE_ADDR);
		log_dbg("MFB_TDRI_ADDR:0x%x!\n",
			(unsigned int)pMfbConfig->MFBDMA_TDRI_BASE_ADDR);
		log_dbg("MFB_MFBI_STRIDE:0x%x!\n",
			(unsigned int)pMfbConfig->MFBDMA_MFBI_STRIDE);
		log_dbg("MFB_MFB2I_STRIDE:0x%x!\n",
			(unsigned int)pMfbConfig->MFBDMA_MFB2I_STRIDE);
		log_dbg("MFB_MFBO_STRIDE:0x%x!\n",
			(unsigned int)pMfbConfig->MFBDMA_MFBO_STRIDE);
		log_dbg("MFB_CON:0x%x!\n",
			(unsigned int)pMfbConfig->MFB_CON);
	}

	if (pMfbConfig->USERDUMP_EN == 1)
		MFB_DumpUserSpaceReg(pMfbConfig);

#ifdef MFB_USE_GCE

#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_begin("ConfigMFBHW");
#endif
	cmdqRecCreate(CMDQ_SCENARIO_KERNEL_CONFIG_GENERAL, &handle);
	/* CMDQ driver dispatches CMDQ HW thread and */
	/* HW thread's priority according to scenario */

	cmdqRecSetEngine(handle, engineFlag);

	cmdqRecReset(handle);
	/* Use command queue to write register */

	cmdqRecWrite(handle, C02_CON_HW,
		pMfbConfig->C02_CON, CMDQ_REG_MASK);

	cmdqRecWrite(handle, SRZ_CONTROL_HW,
		pMfbConfig->SRZ_CONTROL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_IN_IMG_HW,
		pMfbConfig->SRZ_IN_IMG, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_OUT_IMG_HW,
		pMfbConfig->SRZ_OUT_IMG, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_HORI_STEP_HW,
		pMfbConfig->SRZ_HORI_STEP, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_VERT_STEP_HW,
		pMfbConfig->SRZ_VERT_STEP, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_HORI_INT_OFST_HW,
		pMfbConfig->SRZ_HORI_INT_OFST, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_HORI_SUB_OFST_HW,
		pMfbConfig->SRZ_HORI_SUB_OFST, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_VERT_INT_OFST_HW,
		pMfbConfig->SRZ_VERT_INT_OFST, CMDQ_REG_MASK);
	cmdqRecWrite(handle, SRZ_VERT_SUB_OFST_HW,
		pMfbConfig->SRZ_VERT_SUB_OFST, CMDQ_REG_MASK);

	cmdqRecWrite(handle, CRSP_CTRL_HW,
		pMfbConfig->CRSP_CTRL, CMDQ_REG_MASK);
	cmdqRecWrite(handle, CRSP_OUT_IMG_HW,
		pMfbConfig->CRSP_OUT_IMG, CMDQ_REG_MASK);
	cmdqRecWrite(handle, CRSP_STEP_OFST_HW,
		pMfbConfig->CRSP_STEP_OFST, CMDQ_REG_MASK);

	cmdqRecWrite(handle, OMC_TOP_HW,
		pMfbConfig->OMC_TOP, CMDQ_REG_MASK);
	/*cmdqRecWrite(handle, OMC_ATPG_HW, */
	/*pMfbConfig->OMC_ATPG, CMDQ_REG_MASK);*/
	cmdqRecWrite(handle, OMC_FRAME_SIZE_HW,
		pMfbConfig->OMC_FRAME_SIZE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, OMC_TILE_EDGE_HW,
		pMfbConfig->OMC_TILE_EDGE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, OMC_TILE_OFS_HW,
		pMfbConfig->OMC_TILE_OFS, CMDQ_REG_MASK);
	cmdqRecWrite(handle, OMC_TILE_SIZE_HW,
		pMfbConfig->OMC_TILE_SIZE, CMDQ_REG_MASK);
	/*cmdqRecWrite(handle, OMC_TILE_CROP_X_HW,*/
	/*pMfbConfig->OMC_TILE_CROP_X, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, OMC_TILE_CROP_Y_HW,*/
	/*pMfbConfig->OMC_TILE_CROP_Y, CMDQ_REG_MASK);*/
	cmdqRecWrite(handle, OMC_MV_RDMA_BASE_ADDR_HW,
		pMfbConfig->OMC_MV_RDMA_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, OMC_MV_RDMA_STRIDE_HW,
		pMfbConfig->OMC_MV_RDMA_STRIDE, CMDQ_REG_MASK);
	/*cmdqRecWrite(handle, OMCC_OMC_C_CFIFO_CTL_HW,*/
	/*pMfbConfig->OMCC_OMC_C_CFIFO_CTL, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, OMCC_OMC_C_RWCTL_CTL_HW,*/
	/*pMfbConfig->OMCC_OMC_C_RWCTL_CTL, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, OMCC_OMC_C_CACHI_SPECIAL_FUN_EN_HW,*/
	/*pMfbConfig->OMCC_OMC_C_CACHI_SPECIAL_FUN_EN,*/
	/*CMDQ_REG_MASK);*/
	cmdqRecWrite(handle, OMCC_OMC_C_ADDR_GEN_BASE_ADDR_0_HW,
		pMfbConfig->OMCC_OMC_C_ADDR_GEN_BASE_ADDR_0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, OMCC_OMC_C_ADDR_GEN_STRIDE_0_HW,
		pMfbConfig->OMCC_OMC_C_ADDR_GEN_STRIDE_0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, OMCC_OMC_C_ADDR_GEN_BASE_ADDR_1_HW,
		pMfbConfig->OMCC_OMC_C_ADDR_GEN_BASE_ADDR_1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, OMCC_OMC_C_ADDR_GEN_STRIDE_1_HW,
		pMfbConfig->OMCC_OMC_C_ADDR_GEN_STRIDE_1, CMDQ_REG_MASK);

	/*cmdqRecWrite(handle, OMCC_OMC_C_CTL_SW_CTL_HW,*/
	/*pMfbConfig->OMCC_OMC_C_CTL_SW_CTL, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, OMCC_OMC_C_CTL_CFG_HW,*/
	/*pMfbConfig->OMCC_OMC_C_CTL_CFG, CMDQ_REG_MASK);*/
	cmdqRecWrite(handle, OMCC_OMC_C_CTL_FMT_SEL_HW,
		pMfbConfig->OMCC_OMC_C_CTL_FMT_SEL, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFB_CON_HW,
		pMfbConfig->MFB_CON, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON1_HW,
		pMfbConfig->MFB_LL_CON1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON2_HW,
		pMfbConfig->MFB_LL_CON2, CMDQ_REG_MASK);
	/*cmdqRecWrite(handle, MFB_LL_CON3_HW,*/
	/*pMfbConfig->MFB_LL_CON3, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFB_LL_CON4_HW,*/
	/*pMfbConfig->MFB_LL_CON4, CMDQ_REG_MASK);*/
	cmdqRecWrite(handle, MFB_EDGE_HW,
		pMfbConfig->MFB_EDGE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON5_HW,
		pMfbConfig->MFB_LL_CON5, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON6_HW,
		pMfbConfig->MFB_LL_CON6, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON7_HW,
		pMfbConfig->MFB_LL_CON7, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON8_HW,
		pMfbConfig->MFB_LL_CON8, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON9_HW,
		pMfbConfig->MFB_LL_CON9, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_LL_CON10_HW,
		pMfbConfig->MFB_LL_CON10, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON0_HW,
		pMfbConfig->MFB_MBD_CON0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON1_HW,
		pMfbConfig->MFB_MBD_CON1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON2_HW,
		pMfbConfig->MFB_MBD_CON2, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON3_HW,
		pMfbConfig->MFB_MBD_CON3, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON4_HW,
		pMfbConfig->MFB_MBD_CON4, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON5_HW,
		pMfbConfig->MFB_MBD_CON5, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON6_HW,
		pMfbConfig->MFB_MBD_CON6, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON7_HW,
		pMfbConfig->MFB_MBD_CON7, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON8_HW,
		pMfbConfig->MFB_MBD_CON8, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON9_HW,
		pMfbConfig->MFB_MBD_CON9, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MBD_CON10_HW,
		pMfbConfig->MFB_MBD_CON10, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFB_MFB_TOP_CFG0_HW,
		pMfbConfig->MFB_MFB_TOP_CFG0, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFB_MFB_TOP_CFG2_HW,
		pMfbConfig->MFB_MFB_TOP_CFG2, CMDQ_REG_MASK);

	/* BIT0 for INT_EN, BIT20 for CHROMA_INT_EN */
	/* BIT21 for WEIGHT_INT_EN */
	cmdqRecWrite(handle, MFB_MFB_INT_CTL_HW, 0x300001, CMDQ_REG_MASK);

	/*cmdqRecWrite(handle, MFB_MFB_MAIN_DCM_DIS_HW,*/
	/*pMfbConfig->MFB_MFB_MAIN_DCM_DIS, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFB_MFB_DBG_CTL0_HW,*/
	/*pMfbConfig->MFB_MFB_DBG_CTL0, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFB_MFB_DBG_CTL1_HW,*/
	/*pMfbConfig->MFB_MFB_DBG_CTL1, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFB_MFB_DBG_CTL2_HW,*/
	/*pMfbConfig->MFB_MFB_DBG_CTL2, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFB_DFTC_HW,*/
	/*pMfbConfig->MFB_DFTC, CMDQ_REG_MASK);*/

	cmdqRecWrite(handle, MFBDMA_TDRI_BASE_ADDR_HW,
		pMfbConfig->MFBDMA_TDRI_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_TDRI_XSIZE_HW,
		pMfbConfig->MFBDMA_TDRI_XSIZE, CMDQ_REG_MASK);
	/*cmdqRecWrite(handle, MFBDMA_VERTICAL_FLIP_EN_HW,*/
	/*pMfbConfig->MFBDMA_VERTICAL_FLIP_EN, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFBDMA_LAST_ULTRA_EN_HW,*/
	/*pMfbConfig->MFBDMA_LAST_ULTRA_EN, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFBDMA_SPECIAL_FUN_EN_HW,*/
	/*pMfbConfig->MFBDMA_SPECIAL_FUN_EN, CMDQ_REG_MASK);*/
	cmdqRecWrite(handle, MFBDMA_MFBO_BASE_ADDR_HW,
		pMfbConfig->MFBDMA_MFBO_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFBO_STRIDE_HW,
		pMfbConfig->MFBDMA_MFBO_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFBO_XSIZE_HW,
		(pMfbConfig->MFBDMA_MFBO_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFBO_YSIZE_HW,
		pMfbConfig->MFBDMA_MFBO_YSIZE, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFBDMA_MFB2O_BASE_ADDR_HW,
		pMfbConfig->MFBDMA_MFB2O_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFB2O_STRIDE_HW,
		pMfbConfig->MFBDMA_MFB2O_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFB2O_XSIZE_HW,
		(pMfbConfig->MFBDMA_MFB2O_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFB2O_YSIZE_HW,
		pMfbConfig->MFBDMA_MFB2O_YSIZE, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFBDMA_MFBI_BASE_ADDR_HW,
		pMfbConfig->MFBDMA_MFBI_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFBI_STRIDE_HW,
		pMfbConfig->MFBDMA_MFBI_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFBI_XSIZE_HW,
		(pMfbConfig->MFBDMA_MFBI_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFBI_YSIZE_HW,
		pMfbConfig->MFBDMA_MFBI_YSIZE, CMDQ_REG_MASK);

	/*cmdqRecWrite(handle, MFBDMA_MFB2I_BASE_ADDR_HW,*/
	/*pMfbConfig->MFBDMA_MFB2I_BASE_ADDR, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFBDMA_MFB2I_STRIDE_HW,*/
	/*pMfbConfig->MFBDMA_MFB2I_STRIDE, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFBDMA_MFB2I_XSIZE_HW,*/
	/*(pMfbConfig->MFBDMA_MFB2I_STRIDE & 0xFFFF)-1,*/
	/*CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFBDMA_MFB2I_YSIZE_HW,*/
	/*pMfbConfig->MFBDMA_MFB2I_YSIZE, CMDQ_REG_MASK);*/

	cmdqRecWrite(handle, MFBDMA_MFB3I_BASE_ADDR_HW,
		pMfbConfig->MFBDMA_MFB3I_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFB3I_STRIDE_HW,
		pMfbConfig->MFBDMA_MFB3I_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFB3I_XSIZE_HW,
		(pMfbConfig->MFBDMA_MFB3I_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFB3I_YSIZE_HW,
		pMfbConfig->MFBDMA_MFB3I_YSIZE, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFBDMA_MFB4I_BASE_ADDR_HW,
		pMfbConfig->MFBDMA_MFB4I_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFB4I_STRIDE_HW,
		pMfbConfig->MFBDMA_MFB4I_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFB4I_XSIZE_HW,
		(pMfbConfig->MFBDMA_MFB4I_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFB4I_YSIZE_HW,
		pMfbConfig->MFBDMA_MFB4I_YSIZE, CMDQ_REG_MASK);

	/*cmdqRecWrite(handle, MFBDMA_DMA_ERR_CTRL_HW,*/
	/*pMfbConfig->MFBDMA_DMA_ERR_CTRL, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFBDMA_DMA_DEBUG_ADDR_HW,*/
	/*pMfbConfig->MFBDMA_DMA_DEBUG_ADDR, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFBDMA_DMA_DEBUG_SEL_HW,*/
	/*pMfbConfig->MFBDMA_DMA_DEBUG_SEL, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFBDMA_DMA_BW_SELF_TEST_HW,*/
	/*pMfbConfig->MFBDMA_DMA_BW_SELF_TEST, CMDQ_REG_MASK);*/

	cmdqRecWrite(handle, MFBDMA_MFBO_B_BASE_ADDR_HW,
		pMfbConfig->MFBDMA_MFBO_B_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFBO_B_STRIDE_HW,
		pMfbConfig->MFBDMA_MFBO_B_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFBO_B_XSIZE_HW,
		(pMfbConfig->MFBDMA_MFBO_B_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFBO_B_YSIZE_HW,
		pMfbConfig->MFBDMA_MFBO_B_YSIZE, CMDQ_REG_MASK);

	cmdqRecWrite(handle, MFBDMA_MFBI_B_BASE_ADDR_HW,
		pMfbConfig->MFBDMA_MFBI_B_BASE_ADDR, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFBI_B_STRIDE_HW,
		pMfbConfig->MFBDMA_MFBI_B_STRIDE, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFBI_B_XSIZE_HW,
		(pMfbConfig->MFBDMA_MFBI_B_STRIDE & 0xFFFF)-1, CMDQ_REG_MASK);
	cmdqRecWrite(handle, MFBDMA_MFBI_B_YSIZE_HW,
		pMfbConfig->MFBDMA_MFBI_B_YSIZE, CMDQ_REG_MASK);

	/*cmdqRecWrite(handle, MFBDMA_MFB2I_B_BASE_ADDR_HW,*/
	/*pMfbConfig->MFBDMA_MFB2I_B_BASE_ADDR, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFBDMA_MFB2I_B_STRIDE_HW,*/
	/*pMfbConfig->MFBDMA_MFB2I_B_STRIDE, CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFBDMA_MFB2I_B_XSIZE_HW,*/
	/*(pMfbConfig->MFBDMA_MFB2I_B_STRIDE & 0xFFFF)-1,*/
	/*CMDQ_REG_MASK);*/
	/*cmdqRecWrite(handle, MFBDMA_MFB2I_B_YSIZE_HW,*/
	/*pMfbConfig->MFBDMA_MFB2I_B_YSIZE, CMDQ_REG_MASK);*/

	cmdqRecWrite(handle, PAK_CONT_Y_HW,
		pMfbConfig->PAK_CONT_Y, CMDQ_REG_MASK);
	cmdqRecWrite(handle, PAK_CONT_C_HW,
		pMfbConfig->PAK_CONT_C, CMDQ_REG_MASK);
	cmdqRecWrite(handle, UNP_OFST_Y_HW,
		pMfbConfig->UNP_OFST_Y, CMDQ_REG_MASK);
	cmdqRecWrite(handle, UNP_CONT_Y_HW,
		pMfbConfig->UNP_CONT_Y, CMDQ_REG_MASK);
	cmdqRecWrite(handle, UNP_OFST_C_HW,
		pMfbConfig->UNP_OFST_C, CMDQ_REG_MASK);
	cmdqRecWrite(handle, UNP_CONT_C_HW,
		pMfbConfig->UNP_CONT_C, CMDQ_REG_MASK);

	/* Disable MFB DCM if necessary */
	/* cmdqRecWrite(handle, MFB_MFB_MAIN_DCM_DIS_HW,*/
	/* 0xFFFFFFFF, CMDQ_REG_MASK); */

	/* for (tpipe_index = 0; */
	/*	tpipe_index < pMfbConfig->TPIPE_NO; */
	/*	tpipe_index ++) { */
	/*	log_inf("TRIGGER TILE: %d\n", tpipe_index); */
	/*	cmdqRecWrite(handle, MFBDMA_TDRI_BASE_ADDR_HW, */
	/*	pMfbConfig->MFBDMA_TDRI_BASE_ADDR + 40 * tpipe_index,*/
	/*	CMDQ_REG_MASK); */
	cmdqRecWrite(handle, MFB_MFB_TOP_CFG1_HW, 0x1, CMDQ_REG_MASK);
	cmdqRecWait(handle, CMDQ_EVENT_MFB_DONE);
	cmdqRecWrite(handle, MFB_MFB_TOP_CFG1_HW, 0x0, CMDQ_REG_MASK);
	/*}*/

#ifdef MFB_PMQOS
	MFBQOS_UpdateImgFreq(1);
#endif
	/* non-blocking API, Please  use cmdqRecFlushAsync() */
	cmdqRecFlushAsync(handle);
	/* if you want to re-use the handle, please reset the handle */
	cmdqRecReset(handle);
	cmdqRecDestroy(handle);	/* recycle the memory */

#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_end();
#endif

#else

#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_begin("ConfigMFBHW");
#endif

#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
	mt_kernel_trace_end();
#endif

#endif
	return 0;
}
#else
{
	return 0;
}
#endif

#define MFB_IS_BUSY	0x2

#ifndef MFB_USE_GCE

static bool Check_MFB_Is_Busy(void)
#if !BYPASS_REG
{
	unsigned int Ctrl _Fsm;
	unsigned int Mfb_Start;

	Ctrl_Fsm = MFB_RD32(MFB_DBG_INFO_00_REG);
	Mfb_Start = MFB_RD32(MFB_START_REG);
	if ((MFB_IS_BUSY == (Ctrl_Fsm & MFB_IS_BUSY)) ||
		(MFB_START == (Mfb_Start & MFB_START)))
		return MTRUE;

	return MFALSE;
}
#else
{
	return MFALSE;
}
#endif
#endif


/*
 *
 */
static signed int MFB_DumpReg(void)
{
	signed int Ret = 0;
	unsigned int i, j;
	/* unsigned int* tdri_base; */

	log_inf("- E.");

	log_inf("MFB Config Info\n");

	log_inf("[0x%08X %08X]\n", (unsigned int)(C02_CON_HW),
		(unsigned int)MFB_RD32(C02_CON_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(C02_CROP_CON1_HW),
		(unsigned int)MFB_RD32(C02_CROP_CON1_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(C02_CROP_CON2_HW),
		(unsigned int)MFB_RD32(C02_CROP_CON2_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(SRZ_CONTROL_HW),
		(unsigned int)MFB_RD32(SRZ_CONTROL_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(SRZ_IN_IMG_HW),
		(unsigned int)MFB_RD32(SRZ_IN_IMG_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(SRZ_OUT_IMG_HW),
		(unsigned int)MFB_RD32(SRZ_OUT_IMG_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(SRZ_HORI_STEP_HW),
		(unsigned int)MFB_RD32(SRZ_HORI_STEP_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(SRZ_VERT_STEP_HW),
		(unsigned int)MFB_RD32(SRZ_VERT_STEP_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(SRZ_HORI_INT_OFST_HW),
		(unsigned int)MFB_RD32(SRZ_HORI_INT_OFST_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(SRZ_HORI_SUB_OFST_HW),
		(unsigned int)MFB_RD32(SRZ_HORI_SUB_OFST_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(SRZ_VERT_INT_OFST_HW),
		(unsigned int)MFB_RD32(SRZ_VERT_INT_OFST_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(SRZ_VERT_SUB_OFST_HW),
		(unsigned int)MFB_RD32(SRZ_VERT_SUB_OFST_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(CRSP_CTRL_HW),
		(unsigned int)MFB_RD32(CRSP_CTRL_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(CRSP_OUT_IMG_HW),
		(unsigned int)MFB_RD32(CRSP_OUT_IMG_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(CRSP_STEP_OFST_HW),
		(unsigned int)MFB_RD32(CRSP_STEP_OFST_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(CRSP_CROP_X_HW),
		(unsigned int)MFB_RD32(CRSP_CROP_X_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(CRSP_CROP_Y_HW),
		(unsigned int)MFB_RD32(CRSP_CROP_Y_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(OMC_TOP_HW),
		(unsigned int)MFB_RD32(OMC_TOP_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMC_ATPG_HW),
		(unsigned int)MFB_RD32(OMC_ATPG_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMC_FRAME_SIZE_HW),
		(unsigned int)MFB_RD32(OMC_FRAME_SIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMC_TILE_EDGE_HW),
		(unsigned int)MFB_RD32(OMC_TILE_EDGE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMC_TILE_OFS_HW),
		(unsigned int)MFB_RD32(OMC_TILE_OFS_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMC_TILE_SIZE_HW),
		(unsigned int)MFB_RD32(OMC_TILE_SIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMC_TILE_CROP_X_HW),
		(unsigned int)MFB_RD32(OMC_TILE_CROP_X_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMC_TILE_CROP_Y_HW),
		(unsigned int)MFB_RD32(OMC_TILE_CROP_Y_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMC_MV_RDMA_BASE_ADDR_HW),
		(unsigned int)MFB_RD32(OMC_MV_RDMA_BASE_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMC_MV_RDMA_STRIDE_HW),
		(unsigned int)MFB_RD32(OMC_MV_RDMA_STRIDE_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(OMCC_OMC_C_CFIFO_CTL_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_CFIFO_CTL_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMCC_OMC_C_RWCTL_CTL_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_RWCTL_CTL_REG));
	log_inf("[0x%08X %08X]\n",
		(unsigned int)(OMCC_OMC_C_CACHI_SPECIAL_FUN_EN_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_CACHI_SPECIAL_FUN_EN_REG));
	log_inf("[0x%08X %08X]\n",
		(unsigned int)(OMCC_OMC_C_ADDR_GEN_BASE_ADDR_0_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_ADDR_GEN_BASE_ADDR_0_REG));
	log_inf("[0x%08X %08X]\n",
		(unsigned int)(OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_0_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_0_REG));
	log_inf("[0x%08X %08X]\n",
		(unsigned int)(OMCC_OMC_C_ADDR_GEN_STRIDE_0_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_ADDR_GEN_STRIDE_0_REG));
	log_inf("[0x%08X %08X]\n",
		(unsigned int)(OMCC_OMC_C_ADDR_GEN_BASE_ADDR_1_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_ADDR_GEN_BASE_ADDR_1_REG));
	log_inf("[0x%08X %08X]\n",
		(unsigned int)(OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_1_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_ADDR_GEN_OFFSET_ADDR_1_REG));
	log_inf("[0x%08X %08X]\n",
		(unsigned int)(OMCC_OMC_C_ADDR_GEN_STRIDE_1_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_ADDR_GEN_STRIDE_1_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMCC_OMC_C_CACHI_CON2_0_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_CACHI_CON2_0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMCC_OMC_C_CACHI_CON3_0_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_CACHI_CON3_0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMCC_OMC_C_CTL_SW_CTL_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_CTL_SW_CTL_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMCC_OMC_C_CTL_CFG_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_CTL_CFG_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMCC_OMC_C_CTL_FMT_SEL_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_CTL_FMT_SEL_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(OMCC_OMC_C_CTL_RSV0_HW),
		(unsigned int)MFB_RD32(OMCC_OMC_C_CTL_RSV0_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_CON_HW),
		(unsigned int)MFB_RD32(MFB_CON_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_LL_CON1_HW),
		(unsigned int)MFB_RD32(MFB_LL_CON1_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_LL_CON2_HW),
		(unsigned int)MFB_RD32(MFB_LL_CON2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_EDGE_HW),
		(unsigned int)MFB_RD32(MFB_EDGE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_LL_CON5_HW),
		(unsigned int)MFB_RD32(MFB_LL_CON5_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_LL_CON6_HW),
		(unsigned int)MFB_RD32(MFB_LL_CON6_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_LL_CON7_HW),
		(unsigned int)MFB_RD32(MFB_LL_CON7_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_LL_CON8_HW),
		(unsigned int)MFB_RD32(MFB_LL_CON8_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_LL_CON9_HW),
		(unsigned int)MFB_RD32(MFB_LL_CON9_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_LL_CON10_HW),
		(unsigned int)MFB_RD32(MFB_LL_CON10_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MBD_CON0_HW),
		(unsigned int)MFB_RD32(MFB_MBD_CON0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MBD_CON1_HW),
		(unsigned int)MFB_RD32(MFB_MBD_CON1_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MBD_CON2_HW),
		(unsigned int)MFB_RD32(MFB_MBD_CON2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MBD_CON3_HW),
		(unsigned int)MFB_RD32(MFB_MBD_CON3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MBD_CON4_HW),
		(unsigned int)MFB_RD32(MFB_MBD_CON4_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MBD_CON5_HW),
		(unsigned int)MFB_RD32(MFB_MBD_CON5_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MBD_CON6_HW),
		(unsigned int)MFB_RD32(MFB_MBD_CON6_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MBD_CON7_HW),
		(unsigned int)MFB_RD32(MFB_MBD_CON7_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MBD_CON8_HW),
		(unsigned int)MFB_RD32(MFB_MBD_CON8_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MBD_CON9_HW),
		(unsigned int)MFB_RD32(MFB_MBD_CON9_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MBD_CON10_HW),
		(unsigned int)MFB_RD32(MFB_MBD_CON10_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_TOP_CFG0_HW),
		(unsigned int)MFB_RD32(MFB_MFB_TOP_CFG0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_TOP_CFG1_HW),
		(unsigned int)MFB_RD32(MFB_MFB_TOP_CFG1_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_TOP_CFG2_HW),
		(unsigned int)MFB_RD32(MFB_MFB_TOP_CFG2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_INT_CTL_HW),
		(unsigned int)MFB_RD32(MFB_MFB_INT_CTL_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_INT_STATUS_HW),
		(unsigned int)MFB_RD32(MFB_MFB_INT_STATUS_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_SW_RST_HW),
		(unsigned int)MFB_RD32(MFB_MFB_SW_RST_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_MAIN_DCM_ST_HW),
		(unsigned int)MFB_RD32(MFB_MFB_MAIN_DCM_ST_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_DMA_DCM_ST_HW),
		(unsigned int)MFB_RD32(MFB_MFB_DMA_DCM_ST_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_MAIN_DCM_DIS_HW),
		(unsigned int)MFB_RD32(MFB_MFB_MAIN_DCM_DIS_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_DBG_CTL0_HW),
		(unsigned int)MFB_RD32(MFB_MFB_DBG_CTL0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_DBG_CTL1_HW),
		(unsigned int)MFB_RD32(MFB_MFB_DBG_CTL1_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_DBG_CTL2_HW),
		(unsigned int)MFB_RD32(MFB_MFB_DBG_CTL2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_DBG_OUT0_HW),
		(unsigned int)MFB_RD32(MFB_MFB_DBG_OUT0_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_DBG_OUT1_HW),
		(unsigned int)MFB_RD32(MFB_MFB_DBG_OUT1_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_DBG_OUT2_HW),
		(unsigned int)MFB_RD32(MFB_MFB_DBG_OUT2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_DBG_OUT3_HW),
		(unsigned int)MFB_RD32(MFB_MFB_DBG_OUT3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_DBG_OUT4_HW),
		(unsigned int)MFB_RD32(MFB_MFB_DBG_OUT4_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_MFB_DBG_OUT5_HW),
		(unsigned int)MFB_RD32(MFB_MFB_DBG_OUT5_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFB_DFTC_HW),
		(unsigned int)MFB_RD32(MFB_DFTC_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_DMA_SOFT_RSTSTAT_HW),
		(unsigned int)MFB_RD32(MFBDMA_DMA_SOFT_RSTSTAT_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_TDRI_BASE_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_TDRI_BASE_ADDR_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_TDRI_OFST_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_TDRI_OFST_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_TDRI_XSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_TDRI_XSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_VERTICAL_FLIP_EN_HW),
		(unsigned int)MFB_RD32(MFBDMA_VERTICAL_FLIP_EN_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_DMA_SOFT_RESET_HW),
		(unsigned int)MFB_RD32(MFBDMA_DMA_SOFT_RESET_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_LAST_ULTRA_EN_HW),
		(unsigned int)MFB_RD32(MFBDMA_LAST_ULTRA_EN_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_SPECIAL_FUN_EN_HW),
		(unsigned int)MFB_RD32(MFBDMA_SPECIAL_FUN_EN_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_BASE_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_BASE_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_OFST_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_OFST_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_XSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_XSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_YSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_YSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_STRIDE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_STRIDE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_CON_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_CON_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_CON2_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_CON2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_CON3_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_CON3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_CROP_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_CROP_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2O_BASE_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2O_BASE_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2O_OFST_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2O_OFST_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2O_XSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2O_XSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2O_YSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2O_YSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2O_STRIDE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2O_STRIDE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2O_CON_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2O_CON_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2O_CON2_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2O_CON2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2O_CON3_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2O_CON3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2O_CROP_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2O_CROP_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_BASE_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_BASE_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_OFST_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_OFST_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_XSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_XSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_YSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_YSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_STRIDE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_STRIDE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_CON_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_CON_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_CON2_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_CON2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_CON3_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_CON3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_BASE_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_BASE_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_OFST_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_OFST_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_XSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_XSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_YSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_YSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_STRIDE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_STRIDE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_CON_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_CON_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_CON2_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_CON2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_CON3_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_CON3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB3I_BASE_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB3I_BASE_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB3I_OFST_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB3I_OFST_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB3I_XSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB3I_XSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB3I_YSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB3I_YSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB3I_STRIDE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB3I_STRIDE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB3I_CON_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB3I_CON_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB3I_CON2_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB3I_CON2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB3I_CON3_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB3I_CON3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB4I_BASE_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB4I_BASE_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB4I_OFST_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB4I_OFST_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB4I_XSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB4I_XSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB4I_YSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB4I_YSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB4I_STRIDE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB4I_STRIDE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB4I_CON_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB4I_CON_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB4I_CON2_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB4I_CON2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB4I_CON3_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB4I_CON3_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_DMA_ERR_CTRL_HW),
		(unsigned int)MFB_RD32(MFBDMA_DMA_ERR_CTRL_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_ERR_STAT_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_ERR_STAT_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2O_ERR_STAT_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2O_ERR_STAT_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_B_ERR_STAT_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_B_ERR_STAT_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_ERR_STAT_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_ERR_STAT_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_ERR_STAT_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_ERR_STAT_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB3I_ERR_STAT_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB3I_ERR_STAT_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB4I_ERR_STAT_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB4I_ERR_STAT_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_B_ERR_STAT_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_B_ERR_STAT_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_B_ERR_STAT_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_B_ERR_STAT_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_DMA_DEBUG_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_DMA_DEBUG_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_DMA_RSV1_HW),
		(unsigned int)MFB_RD32(MFBDMA_DMA_RSV1_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_DMA_RSV2_HW),
		(unsigned int)MFB_RD32(MFBDMA_DMA_RSV2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_DMA_RSV3_HW),
		(unsigned int)MFB_RD32(MFBDMA_DMA_RSV3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_DMA_RSV4_HW),
		(unsigned int)MFB_RD32(MFBDMA_DMA_RSV4_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_DMA_DEBUG_SEL_HW),
		(unsigned int)MFB_RD32(MFBDMA_DMA_DEBUG_SEL_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_DMA_BW_SELF_TEST_HW),
		(unsigned int)MFB_RD32(MFBDMA_DMA_BW_SELF_TEST_REG));

	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_B_BASE_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_B_BASE_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_B_OFST_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_B_OFST_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_B_XSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_B_XSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_B_YSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_B_YSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_B_STRIDE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_B_STRIDE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_B_CON_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_B_CON_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_B_CON2_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_B_CON2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_B_CON3_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_B_CON3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBO_B_CROP_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBO_B_CROP_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_B_BASE_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_B_BASE_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_B_OFST_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_B_OFST_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_B_XSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_B_XSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_B_YSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_B_YSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_B_STRIDE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_B_STRIDE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_B_CON_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_B_CON_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_B_CON2_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_B_CON2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFBI_B_CON3_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFBI_B_CON3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_B_BASE_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_B_BASE_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_B_OFST_ADDR_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_B_OFST_ADDR_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_B_XSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_B_XSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_B_YSIZE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_B_YSIZE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_B_STRIDE_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_B_STRIDE_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_B_CON_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_B_CON_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_B_CON2_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_B_CON2_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(MFBDMA_MFB2I_B_CON3_HW),
		(unsigned int)MFB_RD32(MFBDMA_MFB2I_B_CON3_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(PAK_CONT_Y_HW),
		(unsigned int)MFB_RD32(PAK_CONT_Y_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(PAK_CONT_C_HW),
		(unsigned int)MFB_RD32(PAK_CONT_C_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(UNP_OFST_Y_HW),
		(unsigned int)MFB_RD32(UNP_OFST_Y_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(UNP_CONT_Y_HW),
		(unsigned int)MFB_RD32(UNP_CONT_Y_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(UNP_OFST_C_HW),
		(unsigned int)MFB_RD32(UNP_OFST_C_REG));
	log_inf("[0x%08X %08X]\n", (unsigned int)(UNP_CONT_C_HW),
		(unsigned int)MFB_RD32(UNP_CONT_C_REG));

	log_inf("MFB:HWProcessIdx:%d, WriteIdx:%d, ReadIdx:%d\n",
		g_MFB_ReqRing.HWProcessIdx,
		g_MFB_ReqRing.WriteIdx,
		g_MFB_ReqRing.ReadIdx);

	for (i = 0; i < _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_; i++) {
		log_inf(
		"MFB Req:State:%d, procID:0x%08X, callerID:0x%08X\n",
		 g_MFB_ReqRing.MFBReq_Struct[i].State,
		 g_MFB_ReqRing.MFBReq_Struct[i].processID,
		 g_MFB_ReqRing.MFBReq_Struct[i].callerID);
		log_inf(
		"MFB Req:enqueReqNum:%d, FrameWRIdx:%d, RrameRDIdx:%d\n",
		 g_MFB_ReqRing.MFBReq_Struct[i].enqueReqNum,
		 g_MFB_ReqRing.MFBReq_Struct[i].FrameWRIdx,
		 g_MFB_ReqRing.MFBReq_Struct[i].RrameRDIdx);

	for (j = 0; j < _SUPPORT_MAX_MFB_FRAME_REQUEST_;) {
		log_inf(
		"FrmSta[%d]:%d, FrmSta[%d]:%d, FrmSta[%d]:%d, FrmSta[%d]:%d\n",
		j,
		g_MFB_ReqRing.MFBReq_Struct[i].MfbFrameStatus[j],
		j + 1,
		g_MFB_ReqRing.MFBReq_Struct[i].MfbFrameStatus[j + 1],
		j + 2,
		g_MFB_ReqRing.MFBReq_Struct[i].MfbFrameStatus[j + 2],
		j + 3,
		g_MFB_ReqRing.MFBReq_Struct[i].MfbFrameStatus[j + 3]);
		j = j + 4;
	}

	}

	log_inf("- X.");
	/*  */
	return Ret;
}

#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
static inline void MFB_Prepare_Enable_ccf_clock(void)
{
	int ret;
	smi_bus_prepare_enable(SMI_LARB5, MFB_DEV_NAME);
	ret = clk_prepare_enable(mfb_clk.CG_IMGSYS_MFB);
	if (ret)
		log_err("cannot prepare and enable CG_IMGSYS_MFB clock\n");
}

static inline void MFB_Disable_Unprepare_ccf_clock(void)
{
	clk_disable_unprepare(mfb_clk.CG_IMGSYS_MFB);
	smi_bus_disable_unprepare(SMI_LARB5, MFB_DEV_NAME);
}
#endif

/**************************************************************
 *
 **************************************************************/
static void MFB_EnableClock(bool En)
{
#if defined(EP_NO_CLKMGR)
	unsigned int setReg;
#endif
	/* Enable clock. */
	if (En) {
		/* log_dbg("clock enbled. ClockCount: %d.", ClockCount); */
		switch (g_u4EnableClockCount) {
		case 0:
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifndef EP_NO_CLKMGR
			MFB_Prepare_Enable_ccf_clock();
#else
			setReg = 0xFFFFFFFF;
			MFB_WR32(IMGSYS_REG_CG_CLR, setReg);
#endif
#else
#endif
			break;
		default:
			break;
		}
		spin_lock(&(MFBInfo.SpinLockMFB));
		g_u4EnableClockCount++;
		spin_unlock(&(MFBInfo.SpinLockMFB));
	} else {		/* Disable clock. */

		/* log_dbg("clock disabled. ClockCount: %d.", ClockCount); */
		spin_lock(&(MFBInfo.SpinLockMFB));
		g_u4EnableClockCount--;
		spin_unlock(&(MFBInfo.SpinLockMFB));
		switch (g_u4EnableClockCount) {
		case 0:
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
#ifndef EP_NO_CLKMGR
			MFB_Disable_Unprepare_ccf_clock();
#else
			setReg = 0xFFFFFFFF;
			MFB_WR32(IMGSYS_REG_CG_SET, setReg);
#endif
#else
#endif
			break;
		default:
			break;
		}
	}
}

/**************************************************************
 *
 **************************************************************/
static inline void MFB_Reset(void)
{
	log_dbg("- E.\n");

	log_dbg(" MFB Reset start!\n");
	spin_lock(&(MFBInfo.SpinLockMFBRef));

	if (MFBInfo.UserCount > 1) {
		spin_unlock(&(MFBInfo.SpinLockMFBRef));
		log_dbg("Curr UserCount(%d) users exist\n", MFBInfo.UserCount);
	} else {
		spin_unlock(&(MFBInfo.SpinLockMFBRef));

		/* Reset MFB flow */
		MFB_WR32(MFB_MFB_SW_RST_REG, 0x800);
		while ((MFB_RD32(MFB_MFB_SW_RST_REG) && 0xf000 != 0xf000))
			log_dbg("MFB resetting...\n");
		MFB_WR32(MFB_MFB_SW_RST_REG, 0x80000800);
		udelay(1);
		MFB_WR32(MFB_MFB_SW_RST_REG, 0x80000000);
		udelay(1);
		MFB_WR32(MFB_MFB_SW_RST_REG, 0x0);

		log_dbg(" MFB Reset end!\n");
	}

}

static signed int MFB_WaitIrq(MFB_WAIT_IRQ_STRUCT *WaitIrq)
{

	signed int Ret = 0;
	signed int Timeout = WaitIrq->Timeout;
	enum MFB_PROCESS_ID_ENUM whichReq = MFB_PROCESS_ID_NONE;

	/*unsigned int i;*/
	unsigned long flags; /* old: unsigned int flags;*/
	unsigned int irqStatus;
	/*int cnt = 0;*/
	struct timeval time_getrequest;
	unsigned long long sec = 0;
	unsigned long usec = 0;

	/* do_gettimeofday(&time_getrequest); */
	sec = cpu_clock(0);	/* ns */
	do_div(sec, 1000);	/* usec */
	usec = do_div(sec, 1000000);	/* sec and usec */
	time_getrequest.tv_usec = usec;
	time_getrequest.tv_sec = sec;


	/* Debug interrupt */
	if (MFBInfo.DebugMask & MFB_DBG_INT) {
		if (WaitIrq->Status & MFBInfo.IrqInfo.Mask[WaitIrq->Type]) {
			if (WaitIrq->UserKey > 0) {
				log_dbg("+WaitIrq clr(%d), Type(%d), Stat(0x%08X)\n",
				WaitIrq->Clear,
				WaitIrq->Type,
				WaitIrq->Status);
				log_dbg("+WaitIrq Timeout(%d),usr(%d), ProcID(%d)\n",
				WaitIrq->Timeout,
				WaitIrq->UserKey,
				WaitIrq->ProcessID);
			}
		}
	}


	/* 1. wait type update */
	if (WaitIrq->Clear == MFB_IRQ_CLEAR_STATUS) {
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]),
			flags);
		MFBInfo.IrqInfo.Status[WaitIrq->Type] &=
			(~WaitIrq->Status);
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]),
			flags);
		return Ret;
	}

	if (WaitIrq->Clear == MFB_IRQ_CLEAR_WAIT) {
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]),
			flags);
		if (MFBInfo.IrqInfo.Status[WaitIrq->Type] & WaitIrq->Status)
			MFBInfo.IrqInfo.Status[WaitIrq->Type] &=
			(~WaitIrq->Status);

		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]),
			flags);
	} else if (WaitIrq->Clear == MFB_IRQ_CLEAR_ALL) {
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]),
			flags);

		MFBInfo.IrqInfo.Status[WaitIrq->Type] = 0;
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]),
			flags);
	}
	/* MFB_IRQ_WAIT_CLEAR ==> do nothing */


	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
	spin_lock_irqsave(&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);
	irqStatus = MFBInfo.IrqInfo.Status[WaitIrq->Type];
	spin_unlock_irqrestore(&(MFBInfo.SpinLockIrq[WaitIrq->Type]), flags);

	if (WaitIrq->Status & MFB_INT_ST) {
		whichReq = MFB_PROCESS_ID_MFB;
	} else {
		log_err("No Stats can be wait!\n");
		log_err("irq Type/User/Sts/Pid(0x%x/%d/0x%x/%d)\n",
			WaitIrq->Type,
			WaitIrq->UserKey,
			WaitIrq->Status,
			WaitIrq->ProcessID);
	}


#ifdef MFB_WAITIRQ_LOG
log_inf("before wait_event:Tout(%d), clear(%d), Type(%d)\n",
	WaitIrq->Timeout,
	WaitIrq->Clear,
	WaitIrq->Type);
log_inf("before wait_event:IrqStat(0x%08X), WaitStat(0x%08X), usrKey(%d)\n",
	irqStatus,
	WaitIrq->Status,
	WaitIrq->UserKey);
log_inf("before wait_event:ProcID(%d), MfbIrqCnt(0x%08X)\n",
	WaitIrq->ProcessID,
	MFBInfo.IrqInfo.MfbIrqCnt);
log_inf("before wait_event:WriteReq(0x%08X), ReadReq(0x%08X), whichReq(%d)\n",
	MFBInfo.WriteReqIdx,
	MFBInfo.ReadReqIdx,
	whichReq);
#endif

	/* 2. start to wait signal */
	Timeout = wait_event_interruptible_timeout(
		MFBInfo.WaitQueueHead,
		MFB_GetIRQState(
			WaitIrq->Type,
			WaitIrq->UserKey,
			WaitIrq->Status,
			whichReq,
			WaitIrq->ProcessID),
		MFB_MsToJiffies(WaitIrq->Timeout));

	/* check if user is interrupted by system signal */
	if ((Timeout != 0) &&
		(!MFB_GetIRQState(
			WaitIrq->Type,
			WaitIrq->UserKey,
			WaitIrq->Status,
			whichReq,
			WaitIrq->ProcessID))) {
		log_inf("interrupt by system, tout(%d)\n", Timeout);
		log_inf("Type/User/Sts/whichReq/Pid(0x%x/%d/0x%x/%d/%d)\n",
			WaitIrq->Type,
			WaitIrq->UserKey,
			WaitIrq->Status,
			whichReq,
			WaitIrq->ProcessID);

		Ret = -ERESTARTSYS;/* actually it should be -ERESTARTSYS */
		goto EXIT;
	}
	/* timeout */
	if (Timeout == 0) {
	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]),
			flags);
		irqStatus = MFBInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]),
			flags);

log_inf("WaitIrq Timeout:Tout(%d), clear(%d), Type(%d)\n",
	WaitIrq->Timeout,
	WaitIrq->Clear,
	WaitIrq->Type);
log_inf("WaitIrq Timeout:IrqStat(0x%08X), WaitStat(0x%08X), usrKey(%d)\n",
	irqStatus,
	WaitIrq->Status,
	WaitIrq->UserKey);
log_inf("WaitIrq Timeout:ProcID(%d), MfbIrqCnt(0x%08X)\n",
	WaitIrq->ProcessID,
	MFBInfo.IrqInfo.MfbIrqCnt);
log_inf("WaitIrq Timeout:WriteReq(0x%08X), ReadReq(0x%08X), whichReq(%d)\n",
	MFBInfo.WriteReqIdx,
	MFBInfo.ReadReqIdx,
	whichReq);

		if (WaitIrq->bDumpReg)
			MFB_DumpReg();

		Ret = -EFAULT;
		goto EXIT;
	} else {
	/* Store irqinfo status in here to redeuce time of spin_lock_irqsave */
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("MFB WaitIrq");
#endif

		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]),
			flags);
		irqStatus = MFBInfo.IrqInfo.Status[WaitIrq->Type];
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[WaitIrq->Type]),
			flags);

		if (WaitIrq->Clear == MFB_IRQ_WAIT_CLEAR) {
			spin_lock_irqsave(
				&(MFBInfo.SpinLockIrq[WaitIrq->Type]),
				flags);
#ifdef MFB_USE_GCE

#ifdef MFB_MULTIPROCESS_TIMING_ISSUE
		MFBInfo.ReadReqIdx = (MFBInfo.ReadReqIdx + 1) %
			_SUPPORT_MAX_MFB_FRAME_REQUEST_;
			/* actually, it doesn't happen the timging issue!! */
			/* wake_up_interruptible(&MFBInfo.WaitQueueHead); */
#endif
		if (WaitIrq->Status & MFB_INT_ST) {
			MFBInfo.IrqInfo.MfbIrqCnt--;
			if (MFBInfo.IrqInfo.MfbIrqCnt == 0)
				MFBInfo.IrqInfo.Status[WaitIrq->Type] &=
				(~WaitIrq->Status);
			} else {
				log_err(
			"MFB_IRQ_WAIT_CLEAR Error, Type(%d), WaitSta(0x%08X)",
				WaitIrq->Type, WaitIrq->Status);
			}
#else
		if (MFBInfo.IrqInfo.Status[WaitIrq->Type] & WaitIrq->Status)
			MFBInfo.IrqInfo.Status[WaitIrq->Type] &=
			(~WaitIrq->Status);
#endif
		spin_unlock_irqrestore(&(MFBInfo.SpinLockIrq[WaitIrq->Type]),
			flags);
		}

#ifdef MFB_WAITIRQ_LOG
log_inf("no Timeout:Tout(%d), clear(%d), Type(%d)\n",
	WaitIrq->Timeout,
	WaitIrq->Clear,
	WaitIrq->Type);
log_inf("no Timeout:IrqStat(0x%08X), WaitStat(0x%08X), usrKey(%d)\n",
	irqStatus,
	WaitIrq->Status,
	WaitIrq->UserKey);
log_inf("no Timeout:ProcID(%d), MfbIrqCnt(0x%08X)\n",
	WaitIrq->ProcessID,
	MFBInfo.IrqInfo.MfbIrqCnt);
log_inf("no Timeout:WriteReq(0x%08X), ReadReq(0x%08X), whichReq(%d)\n",
	MFBInfo.WriteReqIdx,
	MFBInfo.ReadReqIdx,
	whichReq);
#endif

#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif

	}


EXIT:


	return Ret;
}


/**************************************************************
 *
 **************************************************************/
static long MFB_ioctl(struct file *pFile, unsigned int Cmd, unsigned long Param)
{
	signed int Ret = 0;

	/*unsigned int pid = 0;*/
	MFB_WAIT_IRQ_STRUCT IrqInfo;
	MFB_CLEAR_IRQ_STRUCT ClearIrq;
	MFB_Config mfb_MfbConfig;
	MFB_Request mfb_MfbReq;
	unsigned int MfbWriteIdx = 0;
	int idx;
	struct MFB_USER_INFO_STRUCT *pUserInfo;
	int enqueNum;
	int dequeNum;
	unsigned long flags; /* old: unsigned int flags;*/



	/*  */
	if (pFile->private_data == NULL) {
		log_wrn(
		"private_data is NULL,(process, pid, tgid)=(%s, %d, %d)",
		current->comm,
		current->pid, current->tgid);
	return -EFAULT;
	}
	/*  */
	pUserInfo = (struct MFB_USER_INFO_STRUCT *) (pFile->private_data);
	/*  */
	switch (Cmd) {
	case MFB_RESET:
	{
	spin_lock(&(MFBInfo.SpinLockMFB));
	MFB_Reset();
	spin_unlock(&(MFBInfo.SpinLockMFB));
	break;
	}

	/*  */
	case MFB_DUMP_REG:
	{
	Ret = MFB_DumpReg();
	break;
	}
	case MFB_WAIT_IRQ:
	{
	if (copy_from_user(
		&IrqInfo,
		(void *)Param,
		sizeof(MFB_WAIT_IRQ_STRUCT)) == 0) {
		/*  */
		if ((IrqInfo.Type >= MFB_IRQ_TYPE_AMOUNT) ||
			(IrqInfo.Type < 0)) {
			Ret = -EFAULT;
			log_err("invalid type(%d)", IrqInfo.Type);
			goto EXIT;
		}

		if ((IrqInfo.UserKey >= IRQ_USER_NUM_MAX) ||
			(IrqInfo.UserKey < 0)) {
			log_err("invalid userKey(%d), max(%d), force userkey = 0\n",
			IrqInfo.UserKey, IRQ_USER_NUM_MAX);
			IrqInfo.UserKey = 0;
		}

		log_inf("IRQ clr(%d),type(%d),usrKey(%d),timeout(%d),sta(%d)\n",
			 IrqInfo.Clear,
			 IrqInfo.Type,
			 IrqInfo.UserKey,
			 IrqInfo.Timeout,
			 IrqInfo.Status);
		IrqInfo.ProcessID = pUserInfo->Pid;
		Ret = MFB_WaitIrq(&IrqInfo);

		if (copy_to_user(
			(void *)Param,
			&IrqInfo,
			sizeof(MFB_WAIT_IRQ_STRUCT)) != 0) {
			log_err("copy_to_user failed\n");
			Ret = -EFAULT;
		}
	} else {
		log_err("MFB_WAIT_IRQ copy_from_user failed");
		Ret = -EFAULT;
	}

#ifdef MFB_PMQOS
	MFBQOS_UpdateImgFreq(0);
#endif
	break;
	}
	case MFB_CLEAR_IRQ:
	{
	if (copy_from_user(
		&ClearIrq,
		(void *)Param,
		sizeof(MFB_CLEAR_IRQ_STRUCT)) == 0) {
		log_dbg("MFB_CLEAR_IRQ Type(%d)\n", ClearIrq.Type);

		if ((ClearIrq.Type >= MFB_IRQ_TYPE_AMOUNT) ||
			(ClearIrq.Type < 0)) {
			Ret = -EFAULT;
			log_err("invalid type(%d)", ClearIrq.Type);
			goto EXIT;
		}

		/*  */
		if ((ClearIrq.UserKey >= IRQ_USER_NUM_MAX)
			|| (ClearIrq.UserKey < 0)) {
			log_err("errUserEnum(%d)", ClearIrq.UserKey);
			Ret = -EFAULT;
			goto EXIT;
		}

		log_dbg("CLEAR_IRQ:Type(%d),Sta(0x%08X),IrqSta(0x%08X)\n",
			ClearIrq.Type,
			ClearIrq.Status,
			MFBInfo.IrqInfo.Status[ClearIrq.Type]);
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[ClearIrq.Type]),
			flags);
			MFBInfo.IrqInfo.Status[ClearIrq.Type] &=
				(~ClearIrq.Status);
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[ClearIrq.Type]),
			flags);
	} else {
		log_err("MFB_CLEAR_IRQ copy_from_user failed\n");
		Ret = -EFAULT;
	}
	break;
	}
	case MFB_ENQNUE_NUM:
	{
	/* enqueNum */
	if (copy_from_user(&enqueNum, (void *)Param, sizeof(int)) == 0) {
		if (MFB_REQUEST_STATE_EMPTY ==
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.State) {
			spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
			flags);
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing
				.WriteIdx].processID = pUserInfo->Pid;
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing
				.WriteIdx].enqueReqNum = enqueNum;
			spin_unlock_irqrestore(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
				flags);
			if (enqueNum > _SUPPORT_MAX_MFB_FRAME_REQUEST_) {
				log_err
				("MFB Enque Num is bigger than enqueNum:%d\n",
				enqueNum);
			}
			log_dbg("MFB_ENQNUE_NUM:%d\n", enqueNum);
		} else {
			log_err
		("Enque request state is not empty:%d, wrIdx:%d, rdIdx:%d\n",
			g_MFB_ReqRing
			.MFBReq_Struct[g_MFB_ReqRing.WriteIdx].State,
			g_MFB_ReqRing.WriteIdx,
			g_MFB_ReqRing.ReadIdx);
		}
	} else {
		log_err("MFB_EQNUE_NUM copy_from_user failed\n");
		Ret = -EFAULT;
	}

	break;
	}
	/* MFB_Config */
	case MFB_ENQUE:
	{
	if (copy_from_user(
		&mfb_MfbConfig,
		(void *)Param,
		sizeof(MFB_Config)) == 0) {
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
			flags);
		if ((MFB_REQUEST_STATE_EMPTY ==
			g_MFB_ReqRing
			.MFBReq_Struct[g_MFB_ReqRing.WriteIdx].State)
			&& (g_MFB_ReqRing
			.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.FrameWRIdx < g_MFB_ReqRing
			.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.enqueReqNum)) {
			g_MFB_ReqRing
			.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.MfbFrameStatus[g_MFB_ReqRing
			.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.FrameWRIdx] = MFB_FRAME_STATUS_ENQUE;
			memcpy(&g_MFB_ReqRing
			.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.MfbFrameConfig[g_MFB_ReqRing
			.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.FrameWRIdx++],
			&mfb_MfbConfig,
			sizeof(MFB_Config));
			if (g_MFB_ReqRing
			.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.FrameWRIdx ==
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing
			.WriteIdx].enqueReqNum) {
				g_MFB_ReqRing
				.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
				.State = MFB_REQUEST_STATE_PENDING;
				g_MFB_ReqRing.WriteIdx =
				(g_MFB_ReqRing.WriteIdx + 1) %
				_SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
				log_dbg("MFB enque done!!\n");
			} else {
				log_dbg("MFB enque frame!!\n");
			}
		} else {
			log_err("No Buffer! WrIdx(%d), Sta(%d), FrmWrIdx(%d), enReqNum(%d)\n",
			g_MFB_ReqRing.WriteIdx,
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.State,
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.FrameWRIdx,
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.enqueReqNum);
		}
#ifdef MFB_USE_GCE
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
			flags);
		log_dbg("ConfigMFB!!\n");
		ConfigMFB();
#else
		/* check the hw is running or not ? */
		if (Check_MFB_Is_Busy() == MFALSE) {
			/* config the mfb hw and run */
			log_dbg("ConfigMFB\n");
			ConfigMFB();
		} else {
			log_inf("MFB HW is busy!!\n");
		}
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
			flags);
#endif


	} else {
		log_err("MFB_ENQUE copy_from_user failed\n");
		Ret = -EFAULT;
	}

	break;
	}
	case MFB_ENQUE_REQ:
	{
	if (copy_from_user(
		&mfb_MfbReq,
		(void *)Param,
		sizeof(MFB_Request)) == 0) {
		log_dbg("MFB_ENQNUE_NUM:%d, pid:%d\n",
			mfb_MfbReq.m_ReqNum,
			pUserInfo->Pid);
		if (mfb_MfbReq.m_ReqNum >
			_SUPPORT_MAX_MFB_FRAME_REQUEST_) {
			log_err("MFB Enque Num is bigger than enqueNum:%d\n",
			mfb_MfbReq.m_ReqNum);
			Ret = -EFAULT;
			goto EXIT;
		}
		if (copy_from_user(
			g_MfbEnqueReq_Struct.MfbFrameConfig,
			(void *)mfb_MfbReq.m_pMfbConfig,
			mfb_MfbReq.m_ReqNum * sizeof(MFB_Config))
			!= 0) {
			log_err("copy MFBConfig from request is fail!!\n");
			Ret = -EFAULT;
			goto EXIT;
		}

		mutex_lock(&gMfbMutex);	/* Protect the Multi Process */

		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
			flags);
		if (MFB_REQUEST_STATE_EMPTY ==
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.State) {
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing
			.WriteIdx].processID = pUserInfo->Pid;
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing
			.WriteIdx].enqueReqNum =	mfb_MfbReq.m_ReqNum;

			for (idx = 0; idx < mfb_MfbReq.m_ReqNum; idx++) {
				g_MFB_ReqRing
				.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
				.MfbFrameStatus[g_MFB_ReqRing
				.MFBReq_Struct
				[g_MFB_ReqRing.WriteIdx]
				.FrameWRIdx] =
				MFB_FRAME_STATUS_ENQUE;
				memcpy(&g_MFB_ReqRing
				.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
				.MfbFrameConfig[g_MFB_ReqRing
				.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
				.FrameWRIdx++],
				&g_MfbEnqueReq_Struct
				.MfbFrameConfig[idx],
				sizeof(MFB_Config));
			}
			g_MFB_ReqRing
			.MFBReq_Struct[g_MFB_ReqRing.WriteIdx].State =
			MFB_REQUEST_STATE_PENDING;
			MfbWriteIdx = g_MFB_ReqRing.WriteIdx;
			g_MFB_ReqRing.WriteIdx = (g_MFB_ReqRing.WriteIdx + 1) %
				_SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
			log_dbg("MFB request enque done!!\n");
		} else {
			log_err("Enque req NG: WrIdx(%d) Sta(%d) FrmWrIdx(%d) enReqNum(%d)\n",
			g_MFB_ReqRing.WriteIdx,
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.State,
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.FrameWRIdx,
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.WriteIdx]
			.enqueReqNum);
	}
		spin_unlock_irqrestore(
			&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
			flags);
		log_inf("ConfigMFB Request!!\n");
		ConfigMFBRequest(MfbWriteIdx);

		mutex_unlock(&gMfbMutex);
	} else {
		log_err("MFB_ENQUE_REQ copy_from_user failed\n");
		Ret = -EFAULT;
	}

	break;
	}
	case MFB_DEQUE_NUM:
	{
		if (MFB_REQUEST_STATE_FINISHED ==
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
			.State) {
			dequeNum =
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
			.enqueReqNum;
			log_dbg("MFB_DEQUE_NUM(%d)\n", dequeNum);
		} else {
			dequeNum = 0;
	log_err("Deque no buf: RdIdx(%d) Sta(%d) RrameRDIdx(%d) enReqNum(%d)\n",
		g_MFB_ReqRing.ReadIdx,
		g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx].State,
		g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx].RrameRDIdx,
		g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx].enqueReqNum);
		}
		if (copy_to_user(
			(void *)Param,
			&dequeNum,
			sizeof(unsigned int)) != 0) {
			log_err("MFB_DEQUE_NUM copy_to_user failed\n");
			Ret = -EFAULT;
		}

		break;
	}
	case MFB_DEQUE:
	{
		spin_lock_irqsave(
			&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
			flags);
		if ((MFB_REQUEST_STATE_FINISHED ==
			 g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
			 .State)
			&& (g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
			.RrameRDIdx <
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
			.enqueReqNum)) {
			if (MFB_FRAME_STATUS_FINISHED ==
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
			.MfbFrameStatus[g_MFB_ReqRing
			.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
			.RrameRDIdx]) {
				memcpy(&mfb_MfbConfig,
				&g_MFB_ReqRing
				.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
				.MfbFrameConfig[g_MFB_ReqRing
				.MFBReq_Struct[g_MFB_ReqRing
				.ReadIdx].RrameRDIdx],
				sizeof(MFB_Config));
				g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing
				.ReadIdx]
				.MfbFrameStatus[g_MFB_ReqRing
				.MFBReq_Struct[g_MFB_ReqRing
				.ReadIdx].RrameRDIdx++] =
					MFB_FRAME_STATUS_EMPTY;
			}
			if (g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
			.RrameRDIdx ==
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
			.enqueReqNum) {
				g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing
				.ReadIdx].State =
				MFB_REQUEST_STATE_EMPTY;
				g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing
				.ReadIdx].FrameWRIdx = 0;
				g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing
				.ReadIdx].RrameRDIdx = 0;
				g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing
				.ReadIdx].enqueReqNum = 0;
				g_MFB_ReqRing.ReadIdx =
				(g_MFB_ReqRing.ReadIdx + 1) %
					_SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
			log_dbg("MFB ReadIdx(%d)\n", g_MFB_ReqRing.ReadIdx);
			}
			spin_unlock_irqrestore(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
				flags);
			if (copy_to_user
			((void *)Param,
			&g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
			.MfbFrameConfig[g_MFB_ReqRing
			.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
			.RrameRDIdx], sizeof(MFB_Config)) != 0) {
				log_err("MFB_DEQUE copy_to_user failed\n");
				Ret = -EFAULT;
			}

		} else {
			spin_unlock_irqrestore(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
				flags);
	log_err("Deque No Buf:RdIdx(%d)Sta(%d)RrameRDIdx(%d),enReqNum(%d)\n",
	g_MFB_ReqRing.ReadIdx,
	g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx].State,
	g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx].RrameRDIdx,
	g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx].enqueReqNum);
		}

		break;
	}
	case MFB_DEQUE_REQ:
	{
		if (copy_from_user(
			&mfb_MfbReq,
			(void *)Param,
			sizeof(MFB_Request)) == 0) {
			mutex_lock(&gMfbDequeMutex);

			spin_lock_irqsave(
				&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
				flags);

			if (MFB_REQUEST_STATE_FINISHED ==
				g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing
				.ReadIdx].State) {
				dequeNum =
				g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing
				.ReadIdx].enqueReqNum;
				log_dbg("MFB_DEQUE_REQ(%d)\n", dequeNum);
			} else {
				dequeNum = 0;
				log_err(
				"DEQUE_REQ no buf:RIdx(%d) Stat(%d) RrameRDIdx(%d) enqueReqNum(%d)\n",
				g_MFB_ReqRing.ReadIdx,
				g_MFB_ReqRing
				.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
				.State,
				g_MFB_ReqRing
				.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
				.RrameRDIdx,
				g_MFB_ReqRing
				.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
				.enqueReqNum);
			}
			mfb_MfbReq.m_ReqNum = dequeNum;

			for (idx = 0; idx < dequeNum; idx++) {
				if (MFB_FRAME_STATUS_FINISHED ==
					g_MFB_ReqRing
					.MFBReq_Struct[g_MFB_ReqRing
					.ReadIdx]
					.MfbFrameStatus[g_MFB_ReqRing
					.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
					.RrameRDIdx]) {
					memcpy(&g_MfbDequeReq_Struct
					.MfbFrameConfig[idx],
					&g_MFB_ReqRing
					.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
					.MfbFrameConfig[g_MFB_ReqRing
					.MFBReq_Struct
					[g_MFB_ReqRing.ReadIdx]
					.RrameRDIdx],
					sizeof(MFB_Config));
					g_MFB_ReqRing
					.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
					.MfbFrameStatus[g_MFB_ReqRing
					.MFBReq_Struct
					[g_MFB_ReqRing.ReadIdx]
					.RrameRDIdx++] =
					MFB_FRAME_STATUS_EMPTY;
			} else {
				log_err(
				"deq err idx(%d) dequNum(%d) Rd(%d) RrameRD(%d) FrmStat(%d)\n",
				idx,
				dequeNum,
				g_MFB_ReqRing.ReadIdx,
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
				.RrameRDIdx,
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
				.MfbFrameStatus
			[g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
				.RrameRDIdx]);
			}
			}
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
				.State = MFB_REQUEST_STATE_EMPTY;
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
				.FrameWRIdx = 0;
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
				.RrameRDIdx = 0;
			g_MFB_ReqRing.MFBReq_Struct[g_MFB_ReqRing.ReadIdx]
				.enqueReqNum = 0;
			g_MFB_ReqRing.ReadIdx =
				(g_MFB_ReqRing.ReadIdx +
				1) % _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_;
			log_dbg("MFB Request ReadIdx(%d)\n",
				g_MFB_ReqRing.ReadIdx);

			spin_unlock_irqrestore(&
			(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]),
			flags);

			mutex_unlock(&gMfbDequeMutex);

			if (mfb_MfbReq.m_pMfbConfig == NULL) {
				log_err("NULL pointer:mfb_MfbReq.m_pMfbConfig");
				Ret = -EFAULT;
				goto EXIT;
			}

			if (copy_to_user
			((void *)mfb_MfbReq.m_pMfbConfig,
			&g_MfbDequeReq_Struct.MfbFrameConfig[0],
			dequeNum * sizeof(MFB_Config)) != 0) {
				log_err
			("MFB_DEQUE_REQ copy_to_user frameconfig failed\n");
				Ret = -EFAULT;
			}
			if (copy_to_user
			((void *)Param,
			&mfb_MfbReq,
			sizeof(MFB_Request)) != 0) {
				log_err("MFB_DEQUE_REQ copy_to_user failed\n");
				Ret = -EFAULT;
			}
		} else {
			log_err("MFB_CMD_MFB_DEQUE_REQ copy_from_user failed\n");
		Ret = -EFAULT;
		}

		break;
	}
	default:
	{
		log_err("Unknown Cmd(%d)", Cmd);
		log_err("Fail,Cmd(%d),Dir(%d),Type(%d),Nr(%d),Size(%d)\n",
			Cmd,
			_IOC_DIR(Cmd),
			_IOC_TYPE(Cmd),
			_IOC_NR(Cmd),
			_IOC_SIZE(Cmd));
		Ret = -EPERM;
		break;
	}
	}
	/*  */
EXIT:
	if (Ret != 0) {
		log_err("Fail, Cmd(%d), Pid(%d),(proc,pid,tgid)=(%s,%d,%d)",
			Cmd,
			pUserInfo->Pid,
			current->comm,
			current->pid,
			current->tgid);
	}
	/*  */
	return Ret;
}

#ifdef CONFIG_COMPAT

/**************************************************************
 *
 **************************************************************/

static int compat_get_MFB_enque_req_data(
	compat_MFB_Request __user *data32,
	MFB_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pMfbConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pMfbConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_MFB_enque_req_data(
	compat_MFB_Request __user *data32,
	MFB_Request __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pDpeConfig); */
	/* err |= put_user(uptr, &data32->m_pDpeConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}


static int compat_get_MFB_deque_req_data(
	compat_MFB_Request __user *data32,
	MFB_Request __user *data)
{
	compat_uint_t count;
	compat_uptr_t uptr;
	int err = 0;

	err = get_user(uptr, &data32->m_pMfbConfig);
	err |= put_user(compat_ptr(uptr), &data->m_pMfbConfig);
	err |= get_user(count, &data32->m_ReqNum);
	err |= put_user(count, &data->m_ReqNum);
	return err;
}


static int compat_put_MFB_deque_req_data(
	compat_MFB_Request __user *data32,
	MFB_Request __user *data)
{
	compat_uint_t count;
	/*compat_uptr_t uptr;*/
	int err = 0;
	/* Assume data pointer is unchanged. */
	/* err = get_user(compat_ptr(uptr), &data->m_pDpeConfig); */
	/* err |= put_user(uptr, &data32->m_pDpeConfig); */
	err |= get_user(count, &data->m_ReqNum);
	err |= put_user(count, &data32->m_ReqNum);
	return err;
}

static long MFB_ioctl_compat(
	struct file *filp,
	unsigned int cmd,
	unsigned long arg)
{
	long ret;


	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		log_err("no f_op !!!\n");
		return -ENOTTY;
	}
	switch (cmd) {
	case COMPAT_MFB_ENQUE_REQ:
		{
			compat_MFB_Request __user *data32;
			MFB_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_enque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_MFB_ENQUE_REQ error!!!\n");
				return err;
			}
			ret =
				filp->f_op->unlocked_ioctl(filp, MFB_ENQUE_REQ,
							   (unsigned long)data);
			err = compat_put_MFB_enque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_MFB_ENQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}
	case COMPAT_MFB_DEQUE_REQ:
		{
			compat_MFB_Request __user *data32;
			MFB_Request __user *data;
			int err;

			data32 = compat_ptr(arg);
			data = compat_alloc_user_space(sizeof(*data));
			if (data == NULL)
				return -EFAULT;

			err = compat_get_MFB_deque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_MFB_DEQUE_REQ error!!!\n");
				return err;
			}
			ret =
				filp->f_op->unlocked_ioctl(filp, MFB_DEQUE_REQ,
							   (unsigned long)data);
			err = compat_put_MFB_deque_req_data(data32, data);
			if (err) {
				log_inf("COMPAT_MFB_DEQUE_REQ error!!!\n");
				return err;
			}
			return ret;
		}

	case MFB_WAIT_IRQ:
	case MFB_CLEAR_IRQ:	/* structure (no pointer) */
	case MFB_ENQNUE_NUM:
	case MFB_ENQUE:
	case MFB_DEQUE_NUM:
	case MFB_DEQUE:
	case MFB_RESET:
	case MFB_DUMP_REG:
		return filp->f_op->unlocked_ioctl(filp, cmd, arg);
	default:
		return -ENOIOCTLCMD;
		/* return MFB_ioctl(filep, cmd, arg); */
	}
}

#endif

/*************************************************************
 *
 **************************************************************/
static signed int MFB_open(struct inode *pInode, struct file *pFile)
{
	signed int Ret = 0;
	unsigned int i, j;
	/*int q = 0, p = 0;*/
	struct MFB_USER_INFO_STRUCT *pUserInfo;

	log_inf("- E. UserCount: %d.\n", MFBInfo.UserCount);


	/*  */
	spin_lock(&(MFBInfo.SpinLockMFBRef));

	pFile->private_data = NULL;
	pFile->private_data = kmalloc(
		sizeof(struct MFB_USER_INFO_STRUCT),
		GFP_ATOMIC);
	if (pFile->private_data == NULL) {
		log_dbg("ERROR: kmalloc failed,(proc,pid,tgid)=(%s,%d,%d)\n",
			current->comm,
			current->pid,
			current->tgid);
		Ret = -ENOMEM;
	} else {
		pUserInfo = (struct MFB_USER_INFO_STRUCT *) pFile->private_data;
		pUserInfo->Pid = current->pid;
		pUserInfo->Tid = current->tgid;
	}
	/*  */
	if (MFBInfo.UserCount > 0) {
		MFBInfo.UserCount++;
		spin_unlock(&(MFBInfo.SpinLockMFBRef));
		log_dbg("Cur UserCnt(%d),(proc,pid,tgid)=(%s,%d,%d),user exist",
			MFBInfo.UserCount,
			current->comm,
			current->pid,
			current->tgid);
		goto EXIT;
	} else {
		MFBInfo.UserCount++;
		spin_unlock(&(MFBInfo.SpinLockMFBRef));
		log_dbg("Cur UserCnt(%d),(proc,pid,tgid)=(%s,%d,%d),first user",
			MFBInfo.UserCount,
			current->comm,
			current->pid,
			current->tgid);
	}

	/* do wait queue head init when re-enter in camera */
	/*  */
	for (i = 0; i < _SUPPORT_MAX_MFB_REQUEST_RING_SIZE_; i++) {
		/* MFB */
		g_MFB_ReqRing.MFBReq_Struct[i].processID = 0x0;
		g_MFB_ReqRing.MFBReq_Struct[i].callerID = 0x0;
		g_MFB_ReqRing.MFBReq_Struct[i].enqueReqNum = 0x0;
		/* g_MFB_ReqRing.MFBReq_Struct[i].enqueIdx = 0x0; */
		g_MFB_ReqRing.MFBReq_Struct[i].State = MFB_REQUEST_STATE_EMPTY;
		g_MFB_ReqRing.MFBReq_Struct[i].FrameWRIdx = 0x0;
		g_MFB_ReqRing.MFBReq_Struct[i].RrameRDIdx = 0x0;
		for (j = 0; j < _SUPPORT_MAX_MFB_FRAME_REQUEST_; j++) {
			g_MFB_ReqRing.MFBReq_Struct[i].MfbFrameStatus[j] =
				MFB_FRAME_STATUS_EMPTY;
		}

	}
	g_MFB_ReqRing.WriteIdx = 0x0;
	g_MFB_ReqRing.ReadIdx = 0x0;
	g_MFB_ReqRing.HWProcessIdx = 0x0;

	/* Enable clock */
	MFB_EnableClock(MTRUE);
	g_u4MfbCnt = 0;
	log_inf("MFB open g_u4EnableClockCount: %d\n", g_u4EnableClockCount);
	/*  */

	for (i = 0; i < MFB_IRQ_TYPE_AMOUNT; i++)
		MFBInfo.IrqInfo.Status[i] = 0;

	for (i = 0; i < _SUPPORT_MAX_MFB_FRAME_REQUEST_; i++)
		MFBInfo.ProcessID[i] = 0;

	MFBInfo.WriteReqIdx = 0;
	MFBInfo.ReadReqIdx = 0;
	MFBInfo.IrqInfo.MfbIrqCnt = 0;

#define KERNEL_LOG
#ifdef KERNEL_LOG
	/* In EP, Add MFB_DBG_WRITE_REG for debug. Should remove it after EP */
	MFBInfo.DebugMask = (MFB_DBG_INT | MFB_DBG_DBGLOG | MFB_DBG_WRITE_REG);
#endif
	/*  */
EXIT:

	log_dbg("- X. Ret: %d. UserCount: %d.\n", Ret, MFBInfo.UserCount);
	return Ret;

}

/**************************************************************
 *
 **************************************************************/
static signed int MFB_release(struct inode *pInode, struct file *pFile)
{
	struct MFB_USER_INFO_STRUCT *pUserInfo;
	/*unsigned int Reg;*/

	log_inf("- E. UserCount: %d.\n", MFBInfo.UserCount);

	/*  */
	if (pFile->private_data != NULL) {
		pUserInfo =
			(struct MFB_USER_INFO_STRUCT *)pFile->private_data;
		kfree(pFile->private_data);
		pFile->private_data = NULL;
	}
	/*  */
	spin_lock(&(MFBInfo.SpinLockMFBRef));
	MFBInfo.UserCount--;

	if (MFBInfo.UserCount > 0) {
		spin_unlock(&(MFBInfo.SpinLockMFBRef));
		log_dbg("Cur UserCnt(%d),(proc,pid,tgid)=(%s,%d,%d),user exist",
			MFBInfo.UserCount,
			current->comm,
			current->pid,
			current->tgid);
		goto EXIT;
	} else
		spin_unlock(&(MFBInfo.SpinLockMFBRef));
	/*  */
	log_dbg("Cur UserCnt(%d), (proc, pid, tgid)=(%s,%d,%d),last user",
		MFBInfo.UserCount,
		current->comm,
		current->pid,
		current->tgid);


	/* Disable clock. */
	MFB_EnableClock(MFALSE);
	log_inf("MFB release g_u4EnableClockCount: %d", g_u4EnableClockCount);

	/*  */
EXIT:


	log_dbg("- X. UserCount: %d.\n", MFBInfo.UserCount);
	return 0;
}

/**************************************************************
 *
 **************************************************************/

static dev_t MFBDevNo;
static struct cdev *pMFBCharDrv;
static struct class *pMFBClass;

static const struct file_operations MFBFileOper = {
	.owner = THIS_MODULE,
	.open = MFB_open,
	.release = MFB_release,
	/* .flush   = mt_MFB_flush, */
	/* .mmap = MFB_mmap, */
	.unlocked_ioctl = MFB_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = MFB_ioctl_compat,
#endif
};

/**************************************************************
 *
 **************************************************************/
static inline void MFB_UnregCharDev(void)
{
	log_dbg("- E.\n");
	/*  */
	/* Release char driver */
	if (pMFBCharDrv != NULL) {
		cdev_del(pMFBCharDrv);
		pMFBCharDrv = NULL;
	}
	/*  */
	unregister_chrdev_region(MFBDevNo, 1);
}

/**************************************************************
 *
 **************************************************************/
static inline signed int MFB_RegCharDev(void)
{
	signed int Ret = 0;
	/*  */
	log_dbg("- E.\n");
	/*  */
	Ret = alloc_chrdev_region(&MFBDevNo, 0, 1, MFB_DEV_NAME);
	if (Ret < 0) {
		log_err("alloc_chrdev_region failed, %d", Ret);
		return Ret;
	}
	/* Allocate driver */
	pMFBCharDrv = cdev_alloc();
	if (pMFBCharDrv == NULL) {
		log_err("cdev_alloc failed");
		Ret = -ENOMEM;
		goto EXIT;
	}
	/* Attatch file operation. */
	cdev_init(pMFBCharDrv, &MFBFileOper);
	/*  */
	pMFBCharDrv->owner = THIS_MODULE;
	/* Add to system */
	Ret = cdev_add(pMFBCharDrv, MFBDevNo, 1);
	if (Ret < 0) {
		log_err("Attatch file operation failed, %d", Ret);
		goto EXIT;
	}
	/*  */
EXIT:
	if (Ret < 0)
		MFB_UnregCharDev();

	/*  */

	log_dbg("- X.\n");
	return Ret;
}

/**************************************************************
 *
 **************************************************************/
static signed int MFB_probe(struct platform_device *pDev)
{
	signed int Ret = 0;
	/*struct resource *pRes = NULL;*/
	signed int i = 0;
	unsigned char n;
	unsigned int irq_info[3];
	struct device *dev = NULL;
	struct MFB_device *_mfb_dev;


#ifdef CONFIG_OF
	struct MFB_device *MFB_dev;
#endif

	log_inf("- E. MFB driver probe.\n");

	/* Check platform_device parameters */
#ifdef CONFIG_OF

	if (pDev == NULL) {
		dev_dbg(&pDev->dev, "pDev is NULL");
		return -ENXIO;
	}

	nr_MFB_devs += 1;
	_mfb_dev = krealloc(MFB_devs,
		sizeof(struct MFB_device) * nr_MFB_devs,
		GFP_KERNEL);
	if (!_mfb_dev) {
		dev_dbg(&pDev->dev, "Unable to allocate MFB_devs\n");
		return -ENOMEM;
	}
	MFB_devs = _mfb_dev;

	MFB_dev = &(MFB_devs[nr_MFB_devs - 1]);
	MFB_dev->dev = &pDev->dev;

	/* iomap registers */
	MFB_dev->regs = of_iomap(pDev->dev.of_node, 0);
	/* gISPSYS_Reg[nr_MFB_devs - 1] = MFB_dev->regs; */

	if (!MFB_dev->regs) {
		dev_dbg(&pDev->dev,
	"Can't remap registers,of_iomap fail,nr_MFB_devs=%d,devnode(%s).\n",
		nr_MFB_devs,
		pDev->dev.of_node->name);
		return -ENOMEM;
	}

	log_inf("nr_MFB_devs=%d, devnode(%s), map_addr=0x%lx\n", nr_MFB_devs,
		pDev->dev.of_node->name, (unsigned long)MFB_dev->regs);

	/* get IRQ ID and request IRQ */
	MFB_dev->irq = irq_of_parse_and_map(pDev->dev.of_node, 0);

	if (MFB_dev->irq > 0) {
		/* Get IRQ Flag from device node */
		if (of_property_read_u32_array
			(pDev->dev.of_node,
			"interrupts",
			irq_info,
			ARRAY_SIZE(irq_info))) {
			dev_dbg(&pDev->dev, "get irq flags from DTS fail!!\n");
			return -ENODEV;
		}

		for (i = 0; i < MFB_IRQ_TYPE_AMOUNT; i++) {
			if (strcmp(pDev->dev.of_node->name,
				MFB_IRQ_CB_TBL[i].device_name) == 0) {
				Ret =
				request_irq(MFB_dev->irq,
				(irq_handler_t)MFB_IRQ_CB_TBL[i].isr_fp,
				irq_info[2],
				(const char *)MFB_IRQ_CB_TBL[i].device_name,
				NULL);
				if (Ret) {
					dev_dbg(&pDev->dev,
		"request_irq fail,nr_MFB_devs=%d,devnode(%s),irq=%d,ISR:%s\n",
					nr_MFB_devs,
					pDev->dev.of_node->name,
					MFB_dev->irq,
					MFB_IRQ_CB_TBL[i].device_name);
					return Ret;
				}

			log_inf("nr_MFB_devs=%d,devnode(%s),irq=%d,ISR:%s\n",
				nr_MFB_devs,
				pDev->dev.of_node->name,
				MFB_dev->irq,
				MFB_IRQ_CB_TBL[i].device_name);
				break;
			}
		}

		if (i >= MFB_IRQ_TYPE_AMOUNT) {
			log_inf("No ISR!!:nr_MFB_devs=%d,devnode(%s),irq=%d\n",
				nr_MFB_devs,
				pDev->dev.of_node->name,
				MFB_dev->irq);
		}


	} else {
		log_inf("No IRQ!!: nr_MFB_devs=%d, devnode(%s), irq=%d\n",
			nr_MFB_devs,
			pDev->dev.of_node->name, MFB_dev->irq);
	}


#endif

	/* Only register char driver in the 1st time */
	if (nr_MFB_devs == 1) {

		/* Register char driver */
		Ret = MFB_RegCharDev();
		if (Ret) {
			dev_dbg(&pDev->dev, "register char failed");
			return Ret;
		}
#ifndef EP_NO_CLKMGR
#if !defined(CONFIG_MTK_LEGACY) && defined(CONFIG_COMMON_CLK) /*CCF*/
			/*CCF: Grab clock pointer (struct clk*) */
		mfb_clk.CG_IMGSYS_MFB = devm_clk_get(
			&pDev->dev, "MFB_CLK_IMG_MFB");
		if (IS_ERR(mfb_clk.CG_IMGSYS_MFB)) {
			log_err("cannot get CG_IMGSYS_MFB clock\n");
			return PTR_ERR(mfb_clk.CG_IMGSYS_MFB);
		}
#endif
#endif

		/* Create class register */
		pMFBClass = class_create(THIS_MODULE, "MFBdrv");
		if (IS_ERR(pMFBClass)) {
			Ret = PTR_ERR(pMFBClass);
			log_err("Unable to create class, err = %d", Ret);
			goto EXIT;
		}

		dev = device_create(pMFBClass,
			NULL,
			MFBDevNo,
			NULL,
			MFB_DEV_NAME);
		if (IS_ERR(dev)) {
			Ret = PTR_ERR(dev);
			dev_dbg(&pDev->dev,
				"Failed to create device: /dev/%s, err = %d",
				MFB_DEV_NAME, Ret);
			goto EXIT;
		}

		/* Init spinlocks */
		spin_lock_init(&(MFBInfo.SpinLockMFBRef));
		spin_lock_init(&(MFBInfo.SpinLockMFB));
		for (n = 0; n < MFB_IRQ_TYPE_AMOUNT; n++)
			spin_lock_init(&(MFBInfo.SpinLockIrq[n]));

		/*  */
		init_waitqueue_head(&MFBInfo.WaitQueueHead);
		INIT_WORK(&MFBInfo.ScheduleMfbWork, MFB_ScheduleWork);

#ifdef MFB_USE_WAKELOCK
		wake_lock_init(&MFB_wake_lock,
			WAKE_LOCK_SUSPEND,
			"mfb_lock_wakelock");
#endif
		for (i = 0; i < MFB_IRQ_TYPE_AMOUNT; i++)
			tasklet_init(MFB_tasklet[i].pMFB_tkt,
				MFB_tasklet[i].tkt_cb,
				0);

		/* Init MFBInfo */
		spin_lock(&(MFBInfo.SpinLockMFBRef));
		MFBInfo.UserCount = 0;
		spin_unlock(&(MFBInfo.SpinLockMFBRef));
		/*  */
		MFBInfo.IrqInfo.Mask[MFB_IRQ_TYPE_INT_MFB_ST] = INT_ST_MASK_MFB;

	}

EXIT:
	if (Ret < 0)
		MFB_UnregCharDev();


	log_inf("- X. MFB driver probe.");

	return Ret;
}

/**************************************************************
 * Called when the device is being detached from the driver
 **************************************************************/
static signed int MFB_remove(struct platform_device *pDev)
{
	/*struct resource *pRes;*/
	signed int IrqNum;
	int i;
	/*  */
	log_dbg("- E.\n");
	/* unregister char driver. */
	MFB_UnregCharDev();

	/* Release IRQ */
	disable_irq(MFBInfo.IrqNum);
	IrqNum = platform_get_irq(pDev, 0);
	free_irq(IrqNum, NULL);

	/* kill tasklet */
	for (i = 0; i < MFB_IRQ_TYPE_AMOUNT; i++)
		tasklet_kill(MFB_tasklet[i].pMFB_tkt);
	/*  */
	device_destroy(pMFBClass, MFBDevNo);
	/*  */
	class_destroy(pMFBClass);
	pMFBClass = NULL;
	/*  */
	return 0;
}

/**************************************************************
 *
 **************************************************************/
static signed int bMFB_Suspend;

static signed int MFB_suspend(
	struct platform_device *pDev,
	pm_message_t Mesg)
{
	/*log_dbg("bMFB_Suspend(%d)\n", bMFB_Suspend);*/

	bMFB_Suspend = 1;

	if (g_u4EnableClockCount > 0) {
		MFB_EnableClock(MFALSE);
		g_u4MfbCnt++;
	}
	log_inf("MFB suspend g_u4EnableClockCount: %d, g_u4MfbCnt: %d",
		g_u4EnableClockCount, g_u4MfbCnt);
	return 0;
}

/**************************************************************
 *
 **************************************************************/
static signed int MFB_resume(struct platform_device *pDev)
{
	/*log_dbg("bMFB_Suspend(%d).\n", bMFB_Suspend);*/

	bMFB_Suspend = 0;

	if (g_u4MfbCnt > 0) {
		MFB_EnableClock(MTRUE);
		g_u4MfbCnt--;
	}
	log_inf("MFB resume g_u4EnableClockCount: %d, g_u4MfbCnt: %d",
		g_u4EnableClockCount, g_u4MfbCnt);
	return 0;
}

/*------------------------------------------------------------*/
#ifdef CONFIG_PM
/*------------------------------------------------------------*/
int MFB_pm_suspend(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	/* pr_debug("calling %s()\n", __func__); */

	return MFB_suspend(pdev, PMSG_SUSPEND);
}

int MFB_pm_resume(struct device *device)
{
	struct platform_device *pdev = to_platform_device(device);

	WARN_ON(pdev == NULL);

	/* pr_debug("calling %s()\n", __func__); */

	return MFB_resume(pdev);
}
#ifndef CONFIG_OF
/*extern void mt_irq_set_sens(unsigned int irq, unsigned int sens);*/
/*extern void mt_irq_set_polarity(unsigned int irq, unsigned int polarity);*/
#endif
int MFB_pm_restore_noirq(struct device *device)
{
	pr_debug("calling %s()\n", __func__);
#ifndef CONFIG_OF
/*	mt_irq_set_sens(MFB_IRQ_BIT_ID, MT_LEVEL_SENSITIVE);*/
/*	mt_irq_set_polarity(MFB_IRQ_BIT_ID, MT_POLARITY_LOW);*/
#endif
	return 0;

}

/*------------------------------------------------------------*/
#else				/*CONFIG_PM */
/*------------------------------------------------------------*/
#define MFB_pm_suspend NULL
#define MFB_pm_resume  NULL
#define MFB_pm_restore_noirq NULL
/*------------------------------------------------------------*/
#endif				/*CONFIG_PM */
/*------------------------------------------------------------*/
#ifdef CONFIG_OF
static const struct of_device_id MFB_of_ids[] = {
/*	{.compatible = "mediatek,imgsyscq",},*/
	{.compatible = "mediatek,mfb",},
	{}
};
#endif

const struct dev_pm_ops MFB_pm_ops = {
	.suspend = MFB_pm_suspend,
	.resume = MFB_pm_resume,
	.freeze = MFB_pm_suspend,
	.thaw = MFB_pm_resume,
	.poweroff = MFB_pm_suspend,
	.restore = MFB_pm_resume,
	.restore_noirq = MFB_pm_restore_noirq,
};


/**************************************************************
 *
 **************************************************************/
static struct platform_driver MFBDriver = {
	.probe = MFB_probe,
	.remove = MFB_remove,
	.suspend = MFB_suspend,
	.resume = MFB_resume,
	.driver = {
		   .name = MFB_DEV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = MFB_of_ids,
#endif
#ifdef CONFIG_PM
		   .pm = &MFB_pm_ops,
#endif
	}
};

/**************************************************************
 *
 ***************************************************************/

int32_t MFB_ClockOnCallback(uint64_t engineFlag)
{
	/* log_dbg("MFB_ClockOnCallback"); */
	/* log_dbg("+CmdqEn:%d", g_u4EnableClockCount); */
	/* MFB_EnableClock(MTRUE); */

	return 0;
}

int32_t MFB_DumpCallback(uint64_t engineFlag, int level)
{
	log_dbg("MFB DumpCallback\n");

	MFB_DumpReg();

	return 0;
}

int32_t MFB_ResetCallback(uint64_t engineFlag)
{
	log_dbg("MFB ResetCallback\n");
	MFB_Reset();

	return 0;
}

int32_t MFB_ClockOffCallback(uint64_t engineFlag)
{
	/* log_dbg("MFB_ClockOffCallback"); */
	/* MFB_EnableClock(MFALSE); */
	/* log_dbg("-CmdqEn:%d", g_u4EnableClockCount); */
	return 0;
}

static signed int __init MFB_Init(void)
{
	signed int Ret = 0, j;
	void *tmp;
	/* FIX-ME: linux-3.10 procfs API changed */
	/* use proc_create */
	int i;
	/*  */
	log_dbg("- E.\n");
	/*  */
	Ret = platform_driver_register(&MFBDriver);
	if (Ret < 0) {
		log_err("platform_driver_register fail");
		return Ret;
	}

	/* isr log */
	if (PAGE_SIZE <
		((MFB_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
		((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) *
		LOG_PPNUM)) {
		i = 0;
		while (i < ((MFB_IRQ_TYPE_AMOUNT * NORMAL_STR_LEN *
			((DBG_PAGE + INF_PAGE + ERR_PAGE) + 1)) * LOG_PPNUM)) {
			i += PAGE_SIZE;
		}
	} else {
		i = PAGE_SIZE;
	}
	pLog_kmalloc = kmalloc(i, GFP_KERNEL);
	if (pLog_kmalloc == NULL) {
		log_err
			("log mem not enough\n");
		return -ENOMEM;
	}
	memset(pLog_kmalloc, 0x00, i);
	tmp = pLog_kmalloc;
	for (i = 0; i < LOG_PPNUM; i++) {
		for (j = 0; j < MFB_IRQ_TYPE_AMOUNT; j++) {
			gSvLog[j]._str[i][_LOG_DBG] = (char *)tmp;
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * DBG_PAGE));
			gSvLog[j]._str[i][_LOG_INF] = (char *)tmp;
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * INF_PAGE));
			gSvLog[j]._str[i][_LOG_ERR] = (char *)tmp;
			tmp = (void *)((char *)tmp +
				(NORMAL_STR_LEN * ERR_PAGE));
		}
		/* log buffer ,in case of overflow */
		tmp = (void *)((char *)tmp + NORMAL_STR_LEN);
	}

	/* Cmdq */
	/* Register MFB callback */
	log_dbg("register mfb callback for CMDQ\n");
	cmdqCoreRegisterCB(CMDQ_GROUP_MFB,
		MFB_ClockOnCallback,
		MFB_DumpCallback, MFB_ResetCallback, MFB_ClockOffCallback);

#ifdef MFB_PMQOS
	MFBQOS_Init();
#endif
	log_dbg("- X. Ret: %d.\n", Ret);
	return Ret;
}

/**************************************************************
 *
 ***************************************************************/
static void __exit MFB_Exit(void)
{
	/*int i;*/

	log_dbg("- E.\n");

#ifdef MFB_PMQOS
	MFBQOS_Uninit();
#endif
	/*  */
	platform_driver_unregister(&MFBDriver);
	/*  */
	/* Cmdq */
	/* Unregister MFB callback */
	cmdqCoreRegisterCB(CMDQ_GROUP_MFB, NULL, NULL, NULL, NULL);

	kfree(pLog_kmalloc);

	/*  */
}


/**************************************************************
 *
 ***************************************************************/
void MFB_ScheduleWork(struct work_struct *data)
{
	if (MFB_DBG_DBGLOG & MFBInfo.DebugMask)
		log_dbg("- E.\n");

#ifdef MFB_USE_GCE
#else
	ConfigMFB();
#endif
}


static irqreturn_t ISP_Irq_MFB(signed int Irq, void *DeviceId)
{
	unsigned int MfbStatus;
	bool bResulst = MFALSE;
	pid_t ProcessID;

	MfbStatus = MFB_RD32(MFB_MFB_INT_STATUS_REG);	/* MFB Status */
	MFB_WR32(MFB_MFB_INT_STATUS_REG, 0x00000001);	/* IRQ Write 1 Clear */
	MFB_WR32(MFB_MFB_INT_CTL_REG, 0x00000000);	/* IRQ Enable Clear */

	/* MFB_DumpReg(); */

	spin_lock(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]));

	if (MFB_INT_ST == (MFB_INT_ST & MfbStatus)) {
		/* Update the frame status. */
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_begin("mfb_irq");
#endif

#ifndef MFB_USE_GCE
		MFB_WR32(MFB_START_REG, 0);
#endif
		bResulst = UpdateMFB(&ProcessID);
		/* ConfigMFB(); */
		if (bResulst == MTRUE) {
			schedule_work(&MFBInfo.ScheduleMfbWork);
#ifdef MFB_USE_GCE
			MFBInfo.IrqInfo.Status[MFB_IRQ_TYPE_INT_MFB_ST] |=
				MFB_INT_ST;
			MFBInfo.IrqInfo.ProcessID[MFB_PROCESS_ID_MFB] =
				ProcessID;
			MFBInfo.IrqInfo.MfbIrqCnt++;
			MFBInfo.ProcessID[MFBInfo.WriteReqIdx] = ProcessID;
			MFBInfo.WriteReqIdx =
				(MFBInfo.WriteReqIdx + 1) %
				_SUPPORT_MAX_MFB_FRAME_REQUEST_;
#ifdef MFB_MULTIPROCESS_TIMING_ISSUE
			/* check the write value is equal to read value ? */
			/* actually, it doesn't happen!! */
			if (MFBInfo.WriteReqIdx == MFBInfo.ReadReqIdx) {
				IRQ_LOG_KEEPER(MFB_IRQ_TYPE_INT_MFB_ST,
				m_CurrentPPB,
				_LOG_ERR,
				"ISP Irq MFB Err!!, WriteReqIdx:0x%x, ReadReqIdx:0x%x\n",
				MFBInfo.WriteReqIdx,
				MFBInfo.ReadReqIdx);
			}
#endif

#else
			MFBInfo.IrqInfo.Status[MFB_IRQ_TYPE_INT_MFB_ST] |=
				MFB_INT_ST;
			MFBInfo.IrqInfo.ProcessID[MFB_PROCESS_ID_MFB] =
				ProcessID;
#endif
		}
#ifdef __MFB_KERNEL_PERFORMANCE_MEASURE__
		mt_kernel_trace_end();
#endif
		/* Config the Next frame */
	}
	spin_unlock(&(MFBInfo.SpinLockIrq[MFB_IRQ_TYPE_INT_MFB_ST]));
	if (bResulst == MTRUE)
		wake_up_interruptible(&MFBInfo.WaitQueueHead);

	/* dump log, use tasklet */
	IRQ_LOG_KEEPER(MFB_IRQ_TYPE_INT_MFB_ST, m_CurrentPPB, _LOG_INF,
	"MFBIrq:%d,0x%x:0x%x,Result:%d,HWSta:0x%x,Cnt:0x%x,WRIdx:%x,RDIdx:%x\n",
	Irq, MFB_MFB_INT_STATUS_HW, MfbStatus, bResulst, MfbStatus,
	MFBInfo.IrqInfo.MfbIrqCnt,
	MFBInfo.WriteReqIdx, MFBInfo.ReadReqIdx);

	if (MfbStatus & MFB_INT_ST)
		tasklet_schedule(MFB_tasklet[MFB_IRQ_TYPE_INT_MFB_ST].pMFB_tkt);

	return IRQ_HANDLED;
}

static void ISP_TaskletFunc_MFB(unsigned long data)
{
	IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MFB_ST, m_CurrentPPB, _LOG_DBG);
	IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MFB_ST, m_CurrentPPB, _LOG_INF);
	IRQ_LOG_PRINTER(MFB_IRQ_TYPE_INT_MFB_ST, m_CurrentPPB, _LOG_ERR);

}


/**************************************************************
 *
 ***************************************************************/
module_init(MFB_Init);
module_exit(MFB_Exit);
MODULE_DESCRIPTION("Camera MFB driver");
MODULE_AUTHOR("MM3SW5");
MODULE_LICENSE("GPL");
